//--------------------------------------------------------------------------------------
// ASFWriterHelper.cpp
//
// The helper class for the ASFWriter sample. This class initializes, starts, and stops
// the ASF Writer in a manner that supports encoding live content. For encoding cached
// content, several different choices may be made (see the documentation for details).
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "ASFWriterHelper.h"

//--------------------------------------------------------------------------------------
// Name: ASFWriterHelper::ASFWriterHelper
// Desc: Default constructor for the ASFWriterHelper class.
//--------------------------------------------------------------------------------------
ASFWriterHelper::ASFWriterHelper():m_pAsfWriter( NULL ), 
                                   m_pwSingleFrame( 0 ),
                                   m_bRecording( 0 ),
                                   m_bStopping( 0 ),
                                   m_hnsLatestTimeStamp( 0 ),
                                   m_AudioStreamStopped( 0 ),
                                   m_VideoStreamStopped( 0 ),
                                   m_bInitialized( 0 ) {}

//--------------------------------------------------------------------------------------
// Name: ASFWriterHelper::Initialize
// Desc: Creates a new ASFWriter and initializes parameters for each recording session.
//--------------------------------------------------------------------------------------
HRESULT ASFWriterHelper::Initialize( __in const char*  pszDestinationFileName, 
__in const WAVEFORMATEX* pAudioSourceFormat, 
__in const AsfWriterBitmapInfoHeader* pVideoSourceFormat,
__in const AsfWriterAudioEncInfo* pAudioEncoderFormat, 
__in const AsfWriterVideoEncInfo* pVideoEncoderFormat,
__in AudioPipe* pAudioPipe,
__in XMVEncoderGraphicsPipe* pGraphicsPipe )
{
    HRESULT hr = S_OK;


    if( m_pAsfWriter )
    {
        if( m_bRecording )
            ATG::FatalError( "You must stop the stream cleanly before starting a new recording." );
        CloseHandle( m_ASFHandle.hEvent );
        DestroyAsfWriter( m_pAsfWriter );
        ZeroMemory( &m_ASFHandle, sizeof( m_ASFHandle ) );
        delete m_pwSingleFrame;
    }

    m_pAudioPipe = pAudioPipe;
    m_pGraphicsPipe = pGraphicsPipe;
    m_hnsLatestTimeStamp = 0;
    m_bRecording = FALSE;
    m_AudioStreamStopped = FALSE;
    m_VideoStreamStopped = FALSE;
    m_bStopping = FALSE;
    
    m_pwSingleFrame = new BYTE[MAX_FRAME_BUFFERSIZE];

    m_ASFHandle.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

    hr = CreateAsfWriter( pszDestinationFileName, &m_pAsfWriter );
    if( FAILED( hr ) )
        ATG::FatalError( "Unable to create the ASF Writer.\n" );


    hr = m_pAsfWriter->SetAudioInput( this, pAudioSourceFormat, pAudioEncoderFormat );
    if( FAILED( hr ) )
        ATG::FatalError( "Audio definitions were incorrect.\n" );

    hr = m_pAsfWriter->SetVideoInput( this, pVideoSourceFormat, pVideoEncoderFormat );
    if( FAILED( hr ) )
        ATG::FatalError( "Video definitions were incorrect.\n" );

    // Set thread priorities now that the inputs have been created.
    AsfWriterPropertyValue ThreadAffinity;
    ThreadAffinity.Clear();
    ThreadAffinity.m_Type = ThreadAffinity.UINT32Property;
    ThreadAffinity.m_Value.uInt32Value = 2;
    hr = m_pAsfWriter->SetProperty( AsfWriterThreadProcessorMux, &ThreadAffinity );
    ThreadAffinity.m_Value.uInt32Value = 3;
    hr = m_pAsfWriter->SetProperty( AsfWriterThreadProcessorAudio, &ThreadAffinity );
    ThreadAffinity.m_Value.uInt32Value = 4;
    hr = m_pAsfWriter->SetProperty( AsfWriterThreadProcessorVideo, &ThreadAffinity );

    m_bInitialized = TRUE;
    return hr;
}

//--------------------------------------------------------------------------------------
// Name: ASFWriterHelper::~ASFWriterHelper
// Desc: Destructor for the ASFWriterHelper. Destroys the ASFWriter and cleans up memory.
//--------------------------------------------------------------------------------------
ASFWriterHelper::~ASFWriterHelper()
{
    DestroyAsfWriter( m_pAsfWriter );
}

