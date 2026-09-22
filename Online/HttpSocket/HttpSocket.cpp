//--------------------------------------------------------------------------------------
// HttpSocket.cpp
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <winsockx.h>

#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgXmlParser.h>

#include "HttpClient.h"

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_DPAD, ATG::HELP_PLACEMENT_1, L"Move object" },

};
static const DWORD      NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );

// simple animation string to show when waiting
static const WCHAR*     gStrWaiting[] =
{
    L"-", L"\\", L"|", L"/"
};
#define WAITING_STRING ( gStrWaiting[ ( GetTickCount()>>6 ) & 0x3 ] )

// Scene load worker thread entrance
DWORD WINAPI LoadSceneThreadProc( LPVOID lpParameter );

#define LDEFAULT_HTTP_SERVER    L"Your http server"
#define LSUBMIT_PAGE            L"/XboxHttpSocketSubmit.asp"
#define LRESULT_PAGE            L"/XboxHttpSocketResult.asp"
#define LSUBMIT_DATABASE        L"/XboxHttpSocket.mdb"
#define LSCENE_XML_FILE         L"/HttpSocketScene.xml"

#define SUBMIT_PAGE             "/XboxHttpSocketSubmit.asp"
#define SCENE_XML_FILE          "/HttpSocketScene.xml"

#define SAFERELEASE( p )    if( ( p ) != NULL ) { ( p )->Release(); ( p ) = NULL; };

static const D3DCOLOR   COLOR_OBJECT = D3DCOLOR_ARGB( 0xFF, 0xFF, 0x00, 0x00 );
static const D3DCOLOR   COLOR_EXIT = D3DCOLOR_ARGB( 0xFF, 0xFF, 0xFF, 0x00 );
static const D3DCOLOR   COLOR_LINE = D3DCOLOR_ARGB( 0xFF, 0x00, 0xA0, 0x00 );
static const D3DCOLOR   COLOR_BACKGROUND = D3DCOLOR_ARGB( 0xFF, 0xFF, 0xFF, 0xFF );

static const D3DCOLOR   COLOR_TEXT_WHITE = D3DCOLOR_ARGB( 0xFF, 0xFF, 0xFF, 0xFF );
static const D3DCOLOR   COLOR_TEXT_YELLOW = D3DCOLOR_ARGB( 0xFF, 0xFF, 0xFF, 0x00 );
static const D3DCOLOR   COLOR_TEXT_RED = D3DCOLOR_ARGB( 0xFF, 0xFF, 0x00, 0x00 );


//--------------------------------------------------------------------------------------
// for Scene XML parse
//--------------------------------------------------------------------------------------
class SceneXMLFileCallback :
public ATG::ISAXCallback
{
public:

    static const int FILE_PATH_LENGTH = 128;

    virtual HRESULT  StartDocument() {     m_bFound = FALSE;    return ERROR_SUCCESS; };

    virtual HRESULT  EndDocument() {     return  ERROR_SUCCESS;    };

    virtual HRESULT  ElementBegin( CONST WCHAR* strName, UINT NameLen, CONST ATG::XMLAttribute* pAttributes, UINT NumAttributes )
    {
        WCHAR wAttName[ FILE_PATH_LENGTH ] = L"";

        if( NameLen >= FILE_PATH_LENGTH )
            return E_FAIL;
        else
            wcsncpy_s( wAttName, strName, NameLen );

        if( _wcsicmp( wAttName, L"SCENE" ) == 0 )
        {
            m_strMapFile[ 0 ] = 0;
            m_strBackgroundFile[ 0 ] = 0;

            BOOL bMapFound = FALSE;
            BOOL bBackgroundFound = FALSE;

            for( UINT i = 0; i < NumAttributes; ++i )
            {
                wcsncpy_s( wAttName, pAttributes[ i ].strName, pAttributes[ i ].NameLen );
                if( _wcsicmp( wAttName, L"MAP" ) == 0 )
                {
                    if( pAttributes[ i ].ValueLen < FILE_PATH_LENGTH )
                    {
                        WideCharToMultiByte( CP_ACP, 0,
                                             pAttributes[ i ].strValue, pAttributes[ i ].ValueLen,
                                             m_strMapFile, pAttributes[ i ].ValueLen,
                                             NULL, NULL );
                        m_strMapFile[ pAttributes[ i ].ValueLen ] = 0;
                        bMapFound = TRUE;
                    }
                }
                else if( _wcsicmp( wAttName, L"BACKGROUND" ) == 0 )
                {
                    if( pAttributes[ i ].ValueLen < FILE_PATH_LENGTH )
                    {
                        WideCharToMultiByte( CP_ACP, 0,
                                             pAttributes[ i ].strValue, pAttributes[ i ].ValueLen,
                                             m_strBackgroundFile, pAttributes[ i ].ValueLen,
                                             NULL, NULL );
                        m_strBackgroundFile[ pAttributes[ i ].ValueLen ] = 0;
                        bBackgroundFound = TRUE;
                    }
                }
            }

            m_bFound = bMapFound && bBackgroundFound;
            return ERROR_SUCCESS;
        }
        else
        {
            return E_FAIL;
        }
    };

    virtual HRESULT  ElementContent( CONST WCHAR*strData, UINT DataLen, BOOL More ) { return ERROR_SUCCESS; };

    virtual HRESULT  ElementEnd( CONST WCHAR* strName, UINT NameLen ) { return ERROR_SUCCESS; };

    virtual HRESULT  CDATABegin() { return ERROR_SUCCESS; };

    virtual HRESULT  CDATAData( CONST WCHAR* strCDATA, UINT CDATALen, BOOL bMore ) { return ERROR_SUCCESS; };

    virtual HRESULT  CDATAEnd() { return ERROR_SUCCESS; };

    virtual VOID     Error( HRESULT hError, CONST CHAR* strMessage )
    {
        OutputDebugString( "Error when Parsing user word XML\n" );
    };

    CHAR* GetMapFileName() { return m_strMapFile; };

    CHAR* GetBackgroundFileName() { return m_strBackgroundFile; };

    BOOL IsSceneFileNameFound() { return m_bFound; };

private:

    CHAR m_strMapFile[ FILE_PATH_LENGTH ];
    CHAR m_strBackgroundFile[ FILE_PATH_LENGTH ];
    BOOL m_bFound;

};

