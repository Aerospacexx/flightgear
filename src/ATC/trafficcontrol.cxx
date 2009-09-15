// trafficrecord.cxx - Implementation of AIModels ATC code.
//
// Written by Durk Talsma, started September 2006.
//
// Copyright (C) 2006 Durk Talsma.
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

#include "trafficcontrol.hxx"
#include <AIModel/AIAircraft.hxx>
#include <AIModel/AIFlightPlan.hxx>
#include <Traffic/TrafficMgr.hxx>
#include <Airports/groundnetwork.hxx>
#include <Airports/dynamics.hxx>

/***************************************************************************
 * FGTrafficRecord
 **************************************************************************/
FGTrafficRecord::FGTrafficRecord() :
  id(0), waitsForId(0),
  currentPos(0),
  leg(0),
  state(0),
  latitude(0),
  longitude(0), 
   heading(0), 
   speed(0), 
   altitude(0), 
   radius(0) {
}

void FGTrafficRecord::setPositionAndIntentions(int pos, FGAIFlightPlan *route)
{
 
   currentPos = pos;
   if (intentions.size()) {
     intVecIterator i = intentions.begin();
     if ((*i) != pos) {
       SG_LOG(SG_GENERAL, SG_ALERT, "Error in FGTrafficRecord::setPositionAndIntentions");
       //cerr << "Pos : " << pos << " Curr " << *(intentions.begin())  << endl;
       for (intVecIterator i = intentions.begin(); i != intentions.end() ; i++) {
 	//cerr << (*i) << " ";
       }
       //cerr << endl;
     }
     intentions.erase(i);
   } else {
     //int legNr, routeNr;
     //FGAIFlightPlan::waypoint* const wpt= route->getCurrentWaypoint();
     int size = route->getNrOfWayPoints();
     //cerr << "Setting pos" << pos << " ";
     //cerr << "setting intentions ";
     for (int i = 0; i < size; i++) {
       int val = route->getRouteIndex(i);
       //cerr << val<< " ";
       if ((val) && (val != pos))
 	{
 	  intentions.push_back(val); 
 	  //cerr << "[set] ";
 	}
     }
     //cerr << endl;
     //while (route->next(&legNr, &routeNr)) {
     //intentions.push_back(routeNr);
     //}
     //route->rewind(currentPos);
   }
   //exit(1);
}

bool FGTrafficRecord::checkPositionAndIntentions(FGTrafficRecord &other)
{
   bool result = false;
   //cerr << "Start check 1" << endl;
   if (currentPos == other.currentPos) 
     {
       //cerr << callsign << ": Check Position and intentions: we are on the same taxiway" << other.callsign << "Index = " << currentPos << endl;
       result = true;
     }
  //  else if (other.intentions.size()) 
 //     {
 //       cerr << "Start check 2" << endl;
 //       intVecIterator i = other.intentions.begin(); 
 //       while (!((i == other.intentions.end()) || ((*i) == currentPos)))
 // 	i++;
 //       if (i != other.intentions.end()) {
 // 	cerr << "Check Position and intentions: current matches other.intentions" << endl;
 // 	result = true;
 //       }
   else if (intentions.size()) {
     //cerr << "Start check 3" << endl;
     intVecIterator i = intentions.begin(); 
     //while (!((i == intentions.end()) || ((*i) == other.currentPos)))
     while (i != intentions.end()) {
       if ((*i) == other.currentPos) {
	 break;
       }
       i++;
     }
     if (i != intentions.end()) {
       //cerr << callsign << ": Check Position and intentions: .other.current matches" << other.callsign << "Index = " << (*i) << endl;
       result = true;
     }
   }
   //cerr << "Done !!" << endl;
   return result;
}

void FGTrafficRecord::setPositionAndHeading(double lat, double lon, double hdg, 
					    double spd, double alt)
{
  latitude = lat;
  longitude = lon;
  heading = hdg;
  speed = spd;
  altitude = alt;
}

