//--------------------------------------------------------------------------------------
// VarianceShadowMaps.cpp
//
// Shadow mapping sample comparing variance shadow maps to traditional shadow maps.
//
// Variance shadow mapping is a technique that uses mathematical variance to determine
// the maximum probability that a pixel is in shadow. The variance is a function of
// depth and depth-squared, so variance shadow maps have two components (depth and 
// depth-squared) instead of just one (depth).
//
// The real advantage of variance shadow maps is that filtering depth is permissable,
// meaning, for example, a 5x5 Gaussian blur provides a nice softening of shadow edges.
// Furthermore, trilinear and aniosotripic fetching may be used.
//
// In the pixel shader for the final scene, using a variance shadow map offers good
// performance since it's just one (filtered) texture fetch followed by a handful of
// ALU ops. Compared to PCF, which requires many texture fetches and ALU ops, variance
// shadow maps are faster (and look better, too!).

// The steps which build and blur the variance shadow map account for most of the
// performance difference from using a conventional shadow map. Therfore, it is crucial
// to experiment with various blurring algorithms. We use a two-pass separable 5x5
// Gaussian blur for nice soft edges.
//
// The biggest performance improvement will be to shrink the shadow map dimensions.
// For example, a 512x512 variance shadow map can look much better than a 1024x1024
// conventional shadow map.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <d3d9.h>
#include <xgraphics.h>
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgMesh.h"
#include "AtgPostProcess.h"
#include "AtgResource.h"
#include "AtgUtil.h"
#include "ChessSet.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BOTTOM_LEFT,    ATG::HELP_PLACEMENT_2, L"Use triggers to adjust VSM epsilon" },
    { ATG::HELP_LEFTSTICK,      ATG::HELP_PLACEMENT_2, L"Move\ncamera" },
    { ATG::HELP_RIGHTSTICK,     ATG::HELP_PLACEMENT_2, L"Rotate\ncamera" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_2, L"Move\nlight" },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_2, L"Change shadow\nmethod" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_2, L"Cycle shadow\nmap size" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle VSM\nblurring" },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle\ndebug mode" },
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_LEFT_SHOULDER,  ATG::HELP_PLACEMENT_2, L"Cycle\nfiltering" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Cycle\nfiltering" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Access to set custom mesh properties prior to rendering
//--------------------------------------------------------------------------------------
XMMATRIX            CustomMesh::g_matCameraViewProj;   // Global view*projection matrix
XMMATRIX            CustomMesh::g_matShadowViewProj;   // Global shadow view*projection matrix
XMVECTOR            CustomMesh::g_vLightDirection;     // Glogal light direction
XMVECTOR            CustomMesh::g_vViewPosition;       // Global viewer position

enum FILTER_OPTIONS
{
    FILTER_POINT,
    FILTER_BILINEAR,
    FILTER_TRILINEAR,
    FILTER_ANISOTROPIC,
    FILTER_MAX,
    FILTER_FORCEDWORD = 0xffffffff,
};

static WCHAR*       g_FilteringNames[FILTER_MAX] =
{
    L"Point",               //    FILTER_POINT, 
    L"Bilinear",            //    FILTER_BILINEAR, 
    L"Trilinear",           //    FILTER_TRILINEAR, 
    L"Aniso",               //    FILTER_ANISOTROPIC, 
};

