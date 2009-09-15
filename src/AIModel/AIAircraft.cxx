
// FGAIAircraft - FGAIBase-derived class creates an AI airplane
//
// Written by David Culp, started October 2003.
//
// Copyright (C) 2003  David P. Culp - davidculp2@comcast.net
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

#include <simgear/math/point3d.hxx>
#include <simgear/route/waypoint.hxx>
#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Main/viewer.hxx>
#include <Scenery/scenery.hxx>
#include <Scenery/tilemgr.hxx>

#include <string>
#include <math.h>
#include <time.h>
#ifdef _MSC_VER
#  include <float.h>
#  define finite _finite
#elif defined(__sun) || defined(sgi)
#  include <ieeefp.h>
#endif

SG_USING_STD(string);

#include "AIAircraft.hxx"
//#include <Airports/trafficcontroller.hxx>

static string tempReg;
//
// accel, decel, climb_rate, descent_rate, takeoff_speed, climb_speed,
// cruise_speed, descent_speed, land_speed
//

const FGAIAircraft::PERF_STRUCT FGAIAircraft::settings[] = {
            // light aircraft
            {2.0, 2.0,  450.0, 1000.0,  70.0,  80.0, 100.0,  80.0,  60.0},
            // ww2_fighter
            {4.0, 2.0, 3000.0, 1500.0, 110.0, 180.0, 250.0, 200.0, 100.0},
            // jet_transport
            {5.0, 2.0, 3000.0, 1500.0, 140.0, 300.0, 430.0, 300.0, 130.0},
            // jet_fighter
            {7.0, 3.0, 4000.0, 2000.0, 150.0, 350.0, 500.0, 350.0, 150.0},
            // tanker
            {5.0, 2.0, 3000.0, 1500.0, 140.0, 300.0, 430.0, 300.0, 130.0},
            // ufo (extreme accel/decel)
            {30.0, 30.0, 6000.0, 6000.0, 150.0, 300.0, 430.0, 300.0, 130.0}
        };

FGAIAircraft::FGAIAircraft(FGAISchedule *ref) : FGAIBase(otAircraft) {
    trafficRef = ref;
    if (trafficRef) {
        groundOffset = trafficRef->getGroundOffset();
        setCallSign(trafficRef->getCallSign());
    }
    else
        groundOffset = 0;

    fp = 0;
    controller = 0;
    prevController = 0;
    dt_count = 0;
    dt_elev_count = 0;
    use_perf_vs = true;

    no_roll = false;
    tgt_speed = 0;
    speed = 0;
    groundTargetSpeed = 0;

    // set heading and altitude locks
    hdg_lock = false;
    alt_lock = false;
    roll = 0;
    headingChangeRate = 0.0;

    holdPos = false;
}


FGAIAircraft::~FGAIAircraft() {
    //delete fp;
    if (controller)
        controller->signOff(getID());
}


void FGAIAircraft::readFromScenario(SGPropertyNode* scFileNode) {
    if (!scFileNode)
        return;

    FGAIBase::readFromScenario(scFileNode);

    setPerformance(scFileNode->getStringValue("class", "jet_transport"));
    setFlightPlan(scFileNode->getStringValue("flightplan"),
                  scFileNode->getBoolValue("repeat", false));
    setCallSign(scFileNode->getStringValue("callsign"));
}


void FGAIAircraft::bind() {
    FGAIBase::bind();

    props->tie("controls/gear/gear-down",
               SGRawValueMethods<FGAIAircraft,bool>(*this,
                                                    &FGAIAircraft::_getGearDown));
}


void FGAIAircraft::unbind() {
    FGAIBase::unbind();

    props->untie("controls/gear/gear-down");
}


void FGAIAircraft::update(double dt) {
    FGAIBase::update(dt);
    Run(dt);
    Transform();
}


