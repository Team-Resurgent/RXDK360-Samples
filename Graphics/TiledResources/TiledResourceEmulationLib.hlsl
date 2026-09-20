//--------------------------------------------------------------------------------------
// TiledResourceEmulationLib.hlsl
//
// Defines HLSL functions that emulate tiled texture fetches.
// This file contains a lot of internal utility functions that represent the bulk
// of the code that emulates tiled texture sampling.
// Public functions include the TiledTex2D_ and TiledTex3D_ family of functions as 
// well as GetResidencyStatus(), and they can be found below the halfway point in this file.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef TILEDLIB_HLSL
#define TILEDLIB_HLSL

// Optimization switches
// These defines enable functionality that trades ALU performance for compatibility.

// Support for non-power-of-2 dimension textures.  There is a correction factor applied to the raw texture UV
// that supports UV spaces that do not span the entire width and/or height of a particular mip level.
#define SUPPORT_NON_POW2_DIMENSIONS 0

//--------------------------------------------------------------------------------------

// Texture sampler slots for tiled textures.
// Each slot represents one tiled resource.  
// Each slot consists of 2 textures: an index map (2D or 3D) and a physical tile map.
// Each slot also consists of several shader constants, seen below.

// Note that we allow 1 slot for vertex shaders, and 4 for pixel shaders.
// This is due largely to the needs of the sample (no multitexturing needed in the vertex
// shader), as well as shader constant pressure.

// If you change the number of slots allowed, you will have to change the register
// allocations as well.

#ifdef TILED_VERTEX_SHADER

// Number of tiled resource slots allowed in the vertex shader:
#define TILED_RESOURCE_SLOTS 1

// Index map texture:
sampler2D   g_IndexMapTexture2D[TILED_RESOURCE_SLOTS]      : register(s2);

// Index map array texture:
sampler3D   g_IndexMapTexture3D[TILED_RESOURCE_SLOTS]      : register(s2);

// Physical tile array texture:
sampler3D   g_PhysicalTileTexture[TILED_RESOURCE_SLOTS]    : register(s3);

#else

// Number of tiled resource slots allowed in the pixel shader:
#define TILED_RESOURCE_SLOTS 4

// Index map texture:
sampler2D   g_IndexMapTexture2D[TILED_RESOURCE_SLOTS]      : register(s7);

// Index map array texture:
sampler3D   g_IndexMapTexture3D[TILED_RESOURCE_SLOTS]      : register(s7);

// Physical tile array texture:
sampler3D   g_PhysicalTileTexture[TILED_RESOURCE_SLOTS]    : register(s11);

#endif

//--------------------------------------------------------------------------------------

// Shader constants for the tiled resource slots.
// There are 12 shader constants for each tiled resource slot.
// The register allocations start at constant 247, and work backwards.  This leaves
// 8 constants (248-255) for shader literals.
// Again, if you change the number of tiled resource slots, you will have to update
// the register allocations.

// Tile UV size LOD constants - one shader constant per index map LOD
// ZW components are 1,1 for power-of-2 dimension textures
// X component converts from resource 0-1 U space to one tile width 0-1 U space
// Y component converts from resource 0-1 V space to one tile height 0-1 V space
// Z component adjusts resource 0-1 U space to index map 0-1 U space
// W component adjusts resource 0-1 V space to index map 0-1 V space
float4 g_PhysicalTileUVSizePerLOD[TILED_RESOURCE_SLOTS][9] : register(c212);

// Resource constant - one per resource (index map)
// X: mip LOD bias
// Y: 1 / array size
// Z: quilt row width in slices
// W: quilt row count in slices
float4 g_ResourceMiscConstant[TILED_RESOURCE_SLOTS] : register(c208);

// Tile border UV transform - one per resource (tile pool array texture)
float4 g_TileBorderUVTransform[TILED_RESOURCE_SLOTS] : register(c204);

// array texture constants
// X: 1 / number of array slices
// Y: unused
// Z: 1 / atlas width
// W: 1 / atlas height
float4 g_ArrayTexConstants[TILED_RESOURCE_SLOTS]     : register(c200);

//--------------------------------------------------------------------------------------

// g_ResidencyStatus is updated by the tiled texture fetch subroutines when a fetch hits
// a nonresident tile.  Its value can be retrieved by calling the GetResidencyStatus()
// function.

bool g_ResidencyStatus;

//--------------------------------------------------------------------------------------

// add this to an integer slice index before dividing by the number of slices
// i.e. with 10 slices, you have indices ranging from 0 to 9
// add 0.5 to the slice index to get 0.5 to 9.5 for the slice index
// then divide the slice index by 10 to get a 0..1 Z texture coordinate for sampling
static const float g_ArraySliceCenterOffset = 0.5f;

//--------------------------------------------------------------------------------------
// Name: Get2DLOD (Texture2D version)
// Desc: A method that returns the tiled resource mip LOD of the given tiled texture at the
//       given UV coordinates.  Note that this computes the mip LOD against the index map
//       and then applies a bias at the end to convert that mip LOD to the LOD against
//       the tiled texture.
//--------------------------------------------------------------------------------------
float Get2DLOD( sampler2D s, const float2 UV, int IndexMapSlot )
{
    float LOD;
    asm
    {
        getCompTexLOD2D LOD.x, UV, s
    };
    return LOD.x + g_ResourceMiscConstant[IndexMapSlot].x;
}

//--------------------------------------------------------------------------------------
// Name: Get2DLOD (Texture2DArray version)
// Desc: A method that returns the tiled resource mip LOD of the given tiled texture at the
//       given UVW coordinates.  Note that this computes the mip LOD against the index map
//       and then applies a bias at the end to convert that mip LOD to the LOD against
//       the tiled texture.
//--------------------------------------------------------------------------------------

float Get2DLOD( sampler3D s, const float3 UVW, int IndexMapSlot )
{
    float LOD;
    asm
    {
        getCompTexLOD3D LOD.x, UVW, s, VolMagFilter=linear, VolMinFilter=linear
    };
    return LOD.x + g_ResourceMiscConstant[IndexMapSlot].x;
}

//--------------------------------------------------------------------------------------
// Name: ComputeTileUV
// Desc: Returns a tile-relative UV coordinate based on the texture-relative UV coordinate
//       and an inverse tile size constant.
//--------------------------------------------------------------------------------------
float2 ComputeTileUV( const float2 TextureUV, const float2 InverseTileSize )
{
    return frac( TextureUV * InverseTileSize );
}

