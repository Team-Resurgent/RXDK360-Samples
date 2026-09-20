//--------------------------------------------------------------------------------------
// CatmullRomColorSpline.h
//
// Implements a spline of color values permitting interpolating along the spline.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef CATMULL_ROM_COLOR_SPLINE_H
#define CATMULL_ROM_COLOR_SPLINE_H

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------

class DepthValue;
class RGBAvValue;

//--------------------------------------------------------------------------------------
// CatmullRom color spline class.
//--------------------------------------------------------------------------------------
class CatmullRomColorSpline
{
public:
                CatmullRomColorSpline( const RGBAValue* pValues, const UINT count );
    RGBAValue   Interpolate( const DepthValue& depth ) const;
    RGBAValue   Interpolate( const FLOAT t ) const;

private:
    const RGBAValue* m_pControlPoints;
    UINT m_ControlPointCount;

};

#endif
