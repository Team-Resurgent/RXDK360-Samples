//--------------------------------------------------------------------------------------
// Glass.cpp
//
// Demonstrates glass refraction and reflection effects by using Fresnel simulation
// and physically based refraction formulae.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgPostProcess.h>
#include <AtgResource.h>
#include <AtgUtil.h>

#pragma warning(disable:6385)    // m_pSubsets declared as MESH_SUBSET[1], but used as MESH_SUBSET*

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Change\nglass color" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_2, L"Change glass\nrefractancy" },
    { ATG::HELP_Y_BUTTON,     ATG::HELP_PLACEMENT_1, L"Display Settings" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_1, L"Toggle Bloom" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_2, L"Change glass\ntransparency" },
    { ATG::HELP_LEFT_STICK,   ATG::HELP_PLACEMENT_1, L"Move camera" },
    { ATG::HELP_RIGHT_STICK,  ATG::HELP_PLACEMENT_1, L"Rotate camera" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))

// Background gradient colors
static const DWORD  GRADIENT_TOP_COLOR = 0x00007fff;
static const DWORD  GRADIENT_BOTTOM_COLOR = 0x00000000;

// Refraction and reflection texture properties
static const DWORD  REFRACTION_TEXTURE_WIDTH = 512;
static const DWORD  REFRACTION_TEXTURE_HEIGHT = 512;
static const FLOAT  REFRACTION_FOV = XM_PI / 2.0f;
static const DWORD  ENVIRONMENT_TEXTURE_WIDTH = 256;
static const DWORD  NUM_GLASS_TEXTURES = 2;

// Glass properties
static const FLOAT  GLASS_REFLECTANCY = 0.1f;
static const FLOAT  GLASS_SPECULAR = 150.0f;
static const FLOAT  GLASS_SPECULAR_AMPLITUDE = 1.06f;
static const FLOAT  GLASS_REFRACTION_INDEX = 1.05f;
static const FLOAT  GLASS_TRANSPARENCY = 0.8f;

// Bloom properties
static const FLOAT  BLOOM_SIZE = 4.0f;
static const FLOAT  BLOOM_SCALE = 1.0f;

// Bloom texture enum
enum BLOOM_TEXTURES
{
    SCENE_BLOOM_TEXTURE = 0,
    SMALL_BLOOM_TEXTURE,
    NUM_BLOOM_TEXTURES
};



//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::PackedResource m_Resource;          // Bundled textures in a packed resource
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    ATG::Mesh m_Scene;            // A scene to render

    ATG::Mesh m_GlassObject;       // Glass object to render
    XMVECTOR m_vGlassObjectTranslation;
    LPDIRECT3DTEXTURE9  m_pGlassTexture[NUM_GLASS_TEXTURES];     // Textures used to render the glass object
    DWORD m_dwCurrentGlassTexture;
    XMVECTOR m_vGlassObjectCenter;// Center coordinate of object

    XMMATRIX m_matView;           // View matrix
    XMMATRIX m_matProj;           // Projection matrix

    XMMATRIX m_matLight;          // Properties for the light
    XMVECTOR m_vLightPos;

    XMVECTOR m_vEyePt;            // Camera properties
    XMVECTOR m_vLookatDir;
    XMVECTOR m_vUp;

    LPDIRECT3DTEXTURE9 m_pGlassRefractionTexture;// Texture used to render refraction texture
    LPDIRECT3DSURFACE9 m_pRefractionRenderTarget; // Render target used to render refraction texture
    LPDIRECT3DSURFACE9 m_pDepthStencil; // Depth-stencil surface used to render refraction texture

    LPDIRECT3DTEXTURE9 m_pBackfaceZTexture;      // Texture used for rendering backfaces of glass object

    LPDIRECT3DCUBETEXTURE9 m_pEnvironmentTexture; // Cube texture used for environment mapping
    LPDIRECT3DSURFACE9 m_pReflectionRenderTarget; // Render target used to render cube texture

    LPDIRECT3DVERTEXSHADER9 m_pSceneVertexShader; // Custom vertex shaders
    LPDIRECT3DVERTEXSHADER9 m_pGlassVertexShader;
    LPDIRECT3DVERTEXSHADER9 m_pGlassZNVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pScenePixelShader;  // Custom pixel shaders
    LPDIRECT3DPIXELSHADER9 m_pGlassPixelShader;
    LPDIRECT3DPIXELSHADER9 m_pGlassZNPixelShader;

    ATG::PostProcess m_PostProcess;
    LPDIRECT3DTEXTURE9  m_pBloomTextures[NUM_BLOOM_TEXTURES]; // Full screen and downsampled bloom textures

    FLOAT m_fGlassSpecular;
    FLOAT m_fGlassSpecularAmplitude;
    FLOAT m_fGlassRefractionIndex;
    DWORD m_dwRefractionCount;
    FLOAT m_fGlassTransparency;

    FLOAT               m_fAmbientColor[4];

    BOOL m_bDisplaySettings;
    BOOL m_bUseBloom;

    HRESULT             RenderScene();
    HRESULT             RenderGlass( BOOL bZNOnlyRender );
    XMMATRIX            GetCubeMapViewMatrix( DWORD dwFace );