//--------------------------------------------------------------------------------------
// Name: ComputePhysicalTileUV_MinLOD
// Desc: Given a UV coord and index map slot, returns a physical tile array texture 
//       location that should be sampled to produce the final texture sample.
//       A MinLOD value is provided that is used to clamp the computed LOD value.
//--------------------------------------------------------------------------------------
float3 ComputePhysicalTileUV_MinLOD( const float2 TextureUV, const float MinLOD, int IndexMapSlot )
{
    // Compute the mip LOD of the sample against the texture:
    float LOD = Get2DLOD( g_IndexMapTexture2D[IndexMapSlot], TextureUV, IndexMapSlot );

    // Clamp and round the LOD value to a whole number lookup index:
    LOD = clamp( LOD, MinLOD, 8 );
    float IndexMapLOD = round( LOD );
    float ConstantLookupLOD = IndexMapLOD;
    
    // determine physical tile size and resource UV scaling for the given LOD:
    float4 InvTileSize_UVScale = g_PhysicalTileUVSizePerLOD[IndexMapSlot][ConstantLookupLOD];
    
    // adjust incoming UV coordinates to map to the proper scale for the index map lookup:
#ifdef SUPPORT_NON_POW2_DIMENSIONS
    float2 IndexMapUV = TextureUV.xy * InvTileSize_UVScale.zw;
#else
    float2 IndexMapUV = TextureUV.xy;
#endif
    
    // determine physical tile location:
    // X component is the atlas X index
    // Y component is the atlas Y index
    // Z component is the normalized array slice index
    // W component is 255 if the entry is valid, 0 otherwise
    float4 PhysicalTileLocation;
    asm
    {
        setTexLOD IndexMapLOD.x;
        tfetch2D PhysicalTileLocation, IndexMapUV, g_IndexMapTexture2D[IndexMapSlot], UseComputedLOD = false, UseRegisterLOD = true;
    };
    
    // convert resource UV space to tile UV space:
    float2 TileUV = ComputeTileUV( TextureUV, InvTileSize_UVScale.xy );    

    if( PhysicalTileLocation.x >= 31 )
    {
        // residency failure; return the tile UV and -1 for the array slice index
        g_ResidencyStatus = false;
        return float3( TileUV, -1 );
    }
    else
    {
        g_ResidencyStatus = true;
    }
    
    // offset the tile UV by the border offset:
    TileUV = ( TileUV * g_TileBorderUVTransform[IndexMapSlot].xy ) + g_TileBorderUVTransform[IndexMapSlot].zw;
    
    // compute the atlas UV by offsetting the tile UV by the atlas X and Y coordinates
    float2 AtlasUV = ( TileUV + PhysicalTileLocation.xz ) * g_ArrayTexConstants[IndexMapSlot].zw;
    
    // return the 3D texture coordinate used to look up into the physical tile array texture:
    return float3( AtlasUV, ( PhysicalTileLocation.y + g_ArraySliceCenterOffset ) * g_ArrayTexConstants[IndexMapSlot].x );
}

//--------------------------------------------------------------------------------------
// Name: ComputePhysicalTileUV
// Desc: Given a UV coord and index map slot, returns a physical tile array texture 
//       location that should be sampled to produce the final texture sample.
//--------------------------------------------------------------------------------------

float3 ComputePhysicalTileUV( const float2 TextureUV, int IndexMapSlot )
{
    return ComputePhysicalTileUV_MinLOD( TextureUV, 0, IndexMapSlot );
}

//--------------------------------------------------------------------------------------
// Name: ComputePhysicalTileUV_FixedLOD
// Desc: Given a UV coord and index map slot, returns a physical tile array texture 
//       location that should be sampled to produce the final texture sample.
//       A fixed LOD value is provided instead of using a computed LOD value.
//--------------------------------------------------------------------------------------

float3 ComputePhysicalTileUV_FixedLOD( const float2 TextureUV, const float FixedLOD, int IndexMapSlot )
{

    // Clamp and round the LOD value to a whole number lookup index:
    float LOD = clamp( FixedLOD, 0, 8 );
    float IndexMapLOD = round( LOD );
    float ConstantLookupLOD = IndexMapLOD;
    
    // determine physical tile size and resource UV scaling for the given LOD:
    float4 InvTileSize_UVScale = g_PhysicalTileUVSizePerLOD[IndexMapSlot][ConstantLookupLOD];
    
    // adjust incoming UV coordinates to map to the proper scale for the index map lookup:
#ifdef SUPPORT_NON_POW2_DIMENSIONS
    float2 IndexMapUV = TextureUV.xy * InvTileSize_UVScale.zw;
#else
    float2 IndexMapUV = TextureUV.xy;
#endif
    
    // determine physical tile location:
    // X component is the atlas X index
    // Y component is the atlas Y index
    // Z component is the normalized array slice index
    // W component is 255 if the entry is valid, 0 otherwise
    float4 PhysicalTileLocation;
    asm
    {
        setTexLOD IndexMapLOD.x;
        tfetch2D PhysicalTileLocation, IndexMapUV, g_IndexMapTexture2D[IndexMapSlot], UseComputedLOD = false, UseRegisterLOD = true;
    };
    
    // convert resource UV space to tile UV space:
    float2 TileUV = ComputeTileUV( TextureUV, InvTileSize_UVScale.xy );

    if( PhysicalTileLocation.x >= 31 )
    {
        // residency failure; return the tile UV and -1 for the array slice index
        g_ResidencyStatus = false;
        return float3( TileUV, -1 );
    }
    else
    {
        g_ResidencyStatus = true;
    }
    
    // offset the tile UV by the border offset:
    TileUV = ( TileUV * g_TileBorderUVTransform[IndexMapSlot].xy ) + g_TileBorderUVTransform[IndexMapSlot].zw;
    
    // compute the atlas UV by offsetting the tile UV by the atlas X and Y coordinates
    float2 AtlasUV = ( TileUV + PhysicalTileLocation.xz ) * g_ArrayTexConstants[IndexMapSlot].zw;
    
    // return the 3D texture coordinate used to look up into the physical tile array texture:
    return float3( AtlasUV, ( PhysicalTileLocation.y + g_ArraySliceCenterOffset ) * g_ArrayTexConstants[IndexMapSlot].x );
}

//--------------------------------------------------------------------------------------
// Name: ComputeTrilinearPhysicalTileUV_MinLOD
// Desc: Given a UV coord and index map slot, returns a pair of physical tile array texture 
//       locations that should be sampled and blended to produce the final texture sample.
//       The return value is the 0..1 lerp value for the trilinear blend.
//       A MinLOD value is provided that is used to clamp the computed LOD value.
//--------------------------------------------------------------------------------------

