//--------------------------------------------------------------------------------------
// InGameDownloadManager.cpp
//
// Encapsulates the content download process. Provides a method to download a set of
// OfferIDs. Maintains a collection of pending downloads by OfferID and tracks the
// status of downloads.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <vector>
#include <assert.h>
#include <stdio.h>
#include <xtl.h>
#include <xonline.h>
#include "AtgSignIn.h"
#include "AtgUtil.h"

#include "InGameDownloadManager.h"

//--------------------------------------------------------------------------------------
// DownloadManager implementation
//--------------------------------------------------------------------------------------

// The single DownloadManager instance
DownloadManager *DownloadManager::s_TheDownloadManager = NULL;

DownloadManager::DownloadManager() :
m_pCurrentRequest(NULL),
m_hrOverlappedResult(S_OK)
{
    ZeroMemory( &m_Overlapped, sizeof( XOVERLAPPED ) );
    m_Overlapped.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    if( m_Overlapped.hEvent == NULL )
    {
        ATG::FatalError( "Failed to create Overlapped event.\n" );
    }
}

DownloadManager::~DownloadManager()
{
    // Cancel any outstanding request
    if (m_pCurrentRequest)
    {
        XCancelOverlapped( &m_Overlapped );
        delete m_pCurrentRequest;
        m_pCurrentRequest = NULL;
    }

    CloseHandle( m_Overlapped.hEvent );

    assert( s_TheDownloadManager );
    s_TheDownloadManager = NULL;
}

//--------------------------------------------------------------------------------------
// Name: RequestDownload
// Desc: Creates a DownloadRequest object to store OfferIDs to be downloaded.
//       Caller will add OfferIDs to the DownloadRequest object using
//       DownloadRequest::AddOffer
//--------------------------------------------------------------------------------------
DownloadRequest*
DownloadManager::RequestDownload( DWORD dwOffers )
{
    assert( NULL == m_pCurrentRequest );
    if ( NULL == m_pCurrentRequest )
    {
        m_pCurrentRequest = new DownloadRequest ( dwOffers );
        assert( m_pCurrentRequest );
        return m_pCurrentRequest;
    }
    return NULL;
}

//--------------------------------------------------------------------------------------
// Name: DownloadItems
// Desc: Displays the Marketplace blade to the user to allow purchasing/downloading
//       the list of OfferIDs that were added to the DownloadRequest
//--------------------------------------------------------------------------------------
VOID
DownloadManager::DownloadItems( DownloadRequest* pRequest )
{
    DWORD dwErr = ERROR_SUCCESS;

    DWORD dwEntryPoint = XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_PAIDITEMS;
    
    dwErr = XShowMarketplaceDownloadItemsUI(
        ATG::SignIn::GetSignedInUser(),
        dwEntryPoint,
        pRequest->GetOfferIDs(),        // the OfferIDs for the items we're downloading
        pRequest->GetOfferIDCount(),    // the number of OfferIDs to download
        &m_hrOverlappedResult,
        &m_Overlapped                   // XShowMarketplaceDownloadItemsUI must be called asynchronously
        );

    assert( ERROR_IO_PENDING == dwErr );
}

//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Check to see if the Marketplace blade is still showing
//       Refresh the status of all the pending downloads that are being tracked
//--------------------------------------------------------------------------------------
VOID
DownloadManager::Update()
{
    // m_pCurrentRequest is non-null during the call to XShowMarketplaceDownloadItemsUI
    if ( m_pCurrentRequest )
    {
        DWORD dwResult;
        DWORD dwErr = XGetOverlappedResult( &m_Overlapped, &dwResult, FALSE );
        if ( dwErr != ERROR_IO_INCOMPLETE )
        {
            assert( dwErr == ERROR_SUCCESS );

            // Add to the pending downloads
            for( DWORD dw = 0; dw < m_pCurrentRequest->GetOfferIDCount(); ++dw )
            {
                AddPendingDownload( m_pCurrentRequest->GetOfferID(dw) );
            }

            delete m_pCurrentRequest;
            m_pCurrentRequest = NULL;
        }
    }

    RefreshPendingDownloads();
}

