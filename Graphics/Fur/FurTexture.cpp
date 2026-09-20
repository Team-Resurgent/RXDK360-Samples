//--------------------------------------------------------------------------------------
// FurTexture.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// Copyright (C) Tomohide Kano. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <d3dx9.h>
#include <xgraphics.h>
#include "FurTexture.h"
#include <AtgApp.h>
#include <AtgUtil.h>


// Disable warning C6211: Leaking memory <pointer> due to an exception.
// We "know" that we won't run out of memory in this sample
#pragma warning ( disable : 6211 )


//--------------------------------------------------------------------------------------
// Name: rnd()
// Desc: Generate a random floating point value between min and max
//--------------------------------------------------------------------------------------
inline FLOAT rnd( FLOAT min=0.0f, FLOAT max=1.0f )
{
    const FLOAT INV_RAND_MAX = 1.0f / ( RAND_MAX + 1 );
    return min + ( max - min ) * INV_RAND_MAX * rand();
}


//--------------------------------------------------------------------------------------
// Name: FurTexture()
// Desc: 
//--------------------------------------------------------------------------------------
FurTexture::FurTexture()
{
    m_dwSize = 0;
    m_dwNumLayers = 0;
    m_pColorTexture = NULL;
    m_ppLayerTextures = NULL;
}


//--------------------------------------------------------------------------------------
// Name: ~FurTexture()
// Desc: 
//--------------------------------------------------------------------------------------
FurTexture::~FurTexture()
{
    if( m_pColorTexture )
        m_pColorTexture->Release();

    if( m_ppLayerTextures )
    {
        for( DWORD i = 0; i < m_dwNumLayers; i++ )
            if( m_ppLayerTextures )
                m_ppLayerTextures[i]->Release();
        delete[] m_ppLayerTextures;
    }
}


