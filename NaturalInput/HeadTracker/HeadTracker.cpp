//--------------------------------------------------------------------------------------
// HeadTracker.cpp
//
// This sample demonstrates how to move a camera based on a virtual head.
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xnamath.h>
#include <xffb.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgDebugDraw.h>
#include <AtgSimpleShaders.h>
#include <AtgResource.h>
#include <AtgSceneAll.h>
#include <AtgNuiJointFilter.h>

#include "CameraManager.h"

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );
static const XMVECTOR g_vFacing = XMVectorSet ( 0.0f, 0.0f, 1.0f, 0.0f );
static const FLOAT g_fWorldScale = 100.0f;  // used for drawing the Sky Dome.
static const XMFLOAT2 g_fSceneWorldXYMin = XMFLOAT2( -0.5f, -0.5f );
static const XMFLOAT2 g_fSceneWorldXYMax = XMFLOAT2( 0.5f, 0.5f );
static const FLOAT g_fMoveHeadUpTVSpace = 0.002f; 

static const FLOAT g_fScreenSizeMeters = 1.0f; // this is about the size of a 42 inch TV
static const XMVECTOR g_fScreenScale = XMVectorSet( 1.0f / g_fScreenSizeMeters, 1.0f / g_fScreenSizeMeters, 1.0f, 1.0f ); 
static const FLOAT g_fMaxDeviationFromUpVector = 0.98f;
static const INT g_iIterationsToAverageHeadOver = 100;
static const FLOAT g_fMaxHeadMovementTVSpaceBeforeReset = 0.1f;
static const FLOAT g_fUpdateHeadHeightRate = 0.95f;
static const FLOAT g_fUpdateHeadHeightRateLerp = 1.0f - g_fUpdateHeadHeightRate;

//--------------------------------------------------------------------------------------
// Vertex shader for the Environment
//--------------------------------------------------------------------------------------
const CHAR* m_strVertexShaderProgram =
    " float4x4 matWVP : register(c0);              \n"
    " float4x4 g_matWorld : register(c4);          \n"
    "                                              \n"
    " struct VS_IN                                 \n"
    "                                              \n"
    " {                                            \n"
    "     float4 ObjPos   : POSITION;              \n"
    "     float2 Texture  : TEXCOORD0;             \n"
    "     float3 Normal   : NORMAL;                \n"
    " };                                           \n"
    "                                              \n"
    " struct VS_OUT                                \n"
    " {                                            \n"
    "     float4 ProjPos  : POSITION;              \n"
    "     float2 Texture  : TEXCOORD0;             \n"
    "     float3 Normal   : TEXCOORD1;             \n"
    "     float  Alpha   : TEXCOORD3;              \n"
    " };                                           \n"
    "                                              \n"
    " VS_OUT main( VS_IN In )                      \n"
    " {                                            \n"
    "     VS_OUT Out;                              \n"
    "     Out.ProjPos = mul( matWVP, In.ObjPos );  \n"
    "     Out.Normal = In.Normal;                  \n"
    "     Out.Texture = In.Texture;                \n"
    "     Out.Alpha = 1.0f; \n"
    // Fade out the geometry. That's a looken nice.
    "     float fMaxZValue = 1.2f;                             \n"
    "     if ( In.ObjPos.z < -fMaxZValue ) Out.Alpha = 1.0f -  \n"
    "      ( ( -In.ObjPos.z - fMaxZValue )  / fMaxZValue );    \n"
    "     return Out;                                          \n"
    " }                                                        \n";


//-------------------------------------------------------------------------------------
// Atrium Pixel shader
//-------------------------------------------------------------------------------------
const CHAR* m_strPixelShaderProgramScene =
    " struct PS_IN                                                          \n"
    " {                                                                     \n"
    "     float2 Texture : TEXCOORD0;                                       \n"
    "     float3 vNormal : TEXCOORD1;                                       \n"
    "     float  Alpha   : TEXCOORD3;                                       \n"
    " };                                                                    \n"
    "                                                                       \n"
    " sampler TextureSampler0 : register(s0);                               \n"
    "                                                                       \n"
    " float4 main( PS_IN In ) : COLOR                                       \n"
    " {                                                                     \n"
    "     float4 textureColor = tex2D( TextureSampler0, In.Texture );       \n"
    "     textureColor *= In.Alpha;                                        \n"
    "     float3 vLightDir1 = float3( -1.0f, 1.0f, -1.0f );                 \n"
    "     float3 vLightDir2 = float3( 1.0f, 1.0f, -1.0f );                  \n"
    "     float3 vLightDir3 = float3( 0.0f, -1.0f, 0.0f );                  \n"
    "     float3 vLightDir4 = float3( 1.0f, 1.0f, 1.0f );                   \n" 
    "     float fLighting = 0.1f +                                          \n"
    "                  saturate( dot( vLightDir1 , In.vNormal ) )*0.25f +   \n"
    "                  saturate( dot( vLightDir2 , In.vNormal ) )*0.25f +   \n"
    "                  saturate( dot( vLightDir3 , In.vNormal ) )*0.25f +   \n"
    "                  saturate( dot( vLightDir4 , In.vNormal ) )*0.25f ;   \n"
    " return textureColor * fLighting;                                      \n"
    " }                                                                     \n";




