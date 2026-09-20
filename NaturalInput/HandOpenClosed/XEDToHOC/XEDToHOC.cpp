// XedToHOC.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


std::vector< HOCSourceData > g_recording;

// support for range is important for external tools that may tag ranges
struct TaggedRange
{
	BOOL    m_bLeftHand;
	BOOL    m_bRightHand;
	UINT    m_uFirstFrame;
	UINT    m_uLastFrame;
};

struct XedFileRecord
{
    std::string m_name;
    std::vector< TaggedRange >  m_ranges;
};

static HRESULT ReadFile( _In_ const CHAR* pFilename, const std::vector< TaggedRange >& ranges, BOOL bVerbose );
static HRESULT WriteFile( _In_ const CHAR* pFilename, const std::vector< HOCSourceData >& data );
static HRESULT ParseDescriptionFile( _In_ const CHAR* pFilename, std::vector< XedFileRecord >& files );

INT main( INT argc, const CHAR* argv[] )
{
    const BOOL   bVerbose = FALSE;

    // first check if we're given a special file that has range descriptions in it
    if( argc == 5 )
    {
        std::vector< XedFileRecord > files;
        if( FAILED( ParseDescriptionFile( argv[ 4 ], files ) ) )
        {
            printf( "Couldn't parse description file\n" );
            return -1;
        }

        for( UINT i=0; i < (UINT)files.size(); ++i )
        {
            ReadFile( files[ i ].m_name.c_str(), files[ i ].m_ranges, bVerbose );
        }
    } else
    {
        // check for regular parameters
        if( argc < 6 )
        {
            printf( "Usage: XEDToHOC out outTest holdout in1 {l|r|b} in2 {l|r|b} ...\n"
                    "Holdout -- what %% of the input to take for the test\n"
				    "The second parameter indicates whether to take the right(r) or the left(l) or both (b) hands.\n" );
            return -1;
        }

        // read the number of files and the handendess
        for( INT j=4; j < argc; j += 2 )
        {
            if( j + 2 > argc )
                break;

            const char* pFilename = argv[ j ];

		    BOOL bRightHand = FALSE;
		    BOOL bLeftHand = FALSE;
		    if( (argv[ j + 1 ][ 0 ] == 'b') )
			    bRightHand = bLeftHand = TRUE;
		    else if( (argv[ j + 1 ][ 0 ] == 'l') )
			    bLeftHand = TRUE;
		    else
			    bRightHand = TRUE;

            std::vector< TaggedRange >  range( 1 );
            range[ 0 ].m_bLeftHand = bLeftHand;
            range[ 0 ].m_bRightHand = bRightHand;
            range[ 0 ].m_uFirstFrame = 0;
            range[ 0 ].m_uLastFrame = ~0ul;

            ReadFile( pFilename, range, bVerbose );
        }
    }

    //
    // write out with holdout
    //

    if( g_recording.empty() )
    {
        printf( "nothing to write\n" );
        return 0;
    }

    std::vector< HOCSourceData >    g_train;
    std::vector< HOCSourceData >    g_test;

    INT iHoldout = atoi( argv[ 3 ] );
    if( iHoldout < 0 )
        iHoldout = 0;
    if( iHoldout > 100 )
        iHoldout = 100;

    if( iHoldout < 2 )
    {
        WriteFile( argv[ 1 ], g_recording );
    } else
    {
        srand( 720983 );
        for( UINT i=0; i < g_recording.size(); ++i )
        {
            if( (i % iHoldout ) == 0 )
                g_test.push_back( g_recording[ i ] );
            else
                g_train.push_back( g_recording[ i ] );
        }

        WriteFile( argv[ 1 ], g_train );
        WriteFile( argv[ 2 ], g_test );
    }

	return 0;
}



