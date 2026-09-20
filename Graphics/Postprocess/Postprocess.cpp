//-----------------------------------------------------------------------------
// Postprocess.cpp
//
// Sample of Postprocess system
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <d3dx9.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>


// Define a symbol that is used to compile out the use of the GPU performance counter APIs 
// when using a release build of Direct3D.  The GPU performance counter APIs only work with 
// d3d9i.lib and d3d9d.lib.
#if defined( NDEBUG ) && ( !defined( PROFILE ) || defined( FASTCAP ) )
#define _RELEASED3D
#endif


//--------------------------------------------------------------------------------------
// Global constants
//--------------------------------------------------------------------------------------
const static DWORD      g_dwGpuCyclesPerMs = GPU_CLOCK_SPEED / 1000;   // 500 MHz


//--------------------------------------------------------------------------------------
// Custom texture format into which we can losslessly resolve a 7e3 render target.
//--------------------------------------------------------------------------------------
static const D3DFORMAT  D3DFMT_A16B16G16R16_UNSIGNED_INTEGER = ( D3DFORMAT )MAKED3DFMT2(
    GPUTEXTUREFORMAT_16_16_16_16, GPUENDIAN_8IN16, TRUE, GPUSIGN_UNSIGNED,
    GPUSIGN_UNSIGNED, GPUSIGN_UNSIGNED, GPUSIGN_UNSIGNED, GPUNUMFORMAT_INTEGER,
    GPUSWIZZLE_X, GPUSWIZZLE_Y, GPUSWIZZLE_Z, GPUSWIZZLE_W );

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
    { ATG::HELP_LEFT_SHOULDER,  ATG::HELP_PLACEMENT_2, L"Cycle\ntest left" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Cycle\ntest right" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_2, L"Cycle method\nleft/right" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle\nrender grid"  },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Names of the Postprocess tests, as enum and narrow and wide char.
//--------------------------------------------------------------------------------------
enum POSTPROCESS_TEST
{
    POSTPROCESS_TEST_4X4_DOWNSAMPLE,
    POSTPROCESS_TEST_FRAMEBUFFER_AVG,
    POSTPROCESS_TEST_4X4_BLUR,
    POSTPROCESS_TEST_5X5_BLUR,
    POSTPROCESS_TEST_5X5_BLUR_7E3,
    POSTPROCESS_TEST_GENERIC_BLUR,
    POSTPROCESS_TEST_MAX,
    POSTPROCESS_TEST_FORCEDWORD = 0xffffffff,
};


static const char*      g_PostprocessTestPixNames[POSTPROCESS_TEST_MAX] =
{
    "4x4 downsample",      //  POSTPROCESS_TEST_4X4_DOWNSAMPLE,
    "frame buffer avg",    //  POSTPROCESS_TEST_FRAMEBUFFER_AVG,
    "4x4 blur",            //  POSTPROCESS_TEST_4X4_BLUR,
    "5x5 blur",            //  POSTPROCESS_TEST_5X5_BLUR,
    "5x5 blur 7e3",        //  POSTPROCESS_TEST_5X5_BLUR_7E3,
    "generic blur",        //  POSTPROCESS_TEST_GENERIC_BLUR, 
};


static const WCHAR*     g_PostprocessTestNames[POSTPROCESS_TEST_MAX] =
{
    L"4x4 downsample",      //  POSTPROCESS_TEST_4X4_DOWNSAMPLE,
    L"frame buffer avg",    //  POSTPROCESS_TEST_FRAMEBUFFER_AVG,
    L"4x4 blur",            //  POSTPROCESS_TEST_4X4_BLUR,
    L"5x5 blur",            //  POSTPROCESS_TEST_5X5_BLUR,
    L"5x5 blur 7e3",        //  POSTPROCESS_TEST_5X5_BLUR_7E3,
    L"generic blur",        //  POSTPROCESS_TEST_GENERIC_BLUR, 
};


//--------------------------------------------------------------------------------------
// Names of the Postprocess methods, as enum and narrow and wide char.
//--------------------------------------------------------------------------------------
enum POSTPROCESS_METHOD
{
    POSTPROCESS_METHOD_NAIVE,
    POSTPROCESS_METHOD_TUNED,
    POSTPROCESS_METHOD_IDEAL,
    POSTPROCESS_METHOD_MAX,
    POSTPROCESS_METHOD_FORCEDWORD = 0xffffffff,
};


static const char*      g_PostprocessMethodPixNames[POSTPROCESS_METHOD_MAX] =
{
    "naive",           //  POSTPROCESS_METHOD_NAIVE,
    "tuned",           //  POSTPROCESS_METHOD_TUNED,
    "ideal",           //  POSTPROCESS_METHOD_IDEAL,
};


static const WCHAR*     g_PostprocessMethodNames[POSTPROCESS_METHOD_MAX] =
{
    L"naive",           //  POSTPROCESS_METHOD_NAIVE,
    L"tuned",           //  POSTPROCESS_METHOD_TUNED,
    L"ideal",           //  POSTPROCESS_METHOD_IDEAL,
};


//--------------------------------------------------------------------------------------
// Name: lerp
// Desc: Linear interpolation.  Assumes operator of the form:
//       type_t operator*( type_t, FLOAT )
//--------------------------------------------------------------------------------------
template <typename type_t> static inline type_t lerp( type_t a, type_t b, FLOAT t )
{
    return a * t + b * ( 1 - t );
}


