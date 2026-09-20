//--------------------------------------------------------------------------------------
// App.cpp
//
// Application class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "App.h"

#include <xtitleidbegin.h>
XEX_TITLE_ID( TITLEID_ARCADESAMPLE ) // Title ID for ArcadeSample
#include <xtitleidend.h>


//--------------------------------------------------------------------------------------
// Application entry point.
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    // Run the application    
    ArcadeSample::g_App.Run();
}


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Global access to a single application class
//--------------------------------------------------------------------------------------
AppClass g_App;

//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
AppClass::AppClass() : ATG::Application(),
                       m_bRunning( FALSE ),
                       m_hNotify( NULL ),
                       m_dwCmdUserIndex( XUSER_INDEX_ANY ),
                       m_dwUserMask( 0 ),
                       m_Language( eLanguage_English ),
                       m_EndGamePlayerInfos(),
                       m_SessionPlayers(),
                       m_Leaderboards(),
                       m_GamerPics(),
                       m_XuiModule(),
                       m_bQNetWorking( FALSE ),
                       m_bStopQNet( FALSE ),
                       m_pQNet( NULL ),
                       m_pQNetSearch( NULL ),
                       m_LastState( QNET_STATE_IDLE ),
                       m_CurrentState( QNET_STATE_IDLE ),
                       m_pActiveXuiScene( NULL ),
                       m_Ball(),
                       m_Arena(),
                       m_Scoreboard(),
                       m_Playing( FALSE ),
                       m_Pause( FALSE ),
                       m_dwLastFrame( GetTickCount() )
{
    m_EndGamePlayerInfos.reserve( ARCADESAMPLE_MAX_PLAYERS );

    m_d3dpp.BackBufferWidth = 1280;
    m_d3dpp.BackBufferHeight = 720;
}

