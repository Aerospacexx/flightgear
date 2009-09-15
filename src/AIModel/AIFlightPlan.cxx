// FGAIFlightPlan - class for loading and storing  AI flight plans
// Written by David Culp, started May 2004
// - davidculp2@comcast.net
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <simgear/misc/sg_path.hxx>
#include <simgear/debug/logstream.hxx>
#include <simgear/route/waypoint.hxx>
#include <simgear/math/sg_geodesy.hxx>
#include <simgear/structure/exception.hxx>
#include <simgear/constants.h>
#ifdef __BORLANDC__
#  define exception c_exception
#endif
#include <simgear/props/props.hxx>
#include <Main/globals.hxx>
#include <Main/fg_props.hxx>
#include <Main/fg_init.hxx>
#include <Airports/simple.hxx>
#include <Airports/runways.hxx>


#include <Environment/environment_mgr.hxx>
#include <Environment/environment.hxx>

#include "AIFlightPlan.hxx"


FGAIFlightPlan::FGAIFlightPlan(const string& filename)
{
  int i;
  start_time = 0;
  leg = 10;
  gateId = 0;
  taxiRoute = 0;
  SGPath path( globals->get_fg_root() );
  path.append( ("/AI/FlightPlans/" + filename).c_str() );
  SGPropertyNode root;
  repeat = false;

  try {
      readProperties(path.str(), &root);
  } catch (const sg_exception &e) {
      SG_LOG(SG_GENERAL, SG_ALERT,
       "Error reading AI flight plan: " << path.str());
       // cout << path.str() << endl;
     return;
  }

  SGPropertyNode * node = root.getNode("flightplan");
  for (i = 0; i < node->nChildren(); i++) { 
     //cout << "Reading waypoint " << i << endl;        
     waypoint* wpt = new waypoint;
     SGPropertyNode * wpt_node = node->getChild(i);
     wpt->name      = wpt_node->getStringValue("name", "END");
     wpt->latitude  = wpt_node->getDoubleValue("lat", 0);
     wpt->longitude = wpt_node->getDoubleValue("lon", 0);
     wpt->altitude  = wpt_node->getDoubleValue("alt", 0);
     wpt->speed     = wpt_node->getDoubleValue("ktas", 0);
     wpt->crossat   = wpt_node->getDoubleValue("crossat", -10000);
     wpt->gear_down = wpt_node->getBoolValue("gear-down", false);
     wpt->flaps_down= wpt_node->getBoolValue("flaps-down", false);
     wpt->on_ground = wpt_node->getBoolValue("on-ground", false);
     wpt->time_sec   = wpt_node->getDoubleValue("time-sec", 0);
     wpt->time       = wpt_node->getStringValue("time", "");

     if (wpt->name == "END") wpt->finished = true;
     else wpt->finished = false;

     waypoints.push_back( wpt );
   }

  wpt_iterator = waypoints.begin();
  //cout << waypoints.size() << " waypoints read." << endl;
}


