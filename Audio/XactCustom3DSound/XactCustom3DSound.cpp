//--------------------------------------------------------------------------------------
// XactCustom3DSound.cpp
//
// The sample demonstrates the use of X3DAudio engine with XACT directly instead of
// using the xact3d3.h helper header (see the Xact3DSound sample)
//
// With Aug SDK, features that X3DAudio supports are just basic functionality of 3D
// panning. The sample will be updated to show additional features of X3DAudio as they
// become available.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xact3.h>
#include <x3daudio.h>
#include <assert.h>
#include <AtgApp.h>
#include <AtgAudio.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_1, L"Display help" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle\nsound" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_2, L"Change\nsound" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle source/\nlistener" },
    { ATG::HELP_LEFT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Increase\nvolume" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Decrease\nvolume" },
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_2, L"Move object\nin X/Z" },
};

const DWORD         NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[ 0 ] );


//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------

// Speed of sound in the sample
const FLOAT         SPEEDOFSOUND = 340.29f;

// Channel count
const DWORD         CHANNELCOUNT = 6;

// Volume parameters
const FLOAT         VOLUME_SCALE = 0.01f;
const FLOAT         VOLUME_MAX = 1.00f;
const FLOAT         VOLUME_MIN = 0.00f;

// Constants to define our world space
const INT           XMIN = -10;
const INT           XMAX = 10;
const INT           ZMIN = -10;
const INT           ZMAX = 10;

// Constants for colors
static const DWORD  SOURCE_COLOR = 0xffea1b1b;
static const DWORD  LISTENER_COLOR = 0xff1b1bea;
static const DWORD  FLOOR_COLOR = 0xff101010;
static const DWORD  GRID_COLOR = 0xff00a000;

// Constants for scaling input
const FLOAT         MOTION_SCALE = 10.0f;


//--------------------------------------------------------------------------------------
// Global variables and definitions
//--------------------------------------------------------------------------------------

// List of sound cues to cycle through
CHAR*               g_strCueNames[] =
{
    "Heli",
    "DockingMono",
    "EngineStartMono",
    "MaleDialog1",
    "MiningMono",
    "MusicMono",
    "Dolphin4",
};
const DWORD         NUM_SOUNDS = sizeof( g_strCueNames ) / sizeof( g_strCueNames[ 0 ] );

struct D3DVERTEX
{
    XMFLOAT3 p;           // position
    D3DCOLOR c;           // color
};

FLOAT g_EmitterAzimuths[] = { 0.0f };


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    // X3DAudio stuff
    X3DAUDIO_DSP_SETTINGS m_DSPSettings;
    X3DAUDIO_LISTENER m_Listener;
    X3DAUDIO_EMITTER m_Emitter;
    X3DAUDIO_CONE m_Cone;

    // XACT stuff
    IXACT3Engine* m_pXACTEngine;                  // XACT Engine instance
    IXACT3WaveBank* m_pWaveBank;                    // Wave bank
    IXACT3SoundBank* m_pSoundBank;                   // Sound bank
    XACTINDEX m_dwSoundCueIndex;              // Cue index
    IXACT3Cue* m_pCue;                         // Cue instance

    // Misc
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    DWORD m_dwCurrent;               // Current sound
    BOOL m_bPlaying;                // Are we playing?
    FLOAT m_fVolume;                 // Current volume

    // Render stuff
    LPDIRECT3DVERTEXSHADER9 m_pVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pPixelShader;
    IDirect3DVertexDeclaration9* m_pVertexDecl;

    DWORD           m_Pad[1];

    // Sound source and listener positions
    X3DAUDIO_HANDLE m_X3DInstance;
    XMVECTOR m_vSourcePosition;      // Source position vector
    XMVECTOR m_vListenerPosition;    // Listener position vector
    FLOAT m_fListenerAngle;       // Listener orientation angle in x-z

    // Transform matrices
    XMMATRIX m_matWorld;             // World transform
    XMMATRIX m_matView;              // View transform
    XMMATRIX m_matProj;              // Projection transform
    XMMATRIX m_matViewProj;          // ViewProjection transform

    // Models for floor, source, and listener
    LPDIRECT3DVERTEXBUFFER9 m_pvbFloor;             // Quad for the floor
    LPDIRECT3DVERTEXBUFFER9 m_pvbSource;            // Quad for the source
    LPDIRECT3DVERTEXBUFFER9 m_pvbListener;          // Quad for the listener
    LPDIRECT3DVERTEXBUFFER9 m_pvbGrid;              // Lines to grid the floor

    D3DCOLOR m_dwSourceColor;                // Color for sound source
    D3DCOLOR m_dwListenerColor;              // Color for listener

    BOOL m_bControlSource;               // Control source or listener

    VOID            InitializeUIElements();
    VOID            StopCue( IXACT3Cue* pCue );
    HRESULT         PrimeSound( DWORD dwIndex );            // Prime sound data

