//--------------------------------------------------------------------------------------
// XuiMultiInput.cpp
//
// Shows how to collect input from multiple users simultaneously.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>
#include <stdio.h>
#include "XuiMultiInput.h"


namespace
{

//--------------------------------------------------------------------------------------
// Name: GetPlayerInfoFromContainer
// Desc: Searches up the Xui object tree for a parent of class "UserContainerScene",
//       maps that object to the backing CUserContainerScene object, and gets a pointer
//       to the player info structure for this user.
//--------------------------------------------------------------------------------------
PlayerInfo* GetPlayerInfoFromContainer( HXUIOBJ hObj )
{
    PlayerInfo* pPlayerInfo = 0;

    HXUICLASS hContainerClass = XuiFindClass( L"UserContainerScene" );
    if( hContainerClass != 0 )
    {
        while( hObj != 0 )
        {
            HXUIOBJ hParent = 0;
            HRESULT hr = XuiElementGetParent( hObj, &hParent );
            if( SUCCEEDED( hr ) )
            {
                HXUIOBJ hContainerObj = XuiDynamicCast( hParent, hContainerClass );
                if( hContainerObj != 0 )
                {
                    CUserContainerScene* pContainer = 0;
                    if( SUCCEEDED( XuiObjectFromHandle( hContainerObj, ( void** )&pContainer ) ) )
                    {
                        pPlayerInfo = pContainer->GetPlayerInfo();
                        break;
                    }
                }
            }

            hObj = hParent;
        }
    }

    return pPlayerInfo;
}

} // namespace



//--------------------------------------------------------------------------------------
// Multi-Input scene : C++ backing class for a Xui scene containing four sub-scenes,
//                     each of which is a container for user-specific scenes
//--------------------------------------------------------------------------------------
CMultiInputScene::CMultiInputScene() : m_pPlayerArray( 0 )
{
}

HRESULT CMultiInputScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    HRESULT hr = S_OK;

    // A PlayerArray structure is passed in from this object's owner. We'll
    // pass pointers to each entry in the PlayerArray to the respective user
    // containers.
    m_pPlayerArray = ( PlayerArray* )pInitData->pvInitData;
    if( m_pPlayerArray == 0 )
    {
        return E_FAIL;
    }

    HXUICLASS hContainerClass = XuiFindClass( L"UserContainerScene" );

    for( BYTE i = 0; i < 4; ++i )
    {
        // Get child user container scene
        WCHAR containerName[32] =
        {
            0
        };
        swprintf_s( containerName, L"UserContainer%d", i );
        HXUIOBJ hChild = 0;
        hr = GetChildById( containerName, &hChild );
        ASSERT( SUCCEEDED( hr ) );

        // Ensure that the child is of the UserConatinerScene class, and save
        // the child object handle.
        m_hContainers[i] = XuiDynamicCast( hChild, hContainerClass );
        ASSERT( m_hContainers[i] != 0 );

        // Get the backing CUserContainerScene C++ object from XUI handle
        CUserContainerScene* pContainer = 0;
        hr = XuiObjectFromHandle( m_hContainers[i], ( void** )&pContainer );
        ASSERT( SUCCEEDED( hr ) );

        // Init player info on this container
        pContainer->SetPlayerInfo( &m_pPlayerArray->players[i] );

        // Set the "not connected" scene as the first scene in the user container
        CXuiScene sceneNoUser;
        hr = SceneCreate( L"multiinput_notconnected_scene.xur", &sceneNoUser );
        ASSERT( SUCCEEDED( hr ) );

        // Specify the UserIndex (param 3) when navigating--this filters user input
        // by user index.  Otherwise the scene will accept input from all controllers.
        NavigateFirst( m_hContainers[i], sceneNoUser, i );
    }

    // Set a timer to check for controller connections/disconnections
    SetTimer( 0, 200 );

    bHandled = TRUE;
    return hr;
}

HRESULT CMultiInputScene::OnTimer( XUIMessageTimer* pXUIMessageTimer, BOOL& bHandled )
{
    // Check for controller connections/disconnections
    for( int i = 0; i < 4; ++i )
    {
        XINPUT_STATE state;
        BOOL fConnected = ( XInputGetState( i, &state ) == ERROR_SUCCESS );
        if( m_pPlayerArray->players[i].GetConnected() != fConnected )
        {
            // Controller connection state changed
            m_pPlayerArray->players[i].SetConnected( fConnected );

            // Broadcast connection changed message to the appropriate user container
            MessageUserConnectionsChanged info;
            XUIMessage msgUserConnectionsChanged;
            InitMessageUserConnectionsChanged( &msgUserConnectionsChanged, &info,
                                               m_pPlayerArray->players[i].GetUserIndex(), fConnected );
            XuiBroadcastMessage( m_hContainers[i], &msgUserConnectionsChanged );
        }
    }

    bHandled = TRUE;
    return S_OK;
}



