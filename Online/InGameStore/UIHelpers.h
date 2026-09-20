//--------------------------------------------------------------------------------------
// UIHelpers.h
//
// Simple screen and control classes used to implement the InGameStore UI
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#ifndef UIHELPERS_H
#define UIHELPERS_H

#include <vector>
#include <assert.h>
#include <stdio.h>
#include <xtl.h>
#include "AtgInput.h"
#include "AtgFont.h"

class UIHelpers
{
public:

	//--------------------------------------------------------------------------------------
	// Name: Initialize
	// Desc: Inintialize the font used for all the UI.
	//       Compute positions for screen layout
	//--------------------------------------------------------------------------------------
	static HRESULT Initialize( const CHAR* strFontFileName );
	
	//--------------------------------------------------------------------------------------
	// Constants
	//--------------------------------------------------------------------------------------
	static const DWORD TEXT_COLOR          = 0xFFFFFFFF;
	static const DWORD TEXT_COLOR_RED      = 0xFFFF0000;
	static const DWORD TEXT_COLOR_BLUE     = 0xFF0000FF;
	static const DWORD TEXT_COLOR_GREEN    = 0xFF00FF00;
	static const DWORD SELECTED_TEXT_COLOR = 0xFFFFAA00;

	//--------------------------------------------------------------------------------------
	// Globals
	//--------------------------------------------------------------------------------------
	static FLOAT s_fSCREEN_WIDTH;
	static FLOAT s_fSCREEN_HEIGHT;
	static FLOAT s_fTEXT_HEIGHT;
	static FLOAT s_fTEXT_LEADING;
	
	// Three rows of buttons at the bottom
	static FLOAT s_fBUTTON_UPPER_Y;
	static FLOAT s_fBUTTON_MIDDLE_Y;
	static FLOAT s_fBUTTON_LOWER_Y;

	// Two columns of buttons at the bottom
	static FLOAT s_fBUTTON_LEFT_X;
	static FLOAT s_fBUTTON_RIGHT_X;

	//--------------------------------------------------------------------------------------
	// Name: DrawText
	// Desc: var args wrapper for ATGFont::DrawText
	//--------------------------------------------------------------------------------------
	static VOID DrawText( FLOAT sx, FLOAT sy, DWORD dwColor, WCHAR* wstrFormat, ... );

	//--------------------------------------------------------------------------------------
	// Name: DrawTextV
	// Desc: var args wrapper for ATGFont::DrawText
	//--------------------------------------------------------------------------------------
	static VOID DrawTextV( FLOAT sx, FLOAT sy, DWORD dwColor, WCHAR* wstrFormat, va_list pArgList );

private:

	static ATG::Font s_Font;
};


//--------------------------------------------------------------------------------------
// Name: TableControl
// Desc: UI control to present data in scrollable tabular form
//--------------------------------------------------------------------------------------
class TableControl
{
public:

	TableControl( FLOAT fOriginX, FLOAT fOriginY );
	~TableControl();

public:

	//--------------------------------------------------------------------------------------
	// Name: GetSelectionIndex
	// Desc: Get the index of the currently selected row
	//--------------------------------------------------------------------------------------
	INT GetSelectionIndex() { return m_iSelectionIndex; }

	//--------------------------------------------------------------------------------------
	// Name: Render
	// Desc: Render the entire table one calling RenderOneRow on each row
	//--------------------------------------------------------------------------------------
	virtual VOID Render( DWORD dwColor, DWORD dwSelectedColor );

	//--------------------------------------------------------------------------------------
	// Name: Update
	// Desc: Advance the row selection and scroll the view based on gamepad input
	//--------------------------------------------------------------------------------------
	VOID Update( ATG::GAMEPAD *_pGamepad );

	//--------------------------------------------------------------------------------------
	// Name: Reset
	// Desc: Reset the selection index to 0 and restore table invariants
	//--------------------------------------------------------------------------------------
	VOID Reset();

public:

	//--------------------------------------------------------------------------------------
	// Name: AddColumnHeading
	// Desc: Adds a column heading to the table at the specified location
	//--------------------------------------------------------------------------------------
	VOID AddColumnHeading( FLOAT fWidth, WCHAR* strText );

	//--------------------------------------------------------------------------------------
	// Name: RenderOneRow
	// Desc: Derived class implements this method to render one row of the table
	//--------------------------------------------------------------------------------------
	virtual VOID RenderOneRow( INT iWhichRow ) = 0;

