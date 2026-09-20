//--------------------------------------------------------------------------------------
// UIHelpers.cpp
//
// Simple screen and control classes used to implement the InGameStore UI
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include "AtgApp.h"
#include "AtgUtil.h"
#include "UIHelpers.h"
#include <assert.h>
#include <malloc.h>

//--------------------------------------------------------------------------------------
// UIHelpers implementation
//--------------------------------------------------------------------------------------
ATG::Font UIHelpers::s_Font;

FLOAT UIHelpers::s_fSCREEN_WIDTH   = 0.0f;
FLOAT UIHelpers::s_fSCREEN_HEIGHT  = 0.0f;
FLOAT UIHelpers::s_fTEXT_HEIGHT    = 0.0f;
FLOAT UIHelpers::s_fTEXT_LEADING   = 0.0f;

// Three rows of buttons at the bottom
FLOAT UIHelpers::s_fBUTTON_UPPER_Y  = 0.0f;
FLOAT UIHelpers::s_fBUTTON_MIDDLE_Y = 0.0f;
FLOAT UIHelpers::s_fBUTTON_LOWER_Y  = 0.0f;

// Two columns of buttons at the bottom
FLOAT UIHelpers::s_fBUTTON_LEFT_X  = 0.0f;
FLOAT UIHelpers::s_fBUTTON_RIGHT_X = 0.0f;

