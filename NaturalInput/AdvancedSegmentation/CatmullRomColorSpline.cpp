//--------------------------------------------------------------------------------------
// CatmullRomColorSpline.cpp
//
// Catmull Rom spline with colors as control points. Interpolating along the spline
// gives interpolated colors. Useful for showing up > 8 bit values eg depth.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <assert.h>

#include "RGBAValue.h"
#include "DepthValue.h"
#include "CatmullRomColorSpline.h"


//--------------------------------------------------------------------------------------
// Name: CatmullRomColorSpline()
// Constructor taking list of RGBValues to set up spline in color space
//--------------------------------------------------------------------------------------
CatmullRomColorSpline::CatmullRomColorSpline( const RGBAValue* pValues, const UINT count )
{
    assert( pValues != NULL );
    assert( count >= 4 );           // No CR spline with < 4
    m_pControlPoints = pValues;
    m_ControlPointCount = count;
}

//--------------------------------------------------------------------------------------
// Name: Interpolate()
// Given a float value, returns a color from along the spline
//--------------------------------------------------------------------------------------
RGBAValue CatmullRomColorSpline::Interpolate( const FLOAT fInputT ) const
{
    assert( fInputT >= 0.0f && fInputT <= 1.0f );

    const UINT interpolableSegments = m_ControlPointCount - 3;
    const FLOAT fTotal = fInputT * interpolableSegments;
    const UINT index0 = ( UINT )fTotal;
    const FLOAT t = fTotal - index0;

    const RGBAValue c0 = m_pControlPoints[index0];
    const RGBAValue c1 = m_pControlPoints[index0 + 1];
    const RGBAValue c2 = m_pControlPoints[index0 + 2];
    const RGBAValue c3 = m_pControlPoints[index0 + 3];

    // Standard Catmull-Rom weighting factors
    // 0 - 1t + 2t2 - 1t3
    // 2 + 0t - 5t2 + 3t3
    // 0 + 1t - 4t2 - 3t3
    // 0 + 0t - 1t2 + 1t3

    const FLOAT t2 = t * t;
    const FLOAT t3 = t2 * t;

    const FLOAT p0 = ( ( 1.0f * 0.0f ) + ( t * -1.0f ) + ( t2 *  2.0f ) + ( t3 * -1.0f ) ) * 0.5f;
    const FLOAT p1 = ( ( 1.0f * 2.0f ) + ( t *  0.0f ) + ( t2 * -5.0f ) + ( t3 *  3.0f ) ) * 0.5f;
    const FLOAT p2 = ( ( 1.0f * 0.0f ) + ( t *  1.0f ) + ( t2 *  4.0f ) + ( t3 * -3.0f ) ) * 0.5f;
    const FLOAT p3 = ( ( 1.0f * 0.0f ) + ( t *  0.0f ) + ( t2 * -1.0f ) + ( t3 *  1.0f ) ) * 0.5f;

    const FLOAT r = ( p0 * c0.GetRed() )   + ( p1 * c1.GetRed() )   + ( p2 * c2.GetRed() )   + ( p3 * c3.GetRed() );
    const FLOAT g = ( p0 * c0.GetGreen() ) + ( p1 * c1.GetGreen() ) + ( p2 * c2.GetGreen() ) + ( p3 * c3.GetGreen() );
    const FLOAT b = ( p0 * c0.GetBlue() )  + ( p1 * c1.GetBlue() )  + ( p2 * c2.GetBlue() )  + ( p3 * c3.GetBlue() );

    return RGBAValue( ( UINT )r, ( UINT )g, ( UINT )b );
}

//--------------------------------------------------------------------------------------
// Name: Interpolate()
// Given a depth map value, interpolate along the spline.
// NB included here to avoid including DepthMap.h in the header.
//--------------------------------------------------------------------------------------
RGBAValue CatmullRomColorSpline::Interpolate( const DepthValue& depth ) const
{
    return Interpolate( depth.FloatDepth() );

}
