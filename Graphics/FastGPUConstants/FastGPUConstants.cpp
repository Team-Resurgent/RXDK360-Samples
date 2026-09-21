//--------------------------------------------------------------------------------------
// FastGPUConstants.cpp
//
// This sample demonstrates the fast GPU constant loading APIs.  These APIs can bypass
// Direct3D's shadow state management and allow the GPU to consume constants directly
// from the title's memory.  Alternatively, other APIs can bypass parts of Direct3D's
// shadow state pipeline, reducing the amount of copies performed inside of Direct3D.
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <ppcintrinsics.h>


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_2, L"Cycle methods" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_1, L"Pause animation" },
    { ATG::HELP_LEFT_STICK, ATG::HELP_PLACEMENT_2, L"Move camera" },
    { ATG::HELP_MISC_CALLOUT, ATG::HELP_PLACEMENT_1, L"Triggers zoom camera in/out" },
};
static const DWORD  NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );

const FLOAT         CylinderHeight = 20.0f;
const FLOAT         CylinderRadius = 1.0f;
const UINT          CylinderVerticalDiv = 59;
const UINT          CylinderBoneCount = CylinderVerticalDiv + 1;
const UINT          CylinderHorizontalDiv = 10;
const XMVECTOR      CylinderBottomColor = {0.0f, 1.0f, 0.0f, 0.0f };
const XMVECTOR      CylinderTopColor = {1.0f, 0.0f, 0.0f, 0.0f };

const UINT          NumCylinders = 1000;
const FLOAT         FarmRadius = 75.0f;
const UINT          AnimLength = 4;
const UINT          AnimFreq = 30;
const UINT          NumAnimFrames = AnimLength * AnimFreq;

const UINT          NumCylinderWVPBuffers = 3;

enum Method
{
    BASIC = 0,
    GPU_INDIRECT_CONSTANT_LOAD,
    GPU_CONSTANT_LOAD,
    DIRECT_TO_SHADOW,
    DIRECT_TO_SHADOW_AND_COMMANDBUFFER,
    METHOD_MAX
};
const WCHAR*        MethodStrings[5] =
{
    L"Basic",
    L"GPU Indirect Constant Load",
    L"GPU Constant Load",
    L"Direct to D3D Shadow Copy",
    L"Direct to D3D Shadow Copy and Command Buffer"
};
C_ASSERT( METHOD_MAX == ARRAYSIZE( MethodStrings ) );

struct BoneMatrix
{
    XMFLOAT4 m[3];
};

struct CompressedBoneMatrix
{
    XMFLOAT3 Position;
    FLOAT Scale;
    XMFLOAT4 Rotation;
};

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

private:

    FLOAT m_fDeltaTime;
    FLOAT m_fAnimationTime;
    BOOL m_bAnimationPaused;

    XMMATRIX m_View;
    XMMATRIX m_Proj;

    XMMATRIX* m_pCylinderWorld;
    XMMATRIX* m_pCylinderWVP;
    DWORD           m_CylinderWVPFences[NumCylinderWVPBuffers];
    BoneMatrix* m_pCylinderAnim;
    CompressedBoneMatrix* m_pCylinderAnimCompressed;
    UINT            m_CylinderAnimFrameOffset[NumCylinders];


    D3DVertexShader* m_pCylinderVertexShader;
    D3DPixelShader* m_pCylinderPixelShader;
    D3DVertexDeclaration* m_pCylinderVertexDecl;

    D3DVertexBuffer* m_pCylinderVB;
    D3DIndexBuffer* m_pCylinderIB;

    VOID            CreateCylinder( D3DVertexBuffer** ppVB, D3DIndexBuffer** ppIB,
                                    FLOAT Radius, FLOAT Height,
                                    UINT ColumnWidthDiv, UINT ColumnHeightDiv,
                                    XMVECTOR BottomColor, XMVECTOR TopColor );

    DWORD m_Method;

    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};




