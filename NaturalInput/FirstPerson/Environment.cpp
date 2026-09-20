//-------------------------------------------------------------------------------------
// Environment.cpp
//  
// The Environment navigated in this sample.
//  
// Microsoft XNA Developer Connection
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "Environment.h"
#include <AtgUtil.h>
#include <AtgDebugDraw.h>
#include <AtgSimpleShaders.h>

FLOAT fWorldScale = 100.0f;  // used for drawing the Sky Dome.

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
    " };                                           \n"
    "                                              \n"
    " VS_OUT main( VS_IN In )                      \n"
    " {                                            \n"
    "     VS_OUT Out;                              \n"
    "     Out.ProjPos = mul( matWVP, In.ObjPos );  \n"
    "     Out.Normal = In.Normal;                  \n"
    "     Out.Texture = In.Texture;                \n"
    "     return Out;                              \n"
    " }                                            \n";


//-------------------------------------------------------------------------------------
// Atrium Pixel shader
//-------------------------------------------------------------------------------------
const CHAR* m_strPixelShaderProgramScene =
    " struct PS_IN                                                          \n"
    " {                                                                     \n"
    "     float2 Texture : TEXCOORD0;                                       \n"
    "     float3 vNormal : TEXCOORD1;                                       \n"
    " };                                                                    \n"
    "                                                                       \n"
    " sampler TextureSampler0 : register(s0);                               \n"
    "                                                                       \n"
    " float4 main( PS_IN In ) : COLOR                                       \n"
    " {                                                                     \n"
    "     float4 textureColor = tex2D( TextureSampler0, In.Texture );       \n"
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

//-------------------------------------------------------------------------------------
// Wheel pixel Shader
//-------------------------------------------------------------------------------------
const CHAR* m_strPixelShaderProgramWheel =
    " struct PS_IN                                                          \n"
    " {                                                                     \n"
    "     float3 vNormal : TEXCOORD1;                                       \n"
    " };                                                                    \n"
    "                                                                       \n"
    " sampler TextureSampler0 : register(s0);                               \n"
    " float4     g_fDrawColor : register(c0);                               \n"
    "                                                                       \n"
    " float4 main( PS_IN In ) : COLOR                                       \n"
    " {                                                                     \n"
    "     float3 vLightDir1 = float3( -1.0f, 1.0f, -1.0f );                 \n"
    "     float3 vLightDir2 = float3( 1.0f, 1.0f, -1.0f );                  \n"
    "     float3 vLightDir3 = float3( 0.0f, -1.0f, 0.0f );                  \n"
    "     float3 vLightDir4 = float3( 1.0f, 1.0f, 1.0f );                   \n" 
    "     float fLighting = 0.1f +                                          \n"
    "                  saturate( dot( vLightDir1 , In.vNormal ) )*0.25f +   \n"
    "                  saturate( dot( vLightDir2 , In.vNormal ) )*0.25f +   \n"
    "                  saturate( dot( vLightDir3 , In.vNormal ) )*0.25f +   \n"
    "                  saturate( dot( vLightDir4 , In.vNormal ) )*0.25f ;   \n"
    " return fLighting * g_fDrawColor;                                      \n"
    " }                                                                     \n";


//--------------------------------------------------------------------------------------
// Constructor that creates all of the Direct3D resources for the environment.
//--------------------------------------------------------------------------------------
Environment::Environment(): 
    m_pScene ( NULL ),
    m_pSkyDome( NULL ),
    m_pWheel( NULL )
{

}


//--------------------------------------------------------------------------------------
// Destructor that releases all of the Direct3D resources for the environment.
//--------------------------------------------------------------------------------------
Environment::~Environment() 
{

}

//--------------------------------------------------------------------------------------
// Draws the atrium and skybox.
//--------------------------------------------------------------------------------------
VOID Environment::Draw( CXMMATRIX matView, CXMMATRIX matProj, XMFLOAT3 vOrientation )
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

    PIXBeginNamedEvent( 0, "Render Atrium" );

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
    PIXEndNamedEvent(); // RenderSkyDome

    // render Skydome
    PIXBeginNamedEvent( 0, "Render Skydome" );

    XMMATRIX matWorld = XMMatrixScaling( fWorldScale, fWorldScale, fWorldScale );
    XMMATRIX matCameraWVP = ( matWorld * matView * matProj );

    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matCameraWVP, 4 );

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );

    for( i = m_pSkyDome->GetInstanceList()->begin(); i != m_pSkyDome->GetInstanceList()->end(); i++ )
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

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    PIXEndNamedEvent(); // RenderSkyDome


}

