// options.cxx -- class to handle command line options
//
// Written by Curtis Olson, started April 1998.
//
// Copyright (C) 1998  Curtis L. Olson  - http://www.flightgear.org/~curt
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
#include <simgear/structure/exception.hxx>
#include <simgear/debug/logstream.hxx>

#include <math.h>		// rint()
#include <stdio.h>
#include <stdlib.h>		// atof(), atoi()
#include <string.h>		// strcmp()
#include <algorithm>

#include STL_STRING

#include <plib/ul.h>

#include <simgear/math/sg_random.h>
#include <simgear/misc/sgstream.hxx>
#include <simgear/misc/sg_path.hxx>

// #include <Include/general.hxx>
// #include <Airports/simple.hxx>
// #include <Cockpit/cockpit.hxx>
// #include <FDM/flight.hxx>

#include <Autopilot/route_mgr.hxx>
#include <GUI/gui.h>

#include "globals.hxx"
#include "fg_init.hxx"
#include "fg_props.hxx"
#include "options.hxx"
#include "util.hxx"
#include "viewmgr.hxx"


SG_USING_STD(string);
SG_USING_STD(sort);
SG_USING_NAMESPACE(std);


#define NEW_DEFAULT_MODEL_HZ 120

enum
{
    FG_OPTIONS_OK = 0,
    FG_OPTIONS_HELP = 1,
    FG_OPTIONS_ERROR = 2,
    FG_OPTIONS_VERBOSE_HELP = 3,
    FG_OPTIONS_SHOW_AIRCRAFT = 4
};

static double
atof( const string& str )
{

#ifdef __MWERKS__
    // -dw- if ::atof is called, then we get an infinite loop
    return std::atof( str.c_str() );
#else
    return ::atof( str.c_str() );
#endif
}

static int
atoi( const string& str )
{
#ifdef __MWERKS__
    // -dw- if ::atoi is called, then we get an infinite loop
    return std::atoi( str.c_str() );
#else
    return ::atoi( str.c_str() );
#endif
}


/**
 * Set a few fail-safe default property values.
 *
 * These should all be set in $FG_ROOT/preferences.xml, but just
 * in case, we provide some initial sane values here. This method
 * should be invoked *before* reading any init files.
 */
void
fgSetDefaults ()
{
    // set a possibly independent location for scenery data
    char *envp = ::getenv( "FG_SCENERY" );

    if ( envp != NULL ) {
	// fg_root could be anywhere, so default to environmental
	// variable $FG_ROOT if it is set.
        globals->set_fg_scenery(envp);
    } else {
	// Otherwise, default to Scenery being in $FG_ROOT/Scenery
	globals->set_fg_scenery("");
    }
				// Position (deliberately out of range)
    fgSetDouble("/position/longitude-deg", 9999.0);
    fgSetDouble("/position/latitude-deg", 9999.0);
    fgSetDouble("/position/altitude-ft", -9999.0);

				// Orientation
    fgSetDouble("/orientation/heading-deg", 270);
    fgSetDouble("/orientation/roll-deg", 0);
    fgSetDouble("/orientation/pitch-deg", 0.424);

				// Velocities
    fgSetDouble("/velocities/uBody-fps", 0.0);
    fgSetDouble("/velocities/vBody-fps", 0.0);
    fgSetDouble("/velocities/wBody-fps", 0.0);
    fgSetDouble("/velocities/speed-north-fps", 0.0);
    fgSetDouble("/velocities/speed-east-fps", 0.0);
    fgSetDouble("/velocities/speed-down-fps", 0.0);
    fgSetDouble("/velocities/airspeed-kt", 0.0);
    fgSetDouble("/velocities/mach", 0.0);

                                // Presets
    fgSetDouble("/sim/presets/longitude-deg", 9999.0);
    fgSetDouble("/sim/presets/latitude-deg", 9999.0);
    fgSetDouble("/sim/presets/altitude-ft", -9999.0);

    fgSetDouble("/sim/presets/heading-deg", 270);
    fgSetDouble("/sim/presets/roll-deg", 0);
    fgSetDouble("/sim/presets/pitch-deg", 0.424);

    fgSetString("/sim/presets/speed-set", "knots");
    fgSetDouble("/sim/presets/airspeed-kt", 0.0);
    fgSetDouble("/sim/presets/mach", 0.0);
    fgSetDouble("/sim/presets/uBody-fps", 0.0);
    fgSetDouble("/sim/presets/vBody-fps", 0.0);
    fgSetDouble("/sim/presets/wBody-fps", 0.0);
    fgSetDouble("/sim/presets/speed-north-fps", 0.0);
    fgSetDouble("/sim/presets/speed-east-fps", 0.0);
    fgSetDouble("/sim/presets/speed-down-fps", 0.0);

    fgSetBool("/sim/presets/onground", true);
    fgSetBool("/sim/presets/trim", false);

				// Miscellaneous
    fgSetBool("/sim/startup/game-mode", false);
    fgSetBool("/sim/startup/splash-screen", true);
    fgSetBool("/sim/startup/intro-music", true);
    // we want mouse-pointer to have an undefined value if nothing is
    // specified so we can do the right thing for voodoo-1/2 cards.
    // fgSetString("/sim/startup/mouse-pointer", "disabled");
    fgSetString("/sim/control-mode", "joystick");
    fgSetBool("/sim/auto-coordination", false);
#if defined(WIN32)
    fgSetString("/sim/startup/browser-app", "webrun.bat");
#elif defined(__APPLE__)
    fgSetString("/sim/startup/browser-app", "open");
#elif defined(sgi)
    fgSetString("/sim/startup/browser-app", "launchWebJumper");
#else
    envp = ::getenv( "WEBBROWSER" );
    if (!envp) envp = "netscape";
    fgSetString("/sim/startup/browser-app", envp);
#endif
    fgSetString("/sim/logging/priority", "alert");

				// Features
    fgSetBool("/sim/hud/color/antialiased", false);
    fgSetBool("/sim/hud/enable3d", true);
    fgSetBool("/sim/hud/visibility", false);
    fgSetBool("/sim/panel/visibility", true);
    fgSetBool("/sim/sound/pause", false);

				// Flight Model options
    fgSetString("/sim/flight-model", "jsb");
    fgSetString("/sim/aero", "c172");
    fgSetInt("/sim/model-hz", NEW_DEFAULT_MODEL_HZ);
    fgSetInt("/sim/speed-up", 1);

				// Rendering options
    fgSetString("/sim/rendering/fog", "nicest");
    fgSetBool("/environment/clouds/status", true);
    fgSetBool("/sim/startup/fullscreen", false);
    fgSetBool("/sim/rendering/shading", true);
    fgSetBool("/sim/rendering/skyblend", true);
    fgSetBool("/sim/rendering/textures", true);
    fgSetBool("/sim/rendering/wireframe", false);
    fgSetBool("/sim/rendering/horizon-effect", false);
    fgSetBool("/sim/rendering/enhanced-lighting", false);
    fgSetBool("/sim/rendering/distance-attenuation", false);
    fgSetBool("/sim/rendering/specular-highlight", true);
    fgSetInt("/sim/startup/xsize", 800);
    fgSetInt("/sim/startup/ysize", 600);
    fgSetInt("/sim/rendering/bits-per-pixel", 16);
    fgSetString("/sim/view-mode", "pilot");
    fgSetDouble("/sim/current-view/heading-offset-deg", 0);

				// HUD options
    fgSetString("/sim/startup/units", "feet");
    fgSetString("/sim/hud/frame-stat-type", "tris");
	
				// Time options
    fgSetInt("/sim/startup/time-offset", 0);
    fgSetString("/sim/startup/time-offset-type", "system-offset");
    fgSetLong("/sim/time/cur-time-override", 0);

                                // Freeze options
    fgSetBool("/sim/freeze/master", false);
    fgSetBool("/sim/freeze/position", false);
    fgSetBool("/sim/freeze/clock", false);
    fgSetBool("/sim/freeze/fuel", false);

    fgSetString("/sim/multiplay/callsign", "callsign");
    fgSetString("/sim/multiplay/rxhost", "0");
    fgSetString("/sim/multiplay/txhost", "0");
    fgSetInt("/sim/multiplay/rxport", 0);
    fgSetInt("/sim/multiplay/txport", 0);
}

static bool
parse_wind (const string &wind, double * min_hdg, double * max_hdg,
	    double * speed, double * gust)
{
  string::size_type pos = wind.find('@');
  if (pos == string::npos)
    return false;
  string dir = wind.substr(0, pos);
  string spd = wind.substr(pos+1);
  pos = dir.find(':');
  if (pos == string::npos) {
    *min_hdg = *max_hdg = atof(dir.c_str());
  } else {
    *min_hdg = atof(dir.substr(0,pos).c_str());
    *max_hdg = atof(dir.substr(pos+1).c_str());
  }
  pos = spd.find(':');
  if (pos == string::npos) {
    *speed = *gust = atof(spd.c_str());
  } else {
    *speed = atof(spd.substr(0,pos).c_str());
    *gust = atof(spd.substr(pos+1).c_str());
  }
  return true;
}