// This is a modified version of the constructor,
// Which not only reads the waypoints from a 
// Flight plan file, but also adds the current
// Position computed by the traffic manager, as well
// as setting speeds and altitude computed by the
// traffic manager. 
FGAIFlightPlan::FGAIFlightPlan(const std::string& p,
			       double course,
			       time_t start,
			       FGAirport *dep,
			       FGAirport *arr,
			       bool firstLeg,
			       double radius,
                               double alt,
                               double lat,
                               double lon,
                               double speed,
			       const string& fltType,
			       const string& acType,
			       const string& airline)
{
  repeat = false;
  leg = 10;
  gateId=0;
  taxiRoute = 0;
  start_time = start;
  bool useInitialWayPoint = true;
  bool useCurrentWayPoint = false;
  SGPath path( globals->get_fg_root() );
  path.append( "/AI/FlightPlans" );
  path.append( p );
  
  SGPropertyNode root;
  
  // This is a bit of a hack:
  // Normally the value of course will be used to evaluate whether
  // or not a waypoint will be used for midair initialization of 
  // an AI aircraft. However, if a course value of 999 will be passed
  // when an update request is received, which will by definition always be
  // on the ground and should include all waypoints.
  if (course == 999) 
    {
      useInitialWayPoint = false;
      useCurrentWayPoint = true;
    }

  if (path.exists()) 
    {
      try 
	{
	  readProperties(path.str(), &root);
	  
	  SGPropertyNode * node = root.getNode("flightplan");
	  
	  //waypoints.push_back( init_waypoint );
	  for (int i = 0; i < node->nChildren(); i++) { 
	    //cout << "Reading waypoint " << i << endl;
	    waypoint* wpt = new waypoint;
	    SGPropertyNode * wpt_node = node->getChild(i);
	    wpt->name      = wpt_node->getStringValue("name", "END");
	    wpt->latitude  = wpt_node->getDoubleValue("lat", 0);
	    wpt->longitude = wpt_node->getDoubleValue("lon", 0);
	    wpt->altitude  = wpt_node->getDoubleValue("alt", 0);
	    wpt->speed     = wpt_node->getDoubleValue("ktas", 0);
	    //wpt->speed     = speed;
	    wpt->crossat   = wpt_node->getDoubleValue("crossat", -10000);
	    wpt->gear_down = wpt_node->getBoolValue("gear-down", false);
	    wpt->flaps_down= wpt_node->getBoolValue("flaps-down", false);
	    
	    if (wpt->name == "END") wpt->finished = true;
	    else wpt->finished = false;
	    waypoints.push_back(wpt);
	  }
	}
      catch (const sg_exception &e) {
	SG_LOG(SG_GENERAL, SG_WARN,
	       "Error reading AI flight plan: ");
	cerr << "Errno = " << errno << endl;
	if (errno == ENOENT)
	  {
	    cerr << "Reason: No such file or directory" << endl;
	  }
      }
    }
  else
    {
      // cout << path.str() << endl;
      // cout << "Trying to create this plan dynamically" << endl;
      // cout << "Route from " << dep->id << " to " << arr->id << endl;
      time_t now = time(NULL) + fgGetLong("/sim/time/warp");
      time_t timeDiff = now-start; 
      leg = 1;
      if ((timeDiff > 300) && (timeDiff < 1200))
	leg = 2;
      else if ((timeDiff >= 1200) && (timeDiff < 1500))
	leg = 3;
      else if ((timeDiff >= 1500) && (timeDiff < 2000))
	leg = 4;
      else if (timeDiff >= 2000)
	leg = 5;
      
      //cerr << "Set leg to : " << leg << endl;  
      wpt_iterator = waypoints.begin();
      create(dep,arr, leg, alt, speed, lat, lon,
	     firstLeg, radius, fltType, acType, airline);
      wpt_iterator = waypoints.begin();
      //cerr << "after create: " << (*wpt_iterator)->name << endl;
      //leg++;
      // Now that we have dynamically created a flight plan,
      // we need to add some code that pops any waypoints already past.
      //return;
    }
  /*
    waypoint* init_waypoint   = new waypoint;
    init_waypoint->name       = string("initial position");
    init_waypoint->latitude   = entity->latitude;
    init_waypoint->longitude  = entity->longitude;
    init_waypoint->altitude   = entity->altitude;
    init_waypoint->speed      = entity->speed;
    init_waypoint->crossat    = - 10000;
    init_waypoint->gear_down  = false;
    init_waypoint->flaps_down = false;
    init_waypoint->finished   = false;
    
    wpt_vector_iterator i = waypoints.begin();
    while (i != waypoints.end())
    {
      //cerr << "Checking status of each waypoint: " << (*i)->name << endl;
       SGWayPoint first(init_waypoint->longitude, 
		       init_waypoint->latitude, 
		       init_waypoint->altitude);
      SGWayPoint curr ((*i)->longitude, 
		       (*i)->latitude, 
		       (*i)->altitude);
      double crse, crsDiff;
      double dist;
      curr.CourseAndDistance(first, &crse, &dist);
      
      dist *= SG_METER_TO_NM;
      
      // We're only interested in the absolute value of crsDiff
      // wich should fall in the 0-180 deg range.
      crsDiff = fabs(crse-course);
      if (crsDiff > 180)
	crsDiff = 360-crsDiff;
      // These are the three conditions that we consider including
      // in our flight plan:
      // 1) current waypoint is less then 100 miles away OR
      // 2) curren waypoint is ahead of us, at any distance
     
      if ((dist > 20.0) && (crsDiff > 90.0) && ((*i)->name != string ("EOF")))
	{
	  //useWpt = false;
	  // Once we start including waypoints, we have to continue, even though
	  // one of the following way point would suffice. 
	  // so once is the useWpt flag is set to true, we cannot reset it to false.
	  //cerr << "Discarding waypoint: " << (*i)->name 
	  //   << ": Course difference = " << crsDiff
	  //  << "Course = " << course
	  // << "crse   = " << crse << endl;
	}
      else
	useCurrentWayPoint = true;
      
      if (useCurrentWayPoint)
	{
	  if ((dist > 100.0) && (useInitialWayPoint))
	    {
	      //waypoints.push_back(init_waypoint);;
	      waypoints.insert(i, init_waypoint);
	      //cerr << "Using waypoint : " << init_waypoint->name <<  endl;
	    }
	  //if (useInitialWayPoint)
	  // {
	  //    (*i)->speed = dist; // A hack
	  //  }
	  //waypoints.push_back( wpt );
	  //cerr << "Using waypoint : " << (*i)->name 
	  //  << ": course diff : " << crsDiff 
	  //   << "Course = " << course
	  //   << "crse   = " << crse << endl
	  //    << "distance      : " << dist << endl;
	  useInitialWayPoint = false;
	  i++;
	}
      else 
	{
	  //delete wpt;
	  delete *(i);
	  i = waypoints.erase(i);
	  }
	  
	}
  */
  //for (i = waypoints.begin(); i != waypoints.end(); i++)
  //  cerr << "Using waypoint : " << (*i)->name << endl;
  //wpt_iterator = waypoints.begin();
  //cout << waypoints.size() << " waypoints read." << endl;
}




