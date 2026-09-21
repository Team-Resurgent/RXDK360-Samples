//--------------------------------------------------------------------------------------
// FastStencilFill.cpp
//
// This sample demonstrates how to write directly to stencil buffer bits in EDRAM, using
// a special color rendertarget aliased on the depth/stencil surface and specially 
// crafted screenspace geometry.  The special geometry is tile-aligned vertical
// rectangles, which combined together draw the desired pattern in the stencil buffer.
// The reason for these aligned rects is that depth/stencil buffers use EDRAM 
// differently than a color buffer - there is a half-tile swap done on each EDRAM tile 
// when EDRAM is used for depth/stencil data.
// 
// For example, here is how an 80x16 EDRAM tile is addressed as a color buffer:

//    0                                                              79 pixels
// 0  +---------------------------------------------------------------+
//    |                                                               |
//    |                                                               |
//    |                                                               |
// 15 +---------------------------------------------------------------+

// Here is how the same 80x16 EDRAM tile is addressed as a depth/stencil buffer:

//    40                             79 0                            39 pixels
// 0  +--------------------------------+------------------------------+
//    |                                |                              |
//    |                                |                              |
//    |                                |                              |
// 15 +--------------------------------+------------------------------+

// With this in mind, the geometry created in this sample places color samples in the 
// swapped orientation, so that when they are interpreted as depth/stencil samples, the
// order of the pixels is correct.

// Note that if your desired pattern is a repeating pattern, and the width of the
// repeat is less than 40 pixels and is an even divisor into 40 pixels, you do not
// need to build this special geometry, since the half-tile swap will not matter.

//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgApp.h>

// Declare the write barrier intrinsic, which creates a write instruction reordering barrier for the compiler.
extern "C"
    void _WriteBarrier();
#pragma intrinsic(_WriteBarrier)

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
    { ATG::HELP_LEFT_STICK, ATG::HELP_PLACEMENT_2, L"Move test\nrectangle"  },
    { ATG::HELP_A_BUTTON,   ATG::HELP_PLACEMENT_2, L"Cycle stencil\npattern"  },
    { ATG::HELP_Y_BUTTON,   ATG::HELP_PLACEMENT_2, L"Show stencil\nrendering"  },
};
const DWORD     NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );


// Rectangle structure using float coordinates.
struct RECTANGLE
{
    FLOAT x1;
    FLOAT y1;
    FLOAT x2;
    FLOAT y2;

    FLOAT   Width() const
    {
        return x2 - x1;
    }
    FLOAT   Height() const
    {
        return y2 - y1;
    }
};


// Mesh vertex structure for use in screen quads.
struct MeshVertexPT
{
    XMFLOAT3 vPosition;
    XMFLOAT2 vTexCoord;
};

// Helper class for PIX markers - automatically ends the event when it falls out of scope.
class PIXNamedEventHelperClass
{
public:
PIXNamedEventHelperClass( const CHAR* strTitle, DWORD dwColor = 0xFFFFFFFF )
{
    PIXBeginNamedEvent( dwColor, strTitle );
}
~PIXNamedEventHelperClass()
{
    PIXEndNamedEvent();
}
};
#define PIXNamedEvent(Title) PIXNamedEventHelperClass PIXNamedEvent##__LINE__(Title)

