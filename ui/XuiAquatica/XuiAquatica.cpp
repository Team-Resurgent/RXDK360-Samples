//--------------------------------------------------------------------------------------
// XuiAquatica.cpp
//
// The sample demonstrate how to integrate XUI controls in a game. 
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <stdio.h>
#include <assert.h>
#include <xaudio2.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgAudio.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>
#include <AtgSceneAll.h>
#include <AtgCollision.h>

#include "XuiAquaticaUi.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_1, L"Show menu" },
    { ATG::HELP_LEFT_STICK,   ATG::HELP_PLACEMENT_1, L"Move camera" },
    { ATG::HELP_RIGHT_STICK,  ATG::HELP_PLACEMENT_1, L"Rotate camera" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))

//--------------------------------------------------------------------------------------
// Globals variables and definitions
//--------------------------------------------------------------------------------------

const DWORD     g_dwWaterColor = D3DCOLOR_ARGB( 0xff, 0x00, 0x40, 0x80 );
const FLOAT g_fWaterColor[] =
{
    0.0f, 0.25f, 0.5f, 1.0f
};

const DWORD g_dwFogColors[] =
{
    D3DCOLOR_ARGB( 0xff, 183, 255, 0 ),
    D3DCOLOR_ARGB( 0xff, 255, 235, 13 ),
    D3DCOLOR_ARGB( 0xff, 255, 252, 156 ),
    D3DCOLOR_ARGB( 0xff, 172, 115, 199 ),
    D3DCOLOR_ARGB( 0xff, 235, 59, 59 ),
    D3DCOLOR_ARGB( 0xff, 65, 102, 165 )
};


const DWORD     FISH_TEXTURE_COUNT = 6;
const DWORD     FISH_FRAME_COUNT = 3;

static CHAR*    g_strFishTextures[] =
{
    "Shark1",
    "Shark2",
    "Turtle1",
    "Turtle2",
    "Ud1",
    "Ud2",
};

const FLOAT     COLLISION_RADIUS = 70.0f;
const FLOAT     FISH_RADIUS = 120.0f;

//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application,
               public IAquaticaUI
{
    // Frame timer
    ATG::Timer m_Timer;

    // Font for drawing text
    ATG::Font m_Font;
    ATG::Help m_Help;      // Display help
    ATG::PackedResource m_Resource;

    // Transform matrices
    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matViewCopy;
    XMMATRIX m_matProj;

    BOOL m_bDrawHelp;

    XMVECTOR m_vEyePt;            // Camera properties
    XMVECTOR m_vLookatDir;
    XMVECTOR m_vUp;
    XMVECTOR m_vVelocity;

    // Fish objects
    ATG::Mesh2          m_SharkMesh[FISH_FRAME_COUNT];
    ATG::Mesh2          m_TurtleMesh[FISH_FRAME_COUNT];
    ATG::Mesh2          m_UdMesh[FISH_FRAME_COUNT];

    // Available fish textures
    LPDIRECT3DTEXTURE9  m_pFishTextures[FISH_TEXTURE_COUNT];

    LPDIRECT3DVERTEXDECLARATION9 m_pFishVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9 m_pFishVertexShader;

    // Scene object
    ATG::Scene* m_pScene;
    LPDIRECT3DVERTEXDECLARATION9 m_pSceneVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9 m_pSceneVertexShader;

    // Water caustics
    LPDIRECT3DTEXTURE9  m_pCausticTextures[32];
    LPDIRECT3DTEXTURE9 m_pCurrentCausticTexture;

    LPDIRECT3DPIXELSHADER9 m_pPixelShader;

    // Audio related data
    XAUDIO2_BUFFER m_AudioBuffer;
    IXAudio2SourceVoice* m_pSourceVoice;

    // Collision
    XMVECTOR* m_pCollisionVertices;       // Collision vertex list
    XMVECTOR* m_pCollisionNormals;        // Collision normals list
    DWORD m_dwCollisionVertexCount;
    XMVECTOR m_vFishPosition;
    HANDLE m_hCollisionThread;
    HANDLE m_hNewFrame;                // New frame event
    ATG::Timer m_CollisionTimer;

    // UI related members
    FISH_TYPE m_nFishType;
    DWORD m_dwTextureIndex;
    DWORD m_dwFogColorIndex;
    LIGHTING_INTENSITY m_nLightingIntensity;
    double m_fFogDepth;
    double m_fControllerSensitivity;
    BOOL m_bControllerInversion;
    BOOL m_bMusicMuted;

    HRESULT             InitializeAudio();
    HRESULT             InitializeCollision();
    static DWORD WINAPI CollisionUpdateThread( LPVOID lpParameter );

public:
    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();

    // IAquaticaUI implementation
    FISH_TYPE           GetFishType();
    void                SetFishType( FISH_TYPE nFishType );
    DWORD               GetTextureIndex();
    void                SetTextureIndex( DWORD dwTextureIndex );
    DWORD               GetFogColorIndex();
    void                SetFogColorIndex( DWORD dwFogColorIndex );
    LIGHTING_INTENSITY  GetLightingIntensity();
    void                SetLightingIntensity( LIGHTING_INTENSITY nLightingIntensity );
    double              GetFogDepth();
    void                SetFogDepth( double fFogDepth );
    double              GetControllerSensitivity();
    void                SetControllerSensitivity( double fControllerSensitivity );
    BOOL                GetControllerInversion();
    void                SetControllerInversion( BOOL bControllerInversion );
    BOOL                GetMusicMutedState();
    void                SetMusicMutedState( BOOL bMusicMuted );
    IDirect3DDevice9* GetD3DDevice();
    void                RenderFishPreview( FISH_TYPE nFishType, DWORD dwTextureIndex, IDirect3DDevice9* pDevice );
    HRESULT             RenderScene();

    HRESULT             UpdatePreview( FISH_TYPE nFishType, IDirect3DDevice9* pDevice );
    void                SetConstants();
};


