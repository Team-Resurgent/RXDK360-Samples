//--------------------------------------------------------------------------------------
// GameOptionsScene.cpp
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "GameOptionsScene.h"
#include "App.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Initialize the scene
//--------------------------------------------------------------------------------------
HRESULT CGameOptionsScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    QNetSessionScene::OnInit( pInitData, bHandled );

    GetChildById( L"btnBack", &m_btnBack );
    GetChildById( L"btnCreateGame", &m_btnCreateGame );
    GetChildById( L"lstMaxPoints", &m_lstMaxPoints );
    GetChildById( L"lstPrivate", &m_lstPrivate );

    m_lstMaxPoints.SetCurSelVisible( g_App.GetEndGameScore() - 1 );
    m_lstPrivate.SetShow( g_App.GetQNetSessionType() == QNET_SESSIONTYPE_XBOXLIVE_STANDARD );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Handle a button press
//--------------------------------------------------------------------------------------
HRESULT CGameOptionsScene::OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnNotifyPressEx( hObjSource, pNotifyPress, bHandled );
    if( !bHandled )
    {
        // Which button did they press?
        if( hObjSource == m_btnBack )
        {
            hResult = NavigateBack();
        }
        else if( hObjSource == m_btnCreateGame )
        {
            g_App.SetEndGameScore( m_lstMaxPoints.GetCurSel() + 1 );

            NavigateForward( L"GameConnectScene.xur" );
            HRESULT hHostResult = g_App.HostQNetGame();
            if( FAILED( hHostResult ) )
            {
                ShowErrorMessageBox( L"Failed to host the game.", hHostResult );
                NavigateBack();
            }
        }
        bHandled = TRUE;
    }
    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a return navigation
//--------------------------------------------------------------------------------------
HRESULT CGameOptionsScene::OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnNavReturn( hSceneFrom, bHandled );
    if( !bHandled )
    {
        bHandled = TRUE;
    }
    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a message box completion
//--------------------------------------------------------------------------------------
HRESULT CGameOptionsScene::OnMsgReturn( XUIMessageMessageBoxReturn* pXUIMessageMessageBoxReturn, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnMsgReturn( pXUIMessageMessageBoxReturn, bHandled );

    g_App.LeaveQNetGame();

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a QNet state notification
//--------------------------------------------------------------------------------------
VOID CGameOptionsScene::QNetStateChanged( QNET_STATE OldState, QNET_STATE NewState, HRESULT hrInfo )
{
    QNetSessionScene::QNetStateChanged( OldState, NewState, hrInfo );

    if( NewState == QNET_STATE_IDLE )
    {
        NavigateBack();
    }
}

} // namespace ArcadeSample
