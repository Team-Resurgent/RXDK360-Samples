//--------------------------------------------------------------------------------------
// Menu.cpp
//
//  This sample demonstrates the use of Asynchnous Cursors in Kinect using Asynchnous Command Buffers and Callbacks.  
//  This sample is a simplified version of the MenuUsingHover sample, focused on demonstrating two cursors:
//      With the left hand you see a quickly responsive cursor.  
//      With the right h and you see a cursor that is trailing a few frames behind.
//  Both cursors show hover selection using the slower method so that you can see the hover trailing behind the cursor
//  when selecting an item.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xnamath.h>
#include <xffb.h>
#include <xaudio2.h>
#include <nuihandles.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgSimpleShaders.h>
#include <AtgAudio.h>

#include "CameraManager.h"

static CONST FLOAT g_fTransparent = 0.6f;
static CONST INT   g_iCursorSize = 80;
static CONST INT   g_iMenuItems = 4;

NUI_HANDLES_ARMS g_HandlesArms;
CRITICAL_SECTION g_CritNuiHandles;

struct WidthHeight
{
    INT Width;
    INT Height;
};

LPDIRECT3DVERTEXSHADER9         g_pVertexShader = NULL;
LPDIRECT3DPIXELSHADER9          g_pPixelShader = NULL;
LPDIRECT3DPIXELSHADER9          g_pPixelShaderColor = NULL;
LPDIRECT3DVERTEXDECLARATION9    g_pVertexDeclaration = NULL;

static CONST WidthHeight        g_BackButtonLocation = { 1100, 100 };
static CONST WidthHeight        g_MenuLocation = { 640, 360 };
static CONST WidthHeight        g_StartButtonLocation = { 180, 620 };
static CONST WidthHeight        g_ScrollLeftButtonLocation = { 440, 620 };
static CONST WidthHeight        g_ScrollRightButtonLocation = { 840, 620 };
static CONST WidthHeight        g_ScrollUpButtonLocation = { 640, 520 };
static CONST INT                g_iSpaceBetweenMenuItems = 15;
static CONST FLOAT                      g_fHoverTimeToSubmit = 2.0f;
DWORD g_dwTrackinIdInvokingSystemGesture = 0;

D3DCOLOR    g_GlobalBackgroundColor1   = 0xff3f6385;
D3DCOLOR    g_GlobalBackgroundColor2   = 0xffcebdad;
D3DCOLOR    g_CursorOnRailColor[2]     = { 0xffFA883F, 0xff91C942 };

XMFLOAT2    g_fLeftHandCursor = XMFLOAT2( 0.0f, 0.0f );
XMFLOAT2    g_fRightHandCursor = XMFLOAT2( 0.0f, 0.0f );

FLOAT       g_fTimeLeftHovering = 0.0f;
FLOAT       g_fTimeRightHovering = 0.0f;
FLOAT       g_fElapsedTime = 0.0f;


static CONST DWORD                      g_iSkeletonPollingThreadHwThread = 2;
static CONST INT                        g_iNumOfAsyncCommandBuffers = 3;

D3DDevice*                              g_pSampleDevice;
IDirect3DCommandBuffer9*                g_pCursorCommandBuffers[g_iNumOfAsyncCommandBuffers];
IDirect3DAsyncCommandBufferCall9*       g_pAsyncCommandBufferCalls[2];
LONG                                    g_iLastCursorReady = 0;  //The last PCB Nuithread created
DWORD                                   g_iACBcallbackToUse = 0;

struct MeshVertexPT
{
public:
    XMFLOAT3 Position;
    XMFLOAT2 TexCoord;
    static size_t Size()
    {
        return sizeof( MeshVertexPT );
    }
};


//--------------------------------------------------------------------------------------
// Name: DrawImage()
// Desc: Draw a sprite with transparency and color
//--------------------------------------------------------------------------------------
VOID DrawImage( D3DRECT Rect, FLOAT fTransparent, D3DCOLOR color, LPDIRECT3DTEXTURE9 pTexture, D3DDevice* pDevice = g_pSampleDevice)
{
    MeshVertexPT* pVertexData = NULL;

    pDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    pDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    pDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    pDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );    

    pDevice->SetTexture( 0, pTexture );

    D3DXCOLOR d3dxColor = color;
    d3dxColor.a = fTransparent;
    pDevice->SetPixelShaderConstantF( 0, d3dxColor, 1 );

    pDevice->SetPixelShader( g_pPixelShader );
    pDevice->SetVertexShader( g_pVertexShader );
    pDevice->SetVertexDeclaration( g_pVertexDeclaration );

    pDevice->BeginVertices( D3DPT_RECTLIST, 3, sizeof( MeshVertexPT ), ( VOID** )&pVertexData );    
    assert( pVertexData != NULL );
    pVertexData[0].Position = XMFLOAT3( ( FLOAT )Rect.x1, ( FLOAT )Rect.y1, 0 );
    pVertexData[0].TexCoord = XMFLOAT2( 0, 0 );
    pVertexData[1].Position = XMFLOAT3( ( FLOAT )Rect.x2, ( FLOAT )Rect.y1, 0 );
    pVertexData[1].TexCoord = XMFLOAT2( 1, 0 );
    pVertexData[2].Position = XMFLOAT3( ( FLOAT )Rect.x1, ( FLOAT )Rect.y2, 0 );
    pVertexData[2].TexCoord = XMFLOAT2( 0, 1 );
    pDevice->EndVertices();

    pDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    pDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    pDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
            
}