//--------------------------------------------------------------------------------------
// Global instance of the app
//--------------------------------------------------------------------------------------
Sample          g_atgApp;


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    ATG::GetVideoSettings( &g_atgApp.m_d3dpp.BackBufferWidth, &g_atgApp.m_d3dpp.BackBufferHeight );
    g_atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initial scene configuration
    m_nFishType = FISH_TYPE_SHARK;
    m_dwTextureIndex = 0;
    m_dwFogColorIndex = 5;
    m_nLightingIntensity = LIGHTING_MEDIUM;
    m_fFogDepth = 50.0;
    m_fControllerSensitivity = 50.0;
    m_bControllerInversion = FALSE;
    m_bMusicMuted = FALSE;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        ATG::FatalError( "Couldn't create font" );

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        ATG::FatalError( "Couldn't create help" );
    m_bDrawHelp = FALSE;

    // Create the textures resource
    if( FAILED( m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
        ATG::FatalError( "XuiAquatica: Couldn't create Resource.xpr" );

    for( DWORD dwFishTexture = 0; dwFishTexture < FISH_TEXTURE_COUNT; dwFishTexture++ )
        m_pFishTextures[dwFishTexture] = m_Resource.GetTexture( g_strFishTextures[dwFishTexture] );

    for( DWORD t = 0; t < 32; t++ )
    {
        CHAR strTextureName[80];
        sprintf_s( strTextureName, "WaterCaustic%02ld", t );
        m_pCausticTextures[t] = m_Resource.GetTexture( strTextureName );
    }

    if( FAILED( m_SharkMesh[0].Create( "game:\\Media\\Meshes\\shark1.xbg" ) ) )
        ATG::FatalError( "XuiAquatica: Couldn't create shark1.xbg" );

    if( FAILED( m_SharkMesh[1].Create( "game:\\Media\\Meshes\\shark2.xbg" ) ) )
        ATG::FatalError( "XuiAquatica: Couldn't create shark2.xbg" );

    if( FAILED( m_SharkMesh[2].Create( "game:\\Media\\Meshes\\shark3.xbg" ) ) )
        ATG::FatalError( "XuiAquatica: Couldn't create shark3.xbg" );

    if( FAILED( m_TurtleMesh[0].Create( "game:\\Media\\Meshes\\turtle1.xbg" ) ) )
        ATG::FatalError( "XuiAquatica: Couldn't create turtle1.xbg" );

    if( FAILED( m_TurtleMesh[1].Create( "game:\\Media\\Meshes\\turtle2.xbg" ) ) )
        ATG::FatalError( "XuiAquatica: Couldn't create turtle2.xbg" );

    if( FAILED( m_TurtleMesh[2].Create( "game:\\Media\\Meshes\\turtle3.xbg" ) ) )
        ATG::FatalError( "XuiAquatica: Couldn't create turtle3.xbg" );

    if( FAILED( m_UdMesh[0].Create( "game:\\Media\\Meshes\\ud1.xbg" ) ) )
        ATG::FatalError( "XuiAquatica: Couldn't create ud1.xbg" );

    if( FAILED( m_UdMesh[1].Create( "game:\\Media\\Meshes\\ud2.xbg" ) ) )
        ATG::FatalError( "XuiAquatica: Couldn't create ud2.xbg" );

    if( FAILED( m_UdMesh[2].Create( "game:\\Media\\Meshes\\ud3.xbg" ) ) )
        ATG::FatalError( "XuiAquatica: Couldn't create ud3.xbg" );

    // Create scene object.
    m_pScene = new ATG::Scene();
    m_pScene->GetResourceDatabase()->AddBundledResources( &m_Resource );
    if( FAILED( ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\terrain.xatg", m_pScene, NULL,
                                                    ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL ) ) )
        ATG::FatalError( "Could not load scene file." );

    // Build the vertex declaration for the fish
    D3DVERTEXELEMENT9 declFish[MAXD3DDECLLENGTH] =
    {
        0
    };
    ATG::AppendVertexElements( declFish, 0, m_SharkMesh[0].GetMesh()->m_VertexElements, 0 );
    ATG::AppendVertexElements( declFish, 1, m_SharkMesh[1].GetMesh()->m_VertexElements, 1 );
    ATG::AppendVertexElements( declFish, 2, m_SharkMesh[2].GetMesh()->m_VertexElements, 2 );

    HRESULT hr;
    if( FAILED( hr = m_pd3dDevice->CreateVertexDeclaration(
                declFish, &m_pFishVertexDeclaration ) ) )
        return hr;

    VOID* pCode = NULL;
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\ShadeFishVertex.xvu", &pCode ) ) )
        ATG::FatalError( "XuiAquatica: Couldn't create ShadeFishVertex.xvu" );

    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pFishVertexShader ) ) )
        ATG::FatalError( "XuiAquatica: Couldn't create ShadeFishVertex.xvu" );
    ATG::UnloadFile( pCode );

    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\ShadeSceneVertex.xvu", &pCode ) ) )
        ATG::FatalError( "XuiAquatica: Couldn't create ShadeSceneVertex.xvu" );

    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pSceneVertexShader ) ) )
        ATG::FatalError( "Could not create a vertex shader" );
    ATG::UnloadFile( pCode );

    // Create the common pixel shader
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\ShadeCausticsPixel.xpu", &pCode ) ) )
        ATG::FatalError( "XuiAquatica: Couldn't create ShadeCausticsPixel.xpu" );

    if( FAILED( hr = m_pd3dDevice->CreatePixelShader( ( DWORD* )pCode, &m_pPixelShader ) ) )
        return hr;
    ATG::UnloadFile( pCode );

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the transform matrices
    m_vEyePt = XMVectorSet( 800.0f, 0.0f, -1500.0f, 0.0f );
    m_vLookatDir = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
    m_vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matWorld = XMMatrixIdentity();
    m_matView = XMMatrixLookAtLH( m_vEyePt, m_vEyePt + m_vLookatDir, m_vUp );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, 1.0f, 10000.0f );
    m_vVelocity = XMVectorSet( 0, 0, 0, 0 );
    m_matViewCopy = m_matView;

    // Initialize audio
    if( FAILED( hr = InitializeAudio() ) )
        ATG::FatalError( "Failed to initialize audio" );

    // Initialize the collision system
    if( FAILED( hr = InitializeCollision() ) )
        ATG::FatalError( "Failed to initialize collision" );

    // Initialize the UI
    if( FAILED( hr = InitUI( &g_atgApp ) ) )
        ATG::FatalError( "Failed initializing XUI" );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitializeAudio
// Desc: Initialize audio and start playing background music.
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeAudio()
{
    HRESULT hr = S_OK;

    // Initialize XAudio2
    IXAudio2* pXAudio2 = NULL;
    UINT32 flags = 0;

#ifdef _DEBUG
    flags |= XAUDIO2_DEBUG_ENGINE;
#endif

    if( FAILED( hr = XAudio2Create( &pXAudio2, flags ) ) )
        ATG::FatalError( "Error %#X calling XAudio2Create\n", hr );

    // Create a mastering voice
    IXAudio2MasteringVoice* pMasteringVoice = NULL;
    if( FAILED( hr = pXAudio2->CreateMasteringVoice( &pMasteringVoice ) ) )
        ATG::FatalError( "Error %#X calling CreateMasteringVoice\n", hr );

    // Load WAV file
    ATG::WaveFile XmaFile;
    if( FAILED( hr = XmaFile.Open( "game:\\Media\\Sounds\\XENON_Light.xma" ) ) )
        ATG::FatalError( "Error %#X opening WAV file\n", hr );

    // Read the format header
    WAVEFORMATEXTENSIBLE wfx = {0};
    XMA2WAVEFORMATEX xma2 = {0};
    if( FAILED( hr = XmaFile.GetFormat( &wfx, &xma2 ) ) )
        ATG::FatalError( "Error %#X reading WAV format\n", hr );

    if( wfx.Format.wFormatTag != WAVE_FORMAT_XMA2 )
        ATG::FatalError( "Error - Expected an XMA2 XAudio2 compatible file\n" );

    // Calculate how many bytes and samples are in the wave
    DWORD cbWaveSize = 0;
    XmaFile.GetDuration( &cbWaveSize );

    // Read the sample data into memory (XMA packets must be 2K aligned)
    BYTE* pbWaveData = ( BYTE* )XPhysicalAlloc( cbWaveSize, MAXULONG_PTR, 2048, PAGE_READWRITE );
    if( FAILED( hr = XmaFile.ReadSample( 0, pbWaveData, cbWaveSize, &cbWaveSize ) ) )
        ATG::FatalError( "Error %#X reading WAV data\n", hr );

    // Create a source voice to play the wave
    if( FAILED( hr = pXAudio2->CreateSourceVoice( &m_pSourceVoice, ( WAVEFORMATEX* )&xma2 ) ) )
        ATG::FatalError( "Error %#X creating source voice\n", hr );

    // submit the wave sample data using an XAUDIO2_BUFFER structure
    m_AudioBuffer.pAudioData = pbWaveData;
    m_AudioBuffer.Flags = XAUDIO2_END_OF_STREAM;  // tell the source voice not to expect any data after this buffer
    m_AudioBuffer.AudioBytes = cbWaveSize;
    m_AudioBuffer.LoopCount = XAUDIO2_LOOP_INFINITE;

    if( FAILED( hr = m_pSourceVoice->SubmitSourceBuffer( &m_AudioBuffer ) ) )
        ATG::FatalError( "Error %#X submitting source buffer\n", hr );

    // Play the source voice
    if( FAILED( hr = m_pSourceVoice->Start( 0 ) ) )
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitializeCollision
// Desc: Initialize collision mesh and thread.
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeCollision()
{
    m_vFishPosition = XMVectorSet( 0, 0, 0, 0 );

    // Build XMVECTOR vertex collision array
    m_dwCollisionVertexCount = 0;
    ATG::NameIndexedCollection::iterator i;

    // Count vertices
    for( i = m_pScene->GetInstanceList()->begin(); i != m_pScene->GetInstanceList()->end(); i++ )
    {
        // Select models from the object list.
        if( ( *i )->IsDerivedFrom( ATG::Model::TypeID ) )
        {
            ATG::Model* pModel = ( ATG::Model* )( *i );

            // Loop over mesh mappings.
            DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
            for( DWORD dwMapIndex = 0; dwMapIndex < dwMeshMappingCount; ++dwMapIndex )
            {
                ATG::MeshMapping& mm = pModel->GetMeshMapping( dwMapIndex );
                ATG::BaseMesh* pMesh = mm.pMesh;

                m_dwCollisionVertexCount += pMesh->GetVertexData( 0 )->GetNumVertices();
            }
        }
    }

    // Build XMVECTOR array
    m_pCollisionVertices = new XMVECTOR[m_dwCollisionVertexCount];
    if( !m_pCollisionVertices )
        return E_FAIL;

    m_pCollisionNormals = new XMVECTOR[m_dwCollisionVertexCount];
    if( !m_pCollisionNormals )
        return E_FAIL;


    DWORD dwVertexIndex = 0;
    for( i = m_pScene->GetInstanceList()->begin(); i != m_pScene->GetInstanceList()->end(); i++ )
    {
        // Select models from the object list.
        if( ( *i )->IsDerivedFrom( ATG::Model::TypeID ) )
        {
            ATG::Model* pModel = ( ATG::Model* )( *i );

            // Loop over mesh mappings.
            DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
            for( DWORD dwMapIndex = 0; dwMapIndex < dwMeshMappingCount; ++dwMapIndex )
            {
                ATG::MeshMapping& mm = pModel->GetMeshMapping( dwMapIndex );
                ATG::BaseMesh* pMesh = mm.pMesh;

                LPDIRECT3DVERTEXBUFFER9 pVertexBuffer = pMesh->GetVertexData( 0 )->GetVertexStream( 0 )->pVertexBuffer;
                DWORD dwStride = pMesh->GetVertexData( 0 )->GetVertexStream( 0 )->Stride;
                DWORD dwVertexCount = pMesh->GetVertexData( 0 )->GetNumVertices();

                BYTE* pVertices = NULL;
                pVertexBuffer->Lock( 0, 0, ( VOID** )&pVertices, D3DLOCK_READONLY );
                for( DWORD idx = 0; idx < dwVertexCount; ++idx )
                {
                    m_pCollisionVertices[dwVertexIndex].x = ( ( XMFLOAT3* )pVertices )->x;
                    m_pCollisionVertices[dwVertexIndex].y = ( ( XMFLOAT3* )pVertices )->y;
                    m_pCollisionVertices[dwVertexIndex].z = ( ( XMFLOAT3* )pVertices )->z;
                    m_pCollisionVertices[dwVertexIndex].w = 0.0f;

                    m_pCollisionVertices[dwVertexIndex] = XMVector4Transform( m_pCollisionVertices[dwVertexIndex],
                                                                              pModel->GetWorldTransform() );

                    m_pCollisionNormals[dwVertexIndex] = XMLoadDecN4( ( XMDECN4* )( pVertices + 12 ) );

                    dwVertexIndex++;
                    pVertices += dwStride;
                }
                pVertexBuffer->Unlock();
            }
        }
    }

    // Create new frame event
    m_hNewFrame = CreateEvent( NULL, TRUE, FALSE, NULL );
    if( !m_hNewFrame )
        return E_FAIL;

    // Create collision thread
    m_hCollisionThread = CreateThread( NULL, 0, CollisionUpdateThread, this, CREATE_SUSPENDED, NULL );
    if( !m_hCollisionThread )
        return E_FAIL;

    XSetThreadProcessor( m_hCollisionThread, 2 );

    ResumeThread( m_hCollisionThread );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();
    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;


    // Set the view matrix for camera control
    static FLOAT fTheta = -0.05f * XM_PI;
    static FLOAT fPhi = +0.0f * XM_PI;

    // Allow camera control only when UI is inactive
    if( !IsUIActive() )
    {
        fPhi += pGamepad->fX2 * fElapsedTime * ( ( FLOAT )m_fControllerSensitivity / 100.0f ) * XM_PI;
        fTheta += ( m_bControllerInversion ? -1.0f : 1.0f ) * pGamepad->fY2 * fElapsedTime *
            ( ( FLOAT )m_fControllerSensitivity / 100.0f ) * XM_PI;
    }

    m_vLookatDir.x = cosf( fTheta ) * sinf( fPhi );
    m_vLookatDir.y = sinf( fTheta );
    m_vLookatDir.z = cosf( fTheta ) * cosf( fPhi );

    XMVECTOR vCrossDir = XMVectorSet( cosf( fPhi ), 0, -sinf( fPhi ), 0 );

    m_vVelocity = XMVectorSet( 0, 0, 0, 0 );
    if( !IsUIActive() )
    {
        m_vVelocity = ( m_vLookatDir * pGamepad->fY1 * 10.0f ) + ( vCrossDir * pGamepad->fX1 * 10.0f );
    }

    m_matView = m_matViewCopy;

    // Signal the collision thread that this is a new frame
    SetEvent( m_hNewFrame );

    // Retrieve and dispatch input to the UI
    XINPUT_KEYSTROKE keyStroke;

    XInputGetKeystroke( XUSER_INDEX_ANY, XINPUT_FLAG_ANYDEVICE, &keyStroke );
    DispatchXuiInput( &keyStroke );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CollisionUpdateThread
// Desc: Perform simple collision detection on the whole scene.
//--------------------------------------------------------------------------------------
DWORD WINAPI Sample::CollisionUpdateThread( LPVOID lpParameter )
{
    // Give this thread a name
    ATG::SetThreadName( GetCurrentThreadId(), "CollisionUpdateThread" );

    Sample* pSample = ( Sample* )lpParameter;

    for(; ; )
    {
        if( WaitForSingleObject( pSample->m_hNewFrame, INFINITE ) != WAIT_OBJECT_0 )
            return 0;

        FLOAT fElapsedTime = ( FLOAT )pSample->m_CollisionTimer.GetElapsedTime();
        pSample->m_vVelocity *= fElapsedTime;

        BOOL bCollision = FALSE;

        // Perform sphere to sphere collision detection with scene vertices
        XMVECTOR vNormal = XMVectorSet( 0, 0, 0, 0 );
        FLOAT fMinRadius = COLLISION_RADIUS * COLLISION_RADIUS;
        XMVECTOR* pCollisionVertices = pSample->m_pCollisionVertices;
        XMVECTOR vEyePt = pSample->m_vEyePt;
        for( DWORD i = 0; i < pSample->m_dwCollisionVertexCount; i++ )
        {
            XMVECTOR vDiff = __vsubfp( vEyePt, pCollisionVertices[i] );
            vDiff = __vmsum3fp( vDiff, vDiff );

            if( vDiff.x < fMinRadius )
            {
                fMinRadius = vDiff.x;
                vNormal = pSample->m_pCollisionNormals[i];
                bCollision = TRUE;
            }
        }

        // Test collision with the fish
        XMVECTOR vDist = XMVectorSubtract( pSample->m_vEyePt, pSample->m_vFishPosition );
        XMVECTOR vLength = XMVector3Length( vDist );
        if( vLength.x < ( COLLISION_RADIUS + FISH_RADIUS ) )
        {
            fMinRadius = pow( vLength.x - FISH_RADIUS, 2.0f );
            vNormal = XMVector3Normalize( vDist );
            bCollision = TRUE;
        }

        // Move camera away from the colliding vertex plane
        if( bCollision )
        {
            pSample->m_vEyePt += vNormal * ( COLLISION_RADIUS - sqrt( fMinRadius ) );
        }

        pSample->m_vEyePt += pSample->m_vVelocity;

        XMVECTOR vMovement = XMVectorSet( 0, cosf( ( FLOAT )pSample->m_Timer.GetAppTime() ) / 50, 0, 0 );
        pSample->m_matViewCopy = XMMatrixLookAtLH( pSample->m_vEyePt + vMovement,
                                                   pSample->m_vEyePt + pSample->m_vLookatDir, pSample->m_vUp );

        ResetEvent( pSample->m_hNewFrame );
    }
}


//--------------------------------------------------------------------------------------
// Name: SetConstants
// Desc: Set pixel and vertex shader constants for rendering the background scene.
//--------------------------------------------------------------------------------------
void Sample::SetConstants()
{
    FLOAT fTime = ( FLOAT )m_Timer.GetAppTime();

    // Animate the caustic textures
    DWORD tex = ( ( DWORD )( fTime * 32 ) ) % 32;
    m_pCurrentCausticTexture = m_pCausticTextures[ tex ];

    // Animation attributes for the fish
    FLOAT fKickFreq = 2 * fTime;
    FLOAT fPhase = fTime / 3;
    FLOAT fBlendWeight1 = sinf( fKickFreq );
    FLOAT fBlendWeight2 = sinf( fKickFreq + D3DX_PI );

    // Move the fish in a circle
    XMMATRIX matFish, matTrans, matRotate1, matRotate2;

    XMVECTOR vWeight =
    {
        0
    };

    switch( m_nFishType )
    {
        case FISH_TYPE_SHARK:
        {
            matFish = XMMatrixScaling( 0.5f, 0.5f, 0.5f );
            matRotate1 = XMMatrixRotationZ( -cosf( fKickFreq ) / 6 );
            matFish = XMMatrixMultiply( matFish, matRotate1 );
            matRotate2 = XMMatrixRotationY( fPhase - ( D3DX_PI / 2 ) );
            matFish = XMMatrixMultiply( matFish, matRotate2 );
            m_vFishPosition = XMVectorSet( 1300.0f - 1000.0f * sinf( fPhase ), 250.0f,
                                           300.0f - 1000.0f * cosf( fPhase ), 0 );
            matTrans = XMMatrixTranslation( m_vFishPosition.x, m_vFishPosition.y, m_vFishPosition.z );
            matFish = XMMatrixMultiply( matFish, matTrans );
            vWeight.x = ( fBlendWeight1 + 1.0f ) / 2.0f;
            vWeight.y = 0.0f;
            vWeight.z = ( fBlendWeight2 + 1.0f ) / 2.0f;
        }
            break;

        case FISH_TYPE_TURTLE:
        {
            matFish = XMMatrixScaling( 1.5f, 1.5f, 1.5f );
            matRotate1 = XMMatrixRotationZ( D3DX_PI );
            matFish = XMMatrixMultiply( matFish, matRotate1 );
            matRotate2 = XMMatrixRotationY( fPhase + D3DX_PI );
            matFish = XMMatrixMultiply( matFish, matRotate2 );
            m_vFishPosition = XMVectorSet( 1300.0f - 1000.0f * sinf( fPhase ), 250.0f + 20.0f * cosf( fKickFreq ),
                                           300.0f - 1000.0f * cosf( fPhase ), 0 );
            matTrans = XMMatrixTranslation( m_vFishPosition.x, m_vFishPosition.y, m_vFishPosition.z );
            matFish = XMMatrixMultiply( matFish, matTrans );
            if( fBlendWeight1 > 0.0f )
            {
                vWeight.x = fabsf( fBlendWeight1 );
                vWeight.y = 1.0f - fabsf( fBlendWeight1 );
                vWeight.z = 0.0f;
            }
            else
            {
                vWeight.x = 0.0f;
                vWeight.y = 1.0f - fabsf( fBlendWeight1 );
                vWeight.z = fabsf( fBlendWeight1 );
            }
        }
            break;

        case FISH_TYPE_UD:
        {
            matFish = XMMatrixScaling( 2.0f, 2.0f, 2.0f );
            matRotate1 = XMMatrixRotationZ( -cosf( fKickFreq ) / 6 );
            matFish = XMMatrixMultiply( matFish, matRotate1 );
            matRotate2 = XMMatrixRotationY( fPhase - ( D3DX_PI / 2 ) );
            matFish = XMMatrixMultiply( matFish, matRotate2 );
            m_vFishPosition = XMVectorSet( 1300.0f - 1000.0f * sinf( fPhase ), 250.0f,
                                           300.0f - 1000.0f * cosf( fPhase ), 0 );
            matTrans = XMMatrixTranslation( m_vFishPosition.x, m_vFishPosition.y, m_vFishPosition.z );
            matFish = XMMatrixMultiply( matFish, matTrans );
            vWeight.x = ( fBlendWeight1 + 1.0f ) / 2.0f;
            vWeight.y = 0.0f;
            vWeight.z = ( fBlendWeight2 + 1.0f ) / 2.0f;
        }
            break;
    }

    // Set the vertex shader constants. 
    static XMFLOAT4 vZero( 0.0f, 0.0f, 0.0f, 0.0f );
    static XMFLOAT4 vConstants( 1.0f, 0.5f, 1.0f, 0.05f );

    // Lighting vectors (in world space and in model space)
    // and other constants
    XMVECTOR vLight = XMVectorSet( 0.00f, 0.80f, 0.60f, 0.00f );
    XMVECTOR vLightFishSpace = XMVectorSet( 0.00f, 0.80f, 0.60f, 0.00f );
    XMVECTOR vDiffuse = XMVectorSet( 1.00f, 1.00f, 1.00f, 1.00f );
    XMVECTOR vAmbient = XMVectorSet( 0.25f, 0.25f, 0.25f, 0.25f );
    XMVECTOR vFog = XMVectorSet( 0.50f, ( ( FLOAT )m_fFogDepth * 40.0f ),
                                 1.0f / ( ( ( FLOAT )m_fFogDepth * 40.0f ) - 40.0f ), 0.00f );

    XMVECTOR vDeterminant;
    XMMATRIX matFishInv = XMMatrixInverse( &vDeterminant, matFish );
    vLightFishSpace = XMVector4Normalize( XMVector4Transform( vLight, matFishInv ) );

    // Vertex shader operations use transposed matrices
    XMMATRIX mat, matCamera, matTranspose, matCameraTranspose;
    XMMATRIX matViewTranspose, matProjTranspose;
    matCamera = XMMatrixMultiply( matFish, m_matView );
    mat = XMMatrixMultiply( matCamera, m_matProj );
    matTranspose = XMMatrixTranspose( mat );
    matCameraTranspose = XMMatrixTranspose( matCamera );
    matViewTranspose = XMMatrixTranspose( m_matView );
    matProjTranspose = XMMatrixTranspose( m_matProj );
    XMMATRIX matModelTranspose = XMMatrixTranspose( matFish );

    // Set the vertex shader constants
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&vZero, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 1, ( FLOAT* )&vConstants, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 2, ( FLOAT* )&vWeight, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matTranspose, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&matCameraTranspose, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 12, ( FLOAT* )&matViewTranspose, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 16, ( FLOAT* )&matProjTranspose, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 20, ( FLOAT* )&vLight, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 21, ( FLOAT* )&vLightFishSpace, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 22, ( FLOAT* )&vDiffuse, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 23, ( FLOAT* )&vAmbient, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 24, ( FLOAT* )&vFog, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 25, ( FLOAT* )&matModelTranspose, 4 );
}

//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Called once per frame, the call is the entry point for 3D rendering. This
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    UpdateUI();

    HRESULT hr = RenderScene();
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    return hr;
}