FGAIFlightPlan::~FGAIFlightPlan()
{
  deleteWaypoints();
  delete taxiRoute;
}


FGAIFlightPlan::waypoint* const
FGAIFlightPlan::getPreviousWaypoint( void ) const
{
  if (wpt_iterator == waypoints.begin()) {
    return 0;
  } else {
    wpt_vector_iterator prev = wpt_iterator;
    return *(--prev);
  }
}

FGAIFlightPlan::waypoint* const
FGAIFlightPlan::getCurrentWaypoint( void ) const
{
  return *wpt_iterator;
}

FGAIFlightPlan::waypoint* const
FGAIFlightPlan::getNextWaypoint( void ) const
{
  wpt_vector_iterator i = waypoints.end();
  i--;  // end() points to one element after the last one. 
  if (wpt_iterator == i) {
    return 0;
  } else {
    wpt_vector_iterator next = wpt_iterator;
    return *(++next);
  }
}

void FGAIFlightPlan::IncrementWaypoint(bool eraseWaypoints )
{
  if (eraseWaypoints)
    {
      if (wpt_iterator == waypoints.begin())
	wpt_iterator++;
      else
	{
	  delete *(waypoints.begin());
	  waypoints.erase(waypoints.begin());
	  wpt_iterator = waypoints.begin();
	  wpt_iterator++;
	}
    }
  else
    wpt_iterator++;

}

// gives distance in feet from a position to a waypoint
double FGAIFlightPlan::getDistanceToGo(double lat, double lon, waypoint* wp) const{
   // get size of a degree2 at the present latitude
   // this won't work over large distances
   double ft_per_deg_lat = 366468.96 - 3717.12 * cos(lat / SG_RADIANS_TO_DEGREES);
   double ft_per_deg_lon = 365228.16 * cos(lat / SG_RADIANS_TO_DEGREES);
   double lat_diff_ft = fabs(wp->latitude - lat) * ft_per_deg_lat;
   double lon_diff_ft = fabs(wp->longitude - lon) * ft_per_deg_lon;
   return sqrt((lat_diff_ft * lat_diff_ft) + (lon_diff_ft * lon_diff_ft));
}

// sets distance in feet from a lead point to the current waypoint
void FGAIFlightPlan::setLeadDistance(double speed, double bearing, 
                                     waypoint* current, waypoint* next){
  double turn_radius;
    if (fabs(speed) > 1) 
      turn_radius = 0.1911 * speed * speed; // an estimate for 25 degrees bank
    else
      turn_radius = 1.0;

  double inbound = bearing;
  double outbound = getBearing(current, next);
  leadInAngle = fabs(inbound - outbound);
  if (leadInAngle > 180.0) 
    leadInAngle = 360.0 - leadInAngle;
  if (leadInAngle < 1.0) // To prevent lead_dist from getting so small it is skipped 
    leadInAngle = 1.0;
  
  lead_distance = turn_radius * sin(leadInAngle * SG_DEGREES_TO_RADIANS); 
  //  if ((errno == EDOM) || (errno == ERANGE) || lead_distance < 1.0)
  //  {
  //    cerr << "Lead Distance = " << lead_distance
  //	   << "Diff          = " << diff
  //	   << "Turn Radius   = " << turn_radius 
  //	   << "Speed         = " << speed << endl;
  //  }
}