// parse a time string ([+/-]%f[:%f[:%f]]) into hours
static double
parse_time(const string& time_in) {
    char *time_str, num[256];
    double hours, minutes, seconds;
    double result = 0.0;
    int sign = 1;
    int i;

    time_str = (char *)time_in.c_str();

    // printf("parse_time(): %s\n", time_str);

    // check for sign
    if ( strlen(time_str) ) {
	if ( time_str[0] == '+' ) {
	    sign = 1;
	    time_str++;
	} else if ( time_str[0] == '-' ) {
	    sign = -1;
	    time_str++;
	}
    }
    // printf("sign = %d\n", sign);

    // get hours
    if ( strlen(time_str) ) {
	i = 0;
	while ( (time_str[0] != ':') && (time_str[0] != '\0') ) {
	    num[i] = time_str[0];
	    time_str++;
	    i++;
	}
	if ( time_str[0] == ':' ) {
	    time_str++;
	}
	num[i] = '\0';
	hours = atof(num);
	// printf("hours = %.2lf\n", hours);

	result += hours;
    }

    // get minutes
    if ( strlen(time_str) ) {
	i = 0;
	while ( (time_str[0] != ':') && (time_str[0] != '\0') ) {
	    num[i] = time_str[0];
	    time_str++;
	    i++;
	}
	if ( time_str[0] == ':' ) {
	    time_str++;
	}
	num[i] = '\0';
	minutes = atof(num);
	// printf("minutes = %.2lf\n", minutes);

	result += minutes / 60.0;
    }

    // get seconds
    if ( strlen(time_str) ) {
	i = 0;
	while ( (time_str[0] != ':') && (time_str[0] != '\0') ) {
	    num[i] = time_str[0];
	    time_str++;
	    i++;
	}
	num[i] = '\0';
	seconds = atof(num);
	// printf("seconds = %.2lf\n", seconds);

	result += seconds / 3600.0;
    }

    SG_LOG( SG_GENERAL, SG_INFO, " parse_time() = " << sign * result );

    return(sign * result);
}


// parse a date string (yyyy:mm:dd:hh:mm:ss) into a time_t (seconds)
static long int 
parse_date( const string& date)
{
    struct tm gmt;
    char * date_str, num[256];
    int i;
    // initialize to zero
    gmt.tm_sec = 0;
    gmt.tm_min = 0;
    gmt.tm_hour = 0;
    gmt.tm_mday = 0;
    gmt.tm_mon = 0;
    gmt.tm_year = 0;
    gmt.tm_isdst = 0; // ignore daylight savings time for the moment
    date_str = (char *)date.c_str();
    // get year
    if ( strlen(date_str) ) {
	i = 0;
	while ( (date_str[0] != ':') && (date_str[0] != '\0') ) {
	    num[i] = date_str[0];
	    date_str++;
	    i++;
	}
	if ( date_str[0] == ':' ) {
	    date_str++;
	}
	num[i] = '\0';
	gmt.tm_year = atoi(num) - 1900;
    }
    // get month
    if ( strlen(date_str) ) {
	i = 0;
	while ( (date_str[0] != ':') && (date_str[0] != '\0') ) {
	    num[i] = date_str[0];
	    date_str++;
	    i++;
	}
	if ( date_str[0] == ':' ) {
	    date_str++;
	}
	num[i] = '\0';
	gmt.tm_mon = atoi(num) -1;
    }
    // get day
    if ( strlen(date_str) ) {
	i = 0;
	while ( (date_str[0] != ':') && (date_str[0] != '\0') ) {
	    num[i] = date_str[0];
	    date_str++;
	    i++;
	}
	if ( date_str[0] == ':' ) {
	    date_str++;
	}
	num[i] = '\0';
	gmt.tm_mday = atoi(num);
    }
    // get hour
    if ( strlen(date_str) ) {
	i = 0;
	while ( (date_str[0] != ':') && (date_str[0] != '\0') ) {
	    num[i] = date_str[0];
	    date_str++;
	    i++;
	}
	if ( date_str[0] == ':' ) {
	    date_str++;
	}
	num[i] = '\0';
	gmt.tm_hour = atoi(num);
    }
    // get minute
    if ( strlen(date_str) ) {
	i = 0;
	while ( (date_str[0] != ':') && (date_str[0] != '\0') ) {
	    num[i] = date_str[0];
	    date_str++;
	    i++;
	}
	if ( date_str[0] == ':' ) {
	    date_str++;
	}
	num[i] = '\0';
	gmt.tm_min = atoi(num);
    }
    // get second
    if ( strlen(date_str) ) {
	i = 0;
	while ( (date_str[0] != ':') && (date_str[0] != '\0') ) {
	    num[i] = date_str[0];
	    date_str++;
	    i++;
	}
	if ( date_str[0] == ':' ) {
	    date_str++;
	}
	num[i] = '\0';
	gmt.tm_sec = atoi(num);
    }
    time_t theTime = sgTimeGetGMT( gmt.tm_year, gmt.tm_mon, gmt.tm_mday,
				   gmt.tm_hour, gmt.tm_min, gmt.tm_sec );
    //printf ("Date is %s\n", ctime(&theTime));
    //printf ("in seconds that is %d\n", theTime);
    //exit(1);
    return (theTime);
}


// parse angle in the form of [+/-]ddd:mm:ss into degrees
static double
parse_degree( const string& degree_str) {
    double result = parse_time( degree_str );

    // printf("Degree = %.4f\n", result);

    return(result);
}


// parse time offset string into seconds
static int
parse_time_offset( const string& time_str) {
    int result;

    // printf("time offset = %s\n", time_str);

#ifdef HAVE_RINT
    result = (int)rint(parse_time(time_str) * 3600.0);
#else
    result = (int)(parse_time(time_str) * 3600.0);
#endif

    // printf("parse_time_offset(): %d\n", result);

    return( result );
}


// Parse --fov=x.xx type option 
static double
parse_fov( const string& arg ) {
    double fov = atof(arg);

    if ( fov < FG_FOV_MIN ) { fov = FG_FOV_MIN; }
    if ( fov > FG_FOV_MAX ) { fov = FG_FOV_MAX; }

    fgSetDouble("/sim/current-view/field-of-view", fov);

    // printf("parse_fov(): result = %.4f\n", fov);

    return fov;
}


// Parse I/O channel option
//
// Format is "--protocol=medium,direction,hz,medium_options,..."
//
//   protocol = { native, nmea, garmin, AV400, fgfs, rul, pve, etc. }
//   medium = { serial, socket, file, etc. }
//   direction = { in, out, bi }
//   hz = number of times to process channel per second (floating
//        point values are ok.
//
// Serial example "--nmea=serial,dir,hz,device,baud" where
// 
//  device = OS device name of serial line to be open()'ed
//  baud = {300, 1200, 2400, ..., 230400}
//
// Socket exacmple "--native=socket,dir,hz,machine,port,style" where
//
//  machine = machine name or ip address if client (leave empty if server)
//  port = port, leave empty to let system choose
//  style = tcp or udp
//
// File example "--garmin=file,dir,hz,filename" where
//
//  filename = file system file name

static bool
add_channel( const string& type, const string& channel_str ) {
    SG_LOG(SG_GENERAL, SG_INFO, "Channel string = " << channel_str );
    globals->get_channel_options_list()->push_back( type + "," + channel_str );
    return true;
}


// The parse wp and parse flight-plan options don't work anymore, because
// the route manager and the airport subsystems have not yet been initialized
// at this stage.

// Parse --wp=ID[@alt]
static void
parse_wp( const string& arg ) {
    string_list *waypoints = globals->get_initial_waypoints();
    if (!waypoints) {
        waypoints = new string_list;
        globals->set_initial_waypoints(waypoints);
    }
    waypoints->push_back(arg);
}


// Parse --flight-plan=[file]
static bool
parse_flightplan(const string& arg)
{
    string_list *waypoints = globals->get_initial_waypoints();
    if (!waypoints) {
        waypoints = new string_list;
        globals->set_initial_waypoints(waypoints);
    }

    sg_gzifstream in(arg.c_str());
    if ( !in.is_open() )
        return false;

    while ( true ) {
        string line;

#if defined( macintosh )
        getline( in, line, '\r' );
#else
        getline( in, line, '\n' );
#endif
        // catch extraneous (DOS) line ending character
        if ( line[line.length() - 1] < 32 )
            line = line.substr( 0, line.length()-1 );

        if ( in.eof() )
            break;

        waypoints->push_back(line);
    }
    return true;
}


static int
fgOptLanguage( const char *arg )
{
    globals->set_locale( fgInitLocale( arg ) );
    return FG_OPTIONS_OK;
}

static void
clearLocation ()
{
    fgSetString("/sim/presets/airport-id", "");
    fgSetString("/sim/presets/vor-id", "");
    fgSetString("/sim/presets/ndb-id", "");
    fgSetString("/sim/presets/carrier", "");
    fgSetString("/sim/presets/parkpos", "");
    fgSetString("/sim/presets/fix", "");
}

