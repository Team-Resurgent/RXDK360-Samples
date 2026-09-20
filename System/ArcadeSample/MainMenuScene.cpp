//--------------------------------------------------------------------------------------
// MainMenuScene.cpp
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "MainMenuScene.h"
#include "App.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Initialize the scene
//--------------------------------------------------------------------------------------
HRESULT CMainMenuScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    HRESULT hResult = AppXuiScene::OnInit( pInitData, bHandled );

    hResult = GetChildById( L"btnLocal", &m_btnLocal );
    hResult = GetChildById( L"btnXboxLive", &m_btnXboxLive );
    hResult = GetChildById( L"btnLeaderboard", &m_btnLeaderboard );
    hResult = GetChildById( L"btnAchievements", &m_btnAchievements );
    hResult = GetChildById( L"btnHelpOptions", &m_btnHelpOptions );
    hResult = GetChildById( L"btnUnlock", &m_btnUnlock );
    hResult = GetChildById( L"btnReturnToArcade", &m_btnReturnToArcade );

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a button press
//--------------------------------------------------------------------------------------
HRESULT CMainMenuScene::OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled )
{
    HRESULT hResult = AppXuiScene::OnNotifyPressEx( hObjSource, pNotifyPress, bHandled );
    if( !bHandled )
    {
        DWORD dwResult;
        DWORD dwActiveUserIndex = ( DWORD )pNotifyPress->UserIndex;

        // Refresh the gamerpics
        g_App.RefreshGamerPic( dwActiveUserIndex );

        // Which button did they press?
        if( hObjSource == m_btnLocal )
        {
            if( ( XUserGetSigninState( dwActiveUserIndex ) != eXUserSigninState_SignedInLocally ) &&
                ( XUserGetSigninState( dwActiveUserIndex ) != eXUserSigninState_SignedInToLive ) )
            {
                dwResult = XShowSigninUI( 4, XSSUI_FLAGS_LOCALSIGNINONLY );
            }
            else
            {
                NavigateForward( L"GameOptionsScene.xur" );
                hResult = g_App.StartQNet( QNET_SESSIONTYPE_LOCAL );
                if( FAILED( hResult ) )
                {
                    OutputDebugString( "StartQNet() failed.\n" );
                    NavigateBack();
                }
            }
        }
        else if( hObjSource == m_btnXboxLive )
        {
            if( XUserGetSigninState( dwActiveUserIndex ) != eXUserSigninState_SignedInToLive )
            {
                dwResult = XShowSigninUI( 1, XSSUI_FLAGS_SHOWONLYONLINEENABLED );
            }
            else
            {
                hResult = NavigateForward( L"GameTypeScene.xur" );
            }
        }
        else if( hObjSource == m_btnLeaderboard )
        {
            if( XUserGetSigninState( dwActiveUserIndex ) != eXUserSigninState_SignedInToLive )
            {
                dwResult = XShowSigninUI( 1, XSSUI_FLAGS_SHOWONLYONLINEENABLED );
            }
            else
            {
                hResult = NavigateForward( L"LeaderboardScene.xur" );
            }
        }
        else if( hObjSource == m_btnAchievements )
        {
            if( XUserGetSigninState( dwActiveUserIndex ) == eXUserSigninState_NotSignedIn )
            {
                dwResult = XShowSigninUI( 1, 0 );
            }
            else
            {
                dwResult = XShowAchievementsUI( dwActiveUserIndex );
            }
        }
        else if( hObjSource == m_btnHelpOptions )
        {
            hResult = NavigateForward( L"HelpOptionsScene.xur" );
        }
        else if( hObjSource == m_btnUnlock )
        {
            if( XUserGetSigninState( dwActiveUserIndex ) != eXUserSigninState_SignedInToLive )
            {
                dwResult = XShowSigninUI( 1, XSSUI_FLAGS_SHOWONLYONLINEENABLED );
            }
            else
            {
                dwResult = XShowMarketplaceUI(
                    dwActiveUserIndex,
                    XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM,
                    ( ( ULONGLONG )TITLEID_ARCADESAMPLE << 32 ) + 1,
                    ( DWORD )-1
                    );
            }
        }
        else if( hObjSource == m_btnReturnToArcade )
        {
            g_App.SuspendD3DDevice();
            XLaunchNewImage( XLAUNCH_KEYWORD_DASH_ARCADE, 0 );
        }
        bHandled = TRUE;
    }
    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a return navigation
//--------------------------------------------------------------------------------------
HRESULT CMainMenuScene::OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled )
{
    HRESULT hResult = AppXuiScene::OnNavReturn( hSceneFrom, bHandled );
    if( !bHandled )
    {
        bHandled = TRUE;
    }

    g_App.StopQNet();
    return hResult;
}

HRESULT CMainMenuScene::OnRender( XUIMessageRender* pRenderData, BOOL& bHandled )
{
    // Check the license to see if this is the full version
    CheckLicense();

    return AppXuiScene::OnRender( pRenderData, bHandled );
}

//--------------------------------------------------------------------------------------
// Check the license to see if this is the full version
//--------------------------------------------------------------------------------------
VOID CMainMenuScene::CheckLicense()
{
    BOOL bPurchased = g_App.CheckLicense( eLicense_Purchased );

    if( bPurchased )
    {
        m_btnUnlock.SetText( L"Full Game Enabled" );
        m_btnUnlock.SetEnable( FALSE );
    }
}

} // namespace ArcadeSample
