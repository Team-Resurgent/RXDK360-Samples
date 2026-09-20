//-----------------------------------------------------------------------------
// File: Voice.h
//
// Desc: Voice class and type definitions for Sessions sample
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#pragma once

#include <xaudio2.h>
#include <xhv2.h>

// forward declaration
struct ClientInfo;
struct LocalClientInfo;
struct MsgMute;

static const DWORD MAX_REMOTE_TALKERS = 8;


//--------------------------------------------------------------------------------------
// Name: class CVoice
// Desc: Manage voice communication for the sample
//--------------------------------------------------------------------------------------
class CVoice
{
    // constants
    static const DWORD MAX_VOICE_BUFFER_TIME = 200;  // 200ms
private:
    // XHV object
    PIXHV2ENGINE m_pXHV;

    // Voice permissions array
    BOOL    m_bHasVoice[ XUSER_MAX_COUNT ];

    // Loopback array
    BOOL    m_bLoopback[ XUSER_MAX_COUNT ];

    // Local chat data
    static const WORD m_ChatBufferSize =
        XHV_VOICECHAT_MODE_PACKET_SIZE * XHV_MAX_VOICECHAT_PACKETS;
    BYTE    m_ChatBuffer[ XUSER_MAX_COUNT ][ m_ChatBufferSize ];
    WORD    m_wLocalDataSize[ XUSER_MAX_COUNT ];

    // Last voice data sent
    DWORD m_dwLastVoiceSend;

    // Mutelist changed
    BOOL m_bMutelistUpdated;

    const LocalClientInfo* m_pLocal;

public:
    // ctor/dtor
            CVoice();
            ~CVoice();

    // Initialization/termination
    VOID    Initialize( LocalClientInfo* local );
    VOID    EndSession( void );

    // Process a change in local users
    VOID    LocalUsersChanged( void );

    // Process a change in remote users
    VOID    RegisterClient( ClientInfo* pClient );
    VOID    UnregisterClient( ClientInfo* pClient );

    // Get local communicator status
    VOID    GetLocalVoice( BOOL* pHasVoice );

    // Loopback mode
    VOID    ToggleLoopbackMode( UINT nController );
    BOOL    IsLoopbackModeActive( UINT nController )
    {
        return m_bLoopback[ nController ];
    }

    // Test for talking
    BOOL    IsLocalTalking( UINT nController )
    {
        return m_pXHV->IsLocalTalking( nController );
    }
    BOOL    IsRemoteTalking( XUID xuid )
    {
        return m_pXHV->IsRemoteTalking( xuid );
    }

    // Handle voice tasks
    BOOL    ProcessVoice();

    // Retrieve outgoing voice data
    WORD    GetVoiceData( BYTE* pBuffer, WORD wBufferSize );

    // Submit incoming voice data
    VOID    SubmitVoiceData( BYTE* pBuffer, WORD wBufferSize, ClientInfo* pClient );

    // Reevaluate mutelists
    VOID    ProcessMutelists( MsgMute* msg );

    // Reciprocally mute a player, where necessary
    VOID    ProcessMute( UINT nController, XUID xuid );
};
