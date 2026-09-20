//--------------------------------------------------------------------------------------
// InGameContentManager.cpp
//
// Tracks the set of installed content.
// Demonstrates how to correlate Marketplace Offers with installed Content Packages
// Demonstrates how to mount Content Packages and retrieve the license mask.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <assert.h>
#include <stdio.h>
#include <xtl.h>
#include <xonline.h>
#include "AtgSignIn.h"
#include "AtgUtil.h"

#include "InGameContentManager.h"
#include "InGameOfferManager.h"
#include "Enumerator.h"
#include "AtgConsole.h"


//--------------------------------------------------------------------------------------
// ContentManager implementation
//--------------------------------------------------------------------------------------

// Stores the single instance of this class
ContentManager *ContentManager::s_TheContentManager = NULL;

//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Checks to see if the enumerator is ready and if so updates the enumerator
//--------------------------------------------------------------------------------------
VOID
ContentManager::Update()
{
    if ( m_ContentEnumerator.IsInitialized() )
    {
        // If the enumeration has finished then Update will simply return
        // ERROR_NO_MORE_FILES without doing anything else. For simplicity,
        // continue to call Update once the enumerator is initialized
        m_ContentEnumerator.Update();
    }
    else
    {
        DWORD dwUser = ATG::SignIn::GetSignedInUser();
        if ( (dwUser < 4) && (dwUser >= 0) )
        {
            // Initialize the next enumeration
            // Enumerator will become uninitialized in OnContentInstalled
            m_ContentEnumerator.Initialize();
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: OnContentInstalled
// Desc: This method is called in response to XN_LIVE_CONTENT_INSTALLED
//       Kicks off another enumeration to discover new content packages
//--------------------------------------------------------------------------------------
VOID
ContentManager::OnContentInstalled()
{
    // Cancelling the enumerator will restore it to the unitialized state.
    // Update will re-initialize the enumerator and start the next enumeration.
    m_ContentEnumerator.Cancel();

     // Clear out the collection prior to repopulating with more enumeration results
    m_aContentData.clear();
}

//--------------------------------------------------------------------------------------
// ContentEnumerator implementation
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: Create
// Desc: Calls XContentCreateEnumerator to create the enumeration handle
//       and computes the size of the enumeration buffer
//--------------------------------------------------------------------------------------
HRESULT
ContentEnumerator::Create(HANDLE& hEnumeration, DWORD& dwBufferSize)
{
    // Enumerate at most MAX_ENUMERATION_RESULTS items
    DWORD dwError = XContentCreateEnumerator( ATG::SignIn::GetSignedInUser()
                                            , XCONTENTDEVICE_ANY
                                            , XCONTENTTYPE_MARKETPLACE
                                            , NULL
                                            , MAX_ENUMERATION_RESULTS
                                            , &dwBufferSize
                                            , &hEnumeration );

    return HRESULT_FROM_WIN32( dwError );
}

//--------------------------------------------------------------------------------------
// Name: OnData
// Desc: Copies XCONTENT_DATA from the enumeratin buffer to the ContentManager
//--------------------------------------------------------------------------------------
VOID
ContentEnumerator::OnData( DWORD dwCount, VOID* pBuffer )
{
    PXCONTENT_DATA pTempContentData = ( PXCONTENT_DATA )pBuffer;
    for ( DWORD dw = 0; dw < dwCount; ++dw )
    {
        ContentManager::GetSingleton().m_aContentData.push_back( pTempContentData[dw] );
    }
}


//--------------------------------------------------------------------------------------
// ContentScreen implementation
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Adds all the buttons to the screen
//--------------------------------------------------------------------------------------
VOID
ContentScreen::Initialize()
{
	CreateBackButton( UIHelpers::s_fBUTTON_LEFT_X,
		              UIHelpers::s_fBUTTON_LOWER_Y,
                      XINPUT_GAMEPAD_B,
                      GLYPH_B_BUTTON L"Back to Game Lobby" );

    m_pAssociatedOffersButton = 
		CreateButton( UIHelpers::s_fBUTTON_RIGHT_X,
		              UIHelpers::s_fBUTTON_UPPER_Y,
                      XINPUT_GAMEPAD_X,
                      GLYPH_X_BUTTON L"Show Associated Offers",
					  static_cast< UIBUTTON_HANDLER >( &ContentScreen::AssociatedOffers ),
                      TRUE );

    m_pMountPackageButton =
		CreateButton( UIHelpers::s_fBUTTON_RIGHT_X,
		              UIHelpers::s_fBUTTON_LOWER_Y,
					  XINPUT_GAMEPAD_Y,
					  GLYPH_Y_BUTTON L"Mount Content Package",
					  static_cast< UIBUTTON_HANDLER >( &ContentScreen::MountPackage ),
					  TRUE );
}

//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Draws all the controls on the screen
//--------------------------------------------------------------------------------------
VOID
ContentScreen::Render()
{
	UIHelpers::DrawText( -300, 0, UIHelpers::TEXT_COLOR, L"Installed Marketplace Content" );

	m_ContentTable.Render( UIHelpers::TEXT_COLOR, UIHelpers::SELECTED_TEXT_COLOR );

    RenderButtons();
}

//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Updates controls and button visibilty depending on whether the list is empty
//--------------------------------------------------------------------------------------
VOID
ContentScreen::Update( ATG::GAMEPAD* pGamepad )
{
    if ( ContentManager::GetSingleton().Count() > 0 )
    {
        m_pAssociatedOffersButton->Hide( FALSE );
        m_pMountPackageButton->Hide( FALSE );
    }
    else
    {
        m_pAssociatedOffersButton->Hide( TRUE );
        m_pMountPackageButton->Hide( TRUE );
    }

    m_ContentTable.Update( pGamepad );		
    UpdateButtons( pGamepad );
}

//--------------------------------------------------------------------------------------
// Name: AssociatedOffers
// Desc: Brings up a screen listing all offers associated with
//       the selected XCONTENT_DATA
//--------------------------------------------------------------------------------------
VOID
ContentScreen::AssociatedOffers( WORD wButtonCode )
{
    // Get the currently selected XCONTENT_DATA
    DWORD dw = m_ContentTable.GetSelectionIndex();	
    const XCONTENT_DATA& ContentData = ContentManager::GetSingleton()[dw];

    // Instantiante and populate the screen to list all associated offers
    AssociatedOffersScreen *pAssocOfferScreen = new AssociatedOffersScreen();
    pAssocOfferScreen->FindAssociatedOffers( &ContentData );

    // Show the new screen
    Navigate( pAssocOfferScreen );
}

const CHAR PACKAGE_ROOT_NAME[] = "PackageRoot";

//--------------------------------------------------------------------------------------
// Name: MountPackage
// Desc: Mounts the Content Package associated with the currently selected XContentData
//       Brings up a screen showing the error code and the license mask
//--------------------------------------------------------------------------------------
VOID
ContentScreen::MountPackage( WORD wButtonCode )
{
    DWORD dw = m_ContentTable.GetSelectionIndex();
    const XCONTENT_DATA& contentData = ContentManager::GetSingleton()[dw];

    DWORD dwLicenseMask;
    
    DWORD dwErr = XContentCreate(
        ATG::SignIn::GetSignedInUser(),
        PACKAGE_ROOT_NAME,               // Root of the path to any files in the package
        &contentData,                    // Identifies the package to open
        XCONTENTFLAG_OPENEXISTING,
        NULL,
        &dwLicenseMask,                  // Receives the license mask if the call is successful
        NULL );

    // Instantiate the screen to show the results
    LicenseMaskScreen *pLMScreen = new LicenseMaskScreen( dwErr, dwLicenseMask );

    // Show the new screen
    Navigate( pLMScreen );
}


//--------------------------------------------------------------------------------------
// LicenseMaskScreen Implementation
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: Render
// Desc: If the call to XContentCreate succeeded then show the license mask
//       Otherwise, display error information
//--------------------------------------------------------------------------------------
VOID
LicenseMaskScreen::Render()
{
	UIHelpers::DrawText( -200, 0, UIHelpers::TEXT_COLOR, L"License Mask" );

    FLOAT fOriginX = 50.0f;
    FLOAT fOriginY = 50.0f;
    
    if ( ERROR_SUCCESS == m_dwCreateError )
    {
		UIHelpers::DrawText( fOriginX, fOriginY, UIHelpers::TEXT_COLOR,
			L"License Mask: 0x%08X", m_dwLicenseMask );
    }
    else if ( ERROR_ACCESS_DENIED == m_dwCreateError )
    {
		UIHelpers::DrawText( fOriginX, fOriginY, UIHelpers::TEXT_COLOR_RED,
			L"User does not have access to the package." );
    }
    else
    {
		UIHelpers::DrawText( fOriginX, fOriginY, UIHelpers::TEXT_COLOR_RED,
			L"Unexpected error mounting the package: 0x%08X", m_dwCreateError );
    }

    RenderButtons();
}

//--------------------------------------------------------------------------------------
// Name: NavigateBack
// Desc: Clean up the instance of the current screen and then show the previous screen
//--------------------------------------------------------------------------------------
VOID
LicenseMaskScreen::NavigateBack()
{
	DWORD dwErr = ERROR_SUCCESS;
	dwErr = XContentClose( PACKAGE_ROOT_NAME, NULL );
	assert( ERROR_SUCCESS == dwErr );
	UIScreen::NavigateBack();
    delete this;
}

//--------------------------------------------------------------------------------------
// ContentTableControl implementation
//--------------------------------------------------------------------------------------
const FLOAT ContentTableControl::DISPLAY_NAME_WIDTH = 35.0f;
const FLOAT ContentTableControl::FILE_NAME_WIDTH    = 65.0f;

ContentTableControl::ContentTableControl() : TableControl( 0.0f, 50.0f )
{
	AddColumnHeading( ContentTableControl::DISPLAY_NAME_WIDTH, L"Display Name" );
	AddColumnHeading( ContentTableControl::FILE_NAME_WIDTH, L"File Name" );
}

//--------------------------------------------------------------------------------------
// Name: RenderOneRow
// Desc: Render one row of the table
//--------------------------------------------------------------------------------------
VOID
ContentTableControl::RenderOneRow( INT iWhichRow )
{
	XCONTENT_DATA rec = ContentManager::GetSingleton()[iWhichRow];

    // Draw the Display Name value
	DrawRowElement( rec.szDisplayName );

    // Draw the File Name value
    WCHAR wbuf[XCONTENT_MAX_FILENAME_LENGTH];
	DWORD dwLen = strlen( rec.szFileName );
	wbuf[dwLen] = L'\0';
	MultiByteToWideChar( CP_ACP, 0, rec.szFileName, dwLen, wbuf, XCONTENT_MAX_FILENAME_LENGTH );
    
	DrawRowElement( wbuf );
}

//--------------------------------------------------------------------------------------
// AssociatedOfferList implementation
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: RenderOneRow
// Desc: Render one row of the table
//--------------------------------------------------------------------------------------
VOID
AssociatedOfferList::RenderOneRow( INT iWhichRow )
{
	unsigned idx = m_aOfferIndices[iWhichRow];
	const XMARKETPLACE_CONTENTOFFER_INFO& rec = OfferManager::GetSingleton()[idx];
	__super::RenderOneRow( rec );
}

//--------------------------------------------------------------------------------------
// AssociatedOffersScreen implementation
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: FindAssociatedOffers
// Desc: Iterate through all the offers maintained by the OfferManager
//       and check each one against the provided XCONTENT_DATA using the
//       XMarketplaceDoesContentIdMatch API
//--------------------------------------------------------------------------------------
VOID
AssociatedOffersScreen::FindAssociatedOffers( const XCONTENT_DATA* pContentData )
{
    for (DWORD dw = 0; dw < OfferManager::GetSingleton().Count(); ++dw)
    {
        const XMARKETPLACE_CONTENTOFFER_INFO &offerInfo = OfferManager::GetSingleton()[dw];
        if ( XMarketplaceDoesContentIdMatch(offerInfo.contentId, pContentData) )
        {
            m_AssociatedOffers.m_aOfferIndices.push_back(dw);
        }
    }
}