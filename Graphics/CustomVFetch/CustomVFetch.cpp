//--------------------------------------------------------------------------------------
// CustomVFetch.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>

#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>

#include "ParticleSystem.h"
#include "InstancedMesh.h"

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_2, L"Spawn more\nparticles" },
    { ATG::HELP_LEFT_SHOULDER,  ATG::HELP_PLACEMENT_2, L"Less instances" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"More instances" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_2, L"Rotate camera" },
    { ATG::HELP_MISC_CALLOUT,   ATG::HELP_PLACEMENT_2, L"Triggers move camera in/out" },
};
static const DWORD  NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );

// Maximum number of instanced meshes.
static const DWORD  g_dwMaxInstanceCount = 50;

// Initial instance count.
static const DWORD  g_dwInitialInstanceCount = 10;

// Maximum particle count.
static const DWORD  g_dwMaxParticleCount = 30000;

// "Spawn extra" particle count.
static const FLOAT  g_fSpawnExtraParticleCount = 3000.0f;


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
protected:
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    FLOAT m_fViewDistance;

    XMMATRIX m_matView;
    XMMATRIX m_matProj;
    XMMATRIX m_matWorld;

    ParticleSystem m_ParticleSystem;
    InstancedMesh m_InstancedCube;
    DWORD m_dwCubeInstanceCount;

