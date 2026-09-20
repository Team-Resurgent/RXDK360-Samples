//-------------------------------------------------------------------------------------
// DmAutoSample.cpp
//
// Microsoft XNA Game Platform Extensions Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "stdafx.h"
#include "resource.h"

#define MAX_LOADSTRING  100
#define WM_NOTIFYICON   WM_APP
#define WM_NOTIFYEXIT   ( WM_APP + 1 )
#define WM_THREADTIMER  ( WM_APP + 2 )

#define DM_AUTO_THREAD_MAX_WAIT   10000

// Global Variables:
HINSTANCE   hInst;                        // current instance
HWND        g_hwndDlg;                         // hidden dialog
TCHAR szTooltip[ MAX_LOADSTRING ];      // The tooltip text
TCHAR szExit[ MAX_LOADSTRING ];         // the menu "exit" item
BOOL        g_bDone = FALSE;                   // set to TRUE when the app is done
HANDLE      g_hThread = NULL;                // handle to the polling thread

// Forward declarations of functions included in this code module:
BOOL InitInstance( HINSTANCE, int );
INT_PTR CALLBACK    DlgProc( HWND, UINT, WPARAM, LPARAM );
VOID ShowContextMenu( HWND hWnd );
BOOL DelayLoadXBDM( void );
DWORD WINAPI        DmAutoThread( LPVOID lpParameter );


//-------------------------------------------------------------------------------------
// Name: wWinMain
//-------------------------------------------------------------------------------------
int APIENTRY wWinMain( HINSTANCE hInstance,
                       HINSTANCE hPrevInstance,
                       LPWSTR lpCmdLine,
                       int nCmdShow )
{
    UNREFERENCED_PARAMETER( hPrevInstance );
    UNREFERENCED_PARAMETER( lpCmdLine );

    MSG msg;
    HACCEL hAccelTable;

    if( !DelayLoadXBDM() )
    {
        return FALSE;
    }

    // Initialize global strings
    LoadString( hInstance, IDS_TOOLTIP, szTooltip, MAX_LOADSTRING );
    LoadString( hInstance, IDS_EXIT, szExit, MAX_LOADSTRING );

    // Perform application initialization:
    if( !InitInstance( hInstance, nCmdShow ) )
    {
        return FALSE;
    }

    hAccelTable = LoadAccelerators( hInstance, MAKEINTRESOURCE( IDC_DMAUTOSAMPLE ) );

    // Main message loop:
    while( GetMessage( &msg, NULL, 0, 0 ) )
    {
        if( !TranslateAccelerator( msg.hwnd, hAccelTable, &msg ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
    }

    return( ( int )msg.wParam );
}

//-------------------------------------------------------------------------------------
// Name: DelayLoadXBDM
//-------------------------------------------------------------------------------------
BOOL DelayLoadXBDM( void )
{
    // Using vector for automatic resource de-allocation
    size_t size;
    std::vector <char> strPath;
    getenv_s( &size, NULL, 0, "PATH" );
    strPath.resize( size );
    getenv_s( &size, &strPath[ 0 ], size, "PATH" );

    std::vector <char> strXedk;
    getenv_s( &size, NULL, 0, "XEDK" );
    strXedk.resize( size );
    getenv_s( &size, &strXedk[ 0 ], size, "xedk" );

    std::vector <char> strNewPath;
    size = _scprintf( "PATH=%s;%s\\bin\\win32", &strPath[ 0 ], &strXedk[ 0 ] ) + 1;
    strNewPath.resize( size );
    sprintf_s( &strNewPath[ 0 ],
               strNewPath.size(),
               "PATH=%s;%s\\bin\\win32",
               &strPath[ 0 ],
               &strXedk[ 0 ] );
    _putenv( &strNewPath[ 0 ] );

    HMODULE hXBDM = LoadLibraryA( "xbdm.dll" );
    if( !hXBDM )
    {
        return FALSE;
    }

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: InitInstance
//-------------------------------------------------------------------------------------
BOOL InitInstance( HINSTANCE hInstance, int nCmdShow )
{
    UNREFERENCED_PARAMETER( nCmdShow );

    hInst = hInstance; // Store instance handle in our global variable

    g_hwndDlg = CreateDialog( hInstance,
                              MAKEINTRESOURCE( IDD_HIDDEN ),
                              NULL,
                              ( DLGPROC )DlgProc );

    if( !g_hwndDlg )
    {
        return FALSE;
    }

    NOTIFYICONDATA nid = { 0 };

    nid.cbSize = sizeof( nid );
    nid.hWnd = g_hwndDlg;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_NOTIFYICON;
    nid.hIcon = LoadIcon( hInstance, MAKEINTRESOURCE( IDI_TRAY ) );
    _tcscpy_s( nid.szTip, _countof( nid.szTip ), szTooltip );

    Shell_NotifyIcon( NIM_ADD, &nid );

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: ShowContextMenu
//-------------------------------------------------------------------------------------
VOID ShowContextMenu( HWND hWnd )
{
    POINT pt;
    GetCursorPos( &pt );
    HMENU hMenu = CreatePopupMenu();
    if( hMenu != NULL )
    {
        InsertMenu( hMenu, ( UINT )-1, MF_BYPOSITION, WM_NOTIFYEXIT, szExit );
        SetForegroundWindow( hWnd );
        TrackPopupMenu( hMenu,
                        TPM_BOTTOMALIGN,
                        pt.x,
                        pt.y,
                        0,
                        hWnd,
                        NULL );
        DestroyMenu( hMenu );
    }
}

//-------------------------------------------------------------------------------------
// Name: DlgProc
//-------------------------------------------------------------------------------------
INT_PTR CALLBACK DlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    UNREFERENCED_PARAMETER( lParam );

    switch( message )
    {
        case WM_INITDIALOG:
            g_hThread = CreateThread( NULL, 0, DmAutoThread, NULL, 0, NULL );
            SetTimer( hDlg, WM_THREADTIMER, 250, NULL );
            return( ( INT_PTR )TRUE );

        case WM_TIMER:
            if( wParam == WM_THREADTIMER )
            {
                DWORD dwExitCode = 0;

                // If DmAutoThread has exited, display the error message and quit
                if( WaitForSingleObject( g_hThread, 0 ) == WAIT_OBJECT_0 )
                {
                    KillTimer( hDlg, WM_THREADTIMER );
                    GetExitCodeThread( g_hThread, &dwExitCode );
                    switch( dwExitCode )
                    {
                        case ERROR_DEVICE_NOT_CONNECTED:
                            MessageBox( hDlg,
                                        L"No Xbox 360 Controller found or Controller lost!\nPlease connect Xbox 360 Controller to your Host PC's USB port and try again.",
                                        L"DMAutomationInput Sample Error", MB_ICONERROR );
                            break;
                        case XBDM_CANNOTCONNECT:
                            MessageBox( hDlg, L"Can't connect to Xbox 360 Dev Kit!",
                                        L"DMAutomationInput Sample Error", MB_ICONERROR );
                            break;
                        case XBDM_CONNECTIONLOST:
                            MessageBox( hDlg, L"Xbox 360 Dev Kit connection lost!",
                                        L"DMAutomationInput Sample Error", MB_ICONERROR );
                            break;
                    }

                    // The worker thread has stopped. Exit the application
                    DestroyWindow( hDlg );
                }
            }
            break;

        case WM_NOTIFYICON:
            switch( lParam )
            {
                case WM_RBUTTONDOWN:
                case WM_CONTEXTMENU:
                    ShowContextMenu( hDlg );
            }
            break;

        case WM_COMMAND:
            switch( LOWORD( wParam ) )
            {
                case WM_NOTIFYEXIT:
                    DestroyWindow( hDlg );
                    break;
            }
            return 1;

        case WM_DESTROY:
        {
            NOTIFYICONDATA nid = { 0 };
            nid.cbSize = sizeof( nid );
            nid.hWnd = hDlg;
            nid.uID = 1;
            Shell_NotifyIcon( NIM_DELETE, &nid );
            g_bDone = TRUE;
            KillTimer( hDlg, WM_THREADTIMER );
            if( WaitForSingleObject( g_hThread, DM_AUTO_THREAD_MAX_WAIT ) != WAIT_OBJECT_0 )
            {
                TerminateThread( g_hThread, ( DWORD )-1 );
            }

        }
            PostQuitMessage( 0 );
            break;
    }

    return( ( INT_PTR )FALSE );
}
