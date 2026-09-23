#if 0
// Shader type: vertex 

xvs_3_0
config AutoSerialize=false
config AutoResource=false
config VsMaxReg=25
config VsResource=1

dcl_index r0.xyz
dcl_index1 r1.x
dcl_color o0

def c8, -1, 3, -3, 1
def c9, 3, -6, 3, 0
def c10, -3, 3, 0, 0
def c11, 1, 0, 0, 0
def c12, -3, 9, -9, 3
def c13, 6, -12, 6, 0
def c14, -3, 3, 0, 0
def c15, 0, 1, 2, 3


    exec
    vfetch r10, r0.x, position
    vfetch r11, r0.x, position1
    vfetch r12, r0.x, position2
    vfetch r13, r0.x, position3
    vfetch r14, r0.x, position4
    vfetch r15, r0.x, position5
    exec
    vfetch r16, r0.x, position6
    vfetch r17, r0.x, position7
    vfetch r18, r0.x, position8
    vfetch r19, r0.x, position9
    vfetch r20, r0.x, position10
    vfetch r21, r0.x, position11
    exec
    vfetch r22, r0.x, position12
    vfetch r23, r0.x, position13
    vfetch r24, r0.x, position14
    vfetch r25, r0.x, position15
    alloc interpolators
    alloc position
    exec    // PredicateClean=false
    add r1, r1.x, -c15
    setp_eq r2.x, r1.y
    (p0) add r0.y, c15.y, -r0
    setp_eq r2.x, r1.z
    (p0) add r0.yz, c15.y, -r0
    setp_eq r2.x, r1.w
    exec
    (p0) add r0.z, c15.y, -r0
    mov r2.z, r0.y
    mul r2.y, r0.y, r0.y
    mul r2.x, r0.y, r2.y
    mov r3.z, r0.z
    mul r3.y, r0.z, r0.z
    exec
    mul r3.x, r0.z, r3.y
    mad r4, r2.z, c10, c11
    mad r4, r2.y, c9, r4
    mad r4, r2.x, c8, r4
    mad r5, r3.z, c10, c11
    mad r5, r3.y, c9, r5
    exec
    mad r5, r3.x, c8, r5
    mul r6, r4.x, r10
    mad r6, r4.y, r11, r6
    mad r6, r4.z, r12, r6
    mad r6, r4.w, r13, r6
    mul r7, r4.x, r14
    exec
    mad r7, r4.y, r15, r7
    mad r7, r4.z, r16, r7
    mad r7, r4.w, r17, r7
    mul r8, r4.x, r18
    mad r8, r4.y, r19, r8
    mad r8, r4.z, r20, r8
    exec
    mad r8, r4.w, r21, r8
    mul r9, r4.x, r22
    mad r9, r4.y, r23, r9
    mad r9, r4.z, r24, r9
    mad r9, r4.w, r25, r9
    mul r0, r5.x, r6
    exec
    mad r0, r5.y, r7, r0
    mad r0, r5.z, r8, r0
    mad r0, r5.w, r9, r0
    mad r4, r3.z, c13, c14
    mad r4, r3.y, c12, r4
    mul r3, r4.x, r6
    exec
    mad r3, r4.y, r7, r3
    mad r3, r4.z, r8, r3
    mad r3, r4.w, r9, r3
    mul r6, r5.x, r10
    mad r6, r5.y, r14, r6
    mad r6, r5.z, r18, r6
    exec
    mad r6, r5.w, r22, r6
    mul r7, r5.x, r11
    mad r7, r5.y, r15, r7
    mad r7, r5.z, r19, r7
    mad r7, r5.w, r23, r7
    mul r8, r5.x, r12
    exec
    mad r8, r5.y, r16, r8
    mad r8, r5.z, r20, r8
    mad r8, r5.w, r24, r8
    mul r9, r5.x, r13
    mad r9, r5.y, r17, r9
    mad r9, r5.z, r21, r9
    exec
    mad r9, r5.w, r25, r9
    mad r5, r2.z, c13, c14
    mad r5, r2.y, c12, r5
    mul r2, r5.x, r6
    mad r2, r5.y, r7, r2
    mad r2, r5.z, r8, r2
    exec
    mad r2, r5.w, r9, r2
    mul r1, r2.yzx, r3.zxy
    mad r1, -r2.zxy, r3.yzx, r1
    dp3 r1.w, r1, r1
    rsq r1.w, r1.w
    mul r1, r1, r1.w
    exec
    mul r2, r0.x, c0
    mad r2, r0.y, c1, r2
    mad r2, r0.z, c2, r2
    mad oPos, r0.w, c3, r2
    dp3 o0, r1, c4
    exece