void FGAIAircraft::setPerformance(const std::string& acclass) {
    if (acclass == "light") {
        SetPerformance(&FGAIAircraft::settings[FGAIAircraft::LIGHT]);
    } else if (acclass == "ww2_fighter") {
        SetPerformance(&FGAIAircraft::settings[FGAIAircraft::WW2_FIGHTER]);
    } else if (acclass == "jet_transport") {
        SetPerformance(&FGAIAircraft::settings[FGAIAircraft::JET_TRANSPORT]);
    } else if (acclass == "jet_fighter") {
        SetPerformance(&FGAIAircraft::settings[FGAIAircraft::JET_FIGHTER]);
    } else if (acclass == "tanker") {
        SetPerformance(&FGAIAircraft::settings[FGAIAircraft::JET_TRANSPORT]);
    } else if (acclass == "ufo") {
        SetPerformance(&FGAIAircraft::settings[FGAIAircraft::UFO]);
    } else {
        SetPerformance(&FGAIAircraft::settings[FGAIAircraft::JET_TRANSPORT]);
    }
}


void FGAIAircraft::SetPerformance(const PERF_STRUCT *ps) {

    performance = ps;
}


void FGAIAircraft::Run(double dt) {

    FGAIAircraft::dt = dt;
    if (!updateTargetValues())
        return;

    if (controller) {
        controller->update(getID(),
                           pos.getLatitudeDeg(),
                           pos.getLongitudeDeg(),
                           hdg,
                           speed,
                           altitude_ft, dt);
        processATC(controller->getInstruction(getID()));
    }

    if (no_roll) {
        adjustSpeed(groundTargetSpeed);
    } else {
        adjustSpeed(tgt_speed);
    }

    updatePosition();
    updateHeading();
    updateBankAngles();
    updateAltitudes();
    updateVerticalSpeed();
    matchPitchAngle();
    UpdateRadar(manager);
}


void FGAIAircraft::AccelTo(double speed) {
    tgt_speed = speed;
}


void FGAIAircraft::PitchTo(double angle) {
    tgt_pitch = angle;
    alt_lock = false;
}


void FGAIAircraft::RollTo(double angle) {
    tgt_roll = angle;
    hdg_lock = false;
}


void FGAIAircraft::YawTo(double angle) {
    tgt_yaw = angle;
}


void FGAIAircraft::ClimbTo(double alt_ft ) {
    tgt_altitude_ft = alt_ft;
    alt_lock = true;
}


void FGAIAircraft::TurnTo(double heading) {
    tgt_heading = heading;
    hdg_lock = true;
}


double FGAIAircraft::sign(double x) {
    if (x == 0.0)
        return x;
    else
        return x/fabs(x);
}


void FGAIAircraft::setFlightPlan(const std::string& flightplan, bool repeat) {
    if (!flightplan.empty()) {
        FGAIFlightPlan* fp = new FGAIFlightPlan(flightplan);
        fp->setRepeat(repeat);
        SetFlightPlan(fp);
    }
}


void FGAIAircraft::SetFlightPlan(FGAIFlightPlan *f) {
    delete fp;
    fp = f;
}


