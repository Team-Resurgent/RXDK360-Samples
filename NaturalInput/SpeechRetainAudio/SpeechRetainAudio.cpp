//--------------------------------------------------------------------------------------
// NuiSpeech Retain Audio Sample
//
//  Demonstrates use of the Retain Audio ability of the NuiSpeech API
// 
//  Usage:
//  Plug in a Kinect sensory array
//  and start the sample.  Retained Audio is stored in DEVKIT:/RETAINED_AUDIO
//
//    Sample will calls the following APIs to initialize:
//    NuiSpeechEnable
//    NuiSpeechSetEventInterest
//    NuiSpeechLoadGrammar
//
//    The update loop calls the following:
//    NuiSpeechStartRecognition
//    NuiSpeechGetEvents
//    NuiSpeechStopRecognition
//    GetEvents will ultimately display the recognized phrase on screen.
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xbdm.h>
#include <AtgAudio.h>
#include <AtgInput.h>
#include <AtgConsole.h>
#include <AtgUtil.h>
#include <NuiApi.h>
#include <deque>

//startup grammar
#define STARTUP_GRAMMAR L"game:\\Media\\Grammars\\medieval_en_us.cfg"

// Path to the font to use for the app's console
#define FONT_FILE_NAME "game:\\Media\\Fonts\\Arial_16_Speech.xpr"

// Path to the folder used for retaining audio samples
#define RETAIN_AUDIO_PATH "DEVKIT:\\RETAIN_AUDIO"

// Delimiter used for the metadata format
#define METADATA_DELIMITER L"|"

// Byte order mark header for Unicode
#define BOM_HEADER "\xFE\xFF"

// Arbitrary internal ID to assign the loaded grammar file. 
static const ULONG LOADED_GRAMMAR_ID = 55;

// ID value we use when no grammar is loaded.
static const ULONG NO_GRAMMAR_LOADED = 0;

// WAVE infromation
static const int WAVE_NUM_OF_CHANNELS = 1;
static const int WAVE_SAMPLE_RATE = 16000;
static const int WAVE_AVG_BYTES_PER_SEC = 32000;
static const int WAVE_BLOCK_ALIGN = 2;
static const int WAVE_BITS_PER_SAMPLE = 16;
static const int SIZE_OF_CHUNK_BEFORE_DATA = 36;

// Number of retain audio events to capture
static const int RETAIN_AUDIO_BUFFER_SIZE = 5;
static const int RETAIN_AUDIO_BUFFER_BYTE_SIZE = RETAIN_AUDIO_BUFFER_SIZE * WAVE_AVG_BYTES_PER_SEC;

// 1000ms of audio buffer before and after SOUND events
static const int EXTRA_AUDIO_BYTES = WAVE_AVG_BYTES_PER_SEC * 1;  


// WAVE File information
const DWORD RIFF = 'RIFF';
const DWORD WAVE = 'WAVE';
const DWORD FORMAT = 'fmt ';
const DWORD DATA = 'data';

typedef struct _RIFF_DATA
{
    DWORD chunkID;
    DWORD chunkSize;
    DWORD format;
    DWORD subchunk1ID;
    DWORD subchunk1Size;
    PCMWAVEFORMAT pcmWaveFormat;
    DWORD subchunk2ID;
    DWORD subchunk2Size;    
} RIFF_DATA;

typedef struct _AUDIO_CAPTURE
{
    ULONGLONG startOffset;              // SOUND_START OFFSET
    ULONGLONG endOffset;                // SOUND_END OFFSET
    ULONGLONG ullStartTime;             // RECO OFFSET
    BYTE* pLastSerializedState;         // Serialized State from previous SOUND_END
    ULONG ulLastSerializedStateSize;    // Serialized State size from previous SOUND_END
    ULONGLONG ullLastSoundEndOffset;    // Previous SOUND_END offset    
} AUDIO_CAPTURE;

typedef struct _RETAINED_AUDIO
{
    BYTE* pAudioData;
    ULONG ulAudioSize;   
    ULONGLONG ullAudioStreamOffset;
} RETAINED_AUDIO;

//--------------------------------------------------------------------------------------
// Name: class SpeechRetainApp
// Desc: Class for the application SpeechRetainAudio.
//--------------------------------------------------------------------------------------

