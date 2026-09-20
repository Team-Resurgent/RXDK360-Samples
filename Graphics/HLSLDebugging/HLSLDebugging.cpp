//--------------------------------------------------------------------------------------
// HLSLDebugging.cpp
//
// This sample demonstrates the use of HLSL assert and dbgprint.
//
// The scene is a simple sphere with one incorrect normal (length = 2).  The sample
// shader shows multiple ways to detect this problem by using a combination of PIX
// and runtime asserts.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <XGraphics.h>

#include <AtgApp.h>
#include <AtgDebugDraw.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_1, L"Rotate Camera" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Switch Vertex Shader" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_1, L"Switch Pixel Shader" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_1, L"Display Help" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_1, L"Pause" },
};

static const DWORD  NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );


//--------------------------------------------------------------------------------------
// Enum for different debug modes
//--------------------------------------------------------------------------------------
enum HLSL_DEBUG_MODE
{
    HLSL_DEBUG_NONE,
    HLSL_DEBUG_DEBUGGING,
    HLSL_DEBUG_RUNTIMEDEBUGGING,
    HLSL_DEBUG_MODE_MAX
};

WCHAR* g_VSTypeName[] =
{
    L"Vertex Shader: No Debugging",
    L"Vertex Shader: Debugging Enabled",
    L"Vertex Shader: Debugging Enabled w/ Runtime Asserts",
};

WCHAR* g_PSTypeName[] = {
    L"Pixel Shader: No Debugging",
    L"Pixel Shader: Debugging Enabled",
    L"Pixel Shader: Debugging Enabled w/ Runtime Asserts",
};


//--------------------------------------------------------------------------------------
// A position, normal, and tex coords for each vertex
//--------------------------------------------------------------------------------------
struct CUSTOMVERTEX
{
    XMFLOAT3 Position;
    XMFLOAT3 Normal;
    XMFLOAT2 TexCoord;
};

VOID GenerateTexturedSphere( DWORD numSlices, DWORD numStacks, D3DVertexBuffer** pVB,
                             D3DIndexBuffer** pIB, DWORD* numIndices,
                             D3DVertexDeclaration** pDecl, FLOAT fUVScaler );
VOID GenerateTexturedSphereGeometry( DWORD dwNumSlices, DWORD dwNumStacks,
                                     CUSTOMVERTEX* pData, FLOAT fUVScaler );
VOID GenerateSphereIndices( DWORD dwNumSlices, DWORD dwNumStacks, WORD* pIndices );


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    Sample() : m_bDrawHelp( FALSE )
    {
    }

    virtual ~Sample()
    {
        SAFE_RELEASE( m_pPlanetVB );
        SAFE_RELEASE( m_pPlanetIB );
        SAFE_RELEASE( m_pPlanetDecl );
        SAFE_RELEASE( m_pPlanetTexture );

        for ( DWORD i = 0 ; i < HLSL_DEBUG_MODE_MAX ; ++i )
        {
            SAFE_RELEASE( m_pPlanetVS[i] );
            SAFE_RELEASE( m_pPlanetPS[i] );
        }
    }

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    VOID            CreatePlanetMesh();
    
    VOID            RenderOverlays();

