//--------------------------------------------------------------------------------------
// HandSearch.h
//
// Given the position of the hand and the depth map it returns the hand's voxels
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "HOCDetectorInternal.h"


//----------------------------------------------------------------------------------
// Name: FindHand
// Desc: Returns hand voxels. This could be a point to tune in your application.
//----------------------------------------------------------------------------------
void    FindHand( std::vector< Voxel >& voxels,
                  FLOAT& fHandSizeAtDistance,
                  FLOAT fPlayerSize,
                  BOOL bElbowTracked,
                  BOOL bWristTracked,
                  _In_ const USHORT* pDepthMap,
                  XMVECTOR hand,
                  XMVECTOR wrist,
                  XMVECTOR elbow );

