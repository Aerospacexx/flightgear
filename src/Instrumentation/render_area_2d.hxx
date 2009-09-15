// RenderArea2D.hxx - a class to manage 2D polygon-based drawing
//                    for a complex instrument (eg. GPS).
//
// Written by David Luff, started 2005.
//
// Copyright (C) 2005 - David C Luff - david.luff@nottingham.ac.uk
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

#ifndef _RENDER_AREA_2D
#define _RENDER_AREA_2D

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <osg/ref_ptr>
#include <osg/State>
#include <osg/StateSet>

#include <plib/sg.h>
#include <simgear/compiler.h>

#include <vector>

using std::vector;

enum RA2DDrawingType {
	RA2D_LINE,
	RA2D_QUAD,
	RA2D_PIXEL
};

struct RA2DPrimitive {
	RA2DPrimitive();

	RA2DDrawingType type;
	int x1, y1, x2, y2;
	bool invert;
	bool debug;
};

class RenderArea2D {
	
public:
	RenderArea2D(int logx, int logy, int sizex, int sizey, int posx, int posy);
	~RenderArea2D();
	
	void draw(osg::State& state);
	
	void SetPixelColor(const float* rgba);
	void SetBackgroundColor(const float* rgba);
	void SetPosition(int posx, int posy);
	void SetLogicalSize(int logx, int logy);
	void SetActualSize(int sizex, int sizey);
	
	// Set clipping region in logical units
	void SetClipRegion(int x1, int y1, int x2, int y2);
	// Set clip region to be the same as the rendered area (default)
	void ResetClipRegion();
	
	// Drawing specified in logical units
	void DrawLine(int x1, int y1, int x2, int y2);
	void DrawQuad(int x1, int y1, int x2, int y2, bool invert = false);
	void DrawBackground();
	// Draw a pixel specified by *logical* position
	void DrawPixel(int x, int y, bool invert = false);
	
	// The old drawing functions have been renamed in order to buffer the drawing for FG
	//
	// Drawing specified in logical units
	void oldDrawLine(int x1, int y1, int x2, int y2);
	void oldDrawQuad(int x1, int y1, int x2, int y2, bool invert = false);
	void oldDrawBackground();
	// Draw a pixel specified by *logical* position
	void oldDrawPixel(int x, int y, bool invert = false);
	
	// Flush the buffer pipeline
	void Flush();
	
	// Turn debugging on or off.
	inline void SetDebugging(bool b) { _ra2d_debug = b; }

private:
	int _logx, _logy;	// logical size of rendered area
	int _sizex, _sizey;		// Actual size of rendered area
	int _posx, _posy;	// Position of rendered area
	int _clipx1, _clipx2, _clipy1, _clipy2;	// Current clipping region in *logical* units
	
	float _backgroundColor[4];
	float _pixelColor[4];
	
	// Actual drawing routines copied from Atlas
	void doSetColor( const float *rgb );
	void doDrawQuad( const sgVec2 *p, const sgVec3 *normals );
	void doDrawQuad( const sgVec2 *p, const sgVec3 *normals, const sgVec4 *color );
	
	vector<RA2DPrimitive> drawing_list;
	
	// Control whether to output debugging output
	bool _ra2d_debug;
};

#endif
