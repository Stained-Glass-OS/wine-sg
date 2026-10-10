/* tapi32 phone services (patches/sg/2422), run by test/tapiphone-gate.sh. This
 * machine has TAPI and no phone devices: applications can initialise and shut
 * down, the device count is zero, and every call that names a phone handle or a
 * device fails the way TAPI fails for one that does not exist. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define PHONEERR_BADDEVICEID            0x90000002
#define PHONEERR_INCOMPATIBLEAPIVERSION 0x90000003
#define PHONEERR_INVALAPPHANDLE         0x90000007
#define PHONEERR_INVALPARAM             0x90000012
#define PHONEERR_INVALPHONEHANDLE       0x90000013
#define PHONEERR_INVALPOINTER           0x90000015
#define PHONEERR_OPERATIONFAILED        0x9000001C
#define PHONEERR_STRUCTURETOOSMALL      0x90000021
#define USEHIDDENWINDOW 1
#define USEEVENT        2
#define USECOMPLETIONPORT 3

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define IS(call, err, what) do { DWORD r_ = (DWORD)(call); if (r_ != (DWORD)(err)) printf("      got %08lx want %08lx\n", (unsigned long)r_, (unsigned long)(err)); check(r_ == (DWORD)(err), what); } while (0)

static const struct { const char *name; const char *kinds; } rows[] =
{
    { "phoneClose", "h" },
    { "phoneDevSpecific", "hpd" },
    { "phoneGetButtonInfoA", "hdp" },
    { "phoneGetData", "hdpd" },
    { "phoneGetDisplay", "hp" },
    { "phoneGetGain", "hdp" },
    { "phoneGetHookSwitch", "hp" },
    { "phoneGetIDA", "hpp" },
    { "phoneGetLamp", "hdp" },
    { "phoneGetRing", "hpp" },
    { "phoneGetStatusA", "hp" },
    { "phoneGetStatusMessages", "hppp" },
    { "phoneGetVolume", "hdp" },
    { "phoneSetButtonInfoA", "hdp" },
    { "phoneSetData", "hdpd" },
    { "phoneSetDisplay", "hddpd" },
    { "phoneSetGain", "hdd" },
    { "phoneSetHookSwitch", "hdd" },
    { "phoneSetLamp", "hdd" },
    { "phoneSetRing", "hdd" },
    { "phoneSetStatusMessages", "hddd" },
    { "phoneSetVolume", "hdd" },
};

typedef DWORD (WINAPI *f1)(ULONG_PTR);
typedef DWORD (WINAPI *f2)(ULONG_PTR, ULONG_PTR);
typedef DWORD (WINAPI *f3)(ULONG_PTR, ULONG_PTR, ULONG_PTR);
typedef DWORD (WINAPI *f4)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
typedef DWORD (WINAPI *f5)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
static DWORD call(void *fn, int n, const ULONG_PTR *a)
{
    switch (n)
    {
    case 1: return ((f1)fn)(a[0]);
    case 2: return ((f2)fn)(a[0], a[1]);
    case 3: return ((f3)fn)(a[0], a[1], a[2]);
    case 4: return ((f4)fn)(a[0], a[1], a[2], a[3]);
    default: return ((f5)fn)(a[0], a[1], a[2], a[3], a[4]);
    }
}

static BYTE scratch[1024];
static void CALLBACK cb(DWORD dev, DWORD msg, DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR p2, DWORD_PTR p3) {}

typedef DWORD (WINAPI *init_t)(DWORD *, HINSTANCE, void *, const char *, DWORD *);
typedef DWORD (WINAPI *initexa_t)(DWORD *, HINSTANCE, void *, const char *, DWORD *, DWORD *, void *);
typedef DWORD (WINAPI *initexw_t)(DWORD *, HINSTANCE, void *, const WCHAR *, DWORD *, DWORD *, void *);
typedef DWORD (WINAPI *shut_t)(ULONG_PTR);
typedef DWORD (WINAPI *getmsg_t)(ULONG_PTR, void *, DWORD);
typedef DWORD (WINAPI *neg_t)(ULONG_PTR, DWORD, DWORD, DWORD, DWORD *, void *);
typedef DWORD (WINAPI *negext_t)(ULONG_PTR, DWORD, DWORD, DWORD, DWORD, DWORD *);
typedef DWORD (WINAPI *caps_t)(ULONG_PTR, DWORD, DWORD, DWORD, void *);
typedef DWORD (WINAPI *open_t)(ULONG_PTR, DWORD, void *, DWORD, DWORD, DWORD, DWORD);
typedef DWORD (WINAPI *cfg_t)(DWORD, HWND, const char *);
typedef DWORD (WINAPI *icon_t)(DWORD, const char *, HICON *);

int main(void)
{
    HMODULE tapi = LoadLibraryA("tapi32.dll");
    init_t init = (init_t)GetProcAddress(tapi, "phoneInitialize");
    initexa_t exa = (initexa_t)GetProcAddress(tapi, "phoneInitializeExA");
    initexw_t exw = (initexw_t)GetProcAddress(tapi, "phoneInitializeExW");
    shut_t shut = (shut_t)GetProcAddress(tapi, "phoneShutdown");
    getmsg_t getmsg = (getmsg_t)GetProcAddress(tapi, "phoneGetMessage");
    neg_t neg = (neg_t)GetProcAddress(tapi, "phoneNegotiateAPIVersion");
    negext_t negext = (negext_t)GetProcAddress(tapi, "phoneNegotiateExtVersion");
    caps_t caps = (caps_t)GetProcAddress(tapi, "phoneGetDevCapsA");
    open_t open = (open_t)GetProcAddress(tapi, "phoneOpen");
    cfg_t cfg = (cfg_t)GetProcAddress(tapi, "phoneConfigDialogA");
    icon_t icon = (icon_t)GetProcAddress(tapi, "phoneGetIconA");
    DWORD app = 0, ndev = 99, ver = 0x00030001, app2 = 0, i, ext[8];
    unsigned k;
    char msg[160];

    check(init && exa && exw && shut && getmsg && neg && negext && caps && open && cfg && icon, "the phone calls are exported");

    /* every call that takes a phone handle */
    for (k = 0; k < sizeof(rows) / sizeof(rows[0]); k++)
    {
        void *fn = GetProcAddress(tapi, rows[k].name);
        ULONG_PTR a[5];
        int n = (int)strlen(rows[k].kinds), j;
        DWORD r;

        for (j = 0; j < n; j++) a[j] = rows[k].kinds[j] == 'h' ? 0x1234 : rows[k].kinds[j] == 'd' ? 0x10 : (ULONG_PTR)scratch;
        r = fn ? call(fn, n, a) : 0xdead0001;
        sprintf(msg, "%s on a handle that is not one: PHONEERR_INVALPHONEHANDLE (%08lx)", rows[k].name, r);
        check(r == PHONEERR_INVALPHONEHANDLE, msg);
    }

    /* applications */
    IS(init(&app, GetModuleHandleA(NULL), cb, "sgphone", &ndev), 0, "phoneInitialize");
    check(app != 0 && ndev == 0, "gives a handle and no devices");
    IS(init(NULL, GetModuleHandleA(NULL), cb, "sgphone", &ndev), PHONEERR_INVALPOINTER, "without a handle pointer: PHONEERR_INVALPOINTER");
    IS(init(&app2, GetModuleHandleA(NULL), cb, "sgphone", NULL), PHONEERR_INVALPOINTER, "without a count pointer: PHONEERR_INVALPOINTER");
    IS(init(&app2, GetModuleHandleA(NULL), NULL, "sgphone", &ndev), PHONEERR_INVALPOINTER, "without a callback: PHONEERR_INVALPOINTER");
    IS(shut(app), 0, "phoneShutdown");
    IS(shut(app), PHONEERR_INVALAPPHANDLE, "twice: PHONEERR_INVALAPPHANDLE");
    IS(shut(0x77777), PHONEERR_INVALAPPHANDLE, "a handle that is not one: PHONEERR_INVALAPPHANDLE");

    ver = 0x00030001; ndev = 99;
    IS(exa(&app, GetModuleHandleA(NULL), cb, "sgphone", &ndev, &ver, NULL), 0, "phoneInitializeExA");
    check(ndev == 0 && ver == 0x00030001, "no devices, the version as asked");
    ver = 0x00040000;
    {
        DWORD a2 = 0;
        IS(exw(&a2, GetModuleHandleA(NULL), cb, L"sgphone", &ndev, &ver, NULL), 0, "phoneInitializeExW with a version that is too new");
        check(ver == 0x00030001, "which is lowered to the highest");
        shut(a2);
    }
    ver = 0x00010002;
    IS(exa(&app2, GetModuleHandleA(NULL), cb, "sgphone", &ndev, &ver, NULL), PHONEERR_INCOMPATIBLEAPIVERSION, "a version that is too old: PHONEERR_INCOMPATIBLEAPIVERSION");
    ver = 0x00030001;
    IS(exa(&app2, GetModuleHandleA(NULL), cb, "sgphone", &ndev, NULL, NULL), PHONEERR_INVALPOINTER, "without a version pointer: PHONEERR_INVALPOINTER");
    {
        DWORD ex[7];
        DWORD ev = 0;

        memset(ex, 0, sizeof(ex));
        ex[0] = 8;
        IS(exa(&app2, GetModuleHandleA(NULL), cb, "sgphone", &ndev, &ver, ex), PHONEERR_STRUCTURETOOSMALL, "initialisation parameters too small: PHONEERR_STRUCTURETOOSMALL");
        ex[0] = sizeof(void *) == 8 ? 28 : 24; ex[3] = 9;
        IS(exa(&app2, GetModuleHandleA(NULL), cb, "sgphone", &ndev, &ver, ex), PHONEERR_INVALPARAM, "an option that does not exist: PHONEERR_INVALPARAM");
        ex[3] = USEEVENT;
        IS(exa(&ev, GetModuleHandleA(NULL), NULL, "sgphone", &ndev, &ver, ex), 0, "the event option needs no callback");
        check(ex[4] != 0 || ex[5] != 0, "and returns an event");
        IS(getmsg(ev, scratch, 50), PHONEERR_OPERATIONFAILED, "phoneGetMessage waits and times out: PHONEERR_OPERATIONFAILED");
        IS(getmsg(ev, NULL, 50), PHONEERR_INVALPOINTER, "without a message pointer: PHONEERR_INVALPOINTER");
        IS(getmsg(app, scratch, 50), PHONEERR_INVALAPPHANDLE, "an application using a hidden window cannot get messages: PHONEERR_INVALAPPHANDLE");
        IS(getmsg(0x4444, scratch, 50), PHONEERR_INVALAPPHANDLE, "a handle that is not one: PHONEERR_INVALAPPHANDLE");
        shut(ev);
    }

    /* devices */
    IS(neg(app, 0, 0x00010003, 0x00030001, &i, ext), PHONEERR_BADDEVICEID, "phoneNegotiateAPIVersion: no device, PHONEERR_BADDEVICEID");
    IS(neg(app, 0, 0x00010003, 0x00030001, NULL, ext), PHONEERR_INVALPOINTER, "without a version pointer: PHONEERR_INVALPOINTER");
    IS(neg(0x4444, 0, 0x00010003, 0x00030001, &i, ext), PHONEERR_INVALAPPHANDLE, "an application that is not one: PHONEERR_INVALAPPHANDLE");
    IS(negext(app, 0, 0x00030001, 0, 0, &i), PHONEERR_BADDEVICEID, "phoneNegotiateExtVersion: PHONEERR_BADDEVICEID");
    IS(negext(0x4444, 0, 0x00030001, 0, 0, &i), PHONEERR_INVALAPPHANDLE, "and with a bad application: PHONEERR_INVALAPPHANDLE");
    IS(caps(app, 0, 0x00030001, 0, scratch), PHONEERR_BADDEVICEID, "phoneGetDevCaps: PHONEERR_BADDEVICEID");
    IS(caps(app, 0, 0x00030001, 0, NULL), PHONEERR_INVALPOINTER, "without a structure: PHONEERR_INVALPOINTER");
    IS(caps(0x4444, 0, 0x00030001, 0, scratch), PHONEERR_INVALAPPHANDLE, "and with a bad application: PHONEERR_INVALAPPHANDLE");
    {
        ULONG_PTR ph = 0xffff;
        IS(open(app, 0, &ph, 0x00030001, 0, 0, 1), PHONEERR_BADDEVICEID, "phoneOpen: PHONEERR_BADDEVICEID");
        check(ph == 0, "and no phone handle is given back");
        IS(open(app, 0, NULL, 0x00030001, 0, 0, 1), PHONEERR_INVALPOINTER, "without a handle pointer: PHONEERR_INVALPOINTER");
        IS(open(0x4444, 0, &ph, 0x00030001, 0, 0, 1), PHONEERR_INVALAPPHANDLE, "and with a bad application: PHONEERR_INVALAPPHANDLE");
    }
    IS(cfg(0, NULL, "tapi/phone"), PHONEERR_BADDEVICEID, "phoneConfigDialog: PHONEERR_BADDEVICEID");
    {
        HICON ic = (HICON)1;
        IS(icon(0, "tapi/phone", &ic), PHONEERR_BADDEVICEID, "phoneGetIcon: PHONEERR_BADDEVICEID");
        IS(icon(0, "tapi/phone", NULL), PHONEERR_INVALPOINTER, "without an icon pointer: PHONEERR_INVALPOINTER");
    }
    shut(app);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