class SpeechRetainApp {
public:
    SpeechRetainApp();
    ~SpeechRetainApp() { TermApp(); }

    HRESULT CreateNuiSpeech();
    VOID TermApp();
    HRESULT StartListening();
    HRESULT StopListening();
    HRESULT ToggleListening();    
    VOID ProcessRecognitionEvent( const NUI_SPEECH_EVENT* pEvent );
   
    HRESULT ReloadGrammarFile();
    
    HRESULT Update();    
    static BOOL IsMicrophoneConnected(void);
private:
    VOID InitConsole();
    ATG::Console m_Console;                // TTY Display for demo purposes    
    HANDLE m_hMetadata;

    // Speech Recognition state

    BOOL m_speechActivate;
    ULONG m_loadedGrammarID;
    LPCWSTR m_loadedGrammarPath;
    NUI_SPEECH_GRAMMAR m_Grammar;                 // Grammar instance

    VOID AddToAudioBuffer( NUI_SPEECH_EVENT* pEvent );
    VOID ClearAudioBuffer();
    VOID WriteAudio( ULONGLONG start, ULONGLONG end, ULONGLONG ullAudioStreamOffset );   
    VOID WriteSerializedState( BYTE* pLastSerializedState, ULONG ulLastSerializedStateSize, ULONGLONG ullAudioStreamOffset );   
    VOID WriteMetadata( const NUI_SPEECH_RECORESULT* pRR );
    VOID GetAudioFromStreamRange( ULONGLONG ullStart, ULONGLONG ullEnd, BYTE* pAudio, DWORD dwAudioSize, DWORD* pdwAudioWritten );
                
    BYTE *m_pLastSerializedState;
    ULONG m_ulLastSerializedStateSize;
    ULONGLONG m_ullLastSoundEndOffset;

    std::deque<AUDIO_CAPTURE> m_captureAudioQueue;    
    std::deque<RETAINED_AUDIO> m_retainedAudioQueue;   
};

//--------------------------------------------------------------------------------------
// Name: SpeechRetainApp()
// Desc: Initializes the app and starts up the console
//--------------------------------------------------------------------------------------

SpeechRetainApp::SpeechRetainApp() :
    m_loadedGrammarID( NO_GRAMMAR_LOADED ),
    m_loadedGrammarPath( NULL ),
    m_hMetadata( NULL ),
    m_pLastSerializedState( NULL ),
    m_ulLastSerializedStateSize( 0 ),
    m_ullLastSoundEndOffset( 0 )
{
    InitConsole();    
}

//--------------------------------------------------------------------------------------
// Name: VOID InitConsole()
// Desc: Initializes the ATG::Console, only used internally for startup
//--------------------------------------------------------------------------------------

VOID SpeechRetainApp::InitConsole()
{
    //
    // Initialize the console window
    //
    m_Console.Create( FONT_FILE_NAME, 0xFF1F005F, 0xFFFFFFFF );
    m_Console.SendOutputToDebugChannel( TRUE );
}

//--------------------------------------------------------------------------------------
// Name: void TermApp()
// Desc: Frees metadat file
//--------------------------------------------------------------------------------------
VOID SpeechRetainApp::TermApp()
{
    if( m_hMetadata ) 
    {
        CloseHandle( m_hMetadata );
        m_hMetadata = NULL;
    }
}

//--------------------------------------------------------------------------------------
// Name: CreateNuiSpeech()
// Desc: Initialize the NuiSpeech subsystem.
// Return NOERROR on successful initialization or the Nui/XDK
// HRESULT error if a failure.
//--------------------------------------------------------------------------------------

HRESULT SpeechRetainApp::CreateNuiSpeech()
{
    return StartListening();
}

