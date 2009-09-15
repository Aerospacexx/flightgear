// Copyright (C) 2008  Tim Moore
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

#include "WindowBuilder.hxx"

#include "WindowSystemAdapter.hxx"
#include "fg_props.hxx"

#include <sstream>

using namespace std;
using namespace osg;

namespace flightgear
{
string makeName(const string& prefix, int num)
{
    stringstream stream;
    stream << prefix << num;
    return stream.str();
}

ref_ptr<WindowBuilder> WindowBuilder::windowBuilder;

const string WindowBuilder::defaultWindowName("FlightGear");

void WindowBuilder::initWindowBuilder(bool stencil)
{
    windowBuilder = new WindowBuilder(stencil);
}

WindowBuilder::WindowBuilder(bool stencil) : defaultCounter(0)
{
    defaultTraits = makeDefaultTraits(stencil);
}

GraphicsContext::Traits*
WindowBuilder::makeDefaultTraits(bool stencil)
{
    GraphicsContext::WindowingSystemInterface* wsi
        = osg::GraphicsContext::getWindowingSystemInterface();
    int w = fgGetInt("/sim/startup/xsize");
    int h = fgGetInt("/sim/startup/ysize");
    int bpp = fgGetInt("/sim/rendering/bits-per-pixel");
    bool alpha = fgGetBool("/sim/rendering/clouds3d-enable");
    bool fullscreen = fgGetBool("/sim/startup/fullscreen");

    GraphicsContext::Traits* traits = new osg::GraphicsContext::Traits;
    traits->readDISPLAY();
    int cbits = (bpp <= 16) ?  5 :  8;
    int zbits = (bpp <= 16) ? 16 : 24;
    traits->red = traits->green = traits->blue = cbits;
    traits->depth = zbits;
    if (alpha)
	traits->alpha = 8;
    if (stencil)
	traits->stencil = 8;
    traits->doubleBuffer = true;
    traits->mipMapGeneration = true;
    traits->windowName = "FlightGear";
    // XXX should check per window too.
    traits->sampleBuffers = fgGetBool("/sim/rendering/multi-sample-buffers", traits->sampleBuffers);
    traits->samples = fgGetInt("/sim/rendering/multi-samples", traits->samples);
    traits->vsync = fgGetBool("/sim/rendering/vsync-enable", traits->vsync);
    if (fullscreen) {
        unsigned width = 0;
        unsigned height = 0;
        wsi->getScreenResolution(*traits, width, height);
	traits->windowDecoration = false;
        traits->width = width;
        traits->height = height;
        traits->supportsResize = false;
    } else {
	traits->windowDecoration = true;
        traits->width = w;
        traits->height = h;
#if defined(WIN32) || defined(__APPLE__)
        // Ugly Hack, why does CW_USEDEFAULT works like phase of the moon?
        // Mac also needs this to show window frame, menubar and Docks
        traits->x = 100;
        traits->y = 100;
#endif
        traits->supportsResize = true;
    }
    return traits;
}
}

namespace
{
// Helper functions that set a value based on a property if it exists,
// returning 1 if the value was set.

inline int setFromProperty(string& place, const SGPropertyNode* node,
                            const char* name)
{
    const SGPropertyNode* valNode = node->getNode(name);
    if (valNode) {
        place = valNode->getStringValue();
        return 1;
    }
    return 0;
}

inline int setFromProperty(int& place, const SGPropertyNode* node,
                            const char* name)
{
    const SGPropertyNode* valNode = node->getNode(name);
    if (valNode) {
        place = valNode->getIntValue();
        return 1;
    }
    return 0;
}

inline int setFromProperty(bool& place, const SGPropertyNode* node,
                            const char* name)
{
    const SGPropertyNode* valNode = node->getNode(name);
    if (valNode) {
        place = valNode->getBoolValue();
        return 1;
    }
    return 0;
}
}

namespace flightgear
{
GraphicsWindow* WindowBuilder::buildWindow(const SGPropertyNode* winNode)
{
    GraphicsContext::WindowingSystemInterface* wsi
        = osg::GraphicsContext::getWindowingSystemInterface();
    WindowSystemAdapter* wsa = WindowSystemAdapter::getWSA();
    string windowName;
    if (winNode->hasChild("window-name"))
        windowName = winNode->getStringValue("window-name");
    else if (winNode->hasChild("name")) 
        windowName = winNode->getStringValue("name");
    GraphicsWindow* result = 0;
    if (!windowName.empty()) {
        result = wsa->findWindow(windowName);
        if (result)
            return result;
    }
    GraphicsContext::Traits* traits
        = new GraphicsContext::Traits(*defaultTraits);
    int traitsSet = setFromProperty(traits->hostName, winNode, "host-name");
    traitsSet |= setFromProperty(traits->displayNum, winNode, "display");
    traitsSet |= setFromProperty(traits->screenNum, winNode, "screen");
    const SGPropertyNode* fullscreenNode = winNode->getNode("fullscreen");
    if (fullscreenNode && fullscreenNode->getBoolValue()) {
        unsigned width = 0;
        unsigned height = 0;
        wsi->getScreenResolution(*traits, width, height);
        traits->windowDecoration = false;
        traits->width = width;
        traits->height = height;
        traits->supportsResize = false;
        traitsSet = 1;
    } else {
        int resizable = 0;
        resizable |= setFromProperty(traits->windowDecoration, winNode,
                                     "decoration");
        resizable |= setFromProperty(traits->width, winNode, "width");
        resizable |= setFromProperty(traits->height, winNode, "height");
        if (resizable) {
            traits->supportsResize = true;
            traitsSet = 1;
        }
        // Otherwise use default values.
    }
    traitsSet |= setFromProperty(traits->x, winNode, "x");
    traitsSet |= setFromProperty(traits->y, winNode, "y");
    if (!windowName.empty() && windowName != traits->windowName) {
        traits->windowName = windowName;
        traitsSet = 1;
    } else if (traitsSet) {
        traits->windowName = makeName("FlightGear", defaultCounter++);
    }
    bool drawGUI = false;
    traitsSet |= setFromProperty(drawGUI, winNode, "gui");
    if (traitsSet) {
        GraphicsContext* gc = GraphicsContext::createGraphicsContext(traits);
        if (gc) {
            gc->realize();
            GraphicsWindow* window = WindowSystemAdapter::getWSA()
                ->registerWindow(gc, traits->windowName);
            if (drawGUI)
                window->flags |= GraphicsWindow::GUI;
            return window;
        } else {
            return 0;
        }
    } else {
        // XXX What if the window has no traits, but does have a name?
        // We should create a "default window" registered with that name.
        return getDefaultWindow();
    }
}

GraphicsWindow* WindowBuilder::getDefaultWindow()
{
    GraphicsWindow* defaultWindow
        = WindowSystemAdapter::getWSA()->findWindow(defaultWindowName);
    if (defaultWindow)
        return defaultWindow;
    GraphicsContext::Traits* traits
        = new GraphicsContext::Traits(*defaultTraits);
    traits->windowName = "FlightGear";
    GraphicsContext* gc = GraphicsContext::createGraphicsContext(traits);
    if (gc) {
        gc->realize();
        defaultWindow = WindowSystemAdapter::getWSA()
            ->registerWindow(gc, defaultWindowName);
        return defaultWindow;
    } else {
        return 0;
    }
}
}
