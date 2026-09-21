//--------------------------------------------------------------------------------------
// JobScheduler.cpp
//
// XNA Developer Connection.
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
 
#include "Job.h"
#include "SchedulerBuilder.h"
 

//--------------------------------------------------------------------------------------
// Name: FibonacciJob
//
// Desc: FibonacciJob is a concrete implementation derived from the abstract Job class
//       which is the basic unit of work the JobScheduler can understand.
//
//       FibonacciJob implements a recursive algorithm to compute the number 
//       corresponding to a specific position in the Fibonacci sequence.

// Construction:
// Requesting the number corresponding to position 0 or 1 in the Fibonacci sequence will 
// not produce child jobs and, thus can be used to test processing independent task by 
// the  JobScheduler.
//
// Setting ExtraDelay to a value greater than zero at construction time will guarantee
// that the Job will take at least that amount of time to process the job. The total 
// amount of time is divided as follow:
//     1/8 of the total ExtraTime is spent in the Reset function.
//     3/4 of the total ExtraTime is spent in operator ().
//     1/8 of the total ExtraTime is spent in NotifyComplete.
//     1/8 of the total ExtraTime is spent in DisplayResult. DisplayResult is called 
//         only for Top level FibonacciJob objects and will not be called for child jobs.
//
// If a task is created with a font, the font will be used to display the result on  
// screen. The font is not passed to child jobs, meaning that only the top level jobs can 
// use this mechanism.
//
// Member functions of interest
//
// operator (): This member function is called by the JobScheduler when it is time to 
// execute this specific job object. Usually a WorkerThread object will call the function 
// but may also be called by the LoadBalancer or the Scheduler object depending on the 
// configuration of the JobScheduler. It is the only mandatory function for a class 
// deriving from a Job class.
//
// NotifyComplete: This member function is called after a child job has indicated to the 
// JobScheduler that it has competed. A FibonacciJob object has two child jobs that will
// report back to it, unless it is a terminal object, in which case it has none and this 
// function will not be called. When the last of its child job has reported back, a 
// FibonacciJob instance will compute the answer and signal the JobScheduler that its work 
// is done. This will trigger a call to its own parent's NotifyComplete member function, 
// assuming it is not a root object and it does have a parent.
//
// DisplayResult: This member function is called on the root class only, once all child 
// jobs have completed. Internally, it calls the ATG::FONT object passed at construction 
// to display the answer on screen. It is safe to call the shared font object inside 
// DisplayResult because the JobScheduler ensures it executes on the main thread.
//
// Reset: Reset the job to its not executed state. It is used between frames to reset a 
// job to its initial state without having to destroy and reconstruct it.
//
// Try this:
// Try using the ImmediateContext class to set up a cutoff point at which recursive 
// task are executed locally instead of being dispactch. See how it affects the 
// performance of the system.
//
// Try variying the proportion of time spent in reset, operator (), NotifyComplete and 
// DisplayResult and see how it affect the perfomances.
//
// SetMaxPendingJobs controls the size of the job queue used to send job to a 
// WorkerThread. Try changing the size and see how it affects the performances.
//
// Fire up the Performance Investigator for Xbox (PIX) while running a Profile build of 
// the sample and use Record Timing (Tm) to capture a few frames (2 or 3). Examine how 
// the jobs are spread over the hardware threads. Use the controller to change some of 
// the sample settings and do another capture. Compare the results.
//
// Note: That the result from FibonacciJob is a number from the Fibonacci sequence is 
// of no importance for this sample and so is the fact that it uses a very inefficient 
// algorithm to compute it. What is of interest is that it does so using a recursive 
// algorithm which helps demonstrate how hierarchical dependencies between jobs can be
// effectively managed.
//--------------------------------------------------------------------------------------
class FibonacciJob : public Job
{
public:
    FibonacciJob( UINT nPosition, FLOAT fExtraDelay = 0, ATG::Font* Font = 0 );
    virtual ~FibonacciJob();
 
    void Reset();
    UINT GetResult() const { return m_nResult; }


private:
    // These functions will be called in turn when it is the right time to execute them 
    // and on the thread where they supose to  execute.
    virtual JOBRESULT operator () ( Context& context );

    static JOBRESULT NotifyCompleted( Context& context, Job* pOriginator, void* pData );

    static JOBRESULT DisplayResult( Context& context, void* pData );
 
    void WasteCycles( FLOAT fFactor );

    // Pointers to child jobs.
    FibonacciJob* m_pFibSubJob1;
    FibonacciJob* m_pFibSubJob2;
    
    // Function to call upon completion.
    NotifyCallback m_CompletionCallback;

    // Function to call as last step.
    FinalizeFunction m_FinalizeFunction; 
 
    UINT m_nPosition; // The position in the Fibonacci sequence of the number we are 
                     // looking for.
    UINT m_nResult;   // The number corresponding to the position.

    //Number of sub jobs that have reported back with a completed operation.
    UINT m_nCompletedJobCount;

    // Delay and a timer to mesure time to delay.
    FLOAT m_fExtraDelay;
    ATG::Timer m_Timer;

    // If set, will be used during Finalize to display the result.
    ATG::Font* m_pFont;
}; 


//--------------------------------------------------------------------------------------
// Name: FibonacciJob::FibonacciJob
// Desc: Constructs a FibonacciJob object
//--------------------------------------------------------------------------------------
FibonacciJob::FibonacciJob( UINT nPosition, FLOAT fExtraDelay, ATG::Font* pFont )
:m_pFibSubJob1( 0 ),
 m_pFibSubJob2( 0 ),
 m_nPosition( nPosition ),
 m_nResult( 0 ),
 m_nCompletedJobCount( 0 ),
 m_fExtraDelay( fExtraDelay ),
 m_pFont( pFont )
{
	// Allocate memory for sub jobs that will be spawned at run time.
    if( m_nPosition >= 2 )
    {
        m_pFibSubJob1 = new FibonacciJob( m_nPosition - 2, m_fExtraDelay );
        m_pFibSubJob2 = new FibonacciJob( m_nPosition - 1, m_fExtraDelay );
    }
 
    // If someone is setting a font, it means that we'll have to call it from Main, and
    // that means during the finalize call.
    if( m_pFont != NULL )
    {
        m_FinalizeFunction = 
            FinalizeFunction( &FibonacciJob::DisplayResult, this, this );
        SetFinalizeFunction( &m_FinalizeFunction );
    }
}


//--------------------------------------------------------------------------------------
// Name: FibonacciJob::~FibonacciJob
// Desc: Releases all resources used by a FibonacciJob object.
//--------------------------------------------------------------------------------------
FibonacciJob::~FibonacciJob( )
{
    delete m_pFibSubJob1;
    delete m_pFibSubJob2;
}


//--------------------------------------------------------------------------------------
// Name: FibonacciJob::Reset
// Desc: Reset the job to its initial state. Called between frames to reset the job.
//--------------------------------------------------------------------------------------
void FibonacciJob::Reset()
{
    WasteCycles( 0.125 );
 
    m_nResult = 0;
    m_nCompletedJobCount = 0;

    ResetJobStatus(); // Don't forget to reset the job's status too!
}
 

