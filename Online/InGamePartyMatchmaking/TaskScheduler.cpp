//--------------------------------------------------------------------------------------
// TaskScheduler.cpp
//
// Task Scheduler implementation
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "XPlat.h"

// Forward declaration
DWORD ThreadTaskProc( void* lpThreadParameter );

//
// LFHashTableAccessor
//
template<class T>
LFHashTableAccessor<T>::LFHashTableAccessor( XLockFreeHashTable<T>& LFHashtable ) :
m_LFHashtable( LFHashtable )
{
}

template<class T>
T* LFHashTableAccessor<T>::GetElement( unsigned int indexHandle )
{
    T* pT = NULL;

    if( indexHandle > MAX_HANDLE_INDEX )
    {
        return pT;
    }

    // Note that XLockFreeHashTable only supports remove
    // a value by key, and not finding, so we'll have to re-add the task manually
    if( SUCCEEDED( m_LFHashtable.Remove( indexHandle, &pT ) ) )
    {
        m_LFHashtable.Add( indexHandle, pT );
    }
    return pT;
}

template<class T>
LFHashTableAccessor<T>::~LFHashTableAccessor()
{
}

//
// TaskScheduler
//
TaskScheduler::TaskScheduler() :
    m_bTerminateTaskThreads( FALSE ),
    m_cThreads( 0 ),
    m_pThreadTaskData( NULL ),
    m_pTasks( NULL ),
    m_pTaskGroups( NULL )
{
}

TaskScheduler::~TaskScheduler()
{
    // Instruct all threads to exit and block until they do
    m_bTerminateTaskThreads = TRUE;
    BOOL bDone = TRUE;
    while( !bDone )
    {
        for( unsigned int iThread = 0; iThread < m_cThreads; ++iThread )
        {
            DWORD dwExitCode;			
            if( !GetExitCodeThread( m_pThreadTaskData[ iThread ].m_hThread, &dwExitCode ) )
            {
                // Thread hasn't exited yet
                bDone = FALSE;
                break;
            }
        }
    }

    // Acquire exclusive lock to delete destroy all other lockfree resources
    m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

    m_LFQueueWaitingForPreconditions.Destroy();
    m_LFPriorityQueue.Destroy();
    m_LFQueueIncoming.Destroy();
    m_LFHashtableTasks.Destroy();
    m_LFHashtableTaskGroups.Destroy();

    // Release exclusive lock
    m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );

    // Destroy lock pool resources
    m_LFLockPool.Destroy();

    delete [] m_pHandles;
    delete [] m_pThreadTaskData;
    delete [] m_pTasks;
    delete [] m_pTaskGroups;
}

