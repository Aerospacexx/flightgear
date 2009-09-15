//
// light.cxx -- lighting routines
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

#include <cmath>

#include <simgear/constants.h>
#include <simgear/debug/logstream.hxx>
#include <simgear/math/interpolater.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/scene/sky/sky.hxx>
#include <simgear/screen/colors.hxx>

#include <Main/main.hxx>
#include <Main/globals.hxx>
#include <Main/fg_props.hxx>
#include <Main/renderer.hxx>
#include <Main/viewer.hxx>

#include "light.hxx"
#include "tmp.hxx"


// Constructor
FGLight::FGLight ()
    : _ambient_tbl( NULL ),
      _diffuse_tbl( NULL ),
      _specular_tbl( NULL ),
      _sky_tbl( NULL ),
      _sun_lon(0),
      _sun_lat(0),
      _moon_lon(0),
      _moon_gc_lat(0),
      _sunpos(0, 0, 0),
      _moonpos(0, 0, 0),
      _sun_vec(0, 0, 0, 0),
      _moon_vec(0, 0, 0, 0),
      _sun_vec_inv(0, 0, 0, 0),
      _moon_vec_inv(0, 0, 0, 0),
      _sun_angle(0),
      _moon_angle(0),
      _prev_sun_angle(0),
      _sun_rotation(0),
      _moon_rotation(0),
      _scene_ambient(0, 0, 0, 0),
      _scene_diffuse(0, 0, 0, 0),
      _scene_specular(0, 0, 0, 0),
      _sun_color(1, 1, 1, 1),
      _sky_color(0, 0, 0, 0),
      _fog_color(0, 0, 0, 0),
      _cloud_color(0, 0, 0, 0),
      _adj_fog_color(0, 0, 0, 0),
      _adj_sky_color(0, 0, 0, 0),
      _dt_total(0)
{
}

// Destructor
FGLight::~FGLight ()
{
    delete _ambient_tbl;
    delete _diffuse_tbl;
    delete _specular_tbl;
    delete _sky_tbl;
}


// initialize lighting tables
void FGLight::init () {
    SG_LOG( SG_EVENT, SG_INFO, 
	    "Initializing Lighting interpolation tables." );

    // build the path names of the lookup tables
    SGPath path( globals->get_fg_root() );

    // initialize ambient, diffuse and specular tables
    SGPath ambient_path = path;
    ambient_path.append( "Lighting/ambient" );
    _ambient_tbl = new SGInterpTable( ambient_path.str() );

    SGPath diffuse_path = path;
    diffuse_path.append( "Lighting/diffuse" );
    _diffuse_tbl = new SGInterpTable( diffuse_path.str() );

    SGPath specular_path = path;
    specular_path.append( "Lighting/specular" );
    _specular_tbl = new SGInterpTable( specular_path.str() );
    
    // initialize sky table
    SGPath sky_path = path;
    sky_path.append( "Lighting/sky" );
    _sky_tbl = new SGInterpTable( sky_path.str() );
}


void FGLight::reinit () {
    _prev_sun_angle = -9999.0;
    _dt_total = 0;

    delete _ambient_tbl;
    delete _diffuse_tbl;
    delete _specular_tbl;
    delete _sky_tbl;

    init();

    fgUpdateSunPos();

    update_sky_color();
    update_adj_fog_color();
}

void FGLight::bind () {
    SGPropertyNode *prop = globals->get_props();
    prop->tie("/sim/time/sun-angle-rad",SGRawValuePointer<double>(&_sun_angle));
    prop->tie("/rendering/scene/ambient/red",SGRawValuePointer<float>(&_scene_ambient[0]));
    prop->tie("/rendering/scene/ambient/green",SGRawValuePointer<float>(&_scene_ambient[1]));
    prop->tie("/rendering/scene/ambient/blue",SGRawValuePointer<float>(&_scene_ambient[2]));
    prop->tie("/rendering/scene/diffuse/red",SGRawValuePointer<float>(&_scene_diffuse[0]));
    prop->tie("/rendering/scene/diffuse/green",SGRawValuePointer<float>(&_scene_diffuse[1]));
    prop->tie("/rendering/scene/diffuse/blue",SGRawValuePointer<float>(&_scene_diffuse[2]));
    prop->tie("/rendering/scene/specular/red",SGRawValuePointer<float>(&_scene_specular[0]));
    prop->tie("/rendering/scene/specular/green",SGRawValuePointer<float>(&_scene_specular[1]));
    prop->tie("/rendering/scene/specular/blue",SGRawValuePointer<float>(&_scene_specular[2]));
    prop->tie("/rendering/dome/sun/red",SGRawValuePointer<float>(&_sun_color[0]));
    prop->tie("/rendering/dome/sun/green",SGRawValuePointer<float>(&_sun_color[1]));
    prop->tie("/rendering/dome/sun/blue",SGRawValuePointer<float>(&_sun_color[2]));
    prop->tie("/rendering/dome/sky/red",SGRawValuePointer<float>(&_sky_color[0]));
    prop->tie("/rendering/dome/sky/green",SGRawValuePointer<float>(&_sky_color[1]));
    prop->tie("/rendering/dome/sky/blue",SGRawValuePointer<float>(&_sky_color[2]));
    prop->tie("/rendering/dome/fog/red",SGRawValuePointer<float>(&_fog_color[0]));
    prop->tie("/rendering/dome/fog/green",SGRawValuePointer<float>(&_fog_color[1]));
    prop->tie("/rendering/dome/fog/blue",SGRawValuePointer<float>(&_fog_color[1]));
}

