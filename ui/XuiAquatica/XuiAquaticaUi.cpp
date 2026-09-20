//--------------------------------------------------------------------------------------
// XuiAquaticaUi.cpp
//
// XUI implementation of the sample
// 
// Game Technology Group.
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
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>

#include "XuiAquaticaUi.h"

//--------------------------------------------------------------------------------------
// Globals variables and definitions
//--------------------------------------------------------------------------------------

#define IS_PRESSED( pGamepad, mask )\
    ( ( pGamepad->wPressedButtons & mask ) )

BOOL            g_bUIActive;
BOOL            g_bSoundMuted;

const WCHAR*    g_strLanguages[] =
{
    L"ENG", L"ESP", L"JPN"
};
DWORD           g_dwLanguageSelection;

HXUIOBJ         g_hRootObj;

//--------------------------------------------------------------------------------------
// Name: class MainMenu
// Desc: Class used by the main menu
//--------------------------------------------------------------------------------------
class MainMenu : public CXuiSceneImpl
{
public:
    XUI_IMPLEMENT_CLASS( MainMenu, L"MainMenu", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_KEYDOWN( OnKeyDown )
    XUI_END_MSG_MAP()


    //--------------------------------------------------------------------------------------
    // Name: OnInit
    // Desc: Message handler for XM_INIT
    //--------------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        // Set root object for easier access later
        g_hRootObj = m_hObj;

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    // Name: OnKeyDown
    // Desc: Message handler for XM_KEYDOWN
    //--------------------------------------------------------------------------------------
    HRESULT OnKeyDown( XUIMessageInput* pInputData, BOOL& bHandled )
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
};

//--------------------------------------------------------------------------------------
// Name: class PauseMenu
// Desc: Class used by the pause menu
//--------------------------------------------------------------------------------------
class PauseMenu : public CXuiSceneImpl
{
private:
    CXuiControl m_hResumeButton;
public:
    XUI_IMPLEMENT_CLASS( PauseMenu, L"PauseMenu", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_KEYDOWN( OnKeyDown )
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
    XUI_END_MSG_MAP()


    //--------------------------------------------------------------------------------------
    // Name: OnInit
    // Desc: Message handler for XM_INIT
    //--------------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        HRESULT hr = GetChildById( L"XuiNavButton1", &m_hResumeButton );
        if( FAILED( hr ) )
            return hr;

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    // Name: OnKeyDown
    // Desc: Message handler for XM_KEYDOWN
    //--------------------------------------------------------------------------------------
    HRESULT OnKeyDown( XUIMessageInput* pInputData, BOOL& bHandled )
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
    // Name: OnNotifyPress
    // Desc: Message handler for XM_NOTIFY_PRESS
    //--------------------------------------------------------------------------------------
    HRESULT OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled )
    {
        if( hObjSource == m_hResumeButton )
        {
            bHandled = TRUE;
            HideMenu();
        }

        return S_OK;
    }
};


//--------------------------------------------------------------------------------------
// Name: class ControllerOptionsMenu
// Desc: Class used by the controller options menu
//--------------------------------------------------------------------------------------
class ControllerOptionsMenu : public CXuiSceneImpl
{
private:
    CXuiSlider m_hSensitivitySlider;
    CXuiCheckbox m_hInvertedCheckbox;
public:
    XUI_IMPLEMENT_CLASS( ControllerOptionsMenu, L"ControllerOptionsMenu", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
        XUI_ON_XM_NOTIFY_VALUE_CHANGED( OnNotifyValueChanged )
    XUI_END_MSG_MAP()


    //--------------------------------------------------------------------------------------
    // Name: OnInit
    // Desc: Message handler for XM_INIT
    //--------------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        HRESULT hr = GetChildById( L"XuiSlider1", &m_hSensitivitySlider );
        if( FAILED( hr ) )
            return hr;

        hr = GetChildById( L"XuiCheckbox1", &m_hInvertedCheckbox );
        if( FAILED( hr ) )
            return hr;

        m_hSensitivitySlider.SetValue( ( int )GetApp()->GetControllerSensitivity() );
        m_hInvertedCheckbox.SetCheck( GetApp()->GetControllerInversion() );

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    // Name: OnNotifyPress
    // Desc: Message handler for XM_NOTIFY_PRESS
    //--------------------------------------------------------------------------------------
    HRESULT OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled )
    {
        if( hObjSource == m_hInvertedCheckbox )
        {
            GetApp()->SetControllerInversion( m_hInvertedCheckbox.IsChecked() );
            bHandled = TRUE;
        }

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    // Name: OnNotifyValueChanged
    // Desc: Message handler for XM_NOTIFY_VALUE_CHANGED
    //--------------------------------------------------------------------------------------
    HRESULT OnNotifyValueChanged( HXUIOBJ hObjSource, XUINotifyValueChanged* pNotifyValueChangedData, BOOL& bHandled )
    {
        if( hObjSource == m_hSensitivitySlider )
        {
            int iSliderValue;
            HRESULT hr = m_hSensitivitySlider.GetValue( &iSliderValue );
            if( FAILED( hr ) )
                return hr;

            GetApp()->SetControllerSensitivity( ( double )iSliderValue );
            bHandled = TRUE;
        }
        return S_OK;
    }
};


//--------------------------------------------------------------------------------------
// Name: class OptionsAudioMenu
// Desc: Class used by the audio options menu
//--------------------------------------------------------------------------------------
class OptionsAudioMenu : public CXuiSceneImpl
{
private:
    CXuiCheckbox m_hMusicCheckbox;
    CXuiCheckbox m_hSoundsCheckbox;
public:
    XUI_IMPLEMENT_CLASS( OptionsAudioMenu, L"OptionsAudioMenu", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
    XUI_END_MSG_MAP()


    //--------------------------------------------------------------------------------------
    // Name: OnInit
    // Desc: Message handler for XM_INIT
    //--------------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        HRESULT hr = GetChildById( L"XuiCheckbox1", &m_hMusicCheckbox );
        if( FAILED( hr ) )
            return hr;

        hr = GetChildById( L"XuiCheckbox2", &m_hSoundsCheckbox );
        if( FAILED( hr ) )
            return hr;

        m_hSoundsCheckbox.SetCheck( !g_bSoundMuted );
        m_hMusicCheckbox.SetCheck( !GetApp()->GetMusicMutedState() );

        return S_OK;
    }

