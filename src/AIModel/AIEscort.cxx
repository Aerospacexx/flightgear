// FGAIEscort - FGAIShip-derived class creates an AI Ground Vehicle
// by adding a ground following utility
//
// Written by Vivian Meazza, started August 2009.
// - vivian.meazza at lineone.net
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

#include <algorithm>
#include <string>
#include <vector>

#include <simgear/sg_inlines.h>
#include <simgear/math/SGMath.hxx>
#include <simgear/math/sg_geodesy.hxx>

#include <math.h>
#include <Main/util.hxx>
#include <Main/viewer.hxx>

#include <Scenery/scenery.hxx>
#include <Scenery/tilemgr.hxx>

#include "AIEscort.hxx"

FGAIEscort::FGAIEscort() :
FGAIShip(otEscort),

_selected_ac(0),
_relbrg (0),
_stn_truebrg(0),
_parent_speed(0),
_stn_limit(0),
_stn_angle_limit(0),
_stn_speed(0),
_stn_height(0),
_max_speed(0),
_interval(0),
_MPControl(false),
_patrol(false),
_stn_deg_true(false),
_parent("")

{
    invisible = false;
}

FGAIEscort::~FGAIEscort() {}

void FGAIEscort::readFromScenario(SGPropertyNode* scFileNode) {
    if (!scFileNode)
        return;

    FGAIShip::readFromScenario(scFileNode);

    setName(scFileNode->getStringValue("name", "Escort"));
    setSMPath(scFileNode->getStringValue("submodel-path", ""));
    setStnRange(scFileNode->getDoubleValue("station/range-nm", 1));
    setStnBrg(scFileNode->getDoubleValue("station/brg-deg", 0.0));
    setStnLimit(scFileNode->getDoubleValue("station/range-limit-nm", 0.2));
    setStnAngleLimit(scFileNode->getDoubleValue("station/angle-limit-deg", 15.0));
    setStnSpeed(scFileNode->getDoubleValue("station/speed-kts", 2.5));
    setStnPatrol(scFileNode->getBoolValue("station/patrol", false));
    setStnHtFt(scFileNode->getDoubleValue("station/height-ft", 0.0));
    setStnDegTrue(scFileNode->getBoolValue("station/deg-true", false));
    setParentName(scFileNode->getStringValue("station/parent", ""));
    setMaxSpeed(scFileNode->getDoubleValue("max-speed-kts", 30.0));
    setUpdateInterval(scFileNode->getDoubleValue("update-interval-sec", 10.0));
    setCallSign(scFileNode->getStringValue("callsign", ""));

    if(_patrol)
        sg_srandom_time();

}

void FGAIEscort::bind() {
    FGAIShip::bind();

    props->tie("station/rel-bearing-deg",
        SGRawValuePointer<double>(&_stn_relbrg));
    props->tie("station/true-bearing-deg",
        SGRawValuePointer<double>(&_stn_truebrg));
    props->tie("station/range-nm",
        SGRawValuePointer<double>(&_stn_range));
    props->tie("station/range-limit-nm",
        SGRawValuePointer<double>(&_stn_limit));
    props->tie("station/angle-limit-deg",
        SGRawValuePointer<double>(&_stn_angle_limit));
    props->tie("station/speed-kts",
        SGRawValuePointer<double>(&_stn_speed));
    props->tie("station/height-ft",
        SGRawValuePointer<double>(&_stn_height));
    props->tie("controls/update-interval-sec",
        SGRawValuePointer<double>(&_interval));
    props->tie("controls/parent-mp-control",
        SGRawValuePointer<bool>(&_MPControl));
    props->tie("station/target-range-nm",
        SGRawValuePointer<double>(&_tgtrange));
    props->tie("station/target-brg-deg-t",
        SGRawValuePointer<double>(&_tgtbrg));
    props->tie("station/patrol",
        SGRawValuePointer<bool>(&_patrol));
}

void FGAIEscort::unbind() {
    FGAIShip::unbind();

    props->untie("station/rel-bearing-deg");
    props->untie("station/true-bearing-deg");
    props->untie("station/range-nm");
    props->untie("station/range-limit-nm");
    props->untie("station/angle-limit-deg");
    props->untie("station/speed-kts");
    props->untie("station/height-ft");
    props->untie("controls/update-interval-sec");

}