void FGLight::unbind () {
    SGPropertyNode *prop = globals->get_props();
    prop->untie("/sim/time/sun-angle-rad");
    prop->untie("/rendering/scene/ambient/red");
    prop->untie("/rendering/scene/ambient/green");
    prop->untie("/rendering/scene/ambient/blue");
    prop->untie("/rendering/scene/diffuse/red");
    prop->untie("/rendering/scene/diffuse/green");
    prop->untie("/rendering/scene/diffuse/blue");
    prop->untie("/rendering/scene/specular/red");
    prop->untie("/rendering/scene/specular/green");
    prop->untie("/rendering/scene/specular/blue");
    prop->untie("/rendering/dome/sun/red");
    prop->untie("/rendering/dome/sun/green");
    prop->untie("/rendering/dome/sun/blue");
    prop->untie("/rendering/dome/skyred");
    prop->untie("/rendering/dome/sky/green");
    prop->untie("/rendering/dome/sky/blue");
}


// update lighting parameters based on current sun position
void FGLight::update( double dt ) {

    _dt_total += dt;
    if (_dt_total >= 0.5) {
        _dt_total -= 0.5;
        fgUpdateSunPos();
    }

    update_adj_fog_color();

    if (_prev_sun_angle != _sun_angle) {
	_prev_sun_angle = _sun_angle;
        update_sky_color();
    }
}

void FGLight::update_sky_color () {
    // if the 4th field is 0.0, this specifies a direction ...
    // const GLfloat white[4]          = { 1.0,  1.0,  1.0,  1.0 };
    const GLfloat base_sky_color[4] = { 0.31, 0.43, 0.69, 1.0 };
    const GLfloat base_fog_color[4] = { 0.84, 0.87, 1.0,  1.0 };

    SG_LOG( SG_EVENT, SG_DEBUG, "Updating light parameters." );

    // calculate lighting parameters based on sun's relative angle to
    // local up
    static SGConstPropertyNode_ptr humidity = fgGetNode("/environment/relative-humidity");
    float av = humidity->getFloatValue() * 45;
    float visibility_log = log(av)/11.0;
    float visibility_inv = (45000.0 - av)/45000.0;

    float deg = _sun_angle * SGD_RADIANS_TO_DEGREES;
    SG_LOG( SG_EVENT, SG_DEBUG, "  Sun angle = " << deg );

    float ambient = _ambient_tbl->interpolate( deg ) + visibility_inv/10;
    float diffuse = _diffuse_tbl->interpolate( deg );
    float specular = _specular_tbl->interpolate( deg ) * visibility_log;
    float sky_brightness = _sky_tbl->interpolate( deg );

    SG_LOG( SG_EVENT, SG_DEBUG,
	    "  ambient = " << ambient << "  diffuse = " << diffuse 
	    << "  specular = " << specular << "  sky = " << sky_brightness );

    // sky_brightness = 0.15;  // used to force a dark sky (when testing)

    // set fog and cloud color
    float sqrt_sky_brightness = 1.0 - sqrt(1.0 - sky_brightness);
    _fog_color[0] = base_fog_color[0] * sqrt_sky_brightness;
    _fog_color[1] = base_fog_color[1] * sqrt_sky_brightness;
    _fog_color[2] = base_fog_color[2] * sqrt_sky_brightness;
    _fog_color[3] = base_fog_color[3];
    gamma_correct_rgb( _fog_color.data() );

    // set sky color
    _sky_color[0] = base_sky_color[0] * sky_brightness;
    _sky_color[1] = base_sky_color[1] * sky_brightness;
    _sky_color[2] = base_sky_color[2] * sky_brightness;
    _sky_color[3] = base_sky_color[3];
    gamma_correct_rgb( _sky_color.data() );

    _cloud_color[0] = base_fog_color[0] * sky_brightness;
    _cloud_color[1] = base_fog_color[1] * sky_brightness;
    _cloud_color[2] = base_fog_color[2] * sky_brightness;
    _cloud_color[3] = base_fog_color[3];

    // adjust the cloud colors for sunrise/sunset effects (darken them)
    if (_sun_angle > 1.0) {
       float sun2 = sqrt(_sun_angle);
       _cloud_color[0] /= sun2;
       _cloud_color[1] /= sun2;
       _cloud_color[2] /= sun2;
    }
    gamma_correct_rgb( _cloud_color.data() );

    _scene_ambient[0] = _fog_color[0] * ambient;
    _scene_ambient[1] = _fog_color[1] * ambient;
    _scene_ambient[2] = _fog_color[2] * ambient;
    _scene_ambient[3] = 1.0;
    gamma_correct_rgb( _scene_ambient.data() );

    SGVec4f sun_color = thesky->get_scene_color();
    float ndiff = (ambient + specular) / 2;
    float idiff = 1.0 - ndiff;
    _scene_diffuse[0] = (sun_color[0]*ndiff + _fog_color[0]*idiff) * diffuse;
    _scene_diffuse[1] = (sun_color[1]*ndiff + _fog_color[1]*idiff) * diffuse;
    _scene_diffuse[2] = (sun_color[2]*ndiff + _fog_color[2]*idiff) * diffuse;
    _scene_diffuse[3] = 1.0;
    gamma_correct_rgb( _scene_diffuse.data() );

    _scene_specular[0] = sun_color[0] * specular;
    _scene_specular[1] = sun_color[1] * specular;
    _scene_specular[2] = sun_color[2] * specular;
    _scene_specular[3] = 1.0;
    gamma_correct_rgb( _scene_specular.data() );

    sun_color = thesky->get_sun_color();
    _sun_color[0] = sun_color[0];
    _sun_color[1] = sun_color[1];
    _sun_color[2] = sun_color[2];
    _sun_color[3] = sun_color[3];
    gamma_correct_rgb( _sun_color.data() );
}


