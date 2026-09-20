//--------------------------------------------------------------------------------------
// HOCDetector.h
//
// Hand open/closed detector. Shared between the trainer and runtime.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#define NOMINMAX

#ifdef _XBOX
#include <xtl.h>
#include <XnaMath.h>
#include <NuiApi.h>
#else
#include <windows.h>
#include <XnaMath.h>
#include <NuiTools.h>
#include <d3d9.h>
#endif

#include <sal.h>
#include <vector>
#include <FLOAT.h>
#include <math.h>
#include <assert.h>


// unnamed union/struct
#pragma warning( push )
#pragma warning( disable : 4201 )


class HOCDetector;
class HOCStrongClassifier;
struct HOCExtraClassifierData;

// for PC applications, shouldn't be used on Xbox because it takes up more memory to support 640x480
//#define PC_DEPTH_RESOLUTION

// what size of depth buffer are we expecting? 320x240 for 360, 640x480 for PC
#ifndef PC_DEPTH_RESOLUTION
static const UINT   HOC_DEPTH_SIZE_X = 320;
static const UINT   HOC_DEPTH_SIZE_Y = 240;
#else
static const UINT   HOC_DEPTH_SIZE_X = 640;
static const UINT   HOC_DEPTH_SIZE_Y = 480;
#endif

//--------------------------------------------------------------------------------------
// Name: Voxel
// Desc: a hand voxel, packed in memory to 4 bytes or to 8 bytes, depending on the
// depth buffer resolution
//--------------------------------------------------------------------------------------
#ifdef PC_DEPTH_RESOLUTION
struct Voxel
{
    USHORT  x;
    USHORT  y;
    USHORT  z;
    USHORT  extra;
};
#else
struct Voxel
{
    USHORT  x   :   9;      // 0..320
    USHORT  y   :   8;      // 0..240
    USHORT  z   :   13;     // 0..4096
};
#endif

//--------------------------------------------------------------------------------------
// Name: HOCComputedData
// Desc: Runtime data we build from HOCSourceData to feed to the clasifiers
//--------------------------------------------------------------------------------------
struct HOCComputedData
{
    static const UINT   NUM_BUCKETS = 16;           // number of buckets in the distance histograms
    static const UINT   NUM_CONTOUR_POINTS = 64;    // number of points in the contour

    // histogram related data
    // one histogram is for the wrist source point, another one for the centroid
    FLOAT       m_fMean[ 2 ];           // mean of the normalized histogram
    FLOAT       m_fVariance[ 2 ];       // variance of the normalized histogram
    FLOAT       m_fZroundness[ 2 ];     // xy area divided by the depth range
    FLOAT       m_normalizedHistogram[ 2 ][ NUM_BUCKETS ];
    USHORT      m_unnormalizedHistogram[ 2 ][ NUM_BUCKETS ];
    FLOAT       m_integralHistogram[ 2 ][ NUM_BUCKETS ];

    // contour related data
    FLOAT       m_fContourMean;         // where the mean of the contour is
    FLOAT       m_fContourSpread;       // how asymmetric it is
    FLOAT       m_fContourSymmetry;     // how symmetrical it is in xy
    FLOAT       m_fContourExp1;         // experimental values
    FLOAT       m_fContourExp2;         // 
    FLOAT       m_fExperiment1;         // this quantity is proportional to the number of voxels visible
    FLOAT       m_fExperiment2;         // ditto
    FLOAT       m_fExperiment3[ 2 ];    // ratio of lengths between the farthest point on the palm and the center

    // ST-based data
    FLOAT       m_fBoneLengths;         // ratio of the forearm's length to the hand's length

    HOCComputedData()
    {
        m_fMean[ 0 ] = m_fMean[ 1 ] = 0;
        m_fVariance[ 0 ] = m_fVariance[ 1 ] = 0;
        m_fZroundness[ 0 ] = m_fZroundness[ 1 ] = 0;
        m_fContourMean = 0;
        m_fContourSymmetry = 0;
        m_fBoneLengths = 0;
        m_fContourSpread = 0;
        memset( m_normalizedHistogram, 0, sizeof( m_normalizedHistogram ) );
        memset( m_unnormalizedHistogram, 0, sizeof( m_unnormalizedHistogram ) );
        memset( m_integralHistogram, 0, sizeof( m_integralHistogram ) );
    }
};

//--------------------------------------------------------------------------------------
// Name: HOCComputedTransientData
// Desc: this can be used as debug information, you don't really need to carry it around
//--------------------------------------------------------------------------------------
struct HOCComputedTransientData
{
    HOCComputedTransientData()
    {
        memset( m_depthImageBoundingBox, 0, sizeof( m_depthImageBoundingBox ) );

        m_vCentroid.x = m_vCentroid.y = m_vCentroid.z = 0;
        m_vPalmCentre = m_vCentroid;
        m_uIndexToFurthestVoxel[ 0 ] = m_uIndexToFurthestVoxel[ 1 ] = 0;
    }

    FLOAT                   m_contour[ HOCComputedData::NUM_CONTOUR_POINTS ];
    FLOAT                   m_contourSmooth[ HOCComputedData::NUM_CONTOUR_POINTS ];
    std::vector< USHORT >   m_depthImage;
    std::vector< XMFLOAT3 > m_points;
    SHORT                   m_depthImageBoundingBox[ 4 ];     // minX, minY, width, height
    XMFLOAT3                m_vCentroid;
    XMFLOAT3                m_vPalmCentre;
    USHORT                  m_uIndexToFurthestVoxel[ 2 ];
};

//--------------------------------------------------------------------------------------
// Name: HOCSourceData
// Desc: Information we capture for training
//--------------------------------------------------------------------------------------
struct HOCSourceData
{
    static const UINT   MAX_POINTS = 64 * 64;   // when is the hand too large?

    static const USHORT ELBOW_TRACKED = 1;      // these are used in m_bTrackedElbowWrist below
    static const USHORT WRIST_TRACKED = 2;

    XMFLOAT3    m_vHand, m_vWrist, m_vElbow;    // skeleton positions of hand, wrist and elbow
    FLOAT       m_fPlayerSize;                  // shoulder to shoulder distance
    FLOAT       m_fHandSizeAtDistance;          // size of the hand at the distance of the hand joint
    USHORT      m_uNumVoxels;                   // 8192 voxels max
    USHORT      m_bTrackedElbowWrist;           // bit0 -- elbow bit1 -- wrist
    Voxel*      m_pVoxels;

    HOCSourceData() :   m_uNumVoxels( 0 ),
                        m_pVoxels( NULL )
    {
    }

    ~HOCSourceData()
    {
        delete[] m_pVoxels;
    }

    HOCSourceData( const HOCSourceData& rhs ) : m_pVoxels( NULL )
    {
        *this = rhs;
    }

    HOCSourceData& operator = ( const HOCSourceData& rhs )
    {
        if( this == &rhs )
            return *this;

        delete[] m_pVoxels;

        memcpy( this, &rhs, sizeof( rhs ) );

        m_pVoxels = new Voxel[ m_uNumVoxels ];
        memcpy( m_pVoxels, rhs.m_pVoxels, m_uNumVoxels * sizeof( m_pVoxels[ 0 ] ) );

        return *this;
    }

