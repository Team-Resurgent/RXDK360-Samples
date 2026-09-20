//--------------------------------------------------------------------------------------
// XactMarkers.cpp
//
// XACTMarkers is a sample that demonstrates how to use XACT markers and notification
// features.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xact3.h>
#include <AtgApp.h>
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
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Restart Play"  },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//-----------------------------------------------------------------------------
// Global variable declarations
//-----------------------------------------------------------------------------
WCHAR*              g_rgWordArray[] =
{
    L"And ",
    L"the ",
    L"receiving ",
    L"team ",
    L"comes ",
    L"up ",
    L"with ",
    L"the ",
    L"football. ",
    L"That ",
    L"should ",
    L"do ",
    L"it, ",
    L"they ",
    L"didn't ",
    L"recover ",
    L"the ",
    L"on-side ",
    L"attempt. ",
};
const DWORD         NUM_WORDS = sizeof( g_rgWordArray ) / sizeof( g_rgWordArray[0] );
const DWORD         NUM_WORDS_PER_LINE = 7;
const FLOAT         LINE_SPACING = 50.0f;
const FLOAT         WORD_SPACING = 16.0f;


//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
                    Sample();
                    ~Sample()
                    {
                    };

private:
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    IXACT3Engine* m_pXACTEngine;          // XACT Engin instance

    VOID* m_pbWaveBank;           // Wave Bank data
    IXACT3WaveBank* m_pWaveBank;            // XACT Wave Bank

    VOID* m_pbSoundBank;          // Sound Bank data
    IXACT3SoundBank* m_pSoundBank;           // XACT Sound Bank

    XACTINDEX m_dwSoundCueIndex;      // Sound cue index
    IXACT3Cue* m_pCue;                 // Cue instance
    HANDLE m_hCueStop;

    HANDLE m_hMarker;              // signaled each time a marker fires
    DWORD m_dwWordCount;          // Number of words to draw

    static void     XACTNotificationCallback( const XACT_NOTIFICATION* pNotification );

    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    // Utility sample methosd
    HRESULT         ResetState();
    VOID            DrawWords();

};


//--------------------------------------------------------------------------------------
// Name: Sample()
// Desc: Constructor
//--------------------------------------------------------------------------------------
Sample::Sample() : ATG::Application()
{

    m_pbWaveBank = NULL;
    m_pWaveBank = NULL;
    m_pbSoundBank = NULL;
    m_pSoundBank = NULL;
    m_dwWordCount = 0;
    m_dwSoundCueIndex = 0;
    m_pCue = NULL;
    m_hCueStop = NULL;
    m_bDrawHelp = FALSE;
    m_hMarker = NULL;

}


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize
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
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Load the XACT global settings file
    DWORD dwFileSize = 0;
    VOID* pbGlobalSettings = NULL;
    HRESULT hr;
    if( FAILED( hr = ATG::LoadFile( "game:\\media\\sounds\\XactSounds.xgs",
                                    &pbGlobalSettings,
                                    &dwFileSize ) ) )
        ATG::FatalError( "Could not load file \"XactSounds.xgs\", failed with error %#X\n", hr );

    // Create XACT Engine
    if( FAILED( hr = XACT3CreateEngine( 0, &m_pXACTEngine ) ) )
        ATG::FatalError( "Could not create a XACT Engine, failed with error %#X\n", hr );

    // Initialize the XACT runtime parameters
    XACT_RUNTIME_PARAMETERS xrParams = { 0 };
    xrParams.pGlobalSettingsBuffer = pbGlobalSettings;
    xrParams.globalSettingsBufferSize = dwFileSize;
    xrParams.fnNotificationCallback = &this->XACTNotificationCallback;
    xrParams.lookAheadTime = XACT_ENGINE_LOOKAHEAD_DEFAULT;

    // Create the XACT runtime engine
    hr = m_pXACTEngine->Initialize( &xrParams );
    if( FAILED( hr ) )
        ATG::FatalError( "m_pXACTEngine->Initialize failed with error %#X\n", hr );

    HANDLE hFile;
    // Open the in memory wave bank
    hFile = CreateFile( "game:\\media\\sounds\\XactSounds.xwb",
                        GENERIC_READ, 0, 0, OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, 0 );
    if( hFile == INVALID_HANDLE_VALUE )
        ATG::FatalError( "Loading file failed with error %#X\n", hr );

    XACT_WAVEBANK_STREAMING_PARAMETERS wbParams = { 0 };
    wbParams.file = hFile;
    wbParams.packetSize = 16;

    // Register the streaming wave bank with XACT
    if( FAILED( hr = m_pXACTEngine->CreateStreamingWaveBank( &wbParams, &m_pWaveBank ) ) )
        ATG::FatalError( "m_pXACTEngine->CreateStreamingWaveBank failed with error %#X\n", hr );

    // Wait until wave bank is prepared
    DWORD dwState;
    do
    {
        m_pXACTEngine->DoWork();
        m_pWaveBank->GetState( &dwState );
    } while( !( dwState & XACT_WAVEBANKSTATE_PREPARED ) );


    // Load the sound bank
    if( FAILED( hr = ATG::LoadFile( "game:\\media\\sounds\\XactSounds.xsb", &m_pbSoundBank, &dwFileSize ) ) )
        ATG::FatalError( "Loading file failed with error %#X\n", hr );

    // Register the sound bank with XACT
    if( FAILED( hr = m_pXACTEngine->CreateSoundBank( m_pbSoundBank, dwFileSize, 0, 0, &m_pSoundBank ) ) )
        ATG::FatalError( "m_pXACTEngine->CreateSoundBank failed with error %#X\n", hr );

    // Get the sound cue index from the sound bank
    m_dwSoundCueIndex = m_pSoundBank->GetCueIndex( "XACT_marker" ); // Null-terminated string representing the friendly
    if( m_dwSoundCueIndex == XACTINDEX_INVALID )
        ATG::FatalError( "GetCueIndex failed\n" );

    // cue stop event
    m_hCueStop = CreateEvent( NULL, FALSE, FALSE, NULL );
    if( NULL == m_hCueStop )
    {
        ATG::FatalError( "Failed to create event object for XACTNOTIFICATIONTYPE_CUESTOP notification" );
    }

    // marker event
    m_hMarker = CreateEvent( NULL, FALSE, FALSE, NULL );
    if( NULL == m_hMarker )
    {
        ATG::FatalError( "Failed to create event object for XACTNOTIFICATIONTYPE_MARKER notification" );
    }

    // Initialize XACT notification struct
    XACT_NOTIFICATION_DESCRIPTION xactNotificationDesc = { 0 };
    xactNotificationDesc.type = XACTNOTIFICATIONTYPE_MARKER;
    xactNotificationDesc.flags = XACT_FLAG_NOTIFICATION_PERSIST;
    xactNotificationDesc.pSoundBank = m_pSoundBank;
    xactNotificationDesc.cueIndex = m_dwSoundCueIndex;
    xactNotificationDesc.pvContext = ( PVOID )m_hMarker;

    // Register a stop notification with the XACT .
    // This will allow us to monitor when the cue stops playing.
    if( FAILED( hr = m_pXACTEngine->RegisterNotification( &xactNotificationDesc ) ) )
        ATG::FatalError( "Notification registration failed with error %#X\n", hr );

    // Reset state for drawing word array
    ResetState();

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: XACTNotificationCallback()
// Desc: Received notifications from the XACT engine.  Assume that the pvContext
//       is an event handle which is signaled.
//--------------------------------------------------------------------------------------
void Sample::XACTNotificationCallback( const XACT_NOTIFICATION* pNotification )
{
    //
    // is our cue done playing?
    //
    if( pNotification->type == XACTNOTIFICATIONTYPE_CUESTOP )
    {
        SetEvent( ( HANDLE )pNotification->pvContext );
    }
    //
    // marker received? if so we increment the word count
    //
    if( pNotification->type == XACTNOTIFICATIONTYPE_MARKER )
    {
        SetEvent( ( HANDLE )pNotification->pvContext );
    }
}

