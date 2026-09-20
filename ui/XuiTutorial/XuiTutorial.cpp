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
#include "AtgMediaLocator.h"


//--------------------------------------------------------------------------------------
// Scene implementation class.
//--------------------------------------------------------------------------------------
class CMyMainScene : public CXuiSceneImpl
{

protected:

    // Control and Element wrapper objects.
    CXuiControl m_button1;
    CXuiControl m_button2;
    CXuiTextElement m_text1;

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
        GetChildById( L"XuiButton1", &m_button1 );
        GetChildById( L"XuiButton2", &m_button2 );
        GetChildById( L"XuiText1", &m_text1 );
        return S_OK;
    }

    //----------------------------------------------------------------------------------
    // Handler for the button press message.
    //----------------------------------------------------------------------------------
    HRESULT OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
    {
        // Determine which button was pressed,
        // and set the text accordingly.
        if( hObjPressed == m_button1 )
            m_text1.SetText( L"One" );
        else if( hObjPressed == m_button2 )
            m_text1.SetText( L"Two" );
        else
            return S_OK;

        bHandled = TRUE;
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
// Name: main
// Desc: Application entry point.
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    // Declare helper necessary to locate resources inside an xzp archive.
    ATG::MediaLocator mediaLocator( L"file://game:/media/simplescene.xzp" );
    WCHAR szResourceLocator[ ATG::LOCATOR_SIZE ];

    // Declare an instance of the XUI framework application.
    CMyApp app;

    // Initialize the application.    
    HRESULT hr = app.Init( XuiD3DXTextureLoader );
    if( FAILED( hr ) )
    {
        OutputDebugString( "Failed intializing application.\n" );
        return;
    }

    // Register a default typeface
    hr = app.RegisterDefaultTypeface( L"Arial Unicode MS", L"file://game:/media/xarialuni.ttf" );
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
    app.LoadFirstScene( szResourceLocator, L"simple_scene.xur", NULL );

    // Run the scene.    
    app.Run();

    // Free resources, unregister custom classes, and exit.
    app.Uninit();
}
