//----------------------------------------------------------------------------------------------------------------------
// XEDToBVH.cpp
// 
// Converts the Xbox Studio recorded data file (.xed) to mocap file (.bvh)
//
// Microsoft Advanced Technology Group.
// Copyright (c) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "CXEDFile.h"
#include "XEDFileIterators.h"

#include <string>
#include <vector>
#include <set>

static LPCTSTR s_pszHelpText = 
__T( "\n" )
__T( "Converts Xbox Studio recorded data file .xed to mocap file .bvh\n\n" )
__T( "Usage:\n"                                                            )
__T( "XEDToBVH.exe /R n inputfile.xed outputfile.bvh\n\n"                  )
__T( "Where n is the rest pose frame number in the input .xed file\n"      );

enum BVH_NODE
{
    BVH_NODE_HIP_CENTER_LEFT = 0    ,// NUI_SKELETON_POSITION_HIP_CENTER,
    BVH_NODE_HIP_CENTER_RIGHT       ,// NUI_SKELETON_POSITION_HIP_CENTER,
    BVH_NODE_SPINE                  ,// NUI_SKELETON_POSITION_SPINE,
    BVH_NODE_SPINE1                 ,// (NUI_SKELETON_POSITION_SPINE + NUI_SKELETON_POSITION_SHOULDER_CENTER) / 2,
    BVH_NODE_SHOULDER_CENTER        ,// NUI_SKELETON_POSITION_SHOULDER_CENTER,
    BVH_NODE_SHOULDER_CENTER_LEFT   ,// NUI_SKELETON_POSITION_SHOULDER_CENTER,
    BVH_NODE_SHOULDER_CENTER_RIGHT  ,// NUI_SKELETON_POSITION_SHOULDER_CENTER,
    BVH_NODE_HEAD                   ,// NUI_SKELETON_POSITION_HEAD,
    BVH_NODE_SHOULDER_LEFT          ,// NUI_SKELETON_POSITION_SHOULDER_LEFT,
    BVH_NODE_ELBOW_LEFT             ,// NUI_SKELETON_POSITION_ELBOW_LEFT,
    BVH_NODE_WRIST_LEFT             ,// NUI_SKELETON_POSITION_WRIST_LEFT,
    BVH_NODE_HAND_LEFT              ,// NUI_SKELETON_POSITION_HAND_LEFT,
    BVH_NODE_SHOULDER_RIGHT         ,// NUI_SKELETON_POSITION_SHOULDER_RIGHT,
    BVH_NODE_ELBOW_RIGHT            ,// NUI_SKELETON_POSITION_ELBOW_RIGHT,
    BVH_NODE_WRIST_RIGHT            ,// NUI_SKELETON_POSITION_WRIST_RIGHT,
    BVH_NODE_HAND_RIGHT             ,// NUI_SKELETON_POSITION_HAND_RIGHT,
    BVH_NODE_HIP_LEFT               ,// NUI_SKELETON_POSITION_HIP_LEFT,
    BVH_NODE_KNEE_LEFT              ,// NUI_SKELETON_POSITION_KNEE_LEFT,
    BVH_NODE_ANKLE_LEFT             ,// NUI_SKELETON_POSITION_ANKLE_LEFT,
    BVH_NODE_FOOT_LEFT              ,// NUI_SKELETON_POSITION_FOOT_LEFT,
    BVH_NODE_HIP_RIGHT              ,// NUI_SKELETON_POSITION_HIP_RIGHT,
    BVH_NODE_KNEE_RIGHT             ,// NUI_SKELETON_POSITION_KNEE_RIGHT,
    BVH_NODE_ANKLE_RIGHT            ,// NUI_SKELETON_POSITION_ANKLE_RIGHT,
    BVH_NODE_FOOT_RIGHT             ,// NUI_SKELETON_POSITION_FOOT_RIGHT,
    BVH_NODE_COUNT                  
};

