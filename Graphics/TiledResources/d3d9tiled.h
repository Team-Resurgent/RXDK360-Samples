//--------------------------------------------------------------------------------------
// d3d9tiled.h
//
// This is the front end of the tiled resource API, which is patterned after Direct3D 9
// semantics.  Through this API, you can create tiled resources and tile pools,
// manipulate tile memory and mappings, and render using tiled resources.
//
// Please note that this API is only a prototype for this sample only; the API does not 
// reflect current or future plans by Microsoft.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef _TILEDRESOURCEAPI_H_
#define _TILEDRESOURCEAPI_H_

#include <d3d9.h>

// Xbox-style friendly names for our new interfaces:
typedef struct D3DTiledResourceDevice  D3DTiledResourceDevice;
typedef struct D3DTilePool             D3DTilePool;
typedef struct D3DTiledResource        D3DTiledResource;
typedef struct D3DTiledTexture         D3DTiledTexture;
typedef struct D3DTiledArrayTexture    D3DTiledArrayTexture;

#define IDirect3DTiledResourceDevice9  D3DTiledResourceDevice
#define IDirect3DTilePool9             D3DTilePool
#define IDirect3DTiledResource9        D3DTiledResource
#define IDirect3DTiledTexture9         D3DTiledTexture
#define IDirect3DTiledArrayTexture9    D3DTiledArrayTexture

typedef struct D3DTiledResourceDevice  *LPDIRECT3DTILEDRESOURCEDEVICE9, *PDIRECT3DTILEDRESOURCEDEVICE9;
typedef struct D3DTilePool             *LPDIRECT3DTILEPOOL9,            *PDIRECT3DTILEPOOL9;
typedef struct D3DTiledResource        *LPDIRECT3DTILEDRESOURCE9,       *PDIRECT3DTILEDRESOURCE9;
typedef struct D3DTiledTexture         *LPDIRECT3DTILEDTEXTURE9,        *PDIRECT3DTILEDTEXTURE9;
typedef struct D3DTiledArrayTexture    *LPDIRECT3DTILEDARRAYTEXTURE9,   *PDIRECT3DTILEDARRAYTEXTURE9;

// D3DTILED_PHYSICAL_ADDRESS is a handle to a physical tile address in the tiled
// resource system:
typedef UINT64 D3DTILED_PHYSICAL_ADDRESS;

// D3DTILED_VIRTUAL_ADDRESS is a handle to a virtual tile address within a tiled
// resource:
typedef UINT64 D3DTILED_VIRTUAL_ADDRESS;

// Invalid handles for physical and virtual addresses:
#define D3DTILED_INVALID_PHYSICAL_ADDRESS ((D3DTILED_PHYSICAL_ADDRESS)-1)
#define D3DTILED_INVALID_VIRTUAL_ADDRESS ((D3DTILED_VIRTUAL_ADDRESS)0)

// Tiled equivalent to D3DRESOURCETYPE:
enum D3DTILED_RESOURCETYPE
{
    D3DSRTYPE_NONE = 0,
    D3DSRTYPE_TEXTURE,
    D3DSRTYPE_ARRAYTEXTURE
};

//--------------------------------------------------------------------------------------
// Name: D3DTILED_SURFACE_DESC
// Desc: A description of a single mip level within a tiled texture 2D.
//--------------------------------------------------------------------------------------
struct D3DTILED_SURFACE_DESC
{
    // Surface format:
    D3DFORMAT Format;

    // Width and height of the mip level, in texels:
    UINT TexelWidth;
    UINT TexelHeight;

    // Width and height of the mip level's usable virtual address space, in tiles:
    UINT TileWidth;
    UINT TileHeight;

    // Width and height of a single tile, in texels:
    UINT TileTexelWidth;
    UINT TileTexelHeight;
};

//--------------------------------------------------------------------------------------
// Name: D3DTiledResource
// Desc: A base class that represents a single tiled resource.  Currently, all tiled
//       resource methods are kept in resource type subclasses, such as D3DTiledTexture.
//--------------------------------------------------------------------------------------
struct D3DTiledResource
{
    D3DTILED_RESOURCETYPE GetType();
    UINT AddRef();
    UINT Release();
};

//--------------------------------------------------------------------------------------
// Name: D3DTiledTexture
// Desc: Represents a tiled texture 2D in the tiled resource system.  Methods are provided
//       to query the page layout of the tiled resource, and do conversions between UV
//       coordinates and virtual addresses.
//--------------------------------------------------------------------------------------
struct D3DTiledTexture
    : public D3DTiledResource
{
    UINT GetLevelCount();
    VOID GetLevelDesc( UINT Level, __out D3DTILED_SURFACE_DESC* pDesc );

    D3DTILED_VIRTUAL_ADDRESS GetTileVirtualAddress( UINT Level, FLOAT TextureU, FLOAT TextureV );
};

//--------------------------------------------------------------------------------------
// Name: D3DTiledArrayTexture
// Desc: Represents a tiled texture 2D array in the tiled resource system.  Methods are 
//       provided to query the page layout of the tiled resource, and do conversions 
//       between UV coordinates and virtual addresses.
//--------------------------------------------------------------------------------------
struct D3DTiledArrayTexture
    : public D3DTiledTexture
{
    UINT GetArraySize();
    BOOL GetQuiltSize( __out UINT* pQuiltWidth, __out UINT* pQuiltHeight );
    VOID ConvertQuiltUVToArrayUVSlice( __inout FLOAT* pTextureU, __inout FLOAT* pTextureV, __out UINT* pSliceIndex );

    D3DTILED_VIRTUAL_ADDRESS GetTileVirtualAddress( UINT ArraySlice, UINT Level, FLOAT TextureU, FLOAT TextureV );
};