// Enumeration and names for the stencil patterns.
enum STENCILPATTERNTYPE
{
    SPT_ORDEREDDITHER = 0,
    SPT_TEXT,
    SPT_MAX
};
const WCHAR*    g_strStencilPatternNames[] =
{
    L"8x8 Ordered Dither Texture",
    L"512x512 Texture"
};
// Use a compiler assert to make sure the list of name strings matches the enum.
C_ASSERT( ARRAYSIZE( g_strStencilPatternNames ) == SPT_MAX );

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: The FastStencilFill sample class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{

public:

    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();

private:

    VOID    RenderStencilPatterns();
    VOID    RenderStencilTestGeometry();
    VOID    RenderUI();

    VOID    DrawStencilFillGeometry( const RECTANGLE& ScreenRect, const RECTANGLE& TextureRect );
    VOID    DrawFilledRect( const RECTANGLE& ScreenRect, const RECTANGLE& UVRect, D3DTexture* pTexture,
                            D3DCOLOR Color = 0 );

private:

    ATG::Font m_Font;
    ATG::Timer m_Timer;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    ATG::PackedResource m_Resources;

    D3DSurface* m_pRenderTarget;
    D3DSurface* m_pDepthStencil;
    D3DSurface* m_pStencilFillTarget;

    D3DTexture* m_pDitherTexture;
    D3DTexture* m_pBitMaskTexture;

    D3DPixelShader* m_pSolidPS;
    D3DPixelShader* m_pTexturedPS;
    D3DVertexShader* m_pPassthruVS;
    D3DVertexDeclaration* m_pVertexDecl;

    RECTANGLE m_TestRect;
    FLOAT m_fTime;
    STENCILPATTERNTYPE m_PatternType;
    BOOL m_bShowStencilRendering;
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample App;

    ATG::GetVideoSettings( &App.m_d3dpp.BackBufferWidth, &App.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    App.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    App.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    App.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes data and content for the sample.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;
    m_fTime = 0;
    m_PatternType = SPT_ORDEREDDITHER;
    m_bShowStencilRendering = FALSE;

    // Center test rectangle on the screen.
    const FLOAT fRectHalfSize = 100.0f;
    m_TestRect.x1 = ( FLOAT )( m_d3dpp.BackBufferWidth / 2 ) - fRectHalfSize;
    m_TestRect.x2 = m_TestRect.x1 + ( fRectHalfSize * 2 );
    m_TestRect.y1 = ( FLOAT )( m_d3dpp.BackBufferHeight / 2 ) - fRectHalfSize;
    m_TestRect.y2 = m_TestRect.y1 + ( fRectHalfSize * 2 );

    // Set default renderstate.
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    // Load packed resources.
    if( FAILED( m_Resources.Create( "game:\\Media\\Resource.xpr" ) ) )
        ATG::FatalError( "Could not load resources." );
    m_pDitherTexture = m_Resources.GetTexture( "OrderedDither8x8" );
    m_pBitMaskTexture = m_Resources.GetTexture( "BitPattern" );
    if( m_pDitherTexture == NULL || m_pBitMaskTexture == NULL )
        ATG::FatalError( "Could not find textures in resource file." );

    // Create font.
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        ATG::FatalError( "Could not load font." );

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create help screen.
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        ATG::FatalError( "Could not load help resources." );

    // Get the color rendertarget and depth/stencil surface.
    m_pd3dDevice->GetRenderTarget( 0, &m_pRenderTarget );
    m_pd3dDevice->GetDepthStencilSurface( &m_pDepthStencil );

    // Extract the EDRAM offset of the depth/stencil surface.
    D3DSURFACE_PARAMETERS SurfParams = { 0 };
    SurfParams.Base = m_pDepthStencil->DepthInfo.DepthBase;

    // Create an A8R8G8B8 rendertarget that overlaps the depth/stencil surface.
    if( FAILED( m_pd3dDevice->CreateRenderTarget( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                                  D3DFMT_A8R8G8B8,
                                                  D3DMULTISAMPLE_NONE,
                                                  0, FALSE,
                                                  &m_pStencilFillTarget,
                                                  &SurfParams ) ) )
    {
        ATG::FatalError( "Could not create aliased rendertarget." );
    }

    // Load vertex shader.
    if( FAILED( ATG::LoadVertexShader( "game:\\media\\shaders\\PassthruVS.xvu", &m_pPassthruVS ) ) )
        ATG::FatalError( "Could not load vertex shader." );

    // Load pixel shader.
    if( FAILED( ATG::LoadPixelShader( "game:\\media\\shaders\\SolidPS.xpu", &m_pSolidPS ) ) )
        ATG::FatalError( "Could not load pixel shader." );

    // Load pixel shader.
    if( FAILED( ATG::LoadPixelShader( "game:\\media\\shaders\\TexturedPS.xpu", &m_pTexturedPS ) ) )
        ATG::FatalError( "Could not load pixel shader." );

    // Create a vertex declaration.
    static const D3DVERTEXELEMENT9 DeclElements[] =
    {
        { 0,     0, D3DDECLTYPE_FLOAT3,     0,  D3DDECLUSAGE_POSITION,  0 },
        { 0,    12, D3DDECLTYPE_FLOAT2,     0,  D3DDECLUSAGE_TEXCOORD,  0 },
        D3DDECL_END()
    };
    m_pd3dDevice->CreateVertexDeclaration( DeclElements, &m_pVertexDecl );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Updates the timer and samples controller input.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Update FPS in the timer class.
    m_Timer.MarkFrame();

    FLOAT fDeltaTime = ( FLOAT )m_Timer.GetElapsedTime();
    m_fTime += fDeltaTime;

    // Get input from controllers (check for exit button sequence).
    ATG::GAMEPAD* __restrict pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // The A button cycles the stencil pattern.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_PatternType = ( STENCILPATTERNTYPE )( ( m_PatternType + 1 ) % SPT_MAX );
    }

    // The Y button shows the pixels that we're drawing to the stencil buffer.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        m_bShowStencilRendering = !m_bShowStencilRendering;

    // Move test rectangle based on left stick input.
    const FLOAT fSpeed = 500.0f * fDeltaTime;   // 500 pixels per second
    FLOAT fXOffset = pGamepad->fX1 * fSpeed;
    FLOAT fYOffset = pGamepad->fY1 * -fSpeed;

    FLOAT fWidth = m_TestRect.Width();
    FLOAT fHeight = m_TestRect.Height();

    m_TestRect.x1 += fXOffset;
    m_TestRect.y1 += fYOffset;
    m_TestRect.x1 = max( m_TestRect.x1, 0 );
    m_TestRect.y1 = max( m_TestRect.y1, 0 );
    m_TestRect.x1 = min( m_TestRect.x1, ( FLOAT )m_d3dpp.BackBufferWidth - fWidth );
    m_TestRect.y1 = min( m_TestRect.y1, ( FLOAT )m_d3dpp.BackBufferHeight - fHeight );

    m_TestRect.x2 = m_TestRect.x1 + fWidth;
    m_TestRect.y2 = m_TestRect.y1 + fHeight;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Fills the stencil buffer with a specific pattern, and then draws a
//       stencil-tested solid rectangle to show that the stencil buffer is correctly
//       initialized with the desired pattern.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    m_pd3dDevice->BeginScene();

    m_pd3dDevice->SetRenderTarget( 0, m_pRenderTarget );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencil );

    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET0 | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0 );

    // Render the selected pattern into the stencil buffer.  This has to be done every
    // frame, since EDRAM is not preserved from one frame to the next.
    RenderStencilPatterns();

    // Render the stencil test geometry to the color rendertarget and the depth/stencil
    // surface.
    if( !m_bShowStencilRendering )
    {
        RenderStencilTestGeometry();
    }

    RenderUI();

    m_pd3dDevice->EndScene();

    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DrawStencilFillGeometry()