BOOL TaskScheduler::Initialize( unsigned int cThreads )
{
    m_cThreads = cThreads;

    if( FAILED( m_LFHashtableTasks.Initialize( 31 ) ) )
    {
        DebugSpew( "TaskScheduler::Initialize: m_LFHashtableTasks.Initialize() failed!\n" );
        return FALSE;
    }

    if( FAILED( m_LFHashtableTaskGroups.Initialize( 31 ) ) )
    {
        DebugSpew( "TaskScheduler::Initialize: m_LFHashtableTaskGroups.Initialize() failed!\n" );
        return FALSE;
    }

    if( FAILED( m_LFPriorityQueue.Initialize() ) )
    {
        DebugSpew( "TaskScheduler::Initialize: m_LFPriorityQueue.Initialize() failed!\n" );
        return FALSE;
    }

    if( FAILED( m_LFQueueIncoming.Initialize() ) )
    {
        DebugSpew( "TaskScheduler::Initialize: m_LFQueueIncoming.Initialize() failed!\n" );
        return FALSE;
    }

    if( FAILED( m_LFQueueWaitingForPreconditions.Initialize() ) )
    {
        DebugSpew( "TaskScheduler::Initialize: m_LFQueueWaitingForPreconditions.Initialize() failed!\n" );
        return FALSE;
    }

    if( FAILED( m_LFLockPool.Initialize() ) )
    {
        DebugSpew( "TaskScheduler::Initialize: m_LFLockPool.Initialize() failed!\n" );
        return FALSE;
    }

    // Allocate memory for our task handles, tasks, and taskgroups. Plan for the worst case
    m_pHandles      = new TASKHANDLE[ MAX_HANDLE_INDEX + 1 ];
    m_pTasks        = new Task[ MAX_HANDLE_INDEX + 1 ];
    m_pTaskGroups   = new TaskGroup[ MAX_HANDLE_INDEX + 1 ];

    // Initialize stack of free handle IDs. First free handle index is 1
    for( int i = MIN_HANDLE_INDEX; i <= MAX_HANDLE_INDEX; ++i )
    {
        m_StlQueueFreeHandleIndices.push_back( i );
    }

    // Allocate memory for task thread data
    m_pThreadTaskData = new ThreadTaskData[ cThreads ];

    // Create task handler threads, initially suspended
    for( unsigned int iThread = 0; iThread < cThreads; ++iThread )
    {
        DWORD dwThreadID = 0;
        HANDLE hThread = CreateThread(  NULL, 
                                        0, 
                                        (LPTHREAD_START_ROUTINE)ThreadTaskProc, 
                                        &m_pThreadTaskData[ iThread ], 
                                        CREATE_SUSPENDED, 
                                        &dwThreadID ); 

        m_pThreadTaskData[ iThread ].m_hThread			= hThread;
        m_pThreadTaskData[ iThread ].m_dwThreadID		= dwThreadID; 
        m_pThreadTaskData[ iThread ].m_pTaskScheduler	= this;

        if( SUCCEEDED( m_pThreadTaskData[ iThread ].m_LFPriorityQueue.Initialize() ) )
        {
            DebugSpew( "TaskScheduler::Initialize: Created thread ID: %d\n", dwThreadID );
        }
        else
        {
            DebugSpew( "TaskScheduler::Initialize: Failed to initialize thread ID: %d!\n", dwThreadID );
            return FALSE;
        }
    }

    // On Xbox 360, schedule threads in a round-robin fashion on hardware threads 2-5
    for( unsigned int iThread = 0; iThread < cThreads; ++iThread )
    {
        if( m_pThreadTaskData[ iThread ].m_hThread )
        {
            #ifdef _XBOX
            int iHWThread = ( iThread + 2 ) % 6;
            if( iHWThread == 0 )
            {
                iHWThread = 2;
            }
            else if( iHWThread == 1 )
            {
                iHWThread = 3;
            }
            XSetThreadProcessor( m_pThreadTaskData[ iThread ].m_hThread, iHWThread );
            #endif
            ResumeThread( m_pThreadTaskData[ iThread ].m_hThread );
        }
    }

    return TRUE;
}

UINT TaskScheduler::GetMaxTaskCount() const
{
    return ( MAX_HANDLE_INDEX - MIN_HANDLE_INDEX );
}

int TaskScheduler::GetFreeHandle( const BOOL bForceExecutionOnSameThread )
{
    int index = 0;

    if( m_StlQueueFreeHandleIndices.empty() )
    {
        DebugSpew( "TaskScheduler::GetFreeHandle: No more free handles left!\n" );
        return INVALID_HANDLE_INDEX;
    }

    if( !m_StlQueueFreeHandleIndices.empty() )
    {
        index = m_StlQueueFreeHandleIndices.front(); 
        m_StlQueueFreeHandleIndices.pop_front();
    }

    ZeroMemory( &m_pHandles[ index ], sizeof( TASKHANDLE ) );

    TASKHANDLE& handle				= m_pHandles[ index ];
    handle.m_info.m_index			= index;
    handle.m_indexDependsOn			= INVALID_HANDLE_INDEX;
    handle.m_dwPreferredThreadId	= ( bForceExecutionOnSameThread ) ? GetCurrentThreadId() : 0;

    return index;
}

void TaskScheduler::CloseTaskHandle( unsigned int indexHandle )
{
    // Acquire exclusive lock
    m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

    // Remove this handle's entry in our m_LFHashtableTasks hashtable
    m_LFHashtableTasks.Remove( indexHandle );

    // ID used by this handle is freely available again. Add it to the enter of our queue to reduce odds of
    // the ID being used against immediately and mucking up the current task dependency chain
    m_StlQueueFreeHandleIndices.push_back( indexHandle );

    DebugSpew( "TaskScheduler::CloseTaskHandle: Removing task at index: %u and freeing handle resources\n", 
                indexHandle );

    // Release acquired lock
    m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );
}

