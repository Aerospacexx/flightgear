// renderer.cxx -- top level sim routines
//
// Written by Curtis Olson, started May 1997.
// This file contains parts of main.cxx prior to october 2004
//
// Copyright (C) 1997 - 2002  Curtis L. Olson  - http://www.flightgear.org/~curt
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

#ifdef HAVE_WINDOWS_H
#  include <windows.h>
#endif

#include <simgear/compiler.h>

#include <osg/ref_ptr>
#include <osg/AlphaFunc>
#include <osg/BlendFunc>
#include <osg/Camera>
#include <osg/CullFace>
#include <osg/Depth>
#include <osg/Fog>
#include <osg/Group>
#include <osg/Hint>
#include <osg/Light>
#include <osg/LightModel>
#include <osg/LightSource>
#include <osg/Material>
#include <osg/Math>
#include <osg/NodeCallback>
#include <osg/Notify>
#include <osg/PolygonMode>
#include <osg/PolygonOffset>
#include <osg/Version>
#include <osg/TexEnv>

#include <osgUtil/LineSegmentIntersector>

#include <osg/io_utils>
#include <osgDB/WriteFile>

#include <simgear/math/SGMath.hxx>
#include <simgear/screen/extensions.hxx>
#include <simgear/scene/material/matlib.hxx>
#include <simgear/scene/model/animation.hxx>
#include <simgear/scene/model/placement.hxx>
#include <simgear/scene/sky/sky.hxx>
#include <simgear/scene/util/SGUpdateVisitor.hxx>
#include <simgear/scene/util/RenderConstants.hxx>
#include <simgear/scene/util/SGSceneUserData.hxx>
#include <simgear/scene/tgdb/GroundLightManager.hxx>
#include <simgear/scene/tgdb/pt_lights.hxx>
#include <simgear/props/props.hxx>
#include <simgear/timing/sg_time.hxx>
#include <simgear/ephemeris/ephemeris.hxx>
#include <simgear/math/sg_random.h>
#ifdef FG_JPEG_SERVER
#include <simgear/screen/jpgfactory.hxx>
#endif

#include <simgear/environment/visual_enviro.hxx>

#include <Time/light.hxx>
#include <Time/light.hxx>
#include <Aircraft/aircraft.hxx>
#include <Cockpit/panel.hxx>
#include <Cockpit/cockpit.hxx>
#include <Cockpit/hud.hxx>
#include <Model/panelnode.hxx>
#include <Model/modelmgr.hxx>
#include <Model/acmodel.hxx>
#include <Scenery/scenery.hxx>
#include <Scenery/redout.hxx>
#include <Scenery/tilemgr.hxx>
#include <GUI/new_gui.hxx>
#include <Instrumentation/instrument_mgr.hxx>
#include <Instrumentation/HUD/HUD.hxx>
#include <Environment/precipitation_mgr.hxx>

#include <Include/general.hxx>
#include "splash.hxx"
#include "renderer.hxx"
#include "main.hxx"
#include "CameraGroup.hxx"
#include "FGEventHandler.hxx"
#include <Main/viewer.hxx>

using namespace osg;
using namespace simgear;
using namespace flightgear;

class FGHintUpdateCallback : public osg::StateAttribute::Callback {
public:
  FGHintUpdateCallback(const char* configNode) :
    mConfigNode(fgGetNode(configNode, true))
  { }
  virtual void operator()(osg::StateAttribute* stateAttribute,
                          osg::NodeVisitor*)
  {
    assert(dynamic_cast<osg::Hint*>(stateAttribute));
    osg::Hint* hint = static_cast<osg::Hint*>(stateAttribute);

    const char* value = mConfigNode->getStringValue();
    if (!value)
      hint->setMode(GL_DONT_CARE);
    else if (0 == strcmp(value, "nicest"))
      hint->setMode(GL_NICEST);
    else if (0 == strcmp(value, "fastest"))
      hint->setMode(GL_FASTEST);
    else
      hint->setMode(GL_DONT_CARE);
  }
private:
  SGSharedPtr<SGPropertyNode> mConfigNode;
};


