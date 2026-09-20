//--------------------------------------------------------------------------------------
// TiledResourceBase.cpp
//
// This class represents a tiled resource within the tiled resource runtime.  Each
// tiled resource manages an index map texture that contains mappings from texture UV space
// (virtual addresses) to indices within a typed page pool (physical addresses).
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "TiledResourceBase.h"
#include "PhysicalPageManager.h"
#include "TypedPagePool.h"
#include "PageRenderer.h"

#include "TiledRuntimeTest.h"
using namespace TiledRuntimeTest;

namespace TiledRuntime
{
    //--------------------------------------------------------------------------------------
    // Name: CPUTexture constructor
    //--------------------------------------------------------------------------------------
    CPUTexture::CPUTexture()
    {
        m_pAllocation = NULL;
        m_pSubresources = NULL;
        m_ArraySize = 0;
        m_MipLevels = 0;
        m_MemoryUsage = 0;
    }

    //--------------------------------------------------------------------------------------
    // Name: CPUTexture destructor
    //--------------------------------------------------------------------------------------
    CPUTexture::~CPUTexture()
    {
        if( m_pAllocation != NULL )
        {
            delete[] m_pAllocation;
        }
        if( m_pSubresources != NULL )
        {
            delete[] m_pSubresources;
        }
    }

    //--------------------------------------------------------------------------------------
    // Name: CPUTexture::Initialize
    // Desc: Creates the memory allocation and subresource information for a CPU texture.
    //--------------------------------------------------------------------------------------
    VOID CPUTexture::Initialize( UINT Width, UINT Height, UINT ArraySize, UINT Levels, UINT BytesPerPixel )
    {
        m_BytesPerPixel = BytesPerPixel;

        if( Levels == 0 )
        {
            UINT TestWidth = Width;
            UINT TestHeight = Height;
            while( TestWidth > 1 || TestHeight > 1 )
            {
                ++Levels;
                TestWidth = max( 1, TestWidth / 2 );
                TestHeight = max( 1, TestHeight / 2 );
            }
            ++Levels;
        }

        m_ArraySize = ArraySize;
        m_MipLevels = Levels;

        // Create subresource desc array:
        const UINT SubresourceCount = Levels * ArraySize;
        m_pSubresources = new SubresourceDesc[SubresourceCount];

        // Determine the size and offset of each subresource:
        UINT CurrentMipOffset = 0;
        for( UINT i = 0; i < Levels; ++i )
        {
            m_pSubresources[i].RowPitchBytes = Width * BytesPerPixel;
            m_pSubresources[i].Height = Height;
            m_pSubresources[i].pBase = (BYTE*)CurrentMipOffset;

            CurrentMipOffset += ( Width * Height * BytesPerPixel );

            if( Width <= 1 && Height <= 1 )
            {
                break;
            }

            Width = max( 1, Width / 2 );
            Height = max( 1, Height / 2 );
        }

        // The CurrentMipOffset variable now contains the size of one mip chain:
        const UINT MipChainSizeBytes = CurrentMipOffset;

        // Copy the first mip chain's data to the other array slices, if they exist:
        for( UINT i = 1; i < ArraySize; ++i )
        {
            for( UINT j = 0; j < Levels; ++j )
            {
                UINT SubresourceIndex = i * Levels + j;
                m_pSubresources[SubresourceIndex].pBase = m_pSubresources[j].pBase + ( MipChainSizeBytes * i );
                m_pSubresources[SubresourceIndex].RowPitchBytes = m_pSubresources[j].RowPitchBytes;
                m_pSubresources[SubresourceIndex].Height = m_pSubresources[j].Height;
            }
        }

        UINT AllocBytes = MipChainSizeBytes * ArraySize;
        BYTE* pAllocation = new BYTE[AllocBytes];

        //ZeroMemory( pAllocation, AllocBytes );
        memset( pAllocation, 0xFF, AllocBytes );

        for( UINT i = 0; i < SubresourceCount; ++i )
        {
            m_pSubresources[i].pBase += (size_t)pAllocation;
        }
        m_pAllocation = pAllocation;

        m_MemoryUsage = AllocBytes;
        m_MemoryUsage += SubresourceCount * sizeof(SubresourceDesc);
        m_MemoryUsage += sizeof(CPUTexture);
    }

