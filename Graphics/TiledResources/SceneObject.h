//--------------------------------------------------------------------------------------
// SceneObject.h
//
// This structure is a very lightweight and simple representation for a single object
// in the scene, including its geometry, world transform, and tiled surface texture.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <xtl.h>
#include <xnamath.h>
#include <vector>
#include "d3d9tiled.h"
#include "SamplingQualityManager.h"
#include "TitleResidencyManager.h"

//--------------------------------------------------------------------------------------
// Name: BoundTiledTexture
// Desc: Represents a combination of a tiled texture, its associated tile loader, its
//       sampling quality manager, and its shader resource view.
//--------------------------------------------------------------------------------------
struct BoundTiledTexture
{
    ITileLoader* pTileLoader;
    D3DTiledTexture* pTexture;
    SamplingQualityManager* pSamplingQualityManager;
    D3DTILED_SURFACE_DESC BaseLevelDesc;

    //--------------------------------------------------------------------------------------
    // Name: BoundTiledTexture constructor
    //--------------------------------------------------------------------------------------
    BoundTiledTexture()
    {
        ZeroMemory( this, sizeof(BoundTiledTexture) );
    }

    //--------------------------------------------------------------------------------------
    // Name: BoundTiledTexture::Loaded
    // Desc: Returns TRUE if this texture is loaded and valid, FALSE otherwise.
    //--------------------------------------------------------------------------------------
    BOOL Loaded() const
    {
        return pTexture != NULL && pSamplingQualityManager != NULL;
    }
};

// The maximum amount of tiled textures that can be sampled from in a single pass:
static const UINT MAX_TEXTURES_PER_OBJECT = 4;

//--------------------------------------------------------------------------------------
// Name: SceneObject
// Desc: Represents a single textured object that uses tiled resources for its surface
//       textures.
//--------------------------------------------------------------------------------------
struct SceneObject
{
    // World transform matrix:
    XMMATRIX matWorld;

    // Vertex and index buffer members for the object:
    UINT VertexStrideBytes;
    D3DVertexDeclaration* pVertexDeclaration;
    D3DVertexBuffer* pVertexBuffer;
    D3DIndexBuffer* pIndexBuffer;

    // Primitive topology members for the object:
    D3DPRIMITIVETYPE PrimitiveType;
    UINT PrimitiveCount;

    // Tiled texture members for the object:
    UINT TextureCount;
    BoundTiledTexture Textures[MAX_TEXTURES_PER_OBJECT];

    // Resource set ID that represents the set of textures mapped onto this object:
    ResourceSetID RSID;

    //--------------------------------------------------------------------------------------
    // Name: SceneObject constructor
    //--------------------------------------------------------------------------------------
    SceneObject()
    {
        ZeroMemory( this, sizeof(SceneObject) );
        matWorld = XMMatrixIdentity();
    }
};

typedef std::vector<SceneObject*> SceneObjectVector;
