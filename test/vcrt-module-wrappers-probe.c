/* Probe for patches/sg/4526: vcruntime140's __vcrt_GetModuleHandleW,
 * __vcrt_GetModuleFileNameW and __vcrt_LoadLibraryExW against the kernel32
 * functions they wrap. Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(int argc, char **argv)
{
    HMODULE (__cdecl *p_handle)(const WCHAR *), (__cdecl *p_load)(const WCHAR *, HANDLE, DWORD);
    DWORD (__cdecl *p_name)(HMODULE, WCHAR *, DWORD);
    static const WCHAR *names[] = {L"kernel32.dll", L"ntdll", L"vcruntime140.dll", NULL, L"no-such-module.dll"};
    WCHAR a[MAX_PATH], b[MAX_PATH];
    HMODULE mod = LoadLibraryA(argc > 1 ? argv[1] : "vcruntime140"), m;
    unsigned int i;
    DWORD r;

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    p_handle = (void *)GetProcAddress(mod, "__vcrt_GetModuleHandleW");
    p_name = (void *)GetProcAddress(mod, "__vcrt_GetModuleFileNameW");
    p_load = (void *)GetProcAddress(mod, "__vcrt_LoadLibraryExW");
    check(p_handle && p_name && p_load, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    for (i = 0; i < ARRAYSIZE(names); i++)
    {
        m = p_handle(names[i]);
        check(m == GetModuleHandleW(names[i]) && (m != NULL) == (i < 4), "__vcrt_GetModuleHandleW(%ls) = %p",
              names[i] ? names[i] : L"NULL", m);
    }
    r = p_name(mod, a, MAX_PATH);
    check(r && r == GetModuleFileNameW(mod, b, MAX_PATH) && !wcscmp(a, b), "__vcrt_GetModuleFileNameW: %ls", a);
    r = p_name(mod, a, 3);
    check(r == 3 && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "short buffer: %lu", r);
    m = p_load(L"version.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    check(m && m == GetModuleHandleW(L"version.dll"), "__vcrt_LoadLibraryExW(version.dll) %p", m);
    FreeLibrary(m);
    m = p_load(L"no-such-module.dll", NULL, 0);
    check(!m && GetLastError() == ERROR_MOD_NOT_FOUND, "missing module: %p %lu", m, GetLastError());

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
