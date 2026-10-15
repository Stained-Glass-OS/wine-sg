/* LoadKeyboardLayoutA/W/Ex, UnloadKeyboardLayout, the loaded list (patch 3754) */
#include <windows.h>
#include <stdio.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static HKL (WINAPI *pLoadEx)(HKL, LPCWSTR, UINT);
static HKL H(DWORD v) { return (HKL)(ULONG_PTR)v; }

static UINT list(HKL *l, UINT max)
{
    UINT n = GetKeyboardLayoutList(0, NULL), m;
    if (n > max) n = max;
    m = GetKeyboardLayoutList(n, l);
    return m;
}
static int pos(const HKL *l, UINT n, HKL h)
{
    UINT i;
    for (i = 0; i < n; i++) if (l[i] == h) return (int)i;
    return -1;
}
static BOOL preload(UINT idx, char *out)
{
    HKEY k; char name[16]; DWORD sz = 32, type; BOOL r;
    out[0] = 0;
    if (RegOpenKeyA(HKEY_CURRENT_USER, "Keyboard Layout\\Preload", &k)) return FALSE;
    sprintf(name, "%u", idx);
    r = !RegQueryValueExA(k, name, NULL, &type, (BYTE *)out, &sz) && type == REG_SZ;
    RegCloseKey(k);
    return r;
}

