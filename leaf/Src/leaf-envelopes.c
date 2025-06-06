/*
  ==============================================================================

    leaf-envelopes.c
    Created: 20 Jan 2017 12:02:17pm
    Author:  Michael R Mulshine

  ==============================================================================
*/


#if _WIN32 || _WIN64

#include "..\Inc\leaf-envelopes.h"
#include "..\Inc\leaf-tables.h"
#include "..\leaf.h"

#else

#include "../Inc/leaf-envelopes.h"
#include "../Inc/leaf-tables.h"
#include "../leaf.h"

#endif

#if LEAF_INCLUDE_ADSR_TABLES

// ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ Envelope ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ //
void tEnvelope_init(tEnvelope** const envlp, Lfloat attack, Lfloat decay, int loop, LEAF *const leaf)
{
    tEnvelope_initToPool(envlp, attack, decay, loop, &leaf->mempool);
}

void tEnvelope_initToPool (tEnvelope** const envlp, Lfloat attack, Lfloat decay, int loop, tMempool** const mp)
{
    tMempool *m = *mp;
    tEnvelope *env = *envlp = (tEnvelope *) mpool_alloc(sizeof(tEnvelope), m);
    env->mempool = m;

    env->exp_buff = __leaf_table_exp_decay;
    env->inc_buff = __leaf_table_attack_decay_inc;
    env->buff_size = sizeof(__leaf_table_exp_decay);

    env->loop = loop;

    if (attack > 8192.0f)
        attack = 8192.0f;
    if (attack < 0.0f)
        attack = 0.0f;

    if (decay > 8192.0f)
        decay = 8192.0f;
    if (decay < 0.0f)
        decay = 0.0f;

    int16_t attackIndex = ((int16_t) (attack * 8.0f)) - 1;
    int16_t decayIndex = ((int16_t) (decay * 8.0f)) - 1;
    int16_t rampIndex = ((int16_t) (2.0f * 8.0f)) - 1;

    if (attackIndex < 0)
        attackIndex = 0;
    if (decayIndex < 0)
        decayIndex = 0;
    if (rampIndex < 0)
        rampIndex = 0;

    env->inRamp = 0;
    env->inAttack = 0;
    env->inDecay = 0;

    env->attackInc = env->inc_buff[attackIndex];
    env->decayInc = env->inc_buff[decayIndex];
    env->rampInc = env->inc_buff[rampIndex];
}

void tEnvelope_free (tEnvelope** const envlp)
{
    tEnvelope *env = *envlp;
    mpool_free((char *) env, env->mempool);
}

void tEnvelope_setAttack (tEnvelope* const env, Lfloat attack)
{
    int32_t attackIndex;

    if (attack < 0.0f) {
        attackIndex = 0.0f;
    } else if (attack < 8192.0f) {
        attackIndex = ((int32_t) (attack * 8.0f)) - 1;
    } else {
        attackIndex = ((int32_t) (8192.0f * 8.0f)) - 1;
    }

    env->attackInc = env->inc_buff[attackIndex];
}

void tEnvelope_setDecay (tEnvelope* const env, Lfloat decay)
{
    int32_t decayIndex;

    if (decay < 0.0f) {
        decayIndex = 0;
    } else if (decay < 8192.0f) {
        decayIndex = ((int32_t) (decay * 8.0f)) - 1;
    } else {
        decayIndex = ((int32_t) (8192.0f * 8.0f)) - 1;
    }

    env->decayInc = env->inc_buff[decayIndex];
}

void tEnvelope_loop (tEnvelope* const env, int loop)
{
    env->loop = loop;
}

void tEnvelope_on (tEnvelope* const env, Lfloat velocity)
{
    if (env->inAttack || env->inDecay) // In case envelope retriggered while it is still happening.
    {
        env->rampPhase = 0;
        env->inRamp = 1;
        env->rampPeak = env->next;
    } else // Normal start.
    {
        env->inAttack = 1;
    }


    env->attackPhase = 0;
    env->decayPhase = 0;
    env->inDecay = 0;
    env->gain = velocity;
}

Lfloat tEnvelope_tick (tEnvelope* const env)
{
    if (env->inRamp) {
        if (env->rampPhase > UINT16_MAX) {
            env->inRamp = 0;
            env->inAttack = 1;
            env->next = 0.0f;
        } else {
            env->next = env->rampPeak * env->exp_buff[(uint32_t) env->rampPhase];
        }

        env->rampPhase += env->rampInc;
    }

    if (env->inAttack) {

        // If attack done, time to turn around.
        if (env->attackPhase > UINT16_MAX) {
            env->inDecay = 1;
            env->inAttack = 0;
            env->next = env->gain * 1.0f;
        } else {
            // do interpolation !
            env->next = env->gain * env->exp_buff[UINT16_MAX -
                                                  (uint32_t) env->attackPhase]; // inverted and backwards to get proper rising exponential shape/perception
        }

        // Increment envelope attack.
        env->attackPhase += env->attackInc;

    }

    if (env->inDecay) {

        // If decay done, finish.
        if (env->decayPhase >= UINT16_MAX) {
            env->inDecay = 0;

            if (env->loop) {
                env->attackPhase = 0;
                env->decayPhase = 0;
                env->inAttack = 1;
            } else {
                env->next = 0.0f;
            }

        } else {

            env->next = env->gain * (env->exp_buff[(uint32_t) env->decayPhase]); // do interpolation !
        }

        // Increment envelope decay;
        env->decayPhase += env->decayInc;
    }

    return env->next;
}

#endif // LEAF_INCLUDE_ADSR_TABLES

#if LEAF_INCLUDE_ADSR_TABLES

/* ADSR */
void tADSR_init(tADSR** const adsrenv, Lfloat attack, Lfloat decay, Lfloat sustain,
                 Lfloat release, LEAF *const leaf)
{
    tADSR_initToPool(adsrenv, attack, decay, sustain, release, &leaf->mempool);
}

