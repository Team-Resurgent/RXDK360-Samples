//--------------------------------------------------------------------------------------
// XRNMHelpers.cpp
//
// Contains XRNM-related functionality for the FriendMatchmaking sample
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <AtgInput.h>
#include <malloc.h>       // for _alloca
#include "FriendMatchmaking.h"
#include "Messages.h"


//--------------------------------------------------------------------------------------
// Name: CreateXRNMEndpoint()
// Desc: Initialize the endpoint for XRNM. This is the equivalent of a bound socket.
//--------------------------------------------------------------------------------------
HRESULT Sample::CreateXRNMEndpoint( VOID )
{
    return XrnmCreateEndpoint( NULL, NULL, &m_hEndpoint );
}


//--------------------------------------------------------------------------------------
// Name: AllowInboundLinks()
// Desc: Turn on or off autorejection of incoming connection requests. At least one
// peer must have this set to TRUE to allow connections to be formed.
//--------------------------------------------------------------------------------------
HRESULT Sample::AllowInboundLinks( BOOL bAllow )
{
    HRESULT hr = S_OK;

    if( m_hEndpoint == NULL )
    {
        hr = E_FAIL;
    }

    hr = SUCCEEDED( hr ) ? XrnmAllowInboundLinkRequests( m_hEndpoint, bAllow ) : hr;

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: CreateOutboundLink()
// Desc: This is a simple wrapper around XrnmCreateOutboundLink() just to keep
// all the XRNM calls in one place
//--------------------------------------------------------------------------------------
HRESULT Sample::CreateOutboundLink( const XRNM_ADDRESS* addr, _In_opt_ const BYTE* pbData, DWORD cbData, _Inout_ ULONG_PTR pUserData,
                                    _Out_ XRNM_HANDLE* pLink )
{
    return
        XrnmCreateOutboundLink(
        m_hEndpoint,
        addr,
        pbData,
        cbData,
        NULL,
        pUserData,
        pLink );
}


//--------------------------------------------------------------------------------------
// Name: TerminateLink()
// Desc: This is a simple wrapper around XrnmTerminateLink() just to keep
// all the XRNM calls in one place
//--------------------------------------------------------------------------------------
HRESULT Sample::TerminateLink( XRNM_HANDLE hLink )
{
    return
        XrnmTerminateLink( hLink );
}


//--------------------------------------------------------------------------------------
// Name: ProcessXRNMEvents
// Desc: Manage the XRNM event queue. This is the primary way XRNM communicates
// with the application
//--------------------------------------------------------------------------------------
HRESULT Sample::ProcessXRNMEvents( VOID )
{
    HRESULT hr;
    XRNM_EVENT* pEvent = NULL;

    while( SUCCEEDED( hr = XrnmGetEvent( m_hEndpoint, 0, &pEvent ) ) &&
           hr != XRN_S_NOEVENTS )
    {

        if( pEvent && ( m_hSession != INVALID_HANDLE_VALUE ) )
        {
            // There is an event! Process it when the session is valid.
            switch( pEvent->EventType )
            {
                case XRNM_EVENT_TYPE_DATA_RECEIVED:
                    hr = ProcessDataReceivedEvent( ( XRNM_EVENT_DATA_RECEIVED* )pEvent );
                    break;

                case XRNM_EVENT_TYPE_LINK_STATUS_UPDATE:
                    hr = ProcessLinkStatusUpdateEvent( ( XRNM_EVENT_LINK_STATUS_UPDATE** )&pEvent );
                    break;

                case XRNM_EVENT_TYPE_INBOUND_LINK_REQUEST:
                    hr = ProcessInboundLinkRequestEvent( ( XRNM_EVENT_INBOUND_LINK_REQUEST* )pEvent );
                    break;

                case XRNM_EVENT_TYPE_RECEIPT:
                    hr = ProcessReceiptEvent( ( XRNM_EVENT_RECEIPT* )pEvent );
                    break;

                case XRNM_EVENT_TYPE_ALERT:
                    hr = ProcessAlertEvent( ( XRNM_EVENT_ALERT* )pEvent );
                    break;

                default:
                    // we don't process other events
                    break;
            }
        }

        if( pEvent )
        {
            // Return the event to XRNM; this is crucial after processing
            XrnmReturnEvent( pEvent );
        }

    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: ProcessInboundLinkRequestEvent
// Desc: Somebody is attempting to connect to us. Let's see who it is.
//--------------------------------------------------------------------------------------
HRESULT Sample::ProcessInboundLinkRequestEvent( _Inout_ XRNM_EVENT_INBOUND_LINK_REQUEST* pEvent )
{
    HRESULT hr = S_OK;
    JOINRESPONSE response = JOINRESPONSE_APPROVED;

    SConsole joiner = { 0 };       // to hold info on the newbie

    if( response == JOINRESPONSE_APPROVED )
    {
        // In this version of the sample, we're strictly client-server. If we get a
        // link request and we're not hosting, there's a problem
        if( m_AppState.top() != APPSTATE_HOSTING )
        {
            response = JOINRESPONSE_NOTHOSTING;
        }
    }

    // Get info on the new client
    CMessage* pMsg = NULL;

    if( FAILED( CMessage::CreateMessage( pEvent->pbyLinkRequestData, pEvent->dwLinkRequestDataSize, &pMsg ) ) )
    {
        response = JOINRESPONSE_BADREQUEST;
    }

    if( response == JOINRESPONSE_APPROVED )
    {
        if( !pMsg || pMsg->GetMessageID() != MSG_CLIENTINFO )
        {
            response = JOINRESPONSE_BADREQUEST;
        }
    }

    if( response == JOINRESPONSE_APPROVED )
    {
        CClientInfoMsg* pClientInfoMsg = ( CClientInfoMsg* )pMsg;

        // save off the joiner's info
        if( FAILED( CopyMsgToConsole( &joiner, pClientInfoMsg, 0 ) ) )
        {
            response = JOINRESPONSE_BADREQUEST;
        }
    }

    if( response == JOINRESPONSE_APPROVED )
    {
        // Check to make sure we've got room!
        if( joiner.cPlayers + m_cPlayers > PUBLICSLOTS + PRIVATESLOTS )
        {
            response = JOINRESPONSE_SESSIONFULL;
        }
    }

    if( response != JOINRESPONSE_APPROVED )
    {
        // Too bad, so sad
        hr = XrnmDenyInboundLink( pEvent, ( BYTE* )&response, sizeof( response ) );
    }
    else
    {
        // All appears to be well at this point. Let's approve the new link

        // Assign the newbie positions and IDs
        for( UCHAR i = 0; i < joiner.cPlayers; i++ )
        {
            joiner.Players[ i ].nPlayerID = m_nNextPlayerID++;
            joiner.Players[ i ].fXPos = rand() / ( 1.0f * RAND_MAX );
            joiner.Players[ i ].fYPos = rand() / ( 1.0f * RAND_MAX );
            joiner.Players[ i ].bLocal = FALSE;
        }

        // Create three client info messages:
        //    1. Tell the new client about his own updated information and the host 
        //    2. Tell the new client about any additional players in the session
        //    3. Tell existing players about the new client

        CClientInfoMsg msgResponse;
        msgResponse.cConsoles = 2;  // host + joiner
        CopyConsoleToMsg( &msgResponse, &joiner, 0 );
        CopyConsoleToMsg( &msgResponse, &m_Consoles.front(), 1 );

        CClientInfoMsg msgToExistingPlayers;
        msgToExistingPlayers.cConsoles = 1;
        CopyConsoleToMsg( &msgToExistingPlayers, &joiner, 0 );

        CClientInfoMsg msgToNewPlayer;
        UINT iIndex = 0;
        for( std::list <SConsole>::iterator i = ++m_Consoles.begin(); i != m_Consoles.end(); i++ )
        {
            CopyConsoleToMsg( &msgToNewPlayer, &*i, iIndex++ );
        }
        msgToNewPlayer.cConsoles = iIndex;

        // Serialize the client list
        BYTE* pbData = NULL;
        UINT cbData = 0;

        hr = ByteStream::CalculateSize( &msgResponse, &cbData );

        if( SUCCEEDED( hr ) )
        {
            pbData = ( BYTE* )_malloca( cbData );

            ByteStream stream( TRUE, pbData, cbData );
            stream.Stream( &msgResponse );
            hr = stream.GetHRESULT();
        }

        if( SUCCEEDED( hr ) )
        {
            // Add the newbie to the list
            m_Consoles.push_back( joiner );

            // Add him to the session
            AddRemoteUsersToSession( &joiner );

            m_Console.Printf(
                L"Accepting inbound link request from new player ID %s.",
                joiner.Players[ 0 ].wstrGamertag );

            // Send the response, storing a pointer to the SConsole in the user data for the link
            hr = XrnmCreateInboundLink(
                pEvent,
                pbData,
                cbData,
                NULL,
                ( ULONG_PTR )&m_Consoles.back(),
                &m_Consoles.back().hLink );
        }

        // Tell existing players about the new player
        if( SUCCEEDED( hr ) )
        {
            hr = SendMessageToAll(
                XRNM_DEFAULT_GAMEDATA_SEND_CHANNEL_ID,
                &msgToExistingPlayers,
                XRNM_FLAG_SEND_RELIABLE,
                &joiner.xnaddr );
        }

        // Tell the new player about existing players
        if( SUCCEEDED( hr ) && msgToNewPlayer.cConsoles )
        {
            hr = SendMessage(
                XRNM_DEFAULT_GAMEDATA_SEND_CHANNEL_ID,
                m_Consoles.back().hLink,
                &msgToNewPlayer,
                XRNM_FLAG_SEND_RELIABLE );
        }


        _freea( pbData );
    }


    return hr;
}


//--------------------------------------------------------------------------------------
// Name: ProcessLinkStatusUpdateEvent
// Desc: The status of a link has changed. Either we're good to go, or we're lost
//--------------------------------------------------------------------------------------
HRESULT Sample::ProcessLinkStatusUpdateEvent( _Inout_ XRNM_EVENT_LINK_STATUS_UPDATE** ppEvent )
{
    HRESULT hr = S_OK;

    XRNM_EVENT_LINK_STATUS_UPDATE* pEvent = *ppEvent;

    // Grab a pointer to the SConsole associated with this link
    SConsole* pConsole = ( SConsole* )pEvent->ulpLinkUserData;

    switch( pEvent->NewStatus )
    {
        case XRNM_STATUS_ACTIVE:
            // we have a good link!
            if( m_AppState.top() == APPSTATE_JOINING )
            {
                // We've been accepted. Get the remote player info
                CMessage* pMsg = NULL;

                hr = CMessage::CreateMessage( pEvent->pbyReplyData, pEvent->dwReplyDataSize, &pMsg );

                if( SUCCEEDED( hr ) )
                {
                    if( !pMsg || pMsg->GetMessageID() != MSG_CLIENTINFO )
                    {
                        hr = E_FAIL;
                    }
                    else
                    {
                        CClientInfoMsg* pClientInfoMsg = ( CClientInfoMsg* )pMsg;

                        // The first chunk will be our updated info from the host, the 
                        // second will be the host's information. Any further chunks will
                        // be other peers
                        UINT iConsole = 0;


                        for( std::list <SConsole>::iterator i = m_Consoles.begin();
                             iConsole < pClientInfoMsg->cConsoles && SUCCEEDED( hr );
                             iConsole++, i++ )
                        {
                            if( i == m_Consoles.end() )
                            {
                                SConsole console = { 0 };
                                m_Consoles.push_back( console );
                                i = --m_Consoles.end();
                            }

                            // Save off the data
                            hr = CopyMsgToConsole( &*i, pClientInfoMsg, iConsole );

                            if( i != m_Consoles.begin() )
                            {
                                AddRemoteUsersToSession( &*i );
                            }

                        }

                        // We're in the game now
                        PopState();
                        PushState( APPSTATE_JOINED );
                    }
                }

                if( pMsg )
                {
                    delete pMsg;
                }
            }

            m_Console.Printf( L"Connection now active to %s", pConsole->Players[ 0 ].wstrGamertag );

            break;

        case XRNM_STATUS_TERMINATING:
            if( pEvent->OldStatus == XRNM_STATUS_ACTIVATING )
            {
                // An activating link is dying. Is this an attempt to join?
                if( m_AppState.top() == APPSTATE_JOINING )
                {
                    // Do we have a valid response code?
                    if( pEvent->dwReplyDataSize == sizeof( JOINRESPONSE ) )
                    {
                        m_wstrError[ 0 ] = L'\0';

                        switch( *( JOINRESPONSE* )( pEvent->pbyReplyData ) )
                        {
                            case JOINRESPONSE_BADREQUEST:
                                wcscpy_s( m_wstrError,
                                          L"The join request failed because of malformed data." );
                                break;
                            case JOINRESPONSE_NOTHOSTING:
                                wcscpy_s( m_wstrError,
                                          L"The join request failed because the host isn't hosting." );
                                break;
                            case JOINRESPONSE_SESSIONFULL:
                                wcscpy_s( m_wstrError,
                                          L"The join request failed because the session is full." );
                                break;
                        }
                    }

                    if( !*m_wstrError )
                    {
                        wcscpy_s( m_wstrError, L"The join request failed for an unknown reason." );
                    }

                    WCHAR wstrErrorCode[ 16 ];
                    swprintf_s( wstrErrorCode, L" (0x%08x)", pEvent->hrNewStatusInfo );
                    wcscat_s( m_wstrError, wstrErrorCode );

                    // Close our session
                    if( m_hSession != INVALID_HANDLE_VALUE  )
                    {
                        CloseSession();
                    }

                    PushState( APPSTATE_ERROR );
                }
                else
                {
                    m_Console.Printf( L"Player %s link failed to connect, hr = 0x%08x.",
                                      pConsole->Players[ 0 ].wstrGamertag,
                                      pEvent->hrNewStatusInfo );
                }
            }
            else
            {
                // we've been notified of a terminating link. Let's wait and see who it is when it's
                // finally terminated
                m_Console.Printf(
                    L"Player %s link is now terminating, hr = 0x%08x.\n",
                    pConsole->Players[ 0 ].wstrGamertag,
                    pEvent->hrNewStatusInfo );
            }
            break;

        case XRNM_STATUS_TERMINATED:
            m_Console.Printf(
                L"Player %s link is now terminated, hr = 0x%08x.\n",
                pConsole->Players[ 0 ].wstrGamertag,
                pEvent->hrNewStatusInfo );

            // Is this the host?
            BOOL bHost = FALSE;
            if( m_AppState.top() == APPSTATE_JOINED && pConsole == &*++m_Consoles.begin() )
            {
                PopState();
                wcscpy_s( m_wstrError, L"The host has left the session." );
                PushState( APPSTATE_ERROR );

                bHost = TRUE;
            }

            // Give back the event before freeing the handle
            XrnmReturnEvent( ( XRNM_EVENT* )pEvent );
            *ppEvent = NULL;

            // Close the handle
            XrnmCloseHandle( pConsole->hLink );
            pConsole->hLink = NULL;

            if( m_hSession != INVALID_HANDLE_VALUE  )
            {
                // Remove the offending party from the session
                RemoveRemoteUsersFromSession( pConsole );

                if(bHost)
                {
                    // Delete the session
                    CloseSession();
                }
            }

            // Find the departed player in the list
            std::list <SConsole>::iterator i;

            for( i = m_Consoles.begin(); i != m_Consoles.end(); i++ )
            {
                if( &*i == pConsole )
                {
                    break;
                }
            }

            if( i != m_Consoles.end() )
            {
                if( m_AppState.top() == APPSTATE_HOSTING )
                {
                    // Notify everybody that the leaver has left
                    CClientInfoMsg msg;
                    msg.cConsoles = 1;
                    msg.rgConsoles[ 0 ].xnaddr = i->xnaddr;
                    msg.rgConsoles[ 0 ].cPlayers = 0;

                    hr = SendMessageToAll(
                        XRNM_DEFAULT_GAMEDATA_SEND_CHANNEL_ID,
                        &msg,
                        XRNM_FLAG_SEND_RELIABLE,
                        &i->xnaddr );
                }

                // delete the player from the list
                m_Consoles.erase( i );
            }

            break;
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: ProcessDataReceivedEvent
// Desc: Handle incoming data
//--------------------------------------------------------------------------------------
HRESULT Sample::ProcessDataReceivedEvent( const XRNM_EVENT_DATA_RECEIVED* pEvent )
{
    HRESULT hr = S_OK;

    CMessage* pMessage = NULL;

    hr = CMessage::CreateMessage( pEvent->pbyData, pEvent->dwDataSize, &pMessage );

    if( SUCCEEDED( hr ) &&
        m_AppState.top() == APPSTATE_HOSTING &&
        pMessage->GetDestination() == MSG_DESTINATION_ALL )
    {
        // We're the host, someone's sent us a message for everybody. Relay it.

        hr = SendMessageToAll(
            XRNM_DEFAULT_GAMEDATA_SEND_CHANNEL_ID,
            pMessage,
            pMessage->GetFlags(),
            &( ( ( SConsole* )pEvent->ulpLinkUserData )->xnaddr ) );
    }

    // Handle the message
    switch( pMessage->GetMessageID() )
    {
        case MSG_BUTTONPRESSED:
        {
            CButtonPressedMsg* pButtonPressedMessage = ( CButtonPressedMsg* )pMessage;
            // Find the sender
            std::list <SConsole>::iterator i;
            UCHAR n = 0;

            for( i = m_Consoles.begin(); i != m_Consoles.end(); i++ )
            {
                for( n = 0; n < i->cPlayers; n++ )
                {
                    if( i->Players[ n ].nPlayerID == pButtonPressedMessage->nPlayerID )
                    {
                        break;
                    }
                }

                if( n < i->cPlayers )
                {
                    break;
                }
            }

            if( i != m_Consoles.end() )
            {
                i->Players[ n ].nButton = pButtonPressedMessage->nButton;
                i->Players[ n ].dwButtonTime = GetTickCount();
            }
        }
            break;

        case MSG_POSITIONUPDATE:
        {
            CPositionUpdateMsg* pPositionUpdateMessage = ( CPositionUpdateMsg* )pMessage;

            // find the sender
            std::list <SConsole>::iterator i;

            for( i = m_Consoles.begin(); i != m_Consoles.end(); i++ )
            {
                if( !memcmp( &i->xnaddr, &pPositionUpdateMessage->xnaddr, sizeof( XNADDR ) ) )
                {
                    break;
                }
            }

            if( i != m_Consoles.end() )
            {
                for( UINT nUpdate = 0; nUpdate < pPositionUpdateMessage->NumPlayers; nUpdate++ )
                {
                    for( UINT nPlayer = 0; nPlayer < i->cPlayers; nPlayer++ )
                    {
                        if( i->Players[ nPlayer ].nPlayerID == pPositionUpdateMessage->Updates[ nUpdate ].nPlayerID )
                        {
                            i->Players[ nPlayer ].fXPos = pPositionUpdateMessage->Updates[ nUpdate ].fXPos;
                            i->Players[ nPlayer ].fYPos = pPositionUpdateMessage->Updates[ nUpdate ].fYPos;
                            i->Players[ nPlayer ].fXStick = pPositionUpdateMessage->Updates[ nUpdate ].fXStick;
                            i->Players[ nPlayer ].fYStick = pPositionUpdateMessage->Updates[ nUpdate ].fYStick;
                            i->Players[ nPlayer ].fXVelocity = pPositionUpdateMessage->Updates[ nUpdate ].fXVelocity;
                            i->Players[ nPlayer ].fYVelocity = pPositionUpdateMessage->Updates[ nUpdate ].fYVelocity;
                        }
                    }
                }
            }
        }
            break;

        case MSG_CLIENTINFO:
        {
            CClientInfoMsg* pClientInfoMsg = ( CClientInfoMsg* )pMessage;

            // Process information about a new, updated, or missing client
            for( UINT iConsole = 0; iConsole < pClientInfoMsg->cConsoles; iConsole++ )
            {
                // Look for this console in the list
                std::list <SConsole>::iterator i;

                for( i = m_Consoles.begin(); i != m_Consoles.end(); i++ )
                {
                    if( !memcmp( &i->xnaddr, &pClientInfoMsg->rgConsoles[ iConsole ].xnaddr, sizeof( XNADDR ) ) )
                    {
                        break;
                    }
                }

                BOOL bNewPlayer = FALSE;

                if( i == m_Consoles.end() )
                {
                    // It's a new console
                    SConsole console = { 0 };
                    m_Consoles.push_back( console );
                    i = --m_Consoles.end();
                    bNewPlayer = TRUE;
                }

                if( pClientInfoMsg->rgConsoles[ iConsole ].cPlayers )
                {
                    // It's an update or a newbie, store the data
                    CopyMsgToConsole( &*i, pClientInfoMsg, iConsole );

                    if( bNewPlayer )
                    {
                        AddRemoteUsersToSession( &*i );
                    }
                }
                else
                {
                    // It's a leaver
                    RemoveRemoteUsersFromSession( &*i );

                    m_Consoles.erase( i );
                }

            }
        }
            break;
    }

    if( pMessage )
    {
        delete pMessage;
    }

    return hr;
}



//--------------------------------------------------------------------------------------
// Name: ProcessReceiptEvent
// Desc: Receive and display an incoming return receipt
//--------------------------------------------------------------------------------------
HRESULT Sample::ProcessReceiptEvent( const XRNM_EVENT_RECEIPT* pEvent )
{
    m_Console.Printf( L"Received a %s receipt from %s",
                      pEvent->ReceiptType == XRNM_RECEIPT_TYPE_PROCESS  ? L"PROCESS" :
                      pEvent->ReceiptType == XRNM_RECEIPT_TYPE_RECEIVE  ? L"RECEIVE" :
                      pEvent->ReceiptType == XRNM_RECEIPT_TYPE_TRANSMIT ? L"TRANSMIT" : L"UNKNOWN",
                      ( ( SConsole* )pEvent->ulpLinkUserData )->Players[ 0 ].wstrGamertag );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ProcessAlertEvent
// Desc: Receive and display an incoming return receipt
//--------------------------------------------------------------------------------------
HRESULT Sample::ProcessAlertEvent( const XRNM_EVENT_ALERT* pEvent )
{
    // Retrieve the console object associated with the link
    SConsole* pConsole = ( SConsole* )pEvent->ulpLinkUserData;

    // Print information on the alert
    switch( pEvent->AlertType )
    {
        case XRNM_ALERT_TYPE_SEND_CHANNEL_EXCEED_NUM_QUEUED_SENDS:
            m_Console.Printf( L"ALERT: Player %s send channel 0x%08x exceeded %I64u queued sends!\n",
                              pConsole->Players[ 0 ].wstrGamertag, pEvent->idChannel, pEvent->qwValue );
            break;

        case XRNM_ALERT_TYPE_SEND_CHANNEL_EXCEED_NUM_QUEUED_BYTES:
            m_Console.Printf( L"ALERT: Player %s send channel 0x%08x exceeded %I64u queued bytes!\n",
                              pConsole->Players[ 0 ].wstrGamertag, pEvent->idChannel, pEvent->qwValue );
            break;

        case XRNM_ALERT_TYPE_LINK_EXCEED_AVERAGE_RTT:
            m_Console.Printf( L"ALERT: Player %s exceeded average round trip time %I64u!\n",
                              pConsole->Players[ 0 ].wstrGamertag, pEvent->idChannel, pEvent->qwValue );
            break;

        case XRNM_ALERT_TYPE_LINK_EXCEED_SEND_BANDWIDTH_USED:
            m_Console.Printf( L"ALERT: Player %s 0x%08x exceeded send bandwidth %I64u/bps!\n",
                              pConsole->Players[ 0 ].wstrGamertag, pEvent->idChannel, pEvent->qwValue );
            break;

        default:
            m_Console.Printf( L"ALERT: Player %s, alert type %i, value %I64u!\n",
                              pConsole->Players[ 0 ].wstrGamertag, pEvent->AlertType, pEvent->qwValue );
            break;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SendMessageToAll
// Desc: Relay a message to every client
//--------------------------------------------------------------------------------------
HRESULT Sample::SendMessageToAll( XRNM_CHANNEL_ID id, _Inout_ CMessage* pMsg, DWORD dwFlags, const XNADDR* xnaExcept )
{
    HRESULT hr = S_OK;

    if( m_AppState.top() == APPSTATE_HOSTING )
    {
        // we get to send this directly
        for( std::list <SConsole>::iterator i = ++m_Consoles.begin(); i != m_Consoles.end(); i++ )
        {
            if( SUCCEEDED( hr ) && !xnaExcept || memcmp( &i->xnaddr, xnaExcept, sizeof( XNADDR ) ) )
            {
                hr = SendMessage( id, i->hLink, pMsg, dwFlags );
            }
        }
    }
    else
    {
        // the host is always #2 in the list
        hr = SendMessage( id, ( ++m_Consoles.begin() )->hLink, pMsg, dwFlags );
    }

    if( FAILED( hr ) )
    {
        swprintf_s( m_wstrError, L"Failed to send a message, error 0x%08x", hr );
        PushState( APPSTATE_ERROR );
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: SendMessage
// Desc: Send a message to a single client
//--------------------------------------------------------------------------------------
HRESULT Sample::SendMessage( XRNM_CHANNEL_ID id, XRNM_HANDLE hLink, CMessage* pMsg, DWORD dwFlags )
{
    HRESULT hr = S_OK;

    BYTE* pbData = NULL;
    UINT cbData = 0;

    pMsg->SetFlags( dwFlags );

    // If the link is shutting down, silently fail
    XRNM_STATUS status;

    hr = XrnmGetHandleStatus( hLink, &status );
    if( SUCCEEDED( hr ) && status == XRNM_STATUS_TERMINATING )
    {
        return S_FALSE;
    }

    // Calculate the size of the required buffer
    if( SUCCEEDED( hr ) )
    {
        hr = ByteStream::CalculateSize( pMsg, &cbData );
    }

    if( SUCCEEDED( hr ) )
    {
        pbData = ( BYTE* )_malloca( cbData );

        ByteStream stream( TRUE, pbData, cbData );
        stream.Stream( pMsg );

        hr = stream.GetHRESULT();
    }

    if( SUCCEEDED( hr ) )
    {
        XRNM_SEND_BUFFER buf;
        buf.dwDataSize = cbData;
        buf.pbyData = ( BYTE* )pbData;

        hr = XrnmSend( hLink, id, &buf, 1, NULL, NULL, dwFlags );
    }


    _freea( pbData );

    return hr;
}


