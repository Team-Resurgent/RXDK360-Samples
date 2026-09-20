//--------------------------------------------------------------------------------------
// SimpleSocket.cpp
//
// Sample to demonstrate the simplest way to communicate from one
// console to another. 
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xbdm.h>
#include <malloc.h>
#include <stdio.h>
#include <winsockx.h>

#include "AtgConsole.h"
#include "AtgInput.h"
#include "AtgUtil.h"

#pragma warning(disable:4127)   // we use some infinite loops, disable "conditional expression constant" warning

//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
const UINT      NONCE_SIZE = 8;    // 8-byte nonce for recognition of 
// broadcast return
const UINT      MAX_DATA_SIZE = 64;   // maximum length of string data to send
const UINT      MIN_DATA_SIZE = 8;    // minimum length of string data to send
const DWORD     MSG_FREQ_IN_MS = 1000; // time (in ms) between messages
const UINT      MAX_SEEK_RETRY = 5;    // number of times to seek before giving 
// up and hosting

const USHORT    PORT_NUM = 1000;   // For maximum efficiency, all Xbox network 
// traffic should be on port 1000


//--------------------------------------------------------------------------------------
// Message IDs
//--------------------------------------------------------------------------------------
enum MessageID
{
    MessageID_Seeking,    // broadcast: looking for a host
    MessageID_Found,      // broadcast: response to a SEEKING message
    MessageID_Data        // data message, demonstrating communication
};

struct SMessage
{
    MessageID m_id;
};

struct SSeekingMessage : public SMessage
{
    BYTE m_Nonce[NONCE_SIZE];  // the nonce is used so we recognize a reply
};

struct SFoundMessage : public SMessage
{
    BYTE m_Nonce[NONCE_SIZE];   // nonce of the sender of the seeking message
    XNADDR m_xnaddr;              // XNADDR of the host
    XNKID m_xnkid;               // Key ID to use for secure communication
    XNKEY m_xnkey;               // Key to use for secure communication
};

struct SDataMessage : public SMessage
{
    UINT m_uiSequenceNumber;           // integer data
    SIZE_T m_size;                       // message size (for verification)
    CHAR            m_strData[MAX_DATA_SIZE + 1]; // string data (variable length)

    inline SIZE_T   GetSize()
    {
        return
            sizeof( m_id ) +
            sizeof( m_uiSequenceNumber ) +
            sizeof( m_size ) +
            strlen( m_strData ) + 1;
    }
};

union UMessage
{
    SMessage m_Message;
    SSeekingMessage m_Seeking;
    SFoundMessage m_Found;
    SDataMessage m_Data;
};


//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
SOCKET          g_socket;                      // socket we use for transmitting/receiving
XNADDR          g_xnaddr;                      // our own XNADDR
XNKID           g_xnkid;                       // key ID for session
XNKEY           g_xnkey;                       // key for session
DWORD           g_dwLastSend;                  // tick count of last send
IN_ADDR         g_sinPeer;                     // address of our peer
ATG::Console    g_console;                   // console for output
UMessage        g_message;                   // received message


//--------------------------------------------------------------------------------------
// Enumerated types
//--------------------------------------------------------------------------------------
enum SENDMESSAGE_TYPE
{
    SM_PEER         = FALSE,
    SM_BROADCAST    = TRUE
};


//--------------------------------------------------------------------------------------
// Declarations
//--------------------------------------------------------------------------------------
VOID Initialize( VOID );
BOOL SeekForPeer( VOID );
VOID Host( VOID );
VOID Shutdown( VOID );
VOID ProcessDataMessage( SDataMessage* pMessage, SIZE_T cbMessage );

HRESULT SendMessage( SENDMESSAGE_TYPE bBroadcast, const SMessage* pMessage, SIZE_T cbMessage );
UMessage*       ReceiveMessage( SOCKADDR_IN* psaIn, SIZE_T* pcbRecv );