float ComputeTrilinearPhysicalTileUV_MinLOD( const float2 TextureUV, const float MinLOD, int IndexMapSlot, out float3 PhysicalTileA, out float3 PhysicalTileB )
{
    // Compute the mip LOD of the sample against the texture:
    float LOD = Get2DLOD( g_IndexMapTexture2D[IndexMapSlot], TextureUV, IndexMapSlot );

    // Clamp and round the LOD value to two whole number lookup indices:
    LOD = clamp( LOD, MinLOD, 8 );
    float TrilinearLerp = frac( LOD );
    float IndexMapLODA = floor( LOD );
    float IndexMapLODB = ceil( LOD );
    float ConstantLookupLODA = IndexMapLODA;
    float ConstantLookupLODB = IndexMapLODB;
    
    // determine physical tile size and resource UV scaling for the given LOD:
    float4 InvTileSize_UVScaleA = g_PhysicalTileUVSizePerLOD[IndexMapSlot][ConstantLookupLODA];
    float4 InvTileSize_UVScaleB = g_PhysicalTileUVSizePerLOD[IndexMapSlot][ConstantLookupLODB];
    
    // adjust incoming UV coordinates to map to the proper scale for the index map lookup:
#ifdef SUPPORT_NON_POW2_DIMENSIONS
    float2 IndexMapUVA = TextureUV.xy * InvTileSize_UVScaleA.zw;
    float2 IndexMapUVB = TextureUV.xy * InvTileSize_UVScaleB.zw;
#else
    float2 IndexMapUVA = TextureUV.xy;
    float2 IndexMapUVB = TextureUV.xy;
#endif
    
    // determine physical tile location:
    // X component is the atlas X index
    // Y component is the atlas Y index
    // Z component is the normalized array slice index
    // W component is 255 if the entry is valid, 0 otherwise
    float4 PhysicalTileLocationA;
    float4 PhysicalTileLocationB;
    asm
    {
        setTexLOD IndexMapLODA.x;
        tfetch2D PhysicalTileLocationA, IndexMapUVA, g_IndexMapTexture2D[IndexMapSlot], UseComputedLOD = false, UseRegisterLOD = true;
        setTexLOD IndexMapLODB.x;
        tfetch2D PhysicalTileLocationB, IndexMapUVB, g_IndexMapTexture2D[IndexMapSlot], UseComputedLOD = false, UseRegisterLOD = true;
    };
    
    // convert resource UV space to tile UV space:
    float2 TileUVA = ComputeTileUV( TextureUV, InvTileSize_UVScaleA.xy );
    float2 TileUVB = ComputeTileUV( TextureUV, InvTileSize_UVScaleB.xy );
    
    bool ResidencyStatus = true;
    
    if( PhysicalTileLocationA.x >= 31 )
    {
        // residency failure; return the tile UV and -1 for the array slice index
        PhysicalTileA = float3( TileUVA, -1 );
        ResidencyStatus = false;
    }
    else
    {
        // offset the tile UV by the border offset:
        TileUVA = ( TileUVA * g_TileBorderUVTransform[IndexMapSlot].xy ) + g_TileBorderUVTransform[IndexMapSlot].zw;
        
        // compute the atlas UV by offsetting the tile UV by the atlas X and Y coordinates
        float2 AtlasUV = ( TileUVA + PhysicalTileLocationA.xz ) * g_ArrayTexConstants[IndexMapSlot].zw;
        
        // return the 3D texture coordinate used to look up into the physical tile array texture:
        PhysicalTileA = float3( AtlasUV, ( PhysicalTileLocationA.y + g_ArraySliceCenterOffset ) * g_ArrayTexConstants[IndexMapSlot].x );
    }
    
    if( PhysicalTileLocationB.x >= 31 )
    {
        // residency failure; return the tile UV and -1 for the array slice index
        PhysicalTileB = float3( TileUVB, -1 );
        
        ResidencyStatus = false;
    }
    else
    {
        // offset the tile UV by the border offset:
        TileUVB = ( TileUVB * g_TileBorderUVTransform[IndexMapSlot].xy ) + g_TileBorderUVTransform[IndexMapSlot].zw;
        
        // compute the atlas UV by offsetting the tile UV by the atlas X and Y coordinates
        float2 AtlasUV = ( TileUVB + PhysicalTileLocationB.xz ) * g_ArrayTexConstants[IndexMapSlot].zw;
        
        // return the 3D texture coordinate used to look up into the physical tile array texture:
        PhysicalTileB = float3( AtlasUV, ( PhysicalTileLocationB.y + g_ArraySliceCenterOffset ) * g_ArrayTexConstants[IndexMapSlot].x );
    }
    
    g_ResidencyStatus = ResidencyStatus;
    
    return TrilinearLerp;
}

//--------------------------------------------------------------------------------------
// Name: ComputeTrilinearPhysicalTileUV
// Desc: Given a UV coord and index map slot, returns a pair of physical tile array texture 
//       locations that should be sampled and blended to produce the final texture sample.
//       The return value is the 0..1 lerp value for the trilinear blend.
//--------------------------------------------------------------------------------------

float ComputeTrilinearPhysicalTileUV( const float2 TextureUV, int IndexMapSlot, out float3 PhysicalTileA, out float3 PhysicalTileB )
{
    return ComputeTrilinearPhysicalTileUV_MinLOD( TextureUV, 0, IndexMapSlot, PhysicalTileA, PhysicalTileB );
}

//--------------------------------------------------------------------------------------
// Name: ComputeTrilinearPhysicalTileUV_FixedLOD
// Desc: Given a UV coord and index map slot, returns a pair of physical tile array texture 
//       locations that should be sampled and blended to produce the final texture sample.
//       The return value is the 0..1 lerp value for the trilinear blend.
//       A fixed LOD value is provided that is used instead of a computed LOD value.
//--------------------------------------------------------------------------------------

float ComputeTrilinearPhysicalTileUV_FixedLOD( const float2 TextureUV, const float FixedLOD, int IndexMapSlot, out float3 PhysicalTileA, out float3 PhysicalTileB )
{

    // Clamp and round the LOD value to two whole number lookup indices:
    float LOD = clamp( FixedLOD, 0, 8 );
    float TrilinearLerp = frac( LOD );
    float IndexMapLODA = floor( LOD );
    float IndexMapLODB = ceil( LOD );
    float ConstantLookupLODA = IndexMapLODA;
    float ConstantLookupLODB = IndexMapLODB;
    
    // determine physical tile size and resource UV scaling for the given LOD:
    float4 InvTileSize_UVScaleA = g_PhysicalTileUVSizePerLOD[IndexMapSlot][ConstantLookupLODA];
    float4 InvTileSize_UVScaleB = g_PhysicalTileUVSizePerLOD[IndexMapSlot][ConstantLookupLODB];
    
    // adjust incoming UV coordinates to map to the proper scale for the index map lookup:
#ifdef SUPPORT_NON_POW2_DIMENSIONS
    float2 IndexMapUVA = TextureUV.xy * InvTileSize_UVScaleA.zw;
    float2 IndexMapUVB = TextureUV.xy * InvTileSize_UVScaleB.zw;
#else
    float2 IndexMapUVA = TextureUV.xy;
    float2 IndexMapUVB = TextureUV.xy;
#endif
    
    // determine physical tile location:
    // X component is the atlas X index
    // Y component is the atlas Y index
    // Z component is the normalized array slice index
    // W component is 255 if the entry is valid, 0 otherwise
    float4 PhysicalTileLocationA;
    float4 PhysicalTileLocationB;
    asm
    {
        setTexLOD IndexMapLODA.x;
        tfetch2D PhysicalTileLocationA, IndexMapUVA, g_IndexMapTexture2D[IndexMapSlot], UseComputedLOD = false, UseRegisterLOD = true;
        setTexLOD IndexMapLODB.x;
        tfetch2D PhysicalTileLocationB, IndexMapUVB, g_IndexMapTexture2D[IndexMapSlot], UseComputedLOD = false, UseRegisterLOD = true;
    };
    
    // convert resource UV space to tile UV space:
    float2 TileUVA = ComputeTileUV( TextureUV, InvTileSize_UVScaleA.xy );
    float2 TileUVB = ComputeTileUV( TextureUV, InvTileSize_UVScaleB.xy );
    
    bool ResidencyStatus = true;
    
    if( PhysicalTileLocationA.x >= 31 )
    {
        // residency failure; return the tile UV and -1 for the array slice index
        PhysicalTileA = float3( TileUVA, -1 );
        ResidencyStatus = false;
    }
    else
    {
        // offset the tile UV by the border offset:
        TileUVA = ( TileUVA * g_TileBorderUVTransform[IndexMapSlot].xy ) + g_TileBorderUVTransform[IndexMapSlot].zw;
        
        // compute the atlas UV by offsetting the tile UV by the atlas X and Y coordinates
        float2 AtlasUV = ( TileUVA + PhysicalTileLocationA.xz ) * g_ArrayTexConstants[IndexMapSlot].zw;
        
        // return the 3D texture coordinate used to look up into the physical tile array texture:
        PhysicalTileA = float3( AtlasUV, ( PhysicalTileLocationA.y + g_ArraySliceCenterOffset ) * g_ArrayTexConstants[IndexMapSlot].x );
    }
    
    if( PhysicalTileLocationB.x >= 31 )
    {
        // residency failure; return the tile UV and -1 for the array slice index
        PhysicalTileB = float3( TileUVB, -1 );
        
        ResidencyStatus = false;
    }
    else
    {
        // offset the tile UV by the border offset:
        TileUVB = ( TileUVB * g_TileBorderUVTransform[IndexMapSlot].xy ) + g_TileBorderUVTransform[IndexMapSlot].zw;
        
        // compute the atlas UV by offsetting the tile UV by the atlas X and Y coordinates
        float2 AtlasUV = ( TileUVB + PhysicalTileLocationB.xz ) * g_ArrayTexConstants[IndexMapSlot].zw;
        
        // return the 3D texture coordinate used to look up into the physical tile array texture:
        PhysicalTileB = float3( AtlasUV, ( PhysicalTileLocationB.y + g_ArraySliceCenterOffset ) * g_ArrayTexConstants[IndexMapSlot].x );
    }
    
    g_ResidencyStatus = ResidencyStatus;
    
    return TrilinearLerp;
}

