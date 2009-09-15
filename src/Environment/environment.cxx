// environment.cxx -- routines to model the natural environment
//
// Written by David Megginson, started February 2002.
//
// Copyright (C) 2002  David Megginson - david@megginson.com
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

#include <math.h>

#include <simgear/constants.h>
#include <simgear/debug/logstream.hxx>
#include <simgear/math/interpolater.hxx>
#include <simgear/props/props.hxx>
#include <simgear/environment/visual_enviro.hxx>

#include <Main/fg_props.hxx>

#include "environment.hxx"



////////////////////////////////////////////////////////////////////////
// Atmosphere model.
////////////////////////////////////////////////////////////////////////

// Calculated based on the ISA standard day, as found at e.g.
// http://www.av8n.com/physics/altimetry.htm

// Each line of data has 3 elements:
//   Elevation (ft), 
//   temperature factor (dimensionless ratio of absolute temp), 
//   pressure factor (dimensionless ratio)
static double atmosphere_data[][3] = {
 {  -3000.00,   1.021,  1.1133 },
 {      0.00,   1.000,  1.0000 },
 {   2952.76,   0.980,  0.8978 },
 {   5905.51,   0.959,  0.8042 },
 {   8858.27,   0.939,  0.7187 },
 {  11811.02,   0.919,  0.6407 },
 {  14763.78,   0.898,  0.5697 },
 {  17716.54,   0.878,  0.5052 },
 {  20669.29,   0.858,  0.4468 },
 {  23622.05,   0.838,  0.3940 },
 {  26574.80,   0.817,  0.3463 },
 {  29527.56,   0.797,  0.3034 },
 {  32480.31,   0.777,  0.2649 },
 {  35433.07,   0.756,  0.2305 },
 {  38385.83,   0.752,  0.2000 },
 {  41338.58,   0.752,  0.1736 },
 {  44291.34,   0.752,  0.1506 },
 {  47244.09,   0.752,  0.1307 },
 {  50196.85,   0.752,  0.1134 },
 {  53149.61,   0.752,  0.0984 },
 {  56102.36,   0.752,  0.0854 },
 {  59055.12,   0.752,  0.0741 },
 {  62007.87,   0.752,  0.0643 },
 {  65000.00,   0.752,  0.0557 },
 {  68000.00,   0.754,  0.0482 },
 {  71000.00,   0.758,  0.0418 },
 {  74000.00,   0.761,  0.0362 },
 {  77000.00,   0.764,  0.0314 },
 {  80000.00,   0.767,  0.0273 },
 {  83000.00,   0.770,  0.0237 },
 {  86000.00,   0.773,  0.0206 },
 {  89000.00,   0.777,  0.0179 },
 {  92000.00,   0.780,  0.0156 },
 {  95000.00,   0.783,  0.0135 },
 {  98000.00,   0.786,  0.0118 },
 { 101000.00,   0.789,  0.0103 },
 { -1, -1, -1 }
};

static SGInterpTable * _temperature_degc_table = 0;
static SGInterpTable * _pressure_inhg_table = 0;

static void
_setup_tables ()
{
  if (_temperature_degc_table != 0)
      return;

  _temperature_degc_table = new SGInterpTable;
  _pressure_inhg_table = new SGInterpTable;

  for (int i = 0; atmosphere_data[i][0] != -1; i++) {
    _temperature_degc_table->addEntry(atmosphere_data[i][0],
				      atmosphere_data[i][1]);
    _pressure_inhg_table->addEntry(atmosphere_data[i][0],
				   atmosphere_data[i][2]);
  }
}



////////////////////////////////////////////////////////////////////////
// Implementation of FGEnvironment.
////////////////////////////////////////////////////////////////////////

