//--------------------------------------------------------------------------------------
// Xplat.h
//
// Common header file for integrating GFWL into a single codebase with the XBox 360
// version of a project
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#pragma warning( disable: 4995 ) // 'function': name was marked as #pragma deprecated

#ifdef _WIN32
#ifndef _XBOX
#define WIN32_LEAN_AND_MEAN
#include <winlive.h>
#endif
#endif

// forward declaration
class Sample;

// Function prototypes
#ifdef LIVE_ON_WINDOWS
#define FatalError CUtils::FatalError
#define DebugSpew CUtils::DebugSpew
#endif

// Winsock function calls
#ifdef _XBOX
#define Xplat_CreateSocket socket
#define Xplat_BindSocket bind
#define Xplat_IOCTLSocket ioctlsocket
#define Xplat_HTONS htons
#define Xplat_NTOHS ntohs
#define Xplat_WSAGetLastError WSAGetLastError
#define Xplat_SendTo sendto
#define Xplat_RecvFrom recvfrom
#else if LIVE_ON_WINDOWS
#define Xplat_CreateSocket XSocketCreate
#define Xplat_BindSocket XSocketBind
#define Xplat_IOCTLSocket XSocketIOCTLSocket
#define Xplat_HTONS XSocketHTONS
#define Xplat_NTOHS XSocketNTOHS
#define Xplat_WSAGetLastError XWSAGetLastError
#define Xplat_SendTo XSocketSendTo
#define Xplat_RecvFrom XSocketRecvFrom
#endif

#include <map>
#include <math.h>
#include <stack>
#include <vector>

#ifdef _XBOX
#include "stdafx.h"
#include <XParty.h>
#include <AtgUtil.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSignIn.h>
#include "TrueSkillBar.h"
#include "XGUI.h"
#define MAX_USER_COUNT XUSER_MAX_COUNT
#else if LIVE_ON_WINDOWS
//#define WIN32_LEAN_AND_MEAN
// #define MAX_USER_COUNT to ( 1 ) if you don't care about cross-platform play
// with Xbox 360 version of the sample
#define MAX_USER_COUNT XUSER_MAX_COUNT
//#include <winlive.h>
#include "Sample.h"
#include "SampleUtils.h"
#endif

// Function prototypes
#ifdef _XBOX
#define FatalError ATG::FatalError
#define DebugSpew ATG::DebugSpew
#define XNKIDToInt64 ATG::XNKIDToInt64
#define GetVideoSettings ATG::GetVideoSettings
#endif

