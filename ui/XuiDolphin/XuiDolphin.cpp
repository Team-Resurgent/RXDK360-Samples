//--------------------------------------------------------------------------------------
// XuiDolphin.cpp
//
// The sample demonstrate how to use XUI features with Dolphin sample.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <stdio.h>
#include <assert.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>

#include "DolphinUi.h"

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_1, L"Invoke XUI" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))

//--------------------------------------------------------------------------------------
// Globals variables and definitions
//--------------------------------------------------------------------------------------

const DWORD g_dwWaterColor = D3DCOLOR_ARGB( 0xff, 0x00, 0x40, 0x80 );
const FLOAT g_fWaterColor[] =
{
    0.0f, 0.25f, 0.5f, 1.0f
};


// mapping from dolphin style to texture name in the resource
// Note that the order of the names here must match the DOLPHIN_STYLE enum
// defined in ui.h
static LPCSTR g_szDolphinTextures[] =
{
    "DolphinTexture",       // DOLPHIN_STYLE_STANDARD
    "DolphinTextureTiger",  // DOLPHIN_STYLE_TIGER
    "DolphinTextureSpotted",// DOLPHIN_STYLE_SPOTTED
    "DolphinTextureBlue",   // DOLPHIN_STYLE_BLUE
};

static LPCWSTR g_szDolphinStyleDesc[] =
{
    L"Standard Dolphin",    // DOLPHIN_STYLE_STANDARD
    L"Tiger Dolphin",       // DOLPHIN_STYLE_TIGER
    L"Spotted Dolphin",     // DOLPHIN_STYLE_SPOTTED
    L"Blue Dolphin",        // DOLPHIN_STYLE_BLUE
};

//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application,
               public IDolphinUI
{
    // Timer
    ATG::Timer m_Timer;

    // Font for drawing text
    ATG::Font m_Font;
    ATG::Help m_Help;      // Display help
    ATG::PackedResource m_Resource;

    // Transform matrices
    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    // Dolphin object
    ATG::Mesh2 m_DolphinMesh1;
    ATG::Mesh2 m_DolphinMesh2;
    ATG::Mesh2 m_DolphinMesh3;

    BOOL m_bDrawHelp;

    // Current Dolphin Texture
    LPDIRECT3DTEXTURE9 m_pDolphinTexture;

    // Available dolphin textures
    LPDIRECT3DTEXTURE9      m_pDolphinTextures[DOLPHIN_STYLE_MAX];

    LPDIRECT3DVERTEXBUFFER9 m_pDolphinVB1;
    LPDIRECT3DVERTEXBUFFER9 m_pDolphinVB2;
    LPDIRECT3DVERTEXBUFFER9 m_pDolphinVB3;
    LPDIRECT3DINDEXBUFFER9 m_pDolphinIB;
    D3DPRIMITIVETYPE m_dwDolphinPrimType;
    DWORD m_dwDolphinVertexSize;
    DWORD m_dwNumDolphinVertices;
    DWORD m_dwNumDolphinPrimitives;
    LPDIRECT3DVERTEXDECLARATION9 m_pDolphinVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9 m_pDolphinVertexShader;

    // Seafloor object
    ATG::Mesh2 m_SeaFloorMesh;
    LPDIRECT3DTEXTURE9 m_pSeaFloorTexture;
    LPDIRECT3DVERTEXBUFFER9 m_pSeaFloorVB;
    LPDIRECT3DINDEXBUFFER9 m_pSeaFloorIB;
    D3DPRIMITIVETYPE m_dwSeaFloorPrimType;
    DWORD m_dwSeaFloorVertexSize;
    DWORD m_dwNumSeaFloorVertices;
    DWORD m_dwNumSeaFloorPrimitives;
    LPDIRECT3DVERTEXDECLARATION9 m_pSeaFloorVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9 m_pSeaFloorVertexShader;

    // Water caustics
    LPDIRECT3DTEXTURE9      m_pCausticTextures[32];
    LPDIRECT3DTEXTURE9 m_pCurrentCausticTexture;

    LPDIRECT3DPIXELSHADER9 m_pPixelShader;

    // UI related members
    DWORD m_dwRenderOptions;
    DOLPHIN_STYLE m_nDolphinStyle;
public:
    virtual HRESULT         Initialize();
    virtual HRESULT         Update();
    virtual HRESULT         Render();

    // IDolphinUI implementation
    DWORD                   GetRenderOptions();
    void                    SetRenderOptions( DWORD dwRenderOptions );
    virtual DOLPHIN_STYLE   GetDolphinStyle();
    virtual void            SetDolphinStyle( DOLPHIN_STYLE nDolphinStyle );
    virtual LPCWSTR         GetDolphinStyleDesc( DOLPHIN_STYLE nDolphinStyle );
    virtual IDirect3DDevice9* GetD3DDevice();
    virtual void            RenderDolphinStyle( DOLPHIN_STYLE nDolphinStyle, IDirect3DDevice9* pDevice );
    virtual HRESULT         RenderScene();

    HRESULT                 UpdatePreview();
    void                    SetConstants();
};

