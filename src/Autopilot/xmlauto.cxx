// xmlauto.cxx - a more flexible, generic way to build autopilots
//
// Written by Curtis Olson, started January 2004.
//
// Copyright (C) 2004  Curtis L. Olson  - http://www.flightgear.org/~curt
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
#  include <config.h>
#endif

#include <iostream>

#include <simgear/structure/exception.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/sg_inlines.h>
#include <simgear/props/props_io.hxx>

#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Main/util.hxx>

#include "xmlauto.hxx"

using std::cout;
using std::endl;

void FGXMLAutoInput::parse( SGPropertyNode_ptr node, double aValue, double aOffset, double aScale )
{
    value = aValue;
    property = NULL; 
    offset = NULL;
    scale = NULL;
    min = NULL;
    max = NULL;

    if( node == NULL )
        return;

    SGPropertyNode * n;

    if( (n = node->getChild("condition")) != NULL ) {
        _condition = sgReadCondition(node, n);
    }

    if( (n = node->getChild( "scale" )) != NULL ) {
        scale = new FGXMLAutoInput( n, aScale );
    }

    if( (n = node->getChild( "offset" )) != NULL ) {
        offset = new FGXMLAutoInput( n, aOffset );
    }

    if( (n = node->getChild( "max" )) != NULL ) {
        max = new FGXMLAutoInput( n );
    }

    if( (n = node->getChild( "min" )) != NULL ) {
        min = new FGXMLAutoInput( n );
    }

    SGPropertyNode *valueNode = node->getChild( "value" );
    if ( valueNode != NULL ) {
        value = valueNode->getDoubleValue();
    }

    n = node->getChild( "property" );
    // if no <property> element, check for <prop> element for backwards
    // compatibility
    if(  n == NULL )
        n = node->getChild( "prop" );

    if (  n != NULL ) {
        property = fgGetNode(  n->getStringValue(), true );
        if ( valueNode != NULL ) {
            // initialize property with given value 
            // if both <prop> and <value> exist
            double s = get_scale();
            if( s != 0 )
              property->setDoubleValue( (value - get_offset())/s );
            else
              property->setDoubleValue( 0 ); // if scale is zero, value*scale is zero
        }
    }

    if ( n == NULL && valueNode == NULL ) {
        // no <value> element and no <prop> element, use text node 
        const char * textnode = node->getStringValue();
        char * endp = NULL;
        // try to convert to a double value. If the textnode does not start with a number
        // endp will point to the beginning of the string. We assume this should be
        // a property name
        value = strtod( textnode, &endp );
        if( endp == textnode ) {
          property = fgGetNode( textnode, true );
        }
    }
}

void FGXMLAutoInput::set_value( double aValue ) 
{
    double s = get_scale();
    if( s != 0 )
        property->setDoubleValue( (aValue - get_offset())/s );
    else
        property->setDoubleValue( 0 ); // if scale is zero, value*scale is zero
}

double FGXMLAutoInput::get_value() 
{
    if( property != NULL ) 
        value = property->getDoubleValue();

    if( scale ) 
        value *= scale->get_value();

    if( offset ) 
        value += offset->get_value();

    if( min ) {
        double m = min->get_value();
        if( value < m )
            value = m;
    }

    if( max ) {
        double m = max->get_value();
        if( value > m )
            value = m;
    }
    
    return value;
}

