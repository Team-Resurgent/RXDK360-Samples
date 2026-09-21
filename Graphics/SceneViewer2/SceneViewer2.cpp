//--------------------------------------------------------------------------------------
// SceneViewer2.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "SceneViewer2.h"
#include <xgraphics.h>
#include <set>

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display help" },
    { ATG::HELP_START_BUTTON,ATG::HELP_PLACEMENT_2, L"Settings and Options" },
    { ATG::HELP_LEFTSTICK,   ATG::HELP_PLACEMENT_2, L"Camera move" },
    { ATG::HELP_RIGHTSTICK,  ATG::HELP_PLACEMENT_2, L"Camera look" },
    { ATG::HELP_RIGHT_SHOULDER,ATG::HELP_PLACEMENT_1, L"Fast camera" },
    { ATG::HELP_LEFT_SHOULDER,ATG::HELP_PLACEMENT_1, L"Slow camera" },
    { ATG::HELP_B_BUTTON,    ATG::HELP_PLACEMENT_1, L"Tweak objects" },
};
const DWORD     NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );

ATG::HELP_CALLOUT g_HelpCalloutsTweak[] =
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display help" },
    { ATG::HELP_DPAD,        ATG::HELP_PLACEMENT_1, L"Select Object\nChange Filter" },
    { ATG::HELP_LEFTSTICK,   ATG::HELP_PLACEMENT_2, L"Object move" },
    { ATG::HELP_RIGHTSTICK,  ATG::HELP_PLACEMENT_2, L"Object rotate" },
    { ATG::HELP_RIGHT_SHOULDER,ATG::HELP_PLACEMENT_1, L"Fast move" },
    { ATG::HELP_LEFT_SHOULDER,ATG::HELP_PLACEMENT_1, L"Slow move" },
    { ATG::HELP_B_BUTTON,    ATG::HELP_PLACEMENT_1, L"Exit tweak mode" },
    { ATG::HELP_X_BUTTON,    ATG::HELP_PLACEMENT_2, L"Object movement on/off" },
    { ATG::HELP_A_BUTTON,    ATG::HELP_PLACEMENT_1, L"Enable/disable light" }
};
const DWORD     NUM_HELP_CALLOUTS_TWEAK = ARRAYSIZE( g_HelpCalloutsTweak );

ATG::HELP_CALLOUT g_HelpCalloutsSettingsUI[] =
{
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_1, L"Navigate menus and\nadjust settings" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Toggle bools and\nactivate commands" },
    { ATG::HELP_START_BUTTON,ATG::HELP_PLACEMENT_2, L"Exit menus" },
    { ATG::HELP_BACK_BUTTON,ATG::HELP_PLACEMENT_2, L"Exit menus" },
    { ATG::HELP_B_BUTTON,ATG::HELP_PLACEMENT_2, L"Exit menus" },
    { ATG::HELP_RIGHT_SHOULDER,ATG::HELP_PLACEMENT_1, L"Adjust setting faster" },
    { ATG::HELP_LEFT_SHOULDER,ATG::HELP_PLACEMENT_1, L"Adjust setting slower" },
    { ATG::HELP_LEFT_TRIGGER, ATG::HELP_PLACEMENT_1, L"Previous menu" },
    { ATG::HELP_RIGHT_TRIGGER, ATG::HELP_PLACEMENT_1, L"Next menu" },
};
const DWORD     NUM_HELP_CALLOUTS_SETTINGSUI = ARRAYSIZE( g_HelpCalloutsSettingsUI );

SceneViewer*    g_pSceneViewerApp = NULL;
HANDLE          g_hMainThread = INVALID_HANDLE_VALUE;
HANDLE          g_hLoaderThread = INVALID_HANDLE_VALUE;
DWORD           g_dwLoaderThreadID = 0;

DWORD WINAPI LoaderThreadEntry( LPVOID pParam );

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    SceneViewer atgApp;
    g_pSceneViewerApp = &atgApp;

    // SceneViewer2 always creates 720p back buffers and front buffers.
    atgApp.m_d3dpp.BackBufferWidth = 1280;
    atgApp.m_d3dpp.BackBufferHeight = 720;

    // Disable all automatic buffer creation.
    atgApp.m_d3dpp.DisableAutoFrontBuffer = TRUE;
    atgApp.m_d3dpp.DisableAutoBackBuffer = TRUE;
    atgApp.m_d3dpp.EnableAutoDepthStencil = FALSE;
    atgApp.m_d3dpp.BackBufferCount = 0;

    // Immediate presentation interval is the default.
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    // Front buffers are 10 bits per pixel for better color reproduction.
    //atgApp.m_d3dpp.FrontBufferFormat = D3DFMT_LE_X2R10G10B10;
    atgApp.m_d3dpp.FrontBufferFormat = D3DFMT_LE_X8R8G8B8;
    atgApp.m_d3dpp.FrontBufferColorSpace = D3DCOLORSPACE_RGB;

    // 5MB secondary ring buffer size, to accommodate rather large scenes with tiling.
    atgApp.m_d3dpp.RingBufferParameters.SecondarySize = 5120 * 1024;

    atgApp.m_dwDeviceCreationFlags = D3DCREATE_BUFFER_2_FRAMES;

    atgApp.Run();
}

FLOAT           g_fDummyFloat = 0.0f;
DWORD WINAPI BusyWorkThreadEntry( LPVOID pParam )
{
    ATG::SetThreadName( ( DWORD )-1, "Busy Work Thread" );
    DWORD dwValue = 0;
    while( true )
    {
        ++dwValue;
        g_fDummyFloat = ( FLOAT )dwValue;
        if( dwValue > 100000 )
        {
            dwValue = 0;
        }
    }
}

