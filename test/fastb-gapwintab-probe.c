/* wintab32 manager cursor settings and packet hooks (patch 3652), driven through a fake tablet driver. */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef struct { char lcName[40]; UINT lcOptions, lcStatus, lcLocks, lcMsgBase, lcDevice, lcPktRate;
    DWORD lcPktData, lcPktMode, lcMoveMask, lcBtnDnMask, lcBtnUpMask;
    LONG lcInOrgX, lcInOrgY, lcInOrgZ, lcInExtX, lcInExtY, lcInExtZ, lcOutOrgX, lcOutOrgY, lcOutOrgZ,
         lcOutExtX, lcOutExtY, lcOutExtZ;
    DWORD lcSensX, lcSensY, lcSensZ; BOOL lcSysMode; int lcSysOrgX, lcSysOrgY, lcSysExtX, lcSysExtY;
    DWORD lcSysSensX, lcSysSensY; } LOGCTXA;
typedef struct { UINT cursor; DWORD buttons; UINT np; UINT tp; } PKT;

typedef HANDLE HMGR_;
static HANDLE (WINAPI *pWTMgrOpen)(HWND, UINT);
static BOOL (WINAPI *pWTMgrClose)(HANDLE);
static BOOL (WINAPI *pWTMgrCsrEnable)(HANDLE, UINT, BOOL);
static BOOL (WINAPI *pWTMgrCsrButtonMap)(HANDLE, UINT, BYTE *, BYTE *);
static BOOL (WINAPI *pWTMgrCsrPressureBtnMarks)(HANDLE, UINT, DWORD, DWORD);
static BOOL (WINAPI *pWTMgrCsrPressureBtnMarksEx)(HANDLE, UINT, UINT *, UINT *);
static BOOL (WINAPI *pWTMgrCsrPressureResponse)(HANDLE, UINT, UINT *, UINT *);
static HANDLE (WINAPI *pWTMgrPacketHookExA)(HANDLE, int, const char *, const char *);
static HANDLE (WINAPI *pWTMgrPacketHookExW)(HANDLE, int, const WCHAR *, const char *);
static BOOL (WINAPI *pWTMgrPacketUnhook)(HANDLE);
static LRESULT (WINAPI *pWTMgrPacketHookNext)(HANDLE, int, WPARAM, LPARAM);
static UINT (WINAPI *pWTInfoA)(UINT, UINT, void *);
static HANDLE (WINAPI *pWTOpenA)(HWND, LOGCTXA *, BOOL);
static BOOL (WINAPI *pWTClose)(HANDLE);
static int (WINAPI *pWTPacketsGet)(HANDLE, int, void *);

static void (CDECL *FakeSetPacket)(UINT, DWORD, UINT, UINT);
static HWND (CDECL *FakeDefaultWindow)(void);
static void (CDECL *FakeSetNext)(void *, HANDLE);
static LONG (CDECL *FakeHook1Count)(void);
static LONG (CDECL *FakeHook2Count)(void);
static UINT (CDECL *FakeHook1Buttons)(void);

#define DEFAULT ((void *)-1L)
static HWND owner;
static HANDLE ctx;

static int send_packet(UINT cursor, DWORD buttons, UINT np, UINT tp, PKT *out)
{
    PKT drain[8];
    FakeSetPacket(cursor, buttons, np, tp);
    SendMessageA(FakeDefaultWindow(), 0x7ff0, 0, (LPARAM)owner);
    return pWTPacketsGet(ctx, 8, out ? out : drain);
}

static void set_graphics_driver(const WCHAR *guidkey, const WCHAR *value, WCHAR *old, DWORD oldsize)
{
    HKEY key;
    DWORD size = oldsize;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, guidkey, 0, KEY_ALL_ACCESS, &key)) return;
    if (old) RegQueryValueExW(key, L"GraphicsDriver", NULL, NULL, (BYTE *)old, &size);
    RegSetValueExW(key, L"GraphicsDriver", 0, REG_SZ, (const BYTE *)value, (lstrlenW(value) + 1) * sizeof(WCHAR));
    RegCloseKey(key);
}

