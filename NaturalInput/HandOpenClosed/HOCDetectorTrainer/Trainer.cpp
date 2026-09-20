//--------------------------------------------------------------------------------------
// Trainer.cpp
//
// The app that trains classifiers. Several modes are supported including
// - regular AdaBoost+RBoost training, which combines weak classifiers into a strong classifier
// - ensemble training which combines several strong classifiers into one strong classifier
// - test mode which only tests accuracy of the classifier
// - data output mode which spits out all the data into a .csv for external analysis
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "..\HOCDetector\HOCDetector.h"



static std::vector< BOOL > g_openClosed;
static std::vector< std::string > g_names;

static BOOL ParseListFile( const CHAR* argv, const CHAR**& pFilenames, BOOL*& trainOpenOrClosed );
static BOOL AcceptSampleFilter( const HOCDataViews& dv );
static VOID Test( const HOCDetector& detector, const CHAR* pszTestSetFilename );
static VOID FindFilteringParameters( UINT& uNumFrames, FLOAT& fThreshold, const HOCDetector& detector, const CHAR* pszTestSetFilename );
static VOID PrintUsage();
static VOID PrintParams( const HOCDetectorTrainer::Parameters& params );

// tests accuracy on different ranges of data
static const BOOL   EXTENDED_TEST = 0;
static const BOOL   RUN_FILTER_TEST = FALSE;    // experimental

FLOAT   g_fMinDepthAccept = 0, g_fMaxDepthAccept = 4;
UINT    g_uMinVoxelsAccept = 0, g_uMaxVoxelsAccept = HOCSourceData::MAX_POINTS;


