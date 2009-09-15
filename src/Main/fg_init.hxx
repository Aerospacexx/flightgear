//
// fg_init.hxx -- Flight Gear top level initialization routines
//
// Written by Curtis Olson, started August 1997.
//
// Copyright (C) 1997  Curtis L. Olson  - http://www.flightgear.org/~curt
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


#ifndef _FG_INIT_HXX
#define _FG_INIT_HXX


#ifndef __cplusplus                                                          
# error This library requires C++
#endif                                   


#ifdef HAVE_CONFIG
#  include <config.h>
#endif

#include <simgear/compiler.h>
#include <simgear/timing/sg_time.hxx>

#include STL_STRING

#include <Airports/simple.hxx>

SG_USING_STD(string);


// Read in configuration (files and command line optoins) but only set
// fg_root
bool fgInitFGRoot ( int argc, char **argv );


// Return the current base package version
string fgBasePackageVersion();


// Read in configuration (file and command line)
bool fgInitConfig ( int argc, char **argv );


// Initialize the localization
SGPropertyNode *fgInitLocale(const char *language);


// Init navaids and waypoints
bool fgInitNav ();


// General house keeping initializations
bool fgInitGeneral ();


// This is the top level init routine which calls all the other
// initialization routines.  If you are adding a subsystem to flight
// gear, its initialization call should located in this routine.
bool fgInitSubsystems();


// Reset
void fgReInitSubsystems();


// Set the initial position based on presets (or defaults)
bool fgInitPosition();


// Listen to /sim/tower/airport-id and set tower view position accordingly
void fgInitTowerLocationListener();


// Initialize various time dependent systems (lighting, sun position, etc.)
// returns a new instance of the SGTime class
SGTime *fgInitTime();

// set up a time offset (aka warp) if one is specified
void fgInitTimeOffset();


#endif // _FG_INIT_HXX



