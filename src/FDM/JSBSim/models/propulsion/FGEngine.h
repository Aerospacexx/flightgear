/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

 Header:       FGEngine.h
 Author:       Jon S. Berndt
 Date started: 01/21/99

 ------------- Copyright (C) 1999  Jon S. Berndt (jsb@hal-pc.org) -------------

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU Lesser General Public License as published by the Free Software
 Foundation; either version 2 of the License, or (at your option) any later
 version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 Place - Suite 330, Boston, MA  02111-1307, USA.

 Further information about the GNU Lesser General Public License can also be found on
 the world wide web at http://www.gnu.org.

FUNCTIONAL DESCRIPTION
--------------------------------------------------------------------------------

Based on Flightgear code, which is based on LaRCSim. This class simulates
a generic engine.

HISTORY
--------------------------------------------------------------------------------
01/21/99   JSB   Created

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
SENTRY
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifndef FGENGINE_H
#define FGENGINE_H

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
INCLUDES
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifdef FGFS
#  include <simgear/compiler.h>
#  include STL_STRING
   SG_USING_STD(string);
#  ifdef SG_HAVE_STD_INCLUDES
#    include <vector>
#  else
#    include <vector.h>
#  endif
#else
#  include <vector>
#  include <string>
#endif

#include <FGJSBBase.h>
#include "FGThruster.h"
#include <input_output/FGPropertyManager.h>
#include <input_output/FGXMLFileRead.h>

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
DEFINITIONS
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#define ID_ENGINE "$Id$"

using std::string;
using std::vector;

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
FORWARD DECLARATIONS
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

namespace JSBSim {

class FGFDMExec;
class FGState;
class FGAtmosphere;
class FGFCS;
class FGAircraft;
class FGPropagate;
class FGPropulsion;
class FGAuxiliary;
class FGThruster;
class Element;

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
CLASS DOCUMENTATION
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/** Base class for all engines.
    This base class contains methods and members common to all engines, such as
    logic to drain fuel from the appropriate tank, etc.
    @author Jon S. Berndt
    @version $Id$
*/

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
CLASS DECLARATION
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

class FGEngine : public FGJSBBase, public FGXMLFileRead
{
public:
  FGEngine(FGFDMExec* exec, Element* el, int engine_number);
  virtual ~FGEngine();

  enum EngineType {etUnknown, etRocket, etPiston, etTurbine, etTurboprop, etElectric};

  EngineType      GetType(void) { return Type; }
  virtual string  GetName(void) { return Name; }

  // Engine controls
  virtual double  GetThrottleMin(void) { return MinThrottle; }
  virtual double  GetThrottleMax(void) { return MaxThrottle; }
  virtual double  GetThrottle(void) { return Throttle; }
  virtual double  GetMixture(void) { return Mixture; }
  virtual bool    GetStarter(void) { return Starter; }

  virtual double getFuelFlow_gph () const {return FuelFlow_gph;}
  virtual double getFuelFlow_pph () const {return FuelFlow_pph;}
  virtual double GetThrust(void) { return Thrust; }
  virtual bool   GetStarved(void) { return Starved; }
  virtual bool   GetRunning(void) { return Running; }
  virtual bool   GetCranking(void) { return Cranking; }

  virtual void SetStarved(bool tt) { Starved = tt; }
  virtual void SetStarved(void)    { Starved = true; }

  virtual void SetRunning(bool bb) { Running=bb; }
  virtual void SetName(string name) { Name = name; }
  virtual void AddFeedTank(int tkID);
  virtual void SetFuelFreeze(bool f) { FuelFreeze = f; }

  virtual void SetStarter(bool s) { Starter = s; }

  /** Calculates the thrust of the engine, and other engine functions.
      @return Thrust in pounds */
  virtual double Calculate(void) {return 0.0;}

  /** Reduces the fuel in the active tanks by the amount required.
      This function should be called from within the
      derived class' Calculate() function before any other calculations are
      done. This base class method removes fuel from the fuel tanks as
      appropriate, and sets the starved flag if necessary. */
  virtual void ConsumeFuel(void);

  /** The fuel need is calculated based on power levels and flow rate for that
      power level. It is also turned from a rate into an actual amount (pounds)
      by multiplying it by the delta T and the rate.
      @return Total fuel requirement for this engine in pounds. */
  virtual double CalcFuelNeed(void);

  /** The oxidizer need is calculated based on power levels and flow rate for that
      power level. It is also turned from a rate into an actual amount (pounds)
      by multiplying it by the delta T and the rate.
      @return Total oxidizer requirement for this engine in pounds. */
  virtual double CalcOxidizerNeed(void);

  /// Sets engine placement information
  virtual void SetPlacement(FGColumnVector3& location, FGColumnVector3& orientation);

  virtual double GetPowerAvailable(void) {return 0.0;};

  virtual bool GetTrimMode(void) {return TrimMode;}
  virtual void SetTrimMode(bool state) {TrimMode = state;}

  virtual FGColumnVector3& GetBodyForces(void);
  virtual FGColumnVector3& GetMoments(void);

  bool LoadThruster(Element *el);
  FGThruster* GetThruster(void) {return Thruster;}

  virtual string GetEngineLabels(string delimeter) = 0;
  virtual string GetEngineValues(string delimeter) = 0;

protected:
  FGPropertyManager* PropertyManager;
  string Name;
  const int   EngineNumber;
  EngineType Type;
  double X, Y, Z;
  double EnginePitch;
  double EngineYaw;
  double SLFuelFlowMax;
  double SLOxiFlowMax;
  double MaxThrottle;
  double MinThrottle;

  double Thrust;
  double Throttle;
  double Mixture;
  double FuelNeed;
  double OxidizerNeed;
  double PctPower;
  bool  Starter;
  bool  Starved;
  bool  Running;
  bool  Cranking;
  bool  TrimMode;
  bool  FuelFreeze;

  double FuelFlow_gph;
  double FuelFlow_pph;

  FGFDMExec*      FDMExec;
  FGState*        State;
  FGAtmosphere*   Atmosphere;
  FGFCS*          FCS;
  FGPropulsion*   Propulsion;
  FGAircraft*     Aircraft;
  FGPropagate*    Propagate;
  FGAuxiliary*    Auxiliary;
  FGThruster*     Thruster;

  vector <int> SourceTanks;
  void Debug(int from);
};
}
#include <FGState.h>
#include <FGFDMExec.h>
#include <models/FGAtmosphere.h>
#include <models/FGFCS.h>
#include <models/FGAircraft.h>
#include <models/FGPropagate.h>
#include <models/FGPropulsion.h>
#include <models/FGAuxiliary.h>
#include <models/propulsion/FGThruster.h>
#include <input_output/FGXMLElement.h>

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
#endif

