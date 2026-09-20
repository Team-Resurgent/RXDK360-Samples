//--------------------------------------------------------------------------------------
// SimpleXLSPClient.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include "xbox.h"
#include "xonline.h"
#include "AtgConsole.h"
#include "AtgSignIn.h"
#include "AtgUtil.h"
#include "AtgInput.h"

// Send a packet every 1.5 seconds
#define PACKET_SEND_INTERVAL 1.5

// Send five total packets
#define PACKET_SEND_COUNT    5

// The maximum number of servers to retrieve from XTitleServerCreateEnumerator/XEnumerate
#define MAX_SERVERS          10

// Maximum time to wait for the connection to be established, in ms
#define MAX_CONNECT_TIME     30000

// To connect to the title server you must use the service ID you
// were assigned.  The service id in the sample should correspond
// to the one that is set in your sgconfig.ini file:
//
// Service
// {
//    Id         4294967295   ; 0xFFFFFFFF
//    Name       XLSP_SAMPLES
// }
//
// For this sample the service id is bogus, 0xFFFFFFFF.  In order,
// for the sample to work you need to set up your XLSP server(s)
// with your service ID, set the title ID in xex.xml and then set 
// this service ID:
DWORD           SERVICEID_XLSPSAMPLE = 0xFFFFFFFF;

// The Title Server port will be mapped by the XLSP security gateway to
// a specific back-end server's port and IP. That port will be sepcified in the
// sgconfig.ini file like this:
//
// Server
// {
//     Id        1000     ; The port the title uses to connect to this server
//     Service   XLSP_SAMPLES
//     Address { InterfaceId 7 Ip 10.10.0.2 Port 8001  }
// }
//
// Here the port that the Xbox console talks to is port 1000 (the Id) and
// that is forwarded to the Title Server on port 8001 on the datacenter side.
// If you use an Id other than 1000 you will need to change the following line
// to match it:
const WORD      TITLE_SERVER_PORT = 1000;

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
ATG::Console    g_Console;      // Console for output
ATG::Timer      g_Timer;        // Timer for packet transmission
SOCKET          g_Socket;       // Title Server traffic socket
IN_ADDR         g_inaSecure;    // Secure title server address

//--------------------------------------------------------------------------------------
// Name: PrintLine
// Desc: Output a line to the console and to the debugger output window.
//--------------------------------------------------------------------------------------
VOID __cdecl PrintLine( const WCHAR* strFormat, ... )
{
    va_list pArgList;
    va_start( pArgList, strFormat );

    WCHAR buffer[ 1000 ];
    wvsprintfW( buffer, strFormat, pArgList );
    wcscat_s( buffer, L"\n" );
    OutputDebugStringW( buffer );

    g_Console.Format( L"%s", buffer );

    va_end( pArgList );
}


