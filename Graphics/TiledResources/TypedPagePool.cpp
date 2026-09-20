//--------------------------------------------------------------------------------------
// TypedPagePool.cpp
//
// Represents an unordered collection of physical pages in a specific texture format.
// In addition to storing the pages themselves in a large array texture, the typed page
// pool maintains lookup structures to find pages' atlas locations from a page ID, or find
// page IDs given their locations.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "TypedPagePool.h"
#include "PhysicalPageManager.h"

#include "TiledRuntimeTest.h"
using namespace TiledRuntimeTest;

namespace TiledRuntime
{
    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool constructor
    //--------------------------------------------------------------------------------------
    TypedPagePool::TypedPagePool( PhysicalPageManager* pPageManager, PageDataFormat DataFormat, UINT MaxPageCount )
    {
        m_Format = DataFormat;
        m_PageCapacity = 0;
        m_pPageManager = pPageManager;

        // Create the page pool array texture:
        CreateArrayTexture( MaxPageCount );

        // Create the physical page directories:
        CreateAtlasDirectory();

        // Create the shader constants for accessing the page pool array texture from shaders:
        CreateShaderConstants();
    }

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool destructor
    // Desc: Releases D3D objects associated with the typed page pool, and deallocates the
    //       physical page directories:
    //--------------------------------------------------------------------------------------
    TypedPagePool::~TypedPagePool()
    {
        FreeAliasedTexture( m_pAliasedPagePoolArrayTexture );
        FreeTexture( m_pPagePoolArrayTexture );

        m_PageLocationMap.clear();
        delete[] m_pAtlasDirectory;
        m_pAtlasDirectory = NULL;
        m_pFreeEntryList = NULL;
    }

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool::CreateArrayTexture
    // Desc: Creates a single array texture that will hold the physical pages, as well as
    //       room for border texels surrounding each physical page.
    //--------------------------------------------------------------------------------------
    VOID TypedPagePool::CreateArrayTexture( UINT MaxPageCount )
    {
        // Determine how many pages can be stored by the array texture:
        m_PageCapacity = XGNextMultiple( MaxPageCount, ATLAS_PAGES_PER_SLICE );
        ASSERT( m_PageCapacity > 0 );

        // Compute the array slice count:
        const DWORD SliceCount = m_PageCapacity / ATLAS_PAGES_PER_SLICE;
        ASSERT( SliceCount <= MAX_ARRAY_SLICES );
        ASSERT( SliceCount > 0 );
        m_ArraySliceCount = SliceCount;

        // Compute the page size and border sizes for the texture format:
        const SIZE PageSizeTexels = GetPageSizeTexels( m_Format );
        const UINT BorderTexelCount = GetPageBorderTexelCount( m_Format );

        // Compute the dimensions of the array texture:
        const UINT AtlasWidth = ATLAS_COLUMNS * ( PageSizeTexels.cx + BorderTexelCount * 2 );
        const UINT AtlasHeight = ATLAS_ROWS * ( PageSizeTexels.cy + BorderTexelCount * 2 );
        m_AtlasPageSizeTexels.cx = AtlasWidth;
        m_AtlasPageSizeTexels.cy = AtlasHeight;

        const D3DFORMAT ArrayTextureFormat = GetPagePoolArrayTextureFormat( m_Format );
        const D3DFORMAT AliasedFormat = GetAliasedPagePoolArrayTextureFormat( m_Format );
        CreateZeroedArrayTexture( AtlasWidth, AtlasHeight, SliceCount, 1, ArrayTextureFormat, (D3DBaseTexture**)&m_pPagePoolArrayTexture );

        switch( m_Format )
        {
        case PDF_BC1_4:
        case PDF_BC2_3_5:
            CreateAliasedArrayTexture( AtlasWidth / 4, AtlasHeight / 4, SliceCount, AliasedFormat, m_pPagePoolArrayTexture, (D3DBaseTexture**)&m_pAliasedPagePoolArrayTexture );
            break;
        default:
            m_pAliasedPagePoolArrayTexture = NULL;
            break;
        }
    }

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool::CreateAtlasDirectory
    // Desc: Creates a static directory of physical pages, one for each slot in the page
    //       pool.  The entries are in a flat array, but they are linked to each other in a
    //       linked list, so that they can be relinked in separate active/free lists.
    //--------------------------------------------------------------------------------------
    VOID TypedPagePool::CreateAtlasDirectory()
    {
        const UINT DirectorySize = m_PageCapacity;

        // Create the flat directory:
        ASSERT( DirectorySize > 0 );
        m_pAtlasDirectory = new AtlasEntry[DirectorySize];
        ZeroMemory( m_pAtlasDirectory, DirectorySize * sizeof(AtlasEntry) );

        for( DWORD i = 0; i < DirectorySize; ++i )
        {
            // Link each entry to the next entry:
            if( i < ( DirectorySize - 1 ) )
            {
                m_pAtlasDirectory[i].pNextFree = &m_pAtlasDirectory[i+1];
            }

            // Fill in the slot address of each entry:
            UINT SliceIndex = i / ATLAS_PAGES_PER_SLICE;
            UINT AtlasIndex = i % ATLAS_PAGES_PER_SLICE;
            UINT RowIndex = AtlasIndex / ATLAS_COLUMNS;
            UINT ColumnIndex = AtlasIndex % ATLAS_COLUMNS;

            m_pAtlasDirectory[i].Slice = SliceIndex;
            m_pAtlasDirectory[i].X = ColumnIndex;
            m_pAtlasDirectory[i].Y = RowIndex;
        }

        // The free list initially points to the entire directory:
        m_pFreeEntryList = &m_pAtlasDirectory[0];
    }

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool::CreateShaderConstants
    // Desc: Populates a constant buffer with information describing the size of the page
    //       pool array texture, the atlasing dimensions, and the border size relative to the
    //       page contents.
    //--------------------------------------------------------------------------------------
    VOID TypedPagePool::CreateShaderConstants()
    {
        const SIZE PageSizeTexels = GetPageSizeTexels( m_Format );
        const UINT BorderTexelCount = GetPageBorderTexelCount( m_Format );

        FLOAT TotalWidth = (FLOAT)( PageSizeTexels.cx + BorderTexelCount * 2 );
        FLOAT TotalHeight = (FLOAT)( PageSizeTexels.cy + BorderTexelCount * 2 );
        m_PageBorderUVTransform.x = (FLOAT)PageSizeTexels.cx / TotalWidth;
        m_PageBorderUVTransform.y = (FLOAT)PageSizeTexels.cy / TotalHeight;
        m_PageBorderUVTransform.z = (FLOAT)BorderTexelCount / TotalWidth;
        m_PageBorderUVTransform.w = (FLOAT)BorderTexelCount / TotalHeight;

        m_ArrayTexConstant.x = 1.0f / (FLOAT)( max( 1, m_ArraySliceCount ) );
        m_ArrayTexConstant.y = 0;
        m_ArrayTexConstant.z = 1.0f / (FLOAT)ATLAS_COLUMNS;
        m_ArrayTexConstant.w = 1.0f / (FLOAT)ATLAS_ROWS;
    }

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool::FindPage
    // Desc: Locates a page in the page pool by physical address.  This has O(log n) run time.
    //--------------------------------------------------------------------------------------
    INT TypedPagePool::FindPage( PhysicalPageID PageID ) const
    {
        PhysicalPageLocationMap::const_iterator iter = m_PageLocationMap.find( PageID );
        if( iter != m_PageLocationMap.end() )
        {
            return iter->second;
        }
        return -1;
    }

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool::AddPage
    // Desc: Finds or adds the given physical page to the typed page pool.
    //--------------------------------------------------------------------------------------
    INT TypedPagePool::AddPage( PhysicalPageID PageID )
    {
        // Check if the page already exists:
        INT Index = FindPage( PageID );
        if( Index != -1 )
        {
            return Index;
        }

        // Check if we have any free slots available:
        if( m_pFreeEntryList == NULL )
        {
            return INVALID_PAGE_POOL_INDEX;
        }

        // Grab the top entry from the free list:
        AtlasEntry* pFreeEntry = m_pFreeEntryList;
        m_pFreeEntryList = m_pFreeEntryList->pNextFree;

        // Fill in the entry:
        pFreeEntry->pNextFree = NULL;
        pFreeEntry->PageID = PageID;

        Index = GetPageIndex( pFreeEntry );

        Trace::AddPageToPool( PageID, Index, GetFormat() );

        // Add entry to the hash map using the physical address:
        m_PageLocationMap[PageID] = Index;

        return Index;
    }

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool::RemovePage
    // Desc: Removes a page from the page pool, either by index or physical page ID. By index,
    //       this method is O(1), by physical address it is O(log N).
    //--------------------------------------------------------------------------------------
    BOOL TypedPagePool::RemovePage( PhysicalPageID PageID, INT PageIndex )
    {
        // Find the page entry:
        AtlasEntry* pPageEntry = NULL;
        if( PageIndex == -1 )
        {
            PageIndex = FindPage( PageID );
            if( PageIndex != -1 )
            {
                pPageEntry = GetAtlasEntry( PageIndex );
            }
        }
        else
        {
            pPageEntry = GetAtlasEntry( PageIndex );
            ASSERT( PageID == pPageEntry->PageID );
        }

        if( pPageEntry == NULL )
        {
            return FALSE;
        }

        Trace::RemovePageFromPool( PageID, PageIndex, GetFormat() );

        // Clear the page entry:
        pPageEntry->PageID = INVALID_PHYSICAL_PAGE_ID;

        // Add the page entry to the free list:
        pPageEntry->pNextFree = m_pFreeEntryList;
        m_pFreeEntryList = pPageEntry;

        // Remove the page entry from the map:
        m_PageLocationMap.erase( PageID );

        return TRUE;
    }

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool::GetPageCount
    // Desc: Returns the number of occupied pages in this page pool.
    //--------------------------------------------------------------------------------------
    UINT TypedPagePool::GetPageCount() const
    {
        return m_PageLocationMap.size();
    }

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool::IsPagePresent
    // Desc: Returns TRUE if the given physical page is a member of this page pool, otherwise
    //       returns FALSE.
    //--------------------------------------------------------------------------------------
    BOOL TypedPagePool::IsPagePresent( PhysicalPageID PageID ) const
    {
        PhysicalPageLocationMap::const_iterator iter = m_PageLocationMap.find( PageID );
        return ( iter != m_PageLocationMap.end() );
    }

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool::GetPageIndex
    // Desc: Returns the linear index of the given page entry relative to the beginning of
    //       the directory.  It can use simple pointer subtraction since all of the entries
    //       are in the same allocation.
    //--------------------------------------------------------------------------------------
    INT TypedPagePool::GetPageIndex( AtlasEntry* pEntry ) const
    {
        ASSERT( pEntry != NULL );
        INT Index = ( pEntry - m_pAtlasDirectory );
        ASSERT( Index >= 0 && Index < (INT)m_PageCapacity );
        return Index;
    }

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool::GetAtlasEntry
    // Desc: Returns the atlas entry at the given index.
    //--------------------------------------------------------------------------------------
    AtlasEntry* TypedPagePool::GetAtlasEntry( INT Index )
    {
        ASSERT( Index >= 0 && Index < (INT)m_PageCapacity );
        return &m_pAtlasDirectory[Index];
    }

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool::GetAtlasEntry
    // Desc: Returns the atlas entry at the given index.
    //--------------------------------------------------------------------------------------
    const AtlasEntry* TypedPagePool::GetAtlasEntry( INT Index ) const
    {
        ASSERT( Index >= 0 && Index < (INT)m_PageCapacity );
        return &m_pAtlasDirectory[Index];
    }

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool::GetPageRect
    // Desc: For the given atlas entry, this method computes the atlas rectangle in texels 
    //       for that physical page, not including border texels.
    //--------------------------------------------------------------------------------------
    RECT TypedPagePool::GetPageRect( const AtlasEntry* pEntry ) const
    {
        ASSERT( pEntry != NULL );
        RECT PageRect = { 0 };

        const UINT BorderTexels = GetPageBorderTexelCount( m_Format );
        const SIZE PageSizeTexels = GetPageSizeTexels( m_Format );

        PageRect.left = ( BorderTexels * 2 + PageSizeTexels.cx ) * pEntry->X + BorderTexels;
        PageRect.top = ( BorderTexels * 2 + PageSizeTexels.cy ) * pEntry->Y + BorderTexels;
        PageRect.right = PageRect.left + PageSizeTexels.cx;
        PageRect.bottom = PageRect.top + PageSizeTexels.cy;

        return PageRect;
    }

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool::GetPageByAtlasLocation
    // Desc: Returns the atlas entry at a given atlas location (X, Y, and slice).  The atlas
    //       index is computed from the coordinates and then that index is used to look up
    //       the entry within the directory.
    //--------------------------------------------------------------------------------------
    PhysicalPageID TypedPagePool::GetPageByAtlasLocation( UINT Slice, UINT X, UINT Y ) const
    {
        UINT EncodedIndex = ( Slice * ATLAS_PAGES_PER_SLICE ) + ( Y * ATLAS_COLUMNS ) + X;
        ASSERT( EncodedIndex < m_PageCapacity );

        const AtlasEntry& Entry = m_pAtlasDirectory[EncodedIndex];
        ASSERT( Entry.X == X && Entry.Y == Y && Entry.Slice == Slice );

        return Entry.PageID;
    }

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool::SetTexture
    // Desc: Sets the page pool array texture and the samplers into the shader pipeline
    //       inputs, given the slot index.
    //--------------------------------------------------------------------------------------
    VOID TypedPagePool::SetTexture( ::D3DDevice* pd3dDevice, const D3DFORMAT ResourceFormat, UINT IndexMapSlot )
    {
        UINT SamplerIndex = 0;
        BOOL VertexShader = FALSE;
        UINT AdjustedIndexMapSlot = IndexMapSlot;

        if( IndexMapSlot >= D3DVERTEXTEXTURESAMPLER0 )
        {
            AdjustedIndexMapSlot = IndexMapSlot - D3DVERTEXTEXTURESAMPLER0;
            ASSERT( AdjustedIndexMapSlot < VS_MAX_INDEX_MAP_SLOT );
            SamplerIndex = VS_PAGEPOOL_MAP_SAMPLER_BEGIN + AdjustedIndexMapSlot;
            VertexShader = TRUE;
        }
        else
        {
            ASSERT( IndexMapSlot < MAX_INDEX_MAP_SLOT );
            SamplerIndex = PAGEPOOL_MAP_SAMPLER_BEGIN + IndexMapSlot;
        }

        CreateResourceFormatArrayTexture( m_pPagePoolArrayTexture, ResourceFormat );

        pd3dDevice->SetTexture( SamplerIndex, m_pPagePoolArrayTexture );

        CreateResourceFormatArrayTexture( m_pPagePoolArrayTexture, GetPagePoolArrayTextureFormat( m_Format ) );

        pd3dDevice->SetSamplerState( SamplerIndex, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        pd3dDevice->SetSamplerState( SamplerIndex, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        pd3dDevice->SetSamplerState( SamplerIndex, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
        pd3dDevice->SetSamplerState( SamplerIndex, D3DSAMP_SEPARATEZFILTERENABLE, TRUE );
        pd3dDevice->SetSamplerState( SamplerIndex, D3DSAMP_MAGFILTERZ, D3DTEXF_POINT );
        pd3dDevice->SetSamplerState( SamplerIndex, D3DSAMP_MINFILTERZ, D3DTEXF_POINT );
        pd3dDevice->SetSamplerState( SamplerIndex, D3DSAMP_MIPMAPLODBIAS, 0 );
        pd3dDevice->SetSamplerState( SamplerIndex, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
        pd3dDevice->SetSamplerState( SamplerIndex, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
        pd3dDevice->SetSamplerState( SamplerIndex, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP );

        if( VertexShader )
        {
            pd3dDevice->SetVertexShaderConstantF( PAGE_POOL_CONSTANT_BEGIN + AdjustedIndexMapSlot, (FLOAT*)&m_PageBorderUVTransform, 1 );
            pd3dDevice->SetVertexShaderConstantF( ARRAY_POOL_CONSTANT_BEGIN + AdjustedIndexMapSlot, (FLOAT*)&m_ArrayTexConstant, 1 );

        }
        else
        {
            pd3dDevice->SetPixelShaderConstantF( PAGE_POOL_CONSTANT_BEGIN + IndexMapSlot, (FLOAT*)&m_PageBorderUVTransform, 1 );
            pd3dDevice->SetPixelShaderConstantF( ARRAY_POOL_CONSTANT_BEGIN + IndexMapSlot, (FLOAT*)&m_ArrayTexConstant, 1 );
        }
    }

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool::CreateResourceFormatArrayTexture
    // Desc: Modifies the given array texture header to have the given texture format, including
    //       details such as tiling, endianness, and swizzle.
    //--------------------------------------------------------------------------------------
    VOID TypedPagePool::CreateResourceFormatArrayTexture( D3DArrayTexture* pTextureHeader, const D3DFORMAT ResourceFormat )
    {
        // Create a dummy header in the desired format, but with known bad sizes:
        D3DArrayTexture FormatHeader;
        XGSetArrayTextureHeader( 1, 1, 2, 1, 0, ResourceFormat, D3DPOOL_DEFAULT, 0, 0, 0, &FormatHeader, NULL, NULL );

        // Copy all of the format-related members from the dummy header to the given header:
        pTextureHeader->Format.Endian = FormatHeader.Format.Endian;
        pTextureHeader->Format.DataFormat = FormatHeader.Format.DataFormat;
        pTextureHeader->Format.NumFormat = FormatHeader.Format.NumFormat;
        pTextureHeader->Format.RequestSize = FormatHeader.Format.RequestSize;
        pTextureHeader->Format.SignX = FormatHeader.Format.SignX;
        pTextureHeader->Format.SignY = FormatHeader.Format.SignY;
        pTextureHeader->Format.SignZ = FormatHeader.Format.SignZ;
        pTextureHeader->Format.SignW = FormatHeader.Format.SignW;
        pTextureHeader->Format.SwizzleX = FormatHeader.Format.SwizzleX;
        pTextureHeader->Format.SwizzleY = FormatHeader.Format.SwizzleY;
        pTextureHeader->Format.SwizzleZ = FormatHeader.Format.SwizzleZ;
        pTextureHeader->Format.SwizzleW = FormatHeader.Format.SwizzleW;
        pTextureHeader->Format.Tiled = FormatHeader.Format.Tiled;
    }

    //--------------------------------------------------------------------------------------
    // Name: TypedPagePool::GetMemoryUsage
    // Desc: Fills in a memory usage struct with details about the capacity of this page pool
    //       and the amount of contents within.
    //--------------------------------------------------------------------------------------
    VOID TypedPagePool::GetMemoryUsage( D3DTILED_MEMORY_USAGE* pMemoryUsage ) const
    {
        ASSERT( pMemoryUsage != NULL );

        pMemoryUsage->FormatPoolsActive++;

        pMemoryUsage->TileCapacity += m_PageCapacity;
        pMemoryUsage->TilesAllocated += m_PageLocationMap.size();

        UINT BaseSize, MipSize;
        XGGetTextureLayout( m_pPagePoolArrayTexture, NULL, &BaseSize, NULL, NULL, 0, NULL, &MipSize, NULL, NULL, 0 );
        pMemoryUsage->TileTextureMemoryBytesAllocated += ( BaseSize + MipSize );

        UINT AtlasDirectorySizeBytes = m_PageCapacity * sizeof(AtlasEntry);
        pMemoryUsage->OverheadMemoryBytesAllocated += AtlasDirectorySizeBytes;

        pMemoryUsage->OverheadMemoryBytesAllocated += m_PageLocationMap.size() * ( sizeof(INT) + sizeof(PhysicalPageID) );
    }
}
