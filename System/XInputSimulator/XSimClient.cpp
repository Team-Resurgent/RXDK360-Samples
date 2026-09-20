//-------------------------------------------------------------------------------------
// XSimClient.cpp
//
// XSimClient contains the code that actually use the XSimRemote APIs.  A global
// XSimClient object is instantiated on application startup, and destroyed on 
// application shutdown.  For more information on the XSim Remote API methods and types
// please consult the XSim documentation and the header files.
//
// Microsoft XNA Game Platform Extensions Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "stdafx.h"

#include "XSimClient.h"

#include "Resource.h"

#define CHECK(exp)  \
{ HRESULT _hr; if( FAILED( _hr = exp ) ) { ReportError(_hr); return _hr; } }

static const DWORD  FRAMES_PER_SEC = 60;
static const CHAR*  FILE_ROOT = "devkit:\\";
static const CHAR*  FILE_EXT = ".xsim";

struct
{
    int idsCmd;
    int idsCmdDescription;
}
    TEXTSEQUENCE_CMDS[] =
{
    { IDS_TEXTSEQUENCE0_CMD, IDS_TEXTSEQUENCE0_DESC },
    { IDS_TEXTSEQUENCE1_CMD, IDS_TEXTSEQUENCE1_DESC },
    { IDS_TEXTSEQUENCE2_CMD, IDS_TEXTSEQUENCE2_DESC }
};

std::wstring LoadStdStringW( UINT uID );

//-------------------------------------------------------------------------------------
// Name: XSimClient
//-------------------------------------------------------------------------------------
XSimClient::XSimClient()
{
    m_fXenonSampleStarted = false;
    m_hStatusThread = NULL;
    m_hInstance = NULL;
    m_hwndDlg = NULL;
}

//-------------------------------------------------------------------------------------
// Name: ~XSimClient
//-------------------------------------------------------------------------------------
XSimClient::~XSimClient()
{
    Destroy();
}

//-------------------------------------------------------------------------------------
// Name: Destroy
// Desc: If the status update thread is still running, wait for it to shut itself down,
//       but ultimately kill the thread if its hung or taking unreasonably long.
//-------------------------------------------------------------------------------------
void XSimClient::Destroy()
{
    FreePlayerCache();

    Uninitialize();

    // $BUG: Stop and/or kill m_hStatusThread if still running

    m_fXenonSampleStarted = false;
    m_hStatusThread = NULL;
    m_hInstance = NULL;
    m_hwndDlg = NULL;
}

//-------------------------------------------------------------------------------------
// Name: Uninitialize
//-------------------------------------------------------------------------------------
void XSimClient::Uninitialize()
{
    XSimUninitialize();
}

//-------------------------------------------------------------------------------------
// Name: GetActivePort
//-------------------------------------------------------------------------------------
DWORD XSimClient::GetActivePort()
{
    return ( DWORD )SendDlgItemMessage( m_hwndDlg,
                                        IDC_LIST_PORTS,
                                        LB_GETCURSEL,
                                        0,
                                        0 );
}

//-------------------------------------------------------------------------------------
// Name: GetSelectedFile
//-------------------------------------------------------------------------------------
int XSimClient::GetSelectedFile( std::string* strFile )
{
    if( strFile == NULL )
    {
        return -1;
    }

    strFile->clear();

    char strFilename[RECFILE_MAX + 1];

    UINT uChars = GetDlgItemTextA( m_hwndDlg,
                                   IDC_FILENAME,
                                   strFilename,
                                   RECFILE_MAX + 1 );
    if( uChars == 0 )
    {
        return -1;
    }

    _strlwr_s( strFilename, _countof( strFilename ) );
    strFile->append( strFilename );

    if( strstr( strFile->c_str(), FILE_EXT ) == NULL )
    {
        strFile->append( FILE_EXT );
    }

    int nSelected = ( int )SendDlgItemMessageA( m_hwndDlg,
                                                IDC_LIST_FILES,
                                                LB_FINDSTRINGEXACT,
                                                ( WPARAM )-1,
                                                ( LPARAM )strFile->c_str() );
    if( nSelected == LB_ERR )
    {
        nSelected = -1;
    }

    strFile->insert( 0, FILE_ROOT );

    return nSelected;
}

