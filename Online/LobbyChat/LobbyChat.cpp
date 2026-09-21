//--------------------------------------------------------------------------------------
// LobbyChat
//
// Sample to demonstrate the simplest way to communicate with headsets over Xbox Live 
// 
// XHV is independant from networking communications entirely as well as from sessions
// This sample demonstrates how to put sessions, basic networking, and XHV together.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// INCLUDES
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <malloc.h>
#include <stdio.h>
#include <winsockx.h>
#include <xonline.h>
#include <xaudio2.h>
#include <xhv2.h>

#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgInput.h"
#include "AtgUtil.h"

#include "LobbyChat.spa.h"

// use stl to manage remote console data
#include <list> 
#include <set>


//--------------------------------------------------------------------------------------
// CONSTANTS
//--------------------------------------------------------------------------------------

// color constants
const D3DCOLOR TOP_BACK_COLOR      = D3DCOLOR_ARGB( 0xFF, 0x00, 0x00, 0x7F ); 
const D3DCOLOR BLACK_COLOR         = D3DCOLOR_ARGB( 0xFF, 0x00, 0x00, 0x00 );
const D3DCOLOR WHITE_COLOR         = D3DCOLOR_ARGB( 0xFF, 0xFF, 0xFF, 0xFF );
const D3DCOLOR RED_COLOR           = D3DCOLOR_ARGB( 0xFF, 0xFF, 0x00, 0x00 );
const D3DCOLOR GREY_COLOR          = D3DCOLOR_ARGB( 0xFF, 0xBB, 0xBB, 0xBB );
const D3DCOLOR GREEN_COLOR         = D3DCOLOR_ARGB( 0xFF, 0x00, 0xFF, 0x00 );
const D3DCOLOR HIGHLIGHT_COLOR     = D3DCOLOR_ARGB( 0xFF, 0xFF, 0xFF, 0x00 );

// size of maximum voice packet data 
// 1264 is technically the max for the entire payload, currently
const WORD MAX_VDP_DATA_SIZE = 1200;       

// for maximum efficiency, all Xbox network 
// traffic should be on this port
const USHORT    PORT_NUM = 1000;   

// session slot configuration, make all slots public for sample simplicity
const int SLOTS_TOTALPUBLIC  = 32;
const int SLOTS_TOTALPRIVATE = 0; 

// session flags
const int session_flags = XSESSION_CREATE_USES_PRESENCE | XSESSION_CREATE_USES_MATCHMAKING | XSESSION_CREATE_USES_PEER_NETWORK | XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED;

// this generally would be specified via menu UI.  Change this to enter different chat lobbies
const int LOBBY_MAP = 0;

// the send rate for data in "sends a second".
const int send_rate = 10;
const int send_time = 1000 / send_rate;

// XHV_PROCESSING_MODE is typedef as const
// for testing purposes sometimes local users will also use XHV_LOOPBACK_MODE
XHV_PROCESSING_MODE xhv_processing_mode = XHV_VOICECHAT_MODE;

// set to TRUE if you wish to display muting and blocked status for each lobby user
// this is not TCR090 compiant, used for testing purposes only
// not const currently to avoid warning 
BOOL SHOW_MUTED = TRUE;
//BOOL SHOW_MUTED = FALSE;


//--------------------------------------------------------------------------------------
// MESSAGE STRUCTURES
//--------------------------------------------------------------------------------------
enum MessageID
{
    MessageID_NewConsole, 
    MessageID_Recieved,
    MessageID_ConsoleUpdate,
    MessageID_VoiceData
};

// base message structure
struct SMessage
{
    unsigned short cbGameData;                          // since all traffic is VDP, we need to specify this value
    MessageID id;
};

// new console message sent to host for join, then relayed to peers
struct SNewConsoleMessage : public SMessage
{
    XNADDR xnaddr;                                      // XNADDR of the console
    // one entry per local user slot
    BOOL  isActiveTalker[XUSER_MAX_COUNT];              // status of if slot is signed in for active live profile
    XUID  xuid[XUSER_MAX_COUNT];                        // XUID's of logged in session Live users
    WCHAR gamertag[XUSER_MAX_COUNT][XUSER_NAME_SIZE];   // gamertags for each Live user in session
};

// peer response message sent from host after recieving new console message
// this message contains the host data and a join response
struct SRecievedMessage : public SNewConsoleMessage
{
    enum Status
    {
        STATUS_JOINOK,
        STATUS_JOINFAILED,
    };
    Status status;
};

// any console status change comes through this message
// initially to the host, then relayed to the peers through the host
struct SUpdateConsoleMessage : public SMessage
{
    XNADDR xnaddr;   // XNADDR of the remote console sending the update
    DWORD  slot;
    XUID   xuid;     // user xuid which is changing
    WCHAR  gamertag[XUSER_NAME_SIZE]; 
    BOOL   isMuted;
    DWORD  flag;     // only single flag used, specifies updated type

    enum Flags
    {
        FLAG_NONE,
        FLAG_ADDUSER,
        FLAG_REMOVEUSER,
        FLAG_MUTEDUSER
    };
};

// remote voice data message
struct SVoiceMessage : public SMessage
{
    XUID  xuid;     
    DWORD datasize;
    BYTE  data[MAX_VDP_DATA_SIZE]; 
};

// group all messages together for general use and simplicity
union UMessage
{
    SMessage                BaseMessage;
    SNewConsoleMessage      NewConsoleMessage;
    SRecievedMessage        RecievedMessage;
    SUpdateConsoleMessage   UpdateConsoleMessage; 
    SVoiceMessage           VoiceMessage;
};

// data structures used for tracking console and user state for the application
// this structure tracks local console talkers
struct SLocalTalker
{
    BOOL  isActiveTalker;               // is signed into the session
    XUID  xuid;                         // signed in user xuid
    WCHAR gamertag[XUSER_NAME_SIZE];    // signed in user gamertag
};

// this structure tracks remote console talkers
struct SRemoteConsole
{
    XNADDR  xnaddr;
    IN_ADDR inaddr;

    // remote console users based on controller slot index
    BOOL  isActiveTalker[XUSER_MAX_COUNT];
    BOOL  isMuted[XUSER_MAX_COUNT];
    XUID  xuid[XUSER_MAX_COUNT];
    WCHAR gamertag[XUSER_MAX_COUNT][XUSER_NAME_SIZE];
};


//--------------------------------------------------------------------------------------
// TYPEDEFS
//--------------------------------------------------------------------------------------
typedef std::list <SRemoteConsole> RemoteConsoleList;

//--------------------------------------------------------------------------------------
// class LobbyChat
//
// Main class to run this application.
//--------------------------------------------------------------------------------------
class LobbyChat : public ATG::Application
{
public:
    LobbyChat();

    ATG::Timer m_Timer;
    ATG::Font m_Font;

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    //--------------------------------------------------------------------------------------
    // app specific member functions
    //--------------------------------------------------------------------------------------
    VOID GetSessionDetails();

    UMessage* ReceiveMessage(SOCKADDR_IN* psaIn, SIZE_T* pcbRecv);
    HRESULT SendMessage(const IN_ADDR * sinPeer, SMessage* pMessage, SIZE_T cbMessage, unsigned short cbGameData);

    VOID ProcessNewConsoleMessage(UMessage *pMessage); 
    VOID ProcessUpdateConsoleMessage(UMessage *pMessage);
    VOID AddRemoteConsole(UMessage *msg); 

    VOID RemovePlayerFromSession(DWORD slot); 
    VOID ProcessLocalControllerInputs();
    VOID ProcessLiveNotifications();
    VOID ProcessMuteListChanged();

    BOOL CreateSessionAsPeer(DWORD ownerslot);
    VOID CreateSessionAsHost(DWORD ownerslot);
    VOID DoPeerWork();
    VOID DoHostWork();
    VOID InitializeXaudio2();
    VOID ProcessVoice();
    XUID FindSelectedUser(DWORD selected_user);

    //--------------------------------------------------------------------------------------
    // app specific member variables
    //--------------------------------------------------------------------------------------
    SOCKET          m_socket;   // socket we use for transmitting/receiving
    XNADDR          m_xnaddr;   // our own XNADDR
    IN_ADDR         m_sinPeer;  // address of a peer 
    HRESULT         hr;         // standard reusable HR

    // used to test if console is host or peer (or could be neither)
    BOOL isHost;
    BOOL isPeer;

    DWORD session_owner_slot;

    // we track all talker and console data through these structures
    SLocalTalker m_LocalTalkers[XUSER_MAX_COUNT];
    std::set <XUID> RemoteMuteList[XUSER_MAX_COUNT];     // remote xuid muted local slot
    std::set <XUID> LocalMuteList[XUSER_MAX_COUNT];      // local slot muted remote xuid
    std::list <SRemoteConsole> m_RemoteConsoles;

    // XSession and Live notifications
    XSESSION_INFO SessionInfo;
    ULONGLONG SessionNonce; 
    XSESSION_LOCAL_DETAILS* pSessionDetails;
    HANDLE hSession;
    HANDLE hNotification;

    // pointers to XHV2 engine and XAudio2 interface
    PIXHV2ENGINE m_pXHV;
    IXAudio2 *pXAudio2;

    // received message
    UMessage message;  
    IN_ADDR host_inaddr;

    DWORD appState;
    enum AppStates
    {
        STATE_NONE,
        STATE_MENU,
        STATE_HOST,
        STATE_PEER
    };

    // selected remote user (used for persistant profile muting)
    DWORD selected_user;
    DWORD remote_user_count;

    // used to throttle send rate
    DOUBLE prev_time; 
};


//--------------------------------------------------------------------------------------
// FUNCTIONS
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// main
//
// Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    LobbyChat atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, 
        &atgApp.m_d3dpp.BackBufferHeight );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// LobbyChat
//
// Initialize member variables
//--------------------------------------------------------------------------------------
LobbyChat::LobbyChat() :
    isHost(FALSE),
    isPeer(FALSE),
    pSessionDetails(NULL),
    SessionNonce(0),
    hSession(INVALID_HANDLE_VALUE),
    pXAudio2(0),
    appState(STATE_MENU)
{}


