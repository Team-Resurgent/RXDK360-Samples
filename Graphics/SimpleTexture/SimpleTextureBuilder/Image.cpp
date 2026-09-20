//----------------------------------------------------------------------------------
//
// Image.cpp
//
// Implementation for the image encapsulation class.
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------

#include "stdafx.h"
#include "Image.h"

//----------------------------------------------------------------------------------
// Name: CImage::Load
//
// Desc: Create this image instance by loading the image from a file.
//
//----------------------------------------------------------------------------------
HRESULT CImage::Load( const char* szFilename )
{
    WCHAR wzFilename[MAX_PATH];

    // Convert to unicode, as Gdiplus takes unicode strings for all string parameters.
    if( !::MultiByteToWideChar( CP_ACP, 0, szFilename, -1, wzFilename, MAX_PATH ) )
    {
        return E_FAIL;
    }

    m_pbmpImage = new Gdiplus::Bitmap( wzFilename );
    if( NULL == m_pbmpImage )
    {
        return E_OUTOFMEMORY;
    }

    m_dwWidth = m_pbmpImage->GetWidth();
    m_dwHeight = m_pbmpImage->GetHeight();

    return ( ( m_dwWidth > 0 ) && ( m_dwHeight > 0 ) ) ? S_OK : E_FAIL;
}

//----------------------------------------------------------------------------------
// Name: CImage::Copy
//
// Desc: Create this image instance by making a filtered, scaled copy from a source
//       image object.
//
//----------------------------------------------------------------------------------
HRESULT CImage::Copy( CImage* pSource,          // Source image object.
                      Gdiplus::Size& szDest )   // Size of this image (copy)
{
    if( ( NULL == pSource ) || ( szDest.Width == 0 ) || ( szDest.Height == 0 ) )
    {
        return E_INVALIDARG;
    }

    // Delete any existing image data.
    if( NULL != m_pbmpImage )
    {
        delete m_pbmpImage;
    }

    // Create a new blank image of the specified size.
    m_pbmpImage = new Gdiplus::Bitmap( szDest.Width, szDest.Height );

    if( NULL == m_pbmpImage )
    {
        return E_OUTOFMEMORY;
    }

    Gdiplus::Graphics gr( m_pbmpImage );

    // Make sure we get maximum quality from the resize.
    gr.SetInterpolationMode( Gdiplus::InterpolationModeHighQualityBicubic );

    // Copy and scale from the source image.
    if( Gdiplus::Ok != gr.DrawImage( pSource->GetBitmap(),
                                     Gdiplus::Rect( 0, 0, szDest.Width, szDest.Height ),
                                     0,
                                     0,
                                     pSource->GetWidth(),
                                     pSource->GetHeight(),
                                     Gdiplus::UnitPixel,
                                     NULL ) )
    {
        return E_FAIL;
    }

    m_dwWidth = szDest.Width;
    m_dwHeight = szDest.Height;

    return S_OK;
}

