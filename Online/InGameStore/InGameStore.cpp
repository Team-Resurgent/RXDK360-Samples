//--------------------------------------------------------------------------------------
// InGameStore
//
// Sample to demonstrate the Marketplace APIs and XContent APIs that you encounter when
// implementing an In-Game Store.
// 
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// INCLUDES
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgResource.h"
#include "AtgSignIn.cpp"
#include "AtgUtil.h"


#include "UIHelpers.h"
#include "InGameOfferManager.h"
#include "InGameDownloadManager.h"
#include "InGameContentManager.h"


//--------------------------------------------------------------------------------------
// Name: LobbyScreen
// Desc: User may choose between viewing a list of content of viewing a list of offers
//--------------------------------------------------------------------------------------
class LobbyScreen : public UIScreen
{

public:

	//--------------------------------------------------------------------------------------
	// Name: Initialize
	// Desc: Create buttons to navigate to other screens
	//--------------------------------------------------------------------------------------
	VOID Initialize( UIScreen& scrnOffer, UIScreen& scrnContent, UIScreen& scrnDownload)
	{
		CreateNavButton( UIHelpers::s_fBUTTON_LEFT_X,
			             UIHelpers::s_fBUTTON_UPPER_Y,
						 XINPUT_GAMEPAD_A,
						 GLYPH_A_BUTTON L"Offer Screen",
						 &scrnOffer );

		CreateNavButton( UIHelpers::s_fBUTTON_RIGHT_X,
			             UIHelpers::s_fBUTTON_UPPER_Y,
						 XINPUT_GAMEPAD_X,
						 GLYPH_X_BUTTON L"Content Screen",
						 &scrnContent );

		CreateNavButton( UIHelpers::s_fBUTTON_RIGHT_X,
			             UIHelpers::s_fBUTTON_LOWER_Y,
						 XINPUT_GAMEPAD_Y,
						 GLYPH_Y_BUTTON L"Download Screen",
						 &scrnDownload);
	}

	virtual VOID Update( ATG::GAMEPAD* pGamepad ) {	UpdateButtons( pGamepad );	}
	virtual VOID Render()	{ RenderButtons(); }
};

//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:

	~Sample()
	{		
		// Cleanup Managers
		OfferManager::DeleteInstance();
		DownloadManager::DeleteInstance();
		ContentManager::DeleteInstance();
	}

private:

    virtual HRESULT Initialize();
	
	// Perform common tasks then call the appropriate procedure based on application state
	virtual HRESULT Update();
	virtual HRESULT Render();

	//--------------------------------------------------------------------------------------
	// UIScreens
	//--------------------------------------------------------------------------------------
	LobbyScreen    m_LobbyScreen;
	OfferScreen    m_OfferScreen;
	DownloadScreen m_DownloadScreen;
	ContentScreen  m_ContentScreen;
	
