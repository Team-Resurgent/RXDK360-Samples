//--------------------------------------------------------------------------------------
// SamplingQualityManager.h
//
// The sampling quality manager maintains a "sampling quality map" for a single tiled 
// resource.  The sampling quality map is sized so that one texel represents one page of
// the tiled texture's base mip level (level 0).  Each texel's value in the sampling
// quality map represents a fractional minimum mip LOD level allowed at that region of
// the texture.  The value is animated over time as pages are mapped and unmapped in the
// tiled texture, which creates smooth mip level transitions during streaming.
//
// During scene rendering, the scene render pixel shader samples a value from the
// sampling quality map, returning a min LOD value.  Then, when sampling from the tiled
// texture, the sampling quality value is passed as a min LOD clamp value.  This prevents
// the tiled resource sample from sampling from a nonresident mip level of the texture.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <xtl.h>
#include "d3d9tiled.h"
#include "TitleResidencyManager.h"

// When a page is loaded that populates a lower mip level, the new mip value at that location
// is lerped to over this amount of time.
#define MIP_TRANSITION_TIME_SECONDS 0.25f

//--------------------------------------------------------------------------------------
// Name: SamplingQualityManager
// Desc: Class that tracks per-page mip residency for a single tiled texture.  2D and
//       2D array textures are supported.  The sampling quality manager listens to page
//       activity from the title residency manager to know when the sampling quality map
//       needs to be updated.
//--------------------------------------------------------------------------------------
class SamplingQualityManager : public ITileActivityHandler
{
protected:
    // The tiled texture2D that this sampling quality map tracks:
    D3DTiledTexture* m_pResource;
    D3DBaseTexture* m_pPageLODTexture;
    D3DArrayTexture* m_pPageLODArrayTexture;
    D3DSurface* m_pPageLODSurface;
    // Flag that is TRUE on the first update, FALSE otherwise:
    BOOL m_FirstFrame;
    XMFLOAT2 m_UVScaleRender;
    FLOAT m_UVScaleConstant[4];

    // The amount of time that it takes for a section of a new mip level to be lerped into view:
    FLOAT m_MipTransitionDuration;


    // The number of array slices in the tiled texture:
    UINT m_SliceCount;
    // A countdown timer for each array slice, representing the length of time from now that the
    // sampling quality map for each array slice needs to be continuously updated.
    // This allows the sampling quality manager to pause updating the sampling quality maps when
    // there is no page activity on the tiled texture:
    FLOAT* m_pSliceChangingTime;

public:
    SamplingQualityManager( D3DTiledTexture* pResource, D3DDevice* pd3dDevice );
    ~SamplingQualityManager();

    VOID Render( D3DDevice* pd3dDevice, D3DTiledResourceDevice* pTiledResourceDevice, FLOAT fDeltaTime );

    D3DBaseTexture* GetLODQualityTexture() const { return m_pPageLODTexture; }
    const FLOAT* GetUVScalingConstant() const { return m_UVScaleConstant; }

    virtual VOID TileLoaded( const TrackedTileID* pPageID );
    virtual VOID TileUnloaded( const TrackedTileID* pPageID );
};
