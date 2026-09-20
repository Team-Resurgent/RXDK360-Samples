//--------------------------------------------------------------------------------------
// Instancing.cpp
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <XGraphics.h>

#include <AtgApp.h>
#include <AtgDebugDraw.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>

#include "InstancedMesh.h"

#if defined( NDEBUG ) && ( !defined( PROFILE ) || defined( FASTCAP ) )
#define _RELEASED3D
#endif

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_LEFT_SHOULDER,  ATG::HELP_PLACEMENT_1, L"Fewer instances" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_1, L"More instances" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_1, L"Rotate camera" },
    { ATG::HELP_MISC_CALLOUT,   ATG::HELP_PLACEMENT_1, L"Triggers move camera in/out" },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_1, L"Previous instancing method" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_1, L"Next instancing method" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_1, L"Fewer instances per batch" },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_1, L"More instances per batch" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_1, L"Change tessellation level" },
};

static const DWORD  NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );

WCHAR*              g_InstanceTypeName[] =
{
    L"1a. One Instance Per Draw - VFetch Instance Data",
    L"1b. One Instance Per Draw - GPU Constant Instance Data",
    L"2. CustomVFetch",
    L"2a. Batches - VFetch Instance Data",
    L"2b. Batches - GPU Constant Instance Data",
    L"3. XPS",
    L"4a. Tessellator - VFetch Instance Data",
    L"4b. Tessellator - GPU Constant Instance Data",
};

struct InstanceSeed
{
    FLOAT m_fRadius;
    FLOAT m_fAngle;
    FLOAT m_fSize;
    FLOAT m_fY;
};

// Maximum number of instanced meshes.
static const DWORD  MAX_INSTANCE_COUNT = 10000;

InstanceData g_instanceData[ MAX_INSTANCE_COUNT ]; // Updated every frame
InstanceSeed g_instanceSeed[ MAX_INSTANCE_COUNT ]; // Frame independent instance data

VOID GenerateTexturedSphereGeometry( DWORD dwNumSlices, DWORD dwNumStacks,
                                     ATG::MeshVertexPT* pData, FLOAT fUVScaler );
VOID GenerateSphereIndices( DWORD dwNumSlices, DWORD dwNumStacks, WORD* pIndices );

extern LONG g_nXPStime_curFrame;
extern LONG g_nXPStime_prevFrame;
//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
                    Sample() : m_bDrawHelp( FALSE )
                    {
                    }

    virtual         ~Sample()
    {
        SAFE_RELEASE( m_pPlanetVB );
        SAFE_RELEASE( m_pPlanetIB );
        SAFE_RELEASE( m_pPlanetDecl );
        SAFE_RELEASE( m_pSkydomeVB );
        SAFE_RELEASE( m_pSkydomeIB );

        m_InstancedMesh.Destroy();
    }

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    VOID            CreateInstancedMesh();
    VOID            CreatePlanetMesh();
    VOID            CreateSkydomeMesh();

    VOID            InitializeInstanceData();
    VOID            UpdateInstanceData( FLOAT fTime );

    VOID            RenderOverlays();
    VOID            RenderInstances();

private:
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    FLOAT m_fViewDistance;

    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;
    XMMATRIX m_matWVP;
    XMMATRIX m_matWorld_Planet;
    XMMATRIX m_matWVP_Planet;
    XMMATRIX m_matWVP_Sky;

    InstancedMesh m_InstancedMesh;

    INT m_nInstancingMethod;         // Current instancing method

    // Planet rendering data
    D3DVertexBuffer* m_pPlanetVB;
    D3DIndexBuffer* m_pPlanetIB;
    D3DVertexDeclaration* m_pPlanetDecl;
    DWORD m_dwNumPlanetIndices;
    D3DVertexShader* m_pPlanetVS;
    D3DPixelShader* m_pPlanetPS;
    D3DTexture* m_pPlanetTexture;

    // Skydome rendering data
    D3DVertexBuffer* m_pSkydomeVB;
    D3DIndexBuffer* m_pSkydomeIB;
    D3DVertexDeclaration* m_pSkydomeDecl;
    DWORD m_dwNumSkydomeIndices;
    D3DTexture* m_pSkydomeTex;

    // Instance rendering data
    D3DTexture* m_pCraterTexture;
    D3DTexture* m_pDisplacementMap;

    INT m_nInstanceCount;
    INT m_nNumInstsPerBatch;
    DWORD m_dwNumInstsPerBatchRendered;
    DWORD m_dwTessLevel;

    // Timing counters
    FLOAT m_fUpdateTime;
    FLOAT m_fRenderTime;

    D3DPerfCounters* m_pPerfCounterStart[3];
    D3DPerfCounters* m_pPerfCounterEnd[3];

    // elapsed frames
    DWORD m_dwFrameCount;
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;

    // This sample runs exclusively at 720p.  The video scaler will handle all other
    // output resolutions.
    atgApp.m_d3dpp.BackBufferWidth = 1280;
    atgApp.m_d3dpp.BackBufferHeight = 720;

    // Create on multiple threads for XPS
    atgApp.m_dwDeviceCreationFlags |= D3DCREATE_CREATE_THREAD_ON_2 |
        D3DCREATE_CREATE_THREAD_ON_3 |
        D3DCREATE_CREATE_THREAD_ON_4 |
        D3DCREATE_CREATE_THREAD_ON_5 | D3DCREATE_BUFFER_2_FRAMES;

    atgApp.m_d3dpp.BackBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}

