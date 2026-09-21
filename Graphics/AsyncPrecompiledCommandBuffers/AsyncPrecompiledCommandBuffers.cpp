//--------------------------------------------------------------------------------------
// AsyncPrecompiledCommandBuffers.cpp
//
// This sample shows how to use the asynchronous precompiled command buffer APIs to
// record a GPU command buffer on a worker thread without stalling the main rendering
// thread. Instead, the GPU will wait on the command buffer to finish construction
// before executing it.  This has the advantage of freeing up the main rendering thread
// to continue drawing or being the next frame without having to synchronize with the
// worker threads.
//
// There are three technique demonstrated in this sample:
//
// Single Threaded - implemented in RenderSceneSingleThreaded()
//     Main Thread:
//         Update
//         Render the Scene
//
// Synchronous Command Buffers - implemented in RenderSceneSynchronous() and RenderWorker
//     Main Thread:
//         Update
//         Submit Work Items
//         Wait for Worker Threads
//         RunCommandBuffer()
//     Worker Thread:
//         Build Command Buffer
//         Signal Event
//
// Async Command Buffers - implemented in RenderSceneAsync() and RenderWorker
//     Main Thread:
//         Update
//         Submit Work Items
//         Reset the Async Command Buffer Call
//         InsertAsyncCommandBufferCall() - No waiting necessary
//     Worker Thread:
//         Build Command Buffer
//         FixupAndSignal() the Async Command Buffer Call
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <fxl.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgApp.h>
#include <AtgResource.h>
#include "AtgWorkerThread.h"


//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
const DWORD g_dwMaxGameObjectCount = 300;
const DWORD g_dwCrowdSize = 300;
const DWORD g_dwMaxWorkItemObjects = max( g_dwMaxGameObjectCount, g_dwCrowdSize );
const DWORD g_dwBufferFrames = 2;
const DWORD g_dwShadowMapSize = 1024;
const DWORD g_dwPointLightCount = 2;

enum COMMAND_BUFFER_MODE
{
    USE_ASYNC_CB,
    USE_SYNC_CB,
    USE_SINGLE_THREAD
};