static int
fgOptVOR( const char * arg )
{
    clearLocation();
    fgSetString("/sim/presets/vor-id", arg);
    return FG_OPTIONS_OK;
}

static int
fgOptNDB( const char * arg )
{
    clearLocation();
    fgSetString("/sim/presets/ndb-id", arg);
    return FG_OPTIONS_OK;
}

static int
fgOptCarrier( const char * arg )
{
    clearLocation();
    fgSetString("/sim/presets/carrier", arg);
    return FG_OPTIONS_OK;
}

static int
fgOptParkpos( const char * arg )
{
    fgSetString("/sim/presets/parkpos", arg);
    return FG_OPTIONS_OK;
}

static int
fgOptFIX( const char * arg )
{
    clearLocation();
    fgSetString("/sim/presets/fix", arg);
    return FG_OPTIONS_OK;
}

static int
fgOptLon( const char *arg )
{
    clearLocation();
    fgSetDouble("/sim/presets/longitude-deg", parse_degree( arg ));
    fgSetDouble("/position/longitude-deg", parse_degree( arg ));
    return FG_OPTIONS_OK;
}

static int
fgOptLat( const char *arg )
{
    clearLocation();
    fgSetDouble("/sim/presets/latitude-deg", parse_degree( arg ));
    fgSetDouble("/position/latitude-deg", parse_degree( arg ));
    return FG_OPTIONS_OK;
}

static int
fgOptAltitude( const char *arg )
{
    fgSetBool("/sim/presets/onground", false);
    if ( !strcmp(fgGetString("/sim/startup/units"), "feet") )
        fgSetDouble("/sim/presets/altitude-ft", atof( arg ));
    else
        fgSetDouble("/sim/presets/altitude-ft",
		    atof( arg ) * SG_METER_TO_FEET);
    return FG_OPTIONS_OK;
}

static int
fgOptUBody( const char *arg )
{
    fgSetString("/sim/presets/speed-set", "UVW");
    if ( !strcmp(fgGetString("/sim/startup/units"), "feet") )
	fgSetDouble("/sim/presets/uBody-fps", atof( arg ));
    else
	fgSetDouble("/sim/presets/uBody-fps",
                    atof( arg ) * SG_METER_TO_FEET);
    return FG_OPTIONS_OK;
}

static int
fgOptVBody( const char *arg )
{
    fgSetString("/sim/presets/speed-set", "UVW");
    if ( !strcmp(fgGetString("/sim/startup/units"), "feet") )
	fgSetDouble("/sim/presets/vBody-fps", atof( arg ));
    else
	fgSetDouble("/sim/presets/vBody-fps",
			    atof( arg ) * SG_METER_TO_FEET);
    return FG_OPTIONS_OK;
}

static int
fgOptWBody( const char *arg )
{
    fgSetString("/sim/presets/speed-set", "UVW");
    if ( !strcmp(fgGetString("/sim/startup/units"), "feet") )
	fgSetDouble("/sim/presets/wBody-fps", atof(arg));
    else
	fgSetDouble("/sim/presets/wBody-fps",
			    atof(arg) * SG_METER_TO_FEET);
    return FG_OPTIONS_OK;
}

static int
fgOptVNorth( const char *arg )
{
    fgSetString("/sim/presets/speed-set", "NED");
    if ( !strcmp(fgGetString("/sim/startup/units"), "feet") )
	fgSetDouble("/sim/presets/speed-north-fps", atof( arg ));
    else
	fgSetDouble("/sim/presets/speed-north-fps",
			    atof( arg ) * SG_METER_TO_FEET);
    return FG_OPTIONS_OK;
}

static int
fgOptVEast( const char *arg )
{
    fgSetString("/sim/presets/speed-set", "NED");
    if ( !strcmp(fgGetString("/sim/startup/units"), "feet") )
	fgSetDouble("/sim/presets/speed-east-fps", atof(arg));
    else
	fgSetDouble("/sim/presets/speed-east-fps",
		    atof(arg) * SG_METER_TO_FEET);
    return FG_OPTIONS_OK;
}

static int
fgOptVDown( const char *arg )
{
    fgSetString("/sim/presets/speed-set", "NED");
    if ( !strcmp(fgGetString("/sim/startup/units"), "feet") )
	fgSetDouble("/sim/presets/speed-down-fps", atof(arg));
    else
	fgSetDouble("/sim/presets/speed-down-fps",
			    atof(arg) * SG_METER_TO_FEET);
    return FG_OPTIONS_OK;
}

static int
fgOptVc( const char *arg )
{
    // fgSetString("/sim/presets/speed-set", "knots");
    // fgSetDouble("/velocities/airspeed-kt", atof(arg.substr(5)));
    fgSetString("/sim/presets/speed-set", "knots");
    fgSetDouble("/sim/presets/airspeed-kt", atof(arg));
    return FG_OPTIONS_OK;
}

static int
fgOptMach( const char *arg )
{
    fgSetString("/sim/presets/speed-set", "mach");
    fgSetDouble("/sim/presets/mach", atof(arg));
    return FG_OPTIONS_OK;
}

static int
fgOptRoc( const char *arg )
{
    fgSetDouble("/velocities/vertical-speed-fps", atof(arg)/60);
    return FG_OPTIONS_OK;
}

static int
fgOptFgRoot( const char *arg )
{
    globals->set_fg_root(arg);
    return FG_OPTIONS_OK;
}

static int
fgOptFgScenery( const char *arg )
{
    globals->set_fg_scenery(arg);
    return FG_OPTIONS_OK;
}

static int
fgOptFov( const char *arg )
{
    parse_fov( arg );
    return FG_OPTIONS_OK;
}

static int
fgOptGeometry( const char *arg )
{
    bool geometry_ok = true;
    int xsize = 0, ysize = 0;
    string geometry = arg;
    string::size_type i = geometry.find('x');

    if (i != string::npos) {
        xsize = atoi(geometry.substr(0, i));
        ysize = atoi(geometry.substr(i+1));
    } else {
        geometry_ok = false;
    }

    if ( xsize <= 0 || ysize <= 0 ) {
        xsize = 640;
        ysize = 480;
        geometry_ok = false;
    }

    if ( !geometry_ok ) {
        SG_LOG( SG_GENERAL, SG_ALERT, "Unknown geometry: " << geometry );
        SG_LOG( SG_GENERAL, SG_ALERT,
 	        "Setting geometry to " << xsize << 'x' << ysize << '\n');
    } else {
        SG_LOG( SG_GENERAL, SG_INFO,
	        "Setting geometry to " << xsize << 'x' << ysize << '\n');
        fgSetInt("/sim/startup/xsize", xsize);
        fgSetInt("/sim/startup/ysize", ysize);
    }
    return FG_OPTIONS_OK;
}

static int
fgOptBpp( const char *arg )
{
    string bits_per_pix = arg;
    if ( bits_per_pix == "16" ) {
	fgSetInt("/sim/rendering/bits-per-pixel", 16);
    } else if ( bits_per_pix == "24" ) {
	fgSetInt("/sim/rendering/bits-per-pixel", 24);
    } else if ( bits_per_pix == "32" ) {
	fgSetInt("/sim/rendering/bits-per-pixel", 32);
    } else {
	SG_LOG(SG_GENERAL, SG_ALERT, "Unsupported bpp " << bits_per_pix);
    }
    return FG_OPTIONS_OK;
}

static int
fgOptTimeOffset( const char *arg )
{
    fgSetInt("/sim/startup/time-offset",
                parse_time_offset( arg ));
    fgSetString("/sim/startup/time-offset-type", "system-offset");
    return FG_OPTIONS_OK;
}

static int
fgOptStartDateSys( const char *arg )
{
    fgSetInt("/sim/startup/time-offset", parse_date( arg ) );
    fgSetString("/sim/startup/time-offset-type", "system");
    return FG_OPTIONS_OK;
}

static int
fgOptStartDateLat( const char *arg )
{
    fgSetInt("/sim/startup/time-offset", parse_date( arg ) );
    fgSetString("/sim/startup/time-offset-type", "latitude");
    return FG_OPTIONS_OK;
}

static int
fgOptStartDateGmt( const char *arg )
{
    fgSetInt("/sim/startup/time-offset", parse_date( arg ) );
    fgSetString("/sim/startup/time-offset-type", "gmt");
    return FG_OPTIONS_OK;
}

static int
fgSetupProxy( const char *arg )
{
    string options = arg;
    string host, port, auth;
    unsigned int pos;

    host = port = auth = "";
    if ((pos = options.find("@")) != string::npos) 
        auth = options.substr(0, pos++);
    else
        pos = 0;

    host = options.substr(pos, options.size());
    if ((pos = host.find(":")) != string::npos) {
        port = host.substr(++pos, host.size());
        host.erase(--pos, host.size());
    }

    fgSetString("/sim/presets/proxy/host", host.c_str());
    fgSetString("/sim/presets/proxy/port", port.c_str());
    fgSetString("/sim/presets/proxy/authentication", auth.c_str());

    return FG_OPTIONS_OK;
}