FGXMLAutoComponent::FGXMLAutoComponent( SGPropertyNode * node ) :
      debug(false),
      name(""),
      enable_prop( NULL ),
      passive_mode( fgGetNode("/autopilot/locks/passive-mode", true) ),
      enable_value( NULL ),
      honor_passive( false ),
      enabled( false ),
      _condition( NULL ),
      feedback_if_disabled( false )
{
    int i;
    SGPropertyNode *prop; 

    for ( i = 0; i < node->nChildren(); ++i ) {
        SGPropertyNode *child = node->getChild(i);
        string cname = child->getName();
        string cval = child->getStringValue();
        if ( cname == "name" ) {
            name = cval;

        } else if ( cname == "feedback-if-disabled" ) {
            feedback_if_disabled = child->getBoolValue();

        } else if ( cname == "debug" ) {
            debug = child->getBoolValue();

        } else if ( cname == "enable" ) {
            if( (prop = child->getChild("condition")) != NULL ) {
              _condition = sgReadCondition(child, prop);
            } else {
               if ( (prop = child->getChild( "prop" )) != NULL ) {
                   enable_prop = fgGetNode( prop->getStringValue(), true );
               }

               if ( (prop = child->getChild( "value" )) != NULL ) {
                   delete enable_value;
                   enable_value = new string(prop->getStringValue());
               }
            }
            if ( (prop = child->getChild( "honor-passive" )) != NULL ) {
                honor_passive = prop->getBoolValue();
            }

        } else if ( cname == "input" ) {

              valueInput.push_back( new FGXMLAutoInput( child ) );

        } else if ( cname == "reference" ) {

              referenceInput.push_back( new FGXMLAutoInput( child ) );

        } else if ( cname == "output" ) {
            // grab all <prop> and <property> childs
            int found = 0;
            // backwards compatibility: allow <prop> elements
            for( int i = 0; (prop = child->getChild("prop", i)) != NULL; i++ ) { 
                SGPropertyNode *tmp = fgGetNode( prop->getStringValue(), true );
                output_list.push_back( tmp );
                found++;
            }
            for( int i = 0; (prop = child->getChild("property", i)) != NULL; i++ ) { 
                SGPropertyNode *tmp = fgGetNode( prop->getStringValue(), true );
                output_list.push_back( tmp );
                found++;
            }

            // no <prop> elements, text node of <output> is property name
            if( found == 0 )
                output_list.push_back( fgGetNode(child->getStringValue(), true ) );

        } else if ( cname == "config" ) {
            if( (prop = child->getChild("min")) != NULL ) {
              uminInput.push_back( new FGXMLAutoInput( prop ) );
            }
            if( (prop = child->getChild("u_min")) != NULL ) {
              uminInput.push_back( new FGXMLAutoInput( prop ) );
            }
            if( (prop = child->getChild("max")) != NULL ) {
              umaxInput.push_back( new FGXMLAutoInput( prop ) );
            }
            if( (prop = child->getChild("u_max")) != NULL ) {
              umaxInput.push_back( new FGXMLAutoInput( prop ) );
            }
        } else if ( cname == "min" ) {
            uminInput.push_back( new FGXMLAutoInput( child ) );
        } else if ( cname == "u_min" ) {
            uminInput.push_back( new FGXMLAutoInput( child ) );
        } else if ( cname == "max" ) {
            umaxInput.push_back( new FGXMLAutoInput( child ) );
        } else if ( cname == "u_max" ) {
            umaxInput.push_back( new FGXMLAutoInput( child ) );
        } 
    }   
}

FGXMLAutoComponent::~FGXMLAutoComponent() 
{
    delete enable_value;
}

bool FGXMLAutoComponent::isPropertyEnabled()
{
    if( _condition )
        return _condition->test();

    if( enable_prop ) {
        if( enable_value ) {
            return *enable_value == enable_prop->getStringValue();
        } else {
            return enable_prop->getBoolValue();
        }
    }
    return true;
}

void FGXMLAutoComponent::do_feedback_if_disabled()
{
    if( output_list.size() > 0 ) {    
        FGXMLAutoInput * input = valueInput.get_active();
        if( input != NULL )
            input->set_value( output_list[0]->getDoubleValue() );
    }
}

double FGXMLAutoComponent::clamp( double value )
{
    // clamp, if either min or max is defined
    if( uminInput.size() + umaxInput.size() > 0 ) {
        double d = umaxInput.get_value( 0.0 );
        if( value > d ) value = d;
        d = uminInput.get_value( 0.0 );
        if( value < d ) value = d;
    }
    return value;
}

