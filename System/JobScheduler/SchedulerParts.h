//--------------------------------------------------------------------------------------
// SchedulerParts.h
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef SCHEDULERPARTS_H
#define SCHEDULERPARTS_H

#include <AtgUtil.h>
#include "AtgLockFreePipe.h"
#include "Job.h"

class LoadBalancer;
class WorkerThread;


//--------------------------------------------------------------------------------------
// Name: ThreadInfo
// Desc: Utility structure to help keep thread related info together. 
//--------------------------------------------------------------------------------------
template< typename type >
struct ThreadInfo
{
public:

    typename type MainThread;
    typename type LoadBalancer;
    typename type WorkerThread[ THREADID_MAXNUMWORKER ];

    typename type& Value( THREADID ThreadID ) 
    { 
        if( ThreadID == THREADID_MAIN ) 
            return MainThread; 
        else if( ThreadID == THREADID_LOADBALANCER ) 
            return LoadBalancer; 
        else if ( ThreadID < THREADID_MAXNUMWORKER) 
            return WorkerThread[ ThreadID ]; 
        else 
            return m_Invalid; 
    }


private:
    typename type m_Invalid;
};


//--------------------------------------------------------------------------------------
// Name: ThreadInfo_Zero
// Desc: Zeroes each item of a ThreadInfo structure. 
//--------------------------------------------------------------------------------------
template< typename type >
void ThreadInfo_Zero( ThreadInfo< type >& ThreadInfo )
{
    ThreadInfo.MainThread = 0;
    ThreadInfo.LoadBalancer = 0;

    for( UINT i = 0; i < THREADID_MAXNUMWORKER; ++ i ) 
        ThreadInfo.WorkerThread[ i ] = 0; 
};


//--------------------------------------------------------------------------------------
// Name: ThreadInfo_Zero
// Desc: Calls operator delete on each item of a ThreadInfo structure.
//--------------------------------------------------------------------------------------
template< typename type >
void ThreadInfo_Delete( ThreadInfo< type >& ThreadInfo )
{
    delete ThreadInfo.MainThread;
    delete ThreadInfo.LoadBalancer;

    for( UINT i = 0; i < THREADID_MAXNUMWORKER; ++ i ) 
        delete ThreadInfo.WorkerThread[ i ]; 
}


//--------------------------------------------------------------------------------------
// Name: JobQueueLockFree
// Desc: A wrapper over the ATG::LockFreePipe used to pass jobs between threads.
//--------------------------------------------------------------------------------------
class JobQueueLockFree
{
public:
    JobQueueLockFree() {}
    ~JobQueueLockFree() {}

    inline void Push( Job* pJob ) { m_Pipe.Write( &pJob, sizeof( Job* ) ); }
    inline Job* Pop() 
        { Job* pJob; 
          if( m_Pipe.Read( &pJob, sizeof( Job* ) ) == true ) return pJob; 
          else return 0; }


private:
    // Disabling default and copy constructors and assignment operator.
    JobQueueLockFree( const JobQueueLockFree& );
    JobQueueLockFree& operator =( const JobQueueLockFree& );

    ATG::LockFreePipe< 12 > m_Pipe;
};


//--------------------------------------------------------------------------------------
// Name: Node
// Desc: A node that holds a pointer to a Job.
//--------------------------------------------------------------------------------------
class Node
{
public:
    Node( Job* pJob, Node* pNext );
    ~Node();

    inline Job* GetJob() const { return m_pJob; }
    inline Node* GetPrevious() const { return m_pPrevious; }
    inline Node* GetNext() const { return m_pNext; }


private:
    // Disabling default and copy constructors and assignment operator.
    Node();
    Node( const Node& );
    Node& operator =( const Node& );

    Job* m_pJob;
    Node* m_pPrevious;
    Node* m_pNext;
};


//--------------------------------------------------------------------------------------
// Name: JobQueue
// Desc: Used by the LoadBalancer to hold jobs waiting to be scheduled.
//--------------------------------------------------------------------------------------
class JobQueue
{
public:
    explicit JobQueue( UINT nBufferSise );
    ~JobQueue();

    void Push( Job* pJob );
    Job* Pop();

    inline UINT GetJobCount() const { return m_nCount; }


private:
    // Disabling default and copy constructors and assignment operator.
    JobQueue();
    JobQueue( const JobQueue& );
    JobQueue& operator =( const JobQueue& );

    Job** m_ppJobBuffer;
    Job** m_ppHeadJob;
    Job** m_ppTailJob;

    UINT m_nBufferSize;
    UINT m_nCount;
};


inline void JobQueue::Push( Job* pJob )
{ 
    assert( pJob != NULL );
    assert( m_nCount < m_nBufferSize - 1 );

    ++ m_nCount ;

    *m_ppHeadJob = pJob;
    m_ppHeadJob ++;
    if( m_ppHeadJob >= m_ppJobBuffer + m_nBufferSize )
        m_ppHeadJob = m_ppJobBuffer;
}


