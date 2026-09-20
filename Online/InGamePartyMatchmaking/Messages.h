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

//-----------------------------------------------------------------------------
// Message IDs
//
// A "host" is the player who started the session.
// A "client" is a potential player. A client is not currently in a session.
// A "player" is anyone in a session.
//-----------------------------------------------------------------------------
enum
{                               // From     To
    MSG_QUERY_SESSION,          // client   host
    MSG_RESP_SESSION,           // host     client
    MSG_JOIN_SESSION,           // client   host
    MSG_JOIN_SESSION_PARTY,     // client   host
    MSG_JOIN_RESPONSE,          // host     client
    MSG_JOIN_RESPONSE_PARTY,    // host     client
    MSG_PLAYER_INFO,            // host     player
    MSG_WAVE,                   // player   player
    MSG_START_SESSION,          // host     client
    MSG_END_SESSION,            // host     client
    MSG_FOUND_SESSION,          // host     client
    MSG_REGISTER,               // host     client
    MSG_REGISTERED,             // client   host
    MSG_SCORE_POINT,            // player   player
    MSG_POINT_TOTAL,            // player   player
    MSG_HEARTBEAT,              // player   player
    MSG_GOODBYE,                // player   player
    MSG_VOICE,                  // player   player
    MSG_MUTE,                   // player   player
    MSG_MIGRATE,                // host     client
    MSG_LEFT_SESSION,           // host     client
};

//-----------------------------------------------------------------------------
// Endian swapping (needed for interoperability between systems
//-----------------------------------------------------------------------------
struct SEndianSwapData
{
    ptrdiff_t m_nOffset;
    size_t    m_nBytes;
    UINT      m_nArraySize;

    SEndianSwapData( ptrdiff_t nOffset, size_t nBytes, UINT nArraySize = 1 ) :
        m_nOffset( nOffset ), m_nBytes( nBytes ), m_nArraySize( nArraySize ) {}
};

// friendly helper macros
#define SWAP_DATA(_struct, _member) \
    SEndianSwapData( offsetof( _struct, _member ), sizeof( _struct()._member ), 1 ),

#define SWAP_DATA_ARRAY(_struct, _member, _size) \
    SEndianSwapData( offsetof( _struct, _member ), sizeof( _struct()._member[0] ), _size ),

#define SWAP_DATA_TYPE(_struct, _member, _type, _size ) \
    SEndianSwapData( offsetof( _struct, _member ), sizeof( _type ), _size ),

#define DECLARE_SWAP_DATA() static const SEndianSwapData Swap[]; static const UINT SwapCount;

#define BEGIN_SWAP_DATA(_struct) __declspec( selectany ) const SEndianSwapData _struct::Swap[] = {
    
#define END_SWAP_DATA(_struct) }; __declspec( selectany ) const UINT _struct::SwapCount = dimensionof( _struct::Swap );

#define dimensionof( a ) ( sizeof( a ) / sizeof( a[ 0 ] ) )


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
// Sent from client to host requesting details about a session
//-----------------------------------------------------------------------------
struct MsgQuerySession
{
    BOOL bInvited;
};

//-----------------------------------------------------------------------------
// Sent from a host to a client to respond with details about a session
//-----------------------------------------------------------------------------
struct MsgResponseSession
{
    enum
    {
        QUERYRESPONSE_HOSTING,
        QUERYRESPONSE_NOTHOSTING
    } Response;

    BOOL          bInvited;
    ULONGLONG     qwSessionNonce;
    DWORD         dwFlags;
    DWORD         dwMaxPublicSlots;
    DWORD         dwMaxPrivateSlots;
    DWORD         dwGameType;
    DWORD         dwGameMode;
    XSESSION_INFO session_info;

    #ifdef LIVE_ON_WINDOWS
    DECLARE_SWAP_DATA();
    #endif
};

#ifdef LIVE_ON_WINDOWS
BEGIN_SWAP_DATA( MsgResponseSession )
    SWAP_DATA( MsgResponseSession, bInvited )
    SWAP_DATA( MsgResponseSession, qwSessionNonce )
    SWAP_DATA( MsgResponseSession, dwFlags )
    SWAP_DATA( MsgResponseSession, dwMaxPublicSlots )
    SWAP_DATA( MsgResponseSession, dwMaxPrivateSlots )
    SWAP_DATA( MsgResponseSession, dwGameType )
    SWAP_DATA( MsgResponseSession, dwGameMode )
