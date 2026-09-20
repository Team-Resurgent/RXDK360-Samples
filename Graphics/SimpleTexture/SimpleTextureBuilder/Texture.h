//----------------------------------------------------------------------------------
//
// Texture.h
//
// Definition for the CTexture class and related objects. This class encapsulates a
// simple texture resource file's data, and the functionality to process source
// image data into the final texture format.
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#pragma once

#include "Image.h"

// Format definition
struct FORMATSPEC
{
    CHAR* strFormat;
    D3DFORMAT fmtXboxFormat;
};

#define FORMAT_LOOKUP(fmt) { #fmt, ##fmt }

static const FORMATSPEC g_aTextureFormats[] =
 {
    FORMAT_LOOKUP( D3DFMT_UNKNOWN ),

    // DXT:
    FORMAT_LOOKUP( D3DFMT_DXT1 ),
    FORMAT_LOOKUP( D3DFMT_DXT2 ),
    FORMAT_LOOKUP( D3DFMT_DXT3 ),
    FORMAT_LOOKUP( D3DFMT_DXT4 ),
    FORMAT_LOOKUP( D3DFMT_DXT5 ),
    FORMAT_LOOKUP( D3DFMT_DXN ),
    FORMAT_LOOKUP( D3DFMT_LIN_DXT4 ),

    // 8bpp:
    FORMAT_LOOKUP( D3DFMT_A8 ),
    FORMAT_LOOKUP( D3DFMT_L8 ),

    // 16bpp:
    FORMAT_LOOKUP( D3DFMT_R5G6B5 ),
    FORMAT_LOOKUP( D3DFMT_R6G5B5 ),
    FORMAT_LOOKUP( D3DFMT_L6V5U5 ),
    FORMAT_LOOKUP( D3DFMT_X1R5G5B5 ),
    FORMAT_LOOKUP( D3DFMT_A1R5G5B5 ),
    FORMAT_LOOKUP( D3DFMT_A4R4G4B4 ),
    FORMAT_LOOKUP( D3DFMT_X4R4G4B4 ),
    FORMAT_LOOKUP( D3DFMT_Q4W4V4U4 ),
    FORMAT_LOOKUP( D3DFMT_A8L8 ),
    FORMAT_LOOKUP( D3DFMT_G8R8 ),
    FORMAT_LOOKUP( D3DFMT_V8U8 ),
    FORMAT_LOOKUP( D3DFMT_D16 ),
    FORMAT_LOOKUP( D3DFMT_L16 ),
    FORMAT_LOOKUP( D3DFMT_R16F ),
    FORMAT_LOOKUP( D3DFMT_R16F_EXPAND ),
    FORMAT_LOOKUP( D3DFMT_UYVY ),
    FORMAT_LOOKUP( D3DFMT_LE_UYVY ),
    FORMAT_LOOKUP( D3DFMT_G8R8_G8B8 ),
    FORMAT_LOOKUP( D3DFMT_R8G8_B8G8 ),
    FORMAT_LOOKUP( D3DFMT_YUY2 ),
    FORMAT_LOOKUP( D3DFMT_LE_YUY2 ),

    // 32bpp:
    FORMAT_LOOKUP( D3DFMT_A8R8G8B8 ),
    FORMAT_LOOKUP( D3DFMT_X8R8G8B8 ),
    FORMAT_LOOKUP( D3DFMT_A8B8G8R8 ),
    FORMAT_LOOKUP( D3DFMT_X8B8G8R8 ),
    FORMAT_LOOKUP( D3DFMT_X8L8V8U8 ),
    FORMAT_LOOKUP( D3DFMT_Q8W8V8U8 ),
    FORMAT_LOOKUP( D3DFMT_A2R10G10B10 ),
    FORMAT_LOOKUP( D3DFMT_X2R10G10B10 ),
    FORMAT_LOOKUP( D3DFMT_A2B10G10R10 ),
    FORMAT_LOOKUP( D3DFMT_A2W10V10U10 ),
    FORMAT_LOOKUP( D3DFMT_A16L16 ),
    FORMAT_LOOKUP( D3DFMT_G16R16 ),
    FORMAT_LOOKUP( D3DFMT_V16U16 ),
    FORMAT_LOOKUP( D3DFMT_R10G11B11 ),
    FORMAT_LOOKUP( D3DFMT_R11G11B10 ),
    FORMAT_LOOKUP( D3DFMT_W10V11U11 ),
    FORMAT_LOOKUP( D3DFMT_W11V11U10 ),
    FORMAT_LOOKUP( D3DFMT_G16R16F ),
    FORMAT_LOOKUP( D3DFMT_G16R16F_EXPAND ),
    FORMAT_LOOKUP( D3DFMT_L32 ),
    FORMAT_LOOKUP( D3DFMT_R32F ),

    // 64bpp:
    FORMAT_LOOKUP( D3DFMT_A16B16G16R16 ),
    FORMAT_LOOKUP( D3DFMT_Q16W16V16U16 ),
    FORMAT_LOOKUP( D3DFMT_A16B16G16R16F ),
    FORMAT_LOOKUP( D3DFMT_A16B16G16R16F_EXPAND ),
    FORMAT_LOOKUP( D3DFMT_A32L32 ),
    FORMAT_LOOKUP( D3DFMT_G32R32 ),
    FORMAT_LOOKUP( D3DFMT_V32U32 ),
    FORMAT_LOOKUP( D3DFMT_G32R32F ),

    // 128bpp:
    FORMAT_LOOKUP( D3DFMT_A32B32G32R32 ),
    FORMAT_LOOKUP( D3DFMT_Q32W32V32U32 ),
    FORMAT_LOOKUP( D3DFMT_A32B32G32R32F ),

    // Front buffer formats
    FORMAT_LOOKUP( D3DFMT_LE_X8R8G8B8 ),
    FORMAT_LOOKUP( D3DFMT_LE_A8R8G8B8 ),
    FORMAT_LOOKUP( D3DFMT_LE_X2R10G10B10 ),
    FORMAT_LOOKUP( D3DFMT_LE_A2R10G10B10 ),

    // Other
    FORMAT_LOOKUP( D3DFMT_DXT3A ),
    FORMAT_LOOKUP( D3DFMT_DXT3A_1111 ),
    FORMAT_LOOKUP( D3DFMT_DXT5A ),
    FORMAT_LOOKUP( D3DFMT_CTX1 ),
    FORMAT_LOOKUP( D3DFMT_D24S8 ),
    FORMAT_LOOKUP( D3DFMT_D24X8 ),
    FORMAT_LOOKUP( D3DFMT_D24FS8 ),
    FORMAT_LOOKUP( D3DFMT_D32 ),
};

