//--------------------------------------------------------------------------------------
// SystemLinkHelper.cpp
//
// Helper class for handling finding sessions using system link
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "SystemLinkHelper.h"


//--------------------------------------------------------------------------------------
// Name: BeginSearching()
// Desc: Start looking for sessions on the LAN
//--------------------------------------------------------------------------------------
HRESULT SystemLinkHelper::BeginSearching( VOID )
{
    HRESULT hr = S_OK;

    if( m_Mode != IDLE )
    {
        hr = E_FAIL;
    }

    // Generate our nonce
    hr = SUCCEEDED( hr ) ? XNetRandom( m_Nonce, sizeof( m_Nonce ) ) : hr;

    // Open and bind the socket
    hr = SUCCEEDED( hr ) ? CreateSocket() : hr;

    // Send our first seek
    hr = SUCCEEDED( hr ) ? SendSeek() : hr;

    if( SUCCEEDED( hr ) )
    {
        m_Mode = SEARCHING;
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: BeginHosting()
// Desc: Start looking for sessions on the LAN
//--------------------------------------------------------------------------------------
HRESULT SystemLinkHelper::BeginHosting( const XSESSION_INFO* info, UINT nPlayerCount, LPCWSTR wstrHostGamertag )
{
    HRESULT hr = S_OK;

    if( m_Mode != IDLE )
    {
        hr = E_FAIL;
    }

    // Open and bind the socket
    hr = SUCCEEDED( hr ) ? CreateSocket() : hr;

    wcscpy_s( m_SessionInfo.wstrHostGamerTag, wstrHostGamertag );
    m_SessionInfo.nPlayerCount = nPlayerCount;
    memcpy_s( &m_SessionInfo.SessionInfo, sizeof( m_SessionInfo.SessionInfo ), info, sizeof( XSESSION_INFO ) );

    if( SUCCEEDED( hr ) )
    {
        m_Mode = HOSTING;
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: UpdatePlayerCount()
// Desc: Update the number of players in the session
//--------------------------------------------------------------------------------------
HRESULT SystemLinkHelper::UpdatePlayerCount( UINT nPlayerCount )
{
    HRESULT hr = S_OK;

    if( m_Mode == HOSTING )
    {
        m_SessionInfo.nPlayerCount = nPlayerCount;
    }
    else
    {
        hr = E_FAIL;
    }

    return hr;
}



//--------------------------------------------------------------------------------------
// Name: End
// Desc: Stop messing around with system link discovery
//--------------------------------------------------------------------------------------
HRESULT SystemLinkHelper::End( VOID )
{
    HRESULT hr = S_OK;

    m_Sessions.clear();

    int err = closesocket( m_Socket );
    if( err == SOCKET_ERROR )
    {
        hr = HRESULT_FROM_WIN32( err );
    }

    m_Mode = IDLE;

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: CreateSocket
// Desc: Create a socket, bind it, and set it to nonblocking + broadcast
//--------------------------------------------------------------------------------------
HRESULT SystemLinkHelper::CreateSocket( VOID )
{
    HRESULT hr = S_OK;
    int err = 0;

    // Create the socket
    m_Socket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );

    if( m_Socket == INVALID_SOCKET )
    {
        hr = HRESULT_FROM_WIN32( WSAGetLastError() );
    }

    // Bind it
    sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl( INADDR_ANY );
    sa.sin_port = htons( PORT );

    if( SUCCEEDED( hr ) && err != SOCKET_ERROR )
    {
        err = bind( m_Socket, ( sockaddr* )&sa, sizeof( sa ) );
    }

    // Enable broadcasts
    if( SUCCEEDED( hr ) && err != SOCKET_ERROR )
    {
        char bBroadcast = TRUE;
        err = setsockopt( m_Socket, SOL_SOCKET, SO_BROADCAST, &bBroadcast, sizeof( bBroadcast ) );
    }

    // Set the socket to nonblocking
    if( SUCCEEDED( hr ) && err != SOCKET_ERROR )
    {
        ULONG bNonblocking = TRUE;
        err = ioctlsocket( m_Socket, FIONBIO, &bNonblocking );
    }

    // Set up the broadcast sockaddr
    m_saBroadcast.sin_family = AF_INET;
    m_saBroadcast.sin_addr.s_addr = htonl( INADDR_BROADCAST );
    m_saBroadcast.sin_port = htons( PORT );


    if( err == SOCKET_ERROR )
    {
        if( SUCCEEDED( hr ) )
        {
            closesocket( m_Socket );
        }

        hr = HRESULT_FROM_WIN32( WSAGetLastError() );
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: SendSeek
// Desc: Send a message looking for a host
//--------------------------------------------------------------------------------------
HRESULT SystemLinkHelper::SendSeek( VOID )
{
    HRESULT hr = S_OK;

    // save the time
    m_dwLastSeek = GetTickCount();

    // create a seek message
    SSeekingMessage msg;
    msg.bSeeking = TRUE;
    memcpy_s( msg.m_Nonce, sizeof( msg.m_Nonce ), m_Nonce, NONCE_SIZE );

    // send it to the broadcast address
    int err = sendto( m_Socket, ( char* )&msg, sizeof( msg ), 0, ( sockaddr* )&m_saBroadcast, sizeof
                      ( m_saBroadcast ) );

    if( err != sizeof( msg ) )
    {
        hr = HRESULT_FROM_WIN32( WSAGetLastError() );
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Perform per-frame housekeeping
//--------------------------------------------------------------------------------------
HRESULT SystemLinkHelper::Update( VOID )
{
    HRESULT hr = S_OK;

    // If we're doing nothing, just bail
    if( m_Mode == IDLE )
    {
        return hr;
    }

    DWORD dwTickCount = GetTickCount();

    // If we're seeking, see if we need to send 
    if( m_Mode == SEARCHING && ( dwTickCount - m_dwLastSeek ) > SEEK_INTERVAL )
    {
        hr = SendSeek();
    }

    // See if we've received anything
    sockaddr_in saFrom;
    int saFromLen = sizeof( saFrom );
    char buf[ MAX_MESSAGESIZE ];

    int err = recvfrom( m_Socket, buf, sizeof( buf ), 0, ( sockaddr* )&saFrom, &saFromLen );

    if( err != SOCKET_ERROR )
    {
        SSeekingMessage* pMsg = ( SSeekingMessage* )buf;

        if( pMsg->bSeeking && m_Mode == HOSTING )
        {
            // Somebody is looking for hosts, and we are one. Broadcast a reply
            SSeekReplyMessage msg;

            msg.bSeeking = FALSE; // this is a reply, not a seek
            memcpy_s( msg.m_Nonce, sizeof( msg.m_Nonce ), pMsg->m_Nonce, NONCE_SIZE );
            memcpy_s( &msg.SessionInfo, sizeof( msg.SessionInfo ), &m_SessionInfo, sizeof( m_SessionInfo ) );

            // Send!
            err = sendto( m_Socket, ( char* )&msg, sizeof( msg ), 0, ( sockaddr* )&m_saBroadcast, sizeof
                          ( m_saBroadcast ) );

            if( err != sizeof( msg ) )
            {
                hr = HRESULT_FROM_WIN32( WSAGetLastError() );
            }
        }
        else if( !pMsg->bSeeking && m_Mode == SEARCHING && !memcmp( m_Nonce, pMsg->m_Nonce, NONCE_SIZE ) )
        {
            // It's a reply to us! See if it's in the list. If not, add it.
            SSeekReplyMessage* pReply = ( SSeekReplyMessage* )pMsg;
            Session* pSessionInfo = NULL;

            for( std::vector <Session>::iterator i = m_Sessions.begin(); i != m_Sessions.end(); i++ )
            {
                if( !memcmp( &i->SessionInfo.sessionID, &pReply->SessionInfo.SessionInfo.sessionID, sizeof( XNKID ) ) )
                {
                    pSessionInfo = &*i;
                    break;
                }
            }

            if( !pSessionInfo )
            {
                // it's new
                m_Sessions.push_back( pReply->SessionInfo );
                pSessionInfo = &m_Sessions.back();
            }

            // Set the last update received
            pSessionInfo->dwLastUpdateReceived = dwTickCount;
            pSessionInfo->nPlayerCount = pReply->SessionInfo.nPlayerCount;
        }
    }
    else
    {
        err = WSAGetLastError();

        if( err != WSAEWOULDBLOCK )
        {
            hr = HRESULT_FROM_WIN32( err );
        }
    }

    // See if we need to remove any sessions from the list
    if( m_Mode == SEARCHING )
    {
        BOOL bErased = FALSE;

        for( std::vector <Session>::iterator i = m_Sessions.begin(); i != m_Sessions.end(); bErased ? 0 : ( VOID )++i )
        {
            bErased = ( ( dwTickCount - i->dwLastUpdateReceived ) > TIMEOUT_INTERVAL );

            if( bErased )
            {
                i = m_Sessions.erase( i );
            }
        }
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: ClearSessions()
// Desc: Remove all found sessions from the list
//--------------------------------------------------------------------------------------
VOID SystemLinkHelper::ClearSessions( VOID )
{
    m_Sessions.clear();
}
