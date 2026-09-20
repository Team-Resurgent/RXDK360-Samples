//--------------------------------------------------------------------------------------
// Session.cpp
//
// Code for handling session class logic.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgSignIn.h>
#include <AtgUtil.h>
#include "Session.h"
#include "Messages.h"
#include "Sessions.h"


//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
const DWORD MODIFY_FLAGS_ALLOWED = XSESSION_CREATE_USES_ARBITRATION |
    XSESSION_CREATE_INVITES_DISABLED |
    XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED |
    XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED;


//--------------------------------------------------------------------------------------
// Name: Session()
// Desc: Session constructor -- initialize values
//--------------------------------------------------------------------------------------
CSession::CSession()
{
    m_Parent = NULL;
    m_hSession = INVALID_HANDLE_VALUE;
    m_strSessionError = NULL;
    m_pRegistrationResults = NULL;
    m_bIsHost = FALSE;
    m_bUsingQoS = FALSE;
    m_nOwnerController = 0;
    m_dwSessionFlags = XSESSION_CREATE_LIVE_MULTIPLAYER_STANDARD;
    m_SessionState = SESSION_STATE_NONE;

    m_Slots[ SLOTS_TOTALPUBLIC    ] = PUBLICSLOTS;
    m_Slots[ SLOTS_TOTALPRIVATE   ] = PRIVATESLOTS;

    ZeroMemory( &m_SessionInfo, sizeof( m_SessionInfo ) );
}
CSession::CSession( DWORD nOwnerController, DWORD dwSessionFlags, BOOL bIsHost,
                    UINT iPublicSlots, UINT iPrivateSlots )
{
    m_Parent = NULL;
    m_hSession = INVALID_HANDLE_VALUE;
    m_strSessionError = NULL;
    m_pRegistrationResults = NULL;
    m_bIsHost = bIsHost;
    m_bUsingQoS = FALSE;
    m_nOwnerController = nOwnerController;
    m_SessionState = SESSION_STATE_NONE;

    m_Slots[ SLOTS_TOTALPUBLIC    ] = iPublicSlots;
    m_Slots[ SLOTS_TOTALPRIVATE   ] = iPrivateSlots;

    ZeroMemory( &m_SessionInfo, sizeof( m_SessionInfo ) );

    m_dwSessionFlags = dwSessionFlags;
}


//--------------------------------------------------------------------------------------
// Name: ~CSession()
// Desc: Session destructor -- clean up outstanding sessions
//--------------------------------------------------------------------------------------
CSession::~CSession()
{
    Cleanup();
}

//--------------------------------------------------------------------------------------
// Name: Cleanup()
// Desc: Clean up session
//--------------------------------------------------------------------------------------
VOID CSession::Cleanup()
{
    if( m_hSession != INVALID_HANDLE_VALUE )
    {
        StopQoSListener();
        XSessionDelete( m_hSession, NULL );
        XCloseHandle( m_hSession );
        m_hSession = INVALID_HANDLE_VALUE;
        m_SessionState = SESSION_STATE_NONE;
    }
}

//--------------------------------------------------------------------------------------
// Name: CreateSession()
// Desc: Creates a new session 
//--------------------------------------------------------------------------------------
VOID CSession::CreateSession()
{
    if( m_SessionState > SESSION_STATE_NONE )
    {
        ATG::FatalError( "CreateSession called on existing session!" );
    }

    // Configure the slots
    m_Slots[ SLOTS_FILLEDPUBLIC  ] = 0;
    m_Slots[ SLOTS_FILLEDPRIVATE ] = 0;

    if( m_bIsHost )
    {
        m_dwSessionFlags |= XSESSION_CREATE_HOST;
    }

    // Advertise the session
    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

    if( INVALID_HANDLE_VALUE != m_hSession )
    {
        CloseHandle( m_hSession );
        m_hSession = INVALID_HANDLE_VALUE;
    }

    DWORD ret = XSessionCreate(
        m_dwSessionFlags,
        m_nOwnerController,
        m_Slots[ SLOTS_TOTALPUBLIC ],
        m_Slots[ SLOTS_TOTALPRIVATE ],
        &m_SessionNonce,
        &m_SessionInfo,
        &m_Overlapped,
        &m_hSession );

    if( ret != ERROR_IO_PENDING )
    {
        ATG::FatalError( "XSessionCreate failed with error %d\n", ret );
    }
}


