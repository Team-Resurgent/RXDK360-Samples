//---------------------------------------------------------------------------------------------------------
// FastDepthRestore.cpp
//
// XNA Developer Connection
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------
#include <xtl.h>    // must come before the others
#include <d3d9.h>
#include <xgraphics.h>

#include <AtgApp.h>
#include <AtgDebugDraw.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSceneAll.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>

// Define a symbol that is used to compile out the use of the GPU performance counter APIs 
// when using a release build of Direct3D.  The GPU performance counter APIs only work with 
// d3d9i.lib and d3d9d.lib.
#if defined( NDEBUG ) && ( !defined( PROFILE ) || defined( FASTCAP ) )
#define RELEASED3D
#endif

// Define this to debug any issues with inherited/persisted state in the precompiled command buffers.  
// Although the sample runs cleanly, there is a possibility of error if the sample is ported to an 
// location where the D3D pre-existing state is different.
//#define DEBUG_PCBS

//---------------------------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//---------------------------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_2, L"Use command\nbuffer"  },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_2, L"Move\ncamera" },
    { ATG::HELP_RIGHT_STICK,    ATG::HELP_PLACEMENT_2, L"Rotate\ncamera" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_2, L"Cycle method\nleft/right" },
};


//---------------------------------------------------------------------------------------------------------
// enum DEPTH_RESTORE_METHODS:
// 
// Methods for restoring depth buffer data to EDRAM
//---------------------------------------------------------------------------------------------------------
enum DEPTH_RESTORE_METHODS
{
    DEPTH_RESTORE_METHOD_ODEPTH, 
    DEPTH_RESTORE_METHOD_AS_COLOR,
    DEPTH_RESTORE_METHOD_AS_COLOR_WITH_PIPELINED_HIZ,

    DEPTH_RESTORE_METHOD_COUNT
};

const static WCHAR* g_strDepthRestoreMethodNames[] = 
{
    L"Depth as oDepth", 
    L"Depth as color (followed by Hi-Z restore)", 
    L"Depth as color (Hi-Z restored in parallel)", 
};
static_assert( _countof( g_strDepthRestoreMethodNames ) == DEPTH_RESTORE_METHOD_COUNT, 
    "Mismatched counts" );


//---------------------------------------------------------------------------------------------------------
// Utility functions
//---------------------------------------------------------------------------------------------------------
template<typename t_type>
t_type Min( t_type a, t_type b ) { return a < b ? a : b; }
template<typename t_type>
t_type Max( t_type a, t_type b ) { return a > b ? a : b; }


//---------------------------------------------------------------------------------------------------------
// Struct for vertex of test geometry
//---------------------------------------------------------------------------------------------------------
struct ScreenspaceVertex
{
    XMFLOAT2 Position;
};

static const ScreenspaceVertex ScreenspaceRectangleVerts[] = 
{
    { XMFLOAT2( -1.0f, -1.0f ), }, 
    { XMFLOAT2(  1.0f, -1.0f ), }, 
    { XMFLOAT2( -1.0f,  1.0f ), }, 
    //{ XMFLOAT2(  1.0f,  1.0f ), },    // Needed for QUADLIST, but not RECTLIST
};


//---------------------------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited from the ATG::Application 
// base class.
//---------------------------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    // ATG helper items
    ATG::Font                       m_Font;                 // Font for drawing text
    ATG::Help                       m_Help;                 // Display help
    ATG::Scene*                     m_pScene;               // Sample scene
    ATG::Timer                      m_FrameTimer;           // Controls camera speed

    // Sample options adjustable from the controller
    BOOL                            m_bDrawHelp;
    BOOL                            m_bUseCommandBuffer;
    UINT                            m_iDepthRestoreMethod;

    // Camera parameters
    XMVECTOR                        m_vCameraPosition;
    XMVECTOR                        m_vCameraDirection;
    XMVECTOR                        m_vCameraUp;
    XMVECTOR                        m_vCameraRight;
    ATG::Projection                 m_proj;

    // The location of the depth buffer in EDRAM and Hi-Z
    // !!! Titles should modify this to match their own EDRAM layout !!!
    D3DSURFACE_PARAMETERS           m_DepthSurfaceParams;

    // Three aliases of the depth buffer in EDRAM
    IDirect3DSurface9*              m_pDepthStencilSurface;
    IDirect3DSurface9*              m_pDepthStencilAsColorSurface;
    IDirect3DSurface9*              m_pDepthStencilAs4xMSAASurface;

    // Two aliases of the depth buffer texture in main memory
    IDirect3DTexture9*              m_pDepthTexture;
    IDirect3DTexture9*              m_pDepthStencilAsColorTexture;

    // Command buffer for pre-recording the restore operation    
    IDirect3DDevice9*               m_pd3dCommandBufferDevice;
    IDirect3DCommandBuffer9*        m_pRestoreDepthCommandBuffer[ DEPTH_RESTORE_METHOD_COUNT ];

    // Resources for test geometry
    IDirect3DVertexDeclaration9*    m_pVertexDecl;

    // Shaders for rendering the sample
    IDirect3DVertexShader9*         m_pDepthRestoreODepthVS;
    IDirect3DPixelShader9*          m_pDepthRestoreODepthPS;
    IDirect3DVertexShader9*         m_pDepthRestoreAsColorVS;
    IDirect3DPixelShader9*          m_pDepthRestoreAsColorPS;

    // CPU performance data
    ATG::Timer                      m_CpuTimer;
    FLOAT                           m_fDepthRestoreCpuTimeInMS;
    FLOAT                           m_fSmoothedDepthRestoreCpuTimeInMS;

    // GPU performance data
    IDirect3DPerfCounterBatch9*     m_pPerfCounterBatch;
    const static UINT               m_iGpuCyclesPerMs = GPU_CLOCK_SPEED / 1000;   // 500 MHz
    FLOAT                           m_fDepthRestoreGpuTimeInMS;
    FLOAT                           m_fSmoothedDepthRestoreGpuTimeInMS;

    // Performance analysis
    VOID PerfCounterInit();
    VOID BeginCpuTimer();
    VOID EndCpuTimer();
    VOID BeginGpuTimer();
    VOID EndGpuTimer();
    VOID RenderPerf();

    // Main render
    VOID RenderScene();

    // UI
    VOID RenderUI();

    // Depth restore methods
    VOID DepthRestore( IDirect3DDevice9* pd3dDevice, UINT iMethod );
    VOID DepthRestoreODepth( IDirect3DDevice9* pd3dDevice );
    VOID DepthRestoreAsColor( IDirect3DDevice9* pd3dDevice );
    VOID DepthRestoreAsColorWithPipelinedHiZ( IDirect3DDevice9* pd3dDevice );

public:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
};


//---------------------------------------------------------------------------------------------------------
// Name: Sample:PerfCounterInit()
// Desc: Set up the perf counters we plan to capture
//---------------------------------------------------------------------------------------------------------
VOID Sample::PerfCounterInit()
{
#ifndef RELEASED3D
    // Set up GPU performance counter structures.  We need a batch of size 2 in order to time one event.
    m_pd3dDevice->CreatePerfCounterBatch( 2, 0, 0, &m_pPerfCounterBatch );
#endif
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::Initialize()
// Desc: Initialize app-dependent objects.
//---------------------------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initial camera parameters, hand-chosen
    m_vCameraPosition = XMVectorSet( 15.38f, 7.31f, -1.51f, 0.0f );
    m_vCameraDirection = XMVector3Normalize( XMVectorSet( -0.96f, -0.05f, 0.27f, 0.0f ) );
    m_vCameraUp = XMVector3Normalize( XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f ) );
    m_vCameraRight = XMVector3Normalize( XMVector3Cross( m_vCameraUp, m_vCameraDirection ) );
    m_proj.SetFovXAspect( XM_PIDIV2, 16.0f / 9.0f, 1.0f, 100.0f );

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG::FatalError( "Couldn't create Font Arial_16.xpr\n" );
    }

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG::FatalError( "Couldn't create Help.xpr\n" );
    }

    // Initialize simple shaders.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create and load SponzaAtrium scene
    m_pScene = new ATG::Scene();
    if( FAILED( ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\SponzaAtrium.xatg", m_pScene, NULL,
        ATG::XATGLOADER_DONOTINITIALIZEMATERIALS | ATG::XATGLOADER_DONOTBINDTEXTURES, NULL ) ) )
    {
        ATG::FatalError( "Couldn't load scene\n" );
    };

    // Initialize state variables
    m_bDrawHelp = FALSE;
    m_iDepthRestoreMethod = 0;
    m_bUseCommandBuffer = FALSE;
    m_fSmoothedDepthRestoreCpuTimeInMS = m_fDepthRestoreCpuTimeInMS = 0.0f;
    m_fSmoothedDepthRestoreGpuTimeInMS = m_fDepthRestoreGpuTimeInMS = 0.0f;

    // Create common vertex declaration used by all the geometry
    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    m_pd3dDevice->CreateVertexDeclaration( decl, &m_pVertexDecl );

    // Create shaders
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\DepthRestoreODepth.xvu",
                                       &m_pDepthRestoreODepthVS ) ) )
    {
        ATG::FatalError( "Couldn't create VertexShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DepthRestoreODepth.xpu",
                                       &m_pDepthRestoreODepthPS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\DepthRestoreAsColor.xvu",
                                       &m_pDepthRestoreAsColorVS ) ) )
    {
        ATG::FatalError( "Couldn't create VertexShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DepthRestoreAsColor.xpu",
                                       &m_pDepthRestoreAsColorPS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }

    // The following textures alias the same memory
    if( FAILED( m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, 
        m_d3dpp.BackBufferHeight, 
        1, 
        0, 
        D3DFMT_D24S8, 
        0, 
        &m_pDepthTexture, 
        NULL ) ) )
    {
        ATG::FatalError( "Couldn't create Texture\n" );
    }

    // These lines generate a new texture, pointing to the same data as m_pDepthTexture, and
    // having all the same properties except for data format.
    XGTEXTURE_DESC DepthDesc;
    XGGetTextureDesc( m_pDepthTexture, 0, &DepthDesc );

    UINT BaseOffset;
    XGGetTextureLayout( m_pDepthTexture, &BaseOffset, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1 );

    m_pDepthStencilAsColorTexture = new IDirect3DTexture9;
    XGSetTextureHeader( DepthDesc.Width, 
        DepthDesc.Height, 
        1, 
        0, 
        D3DFMT_A8B8G8R8, 
        0, 
        BaseOffset, 
        0, 
        DepthDesc.RowPitch, 
        m_pDepthStencilAsColorTexture, 
        NULL, 
        NULL );

    // Our depth buffer is at address 0 in EDRAM and address 0 in Hi-Z
    // !!!
    // !!! WARNING: Titles need to modify this struct to reflect their own EDRAM and Hi-Z layout !!!
    // !!!
    m_DepthSurfaceParams.Base = 0;
    m_DepthSurfaceParams.ColorExpBias = 0;
    m_DepthSurfaceParams.HierarchicalZBase = 0;
    m_DepthSurfaceParams.HiZFunc = D3DHIZFUNC_DEFAULT;

    // The following surfaces all alias the same EDRAM
    if( FAILED( m_pd3dDevice->CreateRenderTarget( m_d3dpp.BackBufferWidth, 
        m_d3dpp.BackBufferHeight, 
        D3DFMT_D24S8,       
        D3DMULTISAMPLE_NONE, 
        0, 
        FALSE, 
        &m_pDepthStencilSurface, 
        &m_DepthSurfaceParams ) ) )
    {
        ATG::FatalError( "Couldn't create Render Target\n" );
    }

    if( FAILED( m_pd3dDevice->CreateRenderTarget( m_d3dpp.BackBufferWidth / 2, 
        m_d3dpp.BackBufferHeight / 2, 
        D3DFMT_D24S8,      
        D3DMULTISAMPLE_4_SAMPLES, 
        0, 
        FALSE, 
        &m_pDepthStencilAs4xMSAASurface, 
        &m_DepthSurfaceParams ) ) )
    {
        ATG::FatalError( "Couldn't create Render Target\n" );
    }

    if( FAILED( m_pd3dDevice->CreateRenderTarget( m_d3dpp.BackBufferWidth, 
        m_d3dpp.BackBufferHeight, 
        D3DFMT_A8B8G8R8,
        D3DMULTISAMPLE_NONE, 
        0, 
        FALSE, 
        &m_pDepthStencilAsColorSurface, 
        &m_DepthSurfaceParams ) ) )
    {
        ATG::FatalError( "Couldn't create Render Target\n" );
    }

    // Init perf counters
    PerfCounterInit();

    // Auxiliary D3D device for generating command buffers
    if( FAILED( Direct3D_CreateDevice( 0, 
        D3DDEVTYPE_COMMAND_BUFFER, 
        NULL, 
        0, 
        NULL, 
        &m_pd3dCommandBufferDevice ) ) )
    {
        ATG::FatalError( "Couldn't create Command Buffer Device\n" );
    }

    // Build precompiled command buffers for all depth restore methods, to eliminate per-frame CPU overhead.
    D3DTAGCOLLECTION InheritTags = { 0 };
    D3DTAGCOLLECTION PersistTags = { 0 };

    // Skip this block of code to debug inheritance/persistence problems.  
    // With nothing inherited, depth restore will run starting from a clean D3D device.  
    // With nothing persisted, code following the depth restore will run starting from a clean D3D device.
    // This mode will cause some command buffer bloat, and minor GPU slowdown.
