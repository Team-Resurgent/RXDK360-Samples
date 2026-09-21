//--------------------------------------------------------------------------------------
// AdvancedCommandBuffers.cpp
//
// This sample demonstrates many techniques and APIs relevant to runtime playback of
// precompiled command buffers.
// A toolchain sample, RecordCommandBuffer, generates .cmdbuffer files that contain a
// precompiled command buffer as well as additional data useful at runtime.  This sample
// loads the .cmdbuffer files and draws an entire scene using several different command
// buffers.
//
// Microsoft Game Technology Group.
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

#include "CommandBufferFile.h"
#include "Animation.h"

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_2, L"Rotate directional light" },
    { ATG::HELP_RIGHT_STICK,    ATG::HELP_PLACEMENT_2, L"Rotate camera" },
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_2, L"Apply dynamic fixups" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_2, L"Display\nshadow map" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_1, L"Faster camera zoom" },
    { ATG::HELP_MISC_CALLOUT,   ATG::HELP_PLACEMENT_2, L"Triggers move camera in/out" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))

const DWORD MAX_BONE_COUNT = 80;
const DWORD CHARACTER_COUNT = 5;

class CharacterInstance
{

public:

    SkeletonInstance m_SkeletonInstance;
    XMMATRIX m_matTransform;
    XMFLOAT4    m_HeadBoneMatrixPalette[ MAX_BONE_COUNT * 3 ];
    XMFLOAT4    m_BodyBoneMatrixPalette[ MAX_BONE_COUNT * 3 ];

    COMMANDBUFFER_FILE_HEADER m_LeftEye;
    COMMANDBUFFER_FILE_HEADER m_RightEye;
    COMMANDBUFFER_FILE_HEADER m_Head;
    COMMANDBUFFER_FILE_HEADER m_Body;
    COMMANDBUFFER_FILE_HEADER m_LeftEyeShadow;
    COMMANDBUFFER_FILE_HEADER m_RightEyeShadow;
    COMMANDBUFFER_FILE_HEADER m_HeadShadow;
    COMMANDBUFFER_FILE_HEADER m_BodyShadow;
};



//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: The skinned character sample class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{

public:

    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();

private:

    VOID    InitializeAnimation();
    VOID    RenderUI();
    VOID    RenderCharacterInstance( CharacterInstance& ci );
    VOID    RenderCharacterInstanceShadow( CharacterInstance& ci );
    VOID    RenderEnvironment();
    HRESULT LoadCommandBuffer( const CHAR* strFileName, COMMANDBUFFER_FILE_HEADER* pCommandBufferFile );
    VOID    CopyCommandBuffer( COMMANDBUFFER_FILE_HEADER* pDest, const COMMANDBUFFER_FILE_HEADER* pSrc );
    VOID    ApplyCommandBufferStaticFixups( COMMANDBUFFER_FILE_HEADER* pCommandBufferFile, ATG::Scene* pScene,
                                            BOOL bShadowFixups );
    D3DVertexShader* FindVertexShader( const WCHAR* strShaderName );
    D3DPixelShader* FindPixelShader( const WCHAR* strShaderName );
    ATG::BaseMesh* FindMeshFromModelName( const WCHAR* strModelName );
    VOID    ApplyDynamicFixups( CharacterInstance& ci );

