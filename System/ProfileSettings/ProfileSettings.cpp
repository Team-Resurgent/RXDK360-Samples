//--------------------------------------------------------------------------------------
// ProfileSettings.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <AtgApp.h>
#include <AtgHelp.h>
#include <AtgUtil.h>
#include <AtgInput.h>
#include <AtgSignIn.h>
#include "AtgSimpleShaders.h"
#include "AtgDebugDraw.h"


//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
const DWORD STEXT_COLOR_RW = 0xffffffff;       // Value text color for writeable profiles
const DWORD VTEXT_COLOR_RW = 0xffffff00;       // Source text color for writeable profiles
const DWORD STEXT_COLOR_RO = 0xff7f7f7f;       // Value text color for read only profiles
const DWORD VTEXT_COLOR_RO = 0xff7f7f7f;       // Source text color for read only profiles


// Some settings for this sample
enum SETTING
{
    Setting_Gamer_Achievements = 0,
    Setting_Gamer_Creds,
    Setting_Gamer_Motto,
    Setting_Gamer_Region,
    Setting_Gamer_Reputation,
    Setting_Gamer_Titles_Played,
    Setting_Gamer_Zone,
    Setting_Option_Voice_Muted,
    Setting_Option_Voice_Speakers,
    Setting_Option_Voice_Volume,
    Setting_Game_Difficulty,
    Setting_Auto_Aim,
    Setting_Auto_Center,
    Setting_Thumbstick_Control,
    Setting_YAxis_Inversion,
    Setting_Race_Transmission_Type,
    Setting_Race_Camera_Location,
    Setting_Race_Brake_Control,
    Setting_Race_Accelerator_Control,
    Setting_Title_Achievements,
    Setting_Title_Creds,
    Setting_Controller_Vibration,
    Setting_Gamer_Picture,
    Setting_Title1,
    Setting_Title2,
    Setting_Title3,
    NUM_SETTINGS
};

const DWORD SETTINGS_PER_SCREEN = 5;            // Number of settings that can be displayed at once


// Define the game setting IDs the sample will display
// Must match the Setting enumeration above
const DWORD SettingIDs[ NUM_SETTINGS ] =
{
    XPROFILE_GAMERCARD_ACHIEVEMENTS_EARNED,
    XPROFILE_GAMERCARD_CRED,
    XPROFILE_GAMERCARD_MOTTO,
    XPROFILE_GAMERCARD_REGION,
    XPROFILE_GAMERCARD_REP,
    XPROFILE_GAMERCARD_TITLES_PLAYED,
    XPROFILE_GAMERCARD_ZONE,
    XPROFILE_OPTION_VOICE_MUTED,
    XPROFILE_OPTION_VOICE_THRU_SPEAKERS,
    XPROFILE_OPTION_VOICE_VOLUME,
    XPROFILE_GAMER_DIFFICULTY,
    XPROFILE_GAMER_ACTION_AUTO_AIM,
    XPROFILE_GAMER_ACTION_AUTO_CENTER,
    XPROFILE_GAMER_ACTION_MOVEMENT_CONTROL,
    XPROFILE_GAMER_YAXIS_INVERSION,
    XPROFILE_GAMER_RACE_TRANSMISSION,
    XPROFILE_GAMER_RACE_CAMERA_LOCATION,
    XPROFILE_GAMER_RACE_BRAKE_CONTROL,
    XPROFILE_GAMER_RACE_ACCELERATOR_CONTROL,
    XPROFILE_GAMERCARD_TITLE_ACHIEVEMENTS_EARNED,
    XPROFILE_GAMERCARD_TITLE_CRED_EARNED,
    XPROFILE_OPTION_CONTROLLER_VIBRATION,
    XPROFILE_GAMERCARD_PICTURE_KEY,
    XPROFILE_TITLE_SPECIFIC1,
    XPROFILE_TITLE_SPECIFIC2,
    XPROFILE_TITLE_SPECIFIC3
};


