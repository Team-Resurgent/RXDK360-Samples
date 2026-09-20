//--------------------------------------------------------------------------------------
// PrecompiledCommandBuffers.cpp
//
// This sample demonstrates the use of precompiled command buffers in scene rendering.
// Pieces of the scene are recorded into command buffers at initialization time, and
// then played back at runtime.  This results in a large CPU-side savings, since all of
// the Direct3D API usage for the recorded segments of the scene is eliminated.
// 
// BuildCommandBuffers() is where the precompiled command buffers are created and
// recorded.  Note the use of D3D tag collections to describe which parts of D3D state
// are "inherited" from the main device at playback time.  This allows state like shader
// constants or textures to be selected at playback time, while other state which does
// not change per frame is "baked" into the command buffer.
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

// Disable CodeAnalysis stack usage warning:
// warning C6262: Function uses '43908' bytes of stack: exceeds /analyze:stacksize'32768'. Consider moving some data to heap
#pragma warning(disable : 6262)

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

const DWORD g_dwMaxGameObjectCount = 100;
const DWORD g_dwCrowdSize = 300;

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: The precompiled command buffers sample class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    HRESULT     Initialize();
    HRESULT     Update();
    HRESULT     Render();

private:
    VOID        InitializeAssets();
    VOID        InitializeGameObjects();
    VOID        BuildCommandBuffers();

    VOID        PrefetchGameObjects();
    VOID        UpdateGameObjects( FLOAT fDeltaTime );

    VOID        RenderSceneWithBuffers();
    VOID        RenderSceneNormal();

    VOID        RenderCrowdNormal( D3DDevice* pDevice );
    VOID        RenderEnvironmentNormal( D3DDevice* pDevice );
    VOID        RenderObjectNormal( D3DDevice* pDevice, const GameObject& obj );
    VOID        RenderCrowdToBuffer( D3DDevice* pDevice );
    VOID        RenderEnvironmentToBuffer( D3DDevice* pDevice );
    VOID        RenderObjectToBuffer( D3DDevice* pDevice );

    VOID        SetupLightingConstants( D3DDevice* pDevice, const XMMATRIX& matWorld );

    VOID        RenderUI();

private:
    D3DDevice* m_pCommandBufferDevice;
    D3DCommandBuffer* m_pCommandBufferCrowd;
    D3DCommandBuffer* m_pCommandBufferObject;
    D3DCommandBuffer* m_pCommandBufferGround;

    XMMATRIX m_matProj;
    XMMATRIX m_matVP;
    FLOAT m_fViewDistance;

    ATG::Font m_Font;
    ATG::Timer m_Timer;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    ATG::PackedResource m_TextureResource;
    D3DTexture* m_pTextureEnvironment;
    D3DTexture* m_pTextureObject[4];

    D3DVertexShader* m_pVertexShaderWVPTransform;
    D3DVertexShader* m_pVertexShaderWTransformVPTransform;
    D3DPixelShader* m_pPixelShaderObject;
    D3DPixelShader* m_pPixelShaderEnv;

    D3DVertexBuffer* m_pObjectVB;
    D3DVertexDeclaration* m_pObjectDecl;
    D3DVertexBuffer* m_pGroundVB;

    D3DSurface* m_pAutoColorTarget;
    D3DSurface* m_pAutoDepthStencil;

    GameObject  m_CrowdObjects[ g_dwCrowdSize ];
    DWORD m_dwCrowdObjectCount;
    GameObject  m_GameObjects[ g_dwMaxGameObjectCount ];
    DWORD m_dwGameObjectCount;
    DWORD m_dwObjectIndexTarget;
    DWORD m_dwObjectIndexAlive;

    XMFLOAT4    m_PointLightPosRange[2];
    XMFLOAT4    m_PointLightColor[2];
    XMFLOAT3 m_DirLightDirection;
    XMFLOAT4 m_DirLightColor;

    BOOL m_bUsePrecompiledCommandBuffers;
    BOOL m_bRunSimulation;

    FLOAT m_fCPURenderTime;
};




