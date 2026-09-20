//--------------------------------------------------------------------------------------
// TiledResourceXbox360.h
//
// Common functions and defines for the tiled resource runtime on Xbox 360.  Only code
// that is platform specific to Xbox 360 should be in this header file.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#ifndef _XBOX
#error Only for use in Xbox 360 build.
#endif

namespace TiledRuntime
{
    // Max number of array slices supported on Xbox 360:
    static const UINT MAX_ARRAY_SLICES = 64;

    // Max number of simultaneous resources allowed for pixel shaders:
    static const UINT MAX_INDEX_MAP_SLOT = 4;

    // Beginning pixel shader texture register index for index map textures:
    static const UINT INDEX_MAP_SAMPLER_BEGIN = 7;

    // Beginning pixel shader texture register index for physical page array textures:
    static const UINT PAGEPOOL_MAP_SAMPLER_BEGIN = 11;

    // Max number of simultaneous resources allowed for vertex shaders:
    static const UINT VS_MAX_INDEX_MAP_SLOT = 1;

    // Beginning vertex shader texture register index for index map textures:
    static const UINT VS_INDEX_MAP_SAMPLER_BEGIN = D3DVERTEXTEXTURESAMPLER2;

    // Beginning vertex shader texture register index for physical page array textures:
    static const UINT VS_PAGEPOOL_MAP_SAMPLER_BEGIN = D3DVERTEXTEXTURESAMPLER3;

    // Beginning shader constant index for tiled resource LOD lookup tables:
    static const UINT LOD_RESOURCE_CONSTANT_BEGIN = 212;

    // Beginning shader constant index for tiled resource misc constants:
    static const UINT MISC_RESOURCE_CONSTANT_BEGIN = 208;

    // Beginning shader constant index for page pool UV transform constants:
    static const UINT PAGE_POOL_CONSTANT_BEGIN = 204;

    // Beginning shader constant index for physical page array constants:
    static const UINT ARRAY_POOL_CONSTANT_BEGIN = 200;

    // Texture format for index map textures:
    static const D3DFORMAT D3DFMT_INDEXMAP = (D3DFORMAT)MAKELINFMT( MAKED3DFMT(GPUTEXTUREFORMAT_5_6_5, GPUENDIAN_8IN16, TRUE, GPUSIGN_ALL_UNSIGNED, GPUNUMFORMAT_INTEGER, GPUSWIZZLE_ORGB) );

    // Size of a single index map texel in bytes:
    static const UINT INDEXMAP_TEXEL_SIZE_BYTES = sizeof(USHORT);

    // Struct that represents a 16bpp 565 texel:
    struct Texel565
    {
        USHORT Red : 5;
        USHORT Green : 6;
        USHORT Blue : 5;
    };

    // Index map texel value that represents a NULL virtual to physical mapping:
    static const Texel565 NULL_INDEXMAP_TEXEL = { 31, 63, 31 };