//--------------------------------------------------------------------------------------
// Global instance of the app
//--------------------------------------------------------------------------------------
Sample      g_atgApp;

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    ATG::GetVideoSettings( &g_atgApp.m_d3dpp.BackBufferWidth, &g_atgApp.m_d3dpp.BackBufferHeight );
    g_atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize our rendering options
    m_dwRenderOptions = DOLPHIN_RENDER_DOLPHIN | DOLPHIN_RENDER_FLOOR | DOLPHIN_RENDER_STATS;
    m_nDolphinStyle = DOLPHIN_STYLE_STANDARD;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        ATG::FatalError( "Couldn't create font" );

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        ATG::FatalError( "Couldn't create help" );
    m_bDrawHelp = FALSE;

    // Create the textures resource
    if( FAILED( m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
        ATG::FatalError( "Dolphin: Couldn't create Resource.xpr" );

    for( INT nDolphinStyle = 0; nDolphinStyle < DOLPHIN_STYLE_MAX; nDolphinStyle++ )
        m_pDolphinTextures[nDolphinStyle] =
            m_Resource.GetTexture( g_szDolphinTextures[nDolphinStyle] );

    // default to standard dolphin
    m_pDolphinTexture = m_pDolphinTextures[DOLPHIN_STYLE_STANDARD];

    m_pSeaFloorTexture = m_Resource.GetTexture( "SeafloorTexture" );

    for( DWORD t = 0; t < 32; t++ )
    {
        CHAR strTextureName[80];
        sprintf_s( strTextureName, "WaterCaustic%02ld", t );
        m_pCausticTextures[t] = m_Resource.GetTexture( strTextureName );
    }

    if( FAILED( m_DolphinMesh1.Create( "game:\\Media\\Meshes\\dolphin1.xbg" ) ) )
        ATG::FatalError( "Couldn't create Dolphin1.xbg" );

    if( FAILED( m_DolphinMesh2.Create( "game:\\Media\\Meshes\\dolphin2.xbg" ) ) )
        ATG::FatalError( "Couldn't create Dolphin2.xbg" );

    if( FAILED( m_DolphinMesh3.Create( "game:\\Media\\Meshes\\dolphin3.xbg" ) ) )
        ATG::FatalError( "Couldn't create Dolphin3.xbg" );

    if( FAILED( m_SeaFloorMesh.Create( "game:\\Media\\Meshes\\Seafloor.xbg" ) ) )
        ATG::FatalError( "Couldn't create Seafloor.xbg" );

    m_pDolphinVB1 = &m_DolphinMesh1.GetMesh()->m_VB;
    m_pDolphinVB2 = &m_DolphinMesh2.GetMesh()->m_VB;
    m_pDolphinVB3 = &m_DolphinMesh3.GetMesh()->m_VB;
    m_pDolphinIB = &m_DolphinMesh1.GetMesh()->m_IB;

    m_pSeaFloorVB = &m_SeaFloorMesh.GetMesh()->m_VB;
    m_pSeaFloorIB = &m_SeaFloorMesh.GetMesh()->m_IB;

    // Get the number of vertices and faces for the meshes
    m_dwDolphinPrimType = m_DolphinMesh1.GetMesh()->m_dwPrimType;
    m_dwNumDolphinVertices = m_DolphinMesh1.GetMesh()->m_pSubsets[0].dwVertexCount;
    m_dwNumDolphinPrimitives = m_DolphinMesh1.GetMesh()->m_pSubsets[0].dwPrimitiveCount;
    m_dwDolphinVertexSize = m_DolphinMesh1.GetMesh()->m_dwVertexSize;

    m_dwSeaFloorPrimType = m_SeaFloorMesh.GetMesh()->m_dwPrimType;
    m_dwNumSeaFloorVertices = m_SeaFloorMesh.GetMesh()->m_pSubsets[0].dwVertexCount;
    m_dwNumSeaFloorPrimitives = m_SeaFloorMesh.GetMesh()->m_pSubsets[0].dwPrimitiveCount;
    m_pSeaFloorVertexDeclaration = m_SeaFloorMesh.GetMesh()->m_pVertexDecl;
    m_dwSeaFloorVertexSize = m_SeaFloorMesh.GetMesh()->m_dwVertexSize;

    // Add some bumpiness to the seafloor
    {
        srand( 5 );
        BYTE* pDst;
        m_pSeaFloorVB->Lock( 0, 0, ( VOID** )&pDst, 0 );
        for( DWORD i = 0; i < m_dwNumSeaFloorVertices; i++ )
        {
            ( ( XMFLOAT3* )pDst )->y += ( rand() / ( FLOAT )RAND_MAX );
            ( ( XMFLOAT3* )pDst )->y += ( rand() / ( FLOAT )RAND_MAX );
            ( ( XMFLOAT3* )pDst )->y += ( rand() / ( FLOAT )RAND_MAX );
            pDst += m_dwSeaFloorVertexSize;
        }
        m_pSeaFloorVB->Unlock();
    }

    // Build the vertex declaration for the dolphin
    D3DVERTEXELEMENT9 declDolphin[MAXD3DDECLLENGTH] =
    {
        0
    };
    ATG::AppendVertexElements( declDolphin, 0, m_DolphinMesh1.GetMesh()->m_VertexElements, 0 );
    ATG::AppendVertexElements( declDolphin, 1, m_DolphinMesh2.GetMesh()->m_VertexElements, 1 );
    ATG::AppendVertexElements( declDolphin, 2, m_DolphinMesh3.GetMesh()->m_VertexElements, 2 );

    HRESULT hr;
    if( FAILED( hr = m_pd3dDevice->CreateVertexDeclaration(
                declDolphin, &m_pDolphinVertexDeclaration ) ) )
        return hr;

    VOID* pCode = NULL;
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\DolphinTween.xvu", &pCode ) ) )
        ATG::FatalError( "Dolphin: Couldn't create DolphinTween.xvu" );

    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pDolphinVertexShader ) ) )
        ATG::FatalError( "Dolphin: Couldn't create DolphinTween.xvu" );
    ATG::UnloadFile( pCode );

    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\SeaFloor.xvu", &pCode ) ) )
        ATG::FatalError( "Dolphin: Couldn't create SeaFloor.xvu" );

    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pSeaFloorVertexShader ) ) )
        ATG::FatalError( "Could not create a vertex shader" );
    ATG::UnloadFile( pCode );

    // Create the common pixel shader
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\ShadeCausticsPixel.xpu", &pCode ) ) )
        ATG::FatalError( "Dolphin: Couldn't create ShadeCausticsPixel.xpu" );

    if( FAILED( hr = m_pd3dDevice->CreatePixelShader( ( DWORD* )pCode, &m_pPixelShader ) ) )
        return hr;
    ATG::UnloadFile( pCode );

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the transform matrices
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -5.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matWorld = XMMatrixIdentity();
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, 1.0f, 10000.0f );

    // Initialize the UI
    if( FAILED( hr = InitUI( &g_atgApp ) ) )
        ATG::FatalError( "Failed initializing XUI" );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();
    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    XINPUT_KEYSTROKE keyStroke;
    // retrieve and dispatch input to the UI
    XInputGetKeystroke( XUSER_INDEX_ANY, XINPUT_FLAG_ANYDEVICE, &keyStroke );
    DispatchXuiInput( &keyStroke );

    return S_OK;
}

