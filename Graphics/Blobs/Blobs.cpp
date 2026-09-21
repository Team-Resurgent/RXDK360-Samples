//--------------------------------------------------------------------------------------
// Blobs.cpp
//
// This sample mimics a metaball effect in screen space using a pixel shader. True
// metaball techniques deform surfaces according to pushing or pulling modifiers, and
// are commonly used to model liquid effects like the merging of water droplets;
// however, metaball effects can be computationally expensive, and this sample shows
// how to fake a 3D metaball effect in 2D image space using a pixel shader.
//
// Metaballs (also called Blinn blobs after James Blinn who formulated the technique)
// are included with most modeling software as an easy way to create smooth surfaces
// from a series of 'blobs' which merge together; the apparent flow between blobs
// makes the technique particularly well suited for particle systems which model
// liquids.
//
// The underlying technique is based on isosurfaces, equations in 3-dimensions which
// define a closed volume. For instance, x^2 + y^2 + z^2 = 1 defines a sphere of
// radius 1 centered at the origin. This idea extends to arbitrary 3D functions; 
// given a continuous function, the set of all points along that function which are
// equal to an arbitrary constant value define an isosurface. Individual metaballs
// are often implemented as spheres defined by Gaussian distributions where the
// highest value exists at the sphere's center and all values above an arbitrary
// threshold are defined to be inside the sphere; this threshold is subtracted from the
// Gaussian height which yields a surface height of zero at the blob edge. Since the
// surface is completely defined according to this threshold, the values of overlapping
// spheres can be added to produce a new surface. 
//
// For a true metaball effect, these calculations would be made in 3-dimensional space.
// Instead, this sample performs the computation in 2-dimensional screen space, which
// yields a nice visual effect requiring very little computational effort; however,
// you'll notice that accuracy is lost since blobs in the background will merge with
// blobs near the camera as they align along the view vector.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
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
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>

#define V_RETURN(fn)    { if(FAILED(hr=(fn)))return hr;}


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_2, L"Rotate\nblobs" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_1, L"Pause" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
#define NUM_BLOBS           5
#define FIELD_OF_VIEW       ( (70.0f/90.0f)*(XM_PI/2.0f) )

#define GAUSSIAN_TEXSIZE    64
#define GAUSSIAN_HEIGHT     1
#define GAUSSIAN_DEVIATION  0.125f



//--------------------------------------------------------------------------------------
// Custom types
//--------------------------------------------------------------------------------------
struct BLOBDATA
{
    XMFLOAT3 pos;
    FLOAT size;
    XMFLOAT4 color;
};

struct BLOBVERTEX
{
    XMFLOAT2 vTexCoord;
};


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
const DWORD TEX_SourceBlob = 0;
const DWORD TEX_NormalBuffer = 1;
const DWORD TEX_ColorBuffer = 2;
const DWORD TEX_EnvMap = 3;





//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::PackedResource m_Resource;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    LPDIRECT3DSURFACE9 m_pBackBuffer;
    LPDIRECT3DSURFACE9 m_pDepthBuffer;
    FLOAT m_fBackBufferWidth;
    FLOAT m_fBackBufferHeight;

    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;
    XMMATRIX m_matWorldViewT;
    XMMATRIX m_matProjT;

    LPDIRECT3DVERTEXDECLARATION9 m_pBlobBlenderVertexDecl;
    LPDIRECT3DVERTEXSHADER9 m_pBlobBlenderVS;
    LPDIRECT3DPIXELSHADER9 m_pBlobBlenderPS;

    LPDIRECT3DVERTEXDECLARATION9 m_pBlobLightVertexDecl;
    LPDIRECT3DVERTEXSHADER9 m_pBlobLightVS;
    LPDIRECT3DPIXELSHADER9 m_pBlobLightPS;

    LPDIRECT3DVERTEXBUFFER9 m_pBlobVB;               // Vertex buffer for blob billboards

    BLOBDATA        m_BlobPoints[NUM_BLOBS]; // Position, size, and color states

    LPDIRECT3DTEXTURE9 m_pBlobNormalTexture;    // Buffer textures for blending effect   
    LPDIRECT3DTEXTURE9 m_pBlobColorTexture;     // Buffer textures for blending effect   
    LPDIRECT3DSURFACE9 m_pBlobNormalTextureRT;  // Buffer textures for blending effect   
    LPDIRECT3DSURFACE9 m_pBlobColorTextureRT;   // Buffer textures for blending effect   

    LPDIRECT3DTEXTURE9 m_pGaussianTexture;      // Blob texture
    LPDIRECT3DCUBETEXTURE9 m_pEnvMap;               // Environment map   

    HRESULT         RenderFullScreenQuad();
    HRESULT         GenerateGaussianTexture( D3DFORMAT d3dBlobTextureFormat );