//-----------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//-----------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Font m_Font;                     // Font for drawing text
    ATG::Timer m_Timer;                    // Timer
    ATG::Help m_Help;                     // Display help
    BOOL m_bDrawHelp;

    ATG::PackedResource m_xprResource;              // Packed resources (textures)
    D3DTexture* m_pBackgroundTexture;       // Background image

    // Which Postprocess test and which method are active
    DWORD m_dwTest;
    DWORD m_dwMethod;

    DWORD m_bUseQuadGrid;

    // GPU performance counters (debug/profile builds only)
    D3DPerfCounters* m_pPerfCounterStart[3];
    D3DPerfCounters* m_pPerfCounterEnd[3];
    DWORD m_dwFrameCount;

    // Resources for the postprocess tests
    static const DWORD m_dwNumScratchLevels = 11;  // Accommodates 'mips' down to 1x1
    IDirect3DSurface9* m_pScratchRenderTargets[m_dwNumScratchLevels];
    IDirect3DSurface9* m_pScratchRenderTargets4xMSAA[m_dwNumScratchLevels];
    IDirect3DTexture9* m_pScratchTextures[m_dwNumScratchLevels];
    D3DFORMAT m_d3dfmtScratchRenderTargets;
    LONG m_dwRenderTargetColorExpBias;
    D3DFORMAT m_d3dfmtScratchTextures;
    LONG m_dwTextureColorExpBias;
    DWORD m_dwMaxLevels;
    DWORD m_dwMaxLevelsAlignedTo4x4;

    // Prebuilt vertex buffers for texture-cache-efficient fullscreen Draws
    IDirect3DVertexBuffer9* m_pCopyVBs[m_dwNumScratchLevels];
    DWORD   m_pCopyVBSizes[m_dwNumScratchLevels];
    IDirect3DVertexDeclaration9* m_pCopyVtxDecl;

    // Shaders for the postprocess tests
    IDirect3DVertexShader9* m_pCopyTextureVS;
    IDirect3DPixelShader9* m_pCopyTexturePS;
    IDirect3DPixelShader9* m_pDownScale4x4PS;
    IDirect3DPixelShader9* m_pBlur4x4NaiveHorizontalPS;
    IDirect3DPixelShader9* m_pBlur4x4NaiveVerticalPS;
    IDirect3DPixelShader9* m_pBlur4x4TunedPS;
    IDirect3DPixelShader9* m_pBlur4x4IdealPS;
    IDirect3DPixelShader9* m_pBlur5x5NaiveHorizontalPS;
    IDirect3DPixelShader9* m_pBlur5x5NaiveVerticalPS;
    IDirect3DPixelShader9* m_pBlur5x5TunedPS;
    IDirect3DPixelShader9* m_pBlur5x5IdealPS;

    // Copying data between RAM and EDRAM with different options
    VOID    CopyTextureToRenderTarget(
IDirect3DTexture9* pSrcTexture,
IDirect3DSurface9* pDstRenderTarget,
IDirect3DPixelShader9* pPixelShader = NULL );
    VOID    CopyRenderTargetToTexture(
IDirect3DSurface9* pSrcRenderTarget,
IDirect3DTexture9* pDstTexture );

    // The various postprocess test cases:
    // Return value is the index of the output scratch render target 
    DWORD   Test4x4Downsample();
    DWORD   TestFramebufferAvg();
    DWORD   Test4x4Blur();
    DWORD   Test5x5Blur();
    DWORD   TestGenericBlur();

    // Perf counter handling
    VOID    PerfCounterInit();
    VOID    PerfCounterDebugRender();

    // Build the resources which depend on which test is active
    HRESULT CreateResources();

public:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
};


//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.m_d3dpp.EnableAutoDepthStencil = FALSE;
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: PerfCounterInit()
// Desc: Set up the perf counters we plan to capture
//--------------------------------------------------------------------------------------
VOID Sample::PerfCounterInit()
{
#ifndef _RELEASED3D
    // Set up GPU performance counter structures.
    for( DWORD i = 0; i < 3; ++i )
    {
        m_pd3dDevice->CreatePerfCounters( &m_pPerfCounterStart[i], 1 );
        m_pd3dDevice->CreatePerfCounters( &m_pPerfCounterEnd[i], 1 );
    }
    m_pd3dDevice->EnablePerfCounters( TRUE );

    // Enable the performance counters we care about.
    D3DPERFCOUNTER_EVENTS PerfEvents;
    ZeroMemory( &PerfEvents, sizeof( D3DPERFCOUNTER_EVENTS ) );
    // CP clock cycles.
    PerfEvents.CP[0] = GPUPE_CP_COUNT;
    // NRT busy cycles.
    PerfEvents.RBBM[0] = GPUPE_RBBM_NRT_BUSY;
    m_pd3dDevice->SetPerfCounterEvents( &PerfEvents, 0 );
#endif
}