void Sample::SetConstants()
{
    FLOAT fTime = ( FLOAT )m_Timer.GetAppTime();

    // Animation attributes for the dolphin
    FLOAT fKickFreq = 2 * fTime;
    FLOAT fPhase = fTime / 3;
    FLOAT fBlendWeight = sinf( fKickFreq );

    // Move the dolphin in a circle
    XMMATRIX matDolphin, matTrans, matRotate1, matRotate2;
    matDolphin = XMMatrixScaling( 0.01f, 0.01f, 0.01f );
    matRotate1 = XMMatrixRotationZ( -cosf( fKickFreq ) / 6 );
    matDolphin = XMMatrixMultiply( matDolphin, matRotate1 );
    matRotate2 = XMMatrixRotationY( fPhase );
    matDolphin = XMMatrixMultiply( matDolphin, matRotate2 );
    matTrans = XMMatrixTranslation( -5 * sinf( fPhase ), sinf( fKickFreq ) / 2, 10 - 10 * cosf( fPhase ) );
    matDolphin = XMMatrixMultiply( matDolphin, matTrans );

    // Animate the caustic textures
    DWORD tex = ( ( DWORD )( fTime * 32 ) ) % 32;
    m_pCurrentCausticTexture = m_pCausticTextures[ tex ];

    // Set the vertex shader constants. Note: outside of the blend matrices,
    // most of these values don't change, so don't need to really be set every
    // frame. It's just done here for clarity
    {
        // Some basic constants
        static XMFLOAT4 vZero( 0.0f, 0.0f, 0.0f, 0.0f );
        static XMFLOAT4 vConstants( 1.0f, 0.5f, 0.2f, 0.05f );

        FLOAT fWeight1;
        FLOAT fWeight2;
        FLOAT fWeight3;

        if( fBlendWeight > 0.0f )
        {
            fWeight1 = fabsf( fBlendWeight );
            fWeight2 = 1.0f - fabsf( fBlendWeight );
            fWeight3 = 0.0f;
        }
        else
        {
            fWeight1 = 0.0f;
            fWeight2 = 1.0f - fabsf( fBlendWeight );
            fWeight3 = fabsf( fBlendWeight );
        }
        XMVECTOR vWeight = XMVectorSet( fWeight1, fWeight2, fWeight3, 0.0f );

        // Lighting vectors (in world space and in dolphin model space)
        // and other constants
        XMVECTOR vLight = XMVectorSet( 0.00f, 1.00f, 0.00f, 0.00f );
        XMVECTOR vLightDolphinSpace = XMVectorSet( 0.00f, 1.00f, 0.00f, 0.00f );
        XMVECTOR vDiffuse = XMVectorSet( 1.00f, 1.00f, 1.00f, 1.00f );
        XMVECTOR vAmbient = XMVectorSet( 0.25f, 0.25f, 0.25f, 0.25f );
        XMVECTOR vFog = XMVectorSet( 0.50f, 50.00f, 1.00f / ( 50.0f - 1.0f ), 0.00f );
        XMVECTOR vCaustics = XMVectorSet( 0.05f, 0.05f, sinf( fTime ) / 8, cosf( fTime ) / 10 );

        XMVECTOR vDeterminant;
        XMMATRIX matDolphinInv = XMMatrixInverse( &vDeterminant, matDolphin );
        vLightDolphinSpace = XMVector4Normalize( XMVector4Transform( vLight, matDolphinInv ) );

        // Vertex shader operations use transposed matrices
        XMMATRIX mat, matCamera, matTranspose, matCameraTranspose;
        XMMATRIX matViewTranspose, matProjTranspose;
        matCamera = XMMatrixMultiply( matDolphin, m_matView );
        mat = XMMatrixMultiply( matCamera, m_matProj );
        matTranspose = XMMatrixTranspose( mat );
        matCameraTranspose = XMMatrixTranspose( matCamera );
        matViewTranspose = XMMatrixTranspose( m_matView );
        matProjTranspose = XMMatrixTranspose( m_matProj );

        // Set the vertex shader constants
        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&vZero, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 1, ( FLOAT* )&vConstants, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 2, ( FLOAT* )&vWeight, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matTranspose, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&matCameraTranspose, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 12, ( FLOAT* )&matViewTranspose, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 16, ( FLOAT* )&matProjTranspose, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 20, ( FLOAT* )&vLight, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 21, ( FLOAT* )&vLightDolphinSpace, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 22, ( FLOAT* )&vDiffuse, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 23, ( FLOAT* )&vAmbient, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 24, ( FLOAT* )&vFog, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 25, ( FLOAT* )&vCaustics, 1 );
    }
}

