//----------------------------------------------------------------------------------------------------------------------
// Visualization.cpp
// 
// Visualizations for skeletons, depth map and audio data for the sample.
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (c) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#include <xtl.h>
#include <d3d9types.h>
#include <nuiapi.h>
#include <NuiAudio.h>
#include <AtgUtil.h>
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
} Rect;


struct VideoFeedVertex
{
    FLOAT vPosition[ 3 ];
    FLOAT vTexCoords[ 2 ];
};


//--------------------------------------------------------------------------------------
// Constants and defines
//--------------------------------------------------------------------------------------


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

// Player index color table containing selected and non-selected players
static const DWORD g_PlayerColorTable[] =
{
    0xff000000,
    0xff4f0000,
    0xff004f00,
    0xff00004f,
    0xff4f4f00,
    0xff4f004f,
    0xff004f4f,
    0xff4f4f4f,
    0xff000000,
    0xffff0000,
    0xff00ff00,
    0xff0000ff,
    0xffffff00,
    0xffff00ff,
    0xff00ffff,
    0xffffffff,
};

IDirect3DVertexDeclaration9* g_pVideoVertexDecl;     // Vertex format decl
IDirect3DVertexShader9*      g_pVideoVertexShader;   // Vertex Shader
IDirect3DPixelShader9*       g_pVideoPixelShaderRGB; // Pixel Shader for RGB image

IDirect3DTexture9* m_pDepthTexture = NULL;

Rect g_DepthWindow;   // Screen output area for the color feed

// Each stream can have different dimensions. To display it correctly we need to
// know the dimensions of each stream seperately. 
DWORD   g_dwDepthStreamWidth    = 0;
DWORD   g_dwDepthStreamHeight   = 0;

// The screen space joint data will be stored in this array.
XMFLOAT2 g_ScreenSpaceJoints[ NUI_SKELETON_POSITION_COUNT ];

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
HRESULT InitializeVideoTexture( D3DDevice* pd3dDevice, 
                                 IDirect3DTexture9** ppVideoTexture, 
                                 DWORD dwWidth, DWORD dwHeight, D3DFORMAT format );


//--------------------------------------------------------------------------------------
// Name: InitializeVisualization()
// Desc: Creates visualization buffers and shaders
//--------------------------------------------------------------------------------------
HRESULT InitializeVisualization( D3DDevice* pd3dDevice,
                                 DWORD dwDepthStreamWidth, DWORD dwDepthStreamHeight )
{
    // Set the dimension of the stream since we will be using them during visualization
    g_dwDepthStreamWidth    = dwDepthStreamWidth;
    g_dwDepthStreamHeight   = dwDepthStreamHeight;

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
        {
            ATG::DebugSpew( ( char* )pVertexErrorMsg->GetBufferPointer() );
        }

        return E_FAIL;
    }

    // Create vertex shader.
    hr = pd3dDevice->CreateVertexShader( ( DWORD* )pVertexShaderCode->GetBufferPointer(),
                                         &g_pVideoVertexShader );

    if ( FAILED( hr ) )
    {
        ATG::DebugSpew( "Failed to create vertex shader" );
        return E_FAIL;
    }

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
        {
            ATG::DebugSpew( ( char* )pPixelErrorMsg->GetBufferPointer() );
        }
        return E_FAIL;
    }

    // Create pixel shader.
    hr = pd3dDevice->CreatePixelShader( ( DWORD* )pPixelShaderCode->GetBufferPointer(),
                                   &g_pVideoPixelShaderRGB );

    if ( FAILED( hr ) )
    {
        ATG::DebugSpew( "Failed to create pixel shader" );
        return E_FAIL;
    }

    // Define the vertex elements and
    // Create a vertex declaration from the element descriptions.
    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    
    hr = pd3dDevice->CreateVertexDeclaration( VertexElements, &g_pVideoVertexDecl );
    
    if ( FAILED( hr ) )
    {
        ATG::DebugSpew( "Failed to create vertex declaration" );
        return E_FAIL;
    }

    // Initialize depth stream video texture
    if( FAILED( InitializeVideoTexture( pd3dDevice, &m_pDepthTexture, dwDepthStreamWidth, dwDepthStreamHeight, D3DFMT_LIN_X8R8G8B8 ) ) )
    {
        return E_FAIL;
    }

    return S_OK;
}

