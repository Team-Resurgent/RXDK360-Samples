//--------------------------------------------------------------------------------------
// AAMesh.h
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef AAMESH_H
#define AAMESH_H
#include "AtgMesh.h"


//--------------------------------------------------------------------------------------
// Info for each edge. The vertex order matches that of Face0.
// Exactly two faces must be adjacent to an edge.
//--------------------------------------------------------------------------------------
struct AA_MESH_EDGE
{
    WORD V[2];             // Vert indices for the edge
    DWORD Face0, Face1;    // Adjacent faces
};


//--------------------------------------------------------------------------------------
// Info for each face.
//--------------------------------------------------------------------------------------
struct AA_MESH_FACE
{
    XMVECTOR Plane;    // Plane of the face
    WORD V[3];         // Vert indices for the face
};


//--------------------------------------------------------------------------------------
// Struct with additional data for each mesh frame used to build the
// AA edges.
//--------------------------------------------------------------------------------------
struct AA_MESH_DATA
{
    // Data needed to quickly build the AA edges.
    DWORD m_dwNumFaces;
    AA_MESH_FACE* m_pFaces;

    DWORD m_dwNumEdges;
    AA_MESH_EDGE* m_pEdges;

    // Temporary data used for storing the current AA edges
    BOOL* m_pFrontFacing;

    // Interior AA edge verts
    DWORD m_dwNumInteriorEdgeVerts;
    D3DVertexBuffer* m_pInteriorEdgeVB;

    // Silhouette edge verts
    DWORD m_dwNumSilhouetteEdgeVerts;
    D3DVertexBuffer* m_pSilhouetteEdgeVB[2];

    // Vertices for the entire mesh
    XMFLOAT3* m_pVerts;
    DWORD m_dwNumVerts;

};

//--------------------------------------------------------------------------------------
// Name: Class CAAMesh
// Desc: Handles Edge Based Antialiasing for an ATG::Mesh.
//--------------------------------------------------------------------------------------
class CAAMesh 
{
public:
    CAAMesh();
    ~CAAMesh();

    // Build the data needed to quickly compute the AA Edges
    HRESULT         Create( ATG::Mesh* pMesh );

    // Set the view direction and recalculate the silhouette edges
    virtual VOID    SetViewDir( const XMVECTOR vViewDir );
    
    // Renders the AA Edges
    VOID            RenderAAMesh( );

    VOID            Update( FLOAT fElapsedTime ) { m_dwCurFrame = (m_dwCurFrame + 1 ) % 2;}

private:
    // Additional data from each ATG::MESH_FRAME
    AA_MESH_DATA*   m_pAAMeshData;

    // Functions to calculate AA data
    HRESULT         CalculateFrameAAData( ATG::MESH_FRAME* pFrame );

    HRESULT         CalculateMeshAAData( ATG::MESH_DATA* pMesh,
                                         AA_MESH_DATA* pMeshData );

    VOID            ComputeSilhouetteFrame( ATG::MESH_FRAME* pFrame );

    VOID            ComputeSilhouetteMesh( ATG::MESH_DATA* pMesh,
                                       AA_MESH_DATA* pMeshData );
    
    HRESULT BuildAAMesh( const DWORD dwNumFaces,    // Number of faces
                         const WORD* pMeshIndices,  // Input triangle indices
                         const BYTE* pMeshVertices, // Input vertices
                         DWORD dwVertexSize,        // Size of each vertex in bytes
                         DWORD& dwTempCount,        // Number of output vertices
                         XMFLOAT3* pTempVerts,      // Output vertices 
                         DWORD& dwNumEdges,         // Number of output edges
                         AA_MESH_EDGE* pEdges,      // Output edges 
                         AA_MESH_FACE* pFaces,      // Output faces 
                         AA_MESH_DATA* pMeshData );

    // Renders the AA Edges
    virtual VOID    RenderAAMesh( ATG::MESH_DATA* pMesh, DWORD index );
    virtual VOID    RenderAAMeshFrame( ATG::MESH_FRAME* pFrame );

public:
    // Pointer to the original mesh to anti-alias
    ATG::Mesh* m_pMesh;

    D3DVertexDeclaration* m_pAAVertexDecl;
    D3DVertexShader* m_pVS;
    D3DPixelShader* m_pPS;
    D3DTexture* m_pFrameBuffer;

    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    XMVECTOR m_vViewDir;

    DWORD m_dwCurFrame;
};

#endif // AAMESH_H