//--------------------------------------------------------------------------------------
// Name: CreateResources()
// Desc: Create/Recreate the D3D resources whose specifications depend on which 
// test is active.
//--------------------------------------------------------------------------------------
HRESULT Sample::CreateResources()
{
    // How many scratch textures do we need if they start at frame buffer size,
    // decrease by 2x in width and height at each level, and end up as 1x1.
    DWORD dwMaxDim = max( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight );
    m_dwMaxLevels = ( DWORD )( logf( ( FLOAT )dwMaxDim ) / logf( 2.0f ) ) + 1;
    assert( m_dwMaxLevels <= ARRAYSIZE( m_pScratchTextures ) );

    // How many of the scratch textures have dimensions which are multiples of 
    // 2x2?  of 4x4?
    DWORD dwMaxLevelsAlignedTo2x2 = 0;
    DWORD w = m_d3dpp.BackBufferWidth;
    DWORD h = m_d3dpp.BackBufferHeight;
    while( !( w & 1 ) && !( h & 1 ) )
    {
        ++dwMaxLevelsAlignedTo2x2;
        w >>= 1;
        h >>= 1;
    }
    m_dwMaxLevelsAlignedTo4x4 = dwMaxLevelsAlignedTo2x2 - 1;

    // Create the render target at base address 0
    D3DSURFACE_PARAMETERS SurfaceParameters = {0};
    SurfaceParameters.ColorExpBias = m_dwRenderTargetColorExpBias;

    // Create scratch textures and render targets of various sizes down to 1x1.
    DWORD dwWidth = m_d3dpp.BackBufferWidth;
    DWORD dwHeight = m_d3dpp.BackBufferHeight;
    for( DWORD i = 0; i < m_dwMaxLevels; ++i )
    {
        if( m_pScratchTextures[i] != NULL )
        {
            m_pScratchTextures[i]->Release();
        }

        if( FAILED( m_pd3dDevice->CreateTexture( dwWidth, dwHeight, 1, 0,
                                                 m_d3dfmtScratchTextures, D3DPOOL_DEFAULT, &m_pScratchTextures[i],
                                                 NULL ) ) )
            return E_FAIL;
        m_pScratchTextures[i]->Format.ExpAdjust = m_dwTextureColorExpBias;

        if( m_pScratchRenderTargets[i] != NULL )
        {
            m_pScratchRenderTargets[i]->Release();
        }
        m_pd3dDevice->CreateRenderTarget( dwWidth, dwHeight, m_d3dfmtScratchRenderTargets,
                                          D3DMULTISAMPLE_NONE, 0, FALSE, &m_pScratchRenderTargets[i],
                                          &SurfaceParameters );

        dwWidth = max( 1, dwWidth >> 1 );
        dwHeight = max( 1, dwHeight >> 1 );

        if( m_pScratchRenderTargets4xMSAA[i] != NULL )
        {
            m_pScratchRenderTargets4xMSAA[i]->Release();
        }
        m_pd3dDevice->CreateRenderTarget( dwWidth, dwHeight, m_d3dfmtScratchRenderTargets,
                                          D3DMULTISAMPLE_4_SAMPLES, 0, FALSE, &m_pScratchRenderTargets4xMSAA[i],
                                          &SurfaceParameters );

        // The scratch render targets of different levels are non-overlapping.
        // The first one must lie at 0, to alias the back buffer.
        SurfaceParameters.Base += XGSurfaceSize( dwWidth, dwHeight, m_d3dfmtScratchRenderTargets,
                                                 D3DMULTISAMPLE_4_SAMPLES );
    }


    // When drawing a full-screen quad, trial-n-error shows that using a grid can be better
    // due to the GPU's rasterization rules and minimizing texture cache misses
    DWORD dwQuadGridSizeX = ( m_bUseQuadGrid
                              ? ( XGNextMultiple( m_d3dpp.BackBufferWidth, 160 ) / 160 )
                              : 1 );
    DWORD dwQuadGridSizeY = 1;
    for( DWORD i = 0; i < ARRAYSIZE( m_pCopyVBs ); ++i )
    {
        if( m_pCopyVBs[i] != NULL )
        {
            m_pCopyVBs[i]->Release();
        }

        // Create a vertex buffer for screen-space effects
        m_pCopyVBSizes[i] = dwQuadGridSizeX * dwQuadGridSizeY;
        m_pd3dDevice->CreateVertexBuffer( 3 * m_pCopyVBSizes[i] * sizeof( XMFLOAT4 ),
                                          0L, 0L, D3DPOOL_DEFAULT, &m_pCopyVBs[i], NULL );

        XMFLOAT4* v;
        m_pCopyVBs[i]->Lock( 0, 0, ( VOID** )&v, 0 );

        FLOAT fGridDimX = 2.0f / ( FLOAT )dwQuadGridSizeX;
        FLOAT fGridDimY = 2.0f / ( FLOAT )dwQuadGridSizeY;
        FLOAT fGridDimU = 1.0f / ( FLOAT )dwQuadGridSizeX;
        FLOAT fGridDimV = 1.0f / ( FLOAT )dwQuadGridSizeY;
        FLOAT T = +1.0f;
        FLOAT V0 = 0.0f;
        for( DWORD y = 0; y < dwQuadGridSizeY; y++ )
        {
            FLOAT L = -1.0f;
            FLOAT U0 = 0.0f;
            for( DWORD x = 0; x < dwQuadGridSizeX; x++ )
            {
                FLOAT R = L + fGridDimX;
                FLOAT B = T - fGridDimY;
                FLOAT U1 = U0 + fGridDimU;
                FLOAT V1 = V0 + fGridDimV;

                *v++ = XMFLOAT4( L, T, U0, V0 ); // x, y, tu, tv
                *v++ = XMFLOAT4( R, T, U1, V0 ); // x, y, tu, tv
                *v++ = XMFLOAT4( L, B, U0, V1 ); // x, y, tu, tv
                L += fGridDimX;
                U0 += fGridDimU;
            }
            T -= fGridDimY;
            V0 += fGridDimV;
        }
        m_pCopyVBs[i]->Unlock();

        dwQuadGridSizeX = max( 1, dwQuadGridSizeX >> 1 );
        dwQuadGridSizeY = max( 1, dwQuadGridSizeY >> 1 );
    }
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects.
//-----------------------------------------------------------------------------
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

    // Create the resources
    if( FAILED( m_xprResource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the background image
    m_pBackgroundTexture = m_xprResource.GetTexture( "PTC_TestImage.bmp" );
    if( m_pBackgroundTexture == NULL )
        return ATGAPPERR_MEDIANOTFOUND;

    m_d3dfmtScratchTextures = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8B8G8R8 );
    m_d3dfmtScratchRenderTargets = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8B8G8R8 );
    m_dwRenderTargetColorExpBias = 0;
    m_dwTextureColorExpBias = 0;
    m_bUseQuadGrid = TRUE;

    for( DWORD i = 0; i < ARRAYSIZE( m_pScratchTextures ); ++i )
    {
        m_pScratchTextures[i] = NULL;
        m_pScratchRenderTargets[i] = NULL;
        m_pScratchRenderTargets4xMSAA[i] = NULL;
        m_pCopyVBs[i] = NULL;
    }

    HRESULT hr;
    if( FAILED( hr = CreateResources() ) )
    {
        return hr;
    }

    // Create common vertex declaration used by all the screen-space effects
    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    m_pd3dDevice->CreateVertexDeclaration( decl, &m_pCopyVtxDecl );

    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ScreenSpaceShader.xvu",
                                       &m_pCopyTextureVS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\CopyTexture.xpu",
                                      &m_pCopyTexturePS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DownScale4x4.xpu",
                                      &m_pDownScale4x4PS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\Blur4x4NaiveHorizontal.xpu",
                                      &m_pBlur4x4NaiveHorizontalPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\Blur4x4NaiveVertical.xpu",
                                      &m_pBlur4x4NaiveVerticalPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\Blur4x4Tuned.xpu",
                                      &m_pBlur4x4TunedPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\Blur4x4Ideal.xpu",
                                      &m_pBlur4x4IdealPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\Blur5x5NaiveHorizontal.xpu",
                                      &m_pBlur5x5NaiveHorizontalPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\Blur5x5NaiveVertical.xpu",
                                      &m_pBlur5x5NaiveVerticalPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\Blur5x5Tuned.xpu",
                                      &m_pBlur5x5TunedPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\Blur5x5Ideal.xpu",
                                      &m_pBlur5x5IdealPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    PerfCounterInit();

    // Set default render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    m_dwTest = 0;
    m_dwMethod = 0;

    m_dwFrameCount = 0;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    BOOL bRecreateResources = FALSE;

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Switch test
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
        m_dwTest = ( m_dwTest == 0 ) ? ( POSTPROCESS_TEST_MAX - 1 ) : ( m_dwTest - 1 );
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
        m_dwTest = ( m_dwTest == ( POSTPROCESS_TEST_MAX - 1 ) ) ? 0 : ( m_dwTest + 1 );

    // Switch method
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        m_dwMethod = ( m_dwMethod == 0 ) ? ( POSTPROCESS_METHOD_MAX - 1 ) : ( m_dwMethod - 1 );
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        m_dwMethod = ( m_dwMethod == ( POSTPROCESS_METHOD_MAX - 1 ) ) ? 0 : ( m_dwMethod + 1 );

    if( pGamepad->wPressedButtons &
        ( XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER
          | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT ) )
    {
        if( m_dwTest == POSTPROCESS_TEST_5X5_BLUR_7E3 )
        {
            m_d3dfmtScratchRenderTargets = D3DFMT_A2B10G10R10F_EDRAM;
            switch( m_dwMethod )
            {
                case POSTPROCESS_METHOD_NAIVE:
                    m_d3dfmtScratchTextures = D3DFMT_A16B16G16R16F;
                    m_dwTextureColorExpBias = 0;
                    m_dwRenderTargetColorExpBias = 0;
                    break;

                case POSTPROCESS_METHOD_TUNED:
                    m_d3dfmtScratchTextures = D3DFMT_A16B16G16R16F_EXPAND;
                    m_dwTextureColorExpBias = 0;
                    m_dwRenderTargetColorExpBias = 0;
                    break;

                case POSTPROCESS_METHOD_IDEAL:
                    m_d3dfmtScratchTextures = D3DFMT_A16B16G16R16_UNSIGNED_INTEGER;
                    m_dwTextureColorExpBias = -16;      // Convert from 16-bit int to [0,1)
                    m_dwRenderTargetColorExpBias = 5;   // Convert from [0,1} to [0,32) RT range
                    break;
            }
        }
        else
        {
            m_d3dfmtScratchRenderTargets = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8B8G8R8 );
            m_dwTextureColorExpBias = 0;
            m_dwRenderTargetColorExpBias = 0;
            m_d3dfmtScratchTextures = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8B8G8R8 );
        }

        bRecreateResources = TRUE;
    }

    // Toggle whether we render by a single rect or a grid
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_bUseQuadGrid = !m_bUseQuadGrid;
        bRecreateResources = TRUE;
    }

    if( bRecreateResources )
    {
        HRESULT hr;
        if( FAILED( hr = CreateResources() ) )
        {
            return hr;
        }
    }


    // Update frame count.
    ++m_dwFrameCount;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CopyTextureToRenderTarget()