//----------------------------------------------------------------------------------
// Name: CTexture
//
// Desc: The CTexture class encapsulates simple texture data, and provides the 
//       functionality for creating texture data from source images.
//
//----------------------------------------------------------------------------------
class CTexture
{
public:
            CTexture( const D3DFORMAT fmt ) : m_aMipLevels      ( NULL ),
                                              m_pBuffer         ( NULL ),
                                              m_dwLevelCount    ( 0 ),
                                              m_fmtTarget       ( fmt )
            {
            }

            ~CTexture()
            {
                delete[] m_aMipLevels;
            }

    HRESULT Initialize( CImage* pImage, const DWORD dwMipLevels );
    HRESULT SaveToFile( const char* szFilename );

protected:
    HRESULT CreateMipChain( CImage* pImage );
    HRESULT DoConversion();

private:
    CImage* m_aMipLevels;
    DWORD m_dwLevelCount;
    UINT m_dwBaseSize;
    UINT m_dwMipSize;
    BYTE* m_pBuffer;
    D3DBaseTexture m_textureHeader;
    D3DFORMAT m_fmtTarget;
};

//----------------------------------------------------------------------------------
// Name: GetTextureFormat
//
// Desc: Returns the D3DFORMAT given a string description (e.g. returns
//       D3DFMT_DXT1 given the string "D3DFMT_DXT1").
//----------------------------------------------------------------------------------
__inline D3DFORMAT GetTextureFormat( const char* szFmtString )
{
    D3DFORMAT fmt = D3DFMT_UNKNOWN;
    for( int i = 0; i < sizeof( g_aTextureFormats ) / sizeof( FORMATSPEC ); i++ )
    {
        if( !lstrcmp( szFmtString, g_aTextureFormats[ i ].strFormat ) )
        {
            fmt = ( D3DFORMAT )g_aTextureFormats[ i ].fmtXboxFormat;
            break;
        }
    }

    return fmt;
};
