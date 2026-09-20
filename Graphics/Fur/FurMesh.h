//--------------------------------------------------------------------------------------
// File: FurMesh.h
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// Copyright (C) Tomohide Kano. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once


typedef XMVECTOR (*MESH_FUNC )( FLOAT, FLOAT );

struct MESH_PARAMETERS
{
    MESH_FUNC fnPosition;
    DWORD dwNumTilesU;
    DWORD dwNumTilesV;
    FLOAT fFurScaleU;
    FLOAT fFurScaleV;
    const WCHAR* strName;
};


//--------------------------------------------------------------------------------------
// Name: class FurMesh
// Desc: 
//--------------------------------------------------------------------------------------
class FurMesh
{
    const WCHAR* m_strName;
    DWORD m_dwNumVertices;
    DWORD m_dwNumIndices;
    LPDIRECT3DVERTEXDECLARATION9 m_pVertexDecl;
    LPDIRECT3DVERTEXBUFFER9 m_pVertexBuffer;
    LPDIRECT3DINDEXBUFFER9 m_pIndexBuffer;

public:
    const WCHAR* GetName()
    {
        return m_strName;
    }
    VOID    Draw();
    HRESULT Init( const MESH_PARAMETERS& param );

            FurMesh();
            ~FurMesh();
};
