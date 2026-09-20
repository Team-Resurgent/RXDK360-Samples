//--------------------------------------------------------------------------------------
// HOCDetectorInternal.h
//
// HOCDetector internal header with most tweakable constants and common functions
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once



// undefine this if you don't need debug draw support in detector
#define DEBUG_DRAW



#include "HOCDetector.h"


#include <stdlib.h>
#include <algorithm>
#include <assert.h>
#include <stdio.h>
#include <string.h>



// debug rendering can be turned off
#ifdef DEBUG_DRAW
#define ADD_DEBUG_QUAD( x, y, sx, sy, clr )         { if( HOCDetector::ms_debugDraw ) HOCDetector::ms_debugDraw->AddQuadInDepthImageSpace( x, y, sx, sy, clr ); }
#define ADD_DEBUG_QUAD_MIN_MAX( x, y, mx, my, clr ) ADD_DEBUG_QUAD( x, y, (mx) - (x) + 1, (my) - (y) + 1, clr )
#else
#define ADD_DEBUG_QUAD( x, y, sx, sy, clr )         /* empty */
#define ADD_DEBUG_QUAD_MIN_MAX( x, y, mx, my, clr ) /* empty */
#endif



#ifndef RETURN_ON_FILE_ERROR
#define RETURN_ON_FILE_ERROR( pFile )   if ( ferror( pFile) ) return E_FAIL;
#endif

#ifndef RETURN_FILE_ERROR
#define RETURN_FILE_ERROR( pFile )   return ( ferror( pFile) ) ? E_FAIL : S_OK;
#endif


//--------------------------------------------------------------------------------------
// Various constants
//--------------------------------------------------------------------------------------
static const FLOAT      HAND_SIZE_FOR_NORMALIZED_SKELETON = 0.28f;   // metres, slightly depends on WRIST_CUTOFF_RATIO. This is the size of the hand of the skeleton
                                                                     // whose size is NORMALIZED_SKELETON_SIZE (below)
static const FLOAT      NORMALIZED_SKELETON_SIZE = 0.42f;            // playersize, in metres
static const FLOAT      HIST_BIN_SIZE_INV = ( (FLOAT)HOCComputedData::NUM_BUCKETS + 1 );
static const XMVECTOR   TO_BIN_SCALER = XMVectorSet( HIST_BIN_SIZE_INV, HIST_BIN_SIZE_INV, HIST_BIN_SIZE_INV, HIST_BIN_SIZE_INV );  // histogram scaler

static const UINT       MINIMUM_ACCEPTABLE_NUM_VOXELS = 6 * 6;
static const FLOAT      WRIST_CUTOFF_RATIO = 0.0f;          // 0 - cut at wrist, 1 - cut at hand, anything inbetween is a position between hand and wrist

static const FLOAT      DEPTH_RANGE_MULTIPLIER = 0.8f;      // this is by how much we multiple the expected XY size of a hand in screenspace
                                                            // to get the expected size of the hand in Z coordinate

static const UINT       HOC_FILE_VERSION = 0x00001006;
static const UINT       HOC_FILE_PREV_VERSION = 0x00001005;

static const FLOAT      TAN_VERT_CAMERA_FOV = tanf( XMConvertToRadians( 0.5f * NUI_CAMERA_DEPTH_NOMINAL_VERTICAL_FOV ) );
static const FLOAT      TAN_HORZ_CAMERA_FOV = tanf( XMConvertToRadians( 0.5f * NUI_CAMERA_DEPTH_NOMINAL_HORIZONTAL_FOV ) );

static const FLOAT      FOVH_PER_PIXEL_INV = 0.5f * FLOAT( HOC_DEPTH_SIZE_X ) / TAN_HORZ_CAMERA_FOV;
static const FLOAT      FOVV_PER_PIXEL_INV = 0.5f * FLOAT( HOC_DEPTH_SIZE_Y ) / TAN_VERT_CAMERA_FOV;

static const FLOAT      FOVH_PER_PIXEL_1x1_INV = 0.5f / TAN_HORZ_CAMERA_FOV;
static const FLOAT      FOVV_PER_PIXEL_1x1_INV = 0.5f / TAN_VERT_CAMERA_FOV;

static const FLOAT      FOVH_PER_PIXEL = TAN_HORZ_CAMERA_FOV / (0.5f * FLOAT( HOC_DEPTH_SIZE_X ));
static const FLOAT      FOVV_PER_PIXEL = TAN_VERT_CAMERA_FOV / (0.5f * FLOAT( HOC_DEPTH_SIZE_Y ));