FGPIDController::FGPIDController( SGPropertyNode *node ):
    FGXMLAutoComponent( node ),
    alpha( 0.1 ),
    beta( 1.0 ),
    gamma( 0.0 ),
    ep_n_1( 0.0 ),
    edf_n_1( 0.0 ),
    edf_n_2( 0.0 ),
    u_n_1( 0.0 ),
    desiredTs( 0.0 ),
    elapsedTime( 0.0 )
{
    int i;
    for ( i = 0; i < node->nChildren(); ++i ) {
        SGPropertyNode *child = node->getChild(i);
        string cname = child->getName();
        string cval = child->getStringValue();
        if ( cname == "config" ) {
            SGPropertyNode *config;

            if ( (config = child->getChild( "Ts" )) != NULL ) {
                desiredTs = config->getDoubleValue();
            }
           
            Kp.push_back( new FGXMLAutoInput( child->getChild( "Kp" ) ) );
            Ti.push_back( new FGXMLAutoInput( child->getChild( "Ti" ) ) );
            Td.push_back( new FGXMLAutoInput( child->getChild( "Td" ) ) );

            config = child->getChild( "beta" );
            if ( config != NULL ) {
                beta = config->getDoubleValue();
            }

            config = child->getChild( "alpha" );
            if ( config != NULL ) {
                alpha = config->getDoubleValue();
            }

            config = child->getChild( "gamma" );
            if ( config != NULL ) {
                gamma = config->getDoubleValue();
            }

        } else {
            SG_LOG( SG_AUTOPILOT, SG_WARN, "Error in autopilot config logic" );
            if ( get_name().length() ) {
                SG_LOG( SG_AUTOPILOT, SG_WARN, "Section = " << get_name() );
            }
        }
    }   
}


/*
 * Roy Vegard Ovesen:
 *
 * Ok! Here is the PID controller algorithm that I would like to see
 * implemented:
 *
 *   delta_u_n = Kp * [ (ep_n - ep_n-1) + ((Ts/Ti)*e_n)
 *               + (Td/Ts)*(edf_n - 2*edf_n-1 + edf_n-2) ]
 *
 *   u_n = u_n-1 + delta_u_n
 *
 * where:
 *
 * delta_u : The incremental output
 * Kp      : Proportional gain
 * ep      : Proportional error with reference weighing
 *           ep = beta * r - y
 *           where:
 *           beta : Weighing factor
 *           r    : Reference (setpoint)
 *           y    : Process value, measured
 * e       : Error
 *           e = r - y
 * Ts      : Sampling interval
 * Ti      : Integrator time
 * Td      : Derivator time
 * edf     : Derivate error with reference weighing and filtering
 *           edf_n = edf_n-1 / ((Ts/Tf) + 1) + ed_n * (Ts/Tf) / ((Ts/Tf) + 1)
 *           where:
 *           Tf : Filter time
 *           Tf = alpha * Td , where alpha usually is set to 0.1
 *           ed : Unfiltered derivate error with reference weighing
 *             ed = gamma * r - y
 *             where:
 *             gamma : Weighing factor
 * 
 * u       : absolute output
 * 
 * Index n means the n'th value.
 * 
 * 
 * Inputs:
 * enabled ,
 * y_n , r_n , beta=1 , gamma=0 , alpha=0.1 ,
 * Kp , Ti , Td , Ts (is the sampling time available?)
 * u_min , u_max
 * 
 * Output:
 * u_n
 */

