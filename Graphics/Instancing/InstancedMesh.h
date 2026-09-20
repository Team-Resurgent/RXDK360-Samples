//--------------------------------------------------------------------------------------
// InstancedMesh.h
//
// This file contains an instanced mesh system.  It performs instancing in several 
// different ways.  Performance of each of the methods can be compared by switching
// between the methods at runtime.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef INSTANCEDMESH_H
#define INSTANCEDMESH_H

#include <xtl.h>
#include <fxl.h>
#include <AtgSimpleShaders.h>


//--------------------------------------------------------------------------------------
// Name: struct InstanceData
// Desc: Describes one instance.  An array of this struct is used to describe multiple
//       instances.
//--------------------------------------------------------------------------------------
struct InstanceData
{
    XMFLOAT3 m_vPosition;
    FLOAT m_fScale;
};

//--------------------------------------------------------------------------------------
// Name: enum InstancingMethod
// Desc: Declares the different instancing methods
//--------------------------------------------------------------------------------------
enum InstancingMethod
{
    ONEPERDRAW_VFETCH   = 0,
    ONEPERDRAW_GPUCONST,
    CUSTOM_VFETCH,
    BATCHED_VFETCH,
    BATCHED_GPUCONST,
    XPS,
    TESSELLATOR_VFETCH,
    TESSELLATOR_GPUCONST,
    NUM_INSTANCING_METHODS,
    FORCE_DWORD         = 0xFFFFFFFF,
};

//--------------------------------------------------------------------------------------
// Name: struct SourceMeshData
// Desc: Describes a mesh
//--------------------------------------------------------------------------------------
struct SourceMeshData
{
    D3DVertexBuffer* pVB;
    D3DIndexBuffer* pIB;
    D3DVertexDeclaration* pDecl;
    DWORD dwStride;
    D3DPRIMITIVETYPE primType;
    DWORD dwNumIndices;
};

//--------------------------------------------------------------------------------------
// Name: class InstancedMesh
// Desc: Functionality for rendering multiple instances of a single mesh in various ways
//--------------------------------------------------------------------------------------
class InstancedMesh
{
public:
            InstancedMesh();
            ~InstancedMesh();

    VOID    Initialize( const SourceMeshData& SourceMesh, DWORD dwMaxInstanceCount );
    VOID    Destroy();

    VOID    SetInstanceData( const InstanceData* pInstanceData, DWORD dwInstanceCount );

    VOID    RenderOneInstPerDraw( const XMMATRIX& matWVP, BOOL useVFetch );
    VOID    RenderCustomVFetch( const XMMATRIX& matWVP );
    DWORD   RenderBatches( const XMMATRIX& matWVP, DWORD numInstsPerBatch, BOOL useVFetch );
    VOID    RenderUsingXPS( const XMMATRIX& matWVP );
    DWORD   RenderUsingTessellator( const XMMATRIX& matWVP, DWORD dwNumInstancesPerBatch,
                                    DWORD dwTessellationLevel, BOOL bVFetchData );

    DWORD   GetRenderDataSize() const
    {
        return m_dwRenderDataSize;
    }
    DWORD   GetNumVertsPerInst() const
    {
        return m_dwNumVertsPerInst;
    }
    DWORD   GetNumIndicesPerInst() const
    {
        return m_dwNumIndicesPerInst;
    }
    WORD* GetXPSIndexData() const
    {
        return m_pXPSIndexData;
    }
    ATG::MeshVertexPT* GetXPSVertexData() const
    {
        return m_pXPSVertexData;
    }

private:
    VOID    PreRender( InstancingMethod instMethod, const XMMATRIX& matWVP ) const;
    VOID    PostRender();
    VOID    InitializeTessellation();
    VOID    InitializeXPS();
    VOID    CreateIndexVB();
    VOID    CreateInstancedMeshDecl();
    VOID    CreateInstanceDataVB( DWORD dwMaxInstanceCount );
    VOID    SetSourceMeshData( const SourceMeshData& sourceMesh );
    VOID    InitializeFX();

    VOID    UpdateBatchedIndexBuffer( DWORD dwNumIndicesPerBatch );

private:
    SourceMeshData m_SourceMesh;
    const InstanceData* m_pInstData;

    D3DVertexBuffer* m_pInstDataVB[ 2 ]; // Double buffered to avoid lock stalls
    D3DVertexBuffer* m_pMeshIndexDataVB; // Index data for the mesh kept in a VB
    D3DIndexBuffer* m_pBatchedIB;       // Index buffer used in rendering batches

    FXLEffect* m_pEffect;
    FXLHANDLE m_hWVPMatrix;
    FXLHANDLE m_hInstanceParams;
    FXLHANDLE m_hInstanceData;

    D3DVertexDeclaration* m_pInstancedMeshDecl;
    D3DVertexDeclaration* m_pTessellatedInstanceDecl;
    D3DVertexDeclaration* m_pXPSVertexDecl;

    DWORD m_dwMaxInstanceCount;
    DWORD m_dwInstanceCount;
    DWORD m_dwFrameCt;            // Elapsed frames

    DWORD m_dwNumInstsPerBatch;   // Num instances to render per batch

    // counters to keep track of rendering data size
    DWORD m_dwMeshIndexDataVBSize;
    DWORD m_dwTesselatedInstanceVBSize;
    DWORD m_dwXPSVertexDataSize;
    DWORD m_dwXPSIndexDataSize;
    DWORD m_dwBatchedIBSize;
    DWORD m_dwSourceMeshVBSize;
    DWORD m_dwSourceMeshIBSize;
    DWORD m_dwInstDataVBSize;
    DWORD m_dwInstDataSize;

    // The size of the data needed to render the last used method
    DWORD m_dwRenderDataSize;

    WORD* m_pXPSIndexData;   // Index Data used in XPS rendering
    ATG::MeshVertexPT* m_pXPSVertexData;  // Vertex Data used in XPS rendering

    DWORD m_dwNumIndicesPerInst;
    DWORD m_dwNumVertsPerInst;

};

#endif
