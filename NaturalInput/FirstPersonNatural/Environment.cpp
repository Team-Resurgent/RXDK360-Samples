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
// Pixel shader
//-------------------------------------------------------------------------------------
const CHAR* m_strPixelShaderProgram =
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


//--------------------------------------------------------------------------------------
// Constructor that creates all of the Direct3D resources for the environment.
//--------------------------------------------------------------------------------------
Environment::Environment(): m_pScene ( NULL ),
    m_pSkyDome( NULL )
{
}


//--------------------------------------------------------------------------------------
// Destructor that releases all of the Direct3D resources for the environment.
//--------------------------------------------------------------------------------------
Environment::~Environment() 
{
}


//--------------------------------------------------------------------------------------
// Draws the environment on the stored graphics device, using the given view and projection.
//--------------------------------------------------------------------------------------
VOID Environment::Draw( CXMMATRIX matView, CXMMATRIX matProj, FLOAT fTurnAmount, FLOAT fForwardSpeed, FLOAT fStrafeSpeed )
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

    PIXBeginNamedEvent( 0, "Render Atrium" );

    // Set shader constants
    XMMATRIX matVP = matView * matProj;
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matVP, 4 );


    m_pd3dDevice->SetPixelShader( m_pPixelShader );
    m_pd3dDevice->SetVertexShader( m_pVertexShader );

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
    PIXEndNamedEvent(); // Sample::RenderSkyDome

    // render Skydome
    PIXBeginNamedEvent( 0, "Render skydome" );

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
    PIXEndNamedEvent(); // Sample::RenderSkyDome
    D3DRECT rect;
    rect.x1 = 0;
    rect.x2 = 1280;
    rect.y1 = 0;
    rect.y2 = 720;
    m_pd3dDevice->Clear( 1, &rect, D3DCLEAR_ZBUFFER, 0, 1.0f, 0 ); 
    // Render the HUD controls
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    D3DRECT rectClear;
    rectClear.x1 = 1088;
    rectClear.y1 = 452;
    rectClear.x2 = 1216;
    rectClear.y2 = 682;
    ATG::DebugDraw::DrawScreenSpaceRect( rectClear, 2.0f, 0xFFFFFFFF );
    XMFLOAT3 Corner1 = XMFLOAT3( 0.0f, 0.0f, 0.0f );
    XMFLOAT3 Corner2 = XMFLOAT3( 128.0f, 0.0f, 0.0f );
    XMFLOAT3 Corner3 = XMFLOAT3( 0.0f, 128.0f, 0.0f );
    XMFLOAT2 uvRepeat = XMFLOAT2( 1.0f, -1.0f );
    XMVECTOR vScale = XMVectorSet( 1.0f, 1.0f, 1.0f, 1.0f );
    XMVECTOR vRotationOrigin = XMVectorSet( 64.0f, 64.0f, 0.0f, 0.0f );
    FLOAT fAngle = atan2f( fTurnAmount, 1.0 ); 
    XMMATRIX mOrtho = XMMatrixOrthographicOffCenterLH( 0.0f, 1280.0f, 0.0f, 720.0f, 1.0f, -1.0f );
    XMVECTOR vTranslation = XMVectorSet( 1024.0f + 64.0f + 64.0f * fStrafeSpeed, 
        0.0f + 96.0f + 64.0f * fForwardSpeed, 0.0f, 0.0f );
    XMMATRIX mAffine = XMMatrixAffineTransformation2D( vScale, vRotationOrigin, fAngle, vTranslation );
    ATG::DebugDraw::SetViewProjection( mAffine * mOrtho );
    // Render feet to help the user visualize what they are doing.
    // Rotate when based on orientation. Move forward or backward based on direction moving. Move left and right based on strafe.
    ATG::DebugDraw::DrawTexturedQuad( Corner1, Corner2, Corner3, uvRepeat, m_pFeetTexture );

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );




}


//--------------------------------------------------------------------------------------
// Create the Direct3D resources for the environment
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
    D3DXCompileShader( m_strPixelShaderProgram, 
        ( UINT )strlen( m_strPixelShaderProgram ), NULL, NULL, "main", "ps.3.0", 0,
        &pShaderCode, &pErrorMsg, NULL );        
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
        &m_pPixelShader );
    pShaderCode->Release();

    // Fill the vertex buffer
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

