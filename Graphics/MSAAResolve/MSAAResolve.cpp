//--------------------------------------------------------------------------------------
// MSAAResolve.cpp
//
// Shows how to resolve a multi-sample anti-aliased render target
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <xgraphics.h>


//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_1, L"Display help" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_1, L"Pause" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Object size" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_2, L"Color\nresolve" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_2, L"Depth\nresolve" },
    { ATG::HELP_Y_BUTTON,     ATG::HELP_PLACEMENT_1, L"MSAA type" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Fill\nconvention" },
    { ATG::HELP_LEFT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Toggle\nwireframe" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_1, L"Zoom window scroll" },
    { ATG::HELP_BOTTOM_LEFT,  ATG::HELP_PLACEMENT_2, L"Triggers zoom in/out" },
    { ATG::HELP_BOTTOM_CENTER, ATG::HELP_PLACEMENT_1, L"Sample points shown in green" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Sample constants
//--------------------------------------------------------------------------------------
static const UINT       MSAA_WINDOW_SIZE = 256; // Size of MSAA render targets/display windows
static const UINT       REFERENCE_WINDOW_SIZE = 128; // Size of "reference" target w/zoom box
static const UINT       NUM_TETRAHEDRON_VERTS = 12;
static const UINT       NUM_TETRAHEDRON_FACES = 4;
static const XMFLOAT4   WHITECOLOR( 1.0f, 1.0f, 1.0f, 0.0f );
static const XMFLOAT4   REDCOLOR( 1.0f, 0.0f, 0.0f, 0.0f );
static const XMFLOAT4   GREENCOLOR( 0.0f, 1.0f, 0.0f, 0.0f );


//--------------------------------------------------------------------------------------
// MSAA constants
//--------------------------------------------------------------------------------------
// The MSAA sub-pixel grid is 16x16 units per pixel
static const FLOAT      SampleGridSize = 16.0f;

// Offsets for MSAA samples inside the sub-pixel grid.  The pixel center is at 0,0 and
// the grid extents from -8 to 8 on both the x and y axis.
static const XMFLOAT2   MSAA_2X_SAMPLE0( -4.0f / SampleGridSize,  4.0f / SampleGridSize );
static const XMFLOAT2   MSAA_2X_SAMPLE1( 4.0f / SampleGridSize, -4.0f / SampleGridSize );
static const XMFLOAT2   MSAA_4X_SAMPLE0( -2.0f / SampleGridSize, -2.0f / SampleGridSize );
static const XMFLOAT2   MSAA_4X_SAMPLE1( 2.0f / SampleGridSize,  2.0f / SampleGridSize );
static const XMFLOAT2   MSAA_4X_SAMPLE2( -6.0f / SampleGridSize,  6.0f / SampleGridSize );
static const XMFLOAT2   MSAA_4X_SAMPLE3( 6.0f / SampleGridSize, -6.0f / SampleGridSize );


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    BOOL m_bPaused;

    // Off screen textures (1x, 2x, and 4x MSAA render targets )
    D3DSurface* m_pColorTarget[3];
    D3DSurface* m_pDepthStencilTarget[3];

    // Resolve targets
    D3DTexture* m_pColorTexture;
    D3DTexture* m_pDepthStencilTexture;

    // MSAA and resolve flags
    DWORD m_dwCurrentColorResolve;
    DWORD m_dwCurrentDepthStencilResolve;
    DWORD m_dwCurrentMSAA;

    // Zoom constants
    INT m_iZoom;
    INT m_iOffsetX;
    INT m_iOffsetY;
    FLOAT m_fZoomX;
    FLOAT m_fZoomY;

    // Vertex and pixel shaders
    D3DVertexShader* m_pVSScreenspaceDiffuse;
    D3DVertexShader* m_pVSScreenspaceTexcoord;
    D3DVertexShader* m_pVSScreenspace;
    D3DPixelShader* m_pPSDiffuse;
    D3DPixelShader* m_pPSTexture;
    D3DPixelShader* m_pPSConstantColor;
    D3DVertexDeclaration* m_pVDScreenspaceDiffuse;
    D3DVertexDeclaration* m_pVDScreenspaceTexcoord;
    D3DVertexDeclaration* m_pVDScreenspace;

    // World, view, project, and projection matrices
    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    // D3D has a .5,.5 pixel center.  OpenGL has a 0,0 pixel center
    BOOL m_bMatchPixelAndTexelCenter;

    // Draw windows
    D3DRECT m_rcColorWindow;
    D3DRECT m_rcDepthWindow;
    D3DRECT m_rcReferenceWindow;

    // Size of tetrahedron
    FLOAT m_fTetrahedronSize;

    // Draw wire-frame tetrahedron
    BOOL m_bDrawWireframeTetrahedron;

    struct ScreenspaceDiffuseVert
    {
        XMFLOAT4 Pos;
        D3DCOLOR Color;
    };
    struct ScreenspaceTexcoordVert
    {
        XMFLOAT4 Pos;
        XMFLOAT2 UV;
    };
    struct ScreenspaceVert
    {
        XMFLOAT4 Pos;
    };

    // draw functions
    VOID            MakeTetrahedronVerts( FLOAT fSize, ScreenspaceDiffuseVert* pVerts );
    HRESULT         DrawTetrahedron( const D3DRECT* pWindiow,
                                     const XMMATRIX* pWorld, const FLOAT fSize );
    HRESULT         DrawWireframeTetrahedron( const D3DRECT* pWindow,
                                              FLOAT fOffsetX, FLOAT fOffsetY,
                                              FLOAT fZoom,
                                              const XMMATRIX* pWorld, const FLOAT fSize,
                                              const XMFLOAT4* pColor );
    HRESULT         DrawScreenQuad( const D3DRECT* pWindow,
                                    FLOAT fOffsetX, FLOAT fOffsetY,
                                    FLOAT fZoom,
                                    D3DTexture* pTexture );
    HRESULT         DrawScreenQuad( const D3DRECT* pWindow,
                                    const XMFLOAT4* pColor );
    HRESULT         DrawScreenGridAndSamplePoints( const D3DRECT* pWindow,
                                                   FLOAT fOffsetX, FLOAT fOffsetY,
                                                   FLOAT fZoom,
                                                   DWORD dwMSAA,
                                                   const XMFLOAT4* pGridColor,
                                                   const XMFLOAT4* pSamplesColor );
    HRESULT         DrawState();

private:
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
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    m_bDrawHelp = FALSE;

    // Load vertex and pixel shaders
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ScreenspaceDiffuse.xvu",
                                       &m_pVSScreenspaceDiffuse ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ScreenspaceTexcoord.xvu",
                                       &m_pVSScreenspaceTexcoord ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\Screenspace.xvu",
                                       &m_pVSScreenspace ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\Diffuse.xpu",
                                      &m_pPSDiffuse ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\Texture.xpu",
                                      &m_pPSTexture ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ConstantColor.xpu",
                                      &m_pPSConstantColor ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create vertex decls
    static const D3DVERTEXELEMENT9 DeclScreenSpaceDiffuse[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 4 * sizeof( FLOAT ), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,
            D3DDECLUSAGE_COLOR, 0 },
        D3DDECL_END()
    };
    if( FAILED( m_pd3dDevice->CreateVertexDeclaration( DeclScreenSpaceDiffuse,
                                                       &m_pVDScreenspaceDiffuse ) ) )
        return E_FAIL;

    static const D3DVERTEXELEMENT9 DeclPositionTexcoord[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 4 * sizeof( FLOAT ), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,
            D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    if( FAILED( m_pd3dDevice->CreateVertexDeclaration( DeclPositionTexcoord,
                                                       &m_pVDScreenspaceTexcoord ) ) )
        return E_FAIL;

    static const D3DVERTEXELEMENT9 DeclScreenspace[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        D3DDECL_END()
    };
    if( FAILED( m_pd3dDevice->CreateVertexDeclaration( DeclScreenspace,
                                                       &m_pVDScreenspace ) ) )
        return E_FAIL;

    // Create 1X, 2X, and 4X render targets and depth-stencil buffers
    for( DWORD i = 0; i < 3; i++ )
    {
        D3DMULTISAMPLE_TYPE MSAA_Type;

        if( i == 0 )           MSAA_Type = D3DMULTISAMPLE_NONE;
        else if( i == 1 )      MSAA_Type = D3DMULTISAMPLE_2_SAMPLES;
        else
            MSAA_Type = D3DMULTISAMPLE_4_SAMPLES;

        // all color targets are at EDRAM and Hi-Z address 0 (not all used at once)
        D3DSURFACE_PARAMETERS SurfaceParams = { 0, 0 };
        if( FAILED( m_pd3dDevice->CreateRenderTarget( MSAA_WINDOW_SIZE,
                                                      MSAA_WINDOW_SIZE,
                                                      ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
                                                      MSAA_Type,
                                                      0, FALSE,
                                                      &m_pColorTarget[i],
                                                      &SurfaceParams ) ) )
            return E_FAIL;

        // all depth-stencil targets are after their color targets in EDRAM and Hi-Z
        SurfaceParams.Base = XGSurfaceSize( MSAA_WINDOW_SIZE,
                                            MSAA_WINDOW_SIZE,
                                            D3DFMT_A8R8G8B8,
                                            MSAA_Type );
        SurfaceParams.HierarchicalZBase = XGHierarchicalZSize( MSAA_WINDOW_SIZE,
                                                               MSAA_WINDOW_SIZE,
                                                               MSAA_Type );
        if( FAILED( m_pd3dDevice->CreateDepthStencilSurface( MSAA_WINDOW_SIZE,
                                                             MSAA_WINDOW_SIZE,
                                                             D3DFMT_D24S8,
                                                             MSAA_Type,
                                                             0, FALSE,
                                                             &m_pDepthStencilTarget[i],
                                                             &SurfaceParams ) ) )
            return E_FAIL;
    }

    // Create off screen color and depth-stencil textures
    if( FAILED( m_pd3dDevice->CreateTexture( MSAA_WINDOW_SIZE, MSAA_WINDOW_SIZE,
                                             1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ), 0,
                                             &m_pColorTexture, NULL ) ) )
        return E_FAIL;
    if( FAILED( m_pd3dDevice->CreateTexture( MSAA_WINDOW_SIZE, MSAA_WINDOW_SIZE,
                                             1, 0, D3DFMT_D24S8, 0,
                                             &m_pDepthStencilTexture, NULL ) ) )
        return E_FAIL;

    // Set view and projection matrices
    XMVECTOR vEyePosition = XMVectorSet( 0.0f, 0.0f, -2.0f, 0.0f );
    XMVECTOR vFocusPosition = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUpDirection = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matView = XMMatrixLookAtLH( vEyePosition, vFocusPosition, vUpDirection );

    // Use a square aspect ratio since we are rendering off-screen to a square window
    FLOAT fAspectRatio = 1.0f;
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4.0f, fAspectRatio, 1.0f, 1000.0f );

    // Set initial MSAA and resolve flags
    m_dwCurrentMSAA = D3DMULTISAMPLE_2_SAMPLES;
    m_dwCurrentColorResolve = D3DRESOLVE_ALLFRAGMENTS;
    m_dwCurrentDepthStencilResolve = D3DRESOLVE_FRAGMENT0;

    // Set up windows

    // Variables needed to draw color, depth, and reference windows
    UINT dwWidth = m_d3dpp.BackBufferWidth;
    UINT dwHeight = m_d3dpp.BackBufferHeight;
    m_rcColorWindow.x1 = dwWidth / 4 - MSAA_WINDOW_SIZE / 2;
    m_rcColorWindow.y1 = dwHeight / 2 - MSAA_WINDOW_SIZE / 2;
    m_rcColorWindow.x2 = dwWidth / 4 + MSAA_WINDOW_SIZE / 2;
    m_rcColorWindow.y2 = dwHeight / 2 + MSAA_WINDOW_SIZE / 2;
    m_rcDepthWindow.x1 = 3 * dwWidth / 4 - MSAA_WINDOW_SIZE / 2;
    m_rcDepthWindow.y1 = dwHeight / 2 - MSAA_WINDOW_SIZE / 2;
    m_rcDepthWindow.x2 = 3 * dwWidth / 4 + MSAA_WINDOW_SIZE / 2;
    m_rcDepthWindow.y2 = dwHeight / 2 + MSAA_WINDOW_SIZE / 2;
    m_rcReferenceWindow.x1 = dwWidth / 2 - REFERENCE_WINDOW_SIZE / 2;
    m_rcReferenceWindow.y1 = 3 * dwHeight / 4 - REFERENCE_WINDOW_SIZE / 2;
    m_rcReferenceWindow.x2 = dwWidth / 2 + REFERENCE_WINDOW_SIZE / 2;
    m_rcReferenceWindow.y2 = 3 * dwHeight / 4 + REFERENCE_WINDOW_SIZE / 2;

    // Set initial zoom
    m_iZoom = 32;
    m_iOffsetX = 124;
    m_iOffsetY = 124;
    m_fZoomX = 0.0f;
    m_fZoomY = 0.0f;

    // Start with using 0,0 pixel center
    m_bMatchPixelAndTexelCenter = TRUE;

    m_fTetrahedronSize = 1.0f / 32.0f;
    m_bDrawWireframeTetrahedron = TRUE;

    // Not paused on start
    m_bPaused = FALSE;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Pause the timer
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        m_bPaused = !m_bPaused;

        if( m_bPaused )
            m_Timer.Stop();
        else
            m_Timer.Start();
    }

    // Update current MSAA
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        switch( m_dwCurrentMSAA )
        {
            case D3DMULTISAMPLE_NONE:
                m_dwCurrentMSAA = D3DMULTISAMPLE_2_SAMPLES; break;
            case D3DMULTISAMPLE_2_SAMPLES:
                m_dwCurrentMSAA = D3DMULTISAMPLE_4_SAMPLES; break;
            case D3DMULTISAMPLE_4_SAMPLES:
                m_dwCurrentMSAA = D3DMULTISAMPLE_NONE;      break;
        }
    }

    // Update current color resolve
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        switch( m_dwCurrentColorResolve )
        {
            case D3DRESOLVE_ALLFRAGMENTS:
                m_dwCurrentColorResolve = D3DRESOLVE_FRAGMENT0;    break;
            case D3DRESOLVE_FRAGMENT0:
                m_dwCurrentColorResolve = D3DRESOLVE_FRAGMENT1;    break;
            case D3DRESOLVE_FRAGMENT1:
                m_dwCurrentColorResolve = D3DRESOLVE_FRAGMENT2;    break;
            case D3DRESOLVE_FRAGMENT2:
                m_dwCurrentColorResolve = D3DRESOLVE_FRAGMENT3;    break;
            case D3DRESOLVE_FRAGMENT3:
                m_dwCurrentColorResolve = D3DRESOLVE_FRAGMENTS01;  break;
            case D3DRESOLVE_FRAGMENTS01:
                m_dwCurrentColorResolve = D3DRESOLVE_FRAGMENTS23;  break;
            case D3DRESOLVE_FRAGMENTS23:
                m_dwCurrentColorResolve = D3DRESOLVE_ALLFRAGMENTS; break;
        }
    }

    // Update current depth-stencil resolve
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        switch( m_dwCurrentDepthStencilResolve )
        {
            case D3DRESOLVE_FRAGMENT0:
                m_dwCurrentDepthStencilResolve = D3DRESOLVE_FRAGMENT1; break;
            case D3DRESOLVE_FRAGMENT1:
                m_dwCurrentDepthStencilResolve = D3DRESOLVE_FRAGMENT2; break;
            case D3DRESOLVE_FRAGMENT2:
                m_dwCurrentDepthStencilResolve = D3DRESOLVE_FRAGMENT3; break;
            case D3DRESOLVE_FRAGMENT3:
                m_dwCurrentDepthStencilResolve = D3DRESOLVE_FRAGMENT0; break;
        }
    }

    // Change tetrahedron size
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_fTetrahedronSize /= 2.0f;
        if( 1.0f / m_fTetrahedronSize > MSAA_WINDOW_SIZE )
            m_fTetrahedronSize = 1.0f;
    }

    // Switch half pixel offset
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        m_bMatchPixelAndTexelCenter = !m_bMatchPixelAndTexelCenter;
    }

    // Toggle wire-frame tetrahedron
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
    {
        m_bDrawWireframeTetrahedron = !m_bDrawWireframeTetrahedron;
    }

    // Clamp values based off current MSAA
    if( m_dwCurrentMSAA == D3DMULTISAMPLE_NONE )
    {
        m_dwCurrentColorResolve = D3DRESOLVE_ALLFRAGMENTS;
        m_dwCurrentDepthStencilResolve = D3DRESOLVE_FRAGMENT0;
    }

    if( m_dwCurrentMSAA == D3DMULTISAMPLE_2_SAMPLES )
    {
        if( m_dwCurrentColorResolve == D3DRESOLVE_FRAGMENT2 ||
            m_dwCurrentColorResolve == D3DRESOLVE_FRAGMENT3 ||
            m_dwCurrentColorResolve == D3DRESOLVE_FRAGMENTS01 ||
            m_dwCurrentColorResolve == D3DRESOLVE_FRAGMENTS23 )
        {
            m_dwCurrentColorResolve = D3DRESOLVE_ALLFRAGMENTS;
        }

        if( m_dwCurrentDepthStencilResolve == D3DRESOLVE_FRAGMENT2 ||
            m_dwCurrentDepthStencilResolve == D3DRESOLVE_FRAGMENT3 )
        {
            m_dwCurrentDepthStencilResolve = D3DRESOLVE_FRAGMENT0;
        }
    }

    // Update Zoom
    if( pGamepad->bPressedLeftTrigger )
    {
        m_iZoom /= 2;
        if( m_iZoom <= 0 )
            m_iZoom = 1;
        else
        {
            m_iOffsetX -= MSAA_WINDOW_SIZE / m_iZoom / 4;
            m_iOffsetY -= MSAA_WINDOW_SIZE / m_iZoom / 4;
        }

    }
    if( pGamepad->bPressedRightTrigger )
    {
        m_iZoom *= 2;
        if( m_iZoom > ( INT )MSAA_WINDOW_SIZE )
            m_iZoom = ( INT )MSAA_WINDOW_SIZE;
        else
        {
            m_iOffsetX += MSAA_WINDOW_SIZE / m_iZoom / 2;
            m_iOffsetY += MSAA_WINDOW_SIZE / m_iZoom / 2;
        }
    }

    // Update offsets
    FLOAT fZoomOffset = ( FLOAT )m_Timer.GetElapsedTime() * 60.0f;
    if( m_iZoom > 8 )
        fZoomOffset = fZoomOffset * 2.0f / m_iZoom;
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        m_iOffsetX -= 1;
    else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        m_iOffsetX += 1;
    else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        m_iOffsetY -= 1;
    else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        m_iOffsetY += 1;
    else if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        m_fZoomX -= fZoomOffset;
    else if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        m_fZoomX += fZoomOffset;
    else if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_UP )
        m_fZoomY -= fZoomOffset;
    else if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        m_fZoomY += fZoomOffset;
    else
    {
        m_fZoomX = 0.0f;
        m_fZoomY = 0.0f;
    }
    if( fabs( m_fZoomX ) >= 1.0f )
    {
        m_iOffsetX += ( INT )m_fZoomX;
        m_fZoomX = 0.0f;
    }
    if( fabs( m_fZoomY ) >= 1.0f )
    {
        m_iOffsetY += ( INT )m_fZoomY;
        m_fZoomY = 0.0f;
    }
    // Clamp
    INT iWindowSize = ( INT )MSAA_WINDOW_SIZE / m_iZoom;
    if( m_iOffsetX < 0 )
        m_iOffsetX = 0;
    if( m_iOffsetX >= ( INT )MSAA_WINDOW_SIZE - iWindowSize )
        m_iOffsetX = ( INT )MSAA_WINDOW_SIZE - iWindowSize;
    if( m_iOffsetY < 0 )
        m_iOffsetY = 0;
    if( m_iOffsetY >= ( INT )MSAA_WINDOW_SIZE - iWindowSize )
        m_iOffsetY = ( INT )MSAA_WINDOW_SIZE - iWindowSize;

    // Update world matrix
    m_matWorld = XMMatrixRotationY( ( FLOAT )m_Timer.GetAppTime() / 5.0f );

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the view-port, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Only some modes are possible on the alpha kit
    BOOL m_bResolveColor = TRUE;
    BOOL m_bResolveDepth = TRUE;

    // Draw to MSAA render target

    // Get back and depth-stencil
    D3DSurface* pFinalColorTarget;
    D3DSurface* pFinalDepthStencilTarget;
    m_pd3dDevice->GetRenderTarget( 0, &pFinalColorTarget );
    m_pd3dDevice->GetDepthStencilSurface( &pFinalDepthStencilTarget );

    // set render target and depth-stencil
    UINT dwMSAATargetIndex = 0;
    switch( m_dwCurrentMSAA )
    {
        case D3DMULTISAMPLE_NONE:
            dwMSAATargetIndex = 0; break;
        case D3DMULTISAMPLE_2_SAMPLES:
            dwMSAATargetIndex = 1; break;
        case D3DMULTISAMPLE_4_SAMPLES:
            dwMSAATargetIndex = 2; break;
    }
    m_pd3dDevice->SetRenderTarget( 0, m_pColorTarget[dwMSAATargetIndex] );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencilTarget[dwMSAATargetIndex] );

    // Clear them
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET0 | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                         0x00000000, 1.0f, 0L );

    // Draw quad
    D3DRECT rcMSAATarget = { 0, 0, MSAA_WINDOW_SIZE, MSAA_WINDOW_SIZE };
    DrawTetrahedron( &rcMSAATarget, &m_matWorld, m_fTetrahedronSize );

    // Resolve MSAA render targets
    if( m_bResolveColor )
    {
        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | m_dwCurrentColorResolve, NULL,
                               m_pColorTexture, NULL, 0, 0, NULL, 0.0f, 0x0, NULL );
    }

    // Draw depth
    if( m_bResolveDepth )
    {
        m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL | m_dwCurrentDepthStencilResolve, NULL,
                               m_pDepthStencilTexture, NULL, 0, 0, NULL, 0.0f, 0x0, NULL );
    }

    // Draw windows, grid, etc

    // Reset render and depth-stencil
    m_pd3dDevice->SetRenderTarget( 0, pFinalColorTarget );
    m_pd3dDevice->SetDepthStencilSurface( pFinalDepthStencilTarget );
    pFinalColorTarget->Release();
    pFinalDepthStencilTarget->Release();

    // Clear front buffer
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET0 | D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER,
                         0x00000000, 1.0f, 0L );

    // Reset the font window
    D3DRECT rcSavedFontWindow = m_Font.m_rcWindow;

    // Draw colored quad
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.SetWindow( m_rcColorWindow );
    m_Font.DrawText( ( m_rcColorWindow.x2 - m_rcColorWindow.x1 ) / 2.0f,
                     ( m_rcColorWindow.y2 - m_rcColorWindow.y1 ) + 10.0f,
                     0xff808080, L"color", ATGFONT_CENTER_X );
    m_Font.End();

    if( m_bResolveColor )
    {
        DrawScreenQuad( &m_rcColorWindow, ( FLOAT )m_iOffsetX, ( FLOAT )m_iOffsetY, ( FLOAT )m_iZoom,
                        m_pColorTexture );
    }

    // Draw reference tetrahedron
    if( m_bDrawWireframeTetrahedron )
    {
        DrawWireframeTetrahedron( &m_rcColorWindow, ( FLOAT )m_iOffsetX, ( FLOAT )m_iOffsetY, ( FLOAT )m_iZoom,
                                  &m_matWorld, m_fTetrahedronSize, &WHITECOLOR );
    }

    // Draw grid
    if( m_iZoom >= 16 )
    {
        DrawScreenGridAndSamplePoints( &m_rcColorWindow, ( FLOAT )m_iOffsetX, ( FLOAT )m_iOffsetY, ( FLOAT )m_iZoom,
                                       m_dwCurrentMSAA, &REDCOLOR, &GREENCOLOR );
    }
    DrawScreenQuad( &m_rcColorWindow, &WHITECOLOR );


    // Draw depth
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.SetWindow( m_rcDepthWindow );
    m_Font.DrawText( ( m_rcDepthWindow.x2 - m_rcDepthWindow.x1 ) / 2.0f,
                     ( m_rcDepthWindow.y2 - m_rcDepthWindow.y1 ) + 10.0f,
                     0xff808080, L"depth", ATGFONT_CENTER_X );
    m_Font.End();
    if( m_bResolveDepth )
    {
        DrawScreenQuad( &m_rcDepthWindow, ( FLOAT )m_iOffsetX, ( FLOAT )m_iOffsetY, ( FLOAT )m_iZoom,
                        m_pDepthStencilTexture );
    }
    DrawScreenQuad( &m_rcDepthWindow, &WHITECOLOR );

    // Restore the font window
    m_Font.m_rcWindow = rcSavedFontWindow;

    // Draw reference quads
    if( m_bResolveColor )
    {
        DrawScreenQuad( &m_rcReferenceWindow, 0.0f, 0.0f, 1.0f,
                        m_pColorTexture );
    }
    DrawScreenQuad( &m_rcReferenceWindow, &WHITECOLOR );

    // Draw zoom rectangle
    const UINT dwReferenceDiv = MSAA_WINDOW_SIZE / REFERENCE_WINDOW_SIZE;
    D3DRECT rcZoomQuad = m_rcReferenceWindow;
    rcZoomQuad.x1 += m_iOffsetX / dwReferenceDiv;
    rcZoomQuad.y1 += m_iOffsetY / dwReferenceDiv;
    rcZoomQuad.x2 = rcZoomQuad.x1 + ( MSAA_WINDOW_SIZE / m_iZoom ) / dwReferenceDiv;
    rcZoomQuad.y2 = rcZoomQuad.y1 + ( MSAA_WINDOW_SIZE / m_iZoom ) / dwReferenceDiv;
    DrawScreenQuad( &rcZoomQuad, &REDCOLOR );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"MSAAResolve" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.End();

        // Draw app state
        DrawState();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: MakeTetrahedron
