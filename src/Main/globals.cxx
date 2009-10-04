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

#include <simgear/structure/commands.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/timing/sg_time.hxx>
#include <simgear/ephemeris/ephemeris.hxx>
#include <simgear/magvar/magvar.hxx>
#include <simgear/scene/material/matlib.hxx>
#include <simgear/structure/subsystem_mgr.hxx>
#include <simgear/structure/event_mgr.hxx>

#include <Aircraft/controls.hxx>
#include <Airports/runways.hxx>
#include <ATCDCL/AIMgr.hxx>
#include <ATCDCL/ATCmgr.hxx>
#include <Autopilot/route_mgr.hxx>
#include <Cockpit/panel.hxx>
#include <GUI/new_gui.hxx>
#include <Model/acmodel.hxx>
#include <Model/modelmgr.hxx>
#include <MultiPlayer/multiplaymgr.hxx>
#include <Navaids/awynet.hxx>
#include <Scenery/scenery.hxx>
#include <Scenery/tilemgr.hxx>
#include <Navaids/navlist.hxx>

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
    props( new SGPropertyNode ),
    initial_state( NULL ),
    locale( NULL ),
    renderer( new FGRenderer ),
    subsystem_mgr( new SGSubsystemMgr ),
    event_mgr( new SGEventMgr ),
    sim_time_sec( 0.0 ),
    fg_root( "" ),
    warp( 0 ),
    warp_delta( 0 ),
    time_params( NULL ),
    ephem( NULL ),
    mag( NULL ),
    matlib( NULL ),
    route_mgr( NULL ),
    current_panel( NULL ),
    ATC_mgr( NULL ),
    AI_mgr( NULL ),
    controls( NULL ),
    viewmgr( NULL ),
    commands( SGCommandMgr::instance() ),
    acmodel( NULL ),
    model_mgr( NULL ),
    channel_options_list( NULL ),
    initial_waypoints( NULL ),
    scenery( NULL ),
    tile_mgr( NULL ),
    fontcache ( new FGFontCache ),
    navlist( NULL ),
    loclist( NULL ),
    gslist( NULL ),
    dmelist( NULL ),
    tacanlist( NULL ),
    carrierlist( NULL ),
    channellist( NULL ),
    airwaynet( NULL ),
    multiplayer_mgr( NULL )
{
}


// Destructor
FGGlobals::~FGGlobals() 
{
    delete renderer;
// The AIModels manager performs a number of actions upon
    // Shutdown that implicitly assume that other subsystems
    // are still operational (Due to the dynamic allocation and
    // deallocation of AIModel objects. To ensure we can safely
    // shut down all subsystems, make sure we take down the 
    // AIModels system first.
    subsystem_mgr->get_group(SGSubsystemMgr::GENERAL)->remove_subsystem("ai_model");
    // FGInput (FGInputEvent) and FGDialog calls get_subsystem() in their destructors, 
    // which is not safe since some subsystem are already deleted but can be referred.
    // So these subsystems must be deleted prior to deleting subsystem_mgr unless
    // ~SGSubsystemGroup and SGSubsystemMgr::get_subsystem are changed not to refer to
    // deleted subsystems.
    subsystem_mgr->get_group(SGSubsystemMgr::GENERAL)->remove_subsystem("input");
    subsystem_mgr->get_group(SGSubsystemMgr::GENERAL)->remove_subsystem("gui");
    delete subsystem_mgr;
    delete event_mgr;
    delete time_params;
    delete ephem;
    delete mag;
    delete matlib;
    delete route_mgr;
    delete current_panel;

    delete ATC_mgr;
    delete AI_mgr;
    delete controls;
    delete viewmgr;

//     delete commands;
    delete acmodel;
    delete model_mgr;
    delete channel_options_list;
    delete initial_waypoints;
    delete tile_mgr;
    delete scenery;
    delete fontcache;

    delete navlist;
    delete loclist;
    delete gslist;
    delete dmelist;
    delete tacanlist;
    delete carrierlist;
    delete channellist;
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
        fgGetNode("BAD_FG_ROOT", true)->setStringValue(fg_root);
        fg_root += "/data";
        fgGetNode("GOOD_FG_ROOT", true)->setStringValue(fg_root);
        SG_LOG(SG_GENERAL, SG_ALERT, "***\n***\n*** Warning: changing bad FG_ROOT/--fg-root to '"
                << fg_root << "'\n***\n***");
    }

    // remove /sim/fg-root before writing to prevent hijacking
    SGPropertyNode *n = fgGetNode("/sim", true);
    n->removeChild("fg-root", 0, false);
    n = n->getChild("fg-root", 0, true);
    n->setStringValue(fg_root.c_str());
    n->setAttribute(SGPropertyNode::WRITE, false);
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

	// "Terrain" and "Airports" directory don't exist. add directory as is
        // otherwise, automatically append either Terrain, Objects, or both
        //if (td == NULL && od == NULL)
            fg_scenery.push_back( path_list[i] );
        //else {
            if (td != NULL) {
                fg_scenery.push_back( pt.str() );
                ulCloseDir( td );
            }
            if (od != NULL) {
                fg_scenery.push_back( po.str() );
                ulCloseDir( od );
            }
        //}
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
