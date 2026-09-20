//--------------------------------------------------------------------------------------
// SceneViewer2.h
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef SCENEVIEWER2_H
#define SCENEVIEWER2_H

#include <xtl.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgSceneAll.h>
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>

#include <list>
#include <vector>
#include <algorithm>

#include "AnimationPlayback.h"
#include "FileDialog.h"
#include "ObjectTweak.h"
#include "ParameterPool.h"
#include "SettingsUI.h"
#include "Benchmark.h"

// Define a symbol that is used to compile out the use of the GPU performance counter APIs
// when using a release build of Direct3D.  The GPU performance counter APIs only work with
// d3d9i.lib and d3d9d.lib.
#if defined( NDEBUG ) && ( !defined( PROFILE ) || defined( FASTCAP ) )
#define _RELEASED3D
#endif

class Task
{
public:
            Task( const CHAR* strName = NULL )
            {
                m_hTaskBegin = CreateEvent( NULL, FALSE, FALSE, strName );
                m_hTaskComplete = CreateEvent( NULL, FALSE, FALSE, strName );
            }
            ~Task()
            {
                CloseHandle( m_hTaskBegin );
                CloseHandle( m_hTaskComplete );
            }

    // BeginTask() and WaitUntilComplete() are for the requesting thread to use.
    VOID    BeginTask()
    {
        SetEvent( m_hTaskBegin );
    }
    BOOL    WaitUntilComplete()
    {
        DWORD dwResult = WaitForSingleObject( m_hTaskComplete, INFINITE );
        return ( dwResult == WAIT_OBJECT_0 );
    }
    BOOL    IsTaskComplete() const
    {
        DWORD dwResult = WaitForSingleObject( m_hTaskComplete, 0 );
        return( dwResult == WAIT_OBJECT_0 );
    }

    // WaitForBegin() and TaskIsComplete() are for the worker thread.
    BOOL    WaitForBegin()
    {
        DWORD dwResult = WaitForSingleObject( m_hTaskBegin, INFINITE );
        return ( dwResult == WAIT_OBJECT_0 );
    }
    VOID    TaskCompleted()
    {
        SetEvent( m_hTaskComplete );
    }
protected:
    HANDLE m_hTaskBegin;
    HANDLE m_hTaskComplete;
};

template<class T> class CopyVector
{
public:
            CopyVector( DWORD dwInitialSize = 10 ) : m_dwElementSize( 0 ),
                                                     m_dwStorageSize( 0 ),
                                                     m_pElements( NULL )
            {
                ReserveSpace( dwInitialSize );
            }
            ~CopyVector()
            {
                Destroy();
            }
    VOID    ReserveSpace( DWORD dwSize )
    {
        assert( dwSize >= m_dwElementSize );
        if( dwSize == m_dwStorageSize )
            return;
        T* pNewElements = new T[ dwSize ];
        XMemCpy( pNewElements, m_pElements, m_dwElementSize * sizeof( T ) );
        delete[] m_pElements;
        m_pElements = pNewElements;
        m_dwStorageSize = dwSize;
    }
    DWORD   Size() const
    {
        return m_dwElementSize;
    }
    DWORD   Capacity() const
    {
        return m_dwStorageSize;
    }
    T& operator[]( DWORD dwIndex )
    {
        assert( dwIndex < m_dwElementSize ); return m_pElements[dwIndex];
    }
    const T& operator[]( DWORD dwIndex ) const
    {
        assert( dwIndex < m_dwElementSize ); return m_pElements[dwIndex];
    }
    VOID    SetAt( DWORD dwIndex, const T& Value )
    {
        assert( dwIndex < m_dwElementSize );
        XMemCpy( &m_pElements[ dwIndex ], &Value, sizeof( T ) );
    }
    VOID    PushBack( const T& Value )
    {
        if( m_dwElementSize == m_dwStorageSize )
        {
            DWORD dwNewSize = m_dwStorageSize + ( m_dwStorageSize >> 1 ) + 1;
            assert( dwNewSize > m_dwStorageSize );
            ReserveSpace( dwNewSize );
        }
        //XMemCpy( &m_pElements[ m_dwElementSize ], &Value, sizeof( T ) );
        m_pElements[ m_dwElementSize ] = Value;
        ++m_dwElementSize;
    }
    VOID    Clear()
    {
        m_dwElementSize = 0;
    }
    VOID    Destroy()
    {
        m_dwElementSize = 0;
        m_dwStorageSize = 0;
        delete[] m_pElements;
        m_pElements = NULL;
    }
    T& Front() const
    {
        assert( m_dwElementSize > 0 );
        return m_pElements[0];
    }
    T& Back() const
    {
        assert( m_dwElementSize > 0 );
        return m_pElements[ m_dwElementSize - 1 ];
    }
    T* FrontPtr() const
    {
        assert( m_dwElementSize > 0 );
        return m_pElements;
    }
    T* BackPtr() const
    {
        assert( m_dwElementSize > 0 );
        return &m_pElements[ m_dwElementSize - 1 ];
    }
protected:
    DWORD m_dwElementSize;
    DWORD m_dwStorageSize;
    T* m_pElements;
};

