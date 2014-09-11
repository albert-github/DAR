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
} // end extern "C"

#include "cat_lien.hpp"

using namespace std;

namespace libdar
{

    cat_lien::cat_lien(const infinint & uid, const infinint & gid, U_16 perm,
		       const datetime & last_access,
		       const datetime & last_modif,
		       const datetime & last_change,
		       const string & name,
		       const string & target,
		       const infinint & fs_device) : cat_inode(uid, gid, perm, last_access, last_modif, last_change, name, fs_device)
    {
	points_to = target;
	set_saved_status(s_saved);
    }

    cat_lien::cat_lien(user_interaction & dialog,
		       generic_file & f,
		       const archive_version & reading_ver,
		       saved_status saved,
		       compressor *efsa_loc,
		       escape *ptr) : cat_inode(dialog, f, reading_ver, saved, efsa_loc, ptr)
    {
	if(saved == s_saved)
	    tools_read_string(f, points_to);
    }

    const string & cat_lien::get_target() const
    {
	if(get_saved_status() != s_saved)
	    throw SRC_BUG;
	return points_to;
    }

    void cat_lien::set_target(string x)
    {
	set_saved_status(s_saved);
	points_to = x;
    }

    void cat_lien::sub_compare(const cat_inode & other, bool isolated_mode) const
    {
	const cat_lien *l_other = dynamic_cast<const cat_lien *>(&other);
	if(l_other == NULL)
	    throw SRC_BUG; // bad argument cat_inode::compare has a bug

	if(get_saved_status() == s_saved && l_other->get_saved_status() == s_saved)
	    if(get_target() != l_other->get_target())
		throw Erange("cat_lien:sub_compare", string(gettext("symbolic link does not point to the same target: "))
			     + get_target() + " <--> " + l_other->get_target());
    }

    void cat_lien::inherited_dump(generic_file & f, bool small) const
    {
	cat_inode::inherited_dump(f, small);
	if(get_saved_status() == s_saved)
	    tools_write_string(f, points_to);
    }

} // end of namespace