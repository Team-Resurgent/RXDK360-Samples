//--------------------------------------------------------------------------------------
// CubicBezierPatch.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <d3d9.h>
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgResource.h"
#include "AtgUtil.h"

// Source patch data.
#include "Teapot.h"

// Shader microcode data
#include "SimplePS.h"
#include "CubicBezierPatchVS.h"
#include "DebugVS.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_1, L"Move camera" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_RIGHTSTICK,   ATG::HELP_PLACEMENT_2, L"Change\nTessellation Level" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Wireframe" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_1, L"Debug Display" },
    { ATG::HELP_MISC_CALLOUT, ATG::HELP_PLACEMENT_1, L"Use triggers to zoom in/out" },
};
const DWORD NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Vertex elements for a bicubic patch (compressed to 16-bit per component float).
//--------------------------------------------------------------------------------------
static const D3DVERTEXELEMENT9 PatchVertexElements[] =
{
    { 0,  0 * 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  0 },
    { 0,  1 * 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  1 },
    { 0,  2 * 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  2 },
    { 0,  3 * 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  3 },
    { 0,  4 * 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  4 },
    { 0,  5 * 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  5 },
    { 0,  6 * 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  6 },
    { 0,  7 * 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  7 },
    { 0,  8 * 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  8 },
    { 0,  9 * 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  9 },
    { 0, 10 * 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 10 },
    { 0, 11 * 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 11 },
    { 0, 12 * 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 12 },
    { 0, 13 * 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 13 },
    { 0, 14 * 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 14 },
    { 0, 15 * 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 15 },
    D3DDECL_END()
};

// Each patch consists of 16 control points each of which is eight bytes (16:16:16:16).
const UINT  g_PatchSize = 16 * sizeof( XMHALF4 );

// The number of pathces in our teapot.
const UINT  g_TeapotPatchCount = sizeof( g_vTeapotData ) / ( 16 * sizeof( XMFLOAT3 ) );


// Decls and indices for the debug display.
static const D3DVERTEXELEMENT9 DebugVertexElements[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  0 },
    D3DDECL_END()
};

static const WORD g_PatchEdges[] =
{
    0, 1, 1, 2, 2, 3, 3, 7, 7, 11, 11, 15, 15, 14, 14, 13, 13, 12, 12, 8, 8, 4, 4, 0
};

static const WORD g_PatchInternals[] =
{
    1, 5, 2, 6, 5, 9, 6, 10, 9, 13, 10, 14, 4, 5, 8, 9, 5, 6, 9, 10, 6, 7, 10, 11
};


//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    FLOAT m_fTessellationLevel;   // Max tessellation level.

    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    XMVECTOR m_vEye;
    XMVECTOR m_vLookAt;
    XMVECTOR m_vUp;

    BOOL m_bWireframe;
    BOOL m_bDebugDisplay;

    // Vertex declaration for the patch data.
    IDirect3DVertexDeclaration9* m_pVertexDecl;

    // Vertex buffer containing the patch data.
    IDirect3DVertexBuffer9* m_pVB;

    // Tessellation vertex shader.
    IDirect3DVertexShader9* m_pVertexShader;

    // Simple pixel shader for rendering.
    IDirect3DPixelShader9* m_pPixelShader;

    // Simple vertex shader for debug display.
    IDirect3DVertexShader9* m_pDebugVS;

    // Vertex declaration for the debug display.
    IDirect3DVertexDeclaration9* m_pDebugVertDecl;

    IDirect3DIndexBuffer9* m_pDebugEdgeIndices;
    IDirect3DIndexBuffer9* m_pDebugInternalIndices;

