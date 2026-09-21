//--------------------------------------------------------------------------------------
// CustomEDRAMAllocation.cpp
//
// The CustomEDRAMAllocation sample shows how to allocate your own EDRAM surfaces and
// front buffers. Using custom allocation, surfaces can overlap in EDRAM. The free
// hardware clear that can be done during a resolve can be used to clear overlapped
// surfaces when one is resolved. This can be used, for example, to clear the next 
// shadow buffer while the current one is being resolved if both shadow buffers overlap
// in EDRAM.
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
#include <AtgResource.h>
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Vertex shaders
//--------------------------------------------------------------------------------------
const CHAR*         g_strColorVS =
    "                                              "
    " struct VS_IN                                 "
    "                                              "
    " {                                            "
    "     float4 Pos   : POSITION;                 "
    " };                                           "
    "                                              "
    " struct VS_OUT                                "
    " {                                            "
    "     float4 Pos  : POSITION;                  "
    " };                                           "
    "                                              "
    " VS_OUT main( VS_IN In )                      "
    " {                                            "
    "     VS_OUT Out;                              "
    "     Out.Pos = In.Pos;                        "
    "     return Out;                              "
    " }                                            ";

const CHAR*         g_strTextureVS =
    "                                              "
    " struct VS_IN                                 "
    "                                              "
    " {                                            "
    "     float4 Pos   : POSITION;                 "
    "     float2 UV    : TEXCOORD0;                "
    " };                                           "
    "                                              "
    " struct VS_OUT                                "
    " {                                            "
    "     float4 Pos  : POSITION;                  "
    "     float2 UV   : TEXCOORD0;                 "
    " };                                           "
    "                                              "
    " VS_OUT main( VS_IN In )                      "
    " {                                            "
    "     VS_OUT Out;                              "
    "     Out.Pos = In.Pos;                        "
    "     Out.UV = In.UV;                          "
    "     return Out;                              "
    " }                                            ";


//--------------------------------------------------------------------------------------
// Pixel shaders
//--------------------------------------------------------------------------------------
const CHAR*         g_strColorPS =
    " float4 Color : register( c0 );               "
    " struct PS_IN                                 "
    " {                                            "
    " };                                           "
    "                                              "
    " float4 main( PS_IN In ) : COLOR              "
    " {                                            "
    "     return Color;                            "
    " }                                            ";

const CHAR*         g_strTexturePS =
    " sampler2D s : register( s0 );                "
    " bool channelselect : register( c0 );         "
    " float4 channel : register( c1 );             "
    " struct PS_IN                                 "
    " {                                            "
    "     float2 UV : TEXCOORD0;                   "
    " };                                           "
    "                                              "
    " float4 main( PS_IN In ) : COLOR              "
    " {                                            "
    "     if (channelselect)                       "
    "        return dot(tex2D(s,In.UV ),channel);  "
    "     else                                     "
    "       return tex2D( s, In.UV );              "
    "                                              "
    " }                                            ";


//--------------------------------------------------------------------------------------
// Vertex elements
//--------------------------------------------------------------------------------------
struct COLORVERTEX
{
    XMFLOAT4 vPos;
};

