//--------------------------------------------------------------------------------------
// DebugConsole.cpp
//
// Remote Xbox Debug Console. This app runs on a dev machine and connects
// to an app running on the Xbox. In this case, you should run the
// DebugChannel sample on the Xbox before running this sample on your
// dev machine.
//
// The two samples running together will then be able to communicate
// using the debug monitor API. This app (the debug console) is used to
// send commands to the Xbox app, which processes them, and returns a
// result back to the debug console for display in the output window.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DebugConsole.h"
#include "xbdm.h"
#include <string>   // Use the C++ library string object for easy and safe string manipulation.


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
HWND            g_hDlgMain;         // Main hwnd
HWND            g_hwndCommandCombo; // Combobox hwnd
HWND            g_hwndOutputWindow; // Output window
WNDPROC         g_hwndListBox;

BOOL            g_bConnected;    // Connected to Xbox?
BOOL            g_bECPConnected; // Connected to External Command Processor in app?
BOOL            g_bDebugMonitor; // Display debug output?

PDMN_SESSION    g_pdmnSession;   // Debug Monitor Session
PDM_CONNECTION  g_pdmConnection; // Debug Monitor Connection

PrintQueue      g_PrintQueue;

BYTE*           g_pBinarySendBuffer = NULL;     // Send buffer for binary data
DWORD           g_dwBinarySendBufferSize = 0;   // Size of send buffer data


//--------------------------------------------------------------------------------------
// Prototypes
//--------------------------------------------------------------------------------------
LRESULT CALLBACK DlgProc( HWND, UINT, WPARAM, LPARAM );
LRESULT CALLBACK SubclassedLBProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam );


//--------------------------------------------------------------------------------------
// Name: InitMainDlg
// Desc: Performs some initialization, caching hwnds, etc.
//--------------------------------------------------------------------------------------
VOID InitMainDlg()
{
    // Remember some useful HWNDs
    g_hwndOutputWindow = GetDlgItem( g_hDlgMain, IDC_RICHEDITCON );
    g_hwndCommandCombo = GetDlgItem( g_hDlgMain, IDC_RICHEDITCMD );

    // Subclass our dropdown command listbox to handle return key
    g_hwndListBox = SubclassWindow( GetWindow( g_hwndCommandCombo, GW_CHILD ), SubclassedLBProc );

    ShowWindow( g_hDlgMain, SW_SHOWNORMAL );

    // Change the font type of the output window to courier
    CHARFORMAT cf;
    cf.cbSize = sizeof( CHARFORMAT );
    SendMessage( g_hwndOutputWindow, EM_GETCHARFORMAT, 0, ( LPARAM )&cf );
    cf.dwMask &= ~CFM_COLOR;
    strcpy_s( cf.szFaceName, "courier" );
    SendMessage( g_hwndOutputWindow, EM_SETCHARFORMAT, SCF_ALL, ( LPARAM )&cf );
}


//--------------------------------------------------------------------------------------
// Name: WinMain
// Desc: Entry point for program
//--------------------------------------------------------------------------------------
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE, LPSTR, int )
{
    // Set up our print queue
    InitializeCriticalSection( &g_PrintQueue.CriticalSection );
    g_PrintQueue.dwNumMessages = 0;

    // Pull in common and rich edit controls
    InitCommonControls();
    HMODULE hRichEdit = LoadLibrary( TEXT( "Riched32.dll" ) );
    assert( hRichEdit );

    // Set up our window class
    WNDCLASS wndclass;
    wndclass.style = 0;
    wndclass.lpfnWndProc = DlgProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = DLGWINDOWEXTRA;
    wndclass.hInstance = hInstance;
    wndclass.hIcon = LoadIcon( hInstance, "DebugConsole" );
    wndclass.hCursor = LoadCursor( NULL, IDC_ARROW );
    wndclass.hbrBackground = GetStockBrush( LTGRAY_BRUSH );
    wndclass.lpszMenuName = MAKEINTRESOURCE( MENU_DebugConsole );
    wndclass.lpszClassName = "DebugConsole";

    RegisterClass( &wndclass );

    // Create our main dialog
    if( NULL == ( g_hDlgMain = CreateDialog( hInstance, "DebugConsole", 0, NULL ) ) )
    {
        char str[255];
        FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, GetLastError(), 0, str, 255, NULL );
        MessageBeep( 0 );
        FreeLibrary( hRichEdit );
        return 0;
    }

    // Init RTF controls, etc.
    InitMainDlg();

    // The xbdm.dll contains the functionality that DebugConsole needs. However
    // this DLL is not in the system path, and shouldn't be. To avoid this we
    // get DebugConsole to add %XEDK%\bin\win32 to its own copy of the path.
    // Then it loads xbdm.dll. This works because the project specifies that
    // xbdm.dll should be delay loaded, so the Win32 loader doesn't load it
    // when the process loads. As long as we load xbdm.dll before we use it
    // this all works magically.
    // If DebugConsole tries to use xbdm.dll and it cannot be loaded then
    // an exception is thrown. Loading xbdm.dll now is the best way to ensure
    // that it will be available when it is needed.
    // Alternate strategies would include copying xbdm.dll to the tool's directory,
    // or using the COM interface.
    const char* errorMessage = NULL;
    char* path;
    _dupenv_s( &path, NULL, "path" );

    char* xedkDir;
    _dupenv_s( &xedkDir, NULL, "xedk" );

    if( !xedkDir )
        xedkDir = "";
    // Build up a new path using std::string to handle memory management.
    std::string newpath = "path=" + std::string( path ) + ";" + std::string( xedkDir ) + "\\bin\\win32";
    // Set the path to the update version.
    _putenv( newpath.c_str() );
    HMODULE hXBDM = LoadLibrary( TEXT( "xbdm.dll" ) );
    if( !hXBDM )
    {
        if( xedkDir[0] )
            errorMessage = "Couldn't load xbdm.dll";
        else
            errorMessage = "Couldn't load xbdm.dll\nXEDK environment variable not set.";
    }

    // If anything goes wrong while trying to load xbdm.dll, bail out with
    // an error message.
    if( errorMessage )
    {
        MessageBox( g_hDlgMain, errorMessage, "Error", MB_OK | MB_ICONERROR );
        return 0;
    }

    SetWindowText( g_hwndOutputWindow,
                   "Run the DebugChannel sample and then select Connect from the Connection menu.\n" );

    HACCEL hAccel = LoadAccelerators( hInstance, MAKEINTRESOURCE( IDR_MAIN_ACCEL ) );

    MSG msg = {0};

    while( GetMessage( &msg, NULL, 0, 0 ) )
    {
        if( !TranslateAccelerator( g_hDlgMain, hAccel, &msg ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
    }

    FreeLibrary( hRichEdit );
    return msg.wParam;
}


//--------------------------------------------------------------------------------------
// Name: EnqueueStringForPrinting
// Desc: Formats the string and adds it to the print queue
//--------------------------------------------------------------------------------------
VOID EnqueueStringForPrinting( COLORREF rgb, LPCTSTR strFormat, ... )
{
    assert( g_PrintQueue.dwNumMessages <= NUM_STRINGS );

    // Enter critical section so we don't try to process the list
    EnterCriticalSection( &g_PrintQueue.CriticalSection );

    // If the queue is full, that means the main thread is probably blocked
    // and we'll have to drop this message.  Either allocate more space,
    // or make sure windows messages get processed more frequently on the
    // main thread.
    if( g_PrintQueue.dwNumMessages == NUM_STRINGS )
    {
        LeaveCriticalSection( &g_PrintQueue.CriticalSection );
        return;
    }

    // Print the message into the next slot
    va_list arglist;
    va_start( arglist, strFormat );
    vsprintf_s( g_PrintQueue.astrMessages[ g_PrintQueue.dwNumMessages ], strFormat, arglist );

    va_end( arglist );
    g_PrintQueue.aColors[ g_PrintQueue.dwNumMessages++ ] = rgb;

    // Ensure we've got a message posted to process the print queue
    if( g_PrintQueue.dwNumMessages == 1 )
        PostMessage( g_hDlgMain, WM_USER, 0, 0 );

    // Done - now the main thread can safely process the list
    LeaveCriticalSection( &g_PrintQueue.CriticalSection );
}


//--------------------------------------------------------------------------------------
// Name: ProcessEnqueuedStrings
// Desc: Processes the list of enqueued strings to be printed (put there by
//       our notification handlers)
//--------------------------------------------------------------------------------------
VOID ProcessEnqueuedStrings()
{
    // Enter critical section so we don't try to add anything while we're
    // processing
    EnterCriticalSection( &g_PrintQueue.CriticalSection );

    for( DWORD i = 0; i < g_PrintQueue.dwNumMessages; ++i )
    {
        ConsoleWindowPrintf( g_PrintQueue.aColors[i], "%s", g_PrintQueue.astrMessages[i] );
    }

    g_PrintQueue.dwNumMessages = 0;

    // Done - now we can safely add to the list
    LeaveCriticalSection( &g_PrintQueue.CriticalSection );
}


//--------------------------------------------------------------------------------------
// Name: ConsoleWindowPrintf
// Desc: Writes out a string directly to the console window
//--------------------------------------------------------------------------------------
int ConsoleWindowPrintf( COLORREF rgb, LPCTSTR strFormat, ... )
{
    int dwStrLen;
    char strTemp[512];
    va_list arglist;
    CHARRANGE cr = { -1, -2 };

    if( rgb != CLR_INVALID )
    {
        // Set whatever colors, etc. they want
        CHARFORMAT cf = {0};
        cf.cbSize = sizeof( cf );
        cf.dwMask = CFM_COLOR;
        cf.dwEffects = 0;
        cf.crTextColor = rgb;
        SendDlgItemMessage( g_hDlgMain, IDC_RICHEDITCON, EM_SETCHARFORMAT, SCF_SELECTION, ( LPARAM )&cf );
    }

    // Get our string to print
    va_start( arglist, strFormat );
    dwStrLen = vsprintf_s( strTemp, strFormat, arglist );
    va_end( arglist );

    // Move the selection to the end
    SendDlgItemMessage( g_hDlgMain, IDC_RICHEDITCON, EM_EXSETSEL, 0, ( LPARAM )&cr );

    // Add the text and scroll it into view
    SendDlgItemMessage( g_hDlgMain, IDC_RICHEDITCON, EM_REPLACESEL, 0, ( LONG )( LPSTR )strTemp );
    SendDlgItemMessage( g_hDlgMain, IDC_RICHEDITCON, EM_SCROLLCARET, 0, 0L );

    return dwStrLen;
}


//--------------------------------------------------------------------------------------
// Name: RCmdHandle
// Desc: Command parser and dispatcher
//--------------------------------------------------------------------------------------
BOOL RCmdHandle( char* strCmd, BOOL bDisplayCommand )
{
    const int MAXARGVELEMS = 10;
    TCHAR strCmdBak[MAX_PATH];
    char* argv[MAXARGVELEMS];
    BOOL bBuiltinCommand = FALSE;

    // If we're trying to type a binary control command, don't send it
    // If the app is generating a binary control command, bDisplayCommand is FALSE
    if( bDisplayCommand && strCmd[0] == '_' && strCmd[1] == '_' )
        return TRUE;

    // Make a copy of the original command string
    strcpy_s( strCmdBak, strCmd );

    int argc = CmdToArgv( strCmd, argv, MAXARGVELEMS );

    // Nothing but whitespace?
    if( !argv[0][0] )
        return TRUE;

    if( bDisplayCommand )
    {
        // See if we already have this entry in the command history
        int iIndex = ComboBox_FindStringExact( g_hwndCommandCombo, -1, strCmdBak );
        if( iIndex != CB_ERR )
        {
            ComboBox_DeleteString( g_hwndCommandCombo, iIndex );
        }
        ComboBox_InsertItemData( g_hwndCommandCombo, 0, strCmdBak );

        // Limit the # of history items
        if( ( iIndex = ComboBox_GetCount( g_hwndCommandCombo ) ) > 25 )
            ComboBox_DeleteString( g_hwndCommandCombo, iIndex - 1 );
    }

    // See if the command is recognized
    for( int i = 0; i < g_dwNumRemoteCommands; ++i )
    {
        if( !lstrcmpiA( g_RemoteCommands[i].strCommand, argv[0] ) )
        {
            // Echo command to the window
            ConsoleWindowPrintf( CLR_INVALID, "%s\n", strCmdBak );

            // If handler was null, that means send it through raw to the Xbox
            if( !g_RemoteCommands[i].pfnHandler )
            {
                bBuiltinCommand = TRUE;
                break;
            }

            // If we called a handler, and it returns false, then we should
            // send the command remotely as well, otherwise we're done
            if( !g_RemoteCommands[i].pfnHandler( argc, argv ) )
                break;

            return TRUE;
        }
    }

    // OK, try to send the command over the wire
    if( g_bConnected )
    {
        char strRemoteCmd[MAX_PATH + 10];
        char strResponse[MAX_PATH];
        DWORD dwResponseLen = MAX_PATH;
        HRESULT hr;

        if( !bBuiltinCommand )
        {
            // App-defined commands need a command prefix to get routed properly
            strcpy_s( strRemoteCmd, DEBUGCONSOLE_COMMAND_PREFIX "!" );
            strcat_s( strRemoteCmd, strCmdBak );

            if( bDisplayCommand )
            {
                // Echo remote command
                ConsoleWindowPrintf( CLR_INVALID, "%s\n", strRemoteCmd );
            }
        }
        else
        {
            strcpy_s( strRemoteCmd, strCmdBak );
        }

        // Send the command to the Xbox
        hr = DmSendCommand( g_pdmConnection, strRemoteCmd, strResponse, &dwResponseLen );

        if( FAILED( hr ) )
        {
            DisplayError( strResponse, "DmSendCommand", hr );
        }
        else
        {
            // There are different success codes.  If your app defines
            // a command which needs or returns binary data, You will
            // need to take appropriate action here.
            BOOL bFinished = FALSE;
            while( !bFinished )
            {
                switch( hr )
                {
                    case XBDM_NOERR:
                        if( dwResponseLen )
                            ConsoleWindowPrintf( RGB( 0, 0, 255 ), "%s\n", strResponse );
                        bFinished = TRUE;
                        break;
                    case XBDM_MULTIRESPONSE:
                        // Multi-line response - loop, looking for end of response
                        for(; ; )
                        {
                            DWORD dwResponseLen = sizeof( strResponse );

                            hr = DmReceiveSocketLine( g_pdmConnection, strResponse, &dwResponseLen );
                            if( FAILED( hr ) || strResponse[0] == '.' )
                                break;
                            ConsoleWindowPrintf( RGB( 0, 0, 255 ), "%s\n", strResponse );
                        }
                        bFinished = TRUE;
                        break;
                    case XBDM_BINRESPONSE:
                        ConsoleWindowPrintf( RGB( 0, 0, 255 ), "Receiving binary data.\n" );
                        {
                            // Retrieve the file size, since we know the app will send that first.
                            DWORD DataSize = 0;
                            DWORD BytesReceived = 0;
                            hr = DmReceiveBinary( g_pdmConnection, &DataSize, sizeof( DWORD ), &BytesReceived );
                            if( FAILED( hr ) )
                            {
                                bFinished = TRUE;
                                break;
                            }
                            DataSize = _byteswap_ulong( DataSize );
                            assert( DataSize < 1048576 );
                            // Create a buffer to hold the data we're about to receive.
                            BYTE* pReceiveBuffer = new BYTE[ DataSize ];
                            BYTE* pCurrentPos = pReceiveBuffer;
                            while( DataSize > 0 )
                            {
                                // Receive the data in 8192 byte chunks.
                                DWORD ChunkSize = min( DataSize, 8192 );
                                hr = DmReceiveBinary( g_pdmConnection, pCurrentPos, ChunkSize, &ChunkSize );
                                if( FAILED( hr ) )
                                {
                                    ConsoleWindowPrintf( RGB( 255, 0, 0 ), "Binary receive failed.\n" );
                                    bFinished = TRUE;
                                    break;
                                }
                                // Decrement remaining byte count, increment byte received count.
                                DataSize -= ChunkSize;
                                BytesReceived += ChunkSize;
                                pCurrentPos += ChunkSize;
                            }
                            // Delete received data buffer.
                            // In a real scenario, you would go do something useful with this data now,
                            // instead of deleting it.
                            delete[] pReceiveBuffer;
                            ConsoleWindowPrintf( RGB( 0, 0, 255 ), "%d bytes received.\n", BytesReceived );
                            bFinished = TRUE;
                        }
                        break;
                    case XBDM_READYFORBIN:
                        ConsoleWindowPrintf( RGB( 0, 0, 255 ), "Sending binary data.\n" );
                        if( g_pBinarySendBuffer != NULL )
                        {
                            // send binary data
                            hr = DmSendBinary( g_pdmConnection, g_pBinarySendBuffer, g_dwBinarySendBufferSize );
                            if( SUCCEEDED( hr ) )
                            {
                                // delete our send buffer
                                delete[] g_pBinarySendBuffer;
                                g_pBinarySendBuffer = NULL;
                                g_dwBinarySendBufferSize = 0;
                                hr = DmReceiveStatusResponse( g_pdmConnection, strResponse, &dwResponseLen );
                            }
                            else
                            {
                                bFinished = TRUE;
                            }
                        }
                        else
                        {
                            bFinished = TRUE;
                        }
                        break;
                    default:
                        ConsoleWindowPrintf( RGB( 255, 0, 0 ), "Unknown success code (%s).\n", strResponse );
                        bFinished = TRUE;
                        break;
                }
            }
        }
    }

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: SendBinaryData
// Desc: Fills a buffer with dummy data, and initiates a transfer to the dev kit
//--------------------------------------------------------------------------------------
VOID SendBinaryData()
{
    // If the buffer pointer is not NULL, then the previous binary send operation has not
    // completed yet, and we should not proceed.
    if( g_pBinarySendBuffer != NULL )
        return;

    // Create a 256KB buffer (size is arbitrary)
    g_dwBinarySendBufferSize = 262144;
    g_pBinarySendBuffer = new BYTE[ g_dwBinarySendBufferSize ];

    // Fill buffer with a letter of the alphabet (will be displayed on the dev kit)
    static BYTE CharIndex = 0;
    memset( g_pBinarySendBuffer, ( BYTE )'A' + CharIndex, g_dwBinarySendBufferSize );
    CharIndex = ( CharIndex + 1 ) % 26;

    // Send the special recvbinary command that the dev kit app is looking for
    char strCommand[100];
    sprintf_s( strCommand, "__recvbinary__ %d", g_dwBinarySendBufferSize );
    RCmdHandle( strCommand, FALSE );
}


//--------------------------------------------------------------------------------------
// Name: HandleWmKeyDown
// Desc: Handle a WmKeyDown in our RTF cmd window
//--------------------------------------------------------------------------------------
BOOL HandleWmKeyDown( WPARAM wParam )
{
    BOOL bHandled = FALSE;

    switch( wParam )
    {
        case VK_RETURN:
            // User hit return in the combo box
            if( ComboBox_GetDroppedState( g_hwndCommandCombo ) )
            {
                ComboBox_ShowDropdown( g_hwndCommandCombo, FALSE );
            }
            else
            {
                PostMessage( g_hDlgMain, WM_APP, 0, 0 );
                bHandled = TRUE;
            }
            break;
    }

    return bHandled;
}


//--------------------------------------------------------------------------------------
// Name: SubclassedLBProc
// Desc: Our subclassed listbox proc
//--------------------------------------------------------------------------------------
LRESULT CALLBACK SubclassedLBProc( HWND hDlg, UINT msg,
                                   WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
        case WM_KEYDOWN:
            if( HandleWmKeyDown( wParam ) )
                return 1;
            break;
        case WM_CHAR:
            if( wParam == VK_RETURN )
                return 1;
            break;
    }

    return CallWindowProc( g_hwndListBox, hDlg, msg, wParam, lParam );
}


//--------------------------------------------------------------------------------------
// Name: HandleWmSize
// Desc: Handles a WM_SIZE message by resizing all our child windows to 
//       match the main window
//--------------------------------------------------------------------------------------
void HandleWmSize( HWND hDlg, UINT, int cx, int cy )
{
    if( cx == 0 || cy == 0 )
    {
        RECT rcClient;
        GetClientRect( hDlg, &rcClient );
        cx = rcClient.right;
        cy = rcClient.bottom;
    }

    // If we're big enough, position our child windows
    if( g_hwndCommandCombo && cx > 64 && cy > 64 )
    {
        RECT rcCmd;

        // Fit the combo box into our window
        GetWindowRect( g_hwndCommandCombo, &rcCmd );
        ScreenToClient( hDlg, ( LPPOINT )&rcCmd );
        ScreenToClient( hDlg, ( LPPOINT )&rcCmd + 1 );

        int x = rcCmd.left;
        int dx = cx - 4 - x;
        int dy = rcCmd.bottom - rcCmd.top;
        int y = cy - 4 - dy;

        SetWindowPos( g_hwndCommandCombo, NULL, x, y,
                      dx, dy, SWP_NOZORDER );

        // Position the "Cmd" label
        RECT rcStaticCmd;
        HWND hStaticCmd = GetDlgItem( g_hDlgMain, IDC_Cmd );

        GetWindowRect( hStaticCmd, &rcStaticCmd );
        ScreenToClient( hDlg, ( LPPOINT )&rcStaticCmd );
        ScreenToClient( hDlg, ( LPPOINT )&rcStaticCmd + 1 );
        SetWindowPos( hStaticCmd, NULL, 4,
                      y + ( dy - ( rcStaticCmd.bottom - rcStaticCmd.top ) ) / 2 - 1,
                      0, 0, SWP_NOSIZE | SWP_NOZORDER );

        // Position the output window
        RECT rcOut;
        GetWindowRect( g_hwndOutputWindow, &rcOut );
        ScreenToClient( hDlg, ( LPPOINT )&rcOut );

        int dwWidth = cx - rcOut.left - 4;
        int dwHeight = y - rcOut.top - 4;

        SetWindowPos( g_hwndOutputWindow, NULL, 0, 0,
                      dwWidth, dwHeight, SWP_NOMOVE | SWP_NOZORDER );
    }

}


//--------------------------------------------------------------------------------------
// Name: DlgProc
// Desc: Our main dialog proc
//--------------------------------------------------------------------------------------
LRESULT CALLBACK DlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    WORD wID = LOWORD( wParam );

    switch( message )
    {
        case WM_APP:
        {
            // Take the string from the command window and process it
            char strCmd[MAX_PATH + 3]; // extra room for \r\n
            ComboBox_GetText( g_hwndCommandCombo, strCmd, MAX_PATH );

            RCmdHandle( strCmd, TRUE );

            ComboBox_SetText( g_hwndCommandCombo, "" );
            break;
        }
        case WM_USER:
            ProcessEnqueuedStrings();
            break;
        case WM_SIZE:
            HANDLE_WM_SIZE( hDlg, wParam, lParam, HandleWmSize );
            break;
        case WM_SYSCOMMAND:
            if( wID == SC_CLOSE )
            {
                PostMessage( hDlg, WM_CLOSE, 0, 0 );
                return 0;
            }
            break;
        case WM_CLOSE:
            // Must disconnect before closing
            RCmdDisconnect( 0, NULL );
            DeleteCriticalSection( &g_PrintQueue.CriticalSection );

            DestroyWindow( hDlg );
            break;
        case WM_DESTROY:
            SubclassWindow( g_hwndCommandCombo, g_hwndListBox );
            PostQuitMessage( 0 );
            return 0;
        case WM_INITMENU:
            // Set up our menus
            CheckMenuItem( ( HMENU )wParam, IDM_DEBUGMONITOR, MF_BYCOMMAND | ( g_bDebugMonitor ? MF_CHECKED :
                                                                               MF_UNCHECKED ) );
            EnableMenuItem( ( HMENU )wParam, IDM_DEBUGMONITOR,
                            MF_BYCOMMAND | ( g_bConnected ? MF_ENABLED : MF_GRAYED ) );
            CheckMenuItem( ( HMENU )wParam, IDM_CONNECTTOBOX,
                           MF_BYCOMMAND | ( g_bConnected ? MF_CHECKED : MF_UNCHECKED ) );
            EnableMenuItem( ( HMENU )wParam, IDM_SENDBINARYDATA,
                            MF_BYCOMMAND | ( g_bConnected ? MF_ENABLED : MF_GRAYED ) );
            EnableMenuItem( ( HMENU )wParam, IDM_RECEIVEBINARYDATA,
                            MF_BYCOMMAND | ( g_bConnected ? MF_ENABLED : MF_GRAYED ) );
            return 0;
        case WM_COMMAND:
            switch( wID )
            {
                case IDM_DEBUGMONITOR:
                    g_bDebugMonitor = !g_bDebugMonitor;
                    return 0;
                case IDM_CONNECTTOBOX:
                    if( g_bConnected )
                    {
                        RCmdDisconnect( 0, NULL );
                    }
                    else
                    {
                        RCmdConnect( 0, NULL );
                    }
                    return 0;
                case IDM_SENDBINARYDATA:
                    SendBinaryData();
                    return 0;
                case IDM_RECEIVEBINARYDATA:
                    // tell the devkit app to send binary data now
                {
                    char strCommand[100];
                    strcpy_s( strCommand, "__sendbinary__" );
                    RCmdHandle( strCommand, FALSE );
                    return 0;
                }
                case IDM_Exit:
                    PostMessage( hDlg, WM_CLOSE, 0, 0 );
                    return 0;
            }
            break;
    }

    return DefDlgProc( hDlg, message, wParam, lParam );
}


