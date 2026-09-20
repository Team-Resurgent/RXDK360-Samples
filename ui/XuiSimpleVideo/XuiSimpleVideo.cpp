//--------------------------------------------------------------------------------------
// XuiSimpleVideo.cpp
//
// Shows how to display and use video files using a simple XUI scene.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>
#include "AtgMediaLocator.h"


// Declare helper necessary to locate resources inside an xzp archive.
ATG::MediaLocator g_mediaLocator( L"file://game:/media/xuivideo.xzp" );

//--------------------------------------------------------------------------------------
// Name: class CMyMainScene
// Desc: Scene implementation class.
//--------------------------------------------------------------------------------------
class CMyMainScene : public CXuiSceneImpl
{
    // Control and Element wrapper objects.
    CXuiVideo m_Video;
    CXuiControl m_PlayButton;
    CXuiControl m_StopButton;

    // Message map. Here we tie messages to message handlers.
    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
    XUI_END_MSG_MAP()


    //----------------------------------------------------------------------------------
    // Performs initialization tasks - retrieves controls.
    //----------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        // Retrieve controls for later use.
        GetChildById( L"XuiVideo1", &m_Video );
        GetChildById( L"XuiBtnPlay", &m_PlayButton );
        GetChildById( L"XuiBtnStop", &m_StopButton );

        return S_OK;
    }


    //----------------------------------------------------------------------------------
    // Handler for the button press message.
    //----------------------------------------------------------------------------------
    HRESULT OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
    {
        // Play button - if the video is paused, we un-pause from current position. If
        // video is currently playing, we restart the video.
        if( hObjPressed == m_PlayButton )
        {
            // If we're simply paused, unpause
            if( m_Video.IsPaused() )
            {
                m_Video.Pause( FALSE );
            }
            else
            {
                // Else start/restart the video
                WCHAR szResourceLocator[ ATG::LOCATOR_SIZE ];

                g_mediaLocator.ComposeResourceLocator( szResourceLocator, ARRAYSIZE( szResourceLocator ), L"xui/", L"MikesBestCrash.wmv" ); 
                if( FAILED( m_Video.Play( szResourceLocator ) ) )
                {
                    OutputDebugString( "Oops, failed to play video." );
                }
            }
        }
            // Stop button - this actually pauses the video
        else if( hObjPressed == m_StopButton )
        {
            // Pause until play is pressed again
            m_Video.Pause( TRUE );
        }
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
// Name: RegisterXuiClasses()
// Desc: Registers all the scene classes.
//--------------------------------------------------------------------------------------
HRESULT CMyApp::RegisterXuiClasses()
{
    // We must register the video control classes
    XuiVideoRegister();

    // Register any other classes necessary for the app/scene
    return CMyMainScene::Register();
}


//--------------------------------------------------------------------------------------
// Name: UnregisterXuiClasses()
// Desc: Unregisters all the scene classes.
//--------------------------------------------------------------------------------------
HRESULT CMyApp::UnregisterXuiClasses()
{
    // Unregister video stuff
    XuiVideoUnregister();

    CMyMainScene::Unregister();
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RegisterXuiClasses()
// Desc: Application entry point.
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
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

    WCHAR szResourceLocator[ ATG::LOCATOR_SIZE ];

    // Load the skin file used for the scene.
    g_mediaLocator.ComposeResourceLocator( szResourceLocator, ARRAYSIZE( szResourceLocator ), L"xui/", L"simple_scene_skin.xur" ); 
    app.LoadSkin( szResourceLocator );

    // Load the scene.
    g_mediaLocator.ComposeResourceLocator( szResourceLocator, ARRAYSIZE( szResourceLocator ), L"xui/", NULL ); 
    app.LoadFirstScene( szResourceLocator, L"xui_video.xur", NULL );

    // Run the scene using the built-in loop. If you take care of this in your game loop, 
    // you must call XuiTimersRun() in order for video controls to function.
    app.Run();

    // Free resources, unregister custom classes, and exit.
    app.Uninit();
}
