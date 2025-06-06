/*==============================================================================
 
 leaf-analysis.c
 Created: 30 Nov 2018 11:56:49am
 Author:  airship
 
 ==============================================================================*/

#if _WIN32 || _WIN64

#include "..\Inc\leaf-analysis.h"
#include "..\Externals\d_fft_mayer.h"
#include <intrin.h>
#else

#include "../Inc/leaf-analysis.h"
#include "../Externals/d_fft_mayer.h"

#endif

#if LEAF_DEBUG
#include "../../TestPlugin/JuceLibraryCode/JuceHeader.h"
#endif

/******************************************************************************/
/*                            Envelope Follower                               */
/******************************************************************************/

void tEnvelopeFollower_init(tEnvelopeFollower** const ef, Lfloat attackThreshold, Lfloat decayCoeff, LEAF* const leaf)
{
    tEnvelopeFollower_initToPool(ef, attackThreshold, decayCoeff, &leaf->mempool);
}

void tEnvelopeFollower_initToPool (tEnvelopeFollower** const ef, Lfloat attackThreshold, Lfloat decayCoeff,
                                         tMempool** const mp)
{
    tMempool* m = *mp;
    tEnvelopeFollower* e = *ef = (tEnvelopeFollower*) mpool_alloc(sizeof(tEnvelopeFollower), m);
    e->mempool = m;
    
    e->y = 0.0f;
    e->a_thresh = attackThreshold;
    e->d_coeff = decayCoeff;
}

void tEnvelopeFollower_free (tEnvelopeFollower** const ef)
{
    tEnvelopeFollower* e = *ef;
    
    mpool_free((char*)e, e->mempool);
}

Lfloat tEnvelopeFollower_tick (tEnvelopeFollower* const e, Lfloat x)
{
    if (x < 0.0f ) x = -x;  /* Absolute value. */
    
    if (isnan(x)) return 0.0f;
    if ((x >= e->y) && (x > e->a_thresh)) e->y = x; /* If we hit a peak, ride the peak to the top. */
    else e->y = e->y * e->d_coeff; /* Else, exponential decay of output. */
    
    //ef->y = envelope_pow[(uint16_t)(ef->y * (Lfloat)UINT16_MAX)] * ef->d_coeff; //not quite the right behavior - too much loss of precision?
    //ef->y = powf(ef->y, 1.000009f) * ef->d_coeff;  // too expensive
    
#ifdef NO_DENORMAL_CHECK
#else
    if( e->y < VSF)   e->y = 0.0f;
#endif
    return e->y;
}

void tEnvelopeFollower_setDecayCoefficient (tEnvelopeFollower* const e, Lfloat decayCoeff)
{
    e->d_coeff = decayCoeff;
}

void tEnvelopeFollower_setAttackThreshold (tEnvelopeFollower* const e, Lfloat attackThresh)
{
    e->a_thresh = attackThresh;
}


/******************************************************************************/
/*                          Zero Crossing Counter                             */
/******************************************************************************/


void tZeroCrossingCounter_init(tZeroCrossingCounter** const zc, int maxWindowSize, LEAF* const leaf)
{
    tZeroCrossingCounter_initToPool   (zc, maxWindowSize, &leaf->mempool);
}

void tZeroCrossingCounter_initToPool (tZeroCrossingCounter** const zc, int maxWindowSize, tMempool** const mp)
{
    tMempool* m = *mp;
    tZeroCrossingCounter* z = *zc = (tZeroCrossingCounter*) mpool_alloc(sizeof(tZeroCrossingCounter), m);
    z->mempool = m;
    
    z->count = 0;
    z->maxWindowSize = maxWindowSize;
    z->currentWindowSize = maxWindowSize;
    z->invCurrentWindowSize = 1.0f / (Lfloat)maxWindowSize;
    z->position = 0;
    z->prevPosition = maxWindowSize;
    z->inBuffer = (Lfloat*) mpool_calloc(sizeof(Lfloat) * maxWindowSize, m);
    z->countBuffer = (uint16_t*) mpool_calloc(sizeof(uint16_t) * maxWindowSize, m);
}

void tZeroCrossingCounter_free (tZeroCrossingCounter** const zc)
{
    tZeroCrossingCounter* z = *zc;
    
    mpool_free((char*)z->inBuffer, z->mempool);
    mpool_free((char*)z->countBuffer, z->mempool);
    mpool_free((char*)z, z->mempool);
}

//returns proportion of zero crossings within window size (0.0 would be none in window, 1.0 would be all zero crossings)
Lfloat tZeroCrossingCounter_tick (tZeroCrossingCounter* const z, Lfloat input)
{
    z->inBuffer[z->position] = input;
    int futurePosition = ((z->position + 1) % z->currentWindowSize);
    Lfloat output = 0.0f;
    
    //add new value to count
    if ((z->inBuffer[z->position] * z->inBuffer[z->prevPosition]) < 0.0f)
    {
        //zero crossing happened, add it to the count array
        z->countBuffer[z->position] = 1;
        z->count++;
    }
    else
    {
        z->countBuffer[z->position] = 0;
    }
    
    //remove oldest value from count
    if (z->countBuffer[futurePosition] > 0)
    {
        z->count--;
        if (z->count < 0)
        {
            z->count = 0;
        }
    }
    
    z->prevPosition = z->position;
    z->position = futurePosition;
    
    output = z->count * z->invCurrentWindowSize;
    
    return output;
}

void tZeroCrossingCounter_setWindowSize (tZeroCrossingCounter* const z, Lfloat windowSize)
{
    if (windowSize <= z->maxWindowSize)
    {
        z->currentWindowSize = windowSize;
    }
    else
    {
        z->currentWindowSize = z->maxWindowSize;
    }
    z->invCurrentWindowSize = 1.0f / z->currentWindowSize;
}


/******************************************************************************/
/*                             Power Follower                                 */
/******************************************************************************/


void tPowerFollower_init(tPowerFollower** const pf, Lfloat factor, LEAF* const leaf)
{
    tPowerFollower_initToPool(pf, factor, &leaf->mempool);
}

void tPowerFollower_initToPool (tPowerFollower** const pf, Lfloat factor, tMempool** const mp)
{
    tMempool* m = *mp;
    tPowerFollower* p = *pf = (tPowerFollower*) mpool_alloc(sizeof(tPowerFollower), m);
    p->mempool = m;
    
    p->curr=0.0f;
    p->factor=factor;
    p->oneminusfactor=1.0f-factor;
}

void tPowerFollower_free (tPowerFollower** const pf)
{
    tPowerFollower* p = *pf;
    
    mpool_free((char*)p, p->mempool);
}

void tPowerFollower_setFactor (tPowerFollower* const p, Lfloat factor)
{
    if (factor<0.0f) factor=0.0f;
    if (factor>1.0f) factor=1.0f;
    p->factor=factor;
    p->oneminusfactor=1.0f-factor;
}

Lfloat tPowerFollower_tick (tPowerFollower* const p, Lfloat input)
{
    p->curr = p->factor*input*input+p->oneminusfactor*p->curr;
    return p->curr;
}

Lfloat tPowerFollower_getPower (tPowerFollower* const p)
{
    return p->curr;
}


/******************************************************************************/
/*                         Simple Envelope Follower                           */
/******************************************************************************/


void tEnvPD_init(tEnvPD** const xpd, int ws, int hs, int bs, LEAF* const leaf)
{
    tEnvPD_initToPool(xpd, ws, hs, bs, &leaf->mempool);
}

void tEnvPD_initToPool (tEnvPD** const xpd, int ws, int hs, int bs, tMempool** const mp)
{
    tMempool* m = *mp;
    tEnvPD* x = *xpd = (tEnvPD*) mpool_calloc(sizeof(tEnvPD), m);
    x->mempool = m;
    
    int period = hs, npoints = ws;
    
    int i;
    
    if (npoints < 1) npoints = 1024;
    if (period < 1) period = npoints/2;
    if (period < npoints / MAXOVERLAP + 1)
        period = npoints / MAXOVERLAP + 1;
    
    x->x_npoints = npoints;
    x->x_phase = 0;
    x->x_period = period;
    
    x->windowSize = npoints;
    x->hopSize = period;
    x->blockSize = bs;
    
    for (i = 0; i < MAXOVERLAP; i++) x->x_sumbuf[i] = 0.0f;
    for (i = 0; i < npoints; i++)
    x->buf[i] = (1.0f - cosf((2.0f * PI * i) / npoints))/npoints;
    for (; i < npoints+INITVSTAKEN; i++) x->buf[i] = 0.0f;
    
    x->x_f = 0;
    
    x->x_allocforvs = INITVSTAKEN;

    if (x->x_period % x->blockSize)
    {
        x->x_realperiod = x->x_period + x->blockSize - (x->x_period % x->blockSize);
    }
    else
    {
        x->x_realperiod = x->x_period;
    }
}

void tEnvPD_free (tEnvPD** const xpd)
{
    tEnvPD* x = *xpd;
    
    mpool_free((char*)x, x->mempool);
}

