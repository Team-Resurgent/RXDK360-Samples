//--------------------------------------------------------------------------------------
// DynamicSpeech Sample
//
// Demonstrates how to create an equivalent grammar in .xml and in API calls for NuiSpeech.
// 
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
//       This can not be used for grammars created dynamically (via API calls).
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

// Path to the font to use for the app's console
#define FONT_FILE_NAME "game:\\Media\\Fonts\\Arial_16_Speech.xpr"

static const char g_StartAppText[] = 
    "\nRecognition engine initialized.  Speak into the microphone.\n";

static const char g_HelpText[] = 
    "\n[X] builds dynamic grammar    [A] loads compiled grammar \n"
    "Press [RB] [LT] [RT] at the same time to exit.\n\n";

static const char g_phrases[] = 
    "\nPhrases:    \n"
    "Cast Fireball \n"
    "Seven \n"
    "Correct \n"
    "Affirmative \n" 
    "(point/jump/go) (up/down/left/right) \n";

static const DWORD g_numberOfRulesToEnable = 6;

static const wchar_t* g_ruleNames[]={L"basic", L"override", L"alternatives", L"reuse", L"action", L"direction", L"unused"};   
enum GrammarRules { RULE_BASIC = 0, RULE_OVERRIDE, RULE_ALTERNATIVES, RULE_REUSE, RULE_ACTION, RULE_DIRECTION, RULE_NUMBER_OF_RULES };

#define GRAMMAR_FILE_NAME L"game:\\Media\\Grammars\\example_grammar.cfg"

// Minimum allowable confidence level for speech recognition,
// This is done for the sample only, generally we recommend to tune confidenve values at a finer level.
static const float g_fMinimumConfidence = 0.10f;

// Console colors
static const DWORD g_ConsoleBackgroundColor = 0xFF1F005F; //Blue
static const DWORD g_ConsoleTextColor = 0xFFFFFFFF; //White

// ID value we use to identify each grammar.  This is for book keeping and each grammar needs a unique identifier in NuiSpeech
static const DWORD NO_GRAMMAR = 0;
static const DWORD DYNAMIC_GRAMMAR = 1;
static const DWORD COMPILED_GRAMMAR = 2;


//--------------------------------------------------------------------------------------
// Name: class DynamicSpeechApp
// Desc: Class for the application SimpleSpeechRecognition.
//--------------------------------------------------------------------------------------

class DynamicSpeechApp {
public:
    DynamicSpeechApp();
    ~DynamicSpeechApp() {}
    
    HRESULT CreateNuiSpeech();
    VOID ProcessRecoResult( const NUI_SPEECH_RECORESULT* pRR );
    VOID ProcessRecognitionEvent( const NUI_SPEECH_EVENT* pEvent );
    HRESULT SwapGrammar( const DWORD dwSwapToDynamic);
    
    HRESULT Update();
    inline VOID PrintStartText() { m_Console.Format( g_StartAppText ); }
    static BOOL IsMicrophoneConnected(void);
    VOID RequestMECCalibrationIfNeeded();
    VOID BindNotifications();
private:
    
    VOID DrainNotifications();    
    VOID EmitGrammarDescription();
    HRESULT DynamicSpeechCreation();
    HRESULT CreateSimpleRule();
    HRESULT CreateRuleWithOverride();   
    HRESULT CreateRuleWithMultipleItems();
    HRESULT CreateRuleUsingOtherRules();
    HRESULT EnableAllRules();
    VOID InitConsole();
    ATG::Console m_Console;             // TTY Display for demo purposes

    HANDLE m_hXNotifyListener;          // Handle for notifications
    BOOL m_fUIVisible;                  // Keeps track of UI visibility

    // Speech Recognition state    
    NUI_SPEECH_GRAMMAR m_Grammar;       // Grammar instance

    DWORD m_loadedGrammar;
    

};

//--------------------------------------------------------------------------------------
// Name: DynamicSpeechApp()
// Desc: Initializes the app and starts up the console
//--------------------------------------------------------------------------------------
DynamicSpeechApp::DynamicSpeechApp() :
    m_hXNotifyListener( NULL ),
    m_fUIVisible( FALSE ),    
    m_loadedGrammar(NO_GRAMMAR)
{
    InitConsole();
}