	//--------------------------------------------------------------------------------------
	// Name: DrawRowElement
	// Desc: Derived class calls DrawRowElement from implementation of RenderOneRow
	//       in order to draw the next row element
	//--------------------------------------------------------------------------------------
	VOID DrawRowElement( WCHAR* wstrFormat, ... );

	//--------------------------------------------------------------------------------------
	// Name: DrawRowElementV
	// Desc: Derived class calls DrawRowElement from implementation of RenderOneRow
	//       in order to draw the next row element
	//--------------------------------------------------------------------------------------
	VOID DrawRowElementV( WCHAR* wstrFormat, va_list pArgList );

	//--------------------------------------------------------------------------------------
	// Name: RenderEmptyTable
	// Desc: Derived classes provide an implementation to render the table when it is empty
	//--------------------------------------------------------------------------------------
	virtual VOID RenderEmptyTable() = 0;

	//--------------------------------------------------------------------------------------
	// Name: GetItemCount
	// Desc: Derived classes provide an implementation to retrieve the total number of items in the table
	//--------------------------------------------------------------------------------------
	virtual INT GetItemCount() = 0;

private:
	
	//--------------------------------------------------------------------------------------
	// Name: RenderColumnHeadings
	// Desc: Render the column headings for the table
	//--------------------------------------------------------------------------------------
	VOID RenderColumnHeadings();

private:

	FLOAT m_fOriginX;
	FLOAT m_fOriginY;
	
private:

	INT m_iDisplayLowerBound;
	INT m_iSelectionIndex;
	INT m_iDisplayUpperBound;

	DWORD m_dwColor;

	FLOAT m_fX;
	FLOAT m_fY;

protected:
	typedef std::vector<WCHAR*> TextArray;
	typedef std::vector<FLOAT> OffsetArray;


	DWORD        m_dwColumnIndex;     // Current column index into the row being rendered
	TextArray    m_aColumnHeadings;   // Text for the top of each column
	OffsetArray  m_aColumnWidths;     // Widths for each of the columns
	FLOAT        m_fColumTotalWidths; // Combined widths for all columns
	INT          m_iItemsPerScreen;   // Number of rows to rener per screen

private:

    //--------------------------------------------------------------------------------------
    // Prevent copying instances of this class
    //--------------------------------------------------------------------------------------
    TableControl( const TableControl& ) {}
    TableControl& operator=( const TableControl& rhs ) { return *this; }
};

//--------------------------------------------------------------------------------------
// Name: StaticTableControl
// Desc: Like a table control except that rows are added to the table "up front"
//--------------------------------------------------------------------------------------
class StaticTableControl : public TableControl
{

public:
	StaticTableControl( FLOAT fOriginX, FLOAT fOriginY ) :
	  TableControl( fOriginX, fOriginY )
	{
	}

	~StaticTableControl();

	//--------------------------------------------------------------------------------------
	// Name: AddRowElement
	// Desc: Derived class calls DrawRowElement from implementation of RenderOneRow
	//       in order to draw the next row element
	//--------------------------------------------------------------------------------------
	VOID AddRowElement( WCHAR* wstrFormat, ... );

	//--------------------------------------------------------------------------------------
	// Name: AddRowElementV
	// Desc: Derived class calls DrawRowElement from implementation of RenderOneRow
	//       in order to draw the next row element
	//--------------------------------------------------------------------------------------
	VOID AddRowElementV( WCHAR* wstrFormat, va_list pArgList );

	//--------------------------------------------------------------------------------------
	// Name: RenderOneRow
	// Desc: Render one row of the table
	//--------------------------------------------------------------------------------------
	virtual VOID RenderOneRow( INT iWhichRow );
	
	//--------------------------------------------------------------------------------------
	// Name: RenderEmptyTable
	// Desc: Render the table when it is empty
	//--------------------------------------------------------------------------------------
	virtual VOID RenderEmptyTable(){}

	//--------------------------------------------------------------------------------------
	// Name: GetItemCount
	// Desc: Retrieve the total number of items in the table
	//--------------------------------------------------------------------------------------
	virtual INT GetItemCount();

private:
	
	TextArray m_aRowElements; // Arraw of all row elements
};

//--------------------------------------------------------------------------------------
// Name: UIButton
// Desc: Simple button class
//--------------------------------------------------------------------------------------
class UIScreen;

typedef VOID ( UIScreen::*UIBUTTON_HANDLER ) ( WORD wButtoncode );
#define CALL_BUTTON_HANDLER( screen, handler ) ((screen).*(handler))

class UIButton
{
public:
	UIButton( FLOAT fX, FLOAT fY, WORD wButtonCode, WCHAR* wszButtonText, UIBUTTON_HANDLER ButtonHandler );
	virtual ~UIButton() { Cleanup(); }