void TaskScheduler::CloseTaskGroupHandle( unsigned int indexHandle )
{
    // Acquire exclusive lock
    m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

    // Remove this handle's entry in our m_LFHashtableTasks hashtable
    m_LFHashtableTaskGroups.Remove( indexHandle );

    // ID used by this handle is freely available again. Add it to the enter of our queue to reduce odds of
    // the ID being used against immediately and mucking up the current task dependency chain
    m_StlQueueFreeHandleIndices.push_back( indexHandle );

    DebugSpew( "TaskScheduler::CloseTaskGroupHandle: Removing task group at index: %u and freeing handle resources\n", 
                indexHandle );

    // Release acquired lock
    m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );
}

TASKHANDLE TaskScheduler::CreateTaskGroup()
{
    // Acquire exclusive lock
    m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

    // Get a task handle. We get task group handles from the same pool
    // as individual task handles
    unsigned int indexHandle = GetFreeHandle();

    if( indexHandle == INVALID_HANDLE_INDEX )
    {
        TASKHANDLE dummy = {0};
        return dummy;
    }

    TASKHANDLE& handle			= m_pHandles[ indexHandle ];
    handle.m_info.m_bIsTaskGroup  = TRUE;

    // The TaskGroup instance we'll be using is indexed at the same indexHandle
    TaskGroup* pTaskGroup			= &m_pTaskGroups[ indexHandle ];
    pTaskGroup->m_dwCreationTick	= GetTickCount();

    // Add handle id/TaskGroup pair to our hashtable of tasks
    DebugSpew( "TaskScheduler::ScheduleTask: Adding task group index: %u\n", indexHandle );

    m_LFHashtableTaskGroups.Add( indexHandle, pTaskGroup ); 

    // Release acquired lock
    m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );

    return handle;
}

VOID TaskScheduler::RescheduleTask( const TASKHANDLE* pHandle )
{
    if( !pHandle )
    {
        return;
    }

    // Acquire exclusive lock
    m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

    // Re-add handle pointer to queue of incoming tasks
    m_LFQueueIncoming.Add( const_cast<TASKHANDLE*>(pHandle) );

    DebugSpew(  "TaskScheduler::RescheduleTask: Incoming task at index:%u;" \
                "task area/number/priority:%x/%x/%x;" \
                "task group:%u\n", 
                pHandle->m_info.m_index,
                pHandle->m_info.m_area, 
                pHandle->m_info.m_number, 
                pHandle->m_info.m_priority,
                pHandle->m_indexGroup );

    // Release exclusive lock
    m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );
}