//--------------------------------------------------------------------------------------
// Draws the environment on the stored graphics device, using the given view and projection.
//--------------------------------------------------------------------------------------
VOID Environment::DrawUI( XMFLOAT3 vOrientation, FLOAT fForwardSpeed, FLOAT fStrafeSpeed, D3DCOLOR dwWheelColor )
{


    // Draw a box around the feet visualizer
    D3DRECT rect;
    rect.x1 = 974;
    rect.y1 = 452;
    rect.x2 = 1102;
    rect.y2 = 680;
    ATG::DebugDraw::DrawScreenSpaceRect( rect, 2.0f, 0xFFFFFFFF );

    // Render the HUD controls
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    XMFLOAT3 Corner1 = XMFLOAT3( 0.0f, 0.0f, 0.0f );
    XMFLOAT3 Corner2 = XMFLOAT3( 128.0f, 0.0f, 0.0f );
    XMFLOAT3 Corner3 = XMFLOAT3( 0.0f, 128.0f, 0.0f );
    
    //Draw feet to show player's orientation 
    XMMATRIX mOrtho = XMMatrixOrthographicOffCenterLH( 0.0f, 1280.0f, 0.0f, 720.0f, 1.0f, -1.0f );
    XMVECTOR vRotationOrigin = XMVectorSet( 128.0f, 128.0f, 0.0f, 0.0f );
    XMVECTOR vScale = XMVectorSet( 1.0f, 1.0f, 1.0f, 1.0f );
    XMFLOAT2 uvRepeat = XMFLOAT2( 1.0f, -1.0f );
    XMVECTOR vTranslation = XMVectorSet( 974.0f + 64.0f * fStrafeSpeed, 
        96.0f + 64.0f * fForwardSpeed, 0.0f, 0.0f );
    XMMATRIX mAffine = XMMatrixAffineTransformation2D( vScale, vRotationOrigin, 0, vTranslation );
    ATG::DebugDraw::SetViewProjection( mAffine * mOrtho );
    ATG::DebugDraw::DrawTexturedQuad( Corner1, Corner2, Corner3, uvRepeat, m_pFeetTexture );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );
    
    //Draw a wheel to show player how much the camera is being rotated
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAXANISOTROPY, 16 );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );

    XMVECTOR vFrom = XMVectorSet(0.0f, 4.0f, 0.0f, 1.0f );
    XMVECTOR vAt = XMVectorSet( 0.0f, 0.0f, 15.0f, 1.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 1.0f );
    XMMATRIX mView = XMMatrixLookAtLH( vFrom, vAt, vUp );
    
    FLOAT fAspectRatio = 1280.0f / 720.0f;
    XMMATRIX mProjection = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, .10f, 100.0f );
    FLOAT fAngle = atan2f( vOrientation.z, vOrientation.x ); 
    XMMATRIX mWorld = XMMatrixRotationRollPitchYaw(0, fAngle, 0 ) * XMMatrixTranslation( 0.0f, 0.0f, 15.0f );
    XMMATRIX mOrientOnScreen = XMMatrixTranslation( 800.0f / 1280.0f, 0.0f, 0.0f );
    
    // mWorld rotates the wheel based on the scene
    // view projetion gives a perspective look
    // mOrientOnScreen moves the wheel to the right side of the screen
    XMMATRIX mViewProjection = mWorld * mView * mProjection * mOrientOnScreen; 
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&mViewProjection, 4 );
    
    // clear depth behind wheel so that it doesn't ever get culled
    rect.x1 = 800;
    rect.x2 = 1280;
    rect.y1 = 232;
    rect.y2 = 488;
    m_pd3dDevice->Clear( 1, &rect, D3DCLEAR_ZBUFFER, 0, 1.0f, 0 ); 


    // Render the Wheel
    m_pd3dDevice->SetPixelShader( m_pPixelShaderWheel );
    m_pd3dDevice->SetVertexShader( m_pVertexShader );

    D3DXCOLOR d3dxWoodColor ( 0xFF713605 ); 
    D3DXCOLOR d3dxWheelColor( dwWheelColor );
 
    ATG::NameIndexedCollection::iterator i;
    m_pd3dDevice->SetPixelShaderConstantF( 0, d3dxWheelColor, 1 );
    int cnt = -1;
    for( i = m_pWheel->GetInstanceList()->begin(); i != m_pWheel->GetInstanceList()->end(); i++ )
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

                // Loop over mesh subset.
                DWORD dwSubsetCount = pMesh->GetNumSubsets();
                for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
                {
                   ++cnt;
                    if ( cnt  == 9 )
                    {
                        m_pd3dDevice->SetPixelShaderConstantF( 0, d3dxWoodColor, 1 );
                   
                    }
                    else if ( cnt == 10 )
                    {
                        m_pd3dDevice->SetPixelShaderConstantF( 0, d3dxWheelColor, 1 );       
                    }
                    
                    // Render the mesh subset.
                    pMesh->RenderSubset( dwSubsetIndex, m_pd3dDevice );
                }
            }
        }
    }

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    


}


//--------------------------------------------------------------------------------------
// Create the Direct3D resources for the Scene.
//--------------------------------------------------------------------------------------
HRESULT Environment::CreateGraphicsResources( IDirect3DDevice9* pd3dDevice, ATG::PackedResource& resource )
{
    HRESULT hr;

    m_pd3dDevice = pd3dDevice;

    m_pFeetTexture = resource.GetTexture("FeetTexture");

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

    // Create the pixel shader
    D3DXCompileShader( m_strPixelShaderProgramWheel, 
        ( UINT )strlen( m_strPixelShaderProgramWheel ), NULL, NULL, "main", "ps.3.0", 0,
        &pShaderCode, &pErrorMsg, NULL );        
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
        &m_pPixelShaderWheel );
    pShaderCode->Release();

     // Create and load SponzaAtrium scene
    m_pScene = new ATG::Scene();
    assert( m_pScene );
    m_pScene->GetResourceDatabase()->AddBundledResources( &resource );
   
    hr = ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\SponzaAtrium.xatg", m_pScene, NULL,
        ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Couldn't load Sponza Atrium\n" );
        return hr;
    }

    // Create and load Wheel 
    m_pWheel = new ATG::Scene();
    assert( m_pWheel );
    m_pWheel->GetResourceDatabase()->AddBundledResources( &resource );
   
    hr = ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\Wheel.xatg", m_pWheel, NULL,
        ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Couldn't load Wheel\n" );
        return hr;
    }

    // Create and load skydome scene
    m_pSkyDome = new ATG::Scene();
    assert( m_pSkyDome );
    m_pSkyDome->GetResourceDatabase()->AddBundledResources( &resource );
    hr = ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\SkyDome.xatg", m_pSkyDome, NULL,
        ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Couldn't load SkyDome\n" );
        return hr;     
    }
 
    return hr;
}