VOID UpdateDepthTexture( D3DDevice* pd3dDevice, const NUI_IMAGE_FRAME* pDepthMap, DWORD dwActivePlayer )
{
    if( pDepthMap )
    {
        PIXBeginNamedEvent( 0, "VisualizeStreams - Fill DepthMap" );

        pd3dDevice->SetTexture( 0, NULL );

        D3DLOCKED_RECT Locked;
        m_pDepthTexture->LockRect( 0, &Locked, NULL, 0 );
        
        D3DLOCKED_RECT LockedSrc;
        pDepthMap->pFrameTexture->LockRect( 0, &LockedSrc, NULL, D3DLOCK_READONLY );
        
        // Fill in the depthmap data
        DWORD* lpBits = ( DWORD* )Locked.pBits;
        USHORT* pDepthMapData = (USHORT*)LockedSrc.pBits;
        
        for( UINT y = 0; y < g_dwDepthStreamHeight; ++ y )
        {
            for( UINT x = 0; x < g_dwDepthStreamWidth; ++ x )
            {
                DWORD dwPlayerIndex = pDepthMapData[x] & 7;
                if( dwActivePlayer == dwPlayerIndex ) dwPlayerIndex += 8;

                assert( dwPlayerIndex < ARRAY_SIZE( g_PlayerColorTable ) );

                lpBits[x] = g_PlayerColorTable[dwPlayerIndex];
            }

            lpBits += Locked.Pitch / sizeof(DWORD);
            pDepthMapData += LockedSrc.Pitch / sizeof(USHORT);
        }

        pDepthMap->pFrameTexture->UnlockRect( 0 );
        
        m_pDepthTexture->UnlockRect( 0 );

        PIXEndNamedEvent();
    }
}


//--------------------------------------------------------------------------------------
// Name: SubmitVertexData()
// Desc: Create the vertices used to render the video on screen, and feed them inline
// to the D3D command buffer.
//--------------------------------------------------------------------------------------
VOID SubmitVertexData( D3DDevice* pd3dDevice )
{
    UINT uWidth;
    UINT uHeight;
    ATG::GetVideoSettings( &uWidth, &uHeight );

    XVIDEO_MODE VideoMode;
    ZeroMemory( &VideoMode, sizeof( VideoMode ) );
    XGetVideoMode( &VideoMode );

    g_DepthWindow.fHeight = (FLOAT)uHeight;
    g_DepthWindow.fWidth = uHeight * 4.0f / 3.0f;

    if( ( VideoMode.fIsWideScreen ) && ( uWidth == 640.f ) )
        g_DepthWindow.fWidth /= 1.333f;

    g_DepthWindow.fX = ( uWidth / 2.0f ) - ( g_DepthWindow.fWidth / 2.0f );
    g_DepthWindow.fY = 0;

    // Fill in the VB for depth stream texture
    VideoFeedVertex g_SnapshotVertices[] =
    {
        { g_DepthWindow.fX,                                          
        g_DepthWindow.fY, 0,  0, 0 },
        { g_DepthWindow.fX + g_DepthWindow.fWidth, 
        g_DepthWindow.fY, 0,  1, 0 },
        { g_DepthWindow.fX,                                          
        g_DepthWindow.fY + g_DepthWindow.fHeight, 0,  0, 1 },
    };

    VideoFeedVertex* pVertices;

    pd3dDevice->BeginVertices( D3DPT_RECTLIST, 3, sizeof( *g_SnapshotVertices ), &(VOID*&)pVertices );
    memcpy( pVertices, g_SnapshotVertices, sizeof( g_SnapshotVertices ) );
    pd3dDevice->EndVertices();
}


