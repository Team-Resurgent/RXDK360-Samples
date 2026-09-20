//--------------------------------------------------------------------------------------
// SoftParticles.cpp
//
// Many games today suffer from artifacts when rendering fluid or gas-like objects such
// as fire, smoke, dust, steam, etc... The artifact demonstrates itself by sharp
// clip lines where the particle intersects the non-transparent scene geometry.
// To some degree particles can be strategically placed and their texture alpha channel
// modified to reduce these artifacts - but this is not an easy or flexible solution.
//
// The sample demonstrates an easy to implement and fast technique of how to smooth 
// particles by using the already built Z buffer of the scene with 
// opaque geometry to modify the alpha of particle quads in the pixel shader.
// The technique simply casts the view ray to lookup the Z value of the pixel behind
// the particle. Taking the difference of Zscene - Zparticle and using it to modulate
// the alpha channel of the particle achieves the effect.
// 
//
// Microsoft Game Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>
#include <AtgSceneAll.h>
#include <AtgCollision.h>


//--------------------------------------------------------------------------------------
// Globals variables and definitions
//--------------------------------------------------------------------------------------

// Particle vertex
struct PARTICLEVERTEX
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 tex;
};

static const DWORD  PARTICLE_VERTICES = 4;

// Number of animated texture frames
static const DWORD  PARTICLE_FRAMES = 16;

// Number of particles
struct PARTICLE_DATA
{
    XMFLOAT3 vPosition;
    DWORD dwParticleTextureIndex;
};

// Particle positions
PARTICLE_DATA g_Particles[] =
{
    XMFLOAT3( 1.0f, -0.5f, 0.0f ), 4,
    XMFLOAT3( 1.1f, -0.5f, -0.5f ), 0,
    XMFLOAT3( 4.3f, -1.0f, 0.8f ), 7,
    XMFLOAT3( -1.8f, -0.2f, 1.2f ), 1,
    XMFLOAT3( 2.8f, -0.5f, 2.5f ), 11,
    XMFLOAT3( 2.5f, -0.5f, 3.0f ), 10
};

static const DWORD  NUM_PARTICLES = ARRAYSIZE( g_Particles );

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_2, L"Change particle\nsoftness" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_1, L"Camera movement" },
    { ATG::HELP_RIGHT_STICK,    ATG::HELP_PLACEMENT_1, L"Camera direction" },
};
static const DWORD  NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
private:

    ATG::Timer m_Timer;    // Timer
    ATG::Font m_Font;     // Font for drawing text
    ATG::Help m_Help;     // Help object
    BOOL m_bDrawHelp;
    ATG::PackedResource m_Resource; // Bundled textures in a packed resource

    // Transform matrices
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    // Camera
    XMVECTOR m_vEyePt;
    XMVECTOR m_vLookatDir;
    XMVECTOR m_vUpVec;
    FLOAT m_fTheta;
    FLOAT m_fPhi;

    // Scene object
    ATG::Scene* m_pScene;
    LPDIRECT3DVERTEXSHADER9 m_pSceneVS;
    LPDIRECT3DPIXELSHADER9 m_pScenePS;
    LPDIRECT3DPIXELSHADER9 m_pSkyPS;

    // Particle object
    LPDIRECT3DVERTEXBUFFER9 m_pParticleVB;
    LPDIRECT3DTEXTURE9  m_pParticleTextures[PARTICLE_FRAMES];
    LPDIRECT3DPIXELSHADER9 m_pParticlePS;
    LPDIRECT3DVERTEXDECLARATION9 m_pParticleVertexDecl;
    FLOAT m_fParticleSoftness;
    DWORD               m_dwParticleIndices[NUM_PARTICLES]; // Sorted particle indices

    LPDIRECT3DTEXTURE9 m_pDepthTexture;

    // Misc
    FLOAT m_fTime;

    VOID                ParticleDepthSort();