//
// read the file
//
HRESULT ReadFile( _In_ const CHAR* pFilename, const std::vector< TaggedRange >& ranges, BOOL bVerbose )
{
    CXEDFile file;
    if( FAILED( file.Open( pFilename ) ) )
    {
        printf( "failed to open %s\n", pFilename );
        return E_FAIL;
    }

    printf( "opened %s\n", pFilename );

    if( !file.HasDepthData() || !file.HasSkeletonData() )
    {
        printf( "file doesn't contain depth or skeleton data\n" );
        return E_ABORT;
    }

    const XEDMultiFrameRange& info = file.GetMultiFrameInfo();

    const UINT  uFrame0 = info.skeleton.start.frameNumber;
    const UINT  uFrameLast = info.skeleton.end.frameNumber;

    CXEDDepthFrame depth;
    CXEDSkeletonFrame skeleton;
    CXEDTitleDataFrame titleData;
    HOCDataViews hocData;

    depth.AllocateBuffer();

    printf( "frame0 = %d, lastframe = %d, numFrames = %d, numRanges = %d\n", uFrame0, uFrameLast, uFrameLast - uFrame0, ranges.size() );

    UINT    uLastSkeletonIndex = ~0ul;

    FLOAT fPlayerSize = 0;
    UINT uNumImportedFrames = 0;

    for( UINT i=uFrame0; i < uFrameLast; ++i )
    {
        const UINT uEventDepth = file.GetEventIndexFromFrameNumber( XED_DEPTH, i );
        const UINT uEventSkeleton = file.GetEventIndexFromFrameNumber( XED_SKELETON, i );
        const UINT uEventTitleData = file.GetEventIndexFromFrameNumber( XED_TITLE_DATA, i );

        if( FAILED( file.ReadFrame( uEventDepth, &depth ) )  ||
            FAILED( file.ReadFrame( uEventSkeleton, &skeleton ) ) )
        {
            continue;
        }

        // handle skeleton change
        const NUI_SKELETON_DATA* pSkelData = NULL;
        for( UINT j=0; j < NUI_SKELETON_COUNT; ++j )
        {
            if( skeleton.GetData()->SkeletonData[ j ].eTrackingState != NUI_SKELETON_TRACKED )
                continue;

            if( uLastSkeletonIndex != ~0ul &&
                uLastSkeletonIndex != j )
            {
                uLastSkeletonIndex = j;
                printf( "Warning: skeleton index changed\n" );

                fPlayerSize = 0;
            }

            pSkelData = &skeleton.GetData()->SkeletonData[ j ];
            break;
        }

        if( !pSkelData )
            continue;

        // update player size
        if( 0 == uNumImportedFrames )
            HOCDetector::GetPlayerSize( fPlayerSize, pSkelData );

        // LPF it
        FLOAT   fSize;
        if( HOCDetector::GetPlayerSize( fSize, pSkelData ) )
            fPlayerSize = 0.98f * fPlayerSize + 0.02f * fSize;

        UINT    uHandsEnabledForThisFrame = 0;

        // check if this frame belongs to a valid range and we need to export it
        for( UINT k=0; k < ranges.size(); ++k )
        {
            if( ranges[ k ].m_uFirstFrame <= i &&
                ranges[ k ].m_uLastFrame > i )
            {
                if( ranges[ k ].m_bLeftHand )
                    uHandsEnabledForThisFrame |= 2;

                if( ranges[ k ].m_bRightHand )
                    uHandsEnabledForThisFrame |= 1;
            }
        }

        // see if that's inside one of our ranges
        for( UINT uHand = 0; uHand < 2; ++uHand )
        {
            if( 0 == (uHandsEnabledForThisFrame & (1 << uHand)) )
                continue;

            const BOOL bRightHand = 0 == uHand;

            const NUI_SKELETON_POSITION_INDEX hand  = bRightHand ? NUI_SKELETON_POSITION_HAND_RIGHT  : NUI_SKELETON_POSITION_HAND_LEFT;
            const NUI_SKELETON_POSITION_INDEX wrist = bRightHand ? NUI_SKELETON_POSITION_WRIST_RIGHT : NUI_SKELETON_POSITION_WRIST_LEFT;
            const NUI_SKELETON_POSITION_INDEX elbow = bRightHand ? NUI_SKELETON_POSITION_ELBOW_RIGHT : NUI_SKELETON_POSITION_ELBOW_LEFT;

            // perform hand data acquisition
            if( pSkelData->eSkeletonPositionTrackingState[ hand ] == NUI_SKELETON_POSITION_TRACKED )
            {
                const XMVECTOR* pPositions = &pSkelData->SkeletonPositions[ 0 ];

                const BOOL bElbowTracked = pSkelData->eSkeletonPositionTrackingState[ elbow ] == NUI_SKELETON_POSITION_TRACKED;
                const BOOL bWristTracked = pSkelData->eSkeletonPositionTrackingState[ wrist ] == NUI_SKELETON_POSITION_TRACKED;
            
                if( HOCDetector::GetFrameData(  hocData,
                                                bElbowTracked, bWristTracked,
                                                pPositions[ elbow ],
                                                pPositions[ wrist ],
                                                pPositions[ hand ],
                                                depth.GetData(),
                                                fPlayerSize ) )
                {
                    if( !hocData.m_transientData.m_points.empty() )
                    {
                        if( bVerbose )
                            printf( "%d -> %d, %s, %d voxels, m_fPlayerSize %f, tracked 0x%x\n", i, g_recording.size(), bRightHand ? "right" : "left", hocData.m_sourceData.m_uNumVoxels, hocData.m_sourceData.m_fPlayerSize, hocData.m_sourceData.m_bTrackedElbowWrist );

                        g_recording.push_back( hocData.m_sourceData );

                        ++uNumImportedFrames;
                    }
                }
            }
        }
    }

    return S_OK;
}



