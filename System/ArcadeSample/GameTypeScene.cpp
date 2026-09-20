//--------------------------------------------------------------------------------------
// GameTypeScene.cpp
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "GameTypeScene.h"
#include "App.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Initialize the scene
//--------------------------------------------------------------------------------------
HRESULT CGameTypeScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    HRESULT hResult = AppXuiScene::OnInit( pInitData, bHandled );

    hResult = GetChildById( L"btnBack", &m_btnBack );
    hResult = GetChildById( L"btnQuick", &m_btnQuick );
    hResult = GetChildById( L"btnCustom", &m_btnCustom );
    hResult = GetChildById( L"btnCreate", &m_btnCreate );
    hResult = GetChildById( L"lstMatchType", &m_lstMatchType );

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a button press
//--------------------------------------------------------------------------------------
HRESULT CGameTypeScene::OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled )
{
    HRESULT hResult = AppXuiScene::OnNotifyPressEx( hObjSource, pNotifyPress, bHandled );
    if( !bHandled )
    {
        QNET_SESSIONTYPE eSessionType = ( m_lstMatchType.GetCurSel() == 1 ) ?
            QNET_SESSIONTYPE_XBOXLIVE_RANKED : QNET_SESSIONTYPE_XBOXLIVE_STANDARD;

        // Which button did they press?
        if( hObjSource == m_btnBack )
        {
            hResult = NavigateBack();
            if( FAILED( hResult ) )
            {
                OutputDebugString( "NavigateBack() failed.\n" );
            }
        }
        else
        {
            hResult = g_App.StartQNet( eSessionType );
            if( FAILED( hResult ) )
            {
                OutputDebugString( "StartQNet() failed.\n" );
            }
            else if( hObjSource == m_btnQuick )
            {
                hResult = NavigateForward( L"QuickMatchScene.xur" );
            }
            else if( hObjSource == m_btnCustom )
            {
                hResult = NavigateForward( L"GameListScene.xur" );
            }
            else if( hObjSource == m_btnCreate )
            {
                hResult = NavigateForward( L"GameOptionsScene.xur" );
            }
        }
        bHandled = TRUE;
    }
    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a return navigation
//--------------------------------------------------------------------------------------
HRESULT CGameTypeScene::OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled )
{
    HRESULT hResult = AppXuiScene::OnNavReturn( hSceneFrom, bHandled );
    if( !bHandled )
    {
        bHandled = TRUE;
    }
    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a message box completion
//--------------------------------------------------------------------------------------
HRESULT CGameTypeScene::OnMsgReturn( XUIMessageMessageBoxReturn* pXUIMessageMessageBoxReturn, BOOL& bHandled )
{
    HRESULT hResult = AppXuiScene::OnMsgReturn( pXUIMessageMessageBoxReturn, bHandled );

    hResult = NavigateBack();

    return hResult;
}

} // namespace ArcadeSample
