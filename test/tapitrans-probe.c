/* tapi32 locations and translation (patches/sg/2421), run by
 * test/tapitrans-gate.sh: lineSetCurrentLocation sets the current location,
 * lineGetTranslateCapsW gives what lineGetTranslateCapsA gives with wide
 * strings, and lineTranslateAddress/Dialog, which are the job of a line device,
 * fail for lack of one. */
#include <windows.h>
#include <tapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define IS(call, err, what) do { LONG r_ = (LONG)(call); if (r_ != (LONG)(err)) printf("      got %08lx want %08lx\n", (unsigned long)r_, (unsigned long)(err)); check(r_ == (LONG)(err), what); } while (0)

#define E_BADDEVICEID    0x80000002
#define E_INVALLOCATION  0x8000002D
#define E_INVALAPPHANDLE 0x80000014
#define E_STRUCTURETOOSMALL 0x8000004D
#define E_INVALPOINTER   0x80000035

typedef LONG (WINAPI *initexa_fn)(HLINEAPP *, HINSTANCE, LINECALLBACK, const char *, DWORD *, DWORD *, void *);
typedef LONG (WINAPI *shut_fn)(HLINEAPP);
typedef LONG (WINAPI *setloc_fn)(HLINEAPP, DWORD);
typedef LONG (WINAPI *caps_fn)(HLINEAPP, DWORD, void *);
typedef LONG (WINAPI *xlate_fn)(HLINEAPP, DWORD, DWORD, const void *, DWORD, DWORD, void *);
typedef LONG (WINAPI *dlg_fn)(HLINEAPP, DWORD, DWORD, HWND, const void *);

static void CALLBACK cb(DWORD dev, DWORD msg, DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR p2, DWORD_PTR p3) {}