void FGAIAircraft::ProcessFlightPlan( double dt, time_t now ) {

    //if (! fpExecutable(now))
    //      return;

    // the one behind you
    FGAIFlightPlan::waypoint* prev = 0;
    // the one ahead
    FGAIFlightPlan::waypoint* curr = 0;
    // the next plus 1
    FGAIFlightPlan::waypoint* next = 0;

    prev = fp->getPreviousWaypoint();
    curr = fp->getCurrentWaypoint();
    next = fp->getNextWaypoint();

    dt_count += dt;

    ///////////////////////////////////////////////////////////////////////////
    // Initialize the flightplan
    //////////////////////////////////////////////////////////////////////////
    if (!prev) {
        handleFirstWaypoint();
        return;
    }                            // end of initialization

    dt_count = 0;

    if (! leadPointReached(curr)) {
        controlHeading(curr);
        controlSpeed(curr, next);
    } else {
        if (curr->finished)      //end of the flight plan
        {
            if (fp->getRepeat())
                fp->restart();
            else
                setDie(true);
            return;
        }

        if (next) {
            //TODO more intelligent method in AIFlightPlan, no need to send data it already has :-)
            tgt_heading = fp->getBearing(curr, next);
            spinCounter = 0;
        }

        //TODO let the fp handle this (loading of next leg)
        fp->IncrementWaypoint((bool) trafficRef);
        if (!(fp->getNextWaypoint()) && trafficRef)
            loadNextLeg();

        prev = fp->getPreviousWaypoint();
        curr = fp->getCurrentWaypoint();
        next = fp->getNextWaypoint();

        // Now that we have incremented the waypoints, excute some traffic manager specific code
        if (trafficRef) {
            //TODO isn't this best executed right at the beginning?
            if (! aiTrafficVisible()) {
                setDie(true);
                return;
            }

            if (! handleAirportEndPoints(prev, now)) {
                setDie(true);
                return;
            }

            announcePositionToController();

        }

        if (next) {
            fp->setLeadDistance(speed, tgt_heading, curr, next);
        }

        if (!(prev->on_ground))  // only update the tgt altitude from flightplan if not on the ground
        {
            tgt_altitude_ft = prev->altitude;
            if (curr->crossat > -1000.0) {
                use_perf_vs = false;
                tgt_vs = (curr->crossat - altitude_ft) / (fp->getDistanceToGo(pos.getLatitudeDeg(), pos.getLongitudeDeg(), curr)
                         / 6076.0 / speed*60.0);
                tgt_altitude_ft = curr->crossat;
            } else {
                use_perf_vs = true;
            }
        }
        tgt_speed = prev->speed;
        hdg_lock = alt_lock = true;
        no_roll = prev->on_ground;
    }
}


void FGAIAircraft::initializeFlightPlan() {
}


bool FGAIAircraft::_getGearDown() const {
    return ((props->getFloatValue("position/altitude-agl-ft") < 900.0)
            && (props->getFloatValue("velocities/airspeed-kt")
                < performance->land_speed*1.25));
}


void FGAIAircraft::loadNextLeg() {

    int leg;
    if ((leg = fp->getLeg())  == 10) {
        trafficRef->next();
        setCallSign(trafficRef->getCallSign());
        leg = 1;
        fp->setLeg(leg);
    }

    FGAirport *dep = trafficRef->getDepartureAirport();
    FGAirport *arr = trafficRef->getArrivalAirport();
    if (!(dep && arr)) {
        setDie(true);

    } else {
        double cruiseAlt = trafficRef->getCruiseAlt() * 100;

        fp->create (dep,
                    arr,
                    leg,
                    cruiseAlt,           //(trafficRef->getCruiseAlt() * 100), // convert from FL to feet
                    trafficRef->getSpeed(),
                    _getLatitude(),
                    _getLongitude(),
                    false,
                    trafficRef->getRadius(),
                    trafficRef->getFlightType(),
                    acType,
                    company);
    }
}


// Note: This code is copied from David Luff's AILocalTraffic
// Warning - ground elev determination is CPU intensive
// Either this function or the logic of how often it is called
// will almost certainly change.

void FGAIAircraft::getGroundElev(double dt) {
    dt_elev_count += dt;

    // Update minimally every three secs, but add some randomness
    // to prevent all IA objects doing this in synchrony
    if (dt_elev_count < (3.0) + (rand() % 10))
        return;
    else
        dt_elev_count = 0;

    // Only do the proper hitlist stuff if we are within visible range of the viewer.
    if (!invisible) {
        double visibility_meters = fgGetDouble("/environment/visibility-m");

        FGViewer* vw = globals->get_current_view();
        double course, distance;

        SGWayPoint current(pos.getLongitudeDeg(), pos.getLatitudeDeg(), 0);
        SGWayPoint view (vw->getLongitude_deg(), vw->getLatitude_deg(), 0);
        view.CourseAndDistance(current, &course, &distance);
        if (distance > visibility_meters) {
            //aip.getSGLocation()->set_cur_elev_m(aptElev);
            return;
        }

        // FIXME: make sure the pos.lat/pos.lon values are in degrees ...
        double range = 500.0;
        if (!globals->get_tile_mgr()->scenery_available(pos.getLatitudeDeg(), pos.getLongitudeDeg(), range)) {
            // Try to shedule tiles for that position.
            globals->get_tile_mgr()->update( aip.getSGLocation(), range );
        }

        // FIXME: make sure the pos.lat/pos.lon values are in degrees ...
        double alt;
        if (globals->get_scenery()->get_elevation_m(pos.getLatitudeDeg(), pos.getLongitudeDeg(), 20000.0, alt, 0))
            tgt_altitude_ft = alt * SG_METER_TO_FEET;
    }
}


