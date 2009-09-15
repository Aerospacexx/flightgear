// groundnet.cxx - Implimentation of the FlightGear airport ground handling code
//
// Written by Durk Talsma, started June 2005.
//
// Copyright (C) 2004 Durk Talsma.
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
#include <algorithm>

//#include <plib/sg.h>
//#include <plib/ul.h>

//#include <Environment/environment_mgr.hxx>
//#include <Environment/environment.hxx>
//#include <simgear/misc/sg_path.hxx>
//#include <simgear/props/props.hxx>
//#include <simgear/structure/subsystem_mgr.hxx>
#include <simgear/debug/logstream.hxx>
#include <simgear/route/waypoint.hxx>
//#include <Main/globals.hxx>
//#include <Main/fg_props.hxx>
//#include <Airports/runways.hxx>


#include <AIModel/AIFlightPlan.hxx>

//#include STL_STRING

#include "groundnetwork.hxx"

SG_USING_STD(sort);



/**************************************************************************
 * FGTaxiNode
 *************************************************************************/
FGTaxiNode::FGTaxiNode()
{
}

void FGTaxiNode::sortEndSegments(bool byLength)
{
  if (byLength)
    sort(next.begin(), next.end(), sortByLength);
  else
    sort(next.begin(), next.end(), sortByHeadingDiff);
}


bool compare_nodes(FGTaxiNode *a, FGTaxiNode *b) {
return (*a) < (*b);
}

/***************************************************************************
 * FGTaxiSegment
 **************************************************************************/
FGTaxiSegment::FGTaxiSegment()
{
  oppositeDirection = 0;
  isActive = true;
}

void FGTaxiSegment::setStart(FGTaxiNodeVector *nodes)
{
  FGTaxiNodeVectorIterator i = nodes->begin();
  while (i != nodes->end())
    {
      if ((*i)->getIndex() == startNode)
	{
	  start = (*i)->getAddress();
	  (*i)->addSegment(this);
	  return;
	}
      i++;
    }
}

void FGTaxiSegment::setEnd(FGTaxiNodeVector *nodes)
{
  FGTaxiNodeVectorIterator i = nodes->begin();
  while (i != nodes->end())
    {
      if ((*i)->getIndex() == endNode)
	{
	  end = (*i)->getAddress();
	  return;
	}
      i++;
    }
}



// There is probably a computationally cheaper way of 
// doing this.
void FGTaxiSegment::setTrackDistance()
{
  //double course;
  SGWayPoint first  (start->getLongitude(),
		     start->getLatitude(),
		     0);
  SGWayPoint second (end->getLongitude(),
		     end->getLatitude(),
		     0);
  first.CourseAndDistance(second, &course, &length);
}


void FGTaxiSegment::setCourseDiff(double crse)
{
  headingDiff = fabs(course-crse);
  
  if (headingDiff > 180)
    headingDiff = fabs(headingDiff - 360);
}

bool compare_segments(FGTaxiSegment *a, FGTaxiSegment *b) {
return (*a) < (*b);
}

bool sortByHeadingDiff(FGTaxiSegment *a, FGTaxiSegment *b) {
  return a->hasSmallerHeadingDiff(*b);
}

bool sortByLength(FGTaxiSegment *a, FGTaxiSegment *b) {
  return a->getLength() > b->getLength();
}
/***************************************************************************
 * FGTaxiRoute
 **************************************************************************/
bool FGTaxiRoute::next(int *nde) 
{ 
  //for (intVecIterator i = nodes.begin(); i != nodes.end(); i++)
  //  cerr << "FGTaxiRoute contains : " << *(i) << endl;
  //cerr << "Offset from end: " << nodes.end() - currNode << endl;
  //if (currNode != nodes.end())
  //  cerr << "true" << endl;
  //else
  //  cerr << "false" << endl;
  //if (nodes.size() != (routes.size()) +1)
  //  cerr << "ALERT: Misconfigured TaxiRoute : " << nodes.size() << " " << routes.size() << endl;
      
  if (currNode == nodes.end())
    return false;
  *nde = *(currNode); 
  if (currNode != nodes.begin()) // make sure route corresponds to the end node
    currRoute++;
  currNode++;
  return true;
};