int main(void)
{
    HKL l0[64], l1[64], orig = GetKeyboardLayout(0), h, h2;
    UINT n0, n1, i;
    char buf[32];
    WCHAR nm[KL_NAMELENGTH];
    BOOL ok;
    int p;

    pLoadEx = (void *)GetProcAddress(GetModuleHandleA("user32.dll"), "LoadKeyboardLayoutExW");
    if (!pLoadEx) pLoadEx = (void *)GetProcAddress(GetModuleHandleA("user32.dll"), "LoadKeyboardLayoutEx");

    char pre1[32];
    BOOL had_pre = preload(1, pre1);
    n0 = list(l0, 64);
    CHECK(n0 >= 1 && pos(l0, n0, orig) >= 0, "the active layout is listed: %u", n0);

    /* a layout of another language loads, without being active */
    h = LoadKeyboardLayoutA("00000429", 0);
    CHECK(h == H(0x04290429), "load 429: %p", h);
    CHECK(GetKeyboardLayout(0) == orig, "not activated");
    n1 = list(l1, 64);
    CHECK(n1 == n0 + 1, "one more layout: %u -> %u", n0, n1);
    CHECK(pos(l1, n1, h) == (int)n1 - 1, "appended at the end: %d", pos(l1, n1, h));
    CHECK(pos(l1, n1, orig) >= 0, "the first layouts stay");
    /* Windows' own list: Preload */

    /* loading it again changes nothing */
    h2 = LoadKeyboardLayoutW(L"00000429", 0);
    CHECK(h2 == h, "again: %p", h2);
    CHECK(list(l1, 64) == n1, "still %u", n1);

    /* KLF_REORDER puts it first */
    h2 = LoadKeyboardLayoutW(L"0000040C", KLF_REORDER);
    CHECK(h2 == H(0x040c040c), "load 40c: %p", h2);
    n1 = list(l1, 64);
    CHECK(n1 == n0 + 2 && l1[0] == h2, "reordered to the front: %u first %p", n1, l1[0]);
    h2 = LoadKeyboardLayoutW(L"00000429", KLF_REORDER);
    n1 = list(l1, 64);
    CHECK(h2 == h && l1[0] == h && n1 == n0 + 2, "reorder of a loaded one: first %p count %u", l1[0], n1);

    /* names that are no layout */
    SetLastError(0xdeadbeef);
    h2 = LoadKeyboardLayoutW(L"zzzz", 0);
    CHECK(!h2 && GetLastError() == ERROR_INVALID_PARAMETER, "bad name: %p %lu", h2, GetLastError());
    SetLastError(0xdeadbeef);
    h2 = LoadKeyboardLayoutW(L"123456789", 0);
    CHECK(!h2 && GetLastError() == ERROR_INVALID_PARAMETER, "too long: %p %lu", h2, GetLastError());
    SetLastError(0xdeadbeef);
    h2 = LoadKeyboardLayoutW(L"0409zz", 0);
    CHECK(!h2 && GetLastError() == ERROR_INVALID_PARAMETER, "trailing junk: %p %lu", h2, GetLastError());
    CHECK(list(l1, 64) == n0 + 2, "nothing was added");

    /* a short buffer gets that many layouts and nothing more */
    {
        HKL small[3] = { H(0x55555555), H(0x55555555), H(0x55555555) };
        UINT got = GetKeyboardLayoutList(2, small);
        CHECK(got == 2 && small[2] == H(0x55555555), "size 2: %u %p", got, small[2]);
        CHECK(GetKeyboardLayoutList(0, NULL) == n0 + 2, "count only");
    }

    /* a layout that is not loaded cannot be activated, a loaded one can, whatever its language */
    SetLastError(0xdeadbeef);
    CHECK(!ActivateKeyboardLayout(H(0x04070407), 0), "an unloaded layout is refused");
    CHECK(GetKeyboardLayout(0) == orig, "still the original");
    h2 = LoadKeyboardLayoutW(L"00000408", KLF_ACTIVATE);
    CHECK(h2 == H(0x04080408), "load+activate: %p", h2);
    CHECK(GetKeyboardLayout(0) == H(0x04080408), "active: %p", GetKeyboardLayout(0));
    ok = GetKeyboardLayoutNameW(nm);
    CHECK(ok && !lstrcmpW(nm, L"00000408"), "layout name %ls", nm);

    /* UnloadKeyboardLayout */
    SetLastError(0xdeadbeef);
    CHECK(!UnloadKeyboardLayout(H(0x04080408)) && GetLastError() == ERROR_INVALID_KEYBOARD_HANDLE, "the active layout stays: %lu", GetLastError());
    SetLastError(0xdeadbeef);
    CHECK(!UnloadKeyboardLayout(H(0x07770777)) && GetLastError() == ERROR_INVALID_KEYBOARD_HANDLE, "not loaded: %lu", GetLastError());
    CHECK(ActivateKeyboardLayout(orig, 0) != NULL, "back to the original");
    CHECK(GetKeyboardLayout(0) == orig, "original active");
    n1 = list(l1, 64);
    CHECK(UnloadKeyboardLayout(H(0x04080408)), "unload 408: %lu", GetLastError());
    CHECK(list(l1, 64) == n1 - 1 && pos(l1, n1 - 1, H(0x04080408)) < 0, "408 is gone");
    SetLastError(0xdeadbeef);
    CHECK(!UnloadKeyboardLayout(H(0x04080408)) && GetLastError() == ERROR_INVALID_KEYBOARD_HANDLE, "twice");
    SetLastError(0xdeadbeef);
    CHECK(!ActivateKeyboardLayout(H(0x04080408), 0), "an unloaded layout is refused again");
    n1 = list(l1, 64);
    p = pos(l1, n1, H(0x04290429));

    /* LoadKeyboardLayoutEx replaces a slot */
    CHECK(pLoadEx != NULL, "LoadKeyboardLayoutEx");
    if (pLoadEx)
    {
        SetLastError(0xdeadbeef);
        CHECK(pLoadEx(NULL, L"00000429", 0) == NULL, "NULL hkl");
        n0 = list(l0, 64);
        p = pos(l0, n0, H(0x04290429));
        h = pLoadEx(H(0x04290429), L"00000407", 0);
        CHECK(h == H(0x04070407), "ex returns the new layout: %p", h);
        n1 = list(l1, 64);
        CHECK(n1 == n0 && l1[p] == H(0x04070407), "slot %d replaced: %u %p", p, n1, l1[p]);
        for (i = 0; i < n0; i++) if ((int)i != p) CHECK(l1[i] == l0[i], "other slots unchanged %u", i);
        h = pLoadEx(H(0x04070407), L"00000429", 0);
        CHECK(h == H(0x04290429), "and back: %p", h);
        n1 = list(l1, 64);
        CHECK(n1 == n0 && !memcmp(l0, l1, n0 * sizeof(HKL)), "restored");
        /* the active layout can be replaced without leaving it */
        h = pLoadEx(orig, L"00000429", 0);
        CHECK(GetKeyboardLayout(0) == orig, "active stays");
        n1 = list(l1, 64);
        /* orig's slot is now 429 and 429 was elsewhere: one slot less */
        CHECK(n1 == n0 - 1 && pos(l1, n1, orig) < 0, "duplicate collapses: %u", n1);
        h = pLoadEx(H(0x04290429), L"00000409", 0);
        (void)h;
        /* an hkl that is not loaded: the layout is just loaded */
        n1 = list(l1, 64);
        h = pLoadEx(H(0x07770777), L"00000419", 0);
        CHECK(h == H(0x04190419) && list(l1, 64) == n1 + 1 && pos(l1, n1 + 1, h) == (int)n1, "unknown hkl: %p", h);
    }

    /* the last layout cannot be unloaded, active or not */
    {
        HKL cur = GetKeyboardLayout(0);
        n1 = list(l1, 64);
        for (i = 0; i < n1; i++) if (l1[i] != cur && l1[i] != H(0x04290429)) UnloadKeyboardLayout(l1[i]);
        n1 = list(l1, 64);
        if (pos(l1, n1, H(0x04290429)) < 0) LoadKeyboardLayoutW(L"00000429", 0);
        if (pLoadEx)
        {
            n1 = list(l1, 64);
            for (i = 0; i < n1; i++) if (l1[i] != H(0x04290429)) pLoadEx(l1[i], L"00000429", 0);
            n1 = list(l1, 64);
            CHECK(n1 == 1 && l1[0] == H(0x04290429) && cur != l1[0], "only a layout that is not active is left: %u %p", n1, l1[0]);
            SetLastError(0xdeadbeef);
            CHECK(!UnloadKeyboardLayout(H(0x04290429)) && GetLastError() == ERROR_INVALID_KEYBOARD_HANDLE, "the last one stays, not active");
            pLoadEx(H(0x04290429), L"00000409", 0);
        }
    }
    /* the last layout cannot be unloaded */
    n1 = list(l1, 64);
    for (i = 0; i < n1; i++) if (l1[i] != GetKeyboardLayout(0)) UnloadKeyboardLayout(l1[i]);
    n1 = list(l1, 64);
    CHECK(n1 == 1, "only the active one is left: %u", n1);
    SetLastError(0xdeadbeef);
    CHECK(!UnloadKeyboardLayout(l1[0]) && GetLastError() == ERROR_INVALID_KEYBOARD_HANDLE, "the last one stays");

    /* the Preload list is the session's start configuration: untouched */
    {
        char now[32];
        CHECK(preload(1, now) == had_pre && !lstrcmpA(now, pre1), "Preload 1 unchanged: '%s' / '%s'", pre1, now);
        CHECK(!preload(2, now), "Preload has no second entry");
    }
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