const NUI_SKELETON_POSITION_INDEX BVHNodeToNUI[BVH_NODE_COUNT] =
{
    NUI_SKELETON_POSITION_HIP_CENTER,
    NUI_SKELETON_POSITION_HIP_CENTER,
    NUI_SKELETON_POSITION_SPINE,
    NUI_SKELETON_POSITION_COUNT,     // BVH_NODE_SPINE1
    NUI_SKELETON_POSITION_SHOULDER_CENTER,
    NUI_SKELETON_POSITION_SHOULDER_CENTER,
    NUI_SKELETON_POSITION_SHOULDER_CENTER,
    NUI_SKELETON_POSITION_HEAD,
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
    NUI_SKELETON_POSITION_FOOT_RIGHT,    
};

const TCHAR* BVHNodeName[BVH_NODE_COUNT] = 
{
    __T("HipCenterLeft"),
    __T("HipCenterRight"),
    __T("Spine"),
    __T("Spine1"),
    __T("ShoulderCenter"),
    __T("ShoulderCenterLeft"),
    __T("ShoulderCenterRight"),
    __T("Head"),
    __T("LeftShoulder"),
    __T("LeftElbow"),
    __T("LeftWrist"),
    __T("LeftHand"),
    __T("RightShoulder"),
    __T("RightElbow"),
    __T("RightWrist"),
    __T("RightHand"),
    __T("LeftHip"),
    __T("LeftKnee"),
    __T("LeftAnkle"),
    __T("LeftFoot"),
    __T("RightHip"),
    __T("RightKnee"),
    __T("RightAnkle"),
    __T("RightFoot"),
};

struct BVHBone
{
    BVH_NODE   StartJoint;
    BVH_NODE   EndJoint;

    static BVHBone MakeBVHBone( BVH_NODE StartJoint, BVH_NODE EndJoint )
    {
        BVHBone r;        
        r.StartJoint = StartJoint;
        r.EndJoint = EndJoint;
        return r;
    }
};

const BVHBone g_Bones[] =
{
    // Head
    { BVH_NODE_SPINE1,                  BVH_NODE_SHOULDER_CENTER },  
    { BVH_NODE_SHOULDER_CENTER,         BVH_NODE_HEAD },

    // Right arm
    { BVH_NODE_SPINE1,                  BVH_NODE_SHOULDER_CENTER_RIGHT },   
    { BVH_NODE_SHOULDER_CENTER_RIGHT,   BVH_NODE_SHOULDER_RIGHT },   
    { BVH_NODE_SHOULDER_RIGHT,          BVH_NODE_ELBOW_RIGHT },      
    { BVH_NODE_ELBOW_RIGHT,             BVH_NODE_WRIST_RIGHT },      
    { BVH_NODE_WRIST_RIGHT,             BVH_NODE_HAND_RIGHT },       

    // Left arm
    { BVH_NODE_SPINE1,                  BVH_NODE_SHOULDER_CENTER_LEFT },   
    { BVH_NODE_SHOULDER_CENTER_LEFT,    BVH_NODE_SHOULDER_LEFT },    
    { BVH_NODE_SHOULDER_LEFT,           BVH_NODE_ELBOW_LEFT },       
    { BVH_NODE_ELBOW_LEFT,              BVH_NODE_WRIST_LEFT },       
    { BVH_NODE_WRIST_LEFT,              BVH_NODE_HAND_LEFT },        

    // Right leg and foot
    { BVH_NODE_SPINE,                   BVH_NODE_HIP_CENTER_RIGHT },   
    { BVH_NODE_HIP_CENTER_RIGHT,        BVH_NODE_HIP_RIGHT },   
    { BVH_NODE_HIP_RIGHT,               BVH_NODE_KNEE_RIGHT  },      
    { BVH_NODE_KNEE_RIGHT,              BVH_NODE_ANKLE_RIGHT },      
    { BVH_NODE_ANKLE_RIGHT,             BVH_NODE_FOOT_RIGHT },       

    // Left leg and foot
    { BVH_NODE_SPINE,                   BVH_NODE_HIP_CENTER_LEFT },   
    { BVH_NODE_HIP_CENTER_LEFT,         BVH_NODE_HIP_LEFT },   
    { BVH_NODE_HIP_LEFT,                BVH_NODE_KNEE_LEFT  },       
    { BVH_NODE_KNEE_LEFT,               BVH_NODE_ANKLE_LEFT },       
    { BVH_NODE_ANKLE_LEFT,              BVH_NODE_FOOT_LEFT },    

    // Spine
    { BVH_NODE_SPINE1,                  BVH_NODE_SPINE },   
};

