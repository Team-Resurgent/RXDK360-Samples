//--------------------------------------------------------------------------------------
// Scheduler.h
//
// Contains an implementation of a task scheduler, consisting of the TaskScheduler,
// ThreadPool and WorkerThread classes.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <xmcore.h>

namespace ATG
{
class ThreadPool;
class Task;

enum ATG_TASK_COMMAND
{
    PERFORM_TASK = 0,
    TERMINATE_OPERATION,
    TERMINATE_COMPLETE
};

typedef HRESULT ( CALLBACK*TaskFunction )( void* context );

//-----------------------------------------------------------------------------
// Name: Task
// Desc: Class that handles thread tasks.
//-----------------------------------------------------------------------------
class Task
{
public:
                        Task() : taskResult( S_OK ),
                                 command( PERFORM_TASK )
                        {
                        }

    void                SetCommand( ATG_TASK_COMMAND c )
    {
        command = c;
    }

    ATG_TASK_COMMAND    GetCommand()
    {
        return command;
    }

    virtual HRESULT     Start()
    {
        return S_OK;
    };

    virtual HRESULT     Process()
    {
        return S_OK;
    };

    virtual HRESULT     Complete()
    {
        return S_OK;
    };

protected:

    HRESULT taskResult;
    ATG_TASK_COMMAND command;

};

typedef XLockFreePriorityQueue <Task> WorkQueue;
typedef XLockFreeQueue <Task> ResultQueue;


//-----------------------------------------------------------------------------
// Name: TaskScheduler
// Desc: Scheduler accepts tasks and sends them to the thread pool.
//-----------------------------------------------------------------------------
class TaskScheduler
{
public:

            TaskScheduler();
            ~TaskScheduler();

    HRESULT Initialize( XLOCKFREE_LOG l );
    HRESULT Shutdown();

    HRESULT ScheduleTask( Task* task );
    HRESULT ProcessResults( int requiredJobs );

private:

    Task completeMessage;

    WorkQueue* work;
    ResultQueue* result;
    ThreadPool* threadPool;

    XLOCKFREE_LOG errorLog;
};

class ArrayFunctions
{
public:
    static int          compare( int p1,
                                 int p2 );
    static unsigned int hash( unsigned int key,
                              unsigned int buckets );
    static unsigned int bucketSize( unsigned int buckets );
};


//-----------------------------------------------------------------------------
// Name: ThreadWorker
// Desc: Worker only talks to the thread pool.
//-----------------------------------------------------------------------------
class ThreadWorker
{
public:
                            ThreadWorker( int id,
                                          WorkQueue* jobQueue,
                                          ResultQueue* completedJobs );
                            ~ThreadWorker();

    HRESULT                 Initialize( int hardwareThread,
                                        XLOCKFREE_LOG l,
                                        HANDLE* handle );

private:
    static DWORD CALLBACK   Start( void* information );
    DWORD                   Process();

    HRESULT lastError;

    WorkQueue* work;
    ResultQueue* result;
    XLOCKFREE_LOG errorLog;
    int workerId;
};


//-----------------------------------------------------------------------------
// Name: ThreadPool
// Desc: Thread pool class.
//-----------------------------------------------------------------------------
class ThreadPool
{
public:
                            ThreadPool( WorkQueue* incomingJobs,
                                        ResultQueue* jobResults );
                            ~ThreadPool();

    HRESULT                 Initialize( XLOCKFREE_LOG l );
    HRESULT                 Initialize( XLOCKFREE_LOG l,
                                        UINT coreForPool,
                                        UINT workerThreads,
                                        UINT startingCoreForWorkers,
                                        UINT coresForWorkers );

private:
    static DWORD CALLBACK   Start( void* information );
    DWORD                   Process();

    HRESULT                 Shutdown();

    Task completeMessage;

    HRESULT lastError;

    UINT numberOfWorkers;
    UINT hardwareCore;
    UINT firstWorkerCore;
    UINT numberOfWorkerCores;

    HANDLE* workHandles;
    ThreadWorker** workers;
    WorkQueue** threadWork;
    WorkQueue* work;
    ResultQueue* result;

    XLOCKFREE_LOG errorLog;
};

}

#endif // SCHEDULER_H