//--------------------------------------------------------------------------------------
// Name: VOID InitConsole()
// Desc: Initializes the ATG::Console, only used internally for startup
//--------------------------------------------------------------------------------------
VOID DynamicSpeechApp::InitConsole()
{
    //
    // Initialize the console window
    //
    m_Console.Create( FONT_FILE_NAME, g_ConsoleBackgroundColor, g_ConsoleTextColor );
    m_Console.SendOutputToDebugChannel( TRUE );
}

//--------------------------------------------------------------------------------------
// Name: HRESULT DynamicSpeechApp::CreateSimpleRule()
// Desc: This recreates the example_grammar.grxml grammar of the equivalent XML:
//
//      <item> cast fireball <tag>cast fireball</tag> </item>
//--------------------------------------------------------------------------------------
HRESULT DynamicSpeechApp::CreateSimpleRule()
{
    HRESULT hr = S_OK;
    NUI_SPEECH_GRAMMAR* pGrammar = &m_Grammar;
    LPCWSTR pRuleName;
    NUI_SPEECH_STATEHANDLE pState = NULL;
    NUI_SPEECH_SEMANTIC semantic;    
    DWORD dwOptions;  

    // Grab the rule name
    pRuleName = g_ruleNames[RULE_BASIC];  

    // Set the value returned by the tag
    semantic.pcwszValue = L"cast fireball";  

    // Since this is the first rule created, making it a root.  
    dwOptions = NUI_SPEECH_RULEOPTIONS_TOPLEVEL | NUI_SPEECH_RULEOPTIONS_ACTIVE | NUI_SPEECH_RULEOPTIONS_DYNAMIC | NUI_SPEECH_RULEOPTIONS_ROOT;        

    if ( SUCCEEDED(hr) ) hr = NuiSpeechCreateRule( pGrammar, pRuleName, dwOptions, TRUE, &pState);            
    if ( SUCCEEDED(hr) ) hr = NuiSpeechClearRule(pGrammar, pState);    
    if ( SUCCEEDED(hr) ) hr = NuiSpeechAddWordTransition(pGrammar, pState, NULL, L"cast fireball" , NULL, NUI_SPEECH_WORDTYPE_LEXICAL, 1.0,NULL); // &semantic);

    if ( FAILED( hr ) ) ATG::DebugSpew( "Failed to create simple rule.\n" );

    return hr;
}

//--------------------------------------------------------------------------------------
// Name: VOID DynamicSpeechApp::CreateRuleWithOverride()
// Desc: This recreates the example_grammar.grxml grammar of the equivalent XML:
//
// <rule id="override" scope="public">
//    <item>
//        <token sapi:display="Number 7" sapi:pron="s eh v ax n">seven</token>
//    </item>
//    <tag>The Number 7</tag>
// </rule>
//--------------------------------------------------------------------------------------
HRESULT DynamicSpeechApp::CreateRuleWithOverride()
{
    HRESULT hr = S_OK;
    NUI_SPEECH_GRAMMAR* pGrammar = &m_Grammar;
    LPCWSTR pRuleName;
    NUI_SPEECH_STATEHANDLE pState = NULL;
    NUI_SPEECH_SEMANTIC semantic;    
    DWORD dwOptions;  

    pRuleName = g_ruleNames[RULE_OVERRIDE];
    semantic.pcwszValue = L"The Number 7";
    dwOptions = NUI_SPEECH_RULEOPTIONS_TOPLEVEL | NUI_SPEECH_RULEOPTIONS_ACTIVE | NUI_SPEECH_RULEOPTIONS_DYNAMIC;

    if ( SUCCEEDED(hr) ) hr = NuiSpeechCreateRule( pGrammar, pRuleName, dwOptions, TRUE, &pState);            
    if ( SUCCEEDED(hr) ) hr = NuiSpeechClearRule(pGrammar, pState);    
    // For example, override default pronunciation of Seven.
    // US phonemes can be found at http://msdn.microsoft.com/en-us/library/bb813894.aspx
    if ( SUCCEEDED(hr) ) hr = NuiSpeechAddWordTransition(pGrammar, pState, NULL, L"/Number 7/seven/s eh v ax n;" , NULL, NUI_SPEECH_WORDTYPE_LEXICAL, 1.0, NULL); //&semantic);
    
    if ( FAILED( hr ) ) ATG::DebugSpew( "CreateRuleWithOverride() failed.\n" );
    return hr;
}

