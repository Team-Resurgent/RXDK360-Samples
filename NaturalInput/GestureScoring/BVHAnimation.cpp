//--------------------------------------------------------------------------------------
// BVHAnimation.cpp
//
// This loads a BVH animation 
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <assert.h>
#include <vector>
#include "BVHAnimation.h"



// centimeters -> meters and angles -> radians
static const FLOAT  POS_SCALE = 1.f / 100.f;
static const FLOAT  ROT_SCALE = 3.14159265359f / 180.f;


//--------------------------------------------------------------------------------------
// Name: ChannelType
// Desc: BVH has 6 channels, 3 for position 3 for rotation
//--------------------------------------------------------------------------------------
enum    ChannelType
{
    CHAN_POS_X = 0,
    CHAN_POS_Y,
    CHAN_POS_Z,
    CHAN_ROT_X,
    CHAN_ROT_Y,
    CHAN_ROT_Z,
    CHAN_NONE,
};


//--------------------------------------------------------------------------------------
// Name: NodeLoadContainer
// Desc: Temporary representation of the node
//--------------------------------------------------------------------------------------
struct NodeLoadContainer
{
    BVHAnimation::NodeName  m_name;
    UINT                    m_uIndex;
    UINT                    m_uParentIndex;
    UINT                    m_uNumFloatsPerFrame;
    ChannelType             m_channels[ 6 ];
    FLOAT                   m_offset[ 3 ];
};


//--------------------------------------------------------------------------------------
// Name: Node
// Desc: Final node representation
//--------------------------------------------------------------------------------------
struct BVHAnimation::Node
{
    FLOAT           m_offset[ 3 ];
    BYTE            m_uParentIndex;
    NodeName        m_name                  : 8;    // 0..24
    BYTE            m_numFloatsPerFrame;
    BYTE            m_channels[ 6 ];                // ChannelType
    BYTE            m_rotOrder[ 3 ];
};


//--------------------------------------------------------------------------------------
// Name: BoneLoadContainer
// Desc: Temporary representatino of the bone
//--------------------------------------------------------------------------------------
struct BoneLoadContainer
{
    BVHAnimation::Bone          m_bone;
    BVHAnimation::NodeName      m_parent;
    BVHAnimation::NodeName      m_self;
};


//--------------------------------------------------------------------------------------
// Name: AnimationLoadContainer
// Desc: Temporary representation of the animation
//--------------------------------------------------------------------------------------
struct AnimationLoadContainer
{
    std::vector< NodeLoadContainer >    m_nodes;
    std::vector< BoneLoadContainer >    m_bones;
    UINT                                m_nodesRemap[ BVHAnimation::BVH_NODE_COUNT + 1 ];
    FLOAT                       m_fFrameTime;
    UINT                        m_uNumFrames;
    UINT                        m_uNumFloatsPerFrame;
    std::vector< FLOAT >        m_frameData;
};


//--------------------------------------------------------------------------------------
// Name: g_BVHNodeNames
// Desc: Names of BVH nodes corresponding to the BVHNodeName enum
//--------------------------------------------------------------------------------------
static const char* g_BVHNodeNames[][ 3 ] = 
{
    { "HipCenterLeft" },
    { "HipCenterRight" },
    { "Spine", "Hips" },
    { "Spine1", "Chest", "Chest2" },    // some bvh have 3 links in the spine
    { "ShoulderCenter", "Neck" },
    { "ShoulderCenterLeft", "LeftCollar" },
    { "ShoulderCenterRight", "RightCollar" },
    { "Head" },
    { "LeftShoulder" },
    { "LeftElbow" },
    { "LeftWrist" },
    { "LeftHand" },
    { "RightShoulder" },
    { "RightElbow" },
    { "RightWrist" },
    { "RightHand" },
    { "LeftHip" },
    { "LeftKnee" },
    { "LeftAnkle" },
    { "LeftFoot" },
    { "RightHip" },
    { "RightKnee" },
    { "RightAnkle" },
    { "RightFoot" }
};


