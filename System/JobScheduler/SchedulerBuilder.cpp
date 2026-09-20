//--------------------------------------------------------------------------------------
// SchedulerBuilder.cpp
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include "SchedulerBuilder.h"

// Disable warning C6211: Leaking memory <pointer> due to an exception.
// We "know" that we won't run out of memory in this sample
#pragma warning ( disable : 6211 )

//--------------------------------------------------------------------------------------
ResourceManager::ResourceManager( BOOL bThreadsMaySleep )
:m_nJobQueueCount( 0 ),
 m_nBridgeEndPointACount( 0 ),
 m_pLoadBalancer( 0 ),
 m_pSync( 0 ),
 m_bThreadsMaySleep( bThreadsMaySleep ),
 m_nBridgeEndPointBCount( 0 )
{ 
    ThreadInfo_Zero< HANDLE >( m_ThreadHandleInfo );

    for( unsigned i( THREADID_WORKER1 ); i < THREADID_MAXNUMWORKER; ++ i )
    {
        m_pWorkerThread[ i ] = 0;
    }

    ThreadInfo_Zero< TerminateThread* >( m_TerminateThreadInfo );
}


//--------------------------------------------------------------------------------------
ResourceManager::~ResourceManager()
{
    delete m_pSync;

    delete m_pLoadBalancer;

    for( UINT i = THREADID_WORKER1; i < THREADID_MAXNUMWORKER; ++ i )
    {
        delete m_pWorkerThread[ i ];
        if( m_ThreadHandleInfo.WorkerThread[ i ] != 0 )
            CloseHandle( m_ThreadHandleInfo.WorkerThread[ i ] );
    }

    if( m_ThreadHandleInfo.LoadBalancer != 0 )
        CloseHandle( m_ThreadHandleInfo.LoadBalancer );

    for( UINT i = 0; i < m_nBridgeEndPointACount; ++i )
        delete m_pBridgeEndPointA[ i ];
        
    for( UINT i = 0; i < m_nBridgeEndPointBCount; ++i )
        delete m_pBridgeEndPointB[ i ];

    for( UINT i = 0; i < m_nJobQueueCount; ++i ) 
        delete m_pJobQueue[ i ];

    ThreadInfo_Delete< TerminateThread* >( m_TerminateThreadInfo );
}


//--------------------------------------------------------------------------------------
void ResourceManager::AssignHardwareThread( THREADID threadID, UINT hardwareThread)
{
    if( threadID == THREADID_LOADBALANCER )
    {
        m_TerminateThreadInfo.LoadBalancer = new TerminateThread( m_pLoadBalancer, THREADID_LOADBALANCER );

        DWORD hThreadId;
        m_ThreadHandleInfo.LoadBalancer = CreateThread( 0, 0, &ThreadableEntity_Start, m_pLoadBalancer, 
                                                        CREATE_SUSPENDED, &hThreadId );
        ATG::SetThreadName( hThreadId, "LoadBalancer" );

        assert( m_ThreadHandleInfo.LoadBalancer > 0 );
        XSetThreadProcessor( m_ThreadHandleInfo.LoadBalancer, ( hardwareThread ) % 6 );

    }
    else
    {
        assert( threadID < THREADID_MAXNUMWORKER );

        DWORD hThreadId;
        m_ThreadHandleInfo.WorkerThread[ threadID ] = 
            CreateThread( 0, 0, &ThreadableEntity_Start, m_pWorkerThread[ threadID ], 
                          CREATE_SUSPENDED, &hThreadId );
        ATG::SetThreadName( hThreadId, "WorkerThread" );

        XSetThreadProcessor( m_ThreadHandleInfo.WorkerThread[ threadID ], ( hardwareThread ) % 6 );

    }
}


//--------------------------------------------------------------------------------------
BridgeEndPoint* ResourceManager::NewBridgeEndPoint()
{
    JobQueueLockFree* queue1( new JobQueueLockFree );
    JobQueueLockFree* queue2( new JobQueueLockFree );
    m_pJobQueue[ m_nJobQueueCount ++ ] = queue1;
    m_pJobQueue[ m_nJobQueueCount ++ ] = queue2;

    BridgeEndPoint* pBridgeEndPoint = new BridgeEndPoint( queue1, queue2 );
    m_pBridgeEndPointA[ m_nBridgeEndPointACount ++ ] = pBridgeEndPoint;

    return pBridgeEndPoint;
}


