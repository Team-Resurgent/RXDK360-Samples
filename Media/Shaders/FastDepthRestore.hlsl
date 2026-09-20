//---------------------------------------------------------------------------------------------------------
// FastDepthRestore.hlsl
//
// File containing utility shaders used by the FastDepthRestore sample
//
// XNA Developer Connection
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: DepthOnlyVS()
// Desc: Vertex shader that outputs depth for rendering into depth buffer
//--------------------------------------------------------------------------------------
float4x4 g_matWVP                  : register(c0);

float4 DepthOnlyVS( in float4 vPosition : POSITION ) : POSITION
{
    return mul( vPosition, g_matWVP );
}

//---------------------------------------------------------------------------------------------------------
// Declarations for oDepth shader path 
//---------------------------------------------------------------------------------------------------------
struct VERTEX_ODEPTH
{
    float2 vPosition        : POSITION0;
};

struct INTERPOLATORS_ODEPTH
{
    float4 vPosition        : POSITION0;
};

struct PIXEL_ODEPTH
{
    float4 vColor           : COLOR0;
    float fDepth            : DEPTH;
};

//---------------------------------------------------------------------------------------------------------
// Name: DepthRestoreODepthVS()
// Desc: Simple pass through. 
//---------------------------------------------------------------------------------------------------------
INTERPOLATORS_ODEPTH DepthRestoreODepthVS( VERTEX_ODEPTH In )
{
    INTERPOLATORS_ODEPTH Out;
    Out.vPosition = float4( In.vPosition, 0.0f, 1.0f );
    
    return Out;
}


//---------------------------------------------------------------------------------------------------------
// Name: DepthRestoreODepthPS()
// Desc: Restores the depth buffer by writing to oDepth. 
//
// We use the VPOS input here, even though this incurs a performance penalty, because that penalty
// is not a bottleneck.
//---------------------------------------------------------------------------------------------------------
sampler texDepth        : register(s0);

PIXEL_ODEPTH DepthRestoreODepthPS( INTERPOLATORS_ODEPTH In, float2 vPos : VPOS )
{
    PIXEL_ODEPTH Out;
    Out.vColor = 0.0f;

    float fDepth;
    asm 
    {
        tfetch2D fDepth.x___, vPos, texDepth, UnnormalizedTextureCoords=true, MinFilter=point, MagFilter=point
    };

    Out.fDepth = fDepth;

    return Out;
}


//---------------------------------------------------------------------------------------------------------
// Declarations for depth-as-color shader path 
//---------------------------------------------------------------------------------------------------------
struct VERTEX_AS_COLOR
{
    float2 vPosition        : POSITION0;
};

struct INTERPOLATORS_AS_COLOR
{
    float4 vPosition        : POSITION0;
    float2 vTexCoord        : TEXCOORD0;
};

struct PIXEL_AS_COLOR
{
    float4 vColor           : COLOR0;
};

//---------------------------------------------------------------------------------------------------------
// Name: DepthRestoreAsColorVS()
// Desc: Simple pass through. 
//---------------------------------------------------------------------------------------------------------
float2 g_TexDims                  : register(c0);
float2 g_TexOffset                : register(c1);

INTERPOLATORS_AS_COLOR DepthRestoreAsColorVS( VERTEX_AS_COLOR In )
{
    INTERPOLATORS_AS_COLOR Out;
    Out.vPosition = float4( In.vPosition, 0.0f, 1.0f );

    // Avoid the VPOS interpolation penalty, and some pixel shader ALU.
    // Instead, hoist calculation of screenspace coords to vertex shader.
    Out.vTexCoord = ( In.vPosition * float2( 0.5f, -0.5f ) + 0.5f ) * g_TexDims + g_TexOffset + 0.5f;

    return Out;
}


//---------------------------------------------------------------------------------------------------------
// Name: DepthRestoreAsColorPS()
// Desc: Restores the depth buffer by writing to oC0.  This currently compiles to 6 instructions, which
// is already maximum-rate.  If need be, we could eliminate all instructions by rendering 40-pixel-wide 
// rects.  See the FastStencilFill sample for this approach.
//
// We avoid the VPOS input here, because the resulting performance penalty *would* be a bottleneck
// (unlike in the oDepth shader above).
//---------------------------------------------------------------------------------------------------------
//sampler texDepth        : register(s0);

PIXEL_AS_COLOR DepthRestoreAsColorPS( INTERPOLATORS_AS_COLOR In )
{
    PIXEL_AS_COLOR Out;

    float2 vTexCoord = In.vTexCoord;

    bool bLeftRight = frac( In.vTexCoord.x / 80.0f ) < 0.5f; 
    vTexCoord.x += bLeftRight ? 40.0f : -40.0f;

    float4 vDepthStencilAsColor;
    asm 
    {
        tfetch2D vDepthStencilAsColor, vTexCoord, texDepth, UnnormalizedTextureCoords=true, MinFilter=point, MagFilter=point
    };

    Out.vColor = vDepthStencilAsColor;

    return Out;
}
