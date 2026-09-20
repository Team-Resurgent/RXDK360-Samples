//--------------------------------------------------------------------------------------
// PostProcess.hlsl
//
// Common shaders for the ATG::PostProcess class which facillitates full-screen post-
// processing operations like bloom, blur, etc..
//
// Note that each effect used by the ATG::PostProcess class requires that one or more
// corresponding shaders, found below, be built with the project.
// 
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Global constants
//--------------------------------------------------------------------------------------
static const int    MAX_SAMPLES            = 16;    // Maximum texture grabs
static const float  BRIGHT_PASS_THRESHOLD  = 5.0f;  // Threshold for BrightPass filter
static const float  BRIGHT_PASS_OFFSET     = 10.0f; // Offset for BrightPass filter

// The per-color weighting to be used for luminance calculations in RGB order.
static const float3 LUMINANCE_VECTOR  = float3(0.2125f, 0.7154f, 0.0721f);


//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------

// Tone mapping variables
uniform float    g_fMiddleGray      : register(c5);  // The middle gray key value
uniform float    g_fElapsedTime     : register(c7);  // Time in seconds since the last calculation

uniform float    g_fBloomScale      : register(c10);  // Bloom process multiplier
uniform float    g_fStarScale       : register(c11);  // Star process multiplier

// Contains sampling offsets used by the techniques

#ifdef EntryPoint_SampleLumInitialPS
#define USE_SAMPLEOFFSETS
#endif

#ifdef EntryPoint_SampleLumFinalPS
#define USE_SAMPLEOFFSETS
#endif

#ifdef EntryPoint_DownScale4x4PS
#define USE_SAMPLEOFFSETS
#endif

#ifdef EntryPoint_DownScale2x2PS
#define USE_SAMPLEOFFSETS
#endif

#ifdef EntryPoint_GaussBlur5x5PS
#define USE_SAMPLEOFFSETS
#define USE_SAMPLEWEIGHTS
#endif

#ifdef EntryPoint_BloomPS
#define USE_SAMPLEOFFSETS
#define USE_SAMPLEWEIGHTS
#endif

#ifdef EntryPoint_StarPS
#define USE_SAMPLEOFFSETS
#define USE_SAMPLEWEIGHTS
#endif

#ifdef EntryPoint_MergeTextures_1PS
#define USE_SAMPLEWEIGHTS
#endif
#ifdef EntryPoint_MergeTextures_2PS
#define USE_SAMPLEWEIGHTS
#endif
#ifdef EntryPoint_MergeTextures_3PS
#define USE_SAMPLEWEIGHTS
#endif
#ifdef EntryPoint_MergeTextures_4PS
#define USE_SAMPLEWEIGHTS
#endif
#ifdef EntryPoint_MergeTextures_5PS
#define USE_SAMPLEWEIGHTS
#endif
#ifdef EntryPoint_MergeTextures_6PS
#define USE_SAMPLEWEIGHTS
#endif
#ifdef EntryPoint_MergeTextures_7PS
#define USE_SAMPLEWEIGHTS
#endif
#ifdef EntryPoint_MergeTextures_8PS
#define USE_SAMPLEWEIGHTS
#endif

#ifdef USE_SAMPLEOFFSETS
uniform float4   g_avSampleOffsets[MAX_SAMPLES] : register(c0);
#endif // USE_SAMPLEOFFSETS

#ifdef USE_SAMPLEWEIGHTS
uniform float4   g_avSampleWeights[MAX_SAMPLES] : register(c16);
#endif // USE_SAMPLEWEIGHTS


//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
sampler s0  : register(s0);
sampler s1  : register(s1);
sampler s2  : register(s2);
sampler s3  : register(s3);
sampler s4  : register(s4);
sampler s5  : register(s5);
sampler s6  : register(s6);
sampler s7  : register(s7);
sampler s8  : register(s8);
sampler s9  : register(s9);
sampler s10 : register(s10);
sampler s11 : register(s11);
sampler s12 : register(s12);
sampler s13 : register(s13);
sampler s14 : register(s14);
sampler s15 : register(s15);


//--------------------------------------------------------------------------------------
// Vertex shaders
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Name: ScreenSpaceShaderVS()
// Desc: Pass-thru shader
//--------------------------------------------------------------------------------------
struct PASSTHRU_VERTEX
{
    float4 Position   : POSITION;
    float2 TexCoords  : TEXCOORD0;
};

