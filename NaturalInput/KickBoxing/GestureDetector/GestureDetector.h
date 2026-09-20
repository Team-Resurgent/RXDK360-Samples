//--------------------------------------------------------------------------------------
// GestureDetector.h
//
// Definitions for the gesture detector and gesture detector trainer, as well definitions
// for weak and strong classifiers. The gesture detector trainer uses the AdaBoost
// learning alogorith.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifdef _XBOX
#include <xtl.h>
#else
#include <windows.h>
#endif

#include <vector>
#include <deque>
#include <stdio.h>
#include <XnaMath.h>

#ifdef _XBOX
#include <nuiapi.h>
#else
#include <NuiTools.h>
#endif

#include <XStudio.h>

#include "Common.h"
#include "ClassifierData.h"

namespace ATGGestureDetector
{

//--------------------------------------------------------------------------------------
// Defines and constants
//--------------------------------------------------------------------------------------

// For optimization reasons we use _fsel() on XBox, which uses double's, but on PC we
// want to make sure there are no floating point errors when comparing 1 and -1, so we 
// simply use int
#ifdef _XBOX
static const DOUBLE g_fClassificationLabelCorrect   = 1.0f;
static const DOUBLE g_fClassificationLabelIncorrect = -1.0f;
#else
static const INT8 g_iClassificationLabelCorrect     = 1;
static const INT8 g_iClassificationLabelIncorrect   = -1;
#endif

// Version history
//static const FLOAT g_fCurrentVersion          = 1.0f;   // First version
//static const FLOAT g_fCurrentVersion          = 1.1f;   // Added LoadFromMemory() method, and alloc skeleton data as 16 byte aligned
//static const FLOAT g_fCurrentVersion          = 1.2f;   // Minor bug fixes, adding namespace and improvements to automatically finding filtering parameters
static const FLOAT g_fCurrentVersion            = 1.3f;   // Add hand and feet joints to some of the classifiers

static const CHAR g_szGestureFileID[]           = "GestureDetectorFile";    // Simple header for a gesture file
static const CHAR g_szLabeledExampleFileID[]    = "LabeledExampleFile";     // Simple header for a labeled example file
static const CHAR g_szTraining[]                = "Training";
static const CHAR g_szTesting[]                 = "Testing";


//--------------------------------------------------------------------------------------
// Name: DecisionStump
// Desc: A simple decision stump from which weak classifiers are derived
//--------------------------------------------------------------------------------------

class DecisionStump
{
public:
    DecisionStump();
    inline VOID SetThreshold( const FLOAT fThreshold ) { m_fThreshold = fThreshold; }
    inline FLOAT GetThreshold() const { return m_fThreshold; }

#ifndef _XBOX
    inline VOID SetIsReversed( const BOOL bReverse ) { m_iReverse = bReverse ? -1 : 1; }
    inline BOOL GetIsReversed() const { return ( m_iReverse == -1 ); }
    inline INT8 GetReverseValue() const { return m_iReverse; }
#endif

    // For optimization reason, we return a float value which can be multiplied by the floating point weight value. If we
    // used integer as on PC, we introduce a LHS to for the integeter to float conversion. Also, on Xbox we bake m_iReverse
    // into m_fAlpha, and we can use __fsel intrinsic
#ifdef _XBOX
    __forceinline FLOAT Classify( const FLOAT fValue ) const
    {
        return (FLOAT)__fsel( fValue - m_fThreshold, g_fClassificationLabelCorrect, g_fClassificationLabelIncorrect );
    }
#else
    __forceinline INT8 Classify( const FLOAT fValue ) const
    {
        return ( ( ( fValue - m_fThreshold ) >= 0.0f ) ? g_iClassificationLabelCorrect : g_iClassificationLabelIncorrect ) * m_iReverse;
    }
#endif

    HRESULT Read( FILE* pFile );
    HRESULT Read( VOID* pBuffer );
    HRESULT Write( FILE* pFile );

protected:
    FLOAT   m_fThreshold;       // The classifier threshold value
#ifndef _XBOX
    INT8    m_iReverse;         // To reverse the classification we can multiply the result by -1
#endif
};


//--------------------------------------------------------------------------------------
// Name: WeakClassifier
// Desc: A weak classifier implemented as h(x), with x as example input data, and
//       h(x) returning either +1 or -1 
//--------------------------------------------------------------------------------------

class ClassifierData;
class WeakClassifier : public DecisionStump
{
public:
    WeakClassifier();

