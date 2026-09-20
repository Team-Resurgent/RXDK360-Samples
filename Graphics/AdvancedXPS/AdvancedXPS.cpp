//--------------------------------------------------------------------------------------
// AdvancedXPS.cpp
//
// This sample demonstrates the usage of XPS visibility surveys to accelerate XPS
// rendering in conjunction with predicated tiling.  In this case, 4x MSAA rendering is
// performed using 3 tiles, and the XPS visibility surveys ensure that XPS callbacks are
// only performed when the resulting geometry will be drawn on each tiling pass.
//
// The scene is a grassy field with several types of bushes.  The 4x MSAA rendertargets
// are used with alpha-to-coverage blending so that the billboards do not have to be
// depth sorted before rendering and appear in the proper Z-sorted order after all 
// billboards are rendered.  Several other aspects of the content and renderstate were 
// tuned to support alpha-to-coverage, such as the usage of anisotropic texture
// filtering for the grass, and the proper selection of a background color in the grass
// texture so the mipmaps would filter to a good average color in the smaller mips.
//
// XPS is ideal for scenes like this one since such high compression ratios can be
// achieved, and the intermediate geometry does not need to be stored in system memory.  
// The Seed structure defines a group of 3 quads, and is 8 bytes in memory.
// The XPS rendering code expands each Seed out into 12 vertices, each of which is 12 
// bytes.  This results in an 18:1 compression ratio, and would be even higher if the 
// quad vertices did not use compressed vertex element types.
//
// Microsoft Game Technology Group.
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
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgDebugDraw.h>
#include <AtgSimpleShaders.h>
#include <AtgCollision.h>
#include <vector>

// Forward declare the write barrier compiler intrinsic
extern "C"
    void _WriteBarrier();
#pragma intrinsic(_WriteBarrier)

// Forward declare the XPS rendering callback function
VOID RenderXPSGroundCluster( D3DXpsThread* pThreadContext,
                             VOID* pCallbackContext,
                             const VOID* pSubmitData,
                             DWORD InstanceIndex );

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_LEFTSTICK,      ATG::HELP_PLACEMENT_1, L"Move camera" },
    { ATG::HELP_RIGHTSTICK,     ATG::HELP_PLACEMENT_1, L"Rotate camera" },
    { ATG::HELP_START_BUTTON,   ATG::HELP_PLACEMENT_1, L"Reset camera" },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle visibility\nsurveys" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle frustum\nculling" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_2, L"Change XPS\nHW threads" },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_1, L"Toggle wind" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_1, L"Camera speed" },
    { ATG::HELP_MISC_CALLOUT,   ATG::HELP_PLACEMENT_1, L"Triggers move camera in/out" },
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))

//--------------------------------------------------------------------------------------
// Name: struct GroundObjectVertex
// Desc: Compressed vertex structure for billboard quads
//--------------------------------------------------------------------------------------
struct GroundObjectVertex
{
    XMHALF4 vPosition;
    XMHALF2 vTexCoord;
};
// We're using 128KB of L2 cache for XPS rendering, split between 4 threads, with 2 buffers per thread
const DWORD g_dwXPSRawBufferSize = ( 128 * 1024 ) / ( 4 * 2 );
// Each buffer includes ~140 bytes of command packet data, as well as 4 bytes of debug check data
const DWORD g_dwXPSActualBufferSize = g_dwXPSRawBufferSize - D3DXPS_COMMAND_SIZE - 4;
// The batch size is the XPS instance buffer size divided by 12 GroundObjectVertex structures (3 quads)
const DWORD g_dwXPSMaxBatchSize = g_dwXPSActualBufferSize / ( 12 * sizeof( GroundObjectVertex ) );
DWORD       g_dwXPSBatchSize = g_dwXPSMaxBatchSize;

//--------------------------------------------------------------------------------------
// Name: struct Seed
// Desc: Describes the location and state of a single 3-quad billboard set
//--------------------------------------------------------------------------------------
struct Seed
{
    XMDECN4 vPosition;
    XMHALF2 vRandomSeed;
};

//--------------------------------------------------------------------------------------
// Name: struct Cluster
// Desc: Describes a set of seeds and common state for generating and drawing the seeds
//--------------------------------------------------------------------------------------
struct Cluster
{
    XMFLOAT3 vOrigin;
    XMFLOAT3 vPositionScale;
    FLOAT fSize;
    FLOAT fSizeVariance;
    FLOAT fAspectRatio;
    FLOAT fTexURepeat;
    BOOL bLODEnabled;
    DWORD dwSeedCount;
    Seed* pSeeds;
    HANDLE hVisibilitySurvey;
    BOOL bVisible;
    ATG::AxisAlignedBox AABB;
};

//--------------------------------------------------------------------------------------
// Name: struct ClusterGroup
// Desc: Describes a set of clusters and the texture used to render the clusters
//--------------------------------------------------------------------------------------
struct ClusterGroup
{
    DWORD dwStartIndex;
    DWORD dwCount;
    D3DTexture* pTexture;
    DWORD dwMinMagSamplerMode;
};

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Derived class used to run the application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bShowHelp;
    BOOL m_bPerformVisibilitySurvey;
    BOOL m_bPerformFrustumCulling;
    BOOL m_dwXPSThreadUse;
    BOOL m_bApplyWind;

    XMMATRIX m_matView;
    XMMATRIX m_matProj;
    XMMATRIX m_matViewProj;
    XMVECTOR m_vCameraPos;
    ATG::Bound m_CameraFrustum;
    FLOAT m_fRotateY;
    FLOAT m_fRotateX;
    FLOAT m_fCameraDistance;
    XMVECTOR m_vCameraTarget;
    XMFLOAT4 m_vTime;
    XMFLOAT4 m_vWindVector;

    D3DSurface* m_pTiledRenderTarget;
    D3DSurface* m_pTiledDepthStencil;
    D3DSurface* m_pPostRenderTarget;
    D3DTexture* m_pSceneResolveTexture;
    D3DTexture  m_SceneResolveTextureAs16SRGB;
    D3DTexture* m_pFrontBuffer;

    D3DVertexDeclaration* m_pVertexDecl;
    D3DVertexShader* m_pBillboardVS;
    D3DPixelShader* m_pBillboardPS;
    D3DVertexShader* m_pGroundPlaneVS;
    D3DPixelShader* m_pGroundPlanePS;

    ATG::PackedResource m_Resources;
    D3DTexture* m_pGrassTexture;
    D3DTexture* m_pGroundTexture;
    D3DTexture* m_pBushTextures[6];
    D3DVertexBuffer* m_pSkyDomeVB;
    D3DTexture* m_pSkyDomeTexture;
    DWORD m_dwSkyDomePrimitiveCount;

    std::vector <ClusterGroup> m_ClusterGroups;
    std::vector <Cluster> m_Clusters;

private:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
    VOID    ResetCamera();
    DWORD   BuildClusterArray( DWORD dwRows, DWORD dwColumns, FLOAT fRowSpacing, FLOAT fColumnSpacing,
                               DWORD dwSeedCount, FLOAT fSeedSize, FLOAT fSeedAspectRatio, FLOAT fSeedSizeVariance,
                               FLOAT fTexURepeat, BOOL bLODEnabled );
    VOID    BuildClusterAABB( Cluster* pCluster );
    VOID    RenderClusterSurveyCube( Cluster* pCluster );
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    atgApp.m_dwDeviceCreationFlags |= D3DCREATE_CREATE_THREAD_ON_2 | D3DCREATE_CREATE_THREAD_ON_3 |
        D3DCREATE_CREATE_THREAD_ON_4 | D3DCREATE_CREATE_THREAD_ON_5 | D3DCREATE_BUFFER_2_FRAMES;
    atgApp.m_d3dpp.DisableAutoBackBuffer = TRUE;
    atgApp.m_d3dpp.DisableAutoFrontBuffer = TRUE;
    atgApp.m_d3dpp.EnableAutoDepthStencil = FALSE;
    atgApp.m_d3dpp.BackBufferWidth = 1280;
    atgApp.m_d3dpp.BackBufferHeight = 720;
    atgApp.m_d3dpp.BackBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the app
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bPerformVisibilitySurvey = TRUE;
    m_bPerformFrustumCulling = TRUE;
    m_dwXPSThreadUse = 0;
    m_bShowHelp = FALSE;
    m_bApplyWind = TRUE;
    m_vWindVector = XMFLOAT4( 1, 0, 0, 0 );
    m_vTime = XMFLOAT4( 0, 0, 0, 0 );
    ResetCamera();

    HRESULT hr;

    // Create render targets for 1280x720 4x MSAA rendering with predicated tiling,
    // followed by a 1280x720 no MSAA post effects pass
    D3DSURFACE_PARAMETERS SurfParams = { 0 };
    hr = m_pd3dDevice->CreateRenderTarget( 1280, 720, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_X8R8G8B8 ), D3DMULTISAMPLE_NONE, 0,
                                           FALSE, &m_pPostRenderTarget, &SurfParams );
    hr = m_pd3dDevice->CreateDepthStencilSurface( 1280, 256, D3DFMT_D24S8, D3DMULTISAMPLE_4_SAMPLES, 0,
                                                  FALSE, &m_pTiledDepthStencil, &SurfParams );
    SurfParams.Base = XGSurfaceSize( 1280, 256, D3DFMT_D24S8, D3DMULTISAMPLE_4_SAMPLES );
    hr = m_pd3dDevice->CreateRenderTarget( 1280, 256, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_X8R8G8B8 ), D3DMULTISAMPLE_4_SAMPLES, 0,
                                           FALSE, &m_pTiledRenderTarget, &SurfParams );

    // Create the scene resolve and front buffer textures
    hr = m_pd3dDevice->CreateTexture( 1280, 720, 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_X8R8G8B8 ), D3DPOOL_DEFAULT, &m_pSceneResolveTexture,
                                      NULL );
    hr = m_pd3dDevice->CreateTexture( 1280, 720, 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 ), D3DPOOL_DEFAULT, &m_pFrontBuffer, NULL );

    // Load textures from a packed resource file
    if FAILED( m_Resources.Create( "game:\\media\\Resource.xpr" ) )
    {
        ATG_PrintError( "Couldn't load textures\n" );
        return hr;
    }

    // Set up an alias of the resolve texture as an AS_16 sRGB format, since the GPU can't resolve 
    // to an AS_16_16_16_16 format. 
    // Alias this texture so there's no loss of precision when sampling the texture in the shader. If using
    // a standard SRGB format, the sRGB->Linear conversion happens with 8 bit precision, resulting 
    // in a loss of data. Using an AS_16 sRGB format causes the conversion to happen at 16-bit precision,
    // meaning no precision is lost.
    m_SceneResolveTextureAs16SRGB = *m_pSceneResolveTexture;
    ATG::ConvertTextureToAs16SRGBFormat( &m_SceneResolveTextureAs16SRGB );

    m_pGrassTexture = m_Resources.GetTexture( "GrassBlades" );
    m_pGroundTexture = m_Resources.GetTexture( "Ground" );
    m_pSkyDomeTexture = m_Resources.GetTexture( "SkyDome" );
    m_pBushTextures[0] = m_Resources.GetTexture( "Bush0" );
    m_pBushTextures[1] = m_Resources.GetTexture( "Bush1" );
    m_pBushTextures[2] = m_Resources.GetTexture( "Bush2" );
    m_pBushTextures[3] = m_Resources.GetTexture( "Bush3" );
    m_pBushTextures[4] = m_Resources.GetTexture( "Bush4" );
    m_pBushTextures[5] = m_Resources.GetTexture( "Bush5" );

    // Define the vertex elements.
    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,   0 },
        { 0, 8, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,   0 },
        D3DDECL_END()
    };

    // Create a vertex declaration from the element descriptions.
    m_pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pVertexDecl );

    // Create the font
    if( FAILED( hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't load font\n" );
        return hr;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create help screen
    if( FAILED( hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't load help\n" );
        return hr;
    }

    // Initialize simple shaders; they will be used for the ground plane and survey
    // rendering
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Load shaders
    if( FAILED( hr = ATG::LoadVertexShader( "game:\\media\\shaders\\VSWindWave.xvu", &m_pBillboardVS ) ) )
    {
        ATG_PrintError( "Couldn't load vertex shader\n" );
        return hr;
    }

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\media\\shaders\\PSBillboard.xpu", &m_pBillboardPS ) ) )
    {
        ATG_PrintError( "Couldn't load pixel shader\n" );
        return hr;
    }

    if( FAILED( hr = ATG::LoadVertexShader( "game:\\media\\shaders\\VSGroundPlane.xvu", &m_pGroundPlaneVS ) ) )
    {
        ATG_PrintError( "Couldn't load vertex shader\n" );
        return hr;
    }

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\media\\shaders\\PSGroundPlane.xpu", &m_pGroundPlanePS ) ) )
    {
        ATG_PrintError( "Couldn't load pixel shader\n" );
        return hr;
    }

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Initialize the view and projection transforms
    m_matView = m_matViewProj = XMMatrixIdentity();
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, 0.1f, 200.0f );

    // Seed the CRT random number generator using the CPU's time base register
    srand( __mftb32() );

    // Build clusters
    m_Clusters.reserve( 700 );
    DWORD dwGroupIndex = BuildClusterArray( 20, 20, 4.0f, 4.0f, 90, 0.15f, 6.0f, 0.05f, 6.0f, TRUE );
    m_ClusterGroups[dwGroupIndex].pTexture = m_pGrassTexture;
    m_ClusterGroups[dwGroupIndex].dwMinMagSamplerMode = D3DTEXF_ANISOTROPIC;

    dwGroupIndex = BuildClusterArray( 4, 4, 18.0f, 18.0f, 3, 1.2f, 0.6f, 0.1f, 1.0f, FALSE );
    m_ClusterGroups[dwGroupIndex].pTexture = m_pBushTextures[0];
    m_ClusterGroups[dwGroupIndex].dwMinMagSamplerMode = D3DTEXF_LINEAR;

    dwGroupIndex = BuildClusterArray( 4, 4, 18.0f, 18.0f, 6, 0.7f, 1.0f, 0.1f, 1.0f, FALSE );
    m_ClusterGroups[dwGroupIndex].pTexture = m_pBushTextures[1];
    m_ClusterGroups[dwGroupIndex].dwMinMagSamplerMode = D3DTEXF_LINEAR;

    dwGroupIndex = BuildClusterArray( 4, 4, 18.0f, 18.0f, 3, 0.3f, 1.0f, 0.1f, 1.0f, FALSE );
    m_ClusterGroups[dwGroupIndex].pTexture = m_pBushTextures[2];
    m_ClusterGroups[dwGroupIndex].dwMinMagSamplerMode = D3DTEXF_LINEAR;

    dwGroupIndex = BuildClusterArray( 4, 4, 18.0f, 18.0f, 3, 0.7f, 0.6f, 0.1f, 1.0f, FALSE );
    m_ClusterGroups[dwGroupIndex].pTexture = m_pBushTextures[3];
    m_ClusterGroups[dwGroupIndex].dwMinMagSamplerMode = D3DTEXF_ANISOTROPIC;

    dwGroupIndex = BuildClusterArray( 8, 8, 9.0f, 9.0f, 7, 0.20f, 1.0f, 0.2f, 1.0f, FALSE );
    m_ClusterGroups[dwGroupIndex].pTexture = m_pBushTextures[4];
    m_ClusterGroups[dwGroupIndex].dwMinMagSamplerMode = D3DTEXF_ANISOTROPIC;

    dwGroupIndex = BuildClusterArray( 8, 8, 9.0f, 9.0f, 7, 0.20f, 1.0f, 0.2f, 1.0f, FALSE );
    m_ClusterGroups[dwGroupIndex].pTexture = m_pBushTextures[5];
    m_ClusterGroups[dwGroupIndex].dwMinMagSamplerMode = D3DTEXF_ANISOTROPIC;

    // Build sky dome VB
    const DWORD dwXSegments = 10;
    const DWORD dwYSegments = 5;
    DWORD dwVertexCount = ( dwYSegments * 2 ) * ( dwXSegments + 1 );
    m_dwSkyDomePrimitiveCount = ( dwVertexCount - 2 );
    DWORD dwVBSize = dwVertexCount * ATG::MeshVertexPT::Size();
    hr = m_pd3dDevice->CreateVertexBuffer( dwVBSize, 0, 0, D3DPOOL_DEFAULT, &m_pSkyDomeVB, NULL );
    ATG::MeshVertexPT* pVertexData = NULL;
    m_pSkyDomeVB->Lock( 0, 0, ( VOID** )&pVertexData, 0 );
    const FLOAT fDomeRadius = 1.0f;
    const FLOAT fXDelta = XM_2PI / ( FLOAT )dwXSegments;
    const FLOAT fYDelta = XM_PIDIV2 / ( FLOAT )dwYSegments;
    for( DWORD dwYPos = 0; dwYPos < dwYSegments; ++dwYPos )
    {
        const FLOAT fYBottom = ( FLOAT )dwYPos * fYDelta;
        const FLOAT fYTop = fYBottom + fYDelta;
        for( DWORD dwXPos = 0; dwXPos <= dwXSegments; ++dwXPos )
        {
            const FLOAT fXPos = ( FLOAT )dwXPos * -fXDelta;
            pVertexData[0].Position.x = fDomeRadius * cosf( fXPos ) * cosf( fYBottom );
            pVertexData[0].Position.y = fDomeRadius * sinf( fYBottom );
            pVertexData[0].Position.z = fDomeRadius * sinf( fXPos ) * cosf( fYBottom );
            pVertexData[1].Position.x = fDomeRadius * cosf( fXPos ) * cosf( fYTop );
            pVertexData[1].Position.y = fDomeRadius * sinf( fYTop );
            pVertexData[1].Position.z = fDomeRadius * sinf( fXPos ) * cosf( fYTop );
            pVertexData[0].TexCoord.x = pVertexData[1].TexCoord.x = ( FLOAT )dwXPos / ( FLOAT )dwXSegments;
            pVertexData[0].TexCoord.y = 1.0f - ( FLOAT )dwYPos / ( FLOAT )dwYSegments;
            pVertexData[1].TexCoord.y = 1.0f - ( FLOAT )( dwYPos + 1 ) / ( FLOAT )dwYSegments;
            pVertexData += 2;
        }
    }
    m_pSkyDomeVB->Unlock();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame to update the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT fDeltaTime = ( FLOAT )m_Timer.GetElapsedTime();
    m_vTime.x = ( FLOAT )m_Timer.GetAppTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bShowHelp = !m_bShowHelp;
    }

    // The Start button resets the camera to its default settings
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        ResetCamera();
    }

    // The A button toggles visibility surveys for XPS rendering
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_bPerformVisibilitySurvey = !m_bPerformVisibilitySurvey;
        // If surveys have been disabled, clear all of the survey handles used on the
        // previous frame
        if( !m_bPerformVisibilitySurvey )
        {
            DWORD dwClusterCount = m_Clusters.size();
            for( DWORD i = 0; i < dwClusterCount; ++i )
            {
                m_Clusters[i].hVisibilitySurvey = NULL;
            }
        }
    }

    // The B button toggles CPU frustum culling before rendering
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_bPerformFrustumCulling = !m_bPerformFrustumCulling;
        // If frustum culling has been disabled, reset all visible flags to TRUE
        if( !m_bPerformFrustumCulling )
        {
            DWORD dwClusterCount = m_Clusters.size();
            for( DWORD i = 0; i < dwClusterCount; ++i )
            {
                m_Clusters[i].bVisible = TRUE;
            }
        }
    }

    // The X button changes the XPS HW thread selections
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_dwXPSThreadUse = ( m_dwXPSThreadUse + 1 ) % 4;
    }

    // The Y button toggles the wind effect on the foliage
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        m_bApplyWind = !m_bApplyWind;
        if( m_bApplyWind )
            m_vWindVector = XMFLOAT4( 1, 0, 0, 0 );
        else
            m_vWindVector = XMFLOAT4( 0, 0, 0, 0 );
    }

    // Update camera view matrix
    const FLOAT fRotateSpeed = XM_PIDIV2;
    FLOAT fDollySpeed = 2.0f;
    FLOAT fMoveSpeed = 1.0f;
    if( pGamepad->wLastButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        fDollySpeed *= 10.0f;
        fMoveSpeed *= 10.0f;
    }
    FLOAT fRotateYAmount = pGamepad->fX2 * fabs( pGamepad->fX2 ) * fRotateSpeed * fDeltaTime;
    FLOAT fRotateXAmount = pGamepad->fY2 * fabs( pGamepad->fY2 ) * fRotateSpeed * fDeltaTime;
    FLOAT fDistance = ( ( FLOAT )pGamepad->bLeftTrigger / 255.0f ) * fDollySpeed * fDeltaTime -
        ( ( FLOAT )pGamepad->bRightTrigger / 255.0f ) * fDollySpeed * fDeltaTime;
    m_fRotateX += fRotateXAmount;
    m_fRotateY += fRotateYAmount;
    m_fCameraDistance += fDistance;
    m_fRotateX = max( m_fRotateX, 0.0f );
    m_fRotateX = min( m_fRotateX, 1.56f );
    m_fCameraDistance = max( m_fCameraDistance, 1.3f );
    m_fCameraDistance = min( m_fCameraDistance, 200.0f );
    static const XMVECTOR vAxisX = { 1, 0, 0, 0 };
    static const XMVECTOR vAxisY = { 0, 1, 0, 0 };
    XMVECTOR qRotateX = XMQuaternionRotationAxis( vAxisX, m_fRotateX );
    XMVECTOR qRotateY = XMQuaternionRotationAxis( vAxisY, m_fRotateY );
    XMVECTOR qRotation = XMQuaternionMultiply( qRotateX, qRotateY );
    XMMATRIX matRotation = XMMatrixRotationQuaternion( qRotation );
    XMMATRIX matView = XMMatrixTranslation( 0, 0, -m_fCameraDistance );
    matView *= matRotation;

    XMVECTOR vCameraForward = matView.r[2];
    XMVECTOR vCameraRight = matView.r[0];
    static const XMVECTOR vSelectY = XMVectorSelectControl( 0, 1, 0, 0 );
    vCameraForward = XMVectorSelect( vCameraForward, XMVectorZero(), vSelectY );
    vCameraRight = XMVectorSelect( vCameraRight, XMVectorZero(), vSelectY );
    vCameraForward = XMVector3NormalizeEst( vCameraForward );
    vCameraRight = XMVector3NormalizeEst( vCameraRight );

    XMVECTOR vTargetDelta = XMVectorSet( pGamepad->fX1 * fMoveSpeed * fDeltaTime, 0, pGamepad->fY1 * fMoveSpeed *
                                         fDeltaTime, 0 );
    m_vCameraTarget += vCameraForward * XMVectorSplatZ( vTargetDelta );
    m_vCameraTarget += vCameraRight * XMVectorSplatX( vTargetDelta );
    static const XMVECTOR vCameraMinBounds = { -60, 0, -60, 0 };
    static const XMVECTOR vCameraMaxBounds = { 60, 0, 60, 0 };
    m_vCameraTarget = XMVectorClamp( m_vCameraTarget, vCameraMinBounds, vCameraMaxBounds );

    matView *= XMMatrixTranslationFromVector( m_vCameraTarget );

    ATG::Frustum CameraFrustum;
    ATG::ComputeFrustumFromProjection( &CameraFrustum, &m_matProj );
    m_CameraFrustum.SetFrustum( CameraFrustum );
    m_CameraFrustum = m_CameraFrustum * matView;

    m_matView = XMMatrixInverse( &qRotation, matView );
    m_vCameraPos = matView.r[3];

    m_matViewProj = XMMatrixMultiply( m_matView, m_matProj );
    ATG::DebugDraw::SetViewProjection( m_matViewProj );

    // Perform CPU frustum culling
    if( m_bPerformFrustumCulling )
    {
        DWORD dwClusterCount = m_Clusters.size();
        for( DWORD i = 0; i < dwClusterCount; ++i )
        {
            // Test each cluster against the camera frustum and save the result
            BOOL bVisible = m_CameraFrustum.Collide( m_Clusters[i].AABB );
            m_Clusters[i].bVisible = bVisible;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame to render the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Set the tiling rendertargets
    m_pd3dDevice->SetRenderTarget( 0, m_pTiledRenderTarget );
    m_pd3dDevice->SetDepthStencilSurface( m_pTiledDepthStencil );

    const D3DRECT TilingRects[] =
    {
        { 0,   0, 1280, 256 },
        { 0, 256, 1280, 512 },
        { 0, 512, 1280, 720 }
    };
    const DWORD dwTileCount = ARRAYSIZE( TilingRects );

    // Begin tiling
    D3DVECTOR4 ClearColor = { 0.2588f, 0.2274f, 0.0588f, 0.0f };
    m_pd3dDevice->BeginTiling( 0, dwTileCount, TilingRects, &ClearColor, 1.0f, 0 );

    // Render sky dome
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    // Remove the translation component from the view matrix for drawing the sky dome
    XMMATRIX matSkyDome = m_matView;
    matSkyDome.r[3] = XMQuaternionIdentity();
    matSkyDome *= m_matProj;
    ATG::SimpleShaders::BeginShader_Transformed_Textured( matSkyDome, m_pSkyDomeTexture );
    ATG::SimpleShaders::SetDeclPosTex();
    m_pd3dDevice->SetStreamSource( 0, m_pSkyDomeVB, 0, ATG::MeshVertexPT::Size() );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, m_dwSkyDomePrimitiveCount );
    ATG::SimpleShaders::EndShader();
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );

    // Set lighting pixel shader constants
    XMVECTOR vDirLightInvDir = { 0.935f, 0, -0.35f, 0 };
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&vDirLightInvDir, 1 );
    XMVECTOR vDirLightColor = { 1.0f, 0.9f, 0.8f, 0 };
    m_pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&vDirLightColor, 1 );

    // Render a ground plane
    ATG::SimpleShaders::SetDeclPosTex();
    m_pd3dDevice->SetStreamSource( 0, NULL, 0, ATG::MeshVertexPT::Size() );
    m_pd3dDevice->SetVertexShader( m_pGroundPlaneVS );
    m_pd3dDevice->SetPixelShader( m_pGroundPlanePS );
    XMMATRIX matViewProjT = XMMatrixTranspose( m_matViewProj );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matViewProjT, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 6, ( FLOAT* )&m_vCameraPos, 1 );
    m_pd3dDevice->SetTexture( 0, m_pGroundTexture );

    FLOAT fGroundSizeX = 100.0f;
    FLOAT fGroundSizeY = 100.0f;
    FLOAT fTextureSize = 1.0f;
    ATG::MeshVertexPT* pVerts = NULL;
    m_pd3dDevice->BeginVertices( D3DPT_QUADLIST, 4, ATG::MeshVertexPT::Size(), ( VOID** )&pVerts );
    pVerts[0].Position = XMFLOAT3( -fGroundSizeX, 0, fGroundSizeY );
    pVerts[0].TexCoord = XMFLOAT2( 0, 0 );
    pVerts[1].Position = XMFLOAT3( fGroundSizeX, 0, fGroundSizeY );
    pVerts[1].TexCoord = XMFLOAT2( 2 * fGroundSizeX / fTextureSize, 0 );
    pVerts[2].Position = XMFLOAT3( fGroundSizeX, 0, -fGroundSizeY );
    pVerts[2].TexCoord = XMFLOAT2( 2 * fGroundSizeX / fTextureSize, 2 * fGroundSizeY / fTextureSize );
    pVerts[3].Position = XMFLOAT3( -fGroundSizeX, 0, -fGroundSizeY );
    pVerts[3].TexCoord = XMFLOAT2( 0, 2 * fGroundSizeY / fTextureSize );
    m_pd3dDevice->EndVertices();

    // Render visibility surveys for XPS rendering
    if( m_bPerformVisibilitySurvey )
    {
        // XPS visibility surveys must use the D3DSEQM_CULLED query mode
        m_pd3dDevice->SetScreenExtentQueryMode( D3DSEQM_CULLED );
        DWORD dwClusterCount = m_Clusters.size();
        for( DWORD i = 0; i < dwClusterCount; ++i )
        {
            if( m_Clusters[i].bVisible )
            {
                RenderClusterSurveyCube( &m_Clusters[i] );
            }
        }
        // Reset the query mode to default (D3DSEQM_PRECLIP)
        m_pd3dDevice->SetScreenExtentQueryMode( D3DSEQM_PRECLIP );
    }

    // Based on m_dwXPSThreadUse, select a combination of HW threads for XPS to use
    DWORD dwXPSFlags = 0;
    switch( m_dwXPSThreadUse )
    {
        case 0:
            dwXPSFlags = D3DXPS_CPU5;
        case 1:
            dwXPSFlags |= D3DXPS_CPU4;
        case 2:
            dwXPSFlags |= D3DXPS_CPU3;
        case 3:
            dwXPSFlags |= D3DXPS_CPU2;
            break;
    }
    // Begin XPS rendering
    m_pd3dDevice->XpsBegin( dwXPSFlags );

    // Set the XPS callback
    m_pd3dDevice->XpsSetCallback( RenderXPSGroundCluster, &m_vCameraPos, 0 );

    // Set up renderstate for XPS rendering
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    m_pd3dDevice->SetStreamSource( 0, NULL, 0, sizeof( GroundObjectVertex ) );
    m_pd3dDevice->SetVertexShader( m_pBillboardVS );
    m_pd3dDevice->SetPixelShader( m_pBillboardPS );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matViewProjT, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&m_vWindVector, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 5, ( FLOAT* )&m_vTime, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 6, ( FLOAT* )&m_vCameraPos, 1 );

    // Billboards render double sided, so turn off backface culling
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    // Set up texture wrapping/clamping for billboards (no vertical wrapping)
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    // Enable alpha testing and Z writing
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    // Enable alpha-to-mask with dithering so the billboards don't have to be depth sorted
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATOMASKENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATOMASKOFFSETS, D3DALPHATOMASK_DITHERED );

    // Loop over the cluster groups
    DWORD dwGroupCount = m_ClusterGroups.size();
    for( DWORD dwGroup = 0; dwGroup < dwGroupCount; ++dwGroup )
    {
        // Set up the texture and sampler state for the cluster group
        ClusterGroup* pGroup = &m_ClusterGroups[dwGroup];
        m_pd3dDevice->SetTexture( 0, pGroup->pTexture );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, pGroup->dwMinMagSamplerMode );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, pGroup->dwMinMagSamplerMode );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAXANISOTROPY, 6 );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

        // Render all of the clusters in the group using XPS
        DWORD dwClusterStart = pGroup->dwStartIndex;
        DWORD dwClusterEnd = dwClusterStart + pGroup->dwCount;
        for( DWORD dwClusterIndex = dwClusterStart; dwClusterIndex < dwClusterEnd; ++dwClusterIndex )
        {
            if( m_Clusters[dwClusterIndex].bVisible )
            {
                // Set the XPS predication handle from the visibility survey performed earlier
                m_pd3dDevice->XpsSetPredicationFromVisibility( m_Clusters[dwClusterIndex].hVisibilitySurvey );
                DWORD dwBatchCount = ( m_Clusters[dwClusterIndex].dwSeedCount + g_dwXPSBatchSize - 1 ) /
                    g_dwXPSBatchSize;
                m_pd3dDevice->XpsSubmit( dwBatchCount, &m_Clusters[dwClusterIndex], sizeof( Cluster ) );
            }
        }
    }

    // End XPS rendering
    m_pd3dDevice->XpsEnd();

    // End tiling, and resolve tiles to m_pSceneResolveTexture
    m_pd3dDevice->EndTiling( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARRENDERTARGET | D3DRESOLVE_CLEARDEPTHSTENCIL,
                             NULL, m_pSceneResolveTexture, &ClearColor, 1.0f, 0, NULL );

    // Set up rendering for post effects
    m_pd3dDevice->SetRenderTarget( 0, m_pPostRenderTarget );
    m_pd3dDevice->SetDepthStencilSurface( NULL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATOMASKENABLE, FALSE );

    // Restore the scene rendering to the rendertarget, using the high-precision texture to sample from.
    const D3DRECT ScreenRect = { 0, 0, 1280, 720 };
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( ScreenRect, &m_SceneResolveTextureAs16SRGB );

    // Draw UI
    m_Timer.MarkFrame();
    if( m_bShowHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"AdvancedXPS" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.SetScaleFactors( 0.9f, 0.9f );
        WCHAR strText[256];
        swprintf_s( strText, L"Perform XPS Visibility Surveys: %s", m_bPerformVisibilitySurvey ? L"True" : L"False" );
        m_Font.DrawText( 0, 30, 0xffffffff, strText );
        swprintf_s( strText, L"Perform Frustum Culling: %s", m_bPerformFrustumCulling ? L"True" : L"False" );
        m_Font.DrawText( 0, 55, 0xffffffff, strText );
        const WCHAR* strThreadList = L"";
        switch( m_dwXPSThreadUse )
        {
            case 0:
                strThreadList = L"2 3 4 5";
                break;
            case 1:
                strThreadList = L"2 3 4";
                break;
            case 2:
                strThreadList = L"2 3";
                break;
            case 3:
                strThreadList = L"2";
                break;
        }
        swprintf_s( strText, L"XPS HW Threads In Use: %s", strThreadList );
        m_Font.DrawText( 0, 80, 0xffffffff, strText );
        m_Font.End();
    }

    // Sync, resolve to front buffer, and swap
    m_pd3dDevice->SynchronizeToPresentationInterval();
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pFrontBuffer, NULL, 0, 0, NULL, 1.0f, 0, NULL );
    m_pd3dDevice->Swap( m_pFrontBuffer, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: BuildClusterArray()
// Desc: Creates an array of ground cluster objects, each with a certain count of seeds.
//--------------------------------------------------------------------------------------
DWORD Sample::BuildClusterArray( DWORD dwRows, DWORD dwColumns, FLOAT fRowSpacing, FLOAT fColumnSpacing,
                                 DWORD dwSeedCount, FLOAT fSeedSize, FLOAT fSeedAspectRatio, FLOAT fSeedSizeVariance,
                                 FLOAT fTexURepeat, BOOL bLODEnabled )
{
    static const XMVECTOR vSelectYW = XMVectorSelectControl( 0, 1, 0, 1 );

    // Create a cluster group, which is used to locate the cluster objects that we are
    // about to create
    ClusterGroup NewClusterGroup;
    NewClusterGroup.dwStartIndex = ( DWORD )m_Clusters.size();
    NewClusterGroup.dwCount = dwColumns * dwRows;
    NewClusterGroup.dwMinMagSamplerMode = 0;
    NewClusterGroup.pTexture = NULL;

    // Build the array of cluster objects
    FLOAT fXCenter = fColumnSpacing * ( FLOAT )( dwColumns - 1 ) * 0.5f;
    FLOAT fZCenter = fRowSpacing * ( FLOAT )( dwRows - 1 ) * 0.5f;
    for( DWORD dwRow = 0; dwRow < dwRows; ++dwRow )
    {
        for( DWORD dwColumn = 0; dwColumn < dwColumns; ++dwColumn )
        {
            Cluster NewCluster;
            NewCluster.vOrigin = XMFLOAT3( ( FLOAT )dwColumn * fColumnSpacing - fXCenter,
                                           0, ( FLOAT )dwRow * fRowSpacing - fZCenter );
            NewCluster.vPositionScale = XMFLOAT3( fColumnSpacing * 0.5f, 0, fRowSpacing * 0.5f );
            NewCluster.dwSeedCount = dwSeedCount;
            Seed* pSeeds = new Seed[dwSeedCount];
            NewCluster.pSeeds = pSeeds;
            NewCluster.fSize = fSeedSize;
            NewCluster.fSizeVariance = fSeedSizeVariance;
            NewCluster.fAspectRatio = fSeedAspectRatio;
            NewCluster.fTexURepeat = fTexURepeat;
            NewCluster.bLODEnabled = bLODEnabled;
            NewCluster.bVisible = TRUE;
            NewCluster.hVisibilitySurvey = NULL;
            BuildClusterAABB( &NewCluster );
            m_Clusters.push_back( NewCluster );

            // Create an array of seeds
            for( DWORD dwSeed = 0; dwSeed < dwSeedCount; ++dwSeed )
            {
                static const XMVECTOR vRandMax = XMVectorReplicate( ( FLOAT )RAND_MAX );
                static const XMVECTOR vTwo = XMVectorReplicate( 2.0f );
                static const XMVECTOR vOne = XMVectorReplicate( 1.0f );
                XMVECTOR vPos = XMVectorSet( ( FLOAT )rand(), 0, ( FLOAT )rand(), 0 );
                vPos = ( vPos / vRandMax ) * vTwo - vOne;
                vPos = XMVectorSelect( vPos, XMVectorZero(), vSelectYW );

                XMStoreDecN4( &pSeeds[dwSeed].vPosition, vPos );
                FLOAT fRandomSeed = ( FLOAT )rand() / ( FLOAT )RAND_MAX;
                XMHALF2 vRandomSeed( fRandomSeed, 0 );
                pSeeds[dwSeed].vRandomSeed = vRandomSeed;
            }
        }
    }

    // Add the cluster group to the cluster group vector
    DWORD dwGroupIndex = ( DWORD )m_ClusterGroups.size();
    m_ClusterGroups.push_back( NewClusterGroup );
    return dwGroupIndex;
}


//--------------------------------------------------------------------------------------
// Name: BuildClusterAABB()
// Desc: Creates an axis-aligned bounding box that contains all of the seeds in a
//       cluster object
//--------------------------------------------------------------------------------------
VOID Sample::BuildClusterAABB( Cluster* pCluster )
{
    FLOAT fSizeY = pCluster->fSize + pCluster->fSizeVariance;
    FLOAT fSizeX = fSizeY * pCluster->fAspectRatio;
    XMFLOAT3 vTotalSize = pCluster->vPositionScale;
    vTotalSize.x += fSizeX;
    vTotalSize.z += fSizeX;
    vTotalSize.y = fSizeY * 0.5f;
    XMFLOAT3 vOrigin = pCluster->vOrigin;
    vOrigin.y += fSizeY * 0.5f;
    pCluster->AABB.Center = vOrigin;
    pCluster->AABB.Extents = vTotalSize;
}


//--------------------------------------------------------------------------------------
// Name: ResetCamera()
// Desc: Resets the camera to its default viewpoint in the scene
//--------------------------------------------------------------------------------------
VOID Sample::ResetCamera()
{
    m_fRotateY = 1.675f;
    m_fRotateX = 0.10f;
    m_fCameraDistance = 3.5f;
    m_vCameraTarget = XMVectorSet( -10, 0, 10, 0 );
}


//--------------------------------------------------------------------------------------
// Name: RenderClusterSurveyCube()
// Desc: Renders a visibility survey object that will be used to execute or skip XPS
//       rendering for the given cluster on each tiling pass during rendering
//--------------------------------------------------------------------------------------
VOID Sample::RenderClusterSurveyCube( Cluster* pCluster )
{
    // The visibility survey handle is stored in the cluster object
    // We don't need to double-buffer since these handles are generated and consumed in
    // the same frame - they're just tracking values that Direct3D will embed into the
    // command buffer
    pCluster->hVisibilitySurvey = m_pd3dDevice->BeginVisibilitySurvey( D3DSURVEYBEGIN_CULLGEOMETRY );
    ATG::DebugDraw::DrawCubeQuery( pCluster->AABB.Center, pCluster->AABB.Extents );
    m_pd3dDevice->EndVisibilitySurvey( pCluster->hVisibilitySurvey );
}


//--------------------------------------------------------------------------------------
// Name: RenderXPSGroundCluster()
// Desc: An XPS callback routine that draws each seed inside of a cluster
//       One instance of this routine will run on each XPS thread
//--------------------------------------------------------------------------------------
VOID RenderXPSGroundCluster( D3DXpsThread* pThreadContext,
                             VOID* pCallbackContext,
                             const VOID* pSubmitData,
                             DWORD InstanceIndex )
{
    D3DXps xps( pThreadContext );

    // Set up some useful XMVECTOR constants
    static XMVECTOR vCornerUV0 = { 0, 0, 0, 0 };
    static XMVECTOR vCornerUV1 = { 1, 0, 0, 0 };
    static XMVECTOR vCornerUV2 = { 1, 1, 0, 0 };
    static XMVECTOR vCornerUV3 = { 0, 1, 0, 0 };
    static const XMVECTOR vUp = { 0, 1, 0, 0 };

    // LOD factors for drawing seeds, these were chosen to best match the scene content
    // and the expected camera angles
    static const XMVECTOR vLODMin = XMVectorReplicate( 6.0f );
    static const XMVECTOR vLODMax = XMVectorReplicate( 11.0f );
    static const XMVECTOR vOne = { 1, 1, 1, 1 };
    static const XMVECTOR vLODDropFactor = XMVectorReplicate( 0.70f );

    // The callback context is simply the camera location
    const XMVECTOR vCameraPos = *( const XMVECTOR* )pCallbackContext;

    // Loop over the XPS instances for this thread
    do
    {
        const Cluster* pCluster = ( const Cluster* )pSubmitData;

        // Load data from the cluster into vector registers
        const XMVECTOR vClusterOrigin = XMLoadFloat3( &pCluster->vOrigin );
        const XMVECTOR vClusterPositionScale = XMLoadFloat3( &pCluster->vPositionScale );
        DWORD dwTotalSeedCount = pCluster->dwSeedCount;
        const XMVECTOR vBaseSize = XMVectorReplicate( pCluster->fSize );
        const XMVECTOR vAspect = XMVectorReplicate( pCluster->fAspectRatio );
        const XMVECTOR vSizeVariance = XMVectorReplicate( pCluster->fSizeVariance );
        const XMVECTOR vTexUVRepeat = XMVectorSet( pCluster->fTexURepeat, 1.0f, 0.0f, 0.0f );

        // If enabled for this cluster, use the LOD settings to reduce the seed count
        if( pCluster->bLODEnabled )
        {
            XMVECTOR vDistanceToCamera = XMVector3LengthEst( vClusterOrigin - vCameraPos );
            vDistanceToCamera = XMVectorClamp( vDistanceToCamera, vLODMin, vLODMax );
            XMVECTOR vPercent = ( vDistanceToCamera - vLODMin ) / ( vLODMax - vLODMin );
            vPercent = vOne - ( vPercent * vLODDropFactor );
            dwTotalSeedCount = ( DWORD )( vPercent.x * ( FLOAT )dwTotalSeedCount );
        }

        DWORD dwSeedStart = InstanceIndex * g_dwXPSBatchSize;
        if( dwSeedStart >= dwTotalSeedCount )
        {
            xps.Allocate( 0 );
            continue;
        }
        DWORD dwSeedEnd = dwSeedStart + g_dwXPSBatchSize;
        dwSeedEnd = min( dwSeedEnd, dwTotalSeedCount );
        DWORD dwSeedCount = dwSeedEnd - dwSeedStart;
        const Seed* pCurrentSeed = &pCluster->pSeeds[dwSeedStart];

        // Create the XPS allocation in the locked L2 cache section for the seed vertices
        // Note that the type of draw command is specified when we make the allocation,
        // since the command data also consumes a piece of the L2 section
        const DWORD dwVertexCount = dwSeedCount * ( 3 * 4 );
        GroundObjectVertex* pVertices = ( GroundObjectVertex* )xps.Allocate( dwVertexCount * sizeof
                                                                             ( GroundObjectVertex ),
                                                                             D3DXPS_DRAWVERTICES_SIZE );
        GroundObjectVertex* pCurrentVertex = pVertices;

        // Loop over the seeds
        for( DWORD i = 0; i < dwSeedCount; ++i )
        {
            // Build three quads for each seed, at 120 degree angles from each other
            XMVECTOR vSeedOrigin = XMLoadDecN4( &pCurrentSeed->vPosition );
            vSeedOrigin *= vClusterPositionScale;
            vSeedOrigin += vClusterOrigin;
            XMVECTOR vSeedNormal = vUp;

            XMVECTOR vRandomSeed = XMVectorSplatX( XMLoadHalf2( &pCurrentSeed->vRandomSeed ) );

            XMVECTOR qRotation = XMQuaternionRotationNormal( vSeedNormal, vRandomSeed.x );
            XMMATRIX matTransform = XMMatrixRotationQuaternion( qRotation );

            static const XMVECTOR vAxis0PreTransform = { 0, 0, -1, 0 };
            static const XMVECTOR vAxis1PreTransform = { 0.8660f, 0, -0.5f, 0 };
            static const XMVECTOR vAxis2PreTransform = { 0.8660f, 0, 0.5f, 0 };

            XMVECTOR vAxis0 = XMVector3TransformNormal( vAxis0PreTransform, matTransform );
            XMVECTOR vAxis1 = XMVector3TransformNormal( vAxis1PreTransform, matTransform );
            XMVECTOR vAxis2 = XMVector3TransformNormal( vAxis2PreTransform, matTransform );

            XMVECTOR vSizeY = vBaseSize + ( vSizeVariance * vRandomSeed );
            XMVECTOR vSizeX = vSizeY * vAspect;

            vAxis0 *= vSizeX;
            vAxis1 *= vSizeX;
            vAxis2 *= vSizeX;
            vSeedNormal *= vSizeY;

            XMVECTOR vCorner0_0 = ( vSeedOrigin - vAxis0 + vSeedNormal );
            XMVECTOR vCorner0_1 = ( vSeedOrigin + vAxis0 + vSeedNormal );
            XMVECTOR vCorner0_2 = ( vSeedOrigin + vAxis0 );
            XMVECTOR vCorner0_3 = ( vSeedOrigin - vAxis0 );

            XMVECTOR vCorner1_0 = ( vSeedOrigin - vAxis1 + vSeedNormal );
            XMVECTOR vCorner1_1 = ( vSeedOrigin + vAxis1 + vSeedNormal );
            XMVECTOR vCorner1_2 = ( vSeedOrigin + vAxis1 );
            XMVECTOR vCorner1_3 = ( vSeedOrigin - vAxis1 );

            XMVECTOR vCorner2_0 = ( vSeedOrigin - vAxis2 + vSeedNormal );
            XMVECTOR vCorner2_1 = ( vSeedOrigin + vAxis2 + vSeedNormal );
            XMVECTOR vCorner2_2 = ( vSeedOrigin + vAxis2 );
            XMVECTOR vCorner2_3 = ( vSeedOrigin - vAxis2 );

            // Store the twelve vertices into the XPS vertex buffer
            XMStoreHalf4( &pCurrentVertex[0].vPosition, vCorner0_0 );
            XMStoreHalf2( &pCurrentVertex[0].vTexCoord, vCornerUV0 * vTexUVRepeat );
            XMStoreHalf4( &pCurrentVertex[1].vPosition, vCorner0_1 );
            XMStoreHalf2( &pCurrentVertex[1].vTexCoord, vCornerUV1 * vTexUVRepeat );
            XMStoreHalf4( &pCurrentVertex[2].vPosition, vCorner0_2 );
            XMStoreHalf2( &pCurrentVertex[2].vTexCoord, vCornerUV2 * vTexUVRepeat );
            XMStoreHalf4( &pCurrentVertex[3].vPosition, vCorner0_3 );
            XMStoreHalf2( &pCurrentVertex[3].vTexCoord, vCornerUV3 * vTexUVRepeat );

            XMStoreHalf4( &pCurrentVertex[4].vPosition, vCorner1_0 );
            XMStoreHalf2( &pCurrentVertex[4].vTexCoord, vCornerUV0 * vTexUVRepeat );
            XMStoreHalf4( &pCurrentVertex[5].vPosition, vCorner1_1 );
            XMStoreHalf2( &pCurrentVertex[5].vTexCoord, vCornerUV1 * vTexUVRepeat );
            XMStoreHalf4( &pCurrentVertex[6].vPosition, vCorner1_2 );
            XMStoreHalf2( &pCurrentVertex[6].vTexCoord, vCornerUV2 * vTexUVRepeat );
            XMStoreHalf4( &pCurrentVertex[7].vPosition, vCorner1_3 );
            XMStoreHalf2( &pCurrentVertex[7].vTexCoord, vCornerUV3 * vTexUVRepeat );

            XMStoreHalf4( &pCurrentVertex[8].vPosition, vCorner2_0 );
            XMStoreHalf2( &pCurrentVertex[8].vTexCoord, vCornerUV0 * vTexUVRepeat );
            XMStoreHalf4( &pCurrentVertex[9].vPosition, vCorner2_1 );
            XMStoreHalf2( &pCurrentVertex[9].vTexCoord, vCornerUV1 * vTexUVRepeat );
            XMStoreHalf4( &pCurrentVertex[10].vPosition, vCorner2_2 );
            XMStoreHalf2( &pCurrentVertex[10].vTexCoord, vCornerUV2 * vTexUVRepeat );
            XMStoreHalf4( &pCurrentVertex[11].vPosition, vCorner2_3 );
            XMStoreHalf2( &pCurrentVertex[11].vTexCoord, vCornerUV3 * vTexUVRepeat );

            pCurrentVertex += 12;
            ++pCurrentSeed;
        }

        // Draw the quads
        xps.DrawVertices( D3DPT_QUADLIST, dwVertexCount, pVertices );

        // Get the next XPS instance
    } while( xps.KickOffAndGet( &InstanceIndex ) );
}