bool FGAIEscort::init(bool search_in_AI_path) {
    if (!FGAIShip::init(search_in_AI_path))
        return false;

    invisible = false;
    no_roll = false;

    props->setStringValue("controls/parent-name", _parent.c_str());
    setParent();
    pos = _tgtpos;
    speed = _parent_speed;
    hdg = _parent_hdg;

    return true;
}

void FGAIEscort::update(double dt) {
    FGAIShip::update(dt);

    RunEscort(dt);
}

void FGAIEscort::setStnRange(double r) {
    _stn_range = r;
}

void FGAIEscort::setStnBrg(double b) {
    _stn_brg = b;
}

void FGAIEscort::setStnLimit(double l) {
    _stn_limit = l;
}

void FGAIEscort::setStnAngleLimit(double al) {
    _stn_angle_limit = al;
}

void FGAIEscort::setStnSpeed(double s) {
    _stn_speed = s;
}

void FGAIEscort::setStnHtFt(double h) {
    _stn_height = h;
}

void FGAIEscort::setStnDegTrue(bool t) {
    _stn_deg_true = t;
}

void FGAIEscort::setMaxSpeed(double m) {
    _max_speed = m;
}

void FGAIEscort::setUpdateInterval(double i) {
    _interval = i;
}

void FGAIEscort::setStnPatrol(bool p) {
    _patrol = p;
}

void FGAIEscort::setParentName(const string& p) {
    _parent = p;
}

bool FGAIEscort::getGroundElev(SGGeod inpos) {

    double height_m ;

    if (globals->get_scenery()->get_elevation_m(SGGeod::fromGeodM(inpos, 3000), height_m, &_material,0)){
        _ht_agl_ft = inpos.getElevationFt() - height_m * SG_METER_TO_FEET;

        if (_material) {
            const vector<string>& names = _material->get_names();

            _solid = _material->get_solid();

            if (!names.empty())
                props->setStringValue("material/name", names[0].c_str());
            else
                props->setStringValue("material/name", "");

            //cout << "material " << names[0].c_str()
            //    << " _elevation_m " << _elevation_m
            //    << " solid " << _solid
            //    << " load " << _load_resistance
            //    << " frictionFactor " << _frictionFactor
            //    << endl;

        }

        return true;
    } else {
        return false;
    }

}

void FGAIEscort::setParent() {

    const SGPropertyNode *ai = fgGetNode("/ai/models", true);

    for (int i = ai->nChildren() - 1; i >= -1; i--) {
        const SGPropertyNode *model;

        if (i < 0) { // last iteration: selected model
            model = _selected_ac;
        } else {
            model = ai->getChild(i);
            string path = ai->getPath();
            const string name = model->getStringValue("name");

            if (!model->nChildren()){
                continue;
            }
            if (name == _parent) {
                _selected_ac = model;  // save selected model for last iteration
                break;
            }

        }
        if (!model)
            continue;

    }// end for loop

    if (_selected_ac != 0){
        const string name = _selected_ac->getStringValue("name");
        double lat = _selected_ac->getDoubleValue("position/latitude-deg");
        double lon = _selected_ac->getDoubleValue("position/longitude-deg");
        double elevation = _selected_ac->getDoubleValue("position/altitude-ft");
        _MPControl = _selected_ac->getBoolValue("controls/mp-control");

        _selectedpos.setLatitudeDeg(lat);
        _selectedpos.setLongitudeDeg(lon);
        _selectedpos.setElevationFt(elevation);

        _parent_speed    = _selected_ac->getDoubleValue("velocities/speed-kts");
        _parent_hdg      = _selected_ac->getDoubleValue("orientation/true-heading-deg");

        if(!_stn_deg_true){
            _stn_truebrg = calcTrueBearingDeg(_stn_brg, _parent_hdg);
            _stn_relbrg = _stn_brg;
            //cout << _name <<" set rel"<<endl;
        } else {
            _stn_truebrg = _stn_brg;
            _stn_relbrg = calcRelBearingDeg(_stn_brg, _parent_hdg); 
            //cout << _name << " set true"<<endl;
        }

        double course2;

        SGGeodesy::direct( _selectedpos, _stn_truebrg, _stn_range * SG_NM_TO_METER,
            _tgtpos, course2);

        _tgtpos.setElevationFt(_stn_height);

        calcRangeBearing(pos.getLatitudeDeg(), pos.getLongitudeDeg(),
            _tgtpos.getLatitudeDeg(), _tgtpos.getLongitudeDeg(), _tgtrange, _tgtbrg);

        _relbrg = calcRelBearingDeg(_tgtbrg, hdg);

    } else {
        SG_LOG(SG_GENERAL, SG_ALERT, "AIEscort: " << _name
            << " parent not found: dying ");
        setDie(true);
    }

}

