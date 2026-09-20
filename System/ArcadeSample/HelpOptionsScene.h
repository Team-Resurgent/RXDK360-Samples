//--------------------------------------------------------------------------------------
// HelpOptionsScene.h
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUI_HELPOPTIONSSCENE_H
#define ARCADESAMPLE_XUI_HELPOPTIONSSCENE_H

#include "AppXuiScene.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Game type scene class.
//--------------------------------------------------------------------------------------
class CHelpOptionsScene : public AppXuiScene
{
public:
    // Ctor
            CHelpOptionsScene() : AppXuiScene( NO_SIGNIN_REQUIRED )
            {
            }

public:
    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
        XUI_IMPLEMENT_CLASS( CHelpOptionsScene, L"CHelpOptionsScene", XUI_CLASS_SCENE )

protected:
    // Control and Element wrapper objects.
    CXuiControl m_btnBack;

    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled );
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUI_HELPOPTIONSSCENE_H
