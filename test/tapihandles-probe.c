/* tapi32 calls on handles and providers (patches/sg/2420), run by
 * test/tapihandles-gate.sh. This machine has TAPI and no telephony devices,
 * so no line or call handle exists: every call that takes one fails with the
 * error for an invalid handle of its kind, instead of the made-up success it
 * used to give. Table driven; the argument kinds are h (a handle), d (a
 * DWORD), w (a window), p (a pointer to scratch memory). */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define LINEERR_INVALAPPHANDLE   0x80000014
#define LINEERR_INVALCALLHANDLE  0x80000018
#define LINEERR_INVALCALLSELECT  0x8000001B
#define LINEERR_INVALLINEHANDLE  0x8000002B
#define LINEERR_INVALPARAM       0x80000032
#define LINEERR_INVALPOINTER     0x80000035
#define LINEERR_INVALMEDIAMODE   0x8000002F
#define LINEERR_INVALREQUESTMODE 0x80000038
#define LINEERR_STRUCTURETOOSMALL 0x8000004D
#define LINEERR_INCOMPATIBLEAPIVERSION 0x8000000C
#define LINEERR_OPERATIONFAILED  0x80000048
#define LINECALLSELECT_LINE      1
#define LINECALLSELECT_ADDRESS   2
#define LINECALLSELECT_CALL      4

enum { E_CALL, E_LINE, E_SEL, E_SETUPCONF };

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const struct { const char *name; const char *kinds; int expect; } rows[] =
{
    { "lineAccept", "hpd", E_CALL },
    { "lineAddToConference", "hh", E_CALL },
    { "lineAnswer", "hpd", E_CALL },
    { "lineBlindTransferA", "hpd", E_CALL },
    { "lineCompleteCall", "hpdd", E_CALL },
    { "lineCompleteTransfer", "hhpd", E_CALL },
    { "lineDeallocateCall", "h", E_CALL },
    { "lineDialA", "hpd", E_CALL },
    { "lineDialW", "hpd", E_CALL },
    { "lineDrop", "hpd", E_CALL },
    { "lineGatherDigitsA", "hdpdpdd", E_CALL },
    { "lineGenerateDigitsA", "hdpd", E_CALL },
    { "lineGenerateTone", "hdddp", E_CALL },
    { "lineGetCallInfoA", "hp", E_CALL },
    { "lineGetCallInfoW", "hp", E_CALL },
    { "lineGetCallStatus", "hp", E_CALL },
    { "lineGetConfRelatedCalls", "hp", E_CALL },
    { "lineHandoffA", "hpd", E_CALL },
    { "lineHold", "h", E_CALL },
    { "lineMonitorDigits", "hd", E_CALL },
    { "lineMonitorMedia", "hd", E_CALL },
    { "lineMonitorTones", "hpd", E_CALL },
    { "lineParkA", "hdpp", E_CALL },
    { "linePrepareAddToConferenceA", "hpp", E_CALL },
    { "lineRedirectA", "hpd", E_CALL },
    { "lineReleaseUserUserInfo", "h", E_CALL },
    { "lineRemoveFromConference", "h", E_CALL },
    { "lineSecureCall", "h", E_CALL },
    { "lineSendUserUserInfo", "hpd", E_CALL },
    { "lineSetAppSpecific", "hd", E_CALL },
    { "lineSetCallParams", "hdddp", E_CALL },
    { "lineSetCallPrivilege", "hd", E_CALL },
    { "lineSetMediaMode", "hd", E_CALL },
    { "lineSetupTransferA", "hpp", E_CALL },
    { "lineSwapHold", "hh", E_CALL },
    { "lineUnhold", "h", E_CALL },
    { "lineClose", "h", E_LINE },
    { "lineDevSpecific", "hdhpd", E_LINE },
    { "lineDevSpecificFeature", "hdpd", E_LINE },
    { "lineForwardA", "hddpdpp", E_LINE },
    { "lineGetAddressIDA", "hpdpd", E_LINE },
    { "lineGetAddressStatusA", "hdp", E_LINE },
    { "lineGetLineDevStatusA", "hp", E_LINE },
    { "lineGetNewCalls", "hddp", E_LINE },
    { "lineGetNumRings", "hdp", E_LINE },
    { "lineGetStatusMessages", "hpp", E_LINE },
    { "linePickupA", "hdppp", E_LINE },
    { "lineSetNumRings", "hdd", E_LINE },
    { "lineSetStatusMessages", "hdd", E_LINE },
    { "lineUncompleteCall", "hd", E_LINE },
    { "lineUnparkA", "hdpp", E_LINE },
    { "lineMakeCallA", "hppdp", E_LINE },
    { "lineMakeCallW", "hppdp", E_LINE },
    { "lineGetIDW", "hdhdpp", E_SEL },
    { "lineGetIDA", "hdhdpp", E_SEL },
    { "lineSetMediaControl", "hdhdpdpdpdpd", E_SEL },
    { "lineSetTerminal", "hdhdddd", E_SEL },
    { "lineSetupConferenceA", "hhppdp", E_SETUPCONF },
};