void FGAIAircraft::doGroundAltitude() {
    if (fabs(altitude_ft - (tgt_altitude_ft+groundOffset)) > 1000.0)
        altitude_ft = (tgt_altitude_ft + groundOffset);
    else
        altitude_ft += 0.1 * ((tgt_altitude_ft+groundOffset) - altitude_ft);
}


void FGAIAircraft::announcePositionToController() {
    if (trafficRef) {
        int leg = fp->getLeg();

        // Note that leg was been incremented after creating the current leg, so we should use
        // leg numbers here that are one higher than the number that is used to create the leg
        //
        switch (leg) {
        case 3:              // Taxiing to runway
            if (trafficRef->getDepartureAirport()->getDynamics()->getGroundNetwork()->exists())
                controller = trafficRef->getDepartureAirport()->getDynamics()->getGroundNetwork();
            break;
        case 4:              //Take off tower controller
            if (trafficRef->getDepartureAirport()->getDynamics()) {
                controller = trafficRef->getDepartureAirport()->getDynamics()->getTowerController();
                //if (trafficRef->getDepartureAirport()->getId() == "EHAM") {
                //cerr << trafficRef->getCallSign() << " at runway " << fp->getRunway() << "Ready for departure "
                //   << trafficRef->getFlightType() << " to " << trafficRef->getArrivalAirport()->getId() << endl;
                //  if (controller == 0) {
                //cerr << "Error in assigning controller at " << trafficRef->getDepartureAirport()->getId() << endl;
                //}

            } else {
                cerr << "Error: Could not find Dynamics at airport : " << trafficRef->getDepartureAirport()->getId() << endl;
            }
            break;
        case 9:              // Taxiing for parking
            if (trafficRef->getArrivalAirport()->getDynamics()->getGroundNetwork()->exists())
                controller = trafficRef->getArrivalAirport()->getDynamics()->getGroundNetwork();
            break;
        default:
            controller = 0;
            break;
        }

        if ((controller != prevController) && (prevController != 0)) {
            prevController->signOff(getID());
            string callsign =  trafficRef->getCallSign();
            if ( trafficRef->getHeavy())
                callsign += "Heavy";
            switch (leg) {
            case 3:
                //cerr << callsign << " ready to taxi to runway " << fp->getRunway() << endl;
                break;
            case 4:
                //cerr << callsign << " at runway " << fp->getRunway() << "Ready for take-off. "
                //     << trafficRef->getFlightRules() << " to " << trafficRef->getArrivalAirport()->getId()
                //     << "(" << trafficRef->getArrivalAirport()->getName() << ")."<< endl;
                break;
            }
        }
        prevController = controller;
        if (controller) {
            controller->announcePosition(getID(), fp, fp->getCurrentWaypoint()->routeIndex,
                                         _getLatitude(), _getLongitude(), hdg, speed, altitude_ft,
                                         trafficRef->getRadius(), leg, trafficRef->getCallSign());
        }
    }
}


void FGAIAircraft::processATC(FGATCInstruction instruction) {
    //cerr << "Processing ATC instruction (not Implimented yet)" << endl;
    if (instruction.getHoldPattern   ()) {}

    // Hold Position
    if (instruction.getHoldPosition  ()) {
        if (!holdPos) {
            holdPos = true;
        }
        AccelTo(0.0);
    } else {
        if (holdPos) {
            //if (trafficRef)
            //	cerr << trafficRef->getCallSign() << " Resuming Taxi " << endl;
            holdPos = false;
        }
        // Change speed Instruction. This can only be excecuted when there is no
        // Hold position instruction.
        if (instruction.getChangeSpeed   ()) {
            //  if (trafficRef)
            //cerr << trafficRef->getCallSign() << " Changing Speed " << endl;
            AccelTo(instruction.getSpeed());
        } else {
            if (fp) AccelTo(fp->getPreviousWaypoint()->speed);
        }
    }
    if (instruction.getChangeHeading ()) {
        hdg_lock = false;
        TurnTo(instruction.getHeading());
    } else {
        if (fp) {
            hdg_lock = true;
        }
    }
    if (instruction.getChangeAltitude()) {}

}


