//-----------------------------------------------------------------------------
// PointSprites.cpp
//
// How to use point sprites to do particle effects
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>


//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_2, L"Move\ncamera" },
    { ATG::HELP_RIGHTSTICK,   ATG::HELP_PLACEMENT_2, L"Rotate\ncamera" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Animate\nemitter" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_2, L"Change\ncolor" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle\ndiscard band" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_1, L"Pause" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_2, L"Add/remove\nparticles" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//-----------------------------------------------------------------------------
// Custom vertex types
//-----------------------------------------------------------------------------
struct PARTICLE_VERTEX
{
    XMFLOAT3 v;
    DWORD color;
};

struct DIFFUSETEX_VERTEX
{
    XMFLOAT3 v;
    DWORD color;
    FLOAT tu;
    FLOAT tv;
};


//-----------------------------------------------------------------------------
// Global structs and data for the ground object
//-----------------------------------------------------------------------------
#define GROUND_SIZE  20.0f
#define GROUND_COLOR 0xddeeeeff


//-----------------------------------------------------------------------------
// Global data for the particles
//-----------------------------------------------------------------------------
struct PARTICLE
{
    XMVECTOR m_vPos;       // Current position
    XMVECTOR m_vVel;       // Current velocity

    XMVECTOR m_vPos0;      // Initial position
    XMVECTOR m_vVel0;      // Initial velocity
    FLOAT m_fTime0;     // Time of creation

    D3DXCOLOR m_clrDiffuse; // Initial diffuse color
    D3DXCOLOR m_clrFade;    // Faded diffuse color
    FLOAT m_fFade;      // Fade progression

    BOOL m_bSpark;     // Spark? or real particle?
};


enum PARTICLE_COLORS
{
    COLOR_WHITE,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    NUM_COLORS
};


D3DXCOLOR g_clrColor[NUM_COLORS] =
{
    D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ),
    D3DXCOLOR( 1.0f, 0.5f, 0.5f, 1.0f ),
    D3DXCOLOR( 0.5f, 1.0f, 0.5f, 1.0f ),
    D3DXCOLOR( 0.125f, 0.5f, 1.0f, 1.0f )
};


DWORD g_clrColorFade[NUM_COLORS] =
{
    D3DXCOLOR( 1.0f, 0.25f, 0.25f, 1.0f ),
    D3DXCOLOR( 1.0f, 0.25f, 0.25f, 1.0f ),
    D3DXCOLOR( 0.25f, 0.75f, 0.25f, 1.0f ),
    D3DXCOLOR( 0.125f, 0.25f, 0.75f, 1.0f )
};


// # of vertex buffers for the particle system
#define NUM_PARTICLE_BUFFERS 3


//-----------------------------------------------------------------------------
// Name: class CParticleSystem
// Desc: The particle system class
//-----------------------------------------------------------------------------
class CParticleSystem
{
protected:
    FLOAT m_fRadius;
    PARTICLE* m_pParticles;
    DWORD m_dwMaxParticles;
    DWORD m_dwNumParticles;

    // Geometry
    LPDIRECT3DVERTEXBUFFER9 m_pPointSpritesVBs[NUM_PARTICLE_BUFFERS];
    LPDIRECT3DVERTEXBUFFER9 m_pLightsVBs[NUM_PARTICLE_BUFFERS];
    LPDIRECT3DVERTEXBUFFER9 m_pPointSpritesVB;
    LPDIRECT3DVERTEXBUFFER9 m_pLightsVB;
    DWORD m_dwCurrentBuffer;
    DWORD m_dwNumParticlesToRender;
    DWORD m_dwNumLightsToRender;

public:
                            CParticleSystem( DWORD dwMaxParticles, FLOAT fRadius );
                            ~CParticleSystem();

    HRESULT                 InitDeviceObjects();
    HRESULT                 DeleteDeviceObjects();

    HRESULT                 Update( FLOAT fSecsPerFrame, DWORD dwNumParticlesToEmit,
                                    const D3DXCOLOR& dwEmitColor, const D3DXCOLOR& dwFadeColor,
                                    FLOAT fEmitVel, XMVECTOR vPosition );

