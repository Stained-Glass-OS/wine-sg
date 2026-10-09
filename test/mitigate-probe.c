/* Process mitigation policies enforced (patches/sg/1705), run by
 * test/mitigate-gate.sh. Policies were kept and read back but nothing held a
 * process to them. Each policy is tried in a child of its own (a policy once
 * on stays on), before and after it is set:
 *   strict    StrictHandleCheck: an invalid handle raises STATUS_INVALID_HANDLE
 *   acg       DynamicCode: no executable allocation, no data made executable,
 *             no W+X, no executable view of a data section; images still load
 *   acgopt    ... with AllowThreadOptOut, an opted-out thread may
 *   sys32     ImageLoad PreferSystem32Images: sgimg.dll from the system
 *             directory rather than the program's
 *   fonts     FontDisable DisableNonSystemFonts: no font outside the fonts
 *             folder, none from memory; system fonts still load
 *   hooks     ExtensionPointDisable: another process's hook DLL is not
 *             loaded into it (the hook is not called); without it, it is
 *
 *   mitigate-probe.exe [MODE [arg]] */
#include <windows.h>
#include <stdio.h>

#ifndef ERROR_DYNAMIC_CODE_BLOCKED
#define ERROR_DYNAMIC_CODE_BLOCKED 1655
#endif
#ifndef STATUS_INVALID_HANDLE
#define STATUS_INVALID_HANDLE ((DWORD)0xC0000008)
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static BOOL set_policy(PROCESS_MITIGATION_POLICY policy, DWORD flags)
{
    return SetProcessMitigationPolicy(policy, &flags, sizeof(flags));
}

