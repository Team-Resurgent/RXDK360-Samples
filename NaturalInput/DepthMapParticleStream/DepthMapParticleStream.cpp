//-----------------------------------------------------------------------------
// DepthMapParticleStream.cpp
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgSimpleShaders.h>
#include <AtgNuiVisualization.h>
#include <AtgPostProcess.h>
#include <AtgNuiCommon.h>

#include <NuiApi.h>


//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_LEFTSTICK,      ATG::HELP_PLACEMENT_2, L"Move\ncamera" },
    { ATG::HELP_RIGHTSTICK,     ATG::HELP_PLACEMENT_2, L"Rotate\ncamera" },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_2, L"Animate\nemitter" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_2, L"Change\nemitter particle color" },    
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle\ndiscard band" },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_2, L"Change\nbounced particle color" },
    { ATG::HELP_START_BUTTON,   ATG::HELP_PLACEMENT_1, L"Pause" },
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_2, L"Up/down: add/remove particles\nLeft/right: scale figure" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_1, L"Change particle bouncing algorithm" },
    { ATG::HELP_LEFT_SHOULDER,  ATG::HELP_PLACEMENT_1, L"Toggle trails effect rendering" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//-----------------------------------------------------------------------------
// Custom vertex types
//-----------------------------------------------------------------------------
struct PARTICLE_VERTEX
{
    XMFLOAT3 v;
    DWORD color;
};

struct DIFFUSETEX_VERTEX
{
    XMFLOAT3 v;
    DWORD color;
    FLOAT tu;
    FLOAT tv;
};


//-----------------------------------------------------------------------------
// Global structs and data for the ground object
//-----------------------------------------------------------------------------
#define GROUND_SIZE  60.0f
#define GROUND_COLOR 0xddeeeeff

// Constants for rendering the figure under the "particle rain"
#define FIGUREBOARD_COLOR ( D3DCOLOR_ARGB( 255, 255, 0, 0 ) )
const FLOAT FIGUREBOARD_SIZE = 1.0f;

const INT DEPTH_WIDTH = 320;
const INT DEPTH_HEIGHT = 240;

// How many textures to keep in history for rendering the trails effect
#define NUM_TRAILTEX 10


//-----------------------------------------------------------------------------
// Global data for the particles
//-----------------------------------------------------------------------------
struct PARTICLE
{
    XMVECTOR m_vPos;       // Current position
    XMVECTOR m_vVel;       // Current velocity

    XMVECTOR m_vPos0;      // Initial position
    XMVECTOR m_vVel0;      // Initial velocity
    FLOAT m_fTime0;        // Time of creation

    D3DXCOLOR m_clrDiffuse; // Initial diffuse color
    D3DXCOLOR m_clrFade;    // Faded diffuse color
    FLOAT m_fFade;          // Fade progression

    BOOL m_bSpark;          // Spark? or real particle?
};


enum PARTICLE_COLORS
{
    COLOR_WHITE,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    NUM_COLORS
};


D3DXCOLOR g_clrColor[NUM_COLORS] =
{
    D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ),
    D3DXCOLOR( 1.0f, 0.5f, 0.5f, 1.0f ),
    D3DXCOLOR( 0.5f, 1.0f, 0.5f, 1.0f ),
    D3DXCOLOR( 0.125f, 0.5f, 1.0f, 1.0f )
};


DWORD g_clrColorFade[NUM_COLORS] =
{
    D3DXCOLOR( 1.0f, 0.25f, 0.25f, 1.0f ),
    D3DXCOLOR( 1.0f, 0.25f, 0.25f, 1.0f ),
    D3DXCOLOR( 0.25f, 0.75f, 0.25f, 1.0f ),
    D3DXCOLOR( 0.125f, 0.25f, 0.75f, 1.0f )
};


// # of vertex buffers for the particle system
#define NUM_PARTICLE_BUFFERS 3

enum PARTICLE_BOUNCING_ALGORITHM
{
    PARTICLE_BOUNCING_ALGORITHM_FAST,
    PARTICLE_BOUNCING_ALGORITHM_ACCURATE,
    PARTICLE_BOUNCING_ALGORITHM_NUM
};


//-----------------------------------------------------------------------------
// Name: class CParticleSystem
// Desc: The particle system class
//-----------------------------------------------------------------------------
class CParticleSystem
{
protected:
    FLOAT m_fRadius;
    PARTICLE* m_pParticles;
    DWORD m_dwMaxParticles;
    DWORD m_dwNumParticles;

    // Geometry
    LPDIRECT3DVERTEXBUFFER9 m_pPointSpritesVBs[NUM_PARTICLE_BUFFERS];
    LPDIRECT3DVERTEXBUFFER9 m_pLightsVBs[NUM_PARTICLE_BUFFERS];
    LPDIRECT3DVERTEXBUFFER9 m_pPointSpritesVB;
    LPDIRECT3DVERTEXBUFFER9 m_pLightsVB;
    DWORD m_dwCurrentBuffer;
    DWORD m_dwNumParticlesToRender;
    DWORD m_dwNumLightsToRender;

public:
                            CParticleSystem( DWORD dwMaxParticles, FLOAT fRadius );
                            ~CParticleSystem();

    HRESULT                 InitDeviceObjects();
    HRESULT                 DeleteDeviceObjects();

    HRESULT                 Update( FLOAT fSecsPerFrame, DWORD dwNumParticlesToEmit,
                                    const D3DXCOLOR& dwEmitColor, const D3DXCOLOR& dwFadeColor,
                                    const D3DXCOLOR& dwBounceColor, const D3DXCOLOR& dwBounceFadeColor,
                                    FLOAT fEmitVel, XMVECTOR vPosition, 
                                    IDirect3DTexture9* m_pSegEdgeUntiledTexture, FLOAT scale, PARTICLE_BOUNCING_ALGORITHM mode );

