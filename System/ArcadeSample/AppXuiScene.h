//--------------------------------------------------------------------------------------
// AppXuiScene.h
//
// Application class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_APPXUISCENE_H
#define ARCADESAMPLE_APPXUISCENE_H


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Forward Declarations
//--------------------------------------------------------------------------------------
class Player;

//--------------------------------------------------------------------------------------
// Required Sign-in Type
//--------------------------------------------------------------------------------------
enum SIGNIN_REQUIRED
{
    NO_SIGNIN_REQUIRED,

    LOCAL_SIGNIN_REQUIRED,
    LIVE_SIGNIN_REQUIRED,

    DEPENDENT_SIGNIN_REQUIRED,
};

//--------------------------------------------------------------------------------------
// AppScene class
//--------------------------------------------------------------------------------------
class AppXuiScene : public CXuiSceneImpl
{
public:
    // Ctor
                    AppXuiScene( SIGNIN_REQUIRED signinRequired ) : m_bMessageBox( FALSE ),
                                                                    m_SigninRequired( signinRequired )
                    {
                    }

public:
    virtual VOID    SigninStateChanged();

    virtual VOID DisplayPlayScene() {};

    // QNet notifications - TODO: tailer these to more UI specific notifications
    virtual VOID    QNetStateChanged( QNET_STATE OldState, QNET_STATE NewState, HRESULT hrInfo )
    {
    }
    virtual VOID    QNetPlayerJoined( Player& pPlayer )
    {
    }
    virtual VOID    QNetPlayerLeaving( Player& pPlayer )
    {
    }
    virtual VOID    QNetNewHost( Player& pPlayer )
    {
    }
    virtual VOID    QNetReadinessChanged( Player& pPlayer, BOOL bReady )
    {
    }
    virtual VOID    QNetCommSettingsChanged( Player& pPlayer )
    {
    }
    virtual VOID    QNetGameSearchComplete( IQNetGameSearch* pGameSearch, HRESULT hrComplete, DWORD dwNumResults )
    {
    }
    virtual VOID    QNetGameInvite( DWORD dwUserIndex, const XINVITE_INFO& pInviteInfo )
    {
    }
    virtual VOID    QNetContextChanged( const XUSER_CONTEXT& pContext )
    {
    }
    virtual VOID    QNetPropertyChanged( const XUSER_PROPERTY& pProperty )
    {
    }

protected:
    // XUI Message Map
        XUI_BEGIN_MSG_MAP()
            XUI_ON_XM_INIT( OnInit )
            XUI_ON_XM_MSG_RETURN( OnMsgReturn )
            XUI_ON_XM_NAV_RETURN( OnNavReturn )
            XUI_ON_XM_NOTIFY_PRESS_EX( OnNotifyPressEx )
            XUI_ON_XM_TRANSITION_END( OnTransitionEnd )
            XUI_ON_XM_RENDER( OnRender )
        XUI_END_MSG_MAP()

    // XUI callbacks
    virtual HRESULT OnInit( XUIMessageInit* pXUIMessageInit, BOOL& bHandled );
    virtual HRESULT OnMsgReturn( XUIMessageMessageBoxReturn* pXUIMessageMessageBoxReturn, BOOL& bHandled );
    virtual HRESULT OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled );
    virtual HRESULT OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled );
    virtual HRESULT OnTransitionEnd( XUIMessageTransition* pTransData, BOOL& bHandled );
    virtual HRESULT OnRender( XUIMessageRender* pRenderData, BOOL& bHandled );

    // Helper for showing an error message box
    VOID            ShowErrorMessageBox( LPCWSTR szError, HRESULT hResult );

    // Navigate forward by scene name
    HRESULT         NavigateBack( BYTE UserIndex = XUSER_INDEX_FOCUS );
    HRESULT         NavigateForward( LPCWSTR szScenePath, BYTE UserIndex = XUSER_INDEX_FOCUS );

    // Signin requirements
    virtual BOOL    MeetsSceneSigninRequirements() const;
    virtual BOOL    MeetsSceneSigninRequirements( DWORD dwActiveUserIndex ) const;

    // Signin requirements
    static BOOL     MeetsSigninRequirements( SIGNIN_REQUIRED signinRequired, DWORD dwActiveUserIndex );
    static BOOL     SomeoneMeetsSigninRequirements( SIGNIN_REQUIRED signinRequired );

protected:
    BOOL m_bMessageBox;
    SIGNIN_REQUIRED m_SigninRequired;
};

//--------------------------------------------------------------------------------------
// QNetSessionScene class
//--------------------------------------------------------------------------------------
class QNetSessionScene : public AppXuiScene
{
public:
    // Ctor
                    QNetSessionScene() : AppXuiScene( DEPENDENT_SIGNIN_REQUIRED )
                    {
                    }

protected:
    // Signin requirements
    virtual BOOL    MeetsSceneSigninRequirements() const;
    virtual BOOL    MeetsSceneSigninRequirements( DWORD dwActiveUserIndex ) const;
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_APPXUISCENE_H