static volatile LONG handle_exceptions;
static LONG CALLBACK veh(EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == STATUS_INVALID_HANDLE)
    {
        InterlockedIncrement(&handle_exceptions);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void test_strict(void)
{
    HANDLE bad = (HANDLE)(ULONG_PTR)0xdead0;

    AddVectoredExceptionHandler(1, veh);
    check(!CloseHandle(bad) && GetLastError() == ERROR_INVALID_HANDLE && !handle_exceptions,
          "before: CloseHandle of a bad handle fails quietly");
    check(set_policy(ProcessStrictHandleCheckPolicy, 3), "StrictHandleCheck set");
    CloseHandle(bad);
    check(handle_exceptions == 1, "after: CloseHandle raises STATUS_INVALID_HANDLE (was not enforced)");
    WaitForSingleObject(bad, 0);
    check(handle_exceptions == 2, "WaitForSingleObject raises it");
    SetEvent(bad);
    check(handle_exceptions == 3, "SetEvent raises it");
    CloseHandle(GetCurrentProcess());
    check(handle_exceptions == 3, "a pseudo-handle is not invalid");
    {
        HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
        check(SetEvent(ev) && CloseHandle(ev) && handle_exceptions == 3, "good handles: nothing raised");
    }
}

static void test_acg(BOOL optout)
{
    void *before = VirtualAlloc(NULL, 4096, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    void *data = VirtualAlloc(NULL, 4096, MEM_COMMIT, PAGE_READWRITE), *p;
    HANDLE section;
    DWORD old;

    check(before != NULL, "before: an executable allocation");
    check(set_policy(ProcessDynamicCodePolicy, optout ? 3 : 1), "DynamicCode ProhibitDynamicCode set");

    SetLastError(0xdeadbeef);
    p = VirtualAlloc(NULL, 4096, MEM_COMMIT, PAGE_EXECUTE_READ);
    check(!p && GetLastError() == ERROR_DYNAMIC_CODE_BLOCKED, "no executable allocation (was not enforced)");
    check(!VirtualProtect(data, 4096, PAGE_EXECUTE_READ, &old) && GetLastError() == ERROR_DYNAMIC_CODE_BLOCKED,
          "no data made executable");
    check(!VirtualProtect(before, 4096, PAGE_EXECUTE_READWRITE, &old), "no writable and executable memory");
    check(VirtualProtect(data, 4096, PAGE_READONLY, &old), "data protections still change");
    {
        BYTE *code = (BYTE *)test_acg;
        void *page = (void *)((ULONG_PTR)code & ~(ULONG_PTR)0xfff);
        check(VirtualProtect(page, 1, PAGE_EXECUTE_READ, &old), "an image's code keeps its protection");
        check(!VirtualProtect(page, 1, PAGE_EXECUTE_READWRITE, &old), "an image's code is not made writable+executable");
    }
    section = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_EXECUTE_READWRITE, 0, 4096, NULL);
    p = section ? MapViewOfFile(section, FILE_MAP_READ | FILE_MAP_EXECUTE, 0, 0, 0) : NULL;
    check(section && !p, "no executable view of a data section");
    if (section) CloseHandle(section);
    check(LoadLibraryA("version.dll") != NULL, "an image still loads");

    if (optout)
    {
        DWORD allow = THREAD_DYNAMIC_CODE_ALLOW;
        check(SetThreadInformation(GetCurrentThread(), ThreadDynamicCodePolicy, &allow, sizeof(allow)),
              "the thread opts out");
        p = VirtualAlloc(NULL, 4096, MEM_COMMIT, PAGE_EXECUTE_READ);
        check(p != NULL, "an opted-out thread may allocate code");
    }
}

static void test_sys32(void)
{
    HMODULE mod;
    int (*id)(void);

    check(set_policy(ProcessImageLoadPolicy, 4), "ImageLoad PreferSystem32Images set");
    mod = LoadLibraryA("sgimg.dll");
    id = mod ? (void *)GetProcAddress(mod, "sgimg_id") : NULL;
    check(id && id() == 2, "sgimg.dll comes from the system directory (was the program's)");
}

static void test_sys32_before(void)
{
    HMODULE mod = LoadLibraryA("sgimg.dll");
    int (*id)(void) = mod ? (void *)GetProcAddress(mod, "sgimg_id") : NULL;
    check(id && id() == 1, "without the policy: the program's own sgimg.dll");
}

static void test_fonts(const char *font)
{
    char sysfont[MAX_PATH];
    HANDLE mem;
    DWORD count = 0, size;
    char *buf;
    HANDLE file;

    check(AddFontResourceExA(font, FR_PRIVATE, NULL) > 0, "before: a font beside the program loads");
    RemoveFontResourceExA(font, FR_PRIVATE, NULL);
    check(set_policy(ProcessFontDisablePolicy, 1), "FontDisable DisableNonSystemFonts set");
    check(AddFontResourceExA(font, FR_PRIVATE, NULL) == 0, "after: it does not (was not enforced)");
    file = CreateFileA(font, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    size = GetFileSize(file, NULL);
    buf = malloc(size);
    ReadFile(file, buf, size, &size, NULL);
    CloseHandle(file);
    mem = AddFontMemResourceEx(buf, size, NULL, &count);
    check(!mem, "nor a font from memory");
    GetWindowsDirectoryA(sysfont, MAX_PATH);
    strcat(sysfont, "\\Fonts\\sgsys.ttf");
    check(AddFontResourceExA(sysfont, FR_PRIVATE, NULL) > 0, "a font in the fonts folder still does");
}

static int hook_child(BOOL policy)
{
    char name[64];
    HANDLE event, ready;
    MSG msg;
    DWORD r;

    wsprintfA(name, "sghook-%lu", GetCurrentProcessId());
    event = CreateEventA(NULL, TRUE, FALSE, name);
    if (policy) set_policy(ProcessExtensionPointDisablePolicy, 1);
    PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE);
    ready = OpenEventA(EVENT_MODIFY_STATE, FALSE, "sghook-ready");
    SetEvent(ready);
    /* the parent hooks this thread, then posts */
    while ((r = MsgWaitForMultipleObjects(1, &event, FALSE, 4000, QS_ALLINPUT)) == WAIT_OBJECT_0 + 1)
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
    return r == WAIT_OBJECT_0 ? 1 : (GetModuleHandleA("sghook.dll") ? 3 : 2);
}

static int run_hooked(const char *self, const char *mode)
{
    char cmd[MAX_PATH + 32];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    HANDLE ready = CreateEventA(NULL, FALSE, FALSE, "sghook-ready");
    HMODULE dll = LoadLibraryA("sghook.dll");
    HOOKPROC proc = dll ? (HOOKPROC)GetProcAddress(dll, "sghook_proc") : NULL;
    HHOOK hook = NULL;
    DWORD code = 99;

    sprintf(cmd, "\"%s\" %s", self, mode);
    if (!proc || !CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return 98;
    WaitForSingleObject(ready, 10000);
    hook = SetWindowsHookExA(WH_GETMESSAGE, proc, dll, pi.dwThreadId);
    Sleep(300);
    PostThreadMessageA(pi.dwThreadId, WM_USER, 0, 0);
    WaitForSingleObject(pi.hProcess, 15000);
    GetExitCodeProcess(pi.hProcess, &code);
    if (hook) UnhookWindowsHookEx(hook);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(ready);
    return code;
}

int main(int argc, char **argv)
{
    char self[MAX_PATH], cmd[MAX_PATH + 300];
    static const char *modes[] = { "strict", "acg", "acgopt", "sys32", "fonts" };
    unsigned int i;

    if (argc > 1)
    {
        if (!strcmp(argv[1], "strict")) test_strict();
        else if (!strcmp(argv[1], "acg")) test_acg(FALSE);
        else if (!strcmp(argv[1], "acgopt")) test_acg(TRUE);
        else if (!strcmp(argv[1], "sys32")) test_sys32();
        else if (!strcmp(argv[1], "fonts")) test_fonts(argc > 2 ? argv[2] : "sgfont.ttf");
        else if (!strcmp(argv[1], "hookchild")) return hook_child(FALSE);
        else if (!strcmp(argv[1], "hookchildpolicy")) return hook_child(TRUE);
        return failures != 0;
    }

    GetModuleFileNameA(NULL, self, MAX_PATH);
    for (i = 0; i < ARRAYSIZE(modes); i++)
    {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        DWORD code = 99;

        printf("-- %s\n", modes[i]);
        fflush(stdout);
        sprintf(cmd, "\"%s\" %s", self, modes[i]);
        if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 60000);
            GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        check(code == 0, modes[i]);
    }
    test_sys32_before();
    {
        int r = run_hooked(self, "hookchild");
        check(r == 1, "a hook DLL from another process is loaded and called");
        r = run_hooked(self, "hookchildpolicy");
        check(r == 2, "ExtensionPointDisable: it is not loaded, the hook not called (was not enforced)");
        if (r != 2) printf("      child said %d\n", r);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
