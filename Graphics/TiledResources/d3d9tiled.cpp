//--------------------------------------------------------------------------------------
// d3d9tiled.cpp
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include "d3d9tiled.h"

#include "TiledResourceCommon.h"
#include "PhysicalPageManager.h"
#include "TypedPagePool.h"
#include "TiledResourceBase.h"

#pragma warning( disable : 6211 ) //"Leaking memory due to an exception"

using namespace TiledRuntime;

//--------------------------------------------------------------------------------------
// Name: CTiledResource
// Desc: Internal subclass of D3DTiledResource.  It holds a pointer to the
//       TiledResourceBase implementation class.
//--------------------------------------------------------------------------------------
struct CTiledResource : public D3DTiledResource
{
    TiledResourceBase* m_pResource;
    INT m_RefCount;

    CTiledResource()
    {
        m_RefCount = 1;
        m_pResource = NULL;
    }
};

//--------------------------------------------------------------------------------------
// Name: CTilePool
// Desc: Internal subclass of D3DTilePool.  It holds a pointer to a PhysicalPageManager.
//--------------------------------------------------------------------------------------
struct CTilePool : public D3DTilePool
{
    PhysicalPageManager* m_pPageManager;
    INT m_RefCount;

    CTilePool()
    {
        m_RefCount = 1;
        m_pPageManager = NULL;
    }
};

//--------------------------------------------------------------------------------------
// Name: CTiledResourceDevice
// Desc: Internal subclass of D3DTiledResourceDevice.  It holds pointers to the real D3D9 device
//       and the single tile pool.
//--------------------------------------------------------------------------------------
struct CTiledResourceDevice : public D3DTiledResourceDevice
{
    D3DDevice* m_pd3dDevice;
    CTilePool* m_pTilePool;
    INT m_RefCount;

    CTiledResourceDevice()
    {
        m_RefCount = 1;
        m_pd3dDevice = NULL;
        m_pTilePool = NULL;
    }
};

// Promotion functions that cast an external pointer to an internal pointer:
CTiledResourceDevice* Promote( D3DTiledResourceDevice* pTiledResourceDevice ) { return static_cast<CTiledResourceDevice*>( pTiledResourceDevice ); }
CTilePool* Promote( D3DTilePool* pTilePool ) { return static_cast<CTilePool*>( pTilePool ); }
CTiledResource* Promote( D3DTiledResource* pResource ) { return static_cast<CTiledResource*>( pResource ); }

