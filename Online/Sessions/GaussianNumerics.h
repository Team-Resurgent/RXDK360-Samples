//--------------------------------------------------------------------------------------
// GaussianNumerics.h
//
// Numerical approximations of Gaussian densities
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef GAUSSIANNUMERICS_H
#define GAUSSIANNUMERICS_H

#define _USE_MATH_DEFINES
#include <math.h>
#include <float.h>


// this is the square root of DBL_EPSILON necessary for asymptotic approximations
// in the v/v0 and w/w0 functions. Pre-computing these values saves computations.
const DOUBLE SQRT_DBL_EPSILON = ( 0.14901161193847656314265920000000e-7 );

//--------------------------------------------------------------------------------------
// Name: NormalDensity()
// Desc: Computes 1-dimensional Gaussian density value (standard Gaussian,
//       unit variance Gaussian and general Gaussian density)
//--------------------------------------------------------------------------------------
inline DOUBLE NormalDensity( DOUBLE x, DOUBLE dMu, DOUBLE dSigma )
{
    return 1.0 / ( 2.5066282746310005024157652848110 * dSigma ) *
        exp( -( dMu - x ) * ( dMu - x ) / ( 2.0 * dSigma * dSigma ) );
}
inline DOUBLE NormalDensity( DOUBLE x, DOUBLE dMu )
{
    return NormalDensity( x, dMu, 1.0 );
}
inline DOUBLE NormalDensity( DOUBLE x )
{
    return NormalDensity( x, 0.0 );
}

//--------------------------------------------------------------------------------------
// Name: erfc()
// Desc: Computes the complementary error function. This function is defined by
//       2/sqrt(pi) * integral from x to infinity of exp (-t^2) dt.
//--------------------------------------------------------------------------------------
DOUBLE erfc( DOUBLE x );

//--------------------------------------------------------------------------------------
// Name: erfcinv()
// Desc: Computes the inverse of the complementary error function.
//--------------------------------------------------------------------------------------
DOUBLE erfcinv( DOUBLE y );

//--------------------------------------------------------------------------------------
// Name: Phi()
// Desc: 1-dimensional Gaussian tail functions. This function is defined by
//       integral from -infinity to x of N (t; 0, 1) dt.
//--------------------------------------------------------------------------------------
inline DOUBLE Phi( DOUBLE t )
{
    return erfc( -t / 1.4142135623730950488016887242097 ) / 2.0;
}
inline DOUBLE Phi( DOUBLE t, DOUBLE dMu )
{
    return Phi( t - dMu );
}
inline DOUBLE Phi( DOUBLE t, DOUBLE dMu, DOUBLE dSigma )
{
    return Phi( ( t - dMu ) / dSigma );
}

//--------------------------------------------------------------------------------------
// Name: PhiInverse()
// Desc: 1-dimensional inverse of the Gaussian tail functions.
//--------------------------------------------------------------------------------------
inline DOUBLE PhiInverse( DOUBLE dTailProbability )
{
    return -1.4142135623730950488016887242097 * erfcinv( 2.0 * dTailProbability );
}

//--------------------------------------------------------------------------------------
// Name: v()
// Desc: The additive correction of a single-sided truncated Gaussian with unit
//       variance.
//--------------------------------------------------------------------------------------
DOUBLE v( DOUBLE t, DOUBLE dEpsilon );

//--------------------------------------------------------------------------------------
// Name: v0()
// Desc: The additive correction of a symmetrical double-sided truncated Gaussian with
//       unit variance.
//--------------------------------------------------------------------------------------
DOUBLE v0( DOUBLE t, DOUBLE dEpsilon );

//--------------------------------------------------------------------------------------
// Name: w()
// Desc: The multiplicative correction of a single-sided truncated Gaussian with unit
//       variance.
//--------------------------------------------------------------------------------------
DOUBLE w( DOUBLE t, DOUBLE dEpsilon );

//--------------------------------------------------------------------------------------
// Name: w0()
// Desc: The multiplicative correction of a symmetrical double-sided truncated Gaussian
//       with unit variance.
//--------------------------------------------------------------------------------------
DOUBLE w0( DOUBLE t, DOUBLE dEpsilon );

#endif // GAUSSIANNUMERICS_H
