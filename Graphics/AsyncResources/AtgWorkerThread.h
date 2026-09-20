//--------------------------------------------------------------------------------------
// AtgWorkerThread.h
//
// Worker thread management class.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ATGWORKERTHREAD_H
#define ATGWORKERTHREAD_H

#include <xtl.h>
#include <xmcore.h>
#include <assert.h>

//--------------------------------------------------------------------------------------
// Name: class IWorkerThreadContext
// Desc: Interface class that defines a worker context that executes work from a worker
//       thread.
//--------------------------------------------------------------------------------------
class IWorkerThreadContext
{
public:
    virtual VOID ThreadStartup( ) {};
    virtual VOID DoWork( VOID* pData ) = NULL;
    virtual VOID ThreadShutdown( ) {};
};

//--------------------------------------------------------------------------------------
// Name: class WorkerThread
// Desc: Worker thread manager that calls a WorkerThreadContext to do the work.
//--------------------------------------------------------------------------------------
class WorkerThread
{
public:
                    WorkerThread( IWorkerThreadContext* pContext, DWORD Processor );
                    ~WorkerThread();

    VOID            AddWork( VOID* pWorkItem );

private:
    static DWORD    StaticThreadEntry( LPVOID pContext );
    VOID            ThreadEntry();

    IWorkerThreadContext* m_pContext;
    XLockFreeQueue <VOID> m_Queue;
    HANDLE m_ThreadHandle;
};

//--------------------------------------------------------------------------------------
// Name: WorkerThread constructor
// Desc: Sets up a lock-free queue for holding work items.  Also starts up the worker
//       thread on the specified hardware thread.
//--------------------------------------------------------------------------------------
inline WorkerThread::WorkerThread( IWorkerThreadContext* pContext, DWORD Processor ) : m_pContext( pContext )
{
    // Create a lock-free queue that blocks on Remove().
    XLOCKFREE_CREATE xlfinfo = { 0 };
    xlfinfo.attributes = XLOCKFREE_REMOVE_WAIT;
    xlfinfo.removeWaitTime = INFINITE;
    xlfinfo.allocationLength = 128;

    m_Queue.Initialize( &xlfinfo );

    m_ThreadHandle = CreateThread( NULL, 0, &StaticThreadEntry, this, CREATE_SUSPENDED, NULL );
    assert( m_ThreadHandle );   // Tell code analysis to assume that CreateThread won't fail.
    XSetThreadProcessor( m_ThreadHandle, Processor );
    ResumeThread( m_ThreadHandle );
}

//--------------------------------------------------------------------------------------
// Name: WorkerThread destructor
// Desc: Tears down the worker thread.
//--------------------------------------------------------------------------------------
inline WorkerThread::~WorkerThread()
{
    // Relaxing SAL requirements a bit to allow placing a NULL pointer 
    // into the queue. This will cause the worker thread to exit.
#pragma warning ( suppress : 6387 6309 )
    m_Queue.Add( NULL );

    WaitForSingleObject( m_ThreadHandle, INFINITE );
    CloseHandle( m_ThreadHandle );
}

//--------------------------------------------------------------------------------------
// Name: AddWork
// Desc: Inserts a work item into the back of the queue.
//--------------------------------------------------------------------------------------
inline VOID WorkerThread::AddWork( VOID* pWorkItem )
{
    m_Queue.Add( pWorkItem );
}

//--------------------------------------------------------------------------------------
// Name: StaticThreadEntry
// Desc: Static thread entry point for all worker threads.  Passes execution off to the
//       instance's ThreadEntry() method.
//--------------------------------------------------------------------------------------
inline DWORD WorkerThread::StaticThreadEntry( LPVOID pWorkerThreadObject )
{
    ( ( WorkerThread* )pWorkerThreadObject )->ThreadEntry();
    return 0;
}

//--------------------------------------------------------------------------------------
// Name: ThreadEntry
// Desc: Instance-specific thread entry point for the worker thread.  Removes work items
//       from the queue and executes the work function on the context.
//--------------------------------------------------------------------------------------
inline VOID WorkerThread::ThreadEntry()
{
    m_pContext->ThreadStartup();

    while( VOID* pWorkItem = m_Queue.Remove() )
    {
        // Execute the work function.
        m_pContext->DoWork( pWorkItem );
    }

    m_pContext->ThreadShutdown();
}

#endif // ATGWORKERTHREAD_H
