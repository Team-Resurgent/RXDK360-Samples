//--------------------------------------------------------------------------------------
// PageLoaders.h
//
// Implements three different modules that load data into tiled resource tiles, and
// unload tiles as necessary.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <xtl.h>
#include "TitleResidencyManager.h"
#include "TiledFileFormat.h"

#pragma warning (disable: 4324)

//--------------------------------------------------------------------------------------
// Name: ColorTileLoader
// Desc: Implements a tile loader that fills tiles with a checkerboarded solid color,
//       the color chosen by the mip level.
//--------------------------------------------------------------------------------------
class ColorTileLoader : public ITileLoader
{
protected:
    struct LoaderContext
    {
        BYTE* pBuffer;
    };

public:
    BOOL m_Grid;

    ColorTileLoader();

    virtual VOID* CreateThreadContext();
    virtual VOID DestroyThreadContext( VOID* pThreadContext );

    virtual HRESULT LoadAndMapTile( TrackedTileID* pTileID, VOID* pThreadContext );
    virtual HRESULT UnmapTile( TrackedTileID* pTileID, VOID* pThreadContext );
};

//--------------------------------------------------------------------------------------
// Name: MandelbrotTileLoader
// Desc: Implements a tile loader that fills tiles with sections of the Mandelbrot or
//       Julia fractals.
//--------------------------------------------------------------------------------------
class MandelbrotTileLoader : public ITileLoader
{
protected:
    struct LoaderContext
    {
        BYTE* pBuffer;
        BYTE* pUncompressedBuffer;
    };

public:
    XMVECTOR m_JuliaCoordinate;
    BOOL m_Julia;

    BOOL m_DebugColoring;
    BOOL m_Grid;

    MandelbrotTileLoader();

    virtual VOID* CreateThreadContext();
    virtual VOID DestroyThreadContext( VOID* pThreadContext );

    virtual HRESULT LoadAndMapTile( TrackedTileID* pTileID, VOID* pThreadContext );
    virtual HRESULT UnmapTile( TrackedTileID* pTileID, VOID* pThreadContext );

protected:
    VOID CreateMandelbrot( BYTE* pBuffer, UINT TileX, UINT TileY, UINT ArraySlice, const D3DTILED_SURFACE_DESC& MipLevelDesc, const XMVECTOR vQuiltScale, const XMVECTOR vQuiltOffset );
};

//--------------------------------------------------------------------------------------
// Name: TiledFileLoader
// Desc: Implements a tile loader that fills tiles with data that comes from a file.
//--------------------------------------------------------------------------------------
class TiledFileLoader : public ITileLoader
{
protected:
    struct LoaderContext
    {
        BYTE* pBuffer;
    };

    HANDLE m_hFile;
    CRITICAL_SECTION m_FileAccessCritSec;

    TiledContent::TILEDFILE_HEADER m_Header;
    TiledContent::TILEDFILE_SUBRESOURCE* m_pSubresources;

    TiledContent::TILEDFILE_PAGEDATA_LOCATOR* m_pFlatIndices;
    TiledContent::TILEDFILE_PAGEDATA_LOCATOR** m_ppTileIndexes;

    BYTE* m_pDefaultTile;
    D3DTILED_PHYSICAL_ADDRESS m_DefaultPhysicalTile;

public:
    TiledFileLoader();

    HRESULT LoadFile( const CHAR* strFileName );

    HRESULT CreateTiledTexture2D( D3DTiledResourceDevice* pTiledResourceDevice, D3DTiledTexture** ppTexture );

    virtual VOID* CreateThreadContext();
    virtual VOID DestroyThreadContext( VOID* pThreadContext );

    virtual BOOL TileNeedsUniquePhysicalTile( TrackedTileID* pTileID );
    virtual HRESULT LoadAndMapTile( TrackedTileID* pTileID, VOID* pThreadContext );
    virtual HRESULT UnmapTile( TrackedTileID* pTileID, VOID* pThreadContext );

protected:
    TiledContent::TILEDFILE_PAGEDATA_LOCATOR FindTile( UINT Subresource, UINT TileX, UINT TileY, UINT* pBlockOffset ) const;
    HRESULT LoadTile( TiledContent::TILEDFILE_PAGEDATA_LOCATOR Locator, UINT TileOffset, VOID* pDestBuffer );
};
