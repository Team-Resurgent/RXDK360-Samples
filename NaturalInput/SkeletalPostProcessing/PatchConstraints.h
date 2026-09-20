#pragma once

#include "PatchCommon.h"
#include "AtgNuiJointFilter.h"
#include "AtgNuiJointConverter.h"

class FilterJointConstraints : public ISkeletonFilter
{
protected:
    ATG::FilterDoubleExponential* m_pFilterDoubleExp;
    ATG::NuiJointConverter* m_pJointConverter;

public:
    virtual VOID Initialize();

    virtual const WCHAR* GetName() const { return L"Joint Constraints"; }

    virtual BOOL FilterSkeleton( SkeletonFilterContext* pContext, const NUI_SKELETON_DATA* pInputSkeleton, NUI_SKELETON_DATA* pOutputSkeleton, const FLOAT fDeltaTime );
    virtual VOID ResetFilterState();

    virtual VOID DebugRenderUI( IFilterDebugRenderer* pDebugRenderer );
};
