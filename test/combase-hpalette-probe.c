/* Probe for patches/sg/5035: HPALETTE_UserSize/Marshal/Unmarshal/Free
 * (userHPALETTE, wtypes.idl): in-process the handle (WDT_INPROC_CALL),
 * otherwise an rpcLOGPALETTE (WDT_REMOTE_CALL, count, palVersion,
 * palNumEntries, entries) from which the receiver makes a palette with the
 * same entries, freed by HPALETTE_UserFree. They were no-ops.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define WDT_INPROC_CALL_ 0x48746457
#define WDT_REMOTE_CALL_ 0x52746457

typedef ULONG (__RPC_USER *size_fn)(ULONG *, ULONG, HPALETTE *);
typedef unsigned char *(__RPC_USER *marshal_fn)(ULONG *, unsigned char *, HPALETTE *);
typedef void (__RPC_USER *free_fn)(ULONG *, HPALETTE *);

int main(void)
{
    static const struct { const char *what; DWORD ctx; UINT entries; } tests[] =
    {
        {"in-process", MSHCTX_INPROC, 3},
        {"another process", MSHCTX_LOCAL, 3},
        {"another process, one entry", MSHCTX_LOCAL, 1},
        {"another process, NULL", MSHCTX_LOCAL, 0},
        {"in-process, NULL", MSHCTX_INPROC, 0},
    };
    HMODULE combase = LoadLibraryA("combase.dll");
    size_fn psize = (void *)GetProcAddress(combase, "HPALETTE_UserSize");
    marshal_fn pmarshal = (void *)GetProcAddress(combase, "HPALETTE_UserMarshal");
    marshal_fn punmarshal = (void *)GetProcAddress(combase, "HPALETTE_UserUnmarshal");
    free_fn pfree = (void *)GetProcAddress(combase, "HPALETTE_UserFree");
    unsigned int i;

    check(psize && pmarshal && punmarshal && pfree, "combase exports the HPALETTE functions");
    if (!psize) { printf("RESULT: FAIL\n"); return 1; }
    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        struct { WORD ver, num; PALETTEENTRY e[3]; } lp = {0x300, tests[i].entries,
            {{10, 20, 30, 0}, {40, 50, 60, 0}, {70, 80, 90, 0}}};
        HPALETTE pal = tests[i].entries ? CreatePalette((LOGPALETTE *)&lp) : NULL, got = (HPALETTE)0xdeadbeef;
        ULONG flags = MAKELONG(tests[i].ctx, NDR_LOCAL_DATA_REPRESENTATION), size, want_size;
        unsigned char buffer[256], *end;
        PALETTEENTRY e[3];

        memset(buffer, 0xcc, sizeof(buffer));
        size = psize(&flags, 0, &pal);
        want_size = 8 + (tests[i].ctx != MSHCTX_INPROC && pal ? 8 + tests[i].entries * 4 : 0);
        check(size == want_size, "%s: size %lu (want %lu)", tests[i].what, size, want_size);
        end = pmarshal(&flags, buffer, &pal);
        check(end == buffer + want_size && *(ULONG *)buffer == (tests[i].ctx == MSHCTX_INPROC ? WDT_INPROC_CALL_ : WDT_REMOTE_CALL_),
              "%s: marshalled %lu bytes, context %#lx", tests[i].what, (ULONG)(end - buffer), *(ULONG *)buffer);
        if (tests[i].ctx != MSHCTX_INPROC && pal)
            check(*(ULONG *)(buffer + 8) == tests[i].entries && *(WORD *)(buffer + 12) == 0x300
                  && *(WORD *)(buffer + 14) == tests[i].entries && buffer[16] == 10 && buffer[18] == 30,
                  "%s: the rpcLOGPALETTE is in the buffer", tests[i].what);
        end = punmarshal(&flags, buffer, &got);
        check(end == buffer + want_size, "%s: unmarshalled %lu bytes", tests[i].what, (ULONG)(end - buffer));
        if (!pal) check(got == NULL, "%s: NULL back (%p)", tests[i].what, got);
        else if (tests[i].ctx == MSHCTX_INPROC) check(got == pal, "%s: the same handle back (%p %p)", tests[i].what, got, pal);
        else
        {
            memset(e, 0, sizeof(e));
            check(got && got != pal && GetPaletteEntries(got, 0, 3, e) == tests[i].entries && e[0].peRed == 10
                  && e[0].peBlue == 30 && (tests[i].entries < 3 || e[2].peGreen == 80),
                  "%s: a new palette with the same entries (%p)", tests[i].what, got);
            pfree(&flags, &got);
            check(GetObjectType(got) == 0, "%s: HPALETTE_UserFree deleted it", tests[i].what);
        }
        if (pal) DeleteObject(pal);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