    HRESULT                 RenderParticles();
    HRESULT                 RenderLights();
};


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::PackedResource m_Resource;
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;        // Whether to draw help

    // Particle system timing
    BOOL m_bParticleSystemRunning;
    FLOAT m_fParticleSystemTime;
    FLOAT m_fElapsedParticleSystemTime;

    // Particle stuff
    LPDIRECT3DTEXTURE9 m_pParticleTexture;
    CParticleSystem* m_pParticleSystem;
    FLOAT m_fNumParticlesToEmitPerSec;
    DWORD m_dwParticleColor;
    DWORD m_dwBounceParticleColor;
    BOOL m_bAnimateEmitter;

    BOOL m_bEnableDiscardBand;

    // Ground stuff
    LPDIRECT3DVERTEXBUFFER9 m_pGroundVB;
    LPDIRECT3DTEXTURE9 m_pGroundTexture;
    DWORD   m_Pad[2];
    XMVECTOR m_vGroundPlane;

    // Static vectors for determining view position
    XMVECTOR m_vPosition;
    XMVECTOR m_vVelocity;
    FLOAT m_fYaw;
    FLOAT m_fYawVelocity;
    FLOAT m_fPitch;
    FLOAT m_fPitchVelocity;

    XMMATRIX m_matOrientation;
    XMMATRIX m_matWorld;
    XMMATRIX m_matProj;
    XMMATRIX m_matView;
    XMMATRIX m_matReflectedView;
    XMMATRIX m_matReflectedWVPT;
    XMMATRIX m_matWVPT;

    // Shaders
    LPDIRECT3DVERTEXDECLARATION9 m_pDiffuseTexVtxDecl;
    LPDIRECT3DVERTEXSHADER9 m_pDiffuseTexVS;
    LPDIRECT3DPIXELSHADER9 m_pTexModDiffusePS;
    LPDIRECT3DVERTEXDECLARATION9 m_pParticleVertexDecl;
    LPDIRECT3DVERTEXSHADER9 m_pParticleVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pParticlePixelShader;

    //////////////////////////////////////////////////////////////////////////
    // Added members for DepthMapParticleStream sample
    //////////////////////////////////////////////////////////////////////////

    ATG::NuiVisualization       m_pip;
    HANDLE                      m_hImage;
    HANDLE                      m_hDepth;
    HANDLE                      m_hFrameEndEvent;

    LPDIRECT3DVERTEXBUFFER9     m_pFigureboardVB;      // Vertex buffer for billboard
    FLOAT                       m_fFigureBoardScale;

    IDirect3DTexture9*          m_pSegTexture;
    IDirect3DTexture9*          m_pSegEdgeTexture;    
    IDirect3DTexture9*          m_pSegEdgeUntiledTexture;

    IDirect3DTexture9*          m_pSegEdgeTrailTexture[NUM_TRAILTEX];
    INT                         m_TrailTextureIdx;

    ATG::PostProcess            m_PostProcess;       // Commonly used effects (blur, etc.)

    LPDIRECT3DPIXELSHADER9      m_pGetSegmentationFromDepthTexturePS;
    LPDIRECT3DPIXELSHADER9      m_pEdgeDetectPS;

    LPDIRECT3DPIXELSHADER9      m_pRenderTrailPS;
    BOOL                        m_bRenderTrail;

    PARTICLE_BOUNCING_ALGORITHM   m_ParticleBouncingAlg;

    VOID CopyTexturePointSampling( LPDIRECT3DTEXTURE9 pSrcTexture,
                                   LPDIRECT3DTEXTURE9 pDstTexture,
                                   LPDIRECT3DPIXELSHADER9 pPixelShader,
                                   DWORD dwEdramOffset = 0 );

private:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();

    HRESULT StartupCamera();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    atgApp.Run();
}


