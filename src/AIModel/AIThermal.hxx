// FGAIThermal - AIBase derived class creates an AI thunderstorm
//
// Written by David Culp, started Feb 2004.
//
// Copyright (C) 2004  David P. Culp - davidculp2@comcast.net
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

#ifndef _FG_AIThermal_HXX
#define _FG_AIThermal_HXX

#include "AIManager.hxx"
#include "AIBase.hxx"

#include <string>
SG_USING_STD(string);


class FGAIThermal : public FGAIBase {

public:

	FGAIThermal();
	~FGAIThermal();
	
        void readFromScenario(SGPropertyNode* scFileNode);

	virtual bool init(bool search_in_AI_path=false);
        virtual void bind();
        virtual void unbind();
	virtual void update(double dt);

        inline void setMaxStrength( double s ) { max_strength = s; };
        inline void setDiameter( double d ) { diameter = d; };
        inline void setHeight( double h ) { height = h; };
        inline double getStrength() const { return strength; };
        inline double getDiameter() const { return diameter; };
        inline double getHeight() const { return height; };

        virtual const char* getTypeString(void) const { return "thermal"; }
private:

	void Run(double dt);
        double max_strength;
        double strength;
        double diameter;
        double height;
        double factor;
};



#endif  // _FG_AIThermal_HXX
