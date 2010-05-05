// trafficcontrol.hxx - classes to manage AIModels based air traffic control
// Written by Durk Talsma, started September 2006.
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


#ifndef _TRAFFIC_CONTROL_HXX_
#define _TRAFFIC_CONTROL_HXX_


#ifndef __cplusplus
# error This library requires C++
#endif


#include <simgear/compiler.h>
#include <simgear/debug/logstream.hxx>



#include <string>
#include <vector>

using std::string;
using std::vector;


typedef vector<int> intVec;
typedef vector<int>::iterator intVecIterator;


class FGAIFlightPlan;  // forward reference
class FGGroundNetwork; // forward reference
class FGAIAircraft;    // forward reference

/**************************************************************************************
 * class FGATCInstruction
 * like class FGATC Controller, this class definition should go into its own file
 * and or directory... For now, just testing this stuff out though...
 *************************************************************************************/
class FGATCInstruction
{
private:
  bool holdPattern;
  bool holdPosition;
  bool changeSpeed;
  bool changeHeading;
  bool changeAltitude;
  bool resolveCircularWait;

  double speed;
  double heading;
  double alt;
public:

  FGATCInstruction();
  bool hasInstruction   ();
  bool getHoldPattern   () { return holdPattern;    };
  bool getHoldPosition  () { return holdPosition;   };
  bool getChangeSpeed   () { return changeSpeed;    };
  bool getChangeHeading () { return changeHeading;  };
  bool getChangeAltitude() { return changeAltitude; };

  double getSpeed       () { return speed; };
  double getHeading     () { return heading; };
  double getAlt         () { return alt; };

  bool getCheckForCircularWait() { return resolveCircularWait; };

  void setHoldPattern   (bool val) { holdPattern    = val; };
  void setHoldPosition  (bool val) { holdPosition   = val; };
  void setChangeSpeed   (bool val) { changeSpeed    = val; };
  void setChangeHeading (bool val) { changeHeading  = val; };
  void setChangeAltitude(bool val) { changeAltitude = val; };

  void setResolveCircularWait (bool val) { resolveCircularWait = val; }; 

  void setSpeed       (double val) { speed   = val; };
  void setHeading     (double val) { heading = val; };
  void setAlt         (double val) { alt     = val; };
};





/**************************************************************************************
 * class FGTrafficRecord
 *************************************************************************************/
class FGTrafficRecord
{
private:
  int id, waitsForId;
  int currentPos;
  int leg;
  int frequencyId;
  int state;
  bool allowTransmission;
  time_t timer;
  intVec intentions;
  FGATCInstruction instruction;
  double latitude, longitude, heading, speed, altitude, radius;
  string runway;
  //FGAISchedule *trafficRef;
  FGAIAircraft *aircraft;
  
  
public:
  FGTrafficRecord();
  
  void setId(int val)  { id = val; };
  void setRadius(double rad) { radius = rad;};
  void setPositionAndIntentions(int pos, FGAIFlightPlan *route);
  void setRunway(string rwy) { runway = rwy;};
  void setLeg(int lg) { leg = lg;};
  int getId() { return id;};
  int getState() { return state;};
  int setState(int s) { state = s;}
  FGATCInstruction getInstruction() { return instruction;};
  bool hasInstruction() { return instruction.hasInstruction(); };
  void setPositionAndHeading(double lat, double lon, double hdg, double spd, double alt);
  bool checkPositionAndIntentions(FGTrafficRecord &other);
  int  crosses                   (FGGroundNetwork *, FGTrafficRecord &other); 
  bool isOpposing                (FGGroundNetwork *, FGTrafficRecord &other, int node);

  bool onRoute(FGGroundNetwork *, FGTrafficRecord &other);

  bool getSpeedAdjustment() { return instruction.getChangeSpeed(); };
  
  double getLatitude () { return latitude ; };
  double getLongitude() { return longitude; };
  double getHeading  () { return heading  ; };
  double getSpeed    () { return speed    ; };
  double getAltitude () { return altitude ; };
  double getRadius   () { return radius   ; };

  int getWaitsForId  () { return waitsForId; };

  void setSpeedAdjustment(double spd);
  void setHeadingAdjustment(double heading);
  void clearSpeedAdjustment  () { instruction.setChangeSpeed  (false); };
  void clearHeadingAdjustment() { instruction.setChangeHeading(false); };

  bool hasHeadingAdjustment() { return instruction.getChangeHeading(); };
  bool hasHoldPosition() { return instruction.getHoldPosition(); };
  void setHoldPosition (bool inst) { instruction.setHoldPosition(inst); };

  void setWaitsForId(int id) { waitsForId = id; };

  void setResolveCircularWait()   { instruction.setResolveCircularWait(true);  };
  void clearResolveCircularWait() { instruction.setResolveCircularWait(false); };

  string getRunway() { return runway; };
  //void setCallSign(string clsgn) { callsign = clsgn; };
  void setAircraft(FGAIAircraft *ref) { aircraft = ref;};
  void updateState() { state++; allowTransmission=true; };
  //string getCallSign() { return callsign; };
  FGAIAircraft *getAircraft() { return aircraft;};
  int getTime() { return timer; };
  int getLeg() { return leg; };
  void setTime(time_t time) { timer = time; };

