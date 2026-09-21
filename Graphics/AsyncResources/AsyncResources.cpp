//--------------------------------------------------------------------------------------
// AsyncResources.cpp
//
// Demonstrates the Direct3D APIs for asynchronously locking and unlocking resources.
//
// This sample is based on the CustomVFetch sample; it is instructive to compare the two
// samples to see how the execution flow changes when using asynchronous resource
// update techniques.
//
// XNA Developer Connection Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>

#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>

#include "ParticleSystem.h"

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_2, L"Spawn more\nparticles" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle async\nupdate" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle buffering\ndepth" },
    { ATG::HELP_START_BUTTON,   ATG::HELP_PLACEMENT_1, L"Freeze particles" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_2, L"Rotate camera" },
    { ATG::HELP_MISC_CALLOUT,   ATG::HELP_PLACEMENT_2, L"Triggers move camera in/out" },
};
static const DWORD  NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );

// Maximum particle count.
static const DWORD  g_dwMaxParticleCount = 30000;

// "Spawn extra" particle count.
static const FLOAT  g_fSpawnExtraParticleCount = 3000.0f;


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
protected:
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    FLOAT m_fViewDistance;

    XMMATRIX m_matView;
    XMMATRIX m_matProj;
    XMMATRIX m_matWorld;

    static const DWORD m_dwParticleSystemCount = 4;
    ParticleSystem  m_ParticleSystem[m_dwParticleSystemCount];
    BOOL m_bFreezeParticles;

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

    // This sample runs exclusively at 720p.  The video scaler will handle all other
    // output resolutions.
    atgApp.m_d3dpp.BackBufferWidth = 1280;
    atgApp.m_d3dpp.BackBufferHeight = 720;

    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all graphics resources and initializes the particle systems.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;
    m_bFreezeParticles = FALSE;

    // Create the font.
    if( FAILED( m_Font.Create( "d:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area.
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help.
    if( FAILED( m_Help.Create( "d:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Build the projection and world matrices.
    m_matProj = XMMatrixPerspectiveFovLH( XM_PIDIV4, ( 16.0f / 9.0f ), 1.0f, 200.0f );
    m_matWorld = XMMatrixIdentity();
    m_fViewDistance = 30.0f;

    // Initialize the simple shaders library.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Load the particle texture.
    D3DTexture* pTexture = NULL;
    HRESULT hr = D3DXCreateTextureFromFile( m_pd3dDevice, "game:\\media\\textures\\CustomVFetch-particles.dds",
                                            &pTexture );
    if( FAILED( hr ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Force the texture format to an AS_16 sRGB format.
    // Do this so there's no loss of precision when sampling the texture in the shader. If using
    // a standard SRGB format, the sRGB->Linear conversion happens with 8 bit precision, resulting 
    // in a loss of data. Using an AS_16 sRGB format causes the conversion to happen at 16-bit precision,
    // meaning no precision is lost.
    ATG::ConvertTextureToAs16SRGBFormat( pTexture );

    // Seed the CRT random number generator.
    srand( __mftb32() );

    // Initialize the particle system.
    for( DWORD i = 0; i < m_dwParticleSystemCount; ++i )
    {
        DWORD dwHardwareThread = 2 + i;
        m_ParticleSystem[i].Initialize( g_dwMaxParticleCount, pTexture, dwHardwareThread );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current time.
    FLOAT fDeltaTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad state.
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Compute camera rotation.
    static FLOAT s_fRotateY = 0.0f;
    static FLOAT s_fRotateX = 0.5f;
    s_fRotateY += pGamepad->fX1 * fDeltaTime * XM_PIDIV2;
    s_fRotateX += pGamepad->fY1 * fDeltaTime * XM_PIDIV2;
    s_fRotateX = min( max( s_fRotateX, -XM_PIDIV2 ), XM_PIDIV2 );
    XMVECTOR qRotateY = XMQuaternionRotationAxis( XMVectorSet( 0, 1, 0, 0 ), s_fRotateY );
    XMVECTOR qRotateX = XMQuaternionRotationAxis( XMVectorSet( 1, 0, 0, 0 ), s_fRotateX );
    XMVECTOR qRotation = XMQuaternionMultiply( qRotateX, qRotateY );

    // Compute camera distance.
    m_fViewDistance += 8.0f * fDeltaTime * ( ( FLOAT )pGamepad->bLeftTrigger / 255.0f );
    m_fViewDistance -= 8.0f * fDeltaTime * ( ( FLOAT )pGamepad->bRightTrigger / 255.0f );
    if( m_fViewDistance < 2.0f )
        m_fViewDistance = 2.0f;

    // Compose view matrix from camera settings.
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -m_fViewDistance, 0.0f );
    vEyePt = XMVector3Rotate( vEyePt, qRotation );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 2.0f, 0.0f, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    vUpVec = XMVector3Rotate( vUpVec, qRotation );
    vEyePt += vLookatPt;
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );

    // Set the world view projection matrix into the debug draw system.
    XMMATRIX matWVP = m_matWorld * m_matView * m_matProj;
    ATG::DebugDraw::SetViewProjection( matWVP );

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // The Start button freezes the particle systems.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
        m_bFreezeParticles = !m_bFreezeParticles;

    // The A button spawns a bunch of extra particles.
    if( !m_bFreezeParticles && pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        for( DWORD i = 0; i < m_dwParticleSystemCount; ++i )
            m_ParticleSystem[i].SpawnExtra( g_fSpawnExtraParticleCount / m_dwParticleSystemCount );
    }

    // The B button toggles synchronous and asynchronous particle updates.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        for( DWORD i = 0; i < m_dwParticleSystemCount; ++i )
            m_ParticleSystem[i].SetAsynchronous( !m_ParticleSystem[i].IsAsynchronous() );
        // We're changing the timing of buffer updates, so we'll need to clear out the
        // GPU before we start up the new scheme.
        m_pd3dDevice->BlockUntilIdle();
    }

    // The X button toggles the depth of multiple buffering (single vs double buffered).
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        for( DWORD i = 0; i < m_dwParticleSystemCount; ++i )
        {
            DWORD dwBufferCount = m_ParticleSystem[i].GetBufferCount() - 1;
            dwBufferCount = ( dwBufferCount + 1 ) % m_ParticleSystem[i].GetMaxBufferCount();
            m_ParticleSystem[i].SetBufferCount( dwBufferCount + 1 );
        }
        // We're changing the timing of buffer updates, so we'll need to clear out the
        // GPU before we start up the new scheme.
        m_pd3dDevice->BlockUntilIdle();
    }

    // If the particle systems are frozen, we still want to update them, but they will
    // not move, and no new particles are spawned.
    if( m_bFreezeParticles )
        fDeltaTime = 0.0f;

    static FLOAT fEmitterTheta = 0.0f;
    fEmitterTheta += ( fDeltaTime * 0.25f );

    // Update the particle emitter positions and the particle systems.
    for( DWORD i = 0; i < m_dwParticleSystemCount; ++i )
    {
        FLOAT fTheta = XM_2PI * ( ( FLOAT )i / ( FLOAT )m_dwParticleSystemCount ) + fEmitterTheta;
        const FLOAT fRadius = 7.0f;
        XMVECTOR vEmitterPos =
        {
            fRadius * sinf( fTheta * 1.3f ), 0, fRadius * cosf( fTheta * 2.1f ), 0
        };
        m_ParticleSystem[i].SetEmitterPos( vEmitterPos );
        m_ParticleSystem[i].Update( fDeltaTime );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a blue gradient background.
    ATG::RenderBackground( 0xFF000080, 0xFF000020 );

    // Draw gridlines for the ground plane.
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    ATG::DebugDraw::DrawGrid( XMFLOAT3( 10, 0, 0 ), XMFLOAT3( 0, 0, 10 ), XMFLOAT3( 0, 0, 0 ), 20, 20, 0xFF808080 );

    // Draw the particle system.
    for( DWORD i = 0; i < m_dwParticleSystemCount; ++i )
        m_ParticleSystem[i].Render( m_matWorld, m_matView, m_matProj );

    // Show title, frame rate, and help.
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"AsyncResources" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.SetScaleFactors( 0.8f, 0.8f );
        WCHAR strText[100];
        DWORD dwCount = 0;
        for( DWORD i = 0; i < m_dwParticleSystemCount; ++i )
            dwCount += m_ParticleSystem[i].GetActiveCount();
        swprintf_s( strText, L"Particles: %lu", dwCount );
        m_Font.DrawText( 0, 30, 0xff00ffff, strText, ATGFONT_RIGHT );

        m_Font.DrawText( 0, 50, 0xffffff00, m_ParticleSystem[0].IsAsynchronous() ? L"Asynchronous Update" :
                         L"Synchronous Update", ATGFONT_RIGHT );

        swprintf_s( strText, L"Dynamic VB Buffer Count: %lu", m_ParticleSystem[0].GetBufferCount() );
        m_Font.DrawText( 0, 70, 0xffffff00, strText, ATGFONT_RIGHT );

        m_Font.End();
    }

    m_pd3dDevice->UnsetAll();

    // Present the scene.
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