// UI string mappings
#ifdef _XBOX
#define LEFT_ARROW L"<"
#define RIGHT_ARROW L">"
#define DRAW_CENTER_STYLE ATGFONT_CENTER_X
#define DRAW_RIGHT_STYLE ATGFONT_RIGHT
#define DRAW_LEFT_STYLE ATGFONT_LEFT
#define UI_ELEMENT_BACK GLYPH_B_BUTTON L" : "
#define UI_ELEMENT_NOTIFY L"LB : "
#define UI_ELEMENT_FRIENDS GLYPH_Y_BUTTON L" : "
#define UI_ELEMENT_WAVE GLYPH_X_BUTTON L" : "
#define UI_ELEMENT_CANCELXOVERLAPPED L"RB : "
#define UI_TEXT_INSESSION_BEGINGAME L"Press START to begin game"
#define UI_TEXT_INSESSION_SCOREPOINT L"Press " GLYPH_A_BUTTON L"to score a point"
#define UI_TEXT_INSESSION_GAMERTAGPADDING 25
#define USER_INPUT_SELECTION_WAVE XINPUT_GAMEPAD_X
#define USER_INPUT_SELECTION_SCOREPOINT XINPUT_GAMEPAD_A
#define USER_INPUT_SELECTION_STARTGAME XINPUT_GAMEPAD_START
#define USER_INPUT_SELECTION_TOGGLE XINPUT_GAMEPAD_RIGHT_SHOULDER
#define USER_INPUT_SELECTION_CANCEL_OVERLAPPED_OPERATION XINPUT_GAMEPAD_RIGHT_SHOULDER
#define USER_INPUT_SELECTION_CHANGE_NOTIF_POSITION XINPUT_GAMEPAD_LEFT_SHOULDER
#define USER_INPUT_SELECTION_UPDATE_PRESENCE XINPUT_GAMEPAD_LEFT_THUMB
#define USER_INPUT_SELECTION_SHOW_FRIENDS_LIST XINPUT_GAMEPAD_Y
#define USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_LEADERBOARD XINPUT_GAMEPAD_A
#define USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_GAMETYPE XINPUT_GAMEPAD_RIGHT_SHOULDER
#define USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_ALPHABLENDING XINPUT_GAMEPAD_RIGHT_THUMB
#define USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_ZOOM_TOP XINPUT_GAMEPAD_X
#define USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_ZOOM_BOTTOM XINPUT_GAMEPAD_Y
#define USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_ZOOM_USER XINPUT_GAMEPAD_LEFT_SHOULDER
#else if LIVE_ON_WINDOWS
#define LEFT_ARROW L"<"
#define RIGHT_ARROW L">"
#define DRAW_CENTER_STYLE DT_CENTER
#define DRAW_RIGHT_STYLE DT_RIGHT
#define DRAW_LEFT_STYLE DT_LEFT
#define UI_ELEMENT_BACK L"BS:           "
#define UI_ELEMENT_NOTIFY L"N:          "
#define UI_ELEMENT_FRIENDS L"F:         "
#define UI_ELEMENT_WAVE L"W:        "
#define UI_ELEMENT_CANCELXOVERLAPPED L"SPACE:        "
#define UI_TEXT_INSESSION_BEGINGAME L"Press ENTER to begin game"
#define UI_TEXT_INSESSION_SCOREPOINT L"Press ENTER to score a point"
#define UI_TEXT_INSESSION_GAMERTAGPADDING 40
#define USER_INPUT_SELECTION_WAVE 'W'
#define USER_INPUT_SELECTION_SCOREPOINT VK_RETURN
#define USER_INPUT_SELECTION_STARTGAME VK_RETURN
#define USER_INPUT_SELECTION_TOGGLE 'L'
#define USER_INPUT_SELECTION_CANCEL_OVERLAPPED_OPERATION VK_SPACE
#define USER_INPUT_SELECTION_CHANGE_NOTIF_POSITION 'N'
#define USER_INPUT_SELECTION_UPDATE_PRESENCE 'P'
#define USER_INPUT_SELECTION_SHOW_FRIENDS_LIST 'F'
#define USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_LEADERBOARD 'L'
#define USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_GAMETYPE 'T'
#define USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_ALPHABLENDING 'A'
#define USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_ZOOM_TOP VK_PRIOR
#define USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_ZOOM_BOTTOM VK_NEXT
#define USER_INPUT_SELECTION_TOGGLE_VIEW_STATS_ZOOM_USER 'M'
#endif

#include <xtl.h>
#include "AtgSessionManager.h"
#include "ClientInfo.h"
#include "HostMigration.h"
#include "Messages.h"
#include "InGamePartyMatchmaking.spa.h"
#include "TaskScheduler.h"

// Cross-platform wrapper class for drawing
class CXPlat_Draw
{
public:
    inline CXPlat_Draw( Sample* pSample );
    inline ~CXPlat_Draw() {}
    inline BOOL Initialize();
    
    inline VOID BeginDraw();
    inline VOID EndDraw();
    inline VOID BeginFont( BOOL bUseLargeFont = FALSE );
    inline VOID EndFont( BOOL bUseLargeFont = FALSE );
        
    inline FLOAT GetXPosition() const;
    inline FLOAT GetYPosition() const;
    inline FLOAT GetCenterXPosition() const;
    inline FLOAT GetCenterYPosition() const;
    inline FLOAT GetWindowWidth() const;
    inline FLOAT GetWindowHeight() const;
    inline VOID SetXPosition( FLOAT x );
    inline VOID SetYPosition( FLOAT y );
    inline VOID IncrementXPosition( FLOAT deltaX );
    inline VOID IncrementYPosition( FLOAT deltaY );
    inline VOID DecrementXPosition( FLOAT deltaX );
    inline VOID DecrementYPosition( FLOAT deltaY );

#ifdef _XBOX
    inline ATG::Font& GetLargeFont();
    inline ATG::Font& GetSmallFont();
#endif

    inline FLOAT GetFontHeight( BOOL bUseLargeFont = FALSE ) const;
    inline FLOAT GetTextWidth( const WCHAR* str ) const;
    inline VOID SetScaleFactor( FLOAT factorX, FLOAT factorY );
    inline VOID DrawText( FLOAT x, 
                          FLOAT y, 
                          D3DCOLOR col, 
                          const WCHAR* str,
                          BOOL bUseLargeFont = FALSE,
                          DWORD style = 0, 
                          FLOAT len = -1 );

