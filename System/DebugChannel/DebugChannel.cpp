//--------------------------------------------------------------------------------------
// DebugChannel.cpp
//
// Sample to demonstrate how to communicate between the development Xbox and a
// remote debug console running on the dev machine.
//
// This source file runs the frame loop and handles input and rendering.
//
// This app runs on Xbox 360 and needs the DebugConsole sample to be
// running on the dev machine.
//
// See the DebugCmd.cpp file comments for how the API is used.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include "DebugCmd.h"


// Global access to the D3D device
extern LPDIRECT3DDEVICE9    g_pd3dDevice;


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display frame\nrate to console" },
    { ATG::HELP_B_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display texture\nto console" },
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_1, L"Display help" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_1, L"Pause" },
    { ATG::HELP_MISC_CALLOUT, ATG::HELP_PLACEMENT_2,
        L"Note: This sample requires DebugConsole.exe\nto be running on the PC" },
};

#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Globally accessed varaibles
//--------------------------------------------------------------------------------------
ATG::PackedResource         g_xprResource;          // Packed resources for the app
FLOAT                       g_fRadians;             // Radians of rotation

LPDIRECT3DTEXTURE9          g_pTexture;             // Texture for quad
BOOL                        g_bForward;             // TRUE if we should spin CW
FLOAT                       g_fRotationSpeed;       // Rotation speed in radians per second
CHAR g_strTextureName[MAX_PATH]; // Filename of texture
D3DXVECTOR4                 g_vColor( 1.0f, 1.0f, 1.0f, 1.0f ); // Color of quad

// Vertex format for rendering a simple, textured polygon
struct CUSTOMVERTEX
{
    D3DXVECTOR3 p;          // Position
    D3DXVECTOR2 t;          // Texture coordinates
};


//--------------------------------------------------------------------------------------
// Array of structures to expose variables to the debug console's "set" command
//--------------------------------------------------------------------------------------
const REMOTE_VARIABLE g_RemoteVariables[] =
{
    // Var name,  Variable address,   Type,       Handler
    { "bForward", &g_bForward,        SDOS_BOOL,  NULL            },
    { "red",      &g_vColor[0],       SDOS_FLOAT, RCmdLightChange },
    { "green",    &g_vColor[1],       SDOS_FLOAT, RCmdLightChange },
    { "blue",     &g_vColor[2],       SDOS_FLOAT, RCmdLightChange },
};

const DWORD                 g_dwNumRemoteVariables = ( sizeof( g_RemoteVariables ) / sizeof( REMOTE_VARIABLE ) );


//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
// from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:

    ATG::Font m_Font;               // Font object
    ATG::Help m_Help;               // Help object
    BOOL m_bDrawHelp;          // Whether to draw help
    BOOL m_bPaused;            // Whether spinnning is paused
    ATG::Timer m_Timer;              // Timer object

    D3DXMATRIX m_matViewProj;       // View * Projection transform is constant
    D3DXMATRIX m_matWorldViewProj;

    LPDIRECT3DVERTEXBUFFER9 m_pVBQuad;
    LPDIRECT3DVERTEXDECLARATION9 m_pTexturedQuadDecl;
    LPDIRECT3DVERTEXSHADER9 m_pPositionTexcoordVS;
    LPDIRECT3DPIXELSHADER9 m_pTextureModConstantPS;
    LPDIRECT3DPIXELSHADER9 m_pConstantColorPS;

public:

    HRESULT Initialize();
    HRESULT Render();
    HRESULT Update();

            Sample();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Sample
//--------------------------------------------------------------------------------------
Sample::Sample() : ATG::Application()
{
    m_bDrawHelp = FALSE;
    m_bPaused = FALSE;

    g_bForward = TRUE;
    g_pTexture = NULL;
    g_strTextureName[0] = 0;
    g_fRadians = 0.0f;
    g_fRotationSpeed = 0.0f;
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Performs whatever initialization is necessary
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Create the resources
    if( FAILED( g_xprResource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create a font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG::DebugSpew( "Couldn't create font\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Initialize the help system
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Set up our view & projection matrices
    D3DXMATRIX matView;
    D3DXMatrixTranslation( &matView, 0.0f, 0.0f, 1.0f );

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    D3DXMATRIX matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI / 3, fAspectRatio, 1.0f, 10.0f );
    D3DXMatrixMultiply( &m_matViewProj, &matView, &matProj );

    static const D3DVERTEXELEMENT9 elems[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END(),
    };
    if( FAILED( m_pd3dDevice->CreateVertexDeclaration( elems, &m_pTexturedQuadDecl ) ) )
        ATG::FatalError( "Couldn't create vertex declaration" );

    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\PositionTexcoord.xvu", &m_pPositionTexcoordVS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\TextureModConstant.xpu", &m_pTextureModConstantPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ConstantColor.xpu", &m_pConstantColorPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( CUSTOMVERTEX ),
                                      D3DUSAGE_WRITEONLY,
                                      0,
                                      D3DPOOL_DEFAULT,
                                      &m_pVBQuad,
                                      NULL );
    CUSTOMVERTEX* pVerts;
    m_pVBQuad->Lock( 0, 0, ( void** )&pVerts, 0 );
    pVerts[0].p = D3DXVECTOR3( -0.5f, -0.5f, 1.0f );
    pVerts[0].t = D3DXVECTOR2( 0.0f, 1.0f );
    pVerts[1].p = D3DXVECTOR3( -0.5f, 0.5f, 1.0f );
    pVerts[1].t = D3DXVECTOR2( 0.0f, 0.0f );
    pVerts[2].p = D3DXVECTOR3( 0.5f, -0.5f, 1.0f );
    pVerts[2].t = D3DXVECTOR2( 1.0f, 1.0f );
    pVerts[3].p = D3DXVECTOR3( 0.5f, 0.5f, 1.0f );
    pVerts[3].t = D3DXVECTOR2( 1.0f, 0.0f );
    m_pVBQuad->Unlock();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Performs all per-frame calculations to update the application state
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();
    FLOAT fTime = ( FLOAT )m_Timer.GetElapsedTime();

    if( !m_bPaused )
    {
        if( g_bForward )
            g_fRadians = fmodf( g_fRadians - g_fRotationSpeed * fTime, 2 * D3DX_PI );
        else
            g_fRadians = fmodf( g_fRadians + g_fRotationSpeed * fTime, 2 * D3DX_PI );
    }

    D3DXMATRIX matWorld;
    D3DXMatrixRotationZ( &matWorld, g_fRadians );

    D3DXMatrixMultiply( &m_matWorldViewProj, &matWorld, &m_matViewProj );
    D3DXMatrixTranspose( &m_matWorldViewProj, &m_matWorldViewProj );

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Toggle pause
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        m_bPaused = !m_bPaused;
    }

    // Handle requests for debug output
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        DebugConsolePrintf( "Framerate is %S\n", m_Timer.GetFrameRate() );
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        if( g_strTextureName && g_strTextureName[0] )
            DebugConsolePrintf( "Current texture is %s\n", g_strTextureName );
        else
            DebugConsolePrintf( "No texture is currently set\n" );
    }

    // Process any pending commands from the remote debug console
    DebugConsoleHandleCommands();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Called once per frame, the call is the entry point for 3d