END_SWAP_DATA( MsgResponseSession )
#endif


//-----------------------------------------------------------------------------
// Sent from a client to a host to join a session
//-----------------------------------------------------------------------------
struct MsgJoinSession
{
    ULONGLONG id;
    UINT      cPlayers;
    WCHAR     strGamertags[ MAX_USER_COUNT ][ XUSER_NAME_SIZE ];  // player who wants to join
    DWORD     nController[ MAX_USER_COUNT ];
    XUID      xuids[ MAX_USER_COUNT ];
    BOOL      bHasVoice[ MAX_USER_COUNT ];
    BOOL      bInvited;

    #ifdef LIVE_ON_WINDOWS
    DECLARE_SWAP_DATA();
    #endif
};


#ifdef LIVE_ON_WINDOWS
BEGIN_SWAP_DATA( MsgJoinSession )
    SWAP_DATA( MsgJoinSession, id )
    SWAP_DATA( MsgJoinSession, cPlayers )
    SWAP_DATA_TYPE( MsgJoinSession, strGamertags, WCHAR, MAX_USER_COUNT * XUSER_NAME_SIZE )
    SWAP_DATA_ARRAY( MsgJoinSession, nController, MAX_USER_COUNT )
    SWAP_DATA_ARRAY( MsgJoinSession, xuids, MAX_USER_COUNT )
    SWAP_DATA_ARRAY( MsgJoinSession, bHasVoice, MAX_USER_COUNT )
    SWAP_DATA( MsgJoinSession, bInvited )
END_SWAP_DATA( MsgJoinSession )
#endif


//-----------------------------------------------------------------------------
// Sent from a Presence session host to a host to join all party members
// to the session
//-----------------------------------------------------------------------------
struct MsgJoinSessionParty
{
    UINT      cPlayers;
    BOOL      bInvited;

    #ifdef LIVE_ON_WINDOWS
    DECLARE_SWAP_DATA();
    #endif
};

#ifdef LIVE_ON_WINDOWS
BEGIN_SWAP_DATA( MsgJoinSessionParty )
    SWAP_DATA( MsgJoinSessionParty, cPlayers )
    SWAP_DATA( MsgJoinSessionParty, bInvited )
END_SWAP_DATA( MsgJoinSessionParty )
#endif


//-----------------------------------------------------------------------------
// Sent from host to clients telling them about a found matchmaking session
//-----------------------------------------------------------------------------
struct MsgFoundSession
{
    XSESSION_INFO info;
    BOOL bInvited;

    #ifdef LIVE_ON_WINDOWS
    DECLARE_SWAP_DATA();
    #endif
};

#ifdef LIVE_ON_WINDOWS
BEGIN_SWAP_DATA( MsgFoundSession )
    SWAP_DATA( MsgFoundSession, bInvited )
END_SWAP_DATA( MsgFoundSession )
#endif


//-----------------------------------------------------------------------------
// Sent from one player to another to indicate that players
// have left the session
//-----------------------------------------------------------------------------
struct MsgGoodbye
{
    UINT      cXuids;
    XUID      xuids[ MAX_USER_COUNT ];

    #ifdef LIVE_ON_WINDOWS
    DECLARE_SWAP_DATA();
    #endif
};

#ifdef LIVE_ON_WINDOWS
BEGIN_SWAP_DATA( MsgGoodbye )
    SWAP_DATA( MsgGoodbye, cXuids )
    SWAP_DATA_ARRAY( MsgGoodbye, xuids, MAX_USER_COUNT )
END_SWAP_DATA( MsgGoodbye )
#endif


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

    DWORD     nVictoryPoints;  // Number of points for victory
    DWORD     nMap;

    // Info about the host
    UINT      cPlayers;
    WCHAR     strGamertags[ MAX_USER_COUNT ][ XUSER_NAME_SIZE ];  
    XUID      xuids[ MAX_USER_COUNT ];
    BOOL      bHasVoice[ MAX_USER_COUNT ];
    UINT      cMaxPublicSlots;
    UINT      cMaxPrivateSlots;
    UINT      cFilledPublicSlots;
    UINT      cFilledPrivateSlots;
    DWORD     dwSessionFlags;

    #ifdef LIVE_ON_WINDOWS
    DECLARE_SWAP_DATA();
    #endif
};