HRESULT Sample::StartupCamera()
{
    m_hFrameEndEvent = CreateEvent( NULL,
                                    FALSE,  // auto-reset
                                    FALSE,  // create unsignaled
                                    "NuiFrameEndEvent" );
    if ( !m_hFrameEndEvent )
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent\n" );
        return E_FAIL;
    }

    // Initializes the Natural Input system on the default thread
    
    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_COLOR |
                                NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX,
                                NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );
    if ( FAILED(hr) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }

    // Set the frame processing ended event
    hr = NuiSetFrameEndEvent( m_hFrameEndEvent, 0 );
    if ( FAILED(hr) )
    {
        ATG::NuiPrintError( hr, "NuiSetFrameEndEvent" );
        return E_FAIL;
    }

    // Open the color stream
	hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, 0, 1, NULL, &m_hImage );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    // Open the depth stream
	hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, NUI_IMAGE_RESOLUTION_320x240, 0, 1, NULL, &m_hDepth );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    hr = m_pip.Initialize( m_pd3dDevice, 
                           NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX,
                           NUI_IMAGE_RESOLUTION_640x480 );
    if FAILED( hr )
    {
        ATG_PrintError ( "Picture in Picture initialization failed" );
	}

    return hr;
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize the app
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    hr = StartupCamera();
    if ( FAILED( hr ) )
        return hr;

    // Init member variables
    m_bDrawHelp = FALSE;

    m_bParticleSystemRunning = TRUE;
    m_fParticleSystemTime = 0.0f;
    m_fElapsedParticleSystemTime = 0.0f;

    m_pGroundTexture = NULL;
    m_pGroundVB = NULL;
    m_vGroundPlane = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

    m_pParticleTexture = NULL;
    m_pParticleSystem = new CParticleSystem( 4096*2, 0.03f );
    m_fNumParticlesToEmitPerSec = 1000.0f;
    m_bAnimateEmitter = FALSE;
    m_bEnableDiscardBand = TRUE;
    m_dwParticleColor = COLOR_BLUE;
    m_dwBounceParticleColor = COLOR_WHITE;

    m_vPosition = XMVectorSet( 0.0f, 3.0f, -4.0f, 0.0f );
    m_vVelocity = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    m_fYaw = 0.0f;
    m_fYawVelocity = 0.0f;
    m_fPitch = 0.5f;
    m_fPitchVelocity = 0.0f;
    m_matView = XMMatrixTranslation( 0.0f, 0.0f, 10.0f );
    m_matOrientation = XMMatrixTranslation( 0.0f, 0.0f, 0.0f );

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the resources
    if( FAILED( m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create textures
    m_pGroundTexture = m_Resource.GetTexture( "Ground" );
    m_pParticleTexture = m_Resource.GetTexture( "Particle" );

    // Create vertex buffer for ground object
    hr = m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( DIFFUSETEX_VERTEX ),
                                           D3DUSAGE_WRITEONLY, 0L,
                                           D3DPOOL_MANAGED, &m_pGroundVB, NULL );
    if( FAILED( hr ) )
        return E_FAIL;

    // Fill vertex buffer
    DIFFUSETEX_VERTEX* pVertices;
    m_pGroundVB->Lock( 0, 0, ( VOID** )&pVertices, NULL );
    pVertices[0].v = XMFLOAT3( -GROUND_SIZE / 2, 0.0f, -GROUND_SIZE / 2 );
    pVertices[0].color = GROUND_COLOR;
    pVertices[0].tu = 0.0f;
    pVertices[0].tv = 0.0f;
    pVertices[1].v = XMFLOAT3( -GROUND_SIZE / 2, 0.0f, +GROUND_SIZE / 2 );
    pVertices[1].color = GROUND_COLOR;
    pVertices[1].tu = 0.0f;
    pVertices[1].tv = 9.0f;
    pVertices[2].v = XMFLOAT3( +GROUND_SIZE / 2, 0.0f, +GROUND_SIZE / 2 );
    pVertices[2].color = GROUND_COLOR;
    pVertices[2].tu = 9.0f;
    pVertices[2].tv = 9.0f;
    pVertices[3].v = XMFLOAT3( +GROUND_SIZE / 2, 0.0f, -GROUND_SIZE / 2 );
    pVertices[3].color = GROUND_COLOR;
    pVertices[3].tu = 9.0f;
    pVertices[3].tv = 0.0f;
    m_pGroundVB->Unlock();

    // Create the common vertex shader
    {
        static const D3DVERTEXELEMENT9 decl[] =
        {
            { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
            { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR,    0 },
            { 0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };
        m_pd3dDevice->CreateVertexDeclaration( decl, &m_pDiffuseTexVtxDecl );

        if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\PositionDiffuseTexcoord.xvu",
                                                &m_pDiffuseTexVS ) ) )
        {
            ATG_PrintError( "Couldn't create CommonVS.xvu\n" );
            return ATGAPPERR_MEDIANOTFOUND;
        }
    }

    // Create the common pixel shader
    {
        if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\TextureModDiffuse.xpu",
                                               &m_pTexModDiffusePS ) ) )
        {
            ATG_PrintError( "Couldn't create TextureModDiffuse.xpu\n" );
            return ATGAPPERR_MEDIANOTFOUND;
        }
    }

    // Create the vertex shader for the particles
    {
        static const D3DVERTEXELEMENT9 decl[] =
        {
            { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
            { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR,    0 },
            D3DDECL_END()
        };
        m_pd3dDevice->CreateVertexDeclaration( decl, &m_pParticleVertexDecl );

        if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\PointSprite.xvu",
                                                &m_pParticleVertexShader ) ) )
            return ATGAPPERR_MEDIANOTFOUND;
    }

    // Create the pixel shaders for rendering the pointsprites
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\PointSprite.xpu",
                                      &m_pParticlePixelShader ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize the particle system
    if( FAILED( hr = m_pParticleSystem->InitDeviceObjects() ) )
        return hr;

    // Set the world matrix
    m_matWorld = XMMatrixIdentity();

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set projection matrix
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, 0.1f, 100.0f );

    //////////////////////////////////////////////////////////////////////////
    // Added for DepthMapParticleStream sample
    //////////////////////////////////////////////////////////////////////////
    
    m_fFigureBoardScale = 2.0f;
    m_ParticleBouncingAlg = PARTICLE_BOUNCING_ALGORITHM_FAST;
    m_TrailTextureIdx = 0;
    m_bRenderTrail = TRUE;

    if( FAILED( m_PostProcess.Initialize() ) )
    {
        ATG_PrintError( "DX9Sample: Couldn't initialize the effects library\n" );
        return E_FAIL;
    }    

    hr = m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( DIFFUSETEX_VERTEX ),
                                           D3DUSAGE_WRITEONLY, 0L,
                                           D3DPOOL_MANAGED, &m_pFigureboardVB, NULL );
    if ( FAILED( hr ) )
        return E_FAIL;    
    
    m_pFigureboardVB->Lock( 0, 0, (VOID**)&pVertices, NULL );        
    pVertices[0].v = XMFLOAT3( -FIGUREBOARD_SIZE / 2, FIGUREBOARD_SIZE, 0.0f );        
    pVertices[0].color = FIGUREBOARD_COLOR;
    pVertices[0].tu = 0.0f;
    pVertices[0].tv = 0.0f;
    pVertices[1].v = XMFLOAT3( +FIGUREBOARD_SIZE / 2, FIGUREBOARD_SIZE, 0.0f );
    pVertices[1].color = FIGUREBOARD_COLOR;
    pVertices[1].tu = 1.0f;
    pVertices[1].tv = 0.0f;
    pVertices[2].v = XMFLOAT3( +FIGUREBOARD_SIZE / 2, 0.0f, 0.0f);
    pVertices[2].color = FIGUREBOARD_COLOR;
    pVertices[2].tu = 1.0f;
    pVertices[2].tv = 1.0f;        
    pVertices[3].v = XMFLOAT3( -FIGUREBOARD_SIZE / 2, 0.0f, 0.0f );
    pVertices[3].color = FIGUREBOARD_COLOR;
    pVertices[3].tu = 0.0f;
    pVertices[3].tv = 1.0f;
    m_pFigureboardVB->Unlock();
        
    // Create textures
    if ( FAILED( m_pd3dDevice->CreateTexture( DEPTH_WIDTH, DEPTH_HEIGHT, 1, 0, D3DFMT_A8R8G8B8, 0, &m_pSegTexture, NULL ) ) )
        return E_FAIL;
    if ( FAILED( m_pd3dDevice->CreateTexture( DEPTH_WIDTH, DEPTH_HEIGHT, 1, 0, D3DFMT_A8R8G8B8, 0, &m_pSegEdgeTexture, NULL ) ) )
        return E_FAIL;
    for ( INT i = 0; i < NUM_TRAILTEX; ++i )
    {
        if ( FAILED( m_pd3dDevice->CreateTexture( DEPTH_WIDTH, DEPTH_HEIGHT, 1, 0, D3DFMT_A8R8G8B8, 0, &m_pSegEdgeTrailTexture[i], NULL ) ) )
            return E_FAIL;

        m_PostProcess.ClearTexture( m_pSegEdgeTrailTexture[i] );
    }
    if ( FAILED( m_pd3dDevice->CreateTexture( DEPTH_WIDTH, DEPTH_HEIGHT, 1, 0, D3DFMT_LIN_A8R8G8B8, 0, &m_pSegEdgeUntiledTexture, NULL ) ) )
        return E_FAIL;
    
    // Create the pixel shaders
    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\GetSegmentationFromDepthTexture.xpu",
        &m_pGetSegmentationFromDepthTexturePS ) ) )
    {
        ATG_PrintError( "Couldn't create GetSegmentationFromDepthTexture.xpu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\EdgeDetect.xpu",
        &m_pEdgeDetectPS ) ) )
    {
        ATG_PrintError( "Couldn't create EdgeDetect.xpu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\RenderTrail.xpu",
        &m_pRenderTrailPS ) ) )
    {
        ATG_PrintError( "Couldn't create RenderTrail.xpu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }    

    return S_OK;
}

VOID Sample::CopyTexturePointSampling( LPDIRECT3DTEXTURE9 pSrcTexture,
                                       LPDIRECT3DTEXTURE9 pDstTexture,
                                       LPDIRECT3DPIXELSHADER9 pPixelShader,
                                       DWORD dwEdramOffset )
{
    // Make sure that the required shaders and objects exist
    assert( pPixelShader );
    assert( pSrcTexture && pDstTexture );

    XGTEXTURE_DESC SrcDesc;
    XGGetTextureDesc( pSrcTexture, 0, &SrcDesc );

    // Create and set a render target
    D3DSURFACE_PARAMETERS surfaceParams =
    {
        0
    };
    surfaceParams.Base = dwEdramOffset;
    ATG::PushRenderTarget( 0L, ATG::CreateRenderTarget( pDstTexture, &surfaceParams ) );

    // Scale and copy the src texture
    m_pd3dDevice->SetPixelShader( pPixelShader );

    m_pd3dDevice->SetTexture( 0, pSrcTexture );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    // Draw a fullscreen quad to sample the RT
    m_PostProcess.DrawFullScreenQuad();

    XGTEXTURE_DESC DstDesc;
    XGGetTextureDesc( pDstTexture, 0, &DstDesc );
    DWORD ColorExpBias = 0;

    if( DstDesc.Format == ATG::D3DFMT_G16R16_SIGNED_INTEGER )            ColorExpBias = ( DWORD )
        D3DRESOLVE_EXPONENTBIAS( 10 );
    else if( DstDesc.Format == ATG::D3DFMT_A16B16G16R16_SIGNED_INTEGER ) ColorExpBias = ( DWORD )
        D3DRESOLVE_EXPONENTBIAS( 10 );

    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | ColorExpBias, NULL, pDstTexture, NULL,
        0, 0, NULL, 1.0f, 0L, NULL );

    // Cleanup and return
    ATG::PopRenderTarget( 0L )->Release();
    m_pd3dDevice->SetPixelShader( NULL );
}