bool FGTaxiRoute::next(int *nde, int *rte) 
{ 
  //for (intVecIterator i = nodes.begin(); i != nodes.end(); i++)
  //  cerr << "FGTaxiRoute contains : " << *(i) << endl;
  //cerr << "Offset from end: " << nodes.end() - currNode << endl;
  //if (currNode != nodes.end())
  //  cerr << "true" << endl;
  //else
  //  cerr << "false" << endl;
  if (nodes.size() != (routes.size()) +1) {
    SG_LOG(SG_GENERAL, SG_ALERT, "ALERT: Misconfigured TaxiRoute : " << nodes.size() << " " << routes.size());
    exit(1);
  }
  if (currNode == nodes.end())
    return false;
  *nde = *(currNode); 
  //*rte = *(currRoute);
  if (currNode != nodes.begin()) // Make sure route corresponds to the end node
    {
      *rte = *(currRoute);
      currRoute++;
    }
  else
    {
      // If currNode points to the first node, this means the aircraft is not on the taxi node
      // yet. Make sure to return a unique identifyer in this situation though, because otherwise
      // the speed adjust AI code may be unable to resolve whether two aircraft are on the same 
      // taxi route or not. the negative of the preceding route seems a logical choice, as it is 
      // unique for any starting location. 
      // Note that this is probably just a temporary fix until I get Parking / tower control working.
      *rte = -1 * *(currRoute); 
    }
  currNode++;
  return true;
};

void FGTaxiRoute::rewind(int route)
{
  int currPoint;
  int currRoute;
  first();
  do {
    if (!(next(&currPoint, &currRoute))) { 
      SG_LOG(SG_GENERAL,SG_ALERT, "Error in rewinding TaxiRoute: current" << currRoute 
	     << " goal " << route);
    }
  } while (currRoute != route);
}




/***************************************************************************
 * FGGroundNetwork()
 **************************************************************************/

FGGroundNetwork::FGGroundNetwork()
{
  hasNetwork = false;
  foundRoute = false;
  totalDistance = 0;
  maxDistance = 0;
  maxDepth    = 1000;
  count       = 0;
  currTraffic = activeTraffic.begin();

}

FGGroundNetwork::~FGGroundNetwork()
{
  for (FGTaxiNodeVectorIterator node = nodes.begin();
       node != nodes.end();
       node++)
    {
      delete (*node);
    }
  nodes.clear();
  for (FGTaxiSegmentVectorIterator seg = segments.begin();
       seg != segments.end();
       seg++)
    {
      delete (*seg);
    }
  segments.clear();
}

void FGGroundNetwork::addSegment(const FGTaxiSegment &seg)
{
  segments.push_back(new FGTaxiSegment(seg));
}

void FGGroundNetwork::addNode(const FGTaxiNode &node)
{
  nodes.push_back(new FGTaxiNode(node));
}

void FGGroundNetwork::addNodes(FGParkingVec *parkings)
{
  FGTaxiNode n;
  FGParkingVecIterator i = parkings->begin();
  while (i != parkings->end())
    {
      n.setIndex(i->getIndex());
      n.setLatitude(i->getLatitude());
      n.setLongitude(i->getLongitude());
      nodes.push_back(new FGTaxiNode(n));

      i++;
    }
}



void FGGroundNetwork::init()
{
  hasNetwork = true;
  int index = 1;
  sort(nodes.begin(), nodes.end(), compare_nodes);
  //sort(segments.begin(), segments.end(), compare_segments());
  FGTaxiSegmentVectorIterator i = segments.begin();
  while(i != segments.end()) {
    //cerr << "initializing node " << i->getIndex() << endl;
    (*i)->setStart(&nodes);
    (*i)->setEnd  (&nodes);
    (*i)->setTrackDistance();
    (*i)->setIndex(index);
    //cerr << "Track distance = " << i->getLength() << endl;
    //cerr << "Track ends at"      << i->getEnd()->getIndex() << endl;
    i++;
    index++;
  }
 
  i = segments.begin();
  while(i != segments.end()) {
    FGTaxiSegmentVectorIterator j = (*i)->getEnd()->getBeginRoute(); 
    while (j != (*i)->getEnd()->getEndRoute())
      {
	if ((*j)->getEnd()->getIndex() == (*i)->getStart()->getIndex())
	  {
	    int start1 = (*i)->getStart()->getIndex();
	    int end1   = (*i)->getEnd()  ->getIndex();
	    int start2 = (*j)->getStart()->getIndex();
	    int end2   = (*j)->getEnd()->getIndex();
	    int oppIndex = (*j)->getIndex();
	    //cerr << "Opposite of  " << (*i)->getIndex() << " (" << start1 << "," << end1 << ") "
	    //	 << "happens to be " << oppIndex      << " (" << start2 << "," << end2 << ") " << endl;
	    (*i)->setOpposite(*j);
	    break;
	  }
	  j++;
      }
    i++;
  }
  //cerr << "Done initializing ground network" << endl;
  //exit(1);
}



