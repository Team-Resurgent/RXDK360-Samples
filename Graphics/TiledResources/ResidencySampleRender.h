//--------------------------------------------------------------------------------------
// ResidencySampleRender.h
//
// A series of methods to assist in creating a residency sample rendering of a vector of 
// scene objects.  A residency sample render is a special view of the scene that records
// per-pixel UV, UV gradient, and resource ID information that is used to determine which 
// virtual pages of which resources are currently visible.  This information is processed
// by the title residency manager, which streams pages in and out of memory as needed to
// render the scene at the proper texel resolution.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <xtl.h>
#include <xboxmath.h>
#include <AtgSceneAll.h>

#include "d3d9tiled.h"

#include "SceneObject.h"
#include "TitleResidencyManager.h"
#include "SamplingQualityManager.h"

//--------------------------------------------------------------------------------------
// Name: ResidencySampleRender
// Desc: Namespace that includes the residency sample render methods.
//--------------------------------------------------------------------------------------
namespace ResidencySampleRender
{
    VOID Initialize( D3DDevice* pd3dDevice );

    UINT BeginScene( D3DDevice* pd3dDevice, TitleResidencyManager* pResidencyManager );
    VOID EndScene( UINT BeginSceneID, D3DDevice* pd3dDevice, TitleResidencyManager* pResidencyManager );
    VOID SetPixelShader( D3DDevice* pd3dDevice, TitleResidencyManager* pResidencyManager, ResourceSetID RSID );

    VOID Render( D3DDevice* pd3dDevice, TitleResidencyManager* pResidencyManager, const SceneObjectVector& SceneObjects, XMMATRIX matView, XMMATRIX matProjection );
}
