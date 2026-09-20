//--------------------------------------------------------------------------------------
// TiledResourceBase.h
//
// This class represents a tiled resource within the tiled resource runtime.  Each
// tiled resource manages an index map texture that contains mappings from texture UV space
// (virtual addresses) to indices within a typed page pool (physical addresses).
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "TiledResourceCommon.h"
#include "d3d9tiled.h"

namespace TiledRuntime
{
    //--------------------------------------------------------------------------------------
    // Name: CPUTexture
    // Desc: A class that holds untyped texture data in raw buffers, without using Direct3D.  
    //       Mipmaps and array slices are supported.
    //--------------------------------------------------------------------------------------
    class CPUTexture
    {
        //--------------------------------------------------------------------------------------
        // Name: SubresourceDesc
        // Desc: Describes one subresource within the CPUTexture.
        //--------------------------------------------------------------------------------------
        struct SubresourceDesc
        {
            BYTE* pBase;
            UINT RowPitchBytes;
            UINT Height;
        };
    protected:
        // The entire texture is packed into one allocation:
        VOID* m_pAllocation;

        // An array of subresources subdivide the allocation:
        SubresourceDesc* m_pSubresources;

        // Texture array size:
        UINT m_ArraySize;

        // Texture mip level count:
        UINT m_MipLevels;

        // Bytes per pixel:
        UINT m_BytesPerPixel;
        UINT m_MemoryUsage;

    public:
        CPUTexture();
        ~CPUTexture();

        VOID Initialize( UINT Width, UINT Height, UINT ArraySize, UINT Levels, UINT BytesPerPixel );
        VOID CPUMap( UINT SubresourceIndex, D3DLOCKED_RECT* pMappedSubresource, UINT* pSubresourceHeight = NULL );

        UINT GetMemoryUsage() const { return m_MemoryUsage; }
    };

    //--------------------------------------------------------------------------------------
    // Name: InternalSurfaceDesc
    // Desc: Describes the layout of a tiled texture surface, where the surface dimension in
    //       pages may exceed the texel dimensions due to page alignment.
    //--------------------------------------------------------------------------------------
    struct InternalSurfaceDesc : public D3DTILED_SURFACE_DESC
    {
        // Width and height of the mip level's addressable virtual address space, in pages:
        UINT AddressablePageWidth;
        UINT AddressablePageHeight;
    };

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase
    // Desc: Base class for a single tiled resource.  All of the tiled resource functionality
    //       is in this class, including maintentance of CPU and GPU copies of the index map,
    //       conversion between UV/texel coordinates and virtual addresses, and setting the
    //       resource into the D3D device context.
    //--------------------------------------------------------------------------------------
    class TiledResourceBase
    {
    protected:
        friend class PhysicalPageManager;

        // The resource ID that was assigned to this resource by the physical page manager:
        UINT m_ResourceID;

        // The physical page manager that is tracking this resource:
        PhysicalPageManager* m_pPageManager;

        PageDataFormat m_DataFormat;
        TypedPagePool* m_pTypedPagePool;

        // The size of the base level, in texels:
        SIZE m_BaseLevelSizeTexels;

        D3DBaseTexture* m_pIndexMapGPU;

        // The CPU-only respresentation of the index map texture.
        // This copy is in sync with CPU threads, for querying and updating.
        CPUTexture m_IndexMapCPU;


        // Format of this resource:
        D3DFORMAT m_ResourceFormat;

        // Mipmap LOD bias that converts the virtual texture dimensions to the dimensions of the index map texture:
        FLOAT m_fMipLODBias;

        // Shader constants for the tiled resource:
        XMFLOAT4 m_LODConstants[9];
        XMFLOAT4 m_ResourceConstant;


        // Number of array slices:
        UINT m_ArraySliceCount;

        // Quilting width and height:
        UINT m_QuiltWidth;
        UINT m_QuiltHeight;

        // Number of mip levels:
        UINT m_MipLevelCount;

        // Surface desc for each mip level:
        InternalSurfaceDesc m_MipLevelDesc[9];

    public:
        TiledResourceBase();
        ~TiledResourceBase();

        VOID SetTexture( ::D3DDevice* pd3dDevice, UINT IndexMapSlot );

        HRESULT Initialize( PhysicalPageManager* pPageManager, UINT Width, UINT Height, UINT MipLevelCount, UINT ArraySize, D3DFORMAT ResourceFormat );
        HRESULT SetQuilted( UINT QuiltWidth, UINT QuiltHeight );

        VirtualPageID GetVirtualPageIDFloat( FLOAT U, FLOAT V, UINT SliceIndex, UINT MipLevel ) const;
        VirtualPageID GetVirtualPageIDTexel( UINT TexelX, UINT TexelY, UINT SliceIndex, UINT MipLevel ) const;
        VirtualPageID GetVirtualPageIDPage( UINT PageX, UINT PageY, UINT SliceIndex, UINT MipLevel ) const;

        HRESULT SetCPUIndexMapEntry( VirtualPageID VPageID, PhysicalPageID PageID, INT PagePoolIndex );

        HRESULT GetNeighborhood( VirtualPageID CenterPage, PageNeighborhood* pNeighborhood );

        BOOL IsTexture2D() const;
        BOOL IsTexture2DArray() const;
        BOOL IsBuffer() const;
        BOOL IsQuilted() const { return m_QuiltWidth > 1 || m_QuiltHeight > 1; }
        PageDataFormat GetDataFormat() const { return m_DataFormat; }
        TypedPagePool* GetTypedPagePool() const { return m_pTypedPagePool; }
        D3DBaseTexture* GetIndexMapGPUTexture() const { return m_pIndexMapGPU; }
        UINT GetResourceID() const { return m_ResourceID; }

        UINT GetMipLevelCount() const { return m_MipLevelCount; }
        UINT GetArraySliceCount() const { return m_ArraySliceCount; }
        UINT GetQuiltWidth() const { return m_QuiltWidth; }
        UINT GetQuiltHeight() const { return m_QuiltHeight; }
        VOID GetLevelDesc( UINT MipLevel, D3DTILED_SURFACE_DESC* pDesc ) const;

        UINT ConvertQuiltUVToArrayUVW( FLOAT* pU, FLOAT* pV ) const;

        VOID GetMemoryUsage( D3DTILED_MEMORY_USAGE* pMemoryUsage ) const;

    protected:
        VOID ComputeLevelDesc( UINT MipLevel );
        FLOAT ComputeIndexMapLODBias( UINT TexWidth, UINT NumPageWidth, UINT TexHeight, UINT NumPageHeight ) const;
        VOID CreateLODShaderConstants( UINT PageWidthTexels, UINT PageHeightTexels, UINT TextureWidthPixels, UINT TextureHeightPixels );
        HRESULT GetQuiltNeighborhood( VirtualPageID CenterPage, PageNeighborhood* pNeighborhood );
    };

    class TiledTexture : public TiledResourceBase
    {

    };

    class TiledArrayTexture : public TiledResourceBase
    {

    };

    class TiledBuffer : public TiledResourceBase
    {

    };
}