    HRESULT                 RenderParticles();
    HRESULT                 RenderLights();
};


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::PackedResource m_Resource;
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;        // Whether to draw help

    // Particle system timing
    BOOL m_bParticleSystemRunning;
    FLOAT m_fParticleSystemTime;
    FLOAT m_fElapsedParticleSystemTime;

    // Particle stuff
    LPDIRECT3DTEXTURE9 m_pParticleTexture;
    CParticleSystem* m_pParticleSystem;
    FLOAT m_fNumParticlesToEmitPerSec;
    DWORD m_dwParticleColor;
    BOOL m_bAnimateEmitter;

    BOOL m_bEnableDiscardBand;

    // Ground stuff
    LPDIRECT3DVERTEXBUFFER9 m_pGroundVB;
    LPDIRECT3DTEXTURE9 m_pGroundTexture;
    DWORD   m_Pad[2];
    XMVECTOR m_vGroundPlane;

    // Static vectors for determining view position
    XMVECTOR m_vPosition;
    XMVECTOR m_vVelocity;
    FLOAT m_fYaw;
    FLOAT m_fYawVelocity;
    FLOAT m_fPitch;
    FLOAT m_fPitchVelocity;

    XMMATRIX m_matOrientation;
    XMMATRIX m_matWorld;
    XMMATRIX m_matProj;
    XMMATRIX m_matView;
    XMMATRIX m_matReflectedView;
    XMMATRIX m_matReflectedWVPT;
    XMMATRIX m_matWVPT;

    // Shaders
    LPDIRECT3DVERTEXDECLARATION9 m_pDiffuseTexVtxDecl;
    LPDIRECT3DVERTEXSHADER9 m_pDiffuseTexVS;
    LPDIRECT3DPIXELSHADER9 m_pTexModDiffusePS;
    LPDIRECT3DVERTEXDECLARATION9 m_pParticleVertexDecl;
    LPDIRECT3DVERTEXSHADER9 m_pParticleVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pParticlePixelShader;

private:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize the app
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    // Init member variables
    m_bDrawHelp = FALSE;

    m_bParticleSystemRunning = TRUE;
    m_fParticleSystemTime = 0.0f;
    m_fElapsedParticleSystemTime = 0.0f;

    m_pGroundTexture = NULL;
    m_pGroundVB = NULL;
    m_vGroundPlane = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

    m_pParticleTexture = NULL;
    m_pParticleSystem = new CParticleSystem( 4096, 0.03f );
    m_fNumParticlesToEmitPerSec = 600.0f;
    m_bAnimateEmitter = FALSE;
    m_bEnableDiscardBand = TRUE;
    m_dwParticleColor = COLOR_WHITE;

    m_vPosition = XMVectorSet( 0.0f, 3.0f, -4.0f, 0.0f );
    m_vVelocity = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    m_fYaw = 0.0f;
    m_fYawVelocity = 0.0f;
    m_fPitch = 0.5f;
    m_fPitchVelocity = 0.0f;
    m_matView = XMMatrixTranslation( 0.0f, 0.0f, 10.0f );
    m_matOrientation = XMMatrixTranslation( 0.0f, 0.0f, 0.0f );

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the resources
    if( FAILED( m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create textures
    m_pGroundTexture = m_Resource.GetTexture( "Ground" );
    m_pParticleTexture = m_Resource.GetTexture( "Particle" );

    // Create vertex buffer for ground object
    hr = m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( DIFFUSETEX_VERTEX ),
                                           D3DUSAGE_WRITEONLY, 0L,
                                           D3DPOOL_MANAGED, &m_pGroundVB, NULL );
    if( FAILED( hr ) )
        return E_FAIL;

    // Fill vertex buffer
    DIFFUSETEX_VERTEX* pVertices;
    m_pGroundVB->Lock( 0, 0, ( VOID** )&pVertices, NULL );
    pVertices[0].v = XMFLOAT3( -GROUND_SIZE / 2, 0.0f, -GROUND_SIZE / 2 );
    pVertices[0].color = GROUND_COLOR;
    pVertices[0].tu = 0.0f;
    pVertices[0].tv = 0.0f;
    pVertices[1].v = XMFLOAT3( -GROUND_SIZE / 2, 0.0f, +GROUND_SIZE / 2 );
    pVertices[1].color = GROUND_COLOR;
    pVertices[1].tu = 0.0f;
    pVertices[1].tv = 3.0f;
    pVertices[2].v = XMFLOAT3( +GROUND_SIZE / 2, 0.0f, +GROUND_SIZE / 2 );
    pVertices[2].color = GROUND_COLOR;
    pVertices[2].tu = 3.0f;
    pVertices[2].tv = 3.0f;
    pVertices[3].v = XMFLOAT3( +GROUND_SIZE / 2, 0.0f, -GROUND_SIZE / 2 );
    pVertices[3].color = GROUND_COLOR;
    pVertices[3].tu = 3.0f;
    pVertices[3].tv = 0.0f;
    m_pGroundVB->Unlock();

    // Create the common vertex shader
    {
        static const D3DVERTEXELEMENT9 decl[] =
        {
            { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
            { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR,    0 },
            { 0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };
        m_pd3dDevice->CreateVertexDeclaration( decl, &m_pDiffuseTexVtxDecl );

        if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\PositionDiffuseTexcoord.xvu",
                                                &m_pDiffuseTexVS ) ) )
        {
            ATG_PrintError( "Couldn't create CommonVS.xvu\n" );
            return ATGAPPERR_MEDIANOTFOUND;
        }
    }

    // Create the common pixel shader
    {
        if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\TextureModDiffuse.xpu",
                                               &m_pTexModDiffusePS ) ) )
        {
            ATG_PrintError( "Couldn't create TextureModDiffuse.xpu\n" );
            return ATGAPPERR_MEDIANOTFOUND;
        }
    }

    // Create the vertex shader for the particles
    {
        static const D3DVERTEXELEMENT9 decl[] =
        {
            { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
            { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR,    0 },
            D3DDECL_END()
        };
        m_pd3dDevice->CreateVertexDeclaration( decl, &m_pParticleVertexDecl );

        if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\PointSprite.xvu",
                                                &m_pParticleVertexShader ) ) )
            return ATGAPPERR_MEDIANOTFOUND;
    }

    // Create the pixel shaders for rendering the pointsprites
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\PointSprite.xpu",
                                      &m_pParticlePixelShader ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize the particle system
    if( FAILED( hr = m_pParticleSystem->InitDeviceObjects() ) )
        return hr;

    // Set the world matrix
    m_matWorld = XMMatrixIdentity();

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set projection matrix
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, 0.1f, 100.0f );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Update()
// Desc: Animate the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT m_fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // De-accelerate the camera movement (for smooth motion)
    FLOAT fScale = max( 0.0f, ( 1.0f - 2.0f * m_fElapsedTime ) );
    m_vVelocity *= fScale;
    m_fYawVelocity *= fScale;
    m_fPitchVelocity *= fScale;

    // Update velocities from the gamepad
    m_vVelocity.x += 1.0f * m_fElapsedTime * pGamepad->fX1; // Slide left/right
    m_vVelocity.y += 1.0f * m_fElapsedTime * pGamepad->fY1; // Slide up/down
    m_vVelocity.z -= 0.004f * pGamepad->bLeftTrigger * m_fElapsedTime;
    m_vVelocity.z += 0.004f * pGamepad->bRightTrigger * m_fElapsedTime;

    m_fYawVelocity += 1.0f * m_fElapsedTime * pGamepad->fX2; // Turn left/right
    m_fPitchVelocity -= 1.0f * m_fElapsedTime * pGamepad->fY2; // Turn up/down

    // Handle options
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        m_fNumParticlesToEmitPerSec *= 1.1f;
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        m_fNumParticlesToEmitPerSec *= 0.9f;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_bAnimateEmitter = !m_bAnimateEmitter;
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_dwParticleColor = ( m_dwParticleColor + 1 ) % NUM_COLORS;
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        m_bEnableDiscardBand = !m_bEnableDiscardBand;

    // Update the position vector
    XMVECTOR vT = m_vVelocity * 5.0f * m_fElapsedTime;
    vT = XMVector3TransformNormal( vT, m_matOrientation );
    m_vPosition += vT;
    if( m_vPosition.y < 1.0f )
        m_vPosition.y = 1.0f;

    // Update the yaw-pitch-rotation vector
    m_fYaw += 5.0f * m_fElapsedTime * m_fYawVelocity;
    m_fPitch += 5.0f * m_fElapsedTime * m_fPitchVelocity;
    if( m_fPitch < 0.0f )    m_fPitch = 0.0f;
    if( m_fPitch > XM_PI / 2 ) m_fPitch = XM_PI / 2;

    // Set the view matrix
    XMVECTOR vDeterminant;
    XMVECTOR qR;
    qR = XMQuaternionRotationRollPitchYaw( m_fPitch, m_fYaw, 0.0f );
    m_matOrientation = XMMatrixAffineTransformation( XMVectorSet( 1.25f, 1.25f, 1.25f, 1.0f ),
                                                     XMVectorZero(), qR, m_vPosition );
    m_matView = XMMatrixInverse( &vDeterminant, m_matOrientation );

    // Computed the reflected view
    m_matReflectedView = XMMatrixReflect( m_vGroundPlane );
    m_matReflectedView = XMMatrixMultiply( m_matReflectedView, m_matView );

    // Build composite matrices
    m_matWVPT = XMMatrixMultiply( m_matWorld, m_matView );
    m_matWVPT = XMMatrixMultiply( m_matWVPT, m_matProj );
    m_matWVPT = XMMatrixTranspose( m_matWVPT );

    m_matReflectedWVPT = XMMatrixMultiply( m_matWorld, m_matReflectedView );
    m_matReflectedWVPT = XMMatrixMultiply( m_matReflectedWVPT, m_matProj );
    m_matReflectedWVPT = XMMatrixTranspose( m_matReflectedWVPT );

    // Check the Start button to start/stop the particle system
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
        m_bParticleSystemRunning = !m_bParticleSystemRunning;

    if( m_bParticleSystemRunning )
        m_fElapsedParticleSystemTime = m_fElapsedTime;
    else
        m_fElapsedParticleSystemTime = 0.0f;
    m_fParticleSystemTime += m_fElapsedParticleSystemTime;

    // Determine emitter position
    XMVECTOR vEmitterPostion = XMVectorZero();
    if( m_bAnimateEmitter )
        vEmitterPostion = XMVectorSet( 3 * sinf( m_fParticleSystemTime ), 0.0f, 3 * cosf( m_fParticleSystemTime ),
                                       1.0f );

    // Determine framerate-independent number of particles to emit this frame
    static FLOAT fNumParticlesToEmit = 0.0f;
    fNumParticlesToEmit += m_fNumParticlesToEmitPerSec * m_fElapsedTime;
    DWORD dwNumParticlesToEmitThisFrame = ( DWORD )floorf( fNumParticlesToEmit );
    fNumParticlesToEmit -= dwNumParticlesToEmitThisFrame;

    // Update particle system
    m_pParticleSystem->Update( m_fElapsedParticleSystemTime, dwNumParticlesToEmitThisFrame,
                               g_clrColor[m_dwParticleColor], g_clrColorFade[m_dwParticleColor],
                               8.0f, vEmitterPostion );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Render the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                         0x000000ff, 1.0f, 0L );

    // Set common shaders
    m_pd3dDevice->SetPixelShader( m_pTexModDiffusePS );
    m_pd3dDevice->SetVertexDeclaration( m_pDiffuseTexVtxDecl );
    m_pd3dDevice->SetVertexShader( m_pDiffuseTexVS );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m_matWVPT, 4 );

    // Set state for rendering the ground
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // Draw the ground
    m_pd3dDevice->SetTexture( 0, m_pGroundTexture );
    m_pd3dDevice->SetStreamSource( 0, m_pGroundVB, 0, sizeof( DIFFUSETEX_VERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );

    // Modify state for rendering particles and lights
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAREF, 0x10 );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );

    // Render the ground effect lights
    m_pd3dDevice->SetTexture( 0, m_pParticleTexture );
    m_pParticleSystem->RenderLights();

    // Modify state for rendering particels
    m_pd3dDevice->SetRenderState( D3DRS_POINTSPRITEENABLE, TRUE );
    m_pd3dDevice->SetVertexDeclaration( m_pParticleVertexDecl );
    m_pd3dDevice->SetVertexShader( m_pParticleVertexShader );
    m_pd3dDevice->SetPixelShader( m_pParticlePixelShader );

    // Compute discard band. Normal TV overscan makes this pretty worthless to do.
    const FLOAT fPointSpriteRadius = 48.0f / 2.0f;
    const FLOAT fScreenHalfWidth = 640.0f / 2.0f;
    const FLOAT fScreenHalfHeight = 480.0f / 2.0f;

    if( m_bEnableDiscardBand )
    {
        FLOAT fDiscardX = GPU_GUARDBANDFACTOR( fPointSpriteRadius, fScreenHalfWidth );
        FLOAT fDiscardY = GPU_GUARDBANDFACTOR( fPointSpriteRadius, fScreenHalfHeight );

        m_pd3dDevice->SetRenderState( D3DRS_DISCARDBAND_X, *( DWORD* )&fDiscardX );
        m_pd3dDevice->SetRenderState( D3DRS_DISCARDBAND_Y, *( DWORD* )&fDiscardY );
    }

    // Render the particles
    static FLOAT fOpacity = 1.0f;
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m_matWVPT, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&fOpacity, 1 );
    m_pParticleSystem->RenderParticles();

    // Draw reflection of particles
    static FLOAT fOpacityR = 0.25f;
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m_matReflectedWVPT, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&fOpacityR, 1 );
    m_pParticleSystem->RenderParticles();

    // Restore state
    m_pd3dDevice->SetPixelShader( NULL );
    m_pd3dDevice->SetRenderState( D3DRS_POINTSPRITEENABLE, FALSE );

    if( m_bEnableDiscardBand )
    {
        FLOAT fOne = 1.0f;
        m_pd3dDevice->SetRenderState( D3DRS_DISCARDBAND_X, *( DWORD* )&fOne );
        m_pd3dDevice->SetRenderState( D3DRS_DISCARDBAND_Y, *( DWORD* )&fOne );
    }

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"PointSprites" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CParticleSystem()
// Desc: Constructur for the particle system
//-----------------------------------------------------------------------------
CParticleSystem::CParticleSystem( DWORD dwMaxParticles, FLOAT fRadius )
{
    m_fRadius = fRadius;

    m_pParticles = new PARTICLE[dwMaxParticles];
    m_dwMaxParticles = dwMaxParticles;
    m_dwNumParticles = 0;

    m_pPointSpritesVB = NULL;
    m_pLightsVB = NULL;
    m_dwCurrentBuffer = 0;

    m_dwNumParticlesToRender = 0;
    m_dwNumLightsToRender = 0;
}


