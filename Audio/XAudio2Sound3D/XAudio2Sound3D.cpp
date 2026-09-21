//--------------------------------------------------------------------------------------
// XAudio2Sound3D.cpp
//
// The sample demonstrates the use of X3DAudio engine with XAudio2.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <AtgApp.h>
#include <AtgAudio.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include "audio.h"

//--------------------------------------------------------------------------------------
// Helper macros
//--------------------------------------------------------------------------------------
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }
#endif


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_1, L"Display help" },
    { ATG::HELP_BOTTOM_RIGHT, ATG::HELP_PLACEMENT_1, L"Right thumb toggles sound" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_2, L"Change\nsound" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle source/\nlistener" },
    { ATG::HELP_LEFT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Increase\nvolume" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Decrease\nvolume" },
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_2, L"Move object\nin X/Z" },
    { ATG::HELP_LEFT_BUTTON, ATG::HELP_PLACEMENT_1, L"Triggers cycle FX parameters" },
    { ATG::HELP_Y_BUTTON, ATG::HELP_PLACEMENT_1, L"Toggle Listener Cone" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_1, L"Toggle Inner Radius" },
};

const DWORD         NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------

// Volume parameters
const FLOAT         VOLUME_SCALE = 0.01f;
const FLOAT         VOLUME_MAX = 1.00f;
const FLOAT         VOLUME_MIN = 0.00f;

// Constants for colors
static const DWORD  SOURCE_COLOR = 0xffea1b1b;
static const DWORD  LISTENER_COLOR = 0xff2b2bff;
static const DWORD  FLOOR_COLOR = 0xff101010;
static const DWORD  GRID_COLOR = 0xff00a000;

// Constants for scaling input
const FLOAT         MOTION_SCALE = 10.0f;


//--------------------------------------------------------------------------------------
// Global variables and definitions
//--------------------------------------------------------------------------------------

// List of wav files to cycle through
WCHAR*              g_SOUND_NAMES[] =
{
    L"Heli.wav",
    L"DockingMono.wav",
    L"EngineStartMono.wav",
    L"MaleDialog1.wav",
    L"MiningMono.wav",
    L"MusicMono.wav",
    L"Dolphin4.wav",
};

const DWORD         NUM_SOUNDS = sizeof( g_SOUND_NAMES ) / sizeof( g_SOUND_NAMES[ 0 ] );

struct D3DVERTEX
{
    XMFLOAT3 p;           // Position
    D3DCOLOR c;           // Color
};

// Must match order of g_PRESET_PARAMS
WCHAR*              g_PRESET_NAMES[ NUM_PRESETS ] =
{
    L"Forest",
    L"Default",
    L"Generic",
    L"Padded cell",
    L"Room",
    L"Bathroom",
    L"Living room",
    L"Stone room",
    L"Auditorium",
    L"Concert hall",
    L"Cave",
    L"Arena",
    L"Hangar",
    L"Carpeted hallway",
    L"Hallway",
    L"Stone Corridor",
    L"Alley",
    L"City",
    L"Mountains",
    L"Quarry",
    L"Plain",
    L"Parking lot",
    L"Sewer pipe",
    L"Underwater",
    L"Small room",
    L"Medium room",
    L"Large room",
    L"Medium hall",
    L"Large hall",
    L"Plate",
};

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    // Misc
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    DWORD m_dwCurrent;            // Current sound
    DWORD m_dwCurrentParam;       // Current sound
    BOOL m_bPlaying;             // Are we playing?
    FLOAT m_fVolume;              // Current volume

    // Render stuff
    LPDIRECT3DVERTEXSHADER9 m_pVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pPixelShader;
    IDirect3DVertexDeclaration9* m_pVertexDecl;

    // Transform matrices
    XMMATRIX m_matWorld;             // World transform
    XMMATRIX m_matView;              // View transform
    XMMATRIX m_matProj;              // Projection transform
    XMMATRIX m_matViewProj;          // ViewProjection transform

    // Models for floor, source, and listener
    LPDIRECT3DVERTEXBUFFER9 m_pvbFloor;             // Quad for the floor
    LPDIRECT3DVERTEXBUFFER9 m_pvbSource;            // Quad for the source
    LPDIRECT3DVERTEXBUFFER9 m_pvbListener;          // Quad for the listener
    LPDIRECT3DVERTEXBUFFER9 m_pvbListenerCone;      // Lines for the listener cone
    LPDIRECT3DVERTEXBUFFER9 m_pvbInnerRadius;       // Lines for the inner radius
    LPDIRECT3DVERTEXBUFFER9 m_pvbGrid;              // Lines to grid the floor

    D3DCOLOR m_dwSourceColor;                // Color for sound source
    D3DCOLOR m_dwListenerColor;              // Color for listener

    BOOL m_bControlSource;               // Control source (TRUE) or
    // listener (FALSE)

    VOID            InitializeUIElements();
    HRESULT         PrimeSound( DWORD dwIndex );

