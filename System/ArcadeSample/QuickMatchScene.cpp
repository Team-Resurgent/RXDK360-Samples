//--------------------------------------------------------------------------------------
// QuickMatchScene.cpp
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "QuickMatchScene.h"
#include "App.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Initialize the scene
//--------------------------------------------------------------------------------------
HRESULT CQuickMatchScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnInit( pInitData, bHandled );

    hResult = GetChildById( L"btnBack", &m_btnBack );

    m_dwGamesFoundTick = 0;
    ZeroMemory( m_rgGamesTried, sizeof( m_rgGamesTried ) );
    m_cGamesTried = 0;
    m_State = State_WaitingForGamesFound;

    HRESULT hSearchResult = g_App.SearchForQNetGames();
    if( FAILED( hSearchResult ) )
    {
        ShowErrorMessageBox( L"Failed to find a match.", hSearchResult );
    }

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a button press
//--------------------------------------------------------------------------------------
HRESULT CQuickMatchScene::OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnNotifyPressEx( hObjSource, pNotifyPress, bHandled );
    if( !bHandled )
    {
        // Which button did they press?
        if( hObjSource == m_btnBack )
        {
            m_State = State_Cancel;
            g_App.AbortQNetGameSearch();
            g_App.LeaveQNetGame();
            NavigateBack();
        }
        bHandled = TRUE;
    }
    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a return navigation
//--------------------------------------------------------------------------------------
HRESULT CQuickMatchScene::OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnNavReturn( hSceneFrom, bHandled );
    if( !bHandled )
    {
        hResult = NavigateBack();
        bHandled = TRUE;
    }
    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a message box completion
