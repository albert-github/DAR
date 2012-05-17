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
// $Id: macro_tools.cpp,v 1.88 2012/04/27 11:24:30 edrusb Exp $
//
/*********************************************************************/

#include "../my_config.h"

#define LIBDAR_URL_VERSION "http://dar.linux.free.fr/pre-release/doc/Notes.html#Dar_version_naming"

extern "C"
{
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
} // end extern "C"

#include "macro_tools.hpp"
#include "terminateur.hpp"
#include "user_interaction.hpp"
#include "zapette.hpp"
#include "sar.hpp"
#include "elastic.hpp"
#include "tronc.hpp"
#include "trontextual.hpp"
#include "thread_cancellation.hpp"
#include "deci.hpp"
#include "escape_catalogue.hpp"
#include "tronc.hpp"
#include "cache.hpp"

using namespace std;

namespace libdar
{

    const string LIBDAR_STACK_LABEL_UNCOMPRESSED = "UNCOMPRESSED";
    const string LIBDAR_STACK_LABEL_CLEAR =  "CLEAR";
    const string LIBDAR_STACK_LABEL_UNCYPHERED = "UNCYPHERED";
    const string LIBDAR_STACK_LABEL_LEVEL1 = "LEVEL1";

    const archive_version macro_tools_supported_version = archive_version(8,0);

	// this is the archive version format generated by the application
	// this is also the highest version of format that can be read

    static void version_check(user_interaction & dialog, const header_version & ver);
    static void read_header_version(user_interaction & dialog, bool lax, generic_file & where, header_version & ver);

    catalogue *macro_tools_get_catalogue_from(user_interaction & dialog,
					      pile & stack,
					      const header_version & ver,
					      bool info_details,
					      infinint &cat_size,
					      const infinint & second_terminateur_offset,
					      bool lax_mode)
    {
	return macro_tools_get_derivated_catalogue_from(dialog,
							stack,
							stack,
							ver,
							info_details,
							cat_size,
							second_terminateur_offset,
							lax_mode);
    }

    catalogue *macro_tools_get_derivated_catalogue_from(user_interaction & dialog,
							pile & data_stack,
							pile & cata_stack,
							const header_version & ver,
							bool info_details,
							infinint &cat_size,
							const infinint & second_terminateur_offset,
							bool lax_mode)
    {
        terminateur term;
        catalogue *ret = NULL;
	generic_file *ea_loc = data_stack.get_by_label(LIBDAR_STACK_LABEL_UNCOMPRESSED);
	generic_file *data_loc = data_stack.get_by_label(LIBDAR_STACK_LABEL_CLEAR);
	generic_file *crypto = cata_stack.get_by_label(LIBDAR_STACK_LABEL_UNCYPHERED);
	contextual *data_ctxt = NULL;
	contextual *cata_ctxt = NULL;

	data_stack.find_first_from_top(data_ctxt);
	if(data_ctxt == NULL)
	    throw SRC_BUG;

	cata_stack.find_first_from_top(cata_ctxt);
	if(cata_ctxt == NULL)
	    throw SRC_BUG;

        if(info_details)
            dialog.warning(gettext("Locating archive contents..."));

	if(ver.edition > 3)
	    term.read_catalogue(*crypto, (ver.flag & VERSION_FLAG_SCRAMBLED) != 0, ver.edition, 0);
	     // terminator is encrypted since format "04"
	     // elastic buffer present when encryption is used
	 else
	     term.read_catalogue(*crypto, false, ver.edition);
	    // elastic buffer did not exist before format "04"

        if(info_details)
            dialog.warning(gettext("Reading archive contents..."));

        if(cata_stack.skip(term.get_catalogue_start()))
        {
	    if(term.get_catalogue_start() > term.get_terminateur_start())
		throw SRC_BUG;
            cat_size = term.get_terminateur_start() - term.get_catalogue_start();

            try
            {
		label tmp;

		tmp.clear(); // we do not want here to change the catalogue internal data name read from archive
                ret = new catalogue(dialog, cata_stack, ver.edition, char2compression(ver.algo_zip), data_loc, ea_loc, lax_mode, tmp);
		data_ctxt->set_info_status(CONTEXT_OP);
		cata_ctxt->set_info_status(CONTEXT_OP);
            }
	    catch(Ebug & e)
	    {
		throw;
	    }
	    catch(Ethread_cancel & e)
	    {
		throw;
	    }
            catch(Egeneric & e)
            {
                throw Erange("get_catalogue_from", string(gettext("Cannot open catalogue: ")) + e.get_message());
            }
        }
        else
            throw Erange("get_catalogue_from", gettext("Missing catalogue in file."));

        if(ret == NULL)
            throw Ememory("get_catalogue_from");

        return ret;
    }