Lfloat tEnvPD_tick (tEnvPD* const x)
{
    return powtodb(x->x_result);
}

void tEnvPD_processBlock(tEnvPD* const x, Lfloat* in)
{
    int n = x->blockSize;
    
    int count;
    t_sample *sump;
    in += n;
    for (count = x->x_phase, sump = x->x_sumbuf;
         count < x->x_npoints; count += x->x_realperiod, sump++)
    {
        t_sample *hp = x->buf + count;
        t_sample *fp = in;
        t_sample sum = *sump;
        int i;
        
        for (i = 0; i < n; i++)
        {
            fp--;
            sum += *hp++ * (*fp * *fp);
        }
        *sump = sum;
    }
    sump[0] = 0.0f;
    x->x_phase -= n;
    if (x->x_phase < 0)
    {
        x->x_result = x->x_sumbuf[0];
        for (count = x->x_realperiod, sump = x->x_sumbuf;
             count < x->x_npoints; count += x->x_realperiod, sump++)
        sump[0] = sump[1];
        sump[0] = 0.0f;
        x->x_phase = x->x_realperiod - n;
    }
}

/******************************************************************************/
/*                             Attack Detection                               */
/******************************************************************************/

/*********************** Static Function Declarations *************************/

static void atkdtk_init     (tAttackDetection* const a, int blocksize, int atk, int rel);
static void atkdtk_envelope (tAttackDetection* const a, Lfloat *in);

/******************************************************************************/

void tAttackDetection_init(tAttackDetection** const ad, int blocksize, int atk, int rel, LEAF* const leaf)
{
    tAttackDetection_initToPool(ad, blocksize, atk, rel, &leaf->mempool);
}

void tAttackDetection_initToPool (tAttackDetection** const ad, int blocksize, int atk, int rel, tMempool** const mp)
{
    tMempool* m = *mp;
    tAttackDetection* a = *ad = (tAttackDetection*) mpool_alloc(sizeof(tAttackDetection), m);
    a->mempool = m;
    
    atkdtk_init(a, blocksize, atk, rel);
}

void tAttackDetection_free (tAttackDetection** const ad)
{
    tAttackDetection* a = *ad;
    
    mpool_free((char*)a, a->mempool);
}

void tAttackDetection_setBlocksize (tAttackDetection* const a, int size)
{
    a->blockSize = size;
}

void tAttackDetection_setThreshold (tAttackDetection* const a, Lfloat thres)
{
    a->threshold = thres;
}

void tAttackDetection_setAttack (tAttackDetection* const a, int inAtk)
{
    a->atk = inAtk;
    a->atk_coeff = powf(0.01f, 1.0f/(a->atk * a->sampleRate * 0.001f));
}

void tAttackDetection_setRelease (tAttackDetection* const a, int inRel)
{
    a->rel = inRel;
    a->rel_coeff = powf(0.01f, 1.0f/(a->rel * a->sampleRate * 0.001f));
}

int tAttackDetection_detect (tAttackDetection* const a, Lfloat *in)
{
    int result;
    
    atkdtk_envelope(a, in);
    
    if(a->env >= a->prevAmp*2) //2 times greater = 6dB increase
        result = 1;
    else
        result = 0;
    
    a->prevAmp = a->env;
    
    return result;
}

void tAttackDetection_setSampleRate (tAttackDetection* const a, Lfloat sr)
{
    a->sampleRate = sr;
    
    tAttackDetection_setAttack(a, a->atk);
    tAttackDetection_setRelease(a, a->rel);
}

/************************ Static Function Definitions *************************/

static void atkdtk_init(tAttackDetection* const a, int blocksize, int atk, int rel)
{
    LEAF* leaf = a->mempool->leaf;
    
    a->env = 0;
    a->blockSize = blocksize;
    a->threshold = DEFTHRESHOLD;
    a->sampleRate = leaf->sampleRate;
    a->prevAmp = 0;
    
    a->env = 0;
    
    tAttackDetection_setAttack(a, atk);
    tAttackDetection_setRelease(a, rel);
}

static void atkdtk_envelope(tAttackDetection* const a, Lfloat *in)
{
    
    int i = 0;
    Lfloat tmp;
    for(i = 0; i < a->blockSize; ++i){
        tmp = fastabsf(in[i]);
        
        if(tmp > a->env)
            a->env = a->atk_coeff * (a->env - tmp) + tmp;
        else
            a->env = a->rel_coeff * (a->env - tmp) + tmp;
    }
}


#define REALFFT mayer_realfft
#define REALIFFT mayer_realifft

/******************************************************************************/
/*                                  SNAC                                      */
/******************************************************************************/

/*********************** Static Function Declarations *************************/

static  void   snac_analyzeframe      (tSNAC* const s);
static  void   snac_autocorrelation   (tSNAC* const s);
static  void   snac_normalize         (tSNAC* const s);
static  void   snac_pickpeak          (tSNAC* const s);
static  void   snac_periodandfidelity (tSNAC* const s);
static  void   snac_biasbuf           (tSNAC* const s);
static  Lfloat snac_spectralpeak      (tSNAC* const s, Lfloat periodlength);

/******************************************************************************/

void tSNAC_init(tSNAC** const snac, int overlaparg, LEAF* const leaf)
{
    tSNAC_initToPool(snac, overlaparg, &leaf->mempool);
}

void    tSNAC_initToPool    (tSNAC** const snac, int overlaparg, tMempool** const mp)
{
    tMempool* m = *mp;
    tSNAC* s = *snac = (tSNAC*) mpool_alloc(sizeof(tSNAC), m);
    s->mempool = m;
    
    s->biasfactor = DEFBIAS;
    s->timeindex = 0;
    s->periodindex = 0;
    s->periodlength = 0.;
    s->fidelity = 0.;
    s->minrms = DEFMINRMS;
    s->framesize = SNAC_FRAME_SIZE;
    
    s->inputbuf = (Lfloat*) mpool_calloc(sizeof(Lfloat) * SNAC_FRAME_SIZE, m);
    s->processbuf = (Lfloat*) mpool_calloc(sizeof(Lfloat) * (SNAC_FRAME_SIZE * 2), m);
    s->spectrumbuf = (Lfloat*) mpool_calloc(sizeof(Lfloat) * (SNAC_FRAME_SIZE / 2), m);
    s->biasbuf = (Lfloat*) mpool_calloc(sizeof(Lfloat) * SNAC_FRAME_SIZE, m);
    
    snac_biasbuf(s);
    tSNAC_setOverlap(s, overlaparg);
}

void tSNAC_free (tSNAC** const snac)
{
    tSNAC* s = *snac;
    
    mpool_free((char*)s->inputbuf, s->mempool);
    mpool_free((char*)s->processbuf, s->mempool);
    mpool_free((char*)s->spectrumbuf, s->mempool);
    mpool_free((char*)s->biasbuf, s->mempool);
    mpool_free((char*)s, s->mempool);
}

//void tSNAC_ioSamples(tSNAC* const snac, Lfloat *in, Lfloat *out, int size)
void tSNAC_ioSamples (tSNAC* const s, Lfloat *in, int size)
{
    int timeindex = s->timeindex;
    int mask = s->framesize - 1;
//    int outindex = 0;
    Lfloat *inputbuf = s->inputbuf;
//    Lfloat *processbuf = s->processbuf;
    
    // call analysis function when it is time
    if(!(timeindex & (s->framesize / s->overlap - 1))) snac_analyzeframe(s);
    
    while(size--)
    {
        inputbuf[timeindex] = *in++;
//        out[outindex++] = processbuf[timeindex++];
        timeindex++;
        timeindex &= mask;
    }
    s->timeindex = timeindex;
}

void tSNAC_setOverlap (tSNAC* const s, int lap)
{
    if(!((lap==1)|(lap==2)|(lap==4)|(lap==8))) lap = DEFOVERLAP;
    s->overlap = lap;
}

void tSNAC_setBias (tSNAC* const s, Lfloat bias)
{
    if(bias > 1.) bias = 1.;
    if(bias < 0.) bias = 0.;
    s->biasfactor = bias;
    snac_biasbuf(s);
    return;
}

void tSNAC_setMinRMS (tSNAC* const s, Lfloat rms)
{
    if(rms > 1.) rms = 1.;
    if(rms < 0.) rms = 0.;
    s->minrms = rms;
    return;
}

Lfloat tSNAC_getPeriod (tSNAC* const s)
{
    return(s->periodlength);
}

Lfloat tSNAC_getFidelity (tSNAC* const s)
{
    return(s->fidelity);
}


/************************ Static Function Definitions *************************/

