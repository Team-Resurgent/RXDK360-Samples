//--------------------------------------------------------------------------------------
// XactCuePlayback.cpp
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xact3.h>
#include <vector>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
struct CueDescription
{
    const CHAR* strCueName;
    const WCHAR* strDisplayName;
    XACTINDEX CueIndex;
};
CueDescription g_CueDescriptions[] =
{
    { "firegun",  L"Fire Gun", 0 },
    { "footstep",  L"Footstep" , 0 },
    { "forest_animal",  L"Forest Animal", 0 },
};
const DWORD                         NUM_CUES = sizeof( g_CueDescriptions ) / sizeof( g_CueDescriptions[0] );

// This keeps track of a playing cue instance (for display purposes)
struct CueInstance
{
    IXACT3Cue* pCue;
    DWORD dwCueDescIndex;
    DWORD dwID;
    HANDLE hDone;
};
typedef std::vector <CueInstance*>  InstanceList;

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Trigger cue" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_1, L"Select cue" },
};
static const DWORD                  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );

//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    // XACT stuff
    IXACT3Engine* m_pXACTEngine;                  // XACT Engine instance
    IXACT3WaveBank* m_pWaveBank;                    // Wave bank
    IXACT3SoundBank* m_pSoundBank;                   // Sound bank
    InstanceList m_vActiveCues;
    DWORD m_dwSelectedCue;


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
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr = S_OK;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    m_bDrawHelp = FALSE;

    DWORD dwFileSize = 0;

    // Load the XACT global settings file
    VOID* pbGlobalSettings = NULL;
    if( FAILED( hr = ATG::LoadFile( "game:\\media\\sounds\\XactSounds.xgs",
                                    &pbGlobalSettings,
                                    &dwFileSize ) ) )
    {
        ATG::FatalError( "Could not load file \"XactSounds.xgs\"\n" );
    }

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
        ATG::FatalError( "Initialize failed with error %#X\n", hr );

    // Open the in memory wave bank
    VOID* pbWaveBank = NULL;
    if( FAILED( hr = ATG::LoadFilePhysicalMemory( "game:\\media\\sounds\\XactSounds.xwb",
                                                  &pbWaveBank,
                                                  &dwFileSize ) ) )
    {
        ATG::FatalError( "Could not load file \"XactSounds.xwb\"\n" );
    }

    // Register the wave bank with XACT
    if( FAILED( hr = m_pXACTEngine->CreateInMemoryWaveBank( pbWaveBank, dwFileSize, 0, 0, &m_pWaveBank ) ) )
        ATG::FatalError( "CreateInMemoryWaveBank failed with error %#X\n", hr );

    // Load the sound bank
    VOID* pbSoundBank = NULL;
    if( FAILED( hr = ATG::LoadFile( "game:\\media\\sounds\\XactSounds.xsb",
                                    &pbSoundBank,
                                    &dwFileSize ) ) )
    {
        ATG::FatalError( "Could not load file \"XactSounds.xsb\"\n" );
    }

    // Register the sound bank with XACT
    if( FAILED( hr = m_pXACTEngine->CreateSoundBank( pbSoundBank, dwFileSize, 0, 0, &m_pSoundBank ) ) )
        ATG::FatalError( "CreateSoundBank failed with error %#X\n", hr );

    // Look up the index for each of our cues
    for( DWORD i = 0; i < NUM_CUES; ++i )
    {
        g_CueDescriptions[i].CueIndex = m_pSoundBank->GetCueIndex( g_CueDescriptions[i].strCueName );
        if( g_CueDescriptions[i].CueIndex == XACTINDEX_INVALID )
            ATG::FatalError( "GetCueIndex failed\n" );
    }

    m_dwSelectedCue = 0;

    return S_OK;
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

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
    {
        m_dwSelectedCue = ( m_dwSelectedCue + 1 ) % NUM_CUES;
    }
    else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
    {
        m_dwSelectedCue = ( m_dwSelectedCue + NUM_CUES - 1 ) % NUM_CUES;
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        IXACT3Cue* pCue = NULL;

        if( FAILED( m_pSoundBank->Play( g_CueDescriptions[ m_dwSelectedCue ].CueIndex,
                                        0,
                                        0,
                                        &pCue ) ) )
        {
            // Attempting to play a cue could fail for several good reasons - usually
            // because the sound designer has limited the number of concurrent instances
            // of a given sound/cue
            ATG::DebugSpew( "Failed to play cue %S\n",
                            g_CueDescriptions[ m_dwSelectedCue ].strDisplayName );
        }
        else
        {
            static DWORD dwIDCounter = 0;

            // Create a new instance to track this cue
            CueInstance* pInstance = new CueInstance();
            pInstance->pCue = pCue;
            pInstance->dwCueDescIndex = m_dwSelectedCue;
            pInstance->dwID = dwIDCounter++;
            pInstance->hDone = CreateEvent( NULL, FALSE, FALSE, NULL );

            // Initialize XACT notification struct
            XACT_NOTIFICATION_DESCRIPTION xactNotificationDesc = { 0 };
            xactNotificationDesc.type = XACTNOTIFICATIONTYPE_CUESTOP;
            xactNotificationDesc.pCue = pInstance->pCue;
            xactNotificationDesc.pvContext = pInstance;
            xactNotificationDesc.cueIndex = XACTINDEX_INVALID;
            xactNotificationDesc.pvContext = pInstance->hDone;

            if( NULL == xactNotificationDesc.pvContext )
            {
                ATG::FatalError( "Failed to create event object for XACTNOTIFICATIONTYPE_CUESTOP notification" );
            }

            // Register a stop notification with the XACT .
            // This will allow us to monitor when the cue stops playing.
            if( FAILED( m_pXACTEngine->RegisterNotification( &xactNotificationDesc ) ) )
            {
                delete pInstance;
                ATG::FatalError( "Notification registration failed \n" );
            }
            else
            {
                m_vActiveCues.push_back( pInstance );
            }
        }
    }

    //
    // check which cue instances are done and remove them from the internal list
    //    
    InstanceList::iterator it;
    for( it = m_vActiveCues.begin(); it != m_vActiveCues.end(); ++it )
    {

        CueInstance* pInstance = *it;
        //
        // if signaled then this cue is done and we remove from the list
        //
        if( WAIT_OBJECT_0 == WaitForSingleObject( pInstance->hDone, 0 ) )
        {

            pInstance->pCue->Destroy();
            CloseHandle( pInstance->hDone );
            m_vActiveCues.erase( it );
            delete pInstance;
            break;

        }

    }

    // Tell the XACT engine to do its work
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

    FLOAT fY = 72;
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.0f, 1.0f );

    WCHAR str[ 128 ];
    swprintf_s( str, L"Press " GLYPH_A_BUTTON L" to trigger " \
                   GLYPH_LEFT_ARROW L"%s" GLYPH_RIGHT_ARROW,
                   g_CueDescriptions[ m_dwSelectedCue ].strDisplayName );
    m_Font.DrawText( 48, fY, 0xffffffff, str );
    for( DWORD i = 0; i < m_vActiveCues.size(); ++i )
    {
        fY += 30;

        CueInstance* pInstance = m_vActiveCues[i];
        CueDescription* pDesc = &g_CueDescriptions[ pInstance->dwCueDescIndex ];
        swprintf_s( str, L"[%d] %s", pInstance->dwID, pDesc->strDisplayName );
        m_Font.DrawText( 48, fY, 0xffffff00, str );
    }
    m_Font.End();


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
        m_Font.DrawText( 0, 0, 0xffffffff, L"XactCuePlayback" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