// Desc: Draw a full texture into a render target, using either a custom shader or a 
// simple copy.
//--------------------------------------------------------------------------------------
VOID Sample::CopyTextureToRenderTarget(
IDirect3DTexture9* pSrcTexture,
IDirect3DSurface9* pDstRenderTarget,
IDirect3DPixelShader9* pPixelShader )
{
    // Make sure that the required resources exist
    assert( pSrcTexture );
    assert( pDstRenderTarget );

    if( NULL == pPixelShader )
        pPixelShader = m_pCopyTexturePS;

    // Make sure that the required shaders and objects exist
    assert( pPixelShader );

    // Query stats for the src and dst
    XGTEXTURE_DESC SrcDesc;
    XGGetTextureDesc( pSrcTexture, 0, &SrcDesc );
    XGTEXTURE_DESC DstDesc;
    XGGetSurfaceDesc( pDstRenderTarget, &DstDesc );

    // Which pre-built VB corresponds best to the source texture size?
    DWORD i = ( DWORD )logf( ( FLOAT )( m_d3dpp.BackBufferWidth / SrcDesc.Width ) );

    // Not exhaustive...
    BOOL bFilterable = ( SrcDesc.Format != D3DFMT_A16B16G16R16F );

    // Set all the right D3D state and resources
    m_pd3dDevice->SetStreamSource( 0, m_pCopyVBs[i], 0, sizeof( XMFLOAT4 ) );
    m_pd3dDevice->SetVertexDeclaration( m_pCopyVtxDecl );
    m_pd3dDevice->SetVertexShader( m_pCopyTextureVS );
    m_pd3dDevice->SetTexture( 0, pSrcTexture );
    m_pd3dDevice->SetPixelShader( pPixelShader );
    m_pd3dDevice->SetRenderTarget( 0L, pDstRenderTarget );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, bFilterable ? D3DTEXF_LINEAR : D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, bFilterable ? D3DTEXF_LINEAR : D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    // Draw the rect
    m_pd3dDevice->DrawPrimitive( D3DPT_RECTLIST, 0, m_pCopyVBSizes[i] );
}


//--------------------------------------------------------------------------------------
// Name: CopyRenderTargetToTexture()
// Desc: Resolve a full render target into a texture.
//--------------------------------------------------------------------------------------
VOID Sample::CopyRenderTargetToTexture(
IDirect3DSurface9* pSrcRenderTarget,
IDirect3DTexture9* pDstTexture )
{
    // Make sure that the required resources exist
    assert( pSrcRenderTarget );
    assert( pDstTexture );

    // Query stats for the src and dst
    XGTEXTURE_DESC SrcDesc;
    XGGetSurfaceDesc( pSrcRenderTarget, &SrcDesc );
    XGTEXTURE_DESC DstDesc;
    XGGetTextureDesc( pDstTexture, 0, &DstDesc );

    // Set all the right D3D state and resources
    m_pd3dDevice->SetRenderTarget( 0L, pSrcRenderTarget );

    // This custom format combination requires an exp bias.  The render target has 
    // range of 0,2^5 while the texture has range 0,2^16, so bias by +11.
    DWORD ColorExpBias = 0;
    if( SrcDesc.Format == D3DFMT_A2B10G10R10F_EDRAM
        && DstDesc.Format == D3DFMT_A16B16G16R16_UNSIGNED_INTEGER )
        ColorExpBias = ( DWORD )D3DRESOLVE_EXPONENTBIAS( +11 );

    // Resolve the render target
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | ColorExpBias, NULL, pDstTexture,
                           NULL, 0, 0, NULL, 1.0f, 0L, NULL );
}

//--------------------------------------------------------------------------------------
// Name: Test4x4Downsample()
// Desc: Downsample the framebuffer by 4 in each direction.
//--------------------------------------------------------------------------------------
DWORD Sample::Test4x4Downsample()
{
    switch( m_dwMethod )
    {
        case POSTPROCESS_METHOD_NAIVE:
        {
            // Steps: 
            //  a) Resolve
            //  b) Downsample 2x2 using a copy shader
            //  c) Resolve
            //  d) Downsample 2x2 using a copy shader
            CopyRenderTargetToTexture( m_pScratchRenderTargets[0], m_pScratchTextures[0] );
            CopyTextureToRenderTarget( m_pScratchTextures[0], m_pScratchRenderTargets[1] );
            CopyRenderTargetToTexture( m_pScratchRenderTargets[1], m_pScratchTextures[1] );
            CopyTextureToRenderTarget( m_pScratchTextures[1], m_pScratchRenderTargets[2] );
        }
            break;

        case POSTPROCESS_METHOD_TUNED:
        {
            // Steps: 
            //  a) Resolve
            //  b) Downsample 4x4 using a custom shader
            CopyRenderTargetToTexture( m_pScratchRenderTargets[0], m_pScratchTextures[0] );
            CopyTextureToRenderTarget( m_pScratchTextures[0], m_pScratchRenderTargets[2],
                                       m_pDownScale4x4PS );
        }
            break;

        case POSTPROCESS_METHOD_IDEAL:
        {
            // Steps: 
            //  a) Resolve, aliasing render target as 4xMSAA to get a free (but incorrect) 
            //      2x2 downsample
            //  b) Downsample 2x2 using a copy shader (magically compensates for the 
            //      incorrectness of previous step)
            CopyRenderTargetToTexture( m_pScratchRenderTargets4xMSAA[0], m_pScratchTextures[1] );
            CopyTextureToRenderTarget( m_pScratchTextures[1], m_pScratchRenderTargets[2] );
        }
            break;

        default:
            assert( false );    // unsupported test type
            break;
    }

    // Return value is the index of the output scratch render target 
    return 2;
}


