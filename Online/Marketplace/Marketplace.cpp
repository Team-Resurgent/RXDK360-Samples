//--------------------------------------------------------------------------------------
// Marketplace.cpp
//
// Demonstrates how to display the various Marketplace UIs, retrieve offer counts,
// enumerate offers and consumables and consume assets.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <d3d9.h>
#include <winsockx.h>
#include <xonline.h>
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgResource.h"
#include "AtgSignIn.cpp"
#include "AtgUtil.h"


//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
const DWORD         TEXT_COLOR = 0xFFFFFFFF;

const float         CONTENT_PING_INTERVAL = 5.0f; // ping every 5 seconds

//
// UI description strings
//
const DWORD         NUM_UI_SETTINGS = 4;

WCHAR*              g_strUIDesc[ NUM_UI_SETTINGS ] =
{
    L"Display Marketplace Content UI",
    L"Display Marketplace Membership UI",
    L"Display Marketplace Free Items UI",
    L"Display Marketplace Paid Items UI"
};

// Max number of offers and assets to enumerate
const DWORD         MAX_ENUMERATION_RESULTS = 10;


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nSelected UI" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_1, L"Signin" },
    { ATG::HELP_X_BUTTON, ATG::HELP_PLACEMENT_1, L"Enumerate Offers" },
    { ATG::HELP_Y_BUTTON, ATG::HELP_PLACEMENT_1, L"Consume Assets" },
    { ATG::HELP_DPAD, ATG::HELP_PLACEMENT_2, L"Change UI or\nOffer Selection" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );

//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    ATG::Timer m_Timer;
    HRESULT m_hrOverlappedResult;
    XOVERLAPPED m_Overlapped;

    FLOAT m_fContentPingTime;
    XOFFERING_CONTENTAVAILABLE_RESULT m_CachedContentCount;

    DWORD m_dwCurrentUISetting;

    DWORD m_dwSelectedOffer;
    XMARKETPLACE_CONTENTOFFER_INFO* m_pOfferData;
    DWORD m_dwOfferCount;

    XMARKETPLACE_ASSET_ENUMERATE_REPLY* m_pAssetData;
    DWORD m_dwAssetCount;