private:

    static void     XACTNotificationCallback( const XACT_NOTIFICATION* pNotification );

    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    m_bDrawHelp = FALSE;
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create XACT Engine
    if( FAILED( XACT3CreateEngine( 0, &m_pXACTEngine ) ) )
        ATG::FatalError( "Could not create a XACT Engine\n" );

    // Load the XACT global settings file
    VOID* pbGlobalSettings = NULL;
    DWORD dwFileSize = 0;
    if( FAILED( ATG::LoadFile( "game:\\media\\sounds\\XactSounds.xgs",
                               &pbGlobalSettings,
                               &dwFileSize ) ) )
        ATG::FatalError( "Could not load file \"XactSounds.xgs\"\n" );

    // Initialize the XACT runtime parameters
    XACT_RUNTIME_PARAMETERS xrParams = { 0 };

    xrParams.fnNotificationCallback = &this->XACTNotificationCallback;
    xrParams.pGlobalSettingsBuffer = pbGlobalSettings;
    xrParams.globalSettingsBufferSize = dwFileSize;
    xrParams.lookAheadTime = XACT_ENGINE_LOOKAHEAD_DEFAULT;

    // Create the XACT runtime engine
    HRESULT hr = m_pXACTEngine->Initialize( &xrParams );
    if( FAILED( hr ) )
        ATG::FatalError( "Initialize failed with error %#X\n", hr );

    // Initialize the X3DAudio
    //  Speaker geometry configuration on the final mix, specifies assignment of channels
    //  to speaker positions, defined as per WAVEFORMATEXTENSIBLE.dwChannelMask
    //
    //  SpeedOfSound - speed of sound in user-defined world units/second, used
    //  only for doppler calculations, it must be >= FLT_MIN
    X3DAudioInitialize( SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER |
                        SPEAKER_LOW_FREQUENCY |
                        SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT,
                        SPEEDOFSOUND, m_X3DInstance );

    // Initialize 3D audio parameters
    X3DAUDIO_VECTOR ZeroVector = { 0.0f, 0.0f, 0.0f };

    // Set up listener parameters
    m_Listener.OrientFront.x = 0.0f;
    m_Listener.OrientFront.y = 0.0f;
    m_Listener.OrientFront.z = 1.0f;
    m_Listener.OrientTop.x = 0.0f;
    m_Listener.OrientTop.y = 1.0f;
    m_Listener.OrientTop.z = 0.0f;
    m_Listener.Position.x = 0.0f;
    m_Listener.Position.y = 0.0f;
    m_Listener.Position.z = ( FLOAT )ZMIN;
    m_Listener.Velocity = ZeroVector;
    m_Listener.pCone = NULL;

    // Set up emitter parameters
    m_Emitter.OrientFront.x = 0.0f;
    m_Emitter.OrientFront.y = 0.0f;
    m_Emitter.OrientFront.z = 1.0f;
    m_Emitter.OrientTop.x = 0.0f;
    m_Emitter.OrientTop.y = 1.0f;
    m_Emitter.OrientTop.z = 0.0f;
    m_Emitter.Position = ZeroVector;
    m_Emitter.Velocity = ZeroVector;
    m_Emitter.InnerRadius = 0.0f;
    m_Emitter.InnerRadiusAngle = 0.0f;
    m_Emitter.pCone = &m_Cone;
    m_Emitter.pCone->InnerAngle = 0.0f; // Setting the inner cone angles to X3DAUDIO_2PI and
    // outer cone other than 0 causes
    // the emitter to act like a point emitter using the
    // INNER cone settings only.
    m_Emitter.pCone->OuterAngle = 0.0f; // Setting the outer cone angles to zero causes
    // the emitter to act like a point emitter using the
    // OUTER cone settings only.
    m_Emitter.pCone->InnerVolume = 0.0f;
    m_Emitter.pCone->OuterVolume = 1.0f;
    m_Emitter.pCone->InnerLPF = 0.0f;
    m_Emitter.pCone->OuterLPF = 1.0f;
    m_Emitter.pCone->InnerReverb = 0.0f;
    m_Emitter.pCone->OuterReverb = 1.0f;

    m_Emitter.ChannelCount = 1;
    m_Emitter.ChannelRadius = 0.0f;
    m_Emitter.pVolumeCurve = NULL;
    m_Emitter.pLFECurve = NULL;
    m_Emitter.pLPFDirectCurve = NULL;
    m_Emitter.pLPFReverbCurve = NULL;
    m_Emitter.pReverbCurve = NULL;
    m_Emitter.CurveDistanceScaler = 14.0f;
    m_Emitter.DopplerScaler = 1.0f;
    m_Emitter.pChannelAzimuths = g_EmitterAzimuths;

    m_DSPSettings.SrcChannelCount = 1;
    m_DSPSettings.DstChannelCount = CHANNELCOUNT;
    m_DSPSettings.pMatrixCoefficients = new FLOAT[ CHANNELCOUNT ];

    if( m_DSPSettings.pMatrixCoefficients == NULL )
        return E_OUTOFMEMORY;

    // Open the in memory wave bank
    VOID* pbWaveBank = NULL;
    if( FAILED( hr = ATG::LoadFilePhysicalMemory( "game:\\media\\sounds\\XactCustom3DSounds.xwb",
                                                  &pbWaveBank,
                                                  &dwFileSize ) ) )
        ATG::FatalError( "Could not load file \"XactCustom3DSounds.xwb\", failed with error %#X\n", hr );

    // Register the wave bank with XACT
    if( FAILED( hr = m_pXACTEngine->CreateInMemoryWaveBank( pbWaveBank,
                                                            dwFileSize,
                                                            0,
                                                            0,
                                                            &m_pWaveBank ) ) )
        ATG::FatalError( "CreateInMemoryWaveBank failed with error %#X\n", hr );

    // Load the sound bank
    VOID* pbSoundBank = NULL;
    if( FAILED( hr = ATG::LoadFile( "game:\\media\\sounds\\XactCustom3DSounds.xsb",
                                    &pbSoundBank,
                                    &dwFileSize ) ) )
        ATG::FatalError( "Could not load file \"XactCustom3DSounds.xsb\", failed with error %#X\n", hr );

    // Register the sound bank with XACT
    if( FAILED( hr = m_pXACTEngine->CreateSoundBank( pbSoundBank,
                                                     dwFileSize,
                                                     0,
                                                     0,
                                                     &m_pSoundBank ) ) )
        ATG::FatalError( "CreateSoundBank failed with error %#X\n", hr );

    // Set up and play our initial sound
    m_dwCurrent = 0;

    // Null-terminated string representing the cue friendly name
    m_dwSoundCueIndex = m_pSoundBank->GetCueIndex( g_strCueNames[ m_dwCurrent ] );
    if( m_dwSoundCueIndex == XACTINDEX_INVALID )
        ATG::FatalError( "GetCueIndex failed\n" );

    // Positions
    m_vSourcePosition = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    m_vListenerPosition = XMVectorSet( 0.0f, 0.0f, ( FLOAT )ZMIN, 0.0f );

    // Listener default orientation
    m_fListenerAngle = 0.0f;

    // Set up for initial playback
    m_fVolume = VOLUME_MAX;
    m_pCue = NULL;
    m_dwCurrent = 0;
    m_bPlaying = TRUE;
    PrimeSound( m_dwSoundCueIndex );

    InitializeUIElements();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitializeUIElements()