static int
fgOptTraceRead( const char *arg )
{
    string name = arg;
    SG_LOG(SG_GENERAL, SG_INFO, "Tracing reads for property " << name);
    fgGetNode(name.c_str(), true)
	->setAttribute(SGPropertyNode::TRACE_READ, true);
    return FG_OPTIONS_OK;
}

static int
fgOptLogLevel( const char *arg )
{
    fgSetString("/sim/logging/classes", "all");
    fgSetString("/sim/logging/priority", arg);

    string priority = arg;
    logbuf::set_log_classes(SG_ALL);
    if (priority == "bulk") {
      logbuf::set_log_priority(SG_BULK);
    } else if (priority == "debug") {
      logbuf::set_log_priority(SG_DEBUG);
    } else if (priority == "info") {
      logbuf::set_log_priority(SG_INFO);
    } else if (priority == "warn") {
      logbuf::set_log_priority(SG_WARN);
    } else if (priority == "alert") {
      logbuf::set_log_priority(SG_ALERT);
    } else {
      SG_LOG(SG_GENERAL, SG_WARN, "Unknown logging priority " << priority);
    }
    SG_LOG(SG_GENERAL, SG_DEBUG, "Logging priority is " << priority);

    return FG_OPTIONS_OK;
}


static int
fgOptTraceWrite( const char *arg )
{
    string name = arg;
    SG_LOG(SG_GENERAL, SG_INFO, "Tracing writes for property " << name);
    fgGetNode(name.c_str(), true)
	->setAttribute(SGPropertyNode::TRACE_WRITE, true);
    return FG_OPTIONS_OK;
}

static int
fgOptViewOffset( const char *arg )
{
    // $$$ begin - added VS Renganathan, 14 Oct 2K
    // for multi-window outside window imagery
    string woffset = arg;
    double default_view_offset = 0.0;
    if ( woffset == "LEFT" ) {
	    default_view_offset = SGD_PI * 0.25;
    } else if ( woffset == "RIGHT" ) {
	default_view_offset = SGD_PI * 1.75;
    } else if ( woffset == "CENTER" ) {
	default_view_offset = 0.00;
    } else {
	default_view_offset = atof( woffset.c_str() ) * SGD_DEGREES_TO_RADIANS;
    }
    /* apparently not used (CLO, 11 Jun 2002) 
        FGViewer *pilot_view =
	    (FGViewer *)globals->get_viewmgr()->get_view( 0 ); */
    // this will work without calls to the viewer...
    fgSetDouble( "/sim/current-view/heading-offset-deg",
                    default_view_offset  * SGD_RADIANS_TO_DEGREES );
    // $$$ end - added VS Renganathan, 14 Oct 2K
    return FG_OPTIONS_OK;
}

static int
fgOptVisibilityMeters( const char *arg )
{
    double visibility = atof( arg );
    fgDefaultWeatherValue("visibility-m", visibility);
    return FG_OPTIONS_OK;
}

static int
fgOptVisibilityMiles( const char *arg )
{
    double visibility = atof( arg ) * 5280.0 * SG_FEET_TO_METER;
    fgDefaultWeatherValue("visibility-m", visibility);
    return FG_OPTIONS_OK;
}

static int
fgOptRandomWind( const char *arg )
{
    double min_hdg = sg_random() * 360.0;
    double max_hdg = min_hdg + (20 - sqrt(sg_random() * 400));
    double speed = sg_random() * sg_random() * 40;
    double gust = speed + (10 - sqrt(sg_random() * 100));
    fgSetupWind(min_hdg, max_hdg, speed, gust);
    return FG_OPTIONS_OK;
}

static int
fgOptWind( const char *arg )
{
    double min_hdg, max_hdg, speed, gust;
    if (!parse_wind( arg, &min_hdg, &max_hdg, &speed, &gust)) {
	SG_LOG( SG_GENERAL, SG_ALERT, "bad wind value " << arg );
	return FG_OPTIONS_ERROR;
    }
    fgSetupWind(min_hdg, max_hdg, speed, gust);
    return FG_OPTIONS_OK;
}

static int
fgOptTurbulence( const char *arg )
{
    fgDefaultWeatherValue("turbulence/magnitude-norm", atof(arg));
    return FG_OPTIONS_OK;
}

static int
fgOptCeiling( const char *arg )
{
    double elevation, thickness;
    string spec = arg;
    string::size_type pos = spec.find(':');
    if (pos == string::npos) {
        elevation = atof(spec.c_str());
        thickness = 2000;
    } else {
        elevation = atof(spec.substr(0, pos).c_str());
        thickness = atof(spec.substr(pos + 1).c_str());
    }
    fgSetDouble("/environment/clouds/layer[0]/elevation-ft", elevation);
    fgSetDouble("/environment/clouds/layer[0]/thickness-ft", thickness);
    fgSetString("/environment/clouds/layer[0]/coverage", "overcast");
    return FG_OPTIONS_OK;
}

static int
fgOptWp( const char *arg )
{
    parse_wp( arg );
    return FG_OPTIONS_OK;
}

static int
fgOptFlightPlan( const char *arg )
{
    parse_flightplan ( arg );
    return FG_OPTIONS_OK;
}

static int
fgOptConfig( const char *arg )
{
    string file = arg;
    try {
	readProperties(file, globals->get_props());
    } catch (const sg_exception &e) {
	string message = "Error loading config file: ";
	message += e.getFormattedMessage() + e.getOrigin();
	SG_LOG(SG_INPUT, SG_ALERT, message);
	exit(2);
    }
    return FG_OPTIONS_OK;
}

static bool
parse_colon (const string &s, double * val1, double * val2)
{
    string::size_type pos = s.find(':');
    if (pos == string::npos) {
        *val2 = atof(s);
        return false;
    } else {
        *val1 = atof(s.substr(0, pos).c_str());
        *val2 = atof(s.substr(pos+1).c_str());
        return true;
    }
}


static int
fgOptFailure( const char * arg )
{
    string a = arg;
    if (a == "pitot") {
        fgSetBool("/systems/pitot/serviceable", false);
    } else if (a == "static") {
        fgSetBool("/systems/static/serviceable", false);
    } else if (a == "vacuum") {
        fgSetBool("/systems/vacuum/serviceable", false);
    } else if (a == "electrical") {
        fgSetBool("/systems/electrical/serviceable", false);
    } else {
        SG_LOG(SG_INPUT, SG_ALERT, "Unknown failure mode: " << a);
        return FG_OPTIONS_ERROR;
    }

    return FG_OPTIONS_OK;
}


static int
fgOptNAV1( const char * arg )
{
    double radial, freq;
    if (parse_colon(arg, &radial, &freq))
        fgSetDouble("/instrumentation/nav[0]/radials/selected-deg", radial);
    fgSetDouble("/instrumentation/nav[0]/frequencies/selected-mhz", freq);
    return FG_OPTIONS_OK;
}

static int
fgOptNAV2( const char * arg )
{
    double radial, freq;
    if (parse_colon(arg, &radial, &freq))
        fgSetDouble("/instrumentation/nav[1]/radials/selected-deg", radial);
    fgSetDouble("/instrumentation/nav[1]/frequencies/selected-mhz", freq);
    return FG_OPTIONS_OK;
}

static int
fgOptADF( const char * arg )
{
    double rot, freq;
    if (parse_colon(arg, &rot, &freq))
        fgSetDouble("/instrumentation/adf/rotation-deg", rot);
    fgSetDouble("/instrumentation/adf/frequencies/selected-khz", freq);
    return FG_OPTIONS_OK;
}

static int
fgOptDME( const char *arg )
{
    string opt = arg;
    if (opt == "nav1") {
        fgSetInt("/instrumentation/dme/switch-position", 1);
        fgSetString("/instrumentation/dme/frequencies/source",
                    "/instrumentation/nav[0]/frequencies/selected-mhz");
    } else if (opt == "nav2") {
        fgSetInt("/instrumentation/dme/switch-position", 3);
        fgSetString("/instrumentation/dme/frequencies/source",
                    "/instrumentation/nav[1]/frequencies/selected-mhz");
    } else {
        fgSetInt("/instrumentation/dme/switch-position", 2);
        fgSetString("/instrumentation/dme/frequencies/source",
                    "/instrumentation/dme/frequencies/selected-mhz");
        fgSetString("/instrumentation/dme/frequencies/selected-mhz", arg);
    }
    return FG_OPTIONS_OK;
}

static int
fgOptLivery( const char *arg )
{
    string opt = arg;
    string livery_path = "livery/" + opt;
    fgSetString("/sim/model/texture-path", livery_path.c_str() );
    return FG_OPTIONS_OK;
}