//--------------------------------------------------------------------------------------
// Utility Functions
//--------------------------------------------------------------------------------------
inline FLOAT frand( FLOAT fMin, FLOAT fMax )
{
    return fMin + ( fMax - fMin ) * ( ( FLOAT )rand() / ( FLOAT )RAND_MAX );
}


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Toggle\ncommand buffers" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_1, L"Toggle\nsimulation" },
    { ATG::HELP_LEFT_SHOULDER,ATG::HELP_PLACEMENT_1, L"Less objects" },
    { ATG::HELP_RIGHT_SHOULDER,ATG::HELP_PLACEMENT_1, L"More objects" },
    { ATG::HELP_LEFT_STICK,   ATG::HELP_PLACEMENT_1, L"Rotate camera" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
    { ATG::HELP_MISC_CALLOUT, ATG::HELP_PLACEMENT_2, L"Triggers move camera in/out" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Vertex structures and buffers
//--------------------------------------------------------------------------------------
struct Vertex_PTN
{
    XMFLOAT3 vPos;
    XMFLOAT2 vTex;
    XMFLOAT3 vNormal;
};


const D3DVERTEXELEMENT9 g_Vertex_PTN_Elements[] =
{
    { 0,     0, D3DDECLTYPE_FLOAT3,     0,  D3DDECLUSAGE_POSITION,  0 },
    { 0,    12, D3DDECLTYPE_FLOAT2,     0,  D3DDECLUSAGE_TEXCOORD,  0 },
    { 0,    20, D3DDECLTYPE_FLOAT3,     0,  D3DDECLUSAGE_NORMAL,    0 },
    D3DDECL_END()
};


const Vertex_PTN g_MeshDataGround[] =
{
    { XMFLOAT3( -1, 0, 1 ), XMFLOAT2( 0, 0 ), XMFLOAT3( 0, 1, 0 ) },
    { XMFLOAT3( 1, 0, 1 ), XMFLOAT2( 1, 0 ), XMFLOAT3( 0, 1, 0 ) },
    { XMFLOAT3( 1, 0, -1 ), XMFLOAT2( 1, 1 ), XMFLOAT3( 0, 1, 0 ) },
    { XMFLOAT3( -1, 0, -1 ), XMFLOAT2( 0, 1 ), XMFLOAT3( 0, 1, 0 ) }
};


const Vertex_PTN g_MeshDataCube[] =
{
    // top face
    { XMFLOAT3( -1, 2, 1 ), XMFLOAT2( 0, 0 ), XMFLOAT3( 0, 1, 0 ) },
    { XMFLOAT3( 1, 2, 1 ), XMFLOAT2( 1, 0 ), XMFLOAT3( 0, 1, 0 ) },
    { XMFLOAT3( 1, 2, -1 ), XMFLOAT2( 1, 1 ), XMFLOAT3( 0, 1, 0 ) },
    { XMFLOAT3( -1, 2, -1 ), XMFLOAT2( 0, 1 ), XMFLOAT3( 0, 1, 0 ) },

    // front face
    { XMFLOAT3( -1, 2, -1 ), XMFLOAT2( 0, 0 ), XMFLOAT3( 0, 0, -1 ) },
    { XMFLOAT3( 1, 2, -1 ), XMFLOAT2( 1, 0 ), XMFLOAT3( 0, 0, -1 ) },
    { XMFLOAT3( 1, 0, -1 ), XMFLOAT2( 1, 1 ), XMFLOAT3( 0, 0, -1 ) },
    { XMFLOAT3( -1, 0, -1 ), XMFLOAT2( 0, 1 ), XMFLOAT3( 0, 0, -1 ) },

    // back face
    { XMFLOAT3( 1, 2, 1 ), XMFLOAT2( 0, 0 ), XMFLOAT3( 0, 0, 1 ) },
    { XMFLOAT3( -1, 2, 1 ), XMFLOAT2( 1, 0 ), XMFLOAT3( 0, 0, 1 ) },
    { XMFLOAT3( -1, 0, 1 ), XMFLOAT2( 1, 1 ), XMFLOAT3( 0, 0, 1 ) },
    { XMFLOAT3( 1, 0, 1 ), XMFLOAT2( 0, 1 ), XMFLOAT3( 0, 0, 1 ) },

    // left face
    { XMFLOAT3( -1, 2, 1 ), XMFLOAT2( 0, 0 ), XMFLOAT3( -1, 0, 0 ) },
    { XMFLOAT3( -1, 2, -1 ), XMFLOAT2( 1, 0 ), XMFLOAT3( -1, 0, 0 ) },
    { XMFLOAT3( -1, 0, -1 ), XMFLOAT2( 1, 1 ), XMFLOAT3( -1, 0, 0 ) },
    { XMFLOAT3( -1, 0, 1 ), XMFLOAT2( 0, 1 ), XMFLOAT3( -1, 0, 0 ) },

    // right face
    { XMFLOAT3( 1, 2, -1 ), XMFLOAT2( 0, 0 ), XMFLOAT3( 1, 0, 0 ) },
    { XMFLOAT3( 1, 2, 1 ), XMFLOAT2( 1, 0 ), XMFLOAT3( 1, 0, 0 ) },
    { XMFLOAT3( 1, 0, 1 ), XMFLOAT2( 1, 1 ), XMFLOAT3( 1, 0, 0 ) },
    { XMFLOAT3( 1, 0, -1 ), XMFLOAT2( 0, 1 ), XMFLOAT3( 1, 0, 0 ) },

    // bottom face
    { XMFLOAT3( -1, 0, -1 ), XMFLOAT2( 0, 0 ), XMFLOAT3( 0, -1, 0 ) },
    { XMFLOAT3( 1, 0, -1 ), XMFLOAT2( 1, 0 ), XMFLOAT3( 0, -1, 0 ) },
    { XMFLOAT3( 1, 0, 1 ), XMFLOAT2( 1, 1 ), XMFLOAT3( 0, -1, 0 ) },
    { XMFLOAT3( -1, 0, 1 ), XMFLOAT2( 0, 1 ), XMFLOAT3( 0, -1, 0 ) }
};


//--------------------------------------------------------------------------------------
// Game Object
//--------------------------------------------------------------------------------------
struct GameObject
{
    XMFLOAT3 Position;
    XMFLOAT3 Direction;
    FLOAT fSize;
    DWORD dwTextureIndex;
    XMFLOAT4X4 WorldMatrix;
    XMFLOAT3 Velocity;

    inline VOID UpdateWorldTransform()
    {
        static const XMVECTOR vUp = XMVectorSet( 0, 1, 0, 0 );
        static const XMVECTOR vSelectW = XMVectorSelectControl( 0, 0, 0, 1 );
        static const XMVECTOR vOne = XMVectorReplicate( 1.0f );

        XMVECTOR vPos = XMLoadFloat3( &Position );
        XMVECTOR vDirection = XMLoadFloat3( &Direction );
        XMVECTOR vRight = XMVector3Cross( vUp, vDirection );
        XMMATRIX matWorld;
        matWorld.r[0] = XMVectorSelect( vRight, XMVectorZero(), vSelectW );
        matWorld.r[1] = XMVectorSelect( vUp, XMVectorZero(), vSelectW );
        matWorld.r[2] = XMVectorSelect( vDirection, XMVectorZero(), vSelectW );
        matWorld.r[3] = XMVectorSelect( vPos, vOne, vSelectW );
        XMStoreFloat4x4( &WorldMatrix, matWorld );
    }
};


//--------------------------------------------------------------------------------------
// WorkerData
//--------------------------------------------------------------------------------------
class Sample;

struct WorkerData
{
    // Objects to render
    GameObject m_Objects[ g_dwMaxWorkItemObjects ];
    DWORD m_dwNumObjects;

    // View Projection Matrix and Render Type
    XMMATRIX m_matVP;
    BOOL m_bRenderShadows;

    // Render Target and Depth Stencil Surface
    D3DSurface* m_pRenderTarget;
    D3DSurface* m_pDepthStencil;

    // Command Buffer to render to
    D3DCommandBuffer* m_pCommandBuffer;
    D3DAsyncCommandBufferCall* m_pAsyncCommandBufferCall;
    HANDLE m_Event;
};


//--------------------------------------------------------------------------------------
// RenderWorker
//--------------------------------------------------------------------------------------
class RenderWorker : public IWorkerThreadContext
{
public:
    VOID            Initialize( DWORD dwWorkerHWThread, const char* szThreadName, Sample* sample );

    virtual VOID    ThreadStartup( );
    virtual VOID    DoWork( VOID* pData );
    virtual VOID    ThreadShutdown( );

    VOID            SubmitWorkItem( WorkerData* item, COMMAND_BUFFER_MODE mode );
    D3DCommandBuffer* WaitForCommandBuffer( );
    D3DAsyncCommandBufferCall* GetAsyncCommandBufferCall( );

private:
    // Thread
    WorkerThread*   m_pWorkerThread;
    const char*     m_szThreadName;

    // Command buffer and synchronization
    D3DDevice*      m_pCommandBufferDevice;
    DWORD           m_dwBufferNumber; // [0,g_dwBufferFrames)
    HANDLE          m_Event;
    D3DCommandBuffer* m_pCommandBuffer[g_dwBufferFrames];
    D3DAsyncCommandBufferCall* m_pAsyncCommandBufferCall[g_dwBufferFrames];

    // Sample is used for RenderObject()
    Sample*        m_sample;
};


//--------------------------------------------------------------------------------------
// Sample - The async precompiled command buffers sample class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    HRESULT     Initialize();
    HRESULT     Update();
    HRESULT     Render();

    VOID        RenderObject( D3DDevice* pDevice, const GameObject& obj, const XMMATRIX& matVP, BOOL bRenderShadows );

private:
    VOID        InitializeAssets();
    VOID        InitializeGameObjects();
    VOID        BuildCommandBuffers();

    VOID        PrefetchGameObjects();
    VOID        UpdateGameObjects( FLOAT fDeltaTime );

    VOID        RenderSceneSingleThreaded();
    VOID        RenderSceneSynchronous();
    VOID        RenderSceneAsync();

    VOID        RenderEnvironment( D3DDevice* pDevice );

    VOID        SetupLightingConstants( D3DDevice* pDevice, const XMMATRIX& matWorld );

    VOID        RenderUI();

private:
    // Worker Threads
    RenderWorker m_CrowdShadowWorker;
    RenderWorker m_ObjectShadowWorker;
    RenderWorker m_CrowdWorker;
    RenderWorker m_ObjectWorker;

    XMMATRIX m_matProj;
    XMMATRIX m_matVP;
    FLOAT m_fViewDistance;

    XMMATRIX m_matLightProj;
    XMMATRIX m_matLightVP;
    XMMATRIX m_matLightVPT;

    ATG::Font m_Font;
    ATG::Timer m_Timer;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    ATG::PackedResource m_TextureResource;
    D3DTexture* m_pTextureEnvironment;
    D3DTexture* m_pTextureObject[4];

    D3DVertexShader* m_pVertexShaderWVPTransform;
    D3DPixelShader* m_pPixelShaderObject;

    D3DVertexBuffer* m_pObjectVB;
    D3DVertexDeclaration* m_pObjectDecl;
    D3DVertexBuffer* m_pGroundVB;

    D3DSurface* m_pAutoColorTarget;
    D3DSurface* m_pAutoDepthStencil;

    D3DTexture* m_pShadowMapTexture;
    D3DSurface* m_pShadowMapTarget;

    GameObject  m_CrowdObjects[ g_dwCrowdSize ];
    DWORD m_dwCrowdObjectCount;
    GameObject  m_GameObjects[ g_dwMaxGameObjectCount ];
    DWORD m_dwGameObjectCount;
    DWORD m_dwObjectIndexTarget;
    DWORD m_dwObjectIndexAlive;

    XMFLOAT4 m_PointLightPosRange[g_dwPointLightCount];
    XMFLOAT4 m_PointLightColor[g_dwPointLightCount];
    XMFLOAT3 m_DirLightDirection;
    XMFLOAT4 m_DirLightColor;

    COMMAND_BUFFER_MODE m_CommandBufferMode;
    BOOL m_bRunSimulation;

    FLOAT m_fCPURenderTime;
    FLOAT m_fCPUWaitTime;
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    static Sample AsyncPCBSample;

    // Set up the structure used to create the D3DDevice.
    D3DPRESENT_PARAMETERS& d3dpp = AsyncPCBSample.m_d3dpp;
    ZeroMemory( &d3dpp, sizeof( D3DPRESENT_PARAMETERS ) );
    d3dpp.BackBufferWidth = 1280;
    d3dpp.BackBufferHeight = 720;
    d3dpp.BackBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dpp.MultiSampleQuality = 0;
    d3dpp.BackBufferCount = 1;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.DisableAutoBackBuffer = FALSE;
    d3dpp.DisableAutoFrontBuffer = FALSE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    AsyncPCBSample.m_dwDeviceCreationFlags = D3DCREATE_BUFFER_2_FRAMES;

    AsyncPCBSample.Run();
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the sample, and calls into other initialization functions.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;
    m_CommandBufferMode = USE_ASYNC_CB;
    m_bRunSimulation = TRUE;
    m_pTextureEnvironment = NULL;
    m_pObjectVB = NULL;
    m_pObjectDecl = NULL;
    m_pGroundVB = NULL;
    m_PointLightColor[0] = XMFLOAT4( 0, 1, 0, 0 );
    m_PointLightColor[1] = XMFLOAT4( 1, 0, 0, 0 );
    m_PointLightPosRange[0] = XMFLOAT4( 0, 2, 0, 0.1f );
    m_PointLightPosRange[1] = XMFLOAT4( 10, 2, 0, 0.1f );
    m_DirLightColor = XMFLOAT4( 0.8f, 0.8f, 0.8f, 0.0f );
    m_DirLightDirection = XMFLOAT3( 0.7071f, -0.7071f, 0 );

    // Capture rendertarget 0 and depth/stencil.
    m_pd3dDevice->GetRenderTarget( 0, &m_pAutoColorTarget );
    m_pd3dDevice->GetDepthStencilSurface( &m_pAutoDepthStencil );

    // Build the projection and world matrices.
    m_matProj = XMMatrixPerspectiveFovLH( XM_PIDIV4, ( 16.0f / 9.0f ), 1.0f, 200.0f );
    m_fViewDistance = 70.0f;

    // Build the shadow matrices.
    // Light orientation (looks at zero)
    FLOAT fRadius = 60;
    FLOAT fNear = 0.1f;
    FLOAT fFar = fNear + fRadius * 2.0f;

    XMVECTOR vFrom = -XMLoadFloat3( &m_DirLightDirection ) * ( fRadius + fNear );
    XMVECTOR vTo = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    XMMATRIX matLightView = XMMatrixLookAtLH( vFrom, vTo, vUp );

    // Include the entire world in the orthographic projection.
    FLOAT fWidth = fRadius * 2.0f;
    FLOAT fHeight = fRadius * 2.0f;

    m_matLightProj = XMMatrixOrthographicLH( fWidth, fHeight, fNear, fFar );

    // Setup the texture matrix.
    XMMATRIX matTexture( 0.5f,  0.0f,  0.0f,  0.0f,
                         0.0f, -0.5f,  0.0f,  0.0f,
                         0.0f,  0.0f,  1.0f,  0.0f,
                         0.5f,  0.5f,  0.0f,  1.0f );

    m_matLightVP = matLightView * m_matLightProj;

    m_matLightVPT = matLightView * m_matLightProj * matTexture;

    // Create font.
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        ATG::FatalError( "Could not load font." );

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create help screen.
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        ATG::FatalError( "Could not load help resources." );

    // Load assets.
    InitializeAssets();

    // Initialize game and crowd objects.
    InitializeGameObjects();

    // Create the worker threads
    // They are each assigned to hardware threads 2, 3, 4, and 5 respectively
    m_ObjectShadowWorker.Initialize( 2, "Object Shadow Worker", this );
    m_CrowdShadowWorker.Initialize( 3, "Crowd Shadow Worker", this );
    m_ObjectWorker.Initialize( 4, "Object Worker", this );
    m_CrowdWorker.Initialize( 5, "Crowd Worker", this );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitializeAssets()
// Desc: Loads textures and shaders.  Creates mesh geometry and vertex declarations.
//--------------------------------------------------------------------------------------
VOID Sample::InitializeAssets()
{
    // Load textures.
    if( FAILED( m_TextureResource.Create( "game:\\Media\\Resource.xpr" ) ) )
        ATG::FatalError( "Could not load textures." );
    m_pTextureEnvironment = m_TextureResource.GetTexture( "TextureEnv" );
    m_pTextureObject[0] = m_TextureResource.GetTexture( "TextureObj0" );
    m_pTextureObject[1] = m_TextureResource.GetTexture( "TextureObj1" );
    m_pTextureObject[2] = m_TextureResource.GetTexture( "TextureObj2" );
    m_pTextureObject[3] = m_TextureResource.GetTexture( "TextureObj3" );

    // Load shaders.
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ObjectVS.xvu", &m_pVertexShaderWVPTransform ) ) )
        ATG::FatalError( "Could not load shader." );
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ObjectPS.xpu", &m_pPixelShaderObject ) ) )
        ATG::FatalError( "Could not load shader." );

    // Create environment geometry.
    DWORD dwVertexSize = ( DWORD )sizeof( Vertex_PTN );
    DWORD dwVBSize = dwVertexSize * 4;
    HRESULT hr = m_pd3dDevice->CreateVertexBuffer( dwVBSize, 0, 0, D3DPOOL_DEFAULT, &m_pGroundVB, NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not create environment VB." );

    Vertex_PTN* pVertexData = NULL;
    m_pGroundVB->Lock( 0, 0, ( VOID** )&pVertexData, 0 );
    memcpy( pVertexData, g_MeshDataGround, dwVBSize );
    m_pGroundVB->Unlock();

    // Create object geometry.
    dwVertexSize = ( DWORD )sizeof( Vertex_PTN );
    dwVBSize = dwVertexSize * 4 * 6;
    hr = m_pd3dDevice->CreateVertexBuffer( dwVBSize, 0, 0, D3DPOOL_DEFAULT, &m_pObjectVB, NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not create object VB." );

    pVertexData = NULL;
    m_pObjectVB->Lock( 0, 0, ( VOID** )&pVertexData, 0 );
    memcpy( pVertexData, g_MeshDataCube, dwVBSize );
    m_pObjectVB->Unlock();

    hr = m_pd3dDevice->CreateVertexDeclaration( g_Vertex_PTN_Elements, &m_pObjectDecl );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not create object vertex decl." );

    // Bind the vertex shaders for better command buffer efficiency.
    dwVertexSize = ( DWORD )sizeof( Vertex_PTN );
    m_pVertexShaderWVPTransform->Bind( 0, m_pObjectDecl, &dwVertexSize, NULL );

    // Create the shadow map texture and surface
    D3DSURFACE_PARAMETERS SurfaceParameters;
    memset( &SurfaceParameters, 0, sizeof( D3DSURFACE_PARAMETERS ) );
    SurfaceParameters.Base = 0;
    SurfaceParameters.HierarchicalZBase = 0;
    hr = m_pd3dDevice->CreateDepthStencilSurface( g_dwShadowMapSize, g_dwShadowMapSize, D3DFMT_D24S8, D3DMULTISAMPLE_NONE,
                                                  0, 0, &m_pShadowMapTarget, &SurfaceParameters );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not create shadow map surface." );

    hr = m_pd3dDevice->CreateTexture( g_dwShadowMapSize, g_dwShadowMapSize, 1, 0, D3DFMT_D24S8, 0,
                                      &m_pShadowMapTexture, NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not create shadow map texture." );
}


//--------------------------------------------------------------------------------------
// Name: InitializeGameObjects()
// Desc: Sets up the initial positions of the game objects and crowd objects.
//--------------------------------------------------------------------------------------
VOID Sample::InitializeGameObjects()
{
    // Initialize game objects.  They are randomly placed within an XZ square centered
    // about the origin.
    for( DWORD i = 0; i < g_dwMaxGameObjectCount; ++i )
    {
        m_GameObjects[i].Position = XMFLOAT3( frand( -20, 20 ), 0, frand( -20, 20 ) );
        FLOAT fAngle = frand( 0, XM_2PI );
        m_GameObjects[i].Direction = XMFLOAT3( sinf( fAngle ), 0, cosf( fAngle ) );
        m_GameObjects[i].fSize = frand( 0.25f, 1.0f );
        m_GameObjects[i].dwTextureIndex = rand() % ARRAYSIZE( m_pTextureObject );
        m_GameObjects[i].Velocity = XMFLOAT3( 0, 0, 0 );
        m_GameObjects[i].UpdateWorldTransform();
    }
    m_dwGameObjectCount = g_dwMaxGameObjectCount;
    m_dwObjectIndexAlive = 0;
    m_dwObjectIndexTarget = 0;

    // Initialize crowd objects.
    // They are spaced into rings, with a little bit of staggering between the rings
    // for a better appearance.
    const DWORD dwRingSize = 70;
    const DWORD dwRingCount = g_dwCrowdSize / dwRingSize;
    const FLOAT fRingSpacing = 5.0f;
    const FLOAT fRingBaseRadius = 35.0f;
    m_dwCrowdObjectCount = 0;
    for( DWORD dwRing = 0; dwRing < dwRingCount; ++dwRing )
    {
        const FLOAT fRadius = fRingBaseRadius + ( FLOAT )dwRing * fRingSpacing;
        // Ring offset adds the left-right staggering to each ring.
        const FLOAT fRingOffset = ( FLOAT )dwRing * 0.5f;
        // Objects in the outer rings are slightly larger on average than objects in the 
        // inner rings.
        const FLOAT fRingScale = 1.0f + ( FLOAT )dwRing * 0.1f;
        for( DWORD i = 0; i < dwRingSize; ++i )
        {
            FLOAT fTheta = XM_2PI * ( ( ( FLOAT )i + fRingOffset ) / ( FLOAT )dwRingSize );
            assert( m_dwCrowdObjectCount < g_dwCrowdSize );
            GameObject& obj = m_CrowdObjects[ m_dwCrowdObjectCount++ ];
            obj.Position = XMFLOAT3( fRadius * sinf( fTheta ), 0, fRadius * cosf( fTheta ) );
            XMVECTOR vPos = XMLoadFloat3( &obj.Position );
            XMVECTOR vDir = XMVector3Normalize( -vPos );
            XMStoreFloat3( &obj.Direction, vDir );
            obj.dwTextureIndex = rand() % ARRAYSIZE( m_pTextureObject );
            obj.fSize = frand( 1.0f, 2.0f ) * fRingScale;
            obj.UpdateWorldTransform();
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Updates the sample - camera movement, toggles usage of command buffers, and
//       optionally updates the simulation.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    PIXBeginNamedEvent( 0, "Update" );

    // Update FPS in the timer class.
    m_Timer.MarkFrame();

    // Get the current time.
    FLOAT fDeltaTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get input from controllers (check for exit button sequence).
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_bRunSimulation = !m_bRunSimulation;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        if( m_CommandBufferMode == USE_ASYNC_CB ) m_CommandBufferMode = USE_SYNC_CB;
        else if( m_CommandBufferMode == USE_SYNC_CB ) m_CommandBufferMode = USE_SINGLE_THREAD;
        else m_CommandBufferMode = USE_ASYNC_CB;
    }

    if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER ) && m_dwGameObjectCount > 0 )
        m_dwGameObjectCount -= 10;
    else if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER ) &&
             m_dwGameObjectCount < g_dwMaxGameObjectCount )
        m_dwGameObjectCount += 10;

    // Compute camera rotation.
    static FLOAT s_fCameraRotateY = 0.0f;
    static FLOAT s_fCameraRotateX = 0.5f;
    s_fCameraRotateY += pGamepad->fX1 * fDeltaTime * XM_PIDIV2;
    s_fCameraRotateX += pGamepad->fY1 * fDeltaTime * XM_PIDIV2;
    s_fCameraRotateX = min( max( s_fCameraRotateX, 0 ), XM_PIDIV2 );
    XMVECTOR qRotateY = XMQuaternionRotationAxis( XMVectorSet( 0, 1, 0, 0 ), s_fCameraRotateY );
    XMVECTOR qRotateX = XMQuaternionRotationAxis( XMVectorSet( 1, 0, 0, 0 ), s_fCameraRotateX );
    XMVECTOR qRotation = XMQuaternionMultiply( qRotateX, qRotateY );

    // Compute camera distance.
    m_fViewDistance += 8.0f * fDeltaTime * ( ( FLOAT )pGamepad->bLeftTrigger / 255.0f );
    m_fViewDistance -= 8.0f * fDeltaTime * ( ( FLOAT )pGamepad->bRightTrigger / 255.0f );
    if( m_fViewDistance < 2.0f )
        m_fViewDistance = 2.0f;

    // Compose view matrix from camera settings.
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -m_fViewDistance, 0.0f );
    vEyePt = XMVector3Rotate( vEyePt, qRotation );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 2.0f, 0.0f, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    vUpVec = XMVector3Rotate( vUpVec, qRotation );
    vEyePt += vLookatPt;
    XMMATRIX matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );

    // Compute view * projection matrix
    m_matVP = matView * m_matProj;

    // Update game objects.
    if( m_bRunSimulation && m_dwGameObjectCount > 0 )
    {
        // Clamp the delta time to 0.1 sec max, this helps when debugging the sample.
        UpdateGameObjects( min( fDeltaTime, 0.1f ) );
    }

    PIXEndNamedEvent();
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateGameObjects()
// Desc: Performs some very simple game logic and physics computations to move around
//       the game objects.
//--------------------------------------------------------------------------------------
VOID Sample::UpdateGameObjects( FLOAT fDeltaTime )
{
    assert( m_dwGameObjectCount <= g_dwMaxGameObjectCount );

    // Check if the "alive" object and "target" object are still being rendered; if not,
    // select new objects
    if( m_dwObjectIndexTarget >= m_dwGameObjectCount )
    {
        m_dwObjectIndexTarget = rand() % m_dwGameObjectCount;
    }
    if( m_dwObjectIndexAlive >= m_dwGameObjectCount )
    {
        m_dwObjectIndexAlive = rand() % m_dwGameObjectCount;
    }

    // Build some constant vectors for use in the computations
    // Try not to mix vector/float/int math; mixtures may result in load-hit-store (LHS)
    // penalties on the CPU
    const XMVECTOR vSquareRoot2 = XMVectorReplicate( 1.4142136f );
    const XMVECTOR vOne = XMVectorReplicate( 1.0f );
    const XMVECTOR vDeltaTime = XMVectorReplicate( fDeltaTime );
    const XMVECTOR vInverseDeltaTime = XMVectorReciprocal( vDeltaTime );
    const XMVECTOR vDragCoefficient = XMVectorReplicate( 10.0f );
    const XMVECTOR vForceScale = XMVectorReplicate( 1.0f );
    const XMVECTOR vChaseForceScale = XMVectorReplicate( 30.0f );

    XMVECTOR ForceArray[ g_dwMaxGameObjectCount ];
    ZeroMemory( &ForceArray, sizeof( ForceArray ) );

    // Compute force to push the "alive" object towards the "target" object
    {
        XMVECTOR vPosAlive = XMLoadFloat3( &m_GameObjects[m_dwObjectIndexAlive].Position );
        XMVECTOR vPosTarget = XMLoadFloat3( &m_GameObjects[m_dwObjectIndexTarget].Position );
        XMVECTOR vDirection = vPosTarget - vPosAlive;
        XMVECTOR vDistance = XMVector3LengthEst( vPosAlive - vPosTarget );
        FLOAT fMinRadius = m_GameObjects[ m_dwObjectIndexAlive ].fSize + m_GameObjects[ m_dwObjectIndexTarget ].fSize;
        fMinRadius *= 1.5f;
        if( vDistance.x <= fMinRadius )
        {
            // If the alive object reaches the target object, the target becomes the
            // alive object, and a new target is chosen.
            m_dwObjectIndexAlive = m_dwObjectIndexTarget;
            m_dwObjectIndexTarget = rand() % m_dwGameObjectCount;
        }
        else
        {
            // Alive object is still approaching the target, so accumulate the force.
            ForceArray[ m_dwObjectIndexAlive ] = XMVector3Normalize( vDirection ) * vChaseForceScale;
        }
    }

    // Compute forces to push objects away from each other
    for( DWORD i = 0; i < m_dwGameObjectCount; ++i )
    {
        XMVECTOR vPrimaryPos = XMLoadFloat3( &m_GameObjects[i].Position );
        XMVECTOR vPrimaryRange = XMVectorReplicate( m_GameObjects[i].fSize ) * vSquareRoot2;
        XMVECTOR vPrimaryForce = ForceArray[ i ];

        for( DWORD j = ( i + 1 ); j < m_dwGameObjectCount; ++j )
        {
            XMVECTOR vSecondaryPos = XMLoadFloat3( &m_GameObjects[j].Position );
            XMVECTOR vSecondaryRange = XMVectorReplicate( m_GameObjects[j].fSize ) * vSquareRoot2;
            XMVECTOR vSecondaryForce = ForceArray[ j ];

            XMVECTOR vForceDirection = vSecondaryPos - vPrimaryPos;
            XMVECTOR vDistance = XMVector3LengthEst( vForceDirection );
            XMVECTOR vMinDistance = vPrimaryRange + vSecondaryRange;

            XMVECTOR vControl = __vcmpgtfp( vMinDistance, vDistance );
            XMVECTOR vForceAmount = XMVectorSelect( XMVectorZero(), vInverseDeltaTime, vControl );

            XMVECTOR vForce = vForceDirection * vForceAmount * vForceScale;
            vPrimaryForce -= vForce;
            vSecondaryForce += vForce;

            ForceArray[ j ] = vSecondaryForce;
        }
        ForceArray[ i ] = vPrimaryForce;
    }

    const XMVECTOR vDirectionLerpFactor = XMVectorSaturate( XMVectorReplicate( 5.0f ) * vDeltaTime );

    // Apply forces
    for( DWORD i = 0; i < m_dwGameObjectCount; ++i )
    {
        XMVECTOR vPos = XMLoadFloat3( &m_GameObjects[i].Position );
        XMVECTOR vVelocity = XMLoadFloat3( &m_GameObjects[i].Velocity );
        XMVECTOR vDirection = XMLoadFloat3( &m_GameObjects[i].Direction );
        XMVECTOR vMass = XMVectorReplicate( m_GameObjects[i].fSize );
        XMVECTOR vInverseMass = XMVectorReciprocalEst( vMass );

        // Apply force from collisions
        XMVECTOR vAccel = ForceArray[i] * vInverseMass;
        // Apply drag force (friction approximation)
        vAccel -= vVelocity * vDragCoefficient;

        // Integrate velocity
        vVelocity += ( vAccel * vDeltaTime );
        // Integrate position
        vPos += ( vVelocity * vDeltaTime );

        // Update direction (lerp direction to match velocity vector)
        XMVECTOR vVelocitySize = XMVector3LengthSq( vVelocity );
        if( vVelocitySize.x > 0.1f )
        {
            XMVECTOR vVelocityNorm = XMVector3Normalize( vVelocity );
            vDirection = ( vDirectionLerpFactor * vVelocityNorm ) + ( ( vOne - vDirectionLerpFactor ) * vDirection );
            vDirection = XMVector3NormalizeEst( vDirection );
        }

        // Store new values back into the game object
        XMStoreFloat3( &m_GameObjects[i].Position, vPos );
        XMStoreFloat3( &m_GameObjects[i].Velocity, vVelocity );
        XMStoreFloat3( &m_GameObjects[i].Direction, vDirection );

        // Update the game object world transform
        m_GameObjects[i].UpdateWorldTransform();
    }

    // Update point lights to match positions of alive and target objects
    XMFLOAT3 vAlivePos = m_GameObjects[ m_dwObjectIndexAlive ].Position;
    XMFLOAT3 vTargetPos = m_GameObjects[ m_dwObjectIndexTarget ].Position;
    vAlivePos.y += 3.0f;
    vTargetPos.y += 3.0f;
    memcpy( &m_PointLightPosRange[0], &vAlivePos, sizeof( XMFLOAT3 ) );
    memcpy( &m_PointLightPosRange[1], &vTargetPos, sizeof( XMFLOAT3 ) );
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Main render function for the sample.  Also captures CPU time of scene rendering.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    m_pd3dDevice->BeginScene();

    ATG::Timer RenderTimer;

    // Set some default renderstate.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );

    // Render the scene.
    if( m_CommandBufferMode == USE_ASYNC_CB )
        RenderSceneAsync();
    else if( m_CommandBufferMode == USE_SYNC_CB )
        RenderSceneSynchronous();
    else
        RenderSceneSingleThreaded();

    m_fCPURenderTime = ( FLOAT )RenderTimer.GetElapsedTime();

    // Render UI.
    RenderUI();

    m_pd3dDevice->EndScene();

    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderSceneSingleThreaded()
// Desc: Renders the scene without using precompiled command buffers.
//--------------------------------------------------------------------------------------
VOID Sample::RenderSceneSingleThreaded()
{
    // Clear the Shadow Map
    m_pd3dDevice->SetRenderTarget( 0, NULL );
    m_pd3dDevice->SetDepthStencilSurface( m_pShadowMapTarget );
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0 );

    // Render the Shadow Map
    PIXBeginNamedEvent( 0, "Object Shadow Map Rendering" );
    for( DWORD i = 0; i < m_dwGameObjectCount; ++i )
    {
        RenderObject( m_pd3dDevice, m_GameObjects[i], m_matLightVP, TRUE );
    }
    PIXEndNamedEvent();

    PIXBeginNamedEvent( 0, "Crowd Shadow Map Rendering" );
    for( DWORD i = 0; i < m_dwCrowdObjectCount; ++i )
    {
        RenderObject( m_pd3dDevice, m_CrowdObjects[i], m_matLightVP, TRUE );
    }
    PIXEndNamedEvent();

    // Resolve the Shadow Map to the Texture
    m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, m_pShadowMapTexture, NULL, 0, 0, NULL, 0, 0, NULL );

    // Clear the Render Target and Depth Buffer
    m_pd3dDevice->SetRenderTarget( 0, m_pAutoColorTarget );
    m_pd3dDevice->SetDepthStencilSurface( m_pAutoDepthStencil );
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0 );

    // Render The Objects and Crowd
    PIXBeginNamedEvent( 0, "Object Rendering" );
    for( DWORD i = 0; i < m_dwGameObjectCount; ++i )
    {
        RenderObject( m_pd3dDevice, m_GameObjects[i], m_matVP, FALSE );
    }
    PIXEndNamedEvent();

    PIXBeginNamedEvent( 0, "Crowd Rendering" );
    for( DWORD i = 0; i < m_dwCrowdObjectCount; ++i )
    {
        RenderObject( m_pd3dDevice, m_CrowdObjects[i], m_matVP, FALSE );
    }
    PIXEndNamedEvent();
    
    // Render ground plane
    RenderEnvironment( m_pd3dDevice );
}


