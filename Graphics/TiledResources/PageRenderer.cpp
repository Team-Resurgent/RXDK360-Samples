//--------------------------------------------------------------------------------------
// PageRenderer.cpp
//
// A deferred execution queue that performs resource manipulation operations.  Most
// operations manipulate texels within a page pool atlas texture.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "PageRenderer.h"
#include "TypedPagePool.h"
#include "PhysicalPageManager.h"
#include "TiledResourceBase.h"

#include "TiledRuntimeTest.h"
using namespace TiledRuntimeTest;

namespace TiledRuntime
{
    struct CopyVertex
    {
        XMFLOAT2 Position;
        XMFLOAT4 TexCoord;
    };

    //--------------------------------------------------------------------------------------
    // Name: LinearPage constructor
    // Desc: Allocates a single 64KB buffer and then creates a series of texture headers
    //       that all point to the single buffer.
    //--------------------------------------------------------------------------------------
    LinearPage::LinearPage()
    {
        // Create the 64KB buffer:
        m_pPageBuffer = MemAllocPhysical( PAGE_SIZE_BYTES );
        ASSERT( m_pPageBuffer != NULL );

        // Loop over all of the page formats:
        for( UINT i = 0; i < ARRAYSIZE(m_pPageTexture); ++i )
        {
            // Get the page size in texels of this format:
            SIZE PageSizeTexels = GetPageSizeTexels( (PageDataFormat)i );

            // Skip unsupported formats:
            if( PageSizeTexels.cx == 0 || PageSizeTexels.cy == 0 )
            {
                m_pPageTexture[i] = NULL;
                continue;
            }

            // Alias compressed formats as uncompressed 64/128bpp formats:
            switch( (PageDataFormat)i )
            {
            case PDF_BC1_4:
            case PDF_BC2_3_5:
                PageSizeTexels.cx /= 4;
                PageSizeTexels.cy /= 4;
                break;
            }
            D3DFORMAT PageFormat = (D3DFORMAT)MAKELINFMT( GetAliasedPagePoolArrayTextureFormat( (PageDataFormat)i ) );

            // Create the texture header.  Passing a valid buffer to CreateZeroedTexture2D
            // skips the memory allocation in this method:
            CreateZeroedTexture2D( PageSizeTexels.cx, PageSizeTexels.cy, 1, PageFormat, (D3DBaseTexture**)&m_pPageTexture[i], m_pPageBuffer, PAGE_SIZE_BYTES );
        }

        m_pLastUsedTexture = NULL;
        m_Pending = FALSE;
    }

    //--------------------------------------------------------------------------------------
    // Name: LinearPage destructor
    // Desc: Frees the 64KB physical memory buffer.
    //--------------------------------------------------------------------------------------
    LinearPage::~LinearPage()
    {
        MemFreePhysical( m_pPageBuffer );
        m_pPageBuffer = NULL;
    }

    //--------------------------------------------------------------------------------------
    // Name: LinearPage::CopyToPage
    // Desc: Copies 64KB of cached memory to the physical buffer.
    //--------------------------------------------------------------------------------------
    VOID LinearPage::CopyToPage( const VOID* pBuffer )
    {
        XMemCpyStreaming_WriteCombined( m_pPageBuffer, pBuffer, PAGE_SIZE_BYTES );
    }

    //--------------------------------------------------------------------------------------
    // Name: LinearPage::GetTexture
    // Desc: Returns the correct texture header for this linear page, given a page format.
    //--------------------------------------------------------------------------------------
    D3DTexture* LinearPage::GetTexture( PageDataFormat DataFormat )
    {
        m_pLastUsedTexture = m_pPageTexture[DataFormat];
        return m_pLastUsedTexture;
    }

    //--------------------------------------------------------------------------------------
    // Name: LinearPage::IsBusy
    // Desc: Returns TRUE if this linear page is either queued for use, or currently kicked
    //       off in the command buffer and waiting for GPU execution.  Returns FALSE
    //       otherwise.
    //--------------------------------------------------------------------------------------
    BOOL LinearPage::IsBusy()
    {
        if( m_pLastUsedTexture != NULL )
        {
            return m_Pending || m_pLastUsedTexture->IsBusy();
        }
        return m_Pending;
    }

    //--------------------------------------------------------------------------------------
    // Name: LinearPage::GetFullRect
    // Desc: Returns a D3DRECT positioned at 0,0 with the width and height matching the 
    //       texture size for the given format.
    //--------------------------------------------------------------------------------------
    D3DRECT LinearPage::GetFullRect( PageDataFormat DataFormat ) const
    {
        D3DRECT FullRect;
        FullRect.x1 = 0;
        FullRect.y1 = 0;
        
        D3DTexture* pTex = m_pPageTexture[DataFormat];
        FullRect.x2 = pTex->Format.Size.TwoD.Width + 1;
        FullRect.y2 = pTex->Format.Size.TwoD.Height + 1;

        return FullRect;
    }

    //--------------------------------------------------------------------------------------
    // Name: PageRenderer constructor
    //--------------------------------------------------------------------------------------
    PageRenderer::PageRenderer( D3DDevice* pd3dDevice, UINT ExpectedPageUpdatesPerFrame )
    {
        m_pd3dDevice = pd3dDevice;
        CreateShaders();

        InitializeCriticalSection( &m_QueueCritSec );
        InitializeCriticalSection( &m_LinearPageCritSec );

        // double buffer the linear pages
        ExpectedPageUpdatesPerFrame *= 2;
        m_LinearPages.reserve( ExpectedPageUpdatesPerFrame );
        for( UINT i = 0; i < ExpectedPageUpdatesPerFrame; ++i )
        {
            LinearPage* pLP = new LinearPage();
            m_LinearPages.push_back( pLP );
        }
        m_NextLinearPageIndex = 0;

        D3DSURFACE_PARAMETERS Params = { 0 };

        // create rendertargets
        for( UINT i = 0; i < ARRAYSIZE(m_pRenderSurfaces); ++i )
        {
            D3DFORMAT SurfaceFormat = GetPageRenderSurfaceFormat( (PageDataFormat)i );
            SIZE PageSizeTexels = GetPageSizeTexels( (PageDataFormat)i );
            UINT BorderTexelCount = GetPageBorderTexelCount( (PageDataFormat)i );

            if( SurfaceFormat == 0 || PageSizeTexels.cx == 0 || PageSizeTexels.cy == 0 )
            {
                m_pRenderSurfaces[i] = NULL;
                continue;
            }

            UINT TargetWidth = ( PageSizeTexels.cx + BorderTexelCount * 2 ) * ATLAS_COLUMNS;
            UINT TargetHeight = ( PageSizeTexels.cy + BorderTexelCount * 2 ) * ATLAS_ROWS;

            // Adjust target width and height for compressed textures.
            switch( (PageDataFormat)i )
            {
            case PDF_BC1_4:
            case PDF_BC2_3_5:
                TargetWidth /= 4;
                TargetHeight /= 4;
                break;
            }

            m_pd3dDevice->CreateRenderTarget( TargetWidth, TargetHeight, SurfaceFormat, D3DMULTISAMPLE_NONE, 0, FALSE, &m_pRenderSurfaces[i], &Params );
            ASSERT( m_pRenderSurfaces[i] != NULL );
        }

        // create export rendertarget
        m_pd3dDevice->CreateRenderTarget( 1, 1, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &m_pExportSurface, &Params );
    }