//--------------------------------------------------------------------------------------
// Name: ASFWriterHelper::GetNextFrame
// Desc: Callback from the ASFWriter to retrieve a single video frame. 
//       Handles waiting for data if the buffers are starved, and End Of Stream scenarios.
//--------------------------------------------------------------------------------------
HRESULT ASFWriterHelper::GetNextFrame( __out_ecount(*puLength) BYTE* pFrame,
                                 __inout UINT* puLength,
                                 __out LONGLONG* phnsSampleTime )
{
    UINT Length = *puLength;
    *puLength = 0; 
    BOOL bFrameRetrieved = TRUE;

    // Read in the timestamp and frame data
    while( m_bRecording && !m_pGraphicsPipe->Read( m_pwSingleFrame, sizeof( LONGLONG ) + Length ) )
    {
        if( !m_bStopping ){
            Yield();
        }
        else{
            bFrameRetrieved = FALSE;
            break;
        }
    }

    if( bFrameRetrieved )
    {
        if( MAX_FRAME_BUFFERSIZE < Length )
            ATG::FatalError( "Frame buffer size was too small for the requested frame.\n" );
        *puLength = Length;
        ATG::DebugSpew( "Handing off %d bytes of video\n",Length );
        
        // Copy the frame portion of the buffer to pFrame
        memcpy( pFrame, m_pwSingleFrame + sizeof(LONGLONG), Length ); 

        m_hnsLatestTimeStamp += *( LONGLONG* )m_pwSingleFrame;
        *phnsSampleTime = m_hnsLatestTimeStamp;
    }
    else
    {
        ATG::DebugSpew( "Stopping video stream.\n" );
        *puLength = 0; // A value of 0 indicates that the stream is finished.
        m_VideoStreamStopped = TRUE;
    }
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: ASFWriterHelper::GetNextSample
// Desc: Callback from the ASFWriter to retrieve audio buffers. 
//       Handles waiting for data if the buffers are starved, and End Of Stream scenarios.
//--------------------------------------------------------------------------------------
HRESULT ASFWriterHelper::GetNextSample( __out_ecount(*puLength) BYTE* pSample,
                      __inout UINT* puLength )
{
    UINT Length = *puLength;

    if( m_AudioStreamStopped ) 
    {
        *puLength = 0;
        return S_OK;
    }

    while( m_bRecording && !m_pAudioPipe->Read( pSample, Length ) )
    {
        if( !m_bStopping )
        {
            Yield();
        }
        else
        {
            // If the encoding is shutting down, retrieve the audio bytes that remain.
            Length = m_pAudioPipe->BytesAvailable();
            m_pAudioPipe->Read( pSample, Length );
            m_AudioStreamStopped = TRUE;
            break;
        }
    }

    *puLength = Length;
    ATG::DebugSpew( "Handing off %d bytes of PCM\n", Length );
    
    if( Length == 0 )
        ATG::DebugSpew( "Stopping audio stream.\n" ); 

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: ASFWriterHelper::Stop
// Desc: Immediately stops the ASFWriter. This may result in a file that does not contain
//       all of the submitted data.
//--------------------------------------------------------------------------------------
HRESULT ASFWriterHelper::Stop()
{
    HRESULT hr = S_OK;

    hr = m_pAsfWriter->Stop();
    if( FAILED( hr ) )
        ATG::FatalError( "Unable to stop the ASF Writer.\n" );

    while( m_ASFHandle.InternalLow == ERROR_IO_PENDING )
        Yield();

    if( m_ASFHandle.InternalLow == ERROR_SUCCESS )
    {
        ATG::DebugSpew( "ASF stream successfully closed.\n" );
        hr = S_OK;
    }
    else
    {
        ATG::DebugSpew( "An error occurred closing the ASF stream.\n" );
        hr = E_FAIL;
    }
    
    m_bRecording = FALSE;
    m_bStopping = FALSE;
    return hr;
}

//--------------------------------------------------------------------------------------
// Name: ASFWriterHelper::StopAsync
// Desc: Asynchronously stops the ASF Writer, allowing it to finish encoding both streams.
//       When using this method, Update() must be called so that the helper can be  
//       notified of stream completion.
//--------------------------------------------------------------------------------------
HRESULT ASFWriterHelper::StopAsync()
{
    HRESULT hr = S_OK;

    //
    // By setting the m_bStopping flag to TRUE, the audio encoder will hand off a partial audio buffer.
    // this, in turn, will be interpreted by the ASF Writer API as the end of the file. It will
    // then continue streaming the rest of the video frames until that queue is emptied.
    // 
    m_bStopping = TRUE;
    return hr;
}

//--------------------------------------------------------------------------------------
// Name: ASFWriterHelper::Update
// Desc: Updates internal state based on the ASFWriter. Necessary when calling StopAsync().
//--------------------------------------------------------------------------------------
VOID ASFWriterHelper::Update()
{
    if( m_ASFHandle.InternalLow == ERROR_IO_PENDING )
        return;
    
    if( m_bStopping )
    {
        if( m_ASFHandle.InternalLow == ERROR_SUCCESS )
            ATG::DebugSpew( "ASF stream successfully closed.\n" );
        else
            ATG::DebugSpew( "An error occurred closing the ASF stream.\n" );
    }
    m_bStopping = FALSE;
    m_bRecording = FALSE;
}

//--------------------------------------------------------------------------------------
// Name: ASFWriterHelper::Start
// Desc: Starts the ASFWriter component.
//--------------------------------------------------------------------------------------
HRESULT ASFWriterHelper::Start() 
{
    if( m_bRecording )
        ATG::FatalError( "XMV Encoder is already started.\n" );
    if( !m_bInitialized )
        ATG::FatalError( "The ASF Writer helper class must first be initialized.\n" );

    HRESULT hr = S_OK;
    m_bRecording = TRUE;
    m_bStopping = FALSE;

    hr = m_pAsfWriter->Start( &m_ASFHandle ); 
    if( hr == E_PENDING )
    {
        hr = S_OK;
    }
    else
    {
        ATG::FatalError( "Error when starting the ASF Writer.\n" );
    }

    if( FAILED( hr ) )
        m_bRecording = FALSE;
    return hr;
}