#ifndef DEBUG_PCBS
    D3DTagCollection_SetAll( &InheritTags );
    D3DTagCollection_Clear( &InheritTags, D3DTag_Index( D3DTAG_EDRAMMODECONTROL ), D3DTag_Mask( D3DTAG_EDRAMMODECONTROL ) );
    D3DTagCollection_SetAll( &PersistTags );
#endif
    for( UINT i = 0; i < DEPTH_RESTORE_METHOD_COUNT; ++i )
    {
        m_pd3dCommandBufferDevice->CreateCommandBuffer( 64 * 1024, 0, &m_pRestoreDepthCommandBuffer[i] );

        if( FAILED( m_pd3dCommandBufferDevice->BeginCommandBuffer( m_pRestoreDepthCommandBuffer[i], 
            D3DBEGINCB_OVERWRITE_INHERITED_STATE, 
            &InheritTags, 
            &PersistTags, 
            NULL, 
            0 ) ) )
        {
            ATG::FatalError( "Failure from BeginCommandBuffer.\n" );
        }

        DepthRestore( m_pd3dCommandBufferDevice, i );

        if( FAILED ( m_pd3dCommandBufferDevice->EndCommandBuffer() ) )
        {
            ATG::FatalError( "Failure from EndCommandBuffer.\n" );
        }
    }

    return S_OK;
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//---------------------------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Calculate the elapsed time and clamp it to to 30fps
    FLOAT fElapsedTime = ( FLOAT )m_FrameTimer.GetElapsedTime();
    fElapsedTime = min( 1.0f / 30.0f, fElapsedTime );

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( !m_bDrawHelp )
    {
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        {
            m_bUseCommandBuffer = !m_bUseCommandBuffer;
        }

        // Switch method
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        {
            m_iDepthRestoreMethod = ( m_iDepthRestoreMethod == 0 ) 
                ? ( DEPTH_RESTORE_METHOD_COUNT - 1 ) 
                : ( m_iDepthRestoreMethod - 1 );
        }
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        {
            m_iDepthRestoreMethod = ( m_iDepthRestoreMethod == ( DEPTH_RESTORE_METHOD_COUNT - 1 ) ) 
                ? 0 
                : ( m_iDepthRestoreMethod + 1 );
        }

        // Rotate the camera using the right stick
        XMVECTOR vPrevCameraDirection = m_vCameraDirection;
        XMMATRIX matRotation = XMMatrixRotationAxis( m_vCameraRight, pGamepad->fY2 * fElapsedTime * 0.3f * XM_PI ) 
            * XMMatrixRotationAxis( m_vCameraUp, pGamepad->fX2 * fElapsedTime * 0.3f * XM_PI ); 
            
        m_vCameraDirection = XMVector3Normalize( XMVector3Transform( m_vCameraDirection, matRotation ) );
        m_vCameraRight = XMVector3Normalize( XMVector3Cross( m_vCameraUp, m_vCameraDirection ) );

        // Avoid camera lock when looking at the ceiling/sky
        const XMVECTOR vOneMinusEpsilon = XMVectorSet( 0.995f, 0.995f, 0.995f, 0.995f );
        if( XMVector3Greater( XMVectorAbs( XMVector3Dot( m_vCameraDirection, m_vCameraUp ) ), vOneMinusEpsilon ) ) 
        {
            m_vCameraDirection = vPrevCameraDirection;
        }

        // Move the camera using the left stick
        XMVECTOR vPrevCameraPosition = m_vCameraPosition;
        m_vCameraPosition += m_vCameraDirection * pGamepad->fY1 * fElapsedTime * 5.0f;
        m_vCameraPosition += m_vCameraRight * pGamepad->fX1 * fElapsedTime * 5.0f;

        // Constrain the camera position within our world size. This is just a very simple
        // collision detection to keep the camera from going outside the building, or into the central area.
        const XMVECTOR vBoundsMin = { -16.0f, 4.9f, -7.0f, 0.0f };
        const XMVECTOR vBoundsMax = {  16.0f, 8.5f,  7.0f, 0.0f };
        m_vCameraPosition = XMVectorClamp( m_vCameraPosition, vBoundsMin, vBoundsMax );

        const XMVECTOR vInnerBounds = { 11.75, 10.0f, 3.4f, 0.0f };
        if( XMVector3InBounds( m_vCameraPosition, vInnerBounds ) )
        {
            m_vCameraPosition = vPrevCameraPosition;
        }
    }

    return S_OK;
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::BeginCpuTimer()
// Desc: Start a timing bracket on the CPU
//---------------------------------------------------------------------------------------------------------
VOID Sample::BeginCpuTimer()
{
    m_CpuTimer.GetElapsedTime( );
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::EndCpuTimer()
// Desc: Conclude a timing bracket on the CPU
//---------------------------------------------------------------------------------------------------------
VOID Sample::EndCpuTimer()
{
    m_fDepthRestoreCpuTimeInMS = ( FLOAT ) m_CpuTimer.GetElapsedTime( ) * 1000.0f;

    FLOAT fLerpFactor = fabsf( m_fDepthRestoreCpuTimeInMS - m_fSmoothedDepthRestoreCpuTimeInMS ) / 10.0f;
    fLerpFactor = Max( fLerpFactor, 0.01f );
    fLerpFactor = Min( fLerpFactor, 0.9f );

    m_fSmoothedDepthRestoreCpuTimeInMS = ( 1.0f - fLerpFactor ) * m_fDepthRestoreCpuTimeInMS 
        + fLerpFactor * m_fSmoothedDepthRestoreCpuTimeInMS;
}

//---------------------------------------------------------------------------------------------------------
// Name: Sample::BeginGpuTimer()
// Desc: Start a timing bracket on the GPU
//---------------------------------------------------------------------------------------------------------
VOID Sample::BeginGpuTimer()
{
#ifndef RELEASED3D
    m_pPerfCounterBatch->Issue( D3DPERFCOUNTERINDEX_TIMESTAMP, D3DPERFQUERY_WAITGPUIDLE );
#endif
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::EndGpuTimer()
// Desc: Conclude a timing bracket on the GPU
//---------------------------------------------------------------------------------------------------------
VOID Sample::EndGpuTimer()
{
#ifndef RELEASED3D
    m_pPerfCounterBatch->Issue( D3DPERFCOUNTERINDEX_TIMESTAMP, D3DPERFQUERY_WAITGPUIDLE );

    // The Lock operation will stall.  That doesn't matter for the sample.  If this were used in a game, 
    // we'd want to triple buffer the batches.
    VOID* pData = NULL;
    m_pPerfCounterBatch->Lock( &pData );
    UINT iGpuCycles = m_pPerfCounterBatch->GetValue32( 1 ) - m_pPerfCounterBatch->GetValue32( 0 );
    m_pPerfCounterBatch->Unlock();
    m_pPerfCounterBatch->Reset();

    m_fDepthRestoreGpuTimeInMS = iGpuCycles / (FLOAT) m_iGpuCyclesPerMs;

    FLOAT fLerpFactor = fabsf( m_fDepthRestoreGpuTimeInMS - m_fSmoothedDepthRestoreGpuTimeInMS ) / 10.0f;
    fLerpFactor = Max( fLerpFactor, 0.01f );
    fLerpFactor = Min( fLerpFactor, 0.9f );

    m_fSmoothedDepthRestoreGpuTimeInMS = ( 1.0f - fLerpFactor ) * m_fDepthRestoreGpuTimeInMS 
        + fLerpFactor * m_fSmoothedDepthRestoreGpuTimeInMS;
#endif
}


//---------------------------------------------------------------------------------------------------------
// Name: RenderScene()
// Desc: Renders the scene --- standard sample code 
//---------------------------------------------------------------------------------------------------------
VOID Sample::RenderScene()
{
    PIXBeginNamedEvent( 0, "Render Scene" );

    XMMATRIX matWorldView = XMMatrixLookAtLH( m_vCameraPosition, m_vCameraPosition + m_vCameraDirection, m_vCameraUp );
    ATG::SimpleShaders::BeginShader_Transformed_DepthOnly( matWorldView * m_proj.GetMatrix() );

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );

    m_pScene->Render( m_pd3dDevice, FALSE );

    ATG::SimpleShaders::EndShader();

    PIXEndNamedEvent(); // Sample::RenderScene
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::RenderPerf()
// Desc: Print perf info to screen
//---------------------------------------------------------------------------------------------------------
VOID Sample::RenderPerf()
{
    PIXBeginNamedEvent( 0, "Render perf" );

    WCHAR strText[256];
    FLOAT fXColumnPos[2] = { 20.0f, 250.0f };
    FLOAT fYPos = 90.0f;
    m_Font.SetScaleFactors( 0.9f, 0.9f );

#ifndef RELEASED3D
    m_Font.DrawText( fXColumnPos[0], fYPos, 0xFF00FFFF, L"GPU MS Depth Restore:" );
    swprintf_s( strText, L"%3.3f", m_fSmoothedDepthRestoreGpuTimeInMS );
    m_Font.DrawText( fXColumnPos[1], fYPos, 0xFF00FFFF, strText );
    fYPos += 20;
#else
    m_Font.DrawText( fXColumnPos[0], fYPos, 0xFF8080FF, L"GPU MS:" );
    m_Font.DrawText( fXColumnPos[1], fYPos, 0xFF8080FF, L"(unavailable in release)" );
    fYPos += 20;
#endif
    m_Font.DrawText( fXColumnPos[0], fYPos, 0xFF00FFFF, L"CPU MS Depth Restore:" );
    swprintf_s( strText, L"%3.3f", m_fSmoothedDepthRestoreCpuTimeInMS );
    m_Font.DrawText( fXColumnPos[1], fYPos, 0xFF00FFFF, strText );
    fYPos += 20;

    PIXEndNamedEvent();
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::RenderUI()
// Desc: Render the screen display for the UI elements.
//---------------------------------------------------------------------------------------------------------
VOID Sample::RenderUI()
{
    PIXBeginNamedEvent( 0, "Render UI" );

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, _countof(g_HelpCallouts) );
    }
    else
    {
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 20, 0, 0xffffffff, L"FastDepthRestore" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 20, 40, 0xffffffff, L"Method:" );
        m_Font.DrawText( 120, 40, 0xffff00ff, g_strDepthRestoreMethodNames[ m_iDepthRestoreMethod ] );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 20, 60, 0xffffffff, L"Using:" );
        m_Font.DrawText( 120, 60, 0xffff00ff, m_bUseCommandBuffer ? L"Precompiled command buffer" : L"Main D3D device" );

        m_Font.End();
    }

    PIXEndNamedEvent();
}


