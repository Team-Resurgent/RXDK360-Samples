//--------------------------------------------------------------------------------------
// XPSCube.cpp
//
// This sample demonstrates the Xbox Procedural Synthesis (XPS) API for drawing
// procedural and instanced geometry.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_1, L"Move camera" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Vertex shader
//--------------------------------------------------------------------------------------
const CHAR*     g_strVertexShaderProgram =
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


//--------------------------------------------------------------------------------------
// Pixel shader
//--------------------------------------------------------------------------------------
const CHAR*     g_strPixelShaderProgram =
    " struct PS_IN                                 \n"
    " {                                            \n"
    "     float4 Color : COLOR;                    \n"  // Interpolated color from                      
    " };                                           \n"  // the vertex shader
    "                                              \n"
    " float4 main( PS_IN In ) : COLOR              \n"
    " {                                            \n"
    "     return In.Color;                         \n"  // Output color
    " }                                            \n";


// Index buffer data for a cube (using a quad list).
static const DWORD g_pIndexData[] =
{
    3, 2, 0, 1,
    1, 0, 4, 5,
    3, 1, 5, 7,
    2, 3, 7, 6,
    0, 2, 6, 4,
    5, 4, 6, 7
};
static DWORD*   g_pIndexDataPhysical = NULL;

// Vertex struct used for cube mesh data.
struct CubeVertex
{
    XMFLOAT3 Position;
    D3DCOLOR Color;
};

// A seed is the instance-specific data that our XPS routine will use to generate a
// unique instance of a cube.  Ideally, you want seed data to be as compact as possible.
struct CubeSeed
{
    XMFLOAT3 Position;
    FLOAT Scale;
};

// Number of instances to render
const DWORD     g_dwSeedCount = 30;
// Array of seeds
CubeSeed g_pSeedData[ g_dwSeedCount ];

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Derived class used to run the application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bShowHelp;

    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;
    XMMATRIX m_matWorldViewProj;

    IDirect3DVertexShader9* m_pVertexShader;
    IDirect3DPixelShader9* m_pPixelShader;
    IDirect3DVertexDeclaration9* m_pVertexDecl;

private:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the app
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Buffers to hold compiled shaders and possible error messages
    ID3DXBuffer* pShaderCode = NULL;
    ID3DXBuffer* pErrorMsg = NULL;

    // Compile vertex shader.
    HRESULT hr = D3DXCompileShader( g_strVertexShaderProgram,
                                    ( UINT )strlen( g_strVertexShaderProgram ),
                                    NULL, NULL, "main", "vs_2_0", 0,
                                    &pShaderCode, &pErrorMsg, NULL );
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Shader compile error: %s", (char*)pErrorMsg->GetBufferPointer() );
    }

    // Create pixel shader.
    m_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                      &m_pVertexShader );

    // Shader code is no longer required.
    pShaderCode->Release();
    pShaderCode = NULL;

    // Compile pixel shader.
    hr = D3DXCompileShader( g_strPixelShaderProgram,
                            ( UINT )strlen( g_strPixelShaderProgram ),
                            NULL, NULL, "main", "ps_2_0", 0,
                            &pShaderCode, &pErrorMsg, NULL );
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Shader compile error: %s", (char*)pErrorMsg->GetBufferPointer() );
    }

    // Create pixel shader.
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                     &m_pPixelShader );

    // Shader code no longer required.
    pShaderCode->Release();
    pShaderCode = NULL;

    // Define the vertex elements.
    static const D3DVERTEXELEMENT9 VertexElements[3] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
        D3DDECL_END()
    };

    // Create a vertex declaration from the element descriptions.
    m_pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pVertexDecl );

    // Create the font
    if( FAILED( hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't load font\n" );
        return hr;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create help screen
    if( FAILED( hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't load help\n" );
        return hr;
    }
    m_bShowHelp = FALSE;

    // Copy the index data to physical memory
    g_pIndexDataPhysical = ( DWORD* )XPhysicalAlloc( sizeof( g_pIndexData ), MAXULONG_PTR, 0,
                                                     PAGE_READWRITE | PAGE_WRITECOMBINE );
    memcpy( g_pIndexDataPhysical, g_pIndexData, sizeof( g_pIndexData ) );

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Initialize the transforms
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 6.0f, -6.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matWorld = XMMatrixIdentity();
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, 1.0f, 200.0f );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame to update the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Check back button
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bShowHelp = !m_bShowHelp;
    }

    // Perform object rotation
    static XMMATRIX s_matRotateX( 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 );
    static XMMATRIX s_matRotateY( 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 );
    XMMATRIX matRotateX, matRotateY;
    FLOAT fXRotate1 = pGamepad->fX1 * fElapsedTime * XM_PIDIV2;
    FLOAT fYRotate1 = pGamepad->fY1 * fElapsedTime * XM_PIDIV2;
    matRotateX = XMMatrixRotationX( +fYRotate1 );
    matRotateY = XMMatrixRotationY( -fXRotate1 );
    s_matRotateX = XMMatrixMultiply( s_matRotateX, matRotateX );
    s_matRotateY = XMMatrixMultiply( s_matRotateY, matRotateY );
    m_matWorld = XMMatrixMultiply( s_matRotateX, s_matRotateY );

    // Set matrices
    m_matWorldViewProj = XMMatrixMultiply( m_matWorld, m_matView );
    m_matWorldViewProj = XMMatrixMultiply( m_matWorldViewProj, m_matProj );

    FLOAT fAppTime = ( FLOAT )m_Timer.GetAppTime();

    // Generate new seed data
    for( DWORD i = 0; i < g_dwSeedCount; i++ )
    {
        CubeSeed* pSeed = &g_pSeedData[i];
        FLOAT fTime = fAppTime + 0.25f * ( FLOAT )i;
        pSeed->Position.x = 3.0f * sinf( fTime * 1.1f );
        pSeed->Position.z = 3.0f * cosf( fTime * 1.5f );
        pSeed->Position.y = 0.25f * sinf( fTime * 1.4f );
        pSeed->Scale = 0.1f * sinf( 2.0f * fTime ) + 0.2f;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GenerateCubeVertexData()
// Desc: Composes a vertex buffer for an instance of a cube, by combining constant data 
//       and instance-specific data passed in the pSeed parameter.
//--------------------------------------------------------------------------------------
inline VOID GenerateCubeVertexData( CubeVertex* pDestVertexData, CubeSeed* pSeed )
{
    // Fill in the vertex position values with cube corners.
    pDestVertexData[0].Position = XMFLOAT3( -1, -1, -1 );
    pDestVertexData[1].Position = XMFLOAT3( 1, -1, -1 );
    pDestVertexData[2].Position = XMFLOAT3( -1, -1, 1 );
    pDestVertexData[3].Position = XMFLOAT3( 1, -1, 1 );
    pDestVertexData[4].Position = XMFLOAT3( -1, 1, -1 );
    pDestVertexData[5].Position = XMFLOAT3( 1, 1, -1 );
    pDestVertexData[6].Position = XMFLOAT3( -1, 1, 1 );
    pDestVertexData[7].Position = XMFLOAT3( 1, 1, 1 );

    // Modify cube positions based on the seed data.
    XMVECTOR vSeedPos = XMLoadFloat3( &pSeed->Position );
    for( DWORD i = 0; i < 8; i++ )
    {
        XMVECTOR vPos = XMLoadFloat3( &pDestVertexData[i].Position );
        vPos = XMVectorScale( vPos, pSeed->Scale );
        vPos = XMVectorAdd( vPos, vSeedPos );
        XMStoreFloat3( &pDestVertexData[i].Position, vPos );
    }

    // Compute a color based on the seed's scale value.
    D3DCOLOR color = 0xFF000000 + ( ( DWORD )( 511.0f * pSeed->Scale ) << 16 )
        + ( DWORD )( 255.0f * ( 1.0f - pSeed->Scale ) );

    // Assign the color to the cube vertices.
    pDestVertexData[0].Color = color;
    pDestVertexData[1].Color = color;
    pDestVertexData[2].Color = color;
    pDestVertexData[3].Color = color;
    pDestVertexData[4].Color = color;
    pDestVertexData[5].Color = color;
    pDestVertexData[6].Color = color;
    pDestVertexData[7].Color = color;
}


//--------------------------------------------------------------------------------------
// Name: RenderXPSCube()
// Desc: A callback function launched from an XPS rendering thread to render a series of
//       XPS instances (in this case, cubes).
//       This function will be called once from each XPS rendering thread.
//       Note how an D3DXps object is created to handle rendering and instance management
//       inside this callback function.  The D3DXps object is defined in d3d9xps.h.
//       Also note the usage of the D3DXps.Allocate() method to lock down a segment of L2
//       cache for use as a temporary vertex buffer.
//
//       Note: It is not possible to use the D3D device from an XPS callback.  However,
//       all other system libraries (XTL, CRT, etc) are allowed.  Make sure you use a
//       multithreaded version of the CRT if you use CRT from multiple threads (which
//       includes XPS callbacks).  Additionally, any data in your title that is touched
//       by an XPS callback should be controlled by some thread synchronization method
//       that makes data access thread-safe.  In this case, the index buffer used to
//       render the cube is read-only, but if it were read-write, the sample would have
//       to use a critical section around the memory access, or use two buffers updated
//       on alternate frames.
//--------------------------------------------------------------------------------------
VOID RenderXPSCube( D3DXpsThread* pThreadContext,
                    VOID* pCallbackContext,
                    const VOID* pSubmitData,
                    DWORD InstanceIndex )
{
    D3DXps xps( pThreadContext );

    const DWORD dwVertexCount = 8;

    do
    {
        // Get a pointer to the seed for this XPS instance.
        CubeSeed* pSeedData = ( CubeSeed* )pSubmitData + InstanceIndex;

        // Allocate some room in the L2 cache for the vertex data.
        // Note that XPS index buffers must be 32 bit.
        DWORD dwVertexDataSize = dwVertexCount * sizeof( CubeVertex );

        BYTE* pXPSMemory = ( BYTE* )xps.Allocate( dwVertexDataSize,
                                                  D3DXPS_COMMAND_SIZE );

        CubeVertex* pVertexData = ( CubeVertex* )pXPSMemory;

        // Use the seed data to generate the vertices.
        GenerateCubeVertexData( pVertexData, pSeedData );

        // Render the cube using the xps object.
        // The indices come from physical memory, not the XPS memory in the L2 cache.
        // Note how we use the interfaces on the D3DXps object to render, instead of
        // using the D3D device.  It is not possible to use the D3D device directly
        // from an XPS callback.
        xps.DrawIndexedVertices( D3DPT_QUADLIST, 24, g_pIndexDataPhysical,
                                 D3DFMT_INDEX32, pVertexData );

        // Kick-off the result for immediate rendering by the GPU, and
        // at the same time request the next instance index to render.
    } while( xps.KickOffAndGet( &InstanceIndex ) );
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame to render the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Clear the scene
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff000000, 1.0f, 0 );

    // Begin XPS rendering for the cube
    m_pd3dDevice->XpsBegin( 0 );

    // Note: It is not wise to submit D3D drawing commands within an XpsBegin/XpsEnd
    // bracket.  These can cause stalls during XPS rendering.  However, using D3D to
    // set render state is allowed and recommended inside the XPS bracket.

    // Set the XPS callback
    m_pd3dDevice->XpsSetCallback( RenderXPSCube, NULL, 0 );

    // Setup the renderstate for the XPS rendering
    m_pd3dDevice->SetVertexShader( m_pVertexShader );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    m_pd3dDevice->SetPixelShader( m_pPixelShader );
    m_pd3dDevice->SetStreamSource( 0, NULL, 0, sizeof( CubeVertex ) );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( const FLOAT* )&m_matWorldViewProj, 4 );

    // Submit an XPS rendering request that will render one cube for each seed
    m_pd3dDevice->XpsSubmit( g_dwSeedCount, g_pSeedData, g_dwSeedCount * sizeof( CubeSeed ) );

    // End XPS rendering
    m_pd3dDevice->XpsEnd();

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bShowHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"XPSCube" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
