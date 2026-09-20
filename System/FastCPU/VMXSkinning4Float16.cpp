//--------------------------------------------------------------------------------------
// File: VMXSkinning4.cpp
//
// Desc: Contains optimized VMX implementations of matrix palette skinning.
//       This code uses HALF (FLOAT16) data for vertex input and output.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "skinning.h"
#include "ppcintrinsics.h"
#include "xboxmath.h"


// Set up some defines and then include the header file that contains the shared
// VMX skinning implementation.
#define VMX_SKINNING_VERSION 4
#define SkinVMXShared SkinFloat16
#include "VMXSharedSkinning.h" // Shared implementation.