//--------------------------------------------------------------------------------------
// Name: AddPendingDownload
// Desc: Add another OfferID to the list of pending downloads without duplication
//--------------------------------------------------------------------------------------
VOID
DownloadManager::AddPendingDownload( ULONGLONG qwOfferID )
{
    // Record the current download status for the given OfferID
    DWORD dwDownloadStatus;
    DWORD dwErr = XMarketplaceGetDownloadStatus( ATG::SignIn::GetSignedInUser(), qwOfferID, &dwDownloadStatus );
    assert( dwErr == ERROR_SUCCESS );

    if ( dwErr == ERROR_SUCCESS )
    {
        // Add the OfferID and corresponding status to the map
        m_mPendingDownloads[qwOfferID] = dwDownloadStatus;
    }
}

//--------------------------------------------------------------------------------------
// Name: RefreshPendingDownloads
// Desc: Get the latest status for each OfferID in the collection
//--------------------------------------------------------------------------------------
VOID
DownloadManager::RefreshPendingDownloads()
{
    DWORD dwErr = ERROR_SUCCESS;
    for ( PendingDownloadMap::iterator iter = m_mPendingDownloads.begin(); iter != m_mPendingDownloads.end(); ++iter )
    {
        if ( iter->second == ERROR_IO_PENDING )
        {
            DWORD dwDownloadStatus;
            dwErr = XMarketplaceGetDownloadStatus( ATG::SignIn::GetSignedInUser(), iter->first, &dwDownloadStatus );
            if ( ERROR_SUCCESS == dwErr )
            {
                iter->second = dwDownloadStatus;
            }
            else
            {
                break;
            }
        }
    }
    assert( ERROR_SUCCESS == dwErr );
}

//--------------------------------------------------------------------------------------
// Name: RemoveExpiredDownloads
// Desc: Clean out all the downloads that have completed or errored out
//--------------------------------------------------------------------------------------
VOID
DownloadManager::RemoveExpiredDownloads()
{
    // First go through the set and find all the keepers
    std::vector<ULONGLONG> vKeepers;
    for ( PendingDownloadMap::iterator iter = m_mPendingDownloads.begin(); iter != m_mPendingDownloads.end(); ++iter )
    {
        if ( ERROR_IO_PENDING == iter->second )
        {
            vKeepers.push_back( iter->first );
        }
    }

    // Clear out the map
    m_mPendingDownloads.clear();
    

    // Now add all the keepers back in if they are still pending
    for ( DWORD dw = 0; dw < vKeepers.size(); ++dw )
    {
        DWORD dwDownloadStatus;
        DWORD dwErr = XMarketplaceGetDownloadStatus( ATG::SignIn::GetSignedInUser(), vKeepers[dw], &dwDownloadStatus );
        assert( dwErr == ERROR_SUCCESS );
        if ( dwErr == ERROR_SUCCESS && dwDownloadStatus == ERROR_IO_PENDING )
        {
            m_mPendingDownloads[vKeepers[dw]] = dwDownloadStatus;
        }
    }
}

//--------------------------------------------------------------------------------------
// DownloadRequest implementation
//--------------------------------------------------------------------------------------

DownloadRequest::DownloadRequest(DWORD dwSize) :
m_dwOfferIDs(0),
m_dwSize(0),
m_pOfferIDs(NULL)
{
    m_pOfferIDs = new ULONGLONG [dwSize];
    assert( m_pOfferIDs );

    if ( m_pOfferIDs )
    {
        m_dwSize = dwSize;
    }
}

DownloadRequest::~DownloadRequest()
{
    if ( m_pOfferIDs )
    {
        delete [] m_pOfferIDs;
    }
    m_dwSize = 0;
    m_dwOfferIDs = 0;
}

//--------------------------------------------------------------------------------------
// Name: AddOffer
// Desc: Adds an OfferID to the array of OfferIDs
//--------------------------------------------------------------------------------------
BOOL
DownloadRequest::AddOffer( ULONGLONG qwOfferIDtoAdd )
{
    if ( m_dwOfferIDs < m_dwSize )
    {
        m_pOfferIDs[m_dwOfferIDs] = qwOfferIDtoAdd;
        ++m_dwOfferIDs;
        return TRUE;
    }
    return FALSE;
}