TASKHANDLE TaskScheduler::ScheduleTask( const WORD wTaskID,
                                       const TASKHANDLE hGroup, 
                                       const TASKHANDLE hDependsOnTaskOrGroup, 
                                       PFNTASKHANDLER pfnTaskHandler, 
                                       void* pTaskData,
                                       const BOOL bForceExecutionOnSameThread )
{	
    // Acquire exclusive lock
    m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

    // Get a task handle
    unsigned int indexHandle = GetFreeHandle( bForceExecutionOnSameThread );

    if( indexHandle == INVALID_HANDLE_INDEX )
    {
        TASKHANDLE dummy = {0};

        // Release exclusive lock
        m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );

        return dummy;
    }

    TASKHANDLE& handle			 = m_pHandles[ indexHandle ];
    handle.m_info.m_index		 = indexHandle;

    // Encode task id into the handle
    handle.m_info.m_area		    = TASK_AREA( wTaskID );
    handle.m_info.m_number		    = TASK_NUMBER( wTaskID );
    handle.m_info.m_priority	    = TASK_PRIORITY( wTaskID );
    handle.m_info.m_bReschedulable	= TASK_RESCHEDULEABLE( wTaskID );
    handle.m_info.m_bAsync	        = TASK_ASYNC( wTaskID );

    // Encode task dependency into the handle
    handle.m_indexDependsOn = hDependsOnTaskOrGroup.m_info.m_index;

    // The Task instance we'll be using is indexed at the same indexHandle
    Task* pTask			            = &m_pTasks[ indexHandle ];
    pTask->m_pfnTaskHandler	        = pfnTaskHandler;
    pTask->m_pData			        = pTaskData;
    pTask->m_state			        = TASK_STATE_IN_PRECONDITION_QUEUE_WAITING;
    pTask->m_dwCreationTick	        = GetTickCount();
    pTask->m_dwExecutionStartTick	= (DWORD)-1;
    pTask->m_hr				        = S_OK;

    //DebugSpew( "TaskScheduler::ScheduleTask: New task at index: %u; priority:%u\n", 
    //			 indexHandle, handle.m_info.m_priority );

    Task* pTaskDependsOn = NULL;
    TaskGroup* pTaskGroupDependsOn = NULL;

    if( !IsTaskComplete( handle.m_indexDependsOn, &pTaskDependsOn ) )
    {
        handle.m_dwDependsOnCreationTick = pTaskDependsOn->m_dwCreationTick;

        DebugSpew( "TaskScheduler::ScheduleTask: Task at index: %u depends on task at index %u\n", 
                    indexHandle, handle.m_indexDependsOn );
    }
    else if( !IsTaskGroupComplete( handle.m_indexDependsOn, &pTaskGroupDependsOn ) )
    {
        handle.m_dwDependsOnCreationTick = pTaskGroupDependsOn->m_dwCreationTick;

        DebugSpew( "TaskScheduler::ScheduleTask: Task at index: %u depends on task group at index %u\n", 
                    indexHandle, handle.m_indexDependsOn );
    }

    // Add handle id/Task pair to our hashtable of tasks
    m_LFHashtableTasks.Add( indexHandle, pTask ); 

    // If task is part of a task group, add it to the group
    TaskGroup* pTaskGroup = NULL;
    if( !IsTaskGroupComplete( hGroup.m_info.m_index, &pTaskGroup ) )
    {
        pTaskGroup->m_LFTasks.Add( handle.m_info.m_index, &handle );
        handle.m_indexGroup = hGroup.m_info.m_index;
    }
    else
    {
        handle.m_indexGroup = INVALID_HANDLE_INDEX;
    }

    // Add handle pointer to queue of incoming tasks
    m_LFQueueIncoming.Add( &m_pHandles[ indexHandle ] );

    DebugSpew(  "TaskScheduler::ScheduleTask: Incoming task at index:%u;" \
                "task area/number/priority:%x/%x/%x;" \
                "task group:%u\n", 
                indexHandle,
                handle.m_info.m_area, 
                handle.m_info.m_number, 
                handle.m_info.m_priority,
                handle.m_indexGroup );

    // Release exclusive lock
    m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );

    // Return handle
    return handle;
}

void TaskScheduler::MoveTaskToPriorityQueue( unsigned int indexHandle )
{
    DebugSpew( "TaskScheduler::MoveTaskToPriorityQueue: Moving task index: %u; priority:%u\n", 
                m_pHandles[ indexHandle ].m_info.m_index, 
                m_pHandles[ indexHandle ].m_info.m_priority );

    // If task has a preferred thread to run on, add the task to that thread's priority queue.
    // Otherwise add to global priority queue
    if( m_pHandles[ indexHandle ].m_dwPreferredThreadId )
    {
        for( unsigned int iThread = 0; iThread < m_cThreads; ++iThread )
        {
            if( m_pThreadTaskData[ iThread ].m_dwThreadID == m_pHandles[ indexHandle ].m_dwPreferredThreadId )
            {
                //
                // Found preferred thread. Add task to its priority queue. Note that
                // we'll need exlusive access to this queue for the add

                // Acquire exclusive lock
                m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

                // Add task to thread's queue
                m_pThreadTaskData[ iThread ].m_LFPriorityQueue.Add( m_pHandles[ indexHandle ].m_info.m_priority, &m_pHandles[ indexHandle ] );

                // Release lock
                m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );

                DebugSpew( "TaskScheduler::MoveTaskToPriorityQueue: Task index: %u assigned to thread: %d priority queue\n", 
                            m_pHandles[ indexHandle ].m_info.m_index, 
                            m_pHandles[ indexHandle ].m_dwPreferredThreadId );
                return;
            }
        }
    }

    // Add task to global priority queue
    m_LFPriorityQueue.Add( m_pHandles[ indexHandle ].m_info.m_priority, &m_pHandles[ indexHandle ] );
}

