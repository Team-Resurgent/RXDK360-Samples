//--------------------------------------------------------------------------------------
// SchedulerBuilder.h
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef SCHEDULERBUILDER_H
#define SCHEDULERBUILDER_H

#include <xtl.h>
#include "SchedulerParts.h"

class Scheduler;


//--------------------------------------------------------------------------------------
// Name: ResourceManager
// Desc: Provides services for creating and deleting the various components need to
//       assemble a JobScheduler for a specific configuration. 
//
// Note: Resources allocated through an instance of this class will automatically be 
//       destroyed when the instance is destroyed.
//--------------------------------------------------------------------------------------
class ResourceManager
{
public:
    explicit ResourceManager( BOOL bThreadsMaySleep );
    ~ResourceManager();

    void AssignHardwareThread( THREADID ThreadID, UINT nHardwareThread);
    void StartThreads() const { Resume(); }

    // WakeThreads and SleepThreads respectively wake up and put threads to sleep. 
    // If m_bThreadsMaySleep is TRUE, the JobScheduler 
    // will let the worker threads (and the load balancer if running in its own hardware 
    // thread) go to sleep when idle and the WakeThreads function will send an APC to 
    // each threads to wake them up. SleepThreads will do nothing.
    // If m_bThreadsMaySleep is FALSE, the JobScheduler will not let the worker threads 
    // (and load balancer) go to sleep even when idling. Calls to WakeThreads and 
    // SleepThreads in this case will result into suspend and resume system calls for
    // each thread.
    void WakeThreads() const { if( m_bThreadsMaySleep == TRUE ) Wake(); else Resume(); }
    void SleepThreads() const { if( m_bThreadsMaySleep == FALSE ) Suspend(); }

    BridgeEndPoint* NewBridgeEndPoint();
    BridgeEndPoint* NewBridgeEndPoint( BridgeEndPoint* pBridgeEndPoint );

    WorkerThread* NewWorkerThread( THREADID ThreadID, BridgeEndPoint* pBridgeEndPoint );
    LoadBalancer* NewLoadBalancer( UINT nMaxJobs );
    SyncAll* NewSyncAll( LoadBalancer* pLoadBalancer );
    TerminateThread* NewTerminateThread( THREADID ThreadID );

    UINT GetWorkerThreadCount() const;


private:
    void Resume() const;
    void Suspend() const;
    void Wake() const;

    // Disabling default andcopy constructor and assignment operator.
    ResourceManager();
    ResourceManager( const ResourceManager& );
    ResourceManager& operator =( const ResourceManager& );

    // Double the maximum number of worker threads + load balancer
    JobQueueLockFree* m_pJobQueue[ ( THREADID_MAXNUMWORKER + 1 ) * 2 ]; 
    UINT m_nJobQueueCount;

    // Enough for each worker thread and the load balancer.
    BridgeEndPoint* m_pBridgeEndPointA[ THREADID_MAXNUMWORKER + 1 ]; 
    UINT m_nBridgeEndPointACount;

    BridgeEndPoint* m_pBridgeEndPointB[ THREADID_MAXNUMWORKER + 1 ];
    UINT m_nBridgeEndPointBCount;

    WorkerThread* m_pWorkerThread[ THREADID_MAXNUMWORKER ];
    LoadBalancer* m_pLoadBalancer;
    ThreadInfo< HANDLE > m_ThreadHandleInfo;
    BOOL m_bThreadsMaySleep;

    SyncAll* m_pSync;
    ThreadInfo< TerminateThread* > m_TerminateThreadInfo;
};


//--------------------------------------------------------------------------------------
// Name: SchedulerContext
// Desc: Concrete implementation of an execution context to be passed to a job running
//       on the Scheduler. 
//--------------------------------------------------------------------------------------
class SchedulerContext : public Context
{
public:
    explicit SchedulerContext( Scheduler* pScheduler, BridgeEndPoint* pBridgeEndPoint = 0 ) 
        :m_pScheduler( pScheduler ), m_pBridgeEndPoint( pBridgeEndPoint ) 
        { assert( m_pScheduler != NULL ); assert( m_pBridgeEndPoint != NULL ); }

    virtual ~SchedulerContext() {}

    virtual void Dispatch( Job* pJob );

    virtual THREADID GetThreadID() const { return THREADID_MAIN; }

    virtual void AwakenJob( Job* pJob );


private:
    // Disabling default and copy constructors and assignment operator.
    SchedulerContext();
    SchedulerContext( const SchedulerContext& );
    SchedulerContext& operator =( const SchedulerContext& );

    Scheduler* m_pScheduler;
    BridgeEndPoint* m_pBridgeEndPoint;
};


