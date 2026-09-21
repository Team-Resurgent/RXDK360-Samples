//--------------------------------------------------------------------------------------
// SocialPost.cpp
//
// Sample to demonstrate posting an update to a Social Network.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include <xtl.h>
#include <xbdm.h>
#include <malloc.h>
#include <xonline.h>
#include <xsocialpost.h>

#include <cstdio>
#include <cassert>

#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgUtil.h"
#include "AtgInput.h"
#include "AtgDevice.h"
#include "AtgSignIn.h"
#include "AtgResource.h"

#include "SocialPost.spa.h"

#pragma warning(disable:4127)   // we use some infinite loops, disable
                                // "conditional expression constant" warning

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

// Callouts for labelling the gamepad on the help screen
static ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display Help" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_2, L"Post a Link" },
    { ATG::HELP_X_BUTTON, ATG::HELP_PLACEMENT_2, L"Post an Image" },
    { ATG::HELP_Y_BUTTON, ATG::HELP_PLACEMENT_2, L"Change User" },
    { ATG::HELP_DPAD, ATG::HELP_PLACEMENT_2, L"Change Text and\nImage Selection" },
};
static const DWORD NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );

// Very simple shader code for textured 2D polygons used in the UI.
static const CHAR* g_strShader =
    " struct VS_IN                                "
    " {                                           "
    "     float2 Pos            : POSITION;       "
    "     float2 Tex            : TEXCOORD0;      "
    " };                                          "
    "                                             "
    " struct VS_OUT                               "
    " {                                           "
    "     float4 Position       : POSITION;       "
    "     float2 TexCoord0      : TEXCOORD0;      "
    " };                                          "
    "                                             "
    " VS_OUT VertShader( VS_IN In )               "
    " {                                           "
    "     VS_OUT Out;                             "
    "     Out.Position.x  = In.Pos.x;             "
    "     Out.Position.y  = In.Pos.y;             "
    "     Out.Position.z  = 0.0;                  "
    "     Out.Position.w  = 1.0;                  "
    "     Out.TexCoord0.x = In.Tex.x;             "
    "     Out.TexCoord0.y = In.Tex.y;             "
    "     return Out;                             "
    " }                                           "
    "                                             "
    "sampler Texture : register(s0);              "
    "                                             "
    "float4 PixShader( VS_OUT In ) : COLOR0       "
    "{                                            "
    "    return tex2D( Texture, In.TexCoord0 );   "
    "}                                            "
    "                                             ";