//-------------------------------------------------------------------------------------
// Name: DeleteFile
//-------------------------------------------------------------------------------------
void XSimClient::DeleteFile()
{
    std::string strFile;
    int nSelected = GetSelectedFile( &strFile );

    if( nSelected < 0 )
    {
        ConsoleWindowPrint( XSIMPRINT_ERROR, IDS_OUT_DELETE_ERROR_1 );
    }
    else
    {
        DM_FILE_ATTRIBUTES dmFileAttributes;
        HRESULT hr = DmGetFileAttributes( strFile.c_str(), &dmFileAttributes );
        if( FAILED( hr ) )
        {
            // Assume the file does not exist in the console
            ConsoleWindowPrint( XSIMPRINT_NORMAL,
                                IDS_OUT_DELETE_ERROR_2,
                                strFile.c_str() );
        }
        else if( FAILED( hr = DmDeleteFile( strFile.c_str(), false ) ) )
        {
            ConsoleWindowPrint( XSIMPRINT_ERROR,
                                IDS_OUT_DELETE_ERROR_3,
                                strFile.c_str(),
                                hr );
            return;
        }
        else
        {
            ConsoleWindowPrint( XSIMPRINT_NORMAL,
                                IDS_OUT_DELETE_ERROR_4,
                                strFile.c_str() );
        }

        SendDlgItemMessage( m_hwndDlg,
                            IDC_LIST_FILES,
                            LB_DELETESTRING,
                            nSelected,
                            0 );
    }
}

//-------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Set default values to UI elements, attempt to start up the console's 
//       XSimTestSample application to communicate with, and start the status update
//       timer.
//-------------------------------------------------------------------------------------
void XSimClient::Initialize( HWND hwndDlg, HINSTANCE hInstance )
{
    assert( hInstance );
    assert( hwndDlg );

    m_hInstance = hInstance;
    m_hwndDlg = hwndDlg;

    std::vector <char> strCommand;
    for( int iCmd = 0; iCmd < _countof( TEXTSEQUENCE_CMDS ); iCmd++ )
    {
        char strCommand[LOADSTRING_MAX];
        int ids = TEXTSEQUENCE_CMDS[iCmd].idsCmdDescription;
        int nChars = LoadStringA( hInstance, ids, strCommand, LOADSTRING_MAX );
        assert( nChars > 0 );
        if( nChars > 0 )
        {
            SendDlgItemMessage( m_hwndDlg,
                                IDC_TEXTSEQUENCE_COMBO,
                                CB_ADDSTRING,
                                0,
                                ( LPARAM )strCommand );
        }
    }

    std::wstring wstrController = LoadStdStringW( IDS_CONTROLLER );
    for( int iPort = 0; iPort < XSIM_PORTS_MAX; iPort++ )
    {
        wchar_t wstrPort[24];
        swprintf_s( wstrPort, wstrController.c_str(), iPort );
        SendDlgItemMessageW( m_hwndDlg,
                             IDC_LIST_PORTS,
                             LB_ADDSTRING,
                             iPort,
                             ( LPARAM )wstrPort );

        m_rghRandomInputPlayer[iPort] = NULL;
        m_rghRandomStatePlayer[iPort] = NULL;
        m_rghTextSequencePlayer[iPort] = NULL;
    }

    SendDlgItemMessage( m_hwndDlg, IDC_LIST_PORTS, LB_SETCURSEL, 0, 0 );

    EnableWindow( GetDlgItem( m_hwndDlg, IDC_STARTRECORDER ), true );
    EnableWindow( GetDlgItem( m_hwndDlg, IDC_STOPRECORDER ), true );
    EnableWindow( GetDlgItem( m_hwndDlg, IDC_PLAYBACK ), true );
    EnableWindow( GetDlgItem( m_hwndDlg, IDC_START_RANDOMINPUT ), true );
    EnableWindow( GetDlgItem( m_hwndDlg, IDC_STOP_RANDOMINPUT ), true );
    EnableWindow( GetDlgItem( m_hwndDlg, IDC_SEND_TEXTSEQUENCE ), true );

    ConsoleWindowPrint( XSIMPRINT_NORMAL, IDS_OUT_OPEN_CONNECTION );

    // Accept default record/playback frame rate as opposed to passing one in (60 fps)
    HRESULT hr = XSimInitialize( NULL );

    if( FAILED( hr ) )
    {
        ConsoleWindowPrint( XSIMPRINT_NORMAL, IDS_OUT_CONNECTION_FAILED );
        if( XBDM_CANNOTCONNECT == hr )
        {
            ConsoleWindowPrint(
                XSIMPRINT_ERROR,
                IDS_OUT_CONNECTION_XBDM_ERROR );
        }
        else
        {
            ConsoleWindowPrint(
                XSIMPRINT_ERROR,
                IDS_OUT_CONNECTION_XSIM_ERROR, hr );
        }
    }
    else
    {
        RefreshFiles();
        ConsoleWindowPrint( XSIMPRINT_NORMAL, IDS_OUT_CONNECTION_OK );
    }
}