//--------------------------------------------------------------------------------------
// Main
//
// Main game loop; drive the state machine
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    // Initialize variables
    g_socket = INVALID_SOCKET;
    g_sinPeer.s_addr = INADDR_NONE;

    // Initialize the application
    Initialize();

    g_console.Format( L"\nPress LT + RT + RB to exit.\n" );

    // Look for another peer
    BOOL bFoundPeer = SeekForPeer();

    // If we didn't find another peer, host and wait for a connection
    if( !bFoundPeer )
    {
        Host();
    }

    // By the time we reach here, we have a peer we're connected to.
    // Reset the send timer and start sending and receiving messages
    UINT cMessages = 0;
    g_dwLastSend = GetTickCount();

    HRESULT hr = S_OK;

    do
    {
        // Detect reboot keypress
        ATG::Input::GetMergedInput();

        // See if we've received a message from the peer
        SOCKADDR_IN sa;
        SIZE_T cbMessage;

        UMessage* pMessage = ReceiveMessage( &sa, &cbMessage );

        if( pMessage != NULL && pMessage->m_Message.m_id == MessageID_Data )
        {
            // make sure the message came from who we're expecting
            if( sa.sin_addr.s_addr != g_sinPeer.s_addr )
            {
                // This is not a fatal error; it's possible two peers simultaneously 
                // received a FoundMessage from us and we're only taking the one who 
                // came first. If this were a real title, we'd have a more robust 
                // accept/reject connection strategy. As it is, we'll just ignore the 
                // message and let the poor peer send to us but never hear a response.
            }
            else
            {
                ProcessDataMessage( &pMessage->m_Data, cbMessage );
            }
        }

        // See if we need to send a message to the peer
        if( GetTickCount() - g_dwLastSend > MSG_FREQ_IN_MS )
        {
            // generate a message to send
            SDataMessage msg;
            msg.m_id = MessageID_Data;
            msg.m_uiSequenceNumber = ++cMessages;

            // generate a random length
            SIZE_T cbData = rand() % ( MAX_DATA_SIZE - MIN_DATA_SIZE + 1 ) + MIN_DATA_SIZE;

            SIZE_T i;
            for( i = 0; i < cbData; i++ )
            {
                static LPCSTR strAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
                msg.m_strData[i] = strAlphabet[rand() % strlen( strAlphabet )];
            }
            msg.m_strData[i] = '\0';

            msg.m_size = msg.GetSize();

            g_console.Format( "Sending #%d, data=%s\n", msg.m_uiSequenceNumber, msg.m_strData );

            hr = SendMessage( SM_PEER, &msg, msg.GetSize() );
        }
    } while( SUCCEEDED( hr ) );

    g_console.Format( "SendMessage() failed, disconnected from peer with error %d.\n",
                      HRESULT_CODE( hr ) );

    // Release our resources
    Shutdown();

    // Reboot when done
    XLaunchNewImage( "", 0 );
}


//--------------------------------------------------------------------------------------
// ReceiveMessage
//
// Attempt to receive a message from the network
//--------------------------------------------------------------------------------------
UMessage* ReceiveMessage( SOCKADDR_IN* psaIn, SIZE_T* pcbRecv )
{
    INT cbRecv, cbIn, err;

    // Make sure we have a valid socket
    if( g_socket == INVALID_SOCKET )
    {
        ATG::FatalError( "Attempted to receive on an invalid socket.\n" );
    }

    // Try to retrieve a message from the socket
    cbIn = sizeof( SOCKADDR_IN );
    cbRecv = recvfrom(
        g_socket, ( PSTR )&g_message, sizeof( UMessage ), 0, ( sockaddr* )psaIn, &cbIn );

    // Check for errors
    if( cbRecv == SOCKET_ERROR )
    {
        err = WSAGetLastError();

        // WSAEWOULDBLOCK isn't an error, it just means there's no data waiting
        if( err != WSAEWOULDBLOCK )
        {
            ATG::FatalError( "Failed to recvfrom() socket, error %d.\n", err );
        }

        return NULL;
    }

    if( pcbRecv )
    {
        *pcbRecv = cbRecv;
    }

    return ( UMessage* )&g_message;
}