private:
    VOID            EnumerateOffers();
    VOID            ConsumeAssets();

    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
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
    m_bDrawHelp = FALSE;


    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Start up Xbox Live functionality using default Secure Network Layer settings
    if( XOnlineStartup() != ERROR_SUCCESS )
    {
        ATG::FatalError( "Failed to start Xbox Live\n" );
    }

    // Initialize autologin
    ATG::SignIn::Initialize( 1, 1, TRUE, 1 );

    m_Timer.Reset();
    m_Timer.Start();

    ZeroMemory( &m_CachedContentCount, sizeof( XOFFERING_CONTENTAVAILABLE_RESULT ) );
    m_fContentPingTime = 0.0f;

    m_dwCurrentUISetting = 0;

    m_pOfferData = NULL;
    m_dwOfferCount = 0;
    m_pAssetData = NULL;
    m_dwAssetCount = 0;

    m_dwSelectedOffer = ( DWORD )-1;

    m_hrOverlappedResult = S_OK;
    ZeroMemory( &m_Overlapped, sizeof( XOVERLAPPED ) );
    m_Overlapped.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );

    if( m_Overlapped.hEvent == NULL )
    {
        ATG::FatalError( "Failed to call Overlapped event.\n" );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    HRESULT hr = S_OK;

    ATG::SignIn::Update();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    BOOL bPingContent = FALSE;

    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    m_fContentPingTime -= fElapsedTime;
    if( m_fContentPingTime < 0.0f )
    {
        bPingContent = TRUE;
        m_fContentPingTime = CONTENT_PING_INTERVAL;
    }

    // Check to see if user has signed in
    if( ATG::SignIn::AreUsersSignedIn() )
    {
        // Get the current number of new and existing offerings for the player
        // XContentGetMarketplaceCounts will only return non-consumable offer counts
        if( bPingContent )
        {
            hr = XContentGetMarketplaceCounts( ATG::SignIn::GetSignedInUser(), 0xffffffff,
                                               sizeof( XOFFERING_CONTENTAVAILABLE_RESULT ),
                                               &m_CachedContentCount, NULL );
            assert( hr == ERROR_SUCCESS );
        }

        // Move UI Selection
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        {
            m_dwCurrentUISetting++;
            if( m_dwCurrentUISetting == NUM_UI_SETTINGS )
                m_dwCurrentUISetting = 0;
        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        {
            m_dwCurrentUISetting--;
            if( m_dwCurrentUISetting == -1 )
                m_dwCurrentUISetting = NUM_UI_SETTINGS - 1;
        }

        // Change Offer selection
        if( m_dwOfferCount > 0 )
        {
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
            {
                if( m_dwSelectedOffer ) m_dwSelectedOffer--;
            }

            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
            {
                if( m_dwSelectedOffer < ( m_dwOfferCount - 1 ) ) m_dwSelectedOffer++;
            }
        }

        // Show various Marketplace UIs
        if( !ATG::SignIn::IsSystemUIShowing() && pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            DWORD dwErr = ERROR_SUCCESS;

            switch( m_dwCurrentUISetting )
            {
                case 0:
                    dwErr = XShowMarketplaceUI( ATG::SignIn::GetSignedInUser(),
                                                XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST, 0, 0 );
                    assert( dwErr == ERROR_SUCCESS );
                    break;

                case 1:
                    dwErr = XShowMarketplaceUI( ATG::SignIn::GetSignedInUser(),
                                                XSHOWMARKETPLACEUI_ENTRYPOINT_MEMBERSHIPLIST, 0, 0 );
                    assert( dwErr == ERROR_SUCCESS );
                    break;

                    //
                    // Show the dowloadable items UI. The list of offer IDs passed to it allows the player to download any of
                    // those specific offers. The offer IDs are acuired through the XMarketplaceCreateOfferEnumerator API.
                    // Before calling downloadable items UI, you must determine what items has the player downloaded in order
                    // to not accidentally double charge with the paid items UI, and to make sure that consumable items can
                    // be downloaded again.
                    // To determine if a consumable item has been previously purchased, use the XContent APIs to enumerate downloaded
                    // Marketplace content, then pass the XCONTENT_DATA structs to XMarketplaceDoesContentIdMatch.
                    // XMarketplaceDoesContentIdMatch will return TRUE if the player has a particular offer.
                    //
                case 2:
                    // Show the free downloadable items UI. You must only specify free items or consumable items that were previously purchased.
                    // Use XMarketplaceDoesContentIdMatch to determine what items has the user downloaded.
                    if( m_dwSelectedOffer != ( DWORD )-1 )
                    {
                        dwErr = XShowMarketplaceDownloadItemsUI( ATG::SignIn::GetSignedInUser(),
                                                                 XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_FREEITEMS,
                                                                 &m_pOfferData[m_dwSelectedOffer].qwOfferID,
                                                                 1, &m_hrOverlappedResult, &m_Overlapped );
                        assert( dwErr == ERROR_IO_PENDING );
                    }
                    break;

                case 3:
                    // Show the buyable downloadable items UI. The user will be charged for anything selectect through this UI. In case of 
                    // consumables that were previously purchased, if they are passed in here, the user will be charged again. Do NOT pass
                    // consumable items that were previously purchased here. Use XMarketplaceDoesContentIdMatch to determine what items
                    // has the user downloaded.
                    if( m_dwSelectedOffer != ( DWORD )-1 )
                    {
                        dwErr = XShowMarketplaceDownloadItemsUI( ATG::SignIn::GetSignedInUser(),
                                                                 XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_PAIDITEMS,
                                                                 &m_pOfferData[m_dwSelectedOffer].qwOfferID,
                                                                 1, &m_hrOverlappedResult, &m_Overlapped );
                        assert( dwErr == ERROR_IO_PENDING );
                    }
                    break;
            }
        }

        // Enumerate consumable offers
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        {
            EnumerateOffers();
        }

        // Consume assets
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        {
            ConsumeAssets();
            EnumerateOffers();
        }
    }

    // Show signin UI
    if( !ATG::SignIn::IsSystemUIShowing() && pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        ATG::SignIn::ShowSignInUI();
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    HRESULT hr = S_OK;

    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"Marketplace" );
        m_Font.End();

        m_Font.Begin();

        WCHAR wchRender[ 1024 ]; // text buffer

        swprintf_s( wchRender, GLYPH_A_BUTTON L" %s", g_strUIDesc[m_dwCurrentUISetting] );
        m_Font.DrawText( 0, -70, TEXT_COLOR, wchRender );

        m_Font.DrawText( 0, -30, TEXT_COLOR, GLYPH_X_BUTTON L" Enumerate Offers" );
        m_Font.DrawText( 300, -30, TEXT_COLOR, GLYPH_Y_BUTTON L" Consume Assets" );

        // List enumerated offers & assets
        m_Font.DrawText( 40, 50, 0xffffff00, L"Offer Name" );
        m_Font.DrawText( 350, 50, 0xffffff00, L"Price" );
        m_Font.DrawText( 500, 50, 0xffffff00, L"Available Qty" );

        for( DWORD i = 0; i < m_dwOfferCount; i++ )
        {
            FLOAT sy = ( FLOAT )( 80 + ( i * 30 ) );
            if( i == m_dwSelectedOffer )
            {
                m_Font.DrawText( 0, sy, TEXT_COLOR, GLYPH_RIGHT_ARROW );
            }

            m_Font.DrawText( 40, sy, TEXT_COLOR, m_pOfferData[i].wszOfferName );

            swprintf_s( wchRender, L"%d", m_pOfferData[i].dwPointsPrice );
            m_Font.DrawText( 350, sy, TEXT_COLOR, wchRender );

            // Search for matching asset to retrieve available qty
            DWORD dwQty = 0;
            for( DWORD k = 0; k < m_pAssetData->assetPackage.cAssets; k++ )
            {
                if( m_pAssetData->assetPackage.aAssets[k].dwAssetID == m_pOfferData[i].dwAssetID )
                {
                    dwQty = m_pAssetData->assetPackage.aAssets[k].dwQuantity;
                    break;
                }
            }

            swprintf_s( wchRender, L"%d", dwQty );
            m_Font.DrawText( 500, sy, TEXT_COLOR, wchRender );
        }

        // Display number of offers
        swprintf_s( wchRender, L"User %d: %d offers, %d new", ATG::SignIn::GetSignedInUser(),
                    m_CachedContentCount.dwTotalOffers,
                    m_CachedContentCount.dwNewOffers );
        m_Font.DrawText( 0, 0, TEXT_COLOR, wchRender, ATGFONT_RIGHT );

        // Display download status of offer (if any)
        DWORD dwStatus;
        if( m_dwSelectedOffer != ( DWORD )-1 &&
            XMarketplaceGetDownloadStatus( ATG::SignIn::GetSignedInUser(),
                                           m_pOfferData[m_dwSelectedOffer].qwOfferID, &dwStatus ) == ERROR_SUCCESS )
        {
            wchRender[0] = 0;

            switch( dwStatus )
            {
                case ERROR_IO_PENDING:
                    swprintf_s( wchRender, L"Downloading..." );
                    break;
                case ERROR_NOT_FOUND:
                    swprintf_s( wchRender, L"Offer not found" );
                    break;
                case ERROR_DISK_FULL:
                    swprintf_s( wchRender, L"Disk is full" );
                    break;
            }

            m_Font.DrawText( 0, 30, TEXT_COLOR, wchRender, ATGFONT_RIGHT );
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: EnumerateOffers
// Desc: Enumerate available offers. Here we enumerate only consumable offers only.
//--------------------------------------------------------------------------------------
VOID Sample::EnumerateOffers()
{
    DWORD dwErr;
    DWORD cbBuffer;
    HANDLE hEnumeration;

    // Enumerate at most MAX_ENUMERATION_RESULTS consumable items
    dwErr = XMarketplaceCreateOfferEnumerator( ATG::SignIn::GetSignedInUser(), XMARKETPLACE_OFFERING_TYPE_CONSUMABLE,
                                               0xffffffff, MAX_ENUMERATION_RESULTS, &cbBuffer, &hEnumeration );

    assert( dwErr == ERROR_SUCCESS );

    if( !m_pOfferData )
    {
        m_pOfferData = ( XMARKETPLACE_CONTENTOFFER_INFO* )new BYTE[cbBuffer];
    }
    ZeroMemory( m_pOfferData, cbBuffer );

    dwErr = XEnumerate( hEnumeration, ( VOID* )m_pOfferData, cbBuffer, &m_dwOfferCount, NULL );

    m_dwSelectedOffer = 0;

    // No data exists for enumeration
    if( dwErr == ERROR_NO_MORE_FILES )
    {
        m_dwOfferCount = 0;
        m_dwSelectedOffer = ( DWORD )-1;
        dwErr = ERROR_SUCCESS;
    }

    assert( dwErr == ERROR_SUCCESS );

    CloseHandle( hEnumeration );

    // Enumerate consumable assets. This is needed to retreive the current available quantity for each asset
    dwErr = XMarketplaceCreateAssetEnumerator( ATG::SignIn::GetSignedInUser(),
                                               MAX_ENUMERATION_RESULTS, &cbBuffer, &hEnumeration );

    assert( dwErr == ERROR_SUCCESS );

    if( !m_pAssetData )
    {
        m_pAssetData = ( XMARKETPLACE_ASSET_ENUMERATE_REPLY* )new BYTE[cbBuffer];
    }
    ZeroMemory( m_pAssetData, cbBuffer );

    dwErr = XEnumerate( hEnumeration, ( VOID* )m_pAssetData, cbBuffer, &m_dwAssetCount, NULL );

    // No data exists for enumeration
    if( dwErr == ERROR_NO_MORE_FILES )
    {
        m_dwAssetCount = 0;
        dwErr = ERROR_SUCCESS;
    }

    assert( dwErr == ERROR_SUCCESS );

    CloseHandle( hEnumeration );
}


//--------------------------------------------------------------------------------------
// Name: ConsumeAssets
// Desc: Consume the first enumerated asset by decrementing its available qty.
//--------------------------------------------------------------------------------------
VOID Sample::ConsumeAssets()
{
    if( m_dwAssetCount > 0 && m_dwSelectedOffer != ( DWORD )-1 )
    {
        DWORD dwErr;

        XMARKETPLACE_ASSET Asset = {0};

        // Search for asset ID
        for( DWORD i = 0; i < m_pAssetData->assetPackage.cAssets; i++ )
        {
            if( m_pAssetData->assetPackage.aAssets[i].dwAssetID == m_pOfferData[m_dwSelectedOffer].dwAssetID )
            {
                Asset.dwAssetID = m_pAssetData->assetPackage.aAssets[i].dwAssetID;
                Asset.dwQuantity = 1;       // Consume one item
                break;
            }
        }

        dwErr = XMarketplaceConsumeAssets( ATG::SignIn::GetSignedInUser(), 1, &Asset, NULL );

        assert( dwErr == ERROR_SUCCESS );
    }
    else
    {
        ATG::DebugSpew( "Asset could not be consumed.\n" );
    }
}