void FGAIFlightPlan::setLeadDistance(double distance_ft){
  lead_distance = distance_ft;
}


double FGAIFlightPlan::getBearing(waypoint* first, waypoint* second) const{
  return getBearing(first->latitude, first->longitude, second);
}


double FGAIFlightPlan::getBearing(double lat, double lon, waypoint* wp) const{
  double course, distance;
 //  double latd = lat;
//   double lond = lon;
//   double latt = wp->latitude;
//   double lont = wp->longitude;
//   double ft_per_deg_lat = 366468.96 - 3717.12 * cos(lat/SG_RADIANS_TO_DEGREES);
//   double ft_per_deg_lon = 365228.16 * cos(lat/SG_RADIANS_TO_DEGREES);

//   if (lond < 0.0) {
//     lond+=360.0;
//     lont+=360;
//   }
//   if (lont < 0.0) {
//     lond+=360.0;
//     lont+=360.0;
//   }
//   latd+=90.0;
//   latt+=90.0;

//   double lat_diff = (latt - latd) * ft_per_deg_lat;
//   double lon_diff = (lont - lond) * ft_per_deg_lon;
//   double angle = atan(fabs(lat_diff / lon_diff)) * SG_RADIANS_TO_DEGREES;

//   bool southerly = true;
//   if (latt > latd) southerly = false;
//   bool easterly = false;
//   if (lont > lond) easterly = true;
//   if (southerly && easterly) return 90.0 + angle;
//   if (!southerly && easterly) return 90.0 - angle;
//   if (southerly && !easterly) return 270.0 - angle;
//   if (!southerly && !easterly) return 270.0 + angle; 
  SGWayPoint sgWp(wp->longitude,wp->latitude, wp->altitude, SGWayPoint::WGS84, string("temp"));
  sgWp.CourseAndDistance(lon, lat, wp->altitude, &course, &distance);
  
  return course;
  // Omit a compiler warning. 
  //if ((errno == EDOM) || (errno == ERANGE))
  //  {
  //    cerr << "Lon:  " << wp->longitude
  //	   << "Lat       = " << wp->latitude
  //   << "Tgt Lon   = " <<  
  //   << "TgT Lat   = " << speed << endl;
  //  }
  
}



void FGAIFlightPlan::deleteWaypoints()
{
  for (wpt_vector_iterator i = waypoints.begin(); i != waypoints.end();i++)
    delete (*i);
  waypoints.clear();
}

// Delete all waypoints except the last, 
// which we will recycle as the first waypoint in the next leg;
void FGAIFlightPlan::resetWaypoints()
{
  if (waypoints.begin() == waypoints.end())
    return;
  else
    {
      waypoint *wpt = new waypoint;
      wpt_vector_iterator i = waypoints.end();
      i--;
      wpt->name      = (*i)->name;
      wpt->latitude  = (*i)->latitude;
      wpt->longitude =  (*i)->longitude;
      wpt->altitude  =  (*i)->altitude;
      wpt->speed     =  (*i)->speed;
      wpt->crossat   =  (*i)->crossat;
      wpt->gear_down =  (*i)->gear_down;
      wpt->flaps_down=  (*i)->flaps_down;
      wpt->finished  = false;
      wpt->on_ground =  (*i)->on_ground;
      //cerr << "Recycling waypoint " << wpt->name << endl;
      deleteWaypoints();
      waypoints.push_back(wpt);
    }
}

// Start flightplan over from the beginning
void FGAIFlightPlan::restart()
{
  wpt_iterator = waypoints.begin();
}


void FGAIFlightPlan::deleteTaxiRoute() 
{
  delete taxiRoute;
  taxiRoute = 0;
}


int FGAIFlightPlan::getRouteIndex(int i) {
  if ((i > 0) && (i < waypoints.size())) {
    return waypoints[i]->routeIndex;
  }
  else
    return 0;
}