struct HoverButton
{   

//--------------------------------------------------------------------------------------
// Name: Init()
// Desc: setup the button 
//--------------------------------------------------------------------------------------
    VOID Init(
        LPDIRECT3DTEXTURE9 pSelectedTexture,
        LPDIRECT3DTEXTURE9 pUnSelectedTexture,
        WidthHeight DrawLocation )
    {
        m_pTextures[0] = pSelectedTexture;
        if ( pUnSelectedTexture == NULL ) 
        {   
            m_pTextures[1] = pSelectedTexture;
        }
        else
        { 
            m_pTextures[1] = pUnSelectedTexture;
        }
        D3DSURFACE_DESC SurfaceDESC;
        m_pTextures[0]->GetLevelDesc( 0, &SurfaceDESC );
        m_ButtonDims.Width = (INT) SurfaceDESC.Width;
        m_ButtonDims.Height = (INT) SurfaceDESC.Height;
        WidthHeight halfDims;
        halfDims.Width = m_ButtonDims.Width / 2;
        halfDims.Height = m_ButtonDims.Height / 2;
        m_DrawLocation.x1 = DrawLocation.Width - halfDims.Width;
        m_DrawLocation.x2 = DrawLocation.Width + halfDims.Width;
        m_DrawLocation.y1 = DrawLocation.Height - halfDims.Height;
        m_DrawLocation.y2 = DrawLocation.Height + halfDims.Height;
        Reset();
    };
    
//--------------------------------------------------------------------------------------
// Name: Reset()
// Desc: reset all the state.
//--------------------------------------------------------------------------------------
    VOID Reset()
    {
        m_fTimeAsSelectedRight = 0.0f;
        m_fTimeAsSelectedLeft = 0.0f;
        m_bSelected = FALSE;
        m_bRightCursorHovering = FALSE;
        m_bLeftCursorHovering = FALSE;
    }

//--------------------------------------------------------------------------------------
// Name: Update Selected()
// Desc: test the hands against the button.  update the hover radius when a button is selected.
//--------------------------------------------------------------------------------------
    BOOL UpdateSelected( )
    {
        m_bSelected = FALSE;
        BOOL bRightHandIntersected = TRUE;
        BOOL bLeftHandIntersected = TRUE;
        // do the collision tests for both hands.
        if ( g_fRightHandCursor.x < m_DrawLocation.x1 || g_fRightHandCursor.x > m_DrawLocation.x2
            || g_fRightHandCursor.y < m_DrawLocation.y1 || g_fRightHandCursor.y > m_DrawLocation.y2 )
        {
            bRightHandIntersected = FALSE;
        }
        if ( g_fLeftHandCursor.x < m_DrawLocation.x1 || g_fLeftHandCursor.x > m_DrawLocation.x2
            || g_fLeftHandCursor.y < m_DrawLocation.y1 || g_fLeftHandCursor.y > m_DrawLocation.y2 )
        {
            bLeftHandIntersected = FALSE;
        }

        // We're allready selected. Increment the time and test to see if we selected
        if ( bLeftHandIntersected && m_fTimeAsSelectedLeft > 0 )
        {
            m_fTimeAsSelectedLeft += g_fElapsedTime;
            if ( m_fTimeAsSelectedLeft > g_fHoverTimeToSubmit ) 
            {
                m_bSelected = TRUE; 
                m_bLeftCursorHovering = FALSE;
                g_fTimeLeftHovering = 0.0f;
            }
            else
            {
                g_fTimeLeftHovering = m_fTimeAsSelectedLeft;
            }
        }
        // we're newly selected, reset the time as selected
        else if ( bLeftHandIntersected )
        {
            m_fTimeAsSelectedLeft = g_fElapsedTime;
            g_fTimeLeftHovering = m_fTimeAsSelectedLeft;
            m_bLeftCursorHovering = TRUE;
        }
        else 
        {
            m_bLeftCursorHovering = FALSE;
            m_fTimeAsSelectedLeft = 0.0f;
        }

        if ( bRightHandIntersected && m_fTimeAsSelectedRight > 0 )
        {
            m_fTimeAsSelectedRight += g_fElapsedTime;
            if ( m_fTimeAsSelectedRight > g_fHoverTimeToSubmit ) 
            {
                m_bSelected = TRUE;
                m_bRightCursorHovering = FALSE;
                g_fTimeRightHovering = 0.0f;
            }
            else
            {
                g_fTimeRightHovering = m_fTimeAsSelectedRight;
            }
        }
        else if ( bRightHandIntersected )
        {
            m_fTimeAsSelectedRight = g_fElapsedTime;
            g_fTimeRightHovering = m_fTimeAsSelectedRight;
            m_bRightCursorHovering = TRUE;
        }
        else 
        {
            m_bRightCursorHovering = FALSE;
            m_fTimeAsSelectedRight = 0.0f;
        }

        return m_bSelected;
    };
//--------------------------------------------------------------------------------------
// Name: DrawButton()
// Desc: Render the button based on whether it's active or not..
//--------------------------------------------------------------------------------------
    VOID DrawButton()
    {
        if ( m_bLeftCursorHovering || m_bRightCursorHovering ) 
        {
            DrawImage( m_DrawLocation, 1.0f, 0xFF00FFFF, m_pTextures[ 1 ] ); 
        }
        else 
        {
            DrawImage( m_DrawLocation, g_fTransparent, 0xFF00FFFF, m_pTextures[ 0 ] ); 
        }
    }
    