//-----------------------------------------------------------------------------
// Name: ~CParticleSystem()
// Desc: Destructor for the particle system
//-----------------------------------------------------------------------------
CParticleSystem::~CParticleSystem()
{
    DeleteDeviceObjects();

    if( m_pParticles )
        delete[] m_pParticles;
}


//-----------------------------------------------------------------------------
// Name: InitDeviceObjects()
// Desc: Initialize device-dependent objects
//-----------------------------------------------------------------------------
HRESULT CParticleSystem::InitDeviceObjects()
{
    // Create the particle texture
    // Create the particle system's vertex buffers. Each point sprite particle
    // requires one vertex and each light takes four vertices.
    for( DWORD buf = 0; buf < NUM_PARTICLE_BUFFERS; buf++ )
    {
        ATG::g_pd3dDevice->CreateVertexBuffer( ( m_dwMaxParticles + 1 ) * sizeof( PARTICLE_VERTEX ),
                                               D3DUSAGE_WRITEONLY, 0L,
                                               D3DPOOL_DEFAULT, &m_pPointSpritesVBs[buf], NULL );
        ATG::g_pd3dDevice->CreateVertexBuffer( 4 * ( m_dwMaxParticles + 1 ) * sizeof( DIFFUSETEX_VERTEX ),
                                               D3DUSAGE_WRITEONLY, 0L,
                                               D3DPOOL_DEFAULT, &m_pLightsVBs[buf], NULL );

        // Write default values to light vertices (the texture coordinates for
        // the quads never changes)
        DIFFUSETEX_VERTEX* pLightVertices;
        m_pLightsVBs[buf]->Lock( 0, 0, ( VOID** )&pLightVertices, NULL );
        ZeroMemory( pLightVertices, 4 * m_dwMaxParticles * sizeof( DIFFUSETEX_VERTEX ) );

        for( DWORD i = 0; i < m_dwMaxParticles; i++ )
        {
            pLightVertices[1].tv = 1.0f;
            pLightVertices[2].tu = 1.0f;
            pLightVertices[2].tv = 1.0f;
            pLightVertices[3].tu = 1.0f;
            pLightVertices += 4;
        }

        // Unlock the vertex buffer
        m_pLightsVBs[buf]->Unlock();
    }

    // Select starting vertex buffers
    m_dwCurrentBuffer = 0;
    m_pPointSpritesVB = m_pPointSpritesVBs[m_dwCurrentBuffer];
    m_pLightsVB = m_pLightsVBs[m_dwCurrentBuffer];

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: DeleteDeviceObjects()
// Desc: Delete device dependent objects
//-----------------------------------------------------------------------------
HRESULT CParticleSystem::DeleteDeviceObjects()
{
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Update()
// Desc: Update the particles in the particle system.
//-----------------------------------------------------------------------------
HRESULT CParticleSystem::Update( FLOAT fSecsPerFrame, DWORD dwNumParticlesToEmit,
                                 const D3DXCOLOR& clrEmitColor,
                                 const D3DXCOLOR& clrFadeColor, FLOAT fEmitVel,
                                 XMVECTOR vPosition )
{
    static FLOAT fTime = 0.0f;
    fTime += fSecsPerFrame;

    // For performance reasons, vertex buffers are multi-buffered. Each time
    // the vertex buffer contents are updated, use a new vertex buffer.
    if( ++m_dwCurrentBuffer >= NUM_PARTICLE_BUFFERS )
        m_dwCurrentBuffer = 0;
    m_pPointSpritesVB = m_pPointSpritesVBs[m_dwCurrentBuffer];
    m_pLightsVB = m_pLightsVBs[m_dwCurrentBuffer];

    // Lock vertex buffers
    PARTICLE_VERTEX* pPointSpriteVertices;
    DIFFUSETEX_VERTEX* pLightVertices;
    m_pPointSpritesVB->Lock( 0, 0, ( VOID** )&pPointSpriteVertices, NULL );
    m_pLightsVB->Lock( 0, 0, ( VOID** )&pLightVertices, NULL );
    m_dwNumParticlesToRender = 0;
    m_dwNumLightsToRender = 0;

    // Update particles
    for( DWORD i = 0; i < m_dwNumParticles; i++ )
    {
        PARTICLE* pParticle = &m_pParticles[i];

        // Calculate new position
        FLOAT t = fTime - pParticle->m_fTime0;

        if( pParticle->m_bSpark )
        {
            pParticle->m_vPos = pParticle->m_vVel0 * t + pParticle->m_vPos0;
            pParticle->m_vPos.y -= ( 0.5f * 5.0f ) * ( t * t );
            pParticle->m_vVel.y = pParticle->m_vVel0.y - 5.0f * t;
            pParticle->m_fFade -= fSecsPerFrame * 2.25f;
        }
        else
        {
            pParticle->m_vPos = pParticle->m_vVel0 * t + pParticle->m_vPos0;
            pParticle->m_vPos.y -= ( 0.5f * 9.8f ) * ( t * t );
            pParticle->m_vVel.y = pParticle->m_vVel0.y - 9.8f * t;
            pParticle->m_fFade -= fSecsPerFrame * 0.25f;
        }

        if( pParticle->m_fFade < 0.0f )
            pParticle->m_fFade = 0.0f;

        // Kill old particles
        if( pParticle->m_vPos.y < m_fRadius || pParticle->m_bSpark && pParticle->m_fFade <= 0.0f )
        {
            // Emit sparks
            if( !pParticle->m_bSpark )
            {
                for( int j = 0; j < 4; j++ )
                {
                    FLOAT fRand1 = ( ( FLOAT )rand() / ( FLOAT )RAND_MAX ) * XM_PI * 2.00f;
                    FLOAT fRand2 = ( ( FLOAT )rand() / ( FLOAT )RAND_MAX ) * XM_PI * 0.25f;

                    PARTICLE* pSpark = &m_pParticles[m_dwNumParticles++];
                    pSpark->m_bSpark = TRUE;
                    pSpark->m_vPos0 = pParticle->m_vPos;
                    pSpark->m_vPos0.y = m_fRadius;
                    pSpark->m_vVel0.x = pParticle->m_vVel.x * 0.25f + cosf( fRand1 ) * sinf( fRand2 );
                    pSpark->m_vVel0.z = pParticle->m_vVel.z * 0.25f + sinf( fRand1 ) * sinf( fRand2 );
                    pSpark->m_vVel0.y = cosf( fRand2 );
                    pSpark->m_vVel0.y *= ( ( FLOAT )rand() / ( FLOAT )RAND_MAX ) * 1.5f;
                    pSpark->m_vPos = pSpark->m_vPos0;
                    pSpark->m_vVel = pSpark->m_vVel0;
                    D3DXColorLerp( &pSpark->m_clrDiffuse, &pParticle->m_clrFade,
                                   &pParticle->m_clrDiffuse, pParticle->m_fFade );
                    pSpark->m_clrFade = D3DXCOLOR( 0.0f, 0.0f, 0.0f, 1.0f );
                    pSpark->m_fFade = 1.0f;
                    pSpark->m_fTime0 = fTime;
                }
            }

            // Kill this particle (we can do this fast, by simply moving the
            // last particle to take the current particle's place).
            m_pParticles[i--] = m_pParticles[--m_dwNumParticles];
        }
        else
        {
            // Build vertex buffers for the particles
            FLOAT fSpeed = XMVector3LengthSq( pParticle->m_vVel ).x;
            UINT dwSteps;
            if( fSpeed < 1.0f )        dwSteps = 2;
            else if( fSpeed < 4.00f ) dwSteps = 3;
            else if( fSpeed < 9.00f ) dwSteps = 4;
            else if( fSpeed < 12.25f ) dwSteps = 5;
            else if( fSpeed < 16.00f ) dwSteps = 6;
            else if( fSpeed < 20.25f ) dwSteps = 7;
            else
                dwSteps = 8;

            XMVECTOR vPos = pParticle->m_vPos;
            XMVECTOR vVel = pParticle->m_vVel * -0.04f / ( FLOAT )dwSteps;

            D3DXCOLOR clrDiffuse;
            D3DXColorLerp( &clrDiffuse, &pParticle->m_clrFade, &pParticle->m_clrDiffuse,
                           pParticle->m_fFade );
            DWORD dwDiffuse = ( DWORD )clrDiffuse;

            // Compute vertices for ground lighting effects
            if( vPos.y < 1.0f )
            {
                FLOAT fY = vPos.y;
                if( fY < 0.0f )
                    fY = 0.0f;

                FLOAT fSize = fY * 0.25f + m_fRadius;
                DWORD dwLightDiffuse = ( DWORD )( clrDiffuse * ( ( 1.0f - fY ) * 0.5f ) );

                pLightVertices[0].v.x = pLightVertices[1].v.x = vPos.x + fSize;
                pLightVertices[0].v.z = pLightVertices[3].v.z = vPos.z + fSize;
                pLightVertices[2].v.x = pLightVertices[3].v.x = vPos.x - fSize;
                pLightVertices[1].v.z = pLightVertices[2].v.z = vPos.z - fSize;
                pLightVertices[0].color = pLightVertices[1].color = dwLightDiffuse;
                pLightVertices[2].color = pLightVertices[3].color = dwLightDiffuse;

                // Advance to next light
                pLightVertices += 4;
                m_dwNumLightsToRender++;
            }

            // Use multiple pointsprites per particle to get a motion-blur effect
            for( DWORD j = 0; j < dwSteps; j++ )
            {
                if( vPos.y >= 0.0f )
                {
                    // Stop if the VB gets full.
                    if( m_dwNumParticlesToRender >= m_dwMaxParticles )
                        break;

                    XMStoreFloat3( &pPointSpriteVertices->v, vPos );
                    pPointSpriteVertices->color = dwDiffuse;
                    pPointSpriteVertices++;
                    m_dwNumParticlesToRender++;
                }
                vPos += vVel;
            }
        }
    }

    // Unlock the vertex buffers
    m_pPointSpritesVB->Unlock();
    m_pLightsVB->Unlock();

    // Emit new particles
    while( dwNumParticlesToEmit > 0 && m_dwNumParticles < m_dwMaxParticles / 4 )
    {
        FLOAT fRand1 = ( ( FLOAT )rand() / ( FLOAT )RAND_MAX ) * XM_PI * 2.00f;
        FLOAT fRand2 = ( ( FLOAT )rand() / ( FLOAT )RAND_MAX ) * XM_PI * 0.25f;

        PARTICLE* pParticle = &m_pParticles[m_dwNumParticles];
        pParticle->m_bSpark = FALSE;
        pParticle->m_vPos0 = vPosition;
        pParticle->m_vPos0.y += m_fRadius;
        pParticle->m_vVel0.x = cosf( fRand1 ) * sinf( fRand2 ) * 2.5f;
        pParticle->m_vVel0.z = sinf( fRand1 ) * sinf( fRand2 ) * 2.5f;
        pParticle->m_vVel0.y = cosf( fRand2 );
        pParticle->m_vVel0.y *= ( ( FLOAT )rand() / ( FLOAT )RAND_MAX ) * fEmitVel;
        pParticle->m_vPos = pParticle->m_vPos0;
        pParticle->m_vVel = pParticle->m_vVel0;
        pParticle->m_clrDiffuse = clrEmitColor;
        pParticle->m_clrFade = clrFadeColor;
        pParticle->m_fFade = 1.0f;
        pParticle->m_fTime0 = fTime;

        dwNumParticlesToEmit--;
        m_dwNumParticles++;
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: RenderParticles()
// Desc: Renders the particle system using pointsprites.
//-----------------------------------------------------------------------------
HRESULT CParticleSystem::RenderParticles()
{
    if( 0 == m_dwNumParticlesToRender )
        return S_OK;

    // Render particles
    ATG::g_pd3dDevice->SetStreamSource( 0, m_pPointSpritesVB, 0, sizeof( PARTICLE_VERTEX ) );
    ATG::g_pd3dDevice->DrawPrimitive( D3DPT_POINTLIST, 0, m_dwNumParticlesToRender );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: RenderLights()
// Desc: Renders ground lighting effects for the particle system.
//-----------------------------------------------------------------------------
HRESULT CParticleSystem::RenderLights()
{
    if( 0 == m_dwNumLightsToRender )
        return S_OK;

    // Render lights
    ATG::g_pd3dDevice->SetStreamSource( 0, m_pLightsVB, 0, sizeof( DIFFUSETEX_VERTEX ) );
    ATG::g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, m_dwNumLightsToRender );

    return S_OK;
}