//--------------------------------------------------------------------------------------
// Name: EndianSwap
// Desc: Endian swaps a UINT on PC. Saved file is always big-endian for quicker 360 load
//--------------------------------------------------------------------------------------
inline
UINT    EndianSwap( UINT v )
{
#ifdef _XBOX
    return v;
#else
    return _byteswap_ulong( v );
#endif
}

//--------------------------------------------------------------------------------------
// Name: EndianSwap
// Desc: Endian swaps am array of UINTs on PC. Saved file is always big-endian for quicker 360 load
//--------------------------------------------------------------------------------------
inline
void    EndianSwap( UINT* v, UINT num )
{
#ifdef _XBOX
    // nop
#else
    for( UINT i=0; i < num; ++i )
        v[ i ] =_byteswap_ulong( v[ i ] );
#endif
}


//--------------------------------------------------------------------------------------
// Name: EndianSwap
// Desc: Endian swaps an array of USHORTs on PC. Saved file is always big-endian for quicker 360 load
//--------------------------------------------------------------------------------------
inline
void    EndianSwap( USHORT* v, UINT num )
{
#ifdef _XBOX
    // nop
#else
    for( UINT i=0; i < num; ++i )
        v[ i ] =_byteswap_ushort( v[ i ] );
#endif
}

//--------------------------------------------------------------------------------------
// Name: EndianSwap
// Desc: Endian swaps a USHORT on PC. Saved file is always big-endian for quicker 360 load
//--------------------------------------------------------------------------------------
inline
USHORT  EndianSwap( USHORT v )
{
#ifdef _XBOX
    return v;
#else
    return _byteswap_ushort( v );
#endif
}



//--------------------------------------------------------------------------------------
// Name: ProjectWorldDistanceToScreen
// Desc: Projects a size in world space into screen space
//--------------------------------------------------------------------------------------
inline static
FLOAT   ProjectWorldDistanceToScreen( FLOAT fSize, FLOAT fDistMeters )
{
    return fSize * FOVH_PER_PIXEL_INV / fDistMeters;
}

//--------------------------------------------------------------------------------------
// Name: ProjectWorldToScreen
// Desc: Projects a point in world space into screen space
//--------------------------------------------------------------------------------------
inline static
void    ProjectWorldToScreen( XMFLOAT3* pScreen, FLOAT x, FLOAT y, FLOAT z )
{
    const FLOAT iz = 1.f / z;

    pScreen->x = 0.5f + x * FOVH_PER_PIXEL_1x1_INV * iz;
    pScreen->y = 0.5f - y * FOVV_PER_PIXEL_1x1_INV * iz;
    pScreen->z = z;
}

//--------------------------------------------------------------------------------------
// Name: TransformScreenToWorld
// Desc: Transforms a screen point into world space
//--------------------------------------------------------------------------------------
inline static
void    TransformScreenToWorld( XMVECTOR* pWorld, FLOAT fX, FLOAT fY, FLOAT fZ )
{
    *pWorld = XMVectorSet( (fX - FLOAT( HOC_DEPTH_SIZE_X ) * 0.5f) * fZ * FOVH_PER_PIXEL,
                           (FLOAT( HOC_DEPTH_SIZE_Y ) * 0.5f - fY) * fZ * FOVV_PER_PIXEL,
                           fZ, 1 );
}

