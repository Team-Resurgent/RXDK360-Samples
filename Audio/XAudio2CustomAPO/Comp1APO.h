//--------------------------------------------------------------------------------------
// Comp1APO.h
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//
// 
// Comp1 is a "soft knee" dynamic range compressor. Like all compressors it passes
// input below a certain threshold unchanged, but above that threshold it applies
// a gain reduction ratio. The end result is to "squash" the dynamic range, making
// the overall volume vary less. This results in a softer overall sound, but most
// applications of dynamic range compression aim to make the signal sound louder.
// To achieve this an amount of gain, called the "make-up," is applied post
// compression.
//
// What makes this a "soft knee" compressor is the nature of the transition between
// pass-through mode (input < threshold) and gain reduction mode (input > threshold).
// Instead of an abrupt transition, the gain is gradually reduced over a range of 
// values. The "knee" parameter defines the width of the transition band. 
//
//--------------------------------------------------------------------------------------

#pragma once

#include <ATGAPOBase.h>

#include <ATGDsp.h>

//
// Compressor parameters
//
struct Comp1APOParams
{
    float threshold;
    float ratio;
    float knee;
    float makeup;
};

//--------------------------------------------------------------------------------------
// CComp1APO
//
// APO wrapper for the soft knee compressor
// 
// This class derives from CSampleXAPOBase, which implements a class factory
// pattern for object creation. Contrast this with CMonitorAPO, which does not
// use a class factory. The factory pattern is not mandated, but overall it's 
// a safer and better way of creating APOs than using a constructor directly.
// On the other hand, it makes setting the APO's initial state a bit more
// cumbersome.
//
// Note the static method "CalcTransferFunction." This is used to extract the
// transfer function (output value for each given input value) of the compressor
// for a given set of parameters, so that it can be displayed to the user. 
// 
//--------------------------------------------------------------------------------------
class __declspec( uuid("{5EB8D611-FF96-429d-8365-2DDF89A7C1CD}")) 
CComp1APO 
    : public ATG::CSampleXAPOBase<CComp1APO, Comp1APOParams>
{
public:
    const static DWORD c_lookahead = 32;

    CComp1APO(void);
    ~CComp1APO(void);

    // Calculate the compressor's transfer function without taking RMS into account
    //
    static void CalcTransferFunction( const Comp1APOParams& params, __vector4* __restrict pOutput, int outputVectorCount );
private:
    
    // Overrides
    //
    void DoProcess( const Comp1APOParams&, FLOAT32* __restrict pData, UINT32 cFrames, UINT32 cChannels, BOOL bEnabled );
    void OnSetParameters( const Comp1APOParams& params );

    // Core compressor parameters
    ATG::SoftKneeCompressorParams   m_skcParams;

    // Makeup gain--vlume adjustment to compensate for reduction in
    // gain from compressor
    __vector4   m_makeupGain;

    // Data for root-mean-square approximation
    ATG::DelayLine<__vector4, c_lookahead / 4>  m_rmsWindow;
    __vector4                                   m_rmsAccumulator;
};

