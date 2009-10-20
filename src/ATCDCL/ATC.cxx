// Implementation of FGATC - ATC subsystem base class.
//
// Written by David Luff, started February 2002.
//
// Copyright (C) 2002  David C Luff - david.luff@nottingham.ac.uk
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

#include "ATC.hxx"

#include <iostream>
#include <memory>

#include <simgear/sound/soundmgr_openal.hxx>
#include <simgear/structure/exception.hxx>

#include <Main/globals.hxx>
#include <Main/fg_props.hxx>



FGATC::FGATC() :
	_voiceOK(false),
	_sgr(NULL),
	freqClear(true),
	receiving(false),
	respond(false),
	responseID(""),
	runResponseCounter(false),
	_runReleaseCounter(false),
	responseReqd(false),
	_type(INVALID),
	_display(false),
	// Transmission timing stuff
	pending_transmission(""),
	_timeout(0),
	_pending(false),
	_callback_code(0),
	_transmit(false),
	_transmitting(false),
	_counter(0.0),
	_max_count(5.0)
{
	SGSoundMgr *smgr;
	smgr = (SGSoundMgr *)globals->get_subsystem("soundmgr");
	_sgr = smgr->find("atc", true);
}

FGATC::~FGATC() {
}

// Derived classes wishing to use the response counter should 
// call this from their own Update(...).
void FGATC::Update(double dt) {
	if(runResponseCounter) {
		//cout << responseCounter << '\t' << responseTime << '\n';
		if(responseCounter >= responseTime) {
			runResponseCounter = false;
			respond = true;
			//cout << "RESPOND\n";
		} else {
			responseCounter += dt;
		}
	}
	
	if(_runReleaseCounter) {
		if(_releaseCounter >= _releaseTime) {
			freqClear = true;
			_runReleaseCounter = false;
		} else {
			_releaseCounter += dt;
		}
	}
	
	// Transmission stuff cribbed from AIPlane.cxx
	if(_pending) {
		if(GetFreqClear()) {
			//cout << "TUNED STATION FREQ CLEAR\n";
			SetFreqInUse();
			_pending = false;
			_transmit = true;
			_transmitting = false;
		} else {
			if(_timeout > 0.0) {	// allows count down to be avoided by initially setting it to zero
				_timeout -= dt;
				if(_timeout <= 0.0) {
					_timeout = 0.0;
					_pending = false;
					// timed out - don't render.
				}
			}
		}
	}

	if(_transmit) {
		_counter = 0.0;
		_max_count = 5.0;		// FIXME - hardwired length of message - need to calculate it!
		
		//cout << "Transmission = " << pending_transmission << '\n';
		if(_display) {
			//Render(pending_transmission, ident, false);
			Render(pending_transmission);
		}
		// Run the callback regardless of whether on same freq as user or not.
		//cout << "_callback_code = " << _callback_code << '\n';
		if(_callback_code) {
			ProcessCallback(_callback_code);
		}
		_transmit = false;
		_transmitting = true;
	} else if(_transmitting) {
		if(_counter >= _max_count) {
			//NoRender(plane.callsign);  commented out since at the moment NoRender is designed just to stop repeating messages,
			// and this will be primarily used on single messages.
			_transmitting = false;
			//if(tuned_station) tuned_station->NotifyTransmissionFinished(plane.callsign);
			// TODO - need to let the plane the transmission is aimed at that it's finished.
			// However, for now we'll just release the frequency since if we don't it all goes pear-shaped
			_releaseCounter = 0.0;
			_releaseTime = 0.9;
			_runReleaseCounter = true;
		}
		_counter += dt;
	}
}

void FGATC::ReceiveUserCallback(int code) {
	SG_LOG(SG_ATC, SG_WARN, "WARNING - whichever ATC class was intended to receive callback code " << code << " didn't get it!!!");
}

void FGATC::SetResponseReqd(const string& rid) {
	receiving = false;
	responseReqd = true;
	respond = false;	// TODO - this ignores the fact that more than one plane could call this before response
						// Shouldn't happen with AI only, but user could confuse things??
	responseID = rid;
	runResponseCounter = true;
	responseCounter = 0.0;
	responseTime = 1.8;		// TODO - randomize this slightly.
}

void FGATC::NotifyTransmissionFinished(const string& rid) {
	//cout << "Transmission finished, callsign = " << rid << '\n';
	receiving = false;
	responseID = rid;
	if(responseReqd) {
		runResponseCounter = true;
		responseCounter = 0.0;
		responseTime = 1.2;	// TODO - randomize this slightly, and allow it to be dependent on the transmission and how busy the ATC is.
		respond = false;	// TODO - this ignores the fact that more than one plane could call this before response
							// Shouldn't happen with AI only, but user could confuse things??
	} else {
		freqClear = true;
	}
}

void FGATC::Transmit(int callback_code) {
	SG_LOG(SG_ATC, SG_INFO, "Transmit called by " << ident << " " << _type << ", msg = " << pending_transmission);
	_pending = true;
	_callback_code = callback_code;
	_timeout = 0.0;
}

void FGATC::ConditionalTransmit(double timeout, int callback_code) {
	SG_LOG(SG_ATC, SG_INFO, "Timed transmit called by " << ident << " " << _type << ", msg = " << pending_transmission);
	_pending = true;
	_callback_code = callback_code;
	_timeout = timeout;
}

