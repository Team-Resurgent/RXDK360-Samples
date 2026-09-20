//--------------------------------------------------------------------------------------
// CBackdrop.h
//
// Simple backdrop
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// ATI 3D Application Research Group. 
// Copyright (C) ATI Research, Inc. All rights reserved. 
//--------------------------------------------------------------------------------------
#ifndef CBACKDROP_H
#define CBACKDROP_H


//-----------------------------------------------------------------------------
// Name: class CBackdrop
// Desc: Backdrop Class
//-----------------------------------------------------------------------------
class CBackdrop
{
    // Geometry
    UINT m_dwNumVertices;
    LPDIRECT3DVERTEXBUFFER9 m_pVB;

    // Pointers to externally-defined cookie, noise and shadow maps
    LPDIRECT3DTEXTURE9 m_pCookie;
    LPDIRECT3DTEXTURE9 m_pScrollingNoise;
    LPDIRECT3DTEXTURE9 m_pShadowMap;

    // Transforms
    FLOAT m_fFarPlane;
    XMMATRIX m_matWorldViewProj;

    XMMATRIX m_matWorldLight;
    XMMATRIX m_matLightProjBias;
    XMMATRIX m_matLightProjScroll1;
    XMMATRIX m_matLightProjScroll2;

    // Textures
    LPDIRECT3DTEXTURE9 m_pTexture;

public:
            CBackdrop();

    HRESULT Initialize( LPDIRECT3DTEXTURE9 pBackdropTexture );

    VOID    SetWorldViewProjMatrix( XMMATRIX matWorldViewProj );
    VOID    SetWorldLightMatrix( XMMATRIX matWorldLight );
    VOID    SetLightProjBiasMatrix( XMMATRIX matWorldLightProjBias );
    VOID    SetLightProjScrollMatrices( XMMATRIX matWorldLightProjScroll1,
                                        XMMATRIX matWorldLightProjScroll2 );
    VOID    SetTextureHandles( LPDIRECT3DTEXTURE9 pCookie,
                               LPDIRECT3DTEXTURE9 pScrollingNoise,
                               LPDIRECT3DTEXTURE9 pShadowMap );
    VOID    SetFarPlane( FLOAT fFarPlane );

    // Drawing routine
    HRESULT DrawDepthOnly();
    HRESULT Draw( BOOL bScrollingNoise, BOOL bShadowMapping );
};

#endif // CBACKDROP_H
