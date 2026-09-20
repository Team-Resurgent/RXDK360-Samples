//-----------------------------------------------------------------------------
// File: VideoChat.h
//
// Desc: Class and type definitions for Voice sample
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#pragma once

#include "Messages.h"

#pragma warning( disable : 4127 )       // until STL is fixed
#include <list>


const DWORD                         MAX_GAME_NAMES = 6;      // Number of game names to choose from
const DWORD                         COLOR_HIGHLIGHT = 0xffffff00; // Yellow
const DWORD                         COLOR_GREEN = 0xff00ff00;
const DWORD                         COLOR_NORMAL = 0xffffffff;
const DWORD                         COLOR_NOT_CONNECTED = 0xff4c4c4c;
const DWORD                         COLOR_RED = 0xff3f0000;
const DWORD                         COLOR_LIGHTRED = 0x803f0000;
const DWORD                         MAX_STATUS_STR = 128;
const DWORD                         MAX_ERROR_STR = 256;

// Number of buffers that we cycle through for pulling video packets off the
// network and handing them into the IXCamStreamEngine for decoding.  Packets
// cannot be decoded until an entire video frame worth of data has been
// received.  For each incoming stream, the engine will never hold onto more
// than 64 packets, so 256 packets will guarantee that we have enough packet
// buffers to process 4 incoming streams...
const DWORD                         NUM_VIDEO_PACKET_BUFFERS = 256;

// TCR Session Discovery Time for System Link Play
const FLOAT                         GAME_SEARCH_TIME = 2.0f;   // 2 seconds (may not exceed 3)
const FLOAT                         GAME_JOIN_TIME = 2.0f;   // 2 seconds
const DWORD                         PLAYER_TIMEOUT = 2000;   // 2 seconds
const FLOAT                         PLAYER_HEARTBEAT = 0.3f;   // ~3 times per second

//-----------------------------------------------------------------------------
// Name: class PlayerInfo
// Desc: Player information used by players to store list of other players
//       in the game
//-----------------------------------------------------------------------------
struct PlayerInfo
{
    XUID xuid;                           // Player's xuid
    CHAR strGamertag[ XUSER_NAME_SIZE ]; // player name
    BOOL bHasVoice;                      // TRUE if player has voice
    DWORD bMuted:4;                       // Local has muted this player
    BOOL bRemoteMuted:4;                 // This player has muted us
};

//-----------------------------------------------------------------------------
// Name: class MachineInfo
// Desc: Machine information used by players to store list of other machines
//       in the game
//-----------------------------------------------------------------------------
struct MachineInfo
{
    XNADDR xnAddr;                      // XNet address
    IN_ADDR inAddr;                      // Xbox IP (not a "real" IP)
    DWORD dwLastHeartbeat;             // last heartbeat, in our clocks
    BOOL bHasVideo;                   // TRUE if the machine has a camera
    DWORD dwNumPlayers;
    PlayerInfo Players[ 4 ];
};

//-----------------------------------------------------------------------------
// Name: class GameInfo
// Desc: Game information used by clients to store available games
//-----------------------------------------------------------------------------
struct GameInfo
{
    XNKID xnHostKeyID;                    // host key ID
    XNKEY xnHostKey;                      // host key
    XNADDR xnHostAddr;                     // host XNet address
    BYTE byNumPlayers;                   // number of players in game
    XCAMRESOLUTION Resolution;             // video resolution of this session
    WCHAR strGameName[ MAX_GAME_NAME_LENGTH ]; // name of the game
};

typedef std::vector <MachineInfo>   MachineList;
typedef std::vector <GameInfo>      GameList;


//-----------------------------------------------------------------------------
// Name: class VideoPacket
// Desc: Used to associate an XOVERLAPPED structure with a Message body.  These
//       messages are used to pull incoming video messages off the network, and
//       hand them off to the IXCamStreamEngine for processing.
//-----------------------------------------------------------------------------
struct VideoPacket
{
    Message message;
    XOVERLAPPED xoverlapped;
};
typedef std::list <VideoPacket*>    VideoPacketList;

