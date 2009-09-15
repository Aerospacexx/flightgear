// FGCommonInput.hxx -- common functions for all Input subsystems
//
// Written by Torsten Dreyer, started August 2009
// Based on work from David Megginson, started May 2001.
//
// Copyright (C) 2009 Torsten Dreyer, Torsten (at) t3r _dot_ de
// Copyright (C) 2001 David Megginson, david@megginson.com
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// $Id$

#ifndef FGCOMMONINPUT_H
#define FGCOMMONINPUT_H

#include <vector>
#include <simgear/structure/SGBinding.hxx>
#include <plib/ul.h>

#if defined( UL_WIN32 )
#define TGT_PLATFORM	"windows"
#elif defined ( UL_MAC_OSX )
#define TGT_PLATFORM    "mac"
#else
#define TGT_PLATFORM	"unix"
#endif

class FGCommonInput {
public:
  typedef vector<SGSharedPtr<SGBinding> > binding_list_t;

  /*
   read all "binding" nodes directly under the specified base node and fill the 
   vector of SGBinding supplied in binding_list. Reads all the mod-xxx bindings and 
   add the corresponding SGBindings.
   */
  static void read_bindings (const SGPropertyNode * base, binding_list_t * binding_list, int modifiers, string & module );
};

#endif