//--------------------------------------------------------------------------------------
// Name: TestFramebufferAvg()
// Desc: Compute the average color value of the frame buffer.
//--------------------------------------------------------------------------------------
DWORD Sample::TestFramebufferAvg()
{
    switch( m_dwMethod )
    {
        case POSTPROCESS_METHOD_NAIVE:
        {
            // Steps: 
            //  a) Resolve
            //  b) Downsample 2x2 using a copy shader
            //  c) Repeat these steps until we have a 1x1 render target
            CopyRenderTargetToTexture( m_pScratchRenderTargets[0], m_pScratchTextures[0] );
            DWORD i = 0;
            for(; i < m_dwMaxLevels - 2; ++i )
            {
                CopyTextureToRenderTarget( m_pScratchTextures[i], m_pScratchRenderTargets[i + 1] );
                CopyRenderTargetToTexture( m_pScratchRenderTargets[i + 1], m_pScratchTextures[i + 1] );
            }
            CopyTextureToRenderTarget( m_pScratchTextures[i], m_pScratchRenderTargets[i + 1] );
        }
            break;

        case POSTPROCESS_METHOD_TUNED:
        {
            // Steps: 
            //  a) Resolve
            //  b) Downsample 4x4 using a custom shader
            //  c) Repeat these steps until texture dims are not a multiple of 4
            //  d) Proceed as in naive method
            // The requirement that texture dims be a multiple of 4 is so that we don't 
            // accidentally double-count due to clamping.  We could just as well turn on wrap
            // mode.
            CopyRenderTargetToTexture( m_pScratchRenderTargets[0], m_pScratchTextures[0] );
            DWORD i = 0;    // scratch level goes up by 2's in first loop, by 1's in second loop
            for(; i < m_dwMaxLevelsAlignedTo4x4; i += 2 )
            {
                CopyTextureToRenderTarget( m_pScratchTextures[i], m_pScratchRenderTargets[i + 2], m_pDownScale4x4PS );
                CopyRenderTargetToTexture( m_pScratchRenderTargets[i + 2], m_pScratchTextures[i + 2] );
            }
            for(; i < m_dwMaxLevels - 2; ++i )
            {
                CopyTextureToRenderTarget( m_pScratchTextures[i], m_pScratchRenderTargets[i + 1] );
                CopyRenderTargetToTexture( m_pScratchRenderTargets[i + 1], m_pScratchTextures[i + 1] );
            }
            CopyTextureToRenderTarget( m_pScratchTextures[i], m_pScratchRenderTargets[i + 1] );
        }
            break;

        case POSTPROCESS_METHOD_IDEAL:
        {
            // Steps: 
            //  a) Resolve
            //  b) Downsample 4x4 using a custom shader
            //  c) Repeat these steps until render target dims are not a multiple of 4
            //  d) Proceed as in naive method
            // The requirement that render target dims be a multiple of 4 is so that we don't 
            // bring in texels from outside the non-MSAA render target when we resolve.
            CopyRenderTargetToTexture( m_pScratchRenderTargets4xMSAA[0], m_pScratchTextures[1] );
            DWORD i = 1;    // scratch level goes up by 2's in first loop, by 1's in second loop
            for(; i + 1 < m_dwMaxLevelsAlignedTo4x4; i += 2 )
            {
                CopyTextureToRenderTarget( m_pScratchTextures[i], m_pScratchRenderTargets[i + 1] );
                CopyRenderTargetToTexture( m_pScratchRenderTargets4xMSAA[i + 1], m_pScratchTextures[i + 2] );
            }
            for(; i < m_dwMaxLevels - 2; ++i )
            {
                CopyTextureToRenderTarget( m_pScratchTextures[i], m_pScratchRenderTargets[i + 1] );
                CopyRenderTargetToTexture( m_pScratchRenderTargets[i + 1], m_pScratchTextures[i + 1] );
            }
            CopyTextureToRenderTarget( m_pScratchTextures[i], m_pScratchRenderTargets[i + 1] );
        }
            break;

        default:
            assert( false );    // unsupported test type
            break;
    }

    // Return value is the index of the output scratch render target 
    return m_dwMaxLevels - 1;
}


//--------------------------------------------------------------------------------------
// Name: Test4x4Blur()
// Desc: Apply a separable 4x4 blur filter to the frame buffer.
//--------------------------------------------------------------------------------------
DWORD Sample::Test4x4Blur()
{
    static FLOAT v4x4BlurKernelX[4] =
    {
        1.0f / 8.0f, 3.0f / 8.0f, 3.0f / 8.0f, 1.0f / 8.0f
    };
    static FLOAT v4x4BlurKernelY[4] =
    {
        1.0f / 8.0f, 3.0f / 8.0f, 3.0f / 8.0f, 1.0f / 8.0f
    };

    switch( m_dwMethod )
    {
        case POSTPROCESS_METHOD_NAIVE:
        {
            // Steps: 
            //  a) Resolve
            //  b) Blur in the X direction using 4 taps
            //  c) Resolve
            //  d) Blur in the Y direction using 4 taps
            CopyRenderTargetToTexture( m_pScratchRenderTargets[0], m_pScratchTextures[0] );

            m_pd3dDevice->SetPixelShaderConstantF( 12, v4x4BlurKernelX, 1 );
            CopyTextureToRenderTarget( m_pScratchTextures[0], m_pScratchRenderTargets[0],
                                       m_pBlur4x4NaiveHorizontalPS );
            CopyRenderTargetToTexture( m_pScratchRenderTargets[0], m_pScratchTextures[0] );

            m_pd3dDevice->SetPixelShaderConstantF( 12, v4x4BlurKernelY, 1 );
            CopyTextureToRenderTarget( m_pScratchTextures[0], m_pScratchRenderTargets[0], m_pBlur4x4NaiveVerticalPS );
        }
            break;

        case POSTPROCESS_METHOD_TUNED:
        {
            // Steps: 
            //  a) Resolve
            //  b) Blur in the X direction using 2 bilinear taps
            //  c) Resolve
            //  d) Blur in the Y direction using 2 bilinear taps
            CopyRenderTargetToTexture( m_pScratchRenderTargets[0], m_pScratchTextures[0] );

            {
                FLOAT fWeightX01 = v4x4BlurKernelX[0] / ( v4x4BlurKernelX[0] + v4x4BlurKernelX[1] );
                FLOAT fWeightX23 = v4x4BlurKernelX[2] / ( v4x4BlurKernelX[2] + v4x4BlurKernelX[3] );
                FLOAT v4x4BlurOffsets[4] =
                {
                    lerp( -2.0f, -1.0f, fWeightX01 ) / m_d3dpp.BackBufferWidth, 0.0f,
                    lerp( 0.0f, +1.0f, fWeightX23 ) / m_d3dpp.BackBufferWidth, 0.0f,
                };
                FLOAT v4x4BlurKernel[4] =
                {
                    v4x4BlurKernelX[0] + v4x4BlurKernelX[1],
                    v4x4BlurKernelX[2] + v4x4BlurKernelX[3],
                };
                m_pd3dDevice->SetPixelShaderConstantF( 12, v4x4BlurOffsets, 1 );
                m_pd3dDevice->SetPixelShaderConstantF( 13, v4x4BlurKernel, 1 );
                CopyTextureToRenderTarget( m_pScratchTextures[0], m_pScratchRenderTargets[0], m_pBlur4x4TunedPS );
                CopyRenderTargetToTexture( m_pScratchRenderTargets[0], m_pScratchTextures[0] );
            }

            {
                FLOAT fWeightY01 = v4x4BlurKernelY[0] / ( v4x4BlurKernelY[0] + v4x4BlurKernelY[1] );
                FLOAT fWeightY23 = v4x4BlurKernelY[2] / ( v4x4BlurKernelY[2] + v4x4BlurKernelY[3] );
                FLOAT v4x4BlurOffsets[4] =
                {
                    0.0f, lerp( -2.0f, -1.0f, fWeightY01 ) / m_d3dpp.BackBufferHeight,
                    0.0f, lerp( 0.0f, +1.0f, fWeightY23 ) / m_d3dpp.BackBufferHeight,
                };
                FLOAT v4x4BlurKernel[4] =
                {
                    v4x4BlurKernelY[0] + v4x4BlurKernelY[1],
                    v4x4BlurKernelY[2] + v4x4BlurKernelY[3],
                };
                m_pd3dDevice->SetPixelShaderConstantF( 12, v4x4BlurOffsets, 1 );
                m_pd3dDevice->SetPixelShaderConstantF( 13, v4x4BlurKernel, 1 );
                CopyTextureToRenderTarget( m_pScratchTextures[0], m_pScratchRenderTargets[0], m_pBlur4x4TunedPS );
            }
        }
            break;

        case POSTPROCESS_METHOD_IDEAL:
        {
            // Steps: 
            //  a) Resolve
            //  b) Blur in the X and Y directions using 4 bilinear taps
            CopyRenderTargetToTexture( m_pScratchRenderTargets[0], m_pScratchTextures[0] );

            FLOAT fWeightX01 = v4x4BlurKernelX[0] / ( v4x4BlurKernelX[0] + v4x4BlurKernelX[1] );
            FLOAT fWeightX23 = v4x4BlurKernelX[2] / ( v4x4BlurKernelX[2] + v4x4BlurKernelX[3] );
            FLOAT fWeightY01 = v4x4BlurKernelY[0] / ( v4x4BlurKernelY[0] + v4x4BlurKernelY[1] );
            FLOAT fWeightY23 = v4x4BlurKernelY[2] / ( v4x4BlurKernelY[2] + v4x4BlurKernelY[3] );
            FLOAT v4x4BlurOffsets[4] =
            {
                lerp( -2.0f, -1.0f, fWeightX01 ) / m_d3dpp.BackBufferWidth,
                lerp( 0.0f, +1.0f, fWeightX23 ) / m_d3dpp.BackBufferWidth,
                lerp( -2.0f, -1.0f, fWeightY01 ) / m_d3dpp.BackBufferHeight,
                lerp( 0.0f, +1.0f, fWeightY23 ) / m_d3dpp.BackBufferHeight,
            };
            FLOAT v4x4BlurKernel[4] =
            {
                v4x4BlurKernelX[0] * v4x4BlurKernelY[0] + v4x4BlurKernelX[0] * v4x4BlurKernelY[1]
                + v4x4BlurKernelX[1] * v4x4BlurKernelY[0] + v4x4BlurKernelX[1] * v4x4BlurKernelY[1],
                v4x4BlurKernelX[0] * v4x4BlurKernelY[2] + v4x4BlurKernelX[0] * v4x4BlurKernelY[3]
                + v4x4BlurKernelX[1] * v4x4BlurKernelY[2] + v4x4BlurKernelX[1] * v4x4BlurKernelY[3],
                v4x4BlurKernelX[2] * v4x4BlurKernelY[0] + v4x4BlurKernelX[2] * v4x4BlurKernelY[1]
                + v4x4BlurKernelX[3] * v4x4BlurKernelY[0] + v4x4BlurKernelX[3] * v4x4BlurKernelY[1],
                v4x4BlurKernelX[2] * v4x4BlurKernelY[2] + v4x4BlurKernelX[2] * v4x4BlurKernelY[3]
                + v4x4BlurKernelX[3] * v4x4BlurKernelY[2] + v4x4BlurKernelX[3] * v4x4BlurKernelY[3],
            };
            m_pd3dDevice->SetPixelShaderConstantF( 12, v4x4BlurOffsets, 1 );
            m_pd3dDevice->SetPixelShaderConstantF( 13, v4x4BlurKernel, 1 );
            CopyTextureToRenderTarget( m_pScratchTextures[0], m_pScratchRenderTargets[0], m_pBlur4x4IdealPS );
        }
            break;

        default:
            assert( false );    // unsupported test type
            break;
    }

    // Return value is the index of the output scratch render target 
    return 0;
}


