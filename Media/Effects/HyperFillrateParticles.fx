//--------------------------------------------------------------------------------------
// Billboard.fx
//
// This FXLite effect is part of a billboarded particle system.  The input data to the
// vertex shader is a stream of points, each containing a number of parameters.  The
// vertex shader expands each point into a quad using custom vertex fetching, and
// performs positioning and rotation on the billboards.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

shared float4x4     world_view_proj_matrix : register(c0);
float4              camera_right_vector : register(c4);
float4              camera_up_vector : register(c5);

#define TEXTURE_SAMPLER sampler_state { MipFilter = POINT; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = WRAP; AddressV = WRAP; MAXANISOTROPY = 16; }
sampler2D           diffuse_texture : register(s0) = TEXTURE_SAMPLER;

// The corner_vectors constant data is used to build the four corners of a quad.
const float2        corner_vectors[4] = { float2( -1, -1 ), float2( 1, -1 ), float2( 1, 1 ), float2( -1, 1 ) };

// The VS_INPUT structure matches the ParticleRenderData structure in ParticleSystem.h.
struct VS_INPUT
{
    float4  Position: POSITION;
    float2  SizeAspect: TEXCOORD0;
    float4  UVRect: TEXCOORD1;
    float2  CenterOffset: TEXCOORD2;
};

struct VS_OUTPUT
{
    float4  Position: POSITION;
    float4  Tex0 : TEXCOORD0_centroid;
};

struct PS_INPUT
{
	// Note: UV.z contains a fade-out value. UV.w is still unused.
    float4  Tex0 : TEXCOORD0_centroid;
};

VS_OUTPUT vs_main( int Index : INDEX )
{
    // iDiv holds the point index.  The shader is run 4 times for each point.
    int iDiv = Index / 4;
    int fetchIndex = iDiv;
    
    float4 vPosition;
    float4 vSizeAspect;
    float4 vUVRect;
    float4 vCenterOffset;
    // Fetch the particle data using iDiv.
    asm
    {
        vfetch vPosition, fetchIndex, position0;
        vfetch vSizeAspect, fetchIndex, texcoord0;
        vfetch vUVRect, fetchIndex, texcoord1;
        vfetch vCenterOffset, fetchIndex, texcoord2;
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

	float1 vColor = vPosition.w;
	vPosition.w = 1;
	
    // Scale the corner vector and transform into world space.
    float4 vCornerPosWorld = vPosition + ( vCorner.x * vSizeAspect.x * camera_right_vector ) + ( vCorner.y * vSizeAspect.x * camera_up_vector );

    // Transform the corner vector into homogenous space, and output color and UV.
    VS_OUTPUT Out;
    Out.Position = mul( vCornerPosWorld, world_view_proj_matrix );
    Out.Tex0.xy = vTexUV;
    Out.Tex0.z = vColor.x; // store alpha in w component of position
    Out.Tex0.w = 0.0f;

    return Out;
};

float4 ps_main_tex_fade( PS_INPUT In ) : COLOR
{
	float4 Diffuse = float4( 1.0, 0.5, 0.2, 1.f );
    return tex2D( diffuse_texture, In.Tex0.xy ) * Diffuse * In.Tex0.z;
};

float4 ps_main_tex_nofade( PS_INPUT In ) : COLOR
{
    return tex2D( diffuse_texture, In.Tex0.xy ) * In.Tex0.z;
};

technique Additive
{
    pass
    {
        cullmode = none;
        zenable = true;
        zfunc = greaterequal;
        zwriteenable = false;
        alphablendenable = true;
        srcblend = srcalpha;
        destblend = one;
        blendop = add;
        vertexshader = compile vs_3_0 vs_main();
        pixelshader = compile ps_3_0 ps_main_tex_fade();
    }
}

technique Smoke
{
    pass
    {
        cullmode = none;
        zenable = true;
        zfunc = greaterequal;
        zwriteenable = false;
        alphablendenable = true;
		
		// subtractive blend - cartoony saturation. modulate is more physically realistic and produces gray colors.
		// modulate blend, set srcblend=0, destblend=INVSRCCOLOR,blendop=add
        srcblend = 0;
        destblend = INVSRCCOLOR;
        blendop = add;

        vertexshader = compile vs_3_0 vs_main();
        pixelshader = compile ps_3_0 ps_main_tex_nofade();
    }
}