//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;    // Timer
    ATG::Font m_Font;     // Font for drawing text
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    ATG::PackedResource m_Resource; // Bundled textures in a packed resource

    ATG::Scene* m_pHeadTrackerScene;
    XMFLOAT3 m_fPosition;

    // Transform matrices
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    LPDIRECT3DVERTEXSHADER9 m_pVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pPixelShaderSceme;

    // Natural Input data
    CameraManager m_CameraManager;

    HRESULT InitializeCameraNetwork( LPSTR strServerAddr );
    VOID UpdateNUI ( const NUI_SKELETON_DATA* pSkeletonData );
    VOID Draw( CXMMATRIX matView, CXMMATRIX matProj );
    ATG::FilterDoubleExponential m_JointFilter;

public:
    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{

    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;

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
    m_bDrawHelp = FALSE;
    m_fPosition = XMFLOAT3( 0.0f, 0.0f, 2.0f ); // Location of the user

    // Could also try for a bit more smoothing ( 0.25f, 0.25f, 0.25f, 0.03f, 0.05f );
    m_JointFilter.Init( 0.5f, 0.5f, 0.5f, 0.05f, 0.05f );


    HRESULT hr;

    // Initialize simple shaders.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create the font
    if( FAILED( hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create font\n" );
        return hr;
    }

    // Create the help
    if( FAILED( hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create help\n" );
        return hr;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the textures resource
    if( FAILED( hr = m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create Resource.xpr\n" );
        return hr;
    }

    // Initialize the natural input device
    if( FAILED( hr = m_CameraManager.InitializeCamera( m_pd3dDevice ) ) )
    {
        ATG_PrintError( "Couldn't create the natural input device.\n" );
        return hr;
    }

    // Set the transform matrices
    ID3DXBuffer* pShaderCode = NULL;
    ID3DXBuffer* pErrorMsg = NULL;

    // Create the vertex shader
    D3DXCompileShader( m_strVertexShaderProgram, 
        ( UINT )strlen( m_strVertexShaderProgram ),  NULL, NULL, "main", "vs.3.0", 0, 
        &pShaderCode, &pErrorMsg, NULL );
    m_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
        &m_pVertexShader );
    pShaderCode->Release();

    // Create the pixel shader
    D3DXCompileShader( m_strPixelShaderProgramScene, 
        ( UINT )strlen( m_strPixelShaderProgramScene ), NULL, NULL, "main", "ps.3.0", 0,
        &pShaderCode, &pErrorMsg, NULL );        
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
        &m_pPixelShaderSceme );
    pShaderCode->Release();

    // load the visuals
    m_pHeadTrackerScene = new ATG::Scene();
    assert( m_pHeadTrackerScene );
    m_pHeadTrackerScene->GetResourceDatabase()->AddBundledResources( &m_Resource );
    
    hr = ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\headtrackScene.xatg", m_pHeadTrackerScene, NULL,
        ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Couldn't load Scene\n" );
        return hr;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the elapsed time
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Update the natural input device
    BOOL bNewFrameReceived = m_CameraManager.Update( fElapsedTime, NULL );
    if ( bNewFrameReceived )
    {
        // Update if skeleton was tracked
        for (UINT uCurrentSkeleton = 0; uCurrentSkeleton < NUI_SKELETON_COUNT; uCurrentSkeleton++)
        {
            if ( m_CameraManager.GetSkeleton()->SkeletonData[uCurrentSkeleton].eTrackingState == NUI_SKELETON_TRACKED )
            {
                UpdateNUI( &m_CameraManager.GetSkeleton()->SkeletonData[uCurrentSkeleton] );
                break;
            }
        }
    }

    XMVECTOR vPosition =  XMVectorSet( m_fPosition.x, m_fPosition.y, m_fPosition.z, 0.0f );
    XMVECTOR vAt = XMVectorSet( m_fPosition.x, m_fPosition.y, 0.0f, 0.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    float nearPlane = 0.05f;
    float farPlane = 100.0f;

    m_matView = XMMatrixLookAtLH( vPosition, vAt, vUp );

    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    m_matProj = XMMatrixPerspectiveOffCenterLH(
        nearPlane * ( g_fSceneWorldXYMin.x  + m_fPosition.x ) / m_fPosition.z,
        nearPlane * ( g_fSceneWorldXYMax.x + m_fPosition.x ) / m_fPosition.z,
        nearPlane * ( g_fSceneWorldXYMin.y / fAspectRatio - m_fPosition.y )/ m_fPosition.z,
        nearPlane * ( g_fSceneWorldXYMax.y / fAspectRatio - m_fPosition.y )/ m_fPosition.z, nearPlane, farPlane );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// UpdateNUI,  update where the head is located.
//--------------------------------------------------------------------------------------
VOID Sample::UpdateNUI ( const NUI_SKELETON_DATA* pSkeletonData )
{

    if( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED )
    {
        // If the joints are all high confidence proceed
        if( pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SPINE ] != 
                NUI_SKELETON_POSITION_NOT_TRACKED &&
            pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HEAD ] != 
                NUI_SKELETON_POSITION_NOT_TRACKED ) 
        {
            XMFLOAT4 fHead; 
            
            m_JointFilter.Update( pSkeletonData );

            XMVECTOR vHead = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HEAD ];
            XMVECTOR vSpine = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SPINE ];
            XMVECTOR vLeftKnee = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_KNEE_LEFT ];

            XMStoreFloat4( &fHead, vHead * g_fScreenScale  );
            static FLOAT fHeadYOffset = fHead.y - g_fMoveHeadUpTVSpace;
            
            XMVECTOR vHeadToSpine = vHead - vSpine;
            XMVECTOR vSpineToKnee = vSpine - vLeftKnee;

            static const XMVECTOR vZeroX = XMVectorSet( 0.0f, 1.0f, 1.0f, 1.0f );
            vHeadToSpine = XMVector3Normalize( vHeadToSpine * vZeroX );
            vSpineToKnee = XMVector3Normalize( vSpineToKnee * vZeroX );

            static const XMVECTOR vUp = XMVectorSet( 0, 1.0f, 0.0f, 0.0f ); 
            XMVECTOR vHeadUpOrientation = XMVector3Dot( vUp, vHeadToSpine );
            XMVECTOR vSpineUpOrientation = XMVector3Dot( vUp, vSpineToKnee );

            static int iAverageIteration = 0;
            if ( XMVectorGetX( vHeadUpOrientation ) > g_fMaxDeviationFromUpVector && 
                 XMVectorGetX( vSpineUpOrientation ) > g_fMaxDeviationFromUpVector && 
                 fabsf( fHeadYOffset - fHead.y ) > g_fMaxHeadMovementTVSpaceBeforeReset  )
            {
                iAverageIteration = 0;
            }
            if ( XMVectorGetX( vHeadUpOrientation ) > g_fMaxDeviationFromUpVector &&
                 XMVectorGetX( vSpineUpOrientation ) > g_fMaxDeviationFromUpVector && 
                 iAverageIteration < g_iIterationsToAverageHeadOver )
            {
                fHeadYOffset = g_fUpdateHeadHeightRate * fHeadYOffset + 
                    g_fUpdateHeadHeightRateLerp * fHead.y - g_fMoveHeadUpTVSpace; 
            }

            m_fPosition.x = -fHead.x;
            m_fPosition.y = ( fHead.y ) - fHeadYOffset; 
            m_fPosition.z = fHead.z + 1;
            
        }
    }
    else
    {
        m_JointFilter.Reset();
    }
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering. This 
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background black 
    ATG::RenderBackground( 0xff000000, 0xff000000 );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();

    //Draw atrium
    Draw( m_matView, m_matProj );
    // Draw feet and compass wheel
    m_CameraManager.DisplayPIP();

    // Render the HUD
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0.0f, 0.0f, 0xffffffff, L"Head Tracker" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0.0f, 0.0f, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
    
    m_Font.End();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Draws the targets
