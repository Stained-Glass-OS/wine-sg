/* Probe for patches/sg/4516: console line input of the DLL named on the
 * command line: _cgets_s, _cgetws_s (msvcr80 and later, ucrtbase), _cgetws
 * (msvcr70 and later) and ucrtbase's __conio_common_vcscanf/vcwscanf.
 * The probe runs itself again under a pseudo console (headless conhost),
 * types the input lines into it and passes on what the child wrote to a
 * result file. Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define _WIN32_WINNT 0x0a00
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <errno.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static int invalid;
static void __cdecl handler(const wchar_t *e, const wchar_t *f, const wchar_t *fn, unsigned int l, uintptr_t r) { invalid++; }

/* the lines typed for each kind of DLL, in the order the child reads them */
static const char *lines_s[] = {"hello", "wide line", "third", ""};
static const char *lines_ucrt[] = {"hello", "wide line", "third", "", "42 word", "wq"};
static const char *lines_old[] = {"third"};

static void lines_for(const char *dll, const char ***lines, unsigned int *count)
{
    if (!strcmp(dll, "ucrtbase")) { *lines = lines_ucrt; *count = ARRAYSIZE(lines_ucrt); }
    else if (!strcmp(dll, "msvcr70") || !strcmp(dll, "msvcr71")) { *lines = lines_old; *count = ARRAYSIZE(lines_old); }
    else { *lines = lines_s; *count = ARRAYSIZE(lines_s); }
}

/* older mingw headers lack the pseudo console API */
typedef void *pcon_t;
#define ATTRIBUTE_PSEUDOCONSOLE 0x00020016

static DWORD WINAPI drain(void *pipe)
{
    char buf[4096];
    DWORD got;

    while (ReadFile(pipe, buf, sizeof(buf), &got, NULL) && got);
    return 0;
}

static int run_child(const char *dll)
{
    STARTUPINFOEXA si = {{sizeof(si)}};
    PROCESS_INFORMATION pi;
    HANDLE in_r, in_w, out_r, out_w, thread, result;
    char cmd[2 * MAX_PATH + 64], path[MAX_PATH], self[MAX_PATH], buf[4096];
    HRESULT (WINAPI *create)(COORD, HANDLE, HANDLE, DWORD, pcon_t *);
    void (WINAPI *close)(pcon_t);
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    const char **lines;
    unsigned int count, i;
    SIZE_T size = 0;
    COORD dim = {80, 25};
    pcon_t pcon;
    DWORD got, code = 1;

    GetModuleFileNameA(NULL, self, sizeof(self));
    GetTempPathA(sizeof(path), path);
    strcat(path, "crt-cgets-probe.res");
    create = (void *)GetProcAddress(k32, "CreatePseudoConsole");
    close = (void *)GetProcAddress(k32, "ClosePseudoConsole");
    if (!create || !close || !CreatePipe(&in_r, &in_w, NULL, 0) || !CreatePipe(&out_r, &out_w, NULL, 0) ||
        FAILED(create(dim, in_r, out_w, 0, &pcon)))
    {
        printf("FAIL pseudo console\nRESULT: FAIL\n");
        return 1;
    }
    thread = CreateThread(NULL, 0, drain, out_r, 0, NULL);
    InitializeProcThreadAttributeList(NULL, 1, 0, &size);
    si.lpAttributeList = HeapAlloc(GetProcessHeap(), 0, size);
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &size);
    UpdateProcThreadAttribute(si.lpAttributeList, 0, ATTRIBUTE_PSEUDOCONSOLE, pcon, sizeof(pcon), NULL, NULL);
    snprintf(cmd, sizeof(cmd), "\"%s\" %s child \"%s\"", self, dll, path);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &pi))
    {
        printf("FAIL CreateProcess %lu\nRESULT: FAIL\n", GetLastError());
        return 1;
    }
    lines_for(dll, &lines, &count);
    for (i = 0; i < count; i++)
    {
        Sleep(200);
        WriteFile(in_w, lines[i], strlen(lines[i]), &got, NULL);
        WriteFile(in_w, "\r", 1, &got, NULL);
    }
    WaitForSingleObject(pi.hProcess, 60000);
    GetExitCodeProcess(pi.hProcess, &code);
    close(pcon);
    CloseHandle(in_w); CloseHandle(out_w);
    WaitForSingleObject(thread, 5000);
    result = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                         FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (result == INVALID_HANDLE_VALUE)
    {
        printf("FAIL no result file (child exit %#lx)\nRESULT: FAIL\n", code);
        return 1;
    }
    while (ReadFile(result, buf, sizeof(buf), &got, NULL) && got) fwrite(buf, 1, got, stdout);
    CloseHandle(result);
    return code;
}

