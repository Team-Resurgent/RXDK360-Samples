//--------------------------------------------------------------------------------------
// ScreenSpaceAmbientOcclusion.cpp
//
// Demonstrates how the depth buffer can be used to calculate ambient occlusion as a
// post effect with no prepasses for normals or positions. This is very convenient,
// since the technique does not depend on the complexity of the scene. Thus you can
// have a complex scene with animated characters and it will still work. The cost
// of technique is about 3ms on the GPU at 720p, using 2 render passes with 8 occluder
// points each, one for local occlusion and one for global occlusion.
//
// This technique uses the depth buffer to back project each depth value to a camera
// space position, using the inverse projection matrix. Once in camera space, a set
// of occluder points are generated on a sphere, using an Occlusion Radius, around
// each camera space position. In the same way that a shadowmap lookup calculates if
// a point is shadowed or not, this technique looks at the depth difference of each
// calculated occluder position and the sampled depth value at the screen space
// position of that particular occluder. A distance function with a Falloff Radius
// is then used to calculate the occlusion.
//
// To achieve good visual results, two spheres of occluders are used, one for local
// occlusion and one for global occlusion. The results are then blurred and combined.
//
// XNA Developer Connection.
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


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_1, L"Toggle ambient occlusion" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_1, L"Toggle debug mode" },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle local \\ global\nambient occlusion" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_1, L"Toggle blur" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_2,
        L"Up \\ down adjust Occlusion Radius\nLeft \\ right adjust Falloff Radius" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_1, L"Move camera" },
    { ATG::HELP_RIGHT_STICK,    ATG::HELP_PLACEMENT_1, L"Rotate camera" },
    { ATG::HELP_START_BUTTON,   ATG::HELP_PLACEMENT_2, L"Display\nsettings" },
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_LEFT_SHOULDER,  ATG::HELP_PLACEMENT_2, L"Hold to adjust Local\nocclusion using D-Pad" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Hold to adjust Global\nocclusion using D-Pad" },
};
#define NUM_HELP_CALLOUTS ( sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] ) )


// Background gradient colors
static const DWORD  GRADIENT_TOP_COLOR = 0x00007fff;
static const DWORD  GRADIENT_BOTTOM_COLOR = 0xff000000;


// Number of occluder points
static const DWORD  NUM_OCCLUDERS = 8;


// SSAO mode enum
enum SSAO_MODE
{
    SSAO_MODE_LOCAL     = 0,      // Local ambient occlusion
    SSAO_MODE_GLOBAL    = 1,      // Global ambient occlusion
    SSAO_MODE_ALL       = 2,      // Combined local and global occlusion
    NUM_SSAO_MODES
};


