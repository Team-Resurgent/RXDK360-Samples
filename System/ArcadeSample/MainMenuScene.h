//--------------------------------------------------------------------------------------
// MainMenuScene.h
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUI_MAINMENUSCENE_H
#define ARCADESAMPLE_XUI_MAINMENUSCENE_H

#include "AppXuiScene.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Main menu scene class.
//--------------------------------------------------------------------------------------
class CMainMenuScene : public AppXuiScene
{
public:
    // Ctor
            CMainMenuScene() : AppXuiScene( NO_SIGNIN_REQUIRED )
            {
            }

public:
    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
        XUI_IMPLEMENT_CLASS( CMainMenuScene, L"CMainMenuScene", XUI_CLASS_SCENE )

protected:
    // Control and Element wrapper objects.
    CXuiControl m_btnLocal;
    CXuiControl m_btnXboxLive;
    CXuiControl m_btnLeaderboard;
    CXuiControl m_btnAchievements;
    CXuiControl m_btnHelpOptions;
    CXuiControl m_btnUnlock;
    CXuiControl m_btnReturnToArcade;

    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled );
    HRESULT OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled );
    HRESULT OnRender( XUIMessageRender* pRenderData, BOOL& bHandled );

    VOID    CheckLicense();
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUI_MAINMENUSCENE_H
