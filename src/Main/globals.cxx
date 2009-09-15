// globals.cxx -- Global state that needs to be shared among the sim modules
//
// Written by Curtis Olson, started July 2000.
//
// Copyright (C) 2000  Curtis L. Olson - http://www.flightgear.org/~curt
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

#include <simgear/scene/model/modellib.hxx>
#include <simgear/sound/soundmgr_openal.hxx>
#include <simgear/structure/commands.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/timing/sg_time.hxx>
#include <simgear/ephemeris/ephemeris.hxx>
#include <simgear/magvar/magvar.hxx>
#include <simgear/scene/material/matlib.hxx>

#include <Aircraft/controls.hxx>
#include <Airports/runways.hxx>
#include <ATC/AIMgr.hxx>
#include <ATC/ATCmgr.hxx>
#include <ATC/ATCdisplay.hxx>
#include <Autopilot/route_mgr.hxx>
#include <Cockpit/panel.hxx>
#include <GUI/new_gui.hxx>
#include <Model/acmodel.hxx>
#include <Model/modelmgr.hxx>
#include <MultiPlayer/multiplaymgr.hxx>
#include <Navaids/awynet.hxx>
#include <Scenery/scenery.hxx>
#include <Scenery/tilemgr.hxx>

#include "globals.hxx"
#include "renderer.hxx"
#include "viewmgr.hxx"

#include "fg_props.hxx"
#include "fg_io.hxx"


////////////////////////////////////////////////////////////////////////
// Implementation of FGGlobals.
////////////////////////////////////////////////////////////////////////

// global global :-)
FGGlobals *globals;


// Constructor
FGGlobals::FGGlobals() :
    renderer( new FGRenderer ),
    subsystem_mgr( new SGSubsystemMgr ),
    event_mgr( new SGEventMgr ),
    sim_time_sec( 0.0 ),
    fg_root( "" ),
#if defined(FX) && defined(XMESA)
    fullscreen( true ),
#endif
    warp( 0 ),
    warp_delta( 0 ),
    time_params( NULL ),
    ephem( NULL ),
    mag( NULL ),
    route_mgr( NULL ),
    current_panel( NULL ),
    soundmgr( NULL ),
    airports( NULL ),
    ATC_mgr( NULL ),
    ATC_display( NULL ),
    AI_mgr( NULL ),
    controls( NULL ),
    viewmgr( NULL ),
    props( new SGPropertyNode ),
    initial_state( NULL ),
    locale( NULL ),
    commands( new SGCommandMgr ),
    model_lib( NULL ),
    acmodel( NULL ),
    model_mgr( NULL ),
    channel_options_list( NULL ),
    initial_waypoints( NULL ),
    scenery( NULL ),
    tile_mgr( NULL ),
    io( new FGIO ),
    fontcache ( new FGFontCache ),
    navlist( NULL ),
    loclist( NULL ),
    gslist( NULL ),
    dmelist( NULL ),
    mkrlist( NULL ),
    tacanlist( NULL ),
    carrierlist( NULL ), 
    fixlist( NULL )
{
}


// Destructor
FGGlobals::~FGGlobals() 
{
    delete renderer;
    delete subsystem_mgr;
    delete event_mgr;
    delete time_params;
    delete ephem;
    delete mag;
    delete matlib;
    delete route_mgr;
    delete current_panel;
    delete soundmgr;
    delete airports;
    delete runways;
    delete ATC_mgr;
    delete ATC_display;
    delete AI_mgr;
    delete controls;
    delete viewmgr;
    delete props;
    delete initial_state;
    //delete locale; Don't delete locale
    delete commands;
    delete model_lib;
    delete acmodel;
    delete model_mgr;
    delete channel_options_list;
    delete initial_waypoints;
    delete scenery;
    //delete tile_mgr; // Don't delete tile manager yet, because loader thread problems
    delete io;
    delete fontcache;

    delete navlist;
    delete loclist;
    delete gslist;
    delete dmelist;
    delete mkrlist;
    delete tacanlist;
    delete carrierlist;
    delete channellist;
    delete fixlist;
    delete airwaynet;
    delete multiplayer_mgr;

}


