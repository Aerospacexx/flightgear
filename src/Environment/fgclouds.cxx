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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <Main/fg_props.hxx>

#include <simgear/constants.h>
#include <simgear/sound/soundmgr_openal.hxx>
#include <simgear/scene/sky/sky.hxx>
#include <simgear/environment/visual_enviro.hxx>
#include <simgear/scene/sky/cloudfield.hxx>
#include <simgear/scene/sky/newcloud.hxx>
#include <simgear/math/sg_random.h>
#include <simgear/props/props_io.hxx>

#include <Main/globals.hxx>
#include <Airports/simple.hxx>
#include <Main/util.hxx>

#include "fgclouds.hxx"

extern SGSky *thesky;


FGClouds::FGClouds() :
    snd_lightning(0),
    clouds_3d_enabled(false)
{
	update_event = 0;
}

FGClouds::~FGClouds() {
}

int FGClouds::get_update_event(void) const {
	return update_event;
}

void FGClouds::set_update_event(int count) {
	update_event = count;
	buildCloudLayers();
}

void FGClouds::init(void) {
	if( snd_lightning == NULL ) {
		snd_lightning = new SGSoundSample(globals->get_fg_root().c_str(), "Sounds/thunder.wav");
		snd_lightning->set_max_dist(7000.0f);
		snd_lightning->set_reference_dist(3000.0f);
		SGSoundMgr *smgr = (SGSoundMgr *)globals->get_subsystem("soundmgr");
		SGSampleGroup *sgr = smgr->find("weather", true);
		sgr->add( snd_lightning, "thunder" );
		sgEnviro.set_sampleGroup( sgr );
	}
}

// Build an invidual cloud. Returns the extents of the cloud for coverage calculations
double FGClouds::buildCloud(SGPropertyNode *cloud_def_root, SGPropertyNode *box_def_root, const string& name, double grid_z_rand, SGCloudField *layer) {
	SGPropertyNode *box_def=NULL;
	SGPropertyNode *cld_def=NULL;
	double extent = 0.0;

	SGPath texture_root = globals->get_fg_root();
	texture_root.append("Textures");
	texture_root.append("Sky");

	box_def = box_def_root->getChild(name.c_str());
  
	string base_name = name.substr(0,2);
	if( !box_def ) {
		if( name[2] == '-' ) {
			box_def = box_def_root->getChild(base_name.c_str());
		}
		if( !box_def )
			return 0.0;
	}

	double x = sg_random() * SGCloudField::fieldSize - (SGCloudField::fieldSize / 2.0);
	double y = sg_random() * SGCloudField::fieldSize - (SGCloudField::fieldSize / 2.0);
	double z = grid_z_rand * (sg_random() - 0.5);
		
	sgVec3 pos={x,y,z};
	
        
	for(int i = 0; i < box_def->nChildren() ; i++) {
		SGPropertyNode *abox = box_def->getChild(i);
		if( strcmp(abox->getName(), "box") == 0) {

			string type = abox->getStringValue("type", "cu-small");
			cld_def = cloud_def_root->getChild(type.c_str());
			if ( !cld_def ) return 0.0;
			
			double w = abox->getDoubleValue("width", 1000.0);
			double h = abox->getDoubleValue("height", 1000.0);
			int hdist = abox->getIntValue("hdist", 1);
			int vdist = abox->getIntValue("vdist", 1);

			double c = abox->getDoubleValue("count", 5);
			int count = (int) (c + (sg_random() - 0.5) * c);

			extent = max(w*w, extent);

			for (int j = 0; j < count; j++)	{

				// Locate the clouds randomly in the defined space. The hdist and
				// vdist values control the horizontal and vertical distribution
				// by simply summing random components.
				double x = 0.0;
				double y = 0.0;
				double z = 0.0;

				for (int k = 0; k < hdist; k++)
				{
					x += (sg_random() / hdist);
					y += (sg_random() / hdist);
				}

				for (int k = 0; k < vdist; k++)
				{
					z += (sg_random() / vdist);
				}

				x = w * (x - 0.5) + pos[0]; // N/S
				y = w * (y - 0.5) + pos[1]; // E/W
				z = h * z + pos[2]; // Up/Down. pos[2] is the cloudbase

				SGVec3f newpos = SGVec3f(x, y, z);

				double min_width = cld_def->getDoubleValue("min-cloud-width-m", 500.0);
				double max_width = cld_def->getDoubleValue("max-cloud-width-m", 1000.0);
				double min_height = cld_def->getDoubleValue("min-cloud-height-m", min_width);
				double max_height = cld_def->getDoubleValue("max-cloud-height-m", max_width);
				double min_sprite_width = cld_def->getDoubleValue("min-sprite-width-m", 200.0);
				double max_sprite_width = cld_def->getDoubleValue("max-sprite-width-m", min_sprite_width);
				double min_sprite_height = cld_def->getDoubleValue("min-sprite-height-m", min_sprite_width);
				double max_sprite_height = cld_def->getDoubleValue("max-sprite-height-m", max_sprite_width);
				int num_sprites = cld_def->getIntValue("num-sprites", 20);
				int num_textures_x = cld_def->getIntValue("num-textures-x", 1);
				int num_textures_y = cld_def->getIntValue("num-textures-y", 1);
				double bottom_shade = cld_def->getDoubleValue("bottom-shade", 1.0);
				string texture = cld_def->getStringValue("texture", "cu.png");

				SGNewCloud *cld = 
					new SGNewCloud(type,
						texture_root, 
						texture, 
						min_width, 
						max_width, 
						min_height,
						max_height,
						min_sprite_width,
						max_sprite_width,
						min_sprite_height,
						max_sprite_height,
						bottom_shade,
						num_sprites,
						num_textures_x,
						num_textures_y);
				layer->addCloud(newpos, cld);
			}
		}
	}

	// Return the maximum extent of the cloud
	return extent;
}

