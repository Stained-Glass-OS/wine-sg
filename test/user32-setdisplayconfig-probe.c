/* Probe for patches/sg/5209: SetDisplayConfig validates the SDC_* flag combinations, applies
 * the supplied source modes (all or nothing, SDC_ALLOW_CHANGES picks the nearest mode),
 * and answers topology requests for the current topology.
 * Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define A_  0x80
#define V_  0x40
#define US  0x20
#define TS  0x10
#define T_INT 1
#define T_CLONE 2
#define T_EXT 4
#define T_EXTN 8

int main(void)
{
    static const struct { const char *name; UINT flags; int arrays; LONG want; } flag_tests[] = {
        /* arrays: 0 none, 1 current config */
        { "zero flags", 0, 1, ERROR_INVALID_PARAMETER },
        { "apply+validate", A_ | V_ | US, 1, ERROR_INVALID_PARAMETER },
        { "neither apply nor validate", US, 1, ERROR_INVALID_PARAMETER },
        { "supplied+topology", A_ | US | T_EXT, 1, ERROR_INVALID_PARAMETER },
        { "no supplied nor topology", A_, 0, ERROR_INVALID_PARAMETER },
        { "no optimization with validate", V_ | US | 0x100, 1, ERROR_INVALID_PARAMETER },
        { "save without supplied", A_ | T_EXT | 0x200, 0, ERROR_INVALID_PARAMETER },
        { "persist with supplied", A_ | US | 0x800, 1, ERROR_INVALID_PARAMETER },
        { "force enumeration with validate", V_ | US | 0x1000, 1, ERROR_INVALID_PARAMETER },
        { "path order without topology supplied", A_ | T_EXT | 0x2000, 0, ERROR_INVALID_PARAMETER },
        { "unknown bit", A_ | T_EXT | 0x40000000, 0, ERROR_INVALID_PARAMETER },
        { "topology with arrays", A_ | T_EXT, 1, ERROR_INVALID_PARAMETER },
        { "supplied without arrays", V_ | US, 0, ERROR_INVALID_PARAMETER },
        { "validate extend", V_ | T_EXT, 0, ERROR_SUCCESS },
        { "apply extend + persist", A_ | T_EXT | 0x800, 0, ERROR_SUCCESS },
        { "extend|clone", A_ | T_EXT | T_CLONE, 0, ERROR_SUCCESS },
        { "external only", V_ | T_EXTN, 0, ERROR_BAD_CONFIGURATION },
        { "clone only", V_ | T_CLONE, 0, ERROR_BAD_CONFIGURATION },
        { "validate supplied", V_ | US, 1, ERROR_SUCCESS },
        { "validate supplied allow changes", V_ | US | 0x400, 1, ERROR_SUCCESS },
        { "topology supplied (current)", V_ | TS, 1, ERROR_SUCCESS },
        { "topology supplied + extend: supplied ignored", V_ | TS | T_EXT, 0, ERROR_SUCCESS },
        { "apply supplied and save", A_ | US | 0x200, 1, ERROR_SUCCESS },
        { "apply supplied no optimization", A_ | US | 0x100, 1, ERROR_SUCCESS },
        { "apply supplied force enumeration", A_ | US | 0x1000, 1, ERROR_SUCCESS },
    };
    UINT32 npaths = 0, nmodes = 0, i, srcidx;
    DISPLAYCONFIG_PATH_INFO *paths;
    DISPLAYCONFIG_MODE_INFO *modes;
    LONG ret;
    int w0 = GetSystemMetrics(SM_CXSCREEN), h0 = GetSystemMetrics(SM_CYSCREEN);
    DEVMODEW dm = {.dmSize = sizeof(dm)}, other = {.dmSize = sizeof(dm)};
    BOOL have_other = FALSE;

    ret = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &npaths, &nmodes);
    check(ret == 0 && npaths && nmodes, "buffer sizes %ld paths %u modes %u", ret, npaths, nmodes);
    paths = calloc(npaths, sizeof(*paths));
    modes = calloc(nmodes, sizeof(*modes));
    ret = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &npaths, paths, &nmodes, modes, NULL);
    check(ret == 0, "QueryDisplayConfig %ld", ret);

    for (i = 0; i < sizeof(flag_tests) / sizeof(*flag_tests); i++)
    {
        if (flag_tests[i].arrays) ret = SetDisplayConfig(npaths, paths, nmodes, modes, flag_tests[i].flags);
        else ret = SetDisplayConfig(0, NULL, 0, NULL, flag_tests[i].flags);
        check(ret == flag_tests[i].want, "%s: %ld (want %ld)", flag_tests[i].name, ret, flag_tests[i].want);
    }

    /* a bad mode index, no active path, an inactive-only set */
    {
        DISPLAYCONFIG_PATH_INFO bad = paths[0];
        bad.sourceInfo.modeInfoIdx = nmodes + 5;
        ret = SetDisplayConfig(1, &bad, nmodes, modes, V_ | US);
        check(ret == ERROR_INVALID_PARAMETER, "mode index out of range: %ld", ret);
        bad = paths[0];
        bad.flags &= ~DISPLAYCONFIG_PATH_ACTIVE;
        ret = SetDisplayConfig(1, &bad, nmodes, modes, V_ | US);
        check(ret == ERROR_INVALID_PARAMETER, "no active path: %ld", ret);
        bad = paths[0];
        bad.targetInfo.id += 77;
        ret = SetDisplayConfig(1, &bad, nmodes, modes, V_ | TS);
        check(ret == ERROR_BAD_CONFIGURATION, "unknown topology: %ld", ret);
    }

    /* a source mode the display cannot do: validate fails, nothing changes; ALLOW_CHANGES validates */
    srcidx = paths[0].sourceInfo.modeInfoIdx;
    if (srcidx < nmodes && modes[srcidx].infoType == DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE)
    {
        UINT32 w = modes[srcidx].sourceMode.width, h = modes[srcidx].sourceMode.height;

        modes[srcidx].sourceMode.width = 123;
        modes[srcidx].sourceMode.height = 45;
        ret = SetDisplayConfig(npaths, paths, nmodes, modes, V_ | US);
        check(ret == ERROR_BAD_CONFIGURATION, "validate impossible mode: %ld", ret);
        ret = SetDisplayConfig(npaths, paths, nmodes, modes, A_ | US);
        check(ret == ERROR_BAD_CONFIGURATION, "apply impossible mode: %ld", ret);
        check(GetSystemMetrics(SM_CXSCREEN) == w0 && GetSystemMetrics(SM_CYSCREEN) == h0, "screen unchanged");
        ret = SetDisplayConfig(npaths, paths, nmodes, modes, V_ | US | 0x400);
        check(ret == ERROR_SUCCESS, "validate impossible mode with allow changes: %ld", ret);
        modes[srcidx].sourceMode.width = w;
        modes[srcidx].sourceMode.height = h;
    }
    else check(0, "no source mode in the first path");

    /* applying another real mode of the display changes the screen, and back */
    for (i = 0; EnumDisplaySettingsExW(NULL, i, &dm, 0); i++)
        if ((int)dm.dmPelsWidth != w0 || (int)dm.dmPelsHeight != h0)
        {
            if (ChangeDisplaySettingsExW(NULL, &dm, NULL, CDS_TEST, NULL) == DISP_CHANGE_SUCCESSFUL)
            {
                other = dm;
                have_other = TRUE;
                break;
            }
        }
    if (have_other && srcidx < nmodes)
    {
        modes[srcidx].sourceMode.width = other.dmPelsWidth;
        modes[srcidx].sourceMode.height = other.dmPelsHeight;
        paths[0].targetInfo.refreshRate.Numerator = 0;
        ret = SetDisplayConfig(npaths, paths, nmodes, modes, A_ | US);
        check(ret == ERROR_SUCCESS, "apply %ux%u: %ld", other.dmPelsWidth, other.dmPelsHeight, ret);
        check(GetSystemMetrics(SM_CXSCREEN) == (int)other.dmPelsWidth && GetSystemMetrics(SM_CYSCREEN) == (int)other.dmPelsHeight,
              "screen is now %dx%d", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
        modes[srcidx].sourceMode.width = w0;
        modes[srcidx].sourceMode.height = h0;
        ret = SetDisplayConfig(npaths, paths, nmodes, modes, A_ | US);
        check(ret == ERROR_SUCCESS && GetSystemMetrics(SM_CXSCREEN) == w0, "applied back: %ld, %dx%d", ret,
              GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    }
    else if (srcidx < nmodes)
    {
        /* one size only: a different colour depth is a different mode */
        DEVMODEW cur = {.dmSize = sizeof(cur)};
        DISPLAYCONFIG_PIXELFORMAT old_pf = modes[srcidx].sourceMode.pixelFormat;

        modes[srcidx].sourceMode.pixelFormat = DISPLAYCONFIG_PIXELFORMAT_16BPP;
        paths[0].targetInfo.refreshRate.Numerator = 0;
        ret = SetDisplayConfig(npaths, paths, nmodes, modes, A_ | US);
        check(ret == ERROR_SUCCESS, "apply 16 bpp: %ld", ret);
        EnumDisplaySettingsExW(NULL, ENUM_CURRENT_SETTINGS, &cur, 0);
        check(cur.dmBitsPerPel == 16, "current mode has %lu bpp", cur.dmBitsPerPel);
        modes[srcidx].sourceMode.pixelFormat = old_pf;
        ret = SetDisplayConfig(npaths, paths, nmodes, modes, A_ | US);
        EnumDisplaySettingsExW(NULL, ENUM_CURRENT_SETTINGS, &cur, 0);
        check(ret == ERROR_SUCCESS && cur.dmBitsPerPel == 32, "applied back: %ld, %lu bpp", ret, cur.dmBitsPerPel);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