//--------------------------------------------------------------------------------------
// Name: RenderScene
// Desc: Render background scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderScene()
{
    SetConstants();

    // Clear the viewport
    IDirect3DSurface9* pRenderTarget = NULL;
    m_pd3dDevice->GetRenderTarget( 0, &pRenderTarget );
    if( !pRenderTarget )
        return S_OK;

    D3DSURFACE_DESC desc;
    pRenderTarget->GetDesc( &desc );
    D3DRECT rctClear;
    rctClear.x1 = 0;
    rctClear.x2 = desc.Width;
    rctClear.y1 = 0;
    rctClear.y2 = desc.Height;

    pRenderTarget->Release();

    m_pd3dDevice->Clear( 1L, &rctClear, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                         g_dwFogColors[m_dwFogColorIndex], 1.0f, 0L );


    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

    // Initialize default device states at the start of the frame
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    // Set the common pixel shader
    m_pd3dDevice->SetPixelShader( m_pPixelShader );

    // Set light intensity and fog color
    FLOAT fAmbientValue = 0.0f;
    if( m_nLightingIntensity == LIGHTING_LOW )
        fAmbientValue = 0.1f;
    else if( m_nLightingIntensity == LIGHTING_MEDIUM )
        fAmbientValue = 0.25f;
    else if( m_nLightingIntensity == LIGHTING_HIGH )
        fAmbientValue = 0.75f;

    FLOAT fAmbient[4] =
    {
        fAmbientValue, fAmbientValue, fAmbientValue, fAmbientValue
    };

    FLOAT fWaterColor[4];
    fWaterColor[0] = ( FLOAT )( ( g_dwFogColors[m_dwFogColorIndex] >> 16 ) & 0xff ) / 255.0f;
    fWaterColor[1] = ( FLOAT )( ( g_dwFogColors[m_dwFogColorIndex] >> 8 ) & 0xff ) / 255.0f;
    fWaterColor[2] = ( FLOAT )( g_dwFogColors[m_dwFogColorIndex] & 0xff ) / 255.0f;
    fWaterColor[3] = 1.0f;
    m_pd3dDevice->SetPixelShaderConstantF( 0, fWaterColor, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 1, fAmbient, 1 );

    XMVECTOR vLight = XMVectorSet( 0.00f, 0.80f, 0.60f, 0.00f );
    m_pd3dDevice->SetPixelShaderConstantF( 2, ( FLOAT* )&vLight, 1 );

    // Enable normal mapping
    FLOAT fEnableNormalMap = 1.0f;
    m_pd3dDevice->SetPixelShaderConstantF( 3, &fEnableNormalMap, 1 );

    // Render the scene
    m_pd3dDevice->SetVertexShader( m_pSceneVertexShader );
    m_pd3dDevice->SetTexture( 1, m_pCurrentCausticTexture );

    // Render opaque objects
    ATG::NameIndexedCollection::iterator i;
    for( i = m_pScene->GetInstanceList()->begin(); i != m_pScene->GetInstanceList()->end(); i++ )
    {
        // Select models from the object list.
        if( ( *i )->IsDerivedFrom( ATG::Model::TypeID ) )
        {
            ATG::Model* pModel = ( ATG::Model* )( *i );

            // Transform the model to world coordinates
            XMMATRIX matViewTranspose = XMMatrixTranspose( pModel->GetWorldTransform() * m_matView );
            m_pd3dDevice->SetVertexShaderConstantF( 12, ( FLOAT* )&matViewTranspose, 4 );

            // Loop over mesh mappings.
            DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
            for( DWORD dwMapIndex = 0; dwMapIndex < dwMeshMappingCount; ++dwMapIndex )
            {
                ATG::MeshMapping& mm = pModel->GetMeshMapping( dwMapIndex );
                ATG::BaseMesh* pMesh = mm.pMesh;

                // Loop over mesh subsets.
                DWORD dwSubsetCount = pMesh->GetNumSubsets();
                for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
                {
                    ATG::MaterialInstance* pMaterial = mm.Materials[ dwSubsetIndex ];
                    if( !pMaterial->IsTransparent() )
                    {
                        // Retrieve diffuse texture name
                        ATG::MaterialParameter& param = pMaterial->GetRawParameter( 0 );

                        if( param.pValue != NULL )
                        {
                            ATG::Texture2D* pTex2D = ( ATG::Texture2D* )param.pValue;
                            m_pd3dDevice->SetTexture( 0, pTex2D->GetD3DTexture() );
                        }

                        // Retreive normal map
                        if( pMaterial->GetRawParameterCount() >= 2 )
                        {
                            ATG::MaterialParameter& prm = pMaterial->GetRawParameter( 1 );

                            if( prm.pValue != NULL )
                            {
                                ATG::Texture2D* pTex2D = ( ATG::Texture2D* )prm.pValue;
                                m_pd3dDevice->SetTexture( 2, pTex2D->GetD3DTexture() );
                                fEnableNormalMap = 1.0f;
                            }
                        }

                        // Render the mesh subset.
                        pMesh->RenderSubset( dwSubsetIndex, m_pd3dDevice );
                    }
                }
            }
        }
    }

    // Render transparent objects
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    for( i = m_pScene->GetInstanceList()->begin(); i != m_pScene->GetInstanceList()->end(); i++ )
    {
        // Select models from the object list.
        if( ( *i )->IsDerivedFrom( ATG::Model::TypeID ) )
        {
            ATG::Model* pModel = ( ATG::Model* )( *i );

            // Transform the model to world coordinates
            XMMATRIX matViewTranspose = XMMatrixTranspose( pModel->GetWorldTransform() * m_matView );
            m_pd3dDevice->SetVertexShaderConstantF( 12, ( FLOAT* )&matViewTranspose, 4 );

            // Loop over mesh mappings.
            DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
            for( DWORD dwMapIndex = 0; dwMapIndex < dwMeshMappingCount; ++dwMapIndex )
            {
                ATG::MeshMapping& mm = pModel->GetMeshMapping( dwMapIndex );
                ATG::BaseMesh* pMesh = mm.pMesh;

                // Loop over mesh subsets.
                DWORD dwSubsetCount = pMesh->GetNumSubsets();
                for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
                {
                    ATG::MaterialInstance* pMaterial = mm.Materials[ dwSubsetIndex ];
                    if( pMaterial->IsTransparent() )
                    {
                        // Retrieve diffuse texture name
                        ATG::MaterialParameter& param = pMaterial->GetRawParameter( 0 );

                        if( param.pValue != NULL )
                        {
                            ATG::Texture2D* pTex2D = ( ATG::Texture2D* )param.pValue;
                            m_pd3dDevice->SetTexture( 0, pTex2D->GetD3DTexture() );
                        }

                        // Retreive normal map
                        if( pMaterial->GetRawParameterCount() >= 2 )
                        {
                            ATG::MaterialParameter& prm = pMaterial->GetRawParameter( 1 );

                            if( prm.pValue != NULL )
                            {
                                ATG::Texture2D* pTex2D = ( ATG::Texture2D* )prm.pValue;
                                m_pd3dDevice->SetTexture( 2, pTex2D->GetD3DTexture() );
                            }
                        }

                        // Render the mesh subset.
                        pMesh->RenderSubset( dwSubsetIndex, m_pd3dDevice );
                    }
                }
            }
        }
    }

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // Render the fish (disable normal mapping)
    fEnableNormalMap = 0.0f;
    m_pd3dDevice->SetPixelShaderConstantF( 3, &fEnableNormalMap, 1 );
    m_pd3dDevice->SetTexture( 0, m_pFishTextures[m_dwTextureIndex] );
    m_pd3dDevice->SetTexture( 1, m_pCurrentCausticTexture );
    m_pd3dDevice->SetVertexDeclaration( m_pFishVertexDeclaration );
    m_pd3dDevice->SetVertexShader( m_pFishVertexShader );

    switch( m_nFishType )
    {
        case FISH_TYPE_SHARK:
        {
            for( DWORD dwFrame = 0; dwFrame < FISH_FRAME_COUNT; ++dwFrame )
                m_pd3dDevice->SetStreamSource( dwFrame, &m_SharkMesh[dwFrame].GetMesh()->m_VB, 0,
                                               m_SharkMesh[dwFrame].GetMesh()->m_dwVertexSize );

            m_pd3dDevice->SetIndices( &m_SharkMesh[0].GetMesh()->m_IB );
            m_pd3dDevice->DrawIndexedPrimitive( m_SharkMesh[0].GetMesh()->m_dwPrimType, 0,
                                                0, m_SharkMesh[0].GetMesh()->m_pSubsets[0].dwVertexCount,
                                                0, m_SharkMesh[0].GetMesh()->m_pSubsets[0].dwPrimitiveCount );
        }
            break;

        case FISH_TYPE_TURTLE:
        {
            for( DWORD dwFrame = 0; dwFrame < FISH_FRAME_COUNT; ++dwFrame )
                m_pd3dDevice->SetStreamSource( dwFrame, &m_TurtleMesh[dwFrame].GetMesh()->m_VB, 0,
                                               m_TurtleMesh[dwFrame].GetMesh()->m_dwVertexSize );

            m_pd3dDevice->SetIndices( &m_TurtleMesh[0].GetMesh()->m_IB );
            m_pd3dDevice->DrawIndexedPrimitive( m_TurtleMesh[0].GetMesh()->m_dwPrimType, 0,
                                                0, m_TurtleMesh[0].GetMesh()->m_pSubsets[0].dwVertexCount,
                                                0, m_TurtleMesh[0].GetMesh()->m_pSubsets[0].dwPrimitiveCount );
        }
            break;

        case FISH_TYPE_UD:
        {
            for( DWORD dwFrame = 0; dwFrame < FISH_FRAME_COUNT; ++dwFrame )
                m_pd3dDevice->SetStreamSource( dwFrame, &m_UdMesh[dwFrame].GetMesh()->m_VB, 0,
                                               m_UdMesh[dwFrame].GetMesh()->m_dwVertexSize );

            m_pd3dDevice->SetIndices( &m_UdMesh[0].GetMesh()->m_IB );
            m_pd3dDevice->DrawIndexedPrimitive( m_UdMesh[0].GetMesh()->m_dwPrimType, 0,
                                                0, m_UdMesh[0].GetMesh()->m_pSubsets[0].dwVertexCount,
                                                0, m_UdMesh[0].GetMesh()->m_pSubsets[0].dwPrimitiveCount );
        }
            break;
    }

    // Render UI
    RenderUI( m_pd3dDevice, desc.Width, desc.Height );

    // Output statistics
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"XuiAquatica" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// UI implementation
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: GetApp
// Desc: Helper to return UI accessible interface for the application
//--------------------------------------------------------------------------------------
IAquaticaUI* GetApp()
{
    return &g_atgApp;
}


//--------------------------------------------------------------------------------------
// Name: GetFishType
// Desc: Called by the UI to retrieve the current fish type.
//--------------------------------------------------------------------------------------
FISH_TYPE Sample::GetFishType()
{
    return m_nFishType;
}


//--------------------------------------------------------------------------------------
// Name: GetFishType
// Desc: Called by the UI to set the current fish type.
//--------------------------------------------------------------------------------------
void Sample::SetFishType( FISH_TYPE nFishType )
{
    m_nFishType = nFishType;
    switch( m_nFishType )
    {
        case FISH_TYPE_SHARK:
            m_dwTextureIndex = 0;
            break;

        case FISH_TYPE_TURTLE:
            m_dwTextureIndex = 2;
            break;

        case FISH_TYPE_UD:
            m_dwTextureIndex = 4;
            break;
    }
}


//--------------------------------------------------------------------------------------
// Name: GetTextureIndex
// Desc: Called by the UI to get the current fish texture.
//--------------------------------------------------------------------------------------
DWORD Sample::GetTextureIndex()
{
    return m_dwTextureIndex;
}


//--------------------------------------------------------------------------------------
// Name: GetTextureIndex
// Desc: Called by the UI to set the current fish texture.
//--------------------------------------------------------------------------------------
void Sample::SetTextureIndex( DWORD dwTextureIndex )
{
    m_dwTextureIndex = dwTextureIndex;
}


//--------------------------------------------------------------------------------------
// Name: GetFogColorIndex
// Desc: Called by the UI to get the fog color
//--------------------------------------------------------------------------------------
DWORD Sample::GetFogColorIndex()
{
    return m_dwFogColorIndex;
}


//--------------------------------------------------------------------------------------
// Name: SetFogColorIndex
// Desc: Called by the UI to set the fog color
//--------------------------------------------------------------------------------------
void Sample::SetFogColorIndex( DWORD dwFogColorIndex )
{
    m_dwFogColorIndex = dwFogColorIndex;
}


//--------------------------------------------------------------------------------------
// Name: GetLightingIntensity
// Desc: Called by the UI to get the lighting intensity
//--------------------------------------------------------------------------------------
LIGHTING_INTENSITY Sample::GetLightingIntensity()
{
    return m_nLightingIntensity;
}


//--------------------------------------------------------------------------------------
// Name: SetLightingIntensity
// Desc: Called by the UI to set the lighting intensity
//--------------------------------------------------------------------------------------
void Sample::SetLightingIntensity( LIGHTING_INTENSITY nLightingIntensity )
{
    m_nLightingIntensity = nLightingIntensity;
}


//--------------------------------------------------------------------------------------
// Name: GetFogDepth
// Desc: Called by the UI to get the fog depth. Range is 0..100.
//--------------------------------------------------------------------------------------
double Sample::GetFogDepth()
{
    return m_fFogDepth;
}


//--------------------------------------------------------------------------------------
// Name: SetFogDepth
// Desc: Called by the UI to set the fog depth. Range is 0..100.
//--------------------------------------------------------------------------------------
void Sample::SetFogDepth( double fFogDepth )
{
    m_fFogDepth = fFogDepth;
}


//--------------------------------------------------------------------------------------
// Name: GetControllerSensitivity
// Desc: Called by the UI to get the controller sensitivity. Range is 0..100.
//--------------------------------------------------------------------------------------
double Sample::GetControllerSensitivity()
{
    return m_fControllerSensitivity;
}


//--------------------------------------------------------------------------------------
// Name: SetControllerSensitivity
// Desc: Called by the UI to set the controller sensitivity. Range is 0..100.
//--------------------------------------------------------------------------------------
void Sample::SetControllerSensitivity( double fControllerSensitivity )
{
    m_fControllerSensitivity = fControllerSensitivity;
}


//--------------------------------------------------------------------------------------
// Name: GetControllerInversion
// Desc: Called by the UI to get the controller inversion.
//--------------------------------------------------------------------------------------
BOOL Sample::GetControllerInversion()
{
    return m_bControllerInversion;
}


//--------------------------------------------------------------------------------------
// Name: SetControllerInversion
// Desc: Called by the UI to set the controller inversion.
//--------------------------------------------------------------------------------------
void Sample::SetControllerInversion( BOOL bControllerInversion )
{
    m_bControllerInversion = bControllerInversion;
}


//--------------------------------------------------------------------------------------
// Name: GetMusicMutedState
// Desc: Called by the UI to get the music muted state.
//--------------------------------------------------------------------------------------
BOOL Sample::GetMusicMutedState()
{
    return m_bMusicMuted;
}


//--------------------------------------------------------------------------------------
// Name: SetMusicMutedState
// Desc: Called by the UI to set the music muted state.
//--------------------------------------------------------------------------------------
void Sample::SetMusicMutedState( BOOL bMusicMuted )
{
    m_bMusicMuted = bMusicMuted;
    m_pSourceVoice->SetVolume( m_bMusicMuted ? 0.0f : 1.0f , XAUDIO2_COMMIT_NOW );
}


//--------------------------------------------------------------------------------------
// Name: GetD3DDevice
// Desc: Called by the UI to retrieve the IDirect3DDevice9 owned by the application.
//--------------------------------------------------------------------------------------
IDirect3DDevice9* Sample::GetD3DDevice()
{
    return m_pd3dDevice;
}


//--------------------------------------------------------------------------------------
// Name: RenderFishPreview
// Desc: Called by the UI to render the specified fish preview to device
//--------------------------------------------------------------------------------------
void Sample::RenderFishPreview( FISH_TYPE nFishType, DWORD dwTextureIndex, IDirect3DDevice9* pDevice )
{
    UpdatePreview( nFishType, pDevice );

    D3DRECT rctClear;
    rctClear.x1 = 0;
    rctClear.x2 = 512;
    rctClear.y1 = 0;
    rctClear.y2 = 256;

    pDevice->Clear( 1, &rctClear, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                    D3DCOLOR_ARGB( 255, 0, 0, 0 ), 1, 0 );

    // Initialize default device states at the start of the frame
    pDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
    pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );
    pDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    pDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    pDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    // Set the common pixel shader
    static FLOAT fAmbient[] =
    {
        0.25f, 0.25f, 0.25f, 0.25f
    };
    pDevice->SetPixelShader( m_pPixelShader );
    pDevice->SetPixelShaderConstantF( 0, g_fWaterColor, 1 );
    pDevice->SetPixelShaderConstantF( 1, fAmbient, 1 );
    FLOAT fEnableNormalMap = 0.0f;
    pDevice->SetPixelShaderConstantF( 3, &fEnableNormalMap, 1 );

    // Render the fish
    pDevice->SetTexture( 0, m_pFishTextures[dwTextureIndex] );
    pDevice->SetTexture( 1, m_pCurrentCausticTexture );
    pDevice->SetVertexDeclaration( m_pFishVertexDeclaration );
    pDevice->SetVertexShader( m_pFishVertexShader );

    switch( nFishType )
    {
        case FISH_TYPE_SHARK:
        {
            for( DWORD dwFrame = 0; dwFrame < FISH_FRAME_COUNT; ++dwFrame )
                pDevice->SetStreamSource( dwFrame, &m_SharkMesh[dwFrame].GetMesh()->m_VB, 0,
                                          m_SharkMesh[dwFrame].GetMesh()->m_dwVertexSize );

            pDevice->SetIndices( &m_SharkMesh[0].GetMesh()->m_IB );
            pDevice->DrawIndexedPrimitive( m_SharkMesh[0].GetMesh()->m_dwPrimType, 0,
                                           0, m_SharkMesh[0].GetMesh()->m_pSubsets[0].dwVertexCount,
                                           0, m_SharkMesh[0].GetMesh()->m_pSubsets[0].dwPrimitiveCount );
        }
            break;

        case FISH_TYPE_TURTLE:
        {
            for( DWORD dwFrame = 0; dwFrame < FISH_FRAME_COUNT; ++dwFrame )
                pDevice->SetStreamSource( dwFrame, &m_TurtleMesh[dwFrame].GetMesh()->m_VB, 0,
                                          m_TurtleMesh[dwFrame].GetMesh()->m_dwVertexSize );

            pDevice->SetIndices( &m_TurtleMesh[0].GetMesh()->m_IB );
            pDevice->DrawIndexedPrimitive( m_TurtleMesh[0].GetMesh()->m_dwPrimType, 0,
                                           0, m_TurtleMesh[0].GetMesh()->m_pSubsets[0].dwVertexCount,
                                           0, m_TurtleMesh[0].GetMesh()->m_pSubsets[0].dwPrimitiveCount );
        }
            break;

        case FISH_TYPE_UD:
        {
            for( DWORD dwFrame = 0; dwFrame < FISH_FRAME_COUNT; ++dwFrame )
                pDevice->SetStreamSource( dwFrame, &m_UdMesh[dwFrame].GetMesh()->m_VB, 0,
                                          m_UdMesh[dwFrame].GetMesh()->m_dwVertexSize );

            pDevice->SetIndices( &m_UdMesh[0].GetMesh()->m_IB );
            pDevice->DrawIndexedPrimitive( m_UdMesh[0].GetMesh()->m_dwPrimType, 0,
                                           0, m_UdMesh[0].GetMesh()->m_pSubsets[0].dwVertexCount,
                                           0, m_UdMesh[0].GetMesh()->m_pSubsets[0].dwPrimitiveCount );
        }
            break;
    }
}


