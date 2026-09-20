//--------------------------------------------------------------------------------------
// GameStartScene.cpp
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "GameStartScene.h"
#include "App.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Handle a return navigation
//--------------------------------------------------------------------------------------
HRESULT CGameStartScene::OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled )
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
HRESULT CGameStartScene::OnMsgReturn( XUIMessageMessageBoxReturn* pXUIMessageMessageBoxReturn, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnMsgReturn( pXUIMessageMessageBoxReturn, bHandled );

    g_App.LeaveQNetGame();

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a QNet state change notification
//--------------------------------------------------------------------------------------
VOID CGameStartScene::QNetStateChanged( QNET_STATE OldState, QNET_STATE NewState, HRESULT hrInfo )
{
    QNetSessionScene::QNetStateChanged( OldState, NewState, hrInfo );

    HRESULT hResult;
    if( SUCCEEDED( hrInfo ) )
    {
        if( NewState == QNET_STATE_IDLE )
        {
            hResult = NavigateBack();
        }

    }
    else
    {
        if( NewState == QNET_STATE_SESSION_REGISTERING )
        {
            ShowErrorMessageBox( L"Failed to register for arbitration.", hrInfo );
        }
        else
        {
            ShowErrorMessageBox( L"Failed to start the game.", hrInfo );
        }
    }
}


//--------------------------------------------------------------------------------------
// Advance to the Play scene.
//--------------------------------------------------------------------------------------
VOID CGameStartScene::DisplayPlayScene()
{ 
	NavigateForward( L"GamePlayScene.xur" ); 
}

} // namespace ArcadeSample
