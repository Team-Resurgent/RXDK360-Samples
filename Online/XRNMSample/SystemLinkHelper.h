//--------------------------------------------------------------------------------------
// SystemLinkHelper.h
//
// Helper class for handling finding sessions using system link
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <xtl.h>
#include <vector>

/*
 * Please note: the purpose of this sample is to demonstrate XRNM. To avoid 
 * cluttering the sample with Xbox Live matchmaking, it supports system link
 * only. XRNM does not at this time support LAN broadcasts, so the discovery
 * portion must remain separate.
 */

class SystemLinkHelper
{
    // Constants
    static const DWORD SEEK_INTERVAL    = 1000;   // send a seek every second
    static const DWORD TIMEOUT_INTERVAL = 5000;   // timeout if no response in five seconds
    static const WORD PORT             = 1001;   // port 1000 will be used by XRNM
    static const UINT NONCE_SIZE       = 8;

public:
    HRESULT BeginSearching();
    HRESULT BeginHosting( const XSESSION_INFO* info, UINT nPlayerCount, LPCWSTR wstrHostGamertag );
    HRESULT End();

    HRESULT Update();
    HRESULT UpdatePlayerCount( UINT nPlayerCount );

    VOID    ClearSessions( VOID );

    struct Session
    {
        WCHAR wstrHostGamerTag[ XUSER_NAME_SIZE ];
        XSESSION_INFO SessionInfo;
        UINT nPlayerCount;
        DWORD dwLastUpdateReceived;
    };

    const std::vector <Session>& GetSessions( VOID )
    {
        return m_Sessions;
    }

            SystemLinkHelper() : m_Mode( IDLE )
            {
            }

private:
    // Data members
    BYTE    m_Nonce[ NONCE_SIZE ];
    DWORD m_dwLastSeek;
    SOCKET m_Socket;
    sockaddr_in m_saBroadcast;
    Session m_SessionInfo;
    std::vector <Session> m_Sessions;

    enum
    {
        IDLE,
        HOSTING,
        SEARCHING
    } m_Mode;

    // Discovery messages
    struct SSeekingMessage
    {
        BOOL bSeeking;
        BYTE m_Nonce[ NONCE_SIZE ];
    };

    struct SSeekReplyMessage : public SSeekingMessage
    {
        Session SessionInfo;
    };

    static const UINT MAX_MESSAGESIZE  = sizeof( SSeekReplyMessage );

    // Internal methods
    HRESULT CreateSocket( VOID );
    HRESULT SendSeek( VOID );
};