    //--------------------------------------------------------------------------------------
    // Name: GetPageDataFormat
    // Desc: Converts a GPU format value to a page data format.
    //--------------------------------------------------------------------------------------
    inline PageDataFormat GetPageDataFormat( const D3DFORMAT Format )
    {
        UINT GpuFormat = ( Format & D3DFORMAT_TEXTUREFORMAT_MASK ) >> D3DFORMAT_TEXTUREFORMAT_SHIFT;
        switch( GpuFormat )
        {
        case GPUTEXTUREFORMAT_1_REVERSE:
        case GPUTEXTUREFORMAT_1:
            // 1bpp
            return PDF_1BPP;
        case GPUTEXTUREFORMAT_8:
        case GPUTEXTUREFORMAT_8_A:
        case GPUTEXTUREFORMAT_8_B:
            // 8bpp
            return PDF_8BPP;
        case GPUTEXTUREFORMAT_1_5_5_5:
        case GPUTEXTUREFORMAT_5_6_5:
        case GPUTEXTUREFORMAT_6_5_5:
        case GPUTEXTUREFORMAT_8_8:
        case GPUTEXTUREFORMAT_Cr_Y1_Cb_Y0_REP:
        case GPUTEXTUREFORMAT_Y1_Cr_Y0_Cb_REP:
        case GPUTEXTUREFORMAT_16_16_EDRAM:
        case GPUTEXTUREFORMAT_4_4_4_4:
        case GPUTEXTUREFORMAT_16:
        case GPUTEXTUREFORMAT_16_EXPAND:
        case GPUTEXTUREFORMAT_16_FLOAT:
            // 16bpp
            return PDF_16BPP;
        case GPUTEXTUREFORMAT_8_8_8_8:
        case GPUTEXTUREFORMAT_2_10_10_10:
        case GPUTEXTUREFORMAT_8_8_8_8_A:
        case GPUTEXTUREFORMAT_10_11_11:
        case GPUTEXTUREFORMAT_11_11_10:
        case GPUTEXTUREFORMAT_24_8:
        case GPUTEXTUREFORMAT_24_8_FLOAT:
        case GPUTEXTUREFORMAT_16_16:
        case GPUTEXTUREFORMAT_16_16_EXPAND:
        case GPUTEXTUREFORMAT_16_16_FLOAT:
        case GPUTEXTUREFORMAT_32:
        case GPUTEXTUREFORMAT_32_FLOAT:
        case GPUTEXTUREFORMAT_8_8_8_8_AS_16_16_16_16:
        case GPUTEXTUREFORMAT_2_10_10_10_AS_16_16_16_16:
        case GPUTEXTUREFORMAT_10_11_11_AS_16_16_16_16:
        case GPUTEXTUREFORMAT_11_11_10_AS_16_16_16_16:
        case GPUTEXTUREFORMAT_8_8_8_8_GAMMA_EDRAM:
        case GPUTEXTUREFORMAT_2_10_10_10_FLOAT_EDRAM:
            // 32bpp
            return PDF_32BPP;
        case GPUTEXTUREFORMAT_DXT1:
        case GPUTEXTUREFORMAT_DXT1_AS_16_16_16_16:
            // BC1
            return PDF_BC1_4;
        case GPUTEXTUREFORMAT_DXT2_3:
        case GPUTEXTUREFORMAT_DXT2_3_AS_16_16_16_16:
        case GPUTEXTUREFORMAT_DXT4_5:
        case GPUTEXTUREFORMAT_DXT4_5_AS_16_16_16_16:
            // BC2 / BC3
            return PDF_BC2_3_5;
        case GPUTEXTUREFORMAT_16_16_16_16_EDRAM:
        case GPUTEXTUREFORMAT_16_16_16_16:
        case GPUTEXTUREFORMAT_16_16_16_16_EXPAND:
        case GPUTEXTUREFORMAT_16_16_16_16_FLOAT:
        case GPUTEXTUREFORMAT_32_32:
        case GPUTEXTUREFORMAT_32_32_FLOAT:
            // 64bpp
            return PDF_64BPP;
        case GPUTEXTUREFORMAT_32_32_32_32:
        case GPUTEXTUREFORMAT_32_32_32_32_FLOAT:
            // 128bpp
            return PDF_128BPP;
        case GPUTEXTUREFORMAT_DXN:
            // BCN
            return PDF_BC2_3_5;
        case GPUTEXTUREFORMAT_32_32_32_FLOAT:
            // 96bpp
            return PDF_96BPP;
        case GPUTEXTUREFORMAT_DXT3A:
        case GPUTEXTUREFORMAT_DXT3A_AS_1_1_1_1:
            return PDF_BC1_4;
        case GPUTEXTUREFORMAT_DXT5A:
            return PDF_BC1_4;
        case GPUTEXTUREFORMAT_CTX1:
            return PDF_BC1_4;
        case GPUTEXTUREFORMAT_32_AS_8:
        case GPUTEXTUREFORMAT_32_AS_8_8:
        case GPUTEXTUREFORMAT_16_MPEG:
        case GPUTEXTUREFORMAT_16_16_MPEG:
        case GPUTEXTUREFORMAT_8_INTERLACED:
        case GPUTEXTUREFORMAT_32_AS_8_INTERLACED:
        case GPUTEXTUREFORMAT_32_AS_8_8_INTERLACED:
        case GPUTEXTUREFORMAT_16_INTERLACED:
        case GPUTEXTUREFORMAT_16_MPEG_INTERLACED:
        case GPUTEXTUREFORMAT_16_16_MPEG_INTERLACED:
        default:
            // other
            RIP;
            return PDF_INVALID;
        }
    }

