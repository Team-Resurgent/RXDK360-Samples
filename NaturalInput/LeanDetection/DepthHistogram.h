//-----------------------------------------------------------------------------
// DepthProcessor.h
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#pragma once

#include "Detector.h"


namespace Detector
{
    //----------------------------------------------------------------------------------
    // Name: DepthProcessor
    // Desc: Encapsulates histogram building and scanning
    //----------------------------------------------------------------------------------
    class DepthProcessor
    {
    public:
        void    Reset();
        BOOL    BuildLayers( const DepthVector& depth, Vector< Detail::DepthSlice >& layers, MaskVector& mask, BOOL bFreezeFrame );

    private:
        DepthVector     m_average;

        void    FloodFill3d( MaskVector& markedMask, DWORD x, DWORD y, const DepthVector& depth, DWORD dwPaintId, Detail::DepthSlice& layer );
        void    IslandFromFloodFill3d( Vector< Detail::DepthSlice >& layers, MaskVector& markedMask, DWORD dwSeedX, DWORD dwSeedY, const DepthVector& depth );
    };
}
