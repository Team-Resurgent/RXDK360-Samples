//--------------------------------------------------------------------------------------
// Utils.h
//
// Utility functions for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_UTILS_H
#define ARCADESAMPLE_UTILS_H


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Gameplay structs
// TODO: Can we change these to D3D Math?
//--------------------------------------------------------------------------------------
struct Vec
{
    FLOAT x, y;
};

struct Rect
{
    FLOAT x, y, width, height;
};


//--------------------------------------------------------------------------------------
// Get a random number between zero and one
//--------------------------------------------------------------------------------------
inline FLOAT RandomChoice( FLOAT opt0, FLOAT opt1 )
{
    return ( ::rand() & 1 ) ? opt0 : opt1;
}

//--------------------------------------------------------------------------------------
// Get a random number between zero and one
//--------------------------------------------------------------------------------------
inline FLOAT RandomFloat_0To1()
{
    return ( FLOAT )::rand() / ( FLOAT )RAND_MAX;
}

//--------------------------------------------------------------------------------------
// Get a random number between zero and val
//--------------------------------------------------------------------------------------
inline FLOAT RandomFloat_0To( FLOAT val )
{
    return val * RandomFloat_0To1();
}

//--------------------------------------------------------------------------------------
// Get a random number between zero and one
//--------------------------------------------------------------------------------------
inline FLOAT RandomFloat_NegToPos( FLOAT val )
{
    return RandomFloat_0To( val ) - ( val / 2.0f );
}

} // namespace ArcadeSample

#endif // ARCADESAMPLE_UTILS_H