    void macro_tools_open_archive(user_interaction & dialog,
				  const entrepot &where,
                                  const string &basename,
				  const infinint & min_digits,
                                  const string &extension,
				  crypto_algo crypto,
                                  const secu_string & pass,
				  U_32 crypto_size,
				  pile & stack,
                                  header_version &ver,
                                  const string &input_pipe,
                                  const string &output_pipe,
                                  const string & execute,
				  infinint & second_terminateur_offset,
				  bool lax,
				  bool sequential_read,
				  bool info_details)
    {
	secu_string real_pass = pass;
	generic_file *tmp = NULL;
	contextual *tmp_ctxt = NULL;

	stack.clear();

	try
	{

		// ****** Building the sar/tuyau/null layer aka level 1 ******* //

	    if(basename == "-")
	    {
		if(sequential_read)
		{
		    if(input_pipe == "")
		    {
			if(info_details)
			    dialog.warning(gettext("Opening standard input to read the archive..."));
			tmp = new trivial_sar(dialog,
					      basename,
					      lax);
		    }
		    else
		    {
			if(info_details)
			    dialog.printf(gettext("Opening named pipe %S as input to read the archive..."), &input_pipe);
			tmp = new trivial_sar(dialog,
					      input_pipe,
					      lax);
		    }
		}
		else
		{
		    tuyau *in = NULL;
		    tuyau *out = NULL;

		    try
		    {
			dialog.printf(gettext("Opening a pair of pipes to read the archive, expecting dar_slave at the other ends..."));
			tools_open_pipes(dialog, input_pipe, output_pipe, in, out);
			tmp = new zapette(dialog, in, out);
			if(tmp == NULL)
			{
			    delete in;
			    in = NULL;
			    delete out;
			    out = NULL;
			}
			else
			    in = out = NULL; // now managed by the zapette
		    }
		    catch(...)
		    {
			if(in != NULL)
			    delete in;
			if(out != NULL)
			    delete out;
			throw;
		    }
		}
	    }
	    else
	    {
		if(info_details)
		    dialog.warning(gettext("Opening the archive using the multi-slice abstraction layer..."));
		tmp = new sar(dialog,
			      basename,
			      extension,
			      where,
			      !sequential_read, // not openned by the end in sequential read mode
			      min_digits,
			      lax,
			      execute);
	    }

	    if(tmp == NULL)
		throw Ememory("open_archive");
	    else
	    {
		stack.push(tmp, LIBDAR_STACK_LABEL_LEVEL1);
		tmp = NULL;
	    }

		// ****** Reading the header version ************** //

	    if(info_details)
		dialog.warning(gettext("Reading the archive header..."));

	    stack.find_first_from_top(tmp_ctxt);
	    if(tmp_ctxt == NULL)
		throw SRC_BUG;
	    second_terminateur_offset = 0;

	    if(sequential_read || tmp_ctxt->is_an_old_start_end_archive())
		stack.skip(0);
	    else
	    {
		terminateur term;

		try
		{
		    term.read_catalogue(stack, false, macro_tools_supported_version);
		    stack.skip(term.get_catalogue_start());
		    second_terminateur_offset = term.get_catalogue_start();
		}
		catch(Erange & e)
		{
		    dialog.printf(gettext("Error while reading archive's header, this may be because this archive is an old encrypted archive or that data corruption took place, Assuming it is an old archive, we have to read the header at the beginning of the first slice..."));
		    stack.skip(0);
		}
	    }


	    read_header_version(dialog, lax, stack, ver);


		// checking whether the read data was not a catalogue (old format) catalogue stard with the "droot" string.
	    if(ver.edition.is_droot() && ver.algo_zip == 'o')
	    {
		second_terminateur_offset = 0;
		dialog.warning(gettext("Could not find archive information at the end of the last slice, assuming an old archive and trying to read at the beginning of the first slice..."));
		stack.skip(0);
		read_header_version(dialog, lax, stack, ver);
	    }

	    if(second_terminateur_offset == 0 && !sequential_read && ver.edition > 7)
		throw Erange("macro_tools_open_archive", gettext("Found a correct archive header at the beginning of the archive, which does not stands to be an old archive, the end of the archive is thus corrupted. You need to use sequential reading mode to have a chance to use this corrupted archive"));


		// *************  adding a tronc to hide last terminator and trailer_version ******* //

	    if(second_terminateur_offset != 0)
	    {
		if(info_details)
		    dialog.printf(gettext("Opening construction layer..."));
		tmp = new tronc(stack.top(), 0, second_terminateur_offset, false);
		if(tmp == NULL)
		    throw Ememory("macro_tools_open_archive");
		else
		{
		    stack.clear_label(LIBDAR_STACK_LABEL_LEVEL1);
		    stack.push(tmp, LIBDAR_STACK_LABEL_LEVEL1);
		    tmp = NULL;
		}
	    }

		// *************  building the encryption layer if necessary ************** //

	    if(info_details)
		dialog.warning(gettext("Considering cyphering layer..."));

	    if(((ver.flag & VERSION_FLAG_SCRAMBLED) != 0) && crypto == crypto_none)
	    {
		if(lax)
		{
		    dialog.warning(gettext("LAX MODE: Archive seems to be ciphered, but you did not have provided any encryption algorithm, assuming data corruption and considering that the archive is not ciphered"));
		}
		else
		    throw Erange("macro_tools_open_archive", tools_printf(gettext("The archive %S is encrypted and no encryption cipher has been given, cannot open archive."), &basename));
	    }

	    if(crypto != crypto_none && pass == "")
	    {
		if(!secu_string::is_string_secured())
		    dialog.warning(gettext("WARNING: support for secure memory was not available at compilation time, in case of heavy memory load, this may lead the password you are about to provide to be wrote to disk (swap space) in clear. You have been warned!"));
		real_pass = dialog.get_secu_string(tools_printf(gettext("Archive %S requires a password: "), &basename), false);
	    }

	    switch(crypto)
	    {
	    case crypto_none:
		if(info_details)
		    dialog.warning(gettext("No cyphering layer opened"));
		break;
	    case crypto_blowfish:
	    case crypto_aes256:
	    case crypto_twofish256:
	    case crypto_serpent256:
	    case crypto_camellia256:
		if(info_details)
		    dialog.warning(gettext("Opening cyphering layer..."));
		if(second_terminateur_offset != 0) // we have openned the archive by the end
		{
		    crypto_sym *tmp_ptr = NULL;

		    tmp = tmp_ptr = new crypto_sym(crypto_size, real_pass, *(stack.top()), true, ver.edition, crypto);
		    if(tmp_ptr != NULL)
			tmp_ptr->set_initial_shift(ver.initial_offset);
		}
		else // archive openned by the beginning
		{
		    crypto_sym *tmp_ptr;
		    tmp = tmp_ptr = new crypto_sym(crypto_size, real_pass, *(stack.top()), false, ver.edition, crypto);

		    if(tmp_ptr != NULL)
		    {
			tmp_ptr->set_callback_trailing_clear_data(&macro_tools_get_terminator_start);

			if(sequential_read)
			    elastic(*tmp_ptr, elastic_forward, ver.edition); // this is line creates a temporary anonymous object and destroys it just afterward
			    // this is necessary to skip the reading of the initial elastic buffer
			    // nothing prevents the elastic buffer from carrying what could
			    // be considered an escape mark.
		    }
		}
		break;
	    case crypto_scrambling:
		if(info_details)
		    dialog.warning(gettext("Opening cyphering layer..."));
		tmp = new scrambler(real_pass, *(stack.top()));
		break;
	    default:
		throw Erange("macro_tools_open_archive", gettext("Unknown encryption algorithm"));
	    }

	    if(crypto != crypto_none)
	    {
		if(tmp == NULL)
		    throw Ememory("open_archive");
		stack.push(tmp);
		tmp = NULL;
	    }

	    stack.add_label(LIBDAR_STACK_LABEL_UNCYPHERED);

		// *************** building the escape layer if necessary ************ //

	    try
	    {
		if((ver.flag & VERSION_FLAG_SEQUENCE_MARK) != 0)
		{
		    if(info_details)
			dialog.warning(gettext("Opening escape sequence abstraction layer..."));
		    if(lax)
		    {
			if(!sequential_read)
			{
			    dialog.pause(gettext("LAX MODE: Archive is flagged as having escape sequence (which is normal in recent archive versions). However if this is not expected, shall I assume a data corruption occurred in this field and that this flag should be ignored? (If unsure, refuse)"));
			    ver.flag &= ~VERSION_FLAG_SEQUENCE_MARK;
			}
			else
			    throw Euser_abort("this exception triggers the creation of the escape layer");
			    // else in lax & sequential_read, escape sequences are mandatory, we do not propose to ignore them
		    }
		    else
			throw Euser_abort("this exception triggers the creation of the escape layer");
		}
		else // no escape layer in the archive
		{
		    if(!lax)
		    {
			if(sequential_read)
			    throw Erange("macro_tools_open_archive", gettext("Sequential read asked, but this archive is flagged to not have the necessary embedded escape sequences for that operation, aborting"));
		    }
		    else // lax mode
			if(sequential_read)
			{
			    dialog.warning(gettext("LAX MODE: the requested sequential read mode relies on escape sequence which seem to be absent from this archive. Assuming data corruption occurred. However, if no data corruption occurred and thus no escape sequence are present in this archive, do not use sequential reading mode to explore this archive else you will just get nothing usable from it"));
			    ver.flag |= VERSION_FLAG_SEQUENCE_MARK;
			    throw Euser_abort("this exception triggers the creation of the escape layer");
			}
			else // normal mode
			    if(ver.edition >= 8) // most usually escape mark are present, thus we must warn
				dialog.pause(gettext("LAX MODE: Archive is flagged to not have escape sequence. If corruption occurred and an escape sequence is present, this may lead data restoration to fail, answering no at this question will let me consider that an escape sequence layer has to be added in spite of the archive flags. Do you want to continue as suggested by the archive flag, thus without escape sequence layer?"));
		}
	    }
	    catch(Euser_abort & e)
	    {
		set<escape::sequence_type> unjump;

		unjump.insert(escape::seqt_catalogue);
		tmp = new escape(stack.top(), unjump);
		if(tmp == NULL)
		    throw Ememory("open_archive");
		stack.push(tmp);
		tmp = NULL;
	    }

		// *********** if not crypto nor escape layer adding cache layer for better performances

	    if(crypto == crypto_none && (ver.flag & VERSION_FLAG_SEQUENCE_MARK) == 0)
	    {
		if(info_details)
		    dialog.warning(gettext("Opening the cache layer for better performance..."));

		tmp = new cache(*(stack.top()), false);
		if(tmp == NULL)
		    dialog.warning(gettext("Failed opening the cache layer, lack of memory, archive read performances will not be optimized"));
		else
		{
		    stack.push(tmp);
		    tmp = NULL;
		}
	    }

	    stack.add_label(LIBDAR_STACK_LABEL_CLEAR);

		// *********** building the compression layer ************* //

	    if(info_details)
	    {
		if(ver.algo_zip == none)
		    dialog.warning(gettext("Opening the compression abstraction layer (compression algorithm used is none)..."));
		else
		    dialog.warning(gettext("Opening the compression layer..."));
	    }

	    version_check(dialog, ver);
	    if(lax)
	    {
		bool ok = false;
		do
		{
		    try
		    {
			char2compression(ver.algo_zip);
			ok = true;
		    }
		    catch(Erange & e)
		    {
			string answ = dialog.get_string(gettext("LAX MODE: Unknown compression algorithm used, assuming data corruption occurred. Please help me, answering with one of the following words \"none\", \"gzip\", \"bzip2\" or \"lzo\" at the next prompt:"), true);
			if(answ == gettext("none"))
			    ver.algo_zip = compression2char(none);
			else if(answ == gettext("gzip"))
			    ver.algo_zip = compression2char(gzip);
			else if(answ == gettext("bzip2"))
			    ver.algo_zip = compression2char(bzip2);
			else if(answ == gettext("lzo"))
			    ver.algo_zip = compression2char(lzo);
		    }
		}
		while(!ok);
	    }
	    tmp = new compressor(char2compression(ver.algo_zip), *(stack.top()));

	    if(tmp == NULL)
		throw Ememory("open_archive");
	    else
	    {
		stack.push(tmp, LIBDAR_STACK_LABEL_UNCOMPRESSED);
		tmp = NULL;
	    }

		// ************* warning info ************************ //

	    if(info_details)
		dialog.warning(gettext("All layers have been created successfully"));

	    if((ver.flag & VERSION_FLAG_SCRAMBLED) != 0)
		dialog.printf(gettext("Warning, the archive %S has been encrypted. A wrong key is not possible to detect, it would cause DAR to report the archive as corrupted\n"),  &basename);
	}
	catch(...)
	{
	    if(tmp != NULL)
		delete tmp;
	    stack.clear();
	    throw;
	}
    }