//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Reset state for drawing word array
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_pCue->Destroy();
        ResetState();
    }

    if( WAIT_OBJECT_0 == WaitForSingleObject( m_hCueStop, 4 ) )
    {
        ResetEvent( m_hCueStop );
    }

    if( WAIT_OBJECT_0 == WaitForSingleObject( m_hMarker, 4 ) )
    {
        ResetEvent( m_hMarker );
        m_dwWordCount++;
    }

    // Pump XACT's work queue
    m_pXACTEngine->DoWork();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"XactMarkers" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();

        // Draw words from word array according to the number of 
        // markers which have fired thus far.
        DrawWords();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Sample::ResetState()
// Desc: Reset the state for the drawing word array
//-----------------------------------------------------------------------------
HRESULT Sample::ResetState()
{
    HRESULT hr = S_OK;

    // Play the sound cue
    if( FAILED( hr = m_pSoundBank->Play( m_dwSoundCueIndex, 0, 0, &m_pCue ) ) )
    {
        // Attempting to play a cue could fail for several good reasons - usually
        // because the sound designer has limited the number of concurrent instances
        // of a given sound/cue
        ATG::DebugSpew( "Failed to play cue\n" );
    }
    else
    {
        // Reset the word count
        m_dwWordCount = 0;

        XACT_NOTIFICATION_DESCRIPTION xactNotificationDesc = { 0 };
        xactNotificationDesc.type = XACTNOTIFICATIONTYPE_CUESTOP;
        xactNotificationDesc.pCue = m_pCue;
        xactNotificationDesc.cueIndex = XACTINDEX_INVALID;
        xactNotificationDesc.pvContext = ( PVOID )m_hCueStop;

        // Register a stop notification with the XACT .
        // This will allow us to monitor when the cue stops playing.
        if( FAILED( m_pXACTEngine->RegisterNotification( &xactNotificationDesc ) ) )
        {
            ATG::FatalError( "Notification registration failed \n" );
        }
    }
    return hr;
}


//-----------------------------------------------------------------------------
// Name: Sample::DrawWords()
// Desc: Draw words from word array according to the number of markers which 
//       have fired thus far.
//-----------------------------------------------------------------------------
VOID Sample::DrawWords()
{
    FLOAT fLineSpace = LINE_SPACING;

    // Loop through the word array and display as many words
    // as we have recieved markers for.
    for( DWORD dwIndex = 0; dwIndex < m_dwWordCount; dwIndex++ )
    {
        // Adjust to next line
        if( !( dwIndex % NUM_WORDS_PER_LINE ) )
        {
            // Draw an empty string to reset to beginning of new line
            fLineSpace += LINE_SPACING;
            m_Font.DrawText( WORD_SPACING, fLineSpace, 0xFF00FF00, L" " );
        }

        // Draw the word
        m_Font.DrawText( 0xFF00FF00, g_rgWordArray[ dwIndex ] );
    }
}
