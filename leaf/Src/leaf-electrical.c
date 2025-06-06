/*
 * leaf-electrical.c
 *
 *  Created on: Sep 25, 2019
 *      Author: jeffsnyder
 */

#if _WIN32 || _WIN64

#include "..\Inc\leaf-electrical.h"
#include "..\leaf.h"

#else

#include "../Inc/leaf-electrical.h"
#include "../leaf.h"

#endif


//this got messed up in the switch of the pointer styles. Likely not working at all right now. -JS
//==============================================================================

static Lfloat get_port_resistance_for_resistor  (tWDF* const r);
static Lfloat get_port_resistance_for_capacitor (tWDF* const r);
static Lfloat get_port_resistance_for_inductor  (tWDF* const r);
static Lfloat get_port_resistance_for_resistive (tWDF* const r);
static Lfloat get_port_resistance_for_inverter  (tWDF* const r);
static Lfloat get_port_resistance_for_series    (tWDF* const r);
static Lfloat get_port_resistance_for_parallel  (tWDF* const r);
static Lfloat get_port_resistance_for_root      (tWDF* const r);

static void set_incident_wave_for_leaf          (tWDF* const r, Lfloat incident_wave, Lfloat input);
static void set_incident_wave_for_leaf_inverted (tWDF* const r, Lfloat incident_wave, Lfloat input);
static void set_incident_wave_for_inverter      (tWDF* const r, Lfloat incident_wave, Lfloat input);
static void set_incident_wave_for_series        (tWDF* const r, Lfloat incident_wave, Lfloat input);
static void set_incident_wave_for_parallel      (tWDF* const r, Lfloat incident_wave, Lfloat input);

static Lfloat get_reflected_wave_for_resistor   (tWDF* const r, Lfloat input);
static Lfloat get_reflected_wave_for_capacitor  (tWDF* const r, Lfloat input);
static Lfloat get_reflected_wave_for_resistive  (tWDF* const r, Lfloat input);
static Lfloat get_reflected_wave_for_inverter   (tWDF* const r, Lfloat input);
static Lfloat get_reflected_wave_for_series     (tWDF* const r, Lfloat input);
static Lfloat get_reflected_wave_for_parallel   (tWDF* const r, Lfloat input);

static Lfloat get_reflected_wave_for_ideal      (tWDF* const n, Lfloat input, Lfloat incident_wave);
static Lfloat get_reflected_wave_for_diode      (tWDF* const n, Lfloat input, Lfloat incident_wave);
static Lfloat get_reflected_wave_for_diode_pair (tWDF* const n, Lfloat input, Lfloat incident_wave);

