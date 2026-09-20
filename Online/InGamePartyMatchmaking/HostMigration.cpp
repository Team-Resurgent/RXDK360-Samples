//--------------------------------------------------------------------------------------
// HostMigration.cpp
//
// Contains helpers and data for processing host migration
//
// It should be noted that this file is intended to demonstrate the use of the
// XSessionMigrateHost() API. It does not implement a robust and well-featured
// host migration system.
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma warning( disable: 4995 ) // 'function': name was marked as #pragma deprecated

#include "XPlat.h"

const char* CHostMigrationHelper::m_astrHostMigrationStates[] =
{
    "HOSTMIGRATIONSTATE_NOTMIGRATING",
    "HOSTMIGRATIONSTATE_STARTING",
    "HOSTMIGRATIONSTATE_HOSTMIGRATING",
    "HOSTMIGRATIONSTATE_ERROR",
    "HOSTMIGRATIONSTATE_WAITINGFORMIGREES",
    "HOSTMIGRATIONSTATE_WAITINGFORHOST",
};

//--------------------------------------------------------------------------------------
// Name: CHostMigrationHelper()
// Desc: Ctor
//--------------------------------------------------------------------------------------
CHostMigrationHelper::CHostMigrationHelper() : 
    m_State( HOSTMIGRATIONSTATE_NOTMIGRATING ),
    m_pSample( NULL )
{
}

//--------------------------------------------------------------------------------------
// Name: SwitchToState()
// Desc: Changes to a new appstate and performs initialization for the new state
//--------------------------------------------------------------------------------------
VOID CHostMigrationHelper::SwitchToState( HostMigrationState newState )
{
    DebugSpew( "Switching from host migration state %s to %s. m_pSample = %p\n", 
               m_astrHostMigrationStates[m_State], 
               m_astrHostMigrationStates[newState], 
               m_pSample );

    m_State = newState;
}

