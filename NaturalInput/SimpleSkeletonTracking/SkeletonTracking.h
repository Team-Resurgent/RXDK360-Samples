//--------------------------------------------------------------------------------------
// SkeletonTracking.h
//
// Declares functions used for simple skeleton tracking
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


#pragma once

HRESULT InitializeSkeletonTracking( D3DDevice* pd3dDevice );

VOID UpdateSkeletonTracking( D3DDevice* pd3dDevice );

VOID VisualizeSkeletonTracking( D3DDevice* pd3dDevice, BOOL* pbTracking );

VOID SetSmoothingState( BOOL state );
BOOL GetSmoothingState();

VOID SetTiltCorrectionState( BOOL state );
BOOL GetTiltCorrectionState();