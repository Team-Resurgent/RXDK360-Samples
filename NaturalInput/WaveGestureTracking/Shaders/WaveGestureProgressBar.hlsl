//--------------------------------------------------------------------------------------
// WaveGestureProgressBar.hlsl
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Vertex shader
//--------------------------------------------------------------------------------------

Texture ArrowTexture          : register(t0);  // Arrow texture 
sampler ArrowTextureSampler = sampler_state 
{ 
    texture =  <xArrowTexture> ;
    magfilter = LINEAR; 
    minfilter = LINEAR; 
    mipfilter = LINEAR; 
    AddressU = mirror; 
    AddressV = mirror;
};

struct VSOUT
{
   float4 Position  : POSITION;
   float2 TexCoord0 : TEXCOORD0;
};

VSOUT VS_EntryPoint( const float3 Position   : POSITION, 
               const float2 TexCoord0  : TEXCOORD0 )
{
    VSOUT  Output; 

    Output.Position = float4( Position, 1.0f );

    Output.TexCoord0 = TexCoord0;

    return Output;
}


//--------------------------------------------------------------------------------------
// Pixel shader
//--------------------------------------------------------------------------------------
uniform float4   ProgressEmptyColor     : register(c0);  // Progress bar empty color
uniform float4   ProgressFillColor      : register(c1);  // Progress bar fill color
uniform float4   ProgressBarParams      : register(c2);  // The progress bar params: x is the progress between 0.0f and 1.0f
                                                         //                          y is 1.0f if filling from right to left, -1.0f if filling from left to right
                                                         //                          z and w are unused
float4 PS_EntryPoint( VSOUT Input ) : COLOR
{

    float4 output = tex2D(ArrowTextureSampler, Input.TexCoord0);

    // Center of arrow arc.
    float4 center = float4( 0.50f, 1.5f, 0.00f, 1.00f );
    float4 arrowMostLeftPoint = float4( 0.00f, 0.35f, 0.00f, 1.00f );

    // Find the angle between current pixel and the y-parallel axis that passes through the center of arrow arc.
    float thetaCurentPixel = - atan( ( Input.TexCoord0.x - center.x ) / ( Input.TexCoord0.y - center.y ) );

    // Find the angle between current pixel and the y-parallel axis that passes through the center of arrow arc.
    float thetaMax = atan( ( arrowMostLeftPoint.x - center.x ) / ( arrowMostLeftPoint.y - center.y ) );
    
    // The angle of the arc that we will fill with empty-fill color gradient.
    // currently set at 60% of the whole graphics. For example, if the progress is 
    // zero, then we will paint 60% of the arrow graphic with a gradeint fill of empty/filled colors. 
    float gradientArcAngle = thetaMax * 0.60f;

    // Find the angle that corresponds to the progress value. A progress value of 0 should result in -theta, and 
    // a progress value of 1 should result in +theta. 
    float thetaProgress = ( 2.0f * ProgressBarParams.x - 1.0f ) * thetaMax;

    // All calculations we have done so far are for a left to right filling progress bar. 
    // A negative ProgressBarVars.y indicates that progress bar should be filled from right to left.
    thetaProgress  = ProgressBarParams.y * thetaProgress;

    // Find the scale factor for the gradient fill. s = 0 will result in filled color, and 
    // s= 0 results in using empty color. 
    float s = ( thetaCurentPixel - thetaProgress + ( gradientArcAngle / 2.0f ) ) / gradientArcAngle;
    s = saturate(s);

    // In order to adjust for a right-to-left filling progress bar, we need to swap the 
    // filled and empty colors.
    if( ProgressBarParams.y < 0.0f )
    {
         s = 1-s;
    }

    output = output * lerp( ProgressFillColor, ProgressEmptyColor, s );

    return output;
}


