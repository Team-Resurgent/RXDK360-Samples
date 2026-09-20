//--------------------------------------------------------------------------------------
// CVolVizShells.h
//
// Shells for visualizing Light Shafts 
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// ATI 3D Application Research Group. 
// Copyright (C) ATI Research, Inc. All rights reserved. 
//--------------------------------------------------------------------------------------
#ifndef CVOLVIZSHELLS_H
#define CVOLVIZSHELLS_H


//--------------------------------------------------------------------------------------
// Name: class CVolVizShells
// Desc: Class for rendering volviz shells for light shaft rendering
//--------------------------------------------------------------------------------------
class CVolVizShells
{
    // Geometry
    XMVECTOR m_vMinBounds;
    XMVECTOR m_vMaxBounds;
    FLOAT m_fSamplingDelta;

    DWORD m_dwNumShells;
    LPDIRECT3DVERTEXBUFFER9 m_pVB;

    // Light parameters
    FLOAT m_fFarPlane;
    FLOAT       m_Pad1[3];

    XMMATRIX m_matWorldViewProj;
    XMMATRIX m_matWorldLightProj;
    XMMATRIX m_matWorldLight;
    XMMATRIX m_matLightProjBias;
    XMMATRIX m_matLightProjScroll1;
    XMMATRIX m_matLightProjScroll2;

    XMVECTOR    m_ClipSpaceLightFrustumPlanes[6];

    // Textures
    LPDIRECT3DTEXTURE9 m_pCookie;
    LPDIRECT3DTEXTURE9 m_pScrollingNoise;
    LPDIRECT3DTEXTURE9 m_pShadowMap;

public:
                CVolVizShells();

    HRESULT     Initialize();

    // Setters
    VOID        SetVolVizBounds( XMVECTOR vMinBounds, XMVECTOR vMaxBounds, FLOAT fSamplingDelta );
    VOID        SetClipPlanes( XMVECTOR* pClipSpaceFrustumPlanes );
    VOID        SetWorldViewProjMatrix( XMMATRIX matWorldViewProj );
    VOID        SetWorldLightProjMatrix( XMMATRIX matWorldLightProj );
    VOID        SetWorldLightMatrix( XMMATRIX matWorldLight );
    VOID        SetLightProjBiasMatrix( XMMATRIX matLightProjBias );
    VOID        SetLightProjScrollMatrices( XMMATRIX matLightProjScroll1,
                                            XMMATRIX matLightProjScroll2 );
    VOID        SetTextures( LPDIRECT3DTEXTURE9 pCookie, LPDIRECT3DTEXTURE9 pScrollingNoise,
                             LPDIRECT3DTEXTURE9 pShadowMap );
    VOID        SetFarPlane( FLOAT fFarPlane );

    // Drawing routines
    HRESULT     Draw( BOOL bScrollingNoise, BOOL bShadowMapping );
};


#endif // CVOLVIZSHELLS_H