//--------------------------------------------------------------------------------------
BridgeEndPoint* ResourceManager::NewBridgeEndPoint( BridgeEndPoint* pBridgeEndPoint )
{   
    return m_pBridgeEndPointB[ m_nBridgeEndPointBCount ++ ] = pBridgeEndPoint->GetOtherSide();
}


//--------------------------------------------------------------------------------------
WorkerThread* ResourceManager::NewWorkerThread( THREADID ThreadID, BridgeEndPoint* pBridgeEndPoint )
{
    assert( ThreadID < THREADID_MAXNUMWORKER );
    assert( m_pWorkerThread[ ThreadID ] == NULL );

    m_pWorkerThread[ ThreadID ] = new WorkerThread( ThreadID, pBridgeEndPoint );
    assert( m_pWorkerThread[ ThreadID ] != NULL );
    if( m_bThreadsMaySleep == TRUE )
        m_pWorkerThread[ ThreadID ]->SetThreadMaySleep();

    return m_pWorkerThread[ ThreadID ];
}


//--------------------------------------------------------------------------------------
LoadBalancer* ResourceManager::NewLoadBalancer( UINT nMaxJobs )
{
    assert( m_pLoadBalancer == NULL );
    m_pLoadBalancer = new LoadBalancer( nMaxJobs );
    assert( m_pLoadBalancer != NULL );

    return m_pLoadBalancer;
}


//--------------------------------------------------------------------------------------
SyncAll* ResourceManager::NewSyncAll( LoadBalancer* pLoadBalancer )
{
    assert( m_pSync == NULL );
    assert( pLoadBalancer != NULL );

    m_pSync = new SyncAll( pLoadBalancer );
    assert( m_pSync != NULL );

    return m_pSync;
}


//--------------------------------------------------------------------------------------
TerminateThread* ResourceManager::NewTerminateThread( THREADID ThreadID)
{
    assert( ThreadID == THREADID_LOADBALANCER || ThreadID < THREADID_MAXNUMWORKER );

    delete m_TerminateThreadInfo.Value( ThreadID ) ;
    if( ThreadID == THREADID_LOADBALANCER )
    {
        m_TerminateThreadInfo.LoadBalancer = 
            new TerminateThread( m_pLoadBalancer, THREADID_LOADBALANCER );
    }
    else
    {
        m_TerminateThreadInfo.WorkerThread[ ThreadID ] = 
            new TerminateThread( m_pWorkerThread[ ThreadID ], ThreadID );
    }

    return m_TerminateThreadInfo.Value( ThreadID );
}


//--------------------------------------------------------------------------------------
UINT ResourceManager::GetWorkerThreadCount() const 
{ 
    UINT Count = 0; 
    
    for( UINT i =  0 ; i < THREADID_MAXNUMWORKER; ++ i ) 
        if( m_pWorkerThread[ i ] != NULL ) 
            ++ Count; 
    
    return Count; 
}


//--------------------------------------------------------------------------------------
void ResourceManager::Resume() const
{
    if( m_ThreadHandleInfo.LoadBalancer != 0 )    
        ResumeThread( m_ThreadHandleInfo.LoadBalancer );

    for( unsigned i( THREADID_WORKER1 ); i < THREADID_MAXNUMWORKER; ++ i )
    {
        if( m_ThreadHandleInfo.WorkerThread[ i ] != 0 )
            ResumeThread( m_ThreadHandleInfo.WorkerThread[ i ] );
    }
}


//--------------------------------------------------------------------------------------
void ResourceManager::Suspend() const
{
    if( m_ThreadHandleInfo.LoadBalancer != 0 )    
        SuspendThread( m_ThreadHandleInfo.LoadBalancer );

    for( unsigned i( THREADID_WORKER1 ); i < THREADID_MAXNUMWORKER; ++ i )
        if( m_ThreadHandleInfo.WorkerThread[ i ] != 0 )
            SuspendThread( m_ThreadHandleInfo.WorkerThread[ i ] );
}


//--------------------------------------------------------------------------------------
void ResourceManager::Wake() const
{
    if( m_ThreadHandleInfo.LoadBalancer != 0 )    
        QueueUserAPC( &ThreadableEntity_Wake, m_ThreadHandleInfo.LoadBalancer, ( ULONG_PTR )m_pLoadBalancer );

    for( unsigned i( THREADID_WORKER1 ); i < THREADID_MAXNUMWORKER; ++ i )
        if( m_pWorkerThread[ i ] != NULL )
            QueueUserAPC( &ThreadableEntity_Wake, m_ThreadHandleInfo.WorkerThread[ i ], ( ULONG_PTR )m_pWorkerThread[ i ] );
}