//--------------------------------------------------------------------------------------
// Name: ComputePhysicalTileUVW_MinLOD
// Desc: Given a UVW coord and index map slot, returns a physical tile array texture 
//       location that should be sampled to produce the final texture sample.
//       A MinLOD value is provided that is used to clamp the computed LOD value.
//--------------------------------------------------------------------------------------

float3 ComputePhysicalTileUVW_MinLOD( const float3 TextureUVW, const float MinLOD, int IndexMapSlot )
{
    // Compute the mip LOD of the sample against the texture:
    float LOD = Get2DLOD( g_IndexMapTexture3D[IndexMapSlot], TextureUVW, IndexMapSlot );

    // Clamp and round the LOD value to a whole number lookup index:
    LOD = clamp( LOD, MinLOD, 8 );
    float IndexMapLOD = round( LOD );
    float ConstantLookupLOD = IndexMapLOD;
    
    // determine physical tile size and resource UV scaling for the given LOD:
    float4 InvTileSize_UVScale = g_PhysicalTileUVSizePerLOD[IndexMapSlot][ConstantLookupLOD];
    
    // adjust incoming UV coordinates to map to the proper scale for the index map lookup:
#ifdef SUPPORT_NON_POW2_DIMENSIONS
    float3 IndexMapUVW = float3( TextureUVW.xy * InvTileSize_UVScale.zw, TextureUVW.z );
#else
    float3 IndexMapUVW = TextureUVW.xyz;
#endif
    
    // determine physical tile location:
    // X component is the atlas X index
    // Y component is the atlas Y index
    // Z component is the normalized array slice index
    // W component is 255 if the entry is valid, 0 otherwise
    float4 PhysicalTileLocation;
    asm
    {
        setTexLOD IndexMapLOD.x;
        tfetch3D PhysicalTileLocation, IndexMapUVW, g_IndexMapTexture3D[IndexMapSlot], UseComputedLOD = false, UseRegisterLOD = true;
    };
    
    // convert resource UV space to tile UV space:
    float2 TileUV = ComputeTileUV( TextureUVW.xy, InvTileSize_UVScale.xy );

    if( PhysicalTileLocation.x >= 31 )
    {
        // residency failure; return the tile UV and -1 for the array slice index
        g_ResidencyStatus = false;
        return float3( TileUV, -1 );
    }
    else
    {
        g_ResidencyStatus = true;
    }
    
    // offset the tile UV by the border offset:
    TileUV = ( TileUV * g_TileBorderUVTransform[IndexMapSlot].xy ) + g_TileBorderUVTransform[IndexMapSlot].zw;
    
    // compute the atlas UV by offsetting the tile UV by the atlas X and Y coordinates
    float2 AtlasUV = ( TileUV + PhysicalTileLocation.xz ) * g_ArrayTexConstants[IndexMapSlot].zw;
    
    // return the 3D texture coordinate used to look up into the physical tile array texture:
    return float3( AtlasUV, ( PhysicalTileLocation.y + g_ArraySliceCenterOffset ) * g_ArrayTexConstants[IndexMapSlot].x );
}

//--------------------------------------------------------------------------------------
// Name: ComputePhysicalTileUVW
// Desc: Given a UVW coord and index map slot, returns a physical tile array texture 
//       location that should be sampled to produce the final texture sample.
//--------------------------------------------------------------------------------------

float3 ComputePhysicalTileUVW( const float3 TextureUVW, int IndexMapSlot )
{
    return ComputePhysicalTileUVW_MinLOD( TextureUVW, 0, IndexMapSlot);
}

//--------------------------------------------------------------------------------------
// Name: ComputePhysicalTileUVW_FixedLOD
// Desc: Given a UVW coord and index map slot, returns a physical tile array texture 
//       location that should be sampled to produce the final texture sample.
//       A fixed LOD value is provided that is used instead of a computed LOD value.
//--------------------------------------------------------------------------------------

float3 ComputePhysicalTileUVW_FixedLOD( const float3 TextureUVW, const float FixedLOD, int IndexMapSlot )
{

    // Clamp and round the LOD value to a whole number lookup index:
    float LOD = clamp( FixedLOD, 0, 8 );
    float IndexMapLOD = round( LOD );
    float ConstantLookupLOD = IndexMapLOD;
    
    // determine physical tile size and resource UV scaling for the given LOD:
    float4 InvTileSize_UVScale = g_PhysicalTileUVSizePerLOD[IndexMapSlot][ConstantLookupLOD];
    
    // adjust incoming UV coordinates to map to the proper scale for the index map lookup:
#ifdef SUPPORT_NON_POW2_DIMENSIONS
    float3 IndexMapUVW = float3( TextureUVW.xy * InvTileSize_UVScale.zw, TextureUVW.z );
#else
    float3 IndexMapUVW = TextureUV.xyz;
#endif
    
    // determine physical tile location:
    // X component is the atlas X index
    // Y component is the atlas Y index
    // Z component is the normalized array slice index
    // W component is 255 if the entry is valid, 0 otherwise
    float4 PhysicalTileLocation;
    asm
    {
        setTexLOD IndexMapLOD.x;
        tfetch3D PhysicalTileLocation, IndexMapUVW, g_IndexMapTexture3D[IndexMapSlot], UseComputedLOD = false, UseRegisterLOD = true;
    };
    
    // convert resource UV space to tile UV space:
    float2 TileUV = ComputeTileUV( TextureUVW.xy, InvTileSize_UVScale.xy );

    if( PhysicalTileLocation.x >= 31 )
    {
        // residency failure; return the tile UV and -1 for the array slice index
        g_ResidencyStatus = false;
        return float3( TileUV, -1 );
    }
    else
    {
        g_ResidencyStatus = true;
    }
    
    // offset the tile UV by the border offset:
    TileUV = ( TileUV * g_TileBorderUVTransform[IndexMapSlot].xy ) + g_TileBorderUVTransform[IndexMapSlot].zw;
    
    // compute the atlas UV by offsetting the tile UV by the atlas X and Y coordinates
    float2 AtlasUV = ( TileUV + PhysicalTileLocation.xz ) * g_ArrayTexConstants[IndexMapSlot].zw;
    
    // return the 3D texture coordinate used to look up into the physical tile array texture:
    return float3( AtlasUV, ( PhysicalTileLocation.y + g_ArraySliceCenterOffset ) * g_ArrayTexConstants[IndexMapSlot].x );
}

