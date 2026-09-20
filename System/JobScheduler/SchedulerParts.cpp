//--------------------------------------------------------------------------------------
// SchedulerParts.cpp
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include "SchedulerParts.h"


//--------------------------------------------------------------------------------------
Node::Node( Job* pJob, Node* pNext )
:m_pJob( pJob ), 
 m_pPrevious( NULL ), 
 m_pNext( pNext )
{
    assert( pJob != NULL );

    if( m_pNext != NULL ) 
    { 
        m_pPrevious = m_pNext->m_pPrevious; 
        m_pNext->m_pPrevious = this; 
    } 
}


//--------------------------------------------------------------------------------------
Node::~Node() 
{ 
    if( m_pPrevious != NULL ) 
        m_pPrevious->m_pNext = m_pNext; 
    
    if( m_pNext != NULL ) 
        m_pNext->m_pPrevious = m_pPrevious; 
}


//--------------------------------------------------------------------------------------
JobQueue::JobQueue( UINT nBufferSise )
:m_nBufferSize( nBufferSise ), 
 m_nCount( 0 )
{
    m_ppJobBuffer = new Job* [ m_nBufferSize ];
    m_ppHeadJob = m_ppJobBuffer;
    m_ppTailJob = m_ppHeadJob;
}


//--------------------------------------------------------------------------------------
JobQueue::~JobQueue()
{
    delete [] m_ppJobBuffer;
}


//--------------------------------------------------------------------------------------
DWORD ThreadableEntity::Process()
{ 
    // m_bMustTerminate is set to TURE by the TerminateThread message that is sent by the 
    // JobScheduler when it is time to shut down the scheduler.
    while ( ! m_bMustTerminate ) 
    { 
        if( Tick() == FALSE )
            // If Tick had nothing to do during the last call and and if the JobScheduler
            // is configured to allow threads to sleep and this thread as been running for 
            // at least 1 ms since it last was awoken, then it should go to sleep.
            if( m_bThreadMaySleep == TRUE && 
                m_Timer.GetElapsedTime() > 0.001 )
            { 
                // Calling SleepEX instead of Sleep lets the JobScheduler send an APC message 
                // to wake the thread, if new work becomes availabe.
                SleepEx( 1, true ); 
            }   
    } 

    return 0; // Returning 0 will terminate the thread...
}


//--------------------------------------------------------------------------------------
DWORD CALLBACK ThreadableEntity_Start( void* information )
{
    ThreadableEntity* self = ( ThreadableEntity* )information;
    self->Wake();
    return self->Process();
}


//--------------------------------------------------------------------------------------
VOID CALLBACK ThreadableEntity_Wake( ULONG_PTR dwParam )
{
    ThreadableEntity* entity( (ThreadableEntity*) dwParam );
        
    entity->Wake();
}


//--------------------------------------------------------------------------------------
WorkerThreadContext::WorkerThreadContext( THREADID ThreadID, 
                                          BridgeEndPoint* pBridgeEndPoint, 
                                          WorkerThread* pWorkerThread )
:m_ThreadID( ThreadID ), 
 m_pBridgeEndPoint( pBridgeEndPoint ), 
 m_pWorkerThread( pWorkerThread )
{
}


//--------------------------------------------------------------------------------------
void WorkerThreadContext::AwakenJob( Job* pJob )
{
    assert( pJob->GetJobStatus() != JOBSTATUS_WAITING );
    m_pWorkerThread->AwakenJob( pJob );
}


//--------------------------------------------------------------------------------------
WorkerThread::WorkerThread ( THREADID ThreadID, BridgeEndPoint* pBridgeEndPoint )
:m_pBridgeEndPoint( pBridgeEndPoint ), 
 m_ThreadID( ThreadID ), 
 m_pContext( 0 ), 
 m_pJobToAwaken( 0 ) 
{ 
    assert( pBridgeEndPoint != NULL );

    m_pContext = new WorkerThreadContext( m_ThreadID, m_pBridgeEndPoint, this ); 
}