// For display
// Must match the Setting enumeration above
WCHAR*      g_strDesc[ NUM_SETTINGS ] =
{
    L"Gamer Achievements",
    L"Gamer Creds",
    L"Gamer Motto",
    L"Gamer Region",
    L"Gamer Reputation",
    L"Gamer Titles Played",
    L"Gamer Zone",
    L"Voice Muted",
    L"Voice Thru Speaker",
    L"Voice Volume",
    L"Game Difficulty",
    L"Auto Aim",
    L"Auto Center",
    L"Movement Control",
    L"Y Axis Inversion",
    L"Transmission Type",
    L"Race Camera Location",
    L"Brake Control",
    L"Accelerator Control",
    L"Title Achievements",
    L"Title Creds",
    L"Controller Vibration",
    L"Gamer Picture",
    L"Title Setting 1",
    L"Title Setting 2",
    L"Title Setting 3"
};

// Define access to profiles R/O - FALSE, R/W - TRUE
const BOOL g_bWriteAccess[ NUM_SETTINGS ] =
{
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    TRUE,
    TRUE,
    TRUE
};


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_DPAD,        ATG::HELP_PLACEMENT_1, L"Change Setting" },
    { ATG::HELP_B_BUTTON,    ATG::HELP_PLACEMENT_1, L"Signin" },
    { ATG::HELP_X_BUTTON,    ATG::HELP_PLACEMENT_1, L"Save Profile" },
    { ATG::HELP_Y_BUTTON,    ATG::HELP_PLACEMENT_2, L"Show\nGamercard UI" },
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts)/sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Font m_Font;
    ATG::Help m_Help;
    ATG::Timer m_Timer;
    BOOL m_bDrawHelp;
    BOOL m_bNeedToReadSettings;
    HANDLE m_hSystemListener; // listener to accept notifications
    LPDIRECT3DTEXTURE9 m_pGamerPictureTexture;

    // Valid app states
    enum APPSTATE
    {
        APPSTATE_SETTINGS_MENU,     // Main settings page
        APPSTATE_READ_SETTINGS,     // Read the player game settings
        APPSTATE_WRITE_SETTINGS,    // Write the player game settings
        APPSTATE_MAX,
    };

    APPSTATE m_AppState;

    ATG::GAMEPAD* m_pGamepad;
    INT m_nMenuItem;
    XOVERLAPPED m_Overlapped;

    // Settings data
    DWORD m_dwSettingSizeMax;
    XUSER_PROFILE_SETTING   m_Settings[ NUM_SETTINGS ];
    XUSER_PROFILE_SETTING   m_WriteableSettings[ NUM_SETTINGS ];
    XUSER_READ_PROFILE_SETTING_RESULT* m_pSettingResults;

    VOID                    UpdateSettingsMenu();
    VOID                    UpdateReadingSettings();
    VOID                    UpdateWritingSettings();

    VOID                    RenderSettingsMenu();
    VOID                    RenderReadingSettings();
    VOID                    RenderWritingSettings();

    VOID                    ReadPlayerSettings();
    VOID                    WritePlayerSettings();
    VOID                    ShowGamerCard();

    VOID                    ReadGamerPicture( XUSER_PROFILE_SETTING* pSetting );