//--------------------------------------------------------------------------------------
// Name: ModifySessionFlags()
// Desc: Modify the session, to account for the host's correct flag settings (join)
//--------------------------------------------------------------------------------------
VOID CSession::ModifySessionFlags( DWORD flags )
{
    if( m_SessionState > SESSION_STATE_CREATING && m_SessionState < SESSION_STATE_DELETING )
    {
        if( m_Parent->GetGameType() != X_CONTEXT_GAME_TYPE_RANKED )
        {

            // turn the allowed modify flags off
            m_dwSessionFlags &= ~MODIFY_FLAGS_ALLOWED;
            // and get them from the flags input
            m_dwSessionFlags |= ( flags & MODIFY_FLAGS_ALLOWED );

            DWORD dwRet = XSessionModify(
                m_hSession,
                m_dwSessionFlags,
                m_Slots[ SLOTS_TOTALPUBLIC ],
                m_Slots[ SLOTS_TOTALPRIVATE ],
                NULL               // do it synchronously for simplicity; actual titles should
                );                 // be asynchronous

            if( dwRet != ERROR_SUCCESS )
            {
                HRESULT hr = XGetOverlappedExtendedError( NULL );

                ATG::FatalError( "Failed to modify session flags, error 0x%08x\n", hr );
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: StartQoSListener()
// Desc: Turn on the Quality of Service Listener for the current session
//--------------------------------------------------------------------------------------
VOID CSession::StartQoSListener( BYTE* data, UINT dataLen, DWORD bitsPerSec )
{
    DWORD flags = XNET_QOS_LISTEN_SET_DATA;
    if( bitsPerSec )
    {
        flags |= XNET_QOS_LISTEN_SET_BITSPERSEC;
    }
    if( !m_bUsingQoS )
    {
        flags |= XNET_QOS_LISTEN_ENABLE;
    }
    DWORD dwRet;
    dwRet = XNetQosListen( &( m_SessionInfo.sessionID ), data, dataLen, bitsPerSec, flags );
    if( ERROR_SUCCESS != dwRet )
    {
        ATG::FatalError( "Failed to start QoS listener, error 0x%08x\n", dwRet );
    }
    m_bUsingQoS = TRUE;
}


//--------------------------------------------------------------------------------------
// Name: StopQoSListener()
// Desc: Turn off the Quality of Service Listener for the current session
//--------------------------------------------------------------------------------------
VOID CSession::StopQoSListener()
{
    if( m_bUsingQoS )
    {
        DWORD dwRet;
        dwRet = XNetQosListen( &( m_SessionInfo.sessionID ), NULL, 0, 0,
                               XNET_QOS_LISTEN_RELEASE );
        if( ERROR_SUCCESS != dwRet )
        {
            ATG::DebugSpew( "Warning: Failed to stop QoS listener, error 0x%08x\n", dwRet );
        }
        m_bUsingQoS = FALSE;
    }
}


//--------------------------------------------------------------------------------------
// Name: IsSameSession()
// Desc: Returns true if this session info is for the existing session
//--------------------------------------------------------------------------------------
BOOL CSession::IsSameSession( const XSESSION_INFO& sessionInfo )
{
    if( memcmp( &( sessionInfo.sessionID ), &( m_SessionInfo.sessionID ),
                sizeof( m_SessionInfo.sessionID ) ) )
    {
        return FALSE;
    }
    return TRUE;

}


//--------------------------------------------------------------------------------------
// Name: AddLocalPlayers()
// Desc: Add players on the local console to the session
//--------------------------------------------------------------------------------------
VOID CSession::AddLocalPlayers( const ClientInfo* pClient )
{
    DWORD aIndices [ XUSER_MAX_COUNT ];
    BOOL abPrivate[ XUSER_MAX_COUNT ];

    for( UINT i = 0; i < pClient->cPlayers; i++ )
    {
        aIndices [ i ] = pClient->nController[ i ];
        abPrivate[ i ] = pClient->bInvited;
    }

    DWORD dwRet = XSessionJoinLocal(
        m_hSession,
        pClient->cPlayers,
        aIndices,
        abPrivate,
        NULL );

    if( dwRet != ERROR_SUCCESS )
    {
        HRESULT hr = XGetOverlappedExtendedError( NULL );

        ATG::FatalError( "Failed to add local users to session, error 0x%08x\n", hr );
    }
    else
    {
        UINT cPlayers = pClient->cPlayers;
        if( pClient->bInvited )
        {
            m_Slots[ SLOTS_FILLEDPRIVATE ] += pClient->cPlayers;
            cPlayers = 0;
            if( m_Slots[ SLOTS_FILLEDPRIVATE ] > m_Slots[ SLOTS_TOTALPRIVATE ] )
            {
                m_Slots[ SLOTS_FILLEDPRIVATE ] = m_Slots[ SLOTS_TOTALPRIVATE ];
                cPlayers = m_Slots[ SLOTS_FILLEDPRIVATE ] - m_Slots[ SLOTS_TOTALPRIVATE ];
            }
        }
        m_Slots[ SLOTS_FILLEDPUBLIC ] += cPlayers;
        if( m_Slots[ SLOTS_FILLEDPUBLIC ] > m_Slots[ SLOTS_TOTALPUBLIC ] )
        {
            ATG::FatalError( "Too many slots filled!\n" );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: AddRemotePlayers()
// Desc: Add players on a remote console to the session
//--------------------------------------------------------------------------------------
VOID CSession::AddRemotePlayers( const ClientInfo* pClient )
{
    XUID aXuids   [ XUSER_MAX_COUNT ];
    BOOL abPrivate[ XUSER_MAX_COUNT ];

    for( UINT i = 0; i < pClient->cPlayers; i++ )
    {
        aXuids   [ i ] = pClient->xuids[ i ];
        abPrivate[ i ] = pClient->bInvited;
    }

    DWORD dwRet = XSessionJoinRemote(
        m_hSession,
        pClient->cPlayers,
        aXuids,
        abPrivate,
        NULL );

    if( dwRet != ERROR_SUCCESS )
    {
        HRESULT hr = XGetOverlappedExtendedError( NULL );

        ATG::FatalError( "Failed to add remote users to session, error 0x%08x\n", hr );
    }
    else
    {
        UINT cPlayers = pClient->cPlayers;
        if( pClient->bInvited )
        {
            m_Slots[ SLOTS_FILLEDPRIVATE ] += pClient->cPlayers;
            cPlayers = 0;
            if( m_Slots[ SLOTS_FILLEDPRIVATE ] > m_Slots[ SLOTS_TOTALPRIVATE ] )
            {
                m_Slots[ SLOTS_FILLEDPRIVATE ] = m_Slots[ SLOTS_TOTALPRIVATE ];
                cPlayers = m_Slots[ SLOTS_FILLEDPRIVATE ] - m_Slots[ SLOTS_TOTALPRIVATE ];
            }
        }
        m_Slots[ SLOTS_FILLEDPUBLIC ] += cPlayers;
        if( m_Slots[ SLOTS_FILLEDPUBLIC ] > m_Slots[ SLOTS_TOTALPUBLIC ] )
        {
            ATG::FatalError( "Too many slots filled!\n" );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: RemoveLocalPlayers()
// Desc: Remove players on the local console from the session
//--------------------------------------------------------------------------------------
VOID CSession::RemoveLocalPlayers( const ClientInfo* pClient )
{
    DWORD aIndices[ XUSER_MAX_COUNT ];
    DWORD dwRet;

    for( UINT i = 0; i < pClient->cPlayers; i++ )
    {
        aIndices[ i ] = pClient->nController[ i ];

        // If we're in an arbitrated session and the game is in progress, we have someone
        // dropping in the middle of the session. Force his scores to zero and write stats
        if( ( m_dwSessionFlags & XSESSION_CREATE_USES_ARBITRATION ) &&
            ( ( SESSION_STATE_STARTING == m_SessionState ) ||
              ( SESSION_STATE_IN_GAME == m_SessionState ) ) )
        {
            // Note that the value for determining Team ID is a quick-and-dirty way of trying
            // to ensure that each user has a unique team ID, since this is an individual game.
            // Titles should use a more robust method of assigning team ID.

            XUSER_PROPERTY Skill[2];
            Skill[0].dwPropertyId = X_PROPERTY_RELATIVE_SCORE;
            Skill[0].value.nData = 0;
            Skill[0].value.type = XUSER_DATA_TYPE_INT32;
            Skill[1].dwPropertyId = X_PROPERTY_SESSION_TEAM;
            Skill[1].value.nData = ( LONG )
                ( ( pClient->xuids[ i ] >> 32 ) ^ ( pClient->xuids[ i ] & MAXDWORD ) );
            Skill[1].value.type = XUSER_DATA_TYPE_INT32;

            XSESSION_VIEW_PROPERTIES View;
            View.dwNumProperties = 2;
            View.dwViewId = X_STATS_VIEW_SKILL;
            View.pProperties = Skill;

            dwRet = XSessionWriteStats(
                m_hSession,
                aIndices[ i ],
                1,
                &View,
                NULL               // do it synchronously for simplicity; actual titles should
                );                 // be asynchronous

            if( dwRet != ERROR_SUCCESS )
            {
                HRESULT hr = XGetOverlappedExtendedError( NULL );

                ATG::FatalError( "Failed to write stats, error 0x%08x\n", hr );
            }
        }
    }

    dwRet = XSessionLeaveLocal(
        m_hSession,
        pClient->cPlayers,
        aIndices,
        NULL );

    if( dwRet != ERROR_SUCCESS )
    {
        HRESULT hr = XGetOverlappedExtendedError( NULL );

        ATG::FatalError( "Failed to remove local users from session, error 0x%08x\n", hr );
    }
    else
    {
        if( pClient->bInvited )
            m_Slots[ SLOTS_FILLEDPRIVATE ] = max( 0, m_Slots[ SLOTS_FILLEDPRIVATE ] - pClient->cPlayers );
        else
            m_Slots[ SLOTS_FILLEDPUBLIC ] = max( 0, m_Slots[ SLOTS_FILLEDPUBLIC ] - pClient->cPlayers );
    }
}


//--------------------------------------------------------------------------------------
// Name: RemoveRemotePlayers()
// Desc: Remove players on a remote console from the session
//--------------------------------------------------------------------------------------
VOID CSession::RemoveRemotePlayers( const ClientInfo* pClient )
{
    XUID aXuids[ XUSER_MAX_COUNT ];
    DWORD dwRet;

    for( UINT i = 0; i < pClient->cPlayers; i++ )
    {
        aXuids[ i ] = pClient->xuids[ i ];

        // If we're in an arbitrated session and the game is in progress, we have someone
        // dropping in the middle of the session. Force his scores to zero and write stats
        if( ( m_dwSessionFlags & XSESSION_CREATE_USES_ARBITRATION ) &&
            ( ( SESSION_STATE_STARTING == m_SessionState ) ||
              ( SESSION_STATE_IN_GAME == m_SessionState ) ) )
        {
            // Note that the value for determining Team ID is a quick-and-dirty way of trying
            // to ensure that each user has a unique team ID, since this is an individual game.
            // Titles should use a more robust method of assigning team ID.

            XUSER_PROPERTY Skill[2];
            Skill[0].dwPropertyId = X_PROPERTY_RELATIVE_SCORE;
            Skill[0].value.nData = 0;
            Skill[0].value.type = XUSER_DATA_TYPE_INT32;
            Skill[1].dwPropertyId = X_PROPERTY_SESSION_TEAM;
            Skill[1].value.nData = ( LONG )
                ( ( pClient->xuids[ i ] >> 32 ) ^ ( pClient->xuids[ i ] & MAXDWORD ) );
            Skill[1].value.type = XUSER_DATA_TYPE_INT32;

            XSESSION_VIEW_PROPERTIES View;
            View.dwNumProperties = 2;
            View.dwViewId = X_STATS_VIEW_SKILL;
            View.pProperties = Skill;

            dwRet = XSessionWriteStats(
                m_hSession,
                aXuids[ i ],
                1,
                &View,
                NULL               // do it synchronously for simplicity; actual titles should
                );                 // be asynchronous

            if( dwRet != ERROR_SUCCESS )
            {
                HRESULT hr = XGetOverlappedExtendedError( NULL );

                ATG::FatalError( "Failed to write stats, error 0x%08x\n", hr );
            }
        }
    }

    dwRet = XSessionLeaveRemote(
        m_hSession,
        pClient->cPlayers,
        aXuids,
        NULL );

    if( dwRet != ERROR_SUCCESS )
    {
        HRESULT hr = XGetOverlappedExtendedError( NULL );

        ATG::FatalError( "Failed to remove remote users from session, error 0x%08x\n", hr );
    }
    else
    {
        if( pClient->bInvited )
            m_Slots[ SLOTS_FILLEDPRIVATE ] = max( 0, m_Slots[ SLOTS_FILLEDPRIVATE ] - pClient->cPlayers );
        else
            m_Slots[ SLOTS_FILLEDPUBLIC ] = max( 0, m_Slots[ SLOTS_FILLEDPUBLIC ] - pClient->cPlayers );
    }
}


//--------------------------------------------------------------------------------------
// Name: RegisterForArbitration()
// Desc: Register self for arbitration
//--------------------------------------------------------------------------------------
VOID CSession::RegisterForArbitration()
{
    DWORD cbRegistrationResults = 0;
    m_pRegistrationResults = NULL;

    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

    // Call once to determine size of results buffer
    DWORD ret = XSessionArbitrationRegister(
        m_hSession,
        0,
        m_SessionNonce,
        &cbRegistrationResults,
        NULL,
        NULL );

    if( ( ret != ERROR_INSUFFICIENT_BUFFER ) || ( cbRegistrationResults == 0 ) )
    {
        ATG::FatalError( "Failed on first call to XSessionArbitrationRegister, hr=0x%08x\n", ret );
    }

    m_pRegistrationResults = ( PXSESSION_REGISTRATION_RESULTS )new BYTE[ cbRegistrationResults ];
    if( m_pRegistrationResults == NULL )
    {
        ATG::FatalError( "Failed to allocate buffer.\n" );
    }


    ret = XSessionArbitrationRegister(
        m_hSession,
        0,
        m_SessionNonce,
        &cbRegistrationResults,
        m_pRegistrationResults,
        &m_Overlapped );

    if( ret != ERROR_IO_PENDING )
    {
        HRESULT hr = XGetOverlappedExtendedError( NULL );

        ATG::FatalError( "Failed to register the joined session, hr=0x%08x\n", hr );
    }

    m_SessionState = SESSION_STATE_REGISTERING;
}


//--------------------------------------------------------------------------------------
// Name: SwitchToState()
// Desc: Changes to a new session state and performs initialization for the new state
//--------------------------------------------------------------------------------------
VOID CSession::SwitchToState( SESSION_STATE newState )
{
    // Clean up from the previous state
    switch( m_SessionState )
    {
        case SESSION_STATE_IN_GAME:
        case SESSION_STATE_FINISHED:
            // Clear out our slots and update our presence info
            m_Slots[ SLOTS_FILLEDPUBLIC  ] = 0;
            m_Slots[ SLOTS_FILLEDPRIVATE ] = 0;

            m_strSessionError = NULL;
            break;

        case SESSION_STATE_REGISTERED:
            if( m_pRegistrationResults )
            {
                delete[] m_pRegistrationResults;
                m_pRegistrationResults = NULL;
            }
            break;

        case SESSION_STATE_DELETING:
            ZeroMemory( &m_SessionInfo, sizeof( m_SessionInfo ) );
            break;

        case SESSION_STATE_NONE:
        case SESSION_STATE_CREATING:
        case SESSION_STATE_IDLE:
        case SESSION_STATE_WAITING_FOR_REGISTRATION:
        case SESSION_STATE_REGISTERING:
        case SESSION_STATE_STARTING:
        case SESSION_STATE_ENDING:
        default:
            break;
    }

    // Initialize the next state
    switch( newState )
    {
        case SESSION_STATE_CREATING:
            CreateSession();
            break;

        case SESSION_STATE_REGISTERING:
            RegisterForArbitration();
            break;

        case SESSION_STATE_STARTING:
            ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );
            XSessionStart( m_hSession, 0, &m_Overlapped );
            break;

        case SESSION_STATE_ENDING:
            ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );
            XSessionEnd( m_hSession, &m_Overlapped );
            break;

        case SESSION_STATE_DELETING:
            ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );
            if( m_hSession != INVALID_HANDLE_VALUE )
            {
                StopQoSListener();
                XSessionDelete( m_hSession, &m_Overlapped );
            }
            break;

        case SESSION_STATE_NONE:
        case SESSION_STATE_IDLE:
        case SESSION_STATE_WAITING_FOR_REGISTRATION:
        case SESSION_STATE_REGISTERED:
        case SESSION_STATE_IN_GAME:
        case SESSION_STATE_FINISHED:
        default:
            break;
    }

    m_SessionState = newState;
}