inline Job* JobQueue::Pop() 
{ 
    if( m_ppTailJob == m_ppHeadJob ) 
        return 0; 
    else 
    { 
        -- m_nCount ;

        Job* pJobToReturn( *m_ppTailJob );
        m_ppTailJob ++;
        if( m_ppTailJob >= m_ppJobBuffer + m_nBufferSize )
            m_ppTailJob= m_ppJobBuffer;
      
        return pJobToReturn; 
    } 
}


//--------------------------------------------------------------------------------------
// Name: BridgeEndPoint
// Desc: Combines two JobQueue together to provide bi-directionnal communications 
//       between threadable entities.
//--------------------------------------------------------------------------------------
class BridgeEndPoint
{
public:
    BridgeEndPoint( JobQueueLockFree* pSender, JobQueueLockFree* pReceiver ) 
        :m_pSender( pSender ), m_pReceiver( pReceiver ) {}

    BridgeEndPoint* GetOtherSide() const { return new BridgeEndPoint( m_pReceiver, m_pSender ); }

    void Send( Job* pJob ) { m_pSender->Push( pJob ); }
    Job* Receive() { return m_pReceiver->Pop(); }


private:
    // Disabling deault and copy constructor and assignment operator.
    BridgeEndPoint();
    BridgeEndPoint( const BridgeEndPoint& );
    BridgeEndPoint& operator =( const BridgeEndPoint& );

    JobQueueLockFree* m_pSender;
    JobQueueLockFree* m_pReceiver;
};


//--------------------------------------------------------------------------------------
// Name: ThreadableEntity
// Desc: Abstract base class for all entities that can run on a separate thread. The 
//       LoadBalancer and WorkerThread are the only ThreadableEntity (ies) at the moment. 
//--------------------------------------------------------------------------------------
class ThreadableEntity
{
public:
    ThreadableEntity() :m_bMustTerminate( FALSE ), m_bThreadMaySleep( FALSE ) {}
    virtual ~ThreadableEntity() {}

    void SetThreadMaySleep() { m_bThreadMaySleep = TRUE; }

    void Wake() { m_Timer.Reset(); m_Timer.Start(); }
    void Terminate() { m_bMustTerminate = TRUE; }

    virtual BOOL Tick() = 0;

    DWORD Process();


protected:
    BOOL m_bMustTerminate;
    BOOL m_bThreadMaySleep;


private:
    ATG::Timer m_Timer;
};


// Entry point for the OS to start a thread.
DWORD CALLBACK ThreadableEntity_Start( void* information );

// Entry point for an APC message used to wake up threads at the beginnng of a frame. 
VOID CALLBACK ThreadableEntity_Wake( ULONG_PTR dwParam );


//--------------------------------------------------------------------------------------
// Name: WorkerThreadContext
// Desc: Concrete implementation of an execution context to be passed to a job running
//       on the WorkerThread. 
//--------------------------------------------------------------------------------------
class WorkerThreadContext : public Context
{
public:
    WorkerThreadContext( THREADID ThreadID, BridgeEndPoint* pBridgeEndPoint, WorkerThread* pWorkerThread );

    virtual void Dispatch( Job* pJob ) { m_pBridgeEndPoint->Send( pJob ); }

    virtual THREADID GetThreadID() const {  return m_ThreadID; }
    virtual void AwakenJob( Job* pJob );


private:
    // Disabling deafult and copy constructor and assignment operator.
    WorkerThreadContext();
    WorkerThreadContext( const WorkerThreadContext& );
    WorkerThreadContext& operator =( const WorkerThreadContext& );

    THREADID m_ThreadID;
    BridgeEndPoint* m_pBridgeEndPoint;
    WorkerThread* m_pWorkerThread;
};


//--------------------------------------------------------------------------------------
// Name: WorkerThread
// Desc: A class whose only purpose is to execute jobs. It polls the associated bridge 
//       for jobs and execute them in the order received.
//--------------------------------------------------------------------------------------
class WorkerThread : public ThreadableEntity
{
public:
    WorkerThread ( THREADID ThreadID, BridgeEndPoint* pBridgeEndPoint );
    virtual ~WorkerThread();

    virtual BOOL Tick();

    void AwakenJob( Job* pJob ) { m_pJobToAwaken = pJob; }


private:
    // Disabling dafault and copy constructor and assignment operator.
    WorkerThread();
    WorkerThread( const WorkerThread& );
    WorkerThread& operator =( const WorkerThread& );

    void ProcessJob( Job* pJob );

    BridgeEndPoint* m_pBridgeEndPoint;
    THREADID m_ThreadID;
    
    WorkerThreadContext* m_pContext;

    Job* m_pJobToAwaken;
};


