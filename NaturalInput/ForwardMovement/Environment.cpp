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

// artist generated trail path.

XMFLOAT2 g_TrailCoords[] = 
{
    XMFLOAT2( 92, 235 ),
    XMFLOAT2( 180, 233 ),
    XMFLOAT2( 223, 178 ),
    XMFLOAT2( 224, 91 ),
    XMFLOAT2( 214, 55 ),
    XMFLOAT2( 155, 31 ),
    XMFLOAT2( 70, 22 ),
    XMFLOAT2( 30, 65 ),
    XMFLOAT2( 30, 176 ),
    XMFLOAT2( 45, 219 )
};

INT g_iCoordCnt = sizeof(g_TrailCoords) / sizeof(XMFLOAT2);
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
    "     float3 Normal   : NORMAL;                \n"
    "     float2 TextureCoord1   : TEXCOORD0;      \n"
    "     float2 TextureCoord2   : TEXCOORD1;      \n"
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
    "     Out.Texture.xy = In.ObjPos.xz * 0.4f;    \n"
    "     return Out;                              \n"
    " }                                            \n";

//-------------------------------------------------------------------------------------
// Terrain Pixel Shader
//-------------------------------------------------------------------------------------
const CHAR* m_strPixelShaderProgramScene =
    " struct PS_IN                                                              \n"
    " {                                                                         \n"
    "     float2 Texture : TEXCOORD0;                                           \n"
    "     float3 vNormal : TEXCOORD1;                                           \n"
    " };                                                                        \n"
    "                                                                           \n"
    " sampler TextureSampler0 : register(s0);                                   \n"
    " sampler TextureSampler1 : register(s1);                                   \n"
    "                                                                           \n"
    " float4 main( PS_IN In ) : COLOR                                           \n"
    " {                                                                         \n"
    "     float4 textureColor = tex2D( TextureSampler0, In.Texture );           \n"
    "     float3 textureNormal =  tex2D( TextureSampler1, In.Texture  ).rbg - float3(0.5f, 0.5f, 0.5f) + In.vNormal.rbg; \n"
    "     float3 vLightDir1 = float3( -1.0f, 1.0f, 1.0f );                      \n"
    "     float3 vLightDir2 = float3( 1.0f, 1.0f, -1.0f );                      \n"
    "     float3 vLightDir3 = float3( 0.0f, -1.0f, 0.0f );                      \n"
    "     float3 vLightDir4 = float3( 1.0f, 1.0f, 1.0f );                       \n" 
    "     float fLighting =                                                     \n"
    "                  saturate( dot( vLightDir1 , textureNormal ) )*0.5f +     \n"
    "                  saturate( dot( vLightDir2 , textureNormal ) ) * 0.50f;   \n"
    " return textureColor * fLighting;                                          \n"
    " }                                                                         \n";

struct TerrainVertex
{    
    XMFLOAT3 m_vPosition;
    XMFLOAT3 m_vNormal;
    XMFLOAT2 m_vTexCoord1;
    XMFLOAT2 m_vTexCoord2;
};

//--------------------------------------------------------------------------------------
// Constructor that creates all of the Direct3D resources for the environment.
//--------------------------------------------------------------------------------------
Environment::Environment(): 
    m_pTerrainBuffer( NULL ),
    m_pTerrainIndexBuffer( NULL )
{
    for ( int index = 0; index < g_iCoordCnt; ++ index )
    {
        g_TrailCoords[index].x /= 2.56f; // convert from 256 by 256 to 100 by 100;
        g_TrailCoords[index].y = ( 256.0f - g_TrailCoords[index].y ) / 2.56f; // flip y coord;
    }
    m_iCurrentCoord = 0;
    m_fCurrentLocation = g_TrailCoords[m_iCurrentCoord];
    m_fDir = XMFLOAT2( 0.0f, 0.0f );
}