public:
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
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

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

    m_bDrawHelp = FALSE;

    // Keep track of the original render surfaces
    m_pd3dDevice->GetRenderTarget( 0, &m_pBackBuffer );
    m_pd3dDevice->GetDepthStencilSurface( &m_pDepthBuffer );
    m_fBackBufferWidth = ( FLOAT )m_d3dpp.BackBufferWidth;
    m_fBackBufferHeight = ( FLOAT )m_d3dpp.BackBufferHeight;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the textures resource
    if( FAILED( m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the environment map
    m_pEnvMap = m_Resource.GetCubemap( "EnvMap" );

    // Create the Gaussian distribution texture
    GenerateGaussianTexture( D3DFMT_LIN_R32F );

    // Set initial blob states
    for( DWORD i = 0; i < NUM_BLOBS; i++ )
    {
        m_BlobPoints[i].pos = XMFLOAT3( 0.0f, 0.0f, 0.0f );
        m_BlobPoints[i].size = 1.0f;
    }

    m_BlobPoints[0].color = XMFLOAT4( 0.3f, 0.0f, 0.0f, 1.0f );
    m_BlobPoints[1].color = XMFLOAT4( 0.0f, 0.3f, 0.0f, 1.0f );
    m_BlobPoints[2].color = XMFLOAT4( 0.0f, 0.0f, 0.3f, 1.0f );
    m_BlobPoints[3].color = XMFLOAT4( 0.3f, 0.3f, 0.0f, 1.0f );
    m_BlobPoints[4].color = XMFLOAT4( 0.0f, 0.3f, 0.3f, 1.0f );

    // Compile shaders
    {
        static const D3DVERTEXELEMENT9 decl[] =
        {
            { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };

        hr = m_pd3dDevice->CreateVertexDeclaration( decl, &m_pBlobBlenderVertexDecl );
        if( FAILED( hr ) )
            return hr;

        VOID* pCode;
        if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\BlobBlenderVS.xvu", &pCode ) ) )
            return hr;
        if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pBlobBlenderVS ) ) )
        {
            ATG::UnloadFile( pCode );
            return hr;
        }
        ATG::UnloadFile( pCode );
    }
    {
        VOID* pCode;
        if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\BlobBlenderPS.xpu", &pCode ) ) )
            return hr;
        if( FAILED( hr = m_pd3dDevice->CreatePixelShader( ( DWORD* )pCode, &m_pBlobBlenderPS ) ) )
        {
            ATG::UnloadFile( pCode );
            return hr;
        }
        ATG::UnloadFile( pCode );
    }
    {
        static const D3DVERTEXELEMENT9 decl[] =
        {
            { 0,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };

        hr = m_pd3dDevice->CreateVertexDeclaration( decl, &m_pBlobLightVertexDecl );
        if( FAILED( hr ) )
            return hr;

        VOID* pCode;
        if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\BlobLightVS.xvu", &pCode ) ) )
            return hr;
        if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pBlobLightVS ) ) )
        {
            ATG::UnloadFile( pCode );
            return hr;
        }
        ATG::UnloadFile( pCode );
    }
    {
        VOID* pCode;
        if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\BlobLightPS.xpu", &pCode ) ) )
            return hr;
        if( FAILED( hr = m_pd3dDevice->CreatePixelShader( ( DWORD* )pCode, &m_pBlobLightPS ) ) )
        {
            ATG::UnloadFile( pCode );
            return hr;
        }
        ATG::UnloadFile( pCode );
    }

    // Create the blob vertex buffer
    V_RETURN( m_pd3dDevice->CreateVertexBuffer( 4 * sizeof(BLOBVERTEX),
                                                    D3DUSAGE_WRITEONLY,
     0L, D3DPOOL_DEFAULT, &m_pBlobVB, NULL ) );
    {
        XMFLOAT2* pBlobVertex;
        m_pBlobVB->Lock( 0, 0, ( VOID** )&pBlobVertex, 0 );
        *pBlobVertex++ = XMFLOAT2( 0.0f, 0.0f );
        *pBlobVertex++ = XMFLOAT2( 1.0f, 0.0f );
        *pBlobVertex++ = XMFLOAT2( 1.0f, 1.0f );
        *pBlobVertex++ = XMFLOAT2( 0.0f, 1.0f );
        m_pBlobVB->Unlock();
    }

    // Create buffer textures 
    DWORD dwOffscreenWidth = 640;
    DWORD dwOffscreenHeight = 480;
    if( FAILED( m_pd3dDevice->CreateTexture( dwOffscreenWidth, dwOffscreenHeight, 1,
                                             D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F,
                                             D3DPOOL_DEFAULT, &m_pBlobNormalTexture, NULL ) ) )
    {
        ATG_PrintError( "Could not create texture\n" );
        return E_FAIL;
    }
    if( FAILED( m_pd3dDevice->CreateTexture( dwOffscreenWidth, dwOffscreenHeight, 1,
                                             D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F,
                                             D3DPOOL_DEFAULT, &m_pBlobColorTexture, NULL ) ) )
    {
        ATG_PrintError( "Could not create texture\n" );
        return E_FAIL;
    }

    D3DSURFACE_PARAMETERS Params;
    memset( &Params, 0, sizeof( D3DSURFACE_PARAMETERS ) );
    Params.Base = 0L;

    if( FAILED( m_pd3dDevice->CreateRenderTarget( dwOffscreenWidth, dwOffscreenHeight,
                                                  D3DFMT_A16B16G16R16F, D3DMULTISAMPLE_NONE, 0L, FALSE,
                                                  &m_pBlobNormalTextureRT, &Params ) ) )
    {
        ATG_PrintError( "Could not create render target\n" );
        return E_FAIL;
    }

    Params.Base = XGSurfaceSize( dwOffscreenWidth, dwOffscreenHeight,
                                 D3DFMT_A16B16G16R16F, D3DMULTISAMPLE_NONE );
    if( FAILED( m_pd3dDevice->CreateRenderTarget( dwOffscreenWidth, dwOffscreenHeight,
                                                  D3DFMT_A16B16G16R16F, D3DMULTISAMPLE_NONE, 0L, FALSE,
                                                  &m_pBlobColorTextureRT, &Params ) ) )
    {
        ATG_PrintError( "Could not create render target\n" );
        return E_FAIL;
    }

    // Set the camera parameters
    float fAspectRatio = m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -5.0f, 1.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 1.0f );
    m_matWorld = XMMatrixIdentity();
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );
    m_matProj = XMMatrixPerspectiveFovLH( FIELD_OF_VIEW, fAspectRatio, 1.0f, 100.0f );
    m_matWorldViewT = XMMatrixTranspose( m_matView );
    m_matProjT = XMMatrixTranspose( m_matProj );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: GenerateGaussianTexture()