// main analysis function
static void snac_analyzeframe(tSNAC* const s)
{
    int n, tindex = s->timeindex;
    int framesize = s->framesize;
    int mask = framesize - 1;
    Lfloat norm = 1.f / sqrtf((Lfloat)(framesize * 2));
    
    Lfloat *inputbuf = s->inputbuf;
    Lfloat *processbuf = s->processbuf;
    
    // copy input to processing buffers
    for(n=0; n<framesize; n++)
    {
        processbuf[n] = inputbuf[tindex] * norm;
        tindex++;
        tindex &= mask;
    }
    
    // zeropadding
    for(n=framesize; n<(framesize<<1); n++) processbuf[n] = 0.;
    
    // call analysis procedures
    snac_autocorrelation(s);
    snac_normalize(s);
    snac_pickpeak(s);
    snac_periodandfidelity(s);
}

static void snac_autocorrelation(tSNAC* const s)
{
    int n, m;
    int framesize = s->framesize;
    int fftsize = framesize * 2;
    Lfloat *processbuf = s->processbuf;
    Lfloat *spectrumbuf = s->spectrumbuf;
    
    REALFFT(fftsize, processbuf);
    
    // compute power spectrum
    processbuf[0] *= processbuf[0];                      // DC
    processbuf[framesize] *= processbuf[framesize];      // Nyquist
    
    for(n=1; n<framesize; n++)
    {
        processbuf[n] = processbuf[n] * processbuf[n]
        + processbuf[fftsize-n] * processbuf[fftsize-n]; // imag coefficients appear reversed
        processbuf[fftsize-n] = 0.f;
    }
    
    // store power spectrum up to SR/4 for possible later use
    for(m=0; m<(framesize>>1); m++)
    {
        spectrumbuf[m] = processbuf[m];
    }
    
    // transform power spectrum to autocorrelation function
    REALIFFT(fftsize, processbuf);
    return;
}

static void snac_normalize (tSNAC* const s)
{
    int framesize = s->framesize;
    int framesizeplustimeindex = s->framesize + s->timeindex;
    int timeindexminusone = s->timeindex - 1;
    int n, m;
    int mask = framesize - 1;
    int seek = framesize * SEEK;
    Lfloat *inputbuf = s->inputbuf;
    Lfloat *processbuf= s->processbuf;
    Lfloat signal1, signal2;
    
    // minimum RMS implemented as minimum autocorrelation at index 0
    // functionally equivalent to white noise floor
    Lfloat rms = s->minrms / sqrtf(1.0f / (Lfloat)framesize);
    Lfloat minrzero = rms * rms;
    Lfloat rzero = processbuf[0];
    if(rzero < minrzero) rzero = minrzero;
    double normintegral = (double)rzero * 2.;
    
    // normalize biased autocorrelation function
    // inputbuf is circular buffer: timeindex may be non-zero when overlap > 1
    processbuf[0] = 1;
    for(n=1, m=s->timeindex+1; n<seek; n++, m++)
    {
        signal1 = inputbuf[(n + timeindexminusone)&mask];
        signal2 = inputbuf[(framesizeplustimeindex - n)&mask]; //could this be switched to Lfloat resolution without issue? -JS
        normintegral -= (double)(signal1 * signal1 + signal2 * signal2);
        processbuf[n] /= (Lfloat)normintegral * 0.5f;
    }
    
    // flush instable function tail
    for(n = seek; n<framesize; n++) processbuf[n] = 0.;
    return;
}

static void snac_periodandfidelity (tSNAC* const s)
{
    Lfloat periodlength;
    
    if(s->periodindex)
    {
        periodlength = (Lfloat)s->periodindex +
        interpolate3phase(s->processbuf, s->periodindex);
        if(periodlength < 8) periodlength = snac_spectralpeak(s, periodlength);
        s->periodlength = periodlength;
        s->fidelity = interpolate3max(s->processbuf, s->periodindex);
    }
    return;
}

// select the peak which most probably represents period length
static void snac_pickpeak (tSNAC* const s)
{
    int n, peakindex=0;
    int seek = s->framesize * SEEK;
    Lfloat *processbuf= s->processbuf;
    Lfloat maxvalue = 0.;
    Lfloat biasedpeak;
    Lfloat *biasbuf = s->biasbuf;
    
    // skip main lobe
    for(n=1; n<seek; n++)
    {
        if(processbuf[n] < 0.) break;
    }
    
    // find interpolated / biased maximum in SNAC function
    // interpolation finds the 'real maximum'
    // biasing favours the first candidate
    for(; n<seek-1; n++)
    {
        if(processbuf[n] >= processbuf[n-1])
        {
            if(processbuf[n] > processbuf[n+1])     // we have a local peak
            {
                biasedpeak = interpolate3max(processbuf, n) * biasbuf[n];
                
                if(biasedpeak > maxvalue)
                {
                    maxvalue = biasedpeak;
                    peakindex = n;
                }
            }
        }
    }
    s->periodindex = peakindex;
    return;
}

// verify period length via frequency domain (up till SR/4)
// frequency domain is more precise than lag domain for period lengths < 8
// argument 'periodlength' is initial estimation from autocorrelation
static Lfloat snac_spectralpeak (tSNAC* const s, Lfloat periodlength)
{
    if(periodlength < 4.0f) return periodlength;
    
    Lfloat max = 0.;
    int n, startbin, stopbin, peakbin = 0;
    int spectrumsize = s->framesize>>1;
    Lfloat *spectrumbuf = s->spectrumbuf;
    Lfloat peaklocation = (Lfloat)(s->framesize * 2.0f) / periodlength;
    
    startbin = (int)(peaklocation * 0.8f + 0.5f);
    if(startbin < 1) startbin = 1;
    stopbin = (int)(peaklocation * 1.25f + 0.5f);
    if(stopbin >= spectrumsize - 1) stopbin = spectrumsize - 1;
    
    for(n=startbin; n<stopbin; n++)
    {
        if(spectrumbuf[n] >= spectrumbuf[n-1])
        {
            if(spectrumbuf[n] > spectrumbuf[n+1])
            {
                if(spectrumbuf[n] > max)
                {
                    max = spectrumbuf[n];
                    peakbin = n;
                }
            }
        }
    }
    
    // calculate amplitudes in peak region
    for(n=(peakbin-1); n<(peakbin+2); n++)
    {
        spectrumbuf[n] = sqrtf(spectrumbuf[n]);
    }
    
    peaklocation = (Lfloat)peakbin + interpolate3phase(spectrumbuf, peakbin);
    periodlength = (Lfloat)(s->framesize * 2.0f) / peaklocation;
    
    return periodlength;
}


// modified logarithmic bias function
static void snac_biasbuf (tSNAC* const s)
{
    int n;
    int maxperiod = (int)(s->framesize * (Lfloat)SEEK);
    Lfloat bias = s->biasfactor / logf((Lfloat)(maxperiod - 4));
    Lfloat *biasbuf = s->biasbuf;
    
    for(n=0; n<5; n++)    // periods < 5 samples can't be tracked
    {
        biasbuf[n] = 0.0f;
    }
    
    for(n=5; n<maxperiod; n++)
    {
        biasbuf[n] = 1.0f - (Lfloat)logf(n - 4.f) * bias;
    }
}

/******************************************************************************/
/*                             Period Detection                               */
/******************************************************************************/

void tPeriodDetection_init(tPeriodDetection** const pd, Lfloat* in, int bufSize, int frameSize, LEAF* const leaf)
{
    tPeriodDetection_initToPool(pd, in, bufSize, frameSize, &leaf->mempool);
}

void tPeriodDetection_initToPool (tPeriodDetection** const pd, Lfloat* in, int bufSize, int frameSize, tMempool** const mp)
{
    tMempool* m = *mp;
    tPeriodDetection* p = *pd = (tPeriodDetection*) mpool_calloc(sizeof(tPeriodDetection), m);
    p->mempool = m;
    LEAF* leaf = p->mempool->leaf;
    
    p->invSampleRate = leaf->invSampleRate;
    p->inBuffer = in;
    p->bufSize = bufSize;
    p->frameSize = frameSize;
    p->framesPerBuffer = p->bufSize / p->frameSize;
    p->curBlock = 1;
    p->lastBlock = 0;
    p->index = 0;
    
    p->hopSize = DEFHOPSIZE;
    p->windowSize = DEFWINDOWSIZE;
    p->fba = FBA;
    
    tEnvPD_initToPool(&p->env, p->windowSize, p->hopSize, p->frameSize, mp);
    
    tSNAC_initToPool(&p->snac, DEFOVERLAP, mp);
    
    p->history = 0.0f;
    p->alpha = 1.0f;
    p->tolerance = 1.0f;
    p->timeConstant = DEFTIMECONSTANT;
    p->radius = expf(-1000.0f * p->hopSize * p->invSampleRate / p->timeConstant);
    p->fidelityThreshold = 0.95f;
}

void tPeriodDetection_free (tPeriodDetection** const pd)
{
    tPeriodDetection* p = *pd;
    
    tEnvPD_free(&p->env);
    tSNAC_free(&p->snac);
    mpool_free((char*)p, p->mempool);
}

