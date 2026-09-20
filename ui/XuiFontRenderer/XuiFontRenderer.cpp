//--------------------------------------------------------------------------------------
// XuiTutorial.cpp
//
// Shows how to display and use a simple XUI scene.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>
#include <AtgMediaLocator.h>
#include "fontrenderer.h"


const WCHAR* const  g_szText =
    L"Four score and seven years ago our fathers brought forth on this continent, a new nation, "
    L"conceived in Liberty, and dedicated to the proposition that all men are created equal.\n"
    L"Now we are engaged in a great civil war, testing whether that nation, or any nation so "
    L"conceived and so dedicated, can long endure. We are met on a great battle-field of that war. "
    L"We have come to dedicate a portion of that field, as a final resting place for those who here "
    L"gave their lives that that nation might live. It is altogether fitting and proper that we "
    L"should do this.\n"
    L"But, in a larger sense, we can not dedicate -- we can not consecrate -- we can not hallow -- "
    L"this ground. The brave men, living and dead, who struggled here, have consecrated it, far "
    L"above our poor power to add or detract. The world will little note, nor long remember what "
    L"we say here, but it can never forget what they did here. It is for us the living, rather, to "
    L"be dedicated here to the unfinished work which they who fought here have thus far so nobly "
    L"advanced. It is rather for us to be here dedicated to the great task remaining before us -- "
    L"that from these honored dead we take increased devotion to that cause for which they gave the "
    L"last full measure of devotion -- that we here highly resolve that these dead shall not have "
    L"died in vain -- that this nation, under God, shall have a new birth of freedom -- and that "
    L"government of the people, by the people, for the people, shall not perish from the earth.";



MyXuiFontRenderer*  g_pMyFontRenderer = NULL;
int                 g_RenderMode = 0;

IDirect3DDevice9*   g_pDevice = NULL;

//--------------------------------------------------------------------------------------
// Scene implementation class.
//--------------------------------------------------------------------------------------
class CMyMainScene : public CXuiSceneImpl
{

protected:

    // Control and Element wrapper objects.
    CXuiControl m_ToggleButton;
    CXuiTextElement m_RendererText;
    CXuiEdit m_Edit;

    // Message map.
    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
    XUI_END_MSG_MAP()

    //----------------------------------------------------------------------------------
    // Performs initialization tasks - retreives controls.
    //----------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        // Retrieve controls for later use.
        GetChildById( L"ToggleButton", &m_ToggleButton );
        GetChildById( L"RendererText", &m_RendererText );
        GetChildById( L"TextEdit", &m_Edit );

        // Init text field and edit control text
        m_Edit.SetText( g_szText );
        m_RendererText.SetText( L"Rendering to Xui texture cache" );
        return S_OK;
    }

    //----------------------------------------------------------------------------------
    // Handler for the button press message.
    //----------------------------------------------------------------------------------
    HRESULT OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
    {
        // Determine which button was pressed, and set the text accordingly.
        if( hObjPressed == m_ToggleButton )
        {
            HXUIOBJ hRoot = hObjPressed;
            HXUIOBJ hCur = hObjPressed;
            while( SUCCEEDED( XuiElementGetParent( hCur, &hCur ) ) && hCur != NULL )
                hRoot = hCur;
            XuiElementDiscardResources( hRoot, XUI_DISCARD_FONTS );

            g_RenderMode = ++g_RenderMode % 3;

            if( g_RenderMode == 0 )
            {
                g_pMyFontRenderer->SetRendererMode( DrawToTexture );
                XuiFontSetRenderer( g_pMyFontRenderer );
                m_RendererText.SetText( L"Rendering to Xui texture cache" );
            }
            else if( g_RenderMode == 1 )
            {
                g_pMyFontRenderer->SetRendererMode( DrawToDevice );
                XuiFontSetRenderer( g_pMyFontRenderer );
                m_RendererText.SetText( L"Rendering to device" );
            }
            else
            {
                XuiFontSetRenderer( NULL );
                m_RendererText.SetText( L"Xui default renderer" );
            }


            // Make sure all the Xui elements update themselves for the new font metrics
            m_Edit.SetTopLine( 0, FALSE );
            m_Edit.SetText( g_szText );

            bHandled = TRUE;
        }

        return S_OK;
    }

public:

    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
    XUI_IMPLEMENT_CLASS( CMyMainScene, L"MyMainScene", XUI_CLASS_SCENE )
};


//--------------------------------------------------------------------------------------
// Main XUI host class. It is responsible for registering scene classes and provide
// basic initialization, scene loading and rendering capability.
//--------------------------------------------------------------------------------------
class CMyApp : public CXuiModule
{
protected:
    // Override RegisterXuiClasses so that CMyApp can register classes.
    virtual HRESULT RegisterXuiClasses();