    //--------------------------------------------------------------------------------------
    // Name: PageRenderer destructor
    //--------------------------------------------------------------------------------------
    PageRenderer::~PageRenderer()
    {
        // Unimplemented.
    }

    //--------------------------------------------------------------------------------------
    // Name: CompilePixelShader
    // Desc: Compiles a pixel shader from the given HLSL, and a "main" entrypoint.
    //--------------------------------------------------------------------------------------
    HRESULT CompilePixelShader( D3DDevice* pd3dDevice, const CHAR* strShader, D3DPixelShader** ppShader )
    {
        ID3DXBuffer* pShader = NULL;
        ID3DXBuffer* pErrorMsgs = NULL;
        HRESULT hr = D3DXCompileShader( strShader, strlen( strShader ), NULL, NULL, "main", "ps_3_0", 0, &pShader, &pErrorMsgs, NULL );
        if( FAILED(hr) )
        {
            DebugSpew( "Shader compile error: %s\n", (const CHAR*)pErrorMsgs->GetBufferPointer() );
            SAFE_RELEASE( pErrorMsgs );
            return E_FAIL;
        }
        else
        {
            hr = pd3dDevice->CreatePixelShader( (const DWORD*)pShader->GetBufferPointer(), ppShader );
            SAFE_RELEASE( pShader );
            return hr;
        }
    }

    //--------------------------------------------------------------------------------------
    // Name: CompileVertexShader
    // Desc: Compiles a vertex shader from the given HLSL, and a "main" entrypoint.
    //--------------------------------------------------------------------------------------
    HRESULT CompileVertexShader( D3DDevice* pd3dDevice, const CHAR* strShader, D3DVertexShader** ppShader )
    {
        ID3DXBuffer* pShader = NULL;
        ID3DXBuffer* pErrorMsgs = NULL;
        HRESULT hr = D3DXCompileShader( strShader, strlen( strShader ), NULL, NULL, "main", "vs_3_0", 0, &pShader, &pErrorMsgs, NULL );
        if( FAILED(hr) )
        {
            DebugSpew( "Shader compile error: %s\n", (const CHAR*)pErrorMsgs->GetBufferPointer() );
            SAFE_RELEASE( pErrorMsgs );
            return E_FAIL;
        }
        else
        {
            hr = pd3dDevice->CreateVertexShader( (const DWORD*)pShader->GetBufferPointer(), ppShader );
            SAFE_RELEASE( pShader );
            return hr;
        }
    }

    //--------------------------------------------------------------------------------------
    // Name: PageRenderer::CreateShaders
    // Desc: Compiles a variety of shaders used to copy texels from one location to another.
    //--------------------------------------------------------------------------------------
    VOID PageRenderer::CreateShaders()
    {
        // Create a standard vertex decl for copy quads:
        static const D3DVERTEXELEMENT9 CopyVertexElements[] =
        {
            { 0,     0, D3DDECLTYPE_FLOAT2,     0,  D3DDECLUSAGE_POSITION,  0 },
            { 0,     8, D3DDECLTYPE_FLOAT4,     0,  D3DDECLUSAGE_TEXCOORD,  0 },
            D3DDECL_END()
        };

        m_pd3dDevice->CreateVertexDeclaration( CopyVertexElements, &m_pDeclCopyVertex );

        // Compile all of the shaders used in resource copying.
        const CHAR* strVS = "int main( float2 InPos : POSITION, float4 InTex : TEXCOORD0, out float4 OutPos : POSITION, out float4 OutTex : TEXCOORD0 ) { OutPos = float4( InPos, 0, 1 ); OutTex = InTex; }";
        const CHAR* strPSColor = "float4 OutColor : register(c0); float4 main() : COLOR0 { return OutColor; }";
        const CHAR* strPSTex2D = "sampler2D s_tex : register(s0); float4 main( float4 TexCoord : TEXCOORD0 ) : COLOR0 { return tex2D( s_tex, TexCoord.xy ); }";
        const CHAR* strPSTex3D = "sampler3D s_tex : register(s0); float4 main( float4 TexCoord : TEXCOORD0 ) : COLOR0 { return tex3D( s_tex, TexCoord.xyz ); }";
        const CHAR* strVSExport = "float4 export_address : register(c0); static float4 const01 = float4( 0, 1, 0, 0 ); void main( float2 InPos : POSITION, float4 InTex : TEXCOORD0 ) { int Offset = InPos.x; asm { alloc export=1 mad eA, Offset, const01, export_address mov eM0, InTex }; }";
        const CHAR* strPSExportColor = "float4 export_address : register(c0); float4 SurfaceDimensions : register(c1); float4 OutColor : register(c2); static float4 const01 = float4( 0, 1, 0, 0 ); float4 main( float2 ScreenPos : VPOS ) : COLOR0 { float Offset = ScreenPos.y * SurfaceDimensions.x + ScreenPos.x; asm { alloc export=1 mad eA, Offset, const01, export_address mov eM0, OutColor }; return OutColor; }";
        const CHAR* strPSExportTex2D = "float4 export_address : register(c0); float4 SurfaceDimensions : register(c1); sampler2D s_tex : register(s0); static float4 const01 = float4( 0, 1, 0, 0 ); float4 main( float2 ScreenPos : VPOS, float4 TexCoord : TEXCOORD0 ) : COLOR0 { float Offset = ScreenPos.y * SurfaceDimensions.x + ScreenPos.x; float4 Sample = tex2D( s_tex, TexCoord.xy ); asm { alloc export=1 mad eA, Offset, const01, export_address mov eM0, Sample }; return Sample; }";
        const CHAR* strPSExportTex3D = "float4 export_address : register(c0); float4 SurfaceDimensions : register(c1); sampler3D s_tex : register(s0); static float4 const01 = float4( 0, 1, 0, 0 ); float4 main( float2 ScreenPos : VPOS, float4 TexCoord : TEXCOORD0 ) : COLOR0 { float Offset = ScreenPos.y * SurfaceDimensions.x + ScreenPos.x; float4 Sample = tex3D( s_tex, TexCoord.xyz ); asm { alloc export=1 mad eA, Offset, const01, export_address mov eM0, Sample }; return Sample; }";

        HRESULT hr;
        hr = CompileVertexShader( m_pd3dDevice, strVS, &m_pVSPassthru );
        ASSERT( SUCCEEDED(hr) );

        hr = CompilePixelShader( m_pd3dDevice, strPSColor, &m_pPSCopyColor );
        ASSERT( SUCCEEDED(hr) );

        hr = CompilePixelShader( m_pd3dDevice, strPSTex2D, &m_pPSCopyTex2D );
        ASSERT( SUCCEEDED(hr) );

        hr = CompilePixelShader( m_pd3dDevice, strPSTex3D, &m_pPSCopyTexArray );
        ASSERT( SUCCEEDED(hr) );

        hr = CompileVertexShader( m_pd3dDevice, strVSExport, &m_pVSExport );
        ASSERT( SUCCEEDED(hr) );

        hr = CompilePixelShader( m_pd3dDevice, strPSExportColor, &m_pPSExportColor );
        ASSERT( SUCCEEDED(hr) );

        hr = CompilePixelShader( m_pd3dDevice, strPSExportTex2D, &m_pPSExportTex2D );
        ASSERT( SUCCEEDED(hr) );

        hr = CompilePixelShader( m_pd3dDevice, strPSExportTex3D, &m_pPSExportTexArray );
        ASSERT( SUCCEEDED(hr) );
    }

