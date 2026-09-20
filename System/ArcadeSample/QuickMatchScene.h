//--------------------------------------------------------------------------------------
// QuickMatchScene.h
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUI_QUICKMATCHSCENE_H
#define ARCADESAMPLE_XUI_QUICKMATCHSCENE_H

#include "AppXuiScene.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Quick match scene class.
//--------------------------------------------------------------------------------------
class CQuickMatchScene : public QNetSessionScene
{
public:
    // Ctor
            CQuickMatchScene() : QNetSessionScene()
            {
            }

public:
    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
        XUI_IMPLEMENT_CLASS( CQuickMatchScene, L"CQuickMatchScene", XUI_CLASS_SCENE )

    VOID    QNetStateChanged( QNET_STATE OldState, QNET_STATE NewState, HRESULT hrInfo );
    VOID    QNetGameSearchComplete( IQNetGameSearch* pGameSearch, HRESULT hrComplete, DWORD dwNumResults );

protected:
    // Control and Element wrapper objects.
    CXuiControl m_btnBack;

    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled );
    HRESULT OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled );
    HRESULT OnMsgReturn( XUIMessageMessageBoxReturn* pXUIMessageMessageBoxReturn, BOOL& bHandled );
    HRESULT OnRender( XUIMessageRender* pRenderData, BOOL& bHandled );

    bool    HasTriedGame( DWORD dwResult );
    VOID    TryToJoinGame();
    VOID    TryToHostGame();

    const static DWORD c_QoSMinDelay = 3000;
    const static DWORD c_QoSMaxDelay = 10000;
    const static DWORD c_QosMinResults = 5;
    const static DWORD c_MaxGamesToTry = 3;

    DWORD m_dwGamesFoundTick;
    DWORD   m_rgGamesTried[ c_MaxGamesToTry ];
    DWORD m_cGamesTried;

    enum State
    {
        State_WaitingForGamesFound,
        State_WaitingForQoS,
        State_JoiningGame,
        State_HostingGame,
        State_Cancel
    } m_State;
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUI_QUICKMATCHSCENE_H
