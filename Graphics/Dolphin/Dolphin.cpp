//--------------------------------------------------------------------------------------
// Dolphin.cpp
//
// Sample of swimming dolphin
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Globals variables and definitions
//--------------------------------------------------------------------------------------
const DWORD g_dwWaterColor = 0x00004080;
const FLOAT g_fWaterColor[] = { 0.0f, 0.25f, 0.5f, 1.0f };


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;    // Timer
    ATG::Font m_Font;     // Font for drawing text
    ATG::PackedResource m_Resource; // Bundled textures in a packed resource

    // Transform matrices
    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    // Dolphin object
    ATG::Mesh2 m_DolphinMesh1;
    ATG::Mesh2 m_DolphinMesh2;
    ATG::Mesh2 m_DolphinMesh3;
    LPDIRECT3DTEXTURE9 m_pDolphinTexture;
    LPDIRECT3DVERTEXBUFFER9 m_pDolphinVB1;
    LPDIRECT3DVERTEXBUFFER9 m_pDolphinVB2;
    LPDIRECT3DVERTEXBUFFER9 m_pDolphinVB3;
    LPDIRECT3DINDEXBUFFER9 m_pDolphinIB;
    D3DPRIMITIVETYPE m_dwDolphinPrimType;
    DWORD m_dwDolphinVertexSize;
    DWORD m_dwNumDolphinVertices;
    DWORD m_dwNumDolphinPrimitives;
    LPDIRECT3DVERTEXDECLARATION9 m_pDolphinVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9 m_pDolphinVertexShader;

    // Seafloor object
    ATG::Mesh2 m_SeaFloorMesh;
    LPDIRECT3DTEXTURE9 m_pSeaFloorTexture;
    LPDIRECT3DVERTEXBUFFER9 m_pSeaFloorVB;
    LPDIRECT3DINDEXBUFFER9 m_pSeaFloorIB;
    D3DPRIMITIVETYPE m_dwSeaFloorPrimType;
    DWORD m_dwSeaFloorVertexSize;
    DWORD m_dwNumSeaFloorVertices;
    DWORD m_dwNumSeaFloorPrimitives;
    LPDIRECT3DVERTEXDECLARATION9 m_pSeaFloorVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9 m_pSeaFloorVertexShader;

    // Water caustics
    LPDIRECT3DTEXTURE9  m_pCausticTextures[32];
    LPDIRECT3DTEXTURE9 m_pCurrentCausticTexture;

    LPDIRECT3DPIXELSHADER9 m_pPixelShader;