void FGEnvironment::_init()
{
    elevation_ft = 0;
    visibility_m = 32000;
    temperature_sea_level_degc = 15;
    temperature_degc = 15;
    dewpoint_sea_level_degc = 5; // guess
    dewpoint_degc = 5;
    pressure_sea_level_inhg = 29.92;
    pressure_inhg = 29.92;
    turbulence_magnitude_norm = 0;
    turbulence_rate_hz = 1;
    wind_from_heading_deg = 0;
    wind_speed_kt = 0;
    wind_from_north_fps = 0;
    wind_from_east_fps = 0;
    wind_from_down_fps = 0;
    thermal_lift_fps = 0;
    ridge_lift_fps= 0;
    altitude_half_to_sun_m = 1000;
    altitude_tropo_top_m = 10000;
    _setup_tables();
    _recalc_density();
    _recalc_relative_humidity();
    live_update = true;
}

FGEnvironment::FGEnvironment()
{
    _init();
}

FGEnvironment::FGEnvironment (const FGEnvironment &env)
{
    _init();
    copy(env);
}

FGEnvironment::~FGEnvironment()
{
}

void
FGEnvironment::copy (const FGEnvironment &env)
{
    elevation_ft = env.elevation_ft;
    visibility_m = env.visibility_m;
    temperature_sea_level_degc = env.temperature_sea_level_degc;
    temperature_degc = env.temperature_degc;
    dewpoint_sea_level_degc = env.dewpoint_sea_level_degc;
    dewpoint_degc = env.dewpoint_degc;
    pressure_sea_level_inhg = env.pressure_sea_level_inhg;
    wind_from_heading_deg = env.wind_from_heading_deg;
    wind_speed_kt = env.wind_speed_kt;
    wind_from_north_fps = env.wind_from_north_fps;
    wind_from_east_fps = env.wind_from_east_fps;
    wind_from_down_fps = env.wind_from_down_fps;
    thermal_lift_fps = env.thermal_lift_fps;
    ridge_lift_fps= env.ridge_lift_fps;
    turbulence_magnitude_norm = env.turbulence_magnitude_norm;
    turbulence_rate_hz = env.turbulence_rate_hz;
}

static inline bool
maybe_copy_value (FGEnvironment * env, const SGPropertyNode * node,
                  const char * name, void (FGEnvironment::*setter)(double))
{
    const SGPropertyNode * child = node->getNode(name);
                                // fragile: depends on not being typed
                                // as a number
    if (child != 0 && child->hasValue() &&
        child->getStringValue()[0] != '\0') {
        (env->*setter)(child->getDoubleValue());
        return true;
    } else {
        return false;
    }
}

void
FGEnvironment::read (const SGPropertyNode * node)
{
    bool live_update = set_live_update( false );
    maybe_copy_value(this, node, "visibility-m",
                     &FGEnvironment::set_visibility_m);

    if (!maybe_copy_value(this, node, "temperature-sea-level-degc",
                          &FGEnvironment::set_temperature_sea_level_degc))
        maybe_copy_value(this, node, "temperature-degc",
                         &FGEnvironment::set_temperature_degc);

    if (!maybe_copy_value(this, node, "dewpoint-sea-level-degc",
                          &FGEnvironment::set_dewpoint_sea_level_degc))
        maybe_copy_value(this, node, "dewpoint-degc",
                         &FGEnvironment::set_dewpoint_degc);

    if (!maybe_copy_value(this, node, "pressure-sea-level-inhg",
                          &FGEnvironment::set_pressure_sea_level_inhg))
        maybe_copy_value(this, node, "pressure-inhg",
                         &FGEnvironment::set_pressure_inhg);

    maybe_copy_value(this, node, "wind-from-heading-deg",
                     &FGEnvironment::set_wind_from_heading_deg);

    maybe_copy_value(this, node, "wind-speed-kt",
                     &FGEnvironment::set_wind_speed_kt);

    maybe_copy_value(this, node, "elevation-ft",
                     &FGEnvironment::set_elevation_ft);

    maybe_copy_value(this, node, "turbulence/magnitude-norm",
                     &FGEnvironment::set_turbulence_magnitude_norm);

    maybe_copy_value(this, node, "turbulence/rate-hz",
                     &FGEnvironment::set_turbulence_rate_hz);
    // calculate derived properties here to avoid duplicate expensive computations
    _recalc_ne();
    _recalc_alt_temperature();
    _recalc_alt_dewpoint();
    _recalc_alt_pressure();
    _recalc_density();
    _recalc_relative_humidity();

    set_live_update(live_update);
}


