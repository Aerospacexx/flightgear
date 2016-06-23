// util.cxx - general-purpose utility functions.
// Copyright (C) 2002  Curtis L. Olson  - http://www.flightgear.org/~curt
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <simgear/compiler.h>

#include <cmath>

#include <cstdlib>

#include <vector>

#include <simgear/debug/logstream.hxx>
#include <simgear/math/SGLimits.hxx>
#include <simgear/math/SGMisc.hxx>

#include <GUI/MessageBox.hxx>
#include "fg_io.hxx"
#include "fg_props.hxx"
#include "globals.hxx"
#include "util.hxx"

#ifdef OSG_LIBRARY_STATIC
#include "osgDB/Registry"
#endif

using std::vector;

// Originally written by Alex Perry.
double
fgGetLowPass (double current, double target, double timeratio)
{
    if ( timeratio < 0.0 ) {
        if ( timeratio < -1.0 ) {
                                // time went backwards; kill the filter
                current = target;
        } else {
                                // ignore mildly negative time
        }
    } else if ( timeratio < 0.2 ) {
                                // Normal mode of operation; fast
                                // approximation to exp(-timeratio)
        current = current * (1.0 - timeratio) + target * timeratio;
    } else if ( timeratio > 5.0 ) {
                                // Huge time step; assume filter has settled
        current = target;
    } else {
                                // Moderate time step; non linear response
        double keep = exp(-timeratio);
        current = current * keep + target * (1.0 - keep);
    }

    return current;
}

static string_list read_allowed_paths;
static string_list write_allowed_paths;

/**
 * Allowed paths here are absolute, and may contain _one_ *,
 * which matches any string
 */
void fgInitAllowedPaths()
{
    if(SGPath("ygjmyfvhhnvdoesnotexist").realpath() == "ygjmyfvhhnvdoesnotexist"){
        // Forbid using this version of fgValidatePath() with older
        // (not normalizing non-existent files) versions of realpath(),
        // as that would be a security hole
        flightgear::fatalMessageBox("Nasal initialization error",
                                    "Version mismatch - please update simgear",
                                    "");
        exit(-1);
    }
    read_allowed_paths.clear();
    write_allowed_paths.clear();
    std::string fg_root = globals->get_fg_root().realpath();
    std::string fg_home = globals->get_fg_home().realpath();
#if defined(_MSC_VER) /*for MS compilers */ || defined(_WIN32) /*needed for non MS windows compilers like MingW*/
     std::string sep = "\\";
#else
     std::string sep = "/";
#endif
    read_allowed_paths.push_back(fg_root + sep + "*");
    read_allowed_paths.push_back(fg_home + sep + "*");
    read_allowed_paths.push_back(fg_root);
    read_allowed_paths.push_back(fg_home);

    const PathList& aircraft_paths = globals->get_aircraft_paths();
    const PathList& scenery_paths = globals->get_secure_fg_scenery();
    // not plain fg_scenery, to avoid making
    // /sim/terrasync/scenery-dir a security hole
    PathList read_paths = aircraft_paths;
    read_paths.insert(read_paths.end(), scenery_paths.begin(), scenery_paths.end());


      for( PathList::const_iterator it = read_paths.begin(); it != read_paths.end(); ++it )
      {
        // if we get the initialization order wrong, better to have an
        // obvious error than a can-read-everything security hole...
          if (it->isNull() || fg_root.empty() || fg_home.empty()) {
              flightgear::fatalMessageBox("Nasal initialization error",
                 "Empty string in FG_ROOT, FG_HOME, FG_AIRCRAFT or FG_SCENERY",
                                   "or fgInitAllowedPaths() called too early");
              exit(-1);
          }
          read_allowed_paths.push_back(it->realpath() + sep + "*");
          read_allowed_paths.push_back(it->realpath());
      }

    write_allowed_paths.push_back(fg_home + sep + "*.sav");
    write_allowed_paths.push_back(fg_home + sep + "*.log");
    write_allowed_paths.push_back(fg_home + sep + "cache" + sep + "*");
    write_allowed_paths.push_back(fg_home + sep + "Export" + sep + "*");
    write_allowed_paths.push_back(fg_home + sep + "state" + sep + "*.xml");
    write_allowed_paths.push_back(fg_home + sep + "aircraft-data" + sep + "*.xml");
    write_allowed_paths.push_back(fg_home + sep + "Wildfire" + sep + "*.xml");
    write_allowed_paths.push_back(fg_home + sep + "runtime-jetways" + sep + "*.xml");
    write_allowed_paths.push_back(fg_home + sep + "Input" + sep + "Joysticks" + sep + "*.xml");
    
    // Check that it works
    std::string homePath = globals->get_fg_home().utf8Str();
    if(!fgValidatePath(homePath + "/../no.log",true).empty() ||
        !fgValidatePath(homePath + "/no.logt",true).empty() ||
        !fgValidatePath(homePath + "/nolog",true).empty() ||
        !fgValidatePath(homePath + "no.log",true).empty() ||
        !fgValidatePath(homePath + "\\..\\no.log",false).empty() ||
        fgValidatePath(homePath + "/aircraft-data/yes..xml",true).empty() ||
        fgValidatePath(homePath + "/.\\yes.bmp",false).empty()) {
            flightgear::fatalMessageBox("Nasal initialization error",
                                    "The FG_HOME directory must not be inside any of the FG_ROOT, FG_AIRCRAFT or FG_SCENERY directories",
                                    "(check that you have not accidentally included an extra :, as an empty part means the current directory)");
            exit(-1);
    }
}

/**
 * Check whether Nasal is allowed to access a path
 * Warning: because this always (not just on Windows) treats both \ and /
 * as path separators, and accepts relative paths (check-to-use race if
 * the current directory changes),
 * always use the returned path not the original one
 */
std::string fgValidatePath (const std::string& path, bool write)
{
    // Normalize the path (prevents ../../.. or symlink trickery)
    std::string normed_path = SGPath(path).realpath();
    
    const string_list& allowed_paths(write ? write_allowed_paths : read_allowed_paths);
    size_t star_pos;
    
    // Check against each allowed pattern
    for( string_list::const_iterator it = allowed_paths.begin();
                                     it != allowed_paths.end();
                                   ++it )
    {
        star_pos = it->find('*');
        if (star_pos == std::string::npos) {
            if (!(it->compare(normed_path))) {
                return normed_path;
            }
        } else {
            if ((it->size()-1 <= normed_path.size()) /* long enough to be a potential match */
                && !(it->substr(0,star_pos)
                    .compare(normed_path.substr(0,star_pos))) /* before-star parts match */
                && !(it->substr(star_pos+1,it->size()-star_pos-1)
                    .compare(normed_path.substr(star_pos+1+normed_path.size()-it->size(),
                      it->size()-star_pos-1))) /* after-star parts match */) {
                return normed_path;
            }
        }
    }
    // no match found
    return "";
}
std::string fgValidatePath(const SGPath& path, bool write) { return fgValidatePath(path.utf8Str(),write); }
// end of util.cxx

