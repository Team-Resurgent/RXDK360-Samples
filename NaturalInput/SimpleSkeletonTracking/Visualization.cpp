//--------------------------------------------------------------------------------------
// Visualization.cpp
//
// Defines functions used to visualize simple skeleton tracking. The purpose of this 
// sample is not to focus on how to visualize the data, but rather to show how to use the 
// APIs correctly to get the data.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <AtgUtil.h>
#include <d3d9types.h>
#include <AtgDebugDraw.h>
#include <AtgSimpleShaders.h>

#include "Visualization.h"

// Use green when bone confidence is high and red when it is low.
#define BONE_CONFIDENCE_HIGH_COLOR  0xff00ff00
#define BONE_CONFIDENCE_LOW_COLOR   0xffff0000

//--------------------------------------------------------------------------------------
// Data structures
//--------------------------------------------------------------------------------------

// Each skeleton bone has two joints
struct BoneJoints
{
    NUI_SKELETON_POSITION_INDEX   StartJoint;
    NUI_SKELETON_POSITION_INDEX   EndJoint;
};


// Defines a rectangle by its origin and size.
typedef struct
{
    FLOAT fX;
    FLOAT fY;
    FLOAT fWidth;
    FLOAT fHeight;
}Rect;


struct VideoFeedVertex
{
    FLOAT vPosition[ 3 ];
    FLOAT vTexCoords[ 2 ];
};


//--------------------------------------------------------------------------------------
// Constants and defines
//--------------------------------------------------------------------------------------

#define FLT_EPSILON         1.192092896e-07F


// Define the bones in the skeleton using joint indices
const BoneJoints g_Bones[] =
{
    // Head
    { NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER },                // Top of head to top of neck

    // Right arm
    { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT },      // Neck bottom to right shoulder internal
    { NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT },          // Right shoulder internal to right elbow
    { NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT},              // Right elbow to right wrist
    { NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT },              // Right wrist to right hand

    // Left arm
    { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT },       // Neck bottom to left shoulder internal
    { NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT},             // Left shoulder internal to left elbow
    { NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT},                // Left elbow to left wrist
    { NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT },                // Left wrist to left hand

    // Right leg and foot
    { NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT },                // Right hip internal to right knee
    { NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT },              // Right knee to right ankle
    { NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT },              // Left ankle to left foot

    // Left leg and foot
    { NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT },                  // Left hip internal to left knee
    { NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT},                 // Left knee to left ankle
    { NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT },                // Left ankle to left foot

    // Spine
    { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SPINE},                // Neck bottom to spine
    { NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_HIP_CENTER },                    // Spine to hip center

    // Hips
    { NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_HIP_CENTER },                // Right hip to hip center
    { NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT}                   // Hip center to left hip
};

const UINT g_uNumBones = ARRAYSIZE( g_Bones );


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------

IDirect3DVertexDeclaration9* m_pVideoVertexDecl;     // Vertex format decl
IDirect3DVertexShader9*      m_pVideoVertexShader;   // Vertex Shader
IDirect3DPixelShader9*       m_pVideoPixelShaderRGB; // Pixel Shader for RGB image

IDirect3DTexture9* m_pColorTexture = NULL;
IDirect3DTexture9* m_pDepthTexture = NULL;

Rect ColorWindow;   // Screen output area for the color feed
Rect DepthWindow;   // Screen output area for the color feed

// Each stream can have different dimensions. To display it correctly we need to
// know the dimensions of each stream seperately. 
DWORD   g_dwColorStreamWidth    = 0;
DWORD   g_dwColorStreamHeight   = 0;
DWORD   g_dwDepthStreamWidth    = 0;
DWORD   g_dwDepthStreamHeight   = 0;

// Color lookup table to display depth values as color. Depth values can be displayed
// as color values in many different ways, we chose to use a ramp lookup table
D3DCOLOR    g_DepthColorTable[ 512 ];

// The screen space joint data will be stored in this array.
XMFLOAT2 g_ScreenSpaceJoints[ NUI_SKELETON_POSITION_COUNT ];
XMFLOAT2 g_ScreenSpaceJointsColorMap[ NUI_SKELETON_POSITION_COUNT ];