//--------------------------------------------------------------------------------------
// Name: ComputeTrilinearPhysicalTileUVW_MinLOD
// Desc: Given a UVW coord and index map slot, returns a pair of physical tile array texture 
//       locations that should be sampled and blended to produce the final texture sample.
//       The return value is the 0..1 lerp value for the trilinear blend.
//       A MinLOD value is provided that is used to clamp the computed LOD value.
//--------------------------------------------------------------------------------------
float ComputeTrilinearPhysicalTileUVW_MinLOD( const float3 TextureUVW, const float MinLOD, int IndexMapSlot, out float3 PhysicalTileA, out float3 PhysicalTileB )
{
    // Compute the mip LOD of the sample against the texture:
    float LOD = Get2DLOD( g_IndexMapTexture3D[IndexMapSlot], TextureUVW, IndexMapSlot );

    // Clamp and round the LOD value to two whole number lookup indices:
    LOD = clamp( LOD, MinLOD, 8 );
    float TrilinearLerp = frac( LOD );
    float IndexMapLODA = floor( LOD );
    float IndexMapLODB = ceil( LOD );
    float ConstantLookupLODA = IndexMapLODA;
    float ConstantLookupLODB = IndexMapLODB;
    
    // determine physical tile size and resource UV scaling for the given LOD:
    float4 InvTileSize_UVScaleA = g_PhysicalTileUVSizePerLOD[IndexMapSlot][ConstantLookupLODA];
    float4 InvTileSize_UVScaleB = g_PhysicalTileUVSizePerLOD[IndexMapSlot][ConstantLookupLODB];
    
    // adjust incoming UV coordinates to map to the proper scale for the index map lookup:
#ifdef SUPPORT_NON_POW2_DIMENSIONS
    float3 IndexMapUVWA = float3( TextureUVW.xy * InvTileSize_UVScaleA.zw, TextureUVW.z );
    float3 IndexMapUVWB = float3( TextureUVW.xy * InvTileSize_UVScaleB.zw, TextureUVW.z );
#else
    float3 IndexMapUVWA = TextureUV.xyz;
    float3 IndexMapUVWB = TextureUV.xyz;
#endif
    
    // determine physical tile location:
    // X component is the atlas X index
    // Y component is the atlas Y index
    // Z component is the normalized array slice index
    // W component is 255 if the entry is valid, 0 otherwise
    float4 PhysicalTileLocationA;
    float4 PhysicalTileLocationB;
    asm
    {
        setTexLOD IndexMapLODA.x;
        tfetch3D PhysicalTileLocationA, IndexMapUVWA, g_IndexMapTexture3D[IndexMapSlot], UseComputedLOD = false, UseRegisterLOD = true;
        setTexLOD IndexMapLODB.x;
        tfetch3D PhysicalTileLocationB, IndexMapUVWB, g_IndexMapTexture3D[IndexMapSlot], UseComputedLOD = false, UseRegisterLOD = true;
    };
    
    // convert resource UV space to tile UV space:
    float2 TileUVA = ComputeTileUV( TextureUVW.xy, InvTileSize_UVScaleA.xy );
    float2 TileUVB = ComputeTileUV( TextureUVW.xy, InvTileSize_UVScaleB.xy );
    
    bool ResidencyStatus = true;
    
    if( PhysicalTileLocationA.x >= 31 )
    {
        // residency failure; return the tile UV and -1 for the array slice index
        PhysicalTileA = float3( TileUVA, -1 );
        ResidencyStatus = false;
    }
    else
    {
        // offset the tile UV by the border offset:
        TileUVA = ( TileUVA * g_TileBorderUVTransform[IndexMapSlot].xy ) + g_TileBorderUVTransform[IndexMapSlot].zw;
        
        // compute the atlas UV by offsetting the tile UV by the atlas X and Y coordinates
        float2 AtlasUV = ( TileUVA + PhysicalTileLocationA.xz ) * g_ArrayTexConstants[IndexMapSlot].zw;
        
        // return the 3D texture coordinate used to look up into the physical tile array texture:
        PhysicalTileA = float3( AtlasUV, ( PhysicalTileLocationA.y + g_ArraySliceCenterOffset ) * g_ArrayTexConstants[IndexMapSlot].x );
    }
    
    if( PhysicalTileLocationB.x >= 31 )
    {
        // residency failure; return the tile UV and -1 for the array slice index
        PhysicalTileB = float3( TileUVB, -1 );
        
        ResidencyStatus = false;
    }
    else
    {
        // offset the tile UV by the border offset:
        TileUVB = ( TileUVB * g_TileBorderUVTransform[IndexMapSlot].xy ) + g_TileBorderUVTransform[IndexMapSlot].zw;
        
        // compute the atlas UV by offsetting the tile UV by the atlas X and Y coordinates
        float2 AtlasUV = ( TileUVB + PhysicalTileLocationB.xz ) * g_ArrayTexConstants[IndexMapSlot].zw;
        
        // return the 3D texture coordinate used to look up into the physical tile array texture:
        PhysicalTileB = float3( AtlasUV, ( PhysicalTileLocationB.y + g_ArraySliceCenterOffset ) * g_ArrayTexConstants[IndexMapSlot].x );
    }
    
    g_ResidencyStatus = ResidencyStatus;
    
    return TrilinearLerp;
}

//--------------------------------------------------------------------------------------
// Name: ComputeTrilinearPhysicalTileUVW
// Desc: Given a UVW coord and index map slot, returns a pair of physical tile array texture 
//       locations that should be sampled and blended to produce the final texture sample.
//       The return value is the 0..1 lerp value for the trilinear blend.
//--------------------------------------------------------------------------------------

float ComputeTrilinearPhysicalTileUVW( const float3 TextureUVW, int IndexMapSlot, out float3 PhysicalTileA, out float3 PhysicalTileB )
{
    return ComputeTrilinearPhysicalTileUVW_MinLOD( TextureUVW, 0, IndexMapSlot, PhysicalTileA, PhysicalTileB );
}

//--------------------------------------------------------------------------------------
// Name: ComputeTrilinearPhysicalTileUVW_FixedLOD
// Desc: Given a UVW coord and index map slot, returns a pair of physical tile array texture 
//       locations that should be sampled and blended to produce the final texture sample.
//       The return value is the 0..1 lerp value for the trilinear blend.
//       A fixed LOD value is provided that is used instead of a computed LOD value.
//--------------------------------------------------------------------------------------