static void wdf_init(tWDF** const wdf, WDFComponentType type, Lfloat value, tWDF** const rL, tWDF** const rR)
{
    tWDF* r = *wdf;
    LEAF* leaf = r->mempool->leaf;
    
    r->type = type;
    r->child_left = *rL;
    r->child_right = *rR;
    r->incident_wave_up = 0.0f;
    r->incident_wave_left = 0.0f;
    r->incident_wave_right = 0.0f;
    r->reflected_wave_up = 0.0f;
    r->reflected_wave_left = 0.0f;
    r->reflected_wave_right = 0.0f;
    r->sample_rate = leaf->sampleRate;
    r->value = value;
    
    tWDF* child;
    if (r->child_left != NULL) child = r->child_left;
    else child = r->child_right;
    
    if (r->type == Resistor)
    {
        r->port_resistance_up = r->value;
        r->port_conductance_up = 1.0f / r->value;
        
        r->get_port_resistance = &get_port_resistance_for_resistor;
        r->get_reflected_wave_up = &get_reflected_wave_for_resistor;
        r->set_incident_wave = &set_incident_wave_for_leaf;
    }
    else if (r->type == Capacitor)
    {
        r->port_conductance_up = r->sample_rate * 2.0f * r->value;
        r->port_resistance_up = 1.0f / r->port_conductance_up; //based on trapezoidal discretization
        
        r->get_port_resistance = &get_port_resistance_for_capacitor;
        r->get_reflected_wave_up = &get_reflected_wave_for_capacitor;
        r->set_incident_wave = &set_incident_wave_for_leaf;
    }
    else if (r->type == Inductor)
    {
        r->port_resistance_up = r->sample_rate * 2.0f * r->value; //based on trapezoidal discretization
        r->port_conductance_up = 1.0f / r->port_resistance_up;
        
        r->get_port_resistance = &get_port_resistance_for_inductor;
        r->get_reflected_wave_up = &get_reflected_wave_for_capacitor; // same as capacitor
        r->set_incident_wave = &set_incident_wave_for_leaf_inverted;
    }
    else if (r->type == ResistiveSource)
    {
        r->port_resistance_up = r->value;
        r->port_conductance_up = 1.0f / r->port_resistance_up;
        
        r->get_port_resistance = &get_port_resistance_for_resistive;
        r->get_reflected_wave_up = &get_reflected_wave_for_resistive;
        r->set_incident_wave = &set_incident_wave_for_leaf;
    }
    else if (r->type == Inverter)
    {
        r->port_resistance_up = tWDF_getPortResistance(r->child_left);
        r->port_conductance_up = 1.0f / r->port_resistance_up;
        
        r->get_port_resistance = &get_port_resistance_for_inverter;
        r->get_reflected_wave_up = &get_reflected_wave_for_inverter;
        r->set_incident_wave = &set_incident_wave_for_inverter;
    }
    else if (r->type == SeriesAdaptor)
    {
        r->port_resistance_left = tWDF_getPortResistance(r->child_left);
        r->port_resistance_right = tWDF_getPortResistance(r->child_right);
        r->port_resistance_up = r->port_resistance_left + r->port_resistance_right;
        r->port_conductance_up  = 1.0f / r->port_resistance_up;
        r->port_conductance_left = 1.0f / r->port_resistance_left;
        r->port_conductance_right = 1.0f / r->port_resistance_right;
        r->gamma_zero = 1.0f / (r->port_resistance_right + r->port_resistance_left);
        
        r->get_port_resistance = &get_port_resistance_for_series;
        r->get_reflected_wave_up = &get_reflected_wave_for_series;
        r->set_incident_wave = &set_incident_wave_for_series;
    }
    else if (r->type == ParallelAdaptor)
    {
        r->port_resistance_left = tWDF_getPortResistance(r->child_left);
        r->port_resistance_right = tWDF_getPortResistance(r->child_right);
        r->port_resistance_up = (r->port_resistance_left * r->port_resistance_right) / (r->port_resistance_left + r->port_resistance_right);
        r->port_conductance_up  = 1.0f / r->port_resistance_up;
        r->port_conductance_left = 1.0f / r->port_resistance_left;
        r->port_conductance_right = 1.0f / r->port_resistance_right;
        r->gamma_zero = 1.0f / (r->port_resistance_right + r->port_resistance_left);
        
        r->get_port_resistance = &get_port_resistance_for_parallel;
        r->get_reflected_wave_up = &get_reflected_wave_for_parallel;
        r->set_incident_wave = &set_incident_wave_for_parallel;
    }
    else if (r->type == IdealSource)
    {
        r->port_resistance_up = tWDF_getPortResistance(child);
        r->port_conductance_up = 1.0f / r->port_resistance_up;
        
        r->get_reflected_wave_down = &get_reflected_wave_for_ideal;
        r->get_port_resistance = &get_port_resistance_for_root;
    }
    else if (r->type == Diode)
    {
        r->port_resistance_up = tWDF_getPortResistance(child);
        r->port_conductance_up = 1.0f / r->port_resistance_up;
        
        r->get_reflected_wave_down = &get_reflected_wave_for_diode;
        r->get_port_resistance = &get_port_resistance_for_root;
    }
    else if (r->type == DiodePair)
    {
        r->port_resistance_up = tWDF_getPortResistance(child);
        r->port_conductance_up = 1.0f / r->port_resistance_up;
        
        r->get_reflected_wave_down = &get_reflected_wave_for_diode_pair;
        r->get_port_resistance = &get_port_resistance_for_root;
    }
}
//WDF
void tWDF_init(tWDF** const wdf, WDFComponentType type, Lfloat value, tWDF** const rL, tWDF** const rR, LEAF* const leaf)
{
    tWDF_initToPool(wdf, type, value, rL, rR, &leaf->mempool);
}

void    tWDF_initToPool(tWDF** const wdf, WDFComponentType type, Lfloat value, tWDF** const rL, tWDF** const rR, tMempool** const mp)
{
    tMempool* m = *mp;
    *wdf = (tWDF*) mpool_alloc(sizeof(tWDF), m);
    
    wdf_init(wdf, type, value, rL, rR);
}