int FGTrafficRecord::crosses(FGGroundNetwork *net, FGTrafficRecord &other)
{
   if (checkPositionAndIntentions(other) || (other.checkPositionAndIntentions(*this)))
     return -1;
   intVecIterator i, j;
   int currentTargetNode = 0, otherTargetNode = 0;
   if (currentPos > 0)
     currentTargetNode = net->findSegment(currentPos      )->getEnd()->getIndex(); // OKAY,... 
   if (other.currentPos > 0)
     otherTargetNode   = net->findSegment(other.currentPos)->getEnd()->getIndex(); // OKAY,...
   if ((currentTargetNode == otherTargetNode) && currentTargetNode > 0)
     return currentTargetNode;
   if (intentions.size())
     {
       for (i = intentions.begin(); i != intentions.end(); i++)
 	{
 	  if ((*i) > 0) {
 	    if ((currentTargetNode == net->findSegment(*i)->getEnd()->getIndex()))
 	      {
 		//cerr << "Current crosses at " << currentTargetNode <<endl;
 		return currentTargetNode;
 	      }
 	  }
 	}
     }
   if (other.intentions.size())
     {
       for (i = other.intentions.begin(); i != other.intentions.end(); i++)
 	{
 	  if ((*i) > 0) {
 	    if (otherTargetNode == net->findSegment(*i)->getEnd()->getIndex())
 	      {
 		//cerr << "Other crosses at " << currentTargetNode <<endl;
 		return otherTargetNode;
 	      }
 	  }
 	}
     }
   if (intentions.size() && other.intentions.size())
     {
       for (i = intentions.begin(); i != intentions.end(); i++) 
 	{
 	  for (j = other.intentions.begin(); j != other.intentions.end(); j++)
 	    {
 	      //cerr << "finding segment " << *i << " and " << *j << endl;
 	      if (((*i) > 0) && ((*j) > 0)) {
 		currentTargetNode = net->findSegment(*i)->getEnd()->getIndex();
 		otherTargetNode   = net->findSegment(*j)->getEnd()->getIndex();
 		if (currentTargetNode == otherTargetNode) 
 		  {
 		    //cerr << "Routes will cross at " << currentTargetNode << endl;
 		    return currentTargetNode;
 		  }
 	      }
 	    }
 	}
     }
  return -1;
}

bool FGTrafficRecord::onRoute(FGGroundNetwork *net, FGTrafficRecord &other)
{
  int node = -1, othernode = -1;
  if (currentPos >0)
    node = net->findSegment(currentPos)->getEnd()->getIndex();
  if (other.currentPos > 0)
    othernode = net->findSegment(other.currentPos)->getEnd()->getIndex();
  if ((node == othernode) && (node != -1))
    return true;
  if (other.intentions.size())
    {
      for (intVecIterator i = other.intentions.begin(); i != other.intentions.end(); i++)
	{
	  if (*i > 0) 
	    {
	      othernode = net->findSegment(*i)->getEnd()->getIndex();
	      if ((node == othernode) && (node > -1))
		return true;
	    }
	}
    }
  //if (other.currentPos > 0)
  //  othernode = net->findSegment(other.currentPos)->getEnd()->getIndex();
  //if (intentions.size())
  //  {
  //    for (intVecIterator i = intentions.begin(); i != intentions.end(); i++)
  //	{
  //	  if (*i > 0) 
  //	    {
  //	      node = net->findSegment(*i)->getEnd()->getIndex();
  //	      if ((node == othernode) && (node > -1))
  //		return true;
  //	    }
  //	}
  //  }
  return false;
}