//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Inintialize the font used for all the UI.
//       Compute positions for screen layout
//--------------------------------------------------------------------------------------
HRESULT
UIHelpers::Initialize( const CHAR* strFontFileName )
{
	// Create the font
    if( FAILED( s_Font.Create( strFontFileName ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    s_Font.SetWindow( ATG::GetTitleSafeArea() );

	UINT uiDisplayWidth;
	UINT uiDisplayHeight;
	BOOL bIsWideScreen;
	ATG::GetVideoSettings( &uiDisplayWidth, &uiDisplayHeight, &bIsWideScreen );

	FLOAT fScaleFactor = ( FLOAT )uiDisplayWidth;
	fScaleFactor = fScaleFactor / 1024;

	s_Font.SetScaleFactors( fScaleFactor, fScaleFactor );

	// Initialize globals
	s_fTEXT_HEIGHT = s_Font.m_fYScaleFactor * s_Font.m_fFontYAdvance;

	D3DRECT rcWindow;
	s_Font.GetWindow( rcWindow );
	s_fSCREEN_WIDTH  = ( FLOAT )( rcWindow.x2 - rcWindow.x1 );
	s_fSCREEN_HEIGHT = ( FLOAT )( rcWindow.y2 - rcWindow.y1 );

	// Tweak these to taste
	FLOAT fSideMargin  = 10.0f;
	FLOAT fButtonWidth = 50.0f;
	FLOAT fMiddleSpace = 40.0f;

	FLOAT fTotalWidth = 2*fSideMargin + 2*fButtonWidth + fMiddleSpace;
	FLOAT fWidthScale = s_fSCREEN_WIDTH / fTotalWidth;

	s_fBUTTON_LEFT_X  = fSideMargin * fWidthScale;
	s_fBUTTON_RIGHT_X = ( fSideMargin + fButtonWidth + fMiddleSpace ) * fWidthScale;

	// Tweak these to taste
	FLOAT fBottomMarginFactor = 1.0f;
	FLOAT fLeadingFactor      = 0.5f;

	FLOAT fBottomMargin   = fBottomMarginFactor * s_fTEXT_HEIGHT;
	FLOAT s_fTEXT_LEADING = fLeadingFactor * s_fTEXT_HEIGHT;

	s_fBUTTON_LOWER_Y  = s_fSCREEN_HEIGHT - fBottomMargin;
	s_fBUTTON_MIDDLE_Y = s_fBUTTON_LOWER_Y - s_fTEXT_HEIGHT - s_fTEXT_LEADING;
	s_fBUTTON_UPPER_Y  = s_fBUTTON_MIDDLE_Y - s_fTEXT_HEIGHT - s_fTEXT_LEADING;
	
	return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DrawText
// Desc: var args wrapper for ATGFont::DrawText
//--------------------------------------------------------------------------------------
VOID
UIHelpers::DrawText( FLOAT sx, FLOAT sy, DWORD dwColor, WCHAR* wstrFormat, ... )
{
	va_list pArgList;
	va_start( pArgList, wstrFormat );
	DrawTextV( sx, sy, dwColor, wstrFormat, pArgList );
	va_end( pArgList );
}

//--------------------------------------------------------------------------------------
// Name: DrawTextV
// Desc: var args wrapper for ATGFont::DrawText
//--------------------------------------------------------------------------------------
VOID
UIHelpers::DrawTextV( FLOAT sx, FLOAT sy, DWORD dwColor, WCHAR* wstrFormat, va_list pArgList )
{
	// Count the required length of the string
	DWORD dwStrLen = _vscwprintf( wstrFormat, pArgList ) + 1;    // +1 = null terminator
	WCHAR* wstrMessage = ( WCHAR* )_malloca( dwStrLen * sizeof( WCHAR ) );
	vswprintf_s( wstrMessage, dwStrLen, wstrFormat, pArgList );

	s_Font.DrawText( sx, sy, dwColor, wstrMessage );

	_freea( wstrMessage );
}


//--------------------------------------------------------------------------------------
// TableControl implementation
//--------------------------------------------------------------------------------------

TableControl::TableControl( FLOAT fOriginX, FLOAT fOriginY ) :

m_fOriginX( fOriginX ),
m_fOriginY( fOriginY ),

m_iDisplayLowerBound( 0 ),
m_iSelectionIndex( 0 ),
m_iDisplayUpperBound( 0 ),
m_dwColor( UIHelpers::TEXT_COLOR_RED ),
m_fX( 0.0f ),
m_fY( 0.0f ),
m_fColumTotalWidths( 0.0f ),
m_iItemsPerScreen( 0 ),
m_dwColumnIndex( 0 )
{
	FLOAT fRows = UIHelpers::s_fBUTTON_UPPER_Y / UIHelpers::s_fTEXT_HEIGHT;
	m_iItemsPerScreen = ( INT )fRows;
	m_iItemsPerScreen -= 3;
}

TableControl::~TableControl()
{
	for( DWORD dw = 0; dw < m_aColumnHeadings.size(); ++dw )
	{
		delete [] m_aColumnHeadings[dw];
	}
}

//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Render the entire table one calling RenderOneRow on each row
//--------------------------------------------------------------------------------------
VOID
TableControl::Render( DWORD dwColor, DWORD dwSelectedColor )
{
	m_dwColor = dwColor;

	// Render the column headings
	m_fX = m_fOriginX;
	m_fY = m_fOriginY;
	RenderColumnHeadings();

	// Advance to the first row
	m_fX = m_fOriginX;
	m_fY += UIHelpers::s_fTEXT_HEIGHT;

	INT iItems = GetItemCount();
	if ( iItems > 0 )
	{
		for ( INT i = m_iDisplayLowerBound; i < m_iDisplayUpperBound; ++i )
		{
			m_dwColor = ( i == m_iSelectionIndex ) ? dwSelectedColor : dwColor;

			m_dwColumnIndex = 0;
			RenderOneRow( i );

			// Advance to the next row
			m_fX = m_fOriginX;
			m_fY += UIHelpers::s_fTEXT_HEIGHT;;
		}
	}
	else
	{
		m_dwColumnIndex = 0;
		RenderEmptyTable();
	}
}

//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Advance the row selection and scroll the view based on gamepad input
//--------------------------------------------------------------------------------------
VOID
TableControl::Update( ATG::GAMEPAD* pGamepad )
{
	INT iItems = GetItemCount();
	if ( iItems > 0 )
	{
		if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
		{
			if ( m_iSelectionIndex < ( iItems - 1 ) )
				++m_iSelectionIndex;
		}

		if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
		{
			if ( m_iSelectionIndex > 0 )
				--m_iSelectionIndex;
		}

		INT iPerScreen = m_iItemsPerScreen;

		// check for scrolling
		if ( m_iSelectionIndex < m_iDisplayLowerBound ||
			 m_iSelectionIndex >= m_iDisplayUpperBound )
		{
			
			m_iDisplayLowerBound = max(0, m_iSelectionIndex - iPerScreen/2);
		}

		m_iDisplayUpperBound = min( iItems, m_iDisplayLowerBound + iPerScreen);
	}
}

//--------------------------------------------------------------------------------------
// Name: Reset
// Desc: Reset the selection index to 0 and restore table invariants
//--------------------------------------------------------------------------------------
VOID
TableControl::Reset()
{
	INT iItems = GetItemCount();
	m_iSelectionIndex = 0;
	m_iDisplayLowerBound = 0;
	if ( iItems > 0 )
	{
		m_iDisplayUpperBound = min( iItems, m_iItemsPerScreen );
	}
}

//--------------------------------------------------------------------------------------
// Name: AddColumnHeading
// Desc: Adds a column heading to the table at the specified location
//--------------------------------------------------------------------------------------
VOID
TableControl::AddColumnHeading( FLOAT fWidth, WCHAR* strText )
{
	// Make a copy of the string
	DWORD dwTextLen = wcslen( strText ) + 1;
	WCHAR* wstr = new WCHAR[ dwTextLen ];
	wcscpy_s( wstr, dwTextLen, strText );

	// Add to column headings
	m_aColumnHeadings.push_back( wstr );

	// Add the width to the column widths
	m_aColumnWidths.push_back( fWidth );

	// Adjust the total column widths
	m_fColumTotalWidths += fWidth;
}

//--------------------------------------------------------------------------------------
// Name: DrawRowElement
// Desc: Derived class calls DrawRowElement from implementation of RenderOneRow
//       in order to draw the next row element
//--------------------------------------------------------------------------------------
VOID
TableControl::DrawRowElement( WCHAR* wstrFormat, ... )
{
	va_list pArgList;
	va_start( pArgList, wstrFormat );
	DrawRowElementV( wstrFormat, pArgList );
	va_end( pArgList );
}

//--------------------------------------------------------------------------------------
// Name: DrawRowElementV
// Desc: Derived class calls DrawRowElement from implementation of RenderOneRow
//       in order to draw the next row element
//--------------------------------------------------------------------------------------
VOID
TableControl::DrawRowElementV( WCHAR* wstrFormat, va_list pArgList )
{
	UIHelpers::DrawTextV( m_fX, m_fY, m_dwColor, wstrFormat, pArgList );

	// Advance to the next position
	FLOAT fWidthDelta = ( m_aColumnWidths[ m_dwColumnIndex ] / m_fColumTotalWidths ) * UIHelpers::s_fSCREEN_WIDTH;

	m_fX += fWidthDelta;

	if ( m_dwColumnIndex < m_aColumnHeadings.size() )
	{
		++m_dwColumnIndex;
	}
}

//--------------------------------------------------------------------------------------
// Name: RenderColumnHeadings
// Desc: Render the column headings for the table
//--------------------------------------------------------------------------------------
VOID
TableControl::RenderColumnHeadings()
{
	m_dwColumnIndex = 0;
	m_fX = m_fOriginX;
	for (; m_dwColumnIndex < m_aColumnHeadings.size(); ++m_dwColumnIndex )
	{
		UIHelpers::DrawText( m_fX, m_fY, m_dwColor, m_aColumnHeadings[m_dwColumnIndex] );
		FLOAT fWidthDelta = ( m_aColumnWidths[ m_dwColumnIndex ] / m_fColumTotalWidths ) * UIHelpers::s_fSCREEN_WIDTH;
		m_fX += fWidthDelta;
	}
}

//--------------------------------------------------------------------------------------
// StaticTableControl implementation
//--------------------------------------------------------------------------------------
StaticTableControl::~StaticTableControl()
{
	for( DWORD dw = 0; dw < m_aRowElements.size(); ++dw )
	{
		delete [] m_aRowElements[dw];
	}
}

//--------------------------------------------------------------------------------------
// Name: AddRowElement
// Desc: Derived class calls DrawRowElement from implementation of RenderOneRow
//       in order to draw the next row element
//--------------------------------------------------------------------------------------
VOID
StaticTableControl::AddRowElement( WCHAR* wstrFormat, ... )
{
	va_list pArgList;
	va_start( pArgList, wstrFormat );
	AddRowElementV( wstrFormat, pArgList );
	va_end( pArgList );
}

//--------------------------------------------------------------------------------------
// Name: AddRowElementV
// Desc: Derived class calls DrawRowElement from implementation of RenderOneRow
//       in order to draw the next row element
//--------------------------------------------------------------------------------------
VOID
StaticTableControl::AddRowElementV( WCHAR* wstrFormat, va_list pArgList )
{
	// Count the required length of the string
	DWORD dwStrLen = _vscwprintf( wstrFormat, pArgList ) + 1;    // +1 = null terminator
	WCHAR* wstrMessage = new WCHAR[ dwStrLen ];
	vswprintf_s( wstrMessage, dwStrLen, wstrFormat, pArgList );

	m_aRowElements.push_back( wstrMessage );
}

//--------------------------------------------------------------------------------------
// Name: RenderOneRow
// Desc: Render one row of the table
//--------------------------------------------------------------------------------------
VOID
StaticTableControl::RenderOneRow( INT iWhichRow )
{
	DWORD dwNumCols = m_aColumnHeadings.size();
	DWORD dwCurrentRowElement = iWhichRow * dwNumCols;
	for ( DWORD dw = 0; dw < dwNumCols; ++dw, ++dwCurrentRowElement )
	{
		if ( dwCurrentRowElement == m_aRowElements.size() )
		{
			break;
		}
		DrawRowElement( m_aRowElements[dwCurrentRowElement] );
	}
}

//--------------------------------------------------------------------------------------
// Name: GetItemCount
// Desc: Retrieve the total number of items in the table
//--------------------------------------------------------------------------------------
INT
StaticTableControl::GetItemCount()
{
	INT iRowElts = m_aRowElements.size();
	INT iCols = m_aColumnHeadings.size();
	INT iRows =  iRowElts / iCols;

	if ( iRows * iCols < iRowElts )
	{
		++iRows;
	}

	return iRows;
}

//--------------------------------------------------------------------------------------
// UIButton implementation
//--------------------------------------------------------------------------------------
UIButton::UIButton(FLOAT fX, FLOAT fY, WORD wButtonCode, WCHAR* wszButtonText, UIBUTTON_HANDLER ButtonHandler) :
m_ButtonHandler( ButtonHandler ),
m_fX( fX ),
m_fY( fY ),
m_wButtonCode( wButtonCode ),
m_wszButtonText( NULL ),
m_pUISParent( NULL ),
m_bHidden( FALSE )
{
	INT len = wcslen( wszButtonText );
	m_wszButtonText = new WCHAR[len+1];
	wcscpy_s( m_wszButtonText, len+1, wszButtonText );
	m_wszButtonText[len] = L'\0';
}

UIButton::UIButton(const UIButton& rhs) :
m_fX( rhs.m_fX ),
m_fY( rhs.m_fY ),
m_wButtonCode( rhs.m_wButtonCode ),
m_wszButtonText( NULL ),
m_pUISParent( rhs.m_pUISParent ),
m_ButtonHandler( rhs.m_ButtonHandler ),
m_bHidden( rhs.m_bHidden )
{
	INT len = wcslen( rhs.m_wszButtonText );
	m_wszButtonText = new WCHAR[len+1];
	wcscpy_s( m_wszButtonText, len+1, rhs.m_wszButtonText );
	m_wszButtonText[len] = L'\0';
}

UIButton& UIButton::operator=( const UIButton& rhs )
{
	if (&rhs != this)
	{
		m_fX = rhs.m_fX;
		m_fY = rhs.m_fY;
		m_wButtonCode = rhs.m_wButtonCode;
		m_pUISParent = rhs.m_pUISParent;
		m_ButtonHandler = rhs.m_ButtonHandler;
		m_bHidden = rhs.m_bHidden;

		INT len = wcslen( rhs.m_wszButtonText );
		m_wszButtonText = new WCHAR[len+1];
		wcscpy_s( m_wszButtonText, len+1, rhs.m_wszButtonText );
		m_wszButtonText[len] = L'\0';
	}
	return *this;
}

VOID UIButton::Initialize( UIScreen* pUISParent )
{
	m_pUISParent = pUISParent;
}

VOID UIButton::Cleanup()
{
	delete [] m_wszButtonText;
}

VOID UIButton::Render()
{
	if ( !m_bHidden )
	{
		UIHelpers::DrawText( m_fX, m_fY, UIHelpers::TEXT_COLOR, m_wszButtonText );
	}
}

VOID UIButton::Update( ATG::GAMEPAD* pGamepad )
{
	if ( !m_bHidden )
	{
		if ( pGamepad->wPressedButtons & m_wButtonCode )
		{
			// m_ButtonHandler( m_pUISParent, m_wButtonCode );
			CALL_BUTTON_HANDLER( *m_pUISParent, m_ButtonHandler )( m_wButtonCode );
		}
	}
}

//--------------------------------------------------------------------------------------
// BackButton implementation
//--------------------------------------------------------------------------------------

VOID BackButton::Update( ATG::GAMEPAD* pGamepad )
{
	if ( !m_bHidden )
	{
		if ( pGamepad->wPressedButtons & m_wButtonCode )
		{
			m_pUISParent->NavigateBack();
		}
	}
}

//--------------------------------------------------------------------------------------
// BackButton implementation
//--------------------------------------------------------------------------------------

VOID NavButton::Update(ATG::GAMEPAD* pGamepad)
{
	if ( !m_bHidden )
	{
		if ( pGamepad->wPressedButtons & m_wButtonCode )
		{
			m_pUISParent->Navigate(m_pUISNavTo);
		}
	}
}

//--------------------------------------------------------------------------------------
// UIScreen implementation
//--------------------------------------------------------------------------------------
UIScreen *UIScreen::s_pUISCurrentScreen = NULL;

UIScreen::UIScreen() :
m_pUISPreviousScreen( NULL )
{
}

UIScreen::~UIScreen()
{
	for ( DWORD dw = 0; dw < m_aButtons.size(); ++dw )
	{
		delete m_aButtons[dw];
		m_aButtons[dw] = NULL;
	}
}

VOID UIScreen::NavigateBack()
{
	assert( m_pUISPreviousScreen );
	if ( m_pUISPreviousScreen )
	{
		s_pUISCurrentScreen = m_pUISPreviousScreen;
	}
}

VOID UIScreen::Navigate( UIScreen* pNewScreen )
{
	pNewScreen->m_pUISPreviousScreen = this;
	s_pUISCurrentScreen = pNewScreen;
}

UIButton*
UIScreen::CreateButton( FLOAT fX,
					    FLOAT fY,
						WORD wButtonCode,
						WCHAR* wszButtonText,
						UIBUTTON_HANDLER ButtonHandler,
						BOOL bHidden )
{
	UIButton *pNewButton = new UIButton( fX, fY, wButtonCode, wszButtonText, ButtonHandler );
	assert( pNewButton );
	pNewButton->Initialize( this );
	pNewButton->Hide( bHidden );
	m_aButtons.push_back( pNewButton );
	return pNewButton;
}

BackButton*
UIScreen::CreateBackButton( FLOAT fX,
						    FLOAT fY,
							WORD wButtonCode,
							WCHAR* wszButtonText,
							BOOL bHidden )
{
	BackButton *pNewButton = new BackButton( fX, fY, wButtonCode, wszButtonText );
	assert( pNewButton );
	pNewButton->Initialize( this );
	pNewButton->Hide( bHidden );
	m_aButtons.push_back( pNewButton );
	return pNewButton;
}

NavButton*
UIScreen::CreateNavButton( FLOAT fX,
						   FLOAT fY,
						   WORD wButtonCode,
						   WCHAR* wszButtonText,
						   UIScreen* pUISNavTo,
						   BOOL bHidden )
{
	NavButton* pNewButton = new NavButton( fX, fY, wButtonCode, wszButtonText );
	assert( pNewButton );
	pNewButton->Initialize( this, pUISNavTo );
	pNewButton->Hide( bHidden );
	m_aButtons.push_back( pNewButton );
	return pNewButton;
}

VOID UIScreen::UpdateButtons( ATG::GAMEPAD* pGamepad )
{
	for ( DWORD dw = 0; dw < m_aButtons.size(); ++dw )
	{
		m_aButtons[dw]->Update( pGamepad );
	}
}

VOID UIScreen::RenderButtons()
{
	for ( DWORD dw = 0; dw < m_aButtons.size(); ++dw )
	{		
		m_aButtons[dw]->Render();
	}
}