//--------------------------------------------------------------------------------------
// Name: BuildCommandBuffers()
// Desc: Submits the work items to the worker threads.  The worker threads will
//       render the sumitted objects into their command buffer and signal the event
//       or signal the async command buffer when complete.
//--------------------------------------------------------------------------------------
VOID Sample::BuildCommandBuffers()
{
    PIXBeginNamedEvent( 0, "Submit Work Items" );

    // Submit the four work items
    // We must provide a copy of the objects because the worker threads may still
    // be rendering the command buffers while the next frame's update begins when
    // using async command buffers.

    WorkerData* pObjectShadows = new WorkerData;
    pObjectShadows->m_dwNumObjects = m_dwGameObjectCount;
    XMemCpy( pObjectShadows->m_Objects, m_GameObjects, m_dwGameObjectCount * sizeof(GameObject) );
    pObjectShadows->m_matVP = m_matLightVP;
    pObjectShadows->m_bRenderShadows = TRUE;
    pObjectShadows->m_pRenderTarget = NULL;
    pObjectShadows->m_pDepthStencil = m_pShadowMapTarget;
    m_ObjectShadowWorker.SubmitWorkItem( pObjectShadows, m_CommandBufferMode );

    WorkerData* pCrowdShadows = new WorkerData;
    pCrowdShadows->m_dwNumObjects = m_dwCrowdObjectCount;
    XMemCpy( pCrowdShadows->m_Objects, m_CrowdObjects, m_dwCrowdObjectCount * sizeof(GameObject) );
    pCrowdShadows->m_matVP = m_matLightVP;
    pCrowdShadows->m_bRenderShadows = TRUE;
    pCrowdShadows->m_pRenderTarget = NULL;
    pCrowdShadows->m_pDepthStencil = m_pShadowMapTarget;
    m_CrowdShadowWorker.SubmitWorkItem( pCrowdShadows, m_CommandBufferMode );

    WorkerData* pObjects = new WorkerData;
    pObjects->m_dwNumObjects = m_dwGameObjectCount;
    XMemCpy( pObjects->m_Objects, m_GameObjects, m_dwGameObjectCount * sizeof(GameObject) );
    pObjects->m_matVP = m_matVP;
    pObjects->m_bRenderShadows = FALSE;
    pObjects->m_pRenderTarget = m_pAutoColorTarget;
    pObjects->m_pDepthStencil = m_pAutoDepthStencil;
    m_ObjectWorker.SubmitWorkItem( pObjects, m_CommandBufferMode );

    WorkerData* pCrowd = new WorkerData;
    pCrowd->m_dwNumObjects = m_dwCrowdObjectCount;
    XMemCpy( pCrowd->m_Objects, m_CrowdObjects, m_dwCrowdObjectCount * sizeof(GameObject) );
    pCrowd->m_matVP = m_matVP;
    pCrowd->m_bRenderShadows = FALSE;
    pCrowd->m_pRenderTarget = m_pAutoColorTarget;
    pCrowd->m_pDepthStencil = m_pAutoDepthStencil;
    m_CrowdWorker.SubmitWorkItem( pCrowd, m_CommandBufferMode );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RenderSceneSynchronous()
// Desc: Renders the scene using precompiled command buffers.  This method must wait on
//       the worker threads to finish constructing the precompiled command buffers
//       it runs them.
//--------------------------------------------------------------------------------------
VOID Sample::RenderSceneSynchronous()
{
    // Build precompiled command buffers.
    BuildCommandBuffers();

    // Clear the Shadow Map
    m_pd3dDevice->SetRenderTarget( 0, NULL );
    m_pd3dDevice->SetDepthStencilSurface( m_pShadowMapTarget );
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0 );
    m_pd3dDevice->FlushHiZStencil( D3DFHZS_ASYNCHRONOUS );

    // Render the Shadow Map
    ATG::Timer WaitTimer;
    WaitTimer.Start();
    D3DCommandBuffer* pObjectShadowPCB = m_ObjectShadowWorker.WaitForCommandBuffer();
    WaitTimer.Stop();
    m_pd3dDevice->RunCommandBuffer( pObjectShadowPCB, 0 );

    WaitTimer.Start();
    D3DCommandBuffer* pCrowdShadowPCB = m_CrowdShadowWorker.WaitForCommandBuffer();
    WaitTimer.Stop();
    m_pd3dDevice->RunCommandBuffer( pCrowdShadowPCB, 0 );

    // Resolve the Shadow Map to the Texture
    m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, m_pShadowMapTexture, NULL, 0, 0, NULL, 0, 0, NULL );

    // Clear the Render Target and Depth Buffer
    m_pd3dDevice->SetRenderTarget( 0, m_pAutoColorTarget );
    m_pd3dDevice->SetDepthStencilSurface( m_pAutoDepthStencil );
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0 );
    m_pd3dDevice->FlushHiZStencil( D3DFHZS_ASYNCHRONOUS );

    // Render The Objects and Crowd
    WaitTimer.Start();
    D3DCommandBuffer* pObjectPCB = m_ObjectWorker.WaitForCommandBuffer();
    WaitTimer.Stop();
    m_pd3dDevice->RunCommandBuffer( pObjectPCB, 0 );

    WaitTimer.Start();
    D3DCommandBuffer* pCrowdPCB = m_CrowdWorker.WaitForCommandBuffer();
    WaitTimer.Stop();
    m_pd3dDevice->RunCommandBuffer( pCrowdPCB, 0 );

    m_fCPUWaitTime = ( FLOAT )WaitTimer.GetAppTime();

    // Render ground plane
    RenderEnvironment( m_pd3dDevice );
}


