//--------------------------------------------------------------------------------------
// XuiEffect.cpp
//
// Shows how to implement a scene that renders its children to a texture and then
// renders the texture using a custom pixel shader that implements a simple 2d effect.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>
#include <AtgMediaLocator.h>
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Name: class CEffectScene
// Desc: Effect Scene implementation class.  This scene class renders its children
//       to a texture and then renders the texture using a custom pixel shader.
//--------------------------------------------------------------------------------------
class CEffectScene : public CXuiSceneImpl
{
    // simple vertex type used when rendering our texture
    struct VertexType
    {
        FLOAT x;
        FLOAT y;
        FLOAT u;
        FLOAT v;
    };

    DWORD m_dwTexWidth;       // width of texture and render target
    DWORD m_dwTexHeight;      // height of texture and render target
    IDirect3DTexture9* m_pTexTarget;       // texture resolved from render target
    IDirect3DTexture9* m_pTexDisplacement; // our displacement texture
    IDirect3DSurface9* m_pRenderTarget;    // used for render children to texture

    IDirect3DVertexShader9* m_pVertexShader;
    IDirect3DPixelShader9* m_pPixelShader;
    IDirect3DVertexDeclaration9* m_pVertexDecl;

    // effect parameters and state
    FLOAT m_fDisplacementFactor;
    FLOAT m_fDisplacement;

    // Message map. Here we tie messages to message handlers.
    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_RENDER( OnRender )
    XUI_END_MSG_MAP()

    HRESULT InitSurfacesAndShaders( IDirect3DDevice9* pDevice );
    VOID    RenderChildrenToTexture( XUIMessageRender* pData );
    VOID    RenderObjectTexture( IDirect3DDevice9* pDevice );
    HRESULT OnRender( XUIMessageRender* pRenderData, BOOL& bHandled );

public:

    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
    XUI_IMPLEMENT_CLASS( CEffectScene, L"EffectScene", XUI_CLASS_SCENE )

            CEffectScene() : m_dwTexWidth( 256 ),
                             m_dwTexHeight( 256 ),
                             m_pTexTarget( NULL ),
                             m_pTexDisplacement( NULL ),
                             m_pRenderTarget( NULL ),
                             m_pVertexShader( NULL ),
                             m_pPixelShader( NULL ),
                             m_pVertexDecl( NULL ),
                             m_fDisplacementFactor( 0.0f ),
                             m_fDisplacement( 0.0f )
            {
            }

            ~CEffectScene()
            {
                if( m_pTexTarget )
                    m_pTexTarget->Release();
                if( m_pTexDisplacement )
                    m_pTexDisplacement->Release();
                if( m_pRenderTarget )
                    m_pRenderTarget->Release();
                if( m_pVertexShader )
                    m_pVertexShader->Release();
                if( m_pPixelShader )
                    m_pPixelShader->Release();
                if( m_pVertexDecl )
                    m_pVertexDecl->Release();
            }

    // public method to set the displacement factor.  This affects how much 
    // displacement is done in the pixel shader
    VOID    SetDisplacementFactor( FLOAT fFactor )
    {
        m_fDisplacementFactor = fFactor;
    }
};

//--------------------------------------------------------------------------------------
// Name: class CMyMainScene
// Desc: Scene implementation class.
//--------------------------------------------------------------------------------------
class CMyMainScene : public CXuiSceneImpl
{
    // Control and Element wrapper objects.
    CXuiSlider m_Slider;           // slider that controls the effect amount
    CXuiScene m_EffectScene;      // the effect scene we're controlling

    // Message map. Here we tie messages to message handlers.
    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_VALUE_CHANGED( OnNotifyValueChanged )
    XUI_END_MSG_MAP()


    //----------------------------------------------------------------------------------
    // Performs initialization tasks - retrieves controls.
    //----------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        // Retrieve controls for later use.
        GetChildById( L"XuiSlider1", &m_Slider );
        GetChildById( L"EffectScene", &m_EffectScene );