#ifdef LIVE_ON_WINDOWS
BEGIN_SWAP_DATA( MsgJoinResponse )
    SWAP_DATA( MsgJoinResponse, Response )
    SWAP_DATA( MsgJoinResponse, id )
    SWAP_DATA( MsgJoinResponse, Nonce )
    SWAP_DATA( MsgJoinResponse, nVictoryPoints )
    SWAP_DATA( MsgJoinResponse, nMap )
    SWAP_DATA( MsgJoinResponse, cPlayers )
    SWAP_DATA_TYPE( MsgJoinResponse, strGamertags, WCHAR, MAX_USER_COUNT * XUSER_NAME_SIZE )
    SWAP_DATA_ARRAY( MsgJoinResponse, xuids, MAX_USER_COUNT )
    SWAP_DATA_ARRAY( MsgJoinResponse, bHasVoice, MAX_USER_COUNT )
    SWAP_DATA( MsgJoinResponse, cMaxPublicSlots )
    SWAP_DATA( MsgJoinResponse, cMaxPrivateSlots )
    SWAP_DATA( MsgJoinResponse, cFilledPublicSlots )
    SWAP_DATA( MsgJoinResponse, cFilledPrivateSlots )
    SWAP_DATA( MsgJoinResponse, dwSessionFlags )
END_SWAP_DATA( MsgJoinResponse )
#endif


//-----------------------------------------------------------------------------
// Sent from a host to a client Presence session host 
// to respond to a join request for a party
//-----------------------------------------------------------------------------
struct MsgJoinResponseParty
{
    enum
    {
        JOINRESPONSEPARTY_APPROVED,
        JOINRESPONSEPARTY_SESSIONFULL,
        JOINRESPONSEPARTY_SESSIONINPROGRESS,
        JOINRESPONSEPARTY_NOTHOSTING
    } Response;

    #ifdef LIVE_ON_WINDOWS
    DECLARE_SWAP_DATA();
    #endif
};

#ifdef LIVE_ON_WINDOWS
BEGIN_SWAP_DATA( MsgJoinResponseParty )
    SWAP_DATA( MsgJoinResponseParty, Response )
END_SWAP_DATA( MsgJoinResponseParty )
#endif


//-----------------------------------------------------------------------------
// Sent from a host to a client to describe the players on a remote client
//-----------------------------------------------------------------------------
struct MsgPlayerInfo
{
    ULONGLONG id;
    XNADDR    xnaddr;
    UINT      cPlayers;
    BOOL      bInvited;
    WCHAR     strGamertags[ MAX_USER_COUNT ][ XUSER_NAME_SIZE ];  // player who wants to join
    XUID      xuids[ MAX_USER_COUNT ];
    BOOL      bHasVoice[ MAX_USER_COUNT ];

    #ifdef LIVE_ON_WINDOWS
    DECLARE_SWAP_DATA();
    #endif
};

#ifdef LIVE_ON_WINDOWS
BEGIN_SWAP_DATA( MsgPlayerInfo )
    SWAP_DATA( MsgPlayerInfo, id )
    SWAP_DATA( MsgPlayerInfo, cPlayers )
    SWAP_DATA( MsgPlayerInfo, bInvited )
    SWAP_DATA_TYPE( MsgPlayerInfo, strGamertags, WCHAR, MAX_USER_COUNT * XUSER_NAME_SIZE )
    SWAP_DATA_ARRAY( MsgPlayerInfo, xuids, MAX_USER_COUNT )
    SWAP_DATA_ARRAY( MsgPlayerInfo, bHasVoice, MAX_USER_COUNT )
END_SWAP_DATA( MsgPlayerInfo )
#endif


//-----------------------------------------------------------------------------
// Sent from a player to a player to wave
//-----------------------------------------------------------------------------
struct MsgWave
{
    ULONGLONG id;
    XUID xuid;

    #ifdef LIVE_ON_WINDOWS
    DECLARE_SWAP_DATA();
    #endif
};

#ifdef LIVE_ON_WINDOWS
BEGIN_SWAP_DATA( MsgWave )
    SWAP_DATA( MsgWave, id )
    SWAP_DATA( MsgWave, xuid )
END_SWAP_DATA( MsgWave )
#endif


//-----------------------------------------------------------------------------
// Sent from a player to a player to score a point
//-----------------------------------------------------------------------------
struct MsgScorePoint
{
    ULONGLONG id;
    XUID      xuid;

