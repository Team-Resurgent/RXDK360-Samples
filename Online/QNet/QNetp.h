//-----------------------------------------------------------------------------
// qnetp.h
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once
#ifndef QNETP_H
#define QNETP_H


//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// Define the session type to initially create.  Here, we are always setting it
// to System Link.  Real applications can set it dynamically, as well as change
// it later using the QNET_OPTION_TYPE_SESSIONTYPE option (as long as the IQNet
// object's session state is QNET_STATE_IDLE).
// Because this session stores the sample type in a const variable the compiler
// normally gives this warning:
// .\qnet.cpp(856) : warning C4127: conditional expression is constant
// To avoid this we disable that warning.
#pragma warning(disable : 4127)
#pragma warning(disable : 6326)
#pragma warning(disable : 6285)
static const QNET_SESSIONTYPE   c_QNetSampleSessionType = QNET_SESSIONTYPE_LIVE_STANDARD;

// Define the game type to use.  Here we are setting it STANDARD.  When using
// QNET_SESSIONTYPE_XBOXLIVE_RANKED, this would be X_CONTEXT_GAME_TYPE_RANKED.
static const DWORD              c_dwQNetSampleGameType = X_CONTEXT_GAME_TYPE_STANDARD;

// Define the number of public and private slots to enable for the game.
// Public slots are available for users that search for games using Xbox Live
// Matchmaking, private slots are only available to users that are invited by
// or join a friend's session.  The sum of these two numbers is the total
// number of players allowed in a session at one time.
// These values can be changed later using IQNet::SetOpt, passing the
// QNET_OPTION_TYPE_TOTAL_PUBLIC_SLOTS or QNET_OPTION_TYPE_TOTAL_PRIVATE_SLOTS
// options.
static const DWORD              c_dwQNetSampleNumPublicSlots = 8;
static const DWORD              c_dwQNetSampleNumPrivateSlots = 2;

// Cap the maximum number of search results to return.
static const DWORD              c_dwQNetSampleMaxNumSearchResults = 10;


// Time intervals for automating progression through session states.
static const DWORD              c_dwQNetSampleInitialSigninWaitTime = 2 * 1000;
static const DWORD              c_dwQNetSampleWaitUntilReadyTime = 3 * 1000;
static const DWORD              c_dwQNetSampleMinimumLobbyTime = 15 * 1000;
static const DWORD              c_dwQNetSampleMinimumGameTimeHost = 25 * 1000;
static const DWORD              c_dwQNetSampleMinimumGameTimeNonhost = 20 * 1000;
static const DWORD              c_dwQNetSampleSendInterval = 2 * 1000;


//-----------------------------------------------------------------------------
// Name: class CQNetSamplePlayer
// Desc: Wrapper class for QNet players
//-----------------------------------------------------------------------------
class CQNetSamplePlayer
{
public:
CQNetSamplePlayer( IQNetPlayer* pIQNetPlayer )
{
    m_pIQNetPlayer = pIQNetPlayer;
    m_dwLastMessageTime = GetTickCount();
}

~CQNetSamplePlayer()
{
}

    IQNetPlayer* m_pIQNetPlayer;      // pointer to QNet player interface
    DWORD m_dwLastMessageTime; // timestamp when last message was sent for local players, or received for remote players
};


//-----------------------------------------------------------------------------
// Name: class CQNetSample
// Desc: Main class for this sample
//-----------------------------------------------------------------------------
class CQNetSample : public IQNetCallbacks
{
public:
    // IQNetcallback interface functions
    virtual
    VOID
    NotifyStateChanged(
        IN QNET_STATE               OldState,
        IN QNET_STATE               NewState,
        IN HRESULT                  hrInfo
        );

    virtual
    VOID
    NotifyPlayerJoined(
        IN IQNetPlayer *            pPlayer
        );

    virtual
    VOID
    NotifyPlayerLeaving(
        IN IQNetPlayer *            pPlayer
        );

    virtual
    VOID
    NotifyNewHost(
        IN IQNetPlayer *            pPlayer
        );