PASSTHRU_VERTEX ScreenSpaceShaderVS( float2 vPosition  : POSITION, 
                                     float2 vTexCoords : TEXCOORD0 )
{
    PASSTHRU_VERTEX Output;
    Output.Position  = float4( vPosition.x, vPosition.y, 1.0f, 1.0f );
    Output.TexCoords = vTexCoords;
    return Output;
}


//--------------------------------------------------------------------------------------
// Pixel shaders
//--------------------------------------------------------------------------------------


#ifdef EntryPoint_SampleLumInitialPS

//--------------------------------------------------------------------------------------
// Name: SampleLumInitialPS()
// Desc: Sample the luminance of the source image using a kernal of sample points, and
//       return a scaled image containing the log() of averages
//--------------------------------------------------------------------------------------
float4 SampleLumInitialPS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    const int NUM_SAMPLES = 9;
    float fSum = 0.0f;

    for( int i = 0; i < NUM_SAMPLES; i++ )
    {
        // Compute the sum of log(luminance) throughout the sample points
        float3 vColor     = tex2D( s0, vScreenPosition + g_avSampleOffsets[i].xy ).rgb;
        float  fLuminance = dot( vColor, LUMINANCE_VECTOR );
        fSum += log( fLuminance + 0.0001f );
    }
    
    // Divide the sum to complete the average
    fSum /= NUM_SAMPLES;

    return float4( fSum, fSum, fSum, 1.0f );
}

#endif // EntryPoint_SampleLumInitialPS


#ifdef EntryPoint_SampleLumFinalPS

//--------------------------------------------------------------------------------------
// Name: SampleLumFinalPS()
// Desc: Extract the average luminance of the image by completing the averaging and
//       taking the exp() of the result
//--------------------------------------------------------------------------------------
float4 SampleLumFinalPS( /*in float2 vScreenPosition : TEXCOORD0*/ ) : COLOR
{
    // Note that Alpha hardware can't resolve to textures less than 32x32, so the
    // dependant textures are slightly bogus. This will work better on final HW, where
    // we can use unmodified tex coords.
    float2 vScreenPosition;
    vScreenPosition.x = 2.0f/64.0f;
    vScreenPosition.y = 2.0f/64.0f;
    
    const int NUM_SAMPLES = 16;
    float fSum = 0.0f;

    for( int i = 0; i < NUM_SAMPLES; i++ )
    {
        // Compute the sum of luminance throughout the sample points
        fSum += tex2D( s0, vScreenPosition + g_avSampleOffsets[i].xy ).x;
    }
    
    // Divide the sum to complete the average
    fSum /= NUM_SAMPLES;
    
    // Perform an exp() to complete the average luminance calculation
    fSum = exp( fSum );
    
    return float4( fSum, fSum, fSum, 1.0f );
}

#endif // EntryPoint_SampleLumFinalPS


//--------------------------------------------------------------------------------------
// Name: CalculateAdaptedLumPS()
// Desc: Calculate the luminance that the camera is current adapted to, using the most
//       recent adaptation level, the current scene luminance, and the time elapsed
//       since last calculated
//--------------------------------------------------------------------------------------
float4 CalculateAdaptedLumPS() : COLOR
{
    float fAdaptedLum = tex2D( s0, float2(0.5f, 0.5f) ).r;
    float fCurrentLum = tex2D( s1, float2(0.5f, 0.5f) ).r;

    // The user's adapted luminance level is simulated by closing the gap between
    // adapted luminance and current luminance by 2% every frame, based on a 30 fps
    // rate. This is not an accurate model of human adaptation, which can take longer
    // than half an hour.
    float fNewAdaptation = fAdaptedLum + (fCurrentLum - fAdaptedLum) * ( 1 - pow( 0.98f, 30 * g_fElapsedTime ) );
    return float4(fNewAdaptation, fNewAdaptation, fNewAdaptation, 1.0f);
}


#ifdef EntryPoint_CopyTexturePS

//--------------------------------------------------------------------------------------
// Name: CopyTexturePS()
// Desc: Copies a texture. Scaling is controlled by the render target size.
//--------------------------------------------------------------------------------------
float4 CopyTexturePS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    float4 vColor = tex2D( s0, vScreenPosition );
    return vColor;
}

