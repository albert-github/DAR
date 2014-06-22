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

    /// \file fichier_global.hpp
    /// \brief class fichier_global definition. This class is a pure virtual class
    /// class fichier_global is an abstraction of files objects whatever is their localisation
    /// like local filesystem, remote ftp server, etc. inherited classes (like fichier_local)
    /// provide full implementation

    /// \ingroup Private

#ifndef FICHIER_GLOBAL_HPP
#define FICHIER_GLOBAL_HPP


#include "../my_config.h"

extern "C"
{
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
} // end extern "C"

#include "integers.hpp"
#include "thread_cancellation.hpp"
#include "label.hpp"
#include "crc.hpp"
#include "user_interaction.hpp"
#include "mem_ui.hpp"

#include <string>

namespace libdar
{

	/// \addtogroup Private
	/// @{

    class fichier_global : public generic_file, public thread_cancellation
    {
    public :
	enum advise
	{
	    advise_normal,
	    advise_sequential,
	    advise_random,
	    advise_noreuse,
	    advise_willneed,
	    advise_dontneed
	};

	    // constructors
        fichier_global(const user_interaction & dialog, gf_mode mode);
	fichier_global(const fichier_global & ref) : generic_file(ref) { copy_from(ref); };

	    // assignment operator
	const fichier_global & operator = (const fichier_global & ref) { detruit(); copy_parent_from(ref); copy_from(ref); return *this; };

	    // destructor
	~fichier_global() { detruit(); };

	    /// set the ownership of the file
	virtual void change_ownership(const std::string & user, const std::string & group) = 0;

	    /// change the permission of the file
	virtual void change_permission(U_I perm) = 0;

	    /// return the size of the file
        virtual infinint get_size() const = 0;

	    /// set posix_fadvise for the whole file
	virtual void fadvise(advise adv) const = 0;

    protected :
	user_interaction & get_ui() { if(x_dialog == NULL) throw SRC_BUG; return *x_dialog; };

	    /// replaces generic_file::inherited_write() method, to allow the return of partial writings
	    ///
	    /// a parital writing is allowed when no space is available for writing
	    /// this let global_ficher interact with the user asking whether it can make place
	    /// or if (s)he wants to abord
	    /// \param[in] a points to the start of the area of data to write
	    /// \param[in] size is the size in byte of the data to write
	    /// \return the amount of byte wrote. If the returned value is less than size, this
	    /// is a partial write, and is assumed that free storage space is missing to complete
	    /// the operation
	virtual U_I fichier_global_inherited_write(const char *a, U_I size) = 0;


	    /// replaces generic_file::inherited_read() method, to allow the return of partial reading
	    ///
	    /// a partial reading is signaled by the inherited class by returning false
	    /// \param[in] a points to the area where to store read data
	    /// \param[in] size is the available place to store data
	    /// \param[out] read is the total amount of data read so far
	    /// \param[out] message is the request to send to the user upon partial reading
	    /// \return true if the reading is full (either read the maximum allowed data or reached end of file)
	    /// false is returned if a user interaction can let the reading go further the message to display to the
	    /// user asking him for action. He will also be proposed to abort the current operation.
	virtual bool fichier_global_inherited_read(char *a, U_I size, U_I & read, std::string & message) = 0;

    private:
	user_interaction *x_dialog;

	    // inherited from generic_file class and relocated as private methods
	void inherited_write(const char *a, U_I size);
	U_I inherited_read(char *a, U_I size);
	    //

	void copy_from(const fichier_global & ref);
	void copy_parent_from(const fichier_global & ref);
	void detruit() { if(x_dialog != NULL) { delete x_dialog; x_dialog = NULL; } };
    };


	/// @}

} // end of namespace

#endif