double
FGEnvironment::get_visibility_m () const
{
  return visibility_m;
}

double
FGEnvironment::get_temperature_sea_level_degc () const
{
  return temperature_sea_level_degc;
}

double
FGEnvironment::get_temperature_degc () const
{
  return temperature_degc;
}

double
FGEnvironment::get_temperature_degf () const
{
  return (temperature_degc * 9.0 / 5.0) + 32.0;
}

double
FGEnvironment::get_dewpoint_sea_level_degc () const
{
  return dewpoint_sea_level_degc;
}

double
FGEnvironment::get_dewpoint_degc () const
{
  return dewpoint_degc;
}

double
FGEnvironment::get_pressure_sea_level_inhg () const
{
  return pressure_sea_level_inhg;
}

double
FGEnvironment::get_pressure_inhg () const
{
  return pressure_inhg;
}

double
FGEnvironment::get_density_slugft3 () const
{
  return density_slugft3;
}

double
FGEnvironment::get_relative_humidity () const
{
  return relative_humidity;
}

double
FGEnvironment::get_density_tropo_avg_kgm3 () const
{
  return density_tropo_avg_kgm3;
}

double
FGEnvironment::get_altitude_half_to_sun_m () const
{
  return altitude_half_to_sun_m;
}

double
FGEnvironment::get_altitude_tropo_top_m () const
{
  return altitude_tropo_top_m;
}

double
FGEnvironment::get_wind_from_heading_deg () const
{
  return wind_from_heading_deg;
}

double
FGEnvironment::get_wind_speed_kt () const
{
  return wind_speed_kt;
}

double
FGEnvironment::get_wind_from_north_fps () const
{
  return wind_from_north_fps;
}

double
FGEnvironment::get_wind_from_east_fps () const
{
  return wind_from_east_fps;
}

double
FGEnvironment::get_wind_from_down_fps () const
{
  return wind_from_down_fps;
}

double
FGEnvironment::get_thermal_lift_fps () const
{
  return thermal_lift_fps;
}

double
FGEnvironment::get_ridge_lift_fps () const
{
  return ridge_lift_fps;
}

double
FGEnvironment::get_turbulence_magnitude_norm () const
{
  if( sgEnviro.get_turbulence_enable_state() )
    if (fgGetBool("/environment/params/real-world-weather-fetch") == true)
      return sgEnviro.get_cloud_turbulence();
  return turbulence_magnitude_norm;
}

double
FGEnvironment::get_turbulence_rate_hz () const
{
  return turbulence_rate_hz;
}

double
FGEnvironment::get_elevation_ft () const
{
  return elevation_ft;
}

void
FGEnvironment::set_visibility_m (double v)
{
  visibility_m = v;
}

void
FGEnvironment::set_temperature_sea_level_degc (double t)
{
  temperature_sea_level_degc = t;
  if (dewpoint_sea_level_degc > t)
      dewpoint_sea_level_degc = t;
  if( live_update ) {
    _recalc_alt_temperature();
    _recalc_density();
  }
}

void
FGEnvironment::set_temperature_degc (double t)
{
  temperature_degc = t;
  if( live_update ) {
    _recalc_sl_temperature();
    _recalc_density();
    _recalc_relative_humidity();
  }
}

void
FGEnvironment::set_dewpoint_sea_level_degc (double t)
{
  dewpoint_sea_level_degc = t;
  if (temperature_sea_level_degc < t)
      temperature_sea_level_degc = t;
  if( live_update ) {
    _recalc_alt_dewpoint();
    _recalc_density();
  }
}

