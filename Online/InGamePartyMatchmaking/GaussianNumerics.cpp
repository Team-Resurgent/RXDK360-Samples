//--------------------------------------------------------------------------------------
// GaussianNumerics.cpp
//
// Numerical approximations of Gaussian densities
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "stdafx.h"
#include "GaussianNumerics.h"


//--------------------------------------------------------------------------------------
// Name: erfc()
// Desc: Computes the complementary error function. This function is defined by
//       2/sqrt(pi) * integral from x to infinity of exp (-t^2) dt.
//       This method uses a Chebyshev series approximation which is exact in the whole
//       range up to 1 * 10^{-7}.
//--------------------------------------------------------------------------------------
DOUBLE erfc( DOUBLE x )
{
    // check for boundary cases
    if( x == DBL_MIN )
        return 2.0;
    if( x == DBL_MAX )
        return 0.0;

    // ... otherwise do the hard work
    DOUBLE z = abs( x );
    DOUBLE t = 1.0 / (1.0 + 0.5 * z);
    DOUBLE dResult = t * exp ( -z * z
                               - 1.26551223 +
                               t * (1.00002368 +
                               t * (0.37409196 +
                               t * (0.09678418 +
                               t * (-0.18628806 +
                               t * (0.27886807 +
                               t * (-1.13520398 +
                               t * (1.48851587 +
                               t * (-0.82215223 +
                               t * 0.17087277)))))))) );

    if( x >= 0.0 )
        return dResult;
    else
        return 2.0 - dResult;
}

//--------------------------------------------------------------------------------------
// Name: erfcinv()
// Desc: Computes the inverse of the complementary error function. This function uses
//       a polynomial approximation together with one step of Halley's rational method.
//       It returns NAN if the argument is out of range.
//--------------------------------------------------------------------------------------
DOUBLE erfcinv( DOUBLE y )
{
    // check for boundary cases
    if( y < 0.0 || y > 2.0 )
        return log( 0.0 );
    if( y == 0.0 )
        return DBL_MAX;
    if( y == 2.0 )
        return DBL_MIN;

    // stores the result
    DOUBLE x = 0.0;

    // Rational approxiamtion for the central region
    if( y  >= 0.0485 && y <= 1.9515 ) 
    {
        DOUBLE q = y - 1.0;
        DOUBLE r = q * q;
        x = (((((0.01370600482778535 * r - 0.3051415712357203)* r +
                1.524304069216834)* r - 3.057303267970988)* r +
              2.710410832036097)* r - 0.8862269264526915) * q /
            (((((-0.05319931523264068 * r + 0.6311946752267222)* r -
                2.432796560310728)* r + 4.175081992982483)* r -
              3.320170388221430)* r + 1.0);
    }

    // Rational approximation for the lower region
    if( y < 0.0485 )
    {
        DOUBLE q = sqrt( -2.0 * log( y / 2.0 ) );
        x = (((((0.005504751339936943 * q + 0.2279687217114118) * q +
                1.697592457770869) * q + 1.802933168781950) * q +
              -3.093354679843504) * q - 2.077595676404383) /
            ((((0.007784695709041462 * q + 0.3224671290700398) * q +
               2.445134137142996) * q + 3.754408661907416) * q + 1.0);
    }

    // Rational approximation for the upper region
    if( y > 1.9515 ) 
    {
        DOUBLE q = sqrt( -2.0 * log( 1 - y / 2.0 ) );
        x = -(((((0.005504751339936943 * q + 0.2279687217114118) * q +
                 1.697592457770869) * q + 1.802933168781950) * q +
               -3.093354679843504) * q - 2.077595676404383) /
            ((((0.007784695709041462 * q + 0.3224671290700398) * q +
               2.445134137142996) * q + 3.754408661907416) * q + 1.0);
    }

    // One iteration of Halley's rational method (third order) gives full
    // machine precision.
    DOUBLE u = (erfc( x ) - y) / (-1.1283791670955125738961589031216 * exp( -x * x ));
    x = x - u / ( 1.0 + x * u );

    return x;
}

//--------------------------------------------------------------------------------------
// Name: v()
// Desc: Computes the additive correction of a single-sided truncated Gaussian with
//       unit variance.
//--------------------------------------------------------------------------------------
DOUBLE v( DOUBLE t, DOUBLE dEpsilon )
{
    DOUBLE dNumerator = NormalDensity( t - dEpsilon );
    DOUBLE dDenominator = Phi( t - dEpsilon );
    if( dDenominator < SQRT_DBL_EPSILON )
        return -t + dEpsilon;
    else
        return dNumerator / dDenominator;
}

//--------------------------------------------------------------------------------------
// Name: v0()
// Desc: Computes the additive correction of a symmetrical double-sided truncated
//       Gaussian with unit variance.
//--------------------------------------------------------------------------------------
DOUBLE v0( DOUBLE t, DOUBLE dEpsilon )
{
    DOUBLE v = abs( t );
    DOUBLE dNumerator = NormalDensity( -dEpsilon - v ) - NormalDensity( dEpsilon - v );
    DOUBLE dDenominator = Phi( dEpsilon - v ) - Phi( -dEpsilon - v );
    if( dDenominator < SQRT_DBL_EPSILON )
    {
        if( t < 0.0 )
            return -t - dEpsilon;
        else
            return -t + dEpsilon;
    }
    else
    {
        if( t < 0.0 )
            return -dNumerator / dDenominator;
        else
            return dNumerator / dDenominator;
    }
}

//--------------------------------------------------------------------------------------
// Name: w()
// Desc: Computes the multiplicative correction of a single-sided truncated Gaussian
//       with unit variance.
//--------------------------------------------------------------------------------------
DOUBLE w( DOUBLE t, DOUBLE dEpsilon )
{
    DOUBLE dNumerator =( t - dEpsilon ) * NormalDensity( t - dEpsilon );
    DOUBLE dDenominator = Phi( t - dEpsilon );
    if( dDenominator < SQRT_DBL_EPSILON )
    {
        if( t < 0.0 )
            return 1.0;
        else
            return 0.0;
    }
    else
    {
        DOUBLE aV0 = v( t, dEpsilon );
        return aV0 * aV0 + dNumerator / dDenominator;
    }
}

//--------------------------------------------------------------------------------------
// Name: w0()
// Desc: Computes the multiplicative correction of a symmetrical double-sided truncated
//       Gaussian with unit variance.
//--------------------------------------------------------------------------------------
DOUBLE w0( DOUBLE t, DOUBLE dEpsilon )
{
    DOUBLE v = abs( t );
    DOUBLE dNumerator = (dEpsilon - v) * NormalDensity( dEpsilon - v ) -
                        (-dEpsilon - v) * NormalDensity( -dEpsilon - v );
    DOUBLE dDenominator = Phi( dEpsilon - v ) - Phi( -dEpsilon - v );
    if( dDenominator < SQRT_DBL_EPSILON )
        return 1.0;
    else
    {
        DOUBLE aV0 = v0( v, dEpsilon );
        return aV0 * aV0 + dNumerator / dDenominator;
    }
}