//--------------------------------------------------------------------------------------
// Name: GetDataValue
// Desc: Gets the value of the classifier given the sample data
//--------------------------------------------------------------------------------------
__forceinline
FLOAT   HOCBaseWeakClassifier::GetDataValue( const HOCDataViews& d, const HOCExtraClassifierData& e ) const
{
    switch( m_type )
    {
    default:
        return 0;

    case TYPE_CONTOUR_EXPERIMENT1:
        return d.m_computedData.m_fContourExp1;

    case TYPE_CONTOUR_EXPERIMENT2:
        return d.m_computedData.m_fContourExp2;

    case TYPE_EXPERIMENT1:
        return d.m_computedData.m_fExperiment1;

    case TYPE_EXPERIMENT2:
        return d.m_computedData.m_fExperiment2;

    case TYPE_EXPERIMENT3:
        return d.m_computedData.m_fExperiment3[ 0 ];

    case TYPE_EXPERIMENT4:
        return d.m_computedData.m_fExperiment3[ 1 ];

    case TYPE_BUCKET_DIFFERENCE_0:
        return d.m_computedData.m_normalizedHistogram[ 0 ][ m_bucketIndices[ 1 ] ] - d.m_computedData.m_normalizedHistogram[ 0 ][ m_bucketIndices[ 0 ] ];

    case TYPE_BUCKET_ABS_0:
        return d.m_computedData.m_normalizedHistogram[ 0 ][ m_bucketIndices[ 0 ] ];

    case TYPE_INTEGRAL_BUCKET_ABS_0:
        return d.m_computedData.m_integralHistogram[ 0 ][ m_bucketIndices[ 0 ] ];

    case TYPE_MEAN_0:
        return d.m_computedData.m_fMean[ 0 ];

    case TYPE_VARIANCE_0:
        return d.m_computedData.m_fVariance[ 0 ];

    case TYPE_Z_ROUNDNESS_0:
        return d.m_computedData.m_fZroundness[ 0 ];

    case TYPE_BUCKET_DIFFERENCE_1:
        return d.m_computedData.m_normalizedHistogram[ 1 ][ m_bucketIndices[ 1 ] ] - d.m_computedData.m_normalizedHistogram[ 1 ][ m_bucketIndices[ 0 ] ];

    case TYPE_BUCKET_ABS_1:
        return d.m_computedData.m_normalizedHistogram[ 1 ][ m_bucketIndices[ 0 ] ];

    case TYPE_INTEGRAL_BUCKET_ABS_1:
        return d.m_computedData.m_integralHistogram[ 1 ][ m_bucketIndices[ 0 ] ];

    case TYPE_MEAN_1:
        return d.m_computedData.m_fMean[ 1 ];

    case TYPE_VARIANCE_1:
        return d.m_computedData.m_fVariance[ 1 ];

    case TYPE_Z_ROUNDNESS_1:
        return d.m_computedData.m_fZroundness[ 1 ];

    case TYPE_CONTOUR_SYMMETRY:
        return d.m_computedData.m_fContourSymmetry;

    case TYPE_CONTOUR_MEAN:
        return d.m_computedData.m_fContourMean;

    case TYPE_CONTOUR_SPREAD:
        return d.m_computedData.m_fContourSpread;

    case TYPE_BONE_LENGTHS:
        return d.m_computedData.m_fBoneLengths;
    }
}

//--------------------------------------------------------------------------------------
// Name: IsSameClass
// Desc: Returns TRUE is two classifer instances are of the same classifier
//--------------------------------------------------------------------------------------
inline
BOOL HOCBaseWeakClassifier::IsSameClass( const HOCBaseWeakClassifier& s ) const
{
    if( m_type != s.m_type )
        return FALSE;

    if( m_type >= TYPE_REQUIRES_TWO_BUCKETS   &&
        m_type < TYPE_REQUIRES_ONE_BUCKET )
    {
        return (m_bucketIndices[ 1 ] == s.m_bucketIndices[ 1 ]) && (m_bucketIndices[ 0 ] == s.m_bucketIndices[ 0 ]);
    }

    if( m_type >= TYPE_REQUIRES_ONE_BUCKET    &&
        m_type < TYPE_REQUIRES_NO_BUCKETS )
    {
        return m_bucketIndices[ 0 ] == s.m_bucketIndices[ 0 ];
    }

    return TRUE;
}


#ifdef HOC_TRAINER
//--------------------------------------------------------------------------------------
// Name: Iterator
// Desc: allows for fast sequential iteration
//--------------------------------------------------------------------------------------
__forceinline
INT HOCDetectorTrainer::ResultsVector::Iterator::GetResultAndAdvance()
{
    assert( m_uCountdown );

    const INT iRes = (m_uCached & m_uMask) ? 1 : -1;

    if( m_uMask == (1ul << 31ul) )
    {
        m_uMask = 1;
        m_uCached = *m_pPtr++;
    } else
    {
        m_uMask <<= 1;
    }

    --m_uCountdown;

    return iRes;
}


//--------------------------------------------------------------------------------------
// Name: ResultsVector
// Desc: Constructs an empty bit vector
//--------------------------------------------------------------------------------------
inline
HOCDetectorTrainer::ResultsVector::ResultsVector() : m_pData( NULL ), m_uNumBits( 0 ), m_uNumDwords( 0 )
{
}


//--------------------------------------------------------------------------------------
// Name: ResultsVector
// Desc: Copy ctor, we store these in structures so we need it
//--------------------------------------------------------------------------------------
inline
HOCDetectorTrainer::ResultsVector::ResultsVector( const ResultsVector& rhs ) : m_pData( NULL ), m_uNumBits( 0 ), m_uNumDwords( 0 )
{
    *this = rhs;
}


//--------------------------------------------------------------------------------------
// Name: ResultsVector
// Desc: dtor, just frees the memory block
//--------------------------------------------------------------------------------------
inline
HOCDetectorTrainer::ResultsVector::~ResultsVector()
{
    delete[] m_pData;
}

