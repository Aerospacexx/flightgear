// replay.hxx - a system to record and replay FlightGear flights
//
// Written by Curtis Olson, started Juley 2003.
//
// Copyright (C) 2003  Curtis L. Olson  - http://www.flightgear.org/~curt
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


#ifndef _FG_REPLAY_HXX
#define _FG_REPLAY_HXX 1

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <simgear/compiler.h>

#include <deque>

#include <simgear/math/sg_types.hxx>
#include <simgear/props/props.hxx>
#include <simgear/structure/subsystem_mgr.hxx>

#include <Network/net_ctrls.hxx>
#include <Network/net_fdm.hxx>

SG_USING_STD(deque);


class FGReplayData {

public:

    double sim_time;
    FGNetFDM fdm;
    FGNetCtrls ctrls;
};

typedef deque < FGReplayData *> replay_list_type;



/**
 * A recording/replay module for FlightGear flights
 * 
 */

class FGReplay : public SGSubsystem
{

public:

    FGReplay ();
    virtual ~FGReplay();

    virtual void init();
    virtual void bind();
    virtual void unbind();
    virtual void update( double dt );
    //virtual void printTimingInformation();

    //void stamp(string name);
    void replay( double time );
    double get_start_time();
    double get_end_time();
    
private:
    //eventTimeVec timingInfo;

    static const double st_list_time;   // 60 secs of high res data
    static const double mt_list_time;  // 10 mins of 1 fps data
    static const double lt_list_time; // 1 hr of 10 spf data

    // short term sample rate is as every frame
    static const double mt_dt; // medium term sample rate (sec)
    static const double lt_dt; // long term sample rate (sec)

    double sim_time;
    double last_mt_time;
    double last_lt_time;

    replay_list_type short_term;
    replay_list_type medium_term;
    replay_list_type long_term;
    replay_list_type recycler;
    SGPropertyNode_ptr disable_replay;
};


#endif // _FG_REPLAY_HXX
