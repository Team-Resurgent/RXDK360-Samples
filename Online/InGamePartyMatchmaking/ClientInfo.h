//--------------------------------------------------------------------------------------
// ClientInfo.h
//
// Definition of ClientInfo struct
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#pragma warning(disable:4127)  // conditional expression is constant
#include <list>
#include <vector>
#pragma warning(default:4127)
#include <set>

struct ClientInfo; // Forward declaration

//--------------------------------------------------------------------------------------
// Name: struct SessionData
// Desc: Per-session data for each session a local/remote client is in
//--------------------------------------------------------------------------------------
struct SessionData
{
    friend ClientInfo;

    VOID Set( const XNKID& id, const IN_ADDR& inaddrHost, const IN_ADDR& inaddrClient );
    VOID Clear();
    BOOL operator==( const XNKID& ref ) const;

private:
    XNKID   m_SessionID;
    IN_ADDR m_HostInAddr;
    IN_ADDR m_ClientInAddr;
};

#define MAX_SESSIONS_PER_CLIENT 10

//--------------------------------------------------------------------------------------
// Name: struct ClientInfo
// Desc: Information about a local/remote client
//--------------------------------------------------------------------------------------
struct ClientInfo
{
    ULONGLONG       id;                                          // Machine ID
    IN_ADDR         addr;                                        // IP address
    XNADDR          xnaddr;                                      // XNADDR
    UINT            cPlayers;                                    // number of players on the client
    WCHAR           strGamertags[ MAX_USER_COUNT ][ XUSER_NAME_SIZE ];  // gamertags
    XUID            xuids[ MAX_USER_COUNT ];                     // XUIDs
    BOOL            bHasVoice[ MAX_USER_COUNT ];                 // who has voice permission
    DWORD           lastActivity[ MAX_USER_COUNT ];              // tick count of last idle/activity check
    BOOL            bIsIdle[ MAX_USER_COUNT ];                   // who is idle
    BOOL            bToAdd[ MAX_USER_COUNT ];                    // who needs to be added to sessions
    BOOL            bToRemove[ MAX_USER_COUNT ];                 // who needs to be removed from sessions
    BOOL            bUsesPrivateSlot[ MAX_USER_COUNT ];          // does this user consume a private slot?
    ULONGLONG       presenceSessionNonces[ MAX_USER_COUNT ];     // presence session nonce
    DOUBLE          dMu[ MAX_USER_COUNT ];                       // mean skill of the players
    DOUBLE          dSigma[ MAX_USER_COUNT ];                    // skill standard deviation of the players
    UINT            nPoints[ MAX_USER_COUNT ];                   // number of points scored
    DWORD           dwHeartbeatTimer;                            // tick count of last heartbeat check
    DWORD           dwHeartbeat;                                 // last heartbeat received
    DWORD           dwWave[ MAX_USER_COUNT ];                    // last wave received
    BOOL            bInvited;                                    // should the client consume private slots
    BOOL            bRegistered;                                 // has the peer registered for arbitration
    DWORD           nController[ MAX_USER_COUNT ];               // which controller is this player on?
    BOOL            bWantsToMigrate;                             // keep track of who's trying to migrate to us
    BOOL            bPossibleNewHost;                            // keep track of who can possible host a migrated
                                                                 // session
    BOOL            bDroppingClient;                             // are we in the middle of dropping this client?
    
    ClientInfo();
    virtual ~ClientInfo();
    ClientInfo( const ClientInfo& ref );
    VOID Clear();
    BOOL IsInSession( const XNKID& sessionID ) const;
    BOOL GetInAddrForSession( IN_ADDR* pInAddr, const XNKID& sessionID ) const;
    VOID JoinedSession( const XNKID& sessionID, const IN_ADDR& hostInAddr, const IN_ADDR& clientInAddr );
    VOID LeftSession( const XNKID& sessionID );
    VOID ReplaceSession( const XNKID& oldSessionID, const XNKID& newSessionID );
    DWORD GetIdleUserMask() const;
    DWORD GetCountIdlePlayers() const;
    INT IndexPlayer( const XUID& xuid );
    
private:
	SessionData sessionData[ MAX_SESSIONS_PER_CLIENT ]; // Array of SessionData structs
	BOOL IsSessionSlotAvailable( UINT* pIndexSlot );
};

//--------------------------------------------------------------------------------------
// Name: struct LocalClientInfo
// Desc: Include information about who is muted for these clients
//--------------------------------------------------------------------------------------
typedef std::set< XUID > XUIDSet;
struct LocalClientInfo : public ClientInfo
{
    XONLINE_FRIEND                friends[ MAX_USER_COUNT ][ MAX_FRIENDS ];    // Friends data, stored by controller index
    XUID                          friendXuids[ MAX_USER_COUNT ][ MAX_FRIENDS ];// Friends xuids, stored by controller index
    DWORD                         cFriends[ MAX_USER_COUNT ];                  // Friends count, stored by controller index
    std::vector<XUID>             presenceXuids[ MAX_USER_COUNT ]; // Non-friends xuids, stored by controller index
    std::vector<XONLINE_PRESENCE> presenceInfo[ MAX_USER_COUNT ];  // Non-friends presence info, stored by controller index

    LocalClientInfo();
};

typedef std::list< ClientInfo > ClientInfoVec;