//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample PCBSample;

    // Set up the structure used to create the D3DDevice.
    D3DPRESENT_PARAMETERS& d3dpp = PCBSample.m_d3dpp;
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

    PCBSample.Run();
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the sample, and calls into other initialization functions.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;
    m_bUsePrecompiledCommandBuffers = TRUE;
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

    // Build precompiled command buffers.
    BuildCommandBuffers();

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

    // Load vertex shaders.
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ObjectVS_WVP.xvu", &m_pVertexShaderWVPTransform ) ) )
        ATG::FatalError( "Could not load shader." );
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ObjectVS_W_VP.xvu",
                                       &m_pVertexShaderWTransformVPTransform ) ) )
        ATG::FatalError( "Could not load shader." );
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ObjectPS.xpu", &m_pPixelShaderObject ) ) )
        ATG::FatalError( "Could not load shader." );
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\EnvironmentPS.xpu", &m_pPixelShaderEnv ) ) )
        ATG::FatalError( "Could not load shader." );

    // Create environment geometry.
    DWORD dwVertexSize = ( DWORD )sizeof( Vertex_PTN );
    DWORD dwVBSize = dwVertexSize * 4;
    HRESULT hr = m_pd3dDevice->CreateVertexBuffer( dwVBSize,
                                                   0,
                                                   0,
                                                   D3DPOOL_DEFAULT,
                                                   &m_pGroundVB,
                                                   NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not create environment VB." );

    Vertex_PTN* pVertexData = NULL;
    m_pGroundVB->Lock( 0, 0, ( VOID** )&pVertexData, 0 );
    memcpy( pVertexData, g_MeshDataGround, dwVBSize );
    m_pGroundVB->Unlock();

    // Create object geometry.
    dwVertexSize = ( DWORD )sizeof( Vertex_PTN );
    dwVBSize = dwVertexSize * 4 * 6;
    hr = m_pd3dDevice->CreateVertexBuffer( dwVBSize,
                                           0,
                                           0,
                                           D3DPOOL_DEFAULT,
                                           &m_pObjectVB,
                                           NULL );
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
    m_pVertexShaderWTransformVPTransform->Bind( 0, m_pObjectDecl, &dwVertexSize, NULL );
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
// Name: BuildCommandBuffers()
// Desc: Creates the command buffer device and the three command buffer objects.
//       Records pieces of scene rendering to each of the 3 buffers.
//--------------------------------------------------------------------------------------
VOID Sample::BuildCommandBuffers()
{
    // Create command buffer device.
    Direct3D_CreateDevice( 0, D3DDEVTYPE_COMMAND_BUFFER, NULL, 0, NULL, &m_pCommandBufferDevice );

    // Create command buffers for recording various pieces of the scene rendering.
    m_pCommandBufferDevice->CreateCommandBuffer( 256 * 1024, 0, &m_pCommandBufferCrowd );
    m_pCommandBufferDevice->CreateCommandBuffer( 32 * 1024, 0, &m_pCommandBufferObject );
    m_pCommandBufferDevice->CreateCommandBuffer( 32 * 1024, 0, &m_pCommandBufferGround );

    // We don't inherit constants 252 through 255 because our shaders have literal
    // constants which are set in that range and which would clobber any constants
    // we tried to inherit in that range
    const DWORD maxInheritedConstant = 252;

    // Build crowd command buffer.
    {
        D3DTAGCOLLECTION InheritTags = { 0 };
        // VS constants 0-3 for view*projection transform
        D3DTagCollection_SetVertexShaderConstantFTag( &InheritTags, 0, 4 );

        // We will "inherit" the remainder of the constant table that is not used by the
        // crowd rendering shaders.  The shaders do not use these constants, but this
        // inheritance will cut down on redundant state setting both at record time and
        // playback time.  
        D3DTagCollection_SetVertexShaderConstantFTag( &InheritTags, 12, maxInheritedConstant - 12 );
        D3DTagCollection_SetPixelShaderConstantFTag( &InheritTags, 20, maxInheritedConstant - 20 );

        m_pCommandBufferDevice->BeginCommandBuffer( m_pCommandBufferCrowd, 0, &InheritTags, NULL, NULL, 0 );
        m_pCommandBufferDevice->SetRenderTarget( 0, m_pAutoColorTarget );
        m_pCommandBufferDevice->SetDepthStencilSurface( m_pAutoDepthStencil );
        RenderCrowdToBuffer( m_pCommandBufferDevice );
        m_pCommandBufferDevice->EndCommandBuffer();
    }

    // Build object command buffer.
    {
        D3DTAGCOLLECTION InheritTags = { 0 };
        // VS constants 0-3 for world*view*projection transform
        D3DTagCollection_SetVertexShaderConstantFTag( &InheritTags, 0, 4 );
        // VS constant 8 (size)
        D3DTagCollection_SetVertexShaderConstantFTag( &InheritTags, 8, 4 );
        // PS constants 12-17 (lights)
        D3DTagCollection_SetPixelShaderConstantFTag( &InheritTags, 12, 8 );
        // Texture samplers (object texture)
        D3DTagCollection_SetTag( &InheritTags, D3DTAG_TEXTUREFETCHCONSTANTS );

        // Inherit remainder of constant table
        D3DTagCollection_SetVertexShaderConstantFTag( &InheritTags, 12, maxInheritedConstant - 12 );
        D3DTagCollection_SetPixelShaderConstantFTag( &InheritTags, 20, maxInheritedConstant - 20 );

        m_pCommandBufferDevice->BeginCommandBuffer( m_pCommandBufferObject, 0, &InheritTags, NULL, NULL, 0 );
        m_pCommandBufferDevice->SetRenderTarget( 0, m_pAutoColorTarget );
        m_pCommandBufferDevice->SetDepthStencilSurface( m_pAutoDepthStencil );
        RenderObjectToBuffer( m_pCommandBufferDevice );
        m_pCommandBufferDevice->EndCommandBuffer();
    }

    // Build environment command buffer.
    {
        D3DTAGCOLLECTION InheritTags = { 0 };
        // VS constants 0-3 for world*view*projection transform
        D3DTagCollection_SetVertexShaderConstantFTag( &InheritTags, 0, 4 );
        // PS constants 12-17 (lights)
        D3DTagCollection_SetPixelShaderConstantFTag( &InheritTags, 12, 8 );

        // Inherit remainder of constant table
        D3DTagCollection_SetVertexShaderConstantFTag( &InheritTags, 12, maxInheritedConstant - 12 );
        D3DTagCollection_SetPixelShaderConstantFTag( &InheritTags, 20, maxInheritedConstant - 20 );

        m_pCommandBufferDevice->BeginCommandBuffer( m_pCommandBufferGround, 0, &InheritTags, NULL, NULL, 0 );
        m_pCommandBufferDevice->SetRenderTarget( 0, m_pAutoColorTarget );
        m_pCommandBufferDevice->SetDepthStencilSurface( m_pAutoDepthStencil );
        RenderEnvironmentToBuffer( m_pCommandBufferDevice );
        m_pCommandBufferDevice->EndCommandBuffer();
    }
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Updates the sample - camera movement, toggles usage of command buffers, and
//       optionally updates the simulation.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
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
        m_bUsePrecompiledCommandBuffers = !m_bUsePrecompiledCommandBuffers;

    if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER ) && m_dwGameObjectCount > 0 )
        m_dwGameObjectCount -= 10;
    else if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER ) &&
             m_dwGameObjectCount < g_dwMaxGameObjectCount )
        m_dwGameObjectCount += 10;

    // Compute camera rotation.
    static FLOAT s_fRotateY = 0.0f;
    static FLOAT s_fRotateX = 0.5f;
    s_fRotateY += pGamepad->fX1 * fDeltaTime * XM_PIDIV2;
    s_fRotateX += pGamepad->fY1 * fDeltaTime * XM_PIDIV2;
    s_fRotateX = min( max( s_fRotateX, 0 ), XM_PIDIV2 );
    XMVECTOR qRotateY = XMQuaternionRotationAxis( XMVectorSet( 0, 1, 0, 0 ), s_fRotateY );
    XMVECTOR qRotateX = XMQuaternionRotationAxis( XMVectorSet( 1, 0, 0, 0 ), s_fRotateX );
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

    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0 );

    ATG::Timer RenderTimer;

    // Set some default renderstate.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );

    // Render the scene.
    if( m_bUsePrecompiledCommandBuffers )
        RenderSceneWithBuffers();
    else
        RenderSceneNormal();

    m_fCPURenderTime = ( FLOAT )RenderTimer.GetElapsedTime();

    // Render UI.
    RenderUI();

    m_pd3dDevice->EndScene();

    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderSceneNormal()