bool FGTrafficRecord::isOpposing (FGGroundNetwork *net, FGTrafficRecord &other, int node)
{
   // Check if current segment is the reverse segment for the other aircraft
   FGTaxiSegment *opp;
   //cerr << "Current segment " << currentPos << endl;
   if ((currentPos > 0) && (other.currentPos > 0))
     {
       opp = net->findSegment(currentPos)->opposite();
       if (opp) {
 	if (opp->getIndex() == other.currentPos)
 	  return true;
       }
      
       for (intVecIterator i = intentions.begin(); i != intentions.end(); i++)
 	{
	  if (opp = net->findSegment(other.currentPos)->opposite())
	    {
	      if ((*i) > 0)
		if (opp->getIndex() == net->findSegment(*i)->getIndex())
		  {
		    if (net->findSegment(*i)->getStart()->getIndex() == node) {
		      {
			//cerr << "Found the node " << node << endl;
			return true;
		      }
		    }
		  }
	    }
	  if (other.intentions.size())
	    {
	      for (intVecIterator j = other.intentions.begin(); j != other.intentions.end(); j++)
		{  
		  // cerr << "Current segment 1 " << (*i) << endl;
		  if ((*i) > 0) {
		    if (opp = net->findSegment(*i)->opposite())
		      {
			if (opp->getIndex() == 
			    net->findSegment(*j)->getIndex())
			  {
			    //cerr << "Nodes " << net->findSegment(*i)->getIndex()
			    //	 << " and  " << net->findSegment(*j)->getIndex()
			    //	 << " are opposites " << endl;
			    if (net->findSegment(*i)->getStart()->getIndex() == node) {
			      {
				//cerr << "Found the node " << node << endl;
				return true;
			      }
			    }
			  }
		      }
		  }
		}
	    }
	}
     }
   return false;
}

void FGTrafficRecord::setSpeedAdjustment(double spd) 
{ 
  instruction.setChangeSpeed(true); 
  instruction.setSpeed(spd); 
}

void FGTrafficRecord::setHeadingAdjustment(double heading) 
{ 
  instruction.setChangeHeading(true);
  instruction.setHeading(heading); 
}



/***************************************************************************
 * FGATCInstruction
 *
 **************************************************************************/
FGATCInstruction::FGATCInstruction()
{
  holdPattern    = false; 
  holdPosition   = false;
  changeSpeed    = false;
  changeHeading  = false;
  changeAltitude = false;
  resolveCircularWait = false;

  double speed   = 0;
  double heading = 0;
  double alt     = 0;
}

bool FGATCInstruction::hasInstruction()
{
  return (holdPattern || holdPosition || changeSpeed || changeHeading || changeAltitude || resolveCircularWait);
}

string FGATCController::getGateName(FGAIAircraft *ref) 
{
    return ref->atGate();
}