//       rendering. This function sets up render states, clears the
//       viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                         0x00000080, 1.0f, 0L );

    // Set rendering state
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetVertexDeclaration( m_pTexturedQuadDecl );
    m_pd3dDevice->SetStreamSource( 0, m_pVBQuad, 0, sizeof( CUSTOMVERTEX ) );
    m_pd3dDevice->SetVertexShader( m_pPositionTexcoordVS );
    m_pd3dDevice->SetVertexShaderConstantF( 0, m_matWorldViewProj, 4 );

    // Set up the appropriate pixel shader and constants
    if( g_pTexture )
    {
        m_pd3dDevice->SetTexture( 0, g_pTexture );
        m_pd3dDevice->SetPixelShader( m_pTextureModConstantPS );
    }
    else
    {
        m_pd3dDevice->SetTexture( 0, NULL );
        m_pd3dDevice->SetPixelShader( m_pConstantColorPS );
    }
    m_pd3dDevice->SetPixelShaderConstantF( 0, g_vColor, 1 );

    // Draw the quad
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

    m_Timer.MarkFrame();

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"DebugChannel" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        if( IsBinaryTransferInProgress() )
        {
            m_Font.SetScaleFactors( 0.8f, 0.8f );
            m_Font.DrawText( 0, -20, 0xff00ffff, L"Transmitting binary data..." );
        }
        else if( GetBinaryDataSize() > 0 && GetBinaryData() != NULL )
        {
            BYTE* pData = GetBinaryData();
            m_Font.SetScaleFactors( 0.8f, 0.8f );
            WCHAR strText[100];
            swprintf_s( strText, L"%d bytes of binary data received, first byte is 0x%x '%c'",
                        GetBinaryDataSize(), *pData, *pData );
            m_Font.DrawText( 0, -20, 0xff00ffff, strText );
        }
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RCmdTexture
// Desc: Callback handler for the remote "texture" command. Sets the new
//       texture to be used
//--------------------------------------------------------------------------------------
VOID RCmdTexture( int argc, char* argv[] )
{
    // Check our arguments
    if( argc < 2 )
    {
        DebugConsolePrintf( "ERROR: Need to specify a texture resource name.\n" );
        DebugConsolePrintf( "Available textures are:\n" );

        DWORD dwNumResourceTags;
        ATG::RESOURCE* pResourceTags;
        g_xprResource.GetResourceTags( &dwNumResourceTags, &pResourceTags );

        for( DWORD i = 0; i < dwNumResourceTags; ++i )
            DebugConsolePrintf( "   %s\n", pResourceTags[i].strName );

        return;
    }

    // Try to grab the new texture
    assert( argv != NULL );
    strcpy_s( g_strTextureName, argv[1] );

    g_pTexture = g_xprResource.GetTexture( g_strTextureName );

    if( NULL == g_pTexture )
    {
        DebugConsolePrintf( "ERROR: Couldn't find %s.", g_strTextureName );
        return;
    }
}


//--------------------------------------------------------------------------------------
// Name: RCmdSpin
// Desc: Callback handler for the remote "spin" command. Sets the new spin
//       velocity, limited to 2pi in either direction
//--------------------------------------------------------------------------------------
VOID RCmdSpin( int argc, char* argv[] )
{
    // Check our arguments
    if( argc < 2 )
    {
        DebugConsolePrintf( "ERROR: Need to specify a velocity.\n" );
        return;
    }

    assert( argv != NULL );
    FLOAT fVelocity = ( FLOAT )atof( argv[1] );

    if( fabs( fVelocity ) > D3DX_PI * 2 )
    {
        DebugConsolePrintf( "ERROR: Velocity should be between +/- 2 * pi.\n" );
        return;
    }

    // Set our state
    g_fRotationSpeed = fabsf( fVelocity );
    g_bForward = ( fVelocity > 0 );
}


//--------------------------------------------------------------------------------------
// Name: RCmdLightChange
// Desc: Called after changing one of our lighting values, so that we can
//       reset the light
//--------------------------------------------------------------------------------------
VOID RCmdLightChange( void* pAddr )
{
}
