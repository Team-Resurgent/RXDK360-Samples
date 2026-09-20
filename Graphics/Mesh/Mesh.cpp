//--------------------------------------------------------------------------------------
// Mesh.cpp
//
// The simple sample shows how to use D3D to draw a simple mesh using HLSL shaders.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
IDirect3D9*         g_pD3D = NULL; // Used to create the D3D Device
IDirect3DDevice9*   g_pd3dDevice = NULL; // The rendering device


//--------------------------------------------------------------------------------------
// Vertex shader
// We use the register semantic here to directly define the input register
// matWVP.  Conversely, we could let the HLSL compiler decide and check the
// constant table.
//--------------------------------------------------------------------------------------
const CHAR*         g_strVertexShaderProgram =
    " float4x4 matWVP : register(c0);              \n"
    "                                              \n"
    " struct VS_IN                                 \n"
    "                                              \n"
    " {                                            \n"
    "     float4 ObjPos   : POSITION;              \n"  // Object space position 
    "     float4 Color    : COLOR;                 \n"  // Vertex color                 
    " };                                           \n"
    "                                              \n"
    " struct VS_OUT                                \n"
    " {                                            \n"
    "     float4 ProjPos  : POSITION;              \n"  // Projected space position 
    "     float4 Color    : COLOR;                 \n"
    " };                                           \n"
    "                                              \n"
    " VS_OUT main( VS_IN In )                      \n"
    " {                                            \n"
    "     VS_OUT Out;                              \n"
    "     Out.ProjPos = mul( matWVP, In.ObjPos );  \n"  // Transform vertex into
    "     Out.Color = In.Color;                    \n"  // Projected space and 
    "     return Out;                              \n"  // Transfer color
    " }                                            \n";


//-------------------------------------------------------------------------------------
// Pixel shader
//-------------------------------------------------------------------------------------
const CHAR*         g_strPixelShaderProgram =
    " struct PS_IN                                 \n"
    " {                                            \n"
    "     float4 Color : COLOR;                    \n"  // Interpolated color from                      
    " };                                           \n"  // Vertex shader
    "                                              \n"
    " float4 main( PS_IN In ) : COLOR              \n"
    " {                                            \n"
    "     return In.Color;                         \n"  // Output color
    " }                                            \n";


//--------------------------------------------------------------------------------------
// Name: main
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

    // Create the Direct3D device
    g_pD3D->CreateDevice( 0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING,
                          &d3dpp, &g_pd3dDevice );

    // Buffers to hold compiled shaders and possible error messages
    ID3DXBuffer* pShaderCode = NULL;
    ID3DXBuffer* pErrorMsg = NULL;

    // Compile vertex shader
    HRESULT hr = D3DXCompileShader( g_strVertexShaderProgram, ( UINT )strlen( g_strVertexShaderProgram ),
                                    NULL, NULL, "main", "vs.2.0", 0,
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
                            NULL, NULL, "main", "ps.2.0", 0,
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

    // Structure to hold vertex data
    struct COLORVERTEX
    {
        FLOAT Position[3];
        DWORD Color;
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

            BYTE Red = ( BYTE )( FLOAT( i ) / Height * 255.0f );
            BYTE Blue = ( BYTE )( FLOAT( j ) / Width * 255.0f );
            pVertices->Color = D3DCOLOR_ARGB( 0, Red, 0, Blue );

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
        { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
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

    // Main render loop
    for(; ; )
    {
        // Clear the backbuffer.
        g_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                             0xff000000, 1.0f, 0L );

        // Set shaders
        g_pd3dDevice->SetVertexShader( pVertexShader );
        g_pd3dDevice->SetPixelShader( pPixelShader );

        // Set shader constants
        g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );

        // Set the vertex declaration
        g_pd3dDevice->SetVertexDeclaration( pVertexDecl );

        // Set the stream source
        g_pd3dDevice->SetStreamSource( 0, pVertexBuffer, 0, sizeof( COLORVERTEX ) );

        // Set the index buffer
        g_pd3dDevice->SetIndices( pIndexBuffer );

        // Draw the triangle strip
        g_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLESTRIP,
                                            0, 0, NumVertices, 0, NumPrimitives );

        // Present the backbuffer contents to the display
        g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    }
}