Lfloat tPeriodDetection_tick (tPeriodDetection* const p, Lfloat sample)
{
    int i, iLast;
    
    i = (p->curBlock*p->frameSize);
    iLast = (p->lastBlock*p->frameSize)+p->index;
    
    p->i = i;
    p->iLast = iLast;
    
    p->inBuffer[i+p->index] = sample;
    
    p->index++;
    p->indexstore = p->index;
    if (p->index >= p->frameSize)
    {
        p->index = 0;
        
        tEnvPD_processBlock(p->env, &(p->inBuffer[i]));
        
        tSNAC_ioSamples(p->snac, &(p->inBuffer[i]), p->frameSize);
        
        // Fidelity threshold recommended by Katja Vetters is 0.95 for most instruments/voices http://www.katjaas.nl/helmholtz/helmholtz.html
        p->period = tSNAC_getPeriod(p->snac);
        
        p->curBlock++;
        if (p->curBlock >= p->framesPerBuffer) p->curBlock = 0;
        p->lastBlock++;
        if (p->lastBlock >= p->framesPerBuffer) p->lastBlock = 0;
    }
    return p->period;
}

Lfloat tPeriodDetection_getPeriod(tPeriodDetection* const p)
{
    return p->period;
}

Lfloat tPeriodDetection_getFidelity(tPeriodDetection* const p)
{
    return tSNAC_getFidelity(p->snac);
}

void tPeriodDetection_setHopSize(tPeriodDetection* const p, int hs)
{
    p->hopSize = hs;
}

void tPeriodDetection_setWindowSize(tPeriodDetection* const p, int ws)
{
    p->windowSize = ws;
}

void tPeriodDetection_setFidelityThreshold(tPeriodDetection* const p, Lfloat threshold)
{
    p->fidelityThreshold = threshold;
}

void tPeriodDetection_setAlpha            (tPeriodDetection* const p, Lfloat alpha)
{
    p->alpha = LEAF_clip(0.0f, alpha, 1.0f);
}

void tPeriodDetection_setTolerance        (tPeriodDetection* const p, Lfloat tolerance)
{
    if (tolerance < 0.0f) p->tolerance = 0.0f;
    else p->tolerance = tolerance;
}

void tPeriodDetection_setSampleRate (tPeriodDetection* const p, Lfloat sr)
{
    p->invSampleRate = 1.0f/sr;
    p->radius = expf(-1000.0f * p->hopSize * p->invSampleRate / p->timeConstant);
}

/******************************************************************************/
/*                          Zero Crossing Info                                */
/******************************************************************************/

void    tZeroCrossingInfo_init(tZeroCrossingInfo** const zc, LEAF* const leaf)
{
    tZeroCrossingInfo_initToPool(zc, &leaf->mempool);
}

void    tZeroCrossingInfo_initToPool    (tZeroCrossingInfo** const zc, tMempool** const mp)
{
    tMempool* m = *mp;
    tZeroCrossingInfo* z = *zc = (tZeroCrossingInfo*) mpool_calloc(sizeof(tZeroCrossingInfo), m);
    z->mempool = m;
    
    z->_leading_edge = INT_MIN;
    z->_trailing_edge = INT_MIN;
    z->_width = 0.0f;
}

void    tZeroCrossingInfo_free  (tZeroCrossingInfo** const zc)
{
    tZeroCrossingInfo* z = *zc;
    
    mpool_free((char*)z, z->mempool);
}

void    tZeroCrossingInfo_updatePeak(tZeroCrossingInfo* const z, Lfloat s, int pos)
{
    z->_peak = fmaxf(s, z->_peak);
    if ((z->_width == 0.0f) && (s < (z->_peak * 0.3f)))
        z->_width = pos - z->_leading_edge;
}

int     tZeroCrossingInfo_period(tZeroCrossingInfo* const z, tZeroCrossingInfo* const n)
{
    return n->_leading_edge - z->_leading_edge;
}

Lfloat   tZeroCrossingInfo_fractionalPeriod(tZeroCrossingInfo* const z, tZeroCrossingInfo* const next)
{
    tZeroCrossingInfo* n = next;
    
    // Get the start edge
    Lfloat prev1 = z->_before_crossing;
    Lfloat curr1 = z->_after_crossing;
    Lfloat dy1 = curr1 - prev1;
    Lfloat dx1 = -prev1 / dy1;
    
    // Get the next edge
    Lfloat prev2 = n->_before_crossing;
    Lfloat curr2 = n->_after_crossing;
    Lfloat dy2 = curr2 - prev2;
    Lfloat dx2 = -prev2 / dy2;
    
    // Calculate the fractional period
    Lfloat result = n->_leading_edge - z->_leading_edge;
    return result + (dx2 - dx1);
}

int     tZeroCrossingInfo_getWidth(tZeroCrossingInfo* const z)
{
    return z->_width;
}

static inline void update_state(tZeroCrossingCollector* const zc, Lfloat s);
static inline void shift(tZeroCrossingCollector* const zc, int n);
static inline void reset(tZeroCrossingCollector* const zc);

void    tZeroCrossingCollector_init(tZeroCrossingCollector** const zc, int windowSize, Lfloat hysteresis, LEAF* const leaf)
{
    tZeroCrossingCollector_initToPool(zc, windowSize, hysteresis, &leaf->mempool);
}

void    tZeroCrossingCollector_initToPool    (tZeroCrossingCollector** const zc, int windowSize, Lfloat hysteresis, tMempool** const mp)
{
    tMempool* m = *mp;
    tZeroCrossingCollector* z = *zc = (tZeroCrossingCollector*) mpool_alloc(sizeof(tZeroCrossingCollector), m);
    z->mempool = m;
    
    z->_hysteresis = -dbtoa(hysteresis);
    int bits = CHAR_BIT * sizeof(unsigned int);
    z->_window_size = fmax(2, (windowSize + bits - 1) / bits) * bits;
    
    int size = z->_window_size / 2;
    
    // Ensure size is a power of 2
    z->_size = pow(2.0, ceil(log2((double)size)));
    z->_mask = z->_size - 1;

    z->_info = (tZeroCrossingInfo**) mpool_calloc(sizeof(tZeroCrossingInfo*) * z->_size, m);

    for (unsigned i = 0; i < z->_size; i++)
    {
        tZeroCrossingInfo_initToPool(&z->_info[i], mp);
    }
    
    z->_pos = 0;
    
    z->_prev = 0.0f;
    z->_state = 0;
    z->_num_edges = 0;
    z->_frame = 0;
    z->_ready = 0;
    z->_peak_update = 0.0f;
    z->_peak = 0.0f;
}

void    tZeroCrossingCollector_free  (tZeroCrossingCollector** const zc)
{
    tZeroCrossingCollector* z = *zc;
    
    for (unsigned i = 0; i < z->_size; i++)
    {
        tZeroCrossingInfo_free(&z->_info[i]);
    }
    
    mpool_free((char*)z->_info, z->mempool);
    mpool_free((char*)z, z->mempool);
}

int     tZeroCrossingCollector_tick(tZeroCrossingCollector* const z, Lfloat s)
{
    
    // Offset s by half of hysteresis, so that zero cross detection is
    // centered on the actual zero.
    s += z->_hysteresis * 0.5f;
    
    if (z->_num_edges >= (int)z->_size)
        reset(z);
    
    if ((z->_frame == z->_window_size/2) && (z->_num_edges == 0))
        reset(z);
    
    update_state(z, s);
    
    if ((++z->_frame >= z->_window_size) && !z->_state)
    {
        // Remove half the size from _frame, so we can continue seamlessly
        z->_frame -= z->_window_size / 2;
        
        // We need at least two rising edges.
        if (z->_num_edges > 1)
            z->_ready = 1;
        else
            reset(z);
    }
    
    return z->_state;
}

int     tZeroCrossingCollector_getState(tZeroCrossingCollector* const z)
{
    return z->_state;
}

tZeroCrossingInfo* const tZeroCrossingCollector_getCrossing(tZeroCrossingCollector* const z, int index)
{
    int i = (z->_num_edges - 1) - index;
    return z->_info[(z->_pos + i) & z->_mask];
}

int     tZeroCrossingCollector_getNumEdges(tZeroCrossingCollector* const z)
{
    return z->_num_edges;
}

int     tZeroCrossingCollector_getCapacity(tZeroCrossingCollector* const z)
{
    return (int)z->_size;
}

int     tZeroCrossingCollector_getFrame(tZeroCrossingCollector* const z)
{
    return z->_frame;
}

int     tZeroCrossingCollector_getWindowSize(tZeroCrossingCollector* const z)
{
    return z->_window_size;
}

int     tZeroCrossingCollector_isReady(tZeroCrossingCollector* const z)
{
    return z->_ready;
}

Lfloat   tZeroCrossingCollector_getPeak(tZeroCrossingCollector* const z)
{
    return fmaxf(z->_peak, z->_peak_update);
}

int     tZeroCrossingCollector_isReset(tZeroCrossingCollector* const z)
{
    return z->_frame == 0;
}