//--------------------------------------------------------------------------------------
// Initialize the application
//--------------------------------------------------------------------------------------
HRESULT AppClass::Initialize()
{
    OutputDebugString( "AppClass::Initialize().\n" );

    // Initialize XAudio

    HRESULT hResult = XAudio2Create( &m_pXAudio2, 0 );

    IXAudio2MasteringVoice* pMasteringVoice = NULL;
    if ( !FAILED( hResult ) )
    {
        hResult = m_pXAudio2->CreateMasteringVoice( &pMasteringVoice, XAUDIO2_DEFAULT_CHANNELS,
                               XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, NULL );
    }
    if( FAILED( hResult ) )
    {
        OutputDebugString( "Failed intializing XAudio2.\n" );
        return hResult;
    }

    // Initialize XOnline
    hResult = HRESULT_FROM_WIN32(XOnlineStartup());
    if( FAILED( hResult ) )
    {
        OutputDebugString( "XOnlineStartup failed.\n" );
        return hResult;
    }

    // Initialize XUI
    hResult = m_XuiModule.InitShared( m_pd3dDevice, &m_d3dpp, XuiD3DXTextureLoader );
    if( FAILED( hResult ) )
    {
        OutputDebugString( "Failed intializing XuiModule.\n" );
        return hResult;
    }

    // Set the application language
    this->ConfigureLanguage();

    // Register a default typeface
    hResult = m_XuiModule.RegisterDefaultTypeface( L"Arial Unicode MS",
                                                   L"file://game:/ArcadeSample.xzp#xarialuni.ttf" );
    if( FAILED( hResult ) )
    {
        OutputDebugString( "Failed to register default typeface.\n" );
        return hResult;
    }

    // Load the skin file
    hResult = m_XuiModule.LoadSkin( L"file://game:/ArcadeSample.xzp#skin.xur" );
    if( FAILED( hResult ) )
    {
        OutputDebugString( "Failed to load the skin.\n" );
        return hResult;
    }

    // Load the first scene
    hResult = m_XuiModule.LoadFirstScene( L"file://game:/ArcadeSample.xzp#", L"MainMenuScene.xur", NULL );
    if( FAILED( hResult ) )
    {
        OutputDebugString( "Failed to load the scene.\n" );
        return hResult;
    }

    // Create the notify listener
    m_hNotify = XNotifyCreateListener( XNOTIFY_ALL );
    if( m_hNotify == NULL )
    {
        OutputDebugString( "Failed to create a notification listener.\n" );
        return ERROR_FUNCTION_FAILED;
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Set the Language from the XGetLanguage setting
//--------------------------------------------------------------------------------------
VOID AppClass::ConfigureLanguage()
{
    DWORD dwLanguage = XGetLanguage();
    switch( dwLanguage )
    {
        case XC_LANGUAGE_ENGLISH:
            m_Language = eLanguage_English;     break;
        case XC_LANGUAGE_JAPANESE:
            m_Language = eLanguage_Japanese;    break;
        case XC_LANGUAGE_GERMAN:
            m_Language = eLanguage_German;      break;
        case XC_LANGUAGE_FRENCH:
            m_Language = eLanguage_French;      break;
        case XC_LANGUAGE_SPANISH:
            m_Language = eLanguage_Spanish;     break;
        case XC_LANGUAGE_ITALIAN:
            m_Language = eLanguage_Italian;     break;
        case XC_LANGUAGE_KOREAN:
            m_Language = eLanguage_Korean;      break;
        case XC_LANGUAGE_TCHINESE:
            m_Language = eLanguage_TChinese;    break;
        case XC_LANGUAGE_PORTUGUESE:
            m_Language = eLanguage_Portuguese;  break;
        case XC_LANGUAGE_SCHINESE:
            m_Language = eLanguage_SChinese;    break;
        case XC_LANGUAGE_POLISH:
            m_Language = eLanguage_Polish;      break;
        case XC_LANGUAGE_RUSSIAN:
            m_Language = eLanguage_Russian;     break;

        default:
            m_Language = eLanguage_Unknown;     break;
    }

    if( ( m_Language == eLanguage_English ) || ( m_Language == eLanguage_Unknown ) )
    {
        // English is the default XUI language - do nothing
    }
    else
    {
        XuiSetLocale( g_pXuiLocaleStrFromLanguage[ m_Language ] );
    }
}

//--------------------------------------------------------------------------------------
// Configure the game players
//--------------------------------------------------------------------------------------
VOID AppClass::ConfigureGamePlayers()
{
    OutputDebugString( "AppClass::ConfigureGamePlayers().\n" );

    DWORD dwTeam[ eTeam_Count ] = { 0 };

    // Count active team players
    for( AppClass::PlayerItT playerIt = PlayerBegin(); playerIt != PlayerEnd(); ++playerIt )
    {
        ++dwTeam[ ( *playerIt )->GetTeam() ];
    }

    // Enable AI if needed... For each team
    for( eTeam team = eTeam_First; team < eTeam_Count; team = ( eTeam )( team + 1 ) )
    {
        // With no members
        if( dwTeam[ team ] == 0 )
            m_SessionPlayers.push_back( new Player( team ) );
    }
}

//--------------------------------------------------------------------------------------
// Per frame update
//--------------------------------------------------------------------------------------
VOID AppClass::UpdateGameplay()
{
    DWORD dwTickDelta = ( GetTickCount() - m_dwLastFrame );
    dwTickDelta = std::min <DWORD>( dwTickDelta, 67 ); // min framerate at 67ms / frame ==> 15 frames/second

    if( m_Scoreboard.UpdateCountdown( dwTickDelta ) == 0 )
    {
        if( IsHost() )
        {
            m_Ball.SetRandomVelocity();
            SendBallUpdate();
        }
    }
    else
    {
        // Update the game state
        FLOAT speed = ( ( FLOAT )dwTickDelta ) / 16.6667f;

        MovePaddles( speed );
        MoveBall( speed );
    }

    // Update the ball and paddles
    m_Ball.UpdateXui();

    // Process active players
    for( AppClass::PlayerItT playerIt = PlayerBegin(); playerIt != PlayerEnd(); ++playerIt )
    {
        ( *playerIt )->UpdateXui();
    }

    // Update the scores
    m_Scoreboard.UpdateXui();

    // Check for game over
    if( IsHost() && IsGameOver() )
    {
        LeaveQNetGame();
    }
}

//--------------------------------------------------------------------------------------
// Per frame update
//--------------------------------------------------------------------------------------
HRESULT AppClass::Update()
{
    // Check for notifications
    DWORD dwNotifyID;
    ULONG_PTR pNotifyParam;
    while( XNotifyGetNext( m_hNotify, 0, &dwNotifyID, &pNotifyParam ) )
    {
        switch( dwNotifyID )
        {
            case XN_FRIENDS_FRIEND_ADDED:
            case XN_FRIENDS_FRIEND_REMOVED:
                m_Leaderboards.InvalidateFriends();
                break;

            case XN_SYS_SIGNINCHANGED:
                m_Leaderboards.Reset();
                m_GamerPics.Reset();

                if( m_pActiveXuiScene )
                    m_pActiveXuiScene->SigninStateChanged();

                break;

            case XN_LIVE_CONTENT_INSTALLED:
                // License is checked each frame in MainMenuScene
                break;
        }
    }

    // Do work
    m_Leaderboards.DoWork();
    m_GamerPics.DoWork();

    if( m_pQNet != NULL )
    {
        m_bQNetWorking = TRUE;
        m_pQNet->DoWork();
        m_bQNetWorking = FALSE;

        if( m_bStopQNet )
            this->StopQNet();
    }

    HRESULT hResult = XuiTimersRun();
    if( FAILED( hResult ) )
    {
        OutputDebugString( "XuiTimersRun() failed.\n" );
        return hResult;
    }

    m_XuiModule.RunFrame();

    if( m_pQNet != 0 && m_pQNet->GetState() == QNET_STATE_GAME_PLAY && !m_Playing )
    {
        m_pActiveXuiScene->DisplayPlayScene();
        m_Playing = TRUE;
    }

    if( m_Playing )
        this->UpdateGameplay();

    m_dwLastFrame = GetTickCount();

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Per frame render
//--------------------------------------------------------------------------------------
HRESULT AppClass::Render()
{
    // Render Application
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_RGBA( 0, 0, 0, 0 ), 0.0f, 0 );

    m_XuiModule.Render( m_d3dpp, m_pd3dDevice );

    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Start QNet
//--------------------------------------------------------------------------------------
HRESULT AppClass::StartQNet( QNET_SESSIONTYPE SessionType )
{
    StopQNet();

    OutputDebugString( "AppClass::StartQNet().\n" );
    HRESULT hResult = QNetCreateUsingXAudio2( SessionType, this, NULL, m_pXAudio2, &m_pQNet );

    if( FAILED( hResult ) )
    {
        OutputDebugString( "Failed intializing QNet.\n" );
    }
    else
    {
        hResult = m_pQNet->CreateCameraManager(
            XCAMRESOLUTION_160x120,
            XCAMFRAMERATE_10,
            32,
            m_pd3dDevice,
            &m_pIQNetCameraManager );

        if( FAILED( hResult ) )
        {
            OutputDebugString( "Failed to initialize QNetCameraManager.\n" );
        }
    }

    return hResult;
}

//--------------------------------------------------------------------------------------
// Stop QNet
//--------------------------------------------------------------------------------------
VOID AppClass::StopQNet()
{
    if( m_bQNetWorking )
    {
        this->m_bStopQNet = TRUE;
    }
    else
    {
        OutputDebugString( "AppClass::StopQNet().\n" );

        if( m_pQNetSearch != NULL )
        {
            m_pQNetSearch->Destroy();
            m_pQNetSearch = NULL;
        }

        if( m_pIQNetCameraManager != NULL )
        {
            m_pIQNetCameraManager->Destroy();
            m_pIQNetCameraManager = NULL;
        }

        if( m_pQNet != NULL )
        {
            m_pQNet->Destroy();
            m_pQNet = NULL;
        }

        this->m_bStopQNet = FALSE;
    }
}

//--------------------------------------------------------------------------------------
// Render Player Camera Image
//--------------------------------------------------------------------------------------
HRESULT AppClass::RenderCamera( Player* pPlayer, D3DRECT const& rc )
{
    HRESULT hResult = E_FAIL;

    if( m_pIQNetCameraManager )
    {
        IQNetPlayer* pQNetPlayer = pPlayer->QNetPlayer();
        if( pQNetPlayer )
        {
            if( !pQNetPlayer->IsLocal() )
            {
                hResult = m_pIQNetCameraManager->SetRemoteSystemDefaultTexture(
                    pQNetPlayer, m_GamerPics.GetGamerPicTexture( pQNetPlayer->GetXuid() ) );
            }

            hResult = m_pIQNetCameraManager->SetRenderRectForSystem(
                pQNetPlayer, &rc );

            if( !FAILED( hResult ) )
            {
                hResult = m_pIQNetCameraManager->RenderSystem( pQNetPlayer );

                if( !FAILED( hResult ) )
                {
                    XuiRenderRestoreState( m_XuiModule.GetDC() );
                }
            }
        }
    }

    return hResult;
}

//--------------------------------------------------------------------------------------
// Configure Users
//--------------------------------------------------------------------------------------
VOID AppClass::ComputeUserMask()
{
    this->ResetUserMask();

    // Mark all controllers who are signed-in appropriately
    for( DWORD dwUserIndex = 0; dwUserIndex < XUSER_MAX_COUNT; ++dwUserIndex )
    {
        QNET_SESSIONTYPE eSessionType = this->GetQNetSessionType();

        if( ( XUserGetSigninState( dwUserIndex ) == eXUserSigninState_SignedInToLive ) ||
            ( ( eSessionType == QNET_SESSIONTYPE_LOCAL ) &&
              ( XUserGetSigninState( dwUserIndex ) == eXUserSigninState_SignedInLocally ) ) )
        {
            this->SetControllerHasUser( dwUserIndex, TRUE );
        }
    }
}

//--------------------------------------------------------------------------------------
// Search for games with QNet
//--------------------------------------------------------------------------------------
HRESULT AppClass::SearchForQNetGames()
{
    OutputDebugString( "AppClass::SearchForQNetGames().\n" );

    if( m_pQNet == NULL )
    {
        return E_FAIL;
    }

    if( m_pQNetSearch != NULL )
    {
        m_pQNetSearch->Destroy();
        m_pQNetSearch = NULL;
    }

    QNET_SESSIONTYPE eSessionType = GetQNetSessionType();

    XUSER_CONTEXT rgContexts[ 2 ] = {0};
    rgContexts[ 0 ].dwContextId = X_CONTEXT_GAME_TYPE;
    rgContexts[ 0 ].dwValue = ( eSessionType == QNET_SESSIONTYPE_XBOXLIVE_RANKED )
        ? X_CONTEXT_GAME_TYPE_RANKED : X_CONTEXT_GAME_TYPE_STANDARD;
    rgContexts[ 1 ].dwContextId = X_CONTEXT_GAME_MODE;
    rgContexts[ 1 ].dwValue = CONTEXT_GAME_MODE_ARCADESAMPLE;

    const DWORD dwMaxResults = 25;

    this->ComputeUserMask();
    HRESULT hSearchResult = m_pQNet->CreateGameSearch(
        m_dwCmdUserIndex,       // dwUserIndex
        m_dwUserMask,           // dwUserMask
        SESSION_MATCH_QUERY_ARCADESAMPLE, // dwMatchQueryIndex
        dwMaxResults,           // dwMaxResults
        0,                      // cProperties
        NULL,                   // pProperties
        2,                      // cContexts
        rgContexts,             // pContexts
        &m_pQNetSearch          // ppGameSearch
        );

    return hSearchResult;
}

//--------------------------------------------------------------------------------------
// Abort a QNet game search
//--------------------------------------------------------------------------------------
VOID AppClass::AbortQNetGameSearch()
{
    OutputDebugString( "AppClass::AbortQNetGameSearch().\n" );

    if( m_pQNetSearch != NULL )
    {
        m_pQNetSearch->Abort();
    }
}

//--------------------------------------------------------------------------------------
// Host a game with QNet
//--------------------------------------------------------------------------------------
HRESULT AppClass::HostQNetGame()
{
    OutputDebugString( "AppClass::HostQNetGame().\n" );

    if( m_pQNet == NULL )
    {
        return E_FAIL;
    }

    QNET_SESSIONTYPE eSessionType = GetQNetSessionType();

    XUSER_CONTEXT rgContexts[ 2 ] = {0};
    rgContexts[ 0 ].dwContextId = X_CONTEXT_GAME_TYPE;
    rgContexts[ 0 ].dwValue = ( eSessionType == QNET_SESSIONTYPE_XBOXLIVE_RANKED )
        ? X_CONTEXT_GAME_TYPE_RANKED : X_CONTEXT_GAME_TYPE_STANDARD;
    rgContexts[ 1 ].dwContextId = X_CONTEXT_GAME_MODE;
    rgContexts[ 1 ].dwValue = CONTEXT_GAME_MODE_ARCADESAMPLE;

    const DWORD dwPublicSlots = 4;
    const DWORD dwPrivateSlots = 0;

    this->ComputeUserMask();
    HRESULT hHostResult = m_pQNet->HostGame(
        m_dwCmdUserIndex,       // dwUserIndexHost
        m_dwUserMask,           // dwUserMask
        dwPublicSlots,          // dwPublicSlots
        dwPrivateSlots,         // dwPrivateSlots
        0,                      // cProperties
        NULL,                   // pProperties
        2,                      // cContexts        
        rgContexts              // pContexts
        );

    return hHostResult;
}

//--------------------------------------------------------------------------------------
// Join a game with QNet
//--------------------------------------------------------------------------------------
HRESULT AppClass::JoinQNetGame( const XSESSION_SEARCHRESULT* pSearchResult )
{
    OutputDebugString( "AppClass::JoinQNetGame().\n" );

    if( m_pQNet == NULL )
    {
        return E_FAIL;
    }

    this->ComputeUserMask();
    HRESULT hJoinResult = m_pQNet->JoinGameFromSearchResult(
        m_dwCmdUserIndex,   // dwUserIndex
        m_dwUserMask,       // dwUserMask
        pSearchResult       // pSearchResult
        );

    return hJoinResult;
}

//--------------------------------------------------------------------------------------
// Leave a game with QNet
//--------------------------------------------------------------------------------------
VOID AppClass::LeaveQNetGame()
{
    OutputDebugString( "AppClass::LeaveQNetGame().\n" );

    if( m_pQNet != NULL )
    {
        m_pQNet->LeaveGame( TRUE );
    }
}

//--------------------------------------------------------------------------------------
// Get the QNet session type
//--------------------------------------------------------------------------------------
QNET_SESSIONTYPE AppClass::GetQNetSessionType()
{
    QNET_SESSIONTYPE eSessionType = QNET_SESSIONTYPE_LOCAL;

    if( m_pQNet != NULL )
    {
        DWORD dwSize = sizeof( eSessionType );
        m_pQNet->GetOpt( QNET_OPTION_TYPE_SESSIONTYPE, &eSessionType, &dwSize );
    }

    return eSessionType;
}

//--------------------------------------------------------------------------------------
// Get the QNet state
//--------------------------------------------------------------------------------------
BOOL AppClass::GetQNetState( QNET_STATE* pState )
{
    ATG_Verify( pState );

    if( m_pQNet != NULL )
    {
        ATG_Verify( m_CurrentState == m_pQNet->GetState() );
        *pState = m_CurrentState;
    }

    return ( m_pQNet != NULL );
}

//--------------------------------------------------------------------------------------
// IQNet state changed notification
//--------------------------------------------------------------------------------------
VOID AppClass::NotifyStateChanged( QNET_STATE OldState, QNET_STATE NewState, HRESULT hrInfo )
{
    OutputDebugString( "AppClass::NotifyStateChanged().\n  " );

    switch( OldState )
    {
        case QNET_STATE_IDLE:
            OutputDebugString( "QNET_STATE_IDLE -> " ); break;

        case QNET_STATE_SESSION_HOSTING:
            OutputDebugString( "QNET_STATE_SESSION_HOSTING -> " ); break;

        case QNET_STATE_SESSION_JOINING:
            OutputDebugString( "QNET_STATE_SESSION_JOINING -> " ); break;

        case QNET_STATE_GAME_LOBBY:
            OutputDebugString( "QNET_STATE_GAME_LOBBY -> " ); break;

        case QNET_STATE_SESSION_REGISTERING:
            OutputDebugString( "QNET_STATE_SESSION_REGISTERING -> " ); break;

        case QNET_STATE_SESSION_STARTING:
            OutputDebugString( "QNET_STATE_SESSION_STARTING -> " ); break;

        case QNET_STATE_GAME_PLAY:
            OutputDebugString( "QNET_STATE_GAME_PLAY -> " );
            {
                m_Playing = FALSE;

                PlayerInfo playerInfo;
                m_EndGamePlayerInfos.clear();

                for( INT iUIIndex = 0; iUIIndex < ARCADESAMPLE_MAX_PLAYERS; ++iUIIndex )
                {
                    Player* pPlayer = PlayerForUIIndex( iUIIndex );

                    if( pPlayer )
                    {
                        playerInfo.Gamertag = pPlayer->GetGamertag();
                        playerInfo.Xuid = pPlayer->GetXuid();
                        playerInfo.Team = pPlayer->GetTeam();
                    }
                    else
                    {
                        playerInfo.Gamertag = L"---";
                        playerInfo.Xuid = 0;
                        playerInfo.Team = eTeam_Invalid;
                    }
                    m_EndGamePlayerInfos.push_back( playerInfo );
                }

                RemoveAllAIPlayers();
            }
            break;

        case QNET_STATE_SESSION_ENDING:
            OutputDebugString( "QNET_STATE_SESSION_ENDING -> " ); break;

        case QNET_STATE_SESSION_LEAVING:
            OutputDebugString( "QNET_STATE_SESSION_LEAVING -> " ); break;

        case QNET_STATE_SESSION_DELETING:
            OutputDebugString( "QNET_STATE_SESSION_DELETING -> " ); break;
    }

    switch( NewState )
    {
        case QNET_STATE_IDLE:
            OutputDebugString( "QNET_STATE_IDLE\n" ); break;

        case QNET_STATE_SESSION_HOSTING:
            OutputDebugString( "QNET_STATE_SESSION_HOSTING\n" ); break;

        case QNET_STATE_SESSION_JOINING:
            OutputDebugString( "QNET_STATE_SESSION_JOINING\n" ); break;

        case QNET_STATE_GAME_LOBBY:
            OutputDebugString( "QNET_STATE_GAME_LOBBY\n" ); break;

        case QNET_STATE_SESSION_REGISTERING:
            OutputDebugString( "QNET_STATE_SESSION_REGISTERING\n" ); break;

        case QNET_STATE_SESSION_STARTING:
            OutputDebugString( "QNET_STATE_SESSION_STARTING\n" ); break;

        case QNET_STATE_GAME_PLAY:
            OutputDebugString( "QNET_STATE_GAME_PLAY\n" );
            break;

        case QNET_STATE_SESSION_ENDING:
            OutputDebugString( "QNET_STATE_SESSION_ENDING\n" );
            break;

        case QNET_STATE_SESSION_LEAVING:
            OutputDebugString( "QNET_STATE_SESSION_LEAVING\n" ); break;

        case QNET_STATE_SESSION_DELETING:
            OutputDebugString( "QNET_STATE_SESSION_DELETING\n" ); break;
    }


    m_LastState = OldState;
    m_CurrentState = NewState;

    if( m_pActiveXuiScene )
    {
        m_pActiveXuiScene->QNetStateChanged( OldState, NewState, hrInfo );
    }
}

//--------------------------------------------------------------------------------------
// IQNet player joined notification
//--------------------------------------------------------------------------------------
VOID AppClass::NotifyPlayerJoined( IQNetPlayer* pQNetPlayer )
{
    OutputDebugString( "AppClass::NotifyPlayerJoined().\n" );

    DWORD dwHuman[ eTeam_Count ] = { 0 };
    DWORD dwAI[ eTeam_Count ] = { 0 };

    // Count active team players
    eTeam team;
    for( AppClass::PlayerItT playerIt = PlayerBegin(); playerIt != PlayerEnd(); ++playerIt )
    {
        team = ( *playerIt )->GetTeam();

        if( ( *playerIt )->IsAI() )
            ++dwAI[ team ];
        else
            ++dwHuman[ team ];
    }

    // Join side with less humans, left side by default
    if( dwHuman[ eTeam_Left ] == 0 )
        team = eTeam_Left;
    else if( dwHuman[ eTeam_Left ] <= dwHuman[ eTeam_Right ] )
        team = eTeam_Left;
    else
        team = eTeam_Right;

    Player* pPlayer = new Player( team, pQNetPlayer );
    m_SessionPlayers.push_back( pPlayer );

    pQNetPlayer->SetCustomDataValue( reinterpret_cast<ULONG_PTR>( pPlayer ) );

    if( m_pActiveXuiScene )
    {
        m_pActiveXuiScene->QNetPlayerJoined( *pPlayer );
    }
}

//--------------------------------------------------------------------------------------
// IQNet player leaving notification
//--------------------------------------------------------------------------------------
VOID AppClass::NotifyPlayerLeaving( IQNetPlayer* pQNetPlayer )
{
    OutputDebugString( "AppClass::NotifyPlayerLeaving().\n" );

    Player* pPlayer = PlayerFromQNetPlayer( pQNetPlayer );
    if( m_pActiveXuiScene )
    {
        m_pActiveXuiScene->QNetPlayerLeaving( *pPlayer );
    }

    PlayerItT playerIt = std::find( PlayerBegin(), PlayerEnd(), pPlayer );
    ATG_Verify( playerIt != PlayerEnd() );

    pQNetPlayer->SetCustomDataValue( NULL );
    delete pPlayer;

    m_SessionPlayers.erase( playerIt );
}

//--------------------------------------------------------------------------------------
// IQNet host changed notification
//--------------------------------------------------------------------------------------
VOID AppClass::NotifyNewHost( IQNetPlayer* pQNetPlayer )
{
    OutputDebugString( "AppClass::NotifyNewHost().\n" );

    Player* pPlayer = PlayerFromQNetPlayer( pQNetPlayer );
    ResetBall();

    if( m_pActiveXuiScene )
    {
        m_pActiveXuiScene->QNetNewHost( *pPlayer );
    }
}

//--------------------------------------------------------------------------------------
// IQNet data received notification
//--------------------------------------------------------------------------------------
VOID AppClass::NotifyDataReceived( IQNetPlayer* pQNetPlayerFrom, DWORD dwNumPlayersTo, IQNetPlayer** apQNetPlayersTo,
                                   const BYTE* pbData, DWORD dwDataSize )
{
    //OutputDebugString( "AppClass::NotifyDataReceived().\n" );

    switch( *pbData )
    {
        case BALL_UPDATE:
        {
            MSG_BALL_UPDATE* pBall = ( MSG_BALL_UPDATE* )pbData;
            m_Ball.SetDetails( *pBall );
            break;
        }

        case PADDLE_UPDATE:
        {
            MSG_PADDLE_UPDATE* pPaddle = ( MSG_PADDLE_UPDATE* )pbData;

            Player* pPlayer = PlayerFromXuid( pPaddle->playerXuid );
            ATG_Verify( pPlayer );

            pPlayer->SetPaddleDetails( *pPaddle );
            break;
        }

        case SCORE_UPDATE:
        {
            MSG_SCORE_UPDATE* pScore = ( MSG_SCORE_UPDATE* )pbData;
            SetTeamScore( pScore->team, pScore->score );
            ResetBall();
            break;
        }
    }
}

//--------------------------------------------------------------------------------------
// IQNet last chance write stats notification
//--------------------------------------------------------------------------------------
VOID AppClass::NotifyWriteStats( IQNetPlayer* pQNetPlayer )
{
    OutputDebugString( "AppClass::NotifyWriteStats().\n" );

    DWORD viewCount = 0;
    XSESSION_VIEW_PROPERTIES views[ 2 ];
    XUSER_PROPERTY skillProperties[ 2 ];
    XUSER_PROPERTY resultsProperties[ 2 ];

    Player* pPlayer = PlayerFromQNetPlayer( pQNetPlayer );
    eTeam team = pPlayer->GetTeam();

    bool dropped = !this->IsGameOver();
    bool ranked = ( this->GetQNetSessionType() == QNET_SESSIONTYPE_XBOXLIVE_RANKED );

    // If it is an arbitrated game then skill stats need to be written by
    // everyone. Otherwise, only the host needs to write them.
    if( ranked || m_pQNet->IsHost() )
    {
        skillProperties[ 0 ].dwPropertyId = X_PROPERTY_RELATIVE_SCORE;
        skillProperties[ 0 ].value.type = XUSER_DATA_TYPE_INT32;
        if( dropped )
        {
            skillProperties[ 0 ].value.nData = 0;
        }
        else
        {
            skillProperties[ 0 ].value.nData = this->IsWinningTeam( team ) ? 2 : 1;
        }

        skillProperties[ 1 ].dwPropertyId = X_PROPERTY_SESSION_TEAM;
        skillProperties[ 1 ].value.type = XUSER_DATA_TYPE_INT32;
        skillProperties[ 1 ].value.nData = team;

        views[ viewCount ].dwViewId =
            ranked ? STATS_VIEW_SKILL_RANKED_ARCADESAMPLE : STATS_VIEW_SKILL_STANDARD_ARCADESAMPLE;
        views[ viewCount ].dwNumProperties = 2;
        views[ viewCount ].pProperties = skillProperties;
        viewCount++;
    }

    // If it is an arbitrated game then game stats need to be written by
    // everyone. Otherwise, only write them for the local peer.
    if( ranked || pQNetPlayer->IsLocal() )
    {
        resultsProperties[ 0 ].dwPropertyId = PROPERTY_WINS;
        resultsProperties[ 0 ].value.type = XUSER_DATA_TYPE_INT64;

        resultsProperties[ 1 ].dwPropertyId = PROPERTY_LOSSES;
        resultsProperties[ 1 ].value.type = XUSER_DATA_TYPE_INT64;

        if( dropped )
        {
            resultsProperties[ 0 ].value.i64Data = 0;
            resultsProperties[ 1 ].value.i64Data = 1;
        }
        else
        {
            resultsProperties[ 0 ].value.i64Data = this->IsWinningTeam( team ) ? 1 : 0;
            resultsProperties[ 1 ].value.i64Data = this->IsWinningTeam( team ) ? 0 : 1;
        }

        views[ viewCount ].dwViewId = STATS_VIEW_OVERALL;
        views[ viewCount ].dwNumProperties = 2;
        views[ viewCount ].pProperties = resultsProperties;
        viewCount++;
    }

    // If there are any stats to be written, do so
    if( viewCount > 0 )
    {
        pPlayer->WriteStats( viewCount, views );
    }
}

//--------------------------------------------------------------------------------------
// IQNet readyness changed notification
//--------------------------------------------------------------------------------------
VOID AppClass::NotifyReadinessChanged( IQNetPlayer* pQNetPlayer, BOOL bReady )
{
    OutputDebugString( "AppClass::NotifyReadinessChanged().\n" );
    if( m_pActiveXuiScene )
    {
        m_pActiveXuiScene->QNetReadinessChanged( *PlayerFromQNetPlayer( pQNetPlayer ), bReady );
    }

    if( m_pQNet->IsHost() && m_pQNet->IsEveryoneReady() && HasProperTeamPlayers() )
    {
        m_pQNet->ResetReady();
        m_pQNet->StartGame();
    }
}

//--------------------------------------------------------------------------------------
// IQNet communication settings changed notification
//--------------------------------------------------------------------------------------
VOID AppClass::NotifyCommSettingsChanged( IQNetPlayer* pQNetPlayer )
{
    OutputDebugString( "AppClass::NotifyCommSettingsChanged().\n" );
    if( m_pActiveXuiScene )
    {
        m_pActiveXuiScene->QNetCommSettingsChanged( *PlayerFromQNetPlayer( pQNetPlayer ) );
    }
}

//--------------------------------------------------------------------------------------
// IQNet game search completed notification
//--------------------------------------------------------------------------------------
VOID AppClass::NotifyGameSearchComplete( IQNetGameSearch* pGameSearch, HRESULT hrComplete, DWORD dwNumResults )
{
    OutputDebugString( "AppClass::NotifyGameSearchComplete().\n" );
    if( m_pActiveXuiScene )
    {
        m_pActiveXuiScene->QNetGameSearchComplete( pGameSearch, hrComplete, dwNumResults );
    }
}

//--------------------------------------------------------------------------------------
// IQNet game invite notification
//--------------------------------------------------------------------------------------
VOID AppClass::NotifyGameInvite( DWORD dwUserIndex, const XINVITE_INFO* pInviteInfo )
{
    OutputDebugString( "AppClass::NotifyGameInvite().\n" );
    if( m_pActiveXuiScene )
    {
        m_pActiveXuiScene->QNetGameInvite( dwUserIndex, *pInviteInfo );
    }
}

//--------------------------------------------------------------------------------------
// IQNet context changed notification
//--------------------------------------------------------------------------------------
VOID AppClass::NotifyContextChanged( const XUSER_CONTEXT* pContext )
{
    OutputDebugString( "AppClass::NotifyContextChanged().\n" );
    if( m_pActiveXuiScene )
    {
        m_pActiveXuiScene->QNetContextChanged( *pContext );
    }
}

//--------------------------------------------------------------------------------------
// IQNet property changed notification
//--------------------------------------------------------------------------------------
VOID AppClass::NotifyPropertyChanged( const XUSER_PROPERTY* pProperty )
{
    OutputDebugString( "AppClass::NotifyPropertyChanged().\n" );
    if( m_pActiveXuiScene )
    {
        m_pActiveXuiScene->QNetPropertyChanged( *pProperty );
    }
}

//--------------------------------------------------------------------------------------
// Initialize Gameplay
//--------------------------------------------------------------------------------------
VOID AppClass::InitGameplay(
CXuiControl* figBall,
CXuiControl* figBorder,
CXuiControl* figPaddles,
CXuiControl* labScores,
CXuiControl* labCountdown )
{
    OutputDebugString( "AppClass::InitGameplay().\n" );

    m_Ball.BindControl( figBall );
    m_Arena.BindControl( figBorder );
    m_Scoreboard.BindXuiScoreControl( eTeam_Left, &labScores[ 0 ] );
    m_Scoreboard.BindXuiScoreControl( eTeam_Right, &labScores[ 1 ] );
    m_Scoreboard.BindXuiCountdownControl( labCountdown );

    // Configure game players
    ConfigureGamePlayers();

    eTeam team;
    DWORD dwTeam[ eTeam_Count ] = { 0 };
    DWORD dwUIIndices[ eTeam_Count ][ 2 ] =
    {
        { 0, 2 }, // eTeam_Left
        { 1, 3 }, // eTeam_Right
    };

    for( AppClass::PlayerItT playerIt = PlayerBegin(); playerIt != PlayerEnd(); ++playerIt )
    {
        Player* pPlayer = ( *playerIt );

        team = pPlayer->GetTeam();

        DWORD dwTeamMember = dwTeam[ team ];
        DWORD dwUIIndex = dwUIIndices[ team ][ dwTeamMember ];

        pPlayer->BindXuiPaddleControl( &figPaddles[ dwUIIndex ] );

        ++dwTeam[ team ];
        pPlayer->ResetPaddle( m_Arena.GetRect() );
    }

    // Reset the scores
    ClearScore();

    // Reset the ball
    ResetBall();

    m_Pause = FALSE;
}

//--------------------------------------------------------------------------------------
// Reset the ball to the center of the play field
//--------------------------------------------------------------------------------------
VOID AppClass::ResetBall()
{
    OutputDebugString( "AppClass::ResetBall().\n" );

    // Reset the countdown
    m_Scoreboard.SetCountdown( 3000 );

    // Reset the ball
    m_Ball.Reset( m_Arena.GetRect() );
}

//--------------------------------------------------------------------------------------
// Send a ball update message
//--------------------------------------------------------------------------------------
VOID AppClass::SendBallUpdate()
{
    MSG_BALL_UPDATE ball;

    m_Ball.GetDetails( &ball );

    m_pQNet->GetHostPlayer()->SendData(
        NULL, ( BYTE* )&ball, sizeof( ball ),
        QNET_SENDDATA_RELIABLE | QNET_SENDDATA_SEQUENTIAL
        );
}

//--------------------------------------------------------------------------------------
// Move the ball and check for collisions
//--------------------------------------------------------------------------------------
VOID AppClass::MoveBall( FLOAT speed )
{
    // Move the ball
    if( !m_Pause || GetQNetSessionType() != QNET_SESSIONTYPE_LOCAL )
        m_Ball.Move( speed );

    BOOL bCollide = FALSE;
    for( AppClass::PlayerItT playerIt = PlayerBegin(); !bCollide && playerIt != PlayerEnd(); ++playerIt )
    {
        bCollide = ( *playerIt )->CollideAndUpdate( m_Ball );
    }

    BOOL bSendUpdate = bCollide;

    // Check ball collision with edges
    eArenaCollide collide = m_Arena.CollideAndUpdate( m_Ball );

    switch( collide )
    {
        case eArenaCollide_Wall:
            bSendUpdate = TRUE;
            break;

        case eArenaCollide_GoalRight:
            if( IsHost() )
            {
                MSG_SCORE_UPDATE score;
                score.team = eTeam_Left;
                score.score = GetTeamScore( eTeam_Left ) + 1;

                m_pQNet->GetHostPlayer()->SendData(
                    NULL, ( BYTE* )&score, sizeof( score ),
                    QNET_SENDDATA_RELIABLE | QNET_SENDDATA_SEQUENTIAL
                    );
            }
            break;

        case eArenaCollide_GoalLeft:
            if( IsHost() )
            {
                MSG_SCORE_UPDATE score;
                score.team = eTeam_Right;
                score.score = GetTeamScore( eTeam_Right ) + 1;

                m_pQNet->GetHostPlayer()->SendData(
                    NULL, ( BYTE* )&score, sizeof( score ),
                    QNET_SENDDATA_RELIABLE | QNET_SENDDATA_SEQUENTIAL
                    );
            }
            break;
    }

    if( bSendUpdate && IsHost() )
    {
        SendBallUpdate();
    }
}

//--------------------------------------------------------------------------------------
// Move the paddles
//--------------------------------------------------------------------------------------
VOID AppClass::MovePaddles( FLOAT speed )
{
    // Move the local players
    if( !m_Pause )
    {
        for( AppClass::PlayerItT playerIt = PlayerBegin(); playerIt != PlayerEnd(); ++playerIt )
        {
            Player* pPlayer = ( *playerIt );
            if( pPlayer->IsLocal() && !pPlayer->IsAI() )
            {
                pPlayer->ProcessInput( speed, m_Arena.GetRect() );
            }
        }
    }

    // Move the AI players
    if( IsHost() )
    {
        for( AppClass::PlayerItT playerIt = PlayerBegin(); playerIt != PlayerEnd(); ++playerIt )
        {
            Player* pPlayer = ( *playerIt );
            if( pPlayer->IsAI() )
            {
                pPlayer->AIMovePaddle( speed, m_Arena.GetRect(), m_Ball.GetRect() );

                MSG_PADDLE_UPDATE paddle;
                pPlayer->GetPaddleDetails( &paddle );

                m_pQNet->GetHostPlayer()->SendData(
                    NULL, ( BYTE* )&paddle, sizeof( paddle ),
                    0 );
            }
        }
    }
}

} // namespace ArcadeSample