    VOID    Own( HOCSourceData& rhs )
    {
        if( this == &rhs )
            return;

        memcpy( this, &rhs, sizeof( rhs ) );
        memset( &rhs, 0, sizeof( rhs ) );
    }

    VOID    ComputeData( HOCComputedData& hist, _In_opt_ HOCComputedTransientData* pTransientData = NULL ) const;
    HRESULT Load( _In_ FILE* fp );
    HRESULT Save( _In_ FILE* fp ) const;
    VOID    SaveDebugThumbnail( _In_ FILE* fp ) const;

private:
    VOID BuildHistogramRelatedData( UINT uIndex, HOCComputedData& data, HOCComputedTransientData& transientData, const XMVECTOR vCentroid ) const;
    VOID BuildContourRelatedData( HOCComputedData& data, HOCComputedTransientData& transientData, INT sizeX, INT sizeY ) const;
};


//--------------------------------------------------------------------------------------
// Name: HOCDataViews
// Desc: Contains source data we obtain from NUI and computed data for the classifiers
//--------------------------------------------------------------------------------------
struct HOCDataViews
{
    HOCSourceData    m_sourceData;
    HOCComputedData  m_computedData;
    HOCComputedTransientData    m_transientData;    // take this out if you don't need to debug the detector

    HRESULT Load( _In_ FILE* fp );
    HRESULT Save( _In_ FILE* fp ) const;

    VOID    Own( HOCDataViews& dv )
    {
        m_sourceData.Own( dv.m_sourceData );
        m_computedData = dv.m_computedData;
        m_transientData = dv.m_transientData;
    }
};


#ifndef _XBOX
//--------------------------------------------------------------------------------------
// Name: __fsel
// Desc: Make it work on PC, 360 already has this intrinsic
//--------------------------------------------------------------------------------------
__forceinline
FLOAT   __fself( FLOAT v, FLOAT a, FLOAT b )
{
    return v >= 0 ? a : b;
}
#endif


//--------------------------------------------------------------------------------------
// Name: HOCBaseWeakClassifier
// Desc: This is to reduce code duplication between Real AdaBoost classifier and Discrete
//       AdaBoost classifier
//--------------------------------------------------------------------------------------
struct HOCBaseWeakClassifier
{
    enum Type
    {
        TYPE_FIRST_CLASSIFER,

        // level0 -- weak classifiers

        // these 3 are complex classifiers, using extra fields to denote the subtype
        TYPE_REQUIRES_TWO_BUCKETS = TYPE_FIRST_CLASSIFER,
        TYPE_BUCKET_DIFFERENCE_0 = TYPE_REQUIRES_TWO_BUCKETS,
        TYPE_BUCKET_DIFFERENCE_1,

        TYPE_REQUIRES_ONE_BUCKET,
        TYPE_BUCKET_ABS_0 = TYPE_REQUIRES_ONE_BUCKET,
        TYPE_BUCKET_ABS_1,

        TYPE_REQUIRES_ONE_BUCKET_MINUS_ONE,
        TYPE_INTEGRAL_BUCKET_ABS_0 = TYPE_REQUIRES_ONE_BUCKET_MINUS_ONE,
        TYPE_INTEGRAL_BUCKET_ABS_1,

        // these are simple classifiers
        TYPE_FIRST_SIMPLE_CLASSIFER,
        TYPE_REQUIRES_NO_BUCKETS = TYPE_FIRST_SIMPLE_CLASSIFER,
        TYPE_MEAN_0 = TYPE_FIRST_SIMPLE_CLASSIFER,
        TYPE_MEAN_1,
        TYPE_VARIANCE_0,
        TYPE_VARIANCE_1,
        TYPE_Z_ROUNDNESS_0,
        TYPE_Z_ROUNDNESS_1,
        TYPE_CONTOUR_MEAN,
        TYPE_CONTOUR_SPREAD,
        TYPE_CONTOUR_SYMMETRY,
        TYPE_CONTOUR_EXPERIMENT1,
        TYPE_CONTOUR_EXPERIMENT2,
        TYPE_BONE_LENGTHS,
        TYPE_EXPERIMENT1,
        TYPE_EXPERIMENT2,
        TYPE_EXPERIMENT3,
        TYPE_EXPERIMENT4,
        TYPE_LAST_SIMPLE_CLASSIFIER,

        // level1 classifiers use HOCStrongClassifier

        TYPE_NUM_CLASSIFIERS
    };

    // to save memory and increase cache efficiency we make use of all fields. if more data is needed
    // then just indirect into HOCExtraClassifierData
    union
    {
        DWORD   m_dwTypeAndData;

        // used to save to disk as 4 bytes
        BYTE    data[ 4 ];

        // used by level0 classifiers
        // rule outcome should not alias with other fields because it can be used simultaneously
#ifdef _XBOX
#pragma bitfield_order ( push, before, lsb_to_msb )
#endif
        struct
        {
            // 1 byte -- type and reverse
            BYTE    m_type       : 7;           // Type
            BYTE    m_bReverse   : 1;           // Unused in Real classifier

            BYTE    m_bucketIndices[ 2 ];       // indices to compute buckets difference
            CHAR    m_ruleOutcome;              // decision stump positive outcome (-1 or 1) for when used as a rule
                                                // Unused in Real classifier
        };
#ifdef _XBOX
#pragma bitfield_order ( pop, before )
#endif
    };

    FLOAT   m_fAlpha;               // weight

    //--------------------------------------------------------------------------------------
    // Name: operator <
    // Desc: Used to sort based on fAlpha, the confidence of the classifier
    //--------------------------------------------------------------------------------------
    __forceinline
    BOOL operator < ( const HOCBaseWeakClassifier& s ) const
    {
        return ( m_fAlpha < s.m_fAlpha );
    }

    //--------------------------------------------------------------------------------------
    // Name: SetAlpha
    // Desc: Alpha is the weight of the classifier instance
    //--------------------------------------------------------------------------------------
    __forceinline
    VOID    SetAlpha( FLOAT alpha )
    {
        m_fAlpha = alpha;
    }

    //--------------------------------------------------------------------------------------
    // Name: GetAlpha
    // Desc: Alpha is the weight of the classifier instance
    //--------------------------------------------------------------------------------------
    __forceinline
    FLOAT   GetAlpha() const
    {
        return m_fAlpha;
    }

    //--------------------------------------------------------------------------------------
    // Name: IsSameClass
    // Desc: Returns TRUE is two classifer instances are of the same classifier
    //--------------------------------------------------------------------------------------
    BOOL IsSameClass( const HOCBaseWeakClassifier& s ) const;

    //--------------------------------------------------------------------------------------
    // Name: GetDataValue
    // Desc: Returns a floating point value of the training sample in classifier's space
    //--------------------------------------------------------------------------------------
    FLOAT   GetDataValue( const HOCDataViews& d, const HOCExtraClassifierData& e ) const;
};

//--------------------------------------------------------------------------------------
// Name: HOCWeakClassifierR
// Desc: A weak classifier. Should be barely able to tell opened from closed. We create
//       multiple instances of those to form a strong classifier. It's actually a decision
//       stump (one node decision tree) so we use it for implementing rules as well. It's also
//       used as a thunk to a strong classifier, if we boost a set of strong classifiers
//       Real Ada Boost benefits from a better statistically distributed classifier and
//       much faster training times
//--------------------------------------------------------------------------------------
struct HOCWeakClassifierR : HOCBaseWeakClassifier
{
    static const UINT   NUM_RESPONSES = 50;