//--------------------------------------------------------------------------------------
// Name: VisualizeStreams()
// Desc: Output the color and depth streams
//--------------------------------------------------------------------------------------
VOID VisualizeStreams( D3DDevice* pd3dDevice )
{
    PIXBeginNamedEvent( 0, "VisualizeStreams - Render DepthMap" );

    pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );

    pd3dDevice->SetVertexShader( g_pVideoVertexShader );
    pd3dDevice->SetVertexDeclaration( g_pVideoVertexDecl );
    pd3dDevice->SetPixelShader( g_pVideoPixelShaderRGB );

    pd3dDevice->SetTexture( 0, m_pDepthTexture );

    SubmitVertexData( pd3dDevice );

    pd3dDevice->DrawPrimitive( D3DPT_RECTLIST, 0, 2 );

    pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );   // Restore default value.

    PIXEndNamedEvent();
}


#define DEGREES_TO_RADIANS(x) ( ( x ) * ( PI / 180.0f ) )

//--------------------------------------------------------------------------------------
// Name: VisualizeSkeleton()
// Desc: Visualize results of skeleton tracking by drawing the bones of the tracked
//       skeleton on top of the color and depth maps
//--------------------------------------------------------------------------------------
VOID VisualizeSkeleton(const NUI_SKELETON_FRAME * const pSkeletonFrame )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    float fHalfWindowWidth = g_DepthWindow.fWidth * 0.5f ;
    float fHalfWindowHeight = g_DepthWindow.fHeight * 0.5f;    
    float fDepthWindowRatioX = ( g_DepthWindow.fWidth / 320.0f) ;
    float fDepthWindowRatioY = ( g_DepthWindow.fHeight / 240.0f) ;

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
            const FLOAT EPSILON = 1.192092896e-07F;
            if ( fabs( vJointLocation.z ) > EPSILON )
            {
                // Note:  Without tilt correction, any projection will be off, as the skeleton positions 
                //        are camera-relative, with Up as ( 0, 1, 0), which the axis of the camera may
                //        not be aligned to. You can see this by turning on and off tilt correction
                //        in the sample.

                LONG plDepthX, plDepthY;
                USHORT usDepthValue;
                NuiTransformSkeletonToDepthImage( pSkeletonFrame->SkeletonData[ j ].SkeletonPositions[ i ],
                    &plDepthX, &plDepthY, &usDepthValue );
                
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
            pntArray[ 0 ].x = g_ScreenSpaceJoints[ g_Bones[ i ].StartJoint ].x + g_DepthWindow.fX;
            pntArray[ 0 ].y = g_ScreenSpaceJoints[ g_Bones[ i ].StartJoint ].y + g_DepthWindow.fY;
            pntArray[ 1 ].x = g_ScreenSpaceJoints[ g_Bones[ i ].EndJoint ].x + g_DepthWindow.fX;
            pntArray[ 1 ].y = g_ScreenSpaceJoints[ g_Bones[ i ].EndJoint ].y + g_DepthWindow.fY;
            ATG::DebugDraw::DrawScreenSpaceLine( pntArray[ 0 ], startColor, pntArray[ 1 ], endColor, 3 );
        }

    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: VisualizeTalkerPosition()