//-------------------------------------------------------------------------------------
// Video and pixel shader definitions
//-------------------------------------------------------------------------------------
static const char*  g_strVideoShaderHLSL =
    " struct VS_OUT                                                              "
    " {                                                                          "
    "     float4 Position : POSITION;                                            "
    "     float2 TexCoord : TEXCOORD0;                                           "
    " };                                                                         "
    "                                                                            "
    " sampler VideoTexture : register(s0);                                       "
    "                                                                            "
    " VS_OUT VideoVertexShader( const float3 Position : POSITION,                "
    "                           const float2 TexCoord : TEXCOORD0 )              "
    " {                                                                          "
    "     VS_OUT Output;                                                         "
    "     Output.Position.x  = ( Position.x-0.5);                                "
    "     Output.Position.y  = ( Position.y-0.5);                                "
    "     Output.Position.z  = ( 0.0 );                                          "
    "     Output.Position.w  = ( 1.0 );                                          "
    "     Output.TexCoord = TexCoord;                                            "
    "     return Output;                                                         "
    " }                                                                          "
    "                                                                            "
    " float4 VideoPixelShader( VS_OUT Input ) : COLOR                            "
    " {                                                                          "
    "     return tex2D( VideoTexture, Input.TexCoord );                          "
    " }                                                                          ";


//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------

VOID InitializeDepthColorTable();
HRESULT InitializeVideoTextures( D3DDevice* pd3dDevice, 
                                 IDirect3DTexture9** ppVideoTexture, 
                                 DWORD dwWidth, DWORD dwHeight, D3DFORMAT format );


//--------------------------------------------------------------------------------------
// Name: InitializeVisualization()
// Desc: Creates visualization buffers and shaders
//--------------------------------------------------------------------------------------
HRESULT InitializeVisualization( D3DDevice* pd3dDevice,
                                 DWORD dwColorStreamWidth, DWORD dwColorStreamHeight,
                                 DWORD dwDepthStreamWidth, DWORD dwDepthStreamHeight )
{
    // Set the dimension of each stream since we will be using them during visualization
    g_dwColorStreamWidth    = dwColorStreamWidth;
    g_dwColorStreamHeight   = dwColorStreamHeight;
    g_dwDepthStreamWidth    = dwDepthStreamWidth;
    g_dwDepthStreamHeight   = dwDepthStreamHeight;

    // Setup a ramp lookup table to display depth values as colors
    InitializeDepthColorTable();

    // Compile vertex shader.
    ID3DXBuffer* pVertexShaderCode;
    ID3DXBuffer* pVertexErrorMsg;
    HRESULT hr = D3DXCompileShader( g_strVideoShaderHLSL,
                                    ( UINT )strlen( g_strVideoShaderHLSL ),
                                    NULL,
                                    NULL,
                                    "VideoVertexShader",
                                    "vs_2_0",
                                    0,
                                    &pVertexShaderCode,
                                    &pVertexErrorMsg,
                                    NULL );
    if( FAILED( hr ) )
    {
        if( pVertexErrorMsg )
            ATG::DebugSpew( ( char* )pVertexErrorMsg->GetBufferPointer() );
        return E_FAIL;
    }

    // Create vertex shader.
    pd3dDevice->CreateVertexShader( ( DWORD* )pVertexShaderCode->GetBufferPointer(),
                                    &m_pVideoVertexShader );

    // Compile pixel shader.
    ID3DXBuffer* pPixelShaderCode;
    ID3DXBuffer* pPixelErrorMsg;

    hr = D3DXCompileShader( g_strVideoShaderHLSL,
                            ( UINT )strlen( g_strVideoShaderHLSL ),
                            NULL,
                            NULL,
                            "VideoPixelShader",
                            "ps_2_0",
                            0,
                            &pPixelShaderCode,
                            &pPixelErrorMsg,
                            NULL );
    if( FAILED( hr ) )
    {
        if( pPixelErrorMsg )
            ATG::DebugSpew( ( char* )pPixelErrorMsg->GetBufferPointer() );
        return E_FAIL;
    }

    // Create pixel shader.
    pd3dDevice->CreatePixelShader( ( DWORD* )pPixelShaderCode->GetBufferPointer(),
                                   &m_pVideoPixelShaderRGB );

    // Define the vertex elements and
    // Create a vertex declaration from the element descriptions.
    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pVideoVertexDecl );

    // Initialize color stream video texture
    if( FAILED( InitializeVideoTextures( pd3dDevice, &m_pColorTexture, dwColorStreamWidth, dwColorStreamHeight, ATG::GetAs16SRGBFormat( D3DFMT_LIN_X8R8G8B8 ) ) ) )
        return E_FAIL;

    // Initialize depth stream video texture
    if( FAILED( InitializeVideoTextures( pd3dDevice, &m_pDepthTexture, dwDepthStreamWidth, dwDepthStreamHeight, D3DFMT_LIN_X8R8G8B8 ) ) )
        return E_FAIL;

    return S_OK;
}

