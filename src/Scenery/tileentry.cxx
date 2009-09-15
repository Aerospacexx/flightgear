// tileentry.cxx -- routines to handle a scenery tile
//
// Written by Curtis Olson, started May 1998.
//
// Copyright (C) 1998 - 2001  Curtis L. Olson  - http://www.flightgear.org/~curt
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
#include <plib/ul.h>
#include <Main/main.hxx>


#include STL_STRING

#include <simgear/bucket/newbucket.hxx>
#include <simgear/debug/logstream.hxx>
#include <simgear/math/polar3d.hxx>
#include <simgear/math/sg_geodesy.hxx>
#include <simgear/math/sg_random.h>
#include <simgear/misc/sgstream.hxx>
#include <simgear/scene/material/mat.hxx>
#include <simgear/scene/material/matlib.hxx>
#include <simgear/scene/tgdb/apt_signs.hxx>
#include <simgear/scene/tgdb/obj.hxx>
#include <simgear/scene/tgdb/vasi.hxx>
#include <simgear/scene/model/placementtrans.hxx>

#include <Aircraft/aircraft.hxx>
#include <Include/general.hxx>
#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Main/viewer.hxx>
#include <Scenery/scenery.hxx>
#include <Time/light.hxx>

#include "tileentry.hxx"
#include "tilemgr.hxx"

SG_USING_STD(string);


// Constructor
FGTileEntry::FGTileEntry ( const SGBucket& b )
    : center( Point3D( 0.0 ) ),
      tile_bucket( b ),
      terra_transform( new ssgPlacementTransform ),
      vasi_lights_transform( new ssgPlacementTransform ),
      rwy_lights_transform( new ssgPlacementTransform ),
      taxi_lights_transform( new ssgPlacementTransform ),
      terra_range( new ssgRangeSelector ),
      vasi_lights_selector( new ssgSelector ),
      rwy_lights_selector( new ssgSelector ),
      taxi_lights_selector( new ssgSelector ),
      loaded(false),
      pending_models(0),
      is_inner_ring(false),
      free_tracker(0)
{
    // update the contents
    // if ( vec3_ptrs.size() || vec2_ptrs.size() || index_ptrs.size() ) {
    //     SG_LOG( SG_TERRAIN, SG_ALERT, 
    //             "Attempting to overwrite existing or"
    //             << " not properly freed leaf data." );
    //     exit(-1);
    // }
}


// Destructor
FGTileEntry::~FGTileEntry () {
    // cout << "nodes = " << nodes.size() << endl;;
    // delete[] nodes;
}


#if 0
// Please keep this for reference.  We use Norman's optimized routine,
// but here is what the routine really is doing.
void
FGTileEntry::WorldCoordinate( sgCoord *obj_pos, Point3D center,
                              double lat, double lon, double elev, double hdg)
{
    // setup transforms
    Point3D geod( lon * SGD_DEGREES_TO_RADIANS,
                  lat * SGD_DEGREES_TO_RADIANS,
                  elev );
        
    Point3D world_pos = sgGeodToCart( geod );
    Point3D offset = world_pos - center;
        
    sgMat4 POS;
    sgMakeTransMat4( POS, offset.x(), offset.y(), offset.z() );

    sgVec3 obj_rt, obj_up;
    sgSetVec3( obj_rt, 0.0, 1.0, 0.0); // Y axis
    sgSetVec3( obj_up, 0.0, 0.0, 1.0); // Z axis

    sgMat4 ROT_lon, ROT_lat, ROT_hdg;
    sgMakeRotMat4( ROT_lon, lon, obj_up );
    sgMakeRotMat4( ROT_lat, 90 - lat, obj_rt );
    sgMakeRotMat4( ROT_hdg, hdg, obj_up );

    sgMat4 TUX;
    sgCopyMat4( TUX, ROT_hdg );
    sgPostMultMat4( TUX, ROT_lat );
    sgPostMultMat4( TUX, ROT_lon );
    sgPostMultMat4( TUX, POS );

    sgSetCoord( obj_pos, TUX );
}
#endif


// Norman's 'fast hack' for above
static void WorldCoordinate( sgCoord *obj_pos, Point3D center, double lat,
                             double lon, double elev, double hdg )
{
    double lon_rad = lon * SGD_DEGREES_TO_RADIANS;
    double lat_rad = lat * SGD_DEGREES_TO_RADIANS;
    double hdg_rad = hdg * SGD_DEGREES_TO_RADIANS;

    // setup transforms
    Point3D geod( lon_rad, lat_rad, elev );
        
    Point3D world_pos = sgGeodToCart( geod );
    Point3D offset = world_pos - center;

    sgMat4 mat;

    SGfloat sin_lat = (SGfloat)sin( lat_rad );
    SGfloat cos_lat = (SGfloat)cos( lat_rad );
    SGfloat cos_lon = (SGfloat)cos( lon_rad );
    SGfloat sin_lon = (SGfloat)sin( lon_rad );
    SGfloat sin_hdg = (SGfloat)sin( hdg_rad ) ;
    SGfloat cos_hdg = (SGfloat)cos( hdg_rad ) ;

    mat[0][0] =  cos_hdg * (SGfloat)sin_lat * (SGfloat)cos_lon - sin_hdg * (SGfloat)sin_lon;
    mat[0][1] =  cos_hdg * (SGfloat)sin_lat * (SGfloat)sin_lon + sin_hdg * (SGfloat)cos_lon;
    mat[0][2] = -cos_hdg * (SGfloat)cos_lat;
    mat[0][3] =  SG_ZERO;

    mat[1][0] = -sin_hdg * (SGfloat)sin_lat * (SGfloat)cos_lon - cos_hdg * (SGfloat)sin_lon;
    mat[1][1] = -sin_hdg * (SGfloat)sin_lat * (SGfloat)sin_lon + cos_hdg * (SGfloat)cos_lon;
    mat[1][2] =  sin_hdg * (SGfloat)cos_lat;
    mat[1][3] =  SG_ZERO;

    mat[2][0] = (SGfloat)cos_lat * (SGfloat)cos_lon;
    mat[2][1] = (SGfloat)cos_lat * (SGfloat)sin_lon;
    mat[2][2] = (SGfloat)sin_lat;
    mat[2][3] =  SG_ZERO;

    mat[3][0] = offset.x();
    mat[3][1] = offset.y();
    mat[3][2] = offset.z();
    mat[3][3] = SG_ONE ;

    sgSetCoord( obj_pos, mat );
}


