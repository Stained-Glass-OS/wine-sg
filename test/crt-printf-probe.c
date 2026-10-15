/* Probe for patches/sg/4512: the stdout printf variants _printf_l/_p/_p_l/_s_l,
 * _vprintf_*, their wide forms, and the console _cprintf_p/_s (+_l, wide,
 * _vcprintf_*) of msvcr80 .. msvcr120, and ucrtbase's
 * __conio_common_vc(w)printf_p/_s. Table driven: each case prints into the
 * DLL's stdout (a file set as the standard output before the DLL loads) or
 * into a new console, and the probe reads the text back.
 * The console cases need a console without a display: the probe runs itself
 * again under a pseudo console (headless conhost) and passes on what the
 * child wrote to a result file.
 *   probe.exe DLL      lines "ok"/"FAIL", last "RESULT: PASS|FAIL" */
#define _WIN32_WINNT 0x0a00
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <errno.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static HMODULE mod;
static HANDLE file, con;
static int (__cdecl *p_fflush)(void *);
static int invalid;

static void __cdecl handler(const wchar_t *e, const wchar_t *f, const wchar_t *fn, unsigned int l, uintptr_t r)
{
    invalid++;
}

/* the text written to the DLL's stdout since the last call */
static void read_stdout(char *buf, int size)
{
    static DWORD pos;
    DWORD got = 0, end;

    p_fflush(NULL);
    end = GetFileSize(file, NULL);
    SetFilePointer(file, pos, NULL, FILE_BEGIN);
    if (end - pos < (DWORD)size - 1) size = end - pos + 1;
    ReadFile(file, buf, size - 1, &got, NULL);
    buf[got] = 0;
    pos = end;
}

/* the console text written since the last call */
static void read_console(char *buf, int size)
{
    static const COORD origin = {0, 0};
    DWORD got = 0, n;
    int i;

    ReadConsoleOutputCharacterA(con, buf, size - 1, origin, &got);
    buf[got] = 0;
    for (i = got; i > 0 && buf[i - 1] == ' '; i--) buf[i - 1] = 0;
    FillConsoleOutputCharacterA(con, ' ', 80 * 25, origin, &n);
    SetConsoleCursorPosition(con, origin);
}

typedef int (__cdecl *fn_v)(const void *, va_list);
typedef int (__cdecl *fn_vl)(const void *, void *, va_list);
typedef int (__cdecl *fn_conio)(unsigned __int64, const void *, void *, va_list);

static void *vfunc;
static int kind;  /* 0 plain va_list, 1 with locale, 2 ucrtbase conio */
static int __cdecl call(const void *fmt, ...)
{
    va_list va;
    int ret;

    va_start(va, fmt);
    if (kind == 0) ret = ((fn_v)vfunc)(fmt, va);
    else if (kind == 1) ret = ((fn_vl)vfunc)(fmt, NULL, va);
    else ret = ((fn_conio)vfunc)(0, fmt, NULL, va);
    va_end(va);
    return ret;
}

/* the two value forms every case is called with */
#define ARGS 7, "xy"

struct test
{
    const char *name;
    int sig;          /* 0 varargs fmt,...; 1 varargs fmt,locale,...; 2 va_list; 3 va_list+locale; 4 conio */
    int wide, con;
    const char *fmt;  /* converted to wide for the wide forms */
    const char *expect;
    int ret;
};