// Vertex format declaration for the 2D polygons used in the UI.
static const D3DVERTEXELEMENT9 g_VertexDecl[] =
{
    { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};

// Names of each of the bitmap resources, so we can retrieve them.
const CHAR* g_TextureName[] =
{
    "Triangle",
    "Circle",
    "Arrow",
    "Star",
};

const UINT NUM_TEXTURES = ARRAY_SIZE(g_TextureName);

// Messages that will be used in the posts and the UI.
const WCHAR* g_TitleText[] =
{
    L"Super Fun Game",
    L"Run, Jump & Shoot",
    L"I Love Pizza!",
    L"Macroeconomic Simulator",
};

const WCHAR* g_PictureCaption[] =
{
    L"Really Big Achievement",
    L"Not bad, Soldier.",
    L"Good gravy, that's amazing!",
    L"You have reached Supply Equilibrium",
};

const WCHAR* g_PictureDescription[] =
{
    L"You totally achieved that thing. There seems to be no stopping you now.",
    L"You walk into a game and things just explode. Kaboom! Game Over.",
    L"In the world of fast food service, you have more than five pieces of flair.",
    L"You are moderately successful, and have long term plans that are realistic.",
};


// For each editable items in the UI, define a named index for it. MAkes th
// code a little easier to read.
enum UI_ITEM
{
    TITLE_TEXT,
    PICTURE_CAPTION,
    PICTURE_DESCRIPTION,
    PREVIEW_IMAGE,
};

// Positions of the editable items on the display in grid coordinates.
// The UI items are laid out on a course grid that is scaled to fit the
// visible area.
struct UiItemPos
{
    FLOAT x;
    FLOAT y;
};

const UiItemPos g_UiItemPosition[] =
{
    { 6,3 },  // TitleText
    { 6,4 },  // PictureCaption
    { 6,5 },  // PictureDescrition
    { 2,3 },  // PreviewImage
};

const UINT NUM_UI_ITEMS = ARRAY_SIZE( g_UiItemPosition );


// URL to a picture hosted on an external server, used in Link posts as the
// target link.
//
// NOTE: this URL may become invalid in the distant future.
//
const LPCWSTR swExternalPictureURL = L"http://www.bing.com/fd/hpk2/HoustonTX_EN-US1003762935.jpg";


//-----------------------------------------------------------------------------
// Classes
//-----------------------------------------------------------------------------

class SocialPostSample : public ATG::Application
{
private:
    ATG::Font     m_Font;
    ATG::Help     m_Help;
    ATG::PackedResource m_Resource;
    XOVERLAPPED   m_Overlapped;

    // Resources for rendering the bitmaps
    D3DVertexDeclaration* m_pVertexDecl;
    D3DVertexShader* m_pVertexShader;
    D3DPixelShader* m_pPixelShader;
    D3DTexture* m_pTextureArray[ NUM_TEXTURES ];

    // Copy of the XSocial capability flags
    DWORD m_dwSocialCaps;

    // State of selected items
    BOOL m_bHaveCaps;
    BOOL m_bDrawHelp;
    BOOL m_bMessageSent;

    UINT m_nUiActiveItem;            // Which UI item is currently being edited.
    UINT m_nUiItem[ NUM_UI_ITEMS ];  // Current value of each UI item.

    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


// ----------------------------------------------------------------------------

HRESULT SocialPostSample::Initialize()
{
    // Start up Xbox LIVE.
    if ( FAILED( XOnlineStartup() ) )
    {
        ATG::FatalError( "Failed to start Xbox LIVE.\n" );
    }

    // Initialize Xbox LIVE login.
    ATG::SignIn::Initialize( 1,     // minUsers
                             1,     // maxUsers
                             TRUE,  // require online users
                             1 );   // number of sign in panes

    // Create the display font.
    if ( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_12.xpr" ) ) )
    {
        return ATGAPPERR_MEDIANOTFOUND;
    }

    // Confine text drawing to the safe area for this screen resolution.
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help resources.
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        return ATGAPPERR_MEDIANOTFOUND;
    }

    // Create the Bitmap resources.
    if( FAILED( m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        return ATGAPPERR_MEDIANOTFOUND;
    }
    // Set up the texture array by looping over the list of bitmap resource
    // names and requesting each in turn.
    for ( SIZE_T i = 0; i < NUM_TEXTURES; ++i )
    {
        m_pTextureArray[i] = m_Resource.GetTexture( g_TextureName[i] );
    }

    // Create the vertex format decl for the 2D polygons used in the UI.
    if( FAILED( m_pd3dDevice->CreateVertexDeclaration( g_VertexDecl, &m_pVertexDecl ) ) )
    {
        ATG_PrintError("Failed to create Vertex Declaration.");
        return E_FAIL;
    }

    // Allocate a reusable compile buffer.
    ID3DXBuffer* pBuffer;

    // Create a Vertex Shader from the local source code string.
    if( FAILED( D3DXCompileShader( g_strShader,            // shader string
                                   strlen( g_strShader ),  // size of shader string
                                   NULL,                   // defines
                                   NULL,                   // includes
                                   "VertShader",           // entry point
                                   "vs.2.0",               // target profile
                                   0,                      // flags
                                   &pBuffer,               // **shader data
                                   NULL,                   // **errormessages
                                   NULL ) ) )              // constant table
    {
        ATG_PrintError("Failed to create Vertex Shader.");
        return E_FAIL;
    }

    if( FAILED( m_pd3dDevice->CreateVertexShader( (DWORD*)pBuffer->GetBufferPointer(),
        &m_pVertexShader ) ) )
    {
        return E_FAIL;
    }

    pBuffer->Release();

    // Create a Pixel Shader from the local source code string.
    if( FAILED( D3DXCompileShader( g_strShader,
                                   strlen( g_strShader ),
                                   NULL,
                                   NULL,
                                   "PixShader",
                                   "ps.2.0",
                                   0,
                                   &pBuffer,
                                   NULL,
                                   NULL ) ) )
    {
        ATG_PrintError("Failed to create Pixel Shader.");
        return E_FAIL;
    }

    if( FAILED( m_pd3dDevice->CreatePixelShader( (DWORD*)pBuffer->GetBufferPointer(),
        &m_pPixelShader ) ) )
    {
        return E_FAIL;
    }

    pBuffer->Release();

    // Clear the remaining class members.
    XMemSet( &m_Overlapped, 0, sizeof(XOVERLAPPED) );

    // Set up the initial display state.
    m_dwSocialCaps = 0;
    m_bHaveCaps = false;
    m_bDrawHelp = false;
    m_bMessageSent = false;

    // Set up the initial state of each UI item.
    m_nUiActiveItem = TITLE_TEXT;
    m_nUiItem[TITLE_TEXT] = 0;
    m_nUiItem[PICTURE_CAPTION] = 0;
    m_nUiItem[PICTURE_DESCRIPTION] = 0;
    m_nUiItem[PREVIEW_IMAGE] = 0;

    return S_OK;
}


HRESULT SocialPostSample::Update()
{
    // Run the per-frame update for the SignIn object.
    ATG::SignIn::Update();

    // Get keypad inputs, and detect the reboot keypress
    ATG::GAMEPAD *pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Check to see if user has completed signing in
    if( !ATG::SignIn::IsSystemUIShowing() && ATG::SignIn::IsUserSignedIn( 0 ) )
    {
        // If we don't have the XSocialPost capabilities yet then retrieve
        // them (first time through only).
      if ( FALSE == m_bHaveCaps )
        {
            // Get the XSocial capabilities only once
            DWORD result = XSocialGetCapabilities( &m_dwSocialCaps, &m_Overlapped );
            if ( ERROR_FUNCTION_FAILED == result )
            {
                ATG::FatalError( "XSocialGetCapabilities failed, error: %d\n", result );
            }

            // Wait for the result synchronously in this case. The return
            // code is different than the result code.
            if ( FAILED( XGetOverlappedResult( &m_Overlapped, &result, TRUE ) ) )
            {
                // XGetOverlappedResult doesn't use SetLastError.
                ATG::FatalError( "XGetOverlappedResult failed getting XSocialCaps\n" );
            }

            // Check the return code from the Overlapped operation. We asked
            // for the operation to wait until the result was available so
            // ERROR_IO_PENDING will not occur.
            if ( ERROR_FUNCTION_FAILED == result )
            {
                DWORD error = XGetOverlappedExtendedError(&m_Overlapped);
                ATG::FatalError( "XSocialGetCapabilities failed, error: %d\n", error );
            }
            else
            {
                // Success, m_dwSocialCaps is set and we're OK to display
                // capabilities.
                m_bHaveCaps = true;
            }

        }

        if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        {
            // increment edit item with wraparound.
            m_nUiActiveItem = (m_nUiActiveItem + 1) % NUM_UI_ITEMS;
        }

        if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        {
            // decrement edit item with wraparound.
            m_nUiActiveItem = (m_nUiActiveItem - 1) % NUM_UI_ITEMS;
        }

        if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        {
            // increment the value of the editable item with wraparound
            m_nUiItem[m_nUiActiveItem] = (m_nUiItem[m_nUiActiveItem] + 1) % NUM_UI_ITEMS;
        }

        if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        {
            // decrement the value of the editable item
            m_nUiItem[m_nUiActiveItem] = (m_nUiItem[m_nUiActiveItem] - 1) % NUM_UI_ITEMS;
        }

        if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        {
            // Allow the user to signin as someone else.
            ATG::SignIn::ShowSignInUI();
        }

        if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        {
            // Button X was pressed, execute the Image Post UI.
        }

        if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            // Button A was pressed, execute the Link Post UI.

            // Get get the dimensions of the texture.
            D3DSURFACE_DESC surfaceDesc;
            m_pTextureArray[m_nUiItem[PREVIEW_IMAGE]]->GetLevelDesc(0, &surfaceDesc);

            // Get a pointer to the texture data itself.
            D3DLOCKED_RECT lockedRect;
            m_pTextureArray[m_nUiItem[PREVIEW_IMAGE]]->LockRect( 0,                  // level
                                                             &lockedRect,        // description
                                                             NULL,               // rectangle
                                                             D3DLOCK_READONLY ); // flags

            // Fill the structures for an XSocialNetworkLinkPost (images are remotely stored).
            XSOCIAL_LINKPOSTPARAMS params;
            params.Size = sizeof ( XSOCIAL_LINKPOSTPARAMS );
            params.TitleText = g_TitleText[m_nUiItem[TITLE_TEXT]];
            params.TitleURL = L"http://www.bing.com/";
            params.PictureCaption = g_PictureCaption[m_nUiItem[PICTURE_CAPTION]];
            params.PictureDescription = g_PictureDescription[m_nUiItem[PICTURE_DESCRIPTION]];
            params.PictureURL = swExternalPictureURL;
            params.PreviewImage.Format = surfaceDesc.Format;
            params.PreviewImage.Height = surfaceDesc.Height;
            params.PreviewImage.Width = surfaceDesc.Width;
            params.PreviewImage.Pitch = lockedRect.Pitch;
            params.PreviewImage.pBytes = reinterpret_cast<BYTE*>(lockedRect.pBits);
            params.Flags = XSOCIAL_POST_GAMECONTENT | XSOCIAL_POST_ACHIEVEMENTCONTENT;

            // Open the XSocialPost UI for an asynchronous post.
            XShowSocialNetworkLinkPostUI( 0,                // user index
                                          &params,          // struct of parameters
                                          &m_Overlapped );  // asynchronous

            // Finished with the texture data.
            m_pTextureArray[ m_nUiItem[ PREVIEW_IMAGE ] ]->UnlockRect(0);  // unlock level 0

            // wait for the result
            DWORD result;
            XGetOverlappedResult( &m_Overlapped, &result, TRUE );

            // check the return code.
            if (result == ERROR_SUCCESS)
            {
                m_bMessageSent = true;
            }
            else
            {
                m_bMessageSent = false;
            }

        }

    } // AreUsersSignedin()

    return S_OK;
}


// ----------------------------------------------------------------------------
// Name: Render
// Desc: Produce a display from the current object state.
// ----------------------------------------------------------------------------
HRESULT SocialPostSample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    const DWORD TITLE_COLOR = 0xFFFFFF00;
    const DWORD TEXT_COLOR = 0xFFFFFFFF;
    const DWORD GAME_COLOR = 0xFFFF8080;
    const DWORD POST_COLOR = 0xFFA0FFFF;

    // Eith display the Help or the Info display.
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Divide the visible area into a course 16 x 10 grid for placing UI elements on.
        D3DRECT window;
        m_Font.GetWindow(window);
        FLOAT fWidth  = static_cast<FLOAT>(window.x2 - window.x1) - 1.0f;
        FLOAT fHeight = static_cast<FLOAT>(window.y2 - window.y1) - 1.0f;
        FLOAT fGridX = fWidth / 16.0f;
        FLOAT fGridY = fHeight / 10.0f;
        FLOAT fTextHeight = m_Font.GetFontHeight();

        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );

        // Title in the top-left
        m_Font.DrawText( 0.0f * fGridX, 0.0f * fGridY, TITLE_COLOR, L"SocialPost Sample" );

        // Put the user number in the top-right
        WCHAR wchText[1024];  // wchar buffer
        int len = 0;          // number of wchar characters in the buffer.

        // Print in the top-right which user is signed in.
        swprintf_s( wchText, 1024, L"User %d", ATG::SignIn::GetSignedInUser() );

        m_Font.DrawText( 16.0f * fGridX, 0.0f * fGridY, TEXT_COLOR, wchText, ATGFONT_RIGHT );

        // Display the SocialPost capabilities in the top right, under the user.
       if ( 0 == m_dwSocialCaps )
        {
            swprintf_s(wchText, 1024, L"SocialPost is Disabled");
        } else {

            if ( m_dwSocialCaps & XSOCIAL_CAPABILITY_POSTLINK )
            {
                len = swprintf_s(wchText, 1024, L"SocialPost can post a Link\n" );
            }
            if ( m_dwSocialCaps & XSOCIAL_CAPABILITY_POSTIMAGE )
            {
                len += swprintf_s(wchText + len, 1024 - len, L"SocialPost can post an Image" );
            }
        }

        m_Font.DrawText( 16.0f * fGridX, 0.0f * fGridY + fTextHeight, TEXT_COLOR, wchText, ATGFONT_RIGHT );

        // Instructions at the bottom left, start a new string.
        len = swprintf_s(wchText, 1024, L"Press " GLYPH_Y_BUTTON L" to Change User\n");

        if (m_dwSocialCaps & XSOCIAL_CAPABILITY_POSTLINK) {
            // Append instruction when Link Posting is available.
            len += swprintf_s(wchText + len, 1024 - len, L"Press " GLYPH_A_BUTTON L" to Post a Link\n");
        }

        if (m_dwSocialCaps & XSOCIAL_CAPABILITY_POSTIMAGE) {
            // Append instruction when Image Posting is available.
            // len += swprintf_s(wchText + len, 1024 - len, L"Press " GLYPH_X_BUTTON L" to Post an Image");
        }

        m_Font.DrawText( 0.0f * fGridX, 8.0f * fGridY + 1.0f * fTextHeight, TEXT_COLOR, wchText);

        // Draw the UI items:

        // Title Text
        m_Font.DrawText( 6.0f * fGridX, 3.0f * fGridY, GAME_COLOR, g_TitleText[m_nUiItem[TITLE_TEXT]] );

        // Picture Caption
        m_Font.DrawText( 6.0f * fGridX, 4.0f * fGridY, POST_COLOR, g_PictureCaption[m_nUiItem[PICTURE_CAPTION]] );

        // Picture Description
        m_Font.DrawText( 6.0f * fGridX,
                         5.0f * fGridY,
                         POST_COLOR,
                         g_PictureDescription[ m_nUiItem[PICTURE_DESCRIPTION] ] );

        // Draw the selection pointer next to the current active UI item.
        m_Font.DrawText( g_UiItemPosition[m_nUiActiveItem].x * fGridX,
                         g_UiItemPosition[m_nUiActiveItem].y * fGridY,
                         TEXT_COLOR,
                         GLYPH_RIGHT_ARROW,
                         ATGFONT_RIGHT);

        m_Font.End();

        // Draw the bitmap Quad.
        ATG::g_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
        ATG::g_pd3dDevice->SetVertexShader( m_pVertexShader );
        ATG::g_pd3dDevice->SetPixelShader( m_pPixelShader );
        ATG::g_pd3dDevice->SetTexture( 0, m_pTextureArray[m_nUiItem[PREVIEW_IMAGE]] );
        ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
        ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
        ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        ATG::g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
        ATG::g_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
        ATG::g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
        ATG::g_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
        ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        ATG::g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
        ATG::g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
        ATG::g_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
        {
            struct VertexStruct {
              FLOAT  x, y;
              FLOAT  u, v;
            };

            FLOAT fXPos = window.x1 + 2.0f * fGridX;
            FLOAT fYPos = window.y1 + 3.0f * fGridY;
            FLOAT fSize = 3.0f * fGridX;

            VertexStruct Verts[4] = {
                { fXPos +  0.0f, fYPos +  0.0f,  0.0f, 0.0f },
                { fXPos + fSize, fYPos +  0.0f,  1.0f, 0.0f },
                { fXPos + fSize, fYPos + fSize,  1.0f, 1.0f },
                { fXPos +  0.0f, fYPos + fSize,  0.0f, 1.0f },
            };
            ATG::g_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, Verts, sizeof(Verts[0]) );
        }

    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Main loop
//-----------------------------------------------------------------------------
INT __cdecl main()
{
    // Create the application object
    SocialPostSample app;

    // Directly set the D3DPRESENT_PARAMETERS structure inside the app object
    // before the ATG:Application code executes D3D setup.
    ATG::GetVideoSettings( &app.m_d3dpp.BackBufferWidth,
                           &app.m_d3dpp.BackBufferHeight );

    // Setup D3D, run Initialise() then loop on Update() and Render().
    app.Run();
}


// ----------------------------------------------------------------------------
