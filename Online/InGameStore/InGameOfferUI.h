//--------------------------------------------------------------------------------------
// InGameOfferUI.h
//
// offer table used by the offer screen and for associated offers
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef IN_GAME_OFFER_UI_H
#define IN_GAME_OFFER_UI_H

#include "UIHelpers.h"

class FormatContentIDValue
{

public:
	FormatContentIDValue( const BYTE* pContentID );
	
	WCHAR *GetBuffer() { return m_wstrBuf; }

private:

	WCHAR m_wstrBuf[128];
};

//--------------------------------------------------------------------------------------
// Name: OfferTableControl
// Desc: Base class for scrollable table of offers
//--------------------------------------------------------------------------------------
class OfferTableControl : public TableControl
{
	
public:
	OfferTableControl( FLOAT fOriginX, FLOAT fOriginY )
		: TableControl( fOriginX, fOriginY )
	{
	}
	
	//--------------------------------------------------------------------------------------
	// Name: Initialize
	// Desc: Set up the column headers
	//--------------------------------------------------------------------------------------
	virtual VOID Initialize()
	{
		AddColumnHeading( OFFER_NAME_WIDTH, L"Offer Name " );
		AddColumnHeading( OFFER_ID_WIDTH, L"Offer ID" );
		AddColumnHeading( HAS_PURCHASED_WIDTH, L"Purchased?" );
	}

	//--------------------------------------------------------------------------------------
	// Name: RenderOneRow
	// Desc: Render one row of the table
	//--------------------------------------------------------------------------------------
	virtual VOID RenderOneRow( const XMARKETPLACE_CONTENTOFFER_INFO& OfferInfo );
	
	//--------------------------------------------------------------------------------------
	// Name: RenderEmptyTable
	// Desc: Render the table when it is empty
	//--------------------------------------------------------------------------------------
	virtual VOID RenderEmptyTable()
	{
		DrawRowElement( L"No Offers Found" );
	}

	//--------------------------------------------------------------------------------------
	// Name: GetItemCount
	// Desc: Derived classes provide an implementation to retrieve the total number of items in the table
	//--------------------------------------------------------------------------------------
	virtual INT GetItemCount() = 0;

	//--------------------------------------------------------------------------------------
	// Name: RenderOneRow
	// Desc: Derived class implements this method to render one row of the table
	//--------------------------------------------------------------------------------------
	virtual VOID RenderOneRow( INT iWhichRow ) = 0;

private:

	// Formatting constants
	static const FLOAT OFFER_NAME_WIDTH;
	static const FLOAT OFFER_ID_WIDTH;
	static const FLOAT HAS_PURCHASED_WIDTH;
};

//--------------------------------------------------------------------------------------
// Name: OfferDetailsTableControl
// Desc: Table of all the fields in an XMARKETPLACE_CONTENTOFFER_INFO
//--------------------------------------------------------------------------------------
class OfferDetailsTableControl : public StaticTableControl
{
public:

	OfferDetailsTableControl( FLOAT fOriginX, FLOAT fOriginY, const XMARKETPLACE_CONTENTOFFER_INFO *pOffer );

	// formatting constants
	static const FLOAT OFFER_FIELD_NAME_WIDTH;
	static const FLOAT OFFER_FIELD_VALUE_WIDTH;
};

//--------------------------------------------------------------------------------------
// Name: OfferDetailsScreen
// Desc: Allow the user to scroll through all the fields of an Offer and inspect the
//       values
//--------------------------------------------------------------------------------------
class OfferDetailsScreen : public UIScreen
{
public:
	OfferDetailsScreen( const XMARKETPLACE_CONTENTOFFER_INFO* pOffer )
		: UIScreen()
		, m_OfferDetails( 0.0f, 50.0f, pOffer )
	{
		CreateBackButton( UIHelpers::s_fBUTTON_LEFT_X,
			              UIHelpers::s_fBUTTON_LOWER_Y,
						  XINPUT_GAMEPAD_B,
						  GLYPH_B_BUTTON L"Back to Offer Screen" );
	}
	
	virtual VOID Render()
	{
		UIHelpers::DrawText( -200, 0, UIHelpers::TEXT_COLOR, L"Offer Details" );

		m_OfferDetails.Render( UIHelpers::TEXT_COLOR, UIHelpers::SELECTED_TEXT_COLOR );
		
		RenderButtons();
	}
	virtual VOID Update( ATG::GAMEPAD* pGamepad )
	{
		m_OfferDetails.Update( pGamepad );

		UpdateButtons( pGamepad );
	}

	// Clean up each instance when the back button is pressed.
	virtual VOID NavigateBack()
	{
		UIScreen::NavigateBack();
		delete this;
	}

private:
	OfferDetailsTableControl m_OfferDetails;	
};

#endif // IN_GAME_OFFER_UI_H