private:
    HRESULT             Initialize();
    HRESULT             Update();
    HRESULT             Render();
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
// Desc: Initializes the app
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    // Initialize base member variables
    m_matLight = XMMatrixIdentity();
    m_bDrawHelp = FALSE;
    m_bDisplaySettings = FALSE;
    m_bUseBloom = TRUE;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( hr = m_PostProcess.Initialize() ) )
        return hr;

    // Create the textures resource
    if( FAILED( hr = m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    for( DWORD i = 0; i < NUM_GLASS_TEXTURES; ++i )
    {
        char strTextureName[256];
        sprintf_s( strTextureName, "Glass%d.bmp", i + 1 );
        m_pGlassTexture[i] = m_Resource.GetTexture( strTextureName );
    }

    if( FAILED( hr = m_Scene.Create( "game:\\Media\\Meshes\\ruins.xbg", &m_Resource ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( hr = m_GlassObject.Create( "game:\\Media\\Meshes\\ChessKing.xbg" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    XMVECTOR vMin, vMax;
    m_GlassObject.ComputeBoundingBox( vMin, vMax );
    m_vGlassObjectCenter = ( vMin + vMax ) / 2.0f;

    // Set the view matrix
    m_vEyePt = XMVectorSet( 0.0f, 0.1f, -0.2f, 0.0f );
    m_vLookatDir = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
    m_vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matView = XMMatrixLookAtLH( m_vEyePt, m_vEyePt + m_vLookatDir, m_vUp );

    // Load shaders
    if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\ShadeSceneVertex.xvu", &m_pSceneVertexShader ) ) )
        return hr;

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeScenePixel.xpu", &m_pScenePixelShader ) ) )
        return hr;

    if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\ShadeGlassVertex.xvu", &m_pGlassVertexShader ) ) )
        return hr;

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeGlassPixel.xpu", &m_pGlassPixelShader ) ) )
        return hr;

    if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\ShadeZNGlassVertex.xvu",
                                            &m_pGlassZNVertexShader ) ) )
        return hr;

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeZNGlassPixel.xpu", &m_pGlassZNPixelShader ) ) )
        return hr;

    // Create refraction texture to be rendered from the point of view of the glass object
    if( FAILED( hr = m_pd3dDevice->CreateTexture( REFRACTION_TEXTURE_WIDTH, REFRACTION_TEXTURE_HEIGHT,
                                                  1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
                                                  D3DPOOL_DEFAULT, &m_pGlassRefractionTexture, NULL ) ) )
        return hr;

    if( FAILED( hr = m_pd3dDevice->CreateRenderTarget( REFRACTION_TEXTURE_WIDTH, REFRACTION_TEXTURE_HEIGHT,
                                                       ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8),
                                                       D3DMULTISAMPLE_NONE, 0, 0, &m_pRefractionRenderTarget,
                                                       NULL ) ) )
        return hr;

    if( FAILED( hr = m_pd3dDevice->CreateRenderTarget( ENVIRONMENT_TEXTURE_WIDTH, ENVIRONMENT_TEXTURE_WIDTH,
                                                       ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
                                                       D3DMULTISAMPLE_NONE, 0, 0, &m_pReflectionRenderTarget,
                                                       NULL ) ) )
        return hr;

    // In cases when dwTargetWidth and dwTargetHeight is known to be always less
    // than the main render target width and height, this depth-stencil surface
    // does not need to be created.
    DWORD dwTargetWidth = max( REFRACTION_TEXTURE_WIDTH, ENVIRONMENT_TEXTURE_WIDTH );
    DWORD dwTargetHeight = max( REFRACTION_TEXTURE_HEIGHT, ENVIRONMENT_TEXTURE_WIDTH );
    if( FAILED( hr = m_pd3dDevice->CreateDepthStencilSurface( dwTargetWidth, dwTargetHeight, D3DFMT_D24S8,
                                                              D3DMULTISAMPLE_NONE, 0, 0, &m_pDepthStencil, NULL ) ) )
        return hr;

    if( FAILED( hr = m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                                  1, 0, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &m_pBackfaceZTexture,
                                                  NULL ) ) )
        return hr;


    // Create environment cube texture
    if( FAILED( hr = m_pd3dDevice->CreateCubeTexture( ENVIRONMENT_TEXTURE_WIDTH, 1, 0,
                                                      ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ), D3DPOOL_DEFAULT, &m_pEnvironmentTexture,
                                                      NULL ) ) )
        return hr;

    // Create bloom textures
    if( FAILED( hr = m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                                  1, D3DUSAGE_RENDERTARGET, m_d3dpp.BackBufferFormat,
                                                  D3DPOOL_DEFAULT, &m_pBloomTextures[SCENE_BLOOM_TEXTURE], NULL ) ) )
        return hr;

    if( FAILED( hr = m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth / 4, m_d3dpp.BackBufferHeight / 4,
                                                  1, D3DUSAGE_RENDERTARGET, m_d3dpp.BackBufferFormat,
                                                  D3DPOOL_DEFAULT, &m_pBloomTextures[SMALL_BLOOM_TEXTURE], NULL ) ) )
        return hr;


    // Set glass properties
    m_fGlassSpecular = GLASS_SPECULAR;
    m_fGlassSpecularAmplitude = GLASS_SPECULAR_AMPLITUDE;
    m_fGlassRefractionIndex = GLASS_REFRACTION_INDEX;
    m_fGlassTransparency = GLASS_TRANSPARENCY;

    m_vGlassObjectTranslation = XMVectorSet( 0.0f, 0.008f, 0.0f, 1.0f );

    // Set light properties, the last (w) property is the light intensity
    m_vLightPos = XMVectorSet( 0.05f, 0.05f, 0.0f, 0.8f );
    m_fAmbientColor[0] = 0.22f;
    m_fAmbientColor[1] = 0.15f;
    m_fAmbientColor[2] = 0.1f;
    m_fAmbientColor[3] = 1.0f;

    // Initialize adjustable parameters
    m_dwCurrentGlassTexture = 0;
    m_dwRefractionCount = 0;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame to update the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT m_fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Switch glass textures
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_dwCurrentGlassTexture = ( m_dwCurrentGlassTexture + 1 ) % NUM_GLASS_TEXTURES;

    // Change refraction index
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_fGlassRefractionIndex = 1.05f + ( FLOAT )( m_dwRefractionCount % 10 ) / 50.0f;
        m_dwRefractionCount++;
    }

    // Change glass transparency
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        m_fGlassTransparency += 0.05f;
        if( m_fGlassTransparency > 1.0f ) m_fGlassTransparency = 1.0f;
    }
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        m_fGlassTransparency -= 0.05f;
        if( m_fGlassTransparency < 0.0f ) m_fGlassTransparency = 0.0f;
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        if( m_bDisplaySettings )
            m_bDisplaySettings = FALSE;
        else
            m_bDisplaySettings = TRUE;
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        if( m_bUseBloom )
            m_bUseBloom = FALSE;
        else
            m_bUseBloom = TRUE;
    }

    // Set the view matrix
    static FLOAT fTheta = -0.05f * XM_PI;
    static FLOAT fPhi = +0.0f * XM_PI;

    fPhi += pGamepad->fX2 * m_fElapsedTime * 0.3f * XM_PI;
    fTheta += pGamepad->fY2 * m_fElapsedTime * 0.3f * XM_PI;

    m_vLookatDir.x = cosf( fTheta ) * sinf( fPhi );
    m_vLookatDir.y = sinf( fTheta );
    m_vLookatDir.z = cosf( fTheta ) * cosf( fPhi );

    XMVECTOR vCrossDir;
    vCrossDir.x = +cosf( fPhi );
    vCrossDir.y = 0.0;
    vCrossDir.z = -sinf( fPhi );

    m_vEyePt += m_vLookatDir * pGamepad->fY1 * m_fElapsedTime * 0.3f;
    m_vEyePt += vCrossDir * pGamepad->fX1 * m_fElapsedTime * 0.3f;
    m_matView = XMMatrixLookAtLH( m_vEyePt, m_vEyePt + m_vLookatDir, m_vUp );

    // Set glass transparency, ambient color and backface refraction index constants
    FLOAT fGlassProperties2[4] =
    {
        m_fGlassTransparency, GLASS_REFLECTANCY, m_fGlassSpecular,
        m_fGlassSpecularAmplitude
    };
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&fGlassProperties2, 1 );
    FLOAT fFresnel = pow( m_fGlassRefractionIndex - 1.0f, 2.0f ) / pow( m_fGlassRefractionIndex + 1.0f, 2.0f );
    FLOAT fGlassProperties3[4] =
    {
        fFresnel, 0.5f / sinf( REFRACTION_FOV / 2.0f ),
        1.0f / m_fGlassRefractionIndex, 1.0f / ( m_fGlassRefractionIndex * m_fGlassRefractionIndex )
    };
    m_pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&fGlassProperties3, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 2, ( FLOAT* )&m_fAmbientColor, 1 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderScene()