static int
fgOptScenario( const char *arg )
{
    SGPropertyNode_ptr ai_node = fgGetNode( "/sim/ai", true );
    vector<SGPropertyNode_ptr> scenarii = ai_node->getChildren( "scenario" );
    int index = -1;
    for ( size_t i = 0; i < scenarii.size(); ++i ) {
        int ind = scenarii[i]->getIndex();
        if ( index < ind ) {
            index = ind;
        }
    }
    SGPropertyNode_ptr scenario = ai_node->getNode( "scenario", index + 1, true );
    scenario->setStringValue( arg );
    ai_node->setBoolValue( "enabled", true );
    return FG_OPTIONS_OK;
}

static int
fgOptRunway( const char *arg )
{
    fgSetString("/sim/presets/runway", arg );
    fgSetBool("/sim/presets/runway-requested", true );
    return FG_OPTIONS_OK;
}

static map<string,size_t> fgOptionMap;

/*
   option       has_param type        property         b_param s_param  func

where:
 option    : name of the option
 has_param : option is --name=value if true or --name if false
 type      : OPTION_BOOL    - property is a boolean
             OPTION_STRING  - property is a string
             OPTION_DOUBLE  - property is a double
             OPTION_INT     - property is an integer
             OPTION_CHANNEL - name of option is the name of a channel
             OPTION_FUNC    - the option trigger a function
 b_param   : if type==OPTION_BOOL,
             value set to the property (has_param is false for boolean)
 s_param   : if type==OPTION_STRING,
             value set to the property if has_param is false
 func      : function called if type==OPTION_FUNC. if has_param is true,
             the value is passed to the function as a string, otherwise,
             0 is passed. 

    For OPTION_DOUBLE and OPTION_INT, the parameter value is converted into a
    double or an integer and set to the property.

    For OPTION_CHANNEL, add_channel is called with the parameter value as the
    argument.
*/