#endif // EntryPoint_CopyTexturePS

#ifdef EntryPoint_DownScale4x4PS

//--------------------------------------------------------------------------------------
// Name: DownScale4x4PS()
// Desc: Scale the source texture down to 1/16 scale
//--------------------------------------------------------------------------------------
float4 DownScale4x4PS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
#ifdef FILTERABLE
    float4 t0, t1, t2, t3;
    asm
    {
        // Fetch four filtered samples covering a 4x4 area of pixels
        tfetch2D t0, vScreenPosition, s0, OffsetX = -1, OffsetY = -1
        tfetch2D t1, vScreenPosition, s0, OffsetX = -1, OffsetY = +1
        tfetch2D t2, vScreenPosition, s0, OffsetX = +1, OffsetY = -1
        tfetch2D t3, vScreenPosition, s0, OffsetX = +1, OffsetY = +1
    };
    
    return ( t0 + t1 + t2 + t3 ) / 4.0f;
#else
    const int NUM_SAMPLES = 16;
    float4 vColor = 0.0f;

    for( int i=0; i < NUM_SAMPLES; i++ )
    {
        vColor += tex2D( s0, vScreenPosition + g_avSampleOffsets[i].xy );
    }
    
    return vColor / NUM_SAMPLES;
#endif
}

#endif // EntryPoint_DownScale4x4PS


#ifdef EntryPoint_DownScale2x2PS

//--------------------------------------------------------------------------------------
// Name: DownScale2x2PS()
// Desc: Scale the source texture down to 1/4 scale
//--------------------------------------------------------------------------------------
float4 DownScale2x2PS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
#ifdef FILTERABLE
    float4 t0;
    asm
    {
        // Fetch one filtered sample covering a 2x2 area of pixels
        tfetch2D t0, vScreenPosition, s0
    };

    return t0;
#else
    const int NUM_SAMPLES = 4;
    float4 vColor = 0.0f;

    for( int i=0; i < NUM_SAMPLES; i++ )
    {
        vColor += tex2D( s0, vScreenPosition + g_avSampleOffsets[i].xy );
    }
    
    return vColor / NUM_SAMPLES;
#endif
}

#endif // EntryPoint_DownScale2x2PS


#ifdef EntryPoint_GaussBlur5x5PS

//--------------------------------------------------------------------------------------
// Name: GaussBlur5x5PS()
// Desc: Simulate a 5x5 kernel gaussian blur by sampling the 13 points closest to the
//       center point.
//--------------------------------------------------------------------------------------
float4 GaussBlur5x5PS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    float4 vColor = 0.0f;

    for( int i=0; i < 13; i++ )
    {
        vColor += g_avSampleWeights[i] * tex2D( s0, vScreenPosition + g_avSampleOffsets[i].xy );
    }

    return vColor;
}

#endif // EntryPoint_GaussBlur5x5PS


#ifdef EntryPoint_GaussBlur5x5HorizontalPS

//--------------------------------------------------------------------------------------
// Name: GaussBlur5x5HorizontalPS()
// Desc: Horizontal portion of a two-pass separable 5x5 Gaussian blur
//--------------------------------------------------------------------------------------
float4 GaussBlur5x5HorizontalPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    // Fetch 4 samples which filter across 5 pixels, P0..P4
    float4 t0, t1, t2, t3;
    asm
    {
        tfetch2D t0, vTexCoord, s0, OffsetX = +1.5 // Fetch (P0+P1) / 2
        tfetch2D t1, vTexCoord, s0, OffsetX = +0.5 // Fetch (P1+P2) / 2
        tfetch2D t2, vTexCoord, s0, OffsetX = -0.5 // Fetch (P2+P3) / 2
        tfetch2D t3, vTexCoord, s0, OffsetX = -1.5 // Fetch (P3+P4) / 2
    };
    
    // For five pixels, the Gaussian weights are: 1/16, 4/16, 6/16, 4/16, 1/16
    // Working backwards, the fetched values are related to the 5 pixels as:
    //    P0 = 1*t0
    //    P1 = 1*t0 + 3*t1;
    //    P2 =        3*t1 + 3*t2;
    //    P3 =               3*t2 + 1*t3;
    //    P4 =                      1*t3;
    // TOTAL = 2*t0 + 6*t1 + 6*t2 + 2*t3;

    // Sum results and apply Gaussian weights
    return 2.0/16 * t0 + 
           6.0/16 * t1 + 
           6.0/16 * t2 + 
           2.0/16 * t3;
}