// Vertex dec
struct D3DVERTEX
{
    XMFLOAT3 p;           // Position
    D3DCOLOR c;           // Color
};


struct D3DVERTEX_TEX
{
    XMFLOAT3 p;           // Position
    D3DCOLOR c;           // Color
    FLOAT u;           // tex    
    FLOAT v;           // tex    
};

// Grid info
enum GRID_EUM
{
    GRID_SPACE,
    GRID_BLOCK,
    GRID_START,
    GRID_EXIT,
    GRID_UNKNOWN
};

// Game State
enum GAME_STATE_EUM
{
    GAME_STATE_INIT,
    GAME_STATE_LOADFAIL,
    GAME_STATE_SCENE_LOAD,
    GAME_STATE_PLAY,
    GAME_STATE_FINISH
};


// scene data
struct SCENEDATA
{
    BOOL bLoaded;
    XMSHORT4 objPosition;
    XMSHORT4 exitPosition;
    WORD width;
    WORD height;
    CHAR* pGridData;
    CHAR* pTextureData;
    INT nTextureDataLength;
};

//--------------------------------------------------------------------------------------
// Name: class GridMap
// Desc: All the graphics related part, buffers, map, background
//--------------------------------------------------------------------------------------
class GridMap
{
public:

            GridMap( D3DDevice* pDevice )
            {
                m_pDevice = pDevice;
            };
            ~GridMap()
            {
            };

    VOID    Initialize();

    HRESULT InitializeScene( SCENEDATA* pSceneData, FLOAT fAspectRatio );
    VOID    DestroyScene();

    HRESULT Update( FLOAT objX, FLOAT objZ );
    VOID    Render();

private:

    UINT m_nLineCount;
    UINT m_nBlockCount;

    // Render 
    LPDIRECT3DVERTEXSHADER9 m_pVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pPixelShader;
    LPDIRECT3DVERTEXDECLARATION9 m_pVertexDecl;

    LPDIRECT3DVERTEXSHADER9 m_pTextureVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pTexturePixelShader;
    LPDIRECT3DVERTEXDECLARATION9 m_pTextureVertexDecl;

    // Transform matrices
    XMMATRIX m_matWorld;             // World transform
    XMMATRIX m_matView;              // View transform
    XMMATRIX m_matProj;              // Projection transform
    XMMATRIX m_matViewProj;          // ViewProjection transform

    // Models for grid, blocks and moving objectsource, and listener
    LPDIRECT3DVERTEXBUFFER9 m_pvbBackground;        // background
    LPDIRECT3DVERTEXBUFFER9 m_pvbGrid;              // grid points
    LPDIRECT3DVERTEXBUFFER9 m_pvbObject;            // moving object
    LPDIRECT3DINDEXBUFFER9 m_pibBlock;             // blocks
    LPDIRECT3DINDEXBUFFER9 m_pibLine;              // lines
    LPDIRECT3DTEXTURE9 m_pTexture;             // texture

    D3DDevice* m_pDevice;

};



//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CAtgApplication base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:

    SCENEDATA* GetSceneData()
    {
        return &m_sceneData;
    };
    HttpClient* GetHttpClient()
    {
        return &m_HttpClient;
    };
    CHAR* GetHttpServerName()
    {
        return m_strHttpServer;
    };

    void            SetErrorInfo( const CHAR* strInfo )
    {
        MultiByteToWideChar( CP_ACP, 0, strInfo, -1, m_wStrInfo, ARRAYSIZE( m_wStrInfo ) );
    };

private:

    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    // Sub update functions in different game status
    HRESULT         UpdateInit( ATG::GAMEPAD* );
    HRESULT         UpdateLoad( ATG::GAMEPAD* );
    HRESULT         UpdatePlay( ATG::GAMEPAD* );
    HRESULT         UpdateFinish( ATG::GAMEPAD* );

    // Sub Render functions in different game status
    HRESULT         RenderInit();
    HRESULT         RenderLoad();
    HRESULT         RenderPlay();
    HRESULT         RenderFinish();


    VOID            DestroyScene();
    HRESULT         InitializeScene();
    INT             CheckGridInfo( XMSHORT4 vPosi, XMSHORT4 vMove );

    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    ATG::PackedResource m_Resource;
    BOOL m_bDrawHelp;


    XOVERLAPPED m_Overlapped;            // Overlapped struct for virtual keyboard
    WCHAR           m_wstrHttpServer[ HTTP_HOST_IP_STRING_LENGTH ];    // user input
    CHAR            m_strHttpServer[ HTTP_HOST_IP_STRING_LENGTH ];    // ascii
    BOOL m_bKeyboardActive;                                // flag of KB UI       

    // scene data
    SCENEDATA m_sceneData;
    // Http Client
    HttpClient m_HttpClient;

    GAME_STATE_EUM m_gameState;
    BOOL m_bGameStateChanged;
    HANDLE m_hLoadThread;

    WCHAR           m_wStrInfo[ 128 ];

    // statistics data to be sent to server
    INT m_stepMoved;
    DOUBLE m_gameTime;

    GridMap* m_pGridMap;
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//       Call WSACleanup() and XNetCleanup() for clean up
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();

}



