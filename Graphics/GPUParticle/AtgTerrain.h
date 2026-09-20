//-----------------------------------------------------------------------------
// AtgTerrain.h
//
// Defines the CAtgTerrain object, which provides an app-customizable terrain
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef ATGTERRAIN_H
#define ATGTERRAIN_H

#include <xtl.h>


typedef FLOAT __stdcall FNHEIGHTFUNCTION( FLOAT fX, FLOAT fZ );


//-----------------------------------------------------------------------------
// Name: class CAtgTerrain
// Desc: Class for a customizable terrain
//-----------------------------------------------------------------------------
class AtgTerrain
{
public:
            AtgTerrain();
            ~AtgTerrain();

    HRESULT Initialize( LPDIRECT3DDEVICE9 pd3dDevice );
    HRESULT Generate( DWORD dwXSlices, DWORD dwZSlices, XMFLOAT2 vXZMin, XMFLOAT2 vXZMax,
                      LPDIRECT3DTEXTURE9 pTexture, FNHEIGHTFUNCTION pfnHeight = NULL,
                      FLOAT fXRepeat = 1.0f, FLOAT fZRepeat = 1.0f );
    HRESULT Render();
    HRESULT RenderPlaneMap();
    HRESULT Terminate();

private:
    LPDIRECT3DDEVICE9 m_pd3dDevice;
    LPDIRECT3DTEXTURE9 m_pTexture;
    XMFLOAT2 m_vXZMin;
    XMFLOAT2 m_vXZMax;
    FLOAT m_fXTextureRepeat;
    FLOAT m_fZTextureRepeat;
    FNHEIGHTFUNCTION* m_pfnHeight;
    DWORD m_dwNumIndices;

    LPDIRECT3DVERTEXBUFFER9 m_pTerrainVB;
    LPDIRECT3DINDEXBUFFER9 m_pTerrainIB;

    LPDIRECT3DVERTEXDECLARATION9 m_pTerrainVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9 m_pTerrainVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pTerrainPixelShader;
    LPDIRECT3DVERTEXSHADER9 m_pTerrainHeightVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pTerrainHeightPixelShader;
};

#endif // ATGTERRAIN_H
