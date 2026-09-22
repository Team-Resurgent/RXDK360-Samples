//--------------------------------------------------------------------------------------
// Job.h
//
// This file contains everything needed to implement concrete job classes to be passed 
// to the JobScheduler for scheduling and execution.
//
// It contains:
// - An abstract Job class: from which to derive domain specific concrete implementation
//   classes that can be passed to the scheduler.
// - An abstract Context class that provides additional services to a job during its 
//   execution.
// - Callback wrapper classes that are multithreading aware and, among other things, 
//   allow jobs to send notifications messages between them.
//
// It also contains everything needed to execute jobs in immediate mode: in a 
// singlethreaded environment, outside the JobScheduler itself.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#ifndef JOB_H
#define JOB_H

#include <xtl.h>
#include <assert.h>


//--------------------------------------------------------------------------------------
// Name: JOBSTATUS
// Desc: The status of a job. 
//--------------------------------------------------------------------------------------
enum JOBSTATUS
{
    JOBSTATUS_NOTSTARTED, // The job has not been executed yet. It is ready for scheduling.
    JOBSTATUS_WAITING,    // The job has executed but is now awating notifications.
    JOBSTATUS_NOTIFY,     // The job has completed and must now notify another job.
    JOBSTATUS_FINALIZE,   // The job has completed and must now run its finalize function.
    JOBSTATUS_COMPLETED   // The job has completed
};


//--------------------------------------------------------------------------------------
// Name: JOBRESULT
// Desc: Value returned by a job after processing. 
//--------------------------------------------------------------------------------------
enum JOBRESULT
{
    // The job has completed and should advance to the next state.
    JOBRESULT_COMPLETED, 

    // The job is waiting for notification from child job.
    JOBRESULT_WAIT       
};


//--------------------------------------------------------------------------------------
// Name: THREADID
// Desc: Identifies a specific logical thread. 
//--------------------------------------------------------------------------------------
enum THREADID
{
    // WORKER 1 to 5 are used to access 0 base arrays. 
    // Do not change the other of declaration
    THREADID_WORKER1, 
    THREADID_WORKER2,
    THREADID_WORKER3,
    THREADID_WORKER4,
    THREADID_WORKER5,
    THREADID_MAXNUMWORKER,

    THREADID_MAIN,         // Always on thread 0 in this sample.
    THREADID_LOADBALANCER,
    THREADID_ANY,          // Job can be run on any thread. The load balancer will 
                           // select it.
    THREADID_NONE          
};


class Job; // Required by the Context class bellow.


//--------------------------------------------------------------------------------------
// Name: Context
// Desc: Abstract class that specify the interface of a context. The JobScheduler 
//       defines different concrete contexts for the different environment in which a 
//       job can execute.
//--------------------------------------------------------------------------------------
class Context
{
public:
    Context() {}
    virtual ~Context() {}

    // Sends a job to the JobScheduler for execution.
    virtual void Dispatch( Job* pJob ) = 0;

    // Returns the thread on which the job is curently executing.
    virtual THREADID GetThreadID() const = 0;

    // Validate wheither a job's (usually a child) state has changed. 
    // If so, scheduler it for more processing.  
    virtual void AwakenJob( Job* pJob ) = 0;

private:
    // Disabling copy constructor and assignment operator.
    Context( const Context& );
    Context& operator =( const Context& );
};


//--------------------------------------------------------------------------------------
// Name: NOTIFY_FPTR
// Desc: A pointer to a C function that will be called when a child job has completed.
//--------------------------------------------------------------------------------------
typedef JOBRESULT ( *NOTIFY_FPTR )( Context& ExececutionContext, // The execution context.
                                    Job* pNotifyingJob,          // The job sending the 
                                                                 // notification.
                                    void* pData );               // Extra data 


