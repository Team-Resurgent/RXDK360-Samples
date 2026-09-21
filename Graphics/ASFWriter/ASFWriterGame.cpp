//--------------------------------------------------------------------------------------
// ASFWriterGame.cpp
//
// The sample demonstrates using the XAV ASF Writer to record video and audio from the console.
// ASFWriterGame.cpp is the "game", and ASFWriterHelper is the code that actually uses the ASF Writer.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <assert.h>
#include <AtgApp.h>
#include <AtgAudio.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <x3daudio.h>
#include <xgraphics.h>
#include <AtgPostProcess.h>

#include "ASFWriterHelper.h"

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_1, L"Display help" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_2, L"Navigate Menu" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Start/Stop\nRecording" },
    { ATG::HELP_Y_BUTTON,     ATG::HELP_PLACEMENT_2, L"Change Menus" },
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_2, L"Move object\nin X/Z" },
};

const DWORD             NUM_HELP_CALLOUTS = ARRAY_SIZE( g_HelpCallouts );


//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------

// Channel count
const DWORD             CHANNELCOUNT = 6;


// Constants for setting the video quality and bitrate. 
// These can be changed, and a future sample will demonstrate this.
const DWORD               MIN_VIDEO_BITRATE = 1024;
const DWORD               MAX_VIDEO_BITRATE = 2048;
const DWORD               VIDEO_QUALITY = 75;

// Constants to define our world space
const INT               XMIN = -10;
const INT               XMAX = 10;
const INT               ZMIN = -10;
const INT               ZMAX = 10;

// Constants for colors
static const DWORD      SOURCE_COLOR = 0xffea1b1b;
static const DWORD      LISTENER_COLOR = 0xff1b1bea;
static const DWORD      FLOOR_COLOR = 0xff101010;
static const DWORD      GRID_COLOR = 0xff00a000;

// Constants for volume
static const float      VOLUME_MIN = 0.25f;
static const float      VOLUME_MAX = 1.0f;

// Constants for scaling input
const FLOAT             MOTION_SCALE = 10.0f;


//--------------------------------------------------------------------------------------
// Global variables and definitions
//--------------------------------------------------------------------------------------

struct D3DVERTEX
{
    XMFLOAT3 p;           // position
    D3DCOLOR c;           // color
};

// Bitrates for the video encoder. Note that these have been arbitrarily chosen, 
// and you may choose alternative bitrates.
static const UINT32 VIDEO_BITRATES[] = { 500, 1000, 2000, 3000, 4000 };
static const UINT32 VIDEO_BITRATES_SIZE = sizeof( VIDEO_BITRATES ) / sizeof( UINT32 );

// Quality settings for the encoder. Note that these have been arbitrarily chosen, 
// and you may choose alternate quality settings.
static const UINT32 VIDEO_QUALITIES[] = { 0, 33, 66, 100};
static const UINT32 VIDEO_QUALITIES_SIZE = sizeof( VIDEO_QUALITIES ) / sizeof( UINT32 );

// Frame rate settings for the encoder. Note that these have been arbitrarily chosen, 
// and you may choose alternative quality settings.
static const UINT32 VIDEO_FRAME_RATES[] = { 15, 20, 30, 60};
static const UINT32 VIDEO_FRAME_RATES_SIZE = sizeof( VIDEO_FRAME_RATES ) / sizeof( FLOAT );

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    // XAudio2 stuff
    IXAudio2* m_pXAudio2;
    IXAudio2MasteringVoice*  m_pMasteringVoice;
    IXAudio2SourceVoice*     m_pSourceVoice;
    IXAudio2SubmixVoice*     m_pSubmixVoiceResampler;
    IXAudio2SubmixVoice*     m_pSubmixVoicePipe;
    CASFPipeAPO*           m_pPipeAPO;
    BYTE*                    m_pbWaveData;

    // Video Scaling stuff
    ATG::PostProcess         m_PostProcess;
    DWORD                    m_dwVideoWidth;
    DWORD                    m_dwVideoHeight;
    LPDIRECT3DTEXTURE9       m_pSmallFrontBuffer;
    LPDIRECT3DTEXTURE9       m_pSmallFrontBufferLinear;


    // Audio/Video Recording Variables
    ASFWriterHelper          m_Encoder;           // Helper for encoding ASF files.
    XMVEncoderGraphicsPipe*  m_pGraphicsPipe;
    UINT32                   m_iWMARateListIndex;
    UINT32                   m_iWMARateIndex;
    INT32                    m_iVideoScaleFactor;
    UINT32                   m_iVideoFrameRateIndex;
    UINT32                   m_iVideoBitrateMinIndex;
    UINT32                   m_iVideoBitrateMaxIndex;
    UINT32                   m_iVideoQualityIndex;

    // Misc
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    DOUBLE m_dPriorTime;
    BOOL m_bVideoPipeSaturation;

    enum Menu
    {
        MenuAudioSampleRate,
        MenuAudioBitRate,
        MenuVideoBitRateMin,
        MenuVideoBitRateMax,        
        MenuVideoQuality,
        MenuVideoResolution,
        MenuVideoFramerate,
        MenuItemCount,
        MenuSplit = MenuVideoQuality,
    };
    Menu    m_CurrentMenu;
    BOOL    m_bShowingMenu1;

    // Render stuff
    LPDIRECT3DVERTEXSHADER9 m_pVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pPixelShader;
    IDirect3DVertexDeclaration9* m_pVertexDecl;

    DWORD           m_Pad[1];

    // Sound source and listener positions
    XMVECTOR m_vListenerPosition;       // Listener position vector
    XMVECTOR m_vSourcePosition;         // Source position vector

    // Transform matrices
    XMMATRIX m_matWorld;             // World transform
    XMMATRIX m_matView;              // View transform
    XMMATRIX m_matProj;              // Projection transform
    XMMATRIX m_matViewProj;          // ViewProjection transform

    // Models for floor, source, and listener
    LPDIRECT3DVERTEXBUFFER9 m_pvbFloor;             // Quad for the floor
    LPDIRECT3DVERTEXBUFFER9 m_pvbSource;            // Quad for the source
    LPDIRECT3DVERTEXBUFFER9 m_pvbListener;          // Quad for the listener
    LPDIRECT3DVERTEXBUFFER9 m_pvbGrid;              // Lines to grid the floor

    D3DCOLOR m_dwSourceColor;                // Color for sound source
    D3DCOLOR m_dwListenerColor;              // Color for listener
    D3DCOLOR m_dwBlend;                      // Mask for pulsing selected colors.

    BOOL m_bControlSource;               // Control source or listener

    VOID        InitializeUIElements();
    VOID        InitializeEncoder();
    VOID        LoadPCM( const char* szFilename );
    VOID        ConfigureSampleRate();
    HRESULT     SetVolume();
    VOID        ChangeMenu( BOOL MoveRight );
    VOID        ChangeMenuItem( INT32 Size,  INT32 Increment, UINT32 *CurrentValue );
    VOID        DrawMenuHeader( FLOAT X, FLOAT *Y, BOOL IsCurrent, WCHAR *Text, FLOAT *Width, FLOAT *Height );
    VOID        DrawMenuItem( FLOAT X, FLOAT *Y, BOOL IsCurrent, WCHAR *Text, FLOAT *Width, FLOAT *Height );
