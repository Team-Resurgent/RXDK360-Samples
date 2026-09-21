//---------------------------------------------------------------------------------------------------------
// FastUntile.cpp
//
// XNA Developer Connection
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------
#include <xtl.h>    // must come before the others
#include <d3dx9.h>
#include <tracerecording.h>
#include <xgraphics.h>

#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>

#include "FastUntile.h"
#include "FastUntileCPU.h"
#include "FastUntileGPU.h"

// Allocate dynamic textures using 64 KB pages
#define FAST_UNTILE_USE_LARGE_PAGES

// Define a symbol that is used to compile out the use of the GPU performance counter APIs 
// when using a release build of Direct3D.  The GPU performance counter APIs only work with 
// d3d9i.lib and d3d9d.lib.
#if defined( NDEBUG ) && ( !defined( PROFILE ) || defined( FASTCAP ) )
#define _RELEASED3D
#endif


//---------------------------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//---------------------------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_2, L"Switch\ntexture"  },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_2, L"Cacheable vs.\nwrite-combined"  },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_2, L"Error\ncheck"  },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_2, L"PIX Trace\ncapture" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_2, L"Cycle method\nleft/right" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//---------------------------------------------------------------------------------------------------------
// enum TEST_TEXTURES:
// 
// Different source textures.  The only important feature for this sample is number of bytes per texel.
// We demonstrate the cases 4 and 2, which are those used by Kinect.  
//---------------------------------------------------------------------------------------------------------
enum TEST_TEXTURES
{
    TEST_TEXTURE_TESTIMAGE_32F, 
    TEST_TEXTURE_GEAR_BUMP_32F, 
    TEST_TEXTURE_TESTIMAGE, 
    TEST_TEXTURE_GEAR_BUMP, 
    TEST_TEXTURE_NUI_COLOR, 
    TEST_TEXTURE_NUI_DEPTH, 
    TEST_TEXTURE_NUI_LUMINANCE, 

    TEST_TEXTURE_COUNT
};

__declspec(selectany) 
const WCHAR* g_strTestTextureNames[] = 
{
    L"TestImage32F", 
    L"GearBump32F", 
    L"TestImage", 
    L"GearBump", 
    L"NuiColorSample", 
    L"NuiDepthStandIn", 
    L"NuiLuminanceSample", 
};
C_ASSERT( _countof( g_strTestTextureNames ) == TEST_TEXTURE_COUNT );


//-----------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//-----------------------------------------------------------------------------
class Sample : public ATG::Application
{
    // ATG helper items
    ATG::PackedResource             m_Resource;
    ATG::Font                       m_Font;                 // Font for drawing text
    ATG::Timer                      m_UntileTimer;          // Timer for untiling
    ATG::Help                       m_Help;                 // Display help

    // Sample options selectable from UI
    BOOL                            m_bDrawHelp;
    BOOL                            m_bTraceUntile;
    BOOL                            m_bBitwiseCompare;
    BOOL                            m_bCacheableDest;
    UINT                            m_iUntileMethod;
    UINT                            m_iTestTexture;

    // Actual sizes of texture allocations, for error checking
    UINT                            m_iSourceSize;
    UINT                            m_iDestSize;

    // Texture is in sRGB space, but of a format without sRGB support
    BOOL                            m_bFakesRGB;

    // Resources for test geometry
    IDirect3DVertexDeclaration9*    m_pVertexDecl;

    // Render targets
    IDirect3DSurface9*              m_pBackBuffer;
    IDirect3DSurface9*              m_pDummyNonsRGBBackBuffer;

    // Resources for untiling 
    IDirect3DTexture9*              m_pTiledTexture;            // Source
    IDirect3DTexture9*              m_pLinearTexture[2];        // Double-buffered dest

    // Shaders for rendering the sample
    IDirect3DVertexShader9*         m_pCopyTextureVS;
    IDirect3DPixelShader9*          m_pCopyTexturePS;
    IDirect3DPixelShader9*          m_pBitwiseCompareTexturePS;

    // Performance data
    D3DPerfCounters*                m_pPerfCounterStart[3];
    D3DPerfCounters*                m_pPerfCounterEnd[3];
    UINT                            m_iPerfCounterBufferIndex;
    const static UINT               m_iGpuCyclesPerMs = GPU_CLOCK_SPEED / 1000;   // 500 MHz
    FLOAT                           m_fUntileTimeInMS;
    FLOAT                           m_fSmoothedUntileTimeInMS[2];

    // Visualization helper for NUI depth format, luminance format
    inline VOID SetDummyNuiTexture( IDirect3DTexture9* pSourceTexture );

    // The custom untilers
    CPUUntiler                      m_CPUUntiler;
    GPUUntiler                      m_GPUUntiler;

