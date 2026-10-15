/* Probe for patches/sg/5220: D3DKMTQueryAdapterInfo answers ADAPTERTYPE, ADAPTERGUID,
 * PHYSICALADAPTERCOUNT, GETSEGMENTSIZE and CURRENTDISPLAYMODE and validates its parameters; the
 * types that need a hardware driver stay STATUS_NOT_IMPLEMENTED.  Table driven; last line
 * "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef LONG NTSTATUS_;
#define ST_SUCCESS 0
#define ST_INVALID_PARAMETER ((NTSTATUS_)0xc000000d)
#define ST_NOT_IMPLEMENTED ((NTSTATUS_)0xc0000002)

typedef struct { HDC hdc; UINT hAdapter; LUID luid; UINT source; } OPENHDC;
typedef struct { UINT hAdapter; UINT type; void *data; UINT size; } QAI;
typedef struct { UINT width, height, format, rate, num, den, scan, orient, fixed, flags; } MODE;
typedef struct { UINT source; MODE mode; } CURMODE;
typedef struct { ULONGLONG video, system, shared; } SEG;

typedef NTSTATUS_ (WINAPI *open_t)(OPENHDC *);
typedef NTSTATUS_ (WINAPI *qai_t)(QAI *);
typedef NTSTATUS_ (WINAPI *close_t)(const UINT *);


int main(void)
{
    HMODULE gdi = GetModuleHandleA("gdi32.dll");
    open_t pOpen = (open_t)GetProcAddress(gdi, "D3DKMTOpenAdapterFromHdc");
    qai_t pQuery = (qai_t)GetProcAddress(gdi, "D3DKMTQueryAdapterInfo");
    close_t pClose = (close_t)GetProcAddress(gdi, "D3DKMTCloseAdapter");
    OPENHDC oh = {0}, oh2 = {0};
    QAI q;
    UINT type_bits = 0, count = 0xdead;
    GUID g1 = {0}, g2 = {0};
    SEG seg = {1, 2, 3};
    CURMODE cm = {0};
    NTSTATUS_ st;
    BYTE big[256];
    int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    UINT adapter;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (!pOpen || !pQuery || !pClose) { printf("ok   not available\nRESULT: PASS\n"); return 0; }
    oh.hdc = GetDC(NULL);
    st = pOpen(&oh);
    check(st == ST_SUCCESS && oh.hAdapter, "adapter opened %#lx", st);
    adapter = oh.hAdapter;

    q = (QAI){ adapter, 15, &type_bits, sizeof(type_bits) };
    st = pQuery(&q);
    check(st == ST_SUCCESS && (type_bits & 0x1) && (type_bits & 0x2) && (type_bits & 0x8), "ADAPTERTYPE %#lx bits %#x (render, display, post)", st, type_bits);

    q = (QAI){ adapter, 4, &g1, sizeof(g1) };
    st = pQuery(&q);
    check(st == ST_SUCCESS && (g1.Data1 || g1.Data2 || g1.Data3), "ADAPTERGUID %#lx %08lx", st, g1.Data1);
    oh2.hdc = oh.hdc;
    pOpen(&oh2);
    q = (QAI){ oh2.hAdapter, 4, &g2, sizeof(g2) };
    st = pQuery(&q);
    check(st == ST_SUCCESS && !memcmp(&g1, &g2, sizeof(g1)), "the same adapter opened twice has the same GUID");

    q = (QAI){ adapter, 30, &count, sizeof(count) };
    st = pQuery(&q);
    check(st == ST_SUCCESS && count == 1, "PHYSICALADAPTERCOUNT %#lx %u", st, count);

    q = (QAI){ adapter, 3, &seg, sizeof(seg) };
    st = pQuery(&q);
    check(st == ST_SUCCESS && seg.video != 1 && seg.system == 0 && seg.shared != 3, "GETSEGMENTSIZE %#lx video %llu system %llu shared %llu", st, seg.video, seg.system, seg.shared);

    cm.source = oh.source;
    q = (QAI){ adapter, 9, &cm, sizeof(cm) };
    st = pQuery(&q);
    check(st == ST_SUCCESS && (int)cm.mode.width == w && (int)cm.mode.height == h, "CURRENTDISPLAYMODE %#lx %ux%u (screen %dx%d)", st, cm.mode.width, cm.mode.height, w, h);
    check(cm.mode.format == 0x16 && cm.mode.scan == 1 && cm.mode.orient == 1, "format %#x scan %u orientation %u", cm.mode.format, cm.mode.scan, cm.mode.orient);
    check(cm.mode.rate == cm.mode.num, "refresh %u / %u/%u", cm.mode.rate, cm.mode.num, cm.mode.den);

    /* parameters */
    cm.source = 999;
    q = (QAI){ adapter, 9, &cm, sizeof(cm) };
    st = pQuery(&q);
    check(st == ST_INVALID_PARAMETER, "a source that does not exist: %#lx", st);
    q = (QAI){ adapter, 15, &type_bits, 2 };
    st = pQuery(&q);
    check(st == ST_INVALID_PARAMETER, "buffer too small: %#lx", st);
    q = (QAI){ adapter, 15, NULL, sizeof(type_bits) };
    st = pQuery(&q);
    check(st == ST_INVALID_PARAMETER, "no buffer: %#lx", st);
    q = (QAI){ 0, 15, &type_bits, sizeof(type_bits) };
    st = pQuery(&q);
    check(st == ST_INVALID_PARAMETER, "adapter 0: %#lx", st);
    q = (QAI){ 0x7777, 15, &type_bits, sizeof(type_bits) };
    st = pQuery(&q);
    check(st == ST_INVALID_PARAMETER, "unknown adapter: %#lx", st);
    st = pQuery(NULL);
    check(st == ST_INVALID_PARAMETER, "NULL: %#lx", st);
    q = (QAI){ adapter, 1, big, sizeof(big) };
    st = pQuery(&q);
    check(st == ST_NOT_IMPLEMENTED, "UMDRIVERNAME (needs a hardware driver): %#lx", st);

    pClose(&oh2.hAdapter);
    pClose(&oh.hAdapter);
    q = (QAI){ adapter, 15, &type_bits, sizeof(type_bits) };
    st = pQuery(&q);
    check(st == ST_INVALID_PARAMETER, "closed adapter: %#lx", st);
    ReleaseDC(NULL, oh.hdc);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