//--------------------------------------------------------------------------------------
// Move the position in world space around the track.
//--------------------------------------------------------------------------------------
VOID Environment::UpdateTrailLoc ( FLOAT fSpeed )
{
    // make it tiny to remove divide by zero errors
    //fSpeed = 0.2f;
    if ( fSpeed <= 0.0f ) fSpeed = 0.001f;
    INT iNextCoord = ( m_iCurrentCoord + 1 ) % g_iCoordCnt;
    XMFLOAT2 fNextLocation = g_TrailCoords[iNextCoord];
    XMVECTOR vCurrentLocation = XMLoadFloat2( &m_fCurrentLocation );
    XMVECTOR vNextLocation = XMLoadFloat2( &fNextLocation );
    XMVECTOR vPreviousLocation = XMLoadFloat2( &g_TrailCoords[m_iCurrentCoord] );

    XMVECTOR vDir = vNextLocation - vCurrentLocation; 
    vDir = XMVector2Normalize( vDir );
    vDir *= fSpeed;
    static XMVECTOR vRealDir = vDir;

    vCurrentLocation += vRealDir;
    XMVECTOR vNewDir = XMVector2Normalize( vNextLocation - vCurrentLocation ) * fSpeed; 
    XMVECTOR vPrevToNext = XMVector2Normalize( vNextLocation - vPreviousLocation ); 
    XMVECTOR vDot = XMVector2Dot ( vNewDir, vPrevToNext );
    static FLOAT fTurnSharp = 0.0f;
    static FLOAT fTurnSharpEquation = 0.001f;
    static const FLOAT fTurnSharpRate = 1.1f;
    static const FLOAT fTurnSharpMax = 0.05f;

    if ( XMVectorGetX ( vDot ) < 0.0f )
    {
        fTurnSharp = 0.0f;
        fTurnSharpEquation = 0.2f / fSpeed;
        ++m_iCurrentCoord;
        m_iCurrentCoord %= g_iCoordCnt;
    }
    fTurnSharpEquation *= fTurnSharpRate;
    fTurnSharp += fTurnSharpEquation;
    if ( fTurnSharp > fTurnSharpMax ) { 
        fTurnSharp = fTurnSharpMax;
    }
    FLOAT fInterp = 1.0f - fTurnSharp;
    vRealDir = XMVector2Normalize(  fInterp * vRealDir + fTurnSharp * vDir ) * fSpeed; 

    XMStoreFloat2( &m_fCurrentLocation, vCurrentLocation );
    XMStoreFloat2( &m_fDir, vRealDir );
    
}

XMFLOAT2 Environment::GetLocation ()
{
    return m_fCurrentLocation; 
}

XMFLOAT2 Environment::GetDirection ()
{
    return m_fDir; 
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
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAXANISOTROPY, 16 );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );

    D3DXCOLOR Color = 0xFFFFFFFF;
    
    m_pd3dDevice->SetPixelShaderConstantF( 0, Color, 1 );


    m_pd3dDevice->SetTexture( 0, m_pForestGroundTexture );
    m_pd3dDevice->SetTexture( 1, m_pWaterBumps ); 
    // Set shader constants
    XMMATRIX matVP = matView * matProj;
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matVP, 4 );
    
    // Render the Terrain
    m_pd3dDevice->SetPixelShader( m_pPixelShaderScene );    
    m_pd3dDevice->SetVertexShader( m_pVertexShader );
    m_pd3dDevice->SetIndices( m_pTerrainIndexBuffer );
    m_pd3dDevice->SetStreamSource( 0, m_pTerrainBuffer, 0, sizeof( TerrainVertex ) );
    m_pd3dDevice->SetFVF( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX0 );
    m_pd3dDevice->DrawIndexedVertices( D3DPT_QUADLIST, 0, 0, 255*255 * 4 );

}
//--------------------------------------------------------------------------------------
// Returns the height at a specific X and Y value
//--------------------------------------------------------------------------------------
FLOAT Environment::HeightAt( FLOAT x, FLOAT y )
{
    x *= 2.56f;
    y *= 2.56f;
    if ( x < 1.0f || x >254.0f || y < 1.0f || y > 254.0f )
    {
        return -1.0f;
    }
    FLOAT fX = x - (FLOAT)(INT)x; 
    FLOAT fY = y - (FLOAT)(INT)y; 
    
    INT index = (INT)y * 256 + (INT)x;
    INT xOffset = 1;
    INT yOffset = 1;
    if ( fX < 0.5f ) xOffset = -1; 
    if ( fY < 0.5f ) yOffset = -1; 

    FLOAT fDirectPixel = m_pVerticies[index].m_vPosition.y;    
    FLOAT fXOffsetPixel = m_pVerticies[index + xOffset].m_vPosition.y;    
    FLOAT fYoffsetPixel = m_pVerticies[index + yOffset * 256].m_vPosition.y;    
    FLOAT fXYOffsetPixel = m_pVerticies[index + yOffset * 256 + xOffset].m_vPosition.y; 
    return max ( max( fDirectPixel, fXOffsetPixel ), max( fYoffsetPixel, fXYOffsetPixel ) );
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
    

}