private:

    // Sample framework objects
    ATG::Font m_Font;
    ATG::Timer m_Timer;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    FLOAT m_fDeltaTime;

    // Render targets and resolve textures
    D3DSurface* m_pColorTarget;
    D3DSurface* m_pDepthStencil;
    D3DVIEWPORT9 m_DefaultViewport;
    D3DTexture* m_pSceneResolveTexture;
    D3DTexture* m_pFrontBuffer;
    RECT m_DefaultClipRect;
    D3DSurface* m_pShadowDepthStencil;
    D3DTexture* m_pShadowTexture;
    D3DVIEWPORT9 m_ShadowViewport;
    RECT m_ShadowClipRect;
    D3DSurface* m_pPostRenderTarget;

    // Scene members
    ATG::Scene* m_pScene;
    ATG::Camera* m_pCamera;
    ATG::Animation* m_pAnimation;
    XMVECTOR m_CameraOrigin;
    XMMATRIX m_matProj;
    XMMATRIX m_matWVP;
    XMMATRIX m_matViewShadow;
    XMMATRIX m_matProjShadow;
    XMMATRIX m_matWVPShadow;
    ATG::PackedResource m_CharacterTextureVariants;

    // Rendering members
    D3DVertexShader* m_pVertexShaderSkinningConstants;
    D3DVertexShader* m_pVertexShaderTransform;
    D3DVertexShader* m_pVertexShaderSkinningConstantsShadow;
    D3DVertexShader* m_pVertexShaderTransformShadow;
    D3DPixelShader* m_pPixelShaderNormalMapping;
    D3DVertexShader* m_pVertexShaderEnvironment;
    D3DPixelShader* m_pPixelShaderEnvironment;
    D3DVertexShader* m_pVertexShaderQuery;
    DWORD m_dwFrameCount;
    XMFLOAT4 m_DirectionalLightDirection;
    XMFLOAT4 m_DirectionalLightColor;
    XMFLOAT4 m_AmbientColor;
    D3DRECT m_SafeRect;
    BOOL m_bDisplayShadowMap;
    BOOL m_bTilingEnabled;

    // Character instances
    CharacterInstance* m_pCharacterInstances;
    BOOL m_bTextureVariant;

    // Animation members
    Skeleton m_Skeleton;
    DWORD m_dwLeftEyeBoneIndex;
    DWORD m_dwRightEyeBoneIndex;

    // Precompiled command buffers

    COMMANDBUFFER_FILE_HEADER m_Environment;
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the sample.
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample App;

    // The sample runs at 1280x720 since the command buffers were recorded with 1280x720
    // rendertargets and predicated tiling, making them inflexible to rendertarget size
    // changes.
    App.m_d3dpp.BackBufferWidth = 1280;
    App.m_d3dpp.BackBufferHeight = 720;
    App.m_d3dpp.EnableAutoDepthStencil = FALSE;
    App.m_d3dpp.DisableAutoBackBuffer = TRUE;
    App.m_d3dpp.DisableAutoFrontBuffer = TRUE;
    App.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    App.m_dwDeviceCreationFlags |= D3DCREATE_BUFFER_2_FRAMES;

    App.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the objects for the sample, and loads various resources.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize member variables.
    m_bDrawHelp = FALSE;
    m_dwFrameCount = 0;
    m_DirectionalLightColor = XMFLOAT4( 1, 1, 0.8f, 1 );
    m_DirectionalLightDirection = XMFLOAT4( -0.7071f, -0.7071f, 0, 0 );
    m_AmbientColor = XMFLOAT4( 0.08f, 0.08f, 0.2f, 0.1f );
    m_bTextureVariant = FALSE;
    m_bDisplayShadowMap = TRUE;
    m_CameraOrigin = XMVectorSet( 67, 0, 18, 0 );

    m_pCharacterInstances = new CharacterInstance[CHARACTER_COUNT];

    // Create font.
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        ATG::FatalError( "Could not load font." );

    // Confine text drawing to the title safe area
    m_SafeRect = ATG::GetTitleSafeArea();
    m_Font.SetWindow( m_SafeRect );

    // Create help screen.
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        ATG::FatalError( "Could not load help resources." );

    // Initialize simple shaders.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create the scene rendertargets, resolve buffer, front buffer, and viewport.
    D3DSURFACE_PARAMETERS SurfParams = { 0 };
    m_pd3dDevice->CreateRenderTarget( 1280, 256, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ), D3DMULTISAMPLE_4_SAMPLES, 0,
                                      FALSE, &m_pColorTarget, &SurfParams );
    SurfParams.Base = XGSurfaceSize( 1280, 256, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_4_SAMPLES );
    m_pd3dDevice->CreateDepthStencilSurface( 1280, 256, D3DFMT_D24S8, D3DMULTISAMPLE_4_SAMPLES, 0,
                                             FALSE, &m_pDepthStencil, &SurfParams );
    m_pd3dDevice->CreateTexture( 1280, 720, 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_X8R8G8B8 ), D3DPOOL_DEFAULT, &m_pSceneResolveTexture, NULL );
    m_pd3dDevice->CreateTexture( 1280, 720, 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 ), D3DPOOL_DEFAULT, &m_pFrontBuffer, NULL );
    m_DefaultViewport.X = m_DefaultViewport.Y = 0;
    m_DefaultViewport.Width = 1280;
    m_DefaultViewport.Height = 720;
    m_DefaultViewport.MinZ = 0;
    m_DefaultViewport.MaxZ = 1;

    // Build the default clip rect from the default viewport.
    m_DefaultClipRect.left = m_DefaultViewport.X;
    m_DefaultClipRect.top = m_DefaultViewport.Y;
    m_DefaultClipRect.right = m_DefaultViewport.Width;
    m_DefaultClipRect.bottom = m_DefaultViewport.Height;

    // Create a shadow rendertarget, viewport, and clip rect.
    const DWORD dwShadowSize = 512;
    SurfParams.Base = 0;
    if( FAILED( m_pd3dDevice->CreateDepthStencilSurface( dwShadowSize, dwShadowSize, D3DFMT_D24S8, D3DMULTISAMPLE_NONE,
                                                         0, FALSE, &m_pShadowDepthStencil, &SurfParams ) ) )
        ATG::FatalError( "Could not create shadow depth/stencil." );
    if( FAILED( m_pd3dDevice->CreateTexture( dwShadowSize, dwShadowSize, 1, 0, D3DFMT_D24S8,
                                             D3DPOOL_DEFAULT, &m_pShadowTexture, NULL ) ) )
        ATG::FatalError( "Could not create shadow texture." );
    m_ShadowViewport.X = m_ShadowViewport.Y = 0;
    m_ShadowViewport.Width = m_ShadowViewport.Height = dwShadowSize;
    m_ShadowViewport.MinZ = 0;
    m_ShadowViewport.MaxZ = 1;
    m_ShadowClipRect.left = m_ShadowClipRect.top = 0;
    m_ShadowClipRect.right = m_ShadowClipRect.bottom = dwShadowSize;

    // Create a post-effects rendertarget at EDRAM offset 0.
    // This is where the UI will be rendered.
    SurfParams.Base = 0;
    m_pd3dDevice->CreateRenderTarget( 1280, 720, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ), D3DMULTISAMPLE_NONE, 0,
                                      FALSE, &m_pPostRenderTarget, &SurfParams );

    // Create scene object.
    m_pScene = new ATG::Scene();
    ATG::ResourceDatabase* pRDB = m_pScene->GetResourceDatabase();

    // Create default textures in the resource database.
    pRDB->CreateDefaultResources();

    // Load character and animation data from a scene file.
    HRESULT hr = ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\SkinnedCharacter.xatg", m_pScene, NULL,
                                                     ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not load character scene." );
    // Load environment scene data from file.
    ATG::Frame* pFrame = new ATG::Frame;
    pFrame->SetLocalTransform( XMMatrixIdentity() );
    m_pScene->AddChild( pFrame );
    hr = ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\ConstructionSite.xatg", m_pScene, pFrame,
                                             ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not load environment scene." );

    // Load character texture variants from a packed resource file.
    if( FAILED( m_CharacterTextureVariants.Create( "game:\\media\\SkinnedCharacterVariant.xpr" ) ) )
        ATG::FatalError( "Could not load character texture variants." );
    m_pScene->GetResourceDatabase()->AddBundledResources( &m_CharacterTextureVariants );

    // Load shaders.
    hr = ATG::LoadVertexShader( "game:\\media\\shaders\\SkinVSConstants.xvu", &m_pVertexShaderSkinningConstants );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader." );
    hr = ATG::LoadVertexShader( "game:\\media\\shaders\\TransformVS.xvu", &m_pVertexShaderTransform );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader." );
    hr = ATG::LoadVertexShader( "game:\\media\\shaders\\SkinVSConstants.xvu",
                                &m_pVertexShaderSkinningConstantsShadow );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader." );
    hr = ATG::LoadVertexShader( "game:\\media\\shaders\\TransformVS.xvu", &m_pVertexShaderTransformShadow );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader." );
    hr = ATG::LoadPixelShader( "game:\\media\\shaders\\NormalMapPS.xpu", &m_pPixelShaderNormalMapping );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load pixel shader." );
    hr = ATG::LoadVertexShader( "game:\\media\\shaders\\VSBackground.xvu", &m_pVertexShaderEnvironment );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader." );
    hr = ATG::LoadPixelShader( "game:\\media\\shaders\\PSBackground.xpu", &m_pPixelShaderEnvironment );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load pixel shader." );

    // Bind shader pair for rigid models.
    ATG::Model* pModel = ( ATG::Model* )m_pScene->FindObjectOfType( L"L_eyeBall", ATG::Model::TypeID );
    assert( pModel != NULL );
    ATG::BaseMesh* pMesh = pModel->GetMeshMapping( 0 ).pMesh;
    D3DVertexDeclaration* pDecl = pMesh->GetVertexData( 0 )->GetVertexDecl();
    DWORD dwStride = pMesh->GetVertexData( 0 )->GetVertexStream( 0 )->Stride;
    m_pVertexShaderTransform->Bind( 0, pDecl, &dwStride, m_pPixelShaderNormalMapping );
    m_pVertexShaderTransformShadow->Bind( 0, pDecl, &dwStride, NULL );
    m_pVertexShaderEnvironment->Bind( 0, pDecl, &dwStride, m_pPixelShaderEnvironment );

    // Bind shader pair for skinned models.
    pModel = ( ATG::Model* )m_pScene->FindObjectOfType( L"body", ATG::Model::TypeID );
    assert( pModel != NULL );
    pMesh = pModel->GetMeshMapping( 0 ).pMesh;
    pDecl = pMesh->GetVertexData( 0 )->GetVertexDecl();
    dwStride = pMesh->GetVertexData( 0 )->GetVertexStream( 0 )->Stride;
    m_pVertexShaderSkinningConstants->Bind( 0, pDecl, &dwStride, m_pPixelShaderNormalMapping );
    m_pVertexShaderSkinningConstantsShadow->Bind( 0, pDecl, &dwStride, NULL );

    // Load shaders for visibility queries.
    hr = ATG::LoadVertexShader( "game:\\media\\shaders\\VSQuery.xvu", &m_pVertexShaderQuery );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader." );

    // Create vertex decl for visibility queries.
    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,     0, D3DDECLTYPE_FLOAT3,     0,  D3DDECLUSAGE_POSITION,  0 },
        D3DDECL_END()
    };
    D3DVertexDeclaration* pVertexDeclQuery = D3DDevice_CreateVertexDeclaration( VertexElements );

    // Bind visibility vertex shader.
    dwStride = sizeof( XMFLOAT3 );
    m_pVertexShaderQuery->Bind( 0, pVertexDeclQuery, &dwStride, NULL );

    // Load color rendering command buffers to character instance 0.
    LoadCommandBuffer( "game:\\media\\cmdbuffers\\L_eyeBall.cmdbuffer", &m_pCharacterInstances[0].m_LeftEye );
    LoadCommandBuffer( "game:\\media\\cmdbuffers\\R_eyeBall.cmdbuffer", &m_pCharacterInstances[0].m_RightEye );
    LoadCommandBuffer( "game:\\media\\cmdbuffers\\Head.cmdbuffer", &m_pCharacterInstances[0].m_Head );
    LoadCommandBuffer( "game:\\media\\cmdbuffers\\body.cmdbuffer", &m_pCharacterInstances[0].m_Body );
    // Load shadow rendering command buffers to character instance 0.
    LoadCommandBuffer( "game:\\media\\cmdbuffers\\L_eyeBall_shadow.cmdbuffer",
                       &m_pCharacterInstances[0].m_LeftEyeShadow );
    LoadCommandBuffer( "game:\\media\\cmdbuffers\\R_eyeBall_shadow.cmdbuffer",
                       &m_pCharacterInstances[0].m_RightEyeShadow );
    LoadCommandBuffer( "game:\\media\\cmdbuffers\\Head_shadow.cmdbuffer", &m_pCharacterInstances[0].m_HeadShadow );
    LoadCommandBuffer( "game:\\media\\cmdbuffers\\body_shadow.cmdbuffer", &m_pCharacterInstances[0].m_BodyShadow );

    // Copy the loaded command buffers to the other character instances.
    for( DWORD i = 1; i < CHARACTER_COUNT; ++i )
    {
        CopyCommandBuffer( &m_pCharacterInstances[i].m_LeftEye, &m_pCharacterInstances[0].m_LeftEye );
        CopyCommandBuffer( &m_pCharacterInstances[i].m_RightEye, &m_pCharacterInstances[0].m_RightEye );
        CopyCommandBuffer( &m_pCharacterInstances[i].m_Head, &m_pCharacterInstances[0].m_Head );
        CopyCommandBuffer( &m_pCharacterInstances[i].m_Body, &m_pCharacterInstances[0].m_Body );
        CopyCommandBuffer( &m_pCharacterInstances[i].m_LeftEyeShadow, &m_pCharacterInstances[0].m_LeftEyeShadow );
        CopyCommandBuffer( &m_pCharacterInstances[i].m_RightEyeShadow, &m_pCharacterInstances[0].m_RightEyeShadow );
        CopyCommandBuffer( &m_pCharacterInstances[i].m_HeadShadow, &m_pCharacterInstances[0].m_HeadShadow );
        CopyCommandBuffer( &m_pCharacterInstances[i].m_BodyShadow, &m_pCharacterInstances[0].m_BodyShadow );
    }

    // Apply static fixups to the command buffers, which makes them ready for rendering.
    for( DWORD i = 0; i < CHARACTER_COUNT; ++i )
    {
        ApplyCommandBufferStaticFixups( &m_pCharacterInstances[i].m_LeftEye, m_pScene, FALSE );
        ApplyCommandBufferStaticFixups( &m_pCharacterInstances[i].m_RightEye, m_pScene, FALSE );
        ApplyCommandBufferStaticFixups( &m_pCharacterInstances[i].m_Head, m_pScene, FALSE );
        ApplyCommandBufferStaticFixups( &m_pCharacterInstances[i].m_Body, m_pScene, FALSE );
        ApplyCommandBufferStaticFixups( &m_pCharacterInstances[i].m_LeftEyeShadow, m_pScene, TRUE );
        ApplyCommandBufferStaticFixups( &m_pCharacterInstances[i].m_RightEyeShadow, m_pScene, TRUE );
        ApplyCommandBufferStaticFixups( &m_pCharacterInstances[i].m_HeadShadow, m_pScene, TRUE );
        ApplyCommandBufferStaticFixups( &m_pCharacterInstances[i].m_BodyShadow, m_pScene, TRUE );
    }

    // Load environment command buffer and apply static fixups.
    LoadCommandBuffer( "game:\\media\\cmdbuffers\\ConstructionSite.cmdbuffer", &m_Environment );
    ApplyCommandBufferStaticFixups( &m_Environment, m_pScene, FALSE );

    InitializeAnimation();

    // Place characters in the scene, and apply a random time offset to their animations.
    const FLOAT fSpacing = 1.0f;
    const FLOAT fPositionRadius = fSpacing * ( ( FLOAT )CHARACTER_COUNT - 1 ) / XM_2PI;
    for( DWORD i = 0; i < CHARACTER_COUNT; ++i )
    {
        FLOAT fTheta = ( FLOAT )i * ( XM_2PI / ( FLOAT )CHARACTER_COUNT );
        FLOAT fXPos = fPositionRadius * sinf( fTheta ) + m_CameraOrigin.x;
        FLOAT fZPos = fPositionRadius * cosf( fTheta ) + m_CameraOrigin.z;
        XMMATRIX matTransform = XMMatrixRotationY( XM_PIDIV2 ) * XMMatrixTranslation( fXPos, 0, fZPos );
        m_pCharacterInstances[i].m_matTransform = matTransform;

        FLOAT fTimeOffset = ( FLOAT )rand() / ( FLOAT )RAND_MAX;
        m_pCharacterInstances[i].m_SkeletonInstance.UpdateAnimation( fTimeOffset );
    }

    // Create a default camera and add it to the scene.
    m_pCamera = new ATG::Camera();
    m_pCamera->SetLocalTransform( XMMatrixIdentity() );
    m_pCamera->SetLocalPosition( XMVectorSet( 0, 4, -10, 1 ) );

    // Create projection matrix for scene rendering.
    const FLOAT fZNear = 0.01f;
    const FLOAT fZFar = 500.0f;
    ATG::Projection Proj;
    FLOAT fAspect = 16.0f / 9.0f;
    Proj.SetFovXAspect( XM_PIDIV2, fAspect, fZNear, fZFar );
    m_pCamera->SetProjection( Proj );
    m_matProj = Proj.GetMatrix();

    // Create projection matrix for shadow rendering.
    const FLOAT fZNearShadow = 0.1f;
    const FLOAT fZFarShadow = 5.0f;
    ATG::Projection ProjShadow;
    fAspect = ( FLOAT )m_ShadowViewport.Width / ( FLOAT )m_ShadowViewport.Height;
    ProjShadow.SetFovXAspect( XM_PIDIV2, fAspect, fZNearShadow, fZFarShadow );
    m_matProjShadow = ProjShadow.GetMatrix();
    m_pScene->AddChild( m_pCamera );

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

    // Update frame count.
    ++m_dwFrameCount;

    // Get input from controllers (check for exit button sequence).
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // The A button applies dynamic fixups to the character 0 body command buffer.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_bTextureVariant = !m_bTextureVariant;
        ApplyDynamicFixups( m_pCharacterInstances[0] );
    }

    // The B button toggles the display of the shadow map.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_bDisplayShadowMap = !m_bDisplayShadowMap;

    // Update camera position.
    static FLOAT s_fRotateY = 4.64f;
    static FLOAT s_fRotateX = 0.14f;
    static FLOAT s_fCameraDistance = 1.5f;
    FLOAT fTargetHeight = 0.35f;
    const FLOAT fRotateSpeed = XM_PIDIV2;
    FLOAT fZoomSpeed = 2.0f;
    if( pGamepad->wLastButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
        fZoomSpeed *= 10.0f;
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
    s_fCameraDistance = min( s_fCameraDistance, 200.0f );
    static const XMVECTOR vAxisX = { 1, 0, 0, 0 };
    static const XMVECTOR vAxisY = { 0, 1, 0, 0 };
    XMVECTOR qRotateX = XMQuaternionRotationAxis( vAxisX, s_fRotateX );
    XMVECTOR qRotateY = XMQuaternionRotationAxis( vAxisY, s_fRotateY );
    XMVECTOR qRotation = XMQuaternionMultiply( qRotateX, qRotateY );
    XMMATRIX matRotation = XMMatrixRotationQuaternion( qRotation );
    XMMATRIX matWorld = XMMatrixTranslation( 0, 0, -s_fCameraDistance );
    matWorld *= matRotation;
    matWorld *= XMMatrixTranslation( 0, fTargetHeight, 0 );
    matWorld *= XMMatrixTranslationFromVector( m_CameraOrigin );
    m_pCamera->SetLocalTransform( matWorld );

    // Update light direction.
    static FLOAT s_fLightAngle = -3.64f;
    const FLOAT fLightRotateSpeed = XM_PIDIV2;
    fRotateYAmount = pGamepad->fX1 * fLightRotateSpeed * m_fDeltaTime;
    s_fLightAngle += fRotateYAmount;
    m_DirectionalLightDirection.y = -0.5f;
    m_DirectionalLightDirection.x = 0.866f * cosf( s_fLightAngle );
    m_DirectionalLightDirection.z = 0.866f * sinf( s_fLightAngle );

    // Create a light view matrix for shadow rendering.
    // The camera position is based on the light view direction, looking at the origin.
    XMVECTOR vLightCameraPos = -XMLoadFloat4( &m_DirectionalLightDirection );
    vLightCameraPos *= 1.2f;
    m_matViewShadow = XMMatrixLookAtLH( vLightCameraPos + m_CameraOrigin, m_CameraOrigin, XMVectorSet( 0, 1, 0, 0 ) );

    // Update animation.
    for( DWORD dwCharacterIndex = 0; dwCharacterIndex < CHARACTER_COUNT; ++dwCharacterIndex )
    {
        CharacterInstance& ci = m_pCharacterInstances[dwCharacterIndex];

        ci.m_SkeletonInstance.UpdateAnimation( m_fDeltaTime );

        ci.m_SkeletonInstance.CreateBonePalette( 0, ci.m_HeadBoneMatrixPalette, FALSE );
        ci.m_SkeletonInstance.CreateBonePalette( 1, ci.m_BodyBoneMatrixPalette, FALSE );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene and UI.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    XMMATRIX matWV = m_pCamera->GetWorldView();
    m_matWVP = matWV * m_matProj;
    m_matWVPShadow = m_matViewShadow * m_matProjShadow;
    ATG::DebugDraw::SetViewProjection( m_matWVP );

    m_pd3dDevice->BeginScene();

    PIXBeginNamedEvent( 0, "Shadow" );

    // Render a shadow map.
    m_pd3dDevice->SetRenderTarget( 0, NULL );
    m_pd3dDevice->SetDepthStencilSurface( m_pShadowDepthStencil );
    m_pd3dDevice->SetViewport( &m_ShadowViewport );
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0 );

    for( DWORD dwCharacterIndex = 0; dwCharacterIndex < CHARACTER_COUNT; ++dwCharacterIndex )
    {
        CharacterInstance& ci = m_pCharacterInstances[dwCharacterIndex];
        RenderCharacterInstanceShadow( ci );
    }

    m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, m_pShadowTexture, NULL, 0, 0, NULL, 1.0f, 0, NULL );

    PIXEndNamedEvent();

    PIXBeginNamedEvent( 0, "Scene" );

    // Render the scene.
    m_pd3dDevice->SetRenderTarget( 0, m_pColorTarget );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencil );
    m_pd3dDevice->SetViewport( &m_DefaultViewport );

    static const D3DRECT TilingRects[] =
        {
            { 0,   0, 1280, 256 },
            { 0, 256, 1280, 512 },
            { 0, 512, 1280, 720 }
        };
    static const DWORD dwTilingRectCount = ARRAYSIZE( TilingRects );
    m_pd3dDevice->BeginTiling( 0, dwTilingRectCount, TilingRects, NULL, 1.0f, 0 );

    ATG::RenderBackground( 0xFF5050A0, 0xFF9090C0 );

    for( DWORD dwCharacterIndex = 0; dwCharacterIndex < CHARACTER_COUNT; ++dwCharacterIndex )
    {
        CharacterInstance& ci = m_pCharacterInstances[dwCharacterIndex];
        RenderCharacterInstance( ci );
    }

    RenderEnvironment();

    m_pd3dDevice->EndTiling( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARDEPTHSTENCIL, NULL, m_pSceneResolveTexture,
                             NULL, 1.0f, 0, NULL );

    PIXEndNamedEvent();

    PIXBeginNamedEvent( 0, "UI" );

    // Restore the scene rendering to screen.
    m_pd3dDevice->SetRenderTarget( 0, m_pPostRenderTarget );
    m_pd3dDevice->SetDepthStencilSurface( NULL );
    D3DRECT ScreenRect = { 0, 0, 1280, 720 };
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( ScreenRect, m_pSceneResolveTexture, FALSE );

    // Draw the shadow map in the corner of the screen.
    if( m_bDisplayShadowMap )
    {
        D3DRECT ShadowTextureRect;
        const DWORD dwShadowDisplaySize = ( m_SafeRect.y2 - m_SafeRect.y1 ) / 2;
        ShadowTextureRect.x1 = m_SafeRect.x1;
        ShadowTextureRect.x2 = m_SafeRect.x1 + dwShadowDisplaySize;
        ShadowTextureRect.y2 = m_SafeRect.y2;
        ShadowTextureRect.y1 = m_SafeRect.y2 - dwShadowDisplaySize;
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( ShadowTextureRect, m_pShadowTexture, TRUE );
    }

    RenderUI();

    PIXEndNamedEvent();

    m_pd3dDevice->EndScene();

    m_pd3dDevice->SynchronizeToPresentationInterval();
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pFrontBuffer, NULL, 0, 0, NULL, 1.0f, 0, NULL );
    m_pd3dDevice->Swap( m_pFrontBuffer, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderEnvironment()
// Desc: Draws the background environment using its command buffer.
//--------------------------------------------------------------------------------------
VOID Sample::RenderEnvironment()
{
    // Compute the view*projection matrix for the environment.
    XMMATRIX matVPt = XMMatrixTranspose( m_matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matVPt, 4 );

    // Set up the lighting and camera shader constants for the environment.
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&m_DirectionalLightDirection, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&m_DirectionalLightColor, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 2, ( FLOAT* )&m_AmbientColor, 1 );
    XMVECTOR vCameraPos = m_pCamera->GetWorldPosition();
    m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&vCameraPos, 1 );

    // Run the environment command buffer.
    m_pd3dDevice->RunCommandBuffer( m_Environment.pCommandBuffer, 0 );
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
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"AdvancedCommandBuffers" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.End();
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderCharacterInstance()
// Desc: Draws a single character using four command buffers.  Notice how this code only
//       sets up per-frame unique state, and the remainder of the draw commands are
//       encapsulated in the command buffers, which are kicked off using the
//       RunCommandBuffer API.
//--------------------------------------------------------------------------------------
VOID Sample::RenderCharacterInstance( CharacterInstance& ci )
{
    PIXBeginNamedEvent( 0, "Character Instance" );

    // Set up the world*view*projection matrix for the head and body.
    XMMATRIX matWVP = ci.m_matTransform * m_matWVP;
    XMMATRIX matWVPt = XMMatrixTranspose( matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVPt, 4 );

    // Set up lighting and camera shader constants for the head and body.
    XMVECTOR vLightDirection = XMLoadFloat4( &m_DirectionalLightDirection );
    XMVECTOR vDet;
    XMMATRIX matInvWorld = XMMatrixInverse( &vDet, ci.m_matTransform );
    XMVECTOR vObjLightDirection = XMVector3TransformNormal( vLightDirection, matInvWorld );
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&vObjLightDirection, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&m_DirectionalLightColor, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 2, ( FLOAT* )&m_AmbientColor, 1 );
    XMVECTOR vCameraPos = m_pCamera->GetWorldPosition();
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&vCameraPos, 1 );

    // Set the bone matrix palette for the body.
    m_pd3dDevice->SetVertexShaderConstantF( 12, ( FLOAT* )ci.m_BodyBoneMatrixPalette, MAX_BONE_COUNT * 3 );
    // Run the body command buffer.
    m_pd3dDevice->RunCommandBuffer( ci.m_Body.pCommandBuffer, 0 );

    // Set the bone matrix palette for the head.
    m_pd3dDevice->SetVertexShaderConstantF( 12, ( FLOAT* )ci.m_HeadBoneMatrixPalette, MAX_BONE_COUNT * 3 );
    // Run the head command buffer.
    m_pd3dDevice->RunCommandBuffer( ci.m_Head.pCommandBuffer, 0 );

    // Compute the world*view*projection matrix for the left eye.
    XMMATRIX matLeftEye = ci.m_SkeletonInstance.m_WorldPose.LoadTransform( m_dwLeftEyeBoneIndex ) * ci.m_matTransform;
    matWVP = matLeftEye * m_matWVP;
    matWVPt = XMMatrixTranspose( matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVPt, 4 );

    // Set up the lighting and camera shader constants for the left eye.
    matInvWorld = XMMatrixInverse( &vDet, matLeftEye );
    vObjLightDirection = XMVector3TransformNormal( vLightDirection, matInvWorld );
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&vObjLightDirection, 1 );
    XMVECTOR vObjCameraPos = XMVector3TransformNormal( vCameraPos, matInvWorld );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&vObjCameraPos, 1 );

    // Run the left eye command buffer.
    m_pd3dDevice->RunCommandBuffer( ci.m_LeftEye.pCommandBuffer, 0 );

    // Compute the world*view*projection matrix for the right eye.
    XMMATRIX matRightEye = ci.m_SkeletonInstance.m_WorldPose.LoadTransform( m_dwRightEyeBoneIndex ) *
        ci.m_matTransform;
    matWVP = matRightEye * m_matWVP;
    matWVPt = XMMatrixTranspose( matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVPt, 4 );

    // Set up the lighting and camera shader constants for the right eye.
    matInvWorld = XMMatrixInverse( &vDet, matRightEye );
    vObjLightDirection = XMVector3TransformNormal( vLightDirection, matInvWorld );
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&vObjLightDirection, 1 );
    vObjCameraPos = XMVector3TransformNormal( vCameraPos, matInvWorld );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&vObjCameraPos, 1 );

    // Run the right eye command buffer.
    m_pd3dDevice->RunCommandBuffer( ci.m_RightEye.pCommandBuffer, 0 );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RenderCharacterInstanceShadow()