float ComputeTrilinearPhysicalTileUVW_FixedLOD( const float3 TextureUVW, const float FixedLOD, int IndexMapSlot, out float3 PhysicalTileA, out float3 PhysicalTileB )
{

    // Clamp and round the LOD value to two whole number lookup indices:
    float LOD = clamp( FixedLOD, 0, 8 );
    float TrilinearLerp = frac( LOD );
    float IndexMapLODA = floor( LOD );
    float IndexMapLODB = ceil( LOD );
    float ConstantLookupLODA = IndexMapLODA;
    float ConstantLookupLODB = IndexMapLODB;
    
    // determine physical tile size and resource UV scaling for the given LOD:
    float4 InvTileSize_UVScaleA = g_PhysicalTileUVSizePerLOD[IndexMapSlot][ConstantLookupLODA];
    float4 InvTileSize_UVScaleB = g_PhysicalTileUVSizePerLOD[IndexMapSlot][ConstantLookupLODB];
    
    // adjust incoming UV coordinates to map to the proper scale for the index map lookup:
#ifdef SUPPORT_NON_POW2_DIMENSIONS
    float3 IndexMapUVWA = float3( TextureUVW.xy * InvTileSize_UVScaleA.zw, TextureUVW.z );
    float3 IndexMapUVWB = float3( TextureUVW.xy * InvTileSize_UVScaleB.zw, TextureUVW.z );
#else
    float3 IndexMapUVWA = TextureUV.xyz;
    float3 IndexMapUVWB = TextureUV.xyz;
#endif
    
    // determine physical tile location:
    // X component is the atlas X index
    // Y component is the atlas Y index
    // Z component is the normalized array slice index
    // W component is 255 if the entry is valid, 0 otherwise
    float4 PhysicalTileLocationA;
    float4 PhysicalTileLocationB;
    asm
    {
        setTexLOD IndexMapLODA.x;
        tfetch3D PhysicalTileLocationA, IndexMapUVWA, g_IndexMapTexture3D[IndexMapSlot], UseComputedLOD = false, UseRegisterLOD = true;
        setTexLOD IndexMapLODB.x;
        tfetch3D PhysicalTileLocationB, IndexMapUVWB, g_IndexMapTexture3D[IndexMapSlot], UseComputedLOD = false, UseRegisterLOD = true;
    };
    
    // convert resource UV space to tile UV space:
    float2 TileUVA = ComputeTileUV( TextureUVW.xy, InvTileSize_UVScaleA.xy );
    float2 TileUVB = ComputeTileUV( TextureUVW.xy, InvTileSize_UVScaleB.xy );
    
    bool ResidencyStatus = true;
    
    if( PhysicalTileLocationA.x >= 31 )
    {
        // residency failure; return the tile UV and -1 for the array slice index
        PhysicalTileA = float3( TileUVA, -1 );
        ResidencyStatus = false;
    }
    else
    {
        // offset the tile UV by the border offset:
        TileUVA = ( TileUVA * g_TileBorderUVTransform[IndexMapSlot].xy ) + g_TileBorderUVTransform[IndexMapSlot].zw;
        
        // compute the atlas UV by offsetting the tile UV by the atlas X and Y coordinates
        float2 AtlasUV = ( TileUVA + PhysicalTileLocationA.xz ) * g_ArrayTexConstants[IndexMapSlot].zw;
        
        // return the 3D texture coordinate used to look up into the physical tile array texture:
        PhysicalTileA = float3( AtlasUV, ( PhysicalTileLocationA.y + g_ArraySliceCenterOffset ) * g_ArrayTexConstants[IndexMapSlot].x );
    }
    
    if( PhysicalTileLocationB.x >= 31 )
    {
        // residency failure; return the tile UV and -1 for the array slice index
        PhysicalTileB = float3( TileUVB, -1 );
        
        ResidencyStatus = false;
    }
    else
    {
        // offset the tile UV by the border offset:
        TileUVB = ( TileUVB * g_TileBorderUVTransform[IndexMapSlot].xy ) + g_TileBorderUVTransform[IndexMapSlot].zw;
        
        // compute the atlas UV by offsetting the tile UV by the atlas X and Y coordinates
        float2 AtlasUV = ( TileUVB + PhysicalTileLocationB.xz ) * g_ArrayTexConstants[IndexMapSlot].zw;
        
        // return the 3D texture coordinate used to look up into the physical tile array texture:
        PhysicalTileB = float3( AtlasUV, ( PhysicalTileLocationB.y + g_ArraySliceCenterOffset ) * g_ArrayTexConstants[IndexMapSlot].x );
    }
    
    g_ResidencyStatus = ResidencyStatus;
    
    return TrilinearLerp;
}

//--------------------------------------------------------------------------------------
//
// BEGIN PUBLIC FUNCTIONS
//
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: GetResidencyStatus
// Desc: Returns true if the last tiled resource sample fell completely within resident
//       tiles; returns false otherwise.
//--------------------------------------------------------------------------------------
bool GetResidencyStatus()
{
    return g_ResidencyStatus;
}

//--------------------------------------------------------------------------------------
// Name: GetQuiltDimensions
// Desc: Returns the quilt dimensions of the given tiled resource slot.
//--------------------------------------------------------------------------------------
void GetQuiltDimensions( int IndexMapSlot, out float QuiltWidth, out float QuiltHeight )
{
    const float4 QuiltConstants = g_ResourceMiscConstant[IndexMapSlot];
    QuiltWidth = QuiltConstants.z;
    QuiltHeight = QuiltConstants.w;
}

//--------------------------------------------------------------------------------------
// Name: Quilt2DToTex3D
// Desc: Converts an extended UV address formatted for quilting into a normalized UVW
//       address that indexes into an array texture.
//--------------------------------------------------------------------------------------
float3 Quilt2DToTex3D( int IndexMapSlot, const float2 UV )
{
    const float4 QuiltConstants = g_ResourceMiscConstant[IndexMapSlot];
    
    int RowIndex = clamp( (int)UV.y, 0, QuiltConstants.w );
    int ColumnIndex = clamp( (int)UV.x, 0, QuiltConstants.z );
    int SliceIndex = RowIndex * QuiltConstants.z + ColumnIndex;
    
    float SliceAddress = (float)SliceIndex * QuiltConstants.y;
    
    return float3( frac( UV ), SliceAddress );
}

//--------------------------------------------------------------------------------------
//
// BEGIN PUBLIC TEXTURE SAMPLING FUNCTIONS
//
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: TiledTex2D_Point
// Desc: Samples a tiled texture 2D using point sampling.
//--------------------------------------------------------------------------------------
float4 TiledTex2D_Point( int IndexMapSlot, const float2 UV )
{
    float3 PhysicalCoords = ComputePhysicalTileUV( UV, IndexMapSlot );
    if( PhysicalCoords.z < 0 )
    {
        return 0;
    }
    float4 Sample;
    asm
    {
        tfetch3D Sample, PhysicalCoords, g_PhysicalTileTexture[IndexMapSlot], MagFilter=point, MinFilter=point, MipFilter=point, VolMagFilter=point, VolMinFilter=point
    };
    return Sample;
}

//--------------------------------------------------------------------------------------
// Name: TiledTex2D_Point_MinLOD
// Desc: Samples a tiled texture 2D using point sampling, using a minimum LOD clamp value.
//--------------------------------------------------------------------------------------
float4 TiledTex2D_Point_MinLOD( int IndexMapSlot, const float2 UV, const float MinLOD )
{
    float3 PhysicalCoords = ComputePhysicalTileUV_MinLOD( UV, MinLOD, IndexMapSlot );
    if( PhysicalCoords.z < 0 )
    {
        return 0;
    }
    float4 Sample;
    asm
    {
        tfetch3D Sample, PhysicalCoords, g_PhysicalTileTexture[IndexMapSlot], MagFilter=point, MinFilter=point, MipFilter=point, VolMagFilter=point, VolMinFilter=point
    };
    return Sample;
}

//--------------------------------------------------------------------------------------
// Name: TiledTex2D_Point_FixedLOD
// Desc: Samples a tiled texture 2D using point sampling, using a fixed LOD value.
//--------------------------------------------------------------------------------------
float4 TiledTex2D_Point_FixedLOD( int IndexMapSlot, const float2 UV, const float FixedLOD )
{
    float3 PhysicalCoords = ComputePhysicalTileUV_FixedLOD( UV, FixedLOD, IndexMapSlot );
    if( PhysicalCoords.z < 0 )
    {
        return 0;
    }
    float4 Sample = 0;
    asm
    {
        setTexLOD Sample.x
        tfetch3D Sample, PhysicalCoords, g_PhysicalTileTexture[IndexMapSlot], MagFilter=point, MinFilter=point, MipFilter=point, VolMagFilter=point, VolMinFilter=point, UseComputedLOD=false, UseRegisterLOD=true
    };
    return Sample;
}

//--------------------------------------------------------------------------------------
// Name: TiledTex2D_Bilinear
// Desc: Samples a tiled texture 2D using bilinear sampling.
//--------------------------------------------------------------------------------------
float4 TiledTex2D_Bilinear( int IndexMapSlot, const float2 UV )
{
    float3 PhysicalCoords = ComputePhysicalTileUV( UV, IndexMapSlot );
    if( PhysicalCoords.z < 0 )
    {
        return 0;
    }
    float4 Sample;
    asm
    {
        tfetch3D Sample, PhysicalCoords, g_PhysicalTileTexture[IndexMapSlot], MagFilter=linear, MinFilter=linear, MipFilter=point, VolMagFilter=point, VolMinFilter=point
    };
    return Sample;
}

