//--------------------------------------------------------------------------------------
// Visualization.h
//
// Declares functions used to visualize the output of the turn detection filter
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

VOID InitVisualization();

VOID UpdateVisualization( const FLOAT fTurnAngleInDegrees, const BOOL bSearchForFlip, const BOOL bFoundFlip );

VOID RenderVisualization( D3DDevice* pD3DDevice );
