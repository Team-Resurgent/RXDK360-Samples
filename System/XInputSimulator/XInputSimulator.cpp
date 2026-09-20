//-------------------------------------------------------------------------------------
// XInputSimulator.cpp
//
// This sample can be used to drive the XSim debugger extension on the default
// console. All player types can be started: random input, text sequence and file
// player and file recorder. It also supports playback of files saved in the
// console's DEVKIT: drive.
//
// Microsoft XNA Game Platform Extensions Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "stdafx.h"
#include "XSimClient.h"
#include "Resource.h"


#define     WM_INITCTRLS    (WM_USER + 1)

static const CHAR*  XSIM_TEXTSEQUENCE_HELPFILE = "doc\\1033\\Xbox360SDK.chm::/xinput_simulator_using_textsequence.htm";
static const CHAR*  XSIM_XDK_HELPFILE = "doc\\1033\\Xbox360SDK.chm::/atoc_ov_xinput_simulator.htm";

HWND                g_hwndDlg;
HINSTANCE           g_hInstance;
XSimClient*         g_pXSimClient;

LRESULT CALLBACK WndProc( HWND, UINT, WPARAM, LPARAM );

//-------------------------------------------------------------------------------------
// Name: LoadStdStringW
//-------------------------------------------------------------------------------------
std::wstring LoadStdStringW( UINT uID )
{
    assert( g_hInstance != NULL );

    WCHAR wstrResource[LOADSTRING_MAX];
    int nChars = ( int )LoadStringW( g_hInstance,
                                     uID,
                                     wstrResource,
                                     _countof( wstrResource ) );
    std::wstring wstrReturn = L"";
    if( nChars > 0 )
    {
        wstrReturn.append( wstrResource );
    }

    return wstrReturn;
}

//-------------------------------------------------------------------------------------
// Name: DelayLoadXBDM
//-------------------------------------------------------------------------------------
bool DelayLoadXBDM( HWND hwndDlg )
{
    // Using vector for automatic resource de-allocation
    size_t size;
    std::vector <char> strPath;
    getenv_s( &size, NULL, 0, "PATH" );
    strPath.resize( size );
    getenv_s( &size, &strPath[0], size, "PATH" );

    std::vector <char> strXedk;
    getenv_s( &size, NULL, 0, "XEDK" );
    strXedk.resize( size );
    getenv_s( &size, &strXedk[0], size, "xedk" );

    std::vector <char> strNewPath;
    size = _scprintf( "PATH=%s;%s\\bin\\win32", &strPath[0], &strXedk[0] ) + 1;
    strNewPath.resize( size );
    sprintf_s( &strNewPath[0],
               strNewPath.size(),
               "PATH=%s;%s\\bin\\win32",
               &strPath[0],
               &strXedk[0] );
    _putenv( &strNewPath[0] );

    HMODULE hXBDM = LoadLibraryA( "xbdm.dll" );
    if( !hXBDM )
    {
        std::wstring wstrTitle = LoadStdStringW( IDS_ERROR_TITLE );
        std::wstring wstrError = LoadStdStringW( IDS_DELAYLOAD_ERROR );

        if( strXedk.size() == 0 )
        {
            wstrError += L"\n" + LoadStdStringW( IDS_XEDK_ERROR );
        }

        MessageBoxW( hwndDlg,
                     wstrError.c_str(),
                     wstrTitle.c_str(),
                     MB_OK | MB_ICONERROR );

        return false;
    }

    return true;
}

