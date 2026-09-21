//--------------------------------------------------------------------------------------
// XInputRemap.cpp
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xmcore.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSignIn.h>
#include <AtgUtil.h>
#include <xaudio2.h>
#include <xhv2.h>


//--------------------------------------------------------------------------------------
// Demonstrates how to use XInpoutRemap() to change the controllers associated with 
// each signed in user. 
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] = 
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_1, L"Swap Controllers" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_1, L"Sign-in players" },
};
static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE(g_HelpCallouts);


//--------------------------------------------------------------------------------------
// Color values used in this sample
//--------------------------------------------------------------------------------------
#define TOP_BACK_COLOR      0xff00007f          // Background gradient colors
#define BOTTOM_BACK_COLOR   0xff000000

#define TEXT_COLOR          0xffffffff

#define X_MARK_COLOR        0xff00ff00
#define CHECK_MARK_COLOR    0xffff0000

#define LIST_SEL_COLOR           0xffff0000
#define LIST_SELCONFLICT_COLOR   0xff00ff00


// Holds the list of all connected controllers along with associated player index
struct ControllerList
{
    INT   nNumControllers;                    // Number of actual entries in the list ( 0 to XUSER_MAX_COUNT - 1)
    BYTE  ControllerType[ XUSER_MAX_COUNT ];  // Type of controller associated to this entry
    DWORD dwPlayerIndex[ XUSER_MAX_COUNT ];   // Player index 0 to XUSER_MAX_COUNT - 1 associated to this entry
};


// Swaping info for specific player
struct SwapInfo
{
    BOOL  bIsControllerPresent;   // TRUE if the controller is present.
    BYTE  CurrentControllerType;  // Type of controller associated with this player
    DWORD dwSwapSelection;        // Item currently selected in ControllerList.
};


// Maps controller type IDs to human readable names
struct DescribeControllerType
{
    BYTE  ControllerType;
    LPCWSTR szControllerName;
};


// Table that maps controller types to corresponding displayable names
const DescribeControllerType g_DescribeControllerTypes[] =
{
    XINPUT_DEVSUBTYPE_GAMEPAD,          L"Gamepad",
    XINPUT_DEVSUBTYPE_DRUM_KIT,         L"Drum Kit",
    XINPUT_DEVSUBTYPE_GUITAR,           L"Guitar",
    XINPUT_DEVSUBTYPE_GUITAR_ALTERNATE, L"Alt Guitar",
    XINPUT_DEVSUBTYPE_WHEEL,            L"Wheel",
    XINPUT_DEVSUBTYPE_ARCADE_STICK,     L"Arcade Joystick",
    XINPUT_DEVSUBTYPE_FLIGHT_STICK,     L"Flight Stick",
    XINPUT_DEVSUBTYPE_DANCEPAD,         L"Dance Pad",
    XINPUT_DEVSUBTYPE_UNKNOWN,          L"Unknown"
};


// State the sample is in.
enum State
{
    STATE_MAIN,   // Sample main screen is displayed.
    STATE_SWAP,   // Controller swapping screen is displayed
    STATE_HELP    // Help screen is displayed
};


//--------------------------------------------------------------------------------------
// Display helper functions and structures
//--------------------------------------------------------------------------------------

BOOL GetControllerTypeText( BYTE ControllerType, LPWSTR szBuffer, DWORD dwBufferSize );
void ComputeOnScreenPlayerPosition( const ATG::Font& font, DWORD dwPlayerIndex, XMFLOAT2* pSceenPos );

void DisplayGamerTag( ATG::Font& font, DWORD dwPlayerIndex, const char* szGamerTag );
void DisplayControllerType( ATG::Font& font, DWORD dwPlayerIndex, BYTE ControllerType );
void DisplayHeadsetStatus( ATG::Font& font, DWORD dwPlayerIndex, BOOL bHeadsetIsPresent );
void DisplayOptions( ATG::Font& font );