enum OptionType { OPTION_BOOL, OPTION_STRING, OPTION_DOUBLE, OPTION_INT, OPTION_CHANNEL, OPTION_FUNC };
struct OptionDesc {
    char *option;
    bool has_param;
    enum OptionType type;
    char *property;
    bool b_param;
    char *s_param;
    int (*func)( const char * );
    } fgOptionArray[] = {
       
    {"language",                     true,  OPTION_FUNC,   "", false, "", fgOptLanguage },
    {"disable-game-mode",            false, OPTION_BOOL,   "/sim/startup/game-mode", false, "", 0 },
    {"enable-game-mode",             false, OPTION_BOOL,   "/sim/startup/game-mode", true, "", 0 },
    {"disable-splash-screen",        false, OPTION_BOOL,   "/sim/startup/splash-screen", false, "", 0 },
    {"enable-splash-screen",         false, OPTION_BOOL,   "/sim/startup/splash-screen", true, "", 0 },
    {"disable-intro-music",          false, OPTION_BOOL,   "/sim/startup/intro-music", false, "", 0 },
    {"enable-intro-music",           false, OPTION_BOOL,   "/sim/startup/intro-music", true, "", 0 },
    {"disable-mouse-pointer",        false, OPTION_STRING, "/sim/startup/mouse-pointer", false, "disabled", 0 },
    {"enable-mouse-pointer",         false, OPTION_STRING, "/sim/startup/mouse-pointer", false, "enabled", 0 },
    {"disable-random-objects",       false, OPTION_BOOL,   "/sim/rendering/random-objects", false, "", 0 },
    {"enable-random-objects",        false, OPTION_BOOL,   "/sim/rendering/random-objects", true, "", 0 },
    {"disable-real-weather-fetch",   false, OPTION_BOOL,   "/environment/params/real-world-weather-fetch", false, "", 0 },
    {"enable-real-weather-fetch",    false, OPTION_BOOL,   "/environment/params/real-world-weather-fetch", true, "", 0 },
    {"disable-ai-models",            false, OPTION_BOOL,   "/sim/ai/enabled", false, "", 0 },
    {"enable-ai-models",             false, OPTION_BOOL,   "/sim/ai/enabled", true, "", 0 },
    {"disable-freeze",               false, OPTION_BOOL,   "/sim/freeze/master", false, "", 0 },
    {"enable-freeze",                false, OPTION_BOOL,   "/sim/freeze/master", true, "", 0 },
    {"disable-fuel-freeze",          false, OPTION_BOOL,   "/sim/freeze/fuel", false, "", 0 },
    {"enable-fuel-freeze",           false, OPTION_BOOL,   "/sim/freeze/fuel", true, "", 0 },
    {"disable-clock-freeze",         false, OPTION_BOOL,   "/sim/freeze/clock", false, "", 0 },
    {"enable-clock-freeze",          false, OPTION_BOOL,   "/sim/freeze/clock", true, "", 0 },
    {"disable-hud-3d",               false, OPTION_BOOL,   "/sim/hud/enable3d", false, "", 0 },
    {"enable-hud-3d",                false, OPTION_BOOL,   "/sim/hud/enable3d", true, "", 0 },
    {"disable-anti-alias-hud",       false, OPTION_BOOL,   "/sim/hud/color/antialiased", false, "", 0 },
    {"enable-anti-alias-hud",        false, OPTION_BOOL,   "/sim/hud/color/antialiased", true, "", 0 },
    {"control",                      true,  OPTION_STRING, "/sim/control-mode", false, "", 0 },
    {"disable-auto-coordination",    false, OPTION_BOOL,   "/sim/auto-coordination", false, "", 0 },
    {"enable-auto-coordination",     false, OPTION_BOOL,   "/sim/auto-coordination", true, "", 0 },
    {"browser-app",                  true,  OPTION_STRING, "/sim/startup/browser-app", false, "", 0 },
    {"disable-hud",                  false, OPTION_BOOL,   "/sim/hud/visibility", false, "", 0 },
    {"enable-hud",                   false, OPTION_BOOL,   "/sim/hud/visibility", true, "", 0 },
    {"disable-panel",                false, OPTION_BOOL,   "/sim/panel/visibility", false, "", 0 },
    {"enable-panel",                 false, OPTION_BOOL,   "/sim/panel/visibility", true, "", 0 },
    {"disable-sound",                false, OPTION_BOOL,   "/sim/sound/pause", true, "", 0 },
    {"enable-sound",                 false, OPTION_BOOL,   "/sim/sound/pause", false, "", 0 },
    {"airport",                      true,  OPTION_STRING, "/sim/presets/airport-id", false, "", 0 },
    {"airport-id",                   true,  OPTION_STRING, "/sim/presets/airport-id", false, "", 0 },
    {"runway",                       true,  OPTION_FUNC,   "", false, "", fgOptRunway },
    {"vor",                          true,  OPTION_FUNC,   "", false, "", fgOptVOR },
    {"ndb",                          true,  OPTION_FUNC,   "", false, "", fgOptNDB },
    {"carrier",                      true,  OPTION_FUNC,   "", false, "", fgOptCarrier },
    {"parkpos",                      true,  OPTION_FUNC,   "", false, "", fgOptParkpos },
    {"fix",                          true,  OPTION_FUNC,   "", false, "", fgOptFIX },
    {"offset-distance",              true,  OPTION_DOUBLE, "/sim/presets/offset-distance", false, "", 0 },
    {"offset-azimuth",               true,  OPTION_DOUBLE, "/sim/presets/offset-azimuth", false, "", 0 },
    {"lon",                          true,  OPTION_FUNC,   "", false, "", fgOptLon },
    {"lat",                          true,  OPTION_FUNC,   "", false, "", fgOptLat },
    {"altitude",                     true,  OPTION_FUNC,   "", false, "", fgOptAltitude },
    {"uBody",                        true,  OPTION_FUNC,   "", false, "", fgOptUBody },
    {"vBody",                        true,  OPTION_FUNC,   "", false, "", fgOptVBody },
    {"wBody",                        true,  OPTION_FUNC,   "", false, "", fgOptWBody },
    {"vNorth",                       true,  OPTION_FUNC,   "", false, "", fgOptVNorth },
    {"vEast",                        true,  OPTION_FUNC,   "", false, "", fgOptVEast },
    {"vDown",                        true,  OPTION_FUNC,   "", false, "", fgOptVDown },
    {"vc",                           true,  OPTION_FUNC,   "", false, "", fgOptVc },
    {"mach",                         true,  OPTION_FUNC,   "", false, "", fgOptMach },
    {"heading",                      true,  OPTION_DOUBLE, "/sim/presets/heading-deg", false, "", 0 },
    {"roll",                         true,  OPTION_DOUBLE, "/sim/presets/roll-deg", false, "", 0 },
    {"pitch",                        true,  OPTION_DOUBLE, "/sim/presets/pitch-deg", false, "", 0 },
    {"glideslope",                   true,  OPTION_DOUBLE, "/sim/presets/glideslope-deg", false, "", 0 },
    {"roc",                          true,  OPTION_FUNC,   "", false, "", fgOptRoc },
    {"fg-root",                      true,  OPTION_FUNC,   "", false, "", fgOptFgRoot },
    {"fg-scenery",                   true,  OPTION_FUNC,   "", false, "", fgOptFgScenery },
    {"fdm",                          true,  OPTION_STRING, "/sim/flight-model", false, "", 0 },
    {"aero",                         true,  OPTION_STRING, "/sim/aero", false, "", 0 },
    {"aircraft-dir",                 true,  OPTION_STRING, "/sim/aircraft-dir", false, "", 0 },
    {"model-hz",                     true,  OPTION_INT,    "/sim/model-hz", false, "", 0 },
    {"speed",                        true,  OPTION_INT,    "/sim/speed-up", false, "", 0 },
    {"trim",                         false, OPTION_BOOL,   "/sim/presets/trim", true, "", 0 },
    {"notrim",                       false, OPTION_BOOL,   "/sim/presets/trim", false, "", 0 },
    {"on-ground",                    false, OPTION_BOOL,   "/sim/presets/onground", true, "", 0 },
    {"in-air",                       false, OPTION_BOOL,   "/sim/presets/onground", false, "", 0 },
    {"fog-disable",                  false, OPTION_STRING, "/sim/rendering/fog", false, "disabled", 0 },
    {"fog-fastest",                  false, OPTION_STRING, "/sim/rendering/fog", false, "fastest", 0 },
    {"fog-nicest",                   false, OPTION_STRING, "/sim/rendering/fog", false, "nicest", 0 },
    {"disable-horizon-effect",       false, OPTION_BOOL,   "/sim/rendering/horizon-effect", false, "", 0 },
    {"enable-horizon-effect",        false, OPTION_BOOL,   "/sim/rendering/horizon-effect", true, "", 0 },
    {"disable-enhanced-lighting",    false, OPTION_BOOL,   "/sim/rendering/enhanced-lighting", false, "", 0 },
    {"enable-enhanced-lighting",     false, OPTION_BOOL,   "/sim/rendering/enhanced-lighting", true, "", 0 },
    {"disable-distance-attenuation", false, OPTION_BOOL,   "/sim/rendering/distance-attenuation", false, "", 0 },
    {"enable-distance-attenuation",  false, OPTION_BOOL,   "/sim/rendering/distance-attenuation", true, "", 0 },
    {"disable-specular-highlight",   false, OPTION_BOOL,   "/sim/rendering/specular-highlight", false, "", 0 },
    {"enable-specular-highlight",    false, OPTION_BOOL,   "/sim/rendering/specular-highlight", true, "", 0 },
    {"disable-clouds",               false, OPTION_BOOL,   "/environment/clouds/status", false, "", 0 },
    {"enable-clouds",                false, OPTION_BOOL,   "/environment/clouds/status", true, "", 0 },
    {"disable-clouds3d",             false, OPTION_BOOL,   "/sim/rendering/clouds3d-enable", false, "", 0 },
    {"enable-clouds3d",              false, OPTION_BOOL,   "/sim/rendering/clouds3d-enable", true, "", 0 },
    {"fov",                          true,  OPTION_FUNC,   "", false, "", fgOptFov },
    {"aspect-ratio-multiplier",      true,  OPTION_DOUBLE, "/sim/current-view/aspect-ratio-multiplier", false, "", 0 },
    {"disable-fullscreen",           false, OPTION_BOOL,   "/sim/startup/fullscreen", false, "", 0 },
    {"enable-fullscreen",            false, OPTION_BOOL,   "/sim/startup/fullscreen", true, "", 0 },
    {"disable-save-on-exit",         false, OPTION_BOOL,   "/sim/startup/save-on-exit", false, "", 0 },
    {"enable-save-on-exit",          false, OPTION_BOOL,   "/sim/startup/save-on-exit", true, "", 0 },
    {"shading-flat",                 false, OPTION_BOOL,   "/sim/rendering/shading", false, "", 0 },
    {"shading-smooth",               false, OPTION_BOOL,   "/sim/rendering/shading", true, "", 0 },
    {"disable-skyblend",             false, OPTION_BOOL,   "/sim/rendering/skyblend", false, "", 0 },
    {"enable-skyblend",              false, OPTION_BOOL,   "/sim/rendering/skyblend", true, "", 0 },
    {"disable-textures",             false, OPTION_BOOL,   "/sim/rendering/textures", false, "", 0 },
    {"enable-textures",              false, OPTION_BOOL,   "/sim/rendering/textures", true, "", 0 },
    {"disable-wireframe",            false, OPTION_BOOL,   "/sim/rendering/wireframe", false, "", 0 },
    {"enable-wireframe",             false, OPTION_BOOL,   "/sim/rendering/wireframe", true, "", 0 },
    {"geometry",                     true,  OPTION_FUNC,   "", false, "", fgOptGeometry },
    {"bpp",                          true,  OPTION_FUNC,   "", false, "", fgOptBpp },
    {"units-feet",                   false, OPTION_STRING, "/sim/startup/units", false, "feet", 0 },
    {"units-meters",                 false, OPTION_STRING, "/sim/startup/units", false, "meters", 0 },
    {"timeofday",                    true,  OPTION_STRING, "/sim/startup/time-offset-type", false, "noon", 0 },
    {"season",                       true,  OPTION_STRING, "/sim/startup/season", false, "summer", 0 },
    {"time-offset",                  true,  OPTION_FUNC,   "", false, "", fgOptTimeOffset },
    {"time-match-real",              false, OPTION_STRING, "/sim/startup/time-offset-type", false, "system-offset", 0 },
    {"time-match-local",             false, OPTION_STRING, "/sim/startup/time-offset-type", false, "latitude-offset", 0 },
    {"start-date-sys",               true,  OPTION_FUNC,   "", false, "", fgOptStartDateSys },
    {"start-date-lat",               true,  OPTION_FUNC,   "", false, "", fgOptStartDateLat },
    {"start-date-gmt",               true,  OPTION_FUNC,   "", false, "", fgOptStartDateGmt },
    {"hud-tris",                     false, OPTION_STRING, "/sim/hud/frame-stat-type", false, "tris", 0 },
    {"hud-culled",                   false, OPTION_STRING, "/sim/hud/frame-stat-type", false, "culled", 0 },
    {"atcsim",                       true,  OPTION_CHANNEL, "", false, "dummy", 0 },
    {"atlas",                        true,  OPTION_CHANNEL, "", false, "", 0 },
    {"httpd",                        true,  OPTION_CHANNEL, "", false, "", 0 },
#ifdef FG_JPEG_SERVER
    {"jpg-httpd",                    true,  OPTION_CHANNEL, "", false, "", 0 },
#endif
    {"native",                       true,  OPTION_CHANNEL, "", false, "", 0 },
    {"native-ctrls",                 true,  OPTION_CHANNEL, "", false, "", 0 },
    {"native-fdm",                   true,  OPTION_CHANNEL, "", false, "", 0 },
    {"native-gui",                   true,  OPTION_CHANNEL, "", false, "", 0 },
    {"opengc",                       true,  OPTION_CHANNEL, "", false, "", 0 },
    {"AV400",                        true,  OPTION_CHANNEL, "", false, "", 0 },
    {"garmin",                       true,  OPTION_CHANNEL, "", false, "", 0 },
    {"nmea",                         true,  OPTION_CHANNEL, "", false, "", 0 },
    {"generic",                      true,  OPTION_CHANNEL, "", false, "", 0 },
    {"props",                        true,  OPTION_CHANNEL, "", false, "", 0 },
    {"telnet",                       true,  OPTION_CHANNEL, "", false, "", 0 },
    {"pve",                          true,  OPTION_CHANNEL, "", false, "", 0 },
    {"ray",                          true,  OPTION_CHANNEL, "", false, "", 0 },
    {"rul",                          true,  OPTION_CHANNEL, "", false, "", 0 },
    {"joyclient",                    true,  OPTION_CHANNEL, "", false, "", 0 },
    {"jsclient",                     true,  OPTION_CHANNEL, "", false, "", 0 },
    {"proxy",                        true,  OPTION_FUNC,    "", false, "", fgSetupProxy },
    {"callsign",                     true, OPTION_STRING,  "sim/multiplay/callsign", false, "", 0 },
    {"multiplay",                    true,  OPTION_CHANNEL, "", false, "", 0 },
    {"trace-read",                   true,  OPTION_FUNC,   "", false, "", fgOptTraceRead },
    {"trace-write",                  true,  OPTION_FUNC,   "", false, "", fgOptTraceWrite },
    {"log-level",                    true,  OPTION_FUNC,   "", false, "", fgOptLogLevel },
    {"view-offset",                  true,  OPTION_FUNC,   "", false, "", fgOptViewOffset },
    {"visibility",                   true,  OPTION_FUNC,   "", false, "", fgOptVisibilityMeters },
    {"visibility-miles",             true,  OPTION_FUNC,   "", false, "", fgOptVisibilityMiles },
    {"random-wind",                  false, OPTION_FUNC,   "", false, "", fgOptRandomWind },
    {"wind",                         true,  OPTION_FUNC,   "", false, "", fgOptWind },
    {"turbulence",                   true,  OPTION_FUNC,   "", false, "", fgOptTurbulence },
    {"ceiling",                      true,  OPTION_FUNC,   "", false, "", fgOptCeiling },
    {"wp",                           true,  OPTION_FUNC,   "", false, "", fgOptWp },
    {"flight-plan",                  true,  OPTION_FUNC,   "", false, "", fgOptFlightPlan },
    {"config",                       true,  OPTION_FUNC,   "", false, "", fgOptConfig },
    {"aircraft",                     true,  OPTION_STRING, "/sim/aircraft", false, "", 0 },
    {"failure",                      true,  OPTION_FUNC,   "", false, "", fgOptFailure },
    {"com1",                         true,  OPTION_DOUBLE, "/instrumentation/comm[0]/frequencies/selected-mhz", false, "", 0 },
    {"com2",                         true,  OPTION_DOUBLE, "/instrumentation/comm[1]/frequencies/selected-mhz", false, "", 0 },
    {"nav1",                         true,  OPTION_FUNC,   "", false, "", fgOptNAV1 },
    {"nav2",                         true,  OPTION_FUNC,   "", false, "", fgOptNAV2 },
    {"adf",                          true,  OPTION_FUNC,   "", false, "", fgOptADF },
    {"dme",                          true,  OPTION_FUNC,   "", false, "", fgOptDME },
    {"min-status",                   true,  OPTION_STRING,  "/sim/aircraft-min-status", false, "all", 0 },
    {"livery",                       true,  OPTION_FUNC,   "", false, "", fgOptLivery },
    {"ai-scenario",                  true,  OPTION_FUNC,   "", false, "", fgOptScenario },
    {0}
};