//--------------------------------------------------------------------------------------
// Vertex shader
// We use the register semantic here to directly define the input register
// matWVP.  Conversely, we could let the HLSL compiler decide and check the
// constant table.
//--------------------------------------------------------------------------------------
const CHAR*         g_strVertexShaderProgram =
    " float4x4 matWVP : register(c0);              \n"
    " float4x3 matBones[60] : register(c4);        \n"
    "                                              \n"
    " struct VS_IN                                 \n"
    "                                              \n"
    " {                                            \n"
    "     float3 ObjPos   : POSITION;              \n"
    "     float4 Color    : COLOR;                 \n"
    "     float4 Indices  : BLENDINDICES;          \n"
    "     float4 Weights  : BLENDWEIGHT;           \n"
    " };                                           \n"
    "                                              \n"
    " struct VS_OUT                                \n"
    " {                                            \n"
    "     float4 ProjPos  : POSITION;              \n"
    "     float4 Color    : COLOR;                 \n"
    " };                                           \n"
    "                                              \n"
    " VS_OUT main( VS_IN In )                      \n"
    " {                                            \n"
    "     VS_OUT Out;                              \n"
    "     float3 Pos =                              \n"
    "         mul( float4( In.ObjPos, 1 ), matBones[In.Indices[0]] ); // * In.Weights[0] + \n"
    "     //    mul( float4( In.ObjPos, 1 ), matBones[In.Indices[1]] ) * In.Weights[1] + \n"
    "     //    mul( float4( In.ObjPos, 1 ), matBones[In.Indices[2]] ) * In.Weights[2] + \n"
    "     //    mul( float4( In.ObjPos, 1 ), matBones[In.Indices[3]] ) * In.Weights[3];  \n"
    "     Out.ProjPos = mul( matWVP, float4( Pos, 1 ) ); \n"
    "     Out.Color = In.Color;                    \n"
    "     return Out;                              \n"
    " }                                            \n";


//--------------------------------------------------------------------------------------
// Pixel shader
//--------------------------------------------------------------------------------------
const CHAR*         g_strPixelShaderProgram =
    " struct PS_IN                                 \n"
    " {                                            \n"
    "     float4 Color : COLOR;                    \n"
    " };                                           \n"
    "                                              \n"
    " float4 main( PS_IN In ) : COLOR              \n"
    " {                                            \n"
    "     return In.Color;                         \n"
    " }                                            \n";