//--------------------------------------------------------------------------------------
// SendMessage
//
// Send a message to the network
//--------------------------------------------------------------------------------------
HRESULT SendMessage( SENDMESSAGE_TYPE bBroadcast, const SMessage* pMessage, SIZE_T cbMessage )
{
    HRESULT hr = S_OK;

    // Make sure we have a valid socket
    if( g_socket == INVALID_SOCKET )
    {
        ATG::FatalError( "Attempted to send to an invalid socket.\n" );
    }

    // If we're not broadcasting, make sure we have a peer to send to
    if( !bBroadcast && g_sinPeer.s_addr == INADDR_NONE )
    {
        ATG::FatalError( "Attempted to send a non-broadcast when we have no peer.\n" );
    }

    SOCKADDR_IN sa;

    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = bBroadcast ? INADDR_BROADCAST : g_sinPeer.s_addr;
    sa.sin_port = htons( PORT_NUM );

    INT cbSent = sendto(
        g_socket,
        ( PCSTR )pMessage,
        cbMessage,
        0,
        ( const sockaddr* )&sa,
        sizeof( sa ) );

    if( cbSent != ( INT )cbMessage )
    {
        if( cbSent == SOCKET_ERROR )
        {
            // Return the error to the caller so it can decide
            // what to do with it. The most likely cause of this error
            // is a disruption of the security association between two
            // boxes, which will happen if one box disconnects. This is
            // distinct from the Win32 behavior of sendto, where sends to
            // nonexistent peers will silently succeed.
            int err = WSAGetLastError();
            hr = HRESULT_FROM_WIN32( err );
        }
        else
        {
            ATG::FatalError( "Datagram socket sent less than a single datagram, "
                             "tried to send %d but only sent %d.\n", cbMessage, cbSent );
        }
    }

    g_dwLastSend = GetTickCount();

    return hr;
}


//--------------------------------------------------------------------------------------
// Initialize
//
// Set up SNL and socket
//--------------------------------------------------------------------------------------
VOID Initialize( VOID )
{
    // Initialize the console
    HRESULT hr = g_console.Create( "game:\\Media\\Fonts\\Arial_12.xpr", 0xff0000ff, 0xffffffff );
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Console initialization failed.\n" );
    }

    g_console.Format( "*** INITIALIZING ***\n" );

    // Start up the SNL with default initialization parameters
    if( XNetStartup( NULL ) != 0 )
    {
        ATG::FatalError( "XNetStartup failed.\n" );
    }

    // Start up Winsock
    WORD wVersion = MAKEWORD( 2, 2 );   // request version 2.2 of Winsock
    WSADATA wsaData;

    INT err = WSAStartup( wVersion, &wsaData );
    if( err != 0 )
    {
        ATG::FatalError( "WSAStartup failed, error %d.\n", err );
    }

    // Verify that we got the right version of Winsock
    if( wsaData.wVersion != wVersion )
    {
        ATG::FatalError( "Failed to get proper version of Winsock, got %d.%d.\n",
                         LOBYTE( wsaData.wVersion ), HIBYTE( wsaData.wVersion ) );
    }

    // Initialize the socket
    g_socket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if( g_socket == INVALID_SOCKET )
    {
        ATG::FatalError( "Failed to create socket, error %d.\n", WSAGetLastError() );
    }

    // Bind the socket
    SOCKADDR_IN sa;
    sa.sin_family = AF_INET;           // IP family
    sa.sin_addr.s_addr = INADDR_ANY;        // Use the only IP that's available to us
    sa.sin_port = htons( PORT_NUM ); // Port (should be 1000)

    if( bind( g_socket, ( const sockaddr* )&sa, sizeof( sa ) ) != 0 )
    {
        ATG::FatalError( "Failed to bind socket, error %d.\n", WSAGetLastError() );
    }

    // Set the socket to nonblocking
    unsigned long iUnblocking = 1;
    if( ioctlsocket( g_socket, FIONBIO, &iUnblocking ) != 0 )
    {
        ATG::FatalError( "Failed to set socket to nonblocking, error %d\n", WSAGetLastError() );
    }

    // Permit the socket to send broadcasts
    BOOL bBroadcast = TRUE;
    if( setsockopt(
        g_socket, SOL_SOCKET, SO_BROADCAST, ( PCSTR )&bBroadcast, sizeof( BOOL ) ) != 0 )
    {
        ATG::FatalError( "Failed to set socket to broadcast-capable, error %d\n",
                         WSAGetLastError() );
    }

    // Get our own XNADDR
    DWORD dwRet;
    do
    {
        dwRet = XNetGetTitleXnAddr( &g_xnaddr );
    } while( dwRet == XNET_GET_XNADDR_PENDING );

    if( dwRet & XNET_GET_XNADDR_NONE )
    {
        ATG::FatalError( "Unable to find an XNADDR.\n" );
    }

    g_console.Format( "XNADDR: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      g_xnaddr.abEnet[0], g_xnaddr.abEnet[1], g_xnaddr.abEnet[2],
                      g_xnaddr.abEnet[3], g_xnaddr.abEnet[4], g_xnaddr.abEnet[5] );

    // Seed the random number generator while we're here
    DWORD dwSeed;
    XNetRandom( ( BYTE* )&dwSeed, sizeof( DWORD ) );
    srand( dwSeed );

    return;
}


