//--------------------------------------------------------------------------------------
// MicrocodeMemExport.cpp
//
// The simple sample shows how to use D3D to draw a simple triangle using HLSL
// shaders.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <d3d9gpu.h>

// Export shader header file
#include "MemExportShader.h"

//--------------------------------------------------------------------------------------
// Globals
//-------------------------------------------------------------------------------------
IDirect3D9*         g_pD3D = NULL; // Used to create the D3DDevice
IDirect3DDevice9*   g_pd3dDevice = NULL; // the rendering device


//--------------------------------------------------------------------------------------
// Vertex shader
// Pass-through vertex shader to use memory export results
//--------------------------------------------------------------------------------------
const CHAR*         g_strPassThroughVertexShaderProgram =
    " struct VS_IN                                 \n"
    " {                                            \n"
    "     float4 ObjPos   : POSITION;              \n"  // Object space position 
    "     float4 Color    : COLOR;                 \n"  // Vertex color                 
    " };                                           \n"
    "                                              \n"
    " struct VS_OUT                                \n"
    " {                                            \n"
    "     float4 ObjPos   : POSITION;              \n"
    "     float4 Color    : COLOR;                 \n"
    " };                                           \n"
    "                                              \n"
    " VS_OUT main( VS_IN In )                      \n"
    " {                                            \n"
    "     VS_OUT Out;                              \n"
    "     Out.ObjPos = In.ObjPos;                  \n"
    "     Out.Color = In.Color;                    \n"
    "     return Out;                              \n"
    " }                                            \n";


//--------------------------------------------------------------------------------------
// Pixel shader
//--------------------------------------------------------------------------------------
const CHAR*         g_strPixelShaderProgram =
    " struct PS_IN                                 \n"
    " {                                            \n"
    "     float4 Color : COLOR;                    \n"  // Interpolated color from                      
    " };                                           \n"  // the vertex shader
    "                                              \n"
    " float4 main( PS_IN In ) : COLOR              \n"
    " {                                            \n"
    "     return In.Color;                         \n"  // Output color
    " }                                            \n";


//--------------------------------------------------------------------------------------
// Name: main()
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
    d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
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

    // Create microcode shader export vertex shader - located in MemExportShader.h
    IDirect3DVertexShader9* pMemExportVertexShader;
    g_pd3dDevice->CreateVertexShader( g_xvs_main,
                                      &pMemExportVertexShader );


    // Compile pass-through vertex shader.
    HRESULT hr = D3DXCompileShader( g_strPassThroughVertexShaderProgram,
                                    ( UINT )strlen( g_strPassThroughVertexShaderProgram ),
                                    NULL, NULL, "main", "vs_2_0", 0,
                                    &pShaderCode, &pErrorMsg, NULL );
    if( FAILED( hr ) )
    {
        OutputDebugStringA( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
        exit( 1 );
    }

    // Create vertex shader.
    IDirect3DVertexShader9* pPassThroughVertexShader;
    g_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                      &pPassThroughVertexShader );

    // Shader code is no longer required.
    pShaderCode->Release();
    pShaderCode = NULL;

    // Compile pixel shader.
    hr = D3DXCompileShader( g_strPixelShaderProgram, ( UINT )strlen( g_strPixelShaderProgram ),
                            NULL, NULL, "main", "ps_2_0", 0,
                            &pShaderCode, &pErrorMsg, NULL );
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

    // Structure to hold vertex data- we use Vector4 since each element of a memory export structure
    // must be the same size
    struct COLORVERTEX
    {
        XMFLOAT4 Position;
        XMFLOAT4 Color;
    };

    // Triangle vertices
    COLORVERTEX Vertices[3] =
    {
        { XMFLOAT4( -1.0f, -1.0f, 0.0f, 1.0f ), XMFLOAT4( 1.0f, 0.0f, 0.0f, 1.0f ) },
        { XMFLOAT4( 0.0f, 1.0f, 0.0f, 1.0f ), XMFLOAT4( 0.0f, 1.0f, 0.0f, 1.0f ) },
        { XMFLOAT4( 1.0f, -1.0f, 0.0f, 1.0f ), XMFLOAT4( 0.0f, 0.0f, 1.0f, 1.0f ) }
    };

    // Define the vertex elements.
    static const D3DVERTEXELEMENT9 VertexElements[3] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 16, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
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


    // Create a vertex buffer for memory export
    //      The typical use for memory export is to output transformed vertices from
    //      skinning operations, which works with this method.

    IDirect3DVertexBuffer9* pMemExportVB;
    g_pd3dDevice->CreateVertexBuffer( 3 * sizeof( COLORVERTEX ), D3DUSAGE_WRITEONLY, 0,
                                      D3DPOOL_MANAGED, &pMemExportVB, NULL );

    // Get a pointer to the memory export vertex buffer
    VOID* pbData = NULL;
    pMemExportVB->Lock( 0, 3 * sizeof( COLORVERTEX ), &pbData, 0 );
    pMemExportVB->Unlock();

    // Set up the Memory Export Stream Constant using the macro defined in d3d9gpu.h
    GPU_MEMEXPORT_STREAM_CONSTANT streamConstant;
    GPU_SET_MEMEXPORT_STREAM_CONSTANT( &streamConstant,
                                       ( ( BYTE* )pbData ),                               // pointer to the data
                                       6,                                              // max index = # of vertices * stride
                                       SURFACESWAP_LOW_RED,                            // whether to output ABGR or ARGB           
                                       GPUSURFACENUMBER_FLOAT,                         // data type 
                                       GPUCOLORFORMAT_32_32_32_32_FLOAT,               // data format
                                       GPUENDIAN128_8IN32 );                            // endian swap

    // Main render loop
    for(; ; )
    {
        // Clear the backbuffer.
        g_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                             D3DCOLOR_ARGB( 0, 0, 0, 0 ), 1.0f, 0L );

        g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
        g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
        g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
        g_pd3dDevice->SetPixelShader( pPixelShader );

        // Notify the device that an export is beginning into the given vertex buffer
        g_pd3dDevice->BeginExport( 0, pMemExportVB, D3DBEGINEXPORT_VERTEXSHADER );

        // Set up the vertex shader
        g_pd3dDevice->SetVertexShader( pMemExportVertexShader );
        g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );
        g_pd3dDevice->SetVertexDeclaration( pVertexDecl );

        // Set the Memory Export Stream Constant derived from the vertex buffer
        g_pd3dDevice->SetVertexShaderConstantF( 4, streamConstant.c, 1 );

        // Draw the vertices.  Note that it is more efficient to draw large amounts
        // of vertices using vertex and index buffers.            
        g_pd3dDevice->DrawPrimitiveUP( D3DPT_POINTLIST, 3, Vertices, sizeof( COLORVERTEX ) );
        g_pd3dDevice->EndExport( 0, pMemExportVB, 0 );

        // Now set the shaders to use the exported vertices            
        g_pd3dDevice->SetVertexShader( pPassThroughVertexShader );
        g_pd3dDevice->SetVertexDeclaration( pVertexDecl );

        // Draw a vertex buffer using the already-transformed vertices
        g_pd3dDevice->SetStreamSource( 0, pMemExportVB, 0, sizeof( COLORVERTEX ) );
        g_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, 1 );

        // Present the backbuffer contents to the display.
        g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    }
}