//--------------------------------------------------------------------------------------
// Name: LoadSceneThreadProc
// Desc: The thread worker function for this sample.
//         It reads the scene data from the HTTP server
//--------------------------------------------------------------------------------------
DWORD WINAPI LoadSceneThreadProc( LPVOID lpParameter )
{
    OutputDebugString( "Load Scene Thread Running...\n" );
    HRESULT ret;

    CHAR strInfo[ 128 ];
    Sample* pSample = ( Sample* )lpParameter;

    if( pSample )
    {
        HttpClient* pHttpClient = pSample->GetHttpClient();
        SCENEDATA* pSceneData = pSample->GetSceneData();
        pSceneData->bLoaded = FALSE;

        // XML file
        ret = pHttpClient->GET( pSample->GetHttpServerName(), SCENE_XML_FILE );

        if( ret == E_PENDING )
        {
            while( pHttpClient->GetStatus() == HttpClient::HTTP_STATUS_BUSY )
                Sleep( 500 );
        }

        // error when receiving scene xml
        if( pHttpClient->GetStatus() != HttpClient::HTTP_STATUS_DONE )
        {
            if( pHttpClient->GetSocketErrorCode() )
                sprintf_s( strInfo, "Load %s failed!\r\nERROR %d",
                           SCENE_XML_FILE, pHttpClient->GetSocketErrorCode() );
            else
                sprintf_s( strInfo, "Load %s failed!\r\nHTTP %d",
                           SCENE_XML_FILE, pHttpClient->GetResponseCode() );

            pSample->SetErrorInfo( strInfo );
            OutputDebugString( strInfo );
            return S_FALSE;
        }

        ATG::XMLParser sceneParser;
        SceneXMLFileCallback sceneCallback;
        sceneParser.RegisterSAXCallbackInterface( &sceneCallback );

        UCHAR* pContent = pHttpClient->GetResponseContentData();
        UINT nLength = pHttpClient->GetResponseContentDataLength();

        // XML parse error
        sceneParser.ParseXMLBuffer( ( CHAR* )pContent, nLength );
        if( !sceneCallback.IsSceneFileNameFound() )
        {
            sprintf_s( strInfo, "Error in %s!", SCENE_XML_FILE ),
                pSample->SetErrorInfo( strInfo );
            OutputDebugString( strInfo );
            return S_FALSE;
        }


        // map file
        ret = pHttpClient->GET( pSample->GetHttpServerName(), sceneCallback.GetMapFileName() );

        if( ret == E_PENDING )
        {
            while( pHttpClient->GetStatus() == HttpClient::HTTP_STATUS_BUSY )
                Sleep( 500 );
        }

        // error when receiving map file
        if( pHttpClient->GetStatus() != HttpClient::HTTP_STATUS_DONE )
        {
            if( pHttpClient->GetSocketErrorCode() )
                sprintf_s( strInfo, "Load %s failed!\r\nERROR %d",
                           sceneCallback.GetMapFileName(), pHttpClient->GetSocketErrorCode() );
            else
                sprintf_s( strInfo, "Load %s failed!\r\nHTTP %d",
                           sceneCallback.GetMapFileName(), pHttpClient->GetResponseCode() );

            pSample->SetErrorInfo( strInfo );
            OutputDebugString( strInfo );
            return S_FALSE;
        }

        pContent = pHttpClient->GetResponseContentData();
        nLength = pHttpClient->GetResponseContentDataLength();

        pSceneData->width = 0;
        pSceneData->height = 0;

        if( nLength != 0 )
            pSceneData->pGridData = new char[ nLength ];


        WORD w = 0;
        WORD count = 0;
        for( UINT i = 0; i < nLength; ++i )
        {
            switch( *pContent++ )
            {
                case '\r':
                    break;
                case '\n':
                    if( w != 0 )
                        ++pSceneData->height;
                    pSceneData->width = max( pSceneData->width, w );
                    w = 0;
                    break;
                case ' ':
                    pSceneData->pGridData[ count++ ] = GRID_SPACE;
                    ++w;
                    break;
                case 'B':
                    pSceneData->pGridData[ count++ ] = GRID_BLOCK;
                    ++w;
                    break;
                case 'O':
                case 'X':
                    pSceneData->pGridData[ count++ ] = GRID_EXIT;
                    pSceneData->exitPosition.x = w;
                    pSceneData->exitPosition.y = 0;
                    pSceneData->exitPosition.z = pSceneData->height;
                    ++w;
                    break;
                case 'S':
                case 'E':
                    pSceneData->pGridData[ count++ ] = GRID_START;
                    pSceneData->objPosition.x = w;
                    pSceneData->objPosition.y = 0;
                    pSceneData->objPosition.z = pSceneData->height;
                    ++w;
                    break;
                default:
                    pSceneData->pGridData[ count++ ] = GRID_BLOCK;    // treat other char as BLOCK
                    ++w;
                    break;

            }
        }

        if( w != 0 )
            ++pSceneData->height;


        // ifexit not set, default is 0,0
        if( ( pSceneData->exitPosition.x == 0 ) && ( pSceneData->exitPosition.z == 0 ) )
            pSceneData->pGridData[ 0 ] = GRID_EXIT;

        // background
        ret = pHttpClient->GET( pSample->GetHttpServerName(),
                                sceneCallback.GetBackgroundFileName() );

        if( ret == E_PENDING )
        {
            while( pHttpClient->GetStatus() == HttpClient::HTTP_STATUS_BUSY )
                Sleep( 500 );       // Wait till the reponse is received from server
        }

        // error when receiving background texture
        if( pHttpClient->GetStatus() != HttpClient::HTTP_STATUS_DONE )
        {
            if( pHttpClient->GetSocketErrorCode() )
                sprintf_s( strInfo, "Load %s failed!\r\nERROR %d",
                           sceneCallback.GetBackgroundFileName(), pHttpClient->GetSocketErrorCode() );
            else
                sprintf_s( strInfo, "Load %s failed!\r\nHTTP %d",
                           sceneCallback.GetBackgroundFileName(), pHttpClient->GetResponseCode() );

            pSample->SetErrorInfo( strInfo );
            OutputDebugString( strInfo );
            return S_FALSE;
        }

        pContent = pHttpClient->GetResponseContentData();
        nLength = pHttpClient->GetResponseContentDataLength();

        if( nLength != 0 )
        {
            pSceneData->pTextureData = new char[ nLength ];
            if( pSceneData->pTextureData )
            {
                memcpy( pSceneData->pTextureData, pContent, nLength );
                pSceneData->nTextureDataLength = nLength;
            }

        }
        pSceneData->bLoaded = TRUE;
    }

    OutputDebugString( "Load Scene Thread Exiting...\n" );
    return S_OK;
}




//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    DWORD dwResult;
    XNetStartupParams xnsp;
    XNADDR xna;

    memset( &xnsp, 0, sizeof( xnsp ) );
    xnsp.cfgSizeOfStruct = sizeof( XNetStartupParams );
    xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;   // Allow unsecured communications. Only relevant on the Xbox 360 development kit, 
    // This flag is silently ignored when when running on retail hardware.
    dwResult = XNetStartup( &xnsp );
    if( dwResult != 0 )
        return E_FAIL;                              // error XNET startup

    do
    {
        dwResult = XNetGetTitleXnAddr( &xna );
    } while( dwResult == XNET_GET_XNADDR_PENDING );

    WORD wVersionRequested = MAKEWORD( 2, 2 );
    WSADATA wsaData;

    dwResult = WSAStartup( wVersionRequested, &wsaData );
    if( dwResult != 0 )
        return E_FAIL;                            // initialization failed

    // Confirm that version 2.2 is supported.
    // Note that ifthe implementation supported versions
    // greater than 2.2, it would still return 2.2 in
    // wVersion since that is the version we requested.
    if( LOBYTE( wsaData.wVersion ) != 2 ||
        HIBYTE( wsaData.wVersion ) != 2 )
    {
        // We only want 2.2 and this implementation doesn't
        // support it.  This will not happen in Xbox 360 Winsock.
        return E_FAIL;
    }

    // default Http server
    wcsncpy_s( m_wstrHttpServer, LDEFAULT_HTTP_SERVER, HTTP_HOST_IP_STRING_LENGTH );


    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the GridMap 
    m_pGridMap = new GridMap( m_pd3dDevice );
    if( m_pGridMap == NULL )
        return E_FAIL;

    // 
    m_pGridMap->Initialize();

    m_bDrawHelp = FALSE;

    m_gameState = GAME_STATE_INIT;

    m_bKeyboardActive = FALSE;

    m_hLoadThread = NULL;

    // scene data related

    m_sceneData.pGridData = NULL;
    m_sceneData.pTextureData = NULL;
    m_sceneData.nTextureDataLength = 0;

    m_sceneData.bLoaded = FALSE;
    m_sceneData.width = 0;
    m_sceneData.height = 0;

    // if no exit position, default is 0,0
    m_sceneData.exitPosition.x = 0;
    m_sceneData.exitPosition.y = 0;
    m_sceneData.exitPosition.z = 0;

    // if no obj position, default is 1,1
    m_sceneData.objPosition.x = 1;
    m_sceneData.objPosition.y = 0;
    m_sceneData.objPosition.z = 1;

    return ERROR_SUCCESS;
}


//--------------------------------------------------------------------------------------
// Name: CheckGridInfo()
// Desc: Check whether the obj can move to the dest block.or not
//--------------------------------------------------------------------------------------
INT Sample::CheckGridInfo( XMSHORT4 vPosi, XMSHORT4 vMove )
{
    XMSHORT4 vDest;
    vDest.x = vPosi.x + vMove.x;
    vDest.z = vPosi.z + vMove.z;

    if( ( vDest.z >= m_sceneData.height ) || ( vDest.z < 0 ) )
        return GRID_UNKNOWN;

    if( ( vDest.x >= m_sceneData.width ) || ( vDest.x < 0 ) )
        return GRID_UNKNOWN;

    int n = ( vDest.z * m_sceneData.width ) + vDest.x;
    return m_sceneData.pGridData[ n ];
}


//--------------------------------------------------------------------------------------
// Name: DestroyScene()
// Desc: Clean up the scene.
//--------------------------------------------------------------------------------------
VOID Sample::DestroyScene()
{
    if( m_sceneData.pGridData )
    {
        delete [] m_sceneData.pGridData;
        m_sceneData.pGridData = NULL;
    }

    if( m_sceneData.pTextureData )
    {
        delete [] m_sceneData.pTextureData;
        m_sceneData.pTextureData = NULL;
        m_sceneData.nTextureDataLength = 0;
    }

    m_pGridMap->DestroyScene();

    m_sceneData.bLoaded = FALSE;
    m_sceneData.width = 0;
    m_sceneData.height = 0;

    // if no exit position, default is 0,0
    m_sceneData.exitPosition.x = 0;
    m_sceneData.exitPosition.y = 0;
    m_sceneData.exitPosition.z = 0;

    // if no obj position, default is 1,1
    m_sceneData.objPosition.x = 1;
    m_sceneData.objPosition.y = 0;
    m_sceneData.objPosition.z = 1;
}


//--------------------------------------------------------------------------------------
// Name: InitializeScene()
// Desc: prepare the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeScene()
{
    m_stepMoved = 0;

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    return m_pGridMap->InitializeScene( &m_sceneData, fAspectRatio );

}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    GAME_STATE_EUM lastState = m_gameState;

    switch( m_gameState )
    {
        case GAME_STATE_INIT:
        case GAME_STATE_LOADFAIL:
            UpdateInit( pGamepad );
            break;

        case GAME_STATE_SCENE_LOAD:
            UpdateLoad( pGamepad );
            break;

        case GAME_STATE_PLAY:
            UpdatePlay( pGamepad );
            break;

        case GAME_STATE_FINISH:
            UpdateFinish( pGamepad );
            break;
    }

    m_bGameStateChanged = ( lastState == m_gameState ) ? FALSE : TRUE;


    // Toggle help
    // Pause the timer
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;

        if( m_bDrawHelp )   m_Timer.Stop();
        else
            m_Timer.Start();
    }

    return ERROR_SUCCESS;
}


//--------------------------------------------------------------------------------------
// Name: UpdateInit()
// Desc: Update process when in GAME_STATE_INIT & GAME_STATE_LOADFAIL
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateInit( ATG::GAMEPAD* pGamepad )
{

    if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A ) && !m_bKeyboardActive )
    {
        ZeroMemory( &m_Overlapped, sizeof( XOVERLAPPED ) );
        DWORD dwRet = XShowKeyboardUI( 0,                    // User from whom to accept input, can be USERINDEX_ANY
                                       VKBD_LATIN_FULL,      // Flags
                                       m_wstrHttpServer,     // Default entry text
                                       L"HttpSocket",        // Title text
                                       L"Enter the server hostname or ip:", // Prompt text
                                       m_wstrHttpServer,     // Result text
                                       ARRAYSIZE( m_wstrHttpServer ), // Size of result buffer in characters
                                       &m_Overlapped );      // Pointer to XOVERLAPPED object

        if( ERROR_IO_PENDING != dwRet )
        {
            return E_UNEXPECTED;
        }

        m_bKeyboardActive = TRUE;
    }

    if( m_bKeyboardActive )
    {
        if( XHasOverlappedIoCompleted( &m_Overlapped ) )
        {
            m_bKeyboardActive = FALSE;
            if( m_Overlapped.dwExtendedError == ERROR_SUCCESS )
            {
                int wlen = wcslen( m_wstrHttpServer ) + 1;
                WideCharToMultiByte( CP_ACP, 0, m_wstrHttpServer, wlen, m_strHttpServer, wlen, NULL, NULL );

                m_gameState = GAME_STATE_SCENE_LOAD;

            }
        }
    }

    return ERROR_SUCCESS;
}


