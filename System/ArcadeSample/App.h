//--------------------------------------------------------------------------------------
// App.h
//
// Application class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_APP_H
#define ARCADESAMPLE_APP_H

#include "Player.h"
#include "Leaderboards.h"
#include "GamerPics.h"

#include "XuiBall.h"
#include "XuiArena.h"
#include "XuiScoreboard.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Application Class
//--------------------------------------------------------------------------------------
class AppClass : public ATG::Application,
                 public IQNetCallbacks
{
public:
    typedef std::list <Player*> PlayerContainerT;
    typedef PlayerContainerT::iterator PlayerItT;
    typedef PlayerContainerT::const_iterator PlayerCItT;

    typedef std::vector <PlayerInfo> PlayerInfoContainerT;
    typedef PlayerInfoContainerT::iterator PlayerInfoItT;
    typedef PlayerInfoContainerT::const_iterator PlayerInfoCItT;

public:
    // Ctors
                        AppClass();

    PlayerItT           PlayerBegin();
    PlayerItT           PlayerEnd();

    PlayerInfo const& GetEndGamePlayerInfoForUIIndex( INT iUIIndex ) const;

    Player* PlayerForUIIndex( INT iUIIndex );
    Player* GetLocalPlayer( DWORD dwUserIndex );

    VOID                SetControllerHasUser( DWORD dwUserIndex, BOOL bVal = TRUE );
    VOID                ResetUserMask();
    VOID                BindCmdUserIndex( DWORD dwUserIndex );

    VOID                ConfigureGamePlayers();

    // Score Queries
    VOID                SetTeamScore( eTeam team, DWORD dwScore );
    DWORD               GetTeamScore( eTeam team ) const;
    VOID                ClearScore();
    BOOL                IsWinningTeam( eTeam team ) const;

    DWORD               GetEndGameScore() const;
    VOID                SetEndGameScore( DWORD dwEndGameScore );
    BOOL                IsGameOver() const;

    // Leaderboard Queries
    DWORD               GetLeaderboardItemCount();
    LeaderboardItem const* GetLeaderboardItem( DWORD dwItem );
    DWORD               GetDefaultLeaderboardItemIndex();
    HRESULT             RefreshLeaderboard( XUSER_STATS_SPEC* pStatsSpec, Leaderboards::StatsFilter statsFilter );
    BOOL                IsLeaderboardRefreshing();
    VOID                WriteStats();

    // GamerPics Queries
    HXUIBRUSH           GetGamerPicBrush( XUID xuidPlayer );
    VOID                RefreshGamerPic( DWORD dwUserIndex );

    // D3D Routines
    VOID                SuspendD3DDevice() const;

    // License Query
    BOOL                CheckLicense( eLicense license );

    // XShow*UI Functions
    VOID                ShowGamerCardForXuid( XUID xuid );
    VOID                ShowPlayerReviewForXuid( XUID xuid );

    // QNet
    HRESULT             StartQNet( QNET_SESSIONTYPE SessionType );
    VOID                StopQNet();

    VOID                SetCameraStreams( QNET_CAMERA_STREAMS streams );
    HRESULT             RenderCamera( Player* pPlayer, D3DRECT const& rc );

    HRESULT             SearchForQNetGames();
    VOID                AbortQNetGameSearch();

    HRESULT             AddLocalPlayer( DWORD dwUserIndex );
    BOOL                HasProperTeamPlayers();
    BOOL                IsEveryoneReady();

    HRESULT             HostQNetGame();
    HRESULT             JoinQNetGame( const XSESSION_SEARCHRESULT* pSearchResult );
    HRESULT             StartQNetGame();
    HRESULT             EndQNetGame();
    VOID                LeaveQNetGame();

    BOOL                IsHost();

    IQNetGameSearch* GetQNetGameSearch();
    QNET_SESSIONTYPE    GetQNetSessionType();
    BOOL                GetQNetState( QNET_STATE* pState );

    VOID                SetActiveXuiScene( AppXuiScene* pXuiScene );

    VOID                InitGameplay(
CXuiControl* figBall,
CXuiControl* figBorder,
CXuiControl* figPaddles,
CXuiControl* labScores,
CXuiControl* labCountdown );

    VOID                ResetBall();
    VOID                SendBallUpdate();
    VOID                MoveBall( FLOAT speed );
    VOID                MovePaddles( FLOAT speed );
    VOID                Pause( BOOL pause )
    {
        m_Pause = pause;
    }
    BOOL                IsPaused() const
    {
        return m_Pause;
    }

protected:
    // Overridable functions
    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();

    // IQNetCallbacks Interface
    virtual VOID        NotifyCommSettingsChanged( IQNetPlayer* pQNetPlayer );
    virtual VOID        NotifyContextChanged( const XUSER_CONTEXT* pContext );
    virtual VOID        NotifyDataReceived( IQNetPlayer* pQNetPlayerFrom, DWORD dwNumPlayersTo,
                                            IQNetPlayer** apQNetPlayersTo, const BYTE* pbData, DWORD dwDataSize );
    virtual VOID        NotifyGameInvite( DWORD dwUserIndex, const XINVITE_INFO* pInviteInfo );
    virtual VOID        NotifyGameSearchComplete( IQNetGameSearch* pGameSearch, HRESULT hrComplete,
                                                  DWORD dwNumResults );
    virtual VOID        NotifyNewHost( IQNetPlayer* pQNetPlayer );
    virtual VOID        NotifyPlayerJoined( IQNetPlayer* pQNetPlayer );
    virtual VOID        NotifyPlayerLeaving( IQNetPlayer* pQNetPlayer );
    virtual VOID        NotifyPropertyChanged( const XUSER_PROPERTY* pProperty );
    virtual VOID        NotifyReadinessChanged( IQNetPlayer* pQNetPlayer, BOOL bReady );
    virtual VOID        NotifyStateChanged( QNET_STATE OldState, QNET_STATE NewState, HRESULT hrInfo );
    virtual VOID        NotifyWriteStats( IQNetPlayer* pQNetPlayer );

private:
    // Localization functions
    VOID                ConfigureLanguage();

    // Players
    static Player* PlayerFromQNetPlayer( IQNetPlayer* pQNetPlayer );
    Player* PlayerFromXuid( XUID xuid );

    VOID                RemoveAllAIPlayers();

    VOID                ComputeUserMask();

    // Gameplay
    VOID                UpdateGameplay();

private:
    BOOL m_bRunning;

    eLanguage m_Language;

    HANDLE m_hNotify;

    DWORD m_dwCmdUserIndex;
    DWORD m_dwUserMask;

    PlayerInfoContainerT m_EndGamePlayerInfos;
    PlayerContainerT m_SessionPlayers;

    Leaderboards m_Leaderboards;
    GamerPics m_GamerPics;

    BOOL m_bQNetWorking;
    BOOL m_bStopQNet;

    IQNet* m_pQNet;
    IQNetCameraManager* m_pIQNetCameraManager;
    IQNetGameSearch* m_pQNetSearch;

    QNET_STATE m_LastState;
    QNET_STATE m_CurrentState;

    AppXuiModule m_XuiModule;
    AppXuiScene* m_pActiveXuiScene;

    XuiBall m_Ball;
    XuiArena m_Arena;
    XuiScoreboard m_Scoreboard;

    BOOL m_Playing;
    BOOL m_Pause;

    DWORD m_dwLastFrame;

    IXAudio2 *m_pXAudio2;
};