//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Called once per frame, the call is the entry point for 3D rendering. This
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    UpdateUI();

    HRESULT hr = RenderScene();
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    return hr;
}

HRESULT Sample::RenderScene()
{
    SetConstants();

    // Clear the viewport
    IDirect3DSurface9* pRenderTarget = NULL;
    m_pd3dDevice->GetRenderTarget( 0, &pRenderTarget );
    if( !pRenderTarget )
        return S_OK;

    D3DSURFACE_DESC desc;
    pRenderTarget->GetDesc( &desc );
    D3DRECT rctClear;
    rctClear.x1 = 0;
    rctClear.x2 = desc.Width;
    rctClear.y1 = 0;
    rctClear.y2 = desc.Height;

    pRenderTarget->Release();

    m_pd3dDevice->Clear( 1L, &rctClear, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                         g_dwWaterColor, 1.0f, 0L );


    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE,
                                  ( m_dwRenderOptions & DOLPHIN_RENDER_WIREFRAME ) ?
                                  D3DFILL_WIREFRAME : D3DFILL_SOLID );

    // Initialize default device states at the start of the frame
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetVertexShader( NULL );
    m_pd3dDevice->SetVertexDeclaration( NULL );
    m_pd3dDevice->SetPixelShader( NULL );

    // Set the common pixel shader
    static FLOAT fAmbient[] =
    {
        0.25f, 0.25f, 0.25f, 0.25f
    };
    m_pd3dDevice->SetPixelShader( m_pPixelShader );
    m_pd3dDevice->SetPixelShaderConstantF( 0, g_fWaterColor, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 1, fAmbient, 1 );


    // Render the seafloor
    if( m_dwRenderOptions & DOLPHIN_RENDER_FLOOR )
    {
        m_pd3dDevice->SetTexture( 0, m_pSeaFloorTexture );
        m_pd3dDevice->SetTexture( 1, m_pCurrentCausticTexture );
        m_pd3dDevice->SetVertexDeclaration( m_pSeaFloorVertexDeclaration );
        m_pd3dDevice->SetVertexShader( m_pSeaFloorVertexShader );
        m_pd3dDevice->SetStreamSource( 0, m_pSeaFloorVB, 0, m_dwSeaFloorVertexSize );
        m_pd3dDevice->SetIndices( m_pSeaFloorIB );
        m_pd3dDevice->DrawIndexedPrimitive( m_dwSeaFloorPrimType, 0,
                                            0, m_dwNumSeaFloorVertices,
                                            0, m_dwNumSeaFloorPrimitives );
    }

    // Render the dolphin
    if( m_dwRenderOptions & DOLPHIN_RENDER_DOLPHIN )
    {
        m_pd3dDevice->SetTexture( 0, m_pDolphinTexture );
        m_pd3dDevice->SetTexture( 1, m_pCurrentCausticTexture );
        m_pd3dDevice->SetVertexDeclaration( m_pDolphinVertexDeclaration );
        m_pd3dDevice->SetVertexShader( m_pDolphinVertexShader );
        m_pd3dDevice->SetStreamSource( 0, m_pDolphinVB1, 0, m_dwDolphinVertexSize );
        m_pd3dDevice->SetStreamSource( 1, m_pDolphinVB2, 0, m_dwDolphinVertexSize );
        m_pd3dDevice->SetStreamSource( 2, m_pDolphinVB3, 0, m_dwDolphinVertexSize );
        m_pd3dDevice->SetIndices( m_pDolphinIB );
        m_pd3dDevice->DrawIndexedPrimitive( m_dwDolphinPrimType, 0,
                                            0, m_dwNumDolphinVertices,
                                            0, m_dwNumDolphinPrimitives );
    }

    // Output statistics
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else if( m_dwRenderOptions & DOLPHIN_RENDER_STATS )
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"XuiDolphin" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Render UI
    RenderUI( m_pd3dDevice, desc.Width, desc.Height );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Changes for UI implementation
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: GetApp
// Desc: Helper to return UI accessible interface for the application
//--------------------------------------------------------------------------------------
IDolphinUI* GetApp()
{
    return &g_atgApp;
}