void    tWDF_free (tWDF** const wdf)
{
    tWDF* r = *wdf;
    
    mpool_free((char*)r, r->mempool);
}

Lfloat tWDF_tick(tWDF* const r, Lfloat sample, tWDF* const outputPoint, uint8_t paramsChanged)
{
    tWDF* child;
    if (r->child_left != NULL) child = r->child_left;
    else child = r->child_right;
    
    //step 0 : update port resistances if something changed
    if (paramsChanged) tWDF_getPortResistance(r);

    //step 1 : set inputs to what they should be
    Lfloat input = sample;

    //step 2 : scan the waves up the tree
    r->incident_wave_up = tWDF_getReflectedWaveUp(child, input);

    //step 3 : do root scattering computation
    r->reflected_wave_up = tWDF_getReflectedWaveDown(r, input, r->incident_wave_up);

    //step 4 : propogate waves down the tree
    tWDF_setIncidentWave(child, r->reflected_wave_up, input);

    //step 5 : grab whatever voltages or currents we want as outputs
    return tWDF_getVoltage(outputPoint);
}

void tWDF_setValue(tWDF* const r, Lfloat value)
{
    r->value = value;
}

void tWDF_setSampleRate(tWDF* const r, Lfloat sample_rate)
{
    r->sample_rate = sample_rate;
    if (r->type == Capacitor)
    {
        r->port_conductance_up = r->sample_rate * 2.0f * r->value;
        r->port_resistance_up = 1.0f / r->port_conductance_up; //based on trapezoidal discretization
    }
    else if (r->type == Inductor)
    {
        r->port_resistance_up = r->sample_rate * 2.0f * r->value; //based on trapezoidal discretization
        r->port_conductance_up = 1.0f / r->port_resistance_up;
    }
}

uint8_t tWDF_isLeaf(tWDF* const r)
{
    if (r->child_left == NULL && r->child_right == NULL) return 1;
    return 0;
}

Lfloat tWDF_getPortResistance(tWDF* const r)
{
    return r->get_port_resistance(r);
}

void tWDF_setIncidentWave(tWDF* const r, Lfloat incident_wave, Lfloat input)
{
    r->set_incident_wave(r, incident_wave, input);
}

Lfloat tWDF_getReflectedWaveUp(tWDF* const r, Lfloat input)
{
    return r->get_reflected_wave_up(r, input);
}

Lfloat tWDF_getReflectedWaveDown(tWDF* const r, Lfloat input, Lfloat incident_wave)
{
    return r->get_reflected_wave_down(r, input, incident_wave);
}

Lfloat tWDF_getVoltage(tWDF* const r)
{
    return ((r->incident_wave_up * 0.5f) + (r->reflected_wave_up * 0.5f));
}

Lfloat tWDF_getCurrent(tWDF* const r)
{
    return (((r->incident_wave_up * 0.5f) - (r->reflected_wave_up * 0.5f)) * r->port_conductance_up);
}

//============ Static Functions to be Pointed To ====================
//===================================================================
//============ Get and Calculate Port Resistances ===================

static Lfloat get_port_resistance_for_resistor(tWDF* const r)
{
    r->port_resistance_up = r->value;
    r->port_conductance_up = 1.0f / r->value;

    return r->port_resistance_up;
}

static Lfloat get_port_resistance_for_capacitor(tWDF* const r)
{
    r->port_conductance_up = r->sample_rate * 2.0f * r->value; //based on trapezoidal discretization
    r->port_resistance_up = (1.0f / r->port_conductance_up);

    return r->port_resistance_up;
}

static Lfloat get_port_resistance_for_inductor(tWDF* const r)
{
    r->port_resistance_up = r->sample_rate * 2.0f * r->value; //based on trapezoidal discretization
    r->port_conductance_up = (1.0f / r->port_resistance_up);
    
    return r->port_resistance_up;
}

static Lfloat get_port_resistance_for_resistive(tWDF* const r)
{
    r->port_resistance_up = r->value;
    r->port_conductance_up = 1.0f / r->port_resistance_up;
    
    return r->port_resistance_up;
}

static Lfloat get_port_resistance_for_inverter(tWDF* const r)
{
    r->port_resistance_up = tWDF_getPortResistance(r->child_left);
    r->port_conductance_up = 1.0f / r->port_resistance_up;
    
    return r->port_resistance_up;
}

