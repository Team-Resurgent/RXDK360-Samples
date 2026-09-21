//--------------------------------------------------------------------------------------
// XAudio2VoiceReuse.cpp
//
// XAudio2VoiceReuse is a sample that demonstrates reusing XAudio2 voices, 
// in order to avoid the overhead of stopping and stopping voices frequently
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
#include "XAudio2VoiceReuse.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Fire Gun"  },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_1, L"Queue Music"  },
    { ATG::HELP_Y_BUTTON,     ATG::HELP_PLACEMENT_1, L"Queue Ambient"  },
    { ATG::HELP_RIGHT_TRIGGER,     ATG::HELP_PLACEMENT_2, L"R Trigger\nPri: High"  },
    { ATG::HELP_LEFT_TRIGGER,     ATG::HELP_PLACEMENT_2, L"L Trigger\nPri: Low"  },
    { ATG::HELP_LEFT_SHOULDER,     ATG::HELP_PLACEMENT_1, L"Rapid Fire"  }
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//-----------------------------------------------------------------------------
// Global variable declarations
//-----------------------------------------------------------------------------

WCHAR*              g_GUN_SOUNDS[] =
{
    L"Pistol-1.wav",
    L"Pistol-2.wav",
    L"Pistol-3.wav",
    L"Pistol-4.wav",
    L"Pistol-5.wav",
    L"Pistol-6.wav",
    L"Pistol-7.wav"
};
const DWORD         NUM_GUN_SOUNDS = sizeof( g_GUN_SOUNDS ) / sizeof( g_GUN_SOUNDS[ 0 ] );

WCHAR*              g_MUSIC_SOUNDS[] =
{
    L"Music-01.xma2",
    L"Music-02.xma2",
    L"Music-03.xma2"
};
const DWORD         NUM_MUSIC_SOUNDS = sizeof( g_MUSIC_SOUNDS ) / sizeof( g_MUSIC_SOUNDS[ 0 ] );

WCHAR*              g_AMBIENT_SOUNDS[] =
{
    L"Ambient-01.xma2",
    L"Ambient-02.xma2",
    L"Ambient-03.xma2",
    L"Ambient-04.xma2",
    L"Ambient-05.xma2",
    L"Ambient-06.xma2",
    L"Ambient-07.xma2",
    L"Ambient-08.xma2",
    L"Ambient-09.xma2",
    L"Ambient-10.xma2",
    L"Ambient-11.xma2"
};
const DWORD         NUM_AMBIENT_SOUNDS = sizeof( g_AMBIENT_SOUNDS ) / sizeof( g_AMBIENT_SOUNDS [ 0 ] );


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


    PrioritizedVoices* m_pMusic;
    PrioritizedVoices* m_pGuns;
    PrioritizedVoices* m_pAmbient;
    IXAudio2* m_pXAudio2;

    int m_iRoundsRemaining;
    int m_iCurrentPriority;

    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    // Utility sample methods
    void DrawVoiceData(const WCHAR* Name, PrioritizedVoices* Voices, float ConsoleXOffset, float ChartYOffset);
    void PlayGunshot();
};


//--------------------------------------------------------------------------------------
// Name: Sample()
// Desc: Constructor
//--------------------------------------------------------------------------------------
Sample::Sample()
    : m_bDrawHelp( false ),
      m_pMusic( NULL ),
      m_pGuns( NULL ),
      m_pAmbient( NULL ),
      m_pXAudio2( NULL ),
      m_iRoundsRemaining( 7 ),
      m_iCurrentPriority( 1 )
{
}


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
// Name: Initialize
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

    //
    // Initialize XAudio2
    //
    UINT32 flags = 0;
#ifdef _DEBUG
    flags |= XAUDIO2_DEBUG_ENGINE;