//--------------------------------------------------------------------------------------
// Name: D3DTILED_MEMORY_USAGE
// Desc: A struct that is used to query the memory status of the tiled resource system.
//--------------------------------------------------------------------------------------
struct D3DTILED_MEMORY_USAGE
{
    // Total number of physical tiles allowed in the tiled resource system:
    UINT64 TileCapacity;

    // Total number of physical tiles currently allocated:
    UINT64 TilesAllocated;

    // Number of separate typed format pools that are currently in use:
    UINT FormatPoolsActive;

    // The amount of video memory consumed by all of the typed format pools:
    UINT64 TileTextureMemoryBytesAllocated;

    // The amount of video memory consumed by all of the resources' virtual to physical mapping textures:
    UINT64 ResourcePhysicalMemoryBytesAllocated;

    // The amount of video memory consumed by all of the resources' virtual to physical CPU structures:
    UINT64 ResourceCachedMemoryBytesAllocated;

    // The amount of system memory consumed by various tracking structures in the tiled resource system:
    UINT64 OverheadMemoryBytesAllocated;

    // The number of resources active in the tiled resource system:
    UINT ResourceCount;

    // The total amount of virtual address space spanned by all of the tiled resources:
    UINT64 ResourceVirtualBytesAllocated;
};

//--------------------------------------------------------------------------------------
// Name: D3DTilePool
// Desc: The tile pool is the D3D interface to the video memory system, including
//       physical tile allocation and deallocation, physical tile data access, and
//       mappings between virtual addresses and physical addresses.
//--------------------------------------------------------------------------------------
struct D3DTilePool
{
    HRESULT AllocatePhysicalTile( __out D3DTILED_PHYSICAL_ADDRESS* pPhysicalAddress, __in_opt D3DFORMAT TileFormat = (D3DFORMAT)0 );
    HRESULT FreePhysicalTile( D3DTILED_PHYSICAL_ADDRESS PhysicalAddress );

    HRESULT UpdateTileContents( D3DTILED_PHYSICAL_ADDRESS PhysicalAddress, __in const VOID* pBuffer, __in_opt D3DFORMAT BufferDataFormat = (D3DFORMAT)0 );

    HRESULT MapVirtualTileToPhysicalTile( D3DTILED_VIRTUAL_ADDRESS VirtualAddress, D3DTILED_PHYSICAL_ADDRESS PhysicalAddress );
    HRESULT UnmapVirtualAddress( D3DTILED_VIRTUAL_ADDRESS VirtualAddress );

    VOID GetMemoryUsage( __inout D3DTILED_MEMORY_USAGE* pMemoryUsage );

    UINT AddRef();
    UINT Release();
};

//--------------------------------------------------------------------------------------
// Name: D3DTiledResourceDevice
// Desc: The extended device is a set of resource manipulation functions that allow the 
//       title to create tiled resources, tiled resource shader resource views, and 
//       access the page manager.
//       It also contains device context functionality to set tiled resource textures
//       into the device context.
//       It also contains an entrypoint that must be called once per frame by the title
//       app, in order to give the tiled resource system an opportunity to execute D3D
//       operations on its internal resources.
//--------------------------------------------------------------------------------------
struct D3DTiledResourceDevice
{
    HRESULT CreateTilePool( __out D3DTilePool** ppTilePool );
    HRESULT CreateTexture( __in_opt D3DTilePool* pTilePool, UINT Width, UINT Height, UINT Levels, D3DFORMAT Format, __out D3DTiledTexture** ppTexture );
    HRESULT CreateArrayTexture( __in_opt D3DTilePool* pTilePool, UINT Width, UINT Height, UINT ArraySize, UINT Levels, D3DFORMAT Format, __out D3DTiledArrayTexture** ppArrayTexture );
    HRESULT CreateQuiltedArrayTexture( __in_opt D3DTilePool* pTilePool, UINT Width, UINT Height, UINT QuiltWidth, UINT QuiltHeight, UINT Levels, D3DFORMAT Format, __out D3DTiledArrayTexture** ppArrayTexture );

    HRESULT PreFrameRender();

    HRESULT SetTexture( UINT SamplerIndex, __in_opt D3DTiledTexture* pTexture );

    UINT AddRef();
    UINT Release();
};

//--------------------------------------------------------------------------------------
// Name: D3DTILED_EMULATION_PARAMETERS
// Desc: A struct that defines initialization parameters for the software implementation
//       of tiled resources.
//--------------------------------------------------------------------------------------
struct D3DTILED_EMULATION_PARAMETERS
{
    UINT MaxPhysicalTileCount;
    D3DFORMAT DefaultPhysicalTileFormat;
};

//--------------------------------------------------------------------------------------
// Name: Direct3D_CreateTiledResourceDevice
// Desc: Top level method to create a tiled resource D3D device from a D3D9 device.
//--------------------------------------------------------------------------------------
HRESULT Direct3D_CreateTiledResourceDevice( __in D3DDevice* pd3dDevice, __in const D3DTILED_EMULATION_PARAMETERS* pEmulationParameters, __out D3DTiledResourceDevice** ppTiledResourceDevice );

#endif /* _TILEDRESOURCEAPI_H_ */
