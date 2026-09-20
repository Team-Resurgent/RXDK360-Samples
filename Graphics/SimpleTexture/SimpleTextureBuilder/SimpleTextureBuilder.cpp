//--------------------------------------------------------------------------------------
//
// SimpleTextureBuilder.cpp
//
// Main entry point for the SimpleTextureBuilder application.
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Image.h"
#include "Texture.h"

int _tmain( int argc, _TCHAR* argv[] )
{
    if( argc < 4 )
    {
        printf(
            "\nUsage\n    SimpleTextureBuilder <format> <source> <target>\n\nWhere:\n<format> = D3DFMT_A8R8G8B8, D3DFMT_DXT1, etc, \n<source> = full path of source image file, and \n<target> = full path of target texture file.\n\n" );
        return 1;
    }

    // Initialize Gdiplus...
    ULONG_PTR token;
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartupOutput output;
    Gdiplus::GdiplusStartup( &token, &input, &output );

    // Load the root image.
    CImage root;
    if( FAILED( root.Load( argv[2] ) ) )
    {
        printf( "\nError: Failed to load source image.\n" );
        return 1;
    }

    CTexture texture( GetTextureFormat ( argv[1] ) );
    if( FAILED( texture.Initialize( &root, 0 ) ) )
    {
        printf( "\nError: Failed to create texture from source image %s.\n", argv[2] );
        return 1;
    }

    if( FAILED( texture.SaveToFile( argv[3] ) ) )
    {
        printf( "\nError: Failed to save texture file.\n" );
        return 1;
    }

    return 0;
}


