// submodel.hxx - models a releasable submodel.
// Written by Dave Culp, started Aug 2004
//
// This file is in the Public Domain and comes with no warranty.


#ifndef __SYSTEMS_SUBMODEL_HXX
#define __SYSTEMS_SUBMODEL_HXX 1

#ifndef __cplusplus
# error This library requires C++
#endif

#include <simgear/props/props.hxx>
#include <simgear/structure/subsystem_mgr.hxx>
#include <AIModel/AIBase.hxx>
#include <vector>
#include <string>
SG_USING_STD(vector);
SG_USING_STD(string);

class FGAIBase;

class FGSubmodelMgr : public SGSubsystem
{

public:

    typedef struct
    {
        SGPropertyNode_ptr trigger_node;
        SGPropertyNode_ptr prop;
        SGPropertyNode_ptr contents_node;
        SGPropertyNode_ptr submodel_node;
        SGPropertyNode_ptr speed_node;

        string             name;
        string             model;
        double             speed;
        bool               slaved;
        bool               repeat;
        double             delay;
        double             timer;
        int                count;
        double             x_offset;
        double             y_offset;
        double             z_offset;
        double             yaw_offset;
        double             pitch_offset;
        double             drag_area;
        double             life;
        double             buoyancy;
        bool               wind;
        bool               first_time;
        double             cd;
        double             weight;
        double             contents;
        bool               aero_stabilised;
        int                id;
        bool               no_roll;
        bool               serviceable;
    }
    submodel;

    typedef struct
    {
        double     lat;
        double     lon;
        double     alt;
        double     roll;
        double     azimuth;
        double     elevation;
        double     speed;
        double     wind_from_east;
        double     wind_from_north;
        double     speed_down_fps;
        double     speed_east_fps;
        double     speed_north_fps;
        double     total_speed_down;
        double     total_speed_east;
        double     total_speed_north;
        double     mass;
        int        id;
        bool       no_roll;
    }
    IC_struct;

    FGSubmodelMgr();
    ~FGSubmodelMgr();

    void load();
    void init();
    void bind();
    void unbind();
    void update(double dt);
    bool release(submodel* sm, double dt);
    void transform(submodel* sm);
    void updatelat(double lat);

private:

    typedef vector <submodel*> submodel_vector_type;
    typedef submodel_vector_type::const_iterator submodel_vector_iterator;

    submodel_vector_type       submodels;
    submodel_vector_iterator   submodel_iterator;

    float trans[3][3];
    float in[3];
    float out[3];

    double Rx, Ry, Rz;
    double Sx, Sy, Sz;
    double Tx, Ty, Tz;

    float cosRx, sinRx;
    float cosRy, sinRy;
    float cosRz, sinRz;

    int index;

    double ft_per_deg_longitude;
    double ft_per_deg_latitude;

    double x_offset, y_offset, z_offset;
    double pitch_offset, yaw_offset;

    static const double lbs_to_slugs; //conversion factor

    double contrail_altitude;

    SGPropertyNode_ptr _serviceable_node;
    SGPropertyNode_ptr _user_lat_node;
    SGPropertyNode_ptr _user_lon_node;
    SGPropertyNode_ptr _user_heading_node;
    SGPropertyNode_ptr _user_alt_node;
    SGPropertyNode_ptr _user_pitch_node;
    SGPropertyNode_ptr _user_roll_node;
    SGPropertyNode_ptr _user_yaw_node;
    SGPropertyNode_ptr _user_alpha_node;
    SGPropertyNode_ptr _user_speed_node;
    SGPropertyNode_ptr _user_wind_from_east_node;
    SGPropertyNode_ptr _user_wind_from_north_node;
    SGPropertyNode_ptr _user_speed_down_fps_node;
    SGPropertyNode_ptr _user_speed_east_fps_node;
    SGPropertyNode_ptr _user_speed_north_fps_node;
    SGPropertyNode_ptr _contrail_altitude_node;
    SGPropertyNode_ptr _contrail_trigger;

    FGAIManager* ai;
    IC_struct  IC;

    // A list of pointers to AI objects
    typedef list <SGSharedPtr<FGAIBase> > sm_list_type;
    typedef sm_list_type::iterator sm_list_iterator;
    typedef sm_list_type::const_iterator sm_list_const_iterator;

    sm_list_type sm_list;

    void loadAI();
    void loadSubmodels();
    double getRange(double lat, double lon, double lat2, double lon2) const;

};

#endif // __SYSTEMS_SUBMODEL_HXX
