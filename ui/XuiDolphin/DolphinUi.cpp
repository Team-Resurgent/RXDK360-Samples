//--------------------------------------------------------------------------------------
// DolphinUi.cpp
//
// XUI implementation of the sample
// 
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// To enable XUI auditioning, define XUIAPP_AUDITIONING
//--------------------------------------------------------------------------------------
#define XUIAPP_AUDITIONING

#include <xtl.h>
#include <xui.h>
#include <stdio.h>
#include <assert.h>
#include <winsockx.h>
#include <xonline.h>
#include <xuiapp.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgMediaLocator.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>

#include "DolphinUi.h"

//--------------------------------------------------------------------------------------
// Globals variables and definitions
//--------------------------------------------------------------------------------------

#define IS_PRESSED( pGamepad, mask )\
    ( ( pGamepad->wPressedButtons & mask ) )

BOOL            g_bUIActive;
HRESULT ApplySkin( LPCWSTR szSkinFullUrl );

static ATG::MediaLocator s_mediaLocator( L"file://game:/media/ui.xzp" );
static WCHAR s_szDefaultSkinPath[ ATG::LOCATOR_SIZE ];
static WCHAR s_szOrangeSkinPath[ ATG::LOCATOR_SIZE ];

//--------------------------------------------------------------------------------------
// Name: class MainScene
// Desc: Class used by the main menu
//--------------------------------------------------------------------------------------
class MainScene : public CXuiSceneImpl
{
public:
    XUI_IMPLEMENT_CLASS( MainScene, L"MainScene", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_KEYDOWN( OnKeyDown )
    XUI_END_MSG_MAP()

    HRESULT OnKeyDown( XUIMessageInput* pInputData, BOOL& bHandled );
};


//--------------------------------------------------------------------------------------
// Name: class OptionsScene
// Desc: Class used by the options menu
//--------------------------------------------------------------------------------------
class OptionsScene : public CXuiSceneImpl
{
protected:
    // cached handles to option checkboxes
    CXuiCheckbox m_chkFloor;
    CXuiCheckbox m_chkDolphin;
    CXuiCheckbox m_chkStats;
    CXuiCheckbox m_chkWireframe;

public:
    XUI_IMPLEMENT_CLASS( OptionsScene, L"OptionsScene", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
    XUI_END_MSG_MAP()

    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled );
};


//--------------------------------------------------------------------------------------
// Name: class SkinScene
// Desc: Class used by the skin menu
//--------------------------------------------------------------------------------------
class SkinScene : public CXuiSceneImpl
{
protected:
    // cached handles to controls of interest
    CXuiRadioGroup m_radioSkin;

public:
    XUI_IMPLEMENT_CLASS( SkinScene, L"SkinScene", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_SELCHANGED( OnNotifySelChanged )
    XUI_END_MSG_MAP()

    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT OnNotifySelChanged( HXUIOBJ hObjSource,
                                XUINotifySelChanged* pNotifySelChangedData, BOOL& bHandled );
};


//--------------------------------------------------------------------------------------
// Name: class DolphinViewerControl
// Desc: Class used by to render the dolphin viewer control
//       the control uses a render target to render the 3d dolphin into the 2d
//       UI of the application
//--------------------------------------------------------------------------------------
class DolphinViewerControl : public CXuiSceneImpl
{
protected:
    IDirect3DTexture9* m_pTex;              // texture to Resolve into
    IDirect3DSurface9* m_pRenderTarget;     // render target for off-screen rendering
    HXUISHAPE m_hShape;                     // XUI shape for rendering our texture
    DOLPHIN_STYLE m_nDolphinStyle;          // Dolphin style to render

public:
            DolphinViewerControl();
            ~DolphinViewerControl();
    void    SetDolphinStyle( DOLPHIN_STYLE nDolphinStyle );

    XUI_IMPLEMENT_CLASS( DolphinViewerControl, L"DolphinViewerControl", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_RENDER( OnRender )
    XUI_END_MSG_MAP()


    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT OnRender( XUIMessageRender* pRenderData, BOOL& bHandled );
};


//--------------------------------------------------------------------------------------
// Name: class DolphinStyleList
// Desc: Class used for the list in the Choose Dolphin Scene
//--------------------------------------------------------------------------------------
class DolphinStyleList : CXuiListImpl
{
protected:

public:
            DolphinStyleList()
            {
            };

