//--------------------------------------------------------------------------------------
// FXLWater.cpp
//
// Sample application which illustrates a technique for rendering water using the
// per-edge tessellation capabilities of the Xbox 360 via FXLite.
// 
// Water Level of detail is maintained by projecting screen space coordinates onto the
// water plane.  To avoid artifacts on along the horizon, the tessellator is employed
// to add per-vertex detail where needed.
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <fxl.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include "camera.h"
#include "water.h"
#include "dolphin.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_2,
        L"Toggle water wireframe\n(not fully tessellated for clarity)" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_2, L"Pause water\nphysics" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_1, L"Move camera" },
    { ATG::HELP_RIGHT_STICK,    ATG::HELP_PLACEMENT_1, L"Aim camera" },
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_1, L"Toggle help"  },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::PackedResource m_Resource;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    // Skybox components
    ATG::Mesh m_SkyboxMesh;
    FXLEffect* m_pFXLSkybox;
    LPDIRECT3DCUBETEXTURE9 m_pEnvMap;

    // Dolphin control
    DolphinSchool* m_pDolphinSchool;
    Dolphin* m_pDolphin1;
    Dolphin* m_pDolphin2;

    // FXLite parameter pools.
    // Effects created using these pools will be able to inherit
    // parameter values.
    FXLEffectPool* m_pFXLPool;
    FXLHANDLE m_hView;
    FXLHANDLE m_hProj;
    FXLHANDLE m_hViewProj;

    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    // Camera object for view navigation
    Camera* m_pCamera;

    // The water surface object    
    Water* m_pWater;

    void            RenderAllText();

public:
                    Sample();
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
    FXL__EnforceSharedCorrelation = TRUE;

    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth,
                           &atgApp.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Sample()
// Desc: Sample constructor
//--------------------------------------------------------------------------------------
Sample::Sample() : m_pFXLPool( NULL ),
                   m_pFXLSkybox( NULL ),
                   m_pEnvMap( NULL ),
                   m_pDolphin1( NULL ),
                   m_pDolphin2( NULL ),
                   m_pDolphinSchool( NULL ),
                   m_pWater( NULL ),
                   m_pCamera( NULL )

