//--------------------------------------------------------------------------------------
// LightShafts_Backdrop.hlsl
//
// Shaders for backdrop rendering in the LightShafts sample
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
//      float4x4 g_matWorldLightProj        : register(c4);
uniform float4x4 g_matWorldLight            : register(c8);
uniform float4x4 g_matWorldLightProjBias    : register(c20);
uniform float4x4 g_matWorldLightProjScroll1 : register(c24);
uniform float4x4 g_matWorldLightProjScroll2 : register(c28);

uniform float    g_fFarPlane                : register(c32);


//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------
const   bool     g_bScrollingNoise = true;   // : register(c2);
const   bool     g_bShadowMapping  = true;   // : register(c3);

static const float4 g_vAmbient = { 0.3f, 0.3f, 0.3f, 0.3f };


//--------------------------------------------------------------------------------------
// Samplers
//--------------------------------------------------------------------------------------
sampler BaseTextureSampler    : register(s0);
sampler CookieSampler         : register(s1);
sampler ScrollingNoiseSampler : register(s2);
sampler ShadowMapSampler      : register(s3);


//--------------------------------------------------------------------------------------
// Vertex structures
//--------------------------------------------------------------------------------------
struct VS_BACKDROP_OUTPUT
{
    float4 vPos                 : POSITION;
    float4 vColor               : COLOR0;
    float2 vBaseTexCoord        : TEXCOORD0;
    float4 vCookieProjTexCoord  : TEXCOORD2;
    float4 vScroll1ProjTexCoord : TEXCOORD3;
    float4 vScroll2ProjTexCoord : TEXCOORD4;
    float  fDepth               : TEXCOORD5;
};

struct VS_BACKDROP_DEPTH_OUTPUT
{
    float4 vPos   : POSITION;
};

struct VS_BACKDROP_AMBIENT_OUTPUT
{
    float4 vPos      : POSITION;
    float2 vTexCoord : TEXCOORD0;
};


//--------------------------------------------------------------------------------------
// Name: BackdropMainVS()
// Desc: 
//--------------------------------------------------------------------------------------
VS_BACKDROP_OUTPUT BackdropMainVS( float3 vPosition : POSITION, 
                                   float4 vColor    : COLOR0,
                                   float2 vTexCoord : TEXCOORD0 )
{
    VS_BACKDROP_OUTPUT Out = (VS_BACKDROP_OUTPUT)0; 

    float4 vObjectSpacePos = float4( vPosition, 1 );

    // Output clip-space position
    Out.vPos = mul( vObjectSpacePos, g_matWorldViewProj );

    // Pass color through
    Out.vColor = vColor;

    // Pass through the base map texture coordinates
    Out.vBaseTexCoord = vTexCoord;

    // Output projective coordinates for cookie
    Out.vCookieProjTexCoord = mul( vObjectSpacePos, g_matWorldLightProjBias );

    // Output projective coordinates for scrolling noise maps
    Out.vScroll1ProjTexCoord = mul( vObjectSpacePos, g_matWorldLightProjScroll1 );
    Out.vScroll2ProjTexCoord = mul( vObjectSpacePos, g_matWorldLightProjScroll2 );

    // Transform to light space
    float4 vLightSpacePos = mul( vObjectSpacePos, g_matWorldLight );

    // Normalize to max depth in Spotlight volume
    Out.fDepth = vLightSpacePos.z / g_fFarPlane;

    return Out;
}


//--------------------------------------------------------------------------------------
// Name: BackdropDepthVS()
// Desc: Vertex Shader which puts interpolated depth into scalar texture coordinate
//--------------------------------------------------------------------------------------
VS_BACKDROP_DEPTH_OUTPUT BackdropDepthVS( float3 vPosition : POSITION )
{
    VS_BACKDROP_DEPTH_OUTPUT Out = (VS_BACKDROP_DEPTH_OUTPUT)0; 

    // Output clip-space position
    Out.vPos = mul( float4(vPosition,1), g_matWorldViewProj );

    return Out;
}


//--------------------------------------------------------------------------------------
// Name: BackdropAmbientOnlyVS()
// Desc: Vertex Shader which puts interpolated depth into scalar texture coordinate
//--------------------------------------------------------------------------------------
VS_BACKDROP_AMBIENT_OUTPUT BackdropAmbientOnlyVS( float3 vPosition : POSITION,
                                                  float2 vTexCoord : TEXCOORD0 )
{
    VS_BACKDROP_AMBIENT_OUTPUT Out = (VS_BACKDROP_AMBIENT_OUTPUT)0; 

    // Output clip-space position
    Out.vPos = mul( float4(vPosition,1), g_matWorldViewProj );

    // Pass texture coordinates through
    Out.vTexCoord = vTexCoord;

    return Out;
}


//--------------------------------------------------------------------------------------
// Name: BackdropNoiseShadowPS()
// Desc: 
//--------------------------------------------------------------------------------------
float4 BackdropNoiseShadowPS( VS_BACKDROP_OUTPUT In ) : COLOR
{
    // Sample the base map
    float4 vBaseColor = In.vColor * tex2D( BaseTextureSampler, In.vBaseTexCoord );

    // Sample the cookie
    float4 vCookie = tex2Dproj( CookieSampler, In.vCookieProjTexCoord );

    // Allow for a common-case, early out
    if( vCookie.r == 0.0f )
    {
        return vBaseColor * g_vAmbient;
    }
    else
    {
        // Include the scrolling noise
        float fCompositeNoise = 0.015f;
        // if( g_bScrollingNoise )
        {
            // One noise map, but different projections and channels used
            float4 vNoise1 = tex2Dproj( ScrollingNoiseSampler, In.vScroll1ProjTexCoord );
            float4 vNoise2 = tex2Dproj( ScrollingNoiseSampler, In.vScroll2ProjTexCoord );
            fCompositeNoise = vNoise1.r + vNoise2.g; 
        }

        // Perform shadow mapping. When fShadow == 0.0f it is in shadow
        // Note: this is not really a shadow map, but a grayscale map just for the backdrop
        float fShadow = 1.0f;
        // if( g_bShadowMapping )
        {
            fShadow = tex2Dproj( ShadowMapSampler, In.vCookieProjTexCoord ).x;
        }

        // Agressively knock out any back projection
        float fBackProjection = (In.fDepth <= 0.1f ? 0.0f : 1.0f);

        // Final composite
        return vBaseColor * ( fBackProjection * fShadow * vCookie * fCompositeNoise + g_vAmbient );
    }
}


//--------------------------------------------------------------------------------------
// Name: BackdropAmbientOnlyPS()
// Desc: Return the ambient-lit base texture
//--------------------------------------------------------------------------------------
float4 BackdropAmbientOnlyPS( float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    return g_vAmbient * tex2D( BaseTextureSampler, vTexCoord );
}