private:

    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    atgApp.m_d3dpp.FullScreen_RefreshRateInHz = 60;
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_CurrentMenu = MenuAudioSampleRate;
    m_pGraphicsPipe = 0;
    m_pSmallFrontBufferLinear = 0;
    m_pSmallFrontBuffer = 0;
    m_dPriorTime = 0;
    m_iVideoScaleFactor = 2;
    m_iVideoFrameRateIndex = 2;
    m_iVideoBitrateMinIndex = 1;
    m_iVideoBitrateMaxIndex = 2;
    m_iVideoQualityIndex = 2;
    m_bShowingMenu1 = TRUE;
    m_bVideoPipeSaturation = FALSE;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    m_bDrawHelp = FALSE;
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize the XAudio2 engine, 
    // so that we can attach an APO to the Mastering voice
    UINT32 flags = 0;
    HRESULT hr = XAudio2Create( &m_pXAudio2, flags );
    if( FAILED( hr ) || ( m_pXAudio2 == 0 ) )
        return hr;

    hr = m_pXAudio2->CreateMasteringVoice( &m_pMasteringVoice );
    if( FAILED( hr ) || ( m_pMasteringVoice == 0 ) )
        return hr;

    // Creates m_pSourceVoice with the helicopter sound.
    LoadPCM( "GAME:\\Media\\Sounds\\Heli.wav" );
    
    m_pSubmixVoicePipe = 0;
    m_pSubmixVoiceResampler = 0;

    // Set the initial encoding options
    m_iWMARateListIndex = 3;
    m_iWMARateIndex = 2;

    // Positions
    m_vListenerPosition = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    m_vSourcePosition = XMVectorSet( 0.0f, 0.0f, ( FLOAT )ZMIN, 0.0f );

    InitializeUIElements();


    // Video
    if( FAILED( m_PostProcess.Initialize() ) )
    {
        ATG_PrintError( "DX9Sample: Couldn't initialize the effects library\n" );
        return E_FAIL;
    }

    m_dwVideoWidth = m_d3dpp.BackBufferWidth / m_iVideoScaleFactor;
    m_dwVideoHeight = m_d3dpp.BackBufferHeight / m_iVideoScaleFactor;

    if( FAILED( hr = m_pSourceVoice->Start( 0 ) ) )
    {
        ATG_PrintError( "Source voice couldn't be started.\n" );
        return hr;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: LoadPCM
// Desc: Creates a looping source voice and loads the PCM data into it
//--------------------------------------------------------------------------------------
VOID Sample::LoadPCM( const char* szFilename )
{
    HRESULT hr = S_OK;

    //
    // Read the wave file
    //
    ATG::WaveFile WaveFile;
    if( FAILED( hr = WaveFile.Open( szFilename ) ) )
        ATG::FatalError( "Error %#X opening WAV file\n", hr );

    // Read the format header
    WAVEFORMATEXTENSIBLE wfx = {0};
    if( FAILED( hr = WaveFile.GetFormat( &wfx ) ) )
        ATG::FatalError( "Error %#X reading WAV format\n", hr );

    // Calculate how many bytes and samples are in the wave
    DWORD cbWaveSize = 0;
    WaveFile.GetDuration( &cbWaveSize );

    // Read the sample data into memory
    m_pbWaveData = new BYTE[ cbWaveSize ];
    if( FAILED( hr = WaveFile.ReadSample( 0, m_pbWaveData, cbWaveSize, &cbWaveSize ) ) )
        ATG::FatalError( "Error %#X reading WAV data\n", hr );

    // Create the source voice
    if( FAILED( hr = m_pXAudio2->CreateSourceVoice( &m_pSourceVoice, ( WAVEFORMATEX* )&wfx ) ) )
        ATG::FatalError( "Error %#X creating source voice\n", hr );

    // Submit the wave sample data using an XAUDIO2_BUFFER structure
    XAUDIO2_BUFFER buffer = {0};
    buffer.pAudioData = m_pbWaveData;
    buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    buffer.Flags = XAUDIO2_END_OF_STREAM;  // Tell the source voice not to expect any data after this buffer
    buffer.AudioBytes = cbWaveSize;

    if( FAILED( hr = m_pSourceVoice->SubmitSourceBuffer( &buffer ) ) )
        ATG::FatalError( "Error %#X submitting source buffer\n", hr );

}

//--------------------------------------------------------------------------------------
// Name: ConfigureSampleRate
// Desc: Creates a submix of the specified sample rate and channel count
//--------------------------------------------------------------------------------------
VOID Sample::ConfigureSampleRate()
{

    if( m_Encoder.IsRecording() )
        ATG::FatalError( "Sample rate converting submix cannot be changed while in use.\n" );
    
    // If the submix was instantiated, unhook it from the source voice and destroy.
    if( m_pSubmixVoiceResampler )
    {
        m_pSourceVoice->SetOutputVoices( NULL );
        m_pSubmixVoiceResampler->SetOutputVoices( NULL );
        m_pSubmixVoicePipe->DestroyVoice();
        m_pSubmixVoiceResampler->DestroyVoice();
        m_pPipeAPO->Release();
        m_pSubmixVoicePipe = NULL;
        m_pSubmixVoiceResampler = NULL;
        m_pPipeAPO = NULL;
    }
    HRESULT hr = S_OK;
    hr = m_pXAudio2->CreateSubmixVoice( &m_pSubmixVoiceResampler, 
                                        2,
                                        48000,  // Mastering voice sampling rate
                                        0,      // Flags
                                        1       // Stage
    );
    if( FAILED( hr ) || ( m_pSubmixVoiceResampler == 0 ) )
        ATG::FatalError( "Unable to create the submix resampler.\n" );

    hr = m_pXAudio2->CreateSubmixVoice( &m_pSubmixVoicePipe, 
                                        WMAOPTIONS[m_iWMARateListIndex].nChannels,
                                        WMAOPTIONS[m_iWMARateListIndex].nSamplingRate,
                                        0,      // Flags
                                        2       // Stage
                                        );
    if( FAILED( hr ) || ( m_pSubmixVoicePipe == 0 ) )
        ATG::FatalError( "Unable to create the submix pipe.\n" );

    // Send audio from the source voice to the Resampler submix
    XAUDIO2_SEND_DESCRIPTOR SendDescriptors[2] = {0};
    SendDescriptors[0].pOutputVoice = m_pMasteringVoice;
    SendDescriptors[1].pOutputVoice = m_pSubmixVoiceResampler;
    XAUDIO2_VOICE_SENDS VoiceSends = {0};
    VoiceSends.SendCount = 2;
    VoiceSends.pSends = &SendDescriptors[0];
    m_pSourceVoice->SetOutputVoices( &VoiceSends );

    // Send audio from the Resampler submix to the Pipe submix
    SendDescriptors[0].pOutputVoice = m_pSubmixVoicePipe;
    VoiceSends.SendCount = 1;
    VoiceSends.pSends = &SendDescriptors[0];
    m_pSubmixVoiceResampler->SetOutputVoices( &VoiceSends );

	SetVolume();

    // Create the Pipe APO that will send audio to the encoder.
    CASFPipeAPO::CreateInstance( NULL, 0, &m_pPipeAPO );
	XAUDIO2_EFFECT_DESCRIPTOR Descriptor ={0};
	Descriptor.InitialState = FALSE;
	Descriptor.OutputChannels = WMAOPTIONS[m_iWMARateListIndex].nChannels;
	Descriptor.pEffect = static_cast<IXAPO*>( m_pPipeAPO );

	XAUDIO2_EFFECT_CHAIN chain = {0};
	chain.EffectCount = 1;
	chain.pEffectDescriptors = &Descriptor;
    m_pSubmixVoicePipe->SetEffectChain( &chain );

    // Set the volume of the submix maximum for recording purposes.
    m_pSubmixVoicePipe->SetVolume( 1.0f );

    // But change the outgoing volume to be silence.
    // Currently, there can be at most two channels in the submix, going to 6 channels in the mastering voice.
    float fSilence[12] = { 0 };
    m_pSubmixVoicePipe->SetOutputMatrix( m_pMasteringVoice, WMAOPTIONS[m_iWMARateListIndex].nChannels, 6, fSilence );
}


//--------------------------------------------------------------------------------------
// Name: InitializeEncoder()
// Desc: Initialize the Encoder. For simplification of Initialize().
//--------------------------------------------------------------------------------------
void Sample::InitializeEncoder()
{
    HRESULT hr = S_OK;

    ConfigureSampleRate();

    DmMapDevkitDrive();

    WAVEFORMATEX AudioSourceFormat = {0}; 
    AsfWriterBitmapInfoHeader VideoSourceFormat = {0};
    AsfWriterAudioEncInfo AudioEncoderFormat = {0};
    AsfWriterVideoEncInfo VideoEncoderFormat = {0};

    AudioSourceFormat.wFormatTag        = WAVE_FORMAT_PCM;
    AudioSourceFormat.nChannels         = WMAOPTIONS[m_iWMARateListIndex].nChannels;
    AudioSourceFormat.nSamplesPerSec    = WMAOPTIONS[m_iWMARateListIndex].nSamplingRate;
    AudioSourceFormat.nBlockAlign       = AudioSourceFormat.nChannels * sizeof( INT16 );
    AudioSourceFormat.wBitsPerSample    = sizeof( INT16 ) * 8;
    AudioSourceFormat.nAvgBytesPerSec   = AudioSourceFormat.nBlockAlign * AudioSourceFormat.nSamplesPerSec;
    AudioSourceFormat.cbSize            = 0;

    m_dwVideoWidth = m_d3dpp.BackBufferWidth / m_iVideoScaleFactor;
    m_dwVideoHeight = m_d3dpp.BackBufferHeight / m_iVideoScaleFactor;

    // As this is a device-independant bitmap, it is stored in bottom-to-top order. The encoder can automatically
    // reverse this order for us if we specify a negative height.
    VideoSourceFormat.biHeight = -static_cast<LONG>( m_dwVideoHeight );  
    VideoSourceFormat.biSize = sizeof( VideoSourceFormat );
    VideoSourceFormat.biWidth = m_dwVideoWidth;
    VideoSourceFormat.biPlanes = 1;
    VideoSourceFormat.biBitCount = 32;
    VideoSourceFormat.biCompression = BI_RGB;
    VideoSourceFormat.biSizeImage = m_dwVideoWidth * m_dwVideoHeight * 4;

    AudioEncoderFormat.wMode = ASFWRITER_AUDIO_ONEPASS_CBR;
    AudioEncoderFormat.wFormatTag = WAVE_FORMAT_WMAUDIO2;
    AudioEncoderFormat.dwBitRate = WMAOPTIONS[m_iWMARateListIndex].pRates[m_iWMARateIndex].nBitrate ;
    AudioEncoderFormat.dwMaxBitRate = WMAOPTIONS[m_iWMARateListIndex].pRates[m_iWMARateIndex].nBitrate;
    AudioEncoderFormat.cbSize = sizeof( AudioEncoderFormat ); 
    
    VideoEncoderFormat.cbSize = sizeof( VideoEncoderFormat );
    VideoEncoderFormat.wMode = ASFWRITER_VIDEO_ONEPASS_CBR;
    VideoEncoderFormat.dwFormat = FOURCC_WVC1;
    VideoEncoderFormat.dwBitRate = VIDEO_BITRATES[m_iVideoBitrateMinIndex] ;
    VideoEncoderFormat.dwMaxBitRate = VIDEO_BITRATES[m_iVideoBitrateMaxIndex] ;
    VideoEncoderFormat.dwQuality = VIDEO_QUALITY;
    VideoEncoderFormat.dwKeyFrameDistance = ASFWRITER_VIDEO_KEYFRAME_DISTANCE_DEFAULT;
    VideoEncoderFormat.dwBufferDelay = ASFWRITER_VIDEO_BUFFER_DELAY_DEFAULT; 
    VideoEncoderFormat.frameRate = VIDEO_FRAME_RATES[m_iVideoFrameRateIndex];

    if( m_pGraphicsPipe )
        delete m_pGraphicsPipe;
    m_pGraphicsPipe = new XMVEncoderGraphicsPipe();

    // The video format must be [B][G][R][X] 8 bits per channel == D3DFMT_LE_X8R8G8B8
    if( m_pSmallFrontBuffer )
        m_pSmallFrontBuffer->Release();
    if( FAILED( hr = m_pd3dDevice->CreateTexture( m_dwVideoWidth, m_dwVideoHeight, 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 ), 0, &m_pSmallFrontBuffer, NULL ) ) )
    {
        ATG_PrintError( "Couldn't Create Small Front Buffer Texture\n" );
        return;
    }
    if( m_pSmallFrontBufferLinear )
        m_pSmallFrontBufferLinear->Release();
    if( FAILED( hr = m_pd3dDevice->CreateTexture( m_dwVideoWidth, m_dwVideoHeight, 1, 0, ( D3DFORMAT )MAKELINFMT( MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 ) ), 0, &m_pSmallFrontBufferLinear, NULL ) ) )
    {
        ATG_PrintError( "Couldn't Create Small Front Buffer Linear Texture\n" );
        return;
    }

    CHAR strBuffer[200] = {0};
    sprintf_s( strBuffer, "devkit:\\V%dw%.1f-%.1fMbps%dF_A%.1fKHz%dCh%dk.wmv" 
        ,VideoSourceFormat.biWidth
        ,VideoEncoderFormat.dwBitRate / 1000.0f
        ,VideoEncoderFormat.dwMaxBitRate / 1000.0f
        ,static_cast<INT>( VideoEncoderFormat.frameRate )
        ,AudioSourceFormat.nSamplesPerSec / 1000.0f
        ,AudioSourceFormat.nChannels
        ,AudioEncoderFormat.dwBitRate / 1000 );
    m_Encoder.Initialize( strBuffer, 
                            &AudioSourceFormat, 
                            &VideoSourceFormat, 
                            &AudioEncoderFormat, 
                            &VideoEncoderFormat, 
                            m_pPipeAPO->GeAudioPipe(),
                            m_pGraphicsPipe );

    m_dPriorTime = 0;

}

