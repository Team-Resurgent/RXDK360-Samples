//--------------------------------------------------------------------------------------
// TexturedMesh.cpp
//
// The simple sample shows how to use D3D to draw a simple textured mesh using
// HLSL shaders.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>


//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
IDirect3D9*         g_pD3D = NULL; // Used to create the D3DDevice
IDirect3DDevice9*   g_pd3dDevice = NULL; // the rendering device


//--------------------------------------------------------------------------------------
// Vertex shader
// We use the register semantic here to directly define the input register
// matWVP.  Conversely, we could let the HLSL compiler decide and check the
// constant table.
//--------------------------------------------------------------------------------------
const CHAR*         g_strVertexShaderProgram =
    "                                              "
    "                                              "
    " float4x4 matWVP : register(c0);              "
    "                                              "
    " struct VS_IN                                 "
    "                                              "
    " {                                            "
    "     float4 ObjPos : POSITION;                "  // Object space position 
    "     float2 UV     : TEXCOORD;                "  // Texture coordinate
    " };                                           "
    "                                              "
    " struct VS_OUT                                "
    " {                                            "
    "     float4 ProjPos  : POSITION;              "  // Projected space position 
    "     float2 UV       : TEXCOORD0;             "
    " };                                           "
    "                                              "
    " VS_OUT main( VS_IN In )                      "
    " {                                            "
    "     VS_OUT Out;                              "
    "     Out.ProjPos = mul( matWVP, In.ObjPos );  "  // Transform vertex into 
    "     Out.UV = In.UV;                          "  // Projected space and
    "     return Out;                              "  // Transfer UVs
    " }                                            ";