    FLOAT   m_fMinValue;
    FLOAT   m_fMaxValue;
    FLOAT   m_fResponse[ NUM_RESPONSES ];

    HOCWeakClassifierR()
    {
        m_dwTypeAndData = 0;
        m_fMinValue = 0;
        m_fMaxValue = 0;
        memset( m_fResponse, 0, sizeof( m_fResponse ) );
        m_fAlpha = 0;
    }

    //--------------------------------------------------------------------------------------
    // Name: SetReverse
    // Desc: Sets whether the classifier is doing < or >= thresholding
    //--------------------------------------------------------------------------------------
    __forceinline
    VOID SetReverse( BOOL b )
    {
    }

    //--------------------------------------------------------------------------------------
    // Name: SetThreshold
    // Desc: Set the decision threshold for this decision stump
    //--------------------------------------------------------------------------------------
    __forceinline
    VOID SetThreshold( FLOAT fMin, FLOAT fMax, const FLOAT* fResponse )
    {
        m_fMinValue = fMin;
        m_fMaxValue = fMax;
        memcpy( m_fResponse, fResponse, sizeof( m_fResponse ) );
    }

    //--------------------------------------------------------------------------------------
    // Name: Classify
    // Desc: This a simple decision stump that we're trying to boost
    //--------------------------------------------------------------------------------------
    __forceinline
    INT Classify( FLOAT fValue ) const
    {
        const FLOAT fResponse = ClassifyR( fValue );

        if( fResponse > 0 )
            return 1;

        if( fResponse < 0 )
            return -1;

        return 0;
    }

    //--------------------------------------------------------------------------------------
    // Name: Classify
    // Desc: This a simple decision stump that we're trying to boost
    //--------------------------------------------------------------------------------------
    __forceinline
    FLOAT   Classify( FLOAT fValue, FLOAT fRetValue ) const
    {
        return fRetValue * Classify( fValue );
    }

    //--------------------------------------------------------------------------------------
    __forceinline
    UINT    GetResponseIndex( FLOAT fValue ) const
    {
        assert( fValue >= m_fMinValue && fValue <= m_fMaxValue );

        const FLOAT fBucket = (fValue - m_fMinValue) / (m_fMaxValue - m_fMinValue);
        const UINT  uBucket = (UINT)((_countof( m_fResponse ) - 1) * fBucket);

        return uBucket;
    }

    //--------------------------------------------------------------------------------------
    // Name: Classify
    // Desc: This a simple decision stump that we're trying to boost
    //--------------------------------------------------------------------------------------
    __forceinline
    FLOAT ClassifyR( FLOAT fValue ) const
    {
        if( fValue < m_fMinValue ||
            fValue > m_fMaxValue )
        {
            return 0;       // no idea
        }

        return m_fResponse[ GetResponseIndex( fValue ) ];
    }
};


//--------------------------------------------------------------------------------------
// Name: HOCWeakClassifierD
// Desc: A weak classifier. Should be barely able to tell opened from closed. We create
//       multiple instances of those to form a strong classifier. It's actually a decision
//       stump (one node decision tree) so we use it for implementing rules as well. It's also
//       used as a thunk to a strong classifier, if we boost a set of strong classifiers
//--------------------------------------------------------------------------------------
struct HOCWeakClassifierD : HOCBaseWeakClassifier
{
    FLOAT   m_fThreshold;           // decision threshold

    HOCWeakClassifierD()
    {
        m_dwTypeAndData = 0;
        m_fThreshold = 0;
        m_fAlpha = 0;
    }

    //--------------------------------------------------------------------------------------
    // Name: SetReverse
    // Desc: Sets whether the classifier is doing < or >= thresholding
    //--------------------------------------------------------------------------------------
    __forceinline
    VOID SetReverse( BOOL b )
    {
        m_bReverse = b;
    }

    //--------------------------------------------------------------------------------------
    // Name: SetThreshold
    // Desc: Set the decision threshold for this decision stump
    //--------------------------------------------------------------------------------------
    __forceinline
    VOID SetThreshold( FLOAT fThreshold )
    {
        m_fThreshold = fThreshold;
    }

    //--------------------------------------------------------------------------------------
    // Name: Classify
    // Desc: This a simple decision stump that we're trying to boost
    //--------------------------------------------------------------------------------------
    __forceinline
    INT Classify( FLOAT fValue ) const
    {
        return m_bReverse ? (( ( fValue - m_fThreshold >= 0.0f ) ?  1 : -1 ))    :
                            (( ( fValue - m_fThreshold >= 0.0f ) ? -1 :  1 ));
    }

    //--------------------------------------------------------------------------------------
    // Name: Classify
    // Desc: This a simple decision stump that we're trying to boost
    //--------------------------------------------------------------------------------------
    __forceinline
    FLOAT   Classify( FLOAT fValue, FLOAT fRetValue ) const
    {
        return m_bReverse ? __fself( fValue - m_fThreshold,  fRetValue, -fRetValue )  :
                            __fself( fValue - m_fThreshold, -fRetValue,  fRetValue );
    }

    //--------------------------------------------------------------------------------------
    // Name: Classify
    // Desc: This a simple decision stump that we're trying to boost
    //--------------------------------------------------------------------------------------
    __forceinline
    FLOAT ClassifyR( FLOAT fValue ) const
    {
        return m_bReverse ? (( ( fValue - m_fThreshold >= 0.0f ) ?  1.f : -1.f ))    :
                            (( ( fValue - m_fThreshold >= 0.0f ) ? -1.f :  1.f ));
    }

    //--------------------------------------------------------------------------------------
    // Name: SetRuleToClassIfLessThan
    // Desc: When the decision stump is used for implementing rules instead of boosting
    //       this function set the less-than rule
    //--------------------------------------------------------------------------------------
    __forceinline
    VOID SetRuleToClassIfLessThan( FLOAT fValue, INT iClass )
    {
        m_fThreshold = fValue;
        m_bReverse = (iClass <= 0);
        m_ruleOutcome = static_cast< CHAR >( iClass );
    }

    //--------------------------------------------------------------------------------------
    // Name: SetRuleToClassIfGreaterThan
    // Desc: When the decision stump is used for implementing rules instead of boosting
    //       this function set the less-than rule
    //--------------------------------------------------------------------------------------
    __forceinline
    VOID SetRuleToClassIfGreaterThan( FLOAT fValue, INT iClass )
    {
        m_fThreshold = fValue;
        m_bReverse = (iClass > 0);
        m_ruleOutcome = static_cast< CHAR >( iClass );
    }

    //--------------------------------------------------------------------------------------
    // Name: SimpleOutcome
    // Desc: Checks if the rule trivially accepts or rejects when used as a decision rule
    //--------------------------------------------------------------------------------------
    __forceinline
    BOOL    SimpleOutcome( const HOCDataViews& dv, const HOCExtraClassifierData& e ) const
    {
        const FLOAT fValue = GetDataValue( dv, e );
        const INT iValue = Classify( fValue );

        return iValue == m_ruleOutcome;
    }
};