void
FGEnvironment::set_dewpoint_degc (double t)
{
  dewpoint_degc = t;
  if( live_update ) {
    _recalc_sl_dewpoint();
    _recalc_density();
    _recalc_relative_humidity();
  }
}

void
FGEnvironment::set_pressure_sea_level_inhg (double p)
{
  pressure_sea_level_inhg = p;
  if( live_update ) {
    _recalc_alt_pressure();
    _recalc_density();
  }
}

void
FGEnvironment::set_pressure_inhg (double p)
{
  pressure_inhg = p;
  if( live_update ) {
    _recalc_sl_pressure();
    _recalc_density();
  }
}

void
FGEnvironment::set_wind_from_heading_deg (double h)
{
  wind_from_heading_deg = h;
  if( live_update ) {
    _recalc_ne();
  }
}

void
FGEnvironment::set_wind_speed_kt (double s)
{
  wind_speed_kt = s;
  if( live_update ) {
    _recalc_ne();
  }
}

void
FGEnvironment::set_wind_from_north_fps (double n)
{
  wind_from_north_fps = n;
  if( live_update ) {
    _recalc_hdgspd();
  }
}

void
FGEnvironment::set_wind_from_east_fps (double e)
{
  wind_from_east_fps = e;
  if( live_update ) {
    _recalc_hdgspd();
  }
}

void
FGEnvironment::set_wind_from_down_fps (double d)
{
  wind_from_down_fps = d;
  if( live_update ) {
    _recalc_hdgspd();
  }
}

void
FGEnvironment::set_thermal_lift_fps (double th)
{
  thermal_lift_fps = th;
  if( live_update ) {
    _recalc_updraft();
  }
}

void
FGEnvironment::set_ridge_lift_fps (double ri)
{
  ridge_lift_fps = ri;
  if( live_update ) {
    _recalc_updraft();
}
}

void
FGEnvironment::set_turbulence_magnitude_norm (double t)
{
  turbulence_magnitude_norm = t;
}

void
FGEnvironment::set_turbulence_rate_hz (double r)
{
  turbulence_rate_hz = r;
}

void
FGEnvironment::set_elevation_ft (double e)
{
  elevation_ft = e;
  if( live_update ) {
    _recalc_alt_temperature();
    _recalc_alt_dewpoint();
    _recalc_alt_pressure();
    _recalc_density();
    _recalc_relative_humidity();
  }
}

void
FGEnvironment::set_altitude_half_to_sun_m (double alt)
{
  altitude_half_to_sun_m = alt;
  if( live_update ) {
    _recalc_density_tropo_avg_kgm3();
  }
}

void
FGEnvironment::set_altitude_tropo_top_m (double alt)
{
  altitude_tropo_top_m = alt;
  if( live_update ) {
    _recalc_density_tropo_avg_kgm3();
  }
}


void
FGEnvironment::_recalc_hdgspd ()
{
  double angle_rad;

  if (wind_from_east_fps == 0) {
    angle_rad = (wind_from_north_fps >= 0 ? SGD_PI_2 : -SGD_PI_2);
  } else {
    angle_rad = atan(wind_from_north_fps/wind_from_east_fps);
  }
  wind_from_heading_deg = angle_rad * SGD_RADIANS_TO_DEGREES;
  if (wind_from_east_fps >= 0)
    wind_from_heading_deg = 90 - wind_from_heading_deg;
  else
    wind_from_heading_deg = 270 - wind_from_heading_deg;

#if 0
  // FIXME: Windspeed can become negative with these formulas.
  //        which can cause problems for animations that rely
  //        on the windspeed property.
  if (angle_rad == 0)
    wind_speed_kt = fabs(wind_from_east_fps
			 * SG_METER_TO_NM * SG_FEET_TO_METER * 3600);
  else
    wind_speed_kt = (wind_from_north_fps / sin(angle_rad))
      * SG_METER_TO_NM * SG_FEET_TO_METER * 3600;
#else
  wind_speed_kt = sqrt(wind_from_north_fps * wind_from_north_fps +
                       wind_from_east_fps * wind_from_east_fps) 
                  * SG_METER_TO_NM * SG_FEET_TO_METER * 3600;
#endif
}

