//--------------------------------------------------------------------------------------
// XEXResource.cpp
//
// The sample demonstrates how to embedd and retreive resources in the XEX executable.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <d3d9.h>
#include <Xgraphics.h>
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgUtil.h"
#include "AtgSimpleShaders.h"
#include "AtgDebugDraw.h"

//--------------------------------------------------------------------------------------
// Number of images in the resource section.
//--------------------------------------------------------------------------------------
const DWORD         g_dwNumImages = 4;

//--------------------------------------------------------------------------------------
// Color values
//--------------------------------------------------------------------------------------
const DWORD         MSG_COLOR = 0xffffffff;          // message color
const DWORD         INFO_COLOR = 0xffffff00;          // information display color

const DWORD         TOP_BACK_COLOR = 0xff0000ff;          // background gradient colors
const DWORD         BOTTOM_BACK_COLOR = 0xff000000;

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_1, L"Next image" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_1, L"Previous image" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    LPDIRECT3DTEXTURE9 m_pCurrentImage;
    DWORD m_dwCurrentImageIndex;

    HRESULT         ReadResource();

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
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
// Name: Initialize
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;
    m_dwCurrentImageIndex = 0;
    m_pCurrentImage = NULL;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Initialize the simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    ReadResource();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Display next image
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_dwCurrentImageIndex = ( ++m_dwCurrentImageIndex ) % g_dwNumImages;
        ReadResource();
    }

    // Display previous image
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_dwCurrentImageIndex = ( --m_dwCurrentImageIndex ) % g_dwNumImages;
        ReadResource();
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( TOP_BACK_COLOR, BOTTOM_BACK_COLOR );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, MSG_COLOR, L"XEXResource" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, INFO_COLOR, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        WCHAR strResource[256];
        swprintf_s( strResource, L"Resource: IMG_%03d", m_dwCurrentImageIndex );
        m_Font.DrawText( 0, 40, INFO_COLOR, strResource );
        m_Font.End();

        // Draw resource image
        if( m_pCurrentImage )
        {
            D3DRECT rect;
            rect.x1 = ATG::GetTitleSafeArea().x1;
            rect.y1 = ATG::GetTitleSafeArea().y1 + 100;
            rect.x2 = ATG::GetTitleSafeArea().x2;
            rect.y2 = ATG::GetTitleSafeArea().y2;
            ATG::DebugDraw::DrawScreenSpaceTexturedRect( rect, m_pCurrentImage );
        }
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: ReadResource
// Desc: Reads a resource from the XEX file.
//--------------------------------------------------------------------------------------
HRESULT Sample::ReadResource()
{
    CHAR strSectionName[32];
    sprintf_s( strSectionName, "IMG_%03d", m_dwCurrentImageIndex );

    VOID* pSectionData;
    DWORD dwSectionSize;

    // Load binary section data
    HMODULE hModule = GetModuleHandle( NULL );
    assert( hModule != NULL );

    if( !XGetModuleSection( hModule , strSectionName, &pSectionData, &dwSectionSize ) )
        return E_FAIL;

    LPDIRECT3DTEXTURE9 pTexture;
    if( FAILED( D3DXCreateTextureFromFileInMemory( m_pd3dDevice, pSectionData, dwSectionSize, &pTexture ) ) )
        return E_FAIL;

    // Release current texture
    if( m_pCurrentImage )
    {
        m_pd3dDevice->SetTexture( 0, NULL );
        m_pCurrentImage->Release();
    }

    m_pCurrentImage = pTexture;

    return S_OK;
}