void tADSR_initToPool (tADSR** const adsrenv, Lfloat attack, Lfloat decay,
                       Lfloat sustain, Lfloat release, tMempool** const mp)
{
    tMempool *m = *mp;
    tADSR *adsr = *adsrenv = (tADSR *) mpool_alloc(sizeof(tADSR), m);
    adsr->mempool = m;

    adsr->exp_buff = __leaf_table_exp_decay;
    adsr->inc_buff = __leaf_table_attack_decay_inc;
    adsr->buff_size = sizeof(__leaf_table_exp_decay);

    if (attack > 8192.0f)
        attack = 8192.0f;
    if (attack < 0.0f)
        attack = 0.0f;
    adsr->attack = attack;

    if (decay > 8192.0f)
        decay = 8192.0f;
    if (decay < 0.0f)
        decay = 0.0f;
    adsr->decay = decay;

    if (sustain > 1.0f)
        sustain = 1.0f;
    if (sustain < 0.0f)
        sustain = 0.0f;

    if (release > 8192.0f)
        release = 8192.0f;
    if (release < 0.0f)
        release = 0.0f;
    adsr->release = release;

    int16_t attackIndex = ((int16_t) (attack * 8.0f)) - 1;
    int16_t decayIndex = ((int16_t) (decay * 8.0f)) - 1;
    int16_t releaseIndex = ((int16_t) (release * 8.0f)) - 1;
    int16_t rampIndex = ((int16_t) (2.0f * 8.0f)) - 1;

    if (attackIndex < 0)
        attackIndex = 0;
    if (decayIndex < 0)
        decayIndex = 0;
    if (releaseIndex < 0)
        releaseIndex = 0;
    if (rampIndex < 0)
        rampIndex = 0;

    adsr->next = 0.0f;

    adsr->inRamp = 0;
    adsr->inAttack = 0;
    adsr->inDecay = 0;
    adsr->inSustain = 0;
    adsr->inRelease = 0;

    adsr->sustain = sustain;

    adsr->attackInc = adsr->inc_buff[attackIndex];
    adsr->decayInc = adsr->inc_buff[decayIndex];
    adsr->releaseInc = adsr->inc_buff[releaseIndex];
    adsr->rampInc = adsr->inc_buff[rampIndex];

    adsr->baseLeakFactor = 1.0f;
    adsr->leakFactor = 1.0f;
    adsr->invSampleRate = adsr->mempool->leaf->invSampleRate;
}

void tADSR_free (tADSR** const adsrenv)
{
    tADSR *adsr = *adsrenv;
    mpool_free((char *) adsr, adsr->mempool);
}

void tADSR_setAttack (tADSR* const adsr, Lfloat attack)
{
    int32_t attackIndex;

    adsr->attack = attack;

    if (attack < 0.0f) {
        attackIndex = 0.0f;
    } else if (attack < 8192.0f) {
        attackIndex = ((int32_t) (attack * 8.0f)) - 1;
    } else {
        attackIndex = ((int32_t) (8192.0f * 8.0f)) - 1;
    }

    adsr->attackInc = adsr->inc_buff[attackIndex] * (44100.f * adsr->invSampleRate);
}

void tADSR_setDecay (tADSR* const adsr, Lfloat decay)
{
    int32_t decayIndex;

    adsr->decay = decay;

    if (decay < 0.0f) {
        decayIndex = 0.0f;
    } else if (decay < 8192.0f) {
        decayIndex = ((int32_t) (decay * 8.0f)) - 1;
    } else {
        decayIndex = ((int32_t) (8192.0f * 8.0f)) - 1;
    }

    adsr->decayInc = adsr->inc_buff[decayIndex] * (44100.f * adsr->invSampleRate);
}

void tADSR_setSustain (tADSR* const adsr, Lfloat sustain)
{
    if (sustain > 1.0f) adsr->sustain = 1.0f;
    else if (sustain < 0.0f) adsr->sustain = 0.0f;
    else adsr->sustain = sustain;
}

void tADSR_setRelease (tADSR* const adsr, Lfloat release)
{
    int32_t releaseIndex;

    adsr->release = release;

    if (release < 0.0f) {
        releaseIndex = 0.0f;
    } else if (release < 8192.0f) {
        releaseIndex = ((int32_t) (release * 8.0f)) - 1;
    } else {
        releaseIndex = ((int32_t) (8192.0f * 8.0f)) - 1;
    }

    adsr->releaseInc = adsr->inc_buff[releaseIndex] * (44100.f * adsr->invSampleRate);
}

// 0.999999 is slow leak, 0.9 is fast leak
void tADSR_setLeakFactor (tADSR* const adsr, Lfloat leakFactor)
{
    adsr->baseLeakFactor = leakFactor;
    adsr->leakFactor = powf(leakFactor, 44100.0f * adsr->invSampleRate);
}

void tADSR_on (tADSR* const adsr, Lfloat velocity)
{
    if ((adsr->inAttack || adsr->inDecay) ||
        (adsr->inSustain || adsr->inRelease)) // In case ADSR retriggered while it is still happening.
    {
        adsr->rampPhase = 0;
        adsr->inRamp = 1;
        adsr->rampPeak = adsr->next;
    } else // Normal start.
    {
        adsr->inAttack = 1;
    }

    adsr->attackPhase = 0;
    adsr->decayPhase = 0;
    adsr->releasePhase = 0;
    adsr->inDecay = 0;
    adsr->inSustain = 0;
    adsr->inRelease = 0;
    adsr->gain = velocity;
}

void tADSR_off (tADSR* const adsr)
{
    if (adsr->inRelease) return;

    adsr->inAttack = 0;
    adsr->inDecay = 0;
    adsr->inSustain = 0;
    adsr->inRelease = 1;

    adsr->releasePeak = adsr->next;
}