// recurse an ssg tree and call removeKid() on every node from the
// bottom up.  Leaves the original branch in existance, but empty so
// it can be removed by the calling routine.
static void my_remove_branch( ssgBranch * branch ) {
    for ( ssgEntity *k = branch->getKid( 0 );
          k != NULL; 
          k = branch->getNextKid() )
    {
        if ( k -> isAKindOf ( ssgTypeBranch() ) ) {
            my_remove_branch( (ssgBranch *)k );
            branch -> removeKid ( k );
        } else if ( k -> isAKindOf ( ssgTypeLeaf() ) ) {
            branch -> removeKid ( k ) ;
        }
    }
}


// Free "n" leaf elements of an ssg tree.  returns the number of
// elements freed.  An empty branch node is considered a leaf.  This
// is intended to spread the load of freeing a complex tile out over
// several frames.
static int fgPartialFreeSSGtree( ssgBranch *b, int n ) {
    int num_deletes = 0;

    if ( n > 0 ) {
        // we still have some delete budget left
        // if ( b->getNumKids() > 100 ) {
        //     cout << "large family = " << b->getNumKids() << endl;
        // }
        // deleting in reverse would help if my plib patch get's
        // applied, but for now it will make things slower.
        // for ( int i = b->getNumKids() - 1; i >= 0 ; --i ) {
        for ( int i = 0; i < b->getNumKids(); ++i ) {
            ssgEntity *kid = b->getKid(i);
            if ( kid->isAKindOf( ssgTypeBranch() ) && kid->getRef() <= 1 ) {
                int result = fgPartialFreeSSGtree( (ssgBranch *)kid, n );
                num_deletes += result;
                n -= result;
                if ( n < 0 ) {
                    break;
                }
            }
            // remove the kid if (a) it is now empty -or- (b) it's ref
            // count is > zero at which point we don't care if it's
            // empty, we don't want to touch it's contents.
            if ( kid->getNumKids() == 0 || kid->getRef() > 1 ) {
                b->removeKid( kid );
                num_deletes++;
                n--;
            }
        }
    }

    return num_deletes;
}


// Clean up the memory used by this tile and delete the arrays used by
// ssg as well as the whole ssg branch
bool FGTileEntry::free_tile() {
    int delete_size = 100;
    SG_LOG( SG_TERRAIN, SG_DEBUG,
            "FREEING TILE = (" << tile_bucket << ")" );

    SG_LOG( SG_TERRAIN, SG_DEBUG, "(start) free_tracker = " << free_tracker );
    
    if ( !(free_tracker & NODES) ) {
        free_tracker |= NODES;
    } else if ( !(free_tracker & VEC_PTRS) ) {
        free_tracker |= VEC_PTRS;
    } else if ( !(free_tracker & TERRA_NODE) ) {
        // delete the terrain branch (this should already have been
        // disconnected from the scene graph)
        SG_LOG( SG_TERRAIN, SG_DEBUG, "FREEING terra_transform" );
        if ( fgPartialFreeSSGtree( terra_transform, delete_size ) == 0 ) {
            terra_transform = 0;
            free_tracker |= TERRA_NODE;
        }
    } else if ( !(free_tracker & GROUND_LIGHTS) && gnd_lights_transform ) {
        // delete the terrain lighting branch (this should already have been
        // disconnected from the scene graph)
        SG_LOG( SG_TERRAIN, SG_DEBUG, "FREEING gnd_lights_transform" );
        if ( fgPartialFreeSSGtree( gnd_lights_transform, delete_size ) == 0 ) {
            gnd_lights_transform = 0;
            free_tracker |= GROUND_LIGHTS;
        }
    } else if ( !(free_tracker & VASI_LIGHTS) && vasi_lights_selector ) {
        // delete the runway lighting branch (this should already have
        // been disconnected from the scene graph)
        SG_LOG( SG_TERRAIN, SG_DEBUG, "FREEING vasi_lights_selector" );
        if ( fgPartialFreeSSGtree( vasi_lights_selector, delete_size ) == 0 ) {
            vasi_lights_selector = 0;
            free_tracker |= VASI_LIGHTS;
        }
    } else if ( !(free_tracker & RWY_LIGHTS) && rwy_lights_selector ) {
        // delete the runway lighting branch (this should already have
        // been disconnected from the scene graph)
        SG_LOG( SG_TERRAIN, SG_DEBUG, "FREEING rwy_lights_selector" );
        if ( fgPartialFreeSSGtree( rwy_lights_selector, delete_size ) == 0 ) {
            rwy_lights_selector = 0;
            free_tracker |= RWY_LIGHTS;
        }
    } else if ( !(free_tracker & TAXI_LIGHTS) && taxi_lights_selector ) {
        // delete the taxi lighting branch (this should already have been
        // disconnected from the scene graph)
        SG_LOG( SG_TERRAIN, SG_DEBUG, "FREEING taxi_lights_selector" );
        if ( fgPartialFreeSSGtree( taxi_lights_selector, delete_size ) == 0 ) {
            taxi_lights_selector = 0;
            free_tracker |= TAXI_LIGHTS;
        }
    } else if ( !(free_tracker & LIGHTMAPS) ) {
        free_tracker |= LIGHTMAPS;
    } else {
        return true;
    }

    SG_LOG( SG_TERRAIN, SG_DEBUG, "(end) free_tracker = " << free_tracker );

    // if we fall down to here, we still have work todo, return false
    return false;
}