//---------------------------------------------------------------------------------------------------------
// Name: DepthRestoreODepth()
// Desc: Restore the depth buffer using a shader which writes oDepth.  This is the simplest, but
// least-efficient method.
//
// !!! This method is not recommended for general use, and is provided here for purposes of comparison. !!!
//
// One situation where this method might be appropriate, is if a title does a moderate amount of 
// computation at the same time as restoring depth.  In that case, the computation cost may mask the 
// inefficiencies.
//---------------------------------------------------------------------------------------------------------
VOID Sample::DepthRestoreODepth( IDirect3DDevice9* pd3dDevice )
{
    PIXBeginNamedEvent( 0, "Depth restore using oDepth" );

    pd3dDevice->SetVertexDeclaration( m_pVertexDecl );

    pd3dDevice->SetVertexShader( m_pDepthRestoreODepthVS );
    pd3dDevice->SetPixelShader( m_pDepthRestoreODepthPS );

    pd3dDevice->SetDepthStencilSurface( m_pDepthStencilSurface );
    pd3dDevice->SetRenderTarget( 0, m_pDepthStencilAsColorSurface );    // Won't be used

    pd3dDevice->SetTexture( 0, m_pDepthTexture );
    pd3dDevice->SetSamplerFilterStates( 0, D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_POINT, 1 );
    pd3dDevice->SetSamplerAddressStates( 0, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );

    pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0 );
    pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_ALWAYS );
    pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    pd3dDevice->SetRenderState( D3DRS_HIZENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_HIZWRITEENABLE, TRUE );
    pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE ); // This method cannot restore stencil
    pd3dDevice->SetRenderState( D3DRS_HISTENCILENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_HISTENCILWRITEENABLE, FALSE );

    pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 
        1, 
        ScreenspaceRectangleVerts, 
        sizeof( ScreenspaceRectangleVerts[0] ) );

    pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );

    PIXEndNamedEvent();
}
    