//--------------------------------------------------------------------------------------
// Name: operator =
// Desc: Assignment operator, we store these in structures so we need it
//--------------------------------------------------------------------------------------
inline
HOCDetectorTrainer::ResultsVector& HOCDetectorTrainer::ResultsVector::operator = ( const ResultsVector& rhs )
{
    if( &rhs == this )
        return *this;

    delete[] m_pData;
    m_pData = new DWORD[ rhs.m_uNumDwords ];
    memcpy( m_pData, rhs.m_pData, rhs.m_uNumDwords * 4 );
    m_uNumDwords = rhs.m_uNumDwords;
    m_uNumBits = rhs.m_uNumBits;

    return *this;
}

//--------------------------------------------------------------------------------------
// Name: Resize
// Desc: Resizes the bit vector so it can accomodate a given number of bits
//--------------------------------------------------------------------------------------
inline
VOID    HOCDetectorTrainer::ResultsVector::Resize( UINT uNumBits )
{
    const UINT  uNumDwords = (31 + uNumBits) / 32;
    if( m_uNumDwords >= uNumDwords )
    {
        m_uNumBits = uNumBits;
        return;
    }

    DWORD* data = new DWORD[ uNumDwords ];

    if( m_pData )
    {
        memcpy( data, m_pData, m_uNumDwords * 4 );
        delete[] m_pData;
    }

    m_pData = data;
    m_uNumDwords = uNumDwords;
    m_uNumBits = uNumBits;
}

//--------------------------------------------------------------------------------------
// Name: SetResult
// Desc: Setting a bit from result. iResult is -1 or 1, mapping bit=0 to -1 and bit=1 to 1.
//--------------------------------------------------------------------------------------
__forceinline
VOID    HOCDetectorTrainer::ResultsVector::SetResult( UINT uBit, INT iResult )
{
    assert( uBit < m_uNumBits );

    DWORD* __restrict pDword = &m_pData[ uBit / 32 ];
    const DWORD uDword = *pDword;
    const DWORD uMask = 1ul << (uBit & 31);

    if( iResult < 0 )
        *pDword = uDword & ~uMask;
    else
        *pDword = uDword | uMask;
}

//--------------------------------------------------------------------------------------
// Name: GetResult
// Desc: Retrieves a result bit and converts it to {-1,1} result for the trainer
//--------------------------------------------------------------------------------------
__forceinline
INT HOCDetectorTrainer::ResultsVector::GetResult( UINT uBit ) const
{
    assert( uBit < m_uNumBits );

    const DWORD uDword = m_pData[ uBit / 32 ];
    const DWORD uMask = 1ul << (uBit & 31);
    
    return (uDword & uMask) ? 1 : -1;
}

//--------------------------------------------------------------------------------------
// Name: Begin
// Desc: Return an iterator which is optimised for fast sequential iteration of bits
//--------------------------------------------------------------------------------------
__forceinline
HOCDetectorTrainer::ResultsVector::Iterator    HOCDetectorTrainer::ResultsVector::Begin() const
{
    Iterator    i;

    if( !m_pData )
    {
        i.m_pPtr = NULL;
        i.m_uCountdown = 0;
        i.m_uCached = 0;
        i.m_uMask = 0;
    } else
    {
        i.m_pPtr = m_pData;
        i.m_uCountdown = m_uNumBits;
        i.m_uMask = 1;
        i.m_uCached = *i.m_pPtr++;
    }

    return i;
}

