//-------------------------------------------------------------------------------------
// Road.cpp
//  
// The road driven on in this sample.
//  
// Microsoft XNA Developer Connection
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "Road.h"
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Vertex shader for the road
//--------------------------------------------------------------------------------------
const CHAR* m_strVertexShaderProgram =
    " float4x4 matWVP : register(c0);              \n"
    "                                              \n"
    " struct VS_IN                                 \n"
    "                                              \n"
    " {                                            \n"
    "     float4 ObjPos   : POSITION;              \n"
    "     float2 Texture  : TEXCOORD0;             \n"
    " };                                           \n"
    "                                              \n"
    " struct VS_OUT                                \n"
    " {                                            \n"
    "     float4 ProjPos  : POSITION;              \n"
    "     float2 Texture  : TEXCOORD0;             \n"
    " };                                           \n"
    "                                              \n"
    " VS_OUT main( VS_IN In )                      \n"
    " {                                            \n"
    "     VS_OUT Out;                              \n"
    "     Out.ProjPos = mul( matWVP, In.ObjPos );  \n"
    "     Out.Texture = In.Texture;                \n"
    "     return Out;                              \n"
    " }                                            \n";


//-------------------------------------------------------------------------------------
// Pixel shader
//-------------------------------------------------------------------------------------
const CHAR* m_strPixelShaderProgram =
    " struct PS_IN                                     \n"
    " {                                                \n"
    "     float2 Texture : TEXCOORD0;                  \n"
    " };                                               \n"
    "                                                  \n"
    " sampler TextureSampler0 : register(s0);          \n"
    "                                                  \n"
    " float4 main( PS_IN In ) : COLOR                  \n"
    " {                                                \n"
    "     return tex2D( TextureSampler0, In.Texture ); \n"
    " }                                                \n";


//--------------------------------------------------------------------------------------
// The vertex structure for the road vertices.
//--------------------------------------------------------------------------------------
struct ROADVERTEX
{
    XMFLOAT3 m_vPosition;
    XMFLOAT2 m_vTexture;
};


//--------------------------------------------------------------------------------------
// Constructor that creates all of the Direct3D resources for the road.
//--------------------------------------------------------------------------------------
Road::Road()
{
}


//--------------------------------------------------------------------------------------
// Destructor that releases all of the Direct3D resources for the road.
//--------------------------------------------------------------------------------------
Road::~Road() 
{
}


//--------------------------------------------------------------------------------------
// Draws the road on the stored graphics device, using the given view and projection.
//--------------------------------------------------------------------------------------
VOID Road::Draw( CXMMATRIX matView, CXMMATRIX matProj )
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

    // Render the road
    m_pd3dDevice->SetPixelShader( m_pPixelShader );
    m_pd3dDevice->SetTexture( 0, m_pTexture );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDeclaration );
    m_pd3dDevice->SetVertexShader( m_pVertexShader );
    m_pd3dDevice->SetStreamSource( 0, m_pVB, 0, sizeof( ROADVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, s_dwNumRoadPoints );
}