    //--------------------------------------------------------------------------------------
    // Name: CPUTexture::CPUMap
    // Desc: Returns a pointer to a subresource, in a D3DLOCKED_RECT struct. Note
    //       that there is no Unmap function, since we do not need to hold a lock on a CPU
    //       texture.
    //--------------------------------------------------------------------------------------
    VOID CPUTexture::CPUMap( UINT SubresourceIndex, D3DLOCKED_RECT* pMappedSubresource, UINT* pSubresourceHeight )
    {
        ASSERT( SubresourceIndex < m_ArraySize * m_MipLevels );

        // Fill in the data pointer and row pitch members:
        pMappedSubresource->pBits = m_pSubresources[SubresourceIndex].pBase;
        pMappedSubresource->Pitch = m_pSubresources[SubresourceIndex].RowPitchBytes;

        if( pSubresourceHeight != NULL )
        {
            *pSubresourceHeight = m_pSubresources[SubresourceIndex].Height;
        }
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase constructor
    //--------------------------------------------------------------------------------------
    TiledResourceBase::TiledResourceBase()
    {
        m_ResourceID = 0;
        m_pPageManager = NULL;

        m_pIndexMapGPU = NULL;
        m_pTypedPagePool = NULL;
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase destructor
    //--------------------------------------------------------------------------------------
    TiledResourceBase::~TiledResourceBase()
    {
        FreeTexture( m_pIndexMapGPU );
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::SetTexture
    // Desc: Sets the tiled resource into the given "slot" on a real D3D device context.
    //       Each slot is a pair of textures - one for the index map texture, and one for
    //       the typed page pool's physical page array texture.  A pair of sampler states is
    //       also set (one for the resource, one for the typed page pool).
    //--------------------------------------------------------------------------------------
    VOID TiledResourceBase::SetTexture( ::D3DDevice* pd3dDevice, UINT IndexMapSlot )
    {
        UINT SamplerIndex = 0;
        BOOL VertexShader = FALSE;
        UINT AdjustedIndexMapSlot = IndexMapSlot;

        if( IndexMapSlot >= D3DVERTEXTEXTURESAMPLER0 )
        {
            AdjustedIndexMapSlot = IndexMapSlot - D3DVERTEXTEXTURESAMPLER0;
            ASSERT( AdjustedIndexMapSlot < VS_MAX_INDEX_MAP_SLOT );
            SamplerIndex = VS_INDEX_MAP_SAMPLER_BEGIN + AdjustedIndexMapSlot;
            VertexShader = TRUE;
        }
        else
        {
            ASSERT( IndexMapSlot < MAX_INDEX_MAP_SLOT );
            SamplerIndex = INDEX_MAP_SAMPLER_BEGIN + IndexMapSlot;
        }

        // Set the index map texture sampler state.
        pd3dDevice->SetTexture( SamplerIndex, m_pIndexMapGPU );
        pd3dDevice->SetSamplerState( SamplerIndex, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        pd3dDevice->SetSamplerState( SamplerIndex, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        pd3dDevice->SetSamplerState( SamplerIndex, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
        pd3dDevice->SetSamplerState( SamplerIndex, D3DSAMP_SEPARATEZFILTERENABLE, TRUE );
        pd3dDevice->SetSamplerState( SamplerIndex, D3DSAMP_MINFILTERZ, D3DTEXF_POINT );
        pd3dDevice->SetSamplerState( SamplerIndex, D3DSAMP_MAGFILTERZ, D3DTEXF_POINT );

        // LOD bias for index map will be computed manually within the shader, and we don't want to add any additional bias.
        pd3dDevice->SetSamplerState( SamplerIndex, D3DSAMP_MIPMAPLODBIAS, 0 );

        if( VertexShader )
        {
            pd3dDevice->SetVertexShaderConstantF( LOD_RESOURCE_CONSTANT_BEGIN + AdjustedIndexMapSlot * ARRAYSIZE(m_LODConstants), (FLOAT*)m_LODConstants, ARRAYSIZE(m_LODConstants) );
            pd3dDevice->SetVertexShaderConstantF( MISC_RESOURCE_CONSTANT_BEGIN + AdjustedIndexMapSlot, (FLOAT*)&m_ResourceConstant, 1 );
        }
        else
        {
            pd3dDevice->SetPixelShaderConstantF( LOD_RESOURCE_CONSTANT_BEGIN + IndexMapSlot * ARRAYSIZE(m_LODConstants), (FLOAT*)m_LODConstants, ARRAYSIZE(m_LODConstants) );
            pd3dDevice->SetPixelShaderConstantF( MISC_RESOURCE_CONSTANT_BEGIN + IndexMapSlot, (FLOAT*)&m_ResourceConstant, 1 );
        }

        // Call the typed page pool to set itself to the D3D device on this slot.
        m_pTypedPagePool->SetTexture( pd3dDevice, m_ResourceFormat, IndexMapSlot );
    }

    //--------------------------------------------------------------------------------------
    // Name: IndexMapDimension
    // Desc: For a given texel size and page size, compute the 1D index map dimension to allow
    //       for a full mip chain.  This is not as easy as dividing the texel size by the page
    //       size; due to integer rounding, the base level might have to be expanded to ensure
    //       that higher mip levels are allocated enough pages to completely cover the required
    //       texel count for those levels.
    //       The return value is the size of the index map's base dimension, in pages.
    //--------------------------------------------------------------------------------------
    UINT IndexMapDimension( const UINT TexelSize, const UINT PageSize )
    {
        ASSERT( TexelSize <= 16384 );
        UINT MipSizes[14] = { 0 };
        UINT PageCounts[14] = { 0 };

        // Compute the lowest mip index for the given texel size.
        // The lowest mip index is the lowest mip LOD where the texel size is less than or
        // equal to the page size.  This value will be the size of the mip chain:
        INT LowestMipIndex = ARRAYSIZE(MipSizes);
        UINT Size = TexelSize;
        for( UINT i = 0; i < ARRAYSIZE(MipSizes); ++i )
        {
            // Store each mip level's size in texels:
            MipSizes[i] = Size;

            // Break out of the loop if the mip size fits within a single page:
            if( MipSizes[i] <= PageSize && (INT)i < LowestMipIndex )
            {
                LowestMipIndex = (INT)i;
                break;
            }

            Size = max( 1, Size / 2 );
        }
        ASSERT( LowestMipIndex < ARRAYSIZE(MipSizes) );

        // Initialize the lowest mip index as one page in size:
        PageCounts[LowestMipIndex] = 1;

        // If there is only one mip level, return now:
        if( LowestMipIndex == 0 )
        {
            return 1;
        }

        // Walk from the smallest mip level up to the base level, doubling
        // the page count each step.  If the doubled size isn't sufficient to
        // cover the texel size for that mip level, add one to the page count.
        // This is allowed, because odd mip dimensions will be rounded down
        // when divided by 2:
        for( INT i = LowestMipIndex - 1; i >= 0; --i )
        {
            // Double the page count:
            PageCounts[i] = PageCounts[ i + 1 ] * 2;

            // If the page count multiplied by the page size is less than the texel size for this mip level,
            // add one to the page count:
            if( ( PageCounts[i] * PageSize ) < MipSizes[i] )
            {
                PageCounts[i] += 1;
            }

            // Double check that the adjusted page count is sufficient to cover the texel size of this level:
            ASSERT( ( PageCounts[i] * PageSize ) >= MipSizes[i] );
        }

        // Return the base level's page count:
        ASSERT( PageCounts[0] >= 1 );
        return PageCounts[0];
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::Initialize
    // Desc: Initializes a texture 2D or texture 2D array tiled resource.
    //--------------------------------------------------------------------------------------
    HRESULT TiledResourceBase::Initialize( PhysicalPageManager* pPageManager, UINT Width, UINT Height, UINT MipLevelCount, UINT ArraySize, D3DFORMAT ResourceFormat )
    {
        PageDataFormat DataFormat = GetPageDataFormat( ResourceFormat );
        if( DataFormat == PDF_INVALID )
        {
            return E_FAIL;
        }

        m_DataFormat = DataFormat;
        m_ResourceFormat = ResourceFormat;

        switch( m_DataFormat )
        {
        case PDF_BC2_3_5:
        case PDF_128BPP:
            m_ResourceFormat = (D3DFORMAT)MAKELINFMT( m_ResourceFormat );
            break;
        }

        // Store the base level texel dimensions:
        m_BaseLevelSizeTexels.cx = Width;
        m_BaseLevelSizeTexels.cy = Height;

        // Get the page size in texels of this format:
        const SIZE PageSizeTexels = GetPageSizeTexels( DataFormat );

        // Compute the index map base level width and height:
        const UINT IndexMapWidth = IndexMapDimension( Width, PageSizeTexels.cx );
        const UINT IndexMapHeight = IndexMapDimension( Height, PageSizeTexels.cy );

        if( ArraySize > 1 )
        {
            // Create the GPU index map texture array:
            CreateZeroedArrayTexture( IndexMapWidth, IndexMapHeight, ArraySize, MipLevelCount, D3DFMT_INDEXMAP, &m_pIndexMapGPU );
            m_ArraySliceCount = ArraySize;
        }
        else
        {
            // Create the GPU index map texture:
            CreateZeroedTexture2D( IndexMapWidth, IndexMapHeight, MipLevelCount, D3DFMT_INDEXMAP, &m_pIndexMapGPU );

            // Store 1 for the array size:
            m_ArraySliceCount = 1;
        }

        m_QuiltWidth = 1;
        m_QuiltHeight = 1;

        // Create the CPU index map texture:
        m_IndexMapCPU.Initialize( IndexMapWidth, IndexMapHeight, ArraySize, MipLevelCount, INDEXMAP_TEXEL_SIZE_BYTES );

        // Store the mip level count:
        m_MipLevelCount = m_pIndexMapGPU->GetLevelCount();

        // Precompute the level descs (they are expensive to compute, and will be accessed frequently at runtime):
        for( UINT i = 0; i < m_MipLevelCount; ++i )
        {
            ComputeLevelDesc( i );
        }

        // Compute the mip LOD bias that causes sampling on the tiled texture texel dimensions to map to the index map dimensions:
        m_fMipLODBias = max( log2f( (UINT)PageSizeTexels.cx ), log2f( (UINT)PageSizeTexels.cy ) );

        // Create shader constants for sampling from the index map texture:
        CreateLODShaderConstants( PageSizeTexels.cx, PageSizeTexels.cy, Width, Height );


        // Register this resource with the physical page manager:
        pPageManager->RegisterResource( this );

        Trace::CreateTexture2D( m_ResourceID, Width, Height, ResourceFormat );

        return S_OK;
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::SetQuilted
    // Desc: Initializes quilting constants for a tiled texture2D array that is being
    //       interpreted as a rectangular texture quilt.
    //--------------------------------------------------------------------------------------
    HRESULT TiledResourceBase::SetQuilted( UINT QuiltWidth, UINT QuiltHeight )
    {
        if( !IsTexture2DArray() )
        {
            return E_FAIL;
        }

        // Compute the expected array size from the quilt width and height:
        UINT ArraySize = QuiltWidth * QuiltHeight;

        // Make sure the array size matches the quilt size:
        if( ArraySize != GetArraySliceCount() )
        {
            return E_FAIL;
        }

        // Store the quilt width and height:
        m_QuiltWidth = QuiltWidth;
        m_QuiltHeight = QuiltHeight;

        // Add the quilt width and height to the resource constants for use in shading:
        m_ResourceConstant.z = (FLOAT)m_QuiltWidth;
        m_ResourceConstant.w = (FLOAT)m_QuiltHeight;

        return S_OK;
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::ComputeIndexMapLODBias
    // Desc: Computes a mip map LOD bias from the difference between the texel dimensions
    //       and the index map dimensions.
    //--------------------------------------------------------------------------------------
    FLOAT TiledResourceBase::ComputeIndexMapLODBias( UINT TexWidth, UINT NumPageWidth, UINT TexHeight, UINT NumPageHeight ) const
    {
        // Compute the base 2 log difference between the texel width and the index map width:
        FLOAT WidthLevels = log2f( TexWidth );
        FLOAT PageWidthLevels = log2f( NumPageWidth );
        FLOAT MipBiasWidth = PageWidthLevels - WidthLevels;

        // Compute the base 2 log difference between the texel height and the index map height:
        FLOAT HeightLevels = log2f( TexHeight );
        FLOAT PageHeightLevels = log2f( NumPageHeight );
        FLOAT MipBiasHeight = PageHeightLevels - HeightLevels;

        // Return the negated average of the width and height log difference.  The value must
        // be negative because we are making the texture sample softer (instead of sampling texels,
        // we are sampling pages, which are much larger than texels):
        return ( MipBiasWidth + MipBiasHeight ) * -0.5f;
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::CreateLODShaderConstants
    // Desc: Creates an array of shader constants that are used to map UV space coordinates
    //       to page coordinates for each mip level of a texture2D.  This isn't always a
    //       trivial computation, because the dimensions of the index map may be larger than
    //       the page dimensions of the tiled resource for a given mip level.
    //--------------------------------------------------------------------------------------
    VOID TiledResourceBase::CreateLODShaderConstants( UINT PageWidthTexels, UINT PageHeightTexels, UINT TextureWidthPixels, UINT TextureHeightPixels )
    {
        const INT MaxLevels = ARRAYSIZE(m_LODConstants);

        // Determine the index of the max mip level in the texture:
        INT EndLevel = GetMipLevelCount() - 1;
        assert( EndLevel < MaxLevels );

        for( INT i = 0; i < MaxLevels; ++i )
        {
            XGTEXTURE_DESC LevelDesc;
            if( i <= EndLevel )
            {
                XGGetTextureDesc( m_pIndexMapGPU, i, &LevelDesc );
            }
            else
            {
                LevelDesc.Width = 1;
                LevelDesc.Height = 1;
            }

            // Compute the dimensions of the index map in texels, given the page size:
            UINT IndexMapWidth = LevelDesc.Width * PageWidthTexels;
            UINT IndexMapHeight = LevelDesc.Height * PageHeightTexels;

            // Compute the dimensions of a single page in UV space:
            FLOAT PageSizeU = (FLOAT)PageWidthTexels / (FLOAT)TextureWidthPixels;
            FLOAT PageSizeV = (FLOAT)PageHeightTexels / (FLOAT)TextureHeightPixels;

            // Store the reciprocal of the page UV dimensions:
            m_LODConstants[i].x = 1.0f / PageSizeU;
            m_LODConstants[i].y = 1.0f / PageSizeV;

            // Compute the scaling factor between the tiled texture texel dimensions and the index map texel dimensions.
            // The index map texel dimensions are sometimes larger than the tiled texture texel dimensions:
            m_LODConstants[i].z = (FLOAT)TextureWidthPixels / (FLOAT)IndexMapWidth;
            m_LODConstants[i].w = (FLOAT)TextureHeightPixels / (FLOAT)IndexMapHeight;

            // Divide the tiled texture texel dimensions by 2:
            if( i < EndLevel )
            {
                TextureWidthPixels = max( 1, TextureWidthPixels / 2 );
                TextureHeightPixels = max( 1, TextureHeightPixels / 2 );
            }
        }

        // Store additional data for sampling, including the mip LOD bias and the inverse array slice count:
        m_ResourceConstant = XMFLOAT4( m_fMipLODBias, 1.0f / (FLOAT)m_ArraySliceCount, (FLOAT)m_QuiltWidth, (FLOAT)m_QuiltHeight );
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::IsTexture2D
    // Desc: Returns TRUE if this resource is a texture 2D (not an array).
    //--------------------------------------------------------------------------------------
    BOOL TiledResourceBase::IsTexture2D() const
    {
        ASSERT( m_pIndexMapGPU != NULL );
        BOOL Texture2D = m_pIndexMapGPU->Format.Dimension == GPUDIMENSION_2D;
        BOOL OneSlice = !m_pIndexMapGPU->Format.Stacked;

        return Texture2D && OneSlice;
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::IsTexture2DArray
    // Desc: Returns TRUE if this resource is a texture 2D with more than one array slice.
    //--------------------------------------------------------------------------------------
    BOOL TiledResourceBase::IsTexture2DArray() const
    {
        ASSERT( m_pIndexMapGPU != NULL );
        BOOL Texture2D = m_pIndexMapGPU->Format.Dimension == GPUDIMENSION_2D;
        BOOL Stacked = m_pIndexMapGPU->Format.Stacked;

        return Texture2D && Stacked;
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::IsBuffer
    // Desc: Returns TRUE if this resource is a buffer.
    //--------------------------------------------------------------------------------------
    BOOL TiledResourceBase::IsBuffer() const
    {
        NOTIMPL;
        return FALSE;
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::ComputeLevelDesc
    // Desc: Fills in a mip level desc struct for the given level.
    //--------------------------------------------------------------------------------------
    VOID TiledResourceBase::ComputeLevelDesc( UINT MipLevel )
    {
        ASSERT( MipLevel < GetMipLevelCount() );

        // Get a pointer to the level desc:
        InternalSurfaceDesc* pDesc = &m_MipLevelDesc[MipLevel];

        // Store the data format:
        pDesc->Format = m_ResourceFormat;

        // Store a single page's dimensions in texels:
        const SIZE PageSizeTexels = GetPageSizeTexels( m_DataFormat );
        pDesc->TileTexelWidth = PageSizeTexels.cx;
        pDesc->TileTexelHeight = PageSizeTexels.cy;

        // Get the dimensions of this mip level of the index map GPU texture:
        XGTEXTURE_DESC IndexMapDesc;
        XGGetTextureDesc( m_pIndexMapGPU, MipLevel, &IndexMapDesc );


        // The addressable dimensions are the index map dimensions at this level:
        pDesc->AddressablePageWidth = IndexMapDesc.Width;
        pDesc->AddressablePageHeight = IndexMapDesc.Height;

        // Compute the tiled texture texel dimensions:
        UINT Pow2 = 1 << MipLevel;
        FLOAT BaseSizeMultiple = 1.0f / (FLOAT)Pow2;
        pDesc->TexelWidth = max( 1, (UINT)( (FLOAT)m_BaseLevelSizeTexels.cx * BaseSizeMultiple ) );
        pDesc->TexelHeight = max( 1, (UINT)( (FLOAT)m_BaseLevelSizeTexels.cy * BaseSizeMultiple ) );

        // Compute the usable page dimensions at this mip level, by dividing the texel dimensions by the 
        // page dimensions:
        pDesc->TileWidth = XGNextMultiple( pDesc->TexelWidth, pDesc->TileTexelWidth ) / pDesc->TileTexelWidth;
        pDesc->TileHeight = XGNextMultiple( pDesc->TexelHeight, pDesc->TileTexelHeight ) / pDesc->TileTexelHeight;
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::GetLevelDesc
    // Desc: Returns a copy of one of the precomputed level descs for this resource.
    //--------------------------------------------------------------------------------------
    VOID TiledResourceBase::GetLevelDesc( UINT MipLevel, D3DTILED_SURFACE_DESC* pDesc ) const
    {
        ASSERT( pDesc != NULL );
        if( MipLevel >= GetMipLevelCount() )
        {
            ZeroMemory( pDesc, sizeof(D3DTILED_SURFACE_DESC) );
            return;
        }
        XMemCpy( pDesc, &m_MipLevelDesc[MipLevel], sizeof(D3DTILED_SURFACE_DESC) );
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::GetVirtualPageIDFloat
    // Desc: Converts a UV texture coordinate, array slice index, and mip level combination
    //       into a virtual page address for the page that contains those coordinates.
    //--------------------------------------------------------------------------------------
    VirtualPageID TiledResourceBase::GetVirtualPageIDFloat( FLOAT U, FLOAT V, UINT SliceIndex, UINT MipLevel ) const
    {
        if( IsTexture2D() || IsTexture2DArray() )
        {
            // Validate mip level:
            if( SliceIndex >= GetArraySliceCount() )
            {
                return INVALID_VIRTUAL_PAGE_ID;
            }

            // Validate array slice index:
            if( MipLevel >= GetMipLevelCount() )
            {
                return INVALID_VIRTUAL_PAGE_ID;
            }

            // Get the level desc for this mip level:
            assert( MipLevel < 9 );
            const InternalSurfaceDesc& SurfDesc = m_MipLevelDesc[MipLevel];

            // adjust U and V coordinates for resource UV transform
            ASSERT( MipLevel < ARRAYSIZE(m_LODConstants) );
            U *= m_LODConstants[MipLevel].z;
            V *= m_LODConstants[MipLevel].w;

            // Compute the page X and Y coordinates:
            UINT PageX = (UINT)( U * (FLOAT)( SurfDesc.AddressablePageWidth ) );
            PageX = min( PageX, SurfDesc.TileWidth - 1 );
            UINT PageY = (UINT)( V * (FLOAT)( SurfDesc.AddressablePageHeight ) );
            PageY = min( PageY, SurfDesc.TileHeight - 1 );

            ASSERT( PageX < SurfDesc.TileWidth );
            ASSERT( PageY < SurfDesc.TileHeight );

            // Fill in a virtual address with the page X and Y coordinates, along
            // with the mip level and array slice index:
            VirtualPageID VPageID;
            VPageID.ResourceID = m_ResourceID;
            VPageID.PageX = PageX;
            VPageID.PageY = PageY;
            VPageID.ArraySlice = SliceIndex;
            VPageID.MipLevel = MipLevel;
            VPageID.Valid = 1;

            return VPageID;
        }
        NOTIMPL;
        return INVALID_VIRTUAL_PAGE_ID;
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::GetVirtualPageIDTexel
    // Desc: Converts a texel XY coordinate, array slice index, and mip level combination
    //       into a virtual page address for the page that contains those coordinates.
    //--------------------------------------------------------------------------------------
    VirtualPageID TiledResourceBase::GetVirtualPageIDTexel( UINT TexelX, UINT TexelY, UINT SliceIndex, UINT MipLevel ) const
    {
        if( IsTexture2D() || IsTexture2DArray() )
        {
            // Get the page size in texels for the resource format:
            const SIZE PageSizeTexels = GetPageSizeTexels( m_DataFormat );

            // Convert the texel coordinates into page coordinates, and return the virtual address:
            return GetVirtualPageIDPage( TexelX / PageSizeTexels.cx, TexelY / PageSizeTexels.cy, SliceIndex, MipLevel );
        }
        NOTIMPL;
        return INVALID_VIRTUAL_PAGE_ID;
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::GetVirtualPageIDPage
    // Desc: Converts a page XY coordinate, array slice index, and mip level combination
    //       into a virtual page address.
    //--------------------------------------------------------------------------------------
    VirtualPageID TiledResourceBase::GetVirtualPageIDPage( UINT PageX, UINT PageY, UINT SliceIndex, UINT MipLevel ) const
    {
        if( IsTexture2D() || IsTexture2DArray() )
        {
            // Validate mip level:
            if( MipLevel >= GetMipLevelCount() )
            {
                return INVALID_VIRTUAL_PAGE_ID;
            }

            // Validate array slice index:
            if( SliceIndex >= GetArraySliceCount() )
            {
                return INVALID_VIRTUAL_PAGE_ID;
            }

            // Validate page X and Y coordinates:
            assert( MipLevel < 9 );
            const InternalSurfaceDesc& MipDesc = m_MipLevelDesc[MipLevel];
            if( PageX >= MipDesc.AddressablePageWidth || PageY >= MipDesc.AddressablePageHeight )
            {
                return INVALID_VIRTUAL_PAGE_ID;
            }

            // Fill in a virtual address with the page X and Y coordinates, along
            // with the mip level and array slice index:
            VirtualPageID VPageID;
            VPageID.ResourceID = m_ResourceID;
            VPageID.PageX = PageX;
            VPageID.PageY = PageY;
            VPageID.ArraySlice = SliceIndex;
            VPageID.MipLevel = MipLevel;
            VPageID.Valid = 1;
            return VPageID;
        }
        NOTIMPL;
        return INVALID_VIRTUAL_PAGE_ID;
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::SetCPUIndexMapEntry
    // Desc: Given a virtual address to physical address mapping, this method updates a
    //       single texel in the CPU index map.
    //--------------------------------------------------------------------------------------
    HRESULT TiledResourceBase::SetCPUIndexMapEntry( VirtualPageID VPageID, PhysicalPageID PageID, INT PagePoolIndex )
    {
        ASSERT( VPageID.Valid );
        ASSERT( VPageID.ResourceID == m_ResourceID );

        if( IsTexture2D() || IsTexture2DArray() )
        {
            AtlasEntry* pEntry = NULL;
            if( PagePoolIndex != -1 )
            {
                pEntry = m_pTypedPagePool->GetAtlasEntry( PagePoolIndex );
                ASSERT( pEntry->PageID == PageID );
            }

            UINT SubresourceIndex = (UINT)VPageID.ArraySlice * GetMipLevelCount() + (UINT)VPageID.MipLevel;

            D3DLOCKED_RECT LockData;
            m_IndexMapCPU.CPUMap( SubresourceIndex, &LockData );

            BYTE* pBits = (BYTE*)LockData.pBits;
            pBits += VPageID.PageY * LockData.Pitch;
            pBits += VPageID.PageX * INDEXMAP_TEXEL_SIZE_BYTES;

            Texel565 Texel;

            if( PageID != INVALID_PHYSICAL_PAGE_ID )
            {
                ASSERT( pEntry != NULL );
                Texel.Red = pEntry->X;
                Texel.Blue = pEntry->Y;
                Texel.Green = pEntry->Slice;
            }
            else
            {
                Texel = NULL_INDEXMAP_TEXEL;
            }

            Texel565* pTexel = (Texel565*)pBits;
            *pTexel = Texel;

            MemoryBarrier();

            return S_OK;
        }
        NOTIMPL;
        return E_NOTIMPL;
    }

    //--------------------------------------------------------------------------------------
    // Name: FetchFromIndexMap
    // Desc: Helper function that decodes an index map entry from a locked CPU index map
    //       texture.
    //--------------------------------------------------------------------------------------
    inline VOID FetchFromIndexMap( const D3DLOCKED_RECT& LockRect, const D3DSURFACE_DESC& SurfDesc, INT TexelX, INT TexelY, UINT* pAtlasX, UINT* pAtlasY, UINT* pAtlasSlice, BOOL* pValid )
    {
        if( TexelX < 0 || TexelY < 0 || TexelX >= (INT)SurfDesc.Width || TexelY >= (INT)SurfDesc.Height )
        {
            *pValid = FALSE;
            return;
        }

        // Select the proper index map texel:
        const BYTE* pBits = (const BYTE*)LockRect.pBits;
        pBits += TexelY * LockRect.Pitch;
        pBits += TexelX * INDEXMAP_TEXEL_SIZE_BYTES;

        const Texel565 Texel = *(const Texel565*)pBits;

        // Decode the valid flag:
        *pValid = Texel.Red < NULL_INDEXMAP_TEXEL.Red;

        // Decode the atlas location:
        *pAtlasX = Texel.Red;
        *pAtlasY = Texel.Blue;
        *pAtlasSlice = Texel.Green;
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::GetNeighborhood
    // Desc: For a given virtual page address, this method finds that page and its immediate
    //       8 neighbors in the index map, and returns the neighbor pages' physical addresses.
    //--------------------------------------------------------------------------------------
    HRESULT TiledResourceBase::GetNeighborhood( VirtualPageID CenterPage, PageNeighborhood* pNeighborhood )
    {
        if( IsTexture2D() || IsTexture2DArray() )
        {
            // For quilting, we go through a different codepath:
            if( IsQuilted() )
            {
                return GetQuiltNeighborhood( CenterPage, pNeighborhood );
            }

            // Get the index map mip level size for the given mip level:
            D3DTexture* pTexture = (D3DTexture*)m_pIndexMapGPU;
            D3DSURFACE_DESC SurfDesc;
            pTexture->GetLevelDesc( CenterPage.MipLevel, &SurfDesc );

            INT CenterX = (INT)CenterPage.PageX;
            INT CenterY = (INT)CenterPage.PageY;

            BOOL ValidEntry[9];
            UINT AtlasX[9];
            UINT AtlasY[9];
            UINT AtlasSlice[9];

            // Compute the subresource index for the given mip level and array slice:
            UINT SubresourceIndex = (UINT)CenterPage.ArraySlice * GetMipLevelCount() + (UINT)CenterPage.MipLevel;

            // "Map" the CPU texture for reading:
            D3DLOCKED_RECT LockedMipLevel;
            m_IndexMapCPU.CPUMap( SubresourceIndex, &LockedMipLevel );

            // Using the index map texels, convert the 8 neighbors and center page into 9 physical page array locations:
            FetchFromIndexMap( LockedMipLevel, SurfDesc, CenterX - 1, CenterY - 1, &AtlasX[0], &AtlasY[0], &AtlasSlice[0], &ValidEntry[0] );
            FetchFromIndexMap( LockedMipLevel, SurfDesc, CenterX + 0, CenterY - 1, &AtlasX[1], &AtlasY[1], &AtlasSlice[1], &ValidEntry[1] );
            FetchFromIndexMap( LockedMipLevel, SurfDesc, CenterX + 1, CenterY - 1, &AtlasX[2], &AtlasY[2], &AtlasSlice[2], &ValidEntry[2] );

            FetchFromIndexMap( LockedMipLevel, SurfDesc, CenterX - 1, CenterY + 0, &AtlasX[3], &AtlasY[3], &AtlasSlice[3], &ValidEntry[3] );
            FetchFromIndexMap( LockedMipLevel, SurfDesc, CenterX + 0, CenterY + 0, &AtlasX[4], &AtlasY[4], &AtlasSlice[4], &ValidEntry[4] );
            FetchFromIndexMap( LockedMipLevel, SurfDesc, CenterX + 1, CenterY + 0, &AtlasX[5], &AtlasY[5], &AtlasSlice[5], &ValidEntry[5] );

            FetchFromIndexMap( LockedMipLevel, SurfDesc, CenterX - 1, CenterY + 1, &AtlasX[6], &AtlasY[6], &AtlasSlice[6], &ValidEntry[6] );
            FetchFromIndexMap( LockedMipLevel, SurfDesc, CenterX + 0, CenterY + 1, &AtlasX[7], &AtlasY[7], &AtlasSlice[7], &ValidEntry[7] );
            FetchFromIndexMap( LockedMipLevel, SurfDesc, CenterX + 1, CenterY + 1, &AtlasX[8], &AtlasY[8], &AtlasSlice[8], &ValidEntry[8] );

            const PageNeighbors OutputLocations[] = { PN_TOPLEFT, PN_TOP, PN_TOPRIGHT, PN_LEFT, PN_COUNT, PN_RIGHT, PN_BOTTOMLEFT, PN_BOTTOM, PN_BOTTOMRIGHT };
            C_ASSERT( ARRAYSIZE(ValidEntry) == ARRAYSIZE(OutputLocations) );

            // Loop over the 9 physical page array locations:
            for( UINT i = 0; i < ARRAYSIZE(ValidEntry); ++i )
            {
                // Convert the array location into a physical page address using the typed page pool:
                PhysicalPageID PageID = INVALID_PHYSICAL_PAGE_ID;
                if( ValidEntry[i] )
                {
                    PageID = m_pTypedPagePool->GetPageByAtlasLocation( AtlasSlice[i], AtlasX[i], AtlasY[i] );
                }

                // Place the physical page ID into the proper slot in the neighborhood structure:
                if( OutputLocations[i] == PN_COUNT )
                {
                    pNeighborhood->m_CenterPage = PageID;
                }
                else
                {
                    pNeighborhood->m_Neighbors[OutputLocations[i]] = PageID;
                }
            }

            return S_OK;
        }
        return E_NOTIMPL;
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::GetQuiltNeighborhood
    // Desc: For a given virtual page address, this method finds that page and its immediate
    //       8 neighbors in the index map, and returns the neighbor pages' physical addresses.
    //       This method has additional logic to deal with quilt boundaries.
    //--------------------------------------------------------------------------------------
    HRESULT TiledResourceBase::GetQuiltNeighborhood( VirtualPageID CenterPage, PageNeighborhood* pNeighborhood )
    {
        ASSERT( IsTexture2DArray() && IsQuilted() );

        D3DTexture* pTexture = (D3DTexture*)m_pIndexMapGPU;
        D3DSURFACE_DESC SurfDesc;
        pTexture->GetLevelDesc( CenterPage.MipLevel, &SurfDesc );

        // Get the level desc of the mip level:
        const InternalSurfaceDesc& MipSurfDesc = m_MipLevelDesc[CenterPage.MipLevel];

        // Determine the quilt location from the array slice index:
        INT QuiltX = (UINT)CenterPage.ArraySlice % GetQuiltWidth();
        INT QuiltY = (UINT)CenterPage.ArraySlice / GetQuiltWidth();

        // Determine the page location:
        INT CenterX = (INT)CenterPage.PageX;
        INT CenterY = (INT)CenterPage.PageY;

        // Build a static array of X and Y offsets for the given neighbor directions:
        static const INT XOffset[PN_COUNT] = { 0, 0, -1, 1, -1, 1, 1, -1 };
        static const INT YOffset[PN_COUNT] = { -1, 1, 0, 0, -1, 1, -1, 1 };

        // Loop over the neighbors:
        for( UINT Neighbor = 0; Neighbor < PN_COUNT; ++Neighbor )
        {
            // Get the page location of the neighbor:
            INT PageX = CenterX + XOffset[Neighbor];
            INT PageY = CenterY + YOffset[Neighbor];

            INT CurrentQuiltX = QuiltX;
            INT CurrentQuiltY = QuiltY;

            // If the page location is outside the mip level bounds, then go to the quilt neighbor:
            if( PageX < 0 )
            {
                CurrentQuiltX--;
                PageX = MipSurfDesc.TileWidth - 1;
            }
            else if( PageX >= (INT)MipSurfDesc.TileWidth )
            {
                CurrentQuiltX++;
                PageX = 0;
            }

            if( PageY < 0 )
            {
                CurrentQuiltY--;
                PageY = MipSurfDesc.TileHeight - 1;
            }
            else if( PageY >= (INT)MipSurfDesc.TileHeight )
            {
                CurrentQuiltY++;
                PageY = 0;
            }

            // Find the neighboring page if the quilt location is still valid:
            if( CurrentQuiltX >= 0 && CurrentQuiltX < (INT)GetQuiltWidth() && CurrentQuiltY >= 0 && CurrentQuiltY < (INT)GetQuiltHeight() )
            {
                // Compute a new array slice index for the neighbor page:
                UINT SliceIndex = CurrentQuiltY * GetQuiltWidth() + CurrentQuiltX;

                // Compute the subresource index:
                UINT SubresourceIndex = SliceIndex * GetMipLevelCount() + (UINT)CenterPage.MipLevel;

                // Map the CPU texture for reading:
                D3DLOCKED_RECT LockRect;
                m_IndexMapCPU.CPUMap( SubresourceIndex, &LockRect );

                BOOL ValidEntry;
                UINT AtlasX;
                UINT AtlasY;
                UINT AtlasSlice;

                // Get the atlas location of the physical page:
                FetchFromIndexMap( LockRect, SurfDesc, PageX, PageY, &AtlasX, &AtlasY, &AtlasSlice, &ValidEntry );

                // Fill in the appropriate slot on the neighborhood struct:
                if( ValidEntry )
                {
                    PhysicalPageID PageID = m_pTypedPagePool->GetPageByAtlasLocation( AtlasSlice, AtlasX, AtlasY );
                    pNeighborhood->m_Neighbors[Neighbor] = PageID;
                }
                else
                {
                    pNeighborhood->m_Neighbors[Neighbor] = INVALID_PHYSICAL_PAGE_ID;
                }
            }
            else
            {
                pNeighborhood->m_Neighbors[Neighbor] = INVALID_PHYSICAL_PAGE_ID;
            }
        }

        return S_OK;
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::GetMemoryUsage
    // Desc: Returns memory usage statistics for this resource.
    //--------------------------------------------------------------------------------------
    VOID TiledResourceBase::GetMemoryUsage( D3DTILED_MEMORY_USAGE* pMemoryUsage ) const
    {
        ASSERT( pMemoryUsage != NULL );

        // Increment resource count:
        pMemoryUsage->ResourceCount++;

        UINT BaseSize, MipSize;
        XGGetTextureLayout( m_pIndexMapGPU, NULL, &BaseSize, NULL, NULL, 0, NULL, &MipSize, NULL, NULL, 0 );
        pMemoryUsage->ResourcePhysicalMemoryBytesAllocated += ( BaseSize + MipSize );

        // Accumulate the resource's total virtual memory size in bytes:
        UINT SlicePageCount = 0;
        for( UINT i = 0; i < m_MipLevelCount; ++i )
        {
            UINT PageCount = m_MipLevelDesc[i].AddressablePageWidth * m_MipLevelDesc[i].AddressablePageHeight;
            SlicePageCount += PageCount;
        }

        SlicePageCount *= GetArraySliceCount();
        pMemoryUsage->ResourceVirtualBytesAllocated += (UINT64)SlicePageCount * (UINT64)PAGE_SIZE_BYTES;

        pMemoryUsage->ResourceCachedMemoryBytesAllocated += m_IndexMapCPU.GetMemoryUsage();
    }

    //--------------------------------------------------------------------------------------
    // Name: TiledResourceBase::ConvertQuiltUVToArrayUVW
    // Desc: Converts an extended UV coordinate (0..M, 0..N) to normalized UV coordinates
    //       (0..1) plus the array slice index returned as a result.
    //--------------------------------------------------------------------------------------
    UINT TiledResourceBase::ConvertQuiltUVToArrayUVW( FLOAT* pU, FLOAT* pV ) const
    {
        FLOAT U = *pU;
        FLOAT V = *pV;

        const INT SliceCount = (INT)GetArraySliceCount();
        const INT QuiltWidth = (INT)GetQuiltWidth();
        const INT QuiltHeight = (INT)GetQuiltHeight();

        // Compute integer quilt location from the UV coordinates:
        INT QuiltU = min( QuiltWidth - 1, max( 0, (INT)U ) );
        INT QuiltV = min( QuiltHeight - 1, max( 0, (INT)V ) );

        // Compute a slice index from the quilt location:
        INT SliceIndex = QuiltV * m_QuiltWidth + QuiltU;
        ASSERT( SliceIndex < SliceCount );

        // Return the fractional component of U and V:
        *pU = U - floorf( U );
        *pV = V - floorf( V );

        // Return the slice index:
        return (UINT)SliceIndex;
    }
}
