//--------------------------------------------------------------------------------------
// TurnDetectionFilter.h
//
// Filter for turn detection
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

VOID ResetTurnDetectionFilter();
VOID ResetTurnDetectionFilter( const UINT uSkeletonIdx );
VOID UpdateTurnDetectionFilter( NUI_SKELETON_FRAME* pSkeletonFrame );

FLOAT GetTurnAngleInDegrees( const UINT uSkeletonIdx );
UINT GetNumFlips( const UINT uSkeletonIdx );
VOID GetDebugInfo( const UINT uSkeletonIdx, BOOL& bSearchForFlip, BOOL& bFoundFlip );