//--------------------------------------------------------------------------------------
// Name: VOID DynamicSpeechApp::CreateRuleWithMultipleItems()
// Desc: This recreates the example_grammar.grxml grammar of the equivalent XML:
//
//  <rule id="alternatives" scope="public">
//    <one-of>
//        <item>Affirmative</item>
//        <item>Correct</item>
//     </one-of>
//     <tag>Correct</tag>
//  </rule>
//--------------------------------------------------------------------------------------
HRESULT DynamicSpeechApp::CreateRuleWithMultipleItems()
{
    HRESULT hr = S_OK;
    NUI_SPEECH_GRAMMAR* pGrammar = &m_Grammar;
    LPCWSTR pRuleName;
    NUI_SPEECH_STATEHANDLE pState = NULL;
    NUI_SPEECH_SEMANTIC semantic;    
    DWORD dwOptions; 
    pRuleName = g_ruleNames[RULE_ALTERNATIVES];
    semantic.pcwszValue = L"Correct";
    dwOptions = NUI_SPEECH_RULEOPTIONS_TOPLEVEL | NUI_SPEECH_RULEOPTIONS_ACTIVE | NUI_SPEECH_RULEOPTIONS_DYNAMIC;

    if ( SUCCEEDED(hr) ) hr = NuiSpeechCreateRule( pGrammar, pRuleName, dwOptions, TRUE, &pState);           
    if ( SUCCEEDED(hr) ) hr = NuiSpeechClearRule(pGrammar, pState);
    if ( SUCCEEDED(hr) ) hr = NuiSpeechAddWordTransition(pGrammar, pState, NULL, L"Correct", NULL, NUI_SPEECH_WORDTYPE_LEXICAL, 1.0, &semantic);
    if ( SUCCEEDED(hr) ) hr = NuiSpeechAddWordTransition(pGrammar, pState, NULL, L"Affirmative",  NULL, NUI_SPEECH_WORDTYPE_LEXICAL, 1.0, &semantic);

    if ( FAILED( hr ) ) ATG::DebugSpew( "CreateRuleWithMultipleItems() failed.\n" );
    return hr;
}

