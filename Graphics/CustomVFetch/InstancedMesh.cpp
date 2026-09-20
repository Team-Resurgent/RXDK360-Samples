//--------------------------------------------------------------------------------------
// InstancedMesh.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "InstancedMesh.h"

#include <AtgApp.h>
#include <AtgUtil.h>
#include <assert.h>

//--------------------------------------------------------------------------------------
// Name: InstancedMesh constructor
//--------------------------------------------------------------------------------------
InstancedMesh::InstancedMesh()
{
    XMemSet( this, 0, sizeof( InstancedMesh ) );
}


//--------------------------------------------------------------------------------------
// Name: InstancedMesh destructor
//--------------------------------------------------------------------------------------
InstancedMesh::~InstancedMesh()
{
    m_pInstancedMeshDecl->Release();
    m_pInstanceDataVB[0]->Release();
    m_pInstanceDataVB[1]->Release();
    m_pMeshIndexDataVB->Release();
    m_pEffect->Release();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Stores the mesh data, and creates vertex buffers used during instancing.
//--------------------------------------------------------------------------------------
VOID InstancedMesh::Initialize( const SourceMeshData& SourceMesh, DWORD dwMaxInstanceCount )
{
    m_SourceMesh = SourceMesh;
    m_dwMaxInstanceCount = dwMaxInstanceCount;
    m_dwActiveInstanceCount = 0;
    m_dwFrameCount = 0;

    // Create a special vertex buffer to hold the mesh index buffer.
    D3DINDEXBUFFER_DESC IBDesc;
    m_SourceMesh.m_pIB->GetDesc( &IBDesc );
    DWORD dwIndexVBSize = IBDesc.Size;
    if( IBDesc.Format == D3DFMT_INDEX16 )
        dwIndexVBSize *= 2;
    HRESULT hr = ATG::g_pd3dDevice->CreateVertexBuffer( dwIndexVBSize, 0, 0, D3DPOOL_DEFAULT, &m_pMeshIndexDataVB,
                                                        NULL );

    // Compute instance size.
    DWORD dwIndexCount = dwIndexVBSize / sizeof( DWORD );
    m_dwInstanceSize = dwIndexCount;

    // Copy index buffer data to vertex buffer.
    DWORD* pIndexDataDest = NULL;
    hr = m_pMeshIndexDataVB->Lock( 0, 0, ( VOID** )&pIndexDataDest, 0 );
    DWORD* pIndexDataSrc = NULL;
    hr = m_SourceMesh.m_pIB->Lock( 0, 0, ( VOID** )&pIndexDataSrc, D3DLOCK_READONLY );
    if( IBDesc.Format == D3DFMT_INDEX32 )
    {
        memcpy( pIndexDataDest, pIndexDataSrc, dwIndexVBSize );
    }
    else
    {
        WORD* pIndexDataSrcWord = ( WORD* )pIndexDataSrc;
        for( DWORD i = 0; i < dwIndexCount; ++i )
        {
            pIndexDataDest[i] = ( DWORD )pIndexDataSrcWord[i];
        }
    }
    m_SourceMesh.m_pIB->Unlock();
    m_pMeshIndexDataVB->Unlock();

    // Synthesize new vertex declaration.
    D3DVERTEXELEMENT9 VertexElements[64];
    DWORD dwElementCount = ARRAYSIZE( VertexElements );
    m_SourceMesh.m_pVertexDecl->GetDeclaration( VertexElements, ( UINT* )&dwElementCount );
    assert( dwElementCount < ( ARRAYSIZE( VertexElements ) - 3 ) );

    static const D3DVERTEXELEMENT9 NewStreamElements[] =
    {
        // The index data is in stream 1 as UINTs.
        { 1,     0, D3DDECLTYPE_UINT1,      0,  D3DDECLUSAGE_POSITION,  1 },
        // The instance data is in stream 2 as FLOAT4s.
        { 2,     0, D3DDECLTYPE_FLOAT4,     0,  D3DDECLUSAGE_POSITION,  2 },
        D3DDECL_END()
    };

    // Add new stream decls for the index data and the instance data.
    XMemCpy( &VertexElements[ dwElementCount - 1 ],
             NewStreamElements,
             sizeof( NewStreamElements ) );

    // Create new vertex decl.
    hr = ATG::g_pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pInstancedMeshDecl );

    // Create instance data vertex buffers.
    DWORD dwInstanceDataSize = dwMaxInstanceCount * sizeof( InstanceData );
    hr = ATG::g_pd3dDevice->CreateVertexBuffer( dwInstanceDataSize,
                                                0, 0, D3DPOOL_DEFAULT,
                                                &m_pInstanceDataVB[0], NULL );
    hr = ATG::g_pd3dDevice->CreateVertexBuffer( dwInstanceDataSize,
                                                0, 0, D3DPOOL_DEFAULT,
                                                &m_pInstanceDataVB[1], NULL );

    // Load FXLite effect.
    BYTE* pEffectData = NULL;
    ATG::LoadFile( "game:\\media\\effects\\instancemesh.fxobj",
                   ( VOID** )&pEffectData, NULL );
    FXLCreateEffect( ATG::g_pd3dDevice, pEffectData, NULL, &m_pEffect );

    // Cache FXLite handles.
    m_hWVPMatrix = m_pEffect->GetParameterHandle( "world_view_proj_matrix" );
    m_hInstanceData = m_pEffect->GetParameterHandle( "instance_data" );
}


