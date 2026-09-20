//--------------------------------------------------------------------------------------
// FurTexture.h
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// Copyright (C) Tomohide Kano. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once


//--------------------------------------------------------------------------------------
// Name: class FurTexture
// Desc: 
//--------------------------------------------------------------------------------------
class FurTexture
{
    DWORD m_dwSize;
    DWORD m_dwNumLayers;
    LPDIRECT3DTEXTURE9 m_pColorTexture;
    LPDIRECT3DTEXTURE9* m_ppLayerTextures;

public:
    LPDIRECT3DTEXTURE9  GetLayerTexture( FLOAT fLayer );
    LPDIRECT3DTEXTURE9  GetColorTexture();

    HRESULT             Init( DWORD dwSeed, DWORD dwSize, DWORD dwNumLayers );

                        FurTexture();
                        ~FurTexture();
};


extern HRESULT BuildFurLightingTexture( LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture );
