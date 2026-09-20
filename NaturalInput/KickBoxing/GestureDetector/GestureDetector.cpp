//--------------------------------------------------------------------------------------
// GestureDetector.cpp
//
// Definitions for the gesture detector and gesture detector trainer, as well definitions
// for weak and strong classifiers. The gesture detector trainer uses the AdaBoost
// learning alogorith.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "GestureDetector.h"
#include <float.h>
#include <algorithm>
#include <assert.h>

#ifndef _XBOX
#include <XedFile.h>
#include <omp.h>
#include <malloc.h>
#endif

using namespace std;

namespace ATGGestureDetector
{

//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------

// Each feature used to generate weak classifiers has a min, max and step. This controls
// how many weak classifiers gets geenerated per feature set. These values are either
// easy to understand, e.g. min and max values of an angle is 0 and 180, or they were
// emperically derived, e.g. what is the min and max velocity that your hand joint can
// move at. By making the step smaller, you'll get a finer grain of decision stump, resulting
// in more weak classifiers generated for that feature.
static const FLOAT fAngleMin            = 0.0f;
static const FLOAT fAngleMax            = 180.0f;
static const FLOAT fAngleStep           = 2.0f;

static const FLOAT fTimeSpaceAngleMin   = 90.0f;
static const FLOAT fTimeSpaceAngleMax   = 180.0f;
static const FLOAT fTimeSpaceAngleStep  = 1.0f;

static const FLOAT fAngleVelocityMin    = -5.0f;
static const FLOAT fAngleVelocityMax    = 5.0f;
static const FLOAT fAngleVelocityStep   = 0.25f;

static const FLOAT fAngleAccelMin       = -500.0f;
static const FLOAT fAngleAccelMax       = 500.0f;
static const FLOAT fAngleAccelStep      = 10.0f;

static const FLOAT fMusclePowerMin      = -100.0f;
static const FLOAT fMusclePowerMax      = 100.0f;
static const FLOAT fMusclePowerStep     = 1.0f;

static const FLOAT fMuscleForceMin      = -5.0f;
static const FLOAT fMuscleForceMax      = 5.0f;
static const FLOAT fMuscleForceStep     = 0.1f;

static const FLOAT fMuscleTorqueMin     = -5.0f;
static const FLOAT fMuscleTorqueMax     = 5.0f;
static const FLOAT fMuscleTorqueStep    = 0.1f;

static const FLOAT fPositionMin         = 0.0f;
static const FLOAT fPositionMax         = 1.0f;
static const FLOAT fPositionStep        = 2.0f;

static const FLOAT fDiffMuscleForceMin  = -1.0f;
static const FLOAT fDiffMuscleForceMax  = 1.0f;
static const FLOAT fDiffMuscleForceStep = 0.2f;

static const FLOAT fSpeedMin            = 0.0f;
static const FLOAT fSpeedMax            = 10.0f;
static const FLOAT fSpeedStep           = 0.05f;

static const FLOAT fVelocityMin         = -5.0f;
static const FLOAT fVelocityMax         = 5.0f;
static const FLOAT fVelocityStep        = 0.1f;

static const FLOAT fVelocitySQMin       = 0.0f;
static const FLOAT fVelocitySQMax       = 25.0f;
static const FLOAT fVelocitySQStep      = 0.1f;

static const FLOAT fSpeedSQMin          = 0.0f;
static const FLOAT fSpeedSQMax          = 100.0f;
static const FLOAT fSpeedSQStep         = 0.1f;

static const FLOAT fAccelMin		    = 0.0f;
static const FLOAT fAccelMax		    = 100.0f;
static const FLOAT fAccelStep			= 0.1f;

static const FLOAT fBoneChangesMin      = 0.0f;
static const FLOAT fBoneChangesMax      = 2.0f;
static const FLOAT fBoneChangesStep     = 0.01f;

static const DOUBLE fMinErrorThreshold  = 0.25;
static const DOUBLE fMaxErrorThreshold  = 0.5;

// At runtime we get a per frame results, so we need to filter the results to a per gesture
// result. The filter is implemented as a sliding window with two parameters, the size of
// the sliding window and a threshold, almost like a amplitude and frequency. These constants
// define a matrix of possible values of these two parameters which we'll use to find the
// most optimum pair of parameters for filtering.
static const FLOAT fDetectionParamsMinThreshold     = 0.0f;
static const FLOAT fDetectionParamsMaxThreshold     = 0.1f;
static const FLOAT fDetectionParamsThresholdStep    = 0.001f;
static const UINT nDetectionParamsMinNumFrames      = 1;
static const UINT nDetectionParamsMaxNumFrames      = 10;


//--------------------------------------------------------------------------------------
// Static definitions
//--------------------------------------------------------------------------------------

UINT64 GestureDetector::m_uPreviousTimeStamp = 0;
UINT GestureDetector::m_nNumInstances = 0;
#ifdef _XBOX
ClassifierData** GestureDetector::m_ClassifierData = NULL;
UINT GestureDetector::m_nNumClassifierData = 0;
BYTE* GestureDetector::m_pClassifierDataUsed = NULL;
#else
vector<ClassifierData*> GestureDetector::m_ClassifierData;
const CHAR* GestureDetectorTrainer::m_szTagID = "TAG:";
#endif


//--------------------------------------------------------------------------------------
// Name: DecisionStump()
// Desc: Constructor
//--------------------------------------------------------------------------------------

DecisionStump::DecisionStump()
{
    m_fThreshold = 0.0f;
#ifndef _XBOX
    m_iReverse = 1;
#endif
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read data
//--------------------------------------------------------------------------------------

HRESULT DecisionStump::Read( FILE* pFile )
{
    fread( &m_fThreshold, sizeof( m_fThreshold ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

#ifndef _XBOX
    m_fThreshold = ByteSwap32Bit( m_fThreshold );
#endif

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read data
//--------------------------------------------------------------------------------------

HRESULT DecisionStump::Read( VOID* pBuffer )
{
    mread( &m_fThreshold, sizeof( m_fThreshold ), 1, pBuffer );

#ifndef _XBOX
    m_fThreshold = ByteSwap32Bit( m_fThreshold );
#endif

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Write
// Desc: Write data
//--------------------------------------------------------------------------------------

HRESULT DecisionStump::Write( FILE* pFile )
{
    FLOAT fBigEndianValue = ByteSwap32Bit( m_fThreshold );
    fwrite( &fBigEndianValue, sizeof( fBigEndianValue ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: WeakClassifier()
// Desc: Constructor
//--------------------------------------------------------------------------------------

WeakClassifier::WeakClassifier() : DecisionStump()
{
    m_fAlpha    = 0.0f;
    m_pData     = NULL;
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read data
//--------------------------------------------------------------------------------------

HRESULT WeakClassifier::Read( FILE* pFile, UINT* pDataID )
{
    UINT uValue;
    RETURN_ON_FAIL( DecisionStump::Read( pFile ) );

    fread( &m_fAlpha, sizeof( m_fAlpha ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    fread( &uValue, sizeof( uValue ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

#ifndef _XBOX
    m_fAlpha = ByteSwap32Bit( m_fAlpha );
    uValue = ByteSwap32Bit( uValue );
#endif

    *pDataID = uValue;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read data
//--------------------------------------------------------------------------------------

HRESULT WeakClassifier::Read( VOID* pBuffer, UINT* pDataID )
{
    UINT uValue;
    RETURN_ON_FAIL( DecisionStump::Read( pBuffer ) );

    mread( &m_fAlpha, sizeof( m_fAlpha ), 1, pBuffer );
    mread( &uValue, sizeof( uValue ), 1, pBuffer );

#ifndef _XBOX
    m_fAlpha = ByteSwap32Bit( m_fAlpha );
    uValue = ByteSwap32Bit( uValue );
#endif

    *pDataID = uValue;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Write
// Desc: Write data
//--------------------------------------------------------------------------------------

HRESULT WeakClassifier::Write( FILE* pFile )
{
    RETURN_ON_FAIL( DecisionStump::Write( pFile ) );

    FLOAT fBigEndianValue = ByteSwap32Bit( m_fAlpha );
    fwrite( &fBigEndianValue, sizeof( fBigEndianValue ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    UINT uBigEndianValue;
    if ( m_pData )
    {
        uBigEndianValue = ByteSwap32Bit( m_pData->GetID() );
        fwrite( &uBigEndianValue, sizeof( uBigEndianValue ), 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );
    }
    else
    {
        uBigEndianValue = (UINT)-1;
        fwrite( &uBigEndianValue, sizeof( uBigEndianValue ), 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );
        return E_FAIL;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: StrongClassifier()
// Desc: Constructor
//--------------------------------------------------------------------------------------

StrongClassifier::StrongClassifier()
{
#ifdef _XBOX
    m_WeakClassifiers = NULL;
    m_nNumWeakClassifiers = 0;
#else
    m_WeakClassifiers.clear();
#endif

    m_fTotalAlpha                       = 0.0f;
    m_fFilterPerFrameResultsThreshold   = 0.001f;
    m_nFilterPerFrameResultsNumFrames   = 5;

    for ( UINT i = 0; i < NUM_PLAYERS; i++ )
    {
        Reset( i );
    }
}


//--------------------------------------------------------------------------------------
// Name: ~StrongClassifier()
// Desc: Destructor
//--------------------------------------------------------------------------------------

StrongClassifier::~StrongClassifier()
{
#ifdef _XBOX
    if ( m_WeakClassifiers )
    {
        delete [] m_WeakClassifiers;
    }
    m_WeakClassifiers = NULL;
    m_nNumWeakClassifiers = 0;
#else
    m_WeakClassifiers.clear();
#endif
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Initialize and allocate memory
//--------------------------------------------------------------------------------------

HRESULT StrongClassifier::Initialize( const UINT nNumWeakClassifiers )
{
#ifdef _XBOX
    if ( m_WeakClassifiers )
    {
        delete [] m_WeakClassifiers;
    }

    RETURN_ON_NULL( m_WeakClassifiers = new WeakClassifier[ nNumWeakClassifiers ] );

    m_nNumWeakClassifiers = nNumWeakClassifiers;
#else
    m_WeakClassifiers.reserve( nNumWeakClassifiers );

    for ( UINT i = 0; i < nNumWeakClassifiers; i++ )
    {
        m_WeakClassifiers.push_back( WeakClassifier() );
    }
#endif
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read data
//--------------------------------------------------------------------------------------

HRESULT StrongClassifier::Read( FILE* pFile )
{
    fread( &m_fTotalAlpha, sizeof( m_fTotalAlpha ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    fread( &m_fFilterPerFrameResultsThreshold, sizeof( m_fFilterPerFrameResultsThreshold ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    fread( &m_nFilterPerFrameResultsNumFrames, sizeof( m_nFilterPerFrameResultsNumFrames ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

#ifndef _XBOX
    m_fTotalAlpha = ByteSwap32Bit( m_fTotalAlpha );
    m_fFilterPerFrameResultsThreshold = ByteSwap32Bit( m_fFilterPerFrameResultsThreshold );
    m_nFilterPerFrameResultsNumFrames = ByteSwap32Bit( m_nFilterPerFrameResultsNumFrames );
#endif

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read data
//--------------------------------------------------------------------------------------

HRESULT StrongClassifier::Read( VOID* pBuffer )
{
    mread( &m_fTotalAlpha, sizeof( m_fTotalAlpha ), 1, pBuffer );
    mread( &m_fFilterPerFrameResultsThreshold, sizeof( m_fFilterPerFrameResultsThreshold ), 1, pBuffer );
    mread( &m_nFilterPerFrameResultsNumFrames, sizeof( m_nFilterPerFrameResultsNumFrames ), 1, pBuffer );

#ifndef _XBOX
    m_fTotalAlpha = ByteSwap32Bit( m_fTotalAlpha );
    m_fFilterPerFrameResultsThreshold = ByteSwap32Bit( m_fFilterPerFrameResultsThreshold );
    m_nFilterPerFrameResultsNumFrames = ByteSwap32Bit( m_nFilterPerFrameResultsNumFrames );
#endif

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Write
// Desc: Write data
//--------------------------------------------------------------------------------------

HRESULT StrongClassifier::Write( FILE* pFile )
{
    FLOAT fBigEndianValue = ByteSwap32Bit( m_fTotalAlpha );
    fwrite( &fBigEndianValue, sizeof( fBigEndianValue ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    fBigEndianValue = ByteSwap32Bit( m_fFilterPerFrameResultsThreshold );
    fwrite( &fBigEndianValue, sizeof( fBigEndianValue ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    UINT uBigEndianValue = ByteSwap32Bit( m_nFilterPerFrameResultsNumFrames );
    fwrite( &uBigEndianValue, sizeof( uBigEndianValue ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read data for a weak classifier in the strong classifier
//--------------------------------------------------------------------------------------

HRESULT StrongClassifier::Read( FILE* pFile, const UINT uWeakClassifierIndex, UINT* pDataID )
{
    if ( uWeakClassifierIndex >= GetNumWeakClassifiers() )
    {
        return E_FAIL;
    }
    return m_WeakClassifiers[ uWeakClassifierIndex ].Read( pFile, pDataID );
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read data for a weak classifier in the strong classifier
//--------------------------------------------------------------------------------------

HRESULT StrongClassifier::Read( VOID* pBuffer, const UINT uWeakClassifierIndex, UINT* pDataID )
{
    if ( uWeakClassifierIndex >= GetNumWeakClassifiers() )
    {
        return E_FAIL;
    }
    return m_WeakClassifiers[ uWeakClassifierIndex ].Read( pBuffer, pDataID );
}


//--------------------------------------------------------------------------------------
// Name: Write
// Desc: Write data for weak classifier in the strong classifier
//--------------------------------------------------------------------------------------

HRESULT StrongClassifier::Write( FILE* pFile, const UINT uWeakClassifierIndex )
{
    if ( uWeakClassifierIndex >= GetNumWeakClassifiers() )
    {
        return E_FAIL;
    }
    return m_WeakClassifiers[ uWeakClassifierIndex ].Write( pFile );
}


//--------------------------------------------------------------------------------------
// Name: Classify
// Desc: The classifications for the strong classifier as a weighted sum of the weak classifiers
//--------------------------------------------------------------------------------------

#ifdef _XBOX
INT StrongClassifier::Classify( const UINT uPlayerIdx, ClassifierData** __restrict classifierData, FLOAT* pConfidence )
{
    FLOAT fSum = 0.0f;

    const UINT nNumWeakClassifiers = m_nNumWeakClassifiers;

    WeakClassifier* pWeakClassifier = &m_WeakClassifiers[ 0 ];

    UINT uClassifierDataIndex = pWeakClassifier->GetData()->GetID();

    // Prefetch data
    __dcbt( 0, pWeakClassifier );
    __dcbt( 128, pWeakClassifier );

    for ( UINT i = 0; i < nNumWeakClassifiers; i++ )
    {
        uClassifierDataIndex = pWeakClassifier->GetData()->GetID();

        FLOAT fAlpha = pWeakClassifier->GetAlpha();
        FLOAT fValue = classifierData[ uClassifierDataIndex ]->GetValue( uPlayerIdx );              

        fSum += fAlpha * pWeakClassifier->Classify( fValue );

        pWeakClassifier++;

        __dcbt( 128, pWeakClassifier );
    }

    // Normalize the confidence
    assert( m_fTotalAlpha > 0.0f );
    *pConfidence = fSum / m_fTotalAlpha;

    if ( fSum > 0.0f )
    {
        return (INT)g_fClassificationLabelCorrect;
    }

    return (INT)g_fClassificationLabelIncorrect;
}
#else
INT8 StrongClassifier::Classify( const UINT uPlayerIdx, const vector<ClassifierData*>& classifierData, FLOAT* pConfidence )
{
    FLOAT fSum = 0.0f;

    const UINT nNumWeakClassifiers = (UINT)m_WeakClassifiers.size();

    for ( UINT i = 0; i < nNumWeakClassifiers; i++ )
    {
        WeakClassifier* pWeakClassifier = &m_WeakClassifiers[ i ];
        UINT uClassifierDataIndex = pWeakClassifier->GetData()->GetID();

        FLOAT fValue = classifierData[ uClassifierDataIndex ]->GetValue( uPlayerIdx );
        FLOAT fAlpha = pWeakClassifier->GetAlpha();

        fSum += fAlpha * pWeakClassifier->Classify( fValue );
    }

    // Normalize the confidence value
    assert( m_fTotalAlpha > 0.0f );
    *pConfidence = fSum / m_fTotalAlpha;

    if ( fSum > 0.0f )
    {
        return g_iClassificationLabelCorrect;
    }

    return g_iClassificationLabelIncorrect;
}
#endif


//--------------------------------------------------------------------------------------
// Name: Detect
// Desc: Calls Classify() and can optionally filter the per frame classifications
//--------------------------------------------------------------------------------------
#ifdef _XBOX
BOOL StrongClassifier::Detect( const UINT uPlayerIdx, ClassifierData** __restrict classifierData, Results* pResults, const BOOL bFilterResults )
#else
BOOL StrongClassifier::Detect( const UINT uPlayerIdx, const vector<ClassifierData*>& classifierData, Results* pResults, const BOOL bFilterResults )
#endif
{
    FLOAT fConfidence;

    // Do the classification.
    Classify( uPlayerIdx, classifierData, &fConfidence );

    // Filter the per frame results to per gesture results
    if ( bFilterResults )
    {
        FilterDetectionResults( uPlayerIdx, fConfidence, pResults );
    }
    else
    {        
        pResults->m_fConfidence = fConfidence;
        pResults->m_bDetected   = ( fConfidence > 0.0f );
        pResults->m_bFirstFrameDetected = pResults->m_bDetected;
    }

    return pResults->m_bDetected;
}


//--------------------------------------------------------------------------------------
// Name: FilterDetectionResults
// Desc: Classification is done per frame, not per gesture. We can filter the results
//       in anyway we want. This is a simple default filtering method provided that
//       uses the sum of a sliding window of results. The size of the sliding window
//       can be seen as a frequency and the threshold as an amplitude.
//--------------------------------------------------------------------------------------

VOID StrongClassifier::FilterDetectionResults( const UINT uPlayerIdx, const FLOAT fConfidence, Results* pResults )
{
    // Use a small sliding window of previous results to do the filtering
    if ( m_fClassificationHistory[ uPlayerIdx ].size() >= m_nFilterPerFrameResultsNumFrames )
    {
        m_fClassificationHistory[ uPlayerIdx ].pop_front();
    }

    // Filter based on the sum of the clamped confidence values
    FLOAT fSum = 0.0f;
    for ( UINT i = 0; i < m_fClassificationHistory[ uPlayerIdx ].size(); i++ )
    {
        fSum += max( 0.0f, m_fClassificationHistory[ uPlayerIdx ][ i ] );
    }

    // Determine if any previous frames in the sliding window had positive classifications
    BOOL bNoPreviousDetections = ( fSum <= m_fFilterPerFrameResultsThreshold );

    // Add the new confidence value to the sliding window
    m_fClassificationHistory[ uPlayerIdx ].push_back( fConfidence );
    fSum += max( 0.0f, fConfidence );

    // Output the results
    pResults->m_fConfidence         = fSum;
    pResults->m_bDetected           = ( fSum > m_fFilterPerFrameResultsThreshold );
    pResults->m_bFirstFrameDetected = ( bNoPreviousDetections && pResults->m_bDetected );
}

//--------------------------------------------------------------------------------------
// Name: Optimize
// Desc: Ten's of thousands of weak classifiers can be generated during learning, but
//       we only neead about 1000. This allows us to reduce the number of weak classifiers
//       that will be used at runtime
//--------------------------------------------------------------------------------------
#ifndef _XBOX
VOID StrongClassifier::Optimize( const UINT nMaxNumClassifers, const BOOL bBakeReverseInAlpha )
{
    // Optimize for runtime by reducing the number of weak classifiers that contribute
    // to the strong classifier. We simply sort the weak classifiers based on the
    // weights in the strong classifier and take the first N classifiers

    // Sort using alpha
    sort( m_WeakClassifiers.rbegin(), m_WeakClassifiers.rend() );

    // Now throw away all classifiers with zero alpha
    const UINT nNumWeakClassifiers = (UINT)m_WeakClassifiers.size();
    UINT uLastValid = nNumWeakClassifiers;
    for ( UINT i = 0; i < nNumWeakClassifiers; i++ )
    {
        if ( m_WeakClassifiers[ i ].GetAlpha() == 0.0f )
        {
            uLastValid = i;
            break;
        }
    }
    m_WeakClassifiers.resize( uLastValid );

    // Now prune to the max number of classifers asked, prune the classifiers with the lowest alphas
    if ( nMaxNumClassifers > 0 )
    {
        UINT nNumClassifiers = min( (UINT)m_WeakClassifiers.size(), nMaxNumClassifers );
        m_WeakClassifiers.resize( nNumClassifiers );
    }

    // Normalize the weak classifier weights, so that we get a probability value in the range [-1..1]
    // when detecting. -1 is 100% certain this is not the gesture, 0 don't know, 1 is 100% sure this is the gesture
    FLOAT fSum = 0;
    for ( UINT i = 0; i < m_WeakClassifiers.size(); i++ )
    {
        fSum += m_WeakClassifiers[ i ].GetAlpha();
    }
    SetTotalAlpha( fSum );

    // Another runtime optimization is to bake fReverse into fAlpha. This reduces the data size
    // per classifier and also removes one float multiply per classifier
    if ( bBakeReverseInAlpha )
    {
        for ( UINT i = 0; i < m_WeakClassifiers.size(); i++ )
        {
            FLOAT fAlpha = m_WeakClassifiers[ i ].GetAlpha();
            INT8 iReverse = m_WeakClassifiers[ i ].GetReverseValue();
        
            // Bake the reverse value into fAlpha and reset the reverse value
            fAlpha *= (FLOAT)iReverse;
            m_WeakClassifiers[ i ].SetAlpha( fAlpha );
            m_WeakClassifiers[ i ].SetIsReversed( FALSE );
        }
    }
}
#endif


//--------------------------------------------------------------------------------------
// Name: GestureDetector()
// Desc: Constructor
//--------------------------------------------------------------------------------------

GestureDetector::GestureDetector()
{
    for ( UINT i = 0; i < NUM_PLAYERS; i++ )
    {
        m_StrongClassifier.Reset( i );
    }
    m_nNumInstances++;
}


//--------------------------------------------------------------------------------------
// Name: ~GestureDetector()
// Desc: Destructor
//--------------------------------------------------------------------------------------

GestureDetector::~GestureDetector()
{
    m_nNumInstances--;

    if ( m_nNumInstances == 0 )
    {
        FreeClassifierData();
    }
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Initialize and allocate memory used by the classifier data
//--------------------------------------------------------------------------------------

HRESULT GestureDetector::Initialize( const UINT nNumWeakClassifiers, const UINT nNumClassifierData )
{
    RETURN_ON_FAIL( m_StrongClassifier.Initialize( nNumWeakClassifiers ) );

#ifdef _XBOX
    m_nNumClassifierData = nNumClassifierData;

    if ( !m_ClassifierData )
    {
        RETURN_ON_NULL( m_ClassifierData = new ClassifierData*[ nNumClassifierData ] );

        for ( UINT i = 0; i < m_nNumClassifierData; i++ )
        {
            m_ClassifierData[ i ] = NULL;
        }
    }

    if ( !m_pClassifierDataUsed )
    {
        RETURN_ON_NULL( m_pClassifierDataUsed = new BYTE[ nNumClassifierData ] );

        for ( UINT i = 0; i < m_nNumClassifierData; i++ )
        {
            m_pClassifierDataUsed[ i ] = 0;
        }
    }
#else
    m_ClassifierData.clear();
    m_ClassifierData.reserve( nNumClassifierData );
    for ( UINT i = 0; i < nNumClassifierData; i++ )
    {
        m_ClassifierData.push_back( NULL );
    }
#endif

    m_uPreviousTimeStamp = 0;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: FreeClassifierData
// Desc: Free memory allocated for classifier data
//--------------------------------------------------------------------------------------

VOID GestureDetector::FreeClassifierData()
{
#ifdef _XBOX
    if ( m_ClassifierData )
    {
        for ( UINT i = 0; i < m_nNumClassifierData; i++ )
        {
            if ( m_ClassifierData[ i ] )
            {
                delete m_ClassifierData[ i ];
                m_ClassifierData[ i ] = NULL;
            }
        }
        delete [] m_ClassifierData;
        m_ClassifierData = NULL;
        m_nNumClassifierData = 0;
    }

    if ( m_pClassifierDataUsed )
    {
        delete [] m_pClassifierDataUsed;
        m_pClassifierDataUsed = NULL;
    }
#else
    for ( UINT i = 0; i < m_ClassifierData.size(); i++ )
    {
        if ( m_ClassifierData[ i ] )
        {
            delete m_ClassifierData[ i ];
            m_ClassifierData[ i ] = NULL;
        }
    }
    m_ClassifierData.clear();
#endif
}


//--------------------------------------------------------------------------------------
// Name: Load
// Desc: Load data
//--------------------------------------------------------------------------------------

HRESULT GestureDetector::Load( const CHAR* szFileName )
{
    FILE* pFile = NULL;
    fopen_s( &pFile, szFileName, "rb" );
    RETURN_ON_NULL( pFile );

    // Check that this is indeed a gesture file
    CHAR szFileID[ sizeof( g_szGestureFileID ) ];
    fread( szFileID, sizeof( g_szGestureFileID ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    UINT nNumClassifierData;
    UINT nNumWeakClassifiers;

    // Check the file header
    if ( !strcmp( szFileID, g_szGestureFileID ) )
    {
        // Read the version number. This is only added for backwards compatibility when we need to change file formats in the future
        FLOAT fVersion;
        fread( &fVersion, sizeof( fVersion ), 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );
#ifndef _XBOX
        fVersion = ByteSwap32Bit( fVersion );
#endif

        // Check that the file version is valid and can be loaded
        if ( fabsf( fVersion - g_fCurrentVersion ) > 1.0f )
        {
            fclose( pFile );
            printf( "\nError: File version %f != current version %f\n", fVersion, g_fCurrentVersion );
            return E_FAIL;
        }

        fread( &nNumWeakClassifiers, sizeof( nNumWeakClassifiers ), 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );
#ifndef _XBOX
        nNumWeakClassifiers = ByteSwap32Bit( nNumWeakClassifiers );
#endif

        fread( &nNumClassifierData, sizeof( nNumClassifierData ), 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );
#ifndef _XBOX
        nNumClassifierData = ByteSwap32Bit( nNumClassifierData );
#endif

        RETURN_ON_FAIL( Initialize( nNumWeakClassifiers, nNumClassifierData ) );

        for ( UINT i = 0; i < nNumClassifierData; i++ )
        {
            // Classifier data is shared between weak classifiers, there it's static. Therefoer
            // we only want to allocate the memory once
            if ( !m_ClassifierData[ i ] )
            {
                RETURN_ON_NULL( m_ClassifierData[ i ] = ClassifierData::CreatNewInstance( pFile ) );
                RETURN_ON_FAIL( m_ClassifierData[ i ]->Read( pFile ) );
            }
            else
            {
                // If the memory has already been allocated, we still need to advance the file pointer
                ClassifierData* pClassifierData = ClassifierData::CreatNewInstance( pFile );
                RETURN_ON_NULL( pClassifierData );
                RETURN_ON_FAIL( pClassifierData->Read( pFile ) );
                delete pClassifierData;
            }
        }

        // Read classifiers
        RETURN_ON_FAIL( m_StrongClassifier.Read( pFile ) );
        for ( UINT i = 0; i < nNumWeakClassifiers; i++ )
        {
            UINT uDataID;
            RETURN_ON_FAIL( m_StrongClassifier.Read( pFile, i, &uDataID ) );
        
            // Setup data pointer in classifier
            WeakClassifier* pWeakClassifier = m_StrongClassifier.GetWeakClassifierAt( i );
            pWeakClassifier->SetData( m_ClassifierData[ uDataID ] );

#ifdef _XBOX
            // Update that this classifier data is actually being used
            m_pClassifierDataUsed[ uDataID ] = 1;
#endif
        }
    }
    else
    {
        fclose( pFile );
        return E_FAIL;
    }

    fclose( pFile );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: LoadFromMemory
// Desc: Load data from memory location
//--------------------------------------------------------------------------------------

HRESULT GestureDetector::LoadFromMemory( VOID* pBuffer )
{
    RETURN_ON_NULL( pBuffer );

    // Check that this is indeed a gesture file
    CHAR szFileID[ sizeof( g_szGestureFileID ) ];
    mread( szFileID, sizeof( g_szGestureFileID ), 1, pBuffer );

    UINT nNumClassifierData;
    UINT nNumWeakClassifiers;

    // Check the file header
    if ( !strcmp( szFileID, g_szGestureFileID ) )
    {
        // Read the version number. This is only added for backwards compatibility when we need to change file formats in the future
        FLOAT fVersion;
        mread( &fVersion, sizeof( fVersion ), 1, pBuffer );

#ifndef _XBOX
        fVersion = ByteSwap32Bit( fVersion );
#endif

        // Check that the file version is the same as the current version
        if ( fabsf( fVersion - g_fCurrentVersion ) > FLT_EPSILON )
        {
            mclose();
            printf( "\nError: File version %f != current version %f\n", fVersion, g_fCurrentVersion );
            return E_FAIL;
        }

        mread( &nNumWeakClassifiers, sizeof( nNumWeakClassifiers ), 1, pBuffer );
#ifndef _XBOX
        nNumWeakClassifiers = ByteSwap32Bit( nNumWeakClassifiers );
#endif

        mread( &nNumClassifierData, sizeof( nNumClassifierData ), 1, pBuffer );
#ifndef _XBOX
        nNumClassifierData = ByteSwap32Bit( nNumClassifierData );
#endif

        RETURN_ON_FAIL( Initialize( nNumWeakClassifiers, nNumClassifierData ) );

        for ( UINT i = 0; i < nNumClassifierData; i++ )
        {
            // Classifier data is shared between weak classifiers, there it's static. Therefore
            // we only want to allocate the memory once
            if ( !m_ClassifierData[ i ] )
            {
                RETURN_ON_NULL( m_ClassifierData[ i ] = ClassifierData::CreatNewInstance( pBuffer ) );
                RETURN_ON_FAIL( m_ClassifierData[ i ]->Read( pBuffer ) );
            }
            else
            {
                // If the memory has already been allocated, we still need to advance the file pointer
                ClassifierData* pClassifierData = ClassifierData::CreatNewInstance( pBuffer );
                RETURN_ON_NULL( pClassifierData );
                RETURN_ON_FAIL( pClassifierData->Read( pBuffer ) );
                delete pClassifierData;
            }
        }

        // Read classifiers
        RETURN_ON_FAIL( m_StrongClassifier.Read( pBuffer ) );
        for ( UINT i = 0; i < nNumWeakClassifiers; i++ )
        {
            UINT uDataID;
            RETURN_ON_FAIL( m_StrongClassifier.Read( pBuffer, i, &uDataID ) );
        
            // Setup data pointer in classifier
            WeakClassifier* pWeakClassifier = m_StrongClassifier.GetWeakClassifierAt( i );
            pWeakClassifier->SetData( m_ClassifierData[ uDataID ] );

#ifdef _XBOX
            // Update that this classifier data is actually being used
            m_pClassifierDataUsed[ uDataID ] = 1;
#endif
        }
    }
    else
    {
        mclose();
        return E_FAIL;
    }

    mclose();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Detect
// Desc: Does the gesture detection
//--------------------------------------------------------------------------------------

BOOL GestureDetector::Detect( const UINT uPlayerIdx, Results* pResults, const BOOL bFilterResults )
{
    assert( uPlayerIdx < NUM_PLAYERS );
    assert( pResults );

    NUI_SKELETON_DATA* pSkeletonData = ClassifierData::GetCurrentSkeleton( uPlayerIdx );
    if ( pSkeletonData && pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED )
    {
        return m_StrongClassifier.Detect( uPlayerIdx, m_ClassifierData, pResults, bFilterResults );
    }

#ifdef _XBOX
    pResults->m_fConfidence         = bFilterResults ? 0.0f : (FLOAT)g_fClassificationLabelIncorrect;
#else
    pResults->m_fConfidence         = bFilterResults ? 0.0f : (FLOAT)g_iClassificationLabelIncorrect;
#endif
    pResults->m_bDetected           = FALSE;
    pResults->m_bFirstFrameDetected = FALSE;

    return FALSE;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Per frame update of classifier data that is shared between weak classifiers
//--------------------------------------------------------------------------------------

VOID GestureDetector::Update( const UINT uPlayerIdx, const NUI_SKELETON_DATA* pSkeletonData, const LARGE_INTEGER& liTimeStampFromNuiFrame, XMVECTOR* pNormalToGravity )
{
    BOOL bReset = FALSE;
    UINT64 uTimeStamp = liTimeStampFromNuiFrame.QuadPart;

    // Xed files record UINT64 time stamps which are different from LARGE_INTEGER time stamps from NUI_SKELETON_FRAME at runtime
#ifdef _XBOX
    FLOAT fDeltaTimeInSeconds = ( uTimeStamp - m_uPreviousTimeStamp ) * 0.001f;
#else
    FLOAT fDeltaTimeInSeconds = ( uTimeStamp - m_uPreviousTimeStamp ) * 0.000001f;
#endif
    if ( m_uPreviousTimeStamp == 0 )
    {
        fDeltaTimeInSeconds = 0.0f;
        bReset = TRUE;
    }
    m_uPreviousTimeStamp = uTimeStamp;

    NUI_SKELETON_DATA skeleton;
#ifdef _XBOX
    XMemCpy( &skeleton, pSkeletonData, sizeof( NUI_SKELETON_DATA ) );
#else
    memcpy( &skeleton, pSkeletonData, sizeof( NUI_SKELETON_DATA ) );
#endif

    if ( pNormalToGravity )
    {
        // Apply tilt correction on the data
        ApplyTiltCorrection( uPlayerIdx, &skeleton, pSkeletonData, pNormalToGravity, bReset );
    }

    // Update the sliding window of skeleton data frames
    ClassifierData::UpdateHistory( uPlayerIdx, &skeleton, fDeltaTimeInSeconds, &bReset );
    
    if ( bReset )
    {
        // Make sure we have a valid delta time, so just use 33ms
        fDeltaTimeInSeconds = 0.033f;
    }

    // Update the values in the classifier data
#ifdef _XBOX
    const UINT nNumData = m_nNumClassifierData;
#else
    const UINT nNumData = (UINT)m_ClassifierData.size();
#endif

#ifdef _XBOX
    __dcbt( 0, m_ClassifierData );
    __dcbt( 128, m_ClassifierData );
#endif

    for ( UINT i = 0; i < nNumData; i++ )
    {
#ifdef _XBOX
        if ( m_pClassifierDataUsed[ i ] )
#endif
        {
            m_ClassifierData[ i ]->Update( uPlayerIdx, &fDeltaTimeInSeconds );
        }

#ifdef _XBOX
        __dcbt( 128, m_ClassifierData[ i ] );
#endif
    }
}


//--------------------------------------------------------------------------------------
// Name: ApplyTiltCorrection
// Desc: Applies tilt correction to the skeleton data. Source and destination can be the same
//--------------------------------------------------------------------------------------

VOID GestureDetector::ApplyTiltCorrection( const UINT uPlayerIdx, NUI_SKELETON_DATA* pDstSkeleton, const NUI_SKELETON_DATA* pSrcSkeleton,
                                           XMVECTOR* pNormalToGravity, const BOOL bReset )
{
    if ( !pDstSkeleton ||
         !pSrcSkeleton )
    {
        return;
    }

    static const XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    static XMVECTOR vAverageNormalToGravity[ NUM_PLAYERS ] = { vUp, vUp };

    if ( bReset )
    {
        vAverageNormalToGravity[ uPlayerIdx ] = *pNormalToGravity;
    }

    // Get our Up vector from sensor.
    XMVECTOR vNormToGrav = *pNormalToGravity;

    // Check for an invalid up vector
    XMVECTOR vDot = XMVector3Dot( vNormToGrav, vNormToGrav );
    if ( fabsf( XMVectorGetX( vDot ) ) < FLT_EPSILON )
    {
        vNormToGrav = vUp;
    }

    // Average this a lot so that it doesn't add jumpiness ot the scene.
    vAverageNormalToGravity[ uPlayerIdx ] = XMVectorLerp( vAverageNormalToGravity[ uPlayerIdx ], vNormToGrav, 0.1f );

    if ( pSrcSkeleton->eTrackingState != NUI_SKELETON_TRACKED )
    {
        return;
    }

    // Generate the leveling matrix and apply it
    XMMATRIX matLevel;

    // Normalize
    vNormToGrav = XMVector4Normalize( vAverageNormalToGravity[ uPlayerIdx ] );

    // Rotation axis
    XMVECTOR vAxis = XMVector3Cross( vNormToGrav, vUp );

    // if the rotation axis is zero, then Gravity == Camera
    // therefore return the Identity Matrix.
    if ( XMVector4Equal( vAxis, XMVectorZero() ) )
    {
        matLevel = XMMatrixIdentity();
    }
    else
    {
        // angle to rotate
        XMVECTOR vAngle = XMVector4Dot( vUp, vNormToGrav );    
        FLOAT fAngle = acosf( XMVectorGetX( vAngle ) );
        matLevel = XMMatrixRotationAxis( vAxis, fAngle );
    }

    for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
    {
        pDstSkeleton->SkeletonPositions[ i ] = XMVector3Transform( pSrcSkeleton->SkeletonPositions[ i ], matLevel );
    }
}


#ifndef _XBOX

//--------------------------------------------------------------------------------------
// Name: GestureDetectorTrainer()
// Desc: Constructor
//--------------------------------------------------------------------------------------

GestureDetectorTrainer::GestureDetectorTrainer() : GestureDetector()
{
    Reset();
    m_nTotalNumGestures             = 0;
    m_nNumTrainingGestures          = 0;
    m_nNumThreadsForTraining        = 0;
    m_nMaxNumThreads                = omp_get_max_threads();
    m_nNumWeakClassifiersAtRuntime  = 0;
    m_fErrorThreshold               = 0.0f;
}


//--------------------------------------------------------------------------------------
// Name: ~GestureDetectorTrainer
// Desc: Destructor
//--------------------------------------------------------------------------------------

GestureDetectorTrainer::~GestureDetectorTrainer()
{
    Reset();
}


//--------------------------------------------------------------------------------------
// Name: Reset
// Desc: Reset all state and delete allocated memory for labeled example data
//--------------------------------------------------------------------------------------

VOID GestureDetectorTrainer::Reset()
{
    for ( UINT i = 0; i < NUM_PLAYERS; i++ )
    {
        m_StrongClassifier.Reset( i );
    }

    m_uPreviousTimeStamp = 0;

    UINT nNumExamples = (UINT)( m_LabeledExamples.m_pExamples.size() );
    for ( UINT i = 0; i < nNumExamples; i++ )
    {
        if ( m_LabeledExamples.m_pExamples[ i ] )
        {
            _aligned_free( m_LabeledExamples.m_pExamples[ i ] );
            m_LabeledExamples.m_pExamples[ i ] = NULL;
        }
    }

    m_LabeledExamples.m_pExamples.clear();
    m_LabeledExamples.m_iLabels.clear();
    m_LabeledExamples.m_uTimeStamps.clear();

    m_nTotalNumGestures = 0;
    m_nNumTrainingGestures = 0;
}


//--------------------------------------------------------------------------------------
// Name: Save
// Desc: Save data
//--------------------------------------------------------------------------------------

HRESULT GestureDetectorTrainer::Save( const CHAR* szFileName )
{
    FILE* pFile = NULL;
    fopen_s( &pFile, szFileName, "wb" );
    RETURN_ON_NULL( pFile );

    // Write a text identifier
    fwrite( g_szGestureFileID, sizeof( g_szGestureFileID ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    // Write current version number
    FLOAT fBigEndianValue = ByteSwap32Bit( g_fCurrentVersion );
    fwrite( &fBigEndianValue, sizeof( fBigEndianValue ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    // Write weak classifiers, weights and data
    const UINT nNumWeakClassifiers = m_StrongClassifier.GetNumWeakClassifiers();
    const UINT nNumClassifierData = (UINT)m_ClassifierData.size();

    UINT32 uBigEndianValue = ByteSwap32Bit( (UINT32)nNumWeakClassifiers );
    fwrite( &uBigEndianValue, sizeof( uBigEndianValue ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );
    
    uBigEndianValue = ByteSwap32Bit( (UINT32)nNumClassifierData );
    fwrite( &uBigEndianValue, sizeof( uBigEndianValue ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    for ( UINT i = 0; i < nNumClassifierData; i++ )
    {
        RETURN_ON_FAIL( m_ClassifierData[ i ]->Write( pFile ) );
    }

    RETURN_ON_FAIL( m_StrongClassifier.Write( pFile ) );
    for ( UINT i = 0; i < nNumWeakClassifiers; i++ )
    {
        RETURN_ON_FAIL( m_StrongClassifier.Write( pFile, i ) );
    }

    fclose( pFile );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Train
// Desc: Uses the AdaBoost training algorithm to train a strong classifier H(x) as a
//       weighted sum of weak classifiers h(x)
//--------------------------------------------------------------------------------------

HRESULT GestureDetectorTrainer::Train( const CHAR* szPath, const DOUBLE fAccuracyLevel, const UINT nNumWeakClassifiersAtRuntime,
                                       const FLOAT fWeightOfFalsePositivesWhenFiltering, const BOOL bOnlyRetrainDetectionParameters,
                                       const UINT uCPUAvailableForTraining )
{
    m_nNumWeakClassifiersAtRuntime = nNumWeakClassifiersAtRuntime;

    // Accuracy level is an input value between between [0..1], which simply gets converted to a error threshold
    // value of maximum 0.5, since for AdaBoost weak classifiers has to be better thana 50/50 change to be correct
    m_fErrorThreshold = ( fAccuracyLevel * ( fMaxErrorThreshold - fMinErrorThreshold ) ) + fMinErrorThreshold;

    // Set how much CPU resources the user is willing to use for training. Calculate the number of threads from the % utilization
    m_nNumThreadsForTraining = (UINT)( 0.5f + ( m_nMaxNumThreads * uCPUAvailableForTraining / 100.0f ) );
    BOOL bSetDynamic = ( m_nNumThreadsForTraining == m_nMaxNumThreads ) ? FALSE : TRUE;
    omp_set_dynamic( bSetDynamic );
    omp_set_num_threads( m_nNumThreadsForTraining );

    printf( "\n\nStep 1 of 4: Loading Labeled Training Examples" );
    DWORD dwStart = GetTickCount(); 
    RETURN_ON_FAIL( LoadLabeledExamples( szPath ) );
    DWORD dwStop = GetTickCount();

    DWORD dwSeconds = ( (dwStop - dwStart ) / 1000 ) % 60;
    DWORD dwMinutes = ( (dwStop - dwStart ) / 1000) / 60;
    printf( "\n\tNum Labeled %s Examples: %d", g_szTraining, GetNumExamples() );
    printf( "\n\tDuration: %d minutes, %d seconds",  dwMinutes, dwSeconds );
    printf( "\nDone\n" );
        
    printf( "\n\nStep 2 of 4: Generating a Pool of Weak Classifiers" );
    dwStart = GetTickCount();
    if ( !bOnlyRetrainDetectionParameters )
    {
        RETURN_ON_FAIL( TrainWeakClassifiers() );
    }
    dwStop = GetTickCount();

    dwSeconds = ( ( dwStop - dwStart ) / 1000 ) % 60;
    dwMinutes = ( ( dwStop - dwStart ) / 1000) / 60;
    printf( "\n\tNum weak classifiers generated: %d", GetNumWeakClassifiers() );
    printf( "\n\tDuration: %d minutes, %d seconds",  dwMinutes, dwSeconds );
    printf( "\nDone\n" );

    printf( "\n\nStep 3 of 4: Training Strong Classifier" );
    dwStart = GetTickCount();
    if ( !bOnlyRetrainDetectionParameters )
    {
        RETURN_ON_FAIL( TrainStrongClassifier( TRUE ) );
        Optimize( nNumWeakClassifiersAtRuntime );
    }
    dwStop = GetTickCount();

    dwSeconds = ( ( dwStop - dwStart ) / 1000 ) % 60;
    dwMinutes = ( ( dwStop - dwStart ) / 1000) / 60;
    printf( "\n\tNum weak classifiers: %d", GetNumWeakClassifiers() );
    printf( "\n\tDuration: %d minutes, %d seconds",  dwMinutes, dwSeconds );

    printf( "\n\nStep 4 of 4: Optimizing detection parameters" );
    dwStart = GetTickCount(); 
    OptimizeDetectionParameters( fWeightOfFalsePositivesWhenFiltering );
    dwStop = GetTickCount();

    dwSeconds = ( ( dwStop - dwStart ) / 1000 ) % 60;
    dwMinutes = ( ( dwStop - dwStart ) / 1000) / 60;
    printf( "\n\tFiltering %d frames using detection threshold %f", GetNumFramesToFilter(), GetDetectionThreshold() );
    printf( "\n\tDuration: %d minutes, %d seconds",  dwMinutes, dwSeconds );

    printf( "\nDone\n" );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Test
// Desc: Test the accuracy of the training algorithm
//--------------------------------------------------------------------------------------

HRESULT GestureDetectorTrainer::Test( const CHAR* szPath, const BOOL bTestOnTrainingData )
{
    if ( !bTestOnTrainingData )
    {
        RETURN_ON_FAIL( LoadLabeledExamples( szPath ) );
    }

    // Clear all the data
    ClassifierData::Initialize();
    m_StrongClassifier.Reset( 0 );
    m_uPreviousTimeStamp = 0;

    // run strong classifier on training data for verification
    UINT nNumGesturesDetected = 0;
    UINT nNumWrongGesturesDetected = 0;
    INT nNumPositiveExamples = 0;
    INT nNumTrueDetections = 0;
    INT nNumFalseDetections = 0;
    // We need to make sure the indices match up, so add two in the beginning and two at the end
    vector<BOOL> fFilteredClassificationResults;
    fFilteredClassificationResults.push_back( FALSE );
    fFilteredClassificationResults.push_back( FALSE );

    const UINT nNumExamples = (UINT)( m_LabeledExamples.m_pExamples.size() );
    for ( UINT i = 2; i < nNumExamples - 2; i++ )
    {
        // Get the example from the ground truth training set
        NUI_SKELETON_DATA* pSkeletonData = m_LabeledExamples.m_pExamples[ i ];
        UINT64 uTimeStamp = m_LabeledExamples.m_uTimeStamps[ i ];

        // Update the classifier data. Time stamps from Xed files are different to timestamps from runtime NUI_SKELETON_FRAMES
        LARGE_INTEGER liTimeStamp;
        liTimeStamp.QuadPart = uTimeStamp;
        Update( 0, pSkeletonData, liTimeStamp );

        // Run the strong classifier on player 0
        Results results;
        if ( m_StrongClassifier.Detect( 0, m_ClassifierData, &results ) )
        {
            // Testing for per gesture results
            if ( results.m_bFirstFrameDetected )
            {
                // We cannot just detect if the first frame of the gesture correlate to the a
                // ground truth labeled example, since the detection might be a few frams off
                // but still detected the gesture correctly. We therefore allow for 2 frames
                // to etiher side when testing the detection results against the ground truth
                if ( m_LabeledExamples.m_iLabels[ i ] == g_iClassificationLabelCorrect ||
                     m_LabeledExamples.m_iLabels[ i - 1 ] == g_iClassificationLabelCorrect ||
                     m_LabeledExamples.m_iLabels[ i + 1 ] == g_iClassificationLabelCorrect ||
                     m_LabeledExamples.m_iLabels[ i - 2 ] == g_iClassificationLabelCorrect ||
                     m_LabeledExamples.m_iLabels[ i + 2 ] == g_iClassificationLabelCorrect )
                {
                    if ( ( m_LabeledExamples.m_iLabels[ i - 1 ] == g_iClassificationLabelCorrect && m_LabeledExamples.m_iLabels[ i - 2 ] != g_iClassificationLabelCorrect ) ||
                         ( m_LabeledExamples.m_iLabels[ i ] == g_iClassificationLabelCorrect && m_LabeledExamples.m_iLabels[ i - 1 ] != g_iClassificationLabelCorrect ) ||
                         ( m_LabeledExamples.m_iLabels[ i + 1] == g_iClassificationLabelCorrect && m_LabeledExamples.m_iLabels[ i ] != g_iClassificationLabelCorrect ) ||
                         ( m_LabeledExamples.m_iLabels[ i + 2 ] == g_iClassificationLabelCorrect && m_LabeledExamples.m_iLabels[ i + 1 ] != g_iClassificationLabelCorrect ) )
                    {
                        nNumGesturesDetected++;
                    }
                    else
                    {
                        nNumWrongGesturesDetected++;
                    }
                }
                else
                {
                    nNumWrongGesturesDetected++;
                }
            }
        }

        fFilteredClassificationResults.push_back( results.m_bDetected );

        // Testing for per frame results
        if ( m_StrongClassifier.Detect( 0, m_ClassifierData, &results, FALSE ) )
        {
            if ( m_LabeledExamples.m_iLabels[ i ] == g_iClassificationLabelCorrect )
            {
                nNumTrueDetections++;
            }
            else
            {
                nNumFalseDetections++;
            }
        }

        if ( m_LabeledExamples.m_iLabels[ i ] == g_iClassificationLabelCorrect )
        {
            nNumPositiveExamples++;
        }
    }

    fFilteredClassificationResults.push_back( FALSE );
    fFilteredClassificationResults.push_back( FALSE );

    INT nNumTruePositiveGesturesGT = 0;
    INT nNumTruePositiveGesturesObserved = 0;
    INT nNumFalsePositiveGesturesObserved = 0;
        
    for ( UINT i = 1; i < nNumExamples; i++ )
    {      
        // Find a true positive gesture in GT. Count only the start of each sequence
        // of GT frames that make up the gesture
        if ( m_LabeledExamples.m_iLabels[ i ] == g_iClassificationLabelCorrect &&
             m_LabeledExamples.m_iLabels[ i - 1 ] != g_iClassificationLabelCorrect )
        {
            nNumTruePositiveGesturesGT++;
        }
        else
        {
            continue;
        }

        // Find a true positive gesture in the observed data during the GT frames.
        for ( UINT j = i; j < nNumExamples; j++)
        {
            if ( fFilteredClassificationResults[ j ] )
            {
                nNumTruePositiveGesturesObserved++;
                break;
            }

            // Check for the end of the gesture in GT
            if ( m_LabeledExamples.m_iLabels[ j ] != g_iClassificationLabelCorrect &&
                 m_LabeledExamples.m_iLabels[ j - 1 ] == g_iClassificationLabelCorrect )
            {
                break;
            }
        }

        // Find all true positive gesture in the observed data during the GT frames.
        INT nNumObserved = 0;
        for ( UINT j = i; j < nNumExamples; j++ )
        {
            if ( fFilteredClassificationResults[ j ] &&
                 !fFilteredClassificationResults[ j - 1 ] )
            {
                nNumObserved++;
            }

            // Check for the end of the gesture in GT
            if ( m_LabeledExamples.m_iLabels[ j ] != g_iClassificationLabelCorrect &&
                 m_LabeledExamples.m_iLabels[ j - 1 ] == g_iClassificationLabelCorrect )
            {
                break;
            }
        }

        nNumFalsePositiveGesturesObserved += max( 0, nNumObserved - 1 );    // we only allow 1 detection during GT gesture
    }

    // Now we try to find false positives from observed data with no GT
    for ( UINT i = 1; i < nNumExamples; i++ )
    {
        // Find an observed detection
        if ( fFilteredClassificationResults[ i ] &&
             !fFilteredClassificationResults[i - 1] )
        {
        }
        else
        {
            continue;
        }

        // Find a true positive gesture in the GT data during the observed frames.
        INT numGT = 0;
        for ( UINT j = i; j < nNumExamples; j++ )
        {
            if ( m_LabeledExamples.m_iLabels[ j ] == g_iClassificationLabelCorrect )
            {
                numGT++;
                break;
            }

            // Check for the end of the gesture in observed data
            if ( !fFilteredClassificationResults[ j ] &&
                 fFilteredClassificationResults[ j - 1 ] )
            {
                break;
            }
        }

        nNumFalsePositiveGesturesObserved += max( 0, 1 - numGT );
    }

    FLOAT fTruePositives = 0.0f;
    FLOAT fFalsePositives = 0.0f;

    if ( nNumTruePositiveGesturesGT < 1 &&
         nNumTruePositiveGesturesObserved < 1 )
    {   // We have no GT or observed gestures, so no error
        fTruePositives = 100.0f;
    }
    else if ( nNumTruePositiveGesturesGT < 1 &&
              nNumTruePositiveGesturesObserved >= 1 )
    {   // We have no GT gestures, but did find observed gestures, so report no error
        // since we're calculating false negatives here, not false positives
        fTruePositives = 100.0f;
    }
    else
    {
        fTruePositives = 100.0f * nNumTruePositiveGesturesObserved / (FLOAT)nNumTruePositiveGesturesGT;
    }

    if ( nNumTruePositiveGesturesGT < 1 &&
         nNumFalsePositiveGesturesObserved < 1 )
    {   // We have no GT or observed gestures, so no error
        fFalsePositives = 0.0f;
    }
    else if ( nNumTruePositiveGesturesGT < 1 &&
              nNumFalsePositiveGesturesObserved >= 1 )
    {   // We have no GT gestures, but did find observed gestures, so return highest error
        fFalsePositives = 100.0f;
    }
    else
    {
        fFalsePositives = 100.0f * nNumFalsePositiveGesturesObserved / (FLOAT)nNumTruePositiveGesturesGT;
    }

    // Output accuracy in true positives and false positives for per frame results
    FLOAT fAccuracy;
    if ( nNumPositiveExamples == 0 )
    {
        fAccuracy = 100.0f;
    }
    else
    {
        fAccuracy = nNumTrueDetections * 100.0f / nNumPositiveExamples;
    }
    FLOAT fErrorFalsePositives = (FLOAT)nNumFalseDetections * 100.0f / ( nNumExamples - 4 - nNumPositiveExamples );
    printf( "\n    Raw Per Frame Results:" );
    printf( "\n\t%% Accuracy True Positives: %f %% (%d/%d)", fAccuracy, nNumTrueDetections, nNumPositiveExamples );
    printf( "\n\t%% Error False Positives: %f %% (%d/%d)", fErrorFalsePositives, nNumFalseDetections, nNumExamples - 4 - nNumPositiveExamples );

    // Output accuracy in true positives and false positives for filtered per gesture results
    printf( "\n    Filtered Per Gesture Results:" );
    printf( "\n\t%% Accuracy True Positives: %f %% (%d/%d)", fTruePositives, nNumTruePositiveGesturesObserved, nNumTruePositiveGesturesGT );
    printf( "\n\t%% Error False Positives: %f %% (%d/%d)", fFalsePositives, nNumFalsePositiveGesturesObserved, nNumTruePositiveGesturesGT );


    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Test
// Desc: Test the accuracy of the training algorithm. Used to find the best filtering
//       parameters
//--------------------------------------------------------------------------------------

VOID GestureDetectorTrainer::Test( FLOAT* pTruePositives, FLOAT* pFalsePositives, vector<FLOAT>& fRawClassificationResults )
{
    // Clear all the data
    ClassifierData::Initialize();
    m_StrongClassifier.Reset( 0 );
    m_uPreviousTimeStamp = 0;

    // run strong classifier on training data for verification
    INT nNumTruePositiveGesturesGT = 0;
    INT nNumTruePositiveGesturesObserved = 0;
    INT nNumFalsePositiveGesturesObserved = 0;

    const UINT nNumExamples = (UINT)( m_LabeledExamples.m_pExamples.size() );

    // Get all filtered data
    vector<BOOL> fFilteredClassificationResults;
    for ( UINT i = 0; i < nNumExamples; i++ )
    {
        // Filter the cached raw detection results with the current detection thresholds
        Results results;
        m_StrongClassifier.FilterDetectionResults( 0, fRawClassificationResults[ i ], &results );
        fFilteredClassificationResults.push_back( results.m_bDetected );
    }
        
    for ( UINT i = 1; i < nNumExamples; i++ )
    {      
        // Find a true positive gesture in GT. Count only the start of each sequence
        // of GT frames that make up the gesture
        if ( m_LabeledExamples.m_iLabels[ i ] == g_iClassificationLabelCorrect &&
             m_LabeledExamples.m_iLabels[ i - 1 ] != g_iClassificationLabelCorrect )
        {
            nNumTruePositiveGesturesGT++;
        }
        else
        {
            continue;
        }

        // Find a true positive gesture in the observed data during the GT frames.
        for ( UINT j = i; j < nNumExamples; j++)
        {
            if ( fFilteredClassificationResults[ j ] )
            {
                nNumTruePositiveGesturesObserved++;
                break;
            }

            // Check for the end of the gesture in GT
            if ( m_LabeledExamples.m_iLabels[ j ] != g_iClassificationLabelCorrect &&
                 m_LabeledExamples.m_iLabels[ j - 1 ] == g_iClassificationLabelCorrect )
            {
                break;
            }
        }

        // Find all true positive gesture in the observed data during the GT frames.
        INT nNumObserved = 0;
        for ( UINT j = i; j < nNumExamples; j++ )
        {
            if ( fFilteredClassificationResults[ j ] &&
                 !fFilteredClassificationResults[ j - 1 ] )
            {
                nNumObserved++;
            }

            // Check for the end of the gesture in GT
            if ( m_LabeledExamples.m_iLabels[ j ] != g_iClassificationLabelCorrect &&
                 m_LabeledExamples.m_iLabels[ j - 1 ] == g_iClassificationLabelCorrect )
            {
                break;
            }
        }

        nNumFalsePositiveGesturesObserved += max( 0, nNumObserved - 1 );    // we only allow 1 detection during GT gesture
    }

    // Now we try to find false positives from observed data with no GT
    for ( UINT i = 1; i < nNumExamples; i++ )
    {
        // Find an observed detection
        if ( fFilteredClassificationResults[ i ] &&
             !fFilteredClassificationResults[i - 1] )
        {
        }
        else
        {
            continue;
        }

        // Find a true positive gesture in the GT data during the observed frames.
        INT numGT = 0;
        for ( UINT j = i; j < nNumExamples; j++ )
        {
            if ( m_LabeledExamples.m_iLabels[ j ] == g_iClassificationLabelCorrect )
            {
                numGT++;
                break;
            }

            // Check for the end of the gesture in observed data
            if ( !fFilteredClassificationResults[ j ] &&
                 fFilteredClassificationResults[ j - 1 ] )
            {
                break;
            }
        }

        nNumFalsePositiveGesturesObserved += max( 0, 1 - numGT );
    }


    if ( nNumTruePositiveGesturesGT < 1 &&
         nNumTruePositiveGesturesObserved < 1 )
    {   // We have no GT or observed gestures, so no error
        *pTruePositives = 100.0f;
    }
    else if ( nNumTruePositiveGesturesGT < 1 &&
              nNumTruePositiveGesturesObserved >= 1 )
    {   // We have no GT gestures, but did find observed gestures, so report no error
        // since we're calculating false negatives here, not false positives
        *pTruePositives = 100.0f;
    }
    else
    {
        *pTruePositives = 100.0f * nNumTruePositiveGesturesObserved / (FLOAT)nNumTruePositiveGesturesGT;
    }

    if ( nNumTruePositiveGesturesGT < 1 &&
         nNumFalsePositiveGesturesObserved < 1 )
    {   // We have no GT or observed gestures, so no error
        *pFalsePositives = 0.0f;
    }
    else if ( nNumTruePositiveGesturesGT < 1 &&
              nNumFalsePositiveGesturesObserved >= 1 )
    {   // We have no GT gestures, but did find observed gestures, so return highest error
        *pFalsePositives = 100.0f;
    }
    else
    {
        *pFalsePositives = 100.0f * nNumFalsePositiveGesturesObserved / (FLOAT)nNumTruePositiveGesturesGT;
    }
}


//--------------------------------------------------------------------------------------
// Private structure used to find the most optimimum filter parameters for per gesture
// detection.
//--------------------------------------------------------------------------------------

struct DetectionParameters
{
    FLOAT   m_fThreshold;
    UINT    m_nNumFramesToFilter;
    FLOAT   m_fError;
    INT     x;
    INT     y;

    inline DetectionParameters& operator = ( const DetectionParameters& rhs)
    {
        m_fThreshold = rhs.m_fThreshold;
        m_nNumFramesToFilter = rhs.m_nNumFramesToFilter;
        m_fError = rhs.m_fError;
        x = rhs.x;
        y = rhs.y;
        return *this;
    }

    inline bool operator < ( const DetectionParameters& rhs )
    {
        return ( m_fError < rhs.m_fError );
    }
};


//--------------------------------------------------------------------------------------
// Name: CalcSumOfNeighbours
// Desc: Calculates the sum of all values in a 3x3 kernal
//--------------------------------------------------------------------------------------

FLOAT CalcSumOfNeighbours( const INT x, const INT y, vector<vector<DetectionParameters>>& fValues )
{
    FLOAT fSum = 0.0f;

    INT iStartY = max( 0, y - 1 );
    INT iStopY  = min( (INT)fValues.size() - 2, y + 1 );
    INT iStartX = max( 0, x - 1 );
    INT iStopX  = min( (INT)fValues[ 0 ].size() - 2, x + 1 );

    for ( INT iNeighborY = iStartY; iNeighborY <= iStopY; iNeighborY++ )
    {
        for ( INT iNeighborX = iStartX; iNeighborX <= iStopX; iNeighborX++ )
        {
            fSum += fValues[ iNeighborY ][ iNeighborX ].m_fError;
        }
    }

    return fSum;
}


//--------------------------------------------------------------------------------------
// Name: OptimizeDetectionParameters
// Desc: Since the classifier results are per frame and not per gesture, we need to
//       apply a filter on the raw per frame results. This is in the form of a sum of
//       a sliding window with a threshold. We therefore have two parameters to
//       find that will minimize both the error in true positives and false postives.
//--------------------------------------------------------------------------------------

VOID GestureDetectorTrainer::OptimizeDetectionParameters( const FLOAT fWeightOfFalsePositivesWhenFiltering )
{
    printf( "\n\t-Generate matrix of test results using different detection parameters..." );

    // Cache raw results from classification
    vector<FLOAT> fRawClassificationResults;

    // Clear all the data
    ClassifierData::Initialize();
    m_StrongClassifier.Reset( 0 );
    m_uPreviousTimeStamp = 0;

    // Run strong classifier
    const UINT nNumExamples = (UINT)( m_LabeledExamples.m_pExamples.size() );
    for ( UINT i = 0; i < nNumExamples; i++ )
    {
        // Get the example from the ground truth training set
        NUI_SKELETON_DATA* pSkeletonData = m_LabeledExamples.m_pExamples[ i ];
        UINT64 uTimeStamp = m_LabeledExamples.m_uTimeStamps[ i ];

        // Update the classifier data. Time stamps from Xed files are different to timestamps from runtime NUI_SKELETON_FRAMES
        LARGE_INTEGER liTimeStamp;
        liTimeStamp.QuadPart = uTimeStamp;
        Update( 0, pSkeletonData, liTimeStamp );

        // Run the strong classifier on player 0, without any filtering
        Results results;
        m_StrongClassifier.Detect( 0, m_ClassifierData, &results, FALSE );

        // Store the raw classification result
        fRawClassificationResults.push_back( results.m_fConfidence );
    }

    vector<vector<DetectionParameters>> values;
    vector<vector<DetectionParameters>> summedValues;

    // Fill in a 2d matrix of possible parameters
    UINT i = 0;
    for ( FLOAT fThreshold = fDetectionParamsMinThreshold; fThreshold <= fDetectionParamsMaxThreshold; fThreshold += fDetectionParamsThresholdStep )
    {
        values.resize( i + 1 );

        for ( UINT nNumFrames = nDetectionParamsMinNumFrames; nNumFrames <= nDetectionParamsMaxNumFrames; nNumFrames++ )        
        {
            DetectionParameters parameters;
            parameters.m_fThreshold = fThreshold;
            parameters.m_nNumFramesToFilter = nNumFrames;
            parameters.x = nNumFrames - nDetectionParamsMinNumFrames;
            parameters.y = i;

            SetDetectionThreshold( fThreshold );
            SetNumFramesToFilter( nNumFrames );

            FLOAT fTruePositiveResults;
            FLOAT fFalsePositiveResults;
            Test( &fTruePositiveResults, &fFalsePositiveResults, fRawClassificationResults );

            // Combine the results of false negatives and false positives with a weighted sum. We can bias towards optimizing
            // for fewer false positives or fewer false negatives.
            parameters.m_fError = ( fabsf( 100.0f - fTruePositiveResults ) * ( 1.0f - fWeightOfFalsePositivesWhenFiltering ) ) +
                                  ( fFalsePositiveResults * fWeightOfFalsePositivesWhenFiltering );

            values[ i ].push_back( parameters );
        }
        i++;
    }

    INT nNumY = (INT)values.size();
    INT nNumX = (INT)values[ 0 ].size();

    // Fill it with the sum of the 8 surrounding neighbours and itself for each value in the 2d array
    summedValues.resize( nNumY );
    for ( INT y = 0; y < nNumY; y++ )
    {
        for ( INT x = 0; x < nNumX; x++ )
        {
            DetectionParameters params = values[ y ][ x ];

            // Just use FLT_MAX around the edge pixels, since these will be invalid anyway
            if ( y == 0 || y == ( nNumY - 1 ) ||
                 x == 0 || x == ( nNumX - 1 ) )
            {
                params.m_fError = FLT_MAX / 9.0f;
            }
            else
            {
                params.m_fError = CalcSumOfNeighbours( x, y, values );
            }
            summedValues[ y ].push_back( params );
        }
    }

    nNumY = (INT)summedValues.size();
    nNumX = (INT)summedValues[ 0 ].size();
    DetectionParameters bestParams = { 0, 0, FLT_MAX };
    vector<DetectionParameters> medianXFilterMinValues;
    vector<DetectionParameters> medianYFilterMinValues;

    // Find the minimum summed error values
    for ( INT y = 0; y < nNumY; y++ )
    {
        for ( INT x = 0; x < nNumX; x++ )
        {
            if ( summedValues[ y ][ x ] < bestParams )
            {
                bestParams = summedValues[ y ][ x ];
            }
        }
    }

    // Find all the values equal to this minimum value and add them to an array for finding the median value
    // Traverse horizontally
    for ( INT y = 0; y < nNumY; y++ )
    {
        for ( INT x = 0; x < nNumX; x++ )
        {
            if ( fabsf( summedValues[ y ][ x ].m_fError - bestParams.m_fError ) < FLT_EPSILON )
            {
                medianXFilterMinValues.push_back( summedValues[ y ][ x ] );
            }
        }
    }

    // Find all the values equal to this minimum value and add them to an array for finding the median value
    // Traverse vertically
    for ( INT x = 0; x < nNumX; x++ )
    {
        for ( INT y = 0; y < nNumY; y++ )
        {
            if ( fabsf( summedValues[ y ][ x ].m_fError - bestParams.m_fError ) < FLT_EPSILON )
            {
                medianYFilterMinValues.push_back( summedValues[ y ][ x ] );
            }
        }
    }

    // Find the median value in each array
    sort( medianXFilterMinValues.begin(), medianXFilterMinValues.end() );
    sort( medianYFilterMinValues.begin(), medianYFilterMinValues.end() );

    INT iMedian = (INT)(medianXFilterMinValues.size() - 1 ) / 2;
    DetectionParameters medianX = medianXFilterMinValues[ iMedian ];
    DetectionParameters medianY = medianYFilterMinValues[ iMedian ];

    // Find out which one is best
    FLOAT fSmallestXError = FLT_MAX;
    for ( INT i = max( iMedian - 2, 0 ); i < min( iMedian + 2, (INT)medianXFilterMinValues.size() ); i++ )
    {
        FLOAT fSum = CalcSumOfNeighbours( medianXFilterMinValues[ i ].x, medianXFilterMinValues[ i ].y, summedValues );
        if ( fSum < fSmallestXError )
        {
            fSmallestXError = fSum;
            medianX = medianXFilterMinValues[ i ];
        }
    }

    FLOAT fSmallestYError = FLT_MAX;
    for ( INT i = max( iMedian - 2, 0 ); i < min( iMedian + 2, (INT)medianYFilterMinValues.size() ); i++ )
    {
        FLOAT fSum = CalcSumOfNeighbours( medianYFilterMinValues[ i ].x, medianYFilterMinValues[ i ].y, summedValues );
        if ( fSum < fSmallestYError )
        {
            fSmallestYError = fSum;
            medianY = medianYFilterMinValues[ i ];
        }
    }

    // The best one is the one with the smallest summed error value
    bestParams = ( fSmallestXError > fSmallestYError ) ? medianY : medianX;

    SetDetectionThreshold( bestParams.m_fThreshold );
    SetNumFramesToFilter( bestParams.m_nNumFramesToFilter );
}


// XStdudio and XED APIs only run in 32bit, so we need to exclude the API calls from the 64bit trainer application
#ifndef WIN64

//--------------------------------------------------------------------------------------
// Name: GenerateLabeledExamples
// Desc: Used tagged xed files to generate labeled training/testing examples as ground truth
//--------------------------------------------------------------------------------------

HRESULT GestureDetectorTrainer::GenerateLabeledExamples( const CHAR* szGestureName,                                                                
                                                         vector<wstring>& szXedFiles,
                                                         const CHAR* szDestPath,
                                                         const BOOL bUseRawSkeletonData )
{
    // We need to load the XedFile.dll from the XEDK path
    WCHAR szPath[ _MAX_PATH ];
    GetEnvironmentVariable( L"XEDK", szPath, _MAX_PATH );
    wcscat_s( szPath, L"\\bin\\win32" );
    SetDllDirectory( szPath );

    Reset();

    size_t uNumConverted;
    UINT nNumXedFiles = (UINT)szXedFiles.size();
    for ( UINT i = 0; i < nNumXedFiles; i++ )
    {
        CHAR szXedFileName[ MAX_PATH ];
        wcstombs_s( &uNumConverted, szXedFileName, (size_t)MAX_PATH, szXedFiles[ i ].c_str(), (size_t)MAX_PATH );

        // We duplicate each file and mirror the skeleton data so that we can use left gestures for training of right gestures,
        // e.g. if I have 9/10 righ handed players in my training set it would mean that I probably have much more data for
        // a PUNCH_RIGHT than I have for a PUNCH_LEFT, since right handed players would be biased towards doing a right handed punch.
        // We can now mirror all the PUNCH_RIGHT gestures to PUNCH_LEFT gestures and have equal number of training examples for
        // left and right punches.
        if ( bUseRawSkeletonData)
        {
            RETURN_ON_FAIL( GenerateLabeledExamplesFromRawSkeleton( szGestureName, szXedFileName, FALSE ) );
            RETURN_ON_FAIL( GenerateLabeledExamplesFromRawSkeleton( szGestureName, szXedFileName, TRUE ) );
        }
        else
        {
            RETURN_ON_FAIL( GenerateLabeledExamplesFromFilteredSkeleton( szGestureName, szXedFileName, FALSE ) );
            RETURN_ON_FAIL( GenerateLabeledExamplesFromFilteredSkeleton( szGestureName, szXedFileName, TRUE ) );
        }
    }

    RETURN_ON_FAIL( SaveLabeledExamples( szDestPath ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GenerateLabeledExamplesFromRawSkeleton
// Desc: Generate labeled examples using raw skeleton data in the xed files
//--------------------------------------------------------------------------------------

HRESULT GestureDetectorTrainer::GenerateLabeledExamplesFromRawSkeleton( const CHAR* szGestureName,
                                                                        const CHAR* szXedFileName,
                                                                        const BOOL bMirrorData )
{
    // The XedFileTagger has to generate unique time stamps for the tags in the title data stream. These are
    // normally one microsecond after the depth event time stamp, but we allow for 10 microseconds to be 
    // searched when we have a title event and look for a corrosponding depth event
    const UINT uMicrosecondsBetweenTagTitleDataAndDepthData = 10;

    // Open the xed file
    XED_CONTEXT* pContext = NULL;
    HRESULT hr = XedOpenFile( szXedFileName, 0, &pContext );
    if ( FAILED( hr ) )
    {
        printf( "\n\nError: XedOpenFile( %s, 0, &pContext ) failed.\n\n" );
        return E_FAIL;
    }

    // Get the number of depth, skeleton and title data events
    const UINT nNumDepthEvents = XedGetEventCount( pContext, XSTUDIO_STREAM_ID_NUICAM_DEPTH );
    const UINT nNumSkeletonEvents = XedGetEventCount( pContext, XSTUDIO_STREAM_ID_NUIAPI_SKELETON );

    // We must have some depth and skeleton data to label the data. We could treat this just as a warning
    // but we really want to have valid data, so it's better to treat it as an error and fail
    if ( !nNumDepthEvents ||
         !nNumSkeletonEvents )
    {        
        hr = XedCloseFile( &pContext );
        printf( "\n\nError: No skeleton or depth frame events in xed file.\n\n" );
        return E_FAIL;
    }

    // We potentially open multiple files from multiple folders and we want to add all the labeled examples into one list
    const UINT uOffsetIndex = (UINT)m_LabeledExamples.m_pExamples.size();

    NUI_SKELETON_FRAME skeletonFrame;
    UINT uFrameNumber;
    UINT64 uSkeletonTimeStamp;
    UINT64 uTitleDataTimeStamp;
    UINT64 uDepthTimeStamp;
    UINT uRequiredTitleDataBufferSize;
    CHAR szTitleData[ m_uTitleDataBufferSize ];
    CHAR szPreviousTitleData[ m_uTitleDataBufferSize ] = "";
    CHAR szGesture[ 256 ];
    BOOL bFoundGesture = FALSE;
    BOOL bFoundTrainingGesture = FALSE;
    INT iPreviousTrackedSkeleton = -1;

    if ( bMirrorData )
    {
        printf( "\n  %s\n\t-Duplicating and mirroring skeleton data...", szXedFileName );
    }
    else
    {
        printf( "\n  %s\n\t-Reading skeleton frames from xed file and labeling...", szXedFileName );
    }

    // Tags in the XedFileTagger are added to each accosiated depth event, so drive everything from depth events
    for ( UINT i = 0; i < nNumDepthEvents; i++ )
    {      
        // Read the depth event frame number and time stamp
        hr = XedReadNuiDepthFrame( pContext, i, &uFrameNumber, &uDepthTimeStamp, NULL, 0, FALSE );
        if ( FAILED( hr ) )
        {
            continue;
        }

        // Read the skeleton frame at the same frame as the depth event
        UINT uSkeletonEventIndex = XedGetNuiSkeletonEventIndexFromFrameNumber( pContext, uFrameNumber );
        hr = XedReadNuiSkeletonFrame( pContext, uSkeletonEventIndex, &uFrameNumber, &uSkeletonTimeStamp, &skeletonFrame, sizeof( NUI_SKELETON_FRAME ) );
        if ( FAILED( hr ) )
        {
            continue;
        }

        // Find the closest tracked skeleton
        INT iClosestSkeletonIdx = -1;
        FLOAT fMinDistance = FLT_MAX;

        // Find the closest skeleton to the center of the playspace
        for ( UINT j = 0; j < NUI_SKELETON_COUNT; j++ )
        {
            NUI_SKELETON_DATA* pSkeletonData = &skeletonFrame.SkeletonData[ j ];

            // If not tracked, then ignore
            if ( pSkeletonData->eTrackingState != NUI_SKELETON_TRACKED )
            {
                continue;
            }

            // Find the positions closest to the camera.
            XMVECTOR vDistance = XMVector3LengthSq( pSkeletonData->Position );
            FLOAT fDistance = XMVectorGetX( vDistance );
            if ( fDistance < fMinDistance )
            {
                iClosestSkeletonIdx = j;
                fMinDistance = fDistance;
            }       
        }

        BOOL bReset = FALSE;
        if ( iClosestSkeletonIdx != iPreviousTrackedSkeleton )
        {
            bReset = TRUE;
        }
        iPreviousTrackedSkeleton = iClosestSkeletonIdx;

        // If we didn't find a tracked skeleton, continue to next skeleton event
        if ( iClosestSkeletonIdx == -1 )
        {
            continue;
        }

        // Apply tilt correction on the data
        ApplyTiltCorrection( 0, &skeletonFrame.SkeletonData[ iClosestSkeletonIdx ] , &skeletonFrame.SkeletonData[ iClosestSkeletonIdx ], &skeletonFrame.vNormalToGravity, bReset );

        // We allow to mirror data, to get equal amount of data for left/right gestures
        if ( bMirrorData )
        {
            MirrorSkeletonData(  &skeletonFrame.SkeletonData[ iClosestSkeletonIdx ] );
        }

        // Add the skeleton data as a training example with the NUI_SKELETON_FRAME timestamp
        NUI_SKELETON_DATA* pSkeletonData = (NUI_SKELETON_DATA*)_aligned_malloc( sizeof( NUI_SKELETON_DATA ), 16 );
        memcpy( pSkeletonData, &skeletonFrame.SkeletonData[ iClosestSkeletonIdx ], sizeof( NUI_SKELETON_DATA ) );
        m_LabeledExamples.m_pExamples.push_back( pSkeletonData );
        m_LabeledExamples.m_iLabels.push_back( g_iClassificationLabelIncorrect );
        m_LabeledExamples.m_uTimeStamps.push_back( uSkeletonTimeStamp );

        // Now see if this is a tagged frame. This will return the title event prior to the asked timestamp, which if there is
        // a tag at this depth timestamp, will be the time stamp within about 10 microseconds from the depth event time stamp
        UINT uTitleDataEventIndex = XedGetTitleDataEventIndexFromMicroseconds( pContext, uDepthTimeStamp + uMicrosecondsBetweenTagTitleDataAndDepthData );

        // check that events are valid
        if ( uTitleDataEventIndex == XED_EVENTINDEX_INVALID )
        {
            continue;
        }

        // Read the time stamp for the title data event
        hr = XedReadTitleData( pContext, uTitleDataEventIndex, &uTitleDataTimeStamp, NULL, NULL, 0, &uRequiredTitleDataBufferSize );
        if ( FAILED( hr ) )
        {
            continue;
        }

        // if title data is inbetween depth event and skeleton event, then this is a possible tagged frame
        if  ( ( uTitleDataTimeStamp - uDepthTimeStamp ) < uMicrosecondsBetweenTagTitleDataAndDepthData )
        {
            // Read the title data
            hr = XedReadTitleData( pContext, uTitleDataEventIndex, &uTitleDataTimeStamp, NULL, szTitleData, sizeof( szTitleData ), &uRequiredTitleDataBufferSize );
            if ( SUCCEEDED( hr ) )
            {
                // Check if we have a tag. Tags from the XedFileTagger will add tags as strings starting with "TAG:"
                if ( !strncmp( szTitleData, m_szTagID, sizeof( m_szTagID ) ) )
                {
                    // Count the number of gestures for testing purposes
                    if ( strcmp( szTitleData, szPreviousTitleData ) )       // previous tag is different from current
                    {
                        bFoundGesture = FALSE;
                    }
                    strcpy_s( szPreviousTitleData, sizeof( szPreviousTitleData ), szTitleData );

                    // We now found a new gesture
                    if ( !bFoundGesture )
                    {
                        m_nTotalNumGestures++;
                        bFoundGesture = TRUE;
                        bFoundTrainingGesture = FALSE;
                    }

                    // if we're mirroring data, we need to mirror the tags too, i.e. left->right, right->left
                    strcpy_s( szGesture, szTitleData );
                    if ( bMirrorData )
                    {
                        CHAR* pSubString = strstr( szGesture, "_Right" );
                        if ( pSubString )
                        {
                            int size = sizeof( szGesture )  - ( strlen( szGesture ) - strlen( pSubString ) );
                            strncpy_s( pSubString, size, "_Left", strlen( "_Left" ) );
                        }
                        else
                        {
                            pSubString = strstr( szGesture, "_Left" );
                            if ( pSubString )
                            {
                                int size = sizeof( szGesture )  - ( strlen( szGesture ) - strlen( pSubString ) );
                                strncpy_s( pSubString, size, "_Right", strlen( "_Right" ) );
                            }
                        }
                    }

                    // Check if the tag in the title data matches the gesture tag we're training for
                    if ( !strcmp( &szGesture[ sizeof( m_szTagID ) ], szGestureName ) )
                    {
                        const UINT nNumLabels = (UINT)m_LabeledExamples.m_iLabels.size() - uOffsetIndex;
                        const UINT uIndex = (UINT)m_LabeledExamples.m_iLabels.size() - 1;

                        // Some recordings have the player tracked only after they started doing a gesture.
                        // For these recordings we have to ignore that first gesture. This is easy, we simply
                        // check that the first tracked frame is not a labeled frame
                        if ( nNumLabels == 1 )
                        {
                            // Remove the added example                            
                            _aligned_free( m_LabeledExamples.m_pExamples[ uIndex ] );
                            m_LabeledExamples.m_pExamples.pop_back();
                            m_LabeledExamples.m_iLabels.pop_back();
                            m_LabeledExamples.m_uTimeStamps.pop_back();
                        }
                        else
                        {
                            m_LabeledExamples.m_iLabels[ uIndex ] = g_iClassificationLabelCorrect;
                            if ( !bFoundTrainingGesture )
                            {
                                m_nNumTrainingGestures++;
                                bFoundTrainingGesture = TRUE;
                            }
                        }
                    }
                }
                else
                {
                    bFoundGesture = FALSE;
                }
            }
        }
        else
        {
            bFoundGesture = FALSE;
        }
    }

    printf( "Done" );

    // Close the xed file
    hr = XedCloseFile( &pContext );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GenerateLabeledExamplesFromFilteredSkeleton
// Desc: Generate labeled examples using skeleton data that was recorded as title data
//       with tilt correction and filtering already applied. This is useful if you want
//       to train on the exact same data that you will have at runtime but don't want
//       to port your filtering code to the PC. Refer to the source code of the KickBoxing
//       sample in the XDK on how the XStudio APIs can be used to do this.
//--------------------------------------------------------------------------------------

HRESULT GestureDetectorTrainer::GenerateLabeledExamplesFromFilteredSkeleton( const CHAR* szGestureName,
                                                                             const CHAR* szXedFileName,
                                                                             const BOOL bMirrorData )
{
    // The XedFileTagger has to generate unique time stamps for the tags in the title data stream. These are
    // normally one microsecond after the depth event time stamp, but we allow for 10 microseconds to be 
    // searched when we have a title event and look for a corrosponding depth event
    const UINT uMicrosecondsBetweenTagTitleDataAndDepthData = 10;

    // The XedFileTagger adds tags accosiated with depth events, but we could still drop frames when writing 
    // the data to the xed file. Therefore we compensate for this by looking 33ms beyond the depth frame time stamp
    const UINT uMicrosecondsBetweenFilteredSkeletonTitleDataAndDepthData = 33000;

    // Open the xed file
    XED_CONTEXT* pContext = NULL;
    HRESULT hr = XedOpenFile( szXedFileName, 0, &pContext );
    if ( FAILED( hr ) )
    {
        printf( "\n\nError: XedOpenFile( %s, 0, &pContext ) failed.\n\n" );
        return E_FAIL;
    }

    // Get the number of depth, skeleton and title data events
    const UINT nNumDepthEvents = XedGetEventCount( pContext, XSTUDIO_STREAM_ID_NUICAM_DEPTH );
    const UINT nNumSkeletonEvents = XedGetEventCount( pContext, XSTUDIO_STREAM_ID_NUIAPI_SKELETON );

    // We must have some depth and skeleton data to label the data. We could treat this just as a warning
    // but we really want to have valid data, so it's better to treat it as an error and fail
    if ( !nNumDepthEvents ||
         !nNumSkeletonEvents )
    {        
        hr = XedCloseFile( &pContext );
        printf( "\n\nError: No skeleton or depth frame events in xed file.\n\n" );
        return E_FAIL;
    }

    // We potentially open multiple files from multiple folders and we want to add all the labeled examples into one list
    const UINT uOffsetIndex = (UINT)m_LabeledExamples.m_pExamples.size();

    // Read the skeleton frames from the xed file
    UINT uFrameNumber;
    UINT64 uTitleDataTimeStamp;
    UINT64 uDepthTimeStamp;
    UINT uRequiredTitleDataBufferSize;
    NUI_SKELETON_DATA filteredSkeletonData;
    CHAR szTitleData[ m_uTitleDataBufferSize ];
    CHAR szPreviousTitleData[ m_uTitleDataBufferSize ] = "";
    CHAR szGesture[ 256 ];
    BOOL bFoundGesture = FALSE;
    BOOL bFoundTrainingGesture = FALSE;
    UINT64 uPreviousDepthTimeStamp = 0;

    if ( bMirrorData )
    {
        printf( "\n  %s\n\t-Duplicating and mirroring skeleton data...", szXedFileName );
    }
    else
    {
        printf( "\n  %s\n\t-Reading skeleton frames from xed file and labeling...", szXedFileName );
    }

    // Tags in the XedFileTagger are added to each accosiated depth event, so drive everything from depth events
    for ( UINT i = 0; i < nNumDepthEvents; i++ )
    {      
        // Read the depth frame and time stamp from the depth event
        hr = XedReadNuiDepthFrame( pContext, i, &uFrameNumber, &uDepthTimeStamp, NULL, 0, FALSE );
        if ( FAILED( hr ) )
        {
            continue;
        }

        // Read the filtered skeleton from the title data
        UINT uTitleDataEventIndex = XedGetTitleDataEventIndexFromMicroseconds( pContext, uDepthTimeStamp + uMicrosecondsBetweenFilteredSkeletonTitleDataAndDepthData );

        // check that event is valid
        if ( uTitleDataEventIndex == XED_EVENTINDEX_INVALID )
        {
            continue;
        }

        // if this title data is for a filtered skeleton, then the API will succeed and the uRequiredTitleDataBufferSize will be sizeof( NUI_SKELETON_DATA )
        hr = XedReadTitleData( pContext, uTitleDataEventIndex, &uTitleDataTimeStamp, NULL, &filteredSkeletonData, sizeof( NUI_SKELETON_DATA ), &uRequiredTitleDataBufferSize );
        if ( SUCCEEDED( hr ) )
        {
            // Byteswap the data, since it came from the Xbox 360
            ByteSwapSkeletonData( &filteredSkeletonData );

            // We only want to use tracked skeletons as examples
            if ( filteredSkeletonData.eTrackingState != NUI_SKELETON_TRACKED )
            {
                continue;
            }

            // We allow to mirror data, to get equal training data for left/right gestures
            if ( bMirrorData )
            {
                MirrorSkeletonData( &filteredSkeletonData );
            }

            // Add the skeleton data as a training example and use NUI_SKELETON_FRAME for the timestamp
            NUI_SKELETON_DATA* pSkeletonData = (NUI_SKELETON_DATA*)_aligned_malloc( sizeof( NUI_SKELETON_DATA ), 16 );
            memcpy( pSkeletonData, &filteredSkeletonData, sizeof( NUI_SKELETON_DATA ) );
            m_LabeledExamples.m_pExamples.push_back( pSkeletonData );
            m_LabeledExamples.m_iLabels.push_back( g_iClassificationLabelIncorrect );
            m_LabeledExamples.m_uTimeStamps.push_back( uTitleDataTimeStamp );

            // Now see if this is a tagged frame
            uTitleDataEventIndex = XedGetTitleDataEventIndexFromMicroseconds( pContext, uDepthTimeStamp + uMicrosecondsBetweenTagTitleDataAndDepthData );

            // check that events are valid
            if ( uTitleDataEventIndex == XED_EVENTINDEX_INVALID )
            {
                continue;
            }

            // Read the time stamp for the title data event
            hr = XedReadTitleData( pContext, uTitleDataEventIndex, &uTitleDataTimeStamp, NULL, NULL, 0, &uRequiredTitleDataBufferSize );
            if ( FAILED( hr ) )
            {
                continue;
            }

            // if title data is inbetween depth event and skeleton event, then this is a possible tagged frame
            if  ( ( uTitleDataTimeStamp - uDepthTimeStamp ) < uMicrosecondsBetweenTagTitleDataAndDepthData )
            {
                // Read the title data
                hr = XedReadTitleData( pContext, uTitleDataEventIndex, &uTitleDataTimeStamp, NULL, szTitleData, sizeof( szTitleData ), &uRequiredTitleDataBufferSize );
                if ( SUCCEEDED( hr ) )
                {
                    // Check if we have a tag. Tags from the XedFileTagger will add tags as strings starting with "TAG:"
                    if ( !strncmp( szTitleData, m_szTagID, sizeof( m_szTagID ) ) )
                    {
                        // Count the number of gestures for testing purposes
                        if ( strcmp( szTitleData, szPreviousTitleData ) )   // previous tag is different from current
                        {
                            bFoundGesture = FALSE;
                        }
                        uPreviousDepthTimeStamp = uDepthTimeStamp;
                        strcpy_s( szPreviousTitleData, sizeof( szPreviousTitleData ), szTitleData );

                        if ( !bFoundGesture )
                        {
                            m_nTotalNumGestures++;
                            bFoundGesture = TRUE;
                            bFoundTrainingGesture = FALSE;
                        }

                        // if we're mirroring data, we need to mirror the tags too, i.e. left->right, right->left
                        strcpy_s( szGesture, szTitleData );
                        if ( bMirrorData )
                        {
                            CHAR* pSubString = strstr( szGesture, "_Right" );
                            if ( pSubString )
                            {
                                int size = sizeof( szGesture )  - ( strlen( szGesture ) - strlen( pSubString ) );
                                strncpy_s( pSubString, size, "_Left", strlen( "_Left" ) );
                            }
                            else
                            {
                                pSubString = strstr( szGesture, "_Left" );
                                if ( pSubString )
                                {
                                    int size = sizeof( szGesture )  - ( strlen( szGesture ) - strlen( pSubString ) );
                                    strncpy_s( pSubString, size, "_Right", strlen( "_Right" ) );
                                }
                            }
                        }

                        // Check if the tag in the title data matches the gesture tag we're training for
                        if ( !strcmp( &szGesture[ sizeof( m_szTagID ) ], szGestureName ) )
                        {
                            const UINT nNumLabels = (UINT)m_LabeledExamples.m_iLabels.size() - uOffsetIndex;
                            const UINT uIndex = (UINT)m_LabeledExamples.m_iLabels.size() - 1;

                            // Some recordings have the player tracked only after they started doing a gesture.
                            // For these recordings we have to ignore that first gesture. This is easy, we simply
                            // chech that the first tracked frame is not a labeled frame
                            if ( nNumLabels == 1 )
                            {
                                // Remove the added example
                                _aligned_free( m_LabeledExamples.m_pExamples[ uIndex ] );
                                m_LabeledExamples.m_pExamples.pop_back();
                                m_LabeledExamples.m_iLabels.pop_back();
                                m_LabeledExamples.m_uTimeStamps.pop_back();
                            }
                            else
                            {
                                m_LabeledExamples.m_iLabels[ uIndex ] = g_iClassificationLabelCorrect;
                                if ( !bFoundTrainingGesture )
                                {
                                    m_nNumTrainingGestures++;
                                    bFoundTrainingGesture = TRUE;
                                }
                            }
                        }
                    }
                    else
                    {
                        bFoundGesture = FALSE;
                    }
                }
            }
            else
            {
                bFoundGesture = FALSE;
            }
        }
    }

    printf( "Done" );

    // Close the xed file
    hr = XedCloseFile( &pContext );

    return S_OK;
}

#endif


//--------------------------------------------------------------------------------------
// Name: GetNumPositiveExamples
// Desc: Returns the number of positive labeled examples
//--------------------------------------------------------------------------------------

UINT GestureDetectorTrainer::GetNumPositiveExamples()
{
    UINT nNumPositiveExamples = 0;

    for ( UINT i = 0; i < m_LabeledExamples.m_iLabels.size(); i++ )
    {
        if ( m_LabeledExamples.m_iLabels[ i ] == g_iClassificationLabelCorrect )
        {
            nNumPositiveExamples++;
        }
    }

    return nNumPositiveExamples;
}


//--------------------------------------------------------------------------------------
// Name: SaveLabeledExamples
// Desc: Save the labeled examples to a binary file
//--------------------------------------------------------------------------------------

HRESULT GestureDetectorTrainer::SaveLabeledExamples( const CHAR* szFileName )
{
    FILE* pFile = NULL;
    fopen_s( &pFile, szFileName, "wb" );
    if ( !pFile )
    {
        printf( "\nFailed to open %s for saving labeled examples...\n", szFileName );
        return E_FAIL;
    }

    // Write a text identifier
    fwrite( g_szLabeledExampleFileID, sizeof( g_szLabeledExampleFileID ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    // Write current version number
    FLOAT fBigEndianValue = ByteSwap32Bit( g_fCurrentVersion );
    fwrite( &fBigEndianValue, sizeof( fBigEndianValue ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );
    
    fwrite( &m_nTotalNumGestures, sizeof( m_nTotalNumGestures ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    fwrite( &m_nNumTrainingGestures, sizeof( m_nNumTrainingGestures), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    UINT nNumExamples = (UINT)( m_LabeledExamples.m_pExamples.size() );
    fwrite( &nNumExamples, sizeof( UINT ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    INT8 iLabel;
    UINT64 uTimeStamp;
    
    for ( UINT i = 0; i < nNumExamples; i++ )
    {
        fwrite( m_LabeledExamples.m_pExamples[ i ], sizeof( NUI_SKELETON_DATA ), 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );

        iLabel = m_LabeledExamples.m_iLabels[ i ];
        fwrite( &iLabel, sizeof( INT8 ), 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );

        uTimeStamp = m_LabeledExamples.m_uTimeStamps[ i ];
        fwrite( &uTimeStamp, sizeof( UINT64 ), 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );
    }

    fclose( pFile );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: LoadLabeledExamples
// Desc: Load labeled examples from binary file
//--------------------------------------------------------------------------------------

HRESULT GestureDetectorTrainer::LoadLabeledExamples( const CHAR* szFileName )
{
    FILE* pFile = NULL;
    fopen_s( &pFile, szFileName, "rb" );
    if ( !pFile )
    {
        printf( "\nERROR: Failed to open %s for loading labeled examples...\n", szFileName );
        return E_FAIL;
    }

    Reset();

    // Check that this is indeed a gesture file
    CHAR szFileID[ sizeof( g_szLabeledExampleFileID ) ];
    fread( szFileID, sizeof( g_szLabeledExampleFileID ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    // Check the file header
    if ( !strcmp( szFileID, g_szLabeledExampleFileID ) )
    {
        // Read the version number. This is only added for backwards compatibility when we need to change file formats in the future
        FLOAT fVersion;
        fread( &fVersion, sizeof( fVersion ), 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );
#ifndef _XBOX
        fVersion = ByteSwap32Bit( fVersion );
#endif

        // Check that the file version is the same as the current version
        if ( fabsf( fVersion - g_fCurrentVersion ) > 1.0f )
        {
            fclose( pFile );
            printf( "\nError: File version %f != current version %f\n", fVersion, g_fCurrentVersion );
            return E_FAIL;
        }

        fread( &m_nTotalNumGestures, sizeof( m_nTotalNumGestures ), 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );

        fread( &m_nNumTrainingGestures, sizeof( m_nNumTrainingGestures), 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );

        UINT nNumExamples;
        fread( &nNumExamples, sizeof( UINT ), 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );

        for ( UINT i = 0; i < nNumExamples; i++ )
        {
            NUI_SKELETON_DATA* pSkeletonData = (NUI_SKELETON_DATA*)_aligned_malloc( sizeof( NUI_SKELETON_DATA ), 16 );
            RETURN_ON_NULL( pSkeletonData );

            INT8 iLabel;
            UINT64 uTimeStamp;

            fread( pSkeletonData, sizeof( NUI_SKELETON_DATA ), 1, pFile );
            RETURN_ON_FILE_ERROR( pFile );

            fread( &iLabel, sizeof( INT8 ), 1, pFile );
            RETURN_ON_FILE_ERROR( pFile );

            fread( &uTimeStamp, sizeof( UINT64 ), 1, pFile );
            RETURN_ON_FILE_ERROR( pFile );

            m_LabeledExamples.m_pExamples.push_back( pSkeletonData );
            m_LabeledExamples.m_iLabels.push_back( iLabel );
            m_LabeledExamples.m_uTimeStamps.push_back( uTimeStamp );
        }
    }

    fclose( pFile );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: TrainWeakClassifiers
// Desc: In order to optimize training time and use less memory at training time, we
//       do the training in two passes. The first pass finds all the interesting
//       weak classifiers that contribute to each future, and the second pass uses
//       these weak classifiers to do the final training. More accurate results can 
//       be found when using all the weak classifiers from all features in just one
//       sinlge pass, but in our experiments memory usage easily went up to more than
//       14 GBytes RAM and training times were in the order of hours, versus using only
//       about 3 GBytes RAM and a few minutes of training.
//--------------------------------------------------------------------------------------

HRESULT GestureDetectorTrainer::TrainWeakClassifiers()
{
    DebugOutput output;

    // First pass finds all the interesting weak classifiers that contribute per feature
    GestureDetectorTrainer trainers[ ClassifierData::NUM_FEATURES ];
    for ( UINT i = 0; i < ClassifierData::NUM_FEATURES; i++ )
    {
        printf( "\n\t%d/%d - Feature: %s ", i + 1, ClassifierData::NUM_FEATURES, output.Print( (ClassifierData::EType)i ) );
        RETURN_ON_FAIL( trainers[ i ].TrainWeakClassifiers( (ClassifierData::EType)i, m_LabeledExamples, m_fErrorThreshold ) );
        printf( "(%d)", trainers[ i ].GetNumWeakClassifiers() );
    }

    // Now add all these classifiers into one list that can be used in the seconds pass
    CombineWeakClassifiers( trainers, ClassifierData::NUM_FEATURES );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: TrainWeakClassifiers
// Desc: Trains a strong classifier per feature set and returns the weak classifiers
//       that contributes to that strong classifier
//--------------------------------------------------------------------------------------

HRESULT GestureDetectorTrainer::TrainWeakClassifiers( ClassifierData::EType classifierDataType, LabeledExamples& labeledExamples, const DOUBLE fErrorThreshold )
{
    // Clear and duplicate the training example data
    m_LabeledExamples.m_iLabels.clear();
    m_LabeledExamples.m_pExamples.clear();
    m_LabeledExamples.m_uTimeStamps.clear();

    for ( UINT i = 0; i < labeledExamples.m_iLabels.size(); i++ )
    {
        m_LabeledExamples.m_iLabels.push_back( labeledExamples.m_iLabels[ i ] );
    }

    for ( UINT i = 0; i < labeledExamples.m_pExamples.size(); i++ )
    {
        m_LabeledExamples.m_pExamples.push_back( labeledExamples.m_pExamples[ i ] );
    }

    for ( UINT i = 0; i < labeledExamples.m_uTimeStamps.size(); i++ )
    {
        m_LabeledExamples.m_uTimeStamps.push_back( labeledExamples.m_uTimeStamps[ i ] );
    }
 
    // Run Adaboost only on this feature set
    m_fErrorThreshold = fErrorThreshold;
    RETURN_ON_FAIL( GenerateWeakClassifiers( classifierDataType ) );
    RETURN_ON_FAIL( TrainStrongClassifier( FALSE ) );
    Optimize( 0, FALSE );

    m_LabeledExamples.m_iLabels.clear();
    m_LabeledExamples.m_pExamples.clear();
    m_LabeledExamples.m_uTimeStamps.clear();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GenerateWeakClassifiers
// Desc: Generates a set of weak classifiers as decision stumps, given some parameters
//       for min, max and the step value between min and max
//--------------------------------------------------------------------------------------

HRESULT GestureDetectorTrainer::GenerateWeakClassifiers( ClassifierData* pClassifierData,
                                                         const FLOAT fMin,
                                                         const FLOAT fMax,
                                                         const FLOAT fStep )
{
    WeakClassifier  weakClassifier;
    UINT uID;

    // Use the index in the array as the unique id
    uID = (UINT)m_ClassifierData.size();
    pClassifierData->SetID( uID );

    // Add the data
    m_ClassifierData.push_back( pClassifierData );

    // Now add rejection of inferred joints
    ClassifierData* pClassifierDataClone = pClassifierData->Clone();
    RETURN_ON_NULL( pClassifierDataClone );
    pClassifierDataClone->SetRejectInfferedJoints( TRUE );

    // Use the index in the array as the unique id
    uID = (UINT)m_ClassifierData.size();
    pClassifierDataClone->SetID( uID );

    // Add the cloned data
    m_ClassifierData.push_back( pClassifierDataClone );       

    // For each threshold we generate 4 classifiers. Two for choosing to use or
    // reject inferred joints, and each one of those are also reversed, so that
    // the learning algorithm can find the best weak classifier
    ClassifierData* pData[ 2 ] = { pClassifierData,  pClassifierDataClone };

    for ( UINT i = 0; i < 2; i++ )
    {
        for ( FLOAT fThreshold = fMin; fThreshold <= fMax; fThreshold += fStep )
        {
            // Setup the weak classifier
            weakClassifier.SetThreshold( fThreshold );
            weakClassifier.SetData( pData[ i ] );

            // Add the weak classifiers
            weakClassifier.SetIsReversed( FALSE );
            m_StrongClassifier.Add( weakClassifier );
        }
    }

    for ( UINT i = 0; i < 2; i++ )
    {
        for ( FLOAT fThreshold = fMin; fThreshold <= fMax; fThreshold += fStep )
        {
            // Setup the weak classifier
            weakClassifier.SetThreshold( fThreshold );
            weakClassifier.SetData( pData[ i ] );

            // Add the weak classifiers
            weakClassifier.SetIsReversed( TRUE );
            m_StrongClassifier.Add( weakClassifier );
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GenerateWeakClassifiers
// Desc: This is where all the classifiers are generated for each feature
//--------------------------------------------------------------------------------------

HRESULT GestureDetectorTrainer::GenerateWeakClassifiers( ClassifierData::EType classifierDataType )
{
    WeakClassifier  weakClassifier;
    ClassifierData* pClassifierData;

    NUI_SKELETON_POSITION_INDEX verticalAngleJoints[] = { NUI_SKELETON_POSITION_HEAD,
                                                          NUI_SKELETON_POSITION_SHOULDER_LEFT,
                                                          NUI_SKELETON_POSITION_ELBOW_LEFT,
                                                          NUI_SKELETON_POSITION_WRIST_LEFT,
                                                          NUI_SKELETON_POSITION_HAND_LEFT,
                                                          NUI_SKELETON_POSITION_SHOULDER_RIGHT,
                                                          NUI_SKELETON_POSITION_ELBOW_RIGHT,
                                                          NUI_SKELETON_POSITION_WRIST_RIGHT,
                                                          NUI_SKELETON_POSITION_HAND_RIGHT,
                                                          NUI_SKELETON_POSITION_HIP_LEFT,
                                                          NUI_SKELETON_POSITION_KNEE_LEFT,
                                                          NUI_SKELETON_POSITION_ANKLE_LEFT,
                                                          NUI_SKELETON_POSITION_FOOT_LEFT,
                                                          NUI_SKELETON_POSITION_HIP_RIGHT,
                                                          NUI_SKELETON_POSITION_KNEE_RIGHT,
                                                          NUI_SKELETON_POSITION_ANKLE_RIGHT,
                                                          NUI_SKELETON_POSITION_FOOT_RIGHT };

    UINT nNumClassifierData = ARRAYSIZE( verticalAngleJoints );
    for ( UINT i = 0; i < nNumClassifierData; i++ )
    {
#ifdef ADD_TYPE_ANGLE
        if ( classifierDataType == ClassifierData::TYPE_ANGLE )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingAngles( NUI_SKELETON_POSITION_SPINE,
                                                                             NUI_SKELETON_POSITION_SHOULDER_CENTER,
                                                                             verticalAngleJoints[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fAngleMin, fAngleMax, fAngleStep ) );
        }
#endif

#ifdef ADD_TYPE_ANGLE_VELOCITY
        if ( classifierDataType == ClassifierData::TYPE_ANGLE_VELOCITY )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingAngleVelocities( NUI_SKELETON_POSITION_SPINE,
                                                                                      NUI_SKELETON_POSITION_SHOULDER_CENTER,
                                                                                      verticalAngleJoints[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fAngleVelocityMin, fAngleVelocityMax, fAngleVelocityStep ) );
        }
#endif

#ifdef ADD_TYPE_ANGLE_ACCELERATION
        if ( classifierDataType == ClassifierData::TYPE_ANGLE_ACCELERATION )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingAngleAcceleration( NUI_SKELETON_POSITION_SPINE,
                                                                                        NUI_SKELETON_POSITION_SHOULDER_CENTER,
                                                                                        verticalAngleJoints[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fAngleAccelMin, fAngleAccelMax, fAngleAccelStep ) );
        }
#endif
    }
    
    NUI_SKELETON_POSITION_INDEX leftHorizontalAngleJoints[] = { NUI_SKELETON_POSITION_HEAD,
                                                                NUI_SKELETON_POSITION_SPINE,
                                                                NUI_SKELETON_POSITION_ELBOW_LEFT,
                                                                NUI_SKELETON_POSITION_WRIST_LEFT,
                                                                NUI_SKELETON_POSITION_HAND_LEFT,
                                                                NUI_SKELETON_POSITION_HIP_LEFT,
                                                                NUI_SKELETON_POSITION_KNEE_LEFT,
                                                                NUI_SKELETON_POSITION_ANKLE_LEFT,
                                                                NUI_SKELETON_POSITION_FOOT_LEFT };

    nNumClassifierData = ARRAYSIZE( leftHorizontalAngleJoints );
    for ( UINT i = 0; i < nNumClassifierData; i++ )
    {
#ifdef ADD_TYPE_ANGLE
        if ( classifierDataType == ClassifierData::TYPE_ANGLE )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingAngles( NUI_SKELETON_POSITION_SHOULDER_CENTER,
                                                                             NUI_SKELETON_POSITION_SHOULDER_LEFT,
                                                                             leftHorizontalAngleJoints[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fAngleMin, fAngleMax, fAngleStep ) );
        }
#endif

#ifdef ADD_TYPE_ANGLE_VELOCITY
        if ( classifierDataType == ClassifierData::TYPE_ANGLE_VELOCITY )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingAngleVelocities( NUI_SKELETON_POSITION_SHOULDER_CENTER,
                                                                                      NUI_SKELETON_POSITION_SHOULDER_LEFT,
                                                                                      leftHorizontalAngleJoints[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fAngleVelocityMin, fAngleVelocityMax, fAngleVelocityStep ) );
        }
#endif

#ifdef ADD_TYPE_ANGLE_ACCELERATION
        if ( classifierDataType == ClassifierData::TYPE_ANGLE_ACCELERATION )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingAngleAcceleration( NUI_SKELETON_POSITION_SHOULDER_CENTER,
                                                                                        NUI_SKELETON_POSITION_SHOULDER_LEFT,
                                                                                        leftHorizontalAngleJoints[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fAngleAccelMin, fAngleAccelMax, fAngleAccelStep ) );
        }
#endif
    }

    NUI_SKELETON_POSITION_INDEX rightHorizontalAngleJoints[] = { NUI_SKELETON_POSITION_HEAD,
                                                                 NUI_SKELETON_POSITION_SPINE,
                                                                 NUI_SKELETON_POSITION_ELBOW_RIGHT,
                                                                 NUI_SKELETON_POSITION_WRIST_RIGHT,
                                                                 NUI_SKELETON_POSITION_HAND_RIGHT,
                                                                 NUI_SKELETON_POSITION_HIP_RIGHT,
                                                                 NUI_SKELETON_POSITION_KNEE_RIGHT,
                                                                 NUI_SKELETON_POSITION_ANKLE_RIGHT,
                                                                 NUI_SKELETON_POSITION_FOOT_RIGHT };

    nNumClassifierData = ARRAYSIZE( rightHorizontalAngleJoints );
    for ( UINT i = 0; i < nNumClassifierData; i++ )
    {
#ifdef ADD_TYPE_ANGLE
        if ( classifierDataType == ClassifierData::TYPE_ANGLE )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingAngles( NUI_SKELETON_POSITION_SHOULDER_CENTER,
                                                                             NUI_SKELETON_POSITION_SHOULDER_RIGHT,
                                                                             rightHorizontalAngleJoints[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fAngleMin, fAngleMax, fAngleStep ) );
        }
#endif

#ifdef ADD_TYPE_ANGLE_VELOCITY
        if ( classifierDataType == ClassifierData::TYPE_ANGLE_VELOCITY )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingAngleVelocities( NUI_SKELETON_POSITION_SHOULDER_CENTER,
                                                                                      NUI_SKELETON_POSITION_SHOULDER_RIGHT,
                                                                                      rightHorizontalAngleJoints[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fAngleVelocityMin, fAngleVelocityMax, fAngleVelocityStep ) );
        }
#endif

#ifdef ADD_TYPE_ANGLE_ACCELERATION
        if ( classifierDataType == ClassifierData::TYPE_ANGLE_ACCELERATION )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingAngleAcceleration( NUI_SKELETON_POSITION_SHOULDER_CENTER,
                                                                                        NUI_SKELETON_POSITION_SHOULDER_RIGHT,
                                                                                        rightHorizontalAngleJoints[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fAngleAccelMin, fAngleAccelMax, fAngleAccelStep ) );
        }
#endif
    }

    NUI_SKELETON_POSITION_INDEX generalAngleJoints[][3] = { { NUI_SKELETON_POSITION_HAND_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT },
                                                            { NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_SHOULDER_LEFT },
                                                            { NUI_SKELETON_POSITION_HAND_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT },
                                                            { NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_SHOULDER_RIGHT },
                                                            { NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_HIP_LEFT },
                                                            { NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_HIP_RIGHT },
                                                            { NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_WRIST_RIGHT },
                                                            { NUI_SKELETON_POSITION_HAND_LEFT, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_HAND_RIGHT },
                                                            { NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_KNEE_RIGHT },
                                                            { NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_KNEE_LEFT },
                                                            { NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_KNEE_RIGHT },
                                                            { NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_KNEE_RIGHT },
                                                            { NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_KNEE_LEFT },
                                                          };
    nNumClassifierData = ARRAYSIZE( generalAngleJoints );
    for ( UINT i = 0; i < nNumClassifierData; i++ )
    {
#ifdef ADD_TYPE_ANGLE
        if ( classifierDataType == ClassifierData::TYPE_ANGLE )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingAngles( generalAngleJoints[i][0], generalAngleJoints[i][1], generalAngleJoints[i][2] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fAngleMin, fAngleMax, fAngleStep ) );
        }
#endif

#ifdef ADD_TYPE_ANGLE_VELOCITY
        if ( classifierDataType == ClassifierData::TYPE_ANGLE_VELOCITY )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingAngleVelocities( generalAngleJoints[i][0], generalAngleJoints[i][1], generalAngleJoints[i][2] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fAngleVelocityMin, fAngleVelocityMax, fAngleVelocityStep ) );
        }
#endif

#ifdef ADD_TYPE_ANGLE_ACCELERATION
        if ( classifierDataType == ClassifierData::TYPE_ANGLE_ACCELERATION )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingAngleAcceleration( generalAngleJoints[i][0], generalAngleJoints[i][1], generalAngleJoints[i][2] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fAngleAccelMin, fAngleAccelMax, fAngleAccelStep ) );
        }
#endif
    }

#ifdef ADD_TYPE_TIME_SPACE_ANGLE
    if ( classifierDataType == ClassifierData::TYPE_TIME_SPACE_ANGLE )
    {
        for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingTimeSpaceAngles( (NUI_SKELETON_POSITION_INDEX)i ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fTimeSpaceAngleMin, fTimeSpaceAngleMax, fTimeSpaceAngleStep ) );
        }
    }
#endif

#ifdef ADD_TYPE_MUSCLE_POWER
    if ( classifierDataType == ClassifierData::TYPE_MUSCLE_POWER )
    {
        for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingMusclePower( (NUI_SKELETON_POSITION_INDEX)i ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fMusclePowerMin, fMusclePowerMax, fMusclePowerStep ) );
        }
    }
#endif

#ifdef ADD_TYPE_MUSCLE_FORCES
    if ( classifierDataType == ClassifierData::TYPE_MUSCLE_FORCE_X )
    {
        for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingMuscleForceX( (NUI_SKELETON_POSITION_INDEX)i ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fMuscleForceMin, fMuscleForceMax, fMuscleForceStep ) );
        }
    }
#endif

#ifdef ADD_TYPE_MUSCLE_FORCES
    if ( classifierDataType == ClassifierData::TYPE_MUSCLE_FORCE_Y )
    {
        for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingMuscleForceY( (NUI_SKELETON_POSITION_INDEX)i ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fMuscleForceMin, fMuscleForceMax, fMuscleForceStep ) );
        }
    }
#endif

#ifdef ADD_TYPE_MUSCLE_FORCES
    if ( classifierDataType == ClassifierData::TYPE_MUSCLE_FORCE_Z )
    {

        for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingMuscleForceZ( (NUI_SKELETON_POSITION_INDEX)i ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fMuscleForceMin, fMuscleForceMax, fMuscleForceStep ) );
        }
    }
#endif


#ifdef ADD_TYPE_MUSCLE_TORQUES
    if ( classifierDataType == ClassifierData::TYPE_MUSCLE_TORQUE_X )
    {
        for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingMuscleTorqueX( (NUI_SKELETON_POSITION_INDEX)i ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fMuscleTorqueMin, fMuscleTorqueMax, fMuscleTorqueStep ) );
        }
    }
#endif

#ifdef ADD_TYPE_MUSCLE_TORQUES
    if ( classifierDataType == ClassifierData::TYPE_MUSCLE_TORQUE_Y )
    {
        for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingMuscleTorqueY( (NUI_SKELETON_POSITION_INDEX)i ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fMuscleTorqueMin, fMuscleTorqueMax, fMuscleTorqueStep ) );
        }
    }
#endif

#ifdef ADD_TYPE_MUSCLE_TORQUES
    if ( classifierDataType == ClassifierData::TYPE_MUSCLE_TORQUE_Z )
    {
        for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingMuscleTorqueZ( (NUI_SKELETON_POSITION_INDEX)i ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fMuscleTorqueMin, fMuscleTorqueMax, fMuscleTorqueStep ) );
        }
    }
#endif

    NUI_SKELETON_POSITION_INDEX positionJoints[] = { NUI_SKELETON_POSITION_HAND_LEFT,
                                                     NUI_SKELETON_POSITION_WRIST_LEFT,
                                                     NUI_SKELETON_POSITION_HAND_RIGHT,
                                                     NUI_SKELETON_POSITION_WRIST_RIGHT,
                                                     NUI_SKELETON_POSITION_ELBOW_LEFT,
                                                     NUI_SKELETON_POSITION_ELBOW_RIGHT,
                                                     NUI_SKELETON_POSITION_HIP_LEFT,
                                                     NUI_SKELETON_POSITION_HIP_RIGHT,
                                                     NUI_SKELETON_POSITION_KNEE_LEFT,
                                                     NUI_SKELETON_POSITION_KNEE_RIGHT,
                                                     NUI_SKELETON_POSITION_ANKLE_LEFT,
                                                     NUI_SKELETON_POSITION_ANKLE_RIGHT };
    nNumClassifierData = ARRAYSIZE( positionJoints );

#ifdef ADD_TYPE_DIFF_POSITION_X
    if ( classifierDataType == ClassifierData::TYPE_DIFF_POSITION_X )
    {
        for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
        {
            for ( UINT j = 0; j < nNumClassifierData; j++ )
            {
                if ( i != (UINT)positionJoints[ j ] )
                {
                    RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingDiffPositionX( positionJoints[ j ], (NUI_SKELETON_POSITION_INDEX)i ) );
                    RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fPositionMin, fPositionMax, fPositionStep ) );
                }
            }
        }
    }
#endif

#ifdef ADD_TYPE_DIFF_POSITION_Y
    if ( classifierDataType == ClassifierData::TYPE_DIFF_POSITION_Y )
    {
        for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
        {
            for ( UINT j = 0; j < nNumClassifierData; j++ )
            {
                if ( i != (UINT)positionJoints[ j ] )
                {
                    RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingDiffPositionY( positionJoints[ j ], (NUI_SKELETON_POSITION_INDEX)i ) );
                    RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fPositionMin, fPositionMax, fPositionStep ) );
                }
            }
        }
    }
#endif

#ifdef ADD_TYPE_DIFF_POSITION_Z
    if ( classifierDataType == ClassifierData::TYPE_DIFF_POSITION_Z )
    {
        for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
        {
            for ( UINT j = 0; j < nNumClassifierData; j++ )
            {
                if ( i != (UINT)positionJoints[ j ] )
                {
                    RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingDiffPositionZ( positionJoints[ j ], (NUI_SKELETON_POSITION_INDEX)i ) );
                    RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fPositionMin, fPositionMax, fPositionStep ) );
                }
            }
        }
    }
#endif

#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_X
    if ( classifierDataType == ClassifierData::TYPE_DIFF_MUSCLE_FORCE_X )
    {
        for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
        {
            for ( UINT j = 0; j < nNumClassifierData; j++ )
            {
                if ( i != (UINT)positionJoints[ j ] )
                {
                    RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingDiffMuscleForceX( positionJoints[ j ], (NUI_SKELETON_POSITION_INDEX)i ) );
                    RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fDiffMuscleForceMin, fDiffMuscleForceMax, fDiffMuscleForceStep ) );
                }
            }
        }
    }
#endif

#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_Y
    if ( classifierDataType == ClassifierData::TYPE_DIFF_MUSCLE_FORCE_Y )
    {
        for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
        {
            for ( UINT j = 0; j < nNumClassifierData; j++ )
            {
                if ( i != (UINT)positionJoints[ j ] )
                {
                    RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingDiffMuscleForceY( positionJoints[ j ], (NUI_SKELETON_POSITION_INDEX)i ) );
                    RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fDiffMuscleForceMin, fDiffMuscleForceMax, fDiffMuscleForceStep ) );
                }
            }
        }
    }
#endif

#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_Z
    if ( classifierDataType == ClassifierData::TYPE_DIFF_MUSCLE_FORCE_Z )
    {
        for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
        {
            for ( UINT j = 0; j < nNumClassifierData; j++ )
            {
                if ( i != (UINT)positionJoints[ j ] )
                {
                    RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingDiffMuscleForceZ( positionJoints[ j ], (NUI_SKELETON_POSITION_INDEX)i ) );
                    RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fDiffMuscleForceMin, fDiffMuscleForceMax, fDiffMuscleForceStep ) );
                }
            }
        }
    }
#endif

    NUI_SKELETON_POSITION_INDEX jointVelocities[] = { NUI_SKELETON_POSITION_HEAD,
                                                      NUI_SKELETON_POSITION_SHOULDER_CENTER,
                                                      NUI_SKELETON_POSITION_SHOULDER_LEFT,
                                                      NUI_SKELETON_POSITION_ELBOW_LEFT,
                                                      NUI_SKELETON_POSITION_WRIST_LEFT,
                                                      NUI_SKELETON_POSITION_HAND_LEFT,
                                                      NUI_SKELETON_POSITION_SHOULDER_RIGHT,
                                                      NUI_SKELETON_POSITION_ELBOW_RIGHT,
                                                      NUI_SKELETON_POSITION_WRIST_RIGHT,
                                                      NUI_SKELETON_POSITION_HAND_RIGHT,
                                                      NUI_SKELETON_POSITION_HIP_LEFT,
                                                      NUI_SKELETON_POSITION_HIP_LEFT,
                                                      NUI_SKELETON_POSITION_KNEE_LEFT,
                                                      NUI_SKELETON_POSITION_ANKLE_LEFT,
                                                      NUI_SKELETON_POSITION_KNEE_RIGHT,
                                                      NUI_SKELETON_POSITION_ANKLE_RIGHT };

    nNumClassifierData = ARRAYSIZE( jointVelocities );

#ifdef ADD_TYPE_POSITION_SPEED
    if ( classifierDataType == ClassifierData::TYPE_POSITION_SPEED )
    {
        for ( UINT i = 0; i < nNumClassifierData; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingPositionSpeed( jointVelocities[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fSpeedMin, fSpeedMax, fSpeedStep ) );
        }
    }
#endif

#ifdef ADD_TYPE_POSITION_SPEED_SQ
    if ( classifierDataType == ClassifierData::TYPE_POSITION_SPEED_SQ )
    {
        for ( UINT i = 0; i < nNumClassifierData; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingPositionSpeedSQ( jointVelocities[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fSpeedSQMin, fSpeedSQMax, fSpeedSQStep ) );
        }
    }
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION
    if ( classifierDataType == ClassifierData::TYPE_POSITION_ACCELERATION )
    {
        for ( UINT i = 0; i < nNumClassifierData; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingPositionAcceleration( jointVelocities[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fAccelMin, fAccelMax, fAccelStep ) );
        }
    }
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION_X
    if ( classifierDataType == ClassifierData::TYPE_POSITION_ACCELERATION_X )
    {
        for ( UINT i = 0; i < nNumClassifierData; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingPositionAccelerationX( jointVelocities[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fAccelMin, fAccelMax, fAccelStep ) );
        }
    }
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION_Y
    if ( classifierDataType == ClassifierData::TYPE_POSITION_ACCELERATION_Y )
    {
        for ( UINT i = 0; i < nNumClassifierData; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingPositionAccelerationY( jointVelocities[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fAccelMin, fAccelMax, fAccelStep ) );
        }
    }
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION_Z
    if ( classifierDataType == ClassifierData::TYPE_POSITION_ACCELERATION_Z )
    {
        for ( UINT i = 0; i < nNumClassifierData; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingPositionAccelerationZ( jointVelocities[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fAccelMin, fAccelMax, fAccelStep ) );
        }
    }
#endif

#ifdef ADD_TYPE_POSITION_VELOCITY_X
    if ( classifierDataType == ClassifierData::TYPE_POSITION_VELOCITY_X )
    {
        for ( UINT i = 0; i < nNumClassifierData; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingPositionVelocityX( jointVelocities[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fVelocityMin, fVelocityMax, fVelocityStep ) );
        }
    }
#endif

#ifdef ADD_TYPE_POSITION_VELOCITY_Y
    if ( classifierDataType == ClassifierData::TYPE_POSITION_VELOCITY_Y )
    {
        for ( UINT i = 0; i < nNumClassifierData; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingPositionVelocityY( jointVelocities[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fVelocityMin, fVelocityMax, fVelocityStep ) );
        }
    }
#endif

#ifdef ADD_TYPE_POSITION_VELOCITY_Z
    if ( classifierDataType == ClassifierData::TYPE_POSITION_VELOCITY_Z )
    {
        for ( UINT i = 0; i < nNumClassifierData; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingPositionVelocityZ( jointVelocities[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fVelocityMin, fVelocityMax, fVelocityStep ) );
        }
    }
#endif

#ifdef ADD_TYPE_POSITION_VELOCITYSQ_X
    if ( classifierDataType == ClassifierData::TYPE_POSITION_VELOCITYSQ_X )
    {
        for ( UINT i = 0; i < nNumClassifierData; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingPositionVelocitySQX( jointVelocities[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fVelocitySQMin, fVelocitySQMax, fVelocitySQStep ) );
        }
    }
#endif

#ifdef ADD_TYPE_POSITION_VELOCITYSQ_Y
    if ( classifierDataType == ClassifierData::TYPE_POSITION_VELOCITYSQ_Y )
    {
        for ( UINT i = 0; i < nNumClassifierData; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingPositionVelocitySQY( jointVelocities[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fVelocitySQMin, fVelocitySQMax, fVelocitySQStep ) );
        }
    }
#endif

#ifdef ADD_TYPE_POSITION_VELOCITYSQ_Z
    if ( classifierDataType == ClassifierData::TYPE_POSITION_VELOCITYSQ_Z )
    {
        for ( UINT i = 0; i < nNumClassifierData; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingPositionVelocitySQZ( jointVelocities[ i ] ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fVelocitySQMin, fVelocitySQMax, fVelocitySQStep ) );
        }
    }
#endif

    struct Bone
    {
        NUI_SKELETON_POSITION_INDEX parent;
        NUI_SKELETON_POSITION_INDEX child;
    };

    Bone bones[] = { { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT },
                     { NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT },
                     { NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT },
                     { NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT },
                     { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT },
                     { NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT },
                     { NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT },
                     { NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT },
                     { NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT },
                     { NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT },
                     { NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT },
                     { NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT },
                     { NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT },
                     { NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT },
                     { NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT },
                     { NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT },
                     { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SPINE },
                     { NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_SPINE },
                     { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_HEAD },
                     { NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_KNEE_LEFT },
                     { NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_KNEE_RIGHT },
                     { NUI_SKELETON_POSITION_HAND_LEFT, NUI_SKELETON_POSITION_HAND_RIGHT },
                     { NUI_SKELETON_POSITION_HAND_LEFT, NUI_SKELETON_POSITION_SHOULDER_RIGHT },
                     { NUI_SKELETON_POSITION_HAND_RIGHT, NUI_SKELETON_POSITION_SHOULDER_LEFT } };
    nNumClassifierData = ARRAYSIZE( bones );

#ifdef ADD_TYPE_BONE_LENGTH_CHANGES
    if ( classifierDataType == ClassifierData::TYPE_BONE_LENGTH_CHANGES )
    {
        for ( UINT i = 0; i < nNumClassifierData; i++ )
        {
            RETURN_ON_NULL( pClassifierData = new ClassifierDataUsingBoneLengthChanges( bones[ i ].parent, bones[ i ].child ) );
            RETURN_ON_FAIL( GenerateWeakClassifiers( pClassifierData, fBoneChangesMin, fBoneChangesMax, fBoneChangesStep ) );
        }
    }
#endif


    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CombineWeakClassifiers
// Desc: Combine all weak classifiers in the first pass so that it can be used in the
//       seconds pass of training for the final strong classifier
//--------------------------------------------------------------------------------------

VOID GestureDetectorTrainer::CombineWeakClassifiers( GestureDetectorTrainer* pGestureDetectorTrainers, const UINT nNumTrainers )
{
    WeakClassifier  weakClassifier;

    for ( UINT i = 0; i < nNumTrainers; i++ )
    {
        GestureDetectorTrainer* pGestureDetectorTrainer = &pGestureDetectorTrainers[ i ];

        for ( UINT j = 0; j < pGestureDetectorTrainer->GetNumWeakClassifiers(); j++ )
        {
            WeakClassifier* pWeakClassifier = pGestureDetectorTrainer->m_StrongClassifier.GetWeakClassifierAt( j );

            // Add the weak classifier
            weakClassifier.SetThreshold( pWeakClassifier->GetThreshold() );
            weakClassifier.SetData( pWeakClassifier->GetData() );
            weakClassifier.SetIsReversed( pWeakClassifier->GetIsReversed() );
            weakClassifier.SetAlpha( 0.0f );
            m_StrongClassifier.Add( weakClassifier );
            pWeakClassifier->SetData( NULL );
        }
    }        
}


//--------------------------------------------------------------------------------------
// Name: TrainStrongClassifier
// Desc: Implementation of the AdaBoost learning algorithm
//--------------------------------------------------------------------------------------

HRESULT GestureDetectorTrainer::TrainStrongClassifier( const BOOL bFinalPass )
{
    if ( bFinalPass )
    {
        printf( "\n\t-Evaluating classifier data for each example skeleton frame..." );
    }

    // Time how long this takes
    DWORD dwStart = GetTickCount();

    // Clear all the data
    ClassifierData::Initialize();
    m_StrongClassifier.Reset( 0 );
    m_uPreviousTimeStamp = 0;

    // Cache values from weak classifier data on ground truth so that
    // we don't have to call Update() NxN times, but only N times during training
    const UINT nNumExamples = (UINT)( m_LabeledExamples.m_pExamples.size() );
    const UINT nNumClassifierData = (UINT)( m_ClassifierData.size() );
    const UINT nNumWeakClassifiers = m_StrongClassifier.GetNumWeakClassifiers();  // Number of weak classifiers
    vector<vector<FLOAT>> cachedClassifierDataValues;

    // We're caching lots of values to optimize training time, so we better make sure
    // there aren't any bad memory allocations from STL
    try
    {
        cachedClassifierDataValues.resize( nNumClassifierData );
        for ( UINT i = 0; i < nNumClassifierData; i++ )
        {
            cachedClassifierDataValues[ i ].resize( nNumExamples );
        }
    }
    catch ( bad_alloc& badAllocation )
    {
        printf( "\n\nCould not allocate memory (%s)", badAllocation.what() );
        return E_FAIL;
    }
    
    // Do the work that would normally have been done millions of times in the inner loop of the training
    // and cache the results of the Update() with regards to each classifier data instance
    for ( UINT i = 0; i < nNumExamples; i++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = m_LabeledExamples.m_pExamples[ i ];
        UINT64 uTimeStamp = m_LabeledExamples.m_uTimeStamps[ i ];

        LARGE_INTEGER liTimeStamp;
        liTimeStamp.QuadPart = uTimeStamp;
        Update( 0, pSkeletonData, liTimeStamp );

        // Let openMP run this loop on multiple threads
        #pragma omp parallel for
        for ( INT j = 0; j < (INT)nNumClassifierData; j++ )
        {
            cachedClassifierDataValues[ j ][ i ] =  m_ClassifierData[ j ]->GetValue( 0 );
        }
    }

    // We're caching lots of values to optimize training time, so we better make sure
    // there aren't any bad memory allocations from STL
    vector<vector<BYTE>> cachedClassificationsError;
    try
    {
        cachedClassificationsError.resize( nNumWeakClassifiers );
        for ( UINT i = 0; i < nNumWeakClassifiers; i++ )
        {
            cachedClassificationsError[ i ].resize( nNumExamples );
        }
    }
    catch ( bad_alloc& badAllocation )
    {
        printf( "\n\nCould not allocate memory (%s)", badAllocation.what() );
        return E_FAIL;
    }

    vector<UINT> cachedClassificationErrorSum;
    try
    {
        cachedClassificationErrorSum.resize( nNumWeakClassifiers );
    }
    catch ( bad_alloc& badAllocation )
    {
        printf( "\n\nCould not allocate memory (%s)", badAllocation.what() );
        return E_FAIL;
    }

    // Classify() will get called millions of times in the inner loop of training on already known data.
    // We therefore iIterate through all weak classifiers and cache the results of Classify() for each
    // weak classifier with regards to each example data
    #pragma omp parallel for
    for ( INT i = 0; i < (INT)nNumWeakClassifiers; i++ )
    {
        UINT uSum = 0;
        WeakClassifier* pWeakClassifier = m_StrongClassifier.GetWeakClassifierAt( i );

        // Iterate through all ground truth examples
        for ( UINT n = 0; n < nNumExamples; n++ )
        {
            UINT uClassifierDataIndex = pWeakClassifier->GetData()->GetID();
            FLOAT fValue = cachedClassifierDataValues[ uClassifierDataIndex ][ n ];
            INT8 iWeakClassifierResult = pWeakClassifier->Classify( fValue );
            INT8 iGroundTruthLabel = m_LabeledExamples.m_iLabels[ n ];

            // if the weak classifier wrongly classifies this example, then contribute to the error
            cachedClassificationsError[ i ][ n ] = ( iWeakClassifierResult == iGroundTruthLabel ) ? 0 : 1;

            // For optimization, get the sum of the error
            uSum += cachedClassificationsError[ i ][ n ];
        }

        cachedClassificationErrorSum[ i ] = uSum;
    }

    DWORD dwStop = GetTickCount();
    if ( bFinalPass )
    {
        printf( "Done" );
    }

    dwStart = GetTickCount();

    // User specified 0, which means use all weak classifiers at runtime, but only the ones that pass the error threshold
    const BOOL bUseAllWeakClassifiers = ( m_nNumWeakClassifiersAtRuntime == 0 );

    // Number of examples in training set. N is normally used in literature.
    const UINT N = nNumExamples;

    // Number of iterations for training loop. T is normally used in literature. If it is an intermediate pass for one of the
    // feature sets, we simply use all the generated weak classifiers and stop when one of them reaches the error threshold. But,
    // if this is for the final pass where the final strong classifier is trained, we use the number of runtime classifiers specified by the user
    const UINT T = bFinalPass ? ( bUseAllWeakClassifiers ? nNumWeakClassifiers : m_nNumWeakClassifiersAtRuntime ) : nNumWeakClassifiers;

    // The highest error threshold is 0.5 since a weak classifier has to have a better than 50/50 change to be correct, anything higher means
    // that the weak classifier is simply too weak to controibute to solving the problem space
    const DOUBLE fErrorThreshold = bFinalPass ? 0.5 : m_fErrorThreshold;

    vector<DOUBLE> d( N, 1.0 / N );                     // Distributed weight for each example
    vector<BOOL> bClassifierChoosen( nNumWeakClassifiers, FALSE );

    if ( bFinalPass )
    {       
        printf( "\n\t-Running AdaBoost using %d (of %d available) hardware threads...", m_nNumThreadsForTraining, m_nMaxNumThreads );
    }

    // Optimization. Since we're auto generating the classifiers with a min->max range and step, there are most
    // likely lots of overlapping. If we do find that the cached results for classifiers next to each other in
    // the range are exactly the sample for all examples, the we simply remove one of them. Theoretically speaking
    // the removed classifier still has a chance to be boosted by the algorithm, but emperical results show that
    // we can safely remove these from the pool of classifiers.
    if ( !bFinalPass )
    {
        for ( INT i = 1; i < (INT)nNumWeakClassifiers; i++ )
        {
            // if the sum isn't the same, then no need to look through each then no need to compare each example
            if ( cachedClassificationErrorSum[ i ] != cachedClassificationErrorSum[ i - 1 ] )
            {
                continue;
            }

            // if the sum is the same, then check the classification error on each example for both classifiers
            BOOL bClassifiersTheSame = TRUE;

            for ( INT n = 0; n < (INT)nNumExamples; n++ )
            {
                if ( cachedClassificationsError[ i ][ n ] != cachedClassificationsError[ i - 1 ][ n ] )
                {
                    bClassifiersTheSame = FALSE;
                    break;
                }
            }

            // if two neigbor classifiers give the same results, the remove it from the pool of classifiers that
            // AdaBoost will work with.
            if ( bClassifiersTheSame )
            {
                bClassifierChoosen[ i ] = TRUE;
            }
        }
    }

    // AdaBoost algorithm, iterate T times
    for ( UINT t = 0; t < T; t++ )
    {
        // Find the best weak classifier, i.e. the weak classifier with the smallest error
        UINT uBestWeakClassifierIndex = 0;
        DOUBLE fMinError = FLT_MAX;

        // Let openMP run this loop on multiple threads
        #pragma omp parallel for
        // Iterate through all weak classifiers
        for ( INT i = 0; i < (INT)nNumWeakClassifiers; i++ )
        {
            // We could remove a weak classifier from the list and add it to another list, but with
            // so many potential classsifers, it's more optimal to just mark it when it's used
            if ( !bClassifierChoosen[ i ] )
            {
                DOUBLE fError = 0.0;

                // Iterate through all ground truth examples
                for ( UINT n = 0; n < N; n++ )
                {
                    fError += d[ n ] * (DOUBLE)(cachedClassificationsError[ i ][ n ]);
                }
                
                // Let openMP know that this if() operation cannot be parallelized
                #pragma omp critical
                if ( fError < fMinError )
                {
                    fMinError = fError;
                    uBestWeakClassifierIndex = i;
                }
            }
        }

        // A weak classifier's error has to be less than 0.5 to contribute successfully, since a weak classifier
        // has to to be better than 50/50 chance of doing a correct classification
        if ( fMinError >= fErrorThreshold )
        {
            break;
        }

        bClassifierChoosen[ uBestWeakClassifierIndex ] = TRUE;
        WeakClassifier* pBestWeakClassifier = m_StrongClassifier.GetWeakClassifierAt( uBestWeakClassifierIndex );

        // Get the confidence of the weak classifier as alpha (lower error => higher alpha)
        DOUBLE fAlpha = ( fMinError == 0.0f ) ? 0.0f : ( 0.5 * log( ( 1.0 - fMinError ) / fMinError ) );
        pBestWeakClassifier->SetAlpha( (FLOAT)fAlpha );

        // Emphasize the training examples that do not agree with the weak classifier h(x)
        DOUBLE Z = 0.0;
        #pragma omp parallel for
        for ( INT n = 0; n < (INT)N; n++ )
        {
            INT8 iGroundTruthLabel = m_LabeledExamples.m_iLabels[ n ];
            UINT uClassifierDataIndex = pBestWeakClassifier->GetData()->GetID();
            FLOAT fValue = cachedClassifierDataValues[ uClassifierDataIndex ][ n ];

            d[ n ] *= exp( -fAlpha * iGroundTruthLabel * pBestWeakClassifier->Classify( fValue ) );
        }

        // We need to normalize the distribution, but we want to parallelize it, so first get the sum
        for ( INT n = 0; n < (INT)N; n++ )
        {
            Z += d[ n ];
        }

        // Normalize to a probability distribution and use openMP to do this on multiple threads
        #pragma omp parallel for
        for ( INT n = 0; n < (INT)N; n++ )
        {
            d[ n ] /= Z;
        }
    }

    dwStop = GetTickCount();

    if ( bFinalPass )
    {
        printf( "Done" );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ByteSwapSkeletonData
// Desc: Byteswamp skeleton data so that it can be used on PC
//--------------------------------------------------------------------------------------

VOID GestureDetectorTrainer::ByteSwapSkeletonData( NUI_SKELETON_DATA* pSkeletonData )
{
    pSkeletonData->eTrackingState = ByteSwap32Bit( pSkeletonData->eTrackingState );
    pSkeletonData->dwTrackingID = ByteSwap32Bit( pSkeletonData->dwTrackingID );
    pSkeletonData->dwEnrollmentIndex = ByteSwap32Bit( pSkeletonData->dwEnrollmentIndex );
    pSkeletonData->dwUserIndex = ByteSwap32Bit( pSkeletonData->dwUserIndex );
    pSkeletonData->Position.m128_f32[ 0 ] = ByteSwap32Bit( pSkeletonData->Position.m128_f32[ 0 ] );
    pSkeletonData->Position.m128_f32[ 1 ] = ByteSwap32Bit( pSkeletonData->Position.m128_f32[ 1 ] );
    pSkeletonData->Position.m128_f32[ 2 ] = ByteSwap32Bit( pSkeletonData->Position.m128_f32[ 2 ] );
    pSkeletonData->Position.m128_f32[ 3 ] = ByteSwap32Bit( pSkeletonData->Position.m128_f32[ 3 ] );

    for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
    {
        pSkeletonData->SkeletonPositions[ i ].m128_f32[ 0 ] = ByteSwap32Bit( pSkeletonData->SkeletonPositions[ i ].m128_f32[ 0 ] );
        pSkeletonData->SkeletonPositions[ i ].m128_f32[ 1 ] = ByteSwap32Bit( pSkeletonData->SkeletonPositions[ i ].m128_f32[ 1 ] );
        pSkeletonData->SkeletonPositions[ i ].m128_f32[ 2 ] = ByteSwap32Bit( pSkeletonData->SkeletonPositions[ i ].m128_f32[ 2 ] );
        pSkeletonData->SkeletonPositions[ i ].m128_f32[ 3 ] = ByteSwap32Bit( pSkeletonData->SkeletonPositions[ i ].m128_f32[ 3 ] );
    }

    for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
    {
        pSkeletonData->eSkeletonPositionTrackingState[ i ] = ByteSwap32Bit( pSkeletonData->eSkeletonPositionTrackingState[ i ] );
    }

    pSkeletonData->dwQualityFlags = ByteSwap32Bit( pSkeletonData->dwQualityFlags );
}


//--------------------------------------------------------------------------------------
// Name: SwapPositions
// Desc: Used with MirroSkeletonData(), this simply swaps two vectors
//--------------------------------------------------------------------------------------

static inline VOID SwapPositions( XMVECTOR& vPosition0, XMVECTOR& vPosition1 )
{
    XMVECTOR vTemp = vPosition0;
    vPosition0 = vPosition1;
    vPosition1 = vTemp;
}


//--------------------------------------------------------------------------------------
// Name: SwapTrackingStatus
// Desc: Used with MirroSkeletonData(), this simply swaps two states
//--------------------------------------------------------------------------------------

static inline VOID SwapTrackingStatus( NUI_SKELETON_DATA* pSkeletonData, const NUI_SKELETON_POSITION_INDEX left, const NUI_SKELETON_POSITION_INDEX right )
{
    NUI_SKELETON_POSITION_TRACKING_STATE temp = pSkeletonData->eSkeletonPositionTrackingState[ left ];
    pSkeletonData->eSkeletonPositionTrackingState[ left ] = pSkeletonData->eSkeletonPositionTrackingState[ right ];
    pSkeletonData->eSkeletonPositionTrackingState[ right ] = temp;
}


//--------------------------------------------------------------------------------------
// Name: SwapJointData
// Desc: Used with MirroSkeletonData(), this swaps data from two joints
//--------------------------------------------------------------------------------------

VOID SwapJointData( NUI_SKELETON_DATA* pSkeletonData, const NUI_SKELETON_POSITION_INDEX left, const NUI_SKELETON_POSITION_INDEX right )
{
    SwapPositions( pSkeletonData->SkeletonPositions[ left ], pSkeletonData->SkeletonPositions[ right ] );
    SwapTrackingStatus( pSkeletonData, left, right );
}


//--------------------------------------------------------------------------------------
// Name: MirrorSkeletonData
// Desc: Mirror the skeleton data
//--------------------------------------------------------------------------------------

VOID GestureDetectorTrainer::MirrorSkeletonData( NUI_SKELETON_DATA* pSkeletonData )
{
    SwapJointData( pSkeletonData, NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_SHOULDER_RIGHT );
    SwapJointData( pSkeletonData, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_ELBOW_RIGHT );
    SwapJointData( pSkeletonData, NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_WRIST_RIGHT );
    SwapJointData( pSkeletonData, NUI_SKELETON_POSITION_HAND_LEFT, NUI_SKELETON_POSITION_HAND_RIGHT );
    SwapJointData( pSkeletonData, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_HIP_RIGHT );
    SwapJointData( pSkeletonData, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_KNEE_RIGHT );
    SwapJointData( pSkeletonData, NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_ANKLE_RIGHT );
    SwapJointData( pSkeletonData, NUI_SKELETON_POSITION_FOOT_LEFT, NUI_SKELETON_POSITION_FOOT_RIGHT );

    const XMVECTOR vMirror = XMVectorSet( -1.0f, 1.0f, 1.0f, 1.0f );
    for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
    {
        pSkeletonData->SkeletonPositions[ i ] *= vMirror;
    }
    pSkeletonData->Position *= vMirror;
}

#endif


//--------------------------------------------------------------------------------------
// Name: Print
// Desc: Prints useful debug information
//--------------------------------------------------------------------------------------

CHAR* DebugOutput::Print( const ClassifierData::EType type )
{
    switch( type )
    {
#ifdef ADD_TYPE_DIFF_POSITION_X
    case ClassifierData::TYPE_DIFF_POSITION_X:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "DiffPositionX" );
        break;
#endif

#ifdef ADD_TYPE_DIFF_POSITION_Y
    case ClassifierData::TYPE_DIFF_POSITION_Y:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "DiffPositionY" );
        break;
#endif

#ifdef ADD_TYPE_DIFF_POSITION_Z
    case ClassifierData::TYPE_DIFF_POSITION_Z:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "DiffPositionZ" );
        break;
#endif

#ifdef ADD_TYPE_ANGLE
    case ClassifierData::TYPE_ANGLE:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "Angles" );
        break;
#endif

#ifdef ADD_TYPE_TIME_SPACE_ANGLE
    case ClassifierData::TYPE_TIME_SPACE_ANGLE:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "TimeSpaceAngles" );
        break;
#endif

#ifdef ADD_TYPE_POSITION_SPEED
    case ClassifierData::TYPE_POSITION_SPEED:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "Speed" );
        break;
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION
    case ClassifierData::TYPE_POSITION_ACCELERATION:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "Acceleration" );
        break;
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION_X
    case ClassifierData::TYPE_POSITION_ACCELERATION_X:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "AccelerationX" );
        break;
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION_Y
    case ClassifierData::TYPE_POSITION_ACCELERATION_Y:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "AccelerationY" );
        break;
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION_Z
    case ClassifierData::TYPE_POSITION_ACCELERATION_Z:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "AccelerationZ" );
        break;
#endif

#ifdef ADD_TYPE_POSITION_SPEED_SQ
    case ClassifierData::TYPE_POSITION_SPEED_SQ:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "Speed^2" );
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITY_X
    case ClassifierData::TYPE_POSITION_VELOCITY_X:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "VelocityX" );
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITY_Y
    case ClassifierData::TYPE_POSITION_VELOCITY_Y:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "VelocityY" );
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITY_Z
    case ClassifierData::TYPE_POSITION_VELOCITY_Z:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "VelocityZ" );
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITYSQ_X
    case ClassifierData::TYPE_POSITION_VELOCITYSQ_X:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "VelocityX^2" );
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITYSQ_Y
    case ClassifierData::TYPE_POSITION_VELOCITYSQ_Y:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "VelocityY^2" );
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITYSQ_Z
    case ClassifierData::TYPE_POSITION_VELOCITYSQ_Z:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "VelocityZ^2" );
        break;
#endif

#ifdef ADD_TYPE_ANGLE_VELOCITY
    case ClassifierData::TYPE_ANGLE_VELOCITY:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "AngleVelocity" );
        break;
#endif

#ifdef ADD_TYPE_ANGLE_ACCELERATION
    case ClassifierData::TYPE_ANGLE_ACCELERATION:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "AngleAcceleration" );
        break;
#endif

#ifdef ADD_TYPE_MUSCLE_POWER
    case ClassifierData::TYPE_MUSCLE_POWER:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "MusclePower" );
        break;
#endif

#ifdef ADD_TYPE_MUSCLE_FORCES
    case ClassifierData::TYPE_MUSCLE_FORCE_X:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "MuscleForceX" );
        break;

    case ClassifierData::TYPE_MUSCLE_FORCE_Y:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "MuscleForceY" );
        break;

    case ClassifierData::TYPE_MUSCLE_FORCE_Z:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "MuscleForceZ" );
        break;
#endif

#ifdef ADD_TYPE_MUSCLE_TORQUES
    case ClassifierData::TYPE_MUSCLE_TORQUE_X:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "MuscleTorqueX" );
        break;

    case ClassifierData::TYPE_MUSCLE_TORQUE_Y:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "MuscleTorqueY" );
        break;

    case ClassifierData::TYPE_MUSCLE_TORQUE_Z:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "MuscleTorqueZ" );
        break;
#endif

#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_X
    case ClassifierData::TYPE_DIFF_MUSCLE_FORCE_X:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "DiffMuscleForceX" );
        break;
#endif

#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_Y
    case ClassifierData::TYPE_DIFF_MUSCLE_FORCE_Y:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "DiffMuscleForceY" );
        break;
#endif

#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_Z
    case ClassifierData::TYPE_DIFF_MUSCLE_FORCE_Z:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "DiffMuscleForceZ" );
        break;
#endif

#ifdef ADD_TYPE_BONE_LENGTH_CHANGES
    case ClassifierData::TYPE_BONE_LENGTH_CHANGES:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "BoneLengthChanges" );
        break;
#endif

