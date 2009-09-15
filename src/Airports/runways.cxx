// runways.cxx -- a simple class to manage airport runway info
//
// Written by Curtis Olson, started August 2000.
//
// Copyright (C) 2000  Curtis L. Olson  - http://www.flightgear.org/~curt
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

#include <math.h>               // fabs()
#include <stdio.h>              // sprintf()

#include <simgear/compiler.h>
#include <simgear/debug/logstream.hxx>
#include <Main/fg_props.hxx>

#include STL_STRING
#include <map>

#include "runways.hxx"

SG_USING_NAMESPACE(std);
SG_USING_STD(istream);
SG_USING_STD(multimap);


// add an entry to the list
void FGRunwayList::add( const string& id, const string& rwy_no,
                        const double longitude, const double latitude,
                        const double heading, const double length,
                        const double width,
                        const double displ_thresh1, const double displ_thresh2,
                        const double stopway1, const double stopway2,
                        const string& lighting_flags, const int surface_code,
                        const string& shoulder_code, const int marking_code,
                        const double smoothness, const bool dist_remaining )
{
    FGRunway rwy;

    rwy._id = id;
    rwy._rwy_no = rwy_no;
    // strip trailing "x" if it exists in runway number
    string tmp = rwy._rwy_no.substr(2, 1);
    if ( tmp == "x" ) {
        rwy._rwy_no = rwy._rwy_no.substr(0, 2);
    }

    rwy._lon = longitude;
    rwy._lat = latitude;
    rwy._heading = heading;
    rwy._length = length;
    rwy._width = width;
    rwy._displ_thresh1 = displ_thresh1;
    rwy._displ_thresh2 = displ_thresh2;
    rwy._stopway1 = stopway1;
    rwy._stopway2 = stopway2;

    rwy._lighting_flags = lighting_flags;
    rwy._surface_code = surface_code;
    rwy._shoulder_code = shoulder_code;
    rwy._marking_code = marking_code;
    rwy._smoothness = smoothness;
    rwy._dist_remaining = dist_remaining;

    if ( rwy_no == "xxx" ) {
        rwy._type = "taxiway";
        // don't insert taxiways into the DB for now
    } else {
        rwy._type = "runway";
        runways.insert(pair<const string, FGRunway>(rwy._id, rwy));
    }
}


// Return reverse rwy number
// eg 01 -> 19
// 03L -> 21R
static string GetReverseRunwayNo(string& rwyno) {	
    // cout << "Original rwyno = " << rwyNo << '\n';
    
    // Helipads don't have a seperate number per end
    if(rwyno.size() && (rwyno[0] == 'H' || rwyno[0] == 'h')) {
	return rwyno;
    }
    
    // standardize input number
    string tmp = rwyno.substr(1, 1);
    if (( tmp == "L" || tmp == "R" || tmp == "C" ) || (rwyno.size() == 1)) {
	tmp = rwyno;
	rwyno = "0" + tmp;
        SG_LOG( SG_GENERAL, SG_INFO,
                "Standardising rwy number from " << tmp << " to " << rwyno );
    }
    
    char buf[4];
    int rn = atoi(rwyno.substr(0,2).c_str());
    rn += 18;
    while(rn > 36) {
	rn -= 36;
    }
    sprintf(buf, "%02i", rn);
    if(rwyno.size() == 3) {
	if(rwyno.substr(2,1) == "L") {
	    buf[2] = 'R';
	    buf[3] = '\0';
	} else if (rwyno.substr(2,1) == "R") {
	    buf[2] = 'L';
	    buf[3] = '\0';
	} else if (rwyno.substr(2,1) == "C") {
	    buf[2] = 'C';
	    buf[3] = '\0';
	} else if (rwyno.substr(2,1) == "T") {
	    buf[2] = 'T';
	    buf[3] = '\0';
	} else {
	    SG_LOG(SG_GENERAL, SG_ALERT, "Unknown runway code "
	    << rwyno << " passed to GetReverseRunwayNo(...)");
	}
    }
    return(buf);
}


// search for the specified apt id (wierd!)
bool FGRunwayList::search( const string& aptid, FGRunway* r ) {
    runway_map_iterator pos;

    pos = runways.lower_bound(aptid);
    if ( pos != runways.end() ) {
        current = pos;
        *r = pos->second;
        return true;
    } else {
        return false;
    }
}