static Lfloat get_port_resistance_for_series(tWDF* const r)
{
    r->port_resistance_left = tWDF_getPortResistance(r->child_left);
    r->port_resistance_right = tWDF_getPortResistance(r->child_right);
    r->port_resistance_up = r->port_resistance_left + r->port_resistance_right;
    r->port_conductance_up  = 1.0f / r->port_resistance_up;
    r->port_conductance_left = 1.0f / r->port_resistance_left;
    r->port_conductance_right = 1.0f / r->port_resistance_right;
    r->gamma_zero = 1.0f / (r->port_resistance_right + r->port_resistance_left);

    return r->port_resistance_up;
}

static Lfloat get_port_resistance_for_parallel(tWDF* const r)
{
    r->port_resistance_left = tWDF_getPortResistance(r->child_left);
    r->port_resistance_right = tWDF_getPortResistance(r->child_right);
    r->port_resistance_up = (r->port_resistance_left * r->port_resistance_right) / (r->port_resistance_left + r->port_resistance_right);
    r->port_conductance_up  = 1.0f / r->port_resistance_up;
    r->port_conductance_left = 1.0f / r->port_resistance_left;
    r->port_conductance_right = 1.0f / r->port_resistance_right;
    r->gamma_zero = 1.0f / (r->port_conductance_right + r->port_conductance_left);

    return r->port_resistance_up;
}

static Lfloat get_port_resistance_for_root(tWDF* const r)
{
    tWDF* child;
    if (r->child_left != NULL) child = r->child_left;
    else child = r->child_right;
    
    r->port_resistance_up = tWDF_getPortResistance(child);
    r->port_conductance_up = 1.0f / r->port_resistance_up;
    
    return r->port_resistance_up;
}

//===================================================================
//================ Set Incident Waves ===============================

static void set_incident_wave_for_leaf(tWDF* const r, Lfloat incident_wave, Lfloat input)
{
    r->incident_wave_up = incident_wave;
}

static void set_incident_wave_for_leaf_inverted(tWDF* const r, Lfloat incident_wave, Lfloat input)
{
    r->incident_wave_up = -1.0f * incident_wave;
}

static void set_incident_wave_for_inverter(tWDF* const r, Lfloat incident_wave, Lfloat input)
{
    r->incident_wave_up = incident_wave;
    tWDF_setIncidentWave(r->child_left, -1.0f * incident_wave, input);
}

static void set_incident_wave_for_series(tWDF* const r, Lfloat incident_wave, Lfloat input)
{
    r->incident_wave_up = incident_wave;
    Lfloat gamma_left = r->port_resistance_left * r->gamma_zero;
    Lfloat gamma_right = r->port_resistance_right * r->gamma_zero;
    Lfloat left_wave = tWDF_getReflectedWaveUp(r->child_left, input);
    Lfloat right_wave = tWDF_getReflectedWaveUp(r->child_right, input);
//    downPorts[0]->b = yl * ( downPorts[0]->a * ((1.0 / yl) - 1) - downPorts[1]->a - descendingWave );
//    downPorts[1]->b = yr * ( downPorts[1]->a * ((1.0 / yr) - 1) - downPorts[0]->a - descendingWave );
    tWDF_setIncidentWave(r->child_left, (-1.0f * gamma_left * incident_wave) + (gamma_right * left_wave) - (gamma_left * right_wave), input);
    tWDF_setIncidentWave(r->child_right, (-1.0f * gamma_right * incident_wave) + (gamma_left * right_wave) - (gamma_right * left_wave), input);
    // From rt-wdf
//  tWDF_setIncidentWave(r->child_left, gamma_left * (left_wave * ((1.0f / gamma_left) - 1.0f) - right_wave - incident_wave));
//  tWDF_setIncidentWave(r->child_right, gamma_right * (right_wave * ((1.0f / gamma_right) - 1.0f) - left_wave - incident_wave));

}

static void set_incident_wave_for_parallel(tWDF* const r, Lfloat incident_wave, Lfloat input)
{
    r->incident_wave_up = incident_wave;
    Lfloat gamma_left = r->port_conductance_left * r->gamma_zero;
    Lfloat gamma_right = r->port_conductance_right * r->gamma_zero;
    Lfloat left_wave = tWDF_getReflectedWaveUp(r->child_left, input);
    Lfloat right_wave = tWDF_getReflectedWaveUp(r->child_right, input);
//    downPorts[0]->b = ( ( dl - 1 ) * downPorts[0]->a + dr * downPorts[1]->a + du * descendingWave );
//    downPorts[1]->b = ( dl * downPorts[0]->a + ( dr - 1 ) * downPorts[1]->a + du * descendingWave );
    tWDF_setIncidentWave(r->child_left, (gamma_left - 1.0f) * left_wave + gamma_right * right_wave + incident_wave, input);
    tWDF_setIncidentWave(r->child_right, gamma_left * left_wave + (gamma_right - 1.0f) * right_wave + incident_wave, input);
}