//--------------------------------------------------------------------------------------
// Name: RenderSceneAsync()
// Desc: Renders the scene using async precompiled command buffers.
//       This is similar to the normal precompiled command buffer rendering performed
//       above.  However we do not need to wait on the worker threads to finish
//       constructing the PCB.  We can insert the asynchronous calls here and continue
//       on to perform other tasks.
//
//       If the worker thread finishes constructing the PCB before the GPU reaches the
//       call, then everything works like normal.  If the GPU reaches the command buffer
//       call before the worker thread is complete, then it will wait at that location
//       until the worker thread finishes the command buffer and calls FixupAndSignal()
//--------------------------------------------------------------------------------------
VOID Sample::RenderSceneAsync()
{
    // Build precompiled command buffers.
    BuildCommandBuffers();
    
    // Clear the Shadow Map
    m_pd3dDevice->SetRenderTarget( 0, NULL );
    m_pd3dDevice->SetDepthStencilSurface( m_pShadowMapTarget );
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0 );
    m_pd3dDevice->FlushHiZStencil( D3DFHZS_ASYNCHRONOUS );

    // Render the Shadow Map
    m_pd3dDevice->InsertAsyncCommandBufferCall( m_ObjectShadowWorker.GetAsyncCommandBufferCall(), 0, 0 );

    m_pd3dDevice->InsertAsyncCommandBufferCall( m_CrowdShadowWorker.GetAsyncCommandBufferCall(), 0, 0 );

    // Resolve the Shadow Map to the Texture
    m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, m_pShadowMapTexture, NULL, 0, 0, NULL, 0, 0, NULL );

    // Clear the Render Target and Depth Buffer
    m_pd3dDevice->SetRenderTarget( 0, m_pAutoColorTarget );
    m_pd3dDevice->SetDepthStencilSurface( m_pAutoDepthStencil );
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0 );
    m_pd3dDevice->FlushHiZStencil( D3DFHZS_ASYNCHRONOUS );

    // Render The Objects and Crowd
    m_pd3dDevice->InsertAsyncCommandBufferCall( m_ObjectWorker.GetAsyncCommandBufferCall(), 0, 0 );

    m_pd3dDevice->InsertAsyncCommandBufferCall( m_CrowdWorker.GetAsyncCommandBufferCall(), 0, 0 );

    // Render ground plane
    RenderEnvironment( m_pd3dDevice );
}