void FGATCController::transmit(FGTrafficRecord *rec, AtcMsgId msgId, AtcMsgDir msgDir)
{
    string sender, receiver;
    int stationFreq = 0;
    int taxiFreq    = 0;
    string atisInformation;
    //double commFreqD;
    switch (msgDir) {
         case ATC_AIR_TO_GROUND:
             sender = rec->getAircraft()->getTrafficRef()->getCallSign();
             switch (rec->getLeg()) {
                 case 2:
                 case 3:
                     stationFreq =
                        rec->getAircraft()->getTrafficRef()->getDepartureAirport()->getDynamics()->getGroundFrequency(rec->getLeg());
                     taxiFreq =
                        rec->getAircraft()->getTrafficRef()->getDepartureAirport()->getDynamics()->getGroundFrequency(3);
                     receiver = rec->getAircraft()->getTrafficRef()->getDepartureAirport()->getName() + "-Ground";
                     atisInformation = rec->getAircraft()->getTrafficRef()->getDepartureAirport()->getDynamics()->getAtisInformation();
                     break;
                 case 4: 
                     receiver = rec->getAircraft()->getTrafficRef()->getDepartureAirport()->getName() + "-Tower";
                     break;
             }
             break;
         case ATC_GROUND_TO_AIR:
             receiver = rec->getAircraft()->getTrafficRef()->getCallSign();
             switch (rec->getLeg()) {
                 case 2:
                 case 3:
                 stationFreq =
                        rec->getAircraft()->getTrafficRef()->getDepartureAirport()->getDynamics()->getGroundFrequency(rec->getLeg());
                     taxiFreq =
                        rec->getAircraft()->getTrafficRef()->getDepartureAirport()->getDynamics()->getGroundFrequency(3);
                     sender = rec->getAircraft()->getTrafficRef()->getDepartureAirport()->getName() + "-Ground";
                     atisInformation = rec->getAircraft()->getTrafficRef()->getDepartureAirport()->getDynamics()->getAtisInformation();
                     break;

                 case 4: 
                     sender = rec->getAircraft()->getTrafficRef()->getDepartureAirport()->getName() + "-Tower";
                     break;
             }
             break;
         }
    string text;
    string taxiFreqStr;
    char buffer[7];
    double heading = 0;
    string activeRunway;
    string fltType;
    string rwyClass;
    string SID;
    string transponderCode;
    FGAIFlightPlan *fp;
    string fltRules;
    switch (msgId) {
          case MSG_ANNOUNCE_ENGINE_START:
               text = sender + ". Ready to Start up";
               break;
          case MSG_REQUEST_ENGINE_START:
               text = receiver + ", This is " + sender + ". Position " +getGateName(rec->getAircraft()) +
                       ". Information " + atisInformation + ". " +
                       rec->getAircraft()->getTrafficRef()->getFlightRules() + " to " + 
                       rec->getAircraft()->getTrafficRef()->getArrivalAirport()->getName() + ". Request start-up";
               break;
          // Acknowledge engine startup permission
          // Assign departure runway
          // Assign SID, if necessery (TODO)
          case MSG_PERMIT_ENGINE_START:
               taxiFreqStr = formatATCFrequency3_2(taxiFreq);
               
               heading = rec->getAircraft()->getTrafficRef()->getCourse();
               fltType = rec->getAircraft()->getTrafficRef()->getFlightType();
               rwyClass= rec->getAircraft()->GetFlightPlan()->getRunwayClassFromTrafficType(fltType);

               rec->getAircraft()->getTrafficRef()->getDepartureAirport()->getDynamics()->getActiveRunway(rwyClass, 1, activeRunway, heading);
               rec->getAircraft()->GetFlightPlan()->setRunway(activeRunway);
               fp = rec->getAircraft()->getTrafficRef()->getDepartureAirport()->getDynamics()->getSID(activeRunway, heading);
               rec->getAircraft()->GetFlightPlan()->setSID(fp);
               if (fp) {
                   SID = fp->getName() + " departure";
               } else {
                   SID = "fly runway heading ";
               }
               //snprintf(buffer, 7, "%3.2f", heading);
               fltRules = rec->getAircraft()->getTrafficRef()->getFlightRules();
               transponderCode = genTransponderCode(fltRules);
               rec->getAircraft()->SetTransponderCode(transponderCode);
               text = receiver + ". Start-up approved. " + atisInformation + " correct, runway " + activeRunway
                       + ", " + SID + ", squawk " + transponderCode + ". " +
                      "For push-back and taxi clearance call " + taxiFreqStr + ". " + sender + " control.";
               break;
          case MSG_DENY_ENGINE_START:
               text = receiver + ". Standby";
               break;
         case MSG_ACKNOWLEDGE_ENGINE_START:
               fp = rec->getAircraft()->GetFlightPlan()->getSID();
               if (fp) {
                  SID = rec->getAircraft()->GetFlightPlan()->getSID()->getName() + " departure";
               } else {
                  SID = "fly runway heading ";
               }
               taxiFreqStr = formatATCFrequency3_2(taxiFreq);
               activeRunway = rec->getAircraft()->GetFlightPlan()->getRunway();
               transponderCode = rec->getAircraft()->GetTransponderCode();
               text = receiver + ". Start-up approved. " + atisInformation + " correct, runway " +
                      activeRunway + ", " + SID + ", squawk " + transponderCode + ". " +
                      "For push-back and taxi clearance call " + taxiFreqStr + ". " + sender;
               break;
           default:
               break;
    }
    double onBoardRadioFreq0 = fgGetDouble("/instrumentation/comm[0]/frequencies/selected-mhz");
    double onBoardRadioFreq1 = fgGetDouble("/instrumentation/comm[1]/frequencies/selected-mhz");
    int onBoardRadioFreqI0 = (int) floor(onBoardRadioFreq0 * 100 + 0.5);
    int onBoardRadioFreqI1 = (int) floor(onBoardRadioFreq1 * 100 + 0.5);
    //cerr << "Using " << onBoardRadioFreq0 << ", " << onBoardRadioFreq1 << " and " << stationFreq << endl;

    // Display ATC message only when one of the radios is tuned
    // the relevant frequency.
    // Note that distance attenuation is currently not yet implemented
    //cerr << "Transmitting " << text << " at " << stationFreq;
    if ((onBoardRadioFreqI0 == stationFreq) || (onBoardRadioFreqI1 == stationFreq)) {
        fgSetString("/sim/messages/atc", text.c_str());
        //cerr << "Printing Message: " << endl;
    }
}