        // set the slider to the default value specified by the XUI file
        INT nValue;
        m_Slider.GetValue( &nValue );
        SetEffectValue( nValue );
        return S_OK;
    }

    //----------------------------------------------------------------------------------
    // Sets the effect value on the CEffectScene instance.
    //----------------------------------------------------------------------------------
    VOID    SetEffectValue( INT nValue )
    {
        CEffectScene* pEffectScene;

        // Since we implement CEffectScene in this sample, we can just retrieve
        // a CEffectScene instance pointer from the handle contained in m_EffectScene
        XuiObjectFromHandle( m_EffectScene, ( VOID** )&pEffectScene );

        assert( pEffectScene != NULL );

        pEffectScene->SetDisplacementFactor( nValue / 50.0f );
    }

    //----------------------------------------------------------------------------------
    // Handler for the XN_VALUE_CHANGED XUI notification.  When the value of the
    // effect amount slider changes, we set the new value on the contained scene
    //----------------------------------------------------------------------------------
    HRESULT OnNotifyValueChanged( HXUIOBJ hObjSource,
                                  XUINotifyValueChanged* pNotifyValueChangedData,
                                  BOOL& bHandled )
    {
        if( hObjSource == m_Slider )
        {
            SetEffectValue( pNotifyValueChangedData->nValue );
        }
        return S_OK;
    }


public:

    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
    XUI_IMPLEMENT_CLASS( CMyMainScene, L"MyMainScene", XUI_CLASS_SCENE )
};


//--------------------------------------------------------------------------------------
// Main XUI host class. It is responsible for registering scene classes and provide
// basic initialization, scene loading and rendering capability.
//--------------------------------------------------------------------------------------
class CMyApp : public CXuiModule
{
protected:
    // Override RegisterXuiClasses so that CMyApp can register classes.
    virtual HRESULT RegisterXuiClasses();

    // Override UnregisterXuiClasses so that CMyApp can unregister classes. 
    virtual HRESULT UnregisterXuiClasses();
};


