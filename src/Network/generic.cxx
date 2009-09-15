// generic.cxx -- generic protocal class
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>                // strstr()
#include <stdlib.h>                // strtod(), atoi()

#include <simgear/debug/logstream.hxx>
#include <simgear/io/iochannel.hxx>
#include <simgear/structure/exception.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/misc/stdint.hxx>
#include <simgear/props/props.hxx>
#include <simgear/props/props_io.hxx>

#include <Main/globals.hxx>
#include <Main/fg_props.hxx>
#include <Main/util.hxx>

#include "generic.hxx"



FGGeneric::FGGeneric(vector<string> tokens) : exitOnError(false)
{
    size_t configToken;
    if (tokens[1] == "socket") {
        configToken = 7;
    } else if (tokens[1] == "file") {
        configToken = 5;
    } else {
        configToken = 6; 
    }

    if (configToken >= tokens.size()) {
       SG_LOG(SG_GENERAL, SG_ALERT,
              "Not enough tokens passed for generic protocol");
       return;
    }

    string config = tokens[ configToken ];
    file_name = config+".xml";

    if (tokens[2] != "in" && tokens[2] != "out") {
        SG_LOG(SG_GENERAL, SG_ALERT, "Unsuported protocol direction: "
               << tokens[2]);
    }

    reinit();
}

FGGeneric::~FGGeneric() {
}

union u32 {
    uint32_t intVal;
    float floatVal;
};

union u64 {
    uint64_t longVal;
    double doubleVal;
};

// generate the message
bool FGGeneric::gen_message_binary() {
    string generic_sentence;
    length = 0;

    double val;
    for (unsigned int i = 0; i < _out_message.size(); i++) {

        switch (_out_message[i].type) {
        case FG_INT:
            val = _out_message[i].offset +
                  _out_message[i].prop->getIntValue() * _out_message[i].factor;

            if (binary_byte_order == BYTE_ORDER_MATCHES_NETWORK_ORDER) {
                *((int32_t*)&buf[length]) = (int32_t)val;
            } else {
                *((uint32_t*)&buf[length]) = sg_bswap_32((uint32_t)val);
            }
            length += sizeof(int32_t);
            break;

        case FG_BOOL:
            *((int8_t*)&buf[length])
                      = _out_message[i].prop->getBoolValue() ? true : false;
            length += 1;
            break;

        case FG_FIXED:
        {
            val = _out_message[i].offset +
                 _out_message[i].prop->getFloatValue() * _out_message[i].factor;

            int fixed = (int)(val * 65536.0f); 
            if (binary_byte_order == BYTE_ORDER_MATCHES_NETWORK_ORDER) {
                *((int32_t*)&buf[length]) = (int32_t)fixed;
            } else {
                *((uint32_t*)&buf[length]) = sg_bswap_32((uint32_t)fixed);
            } 
            length += sizeof(int32_t);
            break;
        }
        case FG_FLOAT:
            val = _out_message[i].offset +
                 _out_message[i].prop->getFloatValue() * _out_message[i].factor;

            if (binary_byte_order == BYTE_ORDER_MATCHES_NETWORK_ORDER) {
                *((float*)&buf[length]) = val;
            } else {
                u32 tmpun32;
                tmpun32.floatVal = static_cast<float>(val);
                *((uint32_t*)&buf[length]) = sg_bswap_32(tmpun32.intVal);
            }
            length += sizeof(uint32_t);
            break;

        case FG_DOUBLE:
            val = _out_message[i].offset +
                 _out_message[i].prop->getFloatValue() * _out_message[i].factor;

            if (binary_byte_order == BYTE_ORDER_MATCHES_NETWORK_ORDER) {
                *((double*)&buf[length]) = val;
            } else {
                u64 tmpun64;
                tmpun64.doubleVal = val;
                *((uint64_t*)&buf[length]) = sg_bswap_64(tmpun64.longVal);
            }
            length += sizeof(int64_t);
            break;

        default: // SG_STRING
            const char *strdata = _out_message[i].prop->getStringValue();
            int strlength = strlen(strdata);

            if (binary_byte_order == BYTE_ORDER_NEEDS_CONVERSION) {
                SG_LOG( SG_IO, SG_ALERT, "Generic protocol: "
                        "FG_STRING will be written in host byte order.");
            }
            /* Format for strings is 
             * [length as int, 4 bytes][ASCII data, length bytes]
             */
            if (binary_byte_order == BYTE_ORDER_MATCHES_NETWORK_ORDER) {
                *((int32_t*)&buf[length]) = strlength;
            } else {
                *((int32_t*)&buf[length]) = sg_bswap_32(strlength);
            }
            length += sizeof(int32_t);
            strncpy(&buf[length], strdata, strlength);
            length += strlength; 
            /* FIXME padding for alignment? Something like: 
             * length += (strlength % 4 > 0 ? sizeof(int32_t) - strlength % 4 : 0;
             */
        }
    }

    // add the footer to the packet ("line")
    switch (binary_footer_type) {
        case FOOTER_LENGTH:
            binary_footer_value = length;
            break;

        case FOOTER_MAGIC:
        case FOOTER_NONE:
            break;
    }

    if (binary_footer_type != FOOTER_NONE) {
        if (binary_byte_order == BYTE_ORDER_MATCHES_NETWORK_ORDER) {
            *((int32_t*)&buf[length]) = binary_footer_value;
        } else {
            *((int32_t*)&buf[length]) = sg_bswap_32(binary_footer_value);
        }
        length += sizeof(int32_t);
    }

    return true;
}