//--------------------------------------------------------------------------------------
// User Container scene : C++ backing class for the user container sub-scenes within
//                        the Multi-Input scene.  This class receives the player info
//                        pointer from its parent (Multi-Input scene), and supplies it
//                        to its child scenes.
//--------------------------------------------------------------------------------------
CUserContainerScene::CUserContainerScene() : m_pPlayerInfo( 0 )
{
}

void CUserContainerScene::SetPlayerInfo( PlayerInfo* pPlayerInfo )
{
    m_pPlayerInfo = pPlayerInfo;
}

PlayerInfo* CUserContainerScene::GetPlayerInfo()
{
    return m_pPlayerInfo;
}


//--------------------------------------------------------------------------------------
// User base class : C++ base class for scenes that display user-specific info.  This 
//                   class handles the "user connections changed" message by navigating
//                   forward or backward, and informs derrived classes to update their
//                   control states (upon XM_TRANSITION_START).
//--------------------------------------------------------------------------------------
HRESULT CUserBase::OnUserConnectionsChanged( BYTE bUserIndex, BOOL fConnected, BOOL& bHandled )
{
    HRESULT hr = S_OK;
    PlayerInfo* pPlayerInfo = GetPlayerInfoFromContainer( m_hObj );
    if( pPlayerInfo == 0 )
    {
        ASSERT( pPlayerInfo != 0 );
        return hr = E_FAIL;
    }
    else if( pPlayerInfo->GetUserIndex() == bUserIndex )
    {
        if( fConnected == FALSE )
        {
            // Controller disconnected. Navigate back to the "not connected" scene.
            pPlayerInfo->SetConnected( FALSE );
            pPlayerInfo->SetCarModel( 0 );
            pPlayerInfo->SetCarColor( 0 );
            NavigateBackToFirst( bUserIndex );
        }
        else
        {
            // Controller connected. We should be at the first scene, "not connected". Go to "user menu" scene.
            ASSERT( GetBackScene() == 0 );
            CXuiScene sceneUser;
            hr = SceneCreate( L"multiinput_usermenu_scene.xur", &sceneUser );
            ASSERT( SUCCEEDED( hr ) );

            XuiSceneNavigateForward( m_hObj, FALSE, sceneUser, bUserIndex );
        }

        bHandled = TRUE;
    }

    return hr;
}

HRESULT CUserBase::OnTransitionStart( XUIMessageTransition* pTransData, BOOL& bHandled )
{
    HRESULT hr = S_OK;

    PlayerInfo* pPlayerInfo = GetPlayerInfoFromContainer( m_hObj );
    if( pPlayerInfo != 0 )
    {
        // Update control states when a transition starts
        hr = UpdateControls( pPlayerInfo );
    }

    return hr;
}

HRESULT CUserBase::UpdateControls( PlayerInfo* pPlayerInfo )
{
    return S_OK;
}



//--------------------------------------------------------------------------------------
// User Not Connected scene : Scene with a text control that identifies the user index
//                            as being not connected.
//--------------------------------------------------------------------------------------
HRESULT CUserNotConnectedScene::UpdateControls( PlayerInfo* pPlayerInfo )
{
    //
    // Init text with user index from current PlayerInfo data
    //

    ASSERT( pPlayerInfo != 0 );

    CXuiTextElement displayText;
    HRESULT hr = GetChildById( L"DisplayText", &displayText );
    ASSERT( SUCCEEDED( hr ) );

    WCHAR szText[64] =
    {
        0
    };
    swprintf_s( szText, L"User %d\nNot Connected", pPlayerInfo->GetUserIndex() );
    displayText.SetText( szText );

    return hr;
}