int main(void)
{
    HMODULE wintab, fake;
    WCHAR keyname[200], oldval[MAX_PATH] = L"", fakepath[MAX_PATH];
    UINT atom;
    HANDLE mgr, mgr2, h1, h2;
    PKT pk;
    LOGCTXA lc;
    BYTE map[32], got[32];
    UINT marks[2], tmarks[2], resp[256], gotresp[256], i;
    int n;

    GetCurrentDirectoryW(MAX_PATH, fakepath);
    lstrcatW(fakepath, L"\\fakedrv.dll");
    fake = LoadLibraryW(fakepath);
    if (!fake) { printf("FAIL  fakedrv.dll\nRESULT: FAIL\n"); return 1; }
#define GETF(n) n = (void *)GetProcAddress(fake, #n)
    GETF(FakeSetPacket); GETF(FakeDefaultWindow); GETF(FakeSetNext); GETF(FakeHook1Count); GETF(FakeHook2Count);
    GETF(FakeHook1Buttons);

    /* point wintab32 at the fake driver only while it loads */
    atom = HandleToULong(GetPropW(GetDesktopWindow(), L"__wine_display_device_guid"));
    lstrcpyW(keyname, L"System\\CurrentControlSet\\Control\\Video\\{");
    GlobalGetAtomNameW(atom, keyname + lstrlenW(keyname), 60);
    lstrcatW(keyname, L"}\\0000");
    set_graphics_driver(keyname, fakepath, oldval, sizeof(oldval));
    wintab = LoadLibraryA("wintab32.dll");
    set_graphics_driver(keyname, oldval, NULL, 0);
    if (!wintab) { printf("FAIL  wintab32.dll\nRESULT: FAIL\n"); return 1; }
#define GET(n) p##n = (void *)GetProcAddress(wintab, #n)
    GET(WTMgrOpen); GET(WTMgrClose); GET(WTMgrCsrEnable); GET(WTMgrCsrButtonMap); GET(WTMgrCsrPressureBtnMarks);
    GET(WTMgrCsrPressureBtnMarksEx); GET(WTMgrCsrPressureResponse); GET(WTMgrPacketHookExA); GET(WTMgrPacketHookExW);
    GET(WTMgrPacketUnhook); GET(WTMgrPacketHookNext); GET(WTInfoA); GET(WTOpenA); GET(WTClose); GET(WTPacketsGet);

    owner = CreateWindowA("static", "owner", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    mgr = pWTMgrOpen(owner, 0x7ff0);
    check(mgr != NULL, "manager");

    memset(&lc, 0, sizeof(lc));
    strcpy(lc.lcName, "probe");
    lc.lcPktData = 0x0020 | 0x0040 | 0x0400 | 0x0800;  /* cursor, buttons, normal and tangent pressure */
    lc.lcInExtX = lc.lcInExtY = lc.lcOutExtX = lc.lcOutExtY = 1000;
    ctx = pWTOpenA(owner, &lc, TRUE);
    check(ctx != NULL, "context opens on the fake tablet");

    memset(&pk, 0, sizeof(pk));
    n = send_packet(0, 1, 512, 0, &pk);
    check(n == 1 && pk.cursor == 0 && pk.buttons == 1 && pk.np == 512, "packets arrive unchanged by default");

    /* validation */
    SetLastError(0);
    check(!pWTMgrCsrEnable((HANDLE)0x99, 0, TRUE) && GetLastError() == ERROR_INVALID_HANDLE, "enable needs a manager");
    check(!pWTMgrCsrEnable(mgr, 2, TRUE), "cursor 2 does not exist (the tablet has two)");
    check(!pWTMgrCsrButtonMap((HANDLE)0x99, 0, map, NULL), "button map needs a manager");
    check(!pWTMgrCsrPressureResponse(mgr, 7, DEFAULT, DEFAULT), "response needs a valid cursor");

    /* enable */
    check(pWTMgrCsrEnable(mgr, 0, FALSE), "disable cursor 0");
    check(send_packet(0, 1, 100, 0, NULL) == 0, "its packets are dropped");
    check(send_packet(1, 1, 100, 0, NULL) == 1, "cursor 1 is unaffected");
    check(pWTMgrCsrEnable(mgr, 0, TRUE) && send_packet(0, 1, 100, 0, NULL) == 1, "enabled again");

    /* button map */
    memset(got, 0, sizeof(got));
    check(pWTInfoA(200, 7, got) == 32 && got[3] == 3 && got[31] == 31, "driver map before any change");
    for (i = 0; i < 32; i++) map[i] = i;
    map[0] = 3; map[1] = 0;
    check(pWTMgrCsrButtonMap(mgr, 0, map, NULL), "set the button map");
    send_packet(0, 0x3, 0, 0, &pk);
    check(pk.buttons == 0x9, "physical 0 and 1 become logical 3 and 0");
    send_packet(1, 0x3, 0, 0, &pk);
    check(pk.buttons == 0x3, "other cursors keep their map");
    memset(got, 0, sizeof(got));
    check(pWTInfoA(200, 7, got) == 32 && got[0] == 3 && got[1] == 0, "WTInfo reports the new map");
    check(pWTMgrCsrButtonMap(mgr, 0, DEFAULT, NULL), "default map");
    send_packet(0, 0x3, 0, 0, &pk);
    check(pk.buttons == 0x3, "default map restored");
    check(pWTInfoA(200, 7, got) == 32 && got[0] == 0 && got[3] == 3, "WTInfo reports the driver's map again");

    /* pressure response: an inverting curve */
    for (i = 0; i < 256; i++) resp[i] = 1023 - i * 4;
    check(pWTMgrCsrPressureResponse(mgr, 0, resp, NULL), "set the normal pressure curve");
    send_packet(0, 0, 0, 77, &pk);
    check(pk.np == 1023 && pk.tp == 77, "minimum pressure maps to the first entry");
    send_packet(0, 0, 1023, 0, &pk);
    check(pk.np == 3, "maximum pressure maps to the last entry");
    send_packet(0, 0, 512, 0, &pk);
    check(pk.np == resp[127], "middle of the range uses the evenly stretched index");
    memset(gotresp, 0, sizeof(gotresp));
    check(pWTInfoA(200, 11, gotresp) == sizeof(gotresp) && !memcmp(gotresp, resp, sizeof(resp)), "WTInfo reports the curve");
    send_packet(1, 0, 0, 0, &pk);
    check(pk.np == 0, "cursor 1 has no curve");
    check(pWTMgrCsrPressureResponse(mgr, 0, DEFAULT, NULL), "reset curve");
    send_packet(0, 0, 0, 0, &pk);
    check(pk.np == 0, "curve removed");

    /* pressure button marks */
    marks[0] = 200; marks[1] = 600;
    check(pWTMgrCsrPressureBtnMarksEx(mgr, 0, marks, NULL), "set the pressure marks");
    send_packet(0, 0, 400, 0, &pk);
    check(!(pk.buttons & 2), "below the press mark: no button");
    send_packet(0, 0, 700, 0, &pk);
    check(pk.buttons & 2, "above the press mark: the pressure button goes down");
    send_packet(0, 0, 400, 0, &pk);
    check(pk.buttons & 2, "between the marks it stays down");
    send_packet(0, 0, 100, 0, &pk);
    check(!(pk.buttons & 2), "below the release mark it goes up");
    memset(tmarks, 0, sizeof(tmarks));
    check(pWTInfoA(200, 10, tmarks) == 8 && tmarks[0] == 200 && tmarks[1] == 600, "WTInfo reports the marks");
    marks[0] = 600; marks[1] = 200;
    check(pWTMgrCsrPressureBtnMarksEx(mgr, 0, marks, NULL), "invalid marks are accepted");
    send_packet(0, 0, 900, 0, &pk);
    check(!(pk.buttons & 2), "invalid marks switch the button events off");
    check(pWTMgrCsrPressureBtnMarksEx(mgr, 0, DEFAULT, NULL), "default marks");
    send_packet(0, 0, 900, 0, &pk);
    check(!(pk.buttons & 2), "no marks, no pressure button");
    check(pWTMgrCsrPressureBtnMarks(mgr, 0, MAKELONG(100, 300), (DWORD)-1), "packed marks");
    check(pWTInfoA(200, 10, tmarks) == 8 && tmarks[0] == 100 && tmarks[1] == 300, "packed marks: release low word, press high word");
    pWTMgrCsrPressureBtnMarks(mgr, 0, (DWORD)-1, (DWORD)-1);

    /* packet hooks */
    SetLastError(0);
    check(pWTMgrPacketHookExA((HANDLE)0x99, 2, "fakedrv.dll", "RecHook") == NULL && GetLastError() == ERROR_INVALID_HANDLE, "hook needs a manager");
    check(pWTMgrPacketHookExA(mgr, 3, "fakedrv.dll", "RecHook") == NULL && GetLastError() == ERROR_INVALID_PARAMETER, "hook type");
    check(pWTMgrPacketHookExA(mgr, 2, "fakedrv.dll", "NoSuchProc") == NULL && GetLastError() == ERROR_PROC_NOT_FOUND, "unknown hook procedure");
    check(pWTMgrPacketHookExA(mgr, 2, "nosuchmodule.dll", "RecHook") == NULL, "unknown module");
    h1 = pWTMgrPacketHookExA(mgr, 2, "fakedrv.dll", "RecHook");
    check(h1 != NULL, "record hook installed");
    send_packet(0, 0x5, 0, 0, NULL);
    check(FakeHook1Count() == 1 && FakeHook1Buttons() == 0x5, "the hook saw the packet");
    pWTMgrCsrEnable(mgr, 0, FALSE);
    send_packet(0, 0x5, 0, 0, NULL);
    check(FakeHook1Count() == 1, "dropped packets do not reach the hook");
    pWTMgrCsrEnable(mgr, 0, TRUE);
    h2 = pWTMgrPacketHookExW(mgr, 2, L"fakedrv.dll", "RecHook2");
    check(h2 != NULL && h2 != h1, "second hook (wide module name)");
    FakeSetNext(pWTMgrPacketHookNext, h2);
    send_packet(0, 0x1, 0, 0, NULL);
    check(FakeHook2Count() == 1 && FakeHook1Count() == 2, "the newest hook runs first and can call the next one");
    check(pWTMgrPacketHookNext(h1, 0, 0, (LPARAM)&pk) == 0, "no older hook: next returns 0");
    check(pWTMgrPacketUnhook(h2), "unhook the newest");
    check(!pWTMgrPacketUnhook(h2), "unhook twice");
    send_packet(0, 0x1, 0, 0, NULL);
    check(FakeHook2Count() == 1 && FakeHook1Count() == 3, "only the older hook remains");
    mgr2 = pWTMgrOpen(owner, 0x7ff0);
    check(pWTMgrClose(mgr), "closing the manager removes its hooks");
    send_packet(0, 0x1, 0, 0, NULL);
    check(FakeHook1Count() == 3, "no hook after the manager closed");
    check(!pWTMgrPacketUnhook(h1), "its hook handle is gone");
    pWTMgrClose(mgr2);

    pWTClose(ctx);
    DestroyWindow(owner);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
