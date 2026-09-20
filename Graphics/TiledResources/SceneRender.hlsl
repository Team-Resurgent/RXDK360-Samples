//--------------------------------------------------------------------------------------
// SceneRender.hlsl
//
// Defines the vertex and pixel shader used to draw scene objects in the TiledResources
// sample.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "TiledResourceEmulationLib.hlsl"
#include "TiledResources.hlsl"

//--------------------------------------------------------------------------------------
// vertex shader constants

float4x4    world_view_proj_matrix                  : register(c0);
float4      quality_sample_scaling_vs               : register(c4);
float4      quad_layout                             : register(c5);
float4      quad_uv_transform                       : register(c6);

//--------------------------------------------------------------------------------------
// vertex shader samplers

sampler2D HeightMapLODTexture : register(s0);

//--------------------------------------------------------------------------------------
// pixel shader constants

float4      quality_sample_scaling                  : register(c0);
float4      quality_sample_scaling_2                : register(c1);

float4      light_direction_world                   : register(c4);
float4      ambient_light                           : register(c5);

float4      solid_color                             : register(c0);

//--------------------------------------------------------------------------------------
// pixel shader samplers

sampler2D DiffuseLODTexture : register(s0);
sampler3D DiffuseLODTexture3D : register(s0);

sampler2D DiffuseLODTexture_2 : register(s1);
sampler3D DiffuseLODTexture3D_2 : register(s1);

sampler2D DiffuseMapLODTexture : register(s0);
sampler2D NormalMapLODTexture : register(s1);

struct VS_IN
{
    float3  Position        : POSITION0;
    float3  Tex0            : TEXCOORD0;
};

struct VS_OUT
{
    float4 Position         : POSITION;
    float3 Tex0             : TEXCOORD0;
};

struct PS_IN
{
    float3 Tex0             : TEXCOORD0;
};

//--------------------------------------------------------------------------------------
// This vertex shader transforms position and passes through texture coordinates.
//--------------------------------------------------------------------------------------

VS_OUT VSTransform( VS_IN In )
{
    VS_OUT Out;
    Out.Position = mul( float4( In.Position, 1 ), world_view_proj_matrix );
    Out.Tex0 = In.Tex0;
    return Out;
}

//--------------------------------------------------------------------------------------
// This shader outputs the pixel's color by sampling from a single tiled texture 2D.
//--------------------------------------------------------------------------------------

float4 PSSceneRender( PS_IN In ) : COLOR0
{
    float EncodedDiffuseLOD = tex2D( DiffuseLODTexture, In.Tex0.xy * quality_sample_scaling.xy ).r;
    float DiffuseLOD = EncodedDiffuseLOD / g_LODEncode;
    float4 DiffuseTextureSample = TiledTex2D_Trilinear_MinLOD( 0, In.Tex0, DiffuseLOD );
    DiffuseTextureSample.w = 1;

    //DiffuseTextureSample = float4( EncodedDiffuseLOD, EncodedDiffuseLOD, EncodedDiffuseLOD, 1 );

    if( GetResidencyStatus() == false )
    {
        DiffuseTextureSample = float4( 1, 0, 1, 1 );
    }
    
    //return g_DebugColor;
    return DiffuseTextureSample;
}

//--------------------------------------------------------------------------------------
// This shader outputs the pixel's color by sampling from a single tiled texture 2D array.
//--------------------------------------------------------------------------------------

float4 PSSceneRender3D( PS_IN In ) : COLOR0
{
    float EncodedDiffuseLOD = tex3D( DiffuseLODTexture3D, float3( In.Tex0.xy * quality_sample_scaling.xy, In.Tex0.z ) ).r;
    float DiffuseLOD = EncodedDiffuseLOD / g_LODEncode;
    float4 DiffuseTextureSample = TiledTex3D_Trilinear_MinLOD( 0, In.Tex0, DiffuseLOD );
    DiffuseTextureSample.w = 1;

    //DiffuseTextureSample = float4( EncodedDiffuseLOD, EncodedDiffuseLOD, EncodedDiffuseLOD, 1 );

    if( GetResidencyStatus() == false )
    {
        DiffuseTextureSample = float4( 1, 0, 1, 1 );
    }
    
    return DiffuseTextureSample;
}

//--------------------------------------------------------------------------------------