//
//
//
HRESULT WriteFile( const CHAR* pFilename, const std::vector< HOCSourceData >& data )
{
    FILE* fp;
    if( fopen_s( &fp, pFilename, "wb" ) )
    {
        printf( "failed to write %s\n", pFilename );
        return E_FAIL;
    }

    printf( "writing %s %d frames\n", pFilename, data.size() );

    const UINT uNumItems = (UINT)data.size();
    WriteUInt( &uNumItems, fp );

    for( UINT i=0; i < uNumItems; ++i )
        data[ i ].Save( fp );

    fclose( fp );

    return S_OK;
}



//
//
//
HRESULT ParseDescriptionFile( _In_ const CHAR* pFilename, std::vector< XedFileRecord >& files )
{
    std::vector< CHAR > data;

    // read the list file
    {
        FILE* fp;
        if( fopen_s( &fp, pFilename, "rt" ) )
        {
            printf( "failed to open descriptor %s\n", pFilename );
            return E_FAIL;
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

    UINT uLine = 1;

    static const CHAR separator = ',';

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
        while( p < e && *p < ' ' )
            ++p;

        // process the line between lineStart and lineEnd
        // expect filename,L,R,i0,i1
        // L,R = {0;1} -- left hand on, right hand on
        // i0 = start frame index
        // i1 - end + 1 frame index
        const CHAR* delim = lineStart;
        while( delim < lineEnd && *delim != separator )
            delim++;

        if( delim == lineEnd )
        {
            printf( "parse error line %d -- filename\n", uLine );
            return E_FAIL;
        }

        // filename
        std::string name = std::string( lineStart, delim );

        // 4 numbers
        UINT numbers[ 4 ] = { 0 };
        for( UINT i=0; i < 4; ++i )
        {
            while( delim < lineEnd && (*delim <= ' ' || *delim == separator) )
                ++delim;

            if( delim == lineEnd )
            {
                printf( "parse error line %d -- start number %d\n", uLine, i );
                return E_FAIL;
            }

            const CHAR* tokenEnd = delim + 1;
            while( tokenEnd < lineEnd && *tokenEnd > ' ' && *tokenEnd != separator )
                ++tokenEnd;

            if( i < 3 &&
                tokenEnd == lineEnd )
            {
                printf( "parse error line %d -- end number %d\n", uLine, i );
                return E_FAIL;
            }

            numbers[ i ] = (UINT)atoi( std::string( delim, tokenEnd ).c_str() );

            delim = tokenEnd;
        }

        // now find the record with the matching filename
        UINT uRecordIndex;
        for( uRecordIndex = 0; uRecordIndex < files.size(); ++uRecordIndex )
        {
            if( files[ uRecordIndex ].m_name.compare( name ) == 0 )
                break;
        }

        if( uRecordIndex == files.size() )
        {
            XedFileRecord   rec;
            rec.m_name = name;
            files.push_back( rec );
        }

        TaggedRange range;
        range.m_bLeftHand = numbers[ 0 ];
        range.m_bRightHand = numbers[ 1 ];
        range.m_uFirstFrame = numbers[ 2 ];
        range.m_uLastFrame = numbers[ 3 ];

        files[ uRecordIndex ].m_ranges.push_back( range );

        ++uLine;
    }

    return S_OK;
}

