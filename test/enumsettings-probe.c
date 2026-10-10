/* EnumDisplaySettingsEx honours the caller's dmSize (patches/sg/2235). */
#include <windows.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

int main(void)
{
    struct { DEVMODEW dm; BYTE guard[64]; } w;
    struct { DEVMODEA dm; BYTE guard[64]; } a;
    BOOL ret;
    int i;

    /* header only: dmSize returned unchanged, no fields */
    memset(&w, 0, sizeof(w));
    w.dm.dmSize = offsetof(DEVMODEW, dmFields);
    ret = EnumDisplaySettingsExW(NULL, ENUM_CURRENT_SETTINGS, &w.dm, 0);
    CHECK(ret, "ret %d", ret);
    CHECK(w.dm.dmSize == offsetof(DEVMODEW, dmFields), "W dmSize %u", w.dm.dmSize);
    CHECK(w.dm.dmFields == 0, "W dmFields %#lx", w.dm.dmFields);
    CHECK(w.dm.dmDeviceName[0] != 0, "W has device name");

    memset(&w, 0, sizeof(w));
    w.dm.dmSize = offsetof(DEVMODEW, dmFields) + 1;
    ret = EnumDisplaySettingsExW(NULL, ENUM_CURRENT_SETTINGS, &w.dm, 0);
    CHECK(ret, "ret %d", ret);
    CHECK(w.dm.dmSize == offsetof(DEVMODEW, dmFields) + 1, "W dmSize %u", w.dm.dmSize);
    CHECK((w.dm.dmFields & (DM_POSITION | DM_DISPLAYORIENTATION)) == (DM_POSITION | DM_DISPLAYORIENTATION), "W dmFields %#lx", w.dm.dmFields);
    CHECK(w.dm.dmPelsWidth == 0, "W pels width written");

    /* a larger buffer is reported at the known size and nothing past it is touched */
    memset(&w, 0xcc, sizeof(w));
    w.dm.dmSize = sizeof(DEVMODEW);
    ret = EnumDisplaySettingsExW(NULL, ENUM_CURRENT_SETTINGS, &w.dm, 0);
    CHECK(ret && w.dm.dmSize == offsetof(DEVMODEW, dmICMMethod), "W full dmSize %u", w.dm.dmSize);
    CHECK(w.dm.dmPelsWidth > 0 && w.dm.dmPelsHeight > 0, "W size %lux%lu", w.dm.dmPelsWidth, w.dm.dmPelsHeight);
    CHECK(w.dm.dmICMMethod == 0xcccccccc, "W wrote past the known fields");
    for (i = 0; i < 64; i++) if (w.guard[i] != 0xcc) break;
    CHECK(i == 64, "W guard");

    /* enumerating modes with a truncated size */
    memset(&w, 0, sizeof(w));
    w.dm.dmSize = offsetof(DEVMODEW, dmFields) + 1;
    ret = EnumDisplaySettingsExW(NULL, 0, &w.dm, 0);
    CHECK(ret, "enum mode 0 ret %d", ret);
    CHECK(w.dm.dmPelsWidth == 0 && w.dm.dmSize == offsetof(DEVMODEW, dmFields) + 1, "enum mode truncated");

    /* ANSI */
    memset(&a, 0, sizeof(a));
    a.dm.dmSize = offsetof(DEVMODEA, dmFields);
    ret = EnumDisplaySettingsExA(NULL, ENUM_CURRENT_SETTINGS, &a.dm, 0);
    CHECK(ret, "A ret %d", ret);
    CHECK(a.dm.dmSize == offsetof(DEVMODEA, dmFields), "A dmSize %u", a.dm.dmSize);
    CHECK(a.dm.dmFields == 0, "A dmFields %#lx", a.dm.dmFields);

    memset(&a, 0, sizeof(a));
    a.dm.dmSize = offsetof(DEVMODEA, dmFields) + 1;
    ret = EnumDisplaySettingsExA(NULL, ENUM_CURRENT_SETTINGS, &a.dm, 0);
    CHECK(ret, "A ret %d", ret);
    CHECK(a.dm.dmSize == offsetof(DEVMODEA, dmFields) + 1, "A dmSize %u", a.dm.dmSize);
    CHECK((a.dm.dmFields & (DM_POSITION | DM_DISPLAYORIENTATION)) == (DM_POSITION | DM_DISPLAYORIENTATION), "A dmFields %#lx", a.dm.dmFields);
    CHECK(a.dm.dmPelsWidth == 0, "A pels width written");

    memset(&a, 0xcc, sizeof(a));
    a.dm.dmSize = sizeof(DEVMODEA);
    ret = EnumDisplaySettingsExA(NULL, ENUM_CURRENT_SETTINGS, &a.dm, 0);
    CHECK(ret && a.dm.dmSize == offsetof(DEVMODEA, dmICMMethod), "A full dmSize %u", a.dm.dmSize);
    CHECK(a.dm.dmPelsWidth > 0, "A width");
    CHECK(a.dm.dmICMMethod == 0xcccccccc, "A wrote past the known fields");

    /* the plain functions do not look at dmSize */
    memset(&w, 0, sizeof(w));
    ret = EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &w.dm);
    CHECK(ret && w.dm.dmSize == offsetof(DEVMODEW, dmICMMethod) && w.dm.dmPelsWidth > 0, "plain W ret %d size %u", ret, w.dm.dmSize);
    memset(&a, 0, sizeof(a));
    ret = EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &a.dm);
    CHECK(ret && a.dm.dmSize == offsetof(DEVMODEA, dmICMMethod) && a.dm.dmPelsWidth > 0, "plain A ret %d size %u", ret, a.dm.dmSize);

    /* too small to hold the header */
    memset(&w, 0, sizeof(w));
    w.dm.dmSize = 8;
    SetLastError(0xdeadbeef);
    ret = EnumDisplaySettingsExW(NULL, ENUM_CURRENT_SETTINGS, &w.dm, 0);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "tiny W ret %d err %lu", ret, GetLastError());

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