void FGPIDController::update( double dt ) {
    double ep_n;            // proportional error with reference weighing
    double e_n;             // error
    double ed_n;            // derivative error
    double edf_n;           // derivative error filter
    double Tf;              // filter time
    double delta_u_n = 0.0; // incremental output
    double u_n = 0.0;       // absolute output
    double Ts;              // sampling interval (sec)

    double u_min = uminInput.get_value();
    double u_max = umaxInput.get_value();

    elapsedTime += dt;
    if ( elapsedTime <= desiredTs ) {
        // do nothing if time step is not positive (i.e. no time has
        // elapsed)
        return;
    }
    Ts = elapsedTime;
    elapsedTime = 0.0;

    if ( isPropertyEnabled() ) {
        if ( !enabled ) {
            // first time being enabled, seed u_n with current
            // property tree value
            u_n = get_output_value();
            u_n_1 = u_n;
        }
        enabled = true;
    } else {
        enabled = false;
        do_feedback();
    }

    if ( enabled && Ts > 0.0) {
        if ( debug ) cout << "Updating " << get_name()
                          << " Ts " << Ts << endl;

        double y_n = valueInput.get_value();
        double r_n = referenceInput.get_value();
                      
        if ( debug ) cout << "  input = " << y_n << " ref = " << r_n << endl;

        // Calculates proportional error:
        ep_n = beta * r_n - y_n;
        if ( debug ) cout << "  ep_n = " << ep_n;
        if ( debug ) cout << "  ep_n_1 = " << ep_n_1;

        // Calculates error:
        e_n = r_n - y_n;
        if ( debug ) cout << " e_n = " << e_n;

        // Calculates derivate error:
        ed_n = gamma * r_n - y_n;
        if ( debug ) cout << " ed_n = " << ed_n;

        double td = Td.get_value();
        if ( td > 0.0 ) {
            // Calculates filter time:
            Tf = alpha * td;
            if ( debug ) cout << " Tf = " << Tf;

            // Filters the derivate error:
            edf_n = edf_n_1 / (Ts/Tf + 1)
                + ed_n * (Ts/Tf) / (Ts/Tf + 1);
            if ( debug ) cout << " edf_n = " << edf_n;
        } else {
            edf_n = ed_n;
        }

        // Calculates the incremental output:
        double ti = Ti.get_value();
        if ( ti > 0.0 ) {
            delta_u_n = Kp.get_value() * ( (ep_n - ep_n_1)
                               + ((Ts/ti) * e_n)
                               + ((td/Ts) * (edf_n - 2*edf_n_1 + edf_n_2)) );
        }

        if ( debug ) {
            cout << " delta_u_n = " << delta_u_n << endl;
            cout << "P:" << Kp.get_value() * (ep_n - ep_n_1)
                 << " I:" << Kp.get_value() * ((Ts/ti) * e_n)
                 << " D:" << Kp.get_value() * ((td/Ts) * (edf_n - 2*edf_n_1 + edf_n_2))
                 << endl;
        }

        // Integrator anti-windup logic:
        if ( delta_u_n > (u_max - u_n_1) ) {
            delta_u_n = u_max - u_n_1;
            if ( debug ) cout << " max saturation " << endl;
        } else if ( delta_u_n < (u_min - u_n_1) ) {
            delta_u_n = u_min - u_n_1;
            if ( debug ) cout << " min saturation " << endl;
        }

        // Calculates absolute output:
        u_n = u_n_1 + delta_u_n;
        if ( debug ) cout << "  output = " << u_n << endl;

        // Updates indexed values;
        u_n_1   = u_n;
        ep_n_1  = ep_n;
        edf_n_2 = edf_n_1;
        edf_n_1 = edf_n;

        set_output_value( u_n );
    } else if ( !enabled ) {
        ep_n  = 0.0;
        edf_n = 0.0;
        // Updates indexed values;
        u_n_1   = u_n;
        ep_n_1  = ep_n;
        edf_n_2 = edf_n_1;
        edf_n_1 = edf_n;
    }
}


FGPISimpleController::FGPISimpleController( SGPropertyNode *node ):
    FGXMLAutoComponent( node ),
    int_sum( 0.0 )
{
    int i;
    for ( i = 0; i < node->nChildren(); ++i ) {
        SGPropertyNode *child = node->getChild(i);
        string cname = child->getName();
        string cval = child->getStringValue();
        if ( cname == "config" ) {
            Kp.push_back( new FGXMLAutoInput( child->getChild( "Kp" ) ) );
            Ki.push_back( new FGXMLAutoInput( child->getChild( "Ki" ) ) );
        } else {
            SG_LOG( SG_AUTOPILOT, SG_WARN, "Error in autopilot config logic" );
            if ( get_name().length() ) {
                SG_LOG( SG_AUTOPILOT, SG_WARN, "Section = " << get_name() );
            }
        }
    }   
}


