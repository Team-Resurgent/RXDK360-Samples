#include "PatchConstraints.h"

VOID FilterJointConstraints::Initialize()
{
    m_pFilterDoubleExp = new ATG::FilterDoubleExponential();
    m_pJointConverter = new ATG::NuiJointConverter();
    m_pJointConverter->AddDefaultConstraints();
    ResetFilterState();
}

BOOL FilterJointConstraints::FilterSkeleton( SkeletonFilterContext* pContext, const NUI_SKELETON_DATA* pInputSkeleton, NUI_SKELETON_DATA* pOutputSkeleton, const FLOAT fDeltaTime )
{
    if ( pInputSkeleton->eTrackingState != NUI_SKELETON_TRACKED )
    {
        m_pFilterDoubleExp->Reset();
    }
    m_pFilterDoubleExp->Update( pInputSkeleton );
    XMemCpy( pOutputSkeleton->SkeletonPositions, m_pFilterDoubleExp->GetFilteredJoints(), sizeof( pOutputSkeleton->SkeletonPositions ) );
    m_pJointConverter->ConvertNuiJoints( pOutputSkeleton, TRUE );
    m_pJointConverter->GetNuiSkeletonData( pOutputSkeleton );
    return TRUE;
}

VOID FilterJointConstraints::ResetFilterState()
{
    m_pFilterDoubleExp->Init( 0.55f, 0.6f, 0.75f, 0.05f, 0.05f );
}

VOID FilterJointConstraints::DebugRenderUI( IFilterDebugRenderer* pDebugRenderer )
{

}
