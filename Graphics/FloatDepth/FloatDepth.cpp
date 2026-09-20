//--------------------------------------------------------------------------------------
// FloatDepth.cpp
//
// A sample showing how to use an inverted-direction floating-point depth buffer.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <xgraphics.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_1, L"Rotate camera" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Change depth-buffer\nformat" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_1, L"Wireframe" },
    { ATG::HELP_MISC_CALLOUT, ATG::HELP_PLACEMENT_1, L"Use triggers to zoom in/out" },
};
const DWORD NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Globals variables and definitions
//--------------------------------------------------------------------------------------
const FLOAT g_fInnerBoxColor[] = { 0.8f, 0.0f, 0.0f, 1.0f };
const FLOAT g_fOuterBoxColor[] = { 0.0f, 0.8f, 0.0f, 1.0f };

// Structure to hold vertex data.
struct BOXVERTEX
{
    XMFLOAT3 Position;
    XMFLOAT3 Normal;
};

// Unit box
BOXVERTEX g_BoxVertices[4*6] =
{
    // Front
    { XMFLOAT3( -1.0f, 1.0f, -1.0f ), XMFLOAT3( 0.0f, 0.0f, -1.0f ) },
    { XMFLOAT3( 1.0f, 1.0f, -1.0f ), XMFLOAT3( 0.0f, 0.0f, -1.0f ) },
    { XMFLOAT3( 1.0f, -1.0f, -1.0f ), XMFLOAT3( 0.0f, 0.0f, -1.0f ) },
    { XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT3( 0.0f, 0.0f, -1.0f ) },

    // Back
    { XMFLOAT3( -1.0f, 1.0f, 1.0f ), XMFLOAT3( 0.0f, 0.0f, 1.0f ) },
    { XMFLOAT3( 1.0f, 1.0f, 1.0f ), XMFLOAT3( 0.0f, 0.0f, 1.0f ) },
    { XMFLOAT3( 1.0f, -1.0f, 1.0f ), XMFLOAT3( 0.0f, 0.0f, 1.0f ) },
    { XMFLOAT3( -1.0f, -1.0f, 1.0f ), XMFLOAT3( 0.0f, 0.0f, 1.0f ) },

    // Left
    { XMFLOAT3( -1.0f, -1.0f, 1.0f ), XMFLOAT3( -1.0f, 0.0f, 0.0f ) },
    { XMFLOAT3( -1.0f, 1.0f, 1.0f ), XMFLOAT3( -1.0f, 0.0f, 0.0f ) },
    { XMFLOAT3( -1.0f, 1.0f, -1.0f ), XMFLOAT3( -1.0f, 0.0f, 0.0f ) },
    { XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT3( -1.0f, 0.0f, 0.0f ) },

    // Right
    { XMFLOAT3( 1.0f, -1.0f, 1.0f ), XMFLOAT3( 1.0f, 0.0f, 0.0f ) },
    { XMFLOAT3( 1.0f, 1.0f, 1.0f ), XMFLOAT3( 1.0f, 0.0f, 0.0f ) },
    { XMFLOAT3( 1.0f, 1.0f, -1.0f ), XMFLOAT3( 1.0f, 0.0f, 0.0f ) },
    { XMFLOAT3( 1.0f, -1.0f, -1.0f ), XMFLOAT3( 1.0f, 0.0f, 0.0f ) },

    // Bottom
    { XMFLOAT3( -1.0f, -1.0f, 1.0f ), XMFLOAT3( 0.0f, -1.0f, 0.0f ) },
    { XMFLOAT3( 1.0f, -1.0f, 1.0f ), XMFLOAT3( 0.0f, -1.0f, 0.0f ) },
    { XMFLOAT3( 1.0f, -1.0f, -1.0f ), XMFLOAT3( 0.0f, -1.0f, 0.0f ) },
    { XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT3( 0.0f, -1.0f, 0.0f ) },

    // Top
    { XMFLOAT3( -1.0f, 1.0f, 1.0f ), XMFLOAT3( 0.0f, 1.0f, 0.0f ) },
    { XMFLOAT3( 1.0f, 1.0f, 1.0f ), XMFLOAT3( 0.0f, 1.0f, 0.0f ) },
    { XMFLOAT3( 1.0f, 1.0f, -1.0f ), XMFLOAT3( 0.0f, 1.0f, 0.0f ) },
    { XMFLOAT3( -1.0f, 1.0f, -1.0f ), XMFLOAT3( 0.0f, 1.0f, 0.0f ) },
};


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;    // Timer
    ATG::Font m_Font;     // Font for drawing text
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    BOOL m_bFloatDepth;
    BOOL m_bWireframe;

    // Transform matrices
    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    // View parameters
    XMVECTOR m_vEye;
    XMVECTOR m_vLookAt;
    XMVECTOR m_vUp;

    // Vertex buffers
    LPDIRECT3DVERTEXBUFFER9 m_pInnerBoxVB;
    LPDIRECT3DVERTEXBUFFER9 m_pOuterBoxVB;

    // Shaders
    LPDIRECT3DVERTEXSHADER9 m_pBoxVS;
    LPDIRECT3DPIXELSHADER9 m_pBoxPS;

    // Depth buffers
    LPDIRECT3DSURFACE9 m_pFixedDepth;
    LPDIRECT3DSURFACE9 m_pFloatDepth;

public:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;

    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // We are manually creating our depth/stencil buffers to allow the sample to switch
    // formats. If we only wanted one format would could set AutoDepthStencilFormat.
    atgApp.m_d3dpp.EnableAutoDepthStencil = FALSE;

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    // Create the font
    if( FAILED( hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create font\n" );
        return hr;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    m_bDrawHelp = FALSE;

    // Create a normal and a floating-point depth buffer that share the same memory
    // so that we can switch between them.
    D3DSURFACE_PARAMETERS SurfParams;

    SurfParams.Base = XGSurfaceSize( m_d3dpp.BackBufferWidth,
                                     m_d3dpp.BackBufferHeight,
                                     D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE );
    SurfParams.ColorExpBias = 0;
    SurfParams.HierarchicalZBase = 0;
    SurfParams.HiZFunc = D3DHIZFUNC_DEFAULT;

    hr = m_pd3dDevice->CreateDepthStencilSurface( m_d3dpp.BackBufferWidth,
                                                  m_d3dpp.BackBufferHeight,
                                                  D3DFMT_D24S8, D3DMULTISAMPLE_NONE,
                                                  0, FALSE, &m_pFixedDepth, &SurfParams );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Couldn't depth buffer\n" );
        return hr;
    }

    hr = m_pd3dDevice->CreateDepthStencilSurface( m_d3dpp.BackBufferWidth,
                                                  m_d3dpp.BackBufferHeight,
                                                  D3DFMT_D24FS8, D3DMULTISAMPLE_NONE,
                                                  0, FALSE, &m_pFloatDepth, &SurfParams );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Couldn't depth buffer\n" );
        return hr;
    }

    // Create the box vertex shader
    if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\FloatDepthVS.xvu", &m_pBoxVS ) ) )
    {
        ATG_PrintError( "Couldn't create FloatDepthVS.xvu\n" );
        return hr;
    }

    // Create the box pixel shader
    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\FloatDepthPS.xpu", &m_pBoxPS ) ) )
    {
        ATG_PrintError( "Couldn't create FloatDepthPS.xpu\n" );
        return hr;
    }

    // Create and initialize vertex buffers
    if( FAILED( m_pd3dDevice->CreateVertexBuffer( sizeof( g_BoxVertices ),
                                                  D3DUSAGE_WRITEONLY,
                                                  NULL,
                                                  D3DPOOL_DEFAULT,
                                                  &m_pInnerBoxVB,
                                                  NULL ) ) )
        return E_FAIL;

    // Put the data for the patches into our vertex buffer.
    BOXVERTEX* pVertices;
    if( FAILED( m_pInnerBoxVB->Lock( 0, 0, ( void** )&pVertices, 0 ) ) )
        return E_FAIL;

    XMVECTOR InnerScale = { 0.84f, 0.84f, 0.84f, 0.84f };

    for( UINT i = 0; i < 4 * 6; i++ )
    {
        // Scale position
        XMVECTOR Position = XMLoadFloat3( &g_BoxVertices[i].Position );
        Position = Position * InnerScale;
        XMStoreVector3( &pVertices[i].Position, Position );

        pVertices[i].Normal = g_BoxVertices[i].Normal;
    }

    m_pInnerBoxVB->Unlock();

    // Create and initialize vertex buffers
    if( FAILED( m_pd3dDevice->CreateVertexBuffer( sizeof( g_BoxVertices ),
                                                  D3DUSAGE_WRITEONLY,
                                                  NULL,
                                                  D3DPOOL_DEFAULT,
                                                  &m_pOuterBoxVB,
                                                  NULL ) ) )
        return E_FAIL;

    // Put the data for the patches into our vertex buffer.
    if( FAILED( m_pOuterBoxVB->Lock( 0, 0, ( void** )&pVertices, 0 ) ) )
        return E_FAIL;

    for( UINT i = 0; i < 4 * 6; i++ )
    {
        pVertices[i] = g_BoxVertices[i];
    }

    m_pOuterBoxVB->Unlock();

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    m_vEye = XMVectorSet( 0.0f, 0.0f, -5.0f, 0.0f );
    m_vLookAt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    m_vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

    // Set the transform matrices
    m_matWorld = XMMatrixIdentity();
    m_matView = XMMatrixLookAtLH( m_vEye, m_vLookAt, m_vUp );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, 0.01f, 100.0f );

    m_bFloatDepth = TRUE;
    m_bWireframe = FALSE;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the elapsed time.
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Change the depth buffer type
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_bFloatDepth = !m_bFloatDepth;

    // Toggle wireframe mode
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_bWireframe = !m_bWireframe;

    // Rotate eye around up axis.
    XMMATRIX matRotate = XMMatrixRotationAxis( m_vUp, pGamepad->fX1 * fElapsedTime );
    m_vEye = XMVector3TransformCoord( m_vEye, matRotate );

    // Rotate eye points around side axis
    XMVECTOR vView = ( m_vLookAt - m_vEye );
    FLOAT dist = XMVector3Length( vView ).x;

    vView = XMVector3Normalize( vView );

    // Place limits so we don't go over the top or under the bottom
    FLOAT dot = XMVector3Dot( vView, m_vUp ).x;
    if( ( dot < 0.99f || pGamepad->fY1 < 0.0f ) && ( dot > -0.99f || pGamepad->fY1 > 0.0f ) )
    {
        XMVECTOR vAxis = XMVector3Cross( vView, m_vUp );
        matRotate = XMMatrixRotationAxis( vAxis, pGamepad->fY1 * fElapsedTime );
        m_vEye = XMVector3TransformCoord( m_vEye, matRotate );
    }

    // Move in/out
    FLOAT fIn = ( pGamepad->bRightTrigger / 255.0f );
    FLOAT fOut = ( pGamepad->bLeftTrigger / 255.0f );

    if( fIn > 0.1f && dist > 1.5f )
        m_vEye += vView * 40.0f * fIn * fElapsedTime;

    if( fOut > 0.1f && dist < 98.5f )
        m_vEye -= vView * 40.0f * fOut * fElapsedTime;

    m_matView = XMMatrixLookAtLH( m_vEye, m_vLookAt, m_vUp );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering. This 
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    float fClearDepth;

    if( m_bFloatDepth )
    {
        // Floating-point depth buffer
        m_pd3dDevice->SetDepthStencilSurface( m_pFloatDepth );

        m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_GREATEREQUAL );

        // Near is one, far is zero
        D3DVIEWPORT9 Viewport;
        m_pd3dDevice->GetViewport( &Viewport );
        Viewport.MinZ = 1.0f;
        Viewport.MaxZ = 0.0f;
        m_pd3dDevice->SetViewport( &Viewport );

        // Clear to zero
        fClearDepth = 0.0f;
    }
    else
    {
        // Fixed-point depth buffer
        m_pd3dDevice->SetDepthStencilSurface( m_pFixedDepth );

        m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );

        // Near is zero, far is one
        D3DVIEWPORT9 Viewport;
        m_pd3dDevice->GetViewport( &Viewport );
        Viewport.MinZ = 0.0f;
        Viewport.MaxZ = 1.0f;
        m_pd3dDevice->SetViewport( &Viewport );

        // Clear to one
        fClearDepth = 1.0f;
    }

    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                         0x00000000, fClearDepth, 0L );

    // Initialize default device states at the start of the frame
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    if( m_bWireframe )
        m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );

    // Set the common vertex and pixel shaders
    m_pd3dDevice->SetVertexShader( m_pBoxVS );
    m_pd3dDevice->SetPixelShader( m_pBoxPS );

    // Setup the vertex shader inputs.
    XMMATRIX matWVP = m_matWorld * m_matView * m_matProj;
    matWVP = XMMatrixTranspose( matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );

    // Compute the inverse world matrix.
    XMVECTOR vDet;
    XMMATRIX matInvWorld = XMMatrixInverse( &vDet, m_matWorld );
    assert( vDet.x > 0.0f );

    XMVECTOR vLightDirection = XMVectorSet( 1.0f, 0.5f, -1.0f, 0.0f );

    // Transform the light direction into object space.
    XMVECTOR vLocalLightDir = XMVector4Transform( vLightDirection, matInvWorld );
    vLocalLightDir = XMVector4Normalize( vLocalLightDir );
    m_pd3dDevice->SetVertexShaderConstantF( 5, ( FLOAT* )&vLocalLightDir, 1 );

    // Transform the view position into object space.
    XMVECTOR vLocalViewPos = XMVector4Transform( m_vEye, matInvWorld );
    m_pd3dDevice->SetVertexShaderConstantF( 6, ( FLOAT* )&vLocalViewPos, 1 );

    // Render the outer box (green)
    m_pd3dDevice->SetPixelShaderConstantF( 0, g_fOuterBoxColor, 1 );

    m_pd3dDevice->SetFVF( D3DFVF_XYZ | D3DFVF_NORMAL );
    m_pd3dDevice->SetStreamSource( 0, m_pOuterBoxVB, 0, sizeof( BOXVERTEX ) );

    m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 6 );

    // Render the inner box (red)
    m_pd3dDevice->SetPixelShaderConstantF( 0, g_fInnerBoxColor, 1 );

    m_pd3dDevice->SetFVF( D3DFVF_XYZ | D3DFVF_NORMAL );
    m_pd3dDevice->SetStreamSource( 0, m_pInnerBoxVB, 0, sizeof( BOXVERTEX ) );

    m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 6 );

    // Restore some render states
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

    // Output title and framerate
    m_Timer.MarkFrame();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"FloatDepth" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        const WCHAR* str = ( m_bFloatDepth ? L"Floating-point depth" : L"Fixed-point depth" );
        m_Font.DrawText( 0, 30, 0xffffffff, str, ATGFONT_RIGHT );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