//--------------------------------------------------------------------------------------
// Name: VOID DynamicSpeechApp::CreateRuleUsingOtherRules()
// Desc: This recreates the example_grammar.grxml grammar of the equivalent XML:
//
//  <rule id="action" scope="public">
//    <one-of>
//        <item>point</item>
//        <item>jump</item>
//        <item>go</item>
//    </one-of>
//    <tag>actions</tag>
//  </rule>
//  <rule id="direction" scope="public">
//    <one-of>
//        <item>up</item>
//        <item>down</item>
//        <item>left</item>
//        <item>right</item>
//    </one-of>
//    <tag>directions</tag>
//  </rule>
//  <rule id="reuse" scope="public">
//    <ruleref uri="#action"/>
//    <ruleref uri="#direction"/>
//    <tag>reused</tag>
//  </rule>
//--------------------------------------------------------------------------------------
HRESULT DynamicSpeechApp::CreateRuleUsingOtherRules()
{
    HRESULT hr = S_OK;
    NUI_SPEECH_GRAMMAR* pGrammar = &m_Grammar;
    LPCWSTR pRuleName;
    NUI_SPEECH_SEMANTIC semantic;    
    DWORD dwOptions;  
        
    dwOptions = NUI_SPEECH_RULEOPTIONS_TOPLEVEL | NUI_SPEECH_RULEOPTIONS_ACTIVE | NUI_SPEECH_RULEOPTIONS_DYNAMIC;    

    // actions
    // Note that this subrule does not have to be a toplevel rule, the sample has it as a top level rule
    NUI_SPEECH_STATEHANDLE actions = NULL;
    semantic.pcwszValue = g_ruleNames[RULE_ACTION];

    if ( SUCCEEDED(hr) ) hr = NuiSpeechCreateRule( pGrammar, L"action", dwOptions, TRUE, &actions);            
    if ( SUCCEEDED(hr) ) hr = NuiSpeechClearRule(pGrammar, actions);
    if ( SUCCEEDED(hr) ) hr = NuiSpeechAddWordTransition(pGrammar, actions, NULL, L"point", NULL, NUI_SPEECH_WORDTYPE_LEXICAL, 1.0f, &semantic);
    if ( SUCCEEDED(hr) ) hr = NuiSpeechAddWordTransition(pGrammar, actions, NULL, L"jump", NULL, NUI_SPEECH_WORDTYPE_LEXICAL, 1.0f, &semantic);
    if ( SUCCEEDED(hr) ) hr = NuiSpeechAddWordTransition(pGrammar, actions, NULL, L"go", NULL, NUI_SPEECH_WORDTYPE_LEXICAL, 1.0f, &semantic);

    //// directions
    // Note that this subrule does not have to be a toplevel rule, the sample has it as a top level rule
    NUI_SPEECH_STATEHANDLE directions = NULL; 
    semantic.pcwszValue = g_ruleNames[RULE_DIRECTION];

    if ( SUCCEEDED(hr) ) hr = NuiSpeechCreateRule( pGrammar, L"direction", dwOptions, TRUE, &directions);        
    if ( SUCCEEDED(hr) ) hr = NuiSpeechClearRule(pGrammar, directions);    
    if ( SUCCEEDED(hr) ) hr = NuiSpeechAddWordTransition(pGrammar, directions, NULL, L"up", NULL, NUI_SPEECH_WORDTYPE_LEXICAL, 1.0f, &semantic);
    if ( SUCCEEDED(hr) ) hr = NuiSpeechAddWordTransition(pGrammar, directions, NULL, L"down", NULL, NUI_SPEECH_WORDTYPE_LEXICAL, 1.0f, &semantic);
    if ( SUCCEEDED(hr) ) hr = NuiSpeechAddWordTransition(pGrammar, directions, NULL, L"left", NULL, NUI_SPEECH_WORDTYPE_LEXICAL, 1.0f, &semantic);
    if ( SUCCEEDED(hr) ) hr = NuiSpeechAddWordTransition(pGrammar, directions, NULL, L"right", NULL, NUI_SPEECH_WORDTYPE_LEXICAL, 1.0f, &semantic);

    // Combine
    NUI_SPEECH_STATEHANDLE pState = NULL;
    NUI_SPEECH_STATEHANDLE pNewState = NULL;    
    semantic.pcwszValue = g_ruleNames[RULE_REUSE];
    pRuleName = g_ruleNames[RULE_REUSE];

    if ( SUCCEEDED(hr) ) hr = NuiSpeechCreateRule( pGrammar, pRuleName, dwOptions, TRUE, &pState);   
    if ( SUCCEEDED(hr) ) hr = NuiSpeechClearRule(pGrammar, pState);
    if ( SUCCEEDED(hr) ) hr = NuiSpeechCreateState(pGrammar, pState, &pNewState);
    if ( SUCCEEDED(hr) ) hr = NuiSpeechAddRuleTransition(pGrammar, pState, pNewState, actions, 1.0f, &semantic);        
    if ( SUCCEEDED(hr) ) hr = NuiSpeechAddRuleTransition(pGrammar, pNewState, NULL, directions, 1.0f, &semantic);       

    if ( FAILED( hr ) ) ATG::DebugSpew( "CreateRuleUsingOtherRules() failed.\n" );
    return hr;     
}    