public:
                    Sample() : m_bDrawHelp( FALSE )
                    {
                    }

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    VOID            CreateCubeMesh();
    VOID            UpdateCubeInstanceData( FLOAT fTime );
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;

    // This sample runs exclusively at 720p.  The video scaler will handle all other
    // output resolutions.
    atgApp.m_d3dpp.BackBufferWidth = 1280;
    atgApp.m_d3dpp.BackBufferHeight = 720;

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all graphics resources and initializes the particle system and
//       instanced mesh system.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Create the font.
    if( FAILED( m_Font.Create( "d:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area.
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help.
    if( FAILED( m_Help.Create( "d:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Build the projection and world matrices.
    m_matProj = XMMatrixPerspectiveFovLH( XM_PIDIV4, ( 16.0f / 9.0f ), 1.0f, 200.0f );
    m_matWorld = XMMatrixIdentity();
    m_fViewDistance = 30.0f;

    // Initialize the simple shaders library.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Load the particle texture.
    D3DTexture* pTexture = NULL;
    HRESULT hr = D3DXCreateTextureFromFile( m_pd3dDevice, "game:\\media\\textures\\customvfetch-particles.dds",
                                            &pTexture );
    if( FAILED( hr ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize the particle system.
    m_ParticleSystem.Initialize( g_dwMaxParticleCount, pTexture );

    // Initialize the instanced mesh system.
    m_dwCubeInstanceCount = g_dwInitialInstanceCount;
    CreateCubeMesh();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateCubeMesh()
// Desc: This creates vertex and index data for a cube, and loads that data into the
//       instanced mesh system.
//--------------------------------------------------------------------------------------
VOID Sample::CreateCubeMesh()
{
    // Create the mesh vertex data, index data, and vertex declaration.
    const ATG::MeshVertexPCT CubeVerts[] =
    {
        { XMFLOAT3( -1, 1, 1 ), 0xFFFFFFFF, XMFLOAT2( 0, 0 ) },
        { XMFLOAT3( 1, 1, 1 ), 0xFFFF0000, XMFLOAT2( 1, 0 ) },
        { XMFLOAT3( 1, 1, -1 ), 0xFF000000, XMFLOAT2( 1, 1 ) },
        { XMFLOAT3( -1, 1, -1 ), 0xFF0000FF, XMFLOAT2( 0, 1 ) },

        { XMFLOAT3( -1, -1, 1 ), 0xFF000000, XMFLOAT2( 0, 0 ) },
        { XMFLOAT3( 1, -1, 1 ), 0xFFFFFF00, XMFLOAT2( 1, 0 ) },
        { XMFLOAT3( 1, -1, -1 ), 0xFFFFFFFF, XMFLOAT2( 1, 1 ) },
        { XMFLOAT3( -1, -1, -1 ), 0xFF00FF00, XMFLOAT2( 0, 1 ) },
    };

    static const WORD CubeIndices[] =
    {
        0, 1, 3,
        3, 1, 2,
        3, 2, 7,
        7, 2, 6,
        2, 1, 6,
        6, 1, 5,
        1, 0, 5,
        5, 0, 4,
        0, 3, 4,
        4, 3, 7,
        7, 6, 4,
        4, 6, 5
    };

    static const D3DVERTEXELEMENT9 CubeDecl[] =
    {
        { 0,     0, D3DDECLTYPE_FLOAT3,     0,  D3DDECLUSAGE_POSITION,  0 },
        { 0,    12, D3DDECLTYPE_D3DCOLOR,   0,  D3DDECLUSAGE_COLOR,     0 },
        { 0,    16, D3DDECLTYPE_FLOAT2,     0,  D3DDECLUSAGE_TEXCOORD,  0 },
        D3DDECL_END()
    };

    // The SourceMeshData struct holds information about the mesh to be instanced.
    SourceMeshData CubeMeshData;

    // Create a vertex buffer and copy the mesh vertex data into the VB.
    HRESULT hr = ATG::g_pd3dDevice->CreateVertexBuffer( sizeof( CubeVerts ),
                                                        0, 0, D3DPOOL_DEFAULT,
                                                        &CubeMeshData.m_pVB, NULL );
    ATG::MeshVertexPCT* pVBData = NULL;
    hr = CubeMeshData.m_pVB->Lock( 0, 0, ( VOID** )&pVBData, 0 );
    memcpy( pVBData, CubeVerts, sizeof( CubeVerts ) );
    CubeMeshData.m_pVB->Unlock();

    // Create an index buffer and copy the mesh index data into the IB.
    hr = ATG::g_pd3dDevice->CreateIndexBuffer( sizeof( CubeIndices ),
                                               0, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
                                               &CubeMeshData.m_pIB, NULL );
    WORD* pIBData = NULL;
    hr = CubeMeshData.m_pIB->Lock( 0, 0, ( VOID** )&pIBData, 0 );
    memcpy( pIBData, CubeIndices, sizeof( CubeIndices ) );
    CubeMeshData.m_pIB->Unlock();

    // Create the vertex declaration.
    hr = ATG::g_pd3dDevice->CreateVertexDeclaration( CubeDecl, &CubeMeshData.m_pVertexDecl );

    // Record the vertex stride and primitive type.
    CubeMeshData.m_dwVertexStride = ATG::MeshVertexPCT::Size();
    CubeMeshData.m_PrimitiveType = D3DPT_TRIANGLELIST;

    // Initialize the instanced mesh system.
    m_InstancedCube.Initialize( CubeMeshData, g_dwMaxInstanceCount );
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current time.
    FLOAT fTime = ( FLOAT )m_Timer.GetAppTime();
    FLOAT fDeltaTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad state.
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Compute camera rotation.
    static FLOAT s_fRotateY = 0.0f;
    static FLOAT s_fRotateX = 0.5f;
    s_fRotateY += pGamepad->fX1 * fDeltaTime * XM_PIDIV2;
    s_fRotateX += pGamepad->fY1 * fDeltaTime * XM_PIDIV2;
    s_fRotateX = min( max( s_fRotateX, -XM_PIDIV2 ), XM_PIDIV2 );
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
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );

    // Set the world view projection matrix into the debug draw system.
    XMMATRIX matWVP = m_matWorld * m_matView * m_matProj;
    ATG::DebugDraw::SetViewProjection( matWVP );

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // The A button spawns 3000 extra particles.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_ParticleSystem.SpawnExtra( g_fSpawnExtraParticleCount );

    // Update the particle system.
    m_ParticleSystem.Update( fDeltaTime );

    // The shoulder buttons change the mesh instance count.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
    {
        m_dwCubeInstanceCount = max( 1, m_dwCubeInstanceCount - 1 );
    }
    else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        m_dwCubeInstanceCount = min( g_dwMaxInstanceCount, m_dwCubeInstanceCount + 1 );
    }
    // Update the mesh instance data.
    UpdateCubeInstanceData( fTime );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateCubeInstanceData()
// Desc: Creates new instance data (positions and sizes) for the instanced meshes.
//--------------------------------------------------------------------------------------
VOID Sample::UpdateCubeInstanceData( FLOAT fTime )
{
    // Generate position and size data for the cube instances.
    FLOAT fTheta = XM_2PI / ( FLOAT )m_dwCubeInstanceCount;
    FLOAT fAngle = fTime * ( XM_2PI * 0.1f );

    InstanceData InstancePositions[ g_dwMaxInstanceCount ];
    XMemSet( &InstancePositions, 0, sizeof( InstancePositions ) );

    const FLOAT fRadius = 10.0f;

    for( DWORD i = 0; i < m_dwCubeInstanceCount; ++i )
    {
        FLOAT fCubeAngle = fmodf( fAngle, XM_2PI );
        InstancePositions[i].m_Position.x = fRadius * sinf( fCubeAngle );
        InstancePositions[i].m_Position.z = fRadius * cosf( fCubeAngle );
        InstancePositions[i].m_Scale = 0.5f * sinf( fCubeAngle * 3.0f ) + 1.0f;
        fAngle += fTheta;
    }

    // Send the new instance data to the mesh instancing system.
    m_InstancedCube.SetInstanceData( InstancePositions, m_dwCubeInstanceCount );
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a blue gradient background.
    ATG::RenderBackground( 0xFF000080, 0xFF000020 );

    // Draw gridlines for the ground plane.
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    ATG::DebugDraw::DrawGrid( XMFLOAT3( 10, 0, 0 ), XMFLOAT3( 0, 0, 10 ), XMFLOAT3( 0, 0, 0 ), 20, 20, 0xFF808080 );

    // Draw the instanced cubes.
    m_InstancedCube.Render( m_matWorld * m_matView * m_matProj );

    // Draw the particle system.
    m_ParticleSystem.Render( m_matWorld, m_matView, m_matProj );

    // Show title, frame rate, and help.
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"CustomVFetch" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.SetScaleFactors( 0.8f, 0.8f );
        WCHAR strText[100];
        swprintf_s( strText, L"Particles: %lu", m_ParticleSystem.GetActiveCount() );
        m_Font.DrawText( 0, 30, 0xff00ffff, strText, ATGFONT_RIGHT );
        swprintf_s( strText, L"Mesh Instances: %lu", m_dwCubeInstanceCount );
        m_Font.DrawText( 0, 50, 0xff00ffff, strText, ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene.
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