BOOL TaskScheduler::IsTaskComplete( unsigned int indexHandle, Task** ppTask )
{	
    BOOL bRet = FALSE;

    // Acquire exclusive lock
    m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

    Task* pTask = NULL;
    { // Scope for LFHashTableAccessor
        LFHashTableAccessor<Task> taskAccessor( m_LFHashtableTasks );
        pTask = taskAccessor.GetElement( indexHandle );
    }

    if( ppTask )
    {
        *ppTask = pTask;
    }

    if( !pTask || ( pTask->m_state == TASK_STATE_DONE ) )
    {
        bRet = TRUE;
    }

    // Release exclusive lock
    m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );

    return bRet;
}

BOOL TaskScheduler::IsTaskGroupComplete( unsigned int indexHandle, TaskGroup** ppTaskGroup )
{
    BOOL bRet = FALSE;

    // Acquire exclusive lock
    m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

    TaskGroup* pTaskGroup = NULL;
    { // Scope for LFHashTableAccessor
        LFHashTableAccessor<TaskGroup> taskAccessor( m_LFHashtableTaskGroups );
        pTaskGroup = taskAccessor.GetElement( indexHandle );
    }

    if( ppTaskGroup )
    {
        *ppTaskGroup = pTaskGroup;
    }

    if( !pTaskGroup )
    {
        bRet = TRUE;
    }

    // Release exclusive lock
    m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );

    return bRet;
}

BOOL TaskScheduler::ShouldTaskThreadsTerminate()
{
    BOOL bTerminateTaskThreads = FALSE;

    // Acquire exclusive lock
    m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

    bTerminateTaskThreads = m_bTerminateTaskThreads;

    // Release exclusive lock
    m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );

    return bTerminateTaskThreads;
}

BOOL TaskScheduler::DoesWorkExist()
{
    BOOL bRet = FALSE;

    // Acquire exclusive lock
    m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

    //DebugSpew( "TaskScheduler::DoesWorkExist: Priority queue size: %d\n", 
    //			 m_LFPriorityQueue.GetEntryCount() );

    if( m_LFHashtableTasks.GetEntryCount() || m_LFPriorityQueue.GetEntryCount() )
    {
        bRet = TRUE;
    }

    // Release exclusive lock
    m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );

    return bRet;
}

BOOL TaskScheduler::GetHighestPriTask( TASKHANDLE** ppHandle )
{
    BOOL bRet = TRUE;

    // Acquire exclusive lock
    m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

    if( !SUCCEEDED( m_LFPriorityQueue.RemoveFirst( ppHandle ) ) )
    {
        bRet = FALSE;
    }

    // Release exclusive lock
    m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );

    return bRet;
}

BOOL TaskScheduler::StealTask( TASKHANDLE** ppHandle )
{
    // Acquire exclusive lock
    m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

    // $TODO

    // Release exclusive lock
    m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );

    return FALSE;
}

HRESULT TaskScheduler::ExecuteTask( TASKHANDLE* pHandle, Task* pTask )
{
    // Acquire exclusive lock
    m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

    // Reconstruct TASKID and then call task handler with it along with the task handle and data
    const WORD taskID = TASKID( pHandle->m_info.m_area, pHandle->m_info.m_number, pHandle->m_info.m_priority, pHandle->m_info.m_bReschedulable, pHandle->m_info.m_bAsync );
    HRESULT hr = pTask->m_pfnTaskHandler( taskID, pHandle, pTask->m_pData );
    pTask->m_hr = hr;

    // Release exclusive lock
    m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );

    return hr;
}

BOOL TaskScheduler::IsTaskExecuting( const TASKHANDLE* pHandle, DWORD* pdwTickCountSinceExecutionStart )
{
    // Get Task instance from our hashtable.
    Task* pTask = NULL;
    if( IsTaskComplete( pHandle->m_info.m_index, &pTask ) )
    {
        return FALSE;
    }

    if( pTask->m_state != TASK_STATE_IN_PRIORITY_QUEUE_EXECUTING )
    {
        return FALSE;
    }

    if( pdwTickCountSinceExecutionStart )
    {
        *pdwTickCountSinceExecutionStart = GetTickCount() - pTask->m_dwExecutionStartTick;
    }

    return TRUE;
}