//-------------------------------------------------------------------------------------
// Name: WinMain
//-------------------------------------------------------------------------------------
int APIENTRY WinMain( HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPTSTR lpCmdLine,
                      int nCmdShow )
{
    g_hInstance = hInstance;

    InitCommonControls();

    HMODULE hRichEdit = LoadLibraryA( "Riched32.dll" );
    assert( hRichEdit != NULL );

    WNDCLASSEXA wcex = {0};

    wcex.cbSize = sizeof( WNDCLASSEX );
    wcex.style = 0;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = DLGWINDOWEXTRA;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconA( hInstance, MAKEINTRESOURCEA( IDI_XSIMSAMPLE ) );
    wcex.hCursor = LoadCursorA( NULL, IDC_ARROW );
    wcex.hbrBackground = GetSysColorBrush( COLOR_WINDOW );
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = "XSimSample";
    wcex.hIconSm = LoadIconA( hInstance, MAKEINTRESOURCEA( IDI_XSIMSAMPLE ) );

    RegisterClassExA( &wcex );

    g_hwndDlg = CreateDialogW( hInstance,
                               MAKEINTRESOURCEW( IDD_XSIMSAMPLE_DIALOG ),
                               0,
                               NULL );

    assert( g_hwndDlg );

    ShowWindow( g_hwndDlg, nCmdShow );
    UpdateWindow( g_hwndDlg );

    SendMessage( g_hwndDlg, WM_INITCTRLS, 0, 0 );

    if( DelayLoadXBDM( g_hwndDlg ) == false )
    {
        return 0;
    }

    g_pXSimClient = new XSimClient();
    g_pXSimClient->Initialize( g_hwndDlg, hInstance );

    HACCEL hAcc = LoadAccelerators( hInstance, MAKEINTRESOURCEA( IDC_XSIMSAMPLE ) );

    MSG msg;
    while( GetMessage( &msg, NULL, 0, 0 ) )
    {
        if( !TranslateAccelerator( g_hwndDlg, hAcc, &msg ) )
        {
            if( ( msg.message == WM_KEYDOWN ) && ( msg.wParam == VK_RETURN ) )
            {
                // If the control has set a default button, the RETURN key activates it
                LONG_PTR target = GetWindowLongPtrA( msg.hwnd, GWLP_USERDATA );
                if( target != 0 )
                {
                    PostMessage( g_hwndDlg, WM_COMMAND, ( WPARAM )target, 0 );
                    continue;
                }
            }
            if( !IsDialogMessage( g_hwndDlg, &msg ) )
            {
                TranslateMessage( &msg );
                DispatchMessage( &msg );
            }
        }
    }

    g_pXSimClient->Uninitialize();
    delete g_pXSimClient;

    FreeLibrary( hRichEdit );

    UNREFERENCED_PARAMETER( hPrevInstance );
    UNREFERENCED_PARAMETER( lpCmdLine );

    return ( int )msg.wParam;
}

//-------------------------------------------------------------------------------------
// Name: QuickHelpProc
//-------------------------------------------------------------------------------------

INT_PTR CALLBACK QuickHelpProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    switch( message )
    {
        case WM_COMMAND:
            EndDialog( hWnd, 0 );
            break;
    }

    UNREFERENCED_PARAMETER( wParam );
    UNREFERENCED_PARAMETER( lParam );
    return 0;
}