// Desc: Builds and draws a series of EDRAM tile-aligned vertical rects that place a
//       contiguous texture pattern into the depth/stencil buffer.  
//--------------------------------------------------------------------------------------
VOID Sample::DrawStencilFillGeometry( const RECTANGLE& ScreenRect, const RECTANGLE& TextureRect )
{
    // Set up tile size constants (change the first one for 2x or 4x MSAA).
    const FLOAT fTileWidth = ( FLOAT )GPU_EDRAM_TILE_WIDTH_1X;
    const FLOAT fHalfTileWidth = fTileWidth / 2;

    // Set up texture UV constants.
    const FLOAT fRectWidth = ScreenRect.Width();
    const FLOAT fTextureXOffset = TextureRect.x1;
    const FLOAT fTextureWidth = TextureRect.Width();

    // Figure out the left and right sides of the rectangle, in units of EDRAM tile widths.
    FLOAT fStartX = ScreenRect.x1;
    FLOAT fEdramTileStartX = fStartX / fTileWidth;
    FLOAT fEndX = ScreenRect.x2;
    FLOAT fEdramTileEndX = fEndX / fTileWidth;

    // Create 2 vertical rects per EDRAM tile.  This is used to compute a vertex count.
    DWORD dwRectCount = ( DWORD )( fEdramTileEndX - fEdramTileStartX + 1 ) * 2;

    // If the rectangle starts more than halfway into the leftmost EDRAM tile, we won't need one rect.
    if( fStartX - ( fEdramTileStartX * fTileWidth ) >= fHalfTileWidth )
        dwRectCount--;

    // If the rectangle ends less than halfway into the rightmost EDRAM tile, we won't need one rect.
    if( fEndX - ( fEdramTileEndX * fTileWidth ) < fHalfTileWidth )
        dwRectCount--;

    // Begin drawing vertices.
    MeshVertexPT* pVertexData = NULL;
    HRESULT hr = m_pd3dDevice->BeginVertices( D3DPT_RECTLIST, 3 * dwRectCount, sizeof( MeshVertexPT ),
                                              ( VOID** )&pVertexData );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not draw dynamic geometry." );

    // Loop over the EDRAM tiles that are covered by the width of the rectangle.
    for( FLOAT fTileIndex = fEdramTileStartX; fTileIndex <= fEdramTileEndX; ++fTileIndex )
    {
        // Compute the screenspace X coordinates of the left, middle, and right of the
        // EDRAM tile.
        FLOAT fLeftX = fTileIndex * fTileWidth;
        FLOAT fRightX = fLeftX + fTileWidth;
        FLOAT fMiddleX = fLeftX + fHalfTileWidth;

        // Build left rect for this tile, if the left rect falls within the desired boundaries.  
        // The rect will be placed on the right.
        if( ScreenRect.x1 < fMiddleX && ScreenRect.x2 >= fLeftX )
        {
            // Compute the left and right edge screen coordinates of this rect.
            FLOAT fLeftEdge = max( ScreenRect.x1, fLeftX );
            FLOAT fRightEdge = min( ScreenRect.x2, fMiddleX );
            // Compute the left and right texture coordinates of this rect.
            FLOAT fTexCoordLeft = ( ( fLeftEdge - ScreenRect.x1 ) / fRectWidth ) * fTextureWidth + fTextureXOffset;
            FLOAT fTexCoordRight = ( ( fRightEdge - ScreenRect.x1 ) / fRectWidth ) * fTextureWidth + fTextureXOffset;
            // Move the screen coordinates to the right half of the EDRAM tile.
            FLOAT fLeftEdgeTileSwap = fLeftEdge + fHalfTileWidth;
            FLOAT fRightEdgeTileSwap = fRightEdge + fHalfTileWidth;
            // Build rectangle vertices.
            pVertexData->vPosition.x = fLeftEdgeTileSwap;
            _WriteBarrier();
            pVertexData->vPosition.y = ScreenRect.y1;
            _WriteBarrier();
            pVertexData->vPosition.z = 0;
            _WriteBarrier();
            pVertexData->vTexCoord.x = fTexCoordLeft;
            _WriteBarrier();
            pVertexData->vTexCoord.y = TextureRect.y1;
            _WriteBarrier();
            ++pVertexData;
            pVertexData->vPosition.x = fRightEdgeTileSwap;
            _WriteBarrier();
            pVertexData->vPosition.y = ScreenRect.y1;
            _WriteBarrier();
            pVertexData->vPosition.z = 0;
            _WriteBarrier();
            pVertexData->vTexCoord.x = fTexCoordRight;
            _WriteBarrier();
            pVertexData->vTexCoord.y = TextureRect.y1;
            _WriteBarrier();
            ++pVertexData;
            pVertexData->vPosition.x = fLeftEdgeTileSwap;
            _WriteBarrier();
            pVertexData->vPosition.y = ScreenRect.y2;
            _WriteBarrier();
            pVertexData->vPosition.z = 0;
            _WriteBarrier();
            pVertexData->vTexCoord.x = fTexCoordLeft;
            _WriteBarrier();
            pVertexData->vTexCoord.y = TextureRect.y2;
            _WriteBarrier();
            ++pVertexData;
        }

        // Build right rect for this tile, if the right rect falls within the desired boundaries.  
        // The rect will be placed on the left.
        if( ScreenRect.x1 < fRightX && ScreenRect.x2 >= fMiddleX )
        {
            // Compute the left and right edge screen coordinates of this rect.
            FLOAT fLeftEdge = max( ScreenRect.x1, fMiddleX );
            FLOAT fRightEdge = min( ScreenRect.x2, fRightX );
            // Compute the left and right texture coordinates of this rect.
            FLOAT fTexCoordLeft = ( ( fLeftEdge - ScreenRect.x1 ) / fRectWidth ) * fTextureWidth + fTextureXOffset;
            FLOAT fTexCoordRight = ( ( fRightEdge - ScreenRect.x1 ) / fRectWidth ) * fTextureWidth + fTextureXOffset;
            // Move the screen coordinates to the left half of the EDRAM tile.
            FLOAT fLeftEdgeTileSwap = fLeftEdge - fHalfTileWidth;
            FLOAT fRightEdgeTileSwap = fRightEdge - fHalfTileWidth;
            // Build rectangle vertices.
            pVertexData->vPosition.x = fLeftEdgeTileSwap;
            _WriteBarrier();
            pVertexData->vPosition.y = ScreenRect.y1;
            _WriteBarrier();
            pVertexData->vPosition.z = 0;
            _WriteBarrier();
            pVertexData->vTexCoord.x = fTexCoordLeft;
            _WriteBarrier();
            pVertexData->vTexCoord.y = TextureRect.y1;
            _WriteBarrier();
            ++pVertexData;
            pVertexData->vPosition.x = fRightEdgeTileSwap;
            _WriteBarrier();
            pVertexData->vPosition.y = ScreenRect.y1;
            _WriteBarrier();
            pVertexData->vPosition.z = 0;
            _WriteBarrier();
            pVertexData->vTexCoord.x = fTexCoordRight;
            _WriteBarrier();
            pVertexData->vTexCoord.y = TextureRect.y1;
            _WriteBarrier();
            ++pVertexData;
            pVertexData->vPosition.x = fLeftEdgeTileSwap;
            _WriteBarrier();
            pVertexData->vPosition.y = ScreenRect.y2;
            _WriteBarrier();
            pVertexData->vPosition.z = 0;
            _WriteBarrier();
            pVertexData->vTexCoord.x = fTexCoordLeft;
            _WriteBarrier();
            pVertexData->vTexCoord.y = TextureRect.y2;
            _WriteBarrier();
            ++pVertexData;
        }
    }

    m_pd3dDevice->EndVertices();
}