//--------------------------------------------------------------------------------------
// Remove All AI players
//--------------------------------------------------------------------------------------
inline VOID AppClass::RemoveAllAIPlayers()
{
    for( AppClass::PlayerItT playerIt = PlayerBegin(); playerIt != PlayerEnd(); )
    {
        if( ( *playerIt )->IsAI() )
        {
            playerIt = m_SessionPlayers.erase( playerIt );
        }
        else
        {
            ++playerIt;
        }
    }
}

//--------------------------------------------------------------------------------------
// Get a ArcadeSample::Player with matching Xuid
//--------------------------------------------------------------------------------------
inline Player* AppClass::PlayerForUIIndex( INT iUIIndex )
{
    eTeam team = ( iUIIndex & 1 )
        ? eTeam_Right : eTeam_Left;

    INT member = ( iUIIndex & 2 )
        ? 2 : 1;

    Player* pPlayer = NULL;
    for( AppClass::PlayerItT playerIt = PlayerBegin(); ( pPlayer == NULL ) && ( playerIt != PlayerEnd() ); ++playerIt )
    {
        if( ( *playerIt )->GetTeam() == team )
        {
            if( --member == 0 )
                pPlayer = *playerIt;
        }
    }

    return pPlayer;
}

//--------------------------------------------------------------------------------------
// Get a ArcadeSample::Player with matching Xuid
//--------------------------------------------------------------------------------------
inline Player* AppClass::PlayerFromXuid( XUID xuid )
{
    Player* pPlayer = NULL;

    for( AppClass::PlayerItT playerIt = PlayerBegin(); ( pPlayer == NULL ) && ( playerIt != PlayerEnd() ); ++playerIt )
    {
        if( ( *playerIt )->GetXuid() == xuid )
        {
            pPlayer = *playerIt;
        }
    }

    return pPlayer;
}