HRESULT TaskScheduler::ReleaseTask( TASKHANDLE* pHandle )
{
    // Acquire exclusive lock
    m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

    // Remove task from task group
    TaskGroup* pTaskGroup = NULL;
    if( !IsTaskGroupComplete( pHandle->m_indexGroup, &pTaskGroup ) )
    {
        pTaskGroup->m_LFTasks.Remove( pHandle->m_info.m_index );

        // If no tasks left in the task group, then close task group handle
        if( pTaskGroup->m_LFTasks.GetEntryCount() == 0 )
        {
            CloseTaskGroupHandle( pHandle->m_indexGroup );
        }
    }

    // Close task handle
    CloseTaskHandle( pHandle->m_info.m_index );

    // Release exclusive lock
    m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );

    return S_OK;
}

HRESULT TaskScheduler::ReleaseTaskGroup( TASKHANDLE* pHandle )
{
    // Acquire exclusive lock
    m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

    // Remove task from task group
    TaskGroup* pTaskGroup = NULL;
    if( !IsTaskGroupComplete( pHandle->m_indexGroup, &pTaskGroup ) )
    {
        // Remove all tasks from the task group
        TASKHANDLE* pHandleTask;
        while( SUCCEEDED( pTaskGroup->m_LFTasks.RemoveFirst( &pHandleTask ) ) )
        {
            // Close task handle
            CloseTaskHandle( pHandleTask->m_info.m_index );
        }

        // Close task group handle
        CloseTaskGroupHandle( pHandle->m_indexGroup );
    }

    // Release exclusive lock
    m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );

    return S_OK;
}

HRESULT TaskScheduler::ReleaseTaskGroupEx( TASKHANDLE* pHandle, PFNQUERYRELEASETASKCALLBACK pfnCallBack, void* pUserContext )
{
    // Acquire exclusive lock
    m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

    // Remove each task from task group, but only if user-supplied callback returns true for the task
    TaskGroup* pTaskGroup = NULL;
    BOOL bAllTasksRemoved = TRUE;
    if( !IsTaskGroupComplete( pHandle->m_indexGroup, &pTaskGroup ) )
    {
        //
        // Remove tasks from the task group using the provided callback to determine
        // which tasks we can remove
        //

        // The way XLockFreeHashTable works, to iterate through a XLockFreeHashTable,
        // we'll have to remove the entry, and if the callback query fails,
        // reinsert it into the hashtable. Since we're reinserting into a hashtable,
        // we can't just simply reinsert while in our while loop below, so we'll
        // have to use a temporary XLockFreeHashTable instance and drain from there
        // into pTaskGroup->m_LFTasks when we're done
        XLockFreeHashTable<TASKHANDLE> htTemp;

        while( !pTaskGroup->m_LFTasks.IsEmpty() )
        {
            // Get first entry in m_LFTasks
            TASKHANDLE* pHandleTask;
            while( SUCCEEDED( pTaskGroup->m_LFTasks.RemoveFirst( &pHandleTask ) ) )
            {
                // Get Task instance from our hashtable for this handle.
                Task* pTask = NULL;
                if( !IsTaskComplete( pHandleTask->m_info.m_index, &pTask ) )
                {
                    // Query callback if we can remove this task
                    if( pfnCallBack( pHandleTask, pTask->m_pData, pUserContext ) )
                    {
                        CloseTaskHandle( pHandleTask->m_info.m_index );
                    }
                    else
                    {
                        // Can't remove this task. We need to reinsert it
                        bAllTasksRemoved = FALSE;
                        htTemp.Add( pHandleTask->m_info.m_index, pHandleTask );
                    }
                }
            }
        }

        // Close task group handle if all tasks for it have been removed
        if( bAllTasksRemoved )
        {
            CloseTaskGroupHandle( pHandle->m_indexGroup );
        }
        else
        {
            // reinsert still running tasks into pTaskGroup->m_LFTasks
            TASKHANDLE* pHandleTask;
            while( SUCCEEDED( htTemp.RemoveFirst( &pHandleTask ) ) )
            {
                pTaskGroup->m_LFTasks.Add( pHandleTask->m_info.m_index, pHandleTask );
            }
        }
    }

    // Release exclusive lock
    m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );

    // return S_OK if all tasks are removed. Otherwise return E_FAIL
    return ( bAllTasksRemoved ) ? S_OK : E_FAIL;
}

