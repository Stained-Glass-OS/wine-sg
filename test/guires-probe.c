/* Process odds and ends (patches/sg/1677), run by test/guires-gate.sh:
 * GetGuiResources counts the process's GDI objects and windows (and the
 * most seen); WTSGetActiveConsoleSessionId gives the console's session
 * from the shared user data; UnlockFileEx completes its overlapped
 * structure and signals its event; the process cookie is random and not
 * 0; ThreadEnableAlignmentFaultFixup is taken. These were stubs. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

#ifndef GR_GDIOBJECTS
#define GR_GDIOBJECTS 0
#define GR_USEROBJECTS 1
#define GR_GDIOBJECTS_PEAK 2
#define GR_USEROBJECTS_PEAK 4
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

int main(void)
{
    NTSTATUS (WINAPI *query)(HANDLE, PROCESSINFOCLASS, void *, ULONG, ULONG *) =
        (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess");
    NTSTATUS (WINAPI *set_thread)(HANDLE, THREADINFOCLASS, const void *, ULONG) =
        (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtSetInformationThread");
    DWORD gdi0, gdi1, user0, user1, peak, session;
    HBRUSH brushes[20];
    HWND hwnd[3];
    OVERLAPPED ov = { 0 };
    WCHAR path[MAX_PATH], dir[MAX_PATH];
    HANDLE file;
    ULONG cookie = 0;
    BOOLEAN fixup = TRUE;
    int i;

    /* GUI resources */
    gdi0 = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (i = 0; i < 20; i++) brushes[i] = CreateSolidBrush(RGB(i, 0, 0));
    gdi1 = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    printf("      gdi %lu -> %lu\n", gdi0, gdi1);
    check(gdi1 >= gdi0 + 20, "GR_GDIOBJECTS counts 20 new brushes");
    for (i = 0; i < 20; i++) DeleteObject(brushes[i]);
    peak = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS_PEAK);
    check(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) + 20 <= gdi1 + 1 && peak >= gdi1,
          "deleted: fewer, the peak kept");
    user0 = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    for (i = 0; i < 3; i++)
        hwnd[i] = CreateWindowA("STATIC", "probe", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    user1 = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    printf("      user %lu -> %lu\n", user0, user1);
    check(user1 >= user0 + 3, "GR_USEROBJECTS counts 3 new windows");
    for (i = 0; i < 3; i++) DestroyWindow(hwnd[i]);
    check(GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS_PEAK) >= user1, "and their peak");
    SetLastError(0xdeadbeef);
    check(!GetGuiResources(GetCurrentProcess(), 7) && GetLastError() == ERROR_INVALID_PARAMETER,
          "unknown flags: ERROR_INVALID_PARAMETER");

    /* the console session */
    session = WTSGetActiveConsoleSessionId();
    printf("      console session %lu, ours %lu\n", session, NtCurrentTeb()->ProcessEnvironmentBlock->SessionId);
    check(session == *(volatile ULONG *)(0x7ffe0000 + 0x2d8) && session != 0xffffffff,
          "WTSGetActiveConsoleSessionId: the shared user data's ActiveConsoleId");
    check(session == 1, "which is the session the console's user has here");

    /* UnlockFileEx with an event */
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"ulk", 0, path);
    file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    WriteFile(file, "0123456789", 10, NULL, NULL);
    check(LockFile(file, 0, 0, 10, 0), "LockFile");
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    ov.Internal = 0xdead;
    check(UnlockFileEx(file, 0, 10, 0, &ov), "UnlockFileEx with an overlapped structure");
    check(ov.Internal == 0 && WaitForSingleObject(ov.hEvent, 0) == WAIT_OBJECT_0,
          "its status set and its event signaled");
    SetLastError(0);
    i = UnlockFileEx(file, 0, 10, 0, &ov);
    printf("      again: %d, error %lu\n", i, GetLastError());
    check(!i && GetLastError() == ERROR_NOT_LOCKED, "not locked: ERROR_NOT_LOCKED");
    CloseHandle(ov.hEvent);
    CloseHandle(file);

    /* the process cookie */
    check(query && !query(GetCurrentProcess(), 36 /* ProcessCookie */, &cookie, sizeof(cookie), NULL) && cookie,
          "ProcessCookie: not 0");
    {
        ULONG again = 0;
        query(GetCurrentProcess(), 36, &again, sizeof(again), NULL);
        check(again == cookie, "and the same for the process's life");
    }

    check(set_thread && !set_thread(GetCurrentThread(), 7 /* ThreadEnableAlignmentFaultFixup */, &fixup,
                                    sizeof(fixup)), "ThreadEnableAlignmentFaultFixup");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