//--------------------------------------------------------------------------------------
// Get a ArcadeSample::Player attached to the QNetPlayer
//--------------------------------------------------------------------------------------
inline Player* AppClass::PlayerFromQNetPlayer( IQNetPlayer* pQNetPlayer )
{
    return reinterpret_cast<Player*>( pQNetPlayer->GetCustomDataValue() );
}

//--------------------------------------------------------------------------------------
// Player begin iterator
//--------------------------------------------------------------------------------------
inline AppClass::PlayerItT AppClass::PlayerBegin()
{
    return m_SessionPlayers.begin();
}

//--------------------------------------------------------------------------------------
// Player end iterator
//--------------------------------------------------------------------------------------
inline AppClass::PlayerItT AppClass::PlayerEnd()
{
    return m_SessionPlayers.end();
}

//--------------------------------------------------------------------------------------
// Get the registered session player with the specified local index
//--------------------------------------------------------------------------------------
inline Player* AppClass::GetLocalPlayer( DWORD dwUserIndex )
{
    IQNetPlayer* pQNetPlayer = m_pQNet->GetLocalPlayerByUserIndex( dwUserIndex );
    return ( pQNetPlayer ) ? PlayerFromQNetPlayer( pQNetPlayer ) : NULL;
}

//--------------------------------------------------------------------------------------
// Get end game player information, these players existed at the end of the last game
//--------------------------------------------------------------------------------------
inline PlayerInfo const& AppClass::GetEndGamePlayerInfoForUIIndex( INT iUIIndex ) const
{
    return m_EndGamePlayerInfos[ iUIIndex ];
}

//--------------------------------------------------------------------------------------
// Mark a controller as having a user or not
//--------------------------------------------------------------------------------------
inline VOID AppClass::SetControllerHasUser( DWORD dwUserIndex, BOOL bVal )
{
    if( bVal )
    {
        OutputDebugString( "AppClass::SetControllerHasUser( true ).\n" );
        m_dwUserMask |= ( 1 << dwUserIndex );
    }
    else
    {
        OutputDebugString( "AppClass::SetControllerHasUser( false ).\n" );
        m_dwUserMask &= ~( 1 << dwUserIndex );
    }
}

//--------------------------------------------------------------------------------------
// Reset the user mask
//--------------------------------------------------------------------------------------
inline VOID AppClass::ResetUserMask()
{
    m_dwUserMask = 0;
}

//--------------------------------------------------------------------------------------
// Bind the command user to the specified index
//--------------------------------------------------------------------------------------
inline VOID AppClass::BindCmdUserIndex( DWORD dwUserIndex )
{
    m_dwCmdUserIndex = dwUserIndex;
}