void FGPISimpleController::update( double dt ) {

    if ( isPropertyEnabled() ) {
        if ( !enabled ) {
            // we have just been enabled, zero out int_sum
            int_sum = 0.0;
        }
        enabled = true;
    } else {
        enabled = false;
        do_feedback();
    }

    if ( enabled ) {
        if ( debug ) cout << "Updating " << get_name() << endl;
        double y_n = valueInput.get_value();
        double r_n = referenceInput.get_value();
                      
        double error = r_n - y_n;
        if ( debug ) cout << "input = " << y_n
                          << " reference = " << r_n
                          << " error = " << error
                          << endl;

        double prop_comp = error * Kp.get_value();
        int_sum += error * Ki.get_value() * dt;


        if ( debug ) cout << "prop_comp = " << prop_comp
                          << " int_sum = " << int_sum << endl;

        double output = prop_comp + int_sum;
        output = clamp( output );
        set_output_value( output );
        if ( debug ) cout << "output = " << output << endl;
    }
}


FGPredictor::FGPredictor ( SGPropertyNode *node ):
    FGXMLAutoComponent( node )
{
    int i;
    for ( i = 0; i < node->nChildren(); ++i ) {
        SGPropertyNode *child = node->getChild(i);
        string cname = child->getName();
        if ( cname == "seconds" ) {
            seconds.push_back( new FGXMLAutoInput( child, 0 ) );
        } else if ( cname == "filter-gain" ) {
            filter_gain.push_back( new FGXMLAutoInput( child, 0 ) );
        }
    }   
}

void FGPredictor::update( double dt ) {
    /*
       Simple moving average filter converts input value to predicted value "seconds".

       Smoothing as described by Curt Olson:
         gain would be valid in the range of 0 - 1.0
         1.0 would mean no filtering.
         0.0 would mean no input.
         0.5 would mean (1 part past value + 1 part current value) / 2
         0.1 would mean (9 parts past value + 1 part current value) / 10
         0.25 would mean (3 parts past value + 1 part current value) / 4

    */

    double ivalue = valueInput.get_value();

    if ( isPropertyEnabled() ) {
        if ( !enabled ) {
            // first time being enabled
            last_value = ivalue;
        }
        enabled = true;
    } else {
        enabled = false;
        do_feedback();
    }

    if ( enabled ) {

        if ( dt > 0.0 ) {
            double current = (ivalue - last_value)/dt; // calculate current error change (per second)
            double average = dt < 1.0 ? ((1.0 - dt) * average + current * dt) : current;

            // calculate output with filter gain adjustment
            double output = ivalue + 
               (1.0 - filter_gain.get_value()) * (average * seconds.get_value()) + 
                       filter_gain.get_value() * (current * seconds.get_value());
            output = clamp( output );
            set_output_value( output );
        }
        last_value = ivalue;
    }
}