float4 PSSceneRenderDoubleTex2D( PS_IN In ) : COLOR0
{
    float4 DiffuseTextureSample_1;
    float4 DiffuseTextureSample_2;

    {
        float EncodedDiffuseLOD = tex2D( DiffuseLODTexture, In.Tex0.xy * quality_sample_scaling.xy ).r;
        float DiffuseLOD = EncodedDiffuseLOD / g_LODEncode;
        DiffuseTextureSample_1 = TiledTex2D_Trilinear_MinLOD( 0, In.Tex0, DiffuseLOD );
        DiffuseTextureSample_1.w = 1;

        //DiffuseTextureSample_1 = float4( EncodedDiffuseLOD, EncodedDiffuseLOD, EncodedDiffuseLOD, 1 );

        if( GetResidencyStatus() == false )
        {
            DiffuseTextureSample_1 = float4( 1, 0, 1, 1 );
        }
    }
    
    {
        float EncodedDiffuseLOD = tex2D( DiffuseLODTexture_2, In.Tex0.xy * quality_sample_scaling_2.xy ).r;
        float DiffuseLOD = EncodedDiffuseLOD / g_LODEncode;
        DiffuseTextureSample_2 = TiledTex2D_Trilinear_MinLOD( 1, In.Tex0, DiffuseLOD );
        DiffuseTextureSample_2.w = 1;

        //DiffuseTextureSample_2 = float4( EncodedDiffuseLOD, EncodedDiffuseLOD, EncodedDiffuseLOD, 1 );

        if( GetResidencyStatus() == false )
        {
            DiffuseTextureSample_2 = float4( 1, 0, 1, 1 );
        }
    }
    
    return lerp( DiffuseTextureSample_1, DiffuseTextureSample_2, 0.5 );
}

//--------------------------------------------------------------------------------------
// This shader outputs the pixel's color by sampling from a single tiled texture 2D
// array, accessed as a texture quilt.
//--------------------------------------------------------------------------------------
float4 PSSceneRenderQuilt( PS_IN In ) : COLOR0
{
    // need to compute the texture LOD before we convert 2D quilt texcoords into 3D array slice texcoords
    // if we don't do this, the texture LOD will be improperly computed along quilt boundaries, since the
    // gradients along the boundary will be invalid due to the 2D to 3D conversion
    float ComputedLOD = TiledTex3D_ComputeLOD( 0, In.Tex0 );
    
    // convert 2D quilt texcoords into 3D array slice texcoords
    float3 ArrayUVW = Quilt2DToTex3D( 0, In.Tex0.xy );

    // determine our lowest loaded LOD from the sampling quality texture
    float EncodedDiffuseLOD = tex3D( DiffuseLODTexture3D, float3( ArrayUVW.xy * quality_sample_scaling.xy, ArrayUVW.z ) ).r;
    float DiffuseLOD = EncodedDiffuseLOD / g_LODEncode;
    
    // compute our fixed LOD for sampling, from the computed LOD and the lowest loaded LOD
    float MaxLOD = max( DiffuseLOD, ComputedLOD );
    
    // trilinear sample from the tiled texture array, using our computed LOD value
    float4 DiffuseTextureSample = TiledTex3D_Trilinear_FixedLOD( 0, ArrayUVW, MaxLOD );
    DiffuseTextureSample.w = 1;

    //DiffuseTextureSample = float4( EncodedDiffuseLOD, EncodedDiffuseLOD, EncodedDiffuseLOD, 1 );

    // display residency feedback
    if( GetResidencyStatus() == false )
    {
        DiffuseTextureSample = float4( 1, 0, 1, 1 );
    }
    
    return DiffuseTextureSample;
}


