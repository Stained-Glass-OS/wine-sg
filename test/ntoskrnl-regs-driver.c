/* Probe driver for patches/sg/5080: READ_REGISTER_* / WRITE_REGISTER_* and their buffer variants,
 * called through the exports (drivers built with the headers inline them). */
#include "sg-driver.h"

typedef UCHAR (NTAPI *rd8_t)(volatile UCHAR *);
typedef USHORT (NTAPI *rd16_t)(volatile USHORT *);
typedef ULONG (NTAPI *rd32_t)(volatile ULONG *);
typedef void (NTAPI *wr8_t)(volatile UCHAR *, UCHAR);
typedef void (NTAPI *wr16_t)(volatile USHORT *, USHORT);
typedef void (NTAPI *wr32_t)(volatile ULONG *, ULONG);
typedef void (NTAPI *buf8_t)(volatile UCHAR *, UCHAR *, ULONG);
typedef void (NTAPI *buf16_t)(volatile USHORT *, USHORT *, ULONG);
typedef void (NTAPI *buf32_t)(volatile ULONG *, ULONG *, ULONG);

static void *routine(const WCHAR *name)
{
    UNICODE_STRING str;
    void *p;

    RtlInitUnicodeString(&str, name);
    p = MmGetSystemRoutineAddress(&str);
    check(p != NULL, "%ws is exported", name);
    return p;
}

static void run_probe(DRIVER_OBJECT *driver)
{
    static volatile UCHAR regs[32];
    volatile USHORT *r16 = (volatile USHORT *)regs;
    volatile ULONG *r32 = (volatile ULONG *)regs;
    UCHAR b8[8];
    USHORT b16[4];
    ULONG b32[4];
    rd8_t rd8 = routine(L"READ_REGISTER_UCHAR");
    rd16_t rd16 = routine(L"READ_REGISTER_USHORT");
    rd32_t rd32 = routine(L"READ_REGISTER_ULONG");
    wr8_t wr8 = routine(L"WRITE_REGISTER_UCHAR");
    wr16_t wr16 = routine(L"WRITE_REGISTER_USHORT");
    wr32_t wr32 = routine(L"WRITE_REGISTER_ULONG");
    buf8_t rb8 = routine(L"READ_REGISTER_BUFFER_UCHAR");
    buf16_t rb16 = routine(L"READ_REGISTER_BUFFER_USHORT");
    buf32_t rb32 = routine(L"READ_REGISTER_BUFFER_ULONG");
    buf8_t wb8 = routine(L"WRITE_REGISTER_BUFFER_UCHAR");
    buf16_t wb16 = routine(L"WRITE_REGISTER_BUFFER_USHORT");
    buf32_t wb32 = routine(L"WRITE_REGISTER_BUFFER_ULONG");
    ULONG i;

    if (!rd8 || !rd16 || !rd32 || !wr8 || !wr16 || !wr32 || !rb8 || !rb16 || !rb32 || !wb8 || !wb16 || !wb32) return;

    for (i = 0; i < sizeof(regs); i++) regs[i] = 0x10 + i;
    check(rd8(regs + 3) == 0x13, "READ_REGISTER_UCHAR %#x", rd8(regs + 3));
    check(rd16(r16 + 1) == 0x1312, "READ_REGISTER_USHORT %#x", rd16(r16 + 1));
    check(rd32(r32 + 1) == 0x17161514, "READ_REGISTER_ULONG %#lx", rd32(r32 + 1));

    wr8(regs + 20, 0xaa);
    check(regs[20] == 0xaa && regs[19] == 0x23 && regs[21] == 0x25, "WRITE_REGISTER_UCHAR writes one byte");
    wr16(r16 + 11, 0xbbcc);
    check(regs[22] == 0xcc && regs[23] == 0xbb && regs[24] == 0x28, "WRITE_REGISTER_USHORT writes two bytes");
    wr32(r32 + 7, 0x01020304);
    check(regs[28] == 4 && regs[29] == 3 && regs[30] == 2 && regs[31] == 1 && regs[27] == 0x2b, "WRITE_REGISTER_ULONG writes four bytes");

    memset(b8, 0, sizeof(b8));
    rb8(regs + 2, b8, 5);
    check(b8[0] == 0x12 && b8[4] == 0x16 && !b8[5], "READ_REGISTER_BUFFER_UCHAR reads Count bytes");
    memset(b16, 0, sizeof(b16));
    rb16(r16 + 1, b16, 3);
    check(b16[0] == 0x1312 && b16[1] == 0x1514 && b16[2] == 0x1716 && !b16[3], "READ_REGISTER_BUFFER_USHORT reads Count words");
    memset(b32, 0, sizeof(b32));
    rb32(r32, b32, 3);
    check(b32[0] == 0x13121110 && b32[1] == 0x17161514 && b32[2] == 0x1b1a1918 && !b32[3], "READ_REGISTER_BUFFER_ULONG reads Count dwords");

    for (i = 0; i < 8; i++) b8[i] = 0x80 + i;
    wb8(regs + 8, b8, 4);
    check(regs[8] == 0x80 && regs[11] == 0x83 && regs[12] == 0x1c, "WRITE_REGISTER_BUFFER_UCHAR writes Count bytes");
    b16[0] = 0x1111; b16[1] = 0x2222; b16[2] = 0x3333; b16[3] = 0x4444;
    wb16(r16 + 8, b16, 2);
    check(r16[8] == 0x1111 && r16[9] == 0x2222 && r16[10] == 0x25aa, "WRITE_REGISTER_BUFFER_USHORT writes Count words");
    b32[0] = 0xdead0001; b32[1] = 0xdead0002; b32[2] = 0xdead0003; b32[3] = 0;
    wb32(r32 + 1, b32, 2);
    check(r32[1] == 0xdead0001 && r32[2] == 0xdead0002 && r32[3] == 0x1f1e1d1c, "WRITE_REGISTER_BUFFER_ULONG writes Count dwords");
    rb8(regs, b8, 0);
    check(b8[0] == 0x80, "a zero Count leaves the buffer alone");
}