//--------------------------------------------------------------------------------------
// Name: UpdateLoad()
// Desc: Update process when in GAME_STATE_SCENE_LOAD 
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateLoad( ATG::GAMEPAD* pGamepad )
{

    if( m_bGameStateChanged )
    {
        // destroy old scene data if any
        DestroyScene();

        // start the load thread
        m_hLoadThread = CreateThread( NULL, 0, LoadSceneThreadProc, ( VOID* )this, 0, NULL );
        if( !m_hLoadThread )
            return E_FAIL;
    }
    else if( m_hLoadThread )
    {
        // Check if load is done
        if( WaitForSingleObject( m_hLoadThread, 0 ) == WAIT_OBJECT_0 )
        {
            CloseHandle( m_hLoadThread );
            m_hLoadThread = NULL;

            if( m_sceneData.bLoaded )
            {
                // loaded ok
                InitializeScene();
                m_gameState = GAME_STATE_PLAY;
            }
            else
            {
                // loadfail, display message and retry
                m_gameState = GAME_STATE_LOADFAIL;
            }
        }
    }

    return ERROR_SUCCESS;

}


//--------------------------------------------------------------------------------------
// Name: UpdatePlay()
// Desc: Update process when in GAME_STATE_PLAY 
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdatePlay( ATG::GAMEPAD* pGamepad )
{
    // init the timer
    if( m_bGameStateChanged )
        m_Timer.Reset();
    else
        m_gameTime = m_Timer.GetAppTime();

    if( pGamepad->wPressedButtons & ( XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT |
                                      XINPUT_GAMEPAD_DPAD_RIGHT ) )
    {
        XMSHORT4 vMove;
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
            vMove.z = -1, vMove.x = 0;
        else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
            vMove.z = 1, vMove.x = 0;
        else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
            vMove.z = 0, vMove.x = -1;
        else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
            vMove.z = 0, vMove.x = 1;

        switch( CheckGridInfo( m_sceneData.objPosition, vMove ) )
        {
            case GRID_SPACE:
            case GRID_START:
                m_sceneData.objPosition.x = m_sceneData.objPosition.x + vMove.x;
                m_sceneData.objPosition.z = m_sceneData.objPosition.z + vMove.z;
                ++m_stepMoved;
                break;

            case GRID_EXIT:
                m_sceneData.objPosition.x = m_sceneData.objPosition.x + vMove.x;
                m_sceneData.objPosition.z = m_sceneData.objPosition.z + vMove.z;
                ++m_stepMoved;
                m_gameState = GAME_STATE_FINISH;
                break;
        }

        m_pGridMap->Update( ( FLOAT )m_sceneData.objPosition.x, ( FLOAT )m_sceneData.objPosition.z );

    }

    return ERROR_SUCCESS;
}


//--------------------------------------------------------------------------------------
// Name: UpdateFinish()
// Desc: Update process when in GAME_STATE_FINISH 
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateFinish( ATG::GAMEPAD* pGamepad )
{

    if( m_bGameStateChanged )
    {
        char buffer[512];
        sprintf_s( buffer, "Field1=%d&Field2=%0.02f&Submit=Submit\r\n", m_stepMoved, m_gameTime );
        m_HttpClient.POST( GetHttpServerName(), SUBMIT_PAGE, buffer, strlen( buffer ) );
    }

    if( m_HttpClient.GetStatus() != HttpClient::HTTP_STATUS_BUSY )
    {
        // press A to restart
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
            m_gameState = GAME_STATE_INIT;

    }

    return ERROR_SUCCESS;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        switch( m_gameState )
        {
            case GAME_STATE_INIT:
            case GAME_STATE_LOADFAIL:
                RenderInit();
                break;

            case GAME_STATE_SCENE_LOAD:
                RenderLoad();
                break;

            case GAME_STATE_PLAY:
                RenderPlay();
                break;

            case GAME_STATE_FINISH:
                RenderFinish();
                break;
        }
    }
    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return ERROR_SUCCESS;
}


//--------------------------------------------------------------------------------------
// Name: RenderInit()
// Desc: Render process when in GAME_STATE_INIT, GAME_STATE_LOADFAIL
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderInit()
{
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0, 0, COLOR_TEXT_WHITE, L"HttpSocket" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0, 0, COLOR_TEXT_YELLOW, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

    // Length of m_wstrHttpServeris up to 128, so 512 should be enough
    WCHAR wstrText[512];
    if( m_gameState != GAME_STATE_LOADFAIL )
    {
        swprintf_s( wstrText, L"This sample demostrates \r\nhttp communications \r\nvia unsecured socket. \r\n\r\nPlease enter the http server \r\nhostname or ip address.\r\n\r\nPress" GLYPH_A_BUTTON L"to continue." );
        m_Font.DrawText( 0, 50, COLOR_TEXT_WHITE, wstrText );
    }
    else
    {
        m_Font.DrawText( 0, 50, COLOR_TEXT_RED, m_wStrInfo );

        swprintf_s( wstrText, L"Please read sample documentation and\r\ncheck your http server, network settings.\r\n\r\nPress"
                                                   GLYPH_A_BUTTON L"to enter server \r\nhostname or ip address again." );
        m_Font.DrawText( 0, 100, COLOR_TEXT_YELLOW, wstrText );
    }

    m_Font.End();

    return ERROR_SUCCESS;
}


//--------------------------------------------------------------------------------------
// Name: RenderLoad()
// Desc: Render process when in GAME_STATE_SCENE_LOAD 
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderLoad()
{
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0, 0, COLOR_TEXT_WHITE, L"HttpSocket" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0, 0, COLOR_TEXT_YELLOW, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

    // Length of m_wstrHttpServeris up to 128, so 512 should be enough
    WCHAR strText[512];
    swprintf_s( strText, L"Loading game scene file:%s\r\nhttp://%s%s",
                WAITING_STRING, m_wstrHttpServer, LSCENE_XML_FILE );

    m_Font.DrawText( 0, 50, COLOR_TEXT_YELLOW, strText );

    m_Font.End();

    return ERROR_SUCCESS;
}