void
FGEnvironment::_recalc_ne ()
{
  double speed_fps =
    wind_speed_kt * SG_NM_TO_METER * SG_METER_TO_FEET * (1.0/3600);

  wind_from_north_fps = speed_fps *
    cos(wind_from_heading_deg * SGD_DEGREES_TO_RADIANS);
  wind_from_east_fps = speed_fps *
    sin(wind_from_heading_deg * SGD_DEGREES_TO_RADIANS);
}

void
FGEnvironment::_recalc_updraft ()
{
  wind_from_down_fps = thermal_lift_fps + ridge_lift_fps ;
}

void
FGEnvironment::_recalc_sl_temperature ()
{
  // If we're in the stratosphere, leave sea-level temp alone
  if (elevation_ft < 38000) {
    temperature_sea_level_degc = (temperature_degc + 273.15)
        / _temperature_degc_table->interpolate(elevation_ft)
      - 273.15;
  }
}

void
FGEnvironment::_recalc_alt_temperature ()
{
  if (elevation_ft < 38000) {
    temperature_degc = (temperature_sea_level_degc + 273.15) *
        _temperature_degc_table->interpolate(elevation_ft) - 273.15;
  } else {
    temperature_degc = -56.49;	// Stratosphere is constant
  }
}

void
FGEnvironment::_recalc_sl_dewpoint ()
{
				// 0.2degC/1000ft
				// FIXME: this will work only for low
				// elevations
  dewpoint_sea_level_degc = dewpoint_degc + (elevation_ft * .0002);
  if (dewpoint_sea_level_degc > temperature_sea_level_degc)
    dewpoint_sea_level_degc = temperature_sea_level_degc;
}

void
FGEnvironment::_recalc_alt_dewpoint ()
{
				// 0.2degC/1000ft
				// FIXME: this will work only for low
				// elevations
  dewpoint_degc = dewpoint_sea_level_degc + (elevation_ft * .0002);
  if (dewpoint_degc > temperature_degc)
    dewpoint_degc = temperature_degc;
}

void
FGEnvironment::_recalc_sl_pressure ()
{
  pressure_sea_level_inhg =
    pressure_inhg / _pressure_inhg_table->interpolate(elevation_ft);
}

void
FGEnvironment::_recalc_alt_pressure ()
{
  pressure_inhg =
    pressure_sea_level_inhg * _pressure_inhg_table->interpolate(elevation_ft);
}

void
FGEnvironment::_recalc_density ()
{
  double pressure_psf = pressure_inhg * 70.7487;
  
  // adjust for humidity
  // calculations taken from USA Today (oops!) at
  // http://www.usatoday.com/weather/basics/density-calculations.htm
  double temperature_degk = temperature_degc + 273.15;
  double pressure_mb = pressure_inhg * 33.86;
  double vapor_pressure_mb =
    6.11 * pow(10.0, 7.5 * dewpoint_degc / (237.7 + dewpoint_degc));
  double virtual_temperature_degk = temperature_degk / (1 - (vapor_pressure_mb / pressure_mb) * (1.0 - 0.622));
  double virtual_temperature_degr = virtual_temperature_degk * 1.8;

  density_slugft3 = pressure_psf / (virtual_temperature_degr * 1718);
  _recalc_density_tropo_avg_kgm3();
}

// This is used to calculate the average density on the path 
// of sunlight to the observer for calculating sun-color
void
FGEnvironment::_recalc_density_tropo_avg_kgm3 ()
{
  double pressure_mb = pressure_inhg * 33.86;
  double vaporpressure = 6.11 * pow(10.0, ((7.5 * dewpoint_degc) / (237.7 + dewpoint_degc)));

  double virtual_temp = (temperature_degc + 273.15) / (1 - 0.379 * (vaporpressure/pressure_mb));

  double density_half = (100 * pressure_mb * exp(-altitude_half_to_sun_m / 8000))
      / (287.05 * virtual_temp);
  double density_tropo = (100 * pressure_mb * exp((-1 * altitude_tropo_top_m) / 8000))
      / ( 287.05 * virtual_temp);

  density_tropo_avg_kgm3 = ((density_slugft3 * 515.379) + density_half + density_tropo) / 3;
}

