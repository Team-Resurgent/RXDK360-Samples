//--------------------------------------------------------------------------------------
// InGameOfferUI.cpp
//
// Implements offer table used by the offer screen and for associated offers
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "InGameOfferUI.h"

//--------------------------------------------------------------------------------------
// FormatContentIDValue implementation
//--------------------------------------------------------------------------------------

FormatContentIDValue::FormatContentIDValue(const BYTE *pContentID)
{
	DWORD dwWcharsWritten = 0;
	for ( DWORD dw = 0; dw < XMARKETPLACE_CONTENT_ID_LEN; ++dw )
	{
		DWORD dwWcharsLeft = 128 - dwWcharsWritten;
		WCHAR *wbuf_p = &m_wstrBuf[dwWcharsWritten];
		swprintf_s( wbuf_p, dwWcharsLeft, L"%02X", pContentID[dw] );
		dwWcharsWritten += 2;
		if ( 3 == dw%4 && dw < XMARKETPLACE_CONTENT_ID_LEN - 1 )
		{
			m_wstrBuf[dwWcharsWritten] = L'_';
			++dwWcharsWritten;
		}
	}
	m_wstrBuf[dwWcharsWritten] = L'\0';
}

//--------------------------------------------------------------------------------------
// OfferTableControl implementation
//--------------------------------------------------------------------------------------
const FLOAT OfferTableControl::OFFER_NAME_WIDTH    = 700.0f;
const FLOAT OfferTableControl::OFFER_ID_WIDTH      = 350.0f;
const FLOAT OfferTableControl::HAS_PURCHASED_WIDTH = 100.0f;

//--------------------------------------------------------------------------------------
// Name: RenderOneRow
// Desc: Render one row of the table
//--------------------------------------------------------------------------------------
VOID
OfferTableControl::RenderOneRow( const XMARKETPLACE_CONTENTOFFER_INFO& OfferInfo )
{
	// Draw the Offer Name value
	DrawRowElement( OfferInfo.wszOfferName );
	
	// Draw the Offer ID value
	DWORD *pdw = (DWORD *)&OfferInfo.qwOfferID;
	DrawRowElement( L"0x%08X%08X", pdw[0], pdw[1] );

	// Draw the fUserHasPurchased value
	DrawRowElement( OfferInfo.fUserHasPurchased ? (WCHAR*)L"TRUE" : (WCHAR*)L"FALSE" );
}

//--------------------------------------------------------------------------------------
// OfferDetailsTableControl implementation
//--------------------------------------------------------------------------------------

const FLOAT OfferDetailsTableControl::OFFER_FIELD_NAME_WIDTH  = 30.0f;
const FLOAT OfferDetailsTableControl::OFFER_FIELD_VALUE_WIDTH = 70.0f;