int FGGroundNetwork::findNearestNode(double lat, double lon)
{
  double minDist = HUGE_VAL;
  double dist;
  int index;
  SGWayPoint first  (lon,
		     lat,
		     0);
  
  for (FGTaxiNodeVectorIterator 
	 itr = nodes.begin();
       itr != nodes.end(); itr++)
    {
      double course;
      SGWayPoint second ((*itr)->getLongitude(),
			 (*itr)->getLatitude(),
			 0);
      first.CourseAndDistance(second, &course, &dist);
      if (dist < minDist)
	{
	  minDist = dist;
	  index = (*itr)->getIndex();
	  //cerr << "Minimum distance of " << minDist << " for index " << index << endl;
	}
    }
  return index;
}

FGTaxiNode *FGGroundNetwork::findNode(int idx)
{ /*
    for (FGTaxiNodeVectorIterator 
    itr = nodes.begin();
    itr != nodes.end(); itr++)
    {
    if (itr->getIndex() == idx)
    return itr->getAddress();
    }*/
  
  if ((idx >= 0) && (idx < nodes.size())) 
    return nodes[idx]->getAddress();
  else
    return 0;
}

FGTaxiSegment *FGGroundNetwork::findSegment(int idx)
{/*
  for (FGTaxiSegmentVectorIterator 
	 itr = segments.begin();
       itr != segments.end(); itr++)
    {
      if (itr->getIndex() == idx)
	return itr->getAddress();
    } 
 */
  if ((idx > 0) && (idx <= segments.size()))
    return segments[idx-1]->getAddress();
  else
    {
      //cerr << "Alert: trying to find invalid segment " << idx << endl;
      return 0;
    }
}

// FGTaxiRoute FGGroundNetwork::findShortestRoute(int start, int end) 
// {
//   double course;
//   double length;
//   foundRoute = false;
//   totalDistance = 0;
//   FGTaxiNode *firstNode = findNode(start);
//   FGTaxiNode *lastNode  = findNode(end);
//   //prevNode = prevPrevNode = -1;
//   //prevNode = start;
//   routes.clear();
//   nodesStack.clear();
//   routesStack.clear();
//   // calculate distance and heading "as the crow flies" between starn and end points"
//   SGWayPoint first(firstNode->getLongitude(),
// 		   firstNode->getLatitude(),
// 		   0);
//   destination = SGWayPoint(lastNode->getLongitude(),
// 	       lastNode->getLatitude(),
// 	       0);
//   
//   first.CourseAndDistance(destination, &course, &length);
//   for (FGTaxiSegmentVectorIterator 
// 	 itr = segments.begin();
//        itr != segments.end(); itr++)
//     {
//       (*itr)->setCourseDiff(course);
//     } 
//   //FGTaxiNodeVectorIterator nde = nodes.begin();
//   //while (nde != nodes.end()) {
//   //  (*nde)->sortEndSegments();
//   //  nde++;
//   //}	
//   maxDepth = 1000;
//   //do
//   //  {
//   //    cerr << "Begin of Trace " << start << " to "<< end << " maximum depth = " << maxDepth << endl;
//       trace(firstNode, end, 0, 0);
//       //    maxDepth--;
//       //    }
//       //while ((routes.size() != 0) && (maxDepth > 0));
//       //cerr << "End of Trace" << endl;
//   FGTaxiRoute empty;
//  
//   if (!foundRoute)
//     {
//       SG_LOG( SG_GENERAL, SG_ALERT, "Failed to find route from waypoint " << start << " to " << end << " at " << 
// 	      parent->getId());
//       exit(1);
//     }
//   sort(routes.begin(), routes.end());
//   //for (intVecIterator i = route.begin(); i != route.end(); i++)
//   //  {
//   //    rte->push_back(*i);
//   //  }
//   
//   if (routes.begin() != routes.end())
//     {
//      //  if ((routes.begin()->getDepth() < 0.5 * maxDepth) && (maxDepth > 1))
// // 	{
// // 	  maxDepth--;
// // 	  cerr << "Max search depth decreased to : " << maxDepth;
// // 	}
// //       else
// // 	{ 
// // 	  maxDepth++; 
// // 	  cerr << "Max search depth increased to : " << maxDepth;
// // 	}
//       return *(routes.begin());
//     }
//   else
//     return empty;
// }

