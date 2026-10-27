//--------------------------------------------------------------------------------------
// RxdkXuiModule.h
//
// XUI equivalent of the ATG Application frame hook. CXuiModule::Run() (and the XUI
// samples' manual loops) call the virtual RunFrame() every frame, so a thin base that
// logs a sweep frame there gives XUI samples the same [RXDK] markers without touching
// the XUI runtime. XUI samples that already override RunFrame() call ATG::RxdkSweepFrame()
// directly instead of deriving this.
//--------------------------------------------------------------------------------------
#pragma once

#include <xuiapp.h>
#include "RxdkSweep.h"

class CRxdkXuiModule : public CXuiModule
{
public:
    virtual void RunFrame() { CXuiModule::RunFrame(); ATG::RxdkSweepFrame(); }
};
