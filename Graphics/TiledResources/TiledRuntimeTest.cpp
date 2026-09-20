//--------------------------------------------------------------------------------------
// TiledRuntimeTest.cpp
//
// A set of methods and defines that are useful in testing the tiled resource system.
// Helper methods for texture pattern filling are present, as well as a trace system
// that records internal events from the tiled runtime.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "TiledRuntimeTest.h"

#pragma warning( disable : 6326 ) //"Potential comparison of a constant with another constant"

#ifdef _DEBUG
static const UINT TRACE_SPEW = 2;
#else
static const UINT TRACE_SPEW = 0;
#endif

namespace TiledRuntimeTest
{
    static const CHAR* g_strNeighborNames[] =
    {
        "Top",
        "Bottom",
        "Left",
        "Right",
        "Top Left",
        "Bottom Right",
        "Top Right",
        "Bottom Left"
    };
    C_ASSERT( ARRAYSIZE(g_strNeighborNames) == PN_COUNT );

    VOID TestTileData::FillRect32Bit( BYTE* pDestBits, DWORD Width, DWORD Height, DWORD PitchBytes, DWORD ColorARGB, BOOL Checker )
    {
        for( DWORD y = 0; y < Height; ++y )
        {
            DWORD* pRow = (DWORD*)pDestBits;
            for( DWORD x = 0; x < Width; ++x )
            {
                DWORD PixelColor = ColorARGB;
                if( Checker && ( x + y ) % 2 == 1 )
                {
                    DWORD Red = ( ( PixelColor >> 16 ) & 0xFF ) * 2 / 4;
                    DWORD Green = ( ( PixelColor >> 8 ) & 0xFF ) * 2 / 4;
                    DWORD Blue = ( ( PixelColor >> 0 ) & 0xFF ) * 2 / 4;
                    PixelColor = D3DCOLOR_ARGB( 0xFF, Red, Green, Blue );
                }
                pRow[x] = PixelColor;
            }
            pDestBits += PitchBytes;
        }
    }

    VOID TestTileData::FillRect16Bit( BYTE* pDestBits, DWORD Width, DWORD Height, DWORD PitchBytes, DWORD ColorARGB, BOOL Checker )
    {
        for( DWORD y = 0; y < Height; ++y )
        {
            WORD* pRow = (WORD*)pDestBits;
            for( DWORD x = 0; x < Width; ++x )
            {
                DWORD PixelColor = ColorARGB;
                if( Checker && ( x + y ) % 2 == 1 )
                {
                    DWORD Red = ( ( PixelColor >> 16 ) & 0xFF ) * 2 / 4;
                    DWORD Green = ( ( PixelColor >> 8 ) & 0xFF ) * 2 / 4;
                    DWORD Blue = ( ( PixelColor >> 0 ) & 0xFF ) * 2 / 4;
                    PixelColor = D3DCOLOR_ARGB( 0xFF, Red, Green, Blue );
                }
                pRow[x] = Color32To565( PixelColor );
            }
            pDestBits += PitchBytes;
        }
    }

    const CHAR* GetFormatName( PageDataFormat Format )
    {
        static const CHAR* s_FormatNames[] =
        {
            "INVALID",
            "1BPP",
            "8BPP",
            "16BPP",
            "32BPP",
            "64BPP",
            "128BPP",
            "BC1_4",
            "BC2_3_5",
            "96BPP",
        };

        ASSERT( Format >= PDF_INVALID && Format < PDF_COUNT );
        return s_FormatNames[Format];
    }

    VOID SpewThreadID()
    {
        if ( TRACE_SPEW )
        {
            DebugSpew( "(%08x)TRACE:", GetCurrentThreadId() );
        }
    }

    VOID Trace::CreatePage( PhysicalPageID PageID, PageDataFormat Format )
    {
        if( TRACE_SPEW > 1 )
        {
            SpewThreadID();
            DebugSpew( "Creating page %I64d in format %s\n", PageID, GetFormatName( Format ) );
        }
    }

