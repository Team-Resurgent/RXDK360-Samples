//--------------------------------------------------------------------------------------
// GameOptionsScene.h
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUI_GAMEOPTIONSSCENE_H
#define ARCADESAMPLE_XUI_GAMEOPTIONSSCENE_H

#include "AppXuiScene.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Game options scene class.
//--------------------------------------------------------------------------------------
class CGameOptionsScene : public QNetSessionScene
{
public:
    // Ctor
            CGameOptionsScene() : QNetSessionScene()
            {
            }

public:
    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
        XUI_IMPLEMENT_CLASS( CGameOptionsScene, L"CGameOptionsScene", XUI_CLASS_SCENE )

    VOID    QNetStateChanged( QNET_STATE OldState, QNET_STATE NewState, HRESULT hrInfo );

protected:
    // Control and Element wrapper objects.
    CXuiControl m_btnBack;
    CXuiControl m_btnCreateGame;
    CXuiList m_lstMaxPoints;
    CXuiList m_lstPrivate;

    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled );
    HRESULT OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled );
    HRESULT OnMsgReturn( XUIMessageMessageBoxReturn* pXUIMessageMessageBoxReturn, BOOL& bHandled );
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUI_GAMEOPTIONSSCENE_H