//--------------------------------------------------------------------------------------
// Set a given team's score
//--------------------------------------------------------------------------------------
inline VOID AppClass::SetTeamScore( eTeam team, DWORD dwScore )
{
    m_Scoreboard.SetScore( team, dwScore );
}

//--------------------------------------------------------------------------------------
// Get a specified team's score
//--------------------------------------------------------------------------------------
inline DWORD AppClass::GetTeamScore( eTeam team ) const
{
    return m_Scoreboard.GetScore( team );
}

//--------------------------------------------------------------------------------------
// Reset the scores
//--------------------------------------------------------------------------------------
inline VOID AppClass::ClearScore()
{
    m_Scoreboard.ClearScore();
}

//--------------------------------------------------------------------------------------
// Is the specified team winning?
//--------------------------------------------------------------------------------------
inline BOOL AppClass::IsWinningTeam( eTeam team ) const
{
    return m_Scoreboard.IsWinningTeam( team );
}

//--------------------------------------------------------------------------------------
// Get the current 'End Game Score'
//--------------------------------------------------------------------------------------
inline DWORD AppClass::GetEndGameScore() const
{
    return m_Scoreboard.GetEndGameScore();
}

//--------------------------------------------------------------------------------------
// Set the current 'End Game Score'
//--------------------------------------------------------------------------------------
inline VOID AppClass::SetEndGameScore( DWORD dwEndGameScore )
{
    m_Scoreboard.SetEndGameScore( dwEndGameScore );
}

//--------------------------------------------------------------------------------------
// Is the game over, based on the current score?
//--------------------------------------------------------------------------------------
inline BOOL AppClass::IsGameOver() const
{
    return m_Scoreboard.IsGameOver();
}

//--------------------------------------------------------------------------------------
// Leaderboard item count
//--------------------------------------------------------------------------------------
inline DWORD AppClass::GetLeaderboardItemCount()
{
    return m_Leaderboards.GetTotalItemCount();
}

//--------------------------------------------------------------------------------------
// Get a specific leaderboard item 
//--------------------------------------------------------------------------------------
inline LeaderboardItem const* AppClass::GetLeaderboardItem( DWORD dwItem )
{
    return m_Leaderboards.GetItem( dwItem );
}

//--------------------------------------------------------------------------------------
// Get a default leaderboard item index
//--------------------------------------------------------------------------------------
inline DWORD AppClass::GetDefaultLeaderboardItemIndex()
{
    return m_Leaderboards.GetDefaultItem();
}

//--------------------------------------------------------------------------------------
// Refresh the leaderboard
//--------------------------------------------------------------------------------------
inline HRESULT AppClass::RefreshLeaderboard( XUSER_STATS_SPEC* pStatsSpec, Leaderboards::StatsFilter statsFilter )
{
    return m_Leaderboards.Refresh(
        m_dwCmdUserIndex, pStatsSpec, statsFilter );
}

//--------------------------------------------------------------------------------------
// Query if the leaderboard is refreshing
//--------------------------------------------------------------------------------------
inline BOOL AppClass::IsLeaderboardRefreshing()
{
    return m_Leaderboards.IsRefreshing();
}

//--------------------------------------------------------------------------------------
// Get a XuiBrush for the GamerPic of a specied gamer XUID
//--------------------------------------------------------------------------------------
inline HXUIBRUSH AppClass::GetGamerPicBrush( XUID xuidPlayer )
{
    return m_GamerPics.GetGamerPicBrush( xuidPlayer );
}

//--------------------------------------------------------------------------------------
// Refresh the gamerpic of the specified controller index
//--------------------------------------------------------------------------------------
inline VOID AppClass::RefreshGamerPic( DWORD dwUserIndex )
{
    m_GamerPics.Refresh( dwUserIndex, m_pd3dDevice );
}

//--------------------------------------------------------------------------------------
// D3D Suspend
//--------------------------------------------------------------------------------------
inline VOID AppClass::SuspendD3DDevice() const
{
    m_pd3dDevice->Suspend();
}

