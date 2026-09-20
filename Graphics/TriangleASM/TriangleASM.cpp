//--------------------------------------------------------------------------------------
// TriangleASM.cpp
//
// The simple sample shows how to use D3D to draw a simple triangle using
// assembly defined shaders.
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
//--------------------------------------------------------------------------------------
const CHAR g_strVertexShaderProgram[] =
    "vs.2.0\n"              // Vertex shader version
    "dcl_position v0\n"     // Position data from the vertex stream    
    "dcl_color v1\n"        // Color data from the vertex stream         
    ""
    "mul r0,v0.x,c0\n"      // Transform vertex position by           
    "mad r0,v0.y,c1,r0\n"   // World*view*projection matrix in c0:c3  
    "mad r0,v0.z,c2,r0\n"
    "mad r0,v0.w,c3,r0\n"
    "mov oPos, r0\n"
    ""
    "mov oD0,v1";           // Output vertex color                    


//--------------------------------------------------------------------------------------
// Pixel shader
//--------------------------------------------------------------------------------------
const CHAR g_strPixelShaderProgram[] =
    "ps.2.0\n"      // Pixel shader version
    "dcl v0\n"      // Interpolated color data from vertex shader                 
    ""
    "mov oC0,v0";   // Output vertex color                   


//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    // Create the D3D object.
    g_pD3D = Direct3DCreate9( D3D_SDK_VERSION );

    // Set up the structure used to create the D3DDevice.
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory( &d3dpp, sizeof( d3dpp ) );
    d3dpp.BackBufferWidth = 1280;
    d3dpp.BackBufferHeight = 720;
    d3dpp.BackBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dpp.MultiSampleQuality = 0;
    d3dpp.BackBufferCount = 1;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    // Create the Direct3D device.
    g_pD3D->CreateDevice( 0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING,
                          &d3dpp, &g_pd3dDevice );

    // Buffers to hold compiled shaders and possible error messages
    ID3DXBuffer* pShaderCode = NULL;
    ID3DXBuffer* pErrorMsg = NULL;

    // Assemble vertex shader.
    HRESULT hr = D3DXAssembleShader( g_strVertexShaderProgram, sizeof( g_strVertexShaderProgram )-1,
                                     NULL, NULL, 0, &pShaderCode, &pErrorMsg );
    if( FAILED( hr ) )
    {
        OutputDebugStringA( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
        exit( 1 );
    }

    // Create vertex shader.
    IDirect3DVertexShader9* pVertexShader;
    g_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                      &pVertexShader );

    // Shader code is no longer required.
    pShaderCode->Release();
    pShaderCode = NULL;

    // Assemble pixel shader.
    hr = D3DXAssembleShader( g_strPixelShaderProgram, sizeof( g_strPixelShaderProgram )-1,
                             NULL, NULL, 0, &pShaderCode, &pErrorMsg );
    if( FAILED( hr ) )
    {
        OutputDebugStringA( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
        exit( 1 );
    }

    // Create pixel shader.
    IDirect3DPixelShader9* pPixelShader;
    g_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                     &pPixelShader );

    // Shader code no longer required.
    pShaderCode->Release();
    pShaderCode = NULL;

    // Structure to hold vertex data.
    struct COLORVERTEX
    {
        FLOAT Position[3];
        DWORD Color;
    };

    static const COLORVERTEX Vertices[3] =
    {
        { -1.0f, -1.0f, 0.0f, 0x00FF0000 },
        {  0.0f,  1.0f, 0.0f, 0x0000FF00 },
        {  1.0f, -1.0f, 0.0f, 0x000000FF }
    };

    // Define the vertex elements.
    static const D3DVERTEXELEMENT9 VertexElements[3] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
        D3DDECL_END()
    };

    // Create a vertex declaration from the element descriptions.
    IDirect3DVertexDeclaration9* pVertexDecl;
    g_pd3dDevice->CreateVertexDeclaration( VertexElements, &pVertexDecl );

    // World matrix (identity in this sample)
    XMMATRIX matWorld = XMMatrixIdentity();

    // View matrix
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -4.0f, 0.0f );
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

        // Set shaders.
        g_pd3dDevice->SetVertexShader( pVertexShader );
        g_pd3dDevice->SetPixelShader( pPixelShader );

        // Set shader constants.
        g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );

        // Set the vertex declaration.
        g_pd3dDevice->SetVertexDeclaration( pVertexDecl );

        // Draw the vertices.  Note that it is more efficient to draw large amounts
        // of vertices using vertex and index buffers.
        g_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLELIST, 1, Vertices, sizeof( COLORVERTEX ) );

        // Present the backbuffer contents to the display.
        g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    }
}
