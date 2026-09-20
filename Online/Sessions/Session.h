//--------------------------------------------------------------------------------------
// Session.h
//
// Session class for sessions sample
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#ifndef SESSION_H
#define SESSION_H

#include "ClientInfo.h"
#include <map>

// forward declarations
class Sample;

//--------------------------------------------------------------------------------------
// Global constants
//--------------------------------------------------------------------------------------
const DWORD PUBLICSLOTS       = 8;     // Default number of public slots
const DWORD PRIVATESLOTS      = 8;     // Default number of private slots

// valid session states
enum SESSION_STATE
{
    SESSION_STATE_NONE,
    SESSION_STATE_CREATING,
    SESSION_STATE_IDLE,
    SESSION_STATE_WAITING_FOR_REGISTRATION,
    SESSION_STATE_REGISTERING,
    SESSION_STATE_REGISTERED,
    SESSION_STATE_STARTING,
    SESSION_STATE_IN_GAME,
    SESSION_STATE_ENDING,
    SESSION_STATE_FINISHED,
    SESSION_STATE_DELETING
};

enum SESSION_NOTIFY
{
    SESSION_NOTIFY_CREATED,
    SESSION_NOTIFY_REGISTERED,
    SESSION_NOTIFY_FAIL_REGISTER,
    SESSION_NOTIFY_STARTED,
    SESSION_NOTIFY_FAIL_START,
    SESSION_NOTIFY_ENDED,
    SESSION_NOTIFY_FAIL_END,
    SESSION_NOTIFY_DELETED,
};

// slot types for the session
enum SLOTS
{
    SLOTS_TOTALPUBLIC,
    SLOTS_TOTALPRIVATE,
    SLOTS_FILLEDPUBLIC,
    SLOTS_FILLEDPRIVATE,
    SLOTS_MAX
};

//--------------------------------------------------------------------------------------
// Name: class CSession
// Desc: Class to control functionality for sessions created in this sample.
//--------------------------------------------------------------------------------------
class CSession
{
    HANDLE             m_hSession;                  // Session handle
    BOOL               m_bIsHost;                   // Is hosting
    BOOL               m_bUsingQoS;                 // Is the QoS listener enabled
    XSESSION_INFO      m_SessionInfo;               // Session ID, key, and host address
    ULONGLONG          m_SessionNonce;              // Nonce of the session
    DWORD              m_dwSessionFlags;            // Session creation flags
    DWORD              m_nOwnerController;          // Which controller created the session
    WCHAR*             m_strSessionError;           // Error message for current session
    XOVERLAPPED        m_Overlapped;                // Overlapped task data

    // public/private slots for the session
    UINT               m_Slots[ SLOTS_MAX ];        // Filled/open slots

    // Arbitration registration
    PXSESSION_REGISTRATION_RESULTS m_pRegistrationResults;

    SESSION_STATE      m_SessionState;
    Sample*            m_Parent;


public:

    CSession();
    CSession( DWORD nOwnerController, DWORD dwSessionFlags, BOOL bIsHost,
              UINT iPublicSlots = PUBLICSLOTS, UINT iPrivateSlots = PRIVATESLOTS );
    ~CSession();

    VOID Cleanup();

    VOID SetParent( Sample* parent ) { m_Parent = parent; }
    VOID CreateSession();

    VOID ModifySessionFlags( DWORD flags );

    HANDLE  GetSessionHandle()  { return m_hSession; }

    BOOL IsSameSession( const XSESSION_INFO& sessionInfo );

    const XSESSION_INFO& GetSessionInfo()                   { return m_SessionInfo; }
    VOID SetSessionInfo( const XSESSION_INFO& sessionInfo )
    {
        m_SessionInfo = sessionInfo;
    }

    DWORD   GetSessionFlags()                   { return m_dwSessionFlags; }
    BOOL    HasSessionFlags( DWORD flags )      { return m_dwSessionFlags & flags; }
    VOID    SetSessionFlags( DWORD flags, BOOL clear = FALSE )
    {
        if( clear )
            m_dwSessionFlags = flags;
        else
            m_dwSessionFlags |= flags;
    }
    VOID    FlipSessionFlags( DWORD flags )     { m_dwSessionFlags ^= flags; }
    VOID    ClearSessionFlags( DWORD flags )    { m_dwSessionFlags &= ~flags; }

    BOOL IsHost()                   { return m_bIsHost; }
    VOID SetHost( BOOL isHost )     { m_bIsHost = isHost; }

    ULONGLONG   GetSessionNonce()                   { return m_SessionNonce; }
    VOID        SetSessionNonce( ULONGLONG nonce )  { m_SessionNonce = nonce; }
    DWORD       GetSessionOwner()                   { return m_nOwnerController; }
    VOID        SetSessionOwner( DWORD nonce )      { m_nOwnerController = nonce; }

    const WCHAR*    GetSessionError()                   { return m_strSessionError; }
    VOID            SetSessionError( WCHAR* error )     { m_strSessionError = error; }

    const PXSESSION_REGISTRATION_RESULTS GetRegistrationResults()
                                    { return m_pRegistrationResults; }

    UINT GetSessionSlots( SLOTS slotNum )               { return m_Slots[ slotNum ]; }
    VOID SetSessionSlots( SLOTS slotNum, UINT count )   { m_Slots[ slotNum ] = count; }

    VOID StartQoSListener( BYTE* data, UINT dataLen, DWORD bitsPerSec );
    VOID StopQoSListener();

    VOID Update();
    VOID UpdateCreating();
    VOID UpdateRegistering();
    VOID UpdateStarting();
    VOID UpdateEnding();
    VOID UpdateDeleting();

    SESSION_STATE GetSessionState() { return m_SessionState; }
    VOID SwitchToState( SESSION_STATE newState );

    VOID AddLocalPlayers( const ClientInfo* pClient );
    VOID AddRemotePlayers( const ClientInfo* pClient );
    VOID RemoveLocalPlayers( const ClientInfo* pClient );
    VOID RemoveRemotePlayers( const ClientInfo* pClient );

    VOID RegisterForArbitration();
};

#endif // SESSION_H