private:
	
	HANDLE m_hNotificationSys;                          // Track System Notifications
	HANDLE m_hNotificationLiv;                          // Track Live Notifications
	XUSER_SIGNIN_INFO  m_SignInInfo[XUSER_MAX_COUNT];   // sign-in info for the local client
	WCHAR m_wszUserName[XUSER_NAME_SIZE];               // Keep track of the currently signed in user
};

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
	if( FAILED(	UIHelpers::Initialize( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
	{
		ATG::FatalError( "Failed to initialize UIHelers\n " );
	}

	// initialize all the screens
	m_OfferScreen.Initialize();
	m_DownloadScreen.Initialize();
	m_ContentScreen.Initialize();
	m_LobbyScreen.Initialize( m_OfferScreen, m_ContentScreen, m_DownloadScreen );
	
	UIScreen::S_Init( &m_LobbyScreen );

	if ( FAILED(XOnlineStartup() ) )
	{
		ATG::FatalError( "Failed to start Xbox Live\n" );
	}

    // Initialize autologin
    ATG::SignIn::Initialize( 1, 1, TRUE, 1 );

	// Register the notification listener
	m_hNotificationSys = XNotifyCreateListener( XNOTIFY_SYSTEM );
	if ( NULL == m_hNotificationSys || INVALID_HANDLE_VALUE == m_hNotificationSys )
	{
		ATG::FatalError( "Failed to create notification listener.\n" );
	}

	m_hNotificationLiv = XNotifyCreateListener( XNOTIFY_LIVE );
	if ( NULL == m_hNotificationLiv || INVALID_HANDLE_VALUE == m_hNotificationLiv )
	{
		ATG::FatalError( "Failed to create notification listener.\n" );
	}

	// Initialize the sign-in state
	for( UINT i = 0; i < XUSER_MAX_COUNT; ++i )
	{
		XUserGetSigninInfo( i, XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY, &m_SignInInfo[ i ] );
	}

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating
//       the scene. Perform common tasks then call the appropriate procedure based on 
//       application state
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
	HRESULT hr = S_OK;

	ATG::SignIn::Update();

	// Check for signin change notifications
    DWORD dwNotificationID = 0;
    ULONG_PTR ulParam = NULL;
	
	if ( XNotifyGetNext( m_hNotificationLiv, XN_LIVE_CONTENT_INSTALLED, &dwNotificationID, &ulParam ) )
	{
		ContentManager::GetSingleton().OnContentInstalled();

		// Sign-in change: The fUserHasPurchased field in the OfferTable may have changed as a result
		// of the download. Need to enumerate offers
		OfferManager::GetSingleton().EnumerateOffers();

		// Need to reset the offer table
		m_OfferScreen.ResetTable();
	}

    if( XNotifyGetNext( m_hNotificationSys, XN_SYS_SIGNINCHANGED, &dwNotificationID, &ulParam ) )
    {
		XUSER_SIGNIN_INFO oldSignInInfo[XUSER_MAX_COUNT];
		memcpy( oldSignInInfo, m_SignInInfo, XUSER_MAX_COUNT * sizeof( XUSER_SIGNIN_INFO ) );

		for( UINT i = 0; i < XUSER_MAX_COUNT; ++i )
		{
			XUserGetSigninInfo( i, XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY, &m_SignInInfo[ i ] );
		}

		// Check for "spurious" signout notification (where all users appeared
		// signed out). If we have this, ignore
		static DWORD dwTick                     = 0;
		static const DWORD dwValidNotifInterval = 1000;
		BOOL bSpuriousNotif                     = TRUE;

		for( UINT i = 0; i < XUSER_MAX_COUNT; ++i )
		{
			if( m_SignInInfo[ i ].UserSigninState != eXUserSigninState_NotSignedIn )
			{
				bSpuriousNotif = FALSE;
			}
		}

		// Seems spurious. Now check how much time has passed since the last time
		// we got this. If this interval is greater than dwValidNotifInterval,
		// then don't trust it. Otherwise, trust it.
		if ( bSpuriousNotif )
		{
			if ( GetTickCount() - dwTick > dwValidNotifInterval )
			{
				dwTick = GetTickCount();
			}
			else
			{
				bSpuriousNotif = FALSE;
			}
		}

		// If signin info changed, then update everyone who is interested in sign-in changes
		if( !bSpuriousNotif && 
			memcmp( oldSignInInfo, m_SignInInfo, XUSER_MAX_COUNT * sizeof( XUSER_SIGNIN_INFO ) ) )
		{
			CHAR szUserName[XUSER_NAME_SIZE];
			XUserGetName( ATG::SignIn::GetSignedInUser(), szUserName, XUSER_NAME_SIZE);
			MultiByteToWideChar( CP_ACP, 0, szUserName, -1, m_wszUserName, XUSER_NAME_SIZE );

			// Sign-in change: The fUserHasPurchased field in the OfferTable is user dependent
			// Need to enumerate offers
			OfferManager::GetSingleton().EnumerateOffers();

			// Need to reset the offer table for the new user
			m_OfferScreen.ResetTable();
		}
	}

	// Update the Managers
	OfferManager::GetSingleton().Update();
	DownloadManager::GetSingleton().Update();
	ContentManager::GetSingleton().Update();

	// Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

	// Call the update function for the current ui
	UIScreen::S_Update(pGamepad);

	return hr;
}

//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Sets up render states, clears the viewport, and renders the scene. Perform
//       common tasks then call the appropriate procedure based on application state
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
	HRESULT hr = S_OK;

	// Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );
	
	// Prominently display the current signed-in user at the top of each screen
	UIHelpers::DrawText( 0, 0, UIHelpers::TEXT_COLOR, L"In-Game Store (%s)", m_wszUserName );

	// Call the rendering function for the current UI
	UIScreen::S_Render();

	// Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

	return hr;
}