    //--------------------------------------------------------------------------------------
    // Name: OnNotifyPress
    // Desc: Message handler for XM_NOTIFY_PRESS
    //--------------------------------------------------------------------------------------
    HRESULT OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled )
    {
        if( hObjSource == m_hMusicCheckbox )
        {
            GetApp()->SetMusicMutedState( !m_hMusicCheckbox.IsChecked() );
            bHandled = TRUE;
        }
        else if( hObjSource == m_hSoundsCheckbox )
        {
            g_bSoundMuted = !m_hSoundsCheckbox.IsChecked();
            XuiMuteSound( g_bSoundMuted );
            bHandled = TRUE;
        }

        return S_OK;
    }
};


//--------------------------------------------------------------------------------------
// Name: class OptionsLanguageMenu
// Desc: Class used by the language selection menu
//--------------------------------------------------------------------------------------
class OptionsLanguageMenu : public CXuiSceneImpl
{
private:
    CXuiRadioGroup m_hRadioGroup;
public:
    XUI_IMPLEMENT_CLASS( OptionsLanguageMenu, L"OptionsLanguageMenu", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
    XUI_END_MSG_MAP()


    //--------------------------------------------------------------------------------------
    // Name: OnInit
    // Desc: Message handler for XM_INIT
    //--------------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        HRESULT hr = GetChildById( L"XuiRadioGroup1", &m_hRadioGroup );
        if( FAILED( hr ) )
            return hr;

        m_hRadioGroup.SetCurSel( ( int )g_dwLanguageSelection );

        return S_OK;
    }