string FGATCController::formatATCFrequency3_2(int freq) {
    char buffer[7];
    snprintf(buffer, 7, "%3.2f", ( (float) freq / 100.0) );
    return string(buffer);
}

string FGATCController::genTransponderCode(string fltRules) {
    if (fltRules == "VFR") {
        return string("1200");
    } else {
        char buffer[5];
        snprintf(buffer, 5, "%d%d%d%d", rand() % 8, rand() % 8,rand() % 8, rand() % 8);
        return string(buffer);
    }
}

/***************************************************************************
 * class FGTowerController
 *
 **************************************************************************/
FGTowerController::FGTowerController() :
  FGATCController()
{
}

// 
void FGTowerController::announcePosition(int id, FGAIFlightPlan *intendedRoute, int currentPosition,
					 double lat, double lon, double heading, 
					 double speed, double alt, double radius, int leg,
					 FGAIAircraft *ref)
{
  TrafficVectorIterator i = activeTraffic.begin();
  // Search whether the current id alread has an entry
  // This might be faster using a map instead of a vector, but let's start by taking a safe route
  if (activeTraffic.size()) {
    //while ((i->getId() != id) && i != activeTraffic.end()) {
    while (i != activeTraffic.end()) {
      if (i->getId() == id) {
	break;
      }
      i++;
    }
  }
  
  // Add a new TrafficRecord if no one exsists for this aircraft.
  if (i == activeTraffic.end() || (activeTraffic.size() == 0)) {
    FGTrafficRecord rec;
    rec.setId(id);

    rec.setPositionAndHeading(lat, lon, heading, speed, alt);
    rec.setRunway(intendedRoute->getRunway());
    rec.setLeg(leg);
    //rec.setCallSign(callsign);
    rec.setAircraft(ref);
    activeTraffic.push_back(rec);
  } else {
    i->setPositionAndHeading(lat, lon, heading, speed, alt);
  }
}