    // Performance analysis
    VOID PerfCounterInit( );
    VOID BeginGpuTimer( );
    VOID EndGpuTimer( );
    VOID RenderPerf( );

    // Change of texture
    VOID LoadTestTexture( );

    // UI
    VOID RenderUI( );

public:
    Sample( ) {}

    HRESULT Initialize( );
    HRESULT Update( );
    HRESULT Render( );
};


//---------------------------------------------------------------------------------------------------------
// Name: Sample:PerfCounterInit( )
// Desc: Set up the perf counters we plan to capture
//---------------------------------------------------------------------------------------------------------
VOID Sample::PerfCounterInit( )
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

    m_iPerfCounterBufferIndex = 0;
#endif
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::LoadTestTexture( )
// Desc: Load a new texture.
//---------------------------------------------------------------------------------------------------------
VOID Sample::LoadTestTexture( )
{
    // Wait until it's okay to free all resources
    m_pd3dDevice->BlockUntilIdle();

    const WCHAR* strTextureNameW = g_strTestTextureNames[ m_iTestTexture ];
    CHAR strTextureName[MAX_PATH];
    WideCharToMultiByte( CP_ACP, 0, strTextureNameW, wcslen( strTextureNameW ) + 1, 
        strTextureName, MAX_PATH, NULL, NULL );

    // This texture is static data in the resource, so cannot be released via D3D
    //if( m_pTiledTexture != NULL )
    //{
    //    m_pTiledTexture->Release();
    //}
    m_pTiledTexture = m_Resource.GetTexture( strTextureName );

    XGTEXTURE_DESC SourceDesc;
    XGGetTextureDesc( m_pTiledTexture, 0, &SourceDesc );

    // Record the actual size of the texture allocation, for later error checking
    XGGetTextureLayout( m_pTiledTexture, NULL, &m_iSourceSize, NULL, NULL, 1, NULL, 
        NULL, NULL, NULL, 1 );

    // Special case to handle the 32:32:32:32F sample texture:
    //
    // If our texture is in sRGB space but is of a format without sRGB support, we also turn
    // off sRGB conversion in the back buffer to compensate.  This situation is unlikely to arise
    // in a real game.
    m_bFakesRGB = ( m_pTiledTexture->Format.DataFormat == GPUTEXTUREFORMAT_32_32_32_32_FLOAT );  // Not exhaustive

    for( UINT i = 0; i < 2; ++i )
    {
        // If we plan to access these textures much with the CPU, we should allocate
        // using 64 KB pages for best perf.  This is more involved though.

#ifdef FAST_UNTILE_USE_LARGE_PAGES

        if( m_pLinearTexture[i] != NULL )
        {
            if( m_pLinearTexture[i]->Format.BaseAddress != 0 )
            {
                XPhysicalFree( (VOID*) ( m_pLinearTexture[i]->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT ) );
            }
            delete m_pLinearTexture[i];
        }

        m_pLinearTexture[i] = new IDirect3DTexture9;
        assert( m_pLinearTexture[i] != NULL );

        // Figure out the max number of bytes required for padding the tiled dimensions 
        // according to the linear rules, because the Resolve method requires that we alias 
        // the same memory as both tiled and linear.

        UINT    uSizeLinear, uSizeTiled;

        XGSetTextureHeader( SourceDesc.Width, 
            XGNextMultiple( SourceDesc.Height, GPU_TEXTURE_TILE_DIMENSION ), 
            1, 
            0,
            (D3DFORMAT) MAKELINFMT( SourceDesc.Format ), 
            0, 
            0, 
            0, 
            0, 
            NULL, 
            &uSizeLinear, 
            NULL );

        XGSetTextureHeader( SourceDesc.Width, 
            XGNextMultiple( SourceDesc.Height, GPU_TEXTURE_TILE_DIMENSION ),
            1, 
            0,
            SourceDesc.Format, 
            0, 
            0, 
            0, 
            0, 
            NULL, 
            &uSizeTiled,
            NULL );

        // the allocation size should be large enough to accomodate either tiled or linear texture
        m_iDestSize = max( uSizeTiled, uSizeLinear );

        XGSetTextureHeader( SourceDesc.Width, 
            SourceDesc.Height, 
            1, 
            m_bCacheableDest ? D3DUSAGE_CPU_CACHED_MEMORY : 0, 
            (D3DFORMAT) MAKELINFMT( SourceDesc.Format ), 
            0, 
            0, 
            0, 
            0, 
            m_pLinearTexture[i], 
            NULL, 
            NULL );

        VOID* pData = XPhysicalAlloc( m_iDestSize, 
            MAXULONG_PTR, 
            GPU_TEXTURE_ALIGNMENT, 
            PAGE_READWRITE | MEM_LARGE_PAGES | ( m_bCacheableDest ? 0 : PAGE_WRITECOMBINE ) );
        assert( pData != NULL );

        XGOffsetBaseTextureAddress( m_pLinearTexture[i], pData, NULL );

#else

        if( m_pLinearTexture[i] != NULL )
        {
            m_pLinearTexture[i]->Release();
        }

        m_pd3dDevice->CreateTexture( SourceDesc.Width, 
            SourceDesc.Height, 
            1,
            m_bCacheableDest ? D3DUSAGE_CPU_CACHED_MEMORY : 0,
            (D3DFORMAT) MAKELINFMT( SourceDesc.Format ), 
            0, 
            &m_pLinearTexture[i], 
            NULL );

        XGGetTextureLayout( pSourceTexture, NULL, &m_iDestSize, NULL, NULL, 1, NULL, 
            NULL, NULL, NULL, 1 );

#endif
    }

    UINT iTexelPitch = SourceDesc.BytesPerBlock;

    // These do essentially the same thing, but we maintain two copies for modularity
    m_CPUUntiler.InitializeRemapping( iTexelPitch );
    m_GPUUntiler.InitializeRemapping( m_pd3dDevice, iTexelPitch );
}