    HRESULT Read( FILE* pFile, UINT* pDataID );
    HRESULT Read( VOID* pBuffer, UINT* pDataID );
    HRESULT Write( FILE* pFile );

    inline VOID SetAlpha( const FLOAT fAlpha ) { m_fAlpha = fAlpha; }
    inline FLOAT GetAlpha() const { return m_fAlpha; }

    inline VOID SetData( ClassifierData* pData ) { m_pData = pData; }
    inline ClassifierData* GetData() const { return m_pData; }

    // Sort based on fAlpha, the confidence of the classifier
    inline bool operator < (const WeakClassifier& weakClassifier ) { return ( m_fAlpha < weakClassifier.m_fAlpha ); }

    WeakClassifier& operator = ( const WeakClassifier& source )
    {
        SetThreshold( source.GetThreshold() );
#ifndef _XBOX
        SetIsReversed( source.GetIsReversed() );
#endif
        SetAlpha( source.GetAlpha() );
        SetData( source.GetData() );
        return *this;
    }

protected:
    FLOAT           m_fAlpha;   // Confidence of weak classifier
    ClassifierData* m_pData;    // Pointer to values that will be compared to thesholds

    friend class DebugOutput;
};


//--------------------------------------------------------------------------------------
// Name: StrongClassifier
// Desc: A strong classifier implemented as H(x) as the weighted sum of the weak 
//       classifiers h(x)
//--------------------------------------------------------------------------------------

class StrongClassifier
{
public:
    struct Results
    {
        FLOAT   m_fConfidence;          // The confidence calculated from the normalized weighted sum
        BOOL    m_bDetected;            // Detected a gesture
        BOOL    m_bFirstFrameDetected;  // Detection can occur over several frames. If it's the first frame, this value is true
    };

public:
    StrongClassifier();
    ~StrongClassifier();

    HRESULT Initialize( const UINT nNumWeakClassifiers );

    inline VOID Reset( const UINT uPlayerIdx ) { m_fClassificationHistory[ uPlayerIdx ].clear(); }

    HRESULT Read( FILE* pFile );
    HRESULT Read( VOID* pBuffer );
    HRESULT Write( FILE* pFile );

    HRESULT Read( FILE* pFile, const UINT uWeakClassifierIndex, UINT* pDataID );
    HRESULT Read( VOID* pBuffer, const UINT uWeakClassifierIndex, UINT* pDataID );
    HRESULT Write( FILE* pFile, const UINT uWeakClassifierIndex );

#ifdef _XBOX
    inline UINT GetNumWeakClassifiers() const { return m_nNumWeakClassifiers; }
    BOOL Detect( const UINT uPlayerIdx, ClassifierData** __restrict classifierData, Results* pResults, const BOOL bFilterResults = TRUE );
#else
    inline UINT GetNumWeakClassifiers() const { return (UINT)m_WeakClassifiers.size(); }
    BOOL Detect( const UINT uPlayerIdx, const std::vector<ClassifierData*>& classifierData, Results* pResults, const BOOL bFilterResults = TRUE );
    inline VOID Add( const WeakClassifier weakClassifier ) { m_WeakClassifiers.push_back( weakClassifier ); }
#endif

    VOID FilterDetectionResults( const UINT uPlayerIdx, const FLOAT fConfidence, Results* pResults );

    inline WeakClassifier* GetWeakClassifierAt( const UINT uWeakClassifierIndex ) { return &m_WeakClassifiers[ uWeakClassifierIndex ]; }
    inline VOID SetTotalAlpha( const FLOAT fTotalAlpha ) { m_fTotalAlpha = fTotalAlpha; }
    inline VOID SetDetectionThreshold( const FLOAT fThreshold ) { m_fFilterPerFrameResultsThreshold = fThreshold; }
    inline FLOAT GetDetectionThreshold() const { return m_fFilterPerFrameResultsThreshold; }
    inline VOID SetNumFramesToFilter( const UINT nNumFramesToFilter ) { m_nFilterPerFrameResultsNumFrames = nNumFramesToFilter; }
    inline UINT GetNumFramesToFilter() const { return m_nFilterPerFrameResultsNumFrames; }

#ifndef _XBOX
    VOID Optimize( const UINT nMaxNumClassifers, const BOOL bBakeReverseInAlpha = TRUE );      
#endif

protected:
#ifdef _XBOX
    WeakClassifier*         m_WeakClassifiers;
    UINT                    m_nNumWeakClassifiers;
#else
    std::vector<WeakClassifier>  m_WeakClassifiers;
#endif
    std::deque<FLOAT>       m_fClassificationHistory[ NUM_PLAYERS ];
    FLOAT                   m_fTotalAlpha;                      // Sum of all alpha values of all valid weak classifiers
    FLOAT                   m_fFilterPerFrameResultsThreshold;  // Per frame filtering detection threshold
    UINT                    m_nFilterPerFrameResultsNumFrames;  // Size of the sliding window when filtering per frame results

#ifdef _XBOX
    INT Classify( const UINT uPlayerIdx, ClassifierData** __restrict classifierData, FLOAT* pConfidence );
#else
    INT8 Classify( const UINT uPlayerIdx, const std::vector<ClassifierData*>& classifierData, FLOAT* pConfidence );
#endif