//--------------------------------------------------------------------------------------
// Initialize
//
// This creates all display objects and Live functionality.
//--------------------------------------------------------------------------------------
HRESULT LobbyChat::Initialize()
{
    ZeroMemory(m_LocalTalkers, (sizeof(SLocalTalker) * XUSER_MAX_COUNT));

    // initialize globals for socket info
    m_socket = INVALID_SOCKET;
    m_sinPeer.s_addr = INADDR_NONE;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // start up the SNL with default initialization parameters
    XNetStartupParams xnsp;
    ZeroMemory(&xnsp, sizeof(xnsp));
    xnsp.cfgSizeOfStruct = sizeof(XNetStartupParams);

    if( XNetStartup( &xnsp ) != 0 )
    {
        ATG::FatalError( "XNetStartup failed.\n" );
    }

    // start up Winsock
    WORD wVersion = MAKEWORD( 2, 2 );   // request version 2.2 of Winsock
    WSADATA wsaData;

    INT err = WSAStartup( wVersion, &wsaData );
    if( err != 0 )
    {
        ATG::FatalError( "WSAStartup failed, error %d.\n", err );
    }

    // Verify that we got the right version of Winsock
    if( wsaData.wVersion != wVersion )
    {
        ATG::FatalError( "Failed to get proper version of Winsock, got %d.%d.\n",
            LOBYTE( wsaData.wVersion ), HIBYTE( wsaData.wVersion ) );
    }

    XOnlineStartup();

    // initialize the socket
    m_socket = socket( AF_INET, SOCK_DGRAM, IPPROTO_VDP );
    if( m_socket == INVALID_SOCKET )
    {
        ATG::FatalError( "Failed to create socket, error %d.\n", WSAGetLastError() );
    }

    // Bind the socket
    SOCKADDR_IN sa;
    sa.sin_family = AF_INET;           // IP family
    sa.sin_addr.s_addr = INADDR_ANY;   // Use the only IP that's available to us
    sa.sin_port = htons( PORT_NUM );   // Port (should ideally be 1000)

    if( bind( m_socket, ( const sockaddr* )&sa, sizeof( sa ) ) != 0 )
    {
        ATG::FatalError( "Failed to bind socket, error %d.\n", WSAGetLastError() );
    }

    // Set the socket to nonblocking
    ULONG iUnblocking = 1;
    if( ioctlsocket( m_socket, FIONBIO, &iUnblocking ) != 0 )
    {
        ATG::FatalError( "Failed to set socket to nonblocking, error %d\n", WSAGetLastError() );
    }

    // Seed the random number generator while we're here
    DWORD dwSeed;
    XNetRandom( ( BYTE* )&dwSeed, sizeof( DWORD ) );
    srand( dwSeed );

    // ensure someone is signed in
    XShowSigninUI(4, XSSUI_FLAGS_SHOWONLYONLINEENABLED);

    hNotification = XNotifyCreateListener( XNOTIFY_SYSTEM | XNOTIFY_LIVE);

    prev_time = GetTickCount();

    selected_user = 0;
    remote_user_count = 0;
    session_owner_slot = 0;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Update
//
// Called once per frame, the call is the entry point for all updates
//--------------------------------------------------------------------------------------
HRESULT LobbyChat::Update()
{
    // Detect reboot keypress, also populates keypress information so we can check for key presses below
    ATG::Input::GetMergedInput();

    switch(appState)
    {
    case STATE_MENU:
        for (DWORD slot=0; slot<XUSER_MAX_COUNT; slot++)
        {
            // ensure user pushing buttons is signed in
            if (ATG::Input::m_Gamepads[slot].wPressedButtons && XUserGetSigninState(slot) == eXUserSigninState_NotSignedIn) 
            {
                // ideally this would be a messaged through the UI
                ATG::DebugSpew("controller %d not signed in to live, please signin\n", slot);
                continue; // onto next controller slot
            }

            if (ATG::Input::m_Gamepads[slot].wPressedButtons & XINPUT_GAMEPAD_START)
            {
                ATG::DebugSpew("Launching as host (xnaddr=%x)\n", m_xnaddr.ina.s_addr);
                CreateSessionAsHost(slot);    
                appState = STATE_HOST;
                break;
            }

            else if (ATG::Input::m_Gamepads[slot].wPressedButtons & XINPUT_GAMEPAD_BACK)
            {
                ATG::DebugSpew("Launching as peer\n");
                // check to ensure we found a session before changing the appState
                if (CreateSessionAsPeer(slot))
                {
                    appState = STATE_PEER;
                }
                break;
            }
        }
        break;

    case STATE_HOST:
        DoHostWork();
        break;

    case STATE_PEER:
        DoPeerWork();
        break;
    };

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render
//
// Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT LobbyChat::Render()
{
    WCHAR strRender[128] = L"";

    // String to display at the bottom right for leaderboards
    static const WCHAR* wstrFullHelpText =
        GLYPH_A_BUTTON     L": Join"
        GLYPH_B_BUTTON     L": Leave"
        GLYPH_Y_BUTTON     L": Mute  "
        GLYPH_X_BUTTON     L": UnMute ";

    static const WCHAR* wstrPartHelpText =
        GLYPH_A_BUTTON     L": Join"
        GLYPH_B_BUTTON     L": Leave";

    // Draw a gradient filled background
    ATG::RenderBackground( TOP_BACK_COLOR, BLACK_COLOR );

    m_Timer.MarkFrame();

    m_Font.Begin();

    // Draw title text
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0, 0, WHITE_COLOR, L"Lobby Chat" );
    
    // Draw the header
    // Display the Session ID in its byte order, XBox 360 processor is Big Endian
    // Text buffer
    if (isHost || isPeer)
    {
        swprintf_s( strRender, L"%s Session (SID:%016I64X)\n %d/%d public slots\n",
            isHost ? L"Hosting" : L"Peer in",
            *( ULONGLONG* )SessionInfo.sessionID.ab,
            pSessionDetails->dwMaxPublicSlots - pSessionDetails->dwAvailablePublicSlots,
            pSessionDetails->dwMaxPublicSlots);
    
        m_Font.DrawText( ( m_Font.m_rcWindow.x2 - m_Font.m_rcWindow.x1 ) / 2.0f, 32, HIGHLIGHT_COLOR, strRender, ATGFONT_CENTER_X );
    }
    else
    {
        m_Font.DrawText( ( m_Font.m_rcWindow.x2 - m_Font.m_rcWindow.x1 ) / 2.0f, 32, HIGHLIGHT_COLOR, L"Press start to host or back to find existing session", ATGFONT_CENTER_X );
    }

    // Draw timer
    m_Font.SetScaleFactors( 0.75f, 0.75f );
    m_Font.DrawText( 0, 0, HIGHLIGHT_COLOR, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

    // Draw lobby members
    FLOAT y = 64;
    FLOAT x = 0;
    const FLOAT step_y = 20;
    const FLOAT max_y = 400;
    const FLOAT step_x = 100;
    D3DCOLOR color;
    WCHAR buf[256];

    // display local users
    for (DWORD slot=0; slot<XUSER_MAX_COUNT; slot++)
    {
        if (m_LocalTalkers[slot].isActiveTalker)
        {
            if (m_pXHV->IsLocalTalking(slot))
            {
                color = RED_COLOR;
            }
            else if (m_pXHV->IsHeadsetPresent(slot))
            {
                if (m_pXHV->IsSharedMicPresent(slot))
                {
                    color = GREEN_COLOR; // local user is assigned Kinect mic array but isn't talking
                }
                else
                {
                    color = WHITE_COLOR; // local user has headset but isn't talking
                }
            }
            else // local user is in the lobby, but without a voice chat input device
            {
                color = GREY_COLOR;
            }

            if (m_pXHV->IsVoiceOverSpeakers(slot))
            {
                // this indicates that incoming remote voice data for this slot will be played through the TV speakers
                swprintf_s(buf, L"%s%s", m_LocalTalkers[slot].gamertag, L" (TV)"); 
            }
            else
            {
                swprintf_s(buf, L"%s", m_LocalTalkers[slot].gamertag);
            }

            m_Font.DrawText( x, y, color, (const WCHAR *) buf );
            y += step_y; 
        }
    }
    
    // put break inbetween local and remote display
    y += step_y;

    // display remote users
    for (RemoteConsoleList::iterator remote_console = m_RemoteConsoles.begin(); remote_console != m_RemoteConsoles.end(); remote_console++)
    {
        for (DWORD slot=0; slot<XUSER_MAX_COUNT; slot++) 
        {
            if (remote_console->isActiveTalker[slot])
            {
                if (m_pXHV->IsRemoteTalking(remote_console->xuid[slot]))
                {
                    color = RED_COLOR;
                }
                else
                {
                    if (SHOW_MUTED && remote_console->isMuted[slot] == TRUE)
                    {
                        color = BLACK_COLOR;
                    }
                    else
                    {
                        color = WHITE_COLOR;
                    }
                }
                
                XUID remote_xuid = FindSelectedUser(selected_user);
                if (remote_console->xuid[slot] == remote_xuid)
                {
                    swprintf_s(buf, L"%s%s", L"> ", remote_console->gamertag[slot]);
                }
                else
                {
                    swprintf_s(buf, L"%s", remote_console->gamertag[slot]);
                }
                m_Font.DrawText( x, y, color, (const WCHAR *) buf );
                y += step_y; 

                // if to far down the screen push names into new column
                if (y > max_y)
                {
                    y = 64;
                    x += step_x;
                }
            }
        }
    }
    
    // Render the help text, hide mute/unmute if no remote console is present
    FLOAT fxExtent, fyExtent;
    const WCHAR* wstrActiveHelpText;

    if (remote_user_count > 0)
    {
        wstrActiveHelpText = wstrFullHelpText;
    }
    else
    {
        wstrActiveHelpText = wstrPartHelpText;
    }

    m_Font.GetTextExtent( wstrActiveHelpText, &fxExtent, &fyExtent );

    x = m_Font.m_rcWindow.x2 - m_Font.m_rcWindow.x1 - fxExtent;
    y = m_Font.m_rcWindow.y2 - m_Font.m_rcWindow.y1 - fyExtent;

    m_Font.DrawText(
        x,
        y,
        WHITE_COLOR,
        wstrActiveHelpText );

    m_Font.End();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// ReceiveMessage
//
// Attempt to receive a message from the network
//--------------------------------------------------------------------------------------
UMessage* LobbyChat::ReceiveMessage( SOCKADDR_IN* psaIn, SIZE_T* pcbRecv )
{
    INT cbRecv, cbIn, err;

    // Make sure we have a valid socket
    if( m_socket == INVALID_SOCKET )
    {
        ATG::FatalError( "Attempted to receive on an invalid socket.\n" );
    }

    // Try to retrieve a message from the socket
    cbIn = sizeof( SOCKADDR_IN );
    cbRecv = recvfrom(
        m_socket, ( PSTR )&message, sizeof( UMessage ), 0, ( sockaddr* )psaIn, &cbIn );

    // Check for errors
    if( cbRecv == SOCKET_ERROR )
    {
        err = WSAGetLastError();

        // WSAEWOULDBLOCK isn't an error, it just means there's no data waiting
        if( err != WSAEWOULDBLOCK )
        {
            ATG::FatalError( "Failed to recvfrom() socket, error %d.\n", err );
        }

        return NULL;
    }

    if( pcbRecv )
    {
        *pcbRecv = cbRecv;
    }

    return ( UMessage* )&message;
}


//--------------------------------------------------------------------------------------
// SendMessage
//
// Send a message to the network
//--------------------------------------------------------------------------------------
HRESULT LobbyChat::SendMessage( const IN_ADDR * sinPeer, SMessage* pMessage, SIZE_T cbMessage, unsigned short cbGameData )
{
    HRESULT hr = S_OK;

    SOCKADDR_IN sa;

    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = sinPeer->s_addr;
    sa.sin_port = htons( PORT_NUM );

    pMessage->cbGameData = cbGameData - sizeof(pMessage->cbGameData); // account for sizeof cbGameData

    INT cbSent = sendto(
        m_socket,
        ( PCSTR )pMessage,
        cbMessage,
        0,
        ( const sockaddr* )&sa,
        sizeof( sa ) );

    if (cbMessage != (SIZE_T) cbSent)
    {
        ATG::DebugSpew("size mismatch on send\n");
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// AddRemoteConsole
//
// add all remote users as sessions users and talkers.
//--------------------------------------------------------------------------------------
VOID LobbyChat::AddRemoteConsole(UMessage *msg)
{
    XNADDR xnaddr = msg->NewConsoleMessage.xnaddr;

    SRemoteConsole remote_console;
    ZeroMemory(&remote_console, sizeof(remote_console));

    // force key exchange with new remote console, and get real inaddr from xnaddr
    XNetXnAddrToInAddr(&xnaddr, &SessionInfo.sessionID, &remote_console.inaddr);

    // track remote console information
    memcpy_s(&remote_console.xnaddr, sizeof(remote_console.xnaddr), &xnaddr, sizeof(xnaddr));
    for (int slot=0; slot<XUSER_MAX_COUNT; slot++) 
    {
        memcpy_s(&remote_console.gamertag[slot], sizeof(remote_console.gamertag[slot]), msg->NewConsoleMessage.gamertag[slot], sizeof(msg->NewConsoleMessage.gamertag[slot]));
        remote_console.xuid[slot] = msg->NewConsoleMessage.xuid[slot];
        remote_console.isActiveTalker[slot] = msg->NewConsoleMessage.isActiveTalker[slot];
    }

    // register new peers as remote session users and remote XHV talkers
    for (int slot=0; slot<XUSER_MAX_COUNT; slot++)
    {
        if (msg->NewConsoleMessage.isActiveTalker[slot])
        {
            ATG::DebugSpew("new talker join from peer\n");
            ATG::DebugSpew("new talker join from peer\n");
            ATG::DebugSpew("remote talker is %ws xuid=%llx xnaddr=%x\n", msg->NewConsoleMessage.gamertag[slot], msg->NewConsoleMessage.xuid[slot], msg->NewConsoleMessage.xnaddr.ina.s_addr);

            BOOL PrivateSlots[1] = {FALSE};
            DWORD ret = XSessionJoinRemote(hSession, 1, &msg->NewConsoleMessage.xuid[slot], PrivateSlots, NULL);
            remote_user_count++;
            ATG::DebugSpew("Remote join code: %x\n", ret);

            // register remote talker
            if ( m_pXHV->RegisterRemoteTalker( msg->NewConsoleMessage.xuid[slot], NULL, NULL, NULL ) == S_OK )
            {
                m_pXHV->StartRemoteProcessingModes( msg->NewConsoleMessage.xuid[slot], &xhv_processing_mode, 1 );
                ATG::DebugSpew("RemoteTalkerRegistered\n");
            }
            else
            {
                ATG::DebugSpew("problem encountered when registering remote talker\n");
            }

            // populate session details
            GetSessionDetails();
        }
    }

    ATG::DebugSpew("Adding new remote console\n");
    m_RemoteConsoles.push_back(remote_console);

    // process remote users mute states
    ProcessMuteListChanged();
}


//--------------------------------------------------------------------------------------
// ProcessNewConsoleMessage
//
// new console request
//--------------------------------------------------------------------------------------
VOID LobbyChat::ProcessNewConsoleMessage(UMessage *pMessage)
{
    // create peer inaddr based on new console xnaddr
    XNetXnAddrToInAddr(&pMessage->NewConsoleMessage.xnaddr, &SessionInfo.sessionID, &m_sinPeer);

    // send join response to new peer
    SRecievedMessage RecievedMsg;
    ZeroMemory(&RecievedMsg, sizeof(RecievedMsg));
    RecievedMsg.id = MessageID_Recieved;
    RecievedMsg.status = SRecievedMessage::STATUS_JOINOK; 
    memcpy_s(&RecievedMsg.xnaddr, sizeof(RecievedMsg.xnaddr), &m_xnaddr, sizeof(m_xnaddr));

    // copy gamertags 
    for (int slot=0; slot<XUSER_MAX_COUNT; slot++)
    {
        memcpy_s(&RecievedMsg.gamertag[slot], sizeof(RecievedMsg.gamertag[slot]), &m_LocalTalkers[slot].gamertag, sizeof(m_LocalTalkers[slot].gamertag));
        RecievedMsg.xuid[slot] = m_LocalTalkers[slot].xuid;
        RecievedMsg.isActiveTalker[slot] = m_LocalTalkers[slot].isActiveTalker;
    }

    // send the approved/denied join response message to new remote peer console
    SendMessage(&m_sinPeer, &RecievedMsg, sizeof(RecievedMsg), sizeof(RecievedMsg));

    // if approved do peer sync up for new join
    if (RecievedMsg.status == SRecievedMessage::STATUS_JOINOK)
    {
        // send all current peers this new peers console information, send current peer info to newbie peer as well
        ATG::DebugSpew("Send new peer console info to all existing peer consoles\n");
        ATG::DebugSpew("Send out all remote console, local and remote talker info to new peer\n");

        for (RemoteConsoleList::iterator remote_console = m_RemoteConsoles.begin(); remote_console != m_RemoteConsoles.end(); remote_console++)
        {
            // part#1: send old peer the information for the new peer
            SendMessage(&remote_console->inaddr, (SMessage *) pMessage, sizeof(SRemoteConsole), sizeof(SRemoteConsole));

            // part#2: send new peer information for the old peer
            SNewConsoleMessage msg;
            ZeroMemory(&msg, sizeof(msg));

            msg.id = MessageID_NewConsole;
            memcpy_s(&msg.xnaddr, sizeof(msg.xnaddr), &remote_console->xnaddr, sizeof(remote_console->xnaddr));
            for (int slot=0; slot<XUSER_MAX_COUNT; slot++)
            {
                memcpy_s(&msg.gamertag[slot], sizeof(msg.gamertag[slot]), remote_console->gamertag[slot], sizeof(remote_console->gamertag[slot]));
                msg.xuid[slot] = remote_console->xuid[slot];
                msg.isActiveTalker[slot] = remote_console->isActiveTalker[slot];
            }
            SendMessage(&m_sinPeer, &msg, sizeof(msg), sizeof(msg));
        }

        // add Remote Console (last thing todo so we dont think new peer is old peer and send duplicate/redundant data)
        // send the message we recieved on the host on to the peers
        AddRemoteConsole(pMessage);

    } // end if approved...
}


//--------------------------------------------------------------------------------------
// RemovePlayerFromSession
//
// remove a player from the local session and communicate that with the host
//--------------------------------------------------------------------------------------
VOID LobbyChat::RemovePlayerFromSession(DWORD slot) 
{
    ATG::DebugSpew("Local user: %d leaving session\n", slot);
    DWORD UserIndicies[1];
    UserIndicies[0] = slot;

    // leave session
    XSessionLeaveLocal(hSession, 1, UserIndicies, NULL); 

    // unregister local talker
    if (m_LocalTalkers[slot].isActiveTalker == TRUE)
    {
        m_pXHV->UnregisterLocalTalker(slot);
    }

    // populate session details
    GetSessionDetails();

    // create console update message for removing local user... then send that update message to the host to process
    SUpdateConsoleMessage msg;
    msg.id = MessageID_ConsoleUpdate;
    msg.flag = SUpdateConsoleMessage::FLAG_REMOVEUSER;
    msg.xuid = m_LocalTalkers[slot].xuid;
    memcpy_s(&msg.gamertag, sizeof(msg.gamertag), m_LocalTalkers[slot].gamertag, sizeof(m_LocalTalkers[slot].gamertag));
    msg.slot = slot;
    memcpy_s(&msg.xnaddr, sizeof(msg.xnaddr), &m_xnaddr, sizeof(m_xnaddr));

    if (isHost)
    {
        for (RemoteConsoleList::iterator remote_console = m_RemoteConsoles.begin(); remote_console != m_RemoteConsoles.end(); remote_console++)
        {
            SendMessage(&remote_console->inaddr, &msg, sizeof(msg), sizeof(msg));
        }
    }
    else
    {
        ATG::DebugSpew("sending remove player update message to host\n");
        SendMessage(&host_inaddr, &msg, sizeof(msg), sizeof(msg));
    }

    // remove local talker info 
    m_LocalTalkers[slot].isActiveTalker = FALSE;
    m_LocalTalkers[slot].xuid = 0;
    ZeroMemory(&m_LocalTalkers[slot].gamertag, sizeof(m_LocalTalkers[slot].gamertag));

    // remove user from our mute lists
    LocalMuteList[slot].clear();
    RemoteMuteList[slot].clear();
}


//--------------------------------------------------------------------------------------
// ProcessUpdateConsoleMessage
//
// messages sent to host for processing, either add new remote console user,
// remote remote console user, or remove remote console 
//--------------------------------------------------------------------------------------
VOID LobbyChat::ProcessUpdateConsoleMessage(UMessage *pMessage)
{
    ATG::DebugSpew("Recieved console update message from console: %x\n", pMessage->UpdateConsoleMessage.xnaddr.ina.s_addr); 

    // update for remote user slot
    int slot = pMessage->UpdateConsoleMessage.slot;

    // first find the remote console data
    RemoteConsoleList::iterator remote_console;

    // find an open "remoteconsole" so that we can process the message and store that data
    for (remote_console = m_RemoteConsoles.begin(); remote_console != m_RemoteConsoles.end(); remote_console++)
    {
        if (memcmp(&remote_console->xnaddr, &pMessage->UpdateConsoleMessage.xnaddr, sizeof(pMessage->UpdateConsoleMessage.xnaddr)) == 0)
        {
            break;
        }
    }

    if (remote_console == m_RemoteConsoles.end())
    {
        ATG::FatalError("could not find remote console from message");
        return;
    }

    // register remote talker
    BOOL sessionslots[1] = {FALSE};
    DWORD ret = 0;
    DWORD remote_slot = 0;

    // process specific update message type (add, remove, remove-console)
    switch (pMessage->UpdateConsoleMessage.flag)
    {
    case SUpdateConsoleMessage::FLAG_ADDUSER:
        memcpy_s(&remote_console->gamertag[slot], sizeof(remote_console->gamertag[slot]), pMessage->UpdateConsoleMessage.gamertag, sizeof(pMessage->UpdateConsoleMessage.gamertag));
        remote_console->xuid[slot] = pMessage->UpdateConsoleMessage.xuid;
        remote_console->isActiveTalker[slot] = TRUE;

        ret = XSessionJoinRemote(hSession, 1, &pMessage->UpdateConsoleMessage.xuid, sessionslots, NULL);
        remote_user_count++;

        if ( m_pXHV->RegisterRemoteTalker( pMessage->UpdateConsoleMessage.xuid, NULL, NULL, NULL ) == S_OK )
        {
            m_pXHV->StartRemoteProcessingModes( pMessage->UpdateConsoleMessage.xuid, &xhv_processing_mode, 1 );
            ATG::DebugSpew("RemoteTalkerRegistered\n");
        }
        else
        {
            ATG::DebugSpew("problem encountered when registering remote talker\n");
        }

        // populate session details
        GetSessionDetails();

        // process the mute list in case we have the newly joined player muted
        ProcessMuteListChanged();

        break;

    case SUpdateConsoleMessage::FLAG_REMOVEUSER:
        // remove user from console data
        ZeroMemory(&remote_console->gamertag[slot], sizeof(remote_console->gamertag[slot]));
        remote_console->xuid[slot] = 0;
        remote_console->isActiveTalker[slot] = FALSE;
        remote_console->isMuted[slot] = FALSE;

        // take user out of session and remove from remote talkers
        XSessionLeaveRemote(hSession, 1, &pMessage->UpdateConsoleMessage.xuid, NULL);
        remote_user_count--;
        m_pXHV->UnregisterRemoteTalker(pMessage->UpdateConsoleMessage.xuid);

        // populate session details
        GetSessionDetails();

        // remove user from our mute lists
        LocalMuteList[slot].erase(pMessage->UpdateConsoleMessage.xuid);
        RemoteMuteList[slot].erase(pMessage->UpdateConsoleMessage.xuid);

        break;

    case SUpdateConsoleMessage::FLAG_MUTEDUSER:
        // match the remote xuid with the remote slot (not passed in this message)
        for (remote_slot = 0; remote_slot<XUSER_MAX_COUNT; remote_slot++)
        {
            if (remote_console->xuid[remote_slot] == pMessage->UpdateConsoleMessage.xuid)
            {
                break;
            }
        }

        // handle muting on this oposite side then mute originated... 
        ATG::DebugSpew("recieved console update for muteduser:  localslot: %d, remoteslot: %d, isMuted: %d\n", 
            slot, remote_slot, pMessage->UpdateConsoleMessage.isMuted);

        if (remote_slot > XUSER_MAX_COUNT)
        {
            ATG::FatalError("could not match remote xuid with a user, this should not occur");
        }

        if( pMessage->UpdateConsoleMessage.isMuted )
        {
            // handle mute request from remote peer user
            RemoteMuteList[slot].insert(pMessage->UpdateConsoleMessage.xuid);
            hr = m_pXHV->SetPlaybackPriority( pMessage->UpdateConsoleMessage.xuid, slot, XHV_PLAYBACK_PRIORITY_NEVER );
            remote_console->isMuted[remote_slot] = TRUE;
            ATG::DebugSpew("muting set playback priority never returned %x\nslot=%d xuid=%llx", hr, slot, pMessage->UpdateConsoleMessage.xuid);
        }
        else
        {
            // remote user tried to unmute, remove that user from remotemutelist
            RemoteMuteList[slot].erase(pMessage->UpdateConsoleMessage.xuid);

            // need to ensure that the remote unmute request doesnt unmute if local wants remote muted (that was fun to say)
            BOOL isMuted = FALSE;
            XUserMuteListQuery(slot, pMessage->UpdateConsoleMessage.xuid, &isMuted);

            // if we have state of unmuted for remote talker its ok to unmute.
            if (isMuted == FALSE)
            {
                m_pXHV->SetPlaybackPriority( pMessage->UpdateConsoleMessage.xuid, slot, XHV_PLAYBACK_PRIORITY_MAX );
                ATG::DebugSpew("muting set playback priority max returned %x\nslot=%d xuid=%llx", hr, slot, pMessage->UpdateConsoleMessage.xuid);
            }

            // if no local users have this remote talker muted now, go ahead and mark as unmuted
            remote_console->isMuted[remote_slot] = FALSE;
            for(DWORD i=0; i<XUSER_MAX_COUNT; i++)
            {
                // if we see that remote user still muted elseware, ensure its marked as muted still.
                if (LocalMuteList[i].find(pMessage->UpdateConsoleMessage.xuid) != LocalMuteList[i].end() ||
                    RemoteMuteList[i].find(pMessage->UpdateConsoleMessage.xuid) != RemoteMuteList[i].end())
                {
                    remote_console->isMuted[remote_slot] = TRUE;
                    break;
                }
            }
        }
        break;

    default:
        ATG::FatalError("unknown console update request");
        break;
    }
}


//--------------------------------------------------------------------------------------
// ProcessLocalControllerInputs
//
// handle local player controls, allowing add/remove of players to lobby
//--------------------------------------------------------------------------------------
VOID LobbyChat::ProcessLocalControllerInputs()
{
    DWORD ret;

    // check for local players adding/removing themselves from chat lobby
    // handle key input 
    for (int slot=0; slot<XUSER_MAX_COUNT; slot++) 
    {
        // no need to do anything if no buttons pushed.
        if (ATG::Input::m_Gamepads[slot].wPressedButtons == 0)
        {
            continue;
        }

        // ensure user pushing buttons is signed in
        if (XUserGetSigninState(slot) != eXUserSigninState_SignedInToLive)
        {
            ATG::DebugSpew("controller %d not signed in to live, please signin\n", slot); 
            continue; // onto next controller slot
        }

        if ((ATG::Input::m_Gamepads[slot].wPressedButtons & XINPUT_GAMEPAD_A) && m_LocalTalkers[slot].isActiveTalker == false)
        {
            // put user gamertag on console.
            char gamertag[XUSER_NAME_SIZE];
            wchar_t w_gamertag[XUSER_NAME_SIZE];

            XUserGetName(slot, gamertag, XUSER_NAME_SIZE);
            gamertag[XUSER_NAME_SIZE-1] = '\0';
            MultiByteToWideChar(CP_ACP, 0, gamertag, -1, w_gamertag, XUSER_NAME_SIZE);

            // add local talker 
            m_LocalTalkers[slot].isActiveTalker = TRUE;
            memcpy_s(m_LocalTalkers[slot].gamertag, sizeof(m_LocalTalkers[slot].gamertag), w_gamertag, sizeof(w_gamertag));
            XUserGetXUID(slot, &m_LocalTalkers[slot].xuid);

            ATG::DebugSpew("Local user %ws on slot %d joining session\n", w_gamertag, slot);
            DWORD UserIndicies[1];
            BOOL PrivateSlots[1] = {false};
            UserIndicies[0] = slot;

            // put local users into session.
            ret = XSessionJoinLocal(hSession, 1, UserIndicies, PrivateSlots, NULL);

            // register new local talker 
            BOOL result = FALSE;
            DWORD error = XUserCheckPrivilege( slot, XPRIVILEGE_COMMUNICATIONS, &result );

            if( ( error == ERROR_SUCCESS ) && ( result == FALSE ) )
            {
                error = XUserCheckPrivilege(slot, XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY, &result );
            }

            if( ( error == ERROR_SUCCESS ) && ( result == TRUE ) )
            {
                if( m_pXHV->RegisterLocalTalker( slot ) == S_OK )
                {
                    m_pXHV->StartLocalProcessingModes( slot, &xhv_processing_mode, 1 );
                }
            }

            // populate session details
            GetSessionDetails();

            // create console update message for adding new local user... then send that update message to the host to process
            SUpdateConsoleMessage msg;
            msg.id = MessageID_ConsoleUpdate;
            msg.flag = SUpdateConsoleMessage::FLAG_ADDUSER;
            msg.slot = slot;
            msg.xuid = m_LocalTalkers[slot].xuid;
            wcscpy_s(msg.gamertag, XUSER_NAME_SIZE, w_gamertag);
            memcpy_s(&msg.xnaddr, sizeof(msg.xnaddr), &m_xnaddr, sizeof(m_xnaddr));

            if (isHost)
            {
                // relay update to all remote peers
                for (RemoteConsoleList::iterator remote_console = m_RemoteConsoles.begin(); remote_console != m_RemoteConsoles.end(); remote_console++)
                {
                    SendMessage(&remote_console->inaddr, &msg, sizeof(msg), sizeof(msg)); 
                }
            }
            else
            {
                ATG::DebugSpew("sending add player update message to host\n");
                SendMessage(&host_inaddr, &msg, sizeof(msg), sizeof(msg));
            }

            // process the mute list in case the newly joined player has a remote player muted
            ProcessMuteListChanged();
        }
        else if ((ATG::Input::m_Gamepads[slot].wPressedButtons & XINPUT_GAMEPAD_B) && m_LocalTalkers[slot].isActiveTalker == TRUE)
        {
            RemovePlayerFromSession(slot);
        }

        if (m_LocalTalkers[slot].isActiveTalker == TRUE)
        {
            if (ATG::Input::m_Gamepads[slot].wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN)
            {
                if (remote_user_count != 0)
                {
                    if ((selected_user+1) < remote_user_count)
                    {
                        selected_user++;
                    }
                }
                else
                {
                    selected_user = 0;
                }
            }

            if (ATG::Input::m_Gamepads[slot].wPressedButtons & XINPUT_GAMEPAD_DPAD_UP) 
            {
                if (selected_user > 0)
                {
                    selected_user--;
                }
            }

            if (ATG::Input::m_Gamepads[slot].wPressedButtons & XINPUT_GAMEPAD_Y)
            {
                if (remote_user_count > 0)
                {
                    XUID remote_user = FindSelectedUser(selected_user);
                    XUserMuteListSetState(slot, remote_user, TRUE);
                }
            }

            if (ATG::Input::m_Gamepads[slot].wPressedButtons & XINPUT_GAMEPAD_X)
            {
                if (remote_user_count > 0)
                {
                    XUID remote_user = FindSelectedUser(selected_user);
                    XUserMuteListSetState(slot, remote_user, FALSE);
                }
            }
        }
    } 
}


//--------------------------------------------------------------------------------------
// ProcessLiveNotifications
//
// look for signin changes and muting changes, handle those two notifications only
//--------------------------------------------------------------------------------------
VOID LobbyChat::ProcessLiveNotifications()
{
    DWORD dwNotificationID;
    ULONG_PTR ulParam;

    if (XNotifyGetNext( hNotification, 0, &dwNotificationID, &ulParam ))
    {
        switch (dwNotificationID)
        {
        case XN_SYS_SIGNINCHANGED: // ulParm indicates which slot signed in, 0 if signed out
            ATG::DebugSpew("signin change detected %d\n", ulParam);

            // non zero ulParam indicates sign-in, only need to handle sign out really.
            if (!ulParam)
            {
                // walk all local users and verify that all active talkers still have valid xuid information
                for (int slot=0; slot<XUSER_MAX_COUNT; slot++)
                {
                    if (m_LocalTalkers[slot].isActiveTalker)
                    {
                        if (XUserGetSigninState(slot) != eXUserSigninState_SignedInToLive)
                        {
                            // found user that signed out, remove them
                            ATG::DebugSpew("signed out user detected: %ws  %llx\n", m_LocalTalkers[slot].gamertag, m_LocalTalkers[slot].xuid);
                            RemovePlayerFromSession(slot);
                        }
                    }
                }
            }
            break;

        // some local user muted a remote user
        case XN_SYS_MUTELISTCHANGED:
            ATG::DebugSpew("mutelist change detected %d\n", ulParam); //ulParam is not used per the documentation
            ProcessMuteListChanged();
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// ProcessMuteListChanged
//
// Determine the muting relationships for local/remote talker pairs
// done for new XN_SYS_MUTELISTCHANGED notifications as well as new console joins
//--------------------------------------------------------------------------------------
VOID LobbyChat::ProcessMuteListChanged()
{
    // iterate through each remote console
    for (RemoteConsoleList::iterator remote_console = m_RemoteConsoles.begin(); remote_console != m_RemoteConsoles.end(); remote_console++)
    {
        // iterate through each remote talker/slot on the remote console
        for (int remote_slot=0; remote_slot<XUSER_MAX_COUNT; remote_slot++)
        {
            // if that remote talker is active
            if (remote_console->isActiveTalker[remote_slot])
            {
                // check each controller signed in against each remote talker. 
                for (int local_slot=0; local_slot<XUSER_MAX_COUNT; local_slot++)
                {
                    if (m_LocalTalkers[local_slot].isActiveTalker)
                    {
                        // get mute state of local/remote talker pair
                        // update and send console update message when changes occur

                        BOOL isMuted = FALSE;
                        XUserMuteListQuery(local_slot, remote_console->xuid[remote_slot], &isMuted);

                        BOOL isLocalMuted = LocalMuteList[local_slot].find(remote_console->xuid[remote_slot]) != LocalMuteList[local_slot].end();
                        BOOL isRemoteMuted = RemoteMuteList[local_slot].find(remote_console->xuid[remote_slot]) != RemoteMuteList[local_slot].end();

                        // if mute status changed based on what we have stored and what we queried
                        if (isMuted != isLocalMuted)
                        {
                            // if we have state of unmuted for remote talker its ok to unmute.
                            if (isMuted == FALSE)
                            {
                                ATG::DebugSpew("found unmute for %ws to remote user %ws\n", m_LocalTalkers[local_slot].gamertag, remote_console->gamertag[remote_slot]);
                                LocalMuteList[local_slot].erase(remote_console->xuid[remote_slot]);

                                // if local user requested to mute remote user... and we do not see a bi-directional mute existing from that player... go ahead and unmute
                                if (isRemoteMuted == FALSE)
                                {
                                    hr = m_pXHV->SetPlaybackPriority( remote_console->xuid[remote_slot], local_slot, XHV_PLAYBACK_PRIORITY_MAX );
                                    ATG::DebugSpew("muting set playback priority max returned %d\n", hr);
                                }
                            }
                            // remote user is being muted
                            else 
                            {
                                ATG::DebugSpew("found mute for %ws to remote user %ws\n", m_LocalTalkers[local_slot].gamertag, remote_console->gamertag[remote_slot]);
                                LocalMuteList[local_slot].insert(remote_console->xuid[remote_slot]);
                                hr = m_pXHV->SetPlaybackPriority( remote_console->xuid[remote_slot], local_slot, XHV_PLAYBACK_PRIORITY_NEVER );
                                ATG::DebugSpew("muting set playback priority never returned %d\n", hr);

                            }

                            // build mute console update message
                            SUpdateConsoleMessage msg;
                            msg.id = MessageID_ConsoleUpdate;
                            msg.flag = SUpdateConsoleMessage::FLAG_MUTEDUSER;
                            msg.slot = remote_slot;
                            msg.xuid = m_LocalTalkers[local_slot].xuid;
                            msg.isMuted = isMuted;
                            memcpy_s(&msg.xnaddr, sizeof(msg.xnaddr), &m_xnaddr, sizeof(m_xnaddr));

                            if (isHost)
                            {
                                // relay update to all remote peers
                                for (RemoteConsoleList::iterator peer_console = m_RemoteConsoles.begin(); peer_console != m_RemoteConsoles.end(); peer_console++)
                                {
                                    SendMessage(&peer_console->inaddr, &msg, sizeof(msg), sizeof(msg)); 
                                }
                            }
                            else
                            {
                                ATG::DebugSpew("sending muted player update message to host.  localslot: %d remoteslot: %d, isMuted: %d\n", local_slot, remote_slot, isMuted);
                                SendMessage(&host_inaddr, &msg, sizeof(msg), sizeof(msg));
                            }

                        }
                    } 
                } 

                // if no local users have this remote talker muted now, go ahead and mark as unmuted
                remote_console->isMuted[remote_slot] = FALSE;
                for(DWORD i=0; i<XUSER_MAX_COUNT; i++)
                {
                    // if we see that remote user still muted elseware, ensure its marked as muted still.
                    if (LocalMuteList[i].find(remote_console->xuid[remote_slot]) != LocalMuteList[i].end() ||
                        RemoteMuteList[i].find(remote_console->xuid[remote_slot]) != RemoteMuteList[i].end())
                    {
                        remote_console->isMuted[remote_slot] = TRUE;
                        break;
                    }
                }
            } 
        } 
    } 
}


//--------------------------------------------------------------------------------------
// GetSessionDetails
//
// gather the current session detail information, used for displaying slot information
//--------------------------------------------------------------------------------------
VOID LobbyChat::GetSessionDetails()
{
    DWORD cbResultsBuffer = 0;
    if (pSessionDetails != NULL)
    {
        free(pSessionDetails);
        pSessionDetails = NULL;
    }

    XSessionGetDetails(hSession, &cbResultsBuffer, NULL, NULL);
    pSessionDetails = (XSESSION_LOCAL_DETAILS*) malloc(cbResultsBuffer);
    assert( pSessionDetails != NULL );
    XSessionGetDetails(hSession, &cbResultsBuffer, pSessionDetails, NULL);

    ATG::DebugSpew("available public slots: %d\n", pSessionDetails->dwAvailablePublicSlots);
    ATG::DebugSpew("available private slots: %d\n", pSessionDetails->dwAvailablePublicSlots);
}


//--------------------------------------------------------------------------------------
// CreateSessionAsPeer
//
// look for an existing session, if found, join it and create local session copy
//--------------------------------------------------------------------------------------
BOOL LobbyChat::CreateSessionAsPeer(DWORD ownerslot)
{
    isPeer = TRUE;

    session_owner_slot = ownerslot;

    // Get our own XNADDR 
    DWORD dwRet;
    do
    {
        dwRet = XNetGetTitleXnAddr( &m_xnaddr );
    } while( dwRet == XNET_GET_XNADDR_PENDING );

    if( dwRet & XNET_GET_XNADDR_NONE )
    {
        ATG::FatalError( "Unable to find an XNADDR.\n" );
    }

    ATG::DebugSpew("XNADDR: %02X:%02X:%02X:%02X:%02X:%02X\n",
        m_xnaddr.abEnet[0], m_xnaddr.abEnet[1], m_xnaddr.abEnet[2],
        m_xnaddr.abEnet[3], m_xnaddr.abEnet[4], m_xnaddr.abEnet[5] );

    DWORD cbResults = 0;
    DWORD ret;

    PXSESSION_SEARCHRESULT_HEADER pResults = NULL;

    static XUSER_PROPERTY aProperties[2];
    static XUSER_CONTEXT aContexts[3];

    aContexts[0].dwContextId = X_CONTEXT_GAME_TYPE;
    aContexts[0].dwValue     = X_CONTEXT_GAME_TYPE_STANDARD;
    aContexts[1].dwContextId = X_CONTEXT_GAME_MODE;
    aContexts[1].dwValue     = CONTEXT_GAME_MODE_DEATHMATCH;
    aContexts[2].dwContextId = CONTEXT_MAP;
    aContexts[2].dwValue     = LOBBY_MAP;

    ret = XSessionSearch(
        SESSION_MATCH_QUERY_FIND_MATCHES, // Procedure index
        ownerslot,                        // User index (controller slot)
        10,                               // Maximum results
        0,                                // Number of properties   (ignored)
        0,                                // Number of contexts     (ignored)
        NULL,                             // Properties             (ignored)
        NULL,                             // Contexts               (ignored)
        &cbResults,                       // Size of result buffer
        NULL,                             // Pointer to results     (ignored)
        NULL                              // This call is always synchronous
        );

    pResults = (PXSESSION_SEARCHRESULT_HEADER) malloc(cbResults);
    assert( pResults != NULL );
    ZeroMemory(pResults, cbResults); 

    // Next, call the function again with the exact same parameters, except 
    // this time use the modified buffer size and a pointer to a buffer that 
    // matches it.
    ret = XSessionSearch(
        SESSION_MATCH_QUERY_FIND_MATCHES,
        ownerslot,
        10,
        0, //number of properties
        3, //number of contexts
        aProperties,
        aContexts,
        &cbResults,         // Pass in the address of the size variable
        pResults,
        NULL                // synchronous model (otherwise this would be overlapped)
        );

    if (pResults->dwSearchResults == 0)
    {
        ATG::DebugSpew("Error: no search results found\n");
        isPeer = FALSE;
        free(pResults);
        return FALSE;
    }

    XUserSetContext(ownerslot, X_CONTEXT_GAME_TYPE, X_CONTEXT_GAME_TYPE_STANDARD);
    XUserSetContext(ownerslot, X_CONTEXT_GAME_MODE, CONTEXT_GAME_MODE_DEATHMATCH);
    XUserSetContext(ownerslot, CONTEXT_MAP, LOBBY_MAP);

    ZeroMemory(&SessionInfo, sizeof(SessionInfo));

    DWORD flags = session_flags; 

    // create peer copy of session
    ret = XSessionCreate(
        flags,
        ownerslot, 
        SLOTS_TOTALPUBLIC, 
        SLOTS_TOTALPRIVATE, 
        &SessionNonce,
        &pResults->pResults->info,
        NULL, 
        &hSession );

    memcpy_s(&SessionInfo, sizeof(SessionInfo), &pResults->pResults->info, sizeof(pResults->pResults->info));

    // Initialize XHV/XAudio2 and local talkers
    InitializeXaudio2();

    DWORD UserIndicies[4];
    BOOL  PrivateSlots[4] = {false, false, false, false};

    int LocalTalkerCount = 0;
    for (int i=0; i<XUSER_MAX_COUNT; i++)
    {
        if (m_LocalTalkers[i].isActiveTalker)
        {
            UserIndicies[LocalTalkerCount] = i;
            LocalTalkerCount++;
        }
    }

    // put local users in session 
    ret = XSessionJoinLocal(hSession, LocalTalkerCount, UserIndicies, PrivateSlots, NULL);

    // populate session details
    GetSessionDetails();

    ret = XNetXnAddrToInAddr(&pResults->pResults->info.hostAddress, &pResults->pResults->info.sessionID, &host_inaddr); 

    // send a join message to the host 
    SNewConsoleMessage msg;
    ZeroMemory(&msg, sizeof(msg));

    // Set the message ID 
    msg.id = MessageID_NewConsole;
    memcpy_s( &msg.xnaddr, sizeof(msg.xnaddr), &m_xnaddr, sizeof(m_xnaddr)); // copy this console xnaddr to found message

    // add all local talkers registered 
    for (DWORD slot=0; slot<XUSER_MAX_COUNT; slot++)
    {
        memcpy_s(&msg.gamertag[slot], sizeof(msg.gamertag[slot]), &m_LocalTalkers[slot].gamertag, sizeof(m_LocalTalkers[slot].gamertag));
        msg.xuid[slot] = m_LocalTalkers[slot].xuid;
        msg.isActiveTalker[slot] = m_LocalTalkers[slot].isActiveTalker;
    }

    if( FAILED( SendMessage( &host_inaddr, &msg, sizeof(msg), sizeof(msg) ) ) )
    {
        ATG::DebugSpew("failed to send message correctly\n"); 
    }

    free(pResults);

    return TRUE;
}


//--------------------------------------------------------------------------------------
// DoPeerWork
//
// called once per frame for peer
//--------------------------------------------------------------------------------------
VOID LobbyChat::DoPeerWork()
{
    // Check for A or B button pushes from local players
    ProcessLocalControllerInputs();

    // See if anyone's sent us anything
    SOCKADDR_IN sa;
    SIZE_T cbMessage;

    UMessage* pMessage = ReceiveMessage( &sa, &cbMessage );

    if( pMessage )
    {
        switch(pMessage->BaseMessage.id)
        {
        case MessageID_Recieved:
            // join response message 
            AddRemoteConsole(pMessage);
            break;

        case MessageID_NewConsole: 
            AddRemoteConsole(pMessage);
            break;

        case MessageID_ConsoleUpdate:
            ProcessUpdateConsoleMessage(pMessage);
            break;

        case MessageID_VoiceData:
            if (pMessage->VoiceMessage.xuid)
            {
                hr = m_pXHV->SubmitIncomingChatData(pMessage->VoiceMessage.xuid, pMessage->VoiceMessage.data, &pMessage->VoiceMessage.datasize); 
            }
            break;

        default:
            ATG::FatalError("unknown message id\n");
            break;
        }
    }

    ProcessLiveNotifications();

    // check for incoming voice data from local controls
    ProcessVoice();
}


//--------------------------------------------------------------------------------------
// CreateSessionAsHost
//
// create the host session and join it
//--------------------------------------------------------------------------------------
VOID LobbyChat::CreateSessionAsHost(DWORD ownerslot)
{
    isHost = TRUE;

    session_owner_slot = ownerslot;

    // Get our own XNADDR 
    DWORD dwRet;
    do
    {
        dwRet = XNetGetTitleXnAddr( &m_xnaddr );
    } while( dwRet == XNET_GET_XNADDR_PENDING );

    if( dwRet & XNET_GET_XNADDR_NONE )
    {
        ATG::FatalError( "Unable to find an XNADDR.\n" );
    }

    ATG::DebugSpew("XNADDR: %02X:%02X:%02X:%02X:%02X:%02X\n",
        m_xnaddr.abEnet[0], m_xnaddr.abEnet[1], m_xnaddr.abEnet[2],
        m_xnaddr.abEnet[3], m_xnaddr.abEnet[4], m_xnaddr.abEnet[5] );

    XUserSetContext(ownerslot, X_CONTEXT_GAME_TYPE, X_CONTEXT_GAME_TYPE_STANDARD);
    XUserSetContext(ownerslot, X_CONTEXT_GAME_MODE, CONTEXT_GAME_MODE_DEATHMATCH);
    XUserSetContext(ownerslot, CONTEXT_MAP, LOBBY_MAP);

    ZeroMemory(&SessionInfo, sizeof(SessionInfo)); 

    DWORD flags = XSESSION_CREATE_HOST | session_flags;

    DWORD ret = XSessionCreate(
        flags ,
        ownerslot, 
        SLOTS_TOTALPUBLIC, 
        SLOTS_TOTALPRIVATE, 
        &SessionNonce,
        &SessionInfo,
        NULL,
        &hSession );

    // Initialize XHV/XAudio2 and local talkers
    InitializeXaudio2();

    DWORD UserIndicies[4];
    BOOL  PrivateSlots[4] = {FALSE, FALSE, FALSE, FALSE};

    int LocalTalkerCount = 0;
    for (int i=0; i<XUSER_MAX_COUNT; i++)
    {
        if (m_LocalTalkers[i].isActiveTalker)
        {
            UserIndicies[LocalTalkerCount] = i;
            LocalTalkerCount++;
        }
    }

    // put local users into session. 
    ret = XSessionJoinLocal(hSession, LocalTalkerCount, UserIndicies, PrivateSlots, NULL);

    // populate session details
    GetSessionDetails();

    // put user gamertag on console.
    char gamertag[XUSER_NAME_SIZE];
    wchar_t w_gamertag[XUSER_NAME_SIZE];

    XUserGetName(ownerslot, gamertag, XUSER_NAME_SIZE); 
    gamertag[ XUSER_NAME_SIZE - 1 ] = '\0';
    MultiByteToWideChar(CP_ACP, 0, gamertag, -1, w_gamertag, XUSER_NAME_SIZE);

    ATG::DebugSpew("\n%ws is host, signed into slot %d\n", w_gamertag, ownerslot);
}


//--------------------------------------------------------------------------------------
// DoHostWork
//
// called once per frame for host
//--------------------------------------------------------------------------------------
VOID LobbyChat::DoHostWork()
{
    // Check for A or B button pushes from local players
    ProcessLocalControllerInputs();

    // See if anyone's sent us anything
    SOCKADDR_IN sa;
    SIZE_T cbMessage;

    UMessage* pMessage = ReceiveMessage( &sa, &cbMessage );

    // if we recieved a message, handle it
    if( pMessage )
    {
        switch (pMessage->BaseMessage.id)
        {
        case MessageID_NewConsole:
            ProcessNewConsoleMessage(pMessage);
            break;

        case MessageID_ConsoleUpdate:
            // local process update on host
            ProcessUpdateConsoleMessage(pMessage);

            // relay update to all active remote peers
            for (RemoteConsoleList::iterator remote_console = m_RemoteConsoles.begin(); remote_console != m_RemoteConsoles.end(); remote_console++)
            {
                // skip the console that sent us the console update 
                if (memcmp(&remote_console->xnaddr, &pMessage->NewConsoleMessage.xnaddr, sizeof(pMessage->NewConsoleMessage.xnaddr)) != 0)
                {
                    SendMessage(&remote_console->inaddr, (SMessage *) pMessage, sizeof(SUpdateConsoleMessage), sizeof(SUpdateConsoleMessage)); 
                }
            }
            break;

        case MessageID_VoiceData:
            if (pMessage->VoiceMessage.xuid)
            {
                hr = m_pXHV->SubmitIncomingChatData(pMessage->VoiceMessage.xuid, pMessage->VoiceMessage.data, &pMessage->VoiceMessage.datasize); 
            }
            break;

        default:
            ATG::FatalError("unknown message id\n");
            break;
        }
    }

    ProcessLiveNotifications();

    // check for incoming voice data
    ProcessVoice();
}


//--------------------------------------------------------------------------------------
// InitializeXaudio2
//
// initialize xaudio2, XHV2 engine, and then register all current local talkers
//--------------------------------------------------------------------------------------
VOID LobbyChat::InitializeXaudio2()
{
    // Initialize XAudio2
    UINT32 flags = 0;
    HRESULT hr = XAudio2Create( &pXAudio2, flags );

    if( FAILED( hr ) )
    {
        ATG::FatalError( "Failed to create XAudio2, error code 0x%08x\n", hr );
    }

    // Create a mastering voice 
    IXAudio2MasteringVoice *pMasteringVoice = NULL;
    hr = pXAudio2->CreateMasteringVoice( &pMasteringVoice );

    if( FAILED( hr ) )
    {
        ATG::FatalError( "Failed to create mastering voice, error code 0x%08x\n", hr );
    }

    // at this point we have "MasteringVoice" and XAudio2 initialized
    // onto creating the XHV2 engine

    HANDLE  m_hWorkerThread;
    XHV_PROCESSING_MODE rgMode = XHV_VOICECHAT_MODE;

    // Set up parameters for the voice chat engine
    XHV_INIT_PARAMS xhvParams = {0};
    xhvParams.dwMaxRemoteTalkers            = XHV_MAX_REMOTE_TALKERS;
    xhvParams.dwMaxLocalTalkers             = XHV_MAX_LOCAL_TALKERS;
    xhvParams.localTalkerEnabledModes       = &rgMode;
    xhvParams.remoteTalkerEnabledModes      = &rgMode;
    xhvParams.dwNumLocalTalkerEnabledModes  = 1;
    xhvParams.dwNumRemoteTalkerEnabledModes = 1;
    xhvParams.pXAudio2                      = pXAudio2;

    // Create the XHV2 engine
    hr = XHV2CreateEngine( &xhvParams, &m_hWorkerThread, &m_pXHV );

    if( FAILED( hr ) )
    {
        ATG::FatalError( "Failed to create XHV2, error code 0x%08x\n", hr );
    }

    // register local talkers
    // look at permissions, dont bother registering local talkers that should not talk
    for( DWORD local_player = 0; local_player < XUSER_MAX_COUNT; local_player++ )
    {
        if( XUserGetXUID(local_player, &m_LocalTalkers[local_player].xuid ) == ERROR_SUCCESS ) 
        {
            char gamertag[XUSER_NAME_SIZE];

            m_LocalTalkers[local_player].isActiveTalker = TRUE;
            XUserGetName(local_player, gamertag, XUSER_NAME_SIZE);
            MultiByteToWideChar(CP_ACP, 0, gamertag, -1, m_LocalTalkers[local_player].gamertag, XUSER_NAME_SIZE);

            BOOL result = FALSE;
            DWORD error = XUserCheckPrivilege( local_player, XPRIVILEGE_COMMUNICATIONS, &result );

            if( ( error == ERROR_SUCCESS ) && ( result == FALSE ) )
            {
                error = XUserCheckPrivilege(
                    local_player, XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY, &result );
            }

            if( ( error == ERROR_SUCCESS ) && ( result == TRUE ) )
            {
                if( m_pXHV->RegisterLocalTalker( local_player ) == S_OK )
                {
                    m_pXHV->StartLocalProcessingModes( local_player, &xhv_processing_mode, 1 );
                }
            }
        }
        else
        {
            // determined local player is not active talker/signed in
            m_LocalTalkers[local_player].isActiveTalker = FALSE;
        }
    }
}


//--------------------------------------------------------------------------------------
// ProcessVoice
//
// look at each local talker and determine if any local chat data is ready to go out to
// peers. if so grab it and then tell xhv its submitted. finally send it over the wire
//--------------------------------------------------------------------------------------
VOID LobbyChat::ProcessVoice()
{
    DWORD dwVoiceFlags = m_pXHV->GetDataReadyFlags();

    BYTE rawdata[MAX_VDP_DATA_SIZE];
    DWORD datasize;
    DWORD numberofpackets = 0; 

    // throttle send rate down
    if (GetTickCount() - prev_time < send_time)
        return;

    prev_time = GetTickCount();

    // look for new incoming voice data
    for( DWORD slot=0; slot<XUSER_MAX_COUNT; slot++ )
    {
        // data ready for local player slot
        if (dwVoiceFlags & (1 << slot))
        {
            datasize = MAX_VDP_DATA_SIZE;

            m_pXHV->GetLocalChatData(slot, rawdata, &datasize, &numberofpackets);
            if (numberofpackets != 0)
            {
                XUID localxuid;
                XUserGetXUID(slot, &localxuid);

                // send raw voice data
                SVoiceMessage msg;

                msg.id = MessageID_VoiceData;
                msg.xuid = localxuid;
                msg.datasize = datasize;
                memcpy_s(&msg.data, sizeof(msg.data), &rawdata, datasize); 

                // transmit local voice data to all remote peers
                for (RemoteConsoleList::iterator remote_console = m_RemoteConsoles.begin(); remote_console != m_RemoteConsoles.end(); remote_console++)
                {
                    if( FAILED( SendMessage( &remote_console->inaddr, &msg, (sizeof( msg ) - MAX_VDP_DATA_SIZE) + datasize, (unsigned short) (sizeof( msg ) - MAX_VDP_DATA_SIZE)) ) )
                    {
                        ATG::DebugSpew("failed to send voice data\n"); 
                    }
                }
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// FindSelectedUser
//
// traverse the remote consoles and slots structures to find this user
//--------------------------------------------------------------------------------------
XUID LobbyChat::FindSelectedUser(DWORD selected_user)
{
    XUID remote_user = 0;
    DWORD active_count = 0;

    for (RemoteConsoleList::iterator remote_console = m_RemoteConsoles.begin(); remote_console != m_RemoteConsoles.end() && remote_user == 0; remote_console++)
    {
        for (DWORD slot=0; slot<XUSER_MAX_COUNT; slot++) 
        {
            if (remote_console->isActiveTalker[slot])
            {
                if (active_count == selected_user)
                {
                    remote_user = remote_console->xuid[slot];
                    break;
                }
                active_count++;
            }
        }
    }

    return remote_user;
}
