//--------------------------------------------------------------------------------------
// DepthValue.h
//
// Class representing a 16 bit depth value from NUI's depth map.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef DEPTH_VALUE_H
#define DEPTH_VALUE_H

//--------------------------------------------------------------------------------------
// Depth value class
//--------------------------------------------------------------------------------------
class DepthValue
{
public:

    DepthValue()
    {
    }

    DepthValue( const UINT value ) : m_Value( ( USHORT )value )
    {
    }

    DepthValue( const UINT depth, const UINT mask )
    {
        m_Value = ( USHORT )( ( depth << DEPTH_SHIFT ) | ( mask & SEGMENTATION_MASK ) );
    }

    __forceinline UINT  RawValue() const
    {
        return m_Value;
    }

    __forceinline UINT  SegmentationValue() const
    {
        return ( UINT )( m_Value & SEGMENTATION_MASK );
    }

    __forceinline UINT  IntDepth() const
    {
        return ( m_Value & DEPTH_MASK ) >> DEPTH_SHIFT;
    }

    __forceinline UINT  ByteDepth() const
    {
        return ( ( m_Value & DEPTH_MASK ) >> DEPTH_TO_BYTE_SHIFT );
    }

    __forceinline FLOAT FloatDepth() const
    {
        return ( IntDepth() / ( FLOAT )MAX_DEPTH );     // Load hit store
    }

    enum DepthValueConstants
    {
        MAX_DEPTH           = 0xfff,
        SEGMENTATION_MASK   = 0x7,
        DEPTH_MASK          = 0x7ff8,
        DEPTH_SHIFT         = 3,
        DEPTH_TO_BYTE_SHIFT = 7
    };

private:
    USHORT m_Value;

};

#endif