//--------------------------------------------------------------------------------------
// Name: main
// Desc: entry point
//--------------------------------------------------------------------------------------
INT main( INT argc, CHAR* argv[] )
{   
    printf( "Hand Opened Closed Trainer\n" );

    // the smallest "verb" takes two parameters
    if( argc < 3 )
    {
        PrintUsage();
        return -1;
    }

    HOCDetectorTrainer::Parameters  params;
    params.SetDefault();
    params.m_pfnSampleAcceptor = AcceptSampleFilter;
    params.m_bRealAdaboost = FALSE;

    // decide what we're doing -- scan the program arguments sequentially
    for( INT i=1; i < argc; )
    {
        const INT uNumArgsLeft = argc - i - 1;

        if( 0 == strcmp( argv[ i ], "-l" ) )
        {
            params.m_bRealAdaboost = TRUE;
            ++i;
            continue;
        }

        // training
        if( 0 == strcmp( argv[ i ], "-T" ) )
        {
            ++i;

            if( uNumArgsLeft < 3 )
            {
                printf( "Not enough arguments for command %d\n", i - 1 );
                PrintUsage();
                return -1;
            }

            // Given a training set of data as input, we generate weak classifiers, run AdaBoost on them
            // to obtain a list of weak classifiers that work, then we run RBoost on that to made the list
            // smaller.
            // Optionally limit the training set to only the samples that fall into a given range of
            // the number of voxels and the distance. Empyrically the number of voxels is a better way
            // to split the training set. Once several classifiers are trained on the different parts of
            // the training set, they become "experts" in their volume of training data space (in other
            // words, a classifier trained on smaller number of voxels will have a higher accuracy than
            // a classifier trained on a large number of voxels when given data with small number of voxels.)
            // After that, ensemble training can be used to combine those into an even stronger classifier.

            printf( "Training\nTrainSetFile = %s\nTestSetFile = %s\nOutputFile = %s\n", argv[ i + 0 ], argv[ i + 1 ], argv[ i + 2 ] );
            PrintParams( params );

            // read the list of training files
            const CHAR** trainFiles;
            BOOL* trainOpenOrClosed;
            if( 0 != ParseListFile( argv[ i + 0 ], trainFiles, trainOpenOrClosed ) )
                return -2;

            // train with simple classifiers
            HOCDetector detector;
            HOCExtraClassifierData emptyEnsemble;
            if ( FAILED( HOCDetectorTrainer::Train( detector, emptyEnsemble, trainFiles, trainOpenOrClosed, params ) ) )
                return 1;

            // find filtering parameters, turned off
            UINT uNumFrames = 8;
            FLOAT fThreshold = 0;
            if( RUN_FILTER_TEST )
            {
                FindFilteringParameters( uNumFrames, fThreshold, detector, argv[ i + 1 ] );
            }

            detector.SetTrainingSetInfo( g_uMinVoxelsAccept, g_uMaxVoxelsAccept, g_fMinDepthAccept, g_fMaxDepthAccept, uNumFrames, fThreshold );

            printf( "Saving %s\n", argv[ i + 2 ] );
            if ( FAILED( detector.Save( argv[ i + 2 ] ) ) )
                return 1;

            // testing
            Test( detector, argv[ i + 1 ] );

            i += 3;

            continue;
        }

        // testing
        if( 0 == strcmp( argv[ i ], "-X" ) )
        {
            ++i;

            if( uNumArgsLeft < 2 )
            {
                printf( "Not enough arguments for command %d\n", i - 1 );
                PrintUsage();
                return -1;
            }

            // The given detector is loaded and it's run against the given data set to compute accuracy
            printf( "Testing\nDetector = %s\nTestSetFile = %s\n", argv[ i + 0 ], argv[ i + 1 ] );
            PrintParams( params );

            // read the detector
            HOCDetector detector;
            if( FAILED( detector.Load( argv[ i + 0 ] ) ) )
            {
                printf( "failed to load detector\n" );
                return -3;
            }

            Test( detector, argv[ i + 1 ] );

            i += 2;

            continue;
        }

        // ensemble training
        if( 0 == strcmp( argv[ i ], "-S" ) )
        {
            ++i;

            if( uNumArgsLeft < 4 )
            {
                printf( "Not enough arguments for command %d\n", i - 1 );
                PrintUsage();
                return -1;
            }

            // If there are several classifiers, each trained on a specific volume of training set space,
            // this combines them into a single classifier which can be always made stronger than its
            // constituents.

            printf( "Ensemble training\nEnsemble list = %s\nTrainSetFile = %s\nTestSetFile = %s\nOutput = %s\n", argv[ i + 0 ], argv[ i + 1 ], argv[ i + 2 ], argv[ i + 3 ] );
            PrintParams( params );

            const CHAR** trainFiles;
            BOOL* trainOpenOrClosed;
            if( 0 != ParseListFile( argv[ i + 0 ], trainFiles, trainOpenOrClosed ) )
                return -2;

            // load strong classifiers
            HOCExtraClassifierData  ensemble;
            while( *trainFiles )
            {
                HOCDetector d;
                if( FAILED( d.Load( *trainFiles ) ) )
                {
                    printf( "failed to load %s\n", *trainFiles );
                    return -3;
                }

                printf( "loaded %s\n", *trainFiles );

                ensemble.m_level0Detectors.push_back( d );

                ++trainFiles;
            }

            // read the list of training files
            if( 0 != ParseListFile( argv[ i + 1 ], trainFiles, trainOpenOrClosed ) )
                return -2;

            // train with strong classifiers
            HOCDetector detector;
            if ( FAILED( HOCDetectorTrainer::Train( detector, ensemble, trainFiles, trainOpenOrClosed, params ) ) )
                return 1;

            // find filtering parameters
            UINT uNumFrames = 8;
            FLOAT fThreshold = 0;
            if( RUN_FILTER_TEST )
            {
                FindFilteringParameters( uNumFrames, fThreshold, detector, argv[ i + 2 ] );
            }

            detector.SetTrainingSetInfo( g_uMinVoxelsAccept, g_uMaxVoxelsAccept, g_fMinDepthAccept, g_fMaxDepthAccept, uNumFrames, fThreshold );

            printf( "Saving %s\n", argv[ i + 3 ] );
            if ( FAILED( detector.Save( argv[ i + 3 ] ) ) )
                return 1;

            // testing
            Test( detector, argv[ i + 2 ] );

            i += 4;

            continue;
        }

        // data printout
        if( 0 == strcmp( argv[ i ], "-D" ) )
        {
            ++i;

            if( uNumArgsLeft < 2 )
            {
                printf( "Not enough arguments for command %d\n", i - 1 );
                PrintUsage();
                return -1;
            }

            // For external data analysis using math packages this can output a .csv with all of the training set data
            printf( "Data printout\nTrainSetFile = %s\nOutput = %s\n", argv[ i + 0 ], argv[ i + 1 ] );
            PrintParams( params );
        
            // read the list of training files
            const CHAR** trainFiles;
            BOOL* trainOpenOrClosed;
            if( 0 != ParseListFile( argv[ i + 0 ], trainFiles, trainOpenOrClosed ) )
                return -2;

            HOCDetectorTrainer::OutputDataCSV( argv[ i + 1 ], trainFiles, trainOpenOrClosed, params );

            i += 2;

            continue;
        }

        // filter parameters
        if( 0 == strcmp( argv[ i ], "-F" ) )
        {
            ++i;

            if( uNumArgsLeft < 2 )
            {
                printf( "Not enough arguments for command %d\n", i - 1 );
                PrintUsage();
                return -1;
            }

            // this is experimental. it might be possible to deduce best filtering parameters for the given set, although
            // it's not yet clear how to do it
            printf( "Filter parameters search\nDetector = %s\nTestSetFile = %s\n", argv[ i + 0 ], argv[ i + 1 ] );
            PrintParams( params );

            // read the detector
            HOCDetector detector;
            if( FAILED( detector.Load( argv[ i + 0 ] ) ) )
            {
                printf( "failed to load detector\n" );
                return -3;
            }

            // find filtering parameters
            UINT uNumFrames;
            FLOAT fThreshold;
            FindFilteringParameters( uNumFrames, fThreshold, detector, argv[ i + 1 ] );

            // TODO: save?

            printf( "%d, %f\n", uNumFrames, fThreshold );

            i += 2;

            continue;
        }
        
        // set number of voxels
        if( 0 == strcmp( argv[ i ], "-v" ) )
        {
            ++i;

            if( uNumArgsLeft < 2 )
            {
                printf( "Not enough arguments for command %d\n", i - 1 );
                PrintUsage();
                return -1;
            }

            g_uMinVoxelsAccept = atoi( argv[ i + 0 ] );
            g_uMaxVoxelsAccept = atoi( argv[ i + 1 ] );

            printf( "setting voxels range %d %d\n", g_uMinVoxelsAccept, g_uMaxVoxelsAccept );

            i += 2;

            continue;
        }

        // set depth range
        if( 0 == strcmp( argv[ i ], "-d" ) )
        {
            ++i;

            if( uNumArgsLeft < 2 )
            {
                printf( "Not enough arguments for command %d\n", i - 1 );
                PrintUsage();
                return -1;
            }

            g_fMinDepthAccept = static_cast< FLOAT >( atof( argv[ i + 0 ] ) );
            g_fMaxDepthAccept = static_cast< FLOAT >( atof( argv[ i + 1 ] ) );

            printf( "setting depth range %f %f\n", g_fMinDepthAccept, g_fMaxDepthAccept );

            i += 2;

            continue;
        }

        // set number of classifiers
        if( 0 == strcmp( argv[ i ], "-n" ) )
        {
            ++i;

            if( uNumArgsLeft < 1 )
            {
                printf( "Not enough arguments for command %d\n", i - 1 );
                PrintUsage();
                return -1;
            }

            params.m_uNumThresholdSteps = atoi( argv[ i ] );

            printf( "setting num threshold steps = %d\n", params.m_uNumThresholdSteps );

            ++i;

            continue;
        }

        // enable rboost
        if( 0 == strcmp( argv[ i ], "-r" ) )
        {
            ++i;

            if( uNumArgsLeft < 1 )
            {
                printf( "Not enough arguments for command %d\n", i - 1 );
                PrintUsage();
                return -1;
            }

            params.m_bUseRBoost = atoi( argv[ i ] );

            printf( "setting enable rboost = %d\n", params.m_bUseRBoost );

            ++i;

            continue;
        }

        // set number of items printed out in optimise step
        if( 0 == strcmp( argv[ i ], "-o" ) )
        {
            ++i;

            if( uNumArgsLeft < 1 )
            {
                printf( "Not enough arguments for command %d\n", i - 1 );
                PrintUsage();
                return -1;
            }

            params.m_uOptimisePrintTopN = atoi( argv[ i ] );

            printf( "setting optimise print topN = %d\n", params.m_uOptimisePrintTopN );

            ++i;

            continue;
        }

        // set decision tree parameters
        if( 0 == strcmp( argv[ i ], "-p" ) )
        {
            ++i;

            if( uNumArgsLeft < 2 )
            {
                printf( "Not enough arguments for command %d\n", i - 1 );
                PrintUsage();
                return -1;
            }

            params.m_uNumRules = atoi( argv[ i + 0 ] );
            params.m_fRuleMustBeThisSignificant = static_cast< FLOAT >( atof( argv[ i + 1 ] ) );

            printf( "setting decision tree num rules %d, must be this significant %f\n", params.m_uNumRules, params.m_fRuleMustBeThisSignificant );

            i += 2;

            continue;
        }

        // set number of items printed out in optimise step
        if( 0 == strcmp( argv[ i ], "-q" ) )
        {
            ++i;

            if( uNumArgsLeft < 1 )
            {
                printf( "Not enough arguments for command %d\n", i - 1 );
                PrintUsage();
                return -1;
            }

            params.m_uMaxWeakClassifiers = atoi( argv[ i ] );

            printf( "setting max num weak classifier = %d\n", params.m_uMaxWeakClassifiers );

            ++i;

            continue;
        }

        // set number of items printed out in optimise step
        if( 0 == strcmp( argv[ i ], "-s" ) )
        {
            ++i;

            if( uNumArgsLeft < 1 )
            {
                printf( "Not enough arguments for command %d\n", i - 1 );
                PrintUsage();
                return -1;
            }

            params.m_fErrorThreshold = static_cast< FLOAT >( atof( argv[ i ] ) );

            printf( "setting error threshold for training = %f\n", params.m_fErrorThreshold );

            ++i;

            continue;
        }

        // set alpha cutoff
        if( 0 == strcmp( argv[ i ], "-a" ) )
        {
            ++i;

            if( uNumArgsLeft < 1 )
            {
                printf( "Not enough arguments for command %d\n", i - 1 );
                PrintUsage();
                return -1;
            }

            params.m_fAlphaCutoff = static_cast< FLOAT >( atof( argv[ i ] ) );

            printf( "setting alpha cutoff = %f\n", params.m_fAlphaCutoff );

            ++i;

            continue;
        }
    }

    return 0;
}



