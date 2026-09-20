//---------------------------------------------------------------------------------------------------------
// FastBlockCompressVMX.h
//
// XNA Developer Connection
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------


struct TextureDescAndBaseAddress;

class VMXCompressor
{
public:
    VOID CompressTexture( const TextureDescAndBaseAddress* pSrcDescAndBaseAddress, 
        const TextureDescAndBaseAddress* pDstDescAndBaseAddress, 
        UINT iCompressedType, 
        UINT iSIMDFormat );
};

