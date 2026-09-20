//--------------------------------------------------------------------------------------
// HostMigration.h
//
// Contains helpers and data for processing host migration
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

class CHostMigrationHelper
{
public:
            CHostMigrationHelper() : m_State( HOSTMIGRATIONSTATE_NOTMIGRATING )
            {
            }
    void    BeginHostMigration( Sample* pSample );
    void    UpdateHostMigration( void );
    void    RenderHostMigration( void );

    // Handle a host migration message
    void    ProcessMigrateMessage( CMessage* pMsg, IN_ADDR addrFrom );

private:

    // Constants
    static const DWORD HOSTMIGRATION_RETRYINTERVAL  = 1000;  // 1s
    static const DWORD HOSTMIGRATION_MAXRETRIES     = 10;
    static const DWORD HOSTMIGRATION_MAXWAITFORHOST = 10000; // 10s

    ////////////////////////////////////////////////////////
    //
    // Helper functions
    //
    ////////////////////////////////////////////////////////

    // Find a new host
    ClientInfo* SelectNewHost( void );

    // Become the new host
    void    BeginHosting( void );

    // Attach to a new host
    void    SwitchToNewHost( void );

    // After migration on Live, start notifying peers we're hosting
    void    BeginNotifyingClients( void );

    // Tell remote machines to migrate to me
    void    NotifyClientsToMigrate( void );

    // Finish migrating
    void    EndMigration( void );

    ////////////////////////////////////////////////////////
    //
    // Members
    //
    ////////////////////////////////////////////////////////

    // Current state
    enum
    {
        HOSTMIGRATIONSTATE_NOTMIGRATING,
        HOSTMIGRATIONSTATE_STARTING,
        HOSTMIGRATIONSTATE_HOSTMIGRATING,
        HOSTMIGRATIONSTATE_ERROR,
        HOSTMIGRATIONSTATE_WAITINGFORMIGREES,
        HOSTMIGRATIONSTATE_WAITINGFORHOST,
    } m_State;

    // State of application
    DWORD m_AppState;

    // Pointer to application
    Sample* m_pSample;

    // Pointer to new host 
    ClientInfo* m_pNewHost;

    // Error code
    HRESULT m_hr;

    // Overlapped IO
    XOVERLAPPED m_Overlapped;

    // Session info
    XSESSION_INFO m_NewSessionInfo;

    // Timer
    DWORD m_dwTimer;

    // Number of times notification has been sent
    INT m_nNotificationRetries;
};
