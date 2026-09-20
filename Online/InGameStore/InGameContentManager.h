//--------------------------------------------------------------------------------------
// InGameContentManager.h
//
// Tracks the set of installed content.
// Demonstrates how to correlate Marketplace Offers with installed Content Packages
// Demonstrates how to mount Content Packages and retrieve the license mask.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#ifndef IN_GAME_CONTENT_MANAGER_H
#define IN_GAME_CONTENT_MANAGER_H

#include <vector>
#include <xtl.h>
#include <xonline.h>
#include "UIHelpers.h"
#include "InGameOfferUI.h"
#include "Enumerator.h"

//--------------------------------------------------------------------------------------
// Name: ContentEnumerator
// Desc: Implements an asynchronous enumerator for Content Packages
//--------------------------------------------------------------------------------------
class ContentManager;
class ContentEnumerator : public Enumerator
{
public:
    
    //--------------------------------------------------------------------------------------
    // Name: Create
    // Desc: Calls XContentCreateEnumerator to create the enumeration handle
    //       and computes the size of the enumeration buffer
    //--------------------------------------------------------------------------------------
    HRESULT Create( HANDLE& hEnumeration, DWORD& dwBufferSize );

    //--------------------------------------------------------------------------------------
    // Name: OnData
    // Desc: Copies XCONTENT_DATA from the enumeratin buffer to the ContentManager
    //--------------------------------------------------------------------------------------
    VOID OnData( DWORD dwCount, VOID* pBuffer );
};

//--------------------------------------------------------------------------------------
// Name: ContentManager
// Desc: Maintains a list of installed content packages.
//--------------------------------------------------------------------------------------
class ContentManager
{   

public:

	// Need to define these since I've declared the copy constructor
	ContentManager() {}

    //--------------------------------------------------------------------------------------
    // Name: operator[]
    // Desc: Used for iterating over the XCONTENT_DATA collection
    //--------------------------------------------------------------------------------------
    XCONTENT_DATA operator[](DWORD dw) { return m_aContentData[dw];}

    //--------------------------------------------------------------------------------------
    // Name: Count
    // Desc: report the number of elements in the XCONTENT_DATA collection
    //--------------------------------------------------------------------------------------
    DWORD Count() { return m_aContentData.size(); }

public:

    //--------------------------------------------------------------------------------------
    // Name: Update
    // Desc: Checks to see if the enumerator is ready and if so updates the enumerator
    //--------------------------------------------------------------------------------------
    VOID Update();

    //--------------------------------------------------------------------------------------
    // Name: OnContentInstalled
    // Desc: This method is called in response to XN_LIVE_CONTENT_INSTALLED
    //       Kicks off another enumeration to discover new content packages
    //--------------------------------------------------------------------------------------
    VOID OnContentInstalled();

private:
    friend VOID ContentEnumerator::OnData( DWORD dwCount, VOID* pBuffer );

	typedef std::vector<XCONTENT_DATA> ContentDataArray;
    
    ContentDataArray  m_aContentData;         // Collection of XCONTENT_DATA
    ContentEnumerator m_ContentEnumerator;    // Enumerates content and populates the collection

public:
    // Implementation of Singleton 

    static ContentManager &GetSingleton()
    {
		if ( !s_TheContentManager )
		{
			s_TheContentManager = new ContentManager();
		}

        assert( s_TheContentManager );
		return ( *s_TheContentManager );
    }

	static VOID DeleteInstance()
	{
		assert( s_TheContentManager );

		delete ( s_TheContentManager );
		s_TheContentManager = NULL;
	}

private:
    // The single instance of this class
    static ContentManager *s_TheContentManager;

    // Keep these private to prevent copying
    ContentManager( const ContentManager& );
    ContentManager &operator=( const ContentManager& );
};

//--------------------------------------------------------------------------------------
// Name: ContentTableControl
// Desc: Implements a TableControl for the ContentManager
//--------------------------------------------------------------------------------------
class ContentTableControl : public TableControl
{
public:

	ContentTableControl();

	//--------------------------------------------------------------------------------------
	// Name: RenderOneRow
	// Desc: Render one row of the table
	//--------------------------------------------------------------------------------------
	VOID RenderOneRow( INT iWhichRow );

	//--------------------------------------------------------------------------------------
	// Name: RenderEmptyTable
	// Desc: Render the table when it is empty
	//--------------------------------------------------------------------------------------
	virtual VOID RenderEmptyTable()	{ DrawRowElement( L"No Content Found" ); }
	
	//--------------------------------------------------------------------------------------
	// Name: GetItemCount
	// Desc: Retrieve the total number of items in the table
	//--------------------------------------------------------------------------------------
	virtual INT GetItemCount()
	{
		return ContentManager::GetSingleton().Count();
	}

private:
    // formatting constants
    static const FLOAT DISPLAY_NAME_WIDTH;
	static const FLOAT FILE_NAME_WIDTH;
};

//--------------------------------------------------------------------------------------
// Name: ContentScreen
// Desc: The user is presented with a list of available Content Packages
//--------------------------------------------------------------------------------------
class ContentScreen : public UIScreen
{
public:

	//--------------------------------------------------------------------------------------
    // Name: Initialize
    // Desc: Adds all the buttons to the screen
    //--------------------------------------------------------------------------------------
    VOID Initialize();

    //--------------------------------------------------------------------------------------
    // Name: Render
    // Desc: Draws all the controls on the screen
    //--------------------------------------------------------------------------------------
    VOID Render();

    //--------------------------------------------------------------------------------------
    // Name: Update
    // Desc: Updates controls and button visibilty depending on whether the list is empty
    //--------------------------------------------------------------------------------------
    VOID Update( ATG::GAMEPAD* pGamepad );

private:

    //--------------------------------------------------------------------------------------
    // Name: AssociatedOffers
    // Desc: Brings up a screen listing all offers associated with
    //       the selected XCONTENT_DATA
    //--------------------------------------------------------------------------------------
    VOID AssociatedOffers( WORD wButtonCode );

    //--------------------------------------------------------------------------------------
    // Name: MountPackage
    // Desc: Mounts the Content Package associated with the currently selected XContentData
    //       Brings up a screen showing the error code and the license mask
    //--------------------------------------------------------------------------------------
    VOID MountPackage( WORD wButtonCode );

private:

    UIButton* m_pAssociatedOffersButton; // Button to show Associated Offers screen
    UIButton* m_pMountPackageButton;     // Button to show License Mask screen

    ContentTableControl m_ContentTable;  // Selectable table of XCONTENT_DATA
};

//--------------------------------------------------------------------------------------
// Name: LicenseMaskScreen
// Desc: Display the results of mounting the content package
//--------------------------------------------------------------------------------------
class LicenseMaskScreen : public UIScreen
{
public:
    LicenseMaskScreen( DWORD dwCreateError, DWORD dwLicenseMask )
        : UIScreen(),
        m_dwCreateError(dwCreateError),
        m_dwLicenseMask(dwLicenseMask)
    {
        CreateBackButton( -200.0f,
                          -50.0f,
                          XINPUT_GAMEPAD_B,
                          GLYPH_B_BUTTON L"Back to Content Screen" );
    }

    //--------------------------------------------------------------------------------------
    // Name: Render
    // Desc: If the call to XContentCreate succeeded then show the license mask
    //       Otherwise, display error information
    //--------------------------------------------------------------------------------------
    virtual VOID Render();

    //--------------------------------------------------------------------------------------
    // Name: Update
    // Desc: Updates the buttons on the screen
    //--------------------------------------------------------------------------------------
    virtual VOID Update( ATG::GAMEPAD* pGamepad ) { UpdateButtons( pGamepad ); }

    //--------------------------------------------------------------------------------------
    // Name: NavigateBack
    // Desc: Clean up the instance of the current screen and then show the previous screen
    //--------------------------------------------------------------------------------------
    virtual VOID NavigateBack();

private:
    DWORD m_dwCreateError; // Error code from XContentCreate
    DWORD m_dwLicenseMask; // License mask results from XContentCreate
};

//--------------------------------------------------------------------------------------
// Name: AssociatedOffersList
// Desc: List all Offers associated with a particular Content Package
//       There can be up to 16 offers associated with any particular content package
//--------------------------------------------------------------------------------------
class AssociatedOfferList : public OfferTableControl
{
public:
    AssociatedOfferList() : OfferTableControl( 0.0f, 50.0f ){}

	typedef std::vector<unsigned> IndexArray;
    IndexArray m_aOfferIndices;

protected:
    //--------------------------------------------------------------------------------------
	// Name: GetItemCount
	// Desc: Retrieve the total number of items in the table
	//--------------------------------------------------------------------------------------
	INT GetItemCount()	{ return m_aOfferIndices.size(); }
	
	//--------------------------------------------------------------------------------------
	// Name: RenderOneRow
	// Desc: Render one row of the table
	//--------------------------------------------------------------------------------------
	VOID RenderOneRow( INT iWhichRow );
};

//--------------------------------------------------------------------------------------
// Name: AssociatedOffersScreen
// Desc: Allow the user to inspect all offers associated with a particular content
//       package
//--------------------------------------------------------------------------------------
class AssociatedOffersScreen : public UIScreen
{
public:
    AssociatedOffersScreen() : UIScreen()
    {
		m_AssociatedOffers.Initialize();

		CreateBackButton( UIHelpers::s_fBUTTON_LEFT_X,
			              UIHelpers::s_fBUTTON_LOWER_Y,
                          XINPUT_GAMEPAD_B,
                          GLYPH_B_BUTTON L"Back to Content Screen" );
    }

    //--------------------------------------------------------------------------------------
    // Name: FindAssociatedOffers
    // Desc: Iterate through all the offers maintained by the OfferManager
    //       and check each one against the provided XCONTENT_DATA using the
    //       XMarketplaceDoesContentIdMatch API
    //--------------------------------------------------------------------------------------
    VOID FindAssociatedOffers( const XCONTENT_DATA *pContentData );
    
    //--------------------------------------------------------------------------------------
    // Name: Render
    // Desc: Draw the conrols on the screen
    //--------------------------------------------------------------------------------------
    virtual VOID Render()
    {
        // Draw the screen name at the top
		UIHelpers::DrawText( -200, 0, UIHelpers::TEXT_COLOR, L"Associated Offfers" );
                
        // Draw the list of offers
		m_AssociatedOffers.Render( UIHelpers::TEXT_COLOR, UIHelpers::SELECTED_TEXT_COLOR );
        
        // Draw the buttons on the screen
        RenderButtons();
    }

    //--------------------------------------------------------------------------------------
    // Name: Update
    // Desc: Update the controls on the screen
    //--------------------------------------------------------------------------------------
    virtual VOID Update( ATG::GAMEPAD* pGamepad )
    {
        m_AssociatedOffers.Update( pGamepad );
        UpdateButtons( pGamepad );
    }

    //--------------------------------------------------------------------------------------
    // Name: NavigateBack
    // Desc: Clean up each instance when the back button is pressed.
    //--------------------------------------------------------------------------------------
    virtual VOID NavigateBack()
    {
        UIScreen::NavigateBack();
        delete this;
    }

private:
    AssociatedOfferList m_AssociatedOffers; // Scrollable list of offers
};

#endif // IN_GAME_CONTENT_MANAGER_H