VOID UpdateDepthTexture( D3DDevice* pd3dDevice, const NUI_IMAGE_FRAME* pDepthMap )
{
    PIXBeginNamedEvent( 0, "VisualizeStreams - Fill DepthMap" );
    if( pDepthMap )
    {
        D3DLOCKED_RECT Locked;
        if( FAILED( m_pDepthTexture->LockRect( 0, &Locked, NULL, 0 ) ) )
        {
            ATG::DebugSpew( "m_pDepthTexture->LockRect failed\n" );
        }

        D3DLOCKED_RECT LockedSrc;
        if( FAILED( pDepthMap->pFrameTexture->LockRect( 0, &LockedSrc, NULL, D3DLOCK_READONLY ) ) )
        {
            m_pDepthTexture->UnlockRect( 0 );
            ATG::DebugSpew( "pDepthMap->pFrameTexture->LockRect failed\n" );
        }

        // Fill in the depthmap data
        DWORD* lpBits = ( DWORD* )Locked.pBits;
        USHORT* pDepthMapData = (USHORT*)LockedSrc.pBits;
        
        for( UINT y = 0; y < g_dwDepthStreamHeight; ++ y )
        {
            for( UINT x = 0; x < g_dwDepthStreamWidth; ++ x )
            {
                // To colorize the depth values, normalize the depth value against a maximum depth
                // value to be colorized and lookup into the color table
                const static FLOAT fMaxDepth = 3500.0f;                           // use 3.5 meters as max depth to colorize
                const static UINT uNormalize = (UINT) ceil( fMaxDepth / 511.0 );
                UINT uIndex = min( (USHORT)(pDepthMapData[x] >> 3) / uNormalize, 511 );

                lpBits[x] = g_DepthColorTable[ uIndex ];
            }
            lpBits += Locked.Pitch / sizeof(DWORD);
            pDepthMapData += LockedSrc.Pitch / sizeof(USHORT);
        }

        pDepthMap->pFrameTexture->UnlockRect( 0 );
        m_pDepthTexture->UnlockRect( 0 );
    }
    PIXEndNamedEvent();
}

