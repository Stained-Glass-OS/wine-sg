/* Session notifications (patches/sg/1704), run by test/wtsnotify-gate.sh with
 * a stand-in for the compositor's control socket (test/wtsnotify-standin.py):
 * WTSRegisterSessionNotification(Ex) and WTSUnRegisterSessionNotification(Ex)
 * were stubs and no WM_WTSSESSION_CHANGE was ever sent. A registered window
 * is told of lock, unlock, Remote Desktop taking the session (console
 * disconnect, remote connect) and giving it back (remote disconnect, console
 * connect, lock), and remote control; at the lock WTSSessionInfoEx says the
 * session is locked, while remote WTSClientProtocolType says RDP; a window
 * unregistered is told nothing.
 *
 *   wtsnotify-probe.exe GOFILE */
#include <windows.h>
#include <wtsapi32.h>
#include <stdio.h>

#ifndef WM_WTSSESSION_CHANGE
#define WM_WTSSESSION_CHANGE 0x02b1
#endif

static BOOL (WINAPI *pRegisterEx)(HANDLE, HWND, DWORD);
static BOOL (WINAPI *pUnRegisterEx)(HANDLE, HWND);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static WPARAM codes[32];
static int count, other_count;
static LONG flags_at_lock = -2, flags_at_unlock = -2;
static int protocol_remote = -1;
static DWORD session_seen = 0xdeadbeef;

static LONG session_flags(void)
{
    WTSINFOEXW *info = NULL;
    DWORD size = 0;
    LONG flags = -3;

    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionInfoEx,
                                    (WCHAR **)&info, &size) && info)
    {
        flags = info->Data.WTSInfoExLevel1.SessionFlags;
        WTSFreeMemory(info);
    }
    return flags;
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_WTSSESSION_CHANGE)
    {
        if (GetWindowLongPtrW(hwnd, GWLP_USERDATA))
        {
            other_count++;
            return 0;
        }
        if (count < 32) codes[count++] = wp;
        session_seen = (DWORD)lp;
        if (wp == WTS_SESSION_LOCK && flags_at_lock == -2) flags_at_lock = session_flags();
        if (wp == WTS_SESSION_UNLOCK && flags_at_unlock == -2) flags_at_unlock = session_flags();
        if (wp == WTS_REMOTE_CONNECT)
        {
            USHORT *protocol = NULL;
            DWORD size;
            if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSClientProtocolType,
                                            (WCHAR **)&protocol, &size) && protocol)
            {
                protocol_remote = *protocol;
                WTSFreeMemory(protocol);
            }
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int main(int argc, char **argv)
{
    static const WPARAM expected[] = { WTS_SESSION_LOCK, WTS_SESSION_UNLOCK, WTS_CONSOLE_DISCONNECT, WTS_REMOTE_CONNECT,
                                       WTS_REMOTE_DISCONNECT, WTS_CONSOLE_CONNECT, WTS_SESSION_LOCK, WTS_SESSION_UNLOCK,
                                       WTS_SESSION_REMOTE_CONTROL, WTS_SESSION_REMOTE_CONTROL };
    WNDCLASSW wc = { 0 };
    HWND hwnd, other;
    DWORD start, session_id = 0;
    LONG flags;
    MSG msg;
    FILE *f;
    int i, same;

    pRegisterEx = (void *)GetProcAddress(GetModuleHandleA("wtsapi32.dll"), "WTSRegisterSessionNotificationEx");
    pUnRegisterEx = (void *)GetProcAddress(GetModuleHandleA("wtsapi32.dll"), "WTSUnRegisterSessionNotificationEx");
    wc.lpfnWndProc = wndproc;
    wc.lpszClassName = L"sgwts";
    RegisterClassW(&wc);
    hwnd = CreateWindowW(L"sgwts", L"sgwts", 0, 0, 0, 10, 10, HWND_MESSAGE, NULL, NULL, NULL);
    other = CreateWindowW(L"sgwts", L"other", 0, 0, 0, 10, 10, HWND_MESSAGE, NULL, NULL, NULL);
    printf("      windows %p %p\n", hwnd, other);
    SetWindowLongPtrW(other, GWLP_USERDATA, 1);

    flags = session_flags();
    check(flags == WTS_SESSIONSTATE_UNLOCK, "WTSSessionInfoEx: unlocked (was not implemented)");
    check(!WTSRegisterSessionNotification(hwnd, 5) && GetLastError() == ERROR_INVALID_PARAMETER, "bad flags: refused");
    check(!WTSRegisterSessionNotification((HWND)0xdead, NOTIFY_FOR_THIS_SESSION), "no such window: refused");
    check(WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION), "WTSRegisterSessionNotification");
    check(pRegisterEx && pRegisterEx(WTS_CURRENT_SERVER_HANDLE, other, NOTIFY_FOR_ALL_SESSIONS),
          "WTSRegisterSessionNotificationEx");
    check(pUnRegisterEx && pUnRegisterEx(WTS_CURRENT_SERVER_HANDLE, other), "WTSUnRegisterSessionNotificationEx");

    if ((f = fopen(argc > 1 ? argv[1] : "go", "w"))) fclose(f);
    start = GetTickCount();
    while (count < (int)ARRAYSIZE(expected) && GetTickCount() - start < 20000)
    {
        MsgWaitForMultipleObjects(0, NULL, FALSE, 200, QS_ALLINPUT);
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    }
    Sleep(300);
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);

    printf("      %d messages:", count);
    for (i = 0; i < count; i++) printf(" %u", (unsigned int)codes[i]);
    printf("\n");
    same = count == ARRAYSIZE(expected);
    for (i = 0; same && i < count; i++) same = codes[i] == expected[i];
    check(same, "WM_WTSSESSION_CHANGE: lock, unlock, console disconnect, remote connect, remote disconnect, "
                "console connect, lock, unlock, remote control twice");
    ProcessIdToSessionId(GetCurrentProcessId(), &session_id);
    check(session_seen == session_id, "lParam: the session");
    check(flags_at_lock == WTS_SESSIONSTATE_LOCK, "WTSSessionInfoEx at the lock: locked");
    check(flags_at_unlock == WTS_SESSIONSTATE_UNLOCK, "and unlocked after");
    check(protocol_remote == 2, "WTSClientProtocolType while remote: RDP");
    check(other_count == 0, "an unregistered window: nothing");
    check(WTSUnRegisterSessionNotification(hwnd), "WTSUnRegisterSessionNotification");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
