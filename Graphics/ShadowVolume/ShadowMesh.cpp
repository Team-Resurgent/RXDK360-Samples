//--------------------------------------------------------------------------------------
// ShadowMesh.cpp
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
#include <xtl.h>
#include <AtgMesh.h>
#include "ShadowMesh.h"
#include <AtgApp.h>

// Disable warning C6211: Leaking memory <pointer> due to an exception.
// We "know" that we won't run out of memory in this sample
#pragma warning ( disable : 6211 )


const DWORD VSCONT_matWorldViewProj = 0;  // World-view-projection matrix
const DWORD VSCONT_vLightPos = 4;  // Light position (may be infinite: w=0)
const DWORD VSCONT_fOffsetScale = 20;  // Offset scale
const DWORD VSCONT_fOffsetScale2 = 21;  // Offset scale for back-facing polys


//--------------------------------------------------------------------------------------
// Name: BuildShadowMesh()
// Desc: Collapses any vertices that share a postion value into a single vertex. Builds
//       the face and edge list for the shadow mesh.
//--------------------------------------------------------------------------------------
static HRESULT BuildShadowMesh( DWORD dwNumFaces,    // Number of faces.
                                const WORD* pMeshIndices,  // Input triangle indices (3*dwNumFaces).
                                const BYTE* pMeshVertices, // Input vertices.
                                DWORD dwVertexSize,  // Size of each vertex in bytes.
                                DWORD& dwTempCount,   // Number of output vertices.
                                XMVECTOR* pTempVerts,    // Output vertices (up to # of input verts).
                                DWORD& dwNumEdges,    // Number of output edges.
                                SHADOWMESH_EDGE* pEdges,        // Output edges (up to 3*dwNumFaces).
                                SHADOWMESH_FACE* pFaces )       // Output faces (dwNumFaces).
{
    // Start with zero output verts and edges.
    dwTempCount = 0;
    dwNumEdges = 0;

    // Build triangle and edge lists.
    for( DWORD face = 0; face < dwNumFaces; face++ )
    {
        // Add unique vertex positions.
        for( DWORD i = 0; i < 3; i++ )
        {
            WORD wSrcIndex = pMeshIndices[face * 3 + i];

            const XMVECTOR vPos = XMLoadFloat3( ( const XMFLOAT3* )( pMeshVertices + ( DWORD )wSrcIndex *
                                                                     dwVertexSize ) );

            // Check if we already have this position.  
            WORD wDstIndex = 0;
            while( wDstIndex < dwTempCount )
            {
                // Check for exact match of position.
                if( XMVector3Equal( vPos, pTempVerts[wDstIndex] ) )
                    break;

                wDstIndex++;
            }

            if( wDstIndex == dwTempCount )
            {
                // Add vertex.
                pTempVerts[wDstIndex] = vPos;
                dwTempCount++;
            }

            pFaces[face].V[i] = wDstIndex;
        }

        const XMVECTOR vert0 = pTempVerts[pFaces[face].V[0]];
        const XMVECTOR vert1 = pTempVerts[pFaces[face].V[1]];
        const XMVECTOR vert2 = pTempVerts[pFaces[face].V[2]];

        // Compute face plane.
        XMVECTOR vNormal;
        XMVECTOR v2Minus1 = vert2 - vert1;
        XMVECTOR v1Minus0 = vert1 - vert0;
        vNormal = XMVector3Cross( v2Minus1, v1Minus0 );
        vNormal = XMVector3Normalize( vNormal );
        pFaces[face].Plane = XMPlaneFromPointNormal( vert0, vNormal );

        // Add edges.
        DWORD i0 = 2;
        for( DWORD i1 = 0; i1 < 3; i1++ )
        {
            WORD index0 = pFaces[face].V[i0];
            WORD index1 = pFaces[face].V[i1];

            // Check if we already have this edge.
            DWORD edge = 0;
            while( edge < dwNumEdges )
            {
                // Does this edge exist from an adjacent triangle.
                // Note: winding order from the adjacent triangle is reversed.
                if( pEdges[edge].V[0] == index1 && pEdges[edge].V[1] == index0 )
                    break;

                edge++;
            }

            if( edge < dwNumEdges )
            {
                // If the mesh is manifold, only two triangles should ever
                // share an edge.
                if( pEdges[edge].Face1 != 0xffffffff )
                    return E_FAIL;

                // Attach to existing edge.
                pEdges[edge].Face1 = face;
            }
            else
            {
                // Add new edge.
                pEdges[edge].V[0] = index0;
                pEdges[edge].V[1] = index1;
                pEdges[edge].Face0 = face;
                pEdges[edge].Face1 = 0xffffffff;
                dwNumEdges++;
            }

            i0 = i1;
        }
    }

    // Make sure the mesh in closed (2-manifold).
    for( DWORD edge = 0; edge < dwNumEdges; edge++ )
    {
        // If the mesh is manifold, all edges should be shared by two triangles.
        if( pEdges[edge].Face0 == 0xffffffff || pEdges[edge].Face1 == 0xffffffff )
            return E_FAIL;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderVolume()
// Desc: Render the shadow volume for a given light position using the 
//       previously computed volume.
//--------------------------------------------------------------------------------------
VOID CShadowMesh::RenderVolume()
{
    ATG::g_pd3dDevice->SetVertexDeclaration( m_pShadowVolumeVertexDecl );
    ATG::g_pd3dDevice->SetVertexShader( m_pShadowVolumeVertexShader );

    if( m_pMesh->m_pFrames )
        RenderVolumeFrame( m_pMesh->m_pFrames );
}


//--------------------------------------------------------------------------------------
// Name: RenderVolumeFrame()
// Desc: Recursively render the shadow volume for each frame.
//--------------------------------------------------------------------------------------
VOID CShadowMesh::RenderVolumeFrame( ATG::MESH_FRAME* pFrame )
{
    // Apply the frame's local transform
    XMMATRIX matSavedWorld = m_matWorld;
    m_matWorld = XMMatrixMultiply( pFrame->m_matTransform, matSavedWorld );

    // Render the mesh data
    if( pFrame->m_pMeshData->m_dwNumSubsets )
    {
        // Get the current transforms.
        XMMATRIX matProjectionViewport;
        matProjectionViewport = XMMatrixMultiply( m_matView, m_matProj );

        // Transform the light vector to be in object space
        XMVECTOR vDeterminant;
        XMMATRIX matInverse = XMMatrixInverse( &vDeterminant, m_matWorld );
        XMVECTOR vLocalLightPos = XMVector4Transform( m_vLightPos, matInverse );

        // Compute composite matrix.
        XMMATRIX matComposite;
        matComposite = XMMatrixMultiply( m_matWorld, m_matView );
        matComposite = XMMatrixMultiply( matComposite, m_matProj );

        matComposite = XMMatrixTranspose( matComposite );
        ATG::g_pd3dDevice->SetVertexShaderConstantF( VSCONT_matWorldViewProj, ( FLOAT* )&matComposite, 4 );
        ATG::g_pd3dDevice->SetVertexShaderConstantF( VSCONT_vLightPos, ( FLOAT* )&vLocalLightPos, 1 );

        // The distance the volume is protruded.
        static FLOAT fExtrusionFactor[4] = { 50, 0.0f, 0.0f, 1.0f };
        ATG::g_pd3dDevice->SetVertexShaderConstantF( VSCONT_fOffsetScale, fExtrusionFactor, 1 );

        static FLOAT fBackFacingExtrusionFactor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        ATG::g_pd3dDevice->SetVertexShaderConstantF( VSCONT_fOffsetScale2, fBackFacingExtrusionFactor, 1 );

        // Draw the mesh
        DWORD index = pFrame - m_pMesh->m_pFrames;
        RenderVolumeMesh( pFrame->m_pMeshData, index );
    }

    // Render any child frames
    if( pFrame->m_pChild )
        RenderVolumeFrame( pFrame->m_pChild );

    // Restore the transformation matrix
    m_matWorld = matSavedWorld;

    // Render any sibling frames
    if( pFrame->m_pNext )
        RenderVolumeFrame( pFrame->m_pNext );
}


//--------------------------------------------------------------------------------------
// Name: CShadowMeshCPU()
// Desc: Constructor
//--------------------------------------------------------------------------------------
CShadowMeshCPU::CShadowMeshCPU()
{
    m_pMesh = NULL;
    m_pMeshShadowData = NULL;
}


//--------------------------------------------------------------------------------------
// Name: ~CShadowMeshCPU()
// Desc: Destructor - Frees the additional data if it has been allocated.
//--------------------------------------------------------------------------------------
CShadowMeshCPU::~CShadowMeshCPU()
{
    if( m_pMeshShadowData )
    {
        // Free buffers.
        for( DWORD frame = 0; frame < m_pMesh->m_dwNumFrames; frame++ )
        {
            // Free buffers.
            if( m_pMeshShadowData[frame].m_pVB )
                m_pMeshShadowData[frame].m_pVB->Release();

            if( m_pMeshShadowData[frame].m_pTriIndices )
                m_pMeshShadowData[frame].m_pTriIndices->Release();

            if( m_pMeshShadowData[frame].m_pQuadIndices )
                m_pMeshShadowData[frame].m_pQuadIndices->Release();

            delete[] m_pMeshShadowData[frame].m_pFaces;
            delete[] m_pMeshShadowData[frame].m_pEdges;
            delete[] m_pMeshShadowData[frame].m_pFrontFacing;
        }
    }

    delete[] m_pMeshShadowData;
}


//--------------------------------------------------------------------------------------
// Name: Create()
// Desc: Allocate space and compute the shadow mesh data from the base mesh.
//--------------------------------------------------------------------------------------
HRESULT CShadowMeshCPU::Create( ATG::Mesh* pMesh )
{
    if( NULL == pMesh )
        return E_INVALIDARG;

    m_pMesh = pMesh;

    m_pMeshShadowData = new MESH_CPUSHADOW_DATA[m_pMesh->m_dwNumFrames];
    if( NULL == m_pMeshShadowData )
        return E_OUTOFMEMORY;

    ZeroMemory( m_pMeshShadowData, sizeof( MESH_CPUSHADOW_DATA ) * m_pMesh->m_dwNumFrames );

    if( m_pMesh->m_pFrames )
        return CalculateFrameShadowData( m_pMesh->m_pFrames );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CalculateFrameShadowData()
// Desc: Recursively calculate shadow mesh data for each frame. 
//--------------------------------------------------------------------------------------
HRESULT CShadowMeshCPU::CalculateFrameShadowData( ATG::MESH_FRAME* pFrame )
{
    HRESULT hr;

    // Process the mesh data
    if( pFrame->m_pMeshData->m_dwNumSubsets )
    {
        DWORD index = pFrame - m_pMesh->m_pFrames;
        if( FAILED( hr = CalculateMeshShadowData( pFrame->m_pMeshData,
                                                  &m_pMeshShadowData[index] ) ) )
            return hr;
    }

    // Process any child frames
    if( pFrame->m_pChild )
        if( FAILED( hr = CalculateFrameShadowData( pFrame->m_pChild ) ) )
            return hr;

    // Process any sibling frames
    if( pFrame->m_pNext )
        if( FAILED( hr = CalculateFrameShadowData( pFrame->m_pNext ) ) )
            return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CalculateMeshShadowData()
// Desc: Calculate shadow mesh data for the actual indices and vertices.
//--------------------------------------------------------------------------------------
HRESULT CShadowMeshCPU::CalculateMeshShadowData( ATG::MESH_DATA* pMesh,
                                                 MESH_CPUSHADOW_DATA* pShadowData )
{
    HRESULT hr;

    // We only deal with triangle lists for simplicity.
    if( pMesh->m_dwPrimType != D3DPT_TRIANGLELIST )
        return E_INVALIDARG;

    // Lock index and vertex buffers.
    BYTE* pMeshVertices;
    WORD* pMeshIndices;

    pMesh->m_VB.Lock( 0L, 0L, ( VOID** )&pMeshVertices, D3DLOCK_READONLY );
    pMesh->m_IB.Lock( 0L, 0L, ( VOID** )&pMeshIndices, D3DLOCK_READONLY );

    DWORD dwVertexSize = pMesh->m_dwVertexSize;

    // Create mesh shadow data.
    DWORD dwTempCount = 0;
    XMVECTOR* pTempVerts = new XMVECTOR[pMesh->m_dwNumVertices];
    if( NULL == pTempVerts )
        return E_OUTOFMEMORY;

    DWORD dwNumFaces = pMesh->m_dwNumIndices / 3;
    DWORD dwNumEdges = 0;

    SHADOWMESH_FACE* pFaces = new SHADOWMESH_FACE[dwNumFaces];
    if( pFaces == NULL )
    {
        delete[] pTempVerts;
        return E_OUTOFMEMORY;
    }
    SHADOWMESH_EDGE* pEdges = new SHADOWMESH_EDGE[dwNumFaces * 3];
    if( pEdges == NULL )
    {
        delete[] pTempVerts;
        delete[] pFaces;
        return E_OUTOFMEMORY;
    }

    // Build the data from the shadow mesh.
    if( FAILED( hr = BuildShadowMesh( dwNumFaces, pMeshIndices,
                                      pMeshVertices, dwVertexSize,
                                      dwTempCount, pTempVerts,
                                      dwNumEdges, pEdges, pFaces ) ) )
    {
        delete[] pTempVerts;
        delete[] pFaces;
        delete[] pEdges;
        return hr;
    }

    // Keep face data.
    pShadowData->m_pFaces = pFaces;
    pShadowData->m_dwNumFaces = dwNumFaces;

    // Re-allocate and copy edge data.
    pShadowData->m_dwNumEdges = dwNumEdges;
    pShadowData->m_pEdges = new SHADOWMESH_EDGE[dwNumEdges];
    if( NULL == pShadowData->m_pEdges )
    {
        delete[] pTempVerts;
        return E_OUTOFMEMORY;
    }

    memcpy( pShadowData->m_pEdges, pEdges, sizeof( SHADOWMESH_EDGE ) * pShadowData->m_dwNumEdges );

    delete[] pEdges;

    pShadowData->m_dwNumVertices = dwTempCount * 2;

    // assert( pShadowData->m_dwNumVertices < 65536 );

    // Create vertex buffer with offset and non-offset versions of each vert.
    hr = ATG::g_pd3dDevice->CreateVertexBuffer( dwTempCount * 2 * sizeof( XMVECTOR ),
                                                D3DUSAGE_WRITEONLY, 0,
                                                D3DPOOL_DEFAULT, &pShadowData->m_pVB, NULL );
    if( FAILED( hr ) )
    {
        delete[] pTempVerts;
        return hr;
    }

    FLOAT* pShadowVertexData;
    pShadowData->m_pVB->Lock( 0L, 0L, ( VOID** )&pShadowVertexData, 0L );

    for( DWORD i = 0; i < dwTempCount; i++ )
    {
        // Non-offset vert.
        ( *pShadowVertexData++ ) = pTempVerts[i].x;
        ( *pShadowVertexData++ ) = pTempVerts[i].y;
        ( *pShadowVertexData++ ) = pTempVerts[i].z;
        ( *pShadowVertexData++ ) = 0.0f;
    }

    for( DWORD i = 0; i < dwTempCount; i++ )
    {
        // Offset vert.
        ( *pShadowVertexData++ ) = pTempVerts[i].x;
        ( *pShadowVertexData++ ) = pTempVerts[i].y;
        ( *pShadowVertexData++ ) = pTempVerts[i].z;
        ( *pShadowVertexData++ ) = 1.0f;
    }

    pShadowData->m_pVB->Unlock();

    // Free temp vertices.
    delete[] pTempVerts;

    // Unlock buffers.
    pMesh->m_VB.Unlock();
    pMesh->m_IB.Unlock();

    // Allocate temp data for drawing the mesh.
    pShadowData->m_pFrontFacing = new BOOL[pShadowData->m_dwNumFaces];
    ATG::g_pd3dDevice->CreateIndexBuffer( sizeof( WORD ) * pShadowData->m_dwNumFaces * 3, 0, D3DFMT_INDEX16,
                                          D3DPOOL_DEFAULT, &pShadowData->m_pTriIndices, NULL );
    ATG::g_pd3dDevice->CreateIndexBuffer( sizeof( WORD ) * pShadowData->m_dwNumEdges * 4, 0, D3DFMT_INDEX16,
                                          D3DPOOL_DEFAULT, &pShadowData->m_pQuadIndices, NULL );

    if( NULL == pShadowData->m_pFrontFacing )
        return E_OUTOFMEMORY;
    if( NULL == pShadowData->m_pTriIndices )
        return E_OUTOFMEMORY;
    if( NULL == pShadowData->m_pQuadIndices )
        return E_OUTOFMEMORY;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ComputeVolumeFrame()
// Desc: Recursively compute the shadow volume for each frame.
//--------------------------------------------------------------------------------------
VOID CShadowMeshCPU::ComputeVolumeFrame( ATG::MESH_FRAME* pFrame )
{
    // Apply the frame's local transform
    XMMATRIX matSavedWorld = m_matWorld;
    m_matWorld = XMMatrixMultiply( pFrame->m_matTransform, matSavedWorld );

    // Compute shadow volume for this mesh
    if( pFrame->m_pMeshData->m_dwNumSubsets )
    {
        DWORD index = pFrame - m_pMesh->m_pFrames;
        ComputeVolumeMesh( pFrame->m_pMeshData, &m_pMeshShadowData[index] );
    }

    // Compute shadow volume for any child frames
    if( pFrame->m_pChild )
        ComputeVolumeFrame( pFrame->m_pChild );

    // Restore the transformation matrix
    m_matWorld = matSavedWorld;

    // Compute shadow volume for any sibling frames
    if( pFrame->m_pNext )
        ComputeVolumeFrame( pFrame->m_pNext );
}


//--------------------------------------------------------------------------------------
// Name: ComputeVolumeMesh()
// Desc: Compute the shadow volume for the actual indices and vertices.
//--------------------------------------------------------------------------------------
VOID CShadowMeshCPU::ComputeVolumeMesh( ATG::MESH_DATA* pMesh,
                                        MESH_CPUSHADOW_DATA* pShadowData )
{
    // Transform the light vector to be in object space
    XMVECTOR vDeterminant;
    XMMATRIX matInverse = XMMatrixInverse( &vDeterminant, m_matWorld );
    XMVECTOR vLocalLightPos = XMVector4Transform( m_vLightPos, matInverse );

    // The second half of the vertices are offset.
    WORD wOffset = ( WORD )( pShadowData->m_dwNumVertices / 2 );

    DWORD dwNumTriIndices = 0;
    DWORD dwNumQuadIndices = 0;

    BOOL* bFrontFacing = pShadowData->m_pFrontFacing;
    WORD* pTriIndices;
    WORD* pQuadIndices;

    pShadowData->m_pTriIndices->Lock( 0, 0, ( VOID** )&pTriIndices, 0 );
    pShadowData->m_pQuadIndices->Lock( 0, 0, ( VOID** )&pQuadIndices, 0 );

    SHADOWMESH_FACE* pFaces = pShadowData->m_pFaces;

    // Determine if the light is in front of or behind each face and
    // add the faces to the draw list.
    for( DWORD face = 0; face < pShadowData->m_dwNumFaces; face++ )
    {
        if( XMPlaneDot( pFaces[face].Plane, vLocalLightPos ).x > 0.0f )
        {
            bFrontFacing[face] = TRUE;
        }
        else
        {
            bFrontFacing[face] = FALSE;
            // Add tri with offset.
            *pTriIndices++ = pFaces[face].V[0] + wOffset;
            *pTriIndices++ = pFaces[face].V[1] + wOffset;
            *pTriIndices++ = pFaces[face].V[2] + wOffset;
            dwNumTriIndices += 3;
        }
    }

    SHADOWMESH_EDGE* pEdges = pShadowData->m_pEdges;

    // Determine which edges are potential silhouette edges.
    for( DWORD edge = 0; edge < pShadowData->m_dwNumEdges; edge++ )
    {
        if( bFrontFacing[pEdges[edge].Face0] ^ bFrontFacing[pEdges[edge].Face1] )
        {
            // Potential silhouette edge.
            if( bFrontFacing[pEdges[edge].Face1] )
            {
                // Add quad, reverse edge order since it matches Face0.
                *pQuadIndices++ = pEdges[edge].V[0];
                *pQuadIndices++ = pEdges[edge].V[1];
                *pQuadIndices++ = pEdges[edge].V[1] + wOffset;
                *pQuadIndices++ = pEdges[edge].V[0] + wOffset;
                dwNumQuadIndices += 4;
            }
            else
            {
                // Add quad, edge order matches Face0.
                *pQuadIndices++ = pEdges[edge].V[1];
                *pQuadIndices++ = pEdges[edge].V[0];
                *pQuadIndices++ = pEdges[edge].V[0] + wOffset;
                *pQuadIndices++ = pEdges[edge].V[1] + wOffset;
                dwNumQuadIndices += 4;
            }
        }
    }

    pShadowData->m_pTriIndices->Unlock();
    pShadowData->m_pQuadIndices->Unlock();

    // Save the current data.
    pShadowData->m_dwNumTriIndices = dwNumTriIndices;
    pShadowData->m_dwNumQuadIndices = dwNumQuadIndices;
}


//--------------------------------------------------------------------------------------
// Name: SetLightPos()
// Desc: Sets the external light position
//--------------------------------------------------------------------------------------
VOID CShadowMeshCPU::SetLightPos( const XMVECTOR vLightPos )
{
    m_vLightPos = vLightPos;

    if( m_pMesh->m_pFrames )
        ComputeVolumeFrame( m_pMesh->m_pFrames );
}


//--------------------------------------------------------------------------------------
// Name: RenderVolumeMesh()
// Desc: Render the shadow volume for the actual indices and vertices.
//--------------------------------------------------------------------------------------
VOID CShadowMeshCPU::RenderVolumeMesh( ATG::MESH_DATA* pMesh, DWORD index )
{
    // Draw the shadow volume.
    MESH_CPUSHADOW_DATA* pShadowData = &m_pMeshShadowData[index];
    ATG::g_pd3dDevice->SetStreamSource( 0, pShadowData->m_pVB, 0, sizeof( XMVECTOR ) );

    if( pShadowData->m_dwNumQuadIndices )
    {
        ATG::g_pd3dDevice->SetIndices( pShadowData->m_pQuadIndices );
        ATG::g_pd3dDevice->DrawIndexedPrimitive( D3DPT_QUADLIST, 0, 0, 0, 0,
                                                 pShadowData->m_dwNumQuadIndices / 4 );
    }

    if( pShadowData->m_dwNumTriIndices )
    {
        ATG::g_pd3dDevice->SetIndices( pShadowData->m_pTriIndices );
        ATG::g_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, 0, 0,
                                                 pShadowData->m_dwNumTriIndices / 3 );
    }
    ATG::g_pd3dDevice->SetIndices( NULL );
}


//--------------------------------------------------------------------------------------
// Name: CShadowMeshGPU()
// Desc: Constructor
//--------------------------------------------------------------------------------------
CShadowMeshGPU::CShadowMeshGPU()
{
    m_pMesh = NULL;
    m_pMeshShadowData = NULL;
}


//--------------------------------------------------------------------------------------
// Name: ~CShadowMeshGPU()
// Desc: Destructor
//--------------------------------------------------------------------------------------
CShadowMeshGPU::~CShadowMeshGPU()
{
    if( m_pMeshShadowData )
    {
        for( DWORD frame = 0; frame < m_pMesh->m_dwNumFrames; frame++ )
        {
            // Free buffers.
            if( m_pMeshShadowData[frame].m_pVB )
                m_pMeshShadowData[frame].m_pVB->Release();

            if( m_pMeshShadowData[frame].m_pQuadIndices )
                m_pMeshShadowData[frame].m_pQuadIndices->Release();
        }
    }

    delete[] m_pMeshShadowData;
}


//--------------------------------------------------------------------------------------
// Name: Create()
// Desc: Allocate space and compute the shadow mesh data from the base mesh.
//--------------------------------------------------------------------------------------
HRESULT CShadowMeshGPU::Create( ATG::Mesh* pMesh )
{
    if( NULL == pMesh )
        return E_INVALIDARG;

    m_pMesh = pMesh;

    m_pMeshShadowData = new MESH_GPUSHADOW_DATA[m_pMesh->m_dwNumFrames];
    if( NULL == m_pMeshShadowData )
        return E_OUTOFMEMORY;

    ZeroMemory( m_pMeshShadowData, sizeof( MESH_GPUSHADOW_DATA ) * m_pMesh->m_dwNumFrames );

    if( m_pMesh->m_pFrames )
        return CalculateFrameShadowData( m_pMesh->m_pFrames );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CalculateFrameShadowData()
// Desc: Recursively calculate shadow mesh data for each frame. 
//--------------------------------------------------------------------------------------
HRESULT CShadowMeshGPU::CalculateFrameShadowData( ATG::MESH_FRAME* pFrame )
{
    HRESULT hr;

    // Process the mesh data
    if( pFrame->m_pMeshData->m_dwNumSubsets )
    {
        DWORD index = pFrame - m_pMesh->m_pFrames;
        if( FAILED( hr = CalculateMeshShadowData( pFrame->m_pMeshData,
                                                  &m_pMeshShadowData[index] ) ) )
            return hr;
    }

    // Process any child frames
    if( pFrame->m_pChild )
        if( FAILED( hr = CalculateFrameShadowData( pFrame->m_pChild ) ) )
            return hr;

    // Process any sibling frames
    if( pFrame->m_pNext )
        if( FAILED( hr = CalculateFrameShadowData( pFrame->m_pNext ) ) )
            return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CalculateMeshShadowData()
// Desc: Calculate shadow mesh data for the actual indices and vertices.
//--------------------------------------------------------------------------------------
HRESULT CShadowMeshGPU::CalculateMeshShadowData( ATG::MESH_DATA* pMesh,
                                                 MESH_GPUSHADOW_DATA* pShadowData )
{
    HRESULT hr;

    // We only deal with triangle lists for simplicity.
    if( pMesh->m_dwPrimType != D3DPT_TRIANGLELIST )
        return E_INVALIDARG;

    // Lock index and vertex buffers.
    BYTE* pMeshVertices;
    WORD* pMeshIndices;
    pMesh->m_VB.Lock( 0L, 0L, ( VOID** )&pMeshVertices, D3DLOCK_READONLY );
    pMesh->m_IB.Lock( 0L, 0L, ( VOID** )&pMeshIndices, D3DLOCK_READONLY );

    DWORD dwVertexSize = pMesh->m_dwVertexSize;

    // Create mesh shadow data.
    DWORD dwTempCount = 0;
    XMVECTOR* pTempVerts = new XMVECTOR[pMesh->m_dwNumVertices];
    if( NULL == pTempVerts )
        return E_OUTOFMEMORY;

    DWORD dwNumFaces = pMesh->m_dwNumIndices / 3;
    DWORD dwNumEdges = 0;

    SHADOWMESH_FACE* pFaces = new SHADOWMESH_FACE[dwNumFaces];
    SHADOWMESH_EDGE* pEdges = new SHADOWMESH_EDGE[dwNumFaces * 3];
    if( NULL == pFaces || NULL == pEdges )
        return E_OUTOFMEMORY;

    // Build the data from the shadow mesh.
    if( FAILED( hr = BuildShadowMesh( dwNumFaces, pMeshIndices,
                                      pMeshVertices, dwVertexSize,
                                      dwTempCount, pTempVerts,
                                      dwNumEdges, pEdges, pFaces ) ) )
        return hr;

    // Build a vertex buffer containing each vertex duplicated with the normal 
    // of each face it is shared by.
    pShadowData->m_dwNumVertices = dwNumFaces * 3;

    // assert( pShadowData->m_dwNumVertices < 65536 );

    struct SHADOWVERTEX
    {
        XMFLOAT3 Pos;
        XMFLOAT4 Plane;
    };

    // Create vertex buffer.
    hr = ATG::g_pd3dDevice->CreateVertexBuffer( pShadowData->m_dwNumVertices * sizeof( SHADOWVERTEX ),
                                                D3DUSAGE_WRITEONLY, 0,
                                                D3DPOOL_DEFAULT, &pShadowData->m_pVB, NULL );
    if( FAILED( hr ) )
        return hr;

    SHADOWVERTEX* pSrcVertices = new SHADOWVERTEX[pShadowData->m_dwNumVertices];
    for( DWORD face = 0; face < dwNumFaces; face++ )
    {
        // Add triangle vertices with face normal.
        for( DWORD i = 0; i < 3; i++ )
        {
            XMStoreFloat3( &pSrcVertices[face * 3 + i].Pos, pTempVerts[pFaces[face].V[i]] );
            XMStoreFloat4( &pSrcVertices[face * 3 + i].Plane, pFaces[face].Plane );
        }
    }

    SHADOWVERTEX* pDstVertices;
    pShadowData->m_pVB->Lock( 0L, 0L, ( VOID** )&pDstVertices, 0L );
    memcpy( pDstVertices, pSrcVertices, sizeof( SHADOWVERTEX ) * pShadowData->m_dwNumVertices );
    pShadowData->m_pVB->Unlock();
    delete[] pSrcVertices;

    // Free temp vertices.
    delete[] pTempVerts;

    // Build a quad-list for the edges.
    pShadowData->m_dwNumQuadIndices = 0;
    ATG::g_pd3dDevice->CreateIndexBuffer( sizeof( WORD ) * dwNumEdges * 4, 0, D3DFMT_INDEX16,
                                          D3DPOOL_DEFAULT, &pShadowData->m_pQuadIndices, NULL );

    if( NULL == pShadowData->m_pQuadIndices )
        return E_OUTOFMEMORY;

    WORD* pQuadIndices;
    pShadowData->m_pQuadIndices->Lock( 0, 0, ( VOID** )&pQuadIndices, 0 );

    for( DWORD edge = 0; edge < dwNumEdges; edge++ )
    {
        WORD* pQuad = pQuadIndices + pShadowData->m_dwNumQuadIndices;

        // Each quad consists of the two vertices of the edge from one face
        // and the two vertices of the edge from the other face.
        WORD face0 = ( WORD )pEdges[edge].Face0;
        WORD face1 = ( WORD )pEdges[edge].Face1;

        // Which edge of Face0 does this edge correspond to?
        for( WORD i0 = 2, i1 = 0; i1 < 3; i1++ )
        {
            if( pEdges[edge].V[0] == pFaces[face0].V[i0] && pEdges[edge].V[1] == pFaces[face0].V[i1] )
            {
                pQuad[0] = face0 * 3 + i1;
                pQuad[1] = face0 * 3 + i0;
                break;
            }

            i0 = i1;
        }

        // Which edge of Face1 does this edge correspond to?
        for( WORD i0 = 2, i1 = 0; i1 < 3; i1++ )
        {
            // Note that vert order in edge is opposite vert order in face.
            if( pEdges[edge].V[1] == pFaces[face1].V[i0] && pEdges[edge].V[0] == pFaces[face1].V[i1] )
            {
                pQuad[2] = face1 * 3 + i1;
                pQuad[3] = face1 * 3 + i0;
                break;
            }

            i0 = i1;
        }

        pShadowData->m_dwNumQuadIndices += 4;
    }

    pShadowData->m_pQuadIndices->Unlock();

    // Discard face and edge data.
    delete[] pFaces;
    delete[] pEdges;

    // Unlock buffers.
    pMesh->m_VB.Unlock();
    pMesh->m_IB.Unlock();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetLightPos()
// Desc: Sets the external light position
//--------------------------------------------------------------------------------------
VOID CShadowMeshGPU::SetLightPos( const XMVECTOR vLightPos )
{
    m_vLightPos = vLightPos;
}


//--------------------------------------------------------------------------------------
// Name: RenderVolumeMesh()
// Desc: Render the shadow volume for the actual indices and vertices.
//--------------------------------------------------------------------------------------
VOID CShadowMeshGPU::RenderVolumeMesh( ATG::MESH_DATA* pMesh, DWORD index )
{
    // Draw the shadow volume.
    MESH_GPUSHADOW_DATA* pShadowData = &m_pMeshShadowData[index];
    ATG::g_pd3dDevice->SetStreamSource( 0, pShadowData->m_pVB, 0, 7 * sizeof( FLOAT ) );

    ATG::g_pd3dDevice->SetIndices( pShadowData->m_pQuadIndices );
    ATG::g_pd3dDevice->DrawIndexedPrimitive( D3DPT_QUADLIST, 0, 0, 0, 0,
                                             pShadowData->m_dwNumQuadIndices / 4 );

    // Extrude the back facing tris off into never-never land
    static FLOAT fBackFacingExtrusionFactor[4] = { 99999.0f, 0.0f, 0.0f, 1.0f };
    ATG::g_pd3dDevice->SetVertexShaderConstantF( VSCONT_fOffsetScale2, fBackFacingExtrusionFactor, 1 );

    ATG::g_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0,
                                      pShadowData->m_dwNumVertices / 3 );
}