    #ifdef LIVE_ON_WINDOWS
    DECLARE_SWAP_DATA();
    #endif
};


#ifdef LIVE_ON_WINDOWS
BEGIN_SWAP_DATA( MsgScorePoint )
    SWAP_DATA( MsgScorePoint, id )
    SWAP_DATA( MsgScorePoint, xuid )
END_SWAP_DATA( MsgScorePoint )
#endif


//-----------------------------------------------------------------------------
// Sent from a player to a player to update the point total for a player
//-----------------------------------------------------------------------------
struct MsgPointTotal
{
    ULONGLONG id;
    XUID      xuid;
    DWORD     nPoints;

    #ifdef LIVE_ON_WINDOWS
    DECLARE_SWAP_DATA();
    #endif
};


#ifdef LIVE_ON_WINDOWS
BEGIN_SWAP_DATA( MsgPointTotal )
    SWAP_DATA( MsgPointTotal, id )
    SWAP_DATA( MsgPointTotal, xuid )
    SWAP_DATA( MsgPointTotal, nPoints )
END_SWAP_DATA( MsgPointTotal )
#endif


//-----------------------------------------------------------------------------
// Sent from a player to a player to transmit voice data
//-----------------------------------------------------------------------------
struct MsgVoice
{
    ULONGLONG id;
    BYTE bData[ MAX_VDP_PACKET ];
    WORD wSize;

    #ifdef LIVE_ON_WINDOWS
    DECLARE_SWAP_DATA();
    #endif
};


#ifdef LIVE_ON_WINDOWS
BEGIN_SWAP_DATA( MsgVoice )
    SWAP_DATA( MsgVoice, id )
    SWAP_DATA( MsgVoice, wSize )
END_SWAP_DATA( MsgVoice )
#endif


//-----------------------------------------------------------------------------
// Sent from a would-be host to a player
//-----------------------------------------------------------------------------
struct MsgMigrate
{
    enum
    {
        MIGRATE_HOSTING,  // I am now hosting
        MIGRATE_MIGRATED, // You're now my host
        MIGRATE_STANDBY   // I think someone else should host, hang on
    } msgType;
    
    ULONGLONG id;
    XSESSION_INFO info;

    #ifdef LIVE_ON_WINDOWS
    DECLARE_SWAP_DATA();
    #endif
};


#ifdef LIVE_ON_WINDOWS
BEGIN_SWAP_DATA( MsgMigrate )
    SWAP_DATA( MsgMigrate, id )
    SWAP_DATA( MsgMigrate, msgType )
END_SWAP_DATA( MsgMigrate)
#endif


//--------------------------------------------------------------------------------------
// Name: class CMessage
// Desc: Generic message, owner of all other messages
//--------------------------------------------------------------------------------------
class CMessage
{
    WORD        m_cbGameData;
    BYTE        m_byMessageID;
    XNKID       m_sessionID;
    UINT        m_cRef;

    union
    {
        MsgQuerySession         m_QuerySession;         // query session message
        MsgResponseSession      m_RespSession;          // response session message
        MsgJoinSession          m_JoinSession;          // join request message
        MsgJoinSessionParty     m_JoinSessionParty;     // join request message for a party
        MsgFoundSession         m_FoundSession;         // found session message
        MsgJoinResponse         m_JoinResponse;         // join response message
        MsgJoinResponseParty    m_JoinResponseParty;    // join response message for a party
        MsgPlayerInfo           m_PlayerInfo;           // information about a player on a specific box
        MsgWave                 m_Wave;                 // wave message
        MsgScorePoint           m_ScorePoint;           // score a point
        MsgPointTotal           m_PointTotal;           // update point total
        MsgVoice                m_Voice;                // transmit voice data
        MsgMigrate              m_Migrate;              // host migration status messages
        MsgGoodbye              m_Goodbye;              // player leaving session message
    };

    // validate message ID
    VOID AssertID( BYTE byMessageID ) { assert( m_byMessageID == byMessageID ); }

public:

    explicit CMessage( BYTE byMessageID = 0 ) : 
        m_byMessageID( byMessageID ),
        m_cRef( 0 )
    {
    }

    VOID AddRef()
    {
        ++m_cRef;       
    }

    VOID Release()
    {
        if( --m_cRef )
        {
            delete this;
        }
    }