public:
    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();
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
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    // Create the font
    if( FAILED( hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create font\n" );
        return hr;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the textures resource
    if( FAILED( hr = m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create Resource.xpr\n" );
        return hr;
    }

    m_pDolphinTexture = m_Resource.GetTexture( "DolphinTexture" );
    m_pSeaFloorTexture = m_Resource.GetTexture( "SeafloorTexture" );

    for( DWORD t = 0; t < 32; t++ )
    {
        CHAR strTextureName[80];
        sprintf_s( strTextureName, "WaterCaustic%02ld", t );
        m_pCausticTextures[t] = m_Resource.GetTexture( strTextureName );
    }

    if( FAILED( hr = m_DolphinMesh1.Create( "game:\\Media\\Meshes\\dolphin1.xbg" ) ) )
    {
        ATG_PrintError( "Couldn't create Dolphin1.xbg\n" );
        return hr;
    }
    if( FAILED( hr = m_DolphinMesh2.Create( "game:\\Media\\Meshes\\dolphin2.xbg" ) ) )
    {
        ATG_PrintError( "Couldn't create Dolphin2.xbg\n" );
        return hr;
    }
    if( FAILED( hr = m_DolphinMesh3.Create( "game:\\Media\\Meshes\\dolphin3.xbg" ) ) )
    {
        ATG_PrintError( "Couldn't create Dolphin3.xbg\n" );
        return hr;
    }
    if( FAILED( hr = m_SeaFloorMesh.Create( "game:\\Media\\Meshes\\Seafloor.xbg" ) ) )
    {
        ATG_PrintError( "Couldn't create Seafloor.xbg\n" );
        return hr;
    }

    m_pDolphinVB1 = &m_DolphinMesh1.GetMesh()->m_VB;
    m_pDolphinVB2 = &m_DolphinMesh2.GetMesh()->m_VB;
    m_pDolphinVB3 = &m_DolphinMesh3.GetMesh()->m_VB;
    m_pDolphinIB = &m_DolphinMesh1.GetMesh()->m_IB;

    m_pSeaFloorVB = &m_SeaFloorMesh.GetMesh()->m_VB;
    m_pSeaFloorIB = &m_SeaFloorMesh.GetMesh()->m_IB;

    // Get the number of vertices and faces for the meshes
    m_dwDolphinPrimType = m_DolphinMesh1.GetMesh()->m_dwPrimType;
    m_dwNumDolphinVertices = m_DolphinMesh1.GetMesh()->m_pSubsets[0].dwVertexCount;
    m_dwNumDolphinPrimitives = m_DolphinMesh1.GetMesh()->m_pSubsets[0].dwPrimitiveCount;
    m_dwDolphinVertexSize = m_DolphinMesh1.GetMesh()->m_dwVertexSize;

    m_dwSeaFloorPrimType = m_SeaFloorMesh.GetMesh()->m_dwPrimType;
    m_dwNumSeaFloorVertices = m_SeaFloorMesh.GetMesh()->m_pSubsets[0].dwVertexCount;
    m_dwNumSeaFloorPrimitives = m_SeaFloorMesh.GetMesh()->m_pSubsets[0].dwPrimitiveCount;
    m_pSeaFloorVertexDeclaration = m_SeaFloorMesh.GetMesh()->m_pVertexDecl;
    m_dwSeaFloorVertexSize = m_SeaFloorMesh.GetMesh()->m_dwVertexSize;

    // Add some bumpiness to the seafloor
    {
        srand( 5 );
        BYTE* pDst;
        m_pSeaFloorVB->Lock( 0, 0, ( VOID** )&pDst, 0 );
        for( DWORD i = 0; i < m_dwNumSeaFloorVertices; i++ )
        {
            ( ( XMFLOAT3* )pDst )->y += ( rand() / ( FLOAT )RAND_MAX );
            ( ( XMFLOAT3* )pDst )->y += ( rand() / ( FLOAT )RAND_MAX );
            ( ( XMFLOAT3* )pDst )->y += ( rand() / ( FLOAT )RAND_MAX );
            pDst += m_dwSeaFloorVertexSize;
        }
        m_pSeaFloorVB->Unlock();
    }

    // Build the vertex declaration for the dolphin
    D3DVERTEXELEMENT9 declDolphin[MAXD3DDECLLENGTH] = { 0 };
    ATG::AppendVertexElements( declDolphin, 0, m_DolphinMesh1.GetMesh()->m_VertexElements, 0 );
    ATG::AppendVertexElements( declDolphin, 1, m_DolphinMesh2.GetMesh()->m_VertexElements, 1 );
    ATG::AppendVertexElements( declDolphin, 2, m_DolphinMesh3.GetMesh()->m_VertexElements, 2 );

    // Create vertex shader for the dolphin
    if( FAILED( hr = m_pd3dDevice->CreateVertexDeclaration( declDolphin, &m_pDolphinVertexDeclaration ) ) )
    {
        ATG_PrintError( "Couldn't create vertex declaration\n" );
        return hr;
    }
    if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\DolphinTween.xvu", &m_pDolphinVertexShader ) ) )
    {
        ATG_PrintError( "Couldn't create DolphinTween.xvu\n" );
        return hr;
    }

    if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\SeaFloor.xvu", &m_pSeaFloorVertexShader ) ) )
    {
        ATG_PrintError( "Couldn't create SeaFloor.xvu\n" );
        return hr;
    }

    // Create the common pixel shader
    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeCausticsPixel.xpu", &m_pPixelShader ) ) )
    {
        ATG_PrintError( "Couldn't create ShadeCausticsPixel.xpu\n" );
        return hr;
    }

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the transform matrices
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -5.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matWorld = XMMatrixIdentity();
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, 1.0f, 10000.0f );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current time
    FLOAT fTime = ( FLOAT )m_Timer.GetAppTime();

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Let the user pause the animation
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        static BOOL bPaused = FALSE;
        bPaused = !bPaused;

        if( bPaused )   m_Timer.Stop();
        else
            m_Timer.Start();
    }

    // Animation attributes for the dolphin
    FLOAT fKickFreq = 2 * fTime;
    FLOAT fPhase = fTime / 3;
    FLOAT fBlendWeight = sinf( fKickFreq );

    // Move the dolphin in a circle
    XMMATRIX matDolphin, matTrans, matRotate1, matRotate2;
    matDolphin = XMMatrixScaling( 0.01f, 0.01f, 0.01f );
    matRotate1 = XMMatrixRotationZ( -cosf( fKickFreq ) / 6 );
    matDolphin = XMMatrixMultiply( matDolphin, matRotate1 );
    matRotate2 = XMMatrixRotationY( fPhase );
    matDolphin = XMMatrixMultiply( matDolphin, matRotate2 );
    matTrans = XMMatrixTranslation( -5 * sinf( fPhase ), sinf( fKickFreq ) / 2, 10 - 10 * cosf( fPhase ) );
    matDolphin = XMMatrixMultiply( matDolphin, matTrans );

    // Animate the caustic textures
    DWORD tex = ( ( DWORD )( fTime * 32 ) ) % 32;
    m_pCurrentCausticTexture = m_pCausticTextures[tex];

    // Set the vertex shader constants. Note: outside of the blend matrices,
    // most of these values don't change, so don't need to really be set every
    // frame. It's just done here for clarity
    {
        // Some basic constants
        static XMFLOAT4 vZero( 0.0f, 0.0f, 0.0f, 0.0f );
        static XMFLOAT4 vConstants( 1.0f, 0.5f, 0.2f, 0.05f );

        FLOAT fWeight1;
        FLOAT fWeight2;
        FLOAT fWeight3;

        if( fBlendWeight > 0.0f )
        {
            fWeight1 = fabsf( fBlendWeight );
            fWeight2 = 1.0f - fabsf( fBlendWeight );
            fWeight3 = 0.0f;
        }
        else
        {
            fWeight1 = 0.0f;
            fWeight2 = 1.0f - fabsf( fBlendWeight );
            fWeight3 = fabsf( fBlendWeight );
        }
        XMVECTOR vWeight = XMVectorSet( fWeight1, fWeight2, fWeight3, 0.0f );

        // Lighting vectors (in world space and in dolphin model space)
        // and other constants
        XMVECTOR vLight = XMVectorSet( 0.00f, 1.00f, 0.00f, 0.00f );
        XMVECTOR vLightDolphinSpace = XMVectorSet( 0.00f, 1.00f, 0.00f, 0.00f );
        XMVECTOR vDiffuse = XMVectorSet( 1.00f, 1.00f, 1.00f, 1.00f );
        XMVECTOR vAmbient = XMVectorSet( 0.25f, 0.25f, 0.25f, 0.25f );
        XMVECTOR vFog = XMVectorSet( 0.50f, 50.00f, 1.00f / ( 50.0f - 1.0f ), 0.00f );
        XMVECTOR vCaustics = XMVectorSet( 0.05f, 0.05f, sinf( fTime ) / 8, cosf( fTime ) / 10 );

        XMVECTOR vDeterminant;
        XMMATRIX matDolphinInv = XMMatrixInverse( &vDeterminant, matDolphin );
        vLightDolphinSpace = XMVector4Normalize( XMVector4Transform( vLight, matDolphinInv ) );

        // Vertex shader operations use transposed matrices
        XMMATRIX mat, matCamera, matTranspose, matCameraTranspose;
        XMMATRIX matViewTranspose, matProjTranspose;
        matCamera = XMMatrixMultiply( matDolphin, m_matView );
        mat = XMMatrixMultiply( matCamera, m_matProj );
        matTranspose = XMMatrixTranspose( mat );
        matCameraTranspose = XMMatrixTranspose( matCamera );
        matViewTranspose = XMMatrixTranspose( m_matView );
        matProjTranspose = XMMatrixTranspose( m_matProj );

        // Set the vertex shader constants
        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&vZero, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 1, ( FLOAT* )&vConstants, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 2, ( FLOAT* )&vWeight, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matTranspose, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&matCameraTranspose, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 12, ( FLOAT* )&matViewTranspose, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 16, ( FLOAT* )&matProjTranspose, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( 20, ( FLOAT* )&vLight, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 21, ( FLOAT* )&vLightDolphinSpace, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 22, ( FLOAT* )&vDiffuse, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 23, ( FLOAT* )&vAmbient, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 24, ( FLOAT* )&vFog, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 25, ( FLOAT* )&vCaustics, 1 );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering. This 
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                         g_dwWaterColor, 1.0f, 0L );

    // Initialize default device states at the start of the frame
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    // Set the common pixel shader
    static FLOAT fAmbient[] = { 0.25f, 0.25f, 0.25f, 0.25f };
    m_pd3dDevice->SetPixelShader( m_pPixelShader );
    m_pd3dDevice->SetPixelShaderConstantF( 0, g_fWaterColor, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 1, fAmbient, 1 );

    // Set the water caustics texture.
    m_pd3dDevice->SetTexture( 1, m_pCurrentCausticTexture );

    // Render the seafloor
    m_pd3dDevice->SetTexture( 0, m_pSeaFloorTexture );
    m_pd3dDevice->SetVertexDeclaration( m_pSeaFloorVertexDeclaration );
    m_pd3dDevice->SetVertexShader( m_pSeaFloorVertexShader );
    m_pd3dDevice->SetStreamSource( 0, m_pSeaFloorVB, 0, m_dwSeaFloorVertexSize );
    m_pd3dDevice->SetIndices( m_pSeaFloorIB );
    m_pd3dDevice->DrawIndexedPrimitive( m_dwSeaFloorPrimType, 0,
                                        0, m_dwNumSeaFloorVertices,
                                        0, m_dwNumSeaFloorPrimitives );

    // Render the dolphin
    m_pd3dDevice->SetTexture( 0, m_pDolphinTexture );
    m_pd3dDevice->SetVertexDeclaration( m_pDolphinVertexDeclaration );
    m_pd3dDevice->SetVertexShader( m_pDolphinVertexShader );
    m_pd3dDevice->SetStreamSource( 0, m_pDolphinVB1, 0, m_dwDolphinVertexSize );
    m_pd3dDevice->SetStreamSource( 1, m_pDolphinVB2, 0, m_dwDolphinVertexSize );
    m_pd3dDevice->SetStreamSource( 2, m_pDolphinVB3, 0, m_dwDolphinVertexSize );
    m_pd3dDevice->SetIndices( m_pDolphinIB );
    m_pd3dDevice->DrawIndexedPrimitive( m_dwDolphinPrimType, 0,
                                        0, m_dwNumDolphinVertices,
                                        0, m_dwNumDolphinPrimitives );

    // Output title and framerate
    m_Timer.MarkFrame();

    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"Dolphin" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // Display the total time the app has been running
        DOUBLE fAppTimeInSeconds = m_Timer.GetAppTime();
        DOUBLE fAppTimeInMinutes = fAppTimeInSeconds / 60.0;
        DOUBLE fAppTimeInHours = fAppTimeInMinutes / 60.0;
        DOUBLE fAppTimeInDays = fAppTimeInHours / 24.0;

        DWORD dwSeconds = ( DWORD )( floor( fAppTimeInSeconds ) ) % 60;
        DWORD dwMinutes = ( DWORD )( floor( fAppTimeInMinutes ) ) % 60;
        DWORD dwHours = ( DWORD )( floor( fAppTimeInHours ) ) % 24;
        DWORD dwDays = ( DWORD )( floor( fAppTimeInDays ) );

        WCHAR strTime[80];
        swprintf_s( strTime, L"%02ldd%02ldh%02ldm%02lds",
                    dwDays, dwHours, dwMinutes, dwSeconds );
        m_Font.DrawText( 0, 20, 0xffffff00, strTime, ATGFONT_RIGHT );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