//--------------------------------------------------------------------------------------
// Name: GetRenderOptions
// Desc: Called by the UI to retrieve current rendering options.
//--------------------------------------------------------------------------------------
DWORD Sample::GetRenderOptions()
{
    return m_dwRenderOptions;
}

//--------------------------------------------------------------------------------------
// Name: SetRenderOptions
// Desc: Called by the UI to set the rendering options
//--------------------------------------------------------------------------------------
void Sample::SetRenderOptions( DWORD dwRenderOptions )
{
    m_dwRenderOptions = dwRenderOptions;
}

//--------------------------------------------------------------------------------------
// Name: GetDolphinStyle
// Desc: Called by the UI to retrieve the current dolphin style.
//--------------------------------------------------------------------------------------
DOLPHIN_STYLE Sample::GetDolphinStyle()
{
    return m_nDolphinStyle;
}

//--------------------------------------------------------------------------------------
// Name: SetDolphinStyle
// Desc: Called by the UI to set the current dolphin style.
//--------------------------------------------------------------------------------------
void Sample::SetDolphinStyle( DOLPHIN_STYLE nDolphinStyle )
{
    m_nDolphinStyle = nDolphinStyle;
    m_pDolphinTexture = m_pDolphinTextures[ nDolphinStyle ];
}

