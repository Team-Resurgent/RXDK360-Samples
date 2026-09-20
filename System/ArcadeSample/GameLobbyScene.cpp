//--------------------------------------------------------------------------------------
// GameLobbyScene.cpp
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "GameLobbyScene.h"
#include "App.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Get the number of list items
//--------------------------------------------------------------------------------------
HRESULT CGameLobbyList::OnGetItemCountAll( XUIMessageGetItemCount* pGetItemCountData, BOOL& bHandled )
{
    bHandled = TRUE;

    pGetItemCountData->cItems = 4;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Get the list item text
//--------------------------------------------------------------------------------------
HRESULT CGameLobbyList::OnGetSourceText( XUIMessageGetSourceText* pGetSourceText, BOOL& bHandled )
{
    bHandled = TRUE;

    Player* pPlayer = g_App.PlayerForUIIndex( pGetSourceText->iItem );

    if( pPlayer )
    {
        if( eText_Gamertag == pGetSourceText->iData )
        {
            pGetSourceText->szText = pPlayer->GetGamertag();
        }
        else if( eText_QoS == pGetSourceText->iData )
        {
            const int PING_SIZE = 32;
            static WCHAR wszPing[ PING_SIZE ];
            swprintf_s( wszPing, L"%d ms", pPlayer->GetCurrentRtt() );

            pGetSourceText->szText = wszPing;
        }
        else if( eText_Voice == pGetSourceText->iData )
        {
            if( pPlayer->HasVoice() )
            {
                if( pPlayer->IsMutedByLocalUser( XUSER_INDEX_ANY ) )
                {
                    pGetSourceText->szText = L"MUTE";
                }
                else
                {
                    pGetSourceText->szText = L"VOICE";
                }
            }
        }
        else if( eText_Ready == pGetSourceText->iData )
        {
            if( pPlayer->IsReady() )
            {
                pGetSourceText->szText = L"READY";
            }
            else
            {
                pGetSourceText->szText = L"NOT READY";
            }
        }
    }
    else
    {
        if( eText_Gamertag == pGetSourceText->iData )
        {
            pGetSourceText->szText = L"OPEN";
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Get the list item image
//--------------------------------------------------------------------------------------
HRESULT CGameLobbyList::OnGetSourceImage( XUIMessageGetSourceImage* pGetSourceImage, BOOL& bHandled )
{
    bHandled = TRUE;

    Player* pPlayer = g_App.PlayerForUIIndex( pGetSourceImage->iItem );

    if( pPlayer != NULL )
    {
        if( eImg_GamerPic == pGetSourceImage->iData )
        {
            if( pPlayer->HasCamera() )
            {
                HXUIOBJ hListItem =
                    GetItemControl( pGetSourceImage->iItem );

                HXUIOBJ hVisual = NULL;
                XuiControlGetVisual( hListItem, &hVisual );

                HXUIOBJ hImagePresenter = NULL;
                XuiElementGetChildById( hVisual, L"image_Position0", &hImagePresenter );

                D3DXVECTOR3 vPos;
                GetPosition( &vPos );

                D3DXVECTOR3 vListItemPos;
                XuiElementGetPosition( hListItem, &vListItemPos );
                vPos += vListItemPos;

                D3DXVECTOR3 vImagePresenterPos;
                XuiElementGetPosition( hImagePresenter, &vImagePresenterPos );
                vPos += vImagePresenterPos;
                vPos.x -= ( 1280 - ATG::Application::m_d3dpp.BackBufferWidth ) / 2;

                float width, height;
                XuiElementGetBounds( hImagePresenter, &width, &height );

                D3DRECT rc;
                rc.x1 = ( int )vPos.x;
                rc.y1 = ( int )vPos.y;
                rc.x2 = ( int )( vPos.x + width );
                rc.y2 = ( int )( vPos.y + height );

                if( pPlayer->IsLocal() )
                {
                    std::swap( rc.x1, rc.x2 );
                }

                if( FAILED( g_App.RenderCamera( pPlayer, rc ) ) )
                {
                    pGetSourceImage->hBrush = g_App.GetGamerPicBrush( pPlayer->GetXuid() );
                }
            }
            else
            {
                pGetSourceImage->hBrush = g_App.GetGamerPicBrush( pPlayer->GetXuid() );
            }
        }
        else if( eImg_QoS == pGetSourceImage->iData )
        {
            DWORD dwPing = pPlayer->GetCurrentRtt();
            if( dwPing <= 50 )
            {
                pGetSourceImage->szPath = L"file://game:/ArcadeSample.xzp#ping0.png";
            }
            else if( dwPing <= 100 )
            {
                pGetSourceImage->szPath = L"file://game:/ArcadeSample.xzp#ping1.png";
            }
            else if( dwPing <= 150 )
            {
                pGetSourceImage->szPath = L"file://game:/ArcadeSample.xzp#ping2.png";
            }
            else if( dwPing <= 200 )
            {
                pGetSourceImage->szPath = L"file://game:/ArcadeSample.xzp#ping3.png";
            }
            else
            {
                pGetSourceImage->szPath = L"file://game:/ArcadeSample.xzp#ping4.png";
            }
        }
        else if( eImg_Voice == pGetSourceImage->iData )
        {
            if( !pPlayer->HasVoice() )
            {
                pGetSourceImage->szPath = L"";
            }
            else if( pPlayer->IsMutedByLocalUser( XUSER_INDEX_ANY ) )
            {
                pGetSourceImage->szPath = L"file://game:/ArcadeSample.xzp#mute.png";
            }
            else if( pPlayer->IsTalking() )
            {
                switch( ( GetTickCount() / 250 ) % 3 )
                {
                    case 0:
                        pGetSourceImage->szPath = L"file://game:/ArcadeSample.xzp#voice0.png";
                        break;

                    case 1:
                        pGetSourceImage->szPath = L"file://game:/ArcadeSample.xzp#voice1.png";
                        break;

                    case 2:
                        pGetSourceImage->szPath = L"file://game:/ArcadeSample.xzp#voice2.png";
                        break;
                }
            }
            else
            {
                pGetSourceImage->szPath = L"file://game:/ArcadeSample.xzp#voice0.png";
            }
        }
        else if( eImg_Ready == pGetSourceImage->iData )
        {
            if( pPlayer->IsReady() )
            {
                pGetSourceImage->szPath = L"file://game:/ArcadeSample.xzp#ready.png";
            }
            else
            {
                pGetSourceImage->szPath = L"file://game:/ArcadeSample.xzp#notready.png";
            }
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Initialize the scene
//--------------------------------------------------------------------------------------
HRESULT CGameLobbyScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnInit( pInitData, bHandled );

    hResult = GetChildById( L"labStatus", &m_labStatus );
    hResult = GetChildById( L"btnReady", &m_btnReady );
    hResult = GetChildById( L"btnBack", &m_btnBack );
    hResult = GetChildById( L"btnGamercard", &m_btnGamercard );
    hResult = GetChildById( L"btnFriends", &m_btnFriends );

    HXUIOBJ hlstPlayers = NULL;
    if( SUCCEEDED( GetChildById( L"lstPlayers", &hlstPlayers ) ) )
    {
        hResult = XuiObjectFromHandle( hlstPlayers, reinterpret_cast<void**>( &m_plstPlayers ) );
    }

    UpdateStatus();

    g_App.SetCameraStreams( QNET_CAMERA_STREAMS_BOTH );

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a button press
//--------------------------------------------------------------------------------------
HRESULT CGameLobbyScene::OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnNotifyPressEx( hObjSource, pNotifyPress, bHandled );
    if( !bHandled )
    {
        DWORD dwResult;
        DWORD dwUserIndex = ( DWORD )pNotifyPress->UserIndex;

        Player* pPlayer = g_App.GetLocalPlayer( dwUserIndex );

        if( pPlayer == NULL )
        {
            if( ( hObjSource == m_btnReady ) || ( hObjSource == *m_plstPlayers ) )
            {
                if( XUserGetSigninState( dwUserIndex ) == eXUserSigninState_SignedInToLive )
                {
                    // Add the signed in player
                    HRESULT hAddResult = g_App.AddLocalPlayer( dwUserIndex );
                    if( SUCCEEDED( hAddResult ) )
                    {
                        g_App.SetControllerHasUser( dwUserIndex, TRUE );
                    }
                }
            }
        }
        else if( hObjSource == m_btnBack )
        {
            if( pPlayer->IsReady() )
            {
                // Request that the player become not ready
                hResult = pPlayer->SetReady( FALSE );
            }
            else
            {
                // Leave the game
                g_App.LeaveQNetGame();
            }
        }
        else if( ( hObjSource == m_btnReady ) || ( hObjSource == *m_plstPlayers ) )
        {
            if( !pPlayer->IsReady() )
            {
                // Request that the player become ready
                hResult = pPlayer->SetReady( TRUE );
            }
        }
        else if( hObjSource == m_btnGamercard )
        {
            // Show the gamer card
            Player* pOnePlayer = g_App.PlayerForUIIndex( m_plstPlayers->GetCurSel() );

            if( pOnePlayer != NULL )
            {
                dwResult = XShowGamerCardUI( dwUserIndex, pOnePlayer->GetXuid() );
            }
        }
        else if( hObjSource == m_btnFriends )
        {
            // Show the friends list
            dwResult = XShowFriendsUI( dwUserIndex );
        }
        bHandled = TRUE;
    }
    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a return navigation
//--------------------------------------------------------------------------------------
HRESULT CGameLobbyScene::OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled )
{
    HRESULT hResult = QNetSessionScene::OnNavReturn( hSceneFrom, bHandled );
    if( !bHandled )
    {
        QNET_STATE eState;
        if( !g_App.GetQNetState( &eState ) )
        {
            hResult = NavigateBack();
        }
        else if( eState == QNET_STATE_IDLE )
        {
            hResult = NavigateBack();
        }
        else
        {
            UpdateStatus();

            g_App.SetCameraStreams( QNET_CAMERA_STREAMS_BOTH );
        }
        bHandled = TRUE;
    }
    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a QNet state notification
//--------------------------------------------------------------------------------------
VOID CGameLobbyScene::QNetStateChanged( QNET_STATE OldState, QNET_STATE NewState, HRESULT hrInfo )
{
    QNetSessionScene::QNetStateChanged( OldState, NewState, hrInfo );

    UpdateStatus();

    HRESULT hResult;
    if( NewState == QNET_STATE_IDLE )
    {
        hResult = NavigateBack();
    }
    else if( ( NewState == QNET_STATE_SESSION_REGISTERING ) ||
             ( NewState == QNET_STATE_SESSION_STARTING ) )
    {
        hResult = NavigateForward( L"GameStartScene.xur" );

        g_App.SetCameraStreams( QNET_CAMERA_STREAMS_NONE );
    }
}

//--------------------------------------------------------------------------------------
// Handle a QNet player join notification
//--------------------------------------------------------------------------------------
VOID CGameLobbyScene::QNetPlayerJoined( Player& pPlayer )
{
    QNetSessionScene::QNetPlayerJoined( pPlayer );

    UpdateStatus();
}

//--------------------------------------------------------------------------------------
// Handle a QNet player leave notification
//--------------------------------------------------------------------------------------
VOID CGameLobbyScene::QNetPlayerLeaving( Player& pPlayer )
{
    QNetSessionScene::QNetPlayerLeaving( pPlayer );

    UpdateStatus();
}

//--------------------------------------------------------------------------------------
// Handle a QNet player ready notification
//--------------------------------------------------------------------------------------
VOID CGameLobbyScene::QNetReadinessChanged( Player& pPlayer, BOOL bReady )
{
    QNetSessionScene::QNetReadinessChanged( pPlayer, bReady );

    UpdateStatus();
}

//--------------------------------------------------------------------------------------
// Update the status text
//--------------------------------------------------------------------------------------
VOID CGameLobbyScene::UpdateStatus()
{
    BOOL bAllReady = TRUE;
    for( AppClass::PlayerItT playerIt = g_App.PlayerBegin(); bAllReady && ( playerIt != g_App.PlayerEnd() );
         ++playerIt )
    {
        if( ( *playerIt )->IsLocal() && !( *playerIt )->IsReady() )
            bAllReady = FALSE;
    }

    m_btnReady.SetShow( !bAllReady );

    if( g_App.HasProperTeamPlayers() )
    {
        if( g_App.IsEveryoneReady() )
        {
            m_labStatus.SetText( L"Waiting for the game to start..." );
        }
        else if( bAllReady )
        {
            m_labStatus.SetText( L"Waiting for other players to get ready..." );
        }
        else
        {
            m_labStatus.SetText( L"Get ready to start the game by pressing the A button..." );
        }
    }
    else
    {
        m_labStatus.SetText( L"Waiting for other players to join..." );
    }
}

} // namespace ArcadeSample