//--------------------------------------------------------------------------------------
// This vertex shader samples from a tiled texture 2D heightmap, offsets the vertex Y
// coordinate by the height value, transforms the vertex, and then passes through texture
// coordinates.
//--------------------------------------------------------------------------------------
VS_OUT VSTerrain( int QuadIndex : INDEX, float2 BaryUV : BARYCENTRIC, int QuadID : QUADID, out float4 Color : COLOR0 )
{
    // Re-order the parametric coordinates based on the quad id.
    float2 VertexUV = BaryUV * ( QuadID == 0 );
    VertexUV += float2( 1.0 - BaryUV.x,       BaryUV.y ) * ( QuadID == 1 ); 
    VertexUV += float2( 1.0 - BaryUV.x, 1.0 - BaryUV.y ) * ( QuadID == 2 );
    VertexUV += float2(       BaryUV.x, 1.0 - BaryUV.y ) * ( QuadID == 3 );
    
    int QuadY = QuadIndex / quad_layout.x;
    int QuadX = QuadIndex % quad_layout.x;
    
    float2 UpperLeftPositionXZ = float2( QuadX, QuadY ) * quad_layout.zw;
    float2 LowerRightPositionXZ = UpperLeftPositionXZ + quad_layout.zw;
    float2 PositionXZ = lerp( UpperLeftPositionXZ, LowerRightPositionXZ, VertexUV );
    
    float2 Tex = PositionXZ * quad_uv_transform.xy + quad_uv_transform.zw;
    // Perform max reduction filtering on the sampling quality map.
    // Reduction filtering is where we gather multiple samples like a bilinear sample, but
    // instead of performing a bilinear blend, max reduction filtering returns the max value
    // of the samples.
    // We want to use max reduction filtering so that we always get the maximum value of the
    // sampling quality map at a given sample, not a blended result.  With blending, we might
    // get intermediate values that are not valid.  For example, blending between values of 1.0 and
    // 5.0 would return 3.0, but there may not be a mip LOD 3 available at the sampled location in
    // the tiled texture.
    
    float2 HeightMapUV = Tex.xy * quality_sample_scaling_vs.xy;
    float4 EncodedSamples = 0;
    asm
    {
        setTexLOD EncodedSamples.x
        tfetch2D EncodedSamples.x___, HeightMapUV, HeightMapLODTexture, OffsetX = 0.5, OffsetY = 0.5, UseRegisterLOD=true, UseComputedLOD=false
        tfetch2D EncodedSamples._x__, HeightMapUV, HeightMapLODTexture, OffsetX = -0.5, OffsetY = 0.5, UseRegisterLOD=true, UseComputedLOD=false
        tfetch2D EncodedSamples.__x_, HeightMapUV, HeightMapLODTexture, OffsetX = 0.5, OffsetY = -0.5, UseRegisterLOD=true, UseComputedLOD=false
        tfetch2D EncodedSamples.___x, HeightMapUV, HeightMapLODTexture, OffsetX = -0.5, OffsetY = -0.5, UseRegisterLOD=true, UseComputedLOD=false
        max4 EncodedSamples, EncodedSamples
    };

    // Sample the height map using the max filtered LOD value from the sampling quality map:
    float HeightMapLOD = EncodedSamples.x / g_LODEncode;
    float4 HeightMapTextureSample = TiledTex2D_Trilinear_FixedLOD( 0, Tex.xy, HeightMapLOD );
    
    // Construct the 3D position from the heightmap value and the terrain absolute XZ values:
    float3 Position = float3( PositionXZ.x, 0, PositionXZ.y );
    Position.y = HeightMapTextureSample.x * quad_layout.y;
    
    // Transform the position:
    VS_OUT Out;
    Out.Position = mul( float4( Position, 1 ), world_view_proj_matrix );

    // Output the terrain absolute texture coordinate:
    Out.Tex0 = float3( Tex, 0 );
    
    Color = float4( HeightMapTextureSample.xxx, 1 );
    
    return Out;
}

//--------------------------------------------------------------------------------------
// This pixel shader samples from a pair of tiled textures to get the diffuse color and
// surface normal for the pixel.  A simple diffuse lighting computation is done, and the
// pixel color is returned.
//--------------------------------------------------------------------------------------

float4 PSTerrain( PS_IN In, [unused] in float4 Color : COLOR0 ) : COLOR0
{
    float EncodedNormalMapLOD = tex2D( NormalMapLODTexture, In.Tex0.xy * quality_sample_scaling_2.xy ).r;
    float NormalMapLOD = EncodedNormalMapLOD / g_LODEncode;
    float4 NormalMapTextureSample = TiledTex2D_Trilinear_MinLOD( 1, In.Tex0, NormalMapLOD );
    if( GetResidencyStatus() == false )
    {
        NormalMapTextureSample = float4( 0, 0, 1, 1 );
    }
    else
    {
        NormalMapTextureSample.xy = NormalMapTextureSample.xy * 2 - 1;
        NormalMapTextureSample.z = 0.5;
        NormalMapTextureSample.xyz = normalize( NormalMapTextureSample.xyz );
    }
    
    float EncodedDiffuseMapLOD = tex2D( DiffuseMapLODTexture, In.Tex0.xy * quality_sample_scaling.xy ).r;
    float DiffuseMapLOD = EncodedDiffuseMapLOD / g_LODEncode;
    float4 DiffuseMapTextureSample = TiledTex2D_Trilinear_MinLOD( 0, In.Tex0, DiffuseMapLOD );
    
    float DiffuseLight = saturate( dot( NormalMapTextureSample.xyz, -light_direction_world ) );
    
    return ( DiffuseLight.xxxx + ambient_light ) * DiffuseMapTextureSample;
}

//--------------------------------------------------------------------------------------

float4 PSColor( [unused] PS_IN In ) : COLOR0
{
    return solid_color;
}