//--------------------------------------------------------------------------------------
// Check the license to see if this is the full version
//--------------------------------------------------------------------------------------
inline BOOL AppClass::CheckLicense( eLicense license )
{
    DWORD dwLicenseMask;
    XContentGetLicenseMask( &dwLicenseMask, NULL );

    return ( ( dwLicenseMask & ( 1 << license ) ) != 0 );
}

//--------------------------------------------------------------------------------------
// ShowGamerCardForXuid function
//--------------------------------------------------------------------------------------
inline VOID AppClass::ShowGamerCardForXuid( XUID xuid )
{
    XShowGamerCardUI( m_dwCmdUserIndex, xuid );
}

//--------------------------------------------------------------------------------------
// ShowPlayerReviewForXuid function
//--------------------------------------------------------------------------------------
inline VOID AppClass::ShowPlayerReviewForXuid( XUID xuid )
{
    XShowPlayerReviewUI( m_dwCmdUserIndex, xuid );
}

//--------------------------------------------------------------------------------------
// Add a local player 
//--------------------------------------------------------------------------------------
inline HRESULT AppClass::AddLocalPlayer( DWORD dwUserIndex )
{
    OutputDebugString( "AppClass::AddLocalPlayer().\n" );

    ATG_Verify( m_pQNet );
    return m_pQNet->AddLocalPlayerByUserIndex( dwUserIndex );
}

//--------------------------------------------------------------------------------------
// Have a proper team player?
//--------------------------------------------------------------------------------------
inline BOOL AppClass::HasProperTeamPlayers()
{
    DWORD dwTeam[ eTeam_Count ] = { 0 };

    // Count active team players
    for( AppClass::PlayerItT playerIt = PlayerBegin(); playerIt != PlayerEnd(); ++playerIt )
    {
        ++dwTeam[ ( *playerIt )->GetTeam() ];
    }

    return ( ( dwTeam[ eTeam_Left ] > 0 ) && ( dwTeam[ eTeam_Right ] > 0 ) );
}

//--------------------------------------------------------------------------------------
// Is everyone who's in the game ready?
//--------------------------------------------------------------------------------------
inline BOOL AppClass::IsEveryoneReady()
{
    ATG_Verify( m_pQNet );
    return m_pQNet->IsEveryoneReady();
}

//--------------------------------------------------------------------------------------
// Is QNet Host?
//--------------------------------------------------------------------------------------
inline BOOL AppClass::IsHost()
{
    ATG_Verify( m_pQNet );
    return m_pQNet->IsHost();
}

//--------------------------------------------------------------------------------------
// SetCameraStreams
//--------------------------------------------------------------------------------------
inline VOID AppClass::SetCameraStreams( QNET_CAMERA_STREAMS streams )
{
    if( m_pIQNetCameraManager )
    {
        m_pIQNetCameraManager->SetCameraStreams( streams );
    }
}

//--------------------------------------------------------------------------------------
// GetQNetGameSearch
//--------------------------------------------------------------------------------------
inline IQNetGameSearch* AppClass::GetQNetGameSearch()
{
    return m_pQNetSearch;
}

//--------------------------------------------------------------------------------------
// Track active XuiScene
//--------------------------------------------------------------------------------------
inline VOID AppClass::SetActiveXuiScene( AppXuiScene* pXuiScene )
{
    m_pActiveXuiScene = pXuiScene;
}

//--------------------------------------------------------------------------------------
// Start QNet Game
//--------------------------------------------------------------------------------------
inline HRESULT AppClass::StartQNetGame()
{
    OutputDebugString( "AppClass::StartQNetGame().\n" );

    ATG_Verify( m_pQNet );
    return m_pQNet->StartGame();
}

//--------------------------------------------------------------------------------------
// End QNet Game
//--------------------------------------------------------------------------------------
inline HRESULT AppClass::EndQNetGame()
{
    OutputDebugString( "AppClass::EndQNetGame().\n" );

    ATG_Verify( m_pQNet );
    m_Playing = FALSE;

    return m_pQNet->EndGame();
}

// Global access to a single application class
extern AppClass g_App;

} // namespace ArcadeSample

#endif // ARCADESAMPLE_APP_H
