/* user32 functions that were stubs (patches/sg/1604).
 *
 *  - SetWindowDisplayAffinity keeps WDA_MONITOR / WDA_EXCLUDEFROMCAPTURE and
 *    GetWindowDisplayAffinity reads it back (it failed with
 *    ERROR_NOT_ENOUGH_MEMORY); bad values are refused;
 *  - SetUserObjectSecurity sets a desktop's descriptor and
 *    GetUserObjectSecurity reads the object's own descriptor back (the stub
 *    made up an empty one), with the size needed when the buffer is small;
 *  - RegisterPowerSettingNotification returns a handle of its own and sends
 *    the setting's current value at once (WM_POWERBROADCAST,
 *    PBT_POWERSETTINGCHANGE), to a window and to a callback;
 *    UnregisterPowerSettingNotification refuses a bad handle;
 *  - EnableNonClientDpiScaling succeeds for the thread's window.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define PBT_POWERSETTINGCHANGE_ 0x8013

typedef struct { GUID PowerSetting; DWORD DataLength; UCHAR Data[1]; } setting_t;
typedef ULONG (CALLBACK *power_cb_t)(void *, ULONG, void *);
typedef struct { power_cb_t Callback; void *Context; } subscribe_t;

static const GUID acdc = {0x5d3e9a59,0xe9d5,0x4b00,{0xa6,0xbd,0xff,0x34,0xff,0x51,0x65,0x48}};
static const GUID personality = {0x245d8541,0x3943,0x4422,{0xb0,0x25,0x13,0xa7,0x84,0xf6,0x79,0xb7}};
static const GUID monitor_on = {0x02731015,0x4510,0x4526,{0x99,0xe6,0xe5,0xa1,0x7e,0xbd,0x1a,0xea}};

static int failures;
static int got_acdc = -1, got_personality, got_callback = -1;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_POWERBROADCAST && wp == PBT_POWERSETTINGCHANGE_)
    {
        setting_t *s = (setting_t *)lp;
        if (IsEqualGUID(&s->PowerSetting, &acdc) && s->DataLength == 4) got_acdc = *(DWORD *)s->Data;
        if (IsEqualGUID(&s->PowerSetting, &personality) && s->DataLength == sizeof(GUID)) got_personality = 1;
        return TRUE;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static ULONG CALLBACK power_cb(void *ctx, ULONG type, void *setting)
{
    setting_t *s = setting;
    if (ctx == (void *)0x1234 && type == PBT_POWERSETTINGCHANGE_ && IsEqualGUID(&s->PowerSetting, &monitor_on))
        got_callback = *(DWORD *)s->Data;
    return 0;
}

static void pump(DWORD ms)
{
    DWORD start = GetTickCount();
    MSG msg;
    while (GetTickCount() - start < ms)
    {
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(10);
    }
}

int main(void)
{
    WNDCLASSW wc = {0};
    HWND hwnd;
    DWORD aff, needed;
    BYTE sd[4096];
    SECURITY_INFORMATION si = OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;
    PSID owner;
    PACL dacl;
    BOOL defaulted, present, ok;
    HPOWERNOTIFY h1, h2, h3;
    subscribe_t sub = { power_cb, (void *)0x1234 };
    HMODULE user32 = GetModuleHandleA("user32.dll");
    BOOL (WINAPI *pEnableNonClientDpiScaling)(HWND) = (void *)GetProcAddress(user32, "EnableNonClientDpiScaling");
    BOOL (WINAPI *pSetWindowDisplayAffinity)(HWND, DWORD) = (void *)GetProcAddress(user32, "SetWindowDisplayAffinity");
    BOOL (WINAPI *pGetWindowDisplayAffinity)(HWND, DWORD *) = (void *)GetProcAddress(user32, "GetWindowDisplayAffinity");

    wc.lpfnWndProc = wndproc;
    wc.lpszClassName = L"userapis";
    RegisterClassW(&wc);
    hwnd = CreateWindowW(L"userapis", L"userapis", WS_OVERLAPPEDWINDOW, 0, 0, 200, 200, 0, 0, 0, 0);

    /* display affinity */
    SetLastError(0xdeadbeef);
    ok = pSetWindowDisplayAffinity(hwnd, WDA_MONITOR);
    printf("SetWindowDisplayAffinity(WDA_MONITOR): ok %d err %lu\n", ok, ok ? 0 : GetLastError());
    check(ok, "SetWindowDisplayAffinity(WDA_MONITOR) succeeds");
    aff = 0xdead;
    ok = pGetWindowDisplayAffinity(hwnd, &aff);
    check(ok && aff == WDA_MONITOR, "... and GetWindowDisplayAffinity reads it back");
    ok = pSetWindowDisplayAffinity(hwnd, 0x11);
    aff = 0xdead;
    pGetWindowDisplayAffinity(hwnd, &aff);
    check(ok && aff == 0x11, "WDA_EXCLUDEFROMCAPTURE is kept");
    SetLastError(0xdeadbeef);
    ok = pSetWindowDisplayAffinity(hwnd, 5);
    check(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "an unknown affinity is refused");
    ok = pSetWindowDisplayAffinity(hwnd, WDA_NONE);
    aff = 0xdead;
    pGetWindowDisplayAffinity(hwnd, &aff);
    check(ok && aff == WDA_NONE, "WDA_NONE clears it");
    SetLastError(0xdeadbeef);
    ok = pSetWindowDisplayAffinity((HWND)0xdead0, WDA_MONITOR);
    check(!ok && GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "a bad window is refused");

    /* window station and desktop security: the object's own descriptor,
     * set and read back on a desktop of our own */
    {
        HDESK desk = CreateDesktopW(L"userapis-desk", NULL, NULL, 0, GENERIC_ALL, NULL);
        BYTE newsd[SECURITY_DESCRIPTOR_MIN_LENGTH], aclbuf[256], tokbuf[256];
        PACL acl = (PACL)aclbuf;
        HANDLE token;
        DWORD tlen;
        SID_IDENTIFIER_AUTHORITY world_auth = {SECURITY_WORLD_SID_AUTHORITY};
        PSID world;

        OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
        GetTokenInformation(token, TokenUser, tokbuf, sizeof(tokbuf), &tlen);
        AllocateAndInitializeSid(&world_auth, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &world);
        InitializeSecurityDescriptor(newsd, SECURITY_DESCRIPTOR_REVISION);
        InitializeAcl(acl, sizeof(aclbuf), ACL_REVISION);
        AddAccessAllowedAce(acl, ACL_REVISION, GENERIC_ALL, world);
        AddAccessAllowedAce(acl, ACL_REVISION, GENERIC_ALL, ((TOKEN_USER *)tokbuf)->User.Sid);
        SetSecurityDescriptorOwner(newsd, ((TOKEN_USER *)tokbuf)->User.Sid, FALSE);
        SetSecurityDescriptorDacl(newsd, TRUE, acl, FALSE);
        ok = desk && SetUserObjectSecurity(desk, &si, newsd);
        printf("SetUserObjectSecurity: desk %p ok %d err %lu\n", desk, ok, ok ? 0 : GetLastError());
        check(ok, "SetUserObjectSecurity sets a desktop's owner and DACL");
        needed = 0;
        SetLastError(0xdeadbeef);
        ok = GetUserObjectSecurity(desk, &si, sd, 8, &needed);
        printf("small buffer: ok %d err %lu needed %lu\n", ok, GetLastError(), needed);
        check(!ok && GetLastError() == ERROR_INSUFFICIENT_BUFFER && needed > 40, "a too small buffer gives the size needed");
        ok = GetUserObjectSecurity(desk, &si, sd, sizeof(sd), &needed);
        owner = NULL; dacl = NULL; present = FALSE;
        if (ok)
        {
            GetSecurityDescriptorOwner(sd, &owner, &defaulted);
            GetSecurityDescriptorDacl(sd, &present, &dacl, &defaulted);
        }
        printf("desktop sd: ok %d owner %p dacl present %d aces %d\n", ok, owner, present, dacl ? dacl->AceCount : -1);
        check(ok && owner && EqualSid(owner, ((TOKEN_USER *)tokbuf)->User.Sid), "GetUserObjectSecurity gives the owner set");
        check(ok && present && dacl && dacl->AceCount == 2, "... and the DACL set");
        ok = GetUserObjectSecurity(GetProcessWindowStation(), &si, sd, sizeof(sd), &needed);
        check(ok, "GetUserObjectSecurity of the window station succeeds");
        FreeSid(world);
        CloseDesktop(desk);
    }

    /* power setting notifications */
    h1 = RegisterPowerSettingNotification(hwnd, &acdc, DEVICE_NOTIFY_WINDOW_HANDLE);
    h2 = RegisterPowerSettingNotification(hwnd, &personality, DEVICE_NOTIFY_WINDOW_HANDLE);
    h3 = RegisterPowerSettingNotification(&sub, &monitor_on, 2 /* DEVICE_NOTIFY_CALLBACK */);
    printf("handles %p %p %p\n", h1, h2, h3);
    check(h1 && h2 && h1 != h2 && h1 != (HPOWERNOTIFY)0xdeadbeef, "each registration has a handle of its own");
    pump(1500);
    printf("acdc %d personality %d callback %d\n", got_acdc, got_personality, got_callback);
    check(got_acdc == 0 || got_acdc == 1, "the window is sent the power source at once");
    check(got_personality == 1, "... and the power plan");
    check(got_callback == 1, "a callback registration is called with the monitor state");
    check(UnregisterPowerSettingNotification(h1) && UnregisterPowerSettingNotification(h2) &&
          UnregisterPowerSettingNotification(h3), "the registrations are undone");
    SetLastError(0xdeadbeef);
    ok = UnregisterPowerSettingNotification((HPOWERNOTIFY)0xdeadbeef);
    check(!ok && GetLastError() == ERROR_INVALID_HANDLE, "a bad handle is refused");

    /* non-client DPI scaling */
    ok = pEnableNonClientDpiScaling(hwnd);
    check(ok, "EnableNonClientDpiScaling succeeds for the thread's window");

    DestroyWindow(hwnd);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
