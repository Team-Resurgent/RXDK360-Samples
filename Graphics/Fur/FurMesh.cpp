//--------------------------------------------------------------------------------------
// FurMesh.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// Copyright (C) Tomohide Kano. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include "FurMesh.h"
#include <AtgApp.h>


struct FURVERTEX
{
    XMFLOAT3 pos;
    XMFLOAT2 tex1;
    XMFLOAT2 tex2;
    XMFLOAT2 scale;
    XMFLOAT2 radius;
    XMFLOAT3 ds;
    XMFLOAT3 dt;
    XMFLOAT3 normal;
};


// Partial derivatives of bivariate function
const FLOAT DELTA = 0.01f;
inline XMVECTOR deriv_u( MESH_FUNC fn, FLOAT u, FLOAT v )
{
    return ( fn( u + DELTA, v ) - fn( u - DELTA, v ) ) / ( 2 * DELTA );
}
inline XMVECTOR deriv_v( MESH_FUNC fn, FLOAT u, FLOAT v )
{
    return ( fn( u, v + DELTA ) - fn( u, v - DELTA ) ) / ( 2 * DELTA );
}
inline XMVECTOR deriv_uu( MESH_FUNC fn, FLOAT u, FLOAT v )
{
    return ( deriv_u( fn, u + DELTA, v ) - deriv_u( fn, u - DELTA, v ) ) / ( 2 * DELTA );
}
inline XMVECTOR deriv_vv( MESH_FUNC fn, FLOAT u, FLOAT v )
{
    return ( deriv_v( fn, u, v + DELTA ) - deriv_v( fn, u, v - DELTA ) ) / ( 2 * DELTA );
}
inline XMVECTOR deriv_uv( MESH_FUNC fn, FLOAT u, FLOAT v )
{
    return ( deriv_u( fn, u, v + DELTA ) - deriv_u( fn, u, v - DELTA ) ) / ( 2 * DELTA );
}


//--------------------------------------------------------------------------------------
// Name: FurMesh()
// Desc: 
//--------------------------------------------------------------------------------------
FurMesh::FurMesh()
{
    m_strName = L"";
    m_dwNumVertices = 0;
    m_dwNumIndices = 0;
    m_pVertexDecl = NULL;
    m_pVertexBuffer = NULL;
    m_pIndexBuffer = NULL;
}


//--------------------------------------------------------------------------------------
// Name: ~FurMesh()
// Desc: 
//--------------------------------------------------------------------------------------
FurMesh::~FurMesh()
{
    if( m_pVertexDecl )
        m_pVertexDecl->Release();
    if( m_pVertexBuffer )
        m_pVertexBuffer->Release();
    if( m_pIndexBuffer )
        m_pIndexBuffer->Release();
}