// Desc: Generate a texture to store gaussian distribution results
//-----------------------------------------------------------------------------
HRESULT Sample::GenerateGaussianTexture( D3DFORMAT d3dBlobTextureFormat )
{
    HRESULT hr;

    // Create the gaussian texture
    if( FAILED( hr = m_pd3dDevice->CreateTexture( GAUSSIAN_TEXSIZE, GAUSSIAN_TEXSIZE, 1,
                                                  0, d3dBlobTextureFormat,
                                                  D3DPOOL_DEFAULT, &m_pGaussianTexture, NULL ) ) )
        return hr;

    // Fill in the gaussian texture data
    D3DLOCKED_RECT lock;
    m_pGaussianTexture->LockRect( 0, &lock, 0, 0 );

    for( DWORD v = 0; v < GAUSSIAN_TEXSIZE; v++ )
    {
        FLOAT* pBits32 = ( FLOAT* )( ( BYTE* )( lock.pBits ) + v * lock.Pitch );
        HALF* pBits16 = ( HALF* )( ( BYTE* )( lock.pBits ) + v * lock.Pitch );

        for( DWORD u = 0; u < GAUSSIAN_TEXSIZE; u++ )
        {
            FLOAT dx = 2.0f * u / ( FLOAT )GAUSSIAN_TEXSIZE - 1.0f;
            FLOAT dy = 2.0f * v / ( FLOAT )GAUSSIAN_TEXSIZE - 1.0f;
            FLOAT I = GAUSSIAN_HEIGHT * expf( -( dx * dx + dy * dy ) / GAUSSIAN_DEVIATION );

            // Write out the intensity
            if( d3dBlobTextureFormat == D3DFMT_LIN_R32F )
                *( pBits32++ ) = I;
            else
                *( pBits16++ ) = XMConvertFloatToHalf( I );
        }
    }

    m_pGaussianTexture->UnlockRect( 0 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get time
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();
    FLOAT fTime = ( FLOAT )m_Timer.GetAppTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Toggle motion
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        static BOOL bPaused = FALSE;
        bPaused = !bPaused;

        if( bPaused ) m_Timer.Stop();
        else
            m_Timer.Start();
    }

    // Perform object rotation
    if( pGamepad->fX1 || pGamepad->fY1 )
    {
        XMMATRIX matRotate;
        FLOAT fXRotate1 = pGamepad->fX1 * fElapsedTime * XM_PI * 0.5f;
        FLOAT fYRotate1 = pGamepad->fY1 * fElapsedTime * XM_PI * 0.5f;
        matRotate = XMMatrixRotationRollPitchYaw( -fXRotate1, -fYRotate1, 0.0f );
        m_matWorld = XMMatrixMultiply( m_matWorld, matRotate );

        XMMATRIX matWorldView;
        matWorldView = XMMatrixMultiply( m_matWorld, m_matView );
        m_matWorldViewT = XMMatrixTranspose( matWorldView );
    }

    // Animate the blobs
    FLOAT pos = 1.0f + cosf( 2 * XM_PI * fTime / 3.0f );
    m_BlobPoints[1].pos.x = +pos;
    m_BlobPoints[2].pos.x = -pos;
    m_BlobPoints[3].pos.y = +pos;
    m_BlobPoints[4].pos.y = -pos;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: RenderFullScreenQuad()
// Desc: Render a quad at the specified tranformed depth 
//-----------------------------------------------------------------------------
HRESULT Sample::RenderFullScreenQuad()
{
    FLOAT w = m_fBackBufferWidth;
    FLOAT h = m_fBackBufferHeight;

    FLOAT v[4][6] =
    {
        //  sx      sy     sz    rhw     tu    tv
        {  -0.5f,  -0.5f, 1.0f, 1.0f,   0.0f, 0.0f },
        { w - 0.5f,  -0.5f, 1.0f, 1.0f,   1.0f, 0.0f },
        { w - 0.5f, h - 0.5f, 1.0f, 1.0f,   1.0f, 1.0f },
        {  -0.5f, h - 0.5f, 1.0f, 1.0f,   0.0f, 1.0f },
    };

    // Since our vertices are already in screen space, we must
    // disable the viewport transformation when drawing the quad
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, v, sizeof( v[0] ) );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering. This 
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Turn off Z and Alpha
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
    m_pd3dDevice->SetDepthStencilSurface( NULL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // Clear temp textures
    m_pd3dDevice->SetRenderTarget( 0, m_pBlobNormalTextureRT );
    m_pd3dDevice->SetRenderTarget( 1, m_pBlobColorTextureRT );
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L );
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pBlobNormalTexture, NULL, 0, 0, NULL, 1.0f, 0L, NULL );
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET1, NULL, m_pBlobColorTexture, NULL, 0, 0, NULL, 1.0f, 0L, NULL );

    m_pd3dDevice->SetSamplerState( TEX_SourceBlob, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( TEX_SourceBlob, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( TEX_SourceBlob, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( TEX_SourceBlob, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( TEX_SourceBlob, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetSamplerState( TEX_NormalBuffer, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( TEX_NormalBuffer, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( TEX_NormalBuffer, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( TEX_NormalBuffer, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( TEX_NormalBuffer, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetSamplerState( TEX_ColorBuffer, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( TEX_ColorBuffer, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( TEX_ColorBuffer, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( TEX_ColorBuffer, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( TEX_ColorBuffer, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetSamplerState( TEX_EnvMap, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( TEX_EnvMap, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( TEX_EnvMap, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( TEX_EnvMap, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( TEX_EnvMap, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    // Render the blobs
    m_pd3dDevice->SetStreamSource( 0, m_pBlobVB, 0, sizeof( BLOBVERTEX ) );
    m_pd3dDevice->SetVertexDeclaration( m_pBlobBlenderVertexDecl );
    m_pd3dDevice->SetVertexShader( m_pBlobBlenderVS );
    m_pd3dDevice->SetPixelShader( m_pBlobBlenderPS );

    m_pd3dDevice->SetVertexShaderConstantF( 10, ( FLOAT* )&m_matWorldViewT, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 14, ( FLOAT* )&m_matProjT, 4 );

    for( int i = 0; i < NUM_BLOBS; ++i )
    {
        // Render the blob
        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m_BlobPoints[i], sizeof( m_BlobPoints[i] ) / sizeof
                                                ( XMFLOAT4 ) );
        m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&m_BlobPoints[i], sizeof( m_BlobPoints[i] ) / sizeof
                                               ( XMFLOAT4 ) );

        m_pd3dDevice->SetTexture( TEX_SourceBlob, m_pGaussianTexture );
        m_pd3dDevice->SetTexture( TEX_NormalBuffer, m_pBlobNormalTexture );
        m_pd3dDevice->SetTexture( TEX_ColorBuffer, m_pBlobColorTexture );
        m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );

        // Copy the render targets back to the input textures
        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pBlobNormalTexture, NULL, 0, 0, NULL, 1.0f, 0L,
                               NULL );
        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET1, NULL, m_pBlobColorTexture, NULL, 0, 0, NULL, 1.0f, 0L, NULL );
    }

    // Restore initial device surfaces
    m_pd3dDevice->SetRenderTarget( 0, m_pBackBuffer );
    m_pd3dDevice->SetRenderTarget( 1, NULL );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthBuffer );

    // Light and composite the blobs into the backbuffer
    m_pd3dDevice->SetVertexDeclaration( m_pBlobLightVertexDecl );
    m_pd3dDevice->SetVertexShader( m_pBlobLightVS );
    m_pd3dDevice->SetPixelShader( m_pBlobLightPS );
    m_pd3dDevice->SetTexture( TEX_NormalBuffer, m_pBlobNormalTexture );
    m_pd3dDevice->SetTexture( TEX_ColorBuffer, m_pBlobColorTexture );
    m_pd3dDevice->SetTexture( TEX_EnvMap, m_pEnvMap );
    RenderFullScreenQuad();

    // Restore state
    m_pd3dDevice->SetPixelShader( NULL );
    m_pd3dDevice->SetTexture( TEX_SourceBlob, NULL );
    m_pd3dDevice->SetTexture( TEX_NormalBuffer, NULL );
    m_pd3dDevice->SetTexture( TEX_ColorBuffer, NULL );
    m_pd3dDevice->SetTexture( TEX_EnvMap, NULL );

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
        m_Font.DrawText( 0, 0, 0xffffffff, L"Blobs" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Show the frame on the primary surface.
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