// Desc: Renders the scene without using precompiled command buffers.
//--------------------------------------------------------------------------------------
VOID Sample::RenderSceneNormal()
{
    RenderEnvironmentNormal( m_pd3dDevice );
    RenderCrowdNormal( m_pd3dDevice );

    PIXBeginNamedEvent( 0, "Object Rendering" );
    for( DWORD i = 0; i < m_dwGameObjectCount; ++i )
    {
        RenderObjectNormal( m_pd3dDevice, m_GameObjects[i] );
    }
    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RenderSceneWithBuffers()
// Desc: Renders the scene using precompiled command buffers.  This method consists of
//       runtime game state being set into shader constants, followed by calls to
//       RunCommandBuffer to play back command buffer objects.  The command buffer
//       objects contain the GPU commands that do not change from frame to frame.
//--------------------------------------------------------------------------------------
VOID Sample::RenderSceneWithBuffers()
{
    m_pd3dDevice->SetRenderTarget( 0, m_pAutoColorTarget );
    m_pd3dDevice->SetDepthStencilSurface( m_pAutoDepthStencil );

    XMMATRIX matVPTranspose = XMMatrixTranspose( m_matVP );

    // Render ground plane

    // Send camera matrix into shader constants
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matVPTranspose, 4 );

    // Setup lighting for the ground plane
    SetupLightingConstants( m_pd3dDevice, XMMatrixIdentity() );

    // Play back the command buffer for the ground
    m_pd3dDevice->RunCommandBuffer( m_pCommandBufferGround, 0 );

    // Render crowd
    // Send camera matrix into shader constants
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matVPTranspose, 4 );
    // Play back the command buffer for the crowd
    // Individual world transforms for each crowd object are embedded in the command
    // buffer data.  The vertex shader combines the static world transforms with the
    // dynamic view*projection matrix that is computed every frame.
    m_pd3dDevice->RunCommandBuffer( m_pCommandBufferCrowd, 0 );

    // Render objects
    for( DWORD i = 0; i < m_dwGameObjectCount; ++i )
    {
        const GameObject& obj = m_GameObjects[i];

        // Set the camera matrix, lights, and uniformscale into shader constants.
        XMMATRIX matWorld = XMLoadFloat4x4( &obj.WorldMatrix );
        XMMATRIX matWVPTranspose = XMMatrixTranspose( matWorld * m_matVP );
        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVPTranspose, 4 );
        SetupLightingConstants( m_pd3dDevice, matWorld );
        m_pd3dDevice->SetVertexShaderConstantF( 8, &obj.fSize, 1 );
        m_pd3dDevice->SetTexture( 0, m_pTextureObject[ obj.dwTextureIndex ] );

        // Play back the command buffer for a single object
        m_pd3dDevice->RunCommandBuffer( m_pCommandBufferObject, 0 );
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderCrowdNormal()
// Desc: Loops through crowd object array and draws each object.
//--------------------------------------------------------------------------------------
VOID Sample::RenderCrowdNormal( D3DDevice* pDevice )
{
    PIXBeginNamedEvent( 0, "Crowd Render" );

    // Set shaders and mesh data.
    pDevice->SetVertexShader( m_pVertexShaderWVPTransform );
    pDevice->SetVertexDeclaration( m_pObjectDecl );
    pDevice->SetPixelShader( m_pPixelShaderEnv );
    pDevice->SetStreamSource( 0, m_pObjectVB, 0, sizeof( Vertex_PTN ) );

    // Set light constants.
    XMVECTOR vZero[] = { XMVectorZero(), XMVectorZero() };
    pDevice->SetPixelShaderConstantF( 14, ( FLOAT* )&vZero, 2 );
    pDevice->SetPixelShaderConstantF( 16, ( FLOAT* )&vZero, 2 );
    pDevice->SetPixelShaderConstantF( 13, ( FLOAT* )&m_DirLightColor, 1 );

    // Set sampler state common to all crowd objects.
    pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );

    const XMVECTOR vDirLightDirection = XMLoadFloat3( &m_DirLightDirection );
    static const XMVECTOR vSelectW = XMVectorSelectControl( 0, 0, 0, 1 );

    for( DWORD i = 0; i < m_dwCrowdObjectCount; ++i )
    {
        GameObject& obj = m_CrowdObjects[ i ];

        // Set texture corresponding to the object's texture index.
        D3DBaseTexture* pTextureDiffuse = m_pTextureObject[ obj.dwTextureIndex ];
        pDevice->SetTexture( 0, pTextureDiffuse );

        // Set WVP matrix and uniform scale.
        XMMATRIX matWorld = XMLoadFloat4x4( &obj.WorldMatrix );
        XMMATRIX matWVPTranspose = XMMatrixTranspose( matWorld * m_matVP );
        pDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVPTranspose, 4 );
        pDevice->SetVertexShaderConstantF( 8, &obj.fSize, 1 );

        // Transform directional light to object space.
        XMVECTOR vDummy;
        XMMATRIX matInvWorld = XMMatrixInverse( &vDummy, matWorld );
        XMVECTOR vDirLightObjDir = XMVector3TransformNormal( vDirLightDirection, matInvWorld );
        pDevice->SetPixelShaderConstantF( 12, ( FLOAT* )&vDirLightObjDir, 1 );

        // Draw the object.
        pDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 6 );
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RenderCrowdToBuffer()
// Desc: Draws the crowd to a command buffer device.
//--------------------------------------------------------------------------------------
VOID Sample::RenderCrowdToBuffer( D3DDevice* pDevice )
{
    PIXBeginNamedEvent( 0, "Crowd Render" );

    pDevice->SetVertexShader( m_pVertexShaderWTransformVPTransform );
    pDevice->SetVertexDeclaration( m_pObjectDecl );
    pDevice->SetPixelShader( m_pPixelShaderEnv );
    pDevice->SetStreamSource( 0, m_pObjectVB, 0, sizeof( Vertex_PTN ) );

    XMVECTOR vZero[] = { XMVectorZero(), XMVectorZero() };
    pDevice->SetPixelShaderConstantF( 14, ( FLOAT* )&vZero, 2 );
    pDevice->SetPixelShaderConstantF( 16, ( FLOAT* )&vZero, 2 );
    pDevice->SetPixelShaderConstantF( 13, ( FLOAT* )&m_DirLightColor, 1 );

    pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );

    const XMVECTOR vDirLightDirection = XMLoadFloat3( &m_DirLightDirection );
    static const XMVECTOR vSelectW = XMVectorSelectControl( 0, 0, 0, 1 );

    for( DWORD i = 0; i < m_dwCrowdObjectCount; ++i )
    {
        GameObject& obj = m_CrowdObjects[ i ];

        D3DBaseTexture* pTextureDiffuse = m_pTextureObject[ obj.dwTextureIndex ];
        pDevice->SetTexture( 0, pTextureDiffuse );

        // Note that we're only setting the world matrix into constants 4-7 here.
        // The view*projection matrix is common to all crowd objects, and will be set at
        // command buffer playback time.
        XMMATRIX matWorld = XMLoadFloat4x4( &obj.WorldMatrix );
        XMMATRIX matWTranspose = XMMatrixTranspose( matWorld );
        pDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matWTranspose, 4 );
        pDevice->SetVertexShaderConstantF( 8, &obj.fSize, 1 );

        XMVECTOR vDummy;
        XMMATRIX matInvWorld = XMMatrixInverse( &vDummy, matWorld );
        XMVECTOR vDirLightObjDir = XMVector3TransformNormal( vDirLightDirection, matInvWorld );
        pDevice->SetPixelShaderConstantF( 12, ( FLOAT* )&vDirLightObjDir, 1 );

        pDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 6 );
    }

    PIXEndNamedEvent();
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
// Name: RenderEnvironmentNormal()
// Desc: Draws the ground plane.
//--------------------------------------------------------------------------------------
VOID Sample::RenderEnvironmentNormal( D3DDevice* pDevice )
{
    // Set shaders
    pDevice->SetVertexShader( m_pVertexShaderWVPTransform );
    pDevice->SetVertexDeclaration( m_pObjectDecl );
    pDevice->SetPixelShader( m_pPixelShaderObject );

    // Setup texture, WVP matrix, and uniform scale
    pDevice->SetTexture( 0, m_pTextureEnvironment );
    pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
    XMMATRIX matWVPTranspose = XMMatrixTranspose( m_matVP );
    pDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVPTranspose, 4 );
    const FLOAT fScale = 60.0f;
    pDevice->SetVertexShaderConstantF( 8, &fScale, 1 );

    // Setup lights
    SetupLightingConstants( pDevice, XMMatrixIdentity() );

    // Draw
    pDevice->SetStreamSource( 0, m_pGroundVB, 0, sizeof( Vertex_PTN ) );
    pDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );
}