#endif // EntryPoint_GaussBlur5x5HorizontalPS


#ifdef EntryPoint_GaussBlur5x5VerticalPS

//--------------------------------------------------------------------------------------
// Name: GaussBlur5x5VerticalPS()
// Desc: Vertical portion of a two-pass separable 5x5 Gaussian blur
//--------------------------------------------------------------------------------------
float4 GaussBlur5x5VerticalPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    // Fetch 4 samples which filter across 5 pixels, P0..P4
    float4 t0, t1, t2, t3;
    asm
    {
        tfetch2D t0, vTexCoord, s0, OffsetY = +1.5 // Fetch (P0+P1) / 2
        tfetch2D t1, vTexCoord, s0, OffsetY = +0.5 // Fetch (P1+P2) / 2
        tfetch2D t2, vTexCoord, s0, OffsetY = -0.5 // Fetch (P2+P3) / 2
        tfetch2D t3, vTexCoord, s0, OffsetY = -1.5 // Fetch (P3+P4) / 2
    };
    
    // For five pixels, the Gaussian weights are: 1/16, 4/16, 6/16, 4/16, 1/16
    // Working backwards, the fetched values are related to the 5 pixels as:
    //    P0 = 1*t0
    //    P1 = 1*t0 + 3*t1;
    //    P2 =        3*t1 + 3*t2;
    //    P3 =               3*t2 + 1*t3;
    //    P4 =                      1*t3;
    // TOTAL = 2*t0 + 6*t1 + 6*t2 + 2*t3;

    // Sum results and apply Gaussian weights
    return 2.0/16 * t0 + 
           6.0/16 * t1 + 
           6.0/16 * t2 + 
           2.0/16 * t3;
}

#endif // EntryPoint_GaussBlur5x5VerticalPS


#ifdef EntryPoint_GaussBlur3x3HorizontalPS

//--------------------------------------------------------------------------------------
// Name: GaussBlur3x3HorizontalPS()
// Desc: Horizontal portion of a two-pass separable 3x3 Gaussian blur
//--------------------------------------------------------------------------------------
float4 GaussBlur3x3HorizontalPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    // Fetch 2 samples which filter across 3 pixels, P0..P2
    float4 t0, t1;
    asm
    {
        tfetch2D t0, vTexCoord, s0, OffsetX = +0.5 // Fetch (P0+P1) / 2
        tfetch2D t1, vTexCoord, s0, OffsetX = -0.5 // Fetch (P1+P2) / 2
    };
    
    // For five pixels, the Gaussian weights are: 1/4, 2/4, 1/4
    // Working backwards, the fetched values are related to the 3 pixels as:
    //    P0 = 1*t0
    //    P1 = 1*t0 + 1*t1;
    //    P2 =        1*t1;
    // TOTAL = 2*t0 + 2*t1;

    // Sum results and apply Gaussian weights
    return 2.0/4 * t0 + 
           2.0/4 * t1;
}

#endif // EntryPoint_GaussBlur3x3HorizontalPS


#ifdef EntryPoint_GaussBlur3x3VerticalPS

//--------------------------------------------------------------------------------------
// Name: GaussBlur3x3VerticalPS()
// Desc: Vertical portion of a two-pass separable 3x3 Gaussian blur
//--------------------------------------------------------------------------------------
float4 GaussBlur3x3VerticalPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    // Fetch 2 samples which filter across 3 pixels, P0..P2
    float4 t0, t1;
    asm
    {
        tfetch2D t0, vTexCoord, s0, OffsetY = +0.5 // Fetch (P0+P1) / 2
        tfetch2D t1, vTexCoord, s0, OffsetY = -0.5 // Fetch (P1+P2) / 2
    };
    
    // For five pixels, the Gaussian weights are: 1/4, 2/4, 1/4
    // Working backwards, the fetched values are related to the 3 pixels as:
    //    P0 = 1*t0
    //    P1 = 1*t0 + 1*t1;
    //    P2 =        1*t1;
    // TOTAL = 2*t0 + 2*t1;

    // Sum results and apply Gaussian weights
    return 2.0/4 * t0 + 
           2.0/4 * t1;
}