//--------------------------------------------------------------------------------------
// Name: DrawFilledRect()
// Desc: Draws a screenspace rectangle with a texture, specified with certain UV coords.
//       If the pTexture parameter is NULL, the rectangle will be solid and use the
//       specified Color parameter.
//--------------------------------------------------------------------------------------
VOID Sample::DrawFilledRect( const RECTANGLE& ScreenRect, const RECTANGLE& UVRect, D3DTexture* pTexture,
                             D3DCOLOR Color )
{
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    m_pd3dDevice->SetVertexShader( m_pPassthruVS );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );

    if( pTexture != NULL )
    {
        m_pd3dDevice->SetPixelShader( m_pTexturedPS );
        m_pd3dDevice->SetTexture( 0, pTexture );
    }
    else
    {
        D3DXCOLOR FloatColor( Color );
        m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&FloatColor, 1 );
        m_pd3dDevice->SetPixelShader( m_pSolidPS );
        m_pd3dDevice->SetTexture( 0, NULL );
    }

    MeshVertexPT* pVertexData = NULL;
    HRESULT hr = m_pd3dDevice->BeginVertices( D3DPT_RECTLIST, 3, sizeof( MeshVertexPT ), ( VOID** )&pVertexData );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not draw dynamic geometry." );
    pVertexData[0].vPosition.x = ScreenRect.x1;
    _WriteBarrier();
    pVertexData[0].vPosition.y = ScreenRect.y1;
    _WriteBarrier();
    pVertexData[0].vPosition.z = 0;
    _WriteBarrier();
    pVertexData[0].vTexCoord.x = UVRect.x1;
    _WriteBarrier();
    pVertexData[0].vTexCoord.y = UVRect.y1;
    _WriteBarrier();
    pVertexData[1].vPosition.x = ScreenRect.x2;
    _WriteBarrier();
    pVertexData[1].vPosition.y = ScreenRect.y1;
    _WriteBarrier();
    pVertexData[1].vPosition.z = 0;
    _WriteBarrier();
    pVertexData[1].vTexCoord.x = UVRect.x2;
    _WriteBarrier();
    pVertexData[1].vTexCoord.y = UVRect.y1;
    _WriteBarrier();
    pVertexData[2].vPosition.x = ScreenRect.x1;
    _WriteBarrier();
    pVertexData[2].vPosition.y = ScreenRect.y2;
    _WriteBarrier();
    pVertexData[2].vPosition.z = 0;
    _WriteBarrier();
    pVertexData[2].vTexCoord.x = UVRect.x1;
    _WriteBarrier();
    pVertexData[2].vTexCoord.y = UVRect.y2;
    m_pd3dDevice->EndVertices();
}


