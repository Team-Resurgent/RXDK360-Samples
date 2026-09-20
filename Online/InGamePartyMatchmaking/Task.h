//--------------------------------------------------------------------------------------
// Task.h
//
// Task data structures used by TaskScheduler
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

enum TaskState
{
	TASK_STATE_NOT_SCHEDULED = 0,
	TASK_STATE_IN_PRECONDITION_QUEUE_WAITING,
	TASK_STATE_IN_PRIORITY_QUEUE_WAITING,
	TASK_STATE_IN_PRIORITY_QUEUE_EXECUTING,
	TASK_STATE_DONE
};

#define MIN_HANDLE_INDEX						( 1 )
#define MAX_HANDLE_INDEX						( (1 << 16) - 2 )
#define INVALID_HANDLE_INDEX					( (1 << 16) - 1 )

struct TASKHANDLE
{
	unsigned __int32 m_indexDependsOn;
	DWORD			 m_dwDependsOnCreationTick;
	unsigned __int32 m_indexGroup;
	DWORD			 m_dwPreferredThreadId;
	struct _info
	{
		unsigned __int32 m_index			:15;
		unsigned __int32 m_bIsTaskGroup		:1;
		unsigned __int32 m_area				:4;
		unsigned __int32 m_number			:6;
		unsigned __int32 m_priority			:3; // lower priority tasks executed first
		unsigned __int32 m_bReschedulable	:1; // value of 1 indicates task can be rescheduled
		unsigned __int32 m_bAsync	        :1; // value of 1 indicates task runs async (uses XOVERLAPPED)
		unsigned __int32 m_bRescheduled	    :1; // value of 1 indicates task has been rescheduled at least once
	} m_info;
};

typedef HRESULT (*PFNTASKHANDLER)(const unsigned __int16, const TASKHANDLE*, void*);

struct Task
{
	void*			m_pData;
	PFNTASKHANDLER	m_pfnTaskHandler;
	TaskState		m_state;
	DWORD			m_dwCreationTick;
	DWORD			m_dwExecutionStartTick;
	HRESULT			m_hr;

	Task()
	{
		m_pData = NULL;
	}

private:
	Task( const Task& ref );
	Task& operator=( const Task& ref );
};

struct TaskGroup
{
	DWORD							m_dwCreationTick;
	XLockFreeHashTable<TASKHANDLE>	m_LFTasks;

	TaskGroup()
	{
		m_LFTasks.Initialize(13);
	}

	~TaskGroup()
	{
		m_LFTasks.Destroy();
	}
};