//--------------------------------------------------------------------------------------
// Name: UpdatePreview
// Desc: Called from RenderFishPreview to setup the device constants for rendering
//       the fish preview window
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdatePreview( FISH_TYPE nFishType, IDirect3DDevice9* pDevice )
{
    FLOAT fTime = ( FLOAT )m_Timer.GetAppTime();

    // Animation attributes for the fish
    FLOAT fKickFreq = 2 * fTime;
    FLOAT fBlendWeight = sinf( fKickFreq );

    XMMATRIX matFish, matTrans, matRotate;

    switch( m_nFishType )
    {
        case FISH_TYPE_SHARK:
        {
            matFish = XMMatrixScaling( 0.001f, 0.001f, 0.001f );
            matRotate = XMMatrixRotationZ( -cosf( fKickFreq ) / 6 );
            matFish = XMMatrixMultiply( matFish, matRotate );
            matRotate = XMMatrixRotationY( -( D3DX_PI / 2 ) );
            matFish = XMMatrixMultiply( matFish, matRotate );
            matTrans = XMMatrixTranslation( -0.1f, 0, 0 );
            matFish = XMMatrixMultiply( matFish, matTrans );
        }
            break;

        case FISH_TYPE_TURTLE:
        {
            matFish = XMMatrixScaling( 0.003f, 0.003f, 0.003f );
            matRotate = XMMatrixRotationZ( D3DX_PI );
            matFish = XMMatrixMultiply( matFish, matRotate );
            matRotate = XMMatrixRotationY( D3DX_PI );
            matFish = XMMatrixMultiply( matFish, matRotate );
            matTrans = XMMatrixTranslation( -0.2f, 0, 0 );
            matFish = XMMatrixMultiply( matFish, matTrans );
        }
            break;

        case FISH_TYPE_UD:
        {
            matFish = XMMatrixScaling( 0.005f, 0.005f, 0.005f );
            matRotate = XMMatrixRotationZ( -cosf( fKickFreq ) / 6 );
            matFish = XMMatrixMultiply( matFish, matRotate );
            matRotate = XMMatrixRotationY( -( D3DX_PI / 2 ) );
            matFish = XMMatrixMultiply( matFish, matRotate );
            matTrans = XMMatrixTranslation( -0.1f, 0, 0 );
            matFish = XMMatrixMultiply( matFish, matTrans );
        }
            break;
    }

    // Animate the caustic textures
    DWORD tex = ( ( DWORD )( fTime * 32 ) ) % 32;
    m_pCurrentCausticTexture = m_pCausticTextures[ tex ];

    // Set the vertex shader constants. Note: outside of the blend matrices,
    // most of these values don't change, so don't need to really be set every
    // frame. It's just done here for clarity
    {
        // Some basic constants
        static XMFLOAT4 vZero( 0.0f, 0.0f, 0.0f, 0.0f );
        static XMFLOAT4 vConstants( 1.0f, 0.5f, 1.0f, 0.05f );

        FLOAT fWeight1;
        FLOAT fWeight2;
        FLOAT fWeight3;

        if( fBlendWeight > 0.0f )
        {
            fWeight1 = fabsf( fBlendWeight );
            fWeight2 = 1.0f - fabsf( fBlendWeight );
            fWeight3 = 0.0f;
        }
        else
        {
            fWeight1 = 0.0f;
            fWeight2 = 1.0f - fabsf( fBlendWeight );
            fWeight3 = fabsf( fBlendWeight );
        }
        XMVECTOR vWeight = XMVectorSet( fWeight1, fWeight2, fWeight3, 0.0f );

        // Lighting vectors (in world space and in fish model space)
        // and other constants
        XMVECTOR vLight = XMVectorSet( 0.00f, 1.00f, 0.00f, 0.00f );
        XMVECTOR vLightFishSpace = XMVectorSet( 0.00f, 1.00f, 0.00f, 0.00f );
        XMVECTOR vDiffuse = XMVectorSet( 1.00f, 1.00f, 1.00f, 1.00f );
        XMVECTOR vAmbient = XMVectorSet( 0.25f, 0.25f, 0.25f, 0.25f );
        XMVECTOR vFog = XMVectorSet( 0.50f, 50.00f, 1.00f / ( 50.0f - 1.0f ), 0.00f );

        XMVECTOR vDeterminant;
        XMMATRIX matFishInv = XMMatrixInverse( &vDeterminant, matFish );
        vLightFishSpace = XMVector4Normalize( XMVector4Transform( vLight, matFishInv ) );

        // Vertex shader operations use transposed matrices
        XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -1.0f, 0.0f );
        XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
        XMMATRIX matView, matProj;
        matView = XMMatrixLookAtLH( vEyePt, vLookatPt, m_vUp );
        matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, 1.0f, 0.1f, 10000.0f );

        XMMATRIX mat, matCamera, matTranspose, matCameraTranspose;
        XMMATRIX matViewTranspose, matProjTranspose;
        matCamera = XMMatrixMultiply( matFish, matView );
        mat = XMMatrixMultiply( matCamera, matProj );
        matTranspose = XMMatrixTranspose( mat );
        matCameraTranspose = XMMatrixTranspose( matCamera );
        matViewTranspose = XMMatrixTranspose( matView );
        matProjTranspose = XMMatrixTranspose( matProj );
        XMMATRIX matModelTranspose = XMMatrixTranspose( matFish );

        // Set the vertex shader constants
        pDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&vZero, 1 );
        pDevice->SetVertexShaderConstantF( 1, ( FLOAT* )&vConstants, 1 );
        pDevice->SetVertexShaderConstantF( 2, ( FLOAT* )&vWeight, 1 );
        pDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matTranspose, 4 );
        pDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&matCameraTranspose, 4 );
        pDevice->SetVertexShaderConstantF( 12, ( FLOAT* )&matViewTranspose, 4 );
        pDevice->SetVertexShaderConstantF( 16, ( FLOAT* )&matProjTranspose, 4 );
        pDevice->SetVertexShaderConstantF( 20, ( FLOAT* )&vLight, 1 );
        pDevice->SetVertexShaderConstantF( 21, ( FLOAT* )&vLightFishSpace, 1 );
        pDevice->SetVertexShaderConstantF( 22, ( FLOAT* )&vDiffuse, 1 );
        pDevice->SetVertexShaderConstantF( 23, ( FLOAT* )&vAmbient, 1 );
        pDevice->SetVertexShaderConstantF( 24, ( FLOAT* )&vFog, 1 );
        pDevice->SetVertexShaderConstantF( 25, ( FLOAT* )&matModelTranspose, 4 );
    }
    return S_OK;
}