//--------------------------------------------------------------------------------------
// Name: RenderStencilPatterns()
// Desc: Using a color target overlaid onto a depth/stencil target in EDRAM, this
//       method quickly fills the stencil buffer with contents, by rendering to the
//       red channel of the color target.
//--------------------------------------------------------------------------------------
VOID Sample::RenderStencilPatterns()
{
    PIXNamedEvent( "Stencil Buffer Fill" );

    // Render to the stencil fill target only.
    // Remember that the stencil fill target uses the same EDRAM offset as the
    // depth/stencil surface.
    if( !m_bShowStencilRendering )
    {
        m_pd3dDevice->SetRenderTarget( 0, m_pStencilFillTarget );
        m_pd3dDevice->SetDepthStencilSurface( NULL );
    }
    else
    {
        m_pd3dDevice->SetRenderTarget( 0, m_pRenderTarget );
        m_pd3dDevice->SetDepthStencilSurface( NULL );
    }

    // Only render to the red channel; this will make sure any existing depth values 
    // won't get clobbered.
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    const RECTANGLE ScreenRect = { 0, 0, ( FLOAT )m_d3dpp.BackBufferWidth, ( FLOAT )m_d3dpp.BackBufferHeight };
    switch( m_PatternType )
    {
        case SPT_ORDEREDDITHER:
        {
            // Render a fullscreen rect with the 8x8 dither texture.
            // The special stencil fill geometry is not needed because the texture is
            // 8 pixels wide and screen aligned, so the half-tile swap won't change the
            // appearance of the repeating pattern.
            const FLOAT fTexWidth = 8.0f;
            const FLOAT fTexHeight = 8.0f;
            RECTANGLE UVRect =
            {
                0, 0, ( FLOAT )m_d3dpp.BackBufferWidth / fTexWidth,
                ( FLOAT )m_d3dpp.BackBufferHeight / fTexHeight
            };
            DrawFilledRect( ScreenRect, UVRect, m_pDitherTexture );
            break;
        }
        case SPT_TEXT:
        {
            // Render special stencil fill geometry with a repeating 512x512 texture.
            m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
            m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
            m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
            m_pd3dDevice->SetVertexShader( m_pPassthruVS );
            m_pd3dDevice->SetPixelShader( m_pTexturedPS );
            m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
            m_pd3dDevice->SetTexture( 0, m_pBitMaskTexture );
            const FLOAT fTexWidth = 512.0f;
            const FLOAT fTexHeight = 512.0f;
            RECTANGLE UVRect =
            {
                0, 0, ( FLOAT )m_d3dpp.BackBufferWidth / fTexWidth,
                ( FLOAT )m_d3dpp.BackBufferHeight / fTexHeight
            };
            DrawStencilFillGeometry( ScreenRect, UVRect );
            break;
        }
    }

    // Restore renderstate.
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
}