//-------------------------------------------------------------------------------------
// Pixel shader
// We use the register semantic here to directly define the input sampler
// ColorTexture.  Conversely, we could let the HLSL compiler decide and check
// the constant table.
//-------------------------------------------------------------------------------------
const CHAR*         g_strPixelShaderProgram =
    " sampler2D ColorTexture : register(s0);       "
    "                                              "
    " struct PS_IN                                 "
    " {                                            "
    "     float2 UV : TEXCOORD0;                   "  // Interpolated UV from
    " };                                           "  // the vertex shader 
    "                                              "
    " float4 main( PS_IN In ) : COLOR              "
    " {                                            "
    "     return tex2D( ColorTexture, In.UV );     "  // Sample texture and output
    " }                                            ";


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    // Create the D3D object
    g_pD3D = Direct3DCreate9( D3D_SDK_VERSION );

    // Set up the structure used to create the D3DDevice
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory( &d3dpp, sizeof( d3dpp ) );
    d3dpp.BackBufferWidth = 1280;
    d3dpp.BackBufferHeight = 720;
    d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dpp.MultiSampleQuality = 0;
    d3dpp.BackBufferCount = 1;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    // Create the Direct3D9 device
    g_pD3D->CreateDevice( 0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING,
                          &d3dpp, &g_pd3dDevice );

    // Buffers to hold compiled shaders and possible error messages
    ID3DXBuffer* pShaderCode = NULL;
    ID3DXBuffer* pErrorMsg = NULL;

    // Compile vertex shader
    HRESULT hr = D3DXCompileShader( g_strVertexShaderProgram, ( UINT )strlen( g_strVertexShaderProgram ),
                                    NULL, NULL, "main", "vs_2_0", 0,
                                    &pShaderCode, &pErrorMsg, NULL );
    if( FAILED( hr ) )
    {
        OutputDebugStringA( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
        exit( 1 );
    }

    // Create vertex shader
    IDirect3DVertexShader9* pVertexShader;
    g_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                      &pVertexShader );

    // Shader code is no longer required
    pShaderCode->Release();
    pShaderCode = NULL;

    // Compile pixel shader
    hr = D3DXCompileShader( g_strPixelShaderProgram, ( UINT )strlen( g_strPixelShaderProgram ),
                            NULL, NULL, "main", "ps_2_0", 0,
                            &pShaderCode, &pErrorMsg, NULL );
    if( FAILED( hr ) )
    {
        OutputDebugStringA( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
        exit( 1 );
    }

    // Create pixel shader
    IDirect3DPixelShader9* pPixelShader;
    g_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                     &pPixelShader );

    // Shader code no longer required
    pShaderCode->Release();
    pShaderCode = NULL;

    // Load texture
    IDirect3DTexture9* pTexture;

    // D3DXCreateTextureFromFile loads a texture and can optionally resize it,
    // filter it, generate mip levels, etc.  It is good for game prototyping
    // but is not suitable for final shipping code due to load time performance
    // reasons.
    hr = D3DXCreateTextureFromFileEx( g_pd3dDevice, "game:\\Media\\Textures\\Rocks.tga",
                                      D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT,
                                      0, D3DFMT_UNKNOWN, D3DPOOL_DEFAULT,
                                      D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL,
                                      &pTexture );
    if( FAILED( hr ) )
        exit( 1 );

    // Make texture format sRGB, since the source image is encoded in sRGB space.
    // Note: this is not the best quality way of doing this - see the ATG::ConvertTextureToGoodSRGB 
    // function in the ATG framework for the full high-quality method.
    pTexture->Format.SignX = GPUSIGN_GAMMA;
    pTexture->Format.SignY = GPUSIGN_GAMMA;
    pTexture->Format.SignZ = GPUSIGN_GAMMA;

    // Define some vertices to draw

    // Structure to hold vertex data
    struct COLORVERTEX
    {
        FLOAT   Position[3];
        FLOAT   UV[2];
    };

    // Triangle array width and height
    const UINT Width = 35;
    const UINT Height = 35;

    // Number of primitives in stripped mesh
    const UINT NumPrimitives = Width * Height * 2 + ( Height - 1 ) * 4;

    // Number of vertices needed in the vertex buffer
    UINT NumVertices = ( Width + 1 ) * ( Height + 1 );

    // Create a vertex buffer
    IDirect3DVertexBuffer9* pVertexBuffer;
    g_pd3dDevice->CreateVertexBuffer( NumVertices * sizeof( COLORVERTEX ),
                                      D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT,
                                      &pVertexBuffer, NULL );

    // Fill the vertex buffer
    COLORVERTEX* pVertices;
    pVertexBuffer->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    for( UINT i = 0; i < Height + 1; i++ )
    {
        for( UINT j = 0; j < Width + 1; j++ )
        {
            XMVECTOR pos;
            pos.x = ( ( FLOAT )j - ( FLOAT )( Width ) / 2.0f ) * 4.0f / Width;
            pos.y = ( ( FLOAT )i - ( FLOAT )( Height ) / 2.0f ) * 4.0f / Height;
            pos.z = 0.2f * cosf( XMVector2Length( pos ).x * 5.0f );
            pVertices->Position[0] = pos.x;
            pVertices->Position[1] = pos.y;
            pVertices->Position[2] = pos.z;

            FLOAT u = ( FLOAT )( j ) / Width;
            FLOAT v = ( FLOAT )( i ) / Width;
            pVertices->UV[0] = u;
            pVertices->UV[1] = v;

            pVertices++;
        }
    }
    pVertexBuffer->Unlock();

    // Number of indices needed in the index buffer
    UINT NumIndices = ( Width + 1 ) * 2 * Height + 2 * ( Height - 1 );

    // Create an index buffer
    IDirect3DIndexBuffer9* pIndexBuffer;
    g_pd3dDevice->CreateIndexBuffer( NumIndices * sizeof( WORD ),
                                     D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
                                     D3DPOOL_DEFAULT, &pIndexBuffer, NULL );
    // Fill the index buffer
    WORD* pIndices;
    pIndexBuffer->Lock( 0, 0, ( VOID** )&pIndices, 0 );
    UINT Index = 0;
    for( INT i = 0; i < Height; i++ )
    {
        // Tri-stripped vertices
        for( INT j = 0; j < Width + 1; j++ )
        {
            pIndices[Index++] = ( WORD )( i * ( Width + 1 ) + j );
            pIndices[Index++] = ( WORD )( ( i + 1 ) * ( Width + 1 ) + j );
        }

        // Degenerate strip connectors
        if( i != Height - 1 )
        {
            pIndices[Index++] = ( WORD )( ( i + 1 ) * ( Width + 1 ) + Width );
            pIndices[Index++] = ( WORD )( ( i + 1 ) * ( Width + 1 ) );
        }
    }
    pIndexBuffer->Unlock();

    // Define the vertex elements
    static const D3DVERTEXELEMENT9 VertexElements[3] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    // Create a vertex declaration from the element descriptions
    IDirect3DVertexDeclaration9* pVertexDecl;
    g_pd3dDevice->CreateVertexDeclaration( VertexElements, &pVertexDecl );

    // World matrix (identity in this sample)
    XMMATRIX matWorld = XMMatrixIdentity();

    // View matrix
    XMVECTOR vEyePt = XMVectorSet( 0.0f, -4.0f, -4.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    XMMATRIX matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )d3dpp.BackBufferWidth / ( FLOAT )d3dpp.BackBufferHeight;

    // Projection matrix
    XMMATRIX matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, 1.0f, 200.0f );

    // World*view*projection
    XMMATRIX matWVP = matWorld * matView * matProj;

    // The main render loop
    for(; ; )
    {
        // Clear the backbuffer
        g_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                             0xff000000, 1.0f, 0L );

        // Set shaders
        g_pd3dDevice->SetVertexShader( pVertexShader );
        g_pd3dDevice->SetPixelShader( pPixelShader );

        // Set shader constants
        g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );

        // Set the vertex declaration
        g_pd3dDevice->SetVertexDeclaration( pVertexDecl );

        // Configure sampler 0 for trilinear sampling
        g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
        g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

        // Set texture 0
        g_pd3dDevice->SetTexture( 0, pTexture );

        // Set the stream source
        g_pd3dDevice->SetStreamSource( 0, pVertexBuffer, 0, sizeof( COLORVERTEX ) );

        // Set the index buffer
        g_pd3dDevice->SetIndices( pIndexBuffer );

        // Draw the triangle strip
        g_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLESTRIP, 0, 0,
                                            NumVertices, 0, NumPrimitives );

        // Present the backbuffer contents to the display
        g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    }
}