static const struct test tests[] =
{
    {"_printf_p", 0, 0, 0, "[%2$s:%1$d]", "[xy:7]", 6},
    {"_printf_p_l", 1, 0, 0, "<%2$s %1$d>", "<xy 7>", 6},
    {"_printf_l", 1, 0, 0, "%d-%s", "7-xy", 4},
    {"_printf_s_l", 1, 0, 0, "%d/%s", "7/xy", 4},
    {"_vprintf_p", 2, 0, 0, "%2$s%1$d", "xy7", 3},
    {"_vprintf_p_l", 3, 0, 0, "%1$d%2$s%1$d", "7xy7", 4},
    {"_vprintf_l", 3, 0, 0, "%d.%s", "7.xy", 4},
    {"_vprintf_s_l", 3, 0, 0, "(%d)", "(7)", 3},
    {"_wprintf_p", 0, 1, 0, "[%2$hs:%1$d]", "[xy:7]", 6},
    {"_wprintf_p_l", 1, 1, 0, "%2$hs%1$d", "xy7", 3},
    {"_wprintf_l", 1, 1, 0, "%d%hs", "7xy", 3},
    {"_wprintf_s_l", 1, 1, 0, "%d", "7", 1},
    {"_vwprintf_p", 2, 1, 0, "%2$hs|%1$d", "xy|7", 4},
    {"_vwprintf_p_l", 3, 1, 0, "%1$d%2$hs", "7xy", 3},
    {"_vwprintf_l", 3, 1, 0, "%d,%hs", "7,xy", 4},
    {"_vwprintf_s_l", 3, 1, 0, "%d", "7", 1},
    {"_cprintf_p", 0, 0, 1, "[%2$s:%1$d]", "[xy:7]", 6},
    {"_cprintf_p_l", 1, 0, 1, "%2$s+%1$d", "xy+7", 4},
    {"_cprintf_s", 0, 0, 1, "%d %s", "7 xy", 4},
    {"_cprintf_s_l", 1, 0, 1, "%d=%s", "7=xy", 4},
    {"_vcprintf_p", 2, 0, 1, "%2$s%1$d", "xy7", 3},
    {"_vcprintf_p_l", 3, 0, 1, "%2$s%1$d", "xy7", 3},
    {"_vcprintf_s", 2, 0, 1, "%d", "7", 1},
    {"_vcprintf_s_l", 3, 0, 1, "%d;", "7;", 2},
    {"_cwprintf_p", 0, 1, 1, "{%2$hs %1$d}", "{xy 7}", 6},
    {"_cwprintf_p_l", 1, 1, 1, "%1$d%2$hs", "7xy", 3},
    {"_cwprintf_s", 0, 1, 1, "%d", "7", 1},
    {"_cwprintf_s_l", 1, 1, 1, "%d!", "7!", 2},
    {"_vcwprintf_p", 2, 1, 1, "%2$hs%1$d", "xy7", 3},
    {"_vcwprintf_p_l", 3, 1, 1, "%1$d:%2$hs", "7:xy", 4},
    {"_vcwprintf_s", 2, 1, 1, "%d", "7", 1},
    {"_vcwprintf_s_l", 3, 1, 1, "%d%hs", "7xy", 3},
    {"__conio_common_vcprintf_p", 4, 0, 1, "[%2$s:%1$d]", "[xy:7]", 6},
    {"__conio_common_vcprintf_s", 4, 0, 1, "%d %s", "7 xy", 4},
    {"__conio_common_vcwprintf_p", 4, 1, 1, "%2$hs-%1$d", "xy-7", 4},
    {"__conio_common_vcwprintf_s", 4, 1, 1, "%d", "7", 1},
};

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