//--------------------------------------------------------------------------------------
// Name: TiledTex2D_Bilinear_MinLOD
// Desc: Samples a tiled texture 2D using bilinear sampling, using a minimum LOD clamp value.
//--------------------------------------------------------------------------------------
float4 TiledTex2D_Bilinear_MinLOD( int IndexMapSlot, const float2 UV, const float MinLOD )
{
    float3 PhysicalCoords = ComputePhysicalTileUV_MinLOD( UV, MinLOD, IndexMapSlot );
    if( PhysicalCoords.z < 0 )
    {
        return 0;
    }
    float4 Sample;
    asm
    {
        tfetch3D Sample, PhysicalCoords, g_PhysicalTileTexture[IndexMapSlot], MagFilter=linear, MinFilter=linear, MipFilter=point, VolMagFilter=point, VolMinFilter=point
    };
    return Sample;
}

//--------------------------------------------------------------------------------------
// Name: TiledTex2D_Trilinear
// Desc: Samples a tiled texture 2D using trilinear sampling.  Note that the trilinear
//       sample is emulated with two bilinear samples and a lerp.
//--------------------------------------------------------------------------------------
float4 TiledTex2D_Trilinear( int IndexMapSlot, const float2 UV )
{
    float3 PhysicalCoordsA;
    float3 PhysicalCoordsB;
    float TrilinearLerp = ComputeTrilinearPhysicalTileUV( UV, IndexMapSlot, PhysicalCoordsA, PhysicalCoordsB );

    float4 SampleA;
    if( PhysicalCoordsA.z >= 0 )
    {
        asm
        {
            tfetch3D SampleA, PhysicalCoordsA, g_PhysicalTileTexture[IndexMapSlot], MagFilter=linear, MinFilter=linear, MipFilter=point, VolMagFilter=point, VolMinFilter=point
        };
    }
    else
    {
        SampleA = 0;
    }

    float4 SampleB;
    if( PhysicalCoordsB.z >= 0 )
    {
        asm
        {
            tfetch3D SampleB, PhysicalCoordsB, g_PhysicalTileTexture[IndexMapSlot], MagFilter=linear, MinFilter=linear, MipFilter=point, VolMagFilter=point, VolMinFilter=point
        };
    }
    else
    {
        SampleB = 0;
    }

    return lerp( SampleA, SampleB, TrilinearLerp );
}

//--------------------------------------------------------------------------------------
// Name: TiledTex2D_Trilinear_MinLOD
// Desc: Samples a tiled texture 2D using trilinear sampling and a minimum LOD clamp value.  
//       Note that the trilinear sample is emulated with two bilinear samples and a lerp.
//--------------------------------------------------------------------------------------
float4 TiledTex2D_Trilinear_MinLOD( int IndexMapSlot, const float2 UV, const float MinLOD )
{
    float3 PhysicalCoordsA;
    float3 PhysicalCoordsB;
    float TrilinearLerp = ComputeTrilinearPhysicalTileUV_MinLOD( UV, MinLOD, IndexMapSlot, PhysicalCoordsA, PhysicalCoordsB );

    float4 SampleA;
    if( PhysicalCoordsA.z >= 0 )
    {
        asm
        {
            tfetch3D SampleA, PhysicalCoordsA, g_PhysicalTileTexture[IndexMapSlot], MagFilter=linear, MinFilter=linear, MipFilter=point, VolMagFilter=point, VolMinFilter=point
        };
    }
    else
    {
        SampleA = 0;
    }

    float4 SampleB;
    if( PhysicalCoordsB.z >= 0 )
    {
        asm
        {
            tfetch3D SampleB, PhysicalCoordsB, g_PhysicalTileTexture[IndexMapSlot], MagFilter=linear, MinFilter=linear, MipFilter=point, VolMagFilter=point, VolMinFilter=point
        };
    }
    else
    {
        SampleB = 0;
    }

    return lerp( SampleA, SampleB, TrilinearLerp );
}

//--------------------------------------------------------------------------------------
// Name: TiledTex2D_Trilinear_FixedLOD
// Desc: Samples a tiled texture 2D using trilinear sampling and a fixed LOD value.  
//       Note that the trilinear sample is emulated with two bilinear samples and a lerp.
//--------------------------------------------------------------------------------------
float4 TiledTex2D_Trilinear_FixedLOD( int IndexMapSlot, const float2 UV, const float FixedLOD )
{
    float3 PhysicalCoordsA;
    float3 PhysicalCoordsB;
    float TrilinearLerp = ComputeTrilinearPhysicalTileUV_FixedLOD( UV, FixedLOD, IndexMapSlot, PhysicalCoordsA, PhysicalCoordsB );

    float4 SampleA = 0;
    if( PhysicalCoordsA.z >= 0 )
    {
        asm
        {
            setTexLOD SampleA.x
            tfetch3D SampleA, PhysicalCoordsA, g_PhysicalTileTexture[IndexMapSlot], MagFilter=linear, MinFilter=linear, MipFilter=point, VolMagFilter=point, VolMinFilter=point, UseComputedLOD=false, UseRegisterLOD=true
        };
    }
    else
    {
        SampleA = 0;
    }

    float4 SampleB = 0;
    if( PhysicalCoordsB.z >= 0 )
    {
        asm
        {
            setTexLOD SampleA.x
            tfetch3D SampleB, PhysicalCoordsB, g_PhysicalTileTexture[IndexMapSlot], MagFilter=linear, MinFilter=linear, MipFilter=point, VolMagFilter=point, VolMinFilter=point, UseComputedLOD=false, UseRegisterLOD=true
        };
    }
    else
    {
        SampleB = 0;
    }

    return lerp( SampleA, SampleB, TrilinearLerp );
}

//--------------------------------------------------------------------------------------
// Name: TiledTex3D_Point
// Desc: Samples a tiled texture 2D array using point sampling.  
//--------------------------------------------------------------------------------------

float4 TiledTex3D_Point( int IndexMapSlot, const float3 UVW )
{
    float3 PhysicalCoords = ComputePhysicalTileUVW( UVW, IndexMapSlot );
    if( PhysicalCoords.z < 0 )
    {
        return 0;
    }
    float4 Sample;
    asm
    {
        tfetch3D Sample, PhysicalCoords, g_PhysicalTileTexture[IndexMapSlot], MagFilter=point, MinFilter=point, MipFilter=point, VolMagFilter=point, VolMinFilter=point
    };
    return Sample;
}

//--------------------------------------------------------------------------------------
// Name: TiledTex3D_Point_MinLOD
// Desc: Samples a tiled texture 2D array using point sampling and a minimum LOD clamp value. 
//--------------------------------------------------------------------------------------

float4 TiledTex3D_Point_MinLOD( int IndexMapSlot, const float3 UVW, const float MinLOD )
{
    float3 PhysicalCoords = ComputePhysicalTileUVW_MinLOD( UVW, MinLOD, IndexMapSlot );
    if( PhysicalCoords.z < 0 )
    {
        return 0;
    }
    float4 Sample;
    asm
    {
        tfetch3D Sample, PhysicalCoords, g_PhysicalTileTexture[IndexMapSlot], MagFilter=point, MinFilter=point, MipFilter=point, VolMagFilter=point, VolMinFilter=point
    };
    return Sample;
}

//--------------------------------------------------------------------------------------
// Name: TiledTex3D_Point_FixedLOD
// Desc: Samples a tiled texture 2D array using point sampling and a fixed LOD value. 
//--------------------------------------------------------------------------------------

float4 TiledTex3D_Point_FixedLOD( int IndexMapSlot, const float3 UVW, const float FixedLOD )
{
    float3 PhysicalCoords = ComputePhysicalTileUVW_FixedLOD( UVW, FixedLOD, IndexMapSlot );
    if( PhysicalCoords.z < 0 )
    {
        return 0;
    }
    float4 Sample = 0;
    asm
    {
        setTexLOD Sample.x
        tfetch3D Sample, PhysicalCoords, g_PhysicalTileTexture[IndexMapSlot], MagFilter=point, MinFilter=point, MipFilter=point, VolMagFilter=point, VolMinFilter=point, UseComputedLOD=false, UseRegisterLOD=true 
    };
    return Sample;
}