//-----------------------------------------------------------------------------
// Name: PrepareConnection()
// Desc: Demonstrates establishing a connection to the title server
//-----------------------------------------------------------------------------
BOOL PrepareConnection()
{
    XTITLE_SERVER_INFO Servers[ MAX_SERVERS ];
    DWORD dwResult = 0;
    INT iResult = 0;
    DWORD dwServerCount = 0;
    DWORD dwBufferSize = 0;
    HANDLE hServerEnum = INVALID_HANDLE_VALUE;

    ZeroMemory( Servers, sizeof( Servers ) );
    ZeroMemory( &g_inaSecure, sizeof( g_inaSecure ) );

    ///////////////////////////////////////////////////////////////////////////
    // Get a list of available title servers using XTitleServerCreateEnumerator
    ///////////////////////////////////////////////////////////////////////////
    PrintLine( L"Searching for available title servers..." );

    dwResult = XTitleServerCreateEnumerator( NULL, MAX_SERVERS, &dwBufferSize, &hServerEnum );
    if( ERROR_SUCCESS != dwResult )
    {
        PrintLine( L"XTitleServerCreateEnumerator failed with 0x%0x.", dwResult );
        return FALSE;
    }

    dwResult = XEnumerate( hServerEnum, Servers, sizeof( Servers ), &dwServerCount, NULL );
    if( ERROR_SUCCESS != dwResult )
    {
        PrintLine( L"XEnumerate failed with 0x%0x.", dwResult );
        CloseHandle( hServerEnum );
        return FALSE;
    }

    CloseHandle( hServerEnum );

    if( dwServerCount == 0 )
    {
        PrintLine( L"No servers were returned from XTitleServerCreateEnumerator." );
        return FALSE;
    }

    // Display the results (the entire list of available servers)
    PrintLine( L"Found %d servers:", dwServerCount );
    for( DWORD dwIndex = 0; dwIndex < dwServerCount; ++dwIndex )
    {
        PrintLine( L"\"%hs\" located at %d.%d.%d.%d", Servers[ dwIndex ].szServerInfo,
                   Servers[ dwIndex ].inaServer.S_un.S_un_b.s_b1,
                   Servers[ dwIndex ].inaServer.S_un.S_un_b.s_b2,
                   Servers[ dwIndex ].inaServer.S_un.S_un_b.s_b3,
                   Servers[ dwIndex ].inaServer.S_un.S_un_b.s_b4 );
    }


    ///////////////////////////////////////////////////////////////////////////
    // Choose the first one and get it's secure address using XNetServerToInAddr
    ///////////////////////////////////////////////////////////////////////////
    PrintLine( L"Getting secure address for \"%hs\"...", Servers[ 0 ].szServerInfo );

    if( ( iResult = XNetServerToInAddr( Servers[ 0 ].inaServer, SERVICEID_XLSPSAMPLE, &g_inaSecure ) ) != 0 )
    {
        PrintLine( L"XNetServerToInAddr failed with %d.", iResult );
        return FALSE;
    }

    ///////////////////////////////////////////////////////////////////////////
    // Use XNetConnect to establish the connection
    ///////////////////////////////////////////////////////////////////////////
    PrintLine( L"Connecting to title server..." );

    // Connect to server
    if( ( iResult = XNetConnect( g_inaSecure ) ) != 0 )
    {
        PrintLine( L"XNetConnect failed with %d.", iResult );
        return FALSE;
    }

    DWORD dwStartTime = GetTickCount();

    // Wait for the SG connection to complete
    while( ( dwResult = XNetGetConnectStatus( g_inaSecure ) ) == XNET_CONNECT_STATUS_PENDING )
    {
        if( ( GetTickCount() - dwStartTime ) > MAX_CONNECT_TIME )
        {
            PrintLine( L"XNetConnect took longer than %u ms to complete.", MAX_CONNECT_TIME );
            return FALSE;
        }
        Sleep( 100 );
    }

    if( dwResult != XNET_CONNECT_STATUS_CONNECTED )
    {
        PrintLine( L"XNetGetConnectStatus indicated the connection failed with %d.", dwResult );
        return FALSE;
    }

    // We should now be connected!
    PrintLine( L"Connected to \"%hs\".", Servers[ 0 ].szServerInfo );

    ///////////////////////////////////////////////////////////////////////////
    // Set up our socket
    ///////////////////////////////////////////////////////////////////////////
    g_Socket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if( g_Socket == SOCKET_ERROR )
    {
        PrintLine( L"Socket creation failed." );
        return FALSE;
    }

    SOCKADDR_IN addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons( TITLE_SERVER_PORT );

    iResult = bind( g_Socket, ( struct sockaddr* )&addr, sizeof( addr ) );
    if( iResult == SOCKET_ERROR )
    {
        PrintLine( L"Error binding socket, %d.", iResult );
        return FALSE;
    }

    DWORD dwNonBlocking = 1;
    iResult = ioctlsocket( g_Socket, FIONBIO, &dwNonBlocking );
    if( iResult == SOCKET_ERROR )
    {
        PrintLine( L"ioctlsocket failed with %d.", iResult );
        return FALSE;
    }

    return TRUE;
}