DWORD WINAPI ThreadProcUpdate( LPVOID pParam )
{
    ATG::SetThreadName( ( DWORD )-1, "Update Thread" );
    SceneViewer* pSceneViewer = ( SceneViewer* )pParam;
    while( true )
    {
        pSceneViewer->UpdateGameState();
    }
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects, and starts up the various
//       subsystems.
//--------------------------------------------------------------------------------------
HRESULT SceneViewer::Initialize()
{
    m_bDrawHelp = FALSE;
    m_pScene = NULL;
    m_pCurrentSceneCamera = NULL;
    m_dwActiveCameraIndex = 0;
    m_bWireframe = FALSE;
    m_dwDebugRenderMode = 0;
    m_bDisplayLights = FALSE;
    m_bDisplayBounds = FALSE;
    m_bDisplayFrames = FALSE;
    m_bDisplayCameras = FALSE;
    m_bEnableLighting = TRUE;
    m_bEnableAnimations = TRUE;
    m_bEnableShadowUpdates = TRUE;
    m_bReduceShadowShimmer = TRUE;
    m_bDisableAllUI = FALSE;
    m_fLightRangeScale = 1.0f;
    m_fLightIntensityScale = 1.0f;
    m_fDebugNormalsScale = 0.1f;
    m_bInvertRotationYAxis = TRUE;
    m_bFrustumCulling = TRUE;
    m_bLockVisibleSet = FALSE;
    m_bDrawStats = TRUE;
    m_bDrawMemoryStats = FALSE;
    m_ZPassMode = SVZP_D3D;
    m_fAnimationSpeed = 0.0f;
    m_fCameraMoveSpeed = 1.0f;
    m_fAmbient = 0.1f;
    m_dwTriangleCullingMode = D3DCULL_CCW;
    m_dwAsyncLoadProgress = 0;
    m_strSceneParseErrorMsg = NULL;
    m_iIsolatedModelIndex = -1;
    m_iRenderAnimationTrackIndex = -1;
    m_dwUpAxis = 0;
    m_dwMaxPointLightsToSet = 6;
    m_dwMaxSpotLightsToSet = 6;
    m_dwMaxDirLightsToSet = 2;
    m_bSortLightsByDistance = TRUE;
    m_pDepthTexture = NULL;
    m_bShowDebugBuffers = FALSE;
    m_fCameraZFar = 2000.0f;
    m_pShadowMapRenderTarget = NULL;
    m_pDeferredBaseMaterial = NULL;
    m_pPassPerLightBaseMaterial = NULL;
    m_pShaderLibraryBaseMaterial = NULL;
    m_pUbershaderBaseMaterial = NULL;
    m_iShowShadowMap = -1;
    m_fShadowSlopedDepthBias = 3.0f;
    m_bDrawGroundPlane = FALSE;
    m_RenderMode = SVRM_NORMAL;
    m_TilingMode = SVTM_720p4X_3;
    m_pDeferredColorBuffer = NULL;
    m_pDeferredNormalBuffer = NULL;
    m_pFullScreenTarget = NULL;
    m_dwTilingRectCount = 0;
    m_bStencilOptimization = FALSE;
    m_bDisplayPerfChart = FALSE;
    m_bCapturePerfData = FALSE;
    m_bIsolatePerfSections = TRUE;
    m_pFrontBuffer = NULL;
    m_pSceneResolveBuffer = NULL;
    m_dwUbershaderTechniqueIndex = 0;
    m_dwTextureOverrideMode = 0;
    m_fTextScalingFactor = 1.0f;
    m_dwPostEffectTechniqueIndex = 0;
    m_pPostEffects = NULL;
    m_fFocalDepth = 0.0f;
    m_fFocalAperture = 1.0f;
    m_fFocalSlope = 70.0f;
    m_fMaxCircleOfConfusion = 5.0f;
    m_dwSecondaryRingBufferSize = 5120;
    m_bDrawTilingStats = FALSE;
    m_dwShadowMapSize = 1024;
    m_bMipShadowMaps = TRUE;
    m_bRenderUpsideDown = TRUE;
    m_fBoneRadius = 0.0f;
    m_dwDefaultLightRigIndex = 0;
    m_dwCameraControlType = 1;
    m_bDrawSafeRect = FALSE;
    m_bDrawTransparentObjects = TRUE;
    m_pDepthBuffer = NULL;
    m_strSceneFileName[0] = '\0';
    m_pColorRenderTargets[0] = NULL;
    m_pColorRenderTargets[1] = NULL;
    m_pColorRenderTargets[2] = NULL;
    m_pColorRenderTargets[3] = NULL;
    m_bAdaptiveCameraSpeed = TRUE;
    m_fTightDirShadowRadius = 7.5f;

    ATG::SetThreadName( ( DWORD )-1, "Render Thread" );

    // Initialize the Direct3D critical section.
    InitializeCriticalSection( &m_Direct3DCriticalSection );
    // Initialize the scene critical section.
    InitializeCriticalSection( &m_SceneCriticalSection );
    // Initialize render settings critical section.
    InitializeCriticalSection( &m_RenderSettingsCriticalSection );

    // Acquire D3D for initialization tasks.
    AcquireD3D();

    m_pd3dDevice->SetRenderState( D3DRS_HIGHPRECISIONBLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    SetupBuffers();

    // Create the font.
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\SegoeUI_16_Outline.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area.
    m_TitleSafeRect = ATG::GetTitleSafeArea();
    m_Font.SetWindow( m_TitleSafeRect );

    // Create the help screen.
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // SceneViewer2 always runs in widescreen.
    m_fAspectRatio = 16.0f / 9.0f;

    // Set a viewport.
    m_Viewport.X = 0;
    m_Viewport.Y = 0;
    m_Viewport.Width = m_d3dpp.BackBufferWidth;
    m_Viewport.Height = m_d3dpp.BackBufferHeight;
    m_Viewport.MinZ = 0.0f;
    m_Viewport.MaxZ = 1.0f;
    m_pd3dDevice->SetViewport( &m_Viewport );

    {
        m_ShadowSceneState.Camera.SetWorldTransform( XMMatrixIdentity() );
        ATG::Projection ProjDefault;
        ProjDefault.SetFovYAspect( XM_PIDIV2, 1.777f, 0.1f, 100.0f );
        m_ShadowSceneState.Camera.SetProjection( ProjDefault );
        m_ShadowSceneState.Camera.SetViewport( m_Viewport );
    }

    // Initialize simple shaders for debug draw and more.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Initialize UI.
    SetupUI();

    // Initialize benchmarking.
    m_BenchmarkModule.Initialize( this );

    // Create the shadow texture offset matrix.
    m_matShadowTextureOffset = XMMatrixScaling( 0.5f, -0.5f, 1.0f ) * XMMatrixTranslation( 0.5f, 0.5f, 0.0f );

#ifndef _RELEASED3D
    // Create some perf counter objects.  These hold the results of a performance query.
    for( DWORD i = 0; i < ARRAYSIZE( m_pPerfCounters ); i++ )
    {
        m_pd3dDevice->CreatePerfCounters( &m_pPerfCounters[i], 1 );
    }
#endif

    // Create the post-effect library.
    ATG::BaseMaterial::SetMediaRootPath( "game:\\media\\" );
    m_pPostEffects = ATG::BaseMaterial::CreateFXLiteMaterial( L"PostEffects", L"posteffects.fxobj" );
    m_pPostEffects->InitializeImplementation();
    ATG::FXLiteMaterialImplementation* pFXLiteMaterialImpl = ( ATG::FXLiteMaterialImplementation* )
        m_pPostEffects->GetMaterialImplementation();
    pFXLiteMaterialImpl->m_pEffect->ChangeDevice( m_pd3dDevice );
    m_PostEffectParameterPool.Initialize( pFXLiteMaterialImpl->m_pEffect );

    // Release D3D.
    ReleaseD3D();

    // Start up the threaded loader.
    g_hMainThread = GetCurrentThread();
    g_hLoaderThread = CreateThread( NULL, 0, LoaderThreadEntry, this, 0, &g_dwLoaderThreadID );
    if( g_hLoaderThread == NULL )
         return E_FAIL; 

    // Loader thread goes on core 2 (HW thread 4).
    XSetThreadProcessor( g_hLoaderThread, 4 );
    // Pass the Direct3D critical section to the loader so it can safely use D3D.
    ATG::SceneFileParser::PrepareForThreadedLoad( &m_Direct3DCriticalSection );

    // Start up the update thread.
    m_hUpdateThread = CreateThread( NULL, 0, ThreadProcUpdate, this, 0, NULL );
    if( m_hUpdateThread == NULL )
        return E_FAIL;

    XSetThreadProcessor( m_hUpdateThread, 2 );
    m_TaskUpdate.BeginTask();

    // Parse the command line, and load the scene if one is specified.
    CHAR* strCommandLine = GetCommandLine();
    CHAR* strFileName = strstr( strCommandLine, "/L " );
    if( strFileName != NULL )
    {
        LoadSceneAsync( strFileName + 3 );
    }
    else
    {
        DWORD dwFileSize = 0;
        HRESULT hr = ATG::LoadFile( "game:\\commandline.txt", ( VOID** )&strFileName, &dwFileSize );
        if( SUCCEEDED( hr ) )
        {
            CHAR strFileNameClean[MAX_PATH];
            // Copy the text from the file to get it null-terminated.
            // strncpy_s will always null-terminate.
            strncpy_s( strFileNameClean, strFileName, dwFileSize );
            LoadSceneAsync( strFileNameClean );
            ATG::UnloadFile( strFileName );
        }
    }

    // Create a bank of spotlight shadow map textures.
    BuildShadowMapBank();

    return S_OK;
}


D3DRECT MakeD3DRect( DWORD x1, DWORD y1, DWORD x2, DWORD y2 )
{
    D3DRECT Rect;
    Rect.x1 = x1;
    Rect.y1 = y1;
    Rect.x2 = x2;
    Rect.y2 = y2;
    return Rect;
}


//--------------------------------------------------------------------------------------
// Name: SetupBuffers()
// Desc: Creates all of the rendertargets, front buffers, texture rendertargets, and
//       other textures for all of the various rendering modes.
//--------------------------------------------------------------------------------------
VOID SceneViewer::SetupBuffers()
{
    DWORD dwScreenWidth = m_d3dpp.BackBufferWidth;
    DWORD dwScreenHeight = m_d3dpp.BackBufferHeight;

    // Unset all render targets.
    for( DWORD i = 0; i < 4; i++ )
    {
        m_pd3dDevice->SetRenderTarget( i, NULL );
    }
    // Unset all textures.
    for( DWORD i = 0; i < 16; i++ )
    {
        m_pd3dDevice->SetTexture( i, NULL );
    }
    // Block the D3D device until idle.  We're going to nuke a lot of resources, so
    // we want to make sure the device is done with them.
    m_pd3dDevice->BlockUntilIdle();
    // Release all of the color render targets.
    for( DWORD i = 0; i < 4; i++ )
    {
        if( m_pColorRenderTargets[i] != NULL )
            m_pColorRenderTargets[i]->Release();
        m_pColorRenderTargets[i] = NULL;
    }
    // Release the shadow map render target.
    if( m_pShadowMapRenderTarget != NULL )
    {
        m_pShadowMapRenderTarget->Release();
        m_pShadowMapRenderTarget = NULL;
    }
    // Release the deferred lighting textures.
    if( m_pDeferredColorBuffer != NULL )
    {
        m_pDeferredColorBuffer->Release();
        m_pDeferredColorBuffer = NULL;
    }
    if( m_pDeferredNormalBuffer != NULL )
    {
        m_pDeferredNormalBuffer->Release();
        m_pDeferredNormalBuffer = NULL;
    }
    // Clear samplers in the shared parameter pool.
    for( DWORD i = 0; i < 6; i++ )
    {
        m_SampleParameterPool.SetSpotLightShadowDepthTexture( i, NULL );
    }
    // Clear samplers in the shared parameter pool.
    m_DeferredParameterPool.SetShadowParameters( NULL, XMMatrixIdentity() );
    m_PassPerLightParameterPool.SetShadowParameters( NULL, XMMatrixIdentity() );

    // Set up format variables.
    D3DFORMAT ColorBufferFormat = D3DFMT_A2R10G10B10;
    D3DFORMAT NormalBufferFormat = D3DFMT_X2R10G10B10;
    //D3DFORMAT FrontBufferFormat = D3DFMT_LE_X2R10G10B10;
    D3DFORMAT FrontBufferFormat = D3DFMT_LE_X8R8G8B8;
    D3DFORMAT DepthStencilFormat = D3DFMT_D24S8;
    D3DFORMAT DepthStencilTextureFormat = D3DFMT_D24S8;

    HRESULT hr = S_OK;
    // Create a zeroed out surface parameters struct.
    D3DSURFACE_PARAMETERS SurfParams;
    ZeroMemory( &SurfParams, sizeof( D3DSURFACE_PARAMETERS ) );
    SurfParams.ColorExpBias = 0;

    // Create the front buffer if it isn't there already.  This texture persists across
    // device resets, since all rendering modes share a common front buffer size and
    // format.
    if( m_pFrontBuffer == NULL )
    {
        hr = m_pd3dDevice->CreateTexture( dwScreenWidth, dwScreenHeight, 1, 0,
                                          FrontBufferFormat, D3DPOOL_DEFAULT,
                                          &m_pFrontBuffer, NULL );
    }

    // Create the scene resolve buffer if it isn't there already.  This texture is used
    // as the intermediate resolve target for post-effect operations.
    if( m_pSceneResolveBuffer == NULL )
    {
        hr = m_pd3dDevice->CreateTexture( dwScreenWidth, dwScreenHeight, 1, 0,
                                          FrontBufferFormat, D3DPOOL_DEFAULT,
                                          &m_pSceneResolveBuffer, NULL );
    }

    // Create a screen sized depth buffer texture.  This is used primarily for
    // deferred lighting, but it can also be used for debugging.
    if( m_pDepthTexture == NULL )
    {
        hr = m_pd3dDevice->CreateTexture( dwScreenWidth, dwScreenHeight,
                                          1, 0, DepthStencilTextureFormat,
                                          NULL, &m_pDepthTexture, NULL );
    }

    ZeroMemory( m_TilingRects, sizeof( m_TilingRects ) );

    // Create render targets and resolve targets for each rendering mode.
    switch( m_RenderMode )
    {
        case SVRM_NORMAL:
        case SVRM_SHADERLIB:
        case SVRM_PASSPERLIGHT:
            {
                DWORD dwTileWidth = 0;
                DWORD dwTileHeight = 0;
                switch( m_TilingMode )
                {
                    case SVTM_720p2X_H:
                        // Standard rendering with 2x MSAA.
                        m_d3dpp.MultiSampleType = D3DMULTISAMPLE_2_SAMPLES;
                        // Build tiling rectangles.
                        m_dwTilingRectCount = 2;
                        dwTileWidth = 1280;
                        dwTileHeight = 384;
                        m_TilingRects[0] = MakeD3DRect( 0, 0, 1280, 384 );
                        m_TilingRects[1] = MakeD3DRect( 0, 384, 1280, 720 );
                        break;
                    case SVTM_720p2X_V:
                        // Standard rendering with 2x MSAA.
                        m_d3dpp.MultiSampleType = D3DMULTISAMPLE_2_SAMPLES;
                        // Build tiling rectangles.
                        m_dwTilingRectCount = 2;
                        dwTileWidth = 640;
                        dwTileHeight = 736;
                        m_TilingRects[0] = MakeD3DRect( 0, 0, 640, 720 );
                        m_TilingRects[1] = MakeD3DRect( 640, 0, 1280, 720 );
                        break;
                    case SVTM_720p4X_3:
                        // Standard rendering with 4x MSAA.
                        m_d3dpp.MultiSampleType = D3DMULTISAMPLE_4_SAMPLES;
                        // Create tiling rectangles.
                        m_dwTilingRectCount = 3;
                        dwTileWidth = 1280;
                        dwTileHeight = 256;
                        m_TilingRects[0] = MakeD3DRect( 0, 0, 1280, 256 );
                        m_TilingRects[1] = MakeD3DRect( 0, 256, 1280, 512 );
                        m_TilingRects[2] = MakeD3DRect( 0, 512, 1280, 720 );
                        break;
                    case SVTM_720p4X_4:
                        // Standard rendering with 4x MSAA.
                        m_d3dpp.MultiSampleType = D3DMULTISAMPLE_4_SAMPLES;
                        // Create tiling rectangles.
                        m_dwTilingRectCount = 4;
                        dwTileWidth = 320;
                        dwTileHeight = 736;
                        m_TilingRects[0] = MakeD3DRect( 0, 0, 320, 720 );
                        m_TilingRects[1] = MakeD3DRect( 320, 0, 640, 720 );
                        m_TilingRects[2] = MakeD3DRect( 640, 0, 960, 720 );
                        m_TilingRects[3] = MakeD3DRect( 960, 0, 1280, 720 );
                        break;
                    case SVTM_NONE:
                    default:
                        // Standard rendering with no MSAA or tiling.
                        m_d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
                        m_dwTilingRectCount = 0;
                        dwTileWidth = 1280;
                        dwTileHeight = 720;
                        break;
                }

                // Create the color rendertarget.
                hr = m_pd3dDevice->CreateRenderTarget( dwTileWidth, dwTileHeight,
                                                       ColorBufferFormat,
                                                       m_d3dpp.MultiSampleType,
                                                       0, FALSE,
                                                       &m_pColorRenderTargets[0],
                                                       &SurfParams );
                // Increment the base by the size of the surface we just created.
                SurfParams.Base += XGSurfaceSize( dwTileWidth, dwTileHeight, ColorBufferFormat,
                                                  m_d3dpp.MultiSampleType );

                // Create the depth/stencil target.
                SurfParams.HierarchicalZBase = 0;
                hr = m_pd3dDevice->CreateDepthStencilSurface( dwTileWidth, dwTileHeight,
                                                              DepthStencilFormat,
                                                              m_d3dpp.MultiSampleType,
                                                              0, FALSE,
                                                              &m_pDepthBuffer,
                                                              &SurfParams );
                // Increment the base and hi-z base by the size of the surface we just created.
                SurfParams.Base += XGSurfaceSize( dwTileWidth, dwTileHeight, DepthStencilFormat,
                                                  m_d3dpp.MultiSampleType );
                SurfParams.HierarchicalZBase += XGHierarchicalZSize( dwTileWidth, dwTileHeight,
                                                                     m_d3dpp.MultiSampleType );

                break;
            }
        case SVRM_DEFERRED:
        {
            // Deferred rendering does not use hardware MSAA.  The m_TilingMode member
            // variable is ignored during deferred rendering.
            m_d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
            // Build tiling rectangles.  Since we have multiple render targets, we need
            // to tile in order to fit within EDRAM.
            m_dwTilingRectCount = 2;
            DWORD dwTileWidth = 1280;
            DWORD dwTileHeight = 384;
            m_TilingRects[0] = MakeD3DRect( 0, 0, 1280, 384 );
            m_TilingRects[1] = MakeD3DRect( 0, 384, 1280, 720 );

            // Create a diffuse color rendertarget.
            hr = m_pd3dDevice->CreateRenderTarget( dwTileWidth, dwTileHeight,
                                                   ColorBufferFormat,
                                                   D3DMULTISAMPLE_NONE, 0, FALSE,
                                                   &m_pColorRenderTargets[0],
                                                   &SurfParams );
            // Increment the base by the size of the surface we just created.
            SurfParams.Base += XGSurfaceSize( dwTileWidth, dwTileHeight, ColorBufferFormat, D3DMULTISAMPLE_NONE );

            // Create a normals rendertarget for per-pixel normals.
            hr = m_pd3dDevice->CreateRenderTarget( dwTileWidth, dwTileHeight,
                                                   NormalBufferFormat,
                                                   D3DMULTISAMPLE_NONE, 0, FALSE,
                                                   &m_pColorRenderTargets[1],
                                                   &SurfParams );
            // Increment the base by the size of the surface we just created.
            SurfParams.Base += XGSurfaceSize( dwTileWidth, dwTileHeight, NormalBufferFormat, D3DMULTISAMPLE_NONE );

            // Create a depth/stencil target.
            hr = m_pd3dDevice->CreateDepthStencilSurface( dwTileWidth, dwTileHeight,
                                                          DepthStencilFormat,
                                                          D3DMULTISAMPLE_NONE, 0, FALSE,
                                                          &m_pDepthBuffer,
                                                          &SurfParams );
            // Increment the base and hi-z base by the size of the surface we just created.
            SurfParams.Base += XGSurfaceSize( dwTileWidth, dwTileHeight, DepthStencilFormat, D3DMULTISAMPLE_NONE );
            SurfParams.HierarchicalZBase += XGHierarchicalZSize( dwTileWidth, dwTileHeight, D3DMULTISAMPLE_NONE );

            // Create a destination texture for the diffuse color data.
            hr = m_pd3dDevice->CreateTexture( dwScreenWidth, dwScreenHeight, 1, 0,
                                              ColorBufferFormat, D3DPOOL_DEFAULT,
                                              &m_pDeferredColorBuffer, NULL );
            // Create a destination texture for the per-pixel normal data.
            hr = m_pd3dDevice->CreateTexture( dwScreenWidth, dwScreenHeight, 1, 0,
                                              NormalBufferFormat, D3DPOOL_DEFAULT,
                                              &m_pDeferredNormalBuffer, NULL );
            break;
        }
    }

    // Create a shadow map rendertarget.
    // Shadow rendertarget is created at offset 0, and we'll render shadow maps
    // before scene rendering.
    SurfParams.Base = 0;
    SurfParams.HierarchicalZBase = 0;

    m_pd3dDevice->CreateDepthStencilSurface( m_dwShadowMapSize, m_dwShadowMapSize,
                                             DepthStencilFormat,
                                             D3DMULTISAMPLE_NONE, 0, FALSE,
                                             &m_pShadowMapRenderTarget,
                                             &SurfParams );

    // Create a shadow map viewport.
    m_ShadowViewport.X = 0;
    m_ShadowViewport.Y = 0;
    m_ShadowViewport.Width = m_dwShadowMapSize;
    m_ShadowViewport.Height = m_dwShadowMapSize;
    m_ShadowViewport.MinZ = 0.0f;
    m_ShadowViewport.MaxZ = 1.0f;

    // Create a full screen rendertarget to be used in post-processing, including the
    // deferred lighting pass.
    SurfParams.Base = 0;
    SurfParams.HierarchicalZBase = 0;
    hr = m_pd3dDevice->CreateRenderTarget( dwScreenWidth, dwScreenHeight,
                                           ColorBufferFormat,
                                           D3DMULTISAMPLE_NONE, 0, FALSE,
                                           &m_pFullScreenTarget,
                                           &SurfParams );
}


//--------------------------------------------------------------------------------------
// Name: RebindMaterialShaders()
// Desc: When the rendering mode is changed, the material parameter data present in the
//       scene must be re-bound to a new set of FXLite shaders.  In the case of deferred
//       lighting and pass-per-light, there is a fixed FXLite shader for all objects
//       (deferred.fx and passperlight.fx respectively).  If the app is transitioning
//       back to "normal" rendering, we simply re-bind the shader that the scene file
//       specified originally (usually ubershader.fx).
//--------------------------------------------------------------------------------------
VOID SceneViewer::RebindMaterialShaders( ATG::Scene* pScene )
{
    assert( pScene != NULL );
    if( m_RenderMode == SVRM_NORMAL )
    {
        // Keep track of the Material objects that we have already fixed.
        typedef std::set <ATG::MaterialInstance*> MaterialSet;
        MaterialSet TouchedMaterials;

        // Iterate over all scene objects.
        ATG::NameIndexedCollection::iterator iter = pScene->GetInstanceList()->begin();
        ATG::NameIndexedCollection::iterator end = pScene->GetInstanceList()->end();
        while( iter != end )
        {
            // We only care about Models.
            if( !( *iter )->IsDerivedFrom( ATG::Model::TypeID ) )
            {
                iter++;
                continue;
            }
            ATG::Model* pModel = ( ATG::Model* )*iter;
            // Iterate through mesh mappings.
            DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
            for( DWORD i = 0; i < dwMeshMappingCount; ++i )
            {
                ATG::MeshMapping& meshmapping = pModel->GetMeshMapping( i );
                // Iterate over mesh subsets.
                DWORD dwNumSubsets = meshmapping.pMesh->GetNumSubsets();
                for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwNumSubsets; ++dwSubsetIndex )
                {
                    ATG::MaterialInstance* pMaterial = meshmapping.Materials[ dwSubsetIndex ];
                    // Check if we've already fixed this material.
                    if( TouchedMaterials.find( pMaterial ) == TouchedMaterials.end() )
                    {
                        AcquireD3D();
                        // Reload the original shader, and rebind material parameters.
                        //pMaterial->CreateFXFromRawParameters( pScene, pScene->GetMediaRootPath() );
                        ATG::BaseMaterial* pBaseMaterial = ( ATG::BaseMaterial* )pScene->GetResourceDatabase
                            ()->FindResourceOfType( pMaterial->GetBaseMaterialName(), ATG::BaseMaterial::TypeID );
                        assert( pBaseMaterial != NULL );
                        pMaterial->SetBaseMaterial( pBaseMaterial );
                        pMaterial->Initialize();
                        ReleaseD3D();
                        // Add the fixed material to the list.
                        TouchedMaterials.insert( pMaterial );
                    }
                }
            }
            iter++;
        }
    }
    else
    {
        // Get a pointer to the proper parent effect.
        ATG::BaseMaterial* pBaseMaterial = m_pDeferredBaseMaterial;
        if( m_RenderMode == SVRM_PASSPERLIGHT )
            pBaseMaterial = m_pPassPerLightBaseMaterial;
        if( m_RenderMode == SVRM_SHADERLIB )
            pBaseMaterial = m_pShaderLibraryBaseMaterial;
        if( pBaseMaterial == NULL )
            return;
        // Keep track of the Material objects that we've already fixed.
        typedef std::set <ATG::MaterialInstance*> MaterialSet;
        MaterialSet TouchedMaterials;

        // Iterate over all scene objects.
        ATG::NameIndexedCollection::iterator iter = pScene->GetInstanceList()->begin();
        ATG::NameIndexedCollection::iterator end = pScene->GetInstanceList()->end();
        while( iter != end )
        {
            // We only care about Models.
            if( !( *iter )->IsDerivedFrom( ATG::Model::TypeID ) )
            {
                iter++;
                continue;
            }
            ATG::Model* pModel = ( ATG::Model* )*iter;
            // Iterate over mesh mappings.
            DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
            for( DWORD i = 0; i < dwMeshMappingCount; ++i )
            {
                ATG::MeshMapping& meshmapping = pModel->GetMeshMapping( i );
                // Iterate over mesh subsets.
                DWORD dwNumSubsets = meshmapping.pMesh->GetNumSubsets();
                for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwNumSubsets; ++dwSubsetIndex )
                {
                    ATG::MaterialInstance* pMaterial = meshmapping.Materials[ dwSubsetIndex ];
                    // Check if we've already fixed this material.
                    if( TouchedMaterials.find( pMaterial ) == TouchedMaterials.end() )
                    {
                        AcquireD3D();
                        // Bind the material parameters to a new instance of the base material.
                        pMaterial->SetBaseMaterial( pBaseMaterial );
                        pMaterial->Initialize();
                        //pMaterial->BindToEffect( pParentEffect, pScene, pScene->GetMediaRootPath() );
                        ReleaseD3D();
                        // Add the fixed material to the list.
                        TouchedMaterials.insert( pMaterial );
                    }
                }
            }
            iter++;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: SetupUI()
// Desc: Creates all of the debug UI for the app.
//--------------------------------------------------------------------------------------
VOID SceneViewer::SetupUI()
{
    m_SettingsPanel.Initialize( m_pd3dDevice );
    DWORD dwSettingIndex = 0;
    SettingsGroup* pGroup = NULL;

    //----------------------------------------------------------------------------------
    m_SettingsPanel.CreateNewGroup( L"Help" );
    pGroup = m_SettingsPanel.GetGroup();
    assert( pGroup != NULL );
    pGroup->m_bHelpMenu = TRUE;
    pGroup->AddHelpScreen( L"Camera Controls", g_HelpCallouts, NUM_HELP_CALLOUTS );
    pGroup->AddHelpScreen( L"Settings UI Controls", g_HelpCalloutsSettingsUI, NUM_HELP_CALLOUTS_SETTINGSUI );
    pGroup->AddHelpScreen( L"Object Tweaker Controls", g_HelpCalloutsTweak, NUM_HELP_CALLOUTS_TWEAK );

    //----------------------------------------------------------------------------------
    m_SettingsPanel.CreateNewGroup( L"Scene" );
    pGroup = m_SettingsPanel.GetGroup();
    assert( pGroup != NULL );
    pGroup->AddCommand( SVUI_LOADSCENE, L"Load Scene" );
    dwSettingIndex = pGroup->AddEnum( &m_dwUpAxis, L"World Up Axis" );
    pGroup->AddEnumEntry( dwSettingIndex, L"Y Axis", 0 );
    pGroup->AddEnumEntry( dwSettingIndex, L"Z Axis", 1 );

    //----------------------------------------------------------------------------------
    m_SettingsPanel.CreateNewGroup( L"Camera" );
    pGroup = m_SettingsPanel.GetGroup();
    assert( pGroup != NULL );
    pGroup->AddEnum( &m_dwActiveCameraIndex, L"Current Camera" );
    pGroup->AddFloatBounded( &m_fCameraZFar, L"Camera Z Far", 30.0f, 10000.0f, 250.0f );
    pGroup->AddFloatBounded( &m_fCameraMoveSpeed, L"Camera Movement Speed", 0.0f, 500.0f, 1.0f );
    pGroup->AddBoolean( &m_bAdaptiveCameraSpeed, L"Adaptive Camera Speed" );
    dwSettingIndex = pGroup->AddEnum( &m_dwCameraControlType, L"Camera Control Method" );
    pGroup->AddEnumEntry( dwSettingIndex, L"Yaw-Pitch-Roll", 0 );
    pGroup->AddEnumEntry( dwSettingIndex, L"Yaw-Pitch", 1 );

    //----------------------------------------------------------------------------------
    m_SettingsPanel.CreateNewGroup( L"Lighting" );
    pGroup = m_SettingsPanel.GetGroup();
    assert( pGroup != NULL );
    pGroup->AddBoolean( &m_bEnableLighting, L"Enable Lighting" );
    dwSettingIndex = pGroup->AddEnum( &m_dwDefaultLightRigIndex, L"Default Lighting Rig" );
    pGroup->AddEnumEntry( dwSettingIndex, L"None", 0 );
    pGroup->AddEnumEntry( dwSettingIndex, L"Single Directional Light", 1 );
    pGroup->AddEnumEntry( dwSettingIndex, L"Single Spotlight", 2 );
    pGroup->AddEnumEntry( dwSettingIndex, L"3 Point Lights", 3 );
    pGroup->AddFloatBounded( &m_fAmbient, L"Ambient Light", 0.0f, 1.0f, 0.25f );
    pGroup->AddFloatBounded( &m_fLightRangeScale, L"Light Range Scale", 0.001f, 100.0f, 0.05f );
    pGroup->AddFloatBounded( &m_fLightIntensityScale, L"Light Intensity Scale", 0.001f, 100.0f, 0.1f );
    pGroup->AddSeparator();
    pGroup->AddIntegerBounded( &m_iShowShadowMap, L"Show Shadow Map", -1, 7 );
    pGroup->AddFloatBounded( &m_fShadowSlopedDepthBias, L"Shadow Sloped Depth Bias", -10.0f, 10.0f, 0.1f );
    pGroup->AddBoolean( &m_bEnableShadowUpdates, L"Shadow Updates Enabled" );
    pGroup->AddBoolean( &m_bReduceShadowShimmer, L"Reduce Shadow Shimmer" );
    pGroup->AddBoolean( &m_bSortLightsByDistance, L"Sort Lights By Distance" );
    dwSettingIndex = pGroup->AddEnum( &m_dwShadowMapSize, L"Shadow Map Size" );
    pGroup->AddEnumEntry( dwSettingIndex, L"128x128", 128 );
    pGroup->AddEnumEntry( dwSettingIndex, L"256x256", 256 );
    pGroup->AddEnumEntry( dwSettingIndex, L"512x512", 512 );
    pGroup->AddEnumEntry( dwSettingIndex, L"1024x1024", 1024 );
    pGroup->SetCriticalSection( dwSettingIndex, &m_RenderSettingsCriticalSection );
    dwSettingIndex = pGroup->AddBoolean( &m_bMipShadowMaps, L"Create Shadow Map Mips" );
    pGroup->SetCriticalSection( dwSettingIndex, &m_RenderSettingsCriticalSection );
    pGroup->AddFloatBounded( &m_fTightDirShadowRadius, L"Tight Directional Shadow Radius", 0.001f, 10000.0f, 1.0f );

    //----------------------------------------------------------------------------------
    m_SettingsPanel.CreateNewGroup( L"HUD" );
    pGroup = m_SettingsPanel.GetGroup();
    assert( pGroup != NULL );
    pGroup->AddBoolean( &m_bDisableAllUI, L"Disable All UI" );
    pGroup->AddFloatStepped( &m_fTextScalingFactor, L"Text Scaling Factor", 1.0f, 2.0f, 0.1f );
    pGroup->AddBoolean( &m_bDrawStats, L"Draw Statistics" );
    pGroup->AddBoolean( &m_bDrawMemoryStats, L"Draw Memory Statistics" );
    pGroup->AddBoolean( &m_bDrawSafeRect, L"Draw Television Safe Rectangle" );

    //----------------------------------------------------------------------------------
    m_SettingsPanel.CreateNewGroup( L"Animation" );
    pGroup = m_SettingsPanel.GetGroup();
    assert( pGroup != NULL );
    pGroup->AddFloatStepped( &m_fAnimationSpeed, L"Animation Speed", -10.0f, 10.0f, 0.2f );
    pGroup->AddBoolean( &m_bEnableAnimations, L"Animation System Enabled" );

    //----------------------------------------------------------------------------------
    m_SettingsPanel.CreateNewGroup( L"Rendering" );
    pGroup = m_SettingsPanel.GetGroup();
    assert( pGroup != NULL );
    pGroup->AddBoolean( &m_bWireframe, L"Wireframe" );
    pGroup->AddBoolean( &m_bDrawTransparentObjects, L"Draw Transparent Objects" );
    dwSettingIndex = pGroup->AddEnum( &m_dwTextureOverrideMode, L"Texture Override" );
    pGroup->AddEnumEntry( dwSettingIndex, L"Disabled", 0 );
    pGroup->AddEnumEntry( dwSettingIndex, L"No Specular Maps", 6 );
    pGroup->AddEnumEntry( dwSettingIndex, L"White (With Shadows & Normal Maps)", 2 );
    pGroup->AddEnumEntry( dwSettingIndex, L"White (With Shadows)", 3 );
    pGroup->AddEnumEntry( dwSettingIndex, L"White (With Normal Maps)", 5 );
    pGroup->AddEnumEntry( dwSettingIndex, L"White", 4 );
    pGroup->AddEnumEntry( dwSettingIndex, L"Black", 1 );

    dwSettingIndex = pGroup->AddEnum( &m_dwPostEffectTechniqueIndex, L"Post Effect" );
    pGroup->AddEnumEntry( dwSettingIndex, L"None", 0 );
    pGroup->AddEnumEntry( dwSettingIndex, L"Depth of Field (12-tap Poisson)", 1 );
    pGroup->AddEnumEntry( dwSettingIndex, L"Depth of Field (8-tap Poisson)", 2 );
    pGroup->AddEnumEntry( dwSettingIndex, L"Depth of Field (5-tap Poisson)", 3 );
    pGroup->AddEnumEntry( dwSettingIndex, L"Grayscale", 4 );
    pGroup->AddEnumEntry( dwSettingIndex, L"Edge Detect", 5 );
    pGroup->AddEnumEntry( dwSettingIndex, L"Edge Detect With Color", 6 );
    pGroup->SetCriticalSection( dwSettingIndex, &m_RenderSettingsCriticalSection );

    dwSettingIndex = pGroup->AddEnum( ( DWORD* )&m_ZPassMode, L"Z Pass" );
    pGroup->AddEnumEntry( dwSettingIndex, L"None", SVZP_NONE );
    pGroup->AddEnumEntry( dwSettingIndex, L"Direct3D Auto", SVZP_D3D );
    pGroup->AddEnumEntry( dwSettingIndex, L"Manual", SVZP_MANUAL );
    pGroup->SetCriticalSection( dwSettingIndex, &m_RenderSettingsCriticalSection );

    dwSettingIndex = pGroup->AddEnum( ( DWORD* )&m_RenderMode, L"Scene Rendering Mode" );
    pGroup->AddEnumEntry( dwSettingIndex, L"Default (Ubershader)", SVRM_NORMAL );
    pGroup->AddEnumEntry( dwSettingIndex, L"Shader Library", SVRM_SHADERLIB );
    pGroup->AddEnumEntry( dwSettingIndex, L"Pass Per Light", SVRM_PASSPERLIGHT );
    pGroup->AddEnumEntry( dwSettingIndex, L"Deferred Lighting", SVRM_DEFERRED );
    pGroup->SetCriticalSection( dwSettingIndex, &m_RenderSettingsCriticalSection );

    dwSettingIndex = pGroup->AddEnum( ( DWORD* )&m_TilingMode, L"Scene Tiling Mode" );
    pGroup->AddEnumEntry( dwSettingIndex, L"None", SVTM_NONE );
    pGroup->AddEnumEntry( dwSettingIndex, L"2x MSAA Horizontal Split", SVTM_720p2X_H );
    pGroup->AddEnumEntry( dwSettingIndex, L"2x MSAA Vertical Split", SVTM_720p2X_V );
    pGroup->AddEnumEntry( dwSettingIndex, L"4x MSAA 3 Tiles", SVTM_720p4X_3 );
    pGroup->AddEnumEntry( dwSettingIndex, L"4x MSAA 4 Tiles", SVTM_720p4X_4 );
    pGroup->SetCriticalSection( dwSettingIndex, &m_RenderSettingsCriticalSection );

    pGroup->AddBoolean( &m_bFrustumCulling, L"Frustum Culling" );
    pGroup->AddBoolean( &m_bLockVisibleSet, L"Lock Visible Set" );
    dwSettingIndex = pGroup->AddEnum( &m_dwTriangleCullingMode, L"Triangle Culling" );
    pGroup->AddEnumEntry( dwSettingIndex, L"NONE", D3DCULL_NONE );
    pGroup->AddEnumEntry( dwSettingIndex, L"CW", D3DCULL_CW );
    pGroup->AddEnumEntry( dwSettingIndex, L"CCW", D3DCULL_CCW );

    //----------------------------------------------------------------------------------
    m_SettingsPanel.CreateNewGroup( L"Debug Draw" );
    pGroup = m_SettingsPanel.GetGroup();
    assert( pGroup != NULL );
    pGroup->AddBoolean( &m_bDisplayBounds, L"Display Bounds" );
    pGroup->AddBoolean( &m_bDisplayLights, L"Display Lights" );
    pGroup->AddBoolean( &m_bDisplayFrames, L"Display Frames" );
    pGroup->AddBoolean( &m_bDisplayCameras, L"Display Cameras" );
    pGroup->AddFloatBounded( &m_fBoneRadius, L"Display Bones", 0.0f, 10.0f, 0.1f );
    pGroup->AddBoolean( &m_bDrawGroundPlane, L"Draw Ground Plane" );

    dwSettingIndex = pGroup->AddEnum( &m_dwDebugRenderMode, L"Debug Mesh Render Mode" );
    pGroup->AddEnumEntry( dwSettingIndex, L"Off", 0 );
    pGroup->AddEnumEntry( dwSettingIndex, L"White Wireframe", 1 );
    pGroup->AddEnumEntry( dwSettingIndex, L"White", 2 );
    pGroup->AddEnumEntry( dwSettingIndex, L"Overlay Wireframe", 3 );
    pGroup->AddEnumEntry( dwSettingIndex, L"Overlay Normals", 4 );
    pGroup->AddEnumEntry( dwSettingIndex, L"Transparent w/ Normals", 5 );
    pGroup->AddEnumEntry( dwSettingIndex, L"No Render", 100 );
    pGroup->SetCriticalSection( dwSettingIndex, &m_RenderSettingsCriticalSection );

    pGroup->AddFloatBounded( &m_fDebugNormalsScale, L"Debug Normals Scale", 0.0001f, 5.0f, 0.1f );
    pGroup->AddIntegerBounded( &m_iIsolatedModelIndex, L"Isolate Model", -1, 10000 );

    //----------------------------------------------------------------------------------
    m_SettingsPanel.CreateNewGroup( L"Performance" );
    pGroup = m_SettingsPanel.GetGroup();
    assert( pGroup != NULL );
    dwSettingIndex = pGroup->AddBoolean( &m_bDisplayPerfChart, L"Display Perf Chart" );
    pGroup->SetCriticalSection( dwSettingIndex, &m_RenderSettingsCriticalSection );
    dwSettingIndex = pGroup->AddBoolean( &m_bCapturePerfData, L"Capture Performance Data" );
    pGroup->SetCriticalSection( dwSettingIndex, &m_RenderSettingsCriticalSection );
    dwSettingIndex = pGroup->AddBoolean( &m_bIsolatePerfSections, L"Isolate Performance Sections" );
    pGroup->SetCriticalSection( dwSettingIndex, &m_RenderSettingsCriticalSection );

    //----------------------------------------------------------------------------------
    m_SettingsPanel.CreateNewGroup( L"Advanced" );
    pGroup = m_SettingsPanel.GetGroup();
    assert( pGroup != NULL );
    dwSettingIndex = pGroup->AddBoolean( &m_bRenderUpsideDown, L"Render Scene Upside Down" );
    pGroup->SetCriticalSection( dwSettingIndex, &m_RenderSettingsCriticalSection );
    pGroup->AddBoolean( &m_bDrawTilingStats, L"Draw Tiling Statistics" );
    pGroup->AddSeparator();
    pGroup->AddBoolean( &m_bStencilOptimization, L"Light Stencil Optimization" );
    pGroup->AddIntegerBounded( ( INT* )&m_dwMaxPointLightsToSet, L"Max Point Lights", 0, 6 );
    pGroup->AddIntegerBounded( ( INT* )&m_dwMaxSpotLightsToSet, L"Max Spot Lights", 0, 6 );
    pGroup->AddIntegerBounded( ( INT* )&m_dwMaxDirLightsToSet, L"Max Directional Lights", 0, 6 );
    dwSettingIndex = pGroup->AddEnum( ( DWORD* )&m_dwUbershaderTechniqueIndex, L"Ubershader Technique" );
    pGroup->AddEnumEntry( dwSettingIndex, L"Nested Branches", 0 );
    pGroup->AddEnumEntry( dwSettingIndex, L"Loop Unrolling", 1 );
    pGroup->AddSeparator();
    dwSettingIndex = pGroup->AddEnum( ( DWORD* )&m_d3dpp.PresentationInterval, L"D3D Presentation Interval" );
    pGroup->AddEnumEntry( dwSettingIndex, L"Immediate", D3DPRESENT_INTERVAL_IMMEDIATE );
    pGroup->AddEnumEntry( dwSettingIndex, L"One (60 Hz)", D3DPRESENT_INTERVAL_ONE );
    pGroup->AddEnumEntry( dwSettingIndex, L"Two (30 Hz)", D3DPRESENT_INTERVAL_TWO );
    pGroup->SetCriticalSection( dwSettingIndex, &m_RenderSettingsCriticalSection );
    pGroup->AddIntegerBounded( ( INT* )&m_dwSecondaryRingBufferSize, L"D3D Secondary Ring Buffer Size (KB)", 512,
                               64 * 1024, 256 );
    pGroup->AddBoolean( &D3D__NullHardware, L"D3D Null Hardware" );
#ifdef _DEBUG
    pGroup->AddBoolean( &D3D__SingleStepper, L"D3D Single Stepper" );
#endif
    pGroup->AddSeparator();
    pGroup->AddBoolean( &m_bShowDebugBuffers, L"Show Deferred Buffers" );



    m_SettingsPanel.SetCurrentGroup( 1 );


    // Initialize the XUI dialog (the file dialog).
    m_XuiApp.Initialize( m_pd3dDevice, &m_d3dpp );
}


VOID SceneViewer::UpdatePerfCounterUsage()
{
#ifndef _RELEASED3D
    if( m_bCapturePerfData )
    {
        DWORD dwFrameIndex = m_dwRenderThreadFrameCount % 2;
        DWORD dwOffset = dwFrameIndex * SVPE_SIZEOF;
        for( DWORD i = 0; i < SVPE_SIZEOF; ++i )
        {
            m_bPerfCounterUsed[ dwOffset + i ] = FALSE;
        }
    }

    static BOOL s_bPerfCountersEnabled = FALSE;
    if( m_bCapturePerfData == s_bPerfCountersEnabled )
        return;

    s_bPerfCountersEnabled = m_bCapturePerfData;

    if( m_bCapturePerfData )
    {
        m_pd3dDevice->EnablePerfCounters( TRUE );

        // Set up the perf counters we care about.
        D3DPERFCOUNTER_EVENTS PerfEvents;
        ZeroMemory( &PerfEvents, sizeof( D3DPERFCOUNTER_EVENTS ) );

        // CP clock cycles.
        PerfEvents.CP[0] = GPUPE_CP_COUNT;
        // NRT busy cycles.
        PerfEvents.RBBM[0] = GPUPE_RBBM_NRT_BUSY;
        // Texture cache reads.
        PerfEvents.MH[0] = GPUPE_TC0_READ;
        PerfEvents.MH[1] = GPUPE_TC1_READ;
        // SQ stuff.
        PerfEvents.SQ[0] = GPUPE_PIXEL_THREAD_0_ACTIVE;
        PerfEvents.SQ[1] = GPUPE_VERTEX_THREAD_0_ACTIVE;

        m_pd3dDevice->SetPerfCounterEvents( &PerfEvents, 0 );
    }
    else
    {
        m_pd3dDevice->EnablePerfCounters( FALSE );
    }
#endif
}


VOID SceneViewer::RecordPerfEvent( DWORD dwEventIndex, DWORD dwFlags )
{
#ifndef _RELEASED3D
    if( !m_bCapturePerfData )
        return;
    if( m_bIsolatePerfSections )
        dwFlags |= D3DPERFQUERY_WAITGPUIDLE;
    assert( dwEventIndex < SVPE_SIZEOF );
    DWORD dwFrameIndex = m_dwRenderThreadFrameCount % 2;
    dwEventIndex += ( dwFrameIndex * SVPE_SIZEOF );
    m_pd3dDevice->QueryPerfCounters( m_pPerfCounters[ dwEventIndex ], dwFlags );
    m_bPerfCounterUsed[ dwEventIndex ] = TRUE;
#endif
}


BOOL SceneViewer::GetPerfValues( DWORD dwEventIndex, D3DPERFCOUNTER_VALUES& Values )
{
#ifndef _RELEASED3D
    assert( dwEventIndex < SVPE_SIZEOF );
    DWORD dwFrameIndex = ( m_dwRenderThreadFrameCount + 1 ) % 2;
    dwEventIndex += ( dwFrameIndex * SVPE_SIZEOF );
    m_pPerfCounters[dwEventIndex]->GetValues( &Values, 0, NULL );
    return m_bPerfCounterUsed[dwEventIndex];
#else
    return FALSE;
#endif
}


//--------------------------------------------------------------------------------------
// Name: AcquireD3D
// Desc: Acquires a lock on the Direct3D device using a critical section.
//       This handoff is required since multiple threads could potentially need to use
//       the Direct3D device.
//--------------------------------------------------------------------------------------
VOID SceneViewer::AcquireD3D()
{
    // If we call AcquireD3D() recursively from the same thread, it's OK, since
    // EnterCriticalSection cannot deadlock itself from the same thread.
    // Also, it's OK to call AcquireThreadOwnership() on the D3D device multiple times
    // from the same thread.
    EnterCriticalSection( &m_Direct3DCriticalSection );
    m_pd3dDevice->AcquireThreadOwnership();
}


//--------------------------------------------------------------------------------------
// Name: ReleaseD3D
// Desc: Releases a lock on the Direct3D device using a critical section.
//--------------------------------------------------------------------------------------
VOID SceneViewer::ReleaseD3D()
{
    // RecursionCount should be greater than 0 if we have already called AcquireD3D().
    assert( m_Direct3DCriticalSection.RecursionCount > 0 );

    // If the RecursionCount member of the critical section is 1, we need to release
    // ownership of the D3D device.
    if( m_Direct3DCriticalSection.RecursionCount == 1 )
    {
        m_pd3dDevice->ReleaseThreadOwnership();
    }
    LeaveCriticalSection( &m_Direct3DCriticalSection );
}


//--------------------------------------------------------------------------------------
// Name: AcquireScene
// Desc: Acquires a lock on the scene using a critical section.
//       This handoff is required since multiple threads could potentially need to
//       modify the scene at the same time.
//--------------------------------------------------------------------------------------
VOID SceneViewer::AcquireScene()
{
    EnterCriticalSection( &m_SceneCriticalSection );
}


//--------------------------------------------------------------------------------------
// Name: ReleaseScene
// Desc: Releases a lock on the scene using a critical section.
//--------------------------------------------------------------------------------------
VOID SceneViewer::ReleaseScene()
{
    LeaveCriticalSection( &m_SceneCriticalSection );
}

