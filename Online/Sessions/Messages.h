//--------------------------------------------------------------------------------------
// Messages.h
//
// Message classes for sessions sample
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#ifndef MESSAGES_H
#define MESSAGES_H

#include "Voice.h"

//-----------------------------------------------------------------------------
// Message IDs
//
// A "host" is the player who started the session.
// A "client" is a potential player. A client is not currently in a session.
// A "player" is anyone in a session.
//-----------------------------------------------------------------------------
enum
{                       // From     To
    MSG_JOIN_SESSION,   // client   host
    MSG_JOIN_RESPONSE,  // host     client
    MSG_PLAYER_INFO,    // host     player
    MSG_WAVE,           // player   player
    MSG_START_GAME,     // host     client
    MSG_REGISTER,       // host     client
    MSG_REGISTERED,     // client   host
    MSG_SCORE_POINT,    // player   player
    MSG_POINT_TOTAL,    // player   player
    MSG_HEARTBEAT,      // player   player
    MSG_GOODBYE,        // player   player
    MSG_VOICE,          // player   player
    MSG_MUTE,           // player   player
    MSG_MIGRATE,        // host     client
};

//-----------------------------------------------------------------------------
// Maximum packet size
//-----------------------------------------------------------------------------
static const WORD MAX_VDP_PACKET = 1264;

//-----------------------------------------------------------------------------
// Message payloads
//-----------------------------------------------------------------------------
// Pack to minimize network traffic
#pragma pack( push )
#pragma pack( 1 )


//-----------------------------------------------------------------------------
// Sent from a client to a host to join a game
//-----------------------------------------------------------------------------
struct MsgJoinSession
{
    ULONGLONG id;
    BYTE cPlayers;
    WCHAR   strGamertags[ XUSER_MAX_COUNT ][ XUSER_NAME_SIZE ];  // player who wants to join
    BYTE    nController[ XUSER_MAX_COUNT ];
    XUID    xuids[ XUSER_MAX_COUNT ];
    BOOL    bHasVoice[ XUSER_MAX_COUNT ];
    BOOL bInvited;
};


//-----------------------------------------------------------------------------
// Sent from a host to a client to respond to a join request
//-----------------------------------------------------------------------------
struct MsgJoinResponse
{
    enum
    {
        JOINRESPONSE_APPROVED,
        JOINRESPONSE_SESSIONFULL,
        JOINRESPONSE_SESSIONINPROGRESS,
        JOINRESPONSE_NOTHOSTING
    } Response;

    ULONGLONG id;     // Host's machine ID
    ULONGLONG Nonce;  // Session nonce

    DWORD nVictoryPoints;  // Number of points for victory
    DWORD nMap;

    // Info about the host
    BYTE cPlayers;
    WCHAR   strGamertags[ XUSER_MAX_COUNT ][ XUSER_NAME_SIZE ];
    XUID    xuids[ XUSER_MAX_COUNT ];
    BOOL    bHasVoice[ XUSER_MAX_COUNT ];
    UINT cPublicSlots;
    UINT cPrivateSlots;
    DWORD dwSessionFlags;
};


//-----------------------------------------------------------------------------
// Sent from a host to a client to describe the players on a remote client
//-----------------------------------------------------------------------------
struct MsgPlayerInfo
{
    ULONGLONG id;
    XNADDR xnaddr;
    BYTE cPlayers;
    WCHAR   strGamertags[ XUSER_MAX_COUNT ][ XUSER_NAME_SIZE ];  // player who wants to join
    XUID    xuids[ XUSER_MAX_COUNT ];
    BOOL    bHasVoice[ XUSER_MAX_COUNT ];
};


//-----------------------------------------------------------------------------
// Sent from a player to a player to wave
//-----------------------------------------------------------------------------
struct MsgWave
{
    ULONGLONG id;
    XUID xuid;
};


//-----------------------------------------------------------------------------
// Sent from a player to a player to score a point
//-----------------------------------------------------------------------------
struct MsgScorePoint
{
    ULONGLONG id;
    XUID xuid;
};


//-----------------------------------------------------------------------------
// Sent from a player to a player to update the point total for a player
//-----------------------------------------------------------------------------
struct MsgPointTotal
{
    ULONGLONG id;
    XUID xuid;
    DWORD nPoints;
};


//-----------------------------------------------------------------------------
// Sent from a player to a player to transmit voice data
//-----------------------------------------------------------------------------
struct MsgVoice
{
    ULONGLONG id;
    BYTE bData[ MAX_VDP_PACKET ];
    WORD wSize;
};