typedef std::vector <ATG::Camera>               CameraList;
typedef std::vector <ATG::Camera*>              CameraPtrList;
typedef std::vector <ATG::PointLight>           PointLightList;
typedef std::vector <ATG::PointLight*>          PointLightPtrList;
typedef std::vector <ATG::SpotLight>            SpotLightList;
typedef std::vector <ATG::SpotLight*>           SpotLightPtrList;
typedef std::vector <ATG::DirectionalLight>     DirectionalLightList;
typedef std::vector <ATG::DirectionalLight*>    DirectionalLightPtrList;
typedef std::vector <ATG::Model>                ModelList;
typedef std::vector <ATG::Model*>               ModelPtrList;
typedef std::vector <D3DTexture*>               TexturePtrList;
typedef std::vector <XMMATRIX>                  MatrixList;
typedef std::vector <ATG::Bound>                BoundList;

enum SceneViewerRenderMode
{
    SVRM_NORMAL = 0,
    SVRM_DEFERRED,
    SVRM_PASSPERLIGHT,
    SVRM_SHADERLIB
};

enum SceneViewerTilingMode
{
    SVTM_NONE = 0,
    SVTM_720p2X_V,
    SVTM_720p2X_H,
    SVTM_720p4X_3,
    SVTM_720p4X_4,
};

enum SceneViewerZPassMode
{
    SVZP_NONE   = 0,
    SVZP_D3D    = 1,
    SVZP_MANUAL = 2
};

enum SceneViewerPerfEvents
{
    SVPE_BEGINSCENE = 0,
    SVPE_END_SHADOWS,
    SVPE_END_ZPASS,
    SVPE_END_DEFERRED_CONSTRUCTION,
    SVPE_END_SCENERENDER,
    SVPE_END_POSTEFFECTS,
    SVPE_END_UIRENDER,
    SVPE_END_RESOLVE,
    SVPE_SIZEOF
};

enum SceneViewerUICommands
{
    SVUI_LOADSCENE = 1,
};

static const WCHAR*                             g_strPerfSections[] =
{
    L"Begin Scene",
    L"Shadow Maps",
    L"Z Pass",
    L"Deferred Construction",
    L"Scene Render/Deferred Lighting",
    L"Post Effects Render",
    L"UI Render",
    L"Resolve to Front Buffer"
};

typedef struct
{
    XMVECTOR v[3];
}                                               XMVECTOR3;

