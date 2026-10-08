/* What a process is created with is kept and held to (patches/sg/1641).
 *
 * CreateProcess's PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY and
 * PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY were dropped ("Unsupported
 * attribute"; Chromium and Edge start every sandboxed process with them), so
 * the child reported no mitigation policies and a renderer told to create no
 * processes could. Now:
 *  - the child reports the policies it was created with
 *    (GetProcessMitigationPolicy), and so does its parent asking about it;
 *  - a process created with CHILD_PROCESS_RESTRICTED, or that set
 *    NoChildProcessCreation on itself, cannot create processes
 *    (ERROR_CHILD_PROCESS_BLOCKED), and cannot lift that;
 *  - the component filter and audit policy attributes are accepted.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifndef ERROR_CHILD_PROCESS_BLOCKED
#define ERROR_CHILD_PROCESS_BLOCKED 367
#endif
#define ATTR_MITIGATION   0x20007
#define ATTR_CHILD_POLICY 0x2000e
#define ATTR_COMPONENT    0x2001a
#define ATTR_AUDIT        0x20018
#define CHILD_RESTRICTED  0x01
#define PROHIBIT_DYNAMIC_CODE_ON (1ull << 36)
#define IMAGE_LOAD_NO_REMOTE_ON  (1ull << 52)
#define FONT_DISABLE_ON          (1ull << 48)
#define BOTTOM_UP_ASLR_ON        (1ull << 16)

enum { pol_aslr = 1, pol_dynamic_code = 2, pol_font = 9, pol_image_load = 10, pol_child = 13 };

typedef BOOL (WINAPI *get_mitigation_t)(HANDLE, int, void *, SIZE_T);
typedef BOOL (WINAPI *set_mitigation_t)(int, void *, SIZE_T);
static get_mitigation_t pGetProcessMitigationPolicy;
static set_mitigation_t pSetProcessMitigationPolicy;
static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static DWORD policy_of(HANDLE process, int policy)
{
    DWORD flags = 0xdeadbeef;
    if (!pGetProcessMitigationPolicy(process, policy, &flags, sizeof(flags))) return 0xdeadbeef;
    return flags;
}

/* creates a process; 0, or the error */
static DWORD try_spawn(void)
{
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char cmd[] = "cmd.exe /c exit 0";

    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) return GetLastError();
    WaitForSingleObject(pi.hProcess, 20000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}

/* runs argv[0] with a mode and the given attributes; returns its exit code */
static DWORD run_child(const char *argv0, const char *mode, ULONG64 mitigation, DWORD child_policy,
                       BOOL extra_attrs, DWORD *policy_seen_from_parent)
{
    STARTUPINFOEXA si = {{ sizeof(si) }};
    PROCESS_INFORMATION pi;
    SIZE_T size = 0;
    DWORD code = 0xdead, component = 1 /* COMPONENT_KTM */, n = 0;
    ULONG64 audit[2] = { 0, 0 };
    char cmd[MAX_PATH + 64];

    if (mitigation) n++;
    if (child_policy) n++;
    if (extra_attrs) n += 2;
    InitializeProcThreadAttributeList(NULL, n, 0, &size);
    si.lpAttributeList = HeapAlloc(GetProcessHeap(), 0, size);
    InitializeProcThreadAttributeList(si.lpAttributeList, n, 0, &size);
    if (mitigation) UpdateProcThreadAttribute(si.lpAttributeList, 0, ATTR_MITIGATION, &mitigation, sizeof(mitigation), NULL, NULL);
    if (child_policy) UpdateProcThreadAttribute(si.lpAttributeList, 0, ATTR_CHILD_POLICY, &child_policy, sizeof(child_policy), NULL, NULL);
    if (extra_attrs)
    {
        UpdateProcThreadAttribute(si.lpAttributeList, 0, ATTR_COMPONENT, &component, sizeof(component), NULL, NULL);
        UpdateProcThreadAttribute(si.lpAttributeList, 0, ATTR_AUDIT, audit, sizeof(audit), NULL, NULL);
    }
    sprintf(cmd, "\"%s\" %s", argv0, mode);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED,
                        NULL, NULL, &si.StartupInfo, &pi))
    {
        printf("CreateProcess(%s) failed: %lu\n", mode, GetLastError());
        return 0xdead;
    }
    if (policy_seen_from_parent) *policy_seen_from_parent = policy_of(pi.hProcess, pol_dynamic_code);
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, 60000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    DeleteProcThreadAttributeList(si.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
    return code;
}