// Desc: Makes the vertices for the tetrahedron
//--------------------------------------------------------------------------------------
VOID Sample::MakeTetrahedronVerts( FLOAT fSize, ScreenspaceDiffuseVert* pVerts )
{
    // Create vertices and indices
    FLOAT fLen = fSize / 2.0f;
    ScreenspaceDiffuseVert Verts[4] =
    {
        { XMFLOAT4( -fLen, fLen, fLen, 1.0f ), 0xff0000ff },
        { XMFLOAT4( fLen, fLen, -fLen, 1.0f ), 0xff0000ff },
        { XMFLOAT4( -fLen, -fLen, -fLen, 1.0f ), 0xff0000ff },
        { XMFLOAT4( fLen, -fLen, fLen, 1.0f ), 0xff0000ff },
    };
    WORD Indices[4 * 3] =
    {
        0, 1, 2,
        0, 3, 1,
        2, 1, 3,
        0, 2, 3,
    };

    for( DWORD i = 0; i < 4; i++ )
    {
        for( DWORD j = 0; j < 3; j++ )
        {
            pVerts[3 * i + j] = Verts[Indices[3 * i + j]];
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: PixelOffsetClipspacePosition
// Desc: Offsets a clip space position by n pixels x and y
//--------------------------------------------------------------------------------------
VOID PixelOffsetClipspacePosition( const D3DRECT* pWindow, XMFLOAT4* pPos,
                                   FLOAT fOffsetX, FLOAT fOffsetY )
{
    pPos->x += fOffsetX * 2.0f * pPos->w / ( pWindow->x2 - pWindow->x1 );
    pPos->y += -fOffsetY * 2.0f * pPos->w / ( pWindow->y2 - pWindow->y1 );
}


//--------------------------------------------------------------------------------------
// Name: PixelScaleClipspaceScale
// Desc: Scales a clip space position by n pixels
//--------------------------------------------------------------------------------------
VOID PixelScaleClipspacePosition( XMFLOAT4* pPos, FLOAT fScale )
{
    pPos->x *= ( -pPos->w + fScale * ( pPos->x + pPos->w ) ) / pPos->x;
    pPos->y *= ( pPos->w + fScale * ( pPos->y - pPos->w ) ) / pPos->y;
}


//--------------------------------------------------------------------------------------
// Name: DrawTetrahedron
// Desc: Draws a Tetrahedron
//--------------------------------------------------------------------------------------
HRESULT Sample::DrawTetrahedron( const D3DRECT* pWindow,
                                 const XMMATRIX* pWorld, const FLOAT fSize )
{
    // Set D3D state
    m_pd3dDevice->SetPixelShader( m_pPSDiffuse );
    m_pd3dDevice->SetVertexShader( m_pVSScreenspaceDiffuse );
    m_pd3dDevice->SetVertexDeclaration( m_pVDScreenspaceDiffuse );

    // Create vertices and indices
    ScreenspaceDiffuseVert Verts[NUM_TETRAHEDRON_VERTS];
    MakeTetrahedronVerts( fSize, Verts );

    // Transform them
    XMMATRIX matWVP = m_matWorld * m_matView * m_matProj;
    for( DWORD i = 0; i < NUM_TETRAHEDRON_VERTS; i++ )
    {
        XMStoreFloat4( &Verts[i].Pos, XMVector4Transform( XMLoadFloat4( &Verts[i].Pos ), matWVP ) );

        // Offset by half pixel (-.5, -.5) in clip space
        if( m_bMatchPixelAndTexelCenter )
            PixelOffsetClipspacePosition( pWindow, &Verts[i].Pos, -0.5f, -0.5f );
    }

    // Draw
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLELIST, NUM_TETRAHEDRON_FACES,
                                   Verts, sizeof( Verts[0] ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DrawWireframeTetrahedron
// Desc: Draws an wire-frame version of the tetrahedron
//--------------------------------------------------------------------------------------
HRESULT Sample::DrawWireframeTetrahedron( const D3DRECT* pWindow,
                                          FLOAT fOffsetX, FLOAT fOffsetY, FLOAT fZoom,
                                          const XMMATRIX* pWorld, const FLOAT fSize,
                                          const XMFLOAT4* pColor )
{
    // Set D3D state
    m_pd3dDevice->SetPixelShader( m_pPSConstantColor );
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )pColor, 1 );
    m_pd3dDevice->SetVertexShader( m_pVSScreenspace );
    m_pd3dDevice->SetVertexDeclaration( m_pVDScreenspaceDiffuse );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );

    D3DVIEWPORT9 vpOldViewport;
    m_pd3dDevice->GetViewport( &vpOldViewport );
    D3DVIEWPORT9 vpNewViewport;
    vpNewViewport.X = pWindow->x1;
    vpNewViewport.Y = pWindow->y1;
    vpNewViewport.Width = pWindow->x2 - pWindow->x1;
    vpNewViewport.Height = pWindow->y2 - pWindow->y1;
    vpNewViewport.MinZ = 0.0f;
    vpNewViewport.MaxZ = 1.0f;
    m_pd3dDevice->SetViewport( &vpNewViewport );

    ScreenspaceDiffuseVert Verts[NUM_TETRAHEDRON_VERTS];
    MakeTetrahedronVerts( fSize, Verts );

    // Transform vertices to screen space, offset them to window, and draw
    XMMATRIX matWVP = m_matWorld * m_matView * m_matProj;
    for( DWORD i = 0; i < NUM_TETRAHEDRON_VERTS; i++ )
    {
        XMStoreFloat4( &Verts[i].Pos, XMVector4Transform( XMLoadFloat4( &Verts[i].Pos ), matWVP ) );

        if( !m_bMatchPixelAndTexelCenter )
        {
            // offset by half pixel (+.5, +.5) in clip space
            PixelOffsetClipspacePosition( pWindow, &Verts[i].Pos, 0.5f, 0.5f );
        }

        PixelOffsetClipspacePosition( pWindow, &Verts[i].Pos, -fOffsetX, -fOffsetY );
        PixelScaleClipspacePosition( &Verts[i].Pos, fZoom );
    }
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLELIST, NUM_TETRAHEDRON_FACES,
                                   Verts, sizeof( Verts[0] ) );

    // Reset render state
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    m_pd3dDevice->SetViewport( &vpOldViewport );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DrawScreenQuad
// Desc: Draws a quad with a specific texture in screen space
//--------------------------------------------------------------------------------------
HRESULT Sample::DrawScreenQuad( const D3DRECT* pWindow,
                                FLOAT fOffsetX, FLOAT fOffsetY, FLOAT fZoom,
                                D3DTexture* pTexture )
{
    // Set D3D state
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    m_pd3dDevice->SetPixelShader( m_pPSTexture );
    m_pd3dDevice->SetVertexShader( m_pVSScreenspaceTexcoord );
    m_pd3dDevice->SetVertexDeclaration( m_pVDScreenspaceTexcoord );
    m_pd3dDevice->SetTexture( 0, pTexture );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    // Create vertices and draw

    // Texture offsets and zoom
    FLOAT fTextureWidth = 1.0f / fZoom;
    FLOAT fTextureOffsetX = fOffsetX / MSAA_WINDOW_SIZE;
    FLOAT fTextureOffsetY = fOffsetY / MSAA_WINDOW_SIZE;
    ScreenspaceTexcoordVert Verts[4] =
    {
        { XMFLOAT4( FLOAT( pWindow->x1 ), FLOAT( pWindow->y1 ), 0.0f, 1.0f ),
            XMFLOAT2( fTextureOffsetX, fTextureOffsetY ) },
        { XMFLOAT4( FLOAT( pWindow->x2 ), FLOAT( pWindow->y1 ), 0.0f, 1.0f ),
            XMFLOAT2( fTextureOffsetX + fTextureWidth, fTextureOffsetY ) },
        { XMFLOAT4( FLOAT( pWindow->x2 ), FLOAT( pWindow->y2 ), 0.0f, 1.0f ),
            XMFLOAT2( fTextureOffsetX + fTextureWidth, fTextureOffsetY + fTextureWidth ) },
        { XMFLOAT4( FLOAT( pWindow->x1 ), FLOAT( pWindow->y2 ), 0.0f, 1.0f ),
            XMFLOAT2( fTextureOffsetX, fTextureOffsetY + fTextureWidth ) },
    };
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, Verts, sizeof( Verts[0] ) );

    // Reset D3D State
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DrawColoredScreenQuadGrid
// Desc: Draws a grid with a specific color in screen space
//--------------------------------------------------------------------------------------
HRESULT Sample::DrawScreenGridAndSamplePoints( const D3DRECT* pWindow,
                                               FLOAT fOffsetX, FLOAT fOffsetY,
                                               FLOAT fZoom,
                                               DWORD dwMSAA,
                                               const XMFLOAT4* pGridColor,
                                               const XMFLOAT4* pSamplePointColor )
{
    // Set D3D state
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    m_pd3dDevice->SetPixelShader( m_pPSConstantColor );
    m_pd3dDevice->SetVertexShader( m_pVSScreenspace );
    m_pd3dDevice->SetVertexDeclaration( m_pVDScreenspace );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    // Clip to window
    RECT rcOldRect;
    m_pd3dDevice->GetScissorRect( &rcOldRect );
    RECT rcNewRect;
    rcNewRect.left = pWindow->x1 - 1;
    rcNewRect.top = pWindow->y1 - 1;
    rcNewRect.right = pWindow->x2 + 1;
    rcNewRect.bottom = pWindow->y2 + 1;
    m_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, TRUE );
    m_pd3dDevice->SetScissorRect( &rcNewRect );

    // Grid delta, start and end
    FLOAT fDeltaX = fZoom;
    FLOAT fDeltaY = fZoom;

    FLOAT fStartX = ( FLOAT )pWindow->x1;
    FLOAT fStartY = ( FLOAT )pWindow->y1;

    UINT dwTotalX = ( UINT )( ( pWindow->x2 - pWindow->x1 ) / fDeltaX );
    UINT dwTotalY = ( UINT )( ( pWindow->y2 - pWindow->y1 ) / fDeltaY );

    // Draw grid
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )pGridColor, 1 );
    for( DWORD i = 0; i <= dwTotalX; i++ )
    {
        ScreenspaceVert Verts[2] =
        {
            XMFLOAT4( fStartX + i * fDeltaX, fStartY, 0.0f, 1.0f ),
            XMFLOAT4( fStartX + i * fDeltaX, fStartY + fDeltaY * dwTotalY, 0.0f, 1.0f ),
        };
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINELIST, 1, Verts, sizeof( Verts[0] ) );
    }

    for( DWORD i = 0; i <= dwTotalY; i++ )
    {
        ScreenspaceVert Verts[2] =
        {
            XMFLOAT4( fStartX, fStartY + i * fDeltaY, 0.0f, 1.0f ),
            XMFLOAT4( fStartX + fDeltaX * dwTotalX, fStartY + i * fDeltaY, 0.0f, 1.0f ),
        };
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINELIST, 1, Verts, sizeof( Verts[0] ) );
    }

    // Draw sample points
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )pSamplePointColor, 1 );
    FLOAT fPointSize = 2.0f;
    m_pd3dDevice->SetRenderState( D3DRS_POINTSIZE, *( ( DWORD* )&fPointSize ) );
    for( DWORD i = 0; i <= dwTotalX; i++ )
    {
        for( DWORD j = 0; j <= dwTotalY; j++ )
        {
            ScreenspaceVert CenterVert =
            {
                XMFLOAT4( fStartX + i * fDeltaX, fStartY + j * fDeltaY, 0.0f, 1.0f ),
            };

            // Sample point in the center of the pixel
            CenterVert.Pos.x += fDeltaX / 2.0f;
            CenterVert.Pos.y += fDeltaY / 2.0f;

            if( dwMSAA == D3DMULTISAMPLE_NONE )
            {
                m_pd3dDevice->DrawPrimitiveUP( D3DPT_POINTLIST, 1, &CenterVert, sizeof( CenterVert ) );
            }
            if( dwMSAA == D3DMULTISAMPLE_2_SAMPLES )
            {
                ScreenspaceVert MSAA_2X_Samples[2] =
                {
                    XMFLOAT4( CenterVert.Pos.x + fDeltaX * MSAA_2X_SAMPLE0.x,
                              CenterVert.Pos.y + fDeltaY * MSAA_2X_SAMPLE0.y,
                              0.0f, 1.0f ),
                    XMFLOAT4( CenterVert.Pos.x + fDeltaX * MSAA_2X_SAMPLE1.x,
                              CenterVert.Pos.y + fDeltaY * MSAA_2X_SAMPLE1.y,
                              0.0f, 1.0f ),
                };
                m_pd3dDevice->DrawPrimitiveUP( D3DPT_POINTLIST, 2, MSAA_2X_Samples,
                                               sizeof( MSAA_2X_Samples[0] ) );
            }
            else if( dwMSAA == D3DMULTISAMPLE_4_SAMPLES )
            {
                ScreenspaceVert MSAA_4X_Samples[4] =
                {
                    XMFLOAT4( CenterVert.Pos.x + fDeltaX * MSAA_4X_SAMPLE0.x,
                              CenterVert.Pos.y + fDeltaY * MSAA_4X_SAMPLE0.y,
                              0.0f, 1.0f ),
                    XMFLOAT4( CenterVert.Pos.x + fDeltaX * MSAA_4X_SAMPLE1.x,
                              CenterVert.Pos.y + fDeltaY * MSAA_4X_SAMPLE1.y,
                              0.0f, 1.0f ),
                    XMFLOAT4( CenterVert.Pos.x + fDeltaX * MSAA_4X_SAMPLE2.x,
                              CenterVert.Pos.y + fDeltaY * MSAA_4X_SAMPLE2.y,
                              0.0f, 1.0f ),
                    XMFLOAT4( CenterVert.Pos.x + fDeltaX * MSAA_4X_SAMPLE3.x,
                              CenterVert.Pos.y + fDeltaY * MSAA_4X_SAMPLE3.y,
                              0.0f, 1.0f ),
                };
                m_pd3dDevice->DrawPrimitiveUP( D3DPT_POINTLIST, 4, MSAA_4X_Samples,
                                               sizeof( MSAA_4X_Samples[0] ) );
            }
        }
    }

    // Reset D3D state
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE );
    m_pd3dDevice->SetScissorRect( &rcNewRect );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );


    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DrawScreenQuad
