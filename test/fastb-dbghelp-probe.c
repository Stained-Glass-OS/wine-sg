/* dbghelp, fast mode (patch 3536+): SymEnumProcesses, home directory, SymFindExecutableImage / FindDebugInfoFileEx
 * with callbacks, SymNext / SymPrev / SymGetSymNext / SymGetSymPrev, SymEnumSym, SymEnumerateSymbolsW64,
 * SymEnumSymbolsForAddr. */
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef BOOL (CALLBACK *enum_proc_cb)(HANDLE, PVOID);
static BOOL (WINAPI *pSymEnumProcesses)(enum_proc_cb, PVOID);
static int procs;
static BOOL CALLBACK proc_cb(HANDLE h, PVOID ctx) { if (h == GetCurrentProcess() || h) procs++; return TRUE; }
static int syms;
static BOOL CALLBACK sym_cb(PSYMBOL_INFO si, ULONG size, PVOID ctx) { syms++; return TRUE; }
static BOOL CALLBACK sym_cb64w(PCWSTR name, DWORD64 addr, ULONG size, PVOID ctx) { syms++; return TRUE; }
static int cb_seen;
static BOOL CALLBACK exe_cb(HANDLE h, PCSTR name, PVOID ctx) { cb_seen++; return *(BOOL *)ctx; }

int main(void)
{
    HANDLE process = GetCurrentProcess();
    char buf[sizeof(SYMBOL_INFO) + 256], path[MAX_PATH], home[MAX_PATH];
    SYMBOL_INFO *si = (SYMBOL_INFO *)buf;
    DWORD64 addr = (DWORD64)(ULONG_PTR)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetCurrentProcessId"), disp;
    HANDLE h;
    BOOL accept;
    char symbuf[sizeof(IMAGEHLP_SYMBOL64) + 128];
    IMAGEHLP_SYMBOL64 *sym64 = (IMAGEHLP_SYMBOL64 *)symbuf;
    DWORD64 first;

    pSymEnumProcesses = (void *)GetProcAddress(LoadLibraryA("dbghelp.dll"), "SymEnumProcesses");
    SymSetOptions(SYMOPT_UNDNAME);
    check(SymInitialize(process, NULL, TRUE), "SymInitialize");

    procs = 0;
    check(pSymEnumProcesses(proc_cb, NULL) && procs == 1, "SymEnumProcesses lists this process");
    check(!pSymEnumProcesses(NULL, NULL), "SymEnumProcesses without callback fails");

    SymSetHomeDirectory(process, "C:\\dbg");
    check(SymGetHomeDirectory(hdBase, home, sizeof(home)) && !strcmp(home, "C:\\dbg"), "home directory base");
    check(SymGetHomeDirectory(hdSym, home, sizeof(home)) && !strcmp(home, "C:\\dbg\\sym"), "home directory sym");
    check(SymGetHomeDirectory(hdSrc, home, sizeof(home)) && !strcmp(home, "C:\\dbg\\src"), "home directory src");
    check(SymGetHomeDirectory(7, home, sizeof(home)) == NULL, "bad home directory type");

    check(SymSetParentWindow(NULL), "SymSetParentWindow");

    SymSetSearchPath(process, "C:\\windows\\system32");
    accept = TRUE; cb_seen = 0;
    h = SymFindExecutableImage(process, "kernel32.dll", path, exe_cb, &accept);
    check(h && h != INVALID_HANDLE_VALUE && cb_seen == 1, "SymFindExecutableImage finds kernel32 and asks the callback");
    if (h) CloseHandle(h);
    accept = FALSE; cb_seen = 0;
    h = SymFindExecutableImage(process, "kernel32.dll", path, exe_cb, &accept);
    check(h == NULL && cb_seen == 1, "a callback that refuses ends the search");

    memset(buf, 0, sizeof(buf));
    si->SizeOfStruct = sizeof(*si);
    si->MaxNameLen = 256;
    check(SymFromAddr(process, addr, &disp, si), "SymFromAddr kernel32 export");
    first = si->Address;

    memset(buf, 0, sizeof(buf));
    si->SizeOfStruct = sizeof(*si);
    si->MaxNameLen = 256;
    si->Address = first;
    check(SymNext(process, si) && si->Address > first, "SymNext is above");
    memset(buf, 0, sizeof(buf));
    si->SizeOfStruct = sizeof(*si);
    si->MaxNameLen = 256;
    si->Address = first;
    check(SymPrev(process, si) && si->Address < first, "SymPrev is below");

    memset(symbuf, 0, sizeof(symbuf));
    sym64->SizeOfStruct = sizeof(*sym64);
    sym64->MaxNameLength = 128;
    sym64->Address = first;
    check(SymGetSymNext64(process, sym64) && sym64->Address > first && sym64->Name[0], "SymGetSymNext64");
    memset(symbuf, 0, sizeof(symbuf));
    sym64->SizeOfStruct = sizeof(*sym64);
    sym64->MaxNameLength = 128;
    sym64->Address = first;
    check(SymGetSymPrev64(process, sym64) && sym64->Address < first, "SymGetSymPrev64");

    syms = 0;
    check(SymEnumSym(process, SymGetModuleBase64(process, addr), sym_cb, NULL) && syms > 10, "SymEnumSym");
    syms = 0;
    check(SymEnumerateSymbolsW64(process, SymGetModuleBase64(process, addr), sym_cb64w, NULL) && syms > 10,
          "SymEnumerateSymbolsW64");
    syms = 0;
    check(SymEnumSymbolsForAddr(process, addr, sym_cb, NULL) && syms == 1, "SymEnumSymbolsForAddr");

    SymCleanup(process);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