//--------------------------------------------------------------------------------------
// Name: InitializeUIElements()
// Desc: Initialize UI elements. For simplification of Initialize(), separated
//       UI initialization
//--------------------------------------------------------------------------------------
void Sample::InitializeUIElements()
{
    HRESULT hr;

    // Create shaders
    VOID* pCode = NULL;
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\PositionDiffuse.xvu", &pCode ) ) )
        ATG::FatalError( "Shader file is not found\n" );
    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pVertexShader ) ) )
        ATG::FatalError( "Shader creation error\n" );
    ATG::UnloadFile( pCode );

    // Create pixel shader
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\Diffuse.xpu", &pCode ) ) )
        ATG::FatalError( "Shader file is not found\n" );
    if( FAILED( hr = m_pd3dDevice->CreatePixelShader( ( DWORD* )pCode, &m_pPixelShader ) ) )
        ATG::FatalError( "Shader creation error\n" );
    ATG::UnloadFile( pCode );

    // Define the vertex elements.
    static const D3DVERTEXELEMENT9 VertexElements[ 3 ] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
        D3DDECL_END()
    };

    // Create a vertex declaration from the element descriptions.
    m_pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pVertexDecl );

    // Set the transform matrices
    XMVECTOR vEyePt = XMVectorSet( ( FLOAT )XMIN, 45.0f, ( FLOAT )ZMAX / 2.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( ( FLOAT )XMIN, 0.0f, ( FLOAT )ZMAX / 2.0f, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
    m_matWorld = XMMatrixIdentity();
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, 4.0f / 3.0f, 1.0f, 10000.0f );
    m_matViewProj = XMMatrixMultiply( m_matView, m_matProj );

    // Vertex shader operations use transposed matrices
    XMMATRIX mat;
    mat = XMMatrixMultiply( m_matWorld, m_matView );
    mat = XMMatrixMultiply( mat, m_matProj );
    mat = XMMatrixTranspose( mat );

    // Set the vertex shader constants
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&mat, 4 );

    // Create our vertex buffers
    m_pd3dDevice->CreateVertexBuffer( sizeof( D3DVERTEX ) * 4,
                                      0, 0, D3DPOOL_DEFAULT, &m_pvbFloor, NULL );
    m_pd3dDevice->CreateVertexBuffer( sizeof( D3DVERTEX ) * 3,
                                      0, 0, D3DPOOL_DEFAULT, &m_pvbListener, NULL );
    m_pd3dDevice->CreateVertexBuffer( sizeof( D3DVERTEX ) * 4,
                                      0, 0, D3DPOOL_DEFAULT, &m_pvbSource, NULL );
    m_pd3dDevice->CreateVertexBuffer( sizeof( D3DVERTEX ) * 2 * ( ( ZMAX - ZMIN + 1 ) + ( XMAX - XMIN + 1 ) ),
                                      0, 0, D3DPOOL_DEFAULT, &m_pvbGrid, NULL );

    // Fill the VB for the listener
    D3DVERTEX* pVertices;
    m_pvbListener->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    pVertices[ 0 ].p = XMFLOAT3( -0.5f, 0.0f, -1.0f );
    pVertices[ 0 ].c = LISTENER_COLOR;
    pVertices[ 1 ].p = XMFLOAT3( 0.0f, 0.0f, 1.0f );
    pVertices[ 1 ].c = LISTENER_COLOR;
    pVertices[ 2 ].p = XMFLOAT3( 0.5f, 0.0f, -1.0f );
    pVertices[ 2 ].c = LISTENER_COLOR;
    m_pvbListener->Unlock();

    // Fill the VB for the source
    m_pvbSource->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    pVertices[ 0 ].p = XMFLOAT3( -0.5f, 0.0f, -0.5f );
    pVertices[ 0 ].c = SOURCE_COLOR;
    pVertices[ 1 ].p = XMFLOAT3( -0.5f, 0.0f, 0.5f );
    pVertices[ 1 ].c = SOURCE_COLOR;
    pVertices[ 2 ].p = XMFLOAT3( 0.5f, 0.0f, -0.5f );
    pVertices[ 2 ].c = SOURCE_COLOR;
    pVertices[ 3 ].p = XMFLOAT3( 0.5f, 0.0f, 0.5f );
    pVertices[ 3 ].c = SOURCE_COLOR;
    m_pvbSource->Unlock();

    // Fill the VB for the floor
    m_pvbFloor->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    pVertices[ 0 ].p = XMFLOAT3( ( FLOAT )XMIN, 0.0f, ( FLOAT )ZMIN );
    pVertices[ 0 ].c = FLOOR_COLOR;
    pVertices[ 1 ].p = XMFLOAT3( ( FLOAT )XMIN, 0.0f, ( FLOAT )ZMAX );
    pVertices[ 1 ].c = FLOOR_COLOR;
    pVertices[ 2 ].p = XMFLOAT3( ( FLOAT )XMAX, 0.0f, ( FLOAT )ZMIN );
    pVertices[ 2 ].c = FLOOR_COLOR;
    pVertices[ 3 ].p = XMFLOAT3( ( FLOAT )XMAX, 0.0f, ( FLOAT )ZMAX );
    pVertices[ 3 ].c = FLOOR_COLOR;
    m_pvbFloor->Unlock();

    // Fill the VB for the grid
    INT i, j;
    m_pvbGrid->Lock( 0, 0, ( VOID** )&pVertices, 0 );
    for( i = ZMIN, j = 0; i <= ZMAX; i++, j++ )
    {
        pVertices[ j * 2 + 0 ].p = XMFLOAT3( ( FLOAT )XMIN, 0.0f, ( FLOAT )i );
        pVertices[ j * 2 + 0 ].c = GRID_COLOR;
        pVertices[ j * 2 + 1 ].p = XMFLOAT3( ( FLOAT )XMAX, 0.0f, ( FLOAT )i );
        pVertices[ j * 2 + 1 ].c = GRID_COLOR;
    }
    for( i = XMIN; i <= XMAX; i++, j++ )
    {
        pVertices[ j * 2 + 0 ].p = XMFLOAT3( ( FLOAT )i, 0.0f, ( FLOAT )ZMIN );
        pVertices[ j * 2 + 0 ].c = GRID_COLOR;
        pVertices[ j * 2 + 1 ].p = XMFLOAT3( ( FLOAT )i, 0.0f, ( FLOAT )ZMAX );
        pVertices[ j * 2 + 1 ].c = GRID_COLOR;
    }
    m_pvbGrid->Unlock();
}