Lfloat tADSR_tick(tADSR* const adsr)
{
    if (adsr->inRamp) {
        if (adsr->rampPhase > UINT16_MAX) {
            adsr->inRamp = 0;
            adsr->inAttack = 1;
            adsr->next = 0.0f;
        } else {
            adsr->next = adsr->rampPeak * adsr->exp_buff[(uint32_t) adsr->rampPhase];
        }

        adsr->rampPhase += adsr->rampInc;
    }

    if (adsr->inAttack) {

        // If attack done, time to turn around.
        if (adsr->attackPhase > UINT16_MAX) {
            adsr->inDecay = 1;
            adsr->inAttack = 0;
            adsr->next = adsr->gain * 1.0f;
        } else {
            // do interpolation !
            adsr->next = adsr->gain * adsr->exp_buff[UINT16_MAX -
                                                     (uint32_t) adsr->attackPhase]; // inverted and backwards to get proper rising exponential shape/perception
        }

        // Increment ADSR attack.
        adsr->attackPhase += adsr->attackInc;

    }

    if (adsr->inDecay) {

        // If decay done, sustain.
        if (adsr->decayPhase >= UINT16_MAX) {
            adsr->inDecay = 0;
            adsr->inSustain = 1;
            adsr->next = adsr->gain * adsr->sustain;
        } else {
            adsr->next = (adsr->gain *
                          (adsr->sustain + ((adsr->exp_buff[(uint32_t) adsr->decayPhase]) * (1.0f - adsr->sustain)))) *
                         adsr->leakFactor; // do interpolation !
        }

        // Increment ADSR decay.
        adsr->decayPhase += adsr->decayInc;
    }

    if (adsr->inSustain) {
        adsr->next = adsr->next * adsr->leakFactor;
    }

    if (adsr->inRelease) {
        // If release done, finish.
        if (adsr->releasePhase >= UINT16_MAX) {
            adsr->inRelease = 0;
            adsr->next = 0.0f;
        } else {

            adsr->next = adsr->releasePeak * (adsr->exp_buff[(uint32_t) adsr->releasePhase]); // do interpolation !
        }

        // Increment envelope release;
        adsr->releasePhase += adsr->releaseInc;
    }


    return adsr->next;
}

void tADSR_setSampleRate (tADSR* const adsr, Lfloat sr)
{
    adsr->invSampleRate = 1.0f / sr;

    tADSR_setAttack(adsr, adsr->attack);
    tADSR_setDecay(adsr, adsr->decay);
    tADSR_setRelease(adsr, adsr->release);
    tADSR_setLeakFactor(adsr, adsr->baseLeakFactor);
}

#endif // LEAF_INCLUDE_ADSR_TABLES


/* ADSR 3*/
//This one doesn't use any lookup table - by Nigel Redmon from his blog. Thanks, Nigel!
//-JS

Lfloat calcADSR3Coef (Lfloat rate, Lfloat targetRatio)
{
    return (rate <= 0.0f) ? 0.0f : expf(-logf((1.0f + targetRatio) / targetRatio) / rate);
}

void tADSRS_init(tADSRS** const adsrenv, Lfloat attack, Lfloat decay, Lfloat sustain,
                  Lfloat release, LEAF *const leaf)
{
    tADSRS_initToPool(adsrenv, attack, decay, sustain, release, &leaf->mempool);
}

void tADSRS_initToPool (tADSRS** const adsrenv, Lfloat attack, Lfloat decay,
                        Lfloat sustain, Lfloat release,
                       tMempool** const mp)
{
    tMempool *m = *mp;
    tADSRS *adsr = *adsrenv = (tADSRS *) mpool_alloc(sizeof(tADSRS), m);
    adsr->mempool = m;

    LEAF *leaf = adsr->mempool->leaf;

    adsr->sampleRate = leaf->sampleRate;
    adsr->sampleRateInMs = adsr->sampleRate * 0.001f;
    adsr->targetRatioA = 0.3f;
    adsr->targetRatioDR = 0.0001f;
    adsr->attack = attack;
    adsr->attackRate = attack * adsr->sampleRateInMs;
    adsr->attackCoef = calcADSR3Coef(attack * adsr->sampleRateInMs, adsr->targetRatioA);
    adsr->attackBase = (1.0f + adsr->targetRatioA) * (1.0f - adsr->attackCoef);

    adsr->decay = decay;
    adsr->decayRate = decay * adsr->sampleRateInMs;
    adsr->decayCoef = calcADSR3Coef(decay * adsr->sampleRateInMs, adsr->targetRatioDR);
    adsr->decayBase = (adsr->sustainLevel - adsr->targetRatioDR) * (1.0f - adsr->decayCoef);

    adsr->sustainLevel = sustain;
    adsr->decayBase = (adsr->sustainLevel - adsr->targetRatioDR) * (1.0f - adsr->decayCoef);

    adsr->release = release;
    adsr->releaseRate = release * adsr->sampleRateInMs;
    adsr->releaseCoef = calcADSR3Coef(release * adsr->sampleRateInMs, adsr->targetRatioDR);
    adsr->releaseBase = -adsr->targetRatioDR * (1.0f - adsr->releaseCoef);

    adsr->state = env_idle;
    adsr->gain = 1.0f;
    adsr->targetGainSquared = 1.0f;
    adsr->factor = 0.01f;
    adsr->oneMinusFactor = 0.99f;
    adsr->output = 0.0f;

    adsr->baseLeakFactor = 1.0f;
    adsr->leakFactor = 1.0f;

    adsr->invSampleRate = leaf->invSampleRate;
}

void tADSRS_free(tADSRS** const adsrenv) {
    tADSRS *adsr = *adsrenv;
    mpool_free((char *) adsr, adsr->mempool);
}

void tADSRS_setAttack(tADSRS* const adsr, Lfloat attack)
{
    adsr->attack = attack;
    adsr->attackRate = attack * adsr->sampleRateInMs;
    adsr->attackCoef = calcADSR3Coef(adsr->attackRate, adsr->targetRatioA);
    adsr->attackBase = (1.0f + adsr->targetRatioA) * (1.0f - adsr->attackCoef);
}

void tADSRS_setDecay (tADSRS* const adsr, Lfloat decay)
{
    adsr->decay = decay;
    adsr->decayRate = decay * adsr->sampleRateInMs;
    adsr->decayCoef = calcADSR3Coef(adsr->decayRate, adsr->targetRatioDR);
    adsr->decayBase = (adsr->sustainLevel - adsr->targetRatioDR) * (1.0f - adsr->decayCoef);
}

void tADSRS_setSustain (tADSRS* const adsr, Lfloat sustain)
{
    adsr->sustainLevel = sustain;
    adsr->decayBase = (adsr->sustainLevel - adsr->targetRatioDR) * (1.0f - adsr->decayCoef);
}

