//-------------------------------------------------------------------------------------
// Environment.h
//  
// The Environment navigated in this sample.
//  
// Microsoft XNA Developer Connection
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef _ENVIRONMENT_H_
#define _ENVIRONMENT_H_

#include <xtl.h>
#include <xnamath.h>
#include <AtgResource.h>
#include <AtgSceneAll.h>


struct TerrainVertex;

class Environment
{
public:
    Environment();
    virtual ~Environment();

    HRESULT CreateGraphicsResources( IDirect3DDevice9* pd3dDevice, ATG::PackedResource& resource );
    FLOAT HeightAt( FLOAT X, FLOAT Y );
    VOID Draw( CXMMATRIX matView, CXMMATRIX matProj );
    VOID UpdateTrailLoc ( FLOAT fSpeed = 0.5f );
    XMFLOAT2 GetLocation ();
    XMFLOAT2 GetDirection ();

private: 
    
    VOID DrawModel( ATG::Scene* pScene );
    TerrainVertex* m_pVerticies; 
    INT m_iCurrentCoord;
    XMFLOAT2 m_fCurrentLocation;
    XMFLOAT2 m_fDir;

    //
    // Graphics resources
    //
    LPDIRECT3DDEVICE9 m_pd3dDevice;
    LPDIRECT3DVERTEXSHADER9 m_pVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pPixelShaderScene;
    LPDIRECT3DVERTEXBUFFER9 m_pTerrainBuffer;
    LPDIRECT3DINDEXBUFFER9 m_pTerrainIndexBuffer;

    LPDIRECT3DTEXTURE9 m_pTextureTerrain;
    LPDIRECT3DTEXTURE9 m_pTextureTrail;
    LPDIRECT3DTEXTURE9 m_pForestGroundTexture;
    LPDIRECT3DTEXTURE9 m_pWaterBumps;


};

#endif