    BYTE GetID() const 
    { 
        return m_byMessageID; 
    }

    XNKID GetSessionID() const
    {
        return m_sessionID;
    }

    VOID SetSessionID( const XNKID& sessionID )
    {
        memcpy_s( &m_sessionID, sizeof( XNKID ), &sessionID, sizeof( XNKID ) );
    }

    WORD GetGameDataSize() const
    {
        WORD ret  = sizeof( m_byMessageID );
             ret += sizeof( m_sessionID );
             ret += sizeof( m_cRef );

        switch( m_byMessageID )
        {
            case MSG_QUERY_SESSION:         ret += sizeof( m_QuerySession );        break;
            case MSG_RESP_SESSION:          ret += sizeof( m_RespSession );         break;
            case MSG_JOIN_SESSION_PARTY:    ret += sizeof( m_JoinSessionParty );    break;
            case MSG_JOIN_SESSION:          ret += sizeof( m_JoinSession );         break;
            case MSG_FOUND_SESSION:         ret += sizeof( m_FoundSession );        break;
            case MSG_JOIN_RESPONSE:         ret += sizeof( m_JoinResponse );        break;
            case MSG_JOIN_RESPONSE_PARTY:   ret += sizeof( m_JoinResponseParty );   break;
            case MSG_PLAYER_INFO:           ret += sizeof( m_PlayerInfo );          break;
            case MSG_START_SESSION:         /* no data */                           break;
            case MSG_END_SESSION:           /* no data */                           break;
            case MSG_REGISTER:              /* no data */                           break;
            case MSG_REGISTERED:            /* no data */                           break;
            case MSG_WAVE:                  ret += sizeof( m_Wave );                break;
            case MSG_SCORE_POINT:           ret += sizeof( m_ScorePoint );          break;
            case MSG_POINT_TOTAL:           ret += sizeof( m_PointTotal );          break; 
            case MSG_HEARTBEAT:             /* no data */                           break;
            case MSG_GOODBYE:               ret += sizeof( m_Goodbye );             break;
            case MSG_VOICE:                 ret += sizeof( ULONGLONG );             break;
            case MSG_MIGRATE:               ret += sizeof( MsgMigrate );            break;
            case MSG_LEFT_SESSION:          /* no data */                           break;
        }

        return ret;
    }

    // retrieve specific messages
    MsgQuerySession&        GetQuerySession()       { AssertID(MSG_QUERY_SESSION);      return m_QuerySession; }
    MsgResponseSession&     GetRespSession()        { AssertID(MSG_RESP_SESSION);       return m_RespSession; }
    MsgJoinSession&         GetJoinSession()        { AssertID(MSG_JOIN_SESSION);       return m_JoinSession; }
    MsgJoinSessionParty&    GetJoinSessionParty()   { AssertID(MSG_JOIN_SESSION_PARTY); return m_JoinSessionParty; }
    MsgFoundSession&        GetFoundSession()       { AssertID(MSG_FOUND_SESSION);      return m_FoundSession; }
    MsgJoinResponse&        GetJoinResponse()       { AssertID(MSG_JOIN_RESPONSE);      return m_JoinResponse; }
    MsgJoinResponseParty&   GetJoinResponseParty()  { AssertID(MSG_JOIN_RESPONSE_PARTY);return m_JoinResponseParty; }
    MsgPlayerInfo&          GetPlayerInfo()         { AssertID(MSG_PLAYER_INFO);        return m_PlayerInfo; }
    MsgWave&                GetWave()               { AssertID(MSG_WAVE);               return m_Wave; }
    MsgScorePoint&          GetScorePoint()         { AssertID(MSG_SCORE_POINT);        return m_ScorePoint; }
    MsgPointTotal&          GetPointTotal()         { AssertID(MSG_POINT_TOTAL);        return m_PointTotal; }
    MsgVoice&               GetVoice()              { AssertID(MSG_VOICE);              return m_Voice; }
    MsgMigrate&             GetMigrate()            { AssertID(MSG_MIGRATE);            return m_Migrate; }
    MsgGoodbye&             GetGoodbye()            { AssertID(MSG_GOODBYE);            return m_Goodbye; }

    WORD GetSize()
    {
        m_cbGameData = GetGameDataSize();

        WORD wRet = sizeof( m_cbGameData ) + m_cbGameData;

        if( m_byMessageID == MSG_VOICE )
        {
            wRet += m_Voice.wSize & MAXWORD;
        }

        return wRet;
    }

