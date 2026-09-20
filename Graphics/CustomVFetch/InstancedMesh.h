//--------------------------------------------------------------------------------------
// InstancedMesh.h
//
// This file contains an instanced mesh system.  It takes as input an indexed mesh and
// instance data (which is updated regularly).  At render time, a special vertex shader
// combines the static mesh data with the instance data to render the mesh multiple
// times in the same draw call.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <xtl.h>
#include <fxl.h>


//--------------------------------------------------------------------------------------
// Name: struct InstanceData
// Desc: Describes one instance.  An array of this struct is used to describe multiple
//       instances.
//--------------------------------------------------------------------------------------
struct InstanceData
{
    XMFLOAT3 m_Position;
    FLOAT m_Scale;
};


//--------------------------------------------------------------------------------------
// Name: struct SourceMeshData
// Desc: Describes an indexed mesh with one vertex stream.
//--------------------------------------------------------------------------------------
struct SourceMeshData
{
    D3DVertexBuffer* m_pVB;
    D3DIndexBuffer* m_pIB;
    D3DVertexDeclaration* m_pVertexDecl;
    DWORD m_dwVertexStride;
    D3DPRIMITIVETYPE m_PrimitiveType;
};


//--------------------------------------------------------------------------------------
// Name: class IndexedMesh
// Desc: Renders multiple instances of a single mesh.
//--------------------------------------------------------------------------------------
class InstancedMesh
{
public:
            InstancedMesh();
            ~InstancedMesh();

    VOID    Initialize( const SourceMeshData& SourceMesh, DWORD dwMaxInstanceCount );
    VOID    SetInstanceData( InstanceData* pInstanceData, DWORD dwInstanceCount );
    HRESULT Render( const XMMATRIX& matWVP );
    HRESULT Render( const XMMATRIX& matWVP, DWORD dwIndexStart, DWORD dwIndexCount );

private:
    SourceMeshData m_SourceMesh;
    D3DVertexDeclaration* m_pInstancedMeshDecl;
    D3DVertexBuffer* m_pInstanceDataVB[2];
    D3DVertexBuffer* m_pMeshIndexDataVB;
    DWORD m_dwMaxInstanceCount;
    DWORD m_dwActiveInstanceCount;
    DWORD m_dwInstanceSize;
    DWORD m_dwFrameCount;
    FXLEffect* m_pEffect;
    FXLHANDLE m_hWVPMatrix;
    FXLHANDLE m_hInstanceData;
};