void tADSRS_setRelease(tADSRS* const adsr, Lfloat release)
{
    adsr->release = release;
    adsr->releaseRate = release * adsr->sampleRateInMs;
    adsr->releaseCoef = calcADSR3Coef(adsr->releaseRate, (Lfloat) adsr->targetRatioDR);
    adsr->releaseBase = -adsr->targetRatioDR * (1.0f - adsr->releaseCoef);
}

// 0.999999 is slow leak, 0.9 is fast leak
void tADSRS_setLeakFactor (tADSRS* const adsr, Lfloat leakFactor)
{
    adsr->baseLeakFactor = leakFactor;
    adsr->leakFactor = powf(leakFactor, 44100.0f * adsr->invSampleRate);
}

void tADSRS_on (tADSRS* const adsr, Lfloat velocity)
{
    adsr->state = env_attack;
    adsr->targetGainSquared = velocity * velocity;
}

void tADSRS_off (tADSRS* const adsr)
{
    if (adsr->state != env_idle) {
        adsr->state = env_release;
    }
}

Lfloat tADSRS_tick (tADSRS* const adsr)
{
    switch (adsr->state) {
        case env_idle:
            break;
        case env_attack:
            adsr->output = adsr->attackBase + adsr->output * adsr->attackCoef;
            if (adsr->output >= 1.0f) {
                adsr->output = 1.0f;
                adsr->state = env_decay;
            }
            break;
        case env_decay:
            adsr->output = adsr->decayBase + adsr->output * adsr->decayCoef * adsr->leakFactor;
            if (adsr->output <= adsr->sustainLevel) {
                adsr->output = adsr->sustainLevel;
                adsr->state = env_sustain;
            }
            break;
        case env_sustain:
            adsr->output = adsr->output * adsr->leakFactor;
            break;
        case env_release:
            adsr->output = adsr->releaseBase + adsr->output * adsr->releaseCoef;
            if (adsr->output <= 0.0f) {
                adsr->output = 0.0f;
                adsr->state = env_idle;
            }
        default:
            break;
    }
    //smooth the gain value   -- this is not ideal, a retrigger while the envelope is still going with a new gain will cause a jump, although it will be smoothed quickly. Maybe doing the math so the range is computed based on the gain rather than 0.->1. is preferable? But that's harder to get the exponential curve right without a lookup.
    adsr->gain = (adsr->factor * adsr->targetGainSquared) + (adsr->oneMinusFactor * adsr->gain);
    return adsr->output * adsr->gain;
}

void tADSRS_setSampleRate (tADSRS* const adsr, Lfloat sr)
{
    adsr->sampleRate = sr;
    adsr->sampleRateInMs = adsr->sampleRate * 0.001f;
    adsr->invSampleRate = 1.0f / sr;

    tADSRS_setAttack(adsr, adsr->attack);
    tADSRS_setDecay(adsr, adsr->decay);
    tADSRS_setRelease(adsr, adsr->release);
    tADSRS_setLeakFactor(adsr, adsr->baseLeakFactor);
}

//================================================================================

/* ADSR 4 */ // new version of our original table-based ADSR but with the table passed in by the user
// use this if the size of the big ADSR tables is too much.
void tADSRT_init(tADSRT** const adsrenv, Lfloat attack, Lfloat decay, Lfloat sustain,
                  Lfloat release, Lfloat *expBuffer, int bufferSize, LEAF *const leaf)
{
    tADSRT_initToPool(adsrenv, attack, decay, sustain, release, expBuffer,
                      bufferSize, &leaf->mempool);
}

//initialize with an exponential function that decays -- i.e. a call to LEAF_generate_exp(expBuffer, 0.001f, 0.0f, 1.0f, -0.0008f, EXP_BUFFER_SIZE);
//times are in ms
void
tADSRT_initToPool(tADSRT** const adsrenv, Lfloat attack, Lfloat decay, Lfloat sustain,
                  Lfloat release, Lfloat *expBuffer, int bufferSize, tMempool** const mp)
{
    tMempool *m = *mp;
    tADSRT *adsr = *adsrenv = (tADSRT *) mpool_alloc(sizeof(tADSRT), m);
    adsr->mempool = m;

    LEAF *leaf = adsr->mempool->leaf;

    adsr->exp_buff = expBuffer;
    adsr->buff_size = bufferSize;
    adsr->buff_sizeMinusOne = bufferSize - 1;

    adsr->sampleRate = leaf->sampleRate;
    adsr->bufferSizeDividedBySampleRateInMs = adsr->buff_size / (adsr->sampleRate * 0.001f);

    if (attack < 0.0f)
        attack = 0.0f;

    if (decay < 0.0f)
        decay = 0.0f;

    if (sustain > 1.0f)
        sustain = 1.0f;
    if (sustain < 0.0f)
        sustain = 0.0f;

    if (release < 0.0f)
        release = 0.0f;

    adsr->next = 0.0f;

    adsr->whichStage = env_idle;

    adsr->sustain = sustain;

    adsr->attack = attack;
    adsr->decay = decay;
    adsr->release = release;
    adsr->attackInc = adsr->bufferSizeDividedBySampleRateInMs / attack;
    adsr->decayInc = adsr->bufferSizeDividedBySampleRateInMs / decay;
    adsr->releaseInc = adsr->bufferSizeDividedBySampleRateInMs / release;
    adsr->rampInc = adsr->bufferSizeDividedBySampleRateInMs / 8.0f;

    adsr->baseLeakFactor = 1.0f;
    adsr->leakFactor = 1.0f;
    adsr->invSampleRate = leaf->invSampleRate;
}

void tADSRT_free (tADSRT** const adsrenv)
{
    tADSRT *adsr = *adsrenv;
    mpool_free((char *) adsr, adsr->mempool);
}

#ifdef ITCMRAM
void __attribute__ ((section(".itcmram"))) __attribute__ ((aligned (32)))  tADSRT_setAttack(tADSRT* const adsrenv, Lfloat attack)
#else