class SGPuDrawable : public osg::Drawable {
public:
  SGPuDrawable()
  {
    // Dynamic stuff, do not store geometry
    setUseDisplayList(false);

    osg::StateSet* stateSet = getOrCreateStateSet();
    stateSet->setRenderBinDetails(1001, "RenderBin");
    // speed optimization?
    stateSet->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
    // We can do translucent menus, so why not. :-)
    stateSet->setAttribute(new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA));
    stateSet->setMode(GL_BLEND, osg::StateAttribute::ON);
    stateSet->setTextureMode(0, GL_TEXTURE_2D, osg::StateAttribute::OFF);

    stateSet->setTextureAttribute(0, new osg::TexEnv(osg::TexEnv::MODULATE));

    stateSet->setMode(GL_FOG, osg::StateAttribute::OFF);
    stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
  }
  virtual void drawImplementation(osg::RenderInfo& renderInfo) const
  { drawImplementation(*renderInfo.getState()); }
  void drawImplementation(osg::State& state) const
  {
    state.setActiveTextureUnit(0);
    state.setClientActiveTextureUnit(0);

    state.disableAllVertexArrays();

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib(~0u);

    puDisplay();

    glPopClientAttrib();
    glPopAttrib();
  }

  virtual osg::Object* cloneType() const { return new SGPuDrawable; }
  virtual osg::Object* clone(const osg::CopyOp&) const { return new SGPuDrawable; }
  
private:
};

class SGHUDAndPanelDrawable : public osg::Drawable {
public:
  SGHUDAndPanelDrawable()
  {
    // Dynamic stuff, do not store geometry
    setUseDisplayList(false);

    osg::StateSet* stateSet = getOrCreateStateSet();
    stateSet->setRenderBinDetails(1000, "RenderBin");

    // speed optimization?
    stateSet->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
    stateSet->setAttribute(new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA));
    stateSet->setMode(GL_BLEND, osg::StateAttribute::ON);
    stateSet->setMode(GL_FOG, osg::StateAttribute::OFF);
    stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);

    stateSet->setTextureAttribute(0, new osg::TexEnv(osg::TexEnv::MODULATE));
  }
  virtual void drawImplementation(osg::RenderInfo& renderInfo) const
  { drawImplementation(*renderInfo.getState()); }
  void drawImplementation(osg::State& state) const
  {
    state.setActiveTextureUnit(0);
    state.setClientActiveTextureUnit(0);
    state.disableAllVertexArrays();

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib(~0u);

    fgCockpitUpdate(&state);

    FGInstrumentMgr *instr = static_cast<FGInstrumentMgr*>(globals->get_subsystem("instrumentation"));
    HUD *hud = static_cast<HUD*>(instr->get_subsystem("hud"));
    hud->draw(state);

    // update the panel subsystem
    if ( globals->get_current_panel() != NULL )
        globals->get_current_panel()->update(state);
    // We don't need a state here - can be safely removed when we can pick
    // correctly
    fgUpdate3DPanels();

    glPopClientAttrib();
    glPopAttrib();

  }

  virtual osg::Object* cloneType() const { return new SGHUDAndPanelDrawable; }
  virtual osg::Object* clone(const osg::CopyOp&) const { return new SGHUDAndPanelDrawable; }
  
private:
};

class FGLightSourceUpdateCallback : public osg::NodeCallback {
public:
  virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
  {
    assert(dynamic_cast<osg::LightSource*>(node));
    osg::LightSource* lightSource = static_cast<osg::LightSource*>(node);
    osg::Light* light = lightSource->getLight();
    
    FGLight *l = static_cast<FGLight*>(globals->get_subsystem("lighting"));
    light->setAmbient(toOsg(l->scene_ambient()));
    light->setDiffuse(toOsg(l->scene_diffuse()));
    light->setSpecular(toOsg(l->scene_specular()));
    osg::Vec4f position(l->sun_vec()[0], l->sun_vec()[1], l->sun_vec()[2], 0);
    light->setPosition(position);
    osg::Vec3f direction(l->sun_vec()[0], l->sun_vec()[1], l->sun_vec()[2]);
    light->setDirection(direction);
    light->setSpotExponent(0);
    light->setSpotCutoff(180);
    light->setConstantAttenuation(1);
    light->setLinearAttenuation(0);
    light->setQuadraticAttenuation(0);

    traverse(node, nv);
  }
};

