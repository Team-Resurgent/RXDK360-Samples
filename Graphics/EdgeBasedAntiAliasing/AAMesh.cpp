//--------------------------------------------------------------------------------------
// AAMesh.cpp
//
// Support code for building and rendering anti-aliasing edges. These edges are rendered
// after a model has been rendered in order to anti-alias the model's edges. 
//
// There are two types of edges that usually exhibit aliasing: interior and silhouette.
// Interior edges are edges that lie on the boundary of discontinuous shading, usually 
// due to a change in surface normal.  These edges can be found in a pre-processing step.
// Silhouette edges change dynamically and must be computed every frame.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <AtgMesh.h>
#include <AtgApp.h>
#include "AAMesh.h"

// Disable warning C6211: Leaking memory <pointer> due to an exception.
// We "know" that we won't run out of memory in this sample
#pragma warning ( disable : 6211 )

const DWORD VSCONT_matWorldViewProj = 0;  // World-view-projection matrix

typedef struct
{
    XMFLOAT3 vPos;
    XMFLOAT3 vNextPos;
} AAEdgeVert;


//--------------------------------------------------------------------------------------
// Name: CAAMesh()
// Desc: Constructor
//--------------------------------------------------------------------------------------
CAAMesh::CAAMesh()
{
    m_pMesh = NULL;
    m_pAAMeshData = NULL;
}


//--------------------------------------------------------------------------------------
// Name: ~CAAMesh()
// Desc: Destructor - Frees the additional data if it has been allocated.
//--------------------------------------------------------------------------------------
CAAMesh::~CAAMesh()
{
    if( m_pAAMeshData )
    {
        // Free buffers.
        for( DWORD frame = 0; frame < m_pMesh->m_dwNumFrames; frame++ )
        {
            // Free buffers.
            if( m_pAAMeshData[frame].m_pInteriorEdgeVB )
                m_pAAMeshData[frame].m_pInteriorEdgeVB->Release();

            for( DWORD i = 0; i < 2; i++)
            {
                if( m_pAAMeshData[frame].m_pSilhouetteEdgeVB[i] )
                    m_pAAMeshData[frame].m_pSilhouetteEdgeVB[i]->Release();
            }

            delete[] m_pAAMeshData[frame].m_pFaces;
            delete[] m_pAAMeshData[frame].m_pEdges;
            delete[] m_pAAMeshData[frame].m_pVerts;
            delete[] m_pAAMeshData[frame].m_pFrontFacing;
        }
    }

    delete[] m_pAAMeshData;
}