typedef struct
{
    XMVECTOR v[8];
}                                               XMVECTOR8;

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class SceneViewer : public ATG::Application
{
public:
    // Public members, which can be used and manipulated by other objects for 
    // benchmarking purposes.
    SceneViewerRenderMode m_RenderMode;
    SceneViewerTilingMode m_TilingMode;
    SceneViewerZPassMode m_ZPassMode;
    BOOL m_bDisableAllUI;
    //SettingsUI                  m_SettingsUI;
    SettingsPanel m_SettingsPanel;
    BOOL m_bStencilOptimization;
    DWORD m_dwSecondaryRingBufferSize;

protected:
    ATG::Font m_Font;
    ATG::Timer m_Timer;
    FLOAT m_fDeltaTime;
    FLOAT m_fAppTime;
    D3DRECT m_TitleSafeRect;
    ATG::PackedResource m_Resource;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    XMMATRIX m_matShadowTextureOffset;
    DWORD m_dwUpAxis;
    CHAR            m_strSceneFileName[MAX_PATH];
    SimpleAnimationPlayer m_AnimationPlayer;
    DWORD m_dwActiveCameraIndex;
    SampleParameterPool m_SampleParameterPool;
    BOOL m_bWireframe;
    DWORD m_dwDebugRenderMode;
    BOOL m_bDisplayLights;
    BOOL m_bDisplayBounds;
    BOOL m_bDisplayFrames;
    FLOAT m_fBoneRadius;
    BOOL m_bDisplayCameras;
    BOOL m_bEnableLighting;
    BOOL m_bEnableAnimations;
    BOOL m_bEnableShadowUpdates;
    BOOL m_bReduceShadowShimmer;
    BOOL m_bInvertRotationYAxis;
    BOOL m_bFrustumCulling;
    BOOL m_bRenderUpsideDown;
    BOOL m_bLockVisibleSet;
    BOOL m_bSortLightsByDistance;
    DWORD m_dwTriangleCullingMode;
    BOOL m_bDrawStats;
    BOOL m_bDrawMemoryStats;
    BOOL m_bDrawTilingStats;
    BOOL m_bDrawGroundPlane;
    DWORD m_dwPrimitivesRendered;
    DWORD m_dwSubsetsRendered;
    DWORD m_dwModelsRendered;
    DWORD m_dwTotalModels;
    DWORD m_dwModelsVisible;
    DWORD m_dwShadowMapsRendered;
    DWORD m_dwModelsRenderedToShadowMaps;
    DWORD m_dwActiveLights;
    DWORD m_dwLightInfluences;
    DWORD m_dwMaxPointLightsToSet;
    DWORD m_dwMaxSpotLightsToSet;
    DWORD m_dwMaxDirLightsToSet;
    FLOAT m_fAspectRatio;
    FLOAT m_fLightRangeScale;
    FLOAT m_fLightIntensityScale;
    FLOAT m_fAmbient;
    FLOAT m_fAnimationSpeed;
    FLOAT m_fCameraMoveSpeed;
    FLOAT m_fCameraZFar;
    FLOAT m_fDebugNormalsScale;
    ObjectTweaker m_ObjectTweaker;
    SceneViewerXuiApp m_XuiApp;
    CRITICAL_SECTION m_SceneCriticalSection;
    CRITICAL_SECTION m_Direct3DCriticalSection;
    DWORD m_dwAsyncLoadProgress;
    D3DVIEWPORT9 m_Viewport;
    const CHAR* m_strSceneParseErrorMsg;
    INT m_iIsolatedModelIndex;
    INT m_iRenderAnimationTrackIndex;
    D3DTexture* m_pDepthTexture;
    BOOL m_bShowDebugBuffers;
    INT m_iShowShadowMap;
    FLOAT m_fShadowSlopedDepthBias;
    D3DRECT         m_TilingRects[8];
    DWORD m_dwTilingRectCount;
    FLOAT m_fTextScalingFactor;
    HRESULT m_EndTilingResult;
    DWORD m_dwDefaultLightRigIndex;
    DWORD m_dwCameraControlType;
    BOOL m_bDrawSafeRect;
    BOOL m_bDrawTransparentObjects;
    BOOL m_bAdaptiveCameraSpeed;

    // scene state
    ATG::Scene* m_pScene;
    CameraPtrList m_SceneCameraList;
    ATG::Camera* m_pCurrentSceneCamera;

    // shadow scene state
    struct
    {
        XMMATRIX matWVP;
        XMMATRIX matVP;
        ATG::Camera Camera;

        CopyVector <ATG::PointLight> PointLights;
        CopyVector <ATG::SpotLight> SpotLights;
        CopyVector <D3DTexture*> SpotShadowMaps;
        CopyVector <ATG::DirectionalLight> DirLights;
        CopyVector <D3DTexture*> DirShadowMaps;

        ModelPtrList AllModels;
        MatrixList AllModelWorldTransforms;
        BoundList AllModelWorldBounds;

        ModelPtrList VisibleModels;
        MatrixList VisibleModelWorldTransforms;
        BoundList VisibleModelWorldBounds;

        ModelPtrList VisibleTransparentModels;
        MatrixList VisibleTransparentModelWorldTransforms;
        BoundList VisibleTransparentModelWorldBounds;

        ATG::Bound EntireSceneBounds;

        VOID Clear()
        {
            PointLights.Clear();
            SpotLights.Clear();
            SpotShadowMaps.Clear();
            DirLights.Clear();
            DirShadowMaps.Clear();

            AllModels.erase( AllModels.begin(), AllModels.end() );
            AllModelWorldTransforms.erase( AllModelWorldTransforms.begin(), AllModelWorldTransforms.end() );
            AllModelWorldBounds.erase( AllModelWorldBounds.begin(), AllModelWorldBounds.end() );

            VisibleModels.erase( VisibleModels.begin(), VisibleModels.end() );
            VisibleModelWorldTransforms.erase( VisibleModelWorldTransforms.begin(),
                                               VisibleModelWorldTransforms.end() );
            VisibleModelWorldBounds.erase( VisibleModelWorldBounds.begin(), VisibleModelWorldBounds.end() );

            VisibleTransparentModels.erase( VisibleTransparentModels.begin(), VisibleTransparentModels.end() );
            VisibleTransparentModelWorldTransforms.erase( VisibleTransparentModelWorldTransforms.begin(),
                                                          VisibleTransparentModelWorldTransforms.end() );
            VisibleTransparentModelWorldBounds.erase( VisibleTransparentModelWorldBounds.begin(),
                                                      VisibleTransparentModelWorldBounds.end() );
        }
    } m_ShadowSceneState;

    D3DSurface* m_pColorRenderTargets[4];
    D3DSurface* m_pDepthBuffer;
    D3DTexture* m_pFrontBuffer;
    D3DTexture* m_pSceneResolveBuffer;
    D3DSurface* m_pFullScreenTarget;

    D3DSurface* m_pShadowMapRenderTarget;
    DWORD m_dwShadowMapSize;
    BOOL m_bMipShadowMaps;
    TexturePtrList m_ShadowMapBank;
    DWORD m_dwShadowMapBankUsage;
    D3DVIEWPORT9 m_ShadowViewport;
    FLOAT m_fTightDirShadowRadius;

    D3DTexture* m_pDeferredColorBuffer;
    D3DTexture* m_pDeferredNormalBuffer;

    ATG::BaseMaterial* m_pUbershaderBaseMaterial;
    ATG::BaseMaterial* m_pLayeredBaseMaterial;

    DeferredParameterPool m_DeferredParameterPool;
    ATG::BaseMaterial* m_pDeferredBaseMaterial;

    PassPerLightParameterPool m_PassPerLightParameterPool;
    ATG::BaseMaterial* m_pPassPerLightBaseMaterial;

    ATG::BaseMaterial* m_pShaderLibraryBaseMaterial;
    FXLHANDLE m_hCurrentShaderLibTechnique;

    DWORD m_dwUbershaderTechniqueIndex;

    DWORD m_dwPostEffectTechniqueIndex;
    ATG::BaseMaterial* m_pPostEffects;
    PostEffectParameterPool m_PostEffectParameterPool;
    FLOAT m_fFocalDepth;
    FLOAT m_fFocalAperture;
    FLOAT m_fFocalSlope;
    FLOAT m_fMaxCircleOfConfusion;

    Benchmark m_BenchmarkModule;

#ifndef _RELEASED3D
    D3DPerfCounters* m_pPerfCounters[SVPE_SIZEOF * 2];
    BOOL            m_bPerfCounterUsed[SVPE_SIZEOF * 2];
#endif

    BOOL m_bDisplayPerfChart;
    BOOL m_bCapturePerfData;
    BOOL m_bIsolatePerfSections;
    DWORD m_dwTextureOverrideMode;

    HANDLE m_hUpdateThread;
    Task m_TaskUpdate;
    CRITICAL_SECTION m_RenderSettingsCriticalSection;

    DWORD m_dwRenderThreadFrameCount;

public:
    HRESULT         LoadSceneAsync( const CHAR* strSceneFileName );
    HRESULT         LoadScene( const CHAR* strSceneFileName );
    HRESULT         ReloadScene();
    VOID            UnloadScene();
    ATG::Scene* GetScene()
    {
        return m_pScene;
    }
    const CHAR* GetSceneFileName() const
    {
        return m_strSceneFileName;
    }

    VOID            MoveFrame( ATG::GAMEPAD* pGamepad, FLOAT fDeltaTime, ATG::Frame* pFrame );

    VOID            DebugRenderLight( ATG::Light* pLight );
    VOID            DebugRenderFrame( ATG::Frame* pFrame );
    VOID            DebugRenderCamera( ATG::Camera* pCamera );
    VOID            DebugRenderText3D( const XMVECTOR vWorldPos, const WCHAR* strText, D3DCOLOR Color, FLOAT fSize =
                                       1.0f );
    VOID            DebugRenderAnimationTrack( FLOAT fStartTime, FLOAT fDuration,
                                               ATG::AnimationTransformTrack* pTrack );
    VOID            DebugRenderBone( ATG::Frame* pFrame );

    HRESULT         UpdateGameState();

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update()
    {
        return S_OK;
    }
    virtual HRESULT Render();

    HRESULT         UpdateLogic();

    VOID            CreateSceneShadowCopy();

    VOID            UpdateRenderingSettings();

    VOID            SetupSceneRenderingTargets();
    VOID            PermuteTilingRectangles();
    VOID            SetupPostEffectTargets();
    VOID            ResolveFullScreenToFrontBuffer();

    VOID            ClearAndBeginTiling();
    VOID            EndTiling();

    VOID            UpdatePerfCounterUsage();
    VOID            RecordPerfEvent( DWORD dwEventIndex, DWORD dwFlags = 0 );
    BOOL            GetPerfValues( DWORD dwEventIndex, D3DPERFCOUNTER_VALUES& Values );

    VOID            RenderStagingScreen();
    VOID            RenderUI();

    VOID            SetupBuffers();
    VOID            RebindMaterialShaders( ATG::Scene* pScene );

    // frustum culling
    VOID            BuildVisibilityList( ATG::Camera* pCamera );

    // shadow buffer rendering
    VOID            BuildShadowMapBank();
    VOID            ResetShadowMapAllocator();
    D3DTexture* AllocateShadowMap( DWORD dwFlags );
    VOID            RenderAllSpotlightShadowMaps();
    VOID            RenderAllDirLightShadowMaps();

    // manual Z pass
    VOID            RenderManualZPass();

    // geometry construction
    VOID            BuildPointLightRectCorners( ATG::PointLight* pLight, XMVECTOR3& vCorners );
    VOID            BuildFrustumCorners( const ATG::Frustum& frustum, const XMMATRIX& matTransform,
                                         XMVECTOR8& vCorners );

    VOID            SetSamplerOverrides();

    // normal scene rendering
    HRESULT         RenderSceneNormal();
    VOID            RenderModel( ATG::Model* pModel, BOOL bRenderTransparent, const XMMATRIX matModelWorld,
                                 const ATG::Bound& ModelBound, ATG::Camera* pCamera );
    VOID            RenderMeshSubset( ATG::BaseMesh* pMesh, ATG::MaterialInstance* pMaterial, DWORD dwSubsetIndex );
    VOID            RenderShadowMap( ATG::SpotLight* pSourceLight, D3DTexture* pDestTexture, XMMATRIX& matLightVP );
    VOID            RenderShadowMap( ATG::DirectionalLight* pSourceLight, BOOL bTightShadow, D3DTexture* pDestTexture,
                                     XMMATRIX& matLightVP );
    VOID            CreateShadowMapMips( D3DTexture* pTexture );

    // pass per light rendering
    HRESULT         RenderScenePassPerLight();
    VOID            RenderPointLightQuadToStencil( ATG::PointLight* pLight, DWORD dwStencilValue );

    // deferred rendering
    HRESULT         RenderSceneDeferred();
    HRESULT         RenderLightsDeferred();

    // post effects
    VOID            RenderPostEffects();

    HRESULT         RenderDebugObjects();
    VOID            RenderStats();
    VOID            RenderPerfChart();
    VOID            RenderProgressBar( FLOAT fProgress, const D3DRECT& RectSafe );
    VOID            RenderTextureBox( DWORD dwXPos, DWORD dwYPos, DWORD dwHeight, D3DBaseTexture* pTexture,
                                      BOOL bDepthTexture );
    VOID            RenderMeshNormals( ATG::BaseMesh* pMesh, const XMMATRIX& matWVP );

    VOID            AcquireD3D();
    VOID            ReleaseD3D();
    VOID            AcquireScene();
    VOID            ReleaseScene();

    BOOL            IsSceneRenderable() const
    {
        return m_pScene != NULL && m_pCurrentSceneCamera != NULL;
    }

    VOID            SetupUI();

    VOID            InitializeSceneDefaultAssets( ATG::Scene* pScene );

    VOID            FindCameras( ATG::Scene* pScene );
    ATG::Camera* CreateDefaultCamera();
    VOID            MoveCamera( ATG::GAMEPAD* pGamepad, FLOAT fDeltaTime, ATG::Camera* pCamera );
    DWORD           SetupLighting( ATG::Model* pModel, const XMMATRIX matModelWorld, const XMMATRIX matModelInvWorld,
                                   const ATG::Bound& ModelBound );
    D3DTexture* CreateShadowMapTexture();
    VOID            UpdateDefaultLightRig();
};

extern SceneViewer*                             g_pSceneViewerApp;

#endif