//----------------------------------------------------------------------------------------------------------------------
// Name: SpeechRetainApp::StartListening
// Desc: Starts the Speech Engine and puts it in a listen state
//----------------------------------------------------------------------------------------------------------------------
HRESULT SpeechRetainApp::StartListening()
{
    HRESULT hr = S_OK;

    // create the local speech engine
    NUI_SPEECH_INIT_PROPERTIES props;

    ZeroMemory( &props, sizeof(props) );

    // Set the default input language (Needed for "Stop", "Pause", etc, for
    // the internal dictionaries. However, your custom dictionary can
    // "bend" the rules and be multi-lingual if your dictionary is robust enough.
    // Change this to support the language you want
    props.Language = NUI_SPEECH_LANGUAGE_EN_US;

    // use Kinect for input
    props.MicrophoneType = NUI_SPEECH_KINECT;

    // Which hardware thread is preferred to process the audio input into
    // speech tokens (Heavy lifting when data is processed)
    DWORD dwHardwareThread = NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD;

    // Initialize the speech engine with default hardware thread values
    hr = NuiSpeechEnable( &props, dwHardwareThread );

    if( FAILED( hr ) )
    {
        m_Console.Format( "Failed to initialize Nui Speech API." );
        return hr;
    }

    // Initialize event notifications
    // Normally, allow Nui to process all events for you. If there is a need
    // by your application to go beyond what is provided, or if this is for
    // a custom input device, then it's possible you'd want to process only
    // certain events. If you wish to process only certain events, do
    // so with caution.
    const ULONG eventInterest = NUI_SPEECH_ALL_EVENTS;
    hr = NuiSpeechSetEventInterest( eventInterest );
    if( FAILED( hr ) )
    {
        m_Console.Format( "Failed to set event interest." );
        return hr;
    }

    // Parm #1, filename on DVD/Hard drive
    // Parm #2, an application supplied ID number for the dictionary.
    // Parm #3, static means that the grammar rules cannot be modified or committed at runtime
    // Parm #4, Pointer returned for the valid speech object
    hr = NuiSpeechLoadGrammar( STARTUP_GRAMMAR, LOADED_GRAMMAR_ID, NUI_SPEECH_LOADOPTIONS_STATIC, &m_Grammar );
    if( FAILED( hr ) )
    {
        m_Console.Format( "Failed to load Grammar file." );
        return hr;
    }

    m_loadedGrammarID = LOADED_GRAMMAR_ID;
    m_loadedGrammarPath = STARTUP_GRAMMAR;

    // Enables the Speech Engine to retain the audio used for recognition   
    hr = NuiSpeechSetProperty(NUI_SPEECH_PROPERTY_RETAIN_AUDIO, TRUE);
    if( FAILED( hr) )
    {
        m_Console.Format( "Failed to set RETAIN_AUDIO property." );
    }

    // Start the recognition engine
    hr = NuiSpeechStartRecognition();
    if( FAILED( hr ) )
    {
        m_Console.Format( "Failed to start recognition." );
    }

    m_speechActivate = TRUE;

    DmMapDevkitDrive();

    CreateDirectory( RETAIN_AUDIO_PATH, NULL ); 
    
    SYSTEMTIME systemTime;    
    GetSystemTime(&systemTime);

    CHAR strMetadataFileName[MAX_PATH];
    sprintf_s( strMetadataFileName, "%s\\output_%04d%02d%02dT%02d%02d%02d.txt", 
                                    RETAIN_AUDIO_PATH,
                                    systemTime.wYear,
                                    systemTime.wMonth,
                                    systemTime.wDay,
                                    systemTime.wHour,
                                    systemTime.wMinute,
                                    systemTime.wSecond ); 

    DWORD dwNumberOfBytesWritten;
    m_hMetadata = CreateFile( strMetadataFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ); 

    if( m_hMetadata == INVALID_HANDLE_VALUE )
    {
        m_Console.Format( "Unable to create file %s", strMetadataFileName );
    }

    //Write Unicode Marker
    WriteFile(m_hMetadata, BOM_HEADER, 2, &dwNumberOfBytesWritten, NULL);

    WCHAR grammarInfo[MAX_PATH];
    swprintf_s( grammarInfo, L"#grammars=%s\r\n", m_loadedGrammarPath );
    WriteFile(m_hMetadata, &grammarInfo, sizeof(WCHAR) * wcslen(grammarInfo), &dwNumberOfBytesWritten, NULL);

    m_Console.Format( L"Speech started.  Press A to stop.\n" );

    return hr;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: SpeechRetainApp::StopListening
// Desc: Stops the Speech Engine
//----------------------------------------------------------------------------------------------------------------------
HRESULT SpeechRetainApp::StopListening()
{
    HRESULT hr = S_OK;

    // If we already had a grammar loaded, we need to shutdown.
    if( m_loadedGrammarID != NO_GRAMMAR_LOADED )
    {
        hr = NuiSpeechUnloadGrammar( &m_Grammar );
        if( FAILED( hr ) )
        {
            ATG::DebugSpew( "Failed to unload grammar." );
        }

        hr = NuiSpeechStopRecognition();
        if( FAILED( hr ) )
        {
            ATG::DebugSpew( "Failed to disable speech engine." );
            return hr;
        }

        // Disable speech recognition; note: we're shutting EVERYTHING down, so no need to call StopRecognition.
        hr = NuiSpeechDisable();

        if( FAILED( hr ) )
        {
            ATG::DebugSpew( "Failed to disable speech engine." );
            return hr;
        }

        m_loadedGrammarID = NO_GRAMMAR_LOADED;
        m_loadedGrammarPath = NULL;

        if( m_hMetadata ) 
        {
            CloseHandle( m_hMetadata );
            m_hMetadata = NULL;
        }
        
        ClearAudioBuffer();

        if ( m_pLastSerializedState != NULL )
        {
            NuiSpeechDestroySerializedState( m_pLastSerializedState );
            m_pLastSerializedState = NULL;
            m_ulLastSerializedStateSize = 0;
        }
        
        m_speechActivate = FALSE;
        m_Console.Format( L"Speech stopped.  Press A to start.\n" );
    }

    return hr;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: SpeechRetainApp::ToggleListening
// Desc: Toggles the listening state of the speech engine
//----------------------------------------------------------------------------------------------------------------------
HRESULT SpeechRetainApp::ToggleListening()
{
    HRESULT hr = S_OK;

    if(m_speechActivate) 
    {
        hr = StopListening();
    }
    else
    {
        hr = StartListening();
    }

    return hr;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: SpeechRetainApp::WriteAudio
// Desc: Process the retained audio from the Reco Result.  The retained audio is RAW data and a RIFF header must be 
// created for use later on.
//----------------------------------------------------------------------------------------------------------------------
VOID SpeechRetainApp::WriteAudio( ULONGLONG ullStart, ULONGLONG ullEnd, ULONGLONG ullStartTime )
{        
    if ( ullStart >= ullEnd )
    {
        ATG::DebugSpew( "ullStart is greater than or equal ullEnd.  Ignoring Write Audio request" );
        return;
    }

    DWORD dwAudioSize = (DWORD)(ullEnd - ullStart);
    BYTE *pAudioData = new BYTE[dwAudioSize];

    ULONG ulBytesWritten;    
    GetAudioFromStreamRange( ullStart, ullEnd, pAudioData, dwAudioSize, &ulBytesWritten );

    //Wave format data information to be inserted in the RIFF header
    WAVEFORMAT waveFormat = {
        WAVE_FORMAT_PCM,
        WAVE_NUM_OF_CHANNELS,
        WAVE_SAMPLE_RATE,               
        WAVE_AVG_BYTES_PER_SEC,
        WAVE_BLOCK_ALIGN
    };

    // We need to convert from big endian to little endian
    waveFormat.wFormatTag           = __loadshortbytereverse( 0, &waveFormat.wFormatTag );
    waveFormat.nChannels            = __loadshortbytereverse( 0, &waveFormat.nChannels );
    waveFormat.nSamplesPerSec       = __loadwordbytereverse( 0, &waveFormat.nSamplesPerSec );
    waveFormat.nAvgBytesPerSec      = __loadwordbytereverse( 0, &waveFormat.nAvgBytesPerSec );
    waveFormat.nBlockAlign          = __loadshortbytereverse( 0, &waveFormat.nBlockAlign );   

    PCMWAVEFORMAT pcmWaveFormat = {
        waveFormat,
        WAVE_BITS_PER_SAMPLE
    }; 

    // We need to convert from big endian to little endian
    pcmWaveFormat.wBitsPerSample    = __loadshortbytereverse( 0, &pcmWaveFormat.wBitsPerSample );

    // Converting the audio data from big endian to little endian
    for( DWORD dwIndex = 0; dwIndex < ulBytesWritten / 2; ++dwIndex )
    {
        *(SHORT*)(pAudioData + (dwIndex * 2)) = __loadshortbytereverse( dwIndex * 2, pAudioData );
    }

    //Create RIFF header
    RIFF_DATA riffData = {
        RIFF,
        ulBytesWritten + SIZE_OF_CHUNK_BEFORE_DATA,
        WAVE,
        FORMAT,
        sizeof(PCMWAVEFORMAT),
        pcmWaveFormat,
        DATA,
        ulBytesWritten
    };

    //We need to convert from big endian to little endian
    riffData.chunkSize = __loadwordbytereverse( 0, &riffData.chunkSize );
    riffData.subchunk1Size = __loadwordbytereverse( 0, &riffData.subchunk1Size );
    riffData.subchunk2Size = __loadwordbytereverse( 0, &riffData.subchunk2Size );   

    DWORD dwNumberOfBytesWritten;

    CHAR strFileName[MAX_PATH];
    sprintf_s( strFileName, MAX_PATH, "%s\\audio_%llu.wav", RETAIN_AUDIO_PATH, ullStartTime );     
    //Open WAV file for writing
    HANDLE hFile = CreateFile( strFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );

    if( hFile != INVALID_HANDLE_VALUE )
    {
        //Write RIFF header
        WriteFile( hFile, &riffData, sizeof(RIFF_DATA), &dwNumberOfBytesWritten, NULL );

        //Write audio data
        WriteFile( hFile, pAudioData, ulBytesWritten, &dwNumberOfBytesWritten, NULL );    
        CloseHandle( hFile );
    }

    delete [] pAudioData;

    m_Console.Format( "Wrote audio data to %s for audio range %I64u to %I64u\n", strFileName, ullStart, ullEnd );

	if ( ulBytesWritten != dwAudioSize )
	{
		m_Console.Format("Audio buffer ran out of space during capture; expected %u bytes, recorded %u. File may be missing audio from start.\n", dwAudioSize, ulBytesWritten );
	}
}

//----------------------------------------------------------------------------------------------------------------------
// Name: SpeechRetainApp::WriteSerializedState
// Desc: Write the serialized state of the engine for later analysis.
//----------------------------------------------------------------------------------------------------------------------
VOID SpeechRetainApp::WriteSerializedState( BYTE* pLastSerializedState, ULONG ulLastSerializedStateSize, ULONGLONG ullAudioStreamOffset )
{
    if ( pLastSerializedState != NULL && ulLastSerializedStateSize > 0)
    {
        CHAR strSerializedFileName[MAX_PATH];
        sprintf_s( strSerializedFileName, MAX_PATH, "%s\\audio_%llu.ser", RETAIN_AUDIO_PATH, ullAudioStreamOffset );     
        //Open Serialized State file for writing
        HANDLE hSerFile = CreateFile( strSerializedFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );

        if( hSerFile != INVALID_HANDLE_VALUE )
        {
            DWORD dwNumberOfBytesWritten;
            WriteFile( hSerFile, pLastSerializedState, ulLastSerializedStateSize, &dwNumberOfBytesWritten, NULL );
            CloseHandle( hSerFile );
            m_Console.Format( "Wrote serialized state audio data to %s\n", strSerializedFileName );   
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Name: SpeechRetainApp::WriteMetadata
// Desc: Extract metadata from the Reco Result and write to disk.  
//----------------------------------------------------------------------------------------------------------------------
VOID SpeechRetainApp::WriteMetadata( const NUI_SPEECH_RECORESULT* pRR )
{
    if( m_hMetadata != NULL && m_hMetadata != INVALID_HANDLE_VALUE )
    {
        CHAR strFileName[MAX_PATH];
        sprintf_s( strFileName, MAX_PATH, "%s\\audio_%llu.wav", RETAIN_AUDIO_PATH, pRR->Phrase.ullStartTime );        

        WCHAR wFileName[MAX_PATH];
        MultiByteToWideChar( CP_ACP, 0, strFileName, -1, wFileName, MAX_PATH );

        DWORD dwNumberOfBytesWritten;
        WriteFile( m_hMetadata, &wFileName, sizeof(WCHAR) * wcslen(wFileName), &dwNumberOfBytesWritten, NULL );    

        WriteFile( m_hMetadata, METADATA_DELIMITER, sizeof(WCHAR), &dwNumberOfBytesWritten, NULL );

        m_Console.Format( L"Heard \"" ); 
        
        // Write out recognized text to metadata file
        for( ULONG i = 0; i < pRR->Phrase.Rule.ulCountOfElements; i++ )
        {
            if(i > 0) 
            {                
                WriteFile( m_hMetadata, L" ", sizeof(WCHAR), &dwNumberOfBytesWritten, NULL );
                m_Console.Format( L" " );
            }
            const NUI_SPEECH_ELEMENT* pElt = &pRR->Phrase.pElements[i];
            WriteFile( m_hMetadata, pElt->pcwszLexicalForm, sizeof(WCHAR) * wcslen( pElt->pcwszLexicalForm ), &dwNumberOfBytesWritten, NULL );        
            m_Console.Format( L"%s", pElt->pcwszLexicalForm ); 
        }

        WriteFile( m_hMetadata, METADATA_DELIMITER, sizeof(WCHAR), &dwNumberOfBytesWritten, NULL );
        
        // Write out the sematic recognition to metadata file
        NUI_SPEECH_SEMANTICRESULT* pSemanticResult = pRR->Phrase.pSemanticProperties;
        
        m_Console.Format( L"\" tag is \"" );
        if( pSemanticResult != NULL )
        {
            if( pSemanticResult->pcwszValue != NULL )
            {
                WriteFile( m_hMetadata, 
                            pSemanticResult->pcwszValue, 
                            sizeof(WCHAR) * wcslen( pSemanticResult->pcwszValue ), 
                            &dwNumberOfBytesWritten, 
                            NULL );    
                m_Console.Format( L"%s", pSemanticResult->pcwszValue );

            }
        }

        WriteFile( m_hMetadata, L"\r\n", 2 * sizeof(WCHAR), &dwNumberOfBytesWritten, NULL );

        if ( pSemanticResult != NULL )
        {
            m_Console.Format( L"\" [%0.2f%% property confidence]\n\n", pSemanticResult->fSREngineConfidence * 100.0f );
        }
        else
        {
            m_Console.Format( L"\" [%0.2f%% rule confidence]\n\n", pRR->Phrase.Rule.fSREngineConfidence * 100.0f );
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Name: SpeechRetainApp::ClearAudioBuffer
// Desc: Removes all elements from the retain audio buffer
//----------------------------------------------------------------------------------------------------------------------
VOID SpeechRetainApp::ClearAudioBuffer()
{
    std::deque<RETAINED_AUDIO>::iterator it = m_retainedAudioQueue.begin();
    while( it != m_retainedAudioQueue.end() )
    {
        delete [] it->pAudioData;
        ++it;
    }

    m_retainedAudioQueue.clear();   
}

VOID SpeechRetainApp::AddToAudioBuffer( NUI_SPEECH_EVENT* pEvent )
{    
    RETAINED_AUDIO audio;

    audio.ullAudioStreamOffset = pEvent->ullAudioStreamOffset;
    audio.ulAudioSize = pEvent->Audio.ulAudioSize;
    audio.pAudioData = new BYTE[pEvent->Audio.ulAudioSize];
    
    memcpy_s( audio.pAudioData, audio.ulAudioSize,  pEvent->Audio.pAudioData, pEvent->Audio.ulAudioSize );

    m_retainedAudioQueue.push_front( audio );

    if (m_retainedAudioQueue.size() > RETAIN_AUDIO_BUFFER_SIZE )
    {
        delete [] m_retainedAudioQueue.back().pAudioData;        
        m_retainedAudioQueue.pop_back();
    }

}

//--------------------------------------------------------------------------------------
// Name: ProcessRecognitionEvent()
// Desc: Process the recognition result and print to console.
//--------------------------------------------------------------------------------------
VOID SpeechRetainApp::ProcessRecognitionEvent( const NUI_SPEECH_EVENT* pEvent )
{
    switch( pEvent->eventId )
    {        
        // A pretty confident guess has been made (Full phrase recognized)
        case NUI_SPEECH_EVENT_RECOGNITION:
            __fallthrough;
        case NUI_SPEECH_EVENT_FALSE_RECOGNITION:
            {
                const CHAR *strNUIEvent = (pEvent->eventId == NUI_SPEECH_EVENT_RECOGNITION) ?
                    "NUI_SPEECH_EVENT_RECOGNITION" : "NUI_SPEECH_EVENT_FALSE_RECOGNITION";

                m_Console.Format( "%s event received.\n", strNUIEvent );
                m_Console.Format( "%s: %I64u, started at %I64u\n", strNUIEvent, pEvent->ullAudioStreamOffset, pEvent->pResult->Phrase.ullAudioStreamPosition );

                m_captureAudioQueue.front().ullStartTime = pEvent->pResult->Phrase.ullStartTime;      

                if ( m_captureAudioQueue.front().pLastSerializedState != NULL )
                {
                    WriteSerializedState( m_captureAudioQueue.front().pLastSerializedState, 
                                m_captureAudioQueue.front().ulLastSerializedStateSize, 
                                pEvent->pResult->Phrase.ullStartTime );             

                    delete [] m_captureAudioQueue.front().pLastSerializedState;
                    m_captureAudioQueue.front().ulLastSerializedStateSize = 0;
                }

                WriteMetadata( pEvent->pResult );
            }
            break;      
        case NUI_SPEECH_EVENT_RETAINEDAUDIO:
            if ( pEvent->pResult )
            {                    
                AddToAudioBuffer( (NUI_SPEECH_EVENT*) pEvent );    
                
                if ( !( m_captureAudioQueue.empty() )  )
                {
                    AUDIO_CAPTURE capture = m_captureAudioQueue.back();

                    // Wait until we have a endOffset (SOUND_END EVENT) before processing the audio.  
                    // Make sure we have enough buffer to process the audio data.
                    if ( capture.endOffset != 0 && capture.endOffset + EXTRA_AUDIO_BYTES < pEvent->ullAudioStreamOffset + pEvent->Audio.ulAudioSize )
                    {             
                        // If we have ullStartTime, then we had a RECO/FALSE event.  Process the audio.
                        if ( capture.ullStartTime != 0 )
                        {
                            ULONGLONG ullStartProcessOffset = capture.startOffset - EXTRA_AUDIO_BYTES;

                            //We didn't have enough beginning audio.  Use the start of the stream.
                            if ( ullStartProcessOffset > capture.startOffset )
                            {
                                ullStartProcessOffset = 0;
                            }

                            //We only want audio back to the last SOUND_END.
                            if ( ullStartProcessOffset < capture.ullLastSoundEndOffset )
                            {
                                ullStartProcessOffset = capture.ullLastSoundEndOffset;
                            }

                            WriteAudio( ullStartProcessOffset, 
                                            capture.endOffset + EXTRA_AUDIO_BYTES, 
                                            capture.ullStartTime );                              
                        }
                        m_captureAudioQueue.pop_back();                        
                    }                     
                }
            }
            break;
        case NUI_SPEECH_EVENT_SOUND_START:
            {
                m_Console.Format( "NUI_SPEECH_EVENT_SOUND_START: %I64u\n", pEvent->ullAudioStreamOffset );   
                AUDIO_CAPTURE audioCapture = { 0 }; 
                audioCapture.startOffset = pEvent->ullAudioStreamOffset;
                audioCapture.ullLastSoundEndOffset = m_ullLastSoundEndOffset;
                m_captureAudioQueue.push_front( audioCapture );
            }
            break;
        case NUI_SPEECH_EVENT_SOUND_END:
            m_Console.Format( "NUI_SPEECH_EVENT_SOUND_END: %I64u\n", pEvent->ullAudioStreamOffset ); 
            
            if ( m_pLastSerializedState != NULL )
            {
                NuiSpeechDestroySerializedState( m_pLastSerializedState );
                m_pLastSerializedState = NULL;
                m_ulLastSerializedStateSize = 0;
            }

            NuiSpeechGetSerializedState( &m_pLastSerializedState, &m_ulLastSerializedStateSize );
            m_captureAudioQueue.front().endOffset = pEvent->ullAudioStreamOffset;         

            m_ullLastSoundEndOffset = pEvent->ullAudioStreamOffset;
            break;
        case NUI_SPEECH_EVENT_PHRASE_START:
            if ( m_pLastSerializedState != NULL )
            {
                m_captureAudioQueue.front().pLastSerializedState = new BYTE[m_ulLastSerializedStateSize];
                m_captureAudioQueue.front().ulLastSerializedStateSize = m_ulLastSerializedStateSize;
                memcpy_s( m_captureAudioQueue.front().pLastSerializedState, 
                            m_ulLastSerializedStateSize, 
                            m_pLastSerializedState, 
                            m_ulLastSerializedStateSize );                
            }
            break;
    }
}

//--------------------------------------------------------------------------------------
// Name: GetAudioFromStreamRange()
// Desc: Obtains the retained audio buffer data for the audio offsets requested
//--------------------------------------------------------------------------------------
VOID SpeechRetainApp::GetAudioFromStreamRange( ULONGLONG ullStart, ULONGLONG ullEnd, BYTE* pAudio, DWORD dwAudioSize, DWORD* pdwAudioWritten )
{      
    DWORD bufferSize = 0;
    for( std::deque<RETAINED_AUDIO>::reverse_iterator it = m_retainedAudioQueue.rbegin(); it != m_retainedAudioQueue.rend(); ++it )
    {        
        bufferSize += it->ulAudioSize;
    }

    BYTE *tempBuffer = new BYTE[bufferSize]; 

    int offset = 0;
    for( std::deque<RETAINED_AUDIO>::reverse_iterator it = m_retainedAudioQueue.rbegin(); it != m_retainedAudioQueue.rend(); ++it )
    {
        DWORD audioSize = it->ulAudioSize;
        memcpy_s( tempBuffer + offset, audioSize, it->pAudioData, audioSize );
        offset += audioSize;
    }

    ULONGLONG baseAudioOffset = (m_retainedAudioQueue.back()).ullAudioStreamOffset;

	ULONGLONG ullStartBufferOffset;

	// Ensure we're copying from within the buffer.
	if ( ullStart < baseAudioOffset )
	{
		// Adjust to start of buffer...
		ullStartBufferOffset = 0;

		// Adjust copy size to not include area before buffer.
		dwAudioSize -= (DWORD)( baseAudioOffset - ullStart );
	}
	else
	{
		ullStartBufferOffset = ullStart - baseAudioOffset;
	}

	if ( ullStartBufferOffset > bufferSize )
	{
		dwAudioSize = 0;
	}
	else
	{
		dwAudioSize = (DWORD)( min( (ULONGLONG)bufferSize, ullStartBufferOffset + (ULONGLONG)dwAudioSize ) - ullStartBufferOffset );
		memcpy_s( pAudio, dwAudioSize, tempBuffer + ullStartBufferOffset, dwAudioSize );
	}

    *pdwAudioWritten = dwAudioSize;

    delete [] tempBuffer;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Primary update entry function
//--------------------------------------------------------------------------------------
HRESULT SpeechRetainApp::Update()
{
    // Maximum number of events I can process with one call
    // 5 is good enough for a majority of cases.
    const int MAX_EVENTS = 5;
    NUI_SPEECH_EVENT events[MAX_EVENTS];    
    
    BOOL fDone = FALSE;
    ULONG eventsSeen = 0;
    ULONG ulEventsFetched = 0;
    do
    {
        // Detect exit sample request LT-RT-RB (done automatically)
        ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            ToggleListening();
        }

        HRESULT hr = S_OK;

        // Poll up to MAX_EVENTS events in one go.
        hr = NuiSpeechGetEvents( MAX_EVENTS, events, &ulEventsFetched );

        if( SUCCEEDED( hr ) )
        {
            for(ULONG index = 0; index < ulEventsFetched; index++)
            {
                NUI_SPEECH_EVENT *pEvent = &events[index];
                // Deal with this event
                ProcessRecognitionEvent( pEvent );
                eventsSeen |= pEvent->eventId;
                // Don't forget to "acknowledge" the event
                NuiSpeechDestroyEvent( pEvent );
            }
        }

        // Stop on end stream
        if( eventsSeen & NUI_SPEECH_EVENT_END_STREAM)
        {
            fDone = TRUE;
        }
    } while(!fDone);
    return 0;
}        

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: main entry point
//--------------------------------------------------------------------------------------
INT main( VOID )
{
    HRESULT hr = NOERROR;
    // Create my instance which also initializes the console
    SpeechRetainApp TheApp;

    // Initialize the speech engine with the available microphone
    hr = TheApp.CreateNuiSpeech();
    if( FAILED(hr) )
    {
        return 0;
    }
        
    // Primary update loop
    for( ;; )
    {
        // Note: The app will exit() when LT-RT-RB is pressed
        // inside of the function GetMergedInput()
        TheApp.Update();
    }
}
