//-------------------------------------------------------------------------------------
// Road.h
//  
// The road driven on in this sample.
//  
// Microsoft XNA Developer Connection
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef _ROAD_H_
#define _ROAD_H_

#include <xtl.h>
#include <xnamath.h>
#include <AtgResource.h>


class Road
{
public:
    Road();
    virtual ~Road();

    HRESULT CreateGraphicsResources( IDirect3DDevice9* pd3dDevice, ATG::PackedResource& resource );

    VOID Draw( CXMMATRIX matView, CXMMATRIX matProj );

private: 
    VOID BuildRoad();
    
    static const DWORD s_dwNumRoadPoints = 1000;
    XMVECTOR m_vRoadVector[ s_dwNumRoadPoints ];

    //
    // Graphics resources
    //
    LPDIRECT3DDEVICE9 m_pd3dDevice;
    LPDIRECT3DTEXTURE9 m_pTexture;
    LPDIRECT3DVERTEXBUFFER9 m_pVB;
    LPDIRECT3DVERTEXDECLARATION9 m_pVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9 m_pVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pPixelShader;

};

#endif