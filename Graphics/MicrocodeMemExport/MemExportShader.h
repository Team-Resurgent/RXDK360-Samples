#if 0
// Shader type: vertex 

xvs_3_0
config AutoSerialize=false
config AutoResource=false
config VsMaxReg=3
config VsResource=1
config VsExportMode=multipass

dcl_index r0.x

def c252, 0, 0, 0, 0
def c253, 0, 0, 0, 0
def c254, 0, 0, 0, 0
def c255, 0, 1, 2, 0.5


    exec
    vfetch r1, r0.x, position
    vfetch r2, r0.x, color
    serialize
    mul r3, r1.x, c0
    mad r3, r1.y, c1, r3
    mad r3, r1.z, c2, r3
    mad r3, r1.w, c3, r3
    alloc export=2
    exec
    mul r0.x, r0.x, c255.z
    mad eA, r0.x, c255.xyx, c4
    mov eM0, r3
    mul eM1, r2, c255.w
    alloc interpolators
    alloc position
    exece
    mov oPos, r3

// PDB hint 00000000-00000000-00000000

#endif

// This microcode is in native DWORD byte order.

const DWORD g_xvs_main[] =
{
    0x102a1111, 0x0000007c, 0x000000f4, 0x00000000, 0x00000000, 0x00000024, 
    0x0000004c, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x00000000, 
    0x00000000, 0x00000014, 0x00fc0010, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000040, 0x000000b4, 0x07010003, 0x00000000, 0x00000000, 
    0x00000000, 0x00000001, 0x00000002, 0x00000000, 0x00000290, 0x00100003, 
    0x0020a004, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x3f800000, 0x40000000, 0x3f000000, 0x30256003, 
    0x00021200, 0xc6000000, 0x00004009, 0x00001200, 0xc4000000, 0x00000000, 
    0x100dc200, 0x22000000, 0x00081000, 0x00000688, 0x00000000, 0x00082000, 
    0x00000688, 0x00000000, 0xc80f0003, 0x006c0000, 0xa1010000, 0xc80f0003, 
    0x00b10000, 0xab010103, 0xc80f0003, 0x00c60000, 0xab010203, 0xc80f0003, 
    0x001b0000, 0xab010303, 0xc8010000, 0x006cc600, 0xa100ff00, 0xc80f8020, 
    0x006c6000, 0x8b00ff04, 0xc80f8021, 0x00000000, 0xe2030300, 0xc80f8022, 
    0x00001b00, 0xa102ff00, 0xc80f803e, 0x00000000, 0xe2030300, 0x00000000, 
    0x00000000, 0x00000000
};
