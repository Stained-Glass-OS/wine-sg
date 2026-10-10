/* NtQueryInformationProcess: ProcessIoPriority, ProcessProtectionInformation,
 * ProcessCommandLineInformation (patches/sg/2238), own and another process. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

#define ProcessIoPriority 33
#define ProcessCommandLineInformation 60
#define ProcessProtectionInformation 61
#define LENGTH_MISMATCH ((LONG)0xC0000004)
#define ACCESS_DENIED   ((LONG)0xC0000022)
#define INVALID_HANDLE  ((LONG)0xC0000008)

static LONG (WINAPI *pQuery)(HANDLE, ULONG, void *, ULONG, ULONG *);

static void check_cmdline(HANDLE process, const WCHAR *expect, const char *what)
{
    UNICODE_STRING *str;
    ULONG len = 0, need;
    LONG st;
    char *buf;

    st = pQuery(process, ProcessCommandLineInformation, NULL, 0, &len);
    CHECK(st == LENGTH_MISMATCH && len >= sizeof(UNICODE_STRING), "%s: size query st %#lx len %lu", what, st, len);
    need = len;
    buf = malloc(need + 16);
    memset(buf, 0xcc, need + 16);
    st = pQuery(process, ProcessCommandLineInformation, buf, need - 1, &len);
    CHECK(st == LENGTH_MISMATCH && len == need, "%s: one byte short st %#lx len %lu/%lu", what, st, len, need);
    st = pQuery(process, ProcessCommandLineInformation, buf, need, &len);
    str = (UNICODE_STRING *)buf;
    CHECK(!st && len == need, "%s: st %#lx len %lu/%lu", what, st, len, need);
    CHECK(!st && str->Length == wcslen(expect) * sizeof(WCHAR) && str->MaximumLength == str->Length + sizeof(WCHAR),
          "%s: lengths %u %u", what, str->Length, str->MaximumLength);
    CHECK(!st && (char *)str->Buffer == buf + sizeof(UNICODE_STRING) && !wcscmp(str->Buffer, expect),
          "%s: text %ls", what, !st ? str->Buffer : L"?");
    CHECK(((BYTE *)buf)[need] == 0xcc, "%s: wrote past the end", what);
    free(buf);
}

int main(int argc, char **argv)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    char path[MAX_PATH], other[MAX_PATH + 32];
    WCHAR cmdW[MAX_PATH * 2 + 64];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    ULONG len, prio;
    BYTE prot;
    LONG st;
    HANDLE h;

    if (argc > 1 && !strcmp(argv[1], "child")) { Sleep(20000); return 0; }

    pQuery = (void *)GetProcAddress(ntdll, "NtQueryInformationProcess");

    /* I/O priority and protection of this process */
    prio = 0xdead;
    st = pQuery(GetCurrentProcess(), ProcessIoPriority, &prio, sizeof(prio), &len);
    CHECK(!st && prio == 2 && len == sizeof(ULONG), "io priority st %#lx prio %lu len %lu", st, prio, len);
    st = pQuery(GetCurrentProcess(), ProcessIoPriority, &prio, 2, &len);
    CHECK(st == LENGTH_MISMATCH, "io priority short st %#lx", st);
    st = pQuery(GetCurrentProcess(), ProcessIoPriority, &prio, 8, &len);
    CHECK(st == LENGTH_MISMATCH, "io priority long st %#lx", st);
    prot = 0x55;
    st = pQuery(GetCurrentProcess(), ProcessProtectionInformation, &prot, 1, &len);
    CHECK(!st && prot == 0 && len == 1, "protection st %#lx prot %u len %lu", st, prot, len);
    st = pQuery(GetCurrentProcess(), ProcessProtectionInformation, &prot, 4, &len);
    CHECK(st == LENGTH_MISMATCH, "protection long st %#lx", st);
    st = pQuery((HANDLE)(ULONG_PTR)0xdeadbeef, ProcessIoPriority, &prio, sizeof(prio), &len);
    CHECK(st == INVALID_HANDLE, "bad handle st %#lx", st);

    /* the command line of this process */
    check_cmdline(GetCurrentProcess(), GetCommandLineW(), "self");

    /* and of a child of the other bitness */
    GetModuleFileNameA(NULL, path, sizeof(path));
    snprintf(other, sizeof(other), "\"%s\" child", argc > 1 ? argv[1] : path);
    if (argc > 1) { /* the other-bitness binary was passed in */ }
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(NULL, other, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        printf("FAIL  could not start child: %lu\n", GetLastError());
        fails++;
    }
    else
    {
        MultiByteToWideChar(CP_ACP, 0, other, -1, cmdW, (int)(sizeof(cmdW)/sizeof(cmdW[0])));
        Sleep(500);
        check_cmdline(pi.hProcess, cmdW, "child");
        h = OpenProcess(SYNCHRONIZE, FALSE, pi.dwProcessId);
        if (h)
        {
            len = 0;
            st = pQuery(h, ProcessCommandLineInformation, NULL, 0, &len);
            CHECK(st == ACCESS_DENIED, "no query right st %#lx", st);
            CloseHandle(h);
        }
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
