//--------------------------------------------------------------------------------------
// EqualizerAPO.cpp
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "EqualizerAPO.h"


//--------------------------------------------------------------------------------------
// Name: CEqualizerAPO::CEqualizerAPO()
// Desc: Constructor
//--------------------------------------------------------------------------------------
CEqualizerAPO::CEqualizerAPO()
: CSampleXAPOBase<CEqualizerAPO, EqualizerAPOParams>()
{
	for (int i=0;i<EQ_MAXBANDCOUNT;i++)
		for(int j=0;j<4;j++)
		{
			prevInput[i].v[j] = 0.0f;
			prevOutput[i].v[j] = 0.0f;
		}

    //Set the initial parameters to have an even response.
	EqualizerAPOParams initialParams;
	initialParams.Count = EQ_MAXBANDCOUNT;

	initialParams.Params[0].Type = HighPass;
	initialParams.Params[0].Frequency = .001;
	initialParams.Params[0].Gain = 0.0;
	initialParams.Params[0].Q = 1.0;

	//Create the BandPass filters in the middle. Band Counts - 2 for low- & high-pass
	for (int i=1;i<=EQ_MAXBANDCOUNT-2;i++)
	{
		//We want the band passes to be between the 'buckets', so the gaps are
		//calculated with one extra bucket.
		float FrequencyGap = 1.0/(float)(EQ_MAXBANDCOUNT-1); 
		initialParams.Params[i].Type = BandPass;
		initialParams.Params[i].Frequency = FrequencyGap * i;
		initialParams.Params[i].Gain = 0.0;
		initialParams.Params[i].Q = 1.0;
	}

	initialParams.Params[initialParams.Count-1].Type = LowPass;
	initialParams.Params[initialParams.Count-1].Frequency = .999;
	initialParams.Params[initialParams.Count-1].Gain = 0.0;
	initialParams.Params[initialParams.Count-1].Q = 1.0;

    SetParameters( &initialParams, sizeof( initialParams ) );
}

//--------------------------------------------------------------------------------------
// Name: CEqualizerAPO::~CEqualizerAPO
// Desc: Destructor
//--------------------------------------------------------------------------------------
CEqualizerAPO::~CEqualizerAPO()
{
}


//--------------------------------------------------------------------------------------
// Name: CEqualizerAPO::DoProcess
// Desc: Applies dynamic range compression to a batch of samples
//--------------------------------------------------------------------------------------
    
void CEqualizerAPO::DoProcess(  const EqualizerAPOParams& params, FLOAT32* __restrict pData, UINT32 cFrames, UINT32 cChannels, BOOL bIsEnabled  )
{
    if (!bIsEnabled)
        return;

    _ASSERT( cChannels == 1 ); // this fx optimized for mono
    UNREFERENCED_PARAMETER( cChannels );


    for( UINT32 i = 0; i < cFrames / 4; ++i )
    {
		__vector4 pOutput_Total = {0.0f,0.0f,0.0f,0.0f};
		for(int j=0;j<params.Count;j++)
		{
			__vector4 pOutput;
			ATG::FilterCore(((__vector4* __restrict)pData)[i],
				pOutput,
				prevInput[j],
				prevOutput[j],
				coeffsA[j],
				coeffsB[j]);
			prevInput[j] = ((__vector4* __restrict)pData)[i];
			prevOutput[j] = pOutput;
			pOutput_Total += pOutput;
		}
		pOutput_Total /= (FLOAT)params.Count;
		((__vector4* __restrict)pData)[i] = pOutput_Total;
	}
}


//--------------------------------------------------------------------------------------
// Name: CEqualizerAPO::CalcSignalPulseFunction
// Desc: Calculates the signal response for the defined filter parameters
//--------------------------------------------------------------------------------------
void CEqualizerAPO::CalcSignalPulseFunction( const EqualizerAPOParams& params, __vector4* __restrict pOutput, int outputVectorCount )
{
	_ASSERT(outputVectorCount);

    __vector4 prevOutput[EQ_MAXBANDCOUNT] = { 0.0f, 0.0f, 0.0f, 0.0f};
    __vector4 prevInput[EQ_MAXBANDCOUNT] = { 0.0f, 0.0f, 0.0f, 0.0f};
	__vector4 localCoeffsA[EQ_MAXBANDCOUNT];
	__vector4 localCoeffsB[EQ_MAXBANDCOUNT];


	for(int i=0;i<params.Count;i++)
	{
		switch(params.Params[i].Type)
		{
			case LowPass:
				{
					ATG::CalcLowpassCoeffs( params.Params[i].Frequency, params.Params[i].Q, localCoeffsA[i], localCoeffsB[i]);
					break;
				}
			case HighPass:
				{
					ATG::CalcHighpassCoeffs( params.Params[i].Frequency, params.Params[i].Q, localCoeffsA[i], localCoeffsB[i]);
					break;
				}
			case BandPass:
				{
					ATG::CalcBandpassCoeffs( params.Params[i].Frequency, params.Params[i].Q,params.Params[i].Gain, localCoeffsA[i], localCoeffsB[i]);
					break;
				}
			default:
				{
					_ASSERT(false);
					break;
				}
		}
	}

	for(int i=0;i<params.Count;i++)
	{
		//The initial pulse is scaled up to account for the length of the run.
		__vector4 input = { outputVectorCount * 4.0f, 0.0f, 0.0f, 0.0f};
		__vector4 TempOutput;

		for( int j = 0; j < outputVectorCount; ++j )
		{
			ATG::FilterCore(input,
				TempOutput,
				prevInput[i],
				prevOutput[i],
				localCoeffsA[i],
				localCoeffsB[i]);
			prevInput[i] = input;
			prevOutput[i] = TempOutput;
			if (i==0)
				pOutput[j] = TempOutput;
			else
				pOutput[j] += TempOutput;
			//Only use the pulse for the first pass through.
			if (input.x>0.0f)
				input.x = 0.0f;
		}
	}

	//Average the result for all filters.
	for( int j = 0; j < outputVectorCount; ++j )
		pOutput[j] /= (FLOAT)params.Count;

}



//--------------------------------------------------------------------------------------
// Name: CEqualizerAPO::OnSetParameters
// Desc: Recomputes the internal state based on a new set of parameters
//--------------------------------------------------------------------------------------
void CEqualizerAPO::OnSetParameters( const EqualizerAPOParams& params )
{
	_ASSERT(params.Count > 0 && params.Count <= EQ_MAXBANDCOUNT);

	for(int i=0;i<params.Count;i++)
	{
		switch(params.Params[i].Type)
		{
			case LowPass:
				{
					ATG::CalcLowpassCoeffs( params.Params[i].Frequency, params.Params[i].Q, coeffsA[i], coeffsB[i]);
					break;
				}
			case BandPass:
				{
					ATG::CalcBandpassCoeffs( params.Params[i].Frequency, params.Params[i].Q,params.Params[i].Gain, coeffsA[i], coeffsB[i]);
					break;
				}
			case HighPass:
				{
					ATG::CalcHighpassCoeffs( params.Params[i].Frequency, params.Params[i].Q, coeffsA[i], coeffsB[i]);
					break;
				}
		}
	}
}