OfferDetailsTableControl::OfferDetailsTableControl
( FLOAT fOriginX, FLOAT fOriginY, const XMARKETPLACE_CONTENTOFFER_INFO *pOffer )
: StaticTableControl( fOriginX, fOriginY )
{
	// Add the column headings
	AddColumnHeading( OfferDetailsTableControl::OFFER_FIELD_NAME_WIDTH, L"Field Name" );
	AddColumnHeading( OfferDetailsTableControl::OFFER_FIELD_VALUE_WIDTH, L"Field Value" );

	// Add all the rows...

	// qwOfferID
	AddRowElement( L"qwOfferID" );
	DWORD *pdw = (DWORD*)&pOffer->qwOfferID;
	AddRowElement( L"0x%08X%08X", pdw[0], pdw[1] );

	// qwPreviewOfferID
	AddRowElement( L"qwPreviewOfferID" );
	pdw = (DWORD*)&pOffer->qwPreviewOfferID;
	AddRowElement( L"0x%08X%08X", pdw[0], pdw[1] );
	
	// wszOfferName
	AddRowElement( L"wszOfferName" );
	AddRowElement( pOffer->wszOfferName );
	
	// dwOfferType
	AddRowElement( L"dwOfferType" );
	switch ( pOffer->dwOfferType )
	{
	case XMARKETPLACE_OFFERING_TYPE_CONTENT:
		AddRowElement( L"XMARKETPLACE_OFFERING_TYPE_CONTENT" );
		break;
	case XMARKETPLACE_OFFERING_TYPE_GAME_DEMO:
		AddRowElement( L"XMARKETPLACE_OFFERING_TYPE_GAME_DEMO" );
		break;
	case XMARKETPLACE_OFFERING_TYPE_GAME_TRAILER:
		AddRowElement( L"XMARKETPLACE_OFFERING_TYPE_GAME_TRAILER" );
		break;
	case XMARKETPLACE_OFFERING_TYPE_THEME:
		AddRowElement( L"XMARKETPLACE_OFFERING_TYPE_THEME" );
		break;
	case XMARKETPLACE_OFFERING_TYPE_TILE:
		AddRowElement( L"XMARKETPLACE_OFFERING_TYPE_TILE" );
		break;
	case XMARKETPLACE_OFFERING_TYPE_ARCADE:
		AddRowElement( L"XMARKETPLACE_OFFERING_TYPE_ARCADE" );
		break;
	case XMARKETPLACE_OFFERING_TYPE_VIDEO:
		AddRowElement( L"XMARKETPLACE_OFFERING_TYPE_VIDEO" );
		break;
	case XMARKETPLACE_OFFERING_TYPE_CONSUMABLE:
		AddRowElement( L"XMARKETPLACE_OFFERING_TYPE_CONSUMABLE" );
		break;
	default:
		AddRowElement( L"!!!UNEXPECTED OFFERING TYPE!!!" );
	};

	// contentID
	AddRowElement( L"contentID" );
	AddRowElement( FormatContentIDValue( pOffer->contentId ).GetBuffer() );
	
	// fIsUnrestrictedLicense
	AddRowElement( L"fIsUnrestrictedLicense" );
	AddRowElement( pOffer->fIsUnrestrictedLicense ? (WCHAR*)L"TRUE" : (WCHAR*)L"FALSE" );
	
	// dwLicenseMask
	AddRowElement( L"dwLicenseMask" );
	AddRowElement( L"0x%08X", pOffer->dwLicenseMask );

	// dwTitleID
	AddRowElement( L"dwTitleID" );
	AddRowElement( L"0x%08X", pOffer->dwTitleID );
			
	// dwContentCategory
	AddRowElement( L"dwContentCategory" );
	AddRowElement( L"0x%08X", pOffer->dwContentCategory );

	// wszTitleName
	AddRowElement( L"wszTitleName" );
	AddRowElement( pOffer->wszTitleName );

	// fUserHasPurchased
	AddRowElement( L"fUserHasPurchased" );
	AddRowElement( pOffer->fUserHasPurchased ? (WCHAR*)L"TRUE" : (WCHAR*)L"FALSE" );
	
	// dwPackageSize
	AddRowElement( L"dwPackageSize" );
	AddRowElement( L"0x%08X", pOffer->dwPackageSize );
	
	// dwInstallSize
	AddRowElement( L"dwInstallSize" );

	// This value is approximately the installed size of the package in 16KB
	// blocks. The actual install size will include extra space for the file
	// header information (rounded up to the appropriate boundary).
	DWORD dwApproximateInstallSize = pOffer->dwInstallSize * 0x4000;
	AddRowElement( L"0x%X (16KB blocks) == 0x%X (bytes)", pOffer->dwInstallSize, dwApproximateInstallSize );

	// wszSellText
	AddRowElement( L"wszSellText" );
	AddRowElement( pOffer->wszSellText );
	
	// dwAssetID
	AddRowElement( L"dwAssetID" );
	AddRowElement( L"0x%08X", pOffer->dwAssetID );
	
	// dwPurchaseQuantity
	AddRowElement( L"dwPurchaseQuantity" );
	AddRowElement( L"0x%08X", pOffer->dwPurchaseQuantity );

	// dwPointsPrice
	AddRowElement( L"dwPointsPrice" );
	AddRowElement( L"0x%08X", pOffer->dwPointsPrice );
}