    inline VOID DrawTextFromCurrentXY( D3DCOLOR col, 
                                       const WCHAR* str, 
                                       DWORD style = DRAW_CENTER_STYLE, 
                                       FLOAT len = -1 );

    inline VOID DrawTextCentered( FLOAT y, D3DCOLOR col, const WCHAR* str );
    inline VOID DrawTextCenterScreen( D3DCOLOR col, const WCHAR* str );

    inline HRESULT CreateBackground();
    inline VOID DestroyBackground();

private:
    CXPlat_Draw& operator=( const CXPlat_Draw& ref );

private:
    Sample*             m_pSample;
    FLOAT               m_X;
    FLOAT               m_Y;
    FLOAT               m_fCenterX;
    FLOAT               m_fCenterY;
#ifdef _XBOX
    ATG::Font           m_Font16;
    ATG::Font           m_Font12;
#else if LIVE_ON_WINDOWS
    CBackground         m_Background;
#endif
};

// Cross-platform wrapper class for processing user input
class CXPlat_UserInput
{
public:
    inline CXPlat_UserInput( Sample* pSample );
    inline ~CXPlat_UserInput() {}

    inline BOOL Update();
    inline VOID MergeInputs( DWORD dwMask, DWORD* pdwActiveGamePadsMask );
    inline BOOL GotoNextMenuItem( DWORD dwControllerID = 0 );
    inline BOOL GotoPreviousMenuItem( DWORD dwControllerID = 0 );
    inline BOOL SelectCurrentMenuItem( DWORD dwControllerID = 0 );
    inline BOOL GotoNextMenuItemValue( DWORD dwControllerID = 0 );
    inline BOOL GotoPreviousMenuItemValue( DWORD dwControllerID = 0 );
    inline BOOL ReturnToPreviousUIScreen( DWORD dwControllerID = 0 );
    inline BOOL WasUserInputSelected( DWORD dwUserInput, DWORD dwControllerID = 0 );
    inline DWORD GetLastUserInput( DWORD dwControllerID = 0 );

private:
    Sample*             m_pSample;
#ifdef _XBOX
    ATG::GAMEPAD        m_gamepads[MAX_USER_COUNT];
    ATG::GAMEPAD*       m_gamepad;
#else if LIVE_ON_WINDOWS
#endif
};

// Cross-platform wrapper class for signin
class CXPlat_Signin
{
public:
    inline CXPlat_Signin( Sample* pSample );
    inline ~CXPlat_Signin() {}

    inline VOID Initialize( 
                            DWORD dwMinUsers = 1, 
                            DWORD dwMaxUsers = MAX_USER_COUNT,
                            BOOL  bRequireOnlineUsers = TRUE,
                            DWORD dwSignInPanes = MAX_USER_COUNT );

    inline VOID Cleanup();
    inline VOID Update();
    inline VOID ShowSigninUI();
    inline DWORD GetSignedInUserMask();
    inline DWORD GetLowestSignedInControllerID();
    inline BOOL HasConnectionChanged();
    inline BOOL HaveSigninUsersChanged();
    inline BOOL IsSystemUIShowing();
    inline BOOL AreUsersSignedIn();
    inline BOOL AreUsersSignedInToLive();
    inline BOOL AreUsersSignedInLocally();
    
private:
    CXPlat_Signin& operator=( const CXPlat_Signin& ref );

private:
    Sample*             m_pSample;
    BOOL                m_bRequireOnlineUsers;
#ifdef _XBOX
    DWORD               m_dwUpdateFlags;
#else if LIVE_ON_WINDOWS
    CSignIn             m_SignIn;
#endif
};

// Cross-platform wrapper class for Live network startup/cleanup
class CXPlat_XOnline
{
public:
    inline CXPlat_XOnline( Sample* pSample );
    inline ~CXPlat_XOnline() {}

    inline VOID Startup();
    inline VOID Cleanup();
    inline HRESULT OnDeviceCreated();
    inline HRESULT OnDeviceReset();
    inline HRESULT OnDeviceDestroyed();

private:
    Sample*             m_pSample;
#ifdef LIVE_ON_WINDOWS
    CXLiveManager      m_XLiveManager;
#endif
};

#include "InGamePartyMatchmaking.h"
#include "XPlatImpl.h"