// search for the specified apt id and runway no
bool FGRunwayList::search( const string& aptid, const string& rwyno,
                           FGRunway *r )
{
    string revrwyno = "";
    string runwayno = rwyno;
    if ( runwayno.length() ) {
        // standardize input number
        string tmp = runwayno.substr(1, 1);
        if (( tmp == "L" || tmp == "R" || tmp == "C" )
            || (runwayno.size() == 1))
        {
            tmp = runwayno;
            runwayno = "0" + tmp;
            SG_LOG( SG_GENERAL, SG_INFO, "Standardising rwy number from "
                    << tmp << " to " << runwayno );
        }
        revrwyno = GetReverseRunwayNo(runwayno);
    }
    runway_map_iterator pos;
    for ( pos = runways.lower_bound( aptid );
          pos != runways.upper_bound( aptid ); ++pos)
    {
        if ( pos->second._rwy_no == runwayno ) {
            current = pos;
            *r = pos->second;
            return true;
        } else if ( pos->second._rwy_no == revrwyno ) {
            // Search again with the other-end runway number.
            // Remember we have to munge the heading and rwy_no
            // results if this one matches
            current = pos;
            *r = pos->second;
            // NOTE - matching revrwyno implies that runwayno was
            // actually correct.
            r->_rwy_no = runwayno;
            r->_heading += 180.0;
            return true;
        }
    }

    return false;
}


// (wierd!)
FGRunway FGRunwayList::search( const string& aptid ) {
    FGRunway a;
    search( aptid, &a );
    return a;
}


// Return the runway closest to a given heading
bool FGRunwayList::search( const string& aptid, const int tgt_hdg,
                           FGRunway *runway )
{
    string rwyNo = search(aptid, tgt_hdg);
    return(rwyNo == "NN" ? false : search(aptid, rwyNo, runway));
}


// Return the runway number of the runway closest to a given heading
string FGRunwayList::search( const string& aptid, const int hdg ) {
    //SG_LOG(SG_GENERAL, SG_ALERT, "searching runway for " << aptid
    //        << " with target heading " << hdg);

    FGRunway r;
    if (!search(aptid, &r)) {
        SG_LOG(SG_GENERAL, SG_ALERT, "Failed to find "
                << aptid << " in database.");
        return "NN";
    }

    SGPropertyNode *param = fgGetNode("/sim/airport/runways/search", true);
    double lenwgt = param->getDoubleValue("length-weight", 0.01);
    double widwgt = param->getDoubleValue("width-weight", 0.01);
    double surfwgt = param->getDoubleValue("surface-weight", 10);
    double devwgt = param->getDoubleValue("deviation-weight", 1);

    FGRunway best;
    double max = 0.0;
    bool reversed = false;

    do {
        if (r._id != aptid)
            break;

        int surface = 1;
        if (r._surface_code == 12 || r._surface_code == 5) // dry lakebed & gravel
            surface = 2;
        else if (r._surface_code == 1 || r._surface_code == 2) // asphalt & concrete
            surface = 3;

        double quality, bad, diff;
        double good = lenwgt * r._length + widwgt * r._width + surfwgt * surface + 1e-20;

        // this side
        diff = hdg - r._heading;
        while (diff < -180)
            diff += 360;
        while (diff >= 180)
            diff -= 360;
        bad = fabs(devwgt * diff) + 1e-20;

        quality = good / bad;
        //SG_LOG(SG_GENERAL, SG_ALERT, "  runway " << r._rwy_no <<  " -> " << quality);
        if (quality > max) {
            max = quality;
            best = r;
            reversed = false;
        }

        // other side
        diff = hdg - r._heading - 180;
        while (diff < -180)
            diff += 360;
        while (diff >= 180)
            diff -= 360;
        bad = fabs(devwgt * diff) + 1e-20;

        quality = good / bad;
        //SG_LOG(SG_GENERAL, SG_ALERT, "  runway " << GetReverseRunwayNo(r._rwy_no)
        //        <<  " -> " << quality);
        if (quality > max) {
            max = quality;
            best = r;
            reversed = true;
        }

    } while (next(&r));

    return reversed ? GetReverseRunwayNo(best._rwy_no) : best._rwy_no;
}


bool FGRunwayList::next( FGRunway* runway ) {
    ++current;
    if ( current != runways.end() ) {
        *runway = current->second;
        return true;
    } else {
        return false;
    }
}


FGRunway FGRunwayList::next() {
    FGRunway result;

    ++current;
    if ( current != runways.end() ) {
        result = current->second;
    }

    return result;
}


// Destructor
FGRunwayList::~FGRunwayList( void ) {
}
