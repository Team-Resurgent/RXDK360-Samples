//--------------------------------------------------------------------------------------
// File: EndianSwapping.cpp
//
// Desc: Sample to demonstrate the need for dealing with byte ordering (endianness)
//       issues for Xbox 360, along with a technique for handling byte swapping
//       effectively.
//       This sample uses loading a zip file as the main example. It also loads
//       unicode text and some DWORD values from this zip file and displays them.
//
//       Note that byte swapping for endianness should generally be done on your
//       development PC when creating content rather than on the console. However the
//       same techniques and the same code apply.
//
// Hist: 04.10.04 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <assert.h>
#include "ZipFile.h"
#include "EndianSwitch.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_B_BUTTON,  ATG::HELP_PLACEMENT_2, L"Toggle\nendianness" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Name: struct Factorial
// Desc: Simple structure to supply some data for byte swapping
//--------------------------------------------------------------------------------------
struct Factorial
{
    DWORD n;          // Value we are storing the factorial for.
    DWORD factorial;  // Factorial value.
};


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Application class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    CZipFile m_ZipFile;

    Factorial* m_FactorialData;
    DWORD m_FactorialCount;   // Number of Factorial structs

    WCHAR* m_TextData;
    DWORD m_TextCount;    // Number of unicode characters in m_TextData

public:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize base member variables
    m_bDrawHelp = FALSE;

    // Create the fonts
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Load the zip file
    if( !m_ZipFile.LoadZipFile( "game:\\media\\DataFiles\\EndianSwappingData.zip" ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Get the text data from the zip file, then process it.
    DWORD size;
    if( !m_ZipFile.GetFile( "readme.txt", ( CHAR** )&m_TextData, &size ) )
        return ATGAPPERR_MEDIANOTFOUND;
    m_TextCount = size / sizeof( m_TextData[0] );

    // Make sure the text is in big-endian form.
    if( m_TextData[0] != 0xFEFF )
    {
        // Make sure it's validly labeled unicode text
        assert( m_TextData[0] == 0xFFFE );

        // If the text is not big-endian then we need to make it big-endian.

        // Swap all of the words in the text. Pass in the start point, end point,
        // swap description (shorts), and repeat count. This technique works
        // particularly well when you have an array of structures, since the
        // arbitrarily complex structure definition will be repeated.
        //EndianSwitchWorker( m_TextData, m_TextData + m_TextCount, "s", m_TextSize / 2 );

        // Alternately, call a low-level function that will perform this simple
        // task faster.
        EndianSwitchWords( ( WORD* )m_TextData, m_TextCount );
    }

    // Text files tend to not have a terminating zero, but the Font class insists on it.
    // So, we move the text down one character, covering up the Unicode marker, and making
    // space for a zero.
    memmove( m_TextData, m_TextData + 1, ( m_TextCount - 1 ) * sizeof( m_TextData[0] ) );
    // Insert a zero, now that there's room.
    m_TextData[ m_TextCount - 1 ] = 0;


    // Now load the factorial data - although it is in little endian format we
    // intentionally don't fix the byte ordering in order to demonstrate what data
    // looks like if it isn't byte swapped.
    if( !m_ZipFile.GetFile( "data/factorials.bin", ( CHAR** )&m_FactorialData, &size ) )
        return ATGAPPERR_MEDIANOTFOUND;
    m_FactorialCount = size / sizeof( m_FactorialData[0] );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        // Swap all of the words in the text. Pass in the start point, end point,
        // swap description (shorts), and repeat count. This technique works
        // particularly well when you have an array of structures, since the
        // arbitrarily complex structure definition will be repeated.
        // In this case the structure definition is very simple, but it
        // demonstrates the general technique.
        EndianSwitchWorker( m_FactorialData, m_FactorialData + m_FactorialCount,
                            "ii", m_FactorialCount );

        // Alternately, call a low-level function that will perform this simple
        // task faster.
        //EndianSwitchDWords( m_FactorialData, m_FactorialCount * 2 );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    m_Font.Begin();
    // Draw the unicode text from the zip file.
    m_Font.DrawText( 0, 35, 0xffffffff, m_TextData );

    // Draw the factorial data from the zip file - it may be incorrectly endian swapped.
    for( DWORD i = 0; i < m_FactorialCount; ++i )
    {
        WCHAR strBuffer[100];
        swprintf_s( strBuffer, L"%d factorial is %d\n", m_FactorialData[ i ].n,
                    m_FactorialData[ i ].factorial );
        m_Font.DrawText( 0, 160.0f + i * 24.0f, 0xffffffff, strBuffer );
    }
    m_Font.End();

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"EndianSwapping" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