    //--------------------------------------------------------------------------------------
    // Name: GetPagePoolArrayTextureFormat
    // Desc: Returns a common D3DFORMAT for the given page data format.  This common format
    //       is used to alias all texture formats of a given bit depth to a single format.
    //       128bpp formats are always returned as linear.
    //--------------------------------------------------------------------------------------
    inline D3DFORMAT GetPagePoolArrayTextureFormat( const PageDataFormat DataFormat )
    {
        D3DFORMAT ReturnFormat = D3DFMT_A8R8G8B8;

        switch( DataFormat )
        {
        case PDF_1BPP:
            ReturnFormat = (D3DFORMAT)MAKED3DFMT(GPUTEXTUREFORMAT_1, GPUENDIAN_NONE, TRUE, GPUSIGN_ALL_UNSIGNED, GPUNUMFORMAT_FRACTION, GPUSWIZZLE_RZZZ);
            break;
        case PDF_8BPP:
            ReturnFormat = D3DFMT_L8;
            break;
        case PDF_16BPP:
            ReturnFormat = D3DFMT_A4R4G4B4;
            break;
        case PDF_32BPP:
            ReturnFormat = D3DFMT_A8R8G8B8;
            break;
        case PDF_64BPP:
            ReturnFormat = D3DFMT_A16B16G16R16;
            break;
        case PDF_128BPP:
            ReturnFormat = (D3DFORMAT)MAKELINFMT( D3DFMT_A32B32G32R32F );
            break;
        case PDF_BC1_4:
            ReturnFormat = D3DFMT_DXT1;
            break;
        case PDF_BC2_3_5:
            ReturnFormat = (D3DFORMAT)MAKELINFMT( D3DFMT_DXT5 );
            break;
        case PDF_INVALID:
        default:
            RIP;
            break;
        }

        return ReturnFormat;
    }

    //--------------------------------------------------------------------------------------
    // Name: GetAliasedPagePoolArrayTextureFormat
    // Desc: Returns a common aliased D3DFORMAT for the given page data format.  This is
    //       similar to GetPagePoolArrayTextureFormat, but the returned format here is always
    //       an uncompressed format of the proper bit depth.
    //--------------------------------------------------------------------------------------
    inline D3DFORMAT GetAliasedPagePoolArrayTextureFormat( const PageDataFormat DataFormat )
    {
        D3DFORMAT ReturnFormat = D3DFMT_A8R8G8B8;

        switch( DataFormat )
        {
        case PDF_1BPP:
            ReturnFormat = (D3DFORMAT)MAKED3DFMT(GPUTEXTUREFORMAT_1, GPUENDIAN_NONE, TRUE, GPUSIGN_ALL_UNSIGNED, GPUNUMFORMAT_FRACTION, GPUSWIZZLE_RZZZ);
            break;
        case PDF_8BPP:
            ReturnFormat = D3DFMT_L8;
            break;
        case PDF_16BPP:
            ReturnFormat = D3DFMT_A4R4G4B4;
            break;
        case PDF_32BPP:
            ReturnFormat = D3DFMT_A8R8G8B8;
            break;
        case PDF_64BPP:
            ReturnFormat = D3DFMT_A16B16G16R16;
            break;
        case PDF_128BPP:
            ReturnFormat = (D3DFORMAT)MAKELINFMT( D3DFMT_A32B32G32R32F );
            break;
        case PDF_BC1_4:
            ReturnFormat = D3DFMT_A16B16G16R16F;
            break;
        case PDF_BC2_3_5:
            ReturnFormat = (D3DFORMAT)MAKELINFMT( D3DFMT_A32B32G32R32F );
            break;
        case PDF_INVALID:
        default:
            RIP;
            break;
        }

        return ReturnFormat;
    }

    //--------------------------------------------------------------------------------------
    // Name: GetPageRenderSurfaceFormat
    // Desc: Returns a common EDRAM format for a rendertarget used to copy texel data from
    //       one resource to another, for the given page format.
    //--------------------------------------------------------------------------------------
    inline D3DFORMAT GetPageRenderSurfaceFormat( const PageDataFormat DataFormat )
    {
        D3DFORMAT ReturnFormat = D3DFMT_A8R8G8B8;

        switch( DataFormat )
        {
        case PDF_16BPP:
            ReturnFormat = D3DFMT_A8R8G8B8;
            break;
        case PDF_32BPP:
            ReturnFormat = D3DFMT_A8R8G8B8;
            break;
        case PDF_64BPP:
            ReturnFormat = D3DFMT_A16B16G16R16_EDRAM;
            break;
        case PDF_8BPP:
            ReturnFormat = D3DFMT_A8R8G8B8;
            break;
        case PDF_BC1_4:
            ReturnFormat = D3DFMT_A16B16G16R16F;
            break;
        case PDF_BC2_3_5:
            // surface won't be used; memexport only
            ReturnFormat = D3DFMT_A8R8G8B8;
            break;
        case PDF_128BPP:
            // surface won't be used; memexport only
            ReturnFormat = D3DFMT_A8R8G8B8;
            break;
        case PDF_INVALID:
            ReturnFormat = (D3DFORMAT)0;
            break;
        default:
            ReturnFormat = (D3DFORMAT)0;
            break;
        }

        return ReturnFormat;
    }

