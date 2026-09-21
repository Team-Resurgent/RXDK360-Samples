//--------------------------------------------------------------------------------------
// HLSLConstantTable.cpp
//
// Shows how to use the constant table in HLSL
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
// matWVP.  The other constants will be defined using the HLSL constant table
//--------------------------------------------------------------------------------------
const CHAR*         g_strVertexShaderProgram =

    " float4x4  matWVP : register(c0);                          \n"  // Explicit register
    " float4    ambientColor = ( 0.3, 0.3, 0.3, 0.0 );          \n"
    "                                                           \n"
    " struct Light                                              \n"
    " {                                                         \n"
    "     float4 Pos;                                           \n"
    "     float4 Color;                                         \n"
    "     float  Radius;                                        \n"
    " };                                                        \n"
    "                                                           \n"
    " Light     lights[2];                                      \n"
    "                                                           \n"
    " struct VS_IN                                              \n"
    " {                                                         \n"
    "     float4 ObjPos   : POSITION;                           \n"  // Object space position 
    "     float4 Color    : COLOR;                              \n"  // Vertex color                 
    " };                                                        \n"
    "                                                           \n"
    " struct VS_OUT                                             \n"
    " {                                                         \n"
    "     float4 ProjPos  : POSITION;                           \n"  // Projected position 
    "     float4 Color    : COLOR;                              \n"
    " };                                                        \n"
    "                                                           \n"
    " VS_OUT main( VS_IN In )                                   \n"
    " {                                                         \n"
    "     VS_OUT Out;                                           \n"
    "     float  Dist, Intensity, Radius;                       \n"
    "                                                           \n"
    "     Out.ProjPos = mul( matWVP, In.ObjPos );               \n"  // Transform vertex
    "                                                           \n"
    "     Out.Color = ambientColor * In.Color;                  \n"  // Add in ambient
    "                                                           \n"
    "     Dist = distance( In.ObjPos, lights[0].Pos );          \n"  // Add in color from 
    "     Radius = lights[0].Radius;                            \n"  // the first light
    "     Intensity = 1.0 - ( Dist*Dist ) / ( Radius*Radius );  \n"
    "     Intensity = saturate( Intensity );                    \n"
    "     Out.Color += Intensity * In.Color * lights[0].Color;  \n"
    "                                                           \n"
    "     Dist = distance( In.ObjPos, lights[1].Pos );          \n"  // Add in color from
    "     Radius = lights[1].Radius;                            \n"  // the second light
    "     Intensity = 1.0 - ( Dist*Dist ) / ( Radius*Radius );  \n"
    "     Intensity = saturate( Intensity );                    \n"
    "     Out.Color += Intensity * In.Color * lights[1].Color;  \n"
    "                                                           \n"
    "     return Out;                                           \n"  // Transfer color
    " }                                                         \n";


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
// Name: main
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    // Create the D3D object.
    g_pD3D = Direct3DCreate9( D3D_SDK_VERSION );

    // Set up the structure used to create the D3DDevice.
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory( &d3dpp, sizeof( d3dpp ) );
    d3dpp.BackBufferWidth = 1280;
    d3dpp.BackBufferHeight = 720 ;
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

    // Rhe constant table structure which will be created by D3DXCompileShader
    LPD3DXCONSTANTTABLE pVSConstantTable;

    // Compile vertex shader. The constant table is created into pVSConstantTable
    HRESULT hr = D3DXCompileShader( g_strVertexShaderProgram, ( UINT )strlen( g_strVertexShaderProgram ),
                                    NULL, NULL, "main", "vs.2.0", 0,
                                    &pShaderCode, &pErrorMsg, &pVSConstantTable );
    if( FAILED( hr ) )
    {
        OutputDebugStringA( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
        exit( 1 );
    }

    // Create pixel shader.
    IDirect3DVertexShader9* pVertexShader;
    g_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                      &pVertexShader );

    // Shader code is no longer required.
    pShaderCode->Release();
    pShaderCode = NULL;

    // Note: The pixel shader could also have a constant table, but doesn't
    // for the purposes of this sample

    // Compile pixel shader.
    hr = D3DXCompileShader( g_strPixelShaderProgram, ( UINT )strlen( g_strPixelShaderProgram ),
                            NULL, NULL, "main", "ps.2.0", 0,
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

    // Structure to hold vertex data.
    struct COLORVERTEX
    {
        FLOAT Position[3];
        DWORD Color;
    };

    // Triangle array width and height.
    const UINT Width = 35;
    const UINT Height = 35;

    // Number of primitives in stripped mesh
    const UINT NumPrimitives = Width * Height * 2 + ( Height - 1 ) * 4;

    // Number of vertices needed in the vertex buffer.
    UINT NumVertices = ( Width + 1 ) * ( Height + 1 );

    // Create a vertex buffer.
    IDirect3DVertexBuffer9* pVertexBuffer;
    g_pd3dDevice->CreateVertexBuffer( NumVertices * sizeof( COLORVERTEX ),
                                      D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT,
                                      &pVertexBuffer, NULL );

    // Fill the vertex buffer.
    COLORVERTEX* pVertices;
    pVertexBuffer->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    UINT Vertex = 0;
    for( UINT i = 0; i < Height + 1; i++ )
    {
        for( UINT j = 0; j < Width + 1; j++ )
        {
            FLOAT x = ( ( FLOAT )j - ( FLOAT )( Width ) / 2.0f ) * 4.0f / Width;
            FLOAT y = ( ( FLOAT )i - ( FLOAT )( Height ) / 2.0f ) * 4.0f / Height;
            FLOAT z = 0.0f;

            pVertices[Vertex].Position[0] = x;
            pVertices[Vertex].Position[1] = y;
            pVertices[Vertex].Position[2] = z;

            pVertices[Vertex].Color = D3DCOLOR_ARGB( 0, 255, 255, 255 );

            Vertex++;
        }
    }
    pVertexBuffer->Unlock();

    // Number of indices needed in the index buffer.
    UINT NumIndices = ( Width + 1 ) * 2 * Height + 2 * ( Height - 1 );

    // Create an index buffer.
    IDirect3DIndexBuffer9* pIndexBuffer;
    g_pd3dDevice->CreateIndexBuffer( NumIndices * sizeof( WORD ),
                                     D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
                                     D3DPOOL_DEFAULT, &pIndexBuffer, NULL );
    // Fill the index buffer.
    WORD* pIndices;
    pIndexBuffer->Lock( 0, 0, ( VOID** )&pIndices, 0 );
    UINT Index = 0;
    for( INT i = 0; i < Height; i++ )
    {
        // tri-stripped vertices
        for( INT j = 0; j < Width + 1; j++ )
        {
            pIndices[Index++] = ( WORD )( i * ( Width + 1 ) + j );
            pIndices[Index++] = ( WORD )( ( i + 1 ) * ( Width + 1 ) + j );
        }

        // degenerate strip connectors
        if( i != Height - 1 )
        {
            pIndices[Index++] = ( WORD )( ( i + 1 ) * ( Width + 1 ) + Width );
            pIndices[Index++] = ( WORD )( ( i + 1 ) * ( Width + 1 ) );
        }
    }
    pIndexBuffer->Unlock();

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

    // Create constant table associations
    D3DXHANDLE hAmbient, hLights, hLight1, hLight1Pos, hLight1Color, hLight1Rad,
        hLight2, hLight2Pos, hLight2Color, hLight2Rad;

    // Get handles to root level constants
    hAmbient = pVSConstantTable->GetConstantByName( NULL, "ambientColor" );
    hLights = pVSConstantTable->GetConstantByName( NULL, "lights" );

    // GetConstantElement gets a handle out of an array
    hLight1 = pVSConstantTable->GetConstantElement( hLights, 0 );
    hLight2 = pVSConstantTable->GetConstantElement( hLights, 1 );

    // Use GetConstantByName to pull elements out of a struct - 
    // the first parameter is the handle to the struct name
    hLight1Pos = pVSConstantTable->GetConstantByName( hLight1, "Pos" );
    hLight1Color = pVSConstantTable->GetConstantByName( hLight1, "Color" );
    hLight1Rad = pVSConstantTable->GetConstantByName( hLight1, "Radius" );

    hLight2Pos = pVSConstantTable->GetConstantByName( hLight2, "Pos" );
    hLight2Color = pVSConstantTable->GetConstantByName( hLight2, "Color" );
    hLight2Rad = pVSConstantTable->GetConstantByName( hLight2, "Radius" );

    // Define game view of the constants
    FLOAT fLight1Angle = 0.0f;
    FLOAT fLight2Angle = 0.0f;

    XMFLOAT4 vLight1Pos = XMFLOAT4( 0.0f, 0.0f, 0.0f, 0.0f );
    XMFLOAT4 vLight2Pos = XMFLOAT4( 0.0f, 0.0f, 0.0f, 0.0f );
    XMFLOAT4 vAmbient = XMFLOAT4( 0.2f, 0.2f, 0.2f, 0.0f );
    XMFLOAT4 vLight1Color = XMFLOAT4( 0.9f, 0.1f, 0.5f, 0.0f );
    XMFLOAT4 vLight2Color = XMFLOAT4( 0.1f, 0.5f, 0.9f, 0.0f );

    // Main render loop
    for(; ; )
    {
        // Clear the backbuffer.
        g_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                             D3DCOLOR_ARGB( 0, 0, 0, 0 ), 1.0f, 0L );

        // Set shaders.
        g_pd3dDevice->SetVertexShader( pVertexShader );
        g_pd3dDevice->SetPixelShader( pPixelShader );

        // Update light positions
        fLight1Angle += 0.003f;
        fLight2Angle += 0.005f;

        vLight1Pos.x = +sinf( fLight1Angle ) * 2.0f;
        vLight1Pos.z = +cosf( fLight1Angle ) * 2.0f;

        vLight2Pos.y = +cosf( fLight2Angle ) * 2.0f;
        vLight2Pos.z = -sinf( fLight2Angle ) * 2.0f;

        // Set shader constants - we explicitly set the matrix to be
        // stored in c0, so we would use SetVertexShaderConstantF to set it
        g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );

        // Set the defaults ( we will use the default for ambient light )
        pVSConstantTable->SetDefaults( g_pd3dDevice );

        // Update the shader constants for the lights using the constant handles
        pVSConstantTable->SetVector( g_pd3dDevice, hLight1Color, ( D3DXVECTOR4* )&vLight1Color );
        pVSConstantTable->SetVector( g_pd3dDevice, hLight2Color, ( D3DXVECTOR4* )&vLight2Color );
        pVSConstantTable->SetVector( g_pd3dDevice, hLight1Pos, ( D3DXVECTOR4* )&vLight1Pos );
        pVSConstantTable->SetVector( g_pd3dDevice, hLight2Pos, ( D3DXVECTOR4* )&vLight2Pos );
        pVSConstantTable->SetFloat( g_pd3dDevice, hLight1Rad, 4.0f );
        pVSConstantTable->SetFloat( g_pd3dDevice, hLight2Rad, 3.0f );

        // Set the vertex declaration.
        g_pd3dDevice->SetVertexDeclaration( pVertexDecl );

        // Set the stream source.
        g_pd3dDevice->SetStreamSource( 0, pVertexBuffer, 0, sizeof( COLORVERTEX ) );

        // Set the index buffer
        g_pd3dDevice->SetIndices( pIndexBuffer );

        // Draw the triangle strip.
        g_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLESTRIP, 0, 0,
                                            NumVertices, 0, NumPrimitives );

        // Present the backbuffer contents to the display.
        g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    }
}
