// util.hxx - general-purpose utility functions.
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


#ifndef __UTIL_HXX
#define __UTIL_HXX 1

#ifndef __cplusplus
# error This library requires C++
#endif


/**
 * Initialize a single value through all existing weather levels.
 *
 * This function is useful mainly from the command-line.
 *
 * @param propname The name of the subproperty to initialized.
 * @param value The initial value.
 */
extern void fgDefaultWeatherValue (const char * propname, double value);


/**
 * Set up a plausible wind layout, boundary and aloft,
 * based on just a few parameters.
 *
 * @param min_hdg Minimal wind heading
 * @param max_hdg Maximal wind heading
 * @param speed Windspeed in knots
 * @param gust Wind gust variation in knots
 */
extern void fgSetupWind (double min_hdg, double max_hdg,
                         double speed, double gust);

/**
 * Clean up and exit FlightGear.
 *
 * This function makes sure that network connections and I/O streams
 * are cleaned up.
 *
 * @param status The exit status to pass to the operating system.
 */
extern void fgExit (int status = 0);


/**
 * Move a value towards a target.
 *
 * This function was originally written by Alex Perry.
 *
 * @param current The current value.
 * @param target The target value.
 * @param timeratio The percentage of smoothing time that has passed 
 *        (elapsed time/smoothing time)
 * @return The new value.
 */
extern double fgGetLowPass (double current, double target, double timeratio);


/**
 * Unescape string.
 *
 * @param str String possibly containing escaped characters.
 * @return string with escaped characters replaced by single character values.
 */
extern std::string fgUnescape (const char *str);


#endif // __UTIL_HXX