//--------------------------------------------------------------------------------------
// Name: RegisterXuiClasses()
// Desc: Registers all the scene classes.
//--------------------------------------------------------------------------------------
HRESULT CMyApp::RegisterXuiClasses()
{
    // Register any other classes necessary for the app/scene
    HRESULT hr = CMyMainScene::Register();
    if( FAILED( hr ) )
        return hr;

    hr = CEffectScene::Register();
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UnregisterXuiClasses()
// Desc: Unregisters all the scene classes.
//--------------------------------------------------------------------------------------
HRESULT CMyApp::UnregisterXuiClasses()
{
    CEffectScene::Unregister();

    CMyMainScene::Unregister();
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Application entry point.
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    // Declare helper necessary to locate resources inside an xzp archive.
    ATG::MediaLocator mediaLocator( L"file://game:/media/xuieffect.xzp" );
    WCHAR szResourceLocator[ ATG::LOCATOR_SIZE ];

    // Declare an instance of the XUI framework application.
    CMyApp app;

    // Initialize the application.    
    HRESULT hr = app.Init( XuiD3DXTextureLoader );
    if( FAILED( hr ) )
        ATG::FatalError( "Failed intializing application.\n" );

    // Register a default typeface
    hr = app.RegisterDefaultTypeface( L"Arial Unicode MS", L"file://game:/media/xarialuni.ttf" );
    if( FAILED( hr ) )
        ATG::FatalError( "Failed to register default typeface.\n" );

    // Load the skin file used for the scene.
    mediaLocator.ComposeResourceLocator( szResourceLocator, ARRAYSIZE( szResourceLocator ), L"xui/", L"simple_scene_skin.xur" ); 
    hr = app.LoadSkin( szResourceLocator );
    if( FAILED( hr ) )
        ATG::FatalError( "Failed to load skin.\n" );

    // Load the scene.

    mediaLocator.ComposeResourceLocator( szResourceLocator, ARRAYSIZE( szResourceLocator ), L"xui/", NULL ); 
    hr = app.LoadFirstScene( szResourceLocator, L"xuieffect_main.xur", NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Failed to load scene.\n" );

    // Run the scene using the built-in loop.
    app.Run();

    // Free resources, unregister custom classes, and exit.
    app.Uninit();
}




//--------------------------------------------------------------------------------------
// Name: InitSurfacesAndShaders()
// Desc: Creates our render target and texture, loads the displacement texture and
//       creates our shaders
//--------------------------------------------------------------------------------------
HRESULT CEffectScene::InitSurfacesAndShaders( IDirect3DDevice9* pDevice )
{
    HRESULT hr;

    // create our render target and texture for rendering our children.
    hr = pDevice->CreateTexture( m_dwTexWidth, m_dwTexHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_pTexTarget,
                                 NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't create texture" );

    hr = pDevice->CreateRenderTarget( m_dwTexWidth, m_dwTexHeight, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0,
                                      0, &m_pRenderTarget, NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't create render target" );

    // load our displacement texture
    hr = D3DXCreateTextureFromFile( pDevice, "game:\\media\\XuiEffect_Texture.png", &m_pTexDisplacement );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load displacement texture" );

    // load our vertex and pixel shaders
    VOID* pCode = NULL;
    hr = ATG::LoadFile( "game:\\Media\\Shaders\\ShadeXuiEffectVertex.xvu", &pCode );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load ShadeXuiEffectVertex.xvu" );

    hr = pDevice->CreateVertexShader( ( DWORD* )pCode, &m_pVertexShader );
    if( FAILED( hr ) )
        ATG::FatalError( "Failed creating vertex shader" );

    ATG::UnloadFile( pCode );

    hr = ATG::LoadFile( "game:\\Media\\Shaders\\ShadeXuiEffectPixel.xpu", &pCode );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load ShadeXuiEffectPixel.xpu" );

    hr = pDevice->CreatePixelShader( ( DWORD* )pCode, &m_pPixelShader );
    if( FAILED( hr ) )
        ATG::FatalError( "Failed creating pixel shader" );

    ATG::UnloadFile( pCode );

    // create our vertex declaration.  This matches the CEffectScene::VertexType structure
    static const D3DVERTEXELEMENT9 vertexDecl [] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0},
        { 0,  8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,   D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };

    hr = pDevice->CreateVertexDeclaration( vertexDecl, &m_pVertexDecl );
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderChildrenToTexture()
// Desc: Renders our scene's children into the current render target.  Assumes that the
//       render target has already been set to m_pRenderTarget
//--------------------------------------------------------------------------------------
VOID CEffectScene::RenderChildrenToTexture( XUIMessageRender* pData )
{
    // save the color factor and blend mode. We always want to render the children
    // without alpha and using normal blending
    DWORD dwOrigFactor = pData->dwColorFactor;
    XUI_BLEND_MODE nOrigBlendMode = pData->nBlendMode;

    // set the new factor and blending mode
    pData->dwColorFactor = D3DCOLOR_ARGB( 255, 255, 255, 255 );

    pData->nBlendMode = XUI_BLEND_NORMAL;

    // save the view matrix so we can restore it after rendering to our texture
    D3DXMATRIX matOrigView;
    XuiRenderGetViewTransform( pData->hDC, &matOrigView );


    // compute the inverse of our transformation matrix so we can render the
    // children to the top-left of our render target
    D3DXMATRIX matView;
    XuiElementGetFullXForm( m_hObj, &matView );

    D3DXMatrixInverse( &matView, NULL, &matView );

    // set the view transform to the inverse of our full-screen transform matrix
    XuiRenderSetViewTransform( pData->hDC, &matView );

    // now let the base class implementation actually render the children
    CXuiElement::RenderChildren( pData );

    // restore the view matrix
    XuiRenderSetViewTransform( pData->hDC, &matOrigView );

    // restore the color factor and blend mode
    pData->dwColorFactor = dwOrigFactor;
    pData->nBlendMode = nOrigBlendMode;
    XuiSetBlendMode( pData->hDC, nOrigBlendMode );
}

//--------------------------------------------------------------------------------------
// Name: RenderObjectTexture()
// Desc: Renders our scene's texture using our simple 2d effect shaders
//--------------------------------------------------------------------------------------
VOID CEffectScene::RenderObjectTexture( IDirect3DDevice9* pDevice )
{
    // disable the viewport transformation.  Our vertex shader assumes the output
    // is in screen space
    pDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );

    // select our vertex shader into the device and set the appropriate shader constants
    pDevice->SetVertexShader( m_pVertexShader );

    D3DXMATRIX matWorld;
    XuiElementGetFullXForm( m_hObj, &matWorld );

    D3DXMatrixTranspose( &matWorld, &matWorld );
    pDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWorld, 4 );


    // select our pixel shader into the device and set the appropriate shader constants
    pDevice->SetPixelShader( m_pPixelShader );

    D3DXVECTOR4 vColorFactor( 1, 1, 1, 1 );
    pDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&vColorFactor, 1 );

    D3DXVECTOR4 vDisplacementFactor( m_fDisplacementFactor, m_fDisplacementFactor, 0, 0 );
    pDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&vDisplacementFactor, 1 );

    D3DXVECTOR4 vDisplacement( m_fDisplacement, m_fDisplacement, 0, 0 );
    pDevice->SetPixelShaderConstantF( 2, ( FLOAT* )&vDisplacement, 1 );

    // update our displacement amount and wrap around on the 1.0f boundary
    m_fDisplacement += .001f;
    if( m_fDisplacement > 1 )
        m_fDisplacement = 1 - m_fDisplacement;

    // setup the sampler states and select our textures into the device
    pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

    pDevice->SetTexture( 0, m_pTexTarget );
    pDevice->SetTexture( 1, m_pTexDisplacement );

    // get the dimensions of our scene.  We use this as the size of the quad to
    // render
    FLOAT fWidth, fHeight;
    XuiElementGetBounds( m_hObj, &fWidth, &fHeight );

    VertexType points[] =
    {
        { 0, 0, 0, 0 },
        { fWidth, 0, 1, 0 },
        { fWidth, fHeight, 1, 1 },
        { 0, fHeight, 0, 1 },
    };

    // render a quad with the dimensions of our scene
    pDevice->SetVertexDeclaration( m_pVertexDecl );
    pDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, 2, points, sizeof( points[0] ) );
}