    //--------------------------------------------------------------------------------------
    // Name: PageRenderer::QueuePageUpdate
    // Desc: Queues an operation that copies an untyped page buffer into an emulated
    //       physical page, which is a rectangular region within the given typed page pool.
    //--------------------------------------------------------------------------------------
    HRESULT PageRenderer::QueuePageUpdate( TypedPagePool* pPagePool, INT PageIndex, const VOID* pPageBuffer )
    {
        if( PageIndex == -1 )
        {
            return E_FAIL;
        }
        const AtlasEntry* pAtlasEntry = pPagePool->GetAtlasEntry( PageIndex );
        ASSERT( pAtlasEntry->PageID != INVALID_PHYSICAL_PAGE_ID );

        LinearPage* pLP = GetUnusedPage();
        ASSERT( pLP != NULL );

        pLP->CopyToPage( pPageBuffer );
        pLP->SetPending();

        // compute the destination rectangle within the array texture slice
        RECT PageRect = pPagePool->GetPageRect( pAtlasEntry );
        SIZE AtlasPageSizeTexels = pPagePool->GetAtlasPageSizeTexels();

        switch( pPagePool->GetFormat() )
        {
        case PDF_BC1_4:
        case PDF_BC2_3_5:
            PageRect.left /= 4;
            PageRect.top /= 4;
            PageRect.right /= 4;
            PageRect.bottom /= 4;
            AtlasPageSizeTexels.cx /= 4;
            AtlasPageSizeTexels.cy /= 4;
            break;
        }

        RenderOperation Op;
        Op.pLinearPage = pLP;

        // select the surface
        Op.pSurface = GetSurface( pPagePool->GetFormat() );

        // select the base array texture
        Op.pBaseTexture = pPagePool->GetAliasedArrayTexture();
        Op.BaseSliceIndex = pAtlasEntry->Slice;
        Op.BaseRect = AlignToResolveRect( PageRect, AtlasPageSizeTexels );

        // set the temp linear texture as the source texture
        Op.pSrcTexture = pLP->GetTexture( pPagePool->GetFormat() );
        Op.SrcRect = pLP->GetFullRect( pPagePool->GetFormat() );
        Op.SrcSliceIndex = 0;
        Op.pSrcArrayTexture = NULL;

        // draw to the page rect within the atlas
        Op.DrawRect = MakeD3DRect( PageRect );

        // set the resolve texture
        Op.pResolveTexture = Op.pBaseTexture;
        Op.ResolveRect = Op.BaseRect;
        Op.ResolveSliceIndex = Op.BaseSliceIndex;

        switch( pPagePool->GetFormat() )
        {
        case PDF_BC2_3_5:
        case PDF_128BPP:
            {
                // determine offset of array slice
                UINT ArraySliceOffset = XGGetMipLevelOffset( Op.pResolveTexture, Op.ResolveSliceIndex, 0 );
                UINT BaseData;
                UINT MipData;
                XGGetTextureLayout( Op.pBaseTexture, &BaseData, NULL, NULL, NULL, 0, &MipData, NULL, NULL, NULL, 0 );
                Op.pExportBaseAddress = (VOID*)( BaseData + ArraySliceOffset );
                Op.ResolveRect.x1 = 0;
                Op.ResolveRect.y1 = 0;
                Op.ResolveRect.x2 = XGNextMultiple( AtlasPageSizeTexels.cx, GPU_TEXTURE_TILE_DIMENSION );
                Op.ResolveRect.y2 = AtlasPageSizeTexels.cy;
            }
            break;
        default:
            Op.pExportBaseAddress = NULL;
            break;
        }

        QueueOperation( Op );

        Trace::FillPage( pAtlasEntry->PageID, pPagePool->GetFormat() );

        return S_OK;
    }

