//--------------------------------------------------------------------------------------
// GameConnectScene.h
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUI_GAMECONNECTSCENE_H
#define ARCADESAMPLE_XUI_GAMECONNECTSCENE_H

#include "AppXuiScene.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Game connect scene class.
//--------------------------------------------------------------------------------------
class CGameConnectScene : public QNetSessionScene
{
public:
    // Ctor
            CGameConnectScene() : QNetSessionScene()
            {
            }

public:
    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
        XUI_IMPLEMENT_CLASS( CGameConnectScene, L"CGameConnectScene", XUI_CLASS_SCENE )

    VOID    QNetStateChanged( QNET_STATE OldState, QNET_STATE NewState, HRESULT hrInfo );

protected:
    // Control and Element wrapper objects.
    CXuiControl m_btnBack;

    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled );
    HRESULT OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled );
    HRESULT OnMsgReturn( XUIMessageMessageBoxReturn* pXUIMessageMessageBoxReturn, BOOL& bHandled );
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUI_GAMECONNECTSCENE_H