//--------------------------------------------------------------------------------------
// Name: IsSameResults
// Desc: Compares if two instances of a classifier return exactly the same results
//--------------------------------------------------------------------------------------
__forceinline
BOOL    HOCDetectorTrainer::CachedResults::IsSameResults( UINT i, UINT j ) const
{
    for( UINT k=0; k < _countof( m_sumOverIndices ); ++k )
    {
        if( m_sumOverIndices[ k ][ i ].size() != m_sumOverIndices[ k ][ j ].size() )
        {
            return FALSE;
        }

        if( !m_sumOverIndices[ k ][ i ].empty()         &&
            0 != memcmp( &m_sumOverIndices[ k ][ i ][ 0 ],
                         &m_sumOverIndices[ k ][ j ][ 0 ],
                         sizeof( m_sumOverIndices[ k ][ i ][ 0 ] ) * m_sumOverIndices[ k ][ i ].size() ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}
#endif      // HOC_TRAINER


//--------------------------------------------------------------------------------------
// Name: HOCSwap
// Desc: swaps two variables
//--------------------------------------------------------------------------------------
template< class t >
static inline
void    HOCSwap( t& a, t& b )
{
    t temp = a;
    a = b;
    b = temp;
}



//--------------------------------------------------------------------------------------
// Useful to be able to ++ an enum
//--------------------------------------------------------------------------------------
inline
void operator ++ ( HOCBaseWeakClassifier::Type& a )
{
    INT t = (INT)a;
    if( t == HOCBaseWeakClassifier::TYPE_LAST_SIMPLE_CLASSIFIER )
        return;

    a = (HOCBaseWeakClassifier::Type)(t + 1);
}


//--------------------------------------------------------------------------------------
// Name: WriteUShort
// Desc: Writes a ushort with endian conversion on PC, writes as is on 360
//--------------------------------------------------------------------------------------
template< class T >
static inline
HRESULT WriteUShort( _In_count_( uCount ) const T* v, _In_ FILE* pFile, UINT uCount = 1 )
{
    for( UINT i=0; i < uCount; ++i )
    {
        const USHORT t = EndianSwap( (USHORT)v[ i ] );
        fwrite( &t, 2, 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: WriteUByte
// Desc: Writes a ubyte with endian conversion on PC, writes as is on 360
//--------------------------------------------------------------------------------------
template< class T >
static inline
HRESULT WriteUByte( _In_count_( uCount ) const T* v, _In_ FILE* pFile, UINT uCount = 1 )
{
    for( UINT i=0; i < uCount; ++i )
    {
        const BYTE b = v[ i ];
        fwrite( &b, 1, 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: WriteUInt
// Desc: Writes a uint with endian conversion on PC, writes as is on 360
//--------------------------------------------------------------------------------------
template< class T >
static inline
HRESULT WriteUInt( _In_count_( uCount ) const T* v, _In_ FILE* pFile, UINT uCount = 1 )
{
    for( UINT i=0; i < uCount; ++i )
    {
        const UINT t = EndianSwap( (UINT)v[ i ] );
        fwrite( &t, 4, 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: WriteFloat
// Desc: Writes a float with endian conversion on PC, writes as is on 360
//--------------------------------------------------------------------------------------
static inline
HRESULT WriteFloat( _In_count_( uCount ) const FLOAT* v, _In_ FILE* pFile, UINT uCount = 1 )
{
    for( UINT i=0; i < uCount; ++i )
    {
        const UINT t = EndianSwap( *(UINT*)&v[ i ] );
        fwrite( &t, 4, 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: ReadUShort
// Desc: Reads a ushort with endian conversion on PC, reads as is on 360
//--------------------------------------------------------------------------------------
template< class T >
static inline
HRESULT ReadUShort( _Out_cap_( uCount ) T* v, _In_ FILE* pFile, UINT uCount = 1 )
{
    for( UINT i=0; i < uCount; ++i )
    {
        USHORT t;
        fread( &t, 2, 1, pFile );
        v[ i ] = EndianSwap( t );
        RETURN_ON_FILE_ERROR( pFile );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ReadUByte
// Desc: Reads a ubyte with endian conversion on PC, reads as is on 360
//--------------------------------------------------------------------------------------
template< class T >
static inline
HRESULT ReadUByte( _Out_cap_( uCount ) T* v, _In_ FILE* pFile, UINT uCount = 1 )
{
    for( UINT i=0; i < uCount; ++i )
    {
        BYTE b;
        fread( &b, 1, 1, pFile );
        v[ i ] = b;
        RETURN_ON_FILE_ERROR( pFile );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ReadUInt
// Desc: Reads a uint with endian conversion on PC, reads as is on 360
//--------------------------------------------------------------------------------------
template< class T >
static inline
HRESULT ReadUInt( _Out_cap_( uCount ) T* v, _In_ FILE* pFile, UINT uCount = 1 )
{
    for( UINT i=0; i < uCount; ++i )
    {
        UINT t;
        fread( &t, 4, 1, pFile );
        v[ i ] = EndianSwap( t );
        RETURN_ON_FILE_ERROR( pFile );
    }

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: ReadFloat
// Desc: Reads a float with endian conversion on PC, reads as is on 360
//--------------------------------------------------------------------------------------
static inline
HRESULT ReadFloat( _Out_cap_( uCount ) FLOAT* v, _In_ FILE* pFile, UINT uCount = 1 )
{
    for( UINT i=0; i < uCount; ++i )
    {
        UINT t;
        fread( &t, 4, 1, pFile );
        t = EndianSwap( t );
        v[ i ] = *(FLOAT*)&t;
        RETURN_ON_FILE_ERROR( pFile );
    }

    return S_OK;
}
