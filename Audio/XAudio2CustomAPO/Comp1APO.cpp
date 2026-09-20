//--------------------------------------------------------------------------------------
// Comp1APO.cpp
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Comp1APO.h"


//--------------------------------------------------------------------------------------
// Name: CComp1APO::CComp1APO
// Desc: Constructor
//--------------------------------------------------------------------------------------
CComp1APO::CComp1APO()
: CSampleXAPOBase<CComp1APO, Comp1APOParams>()
{
    Comp1APOParams initialParams = { .5, .2, .5, .25 };
    SetParameters( &initialParams, sizeof( initialParams ) );
    m_rmsWindow.Clear();
    m_rmsAccumulator = __vzero();
}

//--------------------------------------------------------------------------------------
// Name: CComp1APO::~CComp1APO
// Desc: Destructor
//--------------------------------------------------------------------------------------
CComp1APO::~CComp1APO()
{
}


//--------------------------------------------------------------------------------------
// Name: CComp1APO::DoProcess
// Desc: Applies dynamic range compression to a batch of samples
//--------------------------------------------------------------------------------------
    
void CComp1APO::DoProcess(  const Comp1APOParams& /*params*/, FLOAT32* __restrict pData, UINT32 cFrames, UINT32 cChannels, BOOL bIsEnabled  )
{
    if (!bIsEnabled)
        return;

    _ASSERT( cChannels == 1 ); // this fx optimized for mono
    UNREFERENCED_PARAMETER( cChannels );

    const __vector4 oneOverWindowLen = { 
        1.0f / (float)(c_lookahead / 4),
        1.0f / (float)(c_lookahead / 4),
        1.0f / (float)(c_lookahead / 4),
        1.0f / (float)(c_lookahead / 4) };


    // Pull values out of the class and put them into
    // automatic variables so that they can stay in
    // registers rather than being pulled from memory
    // each iteration
    __vector4 accumulator = m_rmsAccumulator;
    __vector4 threshold1 = m_skcParams.threshold1;
    __vector4 threshold2 = m_skcParams.threshold2;
    __vector4 ratio = m_skcParams.ratio;
    __vector4 coeffs = m_skcParams.coeffs;
    __vector4 endofknee = m_skcParams.endOfKneeValue;

    for( UINT32 i = 0; i < cFrames / 4; ++i )
    {
        __vector4 input = ((__vector4* __restrict)pData)[i];

        // Compute the current RMS value of the signal
        __vector4 rms = RmsCore( input, &m_rmsWindow, accumulator, oneOverWindowLen );

        // Run the compressor using the RMS as the sidechain. This makes the output a little
        // smoother than using the incoming signal as both input and sidechain.
        //
        __vector4 gain;
        ATG::SoftKneeCompressorCore( rms, gain, threshold1, threshold2, ratio, coeffs, endofknee );
        gain = __vmulfp( gain, m_makeupGain );
        ((__vector4* __restrict)pData)[i] = __vmulfp( input, gain );
    }
    m_rmsAccumulator = accumulator;
}

//--------------------------------------------------------------------------------------
// Name: CComp1APO::CalcTransferFunction
// Desc: Computes the compressor's transfer function for visual display.
//       Note that the output of SoftKneeCompressorCore is a gain ratio, 
//       not a transfer, so it has to be remultiplied by the input.
//--------------------------------------------------------------------------------------
void CComp1APO::CalcTransferFunction( const Comp1APOParams& params, __vector4* __restrict pOutput, int outputVectorCount )
{
    ATG::SoftKneeCompressorParams skcParams;
    ATG::CalcSoftKneeCompressorParams( params.threshold, params.knee, params.ratio, &skcParams );

    float nSamples = (float)outputVectorCount * 4.0f;
    float fScale = 1.0f / nSamples;
    __vector4 input = { 0.0f, fScale, fScale * 2.0f, fScale * 3.0f };
    __vector4 scale = { fScale*4.0f, fScale*4.0f, fScale*4.0f, fScale*4.0f };
    __vector4 makeup = { params.makeup, params.makeup, params.makeup, params.makeup };
    makeup = __vmaddfp( makeup, __vrefp( skcParams.ratio ), ATG::Ones );
    
    for( int i = 0; i < outputVectorCount; ++i )
    {
        __vector4 gain;
        ATG::SoftKneeCompressorCore( 
            input, 
            gain, 
            skcParams.threshold1, 
            skcParams.threshold2, 
            skcParams.ratio, 
            skcParams.coeffs, 
            skcParams.endOfKneeValue );
        pOutput[i] = __vmulfp( input, gain );
        input = __vaddfp( input, scale );
    }

}

//--------------------------------------------------------------------------------------
// Name: CComp1APO::OnParametersChanged
// Desc: Recomputes the internal state based on a new set of parameters
//--------------------------------------------------------------------------------------
void CComp1APO::OnSetParameters( const Comp1APOParams& params )
{
    CalcSoftKneeCompressorParams( 
        params.threshold, 
        params.knee, 
        params.ratio, 
        &m_skcParams );

    m_makeupGain.v[0] = params.makeup;
    m_makeupGain = __vspltw( m_makeupGain, 0 );

    m_makeupGain = __vmaddfp( m_makeupGain, __vrefp( m_skcParams.ratio  ), ATG::Ones );
}
