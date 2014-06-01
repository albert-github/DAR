/*********************************************************************/
// dar - disk archive - a backup/restoration program
// Copyright (C) 2002-2052 Denis Corbin
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// to contact the author : http://dar.linux.free.fr/email.html
/*********************************************************************/

#include "../my_config.h"

extern "C"
{
#if HAVE_LIMITS_H
#include <limits.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

#if HAVE_STRINGS_H
#include <strings.h>
#endif

#if STDC_HEADERS
# include <string.h>
#else
# if !HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char *strchr (), *strrchr ();
# if !HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif

#define MAX_BUFFER_SIZE 10485760

}

#include "cache.hpp"

using namespace std;

namespace libdar
{

// This implementation of cache is a bit simple
// we do not need a read read-write caching scheme but rather a read or write caching scheme
// to make it possible to use for reading and writing (for coherence and avoid change in the use) the
// caching scheme works either in read caching or write caching alternatively depending on the read() or write()
// use.
//
// if a real read-write caching is required, this implementation can be completely reviewed without any
// change in its usage.  Currently we just need to gather several read or write on a single huge one.
//


    cache::cache(generic_file & hidden,
		 bool shift_mode,
		 U_I x_size) : generic_file(hidden.get_mode())
    {
	    // sanity checks
	if(&hidden == NULL)
	    throw Erange("cache::cache", "NULL given as \"hidden\" argument while initializing cache"); // not translated message, this is expected
	if(x_size < 10)
	    throw Erange("cache::cache", gettext("wrong value given as initial_size argument while initializing cache"));

	ref = & hidden;
	buffer = NULL;
	alloc_buffer(x_size);
	next = 0;
	last = 0;
	need_flush_write = false;
	buffer_offset = ref->get_position();
	shifted_mode = shift_mode;
    }

    cache::~cache()
    {
	try
	{
	    flush_write();
	}
	catch(...)
	{
		// ignore all exceptions
	}
    }

    bool cache::skippable(skippability direction, const infinint & amount)
    {
	infinint in_cache;

	    // calculating the availabe data in cache

	in_cache = available_in_cache(direction);

	    // either available data is enough to assure skippability or we
	    // calculate the direction and amount to ask to the lower layer (ref)

	if(in_cache >= amount)
	    return true;
	else
	{
	    switch(direction)
	    {
	    case skip_forward:
		if(ref->get_position() <= buffer_offset)
		    return ref->skippable(direction, buffer_offset - ref->get_position() + next + amount);
		else
		{
		    infinint backw = ref->get_position() - buffer_offset;
		    infinint forw = amount + next;
		    if(backw >= forw)
			return ref->skippable(skip_backward, backw - forw);
		    else
			return ref->skippable(skip_forward, forw - backw);
		}
	    case skip_backward:
		if(ref->get_position() >= buffer_offset)
		{
		    infinint backw = ref->get_position() - buffer_offset  + amount;
		    infinint forw = next;
		    if(backw >= forw)
			return ref->skippable(skip_backward, backw - forw);
		    else
			return ref->skippable(skip_forward, forw - backw);
		}
		else
		{
		    infinint backw = amount;
		    infinint forw = buffer_offset - ref->get_position()  + next;
		    if(backw >= forw)
			return ref->skippable(skip_backward, backw - forw);
		    else
			return ref->skippable(skip_forward, forw - backw);
		}
	    default:
		throw SRC_BUG;
	    }
	}
    }

    bool cache::skip(const infinint & pos)
    {
	if(is_terminated())
	    throw SRC_BUG;

	if(pos >= buffer_offset && pos < buffer_offset + last)
	{
		// skipping inside the buffer is possible

	    infinint tmp_next = pos - buffer_offset;

		// assigning to next its new value to reflect the skip() operation

	    next = 0;
	    tmp_next.unstack(next);
	    if(tmp_next != 0)
		throw SRC_BUG;
	    return true;
	}
	else // skipping would lead the current position to be outside the buffer
	{
	    bool ret;

	    if(need_flush_write)
		flush_write();
	    next = last = 0;
	    ret = ref->skip(pos);
	    buffer_offset = ref->get_position();

	    return ret;
	}
    }

    bool cache::skip_to_eof()
    {
	bool ret;

	if(is_terminated())
	    throw SRC_BUG;

	if(need_flush_write)
	    flush_write();
	next = last = 0;
	ret = ref->skip_to_eof();
	buffer_offset = ref->get_position();

	return ret;
    }

    bool cache::skip_relative(S_I x)
    {
	skippability dir = x >= 0 ? skip_forward : skip_backward;
	U_I in_cache = available_in_cache(dir);
	U_I abs_x = x >= 0 ? x : -x;

	if(is_terminated())
	    throw SRC_BUG;

	if(abs_x <= in_cache) // skipping within cache
	{
	    next += x; // note that x is a *signed* integer

		// sanity checks

	    if(next > last)
		throw SRC_BUG;
	    if(next < 0)
		throw SRC_BUG;
	    return true;
	}
	else // must replace data in cache to skip
	{
	    if(need_flush_write)
		flush_write();

	    switch(dir)
	    {
	    case skip_forward:
		return skip(buffer_offset + abs_x);
	    case skip_backward:
		if(buffer_offset < abs_x)
		    return false;
		else
		    return skip(buffer_offset - abs_x);
	    default:
		throw SRC_BUG;
	    }
	}
    }

