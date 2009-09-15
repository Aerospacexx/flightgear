// Build a cloud layer based on metar
//
// Written by Harald JOHNSEN, started April 2005.
//
// Copyright (C) 2005  Harald JOHNSEN - hjohnsen@evc.net
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
//
#ifndef _FGCLOUDS_HXX
#define _FGCLOUDS_HXX

#ifdef HAVE_CONFIG
#  include <config.h>
#endif

#include <simgear/compiler.h>
#include <simgear/sound/soundmgr_openal.hxx>
#include <Main/fg_props.hxx>

#include <string>

using std::string;

class SGNewCloud;
class SGCloudField;
class FGMetar;
class FGEnvironmentCtrl;

class FGClouds {

private:
void buildCloud(SGPropertyNode *cloud_def_root, SGPropertyNode *box_def_root, const string& name, sgVec3 pos, SGCloudField *layer);
	void buildLayer(int iLayer, const string& name, double alt, double coverage);

	void buildCloudLayers(void);

	void buildScenario( const string& scenario );

	void setLayer( int iLayer, float alt_m, const string& coverage, const string& layer_type );

	void update_metar_properties( const FGMetar *m );

	void update_env_config ();

        void build(void);

	int update_event;
	SGSoundSample *snd_lightning;
	FGEnvironmentCtrl * _controller;
	float station_elevation_ft;
        bool clouds_3d_enabled;
        string last_scenario;
        string scenario;
        SGPropertyNode_ptr last_env_config, last_env_clouds;

public:
	FGClouds(FGEnvironmentCtrl * controller);
	~FGClouds();

	void init(void);

        void set_scenario(const char * sc);
        const char * get_scenario(void) const;

	int get_update_event(void) const;
	void set_update_event(int count);
        bool get_3dClouds() const;
        void set_3dClouds(bool enable);
};

#endif // _FGCLOUDS_HXX