void    tZeroCrossingCollector_setHysteresis(tZeroCrossingCollector* const z, Lfloat hysteresis)
{
    z->_hysteresis = -dbtoa(hysteresis);
}

static inline void update_state(tZeroCrossingCollector* const z, Lfloat s)
{
    if (z->_ready)
    {
        shift(z, z->_window_size / 2);
        z->_ready = 0;
        z->_peak = z->_peak_update;
        z->_peak_update = 0.0f;
    }
    
    if (z->_num_edges >= (int)z->_size)
        reset(z);
    
    if (s > 0.0f)
    {
        if (!z->_state)
        {
            --z->_pos;
            z->_pos &= z->_mask;
            tZeroCrossingInfo* crossing = z->_info[z->_pos & z->_mask];
            crossing->_before_crossing = z->_prev;
            crossing->_after_crossing = s;
            crossing->_peak = s;
            crossing->_leading_edge = (int) z->_frame;
            crossing->_trailing_edge = INT_MIN;
            crossing->_width = 0.0f;
            ++z->_num_edges;
            z->_state = 1;
        }
        else
        {
            tZeroCrossingInfo_updatePeak(z->_info[z->_pos & z->_mask], s, z->_frame);
        }
        if (s > z->_peak_update)
        {
            z->_peak_update = s;
        }
    }
    else if (z->_state && (s < z->_hysteresis))
    {
        z->_state = 0;
        z->_info[z->_pos & z->_mask]->_trailing_edge = z->_frame;
        if (z->_peak == 0.0f)
            z->_peak = z->_peak_update;
    }
    
    if (z->_frame > z->_window_size * 2)
        reset(z);

    z->_prev = s;
}

static inline void shift(tZeroCrossingCollector* const z, int n)
{
    tZeroCrossingInfo* crossing = z->_info[z->_pos & z->_mask];
    
    crossing->_leading_edge -= n;
    if (!z->_state)
        crossing->_trailing_edge -= n;
    int i = 1;
    for (; i != z->_num_edges; ++i)
    {
        int idx = (z->_pos + i) & z->_mask;
        z->_info[idx]->_leading_edge -= n;
        int edge = (z->_info[idx]->_trailing_edge -= n);
        if (edge < 0.0f)
            break;
    }
    z->_num_edges = i;
}

static inline void reset(tZeroCrossingCollector* const z)
{
    z->_num_edges = 0;
    z->_state = 0;
    z->_frame = 0;
}



void    tBitset_init(tBitset** const bitset, int numBits, LEAF* const leaf)
{
    tBitset_initToPool(bitset, numBits, &leaf->mempool);
}

void    tBitset_initToPool  (tBitset** const bitset, int numBits, tMempool** const mempool)
{
    tMempool* m = *mempool;
    tBitset* b = *bitset = (tBitset*) mpool_alloc(sizeof(tBitset), m);
    b->mempool = m;
    
    // Size of the array value in bits
    b->_value_size = (CHAR_BIT * sizeof(unsigned int));
    
    // Size of the array needed to store numBits bits
    b->_size = (numBits + b->_value_size - 1) / b->_value_size;
    
    // Siz of the array in bits
    b->_bit_size = b->_size * b->_value_size;
    
    b->_bits = (unsigned int*) mpool_calloc(sizeof(unsigned int) * b->_size, m);
}

void    tBitset_free    (tBitset** const bitset)
{
    tBitset* b = *bitset;
    
    mpool_free((char*) b->_bits, b->mempool);
    mpool_free((char*) b, b->mempool);
}

int     tBitset_get     (tBitset* const b, int index)
{
    // Check we don't get past the storage
    if (index > b->_bit_size)
        return -1;
    
    unsigned int mask = 1 << (index % b->_value_size);
    return (b->_bits[index / b->_value_size] & mask) != 0;
}

unsigned int*   tBitset_getData   (tBitset* const b)
{
    return b->_bits;
}

void     tBitset_set     (tBitset* const b, int index, unsigned int val)
{
    if (index > b->_bit_size)
        return;
    
    unsigned int mask = 1 << (index % b->_value_size);
    int i = index / b->_value_size;
    b->_bits[i] ^= (-val ^ b->_bits[i]) & mask;
}

void     tBitset_setMultiple (tBitset* const b, int index, int n, unsigned int val)
{
    // Check that the index (i) does not get past size
    if (index > b->_bit_size)
        return;
    
    // Check that the n does not get past the size
    if ((index+n) > b->_bit_size)
        n = b->_bit_size - index;
    
    // index is the bit index, i is the integer index
    int i = index / b->_value_size;
    
    // Do the first partial int
    int mod = index & (b->_value_size - 1);
    if (mod)
    {
        // mask off the high n bits we want to set
        mod = b->_value_size - mod;
        
        // Calculate the mask
        unsigned int mask = ~(UINT_MAX >> mod);
        
        // Adjust the mask if we're not going to reach the end of this int
        if (n < mod)
            mask &= (UINT_MAX >> (mod - n));
        
        if (val)
            b->_bits[i] |= mask;
        else
            b->_bits[i] &= ~mask;
        
        // Fast exit if we're done here!
        if (n < mod)
            return;
        
        n -= mod;
        ++i;
    }
    
    // Write full ints while we can - effectively doing value_size bits at a time
    if (n >= b->_value_size)
    {
        // Store a local value to work with
        unsigned int val_ = val ? UINT_MAX : 0;
        
        do
        {
            b->_bits[i++] = val_;
            n -= b->_value_size;
        }
        while (n >= b->_value_size);
    }
    
    // Now do the final partial int, if necessary
    if (n)
    {
        mod = n & (b->_value_size - 1);
        
        // Calculate the mask
        unsigned int mask = (1 << mod) - 1;
        
        if (val)
            b->_bits[i] |= mask;
        else
            b->_bits[i] &= ~mask;
    }
}

int     tBitset_getSize (tBitset* const b)
{
    return b->_bit_size;
}

void    tBitset_clear   (tBitset* const b)
{
    for (int i = 0; i < b->_size; ++i)
    {
        b->_bits[i] = 0;
    }
}

void    tBACF_init(tBACF** const bacf, tBitset** const bitset, LEAF* const leaf)
{
    tBACF_initToPool(bacf, bitset, &leaf->mempool);
}

void    tBACF_initToPool    (tBACF** const bacf, tBitset** const bitset, tMempool** const mempool)
{
    tMempool* m = *mempool;
    tBACF* b = *bacf = (tBACF*) mpool_alloc(sizeof(tBACF), m);
    b->mempool = m;
    
    b->_bitset = *bitset;
    b->_mid_array = ((b->_bitset->_bit_size / b->_bitset->_value_size) / 2) - 1;
}

void    tBACF_free  (tBACF** const bacf)
{
    tBACF* b = *bacf;
    
    mpool_free((char*) b, b->mempool);
}

int    tBACF_getCorrelation  (tBACF* const b, int pos)
{
    int value_size = b->_bitset->_value_size;
    const int index = pos / value_size;
    const int shift = pos % value_size;
    
    const unsigned int* p1 = b->_bitset->_bits;
    const unsigned int* p2 = b->_bitset->_bits + index;
    int count = 0;
    
    if (shift == 0)
    {
        for (unsigned i = 0; i != b->_mid_array; ++i)
        {
            // built in compiler popcount functions should be faster but we want this to be portable
            // could try to add some define that call the correct function depending on compiler
            // or let the user pointer popcount() to whatever they want
            // something to look into...
#ifdef __GNUC__
            count += __builtin_popcount(*p1++ ^ *p2++);
#elif _MSC_VER
            count += __popcnt(*p1++ ^ *p2++);
#else
            count += popcount(*p1++ ^ *p2++);
#endif
        }
    }
    else
    {
        const int shift2 = value_size - shift;
        for (unsigned i = 0; i != b->_mid_array; ++i)
        {
            unsigned int v = *p2++ >> shift;
            v |= *p2 << shift2;
#ifdef __GNUC__
            count += __builtin_popcount(*p1++ ^ v++);
#elif _MSC_VER
            count += __popcnt(*p1++ ^ v++);
#else
            count += popcount(*p1++ ^ v++);
#endif
        }
    }
    return count;
}

void    tBACF_set  (tBACF* const b, tBitset** const bitset)
{
    b->_bitset = *bitset;
    b->_mid_array = ((b->_bitset->_bit_size / b->_bitset->_value_size) / 2) - 1;
}

static inline void set_bitstream(tPeriodDetector* const p);
static inline void autocorrelate(tPeriodDetector* const detector);

static inline void sub_collector_init(_sub_collector* collector, tZeroCrossingCollector* const crossings, Lfloat pdt, int range);
static inline Lfloat sub_collector_period_of(_sub_collector* collector, _auto_correlation_info info);
static inline void sub_collector_save(_sub_collector* collector, _auto_correlation_info info);
static inline int sub_collector_try_sub_harmonic(_sub_collector* collector, int harmonic, _auto_correlation_info info, Lfloat incoming_period);
static inline int sub_collector_process_harmonics(_sub_collector* collector, _auto_correlation_info info);
static inline void sub_collector_process(_sub_collector* collector, _auto_correlation_info info);
static inline void sub_collector_get(_sub_collector* collector, _auto_correlation_info info, _period_info* result);