// Update the ssg transform node for this tile so it can be
// properly drawn relative to our (0,0,0) point
void FGTileEntry::prep_ssg_node( const Point3D& p, sgVec3 up, float vis) {
    if ( !loaded ) return;

    // visibility can change from frame to frame so we update the
    // range selector cutoff's each time.
    terra_range->setRange( 0, SG_ZERO );
    terra_range->setRange( 1, vis + bounding_radius );

    if ( gnd_lights_range ) {
        gnd_lights_range->setRange( 0, SG_ZERO );
        gnd_lights_range->setRange( 1, vis * 1.5 + bounding_radius );
    }

    sgdVec3 sgdTrans;
    sgdSetVec3( sgdTrans, center.x(), center.y(), center.z() );

    FGLight *l = (FGLight *)(globals->get_subsystem("lighting"));
    if ( gnd_lights_transform ) {
        // we need to lift the lights above the terrain to avoid
        // z-buffer fighting.  We do this based on our altitude and
        // the distance this tile is away from scenery center.

        // we expect 'up' to be a unit vector coming in, but since we
        // modify the value of lift_vec, we need to create a local
        // copy.
        sgVec3 lift_vec;
        sgCopyVec3( lift_vec, up );

        double agl;
        agl = globals->get_current_view()->getAltitudeASL_ft()*SG_FEET_TO_METER
          - globals->get_current_view()->getSGLocation()->get_cur_elev_m();

        // Compute the distance of the scenery center from the view position.
        double dist = center.distance3D(p);

        if ( general.get_glDepthBits() > 16 ) {
            sgScaleVec3( lift_vec, 10.0 + agl / 100.0 + dist / 10000 );
        } else {
            sgScaleVec3( lift_vec, 10.0 + agl / 20.0 + dist / 5000 );
        }

        sgdVec3 dlt_trans;
        sgdCopyVec3( dlt_trans, sgdTrans );
        sgdVec3 dlift_vec;
        sgdSetVec3( dlift_vec, lift_vec );
        sgdAddVec3( dlt_trans, dlift_vec );
        gnd_lights_transform->setTransform( dlt_trans );

        // select which set of lights based on sun angle
        float sun_angle = l->get_sun_angle() * SGD_RADIANS_TO_DEGREES;
        if ( sun_angle > 95 ) {
            gnd_lights_brightness->select(0x04);
        } else if ( sun_angle > 92 ) {
            gnd_lights_brightness->select(0x02);
        } else if ( sun_angle > 89 ) {
            gnd_lights_brightness->select(0x01);
        } else {
            gnd_lights_brightness->select(0x00);
        }
    }

    if ( vasi_lights_transform ) {
        // we need to lift the lights above the terrain to avoid
        // z-buffer fighting.  We do this based on our altitude and
        // the distance this tile is away from scenery center.

        sgVec3 lift_vec;
        sgCopyVec3( lift_vec, up );

        // we fudge agl by 30 meters so that the lifting function
        // doesn't phase in until we are > 30m agl.
        double agl;
        agl = globals->get_current_view()->getAltitudeASL_ft()*SG_FEET_TO_METER
          - globals->get_current_view()->getSGLocation()->get_cur_elev_m()
          - 30.0;
        if ( agl < 0.0 ) {
            agl = 0.0;
        }
        
        if ( general.get_glDepthBits() > 16 ) {
            sgScaleVec3( lift_vec, 0.25 + agl / 400.0 + agl*agl / 5000000.0 );
        } else {
            sgScaleVec3( lift_vec, 0.25 + agl / 150.0 );
        }

        sgdVec3 dlt_trans;
        sgdCopyVec3( dlt_trans, sgdTrans );
        sgdVec3 dlift_vec;
        sgdSetVec3( dlift_vec, lift_vec );
        sgdAddVec3( dlt_trans, dlift_vec );
        vasi_lights_transform->setTransform( dlt_trans );

        // generally, vasi lights are always on
        vasi_lights_selector->select(0x01);
    }

    if ( rwy_lights_transform ) {
        // we need to lift the lights above the terrain to avoid
        // z-buffer fighting.  We do this based on our altitude and
        // the distance this tile is away from scenery center.

        sgVec3 lift_vec;
        sgCopyVec3( lift_vec, up );

        // we fudge agl by 30 meters so that the lifting function
        // doesn't phase in until we are > 30m agl.
        double agl;
        agl = globals->get_current_view()->getAltitudeASL_ft()*SG_FEET_TO_METER
          - globals->get_current_view()->getSGLocation()->get_cur_elev_m()
          - 30.0;
        if ( agl < 0.0 ) {
            agl = 0.0;
        }
        
        if ( general.get_glDepthBits() > 16 ) {
            sgScaleVec3( lift_vec, 0.01 + agl / 400.0 + agl*agl / 5000000.0 );
        } else {
            sgScaleVec3( lift_vec, 0.25 + agl / 150.0 );
        }

        sgdVec3 dlt_trans;
        sgdCopyVec3( dlt_trans, sgdTrans );
        sgdVec3 dlift_vec;
        sgdSetVec3( dlift_vec, lift_vec );
        sgdAddVec3( dlt_trans, dlift_vec );
        rwy_lights_transform->setTransform( dlt_trans );

        // turn runway lights on/off based on sun angle and visibility
        float sun_angle = l->get_sun_angle() * SGD_RADIANS_TO_DEGREES;
        if ( sun_angle > 85 ||
             (fgGetDouble("/environment/visibility-m") < 5000.0) ) {
            rwy_lights_selector->select(0x01);
        } else {
            rwy_lights_selector->select(0x00);
        }
    }

    if ( taxi_lights_transform ) {
        // we need to lift the lights above the terrain to avoid
        // z-buffer fighting.  We do this based on our altitude and
        // the distance this tile is away from scenery center.

        sgVec3 lift_vec;
        sgCopyVec3( lift_vec, up );

        // we fudge agl by 30 meters so that the lifting function
        // doesn't phase in until we are > 30m agl.
        double agl;
        agl = globals->get_current_view()->getAltitudeASL_ft()*SG_FEET_TO_METER
          - globals->get_current_view()->getSGLocation()->get_cur_elev_m()
          - 30.0;
        if ( agl < 0.0 ) {
            agl = 0.0;
        }
        
        if ( general.get_glDepthBits() > 16 ) {
            sgScaleVec3( lift_vec, 0.01 + agl / 400.0 + agl*agl / 5000000.0 );
        } else {
            sgScaleVec3( lift_vec, 0.25 + agl / 150.0 );
        }

        sgdVec3 dlt_trans;
        sgdCopyVec3( dlt_trans, sgdTrans );
        sgdVec3 dlift_vec;
        sgdSetVec3( dlift_vec, lift_vec );
        sgdAddVec3( dlt_trans, dlift_vec );
        taxi_lights_transform->setTransform( dlt_trans );

        // turn taxi lights on/off based on sun angle and visibility
        float sun_angle = l->get_sun_angle() * SGD_RADIANS_TO_DEGREES;
        if ( sun_angle > 85 ||
             (fgGetDouble("/environment/visibility-m") < 5000.0) ) {
            taxi_lights_selector->select(0x01);
        } else {
            taxi_lights_selector->select(0x00);
        }
    }

    if ( vasi_lights_transform && is_inner_ring ) {
        // now we need to traverse the list of vasi lights and update
        // their coloring (but only for the inner ring, no point in
        // doing this extra work for tiles that are relatively far
        // away.)
        for ( int i = 0; i < vasi_lights_transform->getNumKids(); ++i ) {
            // cout << "vasi root = " << i << endl;
            ssgBranch *v = (ssgBranch *)vasi_lights_transform->getKid(i);
            for ( int j = 0; j < v->getNumKids(); ++j ) {
                // cout << "  vasi branch = " << j << endl;
                ssgTransform *kid = (ssgTransform *)v->getKid(j);
                SGVASIUserData *vasi = (SGVASIUserData *)kid->getUserData();

                if ( vasi != NULL ) {
                    sgdVec3 s;
                    sgdCopyVec3( s, vasi->get_abs_pos() );
                    Point3D start(s[0], s[1], s[2]);

                    sgdVec3 d;
                    sgdCopyVec3( d, globals->get_current_view()->get_absolute_view_pos() );

                    double dist = sgdDistanceVec3( s, d );

                    if ( dist < 10000 ) {
                        double cur_alt
                            = globals->get_current_view()->getAltitudeASL_ft()
                            * SG_FEET_TO_METER;

                        double y = cur_alt - vasi->get_alt_m();

                        double angle;
                        if ( dist != 0 ) {
                            angle = asin( y / dist );
                        } else {
                            angle = 0.0;
                        }

                        vasi->set_color(angle * SG_RADIANS_TO_DEGREES);
                    }
                    // cout << "    dist = " << dist
                    //      << " angle = " << angle * SG_RADIANS_TO_DEGREES
                    //      << endl;
                } else {
                    SG_LOG( SG_TERRAIN, SG_ALERT, "no vasi data!" );
                }
            }
        }
    }
}