//---------------------------------------------------------------------------------------------------------
// Name: DepthRestoreAsColor()
// Desc: Restore the depth buffer using a color render target which aliases the depth buffer.  
// Color writes through oC0 are more efficient than depth writes through oDepth.
//
// This method is recommended for titles, as it it simple and close to optimal.  
//
// Titles which are very sensitive to GPU cost, or which must restore depth or stencil more than
// once per frame may wish to instead use the method after this one.
//---------------------------------------------------------------------------------------------------------
VOID Sample::DepthRestoreAsColor( IDirect3DDevice9* pd3dDevice )
{
    PIXBeginNamedEvent( 0, "Depth restore as color" );

    pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    pd3dDevice->SetVertexShader( m_pDepthRestoreAsColorVS );
    FLOAT vBackBufferDims[] = { (FLOAT) m_d3dpp.BackBufferWidth, (FLOAT) m_d3dpp.BackBufferHeight, };
    pd3dDevice->SetVertexShaderConstantF( 0, vBackBufferDims, 1 );
    FLOAT vOffset[] = { 0.0f, 0.0f };
    pd3dDevice->SetVertexShaderConstantF( 1, vOffset, 1 );

    pd3dDevice->SetPixelShader( m_pDepthRestoreAsColorPS );

    // m_pDepthTexture and m_pDepthStencilAsColorTexture alias the same memory, so we need a guarantee 
    // that the Resolve to m_pDepthTexture completes before we read from m_pDepthStencilAsColorTexture.
    // D3D provides this guarantee --- for instance, it always inserts a GPU idle and a texture cache 
    // clear after a Resolve.
    pd3dDevice->SetTexture( 0, m_pDepthStencilAsColorTexture );
    pd3dDevice->SetSamplerFilterStates( 0, D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_POINT, 1 );
    pd3dDevice->SetSamplerAddressStates( 0, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );

    pd3dDevice->SetDepthStencilSurface( NULL );
    pd3dDevice->SetRenderTarget( 0, m_pDepthStencilAsColorSurface );

    pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_HIZENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_HIZWRITEENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_HISTENCILENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_HISTENCILWRITEENABLE, FALSE );

    pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 
        1, 
        ScreenspaceRectangleVerts, 
        sizeof( ScreenspaceRectangleVerts[0] ) );

    // Now refresh Hi-Z/Stencil
    pd3dDevice->SetPixelShader( NULL );

    pd3dDevice->SetDepthStencilSurface( m_pDepthStencilAs4xMSAASurface );
    pd3dDevice->SetRenderTarget( 0, NULL );

    pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_NEVER );
    pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_HIZENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_HIZWRITEENABLE, TRUE );
    pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_HISTENCILENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_HISTENCILWRITEENABLE, TRUE );   // Title sets Stencil/Hi-Stencil states 

    pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 
        1, 
        ScreenspaceRectangleVerts, 
        sizeof( ScreenspaceRectangleVerts[0] ) );

    PIXEndNamedEvent();
}
    

