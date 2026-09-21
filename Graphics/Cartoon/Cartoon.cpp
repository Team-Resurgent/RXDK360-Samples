//--------------------------------------------------------------------------------------
// Cartoon.cpp
//
// Draws a model, simulating cartoon lighting by using a 1D texture map.
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
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_LEFT_STICK,   ATG::HELP_PLACEMENT_2, L"Move\nmodel" },
    { ATG::HELP_RIGHT_STICK,  ATG::HELP_PLACEMENT_2, L"Move\nlight" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Name: class ATG::CartoonMesh
// Desc: Overridden mesh class for rendering a mesh using a cartoon shader
//--------------------------------------------------------------------------------------
class CartoonMesh : public ATG::Mesh
{
public:
    BOOL RenderCallback( DWORD dwSubset, const ATG::MESH_SUBSET* pSubset, DWORD dwFlags )
    {
        // Set matrices for the vertex shader
        XMMATRIX matWorldView = XMMatrixMultiply( m_matWorld, m_matView );
        XMMATRIX matWorldViewProj = XMMatrixMultiply( matWorldView, m_matProj );
        XMMATRIX matWorldViewProjT = XMMatrixTranspose( matWorldViewProj );
        XMMATRIX matWorldT = XMMatrixTranspose( m_matWorld );
        ATG::g_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matWorldT, 4 );
        ATG::g_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&matWorldViewProjT, 4 );
        return TRUE;
    }
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

    XMMATRIX m_matObject;        // Local transform of the model
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    XMMATRIX m_matLight;         // Transform for the light
    XMVECTOR m_vRotLight;

    LPDIRECT3DTEXTURE9 m_pCartoonTexture;  // Texture surface

    LPDIRECT3DVERTEXSHADER9 m_pCartoonVertexShader; // Custom vertex shader
    LPDIRECT3DPIXELSHADER9 m_pCartoonPixelShader;  // Custom pixel shader
    LPDIRECT3DPIXELSHADER9 m_pConstantColorPS;     // Constant Color pixel shader

    CartoonMesh m_Mesh;

    HRESULT CreateCartoonTexture();

private:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize base member variables
    m_matObject = XMMatrixIdentity();
    m_matLight = XMMatrixIdentity();
    m_pCartoonTexture = NULL;
    m_pCartoonVertexShader = NULL;
    m_pCartoonPixelShader = NULL;
    m_bDrawHelp = FALSE;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create a mesh (vertex and index buffers) for the model
    if( FAILED( m_Mesh.Create( "game:\\Media\\Meshes\\Robot.xbg" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the projection matrix
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, 1.0f, 200.0f );

    // Set the view matrix
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -7.0f, 1.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 1.0f );
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );

    // The texture used by this renderer is procedurally generated.
    if( FAILED( CreateCartoonTexture() ) )
        return E_FAIL;

    // Create shaders for doing the cartoon effect
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\Cartoon.xvu", &m_pCartoonVertexShader ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\Cartoon.xpu", &m_pCartoonPixelShader ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ConstantColor.xpu", &m_pConstantColorPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateCartoonTexture()
// Desc: Creates the 1D texture map used as a modulation map based on vertex
//       lighting. This map holds quantized gray-scale values which are applied
//       to the underlying interpolated vertex color.
//--------------------------------------------------------------------------------------
HRESULT Sample::CreateCartoonTexture()
{
    if( FAILED( m_pd3dDevice->CreateTexture( 32, 1, 1, 0,
                                             D3DFMT_LIN_X8R8G8B8, D3DPOOL_MANAGED,
                                             &m_pCartoonTexture, 0 ) ) )
        return E_FAIL;

    // Get the pointer to the bits.
    D3DLOCKED_RECT lock;
    m_pCartoonTexture->LockRect( 0, &lock, NULL, 0L );
    DWORD* pTextureData = ( DWORD* )lock.pBits;

    for( INT i = 0; i < 32; i++ )
    {
        if( i < 8 )       pTextureData[i] = 0xff606060;
        else if( i < 18 ) pTextureData[i] = 0xffa2a2a2;
        else if( i < 29 ) pTextureData[i] = 0xffe5e5e5;
        else
            pTextureData[i] = 0xffffffff;
    }

    m_pCartoonTexture->UnlockRect( 0 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame to update the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT m_fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Perform object rotation
    XMMATRIX matRotate;
    FLOAT fXRotate1 = pGamepad->fX1 * m_fElapsedTime * XM_PI * 0.5f;
    FLOAT fYRotate1 = pGamepad->fY1 * m_fElapsedTime * XM_PI * 0.5f;
    matRotate = XMMatrixRotationRollPitchYaw( -fYRotate1, -fXRotate1, 0.0f );
    m_matObject = XMMatrixMultiply( m_matObject, matRotate );

    // Perform light rotation
    FLOAT fXRotate2 = fXRotate1 + pGamepad->fX2 * m_fElapsedTime * XM_PI * 0.5f;
    FLOAT fYRotate2 = fYRotate1 - pGamepad->fY2 * m_fElapsedTime * XM_PI * 0.5f;
    matRotate = XMMatrixRotationRollPitchYaw( -fYRotate2, -fXRotate2, 0.0f );
    m_matLight = XMMatrixMultiply( m_matLight, matRotate );

    // Set light position for the vertex shader
    XMVECTOR vDeterminant;
    XMVECTOR vRotLight = XMVectorSet( 0.3487f, 0.1162f, -0.9300f, 1.0f );
    XMMATRIX matLightInv = XMMatrixInverse( &vDeterminant, m_matLight );
    XMMATRIX matWorldInv = XMMatrixInverse( &vDeterminant, m_matObject );
    vRotLight = XMVector3TransformCoord( vRotLight, m_matLight );
    m_vRotLight = XMVector3TransformCoord( vRotLight, matWorldInv );

    // Set the matrices for the mesh
    m_Mesh.m_matWorld = m_matObject;
    m_Mesh.m_matView = m_matView;
    m_Mesh.m_matProj = m_matProj;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame to render the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff00ffff );

    // Set default states
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );

    // Set filtering to point sampling; otherwise we lose the sharp
    // transitions in the lighting that we wanted.
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );

    // Set the texture coordinates to be clamped so we do not have to clamp the
    // result of the dot product in the texture gen code. We only really need
    // to clamp U because V is fixed at 0.0.
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    // Draw the first pass, the textured visible polygons. We want to modulate 
    // the texture with the diffuse at stage 0. The texture is a banded gray 
    // scale, that we modulate with a fixed color (yellow) to give the lighting.
    FLOAT vYellow[] = { 1.0f, 1.0f, 0.0f, 1.0f };
    m_pd3dDevice->SetTexture( 0, m_pCartoonTexture );
    m_pd3dDevice->SetPixelShader( m_pCartoonPixelShader );
    m_pd3dDevice->SetPixelShaderConstantF( 0, vYellow, 1 );

    // Bind the cartoon vertex shader
    m_pd3dDevice->SetVertexShader( m_pCartoonVertexShader );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m_vRotLight, 1 );

    // Set an extrusion factor to shrink in the object (so that the next pass
    // will result in a black sihlouette)
    FLOAT fVertexExtrusionFactor[4] = { -0.025f, 0.0f, 0.0f, 0.0f };
    m_pd3dDevice->SetVertexShaderConstantF( 1, fVertexExtrusionFactor, 1 );

    // Draw the geometry
    m_Mesh.Render( ATG::MESH_NOMATERIALS | ATG::MESH_NOTEXTURES );

    // Draw the second pass, the black silhouette. Basically, scale the world
    // matrix up a little and then redraw in black.

    // When we draw the silhouette we derive it from the back-facing polygons; we
    // draw these polygons scaled through the above scale matrix. We should only
    // see black around edges where the culling order changes and therefore the
    // direction of the polygons changes.
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );

    // Switch to constant color pixel shader for rendering black outline
    m_pd3dDevice->SetPixelShader( m_pConstantColorPS );
    m_pd3dDevice->SetTexture( 0, NULL );

    // Finally, to draw the silhouette we just want to output black
    FLOAT vBlack[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_pd3dDevice->SetPixelShaderConstantF( 0, vBlack, 1 );

    // Remove the extrusion factor
    fVertexExtrusionFactor[0] = 0.0f;
    m_pd3dDevice->SetVertexShaderConstantF( 1, fVertexExtrusionFactor, 1 );

    // Draw the geometry
    m_Mesh.Render( ATG::MESH_NOMATERIALS | ATG::MESH_NOTEXTURES );

    // Restore modified states
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

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
        m_Font.DrawText( 0, 0, 0xffffffff, L"Cartoon" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