//--------------------------------------------------------------------------------------
// Name: class CAtgSample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    ATG::PackedResource m_xprResource;              // Packed resources (textures)
    ATG::PostProcess m_PostProcess;

    ChessBoard m_ChessBoard;               // A scene to render

    XMMATRIX m_matCameraView;            // Camera's view matrix
    XMMATRIX m_matCameraProj;            // Camera's projection matrix

    XMMATRIX m_matLightView;             // Light's view matrix
    XMMATRIX m_matLightProj;             // Light's projection matrix

    XMVECTOR m_vEyePt;                   // View parameters
    XMVECTOR m_vLookAtPt;
    XMVECTOR m_vUpVec;

    XMVECTOR m_vLightDirection;          // Light direction vector

    DWORD m_dwShadowMapSize;          // Dimension of shadow map
    IDirect3DTexture9* m_pShadowMap;               // Depth texture for the conventional shadow map
    IDirect3DTexture9* m_pVarianceShadowMap;       // Depth texture for the variance shadow map
    IDirect3DSurface9* m_pShadowMapDepthSurface;   // Depth-stencil render surface for the shadow map

    BOOL m_bUseVarianceShadowMap;
    BOOL m_bBlurVarianceMap;
    DWORD m_dwFiltering;
    FLOAT m_fEpsilonVSM;

    BOOL m_bDebugShowShadow;

    IDirect3DVertexShader9* m_pDepthOnlyVS;
    IDirect3DPixelShader9* m_pCopyDepthToVariancePS;
    IDirect3DPixelShader9* m_pHorizontalBlurDepthToVariancePS;
    IDirect3DPixelShader9* m_pVerticalBlurDepthToVariancePS;
    IDirect3DVertexShader9* m_pLightWithShadowsVS;
    IDirect3DPixelShader9* m_pLightWithShadowsPS;

    HRESULT         CreateShadowMaps();

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

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    m_bDrawHelp = FALSE;

    m_bUseVarianceShadowMap = TRUE;
    m_bBlurVarianceMap = TRUE;
    m_dwFiltering = FILTER_TRILINEAR;
    m_fEpsilonVSM = 0.001f;

    m_bDebugShowShadow = FALSE;

    m_dwShadowMapSize = 1024;
    m_pShadowMap = NULL;
    m_pVarianceShadowMap = NULL;
    m_pShadowMapDepthSurface = NULL;

    // Set the view parameters
    m_vEyePt = XMVectorSet( 0.0f, 0.280240387f, -0.558691025f, 1.0f );
    m_vLookAtPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );
    m_vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

    // Set the light direction
    m_vLightDirection = XMVectorSet( 0.604104698f, 0.531634569f, -0.585469127f, 0.0f );

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the resources
    if( FAILED( m_xprResource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize the post-processing effects library (for blur, bloom, etc.)
    if( FAILED( m_PostProcess.Initialize() ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the chess board
    if( FAILED( m_ChessBoard.Create( &m_xprResource ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Build the instanced set of chess pieces
    m_ChessBoard.AddChessPiece( ChessPiece::BLACK, ChessPiece::PAWN, 1, 7 );
    m_ChessBoard.AddChessPiece( ChessPiece::BLACK, ChessPiece::PAWN, 2, 7 );
    m_ChessBoard.AddChessPiece( ChessPiece::BLACK, ChessPiece::PAWN, 4, 4 );
    m_ChessBoard.AddChessPiece( ChessPiece::BLACK, ChessPiece::PAWN, 7, 5 );
    m_ChessBoard.AddChessPiece( ChessPiece::BLACK, ChessPiece::PAWN, 8, 5 );
    m_ChessBoard.AddChessPiece( ChessPiece::BLACK, ChessPiece::QUEEN, 3, 1 );
    m_ChessBoard.AddChessPiece( ChessPiece::BLACK, ChessPiece::KING, 8, 6 );
    m_ChessBoard.AddChessPiece( ChessPiece::WHITE, ChessPiece::PAWN, 6, 2 );
    m_ChessBoard.AddChessPiece( ChessPiece::WHITE, ChessPiece::PAWN, 7, 3 );
    m_ChessBoard.AddChessPiece( ChessPiece::WHITE, ChessPiece::PAWN, 8, 2 );
    m_ChessBoard.AddChessPiece( ChessPiece::WHITE, ChessPiece::BISHOP, 4, 5 );
    m_ChessBoard.AddChessPiece( ChessPiece::WHITE, ChessPiece::QUEEN, 6, 7 );
    m_ChessBoard.AddChessPiece( ChessPiece::WHITE, ChessPiece::KING, 7, 2 );

    // Load the vertex and pixel shaders.
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\DepthOnly.xvu",
                                       &m_pDepthOnlyVS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\CopyDepthToVariance.xpu",
                                      &m_pCopyDepthToVariancePS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\HorizontalBlurDepthToVariance.xpu",
                                      &m_pHorizontalBlurDepthToVariancePS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\VerticalBlurDepthToVariance.xpu",
                                      &m_pVerticalBlurDepthToVariancePS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\LightWithShadows.xvu",
                                       &m_pLightWithShadowsVS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\LightWithShadows.xpu",
                                      &m_pLightWithShadowsPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the shadow map textures and render surface
    if( FAILED( hr = CreateShadowMaps() ) )
        return E_FAIL;

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the projection matrix
    m_matCameraProj = XMMatrixPerspectiveFovLH( D3DX_PI / 4, fAspectRatio, 0.02f, 5.0f );

    // Set the view matrix
    m_matCameraView = XMMatrixLookAtLH( m_vEyePt, m_vLookAtPt, m_vUpVec );

    // Adjust fill convention so that pixel centers are at (0.5, 0.5)
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateShadowMaps
// Desc: Create textures for the conventional shadow map and the variance shadow map
//--------------------------------------------------------------------------------------
HRESULT Sample::CreateShadowMaps()
{
    // Make sure the GPU is not using any surfaces we're about to release
    m_pd3dDevice->BlockUntilIdle();

    // Note: The GPU cannot resolve to packed mip levels. Therefore, we need to either
    // (1) Create the texture manually with the XGHEADEREX_NONPACKED flag, or
    // (2) Make sure the smallest mip level we use is at least 32x32 in size

    // Create the variance shadow map, with a mip chain
    {
        if( m_pVarianceShadowMap )
            m_pVarianceShadowMap->Release();

        // Compute the number of mip levels from the max shadow size down to 32x32
        DWORD dwNumMipLevels = 1 + ( DWORD )( logf( m_dwShadowMapSize / 32.0f ) / logf( 2.0f ) );

        // Create the variance shadow map (that holds depth and depth-squared) with a mip chain
        if( FAILED( m_pd3dDevice->CreateTexture( m_dwShadowMapSize, m_dwShadowMapSize, dwNumMipLevels,
                                                 0, ATG::D3DFMT_G16R16_SIGNED_INTEGER, D3DPOOL_DEFAULT,
                                                 &m_pVarianceShadowMap, NULL ) ) )
            return E_FAIL;
        m_pVarianceShadowMap->Format.ExpAdjust = -15;   // convert int to [-1,1]
    }

    // Note: for the conventional shadow map, we can either create it normally, with it's own
    // separate memory, or we can use XGSetTextureHeader to have it share memory with the
    // variance shadow map

    // Create the conventional shadow map, sharing memory with the variance shadow map
    {
        if( NULL == m_pShadowMap )
            m_pShadowMap = new IDirect3DTexture9;
        XGSetTextureHeaderEx( m_dwShadowMapSize, m_dwShadowMapSize, 1,
                              0, D3DFMT_D24S8, 0, 0, 0, 0,
                              0, m_pShadowMap, NULL, NULL );
        XGOffsetResourceAddress( m_pShadowMap, ( VOID* )( m_pVarianceShadowMap->Format.BaseAddress <<
                                                          GPU_TEXTURE_ADDRESS_SHIFT ) );
    }

    // Create a depth-stencil target surface for rendering into the conventional shadow map
    {
        if( m_pShadowMapDepthSurface )
            m_pShadowMapDepthSurface->Release();

        D3DSURFACE_PARAMETERS SurfaceParams = { 0 };
        if( FAILED( m_pd3dDevice->CreateDepthStencilSurface( m_dwShadowMapSize, m_dwShadowMapSize, D3DFMT_D24S8,
                                                             D3DMULTISAMPLE_NONE, 0, FALSE,
                                                             &m_pShadowMapDepthSurface, &SurfaceParams ) ) )
            return E_FAIL;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the elapsed time for the last frame.
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Change shadowing method
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_bUseVarianceShadowMap = !m_bUseVarianceShadowMap;

    // Switch filtering level
    if( m_bUseVarianceShadowMap )
    {
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
            m_dwFiltering = ( m_dwFiltering == 0 ) ? ( FILTER_MAX - 1 ) : ( m_dwFiltering - 1 );
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
            m_dwFiltering = ( m_dwFiltering == ( FILTER_MAX - 1 ) ) ? 0 : ( m_dwFiltering + 1 );
    }

    // Toggle the blurring of the variance shadow map
    if( m_bUseVarianceShadowMap )
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
            m_bBlurVarianceMap = !m_bBlurVarianceMap;

    // Toggle a debug mode to view the isolated shadow
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        m_bDebugShowShadow = !m_bDebugShowShadow;

    // Cycle through shadow map sizes
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        if( m_dwShadowMapSize == 256 ) m_dwShadowMapSize = 512;
        else if( m_dwShadowMapSize == 512 ) m_dwShadowMapSize = 1024;
        else if( m_dwShadowMapSize == 1024 ) m_dwShadowMapSize = 256;

        // Re-create the shadow map textures and render surface
        CreateShadowMaps();
    }

    // Adjust the epsilon used in the VSM algorithm
    if( m_bUseVarianceShadowMap )
    {
        m_fEpsilonVSM *= powf( 2.0f, -fElapsedTime * pGamepad->bLeftTrigger / 255.0f );
        m_fEpsilonVSM *= powf( 2.0f, +fElapsedTime * pGamepad->bRightTrigger / 255.0f );
    }

    // Set the view matrix
    static FLOAT fTheta = -0.15f * XM_PI;
    static FLOAT fPhi = +0.00f * XM_PI;

    fPhi += pGamepad->fX2 * fElapsedTime * 0.3f * XM_PI;
    fTheta += pGamepad->fY2 * fElapsedTime * 0.3f * XM_PI;

    XMVECTOR vLookatDir;
    vLookatDir.x = cosf( fTheta ) * sinf( fPhi );
    vLookatDir.y = sinf( fTheta );
    vLookatDir.z = cosf( fTheta ) * cosf( fPhi );

    XMVECTOR vCrossDir;
    vCrossDir.x = +cosf( fPhi );
    vCrossDir.y = 0.0;
    vCrossDir.z = -sinf( fPhi );

    m_vEyePt += vLookatDir * pGamepad->fY1 * fElapsedTime * 4 * m_ChessBoard.GetGridSize();
    m_vEyePt += vCrossDir * pGamepad->fX1 * fElapsedTime * 4 * m_ChessBoard.GetGridSize();
    m_matCameraView = XMMatrixLookAtLH( m_vEyePt, m_vEyePt + vLookatDir, m_vUpVec );

    // Rotate the light direction based on the dpad
    {
        FLOAT fMoveLightX = ( pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT ) ? -1.0f :
            ( pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ) ? 1.0f : 0.0f;
        FLOAT fMoveLightY = ( pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN ) ? -1.0f :
            ( pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP ) ? 1.0f : 0.0f;
        XMMATRIX matRotate = XMMatrixRotationAxis( m_vUpVec, fMoveLightX * 2.0f * fElapsedTime );
        m_vLightDirection = XMVector3TransformNormal( m_vLightDirection, matRotate );

        // Place limits so we don't go over the top or under the bottom
        FLOAT dot = XMVector3Dot( m_vLightDirection, m_vUpVec ).x;
        if( ( dot > 0.1f || fMoveLightY > 0.0f ) && ( dot < 0.9f || fMoveLightY < 0.0f ) )
        {
            XMVECTOR vAxis = XMVector3Cross( m_vLightDirection, m_vUpVec );

            matRotate = XMMatrixRotationAxis( vAxis, fMoveLightY * fElapsedTime );
            m_vLightDirection = XMVector3TransformNormal( m_vLightDirection, matRotate );
        }
    }

    //----------------------------------------------------------------------------------
    // Compute the light view and light projection matrices.
    //----------------------------------------------------------------------------------
    {
        FLOAT fBoardSize = m_ChessBoard.GetGridSize() * 8.0f;
        FLOAT fRadius = sqrt( ( fBoardSize * 0.5f ) * ( fBoardSize * 0.5f ) * 2.0f );

        float fNear = 0.1f;
        float fFar = fNear + fRadius * 2.0f;

        // Light orientation (looks at zero)
        XMVECTOR vFrom = m_vLightDirection * ( fRadius + fNear );
        XMVECTOR vTo = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );
        XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0001f, 0.0f );
        m_matLightView = XMMatrixLookAtLH( vFrom, vTo, vUp );

        // Include the entire world in the orthographic projection.
        FLOAT fWidth = fRadius * 2.0f;
        FLOAT fHeight = fRadius * 2.0f;
        m_matLightProj = XMMatrixOrthographicLH( fWidth, fHeight, fNear, fFar );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    //----------------------------------------------------------------------------------
    // Draw the shadow map
    //----------------------------------------------------------------------------------
    {
        // Save the back buffer, and z-buffer
        IDirect3DSurface9* pBackBufferSurface;
        IDirect3DSurface9* pZBufferSurface;
        m_pd3dDevice->GetRenderTarget( 0, &pBackBufferSurface );
        m_pd3dDevice->GetDepthStencilSurface( &pZBufferSurface );

        // Initialize some render states
        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
        m_pd3dDevice->SetRenderState( D3DRS_DEPTHBIAS, ATG::FtoDW( 0.001f ) );
        m_pd3dDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, ATG::FtoDW( 2.0f ) );

        m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0 );
        m_pd3dDevice->SetRenderTarget( 0, NULL );
        m_pd3dDevice->SetDepthStencilSurface( m_pShadowMapDepthSurface );

        // Set the vertex shader to our depth only shader
        m_pd3dDevice->SetVertexShader( m_pDepthOnlyVS );
        m_pd3dDevice->SetPixelShader( NULL );

        m_pd3dDevice->ClearF( D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, NULL, NULL, 1.0f, 0L );

        // Render the scene into the shadow map
        // Note: the VSM algorithm requires that both shadow casters and shadow recievers be rendered!
        {
            m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

            // Set the mesh camera to the light's point-of-view
            CustomMesh::g_matCameraViewProj = m_matLightView * m_matLightProj;

            // Render the scene
            m_ChessBoard.RenderPieces( XMMatrixIdentity() );
            m_ChessBoard.Render( XMMatrixIdentity() );

            // Resolve depth to our texture
            m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, m_pShadowMap,
                                   NULL, 0, 0, NULL, 1.0f, 0, NULL );
        }

        // Restore states
        m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
        m_pd3dDevice->SetRenderState( D3DRS_DEPTHBIAS, 0 );
        m_pd3dDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, 0 );
        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

        // Restore the main render/depth targets
        m_pd3dDevice->SetRenderTarget( 0, pBackBufferSurface );
        m_pd3dDevice->SetDepthStencilSurface( pZBufferSurface );
        pBackBufferSurface->Release();
        pZBufferSurface->Release();
    }

    //----------------------------------------------------------------------------------
    // Build the variance shadow map (and it's mip-maps)
    //----------------------------------------------------------------------------------
    if( m_bUseVarianceShadowMap )
    {
        // Perf note: The steps here, which build and blur the variance shadow map, 
        // account for most of the performance difference from using a conventional
        // shadow map. However, please do experiment with how a 512x512 variance
        // shadow map compares visually well compared to a 1024x1024 conventional
        // shadow map.

        // Blur the variance shadow map using a 2-pass separable 5x5 Gaussian blur
        if( m_bBlurVarianceMap )
        {
            // Convert the D24S8 shadow map to the 2-channel (depth and depth-squared) VSM
            // and blur the results. We can combine the copy and blur operations in the
            // pixel shaders, as long as we compute depth and depth-squared before any
            // filtering.
            // Perf note: For a 1024x1024 shadow map, this step takes 1.81 ms on the GPU

            // 1st Pass: Copy the shadow map to the variance shadow map and while performing
            // a 5-tap horizontal Gaussian blur
            m_PostProcess.CopyTexture( m_pShadowMap, m_pVarianceShadowMap, m_pHorizontalBlurDepthToVariancePS );

            // 2nd pass: Perform a 5-tap vertical Gaussian blur
            m_PostProcess.CopyTexture( m_pVarianceShadowMap, m_pVarianceShadowMap, m_pVerticalBlurDepthToVariancePS );
        }
        else
        {
            // Convert the D24S8 shadow map to the 2-channel (depth and depth-squared) VSM
            // Perf note: For a 1024x1024 shadow map, this step takes 0.63 ms on the GPU
            m_PostProcess.CopyTexture( m_pShadowMap, m_pVarianceShadowMap, m_pCopyDepthToVariancePS );
        }

        // Build mip-maps for the variance shadow map
        // Perf note: For a 1024x1024 shadow map, this step takes 0.22 ms on the GPU
        m_PostProcess.BuildMipMaps( m_pVarianceShadowMap );
    }

    //----------------------------------------------------------------------------------
    // Draw the scene using the current shadow mapping method
    //----------------------------------------------------------------------------------
    {
        // Draw a gradient filled background and clear the z-buffer.
        ATG::RenderBackground( 0xff000000, 0xff000000 );

        // Set sampler state for the objects' diffuse maps.
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

        // Set shaders and shader options for the appropriate shadowing method
        m_pd3dDevice->SetVertexShader( m_pLightWithShadowsVS );
        m_pd3dDevice->SetPixelShader( m_pLightWithShadowsPS );
        m_pd3dDevice->SetPixelShaderConstantB( 0, &m_bUseVarianceShadowMap, 1 );
        m_pd3dDevice->SetPixelShaderConstantB( 2, &m_bDebugShowShadow, 1 );
        m_pd3dDevice->SetPixelShaderConstantF( 10, &m_fEpsilonVSM, 1 );

        // Set the Shadow map
        if( m_bUseVarianceShadowMap )
        {
            m_pd3dDevice->SetTexture( 1, m_pVarianceShadowMap );
            if( m_dwFiltering == FILTER_BILINEAR || m_dwFiltering == FILTER_TRILINEAR )
            {
                m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
                m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
            }
            else if( m_dwFiltering == FILTER_ANISOTROPIC )
            {
                m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
                m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
                m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAXANISOTROPY, 16 );
            }
            else
            {
                m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
                m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
            }
            if( m_dwFiltering == FILTER_TRILINEAR )
            {
                m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
            }
            else
            {
                m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
            }
        }
        else
        {
            m_pd3dDevice->SetTexture( 1, m_pShadowMap );
            m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
            m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
            m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
        }

        // Set a border color that's equal to a depth of 1.0f for the shadowmap
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER );
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER );
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_BORDERCOLOR, 0xffffffff );

        // Render the chess pieces and board
        {
            // Update global parameters that will be referenced during rendering of the meshes
            CustomMesh::g_matCameraViewProj = m_matCameraView * m_matCameraProj;
            CustomMesh::g_matShadowViewProj = m_matLightView * m_matLightProj;
            CustomMesh::g_vLightDirection = m_vLightDirection;
            CustomMesh::g_vViewPosition = m_vEyePt;

            m_ChessBoard.RenderPieces( XMMatrixIdentity() );
            m_ChessBoard.Render( XMMatrixIdentity() );
        }

        // Cleanup
        m_pd3dDevice->SetTexture( 1, NULL );
    }

    //----------------------------------------------------------------------------------
    // Show title, frame rate, and help
    //----------------------------------------------------------------------------------
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        WCHAR strText[20];
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"VarianceShadowMaps" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.DrawText( 0, 30, 0xffffffff, L"Using: " );
        if( m_bUseVarianceShadowMap )
            m_Font.DrawText( 0xffffff00, L"Variance Shadow Map" );
        else
            m_Font.DrawText( 0xffffff00, L"Conventional Shadow Map" );

        swprintf_s( strText, L"%ld", m_dwShadowMapSize );
        m_Font.DrawText( 0, 55, 0xffffffff, L"ShadowMap Size: " );
        m_Font.DrawText( 0xffffff00, strText );

        if( m_bUseVarianceShadowMap )
        {
            m_Font.DrawText( 0, 80, 0xffffffff, L"Blurring: " );
            m_Font.DrawText( 0xffffff00, m_bBlurVarianceMap ? L"5x5 Gaussian Blur" : L"Off" );

            m_Font.DrawText( 0, 105, 0xffffffff, L"Filtering: " );
            m_Font.DrawText( 0xffffff00, g_FilteringNames[m_dwFiltering] );

            swprintf_s( strText, L"%f", m_fEpsilonVSM );
            m_Font.DrawText( 0, 130, 0xffffffff, L"Epsilon: " );
            m_Font.DrawText( 0xffffff00, strText );
        }

        m_Font.End();
    }

    //----------------------------------------------------------------------------------
    // Present the scene
    //----------------------------------------------------------------------------------
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