void OpenTextSequenceHelp()
{
    CHAR EnvironmentValue[MAX_PATH];
    DWORD   Size;

    // Get the XDK path
    Size = GetEnvironmentVariable(TEXT("xedk"), EnvironmentValue, MAX_PATH);
    if (Size != 0 && (Size * sizeof(TCHAR) < MAX_PATH) 
        && EnvironmentValue[0] != TEXT('\0'))
    {
        size_t StrLen = strlen(EnvironmentValue);
        char* pStr = EnvironmentValue + StrLen - 1;

        if (*pStr != TEXT('\\') && StrLen < MAX_PATH - 1)
        {
            strcat_s(EnvironmentValue, TEXT("\\"));
        }
    }

    errno_t crterr;
    crterr = strcat_s(EnvironmentValue,
        TEXT(XSIM_TEXTSEQUENCE_HELPFILE));

    if (crterr == 0 )
    {
        STARTUPINFOA si = { 0 };
        PROCESS_INFORMATION pi = { 0 };
        si.cb = sizeof(si);

        char helpCmd[256];
        sprintf_s(helpCmd, "hh.exe %s", EnvironmentValue);

        CreateProcessA(NULL, helpCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}
//-------------------------------------------------------------------------------------
// Name: WndProc
//-------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    switch( message )
    {
        case WM_CREATE:
        {
            // Centering application window to the primary display 
            HDC hdc = GetDC( NULL );
            if( hdc )
            {
                RECT rectApp;
                GetWindowRect( hWnd, &rectApp );

                int nScreenX = ( GetDeviceCaps( hdc, HORZRES ) / 2 )
                    - ( ( rectApp.right - rectApp.left ) / 2 );

                int nScreenY = ( GetDeviceCaps( hdc, VERTRES ) / 2 )
                    - ( ( rectApp.bottom - rectApp.top ) / 2 );

                MoveWindow( hWnd,
                            nScreenX,
                            nScreenY,
                            rectApp.right - rectApp.left,
                            rectApp.bottom - rectApp.top,
                            false );

                ReleaseDC( hWnd, hdc );
            }
        }
            break;

        case WM_INITCTRLS:
            SetDlgItemTextW( hWnd,
                             IDC_TEXTSEQUENCE_COMBO,
                             LoadStdStringW( IDS_TEXTSEQUENCE_INIT ).c_str() );

            SendDlgItemMessageA( hWnd, IDC_FILENAME, EM_LIMITTEXT, RECFILE_MAX, 0 );
            SetFocus( GetDlgItem( hWnd, IDC_FILENAME ) );
            // Set the default button for the filename control
            SetWindowLongPtrA( GetDlgItem( hWnd, IDC_FILENAME ),
                               GWLP_USERDATA,
                               IDC_BTN_DELFILE );
            // Set the default button for the text sequence control
            COMBOBOXINFO cbInfo;
            cbInfo.cbSize = sizeof( cbInfo );
            GetComboBoxInfo( GetDlgItem( hWnd, IDC_TEXTSEQUENCE_COMBO ), &cbInfo );
            SetWindowLongPtrA( cbInfo.hwndItem, GWLP_USERDATA, IDC_SEND_TEXTSEQUENCE );
            break;

        case WM_CTLCOLORSTATIC:
            return ( LRESULT )GetSysColorBrush( COLOR_WINDOW );

        case WM_NOTIFY:
        {
            LPNMHDR pnmh = ( LPNMHDR )lParam;
            if( pnmh->idFrom == IDC_TEXTSEQUENCE_FORMAT && pnmh->code == NM_CLICK )
            {
                OpenTextSequenceHelp();
            }
        }
        break;

        case WM_COMMAND:
            // Parsing the menu selections
            switch( LOWORD( wParam ) )
            {
                case IDC_LIST_FILES:
                    if( HIWORD( wParam ) == LBN_SELCHANGE )
                    {
                        int nSel = ( int )SendMessage( ( HWND )lParam,
                                                       LB_GETCURSEL,
                                                       0,
                                                       0 );
                        if( nSel != LB_ERR )
                        {
                            std::vector <char> strFile;
                            int nChars = ( int )SendMessageA( ( HWND )lParam,
                                                              LB_GETTEXTLEN,
                                                              ( WPARAM )nSel,
                                                              0 );
                            assert( nChars > 0 );
                            strFile.resize( nChars + 1 );
                            SendMessageA( ( HWND )lParam,
                                          LB_GETTEXT,
                                          ( WPARAM )nSel,
                                          ( LPARAM )&strFile[0] );

                            SetDlgItemTextA( hWnd, IDC_FILENAME, &strFile[0] );
                        }
                        else
                        {
                            SetDlgItemTextA( hWnd, IDC_FILENAME, "" );
                        }
                    }
                    break;

                case IDC_BTN_DELFILE:
                    g_pXSimClient->DeleteFileA();
                    break;

                case ID_ACC_RECORD_START:
                case IDC_STARTRECORDER:
                    g_pXSimClient->HrStartRecorder();
                    break;

                case ID_ACC_RECORD_STOP:
                case IDC_STOPRECORDER:
                    g_pXSimClient->HrStopRecorder();
                    break;

                case ID_ACC_PLAY_START:
                case IDC_PLAYBACK:
                    g_pXSimClient->HrStartPlayback();
                    break;

                case ID_ACC_PLAY_STOP:
                case IDC_STOPPLAYBACK:
                    g_pXSimClient->HrStopPlayback();
                    break;

                case ID_ACC_RANDOM_START:
                case IDC_START_RANDOMINPUT:
                    g_pXSimClient->HrStartRandomPlayer();
                    break;

                case ID_ACC_RANDOM_STOP:
                case IDC_STOP_RANDOMINPUT:
                    g_pXSimClient->HrStopRandomPlayer();
                    break;

                case ID_ACC_TEXT_SEND:
                case IDC_SEND_TEXTSEQUENCE:
                    g_pXSimClient->HrStartTextSequencePlayer();
                    break;

                case ID_ACC_RETURN_CTL:
                case ID_CONSOLE_RETURNCONTROL:
                    g_pXSimClient->HrReturnControl();
                    break;

                case ID_FILE_EXIT:
                    DestroyWindow( hWnd );
                    break;

                case ID_CONSOLE_DISABLEGUIDEBUTTON:
                {
                    HMENU hMenu = GetMenu( hWnd );
                    UINT uState = GetMenuState( hMenu,
                                                ID_CONSOLE_DISABLEGUIDEBUTTON,
                                                0 );
                    uState = ( uState & MF_CHECKED ) ^ MF_CHECKED;
                    CheckMenuItem( hMenu, ID_CONSOLE_DISABLEGUIDEBUTTON, uState );
                    break;
                }

                case ID_FILE_CLEAROUTPUT:
                    SetDlgItemTextA( hWnd, IDC_OUTPUT, "" );
                    break;

                case ID_HELP_TEXTSEQUENCEHELP:
					OpenTextSequenceHelp();
                    break;

                case ID_FILE_REFRESHFILES:
                    g_pXSimClient->RefreshFiles();
                    break;

                case ID_SYNCMODE_TIME:
                {
                    HMENU hMenu = GetMenu( hWnd );
                    CheckMenuItem( hMenu, ID_SYNCMODE_TIME, MF_CHECKED );
                    CheckMenuItem( hMenu, ID_SYNCMODE_FRAME, 0 );
                    break;
                }

                case ID_SYNCMODE_FRAME:
                {
                    HMENU hMenu = GetMenu( hWnd );
                    CheckMenuItem( hMenu, ID_SYNCMODE_TIME, 0 );
                    CheckMenuItem( hMenu, ID_SYNCMODE_FRAME, MF_CHECKED );
                    break;
                }

                case ID_HELP_QUICKHELP:
                    DialogBoxW( g_hInstance,
                                MAKEINTRESOURCEW( IDD_QUICKHELP ),
                                hWnd,
                                QuickHelpProc );
                    break;

                case ID_HELP_HELP:
                    CHAR EnvironmentValue[MAX_PATH];
                    DWORD   Size;
                    
                    // Get the XDK path
                    Size = GetEnvironmentVariable(TEXT("xedk"), EnvironmentValue, MAX_PATH);
                    if (Size != 0 && (Size * sizeof(TCHAR) < MAX_PATH) 
                        && EnvironmentValue[0] != TEXT('\0'))
                    {
                        size_t StrLen = strlen(EnvironmentValue);
                        char* pStr = EnvironmentValue + StrLen - 1;

                        if (*pStr != TEXT('\\') && StrLen < MAX_PATH - 1)
                        {
                            strcat_s(EnvironmentValue, TEXT("\\"));
                        }
                    }

                    errno_t crterr;
                    crterr = strcat_s(EnvironmentValue,
                        TEXT(XSIM_XDK_HELPFILE));

                    if (crterr == 0 )
                    {
                        STARTUPINFOA si = { 0 };
                        PROCESS_INFORMATION pi = { 0 };
                        si.cb = sizeof(si);

                        char helpCmd[256];
                        sprintf_s(helpCmd, "hh.exe %s", EnvironmentValue);

                        CreateProcessA(NULL, helpCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);

                        return 1;
                    }
                    break;

                default:
                    return DefWindowProc( hWnd, message, wParam, lParam );
            }
            break;

        case WM_DESTROY:
            PostQuitMessage( 0 );
            break;

        default:
            return DefWindowProc( hWnd, message, wParam, lParam );
    }

    return 0;
}
