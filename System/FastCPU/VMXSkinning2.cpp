//--------------------------------------------------------------------------------------
// File: VMXSkinning2.cpp
//
// Desc: Contains optimized VMX implementations of matrix palette skinning.
//       This code contains optimized code for writing XMFLOAT3s to memory.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "skinning.h"
#include "ppcintrinsics.h"
#include "xboxmath.h"

// Set up some defines and then include the header file that contains the shared
// VMX skinning implementation.
#define VMX_SKINNING_VERSION 2
#define SkinVMXShared SkinVMX2
#include "VMXSharedSkinning.h" // Shared implementation.
