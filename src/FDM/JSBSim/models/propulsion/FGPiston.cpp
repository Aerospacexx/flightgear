/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

 Module:       FGPiston.cpp
 Author:       Jon S. Berndt, JSBSim framework
               Dave Luff, Piston engine model
               Ronald Jensen, Piston engine model
 Date started: 09/12/2000
 Purpose:      This module models a Piston engine

 ------------- Copyright (C) 2000  Jon S. Berndt (jsb@hal-pc.org) --------------

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU Lesser General Public License as published by the Free Software
 Foundation; either version 2 of the License, or (at your option) any later
 version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 details.

 You should have received a copy of the GNU Lesser General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 Place - Suite 330, Boston, MA  02111-1307, USA.

 Further information about the GNU Lesser General Public License can also be found on
 the world wide web at http://www.gnu.org.

FUNCTIONAL DESCRIPTION
--------------------------------------------------------------------------------

This class descends from the FGEngine class and models a Piston engine based on
parameters given in the engine config file for this class

HISTORY
--------------------------------------------------------------------------------
09/12/2000  JSB  Created

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
INCLUDES
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include <sstream>

#include "FGPiston.h"
#include <models/FGPropulsion.h>
#include "FGPropeller.h"

namespace JSBSim {

static const char *IdSrc = "$Id$";
static const char *IdHdr = ID_PISTON;

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
CLASS IMPLEMENTATION
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

FGPiston::FGPiston(FGFDMExec* exec, Element* el, int engine_number)
  : FGEngine(exec, el, engine_number),
  R_air(287.3),                  // Gas constant for air J/Kg/K
  rho_fuel(800),                 // estimate
  calorific_value_fuel(47.3e6),
  Cp_air(1005),                  // Specific heat (constant pressure) J/Kg/K
  Cp_fuel(1700)
{
  string token;

  // Defaults and initializations

  Type = etPiston;
  dt = State->Getdt();

  // These items are read from the configuration file

  Cycles = 2;
  IdleRPM = 600;
  MaxRPM = 2800;
  Displacement = 360;
  SparkFailDrop = 1.0;
  MaxHP = 200;
  MinManifoldPressure_inHg = 6.5;
  MaxManifoldPressure_inHg = 28.5;
  nominal_BSFC = -1;

  // Initialisation

  // Zeroth-order approximation to volumetric efficiency:
  volumetric_efficiency_0 = 0.85;
  // Additional third-order loss:
  volumetric_loss_3 = 0.1;

  // These are internal program variables

  crank_counter = 0;
  Magnetos = 0;
  minMAP = 21950;
  maxMAP = 96250;

  ResetToIC();

  // Supercharging
  BoostSpeeds = 0;  // Default to no supercharging
  BoostSpeed = 0;
  Boosted = false;
  BoostOverride = 0;
  bBoostOverride = false;
  bTakeoffBoost = false;
  TakeoffBoost = 0.0;   // Default to no extra takeoff-boost
  int i;
  for (i=0; i<FG_MAX_BOOST_SPEEDS; i++) {
    RatedBoost[i] = 0.0;
    RatedPower[i] = 0.0;
    RatedAltitude[i] = 0.0;
    BoostMul[i] = 1.0;
    RatedMAP[i] = 100000;
    RatedRPM[i] = 2500;
    TakeoffMAP[i] = 100000;
  }
  for (i=0; i<FG_MAX_BOOST_SPEEDS-1; i++) {
    BoostSwitchAltitude[i] = 0.0;
    BoostSwitchPressure[i] = 0.0;
  }

  // First column is thi, second is neta (combustion efficiency)
  Lookup_Combustion_Efficiency = new FGTable(12);
  *Lookup_Combustion_Efficiency << 0.00 << 0.980;
  *Lookup_Combustion_Efficiency << 0.90 << 0.980;
  *Lookup_Combustion_Efficiency << 1.00 << 0.970;
  *Lookup_Combustion_Efficiency << 1.05 << 0.950;
  *Lookup_Combustion_Efficiency << 1.10 << 0.900;
  *Lookup_Combustion_Efficiency << 1.15 << 0.850;
  *Lookup_Combustion_Efficiency << 1.20 << 0.790;
  *Lookup_Combustion_Efficiency << 1.30 << 0.700;
  *Lookup_Combustion_Efficiency << 1.40 << 0.630;
  *Lookup_Combustion_Efficiency << 1.50 << 0.570;
  *Lookup_Combustion_Efficiency << 1.60 << 0.525;
  *Lookup_Combustion_Efficiency << 2.00 << 0.345;

  Power_Mixture_Correlation = new FGTable(13);
  *Power_Mixture_Correlation << 9.19 << 0.780;        // 14.7/1.6  (rich)
  *Power_Mixture_Correlation << 10 <<  0.860;
  *Power_Mixture_Correlation << 11 <<  0.935;
  *Power_Mixture_Correlation << 12 <<  0.980;         // 14.7/1.3
  *Power_Mixture_Correlation << 13 <<  1.000;         // 14.7/1.13
  *Power_Mixture_Correlation << 14 <<  0.990;
  *Power_Mixture_Correlation << 15 <<  0.964;
  *Power_Mixture_Correlation << 16 <<  0.925;
  *Power_Mixture_Correlation << 17 <<  0.880;
  *Power_Mixture_Correlation << 18 <<  0.830;
  *Power_Mixture_Correlation << 19 <<  0.785;
  *Power_Mixture_Correlation << 20 <<  0.740;
  *Power_Mixture_Correlation << 24.5 << 0.58;         // 14.7/0.6  (lean)

// See mixture.gnumeric
// Peaks equal to 1.0 at stoichiometry.
  Power_per_Fuel_vs_AFR = new FGTable(15);
  *Power_per_Fuel_vs_AFR <<	4	<<	0	;
  *Power_per_Fuel_vs_AFR <<	7.19	<<	0.414736203652005	;
  *Power_per_Fuel_vs_AFR <<	8	<<	0.518821139065232	;
  *Power_per_Fuel_vs_AFR <<	9	<<	0.645276689896839	;
  *Power_per_Fuel_vs_AFR <<	10	<<	0.758579016390118	;
  *Power_per_Fuel_vs_AFR <<	11	<<	0.854913587425195	;
  *Power_per_Fuel_vs_AFR <<	12	<<	0.921452629985649	;
  *Power_per_Fuel_vs_AFR <<	13	<<	0.966863860291293	;
  *Power_per_Fuel_vs_AFR <<	14	<<	0.990895028150458	;
  *Power_per_Fuel_vs_AFR <<	15	<<	1	;
  *Power_per_Fuel_vs_AFR <<	16	<<	0.994359043402075	;
  *Power_per_Fuel_vs_AFR <<	17	<<	0.988148108901967	;
  *Power_per_Fuel_vs_AFR <<	18	<<	0.958935665626131	;
  *Power_per_Fuel_vs_AFR <<	22.5	<<	0.670759865554173	;
  *Power_per_Fuel_vs_AFR <<	26	<<	0	;

  Mixture_Efficiency_Correlation = new FGTable(15);
  *Mixture_Efficiency_Correlation << 0.05000 << 0.00000;
  *Mixture_Efficiency_Correlation << 0.05137 << 0.00862;
  *Mixture_Efficiency_Correlation << 0.05179 << 0.21552;
  *Mixture_Efficiency_Correlation << 0.05430 << 0.48276;
  *Mixture_Efficiency_Correlation << 0.05842 << 0.70690;
  *Mixture_Efficiency_Correlation << 0.06312 << 0.83621;
  *Mixture_Efficiency_Correlation << 0.06942 << 0.93103;
  *Mixture_Efficiency_Correlation << 0.07786 << 1.00000;
  *Mixture_Efficiency_Correlation << 0.08845 << 1.00000;
  *Mixture_Efficiency_Correlation << 0.09270 << 0.98276;
  *Mixture_Efficiency_Correlation << 0.10120 << 0.93103;
  *Mixture_Efficiency_Correlation << 0.11455 << 0.72414;
  *Mixture_Efficiency_Correlation << 0.12158 << 0.45690;
  *Mixture_Efficiency_Correlation << 0.12435 << 0.23276;
  *Mixture_Efficiency_Correlation << 0.12500 << 0.00000;


/*
Manifold_Pressure_Lookup = new

        0       0.2      0.4      0.6      0.8     1
0        1.0000    1.0000    1.0000    1.0000    1.0000    1.0000
1000    0.7778    0.8212    0.8647    0.9081    0.9516    0.9950
2000    0.5556    0.6424    0.7293    0.8162    0.9031    0.9900
3000    0.3333    0.4637    0.5940    0.7243    0.8547    0.9850
4000    0.2000    0.2849    0.4587    0.6324    0.8062    0.9800
5000    0.2000    0.2000    0.3233    0.5406    0.7578    0.9750
6000    0.2000    0.2000    0.2000    0.4487    0.7093    0.9700
7000    0.2000    0.2000    0.2000    0.2000    0.4570    0.7611
8000    0.2000    0.2000    0.2000    0.2000    0.2047    0.5522
*/

  // Read inputs from engine data file where present.

  if (el->FindElement("minmp")) // Should have ELSE statement telling default value used?
    MinManifoldPressure_inHg = el->FindElementValueAsNumberConvertTo("minmp","INHG");
  if (el->FindElement("maxmp"))
    MaxManifoldPressure_inHg = el->FindElementValueAsNumberConvertTo("maxmp","INHG");
  if (el->FindElement("displacement"))
    Displacement = el->FindElementValueAsNumberConvertTo("displacement","IN3");
  if (el->FindElement("maxhp"))
    MaxHP = el->FindElementValueAsNumberConvertTo("maxhp","HP");
  if (el->FindElement("sparkfaildrop"))
    SparkFailDrop = Constrain(0, 1 - el->FindElementValueAsNumber("sparkfaildrop"), 1);
  if (el->FindElement("cycles"))
    Cycles = el->FindElementValueAsNumber("cycles");
  if (el->FindElement("idlerpm"))
    IdleRPM = el->FindElementValueAsNumber("idlerpm");
  if (el->FindElement("maxrpm"))
    MaxRPM = el->FindElementValueAsNumber("maxrpm");
  if (el->FindElement("maxthrottle"))
    MaxThrottle = el->FindElementValueAsNumber("maxthrottle");
  if (el->FindElement("minthrottle"))
    MinThrottle = el->FindElementValueAsNumber("minthrottle");
  if (el->FindElement("bsfc"))
    nominal_BSFC = el->FindElementValueAsNumberConvertTo("bsfc", "LBS/HP*HR");
  if (el->FindElement("volumetric-efficiency"))
    volumetric_efficiency_0 = el->FindElementValueAsNumber("volumetric-efficiency_0");
  if (el->FindElement("numboostspeeds")) { // Turbo- and super-charging parameters
    BoostSpeeds = (int)el->FindElementValueAsNumber("numboostspeeds");
    if (el->FindElement("boostoverride"))
      BoostOverride = (int)el->FindElementValueAsNumber("boostoverride");
    if (el->FindElement("takeoffboost"))
      TakeoffBoost = el->FindElementValueAsNumberConvertTo("takeoffboost", "PSI");
    if (el->FindElement("ratedboost1"))
      RatedBoost[0] = el->FindElementValueAsNumberConvertTo("ratedboost1", "PSI");
    if (el->FindElement("ratedboost2"))
      RatedBoost[1] = el->FindElementValueAsNumberConvertTo("ratedboost2", "PSI");
    if (el->FindElement("ratedboost3"))
      RatedBoost[2] = el->FindElementValueAsNumberConvertTo("ratedboost3", "PSI");
    if (el->FindElement("ratedpower1"))
      RatedPower[0] = el->FindElementValueAsNumberConvertTo("ratedpower1", "HP");
    if (el->FindElement("ratedpower2"))
      RatedPower[1] = el->FindElementValueAsNumberConvertTo("ratedpower2", "HP");
    if (el->FindElement("ratedpower3"))
      RatedPower[2] = el->FindElementValueAsNumberConvertTo("ratedpower3", "HP");
    if (el->FindElement("ratedrpm1"))
      RatedRPM[0] = el->FindElementValueAsNumber("ratedrpm1");
    if (el->FindElement("ratedrpm2"))
      RatedRPM[1] = el->FindElementValueAsNumber("ratedrpm2");
    if (el->FindElement("ratedrpm3"))
      RatedRPM[2] = el->FindElementValueAsNumber("ratedrpm3");
    if (el->FindElement("ratedaltitude1"))
      RatedAltitude[0] = el->FindElementValueAsNumberConvertTo("ratedaltitude1", "FT");
    if (el->FindElement("ratedaltitude2"))
      RatedAltitude[1] = el->FindElementValueAsNumberConvertTo("ratedaltitude2", "FT");
    if (el->FindElement("ratedaltitude3"))
      RatedAltitude[2] = el->FindElementValueAsNumberConvertTo("ratedaltitude3", "FT");
  }

  if ( MaxManifoldPressure_inHg > 29.9 )
    MaxManifoldPressure_inHg = 29.9;    // Don't allow boosting with a bogus number
#ifdef UNCOMPREHENDED
/* I don't know how this was supposed to work. */
/* It could be adjusted to work for any one engine */
/* but I don't see how it could work for any two different engines. */
  MaxManifoldPressure_Percent = MaxManifoldPressure_inHg / 29.92;
  // Create a nominal_BSFC to match the engine if not provided
  if (nominal_BSFC < 0) {
      double fudge(9411);       // I have no idea where this came from
      nominal_BSFC = Displacement * MaxRPM * volumetric_efficiency_0 * (1-volumetric_loss_3) 
        / fudge / MaxHP;
// Harsh (third order) penalty for engines with high maxmp:
      nominal_BSFC *= MaxManifoldPressure_Percent;
      nominal_BSFC *= MaxManifoldPressure_Percent;
      nominal_BSFC *= MaxManifoldPressure_Percent;
  }
#endif
// Practically all normally-aspirated Lycomings have a 
// BSFC of 0.42 +- 0.01 when leaned for max efficiency.
// Other engines may differ ... in which case they should
// say so in their engine.xml file.
  if (nominal_BSFC < 0.) nominal_BSFC = 0.42;
#if 0
    cout << "Nominal (stoichiometric) BSFC: " << nominal_BSFC << endl;
#endif

  char property_name[80];
  snprintf(property_name, 80, "propulsion/engine[%d]/power-hp", EngineNumber);
  PropertyManager->Tie(property_name, &HP);
  snprintf(property_name, 80, "propulsion/engine[%d]/bsfc-lbs_hphr", EngineNumber);
  PropertyManager->Tie(property_name, &nominal_BSFC);
  snprintf(property_name, 80, "propulsion/engine[%d]/volumetric-efficiency", EngineNumber);
  PropertyManager->Tie(property_name, &volumetric_efficiency_0);
  minMAP = MinManifoldPressure_inHg * inhgtopa;  // inHg to Pa
  maxMAP = MaxManifoldPressure_inHg * inhgtopa;
  StarterHP = sqrt(MaxHP) * 0.4;

  // Set up and sanity-check the turbo/supercharging configuration based on the input values.
  if (TakeoffBoost > RatedBoost[0]) bTakeoffBoost = true;
  for (i=0; i<BoostSpeeds; ++i) {
    bool bad = false;
    if (RatedBoost[i] <= 0.0) bad = true;
    if (RatedPower[i] <= 0.0) bad = true;
    if (RatedAltitude[i] < 0.0) bad = true;  // 0.0 is deliberately allowed - this corresponds to unregulated supercharging.
    if (i > 0 && RatedAltitude[i] < RatedAltitude[i - 1]) bad = true;
    if (bad) {
      // We can't recover from the above - don't use this supercharger speed.
      BoostSpeeds--;
      // TODO - put out a massive error message!
      break;
    }
    // Now sanity-check stuff that is recoverable.
    if (i < BoostSpeeds - 1) {
      if (BoostSwitchAltitude[i] < RatedAltitude[i]) {
        // TODO - put out an error message
        // But we can also make a reasonable estimate, as below.
        BoostSwitchAltitude[i] = RatedAltitude[i] + 1000;
      }
      BoostSwitchPressure[i] = Atmosphere->GetPressure(BoostSwitchAltitude[i]) * psftopa;
      //cout << "BoostSwitchAlt = " << BoostSwitchAltitude[i] << ", pressure = " << BoostSwitchPressure[i] << '\n';
      // Assume there is some hysteresis on the supercharger gear switch, and guess the value for now
      BoostSwitchHysteresis = 1000;
    }
    // Now work out the supercharger pressure multiplier of this speed from the rated boost and altitude.
    RatedMAP[i] = Atmosphere->GetPressureSL() * psftopa + RatedBoost[i] * 6895;  // psi*6895 = Pa.
    // Sometimes a separate BCV setting for takeoff or extra power is fitted.
    if (TakeoffBoost > RatedBoost[0]) {
      // Assume that the effect on the BCV is the same whichever speed is in use.
      TakeoffMAP[i] = RatedMAP[i] + ((TakeoffBoost - RatedBoost[0]) * 6895);
      bTakeoffBoost = true;
    } else {
      TakeoffMAP[i] = RatedMAP[i];
      bTakeoffBoost = false;
    }
    BoostMul[i] = RatedMAP[i] / (Atmosphere->GetPressure(RatedAltitude[i]) * psftopa);

  }

  if (BoostSpeeds > 0) {
    Boosted = true;
    BoostSpeed = 0;
  }
  bBoostOverride = (BoostOverride == 1 ? true : false);
  if (MinThrottle < 0.001) MinThrottle = 0.001;  //MinThrottle is a denominator in a power equation so it can't be zero
  Debug(0); // Call Debug() routine from constructor if needed
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

FGPiston::~FGPiston()
{
  delete Lookup_Combustion_Efficiency;
  delete Power_Mixture_Correlation;
  delete Power_per_Fuel_vs_AFR;
  delete Mixture_Efficiency_Correlation;
  Debug(1); // Call Debug() routine from constructor if needed
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGPiston::ResetToIC(void)
{
  FGEngine::ResetToIC();

  ManifoldPressure_inHg = Atmosphere->GetPressure() * psftoinhg; // psf to in Hg
  MAP = Atmosphere->GetPressure() * psftopa;
  double airTemperature_degK = RankineToKelvin(Atmosphere->GetTemperature());
  OilTemp_degK = airTemperature_degK;
  CylinderHeadTemp_degK = airTemperature_degK;
  ExhaustGasTemp_degK = airTemperature_degK;
  EGT_degC = ExhaustGasTemp_degK - 273;
  Thruster->SetRPM(0.0);
  RPM = 0.0;
  OilPressure_psi = 0.0;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

double FGPiston::Calculate(void)
{
  if (FuelFlow_gph > 0.0) ConsumeFuel();

  Throttle = FCS->GetThrottlePos(EngineNumber);
  // calculate the throttle plate angle.  1 unit is pi/2 radians.
  ThrottleAngle = MinThrottle+((MaxThrottle-MinThrottle)*Throttle );
  Mixture = FCS->GetMixturePos(EngineNumber);

  //
  // Input values.
  //

  p_amb = Atmosphere->GetPressure() * psftopa;
  p_amb_sea_level = Atmosphere->GetPressureSL() * psftopa;
  T_amb = RankineToKelvin(Atmosphere->GetTemperature());

  RPM = Thruster->GetRPM() * Thruster->GetGearRatio();

  IAS = Auxiliary->GetVcalibratedKTS();

  doEngineStartup();
  if (Boosted) doBoostControl();
  doMAP();
  doAirFlow();
  doFuelFlow();

  //Now that the fuel flow is done check if the mixture is too lean to run the engine
  //Assume lean limit at 22 AFR for now - thats a thi of 0.668
  //This might be a bit generous, but since there's currently no audiable warning of impending
  //cutout in the form of misfiring and/or rough running its probably reasonable for now.
//  if (equivalence_ratio < 0.668)
//    Running = false;

  doEnginePower();
if(HP<0.1250)
  Running = false;

  doEGT();
  doCHT();
  doOilTemperature();
  doOilPressure();

  if (Thruster->GetType() == FGThruster::ttPropeller) {
    ((FGPropeller*)Thruster)->SetAdvance(FCS->GetPropAdvance(EngineNumber));
    ((FGPropeller*)Thruster)->SetFeather(FCS->GetPropFeather(EngineNumber));
  }

  PowerAvailable = (HP * hptoftlbssec) - Thruster->GetPowerRequired();

  return Thruster->Calculate(PowerAvailable);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

double FGPiston::CalcFuelNeed(void)
{
  double dT = State->Getdt() * Propulsion->GetRate();
  FuelExpended = FuelFlowRate * dT;
  return FuelExpended;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

int FGPiston::InitRunning(void) {
  Magnetos=3;
  //Thruster->SetRPM( 1.1*IdleRPM/Thruster->GetGearRatio() );
  Thruster->SetRPM( 1000 );
  Running=true;
  return 1;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/**
 * Start or stop the engine.
 */

void FGPiston::doEngineStartup(void)
{
  // Check parameters that may alter the operating state of the engine.
  // (spark, fuel, starter motor etc)
  bool spark;
  bool fuel;

  // Check for spark
  Magneto_Left = false;
  Magneto_Right = false;
  // Magneto positions:
  // 0 -> off
  // 1 -> left only
  // 2 -> right only
  // 3 -> both
  if (Magnetos != 0) {
    spark = true;
  } else {
    spark = false;
  }  // neglects battery voltage, master on switch, etc for now.

  if ((Magnetos == 1) || (Magnetos > 2)) Magneto_Left = true;
  if (Magnetos > 1)  Magneto_Right = true;

  // Assume we have fuel for now
  fuel = !Starved;

  // Check if we are turning the starter motor
  if (Cranking != Starter) {
    // This check saves .../cranking from getting updated every loop - they
    // only update when changed.
    Cranking = Starter;
    crank_counter = 0;
  }

  if (Cranking) crank_counter++;  //Check mode of engine operation

  if (!Running && spark && fuel) {  // start the engine if revs high enough
    if (Cranking) {
      if ((RPM > IdleRPM*0.8) && (crank_counter > 175)) // Add a little delay to startup
        Running = true;                         // on the starter
    } else {
      if (RPM > IdleRPM*0.8)                            // This allows us to in-air start
        Running = true;                         // when windmilling
    }
  }

  // Cut the engine *power* - Note: the engine may continue to
  // spin if the prop is in a moving airstream

  if ( Running && (!spark || !fuel) ) Running = false;

  // Check for stalling (RPM = 0).
  if (Running) {
    if (RPM == 0) {
      Running = false;
    } else if ((RPM <= IdleRPM *0.8 ) && (Cranking)) {
      Running = false;
    }
  }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/**
 * Calculate the Current Boost Speed
 *
 * This function calculates the current turbo/supercharger boost speed
 * based on altitude and the (automatic) boost-speed control valve configuration.
 *
 * Inputs: p_amb, BoostSwitchPressure, BoostSwitchHysteresis
 *
 * Outputs: BoostSpeed
 */

void FGPiston::doBoostControl(void)
{
  if(BoostSpeed < BoostSpeeds - 1) {
    // Check if we need to change to a higher boost speed
    if(p_amb < BoostSwitchPressure[BoostSpeed] - BoostSwitchHysteresis) {
      BoostSpeed++;
    }
  } else if(BoostSpeed > 0) {
    // Check if we need to change to a lower boost speed
    if(p_amb > BoostSwitchPressure[BoostSpeed - 1] + BoostSwitchHysteresis) {
      BoostSpeed--;
    }
  }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/**
 * Calculate the manifold absolute pressure (MAP) in inches hg
 *
 * This function calculates manifold absolute pressure (MAP)
 * from the throttle position, turbo/supercharger boost control
 * system, engine speed and local ambient air density.
 *
 * TODO: changes in MP should not be instantaneous -- introduce
 * a lag between throttle changes and MP changes, to allow pressure
 * to build up or disperse.
 *
 * Inputs: maxMAP, p_amb, T_amb, RPM, MaxRPM
 *
 * Outputs: map_coefficient, MAP, ManifoldPressure_inHg
 */

const double P0(101325.0);      // ISA std sea-level pressure
const double T0(288.15);        // ISA std sea-level temperature
const double hour(3600);
const double pound(0.45359237); // 1 lb in SI units
const double stoich(14.7);      // stoichiometry air/fuel (mass/mass)
const double avgas_density(6.0);// Assumes 6 lbs / gallon

double findMAPnorm(const double revnorm, const double Tnorm,
         const double throttle_setting) {
  if (throttle_setting == 0.) {
    if (revnorm > 0.) return 1.;
    return 0.;
  }
  double throttle_area = throttle_setting * throttle_setting;
// FIXME:  should parameterize Ksquared
  double Ksquared(10.);              // K squared (not K)
  double J(revnorm*revnorm/Tnorm/Ksquared/throttle_area/throttle_area);  
  if (J < 1e-5) return 1. - 0.5*J;
  return (1./J)*(sqrt(1. + 2.*J) - 1.);           
}

void FGPiston::doMAP(void)
{

    double MAP_norm = findMAPnorm(RPM/MaxRPM, T_amb/T0, ThrottleAngle);

    MAP = p_amb * MAP_norm;

    if(Boosted) {
      // If takeoff boost is fitted, we currently assume the following throttle map:
      // (In throttle % - actual input is 0 -> 1)
      // 99 / 100 - Takeoff boost
      // 96 / 97 / 98 - Rated boost
      // 0 - 95 - Idle to Rated boost (MinManifoldPressure to MaxManifoldPressure)
      // In real life, most planes would be fitted with a mechanical 'gate' between
      // the rated boost and takeoff boost positions.
      double T = Throttle; // processed throttle value.
      bool bTakeoffPos = false;
      if(bTakeoffBoost) {
        if(Throttle > 0.98) {
          //cout << "Takeoff Boost!!!!\n";
          bTakeoffPos = true;
        } else if(Throttle <= 0.95) {
          bTakeoffPos = false;
          T *= 1.0 / 0.95;
        } else {
          bTakeoffPos = false;
          //cout << "Rated Boost!!\n";
          T = 1.0;
        }
      }
      // Boost the manifold pressure.
      double boost_factor = BoostMul[BoostSpeed] * MAP_norm * RPM/RatedRPM[BoostSpeed];
      if (boost_factor < 1.0) boost_factor = 1.0;  // boost will never reduce the MAP
      MAP *= boost_factor;
      // Now clip the manifold pressure to BCV or Wastegate setting.
      if(bTakeoffPos) {
        if(MAP > TakeoffMAP[BoostSpeed]) {
          MAP = TakeoffMAP[BoostSpeed];
        }
      } else {
        if(MAP > RatedMAP[BoostSpeed]) {
          MAP = RatedMAP[BoostSpeed];
        }
      }
    }

  // And set the value in American units as well
  ManifoldPressure_inHg = MAP / inhgtopa;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/**
 * Calculate the air flow through the engine.
 * Also calculates ambient air density
 * (used in CHT calculation for air-cooled engines).
 *
 * Inputs: p_amb, R_air, T_amb, MAP, Displacement,
 *   RPM, MaxRPM, volumetric_efficiency_0, ThrottleAngle
 *
 * TODO: Model inlet manifold air temperature.
 *
 * Outputs: rho_air, m_dot_air
 */

void FGPiston::doAirFlow(void)
{
  rho_air = p_amb / (R_air * T_amb);
  double displacement_SI = Displacement * in3tom3;
  double swept_volume = (displacement_SI * (RPM/60)) / 2;

// Model third-order loss of volumetric efficiency at high RPM:
  double revnorm = RPM/MaxRPM;
  double loss3 = 1. - revnorm*revnorm*revnorm*volumetric_loss_3;
  if (loss3 < 0 ) loss3 = 0;

  double v_dot_air = swept_volume * volumetric_efficiency_0 * loss3;

  double rho_air_manifold = MAP / (R_air * T_amb);
  m_dot_air = v_dot_air * rho_air_manifold;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/**
 * Calculate the fuel flow into the engine.
 *
 * Inputs: Mixture, P0, p_amb, m_dot_air
 *
 * Outputs: equivalence_ratio, m_dot_fuel
 */

void FGPiston::doFuelFlow(void)
{
// Allow an AFR of infinity:1 to 11.3075:1
  equivalence_ratio = 1.4 * Mixture * P0 / p_amb;
  m_dot_fuel = (m_dot_air * equivalence_ratio) / stoich;
  FuelFlowRate = m_dot_fuel / pound;    // kg to lb
  FuelFlow_pph = FuelFlowRate * hour;   // seconds to hours
  FuelFlow_gph = FuelFlow_pph / avgas_density;    
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/**
 * Calculate the power produced by the engine.
 *
 * Currently, the JSBSim propellor model does not allow the
 * engine to produce enough RPMs to get up to a high horsepower.
 * When tested with sufficient RPM, it has no trouble reaching
 * 200HP.
 *
 * Inputs: ManifoldPressure_inHg, p_amb, RPM, T_amb,
 *   Mixture_Efficiency_Correlation, Cycles, MaxHP
 *
 * Outputs: Percentage_Power, HP
 */

void FGPiston::doEnginePower(void)
{
  if (Running) {

// FIXME: this needs to be generalized

// Pnorm is 1.0 at stoichiometry.
// It peaks a little higher than that, at a richer AFR
    double Pnorm = m_dot_fuel <= 0. ? 0. 
        : Power_per_Fuel_vs_AFR->GetValue(m_dot_air/m_dot_fuel);

    if ( Magnetos != 3 ) Pnorm *= SparkFailDrop;

    HP = (FuelFlow_pph / nominal_BSFC ) * Pnorm;

  } else {

    // Power output when the engine is not running
    if (Cranking) {
      if (RPM < 10) {
        HP = StarterHP;
      } else if (RPM < IdleRPM*0.8) {
        HP = StarterHP + ((IdleRPM*0.8 - RPM) / 8.0);
        // This is a guess - would be nice to find a proper starter moter torque curve
      } else {
        HP = StarterHP;
      }
    } else {
      // Quick hack until we port the FMEP stuff
      if (RPM > 0.0)
        HP = -1.5;
      else
        HP = 0.0;
    }
  }
  Percentage_Power = HP / MaxHP ;
//  cout << "Power = " << HP << "  RPM = " << RPM << "  Running = " << Running << "  Cranking = " << Cranking << endl;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/**
 * Calculate the exhaust gas temperature.
 *
 * Inputs: equivalence_ratio, m_dot_fuel, calorific_value_fuel,
 *   Cp_air, m_dot_air, Cp_fuel, m_dot_fuel, T_amb, Percentage_Power
 *
 * Outputs: combustion_efficiency, ExhaustGasTemp_degK
 */

void FGPiston::doEGT(void)
{
  double delta_T_exhaust;
  double enthalpy_exhaust;
  double heat_capacity_exhaust;
  double dEGTdt;

  if ((Running) && (m_dot_air > 0.0)) {  // do the energy balance
    combustion_efficiency = Lookup_Combustion_Efficiency->GetValue(equivalence_ratio);
    enthalpy_exhaust = m_dot_fuel * calorific_value_fuel *
                              combustion_efficiency * 0.33;
    heat_capacity_exhaust = (Cp_air * m_dot_air) + (Cp_fuel * m_dot_fuel);
    delta_T_exhaust = enthalpy_exhaust / heat_capacity_exhaust;
    ExhaustGasTemp_degK = T_amb + delta_T_exhaust;
    ExhaustGasTemp_degK *= 0.444 + ((0.544 - 0.444) * Percentage_Power);
  } else {  // Drop towards ambient - guess an appropriate time constant for now
    combustion_efficiency = 0;
    dEGTdt = (RankineToKelvin(Atmosphere->GetTemperature()) - ExhaustGasTemp_degK) / 100.0;
    delta_T_exhaust = dEGTdt * dt;
    ExhaustGasTemp_degK += delta_T_exhaust;
  }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/**
 * Calculate the cylinder head temperature.
 *
 * Inputs: T_amb, IAS, rho_air, m_dot_fuel, calorific_value_fuel,
 *   combustion_efficiency, RPM, MaxRPM
 *
 * Outputs: CylinderHeadTemp_degK
 */

void FGPiston::doCHT(void)
{
  double h1 = -95.0;
  double h2 = -3.95;
  double h3 = -140.0; // -0.05 * 2800 (default maxrpm)

  double arbitary_area = 1.0;
  double CpCylinderHead = 800.0;
  double MassCylinderHead = 8.0;

  double temperature_difference = CylinderHeadTemp_degK - T_amb;
  double v_apparent = IAS * 0.5144444;
  double v_dot_cooling_air = arbitary_area * v_apparent;
  double m_dot_cooling_air = v_dot_cooling_air * rho_air;
  double dqdt_from_combustion =
    m_dot_fuel * calorific_value_fuel * combustion_efficiency * 0.33;
  double dqdt_forced = (h2 * m_dot_cooling_air * temperature_difference) +
    (h3 * RPM * temperature_difference / MaxRPM);
  double dqdt_free = h1 * temperature_difference;
  double dqdt_cylinder_head = dqdt_from_combustion + dqdt_forced + dqdt_free;

  double HeatCapacityCylinderHead = CpCylinderHead * MassCylinderHead;

  CylinderHeadTemp_degK +=
    (dqdt_cylinder_head / HeatCapacityCylinderHead) * dt;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/**
 * Calculate the oil temperature.
 *
 * Inputs: CylinderHeadTemp_degK, T_amb, OilPressure_psi.
 *
 * Outputs: OilTemp_degK
 */

void FGPiston::doOilTemperature(void)
{
  double idle_percentage_power = 0.023;        // approximately
  double target_oil_temp;        // Steady state oil temp at the current engine conditions
  double time_constant;          // The time constant for the differential equation
  double efficiency = 0.667;     // The aproximate oil cooling system efficiency // FIXME: may vary by engine

//  Target oil temp is interpolated between ambient temperature and Cylinder Head Tempurature
//  target_oil_temp = ( T_amb * efficiency ) + (CylinderHeadTemp_degK *(1-efficiency)) ;
  target_oil_temp = CylinderHeadTemp_degK + efficiency * (T_amb - CylinderHeadTemp_degK) ;

  if (OilPressure_psi > 5.0 ) {
    time_constant = 5000 / OilPressure_psi; // Guess at a time constant for circulated oil.
                                            // The higher the pressure the faster it reaches
					    // target temperature.  Oil pressure should be about
					    // 60 PSI yielding a TC of about 80.
  } else {
    time_constant = 1000;  // Time constant for engine-off; reflects the fact
                           // that oil is no longer getting circulated
  }

  double dOilTempdt = (target_oil_temp - OilTemp_degK) / time_constant;

  OilTemp_degK += (dOilTempdt * dt);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/**
 * Calculate the oil pressure.
 *
 * Inputs: RPM, MaxRPM, OilTemp_degK
 *
 * Outputs: OilPressure_psi
 */

void FGPiston::doOilPressure(void)
{
  double Oil_Press_Relief_Valve = 60; // FIXME: may vary by engine
  double Oil_Press_RPM_Max = MaxRPM * 0.75;    // 75% of max rpm FIXME: may vary by engine
  double Design_Oil_Temp = 358;          // degK; FIXME: may vary by engine
  double Oil_Viscosity_Index = 0.25;

  OilPressure_psi = (Oil_Press_Relief_Valve / Oil_Press_RPM_Max) * RPM;

  if (OilPressure_psi >= Oil_Press_Relief_Valve) {
    OilPressure_psi = Oil_Press_Relief_Valve;
  }

  OilPressure_psi += (Design_Oil_Temp - OilTemp_degK) * Oil_Viscosity_Index * OilPressure_psi / Oil_Press_Relief_Valve;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

string FGPiston::GetEngineLabels(string delimeter)
{
  std::ostringstream buf;

  buf << Name << " Power Available (engine " << EngineNumber << " in HP)" << delimeter
      << Name << " HP (engine " << EngineNumber << ")" << delimeter
      << Name << " equivalent ratio (engine " << EngineNumber << ")" << delimeter
      << Name << " MAP (engine " << EngineNumber << ")" << delimeter
      << Thruster->GetThrusterLabels(EngineNumber, delimeter);

  return buf.str();
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

string FGPiston::GetEngineValues(string delimeter)
{
  std::ostringstream buf;

  buf << PowerAvailable << delimeter << HP << delimeter
      << equivalence_ratio << delimeter << MAP << delimeter
      << Thruster->GetThrusterValues(EngineNumber, delimeter);

  return buf.str();
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//
//    The bitmasked value choices are as follows:
//    unset: In this case (the default) JSBSim would only print
//       out the normally expected messages, essentially echoing
//       the config files as they are read. If the environment
//       variable is not set, debug_lvl is set to 1 internally
//    0: This requests JSBSim not to output any messages
//       whatsoever.
//    1: This value explicity requests the normal JSBSim
//       startup messages
//    2: This value asks for a message to be printed out when
//       a class is instantiated
//    4: When this value is set, a message is displayed when a
//       FGModel object executes its Run() method
//    8: When this value is set, various runtime state variables
//       are printed out periodically
//    16: When set various parameters are sanity checked and
//       a message is printed out when they go out of bounds

void FGPiston::Debug(int from)
{
  if (debug_lvl <= 0) return;

  if (debug_lvl & 1) { // Standard console startup message output
    if (from == 0) { // Constructor

      cout << "\n    Engine Name:         " << Name << endl;
      cout << "      MinManifoldPressure: " << MinManifoldPressure_inHg << endl;
      cout << "      MaxManifoldPressure: " << MaxManifoldPressure_inHg << endl;
      cout << "      MinMaP (Pa):         " << minMAP << endl;
      cout << "      MaxMaP (Pa):         " << maxMAP << endl;
      cout << "      Displacement:        " << Displacement             << endl;
      cout << "      MaxHP:               " << MaxHP                    << endl;
      cout << "      Cycles:              " << Cycles                   << endl;
      cout << "      IdleRPM:             " << IdleRPM                  << endl;
      cout << "      MaxThrottle:         " << MaxThrottle              << endl;
      cout << "      MinThrottle:         " << MinThrottle              << endl;
      cout << "      nominal_BSFC:        " << nominal_BSFC             << endl;

      cout << endl;
      cout << "      Combustion Efficiency table:" << endl;
      Lookup_Combustion_Efficiency->Print();
      cout << endl;

      cout << endl;
      cout << "      Power Mixture Correlation table:" << endl;
      Power_Mixture_Correlation->Print();
      cout << endl;

      cout << endl;
      cout << "      Mixture Efficiency Correlation table:" << endl;
      Mixture_Efficiency_Correlation->Print();
      cout << endl;

    }
  }
  if (debug_lvl & 2 ) { // Instantiation/Destruction notification
    if (from == 0) cout << "Instantiated: FGPiston" << endl;
    if (from == 1) cout << "Destroyed:    FGPiston" << endl;
  }
  if (debug_lvl & 4 ) { // Run() method entry print for FGModel-derived objects
  }
  if (debug_lvl & 8 ) { // Runtime state variables
  }
  if (debug_lvl & 16) { // Sanity checking
  }
  if (debug_lvl & 64) {
    if (from == 0) { // Constructor
      cout << IdSrc << endl;
      cout << IdHdr << endl;
    }
  }
}
} // namespace JSBSim