//--------------------------------------------------------------------------------------
// Name: GetDolphinStyleDesc
// Desc: Called by the UI to retrieve the description for the specified dolphin style.
//--------------------------------------------------------------------------------------
LPCWSTR Sample::GetDolphinStyleDesc( DOLPHIN_STYLE nDolphinStyle )
{
    return g_szDolphinStyleDesc[ nDolphinStyle ];
}

//--------------------------------------------------------------------------------------
// Name: GetD3DDevice
// Desc: Called by the UI to retrieve the IDirect3DDevice9 owned by the application.
//--------------------------------------------------------------------------------------
IDirect3DDevice9* Sample::GetD3DDevice()
{
    return m_pd3dDevice;
}

//--------------------------------------------------------------------------------------
// Name: RenderDolphinStyle
// Desc: Called by the UI to render the specified dolphin style to a device
//--------------------------------------------------------------------------------------
void Sample::RenderDolphinStyle( DOLPHIN_STYLE nDolphinStyle, IDirect3DDevice9* pDevice )
{
    UpdatePreview();

    D3DRECT rctClear;
    rctClear.x1 = 0;
    rctClear.x2 = 256;
    rctClear.y1 = 0;
    rctClear.y2 = 256;

    pDevice->Clear( 1, &rctClear, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                    D3DCOLOR_ARGB( 255, 0, 0, 0 ), 1, 0 );

    // Initialize default device states at the start of the frame
    pDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
    pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );
    pDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    pDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    pDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    // Set the common pixel shader
    static FLOAT fAmbient[] =
    {
        0.25f, 0.25f, 0.25f, 0.25f
    };
    pDevice->SetPixelShader( m_pPixelShader );
    pDevice->SetPixelShaderConstantF( 0, g_fWaterColor, 1 );
    pDevice->SetPixelShaderConstantF( 1, fAmbient, 1 );

    // Render the dolphin
    pDevice->SetTexture( 0, m_pDolphinTextures[ nDolphinStyle ] );
    pDevice->SetTexture( 1, m_pCurrentCausticTexture );
    pDevice->SetVertexDeclaration( m_pDolphinVertexDeclaration );
    pDevice->SetVertexShader( m_pDolphinVertexShader );
    pDevice->SetStreamSource( 0, m_pDolphinVB1, 0, m_dwDolphinVertexSize );
    pDevice->SetStreamSource( 1, m_pDolphinVB2, 0, m_dwDolphinVertexSize );
    pDevice->SetStreamSource( 2, m_pDolphinVB3, 0, m_dwDolphinVertexSize );
    pDevice->SetIndices( m_pDolphinIB );
    pDevice->DrawIndexedPrimitive( m_dwDolphinPrimType, 0,
                                   0, m_dwNumDolphinVertices,
                                   0, m_dwNumDolphinPrimitives );
}