    //--------------------------------------------------------------------------------------
    // Name: OnNotifyPress
    // Desc: Message handler for XM_NOTIFY_PRESS
    //--------------------------------------------------------------------------------------
    HRESULT OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled )
    {
        if( hObjSource == m_hRadioGroup )
        {
            g_dwLanguageSelection = ( DWORD )m_hRadioGroup.GetCurSel();

            if( g_hRootObj )
            {
                XuiSetLocale( g_strLanguages[g_dwLanguageSelection] );
                XuiElementLocaleChanged( g_hRootObj );
            }

            bHandled = TRUE;
        }

        return S_OK;
    }
};

//--------------------------------------------------------------------------------------
// Name: class AboutMenu
// Desc: Class used by the about menu
//--------------------------------------------------------------------------------------
class AboutMenu : public CXuiSceneImpl
{
protected:
    CXuiEdit m_editAboutText;
    DWORD m_dwLineIndex;
    HANDLE m_hTimerThread;
    BOOL m_bExitThread;

public:
    XUI_IMPLEMENT_CLASS( AboutMenu, L"AboutMenu", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_DESTROY( OnDestroy )
    XUI_END_MSG_MAP()


    //--------------------------------------------------------------------------------------
    // Name: OnInit
    // Desc: Message handler for XM_INIT
    //--------------------------------------------------------------------------------------
    HRESULT             OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        m_hTimerThread = NULL;
        m_dwLineIndex = 0;
        m_bExitThread = FALSE;

        HRESULT hr = GetChildById( L"XuiEdit1", &m_editAboutText );
        if( FAILED( hr ) )
            return hr;

        m_editAboutText.SetVSmoothScroll( TRUE, 0.5f, 1.0f, 0.5f );

        m_hTimerThread = CreateThread( NULL, 0, Timer, this, 0, NULL );
        if( m_hTimerThread == NULL )
            return E_FAIL;

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    // Name: Timer
    // Desc: Timer thread proc that handles scrolling
    //--------------------------------------------------------------------------------------
    static DWORD WINAPI Timer( LPVOID lpThreadParameter )
    {
        AboutMenu* thisClass = ( AboutMenu* )lpThreadParameter;

        while( !thisClass->m_bExitThread )
        {
            thisClass->m_editAboutText.SetTopLine( thisClass->m_dwLineIndex++, TRUE );
            if( thisClass->m_dwLineIndex > 40 )
                thisClass->m_dwLineIndex = 40;
            Sleep( 1000 );
        }

        return 0;
    }


    //--------------------------------------------------------------------------------------
    // Name: OnDestroy
    // Desc: Message handler for XM_DESTROY
    //--------------------------------------------------------------------------------------
    HRESULT             OnDestroy()
    {
        if( m_hTimerThread )
        {
            m_bExitThread = TRUE;
            WaitForSingleObject( m_hTimerThread, INFINITE );
            CloseHandle( m_hTimerThread );
        }

        return S_OK;
    }
};


//--------------------------------------------------------------------------------------
// Name: class FishSelectMenu
// Desc: Class used by the fish selection menu
//--------------------------------------------------------------------------------------
class FishSelectMenu : public CXuiSceneImpl
{
private:
    CXuiList m_hList;
public:
    XUI_IMPLEMENT_CLASS( FishSelectMenu, L"FishSelectMenu", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
        XUI_ON_XM_NOTIFY_SELCHANGED( OnNotifySelChanged )
    XUI_END_MSG_MAP()


    //--------------------------------------------------------------------------------------
    // Name: OnInit
    // Desc: Message handler for XM_INIT
    //--------------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        HRESULT hr = GetChildById( L"XuiCommonList1", &m_hList );
        if( FAILED( hr ) )
            return hr;

        m_hList.SetCurSelVisible( ( int )GetApp()->GetFishType() );

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    // Name: OnNotifyPress
    // Desc: Message handler for XM_NOTIFY_PRESS
    //--------------------------------------------------------------------------------------
    HRESULT OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled )
    {
        if( hObjSource == m_hList )
        {
            GetApp()->SetFishType( ( FISH_TYPE )m_hList.GetCurSel() );
            HXUIOBJ hScene;
            HRESULT hr = SceneCreate( L"file://game:/media/ui.xzp#Content\\Xui\\", L"1PTextureSelect01.xur", &hScene );
            if( FAILED( hr ) )
                return hr;

            hr = NavigateForward( hScene );
            if( FAILED( hr ) )
                return hr;

            bHandled = TRUE;
        }
        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    // Name: OnNotifySelChanged
    // Desc: Message handler for XM_NOTIFY_SELCHANGED
    //--------------------------------------------------------------------------------------
    HRESULT OnNotifySelChanged( HXUIOBJ hObjSource, XUINotifySelChanged* pNotifySel, BOOL& bHandled )
    {
        if( hObjSource == m_hList )
        {
            bHandled = TRUE;
            GetApp()->SetFishType( ( FISH_TYPE )m_hList.GetCurSel() );
        }
        return S_OK;
    }
};


//--------------------------------------------------------------------------------------
// Name: class TextureSelectMenu
// Desc: Class used by the fish texture selection menu
//--------------------------------------------------------------------------------------
class TextureSelectMenu : public CXuiSceneImpl
{
private:
    CXuiList m_hList;
public:
    XUI_IMPLEMENT_CLASS( TextureSelectMenu, L"TextureSelectMenu", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
        XUI_ON_XM_NOTIFY_SELCHANGED( OnNotifySelChanged )
    XUI_END_MSG_MAP()


    //--------------------------------------------------------------------------------------
    // Name: OnInit
    // Desc: Message handler for XM_INIT
    //--------------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        HRESULT hr = GetChildById( L"XuiCommonList1", &m_hList );
        if( FAILED( hr ) )
            return hr;

        m_hList.SetCurSelVisible( ( int )GetApp()->GetTextureIndex() );

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    // Name: OnNotifyPress
    // Desc: Message handler for XM_NOTIFY_PRESS
    //--------------------------------------------------------------------------------------
    HRESULT OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled )
    {
        if( hObjSource == m_hList )
        {
            GetApp()->SetTextureIndex( ( DWORD )m_hList.GetCurSel() );
            HXUIOBJ hScene;
            HRESULT hr = SceneCreate( L"file://game:/media/ui.xzp#Content\\Xui\\", L"GameSetup.xur", &hScene );
            if( FAILED( hr ) )
                return hr;

            hr = NavigateForward( hScene );
            if( FAILED( hr ) )
                return hr;

            bHandled = TRUE;
        }
        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    // Name: OnNotifySelChanged
    // Desc: Message handler for XM_NOTIFY_SELCHANGED
    //--------------------------------------------------------------------------------------
    HRESULT OnNotifySelChanged( HXUIOBJ hObjSource, XUINotifySelChanged* pNotifySel, BOOL& bHandled )
    {
        if( hObjSource == m_hList )
        {
            bHandled = TRUE;
            GetApp()->SetTextureIndex( ( DWORD )m_hList.GetCurSel() );
        }
        return S_OK;
    }
};


//--------------------------------------------------------------------------------------
// Name: class GameSetupMenu
// Desc: Class used by the game setup menu
//--------------------------------------------------------------------------------------
class GameSetupMenu : public CXuiSceneImpl
{
private:
    CXuiList m_hLightingList;
    CXuiList m_hFogColorList;
    CXuiSlider m_hFogDepthSlider;
public:
    XUI_IMPLEMENT_CLASS( GameSetupMenu, L"GameSetupMenu", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_KEYDOWN( OnKeyDown )
        XUI_ON_XM_NOTIFY_SELCHANGED( OnNotifySelChanged )
        XUI_ON_XM_NOTIFY_VALUE_CHANGED( OnNotifyValueChanged )
    XUI_END_MSG_MAP()


    //--------------------------------------------------------------------------------------
    // Name: OnInit
    // Desc: Message handler for XM_INIT
    //--------------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        HRESULT hr = GetChildById( L"XuiCommonList1", &m_hLightingList );
        if( FAILED( hr ) )
            return hr;

        hr = GetChildById( L"XuiCommonList2", &m_hFogColorList );
        if( FAILED( hr ) )
            return hr;

        hr = GetChildById( L"XuiSlider1", &m_hFogDepthSlider );
        if( FAILED( hr ) )
            return hr;

        m_hLightingList.SetCurSelVisible( ( int )GetApp()->GetLightingIntensity() );
        m_hFogColorList.SetCurSelVisible( ( int )GetApp()->GetFogColorIndex() );
        m_hFogDepthSlider.SetValue( ( int )GetApp()->GetFogDepth() );

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    // Name: OnKeyDown
    // Desc: Message handler for XM_KEYDOWN
    //--------------------------------------------------------------------------------------
    HRESULT OnKeyDown( XUIMessageInput* pInputData, BOOL& bHandled )
    {
        switch( pInputData->dwKeyCode )
        {
            case VK_PAD_A:
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
    // Name: OnNotifySelChanged
    // Desc: Message handler for XM_NOTIFY_SELCHANGED
    //--------------------------------------------------------------------------------------
    HRESULT OnNotifySelChanged( HXUIOBJ hObjSource, XUINotifySelChanged* pNotifySel, BOOL& bHandled )
    {
        if( hObjSource == m_hLightingList )
        {
            GetApp()->SetLightingIntensity( ( LIGHTING_INTENSITY )m_hLightingList.GetCurSel() );
            bHandled = TRUE;
        }
        else if( hObjSource == m_hFogColorList )
        {
            GetApp()->SetFogColorIndex( ( DWORD )m_hFogColorList.GetCurSel() );
            bHandled = TRUE;
        }

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    // Name: OnNotifyValueChanged
    // Desc: Message handler for XM_NOTIFY_VALUE_CHANGED
    //--------------------------------------------------------------------------------------
    HRESULT OnNotifyValueChanged( HXUIOBJ hObjSource, XUINotifyValueChanged* pNotifyValueChangedData, BOOL& bHandled )
    {
        if( hObjSource == m_hFogDepthSlider )
        {
            int iSliderValue;
            HRESULT hr = m_hFogDepthSlider.GetValue( &iSliderValue );
            if( FAILED( hr ) )
                return hr;

            if( iSliderValue == 0 )
                iSliderValue = 1;

            GetApp()->SetFogDepth( ( double )iSliderValue );
            bHandled = TRUE;
        }
        return S_OK;
    }
};


//--------------------------------------------------------------------------------------
// Name: class ViewerControl
// Desc: Class used by to render the fish viewer control
//--------------------------------------------------------------------------------------
class ViewerControl : public CXuiSceneImpl
{
protected:
    IDirect3DTexture9* m_pTex;              // texture to Resolve into
    IDirect3DSurface9* m_pRenderTarget;     // render target for off-screen rendering
    HXUISHAPE m_hShape;                     // XUI shape for rendering our texture

public:
            ViewerControl()
            {
                m_pTex = NULL;
                m_pRenderTarget = NULL;
                m_hShape = NULL;
            }

            ~ViewerControl()
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

    XUI_IMPLEMENT_CLASS( ViewerControl, L"ViewerControl", XUI_CLASS_SCENE );

    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_RENDER( OnRender )
    XUI_END_MSG_MAP()


    //--------------------------------------------------------------------------------------
    // Name: OnInit
    // Desc: Message handler for the XM_INIT message
    //--------------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        IAquaticaUI* pApp = GetApp();
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
    // Name: OnRender
    // Desc: Message handler for the XM_RENDER message
    //--------------------------------------------------------------------------------------
    HRESULT OnRender( XUIMessageRender* pRenderData, BOOL& bHandled )
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
        GetApp()->RenderFishPreview( GetApp()->GetFishType(), GetApp()->GetTextureIndex(), pDevice );
        // re-enable writes to all channels
        pDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );

        pDevice->Resolve( 0, NULL, m_pTex, NULL, 0, 0, NULL, 0, 0, NULL );
        pDevice->SetRenderTarget( 0, pOrigRenderTarget );
        pOrigRenderTarget->Release();

        XuiRenderRestoreState( hDC );

        HXUIBRUSH hBrush;
        XuiAttachTextureBrush( m_pTex, &hBrush );
        D3DXMATRIX matBrushXForm;
        D3DXMatrixScaling( &matBrushXForm, 1 / 380.0f, 1 / 280.0f, 1 );
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
};


//--------------------------------------------------------------------------------------
// UI class
//--------------------------------------------------------------------------------------
class CAquaticaUIApp : public CXuiModule
{
public:
                    CAquaticaUIApp()
                    {
                    }
                    ~CAquaticaUIApp()
                    {
                    }

    virtual void    RunFrame();
    HRESULT         DestroyMainScene();

protected:

    HRESULT         RegisterXuiClasses()
    {
        XuiVideoRegister();
        XuiSoundXACTRegister();
        XuiSoundXAudioRegister();
        MainMenu::Register();
        PauseMenu::Register();
        AboutMenu::Register();
        FishSelectMenu::Register();
        TextureSelectMenu::Register();
        GameSetupMenu::Register();
        ControllerOptionsMenu::Register();
        OptionsAudioMenu::Register();
        OptionsLanguageMenu::Register();
        ViewerControl::Register();
        return S_OK;
    }


    HRESULT         UnregisterXuiClasses()
    {
        XuiVideoUnregister();
        XuiSoundXACTUnregister();
        XuiSoundXAudioUnregister();
        MainMenu::Unregister();
        PauseMenu::Unregister();
        AboutMenu::Unregister();
        FishSelectMenu::Unregister();
        TextureSelectMenu::Unregister();
        GameSetupMenu::Unregister();
        ControllerOptionsMenu::Unregister();
        OptionsAudioMenu::Unregister();
        OptionsLanguageMenu::Unregister();
        ViewerControl::Unregister();
        return S_OK;
    }


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

        return S_OK;
    }
};

//--------------------------------------------------------------------------------------
// Global instance of the UI class
//--------------------------------------------------------------------------------------
CAquaticaUIApp  g_uiApp;


//--------------------------------------------------------------------------------------
// Name: ShowMainMenu
// Desc: Displays the main menu
//--------------------------------------------------------------------------------------
HRESULT ShowMainMenu()
{
    HRESULT hr;

    hr = g_uiApp.LoadFirstScene( L"file://game:/media/ui.xzp#Content\\Xui\\", L"PauseMenu.xur" );

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
    D3DXMatrixScaling( &matView, uWidth / 1280.0f, uHeight / 720.0f, 1 );
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
// Name: CAquaticaUIApp::RunFrame
// Desc: Override of the CXuiModule RunFrame() method that checks for Signin UI
// completion
//--------------------------------------------------------------------------------------
void CAquaticaUIApp::RunFrame()
{
    CXuiModule::RunFrame();
}


//--------------------------------------------------------------------------------------
// Name: CAquaticaUIApp::DestroyMainScene
// Desc: Destroys the main menu scene.
//--------------------------------------------------------------------------------------
HRESULT CAquaticaUIApp::DestroyMainScene()
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
    g_hRootObj = NULL;

    HRESULT hr = g_uiApp.InitShared( pApp->m_pd3dDevice, &pApp->m_d3dpp, XuiPNGTextureLoader );
    if( FAILED( hr ) )
        return hr;

    // Register a default typeface
    hr = g_uiApp.RegisterDefaultTypeface( L"Arial", L"file://game:/media/xarialuni.ttf" );
    if( FAILED( hr ) )
        return hr;

    hr = g_uiApp.LoadSkin( L"file://game:/media/ui.xzp#Content\\Xui\\Skins\\Skin.xur" );
    if( FAILED( hr ) )
        return hr;

    hr = g_uiApp.LoadFirstScene( L"file://game:/media/ui.xzp#Content\\Xui\\", L"MainMenu.xur" );
    if( FAILED( hr ) )
        return hr;

    g_bUIActive = TRUE;
    g_bSoundMuted = FALSE;
    g_dwLanguageSelection = 0;

    g_uiApp.Resume();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: IsUIActive
// Desc: Returns the state of the UI.
//--------------------------------------------------------------------------------------
BOOL IsUIActive()
{
    return g_bUIActive;
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