void FGAIAircraft::handleFirstWaypoint() {
    bool eraseWaypoints;         //TODO YAGNI
    if (trafficRef) {
        eraseWaypoints = true;
    } else {
        eraseWaypoints = false;
    }

    FGAIFlightPlan::waypoint* prev = 0; // the one behind you
    FGAIFlightPlan::waypoint* curr = 0; // the one ahead
    FGAIFlightPlan::waypoint* next = 0;// the next plus 1

    spinCounter = 0;
    tempReg = "";

    //TODO fp should handle this
    fp->IncrementWaypoint(eraseWaypoints);
    if (!(fp->getNextWaypoint()) && trafficRef)
        loadNextLeg();

    prev = fp->getPreviousWaypoint();   //first waypoint
    curr = fp->getCurrentWaypoint();    //second waypoint
    next = fp->getNextWaypoint();       //third waypoint (might not exist!)

    setLatitude(prev->latitude);
    setLongitude(prev->longitude);
    setSpeed(prev->speed);
    setAltitude(prev->altitude);

    if (prev->speed > 0.0)
        setHeading(fp->getBearing(prev->latitude, prev->longitude, curr));
    else
        setHeading(fp->getBearing(curr->latitude, curr->longitude, prev));

    // If next doesn't exist, as in incrementally created flightplans for
    // AI/Trafficmanager created plans,
    // Make sure lead distance is initialized otherwise
    if (next)
        fp->setLeadDistance(speed, hdg, curr, next);

    if (curr->crossat > -1000.0) //use a calculated descent/climb rate
    {
        use_perf_vs = false;
        tgt_vs = (curr->crossat - prev->altitude)
                 / (fp->getDistanceToGo(pos.getLatitudeDeg(), pos.getLongitudeDeg(), curr)
                    / 6076.0 / prev->speed*60.0);
        tgt_altitude_ft = curr->crossat;
    } else {
        use_perf_vs = true;
        tgt_altitude_ft = prev->altitude;
    }
    alt_lock = hdg_lock = true;
    no_roll = prev->on_ground;
    if (no_roll) {
        Transform();             // make sure aip is initialized.
        getGroundElev(60.1);     // make sure it's executed first time around, so force a large dt value
        doGroundAltitude();
    }
    // Make sure to announce the aircraft's position
    announcePositionToController();
    prevSpeed = 0;
}


/**
 * Check Execution time (currently once every 100 ms)
 * Add a bit of randomization to prevent the execution of all flight plans
 * in synchrony, which can add significant periodic framerate flutter.
 *
 * @param now
 * @return
 */
bool FGAIAircraft::fpExecutable(time_t now) {
    double rand_exec_time = (rand() % 100) / 100;
    return (dt_count > (0.1+rand_exec_time)) && (fp->isActive(now));
}


/**
 * Check to see if we've reached the lead point for our next turn
 *
 * @param curr
 * @return
 */
bool FGAIAircraft::leadPointReached(FGAIFlightPlan::waypoint* curr) {
    double dist_to_go = fp->getDistanceToGo(pos.getLatitudeDeg(), pos.getLongitudeDeg(), curr);

    //cerr << "2" << endl;
    double lead_dist = fp->getLeadDistance();
    //cerr << " Distance : " << dist_to_go << ": Lead distance " << lead_dist << endl;
    // experimental: Use fabs, because speed can be negative (I hope) during push_back.

    if (lead_dist < fabs(2*speed)) {
        //don't skip over the waypoint
        lead_dist = fabs(2*speed);
        //cerr << "Extending lead distance to " << lead_dist << endl;
    }

    //prev_dist_to_go = dist_to_go;
    return dist_to_go < lead_dist;
}