//-----------------------------------------------------------------------------
// Name: Update()
// Desc: Animate the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT m_fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // De-accelerate the camera movement (for smooth motion)
    FLOAT fScale = max( 0.0f, ( 1.0f - 2.0f * m_fElapsedTime ) );
    m_vVelocity *= fScale;
    m_fYawVelocity *= fScale;
    m_fPitchVelocity *= fScale;

    // Update velocities from the gamepad
    m_vVelocity.x += 1.0f * m_fElapsedTime * pGamepad->fX1; // Slide left/right
    m_vVelocity.y += 1.0f * m_fElapsedTime * pGamepad->fY1; // Slide up/down
    m_vVelocity.z -= 0.004f * pGamepad->bLeftTrigger * m_fElapsedTime;
    m_vVelocity.z += 0.004f * pGamepad->bRightTrigger * m_fElapsedTime;

    m_fYawVelocity += 1.0f * m_fElapsedTime * pGamepad->fX2; // Turn left/right
    m_fPitchVelocity -= 1.0f * m_fElapsedTime * pGamepad->fY2; // Turn up/down

    // Handle options
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        m_fNumParticlesToEmitPerSec *= 1.1f;
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        m_fNumParticlesToEmitPerSec *= 0.9f;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        m_fFigureBoardScale *= 1.1f;
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        m_fFigureBoardScale *= 0.9f;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_bAnimateEmitter = !m_bAnimateEmitter;
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_dwParticleColor = ( m_dwParticleColor + 1 ) % NUM_COLORS;
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        m_dwBounceParticleColor = ( m_dwBounceParticleColor + 1 ) % NUM_COLORS;
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        m_bEnableDiscardBand = !m_bEnableDiscardBand;

    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
        m_ParticleBouncingAlg = (PARTICLE_BOUNCING_ALGORITHM)(((INT)m_ParticleBouncingAlg + 1) % PARTICLE_BOUNCING_ALGORITHM_NUM);

    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
        m_bRenderTrail = !m_bRenderTrail;

    // Update the position vector
    XMVECTOR vT = m_vVelocity * 5.0f * m_fElapsedTime;
    vT = XMVector3TransformNormal( vT, m_matOrientation );
    m_vPosition += vT;
    if( m_vPosition.y < 1.0f )
        m_vPosition.y = 1.0f;

    // Update the yaw-pitch-rotation vector
    m_fYaw += 5.0f * m_fElapsedTime * m_fYawVelocity;
    m_fPitch += 5.0f * m_fElapsedTime * m_fPitchVelocity;
    if( m_fPitch < 0.0f )    m_fPitch = 0.0f;
    if( m_fPitch > XM_PI / 2 ) m_fPitch = XM_PI / 2;

    m_matWorld = XMMatrixIdentity();

    // Set the view matrix
    XMVECTOR vDeterminant;
    XMVECTOR qR;
    qR = XMQuaternionRotationRollPitchYaw( m_fPitch, m_fYaw, 0.0f );
    m_matOrientation = XMMatrixAffineTransformation( XMVectorSet( 1.25f, 1.25f, 1.25f, 1.0f ),
                                                     XMVectorZero(), qR, m_vPosition );
    m_matView = XMMatrixInverse( &vDeterminant, m_matOrientation );

    // Computed the reflected view
    m_matReflectedView = XMMatrixReflect( m_vGroundPlane );
    m_matReflectedView = XMMatrixMultiply( m_matReflectedView, m_matView );

    // Build composite matrices
    m_matWVPT = XMMatrixMultiply( m_matWorld, m_matView );
    m_matWVPT = XMMatrixMultiply( m_matWVPT, m_matProj );
    m_matWVPT = XMMatrixTranspose( m_matWVPT );

    m_matReflectedWVPT = XMMatrixMultiply( m_matWorld, m_matReflectedView );
    m_matReflectedWVPT = XMMatrixMultiply( m_matReflectedWVPT, m_matProj );
    m_matReflectedWVPT = XMMatrixTranspose( m_matReflectedWVPT );

    // Check the Start button to start/stop the particle system
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
        m_bParticleSystemRunning = !m_bParticleSystemRunning;

    if( m_bParticleSystemRunning )
        m_fElapsedParticleSystemTime = m_fElapsedTime;
    else
        m_fElapsedParticleSystemTime = 0.0f;
    m_fParticleSystemTime += m_fElapsedParticleSystemTime;

    // Determine emitter position
    XMVECTOR vEmitterPostion = XMVectorSet( 0, 2, 0, 1 ); //XMVectorZero();
    if( m_bAnimateEmitter )
        //vEmitterPostion = XMVectorSet( 3 * sinf( m_fParticleSystemTime ), 0.0f, 3 * cosf( m_fParticleSystemTime ), 1.0f );
        vEmitterPostion = XMVectorSet( 2 * sinf( m_fParticleSystemTime ), 2.0f, 0, 1.0f );

    // Determine framerate-independent number of particles to emit this frame
    static FLOAT fNumParticlesToEmit = 0.0f;
    fNumParticlesToEmit += m_fNumParticlesToEmitPerSec * m_fElapsedTime;
    DWORD dwNumParticlesToEmitThisFrame = ( DWORD )floorf( fNumParticlesToEmit );
    fNumParticlesToEmit -= dwNumParticlesToEmitThisFrame;

    // Update particle system
    m_pParticleSystem->Update( m_fElapsedParticleSystemTime, dwNumParticlesToEmitThisFrame,
                               g_clrColor[m_dwParticleColor], g_clrColorFade[m_dwParticleColor],
                               g_clrColor[m_dwBounceParticleColor], g_clrColorFade[m_dwBounceParticleColor],
                               8.0f, vEmitterPostion, 
                               m_pSegEdgeUntiledTexture, m_fFigureBoardScale, m_ParticleBouncingAlg );

    //////////////////////////////////////////////////////////////////////////
    // Added for DepthMapParticleStream sample
    //////////////////////////////////////////////////////////////////////////

    if ( WAIT_OBJECT_0 != WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        return E_FAIL;
    }
    
    // Get data from the next image frame
    CONST NUI_IMAGE_FRAME* m_pImageFrame = NULL;
    HRESULT hrImage = NuiImageStreamGetNextFrame( m_hImage, 0, &m_pImageFrame );
    // Get data from the next camera depth frame
    CONST NUI_IMAGE_FRAME* m_pDepthFrame = NULL;
    HRESULT hrDepth = NuiImageStreamGetNextFrame( m_hDepth, 0, &m_pDepthFrame );

    if ( SUCCEEDED( hrImage ) )
    {
        m_pip.SetColorTexture( m_pImageFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( m_hImage, m_pImageFrame );
    }

    if ( SUCCEEDED( hrDepth ) )
    {
        if ( m_bParticleSystemRunning )
        {
            // Extract the segmentation (player idx) map from the depth image
            CopyTexturePointSampling( m_pDepthFrame->pFrameTexture, m_pSegTexture, m_pGetSegmentationFromDepthTexturePS );
            m_pd3dDevice->SetTexture( 0, NULL );

            // Blur the extracted segmentation image several times to reduce noise and holes
            m_PostProcess.GaussBlur5x5Texture( m_pSegTexture, m_pSegTexture );
            m_PostProcess.GaussBlur5x5Texture( m_pSegTexture, m_pSegTexture );
            m_PostProcess.GaussBlur5x5Texture( m_pSegTexture, m_pSegTexture );
            m_PostProcess.GaussBlur5x5Texture( m_pSegTexture, m_pSegTexture );
            m_PostProcess.GaussBlur5x5Texture( m_pSegTexture, m_pSegTexture );
            m_PostProcess.GaussBlur5x5Texture( m_pSegTexture, m_pSegTexture );
            m_PostProcess.GaussBlur5x5Texture( m_pSegTexture, m_pSegTexture );
            m_PostProcess.GaussBlur5x5Texture( m_pSegTexture, m_pSegTexture );

            // Run edge detection on the blurred segmentation image
            m_PostProcess.CopyTexture( m_pSegTexture, m_pSegEdgeTexture, m_pEdgeDetectPS );
            m_pd3dDevice->SetTexture( 0, NULL );

            // Untile the result texture from the previous step,
            // so that the CPU can update the particle system against this texture
            D3DLOCKED_RECT src, dst;
            m_pSegEdgeTexture->LockRect( 0, &src, NULL, 0 );
            m_pSegEdgeUntiledTexture->LockRect( 0, &dst, NULL, 0 );
            XGUntileTextureLevel( DEPTH_WIDTH, DEPTH_HEIGHT, 0, XGGetGpuFormat(D3DFMT_LIN_A8R8G8B8), 0, dst.pBits, dst.Pitch, NULL, src.pBits, NULL );
            m_pSegEdgeTexture->UnlockRect( 0 );
            m_pSegEdgeUntiledTexture->UnlockRect( 0 );
            
            // For trails effect
            if ( m_bRenderTrail )
            {
                m_PostProcess.GaussBlur5x5Texture( m_pSegEdgeTexture, m_pSegEdgeTrailTexture[m_TrailTextureIdx] );
                ++m_TrailTextureIdx;
                if ( m_TrailTextureIdx >= NUM_TRAILTEX ) m_TrailTextureIdx = 0;
            }            
        }        

        m_pip.SetDepthTexture( m_pDepthFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( m_hDepth, m_pDepthFrame );
    }   

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Render the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                         0x00000000, 1.0f, 0L );

    // Set common shaders
    m_pd3dDevice->SetPixelShader( m_pTexModDiffusePS );
    m_pd3dDevice->SetVertexDeclaration( m_pDiffuseTexVtxDecl );
    m_pd3dDevice->SetVertexShader( m_pDiffuseTexVS );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m_matWVPT, 4 );

    // Set state for rendering the ground
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // Draw the ground
    m_pd3dDevice->SetTexture( 0, m_pGroundTexture );
    m_pd3dDevice->SetStreamSource( 0, m_pGroundVB, 0, sizeof( DIFFUSETEX_VERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );        

    // Modify state for rendering particles and lights
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAREF, 0x10 );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );

    // Render the ground effect lights
    m_pd3dDevice->SetTexture( 0, m_pParticleTexture );
    m_pParticleSystem->RenderLights();

    // Modify state for rendering particels
    m_pd3dDevice->SetRenderState( D3DRS_POINTSPRITEENABLE, TRUE );
    m_pd3dDevice->SetVertexDeclaration( m_pParticleVertexDecl );
    m_pd3dDevice->SetVertexShader( m_pParticleVertexShader );
    m_pd3dDevice->SetPixelShader( m_pParticlePixelShader );

    // Compute discard band. Normal TV overscan makes this pretty worthless to do.
    const FLOAT fPointSpriteRadius = 48.0f / 2.0f;
    const FLOAT fScreenHalfWidth = 640.0f / 2.0f;
    const FLOAT fScreenHalfHeight = 480.0f / 2.0f;

    if( m_bEnableDiscardBand )
    {
        FLOAT fDiscardX = GPU_GUARDBANDFACTOR( fPointSpriteRadius, fScreenHalfWidth );
        FLOAT fDiscardY = GPU_GUARDBANDFACTOR( fPointSpriteRadius, fScreenHalfHeight );

        m_pd3dDevice->SetRenderState( D3DRS_DISCARDBAND_X, *( DWORD* )&fDiscardX );
        m_pd3dDevice->SetRenderState( D3DRS_DISCARDBAND_Y, *( DWORD* )&fDiscardY );
    }

    // Render the particles
    static FLOAT fOpacity = 1.0f;
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m_matWVPT, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&fOpacity, 1 );
    m_pParticleSystem->RenderParticles();

    // Draw reflection of particles
    static FLOAT fOpacityR = 0.25f;
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m_matReflectedWVPT, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&fOpacityR, 1 );
    m_pParticleSystem->RenderParticles();

    // Restore state
    m_pd3dDevice->SetPixelShader( NULL );
    m_pd3dDevice->SetRenderState( D3DRS_POINTSPRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );

    if( m_bEnableDiscardBand )
    {
        FLOAT fOne = 1.0f;
        m_pd3dDevice->SetRenderState( D3DRS_DISCARDBAND_X, *( DWORD* )&fOne );
        m_pd3dDevice->SetRenderState( D3DRS_DISCARDBAND_Y, *( DWORD* )&fOne );
    }    

    //////////////////////////////////////////////////////////////////////////
    // Added for DepthMapParticleStream sample
    //////////////////////////////////////////////////////////////////////////

    {
        // Render the figure board

        // Build composite matrices
        m_matWorld = XMMatrixScaling( m_fFigureBoardScale, m_fFigureBoardScale, m_fFigureBoardScale );

        m_matWVPT = XMMatrixMultiply( m_matWorld, m_matView );
        m_matWVPT = XMMatrixMultiply( m_matWVPT, m_matProj );
        m_matWVPT = XMMatrixTranspose( m_matWVPT );

        // Set common shaders
        m_pd3dDevice->SetVertexDeclaration( m_pDiffuseTexVtxDecl );
        m_pd3dDevice->SetVertexShader( m_pDiffuseTexVS );
        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m_matWVPT, 4 );

        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );

        if ( m_bRenderTrail )
        {            
            m_pd3dDevice->SetPixelShader( m_pRenderTrailPS );

            // Set state for rendering the figure board
            for ( INT i = 0; i < NUM_TRAILTEX; ++i )
            {
                m_pd3dDevice->SetSamplerState( i, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
                m_pd3dDevice->SetSamplerState( i, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
                m_pd3dDevice->SetSamplerState( i, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
                m_pd3dDevice->SetSamplerState( i, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
                m_pd3dDevice->SetSamplerState( i, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
            }        

            m_pd3dDevice->SetStreamSource( 0, m_pFigureboardVB, 0, sizeof( DIFFUSETEX_VERTEX ) );

            for ( INT i = 0; i < NUM_TRAILTEX; ++i )
                m_pd3dDevice->SetTexture( i, m_pSegEdgeTrailTexture[(i + m_TrailTextureIdx) % NUM_TRAILTEX] );

            // Draw the figure board            
            m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );

            for ( INT i = 0; i < NUM_TRAILTEX; ++i )
                m_pd3dDevice->SetTexture( i, NULL );
        } else
        {
            m_pd3dDevice->SetPixelShader( m_pTexModDiffusePS );

            // Set state for rendering the figure board
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
                
            m_pd3dDevice->SetStreamSource( 0, m_pFigureboardVB, 0, sizeof( DIFFUSETEX_VERTEX ) );

            m_pd3dDevice->SetTexture( 0, m_pSegEdgeTexture );            

            // Draw the figure board            
            m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );                     
        }

        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    }    
    
    // Draw the raw depth and image map with skeleton overlaid as visualization.
    const FLOAT drawWidth = 200.0f;
    const FLOAT drawHeight = drawWidth * 3.0f / 4.0f;
    const FLOAT drawX = 50.0f;
    const FLOAT drawY = 720.0f - 50.0f - drawHeight;
    m_pip.BeginRender();
    m_pip.RenderColorStream( drawX, drawY, drawWidth, drawHeight );
    m_pip.RenderDepthStream( drawX + drawWidth + 10, drawY, drawWidth, drawHeight );
    m_pip.EndRender();    

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        if ( m_ParticleBouncingAlg == PARTICLE_BOUNCING_ALGORITHM_FAST )
            m_Font.DrawText( 0, 0, 0xffffffff, L"DepthMapParticleStream - Fast Bouncing" );
        else
            m_Font.DrawText( 0, 0, 0xffffffff, L"DepthMapParticleStream - Accurate Bouncing" );
        if ( m_bRenderTrail )
            m_Font.DrawText( 0, 25, 0xffffffff, L"Trails effect rendering: ON" );
        else
            m_Font.DrawText( 0, 25, 0xffffffff, L"Trails effect rendering: OFF" );        
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CParticleSystem()
// Desc: Constructur for the particle system
//-----------------------------------------------------------------------------
CParticleSystem::CParticleSystem( DWORD dwMaxParticles, FLOAT fRadius )
{
    m_fRadius = fRadius;

    m_pParticles = new PARTICLE[dwMaxParticles];
    m_dwMaxParticles = dwMaxParticles;
    m_dwNumParticles = 0;

    m_pPointSpritesVB = NULL;
    m_pLightsVB = NULL;
    m_dwCurrentBuffer = 0;

    m_dwNumParticlesToRender = 0;
    m_dwNumLightsToRender = 0;
}


//-----------------------------------------------------------------------------
// Name: ~CParticleSystem()
// Desc: Destructor for the particle system
//-----------------------------------------------------------------------------
CParticleSystem::~CParticleSystem()
{
    DeleteDeviceObjects();

    if( m_pParticles )
        delete[] m_pParticles;
}


//-----------------------------------------------------------------------------
// Name: InitDeviceObjects()
// Desc: Initialize device-dependent objects
//-----------------------------------------------------------------------------
HRESULT CParticleSystem::InitDeviceObjects()
{
    // Create the particle texture
    // Create the particle system's vertex buffers. Each point sprite particle
    // requires one vertex and each light takes four vertices.
    for( DWORD buf = 0; buf < NUM_PARTICLE_BUFFERS; buf++ )
    {
        ATG::g_pd3dDevice->CreateVertexBuffer( ( m_dwMaxParticles + 1 ) * sizeof( PARTICLE_VERTEX ),
                                               D3DUSAGE_WRITEONLY, 0L,
                                               D3DPOOL_DEFAULT, &m_pPointSpritesVBs[buf], NULL );
        ATG::g_pd3dDevice->CreateVertexBuffer( 4 * ( m_dwMaxParticles + 1 ) * sizeof( DIFFUSETEX_VERTEX ),
                                               D3DUSAGE_WRITEONLY, 0L,
                                               D3DPOOL_DEFAULT, &m_pLightsVBs[buf], NULL );

        // Write default values to light vertices (the texture coordinates for
        // the quads never changes)
        DIFFUSETEX_VERTEX* pLightVertices;
        m_pLightsVBs[buf]->Lock( 0, 0, ( VOID** )&pLightVertices, NULL );
        ZeroMemory( pLightVertices, 4 * m_dwMaxParticles * sizeof( DIFFUSETEX_VERTEX ) );

        for( DWORD i = 0; i < m_dwMaxParticles; i++ )
        {
            pLightVertices[1].tv = 1.0f;
            pLightVertices[2].tu = 1.0f;
            pLightVertices[2].tv = 1.0f;
            pLightVertices[3].tu = 1.0f;
            pLightVertices += 4;
        }

        // Unlock the vertex buffer
        m_pLightsVBs[buf]->Unlock();
    }

    // Select starting vertex buffers
    m_dwCurrentBuffer = 0;
    m_pPointSpritesVB = m_pPointSpritesVBs[m_dwCurrentBuffer];
    m_pLightsVB = m_pLightsVBs[m_dwCurrentBuffer];

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: DeleteDeviceObjects()
// Desc: Delete device dependent objects
//-----------------------------------------------------------------------------
HRESULT CParticleSystem::DeleteDeviceObjects()
{
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Update()
// Desc: Update the particles in the particle system.
//-----------------------------------------------------------------------------
HRESULT CParticleSystem::Update( FLOAT fSecsPerFrame, DWORD dwNumParticlesToEmit,
                                 const D3DXCOLOR& clrEmitColor, const D3DXCOLOR& clrFadeColor, 
                                 const D3DXCOLOR& clrBounceColor, const D3DXCOLOR& clrBounceFadeColor,
                                 FLOAT fEmitVel, XMVECTOR vPosition, 
                                 IDirect3DTexture9* m_pSegEdgeUntiledTexture, FLOAT scale, PARTICLE_BOUNCING_ALGORITHM mode )
{
    static FLOAT fTime = 0.0f;
    fTime += fSecsPerFrame;

    // For performance reasons, vertex buffers are multi-buffered. Each time
    // the vertex buffer contents are updated, use a new vertex buffer.
    if( ++m_dwCurrentBuffer >= NUM_PARTICLE_BUFFERS )
        m_dwCurrentBuffer = 0;
    m_pPointSpritesVB = m_pPointSpritesVBs[m_dwCurrentBuffer];
    m_pLightsVB = m_pLightsVBs[m_dwCurrentBuffer];

    // Lock vertex buffers
    PARTICLE_VERTEX* pPointSpriteVertices;
    DIFFUSETEX_VERTEX* pLightVertices;
    m_pPointSpritesVB->Lock( 0, 0, ( VOID** )&pPointSpriteVertices, NULL );
    m_pLightsVB->Lock( 0, 0, ( VOID** )&pLightVertices, NULL );
    m_dwNumParticlesToRender = 0;
    m_dwNumLightsToRender = 0;

    D3DLOCKED_RECT LockedRect;
    m_pSegEdgeUntiledTexture->LockRect( 0, &LockedRect, NULL, 0 );

    // Update particles
    for( DWORD i = 0; i < m_dwNumParticles; i++ )
    {
        PARTICLE* pParticle = &m_pParticles[i];

        // Calculate new position
        FLOAT t = fTime - pParticle->m_fTime0;

        if( pParticle->m_bSpark )
        {
            pParticle->m_vPos = pParticle->m_vVel0 * t + pParticle->m_vPos0;
            pParticle->m_vPos.y -= ( 0.5f * 5.0f ) * ( t * t );
            pParticle->m_vVel.y = pParticle->m_vVel0.y - 5.0f * t;
            pParticle->m_fFade -= fSecsPerFrame * 2.25f;
        }
        else
        {
            BYTE maxr = 0;      // Boundary intensity in the edge map
            XMVECTOR n = {0};   // Boundary normal in the edge map

            // Save last position and velocity of the particle
            XMVECTOR vPosOld = pParticle->m_vPos;
            XMVECTOR vVelOld = pParticle->m_vVel;

            // Update the particle
            pParticle->m_vPos = pParticle->m_vVel0 * t + pParticle->m_vPos0;
            pParticle->m_vPos.y -= ( 0.5f * 9.8f ) * ( t * t );
            pParticle->m_vVel.y = pParticle->m_vVel0.y - 9.8f * t;
            pParticle->m_fFade -= fSecsPerFrame * 0.25f;
            
            if ( mode == PARTICLE_BOUNCING_ALGORITHM_ACCURATE )
            {
                // Test whether the particle should bounce off the silhouette of player body
                // This is a more expensive but more accurate algorithm
                // The algorithm searches along the line segment from last position to the current position of the particle
                // and test whether the line segment interests the boundary of the player in the edge map
                // If yes, the position of the intersection point is set as the particle's new initial position
                // and the normal of the silhouette in the edge map at that point is used to calculate the velocity after bouncing

                // Map particle position to our edge map - last position
                FLOAT rx0 = (vPosOld.x - (-FIGUREBOARD_SIZE / 2) * scale) / scale;
                FLOAT ry0 = (FIGUREBOARD_SIZE * scale - vPosOld.y) / scale;
                INT ix0 = INT(rx0 * DEPTH_WIDTH);
                INT iy0 = INT(ry0 * DEPTH_HEIGHT);

                // Map particle position to our edge map - current position
                FLOAT rx1 = (pParticle->m_vPos.x - (-FIGUREBOARD_SIZE / 2) * scale) / scale;
                FLOAT ry1 = (FIGUREBOARD_SIZE * scale - pParticle->m_vPos.y) / scale;
                INT ix1 = INT(rx1 * DEPTH_WIDTH);
                INT iy1 = INT(ry1 * DEPTH_HEIGHT);

                // Determine how many steps to test along the line segment from last position to current position
                INT steps = __max( 1, __max( abs( ix0 - ix1 ), abs( iy0 - iy1 ) ) );
                FLOAT dx = (rx1 - rx0) / steps;
                FLOAT dy = (ry1 - ry0) / steps;            

                // Test along the line segment                
                for ( INT j = 0; j < steps; ++j )
                {
                    FLOAT rx = rx0 + dx * j;
                    FLOAT ry = ry0 + dy * j;

                    if ( rx > 0 && rx < 1 && ry > 0 && ry < 1 )
                    {
                        INT ix = INT(rx * DEPTH_WIDTH);
                        INT iy = INT(ry * DEPTH_HEIGHT);

                        DWORD* line = (DWORD*)( ( (BYTE*)LockedRect.pBits ) + LockedRect.Pitch * iy );
                        DWORD c = line[ix];
                        BYTE r = D3DCOLOR_GETRED( c );
                        if ( r > 100 )
                        {
                            if ( r > maxr )
                            {
                                maxr = r;
                                n = XMVectorSet( D3DCOLOR_GETGREEN(c) / 255.0f * 2.0f - 1.0f, D3DCOLOR_GETBLUE(c) / 255.0f * 2.0f - 1.0f, 0, 0 );
                            }
                        }  
                    }                
                }
            } else
            if ( mode == PARTICLE_BOUNCING_ALGORITHM_FAST )
            {
                // This naive and fast algorithm only test whether the current position of the particle interests with the boundary of the player in the edge map
                // so some particles traveling fast may get through the player silhouette and go inside the player body
                
                // Map particle position to our edge map
                FLOAT rx = (pParticle->m_vPos.x - (-FIGUREBOARD_SIZE / 2) * scale) / scale;
                FLOAT ry = (FIGUREBOARD_SIZE * scale - pParticle->m_vPos.y) / scale;
                
                if ( rx > 0 && rx < 1 && ry > 0 && ry < 1 )
                {
                    INT ix = INT(rx * DEPTH_WIDTH);
                    INT iy = INT(ry * DEPTH_HEIGHT);
                    DWORD* line = (DWORD*)( ( (BYTE*)LockedRect.pBits ) + LockedRect.Pitch * iy );
                    DWORD c = line[ix];
                    maxr = D3DCOLOR_GETRED( c );
                    n = XMVectorSet( D3DCOLOR_GETGREEN(c) / 255.0f * 2.0f - 1.0f, D3DCOLOR_GETBLUE(c) / 255.0f * 2.0f - 1.0f, 0, 0 );                    
                }                   
            }
                        
            if ( maxr > 100 )
            {
                // There is a particle to player silhouette collision,
                // so bounce the particle using the normal of silhouette at the collision point
                
                pParticle->m_vPos0 = vPosOld;
                pParticle->m_vVel0 = XMVector3Reflect( vVelOld, n );
                pParticle->m_vVel0 *= 0.3;
                pParticle->m_vPos = pParticle->m_vPos0;
                pParticle->m_vVel = pParticle->m_vVel0;
                pParticle->m_clrDiffuse = clrBounceColor;
                pParticle->m_clrFade = clrBounceFadeColor;                
                pParticle->m_fFade = 1.0f;
                pParticle->m_fTime0 = fTime;
            }              
        }

        if( pParticle->m_fFade < 0.0f )
            pParticle->m_fFade = 0.0f;

        // Kill old particles
        if( pParticle->m_vPos.y < m_fRadius || pParticle->m_bSpark && pParticle->m_fFade <= 0.0f )
        {
            // Emit sparks
            if( !pParticle->m_bSpark )
            {
                for( int j = 0; j < 4; j++ )
                {
                    FLOAT fRand1 = ( ( FLOAT )rand() / ( FLOAT )RAND_MAX ) * XM_PI * 2.00f;
                    FLOAT fRand2 = ( ( FLOAT )rand() / ( FLOAT )RAND_MAX ) * XM_PI * 0.25f;

                    PARTICLE* pSpark = &m_pParticles[m_dwNumParticles++];
                    pSpark->m_bSpark = TRUE;
                    pSpark->m_vPos0 = pParticle->m_vPos;
                    pSpark->m_vPos0.y = m_fRadius;
                    pSpark->m_vVel0.x = pParticle->m_vVel.x * 0.25f + cosf( fRand1 ) * sinf( fRand2 );
                    pSpark->m_vVel0.z = pParticle->m_vVel.z * 0.25f + sinf( fRand1 ) * sinf( fRand2 );
                    pSpark->m_vVel0.y = cosf( fRand2 );
                    pSpark->m_vVel0.y *= ( ( FLOAT )rand() / ( FLOAT )RAND_MAX ) * 1.5f;
                    pSpark->m_vPos = pSpark->m_vPos0;
                    pSpark->m_vVel = pSpark->m_vVel0;
                    D3DXColorLerp( &pSpark->m_clrDiffuse, &pParticle->m_clrFade,
                                   &pParticle->m_clrDiffuse, pParticle->m_fFade );
                    pSpark->m_clrFade = D3DXCOLOR( 0.0f, 0.0f, 0.0f, 1.0f );
                    pSpark->m_fFade = 1.0f;
                    pSpark->m_fTime0 = fTime;
                }
            }

            // Kill this particle (we can do this fast, by simply moving the
            // last particle to take the current particle's place).
            m_pParticles[i--] = m_pParticles[--m_dwNumParticles];
        }
        else
        {
            // Build vertex buffers for the particles
            FLOAT fSpeed = XMVector3LengthSq( pParticle->m_vVel ).x;
            UINT dwSteps = 1;
            if( fSpeed < 1.0f )        dwSteps = 2;
            else if( fSpeed < 4.00f ) dwSteps = 3;
            else if( fSpeed < 9.00f ) dwSteps = 4;
            else if( fSpeed < 12.25f ) dwSteps = 5;
            else if( fSpeed < 16.00f ) dwSteps = 6;
            else if( fSpeed < 20.25f ) dwSteps = 7;
            else
                dwSteps = 8;

            XMVECTOR vPos = pParticle->m_vPos;
            XMVECTOR vVel = pParticle->m_vVel * -0.04f / ( FLOAT )dwSteps;

            D3DXCOLOR clrDiffuse;
            D3DXColorLerp( &clrDiffuse, &pParticle->m_clrFade, &pParticle->m_clrDiffuse,
                           pParticle->m_fFade );
            DWORD dwDiffuse = ( DWORD )clrDiffuse;

            // Compute vertices for ground lighting effects
            if( vPos.y < 1.0f )
            {
                FLOAT fY = vPos.y;
                if( fY < 0.0f )
                    fY = 0.0f;

                FLOAT fSize = fY * 0.25f + m_fRadius;
                DWORD dwLightDiffuse = ( DWORD )( clrDiffuse * ( ( 1.0f - fY ) * 0.5f ) );

                pLightVertices[0].v.x = pLightVertices[1].v.x = vPos.x + fSize;
                pLightVertices[0].v.z = pLightVertices[3].v.z = vPos.z + fSize;
                pLightVertices[2].v.x = pLightVertices[3].v.x = vPos.x - fSize;
                pLightVertices[1].v.z = pLightVertices[2].v.z = vPos.z - fSize;
                pLightVertices[0].color = pLightVertices[1].color = dwLightDiffuse;
                pLightVertices[2].color = pLightVertices[3].color = dwLightDiffuse;

                // Advance to next light
                pLightVertices += 4;
                m_dwNumLightsToRender++;
            }

            // Use multiple pointsprites per particle to get a motion-blur effect
            for( DWORD j = 0; j < dwSteps; j++ )
            {
                if( vPos.y >= 0.0f )
                {
                    // Stop if the VB gets full.
                    if( m_dwNumParticlesToRender >= m_dwMaxParticles )
                        break;

                    XMStoreFloat3( &pPointSpriteVertices->v, vPos );
                    pPointSpriteVertices->color = dwDiffuse;
                    pPointSpriteVertices++;
                    m_dwNumParticlesToRender++;
                }
                vPos += vVel;
            }
        }
    }

    m_pSegEdgeUntiledTexture->UnlockRect( 0 );

    // Unlock the vertex buffers
    m_pPointSpritesVB->Unlock();
    m_pLightsVB->Unlock();

    // Emit new particles
    while( dwNumParticlesToEmit > 0 && m_dwNumParticles < m_dwMaxParticles / 4 )
    {
        FLOAT fRand1 = ( ( FLOAT )rand() / ( FLOAT )RAND_MAX ) * XM_PI * 2.00f;
        FLOAT fRand2 = ( ( FLOAT )rand() / ( FLOAT )RAND_MAX ) * XM_PI * 0.25f;

        PARTICLE* pParticle = &m_pParticles[m_dwNumParticles];
        pParticle->m_bSpark = FALSE;
        pParticle->m_vPos0 = vPosition;
        pParticle->m_vPos0.y += m_fRadius;
        pParticle->m_vVel0.x = cosf( fRand1 ) * sinf( fRand2 ) * 2.5f;
        pParticle->m_vVel0.z = 0;
        pParticle->m_vVel0.y = cosf( fRand2 );
        pParticle->m_vVel0.y *= ( ( FLOAT )rand() / ( FLOAT )RAND_MAX ) * fEmitVel;
        pParticle->m_vPos = pParticle->m_vPos0;
        pParticle->m_vVel = pParticle->m_vVel0;
        pParticle->m_clrDiffuse = clrEmitColor;
        pParticle->m_clrFade = clrFadeColor;
        pParticle->m_fFade = 1.0f;
        pParticle->m_fTime0 = fTime;

        dwNumParticlesToEmit--;
        m_dwNumParticles++;
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: RenderParticles()
// Desc: Renders the particle system using pointsprites.
//-----------------------------------------------------------------------------
HRESULT CParticleSystem::RenderParticles()
{
    if( 0 == m_dwNumParticlesToRender )
        return S_OK;

    // Render particles
    ATG::g_pd3dDevice->SetStreamSource( 0, m_pPointSpritesVB, 0, sizeof( PARTICLE_VERTEX ) );
    ATG::g_pd3dDevice->DrawPrimitive( D3DPT_POINTLIST, 0, m_dwNumParticlesToRender );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: RenderLights()
// Desc: Renders ground lighting effects for the particle system.
//-----------------------------------------------------------------------------
HRESULT CParticleSystem::RenderLights()
{
    if( 0 == m_dwNumLightsToRender )
        return S_OK;

    // Render lights
    ATG::g_pd3dDevice->SetStreamSource( 0, m_pLightsVB, 0, sizeof( DIFFUSETEX_VERTEX ) );
    ATG::g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, m_dwNumLightsToRender );

    return S_OK;
}