VOID UpdateColorTexture( D3DDevice* pd3dDevice, const NUI_IMAGE_FRAME* pColorMap )
{
    PIXBeginNamedEvent( 0, "Fill ColorMap" );
    if( pColorMap )
    {
        // Clear texture
        D3DLOCKED_RECT Locked;
        if( FAILED( m_pColorTexture->LockRect( 0, &Locked, NULL, 0 ) ) )
        {
            ATG::DebugSpew( "pColorTexture->LockRect failed\n" );
        }
        D3DLOCKED_RECT LockedSrc;
        if( FAILED( pColorMap->pFrameTexture->LockRect( 0, &LockedSrc, NULL, D3DLOCK_READONLY ) ) )
        {
            ATG::DebugSpew( "pColorMap->pFrameTexture->LockRect failed\n" );
        }

        memcpy( Locked.pBits, LockedSrc.pBits, Locked.Pitch * g_dwColorStreamHeight );

        m_pColorTexture->UnlockRect( 0 );
        pColorMap->pFrameTexture->UnlockRect( 0 );
    }
    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: SubmitVertexData()
// Desc: Create the vertices used to render the video on screen, and feed them inline
// to the D3D command buffer.
//--------------------------------------------------------------------------------------
VOID SubmitVertexData( D3DDevice* pd3dDevice, BOOL bColor )
{
    UINT uWidth;
    UINT uHeight;
    ATG::GetVideoSettings( &uWidth, &uHeight );

    XVIDEO_MODE VideoMode;
    ZeroMemory( &VideoMode, sizeof( VideoMode ) );
    XGetVideoMode( &VideoMode );

    if( bColor )
    {
        ColorWindow.fWidth = uWidth / 2.f * 0.8f;
        ColorWindow.fHeight = uWidth * 3.f / 4.f / 2.f * 0.8f;

        if( ( VideoMode.fIsWideScreen ) && ( uWidth == 640.f ) )
            ColorWindow.fWidth /= 1.333f;

        ColorWindow.fX = uWidth / 2.f - ColorWindow.fWidth - 10.f;
        ColorWindow.fY = ( uHeight - ColorWindow.fHeight ) / 2.f - 30.f;

        // Now we fill the vertex buffer. 
        VideoFeedVertex g_SnapshotVertices[] =
        {
            { ColorWindow.fX,                                                         
            ColorWindow.fY, 0,  0, 0 },
            { ColorWindow.fX + ColorWindow.fWidth,               
            ColorWindow.fY, 0,  1, 0 },
            { ColorWindow.fX,                                                         
            ColorWindow.fY + ColorWindow.fHeight, 0,  0, 1 },
            //{ ColorWindow.fX + ColorWindow.fWidth, 
            //  ColorWindow.fY + ColorWindow.fHeight, 0,  1, 1 },
        };

        VideoFeedVertex* pVertices;

        pd3dDevice->BeginVertices( D3DPT_RECTLIST, 3, sizeof( *g_SnapshotVertices ), &(VOID*&)pVertices );
        memcpy( pVertices, g_SnapshotVertices, sizeof( g_SnapshotVertices ) );
        pd3dDevice->EndVertices();
    }
    else    // depth
    {
        DepthWindow.fWidth = uWidth / 2.f * 0.8f;
        DepthWindow.fHeight = uWidth * 3.f / 4.f / 2.f * 0.8f;

        if( ( VideoMode.fIsWideScreen ) && ( uWidth == 640.f ) )
            DepthWindow.fWidth /= 1.333f;

        DepthWindow.fX = uWidth / 2.f + 10.f;
        DepthWindow.fY = ColorWindow.fY;

        // Fill in the VB for depth stream texture
        VideoFeedVertex g_SnapshotVertices[] =
        {
            { DepthWindow.fX,                                          
            DepthWindow.fY, 0,  0, 0 },
            { DepthWindow.fX + DepthWindow.fWidth, 
            DepthWindow.fY, 0,  1, 0 },
            { DepthWindow.fX,                                          
            DepthWindow.fY + DepthWindow.fHeight, 0,  0, 1 },
            //{ DepthWindow.fX + DepthWindow.fWidth, 
            //  DepthWindow.fY + DepthWindow.fHeight, 0,  1, 1 },
        };

        VideoFeedVertex* pVertices;

        pd3dDevice->BeginVertices( D3DPT_RECTLIST, 3, sizeof( *g_SnapshotVertices ), &(VOID*&)pVertices );
        memcpy( pVertices, g_SnapshotVertices, sizeof( g_SnapshotVertices ) );
        pd3dDevice->EndVertices();
    }
}


//--------------------------------------------------------------------------------------
// Name: VisualizeStreams()
// Desc: Output the color and depth streams
//--------------------------------------------------------------------------------------
VOID VisualizeStreams( D3DDevice* pd3dDevice )
{


    // Render video texture
    PIXBeginNamedEvent( 0, "VisualizeStreams - Render ColorMap" );
    pd3dDevice->SetPixelShader( m_pVideoPixelShaderRGB );

    pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    pd3dDevice->SetVertexShader( m_pVideoVertexShader );
    pd3dDevice->SetVertexDeclaration( m_pVideoVertexDecl );

    pd3dDevice->SetTexture( 0, m_pColorTexture );

    SubmitVertexData( pd3dDevice, TRUE );

    PIXEndNamedEvent();




    PIXBeginNamedEvent( 0, "VisualizeStreams - Render DepthMap" );

    pd3dDevice->SetPixelShader( m_pVideoPixelShaderRGB );

    pd3dDevice->SetTexture( 0, m_pDepthTexture );

    SubmitVertexData( pd3dDevice, FALSE );

    PIXEndNamedEvent();
}


#define PI 3.14159265359f
#define DEGREES_TO_RADIANS(x) ( ( x ) * ( PI / 180.0f ) )
extern BOOL g_bHeadTracking;

//--------------------------------------------------------------------------------------
// Name: VisualizeSkeleton()
// Desc: Visualize results of skeleton tracking by drawing the bones of the tracked
//       skeleton on top of the color and depth maps
//--------------------------------------------------------------------------------------
VOID VisualizeSkeleton(const NUI_SKELETON_FRAME * const pSkeletonFrame )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );



    float fHalfWindowWidth = DepthWindow.fWidth * 0.5f ;
    float fHalfWindowHeight = DepthWindow.fHeight * 0.5f;    
    float fColorWindowRatioX = ( ColorWindow.fWidth / 640.0f) ;
    float fColorWindowRatioY = ( ColorWindow.fHeight / 480.0f) ;
    float fDepthWindowRatioX = ( DepthWindow.fWidth / 320.0f) ;
    float fDepthWindowRatioY = ( DepthWindow.fHeight / 240.0f) ;

    // Project each of the tracked skeletons.
    for ( UINT j = 0; j < NUI_SKELETON_COUNT; ++j)
    {
        if (pSkeletonFrame->SkeletonData[j].eTrackingState != NUI_SKELETON_TRACKED)
            continue;
        
        // Project the world space joints into screen space
        for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
        {
            XMFLOAT3 vJointLocation;
            XMStoreFloat3( &vJointLocation, pSkeletonFrame->SkeletonData[ j ].SkeletonPositions[ i ] );

            // Check for divide by zero
            if ( fabs( vJointLocation.z ) > FLT_EPSILON  )
            {
                // Note:  Without tilt correction, any projection will be off, as the skeleton positions 
                //        are camera-relative, with Up as ( 0, 1, 0), which the axis of the camera may
                //        not be aligned to. You can see this by turning on and off tilt correction
                //        in the sample.

                LONG plDepthX, plDepthY, plColorX, plColorY;
                USHORT usDepthValue;
                NuiTransformSkeletonToDepthImage( pSkeletonFrame->SkeletonData[ j ].SkeletonPositions[ i ],
                    &plDepthX, &plDepthY, &usDepthValue );
                

                HRESULT hr = NuiImageGetColorPixelCoordinatesFromDepthPixel(
                    NUI_IMAGE_RESOLUTION_640x480,
                    NULL,
                    plDepthX,
                    plDepthY,
                    usDepthValue,
                    &plColorX,
                    &plColorY);

                if ( SUCCEEDED( hr ) )
                {
                    g_ScreenSpaceJointsColorMap[i].x = 
                        ( (FLOAT) plColorX ) * fColorWindowRatioX;
                    g_ScreenSpaceJointsColorMap[i].y = 
                        ( (FLOAT) plColorY ) * fColorWindowRatioY;
                }
                g_ScreenSpaceJoints[i].x = 
                    ( (FLOAT) plDepthX ) * fDepthWindowRatioX;
                g_ScreenSpaceJoints[i].y = 
                    ( (FLOAT) plDepthY ) * fDepthWindowRatioY;

            }
            else
            {
                // A joint that is so close to the camera that its Z value is 0 can simply be drawn directly 
                // at the center of the 2D plane.
                g_ScreenSpaceJoints[ i ].x = fHalfWindowWidth;
                g_ScreenSpaceJoints[ i ].y = fHalfWindowHeight;
                g_ScreenSpaceJointsColorMap[ i ].x = fHalfWindowWidth;
                g_ScreenSpaceJointsColorMap[ i ].y = fHalfWindowHeight;

            }
        }

        NUI_SKELETON_POSITION_TRACKING_STATE const * pSkeletonTrackingState = &pSkeletonFrame->SkeletonData[ j ].eSkeletonPositionTrackingState[ 0 ];
        // Draw each bone in the skeleton using the screen space joints
        for( UINT i = 0; i < g_uNumBones; i++ )
        {
            // Asign a color to each joint based on the confidence level. Don't draw a bone if one of its joints has no confidence.


            D3DCOLOR startColor;
            if( pSkeletonTrackingState[ g_Bones[ i ].StartJoint ] == NUI_SKELETON_POSITION_TRACKED )
            {
                startColor = BONE_CONFIDENCE_HIGH_COLOR;
            }
            else if( pSkeletonTrackingState[ g_Bones[ i ].StartJoint ] == NUI_SKELETON_POSITION_INFERRED )
            {
                startColor = BONE_CONFIDENCE_LOW_COLOR;
            }
            else
            {
                // A joint in the bone wasn't tracked during skeleton tracking...
                continue;
            }

            D3DCOLOR endColor;
            if( pSkeletonTrackingState[ g_Bones[ i ].EndJoint ] == NUI_SKELETON_POSITION_TRACKED )
            {
                endColor = BONE_CONFIDENCE_HIGH_COLOR;
            }
            else if( pSkeletonTrackingState[ g_Bones[ i ].EndJoint ] == NUI_SKELETON_POSITION_INFERRED )
            {
                endColor = BONE_CONFIDENCE_LOW_COLOR;
            }
            else
            {
                // A joint in the bone wasn't tracked during skeleton tracking...
                continue;
            }

            // Draw the bone over the depth image
            XMFLOAT2 pntArray[ 2 ];
            pntArray[ 0 ].x = g_ScreenSpaceJoints[ g_Bones[ i ].StartJoint ].x + DepthWindow.fX;
            pntArray[ 0 ].y = g_ScreenSpaceJoints[ g_Bones[ i ].StartJoint ].y + DepthWindow.fY;
            pntArray[ 1 ].x = g_ScreenSpaceJoints[ g_Bones[ i ].EndJoint ].x + DepthWindow.fX;
            pntArray[ 1 ].y = g_ScreenSpaceJoints[ g_Bones[ i ].EndJoint ].y + DepthWindow.fY;
            ATG::DebugDraw::DrawScreenSpaceLine( pntArray[ 0 ], startColor, pntArray[ 1 ], endColor, 3 );


            // Draw the bone over the color image. Assuming the color and depth displays have the 
            // same size and the same x origin, we simply translated the projected skeleton horizontally 
            // over the color image window.
            if ( !g_bHeadTracking )
            {
                assert( ColorWindow.fY == DepthWindow.fY ); 
                assert( ColorWindow.fWidth == DepthWindow.fWidth ); 
                assert( ColorWindow.fHeight == DepthWindow.fHeight ); 

                pntArray[ 0 ].x = g_ScreenSpaceJointsColorMap[ g_Bones[ i ].StartJoint ].x + ColorWindow.fX;
                pntArray[ 0 ].y = g_ScreenSpaceJointsColorMap[ g_Bones[ i ].StartJoint ].y + ColorWindow.fY;
                pntArray[ 1 ].x = g_ScreenSpaceJointsColorMap[ g_Bones[ i ].EndJoint ].x + ColorWindow.fX;
                pntArray[ 1 ].y = g_ScreenSpaceJointsColorMap[ g_Bones[ i ].EndJoint ].y + ColorWindow.fY;

                ATG::DebugDraw::DrawScreenSpaceLine( pntArray[ 0 ], startColor, pntArray[ 1 ], endColor, 3 );
            }            
        }

    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: InitializeDepthColorTable()
// Desc: Initialize the lookup table to display depth values in color
//--------------------------------------------------------------------------------------
VOID InitializeDepthColorTable()
{
    // Build depth map visualization color table. Rainbow linear gradient mirrored at
    // center point with gradual dimming starting at center point to start of table.
    
    const INT iHalfTableSize    = ARRAYSIZE( g_DepthColorTable ) / 2;
    FLOAT fGutter               = 0.2f;   
    INT iTableIndex             = iHalfTableSize;
    FLOAT fStep                 = ( 1.0f - ( fGutter * 2.0f ) ) / iHalfTableSize;

    for( FLOAT t = fGutter; t < ( 1.0f - fGutter ); t += fStep )
    {
        FLOAT fColor[ 3 ]       = { 0, 0, 0 };
        FLOAT fBand             = 0.7f;
        const FLOAT fCurveExp   = 2.0f;   
        const FLOAT fBandGap    = 1.0f - fBand;
         
        for( INT i = 0; i < 3; i++ )  
        {
            FLOAT s = ( t - fBandGap * 0.5f * i ) / fBand;
            if ( ( s >= 0 ) && ( s <= 1 ) )
            {
                fColor[ i ] = powf( sinf( s * XM_PI * 2.0f - XM_PI * 0.5f ) * 0.5f + 0.5f, fCurveExp );
            }
        }

        g_DepthColorTable[ iTableIndex++ ] = D3DCOLOR_RGBA( (BYTE)( fColor[ 0 ] * 255.0f ),
                                                            (BYTE)( fColor[ 1 ] * 255.0f ),
                                                            (BYTE)( fColor[ 2 ] * 255.0f ),
                                                            0xff );
    }
    
    for( INT i = 0; i < iHalfTableSize; i++ )
    {
        COLORREF s = g_DepthColorTable[ ARRAYSIZE( g_DepthColorTable ) - 1 - i ];
        
        FLOAT fDim = ( FLOAT )i / (FLOAT)iHalfTableSize;
        
        g_DepthColorTable[ i ] = D3DCOLOR_RGBA( ( BYTE )( (FLOAT)D3DCOLOR_GETRED( s ) * ( 0.25f + ( fDim * 0.75f ) ) ),
                                                ( BYTE )( (FLOAT)D3DCOLOR_GETGREEN( s ) * ( 0.25f + ( fDim * 0.75f ) ) ),
                                                ( BYTE )( (FLOAT)D3DCOLOR_GETBLUE( s ) * ( 0.25f + ( fDim * 0.75f ) ) ),
                                                 0xff );
    }
}


//--------------------------------------------------------------------------------------
// Name: InitializeVideoTextures()
// Desc: Recreates the m_pVideoTextures array of D3D textures using the current
//       resolution and pixel format settings.
//--------------------------------------------------------------------------------------
HRESULT InitializeVideoTextures( D3DDevice* pd3dDevice, 
                                 IDirect3DTexture9** ppVideoTexture, 
                                 DWORD dwWidth, DWORD dwHeight, D3DFORMAT format )
{
    HRESULT hr;

    // Release the old texture
    if( *ppVideoTexture != NULL )
    {
        (*ppVideoTexture)->BlockUntilNotBusy();
        (*ppVideoTexture)->Release();
        *ppVideoTexture = NULL;
    }

       
    // Create the new texture using the current resolution and pixel format
    hr = pd3dDevice->CreateTexture( dwWidth, dwHeight, 1, 0,
                                    format, D3DPOOL_MANAGED, ppVideoTexture, NULL );

    if( FAILED( hr ) )
    {
        ATG::DebugSpew( "Failed to create texture!\n" );
        return E_FAIL;
    }

    // Clear texture
    D3DLOCKED_RECT Locked;
    if( FAILED( (*ppVideoTexture)->LockRect( 0, &Locked, NULL, 0 ) ) )
        return E_FAIL;

    // Fill the texture with black
    DWORD* lpBits = ( DWORD* )Locked.pBits;
    for( UINT k = 0; k < Locked.Pitch * dwHeight / 4; ++ k ) 
        *lpBits++ = 0xff000000;

    (*ppVideoTexture)->UnlockRect( 0 );

    return S_OK;
}