private:
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

    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    m_bDrawHelp = FALSE;

    m_fTessellationLevel = 8.0f;

    m_bWireframe = FALSE;
    m_bDebugDisplay = FALSE;

    // Create the vertex declaration.
    m_pd3dDevice->CreateVertexDeclaration( PatchVertexElements, &m_pVertexDecl );

    // Create the debug vertex declaration.
    m_pd3dDevice->CreateVertexDeclaration( DebugVertexElements, &m_pDebugVertDecl );

    // Create the vertex buffer.
    if( FAILED( m_pd3dDevice->CreateVertexBuffer( g_TeapotPatchCount * g_PatchSize,
                                                  D3DUSAGE_WRITEONLY,
                                                  NULL,
                                                  D3DPOOL_DEFAULT,
                                                  &m_pVB,
                                                  NULL ) ) )
        return E_FAIL;

    // Put the data for the patches into our vertex buffer.
    void* pVertices;
    if( FAILED( m_pVB->Lock( 0, 0, &pVertices, 0 ) ) )
        return E_FAIL;

    const XMFLOAT3* pSrc = g_vTeapotData;
    XMHALF4* pDst = ( XMHALF4* )pVertices;
    for( INT i = 0; i < g_TeapotPatchCount * 16; i++ )
    {
        // Convert the 32-bit float source data to packed 16-bit float data.
        XMVECTOR V = XMLoadFloat3( pSrc++ );

        // Set w to one.
        V = XMVectorInsert( V, XMVectorSplatOne(), 0, 0, 0, 0, 1 );

        XMStoreHalf4( pDst++, V );
    }

    m_pVB->Unlock();

    // Create debug index lists.
    m_pd3dDevice->CreateIndexBuffer( sizeof( g_PatchEdges ) * g_TeapotPatchCount * 2, 0,
                                     D3DFMT_INDEX16, 0, &m_pDebugEdgeIndices, NULL );

    WORD* pIndices;
    if( FAILED( m_pDebugEdgeIndices->Lock( 0, 0, ( void** )&pIndices, 0 ) ) )
        return E_FAIL;

    for( DWORD i = 0; i < g_TeapotPatchCount; i++ )
    {
        // Copy and offset indices.
        for( INT j = 0; j < sizeof( g_PatchEdges ) / sizeof( WORD ); j++ )
        {
            *pIndices++ = g_PatchEdges[j] + WORD( i * 16 );
        }
    }

    m_pDebugEdgeIndices->Unlock();

    m_pd3dDevice->CreateIndexBuffer( sizeof( g_PatchInternals ) * g_TeapotPatchCount * 2, 0,
                                     D3DFMT_INDEX16, 0, &m_pDebugInternalIndices, NULL );

    if( FAILED( m_pDebugInternalIndices->Lock( 0, 0, ( void** )&pIndices, 0 ) ) )
        return E_FAIL;

    for( DWORD i = 0; i < g_TeapotPatchCount; i++ )
    {
        // Copy and offset indices.
        for( INT j = 0; j < sizeof( g_PatchInternals ) / sizeof( WORD ); j++ )
        {
            *pIndices++ = g_PatchInternals[j] + WORD( i * 16 );
        }
    }

    m_pDebugInternalIndices->Unlock();

    // Load the microcode for the tessellation vertex shader.
    m_pd3dDevice->CreateVertexShader( g_CubicBezierPatchVS, &m_pVertexShader );

    // Load a simple pixel shader.
    m_pd3dDevice->CreatePixelShader( g_SimplePS, &m_pPixelShader );

    // Load the microcode for the debug vertex shader.
    m_pd3dDevice->CreateVertexShader( g_DebugVS, &m_pDebugVS );

    // Center the teapot.
    XMMATRIX matTranslate, matRotate;
    matTranslate = XMMatrixTranslation( -g_vTeapotCenter.x,
                                        -g_vTeapotCenter.y,
                                        -g_vTeapotCenter.z );

    // Rotate it 90 degrees around the x-axis so that top is up.
    matRotate = XMMatrixRotationX( XM_PI / 2 );

    m_matWorld = matTranslate * matRotate;

    // View matrix
    m_vEye = XMVectorSet( 0.0f, 50.0f, -300.0f, 0.0f );
    m_vLookAt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    m_vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matView = XMMatrixLookAtLH( m_vEye, m_vLookAt, m_vUp );

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Projection matrix
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, 1.0f, 1000.0f );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Get the elapsed time.
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_bWireframe = !m_bWireframe;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_bDebugDisplay = !m_bDebugDisplay;

    // Adjust the tessellation level.
    m_fTessellationLevel += pGamepad->fY2 * fElapsedTime * 2.0f;

    // Clamp tessellation level to be a legal value.
    const FLOAT fMaxLevel = 15.0f;

    if( m_fTessellationLevel > fMaxLevel )
        m_fTessellationLevel = fMaxLevel;
    else if( m_fTessellationLevel < 1.0f )
        m_fTessellationLevel = 1.0f;

    // Change view.

    // Rotate eye around up axis.
    XMMATRIX matRotate = XMMatrixRotationAxis( m_vUp, pGamepad->fX1 * fElapsedTime );
    m_vEye = XMVector3TransformCoord( m_vEye, matRotate );

    // Rotate eye points around side axis
    XMVECTOR vView = ( m_vLookAt - m_vEye );
    FLOAT dist = XMVector3Length( vView ).x;

    vView = XMVector3Normalize( vView );

    // Place limits so we don't go over the top or under the bottom
    FLOAT dot = XMVector3Dot( vView, m_vUp ).x;
    if( ( dot < 0.99f || pGamepad->fY1 < 0.0f ) && ( dot > -0.99f || pGamepad->fY1 > 0.0f ) )
    {
        XMVECTOR vAxis = XMVector3Cross( vView, m_vUp );
        matRotate = XMMatrixRotationAxis( vAxis, pGamepad->fY1 * fElapsedTime );
        m_vEye = XMVector3TransformCoord( m_vEye, matRotate );
    }

    // Move in/out
    FLOAT fIn = ( pGamepad->bRightTrigger / 255.0f );
    FLOAT fOut = ( pGamepad->bLeftTrigger / 255.0f );

    if( fIn > 0.1f && dist > 1.0f )
        m_vEye += vView * 40.0f * fIn * fElapsedTime;

    if( fOut > 0.1f )
        m_vEye -= vView * 40.0f * fOut * fElapsedTime;

    m_matView = XMMatrixLookAtLH( m_vEye, m_vLookAt, m_vUp );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

    // Set the composite world*view*projection matrix.
    XMMATRIX matWVP = m_matWorld * m_matView * m_matProj;
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );

    // Set the tessellation level.
    m_pd3dDevice->SetRenderState( D3DRS_MAXTESSELLATIONLEVEL,
                                  ATG::FtoDW( m_fTessellationLevel ) );

    // Set the tessellation mode.
    m_pd3dDevice->SetRenderState( D3DRS_TESSELLATIONMODE, D3DTM_CONTINUOUS );

    if( m_bWireframe )
        m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );

    // Set the shaders.
    m_pd3dDevice->SetVertexShader( m_pVertexShader );
    m_pd3dDevice->SetPixelShader( m_pPixelShader );

    // Set the vertex declaration.
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );

    // Set the vertex buffer.
    m_pd3dDevice->SetStreamSource( 0, m_pVB, 0, g_PatchSize );

    // Set the light direction.
    XMFLOAT4 vLocalLightDireciton( 0.0f, -0.707f, -0.707f, 0.0f );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&vLocalLightDireciton, 1 );

    // Render the patch data.
    m_pd3dDevice->DrawTessellatedPrimitive( D3DTPT_QUADPATCH, 0, g_TeapotPatchCount );

    if( m_bDebugDisplay )
    {
        // Debug display of the control mesh.

        // Set the shaders.
        m_pd3dDevice->SetVertexShader( m_pDebugVS );
        m_pd3dDevice->SetPixelShader( m_pPixelShader );

        // Set the vertex declaration.
        m_pd3dDevice->SetVertexDeclaration( m_pDebugVertDecl );

        // Set the vertex buffer.
        m_pd3dDevice->SetStreamSource( 0, m_pVB, 0, sizeof( XMHALF4 ) );

        // Draw the patch edges in red.    
        XMFLOAT4 vRed( 0.9f, 0.0f, 0.0f, 0.0f );
        m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&vRed, 1 );

        m_pd3dDevice->SetIndices( m_pDebugEdgeIndices );

        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );

        m_pd3dDevice->DrawIndexedPrimitive( D3DPT_LINELIST, 0, 0, 0, 0,
                                            g_TeapotPatchCount * sizeof( g_PatchEdges ) / 2 );

        // Draw the internal parts of the patch in green.
        XMFLOAT4 vGreen( 0.0f, 0.9f, 0.0f, 0.0f );
        m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&vGreen, 1 );

        m_pd3dDevice->SetIndices( m_pDebugInternalIndices );

        m_pd3dDevice->DrawIndexedPrimitive( D3DPT_LINELIST, 0, 0, 0, 0,
                                            g_TeapotPatchCount * sizeof( g_PatchInternals ) / 2 );
    }

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
        m_Font.DrawText( 0, 0, 0xffffffff, L"CubicBezierPatch" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        WCHAR str[32];
        swprintf_s( str, L"Tess. Level: %0.01f", m_fTessellationLevel );
        m_Font.DrawText( 0, 30, 0xffffffff, str, ATGFONT_RIGHT );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
