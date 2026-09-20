//--------------------------------------------------------------------------------------
// NuiViewer.hlsl
//
// This shader file contains the shaders for the NuiViewer sample.
//
// Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


// Vertex shader constants.
float4x4 g_matWorldViewProjMatrix       : register( c0 );       // Clip-space transform
float4x4 g_matWorldRotationMatrix       : register( c4 );       // World space transform 
float    g_fNaN                         : register( c8 );       // Float NaN value, used to discard quads in depth mesh that have indeterminate depth.
float4   g_vTwiceTanHalfFOV             : register( c9 );       // Values used to calculate Nui to world space
bool     g_bRemoveBackground            : register( b0 );      // Boolean used to determine if background is being removed in depth mesh
float4   g_vSkeletonEndColors[2]        : register( c10 );

// Pixel shader constants.
float4   g_vLightDir                    : register( c0 );       // Light direction
float4   g_vTextureStep                 : register( c1 );       // For depth texture, x = 1 / Width, y = 1 / Height

#define DEPTH_WIDTH 320
#define DEPTH_HEIGHT 240
#define DEPTH_DISCARD_DELTA_THRESHOLD 200      // (200 mm is the depth delta threshold per quad we use to determine if we discard or not)

sampler texDepth                        : register(s0);
sampler texColor                        : register(s1);

struct VS_OUTPUT
{
    float4  Pos         : POSITION;
    float2  Tex         : TEXCOORD0;
};

struct SKELETON_VS_OUTPUT
{
    float4  Pos         : POSITION;
    float3  Normal      : NORMAL;
    float4  Color       : TEXCOORD0;
};

static const float2 XYOffsets[4] =
{
   { 0, 0 },
   { 1, 0 },
   { 1, 1 },
   { 0, 1 },
};

//---------------------------------------------------------------------------------------------------------
// Name: NuiToWorld
// Desc: Convert coordinate in NUI camera-space to world-space.
//---------------------------------------------------------------------------------------------------------
float3 NuiToWorld( float3 vNuiPosition )
{
    float3 vWorldPosition;

    vWorldPosition.xy = vNuiPosition.z * g_vTwiceTanHalfFOV * ( vNuiPosition.xy - 0.5 );
    vWorldPosition.z = vNuiPosition.z;

    return vWorldPosition;
}

//---------------------------------------------------------------------------------------------------------
// Name: vs_TransformDepthMesh
// Desc: Vertex Shader to transform quads to world space, using the depth map to provide a 3d visualization
// of the NUI scene. Quad coordinates are derived from the vertex index values, rather than using a pre-
// built mesh.
//---------------------------------------------------------------------------------------------------------
VS_OUTPUT vs_TransformDepthMesh( int VertexIndex : INDEX )
{
    VS_OUTPUT Out = ( VS_OUTPUT ) 0;

    // Calculate which quad we are, and which vertex within the quad.
    int QuadIndex = VertexIndex / 4;
    int VertexIndexInQuad = VertexIndex - ( QuadIndex * 4 );

    // Get the top-left coordinate of this quad.
    int2 XY;
    XY.y = QuadIndex / ( DEPTH_WIDTH );
    XY.x = QuadIndex - XY.y * DEPTH_WIDTH;

    int4 depthValues;
    float4 SegValues;
    
    // Sample all depth and segmentation values for the quad.
    for( int i = 0; i < 4; i++ )
    {
        int2 TempXY = XY + XYOffsets[ i ];
        float4 TexCoord = float4( ( float )TempXY.x / ( float )DEPTH_WIDTH, ( float )TempXY.y  / ( float )DEPTH_HEIGHT, 0, 0 );
        depthValues[ i ] = tex2Dlod( texDepth, TexCoord );
        SegValues[ i ] = frac( ( float )depthValues[ i ] / 8.0f );
    }

    // Average depth. We use this average to check for large deltas in depth (stretching artifacts), and discard if we find any.
    float AvgDepth = dot( (float4)depthValues, 1.0f ) / 4.0f;

    if( any( depthValues == 0 ) || 
        any( abs( depthValues - AvgDepth ) > DEPTH_DISCARD_DELTA_THRESHOLD ) ||
        ( any( SegValues == 0 ) && g_bRemoveBackground ) )
    {
        // Output a NaN, causing the quad to be discarded.
        Out.Pos.z = g_fNaN;
    }
    else
    {
        // Generate the vertex coordinates for this vertex.

        // Start with the depth at this vertex.
        int VertexDepth = depthValues[ VertexIndexInQuad ];

        // Calculate the X and Y positions of the vertex in NUI-space (basically, the U and V coordinates.
        int2 NuiXY = XY + XYOffsets[ VertexIndexInQuad ];
        float2 TexCoord = float2( ( float )NuiXY.x / ( float )DEPTH_WIDTH, ( float )NuiXY.y / ( float ) DEPTH_HEIGHT );

        // Calculate the inferred world position from the NUI coordinates, in meters (divide by 1000 gets it in meters).
        float3 inferred_pos = NuiToWorld( float3( TexCoord, ( float )VertexDepth ) ) / 1000;

        // Transform to clip-space
        Out.Pos = mul( float4( inferred_pos, 1 ), g_matWorldViewProjMatrix );
        Out.Tex = TexCoord;
    }

    return Out;
}

