#ifdef HOC_TRAINER

//--------------------------------------------------------------------------------------
// HOCTrainer.cpp
//
// Uses discrete AdaBoost and RBoost. Given a set of training examples and a set of weak
// classifiers, train a strong classifier using AdaBoost. We then compact the results using
// a pass of RBoost.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "HOCDetectorInternal.h"

// don't try using openmp on 360
#ifndef _XBOX
#define OMP
#endif

#ifdef OMP
#include <omp.h>
#endif


//--------------------------------------------------------------------------------------
static void PrintTopClassifiers( const HOCStrongClassifier& classifier, UINT uNumTopToPrint, UINT uNumInRunToPrint );

//--------------------------------------------------------------------------------------
// Name: Train
// Desc: Trains on the given data set
//--------------------------------------------------------------------------------------
HRESULT HOCDetectorTrainer::Train(  HOCDetector& detector,
                                    const HOCExtraClassifierData& ensemble,
                                    const CHAR** fileNames,
                                    const BOOL* pbOpenedOrClosed,
                                    const Parameters& params )
{
    HOCSamplesSet trainingSet;

    printf( "\n\nStep 1 of 3: Importing a Ground Truth Training Set\n" );
    if( FAILED( ImportGroundTruthTrainingSet( trainingSet, fileNames, pbOpenedOrClosed, params.m_pfnSampleAcceptor, params.m_bVerbose ) ) )
        return E_FAIL;
    printf( "\n\t%d ground truth training examples imported\n", trainingSet.GetNumExamples() );

    // check if we've got the training examples
    if( !trainingSet.GetNumExamples() )
    {
        printf( "no training samples found, exiting\n" );
        return E_FAIL;
    }

    // check if we've got both positive and negative training examples, the algorithm needs both classes
    {
        UINT    uNumPositives = 0;

        for( UINT i=0; i < trainingSet.GetNumExamples(); ++i )
        {
            if( trainingSet.m_labels[ i ] > 0 )
                ++uNumPositives;
        }

        if( uNumPositives == 0 ||
            uNumPositives == trainingSet.GetNumExamples() )
        {
            printf( "The training set needs to contain non zero number of samples for both +1 and -1 classes\n" );
            return E_FAIL;
        }
    }

    // check if we're compiled with appropriate CachedResults::INDEX
    if( trainingSet.GetNumExamples() > 65535    &&
        sizeof( CachedResults::INDEX ) < 4 )
    {
        printf( "Too many training examples -- recompile with UINT CacheResults::INDEX\n" );
        return E_FAIL;
    }

    detector.m_rootClassifier.m_bUseRealAdaboost = params.m_bRealAdaboost;

    if( ensemble.m_level0Detectors.empty() )
    {
        if( !params.m_bRealAdaboost )
        {
            // try to build a short decision tree at the top to make sure all strong signals are removed
            printf( "\tTrying to build simple rules\n" );
            HOCClassifierExtents   bounds;
            bounds.ComputeDimensionsExtents( trainingSet );
            InstantiateWeakClassifiersD( detector.m_rootClassifier, bounds, 1, params.m_bUseSTBasedClassifiers );   // only 1 classifier of each type is needed for the subsequent step
            BuildRulesAndAdjustTrainingSet( detector.m_rootClassifier.m_listOfRules, trainingSet, detector.m_rootClassifier, params.m_uNumRules, params.m_fRuleMustBeThisSignificant );

            // collect axis data and create weak classifiers
            printf( "\n\nStep 2 of 3: Instantiating Weak Classifiers\n" );
            bounds.ComputeDimensionsExtents( trainingSet );
            InstantiateWeakClassifiersD( detector.m_rootClassifier, bounds, params.m_uNumThresholdSteps, params.m_bUseSTBasedClassifiers );
            printf( "\n\t%d weak classifiers generated", detector.m_rootClassifier.GetNumWeakClassifiers() );
        } else 
        {
            InstantiateWeakClassifiersR( detector.m_rootClassifier, trainingSet, params.m_bUseSTBasedClassifiers );   // only 1 classifier of each type is needed for the subsequent step
        }

        // pass it to ada boost now for training
        printf( "\n\nStep 3 of 3: Training Strong Classifier" );
        if( FAILED( TrainStrongClassifier( detector.m_rootClassifier, trainingSet, detector.m_ensemble, params ) ) )
            return E_FAIL;
    } else
    {
        printf( "Strong (level0) classifiers given, ignoring weak classifiers and building level1 (ensemble) classifier\n" );

        // copy ensemble
        detector.m_ensemble = ensemble;

        // instantiate thunk weak classifiers that point to level0 classifiers
        InstantiateLevel1Classifiers( detector, trainingSet );
    }

    printf( "\nAll done\n" );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: OptimizeByAlpha
// Desc: OptimizeByAlpha for runtime by reducing the number of weak classifiers that contribute
//       to the strong classifier. We simply sort the weak classifiers based on the
//       weights in the strong classifier and take the first N classifiers
//--------------------------------------------------------------------------------------
HRESULT HOCDetectorTrainer::OptimizeByAlpha( HOCStrongClassifier& strongClassifier, const Parameters& params )
{
    printf( "\n\nOptimizing\n" );
    UINT nNumClassifiers = strongClassifier.GetNumWeakClassifiers();

    strongClassifier.OptimizeByAlpha( params.m_uMaxWeakClassifiers, params.m_fAlphaCutoff, params.m_uOptimisePrintTopN );

    printf( "\tNum Weak Classifiers: %d -> %d\n", nNumClassifiers, strongClassifier.GetNumWeakClassifiers() );
    printf( "Done\n" );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Test
// Desc: Test the classifier on the test set
//--------------------------------------------------------------------------------------
HRESULT HOCDetectorTrainer::Test(   TestResults& result,
                                    const HOCDetector& testDetector,
                                    const CHAR** fileNames,
                                    const BOOL* pbOpenedOrClosed,
                                    const Parameters& params )
{
    HOCSamplesSet testSet;

    if( FAILED( ImportGroundTruthTrainingSet( testSet, fileNames, pbOpenedOrClosed, params.m_pfnSampleAcceptor, params.m_bVerbose ) ) )
        return E_FAIL;

    result.m_uTruePositive = 0;
    result.m_uTrueNegative = 0;
    result.m_uFalsePositive = 0;
    result.m_uFalseNegative = 0;

    const INT nNumExamples = static_cast< INT >( testSet.m_examples.size() );

    for( INT i=0; i < nNumExamples; ++i )
    {
        const HOCDataViews& truth = testSet.m_examples[ i ];
        const INT iGroundTruthLabel = testSet.m_labels[ i ];

        const FLOAT fConf = testDetector.Detect( truth );
        const INT   iRes = (fConf > 0) ? 1 : -1;

        if( iGroundTruthLabel == iRes )
        {
            if( iGroundTruthLabel == 1 )
            {
                result.m_uTruePositive++;
            } else
            {
                result.m_uTrueNegative++;
            }
        } else
        {
            if( iGroundTruthLabel == 1 )
            {
                result.m_uFalsePositive++;
            } else
            {
                result.m_uFalseNegative++;
            }

            if( params.m_bSaveFailedThumbnails )
            {
                CHAR tmp[ 1024 ];
                sprintf_s( tmp, "fail//%d.%d.tga", i, iGroundTruthLabel );
                FILE* fp;
                if( !fopen_s( &fp, tmp, "wb" ) )
                {
                    truth.m_sourceData.SaveDebugThumbnail( fp );
                    fclose( fp );
                }
            }
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ValidateSample
// Desc: Throw away training samples that aren't suitable
//--------------------------------------------------------------------------------------
static
BOOL    ValidateSample( const HOCDataViews& dv )
{
    // validating input data
    if( (HOCSourceData::ELBOW_TRACKED | HOCSourceData::WRIST_TRACKED) != (dv.m_sourceData.m_bTrackedElbowWrist & (HOCSourceData::ELBOW_TRACKED | HOCSourceData::WRIST_TRACKED)) )
    {
        // got to have elbow and wrist tracked
        return FALSE;
    } else if( dv.m_sourceData.m_uNumVoxels < MINIMUM_ACCEPTABLE_NUM_VOXELS )
    {
        // got to have a reasonable number of voxels
        return FALSE;
    }

    return TRUE;
}

//--------------------------------------------------------------------------------------
// Name: ImportGroundTruthTrainingSet
// Desc: Imports the data from the disk, make sure it's alright, and store it for the trainer
//--------------------------------------------------------------------------------------
HRESULT HOCDetectorTrainer::ImportGroundTruthTrainingSet(   HOCSamplesSet& trainingSet,
                                                            const CHAR** pFileNames,
                                                            const BOOL* pbOpenedOrClosed,
                                                            BOOL (*pfnSampleAcceptor)( const HOCDataViews& ),
                                                            BOOL bVerbose )
{
    while( *pFileNames )
    {
        FILE* fp;
        if( fopen_s( &fp, *pFileNames, "rb" ) || !fp )
        {
            printf( "can't open %s\n", *pFileNames );
            return E_FAIL;
        }

        UINT    uNumFrames;
        ReadUInt( &uNumFrames, fp );

        UINT    uAdded = 0;

        trainingSet.m_examples.reserve( uNumFrames );
        trainingSet.m_labels.reserve( uNumFrames );

        HOCDataViews  dv;
        for( UINT i=0; i < uNumFrames; ++i )
        {
            if( FAILED( dv.Load( fp ) ) )
                return E_FAIL;

            if( ValidateSample( dv ) )
            {
                if( pfnSampleAcceptor && !pfnSampleAcceptor( dv ) )
                    continue;

                trainingSet.m_labels.push_back( *pbOpenedOrClosed ? 1 : -1 );
                trainingSet.m_examples.push_back( HOCDataViews() );
                trainingSet.m_examples.back().Own( dv );
                ++uAdded;
            }
        }

        fclose( fp );

        if( bVerbose )
            printf( "%s accepts %d frames (%d dropped) label=%d\n", *pFileNames, uAdded, uNumFrames - uAdded, *pbOpenedOrClosed );

        pFileNames++;
        pbOpenedOrClosed++;
    }

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: InstantiateWeakClassifiers
// Desc: We generate instances of weak classifiers for the trainer making sure all instances
// of each classifier are laid out sequentially in the array
//--------------------------------------------------------------------------------------
VOID HOCDetectorTrainer::InstantiateWeakClassifiersD( HOCStrongClassifier& strongClassifier, const HOCClassifierExtents& bounds, const UINT uNumSteps, const BOOL bUseSTBasedClassifiers )
{
    strongClassifier.m_weakClassifiersD.clear();

    const DWORD dwStart = GetTickCount();

    // relative interbucket correlations
    // no need for reverse here
    for( HOCBaseWeakClassifier::Type type = HOCBaseWeakClassifier::TYPE_REQUIRES_TWO_BUCKETS;
         type < HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET;
         ++type )
    {
        for( UINT i=0; i < HOCComputedData::NUM_BUCKETS; ++i )
        {
            for( UINT j=0; j < HOCComputedData::NUM_BUCKETS; ++j )
            {
                if( i == j )
                    continue;

                // this will compare bucket sizes to other buckets hopefully
                // capturing the shape of the curve
                HOCWeakClassifierD  bucketDifference;
                bucketDifference.m_type = type;
                bucketDifference.m_bucketIndices[ 0 ] = static_cast< BYTE >( i );
                bucketDifference.m_bucketIndices[ 1 ] = static_cast< BYTE >( j );

                // the difference between two buckets could be negative so we don't add a reverse weak classifier here
                FLOAT fThres;
                FLOAT fStep = bounds.GetStartAndStep( type, fThres, uNumSteps );

                for( UINT k=0; k < uNumSteps; ++k )
                {
                    bucketDifference.SetThreshold( fThres );
                    strongClassifier.AddWeakClassifier( bucketDifference );

                    fThres += fStep;
                }
            }
        }
    }

    // absolute bucket values
    for( HOCBaseWeakClassifier::Type type = HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET;
         type < HOCBaseWeakClassifier::TYPE_REQUIRES_NO_BUCKETS;
         ++type )
    {
        // for integral histogram the last bucket is always 1 by definition so skip it
        UINT uNumBuckets = HOCComputedData::NUM_BUCKETS;

        if( type >= HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET_MINUS_ONE )
            --uNumBuckets;

        //if( type >= HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET_MINUS_TWO )
        //    --uNumBuckets;

        for( UINT i=0; i < uNumBuckets; ++i )
        {
            HOCWeakClassifierD  absValue;
            absValue.m_type = type;
            absValue.m_bucketIndices[ 0 ] = static_cast< BYTE >( i );

            FLOAT fThres;
            FLOAT fStep = bounds.GetStartAndStep( type, fThres, uNumSteps );

            for( UINT j=0; j < uNumSteps; ++j )
            {
                absValue.SetThreshold( fThres );
                absValue.SetReverse( FALSE );
                strongClassifier.AddWeakClassifier( absValue );
                absValue.SetReverse( TRUE );
                strongClassifier.AddWeakClassifier( absValue );

                fThres += fStep;
            }
        }
    }

    // add all simple classifiers
    for(    HOCBaseWeakClassifier::Type type = HOCBaseWeakClassifier::TYPE_FIRST_SIMPLE_CLASSIFER;
            type < HOCBaseWeakClassifier::TYPE_LAST_SIMPLE_CLASSIFIER;
            ++type )
    {
        if( !bUseSTBasedClassifiers &&
            type == HOCBaseWeakClassifier::TYPE_BONE_LENGTHS )
        {
            continue;
        }

        FLOAT fThres;
        const FLOAT fStep = bounds.GetStartAndStep( type, fThres, uNumSteps );

        HOCWeakClassifierD  classifier;
        classifier.m_type = type;

        for( UINT i=0; i < uNumSteps; ++i )
        {
            classifier.SetThreshold( fThres );
            classifier.SetReverse( FALSE );
            strongClassifier.AddWeakClassifier( classifier );
            classifier.SetReverse( TRUE );
            strongClassifier.AddWeakClassifier( classifier );
            fThres += fStep;
        }
    }

    printf( "InstantiateWeakClassifiers done %d (%.2f seconds)\n", strongClassifier.GetNumWeakClassifiers(), ((FLOAT)(GetTickCount() - dwStart) / 1000.f) );
    fflush( stdout );
}


//--------------------------------------------------------------------------------------
// Name: FindMinMaxAndResponse
// Desc: For real classifiers find the response values
//--------------------------------------------------------------------------------------
HOCDetectorTrainer::Real HOCDetectorTrainer::FindMinMaxAndResponse( HOCWeakClassifierR& c,
                                                                    const HOCSamplesSet& trainingSet,
                                                                    std::vector< Real >& w,
                                                                    Real& fMaxAbsHtValue,
                                                                    UINT uIndex,
                                                                    const CachedResultsR& results )
{
    Real fZ = 0;

    const UINT N = trainingSet.GetNumExamples();

    c.m_fMinValue = results.m_minValues[ uIndex ];
    c.m_fMaxValue = results.m_maxValues[ uIndex ];

    // here we split the real-valued output of the classifier into N equal blocks
    // each block will get it's own "response" which is based on the ratio of the number of positive examples
    // to the number of negative examples in the block

    std::vector< Real >    Wplus( HOCWeakClassifierR::NUM_RESPONSES );
    std::vector< Real >    Wminus( HOCWeakClassifierR::NUM_RESPONSES );

    for( UINT i=0; i < N; ++i )
    {
        const UINT b = results.m_indices[ uIndex ][ i ];

        // W+ is the overall weight of positive in the block
        // W- is the overall weight of negative in the block
        if( trainingSet.m_labels[ i ] > 0 )
        {
            Wplus[ b ] += w[ i ];
        } else
        {
            Wminus[ b ] += w[ i ];
        }
    }

    // recommended in "Improved boosting algorithms" to "smooth" the prediction
    const FLOAT fEpsilon = 1.f / (2 * N);

    // finally load up the classifier's blocks with correct responses
    // the math is taken from "Improved boosting algorithms"
    fZ = 0;
    for( UINT i=0; i < HOCWeakClassifierR::NUM_RESPONSES; ++i )
    {
        fZ += sqrt( Wplus[ i ] * Wminus[ i ] );

        c.m_fResponse[ i ] = static_cast< FLOAT >( 0.5 * log( (Wplus[ i ] + fEpsilon) / (Wminus[ i ] + fEpsilon) ) );

        fMaxAbsHtValue = std::max( fMaxAbsHtValue, (Real)fabs( c.m_fResponse[ i ] ) );
    }

    return fZ * 2;
}

//--------------------------------------------------------------------------------------
// Name: InstantiateWeakClassifiers
// Desc: We generate instances of weak classifiers for the trainer making sure all instances
// of each classifier are laid out sequentially in the array
//--------------------------------------------------------------------------------------
VOID HOCDetectorTrainer::InstantiateWeakClassifiersR( HOCStrongClassifier& strongClassifier, const HOCSamplesSet& trainingSet, const BOOL bUseSTBasedClassifiers )
{
    strongClassifier.m_weakClassifiersR.clear();

    const DWORD dwStart = GetTickCount();

    // relative interbucket correlations
    // no need for reverse here
    for( HOCBaseWeakClassifier::Type type = HOCBaseWeakClassifier::TYPE_REQUIRES_TWO_BUCKETS;
         type < HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET;
         ++type )
    {
        for( UINT i=0; i < HOCComputedData::NUM_BUCKETS; ++i )
        {
            for( UINT j=0; j < HOCComputedData::NUM_BUCKETS; ++j )
            {
                if( i == j )
                    continue;

                // this will compare bucket sizes to other buckets hopefully
                // capturing the shape of the curve
                HOCWeakClassifierR  bucketDifference;
                bucketDifference.m_type = type;
                bucketDifference.m_bucketIndices[ 0 ] = static_cast< BYTE >( i );
                bucketDifference.m_bucketIndices[ 1 ] = static_cast< BYTE >( j );

                strongClassifier.AddWeakClassifier( bucketDifference );
            }
        }
    }

    // absolute bucket values
    for( HOCBaseWeakClassifier::Type type = HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET;
         type < HOCBaseWeakClassifier::TYPE_REQUIRES_NO_BUCKETS;
         ++type )
    {
        // for integral histogram the last bucket is always 1 by definition so skip it
        UINT uNumBuckets = HOCComputedData::NUM_BUCKETS;

        if( type >= HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET_MINUS_ONE )
            --uNumBuckets;

        //if( type >= HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET_MINUS_TWO )
        //    --uNumBuckets;

        for( UINT i=0; i < uNumBuckets; ++i )
        {
            HOCWeakClassifierR  absValue;
            absValue.m_type = type;
            absValue.m_bucketIndices[ 0 ] = static_cast< BYTE >( i );

            strongClassifier.AddWeakClassifier( absValue );
        }
    }

    // add all simple classifiers
    for(    HOCBaseWeakClassifier::Type type = HOCBaseWeakClassifier::TYPE_FIRST_SIMPLE_CLASSIFER;
            type < HOCBaseWeakClassifier::TYPE_LAST_SIMPLE_CLASSIFIER;
            ++type )
    {
        if( !bUseSTBasedClassifiers &&
            type == HOCBaseWeakClassifier::TYPE_BONE_LENGTHS )
        {
            continue;
        }

        HOCWeakClassifierR  classifier;
        classifier.m_type = type;

        strongClassifier.AddWeakClassifier( classifier );
    }

    printf( "InstantiateWeakClassifiers done %d (%.2f seconds)\n", strongClassifier.GetNumWeakClassifiers(), ((FLOAT)(GetTickCount() - dwStart) / 1000.f) );
    fflush( stdout );
}


//--------------------------------------------------------------------------------------
// Name: InstantiateLevel1Classifiers
// Desc: Level1 classifiers are fully trainer detectors, so we use thunk "weak" classifiers
//--------------------------------------------------------------------------------------
VOID HOCDetectorTrainer::InstantiateLevel1Classifiers( HOCDetector& detector, const HOCSamplesSet& trainingSet )
{
    printf( "building level1 classifier\n" );

    static const UINT NUM_VOXELS_STEP = 32;
    static const UINT MAX_VOXELS = 4096;
    static const UINT MAX_GATE_VALUE = 550;     // it is really robust if you have more than about 300 voxels

    // add gates by testing subsets of the training set with all level0 classifiers and picking the best in the range
    for( UINT uMinVoxels = 0; uMinVoxels < MAX_GATE_VALUE; uMinVoxels += NUM_VOXELS_STEP )
    {
        const UINT uMaxVoxels = uMinVoxels + NUM_VOXELS_STEP;

        UINT    uMaxScore = 0;
        UINT    uBestClassifier = 0;
        UINT    uTotalItems = 0;

        for( UINT uCurClassifier = 0; uCurClassifier < detector.m_ensemble.m_level0Detectors.size(); ++uCurClassifier )
        {
            const HOCDetector& classifier = detector.m_ensemble.m_level0Detectors[ uCurClassifier ];

            UINT uCurScore = 0;

            for( UINT i=0; i < trainingSet.m_examples.size(); ++i )
            {
                const HOCDataViews& dv = trainingSet.m_examples[ i ];

                if( dv.m_sourceData.m_uNumVoxels < uMinVoxels ||
                    dv.m_sourceData.m_uNumVoxels > uMaxVoxels )
                {
                    continue;
                }

                const INT iLabel = trainingSet.m_labels[ i ];
                const FLOAT fV = classifier.Detect( dv );
                const INT iRes = (fV > 0) ? 1 : -1;

                if( iRes == iLabel )
                    ++uCurScore;

                if( uCurClassifier == 0 )
                    ++uTotalItems;
            }

            if( uCurScore > uMaxScore )
            {
                uMaxScore = uCurScore;
                uBestClassifier = uCurClassifier;
            }
        }

        // add a gate
        if( uTotalItems )
        {
            HOCLevel1Gate   gate;
            gate.m_uDestinationIndex = uBestClassifier;
            gate.m_uMinVoxels = uMinVoxels;
            gate.m_uMaxVoxels = uMaxVoxels;
            detector.m_ensemble.m_level1Gates.push_back( gate );

            // verbose
            printf( "%d-%d voxels (%d items), score %f, index %d\n", uMinVoxels, uMaxVoxels, uTotalItems, (FLOAT)uMaxScore / (FLOAT)uTotalItems, uBestClassifier );
        }
    }

    // make sure last item has upper limit of 4096
    if( !detector.m_ensemble.m_level1Gates.empty() )
        detector.m_ensemble.m_level1Gates.back().m_uMaxVoxels = MAX_VOXELS;
}


//--------------------------------------------------------------------------------------
// Name: ComputeClassifiersResults
// Desc: This step runs each classifier instance against each training frame to produce
//       the results data. Then it removes redundant classifier instances
//--------------------------------------------------------------------------------------
void HOCDetectorTrainer::ComputeClassifiersResults( CachedResults& results,
                                                    const HOCStrongClassifier& classifier,
                                                    const HOCSamplesSet& trainingSet,
                                                    const HOCExtraClassifierData& ensemble,
                                                    BOOL bBuildYHResults,
                                                    BOOL bOptimiseForSums )
{
    const DWORD dwStart = GetTickCount();

    const UINT N = static_cast< UINT >( trainingSet.m_examples.size() );        // Number of examples in training set
    const UINT T = static_cast< UINT >( classifier.GetNumWeakClassifiers() );   // Number of weak classifiers

    for( UINT i=0; i < _countof( results.m_sumOverIndices ); ++i )
    {
        results.m_sumOverIndices[ i ].clear();
        results.m_sumOverIndices[ i ].resize( T );
    }

    if( bBuildYHResults )
    {
        results.m_results.clear();
        results.m_results.resize( T );
    }

    const BOOL bRealAdaBoost = classifier.UsesRealAdaBoost();

    // cache h(x) and h(x) * y(x)
#ifdef OMP
#pragma omp parallel for schedule( dynamic )
#endif
    for( INT i=0; i < (INT)T; ++i )
    {
        // to guarantee win in memory it makes sense to see whether the classifier makes more mistakes than
        // correct classifications or not. if it makes more mistakes we store positive classifications as
        // that array is shorter. otherwise we store negative classifications (mistakes) as this array is
        // shorter. this saves memory and increases runtime performance.
        std::vector< CachedResults::INDEX >   tempP;
        std::vector< CachedResults::INDEX >   tempN;
        tempP.reserve( N );
        tempN.reserve( N );
        tempP.push_back( 1 );   // [ 0 ] will indicate whether it's P (+1)
        tempN.push_back( 0 );   // or N (-1) class we're storing

        // A bit ugly, but don't want to add virtuals to the classifier class
        const HOCWeakClassifierR* pCR = NULL;
        const HOCWeakClassifierD* pCD = NULL;
        if( bRealAdaBoost )
            pCR = &classifier.GetWeakClassifierR( i );
        else
            pCD = &classifier.GetWeakClassifierD( i );

        if( bBuildYHResults )
            results.m_results[ i ].Resize( N );

        for( UINT j=0; j < N; ++j )
        {
            const HOCDataViews& data = trainingSet.m_examples[ j ];
            const INT iGroundTruthLabel = trainingSet.m_labels[ j ];

            // special treatment for heavy level0 classifiers
            INT iWeakClassifierResult;
            if( bRealAdaBoost )
            {
                const FLOAT   fValue = pCR->GetDataValue( data, ensemble );
                iWeakClassifierResult = static_cast< INT >( pCR->Classify( fValue ) );
            } else
            {
                const FLOAT   fValue = pCD->GetDataValue( data, ensemble );
                iWeakClassifierResult = static_cast< INT >( pCD->Classify( fValue ) );
            }

            // if the weak classifier wrongly classifies this example then contribute to the error
            if( iWeakClassifierResult != iGroundTruthLabel )
                tempN.push_back( j );
            else
                tempP.push_back( j );

            // this is h(x) * y(x) which is more useful later
            if( bBuildYHResults )
                results.m_results[ i ].SetResult( j, ( static_cast< CHAR >( iGroundTruthLabel * iWeakClassifierResult ) ) );
        }

        if( tempP.size() == 1 )
            printf( "All negative, can be thrown out\n" );
        if( tempN.size() == 1 )
            printf( "All positive, should be thrown out\n" );

        // collect runs of two, three and four failures in a row as optimisation
        // such runs can be presummed and the results can be shared between many
        // classifiers, which allows for a healthy speed increase
        if( bOptimiseForSums )
        {
            auto& temp = ( tempN.size() < tempP.size() ) ? tempN : tempP;   // choose the shortest

            // got nothing to do
            if( temp.size() < 2 )
                continue;

            // make sure all of them start with either 1 or 0 as the parent array
            for( UINT k=0; k < MAX_LENGTH_OF_RUN; ++k )
            {
                assert( results.m_sumOverIndices[ k ][ i ].empty() );
                results.m_sumOverIndices[ k ][ i ].push_back( temp[ 0 ] );
            }

            const UINT uNumItems = (UINT)temp.size();
            UINT uInRun = temp[ 1 ];
            UINT uRunLen = 1;
            UINT k;
            for( k=2; k < uNumItems; ++k )
            {
                // a run is broken or too long
                if( uInRun + uRunLen != temp[ k ]  ||
                    uRunLen >= _countof( results.m_sumOverIndices ) )
                {
                    results.m_sumOverIndices[ uRunLen - 1 ][ i ].push_back( uInRun );
                    
                    // start a new run
                    uInRun = temp[ k ];
                    uRunLen = 1;
                } else
                {
                    ++uRunLen;
                }
            }

            results.m_sumOverIndices[ uRunLen - 1 ][ i ].push_back( uInRun );
        } else
        {
            results.m_sumOverIndices[ 0 ][ i ] = ( tempN.size() < tempP.size() ) ? tempN : tempP;   // choose the shortest
        }
    }

    printf( "ComputeClassifiersResults done (%.2f seconds)\n", ((FLOAT)(GetTickCount() - dwStart) / 1000.f) );
    fflush( stdout );
}


//--------------------------------------------------------------------------------------
// Name: ComputeClassifiersResultsR
// Desc: This step runs each classifier instance against each training frame to produce
//       the results data. Then it removes redundant classifier instances
//--------------------------------------------------------------------------------------
void HOCDetectorTrainer::ComputeClassifiersResultsR( CachedResultsR& results,
                                                    const HOCStrongClassifier& classifier,
                                                    const HOCSamplesSet& trainingSet,
                                                    const HOCExtraClassifierData& ensemble )
{
    const DWORD dwStart = GetTickCount();
    
    // and now do the extra
    const UINT N = static_cast< UINT >( trainingSet.m_examples.size() );        // Number of examples in training set
    const UINT T = static_cast< UINT >( classifier.GetNumWeakClassifiers() );   // Number of weak classifiers

    results.m_values.clear();
    results.m_indices.clear();
    results.m_values.resize( T );
    results.m_indices.resize( T );
    results.m_minValues.resize( T );
    results.m_maxValues.resize( T );

    // cache h(x) and h(x) * y(x)
#ifdef OMP
#pragma omp parallel for schedule( dynamic )
#endif
    for( INT i=0; i < (INT)T; ++i )
    {
        HOCWeakClassifierR weakClassifier = classifier.GetWeakClassifierR( i );       // COPY

        results.m_values[ i ].resize( N );
        results.m_indices[ i ].resize( N );

        FLOAT fMin = FLT_MAX, fMax = -FLT_MAX;
        for( UINT j=0; j < N; ++j )
        {
            const HOCDataViews& data = trainingSet.m_examples[ j ];

            // special treatment for heavy level0 classifiers
            const FLOAT fValue = weakClassifier.GetDataValue( data, ensemble );

            results.m_values[ i ][ j ] = fValue;

            fMin = std::min( fMin, fValue );
            fMax = std::max( fMax, fValue );
        }

        results.m_maxValues[ i ] = fMax;
        results.m_minValues[ i ] = fMin;

        weakClassifier.m_fMaxValue = fMax;
        weakClassifier.m_fMinValue = fMin;

        for( UINT j=0; j < N; ++j )
        {
            results.m_indices[ i ][ j ] = weakClassifier.GetResponseIndex( results.m_values[ i ][ j ] );
        }
    }

    printf( "ComputeClassifiersResultsR done (%.2f seconds)\n", ((FLOAT)(GetTickCount() - dwStart) / 1000.f) );
    fflush( stdout );
}

//--------------------------------------------------------------------------------------
// Name: OptimiseWeakClassifers
// Desc: This step runs each classifier instance against each training frame to produce
//       the results data. Then it removes redundant classifier instances. This requires
//       a ComputeClassifierResults call afterwards as it may remove classifiers
//--------------------------------------------------------------------------------------
UINT HOCDetectorTrainer::OptimiseWeakClassifers(  CachedResults& results,
                                                  HOCStrongClassifier& strongClassifier,
                                                  const HOCSamplesSet& trainingSet,
                                                  const HOCExtraClassifierData& ensemble )
{
    const DWORD dwStart = GetTickCount();

    const UINT T = strongClassifier.GetNumWeakClassifiers();

    printf( "\n\t\t-Optimising breakpoints...\n" );
    printf( "\n\t\t-Building classifier results...\n" );

    ComputeClassifiersResults( results, strongClassifier, trainingSet, ensemble, FALSE, FALSE );

    // some instances of classifiers will always produce same results for all the items in the training set
    // we never re-evaluate the classifiers' results during training so there is no point in training on
    // the ones that fail on the exact same samples
    // an acute reader will note that the above is not strictly true because one classifier instance can
    // be boosted independently of the others. To avoid pathological cases here we leave one classifier
    // instance on either side of the "wavefront" -- that's where the values change
    // that dramatically reduces the number of classifier instances and in my tests didn't reduce quality
    std::vector< BYTE > toRemove( T, 0 );

    UINT uDropped = 0;
    for( UINT i=0; i < T; ++i )
    {
        if( toRemove[ i ] )
            continue;

        // find the run of exactly the same results from exactly the same classifier
        UINT j = i + 1;
        for( ; j < T; ++j )
        {
            // only allow removals along one axis
            // this works because we add classifiers in order of increasing threshold
            // if the classifiers are reordered this won't work
            if( !strongClassifier.IsSameClassOfClassifier( i, j ) )
                break;

            if( !results.IsSameResults( i, j ) )
                break;
        }

        if( j == T )
            continue;
        
        // remove all but the last one of them
        // the idea is to preserve the classifier that can potentially give a separation plane only
        // of course it's not guaranteed to always work well
        for( UINT k=i + 1; k < j - 1; ++k )
        {
            toRemove[ k ] = 1;
            ++uDropped;
        }
    }

    printf( "\t\tcompacting classifiers (dropped=%d)...\n", uDropped );

    // now remove maintaining order, and reevanluate
    for( INT i=T - 1; i >= 0; --i )
    {
        if( toRemove[ i ] )
            strongClassifier.RemoveWeakClassifierMaintainOrder( i );
    }

    printf( "OptimiseWeakClassifers done (%.2f seconds)\n", ((FLOAT)(GetTickCount() - dwStart) / 1000.f) );
    fflush( stdout );

    return strongClassifier.GetNumWeakClassifiers();
}


//--------------------------------------------------------------------------------------
// Name: GetClassifierW
// Desc: Simply a long dot product using cached result indices into the array of weights
//       this calculates probability of error. An optimisation here is to add values
//       beforehand to be able to sum runs of consequtive indices quicker.
//--------------------------------------------------------------------------------------
__forceinline
HOCDetectorTrainer::Real  HOCDetectorTrainer::GetClassifierW( const std::vector< Real >(& w)[ MAX_LENGTH_OF_RUN ], const CachedResults& results, UINT uIndex )
{
    Real fError =  0.0f;

    for( UINT i=0; i < MAX_LENGTH_OF_RUN; ++i )
    {
        const Real* pW = &w[ i ][ 0 ];
        UINT uNumToSum = static_cast< UINT >( results.m_sumOverIndices[ i ][ uIndex ].size() );
        if( uNumToSum < 2 )
            continue;

        uNumToSum -= 1; // because [ 0 ] is not a real index but a P/N indicator

        const CachedResults::INDEX* pIndices = &results.m_sumOverIndices[ i ][ uIndex ][ 1 ];

        for( UINT n = 0; n < uNumToSum; n++ )
            fError += pW[ *pIndices++ ];
    }

    // W+ + W- = 1, we either get W- or W+ from the above sum, and we've got to output W-
    // so subtract W+ from 1 to get W-
    if( results.m_sumOverIndices[ 0 ][ uIndex ][ 0 ] == 1 )
        fError = 1 - fError;

    return fError;
}

//--------------------------------------------------------------------------------------
// Name: ComputeSums
// Desc: Given an array of real values, calculate other arrays where the values are summed
//--------------------------------------------------------------------------------------
VOID    HOCDetectorTrainer::ComputeSums( std::vector< Real >(& w)[ MAX_LENGTH_OF_RUN ] )
{
    const UINT uNumItems = static_cast< UINT >( w[ 0 ].size() );
    for( UINT i=1; i < MAX_LENGTH_OF_RUN; ++i )
    {
        w[ i ].resize( uNumItems );
    }

    for( UINT k=0; k < uNumItems; ++k )
    {
        Real fSum = w[ 0 ][ k ];

        for( UINT j=1; j < MAX_LENGTH_OF_RUN; ++j )
        {
            if( j + k >= uNumItems )
                break;

            fSum += w[ 0 ][ k + j ];
            w[ j ][ k ] = fSum;
        }
    }
}


//
//
//
void HOCDetectorTrainer::FindBestClassifier( UINT& uBestWeakClassifierIndex, Real& fMinError,
                                                const std::vector< BYTE >& classifierChosen,
                                                std::vector< Real > (&w)[ MAX_LENGTH_OF_RUN ],
                                                const CachedResults& results )
{
    const UINT T = static_cast< UINT >( classifierChosen.size() );  // Number of weak classifiers

    // iterate through all weak classifiers
    // leave openMP to handle threading
    {
        // "thread local" variables to enable us to run this block in parallel
        Real  fLocalMinError = FLT_MAX;
        Real  fLocalMaxAbsError = 0;
        UINT  uLocalBest = 0;

        // tell openMP that it has to create jobs not smaller than 1000 iterations each
#ifdef OMP
#pragma omp parallel for  schedule( dynamic )
#endif
        for( INT i = 0; i < (INT)T; i++ )
        {
            if( classifierChosen[ i ] )
                continue;

            const Real fError = GetClassifierW( w, results, i );
            if( fError < fLocalMinError )
            {
                fLocalMinError = fError;
                uLocalBest = i;
            }
        }

        // serial code
        if( fLocalMinError < fMinError )
        {
            fMinError = fLocalMinError;
            uBestWeakClassifierIndex = uLocalBest;
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: RealAdaBoost
// Desc: Ada boost training. Real generalisation.
//--------------------------------------------------------------------------------------
void    HOCDetectorTrainer::RealAdaBoost( HOCStrongClassifier& strongClassifier, const CachedResultsR& results, const HOCSamplesSet& trainingSet, const Parameters& params )
{
    const HOCExtraClassifierData ee;

    const UINT T = static_cast< UINT >( strongClassifier.GetNumWeakClassifiers() );  // Number of weak classifiers
    const UINT N = static_cast< UINT >( trainingSet.m_examples.size() );  // Number of examples in training set
    assert( N < 65535 );    // use INT instead of SHORT in failIndices if this fires

    std::vector< HOCWeakClassifierR >    finalClassifier;

    // set alphas to 0
    for( UINT i=0; i < T; ++i )
        strongClassifier.GetWeakClassifierR( i ).SetAlpha( 0 );

    // starting distributed weight for each example
    // note here that if N becomes large, use of double is recommended
    std::vector< Real > w = std::vector< Real >( N, 1.f / N );

    // 1/N will mean that by inputting more samples of one class the FP and FN can be biased
    if( 0 )
    {
        // below will renormalize the weights so the output FP/FN is close to 1
        UINT uNegs = 0, uPoss = 0;
        for( UINT i=0; i < N; ++i )
        {
            if( trainingSet.m_labels[ i ] > 0 )
                ++uPoss;
            else
                ++uNegs;
        }

        FLOAT fNegScale = 1.f / (2 * FLOAT( uNegs ) );
        FLOAT fPosScale = 1.f / (2 * FLOAT( uPoss ) );
        for( UINT i=0; i < N; ++i )
        {
            if( trainingSet.m_labels[ i ] > 0 )
                w[ i ] = fPosScale;
            else
                w[ i ] = fNegScale;
        }
    }

    const INT uNumIterations = 2 * T;
    finalClassifier.reserve( uNumIterations );

    for( INT t = 0; t < uNumIterations; t++ )
    {
        // Choose the best weak classifier h that minimize the error on traning examples
        UINT uBestWeakClassifierIndex = 0;
        Real fMinZ = FLT_MAX;
        Real fMaxZ = 0;
        Real fMaxAbsValue = 0;

#ifdef OMP
#pragma omp parallel for
#endif
        for( INT i = 0; i < (INT)T; i++ )
        {
            Real fMaxAbsValueLocal = 0;
            const Real fZ = FindMinMaxAndResponse( strongClassifier.GetWeakClassifierR( i ), trainingSet, w, fMaxAbsValueLocal, i, results );

            if( fZ < fMinZ )
            {
#ifdef OMP
#pragma omp critical
#endif
                if( fZ < fMinZ )
                {
                    fMinZ = fZ;
                    uBestWeakClassifierIndex = i;
                    fMaxAbsValue = fMaxAbsValueLocal;
                }
            }

            if( fZ > fMaxZ )
            {
#ifdef OMP
#pragma omp critical
#endif
                if( fZ > fMaxZ )
                    fMaxZ = fZ;
            }
        }

        if( fMinZ >= 0.99999f )
        {
            printf( "Num iterations: %d (error %f)\n", t, fMinZ );
            break;
        }

        HOCWeakClassifierR& classifier = strongClassifier.GetWeakClassifierR( uBestWeakClassifierIndex );

        std::vector< FLOAT >    htiv( N );

        // get mu_t
        Real fMu_t_Sum = 0;
        for( UINT i=0; i < N; ++i )
        {
            // w_t,i * y_i * h_t( x_i )
            FLOAT hti = classifier.ClassifyR( results.m_values[ uBestWeakClassifierIndex ][ i ] );

            FLOAT yi = trainingSet.m_labels[ i ];

            htiv[ i ] = hti * yi;

            fMu_t_Sum += w[ i ] * yi * hti;
        }
        const Real fMu_t = fMu_t_Sum / fMaxAbsValue;

        // get the alpha. it will only penalise good examples if it reached the margin
        const Real fAlpha = log( (1 + fMu_t) / (1 - fMu_t) ) / (2 * fMaxAbsValue);
        classifier.SetAlpha( static_cast< FLOAT >( fAlpha ) );

        finalClassifier.push_back( classifier );

        classifier.SetAlpha( 0 );

        // Emphasize the training examples that do not agree with h
        const Real fMul2 = 1.0 / ( (1.0 - fMu_t * fMu_t) );
        const Real fMul1 = fMu_t / fMaxAbsValue;
        const Real fMul3 = fMul1 * fMul2;
        Real Z = 0.0;
        for( UINT i = 0; i < N; ++i )
        {
            Real hti = htiv[ i ];

            Real ww = w[ i ] * (fMul2 - fMul3 * hti);

            w[ i ] = ww;

            Z += ww;
        }

        // normalize to a probability distribution
        Real invZ = Real( 1 ) / Z;
        for( INT i = 0; i < (INT)N; ++i )
            w[ i ] *= invZ;

        // progress
        {
            printf( "RealAdaBoost: iter %d/%d, fMinZ = %f, fMaxZ = %f, fMaxAbsValue = %f\n", t, uNumIterations, fMinZ, fMaxZ, fMaxAbsValue );
            fflush( stdout );
        }
    }

    strongClassifier.m_weakClassifiersR.swap( finalClassifier );
}


//--------------------------------------------------------------------------------------
// Name: DiscreteAdaBoost
// Desc: Ada boost training
//--------------------------------------------------------------------------------------
void    HOCDetectorTrainer::DiscreteAdaBoost( HOCStrongClassifier& strongClassifier, const CachedResults& results, const HOCSamplesSet& trainingSet, const Parameters& params )
{
    const UINT T = static_cast< UINT >( strongClassifier.GetNumWeakClassifiers() );  // Number of weak classifiers
    const UINT N = static_cast< UINT >( trainingSet.m_examples.size() );  // Number of examples in training set
    assert( N < 65535 );    // use INT instead of SHORT in failIndices if this fires

    std::vector< BYTE > classifierChosen( T, 0 );

    // set alphas to 0
    for( UINT i=0; i < T; ++i )
        strongClassifier.GetWeakClassifierD( i ).SetAlpha( 0 );

    // starting distributed weight for each example
    // note here that if N becomes large, use of double is recommended
    std::vector< Real > w[ MAX_LENGTH_OF_RUN ] = { std::vector< Real >( N, 1.f / N ) };
    ComputeSums( w );

    for( INT t = 0; t < (INT)T; t++ )       // limit to 2000 or something
    {
        // Choose the best weak classifier h that minimize the error on traning examples
        UINT uBestWeakClassifierIndex = 0;
        Real fMinError = FLT_MAX;

        FindBestClassifier( uBestWeakClassifierIndex, fMinError, classifierChosen, w, results );

        // A weak classifier's error has to be less than 0.5 to contribute successfully
        if( fMinError >= params.m_fErrorThreshold )
        {
            printf( "Num iterations: %d\n", t );
            break;
        }

        // The weak learner returned a weak classifier h
        classifierChosen[ uBestWeakClassifierIndex ] = TRUE;

        HOCWeakClassifierD& classifier = strongClassifier.GetWeakClassifierD( uBestWeakClassifierIndex );

        // Get the confidence of the weak classifier as alpha (lower error => higher alpha)
        const Real fAlpha = (0.5 * log( (1 - fMinError) / fMinError ));      // it's necessary that W- + W+ + W0 = 1
        classifier.SetAlpha( static_cast< FLOAT >( fAlpha ) );

        // Emphasize the training examples that do not agree with h
        ResultsVector::Iterator ii = results.m_results[ uBestWeakClassifierIndex ].Begin();
        Real Z = 0.0;
        for( UINT i = 0; i < N; ++i )
        {
            w[ 0 ][ i ] *= exp( -fAlpha * ii.GetResultAndAdvance() );
            Z += w[ 0 ][ i ];
        }

        // normalize to a probability distribution
        for( INT i = 0; i < (INT)N; ++i )
            w[ 0 ][ i ] /= Z;

        ComputeSums( w );        

        // progress
        if( 0 == (t & params.m_uProgressTick ) )
        {
            printf( "Discrete AdaBoost: %.2f%%, selected %d weak classifiers, fMinError = %f\n", 100.f * (FLOAT)t/(FLOAT)T, t, fMinError );
            fflush( stdout );
        }
    }
}



//--------------------------------------------------------------------------------------
// Name: RBoost
// Desc: Regularised boost
//--------------------------------------------------------------------------------------
void    HOCDetectorTrainer::RBoost( HOCStrongClassifier& strongClassifier, const CachedResults& results, const HOCSamplesSet& trainingSet, const Parameters& params )
{
    const UINT T = static_cast< UINT >( strongClassifier.GetNumWeakClassifiers() );  // Number of weak classifiers
    const UINT N = static_cast< UINT >( trainingSet.m_examples.size() );  // Number of examples in training set
    assert( N < 65535 );    // use INT instead of SHORT in failIndices if this fires

    // Normalize the weak classifier weights, so that we get a probability value in the range [-1..1]
    Real fTotalAlpha = 0;
    for( UINT i = 0; i < T; i++ )
    {
        fTotalAlpha += strongClassifier.GetWeakClassifierD( i ).GetAlpha();
    }

    // if total weight is zero then RBoost has no work
    if( fTotalAlpha == 0 )
        return;

    // RBoost starts with non-uniform weights for the classifiers
    // it generates them similarly to what classification would do
    // it's possible to naively initialize this as well
    std::vector< Real > w[ MAX_LENGTH_OF_RUN ] = { std::vector< Real >( N ) };

    Real fSumWeight = 0;
    for( UINT i=0; i < N; ++i )
    {
        Real v = 0;
        for( UINT j=0; j < T; ++j )
        {
            v += strongClassifier.GetWeakClassifierD( j ).GetAlpha() * results.m_results[ j ].GetResult( i );
        }

        // keep alpha reasonably small
        w[ 0 ][ i ] = exp( -trainingSet.m_labels[ i ] * v / fTotalAlpha );
        fSumWeight += w[ 0 ][ i ];
    }

    // normalize weights
    for( UINT i=0; i < N; ++i )
        w[ 0 ][ i ] /= fSumWeight;

    ComputeSums( w );

    // RBoost is almost the same as AdaBoost, only we demote classifiers as well as promote them
    for( INT t = 0; t < (INT)T; t++ )
    {
        // Choose the best weak classifier h that minimize the error on traning examples
        UINT uBestWeakClassifierIndex = 0;
        UINT uWorstWeakClassifierIndex = 0;
        Real fMinEdge = FLT_MAX;
        Real fMaxEdge = 0;

        // Iterate through all weak classifiers
        {
            // "thread local" variables to enable us to run this block in parallel
            Real  fLocalMinEdge = FLT_MAX;
            Real  fLocalMaxEdge = 0;
            UINT  uLocalBest = 0;
            UINT  uLocalWorst = 0;

#ifdef OMP
#pragma omp parallel for  schedule( dynamic )
#endif
            for( INT i = 0; i < (INT)T; i++ )
            {
                const Real fError = GetClassifierW( w, results, i );
                const Real fEdge = 1 - 2.f * fError;

                if( fEdge > fLocalMaxEdge )
                {
                    fLocalMaxEdge = fEdge;
                    uLocalBest = i;
                }

                // to be considered for the "worst", the classifier should have a non zero alpha
                if( strongClassifier.GetWeakClassifierD( i ).GetAlpha() > 0 )
                {
                    if( fEdge < fLocalMinEdge )
                    {
                        fLocalMinEdge = fEdge;
                        uLocalWorst = i;
                    }
                }
            }

            // pick the best and the worst

            if( fLocalMinEdge < fMinEdge )
            {
                fMinEdge = fLocalMinEdge;
                uWorstWeakClassifierIndex = uLocalWorst;
            }

            if( fLocalMaxEdge > fMaxEdge )
            {
                fMaxEdge = fLocalMaxEdge;
                uBestWeakClassifierIndex = uLocalBest;
            }
        }

        // these are y_i * h( x_i ) premultiplied, i.e. +1 if it was "correct" and -1 if it was "wrong"
        ResultsVector::Iterator ii = results.m_results[ uBestWeakClassifierIndex ].Begin();
        ResultsVector::Iterator jj = results.m_results[ uWorstWeakClassifierIndex ].Begin();

        // RBoost probabilty here -- it measures how much better hk is compared to hl
        Real  fYHKGreaterYHL = 0;
        Real  fYHLGreaterYHK = 0;
        for( UINT i=0; i < N; ++i )
        {
            const INT   iResultK = ii.GetResultAndAdvance();
            const INT   iResultL = jj.GetResultAndAdvance();

            if( iResultK == 1 && iResultL == -1 )
                fYHKGreaterYHL += w[ 0 ][ i ];

            if( iResultK == -1 && iResultL == 1 )
                fYHLGreaterYHK += w[ 0 ][ i ];
        }

        // Get the confidence of the weak classifier as alpha (lower error => higher alpha)
        HOCWeakClassifierD& k = strongClassifier.GetWeakClassifierD( uBestWeakClassifierIndex );
        HOCWeakClassifierD& l = strongClassifier.GetWeakClassifierD( uWorstWeakClassifierIndex );

        const Real fCurAlphaK = k.GetAlpha();
        const Real fCurAlphaL = l.GetAlpha();

        // max here is to avoid making alpha negative in L
        const Real fEpsilon = std::min( 2. * fCurAlphaL, 0.5 * log( fYHKGreaterYHL / fYHLGreaterYHK ) );

        // adjust classifier weights
        k.SetAlpha( static_cast< FLOAT >( fCurAlphaK + fEpsilon / 2. ) );
        l.SetAlpha( static_cast< FLOAT >( fCurAlphaL - fEpsilon / 2. ) );

        ii = results.m_results[ uBestWeakClassifierIndex ].Begin();
        jj = results.m_results[ uWorstWeakClassifierIndex ].Begin();

        // adjust the weights of the training examples
        Real Z = 0.0;
        for( UINT i = 0; i < N; ++i )
        {
            const INT   iResultK = ii.GetResultAndAdvance();
            const INT   iResultL = jj.GetResultAndAdvance();

            if( iResultK != iResultL )
            {
                if( iResultK < iResultL )
                    w[ 0 ][ i ] *= exp( fEpsilon );
                else
                    w[ 0 ][ i ] *= exp( -fEpsilon );
            }

            Z += w[ 0 ][ i ];
        }

        // normalize to a probability distribution
        for( INT i = 0; i < (INT)N; ++i )
            w[ 0 ][ i ] /= Z;

        ComputeSums( w );

        // progress
        if( 0 == (t & params.m_uProgressTick)   ||
            t == T - 1 )
        {
            printf( "RBoost: %.2f%%, selected %d weak classifiers, fMinEdge = %f, fMaxEdge = %f\n", 100.f * (FLOAT)t/(FLOAT)T, t, fMinEdge, fMaxEdge );
            fflush( stdout );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: TrainStrongClassifier
// Desc: Ada boost training. First we run AdaBoost on the entire set. Then we
// drop classifier instances with low alpha and feed the dramatically reduced set of
// classifier intances to RBoost which reduces it even more
//--------------------------------------------------------------------------------------
HRESULT HOCDetectorTrainer::TrainStrongClassifier( HOCStrongClassifier& classifier, const HOCSamplesSet& trainingSet, const HOCExtraClassifierData& ensemble, const Parameters& params )
{
    printf( "\n\t-Running AdaBoost learning algorithm...\n" );

    const DWORD dwStart = GetTickCount();

    UINT T = classifier.GetNumWeakClassifiers();

    // real training
    if( params.m_bRealAdaboost )
    {
        CachedResultsR  results;
        ComputeClassifiersResultsR( results, classifier, trainingSet, ensemble );

        printf( "\n\t\t-Ada Boost R with all classifiers...\n" );
        fflush( stdout );
        RealAdaBoost( classifier, results, trainingSet, params );

        // chop off the tail, rebuild the results for RBoost
        printf( "\n\t\t-Optimise...\n" );
        fflush( stdout );
        OptimizeByAlpha( classifier, params );
    } else
    {
        // classifiers return same results on each iteration, cache them
        CachedResults   results;
        if( params.m_bOptimisationLeaveOnlyBreakpoints )
        {
            // this will always maintain their ordering
            T = OptimiseWeakClassifers( results, classifier, trainingSet, ensemble );
        }

        // this will only leave classifiers that survived the previous step, should be fewer
        ComputeClassifiersResults( results, classifier, trainingSet, ensemble, TRUE, TRUE );

        printf( "\n\t\t-Ada Boost with all classifiers...\n" );
        fflush( stdout );
        DiscreteAdaBoost( classifier, results, trainingSet, params );

        // chop off the tail, rebuild the results for RBoost
        printf( "\n\t\t-Optimise...\n" );
        fflush( stdout );
        OptimizeByAlpha( classifier, params );

        if( params.m_bUseRBoost )
        {
            // need to redo the results after the optimisation step
            ComputeClassifiersResults( results, classifier, trainingSet, ensemble, TRUE, TRUE );
    
            // run RBoost to optimise the rest
            printf( "\n\t\t-RBoost...\n" );
            fflush( stdout );
            RBoost( classifier, results, trainingSet, params );

            // chop off the tail again, no need to recalculate the results here
            OptimizeByAlpha( classifier, params );
        }
    }

    // Compute the normalization coefficient so we get results E [-1..1]
    classifier.ComputeTotalAlpha();

    printf( "TotalAlpha=%f\nDone (%.2f seconds)\n", classifier.GetTotalAlpha(), ((FLOAT)(GetTickCount() - dwStart) / 1000.f) );

    // take top N classifiers and print out their total weight and thresholds
    PrintTopClassifiers( classifier, 5, 5 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Optimise
// Desc: OptimizeByAlpha for runtime by reducing the number of weak classifiers that contribute
//       to the strong classifier. We simply sort the weak classifiers based on the
//       weights in the strong classifier and take the first N classifiers
//--------------------------------------------------------------------------------------
template< class T >
void HOCStrongClassifier::OptimizeByAlpha( T& weakClassifiers, UINT uMaxClassifiers, FLOAT fAlphaCutoff, UINT uPrintMax )
{
    if( weakClassifiers.empty() )
        return;

    // Sort using alpha
    std::sort( weakClassifiers.rbegin(), weakClassifiers.rend() );

    // calculate alpha cut off as percentage of the top classifier's alpha
    const FLOAT fCutoff = weakClassifiers[ 0 ].GetAlpha() * fAlphaCutoff;
    printf( "Cutoff value for %.2f%% is %f\n", fAlphaCutoff * 100, fCutoff );

    // Now throw away all classifiers with zero alpha
    const UINT nNumWeakClassifiers = static_cast< UINT >( weakClassifiers.size() );
    UINT lastValid;
    for( lastValid = 0; lastValid < nNumWeakClassifiers; ++lastValid )
    {
        if( weakClassifiers[ lastValid ].GetAlpha() <= fCutoff )
            break;
    }

    weakClassifiers.resize( lastValid );

    // Now prune to the max number of classifers asked, prune the classifiers with the lowest alphas
    if( uMaxClassifiers > 0 &&
        uMaxClassifiers < weakClassifiers.size() )
    {
        weakClassifiers.resize( uMaxClassifiers );
    }

    // debug print
    for( UINT i=0; i < std::min( uPrintMax, static_cast< UINT >( weakClassifiers.size() ) ); ++i )
    {
        HOCPrintClassifier( weakClassifiers[ i ], i );
    }
}

//--------------------------------------------------------------------------------------
// Name: OptimiseByAlphaD
// Desc: OptimizeByAlpha for runtime by reducing the number of weak classifiers that contribute
//       to the strong classifier. We simply sort the weak classifiers based on the
//       weights in the strong classifier and take the first N classifiers
//--------------------------------------------------------------------------------------
void HOCStrongClassifier::OptimizeByAlpha( UINT uMaxClassifiers, FLOAT fAlphaCutoff, UINT uPrintMax )
{
    if( m_bUseRealAdaboost )
        OptimizeByAlpha( m_weakClassifiersR, uMaxClassifiers, fAlphaCutoff, uPrintMax );
    else
        OptimizeByAlpha( m_weakClassifiersD, uMaxClassifiers, fAlphaCutoff, uPrintMax );
}

//--------------------------------------------------------------------------------------
// Name: ComputeClassifierSplits
// Desc: Finds out whether it's possible to create a split node that would unambiguously
// identify the class of the sample
//--------------------------------------------------------------------------------------
UINT HOCDetectorTrainer::ComputeClassifierSplits( HOCWeakClassifierD pSplit[ 2 ],
                                                  FLOAT& fScore,
                                                  const HOCWeakClassifierD& c,
                                                  const HOCSamplesSet& trainingSet )
{
    HOCExtraClassifierData  emptyEnsemble;

    const UINT N = static_cast< UINT >( trainingSet.m_examples.size() );  // Number of examples in training set

    pSplit[ 0 ] = pSplit[ 1 ] = c;

    // inner -- min/max, outer -- split0 or split1
    FLOAT bounds[ 2 ][ 2 ] = { { FLT_MAX, -FLT_MAX }, { FLT_MAX, -FLT_MAX } };

    // the point right on the edge should be consider to belong to both classes
    static const FLOAT EPSILON = 1.001f;

    // drop all the samples onto the classifier's axis and get the bounds of the data
    for( UINT j=0; j < N; ++j )
    {
        const UINT  uLabel = trainingSet.m_labels[ j ] > 0 ? 1 : 0;
        const HOCDataViews& data = trainingSet.m_examples[ j ];
        const FLOAT fValue = c.GetDataValue( data, emptyEnsemble );

        bounds[ uLabel ][ 0 ] = std::min( bounds[ uLabel ][ 0 ], fValue );
        bounds[ uLabel ][ 1 ] = std::max( bounds[ uLabel ][ 1 ], fValue );
    }

    // this prevents from bad classifiers to generate all encompassing wrong rules
    if( bounds[ 0 ][ 0 ] == bounds[ 1 ][ 0 ] &&
        bounds[ 0 ][ 1 ] == bounds[ 1 ][ 1 ] )
    {
        fScore = 0;
        printf( "Warning: degenerate classifier found, ignoring\n\t" );
        HOCPrintClassifier( c, 0 );
        printf( "Make sure this classifier doesnt just return the same value for all samples\n" );
        return 0;
    }

    // see if there is a significant clear cut separation we can glimpse from the data
    // we only check for two intervals here. theoretically each interval can come from
    // a cluster, but we don't handle those here because our data is well behaved.
    if( bounds[ 0 ][ 0 ] < bounds[ 1 ][ 0 ] )
    {
        // we create a decision stump here with a threshold of the split
        // and reversed because class labeled -1 is to the left and both
        // classes (1 and -1) are to the right. we don't want the mix of two --
        // we need a clean class
        pSplit[ 0 ].SetRuleToClassIfLessThan( bounds[ 1 ][ 0 ] * EPSILON, -1 );
    } else
    {
        pSplit[ 0 ].SetRuleToClassIfLessThan( bounds[ 0 ][ 0 ] * EPSILON, 1 );
    }
    
    // handle the top end of the range
    if( bounds[ 0 ][ 1 ] < bounds[ 1 ][ 1 ] )
    {
        pSplit[ 1 ].SetRuleToClassIfGreaterThan( bounds[ 0 ][ 1 ] * EPSILON, 1 );
    } else
    {
        pSplit[ 1 ].SetRuleToClassIfGreaterThan( bounds[ 1 ][ 1 ] * EPSILON, -1 );
    }

    // get the score for each split
    FLOAT   fSingleClass[ 2 ] = { 0 };

    // drop all the samples onto the classifier's axis and get the bounds of the data
    for( UINT j=0; j < N; ++j )
    {
        const HOCDataViews& data = trainingSet.m_examples[ j ];

        const FLOAT fValue = pSplit[ 0 ].GetDataValue( data, emptyEnsemble );

        const INT   iRes0 = pSplit[ 0 ].Classify( fValue );
        const INT   iRes1 = pSplit[ 1 ].Classify( fValue );

        if( pSplit[ 0 ].m_ruleOutcome == iRes0 )
            fSingleClass[ 0 ]++;

        if( pSplit[ 1 ].m_ruleOutcome == iRes1 )
            fSingleClass[ 1 ]++;
    }

    // we counted how many training examples we can separate, get the score now
    const FLOAT fInvN = 1.f / (FLOAT)N;
    const FLOAT fScore0 = fSingleClass[ 0 ] * fInvN;
    const FLOAT fScore1 = fSingleClass[ 1 ] * fInvN;

    // only pick one
    if( fScore1 > fScore0 )
    {
        pSplit[ 0 ] = pSplit[ 1 ];
        fScore = fScore1;
    } else
    {
        fScore = fScore0;
    }

    return 1;
}

//--------------------------------------------------------------------------------------
// Name: FindBestSplit
// Desc: We can train AdaBoost and pick the top classifier. Or we can find which axis
//       provides the best split
//--------------------------------------------------------------------------------------
UINT HOCDetectorTrainer::FindBestSplit( HOCWeakClassifierD splits[ 2 ],
                                        const HOCSamplesSet& trainingSet,
                                        const HOCStrongClassifier& classifier,
                                        const FLOAT fSignificance )
{
    FLOAT   fBestScore = fSignificance;
    UINT uNumClassifiers = 0;

    const UINT uNumClassifiersToTest = classifier.GetNumWeakClassifiers();
    for( UINT i=0; i < uNumClassifiersToTest; ++i )
    {
        FLOAT   fCurScore;
        HOCWeakClassifierD curSplit[ 2 ];
        const UINT uNumCurClassifiers = ComputeClassifierSplits( curSplit, fCurScore, classifier.GetWeakClassifierD( i ), trainingSet );

        if( fCurScore > fBestScore )
        {
            splits[ 0 ] = curSplit[ 0 ];
            splits[ 1 ] = curSplit[ 1 ];
            uNumClassifiers = uNumCurClassifiers;
            fBestScore = fCurScore;
        }
    }

    // make sure we don't create a decision tree node split for something insignificant
    if( uNumClassifiers )
    {
        printf( "\t\t...A %.2f%% split is possible using this classifier\n\t\t", fBestScore * 100.f );
        HOCPrintClassifier( splits[ 0 ], 0 );
    }

    return uNumClassifiers;
}



//--------------------------------------------------------------------------------------
// Name: BuildRulesAndAdjustTrainingSet
// Desc: Build a few levels of decision tree if possible to get rid of strong signals
//--------------------------------------------------------------------------------------
void HOCDetectorTrainer::BuildRulesAndAdjustTrainingSet( HOCWeakClassifiersVectorD& rules,
                                                         HOCSamplesSet& trainingSet,
                                                         const HOCStrongClassifier& classifier,
                                                         const UINT uNumRules,
                                                         const FLOAT fSignificance )
{
    HOCExtraClassifierData emptyEnsemble;

    for( UINT i=0; i < uNumRules; ++i )
    {
        printf( "\tComputing a best axis for the %d level split\n", i );

        HOCWeakClassifierD split[ 2 ];
        const UINT uNumSplits = FindBestSplit( split, trainingSet, classifier, fSignificance );
        if( !uNumSplits )
        {
            printf( "\t\tDidn't find a good split\n" );
            break;
        }

        // add a node
        rules.push_back( split[ 0 ] );

        UINT    uCount = 0;

        // weed out samples from the training set that are at a leaf child of the node
        for( UINT j=0; j < (UINT)trainingSet.m_examples.size(); ++j )
        {
            if( split[ 0 ].SimpleOutcome( trainingSet.m_examples[ j ], emptyEnsemble ) )
            {
                // this is much faster than collapsing the array
                trainingSet.m_examples[ j ] = trainingSet.m_examples.back();
                trainingSet.m_labels[ j ] = trainingSet.m_labels.back();
                trainingSet.m_labels.pop_back();
                trainingSet.m_examples.pop_back();

                ++uCount;
            }
        }

        printf( "\t\tTaken out %d training samples that can be unambiguously classified by a rule\n", uCount );
    }
}




//--------------------------------------------------------------------------------------
// Name: UpdateClassifierExtents
// Desc: Get min and max for the classifiers response for the entire training set
//--------------------------------------------------------------------------------------
void    HOCDetectorTrainer::HOCClassifierExtents::UpdateClassifierExtents( const HOCBaseWeakClassifier& c,
                                                                           const HOCSamplesSet& trainingSet,
                                                                           const HOCExtraClassifierData& ensemble )
{
    assert( c.m_type < _countof( m_bounds ) );
    FLOAT* pfBounds = m_bounds[ c.m_type ];

    const UINT N = static_cast< UINT >( trainingSet.m_examples.size() );  // Number of examples in training set

    for( UINT j=0; j < N; ++j )
    {
        const HOCDataViews& data = trainingSet.m_examples[ j ];
        const FLOAT fValue = c.GetDataValue( data, ensemble );

        pfBounds[ 0 ] = std::min( pfBounds[ 0 ], fValue );
        pfBounds[ 1 ] = std::max( pfBounds[ 1 ], fValue );
    }
}


//--------------------------------------------------------------------------------------
// Name: ComputeClassifierExtents
// Desc: Get min and max for the classifiers response for the entire training set
//--------------------------------------------------------------------------------------
void    HOCDetectorTrainer::HOCClassifierExtents::ComputeClassifierExtents( HOCBaseWeakClassifier::Type type, 
                                                                            const HOCSamplesSet& trainingSet,
                                                                            const HOCExtraClassifierData& ensemble )
{
    HOCBaseWeakClassifier c;
    c.m_dwTypeAndData = 0;
    c.m_type = type;

    assert( c.m_type < _countof( m_bounds ) );
    FLOAT* pfBounds = m_bounds[ c.m_type ];
    pfBounds[ 0 ] = FLT_MAX;
    pfBounds[ 1 ] = -FLT_MAX;

    if( type >= HOCBaseWeakClassifier::TYPE_REQUIRES_TWO_BUCKETS    &&
        type < HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET )
    {
        for( UINT i=0; i < HOCComputedData::NUM_BUCKETS; ++i )
        {
            c.m_bucketIndices[ 0 ] = static_cast< BYTE >( i );

            for( UINT k=0; k < HOCComputedData::NUM_BUCKETS; ++k )
            {
                c.m_bucketIndices[ 1 ] = static_cast< BYTE >( k );
                UpdateClassifierExtents( c, trainingSet, ensemble );
            }
        }
    } else  if( type >= HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET  &&
                type < HOCBaseWeakClassifier::TYPE_REQUIRES_NO_BUCKETS )
    {
        for( UINT i=0; i < HOCComputedData::NUM_BUCKETS; ++i )
        {
            c.m_bucketIndices[ 0 ] = static_cast< BYTE >( i );
            UpdateClassifierExtents( c, trainingSet, ensemble );
        }
    } else
    {
        UpdateClassifierExtents( c, trainingSet, ensemble );
    }
}


//--------------------------------------------------------------------------------------
// Name: ComputeDimensionsExtents
// Desc: Get values for each axis (classifier)
//--------------------------------------------------------------------------------------
void HOCDetectorTrainer::HOCClassifierExtents::ComputeDimensionsExtents( const HOCSamplesSet& trainingSet )
{
    HOCExtraClassifierData emptyEnsemble;

    // this allows us to get rid of magical scalers in HOCDetector::ComputeData
    for(    HOCBaseWeakClassifier::Type i = HOCBaseWeakClassifier::TYPE_FIRST_CLASSIFER;
            i < HOCBaseWeakClassifier::TYPE_LAST_SIMPLE_CLASSIFIER;
            ++i )
    {
        ComputeClassifierExtents( i, trainingSet, emptyEnsemble );

        printf( "%s range %f %f\n", HOCGetClassiferName( i ), m_bounds[ i ][ 0 ], m_bounds[ i ][ 1 ] );
    }
}


//--------------------------------------------------------------------------------------
// Name: ComputeDimensionsExtents
// Desc: Get values for each axis (classifier)
//--------------------------------------------------------------------------------------
void HOCDetectorTrainer::HOCClassifierExtents::ComputeDimensionsExtents( const HOCStrongClassifier& c, const HOCSamplesSet& trainingSet )
{
    HOCExtraClassifierData emptyEnsemble;

    for(    HOCBaseWeakClassifier::Type i = HOCBaseWeakClassifier::TYPE_FIRST_CLASSIFER;
            i < HOCBaseWeakClassifier::TYPE_LAST_SIMPLE_CLASSIFIER;
            ++i )
    {
        FLOAT* pfBounds = m_bounds[ i ];
        pfBounds[ 0 ] = FLT_MAX;
        pfBounds[ 1 ] = -FLT_MAX;
    }

    // this allows us to get rid of magical scalers in HOCDetector::ComputeData
    if( c.UsesRealAdaBoost() )
    {
        for( UINT i=0; i < c.GetNumWeakClassifiers(); ++i )
            UpdateClassifierExtents( c.GetWeakClassifierR( i ), trainingSet, emptyEnsemble );
    } else
    {
        for( UINT i=0; i < c.GetNumWeakClassifiers(); ++i )
            UpdateClassifierExtents( c.GetWeakClassifierD( i ), trainingSet, emptyEnsemble );
    }

    for(    HOCBaseWeakClassifier::Type i = HOCBaseWeakClassifier::TYPE_FIRST_CLASSIFER;
            i < HOCBaseWeakClassifier::TYPE_LAST_SIMPLE_CLASSIFIER;
            ++i )
    {
        printf( "%s range %f %f\n", HOCGetClassiferName( i ), m_bounds[ i ][ 0 ], m_bounds[ i ][ 1 ] );
    }
}

//--------------------------------------------------------------------------------------
// Name: OutputDataCSV
// Desc: Print out the data for external analysis
//--------------------------------------------------------------------------------------
HRESULT HOCDetectorTrainer::OutputDataCSV( const CHAR* pFilename, const CHAR** fileNames, const BOOL* pbOpenedOrClosed, const Parameters& params )
{
    FILE*   fp;

    if( fopen_s( &fp, pFilename, "wt" ) )
        return E_FAIL;

    HOCSamplesSet trainingSet;

    if( FAILED( HOCDetectorTrainer::ImportGroundTruthTrainingSet( trainingSet, fileNames, pbOpenedOrClosed, params.m_pfnSampleAcceptor, params.m_bVerbose ) ) )
        return E_FAIL;

    HOCExtraClassifierData emptyEnsemble;

    printf( "writing data into %s...\n", pFilename );

    // write out the header
    fprintf( fp, "label,numVoxels,distance" );
    for( HOCBaseWeakClassifier::Type i=HOCBaseWeakClassifier::TYPE_FIRST_SIMPLE_CLASSIFER; i < HOCBaseWeakClassifier::TYPE_LAST_SIMPLE_CLASSIFIER; ++i )
        fprintf( fp, ",%s", HOCGetClassiferName( i ) );

    for( HOCBaseWeakClassifier::Type type = HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET;
         type < HOCBaseWeakClassifier::TYPE_REQUIRES_NO_BUCKETS;
         ++type )
    {
        for( UINT i=0; i < HOCComputedData::NUM_BUCKETS; ++i )
            fprintf( fp, ",%s%d", HOCGetClassiferName( type ), i );
    }

    for( HOCBaseWeakClassifier::Type type = HOCBaseWeakClassifier::TYPE_REQUIRES_TWO_BUCKETS;
         type < HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET;
         ++type )
    {
        for( UINT i=0; i < HOCComputedData::NUM_BUCKETS; ++i )
            for( UINT j=i + 1; j < HOCComputedData::NUM_BUCKETS; ++j )
                fprintf( fp, ",%s%d_%d", HOCGetClassiferName( type ), i, j );
    }

    fprintf( fp, "\n" );

    // write out the data
    const UINT uNumData = trainingSet.GetNumExamples();
    for( UINT k=0; k < uNumData; ++k )
    {
        HOCBaseWeakClassifier   c;

        const HOCDataViews& dv = trainingSet.m_examples[ k ];

        fprintf( fp, "%d,%d,%f", trainingSet.m_labels[ k ], trainingSet.m_examples[ k ].m_sourceData.m_uNumVoxels, trainingSet.m_examples[ k ].m_transientData.m_vCentroid.z );

        // generic classifiers
        for( HOCBaseWeakClassifier::Type i=HOCBaseWeakClassifier::TYPE_FIRST_SIMPLE_CLASSIFER; i < HOCBaseWeakClassifier::TYPE_LAST_SIMPLE_CLASSIFIER; ++i )
        {
            // mean
            c.m_type = i;
            const FLOAT v = c.GetDataValue( dv, emptyEnsemble );

            fprintf( fp, ",%f", v );
        }

        // abs values
        for( HOCBaseWeakClassifier::Type type = HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET;
            type < HOCBaseWeakClassifier::TYPE_REQUIRES_NO_BUCKETS;
            ++type )
        {
            // ibuckets
            c.m_type = type;
            for( UINT i=0; i < HOCComputedData::NUM_BUCKETS; ++i )
            {
                c.m_bucketIndices[ 0 ] = i;
                const FLOAT v = c.GetDataValue( dv, emptyEnsemble );
                fprintf( fp, ",%f", v );
            }
        }

        // bucket diff
        for( HOCBaseWeakClassifier::Type type = HOCBaseWeakClassifier::TYPE_REQUIRES_TWO_BUCKETS;
             type < HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET;
             ++type )
        {
            c.m_type = type;
            for( UINT i=0; i < HOCComputedData::NUM_BUCKETS; ++i )
            {
                c.m_bucketIndices[ 0 ] = i;
                for( UINT j=i + 1; j < HOCComputedData::NUM_BUCKETS; ++j )
                {
                    c.m_bucketIndices[ 1 ] = j;
                    const FLOAT v = c.GetDataValue( dv, emptyEnsemble );
                    fprintf( fp, ",%f", v );
                }
            }
        }

        fprintf( fp, "\n" );
    }

    fclose( fp );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sign
// Desc: Returns the Sign of the floating point value
//--------------------------------------------------------------------------------------
static
BOOL    Sign( FLOAT v )
{
    return v > 0 ? 1 : 0;
}

//--------------------------------------------------------------------------------------
// Name: FindFilteringParameters
// Desc: Finds best filtering parameters (experimental)
//--------------------------------------------------------------------------------------
HRESULT HOCDetectorTrainer::FindFilteringParameters( UINT& uNumFrames,
                                                     FLOAT& fThreshold,
                                                     const HOCDetector& testDetector,
                                                     const CHAR** fileNames,
                                                     const BOOL* pbOpenedOrClosed,
                                                     const Parameters& params )
{
    // set it to 8 for now
    uNumFrames = 8;
    fThreshold = 0;

    // this is experimental -- we should be able to run our data through the test routine and find what threshold gives us
    // a reduction in false negatives
#if 1
    HOCSamplesSet testSet;
    if( FAILED( ImportGroundTruthTrainingSet( testSet, fileNames, pbOpenedOrClosed, params.m_pfnSampleAcceptor, params.m_bVerbose ) ) )
        return E_FAIL;

    const UINT uNumExamples = static_cast< UINT >( testSet.m_examples.size() );

    // get the results of detection
    std::vector< FLOAT >  confidence( uNumExamples );
    for( UINT i=0; i < uNumExamples; ++i )
    {
        const HOCDataViews& truth = testSet.m_examples[ i ];
        confidence[ i ] = testDetector.Detect( truth );
    }

    for( UINT uFrames=1; uFrames <= _countof( (((HOCFilter*)0)->m_fConfidenceHistory ) ); ++uFrames )
    {
        // iterate for each value of threshold
        for( FLOAT fThres = 0; fThres < 0.1f; fThres += 0.01f )
        {
            HOCFilter   filter;

            // ground truth
            const CHAR* __restrict pGroundTruthLabels = &testSet.m_labels[ 0 ];

            // prime filter otherwise there'll be confusing 0s in it
            for( UINT j=0; j < uNumFrames; ++j )
            {
                filter.FilterType0( confidence[ 0 ] );
            }

            TestResults result;
            result.m_uTruePositive = 0;
            result.m_uTrueNegative = 0;
            result.m_uFalsePositive = 0;
            result.m_uFalseNegative = 0;

            // run the data through the filter
            for( UINT i=1; i < uNumExamples; ++i )
            {
                filter.FilterType0( confidence[ i ], fThres, uFrames );

                // see if all examples in the window give are of the same class
                BOOL bTheSameClass = TRUE;
                for( UINT j=1; j < uFrames; ++j )
                {
                    if( Sign( filter.m_fConfidenceHistory[ 0 ] ) != Sign( filter.m_fConfidenceHistory[ j ] ) )
                    {
                        bTheSameClass = FALSE;
                        break;
                    }
                }

                // judge the filtered result
                if( bTheSameClass )
                {
                    if( Sign( filter.m_fFilteredConfidence ) == Sign( filter.m_fConfidenceHistory[ 0 ] ) )
                    {
                        ++result.m_uTruePositive;
                    } else
                    {
                        ++result.m_uFalseNegative;
                    }
                }
            }

            printf( "window %d threshold %f, TP %d, FN %d\n", uFrames, fThres, result.m_uTruePositive, result.m_uFalseNegative );
        }
    }
#endif
    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: Parameters
// Desc: Sets default/recommended training parameters
//--------------------------------------------------------------------------------------
void HOCDetectorTrainer::Parameters::SetDefault()
{
    // this may produce slightly different results but generally it's never worse off more than 0.1%
    // only 5 times faster to train. The idea of this optimisation is to only leave breakpoints along
    // the classifier's axes and remove the points inbetween. Another way to think of it is we remove
    // the instances of classifiers that don't carve out regions in the sample space.
    static const BOOL OPTIMISATION_LEAVE_ONLY_BREAKPOINTS = TRUE;

    // whether or not run RBoost -- recommended on. this reduces the number of final classifiers while
    // maintaining or slightly improving accuracy
    static const BOOL  USE_RBOOST = TRUE;

    // some ST based classifiers like the ratio of bone lengths rely on quirks of ST so with future
    // ST improvements they may become obsolete. make sure to give it a lot of ST-different data, including
    // when the hand occludes the shoulder, etc. ST based classifier assists much more in the far distance,
    // so it makes sense including it here for the ensemble classifier
    static const BOOL  USE_ST_BASED_CLASSIFIERS = FALSE;//TRUE;

    // split into N steps along each axis. space can be only carved out along these "ticks"
    // many items here doesn't mean higher accuracy but always means slower training
    static const UINT  NUM_THRESHOLD_STEPS = 128;

    // testing shows that 2 steps max in the set of rules is good for my training set.
    // a rule should produce a separation values greater than this to be accepted -- here 0.1f is 1%
    // this tends to overfit sometimes if there are not enough training samples, if chosen correctly
    // though, it gives a noticeable improvement in accuracy because it lets AdaBoost concenrate on
    // separating the difficult cloud of points instead of quickly finding that N% can be chopped off
    // by a single classifier and biasing it's weight significantly.
    static const UINT  NUM_RULES = 2;
    static const FLOAT RULE_MUST_BE_THIS_SIGNIFICANT = 0.5f;

    // this will write out thumbnails of samples on which Test() failed
    static const BOOL  SAVE_FAILED_THUMBNAILS = FALSE;//TRUE;

    // this is mainly a runtime optimisation. set to 0 to keep all of them alive. this is a quick
    // optimisation which always leads to a decrease in accuracy, although depending on how alphas
    // are falling off, this could produce reasonable results, so be a viable choice
    static const UINT  MAX_WEAK_CLASSIFIERS = 0;

    // this means we stop training if the best classifier gives us less than a 50% chance of detection
    // if you reduce this number the training is going to finish much quicker although the training                                                            
    // results may suffer. recommended at 50%
    static const FLOAT ERROR_THRESHOLD = 0.5f;

    // naive "optimisation". greater values will result in faster training and runtime code but worse
    // detection rate generally. this is similar to MAX_WEAK_CLASSIFIERS, only it tries to be more
    // careful and checks the weights before it cuts off the tail.
    static const FLOAT ALPHA_CUTOFF = 0.001f;

    // display percentage after this many iterations, should be a power of 2 - 1
    static const UINT PROGRESS_TICK = 511;

    // this is to limit the amount of debug informatino printed out from OptimiseByAlpha
    static const UINT OPTIMIZE_PRINT_TOP_N = 100;



    m_pfnSampleAcceptor = NULL;

    m_bOptimisationLeaveOnlyBreakpoints = OPTIMISATION_LEAVE_ONLY_BREAKPOINTS;
    m_bSaveFailedThumbnails = FALSE;
    m_bUseRBoost = USE_RBOOST;
    m_bRealAdaboost = TRUE;
    m_uNumThresholdSteps = NUM_THRESHOLD_STEPS;
    m_uNumRules = NUM_RULES;
    m_fRuleMustBeThisSignificant = RULE_MUST_BE_THIS_SIGNIFICANT;
    m_uMaxWeakClassifiers = MAX_WEAK_CLASSIFIERS;
    m_fErrorThreshold = ERROR_THRESHOLD;
    m_fAlphaCutoff = ALPHA_CUTOFF;
    m_uProgressTick = PROGRESS_TICK;
    m_uOptimisePrintTopN = OPTIMIZE_PRINT_TOP_N;
    m_bVerbose = FALSE;
    m_bUseSTBasedClassifiers = USE_ST_BASED_CLASSIFIERS;
}


//--------------------------------------------------------------------------------------
// Name: PrintTopClassifiers
// Desc: Prints rank and total influence
//--------------------------------------------------------------------------------------
void PrintTopClassifiers( const HOCStrongClassifier& classifier, const UINT uNumTopToPrint, const UINT uNumInRunToPrint )
{
    const FLOAT fInvTotalWeight = 1.f / classifier.GetTotalAlpha();
    const UINT uNumClassifiers = classifier.GetNumWeakClassifiers();

    std::vector< BYTE > classifierPrinted( uNumClassifiers, 0 );
    std::vector< FLOAT > classifierTotalAlpha( uNumClassifiers, 0 );

    printf( "\nClassifiers rankings\n" );

    // print influence of top classifiers
    for( UINT i=0; i < uNumClassifiers; ++i )
    {
        if( classifierPrinted[ i ] )
            continue;

        const HOCBaseWeakClassifier& c = classifier.GetBaseWeakClassifier( i );

        if( i < uNumTopToPrint )
            HOCPrintClassifier( c, i, TRUE, classifier.UsesRealAdaBoost() );

        FLOAT fTotalContributingWeight = 0;
        UINT uCount = 0;
        for( UINT j=0; j < uNumClassifiers; ++j )
        {
            const HOCBaseWeakClassifier& c1 = classifier.GetBaseWeakClassifier( j );
            if( !c1.IsSameClass( c ) )
                continue;

            if( i < uNumTopToPrint )
            {
                if( uCount < uNumInRunToPrint )
                {
                    printf( "\t" );
                    HOCPrintClassifier( c1, j, TRUE, classifier.UsesRealAdaBoost() );
                } else if( uCount == uNumInRunToPrint )
                {
                    printf( "\t...\n" );
                }
            }

            fTotalContributingWeight += c1.GetAlpha();

            classifierPrinted[ j ] = 1;

            ++uCount;
        }

        classifierTotalAlpha[ i ] = fTotalContributingWeight;

        if( i < uNumTopToPrint )
            printf( "\tTotal weight %f (%f %%)\n", fTotalContributingWeight, 100 * fTotalContributingWeight * fInvTotalWeight );
    }

    // print total ranking
    memset( &classifierPrinted[ 0 ], 0, classifierPrinted.size() );

    printf( "\nRanking table. Classifiers not in the table didn't contribute.\n" );

    // print influence of top classifiers
    UINT uCount = 0;
    UINT uRank = 0;
    for( UINT i=0; i < uNumClassifiers; ++i )
    {
        const HOCBaseWeakClassifier& c = classifier.GetBaseWeakClassifier( i );

        if( classifierPrinted[ i ] )
            continue;

        if( uCount >= uNumTopToPrint )
            break;
        ++uCount;

        HOCPrintClassifier( c, uRank, FALSE, classifier.UsesRealAdaBoost() );
        printf( ", influence of all instances %f %%\n", 100 * classifierTotalAlpha[ i ] * fInvTotalWeight );

        for( UINT j=i + 1; j < uNumClassifiers; ++j )
        {
            const HOCBaseWeakClassifier& c1 = classifier.GetBaseWeakClassifier( j );
            if( !c1.IsSameClass( c ) )
                continue;

            classifierPrinted[ j ] = 1;
        }

        ++uRank;
    }
}


#endif  // HOC_TRAINER