//--------------------------------------------------------------------------------------
// Name: Test5x5Blur()
// Desc: Apply a separable 5x5 blur filter to the frame buffer.
//--------------------------------------------------------------------------------------
DWORD Sample::Test5x5Blur()
{
    static FLOAT v5x5BlurKernelX[5] =
    {
        1.0f / 16.0f, 4.0f / 16.0f, 6.0f / 16.0f, 4.0f / 16.0f, 1.0f / 16.0f
    };
    static FLOAT v5x5BlurKernelY[5] =
    {
        1.0f / 16.0f, 4.0f / 16.0f, 6.0f / 16.0f, 4.0f / 16.0f, 1.0f / 16.0f
    };

    switch( m_dwMethod )
    {
        case POSTPROCESS_METHOD_NAIVE:
        {
            // Steps: 
            //  a) Resolve
            //  b) Blur in the X direction using 5 taps
            //  c) Resolve
            //  d) Blur in the Y direction using 5 taps
            CopyRenderTargetToTexture( m_pScratchRenderTargets[0], m_pScratchTextures[0] );

            m_pd3dDevice->SetPixelShaderConstantF( 12, v5x5BlurKernelX, 2 );
            CopyTextureToRenderTarget( m_pScratchTextures[0], m_pScratchRenderTargets[0],
                                       m_pBlur5x5NaiveHorizontalPS );
            CopyRenderTargetToTexture( m_pScratchRenderTargets[0], m_pScratchTextures[0] );

            m_pd3dDevice->SetPixelShaderConstantF( 12, v5x5BlurKernelY, 2 );
            CopyTextureToRenderTarget( m_pScratchTextures[0], m_pScratchRenderTargets[0], m_pBlur5x5NaiveVerticalPS );
        }
            break;

        case POSTPROCESS_METHOD_TUNED:
        {
            // Steps: 
            //  a) Resolve
            //  b) Blur in the X direction using 3 bilinear taps
            //  c) Resolve
            //  d) Blur in the Y direction using 3 bilinear taps
            CopyRenderTargetToTexture( m_pScratchRenderTargets[0], m_pScratchTextures[0] );

            {
                FLOAT fWeight01 = v5x5BlurKernelX[0] / ( v5x5BlurKernelX[0] + v5x5BlurKernelX[1] );
                FLOAT fWeight34 = v5x5BlurKernelX[3] / ( v5x5BlurKernelX[3] + v5x5BlurKernelX[4] );
                FLOAT v5x5BlurOffsets[4] =
                {
                    lerp( -2.0f, -1.0f, fWeight01 ) / m_d3dpp.BackBufferWidth, 0.0f,
                    lerp( +1.0f, +2.0f, fWeight34 ) / m_d3dpp.BackBufferWidth, 0.0f,
                };
                FLOAT v5x5BlurKernel[4] =
                {
                    v5x5BlurKernelX[0] + v5x5BlurKernelX[1],
                    v5x5BlurKernelX[2],
                    v5x5BlurKernelX[3] + v5x5BlurKernelX[4],
                };
                m_pd3dDevice->SetPixelShaderConstantF( 12, v5x5BlurOffsets, 1 );
                m_pd3dDevice->SetPixelShaderConstantF( 13, v5x5BlurKernel, 1 );
                CopyTextureToRenderTarget( m_pScratchTextures[0], m_pScratchRenderTargets[0], m_pBlur5x5TunedPS );
                CopyRenderTargetToTexture( m_pScratchRenderTargets[0], m_pScratchTextures[0] );
            }

            {
                FLOAT fWeight01 = v5x5BlurKernelY[0] / ( v5x5BlurKernelY[0] + v5x5BlurKernelY[1] );
                FLOAT fWeight34 = v5x5BlurKernelY[3] / ( v5x5BlurKernelY[3] + v5x5BlurKernelY[4] );
                FLOAT v5x5BlurOffsets[4] =
                {
                    0.0f, lerp( -2.0f, -1.0f, fWeight01 ) / m_d3dpp.BackBufferHeight,
                    0.0f, lerp( +1.0f, +2.0f, fWeight34 ) / m_d3dpp.BackBufferHeight,
                };
                FLOAT v5x5BlurKernel[4] =
                {
                    v5x5BlurKernelY[0] + v5x5BlurKernelY[1],
                    v5x5BlurKernelY[2],
                    v5x5BlurKernelY[3] + v5x5BlurKernelY[4],
                };
                m_pd3dDevice->SetPixelShaderConstantF( 12, v5x5BlurOffsets, 1 );
                m_pd3dDevice->SetPixelShaderConstantF( 13, v5x5BlurKernel, 1 );
                CopyTextureToRenderTarget( m_pScratchTextures[0], m_pScratchRenderTargets[0], m_pBlur5x5TunedPS );
            }
        }
            break;

        case POSTPROCESS_METHOD_IDEAL:
        {
            // Steps: 
            //  a) Resolve
            //  b) Blur in the X direction using 2 bilinear taps and const blend
            //  c) Resolve
            //  d) Blur in the Y direction using 2 bilinear taps and const blend
            CopyRenderTargetToTexture( m_pScratchRenderTargets[0], m_pScratchTextures[0] );

            // Set up blend operation of (1 - constant) * src + constant * dst
            m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_HIGHPRECISIONBLENDENABLE, TRUE );   // only needed for the 7e3 case
            m_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
            m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_INVBLENDFACTOR );
            m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_BLENDFACTOR );

            {
                // Set blend constant to weight of current pixel in filter
                BYTE dwDestBlend = ( BYTE )( 255.0f * v5x5BlurKernelX[2] );
                D3DCOLOR d3dColorDestBlend = D3DCOLOR_ARGB( dwDestBlend, dwDestBlend, dwDestBlend, dwDestBlend );
                m_pd3dDevice->SetRenderState( D3DRS_BLENDFACTOR, d3dColorDestBlend );

                FLOAT fWeight01 = v5x5BlurKernelX[0] / ( v5x5BlurKernelX[0] + v5x5BlurKernelX[1] );
                FLOAT fWeight34 = v5x5BlurKernelX[3] / ( v5x5BlurKernelX[3] + v5x5BlurKernelX[4] );
                FLOAT v5x5BlurOffsets[4] =
                {
                    lerp( -2.0f, -1.0f, fWeight01 ) / m_d3dpp.BackBufferWidth, 0.0f,
                    lerp( +1.0f, +2.0f, fWeight34 ) / m_d3dpp.BackBufferWidth, 0.0f,
                };
                FLOAT fRemainingWeight = 1.0f - v5x5BlurKernelX[2];
                FLOAT v5x5BlurKernel[4] =
                {
                    ( v5x5BlurKernelX[0] + v5x5BlurKernelX[1] ) / fRemainingWeight,
                    0,
                    ( v5x5BlurKernelX[3] + v5x5BlurKernelX[4] ) / fRemainingWeight,
                };
                m_pd3dDevice->SetPixelShaderConstantF( 12, v5x5BlurOffsets, 1 );
                m_pd3dDevice->SetPixelShaderConstantF( 13, v5x5BlurKernel, 1 );

                CopyTextureToRenderTarget( m_pScratchTextures[0], m_pScratchRenderTargets[0], m_pBlur5x5IdealPS );
                CopyRenderTargetToTexture( m_pScratchRenderTargets[0], m_pScratchTextures[0] );
            }

            {
                // Set blend constant to weight of current pixel in filter
                BYTE dwDestBlend = ( BYTE )( 255.0f * v5x5BlurKernelY[2] );
                D3DCOLOR d3dColorDestBlend = D3DCOLOR_ARGB( dwDestBlend, dwDestBlend, dwDestBlend, dwDestBlend );
                m_pd3dDevice->SetRenderState( D3DRS_BLENDFACTOR, d3dColorDestBlend );

                FLOAT fWeight01 = v5x5BlurKernelY[0] / ( v5x5BlurKernelY[0] + v5x5BlurKernelY[1] );
                FLOAT fWeight34 = v5x5BlurKernelY[3] / ( v5x5BlurKernelY[3] + v5x5BlurKernelY[4] );
                FLOAT v5x5BlurOffsets[4] =
                {
                    0.0f, lerp( -2.0f, -1.0f, fWeight01 ) / m_d3dpp.BackBufferHeight,
                    0.0f, lerp( +1.0f, +2.0f, fWeight34 ) / m_d3dpp.BackBufferHeight,
                };
                FLOAT fRemainingWeight = 1.0f - v5x5BlurKernelY[2];
                FLOAT v5x5BlurKernel[4] =
                {
                    ( v5x5BlurKernelY[0] + v5x5BlurKernelY[1] ) / fRemainingWeight,
                    0,
                    ( v5x5BlurKernelY[3] + v5x5BlurKernelY[4] ) / fRemainingWeight,
                };

                m_pd3dDevice->SetPixelShaderConstantF( 12, v5x5BlurOffsets, 1 );
                m_pd3dDevice->SetPixelShaderConstantF( 13, v5x5BlurKernel, 1 );
                CopyTextureToRenderTarget( m_pScratchTextures[0], m_pScratchRenderTargets[0], m_pBlur5x5IdealPS );
            }

            m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
            m_pd3dDevice->SetRenderState( D3DRS_HIGHPRECISIONBLENDENABLE, FALSE );
        }
            break;

        default:
            assert( false );    // unsupported test type
            break;
    }

    // Return value is the index of the output scratch render target 
    return 0;
}


