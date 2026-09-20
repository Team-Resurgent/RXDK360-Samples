//--------------------------------------------------------------------------------------
// LightShafts_OverlayQuad.hlsl
//
// Shaders for lightshafts overlay quads in the LightShafts sample
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// ATI 3D Application Research Group. 
// Copyright (C) ATI Research, Inc. All rights reserved. 
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------
#define NUM_BLUR_TAPS 12
uniform float4 g_vFilterTaps[NUM_BLUR_TAPS] : register(c0);


//--------------------------------------------------------------------------------------
// Samplers
//--------------------------------------------------------------------------------------
sampler ShadowMapSampler  : register(s0);
sampler FogTextureSampler : register(s0);


//--------------------------------------------------------------------------------------
// Vertex structures
//--------------------------------------------------------------------------------------
struct VS_OVERLAY_OUTPUT
{
   float4 vPos      : POSITION;
   float2 vTexCoord : TEXCOORD0;
};


//--------------------------------------------------------------------------------------
// Name: OverlayMainVS()
// Desc: Vertex Shader
//--------------------------------------------------------------------------------------
VS_OVERLAY_OUTPUT OverlayMainVS( float4 vPosition : POSITION,
                                 float2 vTexCoord : TEXCOORD0 )
{
    VS_OVERLAY_OUTPUT Out; 
    Out.vPos      = vPosition;
    Out.vTexCoord = vTexCoord;
    return Out;
}


//--------------------------------------------------------------------------------------
// Name: ShadowBlurPS()
// Desc: Routine to blur a shadow. The shadow is blurred more towards the top of the
//       texture.
//--------------------------------------------------------------------------------------
float4 ShadowBlurPS( float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float fColorSum = 0.0f;
    float fScale    = 0.02f * (1.0f-vTexCoord.y);

    // Run through all taps in the Poisson Disc
    for( int i = 0; i < NUM_BLUR_TAPS; i++ )
    {
        // Compute tap coordinates
        float2 vTapCoord = vTexCoord + g_vFilterTaps[i] * fScale;

        // Accumulate color contribution
        if( tex2D( ShadowMapSampler, vTapCoord ).r < 1.0f )
            fColorSum += 1.0f;
    }

    // Divide down and invert the accumulated color
    float fFinalColor = 1.0f - (fColorSum / NUM_BLUR_TAPS);

    return fFinalColor;
}


//--------------------------------------------------------------------------------------
// Name: CompositeFilteredFogPS()
// Desc: Routine to combine four channels of fog buffer into one and apply Poisson filter
//--------------------------------------------------------------------------------------
float4 CompositeFilteredFogPS( float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float fColorSum   = 0.000f;
    float fScale      = 0.002f;
    float fFinalColor = 0.000f;

    // Run through all taps in the Poisson Disc
    for( int i = 0; i < 9; i++ )
    {
        // Compute tap coordinates
        float2 vTapCoord = vTexCoord + g_vFilterTaps[i] * fScale;

        // Accumulate color and contribution
        fColorSum += tex2D( FogTextureSampler, vTapCoord ).r;
    }

    // Return the accumulated color
    float fMaxIntensity = 0.25f;
    return fMaxIntensity * fColorSum;
}