//--------------------------------------------------------------------------------------
// Name: Render_Play()
// Desc: Render process when in GAME_STATE_PLAY 
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderPlay()
{
    // Set default render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    m_pGridMap->Render();

    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0, 0, COLOR_TEXT_WHITE, L"HttpSocket" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0, 0, COLOR_TEXT_YELLOW, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

    // Length of m_wstrHttpServeris up to 128, so 512 should be enough
    WCHAR strText[512];
    swprintf_s( strText,
                L"Move the red object\r\nto yellow exit.\r\n\r\nSeconds: %0.02f\r\nSteps:  %d",
                m_gameTime, m_stepMoved );
    m_Font.DrawText( 0, 50, COLOR_TEXT_WHITE, strText );

    m_Font.End();

    return ERROR_SUCCESS;
}


//--------------------------------------------------------------------------------------
// Name: RenderFinish()
// Desc: Render process when in GAME_STATE_FINISH 
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderFinish()
{

    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0, 0, COLOR_TEXT_WHITE, L"HttpSocket" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0, 0, COLOR_TEXT_YELLOW, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

    // Length of m_wstrHttpServeris up to 128, so 512 should be enough
    WCHAR strText[ 512 ];
    switch( m_HttpClient.GetStatus() )
    {
        case HttpClient::HTTP_STATUS_READY:
        case HttpClient::HTTP_STATUS_BUSY:
            swprintf_s( strText,
                        L"Game finished in %d steps, %0.02f sec.\r\nNow sending result to http server: %s\r\nhttp://%s%s",
                        m_stepMoved, m_gameTime, WAITING_STRING, m_wstrHttpServer, LSUBMIT_PAGE );
            m_Font.DrawText( 0, 50, COLOR_TEXT_YELLOW, strText );
            break;

        case HttpClient::HTTP_STATUS_DONE:
            swprintf_s( strText,
                        L"Game finished in %d steps, %0.02f sec.\r\nResult has been sent to server.\r\nSee the result with your browser:\r\nhttp://%s%s",
                        m_stepMoved, m_gameTime, m_wstrHttpServer, LRESULT_PAGE );
            m_Font.DrawText( 0, 50, COLOR_TEXT_YELLOW, strText );
            m_Font.DrawText( 0, 200, COLOR_TEXT_WHITE, L"Press" GLYPH_A_BUTTON L"to start this demo again." );
            break;

        case HttpClient::HTTP_STATUS_ERROR:
            swprintf_s( strText, L"Game finished in %d steps, %0.02f sec.",
                        m_stepMoved, m_gameTime );
            m_Font.DrawText( 0, 50, COLOR_TEXT_WHITE, strText );
            if( m_HttpClient.GetSocketErrorCode() == S_OK )
                swprintf_s( strText,
                            L"Error sending result to server.\r\nHTTP %d\r\nCheck your http server( IIS ), OLE DB,\r\nDB file: %s, permission,\r\nand %s.",
                            m_HttpClient.GetResponseCode(), LSUBMIT_DATABASE, LSUBMIT_PAGE );
            else
                swprintf_s( strText,
                            L"Error sending result to server.\r\nERROR %d\r\nCheck your http server( IIS ), OLE DB,\r\nDB file: %s, permission,\r\nand %s.",
                            m_HttpClient.GetSocketErrorCode(), LSUBMIT_DATABASE, LSUBMIT_PAGE );

            m_Font.DrawText( 0, 80, COLOR_TEXT_RED, strText );
            swprintf_s( strText, L"Also try to open your browser:\r\nhttp://%s%s",
                        m_wstrHttpServer, LRESULT_PAGE );
            m_Font.DrawText( 0, 200, COLOR_TEXT_YELLOW, strText );
            m_Font.DrawText( 0, 250, COLOR_TEXT_WHITE, L"Press" GLYPH_A_BUTTON L" to start this demo again." );
            break;
    }

    m_Font.End();

    return ERROR_SUCCESS;
}



