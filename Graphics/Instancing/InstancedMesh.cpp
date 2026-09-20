//--------------------------------------------------------------------------------------
// InstancedMesh.cpp
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "InstancedMesh.h"

#include <AtgApp.h>
#include <assert.h>
#include <AtgUtil.h>

const DWORD MAX_GPU_CONST_BATCH_SIZE = 200;

// vertex struct for XPS Rendering
struct XPSMeshVertex
{
public:
    XMFLOAT3 m_vPosition;
    XMFLOAT2 m_vTexCoord;
    FLOAT m_fScale;
    XMFLOAT3 m_vNormal;
    static size_t Size()
    {
        return sizeof( XPSMeshVertex );
    }
};

// per instance data
struct InstanceSeed
{
    XMFLOAT3 m_vPosition;
    FLOAT m_fScale;
};

VOID XPSCallback( D3DXpsThread* __restrict pThreadContext,
                  VOID*         __restrict pCallbackContext,
                  const VOID*   __restrict pSubmitData,
                  DWORD                    dwInstanceIndex );

inline VOID GenerateInstanceVertexData( ATG::MeshVertexPT*  __restrict pSrcVertexData,
                                        XPSMeshVertex*      __restrict pDestVertexData,
                                        const InstanceSeed* __restrict pSeed,
                                        DWORD               dwNumVertsPerInst );

LONG g_nXPStime_curFrame;
LONG g_nXPStime_prevFrame;
//--------------------------------------------------------------------------------------
// Name: InstancedMesh constructor
//--------------------------------------------------------------------------------------
InstancedMesh::InstancedMesh()
{
}