//--------------------------------------------------------------------------------------
void SchedulerContext::Dispatch( Job* pJob )
{ 
    m_pScheduler->Dispatch( pJob ); 
}


//--------------------------------------------------------------------------------------
void SchedulerContext::AwakenJob( Job* pJob )
{
    assert( pJob->GetJobStatus() != JOBSTATUS_WAITING );
    m_pScheduler->AwakenJob( pJob );
}


//--------------------------------------------------------------------------------------
void Scheduler::WakeThreads()
{
    if( m_bThreadsIdle == TRUE )
    { 
        m_pResourceManager->WakeThreads();
        m_bThreadsIdle = FALSE; 
    } 
}


//--------------------------------------------------------------------------------------
void Scheduler::SleepThreads()
{
    m_pResourceManager->SleepThreads(); m_bThreadsIdle = TRUE; 
}


//--------------------------------------------------------------------------------------
void Scheduler::TerminateWorkerThreads()
{
    UINT Count = m_pResourceManager->GetWorkerThreadCount();

    for( unsigned i( THREADID_WORKER1 ); i < Count; ++ i )
        Dispatch( m_pResourceManager->NewTerminateThread( ( THREADID )i ) );
}


//--------------------------------------------------------------------------------------
SchedulerLocal::~SchedulerLocal()
{
    Sync(); // Ensure that the threads have completed their work. This is just a precaution.
    TerminateWorkerThreads();

    // Giving some time for the threads to terminate before freeing any resources the may be using.
    // Consider replacing this ungainly Sleep with a nice WaitForMultipleObjects on the thread handles.
    Sleep( 1 );
}   


//--------------------------------------------------------------------------------------
SchedulerRemote::SchedulerRemote( ResourceManager* pResourceManager, BridgeEndPoint* pBridgeEndPoint, SyncAll* pSyncAll )
:m_pBridgeEndPoint( pBridgeEndPoint ),
 m_pJobToAwaken( 0 ),
 m_pSyncAll( pSyncAll ),
 Scheduler( pResourceManager )
{ 
    assert( m_pBridgeEndPoint != NULL );
    assert( pSyncAll != NULL );
    assert( pResourceManager != NULL );

    m_pSchedulerContext = new SchedulerContext( this, m_pBridgeEndPoint ); 
    m_pSyncAll->SetSynched( &m_bMustTerminate );
}


//--------------------------------------------------------------------------------------
SchedulerRemote::~SchedulerRemote()
{
    Sync(); // Ensure that the threads have completed their work. This is just a precaution.
    TerminateWorkerThreads();
    TerminateLoadBalancerThread();

    // Giving some time for the threads to terminate before freeing any resources the may be using.
    // Consider replacing this ungainly Sleep with a nice WaitForMultipleObjects on the thread handles.
    Sleep( 1 );
}


//--------------------------------------------------------------------------------------
void SchedulerRemote::Sync()
{
    WakeThreads();

    m_pSyncAll->ResetJobStatus();
    m_bMustTerminate = FALSE;
    m_pBridgeEndPoint->Send( m_pSyncAll ); 
    
    while( ! m_bMustTerminate )
    {
        Job* pJob = m_pBridgeEndPoint->Receive();
        if( pJob != NULL )
        {
            assert( pJob->GetJobStatus() != JOBSTATUS_WAITING );
            
            ProcessJob( pJob );

            while( m_pJobToAwaken != NULL )
            {
                Job* pJobToAwaken = m_pJobToAwaken;
                m_pJobToAwaken = NULL;
                ProcessJob( pJobToAwaken );
            }
        }
    }

    SleepThreads(); 
}