//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Update the moving object
//--------------------------------------------------------------------------------------
HRESULT GridMap::Update( FLOAT objX, FLOAT objZ )
{

    assert( m_pDevice != NULL );

    D3DVERTEX* pVertices;

    // update moving object
    if( FAILED( m_pvbObject->Lock( 0, 0, ( VOID** )&pVertices, 0 ) ) )
        return E_FAIL;

    pVertices->p = XMFLOAT3( objX, 0.0f, - objZ );
    ++pVertices;
    pVertices->p = XMFLOAT3( objX, 0.0f, - objZ - 1.0f );
    ++pVertices;
    pVertices->p = XMFLOAT3( objX + 1.0f, 0.0f, - objZ - 1.0f );
    ++pVertices;
    pVertices->p = XMFLOAT3( objX + 1.0f, 0.0f, - objZ );

    m_pvbObject->Unlock();

    return ERROR_SUCCESS;

};


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Render the grid map and backbround
//--------------------------------------------------------------------------------------
VOID GridMap::Render()
{
    assert( m_pDevice != NULL );

    // Vertex shader operations use transposed matrices
    XMMATRIX mat;
    mat = XMMatrixMultiply( m_matWorld, m_matViewProj );
    mat = XMMatrixTranspose( mat );

    // Set the vertex shader constants
    m_pDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&mat, 4 );


    // Draw background
    m_pDevice->SetVertexDeclaration( m_pTextureVertexDecl );
    if( m_pTexture )
    {
        // Set the shader, texture
        m_pDevice->SetVertexShader( m_pTextureVertexShader );
        m_pDevice->SetPixelShader( m_pTexturePixelShader );
        m_pDevice->SetTexture( 0, m_pTexture );
    }
    else
    {
        // Set the vertex shader, non texture
        m_pDevice->SetVertexShader( m_pVertexShader );
        m_pDevice->SetPixelShader( m_pPixelShader );
    }

    m_pDevice->SetStreamSource( 0, m_pvbBackground, 0, sizeof( D3DVERTEX_TEX ) );
    m_pDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );


    // Set the vertex shader
    m_pDevice->SetVertexShader( m_pVertexShader );

    // Set the pixel shader
    m_pDevice->SetPixelShader( m_pPixelShader );

    // Set the vertex declaration.
    m_pDevice->SetVertexDeclaration( m_pVertexDecl );
    m_pDevice->SetTexture( 0, NULL );


    // Draw blocks
    m_pDevice->SetStreamSource( 0, m_pvbGrid, 0, sizeof( D3DVERTEX ) );
    m_pDevice->SetIndices( m_pibBlock );
    m_pDevice->DrawIndexedPrimitive( D3DPT_QUADLIST, 0, 0, 0, 0, m_nBlockCount );

    // Draw  lines
    m_pDevice->SetStreamSource( 0, m_pvbGrid, 0, sizeof( D3DVERTEX ) );
    m_pDevice->SetIndices( m_pibLine );
    m_pDevice->DrawIndexedPrimitive( D3DPT_LINELIST, 0, 0, 0, 0, m_nLineCount );

    // Draw moving object
    m_pDevice->SetStreamSource( 0, m_pvbObject, 0, sizeof( D3DVERTEX ) );
    m_pDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );

    m_pDevice->SetStreamSource( 0, NULL, 0, sizeof( D3DVERTEX ) );
    m_pDevice->SetIndices( NULL );

};


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Load shader, Vertex Declaration
//--------------------------------------------------------------------------------------
VOID GridMap::Initialize()
{

    assert( m_pDevice != NULL );

    m_pvbGrid = NULL;
    m_pibBlock = NULL;
    m_pibLine = NULL;
    m_pvbObject = NULL;
    m_pvbBackground = NULL;
    m_pTexture = NULL;

    // Create shaders
    HRESULT hr;
    VOID* pCode = NULL;

    // Create vertex shader
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\Diffuse.xvu", &pCode ) ) )
        ATG::FatalError( "Shader file is not found\n" );
    if( FAILED( hr = m_pDevice->CreateVertexShader( ( DWORD* )pCode, &m_pVertexShader ) ) )
        ATG::FatalError( "Shader creation error\n" );
    ATG::UnloadFile( pCode );

    // Create pixel shader
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\Diffuse.xpu", &pCode ) ) )
        ATG::FatalError( "Shader file is not found\n" );
    if( FAILED( hr = m_pDevice->CreatePixelShader( ( DWORD* )pCode, &m_pPixelShader ) ) )
        ATG::FatalError( "Shader creation error\n" );
    ATG::UnloadFile( pCode );

    // Create vertex shader, texture
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\Texcoord.xvu", &pCode ) ) )
        ATG::FatalError( "Shader file is not found\n" );
    if( FAILED( hr = m_pDevice->CreateVertexShader( ( DWORD* )pCode, &m_pTextureVertexShader ) ) )
        ATG::FatalError( "Shader creation error\n" );
    ATG::UnloadFile( pCode );

    // Create pixel shader, texture
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\Texcoord.xpu", &pCode ) ) )
        ATG::FatalError( "Shader file is not found\n" );
    if( FAILED( hr = m_pDevice->CreatePixelShader( ( DWORD* )pCode, &m_pTexturePixelShader ) ) )
        ATG::FatalError( "Shader creation error\n" );
    ATG::UnloadFile( pCode );

    // Define the vertex elements and.
    // Create a vertex declaration from the element descriptions.
    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
        D3DDECL_END()
    };
    m_pDevice->CreateVertexDeclaration( VertexElements, &m_pVertexDecl );


    static const D3DVERTEXELEMENT9 TextureVertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR,    0 },
        { 0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    m_pDevice->CreateVertexDeclaration( TextureVertexElements, &m_pTextureVertexDecl );

};


