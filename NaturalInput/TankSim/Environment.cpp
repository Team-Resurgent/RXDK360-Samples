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
    "     Out.Texture.xy = In.ObjPos.xz * 0.01f;   \n"
    "     return Out;                              \n"
    " }                                            \n";

//-------------------------------------------------------------------------------------
// Terrain Pixel Shader
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
    " return ( textureColor + 0.2f ) * fLighting;                                      \n"
    " }                                                                     \n";

//--------------------------------------------------------------------------------------
// Throttle Pixel Shader
//--------------------------------------------------------------------------------------
const CHAR* m_strPixelShaderProgramThrottle =
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

struct TerrainVertex
{    
    XMFLOAT3 m_vPosition;
    XMFLOAT3 m_vNormal;
};

//--------------------------------------------------------------------------------------
// Constructor that creates all of the Direct3D resources for the environment.
//--------------------------------------------------------------------------------------
Environment::Environment(): 
    m_pTerrainBuffer( NULL ),
    m_pTerrainIndexBuffer( NULL )
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
VOID Environment::Draw( CXMMATRIX matView, CXMMATRIX matProj )
{
    // Initialize default device states at the start of the frame
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAXANISOTROPY, 16 );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );

    D3DXCOLOR Color = 0xFFFFFFFF;
    
    m_pd3dDevice->SetPixelShaderConstantF( 0, Color, 1 );

    m_pd3dDevice->SetTexture( 0, m_pTexture );
    // Set shader constants
    XMMATRIX matVP = matView * matProj;
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matVP, 4 );
    
    // Render the Terrain
    m_pd3dDevice->SetTexture( 0, m_pForestGroundTexture ); 
    m_pd3dDevice->SetPixelShader( m_pPixelShaderScene );    
    m_pd3dDevice->SetVertexShader( m_pVertexShader );
    m_pd3dDevice->SetIndices( m_pTerrainIndexBuffer );
    m_pd3dDevice->SetStreamSource( 0, m_pTerrainBuffer, 0, sizeof( TerrainVertex ) );
    m_pd3dDevice->SetFVF( D3DFVF_XYZ | D3DFVF_NORMAL );
    m_pd3dDevice->DrawIndexedVertices( D3DPT_QUADLIST, 0, 0, 255*255 * 4 );

}
//--------------------------------------------------------------------------------------
// Returns the height at a specific X and Y value
//--------------------------------------------------------------------------------------
FLOAT Environment::HeightAt( FLOAT x, FLOAT y )
{
    x *= 2.56f;
    y *= 2.56f;
    if ( x < 0.0f || x >255.0f || y < 0.0f || y > 255.0f )
    {
        return -1.0f;
    }
    INT index = (INT)y * 256 + (INT)x;
    
    return m_pVerticies[index].m_vPosition.y;    
}

//--------------------------------------------------------------------------------------
// Render ATG Scene
//--------------------------------------------------------------------------------------
VOID Environment::DrawModel( ATG::Scene* pScene )
{
    ATG::NameIndexedCollection::iterator i;
    for( i = pScene->GetInstanceList()->begin(); i != pScene->GetInstanceList()->end(); i++ )
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
                    // Render the mesh subset.
                    pMesh->RenderSubset( dwSubsetIndex, m_pd3dDevice );
                }
            }
        }
    }
    

}

