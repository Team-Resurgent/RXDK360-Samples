//--------------------------------------------------------------------------------------
// File: MeshFunc.h
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// Copyright (C) Tomohide Kano. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once


//--------------------------------------------------------------------------------------
// Name: torus()
// Desc: Bivariate function for a torus
//--------------------------------------------------------------------------------------
static XMVECTOR torus( FLOAT u, FLOAT v )
{
    u *= 2 * XM_PI;
    v *= 2 * XM_PI;
    XMVECTOR vec;
    vec.x = +sinf( u ) * ( 2.1f + sinf( v ) );
    vec.y = +cosf( u ) * ( 2.1f + sinf( v ) );
    vec.z = -cosf( v );
    return vec;
}


//--------------------------------------------------------------------------------------
// Name: mobius()
// Desc: Bivariate function for a mobius strip (one-sided strip with three half twists)
//--------------------------------------------------------------------------------------
static XMVECTOR _mobius( FLOAT t )
{
    return XMVectorSet( 2.3f * sinf( t ), 2.3f * cosf( t ), 0.0f, 0.0f );
}
static XMVECTOR mobius( FLOAT u, FLOAT v )
{
    u *= 2 * XM_PI;
    v *= 2 * XM_PI;
    XMVECTOR Z = XMVectorSet( 0, 0, 1, 0 );
    XMVECTOR T = XMVector3Normalize( _mobius( u + 0.01f ) - _mobius( u - 0.01f ) );
    XMVECTOR B = XMVector3Normalize( XMVector3Cross( Z, T ) );
    XMVECTOR N = XMVector3Normalize( XMVector3Cross( B, T ) );
    XMVECTOR b = cosf( 1.5f * u ) * B + sinf( 1.5f * u ) * N;
    XMVECTOR n = -sinf( 1.5f * u ) * B + cosf( 1.5f * u ) * N;
    return _mobius( u ) + ( 1.0f * sinf( v + 0.5f * u ) * b + 0.5f * cosf( v + 0.5f * u ) * n );
}


//--------------------------------------------------------------------------------------
// Name: knot()
// Desc: Bivariate function for a knot
//--------------------------------------------------------------------------------------
static XMVECTOR _knot( FLOAT t )
{
    FLOAT r = 1.8f + 0.8f * cosf( 3 * t );
    FLOAT phi = 0.2f * XM_PI * sinf( 3 * t );
    XMVECTOR vec;
    vec.x = +r * cosf( phi ) * sinf( 2 * t );
    vec.y = +r * cosf( phi ) * cosf( 2 * t );
    vec.z = -r * sinf( phi );
    return vec;
}
static XMVECTOR knot( FLOAT u, FLOAT v )
{
    u *= 2 * XM_PI;
    v *= 2 * XM_PI;
    u += -0.2f * sinf( 3 * u ) + 0.05f * sinf( 6 * u );
    XMVECTOR Z = XMVectorSet( 0, 0, 1, 0 );
    XMVECTOR T = XMVector3Normalize( _knot( u + 0.01f ) - _knot( u - 0.01f ) );
    XMVECTOR B = XMVector3Normalize( XMVector3Cross( Z, T ) );
    XMVECTOR N = XMVector3Normalize( XMVector3Cross( B, T ) );
    return _knot( u ) + 0.55f * ( sinf( v ) * B + cosf( v ) * N );
}


//--------------------------------------------------------------------------------------
// Mesh parameters to be passed to FurMesh::Init()
//--------------------------------------------------------------------------------------
const MESH_PARAMETERS g_MeshParams[] =
{
    { torus,  24, 15,  6, 3, L"Torus" },
    { mobius, 30, 15,  6, 3, L"M\366bius Strip" },
    { knot,   50, 12, 12, 3, L"Knot" },
};

const int NUM_MESHES = sizeof( g_MeshParams ) / sizeof( g_MeshParams[0] );
