//--------------------------------------------------------------------------------------
// TaskScheduler.h
//
// Task Scheduler. Currently round-robin. Uses xmcore for lock-free synchronization.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <stack>
#include <xmcore.h>
#include "Task.h"
#include "Tasks.h"

typedef BOOL (*PFNQUERYRELEASETASKCALLBACK)( const TASKHANDLE* phTask, const void* pTaskData, void* pUserContext );

class TaskScheduler
{
public:
	TaskScheduler();
	~TaskScheduler();

	BOOL Initialize( unsigned int cThreads );

	//puts tasks into the queue
	TASKHANDLE ScheduleTask( const WORD wTaskID,
						     const TASKHANDLE hGroup, 
						     const TASKHANDLE hDependsOnTaskOrGroup, 
						     PFNTASKHANDLER pfnTaskHandler, 
						     void* pTaskData, 
						     const BOOL bForceExecutionOnSameThread = FALSE );

	//timeslice for the TaskScheduler to do all its work
	void DoWork();

	// TaskScheduler still have work to do?
	BOOL DoesWorkExist();

    // Returns the maximum number of tasks that can be scheduled at once
    UINT GetMaxTaskCount() const;

	// Should task threads terminate?
	BOOL ShouldTaskThreadsTerminate();

	// Execute task and immediately release its held resources. For non-overlapped tasks
	HRESULT ExecuteTaskAndRelease( TASKHANDLE* pHandle, Task* pTask );

	// Execute task but don't release its held resources. For overlapped tasks
	HRESULT ExecuteTask( TASKHANDLE* pHandle, Task* pTask );

    // Release task resources
	HRESULT ReleaseTask( TASKHANDLE* pHandle );

    // Release task group resources
	HRESULT ReleaseTaskGroup( TASKHANDLE* pHandle );

    // Release task group resources and query a user-provided callback to determine whether
    // a particular task should be released or not
	HRESULT ReleaseTaskGroupEx( TASKHANDLE* pHandle, PFNQUERYRELEASETASKCALLBACK pfnCallBack, void* pUserContext );

    // Put the task in the priority queue
	void MoveTaskToPriorityQueue( unsigned int indexHandle );

	// Create a task group
	TASKHANDLE CreateTaskGroup();

	// Instruct scheduler to release handle and free any task resources associated with it
	void CloseTaskHandle( unsigned int indexHandle );
	void CloseTaskGroupHandle( unsigned int indexHandle );

    // Is task executing?
    BOOL IsTaskExecuting( const TASKHANDLE* pHandle, DWORD* pdwTickCountSinceExecutionStart = NULL );

    //reschedules a task
    VOID RescheduleTask( const TASKHANDLE* pHandle );

	struct ThreadTaskData
	{
		DWORD								m_dwThreadID;
		HANDLE								m_hThread;
		TaskScheduler*						m_pTaskScheduler;
		XLockFreePriorityQueue<TASKHANDLE>	m_LFPriorityQueue;
	};

	BOOL IsTaskComplete( unsigned int indexHandle, Task** ppTask );

    // Get highest pri task that's ready to be executed
	BOOL GetHighestPriTask( TASKHANDLE** ppHandle );

	BOOL IsTaskGroupComplete( unsigned int indexHandle, TaskGroup** ppTaskGroup );

	// Steal task for another thread's priority queue
	BOOL StealTask( TASKHANDLE** ppHandle );

private:
	int GetFreeHandle( const BOOL bForceExecutionOnSameThread = FALSE );

private:
	TASKHANDLE*										m_pHandles;
	TaskGroup*									    m_pTaskGroups;
	Task*									        m_pTasks;
	ThreadTaskData*									m_pThreadTaskData;
	unsigned int									m_cThreads;
	XLockFreeHashTable<Task>						m_LFHashtableTasks; // key = handle index
	std::deque<int>					                m_StlQueueFreeHandleIndices;
	XLockFreePriorityQueue<TASKHANDLE>				m_LFPriorityQueue;
	XLockFreeQueue<TASKHANDLE>						m_LFQueueIncoming;
	XLockFreeQueue<TASKHANDLE>						m_LFQueueWaitingForPreconditions;
	XLockFreeHashTable<TaskGroup>					m_LFHashtableTaskGroups; // key = handle index
	XLockFreeLockPool								m_LFLockPool;
	TWO_WAY_LOCK									m_lock;
	BOOL											m_bTerminateTaskThreads;
};

template<class T>
class LFHashTableAccessor
{
public:
	LFHashTableAccessor( XLockFreeHashTable<T>& LFHashtable );
	T* GetElement( unsigned int indexHandle );
	~LFHashTableAccessor();
private:
	LFHashTableAccessor( const LFHashTableAccessor& );
	const LFHashTableAccessor& operator=(  const LFHashTableAccessor& );

	XLockFreeHashTable<T>& m_LFHashtable; // key = handle index
};
