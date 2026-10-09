/* tapi32 line applications and devices (patches/sg/2409), run by
 * test/tapiline-gate.sh. These calls were stubs that claimed success; the
 * model is a machine with TAPI and no telephony devices. */
#include <windows.h>
#include <tapi.h>
#include <stdio.h>
#include <string.h>

#ifndef LINEERR_INVALREQUESTMODE
#define LINEERR_INVALREQUESTMODE 0x80000038
#endif
#define E_NOTREG 0x80000047
#define E_NOREQ  0x80000045

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define IS(call, err, what) do { LONG r_ = (LONG)(call); if (r_ != (LONG)(err)) printf("      got %08lx want %08lx\n", (unsigned long)r_, (unsigned long)(err)); check(r_ == (LONG)(err), what); } while (0)

static HMODULE mod;
static FARPROC fn(const char *name)
{
    FARPROC p = GetProcAddress(mod, name);
    if (!p) { printf("FAIL  tapi32 does not export %s\n", name); failures++; }
    return p;
}

typedef LONG (WINAPI *init_fn)(HLINEAPP *, HINSTANCE, LINECALLBACK, const char *, DWORD *);
typedef LONG (WINAPI *initexa_fn)(HLINEAPP *, HINSTANCE, LINECALLBACK, const char *, DWORD *, DWORD *, LINEINITIALIZEEXPARAMS *);
typedef LONG (WINAPI *initexw_fn)(HLINEAPP *, HINSTANCE, LINECALLBACK, const WCHAR *, DWORD *, DWORD *, LINEINITIALIZEEXPARAMS *);
typedef LONG (WINAPI *shut_fn)(HLINEAPP);
typedef LONG (WINAPI *getmsg_fn)(HLINEAPP, LINEMESSAGE *, DWORD);
typedef LONG (WINAPI *negapi_fn)(HLINEAPP, DWORD, DWORD, DWORD, DWORD *, LINEEXTENSIONID *);
typedef LONG (WINAPI *negext_fn)(HLINEAPP, DWORD, DWORD, DWORD, DWORD, DWORD *);
typedef LONG (WINAPI *devcaps_fn)(HLINEAPP, DWORD, DWORD, DWORD, LINEDEVCAPS *);
typedef LONG (WINAPI *addrcaps_fn)(HLINEAPP, DWORD, DWORD, DWORD, DWORD, LINEADDRESSCAPS *);
typedef LONG (WINAPI *open_fn)(HLINEAPP, DWORD, HLINE *, DWORD, DWORD, DWORD, DWORD, DWORD, LINECALLPARAMS *);
typedef LONG (WINAPI *reg_fn)(HLINEAPP, DWORD, DWORD, DWORD);
typedef LONG (WINAPI *getreq_fn)(HLINEAPP, DWORD, void *);
typedef LONG (WINAPI *toll_fn)(HLINEAPP, DWORD, const char *, DWORD);
typedef LONG (WINAPI *cfgd_fn)(DWORD, HWND, const char *);
typedef LONG (WINAPI *cfgdw_fn)(DWORD, HWND, const WCHAR *);
typedef LONG (WINAPI *cfge_fn)(DWORD, HWND, const char *, void *, DWORD, VARSTRING *);
typedef LONG (WINAPI *gdc_fn)(DWORD, VARSTRING *, const char *);
typedef LONG (WINAPI *gdcw_fn)(DWORD, VARSTRING *, const WCHAR *);
typedef LONG (WINAPI *sdc_fn)(DWORD, void *, DWORD, const char *);
typedef LONG (WINAPI *sdcw_fn)(DWORD, void *, DWORD, const WCHAR *);
typedef LONG (WINAPI *icon_fn)(DWORD, const char *, HICON *);
typedef LONG (WINAPI *iconw_fn)(DWORD, const WCHAR *, HICON *);

static void CALLBACK cb(DWORD dev, DWORD msg, DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR p2, DWORD_PTR p3) {}

