//--------------------------------------------------------------------------------------
// InGameDownloadManager.h
//
// Encapsulates the content download process. Provides a method to download a set of
// OfferIDs. Maintains a collection of pending downloads by OfferID and tracks the
// status of downloads.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#ifndef DOWNLOAD_MANAGER_H
#define DOWNLOAD_MANAGER_H

#include <map>
#include <xtl.h>
#include <xonline.h>
#include "UIHelpers.h"

//--------------------------------------------------------------------------------------
// Name: DownloadManager
// Desc: Initiates Content Downloads with the user, tracks download status
//--------------------------------------------------------------------------------------
class DownloadRequest;
class DownloadManager
{
public:
    DownloadManager();
    ~DownloadManager();

    //--------------------------------------------------------------------------------------
    // Name: RequestDownload
    // Desc: Creates a DownloadRequest object to store OfferIDs to be downloaded.
    //       Caller will add OfferIDs to the DownloadRequest object using
    //       DownloadRequest::AddOffer
    //--------------------------------------------------------------------------------------
    DownloadRequest* RequestDownload( DWORD nOffers );

    //--------------------------------------------------------------------------------------
    // Name: DownloadItems
    // Desc: Displays the Marketplace blade to the user to allow purchasing/downloading
    //       the list of OfferIDs that were added to the DownloadRequest
    //--------------------------------------------------------------------------------------
    VOID DownloadItems( DownloadRequest *pRequest );

    //--------------------------------------------------------------------------------------
    // Name: Update
    // Desc: Check to see if the Marketplace blade is still showing
    //       Refresh the status of all the pending downloads that are being tracked
    //--------------------------------------------------------------------------------------
    VOID Update();

	typedef std::map< ULONGLONG, DWORD > PendingDownloadMap;

    // Enable iterating over the collection of pending downloads
    DWORD Count() { return m_mPendingDownloads.size(); }
    PendingDownloadMap::iterator begin() { return m_mPendingDownloads.begin(); }
    PendingDownloadMap::iterator end()   { return m_mPendingDownloads.end(); }

private:
    //--------------------------------------------------------------------------------------
    // Track Pending Downloads
    //--------------------------------------------------------------------------------------

    //--------------------------------------------------------------------------------------
    // Name: AddPendingDownload
    // Desc: Add another OfferID to the list of pending downloads without duplication
    //--------------------------------------------------------------------------------------
    VOID AddPendingDownload( ULONGLONG qwOfferID );

    //--------------------------------------------------------------------------------------
    // Name: RefreshPendingDownloads
    // Desc: Get the latest status for each OfferID in the collection
    //--------------------------------------------------------------------------------------
    VOID RefreshPendingDownloads();

public:

    //--------------------------------------------------------------------------------------
    // Name: RemoveExpiredDownloads
    // Desc: Clean out all the downloads that have completed or errored out
    //--------------------------------------------------------------------------------------
    VOID RemoveExpiredDownloads();
	
    PendingDownloadMap m_mPendingDownloads;

private:
    // Requesting Offer Downloads
    DownloadRequest *m_pCurrentRequest;
    HRESULT m_hrOverlappedResult;
    XOVERLAPPED m_Overlapped;

public:
    // Implementation of Singleton 

    static DownloadManager &GetSingleton()
    {
		if ( !s_TheDownloadManager )
		{
			s_TheDownloadManager = new DownloadManager();
		}

        assert( s_TheDownloadManager );
        return ( *s_TheDownloadManager );
    }

	static VOID DeleteInstance()
	{
		delete ( s_TheDownloadManager );
		s_TheDownloadManager = NULL;
	}

private:
    // The single instance of this class
    static DownloadManager *s_TheDownloadManager;

    // Keep these private to prevent copying
    DownloadManager( const DownloadManager& );
    DownloadManager &operator=( const DownloadManager& );
};

//--------------------------------------------------------------------------------------
// Name: DownloadRequest
// Desc: Auxilliary class to encapsulate a list of OfferIDs for content to be downloaded
//       DownloadRequests are created and managed by the DownloadManager
//--------------------------------------------------------------------------------------
class DownloadRequest
{
public:
    DownloadRequest( DWORD nSize );
    ~DownloadRequest();

    //--------------------------------------------------------------------------------------
    // Name: AddOffer
    // Desc: Adds an OfferID to the array of OfferIDs
    //--------------------------------------------------------------------------------------
    BOOL AddOffer( ULONGLONG qwOfferIDtoAdd );

private:
    DWORD m_dwOfferIDs;       // Number of OfferIDs in the request
    DWORD m_dwSize;           // Max number of OfferIDs that will fit in the request
    ULONGLONG *m_pOfferIDs;   // Storage for the OfferIDs

public:
    