//--------------------------------------------------------------------------------------
// Draws the environment on the stored graphics device, using the given view and projection.
//--------------------------------------------------------------------------------------
VOID Environment::DrawUI( BOOL bHandOnLThrottle, BOOL bHandOnRThrottle, FLOAT fLThrottle, FLOAT fRThrottle, XMFLOAT2 vLHandVis, XMFLOAT2 vRHandVis )
{
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAXANISOTROPY, 16 );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );

    FLOAT fScale = 4.0f;
    XMVECTOR vFrom = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f );
    XMVECTOR vAt = XMVectorSet( 15.0f, -4.0f, 0.0f, 1.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 1.0f );
    XMMATRIX mView = XMMatrixLookAtLH( vFrom, vAt, vUp );
    
    FLOAT fAspectRatio = 1280.0f / 720.0f;
    XMMATRIX mProjection = XMMatrixPerspectiveFovLH( XM_PI / 12.0f, fAspectRatio, 20.0f, 100.0f );

    XMMATRIX mWorldThrottleTop = XMMatrixRotationRollPitchYaw( 0.0f, 0.0f , 0.0f ) * XMMatrixTranslation( 40.0f + fRThrottle * fScale -2.0f, -10.0f, -5.0f );
    XMMATRIX mWorldThrottleBottom = XMMatrixRotationRollPitchYaw( 0.0f, 0.0f , 0.0f ) * XMMatrixTranslation( 40.0f, -10.0f, -5.0f );
    XMMATRIX mOrientOnScreen = XMMatrixTranslation( 200.0f / 1280.0f, -580.0f / 720.0f, 0.0f );
    
    vLHandVis.x *= 640.0f;
    vRHandVis.x *= 640.0f;
    vLHandVis.x += 640.0f;
    vRHandVis.x += 640.0f;
    
    vLHandVis.y *= -360.0f;
    vRHandVis.y *= -360.0f;    
    vLHandVis.y += 360.0f;
    vRHandVis.y += 360.0f;    
    
    ATG::DebugDraw::DrawScreenSpaceRect(vRHandVis, XMFLOAT2( 40.0f, 40.0f), 2.0f, 0xFFFFFFFF) ;
    ATG::DebugDraw::DrawScreenSpaceRect(vLHandVis, XMFLOAT2( 40.0f, 40.0f), 2.0f, 0xFFFFFFFF) ;

    // Clear depth behind wheel so that it doesn't ever get culled
    D3DRECT rect;
    rect.x1 = 0;
    rect.x2 = 1280;
    rect.y1 = 0;
    rect.y2 = 720;
    m_pd3dDevice->Clear( 1, &rect, D3DCLEAR_ZBUFFER, 0, 1.0f, 0 ); 

    // Render the Throttle
    m_pd3dDevice->SetPixelShader( m_pPixelShaderThrottle );
    m_pd3dDevice->SetVertexShader( m_pVertexShader );
    D3DXCOLOR Color;
    if ( bHandOnRThrottle )
    {
        Color = 0xFF00FF00;
    }
    else
    {
        Color = 0xFFFFFFFF;
    }
    m_pd3dDevice->SetPixelShaderConstantF( 0, Color, 1 );

    XMMATRIX mViewProjection = mWorldThrottleBottom * mView * mProjection * mOrientOnScreen; 
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&mViewProjection, 4 );
    DrawModel( m_pControlBase );

    mViewProjection = mWorldThrottleTop * mView * mProjection * mOrientOnScreen; 
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&mViewProjection, 4 );
    DrawModel( m_pControlThrottle );

    mWorldThrottleBottom = XMMatrixRotationRollPitchYaw( 0.0f, 0.0f , 0.0f ) * XMMatrixTranslation( 40.0f, -10.0f, 5.0f );
    mWorldThrottleTop = XMMatrixRotationRollPitchYaw( 0.0f, 0.0f , 0.0f ) * XMMatrixTranslation( 40.0f + fLThrottle * fScale -2.0f, -10.0f, 5.0f );
    mOrientOnScreen = XMMatrixTranslation( -200.0f / 1280.0f, -580.0f / 720.0f, 0.0f );
    if ( bHandOnLThrottle )
    {
        Color = 0xFF00FF00;
    }
    else
    {
        Color = 0xFFFFFFFF;
    }
    m_pd3dDevice->SetPixelShaderConstantF( 0, Color, 1 );

    mViewProjection = mWorldThrottleBottom * mView * mProjection * mOrientOnScreen; 
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&mViewProjection, 4 );
    DrawModel( m_pControlBase );

    mViewProjection = mWorldThrottleTop * mView * mProjection * mOrientOnScreen; 
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&mViewProjection, 4 );
    DrawModel( m_pControlThrottle );
}