XMFLOAT2 g_fHardCodedPath[] = 
{
    XMFLOAT2(1.1f, 2.1f), 
    XMFLOAT2(3.1f, 4.1f) 
};
//--------------------------------------------------------------------------------------
// Create the Direct3D resources for the Scene.
//--------------------------------------------------------------------------------------
HRESULT Environment::CreateGraphicsResources( IDirect3DDevice9* pd3dDevice, ATG::PackedResource& resource )
{

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

    m_pTextureTerrain = resource.GetTexture( "MSH1024" );
    m_pTextureTrail = resource.GetTexture( "Trail" );
    m_pForestGroundTexture = resource.GetTexture( "ForestGround" );
    m_pWaterBumps = resource.GetTexture( "waterbumps" );
        
    D3DSURFACE_DESC sDESC;
    m_pTextureTerrain->GetLevelDesc(0, &sDESC );
    
    INT iSize = 256 * 256 * sizeof(TerrainVertex);
    m_pd3dDevice->CreateVertexBuffer( iSize, 0, 0, 0, &m_pTerrainBuffer, NULL );
    m_pVerticies = new TerrainVertex[256*256];

    D3DLOCKED_RECT LockedRectTerrain;
    m_pTextureTerrain->LockRect( 0, &LockedRectTerrain, NULL, D3DLOCK_READONLY );
    UINT* pTextureTerrainData = (UINT*)LockedRectTerrain.pBits;

    D3DLOCKED_RECT LockedRectTrail;
    m_pTextureTrail->LockRect( 0, &LockedRectTrail, NULL, D3DLOCK_READONLY );
    UINT* pTextureTrailData = (UINT*)LockedRectTrail.pBits;


    // Create a terrain mesh from a grey scale height map.
    TerrainVertex* pCurrentVertex;
    for ( int iY = 0; iY < 256; ++iY )
    {
        for ( int iX = 0; iX < 256; ++iX )
        {
            INT iFlipTrailY = 256 - iY;
            FLOAT fCurrentPixel = (FLOAT)( pTextureTerrainData[(iY+256) *1024+(iX+256)] & 0x000000FF );
            FLOAT fCurrentPixelTrail = (FLOAT)( pTextureTrailData[(iFlipTrailY) *256+(iX)] & 0x000000FF );

            FLOAT fCurrentPixelRight = 0;
            FLOAT fCurrentPixelDown = 0;
            FLOAT fCurrentPixelRightDown = 0;            
            FLOAT fCurrentPixelRightTrail = 0;
            FLOAT fCurrentPixelDownTrail = 0;
            FLOAT fCurrentPixelRightDownTrail = 0;            

            if ( iX < 255 )
            {
                fCurrentPixelRight = (FLOAT)( pTextureTerrainData[( iY + 256 ) * 1024 + ( iX + 1 + 256)] & 0x000000F );
                fCurrentPixelRightTrail = (FLOAT)( pTextureTrailData[( iFlipTrailY ) * 256 + ( iX + 1 )] & 0x000000FF );
            }  
            if ( iFlipTrailY< 255 )
            {
                fCurrentPixelDown = (FLOAT)( pTextureTerrainData[(1 + iY + 256) * 1024 + ( iX+256 )] & 0x000000FF );
                fCurrentPixelDownTrail = (FLOAT)( pTextureTrailData[( iFlipTrailY + 1 ) * 256 + ( iX )] & 0x000000FF );
            }
            if ( iX < 255 && iFlipTrailY < 255 )
            {
                fCurrentPixelRightDown = (FLOAT)( pTextureTerrainData[( 1 + iY + 256) * 1024 + ( iX+1+256 )] & 0x000000FF );
                fCurrentPixelRightDownTrail = (FLOAT)( pTextureTrailData[( 1 + iFlipTrailY ) * 256 + ( iX+1 )] & 0x000000FF );
            }

            FLOAT fLerp = (FLOAT)fCurrentPixelTrail / 256.0f;
            FLOAT fLerp_1 = 1.0f - fLerp;
            fLerp *= 0.25f; // dampen height a bit.
            fCurrentPixel =  fCurrentPixel * fLerp_1 + fLerp * fCurrentPixelTrail;
            fCurrentPixelRight = fCurrentPixelRight * fLerp_1 + fLerp * fCurrentPixelRight;
            fCurrentPixelDown = fCurrentPixelDown * fLerp_1 + fLerp * fCurrentPixelDownTrail;
            fCurrentPixelRightDown = fCurrentPixelRightDown * fLerp_1 + fLerp * fCurrentPixelRightDownTrail;            


            FLOAT fBlurredValue = fCurrentPixel + fCurrentPixelDown 
                + fCurrentPixelRight + fCurrentPixelRightDown;
            fBlurredValue *= 0.25f;
            pCurrentVertex = &m_pVerticies[ iY * 256 + iX ];
            // scale the data in x and z by 100 and in y by 30.
            pCurrentVertex->m_vPosition.y = fBlurredValue / 255.0f * 30.0f;
            pCurrentVertex->m_vPosition.x = ( FLOAT )iX / 256.0f * 100.0f;
            pCurrentVertex->m_vPosition.z = ( FLOAT )iY / 256.0f * 100.0f;

            pCurrentVertex->m_vTexCoord1.x = pCurrentVertex->m_vPosition.x;
            pCurrentVertex->m_vTexCoord1.y = pCurrentVertex->m_vPosition.z;
            pCurrentVertex->m_vTexCoord2.x = pCurrentVertex->m_vPosition.x;
            pCurrentVertex->m_vTexCoord2.y = pCurrentVertex->m_vPosition.z;


            XMVECTOR vCenter = XMVectorSet( pCurrentVertex->m_vPosition.x, pCurrentVertex->m_vPosition.y / 3.0f, pCurrentVertex->m_vPosition.z, 0 );
            XMVECTOR vRight = XMVectorSet( ( FLOAT )(iX+1) / 256.0f, fCurrentPixelRight / 255.0f / 3.0f,( FLOAT )iY / 256.0f,0 );
            XMVECTOR vUp = XMVectorSet(  ( FLOAT )iX / 256.0f, fCurrentPixelDown / 255.0f / 3.0f,( FLOAT )(iY+1) / 256.0f, 0 );
            XMVECTOR vNormal = XMVector3Normalize( XMVector3Cross(vUp -vCenter, vRight - vCenter) );
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
    m_pTerrainBuffer->Lock(0, 0, (VOID**)&pVerticies, 0 );
        memcpy(pVerticies, m_pVerticies, 256 * 256 * sizeof(TerrainVertex) );
    m_pTerrainBuffer->Unlock();
    m_pTerrainIndexBuffer->Unlock();
    m_pTextureTerrain->UnlockRect(0);
    m_pTextureTrail->UnlockRect(0);
    return S_OK;
}
