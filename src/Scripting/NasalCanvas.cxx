// NasalCanvas.cxx -- expose Canvas classes to Nasal
//
// Written by James Turner, started 2012.
//
// Copyright (C) 2012 James Turner
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
#  include "config.h"
#endif

#include "NasalCanvas.hxx"
#include <Canvas/canvas_mgr.hxx>
#include <Main/globals.hxx>
#include <Scripting/NasalSys.hxx>

#include <osgGA/GUIEventAdapter>

#include <simgear/sg_inlines.h>

#include <simgear/canvas/Canvas.hxx>
#include <simgear/canvas/elements/CanvasElement.hxx>
#include <simgear/canvas/elements/CanvasText.hxx>
#include <simgear/canvas/MouseEvent.hxx>

#include <simgear/nasal/cppbind/from_nasal.hxx>
#include <simgear/nasal/cppbind/to_nasal.hxx>
#include <simgear/nasal/cppbind/NasalHash.hxx>
#include <simgear/nasal/cppbind/Ghost.hxx>

extern naRef propNodeGhostCreate(naContext c, SGPropertyNode* n);

namespace sc = simgear::canvas;

template<class Element>
naRef elementGetNode(naContext c, Element& element)
{
  return propNodeGhostCreate(c, element.getProps());
}

typedef nasal::Ghost<sc::EventPtr> NasalEvent;
typedef nasal::Ghost<sc::MouseEventPtr> NasalMouseEvent;
typedef nasal::Ghost<sc::CanvasPtr> NasalCanvas;
typedef nasal::Ghost<sc::ElementPtr> NasalElement;
typedef nasal::Ghost<sc::GroupPtr> NasalGroup;
typedef nasal::Ghost<sc::TextPtr> NasalText;

SGPropertyNode* from_nasal_helper(naContext c, naRef ref, SGPropertyNode**)
{
  SGPropertyNode* props = ghostToPropNode(ref);
  if( !props )
    naRuntimeError(c, "Not a SGPropertyNode ghost.");

  return props;
}

CanvasMgr& requireCanvasMgr(naContext c)
{
  CanvasMgr* canvas_mgr =
    static_cast<CanvasMgr*>(globals->get_subsystem("Canvas"));
  if( !canvas_mgr )
    naRuntimeError(c, "Failed to get Canvas subsystem");

  return *canvas_mgr;
}

/**
 * Create new Canvas and get ghost for it.
 */
static naRef f_createCanvas(naContext c, naRef me, int argc, naRef* args)
{
  return NasalCanvas::create(c, requireCanvasMgr(c).createCanvas());
}

/**
 * Get ghost for existing Canvas.
 */
static naRef f_getCanvas(naContext c, naRef me, int argc, naRef* args)
{
  nasal::CallContext ctx(c, argc, args);
  SGPropertyNode& props = *ctx.requireArg<SGPropertyNode*>(0);
  CanvasMgr& canvas_mgr = requireCanvasMgr(c);

  sc::CanvasPtr canvas;
  if( canvas_mgr.getPropertyRoot() == props.getParent() )
  {
    // get a canvas specified by its root node
    canvas = canvas_mgr.getCanvas( props.getIndex() );
    if( !canvas || canvas->getProps() != &props )
      return naNil();
  }
  else
  {
    // get a canvas by name
    if( props.hasValue("name") )
      canvas = canvas_mgr.getCanvas( props.getStringValue("name") );
    else if( props.hasValue("index") )
      canvas = canvas_mgr.getCanvas( props.getIntValue("index") );
  }

  return NasalCanvas::create(c, canvas);
}

naRef f_canvasCreateGroup(sc::Canvas& canvas, const nasal::CallContext& ctx)
{
  return NasalGroup::create
  (
    ctx.c,
    canvas.createGroup( ctx.getArg<std::string>(0) )
  );
}