//--------------------------------------------------------------------------------------
// Name: FibonacciJob::operator ()
// Desc: Called by the JobScheduler when it is time to execute a specific job.
//--------------------------------------------------------------------------------------
JOBRESULT FibonacciJob::operator ()( Context& ExecutionContext )
{
    // Ensure that operator() takes at least 3/4 of the total execution time.
    WasteCycles( 0.75 );
 

    if( m_nPosition >= 2 )
    {
        // Set up the notification call back for this object.
        m_CompletionCallback = 
            NotifyCallback( &FibonacciJob::NotifyCompleted, 
                            this, 
                            ExecutionContext.GetThreadID(),
                            this );

        m_pFibSubJob1->Reset();
        m_pFibSubJob1->SetNotifyCallback( &m_CompletionCallback );

        //Dispatching a new job means different things depending on which thread this
        //job is executing. Contexts provide a level of abstraction to do just that 
        // type of thing.
        ExecutionContext.Dispatch( m_pFibSubJob1 ); 
 
        m_pFibSubJob2->Reset();
        m_pFibSubJob2->SetNotifyCallback( &m_CompletionCallback );
        ExecutionContext.Dispatch( m_pFibSubJob2 );
       
        //Wait for result, means this job will float in the ether until it returns 
        // completed when called back to Notify.
        return JOBRESULT_WAIT;
    }
    else
    {
        m_nResult = 1;
        // Returning JOBRESULT_COMPLETED will signal its parent that this job has 
        // completed, if it is a top level job and there is a finalize function 
        // to call, it will get scheduled for exectuion on the main thread.
        return JOBRESULT_COMPLETED;
    }
}


//--------------------------------------------------------------------------------------
// Name: FibonacciJob::NotifyCompleted
// Desc: Called when a child job as completed.
//--------------------------------------------------------------------------------------
JOBRESULT FibonacciJob::NotifyCompleted( Context& context, Job* pOriginator, void* pData )
{ 
    FibonacciJob* pFib = ( FibonacciJob* )pData;
    assert( pFib->m_nCompletedJobCount == NULL || pFib->m_nCompletedJobCount == 1 );

    pFib->m_nResult += ( ( FibonacciJob* ) pOriginator )->GetResult();
    pFib->m_nCompletedJobCount ++;

    if( pFib->m_nCompletedJobCount == 2 )
    {
        pFib->WasteCycles( 0.125 );

         //All done, notify parents and or execue finalize as appropriate.
        return JOBRESULT_COMPLETED; 
    } 
    else
    {
        return JOBRESULT_WAIT; // Waiting for second child job to complete.
    }
}

//--------------------------------------------------------------------------------------
// Name: DisplayResult
// Desc: Called by the JobScheduler (always on main thread) when all child jobs have
//       completed to display the answer on screen.
//--------------------------------------------------------------------------------------
JOBRESULT FibonacciJob::DisplayResult( Context& context, void* pData )
{
    FibonacciJob* pFib = ( FibonacciJob* )pData;

    // Spend about 1/8 of the total job time in Display.
    pFib->WasteCycles( 0.125 );
 
    // All jobs are sharing the same font object. It is important that calls are made 
    // from the main thread to avoid concurency issues. The same happens when multiple 
    // jobs need to make GPU calls.
    pFib->m_pFont->Begin();
    pFib->m_pFont->SetScaleFactors( 1.2f, 1.2f );
    static WCHAR szBuffer[ 128 ];
    swprintf_s( szBuffer, L"Oh! And the answer is: %d.", pFib->m_nResult );
    pFib->m_pFont->DrawText( 0, 350, 0xffffffff, szBuffer );
    pFib->m_pFont->End();
 
    return JOBRESULT_COMPLETED; // We are done, totally done.
}


