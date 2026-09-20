//--------------------------------------------------------------------------------------
// GamePlayScene.h
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUI_GAMEPLAYSCENE_H
#define ARCADESAMPLE_XUI_GAMEPLAYSCENE_H

#include "AppXuiScene.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Game play list class.
//--------------------------------------------------------------------------------------
class CGamePlayList : public CXuiListImpl
{
public:
    // Define the class. The class name must match the ClassOverride property
    // set for the list in the UI Authoring tool.
        XUI_IMPLEMENT_CLASS( CGamePlayList, L"CGamePlayList", XUI_CLASS_LIST )

protected:
    // eText enumeration
    enum eText
    {
        eText_Gamertag = 0,
    };

    // eImg enumeration
    enum eImg
    {
        eImg_GamerPic = 0,
    };

    // Message map.
        XUI_BEGIN_MSG_MAP()
            XUI_ON_XM_GET_ITEMCOUNT_ALL( OnGetItemCountAll )
            XUI_ON_XM_GET_SOURCE_TEXT( OnGetSourceText )
            XUI_ON_XM_GET_SOURCE_IMAGE( OnGetSourceImage )
            XUI_END_MSG_MAP();

    HRESULT OnGetItemCountAll( XUIMessageGetItemCount* pGetItemCountData, BOOL& bHandled );
    HRESULT OnGetSourceText( XUIMessageGetSourceText* pGetSourceText, BOOL& bHandled );
    HRESULT OnGetSourceImage( XUIMessageGetSourceImage* pGetSourceImage, BOOL& bHandled );
};

//--------------------------------------------------------------------------------------
// Game play scene class.
//--------------------------------------------------------------------------------------
class CGamePlayScene : public QNetSessionScene
{
public:
    // Ctor
                CGamePlayScene() : QNetSessionScene()
                {
                }

public:
    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
        XUI_IMPLEMENT_CLASS( CGamePlayScene, L"CGamePlayScene", XUI_CLASS_SCENE )

    VOID        QNetStateChanged( QNET_STATE OldState, QNET_STATE NewState, HRESULT hrInfo );

protected:
    // Control and Element wrapper objects.
    CXuiControl m_figBall;
    CXuiControl m_figBorder;
    CXuiControl m_figPaddles[ ARCADESAMPLE_MAX_PLAYERS ];
    CXuiControl m_labScores[ eTeam_Count ];
    CXuiControl m_labCountdown;
    CXuiControl m_btnPause;
    CXuiControl m_grpPause;
    CXuiControl m_btnResume;
    CXuiControl m_btnExit;

    HRESULT     OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT     OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled );
    HRESULT     OnRender( XUIMessageRender* pRenderData, BOOL& bHandled );
    HRESULT     OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled );
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUI_GAMEPLAYSCENE_H