FGTaxiRoute FGGroundNetwork::findShortestRoute(int start, int end) 
{
//implements Dijkstra's algorithm to find shortest distance route from start to end
//taken from http://en.wikipedia.org/wiki/Dijkstra's_algorithm

    //double INFINITE = 100000000000.0;
    // initialize scoring values
    for (FGTaxiNodeVectorIterator
         itr = nodes.begin();
         itr != nodes.end(); itr++) {
            (*itr)->pathscore = HUGE_VAL; //infinity by all practical means
            (*itr)->previousnode = 0; //
            (*itr)->previousseg = 0; //
         }

    FGTaxiNode *firstNode = findNode(start);
    firstNode->pathscore = 0;

    FGTaxiNode *lastNode  = findNode(end);

    FGTaxiNodeVector unvisited(nodes); // working copy

    while (!unvisited.empty()) {
        FGTaxiNode* best = *(unvisited.begin());
        for (FGTaxiNodeVectorIterator
             itr = unvisited.begin();
             itr != unvisited.end(); itr++) {
                 if ((*itr)->pathscore < best->pathscore)
                     best = (*itr);
        }

        FGTaxiNodeVectorIterator newend = remove(unvisited.begin(), unvisited.end(), best);
        unvisited.erase(newend, unvisited.end());
        
        if (best == lastNode) { // found route or best not connected
            break;
        } else {
            for (FGTaxiSegmentVectorIterator
                 seg = best->getBeginRoute();
                 seg != best->getEndRoute(); seg++) {
                FGTaxiNode* tgt = (*seg)->getEnd();
                double alt = best->pathscore + (*seg)->getLength();
                if (alt < tgt->pathscore) {              // Relax (u,v)
                    tgt->pathscore = alt;
                    tgt->previousnode = best;
                    tgt->previousseg = *seg; //
                }
            }
        }
    }

    if (lastNode->pathscore == HUGE_VAL) {
        // no valid route found
        SG_LOG( SG_GENERAL, SG_ALERT, "Failed to find route from waypoint " << start << " to " << end << " at " <<
                parent->getId());
        exit(1); //TODO exit more gracefully, no need to stall the whole sim with broken GN's
    } else {
        // assemble route from backtrace information
        intVec nodes, routes;
        FGTaxiNode* bt = lastNode;
        while (bt->previousnode != 0) {
            nodes.push_back(bt->getIndex());
            routes.push_back(bt->previousseg->getIndex());
            bt = bt->previousnode;
        }
        nodes.push_back(start);
        reverse(nodes.begin(), nodes.end());
        reverse(routes.begin(), routes.end());

        return FGTaxiRoute(nodes, routes, lastNode->pathscore, 0);
    }
}