//--------------------------------------------------------------------------------------
HRESULT CQuickMatchScene::OnMsgReturn( XUIMessageMessageBoxReturn* pXUIMessageMessageBoxReturn, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnMsgReturn( pXUIMessageMessageBoxReturn, bHandled );

    g_App.LeaveQNetGame();

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle per frame processing
//--------------------------------------------------------------------------------------
HRESULT CQuickMatchScene::OnRender( XUIMessageRender* pRenderData, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnRender( pRenderData, bHandled );

    if( m_State == State_WaitingForQoS )
    {
        // Check how many QoS results are valid
        DWORD dwNumResults = g_App.GetQNetGameSearch()->GetNumResults();
        DWORD cValidQosResults = 0;
        DWORD cValidQosResultsWithPing = 0;
        for( DWORD dwResult = 0; dwResult < dwNumResults; dwResult++ )
        {
            const XNQOSINFO* pQosResult =
                g_App.GetQNetGameSearch()->GetQosInfoAtIndex( dwResult );
            if( ( pQosResult != NULL ) && ( pQosResult->bFlags & XNET_XNQOSINFO_COMPLETE ) )
            {
                if( pQosResult->wRttMedInMsecs != 0xFFFF )
                {
                    cValidQosResultsWithPing++;
                }
                cValidQosResults++;
            }
        }

        // Did we get enough QoS results?
        bool minQosResults = ( cValidQosResultsWithPing >= c_QosMinResults );
        bool allQosResults = ( cValidQosResults == dwNumResults );

        // Have we waited long enough?
        bool waitedForMinDelay = ( m_dwGamesFoundTick + c_QoSMinDelay < GetTickCount() );
        bool waitedForMaxDelay = ( m_dwGamesFoundTick + c_QoSMaxDelay < GetTickCount() );

        // Try to join game if appropriate
        if( ( waitedForMinDelay && minQosResults ) || waitedForMaxDelay || allQosResults )
        {
            TryToJoinGame();
        }
    }

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a QNet state change notification
//--------------------------------------------------------------------------------------
VOID CQuickMatchScene::QNetStateChanged( QNET_STATE OldState, QNET_STATE NewState, HRESULT hrInfo )
{
    QNetSessionScene::QNetStateChanged( OldState, NewState, hrInfo );

    HRESULT hResult;
    if( SUCCEEDED( hrInfo ) )
    {
        if( NewState == QNET_STATE_IDLE )
        {
            if( m_State == State_JoiningGame )
            {
                if( m_cGamesTried < c_MaxGamesToTry )
                {
                    // Try to join another game
                    TryToJoinGame();
                }
                else
                {
                    // Host the game
                    TryToHostGame();
                }
            }
            else
            {
                hResult = NavigateBack();
            }
        }
        else if( ( NewState == QNET_STATE_GAME_LOBBY ) && !m_btnBack.IsPressed() )
        {
            // We made it into a game, goto the lobby
            hResult = NavigateForward( L"GameLobbyScene.xur" );
        }
    }
    else
    {
        if( m_State == State_JoiningGame )
        {
            // Leave the failed game so we can try something else
            g_App.LeaveQNetGame();
        }
        else if( m_State == State_HostingGame )
        {
            ShowErrorMessageBox( L"Failed to find a match.", hrInfo );
        }
    }
}

//--------------------------------------------------------------------------------------
// Handle an XANet games found notification
//--------------------------------------------------------------------------------------
VOID CQuickMatchScene::QNetGameSearchComplete( IQNetGameSearch* pGameSearch, HRESULT hrComplete, DWORD dwNumResults )
{
    QNetSessionScene::QNetGameSearchComplete( pGameSearch, hrComplete, dwNumResults );

    if( SUCCEEDED( hrComplete ) && ( dwNumResults > 0 ) )
    {
        // Wait for QoS results
        m_dwGamesFoundTick = GetTickCount();
        m_State = State_WaitingForQoS;
    }
    else
    {
        // Host the game
        TryToHostGame();
    }
}

//--------------------------------------------------------------------------------------
// Have we tried to join this game yet?
//--------------------------------------------------------------------------------------
bool CQuickMatchScene::HasTriedGame( DWORD dwResult )
{
    for( DWORD i = 0; i < m_cGamesTried; i++ )
    {
        if( m_rgGamesTried[ i ] == dwResult )
        {
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------
// Try to join the next best game
//--------------------------------------------------------------------------------------
VOID CQuickMatchScene::TryToJoinGame()
{
    m_State = State_JoiningGame;

    // Choose the best search result
    DWORD dwNumResults = g_App.GetQNetGameSearch()->GetNumResults();
    DWORD dwBestResult = 0;

    const XSESSION_SEARCHRESULT* pBestSearchResult = NULL;
    const XNQOSINFO* pBestQosResult = NULL;

    for( DWORD dwResult = 0; dwResult < dwNumResults; dwResult++ )
    {
        const XSESSION_SEARCHRESULT* pSearchResult =
            g_App.GetQNetGameSearch()->GetSearchResultAtIndex( dwResult );
        const XNQOSINFO* pQosResult =
            g_App.GetQNetGameSearch()->GetQosInfoAtIndex( dwResult );

        if( ( pSearchResult == NULL ) || ( pQosResult == NULL ) || HasTriedGame( dwResult ) ) continue;

        if( ( pQosResult->bFlags & XNET_XNQOSINFO_COMPLETE ) && ( pQosResult->wRttMedInMsecs != 0xFFFF ) &&
            ( ( pBestQosResult == NULL ) || ( pQosResult->wRttMedInMsecs < pBestQosResult->wRttMedInMsecs ) ) )
        {
            // This is a better choice than the previous best because it has
            // a lower ping time.
            dwBestResult = dwResult;
            pBestSearchResult = pSearchResult;
            pBestQosResult = pQosResult;
        }
    }

    if( pBestSearchResult != NULL )
    {
        // We will try the best result
        m_rgGamesTried[ m_cGamesTried++] = dwBestResult;

        // Join the game
        HRESULT hJoinResult = g_App.JoinQNetGame( pBestSearchResult );
        if( FAILED( hJoinResult ) )
        {
            g_App.LeaveQNetGame();
        }
    }
    else
    {
        TryToHostGame();
    }
}

//--------------------------------------------------------------------------------------
// Try to host the game
//--------------------------------------------------------------------------------------
VOID CQuickMatchScene::TryToHostGame()
{
    m_State = State_HostingGame;

    // Host the game
    HRESULT hHostResult = g_App.HostQNetGame();
    if( FAILED( hHostResult ) )
    {
        ShowErrorMessageBox( L"Failed to find a match.", hHostResult );
    }
}

} // namespace ArcadeSample