public:

    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();
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
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    // Create the font
    if( FAILED( hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create font\n" );
        return hr;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    m_bDrawHelp = FALSE;

    if( FAILED( hr = m_Help.Create( "game:\\media\\help\\Help.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create help\n" );
        return hr;
    }

    // Create the textures resource
    if( FAILED( hr = m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create Resource.xpr\n" );
        return hr;
    }

    for( DWORD i = 0; i < PARTICLE_FRAMES; ++i )
    {
        CHAR strTextureName[80];
        sprintf_s( strTextureName, "Fire%d", i );
        m_pParticleTextures[i] = m_Resource.GetTexture( strTextureName );
    }

    // Create scene object
    m_pScene = new ATG::Scene();
    m_pScene->GetResourceDatabase()->AddBundledResources( &m_Resource );
    if( FAILED( ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\SoftParticles.xatg", m_pScene, NULL,
                                                    ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL ) ) )
        ATG::FatalError( "Could not load scene file." );

    // Create particle vertex buffer
    if( FAILED( hr = m_pd3dDevice->CreateVertexBuffer( PARTICLE_VERTICES * sizeof( PARTICLEVERTEX ),
                                                       D3DUSAGE_WRITEONLY, NULL, D3DPOOL_MANAGED, &m_pParticleVB,
                                                       NULL ) ) )
        return hr;

    PARTICLEVERTEX* pVertex;
    m_pParticleVB->Lock( 0, 0, ( VOID** )&pVertex, 0 );
    pVertex[0].position = XMFLOAT3( -1.5f, 0.0f, 0.0f );
    pVertex[0].tex.x = 0.0f;
    pVertex[0].tex.y = 1.0f;

    pVertex[1].position = XMFLOAT3( -1.5f, 4.0f, 0.0f );
    pVertex[1].tex.x = 0.0f;
    pVertex[1].tex.y = 0.0f;

    pVertex[2].position = XMFLOAT3( 1.5f, 4.0f, 0.0f );
    pVertex[2].tex.x = 1.0f;
    pVertex[2].tex.y = 0.0f;

    pVertex[3].position = XMFLOAT3( 1.5f, 0.0f, 0.0f );
    pVertex[3].tex.x = 1.0f;
    pVertex[3].tex.y = 1.0f;
    m_pParticleVB->Unlock();

    // Create vertex declaration
    static const D3DVERTEXELEMENT9 declScene[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL,   0 },
        { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    if( FAILED( hr = m_pd3dDevice->CreateVertexDeclaration( declScene, &m_pParticleVertexDecl ) ) )
    {
        ATG_PrintError( "Couldn't create vertex declaration\n" );
        return hr;
    }

    // Create vertex shader
    if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\SceneVS.xvu", &m_pSceneVS ) ) )
    {
        ATG_PrintError( "Couldn't create SceneVS.xvu\n" );
        return hr;
    }

    // Create pixel shaders
    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\ScenePS.xpu", &m_pScenePS ) ) )
    {
        ATG_PrintError( "Couldn't create ScenePS.xpu\n" );
        return hr;
    }

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\SkyPS.xpu", &m_pSkyPS ) ) )
    {
        ATG_PrintError( "Couldn't create SkyPS.xpu\n" );
        return hr;
    }

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\ParticlePS.xpu", &m_pParticlePS ) ) )
    {
        ATG_PrintError( "Couldn't create ParticlePS.xpu\n" );
        return hr;
    }

    if( FAILED( hr = m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, 1,
                                                  D3DUSAGE_DEPTHSTENCIL, D3DFMT_D24S8, 0, &m_pDepthTexture, NULL ) ) )
    {
        ATG_PrintError( "Couldn't create depth texture.\n" );
        return hr;
    }

    // Determine the aspect ratio
    FLOAT fAspect = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the transform matrices
    m_vEyePt = XMVectorSet( 0.0f, 1.0f, -5.0f, 0.0f );
    m_vLookatDir = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
    m_vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspect, 0.1f, 10000.0f );
    m_fTheta = 0.0f;
    m_fPhi = 0.0f;

    // Set default particle softness. Lower values generate softer particles.
    m_fParticleSoftness = 1.0f;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current time
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();
    m_fTime = ( FLOAT )m_Timer.GetAppTime();

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Change particle softness
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        m_fParticleSoftness *= 1.2f;
        if( m_fParticleSoftness > 1000.0f )
            m_fParticleSoftness = 1000.0f;
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        m_fParticleSoftness *= 1.0f / 1.2f;
        if( m_fParticleSoftness < 0.1f )
            m_fParticleSoftness = 0.1f;
    }

    // Set the view matrix for camera control
    m_fPhi += pGamepad->fX2 * fElapsedTime * 0.3f * XM_PI;
    m_fTheta += pGamepad->fY2 * fElapsedTime * 0.3f * XM_PI;

    m_vLookatDir = XMVectorSet( cosf( m_fTheta ) * sinf( m_fPhi ), sinf( m_fTheta ), cosf( m_fTheta ) * cosf( m_fPhi ),
                                0 );
    XMVECTOR vCrossDir = XMVectorSet( cosf( m_fPhi ), 0, -sinf( m_fPhi ), 0 );

    m_vEyePt += m_vLookatDir * pGamepad->fY1 * fElapsedTime * 2.0f;
    m_vEyePt += vCrossDir * pGamepad->fX1 * fElapsedTime * 2.0f;
    m_matView = XMMatrixLookAtLH( m_vEyePt, m_vEyePt + m_vLookatDir, m_vUpVec );

    // Sort particles so that they are drawn back to front
    ParticleDepthSort();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ParticleDepthSort()
// Desc: Sort particles by depth. 
//--------------------------------------------------------------------------------------
VOID Sample::ParticleDepthSort()
{
    FLOAT fParticleDepths[NUM_PARTICLES];

    for( DWORD i = 0; i < NUM_PARTICLES; ++i )
    {
        m_dwParticleIndices[i] = i;
        XMVECTOR vPos = XMLoadFloat3( &g_Particles[i].vPosition );
        fParticleDepths[i] = XMVector3Dot( m_vLookatDir, vPos - m_vEyePt ).x;
    }

    // Because of the small number of particles, a simple bubble sort is used.
    for( DWORD i = 0; i < NUM_PARTICLES; ++i )
    {
        for( DWORD j = i + 1; j < NUM_PARTICLES; ++j )
        {
            if( fParticleDepths[i] < fParticleDepths[j] )
            {
                FLOAT fTemp;
                fTemp = fParticleDepths[i];
                fParticleDepths[i] = fParticleDepths[j];
                fParticleDepths[j] = fTemp;

                DWORD dwTemp;
                dwTemp = m_dwParticleIndices[i];
                m_dwParticleIndices[i] = m_dwParticleIndices[j];
                m_dwParticleIndices[j] = dwTemp;
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering. This 
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                         0xff000000, 1.0f, 0L );

    // Initialize default device states at the start of the frame
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );

    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    // Set the scene shaders
    m_pd3dDevice->SetVertexShader( m_pSceneVS );
    m_pd3dDevice->SetPixelShader( m_pScenePS );

    XMMATRIX matTransViewProj = XMMatrixTranspose( m_matView * m_matProj );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matTransViewProj, 4 );

    // Lighting vector
    XMVECTOR vLight = XMVectorSet( 0.2, 0.8f, -0.2f, 0.0f );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&vLight, 1 );

    // Set ambient light
    XMVECTOR vAmbient = XMVectorSet( 0.25f, 0.25f, 0.25f, 1.0f );
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&vAmbient, 1 );

    // Render the scene
    ATG::NameIndexedCollection::iterator i;
    for( i = m_pScene->GetInstanceList()->begin(); i != m_pScene->GetInstanceList()->end(); i++ )
    {
        // Select models from the object list.
        if( ( *i )->IsDerivedFrom( ATG::Model::TypeID ) )
        {
            ATG::Model* pModel = ( ATG::Model* )( *i );

            // Loop over mesh mappings.
            DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
            for( DWORD dwMapIndex = 0; dwMapIndex < dwMeshMappingCount; ++dwMapIndex )
            {
                ATG::MeshMapping& mm = pModel->GetMeshMapping( dwMapIndex );
                ATG::BaseMesh* pMesh = mm.pMesh;

                // Set pixel sky pixel shader if rendering the sky dome
                if( wcscmp( pMesh->GetName().GetSafeString(), L"sky_shpereShape" ) == 0 )
                {
                    m_pd3dDevice->SetPixelShader( m_pSkyPS );
                }
                else
                {
                    m_pd3dDevice->SetPixelShader( m_pScenePS );
                }

                // Loop over mesh subsets.
                DWORD dwSubsetCount = pMesh->GetNumSubsets();
                for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
                {
                    ATG::MaterialInstance* pMaterial = mm.Materials[ dwSubsetIndex ];

                    // Retrieve diffuse texture and set it
                    ATG::MaterialParameter& param = pMaterial->GetRawParameter( 0 );
                    if( param.pValue != NULL )
                    {
                        ATG::Texture2D* pTex2D = ( ATG::Texture2D* )param.pValue;
                        m_pd3dDevice->SetTexture( 0, pTex2D->GetD3DTexture() );
                    }

                    // Render the mesh subset.
                    pMesh->RenderSubset( dwSubsetIndex, m_pd3dDevice );
                }
            }
        }
    }


    // Resolve the Z Buffer
    m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, m_pDepthTexture, NULL, 0,
                           0, NULL, 1.0f, 0, NULL );

    //
    // Render the particles
    // 
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    // Set inverse projection matrix
    XMVECTOR vDeterminant;
    XMMATRIX matTransInvProj = XMMatrixTranspose( XMMatrixInverse( &vDeterminant, m_matProj ) );
    m_pd3dDevice->SetPixelShaderConstantF( 2, ( FLOAT* )&matTransInvProj, 4 );

    // Set particle softness
    XMVECTOR vParticleSoftness = XMVectorSet( m_fParticleSoftness, 0.0f, 0.0f, 0.0f );
    m_pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&vParticleSoftness, 1 );

    // Set the particle pixel shader and textures
    m_pd3dDevice->SetPixelShader( m_pParticlePS );
    m_pd3dDevice->SetVertexDeclaration( m_pParticleVertexDecl );

    m_pd3dDevice->SetTexture( 1, m_pDepthTexture );
    m_pd3dDevice->SetStreamSource( 0, m_pParticleVB, 0, sizeof( PARTICLEVERTEX ) );

    for( DWORD dwParticle = 0; dwParticle < NUM_PARTICLES; ++dwParticle )
    {
        DWORD dwParticleIndex = m_dwParticleIndices[dwParticle];
        DWORD dwTextureIndex = ( DWORD )( g_Particles[dwParticleIndex].dwParticleTextureIndex +
                                          ( m_fTime * 25 ) ) % PARTICLE_FRAMES;
        m_pd3dDevice->SetTexture( 0, m_pParticleTextures[dwTextureIndex] );

        XMMATRIX matTranslation = XMMatrixTranslation( g_Particles[dwParticleIndex].vPosition.x,
                                                       g_Particles[dwParticleIndex].vPosition.y,
                                                       g_Particles[dwParticleIndex].vPosition.z );
        XMMATRIX matRotation = XMMatrixRotationRollPitchYaw( -m_fTheta, m_fPhi, 0.0f );
        matTransViewProj = XMMatrixTranspose( matRotation * matTranslation * m_matView * m_matProj );
        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matTransViewProj, 4 );

        m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );
    }

    // Output title and framerate
    m_Timer.MarkFrame();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"SoftParticles" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        WCHAR wstrText[256];
        swprintf_s( wstrText, L"Particle Softness %.1f", m_fParticleSoftness );
        m_Font.DrawText( 0, 30, 0xffffff00, wstrText, ATGFONT_RIGHT );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