//--------------------------------------------------------------------------------------
// Name: CalculateFrameAAData()
// Desc: Recursively calculate AA mesh data for each frame. 
//--------------------------------------------------------------------------------------
HRESULT CAAMesh::CalculateFrameAAData( ATG::MESH_FRAME* pFrame )
{
    HRESULT hr;

    // Process the mesh data
    if( pFrame->m_pMeshData->m_dwNumSubsets )
    {
        DWORD index = pFrame - m_pMesh->m_pFrames;
        if( FAILED( hr = CalculateMeshAAData( pFrame->m_pMeshData,
                                                  &m_pAAMeshData[index] ) ) )
            return hr;
    }

    // Process any child frames
    if( pFrame->m_pChild )
        if( FAILED( hr = CalculateFrameAAData( pFrame->m_pChild ) ) )
            return hr;

    // Process any sibling frames
    if( pFrame->m_pNext )
        if( FAILED( hr = CalculateFrameAAData( pFrame->m_pNext ) ) )
            return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CalculateMeshAAData()
// Desc: Calculate AA mesh data for the actual indices and vertices.
//--------------------------------------------------------------------------------------
HRESULT CAAMesh::CalculateMeshAAData( ATG::MESH_DATA* __restrict pMesh,
                                      AA_MESH_DATA* __restrict pMeshData )
{
    HRESULT hr;

    // We only deal with triangle lists and strips for simplicity.
    if( pMesh->m_dwPrimType != D3DPT_TRIANGLELIST && pMesh->m_dwPrimType != D3DPT_TRIANGLESTRIP )
        return E_INVALIDARG;

    // Lock index and vertex buffers.
    BYTE* pMeshVertices = NULL;
    WORD* pMeshIndices = NULL;

    DWORD dwVertexSize = pMesh->m_dwVertexSize;

    DWORD dwTempCount = 0;

    XMFLOAT3* pVerts = new XMFLOAT3[pMesh->m_dwNumVertices];

    if( NULL == pVerts )
        return E_OUTOFMEMORY;

    DWORD dwNumFaces = 0;
    DWORD dwNumEdges = 0;

    if( pMesh->m_dwPrimType == D3DPT_TRIANGLELIST )
    {
        WORD* pTempIndices;
        pMesh->m_VB.Lock( 0L, 0L, ( VOID** )&pMeshVertices, D3DLOCK_READONLY );
        pMesh->m_IB.Lock( 0L, 0L, ( VOID** )&pTempIndices, D3DLOCK_READONLY );

        DWORD dwNumIndices = pMesh->m_dwNumIndices;
        pMeshIndices = new WORD[dwNumIndices];
        if( pMeshIndices == NULL )
        {
            delete[] pVerts;
            return E_OUTOFMEMORY;
        }
        
        XMemCpy( pMeshIndices, pTempIndices, dwNumIndices * sizeof(WORD) );
        
        dwNumFaces = pMesh->m_dwNumIndices / 3;
    }
    else if( pMesh->m_dwPrimType == D3DPT_TRIANGLESTRIP )
    {
        // Convert the triangle strip to a triangle list
        WORD* pTempIndices;
        pMesh->m_VB.Lock( 0L, 0L, ( VOID** )&pMeshVertices, D3DLOCK_READONLY );
        pMesh->m_IB.Lock( 0L, 0L, ( VOID** )&pTempIndices, D3DLOCK_READONLY );

        DWORD dwNumIndices = ( pMesh->m_dwNumIndices - 2 ) * 3;
        pMeshIndices = new WORD[dwNumIndices];
        if( pMeshIndices == NULL )
        {
            delete[] pVerts;
            return E_OUTOFMEMORY;
        }

        WORD* pCurIndex = pMeshIndices;

        dwNumFaces = 0;
        for( DWORD i = 0; i < pMesh->m_dwNumIndices - 2; i++ )
        {
            if( pTempIndices[i]     == pTempIndices[i + 1] || 
                pTempIndices[i + 1] == pTempIndices[i + 2] ||
                pTempIndices[i]     == pTempIndices[i + 2] )
            {
                // Degenerate triangle, don't add
                continue;
            }
            if( i % 2 == 0 )
            {
                *pCurIndex++ = pTempIndices[i];
                *pCurIndex++ = pTempIndices[i + 1];
                *pCurIndex++ = pTempIndices[i + 2];
            }
            else // Flip the vertex order on every other triangle
            {
                *pCurIndex++ = pTempIndices[i];
                *pCurIndex++ = pTempIndices[i + 2];
                *pCurIndex++ = pTempIndices[i + 1];
            }
            dwNumFaces++;
        }
    }

    AA_MESH_FACE* pFaces = new AA_MESH_FACE[dwNumFaces];
    if( pFaces == NULL )
    {
        delete[] pVerts;
        delete[] pMeshIndices;
        return E_OUTOFMEMORY;
    }

    AA_MESH_EDGE* pEdges = new AA_MESH_EDGE[dwNumFaces * 3];
    if( pEdges == NULL )
    {
        delete[] pVerts;
        delete[] pMeshIndices;
        delete[] pFaces;
        return E_OUTOFMEMORY;
    }

    // Build the data from the mesh.
    if( FAILED( hr = BuildAAMesh( dwNumFaces, pMeshIndices,
                                      pMeshVertices, dwVertexSize,
                                      dwTempCount, pVerts,
                                      dwNumEdges, pEdges, pFaces, pMeshData ) ) )
    {
        delete[] pVerts;
        delete[] pMeshIndices;
        delete[] pFaces;
        delete[] pEdges;
        return hr;
    }

    // Keep face and vert data.
    pMeshData->m_pFaces = pFaces;
    pMeshData->m_dwNumFaces = dwNumFaces;

    pMeshData->m_pVerts = pVerts;
    pMeshData->m_dwNumVerts = dwTempCount;

    // Re-allocate and copy edge data.
    pMeshData->m_dwNumEdges = dwNumEdges;
    pMeshData->m_pEdges = new AA_MESH_EDGE[dwNumEdges];
    if( NULL == pMeshData->m_pEdges )
    {
        delete[] pVerts;
        delete[] pMeshIndices;
        return E_OUTOFMEMORY;
    }

    XMemCpy( pMeshData->m_pEdges, pEdges, sizeof( AA_MESH_EDGE ) * pMeshData->m_dwNumEdges );

    delete[] pEdges;

    // Free temp indices
    delete[] pMeshIndices;

    // Unlock buffers.
    pMesh->m_VB.Unlock();
    pMesh->m_IB.Unlock();

    // Allocate Index VBs for silhouette edges
    pMeshData->m_pFrontFacing = new BOOL[pMeshData->m_dwNumFaces];
    for( DWORD i = 0; i < 2; i++ )
    {
        ATG::g_pd3dDevice->CreateVertexBuffer( sizeof( AAEdgeVert ) * pMeshData->m_dwNumEdges * 4, 0, 0,
                                               D3DPOOL_DEFAULT, &pMeshData->m_pSilhouetteEdgeVB[i], NULL );
    }

    if( NULL == pMeshData->m_pFrontFacing )
        return E_OUTOFMEMORY;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: BuildAAMesh()
// Desc: Collapses any vertices that share a postion value into a single vertex. Builds
//       the face and edge lists for the AA mesh.
//--------------------------------------------------------------------------------------
HRESULT CAAMesh::BuildAAMesh( const DWORD dwNumFaces,    // Number of faces.
                              const WORD* pMeshIndices,  // Input triangle indices 
                              const BYTE* pMeshVertices, // Input vertices
                              DWORD dwVertexSize,        // Size of each vertex in bytes
                              DWORD& dwVertCount,        // Number of output vertices
                              XMFLOAT3* pVerts,          // Output vertices 
                              DWORD& dwNumEdges,         // Number of output edges
                              AA_MESH_EDGE* pEdges,      // Output edges 
                              AA_MESH_FACE* pFaces,      // Output faces 
                              AA_MESH_DATA* pMeshData )
{
    // Start with zero output verts and edges.
    dwVertCount = 0;
    dwNumEdges = 0;

    // Build triangle and edge lists.
    for( DWORD dwFace = 0; dwFace < dwNumFaces; dwFace++ )
    {
        // Add unique vertex positions.
        for( DWORD i = 0; i < 3; i++ )
        {
            WORD wSrcIndex = pMeshIndices[dwFace * 3 + i];

            const XMVECTOR vPos = XMLoadFloat3( ( const XMFLOAT3* )( pMeshVertices + 
                                                ( DWORD )wSrcIndex * dwVertexSize ) );

            // Check if we already have this position.  
            WORD wDstIndex = 0;
            while( wDstIndex < dwVertCount )
            {
                // Check for exact match of position.
                if( XMVector3Equal( vPos, XMLoadFloat3( &pVerts[wDstIndex] ) ) )
                    break;

                wDstIndex++;
            }

            if( wDstIndex == dwVertCount )
            {
                // Add vertex.
                XMStoreFloat3( &pVerts[wDstIndex], vPos );
                dwVertCount++;
            }

            pFaces[dwFace].V[i] = wDstIndex;
        }

        const XMVECTOR vert0 = XMLoadFloat3( &pVerts[pFaces[dwFace].V[0]] );
        const XMVECTOR vert1 = XMLoadFloat3( &pVerts[pFaces[dwFace].V[1]] );
        const XMVECTOR vert2 = XMLoadFloat3( &pVerts[pFaces[dwFace].V[2]] );

        // Compute face plane.
        XMVECTOR vNormal;
        XMVECTOR v2Minus1 = vert2 - vert1;
        XMVECTOR v1Minus0 = vert1 - vert0;
        vNormal = XMVector3Cross( v2Minus1, v1Minus0 );
        vNormal = XMVector3Normalize( vNormal );
        pFaces[dwFace].Plane = XMPlaneFromPointNormal( vert0, vNormal );

        // Add edges.
        DWORD i0 = 2;
        for( DWORD i1 = 0; i1 < 3; i1++ )
        {
            WORD index0 = pFaces[dwFace].V[i0];
            WORD index1 = pFaces[dwFace].V[i1];

            // Check if we already have this edge.
            DWORD dwEdge = 0;
            while( dwEdge < dwNumEdges )
            {
                // Does this edge exist from an adjacent triangle.
                // Note: winding order from the adjacent triangle is reversed.
                if( pEdges[dwEdge].V[0] == index1 && pEdges[dwEdge].V[1] == index0 )
                    break;

                dwEdge++;
            }

            if( dwEdge < dwNumEdges )
            {
                // Attach to existing edge.
                pEdges[dwEdge].Face1 = dwFace;
            }
            else
            {
                // Add new edge.
                pEdges[dwEdge].V[0] = index0;
                pEdges[dwEdge].V[1] = index1;
                pEdges[dwEdge].Face0 = dwFace;
                pEdges[dwEdge].Face1 = 0xffffffff;
                dwNumEdges++;
            }

            i0 = i1;
        }
    }

    // Build a list of interior edges that need to be anti-aliased
    DWORD* pInteriorAAEdges = new DWORD[ dwNumEdges ];
    if( pInteriorAAEdges == NULL )
    {
        return E_OUTOFMEMORY;
    }

    DWORD dwNumInteriorAAEdges = 0;
    for( DWORD dwEdge = 0; dwEdge < dwNumEdges; dwEdge++ )
    {
        // Check to see if the edge has discontinuous normals.  
        BOOL bAAEdge = false;
        for( DWORD i = 0; i < 3; i++ )
        {
            DWORD i0 = pMeshIndices[pEdges[dwEdge].Face0 * 3 + i];
            for( DWORD j = 0; j < 3; j++ )
            {
                int i1 = pMeshIndices[pEdges[dwEdge].Face1 * 3 + j];

                const XMVECTOR vIPos = XMLoadFloat3( ( const XMFLOAT3* )( pMeshVertices + i0 *
                                                                           dwVertexSize ) );
                const XMVECTOR vJPos = XMLoadFloat3( ( const XMFLOAT3* )( pMeshVertices + i1 *
                                                                           dwVertexSize ) );

                const XMVECTOR vINorm = XMLoadFloat3( ( const XMFLOAT3* )( pMeshVertices + i0 *
                                                                           dwVertexSize  + 3) );
                const XMVECTOR vJNorm = XMLoadFloat3( ( const XMFLOAT3* )( pMeshVertices + i1 *
                                                                           dwVertexSize + 3 ) );

                // If it's the same vertex and the normals are different, anti-alias it.
                if( XMVector3Equal ( vIPos, vJPos)  && !XMVector3Equal (vINorm, vJNorm) )
                {
                    bAAEdge = true;
                    break;
                }

            }
        }

        if( bAAEdge )
        {
            pInteriorAAEdges[ dwNumInteriorAAEdges ] = dwEdge;
            dwNumInteriorAAEdges++;
        }
    }

    ATG::g_pd3dDevice->CreateVertexBuffer( sizeof( AAEdgeVert ) * dwNumInteriorAAEdges * 4, 0, 0,
                                           D3DPOOL_DEFAULT, &pMeshData->m_pInteriorEdgeVB, NULL );
    AAEdgeVert* pAAEdgeVerts;
    pMeshData->m_pInteriorEdgeVB->Lock( 0, 0, ( VOID** )&pAAEdgeVerts, 0 );
    pMeshData->m_dwNumInteriorEdgeVerts = 0;

    // Build the interior edge indices.  Each edge is a quad covering the edge.
    // The quad is expanded out to be a single pixel wide in the vertex shader.
    for( DWORD i = 0; i < dwNumInteriorAAEdges; i++ )
    {
        DWORD dwEdge = pInteriorAAEdges[i];

        pAAEdgeVerts->vPos = pVerts[pEdges[dwEdge].V[0]];
        pAAEdgeVerts->vNextPos = pVerts[pEdges[dwEdge].V[1]];
        ++pAAEdgeVerts;
        pAAEdgeVerts->vPos = pVerts[pEdges[dwEdge].V[0]];
        pAAEdgeVerts->vNextPos = pVerts[pEdges[dwEdge].V[1]];
        ++pAAEdgeVerts;
        pAAEdgeVerts->vPos = pVerts[pEdges[dwEdge].V[1]];
        pAAEdgeVerts->vNextPos = pVerts[pEdges[dwEdge].V[0]];
        ++pAAEdgeVerts;
        pAAEdgeVerts->vPos = pVerts[pEdges[dwEdge].V[1]];
        pAAEdgeVerts->vNextPos = pVerts[pEdges[dwEdge].V[0]];
        ++pAAEdgeVerts;

        pMeshData->m_dwNumInteriorEdgeVerts += 4;
    }

    pMeshData->m_pInteriorEdgeVB->Unlock();

    delete[] pInteriorAAEdges;
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderAAMesh()
// Desc: Render the AA edges
//--------------------------------------------------------------------------------------
VOID CAAMesh::RenderAAMesh( )
{
    ATG::g_pd3dDevice->SetVertexDeclaration( m_pAAVertexDecl );
    ATG::g_pd3dDevice->SetVertexShader( m_pVS );
    ATG::g_pd3dDevice->SetPixelShader( m_pPS );

    ATG::g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );

    ATG::g_pd3dDevice->SetTexture( 1, m_pFrameBuffer );

    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );

    if( m_pMesh->m_pFrames )
        RenderAAMeshFrame( m_pMesh->m_pFrames );
}

//--------------------------------------------------------------------------------------
// Name: RenderAAMeshFrame()
// Desc: Recursively render the AA Mesh for each frame.
//--------------------------------------------------------------------------------------
VOID CAAMesh::RenderAAMeshFrame( ATG::MESH_FRAME* pFrame )
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

        // Transform the view vector to be in object space
        XMVECTOR vDeterminant;
        XMMATRIX matInverse = XMMatrixInverse( &vDeterminant, m_matWorld );

        // Compute composite matrix.
        XMMATRIX matComposite;
        matComposite = XMMatrixMultiply( m_matWorld, m_matView );
        matComposite = XMMatrixMultiply( matComposite, m_matProj );
        matComposite = XMMatrixTranspose( matComposite );
        ATG::g_pd3dDevice->SetVertexShaderConstantF( VSCONT_matWorldViewProj, ( FLOAT* )&matComposite, 4 );

        // Draw the mesh
        DWORD index = pFrame - m_pMesh->m_pFrames;
        RenderAAMesh( pFrame->m_pMeshData, index );
    }

    // Render any child frames
    if( pFrame->m_pChild )
        RenderAAMeshFrame( pFrame->m_pChild );

    // Restore the transformation matrix
    m_matWorld = matSavedWorld;

    // Render any sibling frames
    if( pFrame->m_pNext )
        RenderAAMeshFrame( pFrame->m_pNext );
}