void DisplaySwapGamerTag( ATG::Font& font, DWORD dwPlayerIndex, const char* szGamerTag );
void DisplaySwapCurrentControllerType( ATG::Font& font, DWORD dwPlayerIndex, BYTE ControllerType );
void DisplaySwapControllerList( ATG::Font& font, DWORD dwPlayerIndex, const ControllerList& controllerList );
void HighlightSwapSelection( ATG::Font& font, DWORD dwPlayerIndex,  INT nSelectionIndex, DWORD dwColor );
void DisplaySwapOptions( ATG::Font& font );


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    // Swap state specific functions
    void InitSwapInfo();
    BOOL IsSwapSelectionUnique( INT nIndex );
    BOOL RemapControllers();

    PIXHV2ENGINE m_pEngine;

    // Swap state specific data
    XOVERLAPPED m_RemapMessageBoxOverlapped;
    MESSAGEBOX_RESULT m_RemapMessageBoxResult;
    ControllerList m_ControllerList;
    SwapInfo m_SwapInfo[ XUSER_MAX_COUNT ];

    // General sample data
    ATG::Font           m_Font;
    ATG::Help           m_Help;
    State               m_SampleState;
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
    // Start in normal state
    m_SampleState = STATE_MAIN;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize Live, needed for Sign-in functionnality.
    if( FAILED( XOnlineStartup() ) )
        return E_FAIL;

    // Initialize logon
    ATG::SignIn::Initialize( 1, 4, FALSE, 4 );

    // Initialize XAudio2, needed to identify connected headsets.
    IXAudio2* pXAudio;
    if( FAILED( XAudio2Create( &pXAudio ) ) )
        return E_FAIL;

    IXAudio2MasteringVoice* pMasteringVoice;
    if( FAILED( pXAudio->CreateMasteringVoice( &pMasteringVoice ) ) )
    {
        pXAudio->Release();
        return E_FAIL;
    }

    // Init voice chat services, needed to identify connected headsets.
    XHV_PROCESSING_MODE rgModes[] = {XHV_VOICECHAT_MODE, XHV_LOOPBACK_MODE};
    XHV_INIT_PARAMS xhvParams = { 0 };
    xhvParams.dwMaxLocalTalkers = XHV_MAX_LOCAL_TALKERS;
    xhvParams.localTalkerEnabledModes = rgModes;
    xhvParams.dwNumLocalTalkerEnabledModes  = 2;
    xhvParams.pXAudio2 = pXAudio;
    HANDLE hWorkerThread;
    if( FAILED( XHV2CreateEngine( &xhvParams, &hWorkerThread, &m_pEngine ) ) )
    {
        pXAudio->Release();
        return E_FAIL;
    }

    // Register each potential player as a local talker, needed to identify connected headsets.
    for( INT i = 0; i < XUSER_MAX_COUNT; ++ i )
        if( FAILED( m_pEngine->RegisterLocalTalker( i ) ) )
        {
            m_pEngine->Release();
            pXAudio->Release();
            return E_FAIL;
        }

    // Start sample by showing the signin UI for one user
    ATG::SignIn::ShowSignInUI();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitSwapInfo()
