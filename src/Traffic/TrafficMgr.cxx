/******************************************************************************
 * TrafficMGr.cxx
 * Written by Durk Talsma, started May 5, 2004.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 **************************************************************************/
 
/* 
 * Traffic manager parses airlines timetable-like data and uses this to 
 * determine the approximate position of each AI aircraft in its database.
 * When an AI aircraft is close to the user's position, a more detailed 
 * AIModels based simulation is set up. 
 * 
 * I'm currently assuming the following simplifications:
 * 1) The earth is a perfect sphere
 * 2) Each aircraft flies a perfect great circle route.
 * 3) Each aircraft flies at a constant speed (with infinite accelerations and
 *    decelerations) 
 * 4) Each aircraft leaves at exactly the departure time. 
 * 5) Each aircraft arrives at exactly the specified arrival time. 
 *
 *
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <time.h>
#include <cstring>
#include <iostream>
#include <fstream>


#include <string>
#include <vector>
#include <algorithm>

#include <plib/ul.h>

#include <simgear/compiler.h>
#include <simgear/misc/sg_path.hxx>
#include <simgear/props/props.hxx>
#include <simgear/route/waypoint.hxx>
#include <simgear/structure/subsystem_mgr.hxx>
#include <simgear/xml/easyxml.hxx>

#include <AIModel/AIAircraft.hxx>
#include <AIModel/AIFlightPlan.hxx>
#include <AIModel/AIBase.hxx>
#include <Airports/simple.hxx>
#include <Main/fg_init.hxx>



#include "TrafficMgr.hxx"

using std::sort;
using std::strcmp;
 
/******************************************************************************
 * TrafficManager
 *****************************************************************************/
FGTrafficManager::FGTrafficManager()
{
  //score = 0;
  //runCount = 0;
  acCounter = 0;
}

FGTrafficManager:: ~FGTrafficManager()
{
  for (ScheduleVectorIterator sched = scheduledAircraft.begin(); sched != scheduledAircraft.end(); sched++)
    {
      delete (*sched);
    }
  scheduledAircraft.clear();
  flights.clear();
}


void FGTrafficManager::init()
{ 
  ulDir* d, *d2;
  ulDirEnt* dent, *dent2;
  SGPath aircraftDir = globals->get_fg_root();

  SGPath path = aircraftDir;
  
  aircraftDir.append("AI/Traffic");
  if ((d = ulOpenDir(aircraftDir.c_str())) != NULL)
    {
      while((dent = ulReadDir(d)) != NULL) {
	if (string(dent->d_name) != string(".")  && 
	    string(dent->d_name) != string("..") &&
	    dent->d_isdir)
	  {
	    SGPath currACDir = aircraftDir;
	    currACDir.append(dent->d_name);
	    if ((d2 = ulOpenDir(currACDir.c_str())) == NULL)
	      return;
	    while ((dent2 = ulReadDir(d2)) != NULL) {
	      SGPath currFile = currACDir;
	      currFile.append(dent2->d_name);
	      if (currFile.extension() == string("xml"))
		{
		  SGPath currFile = currACDir;
		  currFile.append(dent2->d_name);
		  SG_LOG(SG_GENERAL, SG_DEBUG, "Scanning " << currFile.str() << " for traffic");
		  readXML(currFile.str(),*this);
		}
	    }
	    ulCloseDir(d2);
	  }
      }
      ulCloseDir(d);
    }
    
    currAircraft = scheduledAircraft.begin();
    currAircraftClosest = scheduledAircraft.begin();
}

void FGTrafficManager::update(double /*dt*/)
{

  time_t now = time(NULL) + fgGetLong("/sim/time/warp");
  if (scheduledAircraft.size() == 0) {
    return;
  }
  
  SGVec3d userCart = SGVec3d::fromGeod(SGGeod::fromDeg(
    fgGetDouble("/position/longitude-deg"), 
    fgGetDouble("/position/latitude-deg")));
  
  if(currAircraft == scheduledAircraft.end())
    {
      currAircraft = scheduledAircraft.begin();
    }
  if (!((*currAircraft)->update(now, userCart)))
    {
      // NOTE: With traffic manager II, this statement below is no longer true
      // after proper initialization, we shouldnt get here.
      // But let's make sure
      //SG_LOG( SG_GENERAL, SG_ALERT, "Failed to update aircraft schedule in traffic manager");
    }
  currAircraft++;
}