FGDigitalFilter::FGDigitalFilter(SGPropertyNode *node):
    FGXMLAutoComponent( node ),
    filterType(none)
{
    int i;
    for ( i = 0; i < node->nChildren(); ++i ) {
        SGPropertyNode *child = node->getChild(i);
        string cname = child->getName();
        string cval = child->getStringValue();
        if ( cname == "type" ) {
            if ( cval == "exponential" ) {
                filterType = exponential;
            } else if (cval == "double-exponential") {
                filterType = doubleExponential;
            } else if (cval == "moving-average") {
                filterType = movingAverage;
            } else if (cval == "noise-spike") {
                filterType = noiseSpike;
            } else if (cval == "gain") {
                filterType = gain;
            } else if (cval == "reciprocal") {
                filterType = reciprocal;
            }
        } else if ( cname == "filter-time" ) {
            TfInput.push_back( new FGXMLAutoInput( child, 1.0 ) );
            if( filterType == none ) filterType = exponential;
        } else if ( cname == "samples" ) {
            samplesInput.push_back( new FGXMLAutoInput( child, 1 ) );
            if( filterType == none ) filterType = movingAverage;
        } else if ( cname == "max-rate-of-change" ) {
            rateOfChangeInput.push_back( new FGXMLAutoInput( child, 1 ) );
            if( filterType == none ) filterType = noiseSpike;
        } else if ( cname == "gain" ) {
            gainInput.push_back( new FGXMLAutoInput( child, 1 ) );
            if( filterType == none ) filterType = gain;
        }
    }

    output.resize(2, 0.0);
    input.resize(samplesInput.get_value() + 1, 0.0);
}

void FGDigitalFilter::update(double dt)
{
    if ( isPropertyEnabled() ) {

        input.push_front(valueInput.get_value());
        input.resize(samplesInput.get_value() + 1, 0.0);

        if ( !enabled ) {
            // first time being enabled, initialize output to the
            // value of the output property to avoid bumping.
            output.push_front(get_output_value());
        }

        enabled = true;
    } else {
        enabled = false;
        do_feedback();
    }

    if ( enabled && dt > 0.0 ) {
        /*
         * Exponential filter
         *
         * Output[n] = alpha*Input[n] + (1-alpha)*Output[n-1]
         *
         */
         if( debug ) cout << "Updating " << get_name()
                          << " dt " << dt << endl;

        if (filterType == exponential)
        {
            double alpha = 1 / ((TfInput.get_value()/dt) + 1);
            output.push_front(alpha * input[0] + 
                              (1 - alpha) * output[0]);
        } 
        else if (filterType == doubleExponential)
        {
            double alpha = 1 / ((TfInput.get_value()/dt) + 1);
            output.push_front(alpha * alpha * input[0] + 
                              2 * (1 - alpha) * output[0] -
                              (1 - alpha) * (1 - alpha) * output[1]);
        }
        else if (filterType == movingAverage)
        {
            output.push_front(output[0] + 
                              (input[0] - input.back()) / samplesInput.get_value());
        }
        else if (filterType == noiseSpike)
        {
            double maxChange = rateOfChangeInput.get_value() * dt;

            if ((output[0] - input[0]) > maxChange)
            {
                output.push_front(output[0] - maxChange);
            }
            else if ((output[0] - input[0]) < -maxChange)
            {
                output.push_front(output[0] + maxChange);
            }
            else if (fabs(input[0] - output[0]) <= maxChange)
            {
                output.push_front(input[0]);
            }
        }
        else if (filterType == gain)
        {
            output[0] = gainInput.get_value() * input[0];
        }
        else if (filterType == reciprocal)
        {
            if (input[0] != 0.0) {
                output[0] = gainInput.get_value() / input[0];
            }
        }

        output[0] = clamp(output[0]) ;
        set_output_value( output[0] );

        output.resize(2);

        if (debug)
        {
            cout << "input:" << input[0] 
                 << "\toutput:" << output[0] << endl;
        }
    }
}


FGXMLAutopilot::FGXMLAutopilot() {
}


FGXMLAutopilot::~FGXMLAutopilot() {
}

 
void FGXMLAutopilot::init() {
    config_props = fgGetNode( "/autopilot/new-config", true );

    SGPropertyNode *path_n = fgGetNode("/sim/systems/autopilot/path");

    if ( path_n ) {
        SGPath config( globals->get_fg_root() );
        config.append( path_n->getStringValue() );

        SG_LOG( SG_ALL, SG_INFO, "Reading autopilot configuration from "
                << config.str() );
        try {
            readProperties( config.str(), config_props );

            if ( ! build() ) {
                SG_LOG( SG_ALL, SG_ALERT,
                        "Detected an internal inconsistency in the autopilot");
                SG_LOG( SG_ALL, SG_ALERT,
                        " configuration.  See earlier errors for" );
                SG_LOG( SG_ALL, SG_ALERT,
                        " details.");
                exit(-1);
            }        
        } catch (const sg_exception& exc) {
            SG_LOG( SG_ALL, SG_ALERT, "Failed to load autopilot configuration: "
                    << config.str() );
        }

    } else {
        SG_LOG( SG_ALL, SG_WARN,
                "No autopilot configuration specified for this model!");
    }
}