//--------------------------------------------------------------------------------------
VOID Sample::Draw( CXMMATRIX matView, CXMMATRIX matProj )
{
    // Initialize default device states at the start of the frame
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAXANISOTROPY, 16 );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );

    // Set shader constants
    XMMATRIX matVP = matView * matProj;
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matVP, 4 );
    
    // Render the Atrium
    m_pd3dDevice->SetPixelShader( m_pPixelShaderSceme );    
    m_pd3dDevice->SetVertexShader( m_pVertexShader );

    PIXBeginNamedEvent( 0, "Render Head Tracker Scene" );

    ATG::NameIndexedCollection::iterator i;

    for( i = m_pHeadTrackerScene->GetInstanceList()->begin(); i != m_pHeadTrackerScene->GetInstanceList()->end(); i++ )
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

                // Loop over mesh subsets.
                DWORD dwSubsetCount = pMesh->GetNumSubsets();
                for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
                {
                    //if( bSetTextures )
                    {
                        ATG::MaterialInstance* pMaterial = mm.Materials[ dwSubsetIndex ];

                        for( DWORD j = 0; j < pMaterial->GetRawParameterCount(); ++j )
                        {
                            // Retrieve diffuse texture and normalmaps and set it
                            ATG::MaterialParameter& param = pMaterial->GetRawParameter( j );
                            if( param.pValue != NULL )
                            {
                                ATG::Texture2D* pTex2D = ( ATG::Texture2D* )param.pValue;

                                m_pd3dDevice->SetTexture( j, pTex2D->GetD3DTexture() );
                            }
                        }
                    }

                    // Render the mesh subset.
                    pMesh->RenderSubset( dwSubsetIndex, m_pd3dDevice );
                }
            }
        }
    }
    PIXEndNamedEvent(); // Render head tracker scene

}