//--------------------------------------------------------------------------------------
// Name: HOCWeakClassifiersVectorD
// Desc: A list of weak classifiers
//--------------------------------------------------------------------------------------
typedef std::vector< HOCWeakClassifierD >    HOCWeakClassifiersVectorD;
typedef std::vector< HOCWeakClassifierR >    HOCWeakClassifiersVectorR;


//--------------------------------------------------------------------------------------
// Name: HOCStrongClassifier
// Desc: A collection of weak classifiers forms a strong classifier
//--------------------------------------------------------------------------------------
class HOCStrongClassifier
{
    friend class HOCDetectorTrainer;

    HOCWeakClassifiersVectorD   m_listOfRules;          // simple list of rules, optional
    HOCWeakClassifiersVectorD   m_weakClassifiersD;     // weak classifiers with weights
    HOCWeakClassifiersVectorR   m_weakClassifiersR;     // weak classifiers with weights
    FLOAT                       m_fTotalAlpha;          // total weight
    FLOAT                       m_fInvTotalAlpha;       // 1/total weight, not saved
    UINT                        m_uNumVoxels[ 2 ];      // min/max number of voxels used in training
    FLOAT                       m_fDistance[ 2 ];       // min/max distance used in training
    UINT                        m_uNumFilterFrames;     // how many frames to filter
    FLOAT                       m_fFilterThreshold;     // filtering threshold
    BOOL                        m_bUseRealAdaboost;     // using ClassifierD or ClassifierR


#ifdef HOC_TRAINER
    // it is convinient to be able to define a range of indices to work with
    // during training to avoid a copy of thousands of weak classifiers
    BOOL    m_bSubrangeEnabled;
    UINT    m_uSubrangeStart, m_uSubrangeSize;
#endif

public:

#ifdef HOC_TRAINER
    HOCStrongClassifier() : m_bSubrangeEnabled( FALSE ),
                            m_bUseRealAdaboost( FALSE )
    {
    }

    //--------------------------------------------------------------------------------------
    // Name: OptimizeByAlpha
    // Desc: Throws away weak classifiers that don't meet the weight bar
    //--------------------------------------------------------------------------------------
    VOID    OptimizeByAlpha( UINT uMaxClassifiers, FLOAT fAlphaCutoff, UINT uPrintMax );

    //--------------------------------------------------------------------------------------
    // Name: AddWeakClassifier
    // Desc: Adds a real weak classifier to this strong classifier
    //--------------------------------------------------------------------------------------
    __forceinline
    VOID    AddWeakClassifier( const HOCWeakClassifierR& w )
    {
        assert( !m_bSubrangeEnabled );

        m_weakClassifiersR.push_back( w );
    }

    //--------------------------------------------------------------------------------------
    // Name: AddWeakClassifier
    // Desc: Adds a real weak classifier to this strong classifier
    //--------------------------------------------------------------------------------------
    __forceinline
    VOID    AddWeakClassifier( const HOCWeakClassifierD& w )
    {
        assert( !m_bSubrangeEnabled );

        m_weakClassifiersD.push_back( w );
    }

    //--------------------------------------------------------------------------------------
    // Name: GetNumWeakClassifiers
    // Desc: Returns the number of weak classifiers
    //--------------------------------------------------------------------------------------
    __forceinline
    UINT    GetNumWeakClassifiers() const
    {
        return m_bSubrangeEnabled ? m_uSubrangeSize :
                    (m_bUseRealAdaboost ? static_cast< UINT >( m_weakClassifiersR.size() ) :
                                          static_cast< UINT >( m_weakClassifiersD.size() ) );
    }

    //--------------------------------------------------------------------------------------
    // Name: GetBaseWeakClassifier
    // Desc: Returns a weak classifier i
    //--------------------------------------------------------------------------------------
    __forceinline
    HOCBaseWeakClassifier&   GetBaseWeakClassifier( UINT i )
    {
        if( m_bSubrangeEnabled )
            i += m_uSubrangeStart;
        
        if( m_bUseRealAdaboost )
            return m_weakClassifiersR[ i ];

        return m_weakClassifiersD[ i ];
    }

    //--------------------------------------------------------------------------------------
    // Name: GetBaseWeakClassifier
    // Desc: Returns a weak classifier i
    //--------------------------------------------------------------------------------------
    __forceinline
    const HOCBaseWeakClassifier&   GetBaseWeakClassifier( UINT i ) const
    {
        if( m_bSubrangeEnabled )
            i += m_uSubrangeStart;

        if( m_bUseRealAdaboost )
            return m_weakClassifiersR[ i ];

        return m_weakClassifiersD[ i ];
    }

    //--------------------------------------------------------------------------------------
    // Name: GetWeakClassifier
    // Desc: Returns a weak classifier i
    //--------------------------------------------------------------------------------------
    __forceinline
    HOCWeakClassifierR&   GetWeakClassifierR( UINT i )
    {
        if( m_bSubrangeEnabled )
            i += m_uSubrangeStart;

        return m_weakClassifiersR[ i ];
    }

    //--------------------------------------------------------------------------------------
    // Name: GetWeakClassifier
    // Desc: Returns a weak classifier i
    //--------------------------------------------------------------------------------------
    __forceinline
    const HOCWeakClassifierR&   GetWeakClassifierR( UINT i ) const
    {
        if( m_bSubrangeEnabled )
            i += m_uSubrangeStart;

        return m_weakClassifiersR[ i ];
    }

    //--------------------------------------------------------------------------------------
    // Name: GetWeakClassifier
    // Desc: Returns a weak classifier i
    //--------------------------------------------------------------------------------------
    __forceinline
    HOCWeakClassifierD&   GetWeakClassifierD( UINT i )
    {
        if( m_bSubrangeEnabled )
            i += m_uSubrangeStart;

        return m_weakClassifiersD[ i ];
    }

    //--------------------------------------------------------------------------------------
    // Name: GetWeakClassifier
    // Desc: Returns a weak classifier i
    //--------------------------------------------------------------------------------------
    __forceinline
    const HOCWeakClassifierD&   GetWeakClassifierD( UINT i ) const
    {
        if( m_bSubrangeEnabled )
            i += m_uSubrangeStart;

        return m_weakClassifiersD[ i ];
    }

    //--------------------------------------------------------------------------------------
    // Name: SetTotalAlpha
    // Desc: Sets total alpha (weight) of this strong classifier
    //--------------------------------------------------------------------------------------
    __forceinline
    VOID    SetTotalAlpha( FLOAT a )
    {
        m_fTotalAlpha = a;
        m_fInvTotalAlpha = a ? (1.f / a) : 0;
    }

    //--------------------------------------------------------------------------------------
    // Name: GetTotalAlpha
    // Desc: Gets total alpha (weight) of this strong classifier
    //--------------------------------------------------------------------------------------
    __forceinline
    FLOAT GetTotalAlpha() const
    {
        return m_fTotalAlpha;
    }