//--------------------------------------------------------------------------------------
// Name: GenerateTexturedSphere()
// Desc: Creates a sphere with texture coordinates
//--------------------------------------------------------------------------------------
VOID GenerateTexturedSphere( DWORD numSlices, DWORD numStacks, D3DVertexBuffer** pVB,
                             D3DIndexBuffer** pIB, DWORD* numIndices,
                             D3DVertexDeclaration** pDecl, FLOAT fUVScaler )
{
    // Create a vertex buffer and copy the mesh vertex data into it
    ATG::g_pd3dDevice->BlockUntilIdle();
    ATG::g_pd3dDevice->CreateVertexBuffer(
        sizeof( ATG::MeshVertexPT ) * ( numSlices + 1 ) * ( numStacks + 1 ),
        0, 0, D3DPOOL_DEFAULT, pVB, NULL );
    ATG::MeshVertexPT* pVBData = NULL;
    ( *pVB )->Lock( 0, 0, ( VOID** )&pVBData, 0 );
    GenerateTexturedSphereGeometry( numSlices, numStacks, pVBData, fUVScaler );
    ( *pVB )->Unlock();

    *numIndices = 4 * numSlices * numStacks;

    // Create an index buffer and copy in the mesh index data.
    ATG::g_pd3dDevice->CreateIndexBuffer( *numIndices * sizeof( WORD ),
                                          0, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
                                          pIB, NULL );
    WORD* pIBData = NULL;
    ( *pIB )->Lock( 0, 0, ( VOID** )&pIBData, 0 );
    GenerateSphereIndices( numSlices, numStacks, pIBData );
    ( *pIB )->Unlock();

    // Create the vertex declaration.
    static const D3DVERTEXELEMENT9 TexturedSphereDecl[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    ATG::g_pd3dDevice->CreateVertexDeclaration( TexturedSphereDecl, pDecl );
}


//--------------------------------------------------------------------------------------
// Name: GenerateTexturedSphereGeometry()
// Desc: Creates geometry for a sphere
//--------------------------------------------------------------------------------------
VOID GenerateTexturedSphereGeometry( DWORD numSlices, DWORD numStacks,
                                     ATG::MeshVertexPT* pData, FLOAT fUVScaler )
{
    for( DWORD i = 0; i < numSlices + 1; i++ )
    {
        for( DWORD j = 0; j < numStacks + 1; j++ )
        {
            FLOAT fTheta = FLOAT( i ) / numSlices * 2 * XM_PI;
            FLOAT fPhi = ( FLOAT( j ) / numStacks * 2 - 1.0f ) * XM_PIDIV2;
            pData->TexCoord.x = FLOAT( i ) / numSlices * fUVScaler;
            pData->TexCoord.y = FLOAT( j ) / numStacks * fUVScaler;
            pData->Position.x = cosf( fTheta ) * cosf( fPhi );
            pData->Position.z = sinf( fTheta ) * cosf( fPhi );
            pData->Position.y = sinf( fPhi );

            pData++;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: GenerateSphereIndices()
// Desc: Creates indices for a sphere
//--------------------------------------------------------------------------------------
VOID GenerateSphereIndices( DWORD dwNumSlices, DWORD dwNumStacks, WORD* pIndices )
{
    WORD i0 = ( WORD )0;
    WORD i1 = ( WORD )1;
    WORD i2 = ( WORD )dwNumStacks + 2;
    WORD i3 = ( WORD )dwNumStacks + 1;

    for( DWORD i = 0; i < dwNumSlices; i++ )
    {
        for( DWORD j = 0; j < dwNumStacks; j++ )
        {
            pIndices[0] = i0;
            pIndices[1] = i1;
            pIndices[2] = i2;
            pIndices[3] = i3;

            pIndices += 4;
            i0++;
            i1++;
            i2++;
            i3++;
        }
        i0++;
        i1++;
        i2++;
        i3++;
    }
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Creates all graphics resources and initializes the instanced mesh system.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_dwFrameCount = 0;
    m_dwTessLevel = 15;
    m_nInstancingMethod = ONEPERDRAW_VFETCH;
    m_nNumInstsPerBatch = 1000;
    m_nInstanceCount = 5000;

    // Create the font
    if( FAILED( m_Font.Create( "d:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "d:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Build the projection and world matrices
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3.0f, ( 16.0f / 9.0f ), .01f, 20000.0f );
    m_matWorld = XMMatrixIdentity();
    m_fViewDistance = 6.0f;

    // Initialize the simple shaders library
    ATG::SimpleShaders::Initialize( NULL, NULL );


    // Load textures
    HRESULT hr = D3DXCreateTextureFromFile( m_pd3dDevice,
                                            "game:\\media\\textures\\pgas06L.dds",
                                            &m_pPlanetTexture );
    if( FAILED( hr ) )
        return ATGAPPERR_MEDIANOTFOUND;

    hr = D3DXCreateTextureFromFile( m_pd3dDevice,
                                    "game:\\media\\textures\\starfield.dds",
                                    &m_pSkydomeTex );
    if( FAILED( hr ) )
        return ATGAPPERR_MEDIANOTFOUND;

    hr = D3DXCreateTextureFromFile( m_pd3dDevice,
                                    "game:\\media\\textures\\craters.dds",
                                    &m_pCraterTexture );
    if( FAILED( hr ) )
        return ATGAPPERR_MEDIANOTFOUND;

    hr = D3DXCreateTextureFromFile( m_pd3dDevice,
                                    "game:\\media\\textures\\displacement.dds",
                                    &m_pDisplacementMap );
    if( FAILED( hr ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Load Shaders
    hr = ATG::LoadVertexShader( "game:\\media\\shaders\\PlanetVS.xvu", &m_pPlanetVS );
    if( FAILED( hr ) )
        return ATGAPPERR_MEDIANOTFOUND;

    hr = ATG::LoadPixelShader( "game:\\media\\shaders\\PlanetPS.xpu", &m_pPlanetPS );
    if( FAILED( hr ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Force the texture formats to AS_16 sRGB formats.
    // Do this so there's no loss of precision when sampling the textures in shaders. If using
    // standard SRGB formats, the sRGB->Linear conversion happens with 8 bit precision, resulting 
    // in a loss of data. Using AS_16 sRGB formats causes the conversion to happen at 16-bit precision,
    // meaning no precision is lost.
    ATG::ConvertTextureToAs16SRGBFormat( m_pPlanetTexture );
    ATG::ConvertTextureToAs16SRGBFormat( m_pSkydomeTex );
    ATG::ConvertTextureToAs16SRGBFormat( m_pCraterTexture );

    // Create geometry
    CreateInstancedMesh();
    CreatePlanetMesh();
    CreateSkydomeMesh();

    InitializeInstanceData();

#ifndef _RELEASED3D
    // Set up GPU performance counter structures
    for( DWORD i = 0; i < 3; i++ )
    {
        m_pd3dDevice->CreatePerfCounters( &m_pPerfCounterStart[ i ], 1 );
        m_pd3dDevice->CreatePerfCounters( &m_pPerfCounterEnd[ i ], 1 );
    }

    m_pd3dDevice->EnablePerfCounters( TRUE );

    // Enable the performance counters we want
    D3DPERFCOUNTER_EVENTS PerfEvents;
    ZeroMemory( &PerfEvents, sizeof( D3DPERFCOUNTER_EVENTS ) );
    PerfEvents.RBBM[0] = GPUPE_RBBM_CP_NRT_BUSY;  // Command Processor busy cycles

    m_pd3dDevice->SetPerfCounterEvents( &PerfEvents, 0 );
#endif

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateInstancedMesh()
// Desc: This creates vertex and index data for a instance, and loads that data into the
//       instanced mesh system.
//--------------------------------------------------------------------------------------
VOID Sample::CreateInstancedMesh()
{
    SourceMeshData MeshData;
    MeshData.dwStride = ATG::MeshVertexPT::Size();
    MeshData.primType = D3DPT_QUADLIST;
    GenerateTexturedSphere( m_dwTessLevel + 1, m_dwTessLevel + 1, &MeshData.pVB,
                            &MeshData.pIB, &MeshData.dwNumIndices, &MeshData.pDecl, 1.0f );

    // Initialize the instanced mesh system
    m_InstancedMesh.Initialize( MeshData, MAX_INSTANCE_COUNT );
}


//--------------------------------------------------------------------------------------
// Name: CreatePlanetMesh()
// Desc: Creates vertex and index data for the planet
//--------------------------------------------------------------------------------------
VOID Sample::CreatePlanetMesh()
{
    const DWORD dwNumStacks = 100;
    const DWORD dwNumSlices = 50;
    GenerateTexturedSphere( dwNumStacks, dwNumSlices, &m_pPlanetVB, &m_pPlanetIB,
                            &m_dwNumPlanetIndices, &m_pPlanetDecl, 1.0f );
}


//--------------------------------------------------------------------------------------
// Name: CreateSkydomeMesh()
// Desc: Creates vertex and index data for the skydome
//--------------------------------------------------------------------------------------
VOID Sample::CreateSkydomeMesh()
{
    const DWORD dwNumStacks = 100;
    const DWORD dwNumSlices = 50;
    GenerateTexturedSphere( dwNumStacks, dwNumSlices, &m_pSkydomeVB, &m_pSkydomeIB,
                            &m_dwNumSkydomeIndices, &m_pSkydomeDecl, 2.0f );
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame.  Entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    m_dwFrameCount++;

    // Get the current time.
    FLOAT fTime = ( FLOAT )m_Timer.GetAppTime();
    FLOAT fDeltaTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad state.
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Compute camera rotation.
    static FLOAT s_fRotateY = XM_PIDIV2;
    static FLOAT s_fRotateX = 0.3f;
    s_fRotateY += pGamepad->fX1 * fDeltaTime * XM_PIDIV2;
    s_fRotateX += pGamepad->fY1 * fDeltaTime * XM_PIDIV2;
    s_fRotateX = min( max( s_fRotateX, -XM_PI / 5.0f ), XM_PI / 5.0f );
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
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    vUpVec = XMVector3Rotate( vUpVec, qRotation );
    vEyePt += vLookatPt;
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );

    // Update the world view projection matrix for the instances
    m_matWVP = m_matWorld * m_matView * m_matProj;

    // Update the matrices for the planet
    static FLOAT fYRot = 0.0f;
    const FLOAT rotSpeed = -.05f;
    fYRot += rotSpeed * fDeltaTime;
    const FLOAT fXRot = .4f;
    XMMATRIX matRotY = XMMatrixRotationY( fYRot );
    XMMATRIX matRotX = XMMatrixRotationX( fXRot );
    m_matWorld_Planet = matRotY * matRotX * m_matWorld;
    m_matWVP_Planet = matRotY * matRotX * m_matWorld * m_matView * m_matProj;

    // Update the matrices for the skydome
    const FLOAT fSkyDomeScale = 1000.0f;
    XMMATRIX matScale = XMMatrixScaling( fSkyDomeScale, fSkyDomeScale, fSkyDomeScale );
    m_matWVP_Sky = matScale * m_matWorld * m_matView * m_matProj;

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // The shoulder buttons change the mesh instance count.
    if( pGamepad->wLastButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
    {
        m_nInstanceCount = max( 1, m_nInstanceCount - 10 );
        InitializeInstanceData();
    }
    else if( pGamepad->wLastButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        m_nInstanceCount = min( MAX_INSTANCE_COUNT, m_nInstanceCount + 10 );
        InitializeInstanceData();
    }

    // X and Y change the number of instances per batch.
    if( pGamepad->wLastButtons & XINPUT_GAMEPAD_X )
    {
        m_nNumInstsPerBatch -= 5;
    }
    else if( pGamepad->wLastButtons & XINPUT_GAMEPAD_Y )
    {
        m_nNumInstsPerBatch += 5;
    }
    m_nNumInstsPerBatch = min( m_nInstanceCount, max( 1, m_nNumInstsPerBatch ) );

    // D-pad up and down change the tessellation level
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        m_dwTessLevel = max( m_dwTessLevel - 1, 1 );
        CreateInstancedMesh();
    }
    else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        m_dwTessLevel = min( m_dwTessLevel + 1, 15 );
        CreateInstancedMesh();
    }

    // A and B change the instancing method
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_nInstancingMethod = ( m_nInstancingMethod - 1 );
        if( m_nInstancingMethod < 0 )
            m_nInstancingMethod += NUM_INSTANCING_METHODS;
    }
    else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_nInstancingMethod = ( m_nInstancingMethod + 1 ) % NUM_INSTANCING_METHODS;
    }

    // Update the mesh instance data.
    ATG::Timer CPUTimer;
    UpdateInstanceData( fTime );
    m_fUpdateTime = ( FLOAT )CPUTimer.GetElapsedTime();

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: InitializeInstanceData()
// Desc: Creates new instance data for the instanced meshes.  This function is called
//       only when the settings change.
//--------------------------------------------------------------------------------------
VOID Sample::InitializeInstanceData()
{
    XMemSet( &g_instanceData, 0, sizeof( g_instanceData ) );

    // Seed the random number generator to stay consistent across changes in settings
    srand( 1 );

    const FLOAT fRadius = 3.0f;
    for( INT i = 0; i < m_nInstanceCount; ++i )
    {
        FLOAT frand1 = ( rand() / FLOAT( RAND_MAX ) ) * 2.0f - 1.0f;
        FLOAT frand2 = ( rand() / FLOAT( RAND_MAX ) ) * 2.0f - 1.0f;
        FLOAT frand3 = ( rand() / FLOAT( RAND_MAX ) ) * 2.0f - 1.0f;
        FLOAT frand4 = ( rand() / FLOAT( RAND_MAX ) ) * 2.0f - 1.0f;
        static FLOAT yScaler = .1f;

        g_instanceSeed[i].m_fRadius = fRadius + frand1 * .5f;
        g_instanceSeed[i].m_fAngle = frand3 * XM_PI;

        // Scale and position can be set now, since they don't change per frame
        g_instanceData[i].m_fScale = .007f * frand2 + .01f;
        g_instanceData[i].m_vPosition.y = frand4 * yScaler;
    }
}


//--------------------------------------------------------------------------------------
// Name: UpdateInstanceData()
// Desc: Creates new instance data (positions and sizes) for the instanced meshes.
//       Called once per frame
//--------------------------------------------------------------------------------------
VOID Sample::UpdateInstanceData( FLOAT fTime )
{
    FLOAT fAngle = fTime * ( XM_2PI * 0.01f );

    for( INT i = 0; i < m_nInstanceCount; ++i )
    {
        float fCurAngle = fAngle + g_instanceSeed[i].m_fAngle;

        g_instanceData[i].m_vPosition.x = g_instanceSeed[i].m_fRadius * sinf( fCurAngle );
        g_instanceData[i].m_vPosition.z = g_instanceSeed[i].m_fRadius * cosf( fCurAngle );
    }

    // Send the new instance data to the mesh instancing system.
    m_InstancedMesh.SetInstanceData( g_instanceData, m_nInstanceCount );
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    ATG::g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET0 | D3DCLEAR_ZBUFFER,
                              0x00000000, 1.0f, 0 );

    PIXBeginNamedEvent( 0xFFFFFFFF, "Planet" );
    // Draw the planet
    ATG::g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m_matWVP_Planet, 4 );
    ATG::g_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&m_matWorld_Planet, 4 );
    ATG::g_pd3dDevice->SetVertexShader( m_pPlanetVS );
    ATG::g_pd3dDevice->SetPixelShader( m_pPlanetPS );
    ATG::g_pd3dDevice->SetStreamSource( 0, m_pPlanetVB, 0, sizeof( ATG::MeshVertexPT ) );
    ATG::g_pd3dDevice->SetIndices( m_pPlanetIB );
    ATG::g_pd3dDevice->SetVertexDeclaration( m_pPlanetDecl );
    ATG::g_pd3dDevice->SetTexture( 0, m_pPlanetTexture );
    ATG::g_pd3dDevice->DrawIndexedVertices( D3DPT_QUADLIST, 0, 0, m_dwNumPlanetIndices );
    PIXEndNamedEvent();

    // Draw the background
    PIXBeginNamedEvent( 0xFFFFFFFF, "Background" );
    ATG::SimpleShaders::BeginShader_Transformed_Textured( m_matWVP_Sky, m_pSkydomeTex );
    ATG::g_pd3dDevice->SetStreamSource( 0, m_pSkydomeVB, 0, sizeof( ATG::MeshVertexPT ) );
    ATG::g_pd3dDevice->SetIndices( m_pSkydomeIB );
    ATG::g_pd3dDevice->SetVertexDeclaration( m_pSkydomeDecl );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );
    ATG::g_pd3dDevice->DrawIndexedVertices( D3DPT_QUADLIST, 0, 0, m_dwNumSkydomeIndices );
    ATG::SimpleShaders::EndShader();
    PIXEndNamedEvent();

    RenderInstances();
    RenderOverlays();

    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderInstances()
// Desc: Render the instances using the currently selected method
//--------------------------------------------------------------------------------------
VOID Sample::RenderInstances()
{
#ifndef _RELEASED3D
    m_pd3dDevice->QueryPerfCounters( m_pPerfCounterStart[ m_dwFrameCount % 3 ],
                                     D3DPERFQUERY_WAITGPUIDLE );
#endif

    ATG::g_pd3dDevice->SetTexture( 0, m_pCraterTexture );
    ATG::g_pd3dDevice->SetTexture( D3DVERTEXTEXTURESAMPLER1, m_pDisplacementMap );

    ATG::Timer CPUTimer;

    // Draw the mesh instances.
    switch( m_nInstancingMethod )
    {
        default:
        case ONEPERDRAW_VFETCH:
            m_InstancedMesh.RenderOneInstPerDraw( m_matWVP, true );
            break;
        case ONEPERDRAW_GPUCONST:
            m_InstancedMesh.RenderOneInstPerDraw( m_matWVP, false );
            break;
        case CUSTOM_VFETCH:
            m_InstancedMesh.RenderCustomVFetch( m_matWVP );
            break;
        case BATCHED_VFETCH:
            m_dwNumInstsPerBatchRendered =
                m_InstancedMesh.RenderBatches( m_matWVP, m_nNumInstsPerBatch, true );
            break;
        case BATCHED_GPUCONST:
            m_dwNumInstsPerBatchRendered =
                m_InstancedMesh.RenderBatches( m_matWVP, m_nNumInstsPerBatch, false );
            break;
        case XPS:
            m_InstancedMesh.RenderUsingXPS( m_matWVP );
            break;
        case TESSELLATOR_VFETCH:
            m_dwNumInstsPerBatchRendered =
                m_InstancedMesh.RenderUsingTessellator( m_matWVP, m_nNumInstsPerBatch,
                                                        m_dwTessLevel, true );
            break;
        case TESSELLATOR_GPUCONST:
            m_dwNumInstsPerBatchRendered =
                m_InstancedMesh.RenderUsingTessellator( m_matWVP, m_nNumInstsPerBatch,
                                                        m_dwTessLevel, false );
            break;
    }

    m_fRenderTime = ( FLOAT )CPUTimer.GetElapsedTime();
    m_Timer.MarkFrame();

#ifndef _RELEASED3D
    m_pd3dDevice->QueryPerfCounters( m_pPerfCounterEnd[ m_dwFrameCount % 3 ],
                                     D3DPERFQUERY_WAITGPUIDLE );
#endif
}


//--------------------------------------------------------------------------------------
// Name: RenderOverlays()
// Desc: Show title, timers, and help.
//--------------------------------------------------------------------------------------
VOID Sample::RenderOverlays()
{
    PIXBeginNamedEvent( 0xFFFFFFFF, "Overlays" );
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"Instancing" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.SetScaleFactors( 0.8f, 0.8f );

        FLOAT fYPos = 30;
        WCHAR strText[100];
        m_Font.DrawText( 0, fYPos, 0xff00ffff, g_InstanceTypeName[ m_nInstancingMethod ],
                         ATGFONT_RIGHT );
        fYPos += 40;

        swprintf_s( strText, L"Mesh Instances: %u", m_nInstanceCount );
        m_Font.DrawText( 0, fYPos, 0xFF8080FF, strText, ATGFONT_RIGHT );
        fYPos += 20;

        swprintf_s( strText, L"Tessellation level: %u", m_dwTessLevel );
        m_Font.DrawText( 0, fYPos, 0xFF8080FF, strText, ATGFONT_RIGHT );
        fYPos += 20;

        if( m_nInstancingMethod == BATCHED_VFETCH ||
            m_nInstancingMethod == BATCHED_GPUCONST ||
            m_nInstancingMethod == TESSELLATOR_VFETCH ||
            m_nInstancingMethod == TESSELLATOR_GPUCONST )
        {
            swprintf_s( strText, L"Instances per batch rendered / requested: %d / %d",
                        m_dwNumInstsPerBatchRendered, m_nNumInstsPerBatch );
        }
        else
        {
            swprintf_s( strText, L"Instances per batch rendered / requested:  N/A" );
        }

        m_Font.DrawText( 0, fYPos, 0xFF8080FF, strText, ATGFONT_RIGHT );
        fYPos += 40;

        swprintf_s( strText, L"Size of render data: %u bytes",
                    m_InstancedMesh.GetRenderDataSize() );
        m_Font.DrawText( 0, fYPos, 0xFFFFFF80, strText, ATGFONT_RIGHT );
        fYPos += 20;

        swprintf_s( strText, L"Instanced vertices: %u",
                    ( m_dwTessLevel + 2 ) * ( m_dwTessLevel + 2 ) * m_nInstanceCount );
        m_Font.DrawText( 0, fYPos, 0xFFFFFF80, strText, ATGFONT_RIGHT );
        fYPos += 20;

        swprintf_s( strText, L"CPU update: %0.4f ms", m_fUpdateTime * 1000 );
        m_Font.DrawText( 0, fYPos, 0xFFFFFF80, strText, ATGFONT_RIGHT );
        fYPos += 20;

        if( m_nInstancingMethod == XPS )
        {
            // CPU time is bound by the time spent in the 4 XPS threads, so just take the average XPS time
            LARGE_INTEGER nPerfFreq;
            QueryPerformanceFrequency( &nPerfFreq );
            swprintf_s( strText, L"CPU render: %0.4f ms", ( (FLOAT)g_nXPStime_prevFrame / (nPerfFreq.QuadPart * 4.0f) ) * 1000.0f );
        }
        else
        {
            swprintf_s( strText, L"CPU render: %0.4f ms", m_fRenderTime * 1000.0f );
        }
        m_Font.DrawText( 0, fYPos, 0xFFFFFF80, strText, ATGFONT_RIGHT );
        fYPos += 20;

#ifdef _RELEASED3D
        m_Font.DrawText( 0, fYPos, 0xFF808080,
                         L"GPU render: not instrumented",
                         ATGFONT_RIGHT );
#else
        D3DPERFCOUNTER_VALUES StartValues;
        m_pPerfCounterStart[( m_dwFrameCount + 1 ) % 3]->GetValues( &StartValues, 0, NULL );
        D3DPERFCOUNTER_VALUES EndValues;
        m_pPerfCounterEnd[( m_dwFrameCount + 1 ) % 3]->GetValues( &EndValues, 0, NULL );

        // Subtract start values from end values
        UINT64* pStartValues = ( UINT64* )&StartValues;
        UINT64* pEndValues = ( UINT64* )&EndValues;
        const DWORD dwCount = sizeof( D3DPERFCOUNTER_VALUES ) / sizeof( UINT64 );
        for( DWORD i = 0; i < dwCount; ++i )
        {
            pEndValues[i] -= pStartValues[i];
        }

        // Display GPU time.  This only includes the time to render the instances
        swprintf_s( strText, L"GPU render: %0.4f ms",
                    ( FLOAT )EndValues.RBBM[0].QuadPart / 500000.0f );
        m_Font.DrawText( 0, fYPos, 0xFFFFFF80, strText, ATGFONT_RIGHT );
        fYPos += 20;
#endif

        m_Font.End();
    }
    PIXEndNamedEvent();
}
