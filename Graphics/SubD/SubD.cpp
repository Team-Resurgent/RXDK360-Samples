//--------------------------------------------------------------------------------------
// SubD.cpp
//
// Demonstrates real-time evaluation and rendering of Catmull-Clark subdivision surfaces 
// using a Bezier patch approximation.
//
// This sample is intended to demonstrate a higher-order surface representation running
// in realtime on Xbox 360.  It is not feature-complete at this point, but all of the
// essential components have been implemented.  There are two important algorithms in
// this sample: basis conversion and patch evaluation.  The basis conversion algorithm
// converts a Catmull-Clark quad mesh with adjacency information into patches.  The
// patch evaluation algorithm, in concert with the GPU's tessellation unit, evaluates
// the Bezier patches and generates triangles with normal/tangent information.
//
// Note: Since the original release of this sample, the quad adjacency computation
// algorithm has been removed and placed within the Samples Content Exporter, which
// is another sample app distributed with the XDK.  Please refer to ExportSubD.cpp
// within that project to see the adjacency computation code.  The Samples Content
// Exporter generates specially formatted XATG scene files that contain the adjacency
// information within vertex and index buffers; use the -subdxbox command line option
// to generate these special XATG files.
//
// Performance notes:
//
// - Overall, this approach to rendering geometry is less efficient than rendering
//   equivalent pre-tessellated triangle meshes.  In theory, Bezier patch evaluation
//   may be a more desirable choice given a different set of performance constraints.
//   For example, a more complex deformation implementation (muscle-based skinning,
//   cloth systems, morph targets, etc) may change the balance of performance versus
//   functionality.
//
// - The basis conversion algorithm runs on the CPU.  With significant architectural
//   changes, it could be made to run on the GPU.  The algorithm is robust, but its
//   current performance should not be seen as representative.
//
// - The patch evaluation algorithm is part of a tessellator vertex shader, and runs
//   on the GPU.  This algorithm is reasonably optimal, and is fetch bound.  However,
//   it is not feature complete; it is missing displacement mapping.
//
// - The sample runs at 720p, 4x MSAA with 3 predicated tiles.  This decision was made
//   to make the sample look better, and also accentuate the performance impact of the
//   patch evaluation vertex shader.
//
// Special thanks goes to Charles Loop and Peter-Pike Sloan at Microsoft Research, for
// their ongoing research and implementation assistance.  Special thanks to Scott 
// Schaefer at Texas A&M University for his original research with Charles Loop.
//
// References:
// Charles Loop and Scott Schaefer. "Approximating Catmull-Clark Subdivision Surfaces 
// with Bicubic Patches". Microsoft Research Technical Report, MSR-TR-2007-44. 2007
// http://research.microsoft.com/~cloop/msrtr-2007-44.pdf
//
// Microsoft XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgApp.h>
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>
#include <AtgSceneAll.h>

#include "SubDMesh.h"

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_1, L"Toggle wireframe" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_1, L"Toggle pixel shader" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_1, L"Rotate light" },
    { ATG::HELP_RIGHT_STICK,    ATG::HELP_PLACEMENT_1, L"Rotate camera" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_2, L"Increase/decrease\ntessellation factor" },
    { ATG::HELP_MISC_CALLOUT_2, ATG::HELP_PLACEMENT_1, L"Triggers zoom in/out" },
};
#define NUM_HELP_CALLOUTS (ARRAYSIZE(g_HelpCallouts))

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: The subdivision surface sample class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();

private:
    VOID    RenderUI();
    VOID    SetupMaterial( MaterialInstance* pMaterialInstance );

private:
    // Sample framework objects
    ATG::Font m_Font;
    ATG::Timer m_Timer;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    FLOAT m_fDeltaTime;

    // Render targets
    D3DSurface* m_pColorTarget1X;
    D3DSurface* m_pColorTarget4X;
    D3DSurface* m_pDepthStencil4X;

    // Resolve texture
    D3DTexture* m_pResolveBuffer;

    // Front buffer
    D3DTexture* m_pFrontBuffer;

    // Shaders
    D3DVertexShader* m_pQuadPatchVS;
    D3DPixelShader* m_pQuadPatchPS;
    D3DPixelShader* m_pQuadPatchSimplePS;
    D3DVertexDeclaration* m_pQuadPatchDecl;
    BOOL m_bUseSimplePixelShader;

    // Mesh and scene
    std::vector<ATG::SkeletonInstance*> m_SkeletonInstances;
    SubDMeshVector m_SubDMeshes;
    PolyMeshVector m_PolyMeshes;
    FLOAT m_fTessellationFactor;
    BOOL m_bWireframe;
    FLOAT m_fTargetCenterYPos;

    // Camera and lighting
    XMMATRIX m_matProj;
    XMMATRIX m_matViewProj;
    XMVECTOR m_vCameraPosWorld;
    XMVECTOR m_vDirectionalLightDirection;
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the sample.
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample SubDSample;

    // Set up for 720p, manual buffer creation, and predicated tiling.
    D3DPRESENT_PARAMETERS& d3dpp = SubDSample.m_d3dpp;
    ZeroMemory( &d3dpp, sizeof( D3DPRESENT_PARAMETERS ) );
    d3dpp.BackBufferWidth = 1280;
    d3dpp.BackBufferHeight = 720;
    d3dpp.BackBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dpp.MultiSampleQuality = 0;
    d3dpp.BackBufferCount = 0;
    d3dpp.EnableAutoDepthStencil = FALSE;
    d3dpp.DisableAutoBackBuffer = TRUE;
    d3dpp.DisableAutoFrontBuffer = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    SubDSample.m_dwDeviceCreationFlags |= D3DCREATE_BUFFER_2_FRAMES;

    SubDSample.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the objects for the sample, and loads various resources.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize member variables
    m_bDrawHelp = FALSE;
    m_fTessellationFactor = 3.0f;
    m_bWireframe = FALSE;
    m_bUseSimplePixelShader = FALSE;

    // Create font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        ATG::FatalError( "Could not load font." );

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create help screen
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        ATG::FatalError( "Could not load help resources." );

    // Initialize simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );

    m_matProj = XMMatrixPerspectiveFovLH( XM_PIDIV4, 16.0f / 9.0f, 0.01f, 300.0f );

    // Set up surface parameters for creating the rendertargets
    D3DSURFACE_PARAMETERS ColorSurfParams;
    ColorSurfParams.Base = 0;
    ColorSurfParams.ColorExpBias = 0;
    ColorSurfParams.HierarchicalZBase = 0;
    ColorSurfParams.HiZFunc = D3DHIZFUNC_DEFAULT;

    D3DSURFACE_PARAMETERS DepthSurfParams;
    DepthSurfParams.Base = 1024;
    DepthSurfParams.ColorExpBias = 0;
    DepthSurfParams.HierarchicalZBase = 0;
    DepthSurfParams.HiZFunc = D3DHIZFUNC_DEFAULT;

    // Create 1280x720 1xMSAA surfaces for UI rendering
    HRESULT hr = m_pd3dDevice->CreateRenderTarget( 1280, 720,
                                                   D3DFMT_A8R8G8B8,
                                                   D3DMULTISAMPLE_NONE,
                                                   0, FALSE,
                                                   &m_pColorTarget1X,
                                                   &ColorSurfParams );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't create color rendertarget." );

    // Create 1280x256 4xMSAA surfaces for scene rendering
    hr = m_pd3dDevice->CreateRenderTarget( 1280, 256,
                                           ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
                                           D3DMULTISAMPLE_4_SAMPLES,
                                           0, FALSE,
                                           &m_pColorTarget4X,
                                           &ColorSurfParams );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't create color rendertarget." );

    hr = m_pd3dDevice->CreateRenderTarget( 1280, 256,
                                           D3DFMT_D24S8,
                                           D3DMULTISAMPLE_4_SAMPLES,
                                           0, FALSE,
                                           &m_pDepthStencil4X,
                                           &DepthSurfParams );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't create depth/stencil rendertarget." );

    // Create 1280x720 front buffer texture and resolve buffer texture
    hr = m_pd3dDevice->CreateTexture( 1280, 720, 1, 0, D3DFMT_LE_X8R8G8B8, D3DPOOL_DEFAULT, &m_pFrontBuffer, NULL );
    hr = m_pd3dDevice->CreateTexture( 1280, 720, 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_X8R8G8B8 ),
                                      D3DPOOL_DEFAULT, &m_pResolveBuffer, NULL );

    // Load quad mesh from file
    ATG::SkeletonInstance* pSkelInst = NULL;
    SubDMesh::LoadFromXATG( m_pd3dDevice, "game:\\media\\scenes\\animatedhead.xatg", L"Root", m_SubDMeshes, m_PolyMeshes, &pSkelInst );
    if( pSkelInst != NULL )
    {
        if( pSkelInst->m_pActiveAnimation != NULL )
        {
            pSkelInst->m_pActiveAnimation->m_fPlaybackSpeed = 1.0f;
        }
        m_SkeletonInstances.push_back( pSkelInst );
    }

    // Compute the center Y pos of the scene
    FLOAT fMaxYPos = FLT_MIN;
    FLOAT fMinYPos = FLT_MAX;
    for( DWORD i = 0; i < m_SubDMeshes.size(); ++i )
    {
        const SubDMesh* pMesh = m_SubDMeshes[i];
        XMVECTOR vCenter = pMesh->GetMeshCenter();
        FLOAT fYMin = vCenter.y - pMesh->GetMeshRadius();
        FLOAT fYMax = vCenter.y + pMesh->GetMeshRadius();
        fMaxYPos = max( fMaxYPos, fYMax );
        fMinYPos = min( fMinYPos, fYMin );
    }
    m_fTargetCenterYPos = ( fMaxYPos + fMinYPos ) * 0.5f;

    // Load the shaders for patch rendering
    ATG::LoadVertexShader( "game:\\media\\shaders\\QuadPatchVS.xvu", &m_pQuadPatchVS );
    ATG::LoadPixelShader( "game:\\media\\shaders\\QuadPatchPS.xpu", &m_pQuadPatchPS );
    ATG::LoadPixelShader( "game:\\media\\shaders\\QuadPatchSimplePS.xpu", &m_pQuadPatchSimplePS );
    m_pd3dDevice->CreateVertexDeclaration( PatchVertexElements, &m_pQuadPatchDecl );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Gets controller input and updates the camera.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Update FPS in the timer class.
    m_Timer.MarkFrame();

    // Get frame delta time.
    m_fDeltaTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get input from controllers (check for exit button sequence).
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_bWireframe = !m_bWireframe;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        m_bUseSimplePixelShader = !m_bUseSimplePixelShader;

    if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        m_fTessellationFactor -= m_fDeltaTime * 1.0f;
    else if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_UP )
        m_fTessellationFactor += m_fDeltaTime * 1.0f;
    m_fTessellationFactor = min( 15.0f, max( 1.0f, m_fTessellationFactor ) );

    // Initial values for camera and light.
    static FLOAT s_fRotateY = -0.63f;
    static FLOAT s_fRotateX = 0.181f;
    static FLOAT s_fCameraDistance = 40.0f;
    static FLOAT s_fLightAngle = -0.6f;

    // Update camera position.
    const FLOAT fRotateSpeed = XM_PIDIV2;
    const FLOAT fZoomSpeed = 10.0f;
    FLOAT fRotateYAmount = pGamepad->fX2 * fRotateSpeed * m_fDeltaTime;
    FLOAT fRotateXAmount = pGamepad->fY2 * fRotateSpeed * m_fDeltaTime;
    FLOAT fDistance = ( ( FLOAT )pGamepad->bLeftTrigger / 255.0f ) * fZoomSpeed * m_fDeltaTime -
        ( ( FLOAT )pGamepad->bRightTrigger / 255.0f ) * fZoomSpeed * m_fDeltaTime;
    s_fRotateX += fRotateXAmount;
    s_fRotateY += fRotateYAmount;
    s_fCameraDistance += fDistance;
    s_fRotateX = max( s_fRotateX, -XM_PIDIV2 );
    s_fRotateX = min( s_fRotateX, XM_PIDIV2 );
    s_fCameraDistance = max( s_fCameraDistance, 0.2f );
    s_fCameraDistance = min( s_fCameraDistance, 100.0f );
    static const XMVECTOR vAxisX = { 1, 0, 0, 0 };
    static const XMVECTOR vAxisY = { 0, 1, 0, 0 };
    XMVECTOR qRotateX = XMQuaternionRotationAxis( vAxisX, s_fRotateX );
    XMVECTOR qRotateY = XMQuaternionRotationAxis( vAxisY, s_fRotateY );
    XMVECTOR qRotation = XMQuaternionMultiply( qRotateX, qRotateY );
    XMMATRIX matRotation = XMMatrixRotationQuaternion( qRotation );
    XMMATRIX matWorld = XMMatrixTranslation( 0, 0, -s_fCameraDistance );
    matWorld *= matRotation;
    matWorld *= XMMatrixTranslation( 0, m_fTargetCenterYPos, 0 );

    m_vCameraPosWorld = matWorld.r[3];

    XMVECTOR vDummy;
    XMMATRIX matView = XMMatrixInverse( &vDummy, matWorld );
    m_matViewProj = matView * m_matProj;

    // Update light direction.
    const FLOAT fLightRotateSpeed = XM_PIDIV2;
    fRotateYAmount = pGamepad->fX1 * fLightRotateSpeed * m_fDeltaTime;
    s_fLightAngle += fRotateYAmount;
    m_vDirectionalLightDirection.y = 0.5f;
    m_vDirectionalLightDirection.x = 0.866f * cosf( s_fLightAngle );
    m_vDirectionalLightDirection.z = 0.866f * sinf( s_fLightAngle );
    m_vDirectionalLightDirection = XMVector3Normalize( m_vDirectionalLightDirection );

    for( DWORD i = 0; i < m_SkeletonInstances.size(); ++i )
    {
        ATG::SkeletonInstance* pSkelInst = m_SkeletonInstances[i];
        pSkelInst->UpdateAnimation( m_fDeltaTime );
        pSkelInst->BuildWorldPose();
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene and UI.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    ATG::DebugDraw::SetViewProjection( m_matViewProj );

    m_pd3dDevice->BeginScene();

    // Set up 4x MSAA render targets for tiling.
    m_pd3dDevice->SetRenderTarget( 0, m_pColorTarget4X );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencil4X );

    const D3DRECT pTilingRects[] =
    {
        { 0,   0, 1280, 256 },
        { 0, 256, 1280, 512 },
        { 0, 512, 1280, 720 }
    };

    // Perform basis conversion on the SubD quad mesh, creating one ACC2 patch for each quad.
    DWORD dwSubDMeshCount = m_SubDMeshes.size();
    for( DWORD i = 0; i < dwSubDMeshCount; ++i )
    {
        m_SubDMeshes[i]->GenPatches();
    }

    // Begin tiling.
    D3DVECTOR4 ClearColor = { 0, 0, 0, 0 };
    m_pd3dDevice->BeginTiling( 0, 3, pTilingRects, &ClearColor, 1.0f, 0 );

    // Set default renderstate.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, m_bWireframe ? D3DFILL_WIREFRAME : D3DFILL_SOLID );
    m_pd3dDevice->SetRenderState( D3DRS_LINEWIDTH, ATG::FtoDW( 1.0f ) );

    // Set shaders and decl for rendering patches.
    m_pd3dDevice->SetVertexDeclaration( m_pQuadPatchDecl );
    m_pd3dDevice->SetVertexShader( m_pQuadPatchVS );
    if( m_bUseSimplePixelShader )
    {
        m_pd3dDevice->SetPixelShader( m_pQuadPatchSimplePS );
    }
    else
    {
        m_pd3dDevice->SetPixelShader( m_pQuadPatchPS );
    }

    for( DWORD i = 0; i < dwSubDMeshCount; ++i )
    {
        SubDMesh* pSubDMesh = m_SubDMeshes[i];
        D3DVertexBuffer* pVB = pSubDMesh->GetACC2PatchVB();

        // Set up vertex stream.
        // ACC2 patches go into stream 0, which include texture coordinates and skinned tangent space vectors.
        m_pd3dDevice->SetStreamSource( 0, pVB, 0, sizeof( ACC2PATCH ) );
        m_pd3dDevice->SetIndices( NULL );

        // Set up shader constants for camera and light.
        XMMATRIX matWorld = pSubDMesh->GetWorldTransform();
        XMMATRIX matWVP = matWorld * m_matViewProj;
        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matWorld, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&m_vCameraPosWorld, 1 );
        m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&m_vDirectionalLightDirection, 1 );

        // Set up the GPU's tessellator.
        m_pd3dDevice->SetRenderState( D3DRS_TESSELLATIONMODE, D3DTM_CONTINUOUS );
        m_pd3dDevice->SetRenderState( D3DRS_MAXTESSELLATIONLEVEL, ATG::FtoDW( m_fTessellationFactor ) );

        Model* pModel = pSubDMesh->GetModel();
        assert( pModel->GetNumMeshMappings() == 1 );
        MeshMapping& MeshMap = pModel->GetMeshMapping( 0 );
        StaticMesh* pMesh = (StaticMesh*)MeshMap.pMesh;
        for( DWORD dwSubset = 0; dwSubset < pMesh->GetNumSubsets(); ++dwSubset )
        {
            // Set up material parameters for this subset.
            SetupMaterial( MeshMap.Materials[dwSubset] );

            const SubsetDesc* pCurrentSubset = pMesh->GetSubsetDesc( dwSubset );
            // Draw the quad patches using the tessellator.
            m_pd3dDevice->DrawTessellatedPrimitive( D3DTPT_QUADPATCH, pCurrentSubset->GetStartIndex(), pCurrentSubset->GetNumPrimitives() );
        }

        m_pd3dDevice->SetStreamSource( 0, NULL, 0, 0 );
        m_pd3dDevice->SetStreamSource( 1, NULL, 0, 0 );
        m_pd3dDevice->SetStreamSource( 2, NULL, 0, 0 );
    }

    // Render ground plane.
    const FLOAT fGridSize = 40.0f;
    const DWORD dwGridMinorSteps = ( DWORD )( fGridSize * 2 );
    m_pd3dDevice->SetRenderState( D3DRS_LINEWIDTH, ATG::FtoDW( 1.0f ) );
    ATG::DebugDraw::DrawGrid( XMFLOAT3( fGridSize, 0, 0 ), XMFLOAT3( 0, 0, fGridSize ), XMFLOAT3( 0, 0, 0 ),
                              dwGridMinorSteps, dwGridMinorSteps, 0xFF404080 );

    // End tiling.
    m_pd3dDevice->EndTiling( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARRENDERTARGET, NULL,
                             m_pResolveBuffer, &ClearColor, 1.0f, 0, NULL );

    // Switch to a full-screen rendertarget.
    m_pd3dDevice->SetRenderTarget( 0, m_pColorTarget1X );
    m_pd3dDevice->SetDepthStencilSurface( NULL );

    // Draw resolve buffer back to the screen.
    D3DRECT ScreenSpaceRect = { 0, 0, 1280, 720 };
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( ScreenSpaceRect, m_pResolveBuffer );

    // Render UI.
    RenderUI();

    m_pd3dDevice->EndScene();

    // Sync to present interval, resolve, and swap.
    m_pd3dDevice->SynchronizeToPresentationInterval();
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pFrontBuffer, NULL, 0, 0, NULL, 1.0, 0, NULL );
    m_pd3dDevice->Swap( m_pFrontBuffer, NULL );

    return S_OK;
}