    //--------------------------------------------------------------------------------------
    // Name: RemoveWeakClassifierMaintainOrder
    // Desc: Removes a weak classifier with order preservation
    //--------------------------------------------------------------------------------------
    __forceinline
    VOID    RemoveWeakClassifierMaintainOrder( UINT i )
    {
        assert( !m_bSubrangeEnabled );

        if( m_bUseRealAdaboost )
            m_weakClassifiersR.erase( m_weakClassifiersR.begin() + i );
        else
            m_weakClassifiersD.erase( m_weakClassifiersD.begin() + i );
    }

    //--------------------------------------------------------------------------------------
    // Name: EnableSubset
    // Desc: Optimisation which allows to pretend that we only have a subrange of weak classifiers
    //--------------------------------------------------------------------------------------
    VOID    EnableSubset( UINT uStart, UINT uNum )
    {
        assert( uStart + uNum < (m_bUseRealAdaboost ? static_cast< UINT >( m_weakClassifiersR.size() ) :
                                                      static_cast< UINT >( m_weakClassifiersD.size() ) ) );

        m_bSubrangeEnabled = TRUE;
        m_uSubrangeStart = uStart;
        m_uSubrangeSize = uNum;
    }

    //--------------------------------------------------------------------------------------
    // Name: DisableSubset
    // Desc: Turns off subrange optimisation
    //--------------------------------------------------------------------------------------
    VOID    DisableSubset()
    {
        m_bSubrangeEnabled = FALSE;
    }

    //--------------------------------------------------------------------------------------
    // Name: SetTrainingSetInfo
    // Desc: Saves which parameters were used to trim the training set
    //--------------------------------------------------------------------------------------
    void    SetTrainingSetInfo( UINT uMinVoxels, UINT uMaxVoxels, FLOAT fMinDepth, FLOAT fMaxDepth, UINT uNumFrames, FLOAT fThreshold )
    {
        m_uNumVoxels[ 0 ] = uMinVoxels;
        m_uNumVoxels[ 1 ] = uMaxVoxels;
        m_fDistance[ 0 ] = fMinDepth;
        m_fDistance[ 1 ] = fMaxDepth;
        m_uNumFilterFrames = uNumFrames;
        m_fFilterThreshold = fThreshold;
    }

    //--------------------------------------------------------------------------------------
    // Name: IsSameClassOfClassifier
    // Desc: Shortcut for calling IsSameClass on two classifiers
    //--------------------------------------------------------------------------------------
    BOOL    IsSameClassOfClassifier( UINT i, UINT j ) const
    {
        if( m_bUseRealAdaboost )
        {
            return GetWeakClassifierR( i ).IsSameClass( GetWeakClassifierR( j ) );
        }

        return GetWeakClassifierD( i ).IsSameClass( GetWeakClassifierD( j ) );
    }

    //--------------------------------------------------------------------------------------
    // Name: ComputeTotalAlpha
    // Desc: Calculate total weight of the classifier
    //--------------------------------------------------------------------------------------
    void    ComputeTotalAlpha()
    {
        const UINT T = GetNumWeakClassifiers();

        FLOAT fSum = 0;

        if( m_bUseRealAdaboost )
        {
            for( UINT i = 0; i < T; i++ )
                fSum += GetWeakClassifierR( i ).GetAlpha();
        } else
        {
            for( UINT i = 0; i < T; i++ )
                fSum += GetWeakClassifierD( i ).GetAlpha();
        }

        SetTotalAlpha( fSum );
    }

    //--------------------------------------------------------------------------------------
    // Name: OptimizeByAlpha
    // Desc: Throws away weak classifiers that don't meet the weight bar
    //--------------------------------------------------------------------------------------
private:
    template< class T >
    static VOID    OptimizeByAlpha( T& classifiers, UINT uMaxClassifiers, FLOAT fAlphaCutoff, UINT uPrintMax );
public:
#endif

    //--------------------------------------------------------------------------------------
    // Name: GetTrainingSetInfo
    // Desc: Returns which parameters were used to trim the training set
    //--------------------------------------------------------------------------------------
    void    GetTrainingSetInfo( UINT& uMinVoxels, UINT& uMaxVoxels, FLOAT& fMinDepth, FLOAT& fMaxDepth, UINT& uNumFrames, FLOAT& fThreshold ) const
    {
        uMinVoxels = m_uNumVoxels[ 0 ];
        uMaxVoxels = m_uNumVoxels[ 1 ];
        fMinDepth = m_fDistance[ 0 ];
        fMaxDepth = m_fDistance[ 1 ];
        uNumFrames = m_uNumFilterFrames;
        fThreshold = m_fFilterThreshold;
    }

    //--------------------------------------------------------------------------------------
    // Name: Classify
    // Desc: Performs a classification of the given data view
    //--------------------------------------------------------------------------------------
    FLOAT   Classify( const HOCDataViews& d, const HOCExtraClassifierData& e ) const;

    //--------------------------------------------------------------------------------------
    // Name: Write
    // Desc: Writes a strong classifier to a file
    //--------------------------------------------------------------------------------------
    HRESULT Write( _In_ FILE* fp ) const;

    //--------------------------------------------------------------------------------------
    // Name: Read
    // Desc: Reads a strong classifier from a file
    //--------------------------------------------------------------------------------------
    HRESULT Read( _In_ FILE* fp );
    HRESULT ReadPrev( _In_ FILE* fp );

    //--------------------------------------------------------------------------------------
    // Name: UsesRealAdaBoost
    // Desc: Returns whether this classifier is trained with real adaboost or discrete adaboost
    //--------------------------------------------------------------------------------------
    BOOL    UsesRealAdaBoost() const
    {
        return m_bUseRealAdaboost;
    }
private:

    static HRESULT WriteArrayOfClassifiers( FILE* fp, const HOCWeakClassifiersVectorD& arr );
    static HRESULT WriteArrayOfClassifiers( FILE* fp, const HOCWeakClassifiersVectorR& arr );
    static HRESULT ReadArrayOfClassifiers( FILE* fp, HOCWeakClassifiersVectorD& arr );
    static HRESULT ReadArrayOfClassifiers( FILE* fp, HOCWeakClassifiersVectorR& arr );
};


//--------------------------------------------------------------------------------------
// Name: HOCLevel1Gate
// Desc: This is specific to the ensemble. It tells us what classifier index to use for
// what input data. It checks the range of the input data parameters and redirects to a
// corresponding classifier which is an expert in that area.
//--------------------------------------------------------------------------------------
struct HOCLevel1Gate
{
    USHORT  m_uMinVoxels, m_uMaxVoxels;
    BYTE    m_uDestinationIndex;
    BYTE    pad[ 3 ];
};

//--------------------------------------------------------------------------------------
// Name: HOCExtraClassifierData
// Desc: Data that doesn't come from NUI is stored separately
//--------------------------------------------------------------------------------------
struct HOCExtraClassifierData
{
    std::vector< HOCDetector >      m_level0Detectors;
    std::vector< HOCLevel1Gate >    m_level1Gates;

    //--------------------------------------------------------------------------------------
    // Name: Write
    // Desc: Writes to a file
    //--------------------------------------------------------------------------------------
    HRESULT Write( _In_ FILE* fp ) const;

    //--------------------------------------------------------------------------------------
    // Name: Read
    // Desc: Reads from a file
    //--------------------------------------------------------------------------------------
    HRESULT Read( _In_ FILE* fp );
};