const D3DVERTEXELEMENT9 g_ColorVertexElements[] =
{
    { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    D3DDECL_END()
};

struct TEXTUREVERTEX
{
    XMFLOAT4 vPos;
    XMFLOAT2 vTexCoord;
};

const D3DVERTEXELEMENT9 g_TextureVertexElements[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    FLOAT m_fDrawWidthScale;

    // User allocated front buffer
    D3DTexture* m_pFrontBuffer;

    // User allocated back and depth-stencil buffers
    D3DSurface* m_pBackBuffer;
    D3DSurface* m_pDepthStencilBuffer;

    // User allocated EDRAM render targets and assoiated system memory textures
    D3DSurface* m_pRT[4];
    D3DTexture* m_pTexture[4];

    // Vertex declarations and shaders
    D3DVertexDeclaration* m_pColorVD;
    D3DVertexDeclaration* m_pTextureVD;
    D3DVertexShader* m_pColorVS;
    D3DPixelShader* m_pColorPS;
    D3DVertexShader* m_pTextureVS;
    D3DPixelShader* m_pTexturePS;

private:
    // Simple draw routines
    VOID            DrawColoredTriangle( const XMFLOAT2 vCenter,
                                         const FLOAT fWidth, const FLOAT fHeight,
                                         const FLOAT Color[4] );

    VOID            DrawTexturedQuad( const XMFLOAT2 vCenter,
                                      const FLOAT fWidth, const FLOAT fHeight,
                                      IDirect3DTexture9* pTexture,
                                      DWORD Channel );

    // Simple helper for compiling a shader
    enum SHADERTYPE
    {
        PIXELSHADER,
        VERTEXSHADER
    };
    VOID            CompileShader( SHADERTYPE Type,
                                   const CHAR* strEntry, const CHAR* strTarget,
                                   const CHAR* strProgram, VOID** ppShader );

    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;

    // Set backbuffer dimensions
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Set back, depth-stencil, and front buffer sizes
    atgApp.m_d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
    atgApp.m_d3dpp.FrontBufferFormat = D3DFMT_LE_X8R8G8B8;
    atgApp.m_d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;

    // Disable automatic creation of buffers
    atgApp.m_d3dpp.DisableAutoBackBuffer = TRUE;
    atgApp.m_d3dpp.DisableAutoFrontBuffer = TRUE;
    atgApp.m_d3dpp.EnableAutoDepthStencil = FALSE;

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;
    m_fDrawWidthScale = ( ( 4.0f / 3.0f ) / fAspectRatio );

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    m_bDrawHelp = FALSE;

    // Create the front buffer
    m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                 1, D3DUSAGE_RENDERTARGET, m_d3dpp.FrontBufferFormat,
                                 D3DPOOL_DEFAULT, &m_pFrontBuffer, NULL );


    // Create the back buffer at EDRAM tile address 0
    D3DSURFACE_PARAMETERS SurfaceParameters;
    memset( &SurfaceParameters, 0, sizeof( D3DSURFACE_PARAMETERS ) );
    SurfaceParameters.Base = 0;
    m_pd3dDevice->CreateRenderTarget( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                      m_d3dpp.BackBufferFormat, D3DMULTISAMPLE_NONE, 0, FALSE,
                                      &m_pBackBuffer, &SurfaceParameters );

    // Create the stencil buffer at an address that is beyond our largest render target
    // We store all of the render targets at address 0 since we don't need any of them simultaneously.
    // Put the hierarchical Z buffer at the start of hierarchical Z memory.
    SurfaceParameters.Base = XGSurfaceSize( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                            m_d3dpp.BackBufferFormat, D3DMULTISAMPLE_NONE );
    SurfaceParameters.HierarchicalZBase = 0;
    m_pd3dDevice->CreateDepthStencilSurface( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                             m_d3dpp.AutoDepthStencilFormat, D3DMULTISAMPLE_NONE,
                                             0, FALSE, &m_pDepthStencilBuffer, &SurfaceParameters );


    // Create 4 256 x 256 render targets (all at EDRAM tile address 0)
    // and create a system memory texture for each to resolve to
    SurfaceParameters.Base = 0;

    m_pd3dDevice->CreateRenderTarget( 256, 256, D3DFMT_A2R10G10B10, D3DMULTISAMPLE_NONE, 0, FALSE,
                                      &m_pRT[0], &SurfaceParameters );
    m_pd3dDevice->CreateTexture( 256, 256, 1, 0, D3DFMT_A2R10G10B10, D3DPOOL_DEFAULT,
                                 &m_pTexture[0], NULL );

    m_pd3dDevice->CreateRenderTarget( 256, 256, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                      &m_pRT[1], &SurfaceParameters );
    m_pd3dDevice->CreateTexture( 256, 256, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                 &m_pTexture[1], NULL );

    m_pd3dDevice->CreateRenderTarget( 256, 256, D3DFMT_A2R10G10B10, D3DMULTISAMPLE_NONE, 0, FALSE,
                                      &m_pRT[2], &SurfaceParameters );
    m_pd3dDevice->CreateTexture( 256, 256, 1, 0, D3DFMT_A2R10G10B10, D3DPOOL_DEFAULT,
                                 &m_pTexture[2], NULL );

    m_pd3dDevice->CreateRenderTarget( 256, 256, D3DFMT_R32F, D3DMULTISAMPLE_NONE, 0, FALSE,
                                      &m_pRT[3], &SurfaceParameters );
    m_pd3dDevice->CreateTexture( 256, 256, 1, 0, D3DFMT_R32F, D3DPOOL_DEFAULT,
                                 &m_pTexture[3], NULL );

    // Compile shaders
    CompileShader( VERTEXSHADER, "main", "vs_2_0", g_strColorVS, ( VOID** )&m_pColorVS );
    CompileShader( PIXELSHADER, "main", "ps_2_0", g_strColorPS, ( VOID** )&m_pColorPS );
    CompileShader( VERTEXSHADER, "main", "vs_2_0", g_strTextureVS, ( VOID** )&m_pTextureVS );
    CompileShader( PIXELSHADER, "main", "ps_2_0", g_strTexturePS, ( VOID** )&m_pTexturePS );

    // Create vertex declarations from the element descriptions.
    m_pd3dDevice->CreateVertexDeclaration( g_ColorVertexElements, &m_pColorVD );
    m_pd3dDevice->CreateVertexDeclaration( g_TextureVertexElements, &m_pTextureVD );

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

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw colors
    const FLOAT Red[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    const FLOAT Green[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
    const FLOAT Blue[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
    const FLOAT White[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
    const FLOAT Grey[4] = { 0.5f, 0.5f, 0.5f, 0.0f };

    D3DRESOLVE_PARAMETERS ResolveParams;
    ResolveParams.ColorExpBias = 0;

    // Set the back and stencil buffers and clear both
    m_pd3dDevice->SetRenderTarget( 0, m_pBackBuffer );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencilBuffer );
    m_pd3dDevice->ClearF( D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                          NULL, NULL, 1.0f, 0x0 );

    // Render to surface and resolve while clearing the used EDRAM to a color in the format that is used next
    m_pd3dDevice->SetRenderTarget( 0, m_pRT[0] );
    DrawColoredTriangle( XMFLOAT2( 0.0f, 0.0f ), 1.0f, 1.0f, Red );
    ResolveParams.ColorFormat = D3DFMT_A8R8G8B8;
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARRENDERTARGET,
                           NULL, m_pTexture[0], NULL, 0, 0, ( D3DVECTOR4* )Blue, 0.0f, 0x0, &ResolveParams );

    // Render to surface and resolve while clearing the used EDRAM to a color in the format that is used next
    m_pd3dDevice->SetRenderTarget( 0, m_pRT[1] );
    DrawColoredTriangle( XMFLOAT2( 0.0f, 0.0f ), 1.0f, 1.0f, Green );
    ResolveParams.ColorFormat = D3DFMT_A2R10G10B10;
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARRENDERTARGET,
                           NULL, m_pTexture[1], NULL, 0, 0, ( D3DVECTOR4* )Red, 0.0f, 0x0, &ResolveParams );

    // Render to surface and resolve while clearing the used EDRAM to a color in the format that is used next
    m_pd3dDevice->SetRenderTarget( 0, m_pRT[2] );
    DrawColoredTriangle( XMFLOAT2( 0.0f, 0.0f ), 1.0f, 1.0f, Blue );
    ResolveParams.ColorFormat = D3DFMT_R32F;
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARRENDERTARGET,
                           NULL, m_pTexture[2], NULL, 0, 0, ( D3DVECTOR4* )Grey, 0.0f, 0x0, &ResolveParams );

    // Render to surface and resolve while clearing the used EDRAM back to 0
    m_pd3dDevice->SetRenderTarget( 0, m_pRT[3] );
    DrawColoredTriangle( XMFLOAT2( 0.0f, 0.0f ), 1.0f, 1.0f, White );
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARRENDERTARGET,
                           NULL, m_pTexture[3], NULL, 0, 0, NULL, 0.0f, 0x0, NULL );

    // Use the target surfaces to render quads to the back buffer
    m_pd3dDevice->SetRenderTarget( 0, m_pBackBuffer );
    DrawTexturedQuad( XMFLOAT2( -0.5f * m_fDrawWidthScale, 0.5f ), 0.5f * m_fDrawWidthScale, 0.5f, m_pTexture[0],
                      D3DCOLORWRITEENABLE_ALL );
    DrawTexturedQuad( XMFLOAT2( 0.5f * m_fDrawWidthScale, 0.5f ), 0.5f * m_fDrawWidthScale, 0.5f, m_pTexture[1],
                      D3DCOLORWRITEENABLE_ALL );
    DrawTexturedQuad( XMFLOAT2( 0.5f * m_fDrawWidthScale, -0.5f ), 0.5f * m_fDrawWidthScale, 0.5f, m_pTexture[2],
                      D3DCOLORWRITEENABLE_ALL );
    DrawTexturedQuad( XMFLOAT2( -0.5f * m_fDrawWidthScale, -0.5f ), 0.5f * m_fDrawWidthScale, 0.5f, m_pTexture[3],
                      D3DCOLORWRITEENABLE_RED );

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
        m_Font.DrawText( 0, 0, 0xffffffff, L"CustomEDRAMAllocation" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Wait for the vertical blank before we resolve to the front buffer to avoid tearing.
    m_pd3dDevice->SynchronizeToPresentationInterval();

    // Resolve back buffer to front buffer
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0,
                           NULL, m_pFrontBuffer, NULL, 0, NULL, NULL, 0.0f, 0, NULL );

    // Present the scene
    m_pd3dDevice->Swap( m_pFrontBuffer, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DrawColoredTriangle()
// Desc: Draws a colored triangle 
//--------------------------------------------------------------------------------------
VOID Sample::DrawColoredTriangle( const XMFLOAT2 vCenter,
                                  const FLOAT fWidth, const FLOAT fHeight,
                                  const FLOAT Color[4] )
{
    m_pd3dDevice->SetVertexShader( m_pColorVS );
    m_pd3dDevice->SetVertexDeclaration( m_pColorVD );
    m_pd3dDevice->SetPixelShader( m_pColorPS );

    COLORVERTEX v[3];
    v[0].vPos = XMFLOAT4( vCenter.x - fWidth / 2.0f, vCenter.y - fHeight / 2.0f, 0.0f, 1.0f );
    v[1].vPos = XMFLOAT4( vCenter.x, vCenter.y + fHeight / 2.0f, 0.0f, 1.0f );
    v[2].vPos = XMFLOAT4( vCenter.x + fWidth / 2.0f, vCenter.y - fHeight / 2.0f, 0.0f, 1.0f );

    m_pd3dDevice->SetPixelShaderConstantF( 0, Color, 1 );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLELIST, 1, v, sizeof( COLORVERTEX ) );
}