//--------------------------------------------------------------------------------------
// Name: DisplayError
// Desc: Display friendly error by translating the hr to a message
//--------------------------------------------------------------------------------------
VOID DisplayError( const CHAR* strResponse, const CHAR* strApiName, HRESULT hr )
{
    CHAR strError[100];

    if( FAILED( DmTranslateError( hr, strError, 100 ) ) )
        return;

    if( hr == XBDM_UNDEFINED )
        strcpy_s( strError, strResponse );

    if( strError )
        ConsoleWindowPrintf( RGB( 255, 0, 0 ), "%s failed: '%s'\n", strApiName, strError );
    else
        ConsoleWindowPrintf( RGB( 255, 0, 0 ), "%s failed: 0x%08lx\n", strApiName, hr );
}


//--------------------------------------------------------------------------------------
// Name: CmdToArgv
// Desc: Parses a string into argv and return # of args.
//--------------------------------------------------------------------------------------
int CmdToArgv( char* str, char* argv[], int maxargs )
{
    int argc = 0;
    int argcT = 0;
    char* strNil = str + lstrlenA( str );

    while( argcT < maxargs )
    {
        // Eat whitespace
        while( *str && ( *str == ' ' ) )
            str++;

        if( !*str )
        {
            argv[argcT++] = strNil;
        }
        else
        {
            // Find the end of this arg
            char chEnd = ( *str == '"' || *str == '\'' ) ? *str++ : ' ';
            char* strArgEnd = str;
            while( *strArgEnd && ( *strArgEnd != chEnd ) )
                strArgEnd++;

            // Record this argument
            argv[argcT++] = str;
            argc = argcT;

            // Move strArg to the next argument (or not, if we hit the end)
            str = *strArgEnd ? strArgEnd + 1 : strArgEnd;
            *strArgEnd = 0;
        }
    }

    return argc;
}