// Set up lights rendering call backs
static int fgLightsPredraw( ssgEntity *e ) {
#if 0
    if (glPointParameterIsSupported) {
        static float quadratic[3] = {1.0, 0.01, 0.0001};
        glPointParameterfvProc(GL_DISTANCE_ATTENUATION_EXT, quadratic);
        glPointParameterfProc(GL_POINT_SIZE_MIN_EXT, 1.0); 
        glPointSize(4.0);
    }
#endif
    return true;
}

static int fgLightsPostdraw( ssgEntity *e ) {
#if 0
    if (glPointParameterIsSupported) {
        static float default_attenuation[3] = {1.0, 0.0, 0.0};
        glPointParameterfvProc(GL_DISTANCE_ATTENUATION_EXT,
                              default_attenuation);
        glPointSize(1.0);
    }
#endif
    return true;
}


ssgLeaf* FGTileEntry::gen_lights( SGMaterialLib *matlib, ssgVertexArray *lights,
                                  int inc, float bright )
{
    // generate a repeatable random seed
    float *p1 = lights->get( 0 );
    unsigned int *seed = (unsigned int *)p1;
    sg_srandom( *seed );

    int size = lights->getNum() / inc;

    // Allocate ssg structure
    ssgVertexArray   *vl = new ssgVertexArray( size + 1 );
    ssgNormalArray   *nl = NULL;
    ssgTexCoordArray *tl = NULL;
    ssgColourArray   *cl = new ssgColourArray( size + 1 );

    sgVec4 color;
    for ( int i = 0; i < lights->getNum(); ++i ) {
        // this loop is slightly less efficient than it otherwise
        // could be, but we want a red light to always be red, and a
        // yellow light to always be yellow, etc. so we are trying to
        // preserve the random sequence.
        float zombie = sg_random();
        if ( i % inc == 0 ) {
            vl->add( lights->get(i) );

            // factor = sg_random() ^ 2, range = 0 .. 1 concentrated towards 0
            float factor = sg_random();
            factor *= factor;

            if ( zombie > 0.5 ) {
                // 50% chance of yellowish
                sgSetVec4( color, 0.9, 0.9, 0.3, bright - factor * 0.2 );
            } else if ( zombie > 0.15 ) {
                // 35% chance of whitish
                sgSetVec4( color, 0.9, 0.9, 0.8, bright - factor * 0.2 );
            } else if ( zombie > 0.05 ) {
                // 10% chance of orangish
                sgSetVec4( color, 0.9, 0.6, 0.2, bright - factor * 0.2 );
            } else {
                // 5% chance of redish
                sgSetVec4( color, 0.9, 0.2, 0.2, bright - factor * 0.2 );
            }
            cl->add( color );
        }
    }

    // create ssg leaf
    ssgLeaf *leaf = 
        new ssgVtxTable ( GL_POINTS, vl, nl, tl, cl );

    // assign state
    SGMaterial *mat = matlib->find( "GROUND_LIGHTS" );
    leaf->setState( mat->get_state() );
    leaf->setCallback( SSG_CALLBACK_PREDRAW, fgLightsPredraw );
    leaf->setCallback( SSG_CALLBACK_POSTDRAW, fgLightsPostdraw );

    return leaf;
}


