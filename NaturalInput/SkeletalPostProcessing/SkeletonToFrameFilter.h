#pragma once
#include <assert.h>
#include "PatchCommon.h"

template< class SF = ISkeletonFilter >
class SkeletonToFrameFilter : public IFrameFilter
{
protected:
    SF* m_pSkeletonFilters[NUI_SKELETON_MAX_TRACKED_COUNT];
    SF* m_pSkeletonFilterAssignment[NUI_SKELETON_COUNT];

public:
    SkeletonToFrameFilter()
    {
        for( DWORD i = 0; i < ARRAYSIZE(m_pSkeletonFilters); ++i )
        {
            m_pSkeletonFilters[i] = new SF();
        }
    }

    ~SkeletonToFrameFilter()
    {
        for( DWORD i = 0; i < ARRAYSIZE(m_pSkeletonFilters); ++i )
        {
            delete m_pSkeletonFilters[i];
            m_pSkeletonFilters[i] = NULL;
        }
    }

    virtual VOID Initialize()
    {
        for( DWORD i = 0; i < ARRAYSIZE(m_pSkeletonFilters); ++i )
        {
            m_pSkeletonFilters[i]->Initialize();
        }
    }

    virtual VOID Terminate()
    {
        for( DWORD i = 0; i < ARRAYSIZE(m_pSkeletonFilters); ++i )
        {
            m_pSkeletonFilters[i]->Terminate();
        }
    }

    virtual const WCHAR* GetName() const { return m_pSkeletonFilters[0]->GetName(); }

    virtual VOID ResetFilterState()
    {
        for( DWORD i = 0; i < ARRAYSIZE(m_pSkeletonFilters); ++i )
        {
            m_pSkeletonFilters[i]->ResetFilterState();
        }
    }

    virtual BOOL FilterFrame( FrameFilterContext* pContext, const NUI_SKELETON_FRAME* pInputFrame, NUI_SKELETON_FRAME* pOutputFrame )
    {
        BOOL bFrameChanged = FALSE;
        DWORD CurrentSkeletonFilterIndex = 0;
        for( DWORD i = 0; i < ARRAYSIZE(pInputFrame->SkeletonData); ++i )
        {
            BOOL SkeletonTracked = ( pInputFrame->SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED );
            if( SkeletonTracked )
            {
                assert( CurrentSkeletonFilterIndex < ARRAYSIZE(m_pSkeletonFilters) );
                SF* pCurrentSkeletonFilter = m_pSkeletonFilters[CurrentSkeletonFilterIndex++];
                if( pCurrentSkeletonFilter != m_pSkeletonFilterAssignment[i] )
                {
                    pCurrentSkeletonFilter->ResetFilterState();
                }
                m_pSkeletonFilterAssignment[i] = pCurrentSkeletonFilter;

                SkeletonFilterContext SFContext;
                SFContext.pDepthProcess = pContext->pDepthProcess;
                SFContext.pProcess = pContext->pProcess[i];
                SFContext.pSkeletonFrame = pInputFrame;

                bFrameChanged |= pCurrentSkeletonFilter->FilterSkeleton( &SFContext, &pInputFrame->SkeletonData[i], &pOutputFrame->SkeletonData[i], pContext->fDeltaTime );
            }
            else
            {
                m_pSkeletonFilterAssignment[i] = NULL;
            }
        }
        return bFrameChanged;
    }

    virtual VOID DebugRenderUI( IFilterDebugRenderer* pDebugRenderer )
    {
        for( DWORD i = 0; i < ARRAYSIZE(m_pSkeletonFilters); ++i )
        {
            m_pSkeletonFilters[i]->DebugRenderUI( pDebugRenderer );
        }
    }
};