HRESULT TaskScheduler::ExecuteTaskAndRelease( TASKHANDLE* pHandle, Task* pTask )
{
    // Acquire exclusive lock
    m_LFLockPool.Acquire( XLF_LOCK_EXCLUSIVE, &m_lock );

    // Reconstruct TASKID and then call task handler with it along with the task handle and data
    const WORD taskID = TASKID( pHandle->m_info.m_area, pHandle->m_info.m_number, pHandle->m_info.m_priority, pHandle->m_info.m_bReschedulable, pHandle->m_info.m_bAsync );
    HRESULT hr = pTask->m_pfnTaskHandler( taskID, pHandle, pTask->m_pData );
    pTask->m_hr = hr;

    // Release the task
    hr = ReleaseTask( pHandle );

    // Release exclusive lock
    m_LFLockPool.Release( XLF_LOCK_EXCLUSIVE, &m_lock );

    return hr;
}

void TaskScheduler::DoWork()
{
    //
    // Timeslice for task schedule to move tasks from incoming queue to priority queue
    // of tasks ready for execution
    //

    //
    // If TaskScheduler is initialized with 0 worker threads
    // then execute the task here, once per DoWork call
    TASKHANDLE* pHandle;
    while( ( m_cThreads == 0 ) && GetHighestPriTask( &pHandle ) )
    {
        if( pHandle )
        {						
            // Get Task instance from our hashtable.
            Task* pTask = NULL;
            if( IsTaskComplete( pHandle->m_info.m_index, &pTask ) )
            {
                CloseTaskHandle( pHandle->m_info.m_index );
                continue;
            }

            DebugSpew( "TaskScheduler::DoWork: Executing task index: %u; priority:%u\n", 
                        pHandle->m_info.m_index, 
                        pHandle->m_info.m_priority );

            // Update task state and set m_dwExecutionStartTick if the task isn't
            // already executing (as is the case for reschedulable tasks)
            if( pTask->m_state != TASK_STATE_IN_PRIORITY_QUEUE_EXECUTING )
            {
                pTask->m_state = TASK_STATE_IN_PRIORITY_QUEUE_EXECUTING;
                pTask->m_dwExecutionStartTick = GetTickCount();
            }

            // For non-rescheduleable tasks, immediately release any resources held by the task
            HRESULT hr = S_OK;
            if( !pHandle->m_info.m_bReschedulable )
            {
                hr = ExecuteTaskAndRelease( pHandle, pTask );
            }
            else
            {
                // Rescheduleable task. Don't immediately release resources. It's up to the task handler code
                // to do so
                hr = ExecuteTask( pHandle, pTask );
            }

            if( !SUCCEEDED( hr ) )
            {
                DebugSpew( "TaskScheduler::DoWork: Task index: %u failed with hr=0x%x\n", 
                    pHandle->m_info.m_index, 
                    hr );
            }
        }
    }

    //
    // For all tasks in the incoming queue, move to checking for preconditions queue
    //
    pHandle = NULL;
    while( SUCCEEDED( m_LFQueueIncoming.Remove( &pHandle ) ) )
    {
        DebugSpew( "TaskScheduler::DoWork: Waiting for preconditions for task index: %u; priority:%u\n", 
                    pHandle->m_info.m_index, 
                    pHandle->m_info.m_priority );

        m_LFQueueWaitingForPreconditions.Add( pHandle );
    }

    //
    // For all tasks in the m_LFQueueWaitingForPreconditions queue, check if preconditions have been met. 
    // If so, move the task to our priority queue of tasks to execute. Otherwise keep in queue of
    // tasks waiting for their preconditions
    //
    XLockFreeStack<TASKHANDLE> tempStack;
    if( FAILED( tempStack.Initialize() ) )
    {	
        DebugSpew( "TaskScheduler::DoWork: Failed initializing temp XLockFreeStack!\n" );
        return;
    }

    pHandle = NULL;
    while( SUCCEEDED( m_LFQueueWaitingForPreconditions.Remove( &pHandle ) ) )
    {
        Task* pTask = NULL;
        if( IsTaskComplete( pHandle->m_info.m_index, &pTask ) )
        {
            CloseTaskHandle( pHandle->m_info.m_index );
            continue;
        }

        const unsigned int indexDependsOn = pHandle->m_indexDependsOn;

        BOOL bDoPreconditionsStillExist = FALSE;

        Task* pDependsOnTask = NULL;
        TaskGroup* pDependsOnTaskGroup = NULL;

        if( !IsTaskComplete( indexDependsOn, &pDependsOnTask ) )
        {
            bDoPreconditionsStillExist = TRUE;
        }
        else if( !IsTaskGroupComplete( indexDependsOn, &pDependsOnTaskGroup ) )
        {
            bDoPreconditionsStillExist = TRUE;
        }

        if( bDoPreconditionsStillExist )
        {
            // Preconditions still exists. Need to re-add this task 
            // into our queue of tasks waiting for preconditions,
            tempStack.Push( pHandle );
        }
        else
        {
            // No preconditions - Move task to priority queue
            MoveTaskToPriorityQueue( pHandle->m_info.m_index );

            // Update task state. Only do so if the task isn't already executing,
            // as is the case for reschedulable tasks
            if( pTask->m_state != TASK_STATE_IN_PRIORITY_QUEUE_EXECUTING )
            {
                pTask->m_state = TASK_STATE_IN_PRIORITY_QUEUE_WAITING;
            }
        }
    }

    // Re-populate m_LFQueueWaitingForPreconditions with items in tempStack
    pHandle = NULL;
    while( SUCCEEDED( tempStack.Pop( &pHandle ) ) )
    {
        m_LFQueueWaitingForPreconditions.Add( pHandle );
    }
}