void tADSRT_setAttack (tADSRT* const adsr, Lfloat attack)
#endif
{
    if (attack < 0.0f) {
        attack = 0.01f;
    }
    adsr->attack = attack;
    adsr->attackInc = adsr->bufferSizeDividedBySampleRateInMs / attack;
}

#ifdef ITCMRAM
void __attribute__ ((section(".itcmram"))) __attribute__ ((aligned (32)))  tADSRT_setDecay(tADSRT* const adsrenv, Lfloat decay)
#else

void tADSRT_setDecay (tADSRT* const adsr, Lfloat decay)
#endif
{
    if (decay < 0.0f) {
        decay = 0.01f;
    }
    adsr->decay = decay;
    adsr->decayInc = adsr->bufferSizeDividedBySampleRateInMs / decay;
}

#ifdef ITCMRAM
void __attribute__ ((section(".itcmram"))) __attribute__ ((aligned (32))) tADSRT_setSustain(tADSRT* const adsrenv, Lfloat sustain)
#else

void tADSRT_setSustain (tADSRT* const adsr, Lfloat sustain)
#endif
{
    if (sustain > 1.0f) adsr->sustain = 1.0f;
    else if (sustain < 0.0f) adsr->sustain = 0.0f;
    else adsr->sustain = sustain;
}

#ifdef ITCMRAM
void __attribute__ ((section(".itcmram"))) __attribute__ ((aligned (32))) tADSRT_setRelease(tADSRT* const adsrenv, Lfloat release)
#else

void tADSRT_setRelease (tADSRT* const adsr, Lfloat release)
#endif
{

    if (release < 0.0f) {
        release = 0.01f;
    }
    adsr->release = release;
    adsr->releaseInc = adsr->bufferSizeDividedBySampleRateInMs / release;
}

// 0.999999 is slow leak, 0.9 is fast leak
#ifdef ITCMRAM
void __attribute__ ((section(".itcmram"))) __attribute__ ((aligned (32))) tADSRT_setLeakFactor(tADSRT* const adsrenv, Lfloat leakFactor)
#else

void tADSRT_setLeakFactor (tADSRT* const adsr, Lfloat leakFactor)
#endif
{
    adsr->baseLeakFactor = leakFactor;
    adsr->leakFactor = powf(leakFactor, 44100.0f * adsr->invSampleRate);;
}

#ifdef ITCMRAM
void __attribute__ ((section(".itcmram"))) __attribute__ ((aligned (32))) tADSRT_on(tADSRT* const adsrenv, Lfloat velocity)
#else

void tADSRT_on (tADSRT* const adsr, Lfloat velocity)
#endif
{
    if (adsr->whichStage != env_idle) // In case ADSR retriggered while it is still happening.
    {
        adsr->rampPhase = 0;
        adsr->whichStage = env_ramp;
        adsr->rampPeak = adsr->next;
    } else // Normal start.
    {
        adsr->whichStage = env_attack;
    }

    adsr->attackPhase = 0;
    adsr->decayPhase = 0;
    adsr->releasePhase = 0;
    adsr->gain = velocity;
}

#ifdef ITCMRAM
void __attribute__ ((section(".itcmram"))) __attribute__ ((aligned (32))) tADSRT_off(tADSRT* const adsrenv)
#else

void tADSRT_off (tADSRT* const adsr)
#endif
{
    if (adsr->whichStage == env_idle) {
        return;
    } else {
        adsr->whichStage = env_release;
        adsr->releasePeak = adsr->next;
    }
}

#ifdef ITCMRAM
void __attribute__ ((section(".itcmram"))) __attribute__ ((aligned (32))) tADSRT_clear(tADSRT* const adsrenv)
#else

void tADSRT_clear (tADSRT* const adsr)
#endif
{
    adsr->whichStage = env_idle;
    adsr->next = 0.0f;
}

#ifdef ITCMRAM
Lfloat  __attribute__ ((section(".itcmram"))) __attribute__ ((aligned (32)))   tADSRT_tick(tADSRT* const adsrenv)
#else

Lfloat tADSRT_tick (tADSRT* const adsr)
#endif
{
    switch (adsr->whichStage) {
        case env_ramp:
            if (adsr->rampPhase > adsr->buff_sizeMinusOne) {
                adsr->whichStage = env_attack;
                adsr->next = 0.0f;
            } else {
                uint32_t intPart = (uint32_t) adsr->rampPhase;
                Lfloat LfloatPart = adsr->rampPhase - intPart;
                Lfloat secondValue;
                if (adsr->rampPhase + 1.0f > adsr->buff_sizeMinusOne) {
                    secondValue = 0.0f;
                } else {
                    secondValue = adsr->exp_buff[(uint32_t) ((adsr->rampPhase) + 1)];
                }
                adsr->next =
                        adsr->rampPeak * LEAF_interpolation_linear(adsr->exp_buff[intPart], secondValue, LfloatPart);
            }

            adsr->rampPhase += adsr->rampInc;
            break;


        case env_attack:

            // If attack done, time to turn around.
            if (adsr->attackPhase > adsr->buff_sizeMinusOne) {
                adsr->whichStage = env_decay;
                adsr->next = adsr->gain;
            } else {
                // do interpolation !
                uint32_t intPart = (uint32_t) adsr->attackPhase;
                Lfloat LfloatPart = adsr->attackPhase - intPart;
                Lfloat secondValue;
                if (adsr->attackPhase + 1.0f > adsr->buff_sizeMinusOne) {
                    secondValue = 0.0f;
                } else {
                    secondValue = adsr->exp_buff[(uint32_t) ((adsr->attackPhase) + 1)];
                }

                adsr->next = adsr->gain * (1.0f - LEAF_interpolation_linear(adsr->exp_buff[intPart], secondValue,
                                                                            LfloatPart)); // inverted and backwards to get proper rising exponential shape/perception
            }

            // Increment ADSR attack.
            adsr->attackPhase += adsr->attackInc;
            break;

        case env_decay:

            // If decay done, sustain.
            if (adsr->decayPhase > adsr->buff_sizeMinusOne) {
                adsr->whichStage = env_sustain;
                adsr->next = adsr->gain * adsr->sustain;
                adsr->sustainWithLeak = 1.0f;
            } else {
                uint32_t intPart = (uint32_t) adsr->decayPhase;
                Lfloat LfloatPart = adsr->decayPhase - intPart;
                Lfloat secondValue;
                if (adsr->decayPhase + 1.0f > adsr->buff_sizeMinusOne) {
                    secondValue = 0.0f;
                } else {
                    secondValue = adsr->exp_buff[(uint32_t) ((adsr->decayPhase) + 1)];
                }
                Lfloat interpValue = (LEAF_interpolation_linear(adsr->exp_buff[intPart], secondValue, LfloatPart));
                adsr->next = (adsr->gain * (adsr->sustain + (interpValue * (1.0f - adsr->sustain)))) *
                             adsr->leakFactor; // do interpolation !
            }

            // Increment ADSR decay.
            adsr->decayPhase += adsr->decayInc;
            break;

        case env_sustain:
            adsr->next = adsr->sustain * adsr->gain * (adsr->leakFactor * adsr->sustainWithLeak);
            break;

        case env_release:
            // If release done, finish.
            if (adsr->releasePhase > adsr->buff_sizeMinusOne) {
                adsr->whichStage = env_idle;
                adsr->next = 0.0f;
            } else {
                uint32_t intPart = (uint32_t) adsr->releasePhase;
                Lfloat LfloatPart = adsr->releasePhase - intPart;
                Lfloat secondValue;
                if (adsr->releasePhase + 1.0f > adsr->buff_sizeMinusOne) {
                    secondValue = 0.0f;
                } else {
                    secondValue = adsr->exp_buff[(uint32_t) ((adsr->releasePhase) + 1)];
                }
                adsr->next = adsr->releasePeak * (LEAF_interpolation_linear(adsr->exp_buff[intPart], secondValue,
                                                                            LfloatPart)); // do interpolation !
            }

            // Increment envelope release;
            adsr->releasePhase += adsr->releaseInc;
            break;
    }
    return adsr->next;
}