/* run "probe.exe DLL child RESULTFILE" in a new pseudo console */
static int run_child(const char *dll)
{
    STARTUPINFOEXA si = {{sizeof(si)}};
    PROCESS_INFORMATION pi;
    HANDLE in_r, in_w, out_r, out_w, thread, result;
    char cmd[2 * MAX_PATH + 64], path[MAX_PATH], self[MAX_PATH], buf[4096];
    SIZE_T size = 0;
    COORD dim = {80, 25};
    HRESULT (WINAPI *create)(COORD, HANDLE, HANDLE, DWORD, pcon_t *);
    void (WINAPI *close)(pcon_t);
    pcon_t pcon;
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    DWORD got, code = 1;

    GetModuleFileNameA(NULL, self, sizeof(self));
    GetTempPathA(sizeof(path), path);
    strcat(path, "crt-printf-probe.res");
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
    WaitForSingleObject(pi.hProcess, 120000);
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

int main(int argc, char **argv)
{
    char path[MAX_PATH], buf[256];
    HANDLE saved_out;
    void *(__cdecl *set_handler)(void *);
    unsigned int i, j, ran = 0;
    int ucrt;

    if (argc < 2) { printf("FAIL usage\nRESULT: FAIL\n"); return 1; }
    if (argc < 4 || strcmp(argv[2], "child")) return run_child(argv[1]);
    if (!freopen(argv[3], "w", stdout)) return 1;
    setvbuf(stdout, NULL, _IONBF, 0);
    ucrt = !strcmp(argv[1], "ucrtbase");

    /* the pseudo console, and a file as the standard output of the DLL */
    saved_out = GetStdHandle(STD_OUTPUT_HANDLE);
    con = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    read_console(buf, sizeof(buf));
    GetTempPathA(sizeof(path), path);
    strcat(path, "crt-printf-probe.out");
    file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                       CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    SetStdHandle(STD_OUTPUT_HANDLE, file);
    mod = LoadLibraryA(argv[1]);
    SetStdHandle(STD_OUTPUT_HANDLE, saved_out);
    if (!mod || con == INVALID_HANDLE_VALUE || file == INVALID_HANDLE_VALUE)
    {
        printf("FAIL load %s / console / file\nRESULT: FAIL\n", argv[1]);
        return 1;
    }
    p_fflush = (void *)GetProcAddress(mod, "fflush");
    set_handler = (void *)GetProcAddress(mod, "_set_invalid_parameter_handler");
    if (set_handler) set_handler(handler);

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        const struct test *t = &tests[i];
        WCHAR wfmt[64];
        void *f = GetProcAddress(mod, t->name), *fmt = (void *)t->fmt;
        int ret;

        if (ucrt != (t->sig == 4)) continue;
        ran++;
        check(f != NULL, "%s exported", t->name);
        if (!f) continue;
        if (t->wide)
        {
            for (j = 0; t->fmt[j]; j++) wfmt[j] = t->fmt[j];
            wfmt[j] = 0;
            fmt = wfmt;
        }
        switch (t->sig)
        {
        case 0: ret = ((int (__cdecl *)(const void *, ...))f)(fmt, ARGS); break;
        case 1: ret = ((int (__cdecl *)(const void *, void *, ...))f)(fmt, NULL, ARGS); break;
        default: vfunc = f; kind = t->sig == 2 ? 0 : t->sig == 3 ? 1 : 2; ret = call(fmt, ARGS); break;
        }
        if (t->con) read_console(buf, sizeof(buf));
        else read_stdout(buf, sizeof(buf));
        check(ret == t->ret && !strcmp(buf, t->expect), "%s(\"%s\") = %d \"%s\" (expected %d \"%s\")",
              t->name, t->fmt, ret, buf, t->ret, t->expect);
    }

    /* a format that mixes positional and plain arguments is invalid for the
     * _p forms; the _s forms reject a NULL format */
    if (!ucrt)
    {
        static const char *bad_p[] = {"_printf_p", "_cprintf_p"};
        int *(__cdecl *p_errno)(void) = (void *)GetProcAddress(mod, "_errno");

        for (i = 0; i < ARRAYSIZE(bad_p); i++)
        {
            int (__cdecl *f)(const char *, ...) = (void *)GetProcAddress(mod, bad_p[i]);
            int ret;

            invalid = 0;
            ret = f ? f("%1$d %d", 1, 2) : 0;
            if (i) read_console(buf, sizeof(buf)); else read_stdout(buf, sizeof(buf));
            check(f && ret < 0 && invalid == 1 && !buf[0], "%s mixed positional: %d, handler %d, \"%s\"",
                  bad_p[i], ret, invalid, buf);
        }
        {
            int (__cdecl *f)(const char *, ...) = (void *)GetProcAddress(mod, "_cprintf_s");
            int ret;

            invalid = 0;
            ret = f ? f(NULL) : 0;
            check(f && ret == -1 && invalid == 1 && p_errno && *p_errno() == EINVAL,
                  "_cprintf_s(NULL): %d, handler %d", ret, invalid);
        }
    }
    else
    {
        int (__cdecl *f)(unsigned __int64, const char *, void *, va_list) =
            (void *)GetProcAddress(mod, "__conio_common_vcprintf_p");
        int ret;

        invalid = 0;
        vfunc = f; kind = 2;
        ret = f ? call("%1$d %d", 1, 2) : 0;
        read_console(buf, sizeof(buf));
        check(f && ret < 0 && invalid == 1 && !buf[0], "__conio_common_vcprintf_p mixed positional: %d, handler %d, \"%s\"",
              ret, invalid, buf);
    }

    check(ran > 0, "%u cases", ran);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
