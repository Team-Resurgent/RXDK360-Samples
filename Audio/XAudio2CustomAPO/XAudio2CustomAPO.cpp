//--------------------------------------------------------------------------------------
// XAudio2CustomAPO.cpp
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//
// This file is the sample entry point. It's responsible for setting up the
// XAudio engine, voice and effects chain, and for handling input and drawing
// the UI.
//
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <fxl.h>
#include <AtgApp.h>
#include <AtgAudio.h>
#include <AtgDebugDraw.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include <d3dx9.h>
#include <xaudio2.h>

#include <ATGSpectralDisplay.h>
#include "SimpleAPO.h"
#include "UI.h"





//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
ATG::D3DDevice* g_pd3dDevice;

const INT nSamples = 256;

//
// UI layout
//
const D3DRECT g_rcBackground = { 0, 0, 640, 480 };
const int g_rackLeft = 19;
const int g_rackTop = 42;
const int g_rackSpaceHeight = 82;
const D3DRECT g_rcRackSpace = { 0, 0, 602, 82 };

//
// UI controls
//
const int nWidgets = 5;
IApoWidget* widgets[nWidgets];




//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_1, L"(Depends on selection)" },
    { ATG::HELP_RIGHTSTICK,   ATG::HELP_PLACEMENT_1, L"(Depends on selection)" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"(Depends on selection)" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_2, L"(Up/Down)Change selected effect" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_1, L"Display help" },


};
static const DWORD NUM_HELP_CALLOUTS = sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]);


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer   m_Timer;
    ATG::Font    m_FontArial;
    ATG::Font    m_FontFixed;
    ATG::Help    m_Help;
    BOOL         m_bDrawHelp;

	// Audio
    IXAudio2*               m_pXAudio2;
    IXAudio2MasteringVoice* m_pMasterVoice;
    IXAudio2SourceVoice*    m_pSourceVoice;
    XAUDIO2_BUFFER          m_waveBuffer;

    // Monitoring
    ATG::MonitorAPOPipe  m_monitorPre;
    ATG::MonitorAPOPipe  m_monitorPost;

    // APO
    SimpleAPOParams m_fxParams;

    // UI
    ATG::PackedResource m_resource;
    int                 m_currentWidget;
public:
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
    static Sample atgApp;
    atgApp.m_d3dpp.BackBufferWidth = 640;
    atgApp.m_d3dpp.BackBufferHeight = 480;
    atgApp.Run();
}