bool FGAIAircraft::aiTrafficVisible() {
    double userLatitude  = fgGetDouble("/position/latitude-deg");
    double userLongitude = fgGetDouble("/position/longitude-deg");
    double course, distance;

    SGWayPoint current(pos.getLongitudeDeg(), pos.getLatitudeDeg(), 0);
    SGWayPoint user (userLongitude, userLatitude, 0);

    user.CourseAndDistance(current, &course, &distance);

    return ((distance * SG_METER_TO_NM) <= TRAFFICTOAIDIST);
}


/**
 * Handle release of parking gate, once were taxiing. Also ensure service time at the gate
 * in the case of an arrival.
 *
 * @param prev
 * @return
 */

//TODO the trafficRef is the right place for the method
bool FGAIAircraft::handleAirportEndPoints(FGAIFlightPlan::waypoint* prev, time_t now) {
    // prepare routing from one airport to another
    FGAirport * dep = trafficRef->getDepartureAirport();
    FGAirport * arr = trafficRef->getArrivalAirport();

    if (!( dep && arr))
        return false;

    // This waypoint marks the fact that the aircraft has passed the initial taxi
    // departure waypoint, so it can release the parking.
    if (prev->name == "park2")
        dep->getDynamics()->releaseParking(fp->getGate());

    // This is the last taxi waypoint, and marks the the end of the flight plan
    // so, the schedule should update and wait for the next departure time.
    if (prev->name == "END") {
        time_t nextDeparture = trafficRef->getDepartureTime();
        // make sure to wait at least 20 minutes at parking to prevent "nervous" taxi behavior
        if (nextDeparture < (now+1200)) {
            nextDeparture = now + 1200;
        }
        fp->setTime(nextDeparture);
    }

    return true;
}


/**
 * Check difference between target bearing and current heading and correct if necessary.
 *
 * @param curr
 */
void FGAIAircraft::controlHeading(FGAIFlightPlan::waypoint* curr) {
    double calc_bearing = fp->getBearing(pos.getLatitudeDeg(), pos.getLongitudeDeg(), curr);
    //cerr << "Bearing = " << calc_bearing << endl;
    if (speed < 0) {
        calc_bearing +=180;
        if (calc_bearing > 360)
            calc_bearing -= 360;
    }

    if (finite(calc_bearing)) {
        double hdg_error = calc_bearing - tgt_heading;
        if (fabs(hdg_error) > 1.0) {
            TurnTo( calc_bearing );
        }

    } else {
        cerr << "calc_bearing is not a finite number : "
        << "Speed " << speed
        << "pos : " << pos.getLatitudeDeg() << ", " << pos.getLongitudeDeg()
        << "waypoint " << curr->latitude << ", " << curr->longitude << endl;
        cerr << "waypoint name " << curr->name;
        exit(1);                 // FIXME
    }
}


/**
 * Update the lead distance calculation if speed has changed sufficiently
 * to prevent spinning (hopefully);
 *
 * @param curr
 * @param next
 */
void FGAIAircraft::controlSpeed(FGAIFlightPlan::waypoint* curr, FGAIFlightPlan::waypoint* next) {
    double speed_diff = speed - prevSpeed;

    if (fabs(speed_diff) > 10) {
        prevSpeed = speed;
        fp->setLeadDistance(speed, tgt_heading, curr, next);
    }
}


/**
 * Update target values (heading, alt, speed) depending on flight plan or control properties
 */