// set the fg_root path
void FGGlobals::set_fg_root (const string &root) {
    fg_root = root;
    
    // append /data to root if it exists
    SGPath tmp( fg_root );
    tmp.append( "data" );
    tmp.append( "version" );
    if ( ulFileExists( tmp.c_str() ) ) {
        fg_root += "/data";
    }

    fgSetString("/sim/fg-root", fg_root.c_str());   
}

void FGGlobals::set_fg_scenery (const string &scenery) {
    SGPath s;
    if (scenery.empty()) {
        s.set( fg_root );
        s.append( "Scenery" );
    } else
        s.set( scenery );

    string_list path_list = sgPathSplit( s.str() );
    fg_scenery.clear();

    for (unsigned i = 0; i < path_list.size(); i++) {

        ulDir *d = ulOpenDir( path_list[i].c_str() );
        if (d == NULL)
            continue;
        ulCloseDir( d );

        SGPath pt( path_list[i] ), po( path_list[i] );
        pt.append("Terrain");
        po.append("Objects");

        ulDir *td = ulOpenDir( pt.c_str() );
        ulDir *od = ulOpenDir( po.c_str() );

        if (td == NULL && od == NULL)
            fg_scenery.push_back( path_list[i] );
        else {
            if (td != NULL) {
                fg_scenery.push_back( pt.str() );
                ulCloseDir( td );
            }
            if (od != NULL) {
                fg_scenery.push_back( po.str() );
                ulCloseDir( od );
            }
        }
        // insert a marker for FGTileEntry::load(), so that
        // FG_SCENERY=A:B becomes list ["A/Terrain", "A/Objects", "",
        // "B/Terrain", "B/Objects", ""]
        fg_scenery.push_back("");
    }
}


FGRenderer *
FGGlobals::get_renderer () const
{
   return renderer;
}

SGSubsystemMgr *
FGGlobals::get_subsystem_mgr () const
{
    return subsystem_mgr;
}

SGSubsystem *
FGGlobals::get_subsystem (const char * name)
{
    return subsystem_mgr->get_subsystem(name);
}

void
FGGlobals::add_subsystem (const char * name,
                          SGSubsystem * subsystem,
                          SGSubsystemMgr::GroupType type,
                          double min_time_sec)
{
    subsystem_mgr->add(name, subsystem, type, min_time_sec);
}


SGEventMgr *
FGGlobals::get_event_mgr () const
{
    return event_mgr;
}


// Save the current state as the initial state.
void
FGGlobals::saveInitialState ()
{
  delete initial_state;
  initial_state = new SGPropertyNode();

  if (!copyProperties(props, initial_state))
    SG_LOG(SG_GENERAL, SG_ALERT, "Error saving initial state");
}


// Restore the saved initial state, if any
void
FGGlobals::restoreInitialState ()
{
    if ( initial_state == 0 ) {
        SG_LOG(SG_GENERAL, SG_ALERT,
               "No initial state available to restore!!!");
        return;
    }

    SGPropertyNode *currentPresets = new SGPropertyNode;
    SGPropertyNode *targetNode = fgGetNode( "/sim/presets" );

    // stash the /sim/presets tree
    if ( !copyProperties(targetNode, currentPresets) ) {
        SG_LOG( SG_GENERAL, SG_ALERT, "Failed to save /sim/presets subtree" );
    }
    
    if ( copyProperties(initial_state, props) ) {
        SG_LOG( SG_GENERAL, SG_INFO, "Initial state restored successfully" );
    } else {
        SG_LOG( SG_GENERAL, SG_INFO,
                "Some errors restoring initial state (read-only props?)" );
    }

    // recover the /sim/presets tree
    if ( !copyProperties(currentPresets, targetNode) ) {
        SG_LOG( SG_GENERAL, SG_ALERT,
                "Failed to restore /sim/presets subtree" );
    }

   delete currentPresets;
}

FGViewer *
FGGlobals::get_current_view () const
{
  return viewmgr->get_current_view();
}

// end of globals.cxx
