//--------------------------------------------------------------------------------------
// RingBuffer.h
//
// Implementation of a simple ringbuffer that holds NUI skeleton data
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifdef _XBOX
#include <xtl.h>
#else
#include <windows.h>
#endif

#include <XnaMath.h>

#ifdef _XBOX
#include <nuiapi.h>
#else
#include <NuiTools.h>
#endif

#include "Common.h"

namespace ATGGestureDetector
{

#define RING_BUFFER_SIZE        NUM_HISTORY_FRAMES


//--------------------------------------------------------------------------------------
// Name: RingBuffer
// Desc: Simple ringbuffer that holds nui skeleton data
//--------------------------------------------------------------------------------------

class RingBuffer
{
public:
    RingBuffer()
    {
        Clear();
    }

    inline VOID Clear()
    {
        m_uHead = 0;
        m_uTail = RING_BUFFER_SIZE - 1;

#ifdef _XBOX
        XMemSet( m_Buffer, 0, RING_BUFFER_SIZE * sizeof( NUI_SKELETON_DATA ) );
#else
        memset( m_Buffer, 0, RING_BUFFER_SIZE * sizeof( NUI_SKELETON_DATA ) );
#endif
    }

    inline VOID AddToFront( const NUI_SKELETON_DATA* pSkeletonData )
    {
        DecrementIndex( m_uHead );
        DecrementIndex( m_uTail );

#ifdef _XBOX
        XMemCpy( &m_Buffer[ m_uHead ], pSkeletonData, sizeof( NUI_SKELETON_DATA ) );
#else
        memcpy( &m_Buffer[ m_uHead ], pSkeletonData, sizeof( NUI_SKELETON_DATA ) );
#endif
    }

    inline NUI_SKELETON_DATA* GetCurrentSkeleton()
    {
        return &m_Buffer[ m_uHead ];
    }

    inline NUI_SKELETON_DATA* GetAt( const UINT uIndex )
    {
        return &m_Buffer[ uIndex ];
    }

    inline UINT GetCurrentFrameIndex() const
    {
        return m_uHead;
    }

    inline UINT GetPreviousFrameIndex( const UINT uCurrentIndex )
    {
        UINT uPreviousFrameIndex = uCurrentIndex;
        IncrementIndex( uPreviousFrameIndex );
        return uPreviousFrameIndex;
    }

    VOID Splat( const NUI_SKELETON_DATA* pSkeletonData )
    {
        for ( UINT i = 0; i < RING_BUFFER_SIZE; i++ )
        {
#ifdef _XBOX
            XMemCpy( &m_Buffer[ i ], pSkeletonData, sizeof( NUI_SKELETON_DATA ) );
#else
            memcpy( &m_Buffer[ i ], pSkeletonData, sizeof( NUI_SKELETON_DATA ) );
#endif
        }       
    }

protected:
    UINT                 m_uHead;
    UINT                 m_uTail;
    NUI_SKELETON_DATA    m_Buffer[ RING_BUFFER_SIZE ];

    inline VOID IncrementIndex( UINT& uIndex )
    {
        uIndex++;
        if ( uIndex >= RING_BUFFER_SIZE )
        {
            uIndex = 0;
        }
    }

    inline VOID DecrementIndex( UINT& uIndex )
    {
        uIndex--;
        if ( uIndex >= RING_BUFFER_SIZE )
        {
            uIndex = RING_BUFFER_SIZE - 1;
        }
    }
};

}