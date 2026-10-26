/* Whose is the desktop window (patches/sg/6119)? On Windows the system's (csrss.exe, a process
 * programs cannot open), not the shell's; here explorer.exe owns it, and programs were told so.
 * NVDA caches the desktop object first thing and loaded its explorer app module before its
 * IAccessible support (a circular import): no explorer support all session. */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

static DWORD explorer_pid(void)
{
    PROCESSENTRY32W pe = { sizeof(pe) };
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    DWORD pid = 0;
    BOOL ok;

    for (ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
        if (!lstrcmpiW(pe.szExeFile, L"explorer.exe")) pid = pe.th32ProcessID;
    CloseHandle(snap);
    return pid;
}

static BOOL in_snapshot(DWORD pid)
{
    PROCESSENTRY32W pe = { sizeof(pe) };
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    BOOL ok, found = FALSE;

    for (ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
        if (pe.th32ProcessID == pid) found = TRUE;
    CloseHandle(snap);
    return found;
}

int main(void)
{
    HWND desktop = GetDesktopWindow(), own;
    DWORD pid = 0, pid16 = 0, own_pid = 0, tid, tid16, exp, of_thread = 0;
    HANDLE h;

    tid = GetWindowThreadProcessId(desktop, &pid);
    tid16 = GetWindowThreadProcessId((HWND)(ULONG_PTR)LOWORD(desktop), &pid16);
    exp = explorer_pid();
    printf("desktop tid %04lx pid %04lx explorer %04lx self %04lx\n", tid, pid, exp, GetCurrentProcessId());
    printf("TID %d\n", tid != 0);
    printf("NOTEXPLORER %d\n", exp && pid && pid != exp && pid != GetCurrentProcessId());
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    printf("NOTOPEN %d\n", !h);
    if (h) CloseHandle(h);
    printf("NOTLISTED %d\n", !in_snapshot(pid));
    printf("SHORT %d\n", tid16 == tid && pid16 == pid);
    if ((h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid)))
    {
        of_thread = GetProcessIdOfThread(h);
        CloseHandle(h);
    }
    printf("THREADPROCESS %d\n", of_thread == exp);
    own = CreateWindowW(L"STATIC", L"sg", 0, 0, 0, 10, 10, HWND_MESSAGE, NULL, NULL, NULL);
    tid = GetWindowThreadProcessId(own, &own_pid);
    printf("own %p tid %04lx pid %04lx\n", own, tid, own_pid);
    printf("OWN %d\n", own && tid == GetCurrentThreadId() && own_pid == GetCurrentProcessId());
    DestroyWindow(own);
    return 0;
}