void FGGroundNetwork::trace(FGTaxiNode *currNode, int end, int depth, double distance)
{
  // Just check some preconditions of the trace algorithm
  if (nodesStack.size() != routesStack.size()) 
    {
      SG_LOG(SG_GENERAL, SG_ALERT, "size of nodesStack and routesStack is not equal. NodesStack :" 
	     << nodesStack.size() << ". RoutesStack : " << routesStack.size());
    }
  nodesStack.push_back(currNode->getIndex());
  totalDistance += distance;
  //cerr << "Starting trace " << currNode->getIndex() << " " << "total distance: " << totalDistance << endl;
  //     << currNode->getIndex() << endl;

  // If the current route matches the required end point we found a valid route
  // So we can add this to the routing table
  if (currNode->getIndex() == end)
    {
      maxDepth = depth;
      //cerr << "Found route : " <<  totalDistance << "" << " " << *(nodesStack.end()-1) << " Depth = " << depth << endl;
      routes.push_back(FGTaxiRoute(nodesStack,routesStack,totalDistance, depth));
      if (nodesStack.empty() || routesStack.empty())
	{
	  printRoutingError(string("while finishing route"));
	}
      nodesStack.pop_back();
      routesStack.pop_back();
      if (!(foundRoute)) {
	maxDistance = totalDistance;
      }
      else
	if (totalDistance < maxDistance)
	  maxDistance = totalDistance;
      foundRoute = true;
      totalDistance -= distance;
      return;
    }
 

  // search if the currentNode has been encountered before
  // if so, we should step back one level, because it is
  // rather rediculous to proceed further from here. 
  // if the current node has not been encountered before,
  // i should point to nodesStack.end()-1; and we can continue
  // if i is not nodesStack.end, the previous node was found, 
  // and we should return. 
  // This only works at trace levels of 1 or higher though
  if (depth > 0) {
    intVecIterator i = nodesStack.begin();
    while ((*i) != currNode->getIndex()) {
      //cerr << "Route so far : " << (*i) << endl;
      i++;
    }
    if (i != nodesStack.end()-1) {
      if (nodesStack.empty() || routesStack.empty())
	{
	  printRoutingError(string("while returning from an already encountered node"));
	}
      nodesStack.pop_back();
      routesStack.pop_back();
      totalDistance -= distance;
      return;
    }
    if (depth >= maxDepth) {
      count++;
      if (!(count % 100000)) {
	maxDepth--; // Gradually decrease maxdepth, to prevent "eternal searches"
	//cerr << "Reducing maxdepth to " << maxDepth << endl;
      }
      nodesStack.pop_back();
      routesStack.pop_back();
      totalDistance -= distance;
      return;
    }
    // If the total distance from start to the current waypoint
    // is longer than that of a route we can also stop this trace 
    // and go back one level. 
    if ((totalDistance > maxDistance) && foundRoute)
      //if (foundRoute)
      {
	//cerr << "Stopping rediculously long trace: " << totalDistance << endl;
	if (nodesStack.empty() || routesStack.empty())
	{
	  printRoutingError(string("while returning from finding a rediculously long route"));
	}
	nodesStack.pop_back();
	routesStack.pop_back();
	totalDistance -= distance;
	return;
      }
  }
  
  //cerr << "2" << endl;
  if (currNode->getBeginRoute() != currNode->getEndRoute())
    {
      double course, length;
      //cerr << "3" << endl;
      // calculate distance and heading "as the crow flies" between starn and end points"
      SGWayPoint first(currNode->getLongitude(),
		       currNode->getLatitude(),
		       0);
      //SGWayPoint second (lastNode->getLongitude(),
      //		 lastNode->getLatitude(),
      //		     0);
  
      first.CourseAndDistance(destination, &course, &length);
      //for (FGTaxiSegmentVectorIterator 
      //	     itr = segments.begin();
      //	   itr != segments.end(); itr++)
      //	{
      //	  (*itr)->setCourseDiff(course);
      //	} 
      //FGTaxiNodeVectorIterator nde = nodes.begin();
      //while (nde != nodes.end()) {
      //(*nde)->sortEndSegments();
      //nde++;
      
      for (FGTaxiSegmentVectorIterator 
	     i = currNode->getBeginRoute();
	   i != currNode->getEndRoute();
	   i++)
	{
	  (*i)->setCourseDiff(course);
	}
      currNode->sortEndSegments(foundRoute);
      for (FGTaxiSegmentVectorIterator 
	     i = currNode->getBeginRoute();
	   i != currNode->getEndRoute();
	   i++)
	{
	  //cerr << (*i)->getLength() << endl;
	  //cerr << (*i)->getIndex() << endl;
	  int idx = (*i)->getIndex();
	  routesStack.push_back((*i)->getIndex());
	  trace((*i)->getEnd(), end, depth+1, (*i)->getLength());
	//  {
	//      // cerr << currNode -> getIndex() << " ";
	//      route.push_back(currNode->getIndex());
	//      return true;
	//    }
	}
    }
  else
    {
      //SG_LOG( SG_GENERAL, SG_DEBUG, "4" );
    }
  if (nodesStack.empty())
    {
      printRoutingError(string("while finishing trace"));
    }
  nodesStack.pop_back();
  // Make sure not to dump the level-zero routesStack entry, because that was never created.
  if (depth)
    {
      routesStack.pop_back();
      //cerr << "leaving trace " << routesStack.size() << endl;
    }
  totalDistance -= distance;
  return;
}