void XSimClient::RefreshFiles()
{
    SendDlgItemMessageA( m_hwndDlg, IDC_LIST_FILES, LB_RESETCONTENT, 0, 0 );

    HRESULT hr;
    PDM_WALK_DIR pWalkDir = NULL;
    do
    {
        DM_FILE_ATTRIBUTES fileAttr;
        hr = DmWalkDir( &pWalkDir, "E:\\", &fileAttr );
        if( hr == XBDM_NOERR )
        {
            std::string stSuffix = fileAttr.Name;
            std::string::size_type nFound = stSuffix.find( ".xsim" );
            if( nFound != std::string::npos )
            {
                NewRecordFile( fileAttr.Name );
            }
        }
    } while( hr == XBDM_NOERR );

    DmCloseDir( pWalkDir );

}

//--------------------------------------------------------------------------------------
// Name: ReportError
// Desc: Send Error text to the output rich edit control.
//--------------------------------------------------------------------------------------
void XSimClient::ReportError( HRESULT hr )
{
    assert( m_hInstance );

    int idsError = hr - XSIM_E_FIRST_ERROR + IDS_E_GENERIC;
    std::wstring wstrError = LoadStdStringW( idsError );

    if( wstrError.length() > 0 )
    {
        ConsoleWindowPrint( XSIMPRINT_ERROR, IDS_OUT_ERROR_STRING, wstrError.c_str() );
    }
    else
    {
        ConsoleWindowPrint( XSIMPRINT_ERROR, IDS_OUT_ERROR_CODE, hr );
    }

    // when an error happens, go back to a good known state, but only if the error 
    // was not connection related to begin with, otherwise we'll just hang longer
    // Also don't shut running players or recorders if an "in use" type error comes
    // along.
    if( hr != XSIM_E_CANNOTACCESS && hr != XSIM_E_NO_CONNECTION &&
        hr != XSIM_E_RECORDER_IN_USE && hr != XSIM_E_PLAYER_IN_USE )
    {
        HrReturnControl();
    }
}

