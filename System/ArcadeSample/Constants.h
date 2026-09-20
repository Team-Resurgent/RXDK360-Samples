//--------------------------------------------------------------------------------------
// Constants.h
//
// Constants for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_CONSTANTS_H
#define ARCADESAMPLE_CONSTANTS_H


namespace ArcadeSample
{
#define VELOCITY_INIT_X     5.0f
#define VELOCITY_MAX_X      15.0f
#define VELOCITY_MULT_X     1.2f

#define VELOCITY_INIT_Y     1.0f
#define VELOCITY_MAX_Y      2.0f

//--------------------------------------------------------------------------------------
// Max number of players for our game
//--------------------------------------------------------------------------------------
const DWORD ARCADESAMPLE_VERSION = 1;

//--------------------------------------------------------------------------------------
// Max number of players for our game
//--------------------------------------------------------------------------------------
const DWORD ARCADESAMPLE_MAX_PLAYERS = 4;

//--------------------------------------------------------------------------------------
// XuiListItemLeaderboard Source IDs ( Column IDs )
//--------------------------------------------------------------------------------------
const INT ARCADESAMPLE_XUI_LEADERBOARD_COLUMN_RANK = 0;
const INT ARCADESAMPLE_XUI_LEADERBOARD_COLUMN_GAMERTAG = 1;
const INT ARCADESAMPLE_XUI_LEADERBOARD_COLUMN_WINS = 2;
const INT ARCADESAMPLE_XUI_LEADERBOARD_COLUMN_LOSSES = 3;

//--------------------------------------------------------------------------------------
// eLicense enumeration
//--------------------------------------------------------------------------------------
enum eLicense
{
    eLicense_Purchased,
};

//--------------------------------------------------------------------------------------
// eTeam enumeration
//--------------------------------------------------------------------------------------
enum eTeam
{
    eTeam_Invalid   = -1,
    eTeam_First     = 0,

    eTeam_Left      = eTeam_First,
    eTeam_Right,

    eTeam_Count
};

//--------------------------------------------------------------------------------------
// eArenaCollide enumeration
//--------------------------------------------------------------------------------------
enum eArenaCollide
{
    eArenaCollide_None,
    eArenaCollide_Wall,
    eArenaCollide_GoalRight,
    eArenaCollide_GoalLeft,
};

//--------------------------------------------------------------------------------------
// Network Message Id
//--------------------------------------------------------------------------------------
enum MSG_ID
{
    BALL_UPDATE,
    PADDLE_UPDATE,
    SCORE_UPDATE
};

//--------------------------------------------------------------------------------------
// eLanguage enumeration
//--------------------------------------------------------------------------------------
enum eLanguage
{
    eLanguage_Unknown,
    eLanguage_English,
    eLanguage_Japanese,
    eLanguage_German,
    eLanguage_French,
    eLanguage_Spanish,
    eLanguage_Italian,
    eLanguage_Korean,
    eLanguage_TChinese,
    eLanguage_Portuguese,
    eLanguage_SChinese,
    eLanguage_Polish,
    eLanguage_Russian,

    eLanguage_Count
};

//--------------------------------------------------------------------------------------
// Language string IDs associated with eLanguage enumeration index
//--------------------------------------------------------------------------------------
WCHAR const* const g_pXuiLocaleStrFromLanguage[ eLanguage_Count ] =
{
    L"unknown", // eLanguage_Unknown
    L"en-us",   // eLanguage_English
    L"ja-jp",   // eLanguage_Japanese
    L"de-de",   // eLanguage_German
    L"fr-fr",   // eLanguage_French
    L"es-es",   // eLanguage_Spanish
    L"it-it",   // eLanguage_Italian
    L"ko-kr",   // eLanguage_Korean
    L"zh-cht",  // eLanguage_TChinese
    L"es-es",   // eLanguage_Portuguese ( Reuse Spanish )
    L"zh-cht",  // eLanguage_SChinese ( Reuse TChinese )
    L"en-us",   // eLanguage_Polish ( Reuse English )
    L"en-us",   // eLanguage_Russian ( Reuse English )
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_CONSTANTS_H
