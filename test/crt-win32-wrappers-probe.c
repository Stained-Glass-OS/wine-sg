/* Probe for patches/sg/4518: msvcr110/msvcr120's __crt* wrappers of kernel32
 * functions (CompareStringEx, LCMapStringEx, EnumSystemLocalesEx, date and
 * time formats, locale names, fiber local storage, SetThreadStackGuarantee,
 * CreateSymbolicLinkW, CreateSemaphoreExW, CreateEventExW, GetTickCount64,
 * file information by handle) and __crtIsPackagedApp, each against the
 * function it wraps. Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static HMODULE mod;
static void *get(const char *name, int expected)
{
    void *p = GetProcAddress(mod, name);
    check(!p == !expected, "%s %s", name, expected ? "exported" : "not exported here");
    return p;
}

static int locales, en_us;
static BOOL CALLBACK enum_proc(WCHAR *name, DWORD flags, LPARAM param)
{
    locales++;
    if (!wcscmp(name, L"en-US")) en_us++;
    return TRUE;
}

int main(int argc, char **argv)
{
    int (__cdecl *p_compare)(const WCHAR *, DWORD, const WCHAR *, int, const WCHAR *, int);
    int (__cdecl *p_lcmap)(const WCHAR *, DWORD, const WCHAR *, int, WCHAR *, int);
    BOOL (__cdecl *p_enum)(LOCALE_ENUMPROCEX, DWORD, LPARAM);
    int (__cdecl *p_date)(const WCHAR *, DWORD, const SYSTEMTIME *, const WCHAR *, WCHAR *, int, const WCHAR *);
    int (__cdecl *p_time)(const WCHAR *, DWORD, const SYSTEMTIME *, const WCHAR *, WCHAR *, int);
    int (__cdecl *p_username)(WCHAR *, int);
    BOOL (__cdecl *p_valid)(const WCHAR *);
    DWORD (__cdecl *p_flsalloc)(void *);
    BOOL (__cdecl *p_flsfree)(DWORD), (__cdecl *p_flsset)(DWORD, void *);
    void *(__cdecl *p_flsget)(DWORD);
    BOOL (__cdecl *p_guarantee)(ULONG *);
    BOOLEAN (__cdecl *p_symlink)(const WCHAR *, const WCHAR *, DWORD);
    HANDLE (__cdecl *p_sem)(SECURITY_ATTRIBUTES *, LONG, LONG, const WCHAR *, DWORD, DWORD);
    BOOL (__cdecl *p_packaged)(void);
    int is120, is64 = sizeof(void *) == 8;
    WCHAR buf[64], buf2[64];
    SYSTEMTIME st = {2024, 2, 4, 29, 13, 5, 9, 0};
    DWORD idx;
    ULONG size, size2;
    int r;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    is120 = !strcmp(argv[1], "msvcr120");

    p_compare = get("__crtCompareStringEx", 1);
    p_lcmap = get("__crtLCMapStringEx", 1);
    p_enum = get("__crtEnumSystemLocalesEx", 1);
    p_date = get("__crtGetDateFormatEx", 1);
    p_time = get("__crtGetTimeFormatEx", 1);
    p_username = get("__crtGetUserDefaultLocaleName", 1);
    p_valid = get("__crtIsValidLocaleName", 1);
    p_flsalloc = get("__crtFlsAlloc", 1); p_flsfree = get("__crtFlsFree", 1);
    p_flsget = get("__crtFlsGetValue", 1); p_flsset = get("__crtFlsSetValue", 1);
    p_guarantee = get("__crtSetThreadStackGuarantee", 1);
    p_symlink = get("__crtCreateSymbolicLinkW", is120 || is64);
    p_sem = get(is120 ? "__crtCreateSemaphoreExW" : "___crtCreateSemaphoreExW", !is64);
    p_packaged = get("__crtIsPackagedApp", 1);
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    {
        static const struct { const WCHAR *a, *b; DWORD flags; int want; } cmp[] =
        {
            {L"abc", L"ABC", NORM_IGNORECASE, CSTR_EQUAL}, {L"abc", L"ABC", 0, CSTR_LESS_THAN},
            {L"a", L"b", 0, CSTR_LESS_THAN}, {L"b", L"a", 0, CSTR_GREATER_THAN}, {L"same", L"same", 0, CSTR_EQUAL},
        };
        unsigned int i;

        for (i = 0; i < ARRAYSIZE(cmp); i++)
        {
            r = p_compare(L"en-US", cmp[i].flags, cmp[i].a, -1, cmp[i].b, -1);
            check(r == cmp[i].want && r == CompareStringEx(L"en-US", cmp[i].flags, cmp[i].a, -1, cmp[i].b, -1, NULL, NULL, 0),
                  "__crtCompareStringEx(%ls, %ls, %#lx) = %d", cmp[i].a, cmp[i].b, cmp[i].flags, r);
        }
    }
    r = p_lcmap(L"en-US", LCMAP_UPPERCASE, L"mixed Case", -1, buf, ARRAYSIZE(buf));
    check(r == 11 && !wcscmp(buf, L"MIXED CASE"), "__crtLCMapStringEx upper case: %d %ls", r, buf);
    r = p_lcmap(L"en-US", LCMAP_LOWERCASE, L"ABC", 3, buf, ARRAYSIZE(buf));
    check(r == 3 && !wcsncmp(buf, L"abc", 3), "__crtLCMapStringEx lower case: %d", r);

    check(p_enum(enum_proc, LOCALE_ALL, 0) && locales > 10 && en_us == 1, "__crtEnumSystemLocalesEx: %d locales", locales);

    r = p_date(L"en-US", 0, &st, L"yyyy-MM-dd", buf, ARRAYSIZE(buf), NULL);
    check(r == 11 && !wcscmp(buf, L"2024-02-29"), "__crtGetDateFormatEx: %d %ls", r, buf);
    r = p_time(L"en-US", 0, &st, L"HH:mm:ss", buf, ARRAYSIZE(buf));
    check(r == 9 && !wcscmp(buf, L"13:05:09"), "__crtGetTimeFormatEx: %d %ls", r, buf);

    r = p_username(buf, ARRAYSIZE(buf));
    GetUserDefaultLocaleName(buf2, ARRAYSIZE(buf2));
    check(r > 1 && !wcscmp(buf, buf2), "__crtGetUserDefaultLocaleName: %d %ls", r, buf);
    check(p_valid(L"en-US") && !p_valid(L"xx-NOT-A-LOCALE"), "__crtIsValidLocaleName");

    idx = p_flsalloc(NULL);
    check(idx != FLS_OUT_OF_INDEXES, "__crtFlsAlloc %lu", idx);
    check(p_flsset(idx, (void *)0x1234) && p_flsget(idx) == (void *)0x1234 && FlsGetValue(idx) == (void *)0x1234,
          "__crtFlsSetValue/__crtFlsGetValue");
    check(p_flsfree(idx), "__crtFlsFree");

    size = 0; size2 = 0;
    check(p_guarantee(&size) && SetThreadStackGuarantee(&size2) && size == size2,
          "__crtSetThreadStackGuarantee query: %lu (%lu)", size, size2);

    if (p_symlink)
    {
        BOOLEAN ret = p_symlink(L"C:\\no\\such\\dir\\link", L"C:\\windows", 0);
        check(!ret && GetLastError() == ERROR_PATH_NOT_FOUND, "__crtCreateSymbolicLinkW into a missing directory: %d %lu",
              ret, GetLastError());
    }
    if (p_sem)
    {
        HANDLE h = p_sem(NULL, 1, 2, NULL, 0, SEMAPHORE_ALL_ACCESS);
        LONG prev = -1;

        check(h && !WaitForSingleObject(h, 0) && WaitForSingleObject(h, 0) == WAIT_TIMEOUT &&
              ReleaseSemaphore(h, 2, &prev) && prev == 0, "__crtCreateSemaphoreExW count");
        CloseHandle(h);
    }
    check(!p_packaged(), "__crtIsPackagedApp: not packaged");

    if (is120)
    {
        HANDLE (__cdecl *p_event)(SECURITY_ATTRIBUTES *, const WCHAR *, DWORD, DWORD) = get("__crtCreateEventExW", 1);
        ULONGLONG (__cdecl *p_tick)(void) = get("__crtGetTickCount64", 1);
        BOOL (__cdecl *p_getinfo)(HANDLE, FILE_INFO_BY_HANDLE_CLASS, void *, DWORD) = get("__crtGetFileInformationByHandleEx", 1);
        BOOL (__cdecl *p_setinfo)(HANDLE, FILE_INFO_BY_HANDLE_CLASS, void *, DWORD) = get("__crtSetFileInformationByHandle", 1);
        char path[MAX_PATH];
        HANDLE h, file;
        ULONGLONG t1, t2, t3;

        if (p_event && p_tick && p_getinfo && p_setinfo)
        {
            h = p_event(NULL, NULL, CREATE_EVENT_MANUAL_RESET | CREATE_EVENT_INITIAL_SET, EVENT_ALL_ACCESS);
            check(h && !WaitForSingleObject(h, 0) && !WaitForSingleObject(h, 0), "__crtCreateEventExW manual reset, set");
            CloseHandle(h);
            h = p_event(NULL, NULL, 0, EVENT_ALL_ACCESS);
            check(h && WaitForSingleObject(h, 0) == WAIT_TIMEOUT && SetEvent(h) && !WaitForSingleObject(h, 0) &&
                  WaitForSingleObject(h, 0) == WAIT_TIMEOUT, "__crtCreateEventExW auto reset");
            CloseHandle(h);

            t1 = GetTickCount64(); t2 = p_tick(); t3 = GetTickCount64();
            check(t2 >= t1 && t2 <= t3, "__crtGetTickCount64 %I64u in [%I64u, %I64u]", t2, t1, t3);

            GetTempPathA(sizeof(path), path);
            strcat(path, "crt-wrappers.tmp");
            file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
            {
                FILE_END_OF_FILE_INFO eof;
                FILE_STANDARD_INFO std;

                eof.EndOfFile.QuadPart = 12345;
                check(p_setinfo(file, FileEndOfFileInfo, &eof, sizeof(eof)) && GetFileSize(file, NULL) == 12345,
                      "__crtSetFileInformationByHandle end of file");
                memset(&std, 0, sizeof(std));
                check(p_getinfo(file, FileStandardInfo, &std, sizeof(std)) && std.EndOfFile.QuadPart == 12345 && !std.Directory,
                      "__crtGetFileInformationByHandleEx standard info: %I64d", std.EndOfFile.QuadPart);
            }
            CloseHandle(file);
        }
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