//-------------------------------------------------------------------------------------
// Name: HrStartRandomPlayer
// Desc: Start XSim's RandomInputPlayer (the 'monkey')
//-------------------------------------------------------------------------------------
HRESULT XSimClient::HrStartRandomPlayer()
{
    DWORD dwPort = GetActivePort();
    XSIM_COMPONENTSTATUS status;
    if( SUCCEEDED( XSimGetPortPlayerStatus( dwPort, &status ) ) )
    {
        if( status == XSIM_COMPONENTSTATUS_RUNNING )
        {
            XSimStopPlayer( dwPort );
        }
    }

    CHECK( XSimAcquireControl( PlayerMasks( dwPort ) ) );

    if( m_rghRandomInputPlayer[dwPort] != NULL )
    {
        XSimCloseHandle( m_rghRandomInputPlayer[dwPort] );
        m_rghRandomInputPlayer[dwPort] = NULL;
    }

    CHECK( XSimCreateRandomInputPlayer( &m_rghRandomInputPlayer[dwPort]) );

    // For the purposes of this sample, we disable the left trigger - a common button
    // that could cause the random input player to stop.

    // NOTE This method can be called once in XSimClient::Initialize. 
    // We do it here every time the random input player is started so that 
    // it appears more "in context".    

    XSIM_CONTROLLERPRESSATTRIBUTES pressAttr;
    CHECK( XSimGetRandomInputPlayerPressAttributes( m_rghRandomInputPlayer[dwPort],
     &pressAttr ) );

    // disable the left trigger by setting all of its press attributes to 0 
    memset( &pressAttr.LeftTrigger, 0, sizeof( XSIM_BUTTONPRESSATTRIBUTES ) );

    // disable the start button
    memset( &pressAttr.Start, 0, sizeof( XSIM_BUTTONPRESSATTRIBUTES ) );

    // disable the back button
    memset( &pressAttr.Back, 0, sizeof( XSIM_BUTTONPRESSATTRIBUTES ) );

    UINT uState = GetMenuState( GetMenu( m_hwndDlg ),
                                ID_CONSOLE_DISABLEGUIDEBUTTON,
                                0 );

    if( uState & MF_CHECKED )
    {
        // disable the guide (360) button by setting all of its press attributes to 0
        memset( &pressAttr.XBox360Button, 0, sizeof( XSIM_BUTTONPRESSATTRIBUTES ) );
    }
    else
    {
        // NOTE The guide button is a bit "special" in that it doesn't always come
        // up/close down right away so more conservative values of 12000, 1000 are
        // recommended.
        pressAttr.XBox360Button.dwIntervalMinMs = 12000;
        pressAttr.XBox360Button.dwIntervalMaxMs = 12000;
        pressAttr.XBox360Button.dwHoldTimeMinMs = 1000;
        pressAttr.XBox360Button.dwHoldTimeMaxMs = 1000;
    }

    CHECK( XSimSetRandomInputPlayerPressAttributes( m_rghRandomInputPlayer[dwPort],
     &pressAttr ) );

    CHECK( XSimStartPlayer( m_rghRandomInputPlayer[dwPort], dwPort ) );

    ConsoleWindowPrint( XSIMPRINT_NORMAL, IDS_OUT_RANDOM_START );

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: HrStopRandomPlayer
// Desc: Stop XSim's RandomInputPlayer (the 'monkey')
//-------------------------------------------------------------------------------------
HRESULT XSimClient::HrStopRandomPlayer()
{
    DWORD dwActivePort = GetActivePort();

    CHECK( XSimStopPlayer( dwActivePort ) );
    CHECK( XSimReturnControl( PlayerMasks( dwActivePort ) ) );

    ConsoleWindowPrint( XSIMPRINT_NORMAL, IDS_OUT_RANDOM_STOP );

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: NewRecordFile
// Desc: Adds a file to the list in the UI
//-------------------------------------------------------------------------------------
void XSimClient::NewRecordFile( const char* strFile )
{
    const char* strFilename;

    if( strstr( strFile, FILE_ROOT ) != NULL )
    {
        strFilename = strFile + strlen( FILE_ROOT );
    }
    else
    {
        strFilename = strFile;
    }

    SendDlgItemMessage( m_hwndDlg,
                        IDC_LIST_FILES,
                        LB_ADDSTRING,
                        0,
                        ( LPARAM )strFilename );
}

//-------------------------------------------------------------------------------------
// Name: HrStartRecorder
// Desc: Use XSim's FileRecorder to "record" all console controller activity.  Stop 
//       with the XSimStopFileRecorder API (called from HrStopRecorder).
//-------------------------------------------------------------------------------------
HRESULT XSimClient::HrStartRecorder()
{
    std::string strFile;
    int nSel = GetSelectedFile( &strFile );
    if( strFile.length() == 0 )
    {
        ConsoleWindowPrint( XSIMPRINT_ERROR, IDS_OUT_SELECT_FILE );
        return E_FAIL;
    }

    XSIMHANDLE xsimHandle = m_recorderMap[strFile];
    if( xsimHandle == NULL )
    {
        CHECK( XSimCreateFileRecorder( strFile.c_str(), &xsimHandle ) );
        m_recorderMap[strFile] = xsimHandle;
    }

    // remember the file that the port was recording
    DWORD dwPort = GetActivePort();
    m_portRecorderMemory[dwPort] = strFile;

    CHECK( XSimStartRecorder( xsimHandle, dwPort ) );

    if( nSel < 0 )
    {
        NewRecordFile( strFile.c_str() );
    }

    ShowWindow( GetDlgItem( m_hwndDlg, IDC_RECORDER_STATE ), SW_SHOW );
    ConsoleWindowPrint( XSIMPRINT_NORMAL, IDS_OUT_RECORDER_START );

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: HrStopRecorder
// Desc: Stop the FileRecorder player.  Any further controller events after calling 
//       this method will not be recorded by the FileRecorder.
//-------------------------------------------------------------------------------------
HRESULT XSimClient::HrStopRecorder()
{
    DWORD dwPort = GetActivePort();

    CHECK( XSimStopRecorder( dwPort ) );
    m_portRecorderMemory.erase( dwPort );

    if( m_portRecorderMemory.size() == 0 )
    {
        ShowWindow( GetDlgItem( m_hwndDlg, IDC_RECORDER_STATE ), SW_HIDE );
    }

    ConsoleWindowPrint( XSIMPRINT_NORMAL, IDS_OUT_RECORDER_STOP );

    return S_OK;
}

void XSimClient::FreePlayerCache()
{
    while( !m_playerList.empty() )
    {
        PlayerInfo* pInfo = m_playerList[m_playerList.size() - 1];
        m_playerList.pop_back();
        XSimCloseHandle( pInfo->handle );
        delete pInfo;
    }
}

PlayerInfo* XSimClient::GetPlayerFromCache( std::string file )
{
    PlayerInfo* pInfo = NULL;

    XSIM_SYNCHMODE syncmode = GetSyncMode(); // what's the current sync mode?

    XSimPlayerList::iterator i;
    for( i = m_playerList.begin(); i != m_playerList.end(); ++i )
    {
        if( 0 == ( *i )->file.compare( file ) && syncmode == ( *i )->syncmode )
        {
            pInfo = ( *i );
            break;
        }
    }

    return pInfo;
}

//-------------------------------------------------------------------------------------
// Name: HrStartPlayback
// Desc: Use XSim's FilePlayer to "play back" a previously recorded input session
//       obtained from XSim's FileRecorder player.  
//-------------------------------------------------------------------------------------
HRESULT XSimClient::HrStartPlayback()
{
    std::string strFile;
    if( GetSelectedFile( &strFile ) < 0 )
    {
        ConsoleWindowPrint( XSIMPRINT_ERROR, IDS_OUT_SELECT_FILE );
        return E_FAIL;
    }

    PlayerInfo* pPlayerInfo = GetPlayerFromCache( strFile );
    if( NULL == pPlayerInfo )
    {
        XSIMHANDLE xsimHandle = NULL;

        CHECK( XSimCreateFilePlayer( strFile.c_str(),
                                                     GetSyncMode(),
         &xsimHandle ) );

        pPlayerInfo = new PlayerInfo( strFile, xsimHandle, GetSyncMode() );
        assert( pPlayerInfo );
        m_playerList.push_back( pPlayerInfo );
    }

    // Remember the file that the port was playing
    DWORD dwPort = GetActivePort();
    m_portPlayerMemory[dwPort] = strFile;

    XSimAcquireControl( PlayerMasks( dwPort ) );

    CHECK( XSimStartPlayer( pPlayerInfo->handle, dwPort ) );

    ConsoleWindowPrint( XSIMPRINT_PLAYBACK,
                        IDS_OUT_PLAYBACK_START,
                        LoadStdStringW( GetSyncModeIds() ).c_str() );

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Name: HrStopPlayback
// Desc: Stop the FileRecorder player.  Any further controller events after calling 
//       this method will not be recorded by the FileRecorder.
//-------------------------------------------------------------------------------------
HRESULT XSimClient::HrStopPlayback()
{
    DWORD dwPort = GetActivePort();
    HRESULT hr = XSimStopPlayer( dwPort );
    CHECK( hr );

    if( hr == XSIM_S_ALREADY_STOPPED )
    {
        ConsoleWindowPrint( XSIMPRINT_PLAYBACK, IDS_OUT_PLAYBACK_ALREADY_STOPPED );
    }
    else
    {
        ConsoleWindowPrint( XSIMPRINT_PLAYBACK, IDS_OUT_PLAYBACK_STOP );
    }

    XSimReturnControl( PlayerMasks( dwPort ) );

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: HrReturnControl
// Desc: Start the PortsPlayer, which means "give the console direct input control".
//-------------------------------------------------------------------------------------
HRESULT XSimClient::HrReturnControl()
{
    // explicitly ignore return code, we might not have bound all the ports
    XSimReturnControl( XSIM_USERINDEXMASK_ALL );
    ConsoleWindowPrint( XSIMPRINT_STATECHANGE, IDS_OUT_RETURN_CONTROL );

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: HrStartTextSequencePlayer
// Desc: Prime the TextSequencePlayer with the pre-canned selection from the 
//       command listbox and then run the TextSequencePlayer.
//-------------------------------------------------------------------------------------
HRESULT XSimClient::HrStartTextSequencePlayer()
{
    DWORD dwPort = GetActivePort();
    HRESULT hr = XSimAcquireControl( PlayerMasks( dwPort ) );
    if( FAILED( hr ) && hr != XSIM_E_ALREADY_ACQUIRED )
    {
        ReportError( hr );
        return hr;
    }

    int nSel = ( int )SendDlgItemMessage( m_hwndDlg,
                                          IDC_TEXTSEQUENCE_COMBO,
                                          CB_GETCURSEL,
                                          0,
                                          0 );

    int nChars;
    std::vector <char> strCmd;
    if( nSel == CB_ERR )
    {
        nChars = ( int )SendDlgItemMessageA( m_hwndDlg,
                                             IDC_TEXTSEQUENCE_COMBO,
                                             WM_GETTEXTLENGTH,
                                             0,
                                             0 );

        if( nChars <= 0 )
        {
            ConsoleWindowPrint( XSIMPRINT_ERROR, IDS_OUT_SELECT_SEQUENCE );
            return E_FAIL;
        }

        strCmd.resize( nChars + 1 );

        SendDlgItemMessageA( m_hwndDlg,
                             IDC_TEXTSEQUENCE_COMBO,
                             WM_GETTEXT,
                             nChars + 1,
                             ( LPARAM )&strCmd[0] );
    }
    else
    {
        assert( nSel < _countof( TEXTSEQUENCE_CMDS ) );

        char strLoaded[LOADSTRING_MAX];
        nChars = ( int )LoadStringA( m_hInstance,
                                     TEXTSEQUENCE_CMDS[nSel].idsCmd,
                                     strLoaded,
                                     LOADSTRING_MAX );
        assert( nChars > 0 );
        strCmd.resize( nChars + 1 );
        strcpy_s( &strCmd[0], strCmd.size(), strLoaded );
    }

    ConsoleWindowPrint( XSIMPRINT_NORMAL,
                        IDS_OUT_SEQUENCE_START,
                        dwPort,
                        LoadStdStringW( GetSyncModeIds() ).c_str(),
                        &strCmd[0] );

    XSIM_COMPONENTSTATUS status;
    if( SUCCEEDED( XSimGetPortPlayerStatus( dwPort, &status ) ) )
    {
        if( status == XSIM_COMPONENTSTATUS_RUNNING )
        {
            XSimStopPlayer( dwPort );
        }
    }

    if( m_rghTextSequencePlayer[dwPort] != NULL )
    {
        XSimCloseHandle( m_rghTextSequencePlayer[dwPort] );
        m_rghTextSequencePlayer[dwPort] = NULL;
    }

    CHECK( XSimCreateTextSequencePlayer( &strCmd[0],
                                                 GetSyncMode(),
                                                 FRAMES_PER_SEC,
     &m_rghTextSequencePlayer[dwPort] ) );

    CHECK( XSimStartPlayer( m_rghTextSequencePlayer[dwPort], dwPort ) );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: ConsoleWindowPrint
// Desc: Writes out a string directly to the console output window
//--------------------------------------------------------------------------------------
void XSimClient::ConsoleWindowPrint( XSIMPRINT_TYPE type, UINT uIdMsg, ... )
{
    va_list args;
    va_start( args, uIdMsg );

    std::wstring wstrMsg = LoadStdStringW( uIdMsg );

    std::vector <wchar_t> strFormatted;
    int nChars = _vscwprintf( wstrMsg.c_str(), args );
    strFormatted.resize( nChars + 1 );
    vswprintf_s( &strFormatted[0], strFormatted.size(), wstrMsg.c_str(), args );

    CHARFORMAT cf = {0};
    cf.cbSize = sizeof( cf );
    cf.dwMask = CFM_COLOR;

    switch( type )
    {
        case XSIMPRINT_STATECHANGE:
            cf.crTextColor = RGB( 0, 0, 200 ); break;
        case XSIMPRINT_PLAYBACK:
            cf.crTextColor = RGB( 0, 150, 0 ); break;
        case XSIMPRINT_STARTUP:
            cf.crTextColor = RGB( 0, 255, 0 ); break;
        case XSIMPRINT_ERROR:
            cf.crTextColor = RGB( 255, 0, 0 ); break;
        default:
            cf.crTextColor = CLR_INVALID;
    }

    GETTEXTLENGTHEX getTextEx;
    getTextEx.flags = GTL_DEFAULT;
    getTextEx.codepage = CP_ACP;

    nChars = ( int )SendDlgItemMessageW( m_hwndDlg,
                                         IDC_OUTPUT,
                                         EM_GETTEXTLENGTHEX,
                                         ( WPARAM )&getTextEx,
                                         0 );

    SendDlgItemMessageA( m_hwndDlg,
                         IDC_OUTPUT,
                         EM_SETSEL,
                         ( WPARAM )nChars,
                         nChars );

    SendDlgItemMessageA( m_hwndDlg,
                         IDC_OUTPUT,
                         EM_SETCHARFORMAT,
                         SCF_SELECTION,
                         ( LPARAM )&cf );

    SendDlgItemMessageW( m_hwndDlg,
                         IDC_OUTPUT,
                         EM_REPLACESEL,
                         0,
                         ( LPARAM )&strFormatted[0] );

    SendDlgItemMessageA( m_hwndDlg,
                         IDC_OUTPUT,
                         WM_VSCROLL,
                         SB_BOTTOM,
                         0L );
}
