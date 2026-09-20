//--------------------------------------------------------------------------------------
// VolumeFog.cpp
//
// Renders a scene with a geometric fog volume
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
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
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_1, L"Move camera" },
    { ATG::HELP_BOTTOM_LEFT,    ATG::HELP_PLACEMENT_1, L"Use " GLYPH_LEFT_BUTTON GLYPH_RIGHT_BUTTON L" triggers to zoom in/out" },
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_1, L"Display help" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Globals variables and definitions
//--------------------------------------------------------------------------------------
struct FOGOVERLAYVERT
{
    XMFLOAT4 vPos;
    XMFLOAT2 vTex;
};


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    // Timer
    ATG::Timer m_Timer;

    // Font for drawing text
    ATG::Font m_Font;

    // Help
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    ATG::PackedResource m_Resource;

    // Transform matrices
    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    XMVECTOR m_vEye;
    XMVECTOR m_vLookAt;
    XMVECTOR m_vUp;

    FLOAT m_fNear;
    FLOAT m_fFar;

    // Meshes
    ATG::Mesh m_WorldMesh;            // World mesh.
    ATG::Mesh m_FogMesh;              // Fog volume mesh (2-manifold).
    ATG::Mesh m_SkyboxMesh;           // Sky box mesh.

    // Shaders
    IDirect3DVertexShader9* m_pLitDiffuse1TexVS;
    IDirect3DPixelShader9* m_pDiffuseTexModulatePS;

    IDirect3DVertexShader9* m_pDepthOutputVS;
    IDirect3DPixelShader9* m_pDepthOutputPS;

    IDirect3DPixelShader9* m_pComputeFogValuePS;

    IDirect3DVertexShader9* m_pSkyBoxVS;
    IDirect3DVertexShader9* m_pPassThruPosTexVS;

    // Render targests
    IDirect3DSurface9* m_pFogSurface;

    // Textures
    IDirect3DTexture9* m_pNullTexture;
    IDirect3DTexture9* m_pFogBuffer;

    // Overlay polygon
    IDirect3DVertexBuffer9* m_pOverlayVerts;
    IDirect3DVertexDeclaration9* m_pOverlayVertexDeclaration;

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
// Desc: Initialize app-dependent objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;
    VOID* pCode = NULL;

    m_bDrawHelp = FALSE;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG::DebugSpew( "VolumeFog: Couldn't create font\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG::DebugSpew( "VolumeFog: Couldn't create help\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    // Create the textures resource
    if( FAILED( m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG::DebugSpew( "VolumeFog: Couldn't create Resource.xpr\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    // Load meshes
    if( FAILED( m_WorldMesh.Create( "game:\\Media\\Meshes\\VolFogTerrain.xbg", &m_Resource ) ) )
    {
        ATG::DebugSpew( "VolumeFog: Couldn't create VolFogTerrain.xbg\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    if( FAILED( m_FogMesh.Create( "game:\\Media\\Meshes\\VolFogFog.xbg" ) ) )
    {
        ATG::DebugSpew( "VolumeFog: Couldn't create VolFogFog.xbg\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    if( FAILED( m_SkyboxMesh.Create( "game:\\Media\\Meshes\\SkyBox.xbg", &m_Resource ) ) )
    {
        ATG::DebugSpew( "VolumeFog: Couldn't create SkyBox.xbg\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    // Load and create shaders
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\LitDiffuse1TexVS.xvu", &pCode ) ) )
    {
        ATG::DebugSpew( "VolumeFog: Couldn't create LitDiffuse1TexVS.xvu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }
    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pLitDiffuse1TexVS ) ) )
        return hr;
    ATG::UnloadFile( pCode );

    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\DiffuseTexModulatePS.xpu", &pCode ) ) )
    {
        ATG::DebugSpew( "VolumeFog: Couldn't create DiffuseTexModulatePS.xpu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }
    if( FAILED( hr = m_pd3dDevice->CreatePixelShader( ( DWORD* )pCode, &m_pDiffuseTexModulatePS ) ) )
        return hr;
    ATG::UnloadFile( pCode );

    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\DepthOutputVS.xvu", &pCode ) ) )
    {
        ATG::DebugSpew( "VolumeFog: Couldn't create DepthOutputVS.xvu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }
    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pDepthOutputVS ) ) )
        return hr;
    ATG::UnloadFile( pCode );

    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\DepthOutputPS.xpu", &pCode ) ) )
    {
        ATG::DebugSpew( "VolumeFog: Couldn't create DepthOutputPS.xpu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }
    if( FAILED( hr = m_pd3dDevice->CreatePixelShader( ( DWORD* )pCode, &m_pDepthOutputPS ) ) )
        return hr;
    ATG::UnloadFile( pCode );

    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\ComputeFogValuePS.xpu", &pCode ) ) )
    {
        ATG::DebugSpew( "VolumeFog: Couldn't create ComputeFogValuePS.xpu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }
    if( FAILED( hr = m_pd3dDevice->CreatePixelShader( ( DWORD* )pCode, &m_pComputeFogValuePS ) ) )
        return hr;
    ATG::UnloadFile( pCode );

    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\SkyBoxVS.xvu", &pCode ) ) )
    {
        ATG::DebugSpew( "VolumeFog: Couldn't create SkyBoxVS.xvu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }
    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pSkyBoxVS ) ) )
        return hr;
    ATG::UnloadFile( pCode );

    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\PassThruPosTexVS.xvu", &pCode ) ) )
    {
        ATG::DebugSpew( "VolumeFog: Couldn't create PassThruPosTexVS.xvu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }
    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pPassThruPosTexVS ) ) )
        return hr;
    ATG::UnloadFile( pCode );

    // Create a NULL texture for mesh subsets that don't have a texture.
    if( FAILED( m_pd3dDevice->CreateTexture( 1, 1, 1,
                                             0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                             &m_pNullTexture, NULL ) ) )
        return E_FAIL;

    // Create a texture to hold accumulated fog values.
    if( FAILED( m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, 1,
                                             0, D3DFMT_G16R16, D3DPOOL_DEFAULT,
                                             &m_pFogBuffer, NULL ) ) )
        return E_FAIL;

    // Create a surface to use for accumulating the fog values.
    D3DSURFACE_PARAMETERS SurfaceParameters;
    memset( &SurfaceParameters, 0, sizeof( D3DSURFACE_PARAMETERS ) );

    SurfaceParameters.Base = 0;
    SurfaceParameters.ColorExpBias = +5;    // Exponent bias to fill up the (-32,32) range of D3DFMT_G16R16_EDRAM

    hr = m_pd3dDevice->CreateRenderTarget( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                           D3DFMT_G16R16_EDRAM, D3DMULTISAMPLE_NONE, 0, FALSE,
                                           &m_pFogSurface, &SurfaceParameters );

    if( FAILED( hr ) )
        return hr;

    // Setup a full screen quad with texture coordinates
    FOGOVERLAYVERT FogOverlayVerts[4];

    // Half pixel offsets so that screen fill convention matches texture sampling convention
    FLOAT fWidthOffset = -1.0f / ( FLOAT )m_d3dpp.BackBufferWidth;
    FLOAT fHeightOffset = -1.0f / ( FLOAT )m_d3dpp.BackBufferHeight;

    FogOverlayVerts[0].vPos = XMFLOAT4( -1.0f + fWidthOffset, 1.0f + fHeightOffset, 1.0f, 1.0f );
    FogOverlayVerts[0].vTex = XMFLOAT2( 0.0, 0.0 );

    FogOverlayVerts[1].vPos = XMFLOAT4( 1.0f + fWidthOffset, 1.0f + fHeightOffset, 1.0f, 1.0f );
    FogOverlayVerts[1].vTex = XMFLOAT2( 1.0, 0.0 );

    FogOverlayVerts[2].vPos = XMFLOAT4( 1.0f + fWidthOffset, -1.0f + fHeightOffset, 1.0f, 1.0f );
    FogOverlayVerts[2].vTex = XMFLOAT2( 1.0, 1.0 );

    FogOverlayVerts[3].vPos = XMFLOAT4( -1.0f + fWidthOffset, -1.0f + fHeightOffset, 1.0f, 1.0f );
    FogOverlayVerts[3].vTex = XMFLOAT2( 0.0, 1.0 );

    WORD FogOverlayIndices[6] = { 0, 1, 2, 0, 2, 3 };

    // Create a vertex buffer for the overlay polygon
    if( FAILED( hr = m_pd3dDevice->CreateVertexBuffer( sizeof( FOGOVERLAYVERT ) * 6, 0, 0, D3DPOOL_MANAGED,
                                                       &m_pOverlayVerts, NULL ) ) )
        return hr;

    FOGOVERLAYVERT* pTempVerts;
    m_pOverlayVerts->Lock( 0, 0, ( VOID** )&pTempVerts, 0 );

    for( DWORD i = 0; i < 6; i++ )
        pTempVerts[i] = FogOverlayVerts[FogOverlayIndices[i]];

    m_pOverlayVerts->Unlock();

    // Create vertex declaraion for the overlay.
    static const D3DVERTEXELEMENT9 declOverlay[] =
    {
        // First stream is first mesh
        { 0,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    if( FAILED( hr = m_pd3dDevice->CreateVertexDeclaration( declOverlay, &m_pOverlayVertexDeclaration ) ) )
        return hr;

    // Set the matrices
    m_matWorld = XMMatrixIdentity();

    m_vEye = XMVectorSet( 10.0f, 5.0f, 0.0f, 0.0f );
    m_vLookAt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    m_vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matView = XMMatrixLookAtLH( m_vEye, m_vLookAt, m_vUp );

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    m_fNear = 0.1f;
    m_fFar = 100.0f;

    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, m_fNear, m_fFar );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Rotate eye around up axis.
    XMMATRIX matRotate = XMMatrixRotationAxis( m_vUp, pGamepad->fX1 * fElapsedTime );
    m_vEye = XMVector3TransformCoord( m_vEye, matRotate );

    // Rotate eye points around side axis
    XMVECTOR vView = ( m_vLookAt - m_vEye );
    FLOAT dist = XMVector3Length( vView ).x;

    vView = XMVector3Normalize( vView );

    // Place limits so we don't go over the top or under the bottom
    FLOAT dot = XMVector3Dot( vView, m_vUp ).x;
    if( ( dot < 0.0f || pGamepad->fY1 < 0.0f ) && ( dot > -0.99f || pGamepad->fY1 > 0.0f ) )
    {
        XMVECTOR vAxis = XMVector3Cross( vView, m_vUp );
        matRotate = XMMatrixRotationAxis( vAxis, pGamepad->fY1 * fElapsedTime );
        m_vEye = XMVector3TransformCoord( m_vEye, matRotate );
    }

    // Move in/out
    FLOAT fIn = ( pGamepad->bRightTrigger / 255.0f );
    FLOAT fOut = ( pGamepad->bLeftTrigger / 255.0f );

    if( fIn > 0.1f && dist > 1.0f )
        m_vEye += vView * 4.0f * fIn * fElapsedTime;

    if( fOut > 0.1f )
        m_vEye -= vView * 4.0f * fOut * fElapsedTime;

    m_matView = XMMatrixLookAtLH( m_vEye, m_vLookAt, m_vUp );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d rendering.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0L );

    // Initialize state
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

    // Default sampler state on sampler zero
    m_pd3dDevice->SetTexture( 0, m_pNullTexture );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    // Setup common state for all passes
    m_pd3dDevice->SetVertexShader( m_pDepthOutputVS );
    m_pd3dDevice->SetPixelShader( m_pDepthOutputPS );

    // Calculate and set composite matrix
    XMVECTOR vDeterminant;
    XMMATRIX matWorldView, matWorldViewInverse, matWorldViewProj;
    matWorldView = XMMatrixMultiply( m_matWorld, m_matView );
    matWorldViewInverse = XMMatrixInverse( &vDeterminant, matWorldView );
    matWorldViewProj = XMMatrixMultiply( matWorldView, m_matProj );
    matWorldViewProj = XMMatrixTranspose( matWorldViewProj );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWorldViewProj, 4 );

    // Set local viewer position
    XMVECTOR vLocalEyePos = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    vLocalEyePos = XMVector3TransformCoord( vLocalEyePos, matWorldViewInverse );
    m_pd3dDevice->SetVertexShaderConstantF( 5, ( FLOAT* )&vLocalEyePos, 1 );

    // Scale distance values between 0 and 1
    FLOAT fFogScale[4];
    fFogScale[0] = 1.0f / ( m_fFar - m_fNear );
    fFogScale[1] = -m_fNear / ( m_fFar - m_fNear );
    m_pd3dDevice->SetVertexShaderConstantF( 6, fFogScale, 1 );

    //
    // Z Prepass on the world
    //
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0 );

    // Draw the world
    m_WorldMesh.Render( ATG::MESH_NOMATERIALS | ATG::MESH_NOTEXTURES );

    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );

    //
    // Pass 0: Setup the stencil values correctly for when the viewpoint is in
    //         the fog volume.
    //
    // If the fog volume is near clipped, then we should start the stencil at one
    // instead of zero.
    //
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0 );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_ALWAYS );

    m_pd3dDevice->SetTexture( 0, m_pNullTexture );

    // Increment back faces
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_INCRSAT );
    m_FogMesh.Render( ATG::MESH_NOMATERIALS | ATG::MESH_NOTEXTURES );

    // Decrement front faces
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_DECRSAT );
    m_FogMesh.Render( ATG::MESH_NOMATERIALS | ATG::MESH_NOTEXTURES );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );

    //
    // Common setup for the following passes
    //
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    // Additive blending.
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

    // Change the render target to our fog depth accumulation surface.
    IDirect3DSurface9* pOldTarget;
    m_pd3dDevice->GetRenderTarget( 0, &pOldTarget );

    m_pd3dDevice->SetRenderTarget( 0, m_pFogSurface );

    //
    // Pass 1: Draw the visible front faces of the fog volume(s) with additive
    //         blend into the red channel.  Increment stencil values.
    //
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_INCRSAT );

    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED );

    m_FogMesh.Render( ATG::MESH_NOMATERIALS | ATG::MESH_NOTEXTURES );

    //
    // Pass 2: Draw the visible back faces of the fog volume(s) with additive
    //         blend into the green channel.  Decrement stencil values.
    //
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_DECRSAT );

    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_GREEN );

    m_FogMesh.Render( ATG::MESH_NOMATERIALS | ATG::MESH_NOTEXTURES );

    //
    // Pass 3: Draw any objects intersecting the fog volume.  Add to the far
    //         values where stencil != 0 (where the object is inside the fog).
    //
    // Note: Z invariance with the initial world pass needs to be maintained.
    //
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_KEEP );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_NOTEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILREF, 0 );

    m_WorldMesh.Render( ATG::MESH_NOMATERIALS | ATG::MESH_NOTEXTURES );

    // Resolve to our texture.  Apply a 2^-5 bias when resosolving in order to map the 0..32
    // range of the render target format into the 0..1 range of the texture.
    m_pd3dDevice->Resolve( D3DRESOLVE_CLEARRENDERTARGET | ( DWORD )D3DRESOLVE_EXPONENTBIAS( -5 ),
                           NULL, m_pFogBuffer, NULL, 0, 0, NULL, 0, 0, NULL );

    // Restore the original render target.
    m_pd3dDevice->SetRenderTarget( 0, pOldTarget );
    pOldTarget->Release();

    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );

    //
    // Draw the world into the color buffer
    //
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

    // Set vertex shader to light the world
    m_pd3dDevice->SetVertexShader( m_pLitDiffuse1TexVS );

    // Set world pixel shader
    m_pd3dDevice->SetPixelShader( m_pDiffuseTexModulatePS );

    // Set local light position
    XMVECTOR vLocalLightPos = XMVectorSet( 1.0f, 1.0f, -2.0f, 0.0f );
    vLocalLightPos = XMVector3Normalize( vLocalLightPos );
    vLocalLightPos = XMVector3TransformNormal( vLocalLightPos, matWorldViewInverse );
    m_pd3dDevice->SetVertexShaderConstantF( 5, ( FLOAT* )&vLocalLightPos, 1 );

    static FLOAT vMaterialColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    m_pd3dDevice->SetVertexShaderConstantF( 6, vMaterialColor, 1 );

    static FLOAT vAmbientColor[4] = { 0.2f, 0.2f, 0.2f, 0.2f };
    m_pd3dDevice->SetVertexShaderConstantF( 7, vAmbientColor, 1 );

    // Draw the world
    m_WorldMesh.Render( ATG::MESH_NOMATERIALS );

    // Change the texture mode back to clamp.
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );

    // Render the skybox
    {
        // Use the skybox vertex shader
        m_pd3dDevice->SetVertexShader( m_pSkyBoxVS );

        // Scale the skybox to outside the world but inside the farclip
        XMMATRIX matWorld = XMMatrixScaling( 20.0f, 20.0f, 20.0f );

        // Center view matrix for skybox
        XMMATRIX matView = m_matView;
        matView._41 = 0.0f; matView._42 = 0.0f; matView._43 = 0.0f;

        XMMATRIX matWVP;
        matWVP = XMMatrixMultiply( matWorld, matView );
        matWVP = XMMatrixMultiply( matWVP, m_matProj );
        matWVP = XMMatrixTranspose( matWVP );
        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );

        // Render the skybox
        m_SkyboxMesh.Render( ATG::MESH_NOMATERIALS );
    }

    //
    // Final pass: Compute the final fog value for each pixel and apply it
    //
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    // Set vertex shader and pixel shaders
    m_pd3dDevice->SetVertexShader( m_pPassThruPosTexVS );
    m_pd3dDevice->SetPixelShader( m_pComputeFogValuePS );

    m_pd3dDevice->SetTexture( 0, m_pFogBuffer );

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );

    // Set fog color and density per normalized distance
    static FLOAT FogColorAndDensity[4] = { 0.9f, 0.9f, 0.9f, 8.0f };
    m_pd3dDevice->SetPixelShaderConstantF( 0, FogColorAndDensity, 1 );

    // Draw the fog overlay polygon
    m_pd3dDevice->SetVertexDeclaration( m_pOverlayVertexDeclaration );
    m_pd3dDevice->SetStreamSource( 0, m_pOverlayVerts, 0, sizeof( FOGOVERLAYVERT ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, 2 );

    //
    // Restore state
    //
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );

    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTexture( 1, NULL );

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetVertexShader( NULL );
    m_pd3dDevice->SetPixelShader( NULL );

    // Output statistics
    m_Timer.MarkFrame();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"VolumeFog" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Show the frame on the primary surface
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