//--------------------------------------------------------------------------------------
// Name: TestGenericBlur()
// Desc: Compute a fast blur of the frame buffer, where the exact calcuation is 
//       negotiable.
//--------------------------------------------------------------------------------------
DWORD Sample::TestGenericBlur()
{
    switch( m_dwMethod )
    {
        case POSTPROCESS_METHOD_NAIVE:
            return Test5x5Blur();

        case POSTPROCESS_METHOD_TUNED:
            return Test5x5Blur();

        case POSTPROCESS_METHOD_IDEAL:
        {
            // Steps: 
            //  a) Resolve, aliasing render target as 4xMSAA to get a free (but incorrect) 
            //      2x2 downsample
            //  b) Downsample 2x2 using a copy shader (magically compensates for the 
            //      incorrectness of previous step)
            //  c) Blend downsampled image with original image by const blend
            CopyRenderTargetToTexture( m_pScratchRenderTargets4xMSAA[0], m_pScratchTextures[1] );
            CopyTextureToRenderTarget( m_pScratchTextures[1], m_pScratchRenderTargets[2] );
            CopyRenderTargetToTexture( m_pScratchRenderTargets[2], m_pScratchTextures[2] );

            // Set up blend operation of (1 - constant) * src + constant * dst
            m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_HIGHPRECISIONBLENDENABLE, TRUE );   // only needed for the 7e3 case
            m_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
            m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_INVBLENDFACTOR );
            m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_BLENDFACTOR );

            static FLOAT fBlurriness = 0.75f;
            BYTE dwDestBlend = ( BYTE )( 255.0f * ( 1.0f - fBlurriness ) );
            D3DCOLOR d3dColorDestBlend = D3DCOLOR_ARGB( dwDestBlend, dwDestBlend, dwDestBlend, dwDestBlend );
            m_pd3dDevice->SetRenderState( D3DRS_BLENDFACTOR, d3dColorDestBlend );

            CopyTextureToRenderTarget( m_pScratchTextures[2], m_pScratchRenderTargets[0] );

            m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
        }
            break;

        default:
            assert( false );    // unsupported test type
            break;
    }

    // Return value is the index of the output scratch render target 
    return 0;
}