//--------------------------------------------------------------------------------------
// Name: Direct3D_CreateTiledResourceDevice
// Desc: Creates a new tiled resource Direct3D device.
//--------------------------------------------------------------------------------------
HRESULT Direct3D_CreateTiledResourceDevice( D3DDevice* pd3dDevice, const D3DTILED_EMULATION_PARAMETERS* pEmulationParameters, D3DTiledResourceDevice** ppTiledResourceDevice )
{
    // Create the new device:
    CTiledResourceDevice* pTiledResourceDevice = new CTiledResourceDevice();
    
    // Fill in pointers to the real D3D9 device:
    pTiledResourceDevice->m_pd3dDevice = pd3dDevice;
    pTiledResourceDevice->m_pd3dDevice->AddRef();

    // Convert the initialization parameters to their internal format:
    PhysicalPageManagerDesc PPMDesc;
    ZeroMemory( &PPMDesc, sizeof(PhysicalPageManagerDesc) );
    if( pEmulationParameters != NULL )
    {
        PPMDesc.MaxPhysicalPages = pEmulationParameters->MaxPhysicalTileCount;
        PPMDesc.EmulationParams.DefaultResourceFormat = pEmulationParameters->DefaultPhysicalTileFormat;
    }

    // Create the single tile pool for this device:
    CTilePool* pTilePool = new CTilePool();
    pTilePool->m_pPageManager = new PhysicalPageManager( pTiledResourceDevice->m_pd3dDevice, &PPMDesc );

    pTiledResourceDevice->m_pTilePool = pTilePool;

    // Return the pointer to the tiled resource device:
    *ppTiledResourceDevice = pTiledResourceDevice;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledResourceDevice::CreateTilePool
// Desc: Returns a pointer for the single tile pool for this device.
//--------------------------------------------------------------------------------------
HRESULT D3DTiledResourceDevice::CreateTilePool( __out D3DTilePool** ppTilePool )
{
    CTiledResourceDevice* p = Promote( this );

    p->m_pTilePool->AddRef();
    *ppTilePool = p->m_pTilePool;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledResourceDevice::CreateTexture
// Desc: Creates a new tiled texture 2D resource.
//--------------------------------------------------------------------------------------
HRESULT D3DTiledResourceDevice::CreateTexture( D3DTilePool* pTilePool, UINT Width, UINT Height, UINT Levels, D3DFORMAT Format, __out D3DTiledTexture** ppTexture )
{
    if( ppTexture == NULL )
    {
        return E_INVALIDARG;
    }

    CTiledResourceDevice* p = Promote( this );

    // If we were not passed a tile pool pointer, use the device's default pointer:
    CTilePool* pTP = NULL;
    if( pTilePool != NULL )
    {
        pTP = Promote( pTilePool );

        // We must use the tile pool created by this device:
        ASSERT( pTP == p->m_pTilePool );
    }
    else
    {
        pTP = p->m_pTilePool;
    }

    // Create the tiled texture object and initialize:
    TiledTexture* pTiledTexture = new TiledTexture();
    HRESULT hr = pTiledTexture->Initialize( pTP->m_pPageManager, Width, Height, Levels, 1, Format );

    if( FAILED(hr) )
    {
        delete pTiledTexture;
        *ppTexture = NULL;
        return hr;
    }

    // Create a tiled resource wrapper object:
    CTiledResource* pTiledResource = new CTiledResource();
    pTiledResource->m_pResource = pTiledTexture;

    // Return the wrapper object:
    *ppTexture = (D3DTiledTexture*)pTiledResource;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledResourceDevice::CreateArrayTexture
// Desc: Creates a new tiled texture 2D array resource.
//--------------------------------------------------------------------------------------
HRESULT D3DTiledResourceDevice::CreateArrayTexture( __in D3DTilePool* pTilePool, UINT Width, UINT Height, UINT ArraySize, UINT Levels, D3DFORMAT Format, __out D3DTiledArrayTexture** ppArrayTexture )
{
    if( ppArrayTexture == NULL )
    {
        return E_INVALIDARG;
    }

    CTiledResourceDevice* p = Promote( this );

    // If we were not passed a tile pool pointer, use the device's default pointer:
    CTilePool* pTP = NULL;
    if( pTilePool != NULL )
    {
        pTP = Promote( pTilePool );

        // We must use the tile pool created by this device:
        ASSERT( pTP == p->m_pTilePool );
    }
    else
    {
        pTP = p->m_pTilePool;
    }

    // Create the tiled texture object and initialize:
    TiledArrayTexture* pTiledArrayTexture = new TiledArrayTexture();
    HRESULT hr = pTiledArrayTexture->Initialize( pTP->m_pPageManager, Width, Height, Levels, ArraySize, Format );

    if( FAILED(hr) )
    {
        delete pTiledArrayTexture;
        *ppArrayTexture = NULL;
        return hr;
    }

    // Create a tiled resource wrapper object:
    CTiledResource* pTiledResource = new CTiledResource();
    pTiledResource->m_pResource = pTiledArrayTexture;

    // Return the wrapper object:
    *ppArrayTexture = (D3DTiledArrayTexture*)pTiledResource;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledResourceDevice::CreateQuiltedArrayTexture
// Desc: Creates a new tiled texture 2D array resource that is accessed as a 2D
//       rectangular quilt.
//--------------------------------------------------------------------------------------
HRESULT D3DTiledResourceDevice::CreateQuiltedArrayTexture( __in D3DTilePool* pTilePool, UINT Width, UINT Height, UINT QuiltWidth, UINT QuiltHeight, UINT Levels, D3DFORMAT Format, __out D3DTiledArrayTexture** ppArrayTexture )
{
    if( ppArrayTexture == NULL )
    {
        return E_INVALIDARG;
    }

    // Validate quilt parameters:
    if( QuiltWidth == 0 || QuiltHeight == 0 )
    {
        return E_INVALIDARG;
    }

    UINT ArraySize = QuiltWidth * QuiltHeight;

    if( ArraySize > MAX_ARRAY_SLICES )
    {
        return E_INVALIDARG;
    }

    CTiledResourceDevice* p = Promote( this );

    // If we were not passed a tile pool pointer, use the device's default pointer:
    CTilePool* pTP = NULL;
    if( pTilePool != NULL )
    {
        pTP = Promote( pTilePool );

        // We must use the tile pool created by this device:
        ASSERT( pTP == p->m_pTilePool );
    }
    else
    {
        pTP = p->m_pTilePool;
    }

    // Create the tiled texture object and initialize:
    TiledArrayTexture* pTiledArrayTexture = new TiledArrayTexture();
    HRESULT hr = pTiledArrayTexture->Initialize( pTP->m_pPageManager, Width, Height, Levels, ArraySize, Format );

    if( FAILED(hr) )
    {
        delete pTiledArrayTexture;
        *ppArrayTexture = NULL;
        return hr;
    }

    // Initialize the quilting state in the tiled array texture:
    hr = pTiledArrayTexture->SetQuilted( QuiltWidth, QuiltHeight );
    ASSERT( SUCCEEDED(hr) );

    // Create a tiled resource wrapper object:
    CTiledResource* pTiledResource = new CTiledResource();
    pTiledResource->m_pResource = pTiledArrayTexture;

    // Return the wrapper object:
    *ppArrayTexture = (D3DTiledArrayTexture*)pTiledResource;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledResourceDevice::PreFrameRender
// Desc: Executes per-frame graphics operations that are required for the tiled resource
//       system to operate.
//--------------------------------------------------------------------------------------
HRESULT D3DTiledResourceDevice::PreFrameRender()
{
    CTiledResourceDevice* p = Promote( this );

    // Call into the physical page manager to execute its page operations:
    p->m_pTilePool->m_pPageManager->ExecutePageDataOperations();

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledResourceDevice::SetTexture
// Desc: Sets a single tiled texture into the graphics pipeline.  Also
//       updates the shader constants associated with the tiled texture.
//--------------------------------------------------------------------------------------
HRESULT D3DTiledResourceDevice::SetTexture( UINT SamplerIndex, __in D3DTiledTexture* pTexture )
{
    CTiledResourceDevice* p = Promote( this );
    CTiledResource* pTiledResource = Promote( pTexture );

    if( pTiledResource != NULL )
    {
        pTiledResource->m_pResource->SetTexture( p->m_pd3dDevice, SamplerIndex );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledResourceDevice::AddRef
// Desc: Increases the refcount on the extended device.
//--------------------------------------------------------------------------------------
UINT D3DTiledResourceDevice::AddRef()
{
    CTiledResourceDevice* p = Promote( this );
    p->m_RefCount++;
    ASSERT( p->m_RefCount > 0 );

    return (UINT)p->m_RefCount;
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledResourceDevice::Release
// Desc: Decreases the refcount on the extended device.  When it reaches zero, the device
//       is destroyed.
//--------------------------------------------------------------------------------------
UINT D3DTiledResourceDevice::Release()
{
    CTiledResourceDevice* p = Promote( this );
    p->m_RefCount--;
    ASSERT( p->m_RefCount >= 0 );

    UINT Result = p->m_RefCount;

    if( p->m_RefCount == 0 )
    {
        if( p->m_pTilePool != NULL )
        {
            UINT TilePoolRefCount = p->m_pTilePool->Release();
            ASSERT( TilePoolRefCount == 0 );
            p->m_pTilePool = NULL;
        }

        if( p->m_pd3dDevice != NULL )
        {
            p->m_pd3dDevice->Release();
            p->m_pd3dDevice = NULL;
        }

        delete this;
    }

    return Result;
}

//--------------------------------------------------------------------------------------
// Name: D3DTilePool::AllocatePhysicalTile
// Desc: Allocates a single 64KB physical tile.
//--------------------------------------------------------------------------------------
HRESULT D3DTilePool::AllocatePhysicalTile( __out D3DTILED_PHYSICAL_ADDRESS* pPhysicalAddress, __in_opt D3DFORMAT PageFormat )
{
    CTilePool* p = Promote( this );

    if( pPhysicalAddress == NULL )
    {
        return E_INVALIDARG;
    }

    // Call into the physical page manager to allocate the tile:
    PhysicalPageID PageID;
    HRESULT hr = p->m_pPageManager->AllocatePage( &PageID, PageFormat );

    // Convert the PhysicalPageID to the more opaque D3DTILED_PHYSICAL_ADDRESS:
    *pPhysicalAddress = PageID;
    return hr;
}

//--------------------------------------------------------------------------------------
// Name: D3DTilePool::FreePhysicalTile
// Desc: Frees a single 64KB physical tile.
//--------------------------------------------------------------------------------------
HRESULT D3DTilePool::FreePhysicalTile( D3DTILED_PHYSICAL_ADDRESS PhysicalAddress )
{
    CTilePool* p = Promote( this );
    return p->m_pPageManager->FreePage( PhysicalAddress );
}

//--------------------------------------------------------------------------------------
// Name: D3DTilePool::UpdateTileContents
// Desc: Copies a 64KB buffer into a physical tile that has been allocated.
//--------------------------------------------------------------------------------------
HRESULT D3DTilePool::UpdateTileContents( D3DTILED_PHYSICAL_ADDRESS PhysicalAddress, const VOID* pBuffer, D3DFORMAT BufferDataFormat )
{
    CTilePool* p = Promote( this );
    return p->m_pPageManager->UpdateSinglePageContents( PhysicalAddress, pBuffer, BufferDataFormat );
}

//--------------------------------------------------------------------------------------
// Name: D3DTilePool::MapVirtualTileToPhysicalTile
// Desc: Maps a single virtual tile address to a single physical tile address, which may
//       be invalid.
//--------------------------------------------------------------------------------------
HRESULT D3DTilePool::MapVirtualTileToPhysicalTile( D3DTILED_VIRTUAL_ADDRESS VirtualAddress, D3DTILED_PHYSICAL_ADDRESS PhysicalAddress )
{
    CTilePool* p = Promote( this );

    // Convert the opaque D3D11_TILED_VIRTUAL_ADDRESS into the internal VirtualPageID struct:
    VirtualPageID VPageID;
    VPageID.VirtualAddress = VirtualAddress;

    return p->m_pPageManager->MapVirtualPageToPhysicalPage( VPageID, PhysicalAddress );
}

//--------------------------------------------------------------------------------------
// Name: D3DTilePool::UnmapVirtualAddress
// Desc: Maps the given virtual address to an invalid physical address.
//--------------------------------------------------------------------------------------
HRESULT D3DTilePool::UnmapVirtualAddress( D3DTILED_VIRTUAL_ADDRESS VirtualAddress )
{
    return MapVirtualTileToPhysicalTile( VirtualAddress, D3DTILED_INVALID_PHYSICAL_ADDRESS );
}

//--------------------------------------------------------------------------------------
// Name: D3DTilePool::GetMemoryUsage
// Desc: Populates a struct with current memory usage statistics for the tile pool
//       and all of the tiled resources.
//--------------------------------------------------------------------------------------
VOID D3DTilePool::GetMemoryUsage( D3DTILED_MEMORY_USAGE* pMemoryUsage )
{
    CTilePool* p = Promote( this );
    
    p->m_pPageManager->GetMemoryUsage( pMemoryUsage );
}

//--------------------------------------------------------------------------------------
// Name: D3DTilePool::AddRef
// Desc: Increases the refcount on the tile pool.
//--------------------------------------------------------------------------------------
UINT D3DTilePool::AddRef()
{
    CTilePool* p = Promote( this );

    ASSERT( p->m_RefCount >= 0 );
    p->m_RefCount++;

    return p->m_RefCount;
}

//--------------------------------------------------------------------------------------
// Name: D3DTilePool::Release
// Desc: Decreases the refcount on the tile pool.  When the refcount reaches zero,
//       the tile pool is deleted.
//--------------------------------------------------------------------------------------
UINT D3DTilePool::Release()
{
    CTilePool* p = Promote( this );
    p->m_RefCount--;
    ASSERT( p->m_RefCount >= 0 );

    UINT Result = p->m_RefCount;

    if( p->m_RefCount == 0 )
    {
        if( p->m_pPageManager != NULL )
        {
            delete p->m_pPageManager;
            p->m_pPageManager = NULL;
        }
        delete this;
    }

    return Result;
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledResource::GetType
// Desc: Returns the resource type of a tiled resource (texture or texture array).
//--------------------------------------------------------------------------------------
D3DTILED_RESOURCETYPE D3DTiledResource::GetType()
{
    CTiledResource* p = Promote( this );
    ASSERT( p->m_pResource != NULL );
    if( p->m_pResource->IsTexture2DArray() )
    {
        return D3DSRTYPE_ARRAYTEXTURE;
    }
    else if( p->m_pResource->IsTexture2D() )
    {
        return D3DSRTYPE_TEXTURE;
    }
    return D3DSRTYPE_NONE;
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledResource::AddRef
// Desc: Increases the refcount on the tiled resource.
//--------------------------------------------------------------------------------------
UINT D3DTiledResource::AddRef()
{
    CTiledResource* p = Promote( this );
    p->m_RefCount++;
    ASSERT( p->m_RefCount > 0 );

    return (UINT)p->m_RefCount;
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledResource::Release
// Desc: Decreases the refcount on the tiled resource.  When the refcount reaches zero,
//       the tiled resource is deleted.
//--------------------------------------------------------------------------------------
UINT D3DTiledResource::Release()
{
    CTiledResource* p = Promote( this );
    p->m_RefCount--;
    ASSERT( p->m_RefCount >= 0 );

    UINT Result = p->m_RefCount;

    if( p->m_RefCount == 0 )
    {
        if( p->m_pResource != NULL )
        {
            delete p->m_pResource;
            p->m_pResource = NULL;
        }
        delete this;
    }

    return Result;
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledTexture::GetLevelCount
// Desc: Returns the number of mip levels in the tiled texture.
//--------------------------------------------------------------------------------------
UINT D3DTiledTexture::GetLevelCount()
{
    CTiledResource* p = Promote( this );
    return p->m_pResource->GetMipLevelCount();
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledTexture::GetLevelDesc
// Desc: Fills in a struct that describes a mip level in the tiled texture.
//--------------------------------------------------------------------------------------
VOID D3DTiledTexture::GetLevelDesc( UINT Level, D3DTILED_SURFACE_DESC* pDesc )
{
    if( pDesc == NULL )
    {
        return;
    }

    CTiledResource* p = Promote( this );

    p->m_pResource->GetLevelDesc( Level, pDesc );
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledTexture::GetTileVirtualAddress
// Desc: For the given texture UV coordinates on the given mip level, return the 
//       virtual address of the tile where those UV coordinates are located.
//--------------------------------------------------------------------------------------
D3DTILED_VIRTUAL_ADDRESS D3DTiledTexture::GetTileVirtualAddress( UINT Level, FLOAT TextureU, FLOAT TextureV )
{
    CTiledResource* p = Promote( this );

    VirtualPageID VPageID = p->m_pResource->GetVirtualPageIDFloat( TextureU, TextureV, 0, Level );
    return VPageID.VirtualAddress;
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledArrayTexture::GetArraySize
// Desc: Returns the number of array slices in a tiled array texture.
//--------------------------------------------------------------------------------------
UINT D3DTiledArrayTexture::GetArraySize()
{
    CTiledResource* p = Promote( this );

    return p->m_pResource->GetArraySliceCount();
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledArrayTexture::GetQuiltSize
// Desc: Returns the quilt width and height for a tiled array texture.  If the tiled
//       array texture is not quilted, then this method returns FALSE.
//--------------------------------------------------------------------------------------
BOOL D3DTiledArrayTexture::GetQuiltSize( __out UINT* pQuiltWidth, __out UINT* pQuiltHeight )
{
    CTiledResource* p = Promote( this );

    if( pQuiltWidth == NULL || pQuiltHeight == NULL )
    {
        return FALSE;
    }

    if( !p->m_pResource->IsQuilted() )
    {
        return FALSE;
    }

    UINT QuiltWidth = p->m_pResource->GetQuiltWidth();
    UINT QuiltHeight = p->m_pResource->GetQuiltHeight();
    *pQuiltWidth = QuiltWidth;
    *pQuiltHeight = QuiltHeight;

    return TRUE;
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledArrayTexture::ConvertQuiltUVToArrayUVSlice
// Desc: Converts the given quilted UV coordinates into normalized
//       UV coordinates plus an array slice index.
//--------------------------------------------------------------------------------------
VOID D3DTiledArrayTexture::ConvertQuiltUVToArrayUVSlice( __inout FLOAT* pTextureU, __inout FLOAT* pTextureV, __out UINT* pSliceIndex )
{
    CTiledResource* p = Promote( this );

    if( pTextureU == NULL || pTextureV == NULL || pSliceIndex == NULL )
    {
        return;
    }

    if( !p->m_pResource->IsQuilted() )
    {
        return;
    }

    *pSliceIndex = p->m_pResource->ConvertQuiltUVToArrayUVW( pTextureU, pTextureV );

    return;
}

//--------------------------------------------------------------------------------------
// Name: D3DTiledArrayTexture::GetTileVirtualAddress
// Desc: For the given texture UV coordinates on the given mip level and slice, return the 
//       virtual address of the tile where those UV coordinates are located.
//--------------------------------------------------------------------------------------
D3DTILED_VIRTUAL_ADDRESS D3DTiledArrayTexture::GetTileVirtualAddress( UINT ArraySlice, UINT Level, FLOAT TextureU, FLOAT TextureV )
{
    CTiledResource* p = Promote( this );

    VirtualPageID VPageID = p->m_pResource->GetVirtualPageIDFloat( TextureU, TextureV, ArraySlice, Level );
    return VPageID.VirtualAddress;
}