    DWORD GetOfferIDCount() const { return m_dwOfferIDs; }
    
    const ULONGLONG* GetOfferIDs() const { return m_pOfferIDs; }
    
    ULONGLONG GetOfferID( DWORD dwIndex ) const
    {
        assert( dwIndex < m_dwOfferIDs );
        return m_pOfferIDs[dwIndex];
    }

private:
    // don't want to copy these
    DownloadRequest( const DownloadRequest &) {}
    DownloadRequest &operator=( const DownloadRequest &) { return *this; } 
};

//--------------------------------------------------------------------------------------
// Name: DownloadTable
// Desc: Allow the user to scroll through the list of downloads to look at status
//--------------------------------------------------------------------------------------
class DownloadTable : public TableControl
{

public:

    DownloadTable();

	//--------------------------------------------------------------------------------------
	// Name: RenderOneRow
	// Desc: Render one row of the table
	//--------------------------------------------------------------------------------------
	virtual VOID RenderOneRow( INT iWhichRow );

	//--------------------------------------------------------------------------------------
	// Name: RenderEmptyTable
	// Desc: Render the table when it is empty
	//--------------------------------------------------------------------------------------
	virtual VOID RenderEmptyTable()	{ DrawRowElement( L"No Downloads" ); }
	
	//--------------------------------------------------------------------------------------
	// Name: GetItemCount
	// Desc: Retrieve the total number of items in the table
	//--------------------------------------------------------------------------------------
	virtual INT GetItemCount() { return DownloadManager::GetSingleton().Count(); }

private:

    // formatting constants
    static const FLOAT OFFER_ID_WIDTH;
	static const FLOAT DOWNLOAD_STATUS_WIDTH;
};

//--------------------------------------------------------------------------------------
// Name: DownloadScreen
// Desc: Display status of CurrentDownloads
//--------------------------------------------------------------------------------------
class DownloadScreen : public UIScreen
{
public:
    DownloadScreen() : m_pCleanupDownloadsButton(NULL) {}

    //--------------------------------------------------------------------------------------
    // Name: Initialize
    // Desc: Add buttons to the screen for controlling download behavior and tracking
    //--------------------------------------------------------------------------------------
    VOID Initialize();

    //--------------------------------------------------------------------------------------
    // Name: Render
    // Desc: Render all the buttons on the screen
    //--------------------------------------------------------------------------------------
    virtual VOID Render();

    //--------------------------------------------------------------------------------------
    // Name: Update
    // Desc: Update the controls on the screen. Show or hide buttons based on the status
    //       of the downloads
    //--------------------------------------------------------------------------------------
    virtual VOID Update( ATG::GAMEPAD *_pGamepad );

private:

    //--------------------------------------------------------------------------------------
    // Button handlers
    //--------------------------------------------------------------------------------------

    //--------------------------------------------------------------------------------------
    // Name: RemoveExpiredDownloads
    // Desc: Remove any expired downloads from the list
    //--------------------------------------------------------------------------------------
    VOID RemoveExpiredDownloads( WORD wButtonCode );

    //--------------------------------------------------------------------------------------
    // Name: BackgroundDownloadMode
    // Desc: Show the background downloading screen
    //--------------------------------------------------------------------------------------
    VOID BackgroundDownloadMode( WORD wButtonCode );
    
    UIButton *m_pCleanupDownloadsButton;
    
private:
    DownloadTable m_DownloadTable;
};

//--------------------------------------------------------------------------------------
// Name: BackgoundDownloadingScreen
// Desc: Allow the user to view and change the background downloading mode
//--------------------------------------------------------------------------------------
class BackgroundDownloadingScreen : public UIScreen
{
public:
    BackgroundDownloadingScreen();
    
    //--------------------------------------------------------------------------------------
    // Name: Render
    // Desc: Display the current background downloading mode
    //--------------------------------------------------------------------------------------
    virtual VOID Render();

    //--------------------------------------------------------------------------------------
    // Name: Update
    // Desc: Updates the buttons on the screen
    //--------------------------------------------------------------------------------------
    virtual VOID Update( ATG::GAMEPAD* pGamepad ) {	UpdateButtons( pGamepad );	}

    //--------------------------------------------------------------------------------------
    // Name: NavigateBack
    // Desc: Cleanup each instance when the back button is pressed.
    //--------------------------------------------------------------------------------------
    virtual VOID NavigateBack()
    {
        UIScreen::NavigateBack();
        delete this;
    }

private:

    //--------------------------------------------------------------------------------------
    // Name: ToggleMode
    // Desc: Button handler that toggles the current download mode between the two 
    //       supported modes
    //--------------------------------------------------------------------------------------
    VOID ToggleMode( WORD _wButtonCode );
};



#endif // DOWNLOAD_MANAGER_H