bool FGGeneric::gen_message_ascii() {
    string generic_sentence;
    char tmp[255];
    length = 0;

    double val;
    for (unsigned int i = 0; i < _out_message.size(); i++) {

        if (i > 0) {
            generic_sentence += var_separator;
        }

        switch (_out_message[i].type) {
        case FG_INT:
            val = _out_message[i].offset +
                  _out_message[i].prop->getIntValue() * _out_message[i].factor;
            snprintf(tmp, 255, _out_message[i].format.c_str(), (int)val);
            break;

        case FG_BOOL:
            snprintf(tmp, 255, _out_message[i].format.c_str(),
                               _out_message[i].prop->getBoolValue());
            break;

        case FG_FIXED:
            val = _out_message[i].offset +
                _out_message[i].prop->getFloatValue() * _out_message[i].factor;
            snprintf(tmp, 255, _out_message[i].format.c_str(), (float)val);
            break;

        case FG_FLOAT:
            val = _out_message[i].offset +
                _out_message[i].prop->getFloatValue() * _out_message[i].factor;
            snprintf(tmp, 255, _out_message[i].format.c_str(), (float)val);
            break;

        case FG_DOUBLE:
            val = _out_message[i].offset +
                _out_message[i].prop->getFloatValue() * _out_message[i].factor;
            snprintf(tmp, 255, _out_message[i].format.c_str(), (float)val);
            break;

        default: // SG_STRING
            snprintf(tmp, 255, _out_message[i].format.c_str(),
                                   _out_message[i].prop->getStringValue());
        }

        generic_sentence += tmp;
    }

    /* After each lot of variables has been added, put the line separator
     * char/string
     */
    generic_sentence += line_separator;

    length =  generic_sentence.length();
    strncpy( buf, generic_sentence.c_str(), length );

    return true;
}

bool FGGeneric::gen_message() {
    if (binary_mode) {
        return gen_message_binary();
    } else {
        return gen_message_ascii();
    }
}