class FGWireFrameModeUpdateCallback : public osg::StateAttribute::Callback {
public:
  FGWireFrameModeUpdateCallback() :
    mWireframe(fgGetNode("/sim/rendering/wireframe"))
  { }
  virtual void operator()(osg::StateAttribute* stateAttribute,
                          osg::NodeVisitor*)
  {
    assert(dynamic_cast<osg::PolygonMode*>(stateAttribute));
    osg::PolygonMode* polygonMode;
    polygonMode = static_cast<osg::PolygonMode*>(stateAttribute);

    if (mWireframe->getBoolValue())
      polygonMode->setMode(osg::PolygonMode::FRONT_AND_BACK,
                           osg::PolygonMode::LINE);
    else
      polygonMode->setMode(osg::PolygonMode::FRONT_AND_BACK,
                           osg::PolygonMode::FILL);
  }
private:
  SGSharedPtr<SGPropertyNode> mWireframe;
};

class FGLightModelUpdateCallback : public osg::StateAttribute::Callback {
public:
  FGLightModelUpdateCallback() :
    mHighlights(fgGetNode("/sim/rendering/specular-highlight"))
  { }
  virtual void operator()(osg::StateAttribute* stateAttribute,
                          osg::NodeVisitor*)
  {
    assert(dynamic_cast<osg::LightModel*>(stateAttribute));
    osg::LightModel* lightModel;
    lightModel = static_cast<osg::LightModel*>(stateAttribute);

#if 0
    FGLight *l = static_cast<FGLight*>(globals->get_subsystem("lighting"));
    lightModel->setAmbientIntensity(toOsg(l->scene_ambient());
#else
    lightModel->setAmbientIntensity(osg::Vec4(0, 0, 0, 1));
#endif
    lightModel->setTwoSided(true);
    lightModel->setLocalViewer(false);

    if (mHighlights->getBoolValue()) {
      lightModel->setColorControl(osg::LightModel::SEPARATE_SPECULAR_COLOR);
    } else {
      lightModel->setColorControl(osg::LightModel::SINGLE_COLOR);
    }
  }
private:
  SGSharedPtr<SGPropertyNode> mHighlights;
};

class FGFogEnableUpdateCallback : public osg::StateSet::Callback {
public:
  FGFogEnableUpdateCallback() :
    mFogEnabled(fgGetNode("/sim/rendering/fog"))
  { }
  virtual void operator()(osg::StateSet* stateSet, osg::NodeVisitor*)
  {
    if (strcmp(mFogEnabled->getStringValue(), "disabled") == 0) {
      stateSet->setMode(GL_FOG, osg::StateAttribute::OFF);
    } else {
      stateSet->setMode(GL_FOG, osg::StateAttribute::ON);
    }
  }
private:
  SGSharedPtr<SGPropertyNode> mFogEnabled;
};

class FGFogUpdateCallback : public osg::StateAttribute::Callback {
public:
  virtual void operator () (osg::StateAttribute* sa, osg::NodeVisitor* nv)
  {
    assert(dynamic_cast<SGUpdateVisitor*>(nv));
    assert(dynamic_cast<osg::Fog*>(sa));
    SGUpdateVisitor* updateVisitor = static_cast<SGUpdateVisitor*>(nv);
    osg::Fog* fog = static_cast<osg::Fog*>(sa);
    fog->setMode(osg::Fog::EXP2);
    fog->setColor(toOsg(updateVisitor->getFogColor()));
    fog->setDensity(updateVisitor->getFogExp2Density());
  }
};

// update callback for the switch node guarding that splash
class FGScenerySwitchCallback : public osg::NodeCallback {
public:
  virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
  {
    assert(dynamic_cast<osg::Switch*>(node));
    osg::Switch* sw = static_cast<osg::Switch*>(node);

    double t = globals->get_sim_time_sec();
    bool enabled = 0 < t;
    sw->setValue(0, enabled);
    if (!enabled)
      return;
    traverse(node, nv);
  }
};

// Sky structures
SGSky *thesky;

static osg::ref_ptr<osg::FrameStamp> mFrameStamp = new osg::FrameStamp;
static osg::ref_ptr<SGUpdateVisitor> mUpdateVisitor= new SGUpdateVisitor;

static osg::ref_ptr<osg::Group> mRealRoot = new osg::Group;

static osg::ref_ptr<osg::Group> mRoot = new osg::Group;

FGRenderer::FGRenderer()
{
#ifdef FG_JPEG_SERVER
   jpgRenderFrame = FGRenderer::update;
#endif
   eventHandler = new FGEventHandler;
}

FGRenderer::~FGRenderer()
{
#ifdef FG_JPEG_SERVER
   jpgRenderFrame = NULL;
#endif
}

// Initialize various GL/view parameters
// XXX This should be called "preinit" or something, as it initializes
// critical parts of the scene graph in addition to the splash screen.
void
FGRenderer::splashinit( void ) {
    osgViewer::Viewer* viewer = globals->get_renderer()->getViewer();
    mRealRoot = dynamic_cast<osg::Group*>(viewer->getSceneData());
    mRealRoot->addChild(fgCreateSplashNode());
    mFrameStamp = viewer->getFrameStamp();
    // Scene doesn't seem to pass the frame stamp to the update
    // visitor automatically.
    mUpdateVisitor->setFrameStamp(mFrameStamp.get());
    viewer->setUpdateVisitor(mUpdateVisitor.get());
}

void
FGRenderer::init( void )
{
    osgViewer::Viewer* viewer = globals->get_renderer()->getViewer();
    osg::initNotifyLevel();

    // The number of polygon-offset "units" to place between layers.  In
    // principle, one is supposed to be enough.  In practice, I find that
    // my hardware/driver requires many more.
    osg::PolygonOffset::setUnitsMultiplier(1);
    osg::PolygonOffset::setFactorMultiplier(1);

    // Go full screen if requested ...
    if ( fgGetBool("/sim/startup/fullscreen") )
        fgOSFullScreen();

    viewer->getCamera()
        ->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
    
    osg::StateSet* stateSet = mRoot->getOrCreateStateSet();

    stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    
    stateSet->setAttribute(new osg::Depth(osg::Depth::LESS));
    stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);