//--------------------------------------------------------------------------------------
// Name: RenderEnvironmentToBuffer()
// Desc: Draws the ground plane to a command buffer device.  Note how the only
//       difference between this function and RenderEnvironmentNormal() is the lack of
//       the view*projection matrix in this function.
//--------------------------------------------------------------------------------------
VOID Sample::RenderEnvironmentToBuffer( D3DDevice* pDevice )
{
    pDevice->SetVertexShader( m_pVertexShaderWVPTransform );
    pDevice->SetVertexDeclaration( m_pObjectDecl );
    pDevice->SetPixelShader( m_pPixelShaderObject );

    pDevice->SetTexture( 0, m_pTextureEnvironment );
    pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
    const FLOAT fScale = 60.0f;
    pDevice->SetVertexShaderConstantF( 8, &fScale, 1 );

    pDevice->SetStreamSource( 0, m_pGroundVB, 0, sizeof( Vertex_PTN ) );
    pDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );
}


//--------------------------------------------------------------------------------------
// Name: RenderObjectNormal()
// Desc: Draws a single game object.
//--------------------------------------------------------------------------------------
VOID Sample::RenderObjectNormal( D3DDevice* pDevice, const GameObject& obj )
{
    // Set shaders
    pDevice->SetVertexShader( m_pVertexShaderWVPTransform );
    pDevice->SetVertexDeclaration( m_pObjectDecl );
    pDevice->SetPixelShader( m_pPixelShaderObject );

    // Load data from game object structure
    XMMATRIX matWorld = XMLoadFloat4x4( &obj.WorldMatrix );
    D3DBaseTexture* pTextureDiffuse = m_pTextureObject[ obj.dwTextureIndex ];

    // Set texture, WVP matrix, and uniform scale
    pDevice->SetTexture( 0, pTextureDiffuse );
    pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    XMMATRIX matWVPTranspose = XMMatrixTranspose( matWorld * m_matVP );
    pDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVPTranspose, 4 );
    pDevice->SetVertexShaderConstantF( 8, &obj.fSize, 1 );

    // Setup lights
    SetupLightingConstants( pDevice, matWorld );

    // Draw
    pDevice->SetStreamSource( 0, m_pObjectVB, 0, sizeof( Vertex_PTN ) );
    pDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 6 );
}


