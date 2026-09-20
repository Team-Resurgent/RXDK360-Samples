//--------------------------------------------------------------------------------------
// PageRenderer.h
//
// A deferred execution queue that performs resource manipulation operations.  Most
// operations manipulate texels within a page pool atlas texture.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "TiledResourceCommon.h"

namespace TiledRuntime
{
    //--------------------------------------------------------------------------------------
    // Name: LinearPage
    // Desc: A class that points a series of typed texture headers at a single 64KB buffer.
    //       Each texture header describes a linear texture that is the size of one tiled
    //       resource page of that format.
    //--------------------------------------------------------------------------------------
    class LinearPage
    {
    protected:
        // A 64KB buffer:
        VOID* m_pPageBuffer;

        // One texture header for each page data format:
        D3DTexture* m_pPageTexture[PDF_COUNT];

        // A pointer to the last texture header used, for resource fence tracking purposes:
        D3DTexture* m_pLastUsedTexture;

        // Indicates that this linear page is waiting to be submitted to the GPU or consumed by the GPU:
        BOOL m_Pending;

    public:
        LinearPage();
        ~LinearPage();

        VOID CopyToPage( const VOID* pBuffer );
        D3DTexture* GetTexture( PageDataFormat DataFormat );

        BOOL IsBusy();

        VOID SetPending() { m_Pending = TRUE; }
        VOID ClearPending() { m_Pending = FALSE; }

        D3DRECT GetFullRect( PageDataFormat DataFormat ) const;
    };

    //--------------------------------------------------------------------------------------
    // Name: struct RenderOperation
    // Desc: Describes a deferred operation on a texture resource.
    //--------------------------------------------------------------------------------------
    struct RenderOperation
    {
        // set the surface
        D3DSurface* pSurface;

        // draw from the base array texture, from the given slice, in the given rect
        D3DArrayTexture* pBaseTexture;
        UINT BaseSliceIndex;
        D3DRECT BaseRect;

        // draw from the src texture, from the given rect, to the draw rect
        D3DTexture* pSrcTexture;
        D3DArrayTexture* pSrcArrayTexture;
        UINT SrcSliceIndex;
        D3DRECT SrcRect;
        D3DRECT DrawRect;

        // resolve to the resolve texture, using the resolve rect
        D3DArrayTexture* pResolveTexture;
        UINT ResolveSliceIndex;
        D3DRECT ResolveRect;

        // CPU address of memexport resolve destination
        VOID* pExportBaseAddress;

        // memexport fill color
        XMFLOAT4 FillColor;

        // linear page for busy tracking
        LinearPage* pLinearPage;

        RenderOperation()
        {
            ZeroMemory( this, sizeof(RenderOperation) );
        }
    };

    //--------------------------------------------------------------------------------------
    // Name: PageRenderer
    // Desc: A singleton class that manages a queue of pending operations on texture
    //       resources, and exposes the methods that perform those operations at the desired
    //       time.
    //--------------------------------------------------------------------------------------
    class PageRenderer
    {
    protected:
        D3DDevice* m_pd3dDevice;

        D3DVertexDeclaration* m_pDeclCopyVertex;
        D3DVertexShader* m_pVSPassthru;
        D3DPixelShader* m_pPSCopyColor;
        D3DPixelShader* m_pPSCopyTex2D;
        D3DPixelShader* m_pPSCopyTexArray;
        D3DVertexShader* m_pVSExport;
        D3DPixelShader* m_pPSExportTex2D;
        D3DPixelShader* m_pPSExportTexArray;
        D3DPixelShader* m_pPSExportColor;

        std::vector<LinearPage*> m_LinearPages;
        UINT m_NextLinearPageIndex;
        CRITICAL_SECTION m_LinearPageCritSec;

        D3DSurface* m_pRenderSurfaces[PDF_COUNT];
        D3DSurface* m_pExportSurface;

        std::deque<RenderOperation> m_PendingOperations;
        CRITICAL_SECTION m_QueueCritSec;

    public:
        PageRenderer( D3DDevice* pd3dDevice, UINT ExpectedPageUpdatesPerFrame );
        ~PageRenderer(void);

        BOOL UpdatesPending() const { return !m_PendingOperations.empty(); }
        VOID FlushPendingUpdates();

        HRESULT QueuePageUpdate( TypedPagePool* pPagePool, INT PageIndex, const VOID* pPageBuffer );
        HRESULT QueueBorderUpdate( TypedPagePool* pPagePool, PhysicalPageID CenterPageID, PhysicalPageID BorderPageID, PageNeighbors RelationshipToCenterPage, BOOL InvertSourceRelationship = FALSE );
        HRESULT QueueIndexMapUpdate( TiledResourceBase* pResource, VirtualPageID VPageID, PhysicalPageID PageID, INT PoolIndex );

    protected:
        VOID CreateShaders();

        LinearPage* GetUnusedPage();
        D3DSurface* GetSurface( PageDataFormat DataFormat ) const { return m_pRenderSurfaces[DataFormat]; }
        VOID QueueOperation( RenderOperation& Operation );
        VOID ExecuteTextureOperation( const RenderOperation& Operation );
        VOID ExecuteMemExportIndexMapTexelOperation( const RenderOperation& Operation );
        VOID ExecuteMemExportTextureOperation( const RenderOperation& Operation );
    };
}

