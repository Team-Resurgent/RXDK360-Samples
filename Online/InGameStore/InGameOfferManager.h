//--------------------------------------------------------------------------------------
// InGameOfferManager.h
//
// Enumerates the list of offers available on the service.
// Presents offer data to the user and enables inspecting offer details.
// Collects the OfferIDs from user input and initiates downloading.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#ifndef IN_GAME_OFFER_MANAGER_H
#define IN_GAME_OFFER_MANAGER_H

#include <vector>
#include <set>
#include <xtl.h>
#include <xonline.h>
#include "UIHelpers.h"
#include "InGameOfferUI.h"
#include "Enumerator.h"

//--------------------------------------------------------------------------------------
// Name: OfferEnumerator
// Desc: Enumerator for Marketplace Offers
//--------------------------------------------------------------------------------------
class OfferEnumerator : public Enumerator
{
public:

	//--------------------------------------------------------------------------------------
	// Name: Create
	// Desc: Create the enumeration handle via XMareketplaceCreateOfferEnumerator
	//       Computes the size of the buffer needed for enumeration results
	//--------------------------------------------------------------------------------------
	virtual HRESULT Create( HANDLE& hEnueratoion, DWORD& dwBufferSize );

	//--------------------------------------------------------------------------------------
	// Name: OnData
	// Desc: Copies Marketplace offer data from the enumeration buffer to the OfferManager
	//--------------------------------------------------------------------------------------
	virtual VOID OnData( DWORD dwCount, VOID* pBuffer );
};

//--------------------------------------------------------------------------------------
// Name: OfferManager
// Desc: Manage all the Offers
//--------------------------------------------------------------------------------------
class OfferManager
{

public:

	OfferManager() {}
	~OfferManager()	{ ClearOffers(); }

	//--------------------------------------------------------------------------------------
	// Name: Count
	// Desc: report the number of elements in the XMARKETPLACE_CONTENTOFFER_INFO collection
	//--------------------------------------------------------------------------------------
	DWORD Count() { return m_aOfferData.size(); }

	//--------------------------------------------------------------------------------------
	// Name: operator[]
	// Desc: Allow iterating over the XMARKETPLACE_CONTENTOFFER_INFO collection
	//--------------------------------------------------------------------------------------
	const XMARKETPLACE_CONTENTOFFER_INFO& operator[](DWORD dw){ return *m_aOfferData[dw]; }

	//--------------------------------------------------------------------------------------
	// Name: Update
	// Desc: Checks to see if the enumerator is ready and if so updates the enumerator
	//--------------------------------------------------------------------------------------
	VOID Update();

	//--------------------------------------------------------------------------------------
	// Name: EnumerateOffers
	// Desc: Cancel any pending offer enumeration and start over
	//--------------------------------------------------------------------------------------
	VOID EnumerateOffers();

private:

	friend VOID OfferEnumerator::OnData( DWORD dwCount, VOID* pBuffer );

	//--------------------------------------------------------------------------------------
	// Name: _AddOffer
	// Desc: Create a copy of the referenced offer and add it to the array of offers during
	//       enumeration.
	//--------------------------------------------------------------------------------------
	VOID AddOffer( const XMARKETPLACE_CONTENTOFFER_INFO& Offer );

	//--------------------------------------------------------------------------------------
	// Name: ClearOffers
	// Desc: Free each structure in the collection then clear the collection
	//--------------------------------------------------------------------------------------
	VOID ClearOffers();

private:
	typedef std::vector <PXMARKETPLACE_CONTENTOFFER_INFO> OfferDataArray;

	OfferDataArray m_aOfferData;        // Collection of XMARKETPLACE_CONTENTOFFER_INFO structures
	OfferEnumerator m_OfferEnumerator;  // For asynchronous enumeration of offer data

public:
	// Implementation of Singleton 

	static OfferManager& GetSingleton()
	{
		if ( !s_TheOfferManager )
		{
			s_TheOfferManager = new OfferManager();
		}
		
		assert( s_TheOfferManager );
		return *s_TheOfferManager;
	}

	static VOID DeleteInstance()
	{
		assert( s_TheOfferManager );

		delete ( s_TheOfferManager );
		s_TheOfferManager = NULL;
	}

private:

	// The single instance of this class
	static OfferManager* s_TheOfferManager;

	// Keep these private to prevent copying
	OfferManager( const OfferManager& );
	OfferManager &operator=( const OfferManager& );
};

//--------------------------------------------------------------------------------------
// Name: OfferSelectionTable
// Desc: Implements a OfferTableControl for the OfferManager
//       Player can scroll through the list and select several offers using the dpad
//--------------------------------------------------------------------------------------
class OfferSelectionTable : public OfferTableControl
{

public:

	OfferSelectionTable::OfferSelectionTable() : OfferTableControl( 0.0f, 50.0f ){}

	//--------------------------------------------------------------------------------------
	// Name: Initialize
	// Desc: Set up the column headers
	//--------------------------------------------------------------------------------------
	virtual VOID Initialize();

	//--------------------------------------------------------------------------------------
	// Name: GetItemCount
	// Desc: Retrieve the total number of items in the table
	//--------------------------------------------------------------------------------------
	virtual INT GetItemCount()
	{
		return OfferManager::GetSingleton().Count();
	}

	//--------------------------------------------------------------------------------------
	// Name: RenderOneRow
	// Desc: Render one row of the table
	//--------------------------------------------------------------------------------------
	virtual VOID RenderOneRow( INT iWhichRow );

public:

	//--------------------------------------------------------------------------------------
	// Name: ToggelCheckmark
	// Desc: Toggles the checkmark for the current item
	//--------------------------------------------------------------------------------------
	VOID ToggleCheckmark();

	//--------------------------------------------------------------------------------------
	// Name: ClearCheckmarks
	// Desc: Clears all checkmarks from the list
	//--------------------------------------------------------------------------------------
	VOID ClearCheckmarks() { m_sCheckmarkedItems.clear(); }

	//--------------------------------------------------------------------------------------
	// Name: GetCheckmarkCount
	// Desc: Return the number of items that have checkmarks
	//--------------------------------------------------------------------------------------
	INT GetCheckmarkCount() { return m_sCheckmarkedItems.size(); }
	
	typedef std::set<UINT32> IndexSet;

	// Allow iterating through checkmarked items
	const IndexSet::iterator begin(){ return m_sCheckmarkedItems.begin(); }
	const IndexSet::iterator end(){ return m_sCheckmarkedItems.end(); }

private:
	// formatting constants
	static const FLOAT CHECKBOX_WIDTH;

	IndexSet      m_sCheckmarkedItems;
};

//--------------------------------------------------------------------------------------
// Name: OfferScreen
// Desc: The user is presented with a list of available offers. Users may scroll through
//       the list and checkmark some offers to download. Users may choose to download
//       checkmarked offers.
//--------------------------------------------------------------------------------------
class OfferScreen : public UIScreen
{
public:
	OfferScreen()
		: m_pDownloadOfferButton(NULL),
		m_pOfferDetailsButton(NULL),
		m_pSelectForDownloadButton(NULL)
	{
	}

	//--------------------------------------------------------------------------------------
	// Name: Initialize
	// Desc: Add all the buttons to the screen
	//--------------------------------------------------------------------------------------
	VOID Initialize();

	//--------------------------------------------------------------------------------------
	// Name: Render
	// Desc: Draw the screen title, render the offer list and the buttons
	//--------------------------------------------------------------------------------------
	virtual VOID Render();

	//--------------------------------------------------------------------------------------
	// Name: Update
	// Desc: Hides buttons where there are no visible offers in the list, updates the
	//       offer list, updates all the buttons
	//--------------------------------------------------------------------------------------
	virtual VOID Update( ATG::GAMEPAD* pGamepad );

	//--------------------------------------------------------------------------------------
	// Name: ResetTable
	// Desc: Reset the selection table after enumerating
	//--------------------------------------------------------------------------------------
	void ResetTable() { m_OfferList.ClearCheckmarks(); }

private:

	//--------------------------------------------------------------------------------------
	// Name: OfferDetails
	// Desc: Get the XMARKETPLACE_CONTENTOFFER_INFO for the current selection and then
	//       display all the fields in detail
	//--------------------------------------------------------------------------------------
	VOID OfferDetails( WORD wButtonCode );

	UIButton          *m_pOfferDetailsButton;

private:

	// Offer Downloading

	//--------------------------------------------------------------------------------------
	// Name: ToggleOfferSelection
	// Desc: Toggle the checkmark for the currently selected item
	//--------------------------------------------------------------------------------------
	VOID ToggleOfferSelection( WORD wButtonCode );

	//--------------------------------------------------------------------------------------
	// Name: DownloadItemSelections
	// Desc: User has selected possibly several offers to download.
	//       Pass the set of OfferIDs to the DownloadManager in order to download the DLC
	//--------------------------------------------------------------------------------------
	VOID DownloadSelections( WORD wButtonCode );

	
	UIButton          *m_pDownloadOfferButton;
	UIButton          *m_pSelectForDownloadButton;

private:
	OfferSelectionTable m_OfferList;
};

#endif // IN_GAME_OFFER_MANAGER_H