    //--------------------------------------------------------------------------------------
    // Name: PageRenderer::QueueBorderUpdate
    // Desc: Queues an operation that copies edge texels from one page into the border
    //       texels of another page.
    //--------------------------------------------------------------------------------------
    HRESULT PageRenderer::QueueBorderUpdate( TypedPagePool* pPagePool, PhysicalPageID CenterPageID, PhysicalPageID BorderPageID, PageNeighbors RelationshipToCenterPage, BOOL InvertSourceRelationship )
    {
        INT CenterPageIndex = pPagePool->FindPage( CenterPageID );
        INT BorderPageIndex = -1;
        if( BorderPageID != INVALID_PHYSICAL_PAGE_ID ) 
        {
            BorderPageIndex = pPagePool->FindPage( BorderPageID );
        }

        if( CenterPageIndex == -1 )
        {
            return E_FAIL;
        }

        const AtlasEntry* pCenterAtlasEntry = pPagePool->GetAtlasEntry( CenterPageIndex );
        ASSERT( pCenterAtlasEntry != NULL );

        const AtlasEntry* pBorderAtlasEntry = NULL;
        if( BorderPageIndex != -1 )
        {
            pBorderAtlasEntry = pPagePool->GetAtlasEntry( BorderPageIndex );
        }

        UINT BorderTexelCount = GetPageBorderTexelCount( pPagePool->GetFormat() );

        RECT SourceRect = { 0, 0, 0, 0 };
        if( pBorderAtlasEntry != NULL )
        {
            // compute source rectangle from border page and relationship of border page to center page
            const RECT BorderPageRect = pPagePool->GetPageRect( pBorderAtlasEntry );
            SourceRect = BorderPageRect;
            PageNeighbors SourceRelationshipToCenterPage = RelationshipToCenterPage;
            if( InvertSourceRelationship )
            {
                SourceRelationshipToCenterPage = GetOppositeNeighbor( SourceRelationshipToCenterPage );
            }
            switch( SourceRelationshipToCenterPage )
            {
            case PN_TOP:
                SourceRect.top = SourceRect.bottom - BorderTexelCount;
                break;
            case PN_BOTTOM:
                SourceRect.bottom = SourceRect.top + BorderTexelCount;
                break;
            case PN_LEFT:
                SourceRect.left = SourceRect.right - BorderTexelCount;
                break;
            case PN_RIGHT:
                SourceRect.right = SourceRect.left + BorderTexelCount;
                break;
            case PN_TOPLEFT:
                SourceRect.top = SourceRect.bottom - BorderTexelCount;
                SourceRect.left = SourceRect.right - BorderTexelCount;
                break;
            case PN_TOPRIGHT:
                SourceRect.top = SourceRect.bottom - BorderTexelCount;
                SourceRect.right = SourceRect.left + BorderTexelCount;
                break;
            case PN_BOTTOMLEFT:
                SourceRect.bottom = SourceRect.top + BorderTexelCount;
                SourceRect.left = SourceRect.right - BorderTexelCount;
                break;
            case PN_BOTTOMRIGHT:
                SourceRect.bottom = SourceRect.top + BorderTexelCount;
                SourceRect.right = SourceRect.left + BorderTexelCount;
                break;
            }
        }

        // compute destination rectangle from center page and relationship of border page to center page
        const RECT CenterPageRect = pPagePool->GetPageRect( pCenterAtlasEntry );
        RECT DestRect = CenterPageRect;
        PageNeighbors DestRelationshipToCenterPage = RelationshipToCenterPage;
        switch( DestRelationshipToCenterPage )
        {
        case PN_TOP:
            DestRect.bottom = DestRect.top;
            DestRect.top -= BorderTexelCount;
            break;
        case PN_BOTTOM:
            DestRect.top = DestRect.bottom;
            DestRect.bottom += BorderTexelCount;
            break;
        case PN_LEFT:
            DestRect.right = DestRect.left;
            DestRect.left -= BorderTexelCount;
            break;
        case PN_RIGHT:
            DestRect.left = DestRect.right;
            DestRect.right += BorderTexelCount;
            break;
        case PN_TOPLEFT:
            DestRect.bottom = DestRect.top;
            DestRect.top -= BorderTexelCount;
            DestRect.right = DestRect.left;
            DestRect.left -= BorderTexelCount;
            break;
        case PN_TOPRIGHT:
            DestRect.bottom = DestRect.top;
            DestRect.top -= BorderTexelCount;
            DestRect.left = DestRect.right;
            DestRect.right += BorderTexelCount;
            break;
        case PN_BOTTOMLEFT:
            DestRect.top = DestRect.bottom;
            DestRect.bottom += BorderTexelCount;
            DestRect.right = DestRect.left;
            DestRect.left -= BorderTexelCount;
            break;
        case PN_BOTTOMRIGHT:
            DestRect.top = DestRect.bottom;
            DestRect.bottom += BorderTexelCount;
            DestRect.left = DestRect.right;
            DestRect.right += BorderTexelCount;
            break;
        }

        if( pBorderAtlasEntry != NULL )
        {
            ASSERT( ( SourceRect.right - SourceRect.left ) == ( DestRect.right - DestRect.left ) );
            ASSERT( ( SourceRect.bottom - SourceRect.top ) == ( DestRect.bottom - DestRect.top ) );
        }

        SIZE AtlasPageSizeTexels = pPagePool->GetAtlasPageSizeTexels();

        switch( pPagePool->GetFormat() )
        {
        case PDF_BC1_4:
        case PDF_BC2_3_5:
            SourceRect.left /= 4;
            SourceRect.top /= 4;
            SourceRect.right /= 4;
            SourceRect.bottom /= 4;
            DestRect.left /= 4;
            DestRect.top /= 4;
            DestRect.right /= 4;
            DestRect.bottom /= 4;
            AtlasPageSizeTexels.cx /= 4;
            AtlasPageSizeTexels.cy /= 4;
            break;
        }

        RenderOperation Op;
        Op.pSurface = GetSurface( pPagePool->GetFormat() );

        Op.pBaseTexture = pPagePool->GetAliasedArrayTexture();
        Op.BaseSliceIndex = pCenterAtlasEntry->Slice;
        Op.BaseRect = AlignToResolveRect( DestRect, AtlasPageSizeTexels );
        
        if( pBorderAtlasEntry != NULL )
        {
            Op.pSrcArrayTexture = Op.pBaseTexture;
            Op.pSrcTexture = NULL;
            Op.SrcSliceIndex = pBorderAtlasEntry->Slice;
            Op.SrcRect = MakeD3DRect( SourceRect );
        }
        else
        {
            Op.FillColor = XMFLOAT4( 0, 0, 0, 0 );
            Op.pSrcTexture = NULL;
            Op.pSrcArrayTexture = NULL;
        }

        Op.DrawRect = MakeD3DRect( DestRect );

        Op.pResolveTexture = Op.pBaseTexture;
        Op.ResolveRect = Op.BaseRect;
        Op.ResolveSliceIndex = Op.BaseSliceIndex;

        switch( pPagePool->GetFormat() )
        {
        case PDF_BC2_3_5:
        case PDF_128BPP:
            {
                // determine offset of array slice
                UINT ArraySliceOffset = XGGetMipLevelOffset( Op.pResolveTexture, Op.ResolveSliceIndex, 0 );
                UINT BaseData;
                UINT MipData;
                XGGetTextureLayout( Op.pBaseTexture, &BaseData, NULL, NULL, NULL, 0, &MipData, NULL, NULL, NULL, 0 );
                Op.pExportBaseAddress = (VOID*)( BaseData + ArraySliceOffset );
                Op.ResolveRect.x1 = 0;
                Op.ResolveRect.y1 = 0;
                Op.ResolveRect.x2 = XGNextMultiple( AtlasPageSizeTexels.cx, GPU_TEXTURE_TILE_DIMENSION );
                Op.ResolveRect.y2 = AtlasPageSizeTexels.cy;
            }
            break;
        default:
            Op.pExportBaseAddress = NULL;
            break;
        }

        QueueOperation( Op );

        Trace::UpdatePageBorder( CenterPageID, BorderPageID, RelationshipToCenterPage );

        return S_OK;
    }