void    tPeriodDetector_init(tPeriodDetector** const detector, Lfloat lowestFreq, Lfloat highestFreq, Lfloat hysteresis, LEAF* const leaf)
{
    tPeriodDetector_initToPool(detector, lowestFreq, highestFreq, hysteresis, &leaf->mempool);
}

void    tPeriodDetector_initToPool  (tPeriodDetector** const detector, Lfloat lowestFreq, Lfloat highestFreq, Lfloat hysteresis, tMempool** const mempool)
{
    tMempool* m = *mempool;
    tPeriodDetector* p = *detector = (tPeriodDetector*) mpool_alloc(sizeof(tPeriodDetector), m);
    p->mempool = m;
    
    LEAF* leaf = p->mempool->leaf;
    
    p->sampleRate = leaf->sampleRate;
    p->lowestFreq = lowestFreq;
    p->highestFreq = highestFreq;
    
    tZeroCrossingCollector_initToPool(&p->_zc, (1.0f / lowestFreq) * p->sampleRate * 2.0f, hysteresis, mempool);
    p->_min_period = (1.0f / highestFreq) * p->sampleRate;
    p->_range = highestFreq / lowestFreq;
    
    int windowSize = tZeroCrossingCollector_getWindowSize(p->_zc);
    tBitset_initToPool(&p->_bits, windowSize, mempool);
    p->_weight = 2.0f / windowSize;
    p->_mid_point = windowSize / 2;
    p->_periodicity_diff_threshold = p->_mid_point * PERIODICITY_DIFF_FACTOR;
    
    p->_predicted_period = -1.0f;
    p->_edge_mark = 0;
    p->_predict_edge = 0;
    p->_num_pulses = 0;
    p->_half_empty = 0;
    
    tBACF_initToPool(&p->_bacf, &p->_bits, mempool);
}

void    tPeriodDetector_free    (tPeriodDetector** const detector)
{
    tPeriodDetector* p = *detector;
    
    tZeroCrossingCollector_free(&p->_zc);
    tBitset_free(&p->_bits);
    tBACF_free(&p->_bacf);
    
    mpool_free((char*) p, p->mempool);
}

int   tPeriodDetector_tick    (tPeriodDetector* const p, Lfloat s)
{
    // Zero crossing
    int prev = tZeroCrossingCollector_getState(p->_zc);
    int zc = tZeroCrossingCollector_tick(p->_zc, s);
    
    if (!zc && prev != zc)
    {
        ++p->_edge_mark;
        p->_predicted_period = -1.0f;
    }
    
    if (tZeroCrossingCollector_isReset(p->_zc))
    {
        p->_fundamental.period = -1.0f;
        p->_fundamental.periodicity = 0.0f;
    }
    
    if (tZeroCrossingCollector_isReady(p->_zc))
    {
        set_bitstream(p);
        autocorrelate(p);
        return 1;
    }
    return 0;
}

Lfloat   tPeriodDetector_getPeriod   (tPeriodDetector* const p)
{
    return p->_fundamental.period;
}

Lfloat   tPeriodDetector_getPeriodicity  (tPeriodDetector* const p)
{
    return p->_fundamental.periodicity;
}

Lfloat   tPeriodDetector_harmonic    (tPeriodDetector* const p, int harmonicIndex)
{
    if (harmonicIndex > 0)
    {
        if (harmonicIndex == 1)
            return p->_fundamental.periodicity;
        
        Lfloat target_period = p->_fundamental.period / (Lfloat) harmonicIndex;
        if (target_period >= p->_min_period && target_period < p->_mid_point)
        {
            int count = tBACF_getCorrelation(p->_bacf, roundf(target_period));
            Lfloat periodicity = 1.0f - (count * p->_weight);
            return periodicity;
        }
    }
    return 0.0f;
}

Lfloat   tPeriodDetector_predictPeriod   (tPeriodDetector* const p)
{
    if (p->_predicted_period == -1.0f && p->_edge_mark != p->_predict_edge)
    {
        p->_predict_edge = p->_edge_mark;
        int n = tZeroCrossingCollector_getNumEdges(p->_zc);
        if (n > 1)
        {
            Lfloat threshold = tZeroCrossingCollector_getPeak(p->_zc) * PULSE_THRESHOLD;
            for (int i = n - 1; i > 0; --i)
            {
                tZeroCrossingInfo* edge2 = tZeroCrossingCollector_getCrossing(p->_zc, i);
                if (edge2->_peak >= threshold)
                {
                    for (int j = i-1; j >= 0; --j)
                    {
                        tZeroCrossingInfo* edge1 = tZeroCrossingCollector_getCrossing(p->_zc, j);
                        if (edge1->_peak >= threshold)
                        {
                            Lfloat period = tZeroCrossingInfo_fractionalPeriod(edge1, edge2);
                            if (period > p->_min_period)
                                return (p->_predicted_period = period);
                        }
                    }
                    return p->_predicted_period = -1.0f;
                }
            }
        }
    }
    return p->_predicted_period;
}

int     tPeriodDetector_isReady (tPeriodDetector* const p)
{
    return tZeroCrossingCollector_isReady(p->_zc);
}

int     tPeriodDetector_isReset (tPeriodDetector* const p)
{
    return tZeroCrossingCollector_isReset(p->_zc);
}

void    tPeriodDetector_setHysteresis   (tPeriodDetector* const p, Lfloat hysteresis)
{
    return tZeroCrossingCollector_setHysteresis(p->_zc, hysteresis);
}

void    tPeriodDetector_setSampleRate   (tPeriodDetector* const p, Lfloat sr)
{
    tMempool* m = p->mempool;
    p->sampleRate = sr;
    Lfloat hysteresis = p->_zc->_hysteresis;
    
    tZeroCrossingCollector_free(&p->_zc);
    tZeroCrossingCollector_initToPool(&p->_zc, (1.0f / p->lowestFreq) * p->sampleRate * 2.0f, hysteresis, &m);
    p->_min_period = (1.0f / p->highestFreq) * p->sampleRate;
}

static inline void set_bitstream(tPeriodDetector* const p)
{
    Lfloat threshold = tZeroCrossingCollector_getPeak(p->_zc) * PULSE_THRESHOLD;
    unsigned int leading_edge = tZeroCrossingCollector_getWindowSize(p->_zc);
    unsigned int trailing_edge = 0;
    
    p->_num_pulses = 0;
    tBitset_clear(p->_bits);
    
    for (int i = 0; i != tZeroCrossingCollector_getNumEdges(p->_zc); ++i)
    {
        tZeroCrossingInfo* info = tZeroCrossingCollector_getCrossing(p->_zc, i);
        if (info->_peak >= threshold)
        {
            ++p->_num_pulses;
            if (info->_leading_edge < leading_edge)
                leading_edge = info->_leading_edge;
            if (info->_trailing_edge > trailing_edge)
                trailing_edge = info->_trailing_edge;
            int pos = fmax(info->_leading_edge, 0);
            int n = info->_trailing_edge - pos;
            tBitset_setMultiple(p->_bits, pos, n, 1);
        }
    }
    p->_half_empty = (leading_edge > p->_mid_point) || (trailing_edge < p->_mid_point);
}

static inline void autocorrelate(tPeriodDetector* const p)
{
    Lfloat threshold = tZeroCrossingCollector_getPeak(p->_zc) * PULSE_THRESHOLD;
    
    _sub_collector collect;
    sub_collector_init(&collect, p->_zc, p->_periodicity_diff_threshold, p->_range);
    
    if (p->_half_empty || p->_num_pulses < 2)
    {
        p->_fundamental.periodicity = -1.0f;
        return;
    }
    else
    {
        int shouldBreak = 0;
        int n = tZeroCrossingCollector_getNumEdges(p->_zc);
        for (int i = 0; i != n - 1; ++i)
        {
            tZeroCrossingInfo* curr = tZeroCrossingCollector_getCrossing(p->_zc, i);
            if (curr->_peak >= threshold)
            {
                for (int j = i + 1; j != n; ++j)
                {
                    tZeroCrossingInfo* next = tZeroCrossingCollector_getCrossing(p->_zc, j);
                    if (next->_peak >= threshold)
                    {
                        int period = tZeroCrossingInfo_period(curr, next);
                        if (period > p->_mid_point)
                            break;
                        if (period >= p->_min_period)
                        {
                            
                            int count = tBACF_getCorrelation(p->_bacf, period);
                            
                            int mid = p->_bacf->_mid_array * CHAR_BIT * sizeof(unsigned int);
                            
                            int start = period;
                            
                            if ((collect._fundamental._period == -1.0f) && count == 0)
                            {
                                if (tBACF_getCorrelation(p->_bacf, period / 2.0f) == 0)
                                    count = -1;
                            }
                            else if (period < 32) // Search minimum if the resolution is low
                            {
                                // Search upwards for the minimum autocorrelation count
                                for (int d = start + 1; d < mid; ++d)
                                {
                                    int c = tBACF_getCorrelation(p->_bacf, d);
                                    if (c > count)
                                        break;
                                    count = c;
                                    period = d;
                                }
                                // Search downwards for the minimum autocorrelation count
                                for (int d = start - 1; d > p->_min_period; --d)
                                {
                                    int c = tBACF_getCorrelation(p->_bacf, d);
                                    if (c > count)
                                        break;
                                    count = c;
                                    period = d;
                                }
                            }
                            
                            if (count == -1)
                            {
                                shouldBreak = 1;
                                break; // Return early if we have false correlation
                            }
                            Lfloat periodicity = 1.0f - (count * p->_weight);
                            _auto_correlation_info info = { i, j, (int) period, periodicity };
                            sub_collector_process(&collect, info);
                            if (count == 0)
                            {
                                shouldBreak = 1;
                                break; // Return early if we have perfect correlation
                            }
                        }
                    }
                }
            }
            if (shouldBreak > 0) break;
        }
    }
    
    // Get the final resuts
    sub_collector_get(&collect, collect._fundamental, &p->_fundamental);
}