//--------------------------------------------------------------------------------------
// User Menu scene : Scene that displays current player info, and allows user to
//                   navigate to other scenes to edit the player info.
//--------------------------------------------------------------------------------------
HRESULT CUserMenuScene::UpdateControls( PlayerInfo* pPlayerInfo )
{
    //
    // Init user index, car model and car color text dislays with current PlayerInfo data
    //

    ASSERT( pPlayerInfo != 0 );

    HRESULT hr = S_OK;

    CXuiTextElement textCtrl;

    hr = GetChildById( L"UserIndex", &textCtrl );
    ASSERT( SUCCEEDED( hr ) );
    WCHAR szUserIndex[128] =
    {
        0
    };
    swprintf_s( szUserIndex, L"User %d", pPlayerInfo->GetUserIndex() );
    textCtrl.SetText( szUserIndex );

    hr = GetChildById( L"CarModel", &textCtrl );
    ASSERT( SUCCEEDED( hr ) );
    textCtrl.SetText( pPlayerInfo->GetCarModel() );

    hr = GetChildById( L"CarColor", &textCtrl );
    ASSERT( SUCCEEDED( hr ) );
    textCtrl.SetText( pPlayerInfo->GetCarColor() );

    return hr;
}



//--------------------------------------------------------------------------------------
// Select car model scene : Scene with a list box containing car models to select from.
//                          During UpdateControls, we select the current item identified
//                          in the player info. When a list item is selected (A button)
//                          we persist the selected item to the player data and navigate
//                          back to the user menu scene.
//--------------------------------------------------------------------------------------
HRESULT CSelectCarModelScene::UpdateControls( PlayerInfo* pPlayerInfo )
{
    //
    // Init list box selection from player info
    //

    ASSERT( pPlayerInfo != 0 );

    CXuiList listCtrl;
    HRESULT hr = GetChildById( L"CarList", &listCtrl );
    ASSERT( SUCCEEDED( hr ) );

    LPCWSTR pszCarModel = pPlayerInfo->GetCarModel();
    if( pszCarModel != 0 )
    {
        for( int i = 0; i < listCtrl.GetItemCount(); ++i )
        {
            LPCWSTR pszItemText = listCtrl.GetText( i );
            if( pszItemText != NULL && wcscmp( pszItemText, pszCarModel ) == 0 )
            {
                listCtrl.SetCurSelVisible( i );
                break;
            }
        }
    }

    return hr;
}