    //--------------------------------------------------------------------------------------
    // Name: PageRenderer::QueueIndexMapUpdate
    // Desc: Queues a page render operation that updates a single texel in an index map
    //       texture.
    //--------------------------------------------------------------------------------------
    HRESULT PageRenderer::QueueIndexMapUpdate( TiledResourceBase* pResource, VirtualPageID VPageID, PhysicalPageID PageID, INT PoolIndex )
    {
        ASSERT( VPageID.Valid );
        ASSERT( VPageID.ResourceID == pResource->GetResourceID() );

        TypedPagePool* pTPP = pResource->GetTypedPagePool();
        if( PoolIndex == -1 && PageID != INVALID_PHYSICAL_PAGE_ID )
        {
            PoolIndex = pTPP->FindPage( PageID );
        }
        AtlasEntry* pEntry = NULL;
        if( PoolIndex != -1 )
        {
            pEntry = pTPP->GetAtlasEntry( PoolIndex );
            ASSERT( pEntry->PageID == PageID );
        }

        D3DBaseTexture* pIndexMapTexture = pResource->GetIndexMapGPUTexture();

        BOOL TexelFound = FALSE;
        UINT TexelPhysicalOffset = 0;
        RenderOperation Op;

        if( pResource->IsTexture2D() || pResource->IsTexture2DArray() )
        {
            TexelFound = TRUE;
            TexelPhysicalOffset += XGGetMipLevelOffset( pIndexMapTexture, 0, VPageID.MipLevel );

            XGTEXTURE_DESC MipDesc;
            XGGetTextureDesc( pIndexMapTexture, VPageID.MipLevel, &MipDesc );

            UINT BaseData;
            UINT MipData;
            XGGetTextureLayout( pIndexMapTexture, &BaseData, NULL, NULL, NULL, 0, &MipData, NULL, NULL, NULL, 0 );
            Op.pExportBaseAddress = (VOID*)BaseData;

            if( VPageID.MipLevel > 0 )
            {
                TexelPhysicalOffset += ( MipData - BaseData );
            }

            TexelPhysicalOffset += (UINT)( VPageID.ArraySlice * MipDesc.SlicePitch );
            TexelPhysicalOffset += (UINT)( VPageID.PageY * MipDesc.RowPitch );
            TexelPhysicalOffset += (UINT)( VPageID.PageX * ( MipDesc.BitsPerPixel / 8 ) );
        }

        if( TexelFound )
        {
            Op.pResolveTexture = (D3DArrayTexture*)pIndexMapTexture;
            Op.ResolveRect.x1 = TexelPhysicalOffset;
            if( pEntry != NULL )
            {
                Op.FillColor = XMFLOAT4( (FLOAT)pEntry->X, (FLOAT)pEntry->Slice, (FLOAT)pEntry->Y, 0 );
            }
            else
            {
                Op.FillColor = XMFLOAT4( 31, 63, 31, 0 );
            }

            QueueOperation( Op );
            return S_OK;
        }

        NOTIMPL;
        return E_FAIL;
    }

    //--------------------------------------------------------------------------------------
    // Name: PageRenderer::QueueOperation
    // Desc: Adds a render operation to the queue, in a thread safe manner.
    //--------------------------------------------------------------------------------------
    VOID PageRenderer::QueueOperation( RenderOperation& Operation )
    {
        // TODO: validate op

        EnterCriticalSection( &m_QueueCritSec );
        m_PendingOperations.push_back( Operation );
        LeaveCriticalSection( &m_QueueCritSec );
    }

    //--------------------------------------------------------------------------------------
    // Name: PageRenderer::FlushPendingUpdates
    // Desc: Processes all render operations in the queue, passing each operation to its
    //       appropriate execution function.  This method is called once per render frame,
    //       and holds a lock on the queue while it is operating.
    //--------------------------------------------------------------------------------------
    VOID PageRenderer::FlushPendingUpdates()
    {
        PIXBeginNamedEvent( 0, "Page Renderer Operations" );

        // TODO: sort operations by source and destination

        BOOL CritSec = FALSE;
        if( !m_PendingOperations.empty() )
        {
            EnterCriticalSection( &m_QueueCritSec );
            CritSec = TRUE;
        }

        while( !m_PendingOperations.empty() )
        {
            RenderOperation& Op = m_PendingOperations.front();
            if( Op.pSurface != NULL )
            {
                if( Op.pExportBaseAddress != NULL )
                {
                    ExecuteMemExportTextureOperation( Op );
                }
                else
                {
                    ExecuteTextureOperation( Op );
                }
            }
            else
            {
                ExecuteMemExportIndexMapTexelOperation( Op );
            }
            m_PendingOperations.pop_front();
        }

        if( CritSec )
        {
            LeaveCriticalSection( &m_QueueCritSec );
        }

        PIXEndNamedEvent();
    }