//-----------------------------------------------------------------------------
// Name: CloseConnection()
// Desc: Demonstrates closing the connection to the title server
//-----------------------------------------------------------------------------
void CloseConnection()
{
    INT iResult = 0;

    // Unregister the inaddr
    if( ( iResult = XNetUnregisterInAddr( g_inaSecure ) ) != 0 )
    {
        PrintLine( L"XNetUnregisterInAddr failed with %d.", iResult );
    }

    // Free our socket
    if( ( iResult = closesocket( g_Socket ) ) != 0 )
    {
        PrintLine( L"closesocket failed with %d.", iResult );
    }
    g_Socket = NULL;
}

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    SOCKADDR_IN RemoteInAddr;
    DOUBLE dLastSendTime = 0.0f;
    INT iResult;
    CHAR szMessage[ 256 ];
    INT iMessageLen;
    INT iPacketIndex = 0;
    INT iReceivedPackets = 0;

    // Initialize Live
    if( FAILED( XOnlineStartup() ) )
    {
        ATG::FatalError( "Failed to start Xbox Live\n" );
    }

    // Verify that the service ID has been changed
    if( SERVICEID_XLSPSAMPLE == 0xFFFFFFFF )
    {
        static const WCHAR* s_pwstrMsgBoxText =
            L"The service ID was not yet set.\n\n"
            L"Please set SERVICEID_XLSPSAMPLE in SimpleXLSPClient.cpp to your service ID, "
            L"and set the title ID in xex.xml to your title ID.";

        static LPCWSTR s_pwstrButtons[] = { L"OK" };

        MESSAGEBOX_RESULT result;
        XOVERLAPPED overlapped = { 0 };
        
        XShowMessageBoxUI( XUSER_INDEX_ANY, L"Error", s_pwstrMsgBoxText, ARRAYSIZE( s_pwstrButtons ),
            s_pwstrButtons, 0, XMB_ERRORICON, &result, &overlapped );
        do 
        {
            Sleep( 33 );
        } while ( !XHasOverlappedIoCompleted( &overlapped ) );

        ATG::FatalError( "The service ID was not yet set.  Please set SERVICEID_XLSPSAMPLE in SimpleXLSPClient.cpp"
                         "to your service ID and set the title ID in xex.xml to your title ID.\n" );
    }

    // Initialize login
    ATG::SignIn::Initialize( 1, 1, TRUE, 1 );

    while( ATG::SignIn::IsSystemUIShowing() || !ATG::SignIn::AreUsersSignedIn() )
    {
        // Update login
        ATG::SignIn::Update();

        // Wait for sign in to complete
    }

    // Initialize the console window
    g_Console.Create( "game:\\Media\\Fonts\\Arial_12.xpr", 0xFF1F005F, 0xFFFFFFFF );

    for(; ; )
    {
        if( !ATG::SignIn::GetSignedInUserCount() )
        {
            PrintLine( L"You must be signed in to connect to XLSP." );
        }
        else if( PrepareConnection() )
        {
            iPacketIndex = 0;
            iReceivedPackets = 0;

            for(; ; )
            {
                ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput(); // Detect reboot keypress
                if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
                {
                    break;  // Try again
                }

                SOCKADDR_IN ServerInAddr;
                ZeroMemory( &ServerInAddr, sizeof( ServerInAddr ) );
                ServerInAddr.sin_family = AF_INET;
                ServerInAddr.sin_addr = g_inaSecure;
                ServerInAddr.sin_port = htons( TITLE_SERVER_PORT );

                // On the send interval, create and send a UDP packet to the title server
                if( ( g_Timer.GetAppTime() - dLastSendTime ) >= PACKET_SEND_INTERVAL )
                {
                    dLastSendTime = g_Timer.GetAppTime();

                    if( iPacketIndex < PACKET_SEND_COUNT )
                    {
                        // Send some data...
                        sprintf_s( szMessage, "Packet %d", ++iPacketIndex );
                        PrintLine( L"Sending: %hs", szMessage );
                        iMessageLen = strlen( szMessage );
                        iResult = sendto( g_Socket, szMessage, iMessageLen, 0,
                                          ( struct sockaddr* )&ServerInAddr, sizeof( ServerInAddr ) );
                        if( iResult == SOCKET_ERROR )
                        {
                            PrintLine( L"sendto() failed with 0x%x.", WSAGetLastError() );
                        }
                    }

                }

                // Check if there is a packet from the title server
                int iRemoteAddrLen = sizeof( RemoteInAddr );
                iMessageLen = sizeof( szMessage );
                iResult = recvfrom( g_Socket, szMessage, iMessageLen, 0,
                                    ( struct sockaddr* )&RemoteInAddr, &iRemoteAddrLen );

                if( iResult != SOCKET_ERROR && iResult > 0 )
                {
                    iMessageLen = iResult;         // Length of the data received
                    szMessage[ iMessageLen ] = '\0'; // Terminate the data
                    PrintLine( L"Received: %hs", szMessage );

                    // When we receive the last packet break out of our loop
                    if( ++iReceivedPackets >= PACKET_SEND_COUNT )
                    {
                        break;
                    }
                }
            }

            CloseConnection();
            PrintLine( L"Connection closed." );
        }

        PrintLine( L"Press LT + RT + RB to exit, A to try again." );

        for(; ; )
        {
            ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput(); // Detect reboot keypress
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
            {
                break;  // Try again
            }
        }

        PrintLine( L"\n" );
    }
}