bool FGGeneric::parse_message_binary() {
    char *p2, *p1 = buf;
    int32_t tmp32;
    double val;
    int i = -1;

    p2 = p1 + FG_MAX_MSG_SIZE;
    while ((++i < (int)_in_message.size()) && (p1  < p2)) {

        switch (_in_message[i].type) {
        case FG_INT:
            if (binary_byte_order == BYTE_ORDER_NEEDS_CONVERSION) {
                tmp32 = sg_bswap_32(*(int32_t *)p1);
            } else {
                tmp32 = *(int32_t *)p1;
            }

            val = _in_message[i].offset + (double)tmp32 * _in_message[i].factor;

            _in_message[i].prop->setIntValue((int)val);
            p1 += sizeof(int32_t);
            break;

        case FG_BOOL:
            _in_message[i].prop->setBoolValue( p1[0] != 0 );
            p1 += 1;
            break;

        case FG_FIXED:
            if (binary_byte_order == BYTE_ORDER_NEEDS_CONVERSION) {
                tmp32 = sg_bswap_32(*(int32_t *)p1);
            } else {
                tmp32 = *(int32_t *)p1;
            }

            val = _in_message[i].offset +
                  ((double)tmp32 / 65536.0f) * _in_message[i].factor;

            _in_message[i].prop->setFloatValue(val);
            p1 += sizeof(int32_t);
            break;

        case FG_FLOAT:
            u32 tmpun32;
            if (binary_byte_order == BYTE_ORDER_NEEDS_CONVERSION) {
                tmpun32.intVal = sg_bswap_32(*(uint32_t *)p1);
            } else {
                tmpun32.floatVal = *(float *)p1;
            }

            val = _in_message[i].offset +
                  tmpun32.floatVal * _in_message[i].factor;

            _in_message[i].prop->setFloatValue(val);
            p1 += sizeof(int32_t);
            break;

        case FG_DOUBLE:
            u64 tmpun64;
            if (binary_byte_order == BYTE_ORDER_NEEDS_CONVERSION) {
                tmpun64.longVal = sg_bswap_64(*(uint64_t *)p1);
            } else {
                tmpun64.doubleVal = *(double *)p1;
            }

            val = _in_message[i].offset +
                   tmpun64.doubleVal * _in_message[i].factor;

            _in_message[i].prop->setDoubleValue(val);
            p1 += sizeof(int64_t);
            break;

        default: // SG_STRING
            SG_LOG( SG_IO, SG_ALERT, "Generic protocol: "
                    "Ignoring unsupported binary input chunk type.");
        }
    }
    
    return true;
}

bool FGGeneric::parse_message_ascii() {
    char *p2, *p1 = buf;
    double val;
    int i = -1;

    while ((++i < (int)_in_message.size()) &&
           p1 && strcmp(p1, line_separator.c_str())) {

        p2 = strstr(p1, var_separator.c_str());
        if (p2) {
            *p2 = 0;
            p2 += var_separator.length();
        }

        switch (_in_message[i].type) {
        case FG_INT:
            val = _in_message[i].offset + atoi(p1) * _in_message[i].factor;
            _in_message[i].prop->setIntValue((int)val);
            break;

        case FG_BOOL:
            _in_message[i].prop->setBoolValue( atof(p1) != 0.0 );
            break;

        case FG_FIXED:
        case FG_FLOAT:
        case FG_DOUBLE:
            val = _in_message[i].offset + strtod(p1, 0) * _in_message[i].factor;
            _in_message[i].prop->setFloatValue((float)val);
            break;

        default: // SG_STRING
            _in_message[i].prop->setStringValue(p1);
        }

        p1 = p2;
    }

    return true;
}

bool FGGeneric::parse_message() {
    if (binary_mode) {
        return parse_message_binary(); 
    } else {
        return parse_message_ascii();
    }
}


// open hailing frequencies
bool FGGeneric::open() {
    if ( is_enabled() ) {
        SG_LOG( SG_IO, SG_ALERT, "This shouldn't happen, but the channel " 
                << "is already in use, ignoring" );
        return false;
    }

    SGIOChannel *io = get_io_channel();

    if ( ! io->open( get_direction() ) ) {
        SG_LOG( SG_IO, SG_ALERT, "Error opening channel communication layer." );
        return false;
    }

    set_enabled( true );

    if ( get_direction() == SG_IO_OUT && ! preamble.empty() ) {
        if ( ! io->write( preamble.c_str(), preamble.size() ) ) {
            SG_LOG( SG_IO, SG_WARN, "Error writing preamble." );
            return false;
        }
    }

    return true;
}


