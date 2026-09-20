//--------------------------------------------------------------------------------------
// GameTypeScene.h
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUI_GAMETYPESCENE_H
#define ARCADESAMPLE_XUI_GAMETYPESCENE_H

#include "AppXuiScene.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Game type scene class.
//--------------------------------------------------------------------------------------
class CGameTypeScene : public AppXuiScene
{
public:
    // Ctor
            CGameTypeScene() : AppXuiScene( LIVE_SIGNIN_REQUIRED )
            {
            }

public:
    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
        XUI_IMPLEMENT_CLASS( CGameTypeScene, L"CGameTypeScene", XUI_CLASS_SCENE )

protected:
    // Control and Element wrapper objects.
    CXuiControl m_btnBack;
    CXuiControl m_btnQuick;
    CXuiControl m_btnCustom;
    CXuiControl m_btnCreate;
    CXuiList m_lstMatchType;

    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled );
    HRESULT OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled );
    HRESULT OnMsgReturn( XUIMessageMessageBoxReturn* pXUIMessageMessageBoxReturn, BOOL& bHandled );
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUI_GAMETYPESCENE_H
