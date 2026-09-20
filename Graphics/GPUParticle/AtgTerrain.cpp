//-----------------------------------------------------------------------------
// AtgTerrain.cpp
//
// Implements the AtgTerrain object, which provides an app-customizable terrain
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <AtgUtil.h>
#include "AtgTerrain.h"


struct ATGTERRAIN_VERTEX
{
    XMFLOAT3 p;
    XMFLOAT3 n;
    FLOAT tu, tv;
};


//-----------------------------------------------------------------------------
// Name: AtgTerrain()
// Desc: Zeros out our members
//-----------------------------------------------------------------------------
AtgTerrain::AtgTerrain()
{
    ZeroMemory( this, sizeof( AtgTerrain ) );
}


//-----------------------------------------------------------------------------
// Name: ~AtgTerrain()
// Desc: Releases resource held/allocated
//-----------------------------------------------------------------------------
AtgTerrain::~AtgTerrain()
{
    Terminate();
}


//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: One-time initialization to set the device
//-----------------------------------------------------------------------------
HRESULT AtgTerrain::Initialize( LPDIRECT3DDEVICE9 pd3dDevice )
{
    m_pd3dDevice = pd3dDevice;
    m_pd3dDevice->AddRef();

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Terminate()
// Desc: Releases resource held/allocated
//-----------------------------------------------------------------------------
HRESULT AtgTerrain::Terminate()
{
    if( m_pd3dDevice )                 m_pd3dDevice->Release();
    if( m_pTexture )                   m_pTexture->Release();
    if( m_pTerrainVB )                 m_pTerrainVB->Release();
    if( m_pTerrainIB )                 m_pTerrainIB->Release();
    if( m_pTerrainVertexDeclaration )  m_pTerrainVertexDeclaration->Release();
    if( m_pTerrainVertexShader )       m_pTerrainVertexShader->Release();
    if( m_pTerrainPixelShader )        m_pTerrainPixelShader->Release();
    if( m_pTerrainHeightVertexShader ) m_pTerrainHeightVertexShader->Release();
    if( m_pTerrainHeightPixelShader )  m_pTerrainHeightPixelShader->Release();
    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: Generate()
// Desc: Sets up the terrain object's state based off the parameters.  Can set
//       # of slices in both X and Z, near X/Z corner, far X/Z corner, texture
//       height field function, and repetitions of the texture
//-----------------------------------------------------------------------------
HRESULT AtgTerrain::Generate( DWORD dwNumXSlices, DWORD dwNumZSlices,
                              XMFLOAT2 vXZMin, XMFLOAT2 vXZMax,
                              LPDIRECT3DTEXTURE9 pTexture,
                              FNHEIGHTFUNCTION pfnHeight,
                              FLOAT fXRepeat, FLOAT fZRepeat )
{
    HRESULT hr;

    // Free up old resources
    if( m_pTerrainVB )
    {
        m_pTerrainVB->Release();
        m_pTerrainVB = NULL;
    }
    if( m_pTerrainIB )
    {
        m_pTerrainIB->Release();
        m_pTerrainIB = NULL;
    }
    if( m_pTexture )
    {
        m_pTexture->Release();
        m_pTexture = NULL;
    }
    if( m_pTerrainVertexDeclaration )
    {
        m_pTerrainVertexDeclaration->Release();
        m_pTerrainVertexDeclaration = NULL;
    }
    if( m_pTerrainVertexShader )
    {
        m_pTerrainVertexShader->Release();
        m_pTerrainVertexShader = NULL;
    }
    if( m_pTerrainPixelShader )
    {
        m_pTerrainPixelShader->Release();
        m_pTerrainPixelShader = NULL;
    }
    if( m_pTerrainHeightVertexShader )
    {
        m_pTerrainHeightVertexShader->Release();
        m_pTerrainHeightVertexShader = NULL;
    }
    if( m_pTerrainHeightPixelShader )
    {
        m_pTerrainHeightPixelShader->Release();
        m_pTerrainHeightPixelShader = NULL;
    }

    // Calculate how many vertices & indices we need
    DWORD dwNumVertices = ( dwNumXSlices + 1 ) * ( dwNumZSlices + 1 );
    m_dwNumIndices = 4 * dwNumXSlices * dwNumZSlices;

    // Create vertex buffer for terrain
    hr = m_pd3dDevice->CreateVertexBuffer( dwNumVertices * sizeof( ATGTERRAIN_VERTEX ),
                                           0, 0, D3DPOOL_DEFAULT, &m_pTerrainVB, NULL );
    if( FAILED( hr ) )
        return hr;

    // Create index buffer for terrain
    hr = m_pd3dDevice->CreateIndexBuffer( m_dwNumIndices * sizeof( WORD ), 0, D3DFMT_INDEX16,
                                          D3DPOOL_DEFAULT, &m_pTerrainIB, NULL );
    if( FAILED( hr ) )
        return hr;

    // Now that our allocations have succeeded, we can get to business
    m_vXZMin = vXZMin;
    m_vXZMax = vXZMax;
    m_pfnHeight = pfnHeight;
    m_fXTextureRepeat = fXRepeat;
    m_fZTextureRepeat = fZRepeat;
    m_pTexture = pTexture;
    m_pTexture->AddRef();

    // Calculate size of each grid square
    FLOAT fXUnit = ( vXZMax.x - vXZMin.x ) / dwNumXSlices;
    FLOAT fZUnit = ( vXZMax.y - vXZMin.y ) / dwNumZSlices;

    XMVECTOR vPos = XMVectorZero();
    XMVECTOR vNormal = XMVectorZero();

    // Fill vertex buffer
    ATGTERRAIN_VERTEX* pVertices;
    m_pTerrainVB->Lock( 0, 0, ( VOID** )&pVertices, NULL );

    for( DWORD z = 0; z <= dwNumZSlices; z++ )
    {
        // Calculate Z of top of terrain grid
        vPos.z = m_vXZMin.y + z * fZUnit;

        for( DWORD x = 0; x <= dwNumXSlices; x++ )
        {
            // Calculate X of left of terrain grid
            vPos.x = m_vXZMin.x + x * fXUnit;

            if( m_pfnHeight )
            {
                FLOAT fDX0, fDX1;
                FLOAT fDZ0, fDZ1;

                // Get height of point
                vPos.y = m_pfnHeight( vPos.x, vPos.z );

                if( z )
                    fDZ0 = m_pfnHeight( vPos.x, m_vXZMin.y + ( z - 1 ) * fZUnit );
                else
                    fDZ0 = vPos.y;
                fDZ1 = m_pfnHeight( vPos.x, m_vXZMin.y + ( z + 1 ) * fZUnit );

                if( x )
                    fDX0 = m_pfnHeight( m_vXZMin.x + ( x - 1 ) * fXUnit, vPos.z );
                else
                    fDX0 = vPos.y;
                fDX1 = m_pfnHeight( m_vXZMin.x + ( x + 1 ) * fXUnit, vPos.z );

                XMVECTOR fVecX = XMVectorSet( fXUnit * 2.0f, fDX1 - fDX0, 0.0f, 0.0f );
                XMVECTOR fVecZ = XMVectorSet( 0.f, fDZ1 - fDZ0, fZUnit * 2.0f, 0.0f );
                vNormal = XMVector3Normalize( XMVector3Cross( fVecZ, fVecX ) );
            }

            // Set up position and texture coordinates
            XMStoreFloat3( &pVertices->p, vPos );
            XMStoreFloat3( &pVertices->n, vNormal );
            pVertices->tu = m_fXTextureRepeat * x / dwNumXSlices;
            pVertices->tv = m_fZTextureRepeat * z / dwNumZSlices;

            pVertices++;
        }
    }
    m_pTerrainVB->Unlock();

    // Fill index buffer
    WORD* pIndices;
    m_pTerrainIB->Lock( 0, 0, ( VOID** )&pIndices, 0 );
    for( DWORD z = 0; z < dwNumZSlices; z++ )
    {
        for( DWORD x = 0; x < dwNumXSlices; x++ )
        {
            *pIndices++ = ( WORD )( ( z + 1 ) * ( dwNumXSlices + 1 ) + ( x + 0 ) );
            *pIndices++ = ( WORD )( ( z + 0 ) * ( dwNumXSlices + 1 ) + ( x + 0 ) );
            *pIndices++ = ( WORD )( ( z + 0 ) * ( dwNumXSlices + 1 ) + ( x + 1 ) );
            *pIndices++ = ( WORD )( ( z + 1 ) * ( dwNumXSlices + 1 ) + ( x + 1 ) );
        }
    }
    m_pTerrainIB->Unlock();

    // Create vertex shader for the terrain
    static const D3DVERTEXELEMENT9 declTerrain[] =
    {
        // First stream is first mesh
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL,   0 },
        { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    if( FAILED( hr = m_pd3dDevice->CreateVertexDeclaration( declTerrain, &m_pTerrainVertexDeclaration ) ) )
        return hr;

    VOID* pCode = NULL;

    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\Terrain.xvu", &pCode ) ) )
        return hr;
    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pTerrainVertexShader ) ) )
    {
        ATG::UnloadFile( pCode );
        return hr;
    }
    ATG::UnloadFile( pCode );

    // Create pixel shader
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\Terrain.xpu", &pCode ) ) )
        return hr;
    if( FAILED( hr = m_pd3dDevice->CreatePixelShader( ( DWORD* )pCode, &m_pTerrainPixelShader ) ) )
    {
        ATG::UnloadFile( pCode );
        return hr;
    }
    ATG::UnloadFile( pCode );

    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\TerrainHeight.xvu", &pCode ) ) )
        return hr;
    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pTerrainHeightVertexShader ) ) )
    {
        ATG::UnloadFile( pCode );
        return hr;
    }
    ATG::UnloadFile( pCode );

    // Create pixel shader
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\TerrainHeight.xpu", &pCode ) ) )
        return hr;
    if( FAILED( hr = m_pd3dDevice->CreatePixelShader( ( DWORD* )pCode, &m_pTerrainHeightPixelShader ) ) )
    {
        ATG::UnloadFile( pCode );
        return hr;
    }
    ATG::UnloadFile( pCode );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the terrain
