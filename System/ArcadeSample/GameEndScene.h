//--------------------------------------------------------------------------------------
// GameEndScene.h
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUI_GAMEENDSCENE_H
#define ARCADESAMPLE_XUI_GAMEENDSCENE_H

#include "AppXuiScene.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Game end list class.
//--------------------------------------------------------------------------------------
class CGameEndList : public CXuiListImpl
{
public:
    // Define the class. The class name must match the ClassOverride property
    // set for the list in the UI Authoring tool.
        XUI_IMPLEMENT_CLASS( CGameEndList, L"CGameEndList", XUI_CLASS_LIST )

protected:
    // eText enumeration
    enum eText
    {
        eText_GamerTag  = 0,
        eText_Score     = 1,
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
// Game end scene class.
//--------------------------------------------------------------------------------------
class CGameEndScene : public QNetSessionScene
{
public:
    // Ctor
            CGameEndScene() : QNetSessionScene()
            {
            }

public:
    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
        XUI_IMPLEMENT_CLASS( CGameEndScene, L"CGameEndScene", XUI_CLASS_SCENE )

    VOID    QNetStateChanged( QNET_STATE OldState, QNET_STATE NewState, HRESULT hrInfo );

protected:
    // Control and Element wrapper objects.
    CXuiControl m_btnDone;
    CXuiControl m_btnGamercard;
    CXuiControl m_btnPlayerReview;
    CXuiControl m_ctlStatus;
    CXuiControl m_labStatus;
    CXuiControl m_ctlWait;
    CXuiControl m_labGamertag;
    CXuiControl m_labScore;
    CGameEndList* m_plstFinalScores;

    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled );
    HRESULT OnNavReturn( HXUIOBJ hSceneFrom, BOOL& bHandled );
    HRESULT OnMsgReturn( XUIMessageMessageBoxReturn* pXUIMessageMessageBoxReturn, BOOL& bHandled );

    VOID    ShowFinalScores( bool fShow );
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUI_GAMEENDSCENE_H