typedef DWORD (WINAPI *f0)(void);
typedef DWORD (WINAPI *f1)(ULONG_PTR);
typedef DWORD (WINAPI *f2)(ULONG_PTR, ULONG_PTR);
typedef DWORD (WINAPI *f3)(ULONG_PTR, ULONG_PTR, ULONG_PTR);
typedef DWORD (WINAPI *f4)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
typedef DWORD (WINAPI *f5)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
typedef DWORD (WINAPI *f6)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
typedef DWORD (WINAPI *f7)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
typedef DWORD (WINAPI *f8)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
typedef DWORD (WINAPI *f9)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
typedef DWORD (WINAPI *f10)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
typedef DWORD (WINAPI *f11)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
typedef DWORD (WINAPI *f12)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);

static DWORD call(void *fn, int n, const ULONG_PTR *a)
{
    switch (n)
    {
    case 0: return ((f0)fn)();
    case 1: return ((f1)fn)(a[0]);
    case 2: return ((f2)fn)(a[0], a[1]);
    case 3: return ((f3)fn)(a[0], a[1], a[2]);
    case 4: return ((f4)fn)(a[0], a[1], a[2], a[3]);
    case 5: return ((f5)fn)(a[0], a[1], a[2], a[3], a[4]);
    case 6: return ((f6)fn)(a[0], a[1], a[2], a[3], a[4], a[5]);
    case 7: return ((f7)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
    case 8: return ((f8)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
    case 9: return ((f9)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8]);
    case 10: return ((f10)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9]);
    case 11: return ((f11)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10]);
    default: return ((f12)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11]);
    }
}

static BYTE scratch[4096];

static DWORD run(HMODULE tapi, const char *name, const char *kinds, int select_index, ULONG_PTR select_value)
{
    void *fn = GetProcAddress(tapi, name);
    ULONG_PTR a[12];
    int i, n = (int)strlen(kinds);

    if (!fn) return 0xdead0001;
    memset(scratch, 0, sizeof(scratch));
    for (i = 0; i < n; i++)
        a[i] = kinds[i] == 'h' ? 0x1234 : kinds[i] == 'd' ? 0x10 : kinds[i] == 'w' ? 0 : (ULONG_PTR)scratch;
    if (select_index >= 0) a[select_index] = select_value;
    return call(fn, n, a);
}

