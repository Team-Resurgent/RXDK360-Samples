//--------------------------------------------------------------------------------------
// Compilation.cpp
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <AtgApp.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>

//--------------------------------------------------------------------------------------
// Only enumerate MAX_PACKAGE_COUNT packages in this sample
//--------------------------------------------------------------------------------------
static const DWORD  MAX_PACKAGE_COUNT = 10;

//--------------------------------------------------------------------------------------
// Color values
//--------------------------------------------------------------------------------------
#define SEL_COLOR           0xffff0000          // selection color
#define UNSEL_COLOR         0xffffffff          // de-selection color

#define MSG_COLOR           0xffffffff          // message color
#define INFO_COLOR          0xffffff00          // information display color

#define TOP_BACK_COLOR      0xffbbffbb          // background gradient colors
#define BOTTOM_BACK_COLOR   0xff000000

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] = 
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2,  L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_1, L"Launch selection" },
    { ATG::HELP_X_BUTTON, ATG::HELP_PLACEMENT_1, L"Search for packages" },
};
static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE(g_HelpCallouts);
static const DWORD MAX_FRIENDLY_NAME = 256;

//--------------------------------------------------------------------------------------
// Implement a meta-data scheme to identify packages on the disc based on TitleID
// Note: It is recommended to associate the meta-data with the TitleID of the package
//       rather than using the filename. The package filename will be set to an
//       arbitrary value by the mastering lab after you submit.
//--------------------------------------------------------------------------------------
struct PackageMetaData
{
    DWORD dwTitleID;
    WCHAR wstrFriendlyName[MAX_FRIENDLY_NAME];
};

PackageMetaData g_PackageMap[] =
{
    { 0x58410843, L"ArcadeSample" },
};

//--------------------------------------------------------------------------------------
// Basic directory search results
//--------------------------------------------------------------------------------------
struct PackageSearchResult
{
    DWORD dwTitleID;
    CHAR strFileName[ MAX_PATH ];
};

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{

public:

    // All samples have Initialize,Update and Render
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
    
    VOID LaunchSelectedTitle();

    // This sample searches the entire game: volume for packages.
    // Note: In a real compilation you will know the folder that contains each package
    //       and so searching would not be necessary.
    VOID FindPackagesOnGameDisc( CHAR* strFileName );

private:

    // Helper functions for FindPackagesOnGameDisc
    PackageMetaData* FindMetaData( DWORD dwTitleID );
    BOOL IsContentPackage( CHAR* strFileName );

    // Variables implement a list of packages
    DWORD               m_dwNumPackages;
    DWORD               m_dwSelectedIndex;
    PackageSearchResult m_SearchResults[MAX_PACKAGE_COUNT];

    // Standard variables used by samples
    ATG::Font           m_Font;
    ATG::Help           m_Help;
    BOOL                m_bDrawHelp;
};

//--------------------------------------------------------------------------------------
// Name: FindMetaData()
// Desc: Lookup the meta data for a given TitleID
//--------------------------------------------------------------------------------------
PackageMetaData *Sample::FindMetaData( DWORD dwTitleID )
{
    for( INT i = 0; i < ARRAYSIZE( g_PackageMap ); ++i )
    {
        if( g_PackageMap[i].dwTitleID == dwTitleID )
        {
            return &g_PackageMap[i];
        }
    }
    return NULL;
}