//--------------------------------------------------------------------------------------
// Name: ParseListFile
// Desc: Given a list file name reads it and extracts data
//--------------------------------------------------------------------------------------
BOOL ParseListFile( const CHAR* argv, const CHAR**& pFilenames, BOOL*& pOpenOrClosed )
{
    g_openClosed.clear();
    g_names.clear();

    std::vector< CHAR > data;

    // read the list file
    {
        FILE* fp;
        if( fopen_s( &fp, argv, "rt" ) )
        {
            printf( "can't open %s\n", argv );
            return -2;
        }

        fseek( fp, 0, SEEK_END );
        const LONG sz = ftell( fp );
        fseek( fp, 0, SEEK_SET );

        data.resize( sz );
        fread( &data[ 0 ], sz, 1, fp );

        fclose( fp );
    }

    // parse it now
    const CHAR* p = &data[ 0 ];
    const CHAR* e = &data[ data.size() - 1 ];

    while( p < e )
    {
        const CHAR* lineStart = p;

        // skip till eol
        while( p < e && *p != 10 )
            ++p;

        // end of file?
        if( p == e )
            break;

        // find the end of the line
        const CHAR* lineEnd = p;
        while( p < e && *p < 32 )
            ++p;

        // process the line
        const CHAR* delim = lineStart;
        while( delim < lineEnd && *delim != ',' )
            delim++;

        // add the line to the list
        g_names.push_back( std::string( lineStart, delim ) );

        ++delim;

        const BOOL v = atoi( delim );

        g_openClosed.push_back( v );
    }

    // 
    pFilenames = new const CHAR* [ g_names.size() + 1 ];
    for( UINT i=0; i < g_names.size(); ++i )
        pFilenames[ i ] = g_names[ i ].c_str();
    pFilenames[ g_names.size() ] = NULL;

    pOpenOrClosed = &g_openClosed[ 0 ];

    return 0;
}