    #ifdef LIVE_ON_WINDOWS
    #ifndef BYTESWAP_2
    #define BYTESWAP_2(x) ( (((x) & 0x00ff) << 8) | (((x) & 0xff00) >> 8) )
    #endif
    #endif

    VOID EndianSwap( )
    {
    #ifdef LIVE_ON_WINDOWS

        //
        // First swap header information common to all payloads. We only need
        // to byte-swap m_cbGameData. Session IDs are always big-endian, even
        // on the PC, so we don't need to byte-swap m_sessionID. Finally, m_cRef
        // is never sent across the wire, so don't need to byte-swap it either
        //
        m_cbGameData = BYTESWAP_2( m_cbGameData );

        //
        // Now swap payload
        BYTE* pbData = (BYTE*) &m_RespSession; // address of first byte in the message payload
        const SEndianSwapData* pSwap = NULL;
        UINT                   cSwap = 0;

        switch( m_byMessageID )
        {
            case MSG_QUERY_SESSION:         /* no data */                                                                   break;
            case MSG_RESP_SESSION:          pSwap = MsgResponseSession::Swap;  cSwap = MsgResponseSession::SwapCount;       break;
            case MSG_JOIN_SESSION_PARTY:    pSwap = MsgJoinSessionParty::Swap;  cSwap = MsgJoinSessionParty::SwapCount;     break;
            case MSG_JOIN_SESSION:          pSwap = MsgJoinSession::Swap;  cSwap = MsgJoinSession::SwapCount;               break;
            case MSG_FOUND_SESSION:         pSwap = MsgFoundSession::Swap;  cSwap = MsgFoundSession::SwapCount;             break;
            case MSG_JOIN_RESPONSE:         pSwap = MsgJoinResponse::Swap;  cSwap = MsgJoinResponse::SwapCount;             break;
            case MSG_JOIN_RESPONSE_PARTY:   pSwap = MsgJoinResponseParty::Swap;  cSwap = MsgJoinResponseParty::SwapCount;   break;
            case MSG_PLAYER_INFO:           pSwap = MsgPlayerInfo ::Swap;  cSwap = MsgPlayerInfo::SwapCount;                break;
            case MSG_START_SESSION:         /* no data */                                                                   break;
            case MSG_END_SESSION:           /* no data */                                                                   break;
            case MSG_REGISTER:              /* no data */                                                                   break;
            case MSG_REGISTERED:            /* no data */                                                                   break;
            case MSG_WAVE:                  pSwap = MsgWave::Swap;  cSwap = MsgWave::SwapCount;                             break;
            case MSG_SCORE_POINT:           pSwap = MsgScorePoint::Swap;  cSwap = MsgScorePoint::SwapCount;                 break;
            case MSG_POINT_TOTAL:           pSwap = MsgPointTotal::Swap;  cSwap = MsgPointTotal::SwapCount;                 break;
            case MSG_HEARTBEAT:             /* no data */                                                                   break;
            case MSG_GOODBYE:               pSwap = MsgGoodbye::Swap;  cSwap = MsgGoodbye::SwapCount;                       break;
            case MSG_VOICE:                 pSwap = MsgVoice::Swap;  cSwap = MsgVoice::SwapCount;                           break;
            case MSG_MIGRATE:               pSwap = MsgMigrate::Swap;  cSwap = MsgMigrate::SwapCount;                       break;
            case MSG_LEFT_SESSION:          /* no data */                                                                   break;
        }

        if( pSwap )
        {
            // loop through the data to swap
            for( UINT iSwap = 0; iSwap < cSwap; iSwap++ )
            {
                size_t    nBytes  = pSwap[ iSwap ].m_nBytes;

                for( UINT iArray = 0; iArray < pSwap[ iSwap ].m_nArraySize; iArray++ )
                {
                    BYTE* pbBase = pbData + pSwap[ iSwap ].m_nOffset + iArray * nBytes;

                    for( UINT iByte = 0; iByte < nBytes/2; iByte++ )
                    {
                        std::swap( pbBase[ iByte ],
                                   pbBase[ ( nBytes - iByte - 1) ] );
                    }
                }
            }
        }
    #endif //#ifdef LIVE_ON_WINDOWS
    }
};

#pragma pack( pop )

#endif MESSAGES_H
