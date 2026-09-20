
//--------------------------------------------------------------------------------------
// Globals variables and definitions
//--------------------------------------------------------------------------------------

// Render options
const DWORD DOLPHIN_RENDER_FLOOR = 1;
const DWORD DOLPHIN_RENDER_DOLPHIN = 2;
const DWORD DOLPHIN_RENDER_STATS = 4;
const DWORD DOLPHIN_RENDER_WIREFRAME = 8;

enum DOLPHIN_STYLE
{
    DOLPHIN_STYLE_STANDARD,
    DOLPHIN_STYLE_TIGER,
    DOLPHIN_STYLE_SPOTTED,
    DOLPHIN_STYLE_BLUE,
    DOLPHIN_STYLE_MAX
};

// IDolphinUI
// interface to communicate between the UI and the main application
struct IDolphinUI
{
    virtual DWORD           GetRenderOptions() = 0;
    virtual void            SetRenderOptions( DWORD dwRenderOptions ) = 0;
    virtual DOLPHIN_STYLE   GetDolphinStyle() = 0;
    virtual void            SetDolphinStyle( DOLPHIN_STYLE nDolphinStyle ) = 0;
    virtual LPCWSTR         GetDolphinStyleDesc( DOLPHIN_STYLE nDolphinStyle ) = 0;
    virtual IDirect3DDevice9* GetD3DDevice() = 0;
    virtual void            RenderDolphinStyle( DOLPHIN_STYLE nDolphinStyle, IDirect3DDevice9* pDevice ) = 0;
    virtual HRESULT         RenderScene() = 0;
};

//--------------------------------------------------------------------------------------
// Declarations
//--------------------------------------------------------------------------------------
IDolphinUI* GetApp();

HRESULT ShowMainMenu();
void HideMenu();
HRESULT RenderUI( IDirect3DDevice9* pDevice, UINT uWidth, UINT uHeight );
void DispatchXuiInput( XINPUT_KEYSTROKE* pKeystroke );
HRESULT InitUI( ATG::Application* pApp );
void UpdateUI();