static inline void sub_collector_init(_sub_collector* collector, tZeroCrossingCollector* const crossings, Lfloat pdt, int range)
{
    collector->_zc = crossings;
    collector->_harmonic_threshold = HARMONIC_PERIODICITY_FACTOR * 2.0f / (Lfloat)collector->_zc->_window_size;
    collector->_periodicity_diff_threshold = pdt;
    collector->_range = range;
    collector->_fundamental._i1 = -1;
    collector->_fundamental._i2 = -1;
    collector->_fundamental._period = -1;
    collector->_fundamental._periodicity = 0.0f;
    collector->_fundamental._harmonic = 0;
    collector->_first_period = 0.01f;
}

static inline Lfloat sub_collector_period_of(_sub_collector* collector, _auto_correlation_info info)
{
    tZeroCrossingInfo* first = tZeroCrossingCollector_getCrossing(collector->_zc, info._i1);
    tZeroCrossingInfo* next = tZeroCrossingCollector_getCrossing(collector->_zc, info._i2);
    return tZeroCrossingInfo_fractionalPeriod(first, next);
}

static inline void sub_collector_save(_sub_collector* collector, _auto_correlation_info info)
{
    collector->_fundamental = info;
    collector->_fundamental._harmonic = 1;
    collector->_first_period = sub_collector_period_of(collector, collector->_fundamental);
}

static inline int sub_collector_try_sub_harmonic(_sub_collector* collector, int harmonic, _auto_correlation_info info, Lfloat incoming_period)
{
    if (fabsf(incoming_period - collector->_first_period) < collector->_periodicity_diff_threshold)
    {
        // If incoming is a different harmonic and has better
        // periodicity ...
        if (info._periodicity > collector->_fundamental._periodicity &&
            harmonic != collector->_fundamental._harmonic)
        {
            Lfloat periodicity_diff = fabsf(info._periodicity - collector->_fundamental._periodicity);
            
            // If incoming periodicity is within the harmonic
            // periodicity threshold, then replace _fundamental with
            // incoming. Take note of the harmonic for later.
            if (periodicity_diff <= collector->_harmonic_threshold)
            {
                collector->_fundamental._i1 = info._i1;
                collector->_fundamental._i2 = info._i2;
                collector->_fundamental._periodicity = info._periodicity;
                collector->_fundamental._harmonic = harmonic;
            }
            
            // If not, then we save incoming (replacing the current
            // _fundamental).
            else
            {
                sub_collector_save(collector, info);
            }
        }
        return 1;
    }
    return 0;
}

static inline int sub_collector_process_harmonics(_sub_collector* collector, _auto_correlation_info info)
{
    if (info._period < collector->_first_period)
        return 0;
    
    Lfloat incoming_period = sub_collector_period_of(collector, info);
    int multiple = fmaxf(1.0f, roundf( incoming_period / collector->_first_period));
    return sub_collector_try_sub_harmonic(collector, fmin(collector->_range, multiple), info, incoming_period/multiple);
}

static inline void sub_collector_process(_sub_collector* collector, _auto_correlation_info info)
{
    if (collector->_fundamental._period == -1.0f)
        sub_collector_save(collector, info);
    
    else if (sub_collector_process_harmonics(collector, info))
        return;
    
    else if (info._periodicity > collector->_fundamental._periodicity)
        sub_collector_save(collector, info);
}

static inline void sub_collector_get(_sub_collector* collector, _auto_correlation_info info, _period_info* result)
{
    if (info._period != -1.0f)
    {
        result->period = sub_collector_period_of(collector, info) / info._harmonic;
        result->periodicity = info._periodicity;
    }
    else
    {
        result->period = -1.0f;
        result->period = 0.0f;
    }
}

static inline Lfloat calculate_frequency(tPitchDetector* const detector);
static inline void bias(tPitchDetector* const detector, _pitch_info incoming);

void    tPitchDetector_init(tPitchDetector** const detector, Lfloat lowestFreq, Lfloat highestFreq, LEAF* const leaf)
{
    tPitchDetector_initToPool(detector, lowestFreq, highestFreq, &leaf->mempool);
}

void    tPitchDetector_initToPool   (tPitchDetector** const detector, Lfloat lowestFreq, Lfloat highestFreq, tMempool** const mempool)
{
    tMempool* m = *mempool;
    tPitchDetector* p = *detector = (tPitchDetector*) mpool_alloc(sizeof(tPitchDetector), m);
    p->mempool = m;
    LEAF* leaf = p->mempool->leaf;
    
    tPeriodDetector_initToPool(&p->_pd, lowestFreq, highestFreq, -120.0f, mempool);
    p->_current.frequency = 0.0f;
    p->_current.periodicity = 0.0f;
    p->_frames_after_shift = 0;
    p->sampleRate = leaf->sampleRate;
}

void    tPitchDetector_free (tPitchDetector** const detector)
{
    tPitchDetector* p = *detector;
    
    tPeriodDetector_free(&p->_pd);
    mpool_free((char*) p, p->mempool);
}

int     tPitchDetector_tick    (tPitchDetector* const p, Lfloat s)
{
    tPeriodDetector_tick(p->_pd, s);
    
    if (tPeriodDetector_isReset(p->_pd))
    {
        p->_current.frequency = 0.0f;
        p->_current.periodicity = 0.0f;
    }
    
    int ready = tPeriodDetector_isReady(p->_pd);
    if (ready)
    {
        Lfloat periodicity = p->_pd->_fundamental.periodicity;
        
        if (periodicity == -1.0f)
        {
            p->_current.frequency = 0.0f;
            p->_current.periodicity = 0.0f;
            return 0;
        }
        
        if (p->_current.frequency == 0.0f)
        {
            if (periodicity >= ONSET_PERIODICITY)
            {
                Lfloat f = calculate_frequency(p);
                if (f > 0.0f)
                {
                    p->_current.frequency = f;
                    p->_current.periodicity = periodicity;
                    p->_frames_after_shift = 0;
                }
            }
        }
        else
        {
            if (periodicity < MIN_PERIODICITY)
                p->_frames_after_shift = 0;
            Lfloat f = calculate_frequency(p);
            if (f > 0.0f)
            {
                _pitch_info info = { f, periodicity };
                bias(p, info);
            }
        }
    }
    return ready;
}

Lfloat   tPitchDetector_getFrequency    (tPitchDetector* const p)
{
    return p->_current.frequency;
}

Lfloat   tPitchDetector_getPeriodicity  (tPitchDetector* const p)
{
    return p->_current.periodicity;
}

Lfloat   tPitchDetector_harmonic (tPitchDetector* const p, int harmonicIndex)
{
    return tPeriodDetector_harmonic(p->_pd, harmonicIndex);
}

Lfloat   tPitchDetector_predictFrequency (tPitchDetector* const p)
{
    Lfloat period = tPeriodDetector_predictPeriod(p->_pd);
    if (period > 0.0f)
        return p->sampleRate / period;
    return 0.0f;
}

int     tPitchDetector_indeterminate    (tPitchDetector* const p)
{
    return p->_current.frequency == 0.0f;
}

void    tPitchDetector_setHysteresis    (tPitchDetector* const p, Lfloat hysteresis)
{
    tPeriodDetector_setHysteresis(p->_pd, hysteresis);
}

void    tPitchDetector_setSampleRate    (tPitchDetector* const p, Lfloat sr)
{
    p->sampleRate = sr;
    tPeriodDetector_setSampleRate(p->_pd, p->sampleRate);
}

static inline Lfloat calculate_frequency(tPitchDetector* const p)
{
    Lfloat period = p->_pd->_fundamental.period;
    if (period > 0.0f)
        return p->sampleRate / period;
    return 0.0f;
}