// Desc: Initialize UI elements. For simplification of Initialize(), separated
//       UI initialization
//--------------------------------------------------------------------------------------
void Sample::InitializeUIElements()
{
    HRESULT hr;

    // Create shaders
    VOID* pCode = NULL;
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\PositionDiffuse.xvu", &pCode ) ) )
        ATG::FatalError( "Shader file is not found\n" );
    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pVertexShader ) ) )
        ATG::FatalError( "Shader creation error\n" );
    ATG::UnloadFile( pCode );

    // Create pixel shader
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\Diffuse.xpu", &pCode ) ) )
        ATG::FatalError( "Shader file is not found\n" );
    if( FAILED( hr = m_pd3dDevice->CreatePixelShader( ( DWORD* )pCode, &m_pPixelShader ) ) )
        ATG::FatalError( "Shader creation error\n" );
    ATG::UnloadFile( pCode );

    // Define the vertex elements.
    static const D3DVERTEXELEMENT9 VertexElements[3] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
        D3DDECL_END()
    };

    // Create a vertex declaration from the element descriptions.
    m_pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pVertexDecl );

    // Set the transform matrices
    XMVECTOR vEyePt = XMVectorSet( ( FLOAT )XMIN, 45.0f, ( FLOAT )ZMAX / 2.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( ( FLOAT )XMIN, 0.0f, ( FLOAT )ZMAX / 2.0f, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
    m_matWorld = XMMatrixIdentity();
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, 4.0f / 3.0f, 1.0f, 10000.0f );
    m_matViewProj = XMMatrixMultiply( m_matView, m_matProj );

    // Vertex shader operations use transposed matrices
    XMMATRIX mat;
    mat = XMMatrixMultiply( m_matWorld, m_matView );
    mat = XMMatrixMultiply( mat, m_matProj );
    mat = XMMatrixTranspose( mat );

    // Set the vertex shader constants
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&mat, 4 );

    // Create our vertex buffers
    m_pd3dDevice->CreateVertexBuffer( sizeof( D3DVERTEX ) * 4,
                                      0, 0, D3DPOOL_DEFAULT, &m_pvbFloor, NULL );
    m_pd3dDevice->CreateVertexBuffer( sizeof( D3DVERTEX ) * 4,
                                      0, 0, D3DPOOL_DEFAULT, &m_pvbSource, NULL );
    m_pd3dDevice->CreateVertexBuffer( sizeof( D3DVERTEX ) * 3,
                                      0, 0, D3DPOOL_DEFAULT, &m_pvbListener, NULL );
    m_pd3dDevice->CreateVertexBuffer( sizeof( D3DVERTEX ) * 2 * ( ( ZMAX - ZMIN + 1 ) + ( XMAX - XMIN + 1 ) ),
                                      0, 0, D3DPOOL_DEFAULT, &m_pvbGrid, NULL );

    // Fill the VB for the listener
    D3DVERTEX* pVertices;
    m_pvbListener->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    pVertices[ 0 ].p = XMFLOAT3( -0.5f, 0.0f, -1.0f );
    pVertices[ 0 ].c = LISTENER_COLOR;
    pVertices[ 1 ].p = XMFLOAT3( 0.0f, 0.0f, 1.0f );
    pVertices[ 1 ].c = LISTENER_COLOR;
    pVertices[ 2 ].p = XMFLOAT3( 0.5f, 0.0f, -1.0f );
    pVertices[ 2 ].c = LISTENER_COLOR;
    m_pvbListener->Unlock();

    // Fill the VB for the source
    m_pvbSource->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    pVertices[ 0 ].p = XMFLOAT3( -0.5f, 0.0f, -0.5f );
    pVertices[ 0 ].c = SOURCE_COLOR;
    pVertices[ 1 ].p = XMFLOAT3( -0.5f, 0.0f, 0.5f );
    pVertices[ 1 ].c = SOURCE_COLOR;
    pVertices[ 2 ].p = XMFLOAT3( 0.5f, 0.0f, -0.5f );
    pVertices[ 2 ].c = SOURCE_COLOR;
    pVertices[ 3 ].p = XMFLOAT3( 0.5f, 0.0f, 0.5f );
    pVertices[ 3 ].c = SOURCE_COLOR;
    m_pvbSource->Unlock();

    // Fill the VB for the floor
    m_pvbFloor->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    pVertices[ 0 ].p = XMFLOAT3( ( FLOAT )XMIN, 0.0f, ( FLOAT )ZMIN );
    pVertices[ 0 ].c = FLOOR_COLOR;
    pVertices[ 1 ].p = XMFLOAT3( ( FLOAT )XMIN, 0.0f, ( FLOAT )ZMAX );
    pVertices[ 1 ].c = FLOOR_COLOR;
    pVertices[ 2 ].p = XMFLOAT3( ( FLOAT )XMAX, 0.0f, ( FLOAT )ZMIN );
    pVertices[ 2 ].c = FLOOR_COLOR;
    pVertices[ 3 ].p = XMFLOAT3( ( FLOAT )XMAX, 0.0f, ( FLOAT )ZMAX );
    pVertices[ 3 ].c = FLOOR_COLOR;
    m_pvbFloor->Unlock();

    // Fill the VB for the grid
    INT i, j;
    m_pvbGrid->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    for( i = ZMIN, j = 0; i <= ZMAX; i++, j++ )
    {
        pVertices[ j * 2 + 0 ].p = XMFLOAT3( ( FLOAT )XMIN, 0.0f, ( FLOAT )i );
        pVertices[ j * 2 + 0 ].c = GRID_COLOR;
        pVertices[ j * 2 + 1 ].p = XMFLOAT3( ( FLOAT )XMAX, 0.0f, ( FLOAT )i );
        pVertices[ j * 2 + 1 ].c = GRID_COLOR;
    }
    for( i = XMIN; i <= XMAX; i++, j++ )
    {
        pVertices[ j * 2 + 0 ].p = XMFLOAT3( ( FLOAT )i, 0.0f, ( FLOAT )ZMIN );
        pVertices[ j * 2 + 0 ].c = GRID_COLOR;
        pVertices[ j * 2 + 1 ].p = XMFLOAT3( ( FLOAT )i, 0.0f, ( FLOAT )ZMAX );
        pVertices[ j * 2 + 1 ].c = GRID_COLOR;
    }
    m_pvbGrid->Unlock();
}