//--------------------------------------------------------------------------------------
// Name: AcceptSampleFilter
// Desc: callback from the trainer to ask if it should take a sample into account
//--------------------------------------------------------------------------------------
BOOL AcceptSampleFilter( const HOCDataViews& dv )
{
    // make sure we take the right depth slab
    if( dv.m_transientData.m_vCentroid.z < g_fMinDepthAccept ||
        dv.m_transientData.m_vCentroid.z > g_fMaxDepthAccept )
    {
        return FALSE;
    }

    if( dv.m_sourceData.m_uNumVoxels < g_uMinVoxelsAccept ||
        dv.m_sourceData.m_uNumVoxels > g_uMaxVoxelsAccept )
    {
        return FALSE;
    }

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: Test
// Desc: Runs accuracy tests including extended tests if specified
//--------------------------------------------------------------------------------------
VOID Test( const HOCDetector& detector, const CHAR* pszTestSetFilename )
{
    const CHAR** testFiles;
    BOOL* testOpenOrClosed;
    if( 0 != ParseListFile( pszTestSetFilename, testFiles, testOpenOrClosed ) )
    {
        printf( "\n\nCouldn't open test set file\n" );
        return;
    }

    HOCDetectorTrainer::TestResults res;
    HOCDetectorTrainer::Parameters params;
    params.SetDefault();

    // generate the test results based on the same range of data as the training set used
    printf( "\n\nTesting on Test Set (the result is less meaningful when tested on Train Data because the classifier saw the examples already)\n" );
    printf( "limits = voxels (%d, %d), depth( %f, %f)\n", g_uMinVoxelsAccept, g_uMaxVoxelsAccept, g_fMinDepthAccept, g_fMaxDepthAccept );
    params.m_pfnSampleAcceptor = AcceptSampleFilter;
    params.m_bVerbose = TRUE;
    HOCDetectorTrainer::Test( res, detector, testFiles, testOpenOrClosed, params );
    res.Print();

    // generate the test results per distance / voxel
    if( EXTENDED_TEST )
    {
        params.m_pfnSampleAcceptor = AcceptSampleFilter;
        params.m_bVerbose = FALSE;

        g_uMinVoxelsAccept = 0;
        g_uMaxVoxelsAccept = 4096;
        for( FLOAT d = 0.8f; d < 4.f; d += 0.25f )
        {
            g_fMinDepthAccept = d;
            g_fMaxDepthAccept = d + 0.25f;

            HOCDetectorTrainer::Test( res, detector, testFiles, testOpenOrClosed, params );

            if( res.GetNumCases() )
                printf( "%f,%f,%d,%f\n", g_fMinDepthAccept, g_fMaxDepthAccept, res.GetNumCases(), res.GetSuccessRatio() );
        }

        g_fMinDepthAccept = 0;
        g_fMaxDepthAccept = 4;
        for( UINT i=0; i < 512; i += 32 )
        {
            g_uMinVoxelsAccept = i;
            g_uMaxVoxelsAccept = i + 32;

            if( i + 32 >= 512 )
                g_uMaxVoxelsAccept = 4096;

            HOCDetectorTrainer::Test( res, detector, testFiles, testOpenOrClosed, params );

            if( res.GetNumCases() )
                printf( "%d,%d,%d,%f\n", g_uMinVoxelsAccept, g_uMaxVoxelsAccept, res.GetNumCases(), res.GetSuccessRatio() );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: FindFilteringParameters
// Desc: Finds the filtering window size and threshold
//--------------------------------------------------------------------------------------
VOID FindFilteringParameters( UINT& uNumFrames, FLOAT& fThreshold, const HOCDetector& detector, const CHAR* pszTestSetFilename )
{
    const CHAR** testFiles;
    BOOL* testOpenOrClosed;
    if( 0 != ParseListFile( pszTestSetFilename, testFiles, testOpenOrClosed ) )
    {
        printf( "\n\nCouldn't open test set file\n" );
        return;
    }

    HOCDetectorTrainer::Parameters params;
    params.SetDefault();
    params.m_pfnSampleAcceptor = AcceptSampleFilter;
    params.m_bVerbose = TRUE;
    HOCDetectorTrainer::FindFilteringParameters( uNumFrames, fThreshold, detector, testFiles, testOpenOrClosed, params );
}


//--------------------------------------------------------------------------------------
// Name: PrintUsage
// Desc: Displays help
//--------------------------------------------------------------------------------------
VOID PrintUsage()
{
    printf( "\n\nHOCTrainer trains classifiers. Pass setup parameters (lowercase) before verb\n"
            "parameters (uppercase). Multiple sequences of commands can be specified at once.\n"
            "\nverbs\n"
            "-l                                        use Real AdaBoost, default is Discrete AdaBoost + RBoost\n"
            "-T trainingset testset output             trains a strong classifier\n"
            "-X classifier  testset                    tests a classifier\n"
            "-S ensembleset trainingset testset output trains an ensemble classifier\n"
            "-D testset output                         outputs CSV data\n"
            "\n\nsettings set parameters for the subsequent action verb(s)\n"
            "-v minVoxels maxVoxels     sets voxel range\n"
            "-d minDistnace maxDistance sets distance range\n"
            "-n numSplits               sets the number of notches on axises\n"
            "-r 0|1                     enables or disables rboost\n"
            "-o numItemsToPrintout      sets how many items to print out in optimisation step\n"
            "-p treeSteps minimumValue  sets the number of decision tree levels\n"
            "-q maxNumWeakClassifiers   sets the maximum number of classifiers\n"
            "-s errorThreshold          sets the stop error threshold\n"
            "-a alphaCutoff             sets the alpha cutoff for optimisation\n" );
}



//--------------------------------------------------------------------------------------
// Name: PrintParams
// Desc: Displays parameters
//--------------------------------------------------------------------------------------
VOID PrintParams( const HOCDetectorTrainer::Parameters& params )
{
    printf( "Params\n" );
    printf( "\tm_bRealAdaboost                      %d\n", params.m_bRealAdaboost );
    printf( "\tm_bOptimisationLeaveOnlyBreakpoints  %d\n", params.m_bOptimisationLeaveOnlyBreakpoints);
    printf( "\tm_bSaveFailedThumbnails              %d\n", params.m_bSaveFailedThumbnails            );
    printf( "\tm_bUseRBoost                         %d\n", params.m_bUseRBoost                       );
    printf( "\tm_bUseSTBasedClassifiers             %d\n", params.m_bUseSTBasedClassifiers           );
    printf( "\tm_bVerbose                           %d\n", params.m_bVerbose                         );
    printf( "\tm_uNumThresholdSteps                 %d\n", params.m_uNumThresholdSteps               );
    printf( "\tm_uNumRules                          %d\n", params.m_uNumRules                        );
    printf( "\tm_fRuleMustBeThisSignificant         %f\n", params.m_fRuleMustBeThisSignificant       );
    printf( "\tm_uMaxWeakClassifiers                %d\n", params.m_uMaxWeakClassifiers              );
    printf( "\tm_fErrorThreshold                    %f\n", params.m_fErrorThreshold                  );
    printf( "\tm_fAlphaCutoff                       %f\n", params.m_fAlphaCutoff                     );
    printf( "\tm_uProgressTick                      %d\n", params.m_uProgressTick                    );
    printf( "\tm_uOptimisePrintTopN                 %d\n", params.m_uOptimisePrintTopN               );
}