void
FGEnvironment::_recalc_relative_humidity ()
{
/*
  double vaporpressure = 6.11 * pow(10.0, ((7.5 * dewpoint_degc) / ( 237.7 + dewpoint_degc)));
  double sat_vaporpressure = 6.11 * pow(10.0, ((7.5 * temperature_degc)
      / ( 237.7 + temperature_degc)) );
  relative_humidity = 100 * vaporpressure / sat_vaporpressure ;

  with a little algebra, this gets the same result and spares two multiplications and one pow()
*/
  double a = (7.5 * dewpoint_degc)    / ( 237.7 + dewpoint_degc);
  double b = (7.5 * temperature_degc) / ( 237.7 + temperature_degc);
  relative_humidity = 100 * pow(10,a-b); 
}

bool
FGEnvironment::set_live_update( bool _live_update )
{
  bool b = live_update;
  live_update = _live_update;
  return b;
}


////////////////////////////////////////////////////////////////////////
// Functions.
////////////////////////////////////////////////////////////////////////

static inline double
do_interp (double a, double b, double fraction)
{
    double retval = (a + ((b - a) * fraction));
    return retval;
}

static inline double
do_interp_deg (double a, double b, double fraction)
{
    a = fmod(a, 360);
    b = fmod(b, 360);
    if (fabs(b-a) > 180) {
        if (a < b)
            a += 360;
        else
            b += 360;
    }
    return fmod(do_interp(a, b, fraction), 360);
}

void
interpolate (const FGEnvironment * env1, const FGEnvironment * env2,
             double fraction, FGEnvironment * result)
{
    // don't calculate each internal property every time we set a single value
    // we trigger that at the end of the interpolation process
    bool live_update = result->set_live_update( false );

    result->set_visibility_m
        (do_interp(env1->get_visibility_m(),
                   env2->get_visibility_m(),
                   fraction));

    result->set_temperature_sea_level_degc
        (do_interp(env1->get_temperature_sea_level_degc(),
                   env2->get_temperature_sea_level_degc(),
                   fraction));

    result->set_dewpoint_degc
        (do_interp(env1->get_dewpoint_sea_level_degc(),
                   env2->get_dewpoint_sea_level_degc(),
                   fraction));

    result->set_pressure_sea_level_inhg
        (do_interp(env1->get_pressure_sea_level_inhg(),
                   env2->get_pressure_sea_level_inhg(),
                   fraction));

    result->set_wind_from_heading_deg
        (do_interp_deg(env1->get_wind_from_heading_deg(),
                       env2->get_wind_from_heading_deg(),
                       fraction));

    result->set_wind_speed_kt
        (do_interp(env1->get_wind_speed_kt(),
                   env2->get_wind_speed_kt(),
                   fraction));

    result->set_elevation_ft
        (do_interp(env1->get_elevation_ft(),
                   env2->get_elevation_ft(),
                   fraction));

    result->set_turbulence_magnitude_norm
        (do_interp(env1->get_turbulence_magnitude_norm(),
                   env2->get_turbulence_magnitude_norm(),
                   fraction));

    result->set_turbulence_rate_hz
        (do_interp(env1->get_turbulence_rate_hz(),
                   env2->get_turbulence_rate_hz(),
                   fraction));

    // calculate derived properties here to avoid duplicate expensive computations
    result->_recalc_ne();
    result->_recalc_alt_temperature();
    result->_recalc_alt_dewpoint();
    result->_recalc_alt_pressure();
    result->_recalc_density();
    result->_recalc_relative_humidity();

    result->set_live_update(live_update);
}

// end of environment.cxx