void FGClouds::buildLayer(int iLayer, const string& name, double alt, double coverage) {
	struct {
		string name;
		double count;
	} tCloudVariety[20];
	int CloudVarietyCount = 0;
	double totalCount = 0.0;
        
	SGPropertyNode *cloud_def_root = fgGetNode("/environment/cloudlayers/clouds", false);
	SGPropertyNode *box_def_root   = fgGetNode("/environment/cloudlayers/boxes", false);
	SGPropertyNode *layer_def_root = fgGetNode("/environment/cloudlayers/layers", false);
        SGCloudField *layer = thesky->get_cloud_layer(iLayer)->get_layer3D();
	layer->clear();
        
	// If we don't have the required properties, then render the cloud in 2D
	if ((! clouds_3d_enabled) || coverage == 0.0 ||
		layer_def_root == NULL || cloud_def_root == NULL || box_def_root == NULL) {
			thesky->get_cloud_layer(iLayer)->set_enable3dClouds(false);
			return;
	}
        
	// If we can't find a definition for this cloud type, then render the cloud in 2D
	SGPropertyNode *layer_def=NULL;
	layer_def = layer_def_root->getChild(name.c_str());
	if( !layer_def ) {
		if( name[2] == '-' ) {
			string base_name = name.substr(0,2);
			layer_def = layer_def_root->getChild(base_name.c_str());
		}
		if( !layer_def ) {
			thesky->get_cloud_layer(iLayer)->set_enable3dClouds(false);
			return;
		}
	}

	// At this point, we know we've got some 3D clouds to generate.
	thesky->get_cloud_layer(iLayer)->set_enable3dClouds(true);

	double grid_z_rand = layer_def->getDoubleValue("grid-z-rand");

	for(int i = 0; i < layer_def->nChildren() ; i++) {
		SGPropertyNode *acloud = layer_def->getChild(i);
		if( strcmp(acloud->getName(), "cloud") == 0) {
			string cloud_name = acloud->getStringValue("name");
			tCloudVariety[CloudVarietyCount].name = cloud_name;
			double count = acloud->getDoubleValue("count", 1.0);
			tCloudVariety[CloudVarietyCount].count = count;
			int variety = 0;
			cloud_name = cloud_name + "-%d";
			char variety_name[50];
			do {
				variety++;
				snprintf(variety_name, sizeof(variety_name) - 1, cloud_name.c_str(), variety);
			} while( box_def_root->getChild(variety_name, 0, false) );

			totalCount += count;
			if( CloudVarietyCount < 20 )
				CloudVarietyCount++;
		}
	}
	totalCount = 1.0 / totalCount;

        // Determine how much cloud coverage we need in m^2.
        double cov = coverage * SGCloudField::fieldSize * SGCloudField::fieldSize;

        while (cov > 0.0f) {
		double choice = sg_random();
    
		for(int i = 0; i < CloudVarietyCount ; i ++) {
			choice -= tCloudVariety[i].count * totalCount;
			if( choice <= 0.0 ) {
				cov -= buildCloud(cloud_def_root,
						box_def_root,
						tCloudVariety[i].name,
      						grid_z_rand,
						layer);
				break;
			}
		}
		
        }

	// Now we've built any clouds, enable them and set the density (coverage)
	//layer->setCoverage(coverage);
	//layer->applyCoverage();
	thesky->get_cloud_layer(iLayer)->set_enable3dClouds(clouds_3d_enabled);
}