//--------------------------------------------------------------------------------------
// Name: RenderStencilTestGeometry()
// Desc: Draws a stencil-tested solid rectangle to show the effect of the stencil buffer
//       contents.  The stencil test and stencil ref values are set according to the
//       type of pattern being used.
//--------------------------------------------------------------------------------------
VOID Sample::RenderStencilTestGeometry()
{
    PIXNamedEvent( "Test Geometry" );

    // Set the default rendertarget and depth/stencil surface.
    m_pd3dDevice->SetRenderTarget( 0, m_pRenderTarget );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencil );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    RECTANGLE UVRect = { 0 };

    switch( m_PatternType )
    {
        case SPT_ORDEREDDITHER:
        {
            // Draw the test rectangle with a greater stencil test.
            // The dither pattern in the stencil buffer ranges from values of 0 to 63.
            // Set the stencil ref to a smoothly varying value between 0 and 64 (64 so the fully opaque scenario will be seen for a non-zero amount of time).
            // This will reject certain pixels in the rectangle, causing it to smoothly
            // dither.
            DWORD dwStencilRef = ( DWORD )( ( sinf( m_fTime * 0.5f ) + 1.0f ) * 32.0f );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILREF, dwStencilRef );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILMASK, 0xFFFFFFFF );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILWRITEMASK, 0 );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_GREATER );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_KEEP );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_ZERO );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_ZERO );
            DrawFilledRect( m_TestRect, UVRect, NULL, 0xFFFFFFFF );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );
            break;
        }
        case SPT_TEXT:
        {
            // Draw the test rectangle with an equal stencil test.
            // The pattern in the stencil buffer has different contents in each bit.
            // Set the stencil ref and stencil mask to select one bit at a time.
            // This will cause the rectangle to show only the contents of the selected
            // bit in the stencil buffer.
            DWORD dwStencilRef = 1 << ( ( DWORD )( m_fTime * 0.25f ) % 8 );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILREF, dwStencilRef );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILMASK, dwStencilRef );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILWRITEMASK, 0 );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_EQUAL );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_KEEP );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_ZERO );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_ZERO );
            DrawFilledRect( m_TestRect, UVRect, NULL, 0xFFFFFFFF );
            m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );
            break;
        }
    }

}


//--------------------------------------------------------------------------------------
// Name: RenderUI()
// Desc: Draws some statistics and other UI elements
//--------------------------------------------------------------------------------------
VOID Sample::RenderUI()
{
    PIXNamedEvent( "UI" );

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"FastStencilFill" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.DrawText( 0, 30, 0xff00ffff, L"Stencil Pattern: " );
        m_Font.DrawText( 160, 30, 0xff00ffff, g_strStencilPatternNames[ m_PatternType ] );

        m_Font.End();
    }
}