// Set a property for the --prop: option. Syntax: --prop:[<type>:]<name>=<value>
// <type> can be "double" etc. but also only the first letter "d".
// Examples:  --prop:alpha=1  --prop:bool:beta=true  --prop:d:gamma=0.123
static bool
set_property(const string& arg)
{
    string::size_type pos = arg.find('=');
    if (pos == arg.npos || pos == 0 || pos + 1 == arg.size())
        return false;

    string name = arg.substr(0, pos);
    string value = arg.substr(pos + 1);
    string type = "string";
    pos = name.find(':');
    if (pos != name.npos && pos != 0 && pos + 1 != name.size()) {
        type = name.substr(0, pos);
        name = name.substr(pos + 1);
    }
    if (type == "s" || type == "string")
        ;
    else if (type == "d" || type == "double")
        fgSetDouble(name.c_str(), 0.0);
    else if (type == "f" || type == "float")
        fgSetFloat(name.c_str(), 0.0f);
    else if (type == "l" || type == "long")
        fgSetLong(name.c_str(), 0L);
    else if (type == "i" || type == "int")
        fgSetInt(name.c_str(), 0);
    else if (type == "b" || type == "bool")
        fgSetBool(name.c_str(), false);
    else
        return false;

    fgSetString(name.c_str(), value.c_str());
    return true;
}


// Parse a single option
static int
parse_option (const string& arg)
{
    if ( fgOptionMap.size() == 0 ) {
        size_t i = 0;
        OptionDesc *pt = &fgOptionArray[ 0 ];
        while ( pt->option != 0 ) {
            fgOptionMap[ pt->option ] = i;
            i += 1;
            pt += 1;
        }
    }

    // General Options
    if ( (arg == "--help") || (arg == "-h") ) {
	// help/usage request
	return(FG_OPTIONS_HELP);
    } else if ( (arg == "--verbose") || (arg == "-v") ) {
        // verbose help/usage request
        return(FG_OPTIONS_VERBOSE_HELP);
    } else if ( arg.find( "--show-aircraft") == 0) {
        return(FG_OPTIONS_SHOW_AIRCRAFT);
    } else if ( arg.find( "--prop:" ) == 0 ) {
        if (!set_property(arg.substr(7))) {
            SG_LOG( SG_GENERAL, SG_ALERT, "Bad property assignment: " << arg );
            return FG_OPTIONS_ERROR;
        }
    } else if ( arg.find( "--" ) == 0 ) {
        size_t pos = arg.find( '=' );
        string arg_name;
        if ( pos == string::npos ) {
            arg_name = arg.substr( 2 );
        } else {
            arg_name = arg.substr( 2, pos - 2 );
        }
        map<string,size_t>::iterator it = fgOptionMap.find( arg_name );
        if ( it != fgOptionMap.end() ) {
            OptionDesc *pt = &fgOptionArray[ it->second ];
            switch ( pt->type ) {
                case OPTION_BOOL:
                    fgSetBool( pt->property, pt->b_param );
                    break;
                case OPTION_STRING:
                    if ( pt->has_param && pos != string::npos && pos + 1 < arg.size() ) {
                        fgSetString( pt->property, arg.substr( pos + 1 ).c_str() );
                    } else if ( !pt->has_param && pos == string::npos ) {
                        fgSetString( pt->property, pt->s_param );
                    } else if ( pt->has_param ) {
                        SG_LOG( SG_GENERAL, SG_ALERT, "Option '" << arg << "' needs a parameter" );
                        return FG_OPTIONS_ERROR;
                    } else {
                        SG_LOG( SG_GENERAL, SG_ALERT, "Option '" << arg << "' does not have a parameter" );
                        return FG_OPTIONS_ERROR;
                    }
                    break;
                case OPTION_DOUBLE:
                    if ( pos != string::npos && pos + 1 < arg.size() ) {
                        fgSetDouble( pt->property, atof( arg.substr( pos + 1 ) ) );
                    } else {
                        SG_LOG( SG_GENERAL, SG_ALERT, "Option '" << arg << "' needs a parameter" );
                        return FG_OPTIONS_ERROR;
                    }
                    break;
                case OPTION_INT:
                    if ( pos != string::npos && pos + 1 < arg.size() ) {
                        fgSetInt( pt->property, atoi( arg.substr( pos + 1 ) ) );
                    } else {
                        SG_LOG( SG_GENERAL, SG_ALERT, "Option '" << arg << "' needs a parameter" );
                        return FG_OPTIONS_ERROR;
                    }
                    break;
                case OPTION_CHANNEL:
                    if ( pt->has_param && pos != string::npos && pos + 1 < arg.size() ) {
                        add_channel( pt->option, arg.substr( pos + 1 ) );
                    } else if ( !pt->has_param && pos == string::npos ) {
                        add_channel( pt->option, pt->s_param );
                    } else if ( pt->has_param ) {
                        SG_LOG( SG_GENERAL, SG_ALERT, "Option '" << arg << "' needs a parameter" );
                        return FG_OPTIONS_ERROR;
                    } else {
                        SG_LOG( SG_GENERAL, SG_ALERT, "Option '" << arg << "' does not have a parameter" );
                        return FG_OPTIONS_ERROR;
                    }
                    break;
                case OPTION_FUNC:
                    if ( pt->has_param && pos != string::npos && pos + 1 < arg.size() ) {
                        return pt->func( arg.substr( pos + 1 ).c_str() );
                    } else if ( !pt->has_param && pos == string::npos ) {
                        return pt->func( 0 );
                    } else if ( pt->has_param ) {
                        SG_LOG( SG_GENERAL, SG_ALERT, "Option '" << arg << "' needs a parameter" );
                        return FG_OPTIONS_ERROR;
                    } else {
                        SG_LOG( SG_GENERAL, SG_ALERT, "Option '" << arg << "' does not have a parameter" );
                        return FG_OPTIONS_ERROR;
                    }
                    break;
            }
        } else {
            SG_LOG( SG_GENERAL, SG_ALERT, "Unknown option '" << arg << "'" );
            return FG_OPTIONS_ERROR;
        }
    } else {
        SG_LOG( SG_GENERAL, SG_ALERT, "Unknown option '" << arg << "'" );
        return FG_OPTIONS_ERROR;
    }

    return FG_OPTIONS_OK;
}


// Parse the command line options
void
fgParseArgs (int argc, char **argv)
{
    bool in_options = true;
    bool verbose = false;
    bool help = false;

    SG_LOG(SG_GENERAL, SG_INFO, "Processing command line arguments");

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];

	if (in_options && (arg.find('-') == 0)) {
	  if (arg == "--") {
	    in_options = false;
	  } else {
	    int result = parse_option(arg);
	    if ((result == FG_OPTIONS_HELP) || (result == FG_OPTIONS_ERROR))
              help = true;

            else if (result == FG_OPTIONS_VERBOSE_HELP)
              verbose = true;

            else if (result == FG_OPTIONS_SHOW_AIRCRAFT) {
               fgOptLogLevel( "alert" );
               SGPath path( globals->get_fg_root() );
               path.append("Aircraft");
               fgShowAircraft(path, true);
               exit(0);
            }
	  }
	} else {
	  in_options = false;
	  SG_LOG(SG_GENERAL, SG_INFO,
		 "Reading command-line property file " << arg);
	  readProperties(arg, globals->get_props());
	}
    }

    if (help) {
       fgOptLogLevel( "alert" );
       fgUsage(verbose);
       exit(0);
    }

    SG_LOG(SG_GENERAL, SG_INFO, "Finished command line arguments");
}