//--------------------------------------------------------------------------------------
// Name: Init()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT FurMesh::Init( const MESH_PARAMETERS& param )
{
    m_strName = param.strName;

    DWORD dwNumTilesU = param.dwNumTilesU;
    DWORD dwNumTilesV = param.dwNumTilesV;
    FLOAT fScaleU = param.fFurScaleU;
    FLOAT fScaleV = param.fFurScaleV;

    m_dwNumVertices = ( dwNumTilesU + 1 ) * ( dwNumTilesV + 1 );
    m_dwNumIndices = dwNumTilesU * dwNumTilesV * 4;

    // Create the vertex declaration which matches FURVERTEX
    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0 },
        { 0, 20, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 1 },
        { 0, 28, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 2 },
        { 0, 36, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 3 },
        { 0, 44, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 4 },
        { 0, 56, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 5 },
        { 0, 68, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL,   0 },
        D3DDECL_END()
    };

    ATG::g_pd3dDevice->CreateVertexDeclaration( decl, &m_pVertexDecl );

    // Create the vertex buffer
    ATG::g_pd3dDevice->CreateVertexBuffer( m_dwNumVertices * sizeof( FURVERTEX ),
                                           D3DUSAGE_WRITEONLY,
                                           0L, D3DPOOL_DEFAULT, &m_pVertexBuffer, NULL );
    FURVERTEX* pVertices;
    m_pVertexBuffer->Lock( 0, 0, ( VOID** )&pVertices, 0L );
    for( DWORD j = 0; j <= dwNumTilesV; j++ )
    {
        for( DWORD i = 0; i <= dwNumTilesU; i++ )
        {
            FLOAT u = ( FLOAT )( i ) / dwNumTilesU;
            FLOAT v = ( FLOAT )( j ) / dwNumTilesV;

            // Vertex position
            XMVECTOR pos = param.fnPosition( u, v );
            XMStoreFloat3( &pVertices->pos, pos );

            // Texture coord for normal map
            pVertices->tex1 = XMFLOAT2( u, v );

            // Texture coord for fur texture
            pVertices->tex2 = XMFLOAT2( fScaleU * u, fScaleV * v );

            // Partial derivatives with respect to (u, v)
            XMVECTOR Pu = deriv_u( param.fnPosition, u, v );
            XMVECTOR Pv = deriv_v( param.fnPosition, u, v );
            XMVECTOR Puu = deriv_uu( param.fnPosition, u, v );
            XMVECTOR Pvv = deriv_vv( param.fnPosition, u, v );

            // Basis vectors of local texture space
            XMVECTOR ds = XMVector3Normalize( Pu );
            XMVECTOR dt = XMVector3Normalize( Pv );
            XMVECTOR normal = XMVector3Normalize( XMVector3Cross( Pv, Pu ) );

            XMStoreFloat3( &pVertices->ds, ds );
            XMStoreFloat3( &pVertices->dt, dt );
            XMStoreFloat3( &pVertices->normal, normal );

            // Scale factor
            pVertices->scale.x = fScaleU / XMVector3Length( Pu ).x;
            pVertices->scale.y = fScaleV / XMVector3Length( Pv ).x;

            // Radii of curvature
            {
                // First and second fundamental forms
                // http://mathworld.wolfram.com/FundamentalForms.html
                FLOAT E = XMVector3LengthSq( Pu ).x;
                FLOAT G = XMVector3LengthSq( Pv ).x;
                FLOAT e = -XMVector3Dot( normal, Puu ).x;
                FLOAT g = -XMVector3Dot( normal, Pvv ).x;

                // Normal curvatures in the directions of u and v
                // http://mathworld.wolfram.com/NormalCurvature.html
                FLOAT fCurvatureU = max( 0.001f, -e / E );
                FLOAT fCurvatureV = max( 0.001f, -g / G );

                // Radii of curvature corresponding to the normal curvatures
                // http://mathworld.wolfram.com/RadiusofCurvature.html
                pVertices->radius.x = 1.0f / fCurvatureU;
                pVertices->radius.y = 1.0f / fCurvatureV;

                // To be accurate, principal curvatures should be taken into account...
                // http://mathworld.wolfram.com/PrincipalCurvatures.html
            }

            pVertices++;
        }
    }
    m_pVertexBuffer->Unlock();

    // Create the index buffer
    ATG::g_pd3dDevice->CreateIndexBuffer( m_dwNumIndices * sizeof( WORD ),
                                          D3DUSAGE_WRITEONLY,
                                          D3DFMT_INDEX16, D3DPOOL_DEFAULT,
                                          &m_pIndexBuffer, NULL );
    WORD* pIndices;
    m_pIndexBuffer->Lock( 0, 0, ( VOID** )&pIndices, 0L );
    {
        for( DWORD j = 0; j < dwNumTilesV; j++ )
        {
            for( DWORD i = 0; i < dwNumTilesU; i++ )
            {
                *pIndices++ = ( WORD )( ( j + 0 ) * ( dwNumTilesU + 1 ) + ( i + 1 ) );
                *pIndices++ = ( WORD )( ( j + 0 ) * ( dwNumTilesU + 1 ) + ( i + 0 ) );
                *pIndices++ = ( WORD )( ( j + 1 ) * ( dwNumTilesU + 1 ) + ( i + 0 ) );
                *pIndices++ = ( WORD )( ( j + 1 ) * ( dwNumTilesU + 1 ) + ( i + 1 ) );
            }
        }
    }
    m_pIndexBuffer->Unlock();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Draw()
// Desc: Renders the mesh
//--------------------------------------------------------------------------------------
VOID FurMesh::Draw()
{
    ATG::g_pd3dDevice->SetIndices( m_pIndexBuffer );
    ATG::g_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    ATG::g_pd3dDevice->SetStreamSource( 0, m_pVertexBuffer, 0, sizeof( FURVERTEX ) );
    ATG::g_pd3dDevice->DrawIndexedPrimitive( D3DPT_QUADLIST, 0, 0, m_dwNumIndices, 0, m_dwNumIndices / 4 );
}
