//--------------------------------------------------------------------------------------
// LeaderboardScene.cpp
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "LeaderboardScene.h"
#include "App.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Get the number of list items
//--------------------------------------------------------------------------------------
HRESULT CLeaderboardList::OnGetItemCountAll( XUIMessageGetItemCount* pGetItemCountData, BOOL& bHandled )
{
    bHandled = TRUE;

    pGetItemCountData->cItems = g_App.GetLeaderboardItemCount();

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Get the list item text
//--------------------------------------------------------------------------------------
HRESULT CLeaderboardList::OnGetSourceText( XUIMessageGetSourceText* pGetSourceText, BOOL& bHandled )
{
    bHandled = TRUE;

    INT item = pGetSourceText->iItem;
    const LeaderboardItem* pItem = g_App.GetLeaderboardItem( item );

    if( pItem != NULL )
    {
        const int DATA_SIZE = 24;
        static WCHAR szData[ DATA_SIZE ];

        switch( pGetSourceText->iData )
        {
            case ARCADESAMPLE_XUI_LEADERBOARD_COLUMN_RANK: // Rank
                _itow_s( pItem->GetRank(), szData, DATA_SIZE, 10 );
                pGetSourceText->szText = szData;
                break;

            case ARCADESAMPLE_XUI_LEADERBOARD_COLUMN_GAMERTAG: // Gamertag
                pGetSourceText->szText = pItem->GetGamertag();
                break;

            case ARCADESAMPLE_XUI_LEADERBOARD_COLUMN_WINS: // Wins
                _i64tow_s( pItem->GetRating(), szData, DATA_SIZE, 10 );
                pGetSourceText->szText = szData;
                break;

            case ARCADESAMPLE_XUI_LEADERBOARD_COLUMN_LOSSES: // Losses
                _i64tow_s( pItem->GetLosses(), szData, DATA_SIZE, 10 );
                pGetSourceText->szText = szData;
                break;
        }
    }
    else
    {
        pGetSourceText->szText = L"...";
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Get the list item image
//--------------------------------------------------------------------------------------
HRESULT CLeaderboardList::OnGetSourceImage( XUIMessageGetSourceImage* pGetSourceImage, BOOL& bHandled )
{
    bHandled = TRUE;

    INT item = pGetSourceImage->iItem;
    const LeaderboardItem* pItem = g_App.GetLeaderboardItem( item );

    if( ( pItem != NULL ) && ( pGetSourceImage->iData == 0 ) )
    {
        XUID xuidPlayer = pItem->GetXUID();
        pGetSourceImage->hBrush = g_App.GetGamerPicBrush( xuidPlayer );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Initialize the scene
//--------------------------------------------------------------------------------------
HRESULT CLeaderboardScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    HRESULT hResult = AppXuiScene::OnInit( pInitData, bHandled );

    hResult = GetChildById( L"btnBack", &m_btnBack );
    hResult = GetChildById( L"btnMode", &m_btnMode );
    hResult = GetChildById( L"labMode", &m_labMode );
    hResult = GetChildById( L"labStatus", &m_labStatus );

    HXUIOBJ hlstScores = NULL;
    hResult = GetChildById( L"lstScores", &hlstScores );
    hResult = XuiObjectFromHandle( hlstScores, reinterpret_cast<void**>( &m_plstScores ) );

    m_StatsFilter = Leaderboards::StatsFilter_Friends;
    hResult = m_labMode.SetText( L"Friends" );

    // Setup the stats spec
    ZeroMemory( &m_StatsSpec, sizeof( m_StatsSpec ) );
    m_StatsSpec.dwViewId = STATS_VIEW_OVERALL;
    m_StatsSpec.dwNumColumnIds = 1;
    m_StatsSpec.rgwColumnIds[ 0 ] = STATS_COLUMN_OVERALL_LOSSES;

    // Refresh the leaderboard
    HRESULT hLBRefreshResult = g_App.RefreshLeaderboard( &m_StatsSpec, m_StatsFilter );
    hResult = m_plstScores->InsertItems( 0, 0 );
    m_bRefreshing = SUCCEEDED( hLBRefreshResult );

    UpdateStatus();

    return hResult;
}

//--------------------------------------------------------------------------------------
// Check on our leaderboard queries
//--------------------------------------------------------------------------------------
HRESULT CLeaderboardScene::OnRender( XUIMessageRender* pRenderData, BOOL& bHandled )
{
    HRESULT hResult = AppXuiScene::OnRender( pRenderData, bHandled );

    if( m_bRefreshing && !g_App.IsLeaderboardRefreshing() )
    {
        // Leaderboard finished refreshing
        hResult = m_plstScores->InsertItems( 0, g_App.GetLeaderboardItemCount() );
        hResult = m_plstScores->SetCurSelVisible( g_App.GetDefaultLeaderboardItemIndex() );
        m_bRefreshing = FALSE;

        UpdateStatus();
    }

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a button press
//--------------------------------------------------------------------------------------
HRESULT CLeaderboardScene::OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled )
{
    HRESULT hResult = AppXuiScene::OnNotifyPressEx( hObjSource, pNotifyPress, bHandled );
    if( !bHandled )
    {
        if( hObjSource == *m_plstScores )
        {
            if( m_plstScores->GetItemCount() > 0 )
            {
                // Show the gamercard
                INT item = m_plstScores->GetCurSel();
                const LeaderboardItem* pItem = g_App.GetLeaderboardItem( item );

                if( pItem != NULL )
                {
                    g_App.ShowGamerCardForXuid( pItem->GetXUID() );
                }
            }
        }
        else if( hObjSource == m_btnBack )
        {
            hResult = NavigateBack();
        }
        else if( hObjSource == m_btnMode )
        {
            // Change the leaderboard filter
            switch( m_StatsFilter )
            {
                case Leaderboards::StatsFilter_Overall:
                    m_StatsFilter = Leaderboards::StatsFilter_MyScore;
                    hResult = m_labMode.SetText( L"My Score" );
                    break;

                case Leaderboards::StatsFilter_MyScore:
                    m_StatsFilter = Leaderboards::StatsFilter_Friends;
                    hResult = m_labMode.SetText( L"Friends" );
                    break;

                case Leaderboards::StatsFilter_Friends:
                    m_StatsFilter = Leaderboards::StatsFilter_Overall;
                    hResult = m_labMode.SetText( L"Overall" );
                    break;
            }

            // Refresh the leaderboard
            HRESULT hLBRefreshResult = g_App.RefreshLeaderboard( &m_StatsSpec, m_StatsFilter );
            hResult = m_plstScores->InsertItems( 0, 0 );
            m_bRefreshing = SUCCEEDED( hLBRefreshResult );

            UpdateStatus();
        }
        bHandled = TRUE;
    }
    return hResult;
}

//--------------------------------------------------------------------------------------
// Update the status text
//--------------------------------------------------------------------------------------
VOID CLeaderboardScene::UpdateStatus()
{
    HRESULT hResult;
    if( m_bRefreshing )
    {
        hResult = m_labStatus.SetText( L"Loading leaderboard..." );
    }
    else if( g_App.GetLeaderboardItemCount() > 0 )
    {
        const int STATUS_SIZE = 64;
        static WCHAR szStatus[ STATUS_SIZE ];
        swprintf_s( szStatus, L"%d players ranked", g_App.GetLeaderboardItemCount() );
        hResult = m_labStatus.SetText( szStatus );
    }
    else
    {
        switch( m_StatsFilter )
        {
            case Leaderboards::StatsFilter_Overall:
                hResult = m_labStatus.SetText( L"Nobody is yet ranked on this leaderboard" );
                break;

            case Leaderboards::StatsFilter_MyScore:
                hResult = m_labStatus.SetText( L"You are not yet ranked on this leaderboard" );
                break;

            case Leaderboards::StatsFilter_Friends:
                hResult = m_labStatus.SetText( L"Your friends are not yet ranked on this leaderboard" );
                break;
        }
    }
}

} // namespace ArcadeSample