	UIButton( const UIButton& rhs );
	UIButton& operator=( const UIButton &rhs );

	VOID Initialize( UIScreen* pUISParent );

	// UIButton Interface
	virtual VOID Update( ATG::GAMEPAD* pGamepad );
	virtual VOID Render();

	BOOL Hide( BOOL bHidden )
	{
		BOOL bResult = m_bHidden;
		m_bHidden = bHidden;
		return bResult;
	}
	
protected:
	VOID Cleanup();

	UIBUTTON_HANDLER m_ButtonHandler;
	FLOAT            m_fX;
	FLOAT            m_fY;
	WORD             m_wButtonCode;
	WCHAR*           m_wszButtonText;
	UIScreen*        m_pUISParent;
	BOOL             m_bHidden;
};

class BackButton : public UIButton
{
public:
	BackButton( FLOAT fX, FLOAT fY, WORD wButtonCode, WCHAR* wszButtonText )
		: UIButton( fX, fY, wButtonCode, wszButtonText, NULL)
	{
	}
		
	VOID Update( ATG::GAMEPAD* pGamepad );
};

class NavButton : public UIButton
{
public:
	NavButton( FLOAT fX, FLOAT fY, WORD wButtonCode, WCHAR* wszButtonText )	:
	  UIButton( fX, fY, wButtonCode, wszButtonText, NULL ),
	  m_pUISNavTo( NULL )
	{
	}
	~NavButton(){}

	NavButton( const NavButton& rhs ) : UIButton( rhs )
	{
		m_pUISNavTo = rhs.m_pUISNavTo;
	}
	NavButton& operator=( const NavButton& rhs )
	{
		if ( &rhs != this )
		{
			UIButton::operator=( rhs );
			m_pUISNavTo = rhs.m_pUISNavTo;
		}
		return *this;
	}
	VOID Initialize( UIScreen* pUISParent, UIScreen* pUISNavTo )
	{
		UIButton::Initialize( pUISParent );
		m_pUISNavTo = pUISNavTo;
	}

	VOID Update( ATG::GAMEPAD* pGamepad );

private:
	UIScreen *m_pUISNavTo;
};

//--------------------------------------------------------------------------------------
// Name: UIScreen
// Desc: Base class for all the user interface screens used by the sample
//--------------------------------------------------------------------------------------
class UIScreen
{
public:
	UIScreen();
	~UIScreen();

	// When the UIScreen is visible the Update method is called every frame
	virtual VOID Update( ATG::GAMEPAD* pGamepad ) = 0;
	
	// When the UIScreen is visible the Render method is called every frame
	virtual VOID Render() = 0;

	UIButton *CreateButton( FLOAT fX,
		                    FLOAT fY, 
							WORD wButtonCode,
							WCHAR* wszButtonText,
							UIBUTTON_HANDLER ButtonHandler,
							BOOL bHidden = FALSE );

	BackButton *CreateBackButton( FLOAT _fX,
		                          FLOAT _fY,
								  WORD _wButtonCode,
								  WCHAR *_wszButtonText,
								  BOOL _bHidden = FALSE );

	NavButton *CreateNavButton( FLOAT _fX,
		                        FLOAT _fY,
								WORD _wButtonCode,
								WCHAR *_wszButtonText,
								UIScreen *_pUISNavTo,
								BOOL _bHidden = FALSE );

	// Navigate to the previous screen
	virtual VOID NavigateBack();

	// Navigate to a new screen
	virtual VOID Navigate( UIScreen* pNewScreen );

	VOID UpdateButtons( ATG::GAMEPAD* pGamepad );
	VOID RenderButtons();

	static VOID S_Init( UIScreen* pInitialScreen )
	{
		assert(s_pUISCurrentScreen == NULL);
		s_pUISCurrentScreen = pInitialScreen;
	}

	static VOID S_Update( ATG::GAMEPAD* pGamepad )
	{
		s_pUISCurrentScreen->Update( pGamepad );
	}

	static VOID S_Render()
	{
		s_pUISCurrentScreen->Render();
	}

protected:
	typedef std::vector<UIButton *> ButtonArray;

	static UIScreen *s_pUISCurrentScreen;
	UIScreen *m_pUISPreviousScreen;
	ButtonArray m_aButtons;

private:
	// Don't copy these things
	UIScreen( const UIScreen &  ){};
	UIScreen &operator=( const UIScreen & ){ return *this; }
};

#endif // UIHELPERS_H