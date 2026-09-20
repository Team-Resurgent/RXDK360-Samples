//--------------------------------------------------------------------------------------
// GameStartScene.h
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUI_GAMESTARTSCENE_H
#define ARCADESAMPLE_XUI_GAMESTARTSCENE_H

#include "AppXuiScene.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Game start scene class.
//--------------------------------------------------------------------------------------
class CGameStartScene : public QNetSessionScene
{
public:
    // Ctor
            CGameStartScene() : QNetSessionScene()
            {
            }

public:
     virtual VOID DisplayPlayScene();

    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
        XUI_IMPLEMENT_CLASS( CGameStartScene, L"CGameStartScene", XUI_CLASS_SCENE )

    VOID    QNetStateChanged( QNET_STATE OldState, QNET_STATE NewState, HRESULT hrInfo );

protected:
    HRESULT OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled );
    HRESULT OnMsgReturn( XUIMessageMessageBoxReturn* pXUIMessageMessageBoxReturn, BOOL& bHandled );
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUI_GAMESTARTSCENE_H