// Desc: Fills SwapInfo and ControllerList structures that are used while in SwapState.
//--------------------------------------------------------------------------------------
void Sample::InitSwapInfo()
{
    INT iControllerIndex = 0;
    for( DWORD dwPlayerIndex = 0; dwPlayerIndex < XUSER_MAX_COUNT; ++ dwPlayerIndex )
    {
        m_SwapInfo[ dwPlayerIndex ].bIsControllerPresent = FALSE;
        if( ATG::Input::m_Gamepads[ dwPlayerIndex ].bConnected && 
            ATG::Input::m_Gamepads[ dwPlayerIndex ].caps.Type == XINPUT_DEVTYPE_GAMEPAD )
        {
            m_ControllerList.ControllerType[ iControllerIndex ] =  ATG::Input::m_Gamepads[ dwPlayerIndex ].caps.SubType;
            m_ControllerList.dwPlayerIndex[ iControllerIndex ] =  dwPlayerIndex;
            m_SwapInfo[ dwPlayerIndex ].bIsControllerPresent = TRUE;
            m_SwapInfo[ dwPlayerIndex ].dwSwapSelection = iControllerIndex;
            m_SwapInfo[ dwPlayerIndex ].CurrentControllerType = ATG::Input::m_Gamepads[ dwPlayerIndex ].caps.SubType;
            ++ iControllerIndex;
        }
    }

    m_ControllerList.nNumControllers = iControllerIndex;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Update login
    ATG::SignIn::Update();

    switch( m_SampleState )
    {
        case STATE_HELP:
        {
            // Get input from all the gamepads
            ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

            // Exit help screen if a player presses BACK
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
                m_SampleState = STATE_MAIN;

            break;
        }

        case STATE_SWAP:
        {
            // XInputRemap sample doesn't check if controllers have been removed or added in SWAP mode to allow
            // experimenting with calling XInputRemap() with invalid configuration. The sample will display
            // an error message when the situation happens.

            // Update controller input structures
            ATG::Input::GetInput();

            // Process all four potential players
            for( DWORD dwPlayerIndex = 0; dwPlayerIndex < XUSER_MAX_COUNT; ++ dwPlayerIndex )
            {
                // If player pressed A and there is at least one item in the list, set the selection as first item.
                if( m_ControllerList.nNumControllers > 0 && 
                    ATG::Input::m_Gamepads[ dwPlayerIndex ].wPressedButtons & XINPUT_GAMEPAD_A )
                {
                    m_SwapInfo[ dwPlayerIndex ].dwSwapSelection = 0;
                }

                // If player pressed B and there are at least two items in the list, set the selection as second item.
                if( m_ControllerList.nNumControllers > 1 && 
                    ATG::Input::m_Gamepads[ dwPlayerIndex ].wPressedButtons & XINPUT_GAMEPAD_B )
                {
                    m_SwapInfo[ dwPlayerIndex ].dwSwapSelection = 1;
                }

                // If player pressed X and there are at least three items in the list, set the selection as third item.
                if( m_ControllerList.nNumControllers > 2 && 
                    ATG::Input::m_Gamepads[ dwPlayerIndex ].wPressedButtons & XINPUT_GAMEPAD_X )
                {
                    m_SwapInfo[ dwPlayerIndex ].dwSwapSelection = 2;
                }

                // If player pressed Y and there are at least four items in the list, set the selection as fourth item.
                if( m_ControllerList.nNumControllers > 3 && 
                    ATG::Input::m_Gamepads[ dwPlayerIndex ].wPressedButtons & XINPUT_GAMEPAD_Y )
                {
                    m_SwapInfo[ dwPlayerIndex ].dwSwapSelection = 3;
                }

                // If any player pressed Start, try to remap the controllers.
                if( ATG::Input::m_Gamepads[ dwPlayerIndex ].wPressedButtons & XINPUT_GAMEPAD_START )
                {
                    if( RemapControllers() )
                    {            
                        m_SampleState = STATE_MAIN;
                    }
                }

                // If any player pressed Back, cancel remapping and return to normal state
                if( ATG::Input::m_Gamepads[ dwPlayerIndex ].wPressedButtons & XINPUT_GAMEPAD_BACK )
                {
                    m_SampleState = STATE_MAIN;
                }
            }

            break;
        }

        case STATE_MAIN:
        {
            // Refresh capabilities, in case a controller was added or removed, or XInputRemap was called.
            for( INT i = 0; i < XUSER_MAX_COUNT; ++ i )
                XInputGetCapabilities( i, XINPUT_FLAG_GAMEPAD, &ATG::Input::m_Gamepads[ i ].caps );

            // Get input from all the gamepads
            ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

            // Show help
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
                m_SampleState = STATE_HELP;

            // Show the signin UI
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
            {
                ATG::SignIn::ShowSignInUI();
            }
    
            // Enter controller swap mode
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
            {
                InitSwapInfo();
                m_SampleState = STATE_SWAP;
            }

            break;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: IsSwapSelectionUnique()
// Desc: Returns TRUE if no other player as the same item selected from the 
//       ControllerList.
//--------------------------------------------------------------------------------------
BOOL Sample::IsSwapSelectionUnique( INT nIndex )
{
    assert( m_SwapInfo[ nIndex  ].bIsControllerPresent );

    for(INT current = 0; current < XUSER_MAX_COUNT; ++ current )
    {
        if( current != nIndex && m_SwapInfo[ current  ].bIsControllerPresent && 
            m_SwapInfo[ current ].dwSwapSelection == m_SwapInfo[ nIndex ].dwSwapSelection )
            return FALSE;
    }

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: RemapControllers()
// Desc: Tries to remap controllers. If unsuccessful, displays an explanatory message.
// Return: Return TRUE on success and FALSE if the operation fails.
//--------------------------------------------------------------------------------------
BOOL Sample::RemapControllers()
{
    // rgMapping must have a unique value for each user index, even if no controller is connected.
    assert( XUSER_MAX_COUNT == 4 );
    DWORD rgMapping[ XUSER_MAX_COUNT ] = { 0, 1, 2, 3 };

    // Convert selected items in list to controller indexes.
    for( DWORD i = 0; i < XUSER_MAX_COUNT; ++ i )
    {
        if( m_SwapInfo[ i ].bIsControllerPresent )
        {
            rgMapping[ i ] = m_ControllerList.dwPlayerIndex[ m_SwapInfo[ i ].dwSwapSelection ];
        }
    }

    DWORD dwResult;

    // Try to remap the controllers
    dwResult = XInputRemap( rgMapping );

    // On success, exit.
    if( dwResult== ERROR_SUCCESS )
    {
        return TRUE;
    }
    // Display error message...
    else
    {
        LPCWSTR strErrorMessage;
        switch( dwResult)
        {
            case ERROR_ACCESS_DENIED:
                strErrorMessage = L"ERROR_ACCESS_DENIED\n"
                                  L"Missing privilege.\n\n"
                                  L"This function requires the XEX_PRIVILEGE_ALLOW_CONTROLLER_SWAPPING(30)"
                                  L"privilege bit in the XEX header.";
                break;

            case ERROR_DEVICE_NOT_CONNECTED:
                strErrorMessage = L"ERROR_DEVICE_NOT_CONNECTED\n"
                                  L"Swapping from or to a non-bound device index.\n\n"
                                  L"Maybe a controller was disconnected?";
                break;
    
            case ERROR_INVALID_PARAMETER:
                strErrorMessage = L"ERROR_INVALID_PARAMETER\n"
                                  L"Duplicate entry in the mapping array (e.g. { 1, 1, 0, 3 })\n\n"
                                  L"You are trying to assign the same controller to two players";
                break;

            case ERROR_NOT_SUPPORTED: 
                strErrorMessage = L"ERROR_NOT_SUPPORTED\n"
                                  L"Swapping a device that has a chatpad attached.\n\n"
                                  L"It is not possible to swap controllers that have a Chatpad attached.";
                break;

            default:
                assert( false );  
                strErrorMessage = L"Unknown error.";
                break;
        }
  
        // Display error message
        ZeroMemory( &m_RemapMessageBoxOverlapped, sizeof( XOVERLAPPED ) );
        LPCWSTR pwstrButtons[] = { L"OK" };
        XShowMessageBoxUI( XUSER_INDEX_ANY, L"XInputRemap has failed", strErrorMessage, 1, pwstrButtons, 0, 
                           XMB_ERRORICON, &m_RemapMessageBoxResult, &m_RemapMessageBoxOverlapped );

        return FALSE;
    }
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( TOP_BACK_COLOR, BOTTOM_BACK_COLOR );

    switch( m_SampleState )
    {
        case STATE_HELP:
            m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
            break;

        case STATE_SWAP:
        {
            m_Font.Begin();

            for( DWORD dwPlayerIndex = 0; dwPlayerIndex < XUSER_MAX_COUNT; dwPlayerIndex++ )
            {
                XUSER_SIGNIN_INFO SigninInfo;
                XUserGetSigninInfo( dwPlayerIndex, 0, &SigninInfo );
                DisplaySwapGamerTag( m_Font, dwPlayerIndex, SigninInfo.szUserName );
                
                if( m_SwapInfo[ dwPlayerIndex ].bIsControllerPresent )
                {
                    DisplaySwapCurrentControllerType( m_Font, dwPlayerIndex, 
                                                      m_SwapInfo[ dwPlayerIndex ].CurrentControllerType );
                    DisplaySwapControllerList( m_Font, dwPlayerIndex, m_ControllerList );
                    HighlightSwapSelection( m_Font, dwPlayerIndex,  m_SwapInfo[ dwPlayerIndex ].dwSwapSelection, 
                                            IsSwapSelectionUnique( dwPlayerIndex ) ? 
                                            LIST_SELCONFLICT_COLOR : LIST_SEL_COLOR );
                }
            }
            DisplaySwapOptions( m_Font );

            // Draw title text
            m_Font.SetScaleFactors( 1.2f, 1.2f );
            m_Font.DrawText( 0, 0, 0xffffffff,  L"XInputRemap - Swap Controllers" );
            m_Font.End();
            break;
        }

        case STATE_MAIN:
        {
            m_Font.Begin();

            for( DWORD dwPlayerIndex = 0; dwPlayerIndex < XUSER_MAX_COUNT; dwPlayerIndex++ )
            {
                XUSER_SIGNIN_INFO SigninInfo;
                XUserGetSigninInfo( dwPlayerIndex, 0, &SigninInfo );
                DisplayGamerTag( m_Font, dwPlayerIndex, SigninInfo.szUserName );
                DisplayHeadsetStatus( m_Font, dwPlayerIndex, m_pEngine->IsHeadsetPresent( dwPlayerIndex ) );

                if( ATG::Input::m_Gamepads[ dwPlayerIndex ].caps.Type == XINPUT_DEVTYPE_GAMEPAD )
                    DisplayControllerType( m_Font, dwPlayerIndex, 
                                           ATG::Input::m_Gamepads[ dwPlayerIndex ].caps.SubType );
            }
            DisplayOptions( m_Font );
        
            // Draw title text
            m_Font.SetScaleFactors( 1.2f, 1.2f );
            m_Font.DrawText( 0, 0, 0xffffffff,  L"XInputRemap" );
            m_Font.End();
            break;
        }
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GetControllerTypeText()
// Desc: Helper function that return a text string identifying the controller type.
// Return: TRUE if the controller type was found, FALSE otherwise.
//--------------------------------------------------------------------------------------
BOOL GetControllerTypeText( BYTE ControllerType, LPWSTR szBuffer, DWORD dwBufferSize )
{
    for( INT j = 0; j < sizeof( g_DescribeControllerTypes ) / sizeof( DescribeControllerType ); ++ j )
    {
        if( ControllerType == g_DescribeControllerTypes[ j ].ControllerType )
        {
            swprintf_s( szBuffer, dwBufferSize, L"%s", g_DescribeControllerTypes[ j ].szControllerName );
            return TRUE;
        }
    }

    swprintf_s( szBuffer, dwBufferSize, L"%s", L"Undefined" ); // Unknown index.
    return FALSE;
}


//--------------------------------------------------------------------------------------
// Name: ComputeOnScreenPlayerPosition()
// Desc: Helper function that computes the top left corner of the player's display area.
//                The top lef corner position is returned in pOrigin.
//--------------------------------------------------------------------------------------
void ComputeOnScreenPlayerPosition( const ATG::Font& font, DWORD dwPlayerIndex, XMFLOAT2* pOrigin )
{
    assert( dwPlayerIndex >= 0 && dwPlayerIndex < XUSER_MAX_COUNT );
    assert( pOrigin != NULL );

    pOrigin->x = ((font.m_rcWindow.x2 - font.m_rcWindow.x1)
        / 2) * (FLOAT)( dwPlayerIndex % 2 ) + 20.0f;

    pOrigin->y = ( dwPlayerIndex > 1 ? 160 : 0 ) + 40.0f;
}


//--------------------------------------------------------------------------------------
// Name: DisplayGamerTag()
// Desc: Helper function that displays the gamertag associated with a specific
//       player index.
//--------------------------------------------------------------------------------------
void DisplayGamerTag( ATG::Font &font, DWORD dwPlayerIndex, const char *szGamerTag )
{
    WCHAR szBuffer[ XUSER_NAME_SIZE + 1 ];
    swprintf_s( szBuffer, L"%hs", szGamerTag );

    XMFLOAT2 origin;
    ComputeOnScreenPlayerPosition( font, dwPlayerIndex, &origin );
    font.DrawText( origin.x, origin.y, TEXT_COLOR, szBuffer );
}


//--------------------------------------------------------------------------------------
// Name: DisplayControllerType()
// Desc: Helper function that displays the type of controller associated with a given
//       player. ControllerType expects a value that is valid for the SubType field of
//       the an XINPUT_CAPABILITIES structure.
//--------------------------------------------------------------------------------------
void DisplayControllerType( ATG::Font &font, DWORD dwPlayerIndex, BYTE ControllerType )
{ 
    XMFLOAT2 origin;
    ComputeOnScreenPlayerPosition( font, dwPlayerIndex, &origin );

    WCHAR szBuffer[ 128 ];
    GetControllerTypeText( ControllerType, szBuffer, 128 );

    font.DrawText( origin.x, origin.y + font.GetFontHeight(), TEXT_COLOR, szBuffer );
}


//--------------------------------------------------------------------------------------
// Name: DisplayHeadsetStatus()
// Desc: Helper function that displays whether a headset is associated with a player or 
//       not.
//--------------------------------------------------------------------------------------
void DisplayHeadsetStatus( ATG::Font &font, DWORD dwPlayerIndex, BOOL bHeadsetIsPresent )
{
    XMFLOAT2 origin;
    ComputeOnScreenPlayerPosition( font, dwPlayerIndex, &origin );
    origin.y += 2 * font.GetFontHeight();

    font.DrawText( origin.x, origin.y, TEXT_COLOR, L"Headset" );
    font.DrawText( origin.x + 130.0f, origin.y, bHeadsetIsPresent ? X_MARK_COLOR : CHECK_MARK_COLOR, 
                                                bHeadsetIsPresent ? GLYPH_X_MARK : GLYPH_CHECK_MARK  );
}


//--------------------------------------------------------------------------------------
// Name: DisplayOptions()
// Desc: Helper function that display the option available at the bottom of the screen.
//--------------------------------------------------------------------------------------
void DisplayOptions( ATG::Font &font )
{
    FLOAT fDrawX = ( font.m_rcWindow.x2 - font.m_rcWindow.x1 ) / 2.0f;
    FLOAT fDrawY = font.m_rcWindow.y2 - 2 * font.GetFontHeight();

    font.DrawText( fDrawX, fDrawY, TEXT_COLOR, 
                   GLYPH_BACK_BUTTON L" Help  " GLYPH_B_BUTTON L"Sign In  " GLYPH_A_BUTTON L"Swap", ATGFONT_CENTER_X );
}


//--------------------------------------------------------------------------------------
// Name: DisplaySwapGamerTag()
// Desc: Helper function that displays the gamertag associated with a specific
//       player index while the STATE_SWAP state is active.
//--------------------------------------------------------------------------------------
void DisplaySwapGamerTag( ATG::Font &font, DWORD dwPlayerIndex, const char *szGamerTag )
{
    WCHAR szBuffer[ XUSER_NAME_SIZE + 1 ];
    swprintf_s( szBuffer, L"%hs", szGamerTag );

    XMFLOAT2 origin;
    ComputeOnScreenPlayerPosition( font, dwPlayerIndex, &origin );
    font.DrawText( origin.x, origin.y, TEXT_COLOR, szBuffer );
}


//--------------------------------------------------------------------------------------
// Name: DisplaySwapCurrentControllerType()
// Desc: Helper function that displays the type of controller currently associated with 
//       a given player while the STATE_SWAP state is active. ControllerType expects a 
//       value that is valid for the SubType field of the an XINPUT_CAPABILITIES 
//       structure.
//--------------------------------------------------------------------------------------
void DisplaySwapCurrentControllerType( ATG::Font &font, DWORD dwPlayerIndex, BYTE ControllerType )
{
    XMFLOAT2 origin;
    ComputeOnScreenPlayerPosition( font, dwPlayerIndex, &origin );

    WCHAR szBuffer[ 128 ];
    GetControllerTypeText( ControllerType, szBuffer, 128 );

    font.DrawText( origin.x, origin.y + font.GetFontHeight(), TEXT_COLOR, szBuffer );
}


//--------------------------------------------------------------------------------------
// Name: DisplaySwapControllerList()
// Desc: Helper function that display the list of all controllers available for swapping 
//       along with the controller button that will select the controller.
//--------------------------------------------------------------------------------------
void DisplaySwapControllerList( ATG::Font& font, DWORD dwPlayerIndex, const ControllerList& controllerList )
{
    WCHAR szBuffer[ 128 ];

    WCHAR* szGlyph;

    XMFLOAT2 origin;
    ComputeOnScreenPlayerPosition( font, dwPlayerIndex, &origin );
    origin.x += 20.0f;

    for( INT i = 0; i < controllerList.nNumControllers; ++ i )
    {
        switch( i )
        {
            case 0:
                szGlyph = GLYPH_A_BUTTON;
                break;

            case 1:
                szGlyph = GLYPH_B_BUTTON;
                break;

            case 2:
                szGlyph = GLYPH_X_BUTTON;
                break;

            case 3:
                szGlyph = GLYPH_Y_BUTTON;
                break;

            default:
                assert( false );
                szGlyph = GLYPH_BULLET;
                break;
        }

        WCHAR szBuffer2[ 64 ];
        GetControllerTypeText( controllerList.ControllerType[ i ], szBuffer2, 64 );
        swprintf_s( szBuffer, L"%s %s (%ld)", szGlyph, szBuffer2, controllerList.dwPlayerIndex[ i ] + 1 );
        font.DrawText( origin.x, origin.y + ( i + 2 ) * font.GetFontHeight(), TEXT_COLOR, szBuffer );
    }
}


//--------------------------------------------------------------------------------------
// Name: HighlightSwapSelection()
// Desc: Helper function that displays an arrow in front of the currently selected item 
//       from the controller list for a given player.
//--------------------------------------------------------------------------------------
void HighlightSwapSelection( ATG::Font& font, DWORD dwPlayerIndex,  INT nSelectionIndex, DWORD dwColor )
{
    const WCHAR szBuffer[ 2 ] = GLYPH_RIGHT_ARROW L"";

    XMFLOAT2 origin;
    ComputeOnScreenPlayerPosition( font, dwPlayerIndex, &origin );
    font.DrawText( origin.x, origin.y + ( nSelectionIndex + 2 ) * font.GetFontHeight(), dwColor, szBuffer );
}


//--------------------------------------------------------------------------------------
// Name: DisplaySwapOptions()
// Desc: Helper function that display the option available at the bottom of the screen 
//       while the STATE_SWAP state is active.
//--------------------------------------------------------------------------------------
void DisplaySwapOptions( ATG::Font &font )
{
    FLOAT fDrawX = ( font.m_rcWindow.x2 - font.m_rcWindow.x1 ) / 2.0f;
    FLOAT fDrawY = font.m_rcWindow.y2 - 2 * font.GetFontHeight();

    font.DrawText( fDrawX, fDrawY, TEXT_COLOR, 
                   GLYPH_BACK_BUTTON L" Back  " GLYPH_START_BUTTON L"Proceed", ATGFONT_CENTER_X );
}