//--------------------------------------------------------------------------------------
// Name: InitializeScene()
// Desc: Initialize shader, buffers based on map data
//--------------------------------------------------------------------------------------
HRESULT GridMap::InitializeScene( SCENEDATA* pSceneData, FLOAT fAspectRatio )
{
    assert( m_pDevice != NULL );
    assert( pSceneData != NULL );

    if( D3D_OK != D3DXCreateTextureFromFileInMemory( m_pDevice,
                                                     pSceneData->pTextureData,
                                                     pSceneData->nTextureDataLength,
                                                     &m_pTexture ) )
    {
        return E_FAIL;
    }


    m_nLineCount = ( pSceneData->width + 1 ) + ( pSceneData->height + 1 );
    m_nBlockCount = 0;

    // Create grid vertex buffers & Index buffers
    m_pDevice->CreateVertexBuffer( sizeof( D3DVERTEX ) * ( ( pSceneData->width + 1 ) * ( pSceneData->height + 1 ) +
                                                           4 ),
                                   0, 0, D3DPOOL_DEFAULT, &m_pvbGrid, NULL );

    m_pDevice->CreateVertexBuffer( sizeof( D3DVERTEX_TEX ) * 4,
                                   0, 0, D3DPOOL_DEFAULT, &m_pvbBackground, NULL );

    m_pDevice->CreateIndexBuffer( sizeof( WORD ) * ( pSceneData->width * pSceneData->height ) * 4,
                                  0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &m_pibBlock, NULL );

    m_pDevice->CreateIndexBuffer( sizeof( WORD ) * m_nLineCount * 2, 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &m_pibLine,
                                  NULL );

    // moving object vertex buffer
    m_pDevice->CreateVertexBuffer( sizeof( D3DVERTEX ) * 4, 0, 0, D3DPOOL_DEFAULT, &m_pvbObject, NULL );

    WORD i, j;

    D3DVERTEX* pVertices;

    // Fill the VB for the grid and line
    if( FAILED( m_pvbGrid->Lock( 0, 0, ( VOID** )&pVertices, 0 ) ) )
        return E_FAIL;

    for( j = 0; j <= pSceneData->height; ++j )
    {
        for( i = 0; i <= pSceneData->width; ++i )
        {
            pVertices->p = XMFLOAT3( ( float )i, 0.0f, ( float )( -j ) );
            pVertices->c = COLOR_LINE;
            ++pVertices;
        }
    }

    // Fill the VB for exit position
    pVertices->p = XMFLOAT3( ( float )pSceneData->exitPosition.x, 0.0f, ( float )- pSceneData->exitPosition.z );
    pVertices->c = COLOR_EXIT;
    ++pVertices;
    pVertices->p = XMFLOAT3( ( float )pSceneData->exitPosition.x, 0.0f, ( float )- pSceneData->exitPosition.z - 1.0f );
    pVertices->c = COLOR_EXIT;
    ++pVertices;
    pVertices->p = XMFLOAT3( ( float )pSceneData->exitPosition.x + 1.0f, 0.0f, ( float )- pSceneData->exitPosition.z -
                             1.0f );
    pVertices->c = COLOR_EXIT;
    ++pVertices;
    pVertices->p = XMFLOAT3( ( float )pSceneData->exitPosition.x + 1.0f, 0.0f, ( float )- pSceneData->exitPosition.z );
    pVertices->c = COLOR_EXIT;

    m_pvbGrid->Unlock();


    D3DVERTEX_TEX* pVerticesTex;

    if( FAILED( m_pvbBackground->Lock( 0, 0, ( VOID** )&pVerticesTex, 0 ) ) )
        return E_FAIL;

    // Fill the VB for background
    pVerticesTex->p = XMFLOAT3( -1.0f, 0.0f, +1.0f );
    pVerticesTex->c = COLOR_BACKGROUND;
    pVerticesTex->u = 0.0f;
    pVerticesTex->v = 0.0f;
    ++pVerticesTex;

    pVerticesTex->p = XMFLOAT3( pSceneData->width + 1.0f, 0.0f, +1.0f );
    pVerticesTex->c = COLOR_BACKGROUND;
    pVerticesTex->u = 1.0f;
    pVerticesTex->v = 0.0f;
    ++pVerticesTex;

    pVerticesTex->p = XMFLOAT3( pSceneData->width + 1.0f, 0.0f, - pSceneData->height - 1.0f );
    pVerticesTex->c = COLOR_BACKGROUND;
    pVerticesTex->u = 1.0f;
    pVerticesTex->v = 1.0f;
    ++pVerticesTex;

    pVerticesTex->p = XMFLOAT3( -1.0f, 0.0f, - pSceneData->height - 1.0f );
    pVerticesTex->c = COLOR_BACKGROUND;
    pVerticesTex->u = 0.0f;
    pVerticesTex->v = 1.0f;
    ++pVerticesTex;

    m_pvbBackground->Unlock();


    // Fill the VB for moving object
    if( FAILED( m_pvbObject->Lock( 0, 0, ( VOID** )&pVertices, 0 ) ) )
        return E_FAIL;

    pVertices->p = XMFLOAT3( ( float )pSceneData->objPosition.x, 0.0f, ( float )- pSceneData->objPosition.z );
    pVertices->c = COLOR_OBJECT;
    ++pVertices;
    pVertices->p = XMFLOAT3( ( float )pSceneData->objPosition.x, 0.0f, ( float )- pSceneData->objPosition.z - 1.0f );
    pVertices->c = COLOR_OBJECT;
    ++pVertices;
    pVertices->p = XMFLOAT3( ( float )pSceneData->objPosition.x + 1.0f, 0.0f, ( float )- pSceneData->objPosition.z -
                             1.0f );
    pVertices->c = COLOR_OBJECT;
    ++pVertices;
    pVertices->p = XMFLOAT3( ( float )pSceneData->objPosition.x + 1.0f, 0.0f, ( float )- pSceneData->objPosition.z );
    pVertices->c = COLOR_OBJECT;

    m_pvbObject->Unlock();


    // Fill the Index Buffer for grid block
    WORD* pIndices;
    if( FAILED( m_pibBlock->Lock( 0, 0, ( VOID** )&pIndices, 0 ) ) )
        return E_FAIL;

    // Fill the Index Buffer for EXIT place
    pIndices[ 0 ] = ( pSceneData->height + 1 ) * ( pSceneData->width + 1 );
    pIndices[ 1 ] = pIndices[ 0 ] + 1;
    pIndices[ 2 ] = pIndices[ 0 ] + 2;
    pIndices[ 3 ] = pIndices[ 0 ] + 3;
    ++m_nBlockCount;
    pIndices += 4;

    // Fill the Index Buffer for Block
    const char* p = pSceneData->pGridData;
    for( j = 0; j < pSceneData->height; ++j )
    {
        for( i = 0; i < pSceneData->width; ++i )
        {
            if( *p++ == GRID_BLOCK )
            {
                pIndices[0] = j * ( pSceneData->width + 1 ) + i;
                pIndices[1] = j * ( pSceneData->width + 1 ) + i + 1;
                pIndices[2] = ( j + 1 ) * ( pSceneData->width + 1 ) + i + 1;
                pIndices[3] = ( j + 1 ) * ( pSceneData->width + 1 ) + i;

                pIndices += 4;
                ++m_nBlockCount;
            }
        }
    }

    m_pibBlock->Unlock();


    // Grid line
    if( FAILED( m_pibLine->Lock( 0, 0, ( VOID** )&pIndices, 0 ) ) )
        return E_FAIL;

    for( i = 0; i <= pSceneData->width; ++i, pIndices += 2 )
    {
        pIndices[ 0 ] = i;
        pIndices[ 1 ] = i + ( pSceneData->width + 1 ) * ( pSceneData->height );
    }

    for( j = 0; j <= pSceneData->height; ++j, pIndices += 2 )
    {
        pIndices[ 0 ] = j * ( pSceneData->width + 1 );
        pIndices[ 1 ] = ( j + 1 ) * ( pSceneData->width + 1 ) - 1;
    }

    m_pibLine->Unlock();

    // Set the transform matrices
    XMVECTOR vEyePt = XMVectorSet( pSceneData->width / 4.0f, 45.0f, -pSceneData->height / 2.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( pSceneData->width / 4.0f, 0.0f, -pSceneData->height / 2.0f, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );

    m_matWorld = XMMatrixIdentity();
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, 1.0f, 10000.0f );
    m_matViewProj = XMMatrixMultiply( m_matView, m_matProj );

    return ERROR_SUCCESS;

};

//--------------------------------------------------------------------------------------
// Name: DestroyScene()
// Desc: Release the buffers
//--------------------------------------------------------------------------------------
VOID GridMap::DestroyScene()
{
    // all of the buffers
    SAFERELEASE( m_pTexture );
    SAFERELEASE( m_pvbGrid );
    SAFERELEASE( m_pibBlock );
    SAFERELEASE( m_pibLine );
    SAFERELEASE( m_pvbObject );
    SAFERELEASE( m_pvbBackground );
};



