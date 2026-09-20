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

#include <xtl.h>
#include <AtgUtil.h>
#include <AtgApp.h>
#include "Sessions.h"
#include "HostMigration.h"

//--------------------------------------------------------------------------------------
// Name: BeginHostMigration()
// Desc: Initialize variables for starting the migration
//--------------------------------------------------------------------------------------
void CHostMigrationHelper::BeginHostMigration( Sample* pSample )
{
    m_pSample = pSample;

    m_AppState = m_pSample->m_AppState;
    m_pSample->SwitchToState( Sample::APPSTATE_HOSTMIGRATION );

    m_State = HOSTMIGRATIONSTATE_STARTING;
}


//--------------------------------------------------------------------------------------
// Name: UpdateHostMigration()
// Desc: State machine driver during hosting
//--------------------------------------------------------------------------------------
void CHostMigrationHelper::UpdateHostMigration( void )
{
    // Check for received messages
    m_pSample->ReceiveMessage();

    // Check to see what the next phase in the migration is
    switch( m_State )
    {
        case HOSTMIGRATIONSTATE_STARTING:
            m_pNewHost = SelectNewHost();
            if( m_pNewHost == &m_pSample->m_Local )
            {
                // we're the new host, start hosting
                BeginHosting();
            }
            else
            {
                // switch to the new host
                SwitchToNewHost();
            }
            break;

        case HOSTMIGRATIONSTATE_HOSTMIGRATING:
            if( XHasOverlappedIoCompleted( &m_Overlapped ) )
            {
                m_hr = XGetOverlappedExtendedError( &m_Overlapped );
                if( !SUCCEEDED( m_hr ) )
                {
                    m_State = HOSTMIGRATIONSTATE_ERROR;
                }
                else
                {
                    m_pSample->m_Session.SetSessionInfo( m_NewSessionInfo );
                    // If we're not the host, we're done
                    if( !m_pSample->m_Session.IsHost() )
                    {
                        EndMigration();
                    }
                    else
                    {
                        // Tell everybody else to migrate to me
                        BeginNotifyingClients();
                    }
                }
            }
            break;

        case HOSTMIGRATIONSTATE_WAITINGFORMIGREES:
            if( GetTickCount() - m_dwTimer > HOSTMIGRATION_RETRYINTERVAL )
            {
                if( m_nNotificationRetries > HOSTMIGRATION_MAXRETRIES )
                {
                    EndMigration();
                }
                else
                {
                    NotifyClientsToMigrate();
                }
            }
            m_pSample->HandleHeartbeat();
            break;

        case HOSTMIGRATIONSTATE_WAITINGFORHOST:
            if( GetTickCount() - m_dwTimer > HOSTMIGRATION_MAXWAITFORHOST )
            {
                // This guy isn't hosting. Dump him from our list and move
                // on to the next one
                for( ClientInfoVec::iterator i = m_pSample->m_vecRemote.begin(); i != m_pSample->m_vecRemote.end();
                     i++ )
                {
                    if( i->id == m_pNewHost->id )
                    {
                        m_pSample->RemoveClient( i );
                        break;
                    }
                }

                m_State = HOSTMIGRATIONSTATE_STARTING;
            }
            break;

    }
}


//--------------------------------------------------------------------------------------
// Name: SelectNewHost()
// Desc: Choose the best new client to be the host
//--------------------------------------------------------------------------------------
ClientInfo* CHostMigrationHelper::SelectNewHost( void )
{
    ClientInfo* pRet = &m_pSample->m_Local;

    // loop through the array, look for somebody with a lower ID
    for( ClientInfoVec::iterator i = m_pSample->m_vecRemote.begin(); i != m_pSample->m_vecRemote.end(); i++ )
    {
        if( i->id < pRet->id )
        {
            pRet = &( *i );
        }
    }

    return pRet;
}


