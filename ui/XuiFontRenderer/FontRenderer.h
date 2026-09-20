
#pragma once


enum RenderMode
{
    DrawToTexture,
    DrawToDevice
};

class MyXuiFontRenderer : public IXuiFontRenderer
{
public:
                        MyXuiFontRenderer( IDirect3DDevice9* pD3DDevice );
                        ~MyXuiFontRenderer( VOID );

    virtual HRESULT
    STDMETHODCALLTYPE   Init( FLOAT fDpi );
    virtual VOID
    STDMETHODCALLTYPE   Term();
    virtual HRESULT
    STDMETHODCALLTYPE   GetCaps( DWORD* pdwCaps );
    virtual HRESULT
    STDMETHODCALLTYPE   CreateFont( const TypefaceDescriptor* pTypefaceDescriptor, FLOAT fPointSize,
                                    DWORD dwStyle, DWORD dwReserved, HFONTOBJ* phFont );
    virtual VOID
    STDMETHODCALLTYPE   ReleaseFont( HFONTOBJ hFont );
    virtual HRESULT
    STDMETHODCALLTYPE   GetFontMetrics( HFONTOBJ hFont, XUIFontMetrics* pFontMetrics );
    virtual HRESULT
    STDMETHODCALLTYPE   GetCharMetrics( HFONTOBJ hFont, WCHAR wch, XUICharMetrics* pCharMetrics );
    virtual HRESULT
    STDMETHODCALLTYPE   DrawCharToTexture( HFONTOBJ hFont, WCHAR wch, HXUIDC hDC, IXuiTexture* pTexture,
                                           UINT x, UINT y, UINT width, UINT height, UINT insetX, UINT insetY );
    virtual HRESULT
    STDMETHODCALLTYPE   DrawCharsToDevice( HFONTOBJ hFont, CharData* pCharData, DWORD dwCount, RECT* pClipRect,
                                           HXUIDC hDC, D3DXMATRIX* pWorldViewProj );

    HRESULT             RenderCharacter( IDirect3DDevice9* pDevice, WCHAR wch, FLOAT fPointSize, FLOAT x, FLOAT y,
                                         D3DXCOLOR color );
    void                SetRendererMode( RenderMode mode );

private:

    IDirect3DVertexShader9* m_pVertexShader;
    IDirect3DPixelShader9* m_pPixelShader;
    IDirect3DVertexDeclaration9* m_pVertexDeclaration;
    FLOAT m_fDpi;
    RenderMode m_RenderMode;
};
