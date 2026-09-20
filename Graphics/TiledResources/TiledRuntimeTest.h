//--------------------------------------------------------------------------------------
// TiledRuntimeTest.h
//
// A set of methods and defines that are useful in testing the tiled resource system.
// Helper methods for texture pattern filling are present, as well as a trace system
// that records internal events from the tiled runtime.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "TiledResourceCommon.h"
#include "PhysicalPageManager.h"
#include "TypedPagePool.h"
#include "TiledResourceBase.h"

namespace TiledRuntimeTest
{
    using namespace TiledRuntime;

    static const DWORD g_MipColors[12] = 
    {
        0xFFFF0000,
        0xFFFFFF00,
        0xFF00FF00,
        0xFF00FFFF,
        0xFF0000FF,
        0xFFFF00FF,
        0xFF800000,
        0xFF808000,
        0xFF008000,
        0xFF008080,
        0xFF000080,
        0xFF800080
    };

    inline WORD Color32To565( DWORD Color )
    {
        WORD Red = D3DCOLOR_GETRED( Color );
        WORD Green = D3DCOLOR_GETGREEN( Color );
        WORD Blue = D3DCOLOR_GETBLUE( Color );
        return ( Blue >> 3 ) | ( ( Green >> 2 ) << 5 ) | ( ( Red >> 3 ) << 11 );
    }

    inline WORD Color32To4444( DWORD Color )
    {
        WORD Red = D3DCOLOR_GETRED( Color );
        WORD Green = D3DCOLOR_GETGREEN( Color );
        WORD Blue = D3DCOLOR_GETBLUE( Color );
        WORD Alpha = D3DCOLOR_GETALPHA( Color );
        return ( Blue >> 4 ) | ( ( Green >> 4 ) << 4 ) | ( ( Red >> 4 ) << 8 ) | ( ( Alpha >> 4 ) << 12 );
    }

    inline BYTE Color32To8( DWORD Color )
    {
        BYTE Red = D3DCOLOR_GETRED( Color );
        BYTE Green = D3DCOLOR_GETGREEN( Color );
        BYTE Blue = D3DCOLOR_GETBLUE( Color );
        return max( Red, max( Green, Blue ) );
    }

    inline DWORD Color32To210ABGR( DWORD Color )
    {
        WORD Red = D3DCOLOR_GETRED( Color );
        WORD Green = D3DCOLOR_GETGREEN( Color );
        WORD Blue = D3DCOLOR_GETBLUE( Color );
        WORD Alpha = D3DCOLOR_GETALPHA( Color );
        return ( Red << 2 ) | ( ( Green << 2 ) << 10 ) | ( ( Blue << 2 ) << 20 ) | ( ( Alpha >> 6 ) << 30 );
    }

    inline DWORD Color32To210ARGB( DWORD Color )
    {
        WORD Red = D3DCOLOR_GETRED( Color );
        WORD Green = D3DCOLOR_GETGREEN( Color );
        WORD Blue = D3DCOLOR_GETBLUE( Color );
        WORD Alpha = D3DCOLOR_GETALPHA( Color );
        return ( Blue << 2 ) | ( ( Green << 2 ) << 10 ) | ( ( Red << 2 ) << 20 ) | ( ( Alpha >> 6 ) << 30 );
    }

    inline UINT64 Color32To64ABGR( DWORD Color )
    {
        UINT64 Red = D3DCOLOR_GETRED( Color );
        UINT64 Green = D3DCOLOR_GETGREEN( Color );
        UINT64 Blue = D3DCOLOR_GETBLUE( Color );
        UINT64 Alpha = D3DCOLOR_GETALPHA( Color );
        return ( Alpha << 8 ) | ( ( Blue << 8 ) << 16 ) | ( ( Green << 8 ) << 32 ) | ( ( Red << 8 ) << 48 );
    }

    inline XMFLOAT4 Color32To128ABGR( DWORD Color )
    {
        FLOAT Red = (FLOAT)D3DCOLOR_GETRED( Color ) / 255.0f;
        FLOAT Green = (FLOAT)D3DCOLOR_GETGREEN( Color ) / 255.0f;
        FLOAT Blue = (FLOAT)D3DCOLOR_GETBLUE( Color ) / 255.0f;
        FLOAT Alpha = (FLOAT)D3DCOLOR_GETALPHA( Color ) / 255.0f;
        return XMFLOAT4( Red, Green, Blue, Alpha );
    }

    class TestTileData
    {
    public:
        static VOID FillRect32Bit( BYTE* pDestBits, DWORD Width, DWORD Height, DWORD PitchBytes, DWORD ColorARGB, BOOL Checker );
        static VOID FillRect16Bit( BYTE* pDestBits, DWORD Width, DWORD Height, DWORD PitchBytes, DWORD ColorARGB, BOOL Checker );
    };

    class Trace
    {
    public:
        static VOID CreatePage( PhysicalPageID PageID, PageDataFormat Format );
        static VOID CreateTexture2D( INT ResourceID, INT Width, INT Height, D3DFORMAT Format );
        static VOID MovePage( PhysicalPageID PageID, PageDataFormat SrcFormat, PageDataFormat DestFormat );
        static VOID FillPage( PhysicalPageID PageID, PageDataFormat Format );
        static VOID MapPage( VirtualPageID VPageID, PhysicalPageID PageID );
        static VOID QueueMapPageUpdate( VirtualPageID VPageID, PhysicalPageID PageID );
        static VOID RetireMapPageUpdate( VirtualPageID VPageID, PhysicalPageID PageID );
        static VOID UpdatePageBorder( PhysicalPageID CenterPage, PhysicalPageID BorderPage, PageNeighbors BorderLocation );
        static VOID PageCreateFailure( PhysicalPageID PageID, PageDataFormat Format );
        static VOID AddPageToPool( PhysicalPageID PageID, INT PoolIndex, PageDataFormat PoolFormat );
        static VOID RemovePageFromPool( PhysicalPageID PageID, INT PoolIndex, PageDataFormat PoolFormat );
    };
}
