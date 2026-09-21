//--------------------------------------------------------------------------------------
// SimpleSpeechRecognition Sample
//
// Demonstrates use of the NuiSpeech API
// 
// Usage:
// Plug in a Kinect sensor array and start the sample.  The NATO alphabet is recognized.
//
// Sample calls the following APIs to initialize speech:
// NuiSpeechEnable
// NuiSpeechSetEventInterest
// NuiSpeechLoadGrammar
//
// The update loop calls the following:
// NuiSpeechStartRecognition
// NuiSpeechGetEvents
// NuiSpeechStopRecognition
// GetEvents will ultimately display the recognized phrase on screen.
//
// NOTE: speechlab.xex (which is installed with the XDK Recovery) is an excellent tool
//       for performing ad-hoc testing and tuning of grammars under development.
//       Please look in the XDK Documentation for further information on grammar
//       development using the speechlab tool.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <NuiAudio.h>
#include <AtgInput.h>
#include <AtgConsole.h>
#include <AtgUtil.h>
#include <NuiApi.h>
#include <XInput2.h>
#include <string>
#include <vector>
#include "GrammarFileHelper.h"

// Preferred startup grammar
#define PREFERRED_STARTUP_GRAMMAR "medieval"

// Path to the font to use for the app's console
#define FONT_FILE_NAME "game:\\Media\\Fonts\\Arial_16_Speech.xpr"

static const char g_StartAppText[] = 
    "\nRecognition engine initialized.  Speak into the microphone.\n";

static const char g_HelpText[] = 
    "\n[LB] cycles through grammar files    [RB] cycles through languages    [X] shows available grammars\n"
    "Press [RB] [LT] [RT] at the same time to exit.\n\n\n";


// Minimum allowable confidence level for speech recognition,
// Ranges from 0.0f to 1.0f (0% to 100% confidence)
static const float g_fMinimumConfidence = 0.10f;

// Arbitrary internal ID to assign the loaded grammar file. 
static const ULONG LOADED_GRAMMAR_ID = 55;

// ID value we use when no grammar is loaded.
static const ULONG NO_GRAMMAR_LOADED = 0;


//--------------------------------------------------------------------------------------
// Name: class SimpleSpeechApp
// Desc: Class for the application SimpleSpeechRecognition.
//--------------------------------------------------------------------------------------

class SimpleSpeechApp {
public:
    SimpleSpeechApp();
    ~SimpleSpeechApp() {}
    
    HRESULT CreateNuiSpeech();
    VOID ProcessRecoResult( const NUI_SPEECH_RECORESULT* pRR );
    VOID ProcessRecognitionEvent( const NUI_SPEECH_EVENT* pEvent );
    HRESULT ReloadGrammarFile();
    
    HRESULT Update();
    inline VOID PrintStartText() { m_Console.Format( g_StartAppText ); }
    static BOOL IsMicrophoneConnected(void);
    void RequestMECCalibrationIfNeeded();
    void BindNotifications();
private:
    void DrainNotifications();

    // Grammar file selection routines
    
    VOID CatalogAvailableGrammars();
    HRESULT SelectStartupGrammar();
    HRESULT CycleGrammarFile();
    HRESULT CycleCurrentLanguage();
    VOID EmitGrammarDescription();


    VOID InitConsole();
    ATG::Console m_Console;             // TTY Display for demo purposes

    HANDLE m_hXNotifyListener;          // Handle for notifications
    BOOL m_fUIVisible;                  // Keeps track of UI visibility

    // Speech Recognition state

    ULONG m_loadedGrammarID;
    NUI_SPEECH_GRAMMAR m_Grammar;       // Grammar instance

    // Grammar file selection housekeeping:

    GrammarFileList m_grammarList;      // A catalog of available grammar files.
    NUI_SPEECH_LANGUAGE m_language;     // Current language
    GrammarFileLangVariant* m_pGrammarVariant; // Current grammar file information.
    SIZE_T m_grammarFileIndex;             // Current grammar file index.
};

//--------------------------------------------------------------------------------------
// Name: SimpleSpeechApp()
// Desc: Initializes the app and starts up the console
//--------------------------------------------------------------------------------------