//--------------------------------------------------------------------------------------
// Define the vertex elements.
//--------------------------------------------------------------------------------------
static const D3DVERTEXELEMENT9 s_SkinVertexElements[5] =
{
    { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
    { 0, 16, D3DDECLTYPE_UBYTE4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
    { 0, 20, D3DDECLTYPE_USHORT4N,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0 },
    D3DDECL_END()
};

struct SkinVertex
{
    XMFLOAT3 Position;
    D3DCOLOR Color;
    BYTE    Indices[4];
    WORD    Weights[4];
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample App;
    ATG::GetVideoSettings( &App.m_d3dpp.BackBufferWidth, &App.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    App.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    App.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    // Increase the secondary ring buffer size, so that our CPU timings are 
    // not skewed by stalls.
    App.m_d3dpp.RingBufferParameters.SecondarySize = 3*1024*1024;

    App.Run();
}


//--------------------------------------------------------------------------------------
// Name: FlushCache()
// Desc: Flush the given region from the cache
//--------------------------------------------------------------------------------------
VOID FlushCache( VOID* pMem, UINT NumBytes )
{
    UINT NumLines = ( NumBytes + 127 ) / 128;
    for( UINT i = 0; i < NumLines; i++ )
    {
        __dcbf( i * 128, pMem );
    }
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "d:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "d:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    m_bDrawHelp = FALSE;

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // projection matrix
    m_Proj = XMMatrixPerspectiveFovLH( XM_PI / 2.0f, fAspectRatio, 1.0f, 300.0f );

    // Buffers to hold compiled shaders and possible error messages
    ID3DXBuffer* pShaderCode = NULL;

    // Compile vertex shader.
    HRESULT hr = D3DXCompileShader( g_strVertexShaderProgram, ( UINT )strlen( g_strVertexShaderProgram ),
                                    NULL, NULL, "main", "vs_3_0", 0,
                                    &pShaderCode, NULL, NULL );
    if( FAILED( hr ) )
        exit( 1 );

    // Create pixel shader.
    m_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                      &m_pCylinderVertexShader );

    // Shader code is no longer required.
    pShaderCode->Release();
    pShaderCode = NULL;

    // Compile pixel shader.
    hr = D3DXCompileShader( g_strPixelShaderProgram, ( UINT )strlen( g_strPixelShaderProgram ),
                            NULL, NULL, "main", "ps_3_0", 0,
                            &pShaderCode, NULL, NULL );
    if( FAILED( hr ) )
        exit( 1 );

    // Create pixel shader.
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                     &m_pCylinderPixelShader );

    // Shader code no longer required.
    pShaderCode->Release();
    pShaderCode = NULL;

    // Create a vertex declaration from the element descriptions.
    m_pd3dDevice->CreateVertexDeclaration( s_SkinVertexElements, &m_pCylinderVertexDecl );

    // create the cylinder vertex and index buffers
    CreateCylinder( &m_pCylinderVB, &m_pCylinderIB,
                    CylinderRadius, CylinderHeight,
                    CylinderHorizontalDiv, CylinderVerticalDiv,
                    CylinderBottomColor, CylinderTopColor );

    // seed random number generator
    srand( 0 );

    // create cylinder WVPs
    m_pCylinderWVP = ( XMMATRIX* )XPhysicalAlloc( NumCylinderWVPBuffers * NumCylinders * sizeof( XMMATRIX ),
                                                  MAXULONG_PTR, 16, PAGE_READWRITE );
    ZeroMemory( &m_CylinderWVPFences, sizeof( m_CylinderWVPFences ) );


    // create cylinder world matrices
    m_pCylinderWorld = new XMMATRIX[NumCylinders];
    for( UINT x = 0; x < NumCylinders; x++ )
    {
        FLOAT OffsetDistance = FarmRadius * FLOAT( rand() ) / RAND_MAX;
        FLOAT OffsetAngle = XM_PI * 2.0f * FLOAT( rand() ) / RAND_MAX;
        FLOAT Angle = XM_PI * 2.0f * FLOAT( rand() ) / RAND_MAX;
        m_pCylinderWorld[x] =
            XMMatrixRotationY( Angle ) *
            XMMatrixTranslation( OffsetDistance * cosf( OffsetAngle ), -5.0f, OffsetDistance * sinf( OffsetAngle ) );
    }

    // create cylinder animation data
    m_pCylinderAnim = ( BoneMatrix* )XPhysicalAlloc( NumAnimFrames * CylinderBoneCount * sizeof( BoneMatrix ),
                                                     MAXULONG_PTR, 16, PAGE_READWRITE );
    m_pCylinderAnimCompressed = new CompressedBoneMatrix[ NumAnimFrames * CylinderBoneCount ];
    for( UINT f = 0; f < NumAnimFrames; f++ )
    {
        FLOAT Angle = cosf( XM_2PI / ( NumAnimFrames - 1 ) * FLOAT( f ) ) * ( XM_PI / ( CylinderBoneCount - 1 ) );

        XMMATRIX World = XMMatrixIdentity();
        for( UINT i = 0; i < CylinderBoneCount; i++ )
        {
            XMMATRIX Local;
            if( i == 0 )
                Local = XMMatrixIdentity();
            else if( i == CylinderBoneCount - 1 )
                Local = XMMatrixIdentity();
            else
                Local = XMMatrixTranslation( 0.0f, CylinderHeight / CylinderVerticalDiv,
                                             0.0f ) * XMMatrixRotationZ( Angle );
            World *= Local;

            XMMATRIX InvBindPose = XMMatrixTranslation( 0.0f, -CylinderHeight / CylinderVerticalDiv * FLOAT( i ),
                                                        0.0f );
            XMMATRIX Bone = InvBindPose * World;

            XMMATRIX TransBone = XMMatrixTranspose( Bone );

            BoneMatrix* pMat = &m_pCylinderAnim[f * CylinderBoneCount + i];
            pMat->m[0] = TransBone.m[0];
            pMat->m[1] = TransBone.m[1];
            pMat->m[2] = TransBone.m[2];

            CompressedBoneMatrix* pCompressedBone = &m_pCylinderAnimCompressed[f * CylinderBoneCount + i];
            pCompressedBone->Scale = 1.0f;
            pCompressedBone->Position = XMFLOAT3( Bone._41, Bone._42, Bone._43 );
            XMStoreFloat4( &pCompressedBone->Rotation, XMQuaternionRotationMatrix( Bone ) );
        }
    }

    // flush animation data out of the cache
    FlushCache( m_pCylinderAnim, NumAnimFrames * CylinderBoneCount * sizeof( BoneMatrix ) );

    // init cylinder animation frame offsets
    for( UINT i = 0; i < NumCylinders; i++ )
    {
        m_CylinderAnimFrameOffset[i] = rand() % NumAnimFrames;
    }

    m_Method = BASIC;
    m_fAnimationTime = 0;
    m_fDeltaTime = 0;
    m_bAnimationPaused = FALSE;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    m_fDeltaTime = FLOAT( m_Timer.GetElapsedTime() );

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Start button pauses the animation timer.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        m_bAnimationPaused = !m_bAnimationPaused;
    }
    if( !m_bAnimationPaused )
        m_fAnimationTime += m_fDeltaTime;

    // The A button cycles the constant setting method.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_Method = ( m_Method + 1 ) % METHOD_MAX;
    }

    // View matrix
    static FLOAT RotY = -XM_PIDIV2;
    static FLOAT RotX = XM_PI / 8.0f;
    static FLOAT Zoom = FarmRadius * 1.3f;

    static FLOAT RotSpeed = 1.0f;
    static FLOAT ZoomSpeed = 20.0f;

    if( fabsf( pGamepad->fX1 ) > 0.001 )
        RotY += pGamepad->fX1 * m_fDeltaTime * RotSpeed;
    if( fabsf( pGamepad->fY1 ) > 0.001 )
        RotX += pGamepad->fY1 * m_fDeltaTime * RotSpeed;
    if( pGamepad->bLeftTrigger > 20 )
        Zoom += pGamepad->bLeftTrigger / 255.0f * m_fDeltaTime * ZoomSpeed;
    if( pGamepad->bRightTrigger > 20 )
        Zoom -= pGamepad->bRightTrigger / 255.0f * m_fDeltaTime * ZoomSpeed;

    if( Zoom <= 0.0f )
        Zoom = 0.001f;
    const FLOAT fRotXLimit = XM_PIDIV2 * 0.99f;
    if( RotX < -fRotXLimit )
        RotX = -fRotXLimit;
    if( RotX > fRotXLimit )
        RotX = fRotXLimit;

    XMVECTOR vEyePt = XMVectorSet( cosf( RotY ) * cosf( RotX ), sinf( RotX ), sinf( RotY ) * cosf( RotX ),
                                   0.0f ) * Zoom;
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_View = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CopyAlignedCachedToUnalignedWriteCombiningF4()
// Desc: Copies a 16-byte aligned buffer in cached memory to an unaligned destination in
//       write combined memory.
//--------------------------------------------------------------------------------------
VOID CopyAlignedCachedToUnalignedWriteCombiningF4( VOID* __restrict pDst, VOID* __restrict pSrc, SIZE_T Count )
 {
    assert( !( Count & 0x000003F ) ); // must be 64 byte sized
    assert( !( ( DWORD )pSrc & 0x000000F ) ); // must be 16 byte aligned

    __vector4 T[4];

    for( UINT i = 0; i < Count; i += 64 )
 {
        T[0] = __lvx( pSrc,i );
        T[1] = __lvx( pSrc,i+16 );
        T[2] = __lvx( pSrc,i+32 );
        T[3] = __lvx( pSrc,i+48 );

        //__stvlx( T[0], pDst, i );
        //__stvrx( T[0], pDst, i+16 );
        //__stvlx( T[1], pDst, i+16 );
        //__stvrx( T[1], pDst, i+32 );
        //__stvlx( T[2], pDst, i+32 );
        //__stvrx( T[2], pDst, i+48 );
        //__stvlx( T[3], pDst, i+48 );
        //__stvrx( T[3], pDst, i+64 );
        __stvx_volatile( T[0], pDst, i );
        __stvx_volatile( T[1], pDst, i+16 );
        __stvx_volatile( T[2], pDst, i+32 );
        __stvx_volatile( T[3], pDst, i+48 );
    }
}