//--------------------------------------------------------------------------------------
WorkerThread::~WorkerThread()
{
    delete m_pContext; 
}


//--------------------------------------------------------------------------------------
BOOL WorkerThread::Tick()
{
    BOOL bNeedMore = FALSE;

    Job* pJob( m_pBridgeEndPoint->Receive() );
    if( pJob != NULL )
    {
        assert( pJob->GetExecutionThread() == m_ThreadID || 
                pJob->GetExecutionThread() == THREADID_ANY );
        assert( pJob->GetJobStatus() != JOBSTATUS_COMPLETED );
        assert( pJob->GetJobStatus() != JOBSTATUS_WAITING );
        
        ProcessJob( pJob );

        while( m_pJobToAwaken != NULL )
        {
            Job* pJobToAwaken = m_pJobToAwaken;
            m_pJobToAwaken = NULL;
            ProcessJob( pJobToAwaken );
        }

        bNeedMore = TRUE;
    }
        
    return bNeedMore;
}


//--------------------------------------------------------------------------------------
void WorkerThread::ProcessJob( Job* pJob )
{
    assert( pJob != NULL );
    assert( pJob->GetJobStatus() != JOBSTATUS_COMPLETED );
    assert( pJob->GetJobStatus() != JOBSTATUS_WAITING );

    while ( ( pJob->GetExecutionThread() == THREADID_ANY || 
              pJob->GetExecutionThread() == m_ThreadID    ) && 
            pJob->GetJobStatus() != JOBSTATUS_WAITING )
    {
        pJob->Execute( *m_pContext );
    }

    if( pJob->GetJobStatus() != JOBSTATUS_WAITING )
        if( pJob->IsRegistered() == TRUE || pJob->GetExecutionThread() != THREADID_NONE )
            m_pBridgeEndPoint->Send( pJob );
}


//--------------------------------------------------------------------------------------
void LoadBalancerContext::Dispatch( Job* pJob )
{ 
    m_pLoadBalancer->Dispatch( pJob ); 
}


//--------------------------------------------------------------------------------------
void LoadBalancerContext::AwakenJob( Job* pJob )
{
    assert( pJob->GetJobStatus() != JOBSTATUS_WAITING );

    while( pJob->GetExecutionThread() == THREADID_LOADBALANCER || 
           ( m_pLoadBalancer->IsRemote() == false && 
             pJob->GetExecutionThread() == THREADID_MAIN ) ) 
    {
        pJob->Execute( *this );
    }

    if( pJob->GetExecutionThread() != THREADID_NONE )
        m_pLoadBalancer->Dispatch( pJob, pJob->GetExecutionThread() );
}


//--------------------------------------------------------------------------------------
LoadBalancer::LoadBalancer( UINT nMaxJobs, UINT nMaxDispatch )
:m_nMaxDispatch( nMaxDispatch ),
 m_bUseMainThread( FALSE ),
 m_nActualWorkerThreads( 0 ),
 m_nLastDispatch( THREADID_WORKER5 ),
 m_bSynching( FALSE )
{
    // Create a JobQueue with enough room for all user submitted jobs and
    // Terminate and SyncAll internal jobs.
    m_pLocalJobPool = new JobQueue( nMaxJobs + THREADID_MAXNUMWORKER + 1 );

    ThreadInfo_Zero< BridgeEndPoint* >( m_BridgeEndPointInfo );
    ThreadInfo_Zero< UINT >( m_JobsToProcess );

    m_pLoadBalancerContext = new LoadBalancerContext( this );
}


//--------------------------------------------------------------------------------------
LoadBalancer::~LoadBalancer()
{
    delete m_pLoadBalancerContext;
    delete m_pLocalJobPool;
}