private:
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    BOOL m_bPaused;

    float m_fRotation;
    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;
    XMMATRIX m_matWVP;

    // Planet rendering data
    D3DVertexBuffer* m_pPlanetVB;
    D3DIndexBuffer* m_pPlanetIB;
    D3DVertexDeclaration* m_pPlanetDecl;
    DWORD m_dwNumPlanetIndices;
    D3DTexture* m_pPlanetTexture;

    // Planet Vertex and Pixel Shaders
    IDirect3DVertexShader9* m_pPlanetVS[HLSL_DEBUG_MODE_MAX];
    IDirect3DPixelShader9* m_pPlanetPS[HLSL_DEBUG_MODE_MAX];

    HLSL_DEBUG_MODE m_VSDebugMode;
    HLSL_DEBUG_MODE m_PSDebugMode;
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
// Desc: Creates all graphics resources and initializes the instanced mesh system.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    m_bDrawHelp = FALSE;
    m_bPaused = FALSE;

    m_VSDebugMode = HLSL_DEBUG_NONE;
    m_PSDebugMode = HLSL_DEBUG_NONE;

    m_fRotation = 0.0f;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the transform matrices
    m_matWorld = XMMatrixIdentity();
    m_matView = XMMatrixTranslation( 0.0f, 0.0f, 5.0f );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, 1.0f, 20.0f );

    // Load textures
    hr = D3DXCreateTextureFromFile( m_pd3dDevice,
                                            "game:\\media\\textures\\pgas06L.dds",
                                            &m_pPlanetTexture );
    if( FAILED( hr ) )
        return ATGAPPERR_MEDIANOTFOUND;

    ATG::ConvertTextureToAs16SRGBFormat( m_pPlanetTexture );

    // Load Shaders
    if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\Shader.xvu", &m_pPlanetVS[HLSL_DEBUG_NONE] ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\Shader_Debugging.xvu", &m_pPlanetVS[HLSL_DEBUG_DEBUGGING] ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\Shader_RuntimeDebugging.xvu", &m_pPlanetVS[HLSL_DEBUG_RUNTIMEDEBUGGING] ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\Shader.xpu", &m_pPlanetPS[HLSL_DEBUG_NONE] ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\Shader_Debugging.xpu", &m_pPlanetPS[HLSL_DEBUG_DEBUGGING] ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\Shader_RuntimeDebugging.xpu", &m_pPlanetPS[HLSL_DEBUG_RUNTIMEDEBUGGING] ) ) )
        return hr;

    // Create the planet
    CreatePlanetMesh();

    m_VSDebugMode = HLSL_DEBUG_NONE;
    m_PSDebugMode = HLSL_DEBUG_NONE;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame.  Entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Compute camera rotation.
    static FLOAT s_fRotateY = 0;
    static FLOAT s_fRotateX = 0;
    s_fRotateY += pGamepad->fX1 * fElapsedTime * XM_PIDIV2;
    s_fRotateX += pGamepad->fY1 * fElapsedTime * XM_PIDIV2;
    s_fRotateX = min( max( s_fRotateX, -XM_PI / 5.0f ), XM_PI / 5.0f );
    XMVECTOR qRotateY = XMQuaternionRotationAxis( XMVectorSet( 0, 1, 0, 0 ), s_fRotateY );
    XMVECTOR qRotateX = XMQuaternionRotationAxis( XMVectorSet( 1, 0, 0, 0 ), s_fRotateX );
    XMVECTOR qRotation = XMQuaternionMultiply( qRotateX, qRotateY );

    // Compose view matrix from camera settings.
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, 3.0f, 0.0f );
    vEyePt = XMVector3Rotate( vEyePt, qRotation );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    vUpVec = XMVector3Rotate( vUpVec, qRotation );
    vEyePt += vLookatPt;
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Toggle rotation
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
        m_bPaused = !m_bPaused;

    // Rotate the scene
    m_fRotation += (m_bPaused) ? 0.0f : -fElapsedTime / 2;
    m_matWorld = XMMatrixRotationY( m_fRotation );

    // Update the world view projection matrix for the instances
    m_matWVP = m_matWorld * m_matView * m_matProj;
    
    // Toggle the render passes
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A ) {
        m_VSDebugMode = ( HLSL_DEBUG_MODE )( ( m_VSDebugMode + 1 ) % HLSL_DEBUG_MODE_MAX );
    }
        
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B ) {
        m_PSDebugMode = ( HLSL_DEBUG_MODE )( ( m_PSDebugMode + 1 ) % HLSL_DEBUG_MODE_MAX );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff000000, 0xff888844 );

    m_pd3dDevice->SetVertexShader( m_pPlanetVS[m_VSDebugMode] );
    m_pd3dDevice->SetPixelShader( m_pPlanetPS[m_PSDebugMode] );

    // If we are using a vertex shader, we simply pass variables to
    // the vertex shader, and the vertex shader will do the rest.
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m_matWVP, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&m_matWorld, 4 );

    m_pd3dDevice->SetTexture( 0, m_pPlanetTexture );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    // Set our custom shaders
    m_pd3dDevice->SetVertexDeclaration( m_pPlanetDecl );

    // Render the object
    m_pd3dDevice->SetStreamSource( 0, m_pPlanetVB, 0, sizeof( CUSTOMVERTEX ) );
    m_pd3dDevice->SetIndices( m_pPlanetIB );
    m_pd3dDevice->DrawIndexedVertices( D3DPT_QUADLIST, 0, 0, m_dwNumPlanetIndices );

    RenderOverlays();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderOverlays()
// Desc: Show title, timers, and help.
//--------------------------------------------------------------------------------------
VOID Sample::RenderOverlays()
{
    PIXBeginNamedEvent( 0xFFFFFFFF, "Overlays" );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"HLSL Debugging" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.DrawText( 0, 30.0f, 0xffffffff, g_VSTypeName[m_VSDebugMode] );
        m_Font.DrawText( 0, 55.0f, 0xffffffff, g_PSTypeName[m_PSDebugMode] );
        m_Font.SetScaleFactors( 1.1f, 1.1f );
        m_Font.DrawText( 0, 100.0f, 0xffffffff, L"Watch for the incorrectly lit vertex." );
        m_Font.End();
    }
    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: CreatePlanetMesh()