{

}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    m_bDrawHelp = FALSE;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG_PrintError( "Could not create font\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG_PrintError( "Could not create help resource\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    // Create the textures resource
    if( FAILED( m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG_PrintError( "Could not create Resource.xpr\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    // Create the environment map
    m_pEnvMap = m_Resource.GetCubemap( "EnvMap" );

    // Create the skybox mesh
    if( FAILED( hr = m_SkyboxMesh.Create( "game:\\Media\\Meshes\\Skybox01.xbg" ) ) )
    {
        ATG_PrintError( "Could not create skybox mesh\n" );
        return hr;
    }

    // Create an FXLite Effect Parameter Pool
    // Effects created using the same pool can inherit shared
    // parameters, such as the view and projection matrices, etc.
    if( FAILED( hr = FXLCreateEffectPool( &m_pFXLPool ) ) )
    {
        ATG_PrintError( "Could not create FXL Effect Pool\n" );
        return hr;
    }

    // Create the Skybox FXLite effect
    // The effect is precompiled on the PC-side into a binary file.
    // The binary file is then loaded, and instantiated via FXLCreateEffect
    VOID* pCode;
    DWORD dwSize;
    hr = ATG::LoadFile( "game:\\Media\\Effects\\skybox.fxobj", &pCode, &dwSize );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not load FXL Effect (skybox)\n" );
        return hr;
    }

    hr = FXLCreateEffect( m_pd3dDevice, pCode, m_pFXLPool, &m_pFXLSkybox );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Could not create FXL Effect (skybox)\n" );
        return hr;
    }

    ATG::UnloadFile( pCode );

    // Shared parameter handles can now be retrieved.
    // At least one effect that defines the parameter must be loaded prior to
    // retrieving a handle.
    m_hView = m_pFXLPool->GetParameterHandle( "matView" );
    m_hProj = m_pFXLPool->GetParameterHandle( "matProj" );
    m_hViewProj = m_pFXLPool->GetParameterHandle( "matViewProj" );

    FXLHANDLE hLightDirection = m_pFXLPool->GetParameterHandle( "lightDirection" );
    FXLHANDLE hI_a = m_pFXLPool->GetParameterHandle( "I_a" );
    FXLHANDLE hI_d = m_pFXLPool->GetParameterHandle( "I_d" );
    FXLHANDLE hI_s = m_pFXLPool->GetParameterHandle( "I_s" );
    FXLHANDLE hFXLSkyboxSampler = m_pFXLPool->GetParameterHandle( "envmap_sampler" );

    // Set static shared parameter values - e.g., lighting, etc
    XMVECTOR vLightDirection = XMVectorSet( 0.f, 0.f, -1.f, 0.f );
    vLightDirection = XMVector4Transform( vLightDirection,
                                          XMMatrixRotationX( -25.f / 180.f * D3DX_PI ) );
    vLightDirection = XMVector4Transform( vLightDirection,
                                          XMMatrixRotationY( -60.f / 180.f * D3DX_PI ) );

    const D3DXVECTOR4 vecA( 0.5f, 0.5f, 0.5f, 1.f );
    const D3DXVECTOR4 vecD( 0.5f, 0.5f, 0.5f, 1.f );
    const D3DXVECTOR4 vecS( 1.0f, 1.0f, 1.0f, 1.f );

    m_pFXLPool->SetVectorF( hLightDirection, ( FLOAT* )&vLightDirection );
    m_pFXLPool->SetVectorF( hI_a, ( FLOAT* )&vecA );
    m_pFXLPool->SetVectorF( hI_d, ( FLOAT* )&vecD );
    m_pFXLPool->SetVectorF( hI_s, ( FLOAT* )&vecS );
    m_pFXLPool->SetSampler( hFXLSkyboxSampler, m_pEnvMap );


    // Send the projection matrix to the FXLite parameter pool
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth
        / ( FLOAT )m_d3dpp.BackBufferHeight;

    m_matProj = XMMatrixPerspectiveFovLH( D3DX_PI / 4, fAspectRatio, 1.0f, 500.f );
    m_matView = XMMatrixIdentity();
    m_pFXLPool->SetMatrix( m_hProj, ( FXLMATRIX* )&m_matProj );

    // Create an camera 4 ft above the water line
    XMVECTOR vInitialPosition = { 0.f, 4.f, 0.f, 1.f };
    m_pCamera = Camera::CreateFirstPerson( vInitialPosition, 0.f, +.25f, 8.f );

    // Initialize the water surface
    m_pWater = Water::Create( m_pCamera, m_pd3dDevice, m_pFXLPool );
    m_pWater->SetProjectionMatrix( m_matProj );

    // The scene has two dolphins that always seek the camera focal point,
    // Then dither about.
    XMVECTOR v = XMVectorSet( 0.f, -1.f, 10.f, 0.f );
    m_pDolphinSchool = DolphinSchool::Create( m_pd3dDevice, m_pFXLPool, &m_Resource );
    m_pDolphin1 = m_pDolphinSchool->AddDolphin( v, 0.f, 0.f );
    m_pDolphin1->SetTargetPosition( v );

    v = XMVectorSet( 0.f, -2.5f, 10.f, 0.f );
    m_pDolphin2 = m_pDolphinSchool->AddDolphin( v, 0.f, 0.f );
    m_pDolphin1->SetTargetPosition( v );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    static bool bPauseSimulation = false;
    static bool bWireframe = false;

    // Get time
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Toggle wireframe
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_pWater->SetWireframeEnable( bWireframe = !bWireframe );
    }

    // Pause Water Simulation
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_pWater->SetSimulationPause( bPauseSimulation = !bPauseSimulation );
    }

    // Toggle motion
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        static BOOL bPaused = FALSE;
        bPaused = !bPaused;

        if( bPaused ) m_Timer.Stop();
        else
            m_Timer.Start();
    }

    // Update the camera position
    m_pCamera->Update( fElapsedTime );
    m_pCamera->GetViewMatrix( m_matView );

    // Update the concatenated view*projection matrix
    XMMATRIX matViewProj = m_matView * m_matProj;

    m_pFXLPool->SetMatrix( m_hView, ( FXLMATRIX* )&m_matView );
    m_pFXLPool->SetMatrix( m_hProj, ( FXLMATRIX* )&m_matProj );
    m_pFXLPool->SetMatrix( m_hViewProj, ( FXLMATRIX* )&matViewProj );

    // Allow the water simulation to update
    m_pWater->Update( fElapsedTime );

    // Animate the dolphins.
    // For an interesting effect, we determine the intersection of the camera focus
    // ({0.f,0.f} in clip space), and command the dolphins to that point.
    // After reaching this point, they dither about by constraining their movements
    // the closer they are to the target.

    // Clip space origin
    XMVECTOR v = { 0.f, 0.f, 0.f, 1.f };
    XMVECTOR p;

    XMVECTOR det;
    XMMATRIX matWVPinv = XMMatrixInverse( &det, matViewProj );


    // matWVPinv._32 could quite possibly be 0.f - that would mean the camera is level
    // with the horizon!
    v.z = -1.f;
    if( matWVPinv._32 != 0.f )
    {
        // Gives the z-value required to intersect the y=0 plane
        v.z = - ( matWVPinv._12 * v.x + matWVPinv._22 * v.y
                  + matWVPinv._42 /*- Y_water_plane*/  ) / matWVPinv._32;
    }

    // Transform the intersection from clip space back into world-space
    p = XMVector3TransformCoord( v, matWVPinv );

    // Avoid sending the dolphins too far away form the camera
    // We could use the z value - but it's inexpensive to use absolute feet.
    XMVECTOR vCameraPosition;
    m_pCamera->GetPosition( vCameraPosition );

    // If the target position is ahead of the camera and nearby, order the dolphin
    // To the updated position
    if( v.z > 0.f && XMVector3Length( p - vCameraPosition ).x < 100.f )
    {
        m_pDolphin1->SetTargetPosition( p );
    }

    // Command the second dolphin to seek out the first (slighly offset)
    m_pDolphin1->GetPosition( p );
    p += XMVectorSet( 1.f, 0.f, 1.f, 0.f );
    m_pDolphin2->SetTargetPosition( p );

    // Allow the dolphins to update their positions, etc.
    m_pDolphinSchool->Update( fElapsedTime );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderAllText()