//--------------------------------------------------------------------------------------
// Create the Direct3D resources for the road.
//--------------------------------------------------------------------------------------
HRESULT Road::CreateGraphicsResources( IDirect3DDevice9* pd3dDevice, ATG::PackedResource& resource )
{
    HRESULT hr;

    m_pd3dDevice = pd3dDevice;

    // Load the road texture
    m_pTexture = resource.GetTexture( "RoadTexture" );

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

    // Create the vertex declaration.
    static const D3DVERTEXELEMENT9 VertexElements[3] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    if( FAILED( hr = m_pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pVertexDeclaration ) ) )
    {
        ATG_PrintError( "Couldn't create vertex declaration.\n" );
        return hr;
    }

    // Create the vertex buffer
    if( FAILED( hr = m_pd3dDevice->CreateVertexBuffer( s_dwNumRoadPoints * 400 * sizeof( ROADVERTEX ),
                                                       D3DUSAGE_WRITEONLY,
                                                       0, 0, &m_pVB, NULL ) ) )
    {
        ATG_PrintError( "Couldn't create vertex buffer.\n" );
        return hr;
    }

    // Fill the vertex buffer
    BuildRoad();

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: BuildRoad()
// Desc: Create road vertex buffer from randomly oriented road segments.
//--------------------------------------------------------------------------------------
VOID Road::BuildRoad()
{
    srand( GetTickCount() );
   
    const FLOAT fRoadSegmentLength = 50.0f;

    // Each road segments has 3 quads, the left and right large quads and the center
    // road quad
    XMVECTOR vPos = {0};
    FLOAT fDirection = 0;
    FLOAT fDirectionRate = 0.0f;
    ROADVERTEX* pVertices;
    m_pVB->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    for( DWORD i = 0; i < s_dwNumRoadPoints * 3; i += 3 )
    {
        DWORD dwVertexIndexLeft = i * 4;
        DWORD dwVertexIndexMiddle = ( i + 1 ) * 4;
        DWORD dwVertexIndexRight = ( i + 2 ) * 4;

        // Calculate vector along road directions
        XMVECTOR vNextPos = {0};

        vNextPos.x = vPos.x + fRoadSegmentLength * sin( fDirection );
        vNextPos.z = vPos.z + fRoadSegmentLength * cos( fDirection );

        XMVECTOR vNormal;
        vNormal.x = -( vNextPos.z - vPos.z );
        vNormal.z = ( vNextPos.x - vPos.x );
        FLOAT fLength = sqrt( vNormal.x * vNormal.x + vNormal.z * vNormal.z );
        vNormal.x /= fLength;
        vNormal.z /= fLength;

        m_vRoadVector[i / 3] = vPos;

        if( i != 0 )
        {
            // Copy vertices from last segment
            DWORD dwLastIndexLeft = ( i - 3 ) * 4;
            DWORD dwLastIndexMiddle = ( i - 2 ) * 4;
            DWORD dwLastIndexRight = ( i - 1 ) * 4;

            // Texture coordinates for the left and right quads are sampled from the main
            // texture's grass region
            pVertices[dwVertexIndexLeft] = pVertices[dwLastIndexLeft + 3];
            pVertices[dwVertexIndexLeft].m_vTexture.x = 0.0f;
            pVertices[dwVertexIndexLeft].m_vTexture.y = 0.04f;
            pVertices[dwVertexIndexLeft + 1] = pVertices[dwLastIndexLeft + 2];
            pVertices[dwVertexIndexLeft + 1].m_vTexture.x = 0.2f;
            pVertices[dwVertexIndexLeft + 1].m_vTexture.y = 0.04f;

            // Texture coordinates for the middle (road) quad, uses the full road texture
            pVertices[dwVertexIndexMiddle] = pVertices[dwLastIndexMiddle + 3];
            pVertices[dwVertexIndexMiddle].m_vTexture.x = 0.0f;
            pVertices[dwVertexIndexMiddle].m_vTexture.y = 1.0f;
            pVertices[dwVertexIndexMiddle + 1] = pVertices[dwLastIndexMiddle + 2];
            pVertices[dwVertexIndexMiddle + 1].m_vTexture.x = 1.0f;
            pVertices[dwVertexIndexMiddle + 1].m_vTexture.y = 1.0f;

            pVertices[dwVertexIndexRight] = pVertices[dwLastIndexRight + 3];
            pVertices[dwVertexIndexRight].m_vTexture.x = 0.0f;
            pVertices[dwVertexIndexRight].m_vTexture.y = 0.04f;
            pVertices[dwVertexIndexRight + 1] = pVertices[dwLastIndexRight + 2];
            pVertices[dwVertexIndexRight + 1].m_vTexture.x = 0.2f;
            pVertices[dwVertexIndexRight + 1].m_vTexture.y = 0.04f;
        }
        else
        {
            // First segment, generate first row of vertices
            pVertices[dwVertexIndexLeft].m_vPosition.x = vPos.x - ( 10.0f * fRoadSegmentLength * vNormal.x );
            pVertices[dwVertexIndexLeft].m_vPosition.y = 0;
            pVertices[dwVertexIndexLeft].m_vPosition.z = vPos.z - ( 10.0f * fRoadSegmentLength * vNormal.z );
            pVertices[dwVertexIndexLeft].m_vTexture.x = 0.0f;
            pVertices[dwVertexIndexLeft].m_vTexture.y = 0.04f;
            pVertices[dwVertexIndexLeft + 1].m_vPosition.x = vPos.x - ( fRoadSegmentLength * vNormal.x );
            pVertices[dwVertexIndexLeft + 1].m_vPosition.y = 0;
            pVertices[dwVertexIndexLeft + 1].m_vPosition.z = vPos.z - ( fRoadSegmentLength * vNormal.z );
            pVertices[dwVertexIndexLeft + 1].m_vTexture.x = 0.2f;
            pVertices[dwVertexIndexLeft + 1].m_vTexture.y = 0.04f;

            pVertices[dwVertexIndexMiddle].m_vPosition.x = vPos.x - ( fRoadSegmentLength * vNormal.x );
            pVertices[dwVertexIndexMiddle].m_vPosition.y = 0;
            pVertices[dwVertexIndexMiddle].m_vPosition.z = vPos.z - ( fRoadSegmentLength * vNormal.z );
            pVertices[dwVertexIndexMiddle].m_vTexture.x = 0.0f;
            pVertices[dwVertexIndexMiddle].m_vTexture.y = 1.0f;
            pVertices[dwVertexIndexMiddle + 1].m_vPosition.x = vPos.x + ( fRoadSegmentLength * vNormal.x );
            pVertices[dwVertexIndexMiddle + 1].m_vPosition.y = 0;
            pVertices[dwVertexIndexMiddle + 1].m_vPosition.z = vPos.z + ( fRoadSegmentLength * vNormal.z );
            pVertices[dwVertexIndexMiddle + 1].m_vTexture.x = 1.0f;
            pVertices[dwVertexIndexMiddle + 1].m_vTexture.y = 1.0f;

            pVertices[dwVertexIndexRight].m_vPosition.x = vPos.x + ( fRoadSegmentLength * vNormal.x );
            pVertices[dwVertexIndexRight].m_vPosition.y = 0;
            pVertices[dwVertexIndexRight].m_vPosition.z = vPos.z + ( fRoadSegmentLength * vNormal.z );
            pVertices[dwVertexIndexRight].m_vTexture.x = 0.0f;
            pVertices[dwVertexIndexRight].m_vTexture.y = 0.04f;
            pVertices[dwVertexIndexRight + 1].m_vPosition.x = vPos.x + ( 10.0f * fRoadSegmentLength * vNormal.x );
            pVertices[dwVertexIndexRight + 1].m_vPosition.y = 0;
            pVertices[dwVertexIndexRight + 1].m_vPosition.z = vPos.z + ( 10.0f * fRoadSegmentLength * vNormal.z );
            pVertices[dwVertexIndexRight + 1].m_vTexture.x = 0.2f;
            pVertices[dwVertexIndexRight + 1].m_vTexture.y = 0.04f;
        }

        // Add vertices to the last row of the segment
        pVertices[dwVertexIndexLeft + 2].m_vPosition.x = vNextPos.x - ( fRoadSegmentLength * vNormal.x );
        pVertices[dwVertexIndexLeft + 2].m_vPosition.y = 0;
        pVertices[dwVertexIndexLeft + 2].m_vPosition.z = vNextPos.z - ( fRoadSegmentLength * vNormal.z );
        pVertices[dwVertexIndexLeft + 2].m_vTexture.x = 0.2f;
        pVertices[dwVertexIndexLeft + 2].m_vTexture.y = 0.0f;
        pVertices[dwVertexIndexLeft + 3].m_vPosition.x = vNextPos.x - ( 10.0f * fRoadSegmentLength * vNormal.x );
        pVertices[dwVertexIndexLeft + 3].m_vPosition.y = 0;
        pVertices[dwVertexIndexLeft + 3].m_vPosition.z = vNextPos.z - ( 10.0f * fRoadSegmentLength * vNormal.z );
        pVertices[dwVertexIndexLeft + 3].m_vTexture.x = 0.0f;
        pVertices[dwVertexIndexLeft + 3].m_vTexture.y = 0.0f;

        pVertices[dwVertexIndexMiddle + 2].m_vPosition.x = vNextPos.x + ( fRoadSegmentLength * vNormal.x );
        pVertices[dwVertexIndexMiddle + 2].m_vPosition.y = 0;
        pVertices[dwVertexIndexMiddle + 2].m_vPosition.z = vNextPos.z + ( fRoadSegmentLength * vNormal.z );
        pVertices[dwVertexIndexMiddle + 2].m_vTexture.x = 1.0f;
        pVertices[dwVertexIndexMiddle + 2].m_vTexture.y = 0.0f;
        pVertices[dwVertexIndexMiddle + 3].m_vPosition.x = vNextPos.x - ( fRoadSegmentLength * vNormal.x );
        pVertices[dwVertexIndexMiddle + 3].m_vPosition.y = 0;
        pVertices[dwVertexIndexMiddle + 3].m_vPosition.z = vNextPos.z - ( fRoadSegmentLength * vNormal.z );
        pVertices[dwVertexIndexMiddle + 3].m_vTexture.x = 0.0f;
        pVertices[dwVertexIndexMiddle + 3].m_vTexture.y = 0.0f;

        pVertices[dwVertexIndexRight + 2].m_vPosition.x = vNextPos.x + ( 10.0f * fRoadSegmentLength * vNormal.x );
        pVertices[dwVertexIndexRight + 2].m_vPosition.y = 0;
        pVertices[dwVertexIndexRight + 2].m_vPosition.z = vNextPos.z + ( 10.0f * fRoadSegmentLength * vNormal.z );
        pVertices[dwVertexIndexRight + 2].m_vTexture.x = 0.2f;
        pVertices[dwVertexIndexRight + 2].m_vTexture.y = 0.0f;
        pVertices[dwVertexIndexRight + 3].m_vPosition.x = vNextPos.x + ( fRoadSegmentLength * vNormal.x );
        pVertices[dwVertexIndexRight + 3].m_vPosition.y = 0;
        pVertices[dwVertexIndexRight + 3].m_vPosition.z = vNextPos.z + ( fRoadSegmentLength * vNormal.z );
        pVertices[dwVertexIndexRight + 3].m_vTexture.x = 0.0f;
        pVertices[dwVertexIndexRight + 3].m_vTexture.y = 0.0f;

        vPos = vNextPos;

        // Randomly change the curvature of the road
        DWORD dwRand = rand() % 10;
        if( dwRand == 0 )
            fDirectionRate = -0.2f;
        if( dwRand == 1 )
            fDirectionRate = 0.2f;
        if( dwRand == 2 )
            fDirectionRate = -0.1f;
        if( dwRand == 3 )
            fDirectionRate = 0.1f;
        if( dwRand > 8 )
            fDirectionRate = 0.0f;

        if( fDirection < -( XM_PI / 2 ) )
            fDirection = -( XM_PI / 2 );

        if( fDirection > ( XM_PI / 2 ) )
            fDirection = ( XM_PI / 2 );

        fDirection += fDirectionRate;
    }
    m_pVB->Unlock();
}