//--------------------------------------------------------------------------------------
// Name: CopyCachedToWriteCombiningF4()
// Desc: Copies a 4-byte aligned buffer in cached memory to a 4-byte aligned destination 
//       in write combined memory.
//--------------------------------------------------------------------------------------
VOID CopyCachedToWriteCombiningF4( VOID* __restrict pDst, VOID* __restrict pSrc, SIZE_T Count )
 {
    assert( !( Count & 0x000003F ) ); // must be 64 byte sized
    assert( !( ( DWORD )pDst & 0x0000003 ) ); // must be 4 byte aligned
    assert( !( ( DWORD )pSrc & 0x0000003 ) ); // must be 4 byte aligned

    for( UINT i = 0; i < Count; i += 64 )
 {
        DWORD* pTSrc = ( DWORD* )( ( BYTE* )pSrc + i );
        DWORD* pTDst = ( DWORD* )( ( BYTE* )pDst + i );

        pTDst[0] = pTSrc[0];
        pTDst[1] = pTSrc[1];
        pTDst[2] = pTSrc[2];
        pTDst[3] = pTSrc[3];
        pTDst[4] = pTSrc[4];
        pTDst[5] = pTSrc[5];
        pTDst[6] = pTSrc[6];
        pTDst[7] = pTSrc[7];
        pTDst[8] = pTSrc[8];
        pTDst[9] = pTSrc[9];
        pTDst[10] = pTSrc[10];
        pTDst[11] = pTSrc[11];
        pTDst[12] = pTSrc[12];
        pTDst[13] = pTSrc[13];
        pTDst[14] = pTSrc[14];
        pTDst[15] = pTSrc[15];
    }
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    assert( m_Method < METHOD_MAX );

    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

    m_pd3dDevice->SetVertexDeclaration( m_pCylinderVertexDecl );
    m_pd3dDevice->SetVertexShader( m_pCylinderVertexShader );
    m_pd3dDevice->SetPixelShader( m_pCylinderPixelShader );
    m_pd3dDevice->SetIndices( m_pCylinderIB );
    m_pd3dDevice->SetStreamSource( 0, m_pCylinderVB, 0, sizeof( SkinVertex ) );
    D3DINDEXBUFFER_DESC Desc;
    m_pCylinderIB->GetDesc( &Desc );
    UINT NumIndices = Desc.Size / sizeof( WORD );

    // set up world*view*proj matrices and bone matrices
    UINT Frame = UINT( m_fAnimationTime * AnimFreq );

    static FLOAT* pAnimBones[NumCylinders];
    static XMMATRIX* pWVP;

    UINT FreeCylinderWVPBuffer;
    for( FreeCylinderWVPBuffer = 0; FreeCylinderWVPBuffer < NumCylinderWVPBuffers; FreeCylinderWVPBuffer++ )
    {
        if( !m_pd3dDevice->IsFencePending( m_CylinderWVPFences[FreeCylinderWVPBuffer] ) )
            break;
    }

    if( FreeCylinderWVPBuffer == NumCylinderWVPBuffers )
    {
        ATG::DebugSpew( "Lock stall\n" );
        FreeCylinderWVPBuffer = 0;
        m_pd3dDevice->BlockOnFence( m_CylinderWVPFences[0] );
    }
    pWVP = &m_pCylinderWVP[FreeCylinderWVPBuffer * NumCylinders];

    for( UINT i = 0; i < NumCylinders; i++ )
    {
        UINT AnimFrame = ( Frame + m_CylinderAnimFrameOffset[i] ) % NumAnimFrames;
        pAnimBones[i] = ( FLOAT* )( m_pCylinderAnim + AnimFrame * CylinderBoneCount );
        pWVP[i] = m_pCylinderWorld[i] * m_View * m_Proj;
    }

    // block until idle to simulate
    m_pd3dDevice->BlockUntilIdle();


    LARGE_INTEGER Freq;
    QueryPerformanceFrequency( &Freq );

    LARGE_INTEGER StartTime, EndTime;

    switch( m_Method )
    {
        case BASIC:
        {
            // BASIC: This is the default method of submitting constants.
            // It uses the SetVertexShaderConstantF API, which copies constant data into
            // Direct3D's shadow state.

            QueryPerformanceCounter( &StartTime );

            // submit matrices and draw
            for( UINT i = 0; i < NumCylinders; i++ )
            {
                m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )( pWVP + i ), 4 );
                m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )( pAnimBones[i] ), CylinderBoneCount * 3 );

                m_pd3dDevice->DrawIndexedVertices( D3DPT_TRIANGLELIST, 0, 0, NumIndices );
            }

            QueryPerformanceCounter( &EndTime );
            break;
        }
        case GPU_INDIRECT_CONSTANT_LOAD:
        {
            // GPU_INDIRECT_CONSTANT_LOAD: This method has the GPU directly load shader
            // constants from the sample's buffers.  Since the GPU is accessing these
            // constants directly without Direct3D being involved, we must use the
            // GpuOwn and GpuDisown APIs to disable Direct3D's shadow state management
            // for the constants we're submitting here.

            FlushCache( pWVP, NumCylinders * sizeof( XMMATRIX ) );

            m_pd3dDevice->GpuOwnVertexShaderConstantF( 0, 4 + CylinderBoneCount * 3 );
            QueryPerformanceCounter( &StartTime );

            // submit matrices and draw
            for( UINT i = 0; i < NumCylinders; i++ )
            {
                m_pd3dDevice->GpuLoadVertexShaderConstantF4Pointer( 0, ( FLOAT* )( pWVP + i ), 4 );
                m_pd3dDevice->GpuLoadVertexShaderConstantF4Pointer( 4, ( FLOAT* )( pAnimBones[i] ),
                                                                    CylinderBoneCount * 3 );

                m_pd3dDevice->DrawIndexedVertices( D3DPT_TRIANGLELIST, 0, 0, NumIndices );
            }

            m_CylinderWVPFences[FreeCylinderWVPBuffer] = m_pd3dDevice->GetCurrentFence();

            QueryPerformanceCounter( &EndTime );

            m_pd3dDevice->GpuDisownVertexShaderConstantF( 0, 4 + CylinderBoneCount * 3 );
            break;
        }
        case GPU_CONSTANT_LOAD:
        {
            // GPU_CONSTANT_LOAD: This method copies shader constants directly into the
            // GPU command buffer.  Again, since this operation bypasses Direct3D's
            // shadow state management, GpuOwn/GpuDisown APIs are used to disable shadow
            // state management for the constants we are submitting.

            m_pd3dDevice->GpuOwnVertexShaderConstantF( 0, 4 + CylinderBoneCount * 3 );
            QueryPerformanceCounter( &StartTime );

            // submit matrices and draw
            for( UINT i = 0; i < NumCylinders; i++ )
            {
                __dcbt( 0, &pWVP[i] );
                for( UINT j = 0; j < CylinderBoneCount * sizeof( BoneMatrix ) + 127; j += 128 )
                {
                    __dcbt( j, pAnimBones[i] );
                }

                BYTE* pCommandBufferData;
                m_pd3dDevice->GpuBeginVertexShaderConstantF4( 0, ( D3DVECTOR4** )&pCommandBufferData,
                                                              4 + CylinderBoneCount * 3 );

                CopyAlignedCachedToUnalignedWriteCombiningF4( pCommandBufferData, ( FLOAT* )( pWVP + i ), sizeof
                                                              ( XMMATRIX ) );
                CopyAlignedCachedToUnalignedWriteCombiningF4( pCommandBufferData + sizeof( XMMATRIX ), ( FLOAT* )
                                                              ( pAnimBones[i] ), CylinderBoneCount * sizeof
                                                              ( BoneMatrix ) );

                m_pd3dDevice->GpuEndVertexShaderConstantF4();

                m_pd3dDevice->DrawIndexedVertices( D3DPT_TRIANGLELIST, 0, 0, NumIndices );
            }

            QueryPerformanceCounter( &EndTime );

            m_pd3dDevice->GpuDisownVertexShaderConstantF( 0, 4 + CylinderBoneCount * 3 );
            break;
        }
        case DIRECT_TO_SHADOW:
        {
            // DIRECT_TO_SHADOW: This method provides direct access to Direct3D's
            // shadow state buffers.  Instead of using SetVertexShaderConstantF, which
            // performs a copy into the shadow state, BeginVertexShaderConstantF1 simply
            // provides a pointer to the shadow state buffer, and the title is
            // responsible for performing the copy.  This is analogous to the difference
            // between the DrawPrimitiveUP and BeginVertices/EndVertices APIs for drawing
            // dynamic geometry.

            QueryPerformanceCounter( &StartTime );

            // submit matrices and draw
            for( UINT i = 0; i < NumCylinders; i++ )
            {
                BYTE* pShadowData;
                m_pd3dDevice->BeginVertexShaderConstantF1( 0, ( D3DVECTOR4** )&pShadowData, 4 + CylinderBoneCount *
                                                           3 );

                //__dcbt( 0, &pWVP[i] );
                //for( UINT j = 0; j < CylinderBoneCount * sizeof( BoneMatrix ) + 127; j += 128 )
                //{
                //    __dcbt( j, pAnimBones[i] );
                //}

                // Copy data to the Direct3D shadow data.
                // The title would ideally generate the data directly into this destination
                // pointer, instead of performing a copy.
                XMemCpy( pShadowData, ( FLOAT* )( pWVP + i ), sizeof( XMMATRIX ) );
                //__vector4 vx = __lvx( &pWVP[i], 0 );
                //__vector4 vy = __lvx( &pWVP[i], 16 );
                //__vector4 vz = __lvx( &pWVP[i], 32 );
                //__vector4 vw = __lvx( &pWVP[i], 48 );
                //__stvx( vx, pShadowData, 0 );
                //__stvx( vy, pShadowData, 16 );
                //__stvx( vz, pShadowData, 32 );
                //__stvx( vw, pShadowData, 48 );
                XMemCpy( pShadowData + sizeof( XMMATRIX ), ( FLOAT* )( pAnimBones[i] ), CylinderBoneCount * sizeof
                         ( BoneMatrix ) );
                //BYTE* pSrcData = (BYTE*) pAnimBones[i];
                //pShadowData += sizeof( XMMATRIX );
                //for( UINT j = 0; j < CylinderBoneCount; ++j )
                //{
                //    __vector4 vx = __lvx( pSrcData, 0 );
                //    __vector4 vy = __lvx( pSrcData, 16 );
                //    __vector4 vz = __lvx( pSrcData, 32 );
                //
                //    __stvx( vx, pShadowData, 0 );
                //    __stvx( vy, pShadowData, 16 );
                //    __stvx( vz, pShadowData, 32 );
                //
                //    pSrcData += sizeof( BoneMatrix );
                //    pShadowData += sizeof( BoneMatrix );
                //}
                //
                m_pd3dDevice->EndVertexShaderConstantF1();

                m_pd3dDevice->DrawIndexedVertices( D3DPT_TRIANGLELIST, 0, 0, NumIndices );
            }

            QueryPerformanceCounter( &EndTime );
            break;
        }
        case DIRECT_TO_SHADOW_AND_COMMANDBUFFER:
        {
            // DIRECT_TO_SHADOW_AND_COMMANDBUFFER: This method is similar to the previous
            // method, except the BeginVertexShaderConstantF4 API also provides a pointer
            // to the command buffer destination for the shader constants.  The title is
            // now responsible for updating both the shadow copy and the command buffer
            // copy at the same time.

            QueryPerformanceCounter( &StartTime );

            // submit matrices and draw
            for( UINT i = 0; i < NumCylinders; i++ )
            {
                BYTE* pShadowData;
                BYTE* pCommandBufferData;

                m_pd3dDevice->BeginVertexShaderConstantF4( 0, ( D3DVECTOR4** )&pShadowData,
                                                           ( D3DVECTOR4** )&pCommandBufferData, 4 + CylinderBoneCount *
                                                           3 );
                assert( pShadowData && pCommandBufferData );

                __dcbt( 0, &pWVP[i] );
                for( UINT j = 0; j < CylinderBoneCount * sizeof( BoneMatrix ) + 127; j += 128 )
                {
                    __dcbt( j, pAnimBones[i] );
                }

                // Copy data to the Direct3D shadow data.
                XMemCpy( pShadowData, ( FLOAT* )( pWVP + i ), sizeof( XMMATRIX ) );
                XMemCpy( pShadowData + sizeof( XMMATRIX ), ( FLOAT* )( pAnimBones[i] ), CylinderBoneCount * sizeof
                         ( BoneMatrix ) );

                // Copy data to the command buffer.
                CopyAlignedCachedToUnalignedWriteCombiningF4( pCommandBufferData, ( FLOAT* )( pWVP + i ), sizeof
                                                              ( XMMATRIX ) );
                CopyAlignedCachedToUnalignedWriteCombiningF4( pCommandBufferData + sizeof( XMMATRIX ), ( FLOAT* )
                                                              ( pAnimBones[i] ), CylinderBoneCount * sizeof
                                                              ( BoneMatrix ) );

                m_pd3dDevice->EndVertexShaderConstantF4();
                m_pd3dDevice->DrawIndexedVertices( D3DPT_TRIANGLELIST, 0, 0, NumIndices );
            }

            QueryPerformanceCounter( &EndTime );
            break;
        }
        default:
        {
            QueryPerformanceCounter( &StartTime );
            QueryPerformanceCounter( &EndTime );
        }
    }

    FLOAT ConstantSubmitTime = ( FLOAT )( ( DOUBLE )( EndTime.QuadPart - StartTime.QuadPart ) / ( DOUBLE )
                                          Freq.QuadPart );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"FastGPUConstants" );

        if( m_d3dpp.BackBufferWidth >= 1280 )
        {
            m_Font.SetScaleFactors( 1.0f, 1.0f );
        }
        else
        {
            m_Font.SetScaleFactors( 0.8f, 1.0f );
        }

        static WCHAR strBuf[100];
        swprintf_s( strBuf, L"Method: %s", MethodStrings[m_Method] );
        m_Font.DrawText( 0, 32.0f, 0xffffffff, strBuf );

        swprintf_s( strBuf, L"CPU Draw Time: %2.2f(ms)", ConstantSubmitTime * 1000.0f );
        m_Font.DrawText( 0, 64.0f, 0xffffffff, strBuf );

        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateCylinder()