//--------------------------------------------------------------------------------------
// Name: SetupLightingConstants()
// Desc: Helper function that transforms lights into object space and sets them into
//       shader constants.
//--------------------------------------------------------------------------------------
VOID Sample::SetupLightingConstants( D3DDevice* pDevice, const XMMATRIX& matWorld )
{
    // Load world space light parameters
    XMVECTOR vLightParams[3];
    vLightParams[0] = XMLoadFloat4( &m_PointLightPosRange[0] );
    vLightParams[1] = XMLoadFloat4( &m_PointLightPosRange[1] );
    vLightParams[2] = XMLoadFloat3( &m_DirLightDirection );

    // Create inverse world matrix
    XMVECTOR vDummy;
    XMMATRIX matInvWorld = XMMatrixInverse( &vDummy, matWorld );

    // Transform light params into object space, while retaining the range value in the
    // w coordinate for the point lights
    static const XMVECTOR vSelectW = XMVectorSelectControl( 0, 0, 0, 1 );
    vLightParams[0] = XMVectorSelect( XMVector3Transform( vLightParams[0], matInvWorld ), vLightParams[0], vSelectW );
    vLightParams[1] = XMVectorSelect( XMVector3Transform( vLightParams[1], matInvWorld ), vLightParams[1], vSelectW );
    vLightParams[2] = XMVector3TransformNormal( vLightParams[2], matInvWorld );

    // Send the parameters to pixel shader constants
    pDevice->SetPixelShaderConstantF( 14, ( FLOAT* )&vLightParams[0], 2 );
    pDevice->SetPixelShaderConstantF( 16, ( FLOAT* )m_PointLightColor, 2 );
    pDevice->SetPixelShaderConstantF( 12, ( FLOAT* )&vLightParams[2], 1 );
    pDevice->SetPixelShaderConstantF( 13, ( FLOAT* )&m_DirLightColor, 1 );
}