//--------------------------------------------------------------------------------------
void SchedulerRemote::ProcessJob( Job* pJob )
{
    assert( pJob != NULL );
    assert( pJob->GetJobStatus() != JOBSTATUS_COMPLETED );
    assert( pJob->GetJobStatus() != JOBSTATUS_WAITING );

    while ( ( pJob->GetExecutionThread() == THREADID_ANY || 
              pJob->GetExecutionThread() == THREADID_MAIN    ) && 
            pJob->GetJobStatus() != JOBSTATUS_WAITING )
    {
        pJob->Execute( *m_pSchedulerContext );
    }

    if( pJob->GetJobStatus() != JOBSTATUS_WAITING )
        if( pJob->IsRegistered() == TRUE || pJob->GetExecutionThread() != THREADID_NONE )
            m_pBridgeEndPoint->Send( pJob );
}


//--------------------------------------------------------------------------------------
void SchedulerRemote::TerminateLoadBalancerThread()
{
    Dispatch( m_pResourceManager->NewTerminateThread( THREADID_LOADBALANCER ) );
}


//--------------------------------------------------------------------------------------
Scheduler* SchedulerBuilder::NewScheduler( UINT nExtraThreads, 
                                           UINT nMaxJobs, BOOL bLoadBalancerOnMain ) const
{ 
    assert( nExtraThreads >= THREADID_WORKER1 && nExtraThreads <= THREADID_MAXNUMWORKER );

    if( bLoadBalancerOnMain == TRUE ) 
    {
        return NewSchedulerLocal( nExtraThreads, nMaxJobs ); 
    }
    else 
    {
        return NewSchedulerRemote( nExtraThreads, nMaxJobs ); 
    }
}


//--------------------------------------------------------------------------------------
Scheduler* SchedulerBuilder::NewSchedulerLocal( UINT nExtraThreads, UINT nMaxJobs ) const
{
    ResourceManager* pResMan = new ResourceManager( m_bWorkerThreadsMaySleep );

    LoadBalancer* pLoadBalancer = pResMan->NewLoadBalancer( nMaxJobs );
    pLoadBalancer->SetMaxPendingJobs( m_nMaxPendingJobs );
    Scheduler* pScheduler = new SchedulerLocal( pResMan, *pLoadBalancer );

    for( UINT i = THREADID_WORKER1; i < nExtraThreads; ++ i )
    {
        BridgeEndPoint* pBridgeEndPoint = pResMan->NewBridgeEndPoint();
        pLoadBalancer->ConnectWorkerThread( pBridgeEndPoint );

        pResMan->NewWorkerThread( ( THREADID )i, 
                                  pResMan->NewBridgeEndPoint( pBridgeEndPoint ) );
        pResMan->AssignHardwareThread( ( THREADID )i, i + 1);
    }

    pResMan->StartThreads();
    return pScheduler;
}


//--------------------------------------------------------------------------------------
Scheduler* SchedulerBuilder::NewSchedulerRemote( UINT nExtraThreads, UINT nMaxJobs ) const
{
    ResourceManager* pResMan = new ResourceManager( m_bWorkerThreadsMaySleep );

    UINT nActualWorkerThreads = nExtraThreads > 0 ? nExtraThreads - 1 : 0;
 
    BridgeEndPoint* pSchedulerBridgeEndPoint =  pResMan->NewBridgeEndPoint();
    LoadBalancer* pLoadBalancer = pResMan->NewLoadBalancer( nMaxJobs );

    Scheduler* pScheduler = 
        new SchedulerRemote( pResMan, 
                             pResMan->NewBridgeEndPoint( pSchedulerBridgeEndPoint ), 
                             pResMan->NewSyncAll( pLoadBalancer ) );

    for( UINT i = THREADID_WORKER1; i < nActualWorkerThreads; ++ i )
    {
        BridgeEndPoint* pBridgeEndPoint = pResMan->NewBridgeEndPoint();
        pLoadBalancer->ConnectWorkerThread( pBridgeEndPoint );

        pResMan->NewWorkerThread( ( THREADID )i, 
                                  pResMan->NewBridgeEndPoint( pBridgeEndPoint ) );
        pResMan->AssignHardwareThread( ( THREADID )i, i + 1);
    }

    pLoadBalancer->ConnectScheduler( pSchedulerBridgeEndPoint );

    if( m_bWorkerThreadsMaySleep == TRUE )
        pLoadBalancer->SetThreadMaySleep();

    if( m_bUseMainThread == TRUE )
        pLoadBalancer->SetUseMainThread();

    pLoadBalancer->SetMaxPendingJobs( m_nMaxPendingJobs );
    pResMan->AssignHardwareThread( THREADID_LOADBALANCER, nExtraThreads );

    pResMan->StartThreads();
    return pScheduler;
}