naRef f_elementGetTransformedBounds(sc::Element& el, const nasal::CallContext& ctx)
{
  osg::BoundingBox bb = el.getTransformedBounds( osg::Matrix::identity() );

  std::vector<float> bb_vec(4);
  bb_vec[0] = bb._min.x();
  bb_vec[1] = bb._min.y();
  bb_vec[2] = bb._max.x();
  bb_vec[3] = bb._max.y();

  return nasal::to_nasal(ctx.c, bb_vec);
}

naRef f_groupCreateChild(sc::Group& group, const nasal::CallContext& ctx)
{
  return NasalElement::create
  (
    ctx.c,
    group.createChild( ctx.requireArg<std::string>(0),
                       ctx.getArg<std::string>(1) )
  );
}

naRef f_groupGetChild(sc::Group& group, const nasal::CallContext& ctx)
{
  return NasalElement::create
  (
    ctx.c,
    group.getChild( ctx.requireArg<SGPropertyNode*>(0) )
  );
}

naRef f_groupGetElementById(sc::Group& group, const nasal::CallContext& ctx)
{
  return NasalElement::create
  (
    ctx.c,
    group.getElementById( ctx.requireArg<std::string>(0) )
  );
}

naRef f_textGetNearestCursor(sc::Text& text, const nasal::CallContext& ctx)
{
  return nasal::to_nasal
  (
    ctx.c,
    text.getNearestCursor( ctx.requireArg<osg::Vec2>(0) )
  );
}

naRef f_eventGetTarget(naContext c, sc::Event& event)
{
  return NasalElement::create(c, event.getTarget().lock());
}

// TODO allow directly exposing functions without parameters and return type
naRef f_eventStopPropagation(sc::Event& event, const nasal::CallContext& ctx)
{
  if( ctx.argc != 0 )
    naRuntimeError(ctx.c, "Event::stopPropagation no argument expected");
  event.stopPropagation();
  return naNil();
}

naRef initNasalCanvas(naRef globals, naContext c, naRef gcSave)
{
  NasalEvent::init("canvas.Event")
    .member("type", &sc::Event::getTypeString)
    .member("target", &f_eventGetTarget)
    .method_func<&f_eventStopPropagation>("stopPropagation");
  NasalMouseEvent::init("canvas.MouseEvent")
    .bases<NasalEvent>()
    .member("screenX", &sc::MouseEvent::getScreenX)
    .member("screenY", &sc::MouseEvent::getScreenY)
    .member("clientX", &sc::MouseEvent::getClientX)
    .member("clientY", &sc::MouseEvent::getClientY)
    .member("deltaX", &sc::MouseEvent::getDeltaX)
    .member("deltaY", &sc::MouseEvent::getDeltaY)
    .member("click_count", &sc::MouseEvent::getCurrentClickCount);
  NasalCanvas::init("Canvas")
    .member("_node_ghost", &elementGetNode<sc::Canvas>)
    .member("size_x", &sc::Canvas::getSizeX)
    .member("size_y", &sc::Canvas::getSizeY)
    .method_func<&f_canvasCreateGroup>("_createGroup")
    .method<&sc::Canvas::addEventListener>("addEventListener");
  NasalElement::init("canvas.Element")
    .member("_node_ghost", &elementGetNode<sc::Element>)
    .method<&sc::Element::addEventListener>("addEventListener")
    .method_func<&f_elementGetTransformedBounds>("getTransformedBounds");
  NasalGroup::init("canvas.Group")
    .bases<NasalElement>()
    .method_func<&f_groupCreateChild>("_createChild")
    .method_func<&f_groupGetChild>("_getChild")
    .method_func<&f_groupGetElementById>("_getElementById");
  NasalText::init("canvas.Text")
    .bases<NasalElement>()
    .method_func<&f_textGetNearestCursor>("getNearestCursor");

  nasal::Hash globals_module(globals, c),
              canvas_module = globals_module.createHash("canvas");

  canvas_module.set("_newCanvasGhost", f_createCanvas);
  canvas_module.set("_getCanvasGhost", f_getCanvas);

  return naNil();
}