//--------------------------------------------------------------------------------------
// Name: RenderEnvironment()
// Desc: Draws the ground plane.
//--------------------------------------------------------------------------------------
VOID Sample::RenderEnvironment( D3DDevice* pDevice )
{
    PIXBeginNamedEvent( 0, "Render Environment" );

    // Set shaders
    pDevice->SetVertexShader( m_pVertexShaderWVPTransform );
    pDevice->SetVertexDeclaration( NULL );
    pDevice->SetPixelShader( m_pPixelShaderObject );

    // Setup texture
    pDevice->SetTexture( 0, m_pTextureEnvironment );
    pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );

    pDevice->SetTexture( 1, m_pShadowMapTexture );
    pDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    pDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    pDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    pDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    // This method is intentionally inefficient and draws the ground as a 17x17
    // grid of quads.  This helps highlight the event's CPU time usage in PIX
    // so that we can observe the synchronization behavior when using the
    // different techniques for constructing precompiled command buffers.

    // Draw the ground as a series of squares
    for( int y = -8 ; y <= 8 ; y++ )
    {
        for( int x = -8 ; x <= 8 ; x++ )
        {
            // Setup WVP matrix, and uniform scale
            XMMATRIX matWorld = XMMatrixTranslation( ( FLOAT )x * 8.0f, 0, ( FLOAT )y * 8.0f );
            XMMATRIX matShadowTranspose = XMMatrixTranspose( matWorld * m_matLightVPT );
            XMMATRIX matWVPTranspose = XMMatrixTranspose( matWorld * m_matVP );
            pDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVPTranspose, 4 );
            pDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matShadowTranspose, 4 );
            const FLOAT fScale = 4.0f;
            pDevice->SetVertexShaderConstantF( 8, &fScale, 1 );

            // Setup lights
            SetupLightingConstants( pDevice, matWorld );

            pDevice->SetStreamSource( 0, m_pGroundVB, 0, 0 );
            pDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );
        }
    }

    PIXEndNamedEvent( );
}