bool FGTileEntry::obj_load( const string& path,
                            ssgBranch *geometry,
                            ssgBranch *vasi_lights,
                            ssgBranch *rwy_lights,
                            ssgBranch *taxi_lights,
                            ssgVertexArray *ground_lights, bool is_base )
{
    Point3D c;                  // returned center point
    double br;                  // returned bounding radius

    bool use_random_objects =
        fgGetBool("/sim/rendering/random-objects", true);

    // try loading binary format
    if ( sgBinObjLoad( path, is_base,
                       &c, &br, globals->get_matlib(), use_random_objects,
                       geometry, vasi_lights, rwy_lights, taxi_lights,
                       ground_lights ) )
    {
        if ( is_base ) {
            center = c;
            bounding_radius = br;
        }
    }

    return (geometry != NULL);
}


typedef enum {
    OBJECT,
    OBJECT_SHARED,
    OBJECT_STATIC,
    OBJECT_SIGN,
    OBJECT_RUNWAY_SIGN
} object_type;


// storage class for deferred object processing in FGTileEntry::load()
struct Object {
    Object(object_type t, const string& token, const SGPath& p, istream& in)
        : type(t), path(p)
    {
        in >> name;
        if (type != OBJECT)
            in >> lon >> lat >> elev >> hdg;
        in >> ::skipeol;

        if (type == OBJECT)
            SG_LOG(SG_TERRAIN, SG_INFO, "    " << token << "  " << name);
        else
            SG_LOG(SG_TERRAIN, SG_INFO, "    " << token << "  " << name << "  lon=" <<
                    lon << "  lat=" << lat << "  elev=" << elev << "  hdg=" << hdg);
    }
    object_type type;
    string name;
    SGPath path;
    double lon, lat, elev, hdg;
};