// calculate fog color adjusted for sunrise/sunset effects
void FGLight::update_adj_fog_color () {

    double pitch = globals->get_current_view()->getPitch_deg()
                     * SGD_DEGREES_TO_RADIANS;
    double pitch_offset = globals->get_current_view()-> getPitchOffset_deg()
                     * SGD_DEGREES_TO_RADIANS;
    double heading = globals->get_current_view()->getHeading_deg()
                     * SGD_DEGREES_TO_RADIANS;
    double heading_offset = globals->get_current_view()->getHeadingOffset_deg()
                            * SGD_DEGREES_TO_RADIANS;

    SG_LOG( SG_EVENT, SG_DEBUG, "Updating adjusted fog parameters." );

    // set fog color (we'll try to match the sunset color in the
    // direction we are looking

    // Do some sanity checking ...
    if ( _sun_rotation < -2.0 * SGD_2PI || _sun_rotation > 2.0 * SGD_2PI ) {
	SG_LOG( SG_EVENT, SG_ALERT, "Sun rotation bad = " << _sun_rotation );
	return;
    }

    if ( heading < -2.0 * SGD_2PI || heading > 2.0 * SGD_2PI ) {
	SG_LOG( SG_EVENT, SG_ALERT, "Heading rotation bad = " << heading );
	return;
    }

    if ( heading_offset < -2.0 * SGD_2PI || heading_offset > 2.0 * SGD_2PI ) {
	SG_LOG( SG_EVENT, SG_ALERT, "Heading offset bad = " << heading_offset );
	return;
    }

    double hor_rotation, vert_rotation;

    // first determine the difference between our view angle and local
    // direction to the sun
    vert_rotation = pitch + pitch_offset;
    hor_rotation = -(_sun_rotation + SGD_PI) - heading + heading_offset;
    if (hor_rotation < 0 )
       hor_rotation = fmod(hor_rotation, SGD_2PI) + SGD_2PI;
    else
       hor_rotation = fmod(hor_rotation, SGD_2PI);

    // revert to unmodified values before usign them.
    //
    SGVec4f sun_color = thesky->get_scene_color();

    gamma_restore_rgb( _fog_color.data() );
    gamma_restore_rgb( _sky_color.data() );

    // Calculate the fog color in the direction of the sun for
    // sunrise/sunset effects.
    //
    float s_red =   sun_color[0]*sun_color[0];
    float s_green = sun_color[1]*sun_color[1];
    float s_blue =  sun_color[2];

    // interpolate beween the sunrise/sunset color and the color
    // at the opposite direction of this effect. Take in account
    // the current visibility.
    //
    float av = thesky->get_visibility();
    if (av > 45000) av = 45000;

    float avf = 0.87 - (45000 - av) / 83333.33;
    float sif = 0.5 - cos(_sun_angle*2)/2;

    if (sif < 1e-4)
       sif = 1e-4;

    float rf1 = fabs((hor_rotation - SGD_PI) / SGD_PI);		// 0.0 .. 1.0
    float rf2 = avf * pow(rf1*rf1, 1/sif) * 1.0639;
    float rf3 = 1.0 - rf2;

    _adj_fog_color[0] = rf3 * _fog_color[0] + rf2 * s_red;
    _adj_fog_color[1] = rf3 * _fog_color[1] + rf2 * s_green;
    _adj_fog_color[2] = rf3 * _fog_color[2] + rf2 * s_blue;
    gamma_correct_rgb( _adj_fog_color.data() );

    // make sure the colors have their original value before they are being
    // used by the rest of the program.
    //
    gamma_correct_rgb( _fog_color.data() );
    gamma_correct_rgb( _sky_color.data() );
}