DWORD ThreadTaskProc( void* lpThreadParameter )
{
    TaskScheduler::ThreadTaskData* pThreadData  = (TaskScheduler::ThreadTaskData*)lpThreadParameter;
    TaskScheduler* pTaskScheduler = pThreadData->m_pTaskScheduler;

    // Execute tasks. Prefer executing tasks in local task queue. If none there, steal from
    // other task processors. If no tasks to steal, go to global task queue
    while( !pTaskScheduler->ShouldTaskThreadsTerminate() )
    {
        TASKHANDLE* pTaskHandle = NULL;
        if( SUCCEEDED( pThreadData->m_LFPriorityQueue.RemoveFirst( &pTaskHandle ) ) )
        {
            Task* pTask = NULL;
            if( !pTaskScheduler->IsTaskComplete( pTaskHandle->m_info.m_index, &pTask ) )
            {
                // Call task handler synchronously
                DebugSpew( "ThreadTaskProc(threadID %d): Executing task index: %u; priority:%u\n", 
                            GetCurrentThreadId(),
                            pTaskHandle->m_info.m_index, 
                            pTaskHandle->m_info.m_priority );

                HRESULT hr = pTaskScheduler->ExecuteTaskAndRelease( pTaskHandle, pTask );
                if( !SUCCEEDED( hr ) )
                {
                    DebugSpew( "ThreadTaskProc(threadID %d): Task index: %u failed with hr=0x%x\n", 
                                GetCurrentThreadId(),
                                pTaskHandle->m_info.m_index, 
                                hr );

                    return ERROR_FUNCTION_FAILED; 
                }
            }
        }
        else if( pTaskScheduler->StealTask( &pTaskHandle ) )
        {
            // Steal work from another task processor

            pThreadData->m_LFPriorityQueue.Add( pTaskHandle->m_info.m_priority, pTaskHandle );

            DebugSpew( "ThreadTaskProc(threadID %d): Stole task index: %u; priority:%u\n", 
                        GetCurrentThreadId(),
                        pTaskHandle->m_info.m_index, 
                        pTaskHandle->m_info.m_priority );
        }
        else
        {
            // Get task from global task queue
            pTaskHandle = NULL;
            if( pTaskScheduler->GetHighestPriTask( &pTaskHandle ) )
            {
                pThreadData->m_LFPriorityQueue.Add( pTaskHandle->m_info.m_priority, pTaskHandle );

                DebugSpew( "ThreadTaskProc(threadID %d): Will handle task index: %u; priority:%u\n", 
                            GetCurrentThreadId(),
                            pTaskHandle->m_info.m_index, 
                            pTaskHandle->m_info.m_priority );
            }
        }
    }

    return ERROR_SUCCESS;
}