//--------------------------------------------------------------------------------------
// Name: RenderObjectToBuffer()
// Desc: Draws an object to a command buffer device.  Since all parameters used to draw
//       an object change at runtime, none of the shader constants are set here.
//--------------------------------------------------------------------------------------
VOID Sample::RenderObjectToBuffer( D3DDevice* pDevice )
{
    // Set shaders
    pDevice->SetVertexShader( m_pVertexShaderWVPTransform );
    pDevice->SetVertexDeclaration( m_pObjectDecl );
    pDevice->SetPixelShader( m_pPixelShaderObject );

    // Draw
    pDevice->SetStreamSource( 0, m_pObjectVB, 0, sizeof( Vertex_PTN ) );
    pDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 6 );
}


//--------------------------------------------------------------------------------------
// Name: RenderUI()
// Desc: Draws some statistics and other UI elements
//--------------------------------------------------------------------------------------
VOID Sample::RenderUI()
{
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Draw a title and FPS indicator.
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"PrecompiledCommandBuffers" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.SetScaleFactors( 0.8f, 0.8f );
        WCHAR strText[300];
        swprintf_s( strText, L"%d objects", m_dwGameObjectCount );
        m_Font.DrawText( 0, 30, 0xFFFFFFFF, strText );
        swprintf_s( strText, L"CPU render time: %0.3f microseconds", m_fCPURenderTime * 1000000.0f );
        m_Font.DrawText( 0, 50, 0xFFFFFFFF, strText );
        swprintf_s( strText, L"%s precompiled command buffers",
                    m_bUsePrecompiledCommandBuffers ? L"Using" : L"Not using" );
        m_Font.DrawText( 0, 70, 0xFFFFFFFF, strText );
        m_Font.End();
    }
}
