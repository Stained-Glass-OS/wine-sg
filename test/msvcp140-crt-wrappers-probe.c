/* Probe for patches/sg/4534: msvcp140's __crt* kernel32 wrappers
 * (CompareStringEx, LCMapStringEx, GetLocaleInfoEx,
 * GetSystemTimePreciseAsFileTime, InitOnceExecuteOnce, IsPackagedApp,
 * CreateSymbolicLinkW, Get/SetFileInformationByHandle(Ex)) against the
 * functions they wrap. Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define _WIN32_WINNT 0x0602
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static LONG once_calls;
static BOOL CALLBACK once_fn(INIT_ONCE *once, void *param, void **ctx)
{
    InterlockedIncrement(&once_calls);
    *ctx = param;
    return TRUE;
}

int main(int argc, char **argv)
{
    HMODULE mod = LoadLibraryA(argc > 1 ? argv[1] : "msvcp140");
    int (__cdecl *p_cmp)(const WCHAR *, DWORD, const WCHAR *, int, const WCHAR *, int);
    int (__cdecl *p_map)(const WCHAR *, DWORD, const WCHAR *, int, WCHAR *, int);
    int (__cdecl *p_info)(const WCHAR *, DWORD, WCHAR *, int);
    void (__cdecl *p_time)(FILETIME *);
    BOOL (__cdecl *p_once)(INIT_ONCE *, PINIT_ONCE_FN, void *, void **);
    BOOL (__cdecl *p_pkg)(void);
    BOOLEAN (__cdecl *p_link)(const WCHAR *, const WCHAR *, DWORD);
    BOOL (__cdecl *p_get)(HANDLE, FILE_INFO_BY_HANDLE_CLASS, void *, DWORD), (__cdecl *p_set)(HANDLE, FILE_INFO_BY_HANDLE_CLASS, void *, DWORD);
    WCHAR buf[64], buf2[64];
    int r;

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
#define GET(v, n) v = (void *)GetProcAddress(mod, n); check(v != NULL, "%s exported", n)
    GET(p_cmp, "__crtCompareStringEx"); GET(p_map, "__crtLCMapStringEx"); GET(p_info, "__crtGetLocaleInfoEx");
    GET(p_time, "__crtGetSystemTimePreciseAsFileTime"); GET(p_once, "__crtInitOnceExecuteOnce");
    GET(p_pkg, "__crtIsPackagedApp"); GET(p_link, "__crtCreateSymbolicLinkW");
    GET(p_get, "__crtGetFileInformationByHandleEx"); GET(p_set, "__crtSetFileInformationByHandle");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    check(p_cmp(L"en-US", NORM_IGNORECASE, L"abc", -1, L"ABC", -1) == CSTR_EQUAL &&
          p_cmp(L"en-US", 0, L"a", -1, L"b", -1) == CSTR_LESS_THAN, "__crtCompareStringEx");
    r = p_map(L"en-US", LCMAP_UPPERCASE, L"mixed", -1, buf, 64);
    check(r == 6 && !wcscmp(buf, L"MIXED"), "__crtLCMapStringEx: %d %ls", r, buf);
    r = p_info(L"en-US", LOCALE_SENGLANGUAGE, buf, 64);
    GetLocaleInfoEx(L"en-US", LOCALE_SENGLANGUAGE, buf2, 64);
    check(r > 1 && !wcscmp(buf, buf2), "__crtGetLocaleInfoEx: %d %ls", r, buf);
    r = p_info(L"de-DE", LOCALE_SDECIMAL, buf, 64);
    check(r == 2 && !wcscmp(buf, L","), "__crtGetLocaleInfoEx de-DE decimal: %d %ls", r, buf);
    {
        FILETIME a, b, c;
        ULONGLONG ua, ub, uc;
        GetSystemTimeAsFileTime(&a); p_time(&b); Sleep(20); GetSystemTimeAsFileTime(&c);
        ua = ((ULONGLONG)a.dwHighDateTime << 32) | a.dwLowDateTime;
        ub = ((ULONGLONG)b.dwHighDateTime << 32) | b.dwLowDateTime;
        uc = ((ULONGLONG)c.dwHighDateTime << 32) | c.dwLowDateTime;
        check(ub + 200000 >= ua && ub <= uc, "__crtGetSystemTimePreciseAsFileTime within the system time");
    }
    {
        INIT_ONCE once = INIT_ONCE_STATIC_INIT;
        void *ctx = NULL;
        check(p_once(&once, once_fn, (void *)0x40, &ctx) && p_once(&once, once_fn, (void *)0x80, &ctx) && once_calls == 1,
              "__crtInitOnceExecuteOnce runs once (%ld)", once_calls);
    }
    check(!p_pkg(), "__crtIsPackagedApp: not packaged");
    check(!p_link(L"C:\\no\\such\\dir\\link", L"C:\\windows", 0) && GetLastError() == ERROR_PATH_NOT_FOUND,
          "__crtCreateSymbolicLinkW into a missing directory");
    {
        char path[MAX_PATH];
        HANDLE f;
        FILE_END_OF_FILE_INFO eof;
        FILE_STANDARD_INFO std;

        GetTempPathA(sizeof(path), path);
        strcat(path, "msvcp140-crt.tmp");
        f = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
        eof.EndOfFile.QuadPart = 4242;
        check(p_set(f, FileEndOfFileInfo, &eof, sizeof(eof)) && GetFileSize(f, NULL) == 4242, "__crtSetFileInformationByHandle");
        memset(&std, 0, sizeof(std));
        check(p_get(f, FileStandardInfo, &std, sizeof(std)) && std.EndOfFile.QuadPart == 4242, "__crtGetFileInformationByHandleEx");
        CloseHandle(f);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