//--------------------------------------------------------------------------------------
// Name: VOID DynamicSpeechCreation()
// Desc: This recreates the example_grammar.grxml grammar section by section.
//
//--------------------------------------------------------------------------------------
HRESULT DynamicSpeechApp::DynamicSpeechCreation()
{
    HRESULT hr = S_OK;

    ULONG iGrammarId =  DYNAMIC_GRAMMAR;    
    NUI_SPEECH_GRAMMAR* pGrammar = &m_Grammar;
    
    if ( SUCCEEDED(hr) ) hr = NuiSpeechCreateGrammar( iGrammarId, pGrammar);    
    
    // Create each rule section
    if ( SUCCEEDED(hr) ) hr = CreateSimpleRule();
    if ( SUCCEEDED(hr) ) hr = CreateRuleWithOverride();   
    if ( SUCCEEDED(hr) ) hr = CreateRuleWithMultipleItems();
    if ( SUCCEEDED(hr) ) hr = CreateRuleUsingOtherRules();
    if ( SUCCEEDED(hr) ) hr = NuiSpeechCommitGrammar(pGrammar);

    if ( FAILED( hr ) ) ATG::DebugSpew( "DynamicSpeechCreation() failed.\n" );
    
    return hr;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: DynamicSpeechApp::EnableAllRules
// Desc: Function simply enalbes every rule of the grammar.  Rule names are the same in this instance across both grammars
//----------------------------------------------------------------------------------------------------------------------
HRESULT DynamicSpeechApp::EnableAllRules()
{
    HRESULT hr;
    for(int iIndex = 0; iIndex < g_numberOfRulesToEnable; ++iIndex)
    {        
        hr = NuiSpeechSetRuleState(&m_Grammar, g_ruleNames[iIndex], NUI_SPEECH_RULESTATE_ACTIVE);    
        if( FAILED( hr ) )
        {
            ATG::DebugSpew( "Failed to set rule state.\n" );
        } 
    }

    hr = NuiSpeechSetGrammarState(&m_Grammar, NUI_SPEECH_GRAMMARSTATE_ENABLED);
    if( FAILED( hr ) )
    {
        ATG::DebugSpew( "Failed to set grammar state.\n" );
    } 
    hr = NuiSpeechStartRecognition();
    return hr;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: DynamicSpeechApp::SwapGrammar
// Desc: Loads a grammar file, reinitializing the speech engine so that it uses the correct speech model according to
//       the grammar file's language.
//----------------------------------------------------------------------------------------------------------------------
HRESULT DynamicSpeechApp::SwapGrammar(const DWORD dwSwapToDynamic)
{
    HRESULT hr;

    if (dwSwapToDynamic == m_loadedGrammar)
    {
        // no swap needed;
        return S_OK;
    }    

    // If we already had a grammar loaded, we need to shutdown.
    if ( m_loadedGrammar != NO_GRAMMAR )
    {
        hr = NuiSpeechUnloadGrammar( &m_Grammar );
        if ( FAILED( hr ) )
        {
            ATG::DebugSpew( "Failed to unload grammar.\n" );
        }
        else
        {
            m_Console.Format( "\nSuccessfully unloaded grammar.\n" );
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

        m_loadedGrammar = NO_GRAMMAR;
    }

    // create the local speech engine
    NUI_SPEECH_INIT_PROPERTIES props;
    ZeroMemory( &props, sizeof(props) );
    props.Language = NUI_SPEECH_LANGUAGE_EN_US;
    props.MicrophoneType = NUI_SPEECH_KINECT;
    DWORD dwHardwareThread = NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD;
    hr = NuiSpeechEnable( &props, dwHardwareThread );
    if( FAILED( hr ) )
    {
        m_Console.Format( "Failed to initialize Nui Speech API.\n" );
        return hr;
    }

    // Initialize event notifications
    const ULONG eventInterest = NUI_SPEECH_ALL_EVENTS;
    hr = NuiSpeechSetEventInterest( eventInterest );
    if( FAILED( hr ) )
    {
        m_Console.Format( "Failed to set event interest.\n" );
        return hr;
    }    

    if (dwSwapToDynamic == DYNAMIC_GRAMMAR)
    {
        hr = DynamicSpeechCreation();    
        if( FAILED( hr ) )
        {
            m_Console.Format( "Failed to load dynamic grammar.\n" );
            m_loadedGrammar = NO_GRAMMAR; 
            return hr;
        }   
        else
        {
            m_loadedGrammar = DYNAMIC_GRAMMAR;
            m_Console.Format( "Successfully loaded dynamic grammar.\n" );        
        }
    }
    else if (dwSwapToDynamic == COMPILED_GRAMMAR)
    {
        hr = NuiSpeechLoadGrammar( GRAMMAR_FILE_NAME, COMPILED_GRAMMAR, NUI_SPEECH_LOADOPTIONS_STATIC, &m_Grammar );
        if( FAILED( hr ) )
        {
            m_Console.Format( "\nFailed to load Grammar file %s. No grammar loaded.\n\n", GRAMMAR_FILE_NAME );

            // For the simplicity of the sample, we want to keep rolling if a bad file loads, so continue from here as if we succeeded.
            // Clean up
            hr = NuiSpeechDisable();
            if ( FAILED( hr ) )
            {
                ATG::DebugSpew( "Failed to disable speech engine.\n" );
                return hr;
            }
            m_loadedGrammar = NO_GRAMMAR;        
            return S_OK;
        }
        
        m_Console.Format( "Successfully loaded compiled grammar.\n" );
        m_loadedGrammar = COMPILED_GRAMMAR;        
    }
    else
    {
        // other grammar
        return E_FAIL;
    }

    // Enable all rules for the loaded grammar
    hr = EnableAllRules();
    if( FAILED( hr ) )
    {
        m_Console.Format( "Failed to enable all rules.\n" );
        return hr;
    }        

    EmitGrammarDescription();    

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
VOID DynamicSpeechApp::ProcessRecoResult(const NUI_SPEECH_RECORESULT* pRR )
{
    // We preferentially use the Semantic Property Confidence value vs. the Rule confidence
    // value as this gives better results for false accepts/false rejects than the
    // Rule confidence.

    // NOTE: In your title, you may wish to utilize different minimum confidence values
    // for different languages, grammars, phrases. This can give better results.  
    // This is done only for simplicity.
    if ( !pRR->Phrase.pSemanticProperties || pRR->Phrase.pSemanticProperties->fSREngineConfidence < g_fMinimumConfidence )
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


//--------------------------------------------------------------------------------------
// Name: ProcessRecognitionEvent()
// Desc: Process the recognition result and print to console.
//--------------------------------------------------------------------------------------
VOID DynamicSpeechApp::ProcessRecognitionEvent( const NUI_SPEECH_EVENT* pEvent )
{
    switch(pEvent->eventId)
    {        
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
        // These events are not important for this sample
        // A bookmark was reached on the input stream (Application defined)
        case NUI_SPEECH_EVENT_BOOKMARK:        
        // Stream of audio data is coming in (Does not mean there is actual sound coming in)
        case NUI_SPEECH_EVENT_START_STREAM:        
        // Stream of audio data has ended (Processing is the next logical step)
        case NUI_SPEECH_EVENT_END_STREAM:
        // Stream of non-silence audio data is coming in
        case NUI_SPEECH_EVENT_SOUND_START:
        // Stream of silence has begun
        case NUI_SPEECH_EVENT_SOUND_END:
        // A speech phrase was detected and has begun
        case NUI_SPEECH_EVENT_PHRASE_START:
        // A guess has been made as to the word spoken. (Partial phrase recognized)
        case NUI_SPEECH_EVENT_HYPOTHESIS:            
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
HRESULT DynamicSpeechApp::Update()
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
        // NuiSpeech will handle unplugging and repluggin hardware but for the sample
        // we wait till a Kinect is plugged in.
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
        HRESULT hr = S_OK;

        if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        {
            m_Console.Format( g_HelpText );
        }
        else if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            hr = SwapGrammar(COMPILED_GRAMMAR);
        }
        else if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        {
            hr = SwapGrammar(DYNAMIC_GRAMMAR);            
        }

        if ( FAILED( hr ) )
        {
            ATG::FatalError( "Error while switching grammar files\n" );
        }

        if ( m_loadedGrammar != NO_GRAMMAR )
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
    } while (!fDone);
    return 0;
}        


//----------------------------------------------------------------------------------------------------------------------
// Name: DynamicSpeechApp::EmitGrammarDescription
// Desc: Outputs a brief description for the user regarding the selected grammar file.
//----------------------------------------------------------------------------------------------------------------------
VOID DynamicSpeechApp::EmitGrammarDescription()
{    
    m_Console.Format( g_phrases );
    // Finally, show the help text.
    m_Console.Format( g_HelpText );
}

//----------------------------------------------------------------------------------------------------------------------
// Name: DynamicSpeechApp::RequestMECCalibrationIfNeeded
// Desc: Checks to see if we have a MEC Calibration file on the system - if not, the user should be asked to perform
//       MEC calibration.
//----------------------------------------------------------------------------------------------------------------------
void DynamicSpeechApp::RequestMECCalibrationIfNeeded()
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
// Name: DynamicSpeechApp::DrainNotifications
// Desc: Drains all pending notifications (updating the system-ui displayed state)
//----------------------------------------------------------------------------------------------------------------------
void DynamicSpeechApp::DrainNotifications()
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
// Name: DynamicSpeechApp::BindNotifications
// Desc: Starts listening for notifications from the system.
//----------------------------------------------------------------------------------------------------------------------
void DynamicSpeechApp::BindNotifications()
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
    DynamicSpeechApp TheApp;

    TheApp.BindNotifications();

    // Initialize the speech engine with the available microphone
    hr = TheApp.SwapGrammar(DYNAMIC_GRAMMAR);
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