    //--------------------------------------------------------------------------------------
    // Name: MemAllocPhysical
    // Desc: Wrapper around XPhysicalAlloc that asks for 4K aligned write combined physical
    //       memory.
    //--------------------------------------------------------------------------------------
    inline VOID* MemAllocPhysical( UINT SizeBytes )
    {
        VOID* pBuffer = XPhysicalAlloc( SizeBytes, MAXULONG_PTR, GPU_TEXTURE_ALIGNMENT, PAGE_READWRITE | PAGE_WRITECOMBINE );
        return pBuffer;
    }

    //--------------------------------------------------------------------------------------
    // Name: MemFreePhysical
    // Desc: Wrapper around XPhysicalFree.
    //--------------------------------------------------------------------------------------
    inline VOID MemFreePhysical( VOID* pBuffer )
    {
        XPhysicalFree( pBuffer );
    }

    //--------------------------------------------------------------------------------------
    // Name: CreateZeroedTexture2D
    // Desc: Creates a zero-filled texture 2D without using the D3D device - that is, it
    //       manually allocates physical memory and manually creates a texture header.
    //       Note that if the pTextureBuffer parameter is not NULL, then this method will
    //       use that buffer instead of allocating a new buffer.
    //--------------------------------------------------------------------------------------
    inline VOID CreateZeroedTexture2D( DWORD Width, DWORD Height, DWORD Levels, D3DFORMAT Format, D3DBaseTexture** ppTexture, VOID* pTextureBuffer = NULL, UINT BufferSize = 0 )
    {
        D3DTexture* pTexture = new D3DTexture();
        UINT BaseSize = 0, MipSize = 0;
        XGSetTextureHeader( Width, Height, Levels, 0, Format, 0, 0, XGHEADER_CONTIGUOUS_MIP_OFFSET, 0, pTexture, &BaseSize, &MipSize );

        if( MipSize > 0 )
        {
            BaseSize = XGNextMultiple( BaseSize, GPU_TEXTURE_ALIGNMENT );
        }
        UINT TotalSize = BaseSize + MipSize;

        if( pTextureBuffer == NULL )
        {
            pTextureBuffer = MemAllocPhysical( TotalSize );
            if( Format == D3DFMT_INDEXMAP )
            {
                memset( pTextureBuffer, 0xFF, TotalSize );
            }
            else
            {
                ZeroMemory( pTextureBuffer, TotalSize );
            }
        }
        else
        {
            ASSERT( BufferSize >= TotalSize );
        }

        XGOffsetBaseTextureAddress( pTexture, pTextureBuffer, pTextureBuffer );

        *ppTexture = pTexture;
    }

    //--------------------------------------------------------------------------------------
    // Name: CreateZeroedArrayTexture
    // Desc: Creates a zero-filled texture 2D array without using the D3D device - that is, it
    //       manually allocates physical memory and manually creates a texture header.
    //--------------------------------------------------------------------------------------
    inline VOID CreateZeroedArrayTexture( DWORD Width, DWORD Height, DWORD SliceCount, DWORD LevelCount, D3DFORMAT Format, D3DBaseTexture** ppArrayTexture )
    {
        D3DArrayTexture* pArrayTexture = new D3DArrayTexture();
        UINT BaseSize = 0, MipSize = 0;
        XGSetArrayTextureHeader( Width, Height, SliceCount, LevelCount, 0, Format, 0, 0, XGHEADER_CONTIGUOUS_MIP_OFFSET, 0, pArrayTexture, &BaseSize, &MipSize );

        if( MipSize > 0 )
        {
            BaseSize = XGNextMultiple( BaseSize, GPU_TEXTURE_ALIGNMENT );
        }
        UINT TotalSize = BaseSize + MipSize;

        VOID* pTextureBuffer = NULL;
        pTextureBuffer = MemAllocPhysical( TotalSize );
        if( Format == D3DFMT_INDEXMAP )
        {
            memset( pTextureBuffer, 0xFF, TotalSize );
        }
        else
        {
            ZeroMemory( pTextureBuffer, TotalSize );
        }

        XGOffsetBaseTextureAddress( pArrayTexture, pTextureBuffer, pTextureBuffer );

        *ppArrayTexture = pArrayTexture;
    }