//--------------------------------------------------------------------------------------
// Name: WasteCycles
// Desc: Ensures that the job takes the minimum amount of time set to compute.
//--------------------------------------------------------------------------------------
void FibonacciJob::WasteCycles( FLOAT fFactor )
{
    if( m_fExtraDelay > 0 )
    {
        FLOAT fDelayTime = m_fExtraDelay * fFactor;
        m_Timer.Reset();

        while( fDelayTime > ( FLOAT )m_Timer.GetAppTime() );
    }
}


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON, 
      ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_START_BUTTON, 
      ATG::HELP_PLACEMENT_1, L"Pause" },
    { ATG::HELP_A_BUTTON, 
      ATG::HELP_PLACEMENT_1, L"JobScheduler on/off" },
    { ATG::HELP_B_BUTTON, 
      ATG::HELP_PLACEMENT_1, L"Run load balancer on main thread on/off" },
    { ATG::HELP_Y_BUTTON, 
      ATG::HELP_PLACEMENT_1, L"Decrease compute time by .25 ms" },
    { ATG::HELP_X_BUTTON, 
      ATG::HELP_PLACEMENT_1, L"Increase compute time by .25 ms" },
    { ATG::HELP_DPAD, 
      ATG::HELP_PLACEMENT_1, 
      L"Left - Right adjust Fibonacci postion to compute\nUp - Down adjust the number of top level jobs" },
    { ATG::HELP_LEFT_SHOULDER, 
      ATG::HELP_PLACEMENT_1, L"Decrease the number of extra threads" },
    { ATG::HELP_RIGHT_SHOULDER, 
      ATG::HELP_PLACEMENT_1, L"Increase the number of extra threads" },
    { ATG::HELP_LEFT_STICK, 
      ATG::HELP_PLACEMENT_1, L"Extra threads can sleep on/off" },
    { ATG::HELP_RIGHT_STICK, 
      ATG::HELP_PLACEMENT_1, L"Use main thread to process jobs on/off" },
};
static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE(g_HelpCallouts);
 

