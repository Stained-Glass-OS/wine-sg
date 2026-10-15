/* Probe for patches/sg/5214: D3DKMTEscape validates its parameters as documented
 * (STATUS_INVALID_PARAMETER) and, with no kernel-mode driver behind it, does not pretend:
 * a valid request is STATUS_NOT_SUPPORTED, no longer STATUS_NO_MEMORY.
 * Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef LONG NTSTATUS_;
#define ST_SUCCESS 0
#define ST_INVALID_PARAMETER ((NTSTATUS_)0xc000000d)
#define ST_NOT_SUPPORTED ((NTSTATUS_)0xc00000bb)

typedef struct { UINT hAdapter, hDevice, Type, Flags; void *priv; UINT size; UINT hContext; } ESC;
typedef struct { HDC hdc; UINT hAdapter; LUID luid; UINT source; } OPENHDC;
typedef struct { UINT hAdapter; } CLOSE;

typedef NTSTATUS_ (WINAPI *esc_t)(const ESC *);
typedef NTSTATUS_ (WINAPI *open_t)(OPENHDC *);
typedef NTSTATUS_ (WINAPI *close_t)(const CLOSE *);

int main(void)
{
    HMODULE gdi = GetModuleHandleA("gdi32.dll");
    esc_t pEscape = (esc_t)GetProcAddress(gdi, "D3DKMTEscape");
    open_t pOpen = (open_t)GetProcAddress(gdi, "D3DKMTOpenAdapterFromHdc");
    close_t pClose = (close_t)GetProcAddress(gdi, "D3DKMTCloseAdapter");
    OPENHDC oh = {0};
    BYTE data[16] = {0};
    ESC e;
    NTSTATUS_ st;
    unsigned int i;
    UINT adapter;

    if (!pEscape || !pOpen || !pClose) { printf("ok   not available\nRESULT: PASS\n"); return 0; }
    oh.hdc = GetDC(NULL);
    st = pOpen(&oh);
    check(st == ST_SUCCESS && oh.hAdapter, "adapter opened %#lx handle %u", st, oh.hAdapter);
    adapter = oh.hAdapter;
    ReleaseDC(NULL, oh.hdc);

    {
        struct { const char *name; ESC e; NTSTATUS_ want; } tests[] = {
            { "driver private data", { 0, 0, 0, 0, data, sizeof(data), 0 }, ST_NOT_SUPPORTED },
            { "no private data", { 0, 0, 0, 0, NULL, 0, 0 }, ST_NOT_SUPPORTED },
            { "last escape type", { 0, 0, 9, 0, data, sizeof(data), 0 }, ST_NOT_SUPPORTED },
            { "type past the last", { 0, 0, 10, 0, data, sizeof(data), 0 }, ST_INVALID_PARAMETER },
            { "huge type", { 0, 0, 0x7fffffff, 0, data, sizeof(data), 0 }, ST_INVALID_PARAMETER },
            { "size without data", { 0, 0, 0, 0, NULL, 16, 0 }, ST_INVALID_PARAMETER },
            { "no adapter", { 0xffffffff, 0, 0, 0, data, sizeof(data), 0 }, ST_INVALID_PARAMETER },
            { "adapter zero", { 0, 0, 0, 0, data, sizeof(data), 0 }, ST_INVALID_PARAMETER },
        };
        for (i = 0; i < sizeof(tests) / sizeof(*tests); i++)
        {
            e = tests[i].e;
            if (i != 6 && i != 7) e.hAdapter = adapter;
            st = pEscape(&e);
            check(st == tests[i].want, "%s: %#lx (want %#lx)", tests[i].name, st, tests[i].want);
        }
    }
    st = pEscape(NULL);
    check(st == ST_INVALID_PARAMETER, "NULL: %#lx", st);

    {
        CLOSE c = { adapter };
        st = pClose(&c);
        check(st == ST_SUCCESS, "adapter closed %#lx", st);
        e = (ESC){ adapter, 0, 0, 0, data, sizeof(data), 0 };
        st = pEscape(&e);
        check(st == ST_INVALID_PARAMETER, "closed adapter: %#lx", st);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