void
FGTileEntry::load( const string_list &path_list, bool is_base )
{
    bool found_tile_base = false;

    SGPath object_base;
    vector<const Object*> objects;

    string index_str = tile_bucket.gen_index_str();
    SG_LOG( SG_TERRAIN, SG_INFO, "Loading tile " << index_str );

    // scan and parse all files and store information
    for (unsigned int i = 0; i < path_list.size(); i++) {
        // If we found a terrain tile in Terrain/, we have to process the
        // Objects/ dir in the same group, too, before we can stop scanning.
        // FGGlobals::set_fg_scenery() inserts an empty string to path_list
        // as marker.
        if (path_list[i].empty()) {
            if (found_tile_base)
                break;
            else
                continue;
        }

        bool has_base = false;

        SGPath tile_path = path_list[i];
        tile_path.append( tile_bucket.gen_base_path() );

        SGPath basename = tile_path;
        basename.append( index_str );

        SG_LOG( SG_TERRAIN, SG_INFO, "  Trying " << basename.str() );


        // Check for master .stg (scene terra gear) file
        SGPath stg_name = basename;
        stg_name.concat( ".stg" );

        sg_gzifstream in( stg_name.str() );
        if ( !in.is_open() )
            continue;

        while ( ! in.eof() ) {
            string token;
            in >> token;

            if ( token.empty() || token[0] == '#' ) {
               in >> ::skipeol;
               continue;
            }
                            // Load only once (first found)
            if ( token == "OBJECT_BASE" ) {
                string name;
                in >> name >> ::skipws;
                SG_LOG( SG_TERRAIN, SG_INFO, "    " << token << " " << name );

                if (!found_tile_base) {
                    found_tile_base = true;
                    has_base = true;

                    object_base = tile_path;
                    object_base.append(name);

                } else
                    SG_LOG(SG_TERRAIN, SG_INFO, "    (skipped)");

                            // Load only if base is not in another file
            } else if ( token == "OBJECT" ) {
                if (!found_tile_base || has_base)
                    objects.push_back(new Object(OBJECT, token, tile_path, in));
                else {
                    string name;
                    in >> name >> ::skipeol;
                    SG_LOG(SG_TERRAIN, SG_INFO, "    " << token << "  "
                            << name << "  (skipped)");
                }

                            // Always OK to load
            } else if ( token == "OBJECT_STATIC" ) {
                objects.push_back(new Object(OBJECT_STATIC, token, tile_path, in));

            } else if ( token == "OBJECT_SHARED" ) {
                objects.push_back(new Object(OBJECT_SHARED, token, tile_path, in));

            } else if ( token == "OBJECT_SIGN" ) {
                objects.push_back(new Object(OBJECT_SIGN, token, tile_path, in));

            } else if ( token == "OBJECT_RUNWAY_SIGN" ) {
                objects.push_back(new Object(OBJECT_RUNWAY_SIGN, token, tile_path, in));

            } else {
                SG_LOG( SG_TERRAIN, SG_DEBUG,
                        "Unknown token '" << token << "' in " << stg_name.str() );
                in >> ::skipws;
            }
        }
    }


    // obj_load() will generate ground lighting for us ...
    ssgVertexArray *light_pts = new ssgVertexArray( 100 );
    ssgBranch* new_tile = new ssgBranch;


    if (found_tile_base) {
        // load tile if found ...
        ssgSharedPtr<ssgBranch> geometry = new ssgBranch;
        if ( obj_load( object_base.str(), geometry,
                       NULL, NULL, NULL, light_pts, true ) ) {
            geometry->getKid( 0 )->setTravCallback(SSG_CALLBACK_PRETRAV,
                    &FGTileMgr::tile_filter_cb);

            new_tile -> addKid( geometry );
        }

    } else {
        // ... or generate an ocean tile on the fly
        SG_LOG(SG_TERRAIN, SG_INFO, "  Generating ocean tile");
        ssgSharedPtr<ssgBranch> geometry = new ssgBranch;
        Point3D c;
        double br;
        if ( sgGenTile( path_list[0], tile_bucket, &c, &br,
                        globals->get_matlib(), geometry ) ) {
            center = c;
            bounding_radius = br;
            new_tile -> addKid( geometry );
        } else {
            SG_LOG( SG_TERRAIN, SG_ALERT,
                    "Warning: failed to generate ocean tile!" );
        }
    }


    // now that we have a valid center, process all the objects
    for (unsigned int j = 0; j < objects.size(); j++) {
        const Object *obj = objects[j];

        if (obj->type == OBJECT) {
            SGPath custom_path = obj->path;
            custom_path.append( obj->name );

            ssgSharedPtr<ssgBranch> geometry = new ssgBranch;
            ssgSharedPtr<ssgBranch> vasi_lights = new ssgBranch;
            ssgSharedPtr<ssgBranch> rwy_lights = new ssgBranch;
            ssgSharedPtr<ssgBranch> taxi_lights = new ssgBranch;

            if ( obj_load( custom_path.str(),
                           geometry, vasi_lights, rwy_lights,
                           taxi_lights, NULL, false ) ) {

                if ( geometry -> getNumKids() > 0 ) {
                    geometry->getKid( 0 )->setTravCallback(
                                                SSG_CALLBACK_PRETRAV,
                                                &FGTileMgr::tile_filter_cb );
                    new_tile -> addKid( geometry );
                }

                if ( vasi_lights -> getNumKids() > 0 )
                    vasi_lights_transform -> addKid( vasi_lights );

                if ( rwy_lights -> getNumKids() > 0 )
                    rwy_lights_transform -> addKid( rwy_lights );

                if ( taxi_lights -> getNumKids() > 0 )
                    taxi_lights_transform -> addKid( taxi_lights );
            }


        } else if (obj->type == OBJECT_SHARED || obj->type == OBJECT_STATIC) {
            // object loading is deferred to main render thread,
            // but lets figure out the paths right now.
            SGPath custom_path;
            if ( obj->type == OBJECT_STATIC ) {
                custom_path = obj->path;
            } else {
                custom_path = globals->get_fg_root();
            }
            custom_path.append( obj->name );

            sgCoord obj_pos;
            WorldCoordinate( &obj_pos, center, obj->lat, obj->lon, obj->elev, obj->hdg );

            ssgTransform *obj_trans = new ssgTransform;
            obj_trans->setTransform( &obj_pos );

            // wire as much of the scene graph together as we can
            new_tile->addKid( obj_trans );
            pending_models++;

            // push an entry onto the model load queue
            FGDeferredModel *dm
                = new FGDeferredModel( custom_path.str(),
                                       obj->path.str(),
                                       tile_bucket,
                                       this, obj_trans,
                                       obj->type == OBJECT_SHARED );
            FGTileMgr::model_ready( dm );


        } else if (obj->type == OBJECT_SIGN || obj->type == OBJECT_RUNWAY_SIGN) {
            ssgBranch *(*make_sign)(SGMaterialLib *, const string, const string);
            make_sign = obj->type == OBJECT_SIGN ? sgMakeSign : sgMakeRunwaySign;

            // load the object itself
            SGPath custom_path = obj->path;
            custom_path.append( obj->name );

            sgCoord obj_pos;
            WorldCoordinate( &obj_pos, center, obj->lat, obj->lon, obj->elev, obj->hdg );

            ssgTransform *obj_trans = new ssgTransform;
            obj_trans->setTransform( &obj_pos );

            ssgBranch *custom_obj
                = (*make_sign)(globals->get_matlib(), custom_path.str(), obj->name);

            // wire the pieces together
            if ( custom_obj != NULL ) {
                obj_trans -> addKid( custom_obj );
            }
            new_tile->addKid( obj_trans );

        }
        delete obj;
    }


    if ( new_tile != NULL ) {
        terra_range->addKid( new_tile );
    }

    terra_transform->addKid( terra_range );

    // calculate initial tile offset
    sgdVec3 sgdTrans;
    sgdSetVec3( sgdTrans, center.x(), center.y(), center.z() );
    terra_transform->setTransform( sgdTrans );

    // Add ground lights to scene graph if any exist
    gnd_lights_transform = NULL;
    gnd_lights_range = NULL;
    if ( light_pts->getNum() ) {
        SG_LOG( SG_TERRAIN, SG_DEBUG, "generating lights" );
        gnd_lights_transform = new ssgPlacementTransform;
        gnd_lights_range = new ssgRangeSelector;
        gnd_lights_brightness = new ssgSelector;
        ssgLeaf *lights;

        lights = gen_lights( globals->get_matlib(), light_pts, 4, 0.7 );
        gnd_lights_brightness->addKid( lights );

        lights = gen_lights( globals->get_matlib(), light_pts, 2, 0.85 );
        gnd_lights_brightness->addKid( lights );

        lights = gen_lights( globals->get_matlib(), light_pts, 1, 1.0 );
        gnd_lights_brightness->addKid( lights );

        gnd_lights_range->addKid( gnd_lights_brightness );
        gnd_lights_transform->addKid( gnd_lights_range );
        gnd_lights_transform->setTransform( sgdTrans );
    }

    // Update vasi lights transform
    if ( vasi_lights_transform->getNumKids() > 0 ) {
        vasi_lights_transform->setTransform( sgdTrans );
    }

    // Update runway lights transform
    if ( rwy_lights_transform->getNumKids() > 0 ) {
        rwy_lights_transform->setTransform( sgdTrans );
    }

     // Update taxi lights transform
    if ( taxi_lights_transform->getNumKids() > 0 ) {
        taxi_lights_transform->setTransform( sgdTrans );
    }

    delete light_pts;
}