//-----------------------------------------------------------------------------
HRESULT AtgTerrain::Render()
{
    // Set up to render
    m_pd3dDevice->SetVertexDeclaration( m_pTerrainVertexDeclaration );
    m_pd3dDevice->SetStreamSource( 0, m_pTerrainVB, 0, sizeof( ATGTERRAIN_VERTEX ) );
    m_pd3dDevice->SetVertexShader( m_pTerrainVertexShader );
    m_pd3dDevice->SetPixelShader( m_pTerrainPixelShader );
    m_pd3dDevice->SetIndices( m_pTerrainIB );

    m_pd3dDevice->SetTexture( 0, m_pTexture );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );

    // Render
    m_pd3dDevice->DrawIndexedPrimitive( D3DPT_QUADLIST, 0, 0, m_dwNumIndices,
                                        0, m_dwNumIndices / 4 );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: RenderPlaneMap()
// Desc: Renders the terrain height
//-----------------------------------------------------------------------------
HRESULT AtgTerrain::RenderPlaneMap()
{
    // Set up to render
    m_pd3dDevice->SetVertexDeclaration( m_pTerrainVertexDeclaration );
    m_pd3dDevice->SetStreamSource( 0, m_pTerrainVB, 0, sizeof( ATGTERRAIN_VERTEX ) );
    m_pd3dDevice->SetVertexShader( m_pTerrainHeightVertexShader );
    m_pd3dDevice->SetPixelShader( m_pTerrainHeightPixelShader );
    m_pd3dDevice->SetIndices( m_pTerrainIB );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    // Render
    m_pd3dDevice->DrawIndexedPrimitive( D3DPT_QUADLIST, 0, 0, m_dwNumIndices,
                                        0, m_dwNumIndices / 4 );

    return S_OK;
}