int main(int argc, char **argv)
{
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    DWORD code, seen, flags, err;

    pGetProcessMitigationPolicy = (void *)GetProcAddress(k32, "GetProcessMitigationPolicy");
    pSetProcessMitigationPolicy = (void *)GetProcAddress(k32, "SetProcessMitigationPolicy");

    if (argc > 1 && !strcmp(argv[1], "restricted"))
    {
        /* created with the policies: report them, and create no process */
        DWORD child = policy_of(GetCurrentProcess(), pol_child), dyn = policy_of(GetCurrentProcess(), pol_dynamic_code);
        DWORD img = policy_of(GetCurrentProcess(), pol_image_load), font = policy_of(GetCurrentProcess(), pol_font);
        DWORD aslr = policy_of(GetCurrentProcess(), pol_aslr);
        err = try_spawn();
        printf("child: child %#lx dynamic %#lx image %#lx font %#lx aslr %#lx spawn %lu\n", child, dyn, img, font, aslr, err);
        flags = 0;
        SetLastError(0xdeadbeef);
        if (pSetProcessMitigationPolicy(pol_child, &flags, sizeof(flags))) return 9;   /* lifted: wrong */
        if (child != 1 || dyn != 1 || img != 1 || font != 1 || !(aslr & 1)) return 2;
        if (err != ERROR_CHILD_PROCESS_BLOCKED) return 3;
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "self"))
    {
        /* restricts itself */
        err = try_spawn();
        if (err) return 4;   /* could not before */
        flags = 1;
        if (!pSetProcessMitigationPolicy(pol_child, &flags, sizeof(flags))) return 5;
        if (policy_of(GetCurrentProcess(), pol_child) != 1) return 6;
        err = try_spawn();
        printf("self: spawn after NoChildProcessCreation: %lu\n", err);
        return err == ERROR_CHILD_PROCESS_BLOCKED ? 0 : 7;
    }
    if (argc > 1 && !strcmp(argv[1], "plain"))
    {
        /* no policies: none reported, processes may be created */
        if (policy_of(GetCurrentProcess(), pol_child) != 0 || policy_of(GetCurrentProcess(), pol_dynamic_code) != 0) return 2;
        return try_spawn() ? 3 : 0;
    }

    code = run_child(argv[0], "restricted", PROHIBIT_DYNAMIC_CODE_ON | IMAGE_LOAD_NO_REMOTE_ON | FONT_DISABLE_ON | BOTTOM_UP_ASLR_ON,
                     CHILD_RESTRICTED, FALSE, &seen);
    printf("restricted child: exit %lu, its dynamic code policy seen by the parent %#lx\n", code, seen);
    check(code == 0, "a child created with mitigation and child process policies reports them and creates no process (ERROR_CHILD_PROCESS_BLOCKED), nor lifts it");
    check(seen == 1, "the parent reads the child's policy (ProhibitDynamicCode)");

    code = run_child(argv[0], "self", 0, 0, FALSE, NULL);
    printf("self-restricting child: exit %lu\n", code);
    check(code == 0, "a process that sets NoChildProcessCreation on itself creates no more processes");

    code = run_child(argv[0], "plain", 0, 0, TRUE, NULL);
    printf("plain child with the component filter and audit attributes: exit %lu\n", code);
    check(code == 0, "the component filter and audit policy attributes are accepted; no policies, processes allowed");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