void FGTrafficManager::release(int id)
{
  releaseList.push_back(id);
}

bool FGTrafficManager::isReleased(int id)
{
  IdListIterator i = releaseList.begin();
  while (i != releaseList.end())
    {
      if ((*i) == id)
	{
	  releaseList.erase(i);
	  return true;
	}
      i++;
    }
  return false;
}
/*
void FGTrafficManager::readTimeTableFromFile(SGPath infileName)
{
    string model;
    string livery;
    string homePort;
    string registration;
    string flightReq;
    bool   isHeavy;
    string acType;
    string airline;
    string m_class;
    string FlightType;
    double radius;
    double offset;

    char buffer[256];
    string buffString;
    vector <string> tokens, depTime,arrTime;
    vector <string>::iterator it;
    ifstream infile(infileName.str().c_str());
    while (1) {
         infile.getline(buffer, 256);
         if (infile.eof()) {
             break;
         }
         //cerr << "Read line : " << buffer << endl;
         buffString = string(buffer);
         tokens.clear();
         Tokenize(buffString, tokens, " \t");
         //for (it = tokens.begin(); it != tokens.end(); it++) {
         //    cerr << "Tokens: " << *(it) << endl;
         //}
         //cerr << endl;
         if (!tokens.empty()) {
             if (tokens[0] == string("AC")) {
                 if (tokens.size() != 13) {
                     SG_LOG(SG_GENERAL, SG_ALERT, "Error parsing traffic file " << infileName.str() << " at " << buffString);
                     exit(1);
                 }
                 model          = tokens[12];
                 livery         = tokens[6];
                 homePort       = tokens[1];
                 registration   = tokens[2];
                 if (tokens[11] == string("false")) {
                     isHeavy = false;
                 } else {
                     isHeavy = true;
                 }
                 acType         = tokens[4];
                 airline        = tokens[5];
                 flightReq      = tokens[3] + tokens[5];
                 m_class        = tokens[10];
                 FlightType     = tokens[9];
                 radius         = atof(tokens[8].c_str());
                 offset         = atof(tokens[7].c_str());;
                 //cerr << "Found AC string " << model << " " << livery << " " << homePort << " " 
                 //     << registration << " " << flightReq << " " << isHeavy << " " << acType << " " << airline << " " << m_class 
                 //     << " " << FlightType << " " << radius << " " << offset << endl;
                 scheduledAircraft.push_back(new FGAISchedule(model, 
                                                              livery, 
                                                              homePort,
                                                              registration, 
                                                              flightReq,
                                                              isHeavy,
                                                              acType, 
                                                              airline, 
                                                              m_class, 
                                                              FlightType,
                                                              radius,
                                                              offset));
             }
             if (tokens[0] == string("FLIGHT")) {
                 //cerr << "Found flight " << buffString << " size is : " << tokens.size() << endl;
                 if (tokens.size() != 10) {
                     SG_LOG(SG_GENERAL, SG_ALERT, "Error parsing traffic file " << infileName.str() << " at " << buffString);
                     exit(1);
                 }
                 string callsign = tokens[1];
                 string fltrules = tokens[2];
                 string weekdays = tokens[3];
                 string departurePort = tokens[5];
                 string arrivalPort   = tokens[7];
                 int    cruiseAlt     = atoi(tokens[8].c_str());
                 string depTimeGen    = tokens[4];
                 string arrTimeGen    = tokens[6];
                 string repeat        = "WEEK";
                 string requiredAircraft = tokens[9];
                 
                 if (weekdays.size() != 7) {
                     cerr << "Found misconfigured weekdays string" << weekdays << endl;
                     exit(1);
                 }
                 depTime.clear();
                 arrTime.clear();
                 Tokenize(depTimeGen, depTime, ":");
                 Tokenize(arrTimeGen, arrTime, ":");
                 double dep = atof(depTime[0].c_str()) + (atof(depTime[1].c_str()) / 60.0);
                 double arr = atof(arrTime[0].c_str()) + (atof(arrTime[1].c_str()) / 60.0);
                 //cerr << "Using " << dep << " " << arr << endl;
                 bool arrivalWeekdayNeedsIncrement = false;
                 if (arr < dep) {
                       arrivalWeekdayNeedsIncrement = true;
                 }
                 for (int i = 0; i < 7; i++) {
                     if (weekdays[i] != '.') {
                         char buffer[4];
                         snprintf(buffer, 4, "%d/", i);
                         string departureTime = string(buffer) + depTimeGen + string(":00");
                         string arrivalTime;
                         if (!arrivalWeekdayNeedsIncrement) {
                             arrivalTime   = string(buffer) + arrTimeGen + string(":00");
                         }
                         if (arrivalWeekdayNeedsIncrement && i != 6 ) {
                             snprintf(buffer, 4, "%d/", i+1);
                             arrivalTime   = string(buffer) + arrTimeGen + string(":00");
                         }
                         if (arrivalWeekdayNeedsIncrement && i == 6 ) {
                             snprintf(buffer, 4, "%d/", 0);
                             arrivalTime   = string(buffer) + arrTimeGen  + string(":00");
                         }
                         cerr << "Adding flight: " << callsign       << " "
                                                   << fltrules       << " "
                                                   <<  departurePort << " "
                                                   <<  arrivalPort   << " "
                                                   <<  cruiseAlt     << " "
                                                   <<  departureTime << " "
                                                   <<  arrivalTime   << " "
                                                   <<  repeat        << " " 
                                                   <<  requiredAircraft << endl;

                         flights[requiredAircraft].push_back(new FGScheduledFlight(callsign,
                                                                 fltrules,
                                                                 departurePort,
                                                                 arrivalPort,
                                                                 cruiseAlt,
                                                                 departureTime,
                                                                 arrivalTime,
                                                                 repeat,
                                                                 requiredAircraft));
                    }
                }
             }
         }

    }
    //exit(1);
}*/