//--------------------------------------------------------------------------------------
// Name: VideoSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class VideoSample : public ATG::Application
{
    enum State
    {
        STATE_MENU,             // Main menu
        STATE_GAME,             // Game menu
        STATE_SELECT_SOURCE_AND_RESOLUTION, // Select source camera and resolution
        STATE_SELECT_NAME,      // Select game name screen
        STATE_GAME_SEARCH,      // Searching for game
        STATE_SELECT_GAME,      // Game selection menu
        STATE_REQUEST_CONNECT,  // Joining game - waiting for connection
        STATE_REQUEST_JOIN,     // Joining game - waiting for answer
        STATE_ERROR             // Error screen
    };

    enum MainMenuItems
    {
        MAIN_MENU_START_GAME    = 0,
        MAIN_MENU_JOIN_GAME     = 1,
        MAIN_MENU_MAX,
    };

    enum USBCameraResolutionMenuItems
    {
        USB_CAMERA_RESOLUTION_MENU_QQVGA   = 0,
        USB_CAMERA_RESOLUTION_MENU_QCIF    = 1,
        USB_CAMERA_RESOLUTION_MENU_QVGA    = 2,
        USB_CAMERA_RESOLUTION_MENU_CIF     = 3,
        USB_CAMERA_RESOLUTION_MENU_VGA     = 4,
        USB_CAMERA_RESOLUTION_MENU_MAX,
    };

    enum KinectCameraResolutionMenuItems
    {
        KINECT_CAMERA_RESOLUTION_MENU_QQVGA   = 0,
        KINECT_CAMERA_RESOLUTION_MENU_QVGA    = 1,
        KINECT_CAMERA_RESOLUTION_MENU_VGA     = 2,
        KINECT_CAMERA_RESOLUTION_MENU_MAX,
    };

    enum InitStatus
    {
        Success,
        NotConnected,
        InitFailed
    };


    ATG::Font m_Font;                // Game font
    ATG::Font m_OnlineIconsFont;     // Online icons font
    ATG::Help m_Help;                // Help screen

    BOOL m_DisplayHelp;
    BOOL m_FullscreenMode;

    State m_State;                // Game state
    DWORD m_CurrMenuItem;         // Current menu item
    DWORD m_CurrGameIndex;         // Current game index in m_Games list that we are going to join
    GameList m_Games;                // List of available games to join
    WCHAR           m_GameNames[ MAX_GAME_NAMES ][ MAX_GAME_NAME_LENGTH ]; // List of names to choose from when starting a game

    WCHAR           m_strGameName[ MAX_GAME_NAME_LENGTH ];     // Game name
    MachineList m_Machines;             // List of machines that we are connected to

    CStopwatch m_GameSearchTimer;      // Wait for game search to complete
    CStopwatch m_GameJoinTimer;        // Wait for game join to complete
    CStopwatch m_HeartbeatTimer;       // Keep-alive timer
    CStopwatch m_VoiceTimer;
    CStopwatch m_FrameTimer;
    FLOAT m_FrameSeconds;

    ATG::Timer m_Timer;                // Timer

    BOOL m_bIsOnline;            // TRUE if link status good
    BOOL m_bXnetStarted;         // TRUE if networking initialized
    BOOL m_bIsHost;              // TRUE if we're hosting the game
    XNKID m_xnHostKeyID;          // Host key ID
    XNKEY m_xnHostKeyExchange;    // Host key exchange key
    XNADDR m_xnTitleAddress;       // The XNet address of this machine/game
    IN_ADDR m_inHostAddr;           // The "IP" address of the host
    Nonce m_Nonce;                // Client identifier
    HANDLE m_hNotification;        // System notification listener handle

    DWORD m_SignedInMask;
    Machine m_LocalMachine;

    XUID            m_LocalXUIDs[ XUSER_MAX_COUNT ];
    WCHAR           m_strError[ MAX_ERROR_STR ];


    // We use several types of sockets
    // 1) Broadcast socket for finding sessions
    // 2) Direct UDP socket for game and voice data
    // 3) Reliable TCP socket for infrequent critical messages
    // Note that the host uses m_ReliableSock to listen for incoming
    // connections from clients, and then maintains a list of TCP
    // sockets to each client
    CSocket m_BroadSock;            // Broadcast socket for broadcast msgs
    CSocket m_DirectSock;           // Direct socket for direct msgs
    CSocket m_ReliableSock;         // Reliable socket (or listen socket for host)
    CSocket m_VideoSock;

    VideoPacket* m_VideoPackets;
    VideoPacketList m_AvailableVideoPackets;
    VideoPacketList m_InUseVideoPackets;


    Message m_msgVoiceData;                 // Voice data packet for buffering voice

    PendingMessage m_msgPending;           // Pending message for client
    SocketList m_ClientSockets;        // Reliable sockets for low-bandwidth msgs

    BOOL m_SinglePacketDrop;
    BOOL m_ContinuousPacketDrop;

      
    DWORD m_XCamFramerateIndex;
    DWORD m_TargetBitrate;
    XCAM_STREAM_ENGINE_INIT_PARAMS m_XCamStreamInitParams;
    PIXCAMSTREAMENGINE m_XCamStreamEngine;
	PIXHV2ENGINE  m_pXHV2Engine;
	HANDLE  m_hWorkerThread;
	IXAudio2 *m_pXAudio2;


    DWORD m_dwLowBitrateTime;

    XOVERLAPPED m_DialogOverlapped;      // Opt-in dialog overlapped & result
    OPTIN_DIALOG_RESULT m_DialogResult;
    BOOL m_bDialogShown;          // Shown flags
    BOOL m_bDialogDismissed;
    BOOL m_bSystemUIShowing;

    BOOL    m_bUseKinectCamera;             // Boolean flag which is set to true when we are reading from Kinect camera, and 
                                            // is false when we use USB XCamera.
    BOOL    m_bFirstKinectFrameReceived;    // Boolean flag that indicates if Kinect camera has sent us the first image,
                                            // which is used to show a text message at the beginning char session while 
                                            // the Kinect camera initializes.
    HANDLE  m_hNextFrameEventHandle;        // Handle for signaling Nui next frame 
    HANDLE  m_hEncodeDoneEventHandle;       // Handle for signaling XCamera encode done events
    HANDLE  m_hNuiColorStreamHandle;        // Handle for Nui color stream
    BYTE*   m_pEncoderFrameBuffer;          // Frame buffer for Kinect frames captured and converted to YUY2 format 
    DWORD   m_dwEncoderFrameBufferSize;     // Size of the m_pEncoderFrameBuffer in bytes

public:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

                    VideoSample();

private:
    VOID            UpdateMenu();
    VOID            UpdateGame();
    VOID            UpdateSelectSourceAndResolution();
    VOID            UpdateSelectName();
    VOID            UpdateRequestJoin();
    VOID            UpdateError();

    VOID            RenderMenu();
    VOID            RenderGame();
    VOID            RenderSelectResolution();
    VOID            RenderSelectName();
    VOID            RenderRequestJoin();
    VOID            RenderHeader();
    VOID            RenderHelp();
    VOID            RenderStartHelp();
    VOID            RenderGameHelp();
    VOID            RenderError();

    VOID            InitiateJoin( DWORD );
    VOID            StartVoice();
    
    HRESULT         InitializeKinectCamera();
    HRESULT         ShutdownKinectCamera();
    VOID            ResizeUYVYToYUY2( BYTE* pDst, XCAMRESOLUTION dstResolution, BYTE* pSrc, XCAMRESOLUTION srcResolution);
    
    // Initialization
    BOOL            InitXNet();
    HRESULT         InitXHV();
    VOID            InitializeXCam();

    // Handle microphone data
    VOID            CheckMicrophones();

    // Process system notifications
    VOID            SystemNotificationsUpdate();

    // Send messages
    VOID            SendFindGame();
    VOID            SendGameFound( const Nonce& );
    VOID            SendJoinGame( const SOCKADDR_IN& );
    VOID            SendJoinApproved( const SOCKADDR_IN& );
    VOID            SendJoinDenied( const SOCKADDR_IN& );
    VOID            SendVoiceDataToAll();
    VOID            SendVideoDataToAll();
    INT             SendMessage( const Message* pMsg, BOOL bReliable, const SOCKADDR_IN* psaDest = NULL );

    // Receive messages
    BOOL            ProcessBroadcastMessage();
    BOOL            ProcessDirectMessage();
    BOOL            ProcessReliableMessage();
    BOOL            ProcessVideoMessage();
    VOID            ProcessMessage( Message&, const SOCKADDR_IN& );

    // Process incoming messages
    VOID            ProcessFindGame( const MsgFindGame& );
    VOID            ProcessGameFound( const MsgGameFound& );
    VOID            ProcessJoinGame( const MsgJoinGame&, const SOCKADDR_IN& );
    VOID            ProcessJoinApproved( const MsgJoinApproved&, const SOCKADDR_IN& );
    VOID            ProcessJoinDenied( const SOCKADDR_IN& );
    VOID            ProcessMachineJoined( const MsgMachineJoined&, const SOCKADDR_IN& );
    VOID            ProcessHeartbeat( const SOCKADDR_IN& );
    VOID            ProcessVoiceData( const MsgVoiceData&, const SOCKADDR_IN& );

    //Handle keep-alive
    HRESULT         OnMachineJoined( const Machine& pMachine, XNADDR xnAddr, const in_addr* pinAddr );
    HRESULT         OnMachineDisconnect( MachineInfo* pMachine );
    VOID            ProcessMachineDropouts();

    // Shift the remote console windows around the screen to balance them
    VOID            RecalculateDisplayPositions();

    // Utility
    static VOID     GenRandom( WCHAR*, DWORD );
    static WCHAR    GetRandVowel();
    static WCHAR    GetRandConsonant();
    static VOID     AppendConsonant( WCHAR*, BOOL );
    static VOID     AppendVowel( WCHAR* );
    static BOOL     IsUSBCameraConnected();
    static BOOL     IsKinectCameraConnected();
};



//-----------------------------------------------------------------------------
// Name: class MatchInAddr
// Desc: Predicate functor used to match on IN_ADDRs in machine lists
//-----------------------------------------------------------------------------
struct MatchInAddr
{
    IN_ADDR ia;
    explicit    MatchInAddr( const SOCKADDR_IN& sa ) : ia( sa.sin_addr )
    {
    }
    explicit    MatchInAddr( const in_addr& addr ) : ia( addr )
    {
    }
    bool        operator()( const MachineInfo& machineInfo )
    {
        return machineInfo.inAddr.s_addr == ia.s_addr;
    }
    bool        operator()( const ClientSocket& cs )
    {
        return cs.sa.sin_addr.s_addr == ia.s_addr;
    }
};