    catalogue *macro_tools_lax_search_catalogue(user_interaction & dialog,
						pile & stack,
						const archive_version & edition,
						compression compr_algo,
						bool info_details,
						bool even_partial_catalogue,
						const label & layer1_data_name)
    {
	catalogue *ret = NULL;
	thread_cancellation thr_cancel;
	bool ok = false;
	U_I check_abort_every = 10000;
	U_I check_abort_count = check_abort_every;
	infinint offset;
	infinint max_offset;
	infinint min_offset;
	infinint amplitude;
	entree_stats stats;
	infinint fraction = 101;
	escape *esc = NULL;
	compressor *zip = NULL;
	generic_file *ea_loc = stack.get_by_label(LIBDAR_STACK_LABEL_UNCOMPRESSED);
	generic_file *data_loc = stack.get_by_label(LIBDAR_STACK_LABEL_CLEAR);


	    // obtaining from the user the fraction of the archive to inspect

	do
	{
	    string answ = dialog.get_string(gettext("LAX MODE: The catalogue (table of contents) usually takes a few percents of the archive at its end, which percentage do you want me to scan (answer by an *integer* number between 0 and 100)? "), true);
	    try
	    {
		deci num = answ;
		fraction = num.computer();
		if(fraction > 100)
		    dialog.printf(gettext("LAX MODE: %i is not a valid percent value"), &fraction);
	    }
	    catch(Edeci & e)
	    {
		dialog.printf(gettext("%S is not a valid number"), & answ);
	    }
	}
	while(fraction > 100 || fraction == 0);

	if(info_details)
	    dialog.printf(gettext("LAX MODE: Beginning search of the catalogue (from the end toward the beginning of the archive, on %i %% of its length), this may take a while..."), &fraction);

	stack.find_first_from_top(zip);
	if(zip == NULL)
	    throw SRC_BUG;


	    // finding the upper limit of the search

	if(zip->skip_to_eof())
	    max_offset = zip->get_position();
	else
	{
	    dialog.warning(gettext("LAX MODE: Cannot skip at the end of the archive! Using current position to start the catalogue search"));
	    max_offset = zip->get_position();
	}

	if(max_offset == 0)
	    throw Erange("macro_tools_lax_search_catalogue", gettext("LAX MODE: Failed to read the catalogue (no data to inspect)"));

	if(fraction == 0)
    	    throw Erange("macro_tools_lax_search_catalogue", gettext("LAX MODE: Failed to read the catalogue (0 bytes of the archive length asked to look for the catalogue)"));


	    // calculating starting offset and offset range

	min_offset = ((infinint)100 - fraction)*max_offset/100;
	amplitude = max_offset - min_offset;


	stack.find_first_from_top(esc);
	if(esc != NULL)
	{
	    dialog.warning(gettext("LAX MODE: Escape sequence seems present in this archive. I have thus two different methods, either I look for the escape sequence indicating the start of the catalogue or I try each position in turn in the hope it will not be data that look like a catalogue"));
	    try
	    {
		dialog.pause(gettext("LAX MODE: Trying to locate the escape sequence (safer choice) ?"));
		if(!esc->skip(min_offset))
		    throw SRC_BUG;
		if(esc->skip_to_next_mark(escape::seqt_catalogue, true))
		{
		    dialog.warning(gettext("LAX MODE: Good point! I could find the escape sequence marking the beginning of the catalogue, now trying to read it..."));
		    zip->flush_read();
		    if(zip->get_position() != esc->get_position())
			throw SRC_BUG;
		    offset = zip->get_position();
		    min_offset = offset; // no need to read before this position, we cannot fetch the catalogue there
		}
		else
		{
		    dialog.warning(gettext("LAX MODE: Escape sequence could not be found, it may have been corrupted or out of the scanned portion of the archive, trying to find the catalogue the other way"));
		    throw Euser_abort("THIS MESSAGE SHOULD NEVER APPEAR, It branches the execution to the second way of looking for the catalogue");
		}
	    }
	    catch(Euser_abort & e)
	    {
		offset = min_offset;
		zip->skip(offset);
	    }
	}

	while(!ok)
	{

		// checking whether thread cancellation has been asked

	    if(++check_abort_count > check_abort_every)
	    {
		thr_cancel.check_self_cancellation();
		check_abort_count = 0;
		if(info_details)
		{
		    infinint ratio = (offset - min_offset)*100/amplitude;
		    dialog.warning(tools_printf(gettext("LAX MODE: %i %% remaining"), & ratio));
		}
	    }

		// trying to read a catalogue at the "offset" position

	    try
	    {
		ret = new catalogue(dialog, *zip, edition, compr_algo, data_loc, ea_loc, even_partial_catalogue, layer1_data_name);
		if(ret == NULL)
		    throw Ememory("macro_tools_lax_search_catalogue");
		stats = ret->get_stats();
		dialog.printf(gettext("Could read a catalogue data structure at offset %i, it contains the following:"), &offset);
		stats.listing(dialog);
		dialog.pause(gettext("Do you want to use it for restoration?"));
		ok = true;
	    }
	    catch(Ebug & e)
	    {
		throw;
	    }
	    catch(Ethread_cancel & e)
	    {
		throw;
	    }
	    catch(Efeature & e)
	    {
		throw;
	    }
	    catch(Ehardware & e)
	    {
		throw;
	    }
	    catch(Escript & e)
	    {
		throw;
	    }
	    catch(Ecompilation & e)
	    {
		throw;
	    }
	    catch(Egeneric & e)
	    {
		if(offset >= max_offset)
		{
		    if(info_details)
			dialog.warning(gettext("LAX MODE: Reached the end of the area to scan, FAILED to find any catalogue"));
		    ret = NULL;
		    ok = true;
		}
		else
		    zip->skip(++offset);
	    }
	}

	if(ret == NULL)
	    throw Erange("macro_tools_lax_search_catalogue", gettext("LAX MODE: Failed to read the catalogue"));
	else
	    return ret;
    }