#ifdef ITCMRAM
Lfloat  __attribute__ ((section(".itcmram"))) __attribute__ ((aligned (32)))  tADSRT_tickNoInterp(tADSRT* const adsrenv)
#else

Lfloat tADSRT_tickNoInterp (tADSRT* const adsr)
#endif
{
    switch (adsr->whichStage) {
        case env_ramp:
            if (adsr->rampPhase > adsr->buff_sizeMinusOne) {
                adsr->whichStage = env_attack;
                adsr->next = 0.0f;
            } else {
                adsr->next = adsr->rampPeak * adsr->exp_buff[(uint32_t) adsr->rampPhase];
            }

            adsr->rampPhase += adsr->rampInc;
            break;


        case env_attack:

            // If attack done, time to turn around.
            if (adsr->attackPhase > adsr->buff_sizeMinusOne) {
                adsr->whichStage = env_decay;
                adsr->next = adsr->gain;
            } else {
                adsr->next = adsr->gain * (1.0f -
                                           adsr->exp_buff[(uint32_t) adsr->attackPhase]); // inverted and backwards to get proper rising exponential shape/perception
            }

            // Increment ADSR attack.
            adsr->attackPhase += adsr->attackInc;
            break;

        case env_decay:

            // If decay done, sustain.
            if (adsr->decayPhase > adsr->buff_sizeMinusOne) {
                adsr->whichStage = env_sustain;
                adsr->next = adsr->gain * adsr->sustain;
                adsr->sustainWithLeak = 1.0f;
            } else {
                adsr->next = (adsr->gain * (adsr->sustain +
                                            (adsr->exp_buff[(uint32_t) adsr->decayPhase] * (1.0f - adsr->sustain)))) *
                             adsr->leakFactor;
            }

            // Increment ADSR decay.
            adsr->decayPhase += adsr->decayInc;
            break;

        case env_sustain:
            adsr->sustainWithLeak *= adsr->leakFactor;
            adsr->next = adsr->sustain * adsr->gain * adsr->sustainWithLeak;
            break;

        case env_release:
            // If release done, finish.
            if (adsr->releasePhase > adsr->buff_sizeMinusOne) {
                adsr->whichStage = env_idle;
                adsr->next = 0.0f;
            } else {
                adsr->next = adsr->releasePeak * adsr->exp_buff[(uint32_t) adsr->releasePhase];
            }

            // Increment envelope release;
            adsr->releasePhase += adsr->releaseInc;
            break;
    }
    return adsr->next;
}

#ifdef ITCMRAM
void  __attribute__ ((section(".itcmram"))) __attribute__ ((aligned (32)))  tADSRT_setSampleRate(tADSRT* const adsrenv, Lfloat sr)
#else

void tADSRT_setSampleRate (tADSRT* const adsr, Lfloat sr)
#endif
{
    adsr->sampleRate = sr;
    adsr->invSampleRate = 1.0f / sr;
    adsr->bufferSizeDividedBySampleRateInMs = adsr->buff_size / (adsr->sampleRate * 0.001f);
    adsr->attackInc = adsr->bufferSizeDividedBySampleRateInMs / adsr->attack;
    adsr->decayInc = adsr->bufferSizeDividedBySampleRateInMs / adsr->decay;
    adsr->releaseInc = adsr->bufferSizeDividedBySampleRateInMs / adsr->release;
    adsr->rampInc = adsr->bufferSizeDividedBySampleRateInMs / 8.0f;
    adsr->leakFactor = powf(adsr->baseLeakFactor, 44100.0f * adsr->invSampleRate);
}

/////-----------------
/* Ramp */
void tRamp_init(tRamp** const r, Lfloat time, int samples_per_tick, LEAF *const leaf)
{
    tRamp_initToPool(r, time, samples_per_tick, &leaf->mempool);
}

