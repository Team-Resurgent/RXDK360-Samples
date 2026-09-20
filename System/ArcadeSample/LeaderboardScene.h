//--------------------------------------------------------------------------------------
// LeaderboardScene.h
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUI_LEADERBOARDSCENE_H
#define ARCADESAMPLE_XUI_LEADERBOARDSCENE_H

#include "AppXuiScene.h"
#include "Leaderboards.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Leaderboard list class.
//--------------------------------------------------------------------------------------
class CLeaderboardList : public CXuiListImpl
{
public:
    // Define the class. The class name must match the ClassOverride property
    // set for the list in the UI Authoring tool.
        XUI_IMPLEMENT_CLASS( CLeaderboardList, L"CLeaderboardList", XUI_CLASS_LIST )

protected:
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
// Leaderboard scene class.
//--------------------------------------------------------------------------------------
class CLeaderboardScene : public AppXuiScene
{
public:
    // Ctor
            CLeaderboardScene() : AppXuiScene( LIVE_SIGNIN_REQUIRED )
            {
            }

public:
    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
        XUI_IMPLEMENT_CLASS( CLeaderboardScene, L"CLeaderboardScene", XUI_CLASS_SCENE )

protected:
    // Control and Element wrapper objects.
    CXuiControl m_btnGamercard;
    CXuiControl m_btnBack;
    CXuiControl m_btnMode;
    CXuiControl m_labMode;
    CXuiControl m_labStatus;
    CLeaderboardList* m_plstScores;

    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled );
    HRESULT OnRender( XUIMessageRender* pRenderData, BOOL& bHandled );

    VOID    UpdateStatus();

    XUSER_STATS_SPEC m_StatsSpec;
    Leaderboards::StatsFilter m_StatsFilter;
    BOOL m_bRefreshing;
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUI_LEADERBOARDSCENE_H