#ifdef HOC_TRAINER
//--------------------------------------------------------------------------------------
// Name: HOCDetectorTrainer
// Desc: The trainer for the detector. Its implementation is in a separate file so
// if you aren't using it on 360, it won't get linked into the final executable
//--------------------------------------------------------------------------------------
class HOCDetectorTrainer
{
public:

    //--------------------------------------------------------------------------------------
    // Name: Parameters
    // Desc: All parameters for the trainer that you can change. See SetDefaults for description
    //--------------------------------------------------------------------------------------
    struct Parameters
    {
        BOOL    (*m_pfnSampleAcceptor)( const HOCDataViews& );

        BOOL    m_bRealAdaboost;
        BOOL    m_bOptimisationLeaveOnlyBreakpoints;
        BOOL    m_bSaveFailedThumbnails;
        BOOL    m_bUseRBoost;
        BOOL    m_bUseSTBasedClassifiers;
        BOOL    m_bVerbose;
        UINT    m_uNumThresholdSteps;
        UINT    m_uNumRules;
        FLOAT   m_fRuleMustBeThisSignificant;
        UINT    m_uMaxWeakClassifiers;
        FLOAT   m_fErrorThreshold;
        FLOAT   m_fAlphaCutoff;
        UINT    m_uProgressTick;
        UINT    m_uOptimisePrintTopN;

        Parameters()
        {
            SetDefault();
        }

        void    SetDefault();
    };

    //--------------------------------------------------------------------------------------
    // Name: TestResults
    // Desc: Contains the number of true/false positives/negatives
    //--------------------------------------------------------------------------------------
    struct TestResults
    {
        UINT    m_uTruePositive;
        UINT    m_uTrueNegative;
        UINT    m_uFalseNegative;
        UINT    m_uFalsePositive;

        //--------------------------------------------------------------------------------------
        // Name: GetNumCases
        // Desc: returns the total number of instances tested
        //--------------------------------------------------------------------------------------
        UINT    GetNumCases() const
        {
            return m_uFalseNegative + m_uFalsePositive + m_uTruePositive + m_uTrueNegative;
        }

        //--------------------------------------------------------------------------------------
        // Name: GetSuccessRatio
        // Desc: This is called accuracy in books. (TP + TN) / (TN + FN)
        //--------------------------------------------------------------------------------------
        FLOAT   GetSuccessRatio() const
        {
            return static_cast< FLOAT >( m_uTruePositive + m_uTrueNegative ) / static_cast< FLOAT >( GetNumCases() );
        }

        void Print() const
        {
            const UINT    uTotal = GetNumCases();

            const FLOAT in = 1.f / static_cast< FLOAT >( uTotal );
            printf( "num cases = %d, correct = %f, false +1 %f, false -1 %f\n",
                    uTotal,
                    static_cast< FLOAT >( m_uTruePositive + m_uTrueNegative ) * in,
                    static_cast< FLOAT >( m_uFalsePositive ) * in,
                    static_cast< FLOAT >( m_uFalseNegative ) * in );
        }
    };

    //--------------------------------------------------------------------------------------
    // Name: Train
    // Desc: Trains on the given data set
    //--------------------------------------------------------------------------------------
    static HRESULT Train(   HOCDetector& detector,
                            const HOCExtraClassifierData& ensemble,
                            _In_ const CHAR** fileNames,
                            _In_ const BOOL* pbOpenedOrClosedTags,
                            const Parameters& parameters );

    //--------------------------------------------------------------------------------------
    // Name: Test
    // Desc: Test the classifier on the test set
    //--------------------------------------------------------------------------------------
    static HRESULT Test(    TestResults& results,
                            const HOCDetector& detector,
                            _In_ const CHAR** fileNames,
                            _In_ const BOOL* pbOpenedOrClosedTags,
                            const Parameters& parameters );

    //--------------------------------------------------------------------------------------
    // Name: FindFilteringParameters
    // Desc: Finds best filtering parameters (experimental)
    //--------------------------------------------------------------------------------------
    static HRESULT FindFilteringParameters( UINT& uNumFrames,
                                            FLOAT& fThreshold,
                                            const HOCDetector& testDetector,
                                            const CHAR** fileNames,
                                            const BOOL* pbOpenedOrClosed,
                                            const Parameters& parameters );


    //--------------------------------------------------------------------------------------
    // Name: OutputDataCSV
    // Desc: Print out the data for external analysis
    //--------------------------------------------------------------------------------------
    static HRESULT OutputDataCSV(   _In_ const CHAR* pFilename,
                                    _In_ const CHAR** fileNames,
                                    _In_ const BOOL* pbOpenedOrClosed,
                                    const Parameters& parameters );
   
private:

    // use double or float during training. FLOAT isn't faster by much, so using double is recommended
//  typedef FLOAT   Real;
    typedef DOUBLE  Real;

    // optimisation that trades memory for speed. we pre-add the values in the probability
    // distribution vector to sum it up more quickly
    static const UINT   MAX_LENGTH_OF_RUN = 10;

    //--------------------------------------------------------------------------------------
    // Name: HOCSamplesSet
    // Desc: A set of ground truth samples for the trainer
    //--------------------------------------------------------------------------------------
    struct HOCSamplesSet
    {
        std::vector< CHAR >         m_labels;
        std::vector< HOCDataViews > m_examples;

        __forceinline
        UINT    GetNumExamples() const
        {
            return static_cast< UINT >( m_examples.size() );
        }
    };

    //--------------------------------------------------------------------------------------
    // Name: HOCClassifierExtents
    // Desc: When each training sample is tested by each classifier, we can establish the bounds
    // of classifier's response to the training set. We keep it in this structure.
    //--------------------------------------------------------------------------------------
    class HOCClassifierExtents
    {
        // min and max bounds for all classifiers spaces
        FLOAT   m_bounds[ HOCBaseWeakClassifier::TYPE_NUM_CLASSIFIERS ][ 2 ];

        VOID    UpdateClassifierExtents( const HOCBaseWeakClassifier& c, const HOCSamplesSet& trainingSet, const HOCExtraClassifierData& ensemble );

    public:
        VOID    ComputeClassifierExtents( HOCBaseWeakClassifier::Type type, const HOCSamplesSet& trainingSet, const HOCExtraClassifierData& ensemble );
        VOID    ComputeDimensionsExtents( const HOCSamplesSet& trainingSet );
        VOID    ComputeDimensionsExtents( const HOCStrongClassifier& c, const HOCSamplesSet& trainingSet );

        __forceinline
        FLOAT GetStartAndStep( HOCBaseWeakClassifier::Type type, FLOAT& fStart, INT iNumSteps ) const
        {
            fStart = m_bounds[ type ][ 0 ];
            return (m_bounds[ type ][ 1 ] - m_bounds[ type ][ 0 ]) / (FLOAT)iNumSteps;
        }
    };

    //--------------------------------------------------------------------------------------
    // Name: ResultsVector
    // Desc: Uses bits to store results to save memory
    //--------------------------------------------------------------------------------------
    class ResultsVector
    {
        DWORD*  m_pData;
        UINT    m_uNumBits;
        UINT    m_uNumDwords;

