//--------------------------------------------------------------------------------------
// HttpClient.cpp
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "HttpClient.h"

// Worker thread entrance or main processing routine
DWORD WINAPI HttpSendCommand( LPVOID lpParameter );


DWORD WINAPI HttpSendCommand( LPVOID lpParameter )
{
    OutputDebugString( "HttpSendCommand\n" );

    HttpClient* pHttpClient = ( HttpClient* )lpParameter;

    if( !pHttpClient )
        return S_FALSE;

    // internal buffer
    HTTP_BUFFER& httpBuffer = pHttpClient->GetInternalBuffer();

    int nErrorCode;

    // Create TCP/IP socket
    SOCKET hSocket;
    hSocket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if( ( hSocket == SOCKET_ERROR ) || ( hSocket == INVALID_SOCKET ) )
    {
        nErrorCode = WSAGetLastError();
        pHttpClient->SetSocketErrorCode( nErrorCode );
        pHttpClient->SetStatus( HttpClient::HTTP_STATUS_ERROR );            // always put this last
        return nErrorCode;
    }

    sockaddr_in httpServerAdd;

    httpServerAdd.sin_family = AF_INET;
    httpServerAdd.sin_port = htons( httpBuffer.port );
    httpServerAdd.sin_addr.s_addr = inet_addr( httpBuffer.serverName );

    XNDNS* pxndns = NULL;

    if( httpServerAdd.sin_addr.s_addr == INADDR_NONE )        // Hostname
    {
        //dns LOOKUP
        XNetDnsLookup( httpBuffer.serverName, httpBuffer.overlapped.hEvent, &pxndns );
        WaitForSingleObject( httpBuffer.overlapped.hEvent, INFINITE );
        WSAResetEvent( httpBuffer.overlapped.hEvent );
        if( pxndns->iStatus != 0 )
        {
            //  An error occurred.  One of the following:
            //  pxndns->iStatus == WSAHOST_NOT_FOUND - No such host
            //  pxndns->iStatus == WSAETIMEDOUT - No response from DNS server( s )
            nErrorCode = pxndns->iStatus;
            XNetDnsRelease( pxndns );
            shutdown( hSocket, SD_BOTH );
            closesocket( hSocket );
            pHttpClient->SetSocketErrorCode( nErrorCode );                // DNS lookup fail 
            pHttpClient->SetStatus( HttpClient::HTTP_STATUS_ERROR );      // Always put this last
            return nErrorCode;
        }

        UINT i;
        BOOL bConnection = FALSE;
        for( i = 0; i < pxndns->cina; ++i )
        {
            httpServerAdd.sin_addr = pxndns->aina[ i ];
            if( connect( hSocket, ( struct sockaddr* )&httpServerAdd, sizeof( httpServerAdd ) ) == 0 )
            {
                bConnection = TRUE;
                break;
            }
        }

        XNetDnsRelease( pxndns );

        if( !bConnection )        // we already tried all the IPs
        {
            nErrorCode = WSAGetLastError();
            shutdown( hSocket, SD_BOTH );
            closesocket( hSocket );
            pHttpClient->SetSocketErrorCode( nErrorCode );
            pHttpClient->SetStatus( HttpClient::HTTP_STATUS_ERROR );    // Always put this last
            return nErrorCode;
        }

    }
    else    // IP
    {
        if( connect( hSocket, ( struct sockaddr* )&httpServerAdd, sizeof( httpServerAdd ) ) != 0 )
        {
            nErrorCode = WSAGetLastError();
            shutdown( hSocket, SD_BOTH );
            closesocket( hSocket );
            pHttpClient->SetSocketErrorCode( nErrorCode );
            pHttpClient->SetStatus( HttpClient::HTTP_STATUS_ERROR );    // Always put this last
            return nErrorCode;
        }
    }

    if( SOCKET_ERROR != send( hSocket, ( const char* )httpBuffer.MB_request.GetData(),
                              httpBuffer.MB_request.GetDataLength(), 0 ) )
    {
        int nSize = 0;
        char buff[TCP_RECV_BUFFER_SIZE];
        httpBuffer.MB_response.Rewind();

        while( ( nSize = recv( hSocket, buff, TCP_RECV_BUFFER_SIZE, 0 ) ) != 0 )
        {
            httpBuffer.MB_response.Add( buff, nSize );
        }

        shutdown( hSocket, SD_BOTH );
        closesocket( hSocket );

        pHttpClient->SetSocketErrorCode( ERROR_SUCCESS );                    // socket OK
        return pHttpClient->ParseIncomingHttpResponse();
    }
    else
    {
        shutdown( hSocket, SD_BOTH );
        closesocket( hSocket );
        pHttpClient->SetSocketErrorCode( WSAGetLastError() );
        return S_FALSE;
    }
}