const UINT g_uNumBones = ARRAYSIZE( g_Bones );

// The root of the hierarchical structure of BVH skeleton
const BVH_NODE g_BVHRoot = BVH_NODE_SPINE1;

// Records the depth-first order of the bones used to construct the hierarchical structure of BVH skeleton
std::vector<BVHBone> g_BoneOrder;

// The children of each BVH node, defined by g_BVHRoot and g_Bones
std::vector<BVH_NODE> g_NodeChildren[BVH_NODE_COUNT];

INT ConvertToBVH( const LPCTSTR pszInputFile, const INT iRestFrame, const LPCTSTR pszOutputFile );
VOID HIERARCHYRecursive( FILE* pOutFile, XMVECTOR pRestSkeletonPositions[], BVH_NODE lastnode, BVH_NODE node, INT depth );
VOID MOTIONWrite( FILE* pOutFile, XMVECTOR pFrameSkeletonPositions[], XMVECTOR pRestSkeletonPositions[] );

//----------------------------------------------------------------------------------------------------------------------
// Outputs end user help to the console.
//----------------------------------------------------------------------------------------------------------------------
VOID ShowHelp()
{
    _putts( s_pszHelpText );
}

//----------------------------------------------------------------------------------------------------------------------
// Main entry point for the console application.
//----------------------------------------------------------------------------------------------------------------------
int _tmain(int argc, _TCHAR* argv[])
{
    // The xedfile.dll contains the functionality that this sample needs. However
    // this DLL is not in the system path, and shouldn't be. To avoid this we
    // get DebugConsole to add %XEDK%\bin\win32 to its own copy of the path.
    // Then it loads xbdm.dll. This works because the project specifies that
    // xbdm.dll should be delay loaded, so the Win32 loader doesn't load it
    // when the process loads. 
    char* path;
    _dupenv_s( &path, NULL, "path" );

    char* xedkDir;
    _dupenv_s( &xedkDir, NULL, "xedk" );

    if( !xedkDir )
        xedkDir = "";
    // Build up a new path using std::string to handle memory management.
    std::string newpath = "path=" + std::string( path ) + ";" + std::string( xedkDir ) + "\\bin\\win32";
    // Set the path to the update version.
    _putenv( newpath.c_str() );

    if ( argc == 1 ||
        _tcscmp( argv[1], _T("-h") ) == 0 ||
        _tcscmp( argv[1], _T("-help") ) == 0 ||
        _tcscmp( argv[1], _T("/?") ) == 0
        )
    {
        ShowHelp();
        return 0;
    }

    if ( (_tcscmp( argv[1], _T("/R") ) == 0 || _tcscmp( argv[1], _T("/r") ) == 0 || 
          _tcscmp( argv[1], _T("-R") ) == 0 || _tcscmp( argv[1], _T("-r") ) == 0 ) && argc == 5 )
    {
        // Specify rest pose frame and then convert to .bvh
        // XEDToBVH.exe /R n inputfile.xed outputfile.bvh
        // Where n is the rest pose frame number

        INT iRestFrame = _tstoi( argv[2] );
        if ( iRestFrame == 0 && _tcscmp( argv[2], _T("0") ) != 0 )
        {
            ShowHelp();
            return -1;
        }

        return ConvertToBVH( argv[3], iRestFrame, argv[4] );
    } else
    {
        ShowHelp();
        return -1;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Generates the BVH file, return 0 if successful
//----------------------------------------------------------------------------------------------------------------------
INT ConvertToBVH( const LPCTSTR pszInputFile, const INT iRestFrame, const LPCTSTR pszOutputFile )
{
    CXEDFile input;
    HRESULT hr;
    INT result = 0;

    hr = input.Open( pszInputFile );
    if ( FAILED( hr ) )
    {
        _tprintf( _T("\nError: Couldn't open input file \'%s\' (error: %x).\n"),
                  pszInputFile, hr );
        return -1;
    }

    XEDMultiFrameRange info = input.GetMultiFrameInfo();
    if ( !info.HasSkeletonFrames() )
    {
        _putts( _T("No Skeleton information in file.\n") );
        return -1;
    }

    if ( iRestFrame < (INT)info.skeleton.start.frameNumber || iRestFrame > (INT)info.skeleton.end.frameNumber )
    {
        _tprintf( _T("Error: Frame %d is out of range, since the input file has frame %u to %u\n"), 
                  iRestFrame, info.skeleton.start.frameNumber, info.skeleton.end.frameNumber );
        return -1;
    }

    FILE* pOutFile = NULL;

    if ( pszOutputFile != NULL )
    {
        if ( _tfopen_s( &pOutFile, pszOutputFile, _T("wt") ) != 0)
        {
            _tprintf( _T("\nError: Couldn't open output file \'%s\'.\n"),
                pszOutputFile );
            return -1;
        }
    }

    INT nFrames = 0;
    INT i = 0;
    
    CXEDSkeletonFrame sf;
    hr = input.ReadFrame( iRestFrame - (INT)info.skeleton.start.frameNumber, &sf );
    if ( FAILED( hr ) )
    {
        _tprintf( _T("Error: cannot read rest pose frame %d, ")
                  _T("please check if there are too many discontinuous frames before this frame number in the input .xed file\n"), iRestFrame );
        result = -1;
        goto quit;
    }

    INT iSkeletonIndex = 0;
    if( sf.GetData()->SkeletonData[ iSkeletonIndex ].eTrackingState != NUI_SKELETON_TRACKED )
    {
        // Since we don't have a lock on the skeleton currently being used by the sample, try switching 
        // to the first tracked skeleton we can find.
        for( UINT i = 0; i < NUI_SKELETON_COUNT; ++ i )
        {
            if( sf.GetData()->SkeletonData[ i ].eTrackingState == NUI_SKELETON_TRACKED )
            {
                iSkeletonIndex = i;
                break;
            }
        }
    }

    // Step 1: write out the hierarchical structure of the BVH skeleton
    _ftprintf( pOutFile, _T("HIERARCHY\n") );
    HIERARCHYRecursive( pOutFile, sf.GetData()->SkeletonData[iSkeletonIndex].SkeletonPositions, g_BVHRoot, g_BVHRoot, 0 );

    // Step 2: write out the motion data for each frame
    _ftprintf( pOutFile, _T("MOTION\n") );
    nFrames = (INT)info.skeleton.end.frameNumber - (INT)info.skeleton.start.frameNumber;
    _ftprintf( pOutFile, _T("Frames: %d\n"), nFrames );
    _ftprintf( pOutFile, _T("Frame Time: 0.033333\n") );    
    for ( i = 0; i < nFrames; ++i )
    {
        CXEDSkeletonFrame sfCurrent;
        hr = input.ReadFrame( i, &sfCurrent );
        if ( FAILED( hr ) )
        {
            result = -1;
            goto quit;
        }

        MOTIONWrite( pOutFile, sfCurrent.GetData()->SkeletonData[iSkeletonIndex].SkeletonPositions, sf.GetData()->SkeletonData[iSkeletonIndex].SkeletonPositions );
    }

quit:
    fclose( pOutFile );    

    if ( result == 0 )
    {
        _tprintf( _T("Succeeded\n") );
    }
    else
    {
        if ( i < nFrames )
        {
            _tprintf( _T("Warning: Input .xed file reports it has %d frames, but only %d frames can be read, ") 
                      _T("this usually means there are discontinuous frames in it.\n\n"), nFrames, i - 1 );
            _tprintf( _T("The output .bvh file may not be readable in MotionBuilder. ")
                      _T("If this happens, open the output .bvh file in a text editor and search and replace 'Frames: %d' to 'Frames: %d'"), nFrames, i - 1 );

            result = -2;
        } 
    }
    
    return result;
}

//----------------------------------------------------------------------------------------------------------------------
// Calculate the BVH node position from the NUI skeleton
//----------------------------------------------------------------------------------------------------------------------
XMVECTOR GetBVHNodePosition( XMVECTOR pNUISkeletonPositions[], BVH_NODE node )
{
    if ( node == BVH_NODE_SPINE1 )
        return (pNUISkeletonPositions[NUI_SKELETON_POSITION_SPINE] + pNUISkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER]) / 2;

    return pNUISkeletonPositions[BVHNodeToNUI[node]];
}

//----------------------------------------------------------------------------------------------------------------------
// Coverts a rotation represented by an axis and angle to euler representation
//----------------------------------------------------------------------------------------------------------------------
XMVECTOR AxisAngleToEuler( const FLOAT x, const FLOAT y, const FLOAT z, const FLOAT angle ) 
{
    FLOAT rx, ry, rz;
    
    FLOAT s = sin( angle );
    FLOAT c = cos( angle );
    FLOAT t = 1 - c;
    
    if ( ( x * y * t + z * s ) > 0.998 ) 
    { 
        // north pole singularity detected
        ry = 2 * atan2( x * sin(angle/2), cos(angle/2) );
        rz = XM_PI / 2;
        rx = 0;
        return XMVectorSet( rx, ry, rz, 0 );
    }
    if ( ( x * y * t + z * s ) < -0.998 ) 
    { 
        // south pole singularity detected
        ry = -2 * atan2( x * sin(angle/2), cos(angle/2) );
        rz = -XM_PI / 2;
        rx = 0;
        return XMVectorSet( rx, ry, rz, 0 );
    }
        
    ry = atan2(y * s - x * z * t , 1 - ( y * y+ z * z ) * t);       
    rz = asin(x * y * t + z * s) ;        
    rx = atan2(x * s - y * z * t , 1 - ( x * x + z * z ) * t);

    return XMVectorSet( rx, ry, rz, 0 );
}

//----------------------------------------------------------------------------------------------------------------------
// Calculate the rotation between the direction in rest frame and current frame,
// if base rotation is passed in, then output the delta rotation
//----------------------------------------------------------------------------------------------------------------------
XMVECTOR GetRotationInEuler( XMVECTOR pFrameSkeletonPositions[], XMVECTOR pRestSkeletonPositions[], BVH_NODE a0, BVH_NODE a1, XMMATRIX* pBaseRot = NULL, XMMATRIX* pRotOut = NULL )
{
    XMVECTOR dirRest = XMVector3Normalize(GetBVHNodePosition(pRestSkeletonPositions, a0) - GetBVHNodePosition(pRestSkeletonPositions, a1));
    XMVECTOR dirFrame = XMVector3Normalize(GetBVHNodePosition(pFrameSkeletonPositions, a0) - GetBVHNodePosition( pFrameSkeletonPositions, a1));    

    XMMATRIX matBase = XMMatrixIdentity();
    if ( pBaseRot )
        matBase = *pBaseRot;

    XMVECTOR d;
    dirFrame = XMVector3TransformNormal( dirFrame, XMMatrixInverse( &d, matBase) );

    // Rotation axis
    XMVECTOR axis = XMVector3Cross( dirRest, dirFrame );
    if ( XMVectorGetX(XMVector3Length(axis)) < 0.000001 )
    {
        if ( pRotOut )
            *pRotOut = matBase;
        return XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    }

    axis = XMVector3Normalize(axis);
    XMVECTOR angle = XMVector3AngleBetweenVectors( dirRest, dirFrame ) ;
    XMMATRIX r = XMMatrixRotationAxis( axis, XMVectorGetX(angle) );
    if ( pRotOut )
        *pRotOut = r * matBase;
    
    XMVECTOR euler;    
    euler = AxisAngleToEuler( XMVectorGetX(axis), XMVectorGetY(axis), XMVectorGetZ(axis), XMVectorGetX(angle) );    
    return euler / XM_PI * 180;
}

//----------------------------------------------------------------------------------------------------------------------
// Output the motion data for a single frame in BVH
//----------------------------------------------------------------------------------------------------------------------
VOID MOTIONWrite( FILE* pOutFile, XMVECTOR pFrameSkeletonPositions[], XMVECTOR pRestSkeletonPositions[] )
{
    std::set<BVH_NODE> parentVisited;
    
    XMMATRIX parentRot[BVH_NODE_COUNT];
    for ( INT i = 0; i < BVH_NODE_COUNT; ++i )
        parentRot[i] = XMMatrixIdentity();
    
    for ( INT i = 0; i < (INT)g_BoneOrder.size(); ++i )
    {
        BVHBone &bone = g_BoneOrder[i];

        if ( bone.StartJoint == g_BVHRoot && bone.EndJoint == g_BVHRoot )
        {
            XMVECTOR offset = GetBVHNodePosition( pFrameSkeletonPositions, g_BVHRoot) - GetBVHNodePosition(pRestSkeletonPositions, g_BVHRoot);
            offset *= 100;

            XMMATRIX r;
            XMVECTOR rot = GetRotationInEuler( pFrameSkeletonPositions, pRestSkeletonPositions, BVH_NODE_SHOULDER_CENTER,  BVH_NODE_SPINE1, NULL, &r );            
            _ftprintf( pOutFile, _T("%f %f %f %f %f %f "), 
                XMVectorGetX(offset), XMVectorGetY(offset), XMVectorGetZ(offset), 
                XMVectorGetY(rot), XMVectorGetZ(rot), XMVectorGetX(rot) );

            parentVisited.insert( bone.StartJoint );
            for ( INT j = 0; j < (INT)g_NodeChildren[bone.StartJoint].size(); ++j )
            {
                BVH_NODE child = g_NodeChildren[bone.StartJoint][j];
                parentRot[child] = r;
            }
        } 
        else
        if ( bone.StartJoint == BVH_NODE_COUNT && bone.EndJoint == BVH_NODE_COUNT )
        {
            _ftprintf( pOutFile, _T("0.0 0.0 0.0 ") );
        } 
        else        
        {            
            if ( parentVisited.find(bone.StartJoint) == parentVisited.end() )
            {
                XMMATRIX r;
                XMVECTOR rot = GetRotationInEuler( pFrameSkeletonPositions, pRestSkeletonPositions, bone.EndJoint, bone.StartJoint, &parentRot[bone.StartJoint], &r );   
                _ftprintf( pOutFile, _T("%f %f %f "), XMVectorGetY(rot), XMVectorGetZ(rot), XMVectorGetX(rot) );

                parentVisited.insert( bone.StartJoint );
                for ( INT j = 0; j < (INT)g_NodeChildren[bone.StartJoint].size(); ++j )
                {
                    BVH_NODE child = g_NodeChildren[bone.StartJoint][j];
                    parentRot[child] = r;
                }
            }            
        }
    }

    _ftprintf( pOutFile, _T("\n") );
}

//----------------------------------------------------------------------------------------------------------------------
// Ident in the output BVH file to increase readability
//----------------------------------------------------------------------------------------------------------------------
VOID Ident( FILE* pOutFile, INT depth )
{
    for ( INT i = 0; i < depth; ++i )
        _ftprintf( pOutFile, _T("\t") );
}

//----------------------------------------------------------------------------------------------------------------------
// Recursively constructs and outputs the structure of the BVH skeleton hierarchy
//----------------------------------------------------------------------------------------------------------------------
VOID HIERARCHYRecursive( FILE* pOutFile, XMVECTOR pRestSkeletonPositions[], BVH_NODE lastnode, BVH_NODE node, INT depth )
{
    // Node name
    if ( node == g_BVHRoot )
        _ftprintf( pOutFile, _T("ROOT %s\n"), BVHNodeName[node] );
    else
    {
        for ( INT i = 0; i < depth; ++i )
            _ftprintf( pOutFile, _T("\t") );
        _ftprintf( pOutFile, _T("JOINT %s\n"), BVHNodeName[node] );        
    }

    g_BoneOrder.push_back( BVHBone::MakeBVHBone(lastnode, node) );

    // {    
    Ident( pOutFile, depth );
    _ftprintf( pOutFile, _T("{\n") );

    // OFFSET
    Ident( pOutFile, depth + 1 );
    XMVECTOR offset = GetBVHNodePosition(pRestSkeletonPositions, node) - GetBVHNodePosition( pRestSkeletonPositions, lastnode );
    offset *= 100;
    _ftprintf( pOutFile, _T("OFFSET %f %f %f\n"), XMVectorGetX(offset), XMVectorGetY(offset), XMVectorGetZ(offset) );

    // CHANNELS
    Ident( pOutFile, depth + 1 );
    if ( node == g_BVHRoot )
    {        
       	_ftprintf( pOutFile, _T("CHANNELS 6 Xposition Yposition Zposition Yrotation Zrotation Xrotation\n") );
    } 
    else
    {        
        _ftprintf( pOutFile, _T("CHANNELS 3 Yrotation Zrotation Xrotation\n") );
    }

    static INT BoneUsed[10];    // assume the maximum depth is smaller than 10
    if ( depth == 0 )
        ZeroMemory( &BoneUsed, 10 * sizeof(INT) );
    
    // Find children
    BOOL bLeaf = TRUE;
    for ( INT i = 0; i < g_uNumBones; ++i )
    {
        // Skip any already used bones
        for ( INT j = 0; j < depth; ++j )
            if ( BoneUsed[j] == i )
                goto cont;
        
        if ( g_Bones[i].StartJoint == node )
        {
            bLeaf = FALSE;
            BoneUsed[depth] = i;
            HIERARCHYRecursive( pOutFile, pRestSkeletonPositions, node, g_Bones[i].EndJoint, depth + 1 );
            g_NodeChildren[node].push_back( g_Bones[i].EndJoint );            
        }
        else
        if ( g_Bones[i].EndJoint == node )
        {
            bLeaf = FALSE;
            BoneUsed[depth] = i;
            HIERARCHYRecursive( pOutFile, pRestSkeletonPositions, node, g_Bones[i].StartJoint, depth + 1 );
            g_NodeChildren[node].push_back( g_Bones[i].StartJoint );            
        }
cont:
        ;
    }
    
    // End Site
    if ( bLeaf )
    {
        g_BoneOrder.push_back( BVHBone::MakeBVHBone(BVH_NODE_COUNT, BVH_NODE_COUNT) );
        
        Ident( pOutFile, depth + 1 );
        _ftprintf( pOutFile, _T("End Site\n") );

        Ident( pOutFile, depth + 1 );
        _ftprintf( pOutFile, _T("{\n") );

        Ident( pOutFile, depth + 2 );
        _ftprintf( pOutFile, _T("OFFSET %f %f %f\n"), 0.0, 0,0, 0.0 );

        Ident( pOutFile, depth + 1 );
        _ftprintf( pOutFile, _T("}\n") );
    }

    // }
    Ident( pOutFile, depth );
    _ftprintf( pOutFile, _T("}\n") );
}