// Desc: Similar to RenderCharacterInstance, this method draws a single character, but
//       only to a depth/stencil surface.  Less state setup is needed when rendering a
//       shadow map.
//--------------------------------------------------------------------------------------
VOID Sample::RenderCharacterInstanceShadow( CharacterInstance& ci )
{
    PIXBeginNamedEvent( 0, "Character Instance Shadow" );

    // Set up the world*view*projection matrix for the head and body.
    XMMATRIX matWVP = ci.m_matTransform * m_matWVPShadow;
    XMMATRIX matWVPt = XMMatrixTranspose( matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVPt, 4 );

    // Set the bone matrix palette for the body.
    m_pd3dDevice->SetVertexShaderConstantF( 12, ( FLOAT* )ci.m_BodyBoneMatrixPalette, MAX_BONE_COUNT * 3 );
    // Run the body command buffer.
    m_pd3dDevice->RunCommandBuffer( ci.m_BodyShadow.pCommandBuffer, 0 );

    // Set the bone matrix palette for the head.
    m_pd3dDevice->SetVertexShaderConstantF( 12, ( FLOAT* )ci.m_HeadBoneMatrixPalette, MAX_BONE_COUNT * 3 );
    // Run the head command buffer.
    m_pd3dDevice->RunCommandBuffer( ci.m_HeadShadow.pCommandBuffer, 0 );

    // Compute the world*view*projection matrix for the left eye.
    XMMATRIX matLeftEye = ci.m_SkeletonInstance.m_WorldPose.LoadTransform( m_dwLeftEyeBoneIndex );
    matWVP = matLeftEye * ci.m_matTransform * m_matWVPShadow;
    matWVPt = XMMatrixTranspose( matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVPt, 4 );

    // Run the left eye command buffer.
    m_pd3dDevice->RunCommandBuffer( ci.m_LeftEyeShadow.pCommandBuffer, 0 );

    // Compute the world*view*projection matrix for the right eye.
    XMMATRIX matRightEye = ci.m_SkeletonInstance.m_WorldPose.LoadTransform( m_dwRightEyeBoneIndex );
    matWVP = matRightEye * ci.m_matTransform * m_matWVPShadow;
    matWVPt = XMMatrixTranspose( matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVPt, 4 );

    // Run the right eye command buffer.
    m_pd3dDevice->RunCommandBuffer( ci.m_RightEyeShadow.pCommandBuffer, 0 );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: InitializeAnimation()
// Desc: Initializes the animation system and binds meshes to parts of the animation
//       system.
//--------------------------------------------------------------------------------------
VOID Sample::InitializeAnimation()
{
    // Find a frame named "Root" - this is the root of the animated object.
    ATG::Frame* pSkeletonFrame = ( ATG::Frame* )m_pScene->FindObjectOfType( L"Root", ATG::Frame::TypeID );
    assert( pSkeletonFrame != NULL );

    // Initialize skeleton from the frames found in the scene.
    m_Skeleton.Initialize( pSkeletonFrame );

    // We need to extract model names from the loaded command buffer extra data.
    // We can use the first character's command buffer data and apply that to all of the characters.
    CharacterInstance& ci0 = m_pCharacterInstances[0];

    // Find the skinned meshes (body and head).
    // The names of the models were saved with the command buffer data.  Each model has
    // one and only one mesh associated with it.
    ATG::SkinnedMesh* pSkinnedMeshHead = ( ATG::SkinnedMesh* )
        FindMeshFromModelName( ( const WCHAR* )ci0.m_Head.pExtraData );
    ATG::SkinnedMesh* pSkinnedMeshBody = ( ATG::SkinnedMesh* )
        FindMeshFromModelName( ( const WCHAR* )ci0.m_Body.pExtraData );
    if( pSkinnedMeshHead == NULL || pSkinnedMeshBody == NULL )
    {
        ATG::FatalError( "Could not find skinned mesh data in the scene file." );
    }

    // Find the bones corresponding to the left and right eyes.
    m_dwLeftEyeBoneIndex = m_Skeleton.FindBone( ( const WCHAR* )ci0.m_LeftEye.pExtraData );
    m_dwRightEyeBoneIndex = m_Skeleton.FindBone( ( const WCHAR* )ci0.m_RightEye.pExtraData );

    // Find the animation track set.
    ATG::Animation* pAnimation = ( ATG::Animation* )m_pScene->FindObjectOfType( L"Untitled", ATG::Animation::TypeID );
    if( pAnimation == NULL )
    {
        ATG::FatalError( "Could not find animation data in the scene file." );
    }

    // Initialize all of the character instances.
    for( DWORD dwCharacterIndex = 0; dwCharacterIndex < CHARACTER_COUNT; ++dwCharacterIndex )
    {
        CharacterInstance& ci = m_pCharacterInstances[dwCharacterIndex];

        // Initialize the skeleton instance, and allocate two mesh bindings for the body and head meshes.
        ci.m_SkeletonInstance.Initialize( &m_Skeleton, 2 );

        // Create skeleton bindings for the head and body meshes.
        ci.m_SkeletonInstance.BindSkinnedMesh( 0, pSkinnedMeshHead );
        ci.m_SkeletonInstance.BindSkinnedMesh( 1, pSkinnedMeshBody );

        // Bind animation to skeleton instance.
        ci.m_SkeletonInstance.CreateAnimationBinding( pAnimation );
    }
}


//--------------------------------------------------------------------------------------
// Name: FindMeshFromModelName()
// Desc: Given a model name, this method searches the scene for the correct model, then
//       returns the mesh associated with that model.
//--------------------------------------------------------------------------------------
ATG::BaseMesh* Sample::FindMeshFromModelName( const WCHAR* strModelName )
{
    ATG::Model* pModel = ( ATG::Model* )m_pScene->FindObjectOfType( strModelName, ATG::Model::TypeID );
    if( pModel != NULL )
    {
        return pModel->GetMeshMapping( 0 ).pMesh;
    }
    return NULL;
}


//--------------------------------------------------------------------------------------
// Name: LoadCommandBuffer()
// Desc: Loads a precompiled command buffer file.  The COMMANDBUFFER_FILE_HEADER struct
//       serves as both a file header and the runtime structure for the command buffer
//       and its associated data such as fixups and extra data.
//--------------------------------------------------------------------------------------
HRESULT Sample::LoadCommandBuffer( const CHAR* strFileName, COMMANDBUFFER_FILE_HEADER* pCommandBufferFile )
{
    assert( strFileName != NULL );
    assert( pCommandBufferFile != NULL );

    HANDLE hFile = CreateFile( strFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_READONLY, NULL );
    if( hFile == INVALID_HANDLE_VALUE )
    {
        ATG::DebugSpew( "Could not open command buffer file \"%s\".\n", strFileName );
        return E_FAIL;
    }

    DWORD dwNumBytesRead = 0;
    BOOL bResult = ReadFile( hFile, pCommandBufferFile, sizeof( COMMANDBUFFER_FILE_HEADER ), &dwNumBytesRead, NULL );
    assert( bResult != 0 );

    if( pCommandBufferFile->dwVersion != COMMANDBUFFER_FILE_VERSION ||
        pCommandBufferFile->dwFileHeaderSize < sizeof( COMMANDBUFFER_FILE_HEADER ) )
    {
        ATG::DebugSpew( "Version mismatch in command buffer file \"%s\".\n", strFileName );
        CloseHandle( hFile );
        return E_FAIL;
    }

    pCommandBufferFile->pExtraData = NULL;
    if( pCommandBufferFile->dwFileHeaderSize > sizeof( COMMANDBUFFER_FILE_HEADER ) )
    {
        DWORD dwExtraDataSize = pCommandBufferFile->dwFileHeaderSize - sizeof( COMMANDBUFFER_FILE_HEADER );
        pCommandBufferFile->pExtraData = new BYTE[ dwExtraDataSize ];
        bResult = ReadFile( hFile, pCommandBufferFile->pExtraData, dwExtraDataSize, &dwNumBytesRead, NULL );
        assert( bResult != 0 );
    }

    pCommandBufferFile->pHeaderData = new BYTE[ pCommandBufferFile->dwHeaderSize ];
    bResult = ReadFile( hFile, pCommandBufferFile->pHeaderData, pCommandBufferFile->dwHeaderSize, &dwNumBytesRead,
                        NULL );
    assert( bResult != 0 );

    pCommandBufferFile->pPhysicalData = ( BYTE* )XPhysicalAlloc( pCommandBufferFile->dwPhysicalSize, MAXULONG_PTR,
                                                                 4096, PAGE_READWRITE | PAGE_WRITECOMBINE );
    bResult = ReadFile( hFile, pCommandBufferFile->pPhysicalData, pCommandBufferFile->dwPhysicalSize, &dwNumBytesRead,
                        NULL );
    assert( bResult != 0 );

    pCommandBufferFile->pInitializationData = new BYTE[ pCommandBufferFile->dwInitializationSize ];
    bResult = ReadFile( hFile, pCommandBufferFile->pInitializationData,
                        pCommandBufferFile->dwInitializationSize, &dwNumBytesRead, NULL );
    assert( bResult != 0 );

    if( pCommandBufferFile->dwStaticFixupCount > 0 )
    {
        pCommandBufferFile->pStaticFixupDescs = ( FIXUP_DESC* )new BYTE[ pCommandBufferFile->dwStaticFixupDataSize ];
        bResult = ReadFile( hFile, pCommandBufferFile->pStaticFixupDescs,
                            pCommandBufferFile->dwStaticFixupDataSize, &dwNumBytesRead, NULL );
        assert( bResult != 0 );
    }

    if( pCommandBufferFile->dwDynamicFixupCount > 0 )
    {
        pCommandBufferFile->pDynamicFixupDescs = ( FIXUP_DESC* )new BYTE[ pCommandBufferFile->dwDynamicFixupDataSize ];
        bResult = ReadFile( hFile, pCommandBufferFile->pDynamicFixupDescs,
                            pCommandBufferFile->dwDynamicFixupDataSize, &dwNumBytesRead, NULL );
        assert( bResult != 0 );
    }

    CloseHandle( hFile );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CopyCommandBuffer()
// Desc: Creates a deep copy of a COMMANDBUFFER_FILE_HEADER struct.
//--------------------------------------------------------------------------------------
VOID Sample::CopyCommandBuffer( COMMANDBUFFER_FILE_HEADER* pDest, const COMMANDBUFFER_FILE_HEADER* pSrc )
{
    assert( pDest != NULL );
    assert( pSrc != NULL );

    XMemCpy( pDest, pSrc, sizeof( COMMANDBUFFER_FILE_HEADER ) );
    if( pSrc->pExtraData != NULL )
    {
        DWORD dwExtraDataSize = pSrc->dwFileHeaderSize - sizeof( COMMANDBUFFER_FILE_HEADER );
        pDest->pExtraData = new BYTE[ dwExtraDataSize ];
        XMemCpy( pDest->pExtraData, pSrc->pExtraData, dwExtraDataSize );
    }

    pDest->pHeaderData = new BYTE[ pSrc->dwHeaderSize ];
    XMemCpy( pDest->pHeaderData, pSrc->pHeaderData, pSrc->dwHeaderSize );

    pDest->pPhysicalData = ( BYTE* )XPhysicalAlloc( pSrc->dwPhysicalSize, MAXULONG_PTR, 4096,
                                                    PAGE_READWRITE | PAGE_WRITECOMBINE );
    memcpy( pDest->pPhysicalData, pSrc->pPhysicalData, pSrc->dwPhysicalSize );

    pDest->pInitializationData = new BYTE[ pSrc->dwInitializationSize ];
    XMemCpy( pDest->pInitializationData, pSrc->pInitializationData, pSrc->dwInitializationSize );

    if( pSrc->dwStaticFixupCount > 0 )
    {
        pDest->pStaticFixupDescs = ( FIXUP_DESC* )new BYTE[ pSrc->dwStaticFixupDataSize ];
        XMemCpy( pDest->pStaticFixupDescs, pSrc->pStaticFixupDescs, pSrc->dwStaticFixupDataSize );
    }

    if( pSrc->dwDynamicFixupCount > 0 )
    {
        pDest->pDynamicFixupDescs = ( FIXUP_DESC* )new BYTE[ pSrc->dwDynamicFixupDataSize ];
        XMemCpy( pDest->pDynamicFixupDescs, pSrc->pDynamicFixupDescs, pSrc->dwDynamicFixupDataSize );
    }
}


//--------------------------------------------------------------------------------------
// Name: ApplyCommandBufferStaticFixups()
// Desc: Reconstructs a command buffer from its component pieces and applies all static
//       fixups that were loaded along with the command buffer data.
//--------------------------------------------------------------------------------------
VOID Sample::ApplyCommandBufferStaticFixups( COMMANDBUFFER_FILE_HEADER* pCommandBufferFile, ATG::Scene* pScene,
                                             BOOL bShadowFixups )
{
    assert( pCommandBufferFile != NULL && pCommandBufferFile->pCommandBuffer != NULL );
    D3DCommandBuffer* pCB = pCommandBufferFile->pCommandBuffer;

    assert( pScene != NULL );
    ATG::ResourceDatabase* pRDB = pScene->GetResourceDatabase();

    // BeginReconstruction informs Direct3D that this command buffer is loaded and
    // ready to be finalized for playback.
    pCB->BeginReconstruction( D3DRECONSTRUCT_VERIFY, pCommandBufferFile->pPhysicalData,
                              pCommandBufferFile->pInitializationData );

    // Before calling EndReconstruction, all of the static fixups must be applied.
    DWORD dwFixupCount = pCommandBufferFile->dwStaticFixupCount;
    for( DWORD i = 0; i < dwFixupCount; ++i )
    {
        FIXUP_DESC& Fixup = pCommandBufferFile->pStaticFixupDescs[i];
        switch( Fixup.Type )
        {
            case FIXUP_TEXTURE:
            {
                // Apply a texture fixup by locating the proper texture in the resource
                // database and calling SetTexture on the command buffer object.
                ATG::Texture* pTexture = ( ATG::Texture* )pRDB->FindResourceOfType( Fixup.strResourceName,
                                                                                    ATG::Texture::TypeID );
                if( pTexture != NULL )
                {
                    pCB->SetTexture( Fixup.dwFixupHandle, pTexture->GetD3DTexture() );
                }
                else
                {
                    ATG::DebugSpew( "Could not apply fixup for texture %S.\n", Fixup.strResourceName );
                }
                break;
            }
            case FIXUP_VERTEXBUFFER:
            {
                // Apply a VB fixup by locating the proper mesh in the resource
                // database and calling SetVertexBuffer on the command buffer object.
                ATG::BaseMesh* pMesh = ( ATG::BaseMesh* )pScene->FindObjectOfType( Fixup.strResourceName,
                                                                                   ATG::BaseMesh::TypeID );
                if( pMesh != NULL )
                {
                    DWORD dwStreamIndex = Fixup.dwResourceExtraData;
                    pCB->SetVertexBuffer( Fixup.dwFixupHandle,
                                          pMesh->GetVertexData( 0 )->GetVertexStream( dwStreamIndex )->pVertexBuffer );
                }
                else
                {
                    ATG::DebugSpew( "Could not apply fixup for vertex buffer %S.\n", Fixup.strResourceName );
                }
                break;
            }
            case FIXUP_INDEXBUFFER:
            {
                // Apply an IB fixup.
                ATG::BaseMesh* pMesh = ( ATG::BaseMesh* )pScene->FindObjectOfType( Fixup.strResourceName,
                                                                                   ATG::BaseMesh::TypeID );
                if( pMesh != NULL )
                {
                    pCB->SetIndexBuffer( Fixup.dwFixupHandle, pMesh->GetIndexData( 0 )->GetIndexBuffer() );
                }
                else
                {
                    ATG::DebugSpew( "Could not apply fixup for index buffer %S.\n", Fixup.strResourceName );
                }
                break;
            }
            case FIXUP_VERTEXSHADER:
            {
                // Apply a vertex shader fixup.
                D3DVertexShader* pVS = FindVertexShader( Fixup.strResourceName );
                if( pVS != NULL )
                {
                    pCB->SetVertexShader( Fixup.dwFixupHandle, pVS );
                }
                else
                {
                    ATG::DebugSpew( "Could not apply fixup for vertex shader %S.\n", Fixup.strResourceName );
                }
                break;
            }
            case FIXUP_PIXELSHADER:
            {
                // Apply a pixel shader fixup.
                D3DPixelShader* pPS = FindPixelShader( Fixup.strResourceName );
                if( pPS != NULL )
                {
                    pCB->SetPixelShader( Fixup.dwFixupHandle, pPS );
                }
                else
                {
                    ATG::DebugSpew( "Could not apply fixup for pixel shader %S.\n", Fixup.strResourceName );
                }
                break;
            }
            case FIXUP_SURFACES:
            {
                // Apply a surfaces fixup.
                D3DSURFACES Surfaces = { 0 };
                if( bShadowFixups )
                {
                    Surfaces.pDepthStencilSurface = m_pShadowDepthStencil;
                }
                else
                {
                    Surfaces.pDepthStencilSurface = m_pDepthStencil;
                    Surfaces.pRenderTarget[0] = m_pColorTarget;
                }
                pCB->SetSurfaces( Fixup.dwFixupHandle, &Surfaces );
                break;
            }
            case FIXUP_VIEWPORT:
            {
                // Apply a viewport fixup.
                if( bShadowFixups )
                {
                    pCB->SetViewport( Fixup.dwFixupHandle, &m_ShadowViewport );
                }
                else
                {
                    pCB->SetViewport( Fixup.dwFixupHandle, &m_DefaultViewport );
                }
                break;
            }
            case FIXUP_CLIPRECT:
            {
                // Apply a clip rectangle fixup.
                if( bShadowFixups )
                {
                    pCB->SetClipRect( Fixup.dwFixupHandle, &m_ShadowClipRect );
                }
                else
                {
                    pCB->SetClipRect( Fixup.dwFixupHandle, &m_DefaultClipRect );
                }
                break;
            }
            default:
            {
                ATG::DebugSpew( "Could not identify fixup for resource %S.\n", Fixup.strResourceName );
                break;
            }
        }
    }

    // EndReconstruction makes the command buffer ready for playback.  No more static
    // fixups can be applied at this point.
    pCB->EndReconstruction();

    // We are done with reconstruction, so D3D no longer needs the initialization data.
    delete[] pCommandBufferFile->pInitializationData;
    pCommandBufferFile->pInitializationData = NULL;

    // We no longer need the static fixup array either.
    delete[] pCommandBufferFile->pStaticFixupDescs;
    pCommandBufferFile->pStaticFixupDescs = NULL;
    pCommandBufferFile->dwStaticFixupCount = 0;
}


//--------------------------------------------------------------------------------------
// Name: FindVertexShader()
// Desc: Finds a vertex shader of the given name.  This would be implemented by a shader
//       or resource database in a real game title.
//--------------------------------------------------------------------------------------
D3DVertexShader* Sample::FindVertexShader( const WCHAR* strShaderName )
{
    if( _wcsicmp( strShaderName, L"TransformVS" ) == 0 )
        return m_pVertexShaderTransform;
    else if( _wcsicmp( strShaderName, L"SkinVSConstants" ) == 0 )
        return m_pVertexShaderSkinningConstants;
    else if( _wcsicmp( strShaderName, L"TransformVS_NullPS" ) == 0 )
        return m_pVertexShaderTransformShadow;
    else if( _wcsicmp( strShaderName, L"SkinVSConstants_NullPS" ) == 0 )
        return m_pVertexShaderSkinningConstantsShadow;
    else if( _wcsicmp( strShaderName, L"VSBackground" ) == 0 )
        return m_pVertexShaderEnvironment;
    else if( _wcsicmp( strShaderName, L"VSQuery" ) == 0 )
        return m_pVertexShaderQuery;
    return NULL;
}


//--------------------------------------------------------------------------------------
// Name: FindPixelShader()
// Desc: Finds a pixel shader of the given name.  This would be implemented by a shader
//       or resource database in a real game title.
//--------------------------------------------------------------------------------------
D3DPixelShader* Sample::FindPixelShader( const WCHAR* strShaderName )
{
    if( _wcsicmp( strShaderName, L"NormalMapPS" ) == 0 )
        return m_pPixelShaderNormalMapping;
    else if( _wcsicmp( strShaderName, L"PSBackground" ) == 0 )
        return m_pPixelShaderEnvironment;
    return NULL;
}


//--------------------------------------------------------------------------------------
// Name: FindDynamicFixup()
// Desc: Given a loaded and reconstructed command buffer, this method finds a dynamic 
//       fixup associated with a specific resource name.
//--------------------------------------------------------------------------------------
FIXUP_DESC* FindDynamicFixup( COMMANDBUFFER_FILE_HEADER* pCommandBufferFile, const WCHAR* strResourceName )
{
    assert( pCommandBufferFile != NULL );
    DWORD dwCount = pCommandBufferFile->dwDynamicFixupCount;
    for( DWORD i = 0; i < dwCount; ++i )
    {
        FIXUP_DESC* pFixup = &pCommandBufferFile->pDynamicFixupDescs[i];
        if( _wcsicmp( pFixup->strResourceName, strResourceName ) == 0 )
            return pFixup;
    }
    return NULL;
}


//--------------------------------------------------------------------------------------
// Name: ApplyDynamicFixups()
// Desc: Applies three specific dynamic fixups to the body command buffer.  These fixups
//       are designed to replace the three diffuse textures used to render various
//       pieces of the body.
//       Note that this method is somewhat hardcoded.  This reflects the fact that
//       fixups are highly specific in nature.  In a game title, dynamic fixups would
//       be used to display game state in a precompiled command buffer, such as changing
//       a character's skin or appearance based on infrequently changed game state like
//       character customization.
//--------------------------------------------------------------------------------------
VOID Sample::ApplyDynamicFixups( CharacterInstance& ci )
{
    // Begin dynamic fixups on the body command buffer.
    // Since the command buffer is not double-buffered, this API call will cause a
    // CPU-GPU stall.  It is very similar to calling Lock() on a vertex or index buffer.
    // As such, this method should not be called every frame.  It should
    // only be called when necessary.  State that may change every frame should be set
    // through GPU register inheritance, or do not use command buffers at all.
    ci.m_Body.pCommandBuffer->BeginDynamicFixups();

    // Search for a texture fixup for the pants texture.
    const WCHAR* strTextureName = L"pants.tga";
    FIXUP_DESC* pPantsTextureFixup = FindDynamicFixup( &ci.m_Body, strTextureName );
    if( pPantsTextureFixup != NULL )
    {
        // Apply a new texture using this dynamic fixup.
        if( m_bTextureVariant )
            strTextureName = L"pants-blue.tga";
        ATG::Texture2D* pTexture = ( ATG::Texture2D* )m_pScene->GetResourceDatabase()->FindResourceOfType(
            strTextureName, ATG::Texture2D::TypeID );
        if( pTexture != NULL )
        {
            ci.m_Body.pCommandBuffer->SetTexture( pPantsTextureFixup->dwFixupHandle, pTexture->GetD3DTexture() );
        }
    }

    // Search for a texture fixup for the jacket texture.
    strTextureName = L"jacket.tga";
    FIXUP_DESC* pJacketTextureFixup = FindDynamicFixup( &ci.m_Body, strTextureName );
    if( pJacketTextureFixup != NULL )
    {
        // Apply a new texture using this dynamic fixup.
        if( m_bTextureVariant )
            strTextureName = L"jacket-blue.tga";
        ATG::Texture2D* pTexture = ( ATG::Texture2D* )m_pScene->GetResourceDatabase()->FindResourceOfType(
            strTextureName, ATG::Texture2D::TypeID );
        if( pTexture != NULL )
        {
            ci.m_Body.pCommandBuffer->SetTexture( pJacketTextureFixup->dwFixupHandle, pTexture->GetD3DTexture() );
        }
    }

    // Search for a texture fixup for the upper body texture.
    strTextureName = L"upbodyc.tga";
    FIXUP_DESC* pUpBodyTextureFixup = FindDynamicFixup( &ci.m_Body, strTextureName );
    if( pUpBodyTextureFixup != NULL )
    {
        // Apply a new texture using this dynamic fixup.
        if( m_bTextureVariant )
            strTextureName = L"upbodyc-blue.tga";
        ATG::Texture2D* pTexture = ( ATG::Texture2D* )m_pScene->GetResourceDatabase()->FindResourceOfType(
            strTextureName, ATG::Texture2D::TypeID );
        if( pTexture != NULL )
        {
            ci.m_Body.pCommandBuffer->SetTexture( pUpBodyTextureFixup->dwFixupHandle, pTexture->GetD3DTexture() );
        }
    }

    // End dynamic fixups.  This allows the command buffer to be used for rendering
    // again.
    ci.m_Body.pCommandBuffer->EndDynamicFixups();
}
