//--------------------------------------------------------------------------------------
// GamePlayScene.cpp
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "GamePlayScene.h"
#include "App.h"
#include "Utils.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Get the number of list items
//--------------------------------------------------------------------------------------
HRESULT CGamePlayList::OnGetItemCountAll( XUIMessageGetItemCount* pGetItemCountData, BOOL& bHandled )
{
    bHandled = TRUE;

    pGetItemCountData->cItems = 4;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Get the list item text
//--------------------------------------------------------------------------------------
HRESULT CGamePlayList::OnGetSourceText( XUIMessageGetSourceText* pGetSourceText, BOOL& bHandled )
{
    bHandled = TRUE;

    if( eText_Gamertag == pGetSourceText->iData )
    {
        Player* pPlayer = g_App.PlayerForUIIndex( pGetSourceText->iItem );

        if( pPlayer )
        {
            pGetSourceText->szText = pPlayer->GetGamertag();
        }
        else
        {
            pGetSourceText->szText = L"---";
        }
    }
    else
    {
        pGetSourceText->szText = L"---";
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Get the list item image
//--------------------------------------------------------------------------------------
HRESULT CGamePlayList::OnGetSourceImage( XUIMessageGetSourceImage* pGetSourceImage, BOOL& bHandled )
{
    bHandled = TRUE;

    Player* pPlayer = g_App.PlayerForUIIndex( pGetSourceImage->iItem );

    if( pPlayer )
    {
        if( eImg_GamerPic == pGetSourceImage->iData )
        {
            XUID xuidPlayer = pPlayer->GetXuid();
            pGetSourceImage->hBrush = g_App.GetGamerPicBrush( xuidPlayer );
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Initialize the scene
//--------------------------------------------------------------------------------------
HRESULT CGamePlayScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnInit( pInitData, bHandled );

    hResult = GetChildById( L"figBall", &m_figBall );
    hResult = GetChildById( L"figBorder", &m_figBorder );
    hResult = GetChildById( L"figPaddle0", &m_figPaddles[ 0 ] );
    hResult = GetChildById( L"figPaddle1", &m_figPaddles[ 1 ] );
    hResult = GetChildById( L"figPaddle2", &m_figPaddles[ 2 ] );
    hResult = GetChildById( L"figPaddle3", &m_figPaddles[ 3 ] );
    hResult = GetChildById( L"labScore0", &m_labScores[ 0 ] );
    hResult = GetChildById( L"labScore1", &m_labScores[ 1 ] );
    hResult = GetChildById( L"labCountdown", &m_labCountdown );
    hResult = GetChildById( L"btnPause", &m_btnPause );
    hResult = GetChildById( L"grpPause", &m_grpPause );
    hResult = m_grpPause.GetChildById( L"btnResume", &m_btnResume );
    hResult = m_grpPause.GetChildById( L"btnExit", &m_btnExit );

    m_figPaddles[ 0 ].SetShow( FALSE );
    m_figPaddles[ 1 ].SetShow( FALSE );
    m_figPaddles[ 2 ].SetShow( FALSE );
    m_figPaddles[ 3 ].SetShow( FALSE );

    g_App.InitGameplay( &m_figBall, &m_figBorder, m_figPaddles, m_labScores, &m_labCountdown );

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a button press
//--------------------------------------------------------------------------------------
HRESULT CGamePlayScene::OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnNotifyPressEx( hObjSource, pNotifyPress, bHandled );
    if( !bHandled )
    {
        // Which button did they press?
        if( hObjSource == m_btnPause )
        {
            hResult = m_grpPause.SetShow( TRUE );
            hResult = m_btnResume.SetFocus();
            g_App.Pause( TRUE );
        }
        else if( hObjSource == m_btnResume )
        {
            hResult = m_grpPause.SetShow( FALSE );
            hResult = m_btnPause.SetFocus();
            g_App.Pause( FALSE );
        }
        else if( hObjSource == m_btnExit )
        {
            g_App.LeaveQNetGame();
        }
        bHandled = TRUE;
    }
    return hResult;
}


//--------------------------------------------------------------------------------------
// Render the scene
//--------------------------------------------------------------------------------------
HRESULT CGamePlayScene::OnRender( XUIMessageRender* pRenderData, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnRender( pRenderData, bHandled );

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a return navigation
//--------------------------------------------------------------------------------------
HRESULT CGamePlayScene::OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled )
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
// Handle a QNet state notification
//--------------------------------------------------------------------------------------
VOID CGamePlayScene::QNetStateChanged( QNET_STATE OldState, QNET_STATE NewState, HRESULT hrInfo )
{
    QNetSessionScene::QNetStateChanged( OldState, NewState, hrInfo );

    if( NewState != QNET_STATE_GAME_PLAY )
    {
        // The game play has ended
        NavigateForward( L"GameEndScene.xur" );
    }
}

} // namespace ArcadeSample