//--------------------------------------------------------------------------------------
// Create the Direct3D resources for the Scene.
//--------------------------------------------------------------------------------------
HRESULT Environment::CreateGraphicsResources( IDirect3DDevice9* pd3dDevice, ATG::PackedResource& resource )
{
    HRESULT hr;

    m_pd3dDevice = pd3dDevice;

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
        &m_pPixelShaderScene );
    pShaderCode->Release();

    // Create the pixel shader
    D3DXCompileShader( m_strPixelShaderProgramThrottle, 
        ( UINT )strlen( m_strPixelShaderProgramThrottle ), NULL, NULL, "main", "ps.3.0", 0,
        &pShaderCode, &pErrorMsg, NULL );        
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
        &m_pPixelShaderThrottle );
    pShaderCode->Release();

    // Create and load ControlBase
    hr = LoadATGMesh( "game:\\media\\scenes\\ThrottleBase.xatg", resource, &m_pControlBase  );
    if ( FAILED( hr ) ) return hr;

    // Create and load ControlBase
    hr = LoadATGMesh( "game:\\media\\scenes\\ThrottleTop.xatg", resource, &m_pControlThrottle  );
    if ( FAILED( hr ) ) return hr;

    m_pTexture = resource.GetTexture( "MSH1024" );
    m_pForestGroundTexture = resource.GetTexture( "ForestGround" );
        
    D3DSURFACE_DESC sDESC;
    m_pTexture->GetLevelDesc(0, &sDESC );
    
    INT iSize = 256 * 256 * sizeof(TerrainVertex);
    m_pd3dDevice->CreateVertexBuffer( iSize, 0, 0, 0, &m_pTerrainBuffer, NULL );
    m_pVerticies = new TerrainVertex[256*256];

    D3DLOCKED_RECT LockedRect;
    m_pTexture->LockRect( 0, &LockedRect, NULL, D3DLOCK_READONLY );
    UINT* pTextureData = (UINT*)LockedRect.pBits;

    // Create a terrain mesh from a grey scale height map.
    TerrainVertex* pCurrentVertex;
    for ( int iY = 0; iY < 256; ++iY )
    {
        for ( int iX = 0; iX < 256; ++iX )
        {
            UINT uCurrentPixel = pTextureData[(iY+256) *1024+(iX+256)] & 0x000000FF;
            UINT uCurrentPixelRight = 0;
            UINT uCurrentPixelDown = 0;
            UINT uCurrentPixelRightDown = 0;
            if ( iX < 255 )
            {
                uCurrentPixelRight = pTextureData[(iY+256) * 1024 + (iX + 1 + 256)] & 0x000000FF;
            }  
            if ( iY < 255 )
            {
                uCurrentPixelDown = pTextureData[(1 + iY + 256) * 1024 + (iX+256)] & 0x000000FF;
            }
            if ( iX < 255 && iY < 255 )
            {
                uCurrentPixelRightDown = pTextureData[(1 + iY + 256) * 1024 + (iX+1+256)] & 0x000000FF;
            }
            if ( uCurrentPixel != 0 )
            {
                uCurrentPixel = uCurrentPixel;
            }
            FLOAT fBlurredValue = (FLOAT)uCurrentPixel + (FLOAT)uCurrentPixelDown 
                + (FLOAT)uCurrentPixelRight + (FLOAT)uCurrentPixelRightDown;
            fBlurredValue *= 0.25f;
            pCurrentVertex = &m_pVerticies[ iY * 256 + iX ];
            // scale the data in x and z by 100 and in y by 30.
            pCurrentVertex->m_vPosition.y = fBlurredValue / 255.0f * 30.0f;
            pCurrentVertex->m_vPosition.x = ( FLOAT )iX / 256.0f * 100.0f;
            pCurrentVertex->m_vPosition.z = ( FLOAT )iY / 256.0f * 100.0f;

            XMVECTOR vCenter = XMVectorSet( pCurrentVertex->m_vPosition.x, pCurrentVertex->m_vPosition.y / 3.0f, pCurrentVertex->m_vPosition.z, 0 );
            XMVECTOR vRight = XMVectorSet( ( FLOAT )(iX+1) / 256.0f, ( FLOAT )uCurrentPixelRight / 255.0f / 3.0f,( FLOAT )iY / 256.0f,0 );
            XMVECTOR vUp = XMVectorSet(  ( FLOAT )iX / 256.0f, ( FLOAT )uCurrentPixelDown / 255.0f / 3.0f,( FLOAT )(iY+1) / 256.0f, 0 );
            XMVECTOR vNormal = XMVector3Normalize( XMVector3Cross(vRight -vCenter, vUp - vCenter) );
            pCurrentVertex->m_vNormal.x = XMVectorGetX( vNormal );
            pCurrentVertex->m_vNormal.y = XMVectorGetY( vNormal );
            pCurrentVertex->m_vNormal.z = XMVectorGetZ( vNormal );
        }
    }  
    // create indices
    for ( int iY = 0; iY < 256; ++iY )
    {
        for ( int iX = 0; iX < 256; ++iX )
        {
            pCurrentVertex = &m_pVerticies[ iY * 256 + iX ];
        }
    }
    m_pd3dDevice->CreateIndexBuffer( 255 * 255 * 4 * 4, 0, D3DFMT_INDEX16, 0, &m_pTerrainIndexBuffer, NULL );
    USHORT *pIndData;
    m_pTerrainIndexBuffer->Lock( 0, 0, (VOID**)&pIndData, 0 );
    
    for ( INT iY = 0; iY < 255; iY++ )
    {
        for ( INT iX = 0 ; iX < 255; ++iX )
        {
            pIndData[ iY * 255 * 4 + ( iX * 4 ) ] =     (SHORT)(iY * 256 + iX);                       
            pIndData[ iY * 255 * 4 + ( iX * 4 ) + 1 ] = (SHORT)(iY * 256 + iX + 1);                        
            pIndData[ iY * 255 * 4 + ( iX * 4 ) + 2 ] = (SHORT)(( iY + 1 ) * 256 + iX + 1);                        
            pIndData[ iY * 255 * 4 + ( iX * 4 ) + 3 ] = (SHORT)(( iY + 1 ) * 256 + iX);                        
        }
    }

    TerrainVertex* pVerticies;
    m_pTerrainBuffer->Lock(0, 0, (VOID**)&pVerticies, 0 );\
        memcpy(pVerticies, m_pVerticies, 256 * 256 * sizeof(TerrainVertex) );
    m_pTerrainBuffer->Unlock();
    m_pTerrainIndexBuffer->Unlock();
    m_pTexture->UnlockRect(0);
    return hr;
}

//--------------------------------------------------------------------------------------
// Load ATG Model
//--------------------------------------------------------------------------------------
HRESULT Environment::LoadATGMesh( char* fileName, ATG::PackedResource& resource, ATG::Scene** pReturnedScene )
{
    *pReturnedScene = new ATG::Scene();
    assert( *pReturnedScene );
    (**pReturnedScene).GetResourceDatabase()->AddBundledResources( &resource );
    HRESULT hr = ATG::SceneFileParser::LoadXATGFile( fileName, *pReturnedScene, NULL,
        ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL );

    if( FAILED( hr ) )
    {
        ATG_PrintError( "Couldn't load Model\n" );
    }
    return hr;
}