    //--------------------------------------------------------------------------------------
    // Name: FreeTexture
    // Desc: Frees a texture that was created using CreateZeroedTexture2D or 
    //       CreateZeroedArrayTexture.
    //--------------------------------------------------------------------------------------
    inline VOID FreeTexture( D3DBaseTexture* pTexture )
    {
        ASSERT( pTexture != NULL );

        UINT BaseData;
        UINT MipData;
        XGGetTextureLayout( pTexture, &BaseData, NULL, NULL, NULL, 0, &MipData, NULL, NULL, NULL, 0 );
        VOID* pBuffer = (VOID*)BaseData;

        MemFreePhysical( pBuffer );

        delete pTexture;
    }

    //--------------------------------------------------------------------------------------
    // Name: CreateAliasedArrayTexture
    // Desc: Creates a new texture header that points to the same data as an existing texture
    //       header, except the format is changed to a different format.
    //--------------------------------------------------------------------------------------
    inline VOID CreateAliasedArrayTexture( DWORD Width, DWORD Height, DWORD SliceCount, D3DFORMAT Format, D3DBaseTexture* pExistingTexture, D3DBaseTexture** ppArrayTexture )
    {
        D3DArrayTexture* pArrayTexture = new D3DArrayTexture();
        UINT BaseSize = 0, MipSize = 0;
        XGSetArrayTextureHeader( Width, Height, SliceCount, 1, 0, Format, 0, 0, XGHEADER_CONTIGUOUS_MIP_OFFSET, 0, pArrayTexture, &BaseSize, &MipSize );

        pArrayTexture->Format.BaseAddress = pExistingTexture->Format.BaseAddress;
        pArrayTexture->Format.MipAddress = pExistingTexture->Format.MipAddress;

        *ppArrayTexture = pArrayTexture;
    }

    //--------------------------------------------------------------------------------------
    // Name: FreeAliasedTexture
    // Desc: Frees a texture header that was created with CreateAliasedArrayTexture.
    //--------------------------------------------------------------------------------------
    inline VOID FreeAliasedTexture( D3DBaseTexture* pTexture )
    {
        delete pTexture;
    }

    //--------------------------------------------------------------------------------------
    // Name: AlignToResolveRect
    // Desc: Aligns a D3D rect to resolve alignment.
    //--------------------------------------------------------------------------------------
    inline D3DRECT AlignToResolveRect( RECT InputRect, SIZE SurfaceSize )
    {
        D3DRECT OutputRect;
        OutputRect.x1 = ( InputRect.left / GPU_RESOLVE_ALIGNMENT ) * GPU_RESOLVE_ALIGNMENT;
        OutputRect.y1 = ( InputRect.top / GPU_RESOLVE_ALIGNMENT ) * GPU_RESOLVE_ALIGNMENT;

        if( InputRect.right >= SurfaceSize.cx )
        {
            OutputRect.x2 = InputRect.right;
        }
        else
        {
            OutputRect.x2 = min( (UINT)SurfaceSize.cx, XGNextMultiple( InputRect.right, GPU_RESOLVE_ALIGNMENT ) );
        }

        if( InputRect.bottom >= SurfaceSize.cy )
        {
            OutputRect.y2 = InputRect.bottom;
        }
        else
        {
            OutputRect.y2 = min( (UINT)SurfaceSize.cy, XGNextMultiple( InputRect.bottom, GPU_RESOLVE_ALIGNMENT ) );
        }

        return OutputRect;
    }

    //--------------------------------------------------------------------------------------
    // Name: MakeD3DRect
    // Desc: Converts a Windows RECT struct to a D3DRECT.
    //--------------------------------------------------------------------------------------
    inline D3DRECT MakeD3DRect( RECT Rect )
    {
        return *(D3DRECT*)&Rect;
    }
}
