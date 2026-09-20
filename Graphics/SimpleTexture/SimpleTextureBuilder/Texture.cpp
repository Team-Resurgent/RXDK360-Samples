//----------------------------------------------------------------------------------
//
// Texture.cpp
//
// Implementation for the CTexture class. This class encapsulates a simple texture 
// resource file's data, and the functionality to process source image data into 
// the final texture format.
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


#include "stdafx.h"
#include "Texture.h"

//----------------------------------------------------------------------------------
// Name: CTexture::CreateMipChain
//
// Desc: Create the full mip-chain for this texture, based on a source image.
//
//       The mip-chain generation uses GDI+, and the "high quality bicubic filtering"
//       quality setting for the resize. This produces very high quality results when
//       filtering down from an image to smaller sizes.
//
//----------------------------------------------------------------------------------
HRESULT CTexture::CreateMipChain( CImage* pImage )
{
    // Create an array of CImage objects - one for each mip level.
    m_aMipLevels = new CImage[ m_dwLevelCount ];
    if( NULL == m_aMipLevels )
    {
        return E_OUTOFMEMORY;
    }

    // Now down-sample our source image into each mip-level image.
    for( DWORD i = 0; i < m_dwLevelCount; i++ )
    {
        HRESULT hr;

        Gdiplus::Size sz( max( pImage->GetWidth() >> i, 1 ), max( pImage->GetHeight() >> i, 1 ) );
        hr = m_aMipLevels[i].Copy( pImage,
                                   sz );
        if( FAILED( hr ) )
        {
            delete m_aMipLevels;
            m_aMipLevels = NULL;
            return hr;
        }
    }

    return S_OK;
}

//----------------------------------------------------------------------------------
// Name: CTexture::DoConversion
//
// Desc: Convert the array of images comprising the texture's mip chain into the
//       final format (tiling as appropriate).
//
//----------------------------------------------------------------------------------
HRESULT CTexture::DoConversion()
{
    HRESULT hr = S_OK;
    XGTEXTURE_DESC BaseDesc;
    BYTE* pTemp = NULL;
    DWORD dwGpuFormat;
    BOOL bTiled;
    D3DFORMAT fmtCompress;

    // Get the description of the base texture level.
    XGGetTextureDesc( &m_textureHeader, 0, &BaseDesc );

    dwGpuFormat = XGGetGpuFormat( ( D3DFORMAT )BaseDesc.Format );
    bTiled = XGIsTiledFormat( ( D3DFORMAT )BaseDesc.Format );
    fmtCompress = ( D3DFORMAT )( BaseDesc.Format & ~D3DFORMAT_TILED_MASK );

    if( bTiled )
    {
        // If we're creating a tiled mip-chain, we're going to compress to a linear format first, and then
        // tile it. Create a temporary buffer to hold the (temporary) linear texture data.

        pTemp = new BYTE[ m_dwBaseSize + m_dwMipSize ];
        if( NULL == pTemp )
        {
            return E_OUTOFMEMORY;
        }

    }

    for( DWORD dwLevel = 0; dwLevel < m_dwLevelCount; dwLevel++ )
    {
        XGTEXTURE_DESC MipDesc;

        // Get a local pointer to the current mip level.
        CImage* pImage = &m_aMipLevels[ dwLevel ];

        // Set up lock data for the GDI+ bitmap.
        Gdiplus::Rect rcLock( 0, 0, pImage->GetWidth(), pImage->GetHeight() );
        Gdiplus::BitmapData bmd;
        ZeroMemory( &bmd, sizeof( bmd ) );

        // Get the bitmap bits...
        pImage->GetBitmap()->LockBits( &rcLock, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bmd );

        // Perform the endian-swap in-place on the image data...
        XGEndianSwapMemory( bmd.Scan0,
                            bmd.Scan0,
                            XGENDIAN_8IN32,
                            sizeof( DWORD ),
                            pImage->GetWidth() * pImage->GetHeight() );

        // Get the description of the current mip level.
        XGGetTextureDesc( &m_textureHeader, dwLevel, &MipDesc );

        // Get the mip level offset within our buffer.
        DWORD dwMipLevelOffset = XGGetMipLevelOffset( &m_textureHeader,
                                                      0,
                                                      dwLevel );
        if( ( dwLevel > 0 ) && ( m_dwMipSize > 0 ) )
        {
            dwMipLevelOffset += m_dwBaseSize;
        }

        if( bTiled )
        {
            // Compress the surface to a linear format (the only supported formats for compression
            // are linear).
            hr = XGCopySurface( pTemp,
                                MipDesc.RowPitch,
                                MipDesc.Width,
                                MipDesc.Height,
                                fmtCompress,
                                NULL,
                                bmd.Scan0,
                                bmd.Stride,
                                ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LIN_A8R8G8B8 ),
                                NULL,
                                XGCOMPRESS_NO_DITHERING,
                                0.0f );

            // Now tile.
            if( SUCCEEDED( hr ) )
            {
                XGTileTextureLevel( BaseDesc.Width,
                                    BaseDesc.Height,
                                    dwLevel,
                                    XGGetGpuFormat( fmtCompress ),
                                    0,
                                    m_pBuffer + dwMipLevelOffset,
                                    NULL,
                                    pTemp,
                                    MipDesc.RowPitch,
                                    NULL );
            }
        }
        else
        {
            hr = XGCopySurface( m_pBuffer + dwMipLevelOffset,
                                MipDesc.RowPitch,
                                MipDesc.Width,
                                MipDesc.Height,
                                MipDesc.Format,
                                NULL,
                                bmd.Scan0,
                                bmd.Stride,
                                ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LIN_A8R8G8B8 ),
                                NULL,
                                XGCOMPRESS_NO_DITHERING,
                                0.0f );
        }

        // Unlock the GDI+ bitmap object.
        pImage->GetBitmap()->UnlockBits( &bmd );

        if( FAILED( hr ) )
        {
            break;
        }
    }

    delete[] pTemp;

    return hr;
}