//--------------------------------------------------------------------------------------
// Name: RenderObject()
// Desc: Draws a single game object.
//--------------------------------------------------------------------------------------
VOID Sample::RenderObject( D3DDevice* pDevice, const GameObject& obj, const XMMATRIX& matVP, BOOL bRenderShadows )
{
    PIXBeginNamedEvent( 0, "RenderObject()" );

    // Set shaders
    pDevice->SetVertexShader( m_pVertexShaderWVPTransform );
    pDevice->SetVertexDeclaration( NULL );
    if( bRenderShadows )
    {
        pDevice->SetRenderState( D3DRS_DEPTHBIAS, ATG::FtoDW( 0.001f ) );
        pDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, ATG::FtoDW( 2.0f ) );
        pDevice->SetPixelShader( NULL );
    }
    else
    {
        pDevice->SetRenderState( D3DRS_DEPTHBIAS, 0 );
        pDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, 0 );
        pDevice->SetPixelShader( m_pPixelShaderObject );
    }

    // Load data from game object structure
    XMMATRIX matWorld = XMLoadFloat4x4( &obj.WorldMatrix );
    D3DBaseTexture* pTextureDiffuse = m_pTextureObject[ obj.dwTextureIndex ];

    // Set texture, WVP matrix, and uniform scale
    pDevice->SetTexture( 0, pTextureDiffuse );
    pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );

    pDevice->SetTexture( 1, m_pShadowMapTexture );
    pDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    pDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    pDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    pDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    XMMATRIX matWVPTranspose = XMMatrixTranspose( matWorld * matVP );
    XMMATRIX matShadowTranspose = XMMatrixTranspose( matWorld * m_matLightVPT );
    pDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVPTranspose, 4 );
    pDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matShadowTranspose, 4 );
    pDevice->SetVertexShaderConstantF( 8, &obj.fSize, 1 );

    // Setup lights
    SetupLightingConstants( pDevice, matWorld );

    // Draw
    pDevice->SetStreamSource( 0, m_pObjectVB, 0, 0 );
    pDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 6 );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RenderUI()