//--------------------------------------------------------------------------------------
// Name: BeginHosting()
// Desc: Become the new host
//--------------------------------------------------------------------------------------
void CHostMigrationHelper::BeginHosting()
{
    // We're the new host! Set the appropriate flags
    m_pSample->m_Session.SetHost( TRUE );
    m_pSample->m_pHost = &m_pSample->m_Local;

    // We should take up private slots if we don't already
    if( !m_pSample->m_Local.bInvited )
    {
        m_pSample->RemoveUsersFromSession( &m_pSample->m_Local );
        m_pSample->m_Local.bInvited = TRUE;
        m_pSample->AddUsersToSession( &m_pSample->m_Local );
    }
    m_pSample->RecalcSlots();

    m_NewSessionInfo = m_pSample->m_Session.GetSessionInfo();

    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

    // Call Xbox Live to migrate the session
    m_hr = XSessionMigrateHost(
        m_pSample->m_Session.GetSessionHandle(),
        m_pSample->m_Session.GetSessionOwner(),
        &m_NewSessionInfo,
        &m_Overlapped );

    if( FAILED( m_hr ) )
    {
        m_State = HOSTMIGRATIONSTATE_ERROR;
    }
    else
    {
        m_State = HOSTMIGRATIONSTATE_HOSTMIGRATING;
    }
}


//--------------------------------------------------------------------------------------
// Name: BeginNotifyingClients
// Desc: Tell remote machines that we're now hosting and they should come over
//--------------------------------------------------------------------------------------
void CHostMigrationHelper::BeginNotifyingClients( void )
{
    // loop through the list of clients
    for( ClientInfoVec::iterator i = m_pSample->m_vecRemote.begin(); i != m_pSample->m_vecRemote.end(); i++ )
    {
        i->bWantsToMigrate = FALSE;
    }

    m_nNotificationRetries = 0;

    NotifyClientsToMigrate();
    m_State = HOSTMIGRATIONSTATE_WAITINGFORMIGREES;
}


//--------------------------------------------------------------------------------------
// Name: NotifyClientsToMigrate
// Desc: Send a message to clients to tell them to come over
//--------------------------------------------------------------------------------------
void CHostMigrationHelper::NotifyClientsToMigrate( void )
{
    CMessage msg( MSG_MIGRATE );

    msg.GetMigrate().msgType = MsgMigrate::MIGRATE_HOSTING;
    msg.GetMigrate().info = m_NewSessionInfo;

    // loop through the list of clients
    for( ClientInfoVec::iterator i = m_pSample->m_vecRemote.begin(); i != m_pSample->m_vecRemote.end(); i++ )
    {
        if( i->bWantsToMigrate )
        {
            continue;
        }

        m_pSample->SendMessage( &msg, i->addr );
    }

    // wait a fixed amount of time for replies
    m_dwTimer = GetTickCount();

    m_nNotificationRetries++;
}


//--------------------------------------------------------------------------------------
// Name: EndMigration
// Desc: We're done, one way or another. Prune those who didn't migrate
//--------------------------------------------------------------------------------------
void CHostMigrationHelper::EndMigration( void )
{
    // If we're the new host, prune those who are no longer with us
    if( m_pSample->m_Session.IsHost() )
    {
        std::list <IN_ADDR> addrGone;

        // loop through the list, find who's not with us
        for( ClientInfoVec::iterator i = m_pSample->m_vecRemote.begin(); i != m_pSample->m_vecRemote.end(); i++ )
        {
            if( !i->bWantsToMigrate )
            {
                // he didn't respond, he gets pruned.
                addrGone.push_back( i->addr );
            }
            else
            {
                i->dwHeartbeat = GetTickCount();
            }
        }

        // Notify the remaining peers about the lost ones
        for( std::list <IN_ADDR>::iterator i = addrGone.begin(); i != addrGone.end(); i++ )
        {
            m_pSample->ClientDropped( *i );
        }

        m_pSample->m_pHost->addr = NULLADDR;

    }
    else
    {
        m_pSample->m_pHost->dwHeartbeat = GetTickCount();
    }

    // back to work
    m_pSample->m_AppState = ( Sample::APPSTATE )m_AppState;
    m_State = HOSTMIGRATIONSTATE_NOTMIGRATING;
}