    //--------------------------------------------------------------------------------------
    // Name: PageRenderer::ExecuteTextureOperation
    // Desc: Copies a source buffer to a region within a destination resource.  This is used
    //       to copy page data into a typed page pool array texture.
    //--------------------------------------------------------------------------------------
    VOID PageRenderer::ExecuteTextureOperation( const RenderOperation& Operation )
    {
        ASSERT( Operation.pSurface != NULL );

        PIXBeginNamedEvent( 0, "Page Atlas Texture Update" );

        // set rendertarget
        m_pd3dDevice->SetRenderTarget( 0, Operation.pSurface );
        m_pd3dDevice->SetDepthStencilSurface( NULL );
        m_pd3dDevice->SetRenderTarget( 1, NULL );
        m_pd3dDevice->SetRenderTarget( 2, NULL );
        m_pd3dDevice->SetRenderTarget( 3, NULL );

        // set renderstate
        m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
        m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

        // compute base rect from base texel coordinates and texture size
        CopyVertex BaseVerts[3];
        BaseVerts[0].Position.x = (FLOAT)Operation.BaseRect.x1;
        BaseVerts[0].Position.y = (FLOAT)Operation.BaseRect.y1;
        BaseVerts[1].Position.x = (FLOAT)Operation.BaseRect.x2;
        BaseVerts[1].Position.y = (FLOAT)Operation.BaseRect.y1;
        BaseVerts[2].Position.x = (FLOAT)Operation.BaseRect.x1;
        BaseVerts[2].Position.y = (FLOAT)Operation.BaseRect.y2;

        D3DSURFACE_DESC BaseTexDesc;
        Operation.pBaseTexture->GetLevelDesc( 0, &BaseTexDesc );
        BaseVerts[0].TexCoord.x = (FLOAT)Operation.BaseRect.x1 / (FLOAT)BaseTexDesc.Width;
        BaseVerts[0].TexCoord.y = (FLOAT)Operation.BaseRect.y1 / (FLOAT)BaseTexDesc.Height;
        BaseVerts[1].TexCoord.x = (FLOAT)Operation.BaseRect.x2 / (FLOAT)BaseTexDesc.Width;
        BaseVerts[1].TexCoord.y = (FLOAT)Operation.BaseRect.y1 / (FLOAT)BaseTexDesc.Height;
        BaseVerts[2].TexCoord.x = (FLOAT)Operation.BaseRect.x1 / (FLOAT)BaseTexDesc.Width;
        BaseVerts[2].TexCoord.y = (FLOAT)Operation.BaseRect.y2 / (FLOAT)BaseTexDesc.Height;

        FLOAT BaseArraySize = (FLOAT)Operation.pBaseTexture->GetArraySize();
        FLOAT BaseArrayIndex = ( (FLOAT)Operation.BaseSliceIndex + 0.5f ) / BaseArraySize;
        BaseVerts[0].TexCoord.z = BaseArrayIndex;
        BaseVerts[1].TexCoord.z = BaseArrayIndex;
        BaseVerts[2].TexCoord.z = BaseArrayIndex;

        // set array texture pixel shader
        m_pd3dDevice->SetVertexDeclaration( m_pDeclCopyVertex );
        m_pd3dDevice->SetVertexShader( m_pVSPassthru );
        m_pd3dDevice->SetPixelShader( m_pPSCopyTexArray );

        // set base texture to sampler 0
        m_pd3dDevice->SetTexture( 0, Operation.pBaseTexture );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_SEPARATEZFILTERENABLE, TRUE );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTERZ, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTERZ, D3DTEXF_POINT );

        // draw base rect
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, BaseVerts, sizeof(CopyVertex) );

        // compute source rect from source texel coordinates, draw rect, and texture size
        CopyVertex DrawVerts[3];
        DrawVerts[0].Position.x = (FLOAT)Operation.DrawRect.x1;
        DrawVerts[0].Position.y = (FLOAT)Operation.DrawRect.y1;
        DrawVerts[1].Position.x = (FLOAT)Operation.DrawRect.x2;
        DrawVerts[1].Position.y = (FLOAT)Operation.DrawRect.y1;
        DrawVerts[2].Position.x = (FLOAT)Operation.DrawRect.x1;
        DrawVerts[2].Position.y = (FLOAT)Operation.DrawRect.y2;

        D3DSURFACE_DESC SrcTexDesc;
        if( Operation.pSrcTexture != NULL )
        {
            Operation.pSrcTexture->GetLevelDesc( 0, &SrcTexDesc );
        }
        else if( Operation.pSrcArrayTexture != NULL )
        {
            Operation.pSrcArrayTexture->GetLevelDesc( 0, &SrcTexDesc );
        }
        else
        {
            SrcTexDesc.Height = 1;
            SrcTexDesc.Width = 1;
        }
        DrawVerts[0].TexCoord.x = (FLOAT)Operation.SrcRect.x1 / (FLOAT)SrcTexDesc.Width;
        DrawVerts[0].TexCoord.y = (FLOAT)Operation.SrcRect.y1 / (FLOAT)SrcTexDesc.Height;
        DrawVerts[1].TexCoord.x = (FLOAT)Operation.SrcRect.x2 / (FLOAT)SrcTexDesc.Width;
        DrawVerts[1].TexCoord.y = (FLOAT)Operation.SrcRect.y1 / (FLOAT)SrcTexDesc.Height;
        DrawVerts[2].TexCoord.x = (FLOAT)Operation.SrcRect.x1 / (FLOAT)SrcTexDesc.Width;
        DrawVerts[2].TexCoord.y = (FLOAT)Operation.SrcRect.y2 / (FLOAT)SrcTexDesc.Height;

        if( Operation.pSrcArrayTexture != NULL )
        {
            FLOAT SrcArraySize = (FLOAT)Operation.pSrcArrayTexture->GetArraySize();
            FLOAT ArrayIndex = ( (FLOAT)Operation.SrcSliceIndex + 0.5f ) / SrcArraySize;
            DrawVerts[0].TexCoord.z = ArrayIndex;
            DrawVerts[1].TexCoord.z = ArrayIndex;
            DrawVerts[2].TexCoord.z = ArrayIndex;
        }

        // set pixel shader based on source texture type
        // set source texture to sampler 0
        if( Operation.pSrcTexture != NULL )
        {
            m_pd3dDevice->SetPixelShader( m_pPSCopyTex2D );
            m_pd3dDevice->SetTexture( 0, Operation.pSrcTexture );
        }
        else if( Operation.pSrcArrayTexture != NULL )
        {
            m_pd3dDevice->SetPixelShader( m_pPSCopyTexArray );
            m_pd3dDevice->SetTexture( 0, Operation.pSrcArrayTexture );
        }
        else
        {
            m_pd3dDevice->SetPixelShader( m_pPSCopyColor );
            m_pd3dDevice->SetTexture( 0, NULL );
            m_pd3dDevice->SetPixelShaderConstantF( 0, (FLOAT*)&Operation.FillColor, 1 );
        }

        // draw source rect
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, DrawVerts, sizeof(CopyVertex) );

        // resolve to resolve texture
        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, &Operation.ResolveRect, Operation.pResolveTexture, (const D3DPOINT*)&Operation.ResolveRect, 0, Operation.ResolveSliceIndex, NULL, 0, 0, NULL );

        // restore renderstate
        m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );

        // if we are using a linear page, clear its pending flag
        if( Operation.pLinearPage != NULL )
        {
            Operation.pLinearPage->ClearPending();
        }

        m_pd3dDevice->SetTexture( 0, NULL );
        m_pd3dDevice->InsertFence();

        PIXEndNamedEvent();
    }

    //--------------------------------------------------------------------------------------
    // Name: PageRenderer::ExecuteMemExportTextureOperation
    // Desc: Copies a source buffer to a region within a destination resource.  This is used
    //       to copy page data into a typed page pool array texture, but only for 128bpp
    //       texture formats (including DXT3/5/N textures which are 128 bits per block).
    //--------------------------------------------------------------------------------------
    VOID PageRenderer::ExecuteMemExportTextureOperation( const RenderOperation& Operation )
    {
        ASSERT( Operation.pSurface != NULL );
        ASSERT( Operation.pExportBaseAddress != NULL );

        PIXBeginNamedEvent( 0, "Page Atlas Memexport Texture Update" );

        // set rendertarget
        m_pd3dDevice->SetRenderTarget( 0, Operation.pSurface );
        m_pd3dDevice->SetDepthStencilSurface( NULL );
        m_pd3dDevice->SetRenderTarget( 1, NULL );
        m_pd3dDevice->SetRenderTarget( 2, NULL );
        m_pd3dDevice->SetRenderTarget( 3, NULL );

        // set renderstate
        m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
        m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

        // compute source rect from source texel coordinates, draw rect, and texture size
        CopyVertex DrawVerts[3];
        DrawVerts[0].Position.x = (FLOAT)Operation.DrawRect.x1;
        DrawVerts[0].Position.y = (FLOAT)Operation.DrawRect.y1;
        DrawVerts[1].Position.x = (FLOAT)Operation.DrawRect.x2;
        DrawVerts[1].Position.y = (FLOAT)Operation.DrawRect.y1;
        DrawVerts[2].Position.x = (FLOAT)Operation.DrawRect.x1;
        DrawVerts[2].Position.y = (FLOAT)Operation.DrawRect.y2;

        D3DSURFACE_DESC SrcTexDesc;
        if( Operation.pSrcTexture != NULL )
        {
            Operation.pSrcTexture->GetLevelDesc( 0, &SrcTexDesc );
        }
        else if( Operation.pSrcArrayTexture != NULL )
        {
            Operation.pSrcArrayTexture->GetLevelDesc( 0, &SrcTexDesc );
        }
        else
        {
            SrcTexDesc.Height = 1;
            SrcTexDesc.Width = 1;
        }
        DrawVerts[0].TexCoord.x = (FLOAT)Operation.SrcRect.x1 / (FLOAT)SrcTexDesc.Width;
        DrawVerts[0].TexCoord.y = (FLOAT)Operation.SrcRect.y1 / (FLOAT)SrcTexDesc.Height;
        DrawVerts[1].TexCoord.x = (FLOAT)Operation.SrcRect.x2 / (FLOAT)SrcTexDesc.Width;
        DrawVerts[1].TexCoord.y = (FLOAT)Operation.SrcRect.y1 / (FLOAT)SrcTexDesc.Height;
        DrawVerts[2].TexCoord.x = (FLOAT)Operation.SrcRect.x1 / (FLOAT)SrcTexDesc.Width;
        DrawVerts[2].TexCoord.y = (FLOAT)Operation.SrcRect.y2 / (FLOAT)SrcTexDesc.Height;

        if( Operation.pSrcArrayTexture != NULL )
        {
            FLOAT SrcArraySize = (FLOAT)Operation.pSrcArrayTexture->GetArraySize();
            FLOAT ArrayIndex = ( (FLOAT)Operation.SrcSliceIndex + 0.5f ) / SrcArraySize;
            DrawVerts[0].TexCoord.z = ArrayIndex;
            DrawVerts[1].TexCoord.z = ArrayIndex;
            DrawVerts[2].TexCoord.z = ArrayIndex;
        }

        m_pd3dDevice->SetVertexDeclaration( m_pDeclCopyVertex );
        m_pd3dDevice->SetVertexShader( m_pVSPassthru );

        // set pixel shader based on source texture type
        // set source texture to sampler 0
        if( Operation.pSrcTexture != NULL )
        {
            m_pd3dDevice->SetPixelShader( m_pPSExportTex2D );
            m_pd3dDevice->SetTexture( 0, Operation.pSrcTexture );
        }
        else if( Operation.pSrcArrayTexture != NULL )
        {
            m_pd3dDevice->SetPixelShader( m_pPSExportTexArray );
            m_pd3dDevice->SetTexture( 0, Operation.pSrcArrayTexture );
        }
        else
        {
            m_pd3dDevice->SetPixelShader( m_pPSExportColor );
            m_pd3dDevice->SetTexture( 0, NULL );
            m_pd3dDevice->SetPixelShaderConstantF( 2, (FLOAT*)&Operation.FillColor, 1 );
        }

        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_SEPARATEZFILTERENABLE, TRUE );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTERZ, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTERZ, D3DTEXF_POINT );

        D3DArrayTexture* pExportTexture = Operation.pBaseTexture;
        UINT BaseSizeBytes, MipSizeBytes;
        XGGetTextureLayout( pExportTexture, NULL, &BaseSizeBytes, NULL, NULL, 0, NULL, &MipSizeBytes, NULL, NULL, 0 );
        BaseSizeBytes += MipSizeBytes;

        GPU_MEMEXPORT_STREAM_CONSTANT ExportConstant;
        GPU_SET_MEMEXPORT_STREAM_CONSTANT( &ExportConstant,
            Operation.pExportBaseAddress,                    
            BaseSizeBytes / sizeof(DWORD),                                              
            SURFACESWAP_LOW_RED,                            
            GPUSURFACENUMBER_FLOAT,                         
            GPUCOLORFORMAT_32_32_32_32_FLOAT,               
            GPUENDIAN128_8IN32 );

        m_pd3dDevice->SetPixelShaderConstantF( 0, ExportConstant.c, 1 );

        XMFLOAT4 SurfaceDimensions( (FLOAT)Operation.ResolveRect.x2, (FLOAT)Operation.ResolveRect.y2, 0, 0 );
        m_pd3dDevice->SetPixelShaderConstantF( 1, (FLOAT*)&SurfaceDimensions, 1 );

        m_pd3dDevice->BeginExport( 0, Operation.pBaseTexture, D3DBEGINEXPORT_PIXELSHADER );

        // draw source rect
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, DrawVerts, sizeof(CopyVertex) );

        m_pd3dDevice->EndExport( 0, Operation.pBaseTexture, 0 );

        // restore renderstate
        m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );

        // if we are using a linear page, clear its pending flag
        if( Operation.pLinearPage != NULL )
        {
            Operation.pLinearPage->ClearPending();
        }

        m_pd3dDevice->SetTexture( 0, NULL );
        m_pd3dDevice->InsertFence();

        PIXEndNamedEvent();
    }

    //--------------------------------------------------------------------------------------
    // Name: PageRenderer::ExecuteMemExportIndexMapTexelOperation
    // Desc: Executes an operation which updates one texel in an index map texture.  A
    //       vertex shader memexport draw is used to directly write one texel within the
    //       index map texture.
    //--------------------------------------------------------------------------------------
    VOID PageRenderer::ExecuteMemExportIndexMapTexelOperation( const RenderOperation& Operation )
    {
        ASSERT( Operation.pResolveTexture != NULL );
        ASSERT( Operation.pExportBaseAddress != NULL );

        PIXBeginNamedEvent( 0, "Index Map Update" );

        D3DArrayTexture* pExportTexture = Operation.pResolveTexture;
        UINT BaseSizeBytes, MipSizeBytes;
        XGGetTextureLayout( pExportTexture, NULL, &BaseSizeBytes, NULL, NULL, 0, NULL, &MipSizeBytes, NULL, NULL, 0 );
        BaseSizeBytes += MipSizeBytes;

        GPU_MEMEXPORT_STREAM_CONSTANT ExportConstant;
        GPU_SET_MEMEXPORT_STREAM_CONSTANT( &ExportConstant,
                                           Operation.pExportBaseAddress,                    
                                           BaseSizeBytes / INDEXMAP_TEXEL_SIZE_BYTES,                                              
                                           SURFACESWAP_LOW_BLUE,                            
                                           GPUSURFACENUMBER_UINTEGER,                         
                                           GPUCOLORFORMAT_5_6_5,               
                                           GPUENDIAN128_8IN16 );

        m_pd3dDevice->SetRenderTarget( 0, m_pExportSurface );
        m_pd3dDevice->SetRenderTarget( 1, NULL );
        m_pd3dDevice->SetRenderTarget( 2, NULL );
        m_pd3dDevice->SetRenderTarget( 3, NULL );
        m_pd3dDevice->SetDepthStencilSurface( NULL );

        m_pd3dDevice->BeginExport( 0, Operation.pResolveTexture, D3DBEGINEXPORT_VERTEXSHADER );

        m_pd3dDevice->SetVertexDeclaration( m_pDeclCopyVertex );
        m_pd3dDevice->SetVertexShader( m_pVSExport );
        m_pd3dDevice->SetPixelShader( m_pPSCopyTex2D );

        m_pd3dDevice->SetVertexShaderConstantF( 0, ExportConstant.c, 1 );

        CopyVertex ExportVertex;
        // offset is in units of the export constant (16-bit texels)
        ExportVertex.Position.x = (FLOAT)( Operation.ResolveRect.x1 / INDEXMAP_TEXEL_SIZE_BYTES );
        ExportVertex.Position.y = 0;
        ExportVertex.TexCoord = Operation.FillColor;

        m_pd3dDevice->DrawVerticesUP( D3DPT_POINTLIST, 1, &ExportVertex, sizeof(ExportVertex) );

        m_pd3dDevice->EndExport( 0, Operation.pResolveTexture, 0 );

        m_pd3dDevice->InsertFence();

        PIXEndNamedEvent();
    }

    //--------------------------------------------------------------------------------------
    // Name: PageRenderer::GetUnusedPage
    // Desc: Scans the list of linear pages and returns one that isn't currently in use.
    //       If all of the linear pages are in use, a new linear page is created.
    //--------------------------------------------------------------------------------------
    LinearPage* PageRenderer::GetUnusedPage()
    {
        EnterCriticalSection( &m_LinearPageCritSec );

        INT FoundIndex = -1;
        UINT CurrentIndex = m_NextLinearPageIndex;
        do 
        {
            if( !m_LinearPages[CurrentIndex]->IsBusy() )
            {
                FoundIndex = CurrentIndex;
            }
            CurrentIndex = ( CurrentIndex + 1 ) % m_LinearPages.size();
        } while ( FoundIndex == -1 && CurrentIndex != m_NextLinearPageIndex );

        LinearPage* pReturnPage = NULL;

        if( FoundIndex != -1 )
        {
            m_NextLinearPageIndex = CurrentIndex;
            pReturnPage = m_LinearPages[FoundIndex];
        }
        else
        {
            m_NextLinearPageIndex = 0;

            // add a new page
            LinearPage* pNewLP = new LinearPage();
            m_LinearPages.push_back( pNewLP );

            pReturnPage = pNewLP;
        }

        LeaveCriticalSection( &m_LinearPageCritSec );
        return pReturnPage;
    }
}

