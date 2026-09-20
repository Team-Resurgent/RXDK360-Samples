//--------------------------------------------------------------------------------------
// TypedPagePool.h
//
// Represents an unordered collection of physical pages in a specific texture format.
// In addition to storing the pages themselves in a large array texture, the typed page
// pool maintains lookup structures to find pages' atlas locations from a page ID, or find
// page IDs given their locations.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "TiledResourceCommon.h"
#include "d3d9tiled.h"

namespace TiledRuntime
{
    //--------------------------------------------------------------------------------------
    // Name: AtlasEntry
    // Desc: Represents a physical page and its location within the page pool array texture.
    //--------------------------------------------------------------------------------------
    struct AtlasEntry
    {
        struct
        {
            DWORD X: 8;
            DWORD Y: 8;
            DWORD Slice: 16;
        };
        PhysicalPageID PageID;
        AtlasEntry* pNextFree;
    };

    typedef std::hash_map<PhysicalPageID, INT> PhysicalPageLocationMap;

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool
    // Desc: Class that represents a collection of physical pages sharing a common texture
    //       format.
    //--------------------------------------------------------------------------------------
    class TypedPagePool
    {
    protected:
        PhysicalPageManager* m_pPageManager;

        // The format of the typed page pool and its contents:
        PageDataFormat m_Format;

        // The number of array slices in the page pool array texture:
        UINT m_ArraySliceCount;

        // The total number of physical pages that this pool can contain:
        UINT m_PageCapacity;

        // The directory of physical pages, indexable by location within
        // the array texture:
        AtlasEntry* m_pAtlasDirectory;

        // The free entry list, so that we can locate an empty slot in the
        // page pool with O(1) efficiency:
        AtlasEntry* m_pFreeEntryList;

        // The directory of physical pages, indexable by physical address:
        PhysicalPageLocationMap m_PageLocationMap;

        // The array texture that stores physical pages as a stack of atlased pages:
        D3DArrayTexture* m_pPagePoolArrayTexture;
        D3DArrayTexture* m_pAliasedPagePoolArrayTexture;
        // The atlas texture dimensions in texels
        SIZE m_AtlasPageSizeTexels;

        XMFLOAT4 m_PageBorderUVTransform;
        XMFLOAT4 m_ArrayTexConstant;

    public:
        TypedPagePool( PhysicalPageManager* pPageManager, PageDataFormat DataFormat, UINT MaxPageCount );
        ~TypedPagePool();

        PageDataFormat GetFormat() const { return m_Format; }

        INT FindPage( PhysicalPageID PageID ) const;
        INT AddPage( PhysicalPageID PageID );
        BOOL RemovePage( PhysicalPageID PageID, INT PageIndex = -1 );
        UINT GetPageCount() const;
        BOOL IsPagePresent( PhysicalPageID PageID ) const;
        PhysicalPageID GetPageByAtlasLocation( UINT Slice, UINT X, UINT Y ) const;

        AtlasEntry* GetAtlasEntry( INT Index );
        const AtlasEntry* GetAtlasEntry( INT Index ) const;

        VOID SetTexture( ::D3DDevice* pd3dDevice, const D3DFORMAT ResourceFormat, UINT IndexMapSlot );

        RECT GetPageRect( const AtlasEntry* pEntry ) const;

        D3DArrayTexture* GetArrayTexture() const { return m_pPagePoolArrayTexture; }
        D3DArrayTexture* GetAliasedArrayTexture() const { return ( m_pAliasedPagePoolArrayTexture != NULL ) ? m_pAliasedPagePoolArrayTexture : m_pPagePoolArrayTexture; }
        SIZE GetAtlasPageSizeTexels() const { return m_AtlasPageSizeTexels; }

        VOID GetMemoryUsage( D3DTILED_MEMORY_USAGE* pMemoryUsage ) const;

    protected:
        VOID CreateArrayTexture( UINT MaxPageCount );
        VOID CreateAtlasDirectory();
        VOID CreateShaderConstants();

        INT GetPageIndex( AtlasEntry* pEntry ) const;

        VOID CreateResourceFormatArrayTexture( D3DArrayTexture* pTextureHeader, const D3DFORMAT ResourceFormat );
    };
}

