//--------------------------------------------------------------------------------------
// TerrainView.h
//
// Rendering code to draw a textured 3D heightfield, using tiled textures as inputs.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <xtl.h>
#include <xnamath.h>
#include "d3d9tiled.h"
#include "SceneObject.h"

struct TerrainVertex
{
    XMFLOAT3 Position;
    XMFLOAT2 TexUV;
};

//--------------------------------------------------------------------------------------
// Name: TerrainView
// Desc: Implements a height mapped 3D terrain, using three tiled textures to draw the terrain.
//       The terrain view implements not only its scene render, but also its residency
//       sample render as well.
//--------------------------------------------------------------------------------------
class TerrainView
{
protected:
    XMMATRIX m_matScaling;

    D3DDevice* m_pd3dDevice;
    D3DTiledResourceDevice* m_pTiledResourceDevice;
    D3DTilePool* m_pTilePool;

    // Pointer to the title residency manager:
    TitleResidencyManager* m_pResidencyManager;

    // Bound tiled textures:
    BoundTiledTexture m_DiffuseMapTexture;
    BoundTiledTexture m_NormalMapTexture;
    BoundTiledTexture m_HeightMapTexture;

    // Resource set ID for the combination of the height map, diffuse map, and normal map:
    ResourceSetID m_RSID;

    UINT m_QuadCount;
    SIZE m_DrawGridSize;

    FLOAT m_QuadLayoutConstant[4];
    FLOAT m_QuadUVTransformConstant[4];

    XMFLOAT4 m_LightDirectionWorld;
    XMFLOAT4 m_AmbientLight;

    // Shaders for terrain rendering:
    D3DVertexShader* m_pVSTerrain;
    D3DPixelShader* m_pPSTerrainRender;
    D3DPixelShader* m_pPSTerrainResidency;

    D3DVertexShader* m_pVSTransform;
    D3DPixelShader* m_pPSColor;
    D3DVertexDeclaration* m_pDeclTerrainEdges;
    D3DVertexBuffer* m_pVBWater;

public:
    TerrainView( D3DDevice* pd3dDevice, D3DTiledResourceDevice* pTiledResourceDevice, D3DTilePool* pTilePool, TitleResidencyManager* pResidencyManager );
    ~TerrainView();

    BOOL TexturesLoaded() const { return m_DiffuseMapTexture.Loaded() && m_NormalMapTexture.Loaded() && m_HeightMapTexture.Loaded(); }

    D3DTiledTexture* GetDiffuseTexture() const { return m_DiffuseMapTexture.pTexture; }
    D3DBaseTexture* GetDiffuseSamplingQualityTexture() const 
    { 
        if( m_DiffuseMapTexture.pSamplingQualityManager != NULL ) 
        { 
            return m_DiffuseMapTexture.pSamplingQualityManager->GetLODQualityTexture(); 
        } 
        else
        {
            return NULL; 
        }
    }

    VOID PreSceneRender( FLOAT fDeltaTime );
    VOID RenderResidencyView( XMMATRIX matView, XMMATRIX matProjection );
    VOID RenderScene( XMMATRIX matView, XMMATRIX matProjection );
};