VOID Sample::SetupMaterial( MaterialInstance* pMaterialInstance )
{
    // Load textures into D3D.
    ATG::Texture* pTextureResource = NULL;
    switch( pMaterialInstance->GetRawParameterCount() )
    {
    case 3:
        pTextureResource = ( ATG::Texture* )pMaterialInstance->GetRawParameter( 2 ).pValue;
        if( pTextureResource != NULL )
            m_pd3dDevice->SetTexture( 2, pTextureResource->GetD3DTexture() );
    case 2:
        pTextureResource = ( ATG::Texture* )pMaterialInstance->GetRawParameter( 1 ).pValue;
        if( pTextureResource != NULL )
            m_pd3dDevice->SetTexture( 1, pTextureResource->GetD3DTexture() );
    case 1:
        pTextureResource = ( ATG::Texture* )pMaterialInstance->GetRawParameter( 0 ).pValue;
        if( pTextureResource != NULL )
            m_pd3dDevice->SetTexture( 0, pTextureResource->GetD3DTexture() );
    default:
        break;
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderUI()
// Desc: Draws some statistics and other UI elements
//--------------------------------------------------------------------------------------
VOID Sample::RenderUI()
{
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Draw a title and FPS indicator.
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"SubD" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        WCHAR strText[100];
        swprintf_s( strText, L"Tessellation Factor: %0.1f", m_fTessellationFactor );
        m_Font.SetScaleFactors( 0.9f, 0.9f );
        m_Font.DrawText( 0, 34, 0xFFFFFFFF, strText );

        DWORD dwPatchCount = 0;
        for( DWORD i = 0; i < m_SubDMeshes.size(); ++i )
        {
            dwPatchCount += m_SubDMeshes[i]->GetNumPatches();
        }
        DWORD dwTessFactor = ( DWORD )ceilf( m_fTessellationFactor * 0.5f - 0.5f ) * 2 + 2;
        swprintf_s( strText, L"Patches: %d  Triangles: %d", dwPatchCount, dwPatchCount * dwTessFactor * dwTessFactor *
                    2 );
        m_Font.DrawText( 0, 60, 0xFFFFFFFF, strText );

#ifdef _DEBUG
        D3DRECT TextWindow;
        m_Font.GetWindow( TextWindow );
        m_Font.DrawText( ( TextWindow.x2 - TextWindow.x1 ) * 0.5f, -20, 0xFFFFFFFF, L"Debug build - CPU basis conversion code is unoptimized", ATGFONT_CENTER_X );
#endif

        m_Font.End();
    }
}