//--------------------------------------------------------------------------------------
// Name: ComputeNumberOfJobs
// Desc: Utility function that return the number of jobs that will be required to 
//       compute a value for a specific sequence.
//--------------------------------------------------------------------------------------
UINT ComputeNumberOfJobs( UINT FibonacciPos )
{
    if( FibonacciPos == 0 || FibonacciPos == 1 )
        return 1;
    else
        return ComputeNumberOfJobs( FibonacciPos - 2 ) + 
               ComputeNumberOfJobs( FibonacciPos - 1 ) + 1;
};
 

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
private:
    // See the initialize function bellow for the documentation on these variable.
    ATG::Timer          m_Timer;
    ATG::Font           m_Font;
    ATG::PackedResource m_Resource;
    ATG::Help           m_Help;
    BOOL                m_bDrawHelp;
    FibonacciJob**      m_ppFibArray;
    UINT                m_FibPosition;
    UINT                m_FibJobCount;
    FLOAT               m_fFibMinimumTime;
    UINT                m_NumExtraThreads;
    Scheduler*          m_pScheduler;
    SchedulerBuilder    m_Builder;
    BOOL                m_bUsingScheduler;
    BOOL                m_bLbOnMain;
    UINT                m_FibRootJobs;
    BOOL                m_bUseMain;
    BOOL                m_bThreadsMaySleep;
 
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
 
    void DeleteAllJobs();
};
 
 
//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, 
                           &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();
}
 
 
//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;
 
    // User controlled settings and default values.
    m_FibPosition      = 9;       // The position inside the Foibonacci sequence for 
                                  // which we are seeking an answer.

    m_fFibMinimumTime  = 0.00025; // Minimum amount of time that a Fibonacci job will 
                                  // take for processing.in seconds, default to .25 ms.

    m_FibRootJobs      = 1;       // Number of top level jobs.


    m_NumExtraThreads  = 5;       // Number of extra threads taht will be used. (0 to 5)

    m_bUsingScheduler  = TRUE;    // If set to false, the jobs will be executed directly, 
                                  // without even going throught the scheduler.

    m_bLbOnMain        = TRUE;    // Specifies if the load balancer should run on 
                                  // main or not. If 0 extra thread, then it will have 
                                  // to run on main whatever the value here says.

    m_bUseMain         = TRUE;    // If the load balancer is running on its own thread, 
                                  // then it will use main as a worker thread, when 
                                  // possible.

    m_bThreadsMaySleep = FALSE;   // Sleep threads when idle?

    // Pointer to the scheduler
    m_pScheduler  = 0;

    // An array of all the top level job that will be computed.
    m_ppFibArray = 0;       

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
 
    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );
 
    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
 
    return S_OK;
}
 
 
//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{ 
    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();
 
    // Pause the timer
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        static BOOL bPaused = FALSE;
        bPaused = !bPaused;
 
        if( bPaused )   m_Timer.Stop();
        else            m_Timer.Start();
    }
 
    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;
 
    // Increase the number of top level jobs.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        if( m_FibRootJobs < 500 )
        {
            DeleteAllJobs();
            m_FibRootJobs += 20;
        }
    }
 
	// Decrease the number of top level jobs.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        if( m_FibRootJobs > 0 )
        {
            DeleteAllJobs();
 
            if( m_FibRootJobs <= 20 )
                m_FibRootJobs = 1;
            else
                m_FibRootJobs -= 20;
        }
    }
 
    // Increase the position of the entry to compute in the Fibonacci sequence.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
    {
        DeleteAllJobs();
        m_FibPosition += 1;
    }
 
    // Decrease the position of the entry to compute in the Fibonacci sequence.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
    {
        if( m_FibPosition > 0 )
        {
            DeleteAllJobs();
            m_FibPosition -= 1;
        }
    }
 
    // Increase the time a job takes to execute by .25 ms.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        DeleteAllJobs();
        m_fFibMinimumTime += 0.00025;
    }
 
    // Decrease the time a job takes to execute by .25 ms.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        DeleteAllJobs();
 
        m_fFibMinimumTime -= 0.00025;
        if( m_fFibMinimumTime < 0.0 )
            m_fFibMinimumTime = 0.0;
    }
 
    // Increase the number of extra threads to use.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        if( m_NumExtraThreads < 5)
        {
            m_NumExtraThreads ++;
            delete m_pScheduler;
            m_pScheduler = 0;
        }
    }
 
    // Decrease the number of extra threads to use.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
    {
        if( m_NumExtraThreads > 0)
        {
            m_NumExtraThreads --;
            delete m_pScheduler;
            m_pScheduler = 0;
        }
    }
 
    // Toggle between using and not using the scheduler to execute the jobs.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_bUsingScheduler = !m_bUsingScheduler;
    }
 
    // Toggle between running the load balancer on the main thread or on a 
    // dedicated hardware thread
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_bLbOnMain = !m_bLbOnMain;
        delete m_pScheduler;
        m_pScheduler = 0;
    }
 
    // Toggle between letting the worker threads go to sleep or staying awake
    // when there are no jobs to process.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_THUMB )
    {
        m_bThreadsMaySleep = !m_bThreadsMaySleep; 
        delete m_pScheduler;
        m_pScheduler = 0;
    }
 

    // When the load balancer 9is running on its own hardware thread, toggle 
    // between using the main thread to execute jobs or not using it.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_THUMB )
    {
        m_bUseMain = !m_bUseMain;
        delete m_pScheduler;
        m_pScheduler = 0;
    }
 
    if( m_ppFibArray == NULL )
    {
        // Find total number of jobs required to compute a specific number
        // from the Fibonacci sequence.
        m_FibJobCount = ComputeNumberOfJobs( m_FibPosition );
   
        // Set up the array of fibonacci numbers to compute
        m_ppFibArray = new FibonacciJob* [ m_FibRootJobs ];
        for( unsigned i( 0 ); i < m_FibRootJobs; ++ i )
            m_ppFibArray[ i ] = new FibonacciJob( m_FibPosition, 
                                                  m_fFibMinimumTime, 
                                                  &m_Font );
    }

    // Setup the scheduler
    // This is something that would usually be set only once during the game's 
    // initialization. 
    if( m_pScheduler == NULL )
    {
        m_Builder.SetUseMainThread( m_bUseMain );
        m_Builder.SetThreadsMaySleep( m_bThreadsMaySleep );

        if( m_NumExtraThreads >= 2 )
        {
            m_Builder.SetMaxPendingJobs( ( UINT )( ( ( m_FibRootJobs * m_FibJobCount ) / 
                                                     m_NumExtraThreads ) * 0.7 ) ); 
        }
        else
        {
            // When using 0 or 1 extra threads, there is really no need to throttle 
            // the worker threads. 
            m_Builder.SetMaxPendingJobs( ( UINT )( m_FibRootJobs * m_FibJobCount ) );
        }

        m_pScheduler = m_Builder.NewScheduler( m_NumExtraThreads, 
                                               m_FibRootJobs * m_FibJobCount, m_bLbOnMain );
    }

    // Send the jobs to the JobScheduler for scheduling and execution.
    if( m_bUsingScheduler )
    {
        for( unsigned i( 0 ); i < m_FibRootJobs; ++ i )
        {
            m_ppFibArray[ i ]->Reset();
            m_pScheduler->Dispatch( m_ppFibArray[ i ] );
        }
    }
 
    return S_OK;
}
 
 
//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );
 
    
    if( m_bUsingScheduler )
    {
        // Wait until all jobs have been executed.
        // The main thread is not sitting idle during this time though. It is processing the
        // Finalize methods and executing jobs when time permits.
        m_pScheduler->Sync();
    }
    else
    {
        // Let's run this outside the scheduler. Just to have some reference.
        for( unsigned i( 0 ); i < m_FibRootJobs; ++ i )
        {
            m_ppFibArray[ i ]->Reset();
            ImmediateExecute( m_ppFibArray[ i ] );
        }
    }

    // Show title, frame rate, settings and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        ATG::RenderBackground( 0xff0000ff, 0xff000000 );
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff,  L"JobScheduler" );
 
        static WCHAR szBuffer[ 128 ];
        swprintf_s( szBuffer, L"Computing %d times Fibonacci of %d", m_FibRootJobs, 
                                                                     m_FibPosition );
        m_Font.DrawText( 0, 40, 0xffffffff, szBuffer );
        if( m_FibPosition >= 2 )
            m_Font.DrawText( 30, 65, 0xffffffff, L"using a recursive algorithm." );
        swprintf_s( szBuffer, L"- Running %d jobs %s JobScheduler", 
                              m_FibRootJobs * m_FibJobCount, 
                              m_bUsingScheduler ? L"inside the" : L"outside of" );
        m_Font.DrawText( 30, 95, 0xffffffff, szBuffer );
        m_Font.DrawText( 30, 125, 0xffffffff,  L"- With a minimum compute time of" );
        swprintf_s( szBuffer, L"  %#.3f ms per job.", m_fFibMinimumTime * 1000 );
        m_Font.DrawText( 30, 150, 0xffffffff, szBuffer );
 
        if( m_bUsingScheduler == TRUE )
        {
            swprintf_s( szBuffer, L"- Using %d extra threads", m_NumExtraThreads );
            m_Font.DrawText( 30, 180, 0xffffffff, szBuffer );
            swprintf_s( szBuffer, L"- Load Balancing on thread %d", 
                                  m_bLbOnMain ? 0 : m_NumExtraThreads );
            m_Font.DrawText( 30, 210, 0xffffffff, szBuffer );
            if( m_bLbOnMain == FALSE )
            {
                if( m_bUseMain == TRUE )
                    swprintf_s( szBuffer, 
                                L"- Processing jobs on thread 0." );
                else
                    swprintf_s( szBuffer, L"- Not processing jobs on thread 0." );
                m_Font.DrawText( 60, 240, 0xffffffff, szBuffer );
            }
            if( m_bThreadsMaySleep == TRUE )
            {
                m_Font.DrawText( 30, 270, 0xffffffff, 
                                          L"- Extra threads may sleep when idle." ); 
            }
            else
            {
                m_Font.DrawText( 30, 270, 0xffffffff, L"- Extra threads never sleep." );
            }     
        }
 
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }
 
    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
 
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DeleteAllJobs()
// Desc: Safely delete all jobs in the job array.
//--------------------------------------------------------------------------------------
void Sample::DeleteAllJobs()
{
    if( m_ppFibArray != NULL )
    {
        for( unsigned i( 0 ); i < m_FibRootJobs; ++ i )
            delete m_ppFibArray[ i ];
 
        delete [] m_ppFibArray;
        m_ppFibArray = 0;
    }
}
