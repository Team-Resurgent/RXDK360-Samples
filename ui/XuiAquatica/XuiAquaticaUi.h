
//--------------------------------------------------------------------------------------
// Globals variables and definitions
//--------------------------------------------------------------------------------------

enum FISH_TYPE
{
    FISH_TYPE_SHARK,
    FISH_TYPE_TURTLE,
    FISH_TYPE_UD,
    FISH_STYLE_MAX
};

enum LIGHTING_INTENSITY
{
    LIGHTING_HIGH,
    LIGHTING_MEDIUM,
    LIGHTING_LOW,
    LIGHTING_MAX
};

// IAquaticaUI
// interface to communicate between the UI and the main application
struct IAquaticaUI
{
    virtual FISH_TYPE           GetFishType() = 0;
    virtual void                SetFishType( FISH_TYPE nFishType ) = 0;
    virtual DWORD               GetTextureIndex() = 0;
    virtual void                SetTextureIndex( DWORD dwTextureIndex ) = 0;
    virtual DWORD               GetFogColorIndex() = 0;
    virtual void                SetFogColorIndex( DWORD dwFogColorIndex ) = 0;
    virtual LIGHTING_INTENSITY  GetLightingIntensity() = 0;
    virtual void                SetLightingIntensity( LIGHTING_INTENSITY nLightingIntensity ) = 0;
    virtual double              GetFogDepth() = 0;
    virtual void                SetFogDepth( double fFogDepth ) = 0;
    virtual double              GetControllerSensitivity() = 0;
    virtual void                SetControllerSensitivity( double fControllerSensitivity ) = 0;
    virtual BOOL                GetControllerInversion() = 0;
    virtual void                SetControllerInversion( BOOL bControllerInversion ) = 0;
    virtual BOOL                GetMusicMutedState() = 0;
    virtual void                SetMusicMutedState( BOOL bMusicMuted ) = 0;
    virtual IDirect3DDevice9* GetD3DDevice() = 0;
    virtual void                RenderFishPreview( FISH_TYPE nFishType, DWORD dwTextureIndex,
                                                   IDirect3DDevice9* pDevice ) = 0;
    virtual HRESULT             RenderScene() = 0;
};

//--------------------------------------------------------------------------------------
// Declarations
//--------------------------------------------------------------------------------------
IAquaticaUI* GetApp();

HRESULT ShowMainMenu();
void HideMenu();
HRESULT RenderUI( IDirect3DDevice9* pDevice, UINT uWidth, UINT uHeight );
void DispatchXuiInput( XINPUT_KEYSTROKE* pKeystroke );
HRESULT InitUI( ATG::Application* pApp );
void UpdateUI();
BOOL IsUIActive();
