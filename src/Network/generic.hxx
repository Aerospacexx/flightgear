// generic.hxx -- generic protocol class
//
// Written by Curtis Olson, started November 1999.
//
// Copyright (C) 1999  Curtis L. Olson - http://www.flightgear.org/~curt
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


#ifndef _FG_SERIAL_HXX
#define _FG_SERIAL_HXX


#include <simgear/compiler.h>

#include <string>

#include "protocol.hxx"

using std::string;


class FGGeneric : public FGProtocol {

public:

    FGGeneric(vector<string>);
    ~FGGeneric();

    bool gen_message();
    bool parse_message();

    // open hailing frequencies
    bool open();

    // process work for this port
    bool process();

    // close the channel
    bool close();

    void setExitOnError(bool val) { exitOnError = val; }
    bool getExitOnError() { return exitOnError; }
protected:

    enum e_type { FG_BOOL=0, FG_INT, FG_FLOAT, FG_DOUBLE, FG_STRING, FG_FIXED };

    typedef struct {
     // string name;
        string format;
        e_type type;
        double offset;
        double factor;
        SGPropertyNode_ptr prop;
    } _serial_prot;

private:

    int length;
    char buf[ FG_MAX_MSG_SIZE ];

    string preamble;
    string postamble;
    string var_separator;
    string line_separator;
    string var_sep_string;
    string line_sep_string;
    vector<_serial_prot> _out_message;
    vector<_serial_prot> _in_message;

    bool binary_mode;
    enum {FOOTER_NONE, FOOTER_LENGTH, FOOTER_MAGIC} binary_footer_type;
    int binary_footer_value;
    int binary_record_length;
    enum {BYTE_ORDER_NEEDS_CONVERSION, BYTE_ORDER_MATCHES_NETWORK_ORDER} binary_byte_order;

    void read_config(SGPropertyNode *root, vector<_serial_prot> &msg);
    bool exitOnError;
};


#endif // _FG_SERIAL_HXX