bool FGAIAircraft::updateTargetValues() {
    if (fp)                      // AI object has a flightplan
    {
        //TODO make this a function of AIBase
        time_t now = time(NULL) + fgGetLong("/sim/time/warp");
        //cerr << "UpateTArgetValues() " << endl;
        ProcessFlightPlan(dt, now);
        if (! fp->isActive(now)) {
            // Do execute Ground elev for inactive aircraft, so they
            // Are repositioned to the correct ground altitude when the user flies within visibility range.
            // In addition, check whether we are out of user range, so this aircraft
            // can be deleted.
            if (no_roll) {
                Transform();     // make sure aip is initialized.
                if (trafficRef) {
                    //cerr << trafficRef->getRegistration() << " Setting altitude to " << altitude_ft;
                    if (! aiTrafficVisible()) {
                        setDie(true);
                        return false;
                    }
                    getGroundElev(dt);
                    doGroundAltitude();
                    // Transform();
                    pos.setElevationFt(altitude_ft);
                }
            }
            return false;
        }
    }
    else {
        // no flight plan, update target heading, speed, and altitude
        // from control properties.  These default to the initial
        // settings in the config file, but can be changed "on the
        // fly".
        string lat_mode = props->getStringValue("controls/flight/lateral-mode");
        if ( lat_mode == "roll" ) {
            double angle
            = props->getDoubleValue("controls/flight/target-roll" );
            RollTo( angle );
        } else {
            double angle
            = props->getDoubleValue("controls/flight/target-hdg" );
            TurnTo( angle );
        }

        string lon_mode
        = props->getStringValue("controls/flight/longitude-mode");
        if ( lon_mode == "alt" ) {
            double alt = props->getDoubleValue("controls/flight/target-alt" );
            ClimbTo( alt );
        } else {
            double angle
            = props->getDoubleValue("controls/flight/target-pitch" );
            PitchTo( angle );
        }

        AccelTo( props->getDoubleValue("controls/flight/target-spd" ) );
    }
    return true;
}


/**
 * Adjust the speed (accelerate/decelerate) to tgt_speed.
 */
void FGAIAircraft::adjustSpeed(double tgt_speed) {
    double speed_diff = tgt_speed - speed;
    speed_diff = groundTargetSpeed - speed;

    if (speed_diff > 0.0)        // need to accelerate
    {
        speed += performance->accel * dt;
        if ( speed > tgt_speed )
            speed = tgt_speed;

    } else if (speed_diff < 0.0) {
        if (no_roll) {
            // on ground (aircraft can't roll)
            // deceleration performance is better due to wheel brakes.
            speed -= performance->decel * dt * 3;
        } else {
            speed -= performance->decel * dt;
        }

        if ( speed < tgt_speed )
            speed = tgt_speed;

    }
}


void FGAIAircraft::updatePosition() {
    // convert speed to degrees per second
    double speed_north_deg_sec = cos( hdg * SGD_DEGREES_TO_RADIANS )
                                 * speed * 1.686 / ft_per_deg_lat;
    double speed_east_deg_sec  = sin( hdg * SGD_DEGREES_TO_RADIANS )
                                 * speed * 1.686 / ft_per_deg_lon;

    // set new position
    pos.setLatitudeDeg( pos.getLatitudeDeg() + speed_north_deg_sec * dt);
    pos.setLongitudeDeg( pos.getLongitudeDeg() + speed_east_deg_sec * dt);
}


void FGAIAircraft::updateHeading() {
    // adjust heading based on current bank angle
    if (roll == 0.0)
        roll = 0.01;

    if (roll != 0.0) {
        // double turnConstant;
        //if (no_roll)
        //  turnConstant = 0.0088362;
        //else
        //  turnConstant = 0.088362;
        // If on ground, calculate heading change directly
        if (no_roll) {
            double headingDiff = fabs(hdg-tgt_heading);

            if (headingDiff > 180)
                headingDiff = fabs(headingDiff - 360);

            groundTargetSpeed = tgt_speed - (tgt_speed * (headingDiff/45));
            if (sign(groundTargetSpeed) != sign(tgt_speed))
                groundTargetSpeed = 0.21 * sign(tgt_speed); // to prevent speed getting stuck in 'negative' mode

            if (headingDiff > 30.0) {
                // invert if pushed backward
                headingChangeRate += dt * sign(roll);

                if (headingChangeRate > 30)
                    headingChangeRate = 30;
                else if (headingChangeRate < -30)
                    headingChangeRate = -30;

            } else {
                if (fabs(headingChangeRate) > headingDiff)
                    headingChangeRate = headingDiff*sign(roll);
                else
                    headingChangeRate += dt * sign(roll);
            }
            hdg += headingChangeRate * dt;
        } else {
            if (fabs(speed) > 1.0) {
                turn_radius_ft = 0.088362 * speed * speed
                                 / tan( fabs(roll) / SG_RADIANS_TO_DEGREES );
            } else {
                // Check if turn_radius_ft == 0; this might lead to a division by 0.
                turn_radius_ft = 1.0;
            }
            double turn_circum_ft = SGD_2PI * turn_radius_ft;
            double dist_covered_ft = speed * 1.686 * dt;
            double alpha = dist_covered_ft / turn_circum_ft * 360.0;
            hdg += alpha * sign(roll);
        }
        while ( hdg > 360.0 ) {
            hdg -= 360.0;
            spinCounter++;
        }
        while ( hdg < 0.0) {
            hdg += 360.0;
            spinCounter--;
        }
    }
}


