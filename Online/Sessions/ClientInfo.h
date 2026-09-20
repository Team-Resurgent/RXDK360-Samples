//--------------------------------------------------------------------------------------
// ClientInfo.h
//
// Definition of ClientInfo struct
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#ifndef CLIENTINFO_H
#define CLIENTINFO_H

#pragma warning(disable:4127)  // conditional expression is constant
#include <list>
#pragma warning(default:4127)
#include <set>


//--------------------------------------------------------------------------------------
// Name: struct ClientInfo
// Desc: Information about a local/remote client
//--------------------------------------------------------------------------------------
struct ClientInfo
{
    ULONGLONG id;                           // Machine ID
    IN_ADDR addr;                         // IP address
    XNADDR xnaddr;                       // XNADDR
    BYTE cPlayers;                     // number of players on the client
    WCHAR   strGamertags[ XUSER_MAX_COUNT ][ XUSER_NAME_SIZE ];  // gamertags
    XUID    xuids[ XUSER_MAX_COUNT ];     // XUIDs
    BOOL    bHasVoice[ XUSER_MAX_COUNT ]; // who has voice permission
    DOUBLE  dMu[ XUSER_MAX_COUNT ];       // mean skill of the players
    DOUBLE  dSigma[ XUSER_MAX_COUNT ];    // skill standard deviation of the players
    UINT    nPoints[ XUSER_MAX_COUNT ];   // number of points scored
    DWORD dwHeartbeat;                  // last heartbeat received
    DWORD   dwWave[ XUSER_MAX_COUNT ];    // last wave received
    BOOL bInvited;                     // should the client consume private slots
    BOOL bRegistered;                  // has the peer registered for arbitration
    DWORD   nController[ XUSER_MAX_COUNT ]; // which controller is this player on?

    BOOL bWantsToMigrate;              // keep track of who's trying to migrate to us

            ClientInfo()
            {
                Clear();
            }
    VOID    Clear()
    {
        ZeroMemory( this, sizeof( ClientInfo ) );
    }
};


//--------------------------------------------------------------------------------------
// Name: struct ClientInfo
// Desc: Include information about who is muted for these clients
//--------------------------------------------------------------------------------------
typedef std::set <XUID>         XUIDSet;
struct LocalClientInfo : public ClientInfo
{
    XUIDSet muteXUIDs[ XUSER_MAX_COUNT ];   // set of users not to send voice/text to (XN_SYS_MUTELISTCHANGED)

    BOOL    IsMuted( XUID player, UINT& i )
    {
        for( i = 0; i < cPlayers; ++i )
        {
            if( muteXUIDs[ i ].find( player ) != muteXUIDs[ i ].end() )
                return true;
        }
        return false;
    }
    BOOL    IsMutedFor( UINT i, XUID player ) const
    {
        if( muteXUIDs[ i ].find( player ) != muteXUIDs[ i ].end() )
            return true;
        return false;
    }
};

typedef std::list <ClientInfo>  ClientInfoVec;

#endif CLIENTINFO_H
