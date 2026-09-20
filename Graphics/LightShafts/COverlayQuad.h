//--------------------------------------------------------------------------------------
// COverlayQuad.h
//
// Simple quad overlay for displaying textures 
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// ATI 3D Application Research Group. 
// Copyright (C) ATI Research, Inc. All rights reserved. 
//--------------------------------------------------------------------------------------
#ifndef COVERLAYQUAD_H
#define COVERLAYQUAD_H


//--------------------------------------------------------------------------------------
// Name: class COverlayQuad
// Desc: 
//--------------------------------------------------------------------------------------
class COverlayQuad
{
protected:
    // Geometry
    static LPDIRECT3DVERTEXBUFFER9 m_pOverlayVB;
    static LPDIRECT3DVERTEXDECLARATION9 m_pOverlayVertexDecl;
    static LPDIRECT3DVERTEXSHADER9 m_pOverlayVS;

public:
            COverlayQuad();

    HRESULT Initialize();
};


//--------------------------------------------------------------------------------------
// Name: class CShadowOverlayQuad
// Desc: 
//--------------------------------------------------------------------------------------
class CShadowOverlayQuad : public COverlayQuad
{
    static LPDIRECT3DPIXELSHADER9 m_pShadowBlurPS;

public:
            CShadowOverlayQuad();

    HRESULT Initialize();

    // Drawing routines
    HRESULT Draw( LPDIRECT3DTEXTURE9 pShadowMap );
};


//--------------------------------------------------------------------------------------
// Name: class CFogOverlayQuad
// Desc: 
//--------------------------------------------------------------------------------------
class CFogOverlayQuad : public COverlayQuad
{
    static LPDIRECT3DPIXELSHADER9 m_pCompositeFilteredFogPS;

public:
            CFogOverlayQuad();

    HRESULT Initialize();

    // Drawing routines
    HRESULT Draw( LPDIRECT3DTEXTURE9 pFogTexture );
};

#endif // COVERLAYQUAD_H
