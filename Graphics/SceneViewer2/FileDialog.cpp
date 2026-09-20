//--------------------------------------------------------------------------------------
// FileDialog.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <assert.h>
#include <stdio.h>
#include <xtl.h>
#include <xbdm.h>
#include "FileDialog.h"
#include "SceneViewer2.h"

SceneFileEntryVector    g_SceneFileEntries;
SceneViewerXuiApp*      g_pXuiApp = NULL;


// Handler for the XM_INIT message.
HRESULT FileDialogScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    // Retrieve controls for later use.
    HRESULT hr;
    hr = GetChildById( L"UnloadButton", &m_UnloadButton );
    hr = GetChildById( L"LoadButton", &m_ReloadButton );
    hr = GetChildById( L"FileList", &m_FileListControl );
    return S_OK;
}

HRESULT FileDialogScene::OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled )
{
    if( hObjSource == m_ReloadButton )
    {
        bHandled = TRUE;
        g_pSceneViewerApp->ReloadScene();
        g_pXuiApp->HideFileDialog();
    }
    else if( hObjSource == m_UnloadButton )
    {
        bHandled = TRUE;
        g_pSceneViewerApp->UnloadScene();
        g_pXuiApp->HideFileDialog();
    }
    else if( hObjSource == m_FileListControl )
    {
        bHandled = TRUE;
        INT iSel = m_FileListControl.GetCurSel();
        assert( iSel >= 0 );
        if( ( DWORD )iSel > g_SceneFileEntries.size() )
            return S_OK;
        g_pSceneViewerApp->LoadSceneAsync( g_SceneFileEntries[ iSel ].strFileName );
        g_pXuiApp->HideFileDialog();
    }
    return S_OK;
}

VOID SceneFileList::RefreshFileList()
{
    g_SceneFileEntries.clear();
    WIN32_FIND_DATA FindData;
    HANDLE hFind;

    const CHAR* strSearchPaths[] =
    {
        "game:\\media\\scenes\\*.xatg",
        "devkit:\\previewpipeline\\scenes\\*.xatg"
    };

    for( DWORD i = 0; i < ARRAYSIZE( strSearchPaths ); i++ )
    {
        hFind = FindFirstFile( strSearchPaths[i], &FindData );
        BOOL bFound = ( hFind != INVALID_HANDLE_VALUE );
        while( bFound )
        {
            SceneFileEntry FileEntry;
            MultiByteToWideChar( CP_ACP,
                                 0,
                                 FindData.cFileName,
                                 strlen( FindData.cFileName ) + 1,
                                 FileEntry.strDisplayName,
                                 MAX_PATH );
            const CHAR* pEnd = strrchr( strSearchPaths[i], '\\' );
            assert( pEnd != NULL );
            strncpy_s( FileEntry.strFileName, strSearchPaths[i], pEnd - strSearchPaths[i] + 1 );
            strcat_s( FileEntry.strFileName, FindData.cFileName );
            g_SceneFileEntries.push_back( FileEntry );
            bFound = FindNextFile( hFind, &FindData );
        }
    }
}

HRESULT SceneFileList::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    RefreshFileList();
    InsertItems( 0, g_SceneFileEntries.size() );
    return S_OK;
}

HRESULT SceneFileList::OnGetSourceText( XUIMessageGetSourceText* pGetSourceTextData, BOOL& bHandled )
{
    bHandled = TRUE;
    pGetSourceTextData->szText = NULL;
    DWORD dwItem = ( DWORD )pGetSourceTextData->iItem;
    assert( dwItem < g_SceneFileEntries.size() );
    SceneFileEntry& FileEntry = g_SceneFileEntries[ dwItem ];
    pGetSourceTextData->szText = FileEntry.strDisplayName;
    return S_OK;
}

HRESULT SceneFileList::OnGetItemCountAll( XUIMessageGetItemCount* pGetItemCountData, BOOL& bHandled )
{
    bHandled = TRUE;
    pGetItemCountData->cItems = g_SceneFileEntries.size();
    return S_OK;
}

VOID SceneViewerXuiApp::Initialize( IDirect3DDevice9* pd3dDevice, D3DPRESENT_PARAMETERS* pd3dpp )
{
    DmMapDevkitDrive();
    g_pXuiApp = this;
    m_bActive = FALSE;
    HRESULT hr = InitShared( pd3dDevice, pd3dpp, XuiD3DXTextureLoader );

    m_MediaLocator.SetPackage( L"file://game:/media/SceneViewerXui.xzp" );

    WCHAR szResource[ ATG::LOCATOR_SIZE ];

    // Register a default typeface
    m_MediaLocator.ComposeResourceLocator( szResource, ARRAYSIZE( szResource ), L"xui/", L"xarialuni.ttf" );
    hr = RegisterDefaultTypeface( L"Arial Unicode MS", szResource );

    m_MediaLocator.ComposeResourceLocator( szResource, ARRAYSIZE( szResource ), L"xui/", L"sceneviewer_skin_default.xur" );
    hr = LoadSkin( szResource );
}

VOID SceneViewerXuiApp::Update( FLOAT fDeltaTime )
{
    XINPUT_KEYSTROKE keyStroke;
    // retrieve and dispatch input to the UI
    XInputGetKeystroke( XUSER_INDEX_ANY, XINPUT_FLAG_ANYDEVICE, &keyStroke );

    if( ( keyStroke.VirtualKey == VK_PAD_B || keyStroke.VirtualKey == VK_PAD_BACK ) &&
        ( keyStroke.Flags & XINPUT_KEYSTROKE_KEYDOWN ) )
    {
        if( m_bActive )
        {
            HideFileDialog();
        }
        return;
    }

    if( m_bActive )
    {
        XuiProcessInput( &keyStroke );
    }

    RunFrame();
}


// Register custom classes.
HRESULT SceneViewerXuiApp::RegisterXuiClasses()
{
    XuiSoundXACTRegister();
    SceneFileList::Register();
    FileDialogScene::Register();
    return S_OK;
}

// Unregister custom classes.
HRESULT SceneViewerXuiApp::UnregisterXuiClasses()
{
    FileDialogScene::Unregister();
    SceneFileList::Unregister();
    XuiSoundXACTUnregister();
    return S_OK;
}

VOID SceneViewerXuiApp::ShowFileDialog()
{
    if( m_bActive )
        return;

    WCHAR szResource[ ATG::LOCATOR_SIZE ];
    m_MediaLocator.ComposeResourceLocator( szResource, ARRAYSIZE( szResource ), L"xui/", NULL );

    HRESULT hr;
    hr = LoadFirstScene( szResource, L"SceneViewerFileDialog.xur", NULL, &m_hFileDialog );
    if( FAILED( hr ) )
        return;
    Resume();
    m_bActive = TRUE;
}

VOID SceneViewerXuiApp::HideFileDialog()
{
    XuiDestroyObject( m_hFileDialog );
    m_hFileDialog = NULL;
    m_bActive = FALSE;
}