//--------------------------------------------------------------------------------------
// Name: TiledTex3D_Bilinear
// Desc: Samples a tiled texture 2D array using bilinear sampling. 
//--------------------------------------------------------------------------------------

float4 TiledTex3D_Bilinear( int IndexMapSlot, const float3 UVW )
{
    float3 PhysicalCoords = ComputePhysicalTileUVW( UVW, IndexMapSlot );
    if( PhysicalCoords.z < 0 )
    {
        return 0;
    }
    float4 Sample;
    asm
    {
        tfetch3D Sample, PhysicalCoords, g_PhysicalTileTexture[IndexMapSlot], MagFilter=linear, MinFilter=linear, MipFilter=point, VolMagFilter=point, VolMinFilter=point
    };
    return Sample;
}

//--------------------------------------------------------------------------------------
// Name: TiledTex3D_Bilinear_MinLOD
// Desc: Samples a tiled texture 2D array using bilinear sampling and a minimum LOD clamp value. 
//--------------------------------------------------------------------------------------

float4 TiledTex3D_Bilinear_MinLOD( int IndexMapSlot, const float3 UVW, const float MinLOD )
{
    float3 PhysicalCoords = ComputePhysicalTileUVW_MinLOD( UVW, MinLOD, IndexMapSlot );
    if( PhysicalCoords.z < 0 )
    {
        return 0;
    }
    float4 Sample;
    asm
    {
        tfetch3D Sample, PhysicalCoords, g_PhysicalTileTexture[IndexMapSlot], MagFilter=linear, MinFilter=linear, MipFilter=point, VolMagFilter=point, VolMinFilter=point
    };
    return Sample;
}

//--------------------------------------------------------------------------------------
// Name: TiledTex3D_Trilinear
// Desc: Samples a tiled texture 2D array using trilinear sampling. 
//       Note that the trilinear sample is emulated with two bilinear samples and a lerp.
//--------------------------------------------------------------------------------------

float4 TiledTex3D_Trilinear( int IndexMapSlot, const float3 UVW )
{
    float3 PhysicalCoordsA;
    float3 PhysicalCoordsB;
    float TrilinearLerp = ComputeTrilinearPhysicalTileUVW( UVW, IndexMapSlot, PhysicalCoordsA, PhysicalCoordsB );

    float4 SampleA;
    if( PhysicalCoordsA.z >= 0 )
    {
        asm
        {
            tfetch3D SampleA, PhysicalCoordsA, g_PhysicalTileTexture[IndexMapSlot], MagFilter=linear, MinFilter=linear, MipFilter=point, VolMagFilter=point, VolMinFilter=point
        };
    }
    else
    {
        SampleA = 0;
    }

    float4 SampleB;
    if( PhysicalCoordsB.z >= 0 )
    {
        asm
        {
            tfetch3D SampleB, PhysicalCoordsB, g_PhysicalTileTexture[IndexMapSlot], MagFilter=linear, MinFilter=linear, MipFilter=point, VolMagFilter=point, VolMinFilter=point
        };
    }
    else
    {
        SampleB = 0;
    }

    return lerp( SampleA, SampleB, TrilinearLerp );
}

//--------------------------------------------------------------------------------------

// Name: TiledTex3D_Trilinear_MinLOD
// Desc: Samples a tiled texture 2D array using trilinear sampling and a minimum LOD clamp value.
//       Note that the trilinear sample is emulated with two bilinear samples and a lerp.
//--------------------------------------------------------------------------------------
float4 TiledTex3D_Trilinear_MinLOD( int IndexMapSlot, const float3 UVW, const float MinLOD )
{
    float3 PhysicalCoordsA;
    float3 PhysicalCoordsB;
    float TrilinearLerp = ComputeTrilinearPhysicalTileUVW_MinLOD( UVW, MinLOD, IndexMapSlot, PhysicalCoordsA, PhysicalCoordsB );

    float4 SampleA;
    if( PhysicalCoordsA.z >= 0 )
    {
        asm
        {
            tfetch3D SampleA, PhysicalCoordsA, g_PhysicalTileTexture[IndexMapSlot], MagFilter=linear, MinFilter=linear, MipFilter=point, VolMagFilter=point, VolMinFilter=point
        };
    }
    else
    {
        SampleA = 0;
    }

    float4 SampleB;
    if( PhysicalCoordsB.z >= 0 )
    {
        asm
        {
            tfetch3D SampleB, PhysicalCoordsB, g_PhysicalTileTexture[IndexMapSlot], MagFilter=linear, MinFilter=linear, MipFilter=point, VolMagFilter=point, VolMinFilter=point
        };
    }
    else
    {
        SampleB = 0;
    }

    return lerp( SampleA, SampleB, TrilinearLerp );
}

//--------------------------------------------------------------------------------------
// Name: TiledTex3D_Trilinear_FixedLOD
// Desc: Samples a tiled texture 2D array using trilinear sampling and a fixed LOD value.
//       Note that the trilinear sample is emulated with two bilinear samples and a lerp.
//--------------------------------------------------------------------------------------

float4 TiledTex3D_Trilinear_FixedLOD( int IndexMapSlot, const float3 UVW, const float FixedLOD )
{
    float3 PhysicalCoordsA;
    float3 PhysicalCoordsB;
    float TrilinearLerp = ComputeTrilinearPhysicalTileUVW_FixedLOD( UVW, FixedLOD, IndexMapSlot, PhysicalCoordsA, PhysicalCoordsB );

    float4 SampleA = 0;
    if( PhysicalCoordsA.z >= 0 )
    {
        asm
        {
            setTexLOD SampleA.x
            tfetch3D SampleA, PhysicalCoordsA, g_PhysicalTileTexture[IndexMapSlot], MagFilter=linear, MinFilter=linear, MipFilter=point, VolMagFilter=point, VolMinFilter=point, UseComputedLOD=false, UseRegisterLOD=true
        };
    }
    else
    {
        SampleA = 0;
    }

    float4 SampleB = 0;
    if( PhysicalCoordsB.z >= 0 )
    {
        asm
        {
            setTexLOD SampleB.x
            tfetch3D SampleB, PhysicalCoordsB, g_PhysicalTileTexture[IndexMapSlot], MagFilter=linear, MinFilter=linear, MipFilter=point, VolMagFilter=point, VolMinFilter=point, UseComputedLOD=false, UseRegisterLOD=true
        };
    }
    else
    {
        SampleB = 0;
    }

    return lerp( SampleA, SampleB, TrilinearLerp );
}

//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: TiledTex3D_ComputeLOD
// Desc: Computes the mipmap LOD of the given 3D texture coordinates on the given tiled
//       texture 2D array.
//--------------------------------------------------------------------------------------
float TiledTex3D_ComputeLOD( int IndexMapSlot, const float3 UVW )
{
    float LOD = Get2DLOD( g_IndexMapTexture3D[IndexMapSlot], UVW, IndexMapSlot );
    return LOD;
}

//--------------------------------------------------------------------------------------
// Name: TiledTex2D_ComputeLOD
// Desc: Computes the mipmap LOD of the given texture coordinates on the given tiled
//       texture 2D.
//--------------------------------------------------------------------------------------
float TiledTex2D_ComputeLOD( int IndexMapSlot, const float2 UV )
{
    float LOD = Get2DLOD( g_IndexMapTexture2D[IndexMapSlot], UV, IndexMapSlot );
    return LOD;
}

#endif // TILEDLIB_HLSL