    friend class DebugOutput;
};


//--------------------------------------------------------------------------------------
// Name: GestureDetector
// Desc: The gesture detector that can be run on the PC or Xbox
//--------------------------------------------------------------------------------------

class GestureDetector
{
public:
    typedef StrongClassifier::Results Results;

public:
    GestureDetector();
    ~GestureDetector();

    HRESULT Load( const CHAR* szFileName );
    HRESULT LoadFromMemory( VOID* pBuffer );

    static VOID Update( const UINT uPlayerIdx, const NUI_SKELETON_DATA* pSkeletonData, const LARGE_INTEGER& liTimeStampFromNuiFrame, XMVECTOR* pNormalToGravity = NULL );
    BOOL Detect( const UINT uPlayerIdx, Results* pResults, const BOOL bFilterResults = TRUE );

    inline VOID SetDetectionThreshold( const FLOAT fThreshold ) { m_StrongClassifier.SetDetectionThreshold( fThreshold ); }
    inline FLOAT GetDetectionThreshold() const { return m_StrongClassifier.GetDetectionThreshold(); }
    inline VOID SetNumFramesToFilter( const UINT nNumFramesToFilter ) { m_StrongClassifier.SetNumFramesToFilter( nNumFramesToFilter ); }
    inline UINT GetNumFramesToFilter() const { return m_StrongClassifier.GetNumFramesToFilter(); }

protected:
    StrongClassifier        m_StrongClassifier;

#ifdef _XBOX
    static ClassifierData** m_ClassifierData;
    static UINT             m_nNumClassifierData;
    static BYTE*            m_pClassifierDataUsed;
#else
    static std::vector<ClassifierData*> m_ClassifierData;
#endif
    static UINT             m_nNumInstances;
    static UINT64           m_uPreviousTimeStamp;

    HRESULT Initialize( const UINT nNumWeakClassifiers, const UINT nNumClassifierData );
    VOID FreeClassifierData();

    WeakClassifier* GetWeakClassifierAt( const UINT uWeakClassiferIndex ) { return m_StrongClassifier.GetWeakClassifierAt( uWeakClassiferIndex ); }

    static VOID ApplyTiltCorrection( const UINT uPlayerIdx, NUI_SKELETON_DATA* pDstSkeleton, const NUI_SKELETON_DATA* pSrcSkeleton, XMVECTOR* pNormalToGravity, const BOOL bReset );

    friend class DebugOutput;
};


#ifndef _XBOX

//--------------------------------------------------------------------------------------
// Name: GestureDetectorTrainer
// Desc: The gesture detector trainer that uses the AdaBoost machine learning algorithm
//--------------------------------------------------------------------------------------

class GestureDetectorTrainer : private GestureDetector
{
public:
    struct LabeledExamples
    {
        std::vector<NUI_SKELETON_DATA*> m_pExamples;        // Each tracked skeleton is an example
        std::vector<INT8>               m_iLabels;          // 1 for true, -1 for false
        std::vector<UINT64>             m_uTimeStamps;      // Event time stamp from the XedFile
    };

    static const UINT  m_uTitleDataBufferSize = 256;    // Max tag size from the XedFileTagger sample
    static const CHAR* m_szTagID;                       // "Tag:" string from XedFileTagger sample

public:
    GestureDetectorTrainer();
    ~GestureDetectorTrainer();

    VOID Reset();

    HRESULT Save( const CHAR* szFileName );
    HRESULT Load( const CHAR* szFileName ) { return GestureDetector::Load( szFileName ); }
    HRESULT LoadFromMemory( VOID* pBuffer ) { return GestureDetector::LoadFromMemory( pBuffer ); }