//--------------------------------------------------------------------------------------
// Shutdown
//
// Tear everything down
//--------------------------------------------------------------------------------------
VOID Shutdown( VOID )
{
    g_console.Format( "*** SHUTTING DOWN ***\n" );

    // Close our socket, if any
    if( g_socket != INVALID_SOCKET )
    {
        closesocket( g_socket );
        g_socket = INVALID_SOCKET;
    }

    // Terminate Winsock
    WSACleanup();

    // Terminate the SNL
    XNetCleanup();
    return;
}


//--------------------------------------------------------------------------------------
// SeekForPeer
//
// Look for a peer on the network to connect to. Returns TRUE if a peer was found,
// FALSE if not.
//--------------------------------------------------------------------------------------
BOOL SeekForPeer( VOID )
{
    BYTE Nonce[NONCE_SIZE];           // our nonce

    // Generate a nonce
    if( XNetRandom( Nonce, NONCE_SIZE ) != 0 )
    {
        ATG::FatalError( "Failed to generate a nonce.\n" );
    }

    // Reset the send timer
    g_dwLastSend = GetTickCount();

    UINT cSeeks = 0;

    while( TRUE )
    {
        // Detect reboot keypress
        ATG::Input::GetMergedInput();

        // see if we need to send another seek
        if( GetTickCount() - g_dwLastSend > MSG_FREQ_IN_MS )
        {
            if( ++cSeeks > MAX_SEEK_RETRY )
            {
                // haven't found anyone, start hosting
                g_console.Format( "No remote peer found; switching to host mode.\n" );

                return FALSE;
            }

            g_console.Format( "Sending a seek...\n" );
            SSeekingMessage msg;

            // Set the message ID and the nonce
            msg.m_id = MessageID_Seeking;
            memcpy( msg.m_Nonce, Nonce, NONCE_SIZE );

            if( FAILED( SendMessage( SM_BROADCAST, &msg, sizeof( msg ) ) ) )
            {
                ATG::FatalError( "Failed to send a seeking broadcast.\n" );
            }
        }

        // see if we've received a reply
        SOCKADDR_IN sa;
        SIZE_T cbMessage;

        UMessage* pMessage = ReceiveMessage( &sa, &cbMessage );

        if( !pMessage )
        {
            // we haven't received anything yet
            continue;
        }

        // We've received something.
        // If it's not a found message, ignore it.
        if( pMessage->m_Message.m_id != MessageID_Found )
        {
            continue;
        }

        // Make sure the message isn't corrupt
        if( cbMessage != sizeof( SFoundMessage ) )
        {
            ATG::FatalError(
                "Received a found message of the wrong length.\n" );
        }

        // See if the message is meant for us
        if( memcmp( pMessage->m_Found.m_Nonce, Nonce, NONCE_SIZE ) )
        {
            continue;
        }

        // There's another peer out there. Register the key and connect
        g_console.Format(
            "Found another peer at %02X:%02X:%02X:%02X:%02X:%02X.\n",
            pMessage->m_Found.m_xnaddr.abEnet[0],
            pMessage->m_Found.m_xnaddr.abEnet[1],
            pMessage->m_Found.m_xnaddr.abEnet[2],
            pMessage->m_Found.m_xnaddr.abEnet[3],
            pMessage->m_Found.m_xnaddr.abEnet[4],
            pMessage->m_Found.m_xnaddr.abEnet[5] );

        g_xnkid = pMessage->m_Found.m_xnkid;
        g_xnkey = pMessage->m_Found.m_xnkey;

        XNetRegisterKey( &g_xnkid, &g_xnkey );

        // Try to get the peer's address
        if( XNetXnAddrToInAddr( &pMessage->m_Found.m_xnaddr, &g_xnkid, &g_sinPeer ) != 0 )
        {
            ATG::FatalError( "Failed to retrieve the peer's IP address from XNADDR.\n" );
        }

        // We've successfully found and connected to a remote peer.
        // Return TRUE to let the application know that.
        return TRUE;
    } // while( TRUE )
}