//--------------------------------------------------------------------------------------
void LoadBalancer::ConnectWorkerThread( BridgeEndPoint* pBridgeEndPoint )
{ 
    for( UINT i = THREADID_WORKER1; i < THREADID_MAXNUMWORKER; ++ i ) 
        if( m_BridgeEndPointInfo.WorkerThread[ i ] == NULL ) 
        { 
            m_BridgeEndPointInfo.WorkerThread[ i ] = pBridgeEndPoint;
            ++ m_nActualWorkerThreads;
            break; 
        }
}


//--------------------------------------------------------------------------------------
void LoadBalancer::Dispatch( Job* pJob ) 
{
    assert( pJob != NULL );

    if( pJob->GetExecutionThread() != THREADID_ANY )
        Dispatch( pJob, pJob->GetExecutionThread() );
    else
    {
        THREADID NextThread = GetNextWaitingWorkerThread();
        if( NextThread == THREADID_NONE )
            m_pLocalJobPool->Push( pJob );
        else
        {
            m_nLastDispatch = NextThread;
            Dispatch( pJob, NextThread );
        }
    }
}


//--------------------------------------------------------------------------------------
void LoadBalancer::Dispatch( Job* pJob, THREADID ThreadID )
{
    assert( pJob != NULL );
    assert( ( ThreadID == THREADID_MAIN && m_BridgeEndPointInfo.MainThread != NULL ) || 
            ( ThreadID >= THREADID_WORKER1 && ( UINT )ThreadID < m_nActualWorkerThreads ) );

    assert( pJob->GetJobStatus() != JOBSTATUS_COMPLETED );

    assert( pJob->IsRegistered() == FALSE );
    if( pJob->GetJobStatus() == JOBSTATUS_NOTSTARTED )
    {
        m_JobsToProcess.Value( ThreadID ) ++;
        pJob->SetRegisteredFlag(); 
    }

    m_BridgeEndPointInfo.Value( ThreadID )->Send( pJob );
}


//--------------------------------------------------------------------------------------
void LoadBalancer::Sync()
{
    BOOL bNeedMore;
    m_bSynching = TRUE;

    do
    {
        // Calling LoadBalancer::Tick() directly instead of calling pure virtual
        // Tick in base helps get better performances.
        bNeedMore = LoadBalancer::Tick();
    }
    // Consider lowering the thread's priority (not sleeping it) from time to time to 
    // let the other hadrware thread running on the same core a chance to breath a little.
    while( bNeedMore == TRUE                     ||
           m_JobsToProcess.MainThread > 0        ||
           m_JobsToProcess.WorkerThread[ 0 ] > 0 || 
           m_JobsToProcess.WorkerThread[ 1 ] > 0 ||
           m_JobsToProcess.WorkerThread[ 2 ] > 0 ||
           m_JobsToProcess.WorkerThread[ 3 ] > 0 ||
           m_JobsToProcess.WorkerThread[ 4 ] > 0 ||
           m_pLocalJobPool->GetJobCount() > 0         );

    m_bSynching = FALSE;
}


//--------------------------------------------------------------------------------------
BOOL LoadBalancer::Tick()
{
    BOOL bNeedMore = FALSE;

    if( m_BridgeEndPointInfo.MainThread !=  0 )
    { 
        Job* pJob = m_BridgeEndPointInfo.MainThread->Receive();
        if( pJob != NULL )
        {
            ProcessJob( pJob, THREADID_MAIN );

            bNeedMore = TRUE;
        }

        if( m_bSynching == TRUE && m_bUseMainThread == TRUE )
            if( m_pLocalJobPool->GetJobCount() > 0 )
                Dispatch( m_pLocalJobPool->Pop(), THREADID_MAIN );
    }

    for( UINT i = THREADID_WORKER1; i < m_nActualWorkerThreads; ++ i ) 
        if( m_BridgeEndPointInfo.WorkerThread[ i ] != NULL ) 
        { 
            Job* pJob( m_BridgeEndPointInfo.WorkerThread[ i ]->Receive() );
            if( pJob != NULL )
            {
                ProcessJob( pJob, ( THREADID )i );

                bNeedMore = TRUE;
            }
            
            if( ( m_bSynching == TRUE || ( m_nMaxDispatch == 0 || 
                  m_JobsToProcess.WorkerThread[ i ] < m_nMaxDispatch ) ) && 
                m_pLocalJobPool->GetJobCount() > 0 )
            {
                Dispatch( m_pLocalJobPool->Pop(), ( THREADID )i );
            }

        }

    if( bNeedMore == FALSE && m_bSynching == TRUE )
        if( m_pLocalJobPool->GetJobCount() > 0 )
        {
            Job* pJob = m_pLocalJobPool->Pop();
            pJob->Execute( *m_pLoadBalancerContext );
            if( pJob->GetJobStatus() != JOBSTATUS_WAITING )
                ProcessJob( pJob, THREADID_LOADBALANCER );
            bNeedMore = TRUE;
        }

    return bNeedMore;
}


