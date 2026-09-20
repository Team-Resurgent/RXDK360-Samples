//-------------------------------------------------------------------------------------
// XSimClient.h
//
// XSimClient.h -- Definition of test object to demonstrate how to use the 
// XSim APIs to communicate with the Xbox360 console over the debug channel
// from a PC application.
//
// Microsoft XNA Game Platform Extensions Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef XSIM_CLIENT_H
#define XSIM_CLIENT_H

#include "resource.h"

static const DWORD                          LOADSTRING_MAX = 100;
static const DWORD                          RECFILE_MAX = 32;
static const DWORD                          XSIM_PORTS_MAX = 4;
static const int                            ID_EVENT_POLL_PLAYBACK = 1;
static const int                            ID_EVENT_TEXTSEQUENCE_PLAYBACK = 2;
static const int                            ID_EVENT_PLAYERSTATUS = 3;


//-------------------------------------------------------------------------------------
// Name: PlayerInfo
//-------------------------------------------------------------------------------------
struct PlayerInfo
{
    PlayerInfo::PlayerInfo( std::string fileIn,
                            XSIMHANDLE handleIn,
                            XSIM_SYNCHMODE syncmodeIn ) : handle( handleIn ),
                                                          syncmode( syncmodeIn )
    {
        file.assign( fileIn );
    }

    std::string file;
    XSIMHANDLE handle;
    XSIM_SYNCHMODE syncmode;
};

typedef std::vector <PlayerInfo*>           XSimPlayerList;
typedef std::map <std::string, XSIMHANDLE>  XSimHandleMap;
typedef std::map <DWORD, std::string>       XSimPortMemory;
typedef XSimPortMemory::iterator            XSimPortMemoryIter;

//-------------------------------------------------------------------------------------
// Name: XSimClient
//-------------------------------------------------------------------------------------
class XSimClient
{
public:
                            XSimClient();
                            ~XSimClient();

    void                    Initialize( HWND hwndOutput, HINSTANCE hInstance );
    void                    Uninitialize();
    void                    Destroy();

    int                     GetSelectedFile( std::string* strFile );
    void                    NewRecordFile( const char* szFile );
    void                    DeleteFile();
    void                    RefreshFiles();
    void                    FreePlayerCache();

    HRESULT                 HrReturnControl();
    HRESULT                 HrStartTextSequencePlayer();
    HRESULT                 HrStartRandomPlayer();
    HRESULT                 HrStopRandomPlayer();
    HRESULT                 HrStartRecorder();
    HRESULT                 HrStopRecorder();
    HRESULT                 HrStartPlayback();
    HRESULT                 HrStopPlayback();

private:

    enum XSIMPRINT_TYPE
    {
        XSIMPRINT_STATECHANGE,
        XSIMPRINT_PLAYBACK,
        XSIMPRINT_STARTUP,
        XSIMPRINT_ERROR,
        XSIMPRINT_NORMAL
    };

    static inline DWORD     PlayerMasks( DWORD index )
    {
        switch( index )
        {
            case 0:
                return XSIM_USERINDEXMASK_0;
            case 1:
                return XSIM_USERINDEXMASK_1;
            case 2:
                return XSIM_USERINDEXMASK_2;
            case 3:
                return XSIM_USERINDEXMASK_3;
            default:
                return 0;
        }
    }

    inline XSIM_SYNCHMODE   GetSyncMode()
    {
        if( GetMenuState( GetMenu( m_hwndDlg ), ID_SYNCMODE_FRAME, 0 ) & MF_CHECKED )
        {
            return XSIM_SYNCHMODE_FRAME;
        }
        else
        {
            return XSIM_SYNCHMODE_TIME;
        }
    }

    inline UINT             GetSyncModeIds()
    {
        switch( GetSyncMode() )
        {
            case XSIM_SYNCHMODE_FRAME:
                return IDS_FRAME_MODE;
            case XSIM_SYNCHMODE_TIME:
                return IDS_TIME_MODE;
        }

        assert( false );
        return 0;
    }

    DWORD                   GetActivePort();

    PlayerInfo* XSimClient::GetPlayerFromCache( std::string file );

    // $BUG: HandleConsoleShutdownEvent is not implemented. Should remove?
    void                    HandleConsoleShutdownEvent();
    void                    ReportError( HRESULT hr );
    void                    ConsoleWindowPrint( XSIMPRINT_TYPE type, UINT uIdMsg, ... );


    HANDLE m_hStatusThread;
    DWORD m_dwThreadId;
    HINSTANCE m_hInstance;
    HWND m_hwndDlg;
    XSIMHANDLE              m_rghRandomInputPlayer[XSIM_PORTS_MAX];
    XSIMHANDLE              m_rghRandomStatePlayer[XSIM_PORTS_MAX];
    XSIMHANDLE              m_rghTextSequencePlayer[XSIM_PORTS_MAX];
    XSimHandleMap m_recorderMap;
    XSimPlayerList m_playerList;
    XSimPortMemory m_portPlayerMemory;
    XSimPortMemory m_portRecorderMemory;
    bool m_fXenonSampleStarted;
};

#endif // XSIM_CLIENT_H