//----------------------------------------------------------------------------------
// Name: CTexture::Initialize
//
// Desc: Initialize the texture. This function takes a source image (passed in as
//       the CImage parameter) and generates a fully filtered mip-chain on it. The
//       number of levels is specified by the dwMipLevels parameter. If this 
//       parameter is 0, the number of levels is the maximum number of levels 
//       available for the dimensions of the source image. Once the mip chain is
//       created, the image data is converted to the final format.
//
//----------------------------------------------------------------------------------
HRESULT CTexture::Initialize( CImage* pImage, DWORD dwMipLevels )
{
    HRESULT hr;

    if( NULL == pImage )
    {
        return E_POINTER;
    }

    // Calculate the number of mip-levels to generate on this texture.

    // First, if we pass in 0, it means generate all of them - so just set the parameter value to 
    // something huge.
    if( dwMipLevels == 0 )
    {
        dwMipLevels = 0xffffffff;
    }

    DWORD dwMaxDim = max( pImage->GetWidth(), pImage->GetHeight() );
    DWORD dwMaxLevels = 1UL + ( DWORD )( logf( ( float )dwMaxDim ) / logf( 2.0f ) );

    // Now set the mip count to the minimum value between the parameter and the max allowed number.
    m_dwLevelCount = min( dwMaxLevels, dwMipLevels );

    // Create the mip-chain.
    hr = CreateMipChain( pImage );
    if( FAILED( hr ) )
    {
        return hr;
    }

    // Now set the texture header with the information we've obtained so far.
    XGSetTextureHeaderEx( pImage->GetWidth(),
                          pImage->GetHeight(),
                          m_dwLevelCount,
                          0,
                          ( D3DFORMAT )MAKESRGBFMT( m_fmtTarget ),
                          0,
                          0,
                          0,
                          XGHEADER_CONTIGUOUS_MIP_OFFSET,
                          0,
                          ( D3DTexture* )&m_textureHeader,
                          &m_dwBaseSize,
                          &m_dwMipSize );

    // Create a buffer to hold the texture data.
    m_pBuffer = new BYTE[ m_dwBaseSize + m_dwMipSize ];

    if( NULL == m_pBuffer )
    {
        return E_OUTOFMEMORY;
    }

    // Perform the format conversion...
    return DoConversion();
}

//----------------------------------------------------------------------------------
// Name: CTexture::SaveToFile
//
// Desc: Save the final texture data out to file.
//
//----------------------------------------------------------------------------------
HRESULT CTexture::SaveToFile( const char* szFilename )
{
    if( NULL == szFilename )
    {
        return E_POINTER;
    }

    // First, create a new file.
    HANDLE hFile = ::CreateFile( szFilename,
                                 GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ,
                                 NULL,
                                 CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL,
                                 NULL );

    // If that failed (e.g. the file already exists) truncate the existing file.
    if( INVALID_HANDLE_VALUE == hFile )
    {
        hFile = ::CreateFile( szFilename,
                              GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ,
                              NULL,
                              TRUNCATE_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL );
    }

    // If both the above failed - bail.
    if( INVALID_HANDLE_VALUE == hFile )
    {
        return E_FAIL;
    }

    // Write the header...
    DWORD dwWritten = 0;

    // Endian swap the D3D texture header.
    XGEndianSwapMemory( &m_textureHeader,
                        &m_textureHeader,
                        XGENDIAN_8IN32,
                        sizeof( DWORD ),
                        sizeof( m_textureHeader ) / sizeof( DWORD ) );

    HRESULT hr = E_FAIL;

    // Write the D3D texture header.
    if( ::WriteFile( hFile, &m_textureHeader, sizeof( m_textureHeader ), &dwWritten, NULL ) &&
        ( dwWritten == sizeof( m_textureHeader ) ) )
    {

        // Now write the texture data.
        if( ::WriteFile( hFile, m_pBuffer, m_dwBaseSize + m_dwMipSize, &dwWritten, NULL ) &&
            ( dwWritten == ( m_dwBaseSize + m_dwMipSize ) ) )
        {
            hr = S_OK;
        }
    }

    ::CloseHandle( hFile );

    return hr;
}