    stateSet->setAttribute(new osg::AlphaFunc(osg::AlphaFunc::GREATER, 0.01));
    stateSet->setMode(GL_ALPHA_TEST, osg::StateAttribute::OFF);
    stateSet->setAttribute(new osg::BlendFunc);
    stateSet->setMode(GL_BLEND, osg::StateAttribute::OFF);

    stateSet->setMode(GL_FOG, osg::StateAttribute::OFF);
    
    // this will be set below
    stateSet->setMode(GL_NORMALIZE, osg::StateAttribute::OFF);

    osg::Material* material = new osg::Material;
    stateSet->setAttribute(material);
    
    stateSet->setTextureAttribute(0, new osg::TexEnv);
    stateSet->setTextureMode(0, GL_TEXTURE_2D, osg::StateAttribute::OFF);

    osg::Hint* hint = new osg::Hint(GL_FOG_HINT, GL_DONT_CARE);
    hint->setUpdateCallback(new FGHintUpdateCallback("/sim/rendering/fog"));
    stateSet->setAttribute(hint);
    hint = new osg::Hint(GL_POLYGON_SMOOTH_HINT, GL_DONT_CARE);
    hint->setUpdateCallback(new FGHintUpdateCallback("/sim/rendering/polygon-smooth"));
    stateSet->setAttribute(hint);
    hint = new osg::Hint(GL_LINE_SMOOTH_HINT, GL_DONT_CARE);
    hint->setUpdateCallback(new FGHintUpdateCallback("/sim/rendering/line-smooth"));
    stateSet->setAttribute(hint);
    hint = new osg::Hint(GL_POINT_SMOOTH_HINT, GL_DONT_CARE);
    hint->setUpdateCallback(new FGHintUpdateCallback("/sim/rendering/point-smooth"));
    stateSet->setAttribute(hint);
    hint = new osg::Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_DONT_CARE);
    hint->setUpdateCallback(new FGHintUpdateCallback("/sim/rendering/perspective-correction"));
    stateSet->setAttribute(hint);

    osg::Group* sceneGroup = new osg::Group;
    sceneGroup->addChild(globals->get_scenery()->get_scene_graph());
    sceneGroup->setNodeMask(~simgear::BACKGROUND_BIT);

    //sceneGroup->addChild(thesky->getCloudRoot());

    stateSet = sceneGroup->getOrCreateStateSet();
    stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);

    // need to update the light on every frame
    osg::LightSource* lightSource = new osg::LightSource;
    lightSource->setUpdateCallback(new FGLightSourceUpdateCallback);
    // relative because of CameraView being just a clever transform node
    lightSource->setReferenceFrame(osg::LightSource::RELATIVE_RF);
    lightSource->setLocalStateSetModes(osg::StateAttribute::ON);
    
    lightSource->addChild(sceneGroup);
    lightSource->addChild(thesky->getPreRoot());
    mRoot->addChild(lightSource);

    stateSet = globals->get_scenery()->get_scene_graph()->getOrCreateStateSet();
    stateSet->setMode(GL_ALPHA_TEST, osg::StateAttribute::ON);
    stateSet->setMode(GL_LIGHTING, osg::StateAttribute::ON);
    stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);

    // enable disable specular highlights.
    // is the place where we might plug in an other fragment shader ...
    osg::LightModel* lightModel = new osg::LightModel;
    lightModel->setUpdateCallback(new FGLightModelUpdateCallback);
    stateSet->setAttribute(lightModel);

    // switch to enable wireframe
    osg::PolygonMode* polygonMode = new osg::PolygonMode;
    polygonMode->setUpdateCallback(new FGWireFrameModeUpdateCallback);
    stateSet->setAttributeAndModes(polygonMode);

    // scene fog handling
    osg::Fog* fog = new osg::Fog;
    fog->setUpdateCallback(new FGFogUpdateCallback);
    stateSet->setAttributeAndModes(fog);
    stateSet->setUpdateCallback(new FGFogEnableUpdateCallback);

    // plug in the GUI
    osg::Camera* guiCamera = getGUICamera(CameraGroup::getDefault());
    if (guiCamera) {
        osg::Geode* geode = new osg::Geode;
        geode->addDrawable(new SGPuDrawable);
        geode->addDrawable(new SGHUDAndPanelDrawable);
        guiCamera->addChild(geode);
    }
    osg::Switch* sw = new osg::Switch;
    sw->setUpdateCallback(new FGScenerySwitchCallback);
    sw->addChild(mRoot.get());
    mRealRoot->addChild(sw);
    mRealRoot->addChild(thesky->getCloudRoot());
    mRealRoot->addChild(FGCreateRedoutNode());
}