//--------------------------------------------------------------------------------------
// Name: NotifyCallback
// Desc: Encapsulates all the data needed to safely notify a parent job when the 
//       execution of a child job has completed in a multithreaded environment.
//--------------------------------------------------------------------------------------
class NotifyCallback
{
public:
    // Preferred constructor
    NotifyCallback( NOTIFY_FPTR pFunction,     // The function to be called.
                    Job* pWaitingJob,          // The job that is expecting the callback.
                    THREADID WaitingJobThread, // The thread on which the job is waiting.
                    void* pData )              // Additional data to be passed to 
                                               // pFunction.
        :m_pFunction( pFunction ), 
         m_pWaitingJob( pWaitingJob ), 
         m_WaitingJobThread( WaitingJobThread ),  m_pData( pData ) {}

    // Default constructor
    NotifyCallback() 
        :m_pFunction( 0 ), 
         m_pWaitingJob( 0 ), m_WaitingJobThread( THREADID_NONE ), m_pData( 0 ) {}

    // Copy constructor
    NotifyCallback( const NotifyCallback& notifyCallback ) 
        :m_pFunction( notifyCallback.m_pFunction ),
         m_pWaitingJob( notifyCallback.m_pWaitingJob ),
         m_WaitingJobThread( notifyCallback.m_WaitingJobThread ), 
         m_pData( notifyCallback.m_pData ) {}

    // Assignment
    NotifyCallback& operator =( const NotifyCallback& notifyCallback ) 
        { m_pFunction = notifyCallback.m_pFunction; 
          m_pWaitingJob = notifyCallback.m_pWaitingJob;
          m_WaitingJobThread = notifyCallback.m_WaitingJobThread; 
          m_pData = notifyCallback.m_pData; return *this; }

    // Desctructor
    ~NotifyCallback() {}

    // Called by the JobScheduler to execute the callback.
    inline JOBRESULT operator ()( Context& context, Job* pOriginator ) 
        { return ( *m_pFunction )( context, pOriginator, m_pData ); }

    // Returns a pointer to the job expecting the callback.
    inline Job* GetWaitingJob() { return m_pWaitingJob; }

    // Returns the thread on which the job expecting to be notifyied is waiting.
    inline THREADID GetWaitingJobThread() const { return m_WaitingJobThread; }

private:
    NOTIFY_FPTR m_pFunction;
    Job*  m_pWaitingJob;
    THREADID m_WaitingJobThread;
    void* m_pData;
};


//--------------------------------------------------------------------------------------
// Name: FINALIZE_FPTR
// Desc: A pointer to a C function that will be called when when the job has completed.
//       The function is always called on main thread.
//--------------------------------------------------------------------------------------
typedef JOBRESULT ( *FINALIZE_FPTR )( Context& ExecutionContext, // The execution context.
                                      void* pData );             // Extra data.


//--------------------------------------------------------------------------------------
// Name: FinalizeFunction
// Desc: Encapsulates all the data needed to execute the finalize function when a job 
//       has completed. Note that finalize, is always executed on the main thread.
//--------------------------------------------------------------------------------------
class FinalizeFunction
{
public:
    // Preferred constructor
    FinalizeFunction( FINALIZE_FPTR pFunction, // The function to be called.
                      Job* pJob,               // The job requesting that finalize be 
                                               // called.
                      void* pData )            // Additional data to be passed to 
                                               // pFunction.
        :m_pFunction( pFunction ), 
         m_pJob( pJob ), m_JobThread( THREADID_MAIN ), m_pData( pData ) {}

    // Default constructor
    FinalizeFunction() 
        :m_pFunction( 0 ), m_pJob( 0 ), m_JobThread( THREADID_NONE ), m_pData( 0 ) {}
    
    // Copy constructor
    FinalizeFunction( const FinalizeFunction& finalizeFunction ) 
        :m_pFunction( finalizeFunction.m_pFunction ),
         m_pJob( finalizeFunction.m_pJob ), 
         m_JobThread( finalizeFunction.m_JobThread ), 
         m_pData( finalizeFunction.m_pData ) {}
    
    // Assignment
    FinalizeFunction& operator =( const FinalizeFunction& finalizeFunction ) 
        { m_pFunction = finalizeFunction.m_pFunction; 
          m_pJob = finalizeFunction.m_pJob; 
          m_JobThread = finalizeFunction.m_JobThread; 
          m_pData = finalizeFunction.m_pData; return *this; }