//--------------------------------------------------------------------------------------
// Name: Create()
// Desc: Allocate space and compute the AA mesh data from the base mesh.
//--------------------------------------------------------------------------------------
HRESULT CAAMesh::Create( ATG::Mesh* pMesh )
{
    if( NULL == pMesh )
        return E_INVALIDARG;

    m_pMesh = pMesh;

    m_pAAMeshData = new AA_MESH_DATA[m_pMesh->m_dwNumFrames];
    if( NULL == m_pAAMeshData )
        return E_OUTOFMEMORY;

    ZeroMemory( m_pAAMeshData, sizeof( AA_MESH_DATA ) * m_pMesh->m_dwNumFrames );
    
    m_dwCurFrame = 0;

    if( m_pMesh->m_pFrames )
        return CalculateFrameAAData( m_pMesh->m_pFrames );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ComputeSilhouetteFrame()
// Desc: Recursively compute the silhouette for each frame.
//--------------------------------------------------------------------------------------
VOID CAAMesh::ComputeSilhouetteFrame( ATG::MESH_FRAME* pFrame )
{
    // Apply the frame's local transform
    XMMATRIX matSavedWorld = m_matWorld;
    m_matWorld = XMMatrixMultiply( pFrame->m_matTransform, matSavedWorld );

    // Compute silhouette for this mesh
    if( pFrame->m_pMeshData->m_dwNumSubsets )
    {
        DWORD index = pFrame - m_pMesh->m_pFrames;
        ComputeSilhouetteMesh( pFrame->m_pMeshData, &m_pAAMeshData[index] );
    }

    // Compute silhouette for any child frames
    if( pFrame->m_pChild )
        ComputeSilhouetteFrame( pFrame->m_pChild );

    // Restore the transformation matrix
    m_matWorld = matSavedWorld;

    // Compute silhouette for any sibling frames
    if( pFrame->m_pNext )
        ComputeSilhouetteFrame( pFrame->m_pNext );
}


//--------------------------------------------------------------------------------------
// Name: ComputeSilhouetteMesh()
// Desc: Compute the silhouette of the mesh
//--------------------------------------------------------------------------------------
VOID CAAMesh::ComputeSilhouetteMesh( ATG::MESH_DATA* __restrict pMesh,
                                        AA_MESH_DATA* __restrict pMeshData )
{
    // Transform the view vector to be in object space
    XMVECTOR vDeterminant;
    XMMATRIX matInverse = XMMatrixInverse( &vDeterminant, m_matWorld );
    XMVECTOR vLocalViewDir = XMVector4Transform( m_vViewDir, matInverse );

    DWORD dwNumSilhouetteVerts = 0;

    BOOL* __restrict pbFrontFacing = pMeshData->m_pFrontFacing;
    AAEdgeVert* __restrict pSilhouetteVerts;

    ATG::g_pd3dDevice->SetStreamSource( 0, NULL, 0, 0 );

    pMeshData->m_pSilhouetteEdgeVB[m_dwCurFrame]->Lock( 0, 0, ( VOID** )&pSilhouetteVerts, 0 );

    const AA_MESH_FACE* pFaces = pMeshData->m_pFaces;

    // Determine if the view point is in front of or behind each face and
    // add the faces to the draw list.
    for( DWORD face = 0; face < pMeshData->m_dwNumFaces; face++ )
    {
        __dcbt( 1024, &pFaces[face] );
        if( XMPlaneDot( pFaces[face].Plane, vLocalViewDir ).x > 0.0f )
        {
            pbFrontFacing[face] = TRUE;
        }
        else
        {
            pbFrontFacing[face] = FALSE;
        }
    }

    const AA_MESH_EDGE* pEdges = pMeshData->m_pEdges;

    XMFLOAT3* pVerts = pMeshData->m_pVerts;
    
    // Store the silhouette edges
    for( DWORD edge = 0; edge < pMeshData->m_dwNumEdges; edge++ )
    {
        if( pbFrontFacing[pEdges[edge].Face0] ^ pbFrontFacing[pEdges[edge].Face1] )
        {
            // Each edge is stored as a quad.  The vertex shader will expand
            // the quad to be one pixel wide.
            if( pbFrontFacing[pEdges[edge].Face1] )
            {
                pSilhouetteVerts->vPos = pVerts[pEdges[edge].V[0]];
                pSilhouetteVerts->vNextPos = pVerts[pEdges[edge].V[1]];
                ++pSilhouetteVerts;
                pSilhouetteVerts->vPos = pVerts[pEdges[edge].V[0]];
                pSilhouetteVerts->vNextPos = pVerts[pEdges[edge].V[1]];
                ++pSilhouetteVerts;
                pSilhouetteVerts->vPos = pVerts[pEdges[edge].V[1]];
                pSilhouetteVerts->vNextPos = pVerts[pEdges[edge].V[0]];
                ++pSilhouetteVerts;
                pSilhouetteVerts->vPos = pVerts[pEdges[edge].V[1]];
                pSilhouetteVerts->vNextPos = pVerts[pEdges[edge].V[0]];
                ++pSilhouetteVerts;

                dwNumSilhouetteVerts += 4;
            }
            else
            {
                pSilhouetteVerts->vPos = pVerts[pEdges[edge].V[1]];
                pSilhouetteVerts->vNextPos = pVerts[pEdges[edge].V[0]];
                ++pSilhouetteVerts;
                pSilhouetteVerts->vPos = pVerts[pEdges[edge].V[1]];
                pSilhouetteVerts->vNextPos = pVerts[pEdges[edge].V[0]];
                ++pSilhouetteVerts;
                pSilhouetteVerts->vPos = pVerts[pEdges[edge].V[0]];
                pSilhouetteVerts->vNextPos = pVerts[pEdges[edge].V[1]];
                ++pSilhouetteVerts;
                pSilhouetteVerts->vPos = pVerts[pEdges[edge].V[0]];
                pSilhouetteVerts->vNextPos = pVerts[pEdges[edge].V[1]];
                ++pSilhouetteVerts;

                dwNumSilhouetteVerts += 4;
            }
        }
    }

    pMeshData->m_pSilhouetteEdgeVB[m_dwCurFrame]->Unlock();

    // Save the current data.
    pMeshData->m_dwNumSilhouetteEdgeVerts = dwNumSilhouetteVerts;
}


//--------------------------------------------------------------------------------------
// Name: SetViewDir()
// Desc: Sets the view direction
//--------------------------------------------------------------------------------------
VOID CAAMesh::SetViewDir( const XMVECTOR vViewDir )
{
    m_vViewDir = vViewDir;

    if( m_pMesh->m_pFrames )
        ComputeSilhouetteFrame( m_pMesh->m_pFrames );
}


//--------------------------------------------------------------------------------------
// Name: RenderAAMesh()
// Desc: Render the AA Edges
//--------------------------------------------------------------------------------------
VOID CAAMesh::RenderAAMesh( ATG::MESH_DATA* pMesh, DWORD index )
{
    // Draw the AA Edges
    AA_MESH_DATA* pMeshData = &m_pAAMeshData[index];

    if( pMeshData->m_dwNumSilhouetteEdgeVerts > 0 )
    {
        ATG::g_pd3dDevice->SetStreamSource( 0, pMeshData->m_pSilhouetteEdgeVB[m_dwCurFrame], 0, sizeof( AAEdgeVert ) );
        ATG::g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, pMeshData->m_dwNumSilhouetteEdgeVerts / 4 );
    }

    if( pMeshData->m_dwNumInteriorEdgeVerts > 0 )
    {
        ATG::g_pd3dDevice->SetStreamSource( 0, pMeshData->m_pInteriorEdgeVB, 0, sizeof( AAEdgeVert ) );
        ATG::g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, pMeshData->m_dwNumInteriorEdgeVerts / 4 );
    }

    ATG::g_pd3dDevice->SetIndices( NULL );
}