// Desc: Draw a vertical wedge covering the confidence interval of the speaker angle.
//--------------------------------------------------------------------------------------
VOID VisualizeTalkerPosition( D3DDevice* pd3dDevice, FLOAT fBeamDirection, FLOAT fConfidence )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    XMVECTOR vEyePt = XMVectorZero();
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

    XMMATRIX matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );

    FLOAT fNear = 1.0f;
    FLOAT fFar = 200.0f;

    XMMATRIX matProjection = XMMatrixPerspectiveFovLH( XMConvertToRadians( NUI_CAMERA_DEPTH_NOMINAL_HORIZONTAL_FOV ), 4.0f / 3.0f, fNear, fFar );

    ATG::DebugDraw::SetViewProjection( matView * matProjection );

    XMMATRIX matDirection = XMMatrixRotationY( fBeamDirection );
    XMVECTOR quatOrientation = XMQuaternionRotationMatrix( matDirection );

    // This frustum will just look like a quad from the standard view
    ATG::Frustum Frustum;
    Frustum.Origin = XMFLOAT3( 0.0f, 0.0f, 0.0f );
    Frustum.Orientation = quatOrientation.v;
    Frustum.RightSlope = tanf( fConfidence / 2.0f );
    Frustum.LeftSlope = -Frustum.RightSlope;
    Frustum.TopSlope = tanf( XMConvertToRadians( NUI_CAMERA_DEPTH_NOMINAL_VERTICAL_FOV ) / 2.0f );
    Frustum.BottomSlope = -Frustum.TopSlope;
    Frustum.Near = fNear;
    Frustum.Far = fFar;

    D3DCOLOR Color = D3DCOLOR_ARGB( 0xff, 0x80, 0x00, 0x80 );
    ATG::DebugDraw::DrawFrustum( Frustum, Color );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: InitializeVideoTexture()
// Desc: Recreates the video texture to match the resolution and pixel format settings.
//--------------------------------------------------------------------------------------
HRESULT InitializeVideoTexture( D3DDevice* pd3dDevice, 
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
                                    format, 0, ppVideoTexture, NULL );

    if( FAILED( hr ) )
    {
        ATG::DebugSpew( "Failed to create texture!\n" );
        return E_FAIL;
    }

    // Clear texture
    D3DLOCKED_RECT Locked;
    (*ppVideoTexture)->LockRect( 0, &Locked, NULL, 0 );

    // Fill the texture with black
    DWORD* lpBits = ( DWORD* )Locked.pBits;
    for( UINT k = 0; k < Locked.Pitch * dwHeight / 4; ++ k ) 
        *lpBits++ = 0xff000000;

    (*ppVideoTexture)->UnlockRect( 0 );

    return S_OK;
}



//----------------------------------------------------------------------------------------------------------------------
// Name: VisualizeAudioData
// Desc: Renders an audio packet to the display. 
//----------------------------------------------------------------------------------------------------------------------
VOID VisualizeAudioData( const SHORT* pAudioBuffer, FLOAT fX, FLOAT fY, FLOAT fWidth, FLOAT fHeight )
{
    const INT NUM_AUDIO_SAMPLES = NUIAUDIO_MAX_DURATION;
    const FLOAT TWOS_COMPLEMENT_NEGATIVE_RANGE_F = 32768.0f;
    const D3DCOLOR LINE_COLOR = 0xFFFF0000;

    PIXBeginNamedEvent( 0, "VisualizeAudioWaveform" );

    const FLOAT RecipYRange = 1.0f / (FLOAT)USHRT_MAX;
    const FLOAT RecipXRange = 1.0f / (FLOAT)(NUM_AUDIO_SAMPLES - 1);
    

    XMFLOAT2 data[256];

    for ( int i = 0; i < NUM_AUDIO_SAMPLES; ++i )
    {
        FLOAT fAudioData = (FLOAT)pAudioBuffer[i];
        FLOAT fIndex = ((FLOAT) i );

        // Normalize the audio data from 16-bit signed integer to float in range 0.0 to 1.0
        FLOAT fNormAudioData = (fAudioData + TWOS_COMPLEMENT_NEGATIVE_RANGE_F ) * RecipYRange ;

        FLOAT fScreenSpaceAudioY = ( (1.0f - fNormAudioData) * fHeight ) + fY;
        FLOAT fScreenSpaceAudioX = ( fIndex * RecipXRange * fWidth ) + fX;
        
        data[i].x = fScreenSpaceAudioX;
        data[i].y = fScreenSpaceAudioY;
    }

    ATG::DebugDraw::DrawScreenSpaceLineList( data, NUM_AUDIO_SAMPLES, LINE_COLOR );

    PIXEndNamedEvent();
}