  bool pushBackAllowed();
  bool allowTransmissions() { return allowTransmission; };
  void suppressRepeatedTransmissions () { allowTransmission=false; };
  void allowRepeatedTransmissions () { allowTransmission=true; };
  void nextFrequency() { frequencyId++; };
  int  getNextFrequency() { return frequencyId; };
};

typedef vector<FGTrafficRecord> TrafficVector;
typedef vector<FGTrafficRecord>::iterator TrafficVectorIterator;


/***********************************************************************
 * Active runway, a utility class to keep track of which aircraft has
 * clearance for a given runway.
 **********************************************************************/
class ActiveRunway
{
private:
  string rwy;
  int currentlyCleared;
public:
  ActiveRunway(string r, int cc) { rwy = r; currentlyCleared = cc; };
  
  string getRunwayName() { return rwy; };
  int    getCleared   () { return currentlyCleared; };
};

typedef vector<ActiveRunway> ActiveRunwayVec;
typedef vector<ActiveRunway>::iterator ActiveRunwayVecIterator;

/**
 * class FGATCController
 * NOTE: this class serves as an abstraction layer for all sorts of ATC controller. 
 *************************************************************************************/
class FGATCController
{
protected:
  bool available;
  time_t lastTransmission;

  double dt_count;


  string formatATCFrequency3_2(int );
  string genTransponderCode(string fltRules);

public:
  typedef enum {
      MSG_ANNOUNCE_ENGINE_START,
      MSG_REQUEST_ENGINE_START,
      MSG_PERMIT_ENGINE_START,
      MSG_DENY_ENGINE_START,
      MSG_ACKNOWLEDGE_ENGINE_START,
      MSG_REQUEST_PUSHBACK_CLEARANCE,
      MSG_PERMIT_PUSHBACK_CLEARANCE,
      MSG_HOLD_PUSHBACK_CLEARANCE,
      MSG_ACKNOWLEDGE_SWITCH_GROUND_FREQUENCY,
      MSG_INITIATE_CONTACT,
      MSG_ACKNOWLEDGE_INITIATE_CONTACT,
      MSG_REQUEST_TAXI_CLEARANCE,
      MSG_ISSUE_TAXI_CLEARANCE,
      MSG_ACKNOWLEDGE_TAXI_CLEARANCE,
      MSG_HOLD_POSITION,
      MSG_ACKNOWLEDGE_HOLD_POSITION,
      MSG_RESUME_TAXI,
      MSG_ACKNOWLEDGE_RESUME_TAXI } AtcMsgId;
  typedef enum {
      ATC_AIR_TO_GROUND,
      ATC_GROUND_TO_AIR } AtcMsgDir;
  FGATCController();
  virtual ~FGATCController() {};
  virtual void announcePosition(int id, FGAIFlightPlan *intendedRoute, int currentRoute,
				double lat, double lon,
				double hdg, double spd, double alt, double radius, int leg,
				FGAIAircraft *aircraft) = 0;
  virtual void             signOff(int id) = 0;
  virtual void             update(int id, double lat, double lon, 
				  double heading, double speed, double alt, double dt) = 0;
  virtual bool             hasInstruction(int id) = 0;
  virtual FGATCInstruction getInstruction(int id) = 0;

  double getDt() { return dt_count; };
  void   setDt(double dt) { dt_count = dt;};
  void transmit(FGTrafficRecord *rec, AtcMsgId msgId, AtcMsgDir msgDir);
  string getGateName(FGAIAircraft *aircraft);
};

/******************************************************************************
 * class FGTowerControl
 *****************************************************************************/
class FGTowerController : public FGATCController
{
private:
  TrafficVector activeTraffic;
  ActiveRunwayVec activeRunways;
  
public:
  FGTowerController();
  virtual ~FGTowerController() {};
  virtual void announcePosition(int id, FGAIFlightPlan *intendedRoute, int currentRoute,
				double lat, double lon,
				double hdg, double spd, double alt, double radius, int leg,
				FGAIAircraft *aircraft);
  virtual void             signOff(int id);
  virtual void             update(int id, double lat, double lon, 
				  double heading, double speed, double alt, double dt);
  virtual bool             hasInstruction(int id);
  virtual FGATCInstruction getInstruction(int id);

  bool hasActiveTraffic() { return activeTraffic.size() != 0; };
  TrafficVector &getActiveTraffic() { return activeTraffic; };
};

/******************************************************************************
 * class FGStartupController
 * handle 
 *****************************************************************************/

class FGStartupController : public FGATCController
{
private:
  TrafficVector activeTraffic;
  //ActiveRunwayVec activeRunways;
  
public:
  FGStartupController();
  virtual ~FGStartupController() {};
  virtual void announcePosition(int id, FGAIFlightPlan *intendedRoute, int currentRoute,
				double lat, double lon,
				double hdg, double spd, double alt, double radius, int leg,
				FGAIAircraft *aircraft);
  virtual void             signOff(int id);
  virtual void             update(int id, double lat, double lon, 
				  double heading, double speed, double alt, double dt);
  virtual bool             hasInstruction(int id);
  virtual FGATCInstruction getInstruction(int id);

  bool hasActiveTraffic() { return activeTraffic.size() != 0; };
  TrafficVector &getActiveTraffic() { return activeTraffic; };

}; 

#endif // _TRAFFIC_CONTROL_HXX