//--------------------------------------------------------------------------------------
// Name: PerfCounterDebugRender()
// Desc: Print information from the perf counters to screen
//--------------------------------------------------------------------------------------
VOID Sample::PerfCounterDebugRender()
{
#ifndef _RELEASED3D
    WCHAR strText[256];
    D3DPERFCOUNTER_VALUES StartValues;
    m_pPerfCounterStart[ ( m_dwFrameCount + 1 ) % 3 ]->GetValues( &StartValues, 0, NULL );
    D3DPERFCOUNTER_VALUES EndValues;
    m_pPerfCounterEnd[ ( m_dwFrameCount + 1 ) % 3 ]->GetValues( &EndValues, 0, NULL );

    // Subtract start values from end values.
    UINT64* pStartValues = ( UINT64* )&StartValues;
    UINT64* pEndValues = ( UINT64* )&EndValues;
    const DWORD dwCount = sizeof( D3DPERFCOUNTER_VALUES ) / sizeof( UINT64 );
    for( DWORD i = 0; i < dwCount; ++i )
    {
        pEndValues[i] -= pStartValues[i];
    }

    FLOAT fYPos = m_d3dpp.BackBufferHeight - 300.f;

    m_Font.SetScaleFactors( 0.9f, 0.9f );
    swprintf_s( strText, L"GPU Perf Counters" );
    m_Font.DrawText( 0, fYPos, 0xFF8080FF, strText );
    fYPos += 20;

    // Display GPU busy cycle count for just the time of the test.
    swprintf_s( strText, L"GPU Cycles: %I64d", EndValues.RBBM[0].QuadPart );
    m_Font.DrawText( 0, fYPos, 0xFF8080FF, strText );
    fYPos += 20;
    swprintf_s( strText, L"GPU MS: %3.3f", ( FLOAT )EndValues.RBBM[0].QuadPart / ( FLOAT )g_dwGpuCyclesPerMs );
    m_Font.DrawText( 0, fYPos, 0xFF8080FF, strText );
#endif
}


//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, to render the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Render()
{
    IDirect3DSurface9* pBackBuffer;   // The automatic back buffer

    m_pd3dDevice->GetRenderTarget( 0, &pBackBuffer );

    // Clear the viewport
    D3DCOLOR D3D_BLACK = D3DCOLOR_ARGB( 0x00, 0x00, 0x00, 0x00 );
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                         D3D_BLACK, 1.0f, 0L );

    // Give almost all the GPRs to the pixel shader, since we plan to be bound on 
    // pixel shading.  (Doesn't help on the current tests.)
    m_pd3dDevice->SetShaderGPRAllocation( 0, 16, GPU_GPRS - 16 );

    // Draw Background
    PIXBeginNamedEvent( D3D_BLACK, "Draw Background" );
    CopyTextureToRenderTarget( m_pBackgroundTexture, m_pScratchRenderTargets[0] );
    PIXEndNamedEvent();

    // Will be the index of the output scratch render target 
    DWORD dwOutputLevel = 0;

    PIXBeginNamedEvent( D3D_BLACK, "%s: %s", g_PostprocessTestPixNames[m_dwTest],
                        g_PostprocessMethodPixNames[m_dwMethod] );

#ifndef _RELEASED3D
    m_pd3dDevice->QueryPerfCounters( m_pPerfCounterStart[ m_dwFrameCount % 3 ], D3DPERFQUERY_WAITGPUIDLE );
#endif

    switch( m_dwTest )
    {
        case POSTPROCESS_TEST_4X4_DOWNSAMPLE:
            dwOutputLevel = Test4x4Downsample();
            break;

        case POSTPROCESS_TEST_FRAMEBUFFER_AVG:
            dwOutputLevel = TestFramebufferAvg();
            break;

        case POSTPROCESS_TEST_4X4_BLUR:
            dwOutputLevel = Test4x4Blur();
            break;

        case POSTPROCESS_TEST_5X5_BLUR:
            dwOutputLevel = Test5x5Blur();
            break;

        case POSTPROCESS_TEST_5X5_BLUR_7E3:
            dwOutputLevel = Test5x5Blur();
            break;

        case POSTPROCESS_TEST_GENERIC_BLUR:
            dwOutputLevel = TestGenericBlur();
            break;

        default:
            assert( false );    // unsupported test type
            break;
    }

#ifndef _RELEASED3D
    m_pd3dDevice->QueryPerfCounters( m_pPerfCounterEnd[ m_dwFrameCount % 3 ], D3DPERFQUERY_WAITGPUIDLE );
#endif

    PIXEndNamedEvent();

    PIXBeginNamedEvent( D3D_BLACK, "Display test result" );

    CopyRenderTargetToTexture( m_pScratchRenderTargets[dwOutputLevel], m_pScratchTextures[dwOutputLevel] );
    CopyTextureToRenderTarget( m_pScratchTextures[dwOutputLevel], pBackBuffer );

    PIXEndNamedEvent();

    // Output statistics
    m_Timer.MarkFrame();

    m_pd3dDevice->SetRenderTarget( 0L, pBackBuffer );
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"Postprocess" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 40, 0xffffffff, L"Test:" );
        m_Font.DrawText( 100, 40, 0xffff00ff, g_PostprocessTestNames[m_dwTest] );
        m_Font.DrawText( 0, 70, 0xffffffff, L"Method:" );
        m_Font.DrawText( 100, 70, 0xffff00ff, g_PostprocessMethodNames[m_dwMethod] );
        m_Font.DrawText( 0, 100, 0xffffffff, L"Grid:" );
        m_Font.DrawText( 100, 100, 0xffff00ff, m_bUseQuadGrid ? L"On" : L"Off" );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        PerfCounterDebugRender();

        m_Font.End();
    }

    m_pd3dDevice->UnsetAll();

    // Restore the GPR allocations to the defaults. This must be done before
    // calling Present or Swap.
    m_pd3dDevice->SetShaderGPRAllocation( 0, 0, 0 );

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


