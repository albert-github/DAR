/*********************************************************************/
// dar - disk archive - a backup/restoration program
// Copyright (C) 2002 Denis Corbin
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
// to contact the author : dar.linux@free.fr
/*********************************************************************/
// $Id: mask.cpp,v 1.11 2002/03/18 11:00:54 denis Rel $
//
/*********************************************************************/

#include <fnmatch.h>
#include "tools.hpp"
#include "erreurs.hpp"
#include "mask.hpp"

simple_mask::simple_mask(const string & wilde_card_expression)
{
    the_mask = tools_str2charptr(wilde_card_expression);
    if(the_mask == NULL)
	throw Ememory("simple_mask::simple_mask");
}

bool simple_mask::is_covered(const string &expression) const
{
    char *tmp = tools_str2charptr(expression);
    bool ret;

    if(tmp == NULL)
	throw Ememory("simple_mask::is_covered");

    ret = fnmatch(the_mask, tmp, FNM_NOESCAPE|FNM_PATHNAME) == 0;
    delete tmp;

    return ret;
}

void simple_mask::copy_from(const simple_mask & m)
{
    the_mask = new char[strlen(m.the_mask)+1];
    if(the_mask == NULL)
	throw Ememory("simple_mask::copy_from");
    strcpy(the_mask, m.the_mask);
}

void not_mask::copy_from(const not_mask &m)
{
    ref = m.ref->clone();
    if(ref == NULL)
	throw Ememory("not_mask::copy_from(not_mask)");
}

void not_mask::copy_from(const mask &m)
{
    ref = m.clone();
    if(ref == NULL)
	throw Ememory("not_mask::copy_from(mask)");
}

void not_mask::detruit()
{
    if(ref != NULL)
    {
	delete ref;
	ref = NULL;
    }
}

void et_mask::add_mask(const mask& toadd)
{
    mask *t = toadd.clone();
    if(t != NULL)
	lst.push_back(t);
    else
	throw Ememory("et_mask::et_mask");
}

bool et_mask::is_covered(const string & expression) const
{
    vector<mask *>::const_iterator it = lst.begin();

    if(lst.size() == 0)
	throw Erange("et_mask::is_covered", "no mask in the list of mask to AND");

    while(it != lst.end() && (*it)->is_covered(expression))
	it++;

    return it == lst.end();
}

void et_mask::copy_from(const et_mask &m)
{
    vector<mask *>::const_iterator it = m.lst.begin();
    mask *tmp;

    while(it != m.lst.end() && (tmp = (*it)->clone()) != NULL)
    {
	lst.push_back(tmp);
	it++;
    }

    if(it != m.lst.end())
    {
	detruit();
	throw Ememory("et_mask::copy_from");
    }
}

void et_mask::detruit()
{
    vector<mask *>::iterator it = lst.begin();

    while(it != lst.end())
    {
	delete *it;
	it++;
    }
    lst.clear();
}

static void dummy_call(char *x)
{
    static char id[]="$Id: mask.cpp,v 1.11 2002/03/18 11:00:54 denis Rel $";
    dummy_call(id);
}

bool ou_mask::is_covered(const string & expression) const
{
    vector<mask *>::const_iterator it = lst.begin();

    if(lst.size() == 0)
	throw Erange("et_mask::is_covered", "no mask in the list of mask to OR");

    while(it != lst.end() && ! (*it)->is_covered(expression))
	it++;

    return it != lst.end();
}

bool simple_path_mask::is_covered(const string &ch) const
{
    path p = ch;
    return p.is_subdir_of(chemin) || chemin.is_subdir_of(p);
}