    XUI_IMPLEMENT_CLASS( DolphinStyleList, L"DolphinStyleList", XUI_CLASS_LIST );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_GET_SOURCE_TEXT( OnGetSourceText )
        XUI_ON_XM_GET_ITEMCOUNT_ALL( OnGetItemCountAll )
    XUI_END_MSG_MAP()

    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        InsertItems( 0, DOLPHIN_STYLE_MAX );
        return S_OK;
    }

    HRESULT OnGetSourceText( XUIMessageGetSourceText* pGetSourceTextData, BOOL& bHandled )
    {
        bHandled = TRUE;
        pGetSourceTextData->szText = NULL;
        if( ( pGetSourceTextData->iItem >= 0 && pGetSourceTextData->iItem < DOLPHIN_STYLE_MAX )
            && pGetSourceTextData->iData == 0 )
        {
            IDolphinUI* pApp = GetApp();
            pGetSourceTextData->szText = pApp->GetDolphinStyleDesc(
                ( DOLPHIN_STYLE )pGetSourceTextData->iItem );
        }
        return S_OK;
    }

    HRESULT OnGetItemCountAll( XUIMessageGetItemCount* pGetItemCountData, BOOL& bHandled )
    {
        bHandled = TRUE;
        pGetItemCountData->cItems = DOLPHIN_STYLE_MAX;
        return S_OK;
    }
};


//--------------------------------------------------------------------------------------
// Name: class ChooseDolphinScene
// Desc: Class used for the Choose Dolphin Menu
//--------------------------------------------------------------------------------------
class ChooseDolphinScene : CXuiSceneImpl
{
protected:
    CXuiList m_dolphinList;
    CXuiElement m_viewerElement;
    CXuiControl m_applyButton;

public:
            ChooseDolphinScene()
            {
            };

    XUI_IMPLEMENT_CLASS( ChooseDolphinScene, L"ChooseDolphinScene", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
        XUI_ON_XM_NOTIFY_SELCHANGED( OnNotifySelChanged )
    XUI_END_MSG_MAP()

    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled );
    HRESULT OnNotifySelChanged( HXUIOBJ hObjSource,
                                XUINotifySelChanged* pNotifySelChangedData, BOOL& bHandled );

    void    UpdateDolphinViewer();
};


//--------------------------------------------------------------------------------------
// Name: class SigninScene
// Desc: Class used by the Signin Menu
//--------------------------------------------------------------------------------------
class SigninScene : public CXuiSceneImpl
{
protected:
    CXuiControl m_btnOnePlayer;
    CXuiControl m_btnTwoPlayer;
    CXuiControl m_btnFourPlayer;

public:
    XUI_IMPLEMENT_CLASS( SigninScene, L"SigninScene", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
    XUI_END_MSG_MAP()

    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
    HRESULT OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled );
};


class CDolphinUIApp : public CXuiModule
{
public:
                    CDolphinUIApp()
                    {
                        m_fLiveInit = FALSE;
                        m_hNotification = NULL;
                        m_fSigninRunning = FALSE;
                    }
                    ~CDolphinUIApp()
                    {
                        LiveUninit();
                    }

    HRESULT         LiveInit();
    void            LiveUninit();
    DWORD           Signin( DWORD dwPlayers );
    virtual void    RunFrame();
    HRESULT         DestroyMainScene();

protected:
    XNetStartupParams m_xnsp;
    WSADATA m_wsadata;
    XNADDR m_xna;
    HANDLE m_hWorkEvent;
    BOOL m_fLiveInit;
    HANDLE m_hNotification;
    BOOL m_fSigninRunning;
    HRESULT         RegisterXuiClasses()
    {
        XuiVideoRegister();
        MainScene::Register();
        OptionsScene::Register();
        SkinScene::Register();
        DolphinStyleList::Register();
        ChooseDolphinScene::Register();
        DolphinViewerControl::Register();
        SigninScene::Register();
        XuiSoundXACTRegister();
        return S_OK;
    }

    HRESULT         UnregisterXuiClasses()
    {
        XuiVideoUnregister();
        MainScene::Unregister();
        OptionsScene::Unregister();
        SkinScene::Unregister();
        DolphinStyleList::Unregister();
        ChooseDolphinScene::Unregister();
        DolphinViewerControl::Unregister();
        SigninScene::Unregister();
        XuiSoundXACTUnregister();
        return S_OK;
    }

