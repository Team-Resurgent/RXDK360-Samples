//--------------------------------------------------------------------------------------
// Billboard.fx
//
// This FXLite effect is part of a billboarded particle system.  The input data to the
// vertex shader is a stream of points, each containing a number of parameters.  The
// vertex shader expands each point into a quad using custom vertex fetching, and
// performs positioning and rotation on the billboards.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

shared float4x4     world_view_proj_matrix : register(c0);
float4              camera_right_vector : register(c4);
float4              camera_up_vector : register(c5);

#define TEXTURE_SAMPLER sampler_state { MipFilter = LINEAR; MinFilter = ANISOTROPIC; MagFilter = ANISOTROPIC; AddressU = WRAP; AddressV = WRAP; MAXANISOTROPY = 16; }
sampler2D           diffuse_texture : register(s0) = TEXTURE_SAMPLER;

// The corner_vectors constant data is used to build the four corners of a quad.
const float2        corner_vectors[4] = { float2( -1, -1 ), float2( 1, -1 ), float2( 1, 1 ), float2( -1, 1 ) };

// The VS_INPUT structure matches the ParticleRenderData structure in ParticleSystem.h.
struct VS_INPUT
{
    float3  Position: POSITION;
    float2  SizeAspect: TEXCOORD0;
    float4  UVRect: TEXCOORD1;
    float4  AxisAngle: TEXCOORD2;
    float4  Color: COLOR0;
    float2  CenterOffset: TEXCOORD3;
};

struct VS_OUTPUT
{
    float4  Position: POSITION;
    float4  Color : COLOR;
    float2  Tex0 : TEXCOORD0;
};

struct PS_INPUT
{
    float4  Color : COLOR;
    float2  Tex0 : TEXCOORD0;
};

VS_OUTPUT vs_main( int Index : INDEX )
{
    // iDiv holds the point index.  The shader is run 4 times for each point.
    int iDiv = Index / 4;
    int fetchIndex = iDiv;
    float4 vPosition;
    float4 vSizeAspect;
    float4 vUVRect;
    float4 vAxisAngle;
    float4 vColor;
    float4 vCenterOffset;
    // Fetch the particle data using iDiv.
    asm
    {
        vfetch vPosition, fetchIndex, position0;
        vfetch vSizeAspect, fetchIndex, texcoord0;
        vfetch vUVRect, fetchIndex, texcoord1;
        vfetch vAxisAngle, fetchIndex, texcoord2;
        vfetch vColor, fetchIndex, color0;
        vfetch vCenterOffset, fetchIndex, texcoord3;
    };

    // iMod is the corner index.
    int iMod = Index - ( iDiv * 4 );
    float2 vCorner = corner_vectors[iMod];

    // Compute the texture coordinates for the quad corner using the UV rect data.
    float2 vTexUV = ( ( vCorner * 0.5 + 0.5 ) * vUVRect.zw ) + vUVRect.xy;

    // Apply the aspect ratio to the corners.
    vCorner.x *= vSizeAspect.y;

    // Offset the corners by the center offset data (which is also affected by aspect ratio).
    vCenterOffset.x *= vSizeAspect.y;
    vCorner += vCenterOffset;

    // Compute sin and cos of the rotation angle.
    float fSin, fCos;
    sincos( vAxisAngle.w, fSin, fCos );

    // Rotate the corner vector by the rotation angle.
    float2 vCornerRotated;
    vCornerRotated.x = vCorner.x * fCos - vCorner.y * fSin;
    vCornerRotated.y = vCorner.x * fSin + vCorner.y * fCos;

    // Scale the corner vector and transform into world space.
    float4 vCornerPosWorld = vPosition + ( vCornerRotated.x * vSizeAspect.x * camera_right_vector ) + ( vCornerRotated.y * vSizeAspect.x * camera_up_vector );

    // Transform the corner vector into homogenous space, and output color and UV.
    VS_OUTPUT Out;
    Out.Position = mul( vCornerPosWorld, world_view_proj_matrix );
    Out.Tex0 = vTexUV;
    Out.Color = vColor;

    return Out;
};

float4 ps_main( PS_INPUT In ) : COLOR
{
    return In.Color;
};

float4 ps_main_tex( PS_INPUT In ) : COLOR
{
    return In.Color * tex2D( diffuse_texture, In.Tex0 );
};

technique Additive
{
    pass
    {
        cullmode = none;
        zenable = true;
        zfunc = lessequal;
        zwriteenable = false;
        alphablendenable = true;
        srcblend = srcalpha;
        destblend = one;
        vertexshader = compile vs_3_0 vs_main();
        pixelshader = compile ps_3_0 ps_main_tex();
    }
}
