//--------------------------------------------------------------------------------------
// InGameOfferManager.cpp
//
// Enumerates the list of offers available on the service.
// Presents offer data to the user and enables inspecting offer details.
// Collects the OfferIDs from user input and initiates downloading.
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

#include "Enumerator.h"
#include "InGameOfferManager.h"
#include "InGameDownloadManager.h"

// The single OfferManager instance
OfferManager *OfferManager::s_TheOfferManager = NULL;

//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Checks to see if the enumerator is ready and if so updates the enumerator
//--------------------------------------------------------------------------------------
VOID
OfferManager::Update()
{
	// If the enumeration has finished then Update will simply return
	// ERROR_NO_MORE_FILES without doing anything else. For simplicity,
	// continue to call Update once the enumerator is initialized
	if ( m_OfferEnumerator.IsInitialized() )
	{
		m_OfferEnumerator.Update();
	}
	else
	{
		// Initialize the next enumeration
		// Enumerator will become uninitialized in OnSignInChange
		DWORD dwUser = ATG::SignIn::GetSignedInUser();
		if ( (dwUser < 4) && (dwUser >= 0) )
		{
			m_OfferEnumerator.Initialize();
		}
	}
}

//--------------------------------------------------------------------------------------
// Name: EnumerateOffers
// Desc: Cancel any pending offer enumeration and start over
//--------------------------------------------------------------------------------------
VOID
OfferManager::EnumerateOffers()
{
	m_OfferEnumerator.Cancel();
	ClearOffers();
}

//--------------------------------------------------------------------------------------
// Name: _AddOffer
// Desc: Create a copy of the referenced offer and add it to the array of offers during
//       enumeration.
//--------------------------------------------------------------------------------------
VOID
OfferManager::AddOffer( const XMARKETPLACE_CONTENTOFFER_INFO& Offer )
{
	// Caculate the size needed for the structure
	DWORD dwBufferSize = sizeof( XMARKETPLACE_CONTENTOFFER_INFO );
	// arrange for extra storage at the end to hold the three strings
	dwBufferSize += Offer.dwOfferNameLength;
	dwBufferSize += Offer.dwTitleNameLength;
	dwBufferSize += Offer.dwSellTextLength;

	// Allocate and initialize
	PXMARKETPLACE_CONTENTOFFER_INFO pOfferData = (PXMARKETPLACE_CONTENTOFFER_INFO)new BYTE[dwBufferSize];
	ZeroMemory( pOfferData, dwBufferSize );
	XMemCpy( pOfferData, &Offer, sizeof( XMARKETPLACE_CONTENTOFFER_INFO ) );
	
	// Copy the Offer Name
	pOfferData->wszOfferName = (WCHAR *)&pOfferData[1];
	XMemCpy(pOfferData->wszOfferName, Offer.wszOfferName, Offer.dwOfferNameLength);

	// Copy the Sell Text
	pOfferData->wszSellText = pOfferData->wszOfferName + Offer.dwOfferNameLength/2;
	XMemCpy(pOfferData->wszSellText, Offer.wszSellText, Offer.dwSellTextLength);

	// Copy the Title Name
	pOfferData->wszTitleName = pOfferData->wszSellText  + Offer.dwSellTextLength/2;
	XMemCpy(pOfferData->wszTitleName, Offer.wszTitleName, Offer.dwTitleNameLength);

	// Add the record to the table
	m_aOfferData.push_back(pOfferData);
}

//--------------------------------------------------------------------------------------
// Name: ClearOffers
// Desc: Free each structure in the collection then clear the collection
//--------------------------------------------------------------------------------------
VOID
OfferManager::ClearOffers()
{
	for (unsigned i = 0; i < m_aOfferData.size(); ++i)
	{
		delete [] (BYTE *)m_aOfferData[i];
	}
	m_aOfferData.clear();
}