void FGATC::ImmediateTransmit(int callback_code) {
	SG_LOG(SG_ATC, SG_INFO, "Immediate transmit called by " << ident << " " << _type << ", msg = " << pending_transmission);
	if(_display) {
		//Render(pending_transmission, ident, false);
		Render(pending_transmission);
		// At the moment Render doesn't work except for ATIS
	}
	if(callback_code) {
		ProcessCallback(callback_code);
	}
}

// Derived classes should override this.
void FGATC::ProcessCallback(int code) {
}

void FGATC::AddPlane(const string& pid) {
}

int FGATC::RemovePlane() {
	return 0;
}

void FGATC::SetData(ATCData* d) {
	_type = d->type;
	_geod = d->geod;
	_cart = d->cart;
	range = d->range;
	ident = d->ident;
	name = d->name;
	freq = d->freq;
}

// Render a transmission
// Outputs the transmission either on screen or as audio depending on user preference
// The refname is a string to identify this sample to the sound manager
// The repeating flag indicates whether the message should be repeated continuously or played once.
void FGATC::Render(string& msg, const float volume, 
				   const string& refname, const bool repeating) {
	if (repeating)
		fgSetString("/sim/messages/atis", msg.c_str());
	else
		fgSetString("/sim/messages/atc", msg.c_str());

#ifdef ENABLE_AUDIO_SUPPORT
	_voice = (_voiceOK && fgGetBool("/sim/sound/voice"));
	if(_voice) {
		string buf = _vPtr->WriteMessage((char*)msg.c_str(), _voice);
		if(_voice && (volume > 0.05)) {
			NoRender(refname);
			try {
// >>> Beware: must pass a (new) object to the (add) method,
// >>> because the (remove) method is going to do a (delete)
// >>> whether that's what you want or not.
				std::auto_ptr<unsigned char> ptr( (unsigned char*)buf.c_str() );
				SGSoundSample *simple = 
				    new SGSoundSample(ptr,  buf.length(), 8000);
				// TODO - at the moment the volume can't be changed 
				// after the transmission has started.
				simple->set_volume(volume);
				_sgr->add(simple, refname);
				_sgr->play(refname, repeating);
			} catch ( sg_io_exception &e ) {
				SG_LOG(SG_GENERAL, SG_ALERT, e.getFormattedMessage());
			}
		}
	}
#endif	// ENABLE_AUDIO_SUPPORT
	if(!_voice) {
		// first rip the underscores and the pause hints out of the string - these are for the convienience of the voice parser
		for(unsigned int i = 0; i < msg.length(); ++i) {
			if((msg.substr(i,1) == "_") || (msg.substr(i,1) == "/")) {
				msg[i] = ' ';
			}
		}
	}
	_playing = true;	
}


// Cease rendering a transmission.
void FGATC::NoRender(const string& refname) {
	if(_playing) {
		if(_voice) {
#ifdef ENABLE_AUDIO_SUPPORT		
			_sgr->stop(refname);
			_sgr->remove(refname);
#endif
		}
		_playing = false;
	}
}

// Generate the text of a message from its parameters and the current context.
string FGATC::GenText(const string& m, int c) {
	return("");
}

ostream& operator << (ostream& os, atc_type atc) {
	switch(atc) {
		case(AWOS):	   return(os << "AWOS");
		case(ATIS):	   return(os << "ATIS");
		case(GROUND):	 return(os << "GROUND");
		case(TOWER):	  return(os << "TOWER");
		case(APPROACH):   return(os << "APPROACH");
		case(DEPARTURE):  return(os << "DEPARTURE");
		case(ENROUTE):	return(os << "ENROUTE");
		case(INVALID):	return(os << "INVALID");
	}
	return(os << "ERROR - Unknown switch in atc_type operator << ");
}

std::istream& operator >> ( std::istream& fin, ATCData& a )
{
	double f;
	char ch;
	char tp;
	
	fin >> tp;
	
	switch(tp) {
	case 'I':
		a.type = ATIS;
		break;
	case 'T':
		a.type = TOWER;
		break;
	case 'G':
		a.type = GROUND;
		break;
	case 'A':
		a.type = APPROACH;
		break;
	case '[':
		a.type = INVALID;
		return fin >> skipeol;
	default:
		SG_LOG(SG_GENERAL, SG_ALERT, "Warning - unknown type \'" << tp << "\' found whilst reading ATC frequency data!\n");
		a.type = INVALID;
		return fin >> skipeol;
	}
	
	double lat, lon, elev;
  
	fin >> lat >> lon >> elev >> f >> a.range >> a.ident;
	a.geod = SGGeod::fromDegM(lon, lat, elev);
	a.name = "";
	fin >> ch;
	if(ch != '"') a.name += ch;
	while(1) {
		//in >> noskipws
		fin.unsetf(std::ios::skipws);
		fin >> ch;
		if((ch == '"') || (ch == 0x0A)) {
			break;
		}   // we shouldn't need the 0x0A but it makes a nice safely in case someone leaves off the "
		a.name += ch;
	}
	fin.setf(std::ios::skipws);
	//cout << "Comm name = " << a.name << '\n';
	
	a.freq = (int)(f*100.0 + 0.5);
	
	// cout << a.ident << endl;
	
	// generate cartesian coordinates
	a.cart = SGVec3d::fromGeod(a.geod);	
	return fin >> skipeol;
}

