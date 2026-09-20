//--------------------------------------------------------------------------------------
// LightShafts_VolVizShells.hlsl
//
// Shaders for vol viz light shaft rendering in the LightShafts sample
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// ATI 3D Application Research Group. 
// Copyright (C) ATI Research, Inc. All rights reserved. 
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Vertex shader constants
//--------------------------------------------------------------------------------------
uniform float4x4 g_matWorldViewProj         : register(c0);
uniform float4x4 g_matWorldLightProj        : register(c4);
uniform float4x4 g_matWorldLight            : register(c8); // Transform to light space matrix
uniform float4x4 g_matWorldViewInv          : register(c12);
uniform float4x4 g_matWorldLightProjBias    : register(c20); // Transform to biased projective light space
uniform float4x4 g_matWorldLightProjScroll1 : register(c24); // Transform to scrolling projective light space
uniform float4x4 g_matWorldLightProjScroll2 : register(c28); // Transform to scrolling projective light space

uniform float    g_fFarPlane                : register(c32);
uniform float3   g_vMinBounds               : register(c33); // View-space bounding volume minimum vert
uniform float3   g_vMaxBounds               : register(c34); // View-space bounding volume maximum vert

uniform float    g_fFractionOfMaxShellsVS   : register(c40);  // Scale factor based on number of shells actually drawn


//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------
uniform float    g_fFractionOfMaxShellsPS          : register(c0);  // Scale factor based on number of shells actually drawn

const   bool     g_bScrollingNoise = true;      // : register(c2);
const   bool     g_bShadowMapping  = true;      // : register(c3);


//--------------------------------------------------------------------------------------
// Samplers
//--------------------------------------------------------------------------------------
sampler CookieSampler         : register(s1);
sampler ScrollingNoiseSampler : register(s2);
sampler ShadowMapSampler      : register(s3);


//--------------------------------------------------------------------------------------
// Vertex structures
//--------------------------------------------------------------------------------------
struct VS_VOLVIZSHELLS_OUTPUT
{
   float4 vPos                 : POSITION;
   float4 vCookieProjTexCoord  : TEXCOORD0;
   float4 vScroll1ProjTexCoord : TEXCOORD1;
   float4 vScroll2ProjTexCoord : TEXCOORD2;
   float3 vLightSpacePos       : TEXCOORD3;
   float2 vDepth               : TEXCOORD4;
};


//--------------------------------------------------------------------------------------
// Name: VolVizShellsMainVS()
// Desc: Vertex Shader which generates proper light-space texture coordinates
//--------------------------------------------------------------------------------------
VS_VOLVIZSHELLS_OUTPUT VolVizShellsMainVS( float3 vPos : POSITION )
{
    VS_VOLVIZSHELLS_OUTPUT Out = (VS_VOLVIZSHELLS_OUTPUT)0; 

    // Must stretch the planes in z to compensate for drawing fewer than all of the planes in the VB
    float3 vStretchedMaxBounds = g_vMaxBounds;
    vStretchedMaxBounds.z = vStretchedMaxBounds.z / g_fFractionOfMaxShellsVS;

    // Trilerp position within view-space-axis-aligned bounding volume of light's frustum
    float4 vViewPosition = float4( g_vMinBounds * vPos + ( vStretchedMaxBounds * (1.0f - vPos) ), 1 );

    // Calculate the position in object space
    float4 vObjectSpacePos = mul( vViewPosition, g_matWorldViewInv );

    // Output clip-space position
    Out.vPos = mul( vObjectSpacePos, g_matWorldViewProj );

    // Output projective coordinates for cookie
    Out.vCookieProjTexCoord = mul( vObjectSpacePos, g_matWorldLightProjBias );

    // Output scrolling projective coordinates for noise map
    Out.vScroll1ProjTexCoord = mul( vObjectSpacePos, g_matWorldLightProjScroll1 );
    Out.vScroll2ProjTexCoord = mul( vObjectSpacePos, g_matWorldLightProjScroll2 );

    // Position in projective light space
    float4 vLightSpacePos = mul( vObjectSpacePos, g_matWorldLightProj );

    // Light space position for scattering equations
    Out.vLightSpacePos = mul( vObjectSpacePos, g_matWorldLight );

    // Output projective light space depth (for shadow mapping)
    Out.vDepth.x = vLightSpacePos.z;
    Out.vDepth.y = vLightSpacePos.w;

    return Out;
}


//--------------------------------------------------------------------------------------
// Name: VolVizShellsNoiseShadowPS()
// Desc: 
//--------------------------------------------------------------------------------------
float4 VolVizShellsNoiseShadowPS( VS_VOLVIZSHELLS_OUTPUT In ) : COLOR
{
    // Sample the cookie and shadow map using same texture coordinates
    float4 vCookie = tex2Dproj( CookieSampler, In.vCookieProjTexCoord );

    if( vCookie.r == 0.0f )
    {
        return 0.0f;
    }
    else
    {
        // Add shadow (1.0f == pixel is in light, 0.0f == pixel is occluded)
        float fShadow = 1.0f;
        // if( g_bShadowMapping )
        {
            float fProjLightSpaceDepth = In.vDepth.x / In.vDepth.y;
            float fShadowMapDepth      = tex2Dproj( ShadowMapSampler, In.vCookieProjTexCoord ).x;
            fShadowMapDepth += 0.001f;
            fShadow = ( fProjLightSpaceDepth < fShadowMapDepth ) ? 1.0f : 0.0f;
        }
        if( fShadow == 0.0f )
        {
            return 0.0f;
        }
        else
        {
            // Add noise
            float fCompositeNoise = 0.015f;
            // if( g_bScrollingNoise )
            {
                // One noise map, but different projections and channels used
                float4 vNoise1 = tex2Dproj( ScrollingNoiseSampler, In.vScroll1ProjTexCoord );
                float4 vNoise2 = tex2Dproj( ScrollingNoiseSampler, In.vScroll2ProjTexCoord );
                fCompositeNoise = vNoise1.r * vNoise2.g * 0.05f;
            }

            // Compute attenuation 1/(s^2)
            float fAtten = 0.25f + 20000.0f / dot( In.vLightSpacePos, In.vLightSpacePos );
            float fScale = 1.0f / g_fFractionOfMaxShellsPS;

            // Composite scalar result
            return fCompositeNoise * vCookie.r * fScale * fAtten * fShadow;
        }
    }
}