//--------------------------------------------------------------------------------------
// OfferEnumerator implementation
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: Create
// Desc: Creates the enumeration handle for enumerating offers. Specify the type of
//       offers to enumerate by combining flags from XMARKETPLACE_OFFERING_TYPE
//       defined in xonline.h
//--------------------------------------------------------------------------------------
HRESULT
OfferEnumerator::Create(HANDLE& hEnumeration, DWORD& dwBufferSize)
{
	// Enumerate at most MAX_ENUMERATION_RESULTS items
	DWORD dwError = XMarketplaceCreateOfferEnumerator
		(
		ATG::SignIn::GetSignedInUser(),       // Get the offer data for this player
		XMARKETPLACE_OFFERING_TYPE_CONTENT,   // Marketplace content (can combine values using ||)
		0xffffffff,                           // Retrieve all content catagories
		MAX_ENUMERATION_RESULTS,              // Number of results per call to XEnumerate
		&dwBufferSize,                        // Size of buffer needed for results
		&hEnumeration                         // Enumeration handle
		);

	return HRESULT_FROM_WIN32( dwError );
}

//--------------------------------------------------------------------------------------
// Name: OnData
// Desc: Copies Marketplace offer data from the enumeration buffer to the OfferManager
//--------------------------------------------------------------------------------------
VOID
OfferEnumerator::OnData( DWORD dwCount, VOID* pBuffer )
{
	PXMARKETPLACE_CONTENTOFFER_INFO pTempOfferData = (PXMARKETPLACE_CONTENTOFFER_INFO)pBuffer;
	for (DWORD dw = 0; dw < dwCount; ++dw)
	{
		OfferManager::GetSingleton().AddOffer( pTempOfferData[dw] );
	}
}

//--------------------------------------------------------------------------------------
// OfferScreen implementation
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Add all the buttons to the screen
//--------------------------------------------------------------------------------------
VOID
OfferScreen::Initialize()
{
	m_OfferList.Initialize();

	CreateBackButton( UIHelpers::s_fBUTTON_LEFT_X,
		              UIHelpers::s_fBUTTON_LOWER_Y,
					  XINPUT_GAMEPAD_B,
					  GLYPH_B_BUTTON L"Back to Game Lobby" );

	m_pDownloadOfferButton =
		CreateButton( UIHelpers::s_fBUTTON_RIGHT_X,
		              UIHelpers::s_fBUTTON_UPPER_Y,
					  XINPUT_GAMEPAD_X,
					  GLYPH_X_BUTTON L"Download Selected Items",
					  static_cast< UIBUTTON_HANDLER >( &OfferScreen::DownloadSelections ),
					  TRUE );
	
	m_pSelectForDownloadButton =
		CreateButton( UIHelpers::s_fBUTTON_RIGHT_X,
		              UIHelpers::s_fBUTTON_MIDDLE_Y,
					  XINPUT_GAMEPAD_DPAD_LEFT,
					  GLYPH_LEFT_ARROW L"Toggle Item Selection",
					  static_cast< UIBUTTON_HANDLER >( &OfferScreen::ToggleOfferSelection ),
					  TRUE);

	m_pOfferDetailsButton = 
		CreateButton( UIHelpers::s_fBUTTON_RIGHT_X,
		              UIHelpers::s_fBUTTON_LOWER_Y,
					  XINPUT_GAMEPAD_Y,
					  GLYPH_Y_BUTTON L"Offer Details",
					  static_cast< UIBUTTON_HANDLER >( &OfferScreen::OfferDetails ),
					  TRUE );	
}

//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Draw the screen title, render the offer list and the buttons
//--------------------------------------------------------------------------------------
VOID
OfferScreen::Render()
{
	UIHelpers::DrawText( -200, 0, UIHelpers::TEXT_COLOR, L"Current Offers" );
	
	m_OfferList.Render( UIHelpers::TEXT_COLOR, UIHelpers::SELECTED_TEXT_COLOR );

	RenderButtons();
}

//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Hides buttons where there are no visible offers in the list, updates the
//       offer list, updates all the buttons
//--------------------------------------------------------------------------------------
VOID
OfferScreen::Update( ATG::GAMEPAD* pGamepad )
{
	if ( OfferManager::GetSingleton().Count() > 0 )
	{
		m_pOfferDetailsButton->Hide( FALSE );
		m_pDownloadOfferButton->Hide(!m_OfferList.GetCheckmarkCount());
		m_pSelectForDownloadButton->Hide( FALSE );
	}
	else
	{
		m_pOfferDetailsButton->Hide( TRUE );
		m_pDownloadOfferButton->Hide( TRUE );
		m_pSelectForDownloadButton->Hide( TRUE );
	}
	m_OfferList.Update( pGamepad );

	UpdateButtons( pGamepad );
}