//---------------------------------------------------------------------------------------------------------
// Name: DepthRestoreAsColorWithPipelinedHiZ()
// Desc: Restore the depth buffer using a color render target which aliases the depth buffer.  
// Restore Hi-Z in parallel by operating on tile-aligned subrectangles in EDRAM.  We use rectangles
// which occupy the entire width of the render target, and which have height 32.  This is the smallest
// height for which rectangles map to the same regions of EDRAM and Hi-Z, regardless of whether
// they are standalone render targets or pieces of a larger render target.
//---------------------------------------------------------------------------------------------------------
VOID Sample::DepthRestoreAsColorWithPipelinedHiZ( IDirect3DDevice9* pd3dDevice )
{
    PIXBeginNamedEvent( 0, "Depth restore as color/Hi-Z" );

    UINT iRowWidth = m_d3dpp.BackBufferWidth;
    UINT iRowHeight = 2 * GPU_EDRAM_TILE_HEIGHT_1X; // Smallest size usable for consistent Hi-Z mapping
    UINT iEDRAMTilesPerRow = XGSurfaceSize( iRowWidth, iRowHeight, D3DFMT_D24S8, D3DMULTISAMPLE_NONE );
    UINT iHiZTilesPerRow = XGHierarchicalZSize( iRowWidth, iRowHeight, D3DMULTISAMPLE_NONE );

    // Special case for last row when depth buffer height is not a multiple of row height (for instance 720!)
    UINT iLastRowHeight = ( ( m_d3dpp.BackBufferHeight - 1 ) % iRowHeight ) + 1;
    BOOL bPartialLastRow = iLastRowHeight != iRowHeight;
    FLOAT fFullRowBottom = -1.0f;
    FLOAT fFullRowTop = 1.0f;
    FLOAT fPartialRowBottom = -( 2.0f * ( iLastRowHeight / (FLOAT) iRowHeight ) - 1.0f );
    FLOAT fPartialRowTop = fPartialRowBottom;

    // Same vertex shader used by all rows
    pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    pd3dDevice->SetVertexShader( m_pDepthRestoreAsColorVS );

    FLOAT vRowDims[] = { (FLOAT) iRowWidth, (FLOAT) iRowHeight, };
    FLOAT vRowOffset[] = { 0.0f, 0.0f };
    pd3dDevice->SetVertexShaderConstantF( 0, vRowDims, 1 );

    // m_pDepthTexture and m_pDepthStencilAsColorTexture alias the same memory, so we need a guarantee 
    // that the Resolve to m_pDepthTexture completes before we read from m_pDepthStencilAsColorTexture.
    // D3D provides this guarantee --- for instance, it always inserts a GPU idle and a texture cache 
    // clear after a Resolve.
    pd3dDevice->SetTexture( 0, m_pDepthStencilAsColorTexture );
    pd3dDevice->SetSamplerFilterStates( 0, D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_POINT, 1 );
    pd3dDevice->SetSamplerAddressStates( 0, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );

    // Temporary fake surfaces aliasing rows of the original depth buffer
    IDirect3DSurface9 DepthSurface;
    D3DSURFACE_PARAMETERS DepthSurfaceParams = m_DepthSurfaceParams;
    IDirect3DSurface9 ColorSurface;
    D3DSURFACE_PARAMETERS ColorSurfaceParams = DepthSurfaceParams;

    UINT iDepthBufferSizeInEDRAMTiles = XGSurfaceSize( m_d3dpp.BackBufferWidth, 
        m_d3dpp.BackBufferHeight, 
        D3DFMT_D24S8, 
        D3DMULTISAMPLE_NONE );

    // Restore depth row-by-row, using the color buffer to write depth, and the depth buffer to refresh Hi-Z
    while( DepthSurfaceParams.Base < m_DepthSurfaceParams.Base + iDepthBufferSizeInEDRAMTiles )
    {
        FLOAT fRowBottom = fFullRowBottom;
        FLOAT fRowTop = fFullRowTop;

        pd3dDevice->SetVertexShaderConstantF( 1, vRowOffset, 1 );

        if( ColorSurfaceParams.Base == m_DepthSurfaceParams.Base )
        {
            // First row, restore depth using color-write only.
            // Temp color buffer aliases first row of real depth buffer.
            XGSetSurfaceHeader( iRowWidth, 
                iRowHeight, 
                D3DFMT_A8R8G8B8, 
                D3DMULTISAMPLE_NONE, 
                &ColorSurfaceParams, 
                &ColorSurface, 
                0 );
            pd3dDevice->SetRenderTarget( 0, &ColorSurface );
            pd3dDevice->SetDepthStencilSurface( NULL );
            pd3dDevice->SetPixelShader( m_pDepthRestoreAsColorPS );

            // Advance color row
            ColorSurfaceParams.Base += iEDRAMTilesPerRow;
            vRowOffset[1] += iRowHeight;
        }
        else if ( ColorSurfaceParams.Base < m_DepthSurfaceParams.Base + iDepthBufferSizeInEDRAMTiles ) 
        {
            // Middle row - restore Hi-Z for row n-1 while restoring depth for row n.
            // Temp depth buffer aliases previous row of real depth buffer.
            // Temp color buffer aliases current row of real depth buffer.
            XGSetSurfaceHeader( iRowWidth, 
                iRowHeight, 
                D3DFMT_A8R8G8B8, 
                D3DMULTISAMPLE_NONE, 
                &ColorSurfaceParams, 
                &ColorSurface, 
                0 );
            XGSetSurfaceHeader( iRowWidth, 
                iRowHeight, 
                D3DFMT_D24S8, 
                D3DMULTISAMPLE_NONE, 
                &DepthSurfaceParams, 
                &DepthSurface, 
                0 );
            pd3dDevice->SetRenderTarget( 0, &ColorSurface );
            pd3dDevice->SetDepthStencilSurface( &DepthSurface );
            pd3dDevice->SetPixelShader( m_pDepthRestoreAsColorPS );

            // Special case, last row may be a partial row
            BOOL bThisIsLastRow = 
                ColorSurfaceParams.Base + iEDRAMTilesPerRow > m_DepthSurfaceParams.Base + iDepthBufferSizeInEDRAMTiles;

            // Advance depth row (unless this was a partial row)
            if( bPartialLastRow && bThisIsLastRow )
            {
                fRowBottom = fPartialRowBottom;
            }
            else
            {
                DepthSurfaceParams.Base += iEDRAMTilesPerRow;
                DepthSurfaceParams.HierarchicalZBase += iHiZTilesPerRow;
            }

            // Advance color row
            ColorSurfaceParams.Base += iEDRAMTilesPerRow;
            vRowOffset[1] += iRowHeight;
        }
        else
        {
            // Last row (or two) - restore Hi-Z only, using double-depth & 4xMSAA.
            // Temp depth buffer aliases last row (or two) of real depth buffer.
            XGSetSurfaceHeader( iRowWidth / 2, 
                iRowHeight / 2, 
                D3DFMT_D24S8, 
                D3DMULTISAMPLE_4_SAMPLES, 
                &DepthSurfaceParams, 
                &DepthSurface, 
                0 );
            pd3dDevice->SetRenderTarget( 0, NULL );
            pd3dDevice->SetDepthStencilSurface( &DepthSurface );
            pd3dDevice->SetPixelShader( NULL );

            // Special case, last row may be a partial row
            BOOL bThisIsLastRow = 
                DepthSurfaceParams.Base + iEDRAMTilesPerRow > m_DepthSurfaceParams.Base + iDepthBufferSizeInEDRAMTiles;
            if( bPartialLastRow && !bThisIsLastRow )
            {
                fRowTop = fPartialRowTop;
            }
            else if( bPartialLastRow && bThisIsLastRow )
            {
                fRowBottom = fPartialRowBottom;
            }

            DepthSurfaceParams.Base += iEDRAMTilesPerRow;
            DepthSurfaceParams.HierarchicalZBase += iHiZTilesPerRow;
        }

        // Must reset these after changing depth-stencil surface
        pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
        pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
        pd3dDevice->SetRenderState( D3DRS_HIZENABLE, FALSE );
        pd3dDevice->SetRenderState( D3DRS_HIZWRITEENABLE, TRUE );
        pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_ALWAYS );

        ScreenspaceVertex RowVerts[] = 
        {
            { XMFLOAT2( -1.0f, fRowBottom ), }, 
            { XMFLOAT2(  1.0f, fRowBottom ), }, 
            { XMFLOAT2( -1.0f,  fRowTop ), }, 
            //{ XMFLOAT2(  1.0f,  fRowTop ), },    // Needed for QUADLIST, but not RECTLIST
        };

        pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, &RowVerts, sizeof( RowVerts[0] ) );

        pd3dDevice->SetRenderTarget( 0, NULL );
        pd3dDevice->SetDepthStencilSurface( NULL );
    }

    PIXEndNamedEvent();
}
    