//--------------------------------------------------------------------------------------
// Name: Scheduler
// Desc: Abstract class that specify the interface for a scheduler along with some
//       helper functions. 
//--------------------------------------------------------------------------------------
class Scheduler
{
public:
    Scheduler( ResourceManager* pResourceManager ) 
        : m_bMustTerminate( FALSE ),
          m_pResourceManager( pResourceManager ), m_bThreadsIdle( FALSE ) 
        { assert( m_pResourceManager != NULL ); }

    virtual ~Scheduler() { delete m_pResourceManager; }

    virtual void Dispatch( Job* job ) = 0 ;

    virtual void Sync() = 0;

    virtual void AwakenJob( Job* pJob ) = 0;

    void Terminate() { m_bMustTerminate = TRUE; }


protected:
    void WakeThreads();
    void SleepThreads();

    void TerminateWorkerThreads();

    BOOL m_bMustTerminate;
    ResourceManager* m_pResourceManager;


private:
    // Disabling deafault and copy constructor and assignment operator.
    Scheduler();
    Scheduler( const Scheduler& scheduler );
    Scheduler& operator =( const Scheduler& scheduler );

    BOOL m_bThreadsIdle;
};


//--------------------------------------------------------------------------------------
// Name: SchedulerLocal
// Desc: Implemetation of a Sheduler that uses a local load balancer. i.e.: A load 
//       balancer that runs on the same hardware thread as the Scheduler.
//--------------------------------------------------------------------------------------
class SchedulerLocal : public Scheduler
{
public:
    SchedulerLocal( ResourceManager* pResourceManager, LoadBalancer& loadBalancer ) 
        :m_loadBalancer( loadBalancer ), Scheduler( pResourceManager ) {}

    virtual ~SchedulerLocal();

    virtual void Dispatch( Job* job ) 
        { WakeThreads(); m_loadBalancer.Dispatch( job ); }

    virtual void Sync() { WakeThreads(); m_loadBalancer.Sync(); SleepThreads(); }

    virtual void AwakenJob( Job* pJob ) { assert( false ); } // Should not be called in local mode.


private:
    LoadBalancer& m_loadBalancer;
};


//--------------------------------------------------------------------------------------
// Name: SchedulerRemote
// Desc: Implemetation of a Sheduler that uses a remote load balancer. i.e.: A load
//       balancer that runs on a different hardware thrad than the Scheduler.
//--------------------------------------------------------------------------------------
class SchedulerRemote : public Scheduler
{
public:
    SchedulerRemote( ResourceManager* pResourceManager, 
                     BridgeEndPoint* pBridgeEndPoint, SyncAll* pSyncAll );

    virtual ~SchedulerRemote();

    virtual void Dispatch( Job* pJob ) { WakeThreads(); m_pBridgeEndPoint->Send( pJob ); }

    virtual void Sync();

    virtual void AwakenJob( Job* pJob ) { m_pJobToAwaken = pJob; }


private:
    void ProcessJob( Job* pJob );
    void TerminateLoadBalancerThread();

    SchedulerContext* m_pSchedulerContext;
    BridgeEndPoint*   m_pBridgeEndPoint;
    Job*              m_pJobToAwaken;
    SyncAll*          m_pSyncAll;
};


//--------------------------------------------------------------------------------------
// Name: SchedulerBuilder
// Desc: Contructs a fully constructed / usable Scheduler corresponding to the selected 
//       parameters. 
//--------------------------------------------------------------------------------------
class SchedulerBuilder
{
public:
    SchedulerBuilder()
        :m_bUseMainThread( FALSE ),
         m_bWorkerThreadsMaySleep( FALSE ),
         m_nMaxPendingJobs( 0 ) {}

    ~SchedulerBuilder() {}

    Scheduler* NewScheduler( UINT nExtraThreads, UINT nMaxJobs, BOOL bLoadBalancerOnMain = TRUE  ) const;

    void SetUseMainThread( BOOL bUseMainThread ) { m_bUseMainThread = bUseMainThread; }

    void SetThreadsMaySleep( BOOL bWorkerThreadsMaySleep ) 
        { m_bWorkerThreadsMaySleep = bWorkerThreadsMaySleep; }

    void SetMaxPendingJobs( UINT nMaxPendingJobs ) 
        { m_nMaxPendingJobs = nMaxPendingJobs; }


private:
    Scheduler* NewSchedulerLocal( UINT nExtraThreads, UINT nMaxJobs ) const;
    Scheduler* NewSchedulerRemote( UINT nExtraThreads, UINT nMaxJobs ) const;

    BOOL m_bUseMainThread;
    BOOL m_bWorkerThreadsMaySleep;
    UINT m_nMaxPendingJobs;
};


#endif // SCHEDULERBUILDER_H