//--------------------------------------------------------------------------------------
//Returns the next worker thread whose job queue is not full, or THREADID_NONE.
//--------------------------------------------------------------------------------------
THREADID LoadBalancer::GetNextWaitingWorkerThread()
{
    UINT nLoop = m_nActualWorkerThreads;
    if( m_bSynching == TRUE && m_bUseMainThread == TRUE )
        nLoop ++;

    UINT nNextDispatch = m_nLastDispatch;
    for( UINT i = 0; i < nLoop ; ++ i )
    {
        if( nNextDispatch == THREADID_MAIN )
            nNextDispatch = 0;
        else
            ++ nNextDispatch;

        if( nNextDispatch >= m_nActualWorkerThreads )
        {
            if( m_bSynching == TRUE && m_bUseMainThread == TRUE )
                nNextDispatch = THREADID_MAIN;
            else
                nNextDispatch = 0;
        }

        if( m_nMaxDispatch == 0 || 
            m_JobsToProcess.Value( ( THREADID )nNextDispatch ) < m_nMaxDispatch )
        {
            return ( THREADID )nNextDispatch;
        }
    }

    return THREADID_NONE;
}


//--------------------------------------------------------------------------------------
void LoadBalancer::ProcessJob( Job* pJob, THREADID ThreadFrom )
{
    assert( pJob != NULL );

    if( pJob->IsRegistered() == TRUE )
    {
        m_JobsToProcess.Value( ThreadFrom ) --;
        pJob->ClearRegisteredFlag();
    }
    
    assert( pJob->GetJobStatus() != JOBSTATUS_WAITING );
    if( pJob->GetJobStatus() != JOBSTATUS_WAITING )
    {
        switch( pJob->GetExecutionThread() )
        {
            case THREADID_NONE:
                break;
    
            case THREADID_MAIN:
                if( m_BridgeEndPointInfo.MainThread ==  0 )
                {
                    pJob->Execute( *m_pLoadBalancerContext );
                    if( pJob->GetJobStatus() != JOBSTATUS_COMPLETED )
                        ProcessJob( pJob, THREADID_MAIN );
                }
                else
                    Dispatch( pJob, THREADID_MAIN );
                break;

            case THREADID_LOADBALANCER:
                pJob->Execute( *m_pLoadBalancerContext );
                if( pJob->GetJobStatus() != JOBSTATUS_COMPLETED )
                    ProcessJob( pJob, THREADID_LOADBALANCER );
                break;

            case THREADID_ANY:
                Dispatch( pJob );
                break;

            default:
                Dispatch( pJob, pJob->GetExecutionThread() );
                break;
        }
    }
}


//--------------------------------------------------------------------------------------
SyncAll::SyncAll( LoadBalancer* pLoadBalancer ) 
:m_pLoadBalancer( pLoadBalancer ), 
 m_pbSynched( 0 ),
 Job( THREADID_LOADBALANCER )
{
    m_pFinalizeFunction = FinalizeFunction( &SyncAll::Finalize, this, this );
    SetFinalizeFunction( &m_pFinalizeFunction );
}
