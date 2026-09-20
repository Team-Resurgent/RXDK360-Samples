//--------------------------------------------------------------------------------------
// FileDialog.h
//
// This class implements a simple XUI-based file dialog that is used to select and load
// a scene file.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>
#include <vector>

#include "AtgMediaLocator.h"

struct SceneFileEntry
{
    CHAR    strFileName[MAX_PATH];
    WCHAR   strDisplayName[MAX_PATH];
};
typedef std::vector <SceneFileEntry> SceneFileEntryVector;

class SceneFileList : public CXuiListImpl
{
public:
                    SceneFileList()
                    {
                    }

    XUI_IMPLEMENT_CLASS( SceneFileList, L"SceneFileList", XUI_CLASS_LIST );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_GET_SOURCE_TEXT( OnGetSourceText )
        XUI_ON_XM_GET_ITEMCOUNT_ALL( OnGetItemCountAll )
    XUI_END_MSG_MAP()

    virtual HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    virtual HRESULT OnGetSourceText( XUIMessageGetSourceText* pGetSourceTextData, BOOL& bHandled );
    virtual HRESULT OnGetItemCountAll( XUIMessageGetItemCount* pGetItemCountData, BOOL& bHandled );
protected:
    VOID            RefreshFileList();
};

// Main scene implementation class.
class FileDialogScene : public CXuiSceneImpl
{
public:
    XUI_IMPLEMENT_CLASS( FileDialogScene, L"FileDialog", XUI_CLASS_SCENE )

protected:
    // Control and Element wrapper objects.
    CXuiControl m_UnloadButton;
    CXuiControl m_ReloadButton;
    SceneFileList m_FileListControl;

    // Message map.
    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
    XUI_END_MSG_MAP()

    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled );
};

// declare the main application class
class SceneViewerXuiApp : public CXuiModule
{
public:
    VOID            Initialize( IDirect3DDevice9* pd3dDevice, D3DPRESENT_PARAMETERS* pd3dpp );
    VOID            Update( FLOAT fDeltaTime );
    BOOL            IsActive() const
    {
        return m_bActive;
    }
    VOID            ShowFileDialog();
    VOID            HideFileDialog();
protected:
    // Override RegisterXuiClasses so that CMyApp can register classes.
    virtual HRESULT RegisterXuiClasses();

    // Override UnregisterXuiClasses so that CMyApp can unregister classes. 
    virtual HRESULT UnregisterXuiClasses();

protected:
    BOOL m_bActive;
    HXUIOBJ m_hFileDialog;
    ATG::MediaLocator m_MediaLocator;
};

#endif