//--------------------------------------------------------------------------------------
// Name: LoadDemoWave
// Desc: Loads a buffer with wave content and creates a voice to play it
//--------------------------------------------------------------------------------------
HRESULT LoadDemoWave( IXAudio2* pXAudio2, const char* filename, IXAudio2SourceVoice** out_ppVoice, XAUDIO2_BUFFER* out_pWaveBuffer )
{
    HRESULT hr = S_OK;

    // Set up the default sound buffer values
    //
    XMemSet( out_pWaveBuffer, 0, sizeof(XAUDIO2_BUFFER ) );
    out_pWaveBuffer->LoopCount = XAUDIO2_LOOP_INFINITE;
    out_pWaveBuffer->Flags = XAUDIO2_END_OF_STREAM;

    // Load the wave file
    //
    ATG::WaveFile WaveFile;
    if( FAILED( hr = WaveFile.Open( "game:\\Media\\Sounds\\MusicMono.wav" ) ) )
        ATG::DebugSpew( "Error %#X opening WAV file\n", hr );

    // Read the format header
    //
    WAVEFORMATEXTENSIBLE wfx = {0};
    if( SUCCEEDED( hr ) )
    {
        if( FAILED(hr = WaveFile.GetFormat( &wfx) ) )
            ATG::DebugSpew( "Error %#X reading WAV format\n", hr );
    }

    // Read the sample data into memory
    //
    if( SUCCEEDED( hr ) )
    {
        WaveFile.GetDuration( (DWORD*)&out_pWaveBuffer->AudioBytes );
        out_pWaveBuffer->pAudioData = new BYTE[ out_pWaveBuffer->AudioBytes ];
        if( FAILED(hr = WaveFile.ReadSample( 0, (void*)out_pWaveBuffer->pAudioData, out_pWaveBuffer->AudioBytes, (DWORD*)&out_pWaveBuffer->AudioBytes ) ) )
            ATG::DebugSpew( "Error %#X reading WAV data\n", hr );
    }

    // Create the source voice
    //
    if( SUCCEEDED( hr ) )
    {
        if( FAILED(hr = pXAudio2->CreateSourceVoice( out_ppVoice, (WAVEFORMATEX*)&wfx ) ) )
            ATG::DebugSpew( "Error %#X creating source voice\n", hr );
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    g_pd3dDevice = m_pd3dDevice;
    m_currentWidget = 0;


    //
	// Create the audio engine
    //

    HRESULT hr = S_OK;
    UINT32 flags = 0;
#ifdef _DEBUG
    flags |= XAUDIO2_DEBUG_ENGINE;
#endif
    if ( FAILED(hr = XAudio2Create( &m_pXAudio2, flags ) ) )
        ATG::FatalError( "Error %#X calling XAudio2Create\n", hr );

    if ( FAILED(hr = m_pXAudio2->CreateMasteringVoice( &m_pMasterVoice ) ) )
        ATG::FatalError( "Error %#X calling CreateMasteringVoice\n", hr );

    if( FAILED( hr = LoadDemoWave( m_pXAudio2, "game:\\Media\\Sounds\\MusicMono.wav", &m_pSourceVoice, &m_waveBuffer ) ) )
        ATG::FatalError( "Can't play wave file" );

    //
    // Create the effect chain
    //
    CSimpleAPO* pSimpleAPO = NULL;
    CSimpleAPO::CreateInstance( NULL, 0, &pSimpleAPO );

    ATG::CMonitorAPO* pMonitorAPOPre = NULL;
    ATG::CMonitorAPO::CreateInstance( NULL, 0, &pMonitorAPOPre );

    ATG::CMonitorAPO* pMonitorAPOPost = NULL;
    ATG::CMonitorAPO::CreateInstance( NULL, 0, &pMonitorAPOPost );

    CComp1APO* pComp1APO = NULL;
    CComp1APO::CreateInstance( NULL, 0, &pComp1APO );

    CEqualizerAPO* pEQAPO = NULL;
    CEqualizerAPO::CreateInstance( NULL, 0, &pEQAPO );

	XAUDIO2_EFFECT_DESCRIPTOR apoDesc[5] = {0};
    apoDesc[0].InitialState = true;
    apoDesc[0].OutputChannels = 1;
    apoDesc[0].pEffect = static_cast<IXAPO*>(pMonitorAPOPre);
    apoDesc[1].InitialState = true;
    apoDesc[1].OutputChannels = 1;
    apoDesc[1].pEffect = static_cast<IXAPO*>(pSimpleAPO);
    apoDesc[2].InitialState = true;
    apoDesc[2].OutputChannels = 1;
    apoDesc[2].pEffect = static_cast<IXAPO*>(pComp1APO);
    apoDesc[3].InitialState = true;
    apoDesc[3].OutputChannels = 1;
    apoDesc[3].pEffect = static_cast<IXAPO*>(pEQAPO);
    apoDesc[4].InitialState = true;
    apoDesc[4].OutputChannels = 1;
    apoDesc[4].pEffect = static_cast<IXAPO*>(pMonitorAPOPost);

    XAUDIO2_EFFECT_CHAIN chain = {0};
    chain.EffectCount = sizeof(apoDesc) / sizeof(apoDesc[0]);
    chain.pEffectDescriptors = apoDesc;

    hr = m_pSourceVoice->SetEffectChain( &chain );

    // Don't need to keep them now that XAudio2 has ownership
    pSimpleAPO->Release();
    pMonitorAPOPre->Release();
    pMonitorAPOPost->Release();
    pComp1APO->Release();
	pEQAPO->Release();

	// Submit the buffer
    //
    if( FAILED(hr = m_pSourceVoice->SubmitSourceBuffer( &m_waveBuffer ) ) )
        ATG::FatalError( "Error %#X submitting source buffer\n", hr );
	m_pSourceVoice->Start(0);

    // Set initial effect params
    //
    m_fxParams.gain = 1.0f;
    if( FAILED( hr = m_pSourceVoice->SetEffectParameters( 1, &m_fxParams, sizeof( m_fxParams ) ) ) )
        ATG::FatalError( "Couldn't set effect parameters for SimpleAPO (%#X)", hr );

    ATG::MonitorAPOParams fxParamsMon;
    fxParamsMon.pipe = &m_monitorPre;
    if( FAILED( hr = m_pSourceVoice->SetEffectParameters( 0, &fxParamsMon, sizeof( fxParamsMon ) ) ) )
        ATG::FatalError( "Couldn't set effect parameters for Monitor APO (%#X)", hr );

    fxParamsMon.pipe = &m_monitorPost;
    if( FAILED( hr = m_pSourceVoice->SetEffectParameters( 4, &fxParamsMon, sizeof( fxParamsMon ) ) ) )
        ATG::FatalError( "Couldn't set effect parameters for Monitor APO (%#X)", hr );

	// Create the fonts
    //
    if( FAILED( m_FontArial.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( m_FontFixed.Create( "game:\\Media\\Fonts\\Fixedsys_12.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;


    // Confine text drawing to the title safe area
    m_FontArial.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    //
    m_bDrawHelp = FALSE;

    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    //
    // Create the UI Widgets
    //

    ATG::SimpleShaders::Initialize( NULL, NULL );
    if( FAILED( hr = m_resource.Create( "game:\\Media\\Resource.xpr" ) ) )
        ATG::FatalError( "Error %#X opening resource file\n", hr );

    int count = 0;
    widgets[count++] = new CMonitorApoWidget( &m_monitorPre );
    widgets[count++] = new CSimpleApoWidget( m_pSourceVoice, 1 );
    widgets[count++] = new CComp1ApoWidget( m_pSourceVoice, 2 );
    widgets[count++] = new CEQApoWidget( m_pSourceVoice,3 );
    widgets[count++] = new CMonitorApoWidget( &m_monitorPost );

    _ASSERT( count == nWidgets );

    for( int i = 0; i < nWidgets; ++i )
    {
        if( widgets[i] )
        {
            widgets[i]->Init( m_pd3dDevice, &m_resource );
        }
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT m_fElapsedTime = (FLOAT)m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    //
    // Handle input
    //

    // Help menu
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // DPAD up/down changes which widget is current
    else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        m_currentWidget = ++m_currentWidget % nWidgets;
    else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        m_currentWidget = ( --m_currentWidget + nWidgets ) % nWidgets;

    // Pass all other input to the current widget
    else if( widgets[m_currentWidget] )
        widgets[m_currentWidget]->HandleInput( pGamepad );

    for( int i = 0; i < nWidgets; ++i )
    {
        if( widgets[i] ) widgets[i]->Update( m_fElapsedTime );
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw the background
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( g_rcBackground, m_resource.GetTexture( "UI_Background" ) );

    int top = g_rackTop;

    //
    // Draw the APO widgets
    //
    for( int i = 0; i < nWidgets; ++i )
    {
        D3DRECT rcWidget = g_rcRackSpace;
        OffsetRect( (LPRECT)&rcWidget, g_rackLeft, top );
        if( widgets[i] )
            widgets[i]->Render( m_pd3dDevice, rcWidget );
        if( i == m_currentWidget )
        {
            ATG::DebugDraw::DrawScreenSpaceRect( rcWidget, 2.0f, 0x88aabbaa );
        }
        top += g_rcRackSpace.y2;
    }

    //
    // Show title, frame rate, and help
    //
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_FontArial, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_FontArial.Begin();

        m_FontArial.SetScaleFactors( 1.0f, 1.0f );
        m_FontArial.DrawText( 0, -20, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );


        m_FontArial.End();
    }

    m_pd3dDevice->UnsetAll();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