void tRamp_initToPool (tRamp** const r, Lfloat time, int samples_per_tick,
                       tMempool** const mp)
{
    tMempool *m = *mp;
    tRamp *ramp = *r = (tRamp *) mpool_alloc(sizeof(tRamp), m);
    ramp->mempool = m;

    LEAF *leaf = ramp->mempool->leaf;

    ramp->sampleRate = leaf->sampleRate;
    ramp->inv_sr_ms = 1.0f / (ramp->sampleRate * 0.001f);
    ramp->minimum_time = ramp->inv_sr_ms * samples_per_tick;
    ramp->curr = 0.0f;
    ramp->dest = 0.0f;

    if (time < ramp->minimum_time) {
        ramp->time = ramp->minimum_time;
    } else {
        ramp->time = time;
    }
    ramp->samples_per_tick = samples_per_tick;
    ramp->factor = (1.0f / ramp->time) * ramp->inv_sr_ms * (Lfloat) ramp->samples_per_tick;
    ramp->inc = (ramp->dest - ramp->curr) * ramp->factor;
}

void tRamp_free (tRamp** const r)
{
    tRamp *ramp = *r;
    mpool_free((char *) ramp, ramp->mempool);
}

void tRamp_setTime (tRamp* const r, Lfloat time)
{
    if (time < r->minimum_time) {
        r->time = r->minimum_time;
    } else {
        r->time = time;
    }
    r->factor = (1.0f / r->time) * r->inv_sr_ms * (Lfloat) r->samples_per_tick;
    r->inc = (r->dest - r->curr) * r->factor;

}

void tRamp_setDest (tRamp* const r, Lfloat dest)
{
    r->dest = dest;
    r->inc = (r->dest - r->curr) * r->factor;
}

void tRamp_setVal (tRamp* const r, Lfloat val)
{
    r->curr = val;
    r->inc = (r->dest - r->curr) * r->factor;
}

Lfloat tRamp_tick (tRamp* const r)
{
    r->curr += r->inc;

    if (((r->curr >= r->dest) && (r->inc > 0.0f)) || ((r->curr <= r->dest) && (r->inc < 0.0f))) {
        r->inc = 0.0f;
        r->curr = r->dest;
    }

    return r->curr;
}

Lfloat tRamp_sample (tRamp* const r)
{
    return r->curr;
}

void tRamp_setSampleRate (tRamp* const r, Lfloat sr)
{
    r->sampleRate = sr;
    r->inv_sr_ms = 1.0f / (r->sampleRate * 0.001f);
    r->factor = (1.0f / r->time) * r->inv_sr_ms * (Lfloat) r->samples_per_tick;
    r->inc = (r->dest - r->curr) * r->factor;
}

//===========================================================================================

/* RampUpDown */
void tRampUpDown_init(tRampUpDown** const r, Lfloat upTime, Lfloat downTime,
                       int samples_per_tick, LEAF *const leaf)
{
    tRampUpDown_initToPool(r, upTime, downTime, samples_per_tick, &leaf->mempool);
}

void tRampUpDown_initToPool (tRampUpDown** const r, Lfloat upTime, Lfloat downTime,
                             int samples_per_tick, tMempool** const mp)
{
    tMempool *m = *mp;
    tRampUpDown *ramp = *r = (tRampUpDown *) mpool_alloc(sizeof(tRampUpDown), m);
    ramp->mempool = m;

    LEAF *leaf = ramp->mempool->leaf;

    ramp->sampleRate = leaf->sampleRate;
    ramp->inv_sr_ms = 1.0f / (ramp->sampleRate * 0.001f);
    ramp->minimum_time = ramp->inv_sr_ms * samples_per_tick;
    ramp->curr = 0.0f;
    ramp->dest = 0.0f;

    if (upTime < ramp->minimum_time) {
        ramp->upTime = ramp->minimum_time;
    } else {
        ramp->upTime = upTime;
    }

    if (downTime < ramp->minimum_time) {
        ramp->downTime = ramp->minimum_time;
    } else {
        ramp->downTime = downTime;
    }

    ramp->samples_per_tick = samples_per_tick;
    ramp->upInc = ((ramp->dest - ramp->curr) / ramp->upTime * ramp->inv_sr_ms) * (Lfloat) ramp->samples_per_tick;
    ramp->downInc = ((ramp->dest - ramp->curr) / ramp->downTime * ramp->inv_sr_ms) * (Lfloat) ramp->samples_per_tick;
}

void tRampUpDown_free (tRampUpDown** const r)
{
    tRampUpDown *ramp = *r;
    mpool_free((char *) ramp, ramp->mempool);
}

void tRampUpDown_setUpTime (tRampUpDown* const r, Lfloat upTime)
{
    if (upTime < r->minimum_time) {
        r->upTime = r->minimum_time;
    } else {
        r->upTime = upTime;
    }
    r->upInc = ((r->dest - r->curr) / r->upTime * r->inv_sr_ms) * (Lfloat) r->samples_per_tick;
}


void tRampUpDown_setDownTime (tRampUpDown* const r, Lfloat downTime)
{
    if (downTime < r->minimum_time) {
        r->downTime = r->minimum_time;
    } else {
        r->downTime = downTime;
    }
    r->downInc = ((r->dest - r->curr) / r->downTime * r->inv_sr_ms) * (Lfloat) r->samples_per_tick;
}

void tRampUpDown_setDest (tRampUpDown* const r, Lfloat dest)
{
    r->dest = dest;
    r->upInc = ((r->dest - r->curr) / r->upTime * r->inv_sr_ms) * (Lfloat) r->samples_per_tick;
    r->downInc = ((r->dest - r->curr) / r->downTime * r->inv_sr_ms) * (Lfloat) r->samples_per_tick;
}

void tRampUpDown_setVal (tRampUpDown* const r, Lfloat val)
{
    r->curr = val;
    r->upInc = ((r->dest - r->curr) / r->upTime * r->inv_sr_ms) * (Lfloat) r->samples_per_tick;
    r->downInc = ((r->dest - r->curr) / r->downTime * r->inv_sr_ms) * (Lfloat) r->samples_per_tick;
}

Lfloat tRampUpDown_tick (tRampUpDown* const r)
{
    Lfloat test;

    if (r->dest < r->curr) {
        test = r->curr + r->downInc;
        if (test > r->dest) {
            r->curr = test;
        } else {
            r->downInc = 0.0f;
            r->curr = r->dest;
        }
    } else if (r->dest > r->curr) {
        test = r->curr + r->upInc;
        if (test < r->dest) {
            r->curr = test;
        } else {
            r->upInc = 0.0f;
            r->curr = r->dest;
        }
    }
    return r->curr;
}