//--------------------------------------------------------------------------------------
// Name: OfferDetails
// Desc: Get the XMARKETPLACE_CONTENTOFFER_INFO for the current selection and then
//       display all the fields in detail
//--------------------------------------------------------------------------------------
VOID
OfferScreen::OfferDetails( WORD wButtonCode )
{
	DWORD dw = m_OfferList.GetSelectionIndex();
	const XMARKETPLACE_CONTENTOFFER_INFO* pOffer = &OfferManager::GetSingleton()[dw];
	Navigate( new OfferDetailsScreen( pOffer ) );
}

//--------------------------------------------------------------------------------------
// Name: ToggleOfferSelection
// Desc: Toggle the checkmark for the currently selected item
//--------------------------------------------------------------------------------------
VOID
OfferScreen::ToggleOfferSelection(WORD wButtonCode)
{
	m_OfferList.ToggleCheckmark();
}

//--------------------------------------------------------------------------------------
// Name: DownloadItemSelections
// Desc: User has selected possibly several offers to download.
//       Pass the set of OfferIDs to the DownloadManager in order to download the DLC
//--------------------------------------------------------------------------------------
VOID OfferScreen::DownloadSelections( WORD wButtonCode )
{
	DownloadRequest *pRequest = DownloadManager::GetSingleton().RequestDownload( m_OfferList.GetCheckmarkCount() );
	
	OfferSelectionTable::IndexSet::iterator it_end = m_OfferList.end();
	for ( OfferSelectionTable::IndexSet::iterator iter = m_OfferList.begin(); iter != it_end; ++iter )
	{
		pRequest->AddOffer( OfferManager::GetSingleton()[*iter].qwOfferID );
	}

	DownloadManager::GetSingleton().DownloadItems(pRequest);

	ResetTable();
}

//--------------------------------------------------------------------------------------
// OfferSelectionTable implementation
//--------------------------------------------------------------------------------------

const FLOAT OfferSelectionTable::CHECKBOX_WIDTH   = 50.0f;

//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Set up the column headers
//--------------------------------------------------------------------------------------
VOID
OfferSelectionTable::Initialize()
{
	AddColumnHeading( OfferSelectionTable::CHECKBOX_WIDTH, L"" );
	OfferTableControl::Initialize();
}

//--------------------------------------------------------------------------------------
// Name: RenderOneRow
// Desc: Render one row of the table
//--------------------------------------------------------------------------------------
VOID
OfferSelectionTable::RenderOneRow( INT iWhichRow )
{
	const XMARKETPLACE_CONTENTOFFER_INFO &rec = OfferManager::GetSingleton()[iWhichRow];
	
	// Draw the checkbox
	IndexSet::iterator iter = m_sCheckmarkedItems.find( iWhichRow );
	DrawRowElement( ( WCHAR* )( ( iter == m_sCheckmarkedItems.end() ) ? L"" : GLYPH_CHECK_MARK ) );
	
	__super::RenderOneRow( rec );
}

//--------------------------------------------------------------------------------------
// Name: ToggelCheckmark
// Desc: Toggles the checkmark for the current item
//--------------------------------------------------------------------------------------
VOID
OfferSelectionTable::ToggleCheckmark()
{
	unsigned idx = GetSelectionIndex();
	IndexSet::iterator iter = m_sCheckmarkedItems.find(idx);
	if (iter == m_sCheckmarkedItems.end())
	{
		//------------------------------------------------------------------
		// NOTE: XShowMarketplaceDownloadItemsUI will not accept more than
		//       XMARKETPLACE_MAX_OFFERIDS OfferIDs. You must add logic to
		//       your code to check for this and handle it gracefully.
		//------------------------------------------------------------------
		assert( m_sCheckmarkedItems.size() < XMARKETPLACE_MAX_OFFERIDS );
		if ( m_sCheckmarkedItems.size() < XMARKETPLACE_MAX_OFFERIDS )
		{
			m_sCheckmarkedItems.insert(idx);
		}
	}
	else
	{
		m_sCheckmarkedItems.erase(iter);
	}
}