static void *vfunc;
static int __cdecl conio(unsigned __int64 options, const void *fmt, ...)
{
    va_list va;
    int ret;

    va_start(va, fmt);
    ret = ((int (__cdecl *)(unsigned __int64, const void *, void *, va_list))vfunc)(options, fmt, NULL, va);
    va_end(va);
    return ret;
}

int main(int argc, char **argv)
{
    int (__cdecl *p_cgets_s)(char *, size_t, size_t *);
    int (__cdecl *p_cgetws_s)(WCHAR *, size_t, size_t *);
    WCHAR *(__cdecl *p_cgetws)(WCHAR *);
    void *(__cdecl *p_handler)(void *);
    char buf[64];
    WCHAR wbuf[64], *wr;
    size_t n;
    HMODULE mod;
    int r;

    if (argc < 2) { printf("FAIL usage\nRESULT: FAIL\n"); return 1; }
    if (argc < 4 || strcmp(argv[2], "child")) return run_child(argv[1]);
    if (!freopen(argv[3], "w", stdout)) return 1;
    setvbuf(stdout, NULL, _IONBF, 0);

    if (!(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    p_cgets_s = (void *)GetProcAddress(mod, "_cgets_s");
    p_cgetws_s = (void *)GetProcAddress(mod, "_cgetws_s");
    p_cgetws = (void *)GetProcAddress(mod, "_cgetws");
    p_handler = (void *)GetProcAddress(mod, "_set_invalid_parameter_handler");
    check(p_cgetws != NULL, "_cgetws exported");
    if (!p_cgetws) { printf("RESULT: FAIL\n"); return 1; }
    if (p_handler) p_handler(handler);

    if (p_handler)
    {
        check(p_cgets_s && p_cgetws_s, "_cgets_s, _cgetws_s exported");
        if (failures) { printf("RESULT: FAIL\n"); return 1; }
        memset(buf, 'x', sizeof(buf)); n = 99;
        r = p_cgets_s(buf, sizeof(buf), &n);
        check(!r && n == 5 && !strcmp(buf, "hello"), "_cgets_s: %d, %Iu \"%s\"", r, n, buf);
        n = 99;
        r = p_cgetws_s(wbuf, ARRAYSIZE(wbuf), &n);
        check(!r && n == 9 && !wcscmp(wbuf, L"wide line"), "_cgetws_s: %d, %Iu \"%ls\"", r, n, wbuf);
    }

    wbuf[0] = 40;
    wr = p_cgetws(wbuf);
    check(wr == wbuf + 2 && wbuf[1] == 5 && !wcscmp(wr, L"third"), "_cgetws: %p, length %u \"%ls\"",
          wr, wbuf[1], wr ? wr : L"");

    if (p_handler)
    {
        n = 99;
        r = p_cgets_s(buf, sizeof(buf), &n);
        check(!r && n == 0 && !buf[0], "_cgets_s empty line: %d, %Iu", r, n);

        invalid = 0; n = 99;
        r = p_cgets_s(NULL, 10, &n);
        check(r == EINVAL && invalid == 1 && n == 0, "_cgets_s(NULL buffer): %d, handler %d, %Iu", r, invalid, n);
        invalid = 0;
        r = p_cgets_s(buf, 10, NULL);
        check(r == EINVAL && invalid == 1, "_cgets_s(NULL count): %d, handler %d", r, invalid);
        invalid = 0;
        r = p_cgetws_s(wbuf, 0, &n);
        check(r == EINVAL && invalid == 1, "_cgetws_s(size 0): %d, handler %d", r, invalid);
    }

    if (!strcmp(argv[1], "ucrtbase"))
    {
        int num = 0;

        vfunc = GetProcAddress(mod, "__conio_common_vcscanf");
        r = vfunc ? conio(0, "%d %s", &num, buf) : -2;
        check(r == 2 && num == 42 && !strcmp(buf, "word"), "__conio_common_vcscanf: %d, %d \"%s\"", r, num, buf);
        vfunc = GetProcAddress(mod, "__conio_common_vcwscanf");
        r = vfunc ? conio(1 /* secure */, L" %ls", wbuf, (unsigned)ARRAYSIZE(wbuf)) : -2;
        check(r == 1 && !wcscmp(wbuf, L"wq"), "__conio_common_vcwscanf: %d \"%ls\"", r, wbuf);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