// PDB hint 00000000-00000000-00000000

#endif

// This microcode is in native DWORD byte order.

const DWORD g_CubicBezierPatchVS[] =
{
    0x102a1101, 0x000000c0, 0x00000560, 0x00000000, 0x00000000, 0x00000024, 
    0x0000004c, 0x00000000, 0x00000000, 0x30000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000014, 0x00080020, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000080, 0x000004e0, 0x00010019, 0x00000000, 0x00000000, 
    0x00001021, 0x00000002, 0x00000010, 0x00000001, 0x00000e90, 0x00000391, 
    0x0010000a, 0x0001000b, 0x0002000c, 0x0003000d, 0x0004000e, 0x0005000f, 
    0x00060010, 0x00070011, 0x00080012, 0x00090013, 0x000a0014, 0x000b0015, 
    0x000c0016, 0x000d0017, 0x000e0018, 0x002f0019, 0x0000f0a0, 0x00001066, 
    0xbf800000, 0x40400000, 0xc0400000, 0x3f800000, 0x40400000, 0xc0c00000, 
    0x40400000, 0x00000000, 0xc0400000, 0x40400000, 0x00000000, 0x00000000, 
    0x3f800000, 0x00000000, 0x00000000, 0x00000000, 0xc0400000, 0x41100000, 
    0xc1100000, 0x40400000, 0x40c00000, 0xc1400000, 0x40c00000, 0x00000000, 
    0xc0400000, 0x40400000, 0x00000000, 0x00000000, 0x00000000, 0x3f800000, 
    0x40000000, 0x40400000, 0xf555600a, 0x60101203, 0x1203f555, 0xf0554016, 
    0x00001200, 0xc4000000, 0x00000000, 0x601ac200, 0x10000000, 0x00006020, 
    0x60261200, 0x12000000, 0x0000602c, 0x60321200, 0x12000000, 0x00006038, 
    0x603e1200, 0x12000000, 0x00006044, 0x604a1200, 0x12000000, 0x00006050, 
    0x60561200, 0x12000000, 0x0000605c, 0x50621200, 0x12000000, 0x00000067, 
    0x00002200, 0x00000000, 0x0008a000, 0x00000688, 0x00000000, 0x0008b000, 
    0x00000688, 0x00000000, 0x0008c000, 0x00000688, 0x00000000, 0x0008d000, 
    0x00000688, 0x00000000, 0x0008e000, 0x00000688, 0x00000000, 0x0008f000, 
    0x00000688, 0x00000000, 0x00090000, 0x00000688, 0x00000000, 0x00091000, 
    0x00000688, 0x00000000, 0x00092000, 0x00000688, 0x00000000, 0x00093000, 
    0x00000688, 0x00000000, 0x00094000, 0x00000688, 0x00000000, 0x00095000, 
    0x00000688, 0x00000000, 0x00096000, 0x00000688, 0x00000000, 0x00097000, 
    0x00000688, 0x00000000, 0x00098000, 0x00000688, 0x00000000, 0x00099000, 
    0x00000688, 0x00000000, 0xc80f0001, 0x026c0000, 0xa0010f00, 0x6c100200, 
    0x000000b1, 0xe2000001, 0xc8020000, 0x1ab10000, 0x600f0000, 0x6c100200, 
    0x000000c6, 0xe2000001, 0xc8060000, 0x1ab10000, 0x600f0000, 0x6c100200, 
    0x0000001b, 0xe2000001, 0xc8040000, 0x1ab10000, 0x600f0000, 0xc8040002, 
    0x00b1b100, 0xe2000000, 0xc8020002, 0x00b1b100, 0xe1000000, 0xc8010002, 
    0x00b1b100, 0xe1000200, 0xc8040003, 0x00c6c600, 0xe2000000, 0xc8020003, 
    0x00c6c600, 0xe1000000, 0xc8010003, 0x00c6b100, 0xe1000300, 0xc80f0004, 
    0x00c60000, 0x8b020a0b, 0xc80f0004, 0x00b10000, 0xab020904, 0xc80f0004, 
    0x006c0000, 0xab020804, 0xc80f0005, 0x00c60000, 0x8b030a0b, 0xc80f0005, 
    0x00b10000, 0xab030905, 0xc80f0005, 0x006c0000, 0xab030805, 0xc80f0006, 
    0x006c0000, 0xe1040a00, 0xc80f0006, 0x00b10000, 0xeb040b06, 0xc80f0006, 
    0x00c60000, 0xeb040c06, 0xc80f0006, 0x001b0000, 0xeb040d06, 0xc80f0007, 
    0x006c0000, 0xe1040e00, 0xc80f0007, 0x00b10000, 0xeb040f07, 0xc80f0007, 
    0x00c60000, 0xeb041007, 0xc80f0007, 0x001b0000, 0xeb041107, 0xc80f0008, 
    0x006c0000, 0xe1041200, 0xc80f0008, 0x00b10000, 0xeb041308, 0xc80f0008, 
    0x00c60000, 0xeb041408, 0xc80f0008, 0x001b0000, 0xeb041508, 0xc80f0009, 
    0x006c0000, 0xe1041600, 0xc80f0009, 0x00b10000, 0xeb041709, 0xc80f0009, 
    0x00c60000, 0xeb041809, 0xc80f0009, 0x001b0000, 0xeb041909, 0xc80f0000, 
    0x006c0000, 0xe1050600, 0xc80f0000, 0x00b10000, 0xeb050700, 0xc80f0000, 
    0x00c60000, 0xeb050800, 0xc80f0000, 0x001b0000, 0xeb050900, 0xc80f0004, 
    0x00c60000, 0x8b030d0e, 0xc80f0004, 0x00b10000, 0xab030c04, 0xc80f0003, 
    0x006c0000, 0xe1040600, 0xc80f0003, 0x00b10000, 0xeb040703, 0xc80f0003, 
    0x00c60000, 0xeb040803, 0xc80f0003, 0x001b0000, 0xeb040903, 0xc80f0006, 
    0x006c0000, 0xe1050a00, 0xc80f0006, 0x00b10000, 0xeb050e06, 0xc80f0006, 
    0x00c60000, 0xeb051206, 0xc80f0006, 0x001b0000, 0xeb051606, 0xc80f0007, 
    0x006c0000, 0xe1050b00, 0xc80f0007, 0x00b10000, 0xeb050f07, 0xc80f0007, 
    0x00c60000, 0xeb051307, 0xc80f0007, 0x001b0000, 0xeb051707, 0xc80f0008, 
    0x006c0000, 0xe1050c00, 0xc80f0008, 0x00b10000, 0xeb051008, 0xc80f0008, 
    0x00c60000, 0xeb051408, 0xc80f0008, 0x001b0000, 0xeb051808, 0xc80f0009, 
    0x006c0000, 0xe1050d00, 0xc80f0009, 0x00b10000, 0xeb051109, 0xc80f0009, 
    0x00c60000, 0xeb051509, 0xc80f0009, 0x001b0000, 0xeb051909, 0xc80f0005, 
    0x00c60000, 0x8b020d0e, 0xc80f0005, 0x00b10000, 0xab020c05, 0xc80f0002, 
    0x006c0000, 0xe1050600, 0xc80f0002, 0x00b10000, 0xeb050702, 0xc80f0002, 
    0x00c60000, 0xeb050802, 0xc80f0002, 0x001b0000, 0xeb050902, 0xc80f0001, 
    0x0065be00, 0xe1020300, 0xc80f0001, 0x04be6500, 0xeb020301, 0xc8080001, 
    0x00000000, 0xf0010100, 0x58800100, 0x0000001b, 0xe2000001, 0xc80f0001, 
    0x00001b00, 0xe1010100, 0xc80f0002, 0x006c0000, 0xa1000000, 0xc80f0002, 
    0x00b10000, 0xab000102, 0xc80f0002, 0x00c60000, 0xab000202, 0xc80f803e, 
    0x001b0000, 0xab000302, 0xc80f8000, 0x00000000, 0xb0010400, 0x00000000, 
    0x00000000, 0x00000000
};