//--------------------------------------------------------------------------------------
// Name: BVHToken
// Desc: Tokens we may possibly encounter in a BVH file
//--------------------------------------------------------------------------------------
enum BVHToken
{
    TOK_HIERARCHY,
    TOK_ROOT,
    TOK_OPEN,
    TOK_CLOSE,
    TOK_OFFSET,
    TOK_CHANNELS,
    TOK_JOINT,
    TOK_END_SITE,
    TOK_MOTION,
    TOK_FRAMES,
    TOK_FRAME_TIME,
    TOK_XPOS,
    TOK_YPOS,
    TOK_ZPOS,
    TOK_XROT,
    TOK_YROT,
    TOK_ZROT,
    TOK_NUM_TOKENS
};


//--------------------------------------------------------------------------------------
// Name: BVHTokenString
// Desc: Tokenized string
//--------------------------------------------------------------------------------------
struct BVHTokenString
{
    char*       m_pString;
    UINT        m_uStringLength;
    BVHToken    m_name;
};



//--------------------------------------------------------------------------------------
// Name: g_BVHTokenStrings
// Desc: Strings corresponding to the BVHToken enum
//--------------------------------------------------------------------------------------
#define BVH_TOKEN( str, name )  { str, sizeof( str ), name }
static const BVHTokenString g_BVHTokenStrings[ TOK_NUM_TOKENS ] =
{
    BVH_TOKEN( "HIERARCHY",     TOK_HIERARCHY ),
    BVH_TOKEN( "ROOT",          TOK_ROOT ),
    BVH_TOKEN( "{",             TOK_OPEN ),
    BVH_TOKEN( "}",             TOK_CLOSE ),
    BVH_TOKEN( "OFFSET",        TOK_OFFSET ),
    BVH_TOKEN( "CHANNELS",      TOK_CHANNELS ),
    BVH_TOKEN( "JOINT",         TOK_JOINT ),
    BVH_TOKEN( "End Site",      TOK_END_SITE ),
    BVH_TOKEN( "MOTION",        TOK_MOTION ),
    BVH_TOKEN( "Frames:",       TOK_FRAMES ),
    BVH_TOKEN( "Frame Time:",   TOK_FRAME_TIME ),
    BVH_TOKEN( "Xposition",     TOK_XPOS ),
    BVH_TOKEN( "Yposition",     TOK_YPOS ),
    BVH_TOKEN( "Zposition",     TOK_ZPOS ),
    BVH_TOKEN( "Xrotation",     TOK_XROT ),
    BVH_TOKEN( "Yrotation",     TOK_YROT ),
    BVH_TOKEN( "Zrotation",     TOK_ZROT ),
};
#undef  BVH_TOKEN




//--------------------------------------------------------------------------------------
// Name: BVHAnimation
// Desc: Constructor
//--------------------------------------------------------------------------------------
BVHAnimation::BVHAnimation() :  m_pBones( NULL ),
                                m_pNodes( NULL ),
                                m_pAnimData( NULL ),
                                m_pMatrices( NULL )
{
}


//--------------------------------------------------------------------------------------
// Name: ~BVHAnimation()
// Desc: Destructor
//--------------------------------------------------------------------------------------
BVHAnimation::~BVHAnimation()
{
    delete[] m_pBones;
    delete[] m_pNodes;
    delete[] m_pAnimData;
    delete[] m_pMatrices;
}


//--------------------------------------------------------------------------------------
// Name: GetNodeOffset()
// Desc: Returns a node's offset
//--------------------------------------------------------------------------------------
XMVECTOR BVHAnimation::GetNodeOffset( UINT idx ) const
{
    assert( idx < m_numNodes );

    XMVECTOR    v;
    v.x = m_pNodes[ idx ].m_offset[ 0 ] * POS_SCALE;
    v.y = m_pNodes[ idx ].m_offset[ 1 ] * POS_SCALE;
    v.z = m_pNodes[ idx ].m_offset[ 2 ] * POS_SCALE;
    v.w = 1;

    return v;
}