    HRESULT Train( const CHAR* szPath, const DOUBLE fAccuracyLevel, const UINT nNumWeakClassifiersAtRuntime, const FLOAT fWeightOfFalsePositivesWhenFiltering, const BOOL bOnlyRetrainDetectionParameters, const UINT uCPUAvailableForTraining );
    HRESULT Test( const CHAR* szPath, const BOOL bTestOnTrainingData );

    HRESULT GenerateLabeledExamples( const CHAR* szGestureName, std::vector<std::wstring>& szXedFiles, const CHAR* szDestPath, const BOOL bUseRawSkeletonData );
   
    UINT GetNumWeakClassifiers() { return m_StrongClassifier.GetNumWeakClassifiers(); }
    UINT GetNumExamples() { return (UINT)m_LabeledExamples.m_pExamples.size(); }
    UINT GetNumPositiveExamples();
    UINT GetNumNegativeExamples() { return GetNumExamples() - GetNumPositiveExamples(); }
    UINT GetNumGestures() const { return m_nTotalNumGestures; }
    UINT GetNumTrainingGestures() const { return m_nNumTrainingGestures; }
    INT8 GetLabel( const UINT uIndex ) const { return m_LabeledExamples.m_iLabels[ uIndex ]; }

protected:
    LabeledExamples m_LabeledExamples;              // Labeled examples used as ground truth during training and testing
    UINT            m_nTotalNumGestures;            // Total number of gestures
    UINT            m_nNumTrainingGestures;         // Total number of gestures which we're training on
    UINT            m_nNumThreadsForTraining;       // Total number of threads the user allows for training
    UINT            m_nMaxNumThreads;               // Total threads available on PC
    UINT            m_nNumWeakClassifiersAtRuntime; // Total number of weak classifiers at runtime
    DOUBLE          m_fErrorThreshold;              // Error threshold in AdaBoost, with max value 0.5, since weak classifiers need to be better that 50/50 chance

    HRESULT GenerateLabeledExamplesFromRawSkeleton( const CHAR* szGestureName, const CHAR* szXedFileName, const BOOL bMirrorData );
    HRESULT GenerateLabeledExamplesFromFilteredSkeleton( const CHAR* szGestureName, const CHAR* szXedFileName, const BOOL bMirrorData );   
    HRESULT SaveLabeledExamples( const CHAR* szFileName );
    HRESULT LoadLabeledExamples( const CHAR* szFileName );
    
    HRESULT GenerateWeakClassifiers( ClassifierData::EType classifierDataType = ClassifierData::NUM_FEATURES );
    HRESULT GenerateWeakClassifiers( ClassifierData* pClassifierData, const FLOAT fMin, const FLOAT fMax, const FLOAT fStep );
    VOID CombineWeakClassifiers( GestureDetectorTrainer* pGestureDetectorTrainers, const UINT nNumTrainers );

    HRESULT TrainWeakClassifiers();
    HRESULT TrainWeakClassifiers( ClassifierData::EType classifierDataType, LabeledExamples& labeledExamples, const DOUBLE fErrorThreshold );
    HRESULT TrainStrongClassifier( const BOOL bFinalPass );

    VOID Optimize( const UINT nMaxNumClassifers, const BOOL bBakeReverseInAlpha = TRUE ) { m_StrongClassifier.Optimize( nMaxNumClassifers, bBakeReverseInAlpha ); }
    VOID OptimizeDetectionParameters( const FLOAT fWeightOfFalsePositivesWhenFiltering );
    VOID Test( FLOAT* pTruePositives, FLOAT* pFalsePositives, std::vector<FLOAT>& fRawClassificationResults );

    VOID ByteSwapSkeletonData( NUI_SKELETON_DATA* pSkeletonData );
    VOID MirrorSkeletonData( NUI_SKELETON_DATA* pSkeletonData );
};
#endif


//--------------------------------------------------------------------------------------
// Name: DebugOutput
// Desc: Simple class that outputs text information for debuggin purposes and
//       knowledge extraction puroposes
//--------------------------------------------------------------------------------------

class DebugOutput
{
public:
    CHAR* Print( const ClassifierData::EType type );
    CHAR* Print( const NUI_SKELETON_POSITION_INDEX joint );
    CHAR* Print( const ClassifierData* pClassifierData );
    CHAR* Print( const WeakClassifier* pWeakClassifier );
    CHAR* Print( GestureDetector* pGestureDetector, const UINT uWeakClassiferIndex );

protected:
    CHAR m_szBuffer[ MAX_PATH ];
};

}