HRESULT CSelectCarModelScene::OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled )
{
    //
    // If this is a list item press, save the selection and navigate back
    //

    PlayerInfo* pPlayerInfo = GetPlayerInfoFromContainer( m_hObj );
    if( pPlayerInfo != 0 )
    {
        HXUIOBJ hListCtrl = XuiDynamicCast( hObjSource, XuiFindClass( XUI_CLASS_COMMONLIST ) );
        if( hListCtrl != 0 )
        {
            CXuiList listCtrl( hListCtrl );
            pPlayerInfo->SetCarModel( listCtrl.GetText( listCtrl.GetCurSel() ) );
            NavigateBack( pPlayerInfo->GetUserIndex() );
            bHandled = TRUE;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Select car color scene : Scene with a list box containing car colors to select from.
//                          During UpdateControls, we select the current item identified
//                          in the player info. When a list item is selected (A button)
//                          we persist the selected item to the player data and navigate
//                          back to the user menu scene.
//--------------------------------------------------------------------------------------
HRESULT CSelectCarColorScene::UpdateControls( PlayerInfo* pPlayerInfo )
{
    //
    // Init list box selection from player info
    //

    ASSERT( pPlayerInfo != 0 );

    CXuiList listCtrl;
    HRESULT hr = GetChildById( L"ColorList", &listCtrl );
    ASSERT( SUCCEEDED( hr ) );

    LPCWSTR pszCarColor = pPlayerInfo->GetCarColor();
    if( pszCarColor != 0 )
    {
        for( int i = 0; i < listCtrl.GetItemCount(); ++i )
        {
            LPCWSTR pszItemText = listCtrl.GetText( i );
            if( pszItemText != NULL && wcscmp( pszItemText, pszCarColor ) == 0 )
            {
                listCtrl.SetCurSelVisible( i );
                break;
            }
        }
    }

    return hr;
}

HRESULT CSelectCarColorScene::OnNotifyPress( HXUIOBJ hObjSource, BOOL& bHandled )
{
    //
    // If this is a list item press, save the selection and navigate back
    //

    PlayerInfo* pPlayerInfo = GetPlayerInfoFromContainer( m_hObj );
    if( pPlayerInfo != 0 )
    {
        HXUIOBJ hListCtrl = XuiDynamicCast( hObjSource, XuiFindClass( XUI_CLASS_COMMONLIST ) );
        if( hListCtrl != 0 )
        {
            CXuiList listCtrl( hListCtrl );
            pPlayerInfo->SetCarColor( listCtrl.GetText( listCtrl.GetCurSel() ) );
            NavigateBack( pPlayerInfo->GetUserIndex() );
            bHandled = TRUE;
        }
    }

    return S_OK;
}














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
    HRESULT hr = S_OK;

    hr = CMultiInputScene::Register();
    ASSERT( SUCCEEDED( hr ) );
    hr = CUserContainerScene::Register();
    ASSERT( SUCCEEDED( hr ) );
    hr = CUserNotConnectedScene::Register();
    ASSERT( SUCCEEDED( hr ) );
    hr = CUserMenuScene::Register();
    ASSERT( SUCCEEDED( hr ) );
    hr = CSelectCarModelScene::Register();
    ASSERT( SUCCEEDED( hr ) );
    hr = CSelectCarColorScene::Register();
    ASSERT( SUCCEEDED( hr ) );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: UnregisterXuiClasses
// Desc: Unregisters all the scene classes.
//--------------------------------------------------------------------------------------
HRESULT CMyApp::UnregisterXuiClasses()
{
    HRESULT hr = S_OK;

    hr = CMultiInputScene::Unregister();
    ASSERT( SUCCEEDED( hr ) );
    hr = CUserContainerScene::Unregister();
    ASSERT( SUCCEEDED( hr ) );
    hr = CUserNotConnectedScene::Unregister();
    ASSERT( SUCCEEDED( hr ) );
    hr = CUserMenuScene::Unregister();
    ASSERT( SUCCEEDED( hr ) );
    hr = CSelectCarModelScene::Unregister();
    ASSERT( SUCCEEDED( hr ) );
    hr = CSelectCarColorScene::Unregister();
    ASSERT( SUCCEEDED( hr ) );

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

    HRESULT hr = XuiRenderBegin( m_hDC, D3DCOLOR_ARGB( 255, 0, 0, 0 ) );
    if( FAILED( hr ) )
        return hr;

    UINT uBackBufferWidth, uBackBufferHeight;
    hr = XuiRenderGetBackBufferSize( m_hDC, &uBackBufferWidth, &uBackBufferHeight );
    if( FAILED( hr ) )
        return hr;

    float dx = ( ( float )uBackBufferWidth - 640 ) / 2;
    float dy = ( ( float )uBackBufferHeight - 480 ) / 2;
    D3DXMATRIX matView;
    D3DXMatrixTranslation( &matView, dx, dy, 0 );
    XuiRenderSetViewTransform( m_hDC, &matView );

    XUIMessage msg;
    XUIMessageRender msgRender;
    XuiMessageRender( &msg, &msgRender, m_hDC, 0xffffffff, XUI_BLEND_NORMAL );
    XuiSendMessage( m_hObjRoot, &msg );

    XuiRenderEnd( m_hDC );
    XuiRenderPresent( m_hDC, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: main
// Desc: Application entry point.
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    // Declare an instance of the XUI framework application.
    CMyApp app;

    // Initialize the application.    
    HRESULT hr = app.Init( XuiD3DXTextureLoader );
    if( FAILED( hr ) )
    {
        OutputDebugString( "Failed intializing application.\n" );
        return 0;
    }

    // Turn on interruptable transitions globally
    XuiSetInterruptTransitionsDefault( TRUE );

    // Register a default typeface
    hr = app.RegisterDefaultTypeface( L"Arial Unicode MS", L"file://game:/media/xarialuni.ttf" );
    if( FAILED( hr ) )
    {
        OutputDebugString( "Failed to register default typeface.\n" );
        return 0;
    }

    // Load the skin file used for the scene.
    app.LoadSkin( L"file://game:/media/xuimultiinput.xzp#media\\xui\\simple_scene_skin.xur" );

    // Load the scene.
    PlayerArray players;
    HXUIOBJ hScene = 0;
    hr = XuiSceneCreate( L"file://game:/media/xuimultiinput.xzp#media\\xui\\",
                         L"multiinput_scene.xur", &players, &hScene );

    hr = XuiSceneNavigateFirst( app.GetRootObj(), hScene, XUSER_INDEX_FOCUS );

    // Run the scene.    
    app.Run();

    // Free resources, unregister custom classes, and exit.
    app.Uninit();
}