// Desc: Draws a quad with a specific color in screen space
//--------------------------------------------------------------------------------------
HRESULT Sample::DrawScreenQuad( const D3DRECT* pWindow,
                                const XMFLOAT4* pColor )
{

    //
    // set D3D state
    //
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    m_pd3dDevice->SetPixelShader( m_pPSConstantColor );
    m_pd3dDevice->SetVertexShader( m_pVSScreenspace );
    m_pd3dDevice->SetVertexDeclaration( m_pVDScreenspace );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )pColor, 1 );

    //
    // create vertices and draw
    //
    ScreenspaceVert Verts[5] =
    {
        XMFLOAT4( FLOAT( pWindow->x1 ), FLOAT( pWindow->y1 ), 0.0f, 1.0f ),
        XMFLOAT4( FLOAT( pWindow->x2 ), FLOAT( pWindow->y1 ), 0.0f, 1.0f ),
        XMFLOAT4( FLOAT( pWindow->x2 ), FLOAT( pWindow->y2 ), 0.0f, 1.0f ),
        XMFLOAT4( FLOAT( pWindow->x1 ), FLOAT( pWindow->y2 ), 0.0f, 1.0f ),
        XMFLOAT4( FLOAT( pWindow->x1 ), FLOAT( pWindow->y1 ), 0.0f, 1.0f ),
    };
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINESTRIP, 4, Verts, sizeof( Verts[0] ) );

    //
    // reset D3D state
    //
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );


    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DrawStats
