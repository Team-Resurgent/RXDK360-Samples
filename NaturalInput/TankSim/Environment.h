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
    HRESULT LoadATGMesh( char* fileName, ATG::PackedResource& resource, ATG::Scene** pReturnedScene );

    FLOAT HeightAt( FLOAT X, FLOAT Y );
    VOID Draw( CXMMATRIX matView, CXMMATRIX matProj );
    VOID DrawUI( BOOL bHandOnLThrottle, BOOL bHandOnRThrottle, FLOAT fLThrottle, FLOAT fRThrottle, XMFLOAT2 vLHandVis, XMFLOAT2 vRHandVis  );
private: 
    
    VOID DrawModel( ATG::Scene* pScene );
    TerrainVertex* m_pVerticies;

    //
    // Graphics resources
    //
    LPDIRECT3DDEVICE9 m_pd3dDevice;
    LPDIRECT3DVERTEXSHADER9 m_pVertexShader;
    LPDIRECT3DVERTEXSHADER9 m_pVertexShaderTerrain;
    LPDIRECT3DPIXELSHADER9 m_pPixelShaderScene;
    LPDIRECT3DPIXELSHADER9 m_pPixelShaderThrottle;
    LPDIRECT3DVERTEXBUFFER9 m_pTerrainBuffer;
    LPDIRECT3DINDEXBUFFER9 m_pTerrainIndexBuffer;

    ATG::Scene* m_pControlBase;
    ATG::Scene* m_pControlThrottle;

    LPDIRECT3DTEXTURE9 m_pTexture;
    LPDIRECT3DTEXTURE9 m_pForestGroundTexture;

};

#endif