int main(void)
{
    init_fn init; initexa_fn exa; initexw_fn exw; shut_fn shut; getmsg_fn getmsg;
    negapi_fn negapi; negext_fn negext; devcaps_fn dcA, dcW; addrcaps_fn acaps;
    open_fn openA, openW; reg_fn regrec; getreq_fn getreq; toll_fn toll;
    cfgd_fn cfgA; cfgdw_fn cfgW; cfge_fn cfge; gdc_fn gdcA; gdcw_fn gdcW;
    sdc_fn sdcA; sdcw_fn sdcW; icon_fn iconA; iconw_fn iconW;
    HLINEAPP app = 0, app2 = 0, appev = 0, appcp = 0;
    HINSTANCE inst = GetModuleHandleA(NULL);
    DWORD ndev, ver, ret;
    LINEINITIALIZEEXPARAMS ex;
    LINEMESSAGE msg;
    LINEEXTENSIONID extid;
    DWORD dw;
    HLINE line;
    HICON icon;
    char buf[64];
    int i;

    mod = LoadLibraryA("tapi32.dll");
    if (!mod) { puts("FAIL  tapi32 not loaded"); puts("RESULT: FAIL"); return 1; }
    init = (init_fn)fn("lineInitialize"); exa = (initexa_fn)fn("lineInitializeExA");
    exw = (initexw_fn)fn("lineInitializeExW"); shut = (shut_fn)fn("lineShutdown");
    getmsg = (getmsg_fn)fn("lineGetMessage"); negapi = (negapi_fn)fn("lineNegotiateAPIVersion");
    negext = (negext_fn)fn("lineNegotiateExtVersion"); dcA = (devcaps_fn)fn("lineGetDevCapsA");
    dcW = (devcaps_fn)fn("lineGetDevCapsW"); acaps = (addrcaps_fn)fn("lineGetAddressCapsA");
    openA = (open_fn)fn("lineOpenA"); openW = (open_fn)fn("lineOpenW");
    regrec = (reg_fn)fn("lineRegisterRequestRecipient"); getreq = (getreq_fn)fn("lineGetRequestA");
    toll = (toll_fn)fn("lineSetTollListA"); cfgA = (cfgd_fn)fn("lineConfigDialogA");
    cfgW = (cfgdw_fn)fn("lineConfigDialogW"); cfge = (cfge_fn)fn("lineConfigDialogEditA");
    gdcA = (gdc_fn)fn("lineGetDevConfigA"); gdcW = (gdcw_fn)fn("lineGetDevConfigW");
    sdcA = (sdc_fn)fn("lineSetDevConfigA"); sdcW = (sdcw_fn)fn("lineSetDevConfigW");
    iconA = (icon_fn)fn("lineGetIconA"); iconW = (iconw_fn)fn("lineGetIconW");
    if (failures) { puts("RESULT: FAIL"); return 1; }

    /* lineInitialize */
    IS(init(0, inst, cb, "probe", &ndev), LINEERR_INVALPOINTER, "lineInitialize NULL app");
    IS(init(&app, inst, cb, "probe", NULL), LINEERR_INVALPOINTER, "lineInitialize NULL device count");
    IS(init(&app, inst, NULL, "probe", &ndev), LINEERR_INVALPOINTER, "lineInitialize NULL callback");
    ndev = 99; app = 0;
    IS(init(&app, inst, cb, "probe", &ndev), 0, "lineInitialize succeeds");
    check(app != 0 && ndev == 0, "lineInitialize: a handle and zero devices");
    app2 = 0;
    IS(init(&app2, inst, cb, NULL, &ndev), 0, "a second application");
    check(app2 != 0 && app2 != app, "the handles differ");

    /* lineInitializeEx */
    ver = 0x00020000; ndev = 99;
    IS(exa(&appcp, inst, cb, "probe", &ndev, &ver, NULL), 0, "lineInitializeExA without params");
    check(ver == 0x00020000 && ndev == 0, "version below the maximum is kept");
    IS(shut(appcp), 0, "shutdown of the Ex application");
    ver = 0x00100000;
    IS(exa(&appcp, inst, cb, "probe", &ndev, &ver, NULL), 0, "lineInitializeExA with a huge version");
    check(ver == 0x00030001, "version is clamped to the highest supported");
    shut(appcp);
    ver = 0x00010002;
    IS(exa(&appcp, inst, cb, "probe", &ndev, &ver, NULL), LINEERR_INCOMPATIBLEAPIVERSION, "version too low");
    ver = 0x00020000;
    IS(exa(&appcp, inst, cb, "probe", &ndev, NULL, NULL), LINEERR_INVALPOINTER, "lineInitializeExA NULL version");
    IS(exa(&appcp, inst, NULL, "probe", &ndev, &ver, NULL), LINEERR_INVALPOINTER, "hidden window needs a callback");
    memset(&ex, 0, sizeof(ex));
    ex.dwTotalSize = sizeof(ex) - 4; ex.dwOptions = LINEINITIALIZEEXOPTION_USEEVENT;
    IS(exa(&appev, inst, NULL, "probe", &ndev, &ver, &ex), LINEERR_STRUCTURETOOSMALL, "params too small");
    ex.dwTotalSize = sizeof(ex); ex.dwOptions = 0;
    IS(exa(&appev, inst, NULL, "probe", &ndev, &ver, &ex), LINEERR_INVALPARAM, "option 0 is invalid");
    ex.dwOptions = 4;
    IS(exa(&appev, inst, cb, "probe", &ndev, &ver, &ex), LINEERR_INVALPARAM, "option 4 is invalid");
    ex.dwOptions = LINEINITIALIZEEXOPTION_USEEVENT;
    ex.Handles.hEvent = NULL;
    IS(exa(&appev, inst, NULL, "probe", &ndev, &ver, &ex), 0, "event application needs no callback");
    check(ex.Handles.hEvent != NULL && ex.dwUsedSize == sizeof(ex) && ex.dwNeededSize == sizeof(ex),
          "the event handle and sizes are returned");
    check(WaitForSingleObject(ex.Handles.hEvent, 0) == WAIT_TIMEOUT, "no message is pending");
    memset(&ex, 0, sizeof(ex));
    ex.dwTotalSize = sizeof(ex); ex.dwOptions = LINEINITIALIZEEXOPTION_USEHIDDENWINDOW;
    appcp = 0;
    IS(exa(&appcp, inst, cb, "probe", &ndev, &ver, &ex), 0, "explicit hidden window option");
    check(ex.Handles.hEvent == NULL, "no event for the hidden window option");
    shut(appcp);
    {
        WCHAR name[] = { 'p', 'r', 'o', 'b', 'e', 0 };
        HLINEAPP appw = 0;
        ver = 0x00020000; ndev = 77;
        IS(exw(&appw, inst, cb, name, &ndev, &ver, NULL), 0, "lineInitializeExW");
        check(appw != 0 && ndev == 0, "lineInitializeExW: handle and zero devices");
        IS(exw(0, inst, cb, name, &ndev, &ver, NULL), LINEERR_INVALPOINTER, "lineInitializeExW NULL app");
        IS(shut(appw), 0, "shutdown of the W application");
    }

    /* lineShutdown */
    IS(shut(0), LINEERR_INVALAPPHANDLE, "shutdown of NULL");
    IS(shut(0x1234), LINEERR_INVALAPPHANDLE, "shutdown of a made-up handle");
    IS(shut(app2), 0, "shutdown");
    IS(shut(app2), LINEERR_INVALAPPHANDLE, "second shutdown fails");
    IS(negapi(app2, 0, 0x10003, 0x20000, &ver, &extid), LINEERR_INVALAPPHANDLE, "a shut down handle is invalid");

    /* lineGetMessage */
    memset(&msg, 0, sizeof(msg));
    IS(getmsg(0, &msg, 0), LINEERR_INVALAPPHANDLE, "lineGetMessage bad handle");
    IS(getmsg(app, &msg, 0), LINEERR_INVALAPPHANDLE, "lineGetMessage on a hidden window application");
    {
        /* appev was created with the event option; ex was reused so re-create */
        HLINEAPP ae = 0;
        LINEINITIALIZEEXPARAMS e2;
        DWORD t0;
        memset(&e2, 0, sizeof(e2));
        e2.dwTotalSize = sizeof(e2); e2.dwOptions = LINEINITIALIZEEXOPTION_USEEVENT;
        ver = 0x00020000;
        exa(&ae, inst, NULL, "probe", &ndev, &ver, &e2);
        IS(getmsg(ae, NULL, 0), LINEERR_INVALPOINTER, "lineGetMessage NULL message");
        IS(getmsg(ae, &msg, 0), LINEERR_OPERATIONFAILED, "lineGetMessage times out at once with no message");
        t0 = GetTickCount();
        IS(getmsg(ae, &msg, 150), LINEERR_OPERATIONFAILED, "lineGetMessage waits then times out");
        check(GetTickCount() - t0 >= 120, "the timeout was honoured");
        IS(getmsg(ae, &msg, INFINITE), LINEERR_OPERATIONFAILED, "lineGetMessage INFINITE does not hang");
        IS(shut(ae), 0, "shutdown of the event application");
    }

    /* devices */
    IS(negapi(0, 0, 0x10003, 0x20000, &ver, &extid), LINEERR_INVALAPPHANDLE, "negotiate bad app");
    IS(negapi(app, 0, 0x10003, 0x20000, NULL, &extid), LINEERR_INVALPOINTER, "negotiate NULL version");
    IS(negapi(app, 0, 0x10003, 0x20000, &ver, NULL), LINEERR_INVALPOINTER, "negotiate NULL extension id");
    IS(negapi(app, 0, 0x10003, 0x20000, &ver, &extid), LINEERR_BADDEVICEID, "negotiate device 0");
    IS(negapi(app, 7, 0x10003, 0x20000, &ver, &extid), LINEERR_BADDEVICEID, "negotiate device 7");
    IS(negext(0, 0, 0x20000, 0, 0, &dw), LINEERR_INVALAPPHANDLE, "negotiate ext bad app");
    IS(negext(app, 0, 0x20000, 0, 0, NULL), LINEERR_INVALPOINTER, "negotiate ext NULL");
    IS(negext(app, 0, 0x20000, 0, 0, &dw), LINEERR_BADDEVICEID, "negotiate ext device 0");
    {
        LINEDEVCAPS caps; LINEADDRESSCAPS ac;
        memset(&caps, 0, sizeof(caps)); caps.dwTotalSize = sizeof(caps);
        memset(&ac, 0, sizeof(ac)); ac.dwTotalSize = sizeof(ac);
        IS(dcA(0, 0, 0x20000, 0, &caps), LINEERR_INVALAPPHANDLE, "devcapsA bad app");
        IS(dcA(app, 0, 0x20000, 0, NULL), LINEERR_INVALPOINTER, "devcapsA NULL caps");
        IS(dcA(app, 0, 0x20000, 0, &caps), LINEERR_BADDEVICEID, "devcapsA device 0");
        IS(dcW(0, 0, 0x20000, 0, &caps), LINEERR_INVALAPPHANDLE, "devcapsW bad app");
        IS(dcW(app, 0, 0x20000, 0, NULL), LINEERR_INVALPOINTER, "devcapsW NULL caps");
        IS(dcW(app, 3, 0x20000, 0, &caps), LINEERR_BADDEVICEID, "devcapsW device 3");
        IS(acaps(0, 0, 0, 0x20000, 0, &ac), LINEERR_INVALAPPHANDLE, "addrcaps bad app");
        IS(acaps(app, 0, 0, 0x20000, 0, NULL), LINEERR_INVALPOINTER, "addrcaps NULL caps");
        IS(acaps(app, 0, 0, 0x20000, 0, &ac), LINEERR_BADDEVICEID, "addrcaps device 0");
    }
    line = 1;
    IS(openA(0, 0, &line, 0x20000, 0, 0, 1, 0, NULL), LINEERR_INVALAPPHANDLE, "openA bad app");
    IS(openA(app, 0, NULL, 0x20000, 0, 0, 1, 0, NULL), LINEERR_INVALPOINTER, "openA NULL line");
    IS(openA(app, 0, &line, 0x20000, 0, 0, 1, 0, NULL), LINEERR_BADDEVICEID, "openA device 0");
    IS(openA(app, 0xffffffff, &line, 0x20000, 0, 0, 1, 0, NULL), LINEERR_LINEMAPPERFAILED, "openA line mapper");
    IS(openW(0, 0, &line, 0x20000, 0, 0, 1, 0, NULL), LINEERR_INVALAPPHANDLE, "openW bad app");
    IS(openW(app, 1, &line, 0x20000, 0, 0, 1, 0, NULL), LINEERR_BADDEVICEID, "openW device 1");
    IS(openW(app, 0xffffffff, &line, 0x20000, 0, 0, 1, 0, NULL), LINEERR_LINEMAPPERFAILED, "openW line mapper");

    /* request recipients */
    IS(regrec(0, 1, 1, 1), LINEERR_INVALAPPHANDLE, "register bad app");
    IS(regrec(app, 1, 0, 1), LINEERR_INVALREQUESTMODE, "register mode 0");
    IS(regrec(app, 1, 8, 1), LINEERR_INVALREQUESTMODE, "register mode 8");
    IS(getreq(0, 1, buf), LINEERR_INVALAPPHANDLE, "getRequest bad app");
    IS(getreq(app, 3, buf), LINEERR_INVALREQUESTMODE, "getRequest combined mode");
    IS(getreq(app, 1, NULL), LINEERR_INVALPOINTER, "getRequest NULL buffer");
    IS(getreq(app, 1, buf), E_NOTREG, "getRequest before registering");
    IS(regrec(app, 1, 1, 1), 0, "register MAKECALL");
    IS(getreq(app, 1, buf), E_NOREQ, "getRequest after registering has no request");
    IS(getreq(app, 2, buf), E_NOTREG, "the other mode is still unregistered");
    IS(regrec(app, 1, 1, 0), 0, "unregister MAKECALL");
    IS(getreq(app, 1, buf), E_NOTREG, "getRequest after unregistering");
    IS(regrec(app, 1, 3, 1), 0, "register two modes at once");
    IS(getreq(app, 2, buf), E_NOREQ, "second mode registered");
    IS(regrec(app, 1, 4, 1), 0, "register DROP");
    IS(getreq(app, 4, buf), E_NOREQ, "DROP registered");

    /* toll lists, configuration, icons */
    IS(toll(0, 0, "555", 1), LINEERR_INVALAPPHANDLE, "toll bad app");
    IS(toll(app, 0, NULL, 1), LINEERR_INVALPOINTER, "toll NULL address");
    IS(toll(app, 0, "555", 0), LINEERR_INVALPARAM, "toll option 0");
    IS(toll(app, 0, "555", 3), LINEERR_INVALPARAM, "toll option 3");
    IS(toll(app, 0, "555", 1), LINEERR_BADDEVICEID, "toll device 0");
    IS(cfgA(0, NULL, "tapi/line"), LINEERR_BADDEVICEID, "configDialogA device 0");
    IS(cfgW(2, NULL, L"tapi/line"), LINEERR_BADDEVICEID, "configDialogW device 2");
    {
        VARSTRING vs; char junk[8];
        memset(&vs, 0, sizeof(vs)); vs.dwTotalSize = sizeof(vs);
        IS(cfge(0, NULL, "tapi/line", junk, sizeof(junk), NULL), LINEERR_INVALPOINTER, "configDialogEdit NULL out");
        IS(cfge(0, NULL, "tapi/line", junk, sizeof(junk), &vs), LINEERR_BADDEVICEID, "configDialogEdit device 0");
        IS(gdcA(0, NULL, "tapi/line"), LINEERR_INVALPOINTER, "getDevConfigA NULL out");
        IS(gdcA(0, &vs, NULL), LINEERR_INVALPOINTER, "getDevConfigA NULL class");
        IS(gdcA(0, &vs, "tapi/line"), LINEERR_BADDEVICEID, "getDevConfigA device 0");
        IS(gdcW(0, NULL, L"tapi/line"), LINEERR_INVALPOINTER, "getDevConfigW NULL out");
        IS(gdcW(5, &vs, L"tapi/line"), LINEERR_BADDEVICEID, "getDevConfigW device 5");
        IS(sdcA(0, NULL, 4, "tapi/line"), LINEERR_INVALPOINTER, "setDevConfigA NULL config");
        IS(sdcA(0, junk, 4, "tapi/line"), LINEERR_BADDEVICEID, "setDevConfigA device 0");
        IS(sdcW(0, junk, 4, NULL), LINEERR_INVALPOINTER, "setDevConfigW NULL class");
        IS(sdcW(1, junk, 4, L"tapi/line"), LINEERR_BADDEVICEID, "setDevConfigW device 1");
    }
    IS(iconA(0, "tapi/line", NULL), LINEERR_INVALPOINTER, "getIconA NULL icon");
    IS(iconA(0, "tapi/line", &icon), LINEERR_BADDEVICEID, "getIconA device 0");
    IS(iconW(0, L"tapi/line", NULL), LINEERR_INVALPOINTER, "getIconW NULL icon");
    IS(iconW(9, L"tapi/line", &icon), LINEERR_BADDEVICEID, "getIconW device 9");

    for (i = 0; i < 3; i++)
    {
        HLINEAPP a = 0;
        IS(init(&a, inst, cb, "loop", &ndev), 0, "initialise again");
        IS(shut(a), 0, "shut down again");
    }
    shut(app);
    shut(appev);

    puts(failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