//--------------------------------------------------------------------------------------
// Name: OnRender()
// Desc: Handler for the XM_RENDER message
//--------------------------------------------------------------------------------------
HRESULT CEffectScene::OnRender( XUIMessageRender* pRenderData, BOOL& bHandled )
{
    // mark the message as handled since we fully render this object and its children
    bHandled = TRUE;

    // retrieve the D3D device.  For the sample we just retrieve this from 
    // pRenderData->hDC.  In a real title, this will be the same as the title owned D3D
    // device.
    IDirect3DDevice9* pDevice;
    XuiRenderGetDevice( pRenderData->hDC, &pDevice );
    if( !pDevice )
        return S_OK;

    // if we haven't initialized our render target, etc do so now
    if( !m_pTexTarget )
    {
        HRESULT hr = InitSurfacesAndShaders( pDevice );
        if( FAILED( hr ) )
        {
            pDevice->Release();
            return hr;
        }
    }

    assert( m_pTexTarget != NULL && m_pRenderTarget != NULL );

    // begin the XUI rendering process for this scene.  This sets up the internal XUI
    // rendering state
    XUIRenderStruct rs;
    BeginRender( pRenderData, &rs );

    // switch to our render target before rendering the children
    IDirect3DSurface9* pOrigRenderTarget = NULL;
    pDevice->GetRenderTarget( 0, &pOrigRenderTarget );
    pDevice->SetRenderTarget( 0, m_pRenderTarget );

    pDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB( 0, 255, 255, 255 ), 1, 0 );

    D3DXMATRIX mat;
    D3DXMatrixIdentity( &mat );

    // The BeginRender call causes the current transform to be cached internal to
    // XUI.  We call XuiRenderRestoreState to ensure that the new identity world
    // transform is applied.
    XuiRenderSetTransform( pRenderData->hDC, &mat );
    XuiRenderRestoreState( pRenderData->hDC );

    // now actually render the children to our render target
    RenderChildrenToTexture( pRenderData );

    // resolve the contents of our render target to our texture and restore the
    // render target on the device to the original state
    pDevice->Resolve( 0, NULL, m_pTexTarget, NULL, 0, 0, NULL, 0, 0, NULL );
    pDevice->SetRenderTarget( 0, pOrigRenderTarget );
    pOrigRenderTarget->Release();

    // finally render our texture to the device
    RenderObjectTexture( pDevice );

    // complete the rendering process and call XuiRenderRestoreState to notify XUI
    // that we have changed the state of the device render states etc during the 
    // rendering process
    EndRender( pRenderData, &rs );
    XuiRenderRestoreState( pRenderData->hDC );

    pDevice->Release();
    return S_OK;
}
