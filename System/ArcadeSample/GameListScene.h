//--------------------------------------------------------------------------------------
// GameListScene.h
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUI_GAMELISTSCENE_H
#define ARCADESAMPLE_XUI_GAMELISTSCENE_H

#include "AppXuiScene.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Game list class.
//--------------------------------------------------------------------------------------
class CGameList : public CXuiListImpl
{
public:
    // Define the class. The class name must match the ClassOverride property
    // set for the list in the UI Authoring tool.
        XUI_IMPLEMENT_CLASS( CGameList, L"CGameList", XUI_CLASS_LIST )

protected:
    // eText enumeration
    enum eText
    {
        eText_Slots = 0,
        eText_Host  = 1,
        eText_QoS   = 2,
    };

    // eImg enumeration
    enum eImg
    {
        eImg_GamerPic   = 1,
        eImg_QoS        = 2,
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
// Game list scene class.
//--------------------------------------------------------------------------------------
class CGameListScene : public QNetSessionScene
{
public:
    // Ctor
            CGameListScene() : QNetSessionScene()
            {
            }

public:
    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
        XUI_IMPLEMENT_CLASS( CGameListScene, L"CGameListScene", XUI_CLASS_SCENE )

    VOID    QNetGameSearchComplete( IQNetGameSearch* pGameSearch, HRESULT hrComplete, DWORD dwNumResults );

protected:
    // Control and Element wrapper objects.    
    CXuiControl m_labStatus;
    CXuiControl m_btnBack;
    CXuiControl m_btnRefresh;
    CXuiControl m_btnCreateGame;
    CGameList* m_plstGames;

    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled );
    HRESULT OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled );
    HRESULT OnMsgReturn( XUIMessageMessageBoxReturn* pXUIMessageMessageBoxReturn, BOOL& bHandled );
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUI_GAMELISTSCENE_H