//--------------------------------------------------------------------------------------
// Name: class ATG::Mesh
// Desc: Overridden mesh class for rendering a mesh
//--------------------------------------------------------------------------------------
class Mesh : public ATG::Mesh
{
public:
    BOOL RenderCallback( DWORD dwSubset, const ATG::MESH_SUBSET* pSubset, DWORD dwFlags )
    {
        // Set matrices for the vertex shader
        XMMATRIX matWorldView = XMMatrixMultiply( m_matWorld, m_matView );
        XMMATRIX matWorldViewProj = XMMatrixMultiply( matWorldView, m_matProj );
        XMMATRIX matWorldViewProjT = XMMatrixTranspose( matWorldViewProj );
        XMMATRIX matWorldT = XMMatrixTranspose( m_matWorld );

        ATG::g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWorldViewProjT, 4 );
        ATG::g_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matWorldT, 4 );

        return TRUE;
    }
};

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::PackedResource m_Resource;                  // Bundled textures in a packed resource
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    Mesh m_Scene;                     // A scene to render  
    Mesh    m_Objects[3];                // Objects to render

    XMMATRIX m_matView;                   // View matrix
    XMMATRIX m_matProj;                   // Projection matrix
    XMMATRIX m_matViewProj;               // View matrix

    XMVECTOR m_vEyePt;                    // Camera properties
    XMVECTOR m_vLookatDir;
    XMVECTOR m_vUp;

    XMVECTOR m_vLightDirection;           // Light direction vector

    LPDIRECT3DVERTEXSHADER9 m_pSceneVertexShader; // Custom vertex shader for demo
    LPDIRECT3DPIXELSHADER9 m_pScenePixelShader;  // Custom pixel shader for demo

    ATG::PostProcess m_PostProcess;

    FLOAT   m_fAmbientColor[4];

    HRESULT RenderScene();
    HRESULT RenderObjects();

    BOOL m_bDisplaySettings;                 // Display the current settigns
    BOOL m_bApplySSAO;                       // Toggle the effect on/off
    BOOL m_bDebugSSAO;                       // Show only the ambient occlusion
    BOOL m_bBlurSSAO;                        // Blur the ambient occlusion
    SSAO_MODE m_ModeSSAO;                         // Select which occlusion mode to use
    FLOAT m_fLocalOcclRadius;                 // Local occlusion radius
    FLOAT m_fLocalFalloffRadius;              // Local occlusion falloff radius
    FLOAT m_fGlobalOcclRadius;                // Global occlusion radius
    FLOAT m_fGlobalFalloffRadius;             // Global occlusion falloff radius

    IDirect3DTexture9* m_pDepthBufferTexture;				// Depth buffer texture
    IDirect3DTexture9* m_pLinearDepthBufferTexture;		// Linear depth buffer texture
    IDirect3DTexture9* m_pLocalAmbientOcclusionTexture;	// Local ambient occlusion texture
    IDirect3DTexture9* m_pGlobalAmbientOcclusionTexture;   // Global ambient occlusion texture

    IDirect3DPixelShader9* m_pScreenSpaceAmbientOcclusionPS;	// Screen space ambient occlusion shader
    IDirect3DPixelShader9* m_pBackProjectDepthToCameraSpacePS; // Back project non linear depth to linear camera space shader
    IDirect3DPixelShader9* m_pCombineLocalAndGlobalOcclusionPS;// Combine the results of local and global occlusion
    IDirect3DPixelShader9* m_pDontCombineLocalAndGlobalOcclusionPS;

    HRESULT ApplyScreenSpaceAmbientOcclusion();

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
// Desc: Initializes the app
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    // Initialize base member variables
    m_bDrawHelp = FALSE;
    m_bDisplaySettings = TRUE;
    m_bApplySSAO = TRUE;
    m_bDebugSSAO = FALSE;
    m_bBlurSSAO = TRUE;
    m_ModeSSAO = SSAO_MODE_ALL;

    // Default values for occlusion radius and falloff radius. These values are in world space.
    // For a scene that has a bigger, i.e. x100 scale, these values would typically be x100
    // larger, but it's best to just tune them with some debug menu item, similar to this demo.
    m_fLocalOcclRadius = 0.0008f;
    m_fLocalFalloffRadius = 0.005f;
    m_fGlobalOcclRadius = 0.00175f;
    m_fGlobalFalloffRadius = 0.012f;

    // Set the scale and translation of each object in the scene
    m_Objects[0].m_matWorld = XMMatrixIdentity();
    m_Objects[1].m_matWorld = XMMatrixScaling( 0.025f, 0.025f, 0.025f ) * XMMatrixTranslation( -0.1f, 0.065f, -0.1f );
    m_Objects[2].m_matWorld = XMMatrixScaling( 0.0075f, 0.0075f, 0.0075f ) * XMMatrixTranslation( -0.1f, 0.002f,
                                                                                                  0.1f );

    // Create the font
    if( FAILED( hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return hr;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return hr;

    if( FAILED( hr = m_PostProcess.Initialize() ) )
        return hr;

    // Create the textures resource
    if( FAILED( hr = m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return hr;

    if( FAILED( hr = m_Scene.Create( "game:\\Media\\Meshes\\Ruins.xbg", &m_Resource ) ) )
        return hr;

    if( FAILED( hr = m_Objects[0].Create( "game:\\Media\\Meshes\\ChessKing.xbg", &m_Resource ) ) )
        return hr;

    if( FAILED( hr = m_Objects[1].Create( "game:\\Media\\Meshes\\Robot.xbg", &m_Resource ) ) )
        return hr;

    if( FAILED( hr = m_Objects[2].Create( "game:\\Media\\Meshes\\SkullOcc.xbg", &m_Resource ) ) )
        return hr;

    // Set the view matrix
    m_vEyePt = XMVectorSet( -0.05f, 0.12f, -0.275f, 0.0f );
    m_vLookatDir = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
    m_vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matView = XMMatrixLookAtLH( m_vEyePt, m_vEyePt + m_vLookatDir, m_vUp );

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the projection matrix
    m_matProj = XMMatrixPerspectiveFovLH( XM_PIDIV4, fAspectRatio, 0.01f, 10.0f );

    // Load shaders
    if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\ShadeScene.xvu", &m_pSceneVertexShader ) ) )
        return hr;

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeScene.xpu", &m_pScenePixelShader ) ) )
        return hr;

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\ScreenSpaceAmbientOcclusion.xpu",
                                           &m_pScreenSpaceAmbientOcclusionPS ) ) )
        return hr;

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\BackProjectDepthToCameraSpace.xpu",
                                           &m_pBackProjectDepthToCameraSpacePS ) ) )
        return hr;

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\CombineLocalAndGlobalOcclusion.xpu",
                                           &m_pCombineLocalAndGlobalOcclusionPS ) ) )
        return hr;

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\DontCombineLocalAndGlobalOcclusion.xpu",
                                           &m_pDontCombineLocalAndGlobalOcclusionPS ) ) )
        return hr;

    // Create textures
    m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                 1, D3DUSAGE_DEPTHSTENCIL, D3DFMT_D24S8,
                                 D3DPOOL_DEFAULT, &m_pDepthBufferTexture, NULL );

    m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth / 2, m_d3dpp.BackBufferHeight / 2,
                                 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F,
                                 D3DPOOL_DEFAULT, &m_pLinearDepthBufferTexture, NULL );

    m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth / 2, m_d3dpp.BackBufferHeight / 2,
                                 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F,
                                 D3DPOOL_DEFAULT, &m_pLocalAmbientOcclusionTexture, NULL );

    m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth / 3, m_d3dpp.BackBufferHeight / 3,
                                 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F,
                                 D3DPOOL_DEFAULT, &m_pGlobalAmbientOcclusionTexture, NULL );

    // Set light properties
    m_vLightDirection = XMVectorSet( 0.604104698f, 0.531634569f, -0.585469127f, 0.0f );
    m_fAmbientColor[0] = 0.10f;
    m_fAmbientColor[1] = 0.07f;
    m_fAmbientColor[2] = 0.04f;
    m_fAmbientColor[3] = 1.0f;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame to update the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    PIXBeginNamedEvent( 0, "Update" );

    FLOAT m_fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
        m_bDisplaySettings = !m_bDisplaySettings;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_bApplySSAO = !m_bApplySSAO;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_bBlurSSAO = !m_bBlurSSAO;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        m_bDebugSSAO = !m_bDebugSSAO;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        m_ModeSSAO = SSAO_MODE( ( m_ModeSSAO + 1 ) % NUM_SSAO_MODES );
    }

    // Adjust the Occlusion Radius and Falloff Radius for Local occlusion
    if( pGamepad->wLastButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
    {
        if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_UP )
        {
            m_fLocalOcclRadius *= powf( 2.0f, +m_fElapsedTime * 0.2f );
        }

        if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        {
            m_fLocalOcclRadius *= powf( 2.0f, -m_fElapsedTime * 0.2f );
        }

        if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        {
            m_fLocalFalloffRadius *= powf( 2.0f, +m_fElapsedTime * 0.2f );
        }

        if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        {
            m_fLocalFalloffRadius *= powf( 2.0f, -m_fElapsedTime * 0.2f );
        }
    }

    // Adjust the Occlusion Radius and Falloff Radius for Global occlusion
    if( pGamepad->wLastButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_UP )
        {
            m_fGlobalOcclRadius *= powf( 2.0f, +m_fElapsedTime * 0.2f );
        }

        if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        {
            m_fGlobalOcclRadius *= powf( 2.0f, -m_fElapsedTime * 0.2f );
        }

        if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        {
            m_fGlobalFalloffRadius *= powf( 2.0f, +m_fElapsedTime * 0.2f );
        }

        if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        {
            m_fGlobalFalloffRadius *= powf( 2.0f, -m_fElapsedTime * 0.2f );
        }
    }

    // Set the view matrix form the left and right sticks
    static FLOAT fTheta = -0.05f * XM_PI;
    static FLOAT fPhi = +0.0f * XM_PI;

    fPhi += pGamepad->fX2 * m_fElapsedTime * 0.3f * XM_PI;
    fTheta += pGamepad->fY2 * m_fElapsedTime * 0.3f * XM_PI;

    m_vLookatDir.x = cosf( fTheta ) * sinf( fPhi );
    m_vLookatDir.y = sinf( fTheta );
    m_vLookatDir.z = cosf( fTheta ) * cosf( fPhi );

    XMVECTOR vCrossDir;
    vCrossDir.x = cosf( fPhi );
    vCrossDir.y = 0.0;
    vCrossDir.z = -sinf( fPhi );

    m_vEyePt += m_vLookatDir * pGamepad->fY1 * m_fElapsedTime * 0.3f;
    m_vEyePt += vCrossDir * pGamepad->fX1 * m_fElapsedTime * 0.3f;
    m_matView = XMMatrixLookAtLH( m_vEyePt, m_vEyePt + m_vLookatDir, m_vUp );

    // Update view * projection matrix
    m_matViewProj = m_matView * m_matProj;

    PIXEndNamedEvent();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderScene()
