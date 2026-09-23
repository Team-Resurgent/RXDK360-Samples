#if 0
// Shader type: pixel 

xps_3_0
config AutoSerialize=false
config AutoResource=false
// PsExportColorCount=1

dcl_color_centroid r0


    alloc colors
    exec
    mov oC0, r0
    exece
    nop

// PDB hint 00000000-00000000-00000000

#endif

// This microcode is in native DWORD byte order.

const DWORD g_SimplePS[] =
{
    0x102a1100, 0x00000048, 0x0000003c, 0x00000000, 0x00000000, 0x00000000, 
    0x00000024, 0x00000000, 0x00000000, 0x00000000, 0x0000003c, 0x10000000, 
    0x00000000, 0x00000000, 0x00001021, 0x00000001, 0x00000001, 0x0000f0a0, 
    0x00000000, 0x1002c400, 0x12000000, 0x00001003, 0x00002200, 0x00000000, 
    0xc80f8000, 0x00000000, 0xe2000000, 0xc8000000, 0x00000000, 0xe2000000, 
    0x00000000, 0x00000000, 0x00000000
};