void FGTowerController::update(int id, double lat, double lon, double heading, double speed, double alt, 
			     double dt)
{
    TrafficVectorIterator i = activeTraffic.begin();
    // Search search if the current id has an entry
    // This might be faster using a map instead of a vector, but let's start by taking a safe route
    TrafficVectorIterator current, closest;
    if (activeTraffic.size()) {
      //while ((i->getId() != id) && i != activeTraffic.end()) {
      while (i != activeTraffic.end()) {
	if (i->getId() == id) {
	  break;
	}
        i++;
      }
    }

//    // update position of the current aircraft
    if (i == activeTraffic.end() || (activeTraffic.size() == 0)) {
      SG_LOG(SG_GENERAL, SG_ALERT, "AI error: updating aircraft without traffic record");
    } else {
      i->setPositionAndHeading(lat, lon, heading, speed, alt);
      current = i;
    }
    setDt(getDt() + dt);

//    // see if we already have a clearance record for the currently active runway
    ActiveRunwayVecIterator rwy = activeRunways.begin();
    // again, a map might be more efficient here
    if (activeRunways.size()) {
      //while ((rwy->getRunwayName() != current->getRunway()) && (rwy != activeRunways.end())) {
      while (rwy != activeRunways.end()) {
	if (rwy->getRunwayName() == current->getRunway()) {
	  break;
	}
        rwy++;
      }
    }
    if (rwy == activeRunways.end()) {
      ActiveRunway aRwy(current->getRunway(), id);
      activeRunways.push_back(aRwy);   // Since there are no clearance records for this runway yet
      current->setHoldPosition(false); // Clear the current aircraft to continue
    }
    else {
      // Okay, we have a clearance record for this runway, so check
      // whether the clearence ID matches that of the current aircraft
      if (id == rwy->getCleared()) {
        current->setHoldPosition(false);
      } else {
        current->setHoldPosition(true);
      }
    }
}


void FGTowerController::signOff(int id) 
{
  TrafficVectorIterator i = activeTraffic.begin();
  // Search search if the current id alread has an entry
  // This might be faster using a map instead of a vector, but let's start by taking a safe route
    if (activeTraffic.size()) {
      //while ((i->getId() != id) && i != activeTraffic.end()) {
      while (i != activeTraffic.end()) {
	if (i->getId() == id) {
	  break;
	}
        i++;
      }
    }
    // If this aircraft has left the runway, we can clear the departure record for this runway
    ActiveRunwayVecIterator rwy = activeRunways.begin();
    if (activeRunways.size()) {
      //while ((rwy->getRunwayName() != i->getRunway()) && (rwy != activeRunways.end())) {
      while (rwy != activeRunways.end()) {
        if (rwy->getRunwayName() == i->getRunway()) {
	  break;
	}
        rwy++;
      }
      if (rwy != activeRunways.end()) {
        rwy = activeRunways.erase(rwy);
      } else {
        SG_LOG(SG_GENERAL, SG_ALERT, "AI error: Attempting to erase non-existing runway clearance record in FGTowerController::signoff");
      }
    }
    if (i == activeTraffic.end() || (activeTraffic.size() == 0)) {
      SG_LOG(SG_GENERAL, SG_ALERT, "AI error: Aircraft without traffic record is signing off from tower");
    } else {
      i = activeTraffic.erase(i);
    }
}

// NOTE:
// IF WE MAKE TRAFFICRECORD A MEMBER OF THE BASE CLASS
// THE FOLLOWING THREE FUNCTIONS: SIGNOFF, HAS INSTRUCTION AND GETINSTRUCTION CAN 
// BECOME DEVIRTUALIZED AND BE A MEMBER OF THE BASE ATCCONTROLLER CLASS
// WHICH WOULD SIMPLIFY CODE MAINTENANCE.
// Note that this function is probably obsolete
bool FGTowerController::hasInstruction(int id)
{
    TrafficVectorIterator i = activeTraffic.begin();
    // Search search if the current id has an entry
    // This might be faster using a map instead of a vector, but let's start by taking a safe route
    if (activeTraffic.size()) 
      {
        //while ((i->getId() != id) && i != activeTraffic.end()) {
	while (i != activeTraffic.end()) {
	  if (i->getId() == id) {
	    break;
	  }
        i++;
      }
    }
    if (i == activeTraffic.end() || (activeTraffic.size() == 0)) {
      SG_LOG(SG_GENERAL, SG_ALERT, "AI error: checking ATC instruction for aircraft without traffic record");
    } else {
      return i->hasInstruction();
    }
  return false;
}