#endif

    if( FAILED( hr = XAudio2Create( &m_pXAudio2, flags ) ) )
        ATG::FatalError( "Error %#X calling XAudio2Create\n", hr );

    //
    // Create a mastering voice
    //
    IXAudio2MasteringVoice* pMasteringVoice = NULL;
    if( FAILED( hr = m_pXAudio2->CreateMasteringVoice( &pMasteringVoice ) ) )
        ATG::FatalError( "Error %#X calling CreateMasteringVoice\n", hr );

    m_pMusic = PrioritizedVoices::Create(m_pXAudio2, 44100, 2, TypeXMA, 1,XAUDIO2_VOICE_NOPITCH);
    m_pAmbient = PrioritizedVoices::Create(m_pXAudio2, 44100, 1, TypeXMA, 3,MAX_PITCH);
    m_pGuns = PrioritizedVoices::Create(m_pXAudio2, 44100, 1, TypePCM, 3, MAX_PITCH);

    return S_OK;
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

    // Store the priority, based on Left/Right trigger for lower/higher priority
    m_iCurrentPriority = 1;
    if (pGamepad->bLeftTrigger > 128) 
        m_iCurrentPriority++;
    if (pGamepad->bRightTrigger > 128) 
        m_iCurrentPriority--;

    // A Button: Fire a gunshot for each unique press of the button
    if (pGamepad->wPressedButtons & XINPUT_GAMEPAD_A)
        PlayGunshot();

    // A Button: If the user is holding down the right bumper for "rapidshot",
    //          play the sound while the A button is pressed.
    else if (pGamepad->wLastButtons & XINPUT_GAMEPAD_LEFT_SHOULDER &&
         pGamepad->wLastButtons & XINPUT_GAMEPAD_A)
    {
        //Sleep for a few milliseconds, since the gunshots are now rapid-shot.
        Sleep(50);

        PlayGunshot();
    }
    
    // X Button: Play music
    if (pGamepad->wPressedButtons & XINPUT_GAMEPAD_X)
        m_pMusic->Play(m_iCurrentPriority,1.0f, g_MUSIC_SOUNDS[Random(0,NUM_MUSIC_SOUNDS-1)]);

    // Y Button: Play ambient sounds
    if (pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y)
        m_pAmbient->Play(m_iCurrentPriority,Random(.90f,1.10f), g_AMBIENT_SOUNDS[Random(0,NUM_AMBIENT_SOUNDS-1)]);

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: PlayGunshot
// Desc: Plays the gunshots, and handles playing reload sounds.
//--------------------------------------------------------------------------------------
void Sample::PlayGunshot()
{
    if (--m_iRoundsRemaining < 0)
    {
        // Play the reload sound every 7 shots.
        m_pGuns->Play(0,Random(.85f,1.15f), g_GUN_SOUNDS[0]);
        m_iRoundsRemaining=7;
    }
    else
    {
        // Otherwise, play a random gunshot sound.
        m_pGuns->Play(m_iCurrentPriority,Random(.95f,1.05f), g_GUN_SOUNDS[Random(1,NUM_GUN_SOUNDS-1)]);
    }
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
        // Draw the title text and table headers
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"XAudio2 Voice Reuse" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.SetScaleFactors( 1.1f, 1.1f );
        m_Font.DrawText(40,50, 0xffffffff, L"Pool");
        m_Font.DrawText(160,50, 0xffffffff, L"Max",ATGFONT_RIGHT);
        m_Font.DrawText(230,50, (m_iCurrentPriority==2)?0xffffff00:0xff7f7f7f, L"Pri2",ATGFONT_RIGHT);
        m_Font.DrawText(300,50, (m_iCurrentPriority==1)?0xffffff00:0xff7f7f7f, L"Pri1",ATGFONT_RIGHT);
        m_Font.DrawText(370,50, (m_iCurrentPriority==0)?0xffffff00:0xff7f7f7f, L"Pri0",ATGFONT_RIGHT);
        m_Font.DrawText(440,50, 0xffffffff, L"Total" ,ATGFONT_RIGHT);
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        
        // Display data for each voice pool
        DrawVoiceData(GLYPH_A_BUTTON L" Guns", m_pGuns,      0.0f,  75.0f);
        DrawVoiceData(GLYPH_X_BUTTON L" Music",m_pMusic,   350.0f, 100.0f);
        DrawVoiceData(GLYPH_Y_BUTTON L" Amb.", m_pAmbient, 650.0f, 125.0f);

        m_Font.End();


    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DrawVoiceData
// Desc: Helper function which displays the various counts and diagnostic output
//--------------------------------------------------------------------------------------
void Sample::DrawVoiceData(const WCHAR* Name, PrioritizedVoices* Voices, float ConsoleXOffset, float ChartYOffset)
{
    WCHAR str[120];
    int* iPlayingCounts = Voices->GetPlayingCounts();

    // Draw the summary table entry for this voice pool.
    m_Font.SetScaleFactors( 1.1f, 1.1f );
    m_Font.DrawText(0,ChartYOffset, 0xffffffff,Name);
    swprintf_s( str, L"%d", Voices->m_iMaxVoiceCount );m_Font.DrawText(160,ChartYOffset,0xffffffff, str,ATGFONT_RIGHT);
    swprintf_s( str, L"%d", iPlayingCounts[2]);m_Font.DrawText(230,ChartYOffset, 0xffffffff, str,ATGFONT_RIGHT);
    swprintf_s( str, L"%d", iPlayingCounts[1]);m_Font.DrawText(300,ChartYOffset, 0xffffffff, str,ATGFONT_RIGHT);
    swprintf_s( str, L"%d", iPlayingCounts[0]);m_Font.DrawText(370,ChartYOffset, 0xffffffff, str,ATGFONT_RIGHT);
    swprintf_s( str, L"%d", iPlayingCounts[0]+iPlayingCounts[1]+iPlayingCounts[2]);m_Font.DrawText(440,ChartYOffset, 0xffffffff, str,ATGFONT_RIGHT);


   // Draw the log entries; Offset the Name by two characters to exclude the button glyph
    m_Font.DrawText(ConsoleXOffset,225, 0xffffffff,Name+2);
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.SetCursorPosition(ConsoleXOffset,250);

    // The CurrentLogIndex points to the latest entry in a circular array. 
    // We loop through display all of the logs, starting with the oldest (current index)
    for(int i=0;i<MAX_LOG_ENTRIES;i++)
    {
        LogEntry* e = &Voices->m_ppLogs[(Voices->m_iCurrentLogIndex+i)%MAX_LOG_ENTRIES];
        if (e->VoiceIndex == LOG_EMPTY)
            continue; 
        else if (e->VoiceIndex == LOG_VOICE_UNPLAYED)
        {
            swprintf_s( str, L"No voices for Pri %d\n", e->NewPri);m_Font.DrawText(0xffffffff, str);
        }
        else if (e->OldPri == -1)
        {
            swprintf_s( str, L"Pri %d played in #%d\n", e->NewPri, e->VoiceIndex);m_Font.DrawText(0xffffffff, str);
        }
        else if (e->NewPri <= e->OldPri)
        {
            swprintf_s( str, L"Pri %d overrode Pri %d in #%d\n", e->NewPri, e->OldPri, e->VoiceIndex);m_Font.DrawText(0xffffffff, str);
        }
        else
        {
            ATG::FatalError( "Unexpected log entry.\n");
        }
    }
}

int Random(int min, int max)
{
    assert(min<max);
    return rand()%(max-min+1)+min;
}
float Random(float min, float max)
{
    assert(min<max);
    return (((float)rand()/(float)RAND_MAX)*(max-min)) + min;
}