    LPDIRECT3DTEXTURE9 m_pTextures[2];
    WidthHeight m_ButtonDims;
    D3DRECT m_DrawLocation;
    FLOAT m_fTimeAsSelectedRight;
    FLOAT m_fTimeAsSelectedLeft;
    BOOL m_bSelected;
    BOOL m_bLeftCursorHovering;
    BOOL m_bRightCursorHovering;
};

const CHAR* m_strVertexShaderPassThru =
"struct VS_OUTPUT_TEX_COLOR             \n"
"{                                      \n"
"    float4  Pos     : POSITION;        \n"
"    float4  Color   : COLOR;           \n"
"    float2  Tex     : TEXCOORD0;       \n"
"};                                     \n"
"struct VS_INPUT_TEX_COLOR               \n"
"{                                       \n"
"    float4  vPos    : POSITION0;        \n"       
"    float4  vColor  : COLOR0;           \n"
"    float2  vTex    : TEXCOORD0;        \n"
"};                                      \n"
"                                        \n"
"VS_OUTPUT_TEX_COLOR main( VS_INPUT_TEX_COLOR In ) \n"
"{                              \n"
"    VS_OUTPUT_TEX_COLOR Out;   \n"
"    Out.Pos = In.vPos;         \n"
"    Out.Color = In.vColor;     \n"
"    Out.Tex = In.vTex;         \n"
"    return Out;                \n"
"};                             \n";

const CHAR* m_strPixelShader =
" sampler TextureSampler0 : register(s0);                                   \n"
" float4     g_fDrawColor : register(c0);                                   \n"
"                                                                           \n"
"float4 main( float2 InTex : TEXCOORD0 ) : COLOR                            \n"
"{                                                                          \n"
"                                                                           \n"
"    float4 depthsample = tex2D( TextureSampler0, InTex );                  \n"
"    if ( depthsample.a > g_fDrawColor.a ) depthsample.a = g_fDrawColor.a;  \n"
"    return depthsample;                                                    \n"
"};                                                                         \n";

const CHAR* m_strPixelShaderQuad =
" sampler TextureSampler0 : register(s0);                                   \n"
" float4     g_fDrawColor : register(c0);                                   \n"
"                                                                           \n"
"float4 main( float2 InTex : TEXCOORD0 ) : COLOR                            \n"
"{                                                                          \n"
"                                                                           \n"
"    float4 depthsample = tex2D( TextureSampler0, InTex );                  \n"
"    if ( depthsample.a > g_fDrawColor.a ) depthsample.a = g_fDrawColor.a;  \n"
"    return float4( g_fDrawColor.xyz, depthsample.a );                      \n"
"};                                                                         \n";

static const D3DVERTEXELEMENT9 g_PosTexCoordVertexElements[] =
{
    { 0,     0, D3DDECLTYPE_FLOAT3,     0,  D3DDECLUSAGE_POSITION,  0 },
    { 0,    12, D3DDECLTYPE_FLOAT2,     0,  D3DDECLUSAGE_TEXCOORD,  0 },
    D3DDECL_END()
};

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------l
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" }
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer              m_Timer;    // Timer
    ATG::Font               m_Font;     // Font for drawing text
    ATG::Help               m_Help;
    BOOL                    m_bDrawHelp;
    BOOL                    m_bDrawCursor;
    
    XMMATRIX                m_matView;
    XMMATRIX                m_matProj;
    CameraManager           m_CameraManager;
    HANDLE                  m_hNotification;
    BOOL                    m_bPlayerEngaged;
    HoverButton             m_MenuButton[g_iMenuItems];    
    LPDIRECT3DTEXTURE9      m_pCursorTexture;    
    WidthHeight             m_CursorDims;  
    ATG::PackedResource     m_Resource; // Bundled textures in a packed resource
    
    IDirect3DDevice9*       m_pD3DCommandBufferDevice;
    IDirect3DSurface9*      m_pRenderTarget;
    HANDLE                  m_hSkeletonTrackingUpdateThread;

    // see functions for comments  
    VOID UpdateHandles( FLOAT fElapsedTimeSinceLastSTUpdate );
    
    XMFLOAT2 GetXYForVector( FXMVECTOR vNormalizedSpace );
    VOID FindClosestGrip( NUI_HANDLES_GRIP** pClosestGrip, FLOAT& fLeftTransparency, FLOAT& fRightTransparency );
    
    VOID DrawImageColored( D3DRECT Rect, FLOAT fTransparent, D3DCOLOR color, LPDIRECT3DTEXTURE9 pTexture );
    VOID DrawAndUpdateGameState();
    VOID DrawAndUpdateTheMenu (); 
    VOID DrawRightCursorAndHoveringProgress();
    VOID ResetGlobalHoverState();

    VOID RenderCursorToCommandBuffer(const XMFLOAT2& Cursor, IDirect3DCommandBuffer9* pCommandBuffer );
    static DWORD SkeletonTrackingUpdateThread( LPVOID lpParameter );

