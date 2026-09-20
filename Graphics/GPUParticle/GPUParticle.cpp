//-----------------------------------------------------------------------------
// GPUParticle.cpp
//
// Sample of GPUParticle system
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <d3dx9.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include "AtgTerrain.h"
#include "GPUParticleSystem.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Change\nemitter" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_1, L"Change map" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_1, L"Reset particles" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//-----------------------------------------------------------------------------
// Globals variables and definitions
//-----------------------------------------------------------------------------
FNHEIGHTFUNCTION    HeightFn;
FNHEIGHTFUNCTION    HeightFn2;
FNHEIGHTFUNCTION    HeightFn3;

#define FOG_COLOR         0x00204080

const DWORD         NUMPARTICLES = 1024 * 64;
const DWORD         PLANEMAPSIZE = 256;
const D3DFORMAT     PLANEMAPFORMAT = D3DFMT_A16B16G16R16F;

EMITTERPARAMS g_EmitterParam[] =
{
    {
        XMFLOAT3( -0.5f, 1.3f, -0.5f ),                                         // Position
        XMFLOAT3( 1.0f, 0.0f, 1.0f ),                                           // Position range
        XMFLOAT4( 1.0f, XM_PI / 2.f - 0.5f, 1.0f, XM_PI / 2.f - 0.5f ),         // Velocity range in XZ plane, offset in YZ, Velocity range in XY plane, offset in XY,
        XMFLOAT4( 4.2f, 1.0f, 8.f, 1.0f ),                                      // Speed range, speed offset, Life range, life offset
    },
    {
        XMFLOAT3( 3.f, 0.2f, 0.f ),                                             // Position
        XMFLOAT3( 0.0f, 0.0f, 0.0f ),                                           // Position range
        XMFLOAT4( 0.5f, XM_PI / 2.f - 0.25f, 0.5f, XM_PI / 2.f - 0.25f ),       // Velocity range in XZ plane, offset in YZ, Velocity range in XY plane, offset in XY,
        XMFLOAT4( 4.0f, .75f, 12.f, 2.0f ),                                     // Speed range, speed offset, Life range, life offset
    },
    {
        XMFLOAT3( -32.f, 8.0f, -32.f ),                                         // Position
        XMFLOAT3( 64.0f, 0.0f, 64.0f ),                                         // Position range
        XMFLOAT4( 1.5f, XM_PI / 2.f, 1.5f, -XM_PI / 2.f ),                     // Velocity range in XZ plane, offset in YZ, Velocity range in XY plane, offset in XY,
        XMFLOAT4( -1.5f, -.30f, 20.f, 10.0f ),                                  // Speed range, speed offset, Life range, life offset
    },
};
DWORD               g_EmitterMode = 0;

FNHEIGHTFUNCTION*   g_MapFunc[] =
{
    HeightFn,
    HeightFn2,
    HeightFn3,
};
DWORD               g_MapMode = 0;


//-----------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//-----------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::PackedResource m_Resource;  // Packed resource for the textures
    ATG::Font m_Font;      // Font for drawing text
    ATG::Timer m_Timer;     // Timer
    ATG::Help m_Help;      // Display help
    BOOL m_bDrawHelp;

    XMMATRIX m_matWorld;  // Transform matrices
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    CGPUParticle m_Particle;
    LPDIRECT3DTEXTURE9 m_pParticleTexture;

    AtgTerrain m_Terrain;
    LPDIRECT3DTEXTURE9 m_pGroundTexture;

    LPDIRECT3DTEXTURE9 m_pPlaneMap;

    HRESULT CreatePlaneMap();
    HRESULT InitializeParticles();

public:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
};