// process work for this port
bool FGGeneric::process() {
    SGIOChannel *io = get_io_channel();

    if ( get_direction() == SG_IO_OUT ) {
        gen_message();
        if ( ! io->write( buf, length ) ) {
            SG_LOG( SG_IO, SG_WARN, "Error writing data." );
            goto error_out;
        }
    } else if ( get_direction() == SG_IO_IN ) {
        if ( io->get_type() == sgFileType ) {
            if (!binary_mode) {
                length = io->readline( buf, FG_MAX_MSG_SIZE );
                if ( length > 0 ) {
                    parse_message();
                } else {
                    SG_LOG( SG_IO, SG_ALERT, "Error reading data." );
                    return false;
                }
            } else {
                length = io->read( buf, binary_record_length );
                if ( length == binary_record_length ) {
                    parse_message();
                } else {
                    SG_LOG( SG_IO, SG_ALERT,
                            "Generic protocol: Received binary "
                            "record of unexpected size, expected: "
                            << binary_record_length << " but received: "
                            << length);
                }
            }
        } else {
            do {
                if (!binary_mode) {
                    length = io->readline( buf, FG_MAX_MSG_SIZE );
                    if ( length > 0 ) {
                        parse_message();
                    }
                } else {
                    length = io->read( buf, binary_record_length );
                    if ( length == binary_record_length ) {
                        parse_message();
                    } else if ( length > 0 ) {
                        SG_LOG( SG_IO, SG_ALERT,
                            "Generic protocol: Received binary "
                            "record of unexpected size, expected: "
                            << binary_record_length << " but received: "
                            << length);
                    }
                }
            } while ( length == binary_record_length );
        }
    }
    return true;
error_out:
    if (exitOnError) {
        fgExit(1);
        return true; // should not get there, but please the compiler
    } else
        return false;
}


// close the channel
bool FGGeneric::close() {
    SGIOChannel *io = get_io_channel();

    if ( get_direction() == SG_IO_OUT && ! postamble.empty() ) {
        if ( ! io->write( postamble.c_str(), postamble.size() ) ) {
            SG_LOG( SG_IO, SG_ALERT, "Error writing postamble." );
            return false;
        }
    }

    set_enabled( false );

    if ( ! io->close() ) {
        return false;
    }

    return true;
}


void
FGGeneric::reinit()
{
    SGPath path( globals->get_fg_root() );
    path.append("Protocol");
    path.append(file_name.c_str());

    SG_LOG(SG_GENERAL, SG_INFO, "Reading communication protocol from "
                                << path.str());

    SGPropertyNode root;
    try {
        readProperties(path.str(), &root);
    } catch (const sg_exception &) {
        SG_LOG(SG_GENERAL, SG_ALERT,
         "Unable to load the protocol configuration file");
         return;
    }

    SGPropertyNode *output = root.getNode("generic/output");
    if (output) {
        _out_message.clear();
        read_config(output, _out_message);
    }

    SGPropertyNode *input = root.getNode("generic/input");
    if (input) {
        _in_message.clear();
        read_config(input, _in_message);
    }
}