#endif // EntryPoint_GaussBlur3x3VerticalPS


//--------------------------------------------------------------------------------------
// Name: BrightPassFilterPS()
// Desc: Perform a high-pass filter on the source texture
//--------------------------------------------------------------------------------------
float4 BrightPassFilterPS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    float4 vSample     = tex2D( s0, vScreenPosition );
    float  fAdaptedLum = tex2D( s1, float2(0.5f, 0.5f) ).r;

    // Determine what the pixel's value will be after tone-mapping occurs
    vSample.rgb *= g_fMiddleGray / ( fAdaptedLum + 0.001f );

    // Subtract out dark pixels
    vSample.rgb -= BRIGHT_PASS_THRESHOLD;
    
    // Clamp to 0
    vSample = max( vSample, 0.0f );
    
    // Map the resulting value into the 0 to 1 range. Higher values for
    // BRIGHT_PASS_OFFSET will isolate lights from illuminated scene 
    // objects.
    vSample.rgb /= ( BRIGHT_PASS_OFFSET + vSample );
    
    return vSample;
}


#ifdef EntryPoint_BloomPS

//--------------------------------------------------------------------------------------
// Name: BloomPS()
// Desc: Blur the source image along one axis using a gaussian distribution. Since
//       gaussian blurs are separable, this shader is called twice; first along the
//       horizontal axis, then along the vertical axis.
//--------------------------------------------------------------------------------------
float4 BloomPS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    float4 vColor = 0.0f;
    
    // Perform a one-directional gaussian blur
    for( int i = 0; i < 15; i++ )
    {
        vColor += g_avSampleWeights[i] * tex2D( s0, vScreenPosition + g_avSampleOffsets[i].xy );
    }
    return vColor;
}

#endif // EntryPoint_BloomPS


#ifdef EntryPoint_StarPS

//--------------------------------------------------------------------------------------
// Name: StarPS()
// Desc: Each star is composed of up to 8 lines, and each line is created by up to three
//       passes of this shader, which samples from 8 points along the current line.
//--------------------------------------------------------------------------------------
float4 StarPS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    float4 vColor = 0.0f;
    
    // Sample from eight points along the star line
    for( int i = 0; i < 8; i++ )
    {
        vColor += g_avSampleWeights[i] * tex2D(s0, vScreenPosition + g_avSampleOffsets[i].xy);
    }
        
    return vColor;
}

#endif // EntryPoint_StarPS


#ifdef EntryPoint_MergeTextures_1PS

//--------------------------------------------------------------------------------------
// Name: MergeTextures_N()
// Desc: Return the average of N input textures
//--------------------------------------------------------------------------------------
float4 MergeTextures_1PS(   in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    float4 vColor = 0.0f;
    vColor += g_avSampleWeights[0] * tex2D(s0, vScreenPosition);
    return vColor;
}

#endif // EntryPoint_MergeTextures_1PS


#ifdef EntryPoint_MergeTextures_2PS

//--------------------------------------------------------------------------------------
// Name: MergeTextures_N()
// Desc: Return the average of N input textures
//--------------------------------------------------------------------------------------
float4 MergeTextures_2PS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    float4 vColor = 0.0f;
    vColor += g_avSampleWeights[0] * tex2D(s0, vScreenPosition);
    vColor += g_avSampleWeights[1] * tex2D(s1, vScreenPosition);
    return vColor;
}

#endif // EntryPoint_MergeTextures_2PS


#ifdef EntryPoint_MergeTextures_3PS

//--------------------------------------------------------------------------------------
// Name: MergeTextures_N()
// Desc: Return the average of N input textures
//--------------------------------------------------------------------------------------
float4 MergeTextures_3PS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    float4 vColor = 0.0f;
    vColor += g_avSampleWeights[0] * tex2D(s0, vScreenPosition);
    vColor += g_avSampleWeights[1] * tex2D(s1, vScreenPosition);
    vColor += g_avSampleWeights[2] * tex2D(s2, vScreenPosition);
    return vColor;
}

#endif // EntryPoint_MergeTextures_3PS


#ifdef EntryPoint_MergeTextures_4PS