//--------------------------------------------------------------------------------------
// DownloadScreen implementation
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Add buttons to the screen for controlling download behavior and tracking
//--------------------------------------------------------------------------------------
VOID
DownloadScreen::Initialize()
{
    
		CreateButton( UIHelpers::s_fBUTTON_LEFT_X,
		              UIHelpers::s_fBUTTON_UPPER_Y,
					  XINPUT_GAMEPAD_A,
					  GLYPH_A_BUTTON L"Background Downloading",
					  static_cast< UIBUTTON_HANDLER >( &DownloadScreen::BackgroundDownloadMode ) );

		CreateBackButton( UIHelpers::s_fBUTTON_LEFT_X,
			              UIHelpers::s_fBUTTON_LOWER_Y,
						  XINPUT_GAMEPAD_B,
						  GLYPH_B_BUTTON L"Back to Game Lobby" );

		m_pCleanupDownloadsButton =
			CreateButton( UIHelpers::s_fBUTTON_RIGHT_X,
			              UIHelpers::s_fBUTTON_UPPER_Y,
						  XINPUT_GAMEPAD_X,
						  GLYPH_X_BUTTON L"Cleanup Expired Downloads",
						  static_cast< UIBUTTON_HANDLER >( &DownloadScreen::RemoveExpiredDownloads ),
						  TRUE );
}

//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Render all the buttons on the screen
//--------------------------------------------------------------------------------------
VOID
DownloadScreen::Render()
{
	UIHelpers::DrawText( -300, 0, UIHelpers::TEXT_COLOR, L"Current Downloads" );

	m_DownloadTable.Render( UIHelpers::TEXT_COLOR, UIHelpers::SELECTED_TEXT_COLOR );

    RenderButtons();
}

//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Update the controls on the screen. Show or hide buttons based on the status
//       of the downloads
//--------------------------------------------------------------------------------------
VOID
DownloadScreen::Update( ATG::GAMEPAD* pGamepad )
{
    UpdateButtons( pGamepad );
    m_DownloadTable.Update( pGamepad );

    // Determine if there are any expired downloads and show the button as appropriate
    BOOL bNoExpiredDownloads = TRUE;
	DownloadManager::PendingDownloadMap::iterator iter = DownloadManager::GetSingleton().begin();
	DownloadManager::PendingDownloadMap::iterator it_end = DownloadManager::GetSingleton().end();
    for(; iter != it_end; ++iter)
    {
        if( iter->second != ERROR_IO_PENDING )
        {
            bNoExpiredDownloads = FALSE;
            break;
        }
    }

    m_pCleanupDownloadsButton->Hide( bNoExpiredDownloads );
}

//--------------------------------------------------------------------------------------
// Name: RemoveExpiredDownloads
// Desc: Remove any expired downloads from the list
//--------------------------------------------------------------------------------------
VOID
DownloadScreen::RemoveExpiredDownloads( WORD wButtonCode)
{
    DownloadManager::GetSingleton().RemoveExpiredDownloads();
}

//--------------------------------------------------------------------------------------
// Name: s_BackgroundDownloadMode
// Desc: Show the background downloading screen
//--------------------------------------------------------------------------------------
VOID
DownloadScreen::BackgroundDownloadMode( WORD wButtonCode )
{
    Navigate( new BackgroundDownloadingScreen() );
}

//--------------------------------------------------------------------------------------
// DownloadTable implementation
//--------------------------------------------------------------------------------------
const FLOAT DownloadTable::OFFER_ID_WIDTH        = 500.0f;
const FLOAT DownloadTable::DOWNLOAD_STATUS_WIDTH = 500.0f;

DownloadTable::DownloadTable() : TableControl( 50.0f, 50.0f )
{
	AddColumnHeading( DownloadTable::OFFER_ID_WIDTH, L"Offer ID" );
	AddColumnHeading( DownloadTable::DOWNLOAD_STATUS_WIDTH, L"Download Status" );
}