VOID VisualizeAudioBand( const FLOAT* pBandBuffer, FLOAT fX, FLOAT fY, FLOAT fWidth, FLOAT fHeight )
{
    // Note: Buckets to render = number of samples / 2, because only the first half of the values are useful after an FFT
    const INT NUM_FFT_BUCKETS = NUIAUDIO_MAX_DURATION / 2;
    const D3DCOLOR FFT_LINE_COLOR = 0xFFFFFF00;

    PIXBeginNamedEvent( 0, "VisualizeAudioBand" );

    const FLOAT RecipYRange = 1.0f / 256.0f;    // Scale FFT output for display. Use larger numbers for divisor to take
                                                // up less Y-axis space when rendered.
    const FLOAT RecipXRange = 1.0f / (FLOAT)(NUM_FFT_BUCKETS - 1);

    XMFLOAT2 data[256];

    for ( int i = 0; i < NUM_FFT_BUCKETS; ++i )
    {
        FLOAT fBandData = (FLOAT)pBandBuffer[i];
        FLOAT fIndex = ((FLOAT) i );
        FLOAT fNormAudioData = (fBandData) * RecipYRange ;
        FLOAT fScreenSpaceAudioY = ( (1.0f - fNormAudioData) * fHeight ) + fY;
        FLOAT fScreenSpaceAudioX = ( fIndex * RecipXRange * fWidth ) + fX;
        
        data[i].x = fScreenSpaceAudioX;
        data[i].y = fScreenSpaceAudioY;
    }

    ATG::DebugDraw::DrawScreenSpaceLineList( data, NUM_FFT_BUCKETS, FFT_LINE_COLOR );

    PIXEndNamedEvent();
}

VOID VisualizeTopView( FLOAT fBeamAngle, FLOAT fX, FLOAT fY, FLOAT fWidth, FLOAT fHeight, BOOL bTracked )
{
	const D3DCOLOR FOV_COLOR = 0xFFC0C0C0;
	D3DCOLOR BEAM_DIRECTION_COLOR = 0xFFC000C0;

    PIXBeginNamedEvent( 0, "VisualizeTopView" );

    // Render the outline of the sensor field of view.

	XMFLOAT2 data[3];
	data[0].x = fX + fWidth * 0.5f + sinf( XMConvertToRadians( NUI_CAMERA_DEPTH_NOMINAL_HORIZONTAL_FOV * -0.5f ) ) * fWidth;
	data[0].y = fY + cosf( XMConvertToRadians( NUI_CAMERA_DEPTH_NOMINAL_HORIZONTAL_FOV * -0.5f ) ) * fHeight;
	data[1].x = fX + fWidth * 0.5f;
	data[1].y = fY;
	data[2].x = fX + fWidth * 0.5f + sinf( XMConvertToRadians( NUI_CAMERA_DEPTH_NOMINAL_HORIZONTAL_FOV * 0.5f ) ) * fWidth;
	data[2].y = fY + cosf( XMConvertToRadians( NUI_CAMERA_DEPTH_NOMINAL_HORIZONTAL_FOV * 0.5f ) ) * fHeight;
    ATG::DebugDraw::DrawScreenSpaceLineList( data, 3, FOV_COLOR );

	if ( ! bTracked )
    {
		BEAM_DIRECTION_COLOR = 0xFF400040;
    }

    // Render the beam-former direction

	data[0].x = fX + fWidth * 0.5f;
	data[0].y = fY;
	data[1].x = data[0].x + sinf( fBeamAngle ) * fWidth;
	data[1].y = fY + cosf( fBeamAngle ) * fHeight;
	ATG::DebugDraw::DrawScreenSpaceLineList( data, 2, BEAM_DIRECTION_COLOR );

    PIXEndNamedEvent();
}