// Desc: Render help, text etc.  Any code that could potentially overwrite global
//       shared effect parameters has been placed in this block.
//       ATG::Font will dirty s0, as well as c1-c2.
//       ATG::Help will dirty s0.
//--------------------------------------------------------------------------------------
void Sample::RenderAllText()
{
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
        m_Font.DrawText( 0, 0, 0xffffffff, L"FXLWater" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering. This
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    FXLTECHNIQUE_DESC fxl_desc;
    FXLHANDLE hTechnique;

    // Begin the refraction pass
    // During this pass, we render the refraction of the underwater objects into a
    // texture as if the water was perfectly flat.  Later, we will permute the texture
    // coordinates when rendering the water surface, moving the refractions about.
    // Objects rendered during this pass are expected to clip all geometry above the
    // water-line, and blend the water color based on depth.
    m_pWater->BeginRefraction();
    m_pDolphinSchool->Render( DolphinSchool::PASS_REFRACTION );
    m_pWater->EndRefraction();

    // Begin the reflection pass
    // During this pass, we render the local reflections of everything above the
    // water surface (not including the skybox).  This is done assuming the water
    // Objects rendered during this pass are expected to clip all geometry below
    // the water-line, and set the alpha channel to 1.f.  The alpha channel is used
    // later to mask the skybox reflection in regions a local reflection is present.
    m_pWater->BeginReflection();
    m_pDolphinSchool->Render( DolphinSchool::PASS_REFLECTION );
    m_pWater->EndReflection();

    // Now that all required textures have been computed - we may render the scene
    m_pd3dDevice->Clear( 0L,
                         NULL,
                         D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x000000ff,
                         1.0f,
                         0L );

    // Begin with the skybox.
    // Set effect parameters
    hTechnique = m_pFXLSkybox->GetTechniqueHandleFromIndex( 0 );

    m_pFXLSkybox->GetTechniqueDesc( hTechnique, &fxl_desc );
    m_pFXLSkybox->BeginTechnique( hTechnique, FXL_RESTORE_DEFAULT_RENDER_STATE );

    // Effects can contain multiple passes - render each one
    for( UINT i = 0; i < fxl_desc.Passes; ++i )
    {
        m_pFXLSkybox->BeginPassFromIndex( i );
        m_pFXLSkybox->Commit();
        m_SkyboxMesh.Render( 0L );
        m_pFXLSkybox->EndPass();
    }
    m_pFXLSkybox->EndTechnique();

    // Render the water surface
    m_pWater->Render();

    // Render the non-reflected, non-refracted dolphin portion(s)
    m_pDolphinSchool->Render( DolphinSchool::PASS_NORMAL );

    //       Effects Lite will only reset global shared parameters when their value changes,
    //       so if one is overwritten by external code, the next Effects Lite effect to use
    //       it will use an incorrect value.  There are 3 strategies for resolving such
    //       situations:
    //           1.  Avoid the conflict (do not maintain global shared effect parameters
    //       in locations you know may be overwritten)
    //           2.  Set the internally maintained dirty bits for the parameter in the
    //       effect pool when leaving rendering code that may have dirtied them.  This
    //       will cause Effects Lite to re-set the parameter values.
    //           3.  Save the values of parameters that may be dirtied, and restore such
    //       values when safe to do so.
    //
    //       This sample demonstrates technique #3.

    // Save s0, c1, c2
    LPDIRECT3DBASETEXTURE9 pS0 = NULL;
    FLOAT c1[8];
    m_pd3dDevice->GetTexture( 0, &pS0 );
    m_pd3dDevice->GetVertexShaderConstantF( 1, c1, 2 );

    // Potentially dirties s0, c1, c2
    RenderAllText();

    // Restore s0, c1, c2
    m_pd3dDevice->SetTexture( 0, pS0 );
    m_pd3dDevice->SetVertexShaderConstantF( 1, c1, 2 );
    if( pS0 != NULL )
        pS0->Release();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

