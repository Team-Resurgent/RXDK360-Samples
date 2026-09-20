//--------------------------------------------------------------------------------------
// ClientInfo.cpp
//
// Definition of ClientInfo struct
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "XPlat.h"

VOID SessionData::Clear()
{
    ZeroMemory( &m_SessionID, sizeof( XNKID ) );
    ZeroMemory( &m_HostInAddr, sizeof( IN_ADDR ) );
    ZeroMemory( &m_ClientInAddr, sizeof( IN_ADDR ) );
}

VOID SessionData::Set( const XNKID& id, const IN_ADDR& inaddrHost, const IN_ADDR& inaddrClient )
{
    memcpy_s( &m_SessionID, sizeof( XNKID ), &id, sizeof( XNKID ) );
    memcpy_s( &m_HostInAddr, sizeof( IN_ADDR ), &inaddrHost, sizeof( IN_ADDR ) );
    memcpy_s( &m_ClientInAddr, sizeof( IN_ADDR ), &inaddrClient, sizeof( IN_ADDR ) );
}

BOOL SessionData::operator==( const XNKID& ref ) const
{
    return ( memcmp( &ref, &m_SessionID, sizeof( XNKID ) ) == 0 );
}

ClientInfo::ClientInfo()// : vecSessionData()
{
    Clear();
}

ClientInfo::~ClientInfo()
{
}

ClientInfo::ClientInfo( const ClientInfo& ref )
{
    Clear();

    cPlayers = ref.cPlayers;
    memcpy_s( &xuids, sizeof(XUID), &ref.xuids, sizeof(XUID) );
    memcpy_s( &nController, sizeof(nController), &ref.nController, sizeof(ref.nController) );
}

VOID ClientInfo::Clear()
{
    ZeroMemory( this, sizeof( ClientInfo ) );
}

BOOL ClientInfo::IsInSession( const XNKID& sessionID ) const
{
    for( UINT i = 0; i < MAX_SESSIONS_PER_CLIENT; ++i )
    {
        if( sessionData[i] == sessionID )
        {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL ClientInfo::GetInAddrForSession( IN_ADDR* pInAddr, const XNKID& sessionID ) const 
{
    if( !pInAddr )
    {
        return FALSE;
    }

    for( UINT i = 0; i < MAX_SESSIONS_PER_CLIENT; ++i )
    {
        if( sessionData[i] == sessionID )
        {
            memcpy_s( pInAddr, sizeof( IN_ADDR ), &sessionData[i].m_ClientInAddr, sizeof( IN_ADDR ) );
            return TRUE;
        }
    }
    return FALSE;
}

BOOL ClientInfo::IsSessionSlotAvailable( UINT* pIndexSlot )
{
    const XNKID dummySessionID = {0};

    for( UINT i = 0; i < MAX_SESSIONS_PER_CLIENT; ++i )
    {
        if( sessionData[i] == dummySessionID )
        {
            if( pIndexSlot )
            {
                *pIndexSlot = i;
            }
            return TRUE;
        }
    }
    return FALSE;
}

VOID ClientInfo::JoinedSession( const XNKID& sessionID, const IN_ADDR& hostInAddr, const IN_ADDR& clientInAddr )
{
    for( UINT i = 0; i < MAX_SESSIONS_PER_CLIENT; ++i )
    {
        // Check if we already have an entry for this session ID. If so,
        // nothing more to do
        if( sessionData[i] == sessionID )
        {
            return;
        }
    }

    // New entry for this session ID
    UINT index;
    if( IsSessionSlotAvailable( &index ) )
    {
        sessionData[index].Set( sessionID, hostInAddr, clientInAddr );
    }
    else
    {
        FatalError( "Client used up max available SessionData slots (%d)!\n", MAX_SESSIONS_PER_CLIENT );
    }
}

VOID ClientInfo::LeftSession( const XNKID& sessionID )
{
    for( UINT i = 0; i < MAX_SESSIONS_PER_CLIENT; ++i )
    {
        if( sessionData[i] == sessionID )
        {
            sessionData[i].Clear();
            return;
        }
    }
}

VOID ClientInfo::ReplaceSession( const XNKID& oldSessionID, const XNKID& newSessionID )
{
    for( UINT i = 0; i < MAX_SESSIONS_PER_CLIENT; ++i )
    {
        if( sessionData[i] == oldSessionID )
        {
            memcpy_s( &sessionData[i].m_SessionID, sizeof( XNKID ), &newSessionID, sizeof( XNKID ) );
            return;
        }
    }
}

DWORD ClientInfo::GetIdleUserMask() const
{
    DWORD mask = 0;
    for( UINT i = 0; i < cPlayers; ++i )
    {
        if( bIsIdle[ i ] )
        {
            mask |= 1 << nController[ i ];
        }
    }
    return mask;
}

DWORD ClientInfo::GetCountIdlePlayers() const
{
    DWORD count = 0;
    for( UINT i = 0; i < cPlayers; ++i )
    {
        if( bIsIdle[ i ] )
        {
            count++;
        }
    }
    return count;
}

INT ClientInfo::IndexPlayer( const XUID& xuid )
{
    for( UINT i = 0; i < cPlayers; ++i )
    {
        if( xuid == xuids[ i ] )
        {
            return i;
        }
    }      
    return -1;
}

LocalClientInfo::LocalClientInfo()
{
    // Count of local players is always MAX_USER_COUNT
    cPlayers = MAX_USER_COUNT;

    // All local users use private slots
    bInvited = TRUE;

    // All local users initially non-idle
    // Also pre-allocate 10 slots for non-friends presence info
    for( UINT i = 0; i < cPlayers; ++i )
    {
        bIsIdle[ i ] = FALSE;

        presenceXuids[ i ].reserve( 10 );
        presenceInfo[ i ].reserve( 10 );
    }
}