//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects.
//-----------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    m_bDrawHelp = FALSE;

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the transform matrices
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 5.0f, -12.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matWorld = XMMatrixIdentity();
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, 1.0f, 10000.0f );

    // Set default render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

    if( FAILED( D3DXCreateTextureFromFile( m_pd3dDevice, "game:\\Media\\Textures\\Particle.bmp",
                                           &m_pParticleTexture ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( D3DXCreateTextureFromFile( m_pd3dDevice, "game:\\Media\\Textures\\Stonehengeground.bmp",
                                           &m_pGroundTexture ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Force the texture formats to AS_16 sRGB formats.
    // Do this so there's no loss of precision when sampling the textures in the shader. If using
    // a standard SRGB format, the sRGB->Linear conversion happens with 8 bit precision, resulting 
    // in a loss of data. Using AS_16 sRGB formats causes the conversion to happen at 16-bit precision,
    // meaning no precision is lost.
    ATG::ConvertTextureToAs16SRGBFormat( m_pGroundTexture );
    ATG::ConvertTextureToAs16SRGBFormat( m_pParticleTexture );

    // Create the terrain
    m_Terrain.Initialize( m_pd3dDevice );
    m_Terrain.Generate( 48, 48, XMFLOAT2( GPUFLOAT_MIN, GPUFLOAT_MIN ),
                        XMFLOAT2( GPUFLOAT_MAX, GPUFLOAT_MAX ),
                        m_pGroundTexture, HeightFn, 4.0f, 4.0f );

    // Create the plane map
    m_pd3dDevice->CreateTexture( PLANEMAPSIZE, PLANEMAPSIZE, 1, 0, PLANEMAPFORMAT,
                                 D3DPOOL_DEFAULT, &m_pPlaneMap, 0 );
    CreatePlaneMap();

    // Create the particle system
    m_Particle.Initialize( m_pd3dDevice, NUMPARTICLES, m_pPlaneMap, m_pParticleTexture );
    InitializeParticles();
    m_Particle.SetEmitter( &g_EmitterParam[0] );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT m_fTime = ( FLOAT )m_Timer.GetAppTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Re-init particles
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
        InitializeParticles();

    // Change emitter
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        g_EmitterMode = ( g_EmitterMode + 1 ) % ( sizeof( g_EmitterParam ) / sizeof( EMITTERPARAMS ) );
        m_Particle.SetEmitter( &g_EmitterParam[ g_EmitterMode ] );
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        g_MapMode = ( g_MapMode + 1 ) % ( sizeof( g_MapFunc ) / sizeof( g_MapFunc[0] ) );

        // Unset all resources in the D3D device.
        m_pd3dDevice->SetStreamSource( 0, NULL, 0, 0 );
        m_pd3dDevice->SetStreamSource( 1, NULL, 0, 0 );
        m_pd3dDevice->SetIndices( NULL );
        m_pd3dDevice->SetTexture( 0, NULL );
        m_pd3dDevice->SetTexture( 1, NULL );
        m_pd3dDevice->SetTexture( 2, NULL );
        m_pd3dDevice->SetTexture( 3, NULL );

        // Block until the current frame is completely done rendering, since we need
        // to release and recreate all of the resources.
        m_pd3dDevice->BlockUntilIdle();

        // Recreate the terrain
        m_Terrain.Initialize( m_pd3dDevice );
        m_Terrain.Generate( 48, 48, XMFLOAT2( GPUFLOAT_MIN, GPUFLOAT_MIN ),
                            XMFLOAT2( GPUFLOAT_MAX, GPUFLOAT_MAX ),
                            m_pGroundTexture, g_MapFunc[g_MapMode], 4.0f, 4.0f );

        // Create the plane map
        CreatePlaneMap();
    }

    // Animation attributes
    FLOAT fPhase = m_fTime / 4;

    // Move an object
    XMMATRIX matWorld, matTrans, matRotate1, matRotate2;
    matWorld = XMMatrixScaling( 1.0f, 1.0f, 1.0f );
    matRotate2 = XMMatrixRotationY( fPhase );
    matWorld = XMMatrixMultiply( matWorld, matRotate2 );

    if( g_EmitterMode == 1 )
    {
        EMITTERPARAMS emitter = g_EmitterParam[ g_EmitterMode ];
        XMMATRIX matRotate = XMMatrixRotationY( -m_fTime / 2 );

        XMVECTOR vec = XMVector3Transform( XMLoadFloat3( &emitter.EmitterParamPos ), matRotate2 );
        emitter.EmitterParamPos.x = vec.x;
        emitter.EmitterParamPos.y = vec.y;
        emitter.EmitterParamPos.z = vec.z;
        m_Particle.SetEmitter( &emitter );
    }

    // Set the vertex shader constants. Note: outside of the blend matrices,
    // most of these values don't change, so don't need to really be set every
    // frame. It's just done here for clarity
    {
        // Some basic constants
        XMFLOAT4 vZero( 0.0f, 0.0f, 0.0f, 0.0f );
        XMFLOAT4 vOne( 1.0f, 0.5f, 0.2f, 0.05f );

        XMFLOAT4 vLight( 1.5f, 1.5f, -1.5f, 0.0f );
        XMFLOAT4 vDiffuse( 0.70f, 0.70f, 0.70f, 1.00f );
        XMFLOAT4 vAmbient( 0.25f, 0.08f, 0.08f, 1.00f );
        XMFLOAT4 vFog( 0.5f, 50.0f, 1.0f / ( 50.0f - 1.0f ), 0.0f );

        XMVECTOR matWorldDet;
        XMMATRIX matModelInv = XMMatrixInverse( &matWorldDet, matWorld );
        XMVECTOR vLightModelSpace = XMVector4Normalize( XMVector4Transform( XMLoadFloat4( &vLight ), matModelInv ) );

        // Vertex shader operations use transposed matrices
        XMMATRIX matCamera = XMMatrixMultiply( matWorld, m_matView );
        XMMATRIX matWVP = XMMatrixMultiply( matCamera, m_matProj );
        XMMATRIX matTranspose = XMMatrixTranspose( matWVP );
        XMMATRIX matCameraTranspose = XMMatrixTranspose( matCamera );
        XMMATRIX matViewTranspose = XMMatrixTranspose( m_matView );
        XMMATRIX matProjTranspose = XMMatrixTranspose( m_matProj );

        // Set the vertex shader constants
        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&vZero, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 1, ( FLOAT* )&vOne, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matTranspose, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&matCameraTranspose, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 12, ( FLOAT* )&matViewTranspose, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 19, ( FLOAT* )&vLightModelSpace, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 21, ( FLOAT* )&vDiffuse, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 22, ( FLOAT* )&vAmbient, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 23, ( FLOAT* )&vFog, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 28, ( FLOAT* )&matProjTranspose, 4 );
    }

    // Upstate the particle system
    m_Particle.Update();

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, to render the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                         FOG_COLOR, 1.0f, 0L );

    // Render the terrain
    m_Terrain.Render();

    // Render the particle system
    m_Particle.Render();

    // Output statistics
    m_Timer.MarkFrame();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"GPUParticle" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CreatePlaneMap()
// Desc:
//-----------------------------------------------------------------------------
HRESULT Sample::CreatePlaneMap()
{
    //Create Normal&Plane map
    LPDIRECT3DSURFACE9 pPlaneMapRT;

    // Create the back buffer at base address 0.
    D3DSURFACE_PARAMETERS SurfaceParameters;
    memset( &SurfaceParameters, 0, sizeof( D3DSURFACE_PARAMETERS ) );
    SurfaceParameters.Base = 0;

    m_pd3dDevice->CreateRenderTarget( PLANEMAPSIZE, PLANEMAPSIZE, PLANEMAPFORMAT,
                                      D3DMULTISAMPLE_NONE, 0, 0, &pPlaneMapRT, &SurfaceParameters );

    XMMATRIX matRotate = XMMatrixRotationX( XM_PI / 2.f );
    XMMATRIX matOrtho = XMMatrixOrthographicLH( GPUFLOAT_MAX - GPUFLOAT_MIN,
                                                GPUFLOAT_MAX - GPUFLOAT_MIN,
                                                GPUFLOAT_MIN, GPUFLOAT_MAX );
    XMMATRIX matAll = XMMatrixMultiplyTranspose( matRotate, matOrtho );

    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matAll, 4 );

    m_pd3dDevice->SetRenderTarget( 0, pPlaneMapRT );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L );

    m_Terrain.RenderPlaneMap();

    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_ALLFRAGMENTS, NULL,
                           m_pPlaneMap, NULL, 0, 0, NULL, 0, 0, NULL );

    // Set the render target back to the back buffer
    LPDIRECT3DSURFACE9 pBackBuffer;
    m_pd3dDevice->GetBackBuffer( 0, 0, 0, &pBackBuffer );
    m_pd3dDevice->SetRenderTarget( 0, pBackBuffer );
    pBackBuffer->Release();

    pPlaneMapRT->Release();

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: InitializeParticles()
// Desc:
//-----------------------------------------------------------------------------
HRESULT Sample::InitializeParticles()
{
    LPDIRECT3DTEXTURE9 pTexture[4];
    m_Particle.GetParticleBuffer( &pTexture[0], &pTexture[1], &pTexture[2], &pTexture[3] );

    // Lock the textures
    D3DLOCKED_RECT lock[4];
    FLOAT* pData[4];

    for( DWORD tex = 0; tex < 4; tex++ )
    {
        pTexture[tex]->LockRect( 0, &lock[tex], NULL, 0 );
        pData[tex] = ( FLOAT* )lock[tex].pBits;
    }

    // Initialize the particle data
    for( DWORD i = 0; i < m_Particle.GetNumParticles(); i++ )
    {
        *pData[0]++ = ( FLOAT )rand() / ( FLOAT )RAND_MAX * 16.0f;        // Particle X coordinate
        *pData[0]++ = ( FLOAT )rand() / ( FLOAT )RAND_MAX - 32.0f;        // Particle Y coordinate
        *pData[1]++ = ( FLOAT )rand() / ( FLOAT )RAND_MAX * 16.0f;        // Particle Z coordinate
        *pData[1]++ = ( FLOAT )rand() / ( FLOAT )RAND_MAX * 3.0f;        // Particle life
        *pData[2]++ = ( FLOAT )rand() / ( FLOAT )RAND_MAX - 0.5f;        // Particle X velocity
        *pData[2]++ = ( FLOAT )rand() / ( FLOAT )RAND_MAX + 5.0f;        // Particle Y velocity
        *pData[3]++ = ( FLOAT )rand() / ( FLOAT )RAND_MAX - 0.5f;        // Particle Z velocity
        pData[3]++;
    }

    // Unlock the textures
    for( DWORD tex = 0; tex < 4; tex++ )
    {
        pTexture[tex]->UnlockRect( 0 );
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: HeightFn()
// Desc: Height field function for our terrain
//-----------------------------------------------------------------------------
FLOAT __stdcall HeightFn( FLOAT fX, FLOAT fZ )
{
    return 1.5f * ( cosf( fX / 4 ) * cosf( fZ / 3 ) ) +
        1.0f * ( cosf( fX / 8 ) * cosf( fZ / 6 ) ) - 0.5f;
}


//-----------------------------------------------------------------------------
// Name: HeightFn2()
// Desc: Height field function for our terrain
//-----------------------------------------------------------------------------
FLOAT __stdcall HeightFn2( FLOAT fX, FLOAT fZ )
{
    return cosf( fX ) + 0.5f;
}


//-----------------------------------------------------------------------------
// Name: HeightFn3()
// Desc: Height field function for our terrain
//-----------------------------------------------------------------------------
FLOAT __stdcall HeightFn3( FLOAT fX, FLOAT fZ )
{
    return ( FLOAT )rand() / ( FLOAT )( RAND_MAX / 2 );
}