static const char locs[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Telephony\\Locations";

static void make_location(const char *key, DWORD id, const char *name, const char *area)
{
    HKEY k;
    DWORD country = 1, flags = 1;
    char path[256];

    sprintf(path, "%s\\%s", locs, key);
    RegCreateKeyA(HKEY_LOCAL_MACHINE, path, &k);
    RegSetValueExA(k, "ID", 0, REG_DWORD, (BYTE *)&id, 4);
    RegSetValueExA(k, "Name", 0, REG_SZ, (const BYTE *)name, strlen(name) + 1);
    RegSetValueExA(k, "AreaCode", 0, REG_SZ, (const BYTE *)area, strlen(area) + 1);
    RegSetValueExA(k, "Country", 0, REG_DWORD, (BYTE *)&country, 4);
    RegSetValueExA(k, "Flags", 0, REG_DWORD, (BYTE *)&flags, 4);
    RegSetValueExA(k, "OutsideAccess", 0, REG_SZ, (const BYTE *)"9,", 3);
    RegSetValueExA(k, "LongDistanceAccess", 0, REG_SZ, (const BYTE *)"8,", 3);
    RegSetValueExA(k, "DisableCallWaiting", 0, REG_SZ, (const BYTE *)"*70,", 5);
    RegCloseKey(k);
}

static DWORD current_id(void)
{
    HKEY k;
    DWORD id = 0xffff, n = 4;
    if (!RegOpenKeyA(HKEY_LOCAL_MACHINE, locs, &k)) { RegQueryValueExA(k, "CurrentID", NULL, NULL, (BYTE *)&id, &n); RegCloseKey(k); }
    return id;
}

int main(void)
{
    HMODULE mod = LoadLibraryA("tapi32.dll");
    initexa_fn init = (initexa_fn)GetProcAddress(mod, "lineInitializeExA");
    shut_fn shut = (shut_fn)GetProcAddress(mod, "lineShutdown");
    setloc_fn setloc = (setloc_fn)GetProcAddress(mod, "lineSetCurrentLocation");
    caps_fn capsA = (caps_fn)GetProcAddress(mod, "lineGetTranslateCapsA");
    caps_fn capsW = (caps_fn)GetProcAddress(mod, "lineGetTranslateCapsW");
    xlate_fn xlA = (xlate_fn)GetProcAddress(mod, "lineTranslateAddressA");
    xlate_fn xlW = (xlate_fn)GetProcAddress(mod, "lineTranslateAddressW");
    dlg_fn dlA = (dlg_fn)GetProcAddress(mod, "lineTranslateDialogA");
    dlg_fn dlW = (dlg_fn)GetProcAddress(mod, "lineTranslateDialogW");
    HLINEAPP app = 0;
    DWORD ndev = 0, ver = 0x00030001, i;
    BYTE bufA[8192], bufW[8192];
    DWORD *ca = (DWORD *)bufA, *cw = (DWORD *)bufW;
    LONG r;

    check(init && shut && setloc && capsA && capsW && xlA && xlW && dlA && dlW, "the calls are exported");
    make_location("Location1", 1, "Home", "555");
    make_location("Location2", 2, "Office", "222");
    r = init(&app, GetModuleHandleA(NULL), cb, "sgtrans", &ndev, &ver, NULL);
    check(r == 0 && app, "an application is registered");

    /* lineSetCurrentLocation */
    IS(setloc(app, 2), 0, "lineSetCurrentLocation(2)");
    check(current_id() == 2, "sets the current location");
    IS(setloc(app, 1), 0, "lineSetCurrentLocation(1)");
    check(current_id() == 1, "and back");
    IS(setloc(app, 77), E_INVALLOCATION, "a location that is not there: LINEERR_INVALLOCATION");
    check(current_id() == 1, "which changes nothing");
    IS(setloc((HLINEAPP)0x6666, 2), E_INVALAPPHANDLE, "an application that is not registered: LINEERR_INVALAPPHANDLE");
    check(current_id() == 1, "which changes nothing either");

    /* lineGetTranslateCapsW against A */
    memset(bufA, 0, sizeof(bufA)); memset(bufW, 0, sizeof(bufW));
    ca[0] = sizeof(bufA); cw[0] = sizeof(bufW);
    IS(capsA(app, ver, bufA), 0, "lineGetTranslateCapsA");
    IS(capsW(app, ver, bufW), 0, "lineGetTranslateCapsW");
    check(cw[3] == ca[3] && cw[3] >= 2 && cw[7] == ca[7] && cw[6] == ca[6], "the same numbers of locations and cards, the same current location");
    {
        LINELOCATIONENTRY *la = (LINELOCATIONENTRY *)(bufA + ca[5]), *lw = (LINELOCATIONENTRY *)(bufW + cw[5]);
        int ok = 1, names = 0;

        for (i = 0; i < cw[3]; i++)
        {
            WCHAR *wname = (WCHAR *)(bufW + lw[i].dwLocationNameOffset);
            char *aname = (char *)(bufA + la[i].dwLocationNameOffset);
            WCHAR *warea = (WCHAR *)(bufW + lw[i].dwCityCodeOffset);
            char *aarea = (char *)(bufA + la[i].dwCityCodeOffset);
            WCHAR conv[64];

            MultiByteToWideChar(CP_ACP, 0, aname, -1, conv, 64);
            if (lw[i].dwPermanentLocationID != la[i].dwPermanentLocationID || wcscmp(wname, conv) ||
                lw[i].dwLocationNameSize != (wcslen(conv) + 1) * sizeof(WCHAR)) ok = 0;
            MultiByteToWideChar(CP_ACP, 0, aarea, -1, conv, 64);
            if (wcscmp(warea, conv) || lw[i].dwCountryCode != la[i].dwCountryCode || lw[i].dwOptions != la[i].dwOptions) ok = 0;
            if (!wcscmp(wname, L"Home") || !wcscmp(wname, L"Office")) names++;
        }
        check(ok, "every location carries the same values with wide strings");
        check(names == 2, "including Home and Office");
        {
            WCHAR *access = NULL;
            for (i = 0; i < cw[3]; i++) if (!wcscmp((WCHAR *)(bufW + lw[i].dwLocationNameOffset), L"Home")) access = (WCHAR *)(bufW + lw[i].dwLocalAccessCodeOffset);
            check(access && !wcscmp(access, L"9,"), "Home's outside access code is 9,");
        }
    }
    {
        LINECARDENTRY *cA = (LINECARDENTRY *)(bufA + ca[9]), *cW = (LINECARDENTRY *)(bufW + cw[9]);
        WCHAR conv[64];
        int ok = ca[7] > 0;
        for (i = 0; i < cw[7]; i++)
        {
            MultiByteToWideChar(CP_ACP, 0, (char *)(bufA + cA[i].dwCardNameOffset), -1, conv, 64);
            if (wcscmp((WCHAR *)(bufW + cW[i].dwCardNameOffset), conv) || cW[i].dwPermanentCardID != cA[i].dwPermanentCardID) ok = 0;
        }
        check(ok, "and the cards");
    }
    check(cw[2] <= cw[1] && cw[1] > sizeof(LINETRANSLATECAPS), "used size does not exceed the needed one");
    memset(bufW, 0, sizeof(bufW));
    cw[0] = sizeof(LINETRANSLATECAPS);
    IS(capsW(app, ver, bufW), 0, "a buffer with room for the header only is not an error");
    check(cw[1] > cw[0] && cw[2] == sizeof(LINETRANSLATECAPS), "but says what it needs and uses only the header");
    cw[0] = 8;
    IS(capsW(app, ver, bufW), E_STRUCTURETOOSMALL, "a buffer smaller than the header: LINEERR_STRUCTURETOOSMALL");
    IS(capsW(app, ver, NULL), E_INVALPOINTER, "no buffer: LINEERR_INVALPOINTER");

    /* translation needs a device */
    {
        LINETRANSLATEOUTPUT out;
        memset(&out, 0, sizeof(out));
        out.dwTotalSize = sizeof(out);
        IS(xlA(app, 0, ver, "+1 (555) 123-4567", 0, 0, &out), E_BADDEVICEID, "lineTranslateAddressA: no device, LINEERR_BADDEVICEID");
        IS(xlW(app, 0, ver, L"+1 (555) 123-4567", 0, 0, &out), E_BADDEVICEID, "lineTranslateAddressW too");
        IS(xlA((HLINEAPP)0x6666, 0, ver, "123", 0, 0, &out), E_INVALAPPHANDLE, "an application that is not registered: LINEERR_INVALAPPHANDLE");
        IS(xlA(app, 0, ver, NULL, 0, 0, &out), E_INVALPOINTER, "no address: LINEERR_INVALPOINTER");
        IS(xlA(app, 0, ver, "123", 0, 0, NULL), E_INVALPOINTER, "no output: LINEERR_INVALPOINTER");
        out.dwTotalSize = 8;
        IS(xlA(app, 0, ver, "123", 0, 0, &out), E_STRUCTURETOOSMALL, "an output too small: LINEERR_STRUCTURETOOSMALL");
        IS(dlA(app, 0, ver, NULL, "123"), E_BADDEVICEID, "lineTranslateDialogA: LINEERR_BADDEVICEID");
        IS(dlW(app, 0, ver, NULL, L"123"), E_BADDEVICEID, "lineTranslateDialogW too");
        IS(dlA((HLINEAPP)0x6666, 0, ver, NULL, "123"), E_INVALAPPHANDLE, "and with an application that is not registered: LINEERR_INVALAPPHANDLE");
    }
    shut(app);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
