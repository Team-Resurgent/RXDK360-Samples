//--------------------------------------------------------------------------------------
// GameListScene.cpp
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "GameListScene.h"
#include "App.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Get the number of list items
//--------------------------------------------------------------------------------------
HRESULT CGameList::OnGetItemCountAll( XUIMessageGetItemCount* pGetItemCountData, BOOL& bHandled )
{
    bHandled = TRUE;

    pGetItemCountData->cItems = g_App.GetQNetGameSearch()->GetNumResults();

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Get the list item text
//--------------------------------------------------------------------------------------
HRESULT CGameList::OnGetSourceText( XUIMessageGetSourceText* pGetSourceText, BOOL& bHandled )
{
    const XSESSION_SEARCHRESULT* pSearchResult =
        g_App.GetQNetGameSearch()->GetSearchResultAtIndex( pGetSourceText->iItem );
    const XNQOSINFO* pQosResult =
        g_App.GetQNetGameSearch()->GetQosInfoAtIndex( pGetSourceText->iItem );

    bHandled = TRUE;

    if( pSearchResult != NULL )
    {
        if( eText_Slots == pGetSourceText->iData )
        {
            DWORD dwFilledSlots = pSearchResult->dwFilledPublicSlots + pSearchResult->dwFilledPrivateSlots;
            DWORD dwOpenSlots = pSearchResult->dwOpenPublicSlots + pSearchResult->dwOpenPrivateSlots;

            const int SLOTS_SIZE = 32;
            static WCHAR wszSlots[ SLOTS_SIZE ];
            swprintf_s( wszSlots, L"%d/%d", dwFilledSlots, dwOpenSlots + dwFilledSlots );

            pGetSourceText->szText = wszSlots;
        }
        else if( eText_Host == pGetSourceText->iData )
        {
            for( int iProperty = 0; iProperty < ( int )pSearchResult->cProperties; iProperty++ )
            {
                if( pSearchResult->pProperties[ iProperty ].dwPropertyId == X_PROPERTY_GAMER_HOSTNAME )
                {
                    pGetSourceText->szText = pSearchResult->pProperties[ iProperty ].value.string.pwszData;
                    break;
                }
            }
        }
        else if( eText_QoS == pGetSourceText->iData )
        {
            if( ( pQosResult != NULL ) &&
                ( pQosResult->bFlags & XNET_XNQOSINFO_COMPLETE ) &&
                ( pQosResult->wRttMedInMsecs != 0xFFFF ) )
            {
                const int PING_SIZE = 32;
                static WCHAR wszPing[ PING_SIZE ];
                swprintf_s( wszPing, L"%d ms", pQosResult->wRttMedInMsecs );

                pGetSourceText->szText = wszPing;
            }
            else
            {
                pGetSourceText->szText = L"---";
            }
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
HRESULT CGameList::OnGetSourceImage( XUIMessageGetSourceImage* pGetSourceImage, BOOL& bHandled )
{
    const XSESSION_SEARCHRESULT* pSearchResult =
        g_App.GetQNetGameSearch()->GetSearchResultAtIndex( pGetSourceImage->iItem );
    const XNQOSINFO* pQosResult =
        g_App.GetQNetGameSearch()->GetQosInfoAtIndex( pGetSourceImage->iItem );

    bHandled = TRUE;

    if( pSearchResult != NULL )
    {
        if( eImg_GamerPic == pGetSourceImage->iData )
        {
            for( int iProperty = 0; iProperty < ( int )pSearchResult->cProperties; iProperty++ )
            {
                if( pSearchResult->pProperties[ iProperty ].dwPropertyId == X_PROPERTY_GAMER_PUID )
                {
                    XUID xuidPlayer = pSearchResult->pProperties[ iProperty ].value.i64Data;
                    pGetSourceImage->hBrush = g_App.GetGamerPicBrush( xuidPlayer );
                    break;
                }
            }
        }
        else if( eImg_QoS == pGetSourceImage->iData )
        {
            if( ( pQosResult != NULL ) &&
                ( pQosResult->bFlags & XNET_XNQOSINFO_COMPLETE ) &&
                ( pQosResult->wRttMedInMsecs != 0xFFFF ) )
            {
                if( pQosResult->wRttMedInMsecs <= 50 )
                {
                    pGetSourceImage->szPath = L"file://game:/ArcadeSample.xzp#ping0.png";
                }
                else if( pQosResult->wRttMedInMsecs <= 100 )
                {
                    pGetSourceImage->szPath = L"file://game:/ArcadeSample.xzp#ping1.png";
                }
                else if( pQosResult->wRttMedInMsecs <= 150 )
                {
                    pGetSourceImage->szPath = L"file://game:/ArcadeSample.xzp#ping2.png";
                }
                else if( pQosResult->wRttMedInMsecs <= 200 )
                {
                    pGetSourceImage->szPath = L"file://game:/ArcadeSample.xzp#ping3.png";
                }
                else
                {
                    pGetSourceImage->szPath = L"file://game:/ArcadeSample.xzp#ping4.png";
                }
            }
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Initialize the scene
//--------------------------------------------------------------------------------------
HRESULT CGameListScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnInit( pInitData, bHandled );

    hResult = GetChildById( L"labStatus", &m_labStatus );
    hResult = GetChildById( L"btnBack", &m_btnBack );
    hResult = GetChildById( L"btnCreateGame", &m_btnCreateGame );
    hResult = GetChildById( L"btnRefresh", &m_btnRefresh );

    HXUIOBJ hlstGames = NULL;
    if( SUCCEEDED( GetChildById( L"lstGames", &hlstGames ) ) )
    {
        hResult = XuiObjectFromHandle( hlstGames, reinterpret_cast<void**>( &m_plstGames ) );
    }

    m_labStatus.SetText( L"Searching for games..." );

    HRESULT hSearchResult = g_App.SearchForQNetGames();
    if( FAILED( hSearchResult ) )
    {
        ShowErrorMessageBox( L"Failed to search for games.", hSearchResult );
    }

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a button press
//--------------------------------------------------------------------------------------
HRESULT CGameListScene::OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnNotifyPressEx( hObjSource, pNotifyPress, bHandled );
    if( !bHandled )
    {
        if( hObjSource == m_btnBack )
        {
            g_App.GetQNetGameSearch()->Abort();

            hResult = NavigateBack();
        }
        else if( hObjSource == m_btnCreateGame )
        {
            hResult = NavigateForward( L"GameOptionsScene.xur" );
            g_App.GetQNetGameSearch()->Abort();
        }
        else if( hObjSource == m_btnRefresh )
        {
            HRESULT hSearchResult = g_App.SearchForQNetGames();

            if( SUCCEEDED( hSearchResult ) )
            {
                hResult = m_plstGames->InsertItems( 0, 0 );
                hResult = m_labStatus.SetText( L"Searching for games..." );
            }
            else
            {
                ShowErrorMessageBox( L"Failed to search for games.", hSearchResult );
            }
        }
        else if( hObjSource == *m_plstGames )
        {
            const XSESSION_SEARCHRESULT* pSearchResult =
                g_App.GetQNetGameSearch()->GetSearchResultAtIndex( m_plstGames->GetCurSel() );

            if( pSearchResult != NULL )
            {
                NavigateForward( L"GameConnectScene.xur" );
                HRESULT hJoinResult = g_App.JoinQNetGame( pSearchResult );

                if( FAILED( hJoinResult ) )
                {
                    ShowErrorMessageBox( L"Failed to join the game.", hJoinResult );
                    NavigateBack();
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
HRESULT CGameListScene::OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnNavReturn( hSceneFrom, bHandled );
    if( !bHandled )
    {
        hResult = g_App.SearchForQNetGames();
        if( SUCCEEDED( hResult ) )
        {
            hResult = m_plstGames->InsertItems( 0, 0 );
            hResult = m_labStatus.SetText( L"Searching for games..." );
        }
        else
        {
            ShowErrorMessageBox( L"Failed to search for games.", hResult );
        }
        bHandled = TRUE;
    }
    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a message box completion
//--------------------------------------------------------------------------------------
HRESULT CGameListScene::OnMsgReturn( XUIMessageMessageBoxReturn* pXUIMessageMessageBoxReturn, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnMsgReturn( pXUIMessageMessageBoxReturn, bHandled );

    hResult = NavigateBack();

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle an XANet games found notification
//--------------------------------------------------------------------------------------
VOID CGameListScene::QNetGameSearchComplete( IQNetGameSearch* pGameSearch, HRESULT hrComplete, DWORD dwNumResults )
{
    QNetSessionScene::QNetGameSearchComplete( pGameSearch, hrComplete, dwNumResults );

    if( SUCCEEDED( hrComplete ) )
    {
        const int STATUS_SIZE = 64;
        static WCHAR wszStatus[ STATUS_SIZE ];
        swprintf_s( wszStatus, L"Found %d games", dwNumResults );

        HRESULT hResult = m_labStatus.SetText( wszStatus );
        hResult = m_plstGames->InsertItems( 0, dwNumResults );
    }
    else
    {
        ShowErrorMessageBox( L"Failed to search for games.", hrComplete );
    }
}

} // namespace ArcadeSample