// Update all Visuals (redraws anything graphics related)
void
FGRenderer::update( bool refresh_camera_settings ) {
    bool scenery_loaded = fgGetBool("sim/sceneryloaded")
                          || fgGetBool("sim/sceneryloaded-override");
    osgViewer::Viewer* viewer = globals->get_renderer()->getViewer();
    if ( idle_state < 1000 || !scenery_loaded ) {
        fgSetDouble("/sim/startup/splash-alpha", 1.0);

        // Keep resetting sim time while the sim is initializing
        // the splash screen is now in the scenegraph
        globals->set_sim_time_sec( 0.0 );
        return;
    }

    // Fade out the splash screen over the first three seconds.
    double sAlpha = SGMiscd::max(0, (2.5 - globals->get_sim_time_sec()) / 2.5);
    fgSetDouble("/sim/startup/splash-alpha", sAlpha);

    bool skyblend = fgGetBool("/sim/rendering/skyblend");
    bool use_point_sprites = fgGetBool("/sim/rendering/point-sprites");
    bool enhanced_lighting = fgGetBool("/sim/rendering/enhanced-lighting");
    bool distance_attenuation
        = fgGetBool("/sim/rendering/distance-attenuation");
    // OSGFIXME
    SGConfigureDirectionalLights( use_point_sprites, enhanced_lighting,
                                  distance_attenuation );

    FGLight *l = static_cast<FGLight*>(globals->get_subsystem("lighting"));

    // update fog params
    double actual_visibility;
    if (fgGetBool("/environment/clouds/status")) {
        actual_visibility = thesky->get_visibility();
    } else {
        actual_visibility = fgGetDouble("/environment/visibility-m");
    }

    // idle_state is now 1000 meaning we've finished all our
    // initializations and are running the main loop, so this will
    // now work without seg faulting the system.

    FGViewer *current__view = globals->get_current_view();
    // Force update of center dependent values ...
    current__view->set_dirty();

    if ( refresh_camera_settings ) {
        // update view port
        resize( fgGetInt("/sim/startup/xsize"),
                fgGetInt("/sim/startup/ysize") );
    }
    osg::Camera *camera = viewer->getCamera();

    if ( skyblend ) {
	
        if ( fgGetBool("/sim/rendering/textures") ) {
            SGVec4f clearColor(l->adj_fog_color());
            camera->setClearColor(toOsg(clearColor));
        }
    } else {
        SGVec4f clearColor(l->sky_color());
        camera->setClearColor(toOsg(clearColor));
    }

    // update fog params if visibility has changed
    double visibility_meters = fgGetDouble("/environment/visibility-m");
    thesky->set_visibility(visibility_meters);

    thesky->modify_vis( cur_fdm_state->get_Altitude() * SG_FEET_TO_METER,
                        ( global_multi_loop * fgGetInt("/sim/speed-up") )
                        / (double)fgGetInt("/sim/model-hz") );

    // update the sky dome
    if ( skyblend ) {

        // The sun and moon distances are scaled down versions
        // of the actual distance to get both the moon and the sun
        // within the range of the far clip plane.
        // Moon distance:    384,467 kilometers
        // Sun distance: 150,000,000 kilometers

        double sun_horiz_eff, moon_horiz_eff;
        if (fgGetBool("/sim/rendering/horizon-effect")) {
            sun_horiz_eff
                = 0.67 + pow(osg::clampAbove(0.5 + cos(l->get_sun_angle()),
                                             0.0),
                             0.33) / 3.0;
            moon_horiz_eff
                = 0.67 + pow(osg::clampAbove(0.5 + cos(l->get_moon_angle()),
                                             0.0),
                             0.33)/3.0;
        } else {
           sun_horiz_eff = moon_horiz_eff = 1.0;
        }

        SGSkyState sstate;

        SGVec3d viewPos = current__view->getViewPosition();
        sstate.view_pos  = toVec3f(viewPos);
        SGGeod geodViewPos = SGGeod::fromCart(viewPos);
        SGGeod geodZeroViewPos = SGGeod::fromGeodM(geodViewPos, 0);
        sstate.zero_elev = toVec3f(SGVec3d::fromGeod(geodZeroViewPos));
        SGQuatd hlOr = SGQuatd::fromLonLat(geodViewPos);
        sstate.view_up   = toVec3f(hlOr.backTransform(-SGVec3d::e3()));
        sstate.lon       = geodViewPos.getLongitudeRad();
        sstate.lat       = geodViewPos.getLatitudeRad();
        sstate.alt       = geodViewPos.getElevationM();
        sstate.spin      = l->get_sun_rotation();
        sstate.gst       = globals->get_time_params()->getGst();
        sstate.sun_dist  = 50000.0 * sun_horiz_eff;
        sstate.moon_dist = 40000.0 * moon_horiz_eff;
        sstate.sun_angle = l->get_sun_angle();


        /*
         SG_LOG( SG_GENERAL, SG_BULK, "thesky->repaint() sky_color = "
         << l->sky_color()[0] << " "
         << l->sky_color()[1] << " "
         << l->sky_color()[2] << " "
         << l->sky_color()[3] );
        SG_LOG( SG_GENERAL, SG_BULK, "    fog = "
         << l->fog_color()[0] << " "
         << l->fog_color()[1] << " "
         << l->fog_color()[2] << " "
         << l->fog_color()[3] );
        SG_LOG( SG_GENERAL, SG_BULK,
                "    sun_angle = " << l->sun_angle
         << "    moon_angle = " << l->moon_angle );
        */

        SGSkyColor scolor;

        scolor.sky_color   = SGVec3f(l->sky_color().data());
        scolor.adj_sky_color = SGVec3f(l->adj_sky_color().data());
        scolor.fog_color   = SGVec3f(l->adj_fog_color().data());
        scolor.cloud_color = SGVec3f(l->cloud_color().data());
        scolor.sun_angle   = l->get_sun_angle();
        scolor.moon_angle  = l->get_moon_angle();

        thesky->reposition( sstate, *globals->get_ephem(), delta_time_sec );
        thesky->repaint( scolor, *globals->get_ephem() );

        /*
        SG_LOG( SG_GENERAL, SG_BULK,
                "thesky->reposition( view_pos = " << view_pos[0] << " "
         << view_pos[1] << " " << view_pos[2] );
        SG_LOG( SG_GENERAL, SG_BULK,
                "    zero_elev = " << zero_elev[0] << " "
         << zero_elev[1] << " " << zero_elev[2]
         << " lon = " << cur_fdm_state->get_Longitude()
         << " lat = " << cur_fdm_state->get_Latitude() );
        SG_LOG( SG_GENERAL, SG_BULK,
                "    sun_rot = " << l->get_sun_rotation
         << " gst = " << SGTime::cur_time_params->getGst() );
        SG_LOG( SG_GENERAL, SG_BULK,
             "    sun ra = " << globals->get_ephem()->getSunRightAscension()
          << " sun dec = " << globals->get_ephem()->getSunDeclination()
          << " moon ra = " << globals->get_ephem()->getMoonRightAscension()
          << " moon dec = " << globals->get_ephem()->getMoonDeclination() );
        */

        //OSGFIXME
//         shadows->setupShadows(
//           current__view->getLongitude_deg(),
//           current__view->getLatitude_deg(),
//           globals->get_time_params()->getGst(),
//           globals->get_ephem()->getSunRightAscension(),
//           globals->get_ephem()->getSunDeclination(),
//           l->get_sun_angle());

    }

//     sgEnviro.setLight(l->adj_fog_color());
//     sgEnviro.startOfFrame(current__view->get_view_pos(), 
//         current__view->get_world_up(),
//         current__view->getLongitude_deg(),
//         current__view->getLatitude_deg(),
//         current__view->getAltitudeASL_ft() * SG_FEET_TO_METER,
//         delta_time_sec);

    // OSGFIXME
//     sgEnviro.drawLightning();

//        double current_view_origin_airspeed_horiz_kt =
//         fgGetDouble("/velocities/airspeed-kt", 0.0)
//                        * cos( fgGetDouble("/orientation/pitch-deg", 0.0)
//                                * SGD_DEGREES_TO_RADIANS);

    // OSGFIXME
//     if( is_internal )
//         shadows->endOfFrame();

    // need to call the update visitor once
    mFrameStamp->setCalendarTime(*globals->get_time_params()->getGmt());
    mUpdateVisitor->setViewData(current__view->getViewPosition(),
                                current__view->getViewOrientation());
    SGVec3f direction(l->sun_vec()[0], l->sun_vec()[1], l->sun_vec()[2]);
    mUpdateVisitor->setLight(direction, l->scene_ambient(),
                             l->scene_diffuse(), l->scene_specular(),
                             l->adj_fog_color(),
                             l->get_sun_angle()*SGD_RADIANS_TO_DEGREES);
    mUpdateVisitor->setVisibility(actual_visibility);
    simgear::GroundLightManager::instance()->update(mUpdateVisitor.get());
    bool hotspots = fgGetBool("/sim/panel-hotspots");
    osg::Node::NodeMask cullMask = ~simgear::LIGHTS_BITS & ~simgear::PICK_BIT;
    cullMask |= simgear::GroundLightManager::instance()
        ->getLightNodeMask(mUpdateVisitor.get());
    if (hotspots)
        cullMask |= simgear::PICK_BIT;
    CameraGroup::getDefault()->setCameraCullMasks(cullMask);
}