    virtual
    VOID
    NotifyDataReceived(
        IN IQNetPlayer *            pPlayerFrom,
        IN DWORD                    dwNumPlayersTo,
        IN IQNetPlayer **           apPlayersTo,
        IN const BYTE *             pbData,
        IN DWORD                    dwDataSize
        );

    virtual
    VOID
    NotifyWriteStats(
        IN IQNetPlayer *            pPlayer
        );

    virtual
    VOID
    NotifyReadinessChanged(
        IN IQNetPlayer *            pPlayer,
        IN BOOL                     bReady
        );

    virtual
    VOID
    NotifyCommSettingsChanged(
        IN IQNetPlayer *            pPlayer
        );

    virtual
    VOID
    NotifyGameSearchComplete(
        IN IQNetGameSearch *        pGameSearch,
        IN HRESULT                  hrComplete,
        IN DWORD                    dwNumResults
        );

    virtual
    VOID
    NotifyGameInvite(
        IN DWORD                    dwUserIndex,
        IN const XINVITE_INFO *     pInviteInfo
        );

    virtual
    VOID
    NotifyContextChanged(
        IN const XUSER_CONTEXT *    pContext
        );

    virtual
    VOID
    NotifyPropertyChanged(
        IN const XUSER_PROPERTY *   pProperty
        );

            CQNetSample();
            ~CQNetSample();

    HRESULT Initialize();
    VOID    Cleanup();
    VOID    Run();


private:
    VOID    BlockUntilSignedIn();

    HRESULT StartSearchForGames();
    HRESULT CreateGame();
    HRESULT JoinInvitedGame();
    HRESULT DoSends();

    HRESULT RunStateIdle();
    HRESULT RunStateSessionHosting();
    HRESULT RunStateSessionJoining();
    HRESULT RunStateGameLobby();
    HRESULT RunStateSessionRegistering();
    HRESULT RunStateSessionStarting();
    HRESULT RunStateGamePlay();
    HRESULT RunStateSessionEnding();
    HRESULT RunStateSessionLeaving();
    HRESULT RunStateSessionDeleting();

    IQNet* m_pIQNet;             // pointer to QNet interface
    IXAudio2* m_pXAudio2;       // pointer to XAudio2 instance used by QNet
    IXAudio2MasteringVoice* m_pXAudio2MasteringVoice;  // pointer to XAudio2 mastering voice
    DWORD m_dwPrimaryUserIndex; // the user index of the first player found
    DWORD m_dwLocalUsersMask;   // bit mask for all local users that are participating
    DWORD m_dwStateTime;        // timestamp at which the QNet state last changed
    DWORD m_dwNumGamesPlayed;   // total number of games played
    IQNetGameSearch* m_pIQNetGameSearch;   // pointer to currently active QNet game search interface
    XINVITE_INFO m_XInviteInfo;        // save invite information to join
    BOOL m_fJoinFromInvite;    // whether we need to start joining via the invite info or not
    BOOL m_fQuitting;          // whether the title is terminating or not
};


//-----------------------------------------------------------------------------
// Message structures
// These are 1 byte packed to get them to be the smallest size possible in case
// they aren't designed to be optimally aligned.  It is also good practice to
// standardize on a packing scheme when transmitting data to remote machines.
// Fields larger than a byte are also explicitly defined to be in "network"
// byte order (that is, big-endian).
//-----------------------------------------------------------------------------
#pragma pack(push, 1)

#define QNETSAMPLEMSGTYPE_DUMMY     ((BYTE) 0x01)

// Generic header for all messages, cast to one of the other structures
typedef struct QNETSAMPLEMSG
{
    BYTE byMsgType; // message type
}                               QNETSAMPLEMSG, * PQNETSAMPLEMSG;

// Dummy message
typedef struct QNETSAMPLEMSG_DUMMY
{
    BYTE byMsgType; // message type (QNETSAMPLEMSGTYPE_DUMMY)
    WORD wDummy1;   // 2 bytes of dummy data
    DWORD dwDummy2;  // 4 more bytes dummy data
}                               QNETSAMPLEMSG_DUMMY, * PQNETSAMPLEMSG_DUMMY;

#pragma pack(pop)



#endif // QNETP_H