void FGAIAircraft::updateBankAngles() {
    // adjust target bank angle if heading lock engaged
    if (hdg_lock) {
        double bank_sense = 0.0;
        double diff = fabs(hdg - tgt_heading);
        if (diff > 180)
            diff = fabs(diff - 360);

        double sum = hdg + diff;
        if (sum > 360.0)
            sum -= 360.0;
        if (fabs(sum - tgt_heading) < 1.0) {
            bank_sense = 1.0;    // right turn
        } else {
            bank_sense = -1.0;   // left turn
        }
        if (diff < 30) {
            tgt_roll = diff * bank_sense;
        } else {
            tgt_roll = 30.0 * bank_sense;
        }
        if ((fabs((double) spinCounter) > 1) && (diff > 30)) {
            tgt_speed *= 0.999;  // Ugly hack: If aircraft get stuck, they will continually spin around.
            // The only way to resolve this is to make them slow down.
        }
    }

    // adjust bank angle, use 9 degrees per second
    double bank_diff = tgt_roll - roll;
    if (fabs(bank_diff) > 0.2) {
        if (bank_diff > 0.0)
            roll += 9.0 * dt;

        if (bank_diff < 0.0)
            roll -= 9.0 * dt;
        //while (roll > 180) roll -= 360;
        //while (roll < 180) roll += 360;
    }
}


void FGAIAircraft::updateAltitudes() {
    // adjust altitude (meters) based on current vertical speed (fpm)
    altitude_ft += vs / 60.0 * dt;
    pos.setElevationFt(altitude_ft);

    // adjust target Altitude, based on ground elevation when on ground
    if (no_roll) {
        getGroundElev(dt);
        doGroundAltitude();
    } else {
        // find target vertical speed if altitude lock engaged
        if (alt_lock && use_perf_vs) {
            if (altitude_ft < tgt_altitude_ft) {
                tgt_vs = tgt_altitude_ft - altitude_ft;
                if (tgt_vs > performance->climb_rate)
                    tgt_vs = performance->climb_rate;
            } else {
                tgt_vs = tgt_altitude_ft - altitude_ft;
                if (tgt_vs  < (-performance->descent_rate))
                    tgt_vs = -performance->descent_rate;
            }
        }

        if (alt_lock && !use_perf_vs) {
            double max_vs = 4*(tgt_altitude_ft - altitude_ft);
            double min_vs = 100;
            if (tgt_altitude_ft < altitude_ft)
                min_vs = -100.0;
            if ((fabs(tgt_altitude_ft - altitude_ft) < 1500.0)
                    && (fabs(max_vs) < fabs(tgt_vs)))
                tgt_vs = max_vs;

            if (fabs(tgt_vs) < fabs(min_vs))
                tgt_vs = min_vs;
        }
    }
}


void FGAIAircraft::updateVerticalSpeed() {
    // adjust vertical speed
    double vs_diff = tgt_vs - vs;
    if (fabs(vs_diff) > 10.0) {
        if (vs_diff > 0.0) {
            vs += (performance->climb_rate / 3.0) * dt;

            if (vs > tgt_vs)
                vs = tgt_vs;
        } else {
            vs -= (performance->descent_rate / 3.0) * dt;

            if (vs < tgt_vs)
                vs = tgt_vs;
        }
    }
}


void FGAIAircraft::matchPitchAngle() {
    // match pitch angle to vertical speed
    if (vs > 0) {
        pitch = vs * 0.005;
    } else {
        pitch = vs * 0.002;
    }
}
