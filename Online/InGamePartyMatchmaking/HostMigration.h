//--------------------------------------------------------------------------------------
// HostMigration.h
//
// Contains helpers and data for processing host migration
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "XPlat.h"

// forward declarations
class CMessage;
class Sample;
class SessionManager;

class CHostMigrationHelper
{
public:
    CHostMigrationHelper();
    void BeginHostMigration( Sample* pSample, SessionManager* pSessionMgr );
    HRESULT UpdateHostMigration( void );
    void RenderHostMigration( void );

    // Handle a host migration message
    void ProcessMigrateMessage( CMessage* pMsg, IN_ADDR addrFrom );

private:

    // Current state
    enum HostMigrationState
    {
        HOSTMIGRATIONSTATE_NOTMIGRATING,
        HOSTMIGRATIONSTATE_STARTING,
        HOSTMIGRATIONSTATE_HOSTMIGRATING,
        HOSTMIGRATIONSTATE_ERROR,
        HOSTMIGRATIONSTATE_WAITINGFORMIGREES,
        HOSTMIGRATIONSTATE_WAITINGFORHOST,
    };

    // Constants
    static const DWORD HOSTMIGRATION_RETRYINTERVAL  = 3000;  // 3s
    static const DWORD HOSTMIGRATION_MAXRETRIES     = 3;
    static const DWORD HOSTMIGRATION_MAXWAITFORHOST = 10000; // 10s

    ////////////////////////////////////////////////////////
    //
    // Helper functions
    //
    ////////////////////////////////////////////////////////

    // Find a new host
    ClientInfo* SelectNewHost( void );

    // Become the new host
    void BeginHosting( void );

    // Attach to a new host
    void SwitchToNewHost( void );

    // After migration on Live, start notifying peers we're hosting
    void BeginNotifyingClients( void );

    // Tell remote machines to migrate to me
    void NotifyClientsToMigrate( void ); 

    // Switch to new state
    void SwitchToState( HostMigrationState newState );

    // Finish migrating
    void EndMigration( void );

    // Suppress assignment operator
    CHostMigrationHelper& operator = ( const CHostMigrationHelper& ref );

    ////////////////////////////////////////////////////////
    //
    // Members
    //
    ////////////////////////////////////////////////////////

    HostMigrationState m_State;

    static const char* m_astrHostMigrationStates[];

    // State of application
    DWORD m_AppState;

    // Pointer to application
    Sample* m_pSample;

    // Pointer to the session we're migrating
    SessionManager* m_pSessionMgr;

    // Pointer to new host 
    ClientInfo* m_pNewHost;

    // Error code
    HRESULT m_hr;

    // Timer
    DWORD m_dwTimer;

    // Number of times notification has been sent
    INT m_nNotificationRetries;
};