//--------------------------------------------------------------------------------------
// Name: GetNumNodes()
// Desc: Returns the number of nodes
//--------------------------------------------------------------------------------------
UINT BVHAnimation::GetNumNodes() const
{
    return m_numNodes;
}


//--------------------------------------------------------------------------------------
// Name: GetBones()
// Desc: Returns the list of bones
//--------------------------------------------------------------------------------------
const BVHAnimation::Bone*  BVHAnimation::GetBones() const
{
    return m_pBones;
}


//--------------------------------------------------------------------------------------
// Name: GetNumBones()
// Desc: Returns the number of bones
//--------------------------------------------------------------------------------------
UINT BVHAnimation::GetNumBones() const
{
    return m_numBones;
}


//--------------------------------------------------------------------------------------
// Name: GetFrameDuration()
// Desc: Returns duration of an animation frame
//--------------------------------------------------------------------------------------
FLOAT   BVHAnimation::GetFrameDuration() const
{
    return m_frameDuration;
}


//--------------------------------------------------------------------------------------
// Name: GetNumFrames()
// Desc: Returns the number of frames in the animation
//--------------------------------------------------------------------------------------
UINT    BVHAnimation::GetNumFrames() const
{
    return m_numFrames;
}


//--------------------------------------------------------------------------------------
// Name: GetNodeName()
// Desc: Returns the node's name
//--------------------------------------------------------------------------------------
BVHAnimation::NodeName BVHAnimation::GetNodeName( UINT nodeIndex ) const
{
    assert( nodeIndex < m_numNodes );

    return m_pNodes[ nodeIndex ].m_name;
}


//--------------------------------------------------------------------------------------
// Name: GetNodeParentIndex()
// Desc: Returns index of the node's parent
//--------------------------------------------------------------------------------------
UINT BVHAnimation::GetNodeParentIndex( UINT idx ) const
{
    assert( idx < m_numNodes );

    return m_pNodes[ idx ].m_uParentIndex;
}

//--------------------------------------------------------------------------------------
// Name: CalcNodePositions()
// Desc: Given a frame index and the root matrix, calculate the positions of the nodes
//--------------------------------------------------------------------------------------
BOOL    BVHAnimation::CalcNodePositions( XMVECTOR* pDest, XMMATRIX* pRotationMatrices,
                                            XMVECTOR* pTranslationVectors, XMMATRIX matRoot, UINT uFrameIndex ) const
{
    if( uFrameIndex >= m_numFrames )
        return FALSE;

    const FLOAT*  pDataPtr = &m_pAnimData[ uFrameIndex * m_numFloatsPerFrame ];
    for( UINT i=0; i < m_numNodes; ++i )
    {
        const Node& n = m_pNodes[ i ];

        if( !n.m_numFloatsPerFrame )
            continue;

        FLOAT   posRot[ 6 ] = { n.m_offset[ 0 ], n.m_offset[ 1 ], n.m_offset[ 2 ], 0, 0, 0 };
        for( UINT j=0; (j < 6) && (CHAN_NONE != n.m_channels[ j ]); ++j )
        {
            assert( n.m_channels[ j ] < 6 );
            posRot[ n.m_channels[ j ] ] = *pDataPtr++;
        }

        pTranslationVectors[ i ] = XMVectorSet( posRot[ CHAN_POS_X ], posRot[ CHAN_POS_Y ], posRot[ CHAN_POS_Z ], 1 ) * POS_SCALE;

        const XMMATRIX    rot[ 3 ] =
        {
            XMMatrixRotationX( posRot[ CHAN_ROT_X ] * ROT_SCALE ),
            XMMatrixRotationY( posRot[ CHAN_ROT_Y ] * ROT_SCALE ),
            XMMatrixRotationZ( posRot[ CHAN_ROT_Z ] * ROT_SCALE )
        };

        pRotationMatrices[ i ] =    rot[ n.m_rotOrder[ 2 ] ] *
                                    rot[ n.m_rotOrder[ 1 ] ] *
                                    rot[ n.m_rotOrder[ 0 ] ];

        XMMATRIX    xform = pRotationMatrices[ i ] * XMMatrixTranslationFromVector( pTranslationVectors[ i ] );

        // multiply with parent
        if( n.m_uParentIndex < m_numNodes )
            xform = xform * m_pMatrices[ n.m_uParentIndex ];
        else
            xform = xform * matRoot;

        m_pMatrices[ i ] = xform;

        pDest[ i ] = xform.r[ 3 ];
    }

    return TRUE;
}



