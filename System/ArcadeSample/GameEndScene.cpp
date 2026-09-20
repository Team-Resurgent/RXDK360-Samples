//--------------------------------------------------------------------------------------
// GameEndScene.cpp
//--------------------------------------------------------------------------------------
#include "stdafx.h"
#include "GameEndScene.h"
#include "Player.h"
#include "App.h"
#include "Utils.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Get the number of list items
//--------------------------------------------------------------------------------------
HRESULT CGameEndList::OnGetItemCountAll( XUIMessageGetItemCount* pGetItemCountData, BOOL& bHandled )
{
    bHandled = TRUE;

    pGetItemCountData->cItems = ARCADESAMPLE_MAX_PLAYERS;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Get the list item text
//--------------------------------------------------------------------------------------
HRESULT CGameEndList::OnGetSourceText( XUIMessageGetSourceText* pGetSourceText, BOOL& bHandled )
{
    bHandled = TRUE;

    PlayerInfo const& endGamePlayer = g_App.GetEndGamePlayerInfoForUIIndex( pGetSourceText->iItem );

    if( eText_GamerTag == pGetSourceText->iData )
    {
        pGetSourceText->szText = endGamePlayer.Gamertag.c_str();
    }
    else if( eText_Score == pGetSourceText->iData )
    {
        if( endGamePlayer.Team != eTeam_Invalid )
        {
            const int SCORE_SIZE = 16;
            static WCHAR wszScore[ SCORE_SIZE ];
            eTeam team = endGamePlayer.Team;
            swprintf_s( wszScore, L"%d", g_App.GetTeamScore( team ) );
            pGetSourceText->szText = wszScore;
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Get the list item image
//--------------------------------------------------------------------------------------
HRESULT CGameEndList::OnGetSourceImage( XUIMessageGetSourceImage* pGetSourceImage, BOOL& bHandled )
{
    bHandled = TRUE;

    PlayerInfo const& endGamePlayer = g_App.GetEndGamePlayerInfoForUIIndex( pGetSourceImage->iItem );

    if( ( endGamePlayer.Team != eTeam_Invalid ) && ( eImg_GamerPic == pGetSourceImage->iData ) )
    {
        pGetSourceImage->hBrush = g_App.GetGamerPicBrush( endGamePlayer.Xuid );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Initialize the scene
//--------------------------------------------------------------------------------------
HRESULT CGameEndScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    HRESULT hResult = AppXuiScene::OnInit( pInitData, bHandled );

    hResult = GetChildById( L"labGamertag", &m_labGamertag );
    hResult = GetChildById( L"labScore", &m_labScore );
    hResult = GetChildById( L"btnDone", &m_btnDone );
    hResult = GetChildById( L"btnGamercard", &m_btnGamercard );
    hResult = GetChildById( L"btnPlayerReview", &m_btnPlayerReview );
    hResult = GetChildById( L"ctlStatus", &m_ctlStatus );
    hResult = GetChildById( L"labStatus", &m_labStatus );
    hResult = GetChildById( L"ctlWait", &m_ctlWait );

    HXUIOBJ hlstFinalScores = NULL;
    if( SUCCEEDED( GetChildById( L"lstFinalScores", &hlstFinalScores ) ) )
    {
        hResult = XuiObjectFromHandle( hlstFinalScores, reinterpret_cast<void**>( &m_plstFinalScores ) );
    }

    ShowFinalScores( false );

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a button press
//--------------------------------------------------------------------------------------
HRESULT CGameEndScene::OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled )
{
    HRESULT hResult = AppXuiScene::OnNotifyPressEx( hObjSource, pNotifyPress, bHandled );
    if( !bHandled )
    {
        DWORD dwHighlightedPlayer = ( DWORD )m_plstFinalScores->GetCurSel();

        if( ( hObjSource == m_btnDone ) || ( hObjSource == *m_plstFinalScores ) )
        {
            if( g_App.GetQNetSessionType() != QNET_SESSIONTYPE_XBOXLIVE_STANDARD )
            {
                // Go back to the main menu
                hResult = NavigateBackToFirst();
            }
            else
            {
                // Go back to the lobby
                hResult = NavigateBack();
            }
        }
        else
        {
            PlayerInfo const& endGamePlayer = g_App.GetEndGamePlayerInfoForUIIndex( dwHighlightedPlayer );

            if( hObjSource == m_btnGamercard )
            {
                if( endGamePlayer.Xuid != 0 )
                {
                    // Show the gamer card
                    g_App.ShowGamerCardForXuid( endGamePlayer.Xuid );
                }
            }
            else if( hObjSource == m_btnPlayerReview )
            {
                if( ( g_App.GetQNetSessionType() != QNET_SESSIONTYPE_LOCAL ) && ( endGamePlayer.Xuid != 0 ) )
                {
                    // Show the player review
                    g_App.ShowPlayerReviewForXuid( endGamePlayer.Xuid );
                }
            }
        }
        bHandled = TRUE;
    }
    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a return navigation
//--------------------------------------------------------------------------------------
HRESULT CGameEndScene::OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled )
{
    HRESULT hResult = AppXuiScene::OnNavReturn( hSceneFrom, bHandled );
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
HRESULT CGameEndScene::OnMsgReturn( XUIMessageMessageBoxReturn* pXUIMessageMessageBoxReturn, BOOL& bHandled )
{
    HRESULT hResult = AppXuiScene::OnMsgReturn( pXUIMessageMessageBoxReturn, bHandled );

    hResult = m_plstFinalScores->SetFocus();

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle an QNet state change notification
//--------------------------------------------------------------------------------------
VOID CGameEndScene::QNetStateChanged( QNET_STATE OldState, QNET_STATE NewState, HRESULT hrInfo )
{
    AppXuiScene::QNetStateChanged( OldState, NewState, hrInfo );

    if( SUCCEEDED( hrInfo ) )
    {
        bool fDone = false;

        if( NewState == QNET_STATE_GAME_LOBBY )
        {
            if( g_App.GetQNetSessionType() != QNET_SESSIONTYPE_XBOXLIVE_STANDARD )
            {
                // The game is ranked or local so we can't return to the lobby
                g_App.LeaveQNetGame();
            }
            else
            {
                m_btnDone.SetText( L"Return to Lobby" );
                fDone = true;
            }
        }
        else if( NewState == QNET_STATE_IDLE )
        {
            m_btnDone.SetText( L"Return to Menu" );
            fDone = true;
        }

        ShowFinalScores( fDone );
    }
    else
    {
        if( NewState == QNET_STATE_SESSION_ENDING )
        {
            ShowErrorMessageBox( L"Failed to update the end of game statistics.", hrInfo );
        }
        else
        {
            ShowErrorMessageBox( L"The connection to the game was lost.", hrInfo );
        }

        g_App.LeaveQNetGame();
    }
}

//--------------------------------------------------------------------------------------
// Show the final scores
//--------------------------------------------------------------------------------------
VOID CGameEndScene::ShowFinalScores( bool fShow )
{
    m_labGamertag.SetShow( fShow );
    m_labScore.SetShow( fShow );
    m_plstFinalScores->SetShow( fShow );
    m_btnDone.SetShow( fShow );
    m_btnGamercard.SetShow( fShow );

    m_btnPlayerReview.SetShow( fShow &&
                               ( g_App.GetQNetSessionType() != QNET_SESSIONTYPE_LOCAL ) );

    m_ctlStatus.SetShow( !fShow );
    m_labStatus.SetShow( !fShow );
    m_ctlWait.SetShow( !fShow );

    if( fShow && !m_bMessageBox )
    {
        m_plstFinalScores->SetFocus();
    }
}

} // namespace ArcadeSample