// Desc: Renders the scene
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderScene()
{
    // Set default states
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    // The backface normal texture must be point sampled. Linear sampling
    // will give a "cracked glass" look due to billinear interpolation
    // artifacts at triangle edges.
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetVertexDeclaration( m_Scene.GetMesh( 0 )->m_pVertexDecl );

    m_pd3dDevice->SetVertexShader( m_pSceneVertexShader );

    XMMATRIX matTransView = XMMatrixTranspose( m_matView );
    XMMATRIX matTransProj = XMMatrixTranspose( m_matProj );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matTransView, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matTransProj, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&m_vLightPos, 1 );

    m_pd3dDevice->SetPixelShader( m_pScenePixelShader );

    m_pd3dDevice->SetStreamSource( 0, &m_Scene.GetMesh( 0 )->m_VB, 0, m_Scene.GetMesh( 0 )->m_dwVertexSize );
    m_pd3dDevice->SetIndices( &m_Scene.GetMesh( 0 )->m_IB );

    for( DWORD i = 0; i < m_Scene.GetMesh( 0 )->m_dwNumSubsets; ++i )
    {
        m_pd3dDevice->SetTexture( 0, m_Scene.GetMesh( 0 )->m_pSubsets[i].pTexture );
        m_pd3dDevice->DrawIndexedPrimitive( m_Scene.GetMesh( 0 )->m_dwPrimType, 0, 0,
                                            m_GlassObject.GetMesh( 0 )->m_dwNumVertices,
                                            m_Scene.GetMesh( 0 )->m_pSubsets[i].dwIndexStart,
                                            m_Scene.GetMesh( 0 )->m_pSubsets[i].dwPrimitiveCount );
    }

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: RenderGlass()
// Desc: Renders the glass objects in the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderGlass( BOOL bZNOnlyRender )
{
    // Set front face refraction index constants
    FLOAT vGlassProperties1[4] =
    {
        m_fGlassRefractionIndex, m_fGlassRefractionIndex * m_fGlassRefractionIndex, 0.0f,
        0.0f
    };
    m_pd3dDevice->SetVertexShader( bZNOnlyRender ? m_pGlassZNVertexShader : m_pGlassVertexShader );

    XMMATRIX matTransView = XMMatrixTranspose( XMMatrixTranslationFromVector( m_vGlassObjectTranslation ) *
                                               m_matView );
    XMMATRIX matTransProj = XMMatrixTranspose( m_matProj );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matTransView, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matTransProj, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&m_vLightPos, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 9, ( FLOAT* )&vGlassProperties1, 1 );

    m_pd3dDevice->SetPixelShader( bZNOnlyRender ? m_pGlassZNPixelShader : m_pGlassPixelShader );

    m_pd3dDevice->SetVertexDeclaration( m_GlassObject.GetMesh( 0 )->m_pVertexDecl );
    m_pd3dDevice->SetTexture( 0, m_pGlassTexture[m_dwCurrentGlassTexture] );
    m_pd3dDevice->SetTexture( 1, m_pGlassRefractionTexture );
    if( bZNOnlyRender == FALSE )
    {
        m_pd3dDevice->SetTexture( 2, m_pBackfaceZTexture );
        m_pd3dDevice->SetTexture( 3, m_pEnvironmentTexture );
    }
    m_pd3dDevice->SetStreamSource( 0, &m_GlassObject.GetMesh( 0 )->m_VB, 0,
                                   m_GlassObject.GetMesh( 0 )->m_dwVertexSize );
    m_pd3dDevice->SetIndices( &m_GlassObject.GetMesh( 0 )->m_IB );

    for( DWORD i = 0; i < m_GlassObject.GetMesh( 0 )->m_dwNumSubsets; ++i )
    {
        m_pd3dDevice->DrawIndexedPrimitive( m_GlassObject.GetMesh( 0 )->m_dwPrimType, 0, 0,
                                            m_GlassObject.GetMesh( 0 )->m_dwNumVertices,
                                            m_GlassObject.GetMesh( 0 )->m_pSubsets[i].dwIndexStart,
                                            m_GlassObject.GetMesh( 0 )->m_pSubsets[i].dwPrimitiveCount );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: GetCubeMapViewMatrix()
// Desc: Utility function that gets the view matrix for a cube map face.
//--------------------------------------------------------------------------------------
XMMATRIX Sample::GetCubeMapViewMatrix( DWORD dwFace )
{
    XMVECTOR vEyePt = m_vGlassObjectCenter + m_vGlassObjectTranslation;
    XMVECTOR vLookDir = {0};
    XMVECTOR vUpDir = {0};

    switch( dwFace )
    {
        case D3DCUBEMAP_FACE_POSITIVE_X:
            vLookDir = XMVectorSet( 1.0f, 0.0f, 0.0f, 0.0f );
            vUpDir = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
            break;
        case D3DCUBEMAP_FACE_NEGATIVE_X:
            vLookDir = XMVectorSet( -1.0f, 0.0f, 0.0f, 0.0f );
            vUpDir = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
            break;
        case D3DCUBEMAP_FACE_POSITIVE_Y:
            vLookDir = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
            vUpDir = XMVectorSet( 0.0f, 0.0f, -1.0f, 0.0f );
            break;
        case D3DCUBEMAP_FACE_NEGATIVE_Y:
            vLookDir = XMVectorSet( 0.0f, -1.0f, 0.0f, 0.0f );
            vUpDir = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
            break;
        case D3DCUBEMAP_FACE_POSITIVE_Z:
            vLookDir = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
            vUpDir = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
            break;
        case D3DCUBEMAP_FACE_NEGATIVE_Z:
            vLookDir = XMVectorSet( 0.0f, 0.0f, -1.0f, 0.0f );
            vUpDir = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
            break;
    }

    // Set the view transform for this cubemap surface
    return XMMatrixLookAtLH( vEyePt, vLookDir, vUpDir );
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame to render the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    //
    // Render the scene without any glass objects from the viewpoint of the 
    // glass object. If there is more than one glass object, the scene needs to be
    // rendered from each, however if there are a lot of glass objects in close
    // proximity, an approximation can be made by rendering the scene from the
    // center of all the glass objects.
    //
    m_matProj = XMMatrixPerspectiveFovLH( REFRACTION_FOV, 1.0f, 0.01f, 10000.0f );
    XMMATRIX matOldView = m_matView;
    m_matView = XMMatrixLookAtLH( m_vGlassObjectCenter + m_vGlassObjectTranslation, m_vLookatDir, m_vUp );

    D3DSurface* pRenderTarget0;
    D3DSurface* pDepthStencil;
    m_pd3dDevice->GetRenderTarget( 0, &pRenderTarget0 );
    m_pd3dDevice->GetDepthStencilSurface( &pDepthStencil );
    m_pd3dDevice->SetRenderTarget( 0, m_pRefractionRenderTarget );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencil );

    ATG::RenderBackground( GRADIENT_TOP_COLOR, GRADIENT_BOTTOM_COLOR );
    RenderScene();

    m_matView = matOldView;
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pGlassRefractionTexture, NULL, 0,
                           0, NULL, 0.0f, 0, NULL );

    //
    // Render the 6 faces of the environment map texture. In this particular case
    // the environment map does not need to be rendered every frame because it is
    // static.
    //
    m_pd3dDevice->SetRenderTarget( 0, m_pReflectionRenderTarget );
    matOldView = m_matView;
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 2.0f, 1.0f, 0.01f, 10000.0f );
    XMMATRIX matViewDir = m_matView;
    matViewDir._41 = matViewDir._42 = matViewDir._43 = 0.0f;

    for( DWORD dwFace = 0; dwFace < 6; ++dwFace )
    {
        m_matView = XMMatrixMultiply( matViewDir, GetCubeMapViewMatrix( dwFace ) );

        // Render cube backgrounds correctly
        if( dwFace == D3DCUBEMAP_FACE_POSITIVE_Y )
            ATG::RenderBackground( GRADIENT_TOP_COLOR, GRADIENT_TOP_COLOR );
        else if( dwFace == D3DCUBEMAP_FACE_NEGATIVE_Y )
            ATG::RenderBackground( GRADIENT_BOTTOM_COLOR, GRADIENT_BOTTOM_COLOR );
        else
            ATG::RenderBackground( GRADIENT_TOP_COLOR, GRADIENT_BOTTOM_COLOR );

        RenderScene();

        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pEnvironmentTexture, NULL, 0, dwFace, NULL, 0.0f, 0,
                               NULL );
    }

    m_matView = matOldView;

    m_pd3dDevice->SetRenderTarget( 0, pRenderTarget0 );
    m_pd3dDevice->SetDepthStencilSurface( pDepthStencil );
    pRenderTarget0->Release();
    pDepthStencil->Release();

    //
    // Render backfaces with normal and z
    //
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0x7f7f7f7f, 1.0f, 0L );
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, 0.01f, 10000.0f );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );
    RenderGlass( TRUE );

    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pBackfaceZTexture, NULL, 0,
                           0, NULL, 0.0f, 0, NULL );

    //
    // Render the full scene
    //
    ATG::RenderBackground( GRADIENT_TOP_COLOR, GRADIENT_BOTTOM_COLOR );
    RenderScene();
    RenderGlass( FALSE );

    if( m_bUseBloom )
    {
        //
        // Create bloom effect. The scene is rendered such that the specular highlights of the glass
        // are saved in the Alpha channel. The bloom effect will only bloom areas that have some Alpha.
        //
        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pBloomTextures[SCENE_BLOOM_TEXTURE], NULL, 0,
                               0, NULL, 0.0f, 0, NULL );

        // Downsample the scene, modulating RGB (bloom color) with Alpha (bloom amount)
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
        m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
        m_PostProcess.CopyTexture( m_pBloomTextures[SCENE_BLOOM_TEXTURE], m_pBloomTextures[SMALL_BLOOM_TEXTURE] );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

        // Do the bloom by applying a Gaussian blur to the downsampled texture. Because Gaussian blur
        // is separable, the filter is applied in the x and y directions separatelly.
        m_PostProcess.GaussBlur5x5Texture( m_pBloomTextures[SMALL_BLOOM_TEXTURE],
                                           m_pBloomTextures[SMALL_BLOOM_TEXTURE] );
        m_PostProcess.BloomTexture( m_pBloomTextures[SMALL_BLOOM_TEXTURE], TRUE, m_pBloomTextures[SMALL_BLOOM_TEXTURE],
                                    BLOOM_SIZE, BLOOM_SCALE );
        m_PostProcess.BloomTexture( m_pBloomTextures[SMALL_BLOOM_TEXTURE], FALSE,
                                    m_pBloomTextures[SMALL_BLOOM_TEXTURE], BLOOM_SIZE, BLOOM_SCALE );

        // Add the bloom back on top of the original scene
        m_PostProcess.AddTextures( m_pBloomTextures, NUM_BLOOM_TEXTURES, m_pBloomTextures[SCENE_BLOOM_TEXTURE] );
    }

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
        m_Font.DrawText( 0, 0, 0xffffffff, L"Glass" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        if( m_bDisplaySettings )
        {
            WCHAR strSetting[256];

            m_Font.DrawText( 00.0f, 80.0f, 0xffffffff, L"Refraction Index:" );
            swprintf_s( strSetting, L"%.2f", m_fGlassRefractionIndex );
            m_Font.DrawText( 180.0f, 80.0f, 0xffffffff, strSetting );

            m_Font.DrawText( 0.0f, 110.0f, 0xffffffff, L"Transparency:" );
            swprintf_s( strSetting, L"%.2f", m_fGlassTransparency );
            m_Font.DrawText( 180.0f, 110.0f, 0xffffffff, strSetting );

            m_Font.DrawText( 0.0f, 140.0f, 0xffffffff, L"Bloom:" );
            swprintf_s( strSetting, L"%s", m_bUseBloom ? L"On" : L"Off" );
            m_Font.DrawText( 180.0f, 140.0f, 0xffffffff, strSetting );
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