public:
    Sample();
    ~Sample();

    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;    
    atgApp.m_dwDeviceCreationFlags |=     D3DCREATE_BUFFER_2_FRAMES;
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: DrawImageColored()
// Desc: Draw a sprite with transparency and color
//--------------------------------------------------------------------------------------
VOID Sample::DrawImageColored( D3DRECT Rect, FLOAT fTransparent, D3DCOLOR color, LPDIRECT3DTEXTURE9 pTexture )
{
    MeshVertexPT* pVertexData = NULL;
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTexture( 0, pTexture );

    D3DXCOLOR d3dxColor ;
    d3dxColor.r = D3DCOLOR_GETRED( color ) / 255.0f;
    d3dxColor.g = D3DCOLOR_GETGREEN( color ) / 255.0f;
    d3dxColor.b = D3DCOLOR_GETBLUE( color ) / 255.0f;
    d3dxColor.a = fTransparent;
    m_pd3dDevice->SetPixelShaderConstantF( 0, d3dxColor, 1 );

    m_pd3dDevice->SetPixelShader( g_pPixelShaderColor );
    m_pd3dDevice->SetVertexShader( g_pVertexShader );
    m_pd3dDevice->SetVertexDeclaration( g_pVertexDeclaration );

    m_pd3dDevice->BeginVertices( D3DPT_RECTLIST, 3, sizeof( MeshVertexPT ), ( VOID** )&pVertexData );    
    assert( pVertexData != NULL );
    pVertexData[0].Position = XMFLOAT3( ( FLOAT )Rect.x1, ( FLOAT )Rect.y1, 0 );
    pVertexData[0].TexCoord = XMFLOAT2( 0, 0 );
    pVertexData[1].Position = XMFLOAT3( ( FLOAT )Rect.x2, ( FLOAT )Rect.y1, 0 );
    pVertexData[1].TexCoord = XMFLOAT2( 1, 0 );
    pVertexData[2].Position = XMFLOAT3( ( FLOAT )Rect.x1, ( FLOAT )Rect.y2, 0 );
    pVertexData[2].TexCoord = XMFLOAT2( 0, 1 );
    m_pd3dDevice->EndVertices();

    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
            
}

//--------------------------------------------------------------------------------------
// Name: DeferredProcedureCallbackFromGpu()
// Desc: The callback is called by the GPU right before the AsynCommandBuffer is about 
//  to be executed.  This function then grabs the latest iIndexToUse and fixups the ACB 
//  with the latest, right before the GPU needs it.
//--------------------------------------------------------------------------------------
void DeferredProcedureCallbackFromGpu(DWORD Context)
{
    LONG iIndexToUse = (g_iLastCursorReady)%3;
    AcquireLockBarrier(); // Guarantee everything is correct version
    g_pAsyncCommandBufferCalls[Context]->FixupAndSignal( g_pCursorCommandBuffers[iIndexToUse], 0, 0 );        
}