void FGGroundNetwork::printRoutingError(string mess)
{
  SG_LOG(SG_GENERAL, SG_ALERT,  "Error in ground network trace algorithm " << mess);
  if (nodesStack.empty())
    {
      SG_LOG(SG_GENERAL, SG_ALERT, " nodesStack is empty. Dumping routesStack");
      for (intVecIterator i = routesStack.begin() ; i != routesStack.end(); i++)
	SG_LOG(SG_GENERAL, SG_ALERT, "Route " << (*i));
    }
  if (routesStack.empty())
    {
      SG_LOG(SG_GENERAL, SG_ALERT, " routesStack is empty. Dumping nodesStack"); 
      for (intVecIterator i = nodesStack.begin() ; i != nodesStack.end(); i++)
	SG_LOG(SG_GENERAL, SG_ALERT, "Node " << (*i));
    }
  //exit(1);
}


void FGGroundNetwork::announcePosition(int id, FGAIFlightPlan *intendedRoute, int currentPosition,
				       double lat, double lon, double heading, 
				       double speed, double alt, double radius, int leg,
				       string callsign)
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
   // Add a new TrafficRecord if no one exsists for this aircraft.
   if (i == activeTraffic.end() || (activeTraffic.size() == 0)) {
     FGTrafficRecord rec;
     rec.setId(id);
     rec.setPositionAndIntentions(currentPosition, intendedRoute);
     rec.setPositionAndHeading(lat, lon, heading, speed, alt);
     rec.setRadius(radius); // only need to do this when creating the record.
     rec.setCallSign(callsign);
     activeTraffic.push_back(rec);
   } else {
     i->setPositionAndIntentions(currentPosition, intendedRoute); 
     i->setPositionAndHeading(lat, lon, heading, speed, alt);
   }
}

void FGGroundNetwork::signOff(int id) {
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
     SG_LOG(SG_GENERAL, SG_ALERT, "AI error: Aircraft without traffic record is signing off");
   } else {
       i = activeTraffic.erase(i);
   }
}

void FGGroundNetwork::update(int id, double lat, double lon, double heading, double speed, double alt, 
			     double dt) {
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
   // update position of the current aircraft
   if (i == activeTraffic.end() || (activeTraffic.size() == 0)) {
     SG_LOG(SG_GENERAL, SG_ALERT, "AI error: updating aircraft without traffic record");
   } else {
     i->setPositionAndHeading(lat, lon, heading, speed, alt);
     current = i;
   }
  
   setDt(getDt() + dt);
  
   // Update every three secs, but add some randomness
   // to prevent all IA objects doing this in synchrony
   //if (getDt() < (3.0) + (rand() % 10))
   //  return;
   //else
   //  setDt(0);
   checkSpeedAdjustment(id, lat, lon, heading, speed, alt);
   checkHoldPosition   (id, lat, lon, heading, speed, alt);
}