    default:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "" );
    }
    return m_szBuffer;
}


//--------------------------------------------------------------------------------------
// Name: Print
// Desc: Prints useful debug information
//--------------------------------------------------------------------------------------

CHAR* DebugOutput::Print( const NUI_SKELETON_POSITION_INDEX joint )
{
    switch ( joint )
    {
    case NUI_SKELETON_POSITION_HIP_CENTER:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "HipCenter" );
        break;

    case NUI_SKELETON_POSITION_SPINE:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "Spine" );
        break;

    case NUI_SKELETON_POSITION_SHOULDER_CENTER:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "ShoulderCenter" );
        break;

    case NUI_SKELETON_POSITION_HEAD:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "Head" );
        break;

    case NUI_SKELETON_POSITION_SHOULDER_LEFT:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "ShoulderLeft" );
        break;

    case NUI_SKELETON_POSITION_ELBOW_LEFT:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "ElbowLeft" );
        break;

    case NUI_SKELETON_POSITION_WRIST_LEFT:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "WristLeft" );
        break;

    case NUI_SKELETON_POSITION_HAND_LEFT:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "HandLeft" );
        break;

    case NUI_SKELETON_POSITION_SHOULDER_RIGHT:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "ShoulderRight" );
        break;

    case NUI_SKELETON_POSITION_ELBOW_RIGHT:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "ElbowRight" );
        break;

    case NUI_SKELETON_POSITION_WRIST_RIGHT:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "WristRight" );
        break;

    case NUI_SKELETON_POSITION_HAND_RIGHT:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "HandRight" );
        break;

    case NUI_SKELETON_POSITION_HIP_LEFT:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "HipLeft" );
        break;

    case NUI_SKELETON_POSITION_KNEE_LEFT:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "KneeLeft" );
        break;

    case NUI_SKELETON_POSITION_ANKLE_LEFT:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "AnkleLeft" );
        break;

    case NUI_SKELETON_POSITION_FOOT_LEFT:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "FootLeft" );
        break;

    case NUI_SKELETON_POSITION_HIP_RIGHT:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "HipRight" );
        break;

    case NUI_SKELETON_POSITION_KNEE_RIGHT:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "KneeRight" );
        break;

    case NUI_SKELETON_POSITION_ANKLE_RIGHT:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "AnkleRight" );
        break;

    case NUI_SKELETON_POSITION_FOOT_RIGHT:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "FootRight" );
        break;

    default:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "" );
    }

    return m_szBuffer;
}