//--------------------------------------------------------------------------------------
// Name: IsContentPackage()
// Desc: Guess whether it is a content package based on filename
//--------------------------------------------------------------------------------------
BOOL Sample::IsContentPackage( CHAR* strFileName )
{
    CHAR* p = strFileName;
    while( '\0' != *p )
    {
        if( !isxdigit( UCHAR( *p ) ) )
            return FALSE;

        ++p;
    }

    return TRUE;
}

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Create resources an initialize member variables
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

    // Initialize package search parameters
    m_dwNumPackages = 0;
    m_dwSelectedIndex = 0;
    ZeroMemory( m_SearchResults, sizeof( m_SearchResults ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame. Check input and respond to button presses.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        FindPackagesOnGameDisc( "game:" );
    }

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Load selected data
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        if( m_dwNumPackages > 0 )
        {
            LaunchSelectedTitle();
        }
    }

    // Allow user to select among content display names
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        if( m_dwSelectedIndex > 0 )
        {
            m_dwSelectedIndex--;
        }
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        if( ( m_dwSelectedIndex + 1 ) < m_dwNumPackages )
        {
            m_dwSelectedIndex++;
        }
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
    ATG::RenderBackground( TOP_BACK_COLOR, BOTTOM_BACK_COLOR );

    // Show title and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, MSG_COLOR, L"Compilation" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        // Display the package names
        if ( 0 == m_dwNumPackages )
        {
            FLOAT fD = 30.0f; // Line spacing delta
            FLOAT fY = 40.0f; // Current line as Y coordinate
            m_Font.DrawText(0.0f, fY+=fD, SEL_COLOR, L"Press X to search for packages.\n" );
            m_Font.DrawText(0.0f, fY+=fD, SEL_COLOR, L"Place each Content package in the content folder on the game disc:\n" );
            m_Font.DrawText(0.0f, fY+=fD, SEL_COLOR, L"game:\\content\\0000000000000000\\<titleid>\\<contenttype>\n" );
            m_Font.DrawText(0.0f, fY+=fD, SEL_COLOR, L"For XBLA: <contenttype> = 000D0000\n" );
            m_Font.DrawText(0.0f, fY+=fD, SEL_COLOR, L"For Demo: <contenttype> = 00080000\n" );
            m_Font.DrawText(0.0f, fY+=fD, SEL_COLOR, L"<titleid> is the Title ID of the XBLA/Demo package\n" );
        }
        else
        {
            m_Font.SetScaleFactors( 1.f, 1.f );
            m_Font.DrawText( 200, -34, MSG_COLOR, GLYPH_A_BUTTON L" Launch Package\n" );

            m_Font.SetScaleFactors( 1.0f, 1.0f );

            // Draw list of content display names
            for( DWORD i = 0; i < m_dwNumPackages; ++i )
            {
                FLOAT fTextX;
                FLOAT fTextY;
                if( i < ( MAX_PACKAGE_COUNT / 2 ) )
                {
                    fTextX = 0;
                    fTextY = (FLOAT)( 70 + ( i * 40 ) );
                }
                else
                {
                    fTextX = 300;
                    fTextY = (FLOAT)( 70 + ( ( i - ( MAX_PACKAGE_COUNT / 2 ) ) * 40 ) );
                }

                // Display content name
                PackageMetaData *pPMD = FindMetaData( m_SearchResults[i].dwTitleID );
                if( pPMD )
                {
                    m_Font.DrawText( fTextX, fTextY, i == m_dwSelectedIndex ? SEL_COLOR : UNSEL_COLOR, pPMD->wstrFriendlyName );
                }
                else
                {
                    WCHAR szDisplayName[MAX_PATH];
                    MultiByteToWideChar( CP_ACP, 0, m_SearchResults[i].strFileName, -1, szDisplayName, MAX_PATH );
                    m_Font.DrawText( fTextX, fTextY, i == m_dwSelectedIndex ? SEL_COLOR : UNSEL_COLOR, szDisplayName );
                }
            }
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: FindPackagesOnGameDisc
// Desc: Produce a list of package file names for packages on the game disc
//--------------------------------------------------------------------------------------
VOID Sample::FindPackagesOnGameDisc( CHAR* strFileName )
{
    CHAR strFind[ MAX_PATH ];
    sprintf_s( strFind, "%s\\*", strFileName );

    WIN32_FIND_DATA wfd;
    
    HANDLE hFind = FindFirstFile( strFind, &wfd );

    if( INVALID_HANDLE_VALUE == hFind )
    {
        return;
    }

    do
    {
        if( m_dwNumPackages >= ARRAYSIZE(m_SearchResults) )
            break;
        if( (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY )
        {
            DWORD dwTitleID = 0;
            if ( sscanf_s( wfd.cFileName, "%8x", &dwTitleID, sizeof(DWORD) ) )
            {
                if( FindMetaData( dwTitleID ) )
                {
                    m_SearchResults[m_dwNumPackages].dwTitleID = dwTitleID;
                }
            }

            CHAR strFound[ MAX_PATH ];
            sprintf_s( strFound, "%s\\%s", strFileName, wfd.cFileName );
            FindPackagesOnGameDisc( strFound );
        }
        else if( IsContentPackage( wfd.cFileName ) )
        {
            sprintf_s( m_SearchResults[m_dwNumPackages++].strFileName, "%s\\%s", strFileName, wfd.cFileName );
        }
    }
    while ( FindNextFile( hFind, &wfd ) );

    FindClose( hFind );
}


//--------------------------------------------------------------------------------------
// Name: LaunchSelectedTitle
// Desc: Uses XContentLaunchImageFromFile to launch the current selection
//--------------------------------------------------------------------------------------
void Sample::LaunchSelectedTitle()
{
    XContentLaunchImageFromFile( m_SearchResults[m_dwSelectedIndex].strFileName, "default.xex" );

    // XContentLaunchImageFromFile should never return. If you wind up here then there was an error.
}

