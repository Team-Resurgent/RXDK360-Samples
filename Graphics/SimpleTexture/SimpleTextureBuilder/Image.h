//--------------------------------------------------------------------------------------
//
// Image.h
//
// Definition for the image encapsulation class.
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#pragma once

//----------------------------------------------------------------------------------
// Name: class CImage
//
// Desc: Simple class to encapsulate an image. Provides simple methods 
//       to load, copy with scale, and expose the underlying Gdiplus 
//       bitmap object.
//
//----------------------------------------------------------------------------------
class CImage
{
public:
            CImage() : m_pbmpImage( NULL )
            {
            }

            ~CImage()
            {
                delete m_pbmpImage;
            }

    HRESULT Load( const char* szFilename );
    HRESULT Copy( CImage* pSource, Gdiplus::Size& szDest );

    DWORD   GetWidth() const
    {
        return m_dwWidth;
    }

    DWORD   GetHeight() const
    {
        return m_dwHeight;
    }

    Gdiplus::Bitmap* GetBitmap() const
    {
        return m_pbmpImage;
    }

private:
    Gdiplus::Bitmap* m_pbmpImage;
    DWORD m_dwWidth;
    DWORD m_dwHeight;
};