// Desc: Creates vertex and index data for the planet
//--------------------------------------------------------------------------------------
VOID Sample::CreatePlanetMesh()
{
    const DWORD dwNumStacks = 50;
    const DWORD dwNumSlices = 25;
    GenerateTexturedSphere( dwNumStacks, dwNumSlices, &m_pPlanetVB, &m_pPlanetIB,
                            &m_dwNumPlanetIndices, &m_pPlanetDecl, 1.0f );
}


//--------------------------------------------------------------------------------------
// Name: GenerateTexturedSphere()
// Desc: Creates a sphere with texture coordinates
//--------------------------------------------------------------------------------------
VOID GenerateTexturedSphere( DWORD numSlices, DWORD numStacks, D3DVertexBuffer** pVB,
                             D3DIndexBuffer** pIB, DWORD* numIndices,
                             D3DVertexDeclaration** pDecl, FLOAT fUVScaler )
{
    // Create a vertex buffer and copy the mesh vertex data into it
    ATG::g_pd3dDevice->BlockUntilIdle();
    ATG::g_pd3dDevice->CreateVertexBuffer(
        sizeof( CUSTOMVERTEX ) * ( numSlices + 1 ) * ( numStacks + 1 ),
        0, 0, D3DPOOL_DEFAULT, pVB, NULL );
    CUSTOMVERTEX* pVBData = NULL;
    ( *pVB )->Lock( 0, 0, ( VOID** )&pVBData, 0 );
    GenerateTexturedSphereGeometry( numSlices, numStacks, pVBData, fUVScaler );
    ( *pVB )->Unlock();

    *numIndices = 4 * numSlices * numStacks;

    // Create an index buffer and copy in the mesh index data.
    ATG::g_pd3dDevice->CreateIndexBuffer( *numIndices * sizeof( WORD ),
                                          0, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
                                          pIB, NULL );
    WORD* pIBData = NULL;
    ( *pIB )->Lock( 0, 0, ( VOID** )&pIBData, 0 );
    GenerateSphereIndices( numSlices, numStacks, pIBData );
    ( *pIB )->Unlock();

    // Create the vertex declaration.
    D3DVERTEXELEMENT9 TexturedSphereDecl[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_NORMAL, 0 },
        { 0, 24, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    ATG::g_pd3dDevice->CreateVertexDeclaration( TexturedSphereDecl, pDecl );
}


//--------------------------------------------------------------------------------------
// Name: GenerateTexturedSphereGeometry()
// Desc: Creates geometry for a sphere
//--------------------------------------------------------------------------------------
VOID GenerateTexturedSphereGeometry( DWORD numSlices, DWORD numStacks,
                                     CUSTOMVERTEX* pData, FLOAT fUVScaler )
{
    for( DWORD i = 0; i < numSlices + 1; i++ )
    {
        for( DWORD j = 0; j < numStacks + 1; j++ )
        {
            FLOAT fTheta = FLOAT( i ) / numSlices * 2 * XM_PI;
            FLOAT fPhi = ( FLOAT( j ) / numStacks * 2 - 1.0f ) * XM_PIDIV2;
            pData->TexCoord.x = FLOAT( i ) / numSlices * fUVScaler;
            pData->TexCoord.y = FLOAT( j ) / numStacks * fUVScaler;
            pData->Position.x = cosf( fTheta ) * cosf( fPhi );
            pData->Position.z = sinf( fTheta ) * cosf( fPhi );
            pData->Position.y = sinf( fPhi );
            pData->Normal = pData->Position;

            // Denormalize one normal for demonstration purposes
            if ( i == 25 && j == 12 ) {
                pData->Normal.x *= 2;
                pData->Normal.y *= 2;
                pData->Normal.z *= 2;
            }

            pData++;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: GenerateSphereIndices()
// Desc: Creates indices for a sphere
//--------------------------------------------------------------------------------------
VOID GenerateSphereIndices( DWORD dwNumSlices, DWORD dwNumStacks, WORD* pIndices )
{
    WORD i0 = ( WORD )0;
    WORD i1 = ( WORD )1;
    WORD i2 = ( WORD )dwNumStacks + 2;
    WORD i3 = ( WORD )dwNumStacks + 1;

    for( DWORD i = 0; i < dwNumSlices; i++ )
    {
        for( DWORD j = 0; j < dwNumStacks; j++ )
        {
            pIndices[0] = i0;
            pIndices[1] = i1;
            pIndices[2] = i2;
            pIndices[3] = i3;

            pIndices += 4;
            i0++;
            i1++;
            i2++;
            i3++;
        }
        i0++;
        i1++;
        i2++;
        i3++;
    }
}

