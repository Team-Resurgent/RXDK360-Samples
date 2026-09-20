#pragma once
//--------------------------------------------------------------------------------------
// GpuTimer.h
//
// A timer that inserts callbacks into the command buffer in order to time how long 
// operations take on the GPU.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


// GPU callback.
static VOID GpuTimerCallBack( DWORD dwContext );


//--------------------------------------------------------------------------------------
// Name: GpuTimer
//
// Usage:
//
// globals:
//
//	GpuTimer* GpuTimer::m_pListHead = 0;
//	GpuTimer ClearTimer("Clear");
//	GpuTimer SeafloorTimer("Seafloor");
//	GpuTimer DolphinTimer("Dolphin");
//
// when rendering:
//
//		ClearTimer.StartTiming( m_pd3dDevice );
//      m_pd3dDevice->Clear( ... );
//		ClearTimer.StopTiming( m_pd3dDevice );
//
//		GpuTimer::DumpTimers( m_pd3dDevice );
// 
//--------------------------------------------------------------------------------------
class GpuTimer
{
public:
	GpuTimer( const CHAR* strName )
	{
		m_strName = strName;
		m_bTimingStarted = false;

		// Add the timer to the end of the list.
		GpuTimer** ppCurrentNodeNext = &m_pListHead;
		
		while (*ppCurrentNodeNext != 0)
			ppCurrentNodeNext = &(*ppCurrentNodeNext)->m_pNext;
	
		*ppCurrentNodeNext = this;

		this->m_pNext = 0;
	}
	
	~GpuTimer()
	{
		// Remove the timer from the list.
		GpuTimer** ppCurrentNodeNext = &m_pListHead;
		
		while (*ppCurrentNodeNext != this)
			ppCurrentNodeNext = &(*ppCurrentNodeNext)->m_pNext;
	
		*ppCurrentNodeNext = this->m_pNext;
	}
	
	// Start timing.
	VOID StartTiming( IDirect3DDevice9* pDevice )
	{
		// Add a callback to save the start time.
		m_StartTime.QuadPart = 0;
		m_bTimingStarted = true;
		pDevice->InsertCallback( D3DCALLBACK_IDLE, GpuTimerCallBack, (DWORD)&m_StartTime );
	}

	// Stop timing.	
	VOID StopTiming( IDirect3DDevice9* pDevice )
	{
		// We can't stop unless we started.
		if( !m_bTimingStarted)
			return;
		
		m_bTimingStarted = false;
		
		// Add a callback to save the end time.
		m_EndTime.QuadPart = 0;
		pDevice->InsertCallback( D3DCALLBACK_IDLE, GpuTimerCallBack, (DWORD)&m_EndTime );
	}
	
	// Print the value of the timer.
	VOID PrintTime( IDirect3DDevice9* pDevice )
	{
		// We can't compute the elapsed time unless StopTiming was called.
		if( m_bTimingStarted )
		{
			printf( "StopTiming not yet called\n" );
			return;
		}
		
		LARGE_INTEGER PerfFrequency;
		QueryPerformanceFrequency( &PerfFrequency );
		
		// Wait for GPU idle if the callback hasn't completed.
		if(  m_EndTime.QuadPart == 0 )
			pDevice->BlockUntilIdle();
		
		// Compute the elapsed time.
		LARGE_INTEGER ElapsedTime;
		ElapsedTime.QuadPart = m_EndTime.QuadPart - m_StartTime.QuadPart;

		double ElapsedSeconds = ElapsedTime.QuadPart / double(PerfFrequency.QuadPart);
		
		printf( "%s: %.02f ms\n", m_strName, ElapsedSeconds * 1000.0 );
	}

    	// Print the value of the timer.
	FLOAT GetTime( IDirect3DDevice9* pDevice )
	{
		// We can't compute the elapsed time unless StopTiming was called.
		if( m_bTimingStarted)
		{
			return 0.f;
		}
		
		LARGE_INTEGER PerfFrequency;
		QueryPerformanceFrequency( &PerfFrequency );
		
		// Wait for GPU idle if the callback hasn't completed.
		if(  m_EndTime.QuadPart == 0 )
			pDevice->BlockUntilIdle();
		
		// Compute the elapsed time.
		LARGE_INTEGER ElapsedTime;
		ElapsedTime.QuadPart = m_EndTime.QuadPart - m_StartTime.QuadPart;

		double ElapsedSeconds = ElapsedTime.QuadPart / double(PerfFrequency.QuadPart);

        return (FLOAT)ElapsedSeconds * 1000.0f;
	}

	// Dump the values from all GPU timers.
	static VOID DumpTimers( IDirect3DDevice9* pDevice )
	{
		// Print the values of all the timers.
		GpuTimer* pTimer = m_pListHead;
		while (pTimer != 0)
		{
			pTimer->PrintTime( pDevice );
			
			pTimer = pTimer->m_pNext;
		}
	}

	const CHAR* m_strName;
	
	BOOL m_bTimingStarted;
	
	LARGE_INTEGER m_StartTime;
	LARGE_INTEGER m_EndTime;

	GpuTimer* m_pNext;
	
	// Head pointer of the singly linked list of timers.
	static GpuTimer* m_pListHead;
};


//--------------------------------------------------------------------------------------
// Name: GpuTimerCallBack
//--------------------------------------------------------------------------------------
static VOID GpuTimerCallBack( DWORD dwContext )
{
	LARGE_INTEGER* pTime = (LARGE_INTEGER*)dwContext;
	
	// Is this safe to call at DPC time?
	QueryPerformanceCounter( pTime );
}


//--------------------------------------------------------------------------------------
// Name: GpuTimeBlock
//
// Usage:
//
//		{
//			GpuTimeBlock( m_pd3dDevice, &SeafloorTimer );
//			<rendering calls>
//		}
//
//--------------------------------------------------------------------------------------
class GpuTimeBlock
{
public:
	GpuTimeBlock( IDirect3DDevice9* pDevice, GpuTimer* pTimer )
	{
		m_pDevice = pDevice;
		m_pTimer = pTimer;
		
		pTimer->StartTiming( pDevice );
	}
	
	~GpuTimeBlock()
	{
		m_pTimer->StopTiming( m_pDevice );
	}
	
	IDirect3DDevice9* m_pDevice;
	GpuTimer* m_pTimer;
};