//--------------------------------------------------------------------------------------
// Name: SkeletonTrackingUpdateThread(LPVOID lpParameter )
// Desc: This thread handles getting new events from Nui.  
//--------------------------------------------------------------------------------------
DWORD Sample::SkeletonTrackingUpdateThread( LPVOID lpParameter )
{    
    Sample* pSample = (Sample*)lpParameter;
    static FLOAT fElapsedTimeSinceLastSTUpdate = 0;
    pSample->m_pD3DCommandBufferDevice->AcquireThreadOwnership();    

    for ( ;; )
    {
    
        BOOL bNewSkeletonAndMaps = pSample->m_CameraManager.CheckForNewSkeletonAndDepthMaps( g_fElapsedTime, 0 );        
        // Get the elapsed time since previous rendered frame
        g_fElapsedTime = ( FLOAT )pSample->m_Timer.GetElapsedTime();
        fElapsedTimeSinceLastSTUpdate += g_fElapsedTime;
    
        if ( bNewSkeletonAndMaps )
        {
            // If the player is facing the camera, the player is engaged with the menu
            pSample->m_bPlayerEngaged = pSample->m_CameraManager.FacingCamera();                       
            pSample->UpdateHandles( fElapsedTimeSinceLastSTUpdate );
            fElapsedTimeSinceLastSTUpdate = 0.0f;
            pSample->m_CameraManager.ReleaseDepthMaps();
        }  
    }
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{

    g_pSampleDevice = m_pd3dDevice;

    m_bDrawHelp = FALSE;
    m_bPlayerEngaged = FALSE;
    m_bDrawCursor = TRUE;

    HRESULT hr;

  
    // Initialize simple shaders and set the renderstates.
    ATG::SimpleShaders::Initialize( NULL, NULL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    // Create the font
    if( FAILED( hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create font\n" );
        return hr;
    }

    // Create the help
    if( FAILED( hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create help\n" );
        return hr;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the textures resource
    if( FAILED( hr = m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create Resource.xpr\n" );
        return hr;
    }

    // Load the sprites for the menu
    m_pCursorTexture = m_Resource.GetTexture("Cursor");
  
    m_MenuButton[0].m_pTextures[0] = m_Resource.GetTexture("Menu1Selected");
    m_MenuButton[0].m_pTextures[1] = m_Resource.GetTexture("Menu1Neutral");
    m_MenuButton[1].m_pTextures[0] = m_Resource.GetTexture("Menu2Selected");
    m_MenuButton[1].m_pTextures[1] = m_Resource.GetTexture("Menu2Neutral");
    m_MenuButton[2].m_pTextures[0] = m_Resource.GetTexture("Menu3Selected");
    m_MenuButton[2].m_pTextures[1] = m_Resource.GetTexture("Menu3Neutral");
    m_MenuButton[3].m_pTextures[0] = m_Resource.GetTexture("Menu4Selected");
    m_MenuButton[3].m_pTextures[1] = m_Resource.GetTexture("Menu4Neutral");
    
    D3DSURFACE_DESC SurfaceDESC;
    m_MenuButton[0].m_pTextures[0]->GetLevelDesc( 0, &SurfaceDESC );
    for ( INT iIndex = 0; iIndex < g_iMenuItems; ++iIndex )
    {
        m_MenuButton[iIndex].m_ButtonDims.Width = (INT) SurfaceDESC.Width;
        m_MenuButton[iIndex].m_ButtonDims.Height = (INT) SurfaceDESC.Height;
        m_MenuButton[iIndex].m_fTimeAsSelectedLeft = 0.0f;
        m_MenuButton[iIndex].m_fTimeAsSelectedRight= 0.0f;
    }    
    
    INT iStartY = g_MenuLocation.Height - ( ( m_MenuButton[0].m_ButtonDims.Height + g_iSpaceBetweenMenuItems ) * g_iMenuItems ) / 2;

    INT iMenuX1 = g_MenuLocation.Width - m_MenuButton[0].m_ButtonDims.Width /2;
    INT iMenuX2 = g_MenuLocation.Width + m_MenuButton[0].m_ButtonDims.Width /2;
    for ( INT iIndex = 0; iIndex < g_iMenuItems; ++iIndex )
    {
        m_MenuButton[iIndex].m_DrawLocation.x1 = iMenuX1;
        m_MenuButton[iIndex].m_DrawLocation.x2 = iMenuX2;
        m_MenuButton[iIndex].m_DrawLocation.y1 = iStartY;
        iStartY+=m_MenuButton[0].m_ButtonDims.Height;
        m_MenuButton[iIndex].m_DrawLocation.y2 = iStartY;
        iStartY+=g_iSpaceBetweenMenuItems;
    }    

    m_CursorDims.Width = g_iCursorSize;
    m_CursorDims.Height = g_iCursorSize;

    // Initialize the natural input device
    if( FAILED( hr = m_CameraManager.InitializeCamera( m_pd3dDevice ) ) )
    {
        ATG_PrintError( "Couldn't create the natural input device.\n" );
        return hr;
    }

    // Set the transform matrices
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, .10f, 10000.0f );
    
    NuiHandlesArmsInit( &g_HandlesArms );
    
    ID3DXBuffer* pShaderCode = NULL;
    ID3DXBuffer* pErrorMsg = NULL;

    // Create the vertex shader
    D3DXCompileShader( m_strVertexShaderPassThru, 
        ( UINT )strlen( m_strVertexShaderPassThru ),  NULL, NULL, "main", "vs.3.0", 0, 
        &pShaderCode, &pErrorMsg, NULL );
    m_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
        &g_pVertexShader );
    pShaderCode->Release();

    // Create the pixel shader
    D3DXCompileShader( m_strPixelShader, 
        ( UINT )strlen( m_strPixelShader ), NULL, NULL, "main", "ps.3.0", 0,
        &pShaderCode, &pErrorMsg, NULL );        
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
        &g_pPixelShader );
    pShaderCode->Release();

    D3DXCompileShader( m_strPixelShaderQuad, 
        ( UINT )strlen( m_strPixelShader ), NULL, NULL, "main", "ps.3.0", 0,
        &pShaderCode, &pErrorMsg, NULL );        
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
        &g_pPixelShaderColor );
    pShaderCode->Release();

    m_pd3dDevice->CreateVertexDeclaration( g_PosTexCoordVertexElements, &g_pVertexDeclaration );
  
    m_pd3dDevice->GetRenderTarget(0, &m_pRenderTarget);

    // Create the CommandBuffer Device that will render the Async Cursor
    if( FAILED( hr = Direct3D_CreateDevice(0, D3DDEVTYPE_COMMAND_BUFFER, NULL, 0, NULL,
                    &m_pD3DCommandBufferDevice )))
    {
        ATG_PrintError( "Failed to create command buffer device.\n" );
        return hr;
    }
    m_pD3DCommandBufferDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pD3DCommandBufferDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pD3DCommandBufferDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
   
    // Create the 3 CommandBuffers that will be used.
    //  We need 3 to ensure that: 
    //         1 can be used by the GPU, 
    //         1 can be used by the Updating thread, and 
    //         1 can be updated if the Update thread finishes updating before the GPU is finished with the first.
    //     Note: it is unprobable but if the Update thread some how ran more frequently than the render thread, the update 
    //     thread could overwrite the one used by the GPU and this would be bad.
       
    for(INT iIndex = 0; iIndex < g_iNumOfAsyncCommandBuffers; ++iIndex)
    {
        if( FAILED( hr = m_pD3DCommandBufferDevice->CreateCommandBuffer(256 * 1024, 0, &g_pCursorCommandBuffers[iIndex])))
        {
            ATG_PrintError( "Failed to create command buffer." );
            return hr;
        }    

        // Initalize the Commandbuffers to draw a cursor        
        XMFLOAT2 vInit;
        vInit.x = vInit.y = 0.0f;
        RenderCursorToCommandBuffer( vInit, g_pCursorCommandBuffers[iIndex]); 
    }

    // At most we can be buffered for two frames, so we need 2 Command buffer Calls.
    for(INT iIndex = 0; iIndex < 2; ++iIndex)
    {
        // Createthe AsynCommandBufferCalls
        if( FAILED( hr = m_pD3DCommandBufferDevice->CreateAsyncCommandBufferCall( NULL, NULL, 1, 0, &g_pAsyncCommandBufferCalls[iIndex] ) ))
        {
            ATG_PrintError( "Failed to create command buffer call." );
            return hr;
        }  

    }
    
    // Release thread ownership so the new thread can create command buffers.
    m_pD3DCommandBufferDevice->ReleaseThreadOwnership();    

    // Create a critical section for handing off data between threads
    // This is not being demonstrated in the sample, there are better ways to do this.
    InitializeCriticalSection( &g_CritNuiHandles );    

    // Create the thread to process the NuiEvent
    m_hSkeletonTrackingUpdateThread = CreateThread( NULL, 0, SkeletonTrackingUpdateThread, this, CREATE_SUSPENDED, NULL );
    if (!m_hSkeletonTrackingUpdateThread)
    {
         ATG_PrintError( "Failed to create SkeletonTrackingUpdateThread." );
         return E_FAIL;
    }
    ATG::SetThreadName( GetCurrentThreadId(), "SkeletonTrackingUpdateThread" );
    XSetThreadProcessor( m_hSkeletonTrackingUpdateThread, g_iSkeletonPollingThreadHwThread );
    ResumeThread( m_hSkeletonTrackingUpdateThread );    

    return S_OK;
} 

//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{    
    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;    

    // Update the view from the person's position and facing direction.
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: RenderCursorToCommandBuffer()
// Desc: This is used on the skeleton update thread to create the command buffers that will
//      be patched up when DeferredProcedureCallbackFromGpu executes.
//--------------------------------------------------------------------------------------
VOID Sample::RenderCursorToCommandBuffer(const XMFLOAT2& Cursor, IDirect3DCommandBuffer9* pCommandBuffer )
{    
        D3DRECT rect;        

        rect.x1 = (LONG)Cursor.x - m_CursorDims.Width / 2;
        rect.x2 = (LONG)Cursor.x + m_CursorDims.Width / 2;          
        rect.y1 = (LONG)Cursor.y - m_CursorDims.Height / 2;
        rect.y2 = (LONG)Cursor.y + m_CursorDims.Height / 2;      
        
        D3DTAGCOLLECTION InheritTags = { 0 };
        m_pD3DCommandBufferDevice->BeginCommandBuffer(pCommandBuffer, 0, &InheritTags, NULL, NULL, 0 );        
        m_pD3DCommandBufferDevice->SetRenderTarget(0, m_pRenderTarget);
        DrawImage( rect, 0.8f, g_CursorOnRailColor[0], m_pCursorTexture, m_pD3DCommandBufferDevice);        
        if( FAILED(m_pD3DCommandBufferDevice->EndCommandBuffer()))    
        {
            ATG_PrintError( "BeginCommandBuffer failed.\n" );
        }    
}

//--------------------------------------------------------------------------------------
// Name: RenderCursorToCommandBuffer()
// Desc: This is used on the skeleton update thread to update the handles
//--------------------------------------------------------------------------------------
VOID Sample::UpdateHandles( FLOAT fElapsedTimeSinceLastSTUpdate )
{

    const NUI_SKELETON_DATA* pSkeletonData;

    // Update if skeleton was tracked
    pSkeletonData = m_CameraManager.GetTrackedSkeleton();
    if ( pSkeletonData == NULL )
    {
        return;
    }    
    
    XMFLOAT2 fLeftHandCursor;
    
    // This critical section is used to access data updated by the NuiUpdate thread.
    // There are better ways to hand the data to the update thread and they are out of scope of this sample.
    EnterCriticalSection( &g_CritNuiHandles);   
    
    // Each frame, update the arms based on the joints
    // NOTE: This API requires raw/unfiltered joint positions.        
    NuiHandlesArmsUpdate( &g_HandlesArms, m_CameraManager.GetActiveSkeletonSkeletonDataIndex(), 
        m_CameraManager.GetSkeletonFrame(), m_CameraManager.GetDepthFrame320x240(), NULL,
        m_CameraManager.GetDepthFrame80x60(), NULL );

    fLeftHandCursor = GetXYForVector( NuiHandlesArmGetScreenSpaceLocation( &g_HandlesArms, NUI_HANDLES_ARMS_HANDEDNESS_LEFT_ARM ) );    
    
    LeaveCriticalSection( &g_CritNuiHandles);
    PIXBeginNamedEvent(0, "Cursor CB Update");
    RenderCursorToCommandBuffer( fLeftHandCursor, g_pCursorCommandBuffers[(g_iLastCursorReady + 1) % g_iNumOfAsyncCommandBuffers]);    
    InterlockedIncrementRelease( &g_iLastCursorReady );
    PIXEndNamedEvent();    
}

//--------------------------------------------------------------------------------------
// Name: GetXYForVector()
// Desc: Map normalized space to screen space.
//       
//--------------------------------------------------------------------------------------
XMFLOAT2 Sample::GetXYForVector( FXMVECTOR vNormalizedSpace )
{
    XMFLOAT2 vXYvector;
    vXYvector.x = XMVectorGetX( vNormalizedSpace ) * 640.0f + 640.0f;           
    vXYvector.y = 720 - ( XMVectorGetY( vNormalizedSpace ) * 360.0f + 360.0f);
    return vXYvector;
}

//--------------------------------------------------------------------------------------
// Name: DrawAndUpdateTheMenu()
// Desc: Draw main menu
//--------------------------------------------------------------------------------------
VOID Sample::DrawAndUpdateTheMenu ()
{
 
    PIXBeginNamedEvent( 0, "Menu" );

    for(INT iIndex = 0; iIndex < 2; ++iIndex)
    {
        if ( m_MenuButton[iIndex].UpdateSelected() )
        {
            m_MenuButton[iIndex].Reset();
        }
    }
    if ( m_MenuButton[2].UpdateSelected() )
    {
        XLaunchNewImage( "", 0 );
    }
    if ( m_MenuButton[3].UpdateSelected() )
    {
        XShowNuiGuideUI( g_dwTrackinIdInvokingSystemGesture );
        m_MenuButton[3].Reset();
    }
    
    BOOL bLeftHovering = FALSE;
    BOOL bRightHovering = FALSE;
    for ( UINT uIndex = 0; uIndex < g_iMenuItems; ++uIndex )
    {
        if ( m_MenuButton[uIndex].m_bLeftCursorHovering ) bLeftHovering = TRUE;
        if ( m_MenuButton[uIndex].m_bRightCursorHovering ) bRightHovering = TRUE;
    }

    if ( !bLeftHovering )
    {
        // reset hover when nothing is selected
        g_fTimeLeftHovering = 0.0f;
    }
    if ( !bRightHovering )
    {
        // reset hover when nothing is selected
        g_fTimeRightHovering = 0.0f;
    }

    for ( UINT uIndex = 0; uIndex < g_iMenuItems; ++uIndex )
    {
        m_MenuButton[uIndex].DrawButton();
    }
    PIXEndNamedEvent();    
}

//--------------------------------------------------------------------------------------
// Name: DrawAndUpdateGameState()
// Desc: Render the screen's UI controls
//--------------------------------------------------------------------------------------
VOID Sample::DrawAndUpdateGameState()
{
    PIXBeginNamedEvent( 0, "Draw UI" );

    m_pd3dDevice->SetVertexDeclaration( g_pVertexDeclaration );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAXANISOTROPY, 16 );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );

    m_bDrawCursor = TRUE;

    DrawAndUpdateTheMenu(); 

    // If the player is not engaged / facing the camera, then don't render the cursor    
    if ( m_bPlayerEngaged && m_bDrawCursor )
    {         
        PIXBeginNamedEvent(0,"Async Cursor Draw");

        // Reset the AsyncCommand Callbacks Callback to be ready to use
        g_pAsyncCommandBufferCalls[g_iACBcallbackToUse]->Reset( 0,0,1,0);
            
        // Insert the Defferred Procedure Callback (DPC) that will choose with AsyncCommand Buffer to use
        m_pd3dDevice->InsertCallback( D3DCALLBACK_IDLE, &DeferredProcedureCallbackFromGpu, g_iACBcallbackToUse );            

        // Insert the AsyncCommandBufferCall that will be filled in the DPC
        m_pd3dDevice->InsertAsyncCommandBufferCall( g_pAsyncCommandBufferCalls[g_iACBcallbackToUse],     D3DPRED_ALL_RENDER, 0);                
            
        // XOR the next to use to switch between callback 0 and 1.
        g_iACBcallbackToUse = 0x1 ^ g_iACBcallbackToUse;

        PIXEndNamedEvent();      

        // Draw the right cursor and hovering progress.  This is how it traditionally work
        DrawRightCursorAndHoveringProgress();
    }
    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: DrawRightCursorAndHoveringProgress()
// Desc: Render the right cursor and screen's hover circles if they hover.  This is
//      how MenuUsingHover works that can introdcue latency in the system, included here
//      so that the difference in latency can be seen. 
//--------------------------------------------------------------------------------------
void Sample::DrawRightCursorAndHoveringProgress()
{
        D3DRECT rect;

        // Get the current cursor position
        EnterCriticalSection( &g_CritNuiHandles);
        g_fLeftHandCursor = GetXYForVector( NuiHandlesArmGetScreenSpaceLocation( &g_HandlesArms, NUI_HANDLES_ARMS_HANDEDNESS_LEFT_ARM ) );
        g_fRightHandCursor = GetXYForVector( NuiHandlesArmGetScreenSpaceLocation( &g_HandlesArms, NUI_HANDLES_ARMS_HANDEDNESS_RIGHT_ARM ) );
        LeaveCriticalSection( &g_CritNuiHandles);

        rect.x1 = (LONG)g_fLeftHandCursor.x - m_CursorDims.Width / 2;
        rect.x2 = (LONG)g_fLeftHandCursor.x + m_CursorDims.Width / 2;          
        rect.y1 = (LONG)g_fLeftHandCursor.y - m_CursorDims.Height / 2;
        rect.y2 = (LONG)g_fLeftHandCursor.y + m_CursorDims.Height / 2; 

        if ( g_fTimeLeftHovering > 0 )
        {
            rect.x2 = rect.x2 - (LONG)( (FLOAT)m_CursorDims.Width / 4.0f + g_fTimeLeftHovering * 100.0f);
            rect.x1 = rect.x1 + (LONG)( (FLOAT)m_CursorDims.Width / 4.0f + g_fTimeLeftHovering * 100.0f);
            rect.y1 = rect.y1 + (LONG)( (FLOAT)m_CursorDims.Width / 4.0f + g_fTimeLeftHovering * 100.0f);
            rect.y2 = rect.y2 - (LONG)( (FLOAT)m_CursorDims.Width / 4.0f + g_fTimeLeftHovering * 100.0f);
            PIXBeginNamedEvent(0,"Left Cursor Hovering");
            DrawImageColored( rect, 1.0f, g_CursorOnRailColor[0], m_pCursorTexture );
            PIXEndNamedEvent();
        }
        rect.x1 = (LONG)g_fRightHandCursor.x - m_CursorDims.Width / 2;
        rect.x2 = (LONG)g_fRightHandCursor.x + m_CursorDims.Width / 2;          
        rect.y1 = (LONG)g_fRightHandCursor.y - m_CursorDims.Height / 2;
        rect.y2 = (LONG)g_fRightHandCursor.y + m_CursorDims.Height / 2; 

        PIXBeginNamedEvent(0,"Right Cursor Draw");
        DrawImage( rect, 0.8f, g_CursorOnRailColor[1], m_pCursorTexture );
        PIXEndNamedEvent();

        if ( g_fTimeRightHovering > 0 )
        {
            rect.x2 = rect.x2 - (LONG)( (FLOAT)m_CursorDims.Width / 4.0f + g_fTimeRightHovering * 100.0f);
            rect.x1 = rect.x1 + (LONG)( (FLOAT)m_CursorDims.Width / 4.0f + g_fTimeRightHovering * 100.0f);
            rect.y1 = rect.y1 + (LONG)( (FLOAT)m_CursorDims.Width / 4.0f + g_fTimeRightHovering * 100.0f);
            rect.y2 = rect.y2 - (LONG)( (FLOAT)m_CursorDims.Width / 4.0f + g_fTimeRightHovering * 100.0f);
            PIXBeginNamedEvent(0,"Left Cursor Hovering");
            DrawImageColored( rect, 1.0f, g_CursorOnRailColor[1], m_pCursorTexture );
            PIXEndNamedEvent();
        }
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering. This 
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background with texture on top
    ATG::RenderBackground( g_GlobalBackgroundColor1, g_GlobalBackgroundColor2 );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();

    DrawAndUpdateGameState();

    m_CameraManager.DisplayPIP();

    // Render the HUD
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 500.0f, 0.0f, 0xffffffff, L"Asynchronous Cursors", ATGFONT_CENTER_X );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 500.0f, 25.0f, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_CENTER_X );
    m_Font.SetScaleFactors( 0.8f, 0.8f );
    m_Font.DrawText( 0.0f, 10.0f, 0xffffffff,  L"Left cursor:   Asynchronous", ATGFONT_LEFT );
    m_Font.DrawText( 0.0f, 30.0f, 0xffffffff, L"Right cursor:   Synchronous", ATGFONT_LEFT );
    m_Font.DrawText( 0.0f, 50.0f, 0xffffffff, L"Hover progress: Synchronous", ATGFONT_LEFT );

    // Render Smoothing and Tilt Correction options
    m_Font.End();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::Sample
// Desc: Constructor for Sample object.
//----------------------------------------------------------------------------------------------------------------------
Sample::Sample()
{}

//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::~Sample
// Desc: Destroys the sample
//----------------------------------------------------------------------------------------------------------------------
Sample::~Sample()
{}