public:
                    Sample() : ATG::Application()
                    {
                    };

    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;

    m_fVolume = VOLUME_MAX;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\media\\help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Init audio
    HRESULT hr = InitAudio();
    if( FAILED( hr ) )
        ATG::FatalError( "Error %#X during InitAudio()\n", hr );

    // Set up and play our initial sound
    m_dwCurrent = 0;
    m_dwCurrentParam = 0;

    m_bPlaying = FALSE;

    hr = PrepareAudio( g_SOUND_NAMES[ m_dwCurrent ] );
    if( FAILED( hr ) )
        ATG::FatalError( "Error %#X during PrepareAudio()\n", hr );

    // Initrialize UI
    m_bControlSource = TRUE;
    InitializeUIElements();

    m_bPlaying = TRUE;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitializeUIElements()
// Desc: Initialize UI elements. For simplification of Initialize (), separated
//       UI initialization
//--------------------------------------------------------------------------------------
VOID Sample::InitializeUIElements()
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
    static const D3DVERTEXELEMENT9 VertexElements[ 3 ] =
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
    m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( D3DVERTEX ),
                                      0, 0, D3DPOOL_DEFAULT, &m_pvbFloor, NULL );
    m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( D3DVERTEX ),
                                      0, 0, D3DPOOL_DEFAULT, &m_pvbSource, NULL );
    m_pd3dDevice->CreateVertexBuffer( 3 * sizeof( D3DVERTEX ),
                                      0, 0, D3DPOOL_DEFAULT, &m_pvbListener, NULL );
    m_pd3dDevice->CreateVertexBuffer( 7 * sizeof( D3DVERTEX ),
                                      0, 0, D3DPOOL_DEFAULT, &m_pvbListenerCone, NULL );
    m_pd3dDevice->CreateVertexBuffer( 9 * sizeof( D3DVERTEX ),
                                      0, 0, D3DPOOL_DEFAULT, &m_pvbInnerRadius, NULL );
    m_pd3dDevice->CreateVertexBuffer( 2 * sizeof( D3DVERTEX ) * ( ( ZMAX - ZMIN + 1 ) + ( XMAX - XMIN + 1 ) ),
                                      0, 0, D3DPOOL_DEFAULT, &m_pvbGrid, NULL );

    D3DVERTEX* pVertices;

    // Fill the VB for the listener, listener cone, and inner radius
    m_pvbListener->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    pVertices[0].p = XMFLOAT3( -0.5f, 0.0f, -1.0f );
    pVertices[0].c = LISTENER_COLOR;
    pVertices[1].p = XMFLOAT3( 0.0f, 0.0f, 1.0f );
    pVertices[1].c = LISTENER_COLOR;
    pVertices[2].p = XMFLOAT3( 0.5f, 0.0f, -1.0f );
    pVertices[2].c = LISTENER_COLOR;
    m_pvbListener->Unlock();
    m_pvbListenerCone->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    pVertices[0].p = XMFLOAT3( -1.04f, 0, -3.86f );
    pVertices[0].c = LISTENER_COLOR;
    pVertices[1].p = XMFLOAT3( 0, 0, 0 );
    pVertices[1].c = LISTENER_COLOR;
    pVertices[2].p = XMFLOAT3( -3.86f, 0, 1.04f );
    pVertices[2].c = LISTENER_COLOR;
    pVertices[3].p = XMFLOAT3( 0, 0, 0 );
    pVertices[3].c = LISTENER_COLOR;
    pVertices[4].p = XMFLOAT3( 3.86f, 0, 1.04f );
    pVertices[4].c = LISTENER_COLOR;
    pVertices[5].p = XMFLOAT3( 0, 0, 0 );
    pVertices[5].c = LISTENER_COLOR;
    pVertices[6].p = XMFLOAT3( 1.04f, 0, -3.86 );
    pVertices[6].c = LISTENER_COLOR;
    m_pvbListenerCone->Unlock();
    m_pvbInnerRadius->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    pVertices[0].p = XMFLOAT3( 0.0f, 0, -2.0f );
    pVertices[0].c = LISTENER_COLOR;
    pVertices[1].p = XMFLOAT3( 1.4f, 0, -1.4f );
    pVertices[1].c = LISTENER_COLOR;
    pVertices[2].p = XMFLOAT3( 2.0f, 0, 0.0f );
    pVertices[2].c = LISTENER_COLOR;
    pVertices[3].p = XMFLOAT3( 1.4f, 0, 1.4f );
    pVertices[3].c = LISTENER_COLOR;
    pVertices[4].p = XMFLOAT3( 0.0f, 0, 2.0f );
    pVertices[4].c = LISTENER_COLOR;
    pVertices[5].p = XMFLOAT3( -1.4f, 0, 1.4f );
    pVertices[5].c = LISTENER_COLOR;
    pVertices[6].p = XMFLOAT3( -2.0f, 0, 0.0f );
    pVertices[6].c = LISTENER_COLOR;
    pVertices[7].p = XMFLOAT3( -1.4f, 0, -1.4f );
    pVertices[7].c = LISTENER_COLOR;
    pVertices[8].p = XMFLOAT3( 0.0f, 0, -2.0f );
    pVertices[8].c = LISTENER_COLOR;
    m_pvbInnerRadius->Unlock();

    // Fill the VB for the source
    m_pvbSource->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    pVertices[0].p = XMFLOAT3( -0.5f, 0.0f, -0.5f );
    pVertices[0].c = SOURCE_COLOR;
    pVertices[1].p = XMFLOAT3( -0.5f, 0.0f, 0.5f );
    pVertices[1].c = SOURCE_COLOR;
    pVertices[2].p = XMFLOAT3( 0.5f, 0.0f, -0.5f );
    pVertices[2].c = SOURCE_COLOR;
    pVertices[3].p = XMFLOAT3( 0.5f, 0.0f, 0.5f );
    pVertices[3].c = SOURCE_COLOR;
    m_pvbSource->Unlock();

    // Fill the VB for the floor
    m_pvbFloor->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    pVertices[0].p = XMFLOAT3( ( FLOAT )XMIN, 0.0f, ( FLOAT )ZMIN );
    pVertices[0].c = FLOOR_COLOR;
    pVertices[1].p = XMFLOAT3( ( FLOAT )XMIN, 0.0f, ( FLOAT )ZMAX );
    pVertices[1].c = FLOOR_COLOR;
    pVertices[2].p = XMFLOAT3( ( FLOAT )XMAX, 0.0f, ( FLOAT )ZMIN );
    pVertices[2].c = FLOOR_COLOR;
    pVertices[3].p = XMFLOAT3( ( FLOAT )XMAX, 0.0f, ( FLOAT )ZMAX );
    pVertices[3].c = FLOOR_COLOR;
    m_pvbFloor->Unlock();

    // Fill the VB for the grid
    INT i, j;
    m_pvbGrid->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    for( i = ZMIN, j = 0; i <= ZMAX; ++i, ++j )
    {
        pVertices[ j * 2 + 0 ].p = XMFLOAT3( ( FLOAT )XMIN, 0, ( FLOAT )i );
        pVertices[ j * 2 + 0 ].c = GRID_COLOR;
        pVertices[ j * 2 + 1 ].p = XMFLOAT3( ( FLOAT )XMAX, 0, ( FLOAT )i );
        pVertices[ j * 2 + 1 ].c = GRID_COLOR;
    }
    for( i = XMIN; i <= XMAX; ++i, ++j )
    {
        pVertices[ j * 2 + 0 ].p = XMFLOAT3( ( FLOAT )i, 0, ( FLOAT )ZMIN );
        pVertices[ j * 2 + 0 ].c = GRID_COLOR;
        pVertices[ j * 2 + 1 ].p = XMFLOAT3( ( FLOAT )i, 0, ( FLOAT )ZMAX );
        pVertices[ j * 2 + 1 ].c = GRID_COLOR;
    }
    m_pvbGrid->Unlock();
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

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
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_THUMB )
    {
        PauseAudio( !m_bPlaying );

        m_bPlaying = !m_bPlaying;
    }

    // Cycle through sounds
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_dwCurrent = ( m_dwCurrent + 1 ) % NUM_SOUNDS;
        PrepareAudio( g_SOUND_NAMES[ m_dwCurrent ] );
    }

    // Switch which of source vs. listener we are moving
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_bControlSource = !m_bControlSource;
    }

    // toggle listener cone
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        g_audioState.fUseListenerCone = !g_audioState.fUseListenerCone;
    }

    // toggle inner radius
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        g_audioState.fUseInnerRadius = !g_audioState.fUseInnerRadius;
    }

    // Set up our colors
    m_dwSourceColor = SOURCE_COLOR | ( m_bControlSource ? cBlend : 0 );
    m_dwListenerColor = LISTENER_COLOR | ( !m_bControlSource ? cBlend : 0 );

    // Point to the appropriate vector
    XMVECTOR* pvControl = m_bControlSource ? &g_audioState.vEmitterPos : &g_audioState.vListenerPos;

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

    BOOL bUpdateFxParam = FALSE;
    if( pGamepad->bPressedLeftTrigger )
    {
        m_dwCurrentParam = ( m_dwCurrentParam + 1 ) % NUM_PRESETS;
        bUpdateFxParam = TRUE;
    }
    else if( pGamepad->bPressedRightTrigger )
    {
        m_dwCurrentParam = ( m_dwCurrentParam - 1 + NUM_PRESETS ) % NUM_PRESETS;
        bUpdateFxParam = TRUE;
    }

    if( bUpdateFxParam )
        SetReverb( m_dwCurrentParam );

    UpdateAudio( fElapsedTime );

    SetVolume( m_fVolume );

    return S_OK;
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

    // Draw the listener
    {
        XMMATRIX matTrans = XMMatrixTranslation( g_audioState.vListenerPos.x,
                                                 g_audioState.vListenerPos.y,
                                                 g_audioState.vListenerPos.z );
        XMMATRIX matRotate = XMMatrixRotationY( g_audioState.fListenerAngle );
        XMMATRIX matListener = XMMatrixMultiply( matRotate, matTrans );
        mat = XMMatrixTranspose( XMMatrixMultiply( matListener, m_matViewProj ) );

        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&mat, 4 );
        m_pd3dDevice->SetStreamSource( 0, m_pvbListener, 0, sizeof( D3DVERTEX ) );
        m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 1 );
        if (g_audioState.fUseListenerCone)
        {
            m_pd3dDevice->SetStreamSource( 0, m_pvbListenerCone, 0, sizeof( D3DVERTEX ) );
            m_pd3dDevice->DrawPrimitive( D3DPT_LINESTRIP, 0, 6 );
        }
        if (g_audioState.fUseInnerRadius)
        {
            m_pd3dDevice->SetStreamSource( 0, m_pvbInnerRadius, 0, sizeof( D3DVERTEX ) );
            m_pd3dDevice->DrawPrimitive( D3DPT_LINESTRIP, 0, 8 );
        }
    }

    // Draw the source
    {
        mat = XMMatrixTranslation( g_audioState.vEmitterPos.x,
                                   g_audioState.vEmitterPos.y,
                                   g_audioState.vEmitterPos.z );
        mat = XMMatrixMultiply( mat, m_matViewProj );
        mat = XMMatrixTranspose( mat );

        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&mat, 4 );
        m_pd3dDevice->SetStreamSource( 0, m_pvbSource, 0, sizeof( D3DVERTEX ) );
        m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );
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
        m_Font.DrawText( 0, 0, 0xffffffff, L"XAudio2Sound3D" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        FLOAT x1 = 16.0f;
        FLOAT x2 = x1 + m_Font.GetTextWidth( L"Listener: " );
        FLOAT y = 35.0f;

        // Show sound status
        m_Font.DrawText( x1, y, 0xffffffff, L"Sound: " );
        m_Font.DrawText( x2, y, m_bPlaying ? 0xffffff00 : 0xff808000, g_SOUND_NAMES[ m_dwCurrent ] );
        y += m_Font.GetFontHeight();

        // Show source position
        swprintf_s( strBuffer, L"<%0.1f, %0.1f, %0.1f>", g_audioState.vEmitterPos.x,
                    g_audioState.vEmitterPos.y,
                    g_audioState.vEmitterPos.z );
        m_Font.DrawText( x1, y, 0xffffffff, L"Source: " );
        m_Font.DrawText( x2, y, m_dwSourceColor, strBuffer );
        y += m_Font.GetFontHeight();

        // Show listener position
        swprintf_s( strBuffer, L"<%0.1f, %0.1f, %0.1f>", g_audioState.vListenerPos.x,
                    g_audioState.vListenerPos.y,
                    g_audioState.vListenerPos.z );
        m_Font.DrawText( x1, y, 0xffffffff, L"Listener: " );
        m_Font.DrawText( x2, y, m_dwListenerColor, strBuffer );
        y += m_Font.GetFontHeight();

        // Show volume (rounded to nearest dB)
        FLOAT fPercent = m_fVolume * 100.0f;
        FLOAT fDecibel;
        if( m_fVolume <= 0.01f )
            fDecibel = -85.f;
        else
            fDecibel = 20.0f * log10f( m_fVolume );
        swprintf_s( strBuffer, L"%3.0fdB (%0.0f%%)", fDecibel, fPercent );
        m_Font.DrawText( x1, y, 0xffffffff, L"Volume: " );
        m_Font.DrawText( x2, y, 0xffffff00, strBuffer );
        y += m_Font.GetFontHeight();

        // Scrunch up the font for non-widescreen displays
        if( 3 * m_Font.m_rcWindow.x1 == 4 * m_Font.m_rcWindow.y1 )
            m_Font.SetScaleFactors( 0.6f, 0.8f );
        x2 = x1 + m_Font.GetTextWidth( L"Coefficients: LFE:   " );

        m_Font.SetCursorPosition( x1, y );
        m_Font.DrawText( 0xffffffff, L"Coefficients:\n" );
        m_Font.DrawText( 0xffffffff, L"           L:\n" );
        m_Font.DrawText( 0xffffffff, L"           R:\n" );
        m_Font.DrawText( 0xffffffff, L"           C:\n" );
        m_Font.DrawText( 0xffffffff, L"           LFE:\n" );
        m_Font.DrawText( 0xffffffff, L"           Ls:\n" );
        m_Font.DrawText( 0xffffffff, L"           Rs:\n" );
        m_Font.DrawText( 0xff808080, L"Distance:\n" );
        m_Font.DrawText( 0xffffffff, L"Doppler factor:\n" );
        m_Font.DrawText( 0xff808080, L"LPF Direct:\n" );
        m_Font.DrawText( 0xff808080, L"LPF Reverb:\n" );
        m_Font.DrawText( 0xff808080, L"Reverb:\n" );
        m_Font.DrawText( 0xffffffff, L"FX parameter:         " );
        m_Font.DrawText( 0xffffff00, g_PRESET_NAMES[ m_dwCurrentParam ] );

        m_Font.SetCursorPosition( x2, y );
        m_Font.DrawText( 0xffffff00, L"\n" );
        swprintf_s( strBuffer, L"%.3f\n", g_audioState.dspSettings.pMatrixCoefficients[0] );
        m_Font.DrawText( 0xffffff00, strBuffer );
        swprintf_s( strBuffer, L"%.3f\n", g_audioState.dspSettings.pMatrixCoefficients[1] );
        m_Font.DrawText( 0xffffff00, strBuffer );
        swprintf_s( strBuffer, L"%.3f\n", g_audioState.dspSettings.pMatrixCoefficients[2] );
        m_Font.DrawText( 0xffffff00, strBuffer );
        swprintf_s( strBuffer, L"%.3f\n", g_audioState.dspSettings.pMatrixCoefficients[3] );
        m_Font.DrawText( 0xffffff00, strBuffer );
        swprintf_s( strBuffer, L"%.3f\n", g_audioState.dspSettings.pMatrixCoefficients[4] );
        m_Font.DrawText( 0xffffff00, strBuffer );
        swprintf_s( strBuffer, L"%.3f\n", g_audioState.dspSettings.pMatrixCoefficients[5] );
        m_Font.DrawText( 0xffffff00, strBuffer );
        swprintf_s( strBuffer, L"%.3f\n", g_audioState.dspSettings.EmitterToListenerDistance );
        m_Font.DrawText( 0xff808000, strBuffer );
        swprintf_s( strBuffer, L"%.3f\n", g_audioState.dspSettings.DopplerFactor );
        m_Font.DrawText( 0xffffff00, strBuffer );
        swprintf_s( strBuffer, L"%.3f\n", g_audioState.dspSettings.LPFDirectCoefficient );
        m_Font.DrawText( 0xff808000, strBuffer );
        swprintf_s( strBuffer, L"%.3f\n", g_audioState.dspSettings.LPFReverbCoefficient );
        m_Font.DrawText( 0xff808000, strBuffer );
        swprintf_s( strBuffer, L"%.3f\n", g_audioState.dspSettings.ReverbLevel );
        m_Font.DrawText( 0xff808000, strBuffer );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


