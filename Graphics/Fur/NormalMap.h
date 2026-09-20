//--------------------------------------------------------------------------------------
// NormalMap.h
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// Copyright (C) Tomohide Kano. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once


//--------------------------------------------------------------------------------------
// Name: class NormalMap
// Desc: 
//--------------------------------------------------------------------------------------
class NormalMap
{
    XMVECTOR m_vPrevVel;
    XMVECTOR m_vAccel;
    XMVECTOR m_vPrevOmega;
    XMVECTOR m_vOmegaAccel;

    DWORD m_dwWidth;
    DWORD m_dwHeight;
    LPDIRECT3DSURFACE9 m_pUpdateRT;

    LPDIRECT3DTEXTURE9 m_pOffsetTexture;
    LPDIRECT3DVERTEXSHADER9 m_pUpdateOffsetVS;
    LPDIRECT3DPIXELSHADER9 m_pUpdateOffsetPS;
    LPDIRECT3DVERTEXDECLARATION9 m_pUpdateOffsetVertexDecl;

    HRESULT             InitTextures();
    HRESULT             InitShaders();

public:
    LPDIRECT3DTEXTURE9  GetOffsetMap()
    {
        return m_pOffsetTexture;
    }

    VOID                Update( FurMesh* pMesh, FLOAT dt, const XMVECTOR& vGravity,
                                const XMVECTOR& vVelocity, const XMVECTOR& vOmega );
    VOID                ResetInertia();
    HRESULT             Init( DWORD dwWidth, DWORD dwHeight );

                        NormalMap();
                        ~NormalMap();
};