//-----------------------------------------------------------------------------
// Sent from a player to a player to mute
//-----------------------------------------------------------------------------
struct MsgMute
{
    ULONGLONG id;
    BYTE cPlayers;
    XUID    xuid[ XUSER_MAX_COUNT ];
    BYTE    cMuted[ XUSER_MAX_COUNT ];
    XUID    xuidMuted[ XUSER_MAX_COUNT ][ MAX_REMOTE_TALKERS ];
};


//-----------------------------------------------------------------------------
// Sent from a would-be host to a player
//-----------------------------------------------------------------------------
struct MsgMigrate
{
    enum
    {
        MIGRATE_HOSTING,  // I am now hosting
        MIGRATE_MIGRATED, // You're now my host
        MIGRATE_STANDBY,  // I think someone else should host, hang on
    } msgType;
    XSESSION_INFO info;
};

//--------------------------------------------------------------------------------------
// Name: class CMessage
// Desc: Generic message, owner of all other messages
//--------------------------------------------------------------------------------------
class CMessage
{
    WORD m_cbGameData;
    BYTE m_byMessageID;

    union
    {
        MsgJoinSession m_JoinSession;  // join request message
        MsgJoinResponse m_JoinResponse; // join response message
        MsgPlayerInfo m_PlayerInfo;   // information about a player on a specific box
        MsgWave m_Wave;         // wave message
        MsgScorePoint m_ScorePoint;   // score a point
        MsgPointTotal m_PointTotal;   // update point total
        MsgVoice m_Voice;        // transmit voice data
        MsgMute m_Mute;         // inform a player that they are muted
        MsgMigrate m_Migrate;      // host migration status messages
    };

    // validate message ID
    void        AssertID( BYTE byMessageID )
    {
        assert( m_byMessageID == byMessageID );
        UNREFERENCED_PARAMETER( byMessageID );
    }

public:

    explicit    CMessage( BYTE byMessageID = 0 ) : m_byMessageID( byMessageID )
    {
    }
    BYTE        GetID() const
    {
        return m_byMessageID;
    }

    WORD        GetGameDataSize() const
    {
        WORD ret = sizeof( m_byMessageID );

        switch( m_byMessageID )
        {
            case MSG_JOIN_SESSION:
                ret += sizeof( m_JoinSession );  break;
            case MSG_JOIN_RESPONSE:
                ret += sizeof( m_JoinResponse ); break;
            case MSG_PLAYER_INFO:
                ret += sizeof( m_PlayerInfo );   break;
            case MSG_START_GAME:    /* no data */
                break;
            case MSG_REGISTER:      /* no data */
                break;
            case MSG_REGISTERED:    /* no data */
                break;
            case MSG_WAVE:
                ret += sizeof( m_Wave );         break;
            case MSG_SCORE_POINT:
                ret += sizeof( m_ScorePoint );   break;
            case MSG_POINT_TOTAL:
                ret += sizeof( m_PointTotal );   break;
            case MSG_HEARTBEAT:     /* no data */
                break;
            case MSG_GOODBYE:       /* no data */
                break;
            case MSG_VOICE:
                ret += sizeof( ULONGLONG );      break;
            case MSG_MUTE:
                ret += sizeof( m_Mute );         break;
            case MSG_MIGRATE:
                ret += sizeof( MsgMigrate );     break;
        }

        return ret;
    }

    // retrieve specific messages
    MsgJoinSession& GetJoinSession()
    {
        AssertID( MSG_JOIN_SESSION );  return m_JoinSession;
    }
    MsgJoinResponse& GetJoinResponse()
    {
        AssertID( MSG_JOIN_RESPONSE ); return m_JoinResponse;
    }
    MsgPlayerInfo& GetPlayerInfo()
    {
        AssertID( MSG_PLAYER_INFO );   return m_PlayerInfo;
    }
    MsgWave& GetWave()
    {
        AssertID( MSG_WAVE );          return m_Wave;
    }
    MsgScorePoint& GetScorePoint()
    {
        AssertID( MSG_SCORE_POINT );   return m_ScorePoint;
    }
    MsgPointTotal& GetPointTotal()
    {
        AssertID( MSG_POINT_TOTAL );   return m_PointTotal;
    }
    MsgVoice& GetVoice()
    {
        AssertID( MSG_VOICE );         return m_Voice;
    }
    MsgMute& GetMute()
    {
        AssertID( MSG_MUTE );          return m_Mute;
    }
    MsgMigrate& GetMigrate()
    {
        AssertID( MSG_MIGRATE );       return m_Migrate;
    }

    WORD        GetSize()
    {
        m_cbGameData = GetGameDataSize();

        WORD wRet = sizeof( m_cbGameData ) + m_cbGameData;

        if( m_byMessageID == MSG_VOICE )
        {
            wRet += m_Voice.wSize & MAXWORD;
        }

        return wRet;
    }
};

#pragma pack( pop )

#endif MESSAGES_H