// Desc: Renders the scene
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderScene()
{
    PIXBeginNamedEvent( 0, "RenderScene" );

    // Set default states
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    m_pd3dDevice->SetVertexShader( m_pSceneVertexShader );
    m_pd3dDevice->SetPixelShader( m_pScenePixelShader );

    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&m_vLightDirection, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&m_fAmbientColor, 1 );

    m_Scene.m_matWorld = XMMatrixIdentity();
    m_Scene.m_matView = m_matView;
    m_Scene.m_matProj = m_matProj;
    m_Scene.Render();

    PIXEndNamedEvent();

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: RenderObjects()
// Desc: Renders the objects in the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderObjects()
{
    PIXBeginNamedEvent( 0, "RenderObjects" );

    for( int i = 0; i < 3; ++i )
    {
        m_Objects[i].m_matView = m_matView;
        m_Objects[i].m_matProj = m_matProj;
        m_Objects[i].Render();
    }

    PIXEndNamedEvent();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame to render the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    PIXBeginNamedEvent( 0, "Render" );

    // Render the full scene
    ATG::RenderBackground( GRADIENT_TOP_COLOR, GRADIENT_BOTTOM_COLOR );

    RenderScene();

    RenderObjects();

    ApplyScreenSpaceAmbientOcclusion();

    // Show information
    PIXBeginNamedEvent( 0, "Info" );
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
        m_Font.DrawText( 0, 0, 0xffffffff, L"Screen Space Ambient Occlusion" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        if( m_bDisplaySettings )
        {
            WCHAR strText[256];

            m_Font.SetScaleFactors( 0.8f, 0.8f );

            swprintf_s( strText, L"%s", m_bApplySSAO ? L"On" : L"Off" );
            m_Font.DrawText( 0, 40, 0xffffffff, L"Ambient Occlusion: " );
            m_Font.DrawText( 220, 40, 0xffffff00, strText );

            swprintf_s( strText, L"%s", m_bDebugSSAO ? L"On" : L"Off" );
            m_Font.DrawText( 0, 60, 0xffffffff, L"Debug Ambient Occlusion: " );
            m_Font.DrawText( 220, 60, 0xffffff00, strText );

            swprintf_s( strText, L"%s", m_bBlurSSAO ? L"On" : L"Off" );
            m_Font.DrawText( 0, 80, 0xffffffff, L"Blur Ambient Occlusion: " );
            m_Font.DrawText( 220, 80, 0xffffff00, strText );

            if( m_ModeSSAO == SSAO_MODE_LOCAL )
                swprintf_s( strText, L"%s", L"Local" );
            else if( m_ModeSSAO == SSAO_MODE_GLOBAL )
                swprintf_s( strText, L"%s", L"Global" );
            else
                swprintf_s( strText, L"%s", L"Local & Global" );
            m_Font.DrawText( 0, 100, 0xffffffff, L"Ambient Occlusion Mode: " );
            m_Font.DrawText( 220, 100, 0xffffff00, strText );

            swprintf_s( strText, L"%f", m_fLocalOcclRadius );
            m_Font.DrawText( 0, 120, 0xffffffff, L"Local Occlusion Radius: " );
            m_Font.DrawText( 220, 120, 0xffffff00, strText );

            swprintf_s( strText, L"%f", m_fLocalFalloffRadius );
            m_Font.DrawText( 0, 140, 0xffffffff, L"Local Falloff Radius: " );
            m_Font.DrawText( 220, 140, 0xffffff00, strText );

            swprintf_s( strText, L"%f", m_fGlobalOcclRadius );
            m_Font.DrawText( 0, 160, 0xffffffff, L"Global Occlusion Radius: " );
            m_Font.DrawText( 220, 160, 0xffffff00, strText );

            swprintf_s( strText, L"%f", m_fGlobalFalloffRadius );
            m_Font.DrawText( 0, 180, 0xffffffff, L"Global Falloff Radius: " );
            m_Font.DrawText( 220, 180, 0xffffff00, strText );
        }

        m_Font.End();
    }

    PIXEndNamedEvent(); // info

    PIXEndNamedEvent(); // render

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame to render the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::ApplyScreenSpaceAmbientOcclusion()
{
    static const float cos45 = cosf( XM_PIDIV4 );

    static const float vOccluderPoints[4 * NUM_OCCLUDERS] =
    {
        cos45,	-cos45,		-cos45,		0,
        -cos45,	-cos45,		-cos45,		0,
        -cos45,	-cos45,		cos45,		0,
        cos45,	-cos45,		cos45,		0,
        cos45,	cos45,		-cos45,		0,
        -cos45,	cos45,		-cos45,		0,
        -cos45,	cos45,		cos45,		0,
        cos45,	cos45,		cos45,		0
    };

    if( !m_bApplySSAO )
    {
        return S_OK;
    }

    PIXBeginNamedEvent( 0, "ApplyScreenSpaceAmbientOcclusion" );

    // Bias the use of general purpose registers towards the pixel shader, since this is
    // post effect and needs very little vertex processing power.
    m_pd3dDevice->SetShaderGPRAllocation( 0, 16, GPU_GPRS - 16 );

    // Adjust fill convention so that pixel centers are at (0.5, 0.5)
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    // Save the current back buffer, and z-buffer so that we can restore them after the
    // post effect has been applied
    IDirect3DSurface9* pBackBufferSurface;
    IDirect3DSurface9* pZBufferSurface;
    m_pd3dDevice->GetRenderTarget( 0, &pBackBufferSurface );
    m_pd3dDevice->GetDepthStencilSurface( &pZBufferSurface );

    // Get the EDRAM offset after the backbuffer render target so we don't overwrite the current backbuffer
    // pixels while caclulating the ambient occlusion
    D3DSURFACE_DESC descBackBuffer, descZBuffer;
    pBackBufferSurface->GetDesc( &descBackBuffer );
    pZBufferSurface->GetDesc( &descZBuffer );
    DWORD dwEdramOffset = XGSurfaceSize( descBackBuffer.Width, descBackBuffer.Height, descBackBuffer.Format,
                                         descBackBuffer.MultiSampleType );

    // Set the projection and inverse projection matrices
    XMMATRIX matProj = XMMatrixTranspose( m_matProj );
    XMVECTOR vDet;
    XMMATRIX matInvProj = XMMatrixInverse( &vDet, m_matProj );
    matInvProj = XMMatrixTranspose( matInvProj );

    ATG::g_pd3dDevice->SetPixelShaderConstantF( 0, ( float* )&matProj, 4 );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( 4, ( float* )&matInvProj, 4 );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( 10, ( float* )&vOccluderPoints, NUM_OCCLUDERS );

    // Resolve depth buffer to a texture
    m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, m_pDepthBufferTexture, NULL, 0, 0, NULL, 1.0f, 0, NULL );

    // CopyTexture() uses LINEAR sampling and we need to use POINT sampling for the 
    // depth buffer, so we set the two depth textures on samplers 1 & 2 and just make
    // sure our shader uses the correct samplers
    m_pd3dDevice->SetTexture( 1, m_pDepthBufferTexture );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTexture( 2, m_pLinearDepthBufferTexture );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    // Back project the depth values in the non-linear depth buffer to linear camera space depth values
    m_PostProcess.CopyTexture( m_pDepthBufferTexture, m_pLinearDepthBufferTexture, m_pBackProjectDepthToCameraSpacePS,
                               dwEdramOffset );

    if( m_ModeSSAO == SSAO_MODE_LOCAL || m_ModeSSAO == SSAO_MODE_ALL )
    {
        // Do one pass of screen space ambient occlusion with a smaller occlusion radius for Local Occlusion
        float vTuneLocalSSAO[4] = { m_fLocalOcclRadius, m_fLocalFalloffRadius, 0, 1 };
        ATG::g_pd3dDevice->SetPixelShaderConstantF( 8, ( float* )&vTuneLocalSSAO, 1 );
        m_PostProcess.CopyTexture( m_pDepthBufferTexture, m_pLocalAmbientOcclusionTexture,
                                   m_pScreenSpaceAmbientOcclusionPS, dwEdramOffset );
    }

    if( m_ModeSSAO == SSAO_MODE_GLOBAL || m_ModeSSAO == SSAO_MODE_ALL )
    {
        // Do another pass of screen space ambient occlusion with a larger occlusion radius for Global Occlusion
        float vTuneGlobalSSAO[4] = { m_fGlobalOcclRadius, m_fGlobalFalloffRadius, 0, 1 };
        ATG::g_pd3dDevice->SetPixelShaderConstantF( 8, ( float* )&vTuneGlobalSSAO, 1 );
        m_PostProcess.CopyTexture( m_pDepthBufferTexture, m_pGlobalAmbientOcclusionTexture,
                                   m_pScreenSpaceAmbientOcclusionPS, dwEdramOffset );
    }

    if( m_bBlurSSAO )
    {
        // Blur the results
        m_PostProcess.GaussBlur5x5Texture( m_pLocalAmbientOcclusionTexture, m_pLocalAmbientOcclusionTexture,
                                           dwEdramOffset );
        m_PostProcess.GaussBlur5x5Texture( m_pGlobalAmbientOcclusionTexture, m_pGlobalAmbientOcclusionTexture,
                                           dwEdramOffset );
    }

    // Restore the main render/depth targets
    m_pd3dDevice->SetRenderTarget( 0, pBackBufferSurface );
    m_pd3dDevice->SetDepthStencilSurface( pZBufferSurface );
    pBackBufferSurface->Release();
    pZBufferSurface->Release();

    if( !m_bDebugSSAO )
    {
        // Blend the ambient occlusion onto the current back buffer.
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ZERO );
        m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR );
    }

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    switch( m_ModeSSAO )
    {
        case SSAO_MODE_LOCAL:
            m_pd3dDevice->SetPixelShader( m_pDontCombineLocalAndGlobalOcclusionPS );
            m_pd3dDevice->SetTexture( 0, m_pLocalAmbientOcclusionTexture );
            break;

        case SSAO_MODE_GLOBAL:
            m_pd3dDevice->SetPixelShader( m_pDontCombineLocalAndGlobalOcclusionPS );
            m_pd3dDevice->SetTexture( 0, m_pGlobalAmbientOcclusionTexture );
            break;

        case SSAO_MODE_ALL:
            m_pd3dDevice->SetPixelShader( m_pCombineLocalAndGlobalOcclusionPS );
            m_pd3dDevice->SetTexture( 0, m_pLocalAmbientOcclusionTexture );
            m_pd3dDevice->SetTexture( 1, m_pGlobalAmbientOcclusionTexture );
            break;

    }

    // Copy the ambient occlusion onto the backbuffer
    D3DVIEWPORT9 vp;
    m_pd3dDevice->GetViewport( &vp );
    m_PostProcess.DrawScreenSpaceQuad( ( FLOAT )vp.Width, ( FLOAT )vp.Height, 1.0f, 1.0f );

    // Reset
    m_pd3dDevice->SetPixelShader( NULL );
    m_pd3dDevice->SetShaderGPRAllocation( 0, 0, 0 );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    PIXEndNamedEvent();

    return S_OK;
}