//--------------------------------------------------------------------------------------
// Name: SetInstanceData()
// Desc: Copies new instance data into the current instance data vertex buffer.
//--------------------------------------------------------------------------------------
VOID InstancedMesh::SetInstanceData( InstanceData* pInstanceData, DWORD dwInstanceCount )
{
    D3DVertexBuffer* pVB = m_pInstanceDataVB[ m_dwFrameCount % 2 ];
    InstanceData* pInstanceDataDest = NULL;
    dwInstanceCount = min( dwInstanceCount, m_dwMaxInstanceCount );
    pVB->Lock( 0, 0, ( VOID** )&pInstanceDataDest, 0 );
    memcpy( pInstanceDataDest, pInstanceData, dwInstanceCount * sizeof( InstanceData ) );
    pVB->Unlock();
    m_dwActiveInstanceCount = dwInstanceCount;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the mesh instances.  This function simply calls the detailed Render().
//--------------------------------------------------------------------------------------
HRESULT InstancedMesh::Render( const XMMATRIX& matWVP )
{
    return Render( matWVP, 0, m_dwInstanceSize );
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the mesh instances.  This function can render a subset of each
//       instanced mesh.
//--------------------------------------------------------------------------------------
HRESULT InstancedMesh::Render( const XMMATRIX& matWVP, DWORD dwIndexStart, DWORD dwIndexCount )
{
    // Input verification.
    assert( dwIndexStart < m_dwInstanceSize );
    assert( ( dwIndexStart + dwIndexCount ) <= m_dwInstanceSize );

    // Begin effect.
    m_pEffect->BeginTechniqueFromIndex( 0, 0 );
    m_pEffect->BeginPassFromIndex( 0 );

    // Set WVP matrix and instance data to effect.
    m_pEffect->SetMatrixF4x4A( m_hWVPMatrix, ( const FXLFLOATA* )&matWVP );
    XMFLOAT4 InstanceInfo( ( FLOAT )dwIndexCount, ( FLOAT )dwIndexStart, 0, 0 );
    m_pEffect->SetVectorF( m_hInstanceData, ( const FLOAT* )&InstanceInfo );

    m_pEffect->Commit();

    // Set the vertex streams and vertex decl.
    ATG::g_pd3dDevice->SetStreamSource( 0, m_SourceMesh.m_pVB, 0, m_SourceMesh.m_dwVertexStride );
    ATG::g_pd3dDevice->SetStreamSource( 1, m_pMeshIndexDataVB, 0, sizeof( DWORD ) );
    ATG::g_pd3dDevice->SetStreamSource( 2, m_pInstanceDataVB[ m_dwFrameCount % 2 ], 0, sizeof( InstanceData ) );
    ATG::g_pd3dDevice->SetVertexDeclaration( m_pInstancedMeshDecl );

    // Draw the instanced meshes.  Note that the index count is the number of instances
    // multiplied by the index count for the single mesh.
    ATG::g_pd3dDevice->DrawVertices( m_SourceMesh.m_PrimitiveType, 0, dwIndexCount * m_dwActiveInstanceCount );

    // Clear vertex stream so GPU will free up the vertex buffer for subsequent Lock()
    ATG::g_pd3dDevice->SetStreamSource( 2, NULL, 0, 0 );

    // End the effect.
    m_pEffect->EndPass();
    m_pEffect->EndTechnique();

    ++m_dwFrameCount;

    return S_OK;
}
