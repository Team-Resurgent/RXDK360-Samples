//--------------------------------------------------------------------------------------
// CSpotLight.cpp
//
// Spotlight Class for Light Shaft rendering
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// ATI 3D Application Research Group. 
// Copyright (C) ATI Research, Inc. All rights reserved. 
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgUtil.h>
#include "CSpotlight.h"
#include <AtgApp.h>

// Global scene transforms
extern XMMATRIX                     g_matWorldViewProj;
extern XMMATRIX                     g_matWorldView;


//--------------------------------------------------------------------------------------
// Types
//--------------------------------------------------------------------------------------
struct FRUSTUM_VERTEX
{
    XMFLOAT3 m_vPos;
    XMFLOAT3 m_vNorm;
    XMFLOAT2 m_vTex;
};

struct WIRE_FRUSTUM_VERTEX
{
    XMFLOAT3 m_vPos;
    D3DCOLOR m_dwColor;
};


static LPDIRECT3DVERTEXDECLARATION9 g_pFrustumVertexDecl = NULL;
static LPDIRECT3DVERTEXDECLARATION9 g_pWireFrustumVertexDecl = NULL;
static LPDIRECT3DVERTEXSHADER9      g_pSpotlightFrustumFrontVS = NULL;
static LPDIRECT3DPIXELSHADER9       g_pSpotlightFrustumFrontPS = NULL;
static LPDIRECT3DVERTEXSHADER9      g_pSpotlightFrustumVS = NULL;
static LPDIRECT3DPIXELSHADER9       g_pSpotlightFrustumPS = NULL;
static LPDIRECT3DVERTEXSHADER9      g_pSpotlightWireFrustumVS = NULL;