//////////////////// LOADING



//--------------------------------------------------------------------------------------
// Name: BVHNodeNameFromString()
// Desc: Parses the string to find the node's name
//--------------------------------------------------------------------------------------
static
BVHAnimation::NodeName    BVHNodeNameFromString( const char* str )
{
    for( UINT i=0; i < ARRAYSIZE( g_BVHNodeNames ); ++i )
    {
        for( UINT j=0; j < ARRAYSIZE( g_BVHNodeNames[ 0 ] ); ++j )
        {
            if( !g_BVHNodeNames[ i ][ j ] )
                break;

            if( 0 == strcmp( str, g_BVHNodeNames[ i ][ j ]) )
                return static_cast< BVHAnimation::NodeName >( i );
        }
    }

    return BVHAnimation::BVH_NODE_COUNT;
}


//--------------------------------------------------------------------------------------
// Name: IsEof()
// Desc: Returns whether k signifies the end of file
//--------------------------------------------------------------------------------------
static
BOOL        IsEof( char k )
{
    return 0 == k;
}


//--------------------------------------------------------------------------------------
// Name: IsEofOrDelim()
// Desc: Returns whether k means EOF or is a delimeter
//--------------------------------------------------------------------------------------
static
BOOL        IsEofOrDelim( char k )
{
    return k <= ' ';
}


//--------------------------------------------------------------------------------------
// Name: IsDelim()
// Desc: Returns whether k is a delimeter
//--------------------------------------------------------------------------------------
static
BOOL        IsDelim( char k )
{
    return k <= ' ' && k != 0;
}


//--------------------------------------------------------------------------------------
// Name: SkipWhitespace()
// Desc: Skips delimeters
//--------------------------------------------------------------------------------------
static
void    SkipWhitespace( const char*& pData )
{
    while( IsDelim( *pData ) )
        ++pData;
}


//--------------------------------------------------------------------------------------
// Name: Tokenize()
// Desc: Cuts out a single text token
//--------------------------------------------------------------------------------------
static
void    Tokenize( char* pDest, UINT uMaxDestSize, const char*& pData )
{
    SkipWhitespace( pData );

    UINT count = 0;
    for( ; ; ++count )
    {
        char k = pData[ count ];
        if( IsEofOrDelim( k ) )
            break;

        if( count >= uMaxDestSize - 1 )
        {
            // @ERR: string too large
            break;
        }

        pDest[ count ] = k;
    }

    pDest[ count ] = 0;

    pData += count;
}