// options.cxx needs to see this for toggle_panel()
// Handle new window size or exposure
void
FGRenderer::resize( int width, int height ) {
    int view_h;

    if ( (!fgGetBool("/sim/virtual-cockpit"))
         && fgPanelVisible() && idle_state == 1000 ) {
        view_h = (int)(height * (globals->get_current_panel()->getViewHeight() -
                             globals->get_current_panel()->getYOffset()) / 768.0);
    } else {
        view_h = height;
    }

    static int lastwidth = 0;
    static int lastheight = 0;
    if (width != lastwidth)
        fgSetInt("/sim/startup/xsize", lastwidth = width);
    if (height != lastheight)
        fgSetInt("/sim/startup/ysize", lastheight = height);

    // for all views
    FGViewMgr *viewmgr = globals->get_viewmgr();
    if (viewmgr) {
      for ( int i = 0; i < viewmgr->size(); ++i ) {
        viewmgr->get_view(i)->
          set_aspect_ratio((float)view_h / (float)width);
      }
    }
}

bool
FGRenderer::pick(std::vector<SGSceneryPick>& pickList,
                 const osgGA::GUIEventAdapter* ea)
{
    // wipe out the return ...
    pickList.clear();
    typedef osgUtil::LineSegmentIntersector::Intersections Intersections;
    Intersections intersections;

    if (!computeIntersections(CameraGroup::getDefault(), ea, intersections))
        return false;
    for (Intersections::iterator hit = intersections.begin(),
             e = intersections.end();
         hit != e;
         ++hit) {
        const osg::NodePath& np = hit->nodePath;
        osg::NodePath::const_reverse_iterator npi;
        for (npi = np.rbegin(); npi != np.rend(); ++npi) {
            SGSceneUserData* ud = SGSceneUserData::getSceneUserData(*npi);
            if (!ud)
                continue;
            for (unsigned i = 0; i < ud->getNumPickCallbacks(); ++i) {
                SGPickCallback* pickCallback = ud->getPickCallback(i);
                if (!pickCallback)
                    continue;
                SGSceneryPick sceneryPick;
                sceneryPick.info.local = toSG(hit->getLocalIntersectPoint());
                sceneryPick.info.wgs84 = toSG(hit->getWorldIntersectPoint());
                sceneryPick.callback = pickCallback;
                pickList.push_back(sceneryPick);
            }
        }
    }
    return !pickList.empty();
}

void
FGRenderer::setViewer(osgViewer::Viewer* viewer_)
{
    viewer = viewer_;
}

void
FGRenderer::setEventHandler(FGEventHandler* eventHandler_)
{
    eventHandler = eventHandler_;
}

void
FGRenderer::addCamera(osg::Camera* camera, bool useSceneData)
{
    mRealRoot->addChild(camera);
}

bool
fgDumpSceneGraphToFile(const char* filename)
{
    return osgDB::writeNodeFile(*mRealRoot.get(), filename);
}

bool
fgDumpTerrainBranchToFile(const char* filename)
{
    return osgDB::writeNodeFile( *globals->get_scenery()->get_terrain_branch(),
                                 filename );
}

// For debugging
bool
fgDumpNodeToFile(osg::Node* node, const char* filename)
{
    return osgDB::writeNodeFile(*node, filename);
}
// end of renderer.cxx
    