Lfloat tRampUpDown_sample (tRampUpDown* const r)
{
    return r->curr;
}


/* Exponential Smoother */
void tExpSmooth_init(tExpSmooth** const expsmooth, Lfloat val, Lfloat factor,
                     LEAF *const leaf)  // factor is usually a value between 0 and 0.1. Lower value is slower. 0.01 for example gives you a smoothing time of about 10ms
{
    tExpSmooth_initToPool(expsmooth, val, factor, &leaf->mempool);
}

void tExpSmooth_initToPool(tExpSmooth** const expsmooth, Lfloat val, Lfloat factor,
                           tMempool** const mp)
{
    tMempool *m = *mp;
    tExpSmooth *smooth = *expsmooth = (tExpSmooth *) mpool_alloc(sizeof(tExpSmooth), m);
    smooth->mempool = m;

    smooth->curr = val;
    smooth->dest = val;
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;
    //smooth->baseFactor = factor;
    smooth->factor = factor;
    smooth->oneminusfactor = 1.0f - factor;
    //smooth->invSampleRate = smooth->mempool->leaf->invSampleRate;
}

void tExpSmooth_free (tExpSmooth** const expsmooth)
{
    tExpSmooth *smooth = *expsmooth;
    mpool_free((char *) smooth, smooth->mempool);
}

// factor is usually a value between 0 and 0.1. Lower value is slower. 0.01 for example gives you a smoothing time of about 10ms
void tExpSmooth_setFactor (tExpSmooth* const smooth, Lfloat factor)
{
    if (factor < 0.0f)
        factor = 0.0f;
    else if (factor > 1.0f) factor = 1.0f;
    //smooth->baseFactor = factor;
    //smooth->factor = powf(factor, 44100.f * smooth->invSampleRate);
    smooth->factor = factor;
    smooth->oneminusfactor = 1.0f - smooth->factor;
}

#ifdef ITCMRAM
void __attribute__ ((section(".itcmram"))) __attribute__ ((aligned (32))) tExpSmooth_setDest(tExpSmooth* const expsmooth, Lfloat dest)
#else

void tExpSmooth_setDest (tExpSmooth* const smooth, Lfloat dest)
#endif
{
    smooth->dest = dest;
}

void tExpSmooth_setVal (tExpSmooth* const smooth, Lfloat val)
{
    smooth->curr = val;
}

void tExpSmooth_setValAndDest (tExpSmooth* const smooth, Lfloat val)
{
    smooth->curr = val;
    smooth->dest = val;
}

#ifdef ITCMRAM
Lfloat __attribute__ ((section(".itcmram"))) __attribute__ ((aligned (32))) tExpSmooth_tick(tExpSmooth* const expsmooth)
#else

Lfloat tExpSmooth_tick (tExpSmooth* const smooth)
#endif
{
    smooth->curr = smooth->factor * smooth->dest + smooth->oneminusfactor * smooth->curr;
    return smooth->curr;
}

Lfloat tExpSmooth_sample (tExpSmooth* const smooth)
{
    return smooth->curr;
}

void tExpSmooth_setSampleRate (tExpSmooth* const smooth, Lfloat sr)
{
    //smooth->invSampleRate = 1.0f/sr;
    //smooth->factor = powf(smooth->baseFactor, 44100.f * smooth->invSampleRate);
    //smooth->oneminusfactor = 1.0f - smooth->factor;
}

//tSlide is based on the max/msp slide~ object
////

void tSlide_init(tSlide** const sl, Lfloat upSlide, Lfloat downSlide, LEAF *const leaf)
{
    tSlide_initToPool(sl, upSlide, downSlide, &leaf->mempool);
}

//upslide and downslide are in samples
void tSlide_initToPool (tSlide** const sl, Lfloat upSlide, Lfloat downSlide, tMempool** const mp)
{
    tMempool *m = *mp;
    tSlide *s = *sl = (tSlide *) mpool_alloc(sizeof(tSlide), m);
    s->mempool = m;

    s->prevIn = 0.0f;
    s->currentOut = 0.0f;
    s->prevOut = 0.0f;
    s->dest = 0.0f;
    if (upSlide < 1.0f) {
        upSlide = 1.0f;
    }

    if (downSlide < 1.0f) {
        downSlide = 1.0f;
    }
    s->invUpSlide = 1.0f / upSlide;
    s->invDownSlide = 1.0f / downSlide;
}

void tSlide_free (tSlide** const sl)
{
    tSlide *s = *sl;
    mpool_free((char *) s, s->mempool);
}

void tSlide_setUpSlide (tSlide* const s, Lfloat upSlide)
{
    s->invUpSlide = 1.0f / upSlide;
}

void tSlide_setDownSlide (tSlide* const s, Lfloat downSlide)
{
    s->invDownSlide = 1.0f / downSlide;
}

void tSlide_setDest (tSlide* const s, Lfloat dest)
{
    s->dest = dest;
}

Lfloat tSlide_tickNoInput (tSlide* const s)
{
    Lfloat in = s->dest;

    if (in >= s->prevOut) {
        s->currentOut = s->prevOut + ((in - s->prevOut) * s->invUpSlide);
    } else {
        s->currentOut = s->prevOut + ((in - s->prevOut) * s->invDownSlide);
    }
#ifdef NO_DENORMAL_CHECK
#else
    if (s->currentOut < VSF) s->currentOut = 0.0f;
#endif
    s->prevIn = in;
    s->prevOut = s->currentOut;
    return s->currentOut;
}

Lfloat tSlide_tick (tSlide* const s, Lfloat in)
{
    if (in >= s->prevOut) {
        s->currentOut = s->prevOut + ((in - s->prevOut) * s->invUpSlide);
    } else {
        s->currentOut = s->prevOut + ((in - s->prevOut) * s->invDownSlide);
    }
#ifdef NO_DENORMAL_CHECK
#else
    if (s->currentOut < VSF) s->currentOut = 0.0f;
#endif
    s->prevIn = in;
    s->prevOut = s->currentOut;
    return s->currentOut;
}