//--------------------------------------------------------------------------------------
// Name: XACTNotificationCallback()
// Desc: Received notifications from the XACT engine.  Assume that the pvContext
//         is an event handle which is signaled.
//--------------------------------------------------------------------------------------
void Sample::XACTNotificationCallback( const XACT_NOTIFICATION* pNotification )
{

    if( ( NULL != pNotification ) && ( NULL != pNotification->pvContext ) )
    {
        SetEvent( ( HANDLE )pNotification->pvContext );
    }

}

//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    XMVECTOR vSourceOld = m_vSourcePosition;
    XMVECTOR vListenerOld = m_vListenerPosition;
    DWORD dwPulse = DWORD( ( cosf( ( FLOAT )m_Timer.GetAppTime() * 3.0f ) + 1.f ) * 80.f );
    D3DCOLOR cBlend = dwPulse | ( dwPulse << 8 ) | ( dwPulse << 16 );

    // Increase/Decrease volume
    m_fVolume += ( 255 * ( pGamepad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER ? 1 : 0 ) -
                   255 * ( pGamepad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER ? 1 : 0 ) ) *
        fElapsedTime * VOLUME_SCALE;

    // Make sure volume is in the appropriate range
    if( m_fVolume < VOLUME_MIN )
        m_fVolume = VOLUME_MIN;
    else if( m_fVolume > VOLUME_MAX )
        m_fVolume = VOLUME_MAX;

    // Toggle sound on and off
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        if( m_bPlaying )
        {
            m_bPlaying = FALSE;
            StopCue( m_pCue );
            m_pCue->Destroy();
            m_pCue = NULL;
        }
        else
        {
            m_bPlaying = TRUE;
            // Play the sound cue
            PrimeSound( m_dwCurrent );
        }
    }

    // Cycle through sounds
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_dwCurrent = ( m_dwCurrent + 1 ) % NUM_SOUNDS;
        PrimeSound( m_dwCurrent );
    }

    // Switch which of source vs. listener we are moving
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_bControlSource = !m_bControlSource;
    }

    // Set up our colors
    m_dwSourceColor = SOURCE_COLOR | ( m_bControlSource ? cBlend : 0 );
    m_dwListenerColor = LISTENER_COLOR | ( !m_bControlSource ? cBlend : 0 );

    // Point to the appropriate vector
    XMVECTOR* pvControl = m_bControlSource ? &m_vSourcePosition : &m_vListenerPosition;

    // Move selected object and clamp to the appropriate range
    pvControl->x += pGamepad->fX1 * fElapsedTime * MOTION_SCALE;
    if( pvControl->x < XMIN )
        pvControl->x = XMIN;
    else if( pvControl->x > XMAX )
        pvControl->x = XMAX;

    pvControl->z += pGamepad->fY1 * fElapsedTime * MOTION_SCALE;
    if( pvControl->z < ZMIN )
        pvControl->z = ZMIN;
    else if( pvControl->z > ZMAX )
        pvControl->z = ZMAX;

    // Calculate listener orientation in x-z plane
    if( m_vListenerPosition.x != vListenerOld.x || m_vListenerPosition.z != vListenerOld.z )
    {
        XMVECTOR vDelta = m_vListenerPosition - vListenerOld;
        m_fListenerAngle = atan2f( vDelta.x, vDelta.z );
        vDelta.y = 0.0f;
        vDelta = XMVector3Normalize( vDelta );
        m_Listener.OrientFront.x = vDelta.x;
        m_Listener.OrientFront.y = 0.f;
        m_Listener.OrientFront.z = vDelta.z;
    }

    if( fElapsedTime > 0.0f )
    {
        // Position the sound and listener in 3D.
        XMVECTOR vListenerVelocity = ( m_vListenerPosition - vListenerOld ) / fElapsedTime;
        XMVECTOR vSoundVelocity = ( m_vSourcePosition - vSourceOld ) / fElapsedTime;

        // Set up listener parameters
        m_Listener.Position.x = m_vListenerPosition.x;
        m_Listener.Position.y = m_vListenerPosition.y;
        m_Listener.Position.z = m_vListenerPosition.z;
        m_Listener.Velocity.x = vListenerVelocity.x;
        m_Listener.Velocity.y = vListenerVelocity.y;
        m_Listener.Velocity.z = vListenerVelocity.z;

        // Set up emitter parameters
        m_Emitter.Position.x = m_vSourcePosition.x;
        m_Emitter.Position.y = m_vSourcePosition.y;
        m_Emitter.Position.z = m_vSourcePosition.z;
        m_Emitter.Velocity.x = vSoundVelocity.x;
        m_Emitter.Velocity.y = vSoundVelocity.y;
        m_Emitter.Velocity.z = vSoundVelocity.z;
    }

    // Set source position, velocity and volume

    // Retrieve 3D Audio parameters
    DWORD dwCalculateFlags = X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER |
        X3DAUDIO_CALCULATE_LPF_DIRECT | X3DAUDIO_CALCULATE_LPF_REVERB |
        X3DAUDIO_CALCULATE_REVERB;
    // There are other flags that control additional X3DAudio
    // functionality that will be illustrated as they become
    // available
    X3DAudioCalculate( m_X3DInstance, &m_Listener, &m_Emitter, dwCalculateFlags, &m_DSPSettings );

    // Normally you'd just pass the results directly to the cue, but for this sample we do something 'custom' to it.
    // Here we use a simple volume scale to demonstrate this concept.
    FLOAT matrix[ CHANNELCOUNT ];
    for( int i = 0; i < CHANNELCOUNT; ++i )
    {
        matrix[ i ] = m_fVolume * m_DSPSettings.pMatrixCoefficients[ i ];
    }

    if( m_pCue )
    {
        m_pCue->SetMatrixCoefficients( 1, CHANNELCOUNT, matrix );

        XACTVARIABLEINDEX xactDistanceID = m_pCue->GetVariableIndex( "Distance" );
        m_pCue->SetVariable( xactDistanceID, m_DSPSettings.EmitterToListenerDistance );

        XACTVARIABLEINDEX xactDopplerID = m_pCue->GetVariableIndex( "DopplerPitchScalar" );
        m_pCue->SetVariable( xactDopplerID, m_DSPSettings.DopplerFactor );

        XACTVARIABLEINDEX xactOrientationID = m_pCue->GetVariableIndex( "OrientationAngle" );
        m_pCue->SetVariable( xactOrientationID, m_DSPSettings.EmitterToListenerAngle * ( 180.0f / X3DAUDIO_PI ) );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: PrimeSound()
// Desc: Primes the given sound by:
//       1) Stop playback if we're playing
//       2) Reallocate the sample data buffer
//       3) Set a sound format if necessary
//       4) Point the Source voice to the new data
//       5) Restart playback if needed
//--------------------------------------------------------------------------------------
HRESULT Sample::PrimeSound( DWORD dwIndex )
{
    HRESULT hr;

    if( m_pCue )
    {
        StopCue( m_pCue );
        m_pCue->Destroy();
    }

    // Null-terminated string representing the cue friendly name
    m_dwSoundCueIndex = m_pSoundBank->GetCueIndex( g_strCueNames[ m_dwCurrent ] );
    if( m_dwSoundCueIndex == XACTINDEX_INVALID )
    {
        ATG::FatalError( "GetCueIndex failed\n" );
    }

    // Play the sound cue
    // If we were playing before, restart playback now
    if( m_bPlaying )
    {
        // We have to prepare the cue first, so that we can set the channel
        // map before playback
        if( FAILED( hr = m_pSoundBank->Prepare( m_dwSoundCueIndex, 0, 0, &m_pCue ) ) )
        {
            ATG::FatalError( "Prepare failed with error %#X\n", hr );
        }

        if( FAILED( hr = m_pCue->Play() ) )
        {
            ATG::FatalError( "Play failed with error %#X\n", hr );
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: StopCue()
// Desc: Stops playing the cue
//--------------------------------------------------------------------------------------
VOID Sample::StopCue( IXACT3Cue* pCue )
{
    assert( pCue != NULL );
    HRESULT hr;

    // Initialize XACT notification struct
    XACT_NOTIFICATION_DESCRIPTION xactNotificationDesc = { 0 };
    xactNotificationDesc.type = XACTNOTIFICATIONTYPE_CUESTOP;
    xactNotificationDesc.pSoundBank = m_pSoundBank;
    xactNotificationDesc.cueIndex = m_dwSoundCueIndex;
    xactNotificationDesc.pvContext = CreateEvent( NULL, FALSE, FALSE, NULL );

    if( NULL == xactNotificationDesc.pvContext )
        ATG::FatalError( "Failed to create event object for XACTNOTIFICATIONTYPE_CUESTOP notification" );

    // Register a stop notification with the XACT .
    // This will allow us to monitor when the cue stops playing.
    if( FAILED( hr = m_pXACTEngine->RegisterNotification( &xactNotificationDesc ) ) )
        ATG::FatalError( "Notification registration failed with error %#X\n", hr );

    pCue->Stop( XACT_FLAG_CUE_STOP_IMMEDIATE );

    do
    {

        m_pXACTEngine->DoWork();
        Sleep( 1 );

    } while( WAIT_TIMEOUT == WaitForSingleObject( ( HANDLE )xactNotificationDesc.pvContext, 4 ) );

    CloseHandle( ( HANDLE )xactNotificationDesc.pvContext );
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    // Set default render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    // Vertex shader operations use transposed matrices
    XMMATRIX mat;
    mat = XMMatrixMultiply( m_matWorld, m_matViewProj );
    mat = XMMatrixTranspose( mat );
    // Set the vertex shader constants
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&mat, 4 );

    // Set the vertex shader
    m_pd3dDevice->SetVertexShader( m_pVertexShader );

    // Set the vertex declaration.
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );

    // Set the pixel shader
    m_pd3dDevice->SetPixelShader( m_pPixelShader );

    // Draw the floor
    m_pd3dDevice->SetStreamSource( 0, m_pvbFloor, 0, sizeof( D3DVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

    // Draw the grid
    m_pd3dDevice->SetStreamSource( 0, m_pvbGrid, 0, sizeof( D3DVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_LINELIST, 0, 2 * ( ( ZMAX - ZMIN + 1 ) + ( XMAX - XMIN + 1 ) ) );

    // Draw the source
    {
        mat = XMMatrixTranslation( m_vSourcePosition.x,
                                   m_vSourcePosition.y,
                                   m_vSourcePosition.z );
        mat = XMMatrixMultiply( mat, m_matViewProj );
        mat = XMMatrixTranspose( mat );

        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&mat, 4 );
        m_pd3dDevice->SetStreamSource( 0, m_pvbSource, 0, sizeof( D3DVERTEX ) );
        m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );
    }

    // Draw the listener
    {
        XMMATRIX matTrans = XMMatrixTranslation( m_vListenerPosition.x,
                                                 m_vListenerPosition.y,
                                                 m_vListenerPosition.z );
        XMMATRIX matRotate = XMMatrixRotationY( m_fListenerAngle );
        mat = XMMatrixMultiply( matRotate, matTrans );
        mat = XMMatrixMultiply( mat, m_matViewProj );
        mat = XMMatrixTranspose( mat );

        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&mat, 4 );
        m_pd3dDevice->SetStreamSource( 0, m_pvbListener, 0, sizeof( D3DVERTEX ) );
        m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 1 );
    }

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        WCHAR strBuffer[200];

        // Show title
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"XactCustom3DSound" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // Show status
        swprintf_s( strBuffer, L"%S", g_strCueNames[m_dwCurrent] );
        m_Font.DrawText( 16, 42, 0xffffffff, L"Sound: " );
        m_Font.DrawText( 128, 42, m_bPlaying ? 0xffffff00 : 0xff808000, strBuffer );

        swprintf_s( strBuffer, L"<%0.1f, %0.1f, %0.1f>", m_vSourcePosition.x,
                    m_vSourcePosition.y,
                    m_vSourcePosition.z );
        m_Font.DrawText( 16, 67, 0xffffffff, L"Source: " );
        m_Font.DrawText( 128, 67, m_dwSourceColor, strBuffer );

        swprintf_s( strBuffer, L"<%0.1f, %0.1f, %0.1f>", m_vListenerPosition.x,
                    m_vListenerPosition.y, m_vListenerPosition.z );
        m_Font.DrawText( 16, 92, 0xffffffff, L"Listener: " );
        m_Font.DrawText( 128, 92, m_dwListenerColor, strBuffer );

        // Show percentage and volume (rounded to nearest dB)
        FLOAT fPercent = m_fVolume * 100;
        FLOAT fDecibel;
        if( m_fVolume <= 0.01f )
            fDecibel = -85.f;
        else
            fDecibel = 20.0f * log10f( m_fVolume );
        swprintf_s( strBuffer, L"%3.0fdB (%0.0f%%)", fDecibel, fPercent );
        m_Font.DrawText( 16, 117, 0xffffffff, L"Volume: " );
        m_Font.DrawText( 128, 117, 0xffffff00, strBuffer );

        swprintf_s( strBuffer, L"%.3f\n%.3f\n%.3f\n%.3f\n%.3f\n%.3f",
                    m_DSPSettings.pMatrixCoefficients[ 0 ],
                    m_DSPSettings.pMatrixCoefficients[ 1 ],
                    m_DSPSettings.pMatrixCoefficients[ 2 ],
                    m_DSPSettings.pMatrixCoefficients[ 3 ],
                    m_DSPSettings.pMatrixCoefficients[ 4 ],
                    m_DSPSettings.pMatrixCoefficients[ 5 ] );
        m_Font.SetScaleFactors( 0.6f, 0.8f );
        m_Font.DrawText( 16, 152, 0xffffffff, L"Coefficients:" );
        m_Font.DrawText( 102, 152, 0xffffffff, L"L\nR\nC\nLFE\nLs\nRs" );
        m_Font.DrawText( 132, 152, 0xffffff00, strBuffer );

        FLOAT fYPos = 256.0f;

        swprintf_s( strBuffer, L"%.3f", m_DSPSettings.EmitterToListenerDistance );
        m_Font.DrawText( 16, fYPos += 20, 0xff808080, L"Distance:" );
        m_Font.DrawText( 127, fYPos, 0xff808000, strBuffer );

        swprintf_s( strBuffer, L"%.3f", m_DSPSettings.DopplerFactor );
        m_Font.DrawText( 16, fYPos += 20, 0xff808080, L"Doppler factor:" );
        m_Font.DrawText( 127, fYPos, 0xff808000, strBuffer );

        swprintf_s( strBuffer, L"%.3f", m_DSPSettings.LPFDirectCoefficient );
        m_Font.DrawText( 16, fYPos += 20, 0xff808080, L"LPF Direct:" );
        m_Font.DrawText( 127, fYPos, 0xff808000, strBuffer );

        swprintf_s( strBuffer, L"%.3f", m_DSPSettings.LPFReverbCoefficient );
        m_Font.DrawText( 16, fYPos += 20, 0xff808080, L"LPF Reverb:" );
        m_Font.DrawText( 127, fYPos, 0xff808000, strBuffer );

        swprintf_s( strBuffer, L"%.3f", m_DSPSettings.ReverbLevel );
        m_Font.DrawText( 16, fYPos += 20, 0xff808080, L"Reverb:" );
        m_Font.DrawText( 127, fYPos, 0xff808000, strBuffer );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    // Call XACTDoWork
    m_pXACTEngine->DoWork();

    return S_OK;
}