//--------------------------------------------------------------------------------------
// Name: MergeTextures_N()
// Desc: Return the average of N input textures
//--------------------------------------------------------------------------------------
float4 MergeTextures_4PS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    float4 vColor = 0.0f;
    vColor += g_avSampleWeights[0] * tex2D(s0, vScreenPosition);
    vColor += g_avSampleWeights[1] * tex2D(s1, vScreenPosition);
    vColor += g_avSampleWeights[2] * tex2D(s2, vScreenPosition);
    vColor += g_avSampleWeights[3] * tex2D(s3, vScreenPosition);
    return vColor;
}

#endif // EntryPoint_MergeTextures_4PS


#ifdef EntryPoint_MergeTextures_5PS

//--------------------------------------------------------------------------------------
// Name: MergeTextures_N()
// Desc: Return the average of N input textures
//--------------------------------------------------------------------------------------
float4 MergeTextures_5PS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    float4 vColor = 0.0f;
    vColor += g_avSampleWeights[0] * tex2D(s0, vScreenPosition);
    vColor += g_avSampleWeights[1] * tex2D(s1, vScreenPosition);
    vColor += g_avSampleWeights[2] * tex2D(s2, vScreenPosition);
    vColor += g_avSampleWeights[3] * tex2D(s3, vScreenPosition);
    vColor += g_avSampleWeights[4] * tex2D(s4, vScreenPosition);
    return vColor;
}

#endif // EntryPoint_MergeTextures_5PS


#ifdef EntryPoint_MergeTextures_6PS

//--------------------------------------------------------------------------------------
// Name: MergeTextures_N()
// Desc: Return the average of N input textures
//--------------------------------------------------------------------------------------
float4 MergeTextures_6PS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    float4 vColor = 0.0f;
    vColor += g_avSampleWeights[0] * tex2D(s0, vScreenPosition);
    vColor += g_avSampleWeights[1] * tex2D(s1, vScreenPosition);
    vColor += g_avSampleWeights[2] * tex2D(s2, vScreenPosition);
    vColor += g_avSampleWeights[3] * tex2D(s3, vScreenPosition);
    vColor += g_avSampleWeights[4] * tex2D(s4, vScreenPosition);
    vColor += g_avSampleWeights[5] * tex2D(s5, vScreenPosition);
    return vColor;
}

#endif // EntryPoint_MergeTextures_6PS


#ifdef EntryPoint_MergeTextures_7PS

//--------------------------------------------------------------------------------------
// Name: MergeTextures_N()
// Desc: Return the average of N input textures
//--------------------------------------------------------------------------------------
float4 MergeTextures_7PS(   in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    float4 vColor = 0.0f;
    vColor += g_avSampleWeights[0] * tex2D(s0, vScreenPosition);
    vColor += g_avSampleWeights[1] * tex2D(s1, vScreenPosition);
    vColor += g_avSampleWeights[2] * tex2D(s2, vScreenPosition);
    vColor += g_avSampleWeights[3] * tex2D(s3, vScreenPosition);
    vColor += g_avSampleWeights[4] * tex2D(s4, vScreenPosition);
    vColor += g_avSampleWeights[5] * tex2D(s5, vScreenPosition);
    vColor += g_avSampleWeights[6] * tex2D(s6, vScreenPosition);
    return vColor;
}

#endif // EntryPoint_MergeTextures_7PS


#ifdef EntryPoint_MergeTextures_8PS

//--------------------------------------------------------------------------------------
// Name: MergeTextures_N()
// Desc: Return the average of N input textures
//--------------------------------------------------------------------------------------
float4 MergeTextures_8PS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    float4 vColor = 0.0f;
    vColor += g_avSampleWeights[0] * tex2D(s0, vScreenPosition);
    vColor += g_avSampleWeights[1] * tex2D(s1, vScreenPosition);
    vColor += g_avSampleWeights[2] * tex2D(s2, vScreenPosition);
    vColor += g_avSampleWeights[3] * tex2D(s3, vScreenPosition);
    vColor += g_avSampleWeights[4] * tex2D(s4, vScreenPosition);
    vColor += g_avSampleWeights[5] * tex2D(s5, vScreenPosition);
    vColor += g_avSampleWeights[6] * tex2D(s6, vScreenPosition);
    vColor += g_avSampleWeights[7] * tex2D(s7, vScreenPosition);
    return vColor;
}

#endif // EntryPoint_MergeTextures_8PS