// Desc: Draws some statistics and other UI elements
//--------------------------------------------------------------------------------------
VOID Sample::RenderUI()
{
    PIXBeginNamedEvent( 0, "RenderUI()" );

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Draw a title and FPS indicator.
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"AsyncPrecompiledCommandBuffers" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.SetScaleFactors( 0.8f, 0.8f );
        WCHAR strText[300];
        swprintf_s( strText, L"%d objects", m_dwGameObjectCount );
        m_Font.DrawText( 0, 30, 0xFFFFFFFF, strText );
        swprintf_s( strText, L"Main thread render time: %5.3f microseconds", m_fCPURenderTime * 1000000.0f );
        m_Font.DrawText( 0, 50, 0xFFFFFFFF, strText );
        if( m_CommandBufferMode == USE_SYNC_CB )
        {
            swprintf_s( strText, L"Time spent waiting: %5.3f microseconds", m_fCPUWaitTime * 1000000.0f );
            m_Font.DrawText( 0, 70, 0xFFFFFFFF, strText );
        }
        if( m_CommandBufferMode == USE_ASYNC_CB )
            m_Font.DrawText( 0, 90, 0xFFFFFFFF, L"Using Asynchronous Command Buffers" );
        else if( m_CommandBufferMode == USE_SYNC_CB )
            m_Font.DrawText( 0, 90, 0xFFFFFFFF, L"Using Normal Synchronous Command Buffers" );
        else
            m_Font.DrawText( 0, 90, 0xFFFFFFFF, L"Single Threaded" );
        m_Font.End();
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RenderWorker::Initialize()
// Desc: Initializes a render worker thread
//       This method is called from the main thread
//--------------------------------------------------------------------------------------
VOID RenderWorker::Initialize( DWORD dwWorkerHWThread, const char* szThreadName, Sample* sample )
{
    HRESULT hr = Direct3D_CreateDevice( 0, D3DDEVTYPE_COMMAND_BUFFER, NULL, 0, NULL, &m_pCommandBufferDevice );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not create the command buffer device." );

    // Transfer Command Buffer Device Thread Ownership to the Worker Thread
    m_pCommandBufferDevice->ReleaseThreadOwnership();

    // Command buffers and async calls
    // We have multiple command buffers in case the GPU is still executing one when we begin the next frame
    for( DWORD i = 0 ; i < g_dwBufferFrames ; i++ )
    {
        hr = m_pCommandBufferDevice->CreateCommandBuffer( 512 * 1024, 0, &m_pCommandBuffer[i] );
        if( FAILED( hr ) )
            ATG::FatalError( "Could not create the command buffer." );
        hr = m_pCommandBufferDevice->CreateAsyncCommandBufferCall( NULL, NULL, 1, 0, &m_pAsyncCommandBufferCall[i] );
        if( FAILED( hr ) )
            ATG::FatalError( "Could not create the async command buffer call." );
    }

    // Event to signal when the command buffer is complete when not using async command buffers
    // We only need one event since only one command buffer is being constructed at a time
    m_Event = CreateEvent( NULL, FALSE, FALSE, NULL );

    m_sample = sample;

    m_szThreadName = szThreadName;

    // Start the Worker Thread
    m_pWorkerThread = new WorkerThread( this, dwWorkerHWThread );
}


//--------------------------------------------------------------------------------------
// Name: RenderWorker::ThreadStartup()
// Desc: Transfers the CB device ownership to the thread and sets the thread name
//       This method is called from the worker thread
//--------------------------------------------------------------------------------------
VOID RenderWorker::ThreadStartup( )
{
    // Transfer Command Buffer Device Thread Ownership to the Worker Thread
    m_pCommandBufferDevice->AcquireThreadOwnership();

    // Set the thread name to aid debugging
    ATG::SetThreadName( (DWORD)-1, m_szThreadName );
}


//--------------------------------------------------------------------------------------
// Name: RenderWorker::ThreadShutdown()
// Desc: Release CB device ownership
//       This method is called from the worker thread
//--------------------------------------------------------------------------------------
VOID RenderWorker::ThreadShutdown( )
{
    // Finished with Command Buffer Device Thread Ownership on the Worker Thread
    m_pCommandBufferDevice->ReleaseThreadOwnership();
}


//--------------------------------------------------------------------------------------
// Name: RenderWorker::SubmitWorkItem()
// Desc: Submit a Work Item
//       This method is called from the main thread
//--------------------------------------------------------------------------------------
VOID RenderWorker::SubmitWorkItem( WorkerData* item, COMMAND_BUFFER_MODE mode )
{
    // Switch to the next command buffer
    m_dwBufferNumber++;
    m_dwBufferNumber %= g_dwBufferFrames;

    item->m_pCommandBuffer = m_pCommandBuffer[ m_dwBufferNumber ];
    if( mode == USE_ASYNC_CB )
    {
        HRESULT hr = m_pAsyncCommandBufferCall[ m_dwBufferNumber ]->Reset( NULL, NULL, 1, 0 );
        if( FAILED( hr ) )
            ATG::FatalError( "Could not reset the async command buffer call." );
        item->m_pAsyncCommandBufferCall = m_pAsyncCommandBufferCall[ m_dwBufferNumber ];
        item->m_Event = INVALID_HANDLE_VALUE;
    }
    else
    {
        item->m_pAsyncCommandBufferCall = NULL;
        item->m_Event = m_Event;
    }

    m_pWorkerThread->AddWork( item );
}


//--------------------------------------------------------------------------------------
// Name: RenderWorker::WaitForCommandBuffer()
// Desc: Return the command buffer once it has finish construction
//       This method is called from the main thread
//--------------------------------------------------------------------------------------
D3DCommandBuffer* RenderWorker::WaitForCommandBuffer( )
{
    WaitForSingleObject( m_Event, INFINITE );
    return m_pCommandBuffer[ m_dwBufferNumber ];
}


//--------------------------------------------------------------------------------------
// Name: RenderWorker::GetAsyncCommandBufferCall()
// Desc: Retrieve the async command buffer call that is under construction
//       This method is called from the main thread
//--------------------------------------------------------------------------------------
D3DAsyncCommandBufferCall* RenderWorker::GetAsyncCommandBufferCall( )
{
    return m_pAsyncCommandBufferCall[ m_dwBufferNumber ];
}


//--------------------------------------------------------------------------------------
// Name: RenderWorker::DoWork()
// Desc: Worker thread to render objects into a precompiled command buffer
//       This method is called from the worker thread
//--------------------------------------------------------------------------------------
VOID RenderWorker::DoWork(void *pData)
{
    PIXBeginNamedEvent( 0, "%s DoWork()", m_szThreadName );
    WorkerData* pWorkerData = (WorkerData*)pData;

    // Build the command buffer
    HRESULT hr = m_pCommandBufferDevice->BeginCommandBuffer( pWorkerData->m_pCommandBuffer, 0, NULL, NULL, NULL, 0 );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not begin the command buffer." );

    m_pCommandBufferDevice->SetRenderTarget( 0, pWorkerData->m_pRenderTarget );
    m_pCommandBufferDevice->SetDepthStencilSurface( pWorkerData->m_pDepthStencil );

    for( DWORD i = 0; i < pWorkerData->m_dwNumObjects; ++i )
    {
        m_sample->RenderObject( m_pCommandBufferDevice, pWorkerData->m_Objects[i],
                                pWorkerData->m_matVP, pWorkerData->m_bRenderShadows );
    }

    hr = m_pCommandBufferDevice->EndCommandBuffer();
    if( FAILED( hr ) )
        ATG::FatalError( "Could not finish the command buffer." );

    // If we are using normal synchronous command buffers, we need to signal the main thread
    // that creation is complete so that it can call RunCommandBuffer
    if( pWorkerData->m_Event != INVALID_HANDLE_VALUE )
        SetEvent( pWorkerData->m_Event );

    // Otherwise if we are using async command buffers, we need to fixup the async command buffer call
    // The GPU can now begin excuting the InsertAsyncCommandBufferCall() we inserted on the main thread
    if( pWorkerData->m_pAsyncCommandBufferCall != NULL )
        pWorkerData->m_pAsyncCommandBufferCall->FixupAndSignal( pWorkerData->m_pCommandBuffer, 0, 0 );

    // The work item is complete
    delete pWorkerData;

    PIXEndNamedEvent();
}
