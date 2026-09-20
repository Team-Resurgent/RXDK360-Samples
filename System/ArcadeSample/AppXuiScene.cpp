//--------------------------------------------------------------------------------------
// AppXuiScene.cpp
//
// 
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "AppXuiScene.h"
#include "App.h"

namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Handle scene initialization
//--------------------------------------------------------------------------------------
HRESULT AppXuiScene::OnInit( XUIMessageInit* pXUIMessageInit, BOOL& bHandled )
{
    m_bMessageBox = FALSE;
    g_App.SetActiveXuiScene( this );
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Handle message box return
//--------------------------------------------------------------------------------------
HRESULT AppXuiScene::OnMsgReturn( XUIMessageMessageBoxReturn* pXUIMessageMessageBoxReturn, BOOL& bHandled )
{
    m_bMessageBox = FALSE;
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Handle return navigation
//--------------------------------------------------------------------------------------
HRESULT AppXuiScene::OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled )
{
    bHandled = FALSE;
    g_App.SetActiveXuiScene( this );

    if( !this->MeetsSceneSigninRequirements() )
    {
        this->NavigateBack();
        bHandled = TRUE;
    }
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Check Signin requirements
//--------------------------------------------------------------------------------------
VOID AppXuiScene::SigninStateChanged()
{
    if( !this->MeetsSceneSigninRequirements() )
    {
        this->NavigateBack();
    }
}

//--------------------------------------------------------------------------------------
// Check Signin requirements
//--------------------------------------------------------------------------------------
BOOL AppXuiScene::SomeoneMeetsSigninRequirements( SIGNIN_REQUIRED signinRequired )
{
    BOOL bMeetsRequirements = FALSE;
    for( DWORD dwUserIndex = 0;
         !bMeetsRequirements && dwUserIndex < XUSER_MAX_COUNT;
         ++dwUserIndex )
    {
        bMeetsRequirements = AppXuiScene::MeetsSigninRequirements( signinRequired, dwUserIndex );
    }
    return bMeetsRequirements;
}

//--------------------------------------------------------------------------------------
// Check Signin requirements
//--------------------------------------------------------------------------------------
BOOL AppXuiScene::MeetsSigninRequirements( SIGNIN_REQUIRED signinRequired, DWORD dwUserIndex )
{
    return ( ( ( signinRequired == NO_SIGNIN_REQUIRED ) ) ||
             ( ( signinRequired == LOCAL_SIGNIN_REQUIRED ) &&
               ( XUserGetSigninState( dwUserIndex ) != eXUserSigninState_NotSignedIn ) ) ||
             ( ( signinRequired == LIVE_SIGNIN_REQUIRED ) &&
               ( XUserGetSigninState( dwUserIndex ) == eXUserSigninState_SignedInToLive ) ) );
}

//--------------------------------------------------------------------------------------
// Check Signin requirements
//--------------------------------------------------------------------------------------
BOOL AppXuiScene::MeetsSceneSigninRequirements() const
{
    return AppXuiScene::SomeoneMeetsSigninRequirements( m_SigninRequired );
}

//--------------------------------------------------------------------------------------
// Check Signin requirements
//--------------------------------------------------------------------------------------
BOOL AppXuiScene::MeetsSceneSigninRequirements( DWORD dwActiveUserIndex ) const
{
    return AppXuiScene::MeetsSigninRequirements( m_SigninRequired, dwActiveUserIndex );
}

//--------------------------------------------------------------------------------------
// Handle a button press
//--------------------------------------------------------------------------------------
HRESULT AppXuiScene::OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled )
{
    DWORD dwActiveUserIndex = ( DWORD )pNotifyPress->UserIndex;
    bHandled = !this->MeetsSceneSigninRequirements( dwActiveUserIndex );

    if( !bHandled )
    {
        g_App.BindCmdUserIndex( dwActiveUserIndex );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Handle scene transitions
//--------------------------------------------------------------------------------------
HRESULT AppXuiScene::OnTransitionEnd( XUIMessageTransition* pTransData, BOOL& bHandled )
{
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Handle rendering
//--------------------------------------------------------------------------------------
HRESULT AppXuiScene::OnRender( XUIMessageRender* pRenderData, BOOL& bHandled )
{
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Helper for showing an error message box
//--------------------------------------------------------------------------------------
VOID AppXuiScene::ShowErrorMessageBox( LPCWSTR szError, HRESULT hResult )
{
    if( !m_bMessageBox )
    {
        LPCWSTR rgButtons[] = { L"OK" };

        const int MESSAGE_SIZE = 256;
        static WCHAR wszMessage[ MESSAGE_SIZE ];
        swprintf_s( wszMessage, L"%s\n\n[ 0x%08X ]", szError, hResult );

        ShowMessageBoxEx(
            L"XuiMessageBox1",
            m_hObj,
            L"ArcadeSample Error",
            wszMessage,
            1,
            rgButtons,
            0,
            XUI_MB_CENTER_ON_PARENT
            );

        m_bMessageBox = TRUE;
    }
}

//--------------------------------------------------------------------------------------
// Navigate forward by scene name.
//--------------------------------------------------------------------------------------
HRESULT AppXuiScene::NavigateBack( BYTE UserIndex )
{
    OutputDebugString( "AppXuiScene::NavigateBack().\n" );

    HRESULT hResult = CXuiScene::NavigateBack( UserIndex );
    if( FAILED( hResult ) )
    {
        OutputDebugString( "CXuiScene::NavigateBack() failed.\n" );
    }

    return hResult;
}

//--------------------------------------------------------------------------------------
// Navigate forward by scene name.
//--------------------------------------------------------------------------------------
HRESULT AppXuiScene::NavigateForward( LPCWSTR szScenePath, BYTE UserIndex )
{
    HXUIOBJ hScene = NULL;

    OutputDebugString( "CXuiScene::SceneCreate( " );
    OutputDebugStringW( szScenePath );
    OutputDebugString( " ).\n" );

    HRESULT hResult = CXuiScene::SceneCreate( szScenePath, &hScene );
    if( SUCCEEDED( hResult ) )
    {
        OutputDebugString( "CXuiScene::NavigateForward().\n" );
        hResult = CXuiScene::NavigateForward( hScene, FALSE, UserIndex );
        if( FAILED( hResult ) )
        {
            OutputDebugString( "CXuiScene::NavigateForward() failed.\n" );
        }
    }
    else
    {
        OutputDebugString( "SceneCreate() failed.\n" );
    }

    return hResult;
}


//--------------------------------------------------------------------------------------
// Check Signin requirements
//--------------------------------------------------------------------------------------
BOOL QNetSessionScene::MeetsSceneSigninRequirements() const
{
    QNET_SESSIONTYPE eSessionType = g_App.GetQNetSessionType();

    return ( ( eSessionType == QNET_SESSIONTYPE_LOCAL ) ||
             ( eSessionType == QNET_SESSIONTYPE_SYSTEMLINK ) )
        ? AppXuiScene::SomeoneMeetsSigninRequirements( LOCAL_SIGNIN_REQUIRED )
        : AppXuiScene::SomeoneMeetsSigninRequirements( LIVE_SIGNIN_REQUIRED );
}

//--------------------------------------------------------------------------------------
// Check Signin requirements
//--------------------------------------------------------------------------------------
BOOL QNetSessionScene::MeetsSceneSigninRequirements( DWORD dwActiveUserIndex ) const
{
    QNET_SESSIONTYPE eSessionType = g_App.GetQNetSessionType();

    return ( ( eSessionType == QNET_SESSIONTYPE_LOCAL ) ||
             ( eSessionType == QNET_SESSIONTYPE_SYSTEMLINK ) )
        ? AppXuiScene::MeetsSigninRequirements( LOCAL_SIGNIN_REQUIRED, dwActiveUserIndex )
        : AppXuiScene::MeetsSigninRequirements( LIVE_SIGNIN_REQUIRED, dwActiveUserIndex );
}

} // namespace ArcadeSample