// Desc: Builds a cylinder VB/IB set with skin weights for skeletal mesh deformation.
//--------------------------------------------------------------------------------------
VOID Sample::CreateCylinder( D3DVertexBuffer** ppVB, D3DIndexBuffer** ppIB,
                             FLOAT Radius, FLOAT Height,
                             UINT ColumnWidthDiv, UINT ColumnHeightDiv,
                             XMVECTOR TopColor, XMVECTOR BottomColor )
{
    // The vertices are in rings.  There are 'vertical div + 1' rings, each has 'horizontal div' vertices
    UINT VertCount = ( ColumnHeightDiv + 1 ) * ColumnWidthDiv + 2;

    // Each 'vertical div' takes two triangles.  There are 'horzontal div' rings plus end caps.
    UINT IndexCount = ( ColumnWidthDiv * ColumnHeightDiv * 6 ) + ColumnWidthDiv * 2 * 3;

    m_pd3dDevice->CreateVertexBuffer( VertCount * sizeof( SkinVertex ), 0, 0, 0, ppVB, NULL );
    m_pd3dDevice->CreateIndexBuffer( IndexCount * sizeof( WORD ), 0, D3DFMT_INDEX16, 0, ppIB, NULL );

    SkinVertex* pVerts = NULL;
    ( *ppVB )->Lock( 0, 0, ( VOID** )&pVerts, 0 );

    for( UINT h = 0; h < ColumnHeightDiv + 1; h++ )
    {
        FLOAT RingHeight = 0.0f + Height / ColumnHeightDiv * FLOAT( h );
        SkinVertex* pRingVerts = pVerts + ColumnWidthDiv * h;

        FLOAT ColorDelta = FLOAT( h ) / FLOAT( ColumnHeightDiv );

        XMVECTOR RingColor = XMVectorLerp( BottomColor, TopColor, ColorDelta );

        for( UINT w = 0; w < ColumnWidthDiv; w++ )
        {
            FLOAT Angle = XM_2PI / ColumnWidthDiv * w;
            pRingVerts[w].Position.x = cosf( Angle ) * Radius;
            pRingVerts[w].Position.z = sinf( Angle ) * Radius;
            pRingVerts[w].Position.y = RingHeight;
            pRingVerts[w].Color = D3DCOLOR_COLORVALUE( RingColor.x, RingColor.y, RingColor.z, RingColor.w );
            pRingVerts[w].Indices[3] = BYTE( h );
            pRingVerts[w].Indices[2] = 0;
            pRingVerts[w].Indices[1] = 0;
            pRingVerts[w].Indices[0] = 0;
            pRingVerts[w].Weights[0] = WORD( -1 );
            pRingVerts[w].Weights[1] = 0;
            pRingVerts[w].Weights[2] = 0;
            pRingVerts[w].Weights[3] = 0;
        }
    }
    pVerts[VertCount - 2].Position = XMFLOAT3( 0.0f, 0.0f, 0.0f );
    pVerts[VertCount - 2].Color = D3DCOLOR_COLORVALUE( BottomColor.x, BottomColor.y, BottomColor.z, BottomColor.w );
    pVerts[VertCount - 2].Indices[3] = 0;
    pVerts[VertCount - 2].Indices[2] = 0;
    pVerts[VertCount - 2].Indices[1] = 0;
    pVerts[VertCount - 2].Indices[0] = 0;
    pVerts[VertCount - 2].Weights[0] = WORD( -1 );
    pVerts[VertCount - 2].Weights[1] = 0;
    pVerts[VertCount - 2].Weights[2] = 0;
    pVerts[VertCount - 2].Weights[3] = 0;
    pVerts[VertCount - 1].Position = XMFLOAT3( 0.0f, Height, 0.0f );
    pVerts[VertCount - 1].Color = D3DCOLOR_COLORVALUE( TopColor.x, TopColor.y, TopColor.z, TopColor.w );
    pVerts[VertCount - 1].Indices[3] = BYTE( ColumnHeightDiv );
    pVerts[VertCount - 1].Indices[2] = 0;
    pVerts[VertCount - 1].Indices[1] = 0;
    pVerts[VertCount - 1].Indices[0] = 0;
    pVerts[VertCount - 1].Weights[0] = WORD( -1 );
    pVerts[VertCount - 1].Weights[1] = 0;
    pVerts[VertCount - 1].Weights[2] = 0;
    pVerts[VertCount - 1].Weights[3] = 0;

    ( *ppVB )->Unlock();


    WORD* pIndices = NULL;
    ( *ppIB )->Lock( 0, 0, ( VOID** )&pIndices, 0 );

    // rings
    for( UINT h = 0; h < ColumnHeightDiv; h++ )
    {
        WORD* pRingIndices = pIndices + 6 * ColumnWidthDiv * h;
        for( UINT w = 0; w < ColumnWidthDiv; w++ )
        {
            WORD* pSegIndices = pRingIndices + 6 * w;
            pSegIndices[0] = WORD( ColumnWidthDiv * h + w );
            pSegIndices[1] = WORD( ColumnWidthDiv * ( h + 1 ) + w );
            pSegIndices[2] = WORD( ColumnWidthDiv * h + ( w + 1 ) % ColumnWidthDiv );
            pSegIndices[3] = WORD( ColumnWidthDiv * h + ( w + 1 ) % ColumnWidthDiv );
            pSegIndices[4] = WORD( ColumnWidthDiv * ( h + 1 ) + w );
            pSegIndices[5] = WORD( ColumnWidthDiv * ( h + 1 ) + ( w + 1 ) % ColumnWidthDiv );
        }
    }

    // caps
    WORD* pBottomCapIndices = pIndices + 6 * ColumnWidthDiv * ColumnHeightDiv;
    for( UINT w = 0; w < ColumnWidthDiv; w++ )
    {
        WORD* pSegIndices = pBottomCapIndices + 3 * w;
        pSegIndices[0] = WORD( VertCount - 2 );
        pSegIndices[1] = WORD( w );
        pSegIndices[2] = WORD( ( w + 1 ) % ColumnWidthDiv );
    }
    WORD* pTopCapIndices = pIndices + 6 * ColumnWidthDiv * ColumnHeightDiv + 3 * ColumnWidthDiv;
    for( UINT w = 0; w < ColumnWidthDiv; w++ )
    {
        WORD* pSegIndices = pTopCapIndices + 3 * w;
        pSegIndices[0] = WORD( VertCount - 1 );
        pSegIndices[2] = WORD( ColumnHeightDiv * ColumnWidthDiv + w );
        pSegIndices[1] = WORD( ColumnHeightDiv * ColumnWidthDiv + ( w + 1 ) % ColumnWidthDiv );
    }

    ( *ppIB )->Unlock();
}