//-----------------------------------------------------------------------------
// Name: Sample::Initialize( )
// Desc: Initialize app-dependent objects.
//-----------------------------------------------------------------------------
HRESULT Sample::Initialize( )
{
    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG::FatalError( "Couldn't create Font Arial_16.xpr\n" );
    }

    // Expanding Font area to get additional screen real estate...
    m_Font.SetWindow( 64, 8, 1280 - 64, 720 - 8 );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG::FatalError( "Couldn't create Help.xpr\n" );
    }

    // Initialize state variables
    m_bDrawHelp = FALSE;
    m_iUntileMethod = 0;
    m_iTestTexture = 0;
    m_bTraceUntile = FALSE;
    m_bBitwiseCompare = FALSE;
    m_bCacheableDest = FALSE;
    m_fSmoothedUntileTimeInMS[0] = m_fSmoothedUntileTimeInMS[1] = m_fUntileTimeInMS = 0.0f;

    m_iDestSize = 0;

    // Create common vertex declaration used by all the geometry
    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END( )
    };

    m_pd3dDevice->CreateVertexDeclaration( decl, &m_pVertexDecl );

    // Create shaders
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\CopyTexture.xvu",
                                       &m_pCopyTextureVS ) ) )
    {
        ATG::FatalError( "Couldn't create VertexShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\CopyTexture.xpu",
                                       &m_pCopyTexturePS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\BitwiseCompareTexture.xpu",
                                       &m_pBitwiseCompareTexturePS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }

    m_GPUUntiler.Initialize( m_pd3dDevice );

    // Create the textures resource
    if( FAILED( m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG::FatalError( "Couldn't create Resource.xpr\n" );
    }

    // Create render targets
    D3DSURFACE_PARAMETERS SurfaceParameters = { 0 };
    m_pd3dDevice->CreateRenderTarget( m_d3dpp.BackBufferWidth, 
        m_d3dpp.BackBufferHeight, 
        m_d3dpp.BackBufferFormat,
        D3DMULTISAMPLE_NONE, 
        0,
        FALSE,
        &m_pBackBuffer, 
        &SurfaceParameters );
    m_pd3dDevice->CreateRenderTarget( m_d3dpp.BackBufferWidth, 
        m_d3dpp.BackBufferHeight, 
        GetNonAs16NonsRGBFormat( m_d3dpp.BackBufferFormat ),
        D3DMULTISAMPLE_NONE, 
        0,
        FALSE,
        &m_pDummyNonsRGBBackBuffer, 
        &SurfaceParameters );

    // Initialize the default texture
    m_pTiledTexture = NULL;
    m_pLinearTexture[0] = m_pLinearTexture[1] = NULL;
    LoadTestTexture( );

    // Sample doesn't use z-test
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    // Init perf counters
    PerfCounterInit( );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Sample::Update( )
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Update( )
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput( );

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( !m_bDrawHelp )
    {
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            m_iTestTexture = ++m_iTestTexture % TEST_TEXTURE_COUNT;
            LoadTestTexture( );
        }
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        {
            m_bCacheableDest = !m_bCacheableDest;
            LoadTestTexture( );
        }
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        {
            m_bTraceUntile = TRUE;
        }
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        {
            m_bBitwiseCompare = !m_bBitwiseCompare;
        }

        // Switch method
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
            m_iUntileMethod = ( m_iUntileMethod == 0 ) ? ( UNTILE_METHOD_COUNT - 1 ) : ( m_iUntileMethod - 1 );
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
            m_iUntileMethod = ( m_iUntileMethod == ( UNTILE_METHOD_COUNT - 1 ) ) ? 0 : ( m_iUntileMethod + 1 );
    }

    return S_OK;
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::BeginGpuTimer( )
// Desc: Start a timing bracket on the GPU
//---------------------------------------------------------------------------------------------------------
VOID Sample::BeginGpuTimer( )
{
#ifndef _RELEASED3D
    m_pd3dDevice->QueryPerfCounters( m_pPerfCounterStart[ m_iPerfCounterBufferIndex % 3 ], 
        D3DPERFQUERY_WAITGPUIDLE );
#endif
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::EndGpuTimer( )
// Desc: Conclude a timing bracket on the GPU
//---------------------------------------------------------------------------------------------------------
VOID Sample::EndGpuTimer( )
{
#ifndef _RELEASED3D
    m_pd3dDevice->QueryPerfCounters( m_pPerfCounterEnd[ m_iPerfCounterBufferIndex % 3 ], 
        D3DPERFQUERY_WAITGPUIDLE );

    D3DPERFCOUNTER_VALUES StartValues;
    m_pPerfCounterStart[ m_iPerfCounterBufferIndex % 3 ]->GetValues( &StartValues, 0, NULL );
    D3DPERFCOUNTER_VALUES EndValues;
    m_pPerfCounterEnd[ m_iPerfCounterBufferIndex % 3 ]->GetValues( &EndValues, 0, NULL );

    // Subtract start values from end values.
    UINT64* pStartValues = ( UINT64* )&StartValues;
    UINT64* pEndValues = ( UINT64* )&EndValues;
    const DWORD dwCount = sizeof( D3DPERFCOUNTER_VALUES ) / sizeof( UINT64 );
    for( DWORD i = 0; i < dwCount; ++i )
    {
        pEndValues[i] -= pStartValues[i];
    }

    m_fUntileTimeInMS = ( FLOAT )EndValues.RBBM[0].QuadPart 
        / ( FLOAT )m_iGpuCyclesPerMs;

    ++m_iPerfCounterBufferIndex;
#endif
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::SetDummyNuiTexture( )
// Desc: Special case hack to fake a Nui Depth texture.  The toolchain doesn't natively handle D16, so 
// we use R5G6B5 instead, and switch formats at run-time.  We keep the R5G6B5 format for visualization, 
// but until the D16 format to demonstrate actual usage.
// 
// The same applies for the Nui Luminance texture, which is L8 in the pipeline, but should really be R8 
// here.  And for the A32B32G32R32F sample texture, which needs to be sRGB in the pipeline but not at
// runtime.
//---------------------------------------------------------------------------------------------------------
inline VOID Sample::SetDummyNuiTexture( IDirect3DTexture9* pSourceTexture )
{
    D3DFORMAT D3DFMT_R8 = (D3DFORMAT) MAKED3DFMT(
        GPUTEXTUREFORMAT_8, 
        GPUENDIAN_NONE, 
        TRUE, 
        GPUSIGN_ALL_UNSIGNED, 
        GPUNUMFORMAT_FRACTION, 
        GPUSWIZZLE_OOOR);
    D3DFORMAT D3DFMT_LIN_R8 = (D3DFORMAT) MAKELINFMT( D3DFMT_R8 );

    XGTEXTURE_DESC SourceDesc;
    XGGetTextureDesc( pSourceTexture, 0, &SourceDesc );

    D3DFORMAT NewFormat = SourceDesc.Format;
    if( NewFormat == D3DFMT_R5G6B5 || NewFormat == D3DFMT_LIN_R5G6B5 )
    {
        NewFormat = XGIsTiledFormat( SourceDesc.Format ) ? D3DFMT_D16 : D3DFMT_LIN_D16;
    }
    if( NewFormat == MAKESRGBFMT( D3DFMT_L8 ) || NewFormat == MAKESRGBFMT( D3DFMT_LIN_L8 ) )
    {
        NewFormat = XGIsTiledFormat( SourceDesc.Format ) ? D3DFMT_R8 : D3DFMT_LIN_R8;
    }

    IDirect3DTexture9 DummyTextureHeader;

    XGSetTextureHeader( SourceDesc.Width, 
        SourceDesc.Height, 
        1, 
        ( pSourceTexture->Common & D3DCOMMON_CPU_CACHED_MEMORY ) ? D3DUSAGE_CPU_CACHED_MEMORY : 0, 
        NewFormat, 
        0, 
        pSourceTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT, 
        0, 
        0, 
        &DummyTextureHeader, 
        NULL, 
        NULL );

    pSourceTexture->Format = DummyTextureHeader.Format;
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::RenderPerf( )
// Desc: Print perf info to screen
//---------------------------------------------------------------------------------------------------------
VOID Sample::RenderPerf( )
{
    WCHAR strText[256];
    FLOAT fXColumnPos[2] = { 0.0f, 250.0f };
    FLOAT fYPos = 70.0f;
    m_Font.SetScaleFactors( 0.9f, 0.9f );

    switch( m_iUntileMethod )
    {
    case UNTILE_METHOD_XGUNTILESURFACE:
    case UNTILE_METHOD_CPU_OPTIMIZED:
        {
            m_Font.DrawText( fXColumnPos[0], fYPos, 0xFF00FFFF, L"CPU MS Tiled-to-Linear:" );
            swprintf_s( strText, L"%3.3f", m_fSmoothedUntileTimeInMS[0] );
            m_Font.DrawText( fXColumnPos[1], fYPos, 0xFF00FFFF, strText );
            fYPos += 20;
            m_Font.DrawText( fXColumnPos[0], fYPos, 0xFF00FFFF, L"CPU MS Linear-to-Linear:" );
            swprintf_s( strText, L"%3.3f", m_fSmoothedUntileTimeInMS[1] );
            m_Font.DrawText( fXColumnPos[1], fYPos, 0xFF00FFFF, strText );
            fYPos += 20;
        }
        break;

    case UNTILE_METHOD_MEMEXPORT_TEXEL: 
    case UNTILE_METHOD_MEMEXPORT_TRANSACTION: 
    case UNTILE_METHOD_MEMEXPORT_PACKED: 
    case UNTILE_METHOD_RESOLVE: 
        {
#ifndef _RELEASED3D
            m_Font.DrawText( fXColumnPos[0], fYPos, 0xFF00FFFF, L"GPU MS Tiled-to-Linear:" );
            swprintf_s( strText, L"%3.3f", m_fSmoothedUntileTimeInMS[0] );
            m_Font.DrawText( fXColumnPos[1], fYPos, 0xFF00FFFF, strText );
            fYPos += 20;
            m_Font.DrawText( fXColumnPos[0], fYPos, 0xFF00FFFF, L"GPU MS Linear-to-Linear:" );
            swprintf_s( strText, L"%3.3f", m_fSmoothedUntileTimeInMS[1] );
            m_Font.DrawText( fXColumnPos[1], fYPos, 0xFF00FFFF, strText );
            fYPos += 20;
#else
            m_Font.DrawText( fXColumnPos[0], fYPos, 0xFF8080FF, L"GPU MS:" );
            m_Font.DrawText( fXColumnPos[1], fYPos, 0xFF8080FF, L"(unavailable in release)" );
            fYPos += 20;
#endif
        }
        break;

    default:
        assert( FALSE );
        break;
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::RenderUI( )
// Desc: Render the screen display for the UI elements.
//---------------------------------------------------------------------------------------------------------
VOID Sample::RenderUI( )
{
    PIXBeginNamedEvent( 0, "Render UI" );

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        XGTEXTURE_DESC SourceDesc;
        XGGetTextureDesc( m_pTiledTexture, 0, &SourceDesc );

        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"FastUntile" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 40, 0xffffffff, L"Method:" );
        m_Font.DrawText( 100, 40, 0xffff00ff, g_strUntileMethodNames[ m_iUntileMethod ] );

        XGTEXTURE_DESC UIDesc;
        XGGetTextureDesc( m_pTiledTexture, 0, &UIDesc );
        WCHAR UIText[256];
        swprintf_s( UIText, L"%s\n%d x %d\n%d bytes/texel", 
            g_strTestTextureNames[ m_iTestTexture ], 
            SourceDesc.Width, 
            SourceDesc.Height, 
            UIDesc.BytesPerBlock );

        m_Font.DrawText( 640, 40, 0xffffffff, L"Texture:" );
        m_Font.DrawText( 740, 40, 0xffff00ff, UIText );

        m_Font.DrawText( 256, 160, 0xff00ff00, L"Tiled texture, cacheable", ATGFONT_CENTER_X );
        swprintf_s( UIText, L"Linear texture, %s", m_bCacheableDest ? L"cacheable" : L"write-combined" );
        m_Font.DrawText( 896, 160, 0xff00ff00, 
            m_bBitwiseCompare ? L"Bitwise compare (Green=equal, Red=unequal)" : UIText, ATGFONT_CENTER_X );

        m_Font.End();
    }

    PIXEndNamedEvent( );
}


//-----------------------------------------------------------------------------
// Name: Render( )
// Desc: Called once per frame, to render the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Render( )
{
    // Do GPU methods twice: tiled-to-linear and linear-to-linear.
    // The point is to help in evaluating which is better in conjunction with other GPU processing:
    //      a) Keep textures linear in memory after each pass
    //      b) Keep intermediate results tiled, and convert back to linear at the end
    for( UINT i = 0; i < 2; ++i )
    {
        IDirect3DTexture9* pSourceTexture = ( i == 0 ? m_pTiledTexture : m_pLinearTexture[0] );
        IDirect3DTexture9* pDestTexture = m_pLinearTexture[i];

        UINT iSourceSize = ( i == 0 ? m_iSourceSize : m_iDestSize );
        UINT iDestSize = m_iDestSize;

        // Find data pointers and texture desc's
        VOID* pSourceData = (VOID*) ( pSourceTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT );
        VOID* pSourceDataCachedReadOnly = (VOID*) ( GPU_CONVERT_CPU_TO_CPU_CACHED_READONLY_ADDRESS( pSourceData ) );
        VOID* pDestData = (VOID*) ( pDestTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT );

        XGTEXTURE_DESC SourceDesc;
        XGGetTextureDesc( pSourceTexture, 0, &SourceDesc );
        XGTEXTURE_DESC DestDesc;
        XGGetTextureDesc( pDestTexture, 0, &DestDesc );

#ifdef _DEBUG
        {
            // For debugging purposes, clear the destination texture to gray.  Then, if we fail to write
            // some texels, it will be immediately evident.

            D3DLOCKED_RECT DestLockedRect;
            pDestTexture->LockRect( 0, &DestLockedRect, NULL, 0 );
            memset( pDestData, 0x80, m_iDestSize );
            pDestTexture->UnlockRect( 0 );
        }
#endif

        // Lock textures, if necessary, and don't count the lock stall towards the perf cost
        //-------------------------------------------------------------------------------------------------
        // WARNING:  This may undercount perf cost, because Lock will sometimes flush the cache.
        //-------------------------------------------------------------------------------------------------
        switch( m_iUntileMethod )
        {
        case UNTILE_METHOD_CPU_OPTIMIZED:
        case UNTILE_METHOD_XGUNTILESURFACE:
            {
                D3DLOCKED_RECT SourceLockedRect;
                pSourceTexture->LockRect( 0, &SourceLockedRect, NULL, D3DLOCK_READONLY );
                D3DLOCKED_RECT DestLockedRect;
                pDestTexture->LockRect( 0, &DestLockedRect, NULL, 0 );
            }
            break;

        case UNTILE_METHOD_MEMEXPORT_TEXEL: 
        case UNTILE_METHOD_MEMEXPORT_TRANSACTION: 
        case UNTILE_METHOD_MEMEXPORT_PACKED: 
        case UNTILE_METHOD_RESOLVE: 
            break;

        default:
            assert( FALSE );
            break;
        }

        // When aliasing textures as different types, sizes, etc., it's necessary to keep the 
        // same resource pointer, and overwrite the Format field.  Otherwise, D3D's coherency 
        // guarantees can fail, in situations such as: 
        //
        //    �	Unlock followed by SetTexture
        //    �	Resolve followed by SetTexture
        //    �	Resolve followed by Lock
        //    �	SetTexture followed by BeginExport
        //    �	EndExport followed by SetTexture
        //    �	EndExport followed by Lock
        //
        GPUTEXTURE_FETCH_CONSTANT OldSourceTextureFetchConstant = pSourceTexture->Format;
        SetDummyNuiTexture( pSourceTexture );

        GPUTEXTURE_FETCH_CONSTANT OldDestTextureFetchConstant = pDestTexture->Format;
        SetDummyNuiTexture( pDestTexture );

        PIXBeginNamedEvent( 0, i == 0 ? "Tiled-to-linear" : "Linear-to-linear" );

        // Optionally get CPU trace capture
        if ( m_bTraceUntile )
        {
            XTraceStartRecording( i == 0 
                ? "devkit:\\Trace_FastUntile_Tiled_to_Linear.pix2" 
                : "devkit:\\Trace_FastUntile_Linear_to_Linear.pix2" );
        }

        // Perform tiling
        switch( m_iUntileMethod )
        {
        case UNTILE_METHOD_XGUNTILESURFACE:
            {
                m_UntileTimer.GetElapsedTime( );

                if( i == 0 )
                {
                    XGUntileSurface( pDestData, 
                        DestDesc.RowPitch, 
                        NULL, 
                        pSourceDataCachedReadOnly, 
                        SourceDesc.Width, 
                        SourceDesc.Height, 
                        NULL, 
                        SourceDesc.BytesPerBlock );
                }
                else    // just copying
                {
                    XMemCpyStreaming( pDestData, 
                        pSourceDataCachedReadOnly, 
                        SourceDesc.RowPitch * SourceDesc.Height );
                }

                m_fUntileTimeInMS = ( FLOAT ) m_UntileTimer.GetElapsedTime( ) * 1000.0f;
            }
            break;

        case UNTILE_METHOD_CPU_OPTIMIZED:
            {
                m_UntileTimer.GetElapsedTime( );

                if( i == 0 )
                {
                    m_CPUUntiler.UntileTexture( pSourceTexture, pDestTexture );
                }
                else    // just copying
                {
                    XMemCpyStreaming( pDestData, 
                        pSourceDataCachedReadOnly, 
                        SourceDesc.RowPitch * SourceDesc.Height );
                }

                m_fUntileTimeInMS = ( FLOAT ) m_UntileTimer.GetElapsedTime( ) * 1000.0f;
            }
            break;

        case UNTILE_METHOD_MEMEXPORT_TEXEL: 
        case UNTILE_METHOD_MEMEXPORT_TRANSACTION: 
        case UNTILE_METHOD_MEMEXPORT_PACKED: 
            {
                BeginGpuTimer();

                m_GPUUntiler.UntileMemexport( m_pd3dDevice, pSourceTexture, pDestTexture, m_iUntileMethod );

                EndGpuTimer();
            }
            break;

        case UNTILE_METHOD_RESOLVE:
            {
                BeginGpuTimer();

                m_GPUUntiler.UntileResolve( m_pd3dDevice, pSourceTexture, pDestTexture, iSourceSize, iDestSize );

                EndGpuTimer();
            }
            break;

        default:
            assert( FALSE );
            break;
        }

        // We want smoothed times to converge quickly, and become stable soon afterwards
        // Higher factor means faster convergence but more oscillation
        FLOAT fLerpFactor = fabsf( m_fUntileTimeInMS - m_fSmoothedUntileTimeInMS[i] ) / 10.0f;
        fLerpFactor = Max( fLerpFactor, 0.01f );
        fLerpFactor = Min( fLerpFactor, 0.9f );

        m_fSmoothedUntileTimeInMS[i] = ( 1.0f - fLerpFactor ) * m_fSmoothedUntileTimeInMS[i] 
        + fLerpFactor * m_fUntileTimeInMS;

        if ( m_bTraceUntile )
        {
            XTraceStopRecording( );
        }

        PIXEndNamedEvent( );

        // Restore original texture fetch constant, from prior to aliasing.  
        pSourceTexture->Format = OldSourceTextureFetchConstant;
        pDestTexture->Format = OldDestTextureFetchConstant;

        // Unlock textures, if necessary, and don't count the lock time towards the perf cost
        //-------------------------------------------------------------------------------------------------
        // WARNING:  This may undercount perf cost, because Unlock will sometimes flush the cache.
        //-------------------------------------------------------------------------------------------------
        switch( m_iUntileMethod )
        {
        case UNTILE_METHOD_CPU_OPTIMIZED:
        case UNTILE_METHOD_XGUNTILESURFACE:
            {
                pSourceTexture->UnlockRect( 0 );
                pDestTexture->UnlockRect( 0 );
            }
            break;

        case UNTILE_METHOD_MEMEXPORT_TEXEL: 
        case UNTILE_METHOD_MEMEXPORT_TRANSACTION: 
        case UNTILE_METHOD_MEMEXPORT_PACKED: 
        case UNTILE_METHOD_RESOLVE: 
            break;

        default:
            assert( FALSE );
            break;
        }

        m_pd3dDevice->SetRenderTarget( 0, NULL );
    }

    m_pd3dDevice->SetRenderTarget( 0, m_pBackBuffer );

    // Display the tiled/linear textures onscreen to verify accuracy visually.
    // Not really necessary, but otherwise sample looks awfully boring
    D3DVIEWPORT9 OldViewport;
    m_pd3dDevice->GetViewport( &OldViewport );

    PIXBeginNamedEvent( 0, "Clear to pattern" );

    // Clear the viewport to a pattern ( this is visible behind the non-full-screen test geometry )
    D3DCOLOR D3D_BLACK      = D3DCOLOR_ARGB( 0xff, 0x00, 0x00, 0x00 );
    D3DCOLOR D3D_YELLOW = D3DCOLOR_ARGB( 0xff, 0xff, 0xff, 0x00 );
    UINT iStripCount = 32;
    UINT iStripWidth = m_d3dpp.BackBufferWidth / iStripCount;
    UINT iStripHeight = m_d3dpp.BackBufferHeight;
    for( UINT iStrip = 0; iStrip < iStripCount; ++iStrip )
    {
        D3DRECT d3dRectBlack = { iStripWidth * iStrip, 0, iStripWidth * ( iStrip + 1 ), iStripHeight };
        m_pd3dDevice->Clear( 1, &d3dRectBlack, D3DCLEAR_TARGET, D3D_BLACK, 1.0f, 0L );
        ++iStrip;
        D3DRECT d3dRectYellow = { iStripWidth * iStrip, 0, iStripWidth * ( iStrip + 1 ), iStripHeight };
        m_pd3dDevice->Clear( 1, &d3dRectYellow, D3DCLEAR_TARGET, D3D_YELLOW, 1.0f, 0L );
    }

    PIXEndNamedEvent( );

    if( m_bFakesRGB )
    {
        m_pd3dDevice->SetRenderTarget( 0, m_pDummyNonsRGBBackBuffer );
    }

    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    m_pd3dDevice->SetVertexShader( m_pCopyTextureVS );
    m_pd3dDevice->SetPixelShader( m_pCopyTexturePS );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    {
        // Render tiled into left-hand viewport
        UINT ViewportCenterX = 64  + 512 / 2;
        UINT ViewportCenterY = 192 + 512 / 2;

        XGTEXTURE_DESC SourceDesc;
        XGGetTextureDesc( m_pTiledTexture, 0, &SourceDesc );

        D3DVIEWPORT9 Viewport;
        Viewport.X = ViewportCenterX - SourceDesc.Width / 2;
        Viewport.Y = ViewportCenterY - SourceDesc.Height / 2;;
        Viewport.Width = SourceDesc.Width;
        Viewport.Height = SourceDesc.Height;
        Viewport.MinZ = 0.0f;
        Viewport.MaxZ = 1.0f;
        m_pd3dDevice->SetViewport( &Viewport );

        m_pd3dDevice->SetTexture( 0, m_pTiledTexture );

        PIXBeginNamedEvent( 0, "Left Viewport (Tiled)" );

        m_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, ScreenspaceRectangleVerts, sizeof( ScreenspaceVertex ) );

        PIXEndNamedEvent( );
    }

    {
        // Render tiled into right-hand viewport
        UINT ViewportCenterX = 1280 - 64 - 512 / 2;
        UINT ViewportCenterY = 192 + 512 / 2;

        XGTEXTURE_DESC DestDesc;
        XGGetTextureDesc( m_pLinearTexture[1], 0, &DestDesc );

        D3DVIEWPORT9 Viewport;
        Viewport.X = ViewportCenterX - DestDesc.Width / 2;
        Viewport.Y = ViewportCenterY - DestDesc.Height / 2;;
        Viewport.Width = DestDesc.Width;
        Viewport.Height = DestDesc.Height;
        Viewport.MinZ = 0.0f;
        Viewport.MaxZ = 1.0f;
        m_pd3dDevice->SetViewport( &Viewport );

        m_pd3dDevice->SetTexture( 0, m_pLinearTexture[1] );

        if( m_bBitwiseCompare )
        {
            m_pd3dDevice->SetPixelShader( m_pBitwiseCompareTexturePS );
            m_pd3dDevice->SetTexture( 1, m_pTiledTexture );
        }

        PIXBeginNamedEvent( 0, "Right Viewport (%s)", m_bBitwiseCompare ? "Compare" : "Linear" );

        m_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, ScreenspaceRectangleVerts, sizeof( ScreenspaceVertex ) );

        PIXEndNamedEvent( );
    }

    m_pd3dDevice->SetRenderTarget( 0, m_pBackBuffer );

    m_pd3dDevice->SetViewport( &OldViewport );

    m_bTraceUntile = FALSE;

    RenderPerf( );

    RenderUI( );

    // Present the backbuffer contents to the display
    IDirect3DTexture9* pFrontBuffer;
    m_pd3dDevice->GetFrontBuffer( &pFrontBuffer );
    m_pd3dDevice->SynchronizeToPresentationInterval();
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pFrontBuffer, NULL, 0, 0, NULL, 1.0f, 0L, NULL );
    m_pd3dDevice->Swap( pFrontBuffer, NULL );

    m_pd3dDevice->UnsetAll( );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: main( )
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------

INT __cdecl main( )
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Use fixed back buffer resolution regardless of output dimensions
    atgApp.m_d3dpp.BackBufferWidth = 1280;
    atgApp.m_d3dpp.BackBufferHeight = 720;

    // The sample doesn't use depth
    atgApp.m_d3dpp.EnableAutoDepthStencil = FALSE;
    atgApp.m_d3dpp.DisableAutoBackBuffer = TRUE;    // We use our own backbuffer

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_X8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run( );
}