void
FGTileEntry::makeDList( ssgBranch *b )
{
    int nb = b->getNumKids();
    for (int i = 0; i<nb; i++) {
        ssgEntity *e = b->getKid(i);
        if (e->isAKindOf(ssgTypeLeaf())) {
            ((ssgLeaf*)e)->makeDList();
        } else if (e->isAKindOf(ssgTypeBranch())) {
            makeDList( (ssgBranch*)e );
        }
    }
}

void
FGTileEntry::add_ssg_nodes( ssgBranch *terrain_branch,
                            ssgBranch *gnd_lights_branch,
                            ssgBranch *vasi_lights_branch,
                            ssgBranch *rwy_lights_branch,
                            ssgBranch *taxi_lights_branch )
{
    // bump up the ref count so we can remove this later without
    // having ssg try to free the memory.
#if PLIB_VERSION > 183
    if ( fgGetBool( "/sim/rendering/use-display-list", true ) ) {
        makeDList( terra_transform );
    }
#endif

    terrain_branch->addKid( terra_transform );
    globals->get_scenery()->register_placement_transform(terra_transform);

    SG_LOG( SG_TERRAIN, SG_DEBUG,
            "connected a tile into scene graph.  terra_transform = "
            << terra_transform );
    SG_LOG( SG_TERRAIN, SG_DEBUG, "num parents now = "
            << terra_transform->getNumParents() );

    if ( gnd_lights_transform != NULL ) {
        // bump up the ref count so we can remove this later without
        // having ssg try to free the memory.
        gnd_lights_branch->addKid( gnd_lights_transform );
        globals->get_scenery()->register_placement_transform(gnd_lights_transform);
    }

    if ( vasi_lights_transform != NULL ) {
        // bump up the ref count so we can remove this later without
        // having ssg try to free the memory.
        vasi_lights_selector->addKid( vasi_lights_transform );
        globals->get_scenery()->register_placement_transform(vasi_lights_transform);
        vasi_lights_branch->addKid( vasi_lights_selector );
    }

    if ( rwy_lights_transform != NULL ) {
        // bump up the ref count so we can remove this later without
        // having ssg try to free the memory.
        rwy_lights_selector->addKid( rwy_lights_transform );
        globals->get_scenery()->register_placement_transform(rwy_lights_transform);
        rwy_lights_branch->addKid( rwy_lights_selector );
    }

    if ( taxi_lights_transform != NULL ) {
        // bump up the ref count so we can remove this later without
        // having ssg try to free the memory.
        taxi_lights_selector->addKid( taxi_lights_transform );
        globals->get_scenery()->register_placement_transform(taxi_lights_transform);
        taxi_lights_branch->addKid( taxi_lights_selector );
    }

    loaded = true;
}