    // Destructor
    ~FinalizeFunction() {}


    // The JobScheduler call this function when it is time to run the finalize 
    // function.
    inline JOBRESULT operator ()( Context& context ) 
        { return ( *m_pFunction )( context, m_pData ); }
    
    // Returns a pointer to the job expecting the callback.
    inline Job* GetJob() { return m_pJob; }

    // Returns the thread on which the job expecting to be notifyied is waiting.
    inline THREADID GetJobThread() const { return m_JobThread; }

private:
    FINALIZE_FPTR m_pFunction;
    Job*          m_pJob;
    THREADID      m_JobThread;
    void*         m_pData;
};


//--------------------------------------------------------------------------------------
// Name: Job
// Desc: Abstract class from  which to derive any job to be sent to the JobScheduler.
//--------------------------------------------------------------------------------------
class Job
{
public:
    // Prefered constructor
    Job( THREADID ExecutionThread = THREADID_ANY ) 
        :m_Status( JOBSTATUS_NOTSTARTED ), 
         m_ExecutionThread( ExecutionThread ), 
         m_pNotifyCallback( 0 ), 
         m_pFinalizeFunction( 0 ), m_IsRegistered( FALSE ) 
        {assert( m_ExecutionThread != THREADID_NONE && m_ExecutionThread != THREADID_MAXNUMWORKER ); }

    // Destructor
    virtual ~Job() {}
 
    // Reset the status of the job. Call this function between frames.
    void ResetJobStatus() { m_Status = JOBSTATUS_NOTSTARTED; }

    // Return the current job's status.
    JOBSTATUS GetJobStatus() const {  return m_Status; }

    // The registered flad is used by the JobScheduler to help keep track of the jobs it 
    // dispatches to different threads.
    void SetRegisteredFlag() { m_IsRegistered = TRUE; }
    void ClearRegisteredFlag() { m_IsRegistered = FALSE; }
    BOOL IsRegistered() const { return m_IsRegistered; }

    // A Job (usually the parent) that wants to be notified when this job completes, 
    // should set a notification callback through this function.
    void SetNotifyCallback( NotifyCallback* pNotifyCallback ) 
        { m_pNotifyCallback  = pNotifyCallback; }
 
    // Returns the thread on which the execution should take place.
    // The JobScheduler will use this value to steer the Job to the right thread. 
    THREADID GetExecutionThread() const
    {
        if( m_Status == JOBSTATUS_NOTSTARTED || m_Status == JOBSTATUS_WAITING )
            return m_ExecutionThread; 
        else if( m_Status == JOBSTATUS_NOTIFY ) 
            return m_pNotifyCallback->GetWaitingJobThread(); 
        else if (m_Status == JOBSTATUS_FINALIZE ) 
            return THREADID_MAIN; 
        else 
            return THREADID_NONE;
    }

    // This function is called by the JobScheduler each time a job is ready to be 
    // executed. It determine what action should be taken based on the job's current 
    // state, exectute the action and update the job's state.
    void Execute( Context& ExecutionContext )
    {
        PIXBeginNamedEvent( 0, "Processing job" );

        switch( m_Status )
        {
            case JOBSTATUS_NOTSTARTED:
            {
                // This is where the job execution actually takes place.
                JOBRESULT Result = ( *this )( ExecutionContext );
                if( Result == JOBRESULT_COMPLETED )
                    UpdateStatusAfterNotStarted();
                else
                    if( m_Status == JOBSTATUS_NOTSTARTED )
                        m_Status = JOBSTATUS_WAITING;
                break;
            }

            case JOBSTATUS_NOTIFY:
            {
                // This is where a job notifies another one that is has completed.
                // Both jobs are garanteed tobe available on the same hardware thread 
                // at this point.
                JOBRESULT Result = ( *m_pNotifyCallback )( ExecutionContext, this );
                UpdateStatusAfterNotify();
                if( Result == JOBRESULT_COMPLETED )
                {
                    m_pNotifyCallback->GetWaitingJob()->UpdateStatusAfterWaiting();
                    ExecutionContext.AwakenJob( m_pNotifyCallback->GetWaitingJob() );
                }
                break;
            }

            case JOBSTATUS_FINALIZE:
                // This where finalize is called for a particular job. 
                // The job is garanteed to execute on main thread, giving it access 
                // to shared resources.
                ( *m_pFinalizeFunction )( ExecutionContext );
                UpdateStatusAfterFinalize();
                break;

            default:
                // Obviously, we should never execute a function in a state like 
                // JOBSTATUS_WAITING or JOBSTATUS_COMPLETED.
                assert( false );
                break;
        }

        PIXEndNamedEvent();
    }
 

protected:
    // The concrete Job implementation calls this function to set a finalize function 
    // if it needs one to be called after it has executed.
    void SetFinalizeFunction( FinalizeFunction* pFinalizeFunction ) 
        { m_pFinalizeFunction = pFinalizeFunction; }
 
 
private:
    // Function to be overloaded by the concrete class.
    // It will be called by the JobScheduler when the time comes to execute this job.
    // This is where the real work should take place.
    virtual JOBRESULT operator ()( Context& ExecutionContext ) = 0;