// Parse config file options
void
fgParseOptions (const string& path) {
    sg_gzifstream in( path );
    if ( !in.is_open() ) {
        return;
    }

    SG_LOG( SG_GENERAL, SG_INFO, "Processing config file: " << path );

    in >> skipcomment;
#ifndef __MWERKS__
    while ( ! in.eof() ) {
#else
    char c = '\0';
    while ( in.get(c) && c != '\0' ) {
	in.putback(c);
#endif
	string line;

#if defined( macintosh )
        getline( in, line, '\r' );
#else
	getline( in, line, '\n' );
#endif

        // catch extraneous (DOS) line ending character
        int i;
        for (i = line.length(); i > 0; i--)
            if (line[i - 1] > 32)
                break;
        line = line.substr( 0, i );

	if ( parse_option( line ) == FG_OPTIONS_ERROR ) {
            cerr << endl << "Config file parse error: " << path << " '" 
		    << line << "'" << endl;
	    fgUsage();
	    exit(-1);
	}
	in >> skipcomment;
    }
}


// Print usage message
void 
fgUsage (bool verbose)
{
    SGPropertyNode *locale = globals->get_locale();

    SGPropertyNode options_root;

    SG_LOG( SG_GENERAL, SG_ALERT, "" ); // To popup the console on Windows
    cout << endl;

    try {
        fgLoadProps("options.xml", &options_root);
    } catch (const sg_exception &) {
        cout << "Unable to read the help file." << endl;
        cout << "Make sure the file options.xml is located in the FlightGear base directory," << endl;
        cout << "and the location of the base directory is specified by setting $FG_ROOT or" << endl;
        cout << "by adding --fg-root=path as a program argument." << endl;
        
        exit(-1);
    }

    SGPropertyNode *options = options_root.getNode("options");
    if (!options) {
        SG_LOG( SG_GENERAL, SG_ALERT,
                "Error reading options.xml: <options> directive not found." );
        exit(-1);
    }

    SGPropertyNode *usage = locale->getNode(options->getStringValue("usage"));
    if (usage) {
        cout << "Usage: " << usage->getStringValue() << endl;
    }

    vector<SGPropertyNode_ptr>section = options->getChildren("section");
    for (unsigned int j = 0; j < section.size(); j++) {
        string msg = "";

        vector<SGPropertyNode_ptr>option = section[j]->getChildren("option");
        for (unsigned int k = 0; k < option.size(); k++) {

            SGPropertyNode *name = option[k]->getNode("name");
            SGPropertyNode *short_name = option[k]->getNode("short");
            SGPropertyNode *key = option[k]->getNode("key");
            SGPropertyNode *arg = option[k]->getNode("arg");
            bool brief = option[k]->getNode("brief") != 0;

            if ((brief || verbose) && name) {
                string tmp = name->getStringValue();

                if (key){
                    tmp.append(":");
                    tmp.append(key->getStringValue());
                }
                if (arg) {
                    tmp.append("=");
                    tmp.append(arg->getStringValue());
                }
                if (short_name) {
                    tmp.append(", -");
                    tmp.append(short_name->getStringValue());
                }

                char cstr[96];
                if (tmp.size() <= 25) {
                    snprintf(cstr, 96, "   --%-27s", tmp.c_str());
                } else {
                    snprintf(cstr, 96, "\n   --%s\n%32c", tmp.c_str(), ' ');
                }

                // There may be more than one <description> tag assosiated
                // with one option

                msg += cstr;
                vector<SGPropertyNode_ptr>desc =
                                          option[k]->getChildren("description");

                if (desc.size() > 0) {
                   for ( unsigned int l = 0; l < desc.size(); l++) {

                      // There may be more than one translation line.

                      string t = desc[l]->getStringValue();
                      SGPropertyNode *n = locale->getNode("strings");
                      vector<SGPropertyNode_ptr>trans_desc =
                               n->getChildren(t.substr(8).c_str());

                      for ( unsigned int m = 0; m < trans_desc.size(); m++ ) {
                         string t_str = trans_desc[m]->getStringValue();

                         if ((m > 0) || ((l > 0) && m == 0)) {
                            snprintf(cstr, 96, "%32c", ' ');
                            msg += cstr;

                         }

                         // If the string is too large to fit on the screen,
                         // then split it up in several pieces.

                         while ( t_str.size() > 47 ) {

                            unsigned int m = t_str.rfind(' ', 47);
                            msg += t_str.substr(0, m);
                            snprintf(cstr, 96, "\n%32c", ' ');
                            msg += cstr;

                            t_str.erase(t_str.begin(), t_str.begin() + m + 1);
                        }
                        msg += t_str + '\n';
                     }
                  }
               }
            }
        }

        SGPropertyNode *name =
                            locale->getNode(section[j]->getStringValue("name"));

        if (!msg.empty() && name) {
           cout << endl << name->getStringValue() << ":" << endl;
           cout << msg;
           msg.erase();
        }
    }

    if ( !verbose ) {
        cout << endl;
        cout << "For a complete list of options use --help --verbose" << endl;
    }
#ifdef _MSC_VER
    cout << "Hit a key to continue..." << endl;
    cin.get();
#endif
}

// A simple function to return an integer depending on the position
// of the status string within the array in order to determine the hierarchy.
unsigned int getNumMaturity(const char * str) 
{
  // changes should also be reflected in $FG_ROOT/data/options.xml & 
  // $FG_ROOT/data/Translations/string-default.xml
  const char levels[][20]= {"alpha","beta","early-production","production",0}; 

  for (unsigned int i=0; i<(sizeof(levels)/sizeof(levels[0])-1);i++) 
    if (strcmp(str,levels[i])==0)
      return i;

  return 0;
};


static void fgSearchAircraft(const SGPath &path, string_list &aircraft,
                             bool recursive)
{   
   
   ulDirEnt* dire;
   ulDir *dirp = ulOpenDir(path.str().c_str());
   if (dirp == NULL) {
      cerr << "Unable to open aircraft directory '" << path.str() << '\'' << endl;
      exit(-1);
   }

   while ((dire = ulReadDir(dirp)) != NULL) {
      char *ptr;

      if (dire->d_isdir) {
          if (recursive && strcmp("CVS", dire->d_name)
              && strcmp(".", dire->d_name) && strcmp("..", dire->d_name))
          {
              SGPath next = path;
              next.append(dire->d_name);

              fgSearchAircraft(next, aircraft, true);
          }
      } else if ((ptr = strstr(dire->d_name, "-set.xml")) && (ptr[8] == '\0')) {

          SGPath afile = path;
          afile.append(dire->d_name);

          *ptr = '\0';

          SGPropertyNode root;
          try {
             readProperties(afile.str(), &root);
          } catch (...) {
             continue;
          }

          SGPropertyNode *desc = NULL;
	  SGPropertyNode *status = NULL;
	  
          SGPropertyNode *node = root.getNode("sim");
          if (node) {
             desc = node->getNode("description");
	     // if a status tag is found, read it in
	     if (node->hasValue("status"))
	     	status = node->getNode("status");
          }

          char cstr[96];
	  //additionally display status information where it is available
          
          if (strlen(dire->d_name) <= 27) {
             snprintf(cstr, 96, "   %-27s  %s", dire->d_name,
                      (desc) ? desc->getStringValue() : "");

          } else {
             snprintf(cstr, 96, "   %-27s\n%32c%s", dire->d_name, ' ',
                      (desc) ? desc->getStringValue() : "");
          }

	  SGPropertyNode * required_status
                             = fgGetNode ("/sim/aircraft-min-status", true);
	  
	  // If the node holds the value "all", then there wasn't any status 
	  // level specified, so we simply go ahead and output ALL aircraft
	  if (strcmp(required_status->getStringValue(),"all")==0) {	   
		  aircraft.push_back(cstr);
		  }
	  else
	  {
	  // If the node doesn't hold "all" as its value, then we are supposed
	  // to show only aircraft meeting specific status (development status)
          // requirements:
 
     if (node->hasValue("status"))     {
     	  //Compare (minimally) required status level with actual aircraft status:
	   if (	getNumMaturity(status->getStringValue() ) >= 
	  	getNumMaturity(required_status->getStringValue() ) )
				  aircraft.push_back(cstr); }
	  			  		  
	  }
      
   
   }
   }

   ulCloseDir(dirp);
}

/*
 * Search in the current directory, and in on directory deeper
 * for <aircraft>-set.xml configuration files and show the aircaft name
 * and the contents of the<description> tag in a sorted manner.
 *
 * @parampath the directory to search for configuration files
 * @param recursive defines whether the directory should be searched recursively
 */
void fgShowAircraft(const SGPath &path, bool recursive) {
    string_list aircraft;

    fgSearchAircraft( path, aircraft, recursive );

    sort(aircraft.begin(), aircraft.end());
    SG_LOG( SG_GENERAL, SG_ALERT, "" ); // To popup the console on Windows
    cout << "Available aircraft:" << endl;
    for ( unsigned int i = 0; i < aircraft.size(); i++ ) {
        cout << aircraft[i] << endl;
    }
#ifdef _MSC_VER
    cout << "Hit a key to continue..." << endl;
    cin.get();
#endif
}