void
FGGeneric::read_config(SGPropertyNode *root, vector<_serial_prot> &msg)
{
    binary_mode = root->getBoolValue("binary_mode");

    if (!binary_mode) {
        /* These variables specified in the $FG_ROOT/data/Protocol/xxx.xml
         * file for each format
         *
         * var_sep_string  = the string/charachter to place between variables
         * line_sep_string = the string/charachter to place at the end of each
         *                   lot of variables
         */
        preamble = fgUnescape(root->getStringValue("preamble"));
        postamble = fgUnescape(root->getStringValue("postamble"));
        var_sep_string = fgUnescape(root->getStringValue("var_separator"));
        line_sep_string = fgUnescape(root->getStringValue("line_separator"));

        if ( var_sep_string == "newline" ) {
            var_separator = '\n';
        } else if ( var_sep_string == "tab" ) {
            var_separator = '\t';
        } else if ( var_sep_string == "space" ) {
            var_separator = ' ';
        } else if ( var_sep_string == "formfeed" ) {
            var_separator = '\f';
        } else if ( var_sep_string == "carriagereturn" ) {
            var_sep_string = '\r';
        } else if ( var_sep_string == "verticaltab" ) {
            var_separator = '\v';
        } else {
            var_separator = var_sep_string;
        }

        if ( line_sep_string == "newline" ) {
            line_separator = '\n';
        } else if ( line_sep_string == "tab" ) {
            line_separator = '\t';
        } else if ( line_sep_string == "space" ) {
            line_separator = ' ';
        } else if ( line_sep_string == "formfeed" ) {
            line_separator = '\f';
        } else if ( line_sep_string == "carriagereturn" ) {
            line_separator = '\r';
        } else if ( line_sep_string == "verticaltab" ) {
            line_separator = '\v';
        } else {
            line_separator = line_sep_string;
        }
    } else {
        // default values: no footer and record_length = sizeof(representation)
        binary_footer_type = FOOTER_NONE;
        binary_record_length = -1;

        // default choice is network byte order (big endian)
        if (sgIsLittleEndian()) {
           binary_byte_order = BYTE_ORDER_NEEDS_CONVERSION;
        } else {
           binary_byte_order = BYTE_ORDER_MATCHES_NETWORK_ORDER;
        }

        if ( root->hasValue("binary_footer") ) {
            string footer_type = root->getStringValue("binary_footer");
            if ( footer_type == "length" ) {
                binary_footer_type = FOOTER_LENGTH;
            } else if ( footer_type.substr(0, 5) == "magic" ) {
                binary_footer_type = FOOTER_MAGIC;
                binary_footer_value = strtol(footer_type.substr(6, 
                            footer_type.length() - 6).c_str(), (char**)0, 0);
            } else if ( footer_type != "none" ) {
                SG_LOG(SG_IO, SG_ALERT,
                       "generic protocol: Unknown generic binary protocol "
                       "footer '" << footer_type << "', using no footer.");
            }
        }

        if ( root->hasValue("record_length") ) {
            binary_record_length = root->getIntValue("record_length");
        }

        if ( root->hasValue("byte_order") ) {
            string byte_order = root->getStringValue("byte_order");
            if (byte_order == "network" ) {
                if ( sgIsLittleEndian() ) {
                    binary_byte_order = BYTE_ORDER_NEEDS_CONVERSION;
                } else {
                    binary_byte_order = BYTE_ORDER_MATCHES_NETWORK_ORDER;
                }
            } else if ( byte_order == "host" ) {
                binary_byte_order = BYTE_ORDER_MATCHES_NETWORK_ORDER;
            } else {
                SG_LOG(SG_IO, SG_ALERT,
                       "generic protocol: Undefined generic binary protocol"
                       "byte order, using HOST byte order.");
            }
        }
    }

    int record_length = 0; // Only used for binary protocols.
    vector<SGPropertyNode_ptr> chunks = root->getChildren("chunk");
    for (unsigned int i = 0; i < chunks.size(); i++) {

        _serial_prot chunk;

        // chunk.name = chunks[i]->getStringValue("name");
        chunk.format = fgUnescape(chunks[i]->getStringValue("format", "%d"));
        chunk.offset = chunks[i]->getDoubleValue("offset");
        chunk.factor = chunks[i]->getDoubleValue("factor", 1.0);

        string node = chunks[i]->getStringValue("node", "/null");
        chunk.prop = fgGetNode(node.c_str(), true);

        string type = chunks[i]->getStringValue("type");

        // Note: officially the type is called 'bool' but for backward
        //       compatibility 'boolean' will also be supported.
        if (type == "bool" || type == "boolean") {
            chunk.type = FG_BOOL;
            record_length += 1;
        } else if (type == "float") {
            chunk.type = FG_FLOAT;
            record_length += sizeof(int32_t);
        } else if (type == "double") {
            chunk.type = FG_DOUBLE;
            record_length += sizeof(int64_t);
        } else if (type == "fixed") {
            chunk.type = FG_FIXED;
            record_length += sizeof(int32_t);
        } else if (type == "string")
            chunk.type = FG_STRING;
        else {
            chunk.type = FG_INT;
            record_length += sizeof(int32_t);
        }
        msg.push_back(chunk);

    }

    if (binary_record_length == -1) {
        binary_record_length = record_length;
    } else if (binary_record_length < record_length) {
        SG_LOG(SG_IO, SG_ALERT,
               "generic protocol: Requested binary record length shorter than "
               " requested record representation.");
        binary_record_length = record_length;
    }
}