SimpleSpeechApp::SimpleSpeechApp() :
    m_hXNotifyListener( NULL ),
    m_fUIVisible( FALSE ),
    m_loadedGrammarID( NO_GRAMMAR_LOADED ),
    m_language( DEFAULTSPEECHLANGUAGE ),
    m_pGrammarVariant( NULL ),
    m_grammarFileIndex( 0 )
{
    InitConsole();
}

//--------------------------------------------------------------------------------------
// Name: VOID InitConsole()
// Desc: Initializes the ATG::Console, only used internally for startup
//--------------------------------------------------------------------------------------

VOID SimpleSpeechApp::InitConsole()
{
    //
    // Initialize the console window
    //
    m_Console.Create( FONT_FILE_NAME, 0xFF1F005F, 0xFFFFFFFF );
    m_Console.SendOutputToDebugChannel( TRUE );
}

//----------------------------------------------------------------------------------------------------------------------
// Name: SimpleSpeechApp::CatalogAvailableGrammars
// Desc: Builds a catalog of the grammars that are installed.
//----------------------------------------------------------------------------------------------------------------------
VOID SimpleSpeechApp::CatalogAvailableGrammars()
{
    m_Console.Format( L"Cataloging grammars...\n" );
    FindGrammarFiles( m_grammarList );
    
    for ( GrammarFileList::iterator i = m_grammarList.begin(); i != m_grammarList.end(); ++i )
    {
        for ( GrammarFileVariantList::iterator v = i->variants.begin(); v != i->variants.end(); ++v )
        {
            m_Console.Format( "  Found ");
            m_Console.Format( i->simplename.c_str() );
            m_Console.Format( L" [%s]\n", GrammarFileLanguageToString(v->language) );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: CreateNuiSpeech()
// Desc: Initialize the NuiSpeech subsystem.
// Return NOERROR on successful initialization or the Nui/XDK
// HRESULT error if a failure.
//--------------------------------------------------------------------------------------

HRESULT SimpleSpeechApp::CreateNuiSpeech()
{
    // Init the system (use defaults)
    // If your app wanted to use the camera for RGB or skeleton tracking then you would
    // initialize Nui here witha call to NuiInitialize.  Using the speech subsystem only
    // does not require a call to NuiInitialize

    // Figure out what grammar files we have available to us.
    // Note: This is nothing to do with the speech API per se; it's just so that we can easily flip between
    // grammars for this sample.
    CatalogAvailableGrammars();

    // The rest of the NuiSpeech initialization occurs in here, as part of ReloadGrammarFile().
    return SelectStartupGrammar();
}


//----------------------------------------------------------------------------------------------------------------------
// Name: SimpleSpeechApp::ReloadGrammarFile
// Desc: Loads a grammar file, reinitializing the speech engine so that it uses the correct speech model according to
//       the grammar file's language.
//----------------------------------------------------------------------------------------------------------------------
HRESULT SimpleSpeechApp::ReloadGrammarFile()
{
    HRESULT hr;

    // If we already had a grammar loaded, we need to shutdown.
    if ( m_loadedGrammarID != NO_GRAMMAR_LOADED )
    {
        hr = NuiSpeechUnloadGrammar( &m_Grammar );
        if ( FAILED( hr ) )
        {
            ATG::DebugSpew( "Failed to unload grammar.\n" );
        }

        hr = NuiSpeechStopRecognition();
        if ( FAILED( hr ) )
        {
            ATG::DebugSpew( "Failed to disable speech engine.\n" );
            return hr;
        }

        // Disable speech recognition
        hr = NuiSpeechDisable();

        if ( FAILED( hr ) )
        {
            ATG::DebugSpew( "Failed to disable speech engine.\n" );
            return hr;
        }

        m_loadedGrammarID = NO_GRAMMAR_LOADED;
    }

    // create the local speech engine
    NUI_SPEECH_INIT_PROPERTIES props;

    ZeroMemory( &props, sizeof(props) );

    // XAudio2 does not have to be explicitly initialized unless using XHV2

    // Set the default input language (Needed for "Stop", "Pause", etc, for
    // the internal dictionaries. However, your custom dictionary can
    // "bend" the rules and be multi-lingual if your dictionary is robust enough.

    props.Language = m_language;

    // Use real-time input from Kinect
    props.MicrophoneType = NUI_SPEECH_KINECT;

    // Which hardware thread is preferred to process the audio input into
    // speech tokens (Heavy lifting when data is processed)
    DWORD dwHardwareThread = NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD;

    hr = NuiSpeechEnable( &props, dwHardwareThread );

    if( FAILED( hr ) )
    {
        m_Console.Format( "Failed to initialize Nui Speech API.\n" );
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
        m_Console.Format( "Failed to set event interest.\n" );
        return hr;
    }

    // Parm #1, filename on DVD/Hard drive
    // Parm #2, an application supplied ID number for the dictionary.
    // Parm #3, static means that the grammar rules cannot be modified or committed at runtime
    // Parm #4, Pointer returned for the valid speech object
    hr = NuiSpeechLoadGrammar( ANSItoWstr( m_pGrammarVariant->path.c_str() ).c_str(), LOADED_GRAMMAR_ID, NUI_SPEECH_LOADOPTIONS_STATIC, &m_Grammar );
    if( FAILED( hr ) )
    {
        m_Console.Format( "\nFailed to load Grammar file %s. No grammar loaded.\n\n", m_pGrammarVariant->path.c_str() );


        // For our sample, we want to keep rolling if a bad file loads, so continue from here as if we succeeded.

        // Clean up
        hr = NuiSpeechDisable();

        if ( FAILED( hr ) )
        {
            ATG::DebugSpew( "Failed to disable speech engine.\n" );
            return hr;
        }

        return S_OK;
    }

    EmitGrammarDescription();

    m_loadedGrammarID = LOADED_GRAMMAR_ID;

    // Start the recognition engine
    hr = NuiSpeechStartRecognition();
    if( FAILED( hr ) )
    {
        m_Console.Format( "Failed to start recognition." );
    }
    
    return hr;
}


//--------------------------------------------------------------------------------------
// Name: ProcessRecoResult()
// Desc: Process the recognition result and print to console.
//--------------------------------------------------------------------------------------
VOID SimpleSpeechApp::ProcessRecoResult(const NUI_SPEECH_RECORESULT* pRR )
{
    // We preferentially use the Semantic Property Confidence value vs. the Rule confidence
    // value as this gives better results for false accepts/false rejects than the
    // Rule confidence.

    // We fall-back to the Rule confidence if we can't find any semantic information
    // for the rule.

    if (pRR->Phrase.pSemanticProperties)
    {
        
        // NOTE: In your title, you may wish to utilize different minimum confidence values
        // for different languages. This can give better results.
        if ( pRR->Phrase.pSemanticProperties->fSREngineConfidence < g_fMinimumConfidence )
        {
            return;
        }

        m_Console.Format( L"Heard \"" );
        for( ULONG i = 0; i < pRR->Phrase.Rule.ulCountOfElements; i++ )
        {
            const NUI_SPEECH_ELEMENT* pElt = &pRR->Phrase.pElements[i];
            m_Console.Format( L"%s ",pElt->pcwszLexicalForm );
        }

        m_Console.Format( L"\"" );

        // Print semantic tags for phrase if present...
        m_Console.Format( L" / tag is \"%s\"", pRR->Phrase.pSemanticProperties->pcwszValue );

        // print confidence percentage for the phrase
        m_Console.Format( L" [%0.2f%% property (%0.2f%% rule) confidence]\n",
                          pRR->Phrase.pSemanticProperties->fSREngineConfidence * 100.0f,
                          pRR->Phrase.Rule.fSREngineConfidence * 100.0f ); 
    }
    else
    {
        // Fallback to Rule confidence, as we were missing Phrase data.

        // Discard any matches with 25% or lower confidence rating
        if( pRR->Phrase.Rule.fSREngineConfidence < g_fMinimumConfidence )
            return;

        m_Console.Format( L"Heard \"" );
        for( ULONG i = 0; i < pRR->Phrase.Rule.ulCountOfElements; i++ )
        {
            const NUI_SPEECH_ELEMENT* pElt = &pRR->Phrase.pElements[i];
            m_Console.Format( L"%s ",pElt->pcwszLexicalForm );
        }

        // Semantic tags for phrase are missing...
        m_Console.Format( L"\" (semantic information missing)");

        // print confidence percentage for the phrase
        m_Console.Format( L" [%0.2f%% rule confidence]\n", pRR->Phrase.Rule.fSREngineConfidence * 100.0f ); 

    }
}


//--------------------------------------------------------------------------------------
// Name: ProcessRecognitionEvent()
// Desc: Process the recognition result and print to console.
//--------------------------------------------------------------------------------------
VOID SimpleSpeechApp::ProcessRecognitionEvent( const NUI_SPEECH_EVENT* pEvent )
{

    switch(pEvent->eventId)
    {
        // Stream of audio data is coming in (Does not mean there is actual sound coming in)
        case NUI_SPEECH_EVENT_START_STREAM:
            OutputDebugString( "NUI_SPEECH_EVENT_START_STREAM event received.\n" );
            break;
        // Stream of audio data has ended (Processing is the next logical step)
        case NUI_SPEECH_EVENT_END_STREAM:
            OutputDebugString( "NUI_SPEECH_EVENT_END_STREAM event received.\n" );
            break;
        // Stream of non-silence audio data is coming in
        case NUI_SPEECH_EVENT_SOUND_START:
            OutputDebugString( "NUI_SPEECH_EVENT_SOUND_START event received.\n" );
            break;
        // Stream of silence has begun
        case NUI_SPEECH_EVENT_SOUND_END:
            OutputDebugString( "NUI_SPEECH_EVENT_SOUND_END event received.\n" );
            break;
        // A speech phrase was detected and has begun
        case NUI_SPEECH_EVENT_PHRASE_START:
            OutputDebugString( "NUI_SPEECH_EVENT_PHRASE_START event received.\n" );
            break;
        // A guess has been made as to the word spoken. (Partial phrase recognized)
        case NUI_SPEECH_EVENT_HYPOTHESIS:
            OutputDebugString( "NUI_SPEECH_EVENT_HYPOTHESIS event received.\n" );
            break;
        // A pretty confident guess has been made (Full phrase recognized)
        case NUI_SPEECH_EVENT_RECOGNITION:
            OutputDebugString( "NUI_SPEECH_EVENT_RECOGNITION event received.\n" );
            if( pEvent->pResult )
            {
                ProcessRecoResult( pEvent->pResult );
            }
            else
            {
                OutputDebugString( "*********ERROR: RecoResult is NULL**********\n" );
            }
            break;
        // A phrase was processed and nothing matched
        case NUI_SPEECH_EVENT_FALSE_RECOGNITION:
            OutputDebugString( "NUI_SPEECH_EVENT_FALSE_RECOGNITION event received.\n" );
            if( pEvent->pResult )
            {
                ProcessRecoResult( pEvent->pResult );
            }
            break;
        // Something occured that interrupted speech recognition (Loss of signal, too much noise,
        // hardware failure)
        case NUI_SPEECH_EVENT_INTERFERENCE:
            OutputDebugString( "NUI_SPEECH_EVENT_INTERFERENCE event received.\n" );
            switch ( pEvent->eInterference )
            {
            // Noise caused the loss
            case NUI_SPEECH_INTERFERENCE_NOISE:
                OutputDebugString( "NUI_SPEECH_INTERFERENCE_NOISE\n" );
                break;
            // Signal was cut off
            case NUI_SPEECH_INTERFERENCE_NOSIGNAL:
                OutputDebugString( "NUI_SPEECH_INTERFERENCE_NOSIGNAL\n" );
                break;
            // Sound was clipped due to excessive volume
            case NUI_SPEECH_INTERFERENCE_TOOLOUD:
                OutputDebugString( "NUI_SPEECH_INTERFERENCE_TOOLOUD\n" );
                break;
            // Not enough audio volume to determine anything
            case NUI_SPEECH_INTERFERENCE_TOOQUIET:
                OutputDebugString( "NUI_SPEECH_INTERFERENCE_TOOQUIET\n" );
                break;
            // Speech pattern was too abrupt
            case NUI_SPEECH_INTERFERENCE_TOOFAST:
                OutputDebugString( "NUI_SPEECH_INTERFERENCE_TOOFAST\n" );
                break;
            // Speech pattern was too slow
            case NUI_SPEECH_INTERFERENCE_TOOSLOW:
                OutputDebugString( "NUI_SPEECH_INTERFERENCE_TOOSLOW\n" );
                break;
            // Internal error
            default:
                OutputDebugString( "Unrecognized interference type received.\n" );
                break;
            }
            break;
        // A bookmark was reached on the input stream (Application defined)
        case NUI_SPEECH_EVENT_BOOKMARK:
            OutputDebugString( "NUI_SPEECH_EVENT_BOOKMARK event received.\n" );
            break;
        // Undefined event
        default:
            OutputDebugString( "Unrecognized event received.\n" );
            break;
    }
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Primary update entry function
//--------------------------------------------------------------------------------------
HRESULT SimpleSpeechApp::Update()
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
        // If the kinect sensor is not connected, show an error.
        if ( ( XNuiGetHardwareStatus() & XNUI_HARDWARE_STATUS_CONNECTED ) == 0 )
        {
            // Wait to be able to show system UI.
            while ( m_fUIVisible )
            {
                DrainNotifications();
            }

            XShowNuiHardwareRequiredUI( 0 );
        }
        
        // Detect exit sample request LT-RT-RB (done automatically)
        ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

        // Use shoulder buttons to cycle through loaded grammar files.

        HRESULT hr = S_OK;

        if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        {
            m_Console.Format( g_HelpText );
        }
        else if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        {
            m_Console.Format("\n\nGrammar Files Found:\n\n");

            for ( GrammarFileList::const_iterator i = m_grammarList.cbegin(); i != m_grammarList.cend(); ++i )
            {
                const GrammarFile& g = *i;

                m_Console.Format( "  %s (", g.simplename.c_str() );

                BOOL bNotFirst = FALSE;
                for ( GrammarFileVariantList::const_iterator j = g.variants.cbegin(); j != g.variants.cend(); ++j )
                {
                    if ( bNotFirst )
                    {
                        m_Console.Format(", ");
                    }
                    else
                    {
                        bNotFirst = TRUE;
                    }

                    m_Console.Format( GrammarFileLanguageToString( j->language ) );
                }

                m_Console.Format( ")\n" );
            }
        }
        else if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
        {
            hr = CycleGrammarFile();
        }
        else if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
        {
            hr = CycleCurrentLanguage();
        }

        if ( FAILED( hr ) )
        {
            ATG::FatalError( "Error while switching grammar files\n" );
        }

        if ( m_loadedGrammarID != NO_GRAMMAR_LOADED )
        {

            // Poll up to MAX_EVENTS events in one go.
            hr = NuiSpeechGetEvents( MAX_EVENTS, events, &ulEventsFetched );

            if( SUCCEEDED( hr ) )
            {
                for (ULONG i = 0; i < ulEventsFetched; ++i)
                {
                    // Deal with this event
                    ProcessRecognitionEvent( &events[i] );
                    eventsSeen |= events[i].eventId;
                    // Don't forget to "acknowledge" the event
                    NuiSpeechDestroyEvent( &events[i] );
                }
            }
        }

        // Stop on end stream
        if( eventsSeen & NUI_SPEECH_EVENT_END_STREAM)
        {
            fDone = TRUE;
        }

        // This sleep is only for the sample. It's not needed for shipping code
        Sleep( 1 );
    } while (!fDone);
    return 0;
}        

//----------------------------------------------------------------------------------------------------------------------
// Name: SimpleSpeechApp::CycleGrammarFile
// Desc: Cycles through the available grammar files.
//----------------------------------------------------------------------------------------------------------------------
HRESULT SimpleSpeechApp::CycleGrammarFile()
{
    ++m_grammarFileIndex;
    if ( m_grammarFileIndex >=  m_grammarList.size() )
        m_grammarFileIndex = 0;

    NUI_SPEECH_LANGUAGE lang = m_language;

    // Now cycle through variants to find one that matches the language we're using.
    GrammarFileLangVariant* pLanguageVariant = m_grammarList[ m_grammarFileIndex ].GetGrammarForLanguage( lang );

    if ( pLanguageVariant == NULL )
    {
        // No match... let's try the default language...
        lang = DEFAULTSPEECHLANGUAGE;

        pLanguageVariant = m_grammarList[ m_grammarFileIndex ].GetGrammarForLanguage( lang );

        // If we failed, just get the first one.
        if ( pLanguageVariant == NULL )
        {
            pLanguageVariant = &m_grammarList[ m_grammarFileIndex ].variants[ 0 ];
            lang = pLanguageVariant->language;
        }
    }

    m_language = lang;
    m_pGrammarVariant = pLanguageVariant;

    return ReloadGrammarFile();
}

//----------------------------------------------------------------------------------------------------------------------
// Name: SimpleSpeechApp::EmitGrammarDescription
// Desc: Outputs a brief description for the user regarding the selected grammar file.
//----------------------------------------------------------------------------------------------------------------------
VOID SimpleSpeechApp::EmitGrammarDescription()
{
    m_Console.Format( L"\n" );

    // Kick the file...
    std::wstring desc = m_pGrammarVariant->LoadDescriptionFromFile();
    if ( desc.length() == 0 )
    {
        // Build a description from the name.
        desc = L"Loaded Grammar \"";
        m_Console.Format( L"Loading Grammar \"");
        m_Console.Format( m_grammarList[ m_grammarFileIndex ].simplename.c_str() );
        m_Console.Format( L"\" [%s] ... description file missing\n",
                          GrammarFileLanguageToString( m_pGrammarVariant->language ) );
    }
    else
    {
        m_Console.Format( desc.c_str() );
    }

    // Finally, show the help text.
    m_Console.Format( g_HelpText );
}

//----------------------------------------------------------------------------------------------------------------------
// Name: SimpleSpeechApp::CycleCurrentLanguage
// Desc: Cycles through the languages available for the currently selected grammar file.
//----------------------------------------------------------------------------------------------------------------------
HRESULT SimpleSpeechApp::CycleCurrentLanguage()
{
    GrammarFileVariantList& gfvl = m_grammarList[ m_grammarFileIndex ].variants;

    // Find the index of the grammar file which matches the current language.
    SIZE_T iLang = 0;
    for ( ; iLang < gfvl.size(); ++iLang )
    {
        if ( gfvl[ iLang ].language == m_language )
            break;
    }

    ++iLang;
    if ( iLang >= gfvl.size() )
    {
        iLang = 0;
    }

    m_language = gfvl[ iLang ].language;
    m_pGrammarVariant = &gfvl[ iLang ];

    return ReloadGrammarFile();
}

//----------------------------------------------------------------------------------------------------------------------
// Name: SimpleSpeechApp::SelectStartupGrammar
// Desc: Picks the most appropriate startup grammar, based on the user's console language setting, and if the 
//       "preferred" grammar file is available.
//----------------------------------------------------------------------------------------------------------------------
HRESULT SimpleSpeechApp::SelectStartupGrammar()
{
    if ( m_grammarList.size() == 0 )
    {
        ATG::FatalError( "No grammar files found... " );
    }

    // Figure out the preferred language; Note: this method can't distinguish between British English and US English.
    // We default to US English for currently unsupported languages.

    NUI_SPEECH_LANGUAGE defLang;

    DWORD lang = XGetLanguage();
    switch ( lang )
    {
    case XC_LANGUAGE_FRENCH:
        {
            defLang = NUI_SPEECH_LANGUAGE_FR_CA; break;
        }
    case XC_LANGUAGE_JAPANESE:
        {
            defLang = NUI_SPEECH_LANGUAGE_JA_JP; break;
        }
    case XC_LANGUAGE_SPANISH:
        {
            defLang = NUI_SPEECH_LANGUAGE_ES_MX; break;
        }
    default:
        {
            defLang = NUI_SPEECH_LANGUAGE_EN_US;
        }
    }

    // Default to first file...
    m_grammarFileIndex = 0;

    // Find the preferred grammar file (if possible) - otherwise, just use the first one.
    for ( INT i = 0; i < (INT)m_grammarList.size(); ++i )
    {
        if ( m_grammarList[i].simplename.compare( PREFERRED_STARTUP_GRAMMAR ) == 0 )
        {
            m_grammarFileIndex = i;
            break;
        }
    }

    m_pGrammarVariant = m_grammarList[ m_grammarFileIndex ].GetGrammarForLanguage( defLang );

    if ( m_pGrammarVariant == NULL )
    {
        defLang = DEFAULTSPEECHLANGUAGE;

        // Fall back to the default language if the specified one isn't in the list.
        m_pGrammarVariant = m_grammarList[ m_grammarFileIndex ].GetGrammarForLanguage( defLang );

        if ( m_pGrammarVariant == NULL )
        {
            m_pGrammarVariant = &m_grammarList[ m_grammarFileIndex ].variants[ 0 ];
            defLang = m_pGrammarVariant->language;
        }
    }

    m_language = defLang;

    return ReloadGrammarFile();
}


//----------------------------------------------------------------------------------------------------------------------
// Name: SimpleSpeechApp::RequestMECCalibrationIfNeeded
// Desc: Checks to see if we have a MEC Calibration file on the system - if not, the user should be asked to perform
//       MEC calibration.
//----------------------------------------------------------------------------------------------------------------------
void SimpleSpeechApp::RequestMECCalibrationIfNeeded()
{
    // Do we have valid audio calibration?
    if ( !NuiAudioIsCalibrationValid()  )
    {
        // Wait for System UI to be invisible.
        while ( m_fUIVisible )
        {
            DrainNotifications();
        }

        static LPCWSTR ButtonCaptions[2] = {
            L"Calibrate",
            L"Ignore"
        };

        MESSAGEBOX_RESULT result;

        XOVERLAPPED asyncResult = { 0 };
        HANDLE hEvent = CreateEvent( NULL, TRUE, FALSE, "MsgBoxWaitEvent" );
        asyncResult.hEvent = hEvent;

        const DWORD CalibrateSystemButtonIndex = 0;

        DWORD err = XShowMessageBoxUI( XUSER_INDEX_ANY, L"Audio Has Not Been Calibrated",
            L"You have not yet calibrated audio on this system, which may negatively affect speech recognition.\r\r"
            L"Would you like to run calibration?", 2, ButtonCaptions, CalibrateSystemButtonIndex,
            XMB_WARNINGICON, &result, &asyncResult );

        if ( err == ERROR_IO_PENDING )
        {
            XGetOverlappedResult( &asyncResult, &err, TRUE );
        }

        if ( err == ERROR_SUCCESS )
        {
            // Wait for previous dialog to close
            do
            {
                DrainNotifications();
            } while ( m_fUIVisible );

            if ( result.dwButtonPressed == CalibrateSystemButtonIndex )
            {
                XShowNuiTroubleshooterUI();
            }
            else
            {
                m_Console.Format(
                    L"\nWarning: Speech Recognition Performance may be poorer than expected unless you "
                    L"perform Audio Calibration from the Kinect Troubleshooter" );
            }
        }
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: SimpleSpeechApp::DrainNotifications
// Desc: Drains all pending notifications (updating the system-ui displayed state)
//----------------------------------------------------------------------------------------------------------------------
void SimpleSpeechApp::DrainNotifications()
{
    DWORD dwId;
    BOOL fVisible;

    m_Console.Render();

    if ( XNotifyGetNext( m_hXNotifyListener, XN_SYS_UI, &dwId, (PULONG_PTR) &fVisible ) )
    {
        m_fUIVisible = fVisible;
    }

}


//----------------------------------------------------------------------------------------------------------------------
// Name: SimpleSpeechApp::BindNotifications
// Desc: Starts listening for notifications from the system.
//----------------------------------------------------------------------------------------------------------------------
void SimpleSpeechApp::BindNotifications()
{
    m_hXNotifyListener = XNotifyCreateListener( XNOTIFY_SYSTEM );
}


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: main entry point
//--------------------------------------------------------------------------------------
INT main( VOID )
{
    HRESULT hr = NOERROR;
    // Create my instance which also initializes the console
    SimpleSpeechApp TheApp;

    TheApp.BindNotifications();

    // Initialize the speech engine with the available microphone
    hr = TheApp.CreateNuiSpeech();
    if( FAILED(hr) )
    {
        return 0;
    }

    // Print the intro text
    TheApp.PrintStartText();

    // Ensure that they've run MEC calibration before running...
    TheApp.RequestMECCalibrationIfNeeded();
    
    // Primary update loop
    for( ;; )
    {
        // Note: The app will exit() when LB-RB-LT-RT is pressed
        // inside of the function GetMergedInput()
        TheApp.Update();
    }
}