FGATCInstruction FGTowerController::getInstruction(int id)
{
  TrafficVectorIterator i = activeTraffic.begin();
  // Search search if the current id has an entry
  // This might be faster using a map instead of a vector, but let's start by taking a safe route
  if (activeTraffic.size()) {
    //while ((i->getId() != id) && i != activeTraffic.end()) {
    while (i != activeTraffic.end()) {
      if (i->getId() == id) {
	break;
      }
      i++;
    }
  }
  if (i == activeTraffic.end() || (activeTraffic.size() == 0)) {
    SG_LOG(SG_GENERAL, SG_ALERT, "AI error: requesting ATC instruction for aircraft without traffic record");
  } else {
    return i->getInstruction();
  }
  return FGATCInstruction();
}

/***************************************************************************
 * class FGStartupController
 *
 **************************************************************************/
FGStartupController::FGStartupController() :
  FGATCController()
{
    available        = false;
    lastTransmission = 0;
}

void FGStartupController::announcePosition(int id, FGAIFlightPlan *intendedRoute, int currentPosition,
					 double lat, double lon, double heading, 
					 double speed, double alt, double radius, int leg,
					 FGAIAircraft *ref)
{
  TrafficVectorIterator i = activeTraffic.begin();
  // Search whether the current id alread has an entry
  // This might be faster using a map instead of a vector, but let's start by taking a safe route
  if (activeTraffic.size()) {
    //while ((i->getId() != id) && i != activeTraffic.end()) {
    while (i != activeTraffic.end()) {
      if (i->getId() == id) {
	break;
      }
      i++;
    }
  }
  
  // Add a new TrafficRecord if no one exsists for this aircraft.
  if (i == activeTraffic.end() || (activeTraffic.size() == 0)) {
    FGTrafficRecord rec;
    rec.setId(id);

    rec.setPositionAndHeading(lat, lon, heading, speed, alt);
    rec.setRunway(intendedRoute->getRunway());
    rec.setLeg(leg);
    //rec.setCallSign(callsign);
    rec.setAircraft(ref);
    rec.setHoldPosition(true);
    activeTraffic.push_back(rec);
    } else {
        i->setPositionAndHeading(lat, lon, heading, speed, alt);
        
  }
}

// NOTE:
// IF WE MAKE TRAFFICRECORD A MEMBER OF THE BASE CLASS
// THE FOLLOWING THREE FUNCTIONS: SIGNOFF, HAS INSTRUCTION AND GETINSTRUCTION CAN 
// BECOME DEVIRTUALIZED AND BE A MEMBER OF THE BASE ATCCONTROLLER CLASS
// WHICH WOULD SIMPLIFY CODE MAINTENANCE.
// Note that this function is probably obsolete
bool FGStartupController::hasInstruction(int id)
{
    TrafficVectorIterator i = activeTraffic.begin();
    // Search search if the current id has an entry
    // This might be faster using a map instead of a vector, but let's start by taking a safe route
    if (activeTraffic.size()) 
      {
        //while ((i->getId() != id) && i != activeTraffic.end()) {
	while (i != activeTraffic.end()) {
	  if (i->getId() == id) {
	    break;
	  }
        i++;
      }
    }
    if (i == activeTraffic.end() || (activeTraffic.size() == 0)) {
      SG_LOG(SG_GENERAL, SG_ALERT, "AI error: checking ATC instruction for aircraft without traffic record");
    } else {
      return i->hasInstruction();
    }
  return false;
}