    U_I cache::inherited_read(char *a, U_I x_size)
    {
	U_I ret = 0;
	bool eof = false;

	do
	{
	    if(next >= last) // no more data to read from cache
	    {
		if(need_flush_write)
		    flush_write();
		if(x_size - ret < size)
		{
		    fulfill_read();
		    if(next >= last) // could not read anymore data
			eof = true;
		}
		else // reading the remaining directly from lower layer
		{
		    ret += ref->read(a + ret, x_size - ret);
		    if(ret < x_size)
			eof = true;
		    clear_buffer();   // force clearing whatever is shifted_mode
		    buffer_offset = ref->get_position();
		}
	    }

	    if(!eof && ret < x_size)
	    {
		U_I needed = x_size - ret;
		U_I avail = last - next;
		U_I min = avail > needed ? needed : avail;

		if(min > 0)
		{
		    (void)memcpy(a+ret, buffer + next, min);
		    ret += min;
		    next += min;
		}
		else
		    throw SRC_BUG;
	    }
	}
	while(ret < x_size && !eof);

	return ret;
    }


    void cache::inherited_write(const char *a, U_I x_size)
    {
	U_I wrote = 0;
	U_I avail, remaining;

	while(wrote < x_size)
	{
	    avail = size - next;
	    if(avail == 0) // we need to flush the cache
	    {
		if(need_flush_write)
		    flush_write();
		avail = size - next;
	    }

	    remaining = x_size - wrote;
	    if(avail < remaining && !need_flush_write)
	    {
		    // less data in cache than to be wrote and no write pending data  in cache
		    // we write directly to the lower layer

		buffer_offset += next;
		next = last = 0;
		ref->skip(buffer_offset);
		ref->write(a + wrote, remaining);
		wrote = x_size;
		buffer_offset += remaining;
	    }
	    else // filling cache with data
	    {
		U_I min = remaining < avail ? remaining : avail;
		(void)memcpy(buffer + next, a + wrote, min);
		wrote += min;
		next += min;
		if(last < next)
		    last = next;
		need_flush_write = true;
	    }
	}
    }

    void cache::alloc_buffer(size_t x_size)
    {
	if(buffer != NULL)
	    throw SRC_BUG;

	if(get_pool() != NULL)
	    buffer = (char *)get_pool()->alloc(x_size);
	else
	    buffer = new (nothrow) char[x_size];

	if(buffer == NULL)
	    throw Ememory("cache::alloc_buffer");
	size = x_size;
    }

    void cache::release_buffer()
    {
	if(buffer == NULL)
	    throw SRC_BUG;

	if(get_pool() != NULL)
	    get_pool()->release(buffer);
	else
	    delete [] buffer;
	buffer = NULL;
	size = 0;
    }

    void cache::shift_by_half()
    {
	U_I half = last / 2;
	U_I reste = last % 2;

	if(next < half)
	    throw SRC_BUG; // current position would be out of the buffer
	if(last > 1)
	{
	    (void)memmove(buffer, buffer + half, half + reste);
	    next -= half;
	    last -= half;
	}
	buffer_offset += half;
    }

    void cache::clear_buffer()
    {
	if(next != last)
	    throw SRC_BUG; // current position would be out of buffer
	buffer_offset += last;
	next = last = 0;
    }

    void cache::flush_write()
    {
	if(get_mode() == gf_read_only)
	    return; // nothing to flush

	    // flushing the cache

	if(last > 0) // we have something to flush
	{
	    ref->skip(buffer_offset);
	    ref->write(buffer, last);
	}
	need_flush_write = false;

	if(shifted_mode)
	    shift_by_half();
	else
	    clear_buffer();
    }

    void cache::fulfill_read()
    {
	U_I lu;

	if(get_mode() == gf_write_only)
	    return; // nothing to fill

	    // flushing / shifting the cache contents to make room to receive new data

	if(shifted_mode)
	    shift_by_half();
	else
	    clear_buffer();

	    ///////
	    // some data may remain in the cache, we need to preserve them !!!
	    // this occurres when a shift by half of the buffer has been done just before
	    ///////

	ref->skip(buffer_offset + last);
	lu = ref->read(buffer + last, size - last);
	last += lu;
    }

    U_I cache::available_in_cache(skippability direction) const
    {
	U_I ret;

	switch(direction)
	{
	case skip_forward:
	    ret = last - next;
	    break;
	case skip_backward:
	    ret = next;
	    break;
	default:
	    throw SRC_BUG;
	}

	return ret;
    }

} // end of namespace