//--------------------------------------------------------------------------------------
// Name: UpdatePreview
// Desc: Called from RenderDolphinStyle to setup the device constants for rendering
//       the dolphin style preview
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdatePreview()
{
    FLOAT fTime = ( FLOAT )m_Timer.GetAppTime();

    // Animation attributes for the dolphin
    FLOAT fKickFreq = 2 * fTime;
    FLOAT fPhase = fTime / 3;
    FLOAT fBlendWeight = sinf( fKickFreq );

    // Move the dolphin in a circle
    XMMATRIX matDolphin, matTrans, matRotate1, matRotate2;
    matDolphin = XMMatrixScaling( 0.01f, 0.01f, 0.01f );
    matRotate1 = XMMatrixRotationZ( -cosf( fKickFreq ) / 6 );
    matDolphin = XMMatrixMultiply( matDolphin, matRotate1 );
    matRotate2 = XMMatrixRotationY( fPhase );
    matDolphin = XMMatrixMultiply( matDolphin, matRotate2 );
    matTrans = XMMatrixTranslation( -5 * sinf( fPhase ), sinf( fKickFreq ) / 2, 10 - 10 * cosf( fPhase ) );
    matDolphin = XMMatrixMultiply( matDolphin, matTrans );

    // Animate the caustic textures
    DWORD tex = ( ( DWORD )( fTime * 32 ) ) % 32;
    m_pCurrentCausticTexture = m_pCausticTextures[ tex ];

    // Set the vertex shader constants. Note: outside of the blend matrices,
    // most of these values don't change, so don't need to really be set every
    // frame. It's just done here for clarity
    {
        // Some basic constants
        static XMFLOAT4 vZero( 0.0f, 0.0f, 0.0f, 0.0f );
        static XMFLOAT4 vConstants( 1.0f, 0.5f, 0.2f, 0.05f );

        FLOAT fWeight1;
        FLOAT fWeight2;
        FLOAT fWeight3;

        if( fBlendWeight > 0.0f )
        {
            fWeight1 = fabsf( fBlendWeight );
            fWeight2 = 1.0f - fabsf( fBlendWeight );
            fWeight3 = 0.0f;
        }
        else
        {
            fWeight1 = 0.0f;
            fWeight2 = 1.0f - fabsf( fBlendWeight );
            fWeight3 = fabsf( fBlendWeight );
        }
        XMVECTOR vWeight = XMVectorSet( fWeight1, fWeight2, fWeight3, 0.0f );

        // Lighting vectors (in world space and in dolphin model space)
        // and other constants
        XMVECTOR vLight = XMVectorSet( 0.00f, 1.00f, 0.00f, 0.00f );
        XMVECTOR vLightDolphinSpace = XMVectorSet( 0.00f, 1.00f, 0.00f, 0.00f );
        XMVECTOR vDiffuse = XMVectorSet( 1.00f, 1.00f, 1.00f, 1.00f );
        XMVECTOR vAmbient = XMVectorSet( 0.25f, 0.25f, 0.25f, 0.25f );
        XMVECTOR vFog = XMVectorSet( 0.50f, 50.00f, 1.00f / ( 50.0f - 1.0f ), 0.00f );
        XMVECTOR vCaustics = XMVectorSet( 0.05f, 0.05f, sinf( fTime ) / 8, cosf( fTime ) / 10 );

        XMVECTOR vDeterminant;
        XMMATRIX matDolphinInv = XMMatrixInverse( &vDeterminant, matDolphin );
        vLightDolphinSpace = XMVector4Normalize( XMVector4Transform( vLight, matDolphinInv ) );

        // Vertex shader operations use transposed matrices
        XMMATRIX mat, matCamera, matTranspose, matCameraTranspose;
        XMMATRIX matViewTranspose, matProjTranspose;
        matCamera = XMMatrixMultiply( matDolphin, m_matView );
        mat = XMMatrixMultiply( matCamera, m_matProj );
        matTranspose = XMMatrixTranspose( mat );
        matCameraTranspose = XMMatrixTranspose( matCamera );
        matViewTranspose = XMMatrixTranspose( m_matView );
        matProjTranspose = XMMatrixTranspose( m_matProj );

        // Set the vertex shader constants
        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&vZero, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 1, ( FLOAT* )&vConstants, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 2, ( FLOAT* )&vWeight, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matTranspose, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&matCameraTranspose, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 12, ( FLOAT* )&matViewTranspose, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 16, ( FLOAT* )&matProjTranspose, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 20, ( FLOAT* )&vLight, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 21, ( FLOAT* )&vLightDolphinSpace, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 22, ( FLOAT* )&vDiffuse, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 23, ( FLOAT* )&vAmbient, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 24, ( FLOAT* )&vFog, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 25, ( FLOAT* )&vCaustics, 1 );
    }
    return S_OK;
}