//--------------------------------------------------------------------------------------
// Name: LoadBalancerContext
// Desc: Concrete impletation of an execution context to be passed to a job running on 
//       the LoadBalancer. When times permit the load balancer act as a worker thread 
//       and contribute to processing the jobs.
//--------------------------------------------------------------------------------------
class LoadBalancerContext : public Context
{
public:
    LoadBalancerContext( LoadBalancer* pLoadBalancer ) :m_pLoadBalancer( pLoadBalancer )
        { assert( pLoadBalancer != NULL ); }

    ~LoadBalancerContext() {}

    virtual void Dispatch( Job* pJob );

    virtual THREADID GetThreadID() const { return THREADID_LOADBALANCER; }

    virtual void AwakenJob( Job* pJob );


private:
    // Disabling default and copy constructors and assignment operator.
    LoadBalancerContext();
    LoadBalancerContext( const LoadBalancerContext& );
    LoadBalancerContext& operator =( const LoadBalancerContext& );

    LoadBalancer* m_pLoadBalancer;
};


//--------------------------------------------------------------------------------------
// Name: LoadBalancer
// Desc: A round robin load balancer that dispatches jobs to the worker threads.
//--------------------------------------------------------------------------------------
class LoadBalancer : public ThreadableEntity
{
public:
    LoadBalancer( UINT nMaxJobs, UINT maxDispatch = 0 );
    ~LoadBalancer();

    void SetUseMainThread() { m_bUseMainThread = TRUE; }
    void SetMaxPendingJobs( UINT nMaxPendingJobs ) { m_nMaxDispatch = nMaxPendingJobs; }

    void ConnectWorkerThread( BridgeEndPoint* pBridgeEndPoint );
    void ConnectScheduler( BridgeEndPoint* pBridgeEndPoint ) 
        { m_BridgeEndPointInfo.MainThread = pBridgeEndPoint; }

    BOOL IsRemote() const { return m_BridgeEndPointInfo.MainThread != NULL; }

    void Dispatch( Job* pJob );
    void Dispatch( Job* pJob, THREADID ThreadID );

    void Sync();

    BOOL Tick();


private:
    // Disabling copy constructor and assignment operator.
    LoadBalancer( const LoadBalancer& );
    LoadBalancer& operator =( const LoadBalancer& );


    THREADID GetNextWaitingWorkerThread();
    void ProcessJob( Job* pJob, THREADID ThreadFrom );

    UINT m_nMaxDispatch;
    BOOL m_bUseMainThread;

    UINT m_nActualWorkerThreads;
    ThreadInfo< BridgeEndPoint* > m_BridgeEndPointInfo;

    ThreadInfo< UINT > m_JobsToProcess;
    UINT m_nLastDispatch;
    JobQueue* m_pLocalJobPool;
    BOOL m_bSynching;

    LoadBalancerContext* m_pLoadBalancerContext;
};


//--------------------------------------------------------------------------------------
// Name: SyncAll
// Desc: Encapsulates a command specific to the LoadBalancer to request it procees all 
//       dispatched jobs before reporting back.
//--------------------------------------------------------------------------------------
class SyncAll : public Job
{
public:
    SyncAll( LoadBalancer* pLoadBalancer );
    virtual ~SyncAll() {}

    void SetSynched( BOOL* pbSynched ) 
        { assert( pbSynched != NULL ); m_pbSynched = pbSynched; }

    virtual JOBRESULT operator () ( Context& context ) 
        { m_pLoadBalancer->Sync(); return JOBRESULT_COMPLETED; }


private:
    static JOBRESULT Finalize( Context& context, void* pData ) 
        { SyncAll* pThis = ( SyncAll* )pData; 
          assert( pThis->m_pbSynched != NULL ); 
          *pThis->m_pbSynched = TRUE; return JOBRESULT_COMPLETED; }

    // Disabling default and copy constructors and assignment operator.
    SyncAll();
    SyncAll( const SyncAll& );
    SyncAll& operator =( const SyncAll& );

    LoadBalancer* m_pLoadBalancer;
    BOOL* m_pbSynched;
    FinalizeFunction m_pFinalizeFunction;
};


//--------------------------------------------------------------------------------------
// Name: TerminateThread
// Desc: Encapsulates a command designated to a specific ThreadableEntity to terminate 
//       itself. The JobScheduler will send one TermintaeThread to each of the 
//       ThreadableEntity that were created.
//--------------------------------------------------------------------------------------
class TerminateThread : public Job
{
public:
    TerminateThread( ThreadableEntity* pThreadableEntity, THREADID ThreadID ) 
        :m_pThreadableEntity( pThreadableEntity ), Job( ThreadID )
        { assert( m_pThreadableEntity != NULL ); }

    virtual ~TerminateThread() {}

    virtual JOBRESULT operator () ( Context& ) 
        { m_pThreadableEntity->Terminate(); return JOBRESULT_COMPLETED; }


private:
    // Disabling default and copy constructors and assignment operator.
    TerminateThread();
    TerminateThread( const TerminateThread& );
    TerminateThread& operator =( const TerminateThread& );

    ThreadableEntity* m_pThreadableEntity;
};


#endif // SCHEDULERPARTS_H