void
FGTileEntry::disconnect_ssg_nodes()
{
    SG_LOG( SG_TERRAIN, SG_DEBUG, "disconnecting ssg nodes" );

    if ( ! loaded ) {
        SG_LOG( SG_TERRAIN, SG_DEBUG, "removing a not-fully loaded tile!" );
    } else {
        SG_LOG( SG_TERRAIN, SG_DEBUG, "removing a fully loaded tile!  terra_transform = " << terra_transform );
    }
        
    // Unregister that one at the scenery manager
    globals->get_scenery()->unregister_placement_transform(terra_transform);

    // find the terrain branch parent
    int pcount = terra_transform->getNumParents();
    if ( pcount > 0 ) {
        // find the first parent (should only be one)
        ssgBranch *parent = terra_transform->getParent( 0 ) ;
        if( parent ) {
            // disconnect the tile (we previously ref()'d it so it
            // won't get freed now)
            parent->removeKid( terra_transform );
        } else {
            SG_LOG( SG_TERRAIN, SG_ALERT,
                    "parent pointer is NULL!  Dying" );
            exit(-1);
        }
    } else {
        SG_LOG( SG_TERRAIN, SG_ALERT,
                "Parent count is zero for an ssg tile!  Dying" );
        exit(-1);
    }

    // find the ground lighting branch
    if ( gnd_lights_transform ) {
        // Unregister that one at the scenery manager
        globals->get_scenery()->unregister_placement_transform(gnd_lights_transform);
        pcount = gnd_lights_transform->getNumParents();
        if ( pcount > 0 ) {
            // find the first parent (should only be one)
            ssgBranch *parent = gnd_lights_transform->getParent( 0 ) ;
            if( parent ) {
                // disconnect the light branch (we previously ref()'d
                // it so it won't get freed now)
                parent->removeKid( gnd_lights_transform );
            } else {
                SG_LOG( SG_TERRAIN, SG_ALERT,
                        "parent pointer is NULL!  Dying" );
                exit(-1);
            }
        } else {
            SG_LOG( SG_TERRAIN, SG_ALERT,
                    "Parent count is zero for an ssg light tile!  Dying" );
            exit(-1);
        }
    }

    // find the vasi lighting branch
    if ( vasi_lights_transform ) {
        // Unregister that one at the scenery manager
        globals->get_scenery()->unregister_placement_transform(vasi_lights_transform);
        pcount = vasi_lights_transform->getNumParents();
        if ( pcount > 0 ) {
            // find the first parent (should only be one)
            ssgBranch *parent = vasi_lights_transform->getParent( 0 ) ;
            if( parent ) {
                // disconnect the light branch (we previously ref()'d
                // it so it won't get freed now)
                parent->removeKid( vasi_lights_transform );
            } else {
                SG_LOG( SG_TERRAIN, SG_ALERT,
                        "parent pointer is NULL!  Dying" );
                exit(-1);
            }
        } else {
            SG_LOG( SG_TERRAIN, SG_ALERT,
                    "Parent count is zero for an ssg light tile!  Dying" );
            exit(-1);
        }
    }

    // find the runway lighting branch
    if ( rwy_lights_transform ) {
        // Unregister that one at the scenery manager
        globals->get_scenery()->unregister_placement_transform(rwy_lights_transform);
        pcount = rwy_lights_transform->getNumParents();
        if ( pcount > 0 ) {
            // find the first parent (should only be one)
            ssgBranch *parent = rwy_lights_transform->getParent( 0 ) ;
            if( parent ) {
                // disconnect the light branch (we previously ref()'d
                // it so it won't get freed now)
                parent->removeKid( rwy_lights_transform );
            } else {
                SG_LOG( SG_TERRAIN, SG_ALERT,
                        "parent pointer is NULL!  Dying" );
                exit(-1);
            }
        } else {
            SG_LOG( SG_TERRAIN, SG_ALERT,
                    "Parent count is zero for an ssg light tile!  Dying" );
            exit(-1);
        }
    }

    // find the taxi lighting branch
    if ( taxi_lights_transform ) {
        // Unregister that one at the scenery manager
        globals->get_scenery()->unregister_placement_transform(taxi_lights_transform);
        pcount = taxi_lights_transform->getNumParents();
        if ( pcount > 0 ) {
            // find the first parent (should only be one)
            ssgBranch *parent = taxi_lights_transform->getParent( 0 ) ;
            if( parent ) {
                // disconnect the light branch (we previously ref()'d
                // it so it won't get freed now)
                parent->removeKid( taxi_lights_transform );
            } else {
                SG_LOG( SG_TERRAIN, SG_ALERT,
                        "parent pointer is NULL!  Dying" );
                exit(-1);
            }
        } else {
            SG_LOG( SG_TERRAIN, SG_ALERT,
                    "Parent count is zero for an ssg light tile!  Dying" );
            exit(-1);
        }
    }
}