    // Override UnregisterXuiClasses so that CMyApp can unregister classes. 
    virtual HRESULT UnregisterXuiClasses();

    // Override Render so that CMyApp can set the view matrix. 
    virtual HRESULT Render();

};


//--------------------------------------------------------------------------------------
// Name: RegisterXuiClasses
// Desc: Registers all the scene classes.
//--------------------------------------------------------------------------------------
HRESULT CMyApp::RegisterXuiClasses()
{
    return CMyMainScene::Register();
}


//--------------------------------------------------------------------------------------
// Name: UnregisterXuiClasses
// Desc: Unregisters all the scene classes.
//--------------------------------------------------------------------------------------
HRESULT CMyApp::UnregisterXuiClasses()
{
    CMyMainScene::Unregister();
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Overrides the CXuiModule Render method in order to set the view matrix and 
//       center the scene on the screen.
//--------------------------------------------------------------------------------------
HRESULT CMyApp::Render()
{
    ASSERT( m_hDC != NULL );
    ASSERT( g_pDevice != NULL );

    g_pDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0L );

    HRESULT hr = XuiRenderBegin( m_hDC, D3DCOLOR_ARGB( 255, 0, 0, 0 ) );
    if( FAILED( hr ) )
        return hr;

    UINT uBackBufferWidth, uBackBufferHeight;
    hr = XuiRenderGetBackBufferSize( m_hDC, &uBackBufferWidth, &uBackBufferHeight );
    if( FAILED( hr ) )
        return hr;

    FLOAT dx = ( ( FLOAT )uBackBufferWidth - 640 ) / 2;
    FLOAT dy = ( ( FLOAT )uBackBufferHeight - 480 ) / 2;
    D3DXMATRIX matView;
    D3DXMatrixTranslation( &matView, dx, dy, 0 );
    XuiRenderSetViewTransform( m_hDC, &matView );

    XUIMessage msg;
    XUIMessageRender msgRender;
    XuiMessageRender( &msg, &msgRender, m_hDC, 0xffffffff, XUI_BLEND_NORMAL );
    XuiSendMessage( m_hObjRoot, &msg );

    XuiRenderEnd( m_hDC );
    XuiRenderPresent( m_hDC, NULL, NULL, NULL );

    g_pDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: main
// Desc: Application entry point.
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    // Declare helper necessary to locate resources inside an xzp archive.
    ATG::MediaLocator mediaLocator( L"file://game:/media/xuifontrenderer.xzp" );
    WCHAR szResourceLocator[ ATG::LOCATOR_SIZE ];

    // Declare an instance of the XUI framework application.
    CMyApp app;

    // Init D3D
    D3DPRESENT_PARAMETERS d3dpp =
    {
        0
    };
    d3dpp.BackBufferWidth = 1280;
    d3dpp.BackBufferHeight = 720;
    d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
    d3dpp.BackBufferCount = 1;
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    IDirect3D9* pD3D = Direct3DCreate9( D3D_SDK_VERSION );
    if( !pD3D )
        return;

    HRESULT hr = pD3D->CreateDevice( 0, D3DDEVTYPE_HAL, NULL,
                                     D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &g_pDevice );
    if( FAILED( hr ) )
    {
        return;
    }


    // Initialize the application.
    hr = app.InitShared( g_pDevice, &d3dpp, XuiD3DXTextureLoader, NULL );
    if( FAILED( hr ) )
    {
        OutputDebugString( "Failed intializing application.\n" );
        return;
    }

    // Initialize custom font renderer
    g_pMyFontRenderer = new MyXuiFontRenderer( g_pDevice );
    g_pMyFontRenderer->SetRendererMode( DrawToTexture );

    // Set custom font renderer before loading Xui skins or scenes. To change the renderer 
    // after scenes are loaded, XuiElementDiscardResources( hRoot, XUI_DISCARD_FONTS ) will
    // have to be called.
    XuiFontSetRenderer( g_pMyFontRenderer );

    // Register a default typeface
    hr = app.RegisterDefaultTypeface( L"MyTypeface", L"file://game:/media/xarialuni.ttf" );
    if( FAILED( hr ) )
    {
        OutputDebugString( "Failed to register default typeface.\n" );
        return;
    }

    // Load the skin file used for the scene.
    mediaLocator.ComposeResourceLocator( szResourceLocator, ARRAYSIZE( szResourceLocator ), L"xui/", L"simple_scene_skin.xur" ); 
    app.LoadSkin( szResourceLocator );

    // Load the scene.
    mediaLocator.ComposeResourceLocator( szResourceLocator, ARRAYSIZE( szResourceLocator ), L"xui/", NULL ); 
    app.LoadFirstScene( szResourceLocator, L"XuiFontRenderer.xur", NULL );

    // Run the scene.    
    app.Run();

    // Free resources, unregister custom classes, and exit.
    app.Uninit();
}