//---------------------------------------------------------------------------------------------------------
// Name: DepthRestore()
// Desc: Restore the depth buffer using the selected method.
//---------------------------------------------------------------------------------------------------------
VOID Sample::DepthRestore( IDirect3DDevice9* pd3dDevice, UINT iDepthRestoreMethod )
{
    switch( iDepthRestoreMethod )
    {
    case DEPTH_RESTORE_METHOD_ODEPTH:
        DepthRestoreODepth( pd3dDevice );
        break;

    case DEPTH_RESTORE_METHOD_AS_COLOR:
        DepthRestoreAsColor( pd3dDevice );
        break;

    case DEPTH_RESTORE_METHOD_AS_COLOR_WITH_PIPELINED_HIZ:
        DepthRestoreAsColorWithPipelinedHiZ( pd3dDevice );
        break;

    default:
        assert( FALSE );
        break;
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, to render the scene.
//---------------------------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    HRESULT hr = S_OK;

    PIXBeginNamedEvent( 0, "Main scene (depth only)" );

    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencilSurface );

    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0 );

    RenderScene();

    m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, m_pDepthTexture, NULL, 0, 0, NULL, 1.0f, 0, NULL );

    PIXEndNamedEvent();

    // Clear depth again to make sure we don't cheat
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0 );

    // Start timing
    BeginGpuTimer();
    BeginCpuTimer();

    if( m_bUseCommandBuffer )
    {
        m_pd3dDevice->RunCommandBuffer( m_pRestoreDepthCommandBuffer[ m_iDepthRestoreMethod ], 0 );
    }
    else
    {
        DepthRestore( m_pd3dDevice, m_iDepthRestoreMethod );
    }

    // If Hi-Z/Stencil contents were changed, then need a flush before using
    m_pd3dDevice->FlushHiZStencil( D3DFHZS_ASYNCHRONOUS );

    // End timing
    EndCpuTimer();
    EndGpuTimer();

    // Typical settings to use z-test going forward
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencilSurface );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_HIZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_HIZWRITEENABLE, TRUE );

    // Turn stencil off, unless we restored it
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_HISTENCILENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_HISTENCILWRITEENABLE, FALSE );

    // Draw final depth as screen visualization
    m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, m_pDepthTexture, NULL, 0, 0, NULL, 1.0f, 0, NULL );
    m_pd3dDevice->SetRenderTarget( 0, m_pDepthStencilAsColorSurface );
    D3DRECT rect = { 0, 0, m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight };
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( rect, m_pDepthTexture, TRUE );

    // Display CPU and GPU frame time
    RenderPerf();

    // Render the UI
    RenderUI();

    // Present the visualized depth buffer contents to the display
    IDirect3DTexture9* pFrontBuffer;
    m_pd3dDevice->GetFrontBuffer( &pFrontBuffer );
    m_pd3dDevice->SynchronizeToPresentationInterval();
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pFrontBuffer, NULL, 0, 0, NULL, 1.0f, 0L, NULL );
    m_pd3dDevice->Swap( pFrontBuffer, NULL );

    m_pd3dDevice->UnsetAll();

    return hr;
}


//---------------------------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//---------------------------------------------------------------------------------------------------------

VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Use fixed back buffer resolution regardless of output dimensions
    atgApp.m_d3dpp.BackBufferWidth = 1280;
    atgApp.m_d3dpp.BackBufferHeight = 720;

    // The sample doesn't use depth
    atgApp.m_d3dpp.EnableAutoDepthStencil = FALSE;
    atgApp.m_d3dpp.DisableAutoBackBuffer = TRUE;

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_X8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}