FGATCInstruction FGStartupController::getInstruction(int id)
{
  TrafficVectorIterator i = activeTraffic.begin();
  // Search search if the current id has an entry
  // This might be faster using a map instead of a vector, but let's start by taking a safe route
  if (activeTraffic.size()) {
    //while ((i->getId() != id) && i != activeTraffic.end()) {
    while (i != activeTraffic.end()) {
      if (i->getId() == id) {
	break;
      }
      i++;
    }
  }
  if (i == activeTraffic.end() || (activeTraffic.size() == 0)) {
    SG_LOG(SG_GENERAL, SG_ALERT, "AI error: requesting ATC instruction for aircraft without traffic record");
  } else {
    return i->getInstruction();
  }
  return FGATCInstruction();
}

void FGStartupController::signOff(int id) 
{
  TrafficVectorIterator i = activeTraffic.begin();
  // Search search if the current id alread has an entry
  // This might be faster using a map instead of a vector, but let's start by taking a safe route
    if (activeTraffic.size()) {
      //while ((i->getId() != id) && i != activeTraffic.end()) {
      while (i != activeTraffic.end()) {
	if (i->getId() == id) {
	  break;
	}
        i++;
      }
    }
    if (i == activeTraffic.end() || (activeTraffic.size() == 0)) {
      SG_LOG(SG_GENERAL, SG_ALERT, "AI error: Aircraft without traffic record is signing off from tower");
    } else {
      i = activeTraffic.erase(i);
    }
}

void FGStartupController::update(int id, double lat, double lon, double heading, double speed, double alt, 
			     double dt)
{
    TrafficVectorIterator i = activeTraffic.begin();
    // Search search if the current id has an entry
    // This might be faster using a map instead of a vector, but let's start by taking a safe route
    TrafficVectorIterator current, closest;
    if (activeTraffic.size()) {
      //while ((i->getId() != id) && i != activeTraffic.end()) {
      while (i != activeTraffic.end()) {
	if (i->getId() == id) {
	  break;
	}
        i++;
      }
    }

//    // update position of the current aircraft
    if (i == activeTraffic.end() || (activeTraffic.size() == 0)) {
      SG_LOG(SG_GENERAL, SG_ALERT, "AI error: updating aircraft without traffic record");
    } else {
      i->setPositionAndHeading(lat, lon, heading, speed, alt);
      current = i;
    }
    setDt(getDt() + dt);

    int state = i->getState();
    time_t startTime = i->getAircraft()->getTrafficRef()->getDepartureTime();
    time_t now       = time(NULL) + fgGetLong("/sim/time/warp");
    //cerr << i->getAircraft()->getTrafficRef()->getCallSign() 
    //     << " is scheduled to depart in " << startTime-now << " seconds. Available = " << available
    //     << " at parking " << getGateName(i->getAircraft()) << endl;

    if ((now - lastTransmission) > 3 + (rand() % 15)) {
        available = true;
    }

    if ((state == 0) && available) {
        if (now > startTime) {
             //cerr << "Transmitting startup msg" << endl;
             transmit(&(*i), MSG_ANNOUNCE_ENGINE_START, ATC_AIR_TO_GROUND);
             i->updateState();
             lastTransmission = now;
             available = false;
        }
    }
    if ((state == 1) && available) {
        if (now > startTime+60) {
            transmit(&(*i), MSG_REQUEST_ENGINE_START, ATC_AIR_TO_GROUND);
            i->updateState();
            lastTransmission = now;
             available = false;
        }
    }
    if ((state == 2) && available) {
        if (now > startTime+80) {
            transmit(&(*i), MSG_PERMIT_ENGINE_START, ATC_GROUND_TO_AIR);
            i->updateState();
            lastTransmission = now;
             available = false;
        }
    }
    if ((state == 3) && available){
        if (now > startTime+100) {
            transmit(&(*i), MSG_ACKNOWLEDGE_ENGINE_START, ATC_AIR_TO_GROUND);
            i->updateState();
            lastTransmission = now;
             available = false;
        }
     }
     // TODO: Switch to APRON control and request pushback Clearance.
     // Get Push back clearance
     if ((state == 4) && available){
          i->setHoldPosition(false);
     }
}