//--------------------------------------------------------------------------------------
// Name: RenderOneRow
// Desc: Render one row of the table
//--------------------------------------------------------------------------------------
VOID
DownloadTable::RenderOneRow( INT iWhichRow )
{
	DownloadManager::PendingDownloadMap::iterator iter = DownloadManager::GetSingleton().begin();
	DownloadManager::PendingDownloadMap::iterator it_end = DownloadManager::GetSingleton().end();
    for ( INT i = 0; i < iWhichRow; ++i)
    {
        ++iter;
        assert(iter != it_end );
    }

	// Draw the OfferID value
	DWORD *pdw = (DWORD *)&iter->first;
	DrawRowElement( L"0x%08X%08X", pdw[0], pdw[1] );

    // Draw the Download Status value
    switch ( iter->second )
    {
    default:
		DrawRowElement( L"0x%08X", iter->second );
        break;

    case ERROR_SUCCESS:
        DrawRowElement( L"ERROR_SUCCESS" );
        break;

    case ERROR_IO_PENDING:
        DrawRowElement( L"ERROR_IO_PENDING" );
        break;

    case ERROR_NOT_FOUND:
        DrawRowElement( L"ERROR_NOT_FOUND" );
        break;

    case ERROR_DISK_FULL:
        DrawRowElement( L"ERROR_DISK_FULL" );
        break;
    };
}

//--------------------------------------------------------------------------------------
// BackgroundDownloadingScreen implementation
//--------------------------------------------------------------------------------------

BackgroundDownloadingScreen::BackgroundDownloadingScreen()
: UIScreen()
{
	CreateButton( UIHelpers::s_fBUTTON_LEFT_X,
		          UIHelpers::s_fBUTTON_UPPER_Y,
                  XINPUT_GAMEPAD_A,
                  GLYPH_A_BUTTON L"Toggle Download Mode",
				  static_cast< UIBUTTON_HANDLER >( &BackgroundDownloadingScreen::ToggleMode ) );

    CreateBackButton( UIHelpers::s_fBUTTON_LEFT_X,
		              UIHelpers::s_fBUTTON_LOWER_Y,
                      XINPUT_GAMEPAD_B,
                      GLYPH_B_BUTTON L"Back to Download Screen" );
}

//--------------------------------------------------------------------------------------
// Name: s_ToggleMode
// Desc: Button handler that toggles the current download mode between the two 
//       supported modes
//--------------------------------------------------------------------------------------
VOID
BackgroundDownloadingScreen::ToggleMode( WORD wButtonCode )
{
    XBACKGROUND_DOWNLOAD_MODE currentMode = XBackgroundDownloadGetMode();

    switch ( currentMode )
    {
    default:
        assert(FALSE);
        break;

    case XBACKGROUND_DOWNLOAD_MODE_ALWAYS_ALLOW: 
        XBackgroundDownloadSetMode(XBACKGROUND_DOWNLOAD_MODE_AUTO);
        break;

    case XBACKGROUND_DOWNLOAD_MODE_AUTO:
        XBackgroundDownloadSetMode(XBACKGROUND_DOWNLOAD_MODE_ALWAYS_ALLOW);
        break;
    };
}

//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Display the current background downloading mode
//--------------------------------------------------------------------------------------
VOID
BackgroundDownloadingScreen::Render()
{
	UIHelpers::DrawText( -400, 0, UIHelpers::TEXT_COLOR, L"Background Downloading Details" );
    FLOAT fOriginY = 50.0;

	UIHelpers::DrawText( 0.0, fOriginY, UIHelpers::TEXT_COLOR, L"Current background downloading mode:" );
    fOriginY += 50.0;

    XBACKGROUND_DOWNLOAD_MODE currentMode = XBackgroundDownloadGetMode();

    FLOAT fX = 100.0;
    // Draw the Background Download Mode value
    switch ( currentMode )
    {
	default:
		UIHelpers::DrawText( fX, fOriginY, UIHelpers::TEXT_COLOR, L"0x%08X", currentMode );
        break;

    case XBACKGROUND_DOWNLOAD_MODE_ALWAYS_ALLOW: 
		UIHelpers::DrawText( fX, fOriginY, UIHelpers::TEXT_COLOR, L"XBACKGROUND_DOWNLOAD_MODE_ALWAYS_ALLOW" );
        break;

    case XBACKGROUND_DOWNLOAD_MODE_AUTO:
		UIHelpers::DrawText( fX, fOriginY, UIHelpers::TEXT_COLOR, L"XBACKGROUND_DOWNLOAD_MODE_AUTO" );
        break;
    };

    RenderButtons();
}