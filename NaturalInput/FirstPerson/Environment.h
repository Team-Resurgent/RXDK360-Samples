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

class Environment
{
public:
    Environment();
    virtual ~Environment();

    HRESULT CreateGraphicsResources( IDirect3DDevice9* pd3dDevice, ATG::PackedResource& resource );

    VOID Draw( CXMMATRIX matView, CXMMATRIX matProj, XMFLOAT3 vOrientation );
    VOID DrawUI ( XMFLOAT3 vOrientation, FLOAT fForwardSpeed, FLOAT fStrafeSpeed, D3DCOLOR dwWheelColor );
private: 
    
    //
    // Graphics resources
    //
    LPDIRECT3DDEVICE9 m_pd3dDevice;
    LPDIRECT3DVERTEXSHADER9 m_pVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pPixelShaderSceme;
    LPDIRECT3DPIXELSHADER9 m_pPixelShaderWheel;

    ATG::Scene* m_pScene;
    ATG::Scene* m_pSkyDome;
    ATG::Scene* m_pWheel;

    LPDIRECT3DTEXTURE9 m_pFeetTexture;


};

#endif