    public:
        //--------------------------------------------------------------------------------------
        // Name: Iterator
        // Desc: allows for fast sequential iteration
        //--------------------------------------------------------------------------------------
        struct Iterator
        {
            DWORD*  m_pPtr;
            DWORD   m_uCountdown;
            DWORD   m_uCached;
            DWORD   m_uMask;

            INT GetResultAndAdvance();
        };

        ResultsVector();
        ResultsVector( const ResultsVector& rhs );
        ~ResultsVector();
        ResultsVector& operator = ( const ResultsVector& rhs );

        VOID        Resize( UINT uNumBits );
        VOID        SetResult( UINT uBit, INT iResult );
        INT         GetResult( UINT uBit ) const;
        Iterator    Begin() const;
    };

    //--------------------------------------------------------------------------------------
    // Name: CachedResults
    // Desc: In AdaBoost the results of testing each classifiers with each training sample
    // never changes between the iterations so we cache it
    // inner size ~ number of samples
    // outer size = number of classifiers
    //--------------------------------------------------------------------------------------
    struct CachedResults
    {
        //typedef UINT    INDEX;      // make it USHORT to save memory in runtime if you're not training on a lot of samples
        typedef USHORT INDEX;      // make it UINT to support more than 65535 training samples

        std::vector< std::vector< INDEX > >  m_sumOverIndices[ MAX_LENGTH_OF_RUN ];    // indices of samples whose weight we need to sum up
        std::vector< ResultsVector >         m_results;                                 // h(x) E {Y} for each sample xi

        //--------------------------------------------------------------------------------------
        // Name: IsSameResults
        // Desc: Compares if two instances of a classifier return exactly the same results
        //--------------------------------------------------------------------------------------
        BOOL    IsSameResults( UINT i, UINT j ) const;
    };

    //--------------------------------------------------------------------------------------
    // Name: CachedResultsR
    // Desc: 
    //--------------------------------------------------------------------------------------
    struct CachedResultsR
    {
        std::vector< std::vector< FLOAT > > m_values;
        std::vector< std::vector< BYTE > > m_indices;
        std::vector< FLOAT >    m_minValues;
        std::vector< FLOAT >    m_maxValues;
    };

    static HRESULT OptimizeByAlpha( HOCStrongClassifier& strongClassifier, const Parameters& params );
    static HRESULT ImportGroundTruthTrainingSet( HOCSamplesSet& groundTruthTrainingSet, _In_ const CHAR** pFileNames, _In_ const BOOL* pbOpenedOrClosed, _In_opt_ BOOL (*pfnSampleAcceptor)( const HOCDataViews& ), BOOL bVerbose );
    static VOID    InstantiateWeakClassifiersD( HOCStrongClassifier& strongClassifier, const HOCClassifierExtents& bounds, UINT uNumSteps, BOOL bUseSTBasedClassifiers );
    static VOID    InstantiateWeakClassifiersR( HOCStrongClassifier& strongClassifier, const HOCSamplesSet& trainingSet, const BOOL bUseSTBasedClassifiers );
    static VOID    InstantiateLevel1Classifiers( HOCDetector& level1Classifier, const HOCSamplesSet& trainingSet );
    static UINT    ComputeClassifierSplits( _In_count_c_( 2 ) HOCWeakClassifierD pSplit[ 2 ], FLOAT& fScore, const HOCWeakClassifierD& c, const HOCSamplesSet& trainingSet );
    static UINT    FindBestSplit( _In_count_c_( 2 ) HOCWeakClassifierD splits[ 2 ], const HOCSamplesSet& trainingSet, const HOCStrongClassifier& classifier, FLOAT fSignificance );
    static VOID    BuildRulesAndAdjustTrainingSet( HOCWeakClassifiersVectorD& rules, HOCSamplesSet& trainingSet, const HOCStrongClassifier& classifier, UINT uNumRules, FLOAT fSignificance );
    static UINT    OptimiseWeakClassifers( CachedResults& results, HOCStrongClassifier& classifier, const HOCSamplesSet& trainingSet, const HOCExtraClassifierData& ensemble );
    static VOID    ComputeClassifiersResults( CachedResults& results, const HOCStrongClassifier& classifier, const HOCSamplesSet& trainingSet, const HOCExtraClassifierData& ensemble, BOOL bBuildYHResults, BOOL bOptimiseForSums );
    static VOID    ComputeClassifiersResultsR( CachedResultsR& results, const HOCStrongClassifier& classifier, const HOCSamplesSet& trainingSet, const HOCExtraClassifierData& ensemble );
    static HRESULT TrainStrongClassifier( HOCStrongClassifier& classifier, const HOCSamplesSet& trainingSet, const HOCExtraClassifierData& ensemble, const Parameters& params );
    static Real    GetClassifierW( const std::vector< Real >(& w)[ MAX_LENGTH_OF_RUN ], const CachedResults& results, UINT uIndex );
    static VOID    ComputeSums( std::vector< Real >(& w)[ MAX_LENGTH_OF_RUN ] );
    static VOID    DiscreteAdaBoost( HOCStrongClassifier& strongClassifier, const CachedResults& results, const HOCSamplesSet& trainingSet, const Parameters& params );
    static VOID    RBoost( HOCStrongClassifier& strongClassifier, const CachedResults& results, const HOCSamplesSet& trainingSet, const Parameters& params );
    static VOID    RealAdaBoost( HOCStrongClassifier& strongClassifier, const CachedResultsR& results, const HOCSamplesSet& trainingSet, const Parameters& params );
    static VOID    FindBestClassifier( UINT& uBestWeakClassifierIndex, Real& fMinError, const std::vector< BYTE >& classifierChosen, std::vector< Real > (&w)[ MAX_LENGTH_OF_RUN ], const CachedResults& results );
    static Real    FindMinMaxAndResponse( HOCWeakClassifierR& c, const HOCSamplesSet& trainingSet, std::vector< Real >& w, Real& fMaxAbsHtValue, UINT uIndex, const CachedResultsR& results );
};

#endif


//--------------------------------------------------------------------------------------
// Name: IHOCDetectorDebugDraw
// Desc: This allows the detector to call back debug rendering code. Can be compiled out
//--------------------------------------------------------------------------------------
struct IHOCDetectorDebugDraw
{
    virtual VOID    AddQuadInDepthImageSpace( INT x, INT y, INT sx, INT sy, DWORD clr ) = 0;
};

//--------------------------------------------------------------------------------------
// Name: HOCDetector
// Desc: The class in charge of the Hand Open Closed detection
//--------------------------------------------------------------------------------------
class HOCDetector
{
    friend class HOCDetectorTrainer;
public:
    HRESULT Save( _In_ const CHAR* szFileName ) const;
    HRESULT Save( _In_ FILE* pFile ) const;
    HRESULT Load( _In_ const CHAR* szFileName );
    HRESULT Load( _In_ FILE* pFile );