// Desc: Draws sample state
//--------------------------------------------------------------------------------------
HRESULT Sample::DrawState()
{
    WCHAR strBuf[128];
    const WCHAR* strMSAA = NULL;
    const WCHAR* strColorResolve = NULL;
    const WCHAR* strDepthResolve = NULL;

    switch( m_dwCurrentMSAA )
    {
        case D3DMULTISAMPLE_NONE:
            strMSAA = L"None"; break;
        case D3DMULTISAMPLE_2_SAMPLES:
            strMSAA = L"2X";   break;
        case D3DMULTISAMPLE_4_SAMPLES:
            strMSAA = L"4X";   break;
    }
    switch( m_dwCurrentColorResolve )
    {
        case D3DRESOLVE_ALLFRAGMENTS:
            strColorResolve = L"All"; break;
        case D3DRESOLVE_FRAGMENT0:
            strColorResolve = L"Fragment 0"; break;
        case D3DRESOLVE_FRAGMENT1:
            strColorResolve = L"Fragment 1"; break;
        case D3DRESOLVE_FRAGMENT2:
            strColorResolve = L"Fragment 2"; break;
        case D3DRESOLVE_FRAGMENT3:
            strColorResolve = L"Fragment 3"; break;
        case D3DRESOLVE_FRAGMENTS01:
            strColorResolve = L"Fragments 0 and 1"; break;
        case D3DRESOLVE_FRAGMENTS23:
            strColorResolve = L"Fragments 2 and 3"; break;
    }

    switch( m_dwCurrentDepthStencilResolve )
    {
        case D3DRESOLVE_FRAGMENT0:
            strDepthResolve = L"Fragment 0"; break;
        case D3DRESOLVE_FRAGMENT1:
            strDepthResolve = L"Fragment 1"; break;
        case D3DRESOLVE_FRAGMENT2:
            strDepthResolve = L"Fragment 2"; break;
        case D3DRESOLVE_FRAGMENT3:
            strDepthResolve = L"Fragment 3"; break;
    }

    m_Font.Begin();
    m_Font.SetScaleFactors( 0.8f, 0.8f );

    m_Font.DrawText( 15, 35, 0xffffffff, L"MSAA: " );
    m_Font.DrawText( 0xffffff00, strMSAA );

    m_Font.DrawText( 15, 55, 0xffffffff, L"Color Resolve: " );
    m_Font.DrawText( 0xffffff00, strColorResolve );

    m_Font.DrawText( 15, 75, 0xffffffff, L"Depth Resolve: " );
    m_Font.DrawText( 0xffffff00, strDepthResolve );

    swprintf_s( strBuf, L"%dX", m_iZoom );
    m_Font.DrawText( 315, 35, 0xffffffff, L"Zoom: " );
    m_Font.DrawText( 0xffffff00, strBuf );

    swprintf_s( strBuf, L"(%d,%d)", m_iOffsetX, m_iOffsetY );
    m_Font.DrawText( 315, 55, 0xffffffff, L"Offset: " );
    m_Font.DrawText( 0xffffff00, strBuf );

    swprintf_s( strBuf, L"%s", m_bMatchPixelAndTexelCenter ? L"(.5,.5)":L"(0,0)" );
    m_Font.DrawText( 315, 75, 0xffffffff, L"Pixel Center: " );
    m_Font.DrawText( 0xffffff00, strBuf );

    m_Font.SetScaleFactors( 1.0f, 1.0f );

    m_Font.End();

    return S_OK;
}

