//--------------------------------------------------------------------------------------
// GrayscaleBlur.cpp
//
// This sample demonstrates fast grayscale blur done by aliasing a 32bpp texture with
// an 8 bit texture
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xboxmath.h>
#include <xgraphics.h>

#include <assert.h>
#include <stdio.h>
#include <vector>


#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>

#include "texSize.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Next method" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_1, L"Previous method" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_1, L"Pause rotation" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))



// texture size for blurring
static const INT    TEX_SX = 1280;
static const INT    TEX_SY = 720;


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Derived class used to run the application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    enum BlurType
    {
        BLUR_NONE,
        BLUR_COLOR_9x9,
        BLUR_GRAY_9x9,
        BLUR_COLOR_5x5,
        BLUR_GRAY_5x5,
        BLUR_LAST
    };

    ATG::Timer  m_Timer;
    ATG::Font   m_Font;
    ATG::Help   m_Help;
    BOOL        m_bShowHelp;
    BlurType    m_blurType;
    BOOL        m_bRotation;
    FLOAT       m_fAnimationTime;

    D3DVertexBuffer*        m_pVB;
    D3DVertexDeclaration*   m_pVertexDecl;
    D3DVertexShader*        m_pVertexShader;
    D3DPixelShader*         m_pPixelShader;

    D3DPixelShader*         m_pPSHorz;
    D3DVertexShader*        m_pVSVert;
    D3DPixelShader*         m_pPSVert;
    D3DPixelShader*         m_pPSBlur9V;
    D3DPixelShader*         m_pPSBlur9H;
    D3DPixelShader*         m_pPSBlur5V;
    D3DPixelShader*         m_pPSBlur5H;
    D3DPixelShader*         m_pPSL8toABGR;
    D3DPixelShader*         m_pPSBlur5x5;
    D3DVertexShader*        m_pVSConsume;
    D3DPixelShader*         m_pPSConsume;
    D3DTexture              m_SharedTextureHeader;
    D3DTexture              m_R8ResolveTarget;
    D3DTexture*             m_p32ResolveTarget;
    D3DTexture              m_R8Aliased8888;
    D3DTexture              m_R8Aliased8888Enlarged;
    D3DTexture*             m_pRemap8To32;
    D3DTexture*             m_pRemap32To8;
    D3DSurface*             m_pBlurTempRT;
    D3DSurface*             m_pR8Aliased8888EnlargedRT;

    D3DTexture* GetTextureHeader( D3DTexture& templateTexture )
    {
        m_SharedTextureHeader.Format = templateTexture.Format;
        return &m_SharedTextureHeader;
    }

    XMMATRIX m_matWorld;
    XMMATRIX m_matProj;
    XMMATRIX m_matView;

    //-------------------------------------------------------------------------------------
    // Structure to hold vertex data.
    //-------------------------------------------------------------------------------------
    struct COLORVERTEX
    {
        FLOAT       Position[3];
        DWORD       Color;
    };

private:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
    void RenderUI();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferWidth = TEX_SX;
    atgApp.m_d3dpp.BackBufferHeight = TEX_SY;
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//-------------------------------------------------------------------------------------
// Name: InitScene()
// Desc: 
//-------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    // Create the font
    if( FAILED( hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return hr;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create help screen
    if( FAILED( hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return hr;

    if( FAILED( hr = ATG::LoadVertexShader( "GAME:\\Media\\Shaders\\vs.xvu", &m_pVertexShader ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadPixelShader( "GAME:\\Media\\Shaders\\ps.xpu", &m_pPixelShader ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadPixelShader( "GAME:\\Media\\Shaders\\psHorz.xpu", &m_pPSHorz ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadVertexShader( "GAME:\\Media\\Shaders\\vsVert.xvu", &m_pVSVert ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadPixelShader( "GAME:\\Media\\Shaders\\psVert.xpu", &m_pPSVert ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadVertexShader( "GAME:\\Media\\Shaders\\vsConsume.xvu", &m_pVSConsume ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadPixelShader( "GAME:\\Media\\Shaders\\psConsume.xpu", &m_pPSConsume ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadPixelShader( "GAME:\\Media\\Shaders\\psBlur9V.xpu", &m_pPSBlur9V ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadPixelShader( "GAME:\\Media\\Shaders\\psBlur9H.xpu", &m_pPSBlur9H ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadPixelShader( "GAME:\\Media\\Shaders\\psBlur5V.xpu", &m_pPSBlur5V ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadPixelShader( "GAME:\\Media\\Shaders\\psBlur5H.xpu", &m_pPSBlur5H ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadPixelShader( "GAME:\\Media\\Shaders\\psL8toABGR.xpu", &m_pPSL8toABGR ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadPixelShader( "GAME:\\Media\\Shaders\\psBlur5x5.xpu", &m_pPSBlur5x5 ) ) )
        return hr;

    m_bShowHelp = FALSE;
    m_bRotation = TRUE;

    m_fAnimationTime = 0;

    // create the vertex declaration
    static const D3DVERTEXELEMENT9 VertexElements[3] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
        D3DDECL_END()
    };
    m_pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pVertexDecl );


    // create the vertex buffer
    if( FAILED( m_pd3dDevice->CreateVertexBuffer( 3*sizeof(COLORVERTEX),
                                                  D3DUSAGE_WRITEONLY, 
                                                  NULL,
                                                  D3DPOOL_MANAGED, 
                                                  &m_pVB, 
                                                  NULL ) ) )
    {
        return E_FAIL;
    }

    static const COLORVERTEX vertices[] =
    {
        {  -1,  -1, 0.0f, 0xffff0000 }, // x, y, z, color
        {   1,  -1, 0.0f, 0xff00ff00 },
        {   1,   1, 0.0f, 0xffffff00 },
    };

    COLORVERTEX* pVertices;
    if( FAILED( m_pVB->Lock( 0, 0, (void**)&pVertices, 0 ) ) )
        return E_FAIL;
    memcpy( pVertices, vertices, 3*sizeof(COLORVERTEX) );
    m_pVB->Unlock();

    // Initialize the world matrix
    m_matWorld = XMMatrixIdentity();

    // Initialize the projection matrix
    BOOL bWidescreen;
    ATG::GetVideoSettings( NULL, NULL, &bWidescreen );
    FLOAT fAspect = ( bWidescreen ) ? (16.0f / 9.0f) : (4.0f / 3.0f); 
    m_matProj = XMMatrixPerspectiveFovLH( XM_PIDIV4, fAspect, 1.0f, 200.0f );

    // Initialize the view matrix
    XMVECTOR vEyePt    = { 0.0f, 0.0f,-1.9f, 0.0f };
    XMVECTOR vLookatPt = { 0.0f, 0.0f, 0.0f, 0.0f };
    XMVECTOR vUp       = { 0.0f, 1.0f, 0.0f, 0.0f };
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );

    // this is the resolve target
    m_pd3dDevice->CreateTexture( TEX_SX, TEX_SY, 1, 0, D3DFMT_A8R8G8B8, 0, &m_p32ResolveTarget, 0 );
    XGSetSamplerAddressStates( m_p32ResolveTarget, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );
    XGSetSamplerFilterStates( m_p32ResolveTarget, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_NONE, 1 );

    // create the resolve target and its 32bpp read out aliases
    // the resolve target is going to be used to alias an 8 bit texture so its pitch must
    // match that of the 8 bit texture
    // we also want to make sure that the texture can accomodate a possibly slightly taller resolve target
    // so we need to make sure we allocate the right amount of memory
    // this memory is going to be aliased by the following textures
    // 1. regular resolve target to receive the L8 data from EDRAM
    // 2. 32bit texture aliased on top of the L8 data, point- and bilinearly sampled
    // 3. 32bit texture aliased on top of the L8 data with height set to be a multiple of 16, point sampling
    {
        const UINT uSYRoundedToTile = XGNextMultiple( TEX_SY, VERT_TILE_REPEAT );
        const UINT uSXRoundedToTile = XGNextMultiple( TEX_SX, HORZ_32_TO_8_TILE_REPEAT );
        const UINT uPitch32bpp = XGNextMultiple( uSXRoundedToTile / 4, GPU_TEXTURE_TILE_DIMENSION );

        // allocate enough physical memory
        UINT    uBaseSize;
        XGSetTextureHeader( uPitch32bpp * 4, uSYRoundedToTile, 1, 0, D3DFMT_L8, 0, 0, 0, uPitch32bpp * 4, NULL, &uBaseSize, NULL );
        void* pBuffer = XPhysicalAlloc( uBaseSize, MAXULONG_PTR, 0, PAGE_READWRITE | PAGE_WRITECOMBINE );

        // set the texture headers now

        // this is the texture to take the L8 resolve
        XGSetTextureHeader( TEX_SX, TEX_SY, 1, 0, D3DFMT_L8, 0, 0, 0, uPitch32bpp * 4, &m_R8ResolveTarget, NULL, NULL );
        XGOffsetResourceAddress( &m_R8ResolveTarget, pBuffer );
        XGSetSamplerAddressStates( &m_R8ResolveTarget, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );
        XGSetSamplerFilterStates( &m_R8ResolveTarget, D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_NONE, 1 );

        // this is the 32bpp alias for taking the results after the 1st pass
        XGSetTextureHeader( TEX_SX / 4, TEX_SY, 1, 0, D3DFMT_A8R8G8B8, 0, 0, 0, uPitch32bpp * 4, &m_R8Aliased8888, NULL, NULL );
        XGOffsetResourceAddress( &m_R8Aliased8888, pBuffer );
        XGSetSamplerAddressStates( &m_R8Aliased8888, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );
        XGSetSamplerFilterStates( &m_R8Aliased8888, D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_NONE, 1 );
    
        // this is the enlarged 32bpp alias for taking the final results after the 2nd pass
        // make sure that the enlarged 32bpp encompasses the last tile in full
        // the enlarged texture is used as a resolve target for the pass where we output pixels based on a swizzling pattern
        // which has a vertical repeat of 16 pixels. if a shorter texture is used, some pixels will be lost
        XGSetTextureHeader( uSXRoundedToTile / 4, uSYRoundedToTile, 1, 0, D3DFMT_A8R8G8B8, 0, 0, 0, uPitch32bpp * 4, &m_R8Aliased8888Enlarged, NULL, NULL );
        XGOffsetResourceAddress( &m_R8Aliased8888Enlarged, pBuffer );
        XGSetSamplerAddressStates( &m_R8Aliased8888Enlarged, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );
        XGSetSamplerFilterStates( &m_R8Aliased8888Enlarged, D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_NONE, 1 );

        // temp RT for a blur to render the first pass and the second pass
        D3DSURFACE_PARAMETERS   p = { 0 };
        m_pd3dDevice->CreateRenderTarget( TEX_SX / 4, TEX_SY, D3DFMT_A8B8G8R8, D3DMULTISAMPLE_NONE, 0, FALSE, &m_pBlurTempRT, &p );
        m_pd3dDevice->CreateRenderTarget( uPitch32bpp, uSYRoundedToTile, D3DFMT_A8B8G8R8, D3DMULTISAMPLE_NONE, 0, FALSE, &m_pR8Aliased8888EnlargedRT, &p );

        // we will use a single texture header instead of several because it lets D3D correctly track the resource usage
        // although there should be no problem in practice to use several headers to reference the same memory (especially when you're only
        // performing Resolve and tfetch to and from that memory) D3D doesn't formally guarantee correct ordering of resource access
        // this will most certainly become a problem if you start involving the CPU (Lock/Unlock)
        // to prevent any such issues, we use a single texture header, but change it's Format field to make sure we reinterpret the
        // underlying memory correctly. we recommend you do the same
        m_SharedTextureHeader = m_R8ResolveTarget;
    }

    // these should match up
    XGTEXTURE_DESC desc1, desc2;
    XGGetTextureDesc( &m_R8ResolveTarget, 0, &desc1 );
    XGGetTextureDesc( &m_R8Aliased8888, 0, &desc2 );
    assert( desc1.RowPitch == desc2.RowPitch );

    // generate 8 to 32 and 32 to 8 remap look up textures

    // use NumFormat=INTEGER so we read out [0..255] directly -- that saves us ALU in the shader
    static const D3DFORMAT D3DFMT_R8G8_BYTE = (D3DFORMAT)MAKED3DFMT(    GPUTEXTUREFORMAT_8_8,
                                                                        GPUENDIAN_8IN16,
                                                                        TRUE,
                                                                        GPUSIGN_ALL_UNSIGNED,
                                                                        GPUNUMFORMAT_INTEGER,
                                                                        GPUSWIZZLE_GRRR );

    {
        m_pd3dDevice->CreateTexture( HORZ_8_TO_32_TILE_REPEAT, VERT_TILE_REPEAT, 1, 0, D3DFMT_R8G8_BYTE, 0, &m_pRemap8To32, NULL );
        XGSetSamplerAddressStates( m_pRemap8To32, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );

        D3DLOCKED_RECT rc;
        m_pRemap8To32->LockRect( 0, &rc, NULL, 0 );
        for( UINT i=0; i < VERT_TILE_REPEAT; ++i )
        {
            for( UINT j=0; j < HORZ_8_TO_32_TILE_REPEAT; ++j )
            {
                // i and j here is 2d coordinate of an 8 bit tiled texture with each stepX = 4 pixels
                const UINT  offset8 = XGAddress2DTiledOffset( j * 4, i, HORZ_8_TO_32_TILE_REPEAT * 4, 1 );

                // from the 8 bit surface offset we go into 32bpp tiled x/y coordinates
                const UINT  xx32 = XGAddress2DTiledX( offset8 / 4, HORZ_8_TO_32_TILE_REPEAT, 4 );
                const UINT  yy32 = XGAddress2DTiledY( offset8 / 4, HORZ_8_TO_32_TILE_REPEAT, 4 );

                // find where to store in the tiled LUT
                const UINT destOffset = XGAddress2DTiledOffset( j, i, HORZ_8_TO_32_TILE_REPEAT, 2 );

                // store the 32bpp tiled coordinates
                ((BYTE*)rc.pBits)[ destOffset * 2 + 0 ] = static_cast< BYTE >( xx32 );
                ((BYTE*)rc.pBits)[ destOffset * 2 + 1 ] = static_cast< BYTE >( yy32 );
            }
        }
        m_pRemap8To32->UnlockRect( 0 );
    }

    {
        m_pd3dDevice->CreateTexture( HORZ_32_TO_8_TILE_REPEAT, VERT_TILE_REPEAT, 1, 0, D3DFMT_R8G8_BYTE, 0, &m_pRemap32To8, NULL );
        XGSetSamplerAddressStates( m_pRemap32To8, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );

        D3DLOCKED_RECT rc;
        m_pRemap32To8->LockRect( 0, &rc, NULL, 0 );
        for( UINT i=0; i < VERT_TILE_REPEAT; ++i )
        {
            for( UINT j=0; j < HORZ_32_TO_8_TILE_REPEAT; ++j )
            {
                const UINT  offset32 = XGAddress2DTiledOffset( j, i, HORZ_32_TO_8_TILE_REPEAT, 4 );

                // from 8 bit surface offset we go into 32bpp tiled
                const UINT  xx32 = XGAddress2DTiledX( offset32 * 4, HORZ_32_TO_8_TILE_REPEAT * 4, 1 );
                const UINT  yy32 = XGAddress2DTiledY( offset32 * 4, HORZ_32_TO_8_TILE_REPEAT * 4, 1 );

                // find where to store in the tiled LUT
                const UINT destOffset = XGAddress2DTiledOffset( j, i, HORZ_32_TO_8_TILE_REPEAT, 2 );

                // store the 32bpp tiled coordinates
                ((BYTE*)rc.pBits)[ destOffset * 2 + 0 ] = static_cast< BYTE >( xx32 / 4 );
                ((BYTE*)rc.pBits)[ destOffset * 2 + 1 ] = static_cast< BYTE >( yy32 );
            }
        }
        m_pRemap32To8->UnlockRect( 0 );
    }

    // 
    m_blurType = BLUR_NONE;

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: Update()
// Desc: Updates the world for the next frame
//-------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Update FPS in the timer class.
    m_Timer.MarkFrame();

    static const XMVECTOR vAxisZ = { 0, 0, 1.0f, 0 };

    const FLOAT fDelta = static_cast< FLOAT >( m_Timer.GetElapsedTime() );

    // Set the world matrix
    if( m_bRotation )
    {
        m_fAnimationTime += fDelta;
        const FLOAT fAngle = fmodf( -m_fAnimationTime * 0.1f, XM_2PI );
        m_matWorld = XMMatrixRotationAxis( vAxisZ, fAngle );
    }

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bShowHelp = !m_bShowHelp;

    // next mode
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_blurType = static_cast< BlurType >( static_cast< INT >( m_blurType ) + 1 );
        if( m_blurType >= BLUR_LAST )
            m_blurType = BLUR_NONE;
    }

    // next mode
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        INT iMethod = m_blurType - 1;

        if( iMethod < 0 )
            iMethod = BLUR_LAST - 1;

        m_blurType = static_cast< BlurType >( iMethod );
    }

    // rotation
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        m_bRotation = !m_bRotation;

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the scene
//-------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Clear the backbuffer to a blue color
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, D3DCOLOR_XRGB(0,0,255), 1.0f, 0L );

    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    m_pd3dDevice->SetStreamSource( 0, m_pVB, 0, sizeof(COLORVERTEX) );
    m_pd3dDevice->SetVertexShader( m_pVertexShader );
    m_pd3dDevice->SetPixelShader( m_pPixelShader );
   
    // Build the world-view-projection matrix and pass it into the vertex shader
    XMMATRIX matWVP = m_matWorld * m_matView * m_matProj;
    m_pd3dDevice->SetVertexShaderConstantF( 0, (FLOAT*)&matWVP, 4 );

    // Draw the vertices in the vertex buffer
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, 1 );

    if( m_blurType != BLUR_NONE )
    {
        // one channel blur or a classical 4 channel blur
        const BOOL bGray = (m_blurType == BLUR_GRAY_9x9) || (m_blurType == BLUR_GRAY_5x5);

        IDirect3DSurface9* pRT;
        m_pd3dDevice->GetRenderTarget( 0, &pRT );

        // resolve into L8 if doing grayscale blur. in case of the 8 bit resolve here we just take one channel of 4, you may
        // want to do something more clever
        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, bGray ? GetTextureHeader( m_R8ResolveTarget ) : m_p32ResolveTarget, NULL, 0, 0, NULL, 0, 0, NULL );

        // set shared shader constants
        const FLOAT   texSizes0[ 4 ] =
        {
            TEX_SX / 4,
            TEX_SY,
            TEX_SY + ((VERT_TILE_REPEAT - (TEX_SY % VERT_TILE_REPEAT)) % VERT_TILE_REPEAT),       // align to VERT_TILE_REPEAT
            (TEX_SX + ((HORZ_32_TO_8_TILE_REPEAT - (TEX_SX % HORZ_32_TO_8_TILE_REPEAT)) % HORZ_32_TO_8_TILE_REPEAT)) / 4,       // align to HORZ_32_TO_8_TILE_REPEAT
        };

        const FLOAT   texSizes1[ 4 ] =
        {
            TEX_SX,
            TEX_SY,
            texSizes0[ 0 ] / texSizes0[ 3 ],  // RT_SX / RT_SX_A
            0
        };

        m_pd3dDevice->SetVertexShaderConstantF( REG_TEX_SIZES_0, texSizes0, 1 );
        m_pd3dDevice->SetPixelShaderConstantF(  REG_TEX_SIZES_0, texSizes0, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( REG_TEX_SIZES_1, texSizes1, 1 );
        m_pd3dDevice->SetPixelShaderConstantF(  REG_TEX_SIZES_1, texSizes1, 1 );

        if( m_blurType == BLUR_GRAY_9x9 )
        {
            PIXBeginNamedEvent( 0, "Fullscreen grayscale 9x9 blur" );

            // alias L8 as 8888
            m_pd3dDevice->SetTexture( 0, GetTextureHeader( m_R8Aliased8888Enlarged ) );
            m_pd3dDevice->SetSamplerFilterStates( 0, D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_NONE, 1 );
            m_pd3dDevice->SetSamplerAddressStates( 0, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );
            m_pd3dDevice->SetTextureFetchConstant( 1, m_pRemap8To32 );
            m_pd3dDevice->SetShaderGPRAllocation( 0, 16, 112 );
            m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, FALSE );

            // consume aliased surface remapping accordingly and blurring horizontally
            m_pd3dDevice->SetRenderTarget( 0, m_pBlurTempRT );
            m_pd3dDevice->SetVertexShader( m_pVSVert );
            m_pd3dDevice->SetPixelShader( m_pPSHorz );
            m_pd3dDevice->DrawPrimitive( D3DPT_RECTLIST, 0, 1 );
            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, GetTextureHeader( m_R8Aliased8888 ), NULL, 0, 0, NULL, 0, 0, NULL );

            // using this scratch surface now as source -- it's blurred horizontally
            m_pd3dDevice->SetTexture( 0, GetTextureHeader( m_R8Aliased8888 ) );
            m_pd3dDevice->SetSamplerFilterStates( 0, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_NONE, 1 );
            m_pd3dDevice->SetSamplerAddressStates( 0, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );

            // blur vertically, remapping to 8 bit output
            m_pd3dDevice->SetRenderTarget( 0, m_pR8Aliased8888EnlargedRT );
            m_pd3dDevice->SetTextureFetchConstant( 1, m_pRemap32To8 );
            m_pd3dDevice->SetVertexShader( m_pVSVert );
            m_pd3dDevice->SetPixelShader( m_pPSVert );
            m_pd3dDevice->DrawPrimitive( D3DPT_RECTLIST, 0, 1 );
            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, GetTextureHeader( m_R8Aliased8888Enlarged ), NULL, 0, 0, NULL, 0, 0, NULL );

            // unset the aliased texture
            m_pd3dDevice->SetTexture( 0, NULL );
            m_pd3dDevice->SetTexture( 1, NULL );
            m_pd3dDevice->SetShaderGPRAllocation( 0, 0, 0 );

            PIXEndNamedEvent();
        } else if( m_blurType == BLUR_GRAY_5x5 )
        {
            PIXBeginNamedEvent( 0, "Fullscreen grayscale 5x5 blur" );

            // alias as 8888
            m_pd3dDevice->SetTexture( 0, GetTextureHeader( m_R8Aliased8888Enlarged ) );
            m_pd3dDevice->SetSamplerFilterStates( 0, D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_NONE, 1 );
            m_pd3dDevice->SetSamplerAddressStates( 0, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );
            m_pd3dDevice->SetTextureFetchConstant( 1, m_pRemap8To32 );
            m_pd3dDevice->SetShaderGPRAllocation( 0, 16, 112 );
            m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, FALSE );

            // consume aliased surface and re-tile
            // we expect this pass to happen during an earlier stage, when this surface is generated
            m_pd3dDevice->SetRenderTarget( 0, m_pBlurTempRT );
            m_pd3dDevice->SetVertexShader( m_pVSVert );
            m_pd3dDevice->SetPixelShader( m_pPSL8toABGR );
            m_pd3dDevice->DrawPrimitive( D3DPT_RECTLIST, 0, 1 );
            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, GetTextureHeader( m_R8Aliased8888 ), NULL, 0, 0, NULL, 0, 0, NULL );

            // using this scratch surface now as source -- it's blurred horizontally
            m_pd3dDevice->SetTextureFetchConstant( 0, GetTextureHeader( m_R8Aliased8888 ) );
            m_pd3dDevice->SetSamplerFilterStates( 0, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_NONE, 1 );
            m_pd3dDevice->SetSamplerAddressStates( 0, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );

            // blur 5x5 in one pass
            m_pd3dDevice->SetRenderTarget( 0, m_pR8Aliased8888EnlargedRT );
            m_pd3dDevice->SetTextureFetchConstant( 1, m_pRemap32To8 );
            m_pd3dDevice->SetVertexShader( m_pVSVert );
            m_pd3dDevice->SetPixelShader( m_pPSBlur5x5 );
            m_pd3dDevice->DrawPrimitive( D3DPT_RECTLIST, 0, 1 );
            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, GetTextureHeader( m_R8Aliased8888Enlarged ), NULL, 0, 0, NULL, 0, 0, NULL );

            // unset the aliased texture
            m_pd3dDevice->SetTexture( 0, NULL );
            m_pd3dDevice->SetTexture( 1, NULL );
            m_pd3dDevice->SetShaderGPRAllocation( 0, 0, 0 );

            PIXEndNamedEvent();
        } else if( m_blurType == BLUR_COLOR_9x9 || m_blurType == BLUR_COLOR_5x5 )
        {
            PIXBeginNamedEvent( 0, "Fullscreen colour blur" );

            // read directly as L8
            m_pd3dDevice->SetTextureFetchConstant( 0, m_p32ResolveTarget );
            m_pd3dDevice->SetShaderGPRAllocation( 0, 16, 112 );
            m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, FALSE );

            // blur vertically
            m_pd3dDevice->SetVertexShader( m_pVSVert );
            m_pd3dDevice->SetPixelShader( (m_blurType == BLUR_COLOR_9x9) ? m_pPSBlur9H : m_pPSBlur5H );
            m_pd3dDevice->DrawPrimitive( D3DPT_RECTLIST, 0, 1 );
            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_p32ResolveTarget, NULL, 0, 0, NULL, 0, 0, NULL );

            // blur horizontally
            m_pd3dDevice->SetVertexShader( m_pVSVert );
            m_pd3dDevice->SetPixelShader( (m_blurType == BLUR_COLOR_9x9) ? m_pPSBlur9V : m_pPSBlur5V );
            m_pd3dDevice->DrawPrimitive( D3DPT_RECTLIST, 0, 1 );
            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_p32ResolveTarget, NULL, 0, 0, NULL, 0, 0, NULL );

            // unset the aliased texture
            m_pd3dDevice->SetTexture( 0, NULL );
            m_pd3dDevice->SetTexture( 1, NULL );
            m_pd3dDevice->SetShaderGPRAllocation( 0, 0, 0 );
            
            PIXEndNamedEvent();
        }

        // simply consume blurred as L8
        m_pd3dDevice->SetTexture( 0, bGray ? GetTextureHeader( m_R8ResolveTarget ) : m_p32ResolveTarget );
        m_pd3dDevice->SetSamplerFilterStates( 0, D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_POINT, 1 );
        m_pd3dDevice->SetSamplerAddressStates( 0, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );
        m_pd3dDevice->SetRenderTarget( 0, pRT );
        m_pd3dDevice->SetVertexShader( m_pVSConsume );
        m_pd3dDevice->SetPixelShader( m_pPSConsume );
        m_pd3dDevice->SetStreamSource( 0, m_pVB, 0, sizeof(COLORVERTEX) );
        m_pd3dDevice->DrawPrimitive( D3DPT_RECTLIST, 0, 1 );
    }

    // render information
    RenderUI();

    // Present the backbuffer contents to the display
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    m_pd3dDevice->UnsetAll();

    return S_OK;
}


void Sample::RenderUI()
{
    if( m_bShowHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    } else
    {
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffff00, L"Grayscale Blur" );
        m_Font.SetScaleFactors( 1.f, 1.f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        static const WCHAR* pBlurTypeNames[] =
        {
            L"Original image",
            L"9x9 32bpp grayscale blur",
            L"9x9 8bpp grayscale blur",
            L"5x5 32bpp grayscale blur",
            L"5x5 8bpp grayscale blur",
        };

        m_Font.DrawText( 0, 30, 0xffffffff, pBlurTypeNames[ m_blurType ] );

        m_Font.End();
    }
}


