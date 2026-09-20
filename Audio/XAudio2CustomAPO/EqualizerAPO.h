//--------------------------------------------------------------------------------------
// Equalizer.h
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//--------------------------------------------------------------------------------------

#pragma once

#include <ATGAPOBase.h>
#include <ATGDsp.h>

//
// Compressor parameters
//

//Currently, this supports 3 band-pass filters. Increase to add additional
//band-pass filters automatically in the CEqualizerAPO constructor.
const int EQ_MAXBANDCOUNT = 5;

enum EqualizerAPOParamType
{
	LowPass,
	BandPass,
	HighPass
};
struct EqualizerAPOParam
{
	EqualizerAPOParamType Type;
    float Frequency;
    float Gain;
    float Q;
};

struct EqualizerAPOParams
{
	int Count;
	EqualizerAPOParam Params[EQ_MAXBANDCOUNT];
};
//--------------------------------------------------------------------------------------
// CEqualizerAPO
//
//--------------------------------------------------------------------------------------
class __declspec( uuid("{5EB8D622-FF96-429d-8365-2DDF89A7C1CD}")) 
CEqualizerAPO 
    : public ATG::CSampleXAPOBase<CEqualizerAPO, EqualizerAPOParams>
{
public:
    const static DWORD c_lookahead = 32;

    CEqualizerAPO(void);
    ~CEqualizerAPO(void);

    // Calculate the Equalizer's transfer function
    //
    static void CalcSignalPulseFunction( const EqualizerAPOParams& params, __vector4* __restrict pOutput, int outputVectorCount );
private:
	//Coefficients for the filter.
	__vector4 coeffsA[EQ_MAXBANDCOUNT];
	__vector4 coeffsB[EQ_MAXBANDCOUNT];
	__vector4 prevOutput[EQ_MAXBANDCOUNT];
    __vector4 prevInput[EQ_MAXBANDCOUNT];

    // Overrides
    //
    void DoProcess( const EqualizerAPOParams&, FLOAT32* __restrict pData, UINT32 cFrames, UINT32 cChannels, BOOL bEnabled );
    void OnSetParameters( const EqualizerAPOParams& params );
};