HttpClient::HttpClient( BOOL bThread, DWORD dwPort )
{
    m_buffer.port = dwPort;
    m_bUsingThread = bThread;

    // Create an event handle and setup an overlapped structure.
    m_buffer.overlapped.hEvent = WSACreateEvent();
    if( m_buffer.overlapped.hEvent == NULL )
        m_status = HTTP_STATUS_ERROR;
    else
        m_status = HTTP_STATUS_READY;

}


HttpClient::~HttpClient()
{
    WSACloseEvent( m_buffer.overlapped.hEvent );
}


HRESULT HttpClient::SendCommand()
{

    if( m_bUsingThread )
    {
        // start worker thread
        HANDLE hThread = CreateThread( NULL, 0, HttpSendCommand, ( VOID* )this, 0, NULL );
        if( hThread )
        {
            CloseHandle( hThread );
            return E_PENDING;
        }
        else
        {
            return E_FAIL;;
        }
    }
    else
    {
        // Or process it directly, synchronized call. Don't do it in your main thread
        return HttpSendCommand( this );
    }


}


HRESULT HttpClient::ParseIncomingHttpResponse()
{

    UINT nDataLength = m_buffer.MB_response.GetDataLength();

    if( nDataLength <= 4 )
    {
        SetStatus( HttpClient::HTTP_STATUS_ERROR );
        return E_FAIL;
    }

    UCHAR* pData = m_buffer.MB_response.GetData();

    // HTTP/1.1 expected
    if( memcmp( "HTTP", pData, 4 ) != 0 )
    {
        SetStatus( HttpClient::HTTP_STATUS_ERROR );
        return E_FAIL;
    }

    // find the response status 
    while( !isspace( *pData ) && nDataLength )
    {
        --nDataLength;
        ++pData;
    }

    if( nDataLength == 0 )
    {
        SetStatus( HttpClient::HTTP_STATUS_ERROR );
        return E_FAIL;
    }

    m_dwResponseCode = atoi( ( char* )pData );

    // OK 200 expected, for simplicity we only check this
    if( m_dwResponseCode != 200 )
    {
        SetStatus( HttpClient::HTTP_STATUS_ERROR );
        return E_FAIL;
    }

    // find the content data
    BOOL bLineHead = FALSE;
    while( nDataLength )
    {
        if( *pData == '\n' )
        {
            if( bLineHead )    // found 2nd LF ?
            {
                --nDataLength;
                ++pData;
                break;
            }
            else
            {
                bLineHead = TRUE;
            }

        }
        else if( *pData != '\r' )
        {
            bLineHead = FALSE;
        }

        --nDataLength;
        ++pData;
    }

    if( nDataLength == 0 )
    {
        SetResponseContentDataLength( 0 );
        SetResponseContentData( NULL );
        SetStatus( HttpClient::HTTP_STATUS_ERROR );
        return E_FAIL;
    }
    else
    {
        SetResponseContentDataLength( nDataLength );
        SetResponseContentData( pData );
        SetStatus( HttpClient::HTTP_STATUS_DONE );
        return ERROR_SUCCESS;
    }

}
//--------------------------------------------------------------------------------------
// strFileName can't contain unsafe characters, for example, SPACE.
// Otherwise, URL encode function should be added.
// Check RFC 1738 for detail.
//--------------------------------------------------------------------------------------
HRESULT HttpClient::GET( const CHAR* strServerName, const CHAR* strFileName )
{

    char cmdBuff[ HTTP_COMMAND_BUFFER_SIZE ];

    if( GetStatus() == HTTP_STATUS_BUSY )
        return E_FAIL;

    SetStatus( HttpClient::HTTP_STATUS_BUSY );

    sprintf_s( cmdBuff, "GET %s HTTP/1.0 \r\n\r\n", strFileName );
    m_buffer.MB_request.Rewind();
    m_buffer.MB_request.Add( cmdBuff, strlen( cmdBuff ) );
    strcpy_s( m_buffer.serverName, strServerName );
    return SendCommand();
}

//--------------------------------------------------------------------------------------
// strFileName can't contain unsafe characters, for example, SPACE.
// Otherwise, URL encode function should be added.
// Check RFC 1738 for detail.
//--------------------------------------------------------------------------------------
HRESULT HttpClient::POST( const CHAR* strServerName, const CHAR* strFileName, const CHAR* pCmd, DWORD dwCmdLength )
{

    char cmdBuff[ HTTP_COMMAND_BUFFER_SIZE ];

    if( GetStatus() == HTTP_STATUS_BUSY )
        return E_FAIL;

    SetStatus( HttpClient::HTTP_STATUS_BUSY );

    sprintf_s( cmdBuff,
               "POST %s HTTP/1.0 \r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\n\r\n", strFileName,
               dwCmdLength );
    m_buffer.MB_request.Rewind();
    m_buffer.MB_request.Add( cmdBuff, strlen( cmdBuff ) );
    m_buffer.MB_request.Add( pCmd, dwCmdLength );
    strcpy_s( m_buffer.serverName, strServerName );
    return SendCommand();

}

