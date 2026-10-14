/* Probe for patches/sg/5200: GetAspectRatioFilterEx returns the device's pixel
 * aspect (the filter SetMapperFlags(ASPECT_FILTERING) selects fonts by), and
 * DdQueryDisplaySettingsUniqueness returns the mode-switch counter: stable between
 * calls without a mode change (exported as GdiEntry13).
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef ULONG (WINAPI *uniq_t)(void);

int main(void)
{
    HMODULE gdi = GetModuleHandleA("gdi32.dll");
    uniq_t uniq = (uniq_t)GetProcAddress(gdi, "GdiEntry13"); /* DdQueryDisplaySettingsUniqueness */
    HDC screen = GetDC(0), mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, 4, 4);
    SIZE sz;
    BOOL ret;
    DWORD err;
    ULONG a, b;
    unsigned int i;
    struct { const char *name; HDC dc; } dcs[] = {{"screen", screen}, {"memory", mem}};

    SelectObject(mem, bmp);
    for (i = 0; i < ARRAYSIZE(dcs); i++)
    {
        memset(&sz, 0xcc, sizeof(sz));
        ret = GetAspectRatioFilterEx(dcs[i].dc, &sz);
        check(ret, "%s: GetAspectRatioFilterEx succeeds", dcs[i].name);
        check(sz.cx == GetDeviceCaps(dcs[i].dc, ASPECTX) && sz.cy == GetDeviceCaps(dcs[i].dc, ASPECTY),
              "%s: %ldx%ld is the device aspect %dx%d", dcs[i].name, sz.cx, sz.cy,
              GetDeviceCaps(dcs[i].dc, ASPECTX), GetDeviceCaps(dcs[i].dc, ASPECTY));
        SetMapperFlags(dcs[i].dc, ASPECT_FILTERING);
        memset(&sz, 0xcc, sizeof(sz));
        ret = GetAspectRatioFilterEx(dcs[i].dc, &sz);
        check(ret && sz.cx == GetDeviceCaps(dcs[i].dc, ASPECTX) && sz.cy == GetDeviceCaps(dcs[i].dc, ASPECTY),
              "%s: with ASPECT_FILTERING still the device aspect", dcs[i].name);
        SetMapperFlags(dcs[i].dc, 0);
    }
    SetLastError(0xdeadbeef);
    ret = GetAspectRatioFilterEx((HDC)0xdeadbeef, &sz);
    err = GetLastError();
    check(!ret && err == ERROR_INVALID_HANDLE, "bad DC: ret %d, error %lu", ret, err);
    SetLastError(0xdeadbeef);
    ret = GetAspectRatioFilterEx(screen, NULL);
    err = GetLastError();
    check(!ret && err == ERROR_INVALID_PARAMETER, "NULL size: ret %d, error %lu", ret, err);

    check(uniq != NULL, "GdiEntry13 found");
    if (uniq)
    {
        a = uniq();
        b = uniq();
        check(a != 0, "uniqueness is not zero: %lu", a);
        check(a == b, "stable without a mode switch: %lu %lu", a, b);
    }
    DeleteDC(mem); DeleteObject(bmp); ReleaseDC(0, screen);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