void FGGroundNetwork::checkSpeedAdjustment(int id, double lat, 
					   double lon, double heading, 
					   double speed, double alt)
{
  
  // Scan for a speed adjustment change. Find the nearest aircraft that is in front
  // and adjust speed when we get too close. Only do this when current position and/or
  // intentions of the current aircraft match current taxiroute position of the proximate
  // aircraft. For traffic that is on other routes we need to issue a "HOLD Position"
  // instruction. See below for the hold position instruction.
  TrafficVectorIterator current, closest;
  TrafficVectorIterator i = activeTraffic.begin();
  bool otherReasonToSlowDown = false;
  bool previousInstruction;
  if (activeTraffic.size()) 
    {
      //while ((i->getId() != id) && (i != activeTraffic.end()))
      while (i != activeTraffic.end()) {
	if (i->getId() == id) {
	  break;
	}
	i++;
      }
    }
  else
    {
      return;
    }
  if (i == activeTraffic.end() || (activeTraffic.size() == 0)) {
    SG_LOG(SG_GENERAL, SG_ALERT, "AI error: Trying to access non-existing aircraft in FGGroundNetwork::checkSpeedAdjustment");
  }
  current = i;
  previousInstruction = current->getSpeedAdjustment();
  double mindist = HUGE;
  if (activeTraffic.size()) 
    {
      double course, dist, bearing, minbearing;
      SGWayPoint curr  (lon,
			lat,
			alt);
      //TrafficVector iterator closest;
      closest = current;
      for (TrafficVectorIterator i = activeTraffic.begin(); 
   	   i != activeTraffic.end(); i++)
   	{
   	  if (i->getId() != current->getId()) {
   	    //SGWayPoint curr  (lon,
  	    //	      lat,
  	    //	      alt);
   	    SGWayPoint other    (i->getLongitude  (),
   				 i->getLatitude (),
   				 i->getAltitude  ());
   	    other.CourseAndDistance(curr, &course, &dist);
   	    bearing = fabs(heading-course);
   	    if (bearing > 180)
   	      bearing = 360-bearing;
   	    if ((dist < mindist) && (bearing < 60.0))
   	      {
   		mindist = dist;
   		closest = i;
   		minbearing = bearing;
   	      }
   	  }
   	}
      //Check traffic at the tower controller
      if (towerController->hasActiveTraffic())
	{
	  for (TrafficVectorIterator i = towerController->getActiveTraffic().begin(); 
	       i != towerController->getActiveTraffic().end(); i++)
	    {
	      if (i->getId() != current->getId()) {
		//SGWayPoint curr  (lon,
		//		  lat,
		//		  alt);
		SGWayPoint other    (i->getLongitude  (),
				     i->getLatitude (),
				     i->getAltitude  ());
		other.CourseAndDistance(curr, &course, &dist);
		bearing = fabs(heading-course);
		if (bearing > 180)
		  bearing = 360-bearing;
		if ((dist < mindist) && (bearing < 60.0))
		  {
		    mindist = dist;
		    closest = i;
		    minbearing = bearing;
		    otherReasonToSlowDown = true;
		  }
	      }
	    }
	}
      // Finally, check UserPosition
      double userLatitude  = fgGetDouble("/position/latitude-deg");
      double userLongitude = fgGetDouble("/position/longitude-deg");
      SGWayPoint user    (userLongitude,
			  userLatitude,
			  alt); // Alt is not really important here. 
      user.CourseAndDistance(curr, &course, &dist);
      bearing = fabs(heading-course);
      if (bearing > 180)
	bearing = 360-bearing;
      if ((dist < mindist) && (bearing < 60.0))
	{
	  mindist = dist;
	  //closest = i;
	  minbearing = bearing;
	  otherReasonToSlowDown = true;
	}
      
      // 	if (closest == current) {
      // 	  //SG_LOG(SG_GENERAL, SG_ALERT, "AI error: closest and current match");
      // 	  //return;
      // 	}      
      //cerr << "Distance : " << dist << " bearing : " << bearing << " heading : " << heading 
      //   << " course : " << course << endl;
      current->clearSpeedAdjustment();
    
      if (current->checkPositionAndIntentions(*closest) || otherReasonToSlowDown) 
	{
	   double maxAllowableDistance = (1.1*current->getRadius()) + (1.1*closest->getRadius());
	   if (mindist < 2*maxAllowableDistance)
	     {
	       if (current->getId() == closest->getWaitsForId())
		 return;
	       else 
		 current->setWaitsForId(closest->getId());
	       if (closest != current)
		 current->setSpeedAdjustment(closest->getSpeed()* (mindist/100));
	       else
		 current->setSpeedAdjustment(0); // This can only happen when the user aircraft is the one closest
	       if (mindist < maxAllowableDistance)
		 {
		   //double newSpeed = (maxAllowableDistance-mindist);
		   //current->setSpeedAdjustment(newSpeed);
		   //if (mindist < 0.5* maxAllowableDistance)
		   //  {
		       current->setSpeedAdjustment(0);
		       //  }
		 }
	     }
	}
    }
}