//--------------------------------------------------------------------------------------
// Name: InstancedMesh destructor
//--------------------------------------------------------------------------------------
InstancedMesh::~InstancedMesh()
{
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Stores the mesh data and creates vertex buffers used during instancing.
//--------------------------------------------------------------------------------------
VOID InstancedMesh::Initialize( const SourceMeshData& sourceMesh, DWORD dwMaxInstCount )
{
    // Wait for the GPU to finish before modifying anything
    ATG::g_pd3dDevice->BlockUntilIdle();

    ZeroMemory( this, sizeof( InstancedMesh ) );
    m_pXPSVertexData = NULL;
    m_pXPSIndexData = NULL;
    m_dwNumIndicesPerInst = 0;
    m_dwNumVertsPerInst = 0;

    ATG::g_pd3dDevice->SetStreamSource( 0, NULL, 0, 0 );
    ATG::g_pd3dDevice->SetStreamSource( 1, NULL, 0, 0 );
    ATG::g_pd3dDevice->SetStreamSource( 2, NULL, 0, 0 );
    ATG::g_pd3dDevice->SetIndices( NULL );
    ATG::g_pd3dDevice->SetVertexDeclaration( NULL );

    m_dwMaxInstanceCount = dwMaxInstCount;
    m_dwInstanceCount = 0;
    m_dwFrameCt = 0;
    m_pBatchedIB = NULL;

    D3DVERTEXBUFFER_DESC VBDesc;
    sourceMesh.pVB->GetDesc( &VBDesc );
    m_dwNumVertsPerInst = VBDesc.Size / sourceMesh.dwStride;
    m_dwSourceMeshVBSize = VBDesc.Size;

    D3DINDEXBUFFER_DESC IBDesc;
    sourceMesh.pIB->GetDesc( &IBDesc );
    m_dwNumIndicesPerInst = IBDesc.Size / ( IBDesc.Format == D3DFMT_INDEX32 ? 4 : 2 );
    m_dwSourceMeshIBSize = IBDesc.Size;

    InitializeFX();

    SetSourceMeshData( sourceMesh );

    CreateInstancedMeshDecl();
    CreateInstanceDataVB( dwMaxInstCount );
    CreateIndexVB();

    InitializeTessellation();
    InitializeXPS();
}

//--------------------------------------------------------------------------------------
// Name: Destroy
//--------------------------------------------------------------------------------------
VOID InstancedMesh::Destroy()
{
    ATG::g_pd3dDevice->BlockUntilIdle();

    SAFE_RELEASE( m_pInstancedMeshDecl );
    SAFE_RELEASE( m_pTessellatedInstanceDecl );
    SAFE_RELEASE( m_pXPSVertexDecl );
    for( DWORD i = 0; i < 2; i++ )
    {
        SAFE_RELEASE( m_pInstDataVB[i] );
    }
    SAFE_RELEASE( m_pMeshIndexDataVB );
    SAFE_RELEASE( m_pEffect );
    SAFE_RELEASE( m_pBatchedIB );

    if( m_pXPSIndexData )
    {
        XPhysicalFree( m_pXPSIndexData );
    }
    if( m_pXPSVertexData )
    {
        XPhysicalFree( m_pXPSVertexData );
    }

    ATG::g_pd3dDevice->SetStreamSource( 0, NULL, 0, 0 );
    ATG::g_pd3dDevice->SetStreamSource( 1, NULL, 0, 0 );
    ATG::g_pd3dDevice->SetStreamSource( 2, NULL, 0, 0 );
    ATG::g_pd3dDevice->SetIndices( NULL );
    ATG::g_pd3dDevice->SetVertexDeclaration( NULL );

    SAFE_RELEASE( m_SourceMesh.pVB );
    SAFE_RELEASE( m_SourceMesh.pIB );

}


//--------------------------------------------------------------------------------------
// Name: SetSourceMeshData()
// Desc: Sets the data for the source mesh, and releases any outdated resources.
//--------------------------------------------------------------------------------------
VOID InstancedMesh::SetSourceMeshData( const SourceMeshData& sourceMesh )
{
    // Wait for the GPU to finish before modifying anything
    ATG::g_pd3dDevice->BlockUntilIdle();

    ATG::g_pd3dDevice->SetStreamSource( 0, NULL, 0, 0 );
    ATG::g_pd3dDevice->SetStreamSource( 1, NULL, 0, 0 );
    ATG::g_pd3dDevice->SetStreamSource( 2, NULL, 0, 0 );
    ATG::g_pd3dDevice->SetIndices( NULL );
    ATG::g_pd3dDevice->SetVertexDeclaration( NULL );

    if( m_SourceMesh.pVB )
    {
        m_SourceMesh.pVB->Release();
    }
    if( m_SourceMesh.pIB )
    {
        m_SourceMesh.pIB->Release();
    }

    m_SourceMesh = sourceMesh;
}


//--------------------------------------------------------------------------------------
// Name: InitializeFX()
// Desc: Loads the FX file and stores handles to the FXLite constants
//--------------------------------------------------------------------------------------
VOID InstancedMesh::InitializeFX()
{
    // Load FXLite effect.
    BYTE* pEffectData = NULL;
    ATG::LoadFile( "game:\\media\\effects\\instancing.fxobj", ( VOID** )&pEffectData,
                   NULL );
    FXLCreateEffect( ATG::g_pd3dDevice, pEffectData, NULL, &m_pEffect );

    // Cache FXLite handles.
    m_hWVPMatrix = m_pEffect->GetParameterHandle( "world_view_proj_matrix" );
    m_hInstanceParams = m_pEffect->GetParameterHandle( "instancing_params" );
    m_hInstanceData = m_pEffect->GetParameterHandle( "instance_data" );
}


//--------------------------------------------------------------------------------------
// Name: CreateInstancedMeshDecl()
// Desc: Creates the vertex declaration for the instanced mesh.  New stream elements
//       are appended to the instanced mesh vertex decl to accomodate instancing
//       data.
//--------------------------------------------------------------------------------------
VOID InstancedMesh::CreateInstancedMeshDecl()
{
    // Create a new vertex declaration
    D3DVERTEXELEMENT9 vertexElements[ 64 ];
    DWORD dwElementCount = ARRAYSIZE( vertexElements );
    m_SourceMesh.pDecl->GetDeclaration( vertexElements, ( UINT* )&dwElementCount );
    assert( dwElementCount < ( ARRAYSIZE( vertexElements ) - 2 ) );

    // We're going to add some elements, so make sure that we won't overwrite anything
    // (there can't be anything in stream 1 or 2 and there can't be anything in 
    //  position 1 or 2)
    for( DWORD i = 0; i < dwElementCount; i++ )
    {
        assert( !( vertexElements[i].Stream == 1 || vertexElements[i].Stream == 2 ) &&
                !( vertexElements[i].Usage == D3DDECLUSAGE_POSITION &&
                   ( vertexElements[i].UsageIndex == 1 ||
                     vertexElements[i].UsageIndex == 2 )
                   ) );
    }

    static const D3DVERTEXELEMENT9 newStreamElements[] =
    {
        { 1, 0, D3DDECLTYPE_UINT1,  0, D3DDECLUSAGE_POSITION, 1 },    // Index data
        { 2, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_POSITION, 2 },    // Instance data
        D3DDECL_END()
    };

    // Add new stream decls for the index data and the instance data.
    XMemCpy( &vertexElements[ dwElementCount - 1 ], newStreamElements,
             sizeof( newStreamElements ) );

    // Create the vertex decl
    ATG::g_pd3dDevice->CreateVertexDeclaration( vertexElements, &m_pInstancedMeshDecl );
}


//--------------------------------------------------------------------------------------
// Name: CreateInstanceDataVB()
// Desc: Creates the vertex buffer that holds the instance data.
//--------------------------------------------------------------------------------------
VOID InstancedMesh::CreateInstanceDataVB( DWORD dwMaxInstanceCount )
{
    // Create instance data vertex buffers.
    DWORD dwInstanceDataVBSize = dwMaxInstanceCount * sizeof( InstanceData );
    for( DWORD i = 0; i < 2; i++ )
    {
        ATG::g_pd3dDevice->CreateVertexBuffer( dwInstanceDataVBSize,
                                               0, 0, D3DPOOL_DEFAULT,
                                               &m_pInstDataVB[ i ], NULL );
    }
}


//--------------------------------------------------------------------------------------
// Name: CreateIndexVB()
// Desc: Creates a vertex buffer to hold index data.  This is used to implement 
//       instancing via vfetching the mesh indices.
//--------------------------------------------------------------------------------------
VOID InstancedMesh::CreateIndexVB()
{
    D3DINDEXBUFFER_DESC sourceIBDesc;
    m_SourceMesh.pIB->GetDesc( &sourceIBDesc );

    // Create a special vertex buffer to hold the mesh index buffer.
    m_dwMeshIndexDataVBSize = m_dwNumIndicesPerInst * sizeof( DWORD );
    ATG::g_pd3dDevice->CreateVertexBuffer( m_dwMeshIndexDataVBSize,
                                           0, 0, D3DPOOL_DEFAULT,
                                           &m_pMeshIndexDataVB, NULL );

    // Copy index buffer data to vertex buffer.
    DWORD* pIndexDataDest = NULL;
    m_pMeshIndexDataVB->Lock( 0, 0, ( VOID** )&pIndexDataDest, 0 );
    DWORD* pIndexDataSrc = NULL;
    m_SourceMesh.pIB->Lock( 0, 0, ( VOID** )&pIndexDataSrc, D3DLOCK_READONLY );

    if( sourceIBDesc.Format == D3DFMT_INDEX32 )
    {
        XMemCpy( pIndexDataDest, pIndexDataSrc, m_dwNumIndicesPerInst * sizeof( DWORD ) );
    }
    else
    {
        WORD* pIndexDataSrcWord = ( WORD* )pIndexDataSrc;
        for( DWORD i = 0; i < m_dwNumIndicesPerInst; ++i )
        {
            pIndexDataDest[i] = ( DWORD )pIndexDataSrcWord[i];
        }
    }

    m_SourceMesh.pIB->Unlock();
    m_pMeshIndexDataVB->Unlock();
}


//--------------------------------------------------------------------------------------
// Name: InitializeTessellation()
// Desc: Initializes resources used in instancing via the tessellator.  Only a vertex 
//       declaration is needed.  The vertex buffer for instance data is shared between 
//       methods, and all other data is generated in the vertex shader.
//--------------------------------------------------------------------------------------
VOID InstancedMesh::InitializeTessellation()
{
    // Create the tessellator vertex declaration
    static const D3DVERTEXELEMENT9 TessInstVertexElements[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        D3DDECL_END()
    };

    ATG::g_pd3dDevice->CreateVertexDeclaration( TessInstVertexElements,
                                                &m_pTessellatedInstanceDecl );
}


//--------------------------------------------------------------------------------------
// Name: InitializeXPS()
// Desc: Initializes resources used in instancing via XPS.  XPS needs access to vertex
//       and index data outside of GPU resources, so this data is copied to memory that
//       can be accessed from XPS.
//--------------------------------------------------------------------------------------
VOID InstancedMesh::InitializeXPS()
{
    if( m_pXPSIndexData )
    {
        XPhysicalFree( m_pXPSIndexData );
    }
    if( m_pXPSVertexData )
    {
        XPhysicalFree( m_pXPSVertexData );
    }

    // Define the vertex elements.
    static const D3DVERTEXELEMENT9 XPSVertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0 },   // Position
        { 0, 12, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 0 },   // TexCoord
        { 0, 20, D3DDECLTYPE_FLOAT1, 0, D3DDECLUSAGE_COLOR,    0 },   // Scale
        { 0, 24, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_NORMAL,   0 },   // Normal
        D3DDECL_END()
    };

    // Create a vertex declaration from the element descriptions.
    ATG::g_pd3dDevice->CreateVertexDeclaration( XPSVertexElements, &m_pXPSVertexDecl );

    // Copy the vertex buffer data to memory that XPS can access
    m_dwXPSVertexDataSize = m_dwNumVertsPerInst * sizeof( ATG::MeshVertexPT );
    m_pXPSVertexData = ( ATG::MeshVertexPT* )XPhysicalAlloc( m_dwXPSVertexDataSize,
                                                             MAXULONG_PTR, 0,
                                                             PAGE_READWRITE );

    ATG::MeshVertexPT* pVertexDataSrc;
    m_SourceMesh.pVB->Lock( 0, 0, ( VOID** )&pVertexDataSrc, D3DLOCK_READONLY );
    XMemCpy( m_pXPSVertexData, pVertexDataSrc, m_dwXPSVertexDataSize );
    m_SourceMesh.pVB->Unlock();

    // Copy the index buffer data to memory that XPS can access
    D3DINDEXBUFFER_DESC IBDesc;
    m_SourceMesh.pIB->GetDesc( &IBDesc );
    m_dwXPSIndexDataSize = IBDesc.Size;

    // Allocate the index data.  This must either be in non-cacheable memory 
    // (write combined is non-cacheable), or it needs to be flushed from the cache
    // after being written so the GPU can access it.
    m_pXPSIndexData = ( WORD* )XPhysicalAlloc( m_dwXPSIndexDataSize, MAXULONG_PTR, 0,
                                               PAGE_READWRITE | PAGE_WRITECOMBINE );

    DWORD* pIndexDataSrc = NULL;
    m_SourceMesh.pIB->Lock( 0, 0, ( VOID** )&pIndexDataSrc, D3DLOCK_READONLY );

    // Copy index data
    if( IBDesc.Format == D3DFMT_INDEX16 )
    {
        XMemCpyStreaming_WriteCombined( m_pXPSIndexData, pIndexDataSrc, IBDesc.Size );
    }
    else
    {
        for( DWORD i = 0; i < m_dwNumIndicesPerInst; ++i )
        {
            m_pXPSIndexData[i] = ( WORD )pIndexDataSrc[i];
        }
    }

    m_SourceMesh.pIB->Unlock();
}