/*
void FGTrafficManager::Tokenize(const string& str,
                      vector<string>& tokens,
                      const string& delimiters)
{
    // Skip delimiters at beginning.
    string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (string::npos != pos || string::npos != lastPos)
    {
        // Found a token, add it to the vector.
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
}
*/

void  FGTrafficManager::startXML () {
  //cout << "Start XML" << endl;
  requiredAircraft = "";
  homePort         = "";
}

void  FGTrafficManager::endXML () {
  //cout << "End XML" << endl;
}

void  FGTrafficManager::startElement (const char * name, const XMLAttributes &atts) {
  const char * attval;
  //cout << "Start element " << name << endl;
  //FGTrafficManager temp;
  //for (int i = 0; i < atts.size(); i++)
  //  if (string(atts.getName(i)) == string("include"))
  attval = atts.getValue("include");
  if (attval != 0)
      {
	//cout << "including " << attval << endl;
	SGPath path = 
	  globals->get_fg_root();
	path.append("/Traffic/");
	path.append(attval);
	readXML(path.str(), *this);
      }
  elementValueStack.push_back( "" );
  //  cout << "  " << atts.getName(i) << '=' << atts.getValue(i) << endl; 
}

void  FGTrafficManager::endElement (const char * name) {
  //cout << "End element " << name << endl;
  const string& value = elementValueStack.back();

  if (!strcmp(name, "model"))
    mdl = value;
  else if (!strcmp(name, "livery"))
    livery = value;
  else if (!strcmp(name, "home-port"))
    homePort = value;
  else if (!strcmp(name, "registration"))
    registration = value;
  else if (!strcmp(name, "airline"))
    airline = value;
  else if (!strcmp(name, "actype"))
    acType = value;
  else if (!strcmp(name, "required-aircraft"))
    requiredAircraft = value;
  else if (!strcmp(name, "flighttype"))
    flighttype = value;
  else if (!strcmp(name, "radius"))
    radius = atoi(value.c_str());
  else if (!strcmp(name, "offset"))
    offset = atoi(value.c_str());
  else if (!strcmp(name, "performance-class"))
    m_class = value;
  else if (!strcmp(name, "heavy"))
    {
      if(value == string("true"))
	heavy = true;
      else
	heavy = false;
    }
  else if (!strcmp(name, "callsign"))
    callsign = value;
  else if (!strcmp(name, "fltrules"))
    fltrules = value;
  else if (!strcmp(name, "port"))
    port = value;
  else if (!strcmp(name, "time"))
    timeString = value;
  else if (!strcmp(name, "departure"))
    {
      departurePort = port;
      departureTime = timeString;
    }
  else if (!strcmp(name, "cruise-alt"))
    cruiseAlt = atoi(value.c_str());
  else if (!strcmp(name, "arrival"))
    {
      arrivalPort = port;
      arrivalTime = timeString;
    }
  else if (!strcmp(name, "repeat"))
    repeat = value;
  else if (!strcmp(name, "flight"))
    {
      // We have loaded and parsed all the information belonging to this flight
      // so we temporarily store it. 
      //cerr << "Pusing back flight " << callsign << endl;
      //cerr << callsign  <<  " " << fltrules     << " "<< departurePort << " " <<  arrivalPort << " "
      //   << cruiseAlt <<  " " << departureTime<< " "<< arrivalTime   << " " << repeat << endl;

      //Prioritize aircraft 
      string apt = fgGetString("/sim/presets/airport-id");
      //cerr << "Airport information: " << apt << " " << departurePort << " " << arrivalPort << endl;
      //if (departurePort == apt) score++;
      //flights.push_back(new FGScheduledFlight(callsign,
	//				  fltrules,
	//				  departurePort,
	//				  arrivalPort,
	//				  cruiseAlt,
	//				  departureTime,
	//				  arrivalTime,
	//				  repeat));
    if (requiredAircraft == "") {
        char buffer[16];
        snprintf(buffer, 16, "%d", acCounter);
        requiredAircraft = buffer;
    }
    SG_LOG(SG_GENERAL, SG_DEBUG, "Adding flight: " << callsign       << " "
                              << fltrules       << " "
                              <<  departurePort << " "
                              <<  arrivalPort   << " "
                              <<  cruiseAlt     << " "
                              <<  departureTime << " "
                              <<  arrivalTime   << " "
                              <<  repeat        << " " 
                              <<  requiredAircraft);

     flights[requiredAircraft].push_back(new FGScheduledFlight(callsign,
                                                                 fltrules,
                                                                 departurePort,
                                                                 arrivalPort,
                                                                 cruiseAlt,
                                                                 departureTime,
                                                                 arrivalTime,
                                                                 repeat,
                                                                 requiredAircraft));
      requiredAircraft = "";
  }
  else if (!strcmp(name, "aircraft"))
    {
      int proportion = (int) (fgGetDouble("/sim/traffic-manager/proportion") * 100);
      int randval = rand() & 100;
      if (randval < proportion) {
          //scheduledAircraft.push_back(new FGAISchedule(mdl, 
	//				       livery, 
	//				       registration, 
	//				       heavy,
	//				       acType, 
	//				       airline, 
	//				       m_class, 
	//				       flighttype,
	//				       radius,
	//				       offset,
	//				       score,
	//				       flights));
    if (requiredAircraft == "") {
        char buffer[16];
        snprintf(buffer, 16, "%d", acCounter);
        requiredAircraft = buffer;
    }
    if (homePort == "") {
        homePort = departurePort;
    }
            scheduledAircraft.push_back(new FGAISchedule(mdl, 
                                                         livery, 
                                                         homePort,
                                                         registration, 
                                                         requiredAircraft,
                                                         heavy,
                                                         acType, 
                                                         airline, 
                                                         m_class, 
                                                         flighttype,
                                                         radius,
                                                         offset));

     //  while(flights.begin() != flights.end()) {
// 	flights.pop_back();
//       }
        }
    acCounter++;
    requiredAircraft = "";
    homePort = "";
  //for (FGScheduledFlightVecIterator flt = flights.begin(); flt != flights.end(); flt++)
  //  {
  //    delete (*flt);
  //  }
  //flights.clear();
      SG_LOG( SG_GENERAL, SG_BULK, "Reading aircraft : " 
	      << registration 
	      << " with prioritization score " 
	      << score);
      score = 0;
    }
  elementValueStack.pop_back();
}

void  FGTrafficManager::data (const char * s, int len) {
  string token = string(s,len);
  //cout << "Character data " << string(s,len) << endl;
  elementValueStack.back() += token;
}

void  FGTrafficManager::pi (const char * target, const char * data) {
  //cout << "Processing instruction " << target << ' ' << data << endl;
}

void  FGTrafficManager::warning (const char * message, int line, int column) {
  SG_LOG(SG_IO, SG_WARN, "Warning: " << message << " (" << line << ',' << column << ')');
}

void  FGTrafficManager::error (const char * message, int line, int column) {
  SG_LOG(SG_IO, SG_ALERT, "Error: " << message << " (" << line << ',' << column << ')');
}