void FGGroundNetwork::checkHoldPosition(int id, double lat, 
					double lon, double heading, 
					double speed, double alt)
{
  // Check for "Hold position instruction".
  // The hold position should be issued under the following conditions:
  // 1) For aircraft entering or crossing a runway with active traffic on it, or landing aircraft near it
  // 2) For taxiing aircraft that use one taxiway in opposite directions
  // 3) For crossing or merging taxiroutes.
  
  TrafficVectorIterator current;
  TrafficVectorIterator i = activeTraffic.begin();
  if (activeTraffic.size()) 
    {
      //while ((i->getId() != id) && i != activeTraffic.end()) 
      while (i != activeTraffic.end()) {
	if (i->getId() == id) {
	  break;
	}
	i++;
      }
    }
  else
    {
      return ;
    } 
  if (i == activeTraffic.end() || (activeTraffic.size() == 0)) {
    SG_LOG(SG_GENERAL, SG_ALERT, "AI error: Trying to access non-existing aircraft in FGGroundNetwork::checkHoldPosition");
  }
  current = i;
  current->setHoldPosition(false);
  SGWayPoint curr  (lon,
		    lat,
		    alt);
  double course, dist, bearing, minbearing;
  for (i = activeTraffic.begin(); 
       i != activeTraffic.end(); i++)
    {
      if (i != current) 
  	{
  	  int node = current->crosses(this, *i);
  	  if (node != -1)
  	    {
  	      // Determine whether it's save to continue or not. 
  	      // If we have a crossing route, there are two possibilities:
  	      // 1) This is an interestion
  	      // 2) This is oncoming two-way traffic, using the same taxiway.
  	      //cerr << "Hold check 1 : " << id << " has common node " << node << endl;
  	      SGWayPoint nodePos(findNode(node)->getLongitude  (),
  				 findNode(node)->getLatitude   (),
  				 alt);
	      
	      
  	      SGWayPoint other    (i->getLongitude  (),
  				   i->getLatitude (),
  				   i->getAltitude  ());
  	      //other.CourseAndDistance(curr, &course, &dist);
  	      bool needsToWait;
	      bool opposing;
  	      if (current->isOpposing(this, *i, node))
  		{
  		  needsToWait = true;
		  opposing    = true;
  		  //cerr << "Hold check 2 : " << node << "  has opposing segment " << endl;
  		  // issue a "Hold Position" as soon as we're close to the offending node
  		  // For now, I'm doing this as long as the other aircraft doesn't
 		  // have a hold instruction as soon as we're within a reasonable 
  		  // distance from the offending node.
  		  // This may be a bit of a conservative estimate though, as it may
  		  // be well possible that both aircraft can both continue to taxi 
  		  // without crashing into each other.
  		} 
  	      else 
  		{
		  opposing = false;
  		  other.CourseAndDistance(nodePos, &course, &dist);
  		  if (dist > 200) // 2.0*i->getRadius())
  		    {
  		      needsToWait = false; 
  		      //cerr << "Hold check 3 : " << id <<"  Other aircraft approaching node is still far away. (" << dist << " nm). Can safely continue " 
  		      //	   << endl;
  		    }
  		  else 
  		    {
  		      needsToWait = true;
  		      //cerr << "Hold check 4: " << id << "  Would need to wait for other aircraft : distance = " << dist << " meters" << endl;
  		    }
  		}
  	      curr.CourseAndDistance(nodePos, &course, &dist);
  	      if (!(i->hasHoldPosition()))
  		{
		  
  		  if ((dist < 200) && //2.5*current->getRadius()) && 
  		      (needsToWait) &&
		      (i->onRoute(this, *current)) &&
		      //((i->onRoute(this, *current)) || ((!(i->getSpeedAdjustment())))) &&
  		      (!(current->getId() == i->getWaitsForId())))
		      //(!(i->getSpeedAdjustment()))) // &&
  		      //(!(current->getSpeedAdjustment())))
		    
  		    {
  		      current->setHoldPosition(true);
  		      //cerr << "Hold check 5: " << current->getCallSign() <<"  Setting Hold Position: distance to node ("  << node << ") "
		      //	   << dist << " meters. Waiting for " << i->getCallSign();
		      //if (opposing)
		      //cerr <<" [opposing] " << endl;
		      //else
		      //	cerr << "[non-opposing] " << endl;
		      //if (i->hasSpeefAdjustment())
		      //	{
		      //	  cerr << " (which in turn waits for ) " << i->
  		    }
  		  else
  		    {
  		      //cerr << "Hold check 6: " << id << "  No need to hold yet: Distance to node : " << dist << " nm"<< endl;
  		    }
  		}
  	    }
  	}
    }
}

// Note that this function is probably obsolete...
bool FGGroundNetwork::hasInstruction(int id)
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

FGATCInstruction FGGroundNetwork::getInstruction(int id)
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