//--------------------------------------------------------------------------------------
// Name: CSpotlight()
// Desc: 
//--------------------------------------------------------------------------------------
CSpotlight::CSpotlight()
{
    // Light source geometry
    m_pFrustumVB = NULL;

    // Wireframe frustum geometry
    m_pWireFrustumIB = NULL;
    m_pWireFrustumVB = NULL;

    // Texture and surface pointers
    m_pCookie = NULL;

    // BOOL to tell if we're using ground plane instead of far frustum plane
    m_bFarPlaneIsGroundPlane = TRUE;
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CSpotlight::Initialize()
{
    HRESULT hr;

    // Create vertices
    m_dwNumFrustumVertices = 18;

    if( FAILED( hr = ATG::g_pd3dDevice->CreateVertexBuffer( m_dwNumFrustumVertices * sizeof( FRUSTUM_VERTEX ),
                                                            D3DUSAGE_WRITEONLY, 0L,
                                                            D3DPOOL_DEFAULT, &m_pFrustumVB, NULL ) ) )
        return hr;

    // Create indices for wireframe frustum
    m_dwNumWireFrustumIndices = 24;

    if( FAILED( hr = ATG::g_pd3dDevice->CreateIndexBuffer( m_dwNumWireFrustumIndices * sizeof( WORD ),
                                                           D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
                                                           D3DPOOL_DEFAULT, &m_pWireFrustumIB, NULL ) ) )
        return hr;

    WORD SrcIndices[24];
    SrcIndices[0] = 0;   SrcIndices[1] = 1;
    SrcIndices[2] = 1;   SrcIndices[3] = 2;
    SrcIndices[4] = 2;   SrcIndices[5] = 3;
    SrcIndices[6] = 3;   SrcIndices[7] = 0;
    SrcIndices[8] = 4;   SrcIndices[9] = 5;
    SrcIndices[10] = 5;   SrcIndices[11] = 6;
    SrcIndices[12] = 6;   SrcIndices[13] = 7;
    SrcIndices[14] = 7;   SrcIndices[15] = 4;
    SrcIndices[16] = 0;   SrcIndices[17] = 4;
    SrcIndices[18] = 1;   SrcIndices[19] = 5;
    SrcIndices[20] = 2;   SrcIndices[21] = 6;
    SrcIndices[22] = 3;   SrcIndices[23] = 7;

    WORD* pDstIndices;
    m_pWireFrustumIB->Lock( 0, 0, ( VOID** )&pDstIndices, 0 );
    memcpy( pDstIndices, SrcIndices, sizeof( SrcIndices ) );
    m_pWireFrustumIB->Unlock();

    // Create vertices
    m_dwNumWireFrustumVertices = 8;

    if( FAILED( hr = ATG::g_pd3dDevice->CreateVertexBuffer( m_dwNumWireFrustumVertices * sizeof( WIRE_FRUSTUM_VERTEX ),
                                                            D3DUSAGE_WRITEONLY, 0L,
                                                            D3DPOOL_DEFAULT, &m_pWireFrustumVB, NULL ) ) )
        return hr;

    RegenerateWireFrustum();

    // Create vertex declarations
    {
        static const D3DVERTEXELEMENT9 decl[] =
        {
            { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
            { 0, 12, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL,   0 },
            { 0, 24, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };
        ATG::g_pd3dDevice->CreateVertexDeclaration( decl, &g_pFrustumVertexDecl );
    }
    {
        static const D3DVERTEXELEMENT9 decl[] =
        {
            { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
            { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR,    0 },
            D3DDECL_END()
        };
        ATG::g_pd3dDevice->CreateVertexDeclaration( decl, &g_pWireFrustumVertexDecl );
    }

    // Create the shaders
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\SpotlightFrustumVS.xvu",
                                       &g_pSpotlightFrustumVS ) ) ) return E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\SpotlightFrustumPS.xpu",
                                      &g_pSpotlightFrustumPS ) ) ) return E_FAIL;
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\SpotlightFrustumFrontVS.xvu",
                                       &g_pSpotlightFrustumFrontVS ) ) ) return E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\SpotlightFrustumFrontPS.xpu",
                                      &g_pSpotlightFrustumFrontPS ) ) ) return E_FAIL;
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\SpotlightWireFrustumVS.xvu",
                                       &g_pSpotlightWireFrustumVS ) ) ) return E_FAIL;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GetViewSpaceBounds()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CSpotlight::GetViewSpaceBounds( XMVECTOR& vMinBounds, XMVECTOR& vMaxBounds )
{
    // Init mins and maxes
    vMinBounds = XMVectorSet( FLT_MAX, FLT_MAX, FLT_MAX, 0.0f );
    vMaxBounds = XMVectorSet( -FLT_MAX, -FLT_MAX, -FLT_MAX, 0.0f );

    // Transformation from light space to view space
    XMVECTOR vDeterminant;
    XMMATRIX matFrustum, matFrustumWorldView;
    matFrustum = XMMatrixInverse( &vDeterminant, m_matView );
    matFrustumWorldView = XMMatrixMultiply( matFrustum, g_matWorldView );

    // For each of the eight verts of the light frustum...
    for( DWORD i = 0; i < 8; i++ )
    {
        // Transform to view space
        XMVECTOR v = XMVector3Transform( m_FrustumVerts[i], matFrustumWorldView );
        if( v.x < vMinBounds.x )   vMinBounds.x = v.x;
        if( v.y < vMinBounds.y )   vMinBounds.y = v.y;
        if( v.z < vMinBounds.z )   vMinBounds.z = v.z;
        if( v.x > vMaxBounds.x )   vMaxBounds.x = v.x;
        if( v.y > vMaxBounds.y )   vMaxBounds.y = v.y;
        if( v.z > vMaxBounds.z )   vMaxBounds.z = v.z;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GetWorldSpaceFrustumPlanes()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CSpotlight::GetWorldSpaceFrustumPlanes( XMVECTOR* pWorldSpaceFrustumPlanes )
{
    // Plane transformation from light space to world space
    XMMATRIX matLightToWorldInv;
    matLightToWorldInv = XMMatrixTranspose( m_matView );

    for( DWORD i = 0; i < 6; i++ )
    {
        pWorldSpaceFrustumPlanes[i] = XMPlaneTransform( m_FrustumPlanes[i], matLightToWorldInv );
    }

    // If we're using the ground plane instead of the far plane
    if( m_bFarPlaneIsGroundPlane )
    {
        // This plane is far/ground plane, hard-coded to y=-epsilon plane
        XMVECTOR v0 = XMVectorSet( 0.0f, -0.02f, 0.0f, 0.0f );
        XMVECTOR v1 = XMVectorSet( 1.0f, -0.02f, 1.0f, 0.0f );
        XMVECTOR v2 = XMVectorSet( 1.0f, -0.02f, 0.0f, 0.0f );
        pWorldSpaceFrustumPlanes[1] = XMPlaneFromPoints( v0, v1, v2 );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RegenerateWireFrustum()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CSpotlight::RegenerateWireFrustum()
{
    const FLOAT NEAR_DIST = 10.0f;
    const FLOAT FAR_DIST = 400.0f;
    const DWORD CLIP_VOLUME_COLOR = 0xfff68e07;

    // Compute Field of View as function of width at z==1
    m_fFOV = 2 * atanf( m_fWidth );
    m_fVerticalFOV = 2 * atanf( m_fHeight );

    // Size of near and far quads
    m_fNearWidth = NEAR_DIST * tanf( m_fFOV / 2.0f );
    m_fNearHeight = NEAR_DIST * tanf( m_fVerticalFOV / 2.0f );

    m_fVerticalFOV = 2 * atanf( m_fHeight );
    m_fFarWidth = FAR_DIST * tanf( m_fFOV / 2.0f );
    m_fFarHeight = FAR_DIST * tanf( m_fVerticalFOV / 2.0f );

    {
        // Near quad of wireframe frustum
        m_FrustumVerts[0] = XMVectorSet( -m_fNearWidth, m_fNearHeight, NEAR_DIST, 0.0f );
        m_FrustumVerts[1] = XMVectorSet( m_fNearWidth, m_fNearHeight, NEAR_DIST, 0.0f );
        m_FrustumVerts[2] = XMVectorSet( m_fNearWidth, -m_fNearHeight, NEAR_DIST, 0.0f );
        m_FrustumVerts[3] = XMVectorSet( -m_fNearWidth, -m_fNearHeight, NEAR_DIST, 0.0f );

        // Far quad of wireframe frustum 
        m_FrustumVerts[4] = XMVectorSet( -m_fFarWidth, m_fFarHeight, FAR_DIST, 0.0f );
        m_FrustumVerts[5] = XMVectorSet( m_fFarWidth, m_fFarHeight, FAR_DIST, 0.0f );
        m_FrustumVerts[6] = XMVectorSet( m_fFarWidth, -m_fFarHeight, FAR_DIST, 0.0f );
        m_FrustumVerts[7] = XMVectorSet( -m_fFarWidth, -m_fFarHeight, FAR_DIST, 0.0f );

        WIRE_FRUSTUM_VERTEX SrcVertices[8];
        SrcVertices[0].m_vPos = XMFLOAT3( -m_fNearWidth, m_fNearHeight, NEAR_DIST );
        SrcVertices[1].m_vPos = XMFLOAT3( m_fNearWidth, m_fNearHeight, NEAR_DIST );
        SrcVertices[2].m_vPos = XMFLOAT3( m_fNearWidth, -m_fNearHeight, NEAR_DIST );
        SrcVertices[3].m_vPos = XMFLOAT3( -m_fNearWidth, -m_fNearHeight, NEAR_DIST );
        SrcVertices[4].m_vPos = XMFLOAT3( -m_fFarWidth, m_fFarHeight, FAR_DIST );
        SrcVertices[5].m_vPos = XMFLOAT3( m_fFarWidth, m_fFarHeight, FAR_DIST );
        SrcVertices[6].m_vPos = XMFLOAT3( m_fFarWidth, -m_fFarHeight, FAR_DIST );
        SrcVertices[7].m_vPos = XMFLOAT3( -m_fFarWidth, -m_fFarHeight, FAR_DIST );
        SrcVertices[0].m_dwColor = CLIP_VOLUME_COLOR;
        SrcVertices[1].m_dwColor = CLIP_VOLUME_COLOR;
        SrcVertices[2].m_dwColor = CLIP_VOLUME_COLOR;
        SrcVertices[3].m_dwColor = CLIP_VOLUME_COLOR;
        SrcVertices[4].m_dwColor = CLIP_VOLUME_COLOR;
        SrcVertices[5].m_dwColor = CLIP_VOLUME_COLOR;
        SrcVertices[6].m_dwColor = CLIP_VOLUME_COLOR;
        SrcVertices[7].m_dwColor = CLIP_VOLUME_COLOR;

        WIRE_FRUSTUM_VERTEX* pDstVertices;
        m_pWireFrustumVB->Lock( 0, 0, ( VOID** )&pDstVertices, 0 );
        memcpy( pDstVertices, SrcVertices, sizeof( SrcVertices ) );
        m_pWireFrustumVB->Unlock();
    }

    {
        XMVECTOR vNormal;
        FRUSTUM_VERTEX SrcVertices[18];

        // Pyramid faces +z with tip at origin
        FLOAT fLightCapDistance = 10.0f;
        FLOAT fLightNearWidth = fLightCapDistance * tanf( m_fFOV / 2.0f );
        FLOAT fLightNearHeight = fLightCapDistance * tanf( m_fVerticalFOV / 2.0f );

        // +x face of frustum body
        SrcVertices[ 0].m_vPos = XMFLOAT3( 0.0f, 0.0f, 0.0f );
        SrcVertices[ 1].m_vPos = XMFLOAT3( fLightNearWidth, fLightNearHeight, fLightCapDistance );
        SrcVertices[ 2].m_vPos = XMFLOAT3( fLightNearWidth, -fLightNearHeight, fLightCapDistance );
        vNormal = XMVector3Cross( XMLoadFloat3( &SrcVertices[1].m_vPos ), XMLoadFloat3( &SrcVertices[2].m_vPos ) );
        vNormal = XMVector3Normalize( vNormal );
        XMStoreFloat3( &SrcVertices[ 0].m_vNorm, vNormal );
        XMStoreFloat3( &SrcVertices[ 1].m_vNorm, vNormal );
        XMStoreFloat3( &SrcVertices[ 2].m_vNorm, vNormal );

        // -y face of frustum body
        SrcVertices[ 3].m_vPos = XMFLOAT3( 0.0f, 0.0f, 0.0f );
        SrcVertices[ 4].m_vPos = XMFLOAT3( fLightNearWidth, -fLightNearHeight, fLightCapDistance );
        SrcVertices[ 5].m_vPos = XMFLOAT3( -fLightNearWidth, -fLightNearHeight, fLightCapDistance );
        vNormal = XMVector3Cross( XMLoadFloat3( &SrcVertices[4].m_vPos ), XMLoadFloat3( &SrcVertices[5].m_vPos ) );
        vNormal = XMVector3Normalize( vNormal );
        XMStoreFloat3( &SrcVertices[ 3].m_vNorm, vNormal );
        XMStoreFloat3( &SrcVertices[ 4].m_vNorm, vNormal );
        XMStoreFloat3( &SrcVertices[ 5].m_vNorm, vNormal );

        // -x face of frustum body
        SrcVertices[ 6].m_vPos = XMFLOAT3( 0.0f, 0.0f, 0.0f );
        SrcVertices[ 7].m_vPos = XMFLOAT3( -fLightNearWidth, -fLightNearHeight, fLightCapDistance );
        SrcVertices[ 8].m_vPos = XMFLOAT3( -fLightNearWidth, fLightNearHeight, fLightCapDistance );
        vNormal = XMVector3Cross( XMLoadFloat3( &SrcVertices[8].m_vPos ), XMLoadFloat3( &SrcVertices[7].m_vPos ) );
        vNormal = XMVector3Normalize( vNormal );
        XMStoreFloat3( &SrcVertices[ 6].m_vNorm, vNormal );
        XMStoreFloat3( &SrcVertices[ 7].m_vNorm, vNormal );
        XMStoreFloat3( &SrcVertices[ 8].m_vNorm, vNormal );

        // -y face of frustum body
        SrcVertices[ 9].m_vPos = XMFLOAT3( 0.0f, 0.0f, 0.0f );
        SrcVertices[10].m_vPos = XMFLOAT3( -fLightNearWidth, fLightNearHeight, fLightCapDistance );
        SrcVertices[11].m_vPos = XMFLOAT3( fLightNearWidth, fLightNearHeight, fLightCapDistance );
        vNormal = XMVector3Cross( XMLoadFloat3( &SrcVertices[10].m_vPos ), XMLoadFloat3( &SrcVertices[11].m_vPos ) );
        vNormal = XMVector3Normalize( vNormal );
        XMStoreFloat3( &SrcVertices[ 9].m_vNorm, vNormal );
        XMStoreFloat3( &SrcVertices[10].m_vNorm, vNormal );
        XMStoreFloat3( &SrcVertices[11].m_vNorm, vNormal );

        // Cookie capping the light frustum
        SrcVertices[12].m_vPos = XMFLOAT3( -fLightNearWidth, -fLightNearHeight, fLightCapDistance );
        SrcVertices[12].m_vTex = XMFLOAT2( 0.0f, 1.0f );
        SrcVertices[13].m_vPos = XMFLOAT3( fLightNearWidth, fLightNearHeight, fLightCapDistance );
        SrcVertices[13].m_vTex = XMFLOAT2( 1.0f, 0.0f );
        SrcVertices[14].m_vPos = XMFLOAT3( -fLightNearWidth, fLightNearHeight, fLightCapDistance );
        SrcVertices[14].m_vTex = XMFLOAT2( 0.0f, 0.0f );
        vNormal = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
        XMStoreFloat3( &SrcVertices[12].m_vNorm, vNormal );
        XMStoreFloat3( &SrcVertices[13].m_vNorm, vNormal );
        XMStoreFloat3( &SrcVertices[14].m_vNorm, vNormal );

        SrcVertices[15].m_vPos = XMFLOAT3( -fLightNearWidth, -fLightNearHeight, fLightCapDistance );
        SrcVertices[15].m_vTex = XMFLOAT2( 0.0f, 1.0f );
        SrcVertices[16].m_vPos = XMFLOAT3( fLightNearWidth, -fLightNearHeight, fLightCapDistance );
        SrcVertices[16].m_vTex = XMFLOAT2( 1.0f, 1.0f );
        SrcVertices[17].m_vPos = XMFLOAT3( fLightNearWidth, fLightNearHeight, fLightCapDistance );
        SrcVertices[17].m_vTex = XMFLOAT2( 1.0f, 0.0f );
        vNormal = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
        XMStoreFloat3( &SrcVertices[15].m_vNorm, vNormal );
        XMStoreFloat3( &SrcVertices[16].m_vNorm, vNormal );
        XMStoreFloat3( &SrcVertices[17].m_vNorm, vNormal );

        // Regen the little light-source geometry here...
        FRUSTUM_VERTEX* pDstVertices;
        m_pFrustumVB->Lock( 0, 0, ( VOID** )&pDstVertices, 0 );
        memcpy( pDstVertices, SrcVertices, sizeof( SrcVertices ) );
        m_pFrustumVB->Unlock();
    }

    // Update light-space planes defining the frustum
    m_FrustumPlanes[0] = XMPlaneFromPoints( m_FrustumVerts[2], m_FrustumVerts[1], m_FrustumVerts[3] );
    m_FrustumPlanes[1] = XMPlaneFromPoints( m_FrustumVerts[7], m_FrustumVerts[4], m_FrustumVerts[6] );
    m_FrustumPlanes[2] = XMPlaneFromPoints( m_FrustumVerts[0], m_FrustumVerts[1], m_FrustumVerts[4] );
    m_FrustumPlanes[3] = XMPlaneFromPoints( m_FrustumVerts[3], m_FrustumVerts[0], m_FrustumVerts[7] );
    m_FrustumPlanes[4] = XMPlaneFromPoints( m_FrustumVerts[3], m_FrustumVerts[6], m_FrustumVerts[2] );
    m_FrustumPlanes[5] = XMPlaneFromPoints( m_FrustumVerts[1], m_FrustumVerts[2], m_FrustumVerts[5] );

    return S_OK;

}


//--------------------------------------------------------------------------------------
// Name: SetView()
// Desc: Set Parameters which define the view
//--------------------------------------------------------------------------------------
HRESULT CSpotlight::SetView( XMVECTOR vEyePt, XMVECTOR vLookAtPt, XMVECTOR vUpVec,
                             FLOAT fWidth, FLOAT fHeight, FLOAT fZNear, FLOAT fZFar )
{
    // Stash inputs away in private members
    m_vEyePt = vEyePt;
    m_vLookAtPt = vLookAtPt;
    m_vUpVec = vUpVec;

    // Compute Field of View as function of width at z==1
    m_fFOV = 2 * atanf( fHeight );
    FLOAT fAspectRatio = fWidth / fHeight;

    // Construct View and Projection matrices
    m_matView = XMMatrixLookAtLH( vEyePt, vLookAtPt, vUpVec );
    m_matProjection = XMMatrixPerspectiveFovLH( m_fFOV, fAspectRatio, fZNear, fZFar );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetWidth()
// Desc: Set the spotlight width
//--------------------------------------------------------------------------------------
HRESULT CSpotlight::SetWidth( FLOAT fWidth )
{
    m_fWidth = fWidth;
    RegenerateWireFrustum();
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetHeight()
// Desc: Set the spotlight height
//--------------------------------------------------------------------------------------
HRESULT CSpotlight::SetHeight( FLOAT fHeight )
{
    m_fHeight = fHeight;
    RegenerateWireFrustum();
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetSceneLightPos()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CSpotlight::SetSceneLightPos( XMVECTOR vSceneLightPos )
{
    m_vSceneLightPos = vSceneLightPos;
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetCookie()
// Desc: Set the cookie
//--------------------------------------------------------------------------------------
HRESULT CSpotlight::SetCookie( LPDIRECT3DTEXTURE9 pCookie )
{
    m_pCookie = pCookie;
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetGroundPlaneClip()
// Desc: Decide whether or not the far plane gets replaced by a ground plane
//--------------------------------------------------------------------------------------
HRESULT CSpotlight::SetGroundPlaneClip( BOOL bFarPlaneIsGroundPlane )
{
    m_bFarPlaneIsGroundPlane = bFarPlaneIsGroundPlane;
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Draw()
// Desc: Draw the light
//--------------------------------------------------------------------------------------
HRESULT CSpotlight::Draw()
{
    // Set up proper frustum transformations
    XMVECTOR vDeterminant;
    XMMATRIX matFrustum, matFrustumWorldViewProj;
    matFrustum = XMMatrixInverse( &vDeterminant, m_matView );
    matFrustumWorldViewProj = XMMatrixMultiply( matFrustum, g_matWorldViewProj );
    matFrustumWorldViewProj = XMMatrixTranspose( matFrustumWorldViewProj );
    ATG::g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matFrustumWorldViewProj, 4 );

    // Transform light position into eye space and pass to effect
    XMVECTOR vEyeL = XMVector3Transform( m_vSceneLightPos, g_matWorldView );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&vEyeL, 1 );

    // Set the cookie texture
    const DWORD tBaseTexture = 0;
    ATG::g_pd3dDevice->SetTexture( tBaseTexture, m_pCookie );
    ATG::g_pd3dDevice->SetSamplerState( tBaseTexture, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tBaseTexture, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tBaseTexture, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tBaseTexture, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    ATG::g_pd3dDevice->SetSamplerState( tBaseTexture, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    ATG::g_pd3dDevice->SetSamplerState( tBaseTexture, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP );

    ATG::g_pd3dDevice->SetVertexDeclaration( g_pFrustumVertexDecl );
    ATG::g_pd3dDevice->SetStreamSource( 0, m_pFrustumVB, 0, sizeof( FRUSTUM_VERTEX ) );

    // Draw the cookie capping the frustum
    ATG::g_pd3dDevice->SetVertexShader( g_pSpotlightFrustumFrontVS );
    ATG::g_pd3dDevice->SetPixelShader( g_pSpotlightFrustumFrontPS );
    ATG::g_pd3dDevice->DrawVertices( D3DPT_TRIANGLELIST, 12, 6 );

    // Draw the body of the frustum
    ATG::g_pd3dDevice->SetVertexShader( g_pSpotlightFrustumVS );
    ATG::g_pd3dDevice->SetPixelShader( g_pSpotlightFrustumPS );
    ATG::g_pd3dDevice->DrawVertices( D3DPT_TRIANGLELIST, 0, 12 );

    // Unset the VB from the device
    ATG::g_pd3dDevice->SetVertexDeclaration( NULL );
    ATG::g_pd3dDevice->SetStreamSource( 0, NULL, 0, 0 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DrawClippingFrustum()
// Desc: Draw the frustum
//--------------------------------------------------------------------------------------
HRESULT CSpotlight::DrawClippingFrustum()
{
    // Set up proper frustum transformations
    XMVECTOR vDeterminant;
    XMMATRIX matFrustum, matFrustumWorldViewProj;
    matFrustum = XMMatrixInverse( &vDeterminant, m_matView );
    matFrustumWorldViewProj = XMMatrixMultiply( matFrustum, g_matWorldViewProj );
    matFrustumWorldViewProj = XMMatrixTranspose( matFrustumWorldViewProj );

    ATG::g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matFrustumWorldViewProj, 4 );

    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    // Draw the wireframe frustum
    ATG::g_pd3dDevice->SetVertexShader( g_pSpotlightWireFrustumVS );
    ATG::g_pd3dDevice->SetPixelShader( g_pSpotlightFrustumPS );

    ATG::g_pd3dDevice->SetVertexDeclaration( g_pWireFrustumVertexDecl );
    ATG::g_pd3dDevice->SetStreamSource( 0, m_pWireFrustumVB, 0, sizeof( WIRE_FRUSTUM_VERTEX ) );
    ATG::g_pd3dDevice->SetIndices( m_pWireFrustumIB );
    ATG::g_pd3dDevice->DrawIndexedVertices( D3DPT_LINELIST, 0, 0, 24 );

    // Restore z writing renderstate
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );

    // Unset the VB from the device
    ATG::g_pd3dDevice->SetStreamSource( 0, NULL, 0, 0 );

    return S_OK;
}