static inline void bias(tPitchDetector* const p, _pitch_info incoming)
{
    ++p->_frames_after_shift;
    int shifted = 0;
    
    _pitch_info result;
    
    //=============================================================================
    //_pitch_info result = bias(current, incoming, shift);
    {
        Lfloat error = p->_current.frequency * 0.015625; // approx 1/4 semitone
        Lfloat diff = fabsf(p->_current.frequency - incoming.frequency);
        int done = 0;
        
        // Try fundamental
        if (diff < error)
        {
            result = incoming;
            done = 1;
        }
        // Try harmonics and sub-harmonics
        else if (p->_frames_after_shift > 1)
        {
            if (p->_current.frequency > incoming.frequency)
            {
                int multiple = roundf(p->_current.frequency / incoming.frequency);
                if (multiple > 1)
                {
                    Lfloat f = incoming.frequency * multiple;
                    if (fabsf(p->_current.frequency - f) < error)
                    {
                        result.frequency = f;
                        result.periodicity = incoming.periodicity;
                        done = 1;
                    }
                }
            }
            else
            {
                int multiple = roundf(incoming.frequency / p->_current.frequency);
                if (multiple > 1)
                {
                    Lfloat f = incoming.frequency / multiple;
                    if (fabsf(p->_current.frequency - f) < error)
                    {
                        result.frequency = f;
                        result.periodicity = incoming.periodicity;
                        done = 1;
                    }
                }
            }
        }
        // Don't do anything if the latest autocorrelation is not periodic
        // enough. Note that we only do this check on frequency shifts (i.e. at
        // this point, we are looking at a potential frequency shift, after
        // passing through the code above, checking for fundamental and
        // harmonic matches).
        if (!done)
        {
            if (p->_pd->_fundamental.periodicity > MIN_PERIODICITY)
            {
                // Now we have a frequency shift
                shifted = 1;
                result = incoming;
            }
            else result = p->_current;
        }
    }
    
    //=============================================================================
    
    // Don't do anything if incoming is not periodic enough
    // Note that we only do this check on frequency shifts
    if (shifted)
    {
        Lfloat periodicity = p->_pd->_fundamental.periodicity;
        if (periodicity >= ONSET_PERIODICITY)
        {
            p->_frames_after_shift = 0;
            p->_current = result;
        }
        else if (periodicity < MIN_PERIODICITY)
        {
            p->_current.frequency = 0.0f;
            p->_current.periodicity = 0.0f;
        }
    }
    else
    {
        p->_current = result;
    }
}

static inline void compute_predicted_frequency(tDualPitchDetector* const detector);

void    tDualPitchDetector_init(tDualPitchDetector** const detector, Lfloat lowestFreq, Lfloat highestFreq, Lfloat* inBuffer, int bufSize, LEAF* const leaf)
{
    tDualPitchDetector_initToPool(detector, lowestFreq, highestFreq, inBuffer, bufSize, &leaf->mempool);
}

void    tDualPitchDetector_initToPool   (tDualPitchDetector** const detector, Lfloat lowestFreq, Lfloat highestFreq, Lfloat* inBuffer, int bufSize, tMempool** const mempool)
{
    tMempool* m = *mempool;
    tDualPitchDetector* p = *detector = (tDualPitchDetector*) mpool_alloc(sizeof(tDualPitchDetector), m);
    p->mempool = m;
    LEAF* leaf = p->mempool->leaf;
    
    tPeriodDetection_initToPool(&p->_pd1, inBuffer, bufSize, bufSize / 2, mempool);
    tPitchDetector_initToPool(&p->_pd2, lowestFreq, highestFreq, mempool);
    
    p->sampleRate = leaf->sampleRate;

    p->_current.frequency = 0.0f;
    p->_current.periodicity = 0.0f;
    p->_mean = lowestFreq + ((highestFreq - lowestFreq) / 2.0f);
    p->_predicted_frequency = 0.0f;
    p->_first = 1;
    p->thresh = 0.98f;
    
    p->lowest = lowestFreq;
    p->highest = highestFreq;
}

void    tDualPitchDetector_free (tDualPitchDetector** const detector)
{
    tDualPitchDetector* p = *detector;
    
    tPeriodDetection_free(&p->_pd1);
    tPitchDetector_free(&p->_pd2);
    
    mpool_free((char*) p, p->mempool);
}

int     tDualPitchDetector_tick    (tDualPitchDetector* const p, Lfloat sample)
{
    tPeriodDetection_tick(p->_pd1, sample);
    int ready = tPitchDetector_tick(p->_pd2, sample);

    if (ready)
    {
        int pd2_indeterminate = tPitchDetector_indeterminate(p->_pd2);
        int disagreement = 0;
        Lfloat period = tPeriodDetection_getPeriod(p->_pd1);
        if (!pd2_indeterminate && period != 0.0f)
        {
            _pitch_info _i1;
            _i1.frequency = p->sampleRate / tPeriodDetection_getPeriod(p->_pd1);
            _i1.periodicity = tPeriodDetection_getFidelity(p->_pd1);
            _pitch_info _i2 = p->_pd2->_current;
            
            Lfloat pd1_diff = fabsf(_i1.frequency - p->_mean);
            Lfloat pd2_diff = fabsf(_i2.frequency - p->_mean);

            _pitch_info i;
            disagreement = fabsf(_i1.frequency - _i2.frequency) > (p->_mean * 0.03125f);
            // If they agree, we'll use bacf
            if (!disagreement) i = _i2;
            // A disagreement implies a change
            // Start with smaller changes
            else if (pd2_diff < p->_mean * 0.03125f) i = _i2;
            else if (pd1_diff < p->_mean * 0.03125f) i = _i1;
            // Now filter out lower fidelity stuff
            else if (_i1.periodicity < p->thresh) return ready;
            // Changing up (bacf tends to lead changes)
            else if ((_i1.frequency > p->_mean && _i2.frequency > p->_mean) &&
                     (_i1.frequency < _i2.frequency) &&
                     (_i2.periodicity > p->thresh))
            {
                if (roundf(_i2.frequency / _i1.frequency) > 1) i = _i1;
                else i = _i2;
            }
            // Changing down
            else if ((_i1.frequency < p->_mean && _i2.frequency < p->_mean) &&
                     (_i1.frequency > _i2.frequency) &&
                     (_i2.periodicity > p->thresh))
            {
                if (roundf(_i1.frequency / _i2.frequency) > 1) i = _i1;
                else i = _i2;
            }
            // A bit of handling for stuff out of bacf range, won't be as solid but better than nothing
            else if (_i1.frequency > p->highest)
            {
                if (roundf(_i1.frequency / _i2.frequency) > 1) i = _i2;
                else i = _i1;
            }
            else if (_i1.frequency < p->lowest)
            {
                if (roundf(_i2.frequency / _i1.frequency) > 1) i = _i2;
                else i = _i1;
            }
            // Don't change if we met non of these, probably a bad read
            else return ready;
            
            if (p->_first)
            {
                p->_current = i;
                p->_mean = p->_current.frequency;
                p->_first = 0;
                p->_predicted_frequency = 0.0f;
            }
            else
            {
                p->_current = i;
                p->_mean = (0.2222222 * p->_current.frequency) + (0.7777778 * p->_mean);
                p->_predicted_frequency = 0.0f;
            }
            return ready;
        }
    }

    return ready;
}

Lfloat   tDualPitchDetector_getFrequency    (tDualPitchDetector* const p)
{
    return p->_current.frequency;
}

Lfloat   tDualPitchDetector_getPeriodicity  (tDualPitchDetector* const p)
{
    return p->_current.periodicity;
}

Lfloat   tDualPitchDetector_predictFrequency (tDualPitchDetector* const p)
{
    if (p->_predicted_frequency == 0.0f)
        compute_predicted_frequency(p);
    return p->_predicted_frequency;
}

void    tDualPitchDetector_setHysteresis (tDualPitchDetector* const p, Lfloat hysteresis)
{
    tPitchDetector_setHysteresis(p->_pd2, hysteresis);
}

void    tDualPitchDetector_setPeriodicityThreshold (tDualPitchDetector* const p, Lfloat thresh)
{
    p->thresh = thresh;
}

void    tDualPitchDetector_setSampleRate (tDualPitchDetector* const p, Lfloat sr)
{
    p->sampleRate = sr;
    tPeriodDetection_setSampleRate(p->_pd1, p->sampleRate);
    tPitchDetector_setSampleRate(p->_pd2, p->sampleRate);
}

static inline void compute_predicted_frequency(tDualPitchDetector* const p)
{
    Lfloat f1 = 1.0f / tPeriodDetection_getPeriod(p->_pd1);
    Lfloat f2 = tPitchDetector_predictFrequency(p->_pd2);
    if (f2 > 0.0f)
    {
        Lfloat error = f1 * 0.1f;
        if (fabsf(f1 - f2) < error)
        {
            p->_predicted_frequency = f1;
            return;
        }
    }
    
    p->_predicted_frequency = 0.0f;
}