//--------------------------------------------------------------------------------------
// Name: DrawTexturedTriangle()
// Desc: Draws a textured quad
//--------------------------------------------------------------------------------------
VOID Sample::DrawTexturedQuad( const XMFLOAT2 vCenter,
                               const FLOAT fWidth, const FLOAT fHeight,
                               IDirect3DTexture9* pTexture,
                               DWORD Channel )
{
    m_pd3dDevice->SetVertexShader( m_pTextureVS );
    m_pd3dDevice->SetVertexDeclaration( m_pTextureVD );
    m_pd3dDevice->SetPixelShader( m_pTexturePS );

    TEXTUREVERTEX v[4];
    v[0].vPos = XMFLOAT4( vCenter.x - fWidth / 2.0f, vCenter.y - fHeight / 2.0f, 0.0f, 1.0f );
    v[1].vPos = XMFLOAT4( vCenter.x - fWidth / 2.0f, vCenter.y + fHeight / 2.0f, 0.0f, 1.0f );
    v[2].vPos = XMFLOAT4( vCenter.x + fWidth / 2.0f, vCenter.y + fHeight / 2.0f, 0.0f, 1.0f );
    v[3].vPos = XMFLOAT4( vCenter.x + fWidth / 2.0f, vCenter.y - fHeight / 2.0f, 0.0f, 1.0f );
    v[0].vTexCoord = XMFLOAT2( 0.0f, 1.0f );
    v[1].vTexCoord = XMFLOAT2( 0.0f, 0.0f );
    v[2].vTexCoord = XMFLOAT2( 1.0f, 0.0f );
    v[3].vTexCoord = XMFLOAT2( 1.0f, 1.0f );

    m_pd3dDevice->SetTexture( 0, pTexture );
    XMFLOAT4 bChannel( ( FLOAT )Channel != D3DCOLORWRITEENABLE_ALL, 0.0f, 0.0f, 0.0f );
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&bChannel, 1 );

    if( Channel != D3DCOLORWRITEENABLE_ALL )
    {
        XMFLOAT4 Color( 0.0f, 0.0f, 0.0f, 0.0f );
        switch( Channel )
        {
            case D3DCOLORWRITEENABLE_RED:
                Color.x = 1.0f; break;
            case D3DCOLORWRITEENABLE_GREEN:
                Color.y = 1.0f; break;
            case D3DCOLORWRITEENABLE_BLUE:
                Color.z = 1.0f; break;
            case D3DCOLORWRITEENABLE_ALPHA:
                Color.w = 1.0f; break;
        }

        m_pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&Color, 1 );
    }


    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, v, sizeof( TEXTUREVERTEX ) );
}


//--------------------------------------------------------------------------------------
// Name: CompleShader()
// Desc: Compiles a simple shader
//--------------------------------------------------------------------------------------
VOID Sample::CompileShader( SHADERTYPE Type,
                            const CHAR* strEntry, const CHAR* strTarget,
                            const CHAR* strProgram, VOID** ppShader )
{
    // Buffers to hold compiled shaders and possible error messages
    ID3DXBuffer* pShaderCode = NULL;
    ID3DXBuffer* pErrorMsg = NULL;

    // Compile vertex shader.
    HRESULT hr = D3DXCompileShader( strProgram, ( UINT )strlen( strProgram ),
                                    NULL, NULL, strEntry, strTarget, 0,
                                    &pShaderCode, &pErrorMsg, NULL );
    if( FAILED( hr ) )
    {
        OutputDebugStringA( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
        exit( 1 );
    }

    if( Type == VERTEXSHADER )
        m_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(), ( D3DVertexShader** )ppShader );
    else
        m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(), ( D3DPixelShader** )ppShader );

    // Shader code is no longer required.
    pShaderCode->Release();
}