    // NOTE: currently hardcoded to create a 640x480 canvas
    HRESULT         CreateMainCanvas()
    {
        ASSERT( m_bXuiInited );
        if( !m_bXuiInited )
            return E_UNEXPECTED;

        ASSERT( m_hObjRoot == NULL );
        if( m_hObjRoot )
            return E_UNEXPECTED;


        HRESULT hr = XuiCreateObject( L"XuiCanvas", &m_hObjRoot );
        if( FAILED( hr ) )
            return hr;

        hr = XuiElementSetBounds( m_hObjRoot, ( float )640, ( float )480 );
        if( FAILED( hr ) )
            return hr;
        return S_OK;
    }
};


//--------------------------------------------------------------------------------------
// Global instance of the UI class
//--------------------------------------------------------------------------------------
CDolphinUIApp   g_uiApp;


//--------------------------------------------------------------------------------------
// Name: ShowMainMenu
// Desc: Displays the main menu
//--------------------------------------------------------------------------------------
HRESULT ShowMainMenu()
{
    WCHAR szRessourePath[ ATG::LOCATOR_SIZE ];

    s_mediaLocator.ComposeResourceLocator( szRessourePath, ARRAYSIZE( szRessourePath ), L"xui/", NULL );
    HRESULT hr = g_uiApp.LoadFirstScene( szRessourePath, L"dolphin_menu.xur" );
    if( FAILED( hr ) )
        return hr;

    g_bUIActive = TRUE;
    g_uiApp.Resume();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: HideMenu
// Desc: Hides the main menu
//--------------------------------------------------------------------------------------
void HideMenu()
{
    g_bUIActive = FALSE;
    g_uiApp.DestroyMainScene();
}


//--------------------------------------------------------------------------------------
// Name: UpdateUI
// Desc: Update the UI
//--------------------------------------------------------------------------------------
void UpdateUI()
{
    if( g_bUIActive )
        g_uiApp.RunFrame();
}


//--------------------------------------------------------------------------------------
// Name: RenderUI
// Desc: Render the UI if the UI is active, otherwise it just returns
//--------------------------------------------------------------------------------------
HRESULT RenderUI( IDirect3DDevice9* pDevice, UINT uWidth, UINT uHeight )
{
    if( !g_bUIActive )
        return S_OK;

    XuiTimersRun();

    XuiRenderBegin( g_uiApp.GetDC(), D3DCOLOR_ARGB( 255, 0, 0, 0 ) );

    D3DXMATRIX matOrigView;
    XuiRenderGetViewTransform( g_uiApp.GetDC(), &matOrigView );

    // scale depending on the width of the render target
    D3DXMATRIX matView;
    D3DXMatrixScaling( &matView, uWidth / 640.0f, uHeight / 480.0f, 1 );
    XuiRenderSetViewTransform( g_uiApp.GetDC(), &matView );

    XUIMessage msg;
    XUIMessageRender msgRender;
    XuiMessageRender( &msg, &msgRender, g_uiApp.GetDC(), 0xffffffff, XUI_BLEND_NORMAL );
    XuiSendMessage( g_uiApp.GetRootObj(), &msg );

    XuiRenderSetViewTransform( g_uiApp.GetDC(), &matOrigView );

    XuiRenderEnd( g_uiApp.GetDC() );

    GetApp()->GetD3DDevice()->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: MainScene::OnKeyDown
// Desc: Message handler for XM_KEYDOWN
//--------------------------------------------------------------------------------------
HRESULT MainScene::OnKeyDown( XUIMessageInput* pInputData, BOOL& bHandled )
{
    switch( pInputData->dwKeyCode )
    {
        case VK_PAD_B:
        {
            bHandled = TRUE;
            if( !( pInputData->dwFlags & XUI_INPUT_FLAG_REPEAT ) )
                HideMenu();
            break;
        }
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: OptionsScene::OnInit
// Desc: Message handler for the options scene XM_INIT
//--------------------------------------------------------------------------------------
HRESULT OptionsScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    GetChildById( L"chkFloor", &m_chkFloor );
    GetChildById( L"chkDolphin", &m_chkDolphin );
    GetChildById( L"chkStats", &m_chkStats );
    GetChildById( L"chkWireframe", &m_chkWireframe );

    DWORD dwRenderOptions = GetApp()->GetRenderOptions();
    m_chkFloor.SetCheck( !!( dwRenderOptions & DOLPHIN_RENDER_FLOOR ) );
    m_chkDolphin.SetCheck( !!( dwRenderOptions & DOLPHIN_RENDER_DOLPHIN ) );
    m_chkStats.SetCheck( !!( dwRenderOptions & DOLPHIN_RENDER_STATS ) );
    m_chkWireframe.SetCheck( !!( dwRenderOptions & DOLPHIN_RENDER_WIREFRAME ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: OptionsScene::OnNotifyPress
// Desc: Message handler for the options scene XM_NOTIFY XN_PRESS
//--------------------------------------------------------------------------------------
HRESULT OptionsScene::OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled )
{
    DWORD dwRenderOptions = GetApp()->GetRenderOptions();

    if( hObjSource == m_chkFloor )
        dwRenderOptions ^= DOLPHIN_RENDER_FLOOR;
    else if( hObjSource == m_chkDolphin )
        dwRenderOptions ^= DOLPHIN_RENDER_DOLPHIN;
    else if( hObjSource == m_chkStats )
        dwRenderOptions ^= DOLPHIN_RENDER_STATS;
    else if( hObjSource == m_chkWireframe )
        dwRenderOptions ^= DOLPHIN_RENDER_WIREFRAME;
    else
    {
        // something we don't know about, let any XUI base classes
        // deal with it, by not setting bHandled to TRUE
        return S_OK;
    }
    GetApp()->SetRenderOptions( dwRenderOptions );
    bHandled = TRUE;
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SkinScene::OnInit
// Desc: Message handler for XM_INIT
//--------------------------------------------------------------------------------------
static int   s_iSkin = 0;

HRESULT SkinScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    GetChildById( L"radioSkin", &m_radioSkin );
    if( m_radioSkin )
        m_radioSkin.SetCurSel( s_iSkin );
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: SkinScene::OnInit
// Desc: Message handler for XM_NOTIFY XN_SELCHANGED
//--------------------------------------------------------------------------------------
HRESULT SkinScene::OnNotifySelChanged( HXUIOBJ hObjSource,
                                       XUINotifySelChanged* pNotifySelChangedData, BOOL& bHandled )
{
    if( hObjSource == m_radioSkin )
    {
        s_iSkin = m_radioSkin.GetCurSel();
        LPCWSTR szSkinPath = s_szDefaultSkinPath;
        if( s_iSkin == 1 )
            szSkinPath = s_szOrangeSkinPath;
        ApplySkin( szSkinPath );
        bHandled = TRUE;
        return S_OK;
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DolphinViewerControl::DolphinViewerControl
// Desc: Constructor for the DolphinViewControl class
//--------------------------------------------------------------------------------------
DolphinViewerControl::DolphinViewerControl()
{
    m_pTex = NULL;
    m_pRenderTarget = NULL;
    m_hShape = NULL;
    m_nDolphinStyle = DOLPHIN_STYLE_STANDARD;
}


//--------------------------------------------------------------------------------------
// Name: DolphinViewerControl::~DolphinViewerControl
// Desc: Destructor for the DolphinViewControl class
//--------------------------------------------------------------------------------------
DolphinViewerControl::~DolphinViewerControl()
{
    if( m_pTex )
        m_pTex->Release();
    m_pTex = NULL;
    if( m_pRenderTarget )
        m_pRenderTarget->Release();
    m_pRenderTarget = NULL;
    if( m_hShape )
        XuiRenderDestroyShape( m_hShape );
    m_hShape = NULL;
}


//--------------------------------------------------------------------------------------
// Name: DolphinViewerControl::SetDolphinStyle
// Desc: Called by the SkinScene object to update the dolphin style displayed in the
//       control
//--------------------------------------------------------------------------------------
void DolphinViewerControl::SetDolphinStyle( DOLPHIN_STYLE nDolphinStyle )
{
    m_nDolphinStyle = nDolphinStyle;
}


//--------------------------------------------------------------------------------------
// Name: DolphinViewerControl::OnInit
// Desc: Message handler for the XM_INIT message
//--------------------------------------------------------------------------------------
HRESULT DolphinViewerControl::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    IDolphinUI* pApp = GetApp();
    IDirect3DDevice9* pDevice = pApp->GetD3DDevice();
    pDevice->CreateTexture( 256, 256, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_pTex, NULL );
    pDevice->CreateRenderTarget( 256, 256, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE,
                                 0, 0, &m_pRenderTarget, NULL );

    float fWidth, fHeight;
    XuiElementGetBounds( m_hObj, &fWidth, &fHeight );

    D3DXVECTOR2 verts[] =
    {
        D3DXVECTOR2( 0, 0 ),
        D3DXVECTOR2( 0, 0 ),
        D3DXVECTOR2( fWidth, 0 ),
        D3DXVECTOR2( fWidth, 0 ),

        D3DXVECTOR2( fWidth, 0 ),
        D3DXVECTOR2( fWidth, fHeight ),
        D3DXVECTOR2( fWidth, fHeight ),

        D3DXVECTOR2( fWidth, fHeight ),
        D3DXVECTOR2( 0, fHeight ),
        D3DXVECTOR2( 0, fHeight ),

        D3DXVECTOR2( 0, fHeight ),
        D3DXVECTOR2( 0, 0 ),
        D3DXVECTOR2( 0, 0 ),
    };
    XuiRenderCreateShape( sizeof( verts ) / sizeof( verts[ 0 ] ), verts, FALSE, &m_hShape );
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DolphinViewerControl::OnRender
// Desc: Message handler for the XM_RENDER message
//--------------------------------------------------------------------------------------
HRESULT DolphinViewerControl::OnRender( XUIMessageRender* pRenderData, BOOL& bHandled )
{
    bHandled = TRUE;
    if( !m_pTex )
        return S_OK;
    HXUIDC hDC = pRenderData->hDC;

    IDirect3DDevice9* pDevice;
    XuiRenderGetDevice( hDC, &pDevice );
    if( !pDevice )
        return S_OK;

    IDirect3DSurface9* pOrigRenderTarget = NULL;
    pDevice->GetRenderTarget( 0, &pOrigRenderTarget );
    pDevice->SetRenderTarget( 0, m_pRenderTarget );

    // disable writes to the alpha channel
    pDevice->SetRenderState( D3DRS_COLORWRITEENABLE,
                             D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE );
    GetApp()->RenderDolphinStyle( m_nDolphinStyle, pDevice );
    // re-enable writes to all channels
    pDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );

    pDevice->Resolve( 0, NULL, m_pTex, NULL, 0, 0, NULL, 0, 0, NULL );
    pDevice->SetRenderTarget( 0, pOrigRenderTarget );
    pOrigRenderTarget->Release();

    XuiRenderRestoreState( hDC );

    HXUIBRUSH hBrush;
    XuiAttachTextureBrush( m_pTex, &hBrush );
    D3DXMATRIX matBrushXForm;
    D3DXMatrixScaling( &matBrushXForm, 1 / 256.0f, 1 / 256.0f, 1 );
    XuiBrushSetXForm( hBrush, &matBrushXForm );
    XuiSelectBrush( hDC, hBrush );

    D3DXMATRIX matXForm;
    GetFullXForm( &matXForm );

    XuiRenderSetTransform( hDC, &matXForm );
    XuiSetColorFactor( hDC, pRenderData->dwColorFactor );
    XuiSetBlendMode( hDC, pRenderData->nBlendMode );

    XuiDrawShape( hDC, m_hShape );

    XuiSelectBrush( hDC, NULL );
    XuiDestroyBrush( hBrush );

    pDevice->Release();
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ChooseDolphinScene::OnInit
// Desc: Message handler for XM_INIT
//--------------------------------------------------------------------------------------
HRESULT ChooseDolphinScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    HRESULT hr = GetChildById( L"dolphinChooser", &m_dolphinList );
    if( FAILED( hr ) )
        return hr;
    GetChildById( L"dolphinViewer", &m_viewerElement );
    GetChildById( L"applyButton", &m_applyButton );

    m_dolphinList.SetCurSelVisible( GetApp()->GetDolphinStyle() );
    UpdateDolphinViewer();
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: ChooseDolphinScene::OnInit
// Desc: Message handler for XM_NOTIFY XN_PRESS
//--------------------------------------------------------------------------------------
HRESULT ChooseDolphinScene::OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled )
{
    if( hObjSource == m_dolphinList )
    {
        m_applyButton.Press();
        GetApp()->SetDolphinStyle( ( DOLPHIN_STYLE )m_dolphinList.GetCurSel() );
        bHandled = TRUE;
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ChooseDolphinScene::OnNotifySelChanged
// Desc: Message handler for selection changed notification
//--------------------------------------------------------------------------------------
HRESULT ChooseDolphinScene::OnNotifySelChanged( HXUIOBJ hObjSource,
                                                XUINotifySelChanged* pNotifySelChangedData, BOOL& bHandled )
{
    if( hObjSource == m_dolphinList )
    {
        bHandled = TRUE;
        UpdateDolphinViewer();
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ChooseDolphinScene::UpdateDolphinViewer
// Desc: Updates the state of the controls in the ChooseDolphinScene based
//       on the currently selected dolphin style
//--------------------------------------------------------------------------------------
void ChooseDolphinScene::UpdateDolphinViewer()
{
    DolphinViewerControl* pViewer = NULL;
    if( m_viewerElement.m_hObj != NULL )
        XuiObjectFromHandle( m_viewerElement, ( void** )&pViewer );

    if( pViewer != NULL )
        pViewer->SetDolphinStyle( ( DOLPHIN_STYLE )m_dolphinList.GetCurSel() );
}


//--------------------------------------------------------------------------------------
// Name: SigninScene::OnInit
// Desc: Message handler for XM_INIT
//--------------------------------------------------------------------------------------
HRESULT SigninScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    GetChildById( L"btnOne", &m_btnOnePlayer );
    GetChildById( L"btnTwo", &m_btnTwoPlayer );
    GetChildById( L"btnFour", &m_btnFourPlayer );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SigninScene::OnNotifyPress
// Desc: Message handler for XM_NOTIFY / XN_PRESS
//--------------------------------------------------------------------------------------
HRESULT SigninScene::OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled )
{
    DWORD dwPlayers = 0xFFFFFFFF;

    if( hObjSource == m_btnOnePlayer )
    {
        dwPlayers = 1;
    }
    else if( hObjSource == m_btnTwoPlayer )
    {
        dwPlayers = 2;
    }
    else if( hObjSource == m_btnFourPlayer )
    {
        dwPlayers = 4;
    }

    // If one of the player selection count buttons was pressed then launch
    // the signin UI
    if( 0xFFFFFFFF != dwPlayers )
    {
        g_uiApp.Signin( dwPlayers );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CDolphinUIApp::LiveInit
// Desc: Initializes connection to Live if it hasn't already been done
//--------------------------------------------------------------------------------------
HRESULT CDolphinUIApp::LiveInit()
{
    HRESULT hr = E_FAIL;
    DWORD dwStatus = 0;

    // Just return if the live connection has already been setup
    if( m_fLiveInit )
    {
        return S_OK;
    }

    ZeroMemory( &m_xnsp, sizeof( m_xnsp ) );
    m_xnsp.cfgSizeOfStruct = sizeof( m_xnsp );
    INT err = XNetStartup( &m_xnsp );
    if( err )
    {
        return hr;
    }

    err = WSAStartup( MAKEWORD( 2, 0 ), &m_wsadata );
    if( err )
    {
        XNetCleanup();
        return hr;
    }

    m_hWorkEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    if( m_hWorkEvent == NULL )
    {
        WSACleanup();
        XNetCleanup();
        return HRESULT_FROM_WIN32( GetLastError() );
    }

    do
    {
        dwStatus = XNetGetTitleXnAddr( &m_xna );
    } while( dwStatus == XNET_GET_XNADDR_PENDING );

    if( dwStatus & XNET_GET_XNADDR_TROUBLESHOOT )
    {
        CloseHandle( m_hWorkEvent );
        WSACleanup();
        XNetCleanup();
        return E_FAIL;
    }

    m_fLiveInit = TRUE;
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CDolphinUIApp::LiveUninit
// Desc: Cleans up the connection to Live if it hasn't already been done
//--------------------------------------------------------------------------------------
void CDolphinUIApp::LiveUninit()
{
    if( m_fLiveInit )
    {
        CloseHandle( m_hWorkEvent );

        WSACleanup();

        XNetCleanup();

        m_fLiveInit = FALSE;
    }
}


//--------------------------------------------------------------------------------------
// Name: CDolphinUIApp::Signin
// Desc: Pulls up the Signin UI if it isn't already displayed
//--------------------------------------------------------------------------------------
DWORD CDolphinUIApp::Signin( DWORD dwPlayers )
{
    HRESULT hr = E_FAIL;
    DWORD dwRes = 0xFFFFFFFF;

    if( m_fSigninRunning )
    {
        return 0;
    }

    hr = LiveInit();
    if( FAILED( hr ) )
    {
        return ( DWORD )hr;
    }

    if( m_hNotification == NULL )
    {
        m_hNotification = XNotifyCreateListener( XNOTIFY_SYSTEM );
        if( m_hNotification == NULL || m_hNotification == INVALID_HANDLE_VALUE )
        {
            return ( DWORD )hr;
        }
    }

    dwRes = XShowSigninUI( dwPlayers, 0 );
    m_fSigninRunning = TRUE;

    return dwRes;
}


//--------------------------------------------------------------------------------------
// Name: CDolphinUIApp::RunFrame
// Desc: Override of the CXuiModule RunFrame() method that checks for Signin UI
// completion
//--------------------------------------------------------------------------------------
void CDolphinUIApp::RunFrame()
{
    DWORD dwNotificationId;
    ULONG_PTR ulParam;
    if( m_hNotification != NULL && XNotifyGetNext( m_hNotification, 0, &dwNotificationId, &ulParam ) )
    {
        if( dwNotificationId == XN_SYS_UI )
        {
            m_fSigninRunning = static_cast<BOOL>( ulParam );
        }
    }
    CXuiModule::RunFrame();
}


//--------------------------------------------------------------------------------------
// Name: CDolphinUIApp::DestroyMainScene
// Desc: Destroys the main menu scene.
//--------------------------------------------------------------------------------------
HRESULT CDolphinUIApp::DestroyMainScene()
{
    if( m_hObjRoot )
    {
        CXuiElement canvas;
        CXuiElement element;

        canvas.Attach( m_hObjRoot );

        //
        // Destroy all UI elements under the canvas
        //
        canvas.GetFirstChild( &element );
        while( element.m_hObj != NULL )
        {
            if( element.m_hObj != NULL )
            {
                element.Destroy();
            }
            canvas.GetFirstChild( &element );
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitUI
// Desc: Initializes the UI runtime for the specified CAtgApplication
//       Note that the function also loads the default skin and navigates to the main
//       menu (without displaying it)
//--------------------------------------------------------------------------------------
HRESULT InitUI( ATG::Application* pApp )
{
    HRESULT hr = g_uiApp.InitShared( pApp->m_pd3dDevice, &pApp->m_d3dpp, XuiD3DXTextureLoader );
    if( FAILED( hr ) )
        return hr;

    s_mediaLocator.ComposeResourceLocator(  s_szDefaultSkinPath, ARRAYSIZE( s_szDefaultSkinPath ), L"xui\\", L"skin_default.xur" );

    s_mediaLocator.ComposeResourceLocator( s_szOrangeSkinPath, ARRAYSIZE( s_szOrangeSkinPath ), L"xui\\", L"skin_orange.xur" );

    // Register a default typeface
    hr = g_uiApp.RegisterDefaultTypeface( L"Arial Unicode MS", L"file://game:/media/xarialuni.ttf" );
    if( FAILED( hr ) )
        return hr;

    hr = g_uiApp.LoadSkin( s_szDefaultSkinPath );
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: ApplySkin
// Desc: Apply new skin
//--------------------------------------------------------------------------------------
HRESULT ApplySkin( LPCWSTR szSkinFullUrl )
{
    XuiFreeVisuals( NULL );
    HRESULT hr = XuiLoadVisualFromBinary( szSkinFullUrl, NULL );
    if( FAILED( hr ) )
        return hr;
    return XuiElementSkinChanged( g_uiApp.GetRootObj() );
}


//--------------------------------------------------------------------------------------
// Name: DispatchXuiInput
// Desc: Intercepts the Start button for the main menu, passes input into XUI.
//--------------------------------------------------------------------------------------
void DispatchXuiInput( XINPUT_KEYSTROKE* pKeystroke )
{
    if( ( pKeystroke->VirtualKey == VK_PAD_START ) &&
        ( pKeystroke->Flags & XINPUT_KEYSTROKE_KEYDOWN ) )
    {
        if( g_bUIActive == FALSE )
        {
            // bring up the UI
            ShowMainMenu();
        }
        else
        {
            HideMenu();
        }
        return;
    }
    if( g_bUIActive )
    {
        XuiProcessInput( pKeystroke );
    }
}
