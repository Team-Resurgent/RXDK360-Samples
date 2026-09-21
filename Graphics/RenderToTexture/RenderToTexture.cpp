//--------------------------------------------------------------------------------------
// RenderToTexture.cpp
//
// Shows how to render a scene to a secondary render target and use the Resolve() API
// to copy the result to a texture.
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


struct MIRROR_VERTEX
{
    FLOAT x, y, z;
    FLOAT tu, tv;
};

MIRROR_VERTEX g_MirrorVertices[4] =
{
    //  x      y      z     tu    tv
    { -1.0f, +1.0f, 2.0f,  0.0f, 1.0f },
    { +1.0f, +1.0f, 2.0f,  1.0f, 1.0f },
    { +1.0f, -1.0f, 2.0f,  1.0f, 0.0f },
    { -1.0f, -1.0f, 2.0f,  0.0f, 0.0f },
};


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_2, L"Rotate\nscene" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


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

    ATG::Mesh m_Mesh;
    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matReflectedView;
    XMMATRIX m_matProj;
    XMMATRIX m_matWorldViewProj;

    LPDIRECT3DTEXTURE9 m_pLightingTexture;
    LPDIRECT3DTEXTURE9 m_pOffscreenTexture;
    D3DTexture         m_OffscreenTextureAs16SRGB;
    LPDIRECT3DSURFACE9 m_pOffscreenRenderTarget;

    LPDIRECT3DVERTEXDECLARATION9 m_pMirrorVertexDecl;
    LPDIRECT3DVERTEXSHADER9 m_pMirrorVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pMirrorPixelShader;

    LPDIRECT3DVERTEXSHADER9 m_pMeshVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pMeshPixelShader;

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
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the app
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

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

    // Create a mesh
    if( FAILED( hr = m_Mesh.Create( "game:\\Media\\Meshes\\Teapot.xbg" ) ) )
    {
        ATG_PrintError( "Couldn't load mesh\n" );
        return hr;
    }

    // Create a texture for simple lighting
    {
        if( FAILED( hr = m_pd3dDevice->CreateTexture( 256, 1, 1, 0, D3DFMT_LIN_L8,
                                                      D3DPOOL_DEFAULT, &m_pLightingTexture, NULL ) ) )
            return E_FAIL;

        D3DLOCKED_RECT lock;
        m_pLightingTexture->LockRect( 0, &lock, NULL, 0 );
        BYTE* pd = ( BYTE* )lock.pBits;
        for( DWORD x = 0; x < 256; x++ )
            *pd++ = ( BYTE )x;
        m_pLightingTexture->UnlockRect( 0 );
    }

    // Create a 256 x 256 texture to render into
    if( FAILED( m_pd3dDevice->CreateTexture( 256, 256, 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
                                             D3DPOOL_DEFAULT, &m_pOffscreenTexture, NULL ) ) )
        return E_FAIL;

    // Set up an alias of the resolve texture as an AS_16 sRGB format, since the GPU can't resolve 
    // to an AS_16_16_16_16 format. 
    // Alias this texture so there's no loss of precision when sampling the texture in the shader. If using
    // a standard SRGB format, the sRGB->Linear conversion happens with 8 bit precision, resulting 
    // in a loss of data. Using an AS_16 sRGB format causes the conversion to happen at 16-bit precision,
    // meaning no precision is lost.
    m_OffscreenTextureAs16SRGB = *m_pOffscreenTexture;
    ATG::ConvertTextureToAs16SRGBFormat( &m_OffscreenTextureAs16SRGB );

    // Create a 256x256 render target for rendering into the texture
    if( FAILED( m_pd3dDevice->CreateRenderTarget( 256, 256, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
                                                  D3DMULTISAMPLE_NONE, 0, 0,
                                                  &m_pOffscreenRenderTarget, NULL ) ) )
        return E_FAIL;

    // Create the vertex and pixel shaders
    {
        static const D3DVERTEXELEMENT9 decl[] =
        {
            { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
            { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };
        m_pd3dDevice->CreateVertexDeclaration( decl, &m_pMirrorVertexDecl );

        if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\Mirror.xvu",
                                                &m_pMirrorVertexShader ) ) )
        {
            ATG_PrintError( "Couldn't load vertex shader\n" );
            return hr;
        }
        if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\Mirror.xpu",
                                               &m_pMirrorPixelShader ) ) )
        {
            ATG_PrintError( "Couldn't load pixel shader\n" );
            return hr;
        }
    }
    {
        if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\Mesh.xvu",
                                                &m_pMeshVertexShader ) ) )
        {
            ATG_PrintError( "Couldn't load vertex shader\n" );
            return hr;
        }
        if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\Mesh.xpu",
                                               &m_pMeshPixelShader ) ) )
        {
            ATG_PrintError( "Couldn't load pixel shader\n" );
            return hr;
        }
    }

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Initialize the transforms
    XMVECTOR vEyePt = XMVectorSet( 4.0f, 1.0f, -3.0f, 0.0f );
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
    FLOAT fXRotate1 = pGamepad->fX1 * fElapsedTime * XM_PI * 0.5f;
    FLOAT fYRotate1 = pGamepad->fY1 * fElapsedTime * XM_PI * 0.5f;
    matRotateX = XMMatrixRotationX( +fYRotate1 );
    matRotateY = XMMatrixRotationY( -fXRotate1 );
    s_matRotateX = XMMatrixMultiply( s_matRotateX, matRotateX );
    s_matRotateY = XMMatrixMultiply( s_matRotateY, matRotateY );
    m_matWorld = XMMatrixMultiply( s_matRotateX, s_matRotateY );

    // Calc the plane for the mirror, in world space
    XMVECTOR vMirrorPosition = XMVectorSet( 0.0f, 0.0f, 2.0f, 0.0f ); // Should compute!
    XMVECTOR vMirrorNormal = XMVectorSet( 0.0f, 0.0f, -1.0f, 0.0f );  // Should compute!
    XMVECTOR vPlane;
    vPlane = XMPlaneFromPointNormal( vMirrorPosition, vMirrorNormal );
    vPlane = XMPlaneTransform( vPlane, m_matWorld );

    // Reflect the camera in the plane
    XMMATRIX matReflection;
    matReflection = XMMatrixReflect( vPlane );
    m_matReflectedView = XMMatrixMultiply( matReflection, m_matView );
    m_matReflectedView._11 *= -1;
    m_matReflectedView._21 *= -1;
    m_matReflectedView._31 *= -1;
    m_matReflectedView._41 *= -1;

    // Compute texture coords
    XMVECTOR v0 = XMVectorSet( -1.0f, +1.0f, 2.0f, 1.0f );
    XMVECTOR v1 = XMVectorSet( +1.0f, +1.0f, 2.0f, 1.0f );
    XMVECTOR v2 = XMVectorSet( +1.0f, -1.0f, 2.0f, 1.0f );
    XMVECTOR v3 = XMVectorSet( -1.0f, -1.0f, 2.0f, 1.0f );

    v0 = XMVector4Transform( v0, m_matWorld ); v0 = XMVector4Transform( v0, m_matReflectedView );
    v0 = XMVector4Transform( v0, m_matProj ); v0 /= v0.w;
    v1 = XMVector4Transform( v1, m_matWorld ); v1 = XMVector4Transform( v1, m_matReflectedView );
    v1 = XMVector4Transform( v1, m_matProj ); v1 /= v1.w;
    v2 = XMVector4Transform( v2, m_matWorld ); v2 = XMVector4Transform( v2, m_matReflectedView );
    v2 = XMVector4Transform( v2, m_matProj ); v2 /= v2.w;
    v3 = XMVector4Transform( v3, m_matWorld ); v3 = XMVector4Transform( v3, m_matReflectedView );
    v3 = XMVector4Transform( v3, m_matProj ); v3 /= v3.w;

    // Set the texture coordinates for the mirror
    g_MirrorVertices[0].tu = ( 1 + v0.x ) / 2; g_MirrorVertices[0].tv = ( 1 - v0.y ) / 2;
    g_MirrorVertices[1].tu = ( 1 + v1.x ) / 2; g_MirrorVertices[1].tv = ( 1 - v1.y ) / 2;
    g_MirrorVertices[2].tu = ( 1 + v2.x ) / 2; g_MirrorVertices[2].tv = ( 1 - v2.y ) / 2;
    g_MirrorVertices[3].tu = ( 1 + v3.x ) / 2; g_MirrorVertices[3].tv = ( 1 - v3.y ) / 2;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame to render the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Set matrices
    XMMATRIX matWorldT, matWorldViewProjT, matReflectedWorldViewProjT;
    m_matWorldViewProj = XMMatrixMultiply( m_matWorld, m_matView );
    m_matWorldViewProj = XMMatrixMultiply( m_matWorldViewProj, m_matProj );
    matWorldT = XMMatrixTranspose( m_matWorld );
    matWorldViewProjT = XMMatrixTranspose( m_matWorldViewProj );

    m_matWorldViewProj = XMMatrixMultiply( m_matWorld, m_matReflectedView );
    m_matWorldViewProj = XMMatrixMultiply( m_matWorldViewProj, m_matProj );
    matReflectedWorldViewProjT = XMMatrixTranspose( m_matWorldViewProj );

    // Prepare the states for rendering the mesh
    {
        XMFLOAT4 vLightDir( 1, 1, 1, 1 );
        FLOAT fExtrustionFactor = 0.0f;

        m_pd3dDevice->SetVertexShader( m_pMeshVertexShader );
        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&vLightDir, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 1, ( FLOAT* )&fExtrustionFactor, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matWorldT, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&matWorldViewProjT, 4 );

        XMFLOAT4 vDiffuse( 1.0f, 1.0f, 0.0, 1.0f );
        m_pd3dDevice->SetPixelShader( m_pMeshPixelShader );
        m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&vDiffuse, 1 );
        m_pd3dDevice->SetTexture( 0, m_pLightingTexture );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    }

    // Render the scene to a texture
    {
        // Save the old render target
        D3DSurface* pRenderTarget0;
        m_pd3dDevice->GetRenderTarget( 0, &pRenderTarget0 );

        // Set the render target to be our offscreen texture
        m_pd3dDevice->SetRenderTarget( 0, m_pOffscreenRenderTarget );

        // Clear the scene
        m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff000040, 1.0f, 0 );

        // Render the mesh
        m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&matReflectedWorldViewProjT, 4 );
        m_Mesh.Render( ATG::MESH_NOTEXTURES );

        // Resolve the render target to a texture
        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pOffscreenTexture,
                               NULL, 0, 0, NULL, 0, 0, NULL );

        // Set the render target back to the back buffer
        m_pd3dDevice->SetRenderTarget( 0, pRenderTarget0 );
        pRenderTarget0->Release();
    }

    // Clear the scene
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff404040, 1.0f, 0 );

    // Render the mesh in the main render target
    m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&matWorldViewProjT, 4 );
    m_Mesh.Render( ATG::MESH_NOTEXTURES );

    // Finally, use the resolved texture in the main scene
    {
        m_pd3dDevice->SetVertexDeclaration( m_pMirrorVertexDecl );
        m_pd3dDevice->SetVertexShader( m_pMirrorVertexShader );
        m_pd3dDevice->SetPixelShader( m_pMirrorPixelShader );
        m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&matWorldViewProjT, 4 );

        m_pd3dDevice->SetTexture( 0, &m_OffscreenTextureAs16SRGB );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

        // Draw the mirror
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, g_MirrorVertices, sizeof( g_MirrorVertices[0] ) );
    }

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bShowHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"RenderToTexture" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