private:
    virtual HRESULT         Initialize();
    virtual HRESULT         Update();
    virtual HRESULT         Render();
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
    m_bDrawHelp = FALSE;
    m_AppState = APPSTATE_READ_SETTINGS;
    m_pGamepad = NULL;
    m_nMenuItem = 0;
    m_dwSettingSizeMax = 0;
    m_pSettingResults = NULL;
    m_bNeedToReadSettings = TRUE;
    m_hSystemListener = NULL;

    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );
    ZeroMemory( m_Settings, sizeof( XUSER_PROFILE_SETTING ) * NUM_SETTINGS );

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        ATG::FatalError( "Could not create font\n" );

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        ATG::FatalError( "Could not create help\n" );

    // Initialize the simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Start up Xbox Live functionality using default Secure Network Layer settings
    if( XOnlineStartup() != ERROR_SUCCESS )
        ATG::FatalError( "Failed to start Xbox Live\n" );

    // Determine the maximum read buffer size and allocate space for it
    // Determine buffer size by passing a zero settings size
    m_dwSettingSizeMax = 0;
    DWORD dwErr;
    dwErr = XUserReadProfileSettings( 0,                      // A title in your family or 0 for the current title
                                      0,                      // Player index (not used)
                                      NUM_SETTINGS,           // Number of settings to read
                                      SettingIDs,             // List of settings to read
                                      &m_dwSettingSizeMax,    // Results size (0 to determine maximum)
                                      NULL,                   // Results (not used)
                                      NULL );                 // Overlapped (not used)

    assert( dwErr == ERROR_INSUFFICIENT_BUFFER );
    assert( m_dwSettingSizeMax > 0 );

    // NOTE: The game is responsible for freeing this memory when it is no longer needed
    BYTE* pData = new BYTE [ m_dwSettingSizeMax ];
    m_pSettingResults = ( XUSER_READ_PROFILE_SETTING_RESULT* )pData;

    for( DWORD i = 0; i < NUM_SETTINGS; ++i )
    {
        if( i >= Setting_Title1 )
        {
            // Allocate area for title specific data
            m_Settings[i].data.binary.pbData = new BYTE[ XPROFILE_SETTING_MAX_SIZE ];
            m_Settings[i].data.binary.cbData = XPROFILE_SETTING_MAX_SIZE;
            ZeroMemory( m_Settings[i].data.binary.pbData, XPROFILE_SETTING_MAX_SIZE );
        }
        else
        {
            // Allocate area for strings
            m_Settings[i].data.string.pwszData = new WCHAR[ 256 ];
            // cbData is the array size in bytes, not characters.
            m_Settings[i].data.string.cbData = 256 * sizeof( WCHAR );
        }
    }

    // Create gamer picture texture
    if( FAILED( m_pd3dDevice->CreateTexture( 64, 64, 1, 0, D3DFMT_LIN_A8R8G8B8, 0,
                                             &m_pGamerPictureTexture, NULL ) ) )
        ATG::FatalError( "Failed to create gamer picture texture.\n" );

    // Initialize login
    ATG::SignIn::Initialize( 1, 1, FALSE, 1 );

    // Register our notification listener
    m_hSystemListener = XNotifyCreateListener( XNOTIFY_SYSTEM );
    if( m_hSystemListener == NULL || m_hSystemListener == INVALID_HANDLE_VALUE )
    {
        ATG::FatalError( "Failed to create system notification listener.\n" );
    }

    ATG::SignIn::ShowSignInUI();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    ATG::SignIn::Update();

    // If we're not signed in, wait until we are
    if( !ATG::SignIn::AreUsersSignedIn() || ATG::SignIn::IsSystemUIShowing() )
    {
        return S_OK;
    }

    // Get the current gamepad status
    m_pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    static VOID ( Sample::*pfnUpdate[ APPSTATE_MAX ] )() =
    {
        &Sample::UpdateSettingsMenu,
        &Sample::UpdateReadingSettings,
        &Sample::UpdateWritingSettings
    };

    // call the appropriate update function based on the current state
    ( this->*pfnUpdate[ m_AppState ] )();
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
        m_Font.DrawText( 0, 0, 0xffffffff, L"ProfileSettings" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffffff, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();

        static VOID ( Sample::*pfnRender[ APPSTATE_MAX ] )() =
        {
            &Sample::RenderSettingsMenu,
            &Sample::RenderReadingSettings,
            &Sample::RenderWritingSettings
        };

        // Call the appropriate rendering function based on the current state
        ( this->*pfnRender[ m_AppState ] )();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: SettingsMenu()
// Desc: Shows the player's settings and allows updates
//--------------------------------------------------------------------------------------
VOID Sample::UpdateSettingsMenu()
{
    // Check for system notifications
    DWORD dwNotificationID;
    ULONG_PTR ulParam;

    while( XNotifyGetNext( m_hSystemListener, 0, &dwNotificationID, &ulParam ) )
    {
        if( dwNotificationID == XN_SYS_PROFILESETTINGCHANGED )
        {
            m_bNeedToReadSettings = TRUE;
            m_AppState = APPSTATE_READ_SETTINGS;
        }
    }

    // Check for menu scrolling
    if( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        if( m_nMenuItem < ( NUM_SETTINGS - 1 ) )
            ++m_nMenuItem;
    }

    if( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        if( m_nMenuItem > 0 )
            --m_nMenuItem;
    }

    // Left and right update the selected item if writeable profile
    if( g_bWriteAccess[ m_nMenuItem ] )
    {
        if( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        {
            XUSER_PROFILE_SETTING* pSetting = m_Settings + m_nMenuItem;

            switch( XUserGetProfileSettingType( pSetting->dwSettingId ) )
            {
                    // Sample doesn't handle overflow
                case XUSER_DATA_TYPE_INT32:
                    ++pSetting->data.nData;    break;
                case XUSER_DATA_TYPE_INT64:
                    ++pSetting->data.i64Data;  break;
                case XUSER_DATA_TYPE_DOUBLE:
                    ++pSetting->data.dblData;  break;
                case XUSER_DATA_TYPE_FLOAT:
                    ++pSetting->data.fData;    break;
                    // In case of binary data, tread it as a DWORD for simplicity
                case XUSER_DATA_TYPE_BINARY:
                    ++( *( DWORD* )pSetting->data.binary.pbData );
                    break;

                    // Unicode data is left unchanged
                case XUSER_DATA_TYPE_UNICODE:
                    break;
            }
        }
        if( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        {
            XUSER_PROFILE_SETTING* pSetting = m_Settings + m_nMenuItem;
            switch( XUserGetProfileSettingType( pSetting->dwSettingId ) )
            {
                case XUSER_DATA_TYPE_INT32:
                    if( pSetting->data.nData > 0 ) --pSetting->data.nData;     break;
                case XUSER_DATA_TYPE_INT64:
                    if( pSetting->data.i64Data > 0 ) --pSetting->data.i64Data; break;
                case XUSER_DATA_TYPE_DOUBLE:
                    if( pSetting->data.dblData > 0 ) --pSetting->data.dblData; break;
                case XUSER_DATA_TYPE_FLOAT:
                    if( pSetting->data.fData > 0 ) --pSetting->data.fData;     break;
                    // In case of binary data, treat it as a DWORD for simplicity
                case XUSER_DATA_TYPE_BINARY:
                    if( ( *( DWORD* )pSetting->data.binary.pbData ) > 0 ) --
                            ( *( DWORD* )pSetting->data.binary.pbData );
                    break;

                    // Unicode data is left unchanged
                case XUSER_DATA_TYPE_UNICODE:
                    break;
            }
        }
    }

    // "B" returns to sign-in
    if( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_AppState = APPSTATE_READ_SETTINGS;
        m_bNeedToReadSettings = TRUE;

        ATG::SignIn::ShowSignInUI();
    }

    // "X" saves the profile
    if( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_AppState = APPSTATE_WRITE_SETTINGS;
        WritePlayerSettings();
    }

    // "Y" shows the gamercard UI
    if( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        ShowGamerCard();
    }
}


//--------------------------------------------------------------------------------------
// Name: UpdateReadingSettings()
// Desc: Wait for the read of settings to complete, the display settings
//--------------------------------------------------------------------------------------
VOID Sample::UpdateReadingSettings()
{
    // If we need to read settings, do it
    if( m_bNeedToReadSettings )
    {
        ReadPlayerSettings();
        m_bNeedToReadSettings = FALSE;
    }

    // If the read has completed, show the settings
    // This should only be called when XUserReadProfileSettings returns ERROR_IO_PENDING
    if( XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        // Let the message display for a bit
        Sleep( 1000 ); // remove in a real game

        // The sample doesn't check for read failures
        assert( XGetOverlappedExtendedError( &m_Overlapped ) == ERROR_SUCCESS );

        // Copy values into local struct
        for( DWORD i = 0; i < NUM_SETTINGS; ++i )
        {
            XUSER_PROFILE_SETTING* pDest = m_Settings + i;
            XUSER_PROFILE_SETTING* pSrc = m_pSettingResults->pSettings + i;

            pDest->user = pSrc->user;
            pDest->source = pSrc->source;
            pDest->dwSettingId = SettingIDs[ i ];

            // Get the setting value only if there is one, else default it for display
            switch( pSrc->data.type )
            {
                case XUSER_DATA_TYPE_INT32:
                    pDest->data.nData = ( XSOURCE_NO_VALUE == pSrc->source ? 0 : pSrc->data.nData );
                    break;
                case XUSER_DATA_TYPE_INT64:
                    pDest->data.i64Data = ( XSOURCE_NO_VALUE == pSrc->source ? 0 : pSrc->data.i64Data );
                    break;
                case XUSER_DATA_TYPE_DOUBLE:
                    pDest->data.dblData = ( XSOURCE_NO_VALUE == pSrc->source ? 0.0f : pSrc->data.dblData );
                    break;
                case XUSER_DATA_TYPE_FLOAT:
                    pDest->data.fData = ( XSOURCE_NO_VALUE == pSrc->source ? 0.0f : pSrc->data.fData );
                    break;
                case XUSER_DATA_TYPE_BINARY:
                    if( pSrc->data.binary.pbData != NULL )
                    {
                        memcpy( pDest->data.binary.pbData, pSrc->data.binary.pbData, pSrc->data.binary.cbData );
                        pDest->data.binary.cbData = pSrc->data.binary.cbData;
                    }
                    else
                    {
                        *( DWORD* )pDest->data.binary.pbData = 0;
                        pDest->data.binary.cbData = XPROFILE_SETTING_MAX_SIZE;
                    }
                    break;

                    // Copy string data
                case XUSER_DATA_TYPE_UNICODE:
                    ZeroMemory(pDest->data.string.pwszData, pDest->data.string.cbData);

                    if( pSrc->data.string.pwszData && pSrc->data.string.cbData )
                    {
                        memcpy(pDest->data.string.pwszData,
                            pSrc->data.string.pwszData,
                            pSrc->data.string.cbData);
                        pDest->data.string.cbData = pSrc->data.string.cbData;
                    }
                    break;
            }

            if( pSrc->dwSettingId == XPROFILE_GAMERCARD_PICTURE_KEY )
                ReadGamerPicture( pSrc );
        }

        m_AppState = APPSTATE_SETTINGS_MENU;
    }
}


//--------------------------------------------------------------------------------------
// Name: UpdateWritingSettings()
// Desc: Shows the player's settings and allows updates
//--------------------------------------------------------------------------------------
VOID Sample::UpdateWritingSettings()
{
    // If the write has completed, continue with the next step
    // This should only be called when XUserWriteProfileSettings returns ERROR_IO_PENDING
    if( XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        // Let the message display for a bit
        Sleep( 1000 ); // remove in a real game

        // The sample doesn't check for write failures
        assert( XGetOverlappedExtendedError( &m_Overlapped ) == ERROR_SUCCESS );

        // Now go back to the view settings page and wait for
        // the notification to tell us to read the new values
        m_AppState = APPSTATE_SETTINGS_MENU;
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderSettingsMenu()
// Desc: Draw the settings menu
//--------------------------------------------------------------------------------------
VOID Sample::RenderSettingsMenu()
{
    WCHAR* strSource[] =
    {
        L"No value",    // XSOURCE_NO_VALUE
        L"Default",     // XSOURCE_DEFAULT
        L"Title",       // XSOURCE_TITLE
        L"No access"    // XSOURCE_PERMISSION_DENIED
    };

    WCHAR strType[ 1024 ];
    WCHAR strValue[ 1024 ];
    m_Font.Begin();
    m_Font.DrawText( -146, 62, 0xffffffff, L"Value", ATGFONT_RIGHT );
    m_Font.DrawText( -106, 62, 0xffffffff, L"Source" );
    const FLOAT fStartY = 92.0f;
    const FLOAT fOffset = 40.0f;
    DWORD dwSettingsOffset = ( ( DWORD )m_nMenuItem / SETTINGS_PER_SCREEN ) * SETTINGS_PER_SCREEN;
    for( DWORD i = 0; i < SETTINGS_PER_SCREEN; ++i )
    {
        DWORD dwSettingIndex = i + dwSettingsOffset;
        if( dwSettingIndex < NUM_SETTINGS )
        {
            DWORD dwTextColor;
            DWORD dwValueColor;
            if( g_bWriteAccess[dwSettingIndex] )
            {
                dwTextColor = STEXT_COLOR_RW;
                dwValueColor = VTEXT_COLOR_RW;
            }
            else
            {
                dwTextColor = STEXT_COLOR_RO;
                dwValueColor = VTEXT_COLOR_RO;
            }

            FLOAT y = fStartY + ( i * fOffset );
            m_Font.DrawText( 90, y, dwTextColor, g_strDesc[dwSettingIndex] );

            const XUSER_PROFILE_SETTING* pSetting = m_Settings + dwSettingIndex;

            // Extract setting value as a string
            *strValue = 0;
            switch( XUserGetProfileSettingType( pSetting->dwSettingId ) )
            {
                case XUSER_DATA_TYPE_INT32:
                    _ultow_s( pSetting->data.nData, strValue, 10 );
                    break;
                case XUSER_DATA_TYPE_INT64:
                    _i64tow_s( pSetting->data.i64Data, strValue, ARRAYSIZE( strValue ), 10 );
                    break;
                case XUSER_DATA_TYPE_DOUBLE:
                    swprintf_s( strValue, L"%.1f", pSetting->data.dblData );
                    break;
                case XUSER_DATA_TYPE_UNICODE:
                    assert( pSetting->data.string.pwszData && pSetting->data.string.cbData );
                    memcpy(strValue,
                           pSetting->data.string.pwszData,
                           pSetting->data.string.cbData);
                    break;
                case XUSER_DATA_TYPE_FLOAT:
                    swprintf_s( strValue, L"%.1f", pSetting->data.fData );
                    break;
                case XUSER_DATA_TYPE_BINARY:
                    if( pSetting->data.binary.pbData )
                        _ultow_s( *( DWORD* )pSetting->data.binary.pbData, strValue, 10 );
                    break;
                default:
                    // sample doesn't currently handle other types
                    assert( false );
                    break;
            }

            // Draw gamer picture
            if( pSetting->dwSettingId == XPROFILE_GAMERCARD_PICTURE_KEY )
            {
                m_Font.End();
                D3DRECT rect;
                rect.x1 = ATG::GetTitleSafeArea().x2 - 146 - 32;
                rect.y1 = ATG::GetTitleSafeArea().y1 + ( LONG )y;
                rect.x2 = rect.x1 + 32;
                rect.y2 = rect.y1 + 32;
                ATG::DebugDraw::DrawScreenSpaceTexturedRect( rect, m_pGamerPictureTexture );
                m_Font.Begin();
            }
            else
            {
                m_Font.DrawText( -146, y, dwValueColor, strValue, ATGFONT_RIGHT );

                // Extract source value
                assert( pSetting->source < ARRAYSIZE( strSource ) );
                wcscpy_s( strType, strSource[ pSetting->source ] );
                m_Font.DrawText( -106, y, dwValueColor, strType );
            }

            // Indicator
            if( dwSettingIndex == ( DWORD )m_nMenuItem )
                m_Font.DrawText( 0, y, dwValueColor, GLYPH_RIGHT_ARROW );
        }
    }

    // Buttons
    m_Font.DrawText( 0, -50, 0xffffffff, GLYPH_Y_BUTTON L" Show Gamercard" );
    m_Font.DrawText( 0, -25, 0xffffffff, GLYPH_LEFT_BUTTON GLYPH_RIGHT_BUTTON L" Change Setting" );
    m_Font.DrawText( 0, -50, 0xffffffff, GLYPH_X_BUTTON L" Save", ATGFONT_RIGHT );
    m_Font.DrawText( 0, -25, 0xffffffff, GLYPH_B_BUTTON L" Back", ATGFONT_RIGHT );

    m_Font.End();
}


//--------------------------------------------------------------------------------------
// Name: RenderReadingSettings()
// Desc: Draw read text
//--------------------------------------------------------------------------------------
VOID Sample::RenderReadingSettings()
{
    FLOAT fCenterX = ( m_Font.m_rcWindow.x2 - m_Font.m_rcWindow.x1 ) / 2.0f;
    FLOAT fCenterY = ( m_Font.m_rcWindow.y2 - m_Font.m_rcWindow.y1 ) / 2.0f;
    m_Font.DrawText( fCenterX, fCenterY, 0xffffffff, L"Reading Settings",
                     ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
}


//--------------------------------------------------------------------------------------
// Name: RenderWritingSettings()
// Desc: Draw read text
//--------------------------------------------------------------------------------------
VOID Sample::RenderWritingSettings()
{
    FLOAT fCenterX = ( m_Font.m_rcWindow.x2 - m_Font.m_rcWindow.x1 ) / 2.0f;
    FLOAT fCenterY = ( m_Font.m_rcWindow.y2 - m_Font.m_rcWindow.y1 ) / 2.0f;
    m_Font.DrawText( fCenterX, fCenterY, 0xffffffff, L"Writing Settings",
                     ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
}


//--------------------------------------------------------------------------------------
// Name: ReadPlayerSettings()
// Desc: Initiates the read of the player's settings
//--------------------------------------------------------------------------------------
VOID Sample::ReadPlayerSettings()
{
    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );
    DWORD dwErr;
    dwErr = XUserReadProfileSettings( 0,                   // A title in your family or 0 for the current title
                                      ATG::SignIn::GetSignedInUser(),  // Player index making the request
                                      NUM_SETTINGS,        // Number of settings to read
                                      SettingIDs,          // List of settings to read
                                      &m_dwSettingSizeMax, // Results size
                                      m_pSettingResults,   // Results go here
                                      &m_Overlapped );     // Overlapped struct

    assert( dwErr == ERROR_SUCCESS || dwErr == ERROR_IO_PENDING );
}


//--------------------------------------------------------------------------------------
// Name: WritePlayerSettings()
// Desc: Initiates the write of the player's settings
//--------------------------------------------------------------------------------------
VOID Sample::WritePlayerSettings()
{
    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

    // Select only writeable settings
    DWORD dwNumWriteableSettings = 0;
    for( DWORD i = 0; i < NUM_SETTINGS; ++i )
    {
        if( g_bWriteAccess[i] )
        {
            m_WriteableSettings[dwNumWriteableSettings++] = m_Settings[i];
        }
    }

    DWORD dwErr;
    dwErr = XUserWriteProfileSettings( ATG::SignIn::GetSignedInUser(),  // Player index making the request
                                       dwNumWriteableSettings,      // Number of settings to write
                                       m_WriteableSettings,         // List of settings to write
                                       &m_Overlapped );             // Overlapped struct

    assert( dwErr == ERROR_SUCCESS || dwErr == ERROR_IO_PENDING );
}


//--------------------------------------------------------------------------------------
// Name: ShowGamerCard()
// Desc: Show the Gamercard UI
//--------------------------------------------------------------------------------------
VOID Sample::ShowGamerCard()
{
    XUID playerXuid;
    DWORD dwErr;

    // Gamercards only available for online users
    if( ATG::SignIn::IsUserOnline( ATG::SignIn::GetSignedInUser() ) )
    {
        dwErr = XUserGetXUID( ATG::SignIn::GetSignedInUser(), &playerXuid );
        assert( dwErr == ERROR_SUCCESS );

        dwErr = XShowGamerCardUI( ATG::SignIn::GetSignedInUser(), playerXuid );
        assert( dwErr == ERROR_SUCCESS );
    }
}


//--------------------------------------------------------------------------------------
// Name: ReadGamerPicture()
// Desc: Read the gamer picture texture.
//--------------------------------------------------------------------------------------
VOID Sample::ReadGamerPicture( XUSER_PROFILE_SETTING* pSetting )
{
    D3DLOCKED_RECT rect;
    D3DSURFACE_DESC desc;

    // Get the texture size
    m_pGamerPictureTexture->GetLevelDesc( 0, &desc );

    // Lock texture to get pointer
    m_pGamerPictureTexture->LockRect( 0, &rect, NULL, 0 );

    // Read gamer picture
    DWORD dwErr;
    dwErr = XUserReadGamerPictureByKey( &pSetting->data, FALSE, ( BYTE* )rect.pBits, rect.Pitch, desc.Height, NULL );
    assert( dwErr == ERROR_SUCCESS );

    // Unlock texture
    m_pGamerPictureTexture->UnlockRect( 0 );
}
