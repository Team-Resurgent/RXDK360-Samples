//--------------------------------------------------------------------------------------
// GameLobbyScene.h
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUI_GAMELOBBYSCENE_H
#define ARCADESAMPLE_XUI_GAMELOBBYSCENE_H

#include "AppXuiScene.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Game lobby list class.
//--------------------------------------------------------------------------------------
class CGameLobbyList : public CXuiListImpl
{
public:
    // Define the class. The class name must match the ClassOverride property
    // set for the list in the UI Authoring tool.
        XUI_IMPLEMENT_CLASS( CGameLobbyList, L"CGameLobbyList", XUI_CLASS_LIST )

protected:
    // eText enumeration
    enum eText
    {
        eText_Gamertag  = 0,
        eText_QoS       = 1,
        eText_Voice     = 2,
        eText_Ready     = 3,
    };

    // eImg enumeration
    enum eImg
    {
        eImg_GamerPic   = 0,
        eImg_QoS        = 1,
        eImg_Voice      = 2,
        eImg_Ready      = 3,
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
// Game lobby scene class.
//--------------------------------------------------------------------------------------
class CGameLobbyScene : public QNetSessionScene
{
public:
    // Ctor
            CGameLobbyScene() : QNetSessionScene()
            {
            }

public:
    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
        XUI_IMPLEMENT_CLASS( CGameLobbyScene, L"CGameLobbyScene", XUI_CLASS_SCENE )

    VOID    QNetStateChanged( QNET_STATE OldState, QNET_STATE NewState, HRESULT hrInfo );
    VOID    QNetPlayerJoined( Player& pPlayer );
    VOID    QNetPlayerLeaving( Player& pPlayer );
    VOID    QNetReadinessChanged( Player& pPlayer, BOOL bReady );

protected:
    // Control and Element wrapper objects.
    CXuiControl m_labStatus;
    CXuiControl m_btnReady;
    CXuiControl m_btnBack;
    CXuiControl m_btnGamercard;
    CXuiControl m_btnFriends;
    CGameLobbyList* m_plstPlayers;

    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled );
    HRESULT OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled );

    VOID    UpdateStatus();
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUI_GAMELOBBYSCENE_H
