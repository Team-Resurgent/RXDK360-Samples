//--------------------------------------------------------------------------------------
// GameConnectScene.cpp
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "GameConnectScene.h"
#include "App.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Initialize the scene
//--------------------------------------------------------------------------------------
HRESULT CGameConnectScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnInit( pInitData, bHandled );

    hResult = GetChildById( L"btnBack", &m_btnBack );

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a button press
//--------------------------------------------------------------------------------------
HRESULT CGameConnectScene::OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnNotifyPressEx( hObjSource, pNotifyPress, bHandled );
    if( !bHandled )
    {
        // Which button did they press?
        if( hObjSource == m_btnBack )
        {
            g_App.LeaveQNetGame();
        }
        bHandled = TRUE;
    }
    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a return navigation
//--------------------------------------------------------------------------------------
HRESULT CGameConnectScene::OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled )
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
HRESULT CGameConnectScene::OnMsgReturn( XUIMessageMessageBoxReturn* pXUIMessageMessageBoxReturn, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnMsgReturn( pXUIMessageMessageBoxReturn, bHandled );

    g_App.LeaveQNetGame();

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a QNet state change notification
//--------------------------------------------------------------------------------------
VOID CGameConnectScene::QNetStateChanged( QNET_STATE OldState, QNET_STATE NewState, HRESULT hrInfo )
{
    QNetSessionScene::QNetStateChanged( OldState, NewState, hrInfo );

    if( SUCCEEDED( hrInfo ) )
    {
        if( NewState == QNET_STATE_IDLE )
        {
            NavigateBack();
        }
        else if( ( NewState == QNET_STATE_GAME_LOBBY ) && !m_btnBack.IsPressed() )
        {
            if( g_App.GetQNetSessionType() == QNET_SESSIONTYPE_LOCAL )
            {
                NavigateForward( L"GameStartScene.xur" );
                HRESULT hStartResult = g_App.StartQNetGame();
                if( FAILED( hStartResult ) )
                {
                    ShowErrorMessageBox( L"Failed to start the game.", hStartResult );
                    NavigateBack();
                }
            }
            else
            {
                NavigateForward( L"GameLobbyScene.xur" );
            }
        }
    }
    else
    {
        if( g_App.GetQNetSessionType() == QNET_SESSIONTYPE_LOCAL )
        {
            ShowErrorMessageBox( L"Failed to start the game.", hrInfo );
        }
        else if( g_App.IsHost() )
        {
            ShowErrorMessageBox( L"Failed to host the game.", hrInfo );
        }
        else
        {
            /*if( hr == XANET_E_JOIN_GAME_NOT_EXIST )
               {
               ShowErrorMessageBox( L"Failed to join the game because it could not be found.", hr );
               }
               else */if( hrInfo == QNET_E_SESSION_FULL )
               {
                  ShowErrorMessageBox( L"Failed to join the game because it is full.", hrInfo );
               }
                /*else if( hr == XANET_E_JOIN_GAME_STARTED )
               {
                   ShowErrorMessageBox( L"Failed to join the game because it has already started.", hr );
               }
                   else if( hr == XANET_E_JOIN_GAME_TIMEOUT )
               {
                   ShowErrorMessageBox( L"Failed to join the game because the connection timed out.", hr );
               }*/
               else
               {
                   ShowErrorMessageBox( L"Failed to join the game.", hrInfo );
               }
        }
    }
}

} // namespace ArcadeSample