float4 ps_Textured( float2 Tex : TEXCOORD0 ) : COLOR
{
    return tex2D( texColor, Tex );
}

static const float4 lit_color = {0.75f, 0.75f, 0.75f, 1};

float4 ps_Lit( VS_OUTPUT Input ) : COLOR
{
    float3 depths;
    float2 Tex = Input.Tex;
    asm
    {
        tfetch2D depths.x___, Tex, texDepth, OffsetX = 0.0, OffsetY = 0.0, MinFilter=linear, MagFilter=linear
        tfetch2D depths._x__, Tex, texDepth, OffsetX = 1.0, OffsetY = 0.0, MinFilter=linear, MagFilter=linear
        tfetch2D depths.__x_, Tex, texDepth, OffsetX = 0.0, OffsetY = 1.0, MinFilter=linear, MagFilter=linear
    };

    // Recover the 3 worldspace sample coordinates
    float3 vWorld00 = NuiToWorld( float3( Input.Tex, depths.x ) );
    float3 vWorld10 = NuiToWorld( float3( Input.Tex + float2( g_vTextureStep.x, 0 ), depths.y ) );
    float3 vWorld01 = NuiToWorld( float3( Input.Tex + float2( 0, g_vTextureStep.y ), depths.z ) );

    // From the change in depth in the x and the y direction, compute the viewspace normal vector
    float3 vTangent = vWorld10 - vWorld00;
    float3 vBinormal = vWorld01 - vWorld00;
    float3 vNormal = normalize( cross( vTangent, vBinormal ) );

    float intensity = saturate( dot( vNormal, -g_vLightDir ) );

    // Assemble the final color
    return intensity * lit_color;
}


SKELETON_VS_OUTPUT vs_SkeletonBone( float3 Position : POSITION, float3 Normal : NORMAL )
{
    SKELETON_VS_OUTPUT Out = ( SKELETON_VS_OUTPUT )0;

    Out.Pos = mul( float4( Position, 1 ), g_matWorldViewProjMatrix );
    Out.Normal = normalize( mul( Normal, ( float3x3 )g_matWorldRotationMatrix ) );

    if( Position.z == 0 )
        Out.Color = g_vSkeletonEndColors[ 0 ];
    else
        Out.Color = g_vSkeletonEndColors[ 1 ];

    return Out;
}

float4 ps_SkeletonBone( float3 Normal : NORMAL, float4 Color : TEXCOORD0 ) : COLOR
{
    return saturate( dot( Normal, -g_vLightDir ) * Color );
}
