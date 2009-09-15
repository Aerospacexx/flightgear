// FGAIEntity - abstract base class an artificial intelligence entity
//
// Written by David Luff, started March 2002.
//
// Copyright (C) 2002  David C. Luff - david.luff@nottingham.ac.uk
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

#ifndef _FG_AIEntity_HXX
#define _FG_AIEntity_HXX

#include <simgear/math/point3d.hxx>
#include <simgear/scene/model/placement.hxx>


/*****************************************************************
*
*  FGAIEntity - this class implements the minimum requirement
*  for any AI entity - a position, an orientation, an associated
*  3D model, and the ability to be moved.  It does nothing useful
*  and all AI entities are expected to be derived from it.
*
******************************************************************/
class FGAIEntity {

public:

	FGAIEntity();
    virtual ~FGAIEntity();
	
	// Set the 3D model to use (Must be called)
  void SetModel(osg::Node* model);

    // Run the internal calculations
    virtual void Update(double dt)=0;
	
    // Send a transmission *TO* the AIEntity.
    // FIXME int code is a hack - eventually this will receive Alexander's coded messages.
    virtual void RegisterTransmission(int code)=0;
	
	inline const Point3D& GetPos() const { return(_pos); }
	
	virtual const string& GetCallsign()=0;
	
protected:

    Point3D _pos;	// WGS84 lat & lon in degrees, elev above sea-level in meters
    double _hdg;		//True heading in degrees
    double _roll;	//degrees
    double _pitch;	//degrees

    SGModelPlacement _aip;
    double _ground_elevation_m;

    void Transform();
};

#endif  // _FG_AIEntity_HXX