//===================================================================
//================ Get Reflected Waves ==============================

static Lfloat get_reflected_wave_for_resistor(tWDF* const r, Lfloat input)
{
    r->reflected_wave_up = 0.0f;
    return r->reflected_wave_up;
}

static Lfloat get_reflected_wave_for_capacitor(tWDF* const r, Lfloat input)
{
    r->reflected_wave_up = r->incident_wave_up;
    return r->reflected_wave_up;
}

static Lfloat get_reflected_wave_for_resistive(tWDF* const r, Lfloat input)
{
    r->reflected_wave_up = input;
    return r->reflected_wave_up;
}

static Lfloat get_reflected_wave_for_inverter(tWDF* const r, Lfloat input)
{
    r->reflected_wave_up = -1.0f * tWDF_getReflectedWaveUp(r->child_left, input);
    return r->reflected_wave_up;
}

static Lfloat get_reflected_wave_for_series(tWDF* const r, Lfloat input)
{
    //-( downPorts[0]->a + downPorts[1]->a );
    r->reflected_wave_up = (-1.0f * (tWDF_getReflectedWaveUp(r->child_left, input) + tWDF_getReflectedWaveUp(r->child_right, input)));
    return r->reflected_wave_up;
}

static Lfloat get_reflected_wave_for_parallel(tWDF* const r, Lfloat input)
{
    Lfloat gamma_left = r->port_conductance_left * r->gamma_zero;
    Lfloat gamma_right = r->port_conductance_right * r->gamma_zero;
    //return ( dl * downPorts[0]->a + dr * downPorts[1]->a );
    r->reflected_wave_up = (gamma_left * tWDF_getReflectedWaveUp(r->child_left, input) + gamma_right * tWDF_getReflectedWaveUp(r->child_right, input));
    return r->reflected_wave_up;
}

static Lfloat get_reflected_wave_for_ideal(tWDF* const wdf, Lfloat input, Lfloat incident_wave)
{
    return (2.0f * input) - incident_wave;
}


#define wX1 -3.684303659906469f
#define wX2 1.972967391708859f
#define wA  9.451797158780131e-3f
#define wB  0.1126446405111627f
#define wY  0.4451353886588814f
#define wK  0.5836596684310648f
static Lfloat wrightOmega3(Lfloat x)
{
    if (x <= wX1)
    {
        return 0;
    }
    else if (x < wX2)
    {
        return (wA * x*x*x) + (wB * x*x) + (wY * x) + wK;
    }
    else
    {
        return x - logf(x);
    }
}

static Lfloat wrightOmegaApproximation(Lfloat x)
{
    Lfloat w3 = wrightOmega3(x);
    return w3 - ((w3 - expf(x - w3)) / (w3 + 1.0f));
}

static Lfloat lambertW(Lfloat a, Lfloat r, Lfloat I, Lfloat iVT)
{
    return wrightOmegaApproximation(((a + r*I) * iVT) + logf((r * I) * iVT));
}

#define Is_DIODE    2.52e-9f
#define VT_DIODE    0.02585f
static Lfloat get_reflected_wave_for_diode(tWDF* const n, Lfloat input, Lfloat incident_wave)
{
    Lfloat a = incident_wave;
    Lfloat r = n->port_resistance_up;
    return a + 2.0f*r*Is_DIODE - 2.0f*VT_DIODE*lambertW(a, r, Is_DIODE, 1.0f/VT_DIODE);
}

static Lfloat get_reflected_wave_for_diode_pair(tWDF* const n, Lfloat input, Lfloat incident_wave)
{
    Lfloat a = incident_wave;
    Lfloat sgn = 0.0f;
    if (a > 0.0f) sgn = 1.0f;
    else if (a < 0.0f) sgn = -1.0f;
    Lfloat r = n->port_resistance_up;
    return a + 2 * sgn * (r*Is_DIODE - VT_DIODE*lambertW(sgn*a, r, Is_DIODE, 1.0f/VT_DIODE));
}