void FGClouds::buildCloudLayers(void) {
	SGPropertyNode *metar_root = fgGetNode("/environment", true);

	//double wind_speed_kt	 = metar_root->getDoubleValue("wind-speed-kt");
	double temperature_degc  = metar_root->getDoubleValue("temperature-sea-level-degc");
	double dewpoint_degc	 = metar_root->getDoubleValue("dewpoint-sea-level-degc");
	double pressure_mb		= metar_root->getDoubleValue("pressure-sea-level-inhg") * SG_INHG_TO_PA / 100.0;

	double dewp = pow(10.0, 7.5 * dewpoint_degc / (237.7 + dewpoint_degc));
	double temp = pow(10.0, 7.5 * temperature_degc / (237.7 + temperature_degc));
	double rel_humidity = dewp * 100 / temp;

	// formule d'Epsy, base d'un cumulus
	double cumulus_base = 122.0 * (temperature_degc - dewpoint_degc);
	double stratus_base = 100.0 * (100.0 - rel_humidity) * SG_FEET_TO_METER;

	for(int iLayer = 0 ; iLayer < thesky->get_cloud_layer_count(); iLayer++) {
		SGPropertyNode *cloud_root = fgGetNode("/environment/clouds/layer", iLayer, true);

		double alt_ft = cloud_root->getDoubleValue("elevation-ft");
		double alt_m = alt_ft * SG_FEET_TO_METER;
                string coverage = cloud_root->getStringValue("coverage");
                
		double coverage_norm = 0.0;
		if( coverage == "few" )
			coverage_norm = 2.0/8.0;	// <1-2
		else if( coverage == "scattered" )
			coverage_norm = 4.0/8.0;	// 3-4
		else if( coverage == "broken" )
			coverage_norm = 6.0/8.0;	// 5-7
		else if( coverage == "overcast" )
			coverage_norm = 8.0/8.0;	// 8

		string layer_type = "nn";
		if( coverage == "cirrus" ) {
			layer_type = "ci";
		} else if( alt_ft > 16500 ) {
//			layer_type = "ci|cs|cc";
			layer_type = "ci";
		} else if( alt_ft > 6500 ) {
//			layer_type = "as|ac|ns";
			layer_type = "ac";
			if( pressure_mb < 1005.0 && coverage_norm >= 0.5 )
				layer_type = "ns";
		} else {
//			layer_type = "st|cu|cb|sc";
			if( cumulus_base * 0.80 < alt_m && cumulus_base * 1.20 > alt_m ) {
				// +/- 20% from cumulus probable base
				layer_type = "cu";
			} else if( stratus_base * 0.80 < alt_m && stratus_base * 1.40 > alt_m ) {
				// +/- 20% from stratus probable base
				layer_type = "st";
			} else {
				// above formulae is far from perfect
				if ( alt_ft < 2000 )
					layer_type = "st";
				else if( alt_ft < 4500 )
					layer_type = "cu";
				else
					layer_type = "sc";
			}
		}
                
		buildLayer(iLayer, layer_type, alt_m, coverage_norm);
	}
}

void FGClouds::set_3dClouds(bool enable)
{
  if (enable != clouds_3d_enabled) {
    clouds_3d_enabled = enable;
    buildCloudLayers();
  }
}

bool FGClouds::get_3dClouds() const {
  return clouds_3d_enabled;
}
  