//--------------------------------------------------------------------------------------
// Name: ReadFloats()
// Desc: Reads a bunch of floats in the text
//--------------------------------------------------------------------------------------
static
BOOL    ReadFloats( FLOAT* pDest, UINT uNumItems, const char*& pData )
{
    for( UINT i=0; i < uNumItems; ++i )
    {
        if( IsEof( *pData ) )
        {
            // @ERR: unexpected EOF
            return FALSE;
        }

        // TODO: can do a better checking for numeric chars
        char    tmp[ 128 ];
        Tokenize( tmp, ARRAYSIZE( tmp ) - 1, pData );

        if( pDest )
            pDest[ i ] = static_cast< FLOAT >( atof( tmp ) );
    }

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: ReadInts()
// Desc: Reads a bunch of integers in the text
//--------------------------------------------------------------------------------------
static
BOOL    ReadInts( INT* pDest, UINT uNumItems, const char*& pData )
{
    for( UINT i=0; i < uNumItems; ++i )
    {
        if( IsEof( *pData ) )
        {
            // @ERR: unexpected EOF
            return FALSE;
        }

        // TODO: can do a better checking for numeric chars
        char    tmp[ 128 ];
        Tokenize( tmp, ARRAYSIZE( tmp ) - 1, pData );

        if( pDest )
            pDest[ i ] = static_cast< INT >( atoi( tmp ) );
    }

    return TRUE;
}



//--------------------------------------------------------------------------------------
// Name: FindWinnerToken()
// Desc: Tries to find the longest token possible with the new given character
//--------------------------------------------------------------------------------------
static
BVHToken    FindWinnerToken( char k, UINT pos, BYTE* shortList )
{
    static_assert( TOK_NUM_TOKENS == ARRAYSIZE( g_BVHTokenStrings ), "NodeName Enum must be off" );

    BVHToken    winner = TOK_NUM_TOKENS;

    for( UINT i=0; i < ARRAYSIZE( g_BVHTokenStrings ); ++i )
    {
        if( g_BVHTokenStrings[ i ].m_uStringLength < pos )
            continue;

        if( !shortList[ i ] )
            continue;

        if( k == g_BVHTokenStrings[ i ].m_pString[ pos ] )
        {
            winner = (BVHToken)i;
        } else
        {
            shortList[ i ] = 0;
        }
    }

    return winner;
}



//--------------------------------------------------------------------------------------
// Name: ReadToken()
// Desc: Reads a token from the string
//--------------------------------------------------------------------------------------
static
BVHToken    ReadToken( const char*& pData )
{
    BVHToken    winnerToken = TOK_NUM_TOKENS;
    UINT        posCount = 0;

    SkipWhitespace( pData );

    BYTE        shortList[ ARRAYSIZE( g_BVHTokenStrings ) ];
    memset( shortList, 1, sizeof( shortList ) );

    for( ;; )
    {
        char k = *pData;
        if( IsEof( k ) )
            break;

        const BVHToken thisStepWinner = FindWinnerToken( k, posCount, shortList );
        if( TOK_NUM_TOKENS == thisStepWinner )
            break;

        winnerToken = thisStepWinner;
        ++pData;
        ++posCount;
    }

    return winnerToken;
}


//--------------------------------------------------------------------------------------
// Name: ConsumeToken()
// Desc: Reads the expected token
//--------------------------------------------------------------------------------------
static
BOOL    ConsumeToken( BVHToken tok, const char*& pData )
{
    return ( tok == ReadToken( pData ) );
}



//--------------------------------------------------------------------------------------
// Name: ReadChannels()
// Desc: Reads BVH channel from the text
//--------------------------------------------------------------------------------------
static
BOOL    ReadChannels( ChannelType* pChannels, UINT& uNumFloatsOut, const char*& pData )
{
    for( UINT i=0; i < CHAN_NONE; ++i )
        pChannels[ i ] = CHAN_NONE;

    uNumFloatsOut = 0;

    char    tmp[ 128 ];

    Tokenize( tmp, ARRAYSIZE( tmp ) - 1, pData );
    if( IsEof( *pData ) )
    {
        // @ERR: unexpected EOF
        return FALSE;
    }

    const UINT uNumChannels = atoi( tmp );
    if( uNumChannels > 6 )
    {
        // @ERR: only know how to deal with pos + rot or rot
        return FALSE;
    }

    for( UINT i=0; i < uNumChannels; ++i )
    {
        const BVHToken tok = ReadToken( pData );

        switch( tok )
        {
        case TOK_XPOS:  pChannels[ i ] = CHAN_POS_X; break;
        case TOK_YPOS:  pChannels[ i ] = CHAN_POS_Y; break;  
        case TOK_ZPOS:  pChannels[ i ] = CHAN_POS_Z; break;
        case TOK_XROT:  pChannels[ i ] = CHAN_ROT_X; break;
        case TOK_YROT:  pChannels[ i ] = CHAN_ROT_Y; break;
        case TOK_ZROT:  pChannels[ i ] = CHAN_ROT_Z; break;
        default:
            // @ERR: expecting channels defineteion
            return FALSE;
        }
    }

    uNumFloatsOut = uNumChannels;

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: ReadJoint()
// Desc: Reads the BVH joint record
//--------------------------------------------------------------------------------------
static
BOOL    ReadJoint( AnimationLoadContainer& anim, const char*& pData, BVHAnimation::NodeName parentName, UINT& uNodeIndex )
{
    // root name
    char strName[ 128 ];
    Tokenize( strName, ARRAYSIZE( strName ), pData );

    const BVHAnimation::NodeName thisName = BVHNodeNameFromString( strName );
    if( BVHAnimation::BVH_NODE_COUNT == thisName )
    {
        // @ERR: node name not recognized
        return FALSE;
    }

    // expect opening bracket now
    if( !ConsumeToken( TOK_OPEN, pData ) )
    {
        // @ERR: expect opening { after ROOT
        return FALSE;
    }

    // add the node
    anim.m_nodes.resize( anim.m_nodes.size() + 1 );

    // expect a number of things here -- child joints, offset or channels
    NodeLoadContainer nodeData;
    memset( &nodeData, 0, sizeof( nodeData ) );
    nodeData.m_name = thisName;
    nodeData.m_uParentIndex = (UINT)parentName;      // use to store a name for now
    nodeData.m_uIndex = uNodeIndex++;                 // don't screw up nodes' ordering within the file

    for(    BVHToken tok = ReadToken( pData );
            tok != TOK_CLOSE;
            tok = ReadToken( pData ) )
    {
        switch( tok )
        {
        default:    // @ERR: unknown token, expecting a known token or }
                    return FALSE;

        case TOK_OFFSET:
                    if( !ReadFloats( nodeData.m_offset, 3, pData ) )
                    {
                        // @ERR: expecting 3 floats
                        return FALSE;
                    }
                    break;

        case TOK_CHANNELS:
                    if( !ReadChannels( nodeData.m_channels, nodeData.m_uNumFloatsPerFrame, pData ) )
                    {
                        // @ERR: expecting properly formatted channels
                        return FALSE;
                    }
                    break;

        case TOK_JOINT:
                    if( !ReadJoint( anim, pData, thisName, uNodeIndex ) )
                        return FALSE;
                    break;

        case TOK_END_SITE:
                    if( !ConsumeToken( TOK_OPEN, pData )    ||
                        !ConsumeToken( TOK_OFFSET, pData )  ||
                        !ReadFloats( NULL, 3, pData )       ||
                        !ConsumeToken( TOK_CLOSE, pData ) )
                    {
                        // @ERR: unexpected End Site formatting
                        return FALSE;
                    }
                    break;
        }
    }

    // store here "atomically" so no vector realloc happens during structure construction
    anim.m_nodes[ nodeData.m_uIndex ] = nodeData;
    
    assert( nodeData.m_name < BVHAnimation::BVH_NODE_COUNT );
    if( anim.m_nodesRemap[ nodeData.m_name ] == ~0ul )
    {
        anim.m_nodesRemap[ nodeData.m_name ] = nodeData.m_uIndex;
    } else
    {
        // @WRN: collapsing two nodes into one
    }

    // add the bone
    if( BVHAnimation::BVH_NODE_COUNT != parentName )
    {
        BoneLoadContainer bone;
        bone.m_self = thisName;
        bone.m_parent = parentName;
        anim.m_bones.push_back( bone );
    }

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: ReadRoot()
// Desc: Reads the BVH root record
//--------------------------------------------------------------------------------------
static
BOOL    ReadRoot( AnimationLoadContainer& anim, const char*& pData )
{
    if( !ConsumeToken( TOK_ROOT, pData ) )
    {
        // @ERR: expected root node
        return FALSE;
    }

    // expect a node
    UINT    nodeIndex = 0;
    if( !ReadJoint( anim, pData, BVHAnimation::BVH_NODE_COUNT, nodeIndex ) )
        return FALSE;

    // -1 marks the root's parent
    anim.m_nodesRemap[ BVHAnimation::BVH_NODE_COUNT ] = ~0ul;

    const UINT numNodes = anim.m_nodes.size();
    assert( nodeIndex == numNodes );

    // fix up self and parent indices
    for( UINT i=0; i < numNodes; ++i )
    {
        const UINT parentIdx = anim.m_nodesRemap[ anim.m_nodes[ i ].m_uParentIndex ];

        assert( anim.m_nodes[ i ].m_uIndex == i );
        anim.m_nodes[ i ].m_uParentIndex = parentIdx;

        // make sure we have the right ordering
        assert( ~0ul == parentIdx || parentIdx < anim.m_nodes[ i ].m_uIndex );
    }

    // generate bones mapping
    for( UINT i=0; i < anim.m_bones.size(); ++i )
    {
        anim.m_bones[ i ].m_bone.startNodeIndex = anim.m_nodesRemap[ anim.m_bones[ i ].m_parent ];
        anim.m_bones[ i ].m_bone.endNodeIndex = anim.m_nodesRemap[ anim.m_bones[ i ].m_self ];

        assert( anim.m_bones[ i ].m_bone.startNodeIndex < anim.m_nodes.size() );
        assert( anim.m_bones[ i ].m_bone.endNodeIndex < anim.m_nodes.size() );
    }

    return TRUE;
}



//--------------------------------------------------------------------------------------
// Name: ReadHierarchy()
// Desc: Reads the BVH hierarchy structure
//--------------------------------------------------------------------------------------
static
BOOL    ReadHierarchy( AnimationLoadContainer& anim, const char*& pData )
{
    if( !ConsumeToken( TOK_HIERARCHY, pData ) )
    {
        // @ERR: expected hierarchy
        return FALSE;
    }
    
    return ReadRoot( anim, pData );
}


//--------------------------------------------------------------------------------------
// Name: ReadMotion()
// Desc: Reads the BVH motion record
//--------------------------------------------------------------------------------------
static
BOOL    ReadMotion( AnimationLoadContainer& anim, const char*& pData )
{
    if( !ConsumeToken( TOK_MOTION, pData ) )
    {
        // @ERR: expected root node
        return FALSE;
    }

    if( !ConsumeToken( TOK_FRAMES, pData )  ||
        !ReadInts( reinterpret_cast< INT* >( &anim.m_uNumFrames ), 1, pData ) )
    {
        // @ERR: expected FRames: N
        return FALSE;
    }

    if( !ConsumeToken( TOK_FRAME_TIME, pData )  ||
        !ReadFloats( &anim.m_fFrameTime, 1, pData ) )
    {
        // @ERR: expected FRames: N
        return FALSE;
    }

    // calculate how many floats we actually expect
    anim.m_uNumFloatsPerFrame = 0;
    for( UINT i=0; i < anim.m_nodes.size(); ++i )
        anim.m_uNumFloatsPerFrame += anim.m_nodes[ i ].m_uNumFloatsPerFrame;

    // reserve memory
    const UINT uNumFloatsTotal = anim.m_uNumFloatsPerFrame * anim.m_uNumFrames;
    anim.m_frameData.resize( uNumFloatsTotal );

    // read the data
    UINT j;
    for( j=0; j < anim.m_uNumFrames; ++j )
    {
        if( !ReadFloats( &anim.m_frameData[ j * anim.m_uNumFloatsPerFrame ], anim.m_uNumFloatsPerFrame, pData ) )
        {
            // @ERR: technically an error but let's be lenient
            break;
        }
    }

    // last one is not complete
    anim.m_uNumFrames = j - 1;

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: ParseBVHFile()
// Desc: Given the text returns a new BVH animation if it can be loaded from the text
//--------------------------------------------------------------------------------------
BVHAnimation*   BVHAnimation::ParseBVHFile( const char* pData )
{
    static_assert( ARRAYSIZE( g_BVHNodeNames ) == BVH_NODE_COUNT, "string to enum mismatch" );

    AnimationLoadContainer   anim;
    memset( &anim.m_nodesRemap[ 0 ], 0xff, sizeof( anim.m_nodesRemap ) );

    if( !ReadHierarchy( anim, pData ) ||
        !ReadMotion( anim, pData ) )
    {
        return NULL;
    }

    BVHAnimation*   pAnim = new BVHAnimation;

    pAnim->m_numBones = anim.m_bones.size();
    pAnim->m_numFloatsPerFrame = anim.m_uNumFloatsPerFrame;
    pAnim->m_numFrames = anim.m_uNumFrames;
    pAnim->m_numNodes = anim.m_nodes.size();
    pAnim->m_frameDuration = anim.m_fFrameTime;
    pAnim->m_pAnimData = new FLOAT[ pAnim->m_numFloatsPerFrame * pAnim->m_numFrames ];
    memcpy( pAnim->m_pAnimData, &anim.m_frameData[ 0 ], pAnim->m_numFloatsPerFrame * pAnim->m_numFrames * 4 );

    pAnim->m_pNodes = new Node[ pAnim->m_numNodes ];
    for( UINT i=0; i < pAnim->m_numNodes; ++i )
    {
        BVHAnimation::Node& dest = pAnim->m_pNodes[ i ];
        const NodeLoadContainer& src = anim.m_nodes[ i ];

        for( UINT j=0; j < 3; ++j )
            dest.m_offset[ j ] = src.m_offset[ j ];

        dest.m_name = src.m_name;
        dest.m_uParentIndex = static_cast< BYTE >( src.m_uParentIndex );
        dest.m_numFloatsPerFrame = static_cast< BYTE >( src.m_uNumFloatsPerFrame );

        UINT curRotOrder = 0;
        for( UINT j=0; j < 6; ++j )
        {
            dest.m_channels[ j ] = static_cast< BYTE >( src.m_channels[ j ] );

            if( src.m_channels[ j ] >= CHAN_ROT_X &&
                src.m_channels[ j ] <= CHAN_ROT_Z )
            {
                assert( curRotOrder <= 3 );

                dest.m_rotOrder[ curRotOrder++ ] = static_cast< BYTE >( src.m_channels[ j ] - CHAN_ROT_X );
            }
        }

        assert( 3 == curRotOrder );
    }

    pAnim->m_pBones = new Bone[ pAnim->m_numBones ];
    for( UINT i=0; i < pAnim->m_numBones; ++i )
    {
        pAnim->m_pBones[ i ].startNodeIndex = anim.m_bones[ i ].m_bone.startNodeIndex;
        pAnim->m_pBones[ i ].endNodeIndex = anim.m_bones[ i ].m_bone.endNodeIndex;
    }

    pAnim->m_pMatrices = new XMMATRIX[ pAnim->m_numNodes ];
    assert( 0 == (((UINT)pAnim->m_pMatrices) & 0x0f) );

    return pAnim;
}


//--------------------------------------------------------------------------------------
// Name: LoadBVH()
// Desc: Given a file name tries to load a BVH animation from it
//--------------------------------------------------------------------------------------
BVHAnimation*   BVHAnimation::LoadBVH( const char* pFilename )
{
    FILE* fp;
    if( fopen_s( &fp, pFilename, "rb" ) )
        return NULL;
    assert( fp != NULL );

    fseek( fp, 0, SEEK_END );
    const long fileSize = ftell( fp );
    fseek( fp, 0, SEEK_SET );

    char* pData = new char[ fileSize + 1 ];

    fread( pData, 1, fileSize, fp );

    pData[ fileSize ] = 0;

    fclose( fp );

    BVHAnimation*   pBvh = ParseBVHFile( pData );

    delete[] pData;

    return pBvh;
}



