//--------------------------------------------------------------------------------------
// XuiLocale.cpp
//
// Shows how to implement a XUI based application supporting multiple locales.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>
#include <AtgUtil.h>
#include "AtgMediaLocator.h"
#include "xuilocale_strings.h"

//--------------------------------------------------------------------------------------
// Name: struct LanguageData
// Desc: Maps language path and string index to a language id.
//--------------------------------------------------------------------------------------
struct LanguageData
{
    LPCWSTR pszLanguagePath;
    DWORD dwStringIndex;
} Languages[] =
{
    { NULL,     IDS_ENGLISH },
    { L"ES-ES", IDS_SPANISH },
    { L"FR-FR", IDS_FRENCH },
    { L"DE-DE", IDS_GERMAN },
    { L"IT-IT", IDS_ITALIAN },
    { L"PT-BR", IDS_PORTUGESE },
    { L"JA-JP", IDS_JAPANESE },
    { L"KO-KR", IDS_KOREAN },
    { L"ZH-TW", IDS_CHINESE },
};

// Declare helper necessary to locate resources inside an xzp archive.
static ATG::MediaLocator s_mediaLocator( L"file://game:/media/xuilocale.xzp" );

// Global string table for this application.
CXuiStringTable StringTable;

//----------------------------------------------------------------------------------
// Loads the global string table. CXuiStringTable will clear the table first.
//----------------------------------------------------------------------------------
VOID LoadStringTable( VOID )
{
    WCHAR szResourceLocator[ ATG::LOCATOR_SIZE ];

    s_mediaLocator.ComposeResourceLocator( szResourceLocator, ARRAYSIZE( szResourceLocator ), L"xui/", L"xuilocale_strings.xus" ); 
    StringTable.Load( szResourceLocator );
}

//--------------------------------------------------------------------------------------
// Name: class CLanguageList
// Desc: List implementation class.
//--------------------------------------------------------------------------------------
class CLanguageList : public CXuiListImpl
{
    // Message map. Here we tie messages to message handlers.
    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_GET_SOURCE_TEXT( OnGetSourceText )
        XUI_ON_XM_GET_ITEMCOUNT_ALL( OnGetItemCountAll )
    XUI_END_MSG_MAP()


    //----------------------------------------------------------------------------------
    // Returns the number of items in the list.
    //----------------------------------------------------------------------------------
    HRESULT OnGetItemCountAll( XUIMessageGetItemCount* pGetItemCountData, BOOL& bHandled )
    {
        pGetItemCountData->cItems = sizeof( Languages ) / sizeof( Languages[0] );
        bHandled = TRUE;
        return S_OK;
    }

    //----------------------------------------------------------------------------------
    // Returns the text for the items in the list.
    //----------------------------------------------------------------------------------
    HRESULT OnGetSourceText( XUIMessageGetSourceText* pGetSourceTextData, BOOL& bHandled )
    {
        if( pGetSourceTextData->bItemData && pGetSourceTextData->iItem >= 0 )
        {
            pGetSourceTextData->szText = StringTable.Lookup( Languages[pGetSourceTextData->iItem].dwStringIndex );
            bHandled = TRUE;
        }
        return S_OK;
    }

public:

    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
    XUI_IMPLEMENT_CLASS( CLanguageList, L"LanguageList", XUI_CLASS_LIST )
};


//--------------------------------------------------------------------------------------
// Name: class CMyMainScene
// Desc: Scene implementation class.
//--------------------------------------------------------------------------------------
class CMyMainScene : public CXuiSceneImpl
{
    // Control and Element wrapper objects.
    CXuiControl m_Language;
    CXuiControl m_Setting;
    CXuiControl m_Value;
    CXuiList m_List;


    // Message map. Here we tie messages to message handlers.
    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_SELCHANGED( OnNotifySelChanged )
        XUI_ON_XM_LOCALE_CHANGED( OnLocaleChanged )
    XUI_END_MSG_MAP()


    //----------------------------------------------------------------------------------
    // Performs initialization tasks - retrieves controls.
    //----------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        // Retrieve controls for later use.
        GetChildById( L"labelLanguage", &m_Language );
        GetChildById( L"labelSetting", &m_Setting );
        GetChildById( L"labelValue", &m_Value );
        GetChildById( L"listLanguages", &m_List );

        return S_OK;
    }

    //----------------------------------------------------------------------------------
    // Updates the UI when the list selection changes.
    //----------------------------------------------------------------------------------
    HRESULT OnNotifySelChanged( HXUIOBJ hObjSource, XUINotifySelChanged* pNotifySelChangedData, BOOL& bHandled )
    {
        if( hObjSource == m_List )
        {
            int curSel = m_List.GetCurSel();

            // Set the locale with the current language.
            XuiSetLocale( Languages[curSel].pszLanguagePath );

            // Apply the locale to the main scene.
            XuiApplyLocale( m_hObj, NULL );

            // Update the text for the current value.
            m_Value.SetText( m_List.GetText( curSel ) );

            bHandled = TRUE;
        }

        return S_OK;
    }

    //----------------------------------------------------------------------------------
    // Reloads the global string table when locale changes.
    //----------------------------------------------------------------------------------
    HRESULT OnLocaleChanged( BOOL& bHandled )
    {
        LoadStringTable();
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
    // Register any other classes necessary for the app/scene
    HRESULT hr = CMyMainScene::Register();
    if( FAILED( hr ) )
        return hr;

    hr = CLanguageList::Register();
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UnregisterXuiClasses()
// Desc: Unregisters all the scene classes.
//--------------------------------------------------------------------------------------
HRESULT CMyApp::UnregisterXuiClasses()
{
    CLanguageList::Unregister();
    CMyMainScene::Unregister();
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Application entry point.
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    // Declare an instance of the XUI framework application.
    CMyApp app;

    // Initialize the application.    
    HRESULT hr = app.Init( XuiD3DXTextureLoader );
    if( FAILED( hr ) )
        ATG::FatalError( "Failed intializing application.\n" );

    // Register a default typeface
    hr = app.RegisterDefaultTypeface( L"Arial Unicode MS", L"file://game:/media/xarialuni.ttf" );
    if( FAILED( hr ) )
        ATG::FatalError( "Failed to register default typeface.\n" );

    // Load the string table.
    LoadStringTable();


    WCHAR szResourceLocator[ ATG::LOCATOR_SIZE ];

    // Load the skin file used for the scene.
    s_mediaLocator.ComposeResourceLocator( szResourceLocator, ARRAYSIZE( szResourceLocator ), L"xui/", L"simple_scene_skin.xur" ); 
    app.LoadSkin( szResourceLocator );

    // Load the scene.
    s_mediaLocator.ComposeResourceLocator( szResourceLocator, ARRAYSIZE( szResourceLocator ), L"xui/", NULL ); 
    app.LoadFirstScene( szResourceLocator, L"xuilocale_main.xur", NULL );

    // Run the scene using the built-in loop.
    app.Run();

    // Free resources, unregister custom classes, and exit.
    app.Uninit();
}