void FGXMLAutopilot::reinit() {
    components.clear();
    init();
}


void FGXMLAutopilot::bind() {
}

void FGXMLAutopilot::unbind() {
}

bool FGXMLAutopilot::build() {
    SGPropertyNode *node;
    int i;

    int count = config_props->nChildren();
    for ( i = 0; i < count; ++i ) {
        node = config_props->getChild(i);
        string name = node->getName();
        // cout << name << endl;
        if ( name == "pid-controller" ) {
            components.push_back( new FGPIDController( node ) );
        } else if ( name == "pi-simple-controller" ) {
            components.push_back( new FGPISimpleController( node ) );
        } else if ( name == "predict-simple" ) {
            components.push_back( new FGPredictor( node ) );
        } else if ( name == "filter" ) {
            components.push_back( new FGDigitalFilter( node ) );
        } else {
            SG_LOG( SG_ALL, SG_ALERT, "Unknown top level section: " 
                    << name );
            return false;
        }
    }

    return true;
}


/*
 * Update helper values
 */
static void update_helper( double dt ) {
    // Estimate speed in 5,10 seconds
    static SGPropertyNode_ptr vel = fgGetNode( "/velocities/airspeed-kt", true );
    static SGPropertyNode_ptr lookahead5
        = fgGetNode( "/autopilot/internal/lookahead-5-sec-airspeed-kt", true );
    static SGPropertyNode_ptr lookahead10
        = fgGetNode( "/autopilot/internal/lookahead-10-sec-airspeed-kt", true );

    static double average = 0.0; // average/filtered prediction
    static double v_last = 0.0;  // last velocity

    double v = vel->getDoubleValue();
    double a = 0.0;
    if ( dt > 0.0 ) {
        a = (v - v_last) / dt;

        if ( dt < 1.0 ) {
            average = (1.0 - dt) * average + dt * a;
        } else {
            average = a;
        }

        lookahead5->setDoubleValue( v + average * 5.0 );
        lookahead10->setDoubleValue( v + average * 10.0 );
        v_last = v;
    }

    // Calculate heading bug error normalized to +/- 180.0 (based on
    // DG indicated heading)
    static SGPropertyNode_ptr bug
        = fgGetNode( "/autopilot/settings/heading-bug-deg", true );
    static SGPropertyNode_ptr ind_hdg
        = fgGetNode( "/instrumentation/heading-indicator/indicated-heading-deg",
                     true );
    static SGPropertyNode_ptr ind_bug_error
        = fgGetNode( "/autopilot/internal/heading-bug-error-deg", true );

    double diff = bug->getDoubleValue() - ind_hdg->getDoubleValue();
    if ( diff < -180.0 ) { diff += 360.0; }
    if ( diff > 180.0 ) { diff -= 360.0; }
    ind_bug_error->setDoubleValue( diff );

    // Calculate heading bug error normalized to +/- 180.0 (based on
    // actual/nodrift magnetic-heading, i.e. a DG slaved to magnetic
    // compass.)
    static SGPropertyNode_ptr mag_hdg
        = fgGetNode( "/orientation/heading-magnetic-deg", true );
    static SGPropertyNode_ptr fdm_bug_error
        = fgGetNode( "/autopilot/internal/fdm-heading-bug-error-deg", true );

    diff = bug->getDoubleValue() - mag_hdg->getDoubleValue();
    if ( diff < -180.0 ) { diff += 360.0; }
    if ( diff > 180.0 ) { diff -= 360.0; }
    fdm_bug_error->setDoubleValue( diff );

    // Calculate true heading error normalized to +/- 180.0
    static SGPropertyNode_ptr target_true
        = fgGetNode( "/autopilot/settings/true-heading-deg", true );
    static SGPropertyNode_ptr true_hdg
        = fgGetNode( "/orientation/heading-deg", true );
    static SGPropertyNode_ptr true_track
        = fgGetNode( "/instrumentation/gps/indicated-track-true-deg", true );
    static SGPropertyNode_ptr true_error
        = fgGetNode( "/autopilot/internal/true-heading-error-deg", true );

    diff = target_true->getDoubleValue() - true_hdg->getDoubleValue();
    if ( diff < -180.0 ) { diff += 360.0; }
    if ( diff > 180.0 ) { diff -= 360.0; }
    true_error->setDoubleValue( diff );

    // Calculate nav1 target heading error normalized to +/- 180.0
    static SGPropertyNode_ptr target_nav1
        = fgGetNode( "/instrumentation/nav[0]/radials/target-auto-hdg-deg", true );
    static SGPropertyNode_ptr true_nav1
        = fgGetNode( "/autopilot/internal/nav1-heading-error-deg", true );
    static SGPropertyNode_ptr true_track_nav1
        = fgGetNode( "/autopilot/internal/nav1-track-error-deg", true );

    diff = target_nav1->getDoubleValue() - true_hdg->getDoubleValue();
    if ( diff < -180.0 ) { diff += 360.0; }
    if ( diff > 180.0 ) { diff -= 360.0; }
    true_nav1->setDoubleValue( diff );

    diff = target_nav1->getDoubleValue() - true_track->getDoubleValue();
    if ( diff < -180.0 ) { diff += 360.0; }
    if ( diff > 180.0 ) { diff -= 360.0; }
    true_track_nav1->setDoubleValue( diff );

    // Calculate nav1 selected course error normalized to +/- 180.0
    // (based on DG indicated heading)
    static SGPropertyNode_ptr nav1_course_error
        = fgGetNode( "/autopilot/internal/nav1-course-error", true );
    static SGPropertyNode_ptr nav1_selected_course
        = fgGetNode( "/instrumentation/nav[0]/radials/selected-deg", true );

    diff = nav1_selected_course->getDoubleValue() - ind_hdg->getDoubleValue();
//    if ( diff < -180.0 ) { diff += 360.0; }
//    if ( diff > 180.0 ) { diff -= 360.0; }
    SG_NORMALIZE_RANGE( diff, -180.0, 180.0 );
    nav1_course_error->setDoubleValue( diff );

    // Calculate vertical speed in fpm
    static SGPropertyNode_ptr vs_fps
        = fgGetNode( "/velocities/vertical-speed-fps", true );
    static SGPropertyNode_ptr vs_fpm
        = fgGetNode( "/autopilot/internal/vert-speed-fpm", true );

    vs_fpm->setDoubleValue( vs_fps->getDoubleValue() * 60.0 );


    // Calculate static port pressure rate in [inhg/s].
    // Used to determine vertical speed.
    static SGPropertyNode_ptr static_pressure
	= fgGetNode( "/systems/static[0]/pressure-inhg", true );
    static SGPropertyNode_ptr pressure_rate
	= fgGetNode( "/autopilot/internal/pressure-rate", true );

    static double last_static_pressure = 0.0;

    if ( dt > 0.0 ) {
	double current_static_pressure = static_pressure->getDoubleValue();

	double current_pressure_rate = 
	    ( current_static_pressure - last_static_pressure ) / dt;

	pressure_rate->setDoubleValue(current_pressure_rate);

	last_static_pressure = current_static_pressure;
    }

}


/*
 * Update the list of autopilot components
 */

void FGXMLAutopilot::update( double dt ) {
    update_helper( dt );

    unsigned int i;
    for ( i = 0; i < components.size(); ++i ) {
        components[i]->update( dt );
    }
}