//--------------------------------------------------------------------------------------
// Name: ChangeMenu()
// Desc: Scrolls the currently selected menu right or left.
//--------------------------------------------------------------------------------------
VOID Sample::ChangeMenu( BOOL MoveRight )
{

    int iCurrentMenu = static_cast<int>( m_CurrentMenu );
    iCurrentMenu = ( ( ( MoveRight ) ? 1 : -1 ) + MenuItemCount + iCurrentMenu ) % MenuItemCount;

    m_bShowingMenu1 = iCurrentMenu < MenuSplit;
    m_CurrentMenu = static_cast<Menu>( iCurrentMenu );
}

//--------------------------------------------------------------------------------------
// Name: ChangeMenuItem()
// Desc: Scrolls the currently selected menu item up or down.
//--------------------------------------------------------------------------------------
VOID Sample::ChangeMenuItem( INT32 Size, BOOL Increment, UINT32 *CurrentValue )
{
    *CurrentValue = ( *CurrentValue + ( Increment ? 1 : -1 ) + Size ) % ( Size );
}

//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    DWORD dwPulse = DWORD( ( cosf( ( FLOAT )m_Timer.GetAppTime() * 3.0f ) + 1.f ) * 80.f );
    m_dwBlend = dwPulse | ( dwPulse << 8 ) | ( dwPulse << 16 );

    // Update the internal state of the ASF Writer Helper class. Necessary when calling StopAsync().
    if( m_Encoder.IsStopping() )
        m_Encoder.Update();

    // Jump to next WMA rate list.
    if( !m_Encoder.IsRecording() )
    {
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT || pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        {
            ChangeMenu( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT );

        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        {
            m_bShowingMenu1 = !m_bShowingMenu1;
            m_CurrentMenu = m_bShowingMenu1 ? MenuAudioSampleRate : MenuVideoQuality;
        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN || pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        {
            BOOL bIncrement = 0 != ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN ) ;
            switch( m_CurrentMenu )
            {
                case MenuAudioSampleRate:
                {
                    ChangeMenuItem( WMAOPTIONS_SIZE, bIncrement, &m_iWMARateListIndex );
                    m_iWMARateIndex = 0;
                    break;
                }
                case MenuAudioBitRate:
                {
                    ChangeMenuItem( WMAOPTIONS[m_iWMARateListIndex].nEntries, bIncrement, &m_iWMARateIndex );
                    break;
                }
                case MenuVideoResolution:
                {
                    if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN ) && m_iVideoScaleFactor < 4 )
                    {
                        m_iVideoScaleFactor= m_iVideoScaleFactor << 1;
                    }
                    if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP ) && m_iVideoScaleFactor > 1 )
                    {
                        m_iVideoScaleFactor = m_iVideoScaleFactor >> 1;
                    }
                    // It's difficult to keep up with real-time encoding at 640x360 & 60fps, so don't provide the option
                    if( m_iVideoScaleFactor <= 2 && VIDEO_FRAME_RATES[m_iVideoFrameRateIndex] > 30 )
                        m_iVideoScaleFactor = 4;
                    break;
                }
                case MenuVideoBitRateMin:
                {
                    ChangeMenuItem( m_iVideoBitrateMaxIndex + 1, bIncrement, &m_iVideoBitrateMinIndex );
                    break;
                }
                case MenuVideoBitRateMax:
                {
                    ChangeMenuItem( VIDEO_BITRATES_SIZE, bIncrement, &m_iVideoBitrateMaxIndex );
                    if (m_iVideoBitrateMaxIndex < m_iVideoBitrateMinIndex)
                        m_iVideoBitrateMinIndex = m_iVideoBitrateMaxIndex;
                    break;
                }
                case MenuVideoQuality:
                {
                    ChangeMenuItem( VIDEO_QUALITIES_SIZE, bIncrement, &m_iVideoQualityIndex );
                    break;
                }
                case MenuVideoFramerate:
                {
                    ChangeMenuItem( VIDEO_FRAME_RATES_SIZE, bIncrement, &m_iVideoFrameRateIndex );

                    // It's difficult to keep up with real-time encoding at 640x360 & 60fps, so don't provide the option
                    if( m_iVideoScaleFactor <= 2 && VIDEO_FRAME_RATES[m_iVideoFrameRateIndex] > 30 )
                    {
                        if (bIncrement)
                            m_iVideoFrameRateIndex = 0;
                        else
                            m_iVideoFrameRateIndex = 2; // Cap at 30fps
                    }

                    break;
                }
            }
        }
    }

    // Toggle recording on or off.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        if( !m_Encoder.IsRecording() )
        {
            // Ensure the sample rates are up-to-date, and initialize the encoder.
            ConfigureSampleRate(); 
            InitializeEncoder();
            m_pSubmixVoicePipe->EnableEffect( 0 );
            m_Encoder.Start();
        }
        else
        {
            m_pSubmixVoicePipe->DisableEffect( 0 );
            m_Encoder.StopAsync();

        }
    }

    // Set up our colors
    m_dwSourceColor = SOURCE_COLOR | m_dwBlend;
    m_dwListenerColor = LISTENER_COLOR;

    // Point to the appropriate vector
    XMVECTOR* pvControl = &m_vSourcePosition;

    // Move selected object and clamp to the appropriate range
    pvControl->x += pGamepad->fX1 * fElapsedTime * MOTION_SCALE;
    if( pvControl->x < XMIN )
        pvControl->x = XMIN;
    else if( pvControl->x > XMAX )
        pvControl->x = XMAX;

    pvControl->z += pGamepad->fY1 * fElapsedTime * MOTION_SCALE;
    if( pvControl->z < ZMIN )
        pvControl->z = ZMIN;
    else if( pvControl->z > ZMAX )
        pvControl->z = ZMAX;


    SetVolume();
    
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: SetVolume()
// Desc: Sets the volume of the helicopter from the listening point.
//       Extremely simplistic; see XAudio2Sound3D for proper positioning code.
//--------------------------------------------------------------------------------------
HRESULT Sample::SetVolume()
{
    // Create a master volume to scale everything by distance.
    float fMasterVolume = 1.0f - sqrtf( pow( m_vSourcePosition.x,2 ) + pow( m_vSourcePosition.z ,2 ) ) / 10.0f;
    fMasterVolume = max( VOLUME_MIN, fMasterVolume );
    fMasterVolume = min( VOLUME_MAX, fMasterVolume );

    XMVECTOR vNormalized = ( ( XMVector3Normalize( m_vSourcePosition ) + XMVectorSplatOne() ) / 2.0f ) ;
    float Right  = vNormalized.x;
    float Left = 1.0f - Right;

    // Set the voice volumes.
    float fOutputMatrix[6] = {Left * fMasterVolume,  Right * fMasterVolume, 0, 0, 0, 0 };
    m_pSourceVoice->SetOutputMatrix( m_pMasteringVoice, 1, 6, fOutputMatrix );
    if( m_pSubmixVoiceResampler != NULL )
    {
        m_pSourceVoice->SetOutputMatrix( m_pSubmixVoiceResampler, 1, 2, fOutputMatrix );
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DrawMenuHeader()
// Desc: Displays the menu header for a column of options.
//--------------------------------------------------------------------------------------
VOID Sample::DrawMenuHeader( FLOAT X, FLOAT *Y, BOOL IsCurrent, WCHAR *Text, FLOAT *MaxWidth, FLOAT *Height )
{
    static const WCHAR strUnderline[] = L"***********************************";
    WCHAR strUnderlineBuffer[200] = {0};
    *MaxWidth = 0.0f;
    *Y = 67.0f;

    m_Font.SetScaleFactors( 1.1f, 1.1f );
    m_Font.DrawText( X, *Y, 0xffffffff, Text );
    *MaxWidth = max( *MaxWidth, m_Font.GetTextWidth( Text ) );
    *Height = m_Font.GetFontHeight();
    *Y += *Height;

    if( IsCurrent )
    {
        int nCount = 0;
        while ( m_Font.GetTextWidth( strUnderlineBuffer ) < *MaxWidth )
            swprintf_s( strUnderlineBuffer, L"%.*s", ++nCount, strUnderline );
        m_Font.DrawText( X, *Y, 0xffffff00 | m_dwBlend, strUnderlineBuffer );
    }
    *Y += *Height ;
}


//--------------------------------------------------------------------------------------
// Name: DrawMenuItem()
// Desc: Displays the menu item for an option.
//--------------------------------------------------------------------------------------
VOID Sample::DrawMenuItem( FLOAT X, FLOAT *Y, BOOL IsCurrent, WCHAR *Text, FLOAT *MaxWidth, FLOAT *Height )
{
    m_Font.SetScaleFactors( 0.9f, 0.9f );
    m_Font.DrawText( X, *Y, IsCurrent ? 0xffffff00 : 0xffffffff, Text );
    *MaxWidth = max( *MaxWidth, m_Font.GetTextWidth( Text ) );
    *Height = m_Font.GetFontHeight();
    *Y += *Height ;
}

//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    // Set default render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    // Vertex shader operations use transposed matrices
    XMMATRIX mat;
    mat = XMMatrixMultiply( m_matWorld, m_matViewProj );
    mat = XMMatrixTranspose( mat );
    // Set the vertex shader constants
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&mat, 4 );

    // Set the vertex shader
    m_pd3dDevice->SetVertexShader( m_pVertexShader );

    // Set the vertex declaration.
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );

    // Set the pixel shader
    m_pd3dDevice->SetPixelShader( m_pPixelShader );

    // Draw the floor
    m_pd3dDevice->SetStreamSource( 0, m_pvbFloor, 0, sizeof( D3DVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

    // Draw the grid
    m_pd3dDevice->SetStreamSource( 0, m_pvbGrid, 0, sizeof( D3DVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_LINELIST, 0, 2 * ( ( ZMAX - ZMIN + 1 ) + ( XMAX - XMIN + 1 ) ) );

    // Draw the Listener
    {
        mat = XMMatrixTranslation( m_vListenerPosition.x,
                                   m_vListenerPosition.y,
                                   m_vListenerPosition.z );
        mat = XMMatrixMultiply( mat, m_matViewProj );
        mat = XMMatrixTranspose( mat );

        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&mat, 4 );
        m_pd3dDevice->SetStreamSource( 0, m_pvbListener, 0, sizeof( D3DVERTEX ) );
        m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 1 );
    }

    // Draw the Source
    {
        XMMATRIX matTrans = XMMatrixTranslation( m_vSourcePosition.x,
                                                 m_vSourcePosition.y,
                                                 m_vSourcePosition.z );
        XMMATRIX matRotate = XMMatrixRotationY( 0.0f );
        mat = XMMatrixMultiply( matRotate, matTrans );
        mat = XMMatrixMultiply( mat, m_matViewProj );
        mat = XMMatrixTranspose( mat );

        m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&mat, 4 );
        m_pd3dDevice->SetStreamSource( 0, m_pvbSource, 0, sizeof( D3DVERTEX ) );
        m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );
    }

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        WCHAR strBuffer[200];

        // Show title
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"ASF Writer" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );


        // Show status
        if( !m_Encoder.IsRecording() )
        {
            float X = 16.0f;
            float Y = 100.0f;
            float MaxWidth = 0.0f;
            float Height = 0.0f;

            if( m_bShowingMenu1 )
            {
                DrawMenuHeader( X, &Y, m_CurrentMenu == MenuAudioSampleRate, L"Sample Rate", &MaxWidth, &Height );
                for( UINT32 i = 0; i < WMAOPTIONS_SIZE ; i++ )
                {
                    swprintf_s( strBuffer, L"%6dHz  %dCh", WMAOPTIONS[i].nSamplingRate, WMAOPTIONS[i].nChannels );
                    DrawMenuItem( X, &Y, ( i == m_iWMARateListIndex ), strBuffer, &MaxWidth, &Height );
                }
                X += MaxWidth + 30.0f;

                DrawMenuHeader( X, &Y, m_CurrentMenu == MenuAudioBitRate, L"Audio Bitrate", &MaxWidth, &Height );
                for( UINT32 i = 0; i < WMAOPTIONS[m_iWMARateListIndex].nEntries; i++ )
                {
                    swprintf_s( strBuffer, L"%6d Bitrate", WMAOPTIONS[m_iWMARateListIndex].pRates[i].nBitrate );
                    DrawMenuItem( X, &Y, ( i == m_iWMARateIndex ), strBuffer, &MaxWidth, &Height );
                }
                X += MaxWidth + 30.0f;

                DrawMenuHeader( X, &Y, m_CurrentMenu == MenuVideoBitRateMin, L"Minimum Video Bitrate", &MaxWidth, &Height );
                for( UINT32 i = 0; i < VIDEO_BITRATES_SIZE; i++ )
                {
                    // We only show Minimum bitrates that are less than or equal to the current max bitrate.
                    if( i <= m_iVideoBitrateMaxIndex )
                    {
                        swprintf_s( strBuffer, L"%6d", VIDEO_BITRATES[i] );
                        DrawMenuItem( X, &Y, ( i == m_iVideoBitrateMinIndex ), strBuffer, &MaxWidth, &Height );
                    }
                }
                X += MaxWidth + 30.0f;

                DrawMenuHeader( X, &Y, m_CurrentMenu == MenuVideoBitRateMax, L"Maximum Video Bitrate", &MaxWidth, &Height );
                for( UINT32 i = 0; i < VIDEO_BITRATES_SIZE; i++ )
                {
                    swprintf_s( strBuffer, L"%6d", VIDEO_BITRATES[i] );
                    DrawMenuItem( X, &Y, ( i == m_iVideoBitrateMaxIndex ), strBuffer, &MaxWidth, &Height );
                }
                X += MaxWidth + 30.0f;
            }
            else
            {   
                // Showing second menu

                DrawMenuHeader( X, &Y, m_CurrentMenu == MenuVideoQuality, L"Quality", &MaxWidth, &Height );
                for( UINT32 i = 0; i < VIDEO_QUALITIES_SIZE; i++ )
                {
                    swprintf_s( strBuffer, L"%6d", VIDEO_QUALITIES[i] );
                    DrawMenuItem( X, &Y, ( i == m_iVideoQualityIndex ), strBuffer, &MaxWidth, &Height );
                }
                X += MaxWidth + 30.0f;

                DrawMenuHeader( X, &Y, m_CurrentMenu == MenuVideoResolution, L"Resolution", &MaxWidth, &Height );
                for( int i = 0; i < 3; i++ )
                {
                    // It's difficult to keep up with real-time encoding at 640x360 & 60fps, so don't provide the option
                    if( i < 2 && VIDEO_FRAME_RATES[m_iVideoFrameRateIndex] > 30 )
                        continue;

                    swprintf_s( strBuffer, L"%dx%d", m_d3dpp.BackBufferWidth / ( 1<<i ), m_d3dpp.BackBufferHeight / ( 1<<i ) );
                    DrawMenuItem( X, &Y, ( ( 1<<i ) == m_iVideoScaleFactor ), strBuffer, &MaxWidth, &Height );
                }
                X += MaxWidth + 30.0f;

                DrawMenuHeader( X, &Y, m_CurrentMenu == MenuVideoFramerate, L"Framerate", &MaxWidth, &Height );
                for( UINT32 i = 0; i < VIDEO_FRAME_RATES_SIZE; i++ )
                {
                    // It's difficult to keep up with real-time encoding at 640x360 & 60fps, so don't provide the option
                    if( m_iVideoScaleFactor <= 2 && VIDEO_FRAME_RATES[i] > 30 )
                        continue;

                    swprintf_s( strBuffer, L"%6d", VIDEO_FRAME_RATES[i] );
                    DrawMenuItem( X, &Y, ( i == m_iVideoFrameRateIndex ), strBuffer, &MaxWidth, &Height );
                }
                X += MaxWidth + 30.0f;

            }
        }
        else
        {
            if( m_Encoder.IsStopping() )
            {
                m_Font.SetScaleFactors( 1.5f, 1.5f );
                m_Font.DrawText( 200.0f, 0, 0xffff0000 | m_dwBlend, L"Stopping" );
            }
            else
            {
                m_Font.SetScaleFactors( 1.5f, 1.5f );
                m_Font.DrawText( 200.0f, 0, 0xffff0000 | m_dwBlend, L"Recording" );
            }
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    // Video
    if( m_Encoder.IsRecording() && !m_Encoder.IsStopping() )
    {
        D3DTexture* pFrontBuffer = NULL;
        D3DLOCKED_RECT tiled_rect;
        D3DLOCKED_RECT linear_rect;
        m_pd3dDevice->GetFrontBuffer( &pFrontBuffer );
        if( m_iVideoScaleFactor > 1 )
        {
            if( m_iVideoScaleFactor == 2 )
                m_PostProcess.Downsample2x2Texture( pFrontBuffer, m_pSmallFrontBuffer );
            else if( m_iVideoScaleFactor == 4 )
                m_PostProcess.Downsample4x4Texture( pFrontBuffer, m_pSmallFrontBuffer );
        
            

            // Ideally you would wait 2 frames before locking and retrieving the results ot use an async resource read
            m_pSmallFrontBuffer->LockRect( 0, &tiled_rect, NULL, D3DLOCK_READONLY );
            m_pSmallFrontBufferLinear->LockRect( 0, &linear_rect, NULL, 0 );

            // Better technique is to use the FastUntil sample
            // Data will now be in linear_rect.pBits
            XGUntileSurface( linear_rect.pBits, linear_rect.Pitch, NULL, tiled_rect.pBits, m_dwVideoWidth, m_dwVideoHeight, NULL, 4 );
        }
        else
        {
            // Ideally you would wait 2 frames before locking and retrieving the results ot use an async resource read
            pFrontBuffer->LockRect( 0, &tiled_rect, NULL, D3DLOCK_READONLY );
            m_pSmallFrontBufferLinear->LockRect( 0, &linear_rect, NULL, 0 );
            XGUntileSurface( linear_rect.pBits, linear_rect.Pitch, NULL, tiled_rect.pBits, m_dwVideoWidth, m_dwVideoHeight, NULL, 4 );
        }
        
        DOUBLE dCurrentTime = m_Timer.GetAbsoluteTime();
        if( m_dPriorTime == 0 )
            m_dPriorTime = dCurrentTime;

        //
        // Convert the frame and add to the buffer if the frame is needed for the
        // encoding video frame rate. The encoder will not throw away extra frames.
        // Note that this is a simplistic test, and more advanced heuristics would 
        // reduce jitter when the display framerate and recording framerate only 
        // slightly off.
        //

        if( 1.0f / VIDEO_FRAME_RATES[m_iVideoFrameRateIndex] <= ( dCurrentTime-m_dPriorTime ) )
        {
            // Check to see if the buffer is currently full. If it is, we will try again next frame.
            DWORD dwSizeOfRemainingSpace = ( ( 1 << XMVENCODER_GRAPHICS_PIPE_LENGTH ) - m_pGraphicsPipe->BytesAvailable() );
            DWORD dwSizeOfSingleFrame = sizeof( LONGLONG ) + linear_rect.Pitch * m_dwVideoHeight;
            if( dwSizeOfRemainingSpace < dwSizeOfSingleFrame )
            {
                if( !m_bVideoPipeSaturation )
                    ATG::DebugSpew( "Video pipe is experiencing saturation. Encoded video may become choppy.\n" );
                m_bVideoPipeSaturation = TRUE;
            }
            else
            {
                //
                // Write out the frame time to the stream. 
                //
                // The encoder requests one frame, and the time to wait before displaying it, 
                // so we prepend the time duration to the buffer in 100ns increments.
                //
                // This is extracted by the sample's IAsfWriterVideoStream::GetNextFrame implementation,
                // and returned to the encoder using the parameter phnsSampleTime.
                //

                LONGLONG FrameTime = static_cast<LONGLONG>( ( dCurrentTime - m_dPriorTime ) *10000000 );
                m_dPriorTime = dCurrentTime;
                if( !m_pGraphicsPipe->Write( &FrameTime, sizeof( LONGLONG ) ) )
                    ATG::FatalError( "Video pipe was unexpectedly full.\n" );

                // Write out the video data to the stream.
                if( !m_pGraphicsPipe->Write( linear_rect.pBits, linear_rect.Pitch * m_dwVideoHeight ) )
                    ATG::FatalError( "Video pipe was unexpectedly full.\n" );
                
                m_bVideoPipeSaturation = FALSE;
            }
        }
        m_pSmallFrontBufferLinear->UnlockRect( 0 );
        if (m_iVideoScaleFactor > 1)
            m_pSmallFrontBuffer->UnlockRect( 0 );
        else
            pFrontBuffer->UnlockRect( 0 );
    }
    
    return S_OK;
}

