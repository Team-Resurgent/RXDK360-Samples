//--------------------------------------------------------------------------------------
// XuiArena.h
//
// XuiArena class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUIARENA_H
#define ARCADESAMPLE_XUIARENA_H

#include "AppXuiRectControl.h"

namespace ArcadeSample
{
class XuiBall;

//--------------------------------------------------------------------------------------
// Arena Class
//--------------------------------------------------------------------------------------
class XuiArena : public AppXuiRectControl
{
public:
                    XuiArena()
                    {
                    }
    virtual         ~XuiArena()
    {
    }

    eArenaCollide   CollideAndUpdate( XuiBall& ball );
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUIARENA_H
