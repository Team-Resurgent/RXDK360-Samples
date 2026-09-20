//---------------------------------------------------------------------------------------------------------
// FastUntileCPU.h
//
// XNA Developer Connection
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------


#pragma once

class CPUUntiler
{
public:
    VOID InitializeRemapping( UINT iTexelPitch );
    VOID UntileTexture( IDirect3DTexture9* pSourceTexture, IDirect3DTexture9* pDestTexture );

private:
    WORD m_pLinearToTiled2DAddress[64*64];
    UINT m_iRepeatBlockWidth;
    UINT m_iRepeatBlockHeight;
};