    // Disabling copy constructor and assignment operator.
    Job( const Job& );
    Job& operator =( const Job& );

    // Helper functions to update this job's status depending on its current status.
    inline void UpdateStatusAfterNotStarted() 
        { assert( m_Status == JOBSTATUS_NOTSTARTED || m_Status == JOBSTATUS_WAITING ); 
          m_pNotifyCallback == NULL ? UpdateStatusAfterNotify() :
          (void)( m_Status = JOBSTATUS_NOTIFY ); }
    inline void UpdateStatusAfterWaiting() { UpdateStatusAfterNotStarted(); }
    inline void UpdateStatusAfterNotify() 
        { m_pFinalizeFunction == NULL ? UpdateStatusAfterFinalize() :
          (void)( m_Status = JOBSTATUS_FINALIZE ); }
    inline void UpdateStatusAfterFinalize() { m_Status = JOBSTATUS_COMPLETED; }


    JOBSTATUS m_Status;
    const THREADID m_ExecutionThread;
    NotifyCallback* m_pNotifyCallback;
    FinalizeFunction* m_pFinalizeFunction;
    BOOL m_IsRegistered;
};


static void ImmediateExecute( Job* job ); // Required by the ImmediateContext class 
                                          // bellow.


//--------------------------------------------------------------------------------------
// Name: ImmediateContext
// Desc: Concrete impletation of an execution context whose purpose is to to bypass the 
//       JobScheduler and execute jobs right on the spot.
//--------------------------------------------------------------------------------------
class ImmediateContext : public Context
{
public:
    ImmediateContext() {}
    virtual ~ImmediateContext() {}

    // Immediatelly process the job, no need to wait here.
    virtual void Dispatch( Job* pJob ) { ImmediateExecute( pJob ); }

    // Immediate mode always returns THREADID_MAIN.
    virtual THREADID GetThreadID() const { return THREADID_MAIN; }

    // Immediatelly process the job, no need to wait here.
    virtual void AwakenJob( Job* pJob ) { ImmediateExecute( pJob ); }

private:
    // Disabling copy constructor and assignment operator.
    ImmediateContext( const ImmediateContext& );
    ImmediateContext& operator =( const ImmediateContext& );
};


//--------------------------------------------------------------------------------------
// Name: ImmediateExecute
// Desc: Any job passed to ImmediateExecute will be executed on the spot without going 
//       through the JobScheduler. Any child job dispatched during this time, will also 
//       be executed right away.
//--------------------------------------------------------------------------------------
static void ImmediateExecute( Job* pJob )
{ 
    static ImmediateContext Context; // Shared by all instances of this function.

    while( pJob->GetJobStatus() != JOBSTATUS_COMPLETED )
    {
        // Because we are in immediate mode, we can assume that the job completes 
        // including its sub-jobs before returning.
        assert( pJob->GetJobStatus() != JOBSTATUS_WAITING );

        pJob->Execute( Context );
    }
}


#endif // JOB_H