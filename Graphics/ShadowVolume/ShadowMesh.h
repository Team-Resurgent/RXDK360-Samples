//--------------------------------------------------------------------------------------
// ShadowMesh.h
//
// Support code for building and rendering shadow volumes. Shadow volumes can be built
// either on the CPU or on the GPU, and can be rendered using either a one-pass or
// two-pass technique.
//
// In the two-pass case, the geometry must be transformed twice, but the rasterization
// cost is slightly less. Since rendering shadow volumes tends to be fill-bound (in
// other words, the transform cost is masked by the fill cost), the two-pass case
// usually works out to be faster.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef SHADOWMESH_H
#define SHADOWMESH_H
#include "AtgMesh.h"


//--------------------------------------------------------------------------------------
// Info for each edge. The vertex order matches that of Face0.
// Exactly two faces must be adjacent to an edge.
//--------------------------------------------------------------------------------------
struct SHADOWMESH_EDGE
{
    WORD V[2];             // Vert indices for the edge.
    DWORD Face0, Face1;     // Adjacent faces.
};


//--------------------------------------------------------------------------------------
// Info for each face.
//--------------------------------------------------------------------------------------
struct SHADOWMESH_FACE
{
    XMVECTOR Plane;        // Plane of the face.
    WORD V[3];         // Vert indices for the face.
};


//--------------------------------------------------------------------------------------
// Struct with additional data for each mesh frame used to build the
// shadow volume.
//--------------------------------------------------------------------------------------
struct MESH_CPUSHADOW_DATA
{
    // Data needed to quickly build the shadow volume.
    DWORD m_dwNumFaces;
    SHADOWMESH_FACE* m_pFaces;

    DWORD m_dwNumEdges;
    SHADOWMESH_EDGE* m_pEdges;

    DWORD m_dwNumVertices;
    LPDIRECT3DVERTEXBUFFER9 m_pVB;

    // Temporary data used for storing the current shadow volume.
    BOOL* m_pFrontFacing;

    DWORD m_dwNumTriIndices;
    D3DIndexBuffer* m_pTriIndices;

    DWORD m_dwNumQuadIndices;
    D3DIndexBuffer* m_pQuadIndices;
};


//--------------------------------------------------------------------------------------
// Struct with additional data for each mesh frame used to build the shadow volume.
//--------------------------------------------------------------------------------------
struct MESH_GPUSHADOW_DATA
{
    DWORD m_dwNumVertices;
    LPDIRECT3DVERTEXBUFFER9 m_pVB;

    DWORD m_dwNumQuadIndices;
    D3DIndexBuffer* m_pQuadIndices;
};


//--------------------------------------------------------------------------------------
// Name: class CShadowMesh 
// Desc: A subclass of ATG::Mesh that has extra data to quickly build shadow volumes.
//--------------------------------------------------------------------------------------
class CShadowMesh
{
public:
    // Pointer to the original mesh that casts this shadow
    ATG::Mesh* m_pMesh;

    D3DVertexDeclaration* m_pShadowVolumeVertexDecl;
    D3DVertexShader* m_pShadowVolumeVertexShader;

    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    XMVECTOR m_vLightPos;

    // Internal rendering functions
    virtual VOID    RenderVolumeFrame( ATG::MESH_FRAME* pFrame );
    virtual VOID    RenderVolumeMesh( ATG::MESH_DATA* pMesh, DWORD index ) = 0;

public:
    // Renders the shadow volume for the mesh.
    VOID            RenderVolume();
    virtual VOID    SetLightPos( const XMVECTOR vLightPos ) = 0;

                    CShadowMesh()
                    {
                    };
                    ~CShadowMesh()
                    {
                    };
};


//--------------------------------------------------------------------------------------
// Name: class CShadowMeshCPU
// Desc: A subclass of ATG::Mesh that has extra data to quickly build shadow volumes.
//--------------------------------------------------------------------------------------
class CShadowMeshCPU : public CShadowMesh
{
    // Additional data from each ATG::MESH_FRAME.
    // Basically a parallel array to m_pMeshFrames.
    MESH_CPUSHADOW_DATA* m_pMeshShadowData;

    // Functions to calculate shadow data
    HRESULT         CalculateFrameShadowData( ATG::MESH_FRAME* pFrame );

    HRESULT         CalculateMeshShadowData( ATG::MESH_DATA* pMesh,
                                             MESH_CPUSHADOW_DATA* pShadowData );

    VOID            ComputeVolumeFrame( ATG::MESH_FRAME* pFrame );

    VOID            ComputeVolumeMesh( ATG::MESH_DATA* pMesh,
                                       MESH_CPUSHADOW_DATA* pShadowData );

    // Internal rendering functions
    virtual VOID    RenderVolumeMesh( ATG::MESH_DATA* pMesh, DWORD index );

public:
    // Build the data needed to quickly compute the shadow volume.
    HRESULT         Create( ATG::Mesh* pMesh );

    // Renders the shadow volume for the mesh.
    virtual VOID    SetLightPos( const XMVECTOR vLightPos );

                    CShadowMeshCPU();
                    ~CShadowMeshCPU();
};


//--------------------------------------------------------------------------------------
// Name: class CShadowMeshGPU
// Desc: A subclass of ATG::Mesh that has extra data to render shadow volumes entirely 
//       using the GPU.
//--------------------------------------------------------------------------------------
class CShadowMeshGPU : public CShadowMesh
{
    // Additional data from each ATG::MESH_FRAME.
    // Basically a parallel array to m_pMeshFrames.
    MESH_GPUSHADOW_DATA* m_pMeshShadowData;

    // Functions to calculate shadow data
    HRESULT         CalculateFrameShadowData( ATG::MESH_FRAME* pFrame );

    HRESULT         CalculateMeshShadowData( ATG::MESH_DATA* pMesh,
                                             MESH_GPUSHADOW_DATA* pShadowData );

    // Internal rendering functions
    virtual VOID    RenderVolumeMesh( ATG::MESH_DATA* pMesh, DWORD index );

public:
    // Build the data needed to compute the shadow volume on the GPU.
    HRESULT         Create( ATG::Mesh* pMesh );

    // Renders the shadow volume for the mesh.
    virtual VOID    SetLightPos( const XMVECTOR vLightPos );

                    CShadowMeshGPU();
                    ~CShadowMeshGPU();
};


#endif // SHADOWMESH_H