//--------------------------------------------------------------------------------------
// Name: BeginHostMigration()
// Desc: Initialize variables for starting the migration
//--------------------------------------------------------------------------------------
void CHostMigrationHelper::BeginHostMigration( Sample* pSample, SessionManager* pSessionMgr )
{
    m_pSample               = pSample;
    m_AppState              = m_pSample->m_AppState;
    m_pSessionMgr           = pSessionMgr;

    SwitchToState( HOSTMIGRATIONSTATE_STARTING );

    // Mark all clients in the session we're trying to migrate
    // as potential hosts
    const XNKID sessionID = m_pSessionMgr->GetSessionID();
    for( ClientInfoVec::iterator i = m_pSample->m_vecRemote.begin(); i != m_pSample->m_vecRemote.end(); ++i )
    {
        if( i->IsInSession( sessionID ) && !i->bDroppingClient )
        {
            i->bPossibleNewHost = TRUE;
        }
        else
        {
            i->bPossibleNewHost = FALSE;
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: UpdateHostMigration()
// Desc: State machine driver during hosting. Return S_OK if done with host migration.
//       Otherwise returns E_FAIL
//--------------------------------------------------------------------------------------
HRESULT CHostMigrationHelper::UpdateHostMigration( void )
{
    // Check for received messages
    if( m_pSample )
    {
        m_pSample->ReceiveMessage();
    }

    // Check to see what the next phase in the migration is
    switch( m_State )
    {
    case HOSTMIGRATIONSTATE_NOTMIGRATING:
    {                
        return S_OK; // Done with host migration
    }
    case HOSTMIGRATIONSTATE_STARTING:
        m_pNewHost = NULL;
        m_pNewHost = SelectNewHost();

        if( !m_pNewHost )
        {
            SwitchToState( HOSTMIGRATIONSTATE_ERROR );
        }
        else if( m_pNewHost == &m_pSample->m_Local )
        {
            // we're the new host, start hosting
            BeginHosting();
        }
        else
        {
            SwitchToNewHost();
        }
        break;

    case HOSTMIGRATIONSTATE_HOSTMIGRATING:
        //if( m_pSample->SetLastXSessionError( m_pSessionMgr ) )
        //{
        //    SwitchToState( HOSTMIGRATIONSTATE_ERROR );
        //    break;
        //}

        // If we're not the host, we're done
        if( !m_pSessionMgr->IsSessionHost() )
        {
            EndMigration();
        }
        else
        {
            // If this is a Matchmaking session, start QoS listener
            if( m_pSessionMgr == m_pSample->GetMatchmakingSession() )
            {
                m_pSessionMgr->StartQoSListener( ( BYTE* )m_pSample->m_QoSString, 
                                                wcslen( m_pSample->m_QoSString ) * sizeof( WCHAR ),
                                                0 );
            }

            // Tell everybody else to migrate to me
            BeginNotifyingClients();
        }
        break;

    case HOSTMIGRATIONSTATE_WAITINGFORMIGREES:
        if( GetTickCount() - m_dwTimer > HOSTMIGRATION_RETRYINTERVAL )
        {
            if( ++m_nNotificationRetries > HOSTMIGRATION_MAXRETRIES )
            {
                EndMigration();
            }
            else
            {
                NotifyClientsToMigrate();
            }
        }
        break;

    case HOSTMIGRATIONSTATE_WAITINGFORHOST:
        if( GetTickCount() - m_dwTimer > HOSTMIGRATION_MAXWAITFORHOST )
        {
            // This guy isn't hosting. Dump him from our list and move
            // on to the next one
            for( ClientInfoVec::iterator i = m_pSample->m_vecRemote.begin(); i != m_pSample->m_vecRemote.end(); ++i )
            {
                if( i->id == m_pNewHost->id )
                {
                    i->bPossibleNewHost = FALSE;
                    break;
                }
            }

            SwitchToState( HOSTMIGRATIONSTATE_STARTING );
        }
        break;

    case HOSTMIGRATIONSTATE_ERROR:
        // Schedule tasks to handle the session deletion
        m_pSample->ScheduleSessionDeletionTasks( m_pSessionMgr ); 

        return S_OK; // Done with host migration
    }

    return E_FAIL; // Not done with host migration
}


//--------------------------------------------------------------------------------------
// Name: SelectNewHost()
// Desc: Choose the best new client to be the host
//--------------------------------------------------------------------------------------
ClientInfo* CHostMigrationHelper::SelectNewHost( void )
{
    ClientInfo* pRet = NULL;

    const XNKID sessionID          = m_pSessionMgr->GetSessionID();
    const __int64 sessionIDInt64   = XNKIDToInt64( sessionID );

    // Prefer local client, but only if there are players who aren't to be removed
    // Determine which local user will be the owner of this session
    UINT cRemainingPlayers = 0;
    for( UINT idx = 0; idx < MAX_USER_COUNT; idx++ )
    {
        const XUSER_SIGNIN_INFO& info   = m_pSample->m_SignInInfo[ m_pSample->m_Local.nController[ idx ] ];
        const BOOL bIsIdle              = m_pSample->m_Local.bIsIdle[ idx ];
        const BOOL bToRemove            = m_pSample->m_Local.bToRemove[ idx ];
        const BOOL bIsSignedInToLive    = ( info.UserSigninState == eXUserSigninState_SignedInToLive );

        if( bIsSignedInToLive && !bIsIdle && !bToRemove )
        {
           cRemainingPlayers++;
        }
    }

    if( cRemainingPlayers )
    {
        pRet = &m_pSample->m_Local;
    }

    // loop through the array, look for somebody in the session with a lower ID
    for( ClientInfoVec::iterator i = m_pSample->m_vecRemote.begin(); i != m_pSample->m_vecRemote.end(); ++i )
    {
        if( !i->IsInSession( sessionID ) )
        {
            DebugSpew( "SelectNewHost(%016I64X): Disregarding peer %I64u as new host. Reason: Not in session\n", 
                       sessionIDInt64, i->id );

            continue;
        }

        if( !i->bPossibleNewHost || i->bDroppingClient )
        {
            DebugSpew( "SelectNewHost(%016I64X): Disregarding peer %I64u as new host. Possible Reasons: bPossibleNewHost: %d; bDroppingClient: %d\n", 
                       sessionIDInt64, i->id, i->bPossibleNewHost, i->bDroppingClient );

            continue;
        }

        if( !pRet )
        {
            DebugSpew( "SelectNewHost(%016I64X): Found new host %I64u. Waiting for MSG_MIGRATE from new host...\n",
                       sessionIDInt64, i->id );
                           
            pRet = &(*i);
            break;
        }
        else if( i->id < pRet->id  )
        {
            DebugSpew( "SelectNewHost(%016I64X): Found new host %I64u. Waiting for MSG_MIGRATE from new host...\n",
                       sessionIDInt64, i->id );
                           
            pRet = &(*i);
            break;
        }
    }

    // Old host can't be new host, otherwise we can be caught
    // in a neverending loop
    if( m_pNewHost && pRet->id == m_pNewHost->id )
    {
        pRet = NULL;
    }

    return pRet;
}


//--------------------------------------------------------------------------------------
// Name: BeginHosting()
// Desc: Become the new host
//--------------------------------------------------------------------------------------
void CHostMigrationHelper::BeginHosting()
{
    const XNKID sessionID          = m_pSessionMgr->GetSessionID();
    const __int64 sessionIDInt64   = XNKIDToInt64( sessionID );

    // We're the new host! Set the appropriate flags
    DebugSpew( "BeginHosting(%016I64X): Will be hosting the migrated session! My ID is %I64u:\n", sessionIDInt64, m_pSample->m_Local.id );

    m_pSessionMgr->MakeSessionHost();
    m_pSessionMgr->SetHostInAddr( m_pSample->m_Local.addr );

    // Determine which local user will be the owner of this session
    UINT idx = 0;
    for( ; idx < m_pSample->m_Local.cPlayers; idx++ )
    {
        const XUSER_SIGNIN_INFO& info   = m_pSample->m_SignInInfo[ m_pSample->m_Local.nController[ idx ] ];
        const BOOL bIsIdle              = m_pSample->m_Local.bIsIdle[ idx ];
        const BOOL bToRemove            = m_pSample->m_Local.bToRemove[ idx ];
        const BOOL bIsSignedInToLive    = ( info.UserSigninState == eXUserSigninState_SignedInToLive );

        if( bIsSignedInToLive && !bIsIdle && !bToRemove )
        {
           break;
        }
    }
    
    assert( idx < MAX_USER_COUNT );

    // We found our session owner
    m_pSessionMgr->SetSessionOwner( m_pSample->m_Local.nController[ idx ] );

    // Since we'll be the owner of the session, set up pSessionMgr to pass in
    // any empty XSESSION_INFO to XSessionMigrateHost
    XSESSION_INFO emptyInfo;
    ZeroMemory( &emptyInfo, sizeof( XSESSION_INFO ) );
    m_pSessionMgr->SetNewSessionInfo( emptyInfo, TRUE );

    // Start host migration
    m_pSessionMgr->MigrateSession( NULL );
    SwitchToState( HOSTMIGRATIONSTATE_HOSTMIGRATING );
}


//--------------------------------------------------------------------------------------
// Name: BeginNotifyingClients
// Desc: Tell remote machines that we're now hosting and they should come over
//--------------------------------------------------------------------------------------
void CHostMigrationHelper::BeginNotifyingClients( void )
{
    // loop through the list of clients and indicate that they haven't told us they want to migrate yet
    for( ClientInfoVec::iterator i = m_pSample->m_vecRemote.begin(); i != m_pSample->m_vecRemote.end(); ++i )
    {
        // for bookkeeping reasons, if we're dropping the client, pretend that they want to migrate to us
        // so we don't wait for a response from them to migrate
        if( i->bDroppingClient )
        {
            i->bWantsToMigrate = TRUE;
        }
        else
        {
            i->bWantsToMigrate = FALSE;
        }
    }

    m_nNotificationRetries = 0;

    // Update all of our clients to use the new session ID from now on
    const XNKID migratedSessionID       = m_pSessionMgr->GetMigratedSessionID();
    const XNKID newSessionID            = m_pSessionMgr->GetSessionID();

    #ifdef _DEBUG
    if( memcmp( &newSessionID, &migratedSessionID, sizeof( XNKID ) ) == 0 )
    {
        DebugSpew( "BeginNotifyingClients(%016I64X): SessionID and migratedSessionID the same\n", 
                   XNKIDToInt64( newSessionID ) );
    }
    #endif

    for( ClientInfoVec::iterator i = m_pSample->m_vecRemote.begin(); i != m_pSample->m_vecRemote.end(); ++i )
    {
        if( i->IsInSession( migratedSessionID ) )
        {
            i->ReplaceSession( migratedSessionID, newSessionID );
        }
    }

    NotifyClientsToMigrate();
    SwitchToState( HOSTMIGRATIONSTATE_WAITINGFORMIGREES );
}


//--------------------------------------------------------------------------------------
// Name: NotifyClientsToMigrate
// Desc: Send a message to clients to tell them to come over
//--------------------------------------------------------------------------------------
void CHostMigrationHelper::NotifyClientsToMigrate( void )
{
    const XNKID sessionID           = m_pSessionMgr->GetSessionID();
    const __int64 sessionIDInt64    = XNKIDToInt64( sessionID );

    // Until our clients initiate host migration they will still
    // be aware of the old (migrated) session ID, so send messages to them
    // with that ID
    const XNKID migratedSessionID         = m_pSessionMgr->GetMigratedSessionID();
    const __int64 migratedsessionIDInt64  = XNKIDToInt64( migratedSessionID );

    // assert that sessionID and migratedSessionID are different
    assert( memcmp( &sessionID, &migratedSessionID, sizeof( XNKID ) ) );

    // Get our new session info. It's what we pass to existing
    // clients that we want to migrate to us
    const XSESSION_INFO& newSessionInfo = m_pSessionMgr->GetSessionInfo();

    // Assert that the host address specified in the newSessionInfo is us,
    // since we're the new host
    assert( memcmp( &newSessionInfo.hostAddress, 
                    &m_pSample->m_Local.xnaddr, sizeof( XNADDR ) ) == 0 );

    // Send MSG_MIGRATE message
    CMessage msg(MSG_MIGRATE);
    msg.GetMigrate().id = m_pSample->m_Local.id;          
    msg.SetSessionID( migratedSessionID );
    msg.GetMigrate().msgType = MsgMigrate::MIGRATE_HOSTING;
    memcpy_s( &msg.GetMigrate().info, 
            sizeof( XSESSION_INFO ),
            &newSessionInfo, 
            sizeof( XSESSION_INFO ) );
    
    // loop through the list of clients in the session
    UINT cClientsLeftInSession = 0;
    for( ClientInfoVec::iterator i = m_pSample->m_vecRemote.begin(); i != m_pSample->m_vecRemote.end(); ++i )
    {
        if( i->IsInSession( sessionID ) && !i->bDroppingClient )
        {
            cClientsLeftInSession++;
        }

        if( i->IsInSession( sessionID ) && i->bWantsToMigrate )
        {
            continue;
        }

        IN_ADDR inaddr = {0};
        if( i->GetInAddrForSession( &inaddr, sessionID ) && !i->bDroppingClient )
        {
            DebugSpew( "NotifyClientsToMigrate(%016I64X): Sending MSG_MIGRATE to client %I64u to migrate to us\n"
                       "using migrated sessionID %016I64X\n",
                       sessionIDInt64,
                       i->id,
                       migratedsessionIDInt64 );

            m_pSample->SendMessage( &msg, inaddr );
        }
    }

    // If no clients left to notify, we're done
    if( cClientsLeftInSession == 0 )
    {
        m_nNotificationRetries = HOSTMIGRATION_MAXRETRIES + 1;
        m_dwTimer = 0;
    }
    else
    {
        // wait a fixed amount of time for replies
        m_dwTimer = GetTickCount();
    }
}


//--------------------------------------------------------------------------------------
// Name: EndMigration
// Desc: We're done, one way or another. Prune those who didn't migrate
//--------------------------------------------------------------------------------------
void CHostMigrationHelper::EndMigration( void )
{
    const XNKID sessionID           = m_pSessionMgr->GetSessionID();
    const __int64 sessionIDInt64    = XNKIDToInt64( sessionID );

    // If we're the new host, prune those who are no longer with us
    if( m_pSessionMgr->IsSessionHost() )
    {
        DebugSpew( "EndMigration(%016I64X - new host): ID %I64u\n", 
                   sessionIDInt64,
                   m_pSample->m_Local.id );

        std::list<IN_ADDR> addrGone;
        std::vector<ULONGLONG> idGone;

        // loop through the list, find who's not with us
        for( ClientInfoVec::iterator i = m_pSample->m_vecRemote.begin(); i != m_pSample->m_vecRemote.end(); ++i )
        {
            if( !i->bWantsToMigrate && i->IsInSession( sessionID ) )
            {
                // he didn't respond, he gets pruned.
                IN_ADDR inaddr = {0};
                if( i->GetInAddrForSession( &inaddr, sessionID ) )
                {
                    addrGone.push_back( inaddr );
                    idGone.push_back( i->id );
                }
            }
            else
            {
                i->dwHeartbeat = GetTickCount();
            }
        }

        // Remove users from the new session and then tell peers about the lost ones
        int k = 0;
        for( std::list<IN_ADDR>::iterator i = addrGone.begin(); i != addrGone.end(); ++i )
        {
            DebugSpew( "EndMigration(%016I64X): Dropping client %I64u\n", 
                       sessionIDInt64,
                       idGone[k] );

            m_pSample->ClientDropped( *i, sessionID );
            k++;
        }
    }
    else
    { 
        DebugSpew( "EndMigration(%016I64X - new client): ID %I64u\n", 
                   sessionIDInt64,
                   m_pSample->m_Local.id );

        // Make sure our new host hasn't forgotten us
        m_pNewHost->dwHeartbeat = GetTickCount();
        m_pSample->HandleHeartbeat( m_pNewHost, sessionID );
    }
    
    // No longer migrating
    SwitchToState( HOSTMIGRATIONSTATE_NOTMIGRATING );
}


//--------------------------------------------------------------------------------------
// Name: SwitchToNewHost
// Desc: Wait for the new host to give us a call
//--------------------------------------------------------------------------------------
void CHostMigrationHelper::SwitchToNewHost( void )
{
    // Set a timer to elapse
    m_dwTimer = GetTickCount();

    SwitchToState( HOSTMIGRATIONSTATE_WAITINGFORHOST );
}


//--------------------------------------------------------------------------------------
// Name: ProcessMigrateMessage
// Desc: Handle a host migration message
//--------------------------------------------------------------------------------------
void CHostMigrationHelper::ProcessMigrateMessage( CMessage* pMsg, IN_ADDR addrFrom )
{
    MsgMigrate msg                  = pMsg->GetMigrate();
    const XNKID sessionID           = pMsg->GetSessionID();
    const __int64 sessionIDInt64    = XNKIDToInt64( sessionID );
    const __int64 newSessionIDInt64 = XNKIDToInt64( msg.info.sessionID );

    CMessage msgReply( MSG_MIGRATE );

    // Get client object for the message source
    ClientInfo* pClient = NULL;
    for( ClientInfoVec::iterator i = m_pSample->m_vecRemote.begin(); i != m_pSample->m_vecRemote.end(); ++i )
    {
        IN_ADDR inaddr = {0};
        if( i->GetInAddrForSession( &inaddr, sessionID ) && inaddr == addrFrom )
        {
            pClient = &(*i);
            break;
        }
    }

    if( pClient == NULL )
    {
        DebugSpew( "ProcessMigrateMessage(%016I64X): Got message from unknown remote source!\n", 
                   sessionIDInt64 );
        return;
    }

    DebugSpew( "ProcessMigrateMessage(%016I64X): Received MSG_MIGRATE from client %I64u ...\n", 
               sessionIDInt64, pClient->id );


    // If we're not migrating, ignore the message
    if( m_State == HOSTMIGRATIONSTATE_NOTMIGRATING )
    {
        DebugSpew( "ProcessMigrateMessage(%016I64X): Currently in state HOSTMIGRATIONSTATE_NOTMIGRATING, so instructing client %I64u to standby\n",
                   sessionIDInt64, pClient->id );

        msgReply.GetMigrate().msgType = MsgMigrate::MIGRATE_STANDBY;
        m_pSample->SendMessage( &msgReply, addrFrom );

    }
    else if( msg.msgType == MsgMigrate::MIGRATE_HOSTING )
    {
        //
        // Is somebody else inviting us to migrate?
        //

        // Our response will refer to the new session ID sent
        // from the host, since this is the one that the host
        // will be using from now on
        msgReply.SetSessionID( msg.info.sessionID );

        DebugSpew( "ProcessMigrateMessage(%016I64X): Potential new session found %016I64X\n",
                   sessionIDInt64,
                   newSessionIDInt64 );

        // Somebody's hosting. Do we expect someone to host?
        if( m_State == HOSTMIGRATIONSTATE_WAITINGFORHOST )
        {
            DebugSpew( "ProcessMigrateMessage(%016I64X): HOSTMIGRATIONSTATE_WAITINGFORHOST\n", 
                       sessionIDInt64 );

            // Is the host the one we're looking for?
            if( msg.id == m_pNewHost->id )
            {
                DebugSpew( "ProcessMigrateMessage(%016I64X): New host %I64u found! "
                           "Migrating session and all data to new session ID %016I64X\n",
                           sessionIDInt64, m_pNewHost->id, newSessionIDInt64 );

                m_pSessionMgr->SetNewSessionInfo( msg.info, FALSE );
                m_pSessionMgr->SetHostInAddr( addrFrom );

                // Let them know we're migrating to them
                msgReply.GetMigrate().msgType = MsgMigrate::MIGRATE_MIGRATED;

                DebugSpew( "ProcessMigrateMessage(%016I64X): Sending MIGRATE_MIGRATED to new host %I64u "
                           "of session %016I64X\n",
                           sessionIDInt64, m_pNewHost->id, newSessionIDInt64 );

                m_pSample->SendMessage( &msgReply, addrFrom );

                // Start host migration
                m_pSessionMgr->MigrateSession( NULL );
                SwitchToState( HOSTMIGRATIONSTATE_HOSTMIGRATING );      

                // Update all of our clients to use the new session ID from now on
                for( ClientInfoVec::iterator i = m_pSample->m_vecRemote.begin(); 
                     i != m_pSample->m_vecRemote.end(); ++i )
                {
                    if( i->IsInSession( sessionID ) )
                    {
                        i->ReplaceSession( sessionID, msg.info.sessionID );
                    }
                }
            }
            else
            {
                // Somebody else is trying to host. Tell him to stand by
                msgReply.GetMigrate().msgType = MsgMigrate::MIGRATE_STANDBY;

                DebugSpew( "ProcessMigrateMessage(%016I64X): Sending MIGRATE_STANDBY to new host %I64u! "
                           "of session %016I64X\n",
                           sessionIDInt64, m_pNewHost->id, newSessionIDInt64 );

                m_pSample->SendMessage( &msgReply, addrFrom );
            }
        }
    }

    // Has somebody in our session else migrated to us?
    if( m_State == HOSTMIGRATIONSTATE_WAITINGFORMIGREES )
    {
        DebugSpew( "ProcessMigrateMessage(%016I64X): HOSTMIGRATIONSTATE_WAITINGFORMIGREES\n", 
                   sessionIDInt64 );

        if( msg.msgType == MsgMigrate::MIGRATE_MIGRATED )
        {
            DebugSpew( "ProcessMigrateMessage(%016I64X): HOSTMIGRATIONSTATE_WAITINGFORMIGREES - received MIGRATE_MIGRATED\n", 
                       sessionIDInt64 );

            // Find him in the list and flag him as having replied
            BOOL bAllreplied = TRUE;

            for( ClientInfoVec::iterator i = m_pSample->m_vecRemote.begin(); i != m_pSample->m_vecRemote.end(); ++i )
            {
                if( !i->IsInSession( sessionID ) )
                {
                    continue;
                }

                IN_ADDR inaddr = {0};
                if( !i->GetInAddrForSession( &inaddr, sessionID ) )
                {
                    continue;
                }

                if( inaddr == addrFrom )
                {
                    DebugSpew( "ProcessMigrateMessage(%016I64X): New client %I64u found!\n", 
                               sessionIDInt64, i->id );

                    i->bWantsToMigrate = TRUE;
                }

                if( msg.msgType == MsgMigrate::MIGRATE_MIGRATED )
                {
                    bAllreplied = bAllreplied && i->bWantsToMigrate;
                }
            }

            // If everybody's migrating, no need to stick around
            if( bAllreplied )
            {
                DebugSpew( "ProcessMigrateMessage(%016I64X): All expected clients have migrated to us. Ending migration\n", 
                           sessionIDInt64 );

                EndMigration();
            }
        }

        // Is somebody requesting a standby?
        if( msg.msgType == MsgMigrate::MIGRATE_STANDBY )
        {
            m_nNotificationRetries--;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderHostMigration()
// Desc: Render the screen when host migration happens
//--------------------------------------------------------------------------------------
void CHostMigrationHelper::RenderHostMigration()
{
    static const WCHAR* wstrMigState[] =
    {
        L"HOST MIGRATION: NOT MIGRATING",
        L"HOST MIGRATION: STARTING",
        L"HOST MIGRATION: HOST MIGRATING",
        L"HOST MIGRATION: ERROR",
        L"HOST MIGRATION: WAITING FOR MIGREES",
        L"HOST MIGRATION: WAITING FOR HOST",
        L"HOST MIGRATION: FAILED. DELETING SESSION",
    };

    if( m_pSample )
    {
        m_pSample->m_CXPlat_Draw.DrawTextCenterScreen( Sample::COLOR_TEXT, wstrMigState[ m_State ] );
    }
}