//--------------------------------------------------------------------------------------
// Name: SwitchToNewHost
// Desc: Wait for the new host to give us a call
//--------------------------------------------------------------------------------------
void CHostMigrationHelper::SwitchToNewHost( void )
{
    // Set a timer to elapse
    m_dwTimer = GetTickCount();

    m_State = HOSTMIGRATIONSTATE_WAITINGFORHOST;
}


//--------------------------------------------------------------------------------------
// Name: ProcessMigrateMessage
// Desc: Handle a host migration message
//--------------------------------------------------------------------------------------
void CHostMigrationHelper::ProcessMigrateMessage( CMessage* pMsg, IN_ADDR addrFrom )
{
    // If we're not migrating, ignore the message
    if( m_State == HOSTMIGRATIONSTATE_NOTMIGRATING )
    {
        return;
    }

    MsgMigrate msg = pMsg->GetMigrate();
    CMessage msgReply( MSG_MIGRATE );

    // Is somebody else inviting us to migrate?
    if( msg.msgType == MsgMigrate::MIGRATE_HOSTING )
    {
        // Somebody's hosting. Do we expect someone to host?
        if( m_State == HOSTMIGRATIONSTATE_WAITINGFORHOST )
        {
            // Is the host the one we're looking for?
            if( !memcmp( &msg.info.hostAddress, &m_pNewHost->xnaddr, sizeof( XNADDR ) ) )
            {
                // It is! He's now the host. Let him know
                msgReply.GetMigrate().msgType = MsgMigrate::MIGRATE_MIGRATED;
                m_pSample->m_pHost = m_pNewHost;
                m_NewSessionInfo = msg.info;

                ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

                m_hr = XSessionMigrateHost(
                    m_pSample->m_Session.GetSessionHandle(),
                    XUSER_INDEX_NONE,
                    &m_NewSessionInfo,
                    &m_Overlapped );

                if( FAILED( m_hr ) )
                {
                    m_State = HOSTMIGRATIONSTATE_ERROR;
                }
                else
                {
                    m_State = HOSTMIGRATIONSTATE_HOSTMIGRATING;
                }
            }
            else
            {
                // Somebody else is trying to host. Tell him to stand by
                msgReply.GetMigrate().msgType = MsgMigrate::MIGRATE_STANDBY;
            }
            m_pSample->SendMessage( &msgReply, addrFrom );
        }
    }

    // Has somebody else migrated to us?
    if( m_State == HOSTMIGRATIONSTATE_WAITINGFORMIGREES )
    {
        if( msg.msgType == MsgMigrate::MIGRATE_MIGRATED )
        {
            // Find him in the list and flag him as having replied
            BOOL bAllreplied = TRUE;

            for( ClientInfoVec::iterator i = m_pSample->m_vecRemote.begin(); i != m_pSample->m_vecRemote.end(); i++ )
            {
                if( i->addr == addrFrom )
                {
                    i->bWantsToMigrate = TRUE;
                }

                bAllreplied = bAllreplied && i->bWantsToMigrate;
            }

            // If everybody's migrating, no need to stick around
            if( bAllreplied )
            {
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
    };


    FLOAT fCenterX = ( m_pSample->m_Font16.m_rcWindow.x2 - m_pSample->m_Font16.m_rcWindow.x1 ) / 2.0f;
    FLOAT fCenterY = ( m_pSample->m_Font16.m_rcWindow.y2 - m_pSample->m_Font16.m_rcWindow.y1 ) / 2.0f;
    m_pSample->m_Font16.DrawText( fCenterX, fCenterY, Sample::COLOR_TEXT,
                                  wstrMigState[ m_State ], ATGFONT_CENTER_X );

}