//--------------------------------------------------------------------------------------
// Host
//
// Wait for someone to connect to us and start communicating
//--------------------------------------------------------------------------------------
VOID Host( VOID )
{
    // Generate and register a key
    if( XNetCreateKey( &g_xnkid, &g_xnkey ) != 0 )
    {
        ATG::FatalError( "Failed to create a key.\n" );
    }

    if( XNetRegisterKey( &g_xnkid, &g_xnkey ) != 0 )
    {
        ATG::FatalError( "Failed to register the key.\n" );
    }

    while( TRUE )
    {
        // Detect reboot keypress
        ATG::Input::GetMergedInput();

        // See if anyone's sent us anything
        SOCKADDR_IN sa;
        SIZE_T cbMessage;

        UMessage* pMessage = ReceiveMessage( &sa, &cbMessage );

        if( !pMessage )
        {
            continue;  // nothing yet
        }

        // is this someone seeking a host?
        if( pMessage->m_Message.m_id == MessageID_Seeking )
        {
            if( cbMessage != sizeof( SSeekingMessage ) )
            {
                ATG::FatalError( "Received a seeking message of the wrong length.\n" );
            }

            // let him know we're here
            SSeekingMessage* pSeekingMessage = ( SSeekingMessage* )pMessage;

            SFoundMessage msg;
            msg.m_id = MessageID_Found;
            msg.m_xnaddr = g_xnaddr;
            msg.m_xnkid = g_xnkid;
            msg.m_xnkey = g_xnkey;

            memcpy( msg.m_Nonce, pSeekingMessage->m_Nonce, NONCE_SIZE );

            g_console.Format( "Received a seek; sending reply.\n" );

            if( FAILED( SendMessage( SM_BROADCAST, &msg, sizeof( msg ) ) ) )
            {
                ATG::FatalError( "Failed to send a found broadcast.\n" );
            }
        }
            // is this someone sending us data, establishing a connection?
        else if( pMessage->m_Message.m_id == MessageID_Data )
        {
            // We have a peer. Save off his address and start communicating
            g_console.Format( "Received a message! Connecting...\n" );
            g_sinPeer.s_addr = sa.sin_addr.s_addr;

            // Process the message we just received
            ProcessDataMessage( &pMessage->m_Data, cbMessage );

            return;
        }
    }
}


//--------------------------------------------------------------------------------------
// ProcessDataMessage
//
// When a data message is received, process it -- for now, output to debug window
//--------------------------------------------------------------------------------------
VOID ProcessDataMessage( SDataMessage* pMessage, SIZE_T cbMessage )
{
    // Validate the message length
    if( pMessage->m_size != cbMessage )
    {
        ATG::FatalError( "Received a data message with the wrong length.\n" );
    }

    // Output the message
    g_console.Format(
        "Received #%d, data=%s\n",
        pMessage->m_uiSequenceNumber,
        pMessage->m_strData );
}
