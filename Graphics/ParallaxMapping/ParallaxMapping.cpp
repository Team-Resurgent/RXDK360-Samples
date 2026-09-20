//--------------------------------------------------------------------------------------
// ParallaxMapping.cpp
//
// Desc: The sample demonstrates a technique of parallax mapping.
// References: http://www.infiscape.com/doc/parallax_mapping.pdf
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_1, L"Rotate object" },
    { ATG::HELP_RIGHTSTICK,   ATG::HELP_PLACEMENT_1, L"Move object" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle\nbase texture" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle\nparallax\nmapping" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle\nLight motion" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_2, L"Change parallax\noffset value" },
    { ATG::HELP_LEFT_BUTTON,  ATG::HELP_PLACEMENT_1, L"Triggers move object in Z" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Structures, Globals
//--------------------------------------------------------------------------------------

// Structure to hold vertex data.
struct VERTEX
{
    FLOAT   Position[3];
    FLOAT   Tex[2];
    FLOAT   Tangent[3];
    FLOAT   Normal[3];
    FLOAT   Binormal[3];
};

VERTEX Vertices[] =
{
    { {-1, -1, 0 }, { 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } },
    { {-1,  1, 0 }, { 0, 1 }, { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } },
    { { 1, -1, 0 }, { 1, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } },
    { { 1,  1, 0 }, { 1, 1 }, { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } }
};
VERTEX VerticesBack[] =
{
    { {-1, -1, 0 }, { 1, 1 }, {-1, 0, 0 }, { 0, 0,-1 }, { 0,-1, 0 } },
    { { 1, -1, 0 }, { 0, 1 }, {-1, 0, 0 }, { 0, 0,-1 }, { 0,-1, 0 } },
    { {-1,  1, 0 }, { 1, 0 }, {-1, 0, 0 }, { 0, 0,-1 }, { 0,-1, 0 } },
    { { 1,  1, 0 }, { 0, 0 }, {-1, 0, 0 }, { 0, 0,-1 }, { 0,-1, 0 } }
};
XMVECTOR            g_vPointLightPos; // Directional light direction
XMVECTOR            g_vEyeVector;


//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------

// Constants to define our world space
const FLOAT         XMIN = -6.f;
const FLOAT         XMAX = 6.f;
const FLOAT         ZMIN = 1.8f;
const FLOAT         ZMAX = 7.f;
const FLOAT         YMIN = -5.f;
const FLOAT         YMAX = 5.f;
const FLOAT         ZADJUSTMENT = 10.f;

const FLOAT         PARALLAX_DEFAULT = 0.04f;
const FLOAT         PARALLAX_MAX = 0.05f;
const FLOAT         PARALLAX_MIN = 0.00f;
const FLOAT         PARALLAX_STEP = 0.005f;

// Constants for scaling input
const FLOAT         MOTION_SCALE = 1.5f;
const FLOAT         MOTION_SCALE_Z = 1.0f / 65536.f;


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::PackedResource m_Resource;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    // Flags
    BOOL m_bParallax;
    BOOL m_bMoveLight;
    BOOL m_bTexture;

    DWORD           m_Pad[3]; // Padding for alignment of XMVECTORs

    // D3D objects
    LPDIRECT3DVERTEXDECLARATION9 m_pVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9 m_pVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pPixelShaderBump;
    LPDIRECT3DPIXELSHADER9 m_pPixelShaderParallax;
    LPDIRECT3DTEXTURE9 m_pTexture;
    LPDIRECT3DTEXTURE9 m_pNormalHeightTexture;

    // Object parameters
    XMVECTOR m_vPosition;      // Object position vector
    XMVECTOR m_qRotation;      // Object roataion quaternion
    FLOAT m_fParallaxFactor;

    // Transform matrices
    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    XMVECTOR        RotationArc( XMVECTOR v0, XMVECTOR v1 );

public:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
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
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    m_bDrawHelp = FALSE;

    // Create the textures resource
    if( FAILED( hr = m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    m_pTexture = m_Resource.GetTexture( "Texture0" );
    m_pNormalHeightTexture = m_Resource.GetTexture( "Texture0Height" );
    m_bTexture = TRUE;

    // Create vertex shader
    static const D3DVERTEXELEMENT9 decl[] =
    {
        // First stream is first mesh
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0 },
        { 0, 20, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TANGENT, 0 },
        { 0, 32, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL, 0 },
        { 0, 44, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_BINORMAL, 0 },
        D3DDECL_END()
    };

    if( FAILED( hr = m_pd3dDevice->CreateVertexDeclaration( decl, &m_pVertexDeclaration ) ) )
        ATG::FatalError( "Error %#X creating Vertex Declaration\n", hr );

    VOID* pCode = NULL;
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\ParallaxMapping.xvu", &pCode ) ) )
        ATG::FatalError( "Error %#X creating Vertex Shader\n", hr );

    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pVertexShader ) ) )
        ATG::FatalError( "Error %#X creating Vertex Shader\n", hr );
    ATG::UnloadFile( pCode );

    // Create pixel shader
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\ParallaxMapping.xpu", &pCode ) ) )
        ATG::FatalError( "Error %#X creating Pixel Shader\n", hr );

    if( FAILED( hr = m_pd3dDevice->CreatePixelShader( ( DWORD* )pCode, &m_pPixelShaderParallax ) ) )
        ATG::FatalError( "Error %#X creating Pixel Shader\n", hr );
    ATG::UnloadFile( pCode );

    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\BumpMapping.xpu", &pCode ) ) )
        ATG::FatalError( "Error %#X creating Pixel Shader\n", hr );

    if( FAILED( hr = m_pd3dDevice->CreatePixelShader( ( DWORD* )pCode, &m_pPixelShaderBump ) ) )
        ATG::FatalError( "Error %#X creating Pixel Shader\n", hr );
    ATG::UnloadFile( pCode );

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Positions
    m_vPosition = XMVectorSet( 0.0f, 0.0f, 3.0f, 1.0f );

    // Set up rotation parameters
    m_qRotation.x = 0.4f;
    m_qRotation.y = 0.0f;
    m_qRotation.z = 0.0f;
    m_qRotation.w = 1.0f;

    // Set the transform matrices
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 3.0f, 1.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 1.0f );
    m_matWorld = XMMatrixIdentity();
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, 1.0f, 10000.0f );

    g_vEyeVector = vEyePt;
    m_bMoveLight = TRUE;
    g_vPointLightPos = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );

    m_fParallaxFactor = PARALLAX_DEFAULT;
    m_bParallax = TRUE;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();
    FLOAT fTime = ( FLOAT )m_Timer.GetAppTime();
    if( m_bMoveLight )
        g_vPointLightPos = XMVectorSet( sinf( fTime ) * 2.f - 0.f, cosf( fTime ) * 2.f + 0.f, 0.0f, 1.0f );

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Toggle parallax
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_bParallax = !m_bParallax;
    }
    // Toggle light movement
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_bMoveLight = !m_bMoveLight;
    }

    // Toggle texture mapping
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_bTexture = !m_bTexture;
    }

    // Move an object and clamp to the appropriate range
    m_vPosition.z += ( pGamepad->bLeftTrigger - pGamepad->bRightTrigger ) * MOTION_SCALE_Z;
    if( m_vPosition.z < ZMIN )
        m_vPosition.z = ZMIN;
    else if( m_vPosition.z > ZMAX )
        m_vPosition.z = ZMAX;

    FLOAT fAdjust = m_vPosition.z / ZADJUSTMENT;

    m_vPosition.x += pGamepad->fX2 * fElapsedTime * MOTION_SCALE;
    if( m_vPosition.x < XMIN * fAdjust )
        m_vPosition.x = XMIN * fAdjust;
    else if( m_vPosition.x > XMAX * fAdjust )
        m_vPosition.x = XMAX * fAdjust;

    m_vPosition.y += pGamepad->fY2 * fElapsedTime * MOTION_SCALE;
    if( m_vPosition.y < YMIN * fAdjust )
        m_vPosition.y = YMIN * fAdjust;
    else if( m_vPosition.y > YMAX * fAdjust )
        m_vPosition.y = YMAX * fAdjust;

    // Change Parallax factor
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        m_fParallaxFactor = min( PARALLAX_MAX, m_fParallaxFactor + PARALLAX_STEP );
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        m_fParallaxFactor = max( PARALLAX_MIN, m_fParallaxFactor - PARALLAX_STEP );

    // Update the object rotation
    if( pGamepad->fX1 || pGamepad->fY1 )
    {
        XMVECTOR vOrig = XMVectorSet( 0.0f, 0.0f, -1.0f, 0.0f );
        XMVECTOR vDest = XMVectorSet( pGamepad->fX1 * fElapsedTime * 2.0f, pGamepad->fY1 * fElapsedTime * 1.0f, -1.0f,
                                      0.0f );
        XMVECTOR qR = RotationArc( vOrig, vDest );
        m_qRotation = XMQuaternionMultiply( m_qRotation, qR );
    }

    m_matWorld = XMMatrixAffineTransformation( XMVectorSet( 1.0f, 1.0f, 1.0f, 0.0f ),
                                               XMVectorZero(), m_qRotation, XMVectorZero() );
    m_matWorld._41 = m_vPosition.x;
    m_matWorld._42 = m_vPosition.y;
    m_matWorld._43 = m_vPosition.z;

    // Some basic constants
    XMVECTOR vZero = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.00f );
    XMVECTOR vOne = XMVectorSet( 1.0f, 0.5f, 0.2f, 0.05f );

    // Vertex shader operations use transposed matrices
    XMMATRIX mat, matCamera, matTranspose, matCameraTranspose;
    XMMATRIX matViewTranspose, matProjTranspose;

    matCamera = XMMatrixMultiply( m_matWorld, m_matView );
    mat = XMMatrixMultiply( matCamera, m_matProj );
    matTranspose = XMMatrixTranspose( mat );
    matCameraTranspose = XMMatrixTranspose( matCamera );
    matViewTranspose = XMMatrixTranspose( m_matView );
    matProjTranspose = XMMatrixTranspose( m_matProj );

    // Get inverse of world matrix
    XMVECTOR vDeterminant;
    XMMATRIX matInvWorld = XMMatrixInverse( &vDeterminant, m_matWorld );

    // Transform point light position into object space
    XMVECTOR vPointtLightWorldPos = XMVector3TransformCoord( g_vPointLightPos, matInvWorld );

    // Transform eye position into object space
    XMVECTOR vEyeWorldPos = XMVector3TransformCoord( g_vEyeVector, matInvWorld );

    // Set the vertex shader constants
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&vZero, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 1, ( FLOAT* )&vOne, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matTranspose, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&matCameraTranspose, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 12, ( FLOAT* )&matViewTranspose, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 16, ( FLOAT* )&matProjTranspose, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 21, ( FLOAT* )&vPointtLightWorldPos, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 22, ( FLOAT* )&vEyeWorldPos, 1 );

    if( m_bTexture )
        m_pd3dDevice->SetPixelShaderConstantF( 3, ( FLOAT* )&vOne, 1 );
    else
        m_pd3dDevice->SetPixelShaderConstantF( 3, ( FLOAT* )&vZero, 1 );
    XMVECTOR vParallaxFactor;
    vParallaxFactor.x = m_fParallaxFactor;
    m_pd3dDevice->SetPixelShaderConstantF( 4, ( FLOAT* )&vParallaxFactor, 1 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RotationArc()
// Desc: Calc rotation arc
//--------------------------------------------------------------------------------------
XMVECTOR Sample::RotationArc( XMVECTOR v0, XMVECTOR v1 )
{
    v0 = XMVector3Normalize( v0 );
    v1 = XMVector3Normalize( v1 );

    XMVECTOR cp;
    cp = XMVector3Cross( v0, v1 );

    FLOAT s = sqrtf( ( 1.0f + XMVector3Dot( v0, v1 ).x ) * 2.0f );

    XMVECTOR q;
    q.x = cp.x / s;
    q.y = cp.y / s;
    q.z = cp.z / s;
    q.w = s / 2.0f;
    return q;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    // Set the common pixel shader
    if( m_bParallax )
        m_pd3dDevice->SetPixelShader( m_pPixelShaderParallax );
    else
        m_pd3dDevice->SetPixelShader( m_pPixelShaderBump );

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

    m_pd3dDevice->SetTexture( 0, m_pTexture );
    m_pd3dDevice->SetTexture( 1, m_pNormalHeightTexture );

    // Render the Teapot
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDeclaration );
    m_pd3dDevice->SetVertexShader( m_pVertexShader );

    m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, Vertices, sizeof( VERTEX ) );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, VerticesBack, sizeof( VERTEX ) );

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
        m_Font.DrawText( 0, 0, 0xffffffff, L"ParallaxMapping" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        WCHAR strParallaxFactor[20];
        swprintf_s( strParallaxFactor, L"%1.3f", m_fParallaxFactor );
        m_Font.DrawText( 16, 35, 0xffffffff, L"Parallax mapping:" );
        m_Font.DrawText( 216, 35, 0xffffff00, m_bParallax ? L"On" : L"Off" );
        m_Font.DrawText( 16, 55, 0xffffffff, L"Move light:" );
        m_Font.DrawText( 216, 55, 0xffffff00, m_bMoveLight ? L"On" : L"Off" );
        m_Font.DrawText( 16, 75, 0xffffffff, L"Parallax factor:" );
        m_Font.DrawText( 216, 75, 0xffffff00, strParallaxFactor );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