//--------------------------------------------------------------------------------------
// Name: SetInstanceData()
// Desc: Copies new instance data into the current instance data vertex buffer.
//--------------------------------------------------------------------------------------
VOID InstancedMesh::SetInstanceData( const InstanceData* __restrict pInstData,
                                     DWORD dwInstCount )
 {
    D3DVertexBuffer* __restrict pVB = m_pInstDataVB[ m_dwFrameCt % 2 ];
    InstanceData*    __restrict pInstDataDest = NULL;

    dwInstCount = min( dwInstCount, m_dwMaxInstanceCount );
    pVB->Lock( 0, 0, ( VOID** )&pInstDataDest, 0 );
    XMemCpyStreaming_WriteCombined( pInstDataDest, pInstData,
                                    dwInstCount * sizeof( InstanceData ) );
    pVB->Unlock();

    m_dwInstDataSize = m_dwInstDataVBSize = dwInstCount * sizeof( InstanceData );
    m_pInstData = pInstData;
    m_dwInstanceCount  = dwInstCount;
}


//--------------------------------------------------------------------------------------
// Name: RenderOneInstPerDraw()
// Desc: Renders the mesh instances one at a time.  This method calls 
//       DrawIndexedVertices once per instance.  The instance data is set before each 
//       draw call, either using GPU constants, or through a vertex stream.  Index data
//       comes from an index buffer.  This enables usage of the post-transform vertex
//       cache, but comes with the overhead of additional CPU usage.
//--------------------------------------------------------------------------------------
VOID InstancedMesh::RenderOneInstPerDraw( const XMMATRIX& matWVP, BOOL bUseVFetch )
{
    if( bUseVFetch )
    {
        PIXBeginNamedEvent( 0xFFFFFFFF, "Instances ( One per draw / VFetch )" );
    }
    else
    {
        PIXBeginNamedEvent( 0xFFFFFFFF, "Instances ( One per draw / GPU Constants )" );
    }

    PreRender( bUseVFetch ? ONEPERDRAW_VFETCH : ONEPERDRAW_GPUCONST, matWVP );

    // Set the vertex stream, indices, and vertex decl.
    ATG::g_pd3dDevice->SetStreamSource( 0, m_SourceMesh.pVB, 0, m_SourceMesh.dwStride );
    ATG::g_pd3dDevice->SetIndices( m_SourceMesh.pIB );
    ATG::g_pd3dDevice->SetVertexDeclaration( m_pInstancedMeshDecl );

    if( bUseVFetch )
    {
        for( DWORD i = 0; i < m_dwInstanceCount; i++ )
        {
            // Change the offset into the instance data vertex buffer
            ATG::g_pd3dDevice->SetStreamSource( 2,
                                                m_pInstDataVB[ m_dwFrameCt % 2 ],
                                                i * sizeof( InstanceData ),
                                                sizeof( InstanceData ) );
            ATG::g_pd3dDevice->DrawIndexedVertices( m_SourceMesh.primType, 0, 0,
                                                    m_dwNumIndicesPerInst );
        }
        m_dwRenderDataSize = m_dwSourceMeshVBSize +
            m_dwSourceMeshIBSize +
            m_dwInstDataVBSize * 2;
    }
    else // Use GPU constants
    {
        for( DWORD i = 0; i < m_dwInstanceCount; i++ )
        {
            ATG::g_pd3dDevice->SetVertexShaderConstantF( 5, ( FLOAT* )&m_pInstData[i], 1 );
            ATG::g_pd3dDevice->DrawIndexedVertices( m_SourceMesh.primType, 0, 0,
                                                    m_dwNumIndicesPerInst );
        }
        m_dwRenderDataSize = m_dwSourceMeshVBSize +
            m_dwSourceMeshIBSize +
            m_dwInstDataSize;
    }

    PostRender();
    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RenderCustomVFetch()
// Desc: Renders the mesh instances using custom vfetches.  The vertex shader will use
//       the GPU generated indices to do a custom vfetch from the instance and index 
//       data held in the vertex buffers in streams 1 and 2.
//--------------------------------------------------------------------------------------
VOID InstancedMesh::RenderCustomVFetch( const XMMATRIX& matWVP )
{
    PIXBeginNamedEvent( 0xFFFFFFFF, "Instances ( Custom VFetch)" );

    PreRender( CUSTOM_VFETCH, matWVP );

    // Set the vertex streams and decl.
    ATG::g_pd3dDevice->SetStreamSource( 0, m_SourceMesh.pVB, 0, m_SourceMesh.dwStride );
    ATG::g_pd3dDevice->SetStreamSource( 1, m_pMeshIndexDataVB, 0, sizeof( DWORD ) );
    ATG::g_pd3dDevice->SetStreamSource( 2, m_pInstDataVB[ m_dwFrameCt % 2 ],
                                        0, sizeof( InstanceData ) );
    ATG::g_pd3dDevice->SetVertexDeclaration( m_pInstancedMeshDecl );

    // Draw the instanced meshes.
    ATG::g_pd3dDevice->DrawVertices( m_SourceMesh.primType, 0,
                                     m_dwNumIndicesPerInst * m_dwInstanceCount );

    m_dwRenderDataSize = m_dwSourceMeshVBSize +
        m_dwMeshIndexDataVBSize +
        m_dwInstDataVBSize * 2;

    PostRender();
    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RenderBatches()
// Desc: Renders the mesh instances in batches.  This method duplicates the index
//       data multiple times and stores the result in a single index buffer.  This enables
//       efficient use of the post-transform vertex cache without the CPU overhead of 
//       large numbers of draw calls.  The number of instances to draw in each batch is
//       a tradeoff between memory usage, CPU usage, and efficiency of the 
//       post-transform vertex cache.
//--------------------------------------------------------------------------------------
DWORD InstancedMesh::RenderBatches( const XMMATRIX& matWVP,
                                    DWORD dwBatchSize,
                                    BOOL bUseVFetch )
{
    // Clamp the batch size
    DWORD dwBatchSizeRendered = max( 0, min( dwBatchSize, m_dwInstanceCount ) );
    if( !bUseVFetch ) // if using GPU constants
    {
        // There are only so many constants, so clamp the batch size
        dwBatchSizeRendered = min( dwBatchSizeRendered, MAX_GPU_CONST_BATCH_SIZE );
    }

    if( bUseVFetch )
    {
        PIXBeginNamedEvent( 0xFFFFFFFF, "Instances ( Batches / VFetch)" );
    }
    else
    {
        PIXBeginNamedEvent( 0xFFFFFFFF, "Instances ( Batches / GPU Constants)" );
    }

    UpdateBatchedIndexBuffer( dwBatchSizeRendered );

    PreRender( bUseVFetch ? BATCHED_VFETCH : BATCHED_GPUCONST, matWVP );

    // Set the vertex streams and vertex decl.
    ATG::g_pd3dDevice->SetStreamSource( 0, m_SourceMesh.pVB, 0, m_SourceMesh.dwStride );
    ATG::g_pd3dDevice->SetIndices( m_pBatchedIB );
    ATG::g_pd3dDevice->SetVertexDeclaration( m_pInstancedMeshDecl );

    // Draw the batched instances
    if( bUseVFetch )
    {
        for( DWORD i = 0; i < m_dwInstanceCount; i += m_dwNumInstsPerBatch )
        {
            DWORD numInstances = min( dwBatchSizeRendered, m_dwInstanceCount - i );

            // Change the offset into the instance data vertex buffer
            ATG::g_pd3dDevice->SetStreamSource( 2,
                                                m_pInstDataVB[m_dwFrameCt % 2],
                                                i * sizeof( InstanceData ),
                                                sizeof( InstanceData ) );
            ATG::g_pd3dDevice->DrawIndexedVertices( m_SourceMesh.primType, 0, 0,
                                                    m_dwNumIndicesPerInst * numInstances );
        }

        m_dwRenderDataSize = m_dwSourceMeshVBSize +
            m_dwBatchedIBSize +
            m_dwInstDataVBSize * 2;
    }
    else // Use GPU constants for instance data
    {
        for( DWORD i = 0; i < m_dwInstanceCount; i += m_dwNumInstsPerBatch )
        {
            DWORD dwNumInsts = min( dwBatchSizeRendered, m_dwInstanceCount - i );
            ATG::g_pd3dDevice->SetVertexShaderConstantF( 6,
                                                         ( FLOAT* )&m_pInstData[i],
                                                         dwNumInsts );
            ATG::g_pd3dDevice->DrawIndexedVertices( m_SourceMesh.primType, 0, 0,
                                                    m_dwNumIndicesPerInst * dwNumInsts );
        }

        m_dwRenderDataSize = m_dwSourceMeshVBSize + m_dwBatchedIBSize + m_dwInstDataSize;
    }

    PostRender();
    PIXEndNamedEvent();

    return dwBatchSizeRendered;
}


//--------------------------------------------------------------------------------------
// Name: UpdateBatchedIndexBuffer()
// Desc: Updates the batched index buffer which holds multiple copies of the mesh
//       indices to enable rendering instances in batches.  The starting index of each
//       copy of the indices is offset in order to distinguish instances.
//--------------------------------------------------------------------------------------
VOID InstancedMesh::UpdateBatchedIndexBuffer( DWORD numInstancesPerBatch )
{
    // Make sure the indices can be held in DWORDs
    assert( m_dwNumIndicesPerInst * numInstancesPerBatch < 0xFFFFFFFF );

    // If the number of instances per batch hasn't changed, no need to update
    if( m_dwNumInstsPerBatch == numInstancesPerBatch && m_pBatchedIB != NULL )
    {
        return;
    }

    ATG::g_pd3dDevice->BlockUntilIdle();

    m_dwNumInstsPerBatch = numInstancesPerBatch;

    if( m_pBatchedIB != NULL )
    {
        m_pBatchedIB->Release();
    }

    // Create the Index Buffer
    m_dwBatchedIBSize = m_dwNumIndicesPerInst * m_dwNumInstsPerBatch * sizeof( DWORD );
    ATG::g_pd3dDevice->CreateIndexBuffer( m_dwBatchedIBSize, 0, D3DFMT_INDEX32,
                                          0, &m_pBatchedIB, NULL );

    // Write multiple copies of the mesh index buffer to the batched buffer
    DWORD* pIndexDataDest = NULL;
    m_pBatchedIB->Lock( 0, 0, ( VOID** )&pIndexDataDest, 0 );

    DWORD* pIndexDataSrc = NULL;
    m_SourceMesh.pIB->Lock( 0, 0, ( VOID** )&pIndexDataSrc, D3DLOCK_READONLY );

    D3DINDEXBUFFER_DESC IBDesc;
    m_SourceMesh.pIB->GetDesc( &IBDesc );

    for( DWORD i = 0; i < m_dwNumInstsPerBatch; i++ )
    {
        for( DWORD j = 0; j < m_dwNumIndicesPerInst; j++ )
        {
            if( IBDesc.Format == D3DFMT_INDEX32 )
            {
                *pIndexDataDest = pIndexDataSrc[j] + i * m_dwNumIndicesPerInst;
            }
            else
            {
                *pIndexDataDest = ( ( WORD* )pIndexDataSrc )[j] + i * m_dwNumIndicesPerInst;
            }
            pIndexDataDest++;
        }
    }

    m_SourceMesh.pIB->Unlock();
    m_pBatchedIB->Unlock();
}


//--------------------------------------------------------------------------------------
// Name: RenderUsingXPS()
// Desc: Renders instances using XPS.
//--------------------------------------------------------------------------------------
VOID InstancedMesh::RenderUsingXPS( const XMMATRIX& matWVP )
{
    PIXBeginNamedEvent( 0xFFFFFFFF, "Instances ( XPS )" );
    PreRender( XPS, matWVP );

    // Begin XPS on 4 hardware threads
    ATG::g_pd3dDevice->XpsBegin( D3DXPS_LOCK_128KB_L2 | D3DXPS_CPU2 | D3DXPS_CPU3 |
                                 D3DXPS_CPU4 | D3DXPS_CPU5 );
    ATG::g_pd3dDevice->XpsSetCallback( XPSCallback, this, 0 );

    ATG::g_pd3dDevice->SetStreamSource( 0, NULL, 0, sizeof( XPSMeshVertex ) );
    ATG::g_pd3dDevice->SetVertexDeclaration( m_pXPSVertexDecl );

    // A batch size of 100 gives good performance in this case; Your mileage may vary.
    const DWORD dwMaxInstsPerSubmit = 100;
    DWORD dwNumInstsPerSubmit = min( m_dwInstanceCount, dwMaxInstsPerSubmit );
    DWORD dwSubmitSize = dwNumInstsPerSubmit * sizeof( InstanceSeed );

    for( DWORD i = 0; i < m_dwInstanceCount; i += dwNumInstsPerSubmit )
    {
        ATG::g_pd3dDevice->XpsSubmit( dwNumInstsPerSubmit,
                                      &m_pInstData[ i ],
                                      dwSubmitSize );
    }

    ATG::g_pd3dDevice->XpsEnd();

    m_dwRenderDataSize = m_dwXPSVertexDataSize +
        m_dwXPSIndexDataSize +
        m_dwInstDataSize;


    PostRender();
    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: XPSCallback()
// Desc: Callback for XPS rendering
//--------------------------------------------------------------------------------------
VOID XPSCallback( D3DXpsThread* __restrict pThreadContext,
                  VOID*         __restrict pCallbackContext,
                  const VOID*   __restrict pSubmitData,
                  DWORD                    dwInstanceIndex )
 {
    PIXBeginNamedEvent( 0xFFFFFFFF, "XPSCallback" );
    ATG::Timer CPUTimer;

    D3DXps xps( pThreadContext );

    InstancedMesh* pInstancedMesh = ( InstancedMesh* )pCallbackContext;

    do
    {
        // Get a pointer to the seed for this XPS instance.
        InstanceSeed* pSeedData = ( InstanceSeed* )pSubmitData + dwInstanceIndex;

        // Allocate some room in the L2 cache for the vertex data.
        DWORD dwVertexDataSize = pInstancedMesh->GetNumVertsPerInst()
                                    * sizeof( XPSMeshVertex );
        BYTE* pXPSMemory = ( BYTE* )xps.Allocate( dwVertexDataSize, D3DXPS_COMMAND_SIZE );

        XPSMeshVertex* pVertexData = ( XPSMeshVertex* )pXPSMemory;

        // Use the seed data to generate the vertices.
        GenerateInstanceVertexData( pInstancedMesh->GetXPSVertexData(), pVertexData,
                                    pSeedData, pInstancedMesh->GetNumVertsPerInst() );

        // Render the instance
        xps.DrawIndexedVertices( D3DPT_QUADLIST, pInstancedMesh->GetNumIndicesPerInst(),
                                 pInstancedMesh->GetXPSIndexData(),
                                 D3DFMT_INDEX16, pVertexData );

        // Kick-off the result for immediate rendering by the GPU, and
        // at the same time request the next instance index to render.
    } while( xps.KickOffAndGet( &dwInstanceIndex ) );

    LONG time = (LONG)(CPUTimer.GetElapsedTime() * CPUTimer.m_PerfFreq.QuadPart); 
    InterlockedExchangeAdd( &g_nXPStime_curFrame, time);
    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: GenerateInstanceVertexData()
// Desc: Composes the vertex data for an instance by applying the instance seed 
//       parameters.  This is for generating instances on the CPU during XPS rendering.
//       This function uses sub-optimal code clarity.  A more optimal solution would
//       unwind the loop and use VMX.
//--------------------------------------------------------------------------------------
inline VOID GenerateInstanceVertexData( ATG::MeshVertexPT*  __restrict pSrcVertexData,
                                        XPSMeshVertex*      __restrict pDestVertexData,
                                        const InstanceSeed* __restrict pSeed,
                                        DWORD               dwNumVertsPerInst )
 {
    const ATG::MeshVertexPT* __restrict srcVert  = pSrcVertexData;
    XPSMeshVertex*           __restrict destVert = pDestVertexData;

    for( DWORD i = 0; i < dwNumVertsPerInst; i++ )
    {
        destVert->m_vNormal = srcVert->Position;

        destVert->m_vPosition.x = srcVert->Position.x * pSeed->m_fScale
                                  + pSeed->m_vPosition.x;
        destVert->m_vPosition.y = srcVert->Position.y * pSeed->m_fScale
                                  + pSeed->m_vPosition.y;
        destVert->m_vPosition.z = srcVert->Position.z * pSeed->m_fScale
                                  + pSeed->m_vPosition.z;

        destVert->m_vTexCoord = srcVert->TexCoord;

        destVert->m_fScale = pSeed->m_fScale;

        srcVert++;
        destVert++;
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderUsingTessellator()
// Desc: Renders instances using the tessellation hardware.  Each patch output by the 
//       tessellator equates to an instance.  The position of each of the vertices in 
//       the batch is calculated in the vertex shader.
//--------------------------------------------------------------------------------------
DWORD InstancedMesh::RenderUsingTessellator( const XMMATRIX& matWVP,
                                             DWORD dwNumInstancesPerBatch,
                                             DWORD dwTessLevel,
                                             BOOL bUseVFetch )
{
    InstancingMethod instMethod = bUseVFetch ? TESSELLATOR_VFETCH : TESSELLATOR_GPUCONST;
    if( bUseVFetch )
    {
        PIXBeginNamedEvent( 0xFFFFFFFF, "Instances ( Tessellator / VFetch )" );
    }
    else
    {
        PIXBeginNamedEvent( 0xFFFFFFFF, "Instances ( Tessellator / GPU Constants )" );
    }
    PreRender( instMethod, matWVP );

    dwNumInstancesPerBatch = min( dwNumInstancesPerBatch, m_dwInstanceCount );
    dwNumInstancesPerBatch = max( 0, dwNumInstancesPerBatch );
    if( instMethod == TESSELLATOR_GPUCONST )
    {
        dwNumInstancesPerBatch = min( dwNumInstancesPerBatch, MAX_GPU_CONST_BATCH_SIZE );
    }

    // Set the vertex decl.  No vertex stream needs to be set here - the geometry
    // is generated by the tessellator.
    ATG::g_pd3dDevice->SetVertexDeclaration( m_pTessellatedInstanceDecl );

    ATG::g_pd3dDevice->SetRenderState( D3DRS_MAXTESSELLATIONLEVEL,
                                       ATG::FtoDW( ( FLOAT )dwTessLevel ) );

    if( instMethod == TESSELLATOR_VFETCH )
    {
        for( DWORD i = 0; i < m_dwInstanceCount; i += dwNumInstancesPerBatch )
        {
            DWORD dwNumInsts = min( dwNumInstancesPerBatch, m_dwInstanceCount - i );
            ATG::g_pd3dDevice->SetStreamSource( 0, m_pInstDataVB[ m_dwFrameCt % 2 ],
                                                i * sizeof( InstanceData ),
                                                sizeof( InstanceData ) );
            ATG::g_pd3dDevice->DrawTessellatedPrimitive( D3DTPT_QUADPATCH, 0,
                                                         dwNumInsts );
        }

        m_dwRenderDataSize = m_dwTesselatedInstanceVBSize + m_dwInstDataVBSize * 2;
    }
    else // put instance data in GPU constants
    {
        for( DWORD i = 0; i < m_dwInstanceCount; i += dwNumInstancesPerBatch )
        {
            DWORD dwNumInsts = min( dwNumInstancesPerBatch, m_dwInstanceCount - i );
            ATG::g_pd3dDevice->SetVertexShaderConstantF( 6, ( const FLOAT* )
                                                         &m_pInstData[i],
                                                         dwNumInsts );
            ATG::g_pd3dDevice->DrawTessellatedPrimitive( D3DTPT_QUADPATCH, 0,
                                                         dwNumInsts );
        }

        m_dwRenderDataSize = m_dwTesselatedInstanceVBSize + m_dwInstDataSize;
    }

    PostRender();
    PIXEndNamedEvent();

    return dwNumInstancesPerBatch;
}


//--------------------------------------------------------------------------------------
// Name: PreRender()
// Desc: Perform pre-rendering tasks common to each method
//--------------------------------------------------------------------------------------
VOID InstancedMesh::PreRender( InstancingMethod instMethod, const XMMATRIX& matWVP ) const
{
    g_nXPStime_prevFrame = g_nXPStime_curFrame;
    g_nXPStime_curFrame = 0;

    m_pEffect->BeginTechniqueFromIndex( instMethod, 0 );
    m_pEffect->BeginPassFromIndex( 0 );

    // Set WVP matrix and instance params to effect.
    m_pEffect->SetMatrixF4x4A( m_hWVPMatrix, ( const FXLFLOATA* )&matWVP );
    XMFLOAT4 InstanceInfo( ( FLOAT )m_dwNumIndicesPerInst, 0, 0, 0 );
    m_pEffect->SetVectorF( m_hInstanceParams, ( const FLOAT* )&InstanceInfo );

    m_pEffect->Commit();

}


//--------------------------------------------------------------------------------------
// Name: PostRender()
// Desc: Perform post-rendering tasks common to each method
//--------------------------------------------------------------------------------------
VOID InstancedMesh::PostRender()
{
    m_pEffect->EndPass();
    m_pEffect->EndTechnique();

    ATG::g_pd3dDevice->SetStreamSource( 0, NULL, 0, 0 );
    ATG::g_pd3dDevice->SetStreamSource( 1, NULL, 0, 0 );
    ATG::g_pd3dDevice->SetStreamSource( 2, NULL, 0, 0 );

    m_dwFrameCt++;
}
