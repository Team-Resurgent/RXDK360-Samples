//--------------------------------------------------------------------------------------
// AppXuiModule.h
//
// Application Xui Module class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_APPXUIMODULE_H
#define ARCADESAMPLE_APPXUIMODULE_H


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Application Xui Module Class
//--------------------------------------------------------------------------------------
class AppXuiModule : public CXuiModule
{
public:
    HRESULT         Render( const D3DPRESENT_PARAMETERS& d3dpp, IDirect3DDevice9* d3dDevice );

protected:
    // Register and unregister Xui classes
    virtual HRESULT RegisterXuiClasses();
    virtual HRESULT UnregisterXuiClasses();
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_APPXUIMODULE_H