void FGAIEscort::calcRangeBearing(double lat, double lon, double lat2, double lon2,
                                  double &range, double &bearing) const
{
    // calculate the bearing and range of the second pos from the first
    double az2, distance;
    geo_inverse_wgs_84(lat, lon, lat2, lon2, &bearing, &az2, &distance);
    range = distance * SG_METER_TO_NM;
}

double FGAIEscort::calcRelBearingDeg(double bearing, double heading)
{
    double angle = bearing - heading;
    SG_NORMALIZE_RANGE(angle, -180.0, 180.0);
    return angle;
}

double FGAIEscort::calcTrueBearingDeg(double bearing, double heading)
{
    double angle = bearing + heading;
    SG_NORMALIZE_RANGE(angle, 0.0, 360.0);
    return angle;
}

double FGAIEscort::calcRecipBearingDeg(double bearing)
{
    double angle = bearing - 180;
    SG_NORMALIZE_RANGE(angle, 0.0, 360.0);
    return angle;
}

SGVec3d FGAIEscort::getCartHitchPosAt(const SGVec3d& _off) const {
    double hdg = _selected_ac->getDoubleValue("orientation/true-heading-deg");
    double pitch = _selected_ac->getDoubleValue("orientation/pitch-deg");
    double roll = _selected_ac->getDoubleValue("orientation/roll-deg");

    // Transform that one to the horizontal local coordinate system.
    SGQuatd hlTrans = SGQuatd::fromLonLat(_selectedpos);

    // and postrotate the orientation of the AIModel wrt the horizontal
    // local frame
    hlTrans *= SGQuatd::fromYawPitchRollDeg(hdg, pitch, roll);

    // The offset converted to the usual body fixed coordinate system
    // rotated to the earth fiexed coordinates axis
    SGVec3d off = hlTrans.backTransform(_off);

    // Add the position offset of the AIModel to gain the earth centered position
    SGVec3d cartPos = SGVec3d::fromGeod(_selectedpos);

    return cartPos + off;
}


void FGAIEscort::setStationSpeed(){

    double speed = 0;
    double angle = 0;

    // these are the AI rules for the manoeuvring of escorts

    if (_MPControl && _tgtrange > 4 * _stn_limit){
        SG_LOG(SG_GENERAL, SG_ALERT, "AIEscort: " << _name
            << " re-aligning to MP pos");
        pos = _tgtpos;
        speed = 0;
        angle = 0;
    }else if ((_relbrg < -90 || _relbrg > 90) && _tgtrange > _stn_limit ){
        angle =_relbrg;

        if(_tgtrange > 4 * _stn_limit)
            speed = 4 * -_stn_speed;
        else
            speed = -_stn_speed;

    }else if ((_relbrg >= -90 || _relbrg <= 90) && _tgtrange > _stn_limit){
        angle = _relbrg;

        if(_tgtrange > 4 * _stn_limit)
            speed = 4 * _stn_speed;
        else
            speed = _stn_speed;

    } else {

        if(_patrol){
            angle = 15 * sg_random();
            speed =  5 * sg_random();
        } else {
            angle = 0;
            speed = 0;
        }

    }

    double station_speed = _parent_speed + speed;

    SG_CLAMP_RANGE(station_speed, 5.0, _max_speed);
    SG_CLAMP_RANGE(angle, -_stn_angle_limit, _stn_angle_limit);

    AccelTo(station_speed);
    TurnTo(_parent_hdg + angle);
    ClimbTo(_stn_height);

}

void FGAIEscort::RunEscort(double dt){

    _dt_count += dt;



    ///////////////////////////////////////////////////////////////////////////
    // Check execution time (currently once every 0.05 sec or 20 fps)
    // Add a bit of randomization to prevent the execution of all flight plans
    // in synchrony, which can add significant periodic framerate flutter.
    // Randomization removed to get better appearance
    ///////////////////////////////////////////////////////////////////////////

    //cout << "_start_sec " << _start_sec << " time_sec " << time_sec << endl;
    if (_dt_count < _next_run)
        return;
    _next_run = _interval /*+ (0.015 * sg_random())*/;

    if(_parent == ""){
        return;
    }

    setParent();
    setStationSpeed();
    //getGroundElev(pos);

    _dt_count = 0;

}

// end AIGroundvehicle