//--------------------------------------------------------------------------------------
// Name: Init()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT FurTexture::Init( DWORD dwSeed, DWORD dwSize, DWORD dwNumLayers )
{
    srand( dwSeed );
    m_dwSize = dwSize;
    m_dwNumLayers = dwNumLayers;

    D3DXCOLOR* data = new D3DXCOLOR[m_dwNumLayers * m_dwSize * m_dwSize];
#define DATA(layer, x, y) data[m_dwSize*m_dwSize*(layer) + m_dwSize*(y) + (x)]

    for( DWORD y = 0; y < m_dwSize; y++ )
    {
        for( DWORD x = 0; x < m_dwSize; x++ )
        {
            // Hair color
            D3DXCOLOR color;
            color.r = rnd( 1.0f, 1.0f );
            color.g = rnd( 0.6f, 0.8f );
            color.b = rnd( 0.2f, 0.5f );
            color.a = 0;
            if( rnd() < 0.15f ) color *= rnd( 0.2f, 0.4f );
            else
                color *= rnd( 0.7f, 1.0f );

            // Adjust color values based on the fact we use an sRGB rendertarget and front-buffer.
            color.r = powf( color.r, 2.2f );
            color.g = powf( color.g, 2.2f );
            color.b = powf( color.b, 2.2f );
            color.a = powf( color.a, 2.2f );

            // Must have more than 1 layer, otherwise we'll run into a divide by zero problem
            assert( dwNumLayers > 1 );

            for( DWORD layer = 0; layer < m_dwNumLayers; layer++ )
            {
                // Lower layer is darker
                FLOAT t = max( 0.0f, 1.0f - 2.0f * ( FLOAT )( layer ) / ( m_dwNumLayers - 1 ) );
                DATA(layer, x, y) = color * ( 1.0f - 0.7f * powf( t, 1.5f ) );
            }

            // Length of the hair
            DWORD length = 1 + ( DWORD )( m_dwNumLayers * powf( rnd(), 3.0f ) );
            length = min( m_dwNumLayers, length );

            for( DWORD layer = 0; layer < length; layer++ )
            {
                // Tip of the hair is semi-transparent
                FLOAT t = max( 0.0f, -1.0f + 2.0f * ( FLOAT )( layer + 1 ) / length );
                DATA(layer, x, y).a = 1.0f - 0.85f * powf( t, 2.0f );
            }
        }
    }

    m_ppLayerTextures = new LPDIRECT3DTEXTURE9[m_dwNumLayers];

    for( DWORD layer = 0; layer < m_dwNumLayers; layer++ )
    {
        // Note that since the texels are random, we can use tiled textures
        // without having to call XGTileSurface()
        ATG::g_pd3dDevice->CreateTexture( m_dwSize, m_dwSize, 0, 0, D3DFMT_A8R8G8B8,
                                          D3DPOOL_DEFAULT, &m_ppLayerTextures[layer], NULL );

        D3DLOCKED_RECT lock;
        m_ppLayerTextures[layer]->LockRect( 0, &lock, 0, 0L );
        DWORD* p = ( DWORD* )lock.pBits;
        D3DXCOLOR* pColor = &DATA(layer, 0, 0);
        for( DWORD i = 0; i < m_dwSize * m_dwSize; i++ )
            *p++ = ( DWORD )( *pColor++ );
        m_ppLayerTextures[layer]->UnlockRect( 0 );

        LPDIRECT3DSURFACE9 pSrcSurface;
        m_ppLayerTextures[layer]->GetSurfaceLevel( 0, &pSrcSurface );
        DWORD dwNumMipMaps = m_ppLayerTextures[layer]->GetLevelCount();
        for( DWORD mip = 1; mip < dwNumMipMaps; mip++ )
        {
            LPDIRECT3DSURFACE9 pDstSurface;
            m_ppLayerTextures[layer]->GetSurfaceLevel( mip, &pDstSurface );
            D3DXLoadSurfaceFromSurface( pDstSurface, NULL, NULL, pSrcSurface,
                                        NULL, NULL, D3DX_DEFAULT, 0L );
            pDstSurface->Release();
        }
        pSrcSurface->Release();
    }

    delete [] data;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GeColorTexture()
// Desc: 
//--------------------------------------------------------------------------------------
LPDIRECT3DTEXTURE9 FurTexture::GetColorTexture()
{
    //  return m_pColorTexture;
    return m_ppLayerTextures[0];
}


//--------------------------------------------------------------------------------------
// Name: GetLayerTexture()
// Desc: 
//--------------------------------------------------------------------------------------
LPDIRECT3DTEXTURE9 FurTexture::GetLayerTexture( FLOAT fLayer )
{
    DWORD tex = ( DWORD )floorf( m_dwNumLayers * max( 0.0f, min( 0.9999f, fLayer ) ) );

    return m_ppLayerTextures[tex];
}


//--------------------------------------------------------------------------------------
// Name: BuildFurLightingTexture()
// Desc: Create a 3D volume texture for looking up lighting calculations
//--------------------------------------------------------------------------------------
HRESULT BuildFurLightingTexture( LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture )
{
#define CLAMP(x) max(0, min(1, (x)))

    // Texture size (as small as possible in order to minimize cache miss rate)
    const D3DFORMAT dwFormat = D3DFMT_LIN_DXT5;
    const DWORD dwUSize = 16;
    const DWORD dwVSize = 32;
    const DWORD dwWSize = 16;

    // Exponents for diffuse and specular
    static FLOAT fDiffusePower = 6.0f;
    static FLOAT fSpecularPower = 32.0f;

    DWORD* pBits = new DWORD[dwUSize * dwVSize * dwWSize];
    BYTE* p = ( BYTE* )pBits;

    for( DWORD w = 0; w < dwWSize; w++ )
    {
        FLOAT Lz = ( FLOAT )( 2 * w ) / ( dwWSize - 1 ) - 1; // w = L.z       [-1..+1]
        FLOAT fShadow = powf( cosf( D3DX_PI / 2 * CLAMP(-0.2f-1.4f*Lz) ), 4.0f );

        for( DWORD v = 0; v < dwVSize; v++ )
        {
            FLOAT fHdotN = ( FLOAT )( v ) / ( dwVSize - 1 );       // v = H dot N   [0..1]
            FLOAT fSpecular = powf( 1.0f - fHdotN * fHdotN, 0.5f * fSpecularPower );

            for( DWORD u = 0; u < dwUSize; u++ )
            {
                FLOAT fLdotN = ( FLOAT )( u ) / ( dwUSize - 1 );       // u = L dot N   [0..1]
                FLOAT fDiffuse = 0.2f + 0.2f * max( 0.0f, Lz ) + 0.6f * powf( 1.0f - fLdotN * fLdotN,
                                                                              0.5f * fDiffusePower );

                *p++ = ( BYTE )( 0xff * CLAMP(fSpecular*fShadow) );
                *p++ = ( BYTE )( 0xff * CLAMP(fDiffuse) );
                *p++ = ( BYTE )( 0xff * CLAMP(fDiffuse) );
                *p++ = ( BYTE )( 0xff * CLAMP(fDiffuse) );
            }
        }
    }

    // Copy the above data into a texture
    ATG::g_pd3dDevice->CreateVolumeTexture( dwUSize, dwVSize, dwWSize, 1, 0, dwFormat,
                                            D3DPOOL_DEFAULT, ppVolumeTexture, NULL );

    XGTEXTURE_DESC desc;
    XGGetTextureDesc( ( *ppVolumeTexture ), 0, &desc );

    D3DLOCKED_BOX lock;
    ( *ppVolumeTexture )->LockBox( 0, &lock, 0, 0 );

    // Compress the volume
    {
        D3DBOX box =
        {
            0, 0, desc.Width, desc.Height, 0, desc.Depth
        };

        DWORD dwSrcRowPitch = sizeof( DWORD ) * dwUSize;
        DWORD dwSrcSlicePitch = sizeof( DWORD ) * dwUSize * dwVSize;

        XGCompressVolume( lock.pBits, desc.RowPitch, desc.SlicePitch,
                          desc.Width, desc.Height, desc.Depth,
                          ( D3DFORMAT )MAKELINFMT( dwFormat ), NULL,
                          pBits, dwSrcRowPitch, dwSrcSlicePitch, D3DFMT_LIN_A8R8G8B8,
                          &box, 0, 0.0f );
    }

    // Tile the volume
    if( XGIsTiledFormat( dwFormat ) )
    {
        memcpy( pBits, lock.pBits, desc.SlicePitch * dwWSize );
        D3DBOX box =
        {
            0, 0, desc.WidthInBlocks, desc.HeightInBlocks, 0, desc.DepthInBlocks
        };

        XGTileVolume( lock.pBits, desc.WidthInBlocks, desc.HeightInBlocks, desc.DepthInBlocks, NULL,
                      pBits, desc.RowPitch, desc.SlicePitch, &box, desc.BytesPerBlock );
    }

    ( *ppVolumeTexture )->UnlockBox( 0 );

    delete[] pBits;

    return S_OK;
}