    //--------------------------------------------------------------------------------------
    // Name: Detect
    // Desc: Given the hand data, calculate what class it belongs to
    //--------------------------------------------------------------------------------------
    __forceinline
    FLOAT   Detect( const HOCDataViews& data ) const
    {
        if( m_ensemble.m_level0Detectors.empty() )
            return m_rootClassifier.Classify( data, m_ensemble );
        else
            return Level1Detect( data, m_ensemble );
    }

#ifdef HOC_TRAINER
    void    SetTrainingSetInfo( UINT uMinVoxels, UINT uMaxVoxels, FLOAT fMinDepth, FLOAT fMaxDepth, UINT uNumFramesFilter, FLOAT& fFilterThreshold )
    {
        m_rootClassifier.SetTrainingSetInfo( uMinVoxels, uMaxVoxels, fMinDepth, fMaxDepth, uNumFramesFilter, fFilterThreshold );
    }
#endif

    //--------------------------------------------------------------------------------------
    // Name: GetPlayerSize
    // Desc: Given the skeleton, update current player's size. Size is always updated,
    // confidence is returned
    //--------------------------------------------------------------------------------------
    static BOOL GetPlayerSize( FLOAT& fPlayerSize, const NUI_SKELETON_DATA* pSkeleton );

    //--------------------------------------------------------------------------------------
    // Name: GetFrameData
    // Desc: Given NUI data fill out the source and computed data structures
    //--------------------------------------------------------------------------------------
    static BOOL GetFrameData(   HOCDataViews& data,
                                BOOL bTrackedElbow, BOOL bTrackedWrist,
                                XMVECTOR vElbow, XMVECTOR vWrist, XMVECTOR vHand,
                                _In_ const USHORT* pDepth,
                                FLOAT fPlayerSize );

    static  IHOCDetectorDebugDraw*    ms_debugDraw;

private:

    //--------------------------------------------------------------------------------------
    // Name: Level1Detect
    // Desc: Level1 doesn't use ada boost but is hardcoded
    //--------------------------------------------------------------------------------------
    FLOAT   Level1Detect( const HOCDataViews& d, const HOCExtraClassifierData& e ) const;

    HOCExtraClassifierData  m_ensemble;
    HOCStrongClassifier     m_rootClassifier;
};



//--------------------------------------------------------------------------------------
// Name: HOCFilter
// Desc: Filters results, optional
//--------------------------------------------------------------------------------------
struct HOCFilter
{
    FLOAT   m_fConfidenceHistory[ 8 ];  // this keeps N frames of previous data for filetring
    FLOAT   m_fFilteredConfidence;     // this is filtered confidence
    FLOAT   m_fConfidence;             // current instant confidence
    UINT    m_uFilterHead;

    HOCFilter() : m_uFilterHead( 0 ), m_fConfidence( 0 ), m_fFilteredConfidence( 0 )
    {
        memset( m_fConfidenceHistory, 0, sizeof( m_fConfidenceHistory ) );
    }

    //--------------------------------------------------------------------------------------
    // Name: FilterType0
    // Desc: This clamps the negative values to 0, then sums over a window, and thresholds
    // the result. Similar to Kickboxing sample.
    //--------------------------------------------------------------------------------------
    FLOAT   FilterType0( FLOAT fConfidence, FLOAT fThres = 0.01f, UINT uNumFrames = _countof( (((HOCFilter*)0)->m_fConfidenceHistory ) ) )
    {
        m_fConfidence = fConfidence;

        // add a new value to the top
        m_fConfidenceHistory[ m_uFilterHead ] = __fself( fConfidence, fConfidence, 0 );
        m_uFilterHead = (m_uFilterHead + 1) % uNumFrames;

        m_fFilteredConfidence = 0;
        for( UINT i=0; i < uNumFrames; ++i )
        {
            m_fFilteredConfidence += m_fConfidenceHistory[ i ];
        }

        m_fFilteredConfidence = m_fFilteredConfidence > fThres ? 1.f : -1.f;

        return m_fFilteredConfidence;
    }

    //--------------------------------------------------------------------------------------
    // Name: FilterType1
    // Desc: This checks if a value is significant first to suppress noise, then sums over a
    // window and returns the result. This is the original filter from HOC sample.
    //--------------------------------------------------------------------------------------
    FLOAT FilterType1( FLOAT fConfidence, FLOAT fThres = 0.01f, UINT uNumFrames = _countof( (((HOCFilter*)0)->m_fConfidenceHistory ) ) )
    {
        m_fConfidence = fConfidence;

        if( fabsf( fConfidence ) > fThres )
        {
            // add a new value to the top
            m_fConfidenceHistory[ m_uFilterHead ] = fConfidence;
            m_uFilterHead = (m_uFilterHead + 1) % uNumFrames;

            m_fFilteredConfidence = 0;
            for( UINT i=0; i < uNumFrames; ++i )
            {
                m_fFilteredConfidence += m_fConfidenceHistory[ i ];
            }
        }

        return m_fFilteredConfidence;
    }

    //--------------------------------------------------------------------------------------
    // Name: FilterType2
    // Desc: This calculates the longest contiguous run in the window.
    //--------------------------------------------------------------------------------------
    FLOAT   FilterType2( FLOAT fConfidence, UINT uNumFrames = _countof( (((HOCFilter*)0)->m_fConfidenceHistory ) ) )
    {
        m_fConfidence = fConfidence;

        // add a new value to the top
        m_fConfidenceHistory[ m_uFilterHead ] = fConfidence;
        m_uFilterHead = (m_uFilterHead + 1) % uNumFrames;

        UINT uMaxLength[ 2 ] = { 0 };
        UINT uCurLength = 0;
        BOOL bLastSign = m_fConfidenceHistory[ 0 ] > 0;
        for( UINT i=0; i < uNumFrames; ++i )
        {
            const BOOL bThisSign = ( m_fConfidenceHistory[ i ] > 0 );

            if( bLastSign == bThisSign )
                ++uCurLength;
            else
                if( uMaxLength[ bLastSign ] < uCurLength )
                    uMaxLength[ bLastSign ] = uCurLength;

            bLastSign = bThisSign;
        }

        if( uMaxLength[ 0 ] == uMaxLength[ 1 ] )
            m_fFilteredConfidence = (fConfidence > 0) ? 1.f : -1.f;
        else
            m_fFilteredConfidence = (uMaxLength[ 1 ] > uMaxLength[ 0 ]) ? 1.f : -1.f;

        return m_fFilteredConfidence;
    }
};

//--------------------------------------------------------------------------------------
// Name: HOCPrintClassifier
// Desc: Debug printout of the classifier
//--------------------------------------------------------------------------------------
VOID    HOCPrintClassifier( const HOCWeakClassifierD& c, UINT i, BOOL bPrintEol = TRUE );
VOID    HOCPrintClassifier( const HOCWeakClassifierR& c, UINT i, BOOL bPrintEol = TRUE );

inline
VOID    HOCPrintClassifier( const HOCBaseWeakClassifier& c, UINT i, BOOL bPrintEol = TRUE, BOOL bReal = TRUE )
{
    if( bReal )
        HOCPrintClassifier( static_cast< const HOCWeakClassifierR& >( c ), i, bPrintEol );
    else
        HOCPrintClassifier( static_cast< const HOCWeakClassifierD& >( c ), i, bPrintEol );
}

//--------------------------------------------------------------------------------------
// Name: HOCGetClassiferName
// Desc: Returns a printable name for a classifier
//--------------------------------------------------------------------------------------
const CHAR* HOCGetClassiferName( UINT t /*HOCWeakClassifier::Type*/ );



#pragma warning( pop )