    static void dummy_call(char *x)
    {
        static char id[]="$Id: macro_tools.cpp,v 1.88 2012/04/27 11:24:30 edrusb Exp $";
        dummy_call(id);
    }

    static void version_check(user_interaction & dialog, const header_version & ver)
    {
        if(ver.edition > macro_tools_supported_version)
            dialog.pause(gettext("The format version of the archive is too high for that software version, try reading anyway?"));
    }

    static void read_header_version(user_interaction & dialog, bool lax, generic_file & where, header_version & ver)
    {
	try
	{
	    ver.read(where);
	}
	catch(Ebug & e)
	{
	    throw;
	}
	catch(Ethread_cancel & e)
	{
	    throw;
	}
	catch(Egeneric & e)
	{
	    if(!lax)
		throw;
	    else
	    {
		dialog.warning(gettext("LAX MODE: Failed to read the archive header, I will need your help to know what is the missing information."));

		if(ver.edition > macro_tools_supported_version)
		{
 		    dialog.printf(gettext("LAX MODE: Archive format revision found is [%s] but the higher version this binary can handle is [%s]. Thus, assuming the archive version is corrupted and falling back to the higher version this binary can support (%s)"), ver.edition.display().c_str(), macro_tools_supported_version.display().c_str(),  macro_tools_supported_version.display().c_str());
		    ver.edition = macro_tools_supported_version;
		}
		else
		    dialog.printf(gettext("LAX MODE: Archive format revision found is [version %s]"), ver.edition.display().c_str());

		try
		{
		    dialog.pause(tools_printf(gettext("LAX MODE: is it correct, seen the table at the following URL: %s ?"), LIBDAR_URL_VERSION));
		}
		catch(Euser_abort & e)
		{
		    string answ;
		    U_I equivalent;
		    bool ok = false;
		    do
		    {
			answ = dialog.get_string(tools_printf(gettext("LAX MODE: Please provide the archive format: You can use the table at %s to find the archive format depending on the release version, (for example if this archive has been created using dar release 2.3.4 to 2.3.7 answer \"6\" without the quotes here): "), LIBDAR_URL_VERSION), true);
			if(tools_my_atoi(answ.c_str(), equivalent))
			    ver.edition = equivalent;
			else
			{
			    dialog.pause(tools_printf(gettext("LAX MODE: \"%S\" is not a valid archive format"), &answ));
			    continue;
			}

			try
			{
			    dialog.pause(tools_printf(gettext("LAX MODE: Using archive format \"%d\"?"), equivalent));
			    ok = true;
			}
			catch(Euser_abort & e)
			{
			    ok = false;
			}
		    }
		    while(!ok);
		}

		ver.cmd_line = "";
		ver.flag = VERSION_FLAG_SCRAMBLED; // this is the lax mode way to handle both ciphered and clear archives

		bool has_escape = (ver.flag & VERSION_FLAG_SEQUENCE_MARK) != 0;

		try
		{
		    if(ver.edition < 8)
		    {
			if(has_escape)
			    ver.flag &= ~VERSION_FLAG_SEQUENCE_MARK; // Escape sequence marks appeared at revision 08
		    }
		}
		catch(Euser_abort & e)
		{
		    if(has_escape)
			ver.flag &= ~VERSION_FLAG_SEQUENCE_MARK; // setting the bit to zero
		    else
			ver.flag |= VERSION_FLAG_SEQUENCE_MARK; // setting the bit to one
		}
	    }
	}
    }

    infinint macro_tools_get_terminator_start(generic_file & f, const archive_version & reading_ver)
    {
	terminateur term;
	infinint width;

	f.skip_to_eof();
	width = f.get_position();

	term.read_catalogue(f, false, reading_ver);
	if(term.get_catalogue_start() < width)
	    width = term.get_catalogue_start();

	return width;
    }

} // end of namespace