//--------------------------------------------------------------------------------------
// Name: Print
// Desc: Prints useful debug information
//--------------------------------------------------------------------------------------

CHAR* DebugOutput::Print( const ClassifierData* pClassifierData )
{
    assert( pClassifierData );

    CHAR szType[ MAX_PATH ];
    CHAR szJoint0[  MAX_PATH ];
    CHAR szJoint1[  MAX_PATH ];
    CHAR szJoint2[  MAX_PATH ];

    switch( pClassifierData->m_Type )
    {
#ifdef ADD_TYPE_DIFF_POSITION_X
    case ClassifierData::TYPE_DIFF_POSITION_X:
        {
            ClassifierDataUsingDiffPositionX* pData = (ClassifierDataUsingDiffPositionX*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndices[ 0 ] ) );
            strcpy_s( szJoint1, sizeof( szType ), Print( pData->m_jointIndices[ 1 ] ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s, %s ) %s inferred joints", szType, szJoint0, szJoint1, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_DIFF_POSITION_Y
    case ClassifierData::TYPE_DIFF_POSITION_Y:
        {
            ClassifierDataUsingDiffPositionY* pData = (ClassifierDataUsingDiffPositionY*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndices[ 0 ] ) );
            strcpy_s( szJoint1, sizeof( szType ), Print( pData->m_jointIndices[ 1 ] ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s, %s ) %s inferred joints", szType, szJoint0, szJoint1, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_DIFF_POSITION_Z
    case ClassifierData::TYPE_DIFF_POSITION_Z:
        {
            ClassifierDataUsingDiffPositionZ* pData = (ClassifierDataUsingDiffPositionZ*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndices[ 0 ] ) );
            strcpy_s( szJoint1, sizeof( szType ), Print( pData->m_jointIndices[ 1 ] ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s, %s ) %s inferred joints", szType, szJoint0, szJoint1, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_ANGLE
    case ClassifierData::TYPE_ANGLE:
        {
            ClassifierDataUsingAngles* pData = (ClassifierDataUsingAngles*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndices[ 0 ] ) );
            strcpy_s( szJoint1, sizeof( szType ), Print( pData->m_jointIndices[ 1 ] ) );
            strcpy_s( szJoint2, sizeof( szType ), Print( pData->m_jointIndices[ 2 ] ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s, %s, %s ) %s inferred joints", szType, szJoint0, szJoint1, szJoint2, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_TIME_SPACE_ANGLE
    case ClassifierData::TYPE_TIME_SPACE_ANGLE:
        {
            ClassifierDataUsingTimeSpaceAngles* pData = (ClassifierDataUsingTimeSpaceAngles*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_POSITION_SPEED
    case ClassifierData::TYPE_POSITION_SPEED:
        {
            ClassifierDataUsingPositionSpeed* pData = (ClassifierDataUsingPositionSpeed*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION
    case ClassifierData::TYPE_POSITION_ACCELERATION:
        {
            ClassifierDataUsingPositionAcceleration* pData = (ClassifierDataUsingPositionAcceleration*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION_X
    case ClassifierData::TYPE_POSITION_ACCELERATION_X:
        {
            ClassifierDataUsingPositionAccelerationX* pData = (ClassifierDataUsingPositionAccelerationX*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION_Y
    case ClassifierData::TYPE_POSITION_ACCELERATION_Y:
        {
            ClassifierDataUsingPositionAccelerationY* pData = (ClassifierDataUsingPositionAccelerationY*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION_Z
    case ClassifierData::TYPE_POSITION_ACCELERATION_Z:
        {
            ClassifierDataUsingPositionAccelerationZ* pData = (ClassifierDataUsingPositionAccelerationZ*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_POSITION_SPEED_SQ
    case ClassifierData::TYPE_POSITION_SPEED_SQ:
        {
            ClassifierDataUsingPositionSpeedSQ* pData = (ClassifierDataUsingPositionSpeedSQ*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITY_X
    case ClassifierData::TYPE_POSITION_VELOCITY_X:
        {
            ClassifierDataUsingPositionVelocityX* pData = (ClassifierDataUsingPositionVelocityX*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITY_Y
    case ClassifierData::TYPE_POSITION_VELOCITY_Y:
        {
            ClassifierDataUsingPositionVelocityY* pData = (ClassifierDataUsingPositionVelocityY*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITY_Z
        case ClassifierData::TYPE_POSITION_VELOCITY_Z:
        {
            ClassifierDataUsingPositionVelocityZ* pData = (ClassifierDataUsingPositionVelocityZ*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITYSQ_X
    case ClassifierData::TYPE_POSITION_VELOCITYSQ_X:
        {
            ClassifierDataUsingPositionVelocitySQX* pData = (ClassifierDataUsingPositionVelocitySQX*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITYSQ_Y
    case ClassifierData::TYPE_POSITION_VELOCITYSQ_Y:
        {
            ClassifierDataUsingPositionVelocitySQY* pData = (ClassifierDataUsingPositionVelocitySQY*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITYSQ_Z
        case ClassifierData::TYPE_POSITION_VELOCITYSQ_Z:
        {
            ClassifierDataUsingPositionVelocityZ* pData = (ClassifierDataUsingPositionVelocityZ*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_ANGLE_VELOCITY
    case ClassifierData::TYPE_ANGLE_VELOCITY:
        {
            ClassifierDataUsingAngleVelocities* pData = (ClassifierDataUsingAngleVelocities*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndices[ 0 ] ) );
            strcpy_s( szJoint1, sizeof( szType ), Print( pData->m_jointIndices[ 1 ] ) );
            strcpy_s( szJoint2, sizeof( szType ), Print( pData->m_jointIndices[ 2 ] ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s, %s, %s ) %s inferred joints", szType, szJoint0, szJoint1, szJoint2, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_ANGLE_ACCELERATION
    case ClassifierData::TYPE_ANGLE_ACCELERATION:
        {
            ClassifierDataUsingAngleAcceleration* pData = (ClassifierDataUsingAngleAcceleration*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndices[ 0 ] ) );
            strcpy_s( szJoint1, sizeof( szType ), Print( pData->m_jointIndices[ 1 ] ) );
            strcpy_s( szJoint2, sizeof( szType ), Print( pData->m_jointIndices[ 2 ] ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s, %s, %s ) %s inferred joints", szType, szJoint0, szJoint1, szJoint2, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_MUSCLE_POWER
    case ClassifierData::TYPE_MUSCLE_POWER:
        {
            ClassifierDataUsingMusclePower* pData = (ClassifierDataUsingMusclePower*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_MUSCLE_FORCES
    case ClassifierData::TYPE_MUSCLE_FORCE_X:
        {
            ClassifierDataUsingMuscleForceX* pData = (ClassifierDataUsingMuscleForceX*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;

    case ClassifierData::TYPE_MUSCLE_FORCE_Y:
        {
            ClassifierDataUsingMuscleForceY* pData = (ClassifierDataUsingMuscleForceY*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;

    case ClassifierData::TYPE_MUSCLE_FORCE_Z:
        {
            ClassifierDataUsingMuscleForceZ* pData = (ClassifierDataUsingMuscleForceZ*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_MUSCLE_TORQUES
    case ClassifierData::TYPE_MUSCLE_TORQUE_X:
        {
            ClassifierDataUsingMuscleTorqueX* pData = (ClassifierDataUsingMuscleTorqueX*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;

    case ClassifierData::TYPE_MUSCLE_TORQUE_Y:
        {
            ClassifierDataUsingMuscleTorqueY* pData = (ClassifierDataUsingMuscleTorqueY*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;

    case ClassifierData::TYPE_MUSCLE_TORQUE_Z:
        {
            ClassifierDataUsingMuscleTorqueZ* pData = (ClassifierDataUsingMuscleTorqueZ*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndex ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s ) %s inferred joints", szType, szJoint0, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_X
    case ClassifierData::TYPE_DIFF_MUSCLE_FORCE_X:
        {
            ClassifierDataUsingDiffMuscleForceX* pData = (ClassifierDataUsingDiffMuscleForceX*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndices[ 0 ] ) );
            strcpy_s( szJoint1, sizeof( szType ), Print( pData->m_jointIndices[ 1 ] ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s, %s ) %s inferred joints", szType, szJoint0, szJoint1, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_Y
    case ClassifierData::TYPE_DIFF_MUSCLE_FORCE_Y:
        {
            ClassifierDataUsingDiffMuscleForceY* pData = (ClassifierDataUsingDiffMuscleForceY*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndices[ 0 ] ) );
            strcpy_s( szJoint1, sizeof( szType ), Print( pData->m_jointIndices[ 1 ] ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s, %s ) %s inferred joints", szType, szJoint0, szJoint1, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_Z
    case ClassifierData::TYPE_DIFF_MUSCLE_FORCE_Z:
        {
            ClassifierDataUsingDiffMuscleForceZ* pData = (ClassifierDataUsingDiffMuscleForceZ*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndices[ 0 ] ) );
            strcpy_s( szJoint1, sizeof( szType ), Print( pData->m_jointIndices[ 1 ] ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s, %s ) %s inferred joints", szType, szJoint0, szJoint1, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

#ifdef ADD_TYPE_BONE_LENGTH_CHANGES
    case ClassifierData::TYPE_BONE_LENGTH_CHANGES:
        {
            ClassifierDataUsingBoneLengthChanges* pData = (ClassifierDataUsingBoneLengthChanges*)pClassifierData;
            strcpy_s( szType, sizeof( szType ), Print( pData->m_Type ) );
            strcpy_s( szJoint0, sizeof( szType ), Print( pData->m_jointIndices[ 0 ] ) );
            strcpy_s( szJoint1, sizeof( szType ), Print( pData->m_jointIndices[ 1 ] ) );
            sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s( %s, %s ) %s inferred joints", szType, szJoint0, szJoint1, pData->m_bRejectInferred ? "rejecting" : "using" );
        }
        break;
#endif

    default:
        sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "" );
    }

    return m_szBuffer;
}


//--------------------------------------------------------------------------------------
// Name: Print
// Desc: Prints useful debug information
//--------------------------------------------------------------------------------------

CHAR* DebugOutput::Print( const WeakClassifier* pWeakClassifier )
{
    assert( pWeakClassifier );

    CHAR szBuffer[ MAX_PATH ];

#ifdef _XBOX
    BOOL bReverse = ( pWeakClassifier->m_fAlpha < 0.0f );
#else
    BOOL bReverse = FALSE;
    if ( pWeakClassifier->m_iReverse < 0 ||
         pWeakClassifier->m_fAlpha < 0.0f )
    {
        bReverse = TRUE;
    }
#endif
    sprintf_s( szBuffer, sizeof( szBuffer ), "%s, %s %f, alpha = %f", Print( pWeakClassifier->m_pData ),
                bReverse ? "fValue <" : "fValue >=", pWeakClassifier->m_fThreshold, fabs( pWeakClassifier->m_fAlpha ) );

    sprintf_s( m_szBuffer, sizeof( m_szBuffer ), "%s", szBuffer );

    return m_szBuffer;
}


//--------------------------------------------------------------------------------------
// Name: Print
// Desc: Prints useful debug information
//--------------------------------------------------------------------------------------

CHAR* DebugOutput::Print( GestureDetector* pGestureDetector, const UINT uWeakClassiferIndex )
{
    assert( pGestureDetector );

    if ( uWeakClassiferIndex < pGestureDetector->m_StrongClassifier.GetNumWeakClassifiers() )
    {
        WeakClassifier* pWeakClassifier = pGestureDetector->GetWeakClassifierAt( uWeakClassiferIndex );
        Print( pWeakClassifier );
    }
    else
    {
        m_szBuffer[ 0 ] = '\0';
    }

    return m_szBuffer;
}


//--------------------------------------------------------------------------------------
// Name: mopen
// Desc: Opens memory pointer for reading from memory
//--------------------------------------------------------------------------------------

static UINT g_uOffset = 0;

HRESULT mopen( VOID* pSource )
{
    g_uOffset = 0;
    return pSource ? S_OK : E_FAIL;
}


//--------------------------------------------------------------------------------------
// Name: mread
// Desc: Reads from memory
//--------------------------------------------------------------------------------------

size_t mread( VOID* pDest, size_t elementSize, size_t count, VOID* pSource )
{
    UINT uOffset = (UINT)(elementSize * count);
    memcpy( pDest, (VOID*)((UINT)pSource + g_uOffset), uOffset );
    g_uOffset += uOffset;
    return g_uOffset;
}


//--------------------------------------------------------------------------------------
// Name: mclose
// Desc: Close memory reads
//--------------------------------------------------------------------------------------

VOID mclose()
{
    g_uOffset = 0;
}

}