    VOID Trace::PageCreateFailure( PhysicalPageID PageID, PageDataFormat Format )
    {
        if( TRACE_SPEW )
        {
            SpewThreadID();
            DebugSpew( "Could not create page %I64d in format %s\n", PageID, GetFormatName( Format ) );
        }
    }

    VOID Trace::CreateTexture2D( INT ResourceID, INT Width, INT Height, D3DFORMAT Format )
    {
        if( TRACE_SPEW )
        {
            UINT GpuFormat = ( Format & D3DFORMAT_TEXTUREFORMAT_MASK ) >> D3DFORMAT_TEXTUREFORMAT_SHIFT;
            SpewThreadID();
            DebugSpew( "Creating resource ID %d: tiled texture2D size (%d, %d) gpuformat %d\n", ResourceID, Width, Height, GpuFormat );
        }
    }

    VOID Trace::MovePage( PhysicalPageID PageID, PageDataFormat SrcFormat, PageDataFormat DestFormat )
    {
        if( TRACE_SPEW )
        {
            SpewThreadID();
            DebugSpew( "Moving page %I64d from format %s to format %s\n", PageID, GetFormatName( SrcFormat ), GetFormatName( DestFormat ) );
        }
    }

    VOID Trace::FillPage( PhysicalPageID PageID, PageDataFormat Format )
    {
        if( TRACE_SPEW )
        {
            SpewThreadID();
            DebugSpew( "Filling page %I64d in format %s\n", PageID, GetFormatName( Format ) );
        }
    }

    VOID Trace::MapPage( VirtualPageID VPageID, PhysicalPageID PageID )
    {
        if( TRACE_SPEW )
        {
            SpewThreadID();
            DebugSpew( "Mapping page %I64d to resource ID %I64d, slice %I64d, mip %I64d, X %I64d, Y %I64d\n", PageID, VPageID.ResourceID, VPageID.ArraySlice, VPageID.MipLevel, VPageID.PageX, VPageID.PageY );
        }
    }

    VOID Trace::QueueMapPageUpdate( VirtualPageID VPageID, PhysicalPageID PageID )
    {
        if( TRACE_SPEW )
        {
            SpewThreadID();
            DebugSpew( "Queueing request to map page %I64d to resource ID %I64d, slice %I64d, mip %I64d, X %I64d, Y %I64d\n", PageID, VPageID.ResourceID, VPageID.ArraySlice, VPageID.MipLevel, VPageID.PageX, VPageID.PageY );
        }
    }

    VOID Trace::RetireMapPageUpdate( VirtualPageID VPageID, PhysicalPageID PageID )
    {
        if( TRACE_SPEW )
        {
            SpewThreadID();
            DebugSpew( "Retiring request to map page %I64d to resource ID %I64d, slice %I64d, mip %I64d, X %I64d, Y %I64d\n", PageID, VPageID.ResourceID, VPageID.ArraySlice, VPageID.MipLevel, VPageID.PageX, VPageID.PageY );
        }
    }

    VOID Trace::UpdatePageBorder( PhysicalPageID CenterPage, PhysicalPageID BorderPage, PageNeighbors BorderLocation )
    {
        if( TRACE_SPEW > 1 )
        {
            SpewThreadID();
            DebugSpew( "Setting %s border of page %I64d with contents from page %I64d\n", g_strNeighborNames[BorderLocation], CenterPage, BorderPage );
        }
    }

    VOID Trace::AddPageToPool( PhysicalPageID PageID, INT PoolIndex, PageDataFormat PoolFormat )
    {
        if( TRACE_SPEW )
        {
            SpewThreadID();
            DebugSpew( "Adding page %I64d to %s pool at index %d\n", PageID, GetFormatName( PoolFormat ), PoolIndex );
        }
    }

    VOID Trace::RemovePageFromPool( PhysicalPageID PageID, INT PoolIndex, PageDataFormat PoolFormat )
    {
        if( TRACE_SPEW )
        {
            SpewThreadID();
            DebugSpew( "Removing page %I64d from %s pool at index %d\n", PageID, GetFormatName( PoolFormat ), PoolIndex );
        }
    }
}