int main(void)
{
    HMODULE tapi = LoadLibraryA("tapi32.dll");
    unsigned i;
    char msg[128];
    DWORD r;

    check(tapi != NULL, "tapi32 loads");
    for (i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
    {
        DWORD want;

        switch (rows[i].expect)
        {
        case E_CALL: want = LINEERR_INVALCALLHANDLE; break;
        case E_LINE: want = LINEERR_INVALLINEHANDLE; break;
        case E_SETUPCONF: want = LINEERR_INVALCALLHANDLE; break;
        default: want = 0; break;
        }
        if (rows[i].expect == E_SEL)
        {
            /* the select argument (the fourth of lineGetID, lineSetMediaControl, lineSetTerminal) says which handle counts */
            int idx = 3;
            r = run(tapi, rows[i].name, rows[i].kinds, idx, LINECALLSELECT_LINE);
            sprintf(msg, "%s with a line select: invalid line handle (%08lx)", rows[i].name, r);
            check(r == LINEERR_INVALLINEHANDLE, msg);
            r = run(tapi, rows[i].name, rows[i].kinds, idx, LINECALLSELECT_ADDRESS);
            sprintf(msg, "%s with an address select: invalid line handle (%08lx)", rows[i].name, r);
            check(r == LINEERR_INVALLINEHANDLE, msg);
            r = run(tapi, rows[i].name, rows[i].kinds, idx, LINECALLSELECT_CALL);
            sprintf(msg, "%s with a call select: invalid call handle (%08lx)", rows[i].name, r);
            check(r == LINEERR_INVALCALLHANDLE, msg);
            r = run(tapi, rows[i].name, rows[i].kinds, idx, 0x40);
            sprintf(msg, "%s with a bad select: invalid call select (%08lx)", rows[i].name, r);
            check(r == LINEERR_INVALCALLSELECT, msg);
            continue;
        }
        r = run(tapi, rows[i].name, rows[i].kinds, -1, 0);
        sprintf(msg, "%s on a handle that is not one: %08lx", rows[i].name, want);
        check(r == want, msg);
    }
    /* lineSetupConference names a call or a line */
    {
        ULONG_PTR a[6] = { 0, 0x1234, (ULONG_PTR)scratch, (ULONG_PTR)scratch + 16, 3, 0 };
        void *fn = GetProcAddress(tapi, "lineSetupConferenceA");
        r = call(fn, 6, a);
        check(r == LINEERR_INVALLINEHANDLE, "lineSetupConference with a line only: invalid line handle");
        a[0] = 0x1234; a[1] = 0;
        r = call(fn, 6, a);
        check(r == LINEERR_INVALCALLHANDLE, "with a call: invalid call handle");
        a[0] = 0; a[1] = 0;
        r = call(fn, 6, a);
        check(r == LINEERR_INVALPARAM, "with neither: invalid parameter");
    }
    /* a handle is checked before what it is asked for: lineMakeCall clears its output */
    {
        ULONG_PTR out = 0xffff;
        ULONG_PTR a[5] = { 0x1234, (ULONG_PTR)&out, (ULONG_PTR)"123", 0, 0 };
        void *fn = GetProcAddress(tapi, "lineMakeCallA");
        r = call(fn, 5, a);
        check(r == LINEERR_INVALLINEHANDLE && (DWORD)out == 0, "lineMakeCall: invalid line handle, and no call handle given back");
    }

    /* providers: none installed, none can be added */
    {
        typedef DWORD (WINAPI *prov_list_t)(DWORD, void *);
        typedef DWORD (WINAPI *add_prov_t)(const char *, HWND, DWORD *);
        typedef DWORD (WINAPI *rm_prov_t)(DWORD, HWND);
        typedef DWORD (WINAPI *cfg_prov_t)(HWND, DWORD);
        prov_list_t list = (prov_list_t)GetProcAddress(tapi, "lineGetProviderListA");
        prov_list_t listw = (prov_list_t)GetProcAddress(tapi, "lineGetProviderListW");
        add_prov_t add = (add_prov_t)GetProcAddress(tapi, "lineAddProviderA");
        rm_prov_t rm = (rm_prov_t)GetProcAddress(tapi, "lineRemoveProvider");
        cfg_prov_t cfg = (cfg_prov_t)GetProcAddress(tapi, "lineConfigProvider");
        DWORD pl[6], id = 77;

        memset(pl, 0, sizeof(pl));
        pl[0] = sizeof(pl);
        r = list(0x00020002, pl);
        check(r == 0 && pl[1] == sizeof(pl) && pl[2] == sizeof(pl) && pl[3] == 0 && pl[4] == 0, "lineGetProviderList: no providers, the size it needs");
        memset(pl, 0xaa, sizeof(pl)); pl[0] = sizeof(pl);
        r = listw(0x00030001, pl);
        check(r == 0 && pl[3] == 0, "the wide form too");
        pl[0] = 8;
        check(list(0x00020002, pl) == LINEERR_STRUCTURETOOSMALL, "a list too small: LINEERR_STRUCTURETOOSMALL");
        pl[0] = sizeof(pl);
        check(list(0x00010002, pl) == LINEERR_INCOMPATIBLEAPIVERSION, "an API version too old: LINEERR_INCOMPATIBLEAPIVERSION");
        check(list(0x00040000, pl) == LINEERR_INCOMPATIBLEAPIVERSION, "one too new: LINEERR_INCOMPATIBLEAPIVERSION");
        check(list(0x00020002, NULL) == LINEERR_INVALPOINTER, "no list: LINEERR_INVALPOINTER");
        check(add("sgprov.tsp", 0, NULL) == LINEERR_INVALPOINTER, "lineAddProvider without an id pointer: LINEERR_INVALPOINTER");
        check(add("", 0, &id) == LINEERR_INVALPARAM, "with an empty name: LINEERR_INVALPARAM");
        check(add("sgprov.tsp", 0, &id) == LINEERR_OPERATIONFAILED && id == 0, "and it cannot be added: LINEERR_OPERATIONFAILED");
        check(rm(5, 0) == LINEERR_INVALPARAM, "lineRemoveProvider of one that is not there: LINEERR_INVALPARAM");
        check(cfg(0, 5) == LINEERR_INVALPARAM, "lineConfigProvider of one that is not there: LINEERR_INVALPARAM");
    }
    /* application priorities, kept for the process */
    {
        typedef DWORD (WINAPI *get_prio_t)(const char *, DWORD, void *, DWORD, void *, DWORD *);
        typedef DWORD (WINAPI *set_prio_t)(const char *, DWORD, void *, DWORD, const char *, DWORD);
        get_prio_t get = (get_prio_t)GetProcAddress(tapi, "lineGetAppPriorityA");
        set_prio_t set = (set_prio_t)GetProcAddress(tapi, "lineSetAppPriorityA");
        DWORD prio = 99, ext[8];

        check(get("sgapp.exe", 0x4, NULL, 0, NULL, &prio) == 0 && prio == 0, "an application not on the list has priority 0");
        check(set("sgapp.exe", 0x4, NULL, 0, NULL, 1) == 0, "lineSetAppPriority puts it first");
        check(get("SGAPP.EXE", 0x4, NULL, 0, NULL, &prio) == 0 && prio == 1, "and lineGetAppPriority says so (names compare without case)");
        check(get("sgapp.exe", 0x8, NULL, 0, NULL, &prio) == 0 && prio == 0, "for that media mode only");
        check(set("sgapp.exe", 0, NULL, 2, NULL, 1) == 0 && get("sgapp.exe", 0, NULL, 2, NULL, &prio) == 0 && prio == 1, "a request mode works the same way");
        check(set("sgapp.exe", 0x4, NULL, 0, NULL, 0) == 0 && get("sgapp.exe", 0x4, NULL, 0, NULL, &prio) == 0 && prio == 0, "priority 0 takes it off the list");
        check(get(NULL, 0x4, NULL, 0, NULL, &prio) == LINEERR_INVALPOINTER, "no name: LINEERR_INVALPOINTER");
        check(get("sgapp.exe", 0x4, NULL, 0, NULL, NULL) == LINEERR_INVALPOINTER, "no priority pointer: LINEERR_INVALPOINTER");
        check(get("sgapp.exe", 0x6, NULL, 0, NULL, &prio) == LINEERR_INVALMEDIAMODE, "two media modes: LINEERR_INVALMEDIAMODE");
        check(get("sgapp.exe", 0, NULL, 3, NULL, &prio) == LINEERR_INVALREQUESTMODE, "a bad request mode: LINEERR_INVALREQUESTMODE");
        check(get("sgapp.exe", 0, NULL, 0, NULL, &prio) == LINEERR_INVALPARAM, "neither mode: LINEERR_INVALPARAM");
        check(set("sgapp.exe", 0x4, NULL, 0, NULL, 5) == LINEERR_INVALPARAM, "a priority of 5: LINEERR_INVALPARAM");
        memset(ext, 0, sizeof(ext)); ext[0] = 8;
        check(get("sgapp.exe", 0x4, NULL, 0, ext, &prio) == LINEERR_STRUCTURETOOSMALL, "an extension name too small: LINEERR_STRUCTURETOOSMALL");
        ext[0] = sizeof(ext);
        check(get("sgapp.exe", 0x4, NULL, 0, ext, &prio) == 0 && ext[4] == 0, "an extension name is given empty");
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
