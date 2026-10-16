/* Smoke probe for patches/sg/4571 (fast mode), in the DLL named on the
 * command line (msvcp80, msvcp90): _Xfsopen (the wide-name _wfsopen of
 * the file buffers) writing, appending and reading a file through the
 * matching CRT's FILE functions, and msvcp90's tr1::_Xinvalid (throws
 * std::invalid_argument; a child process reads the thrown type).
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN64
#define N(a, b) a
#else
#define N(a, b) b
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static LONG CALLBACK handler(EXCEPTION_POINTERS *ep)
{
    EXCEPTION_RECORD *rec = ep->ExceptionRecord;

    if (rec->ExceptionCode == 0xe06d7363 && rec->NumberParameters >= 3)
    {
#ifdef _WIN64
        char *base = (char *)rec->ExceptionInformation[3];
        const int *ti = (const int *)rec->ExceptionInformation[2];
        const int *cta = (const int *)(base + ti[3]);
        const char *name = base + ((const int *)(base + cta[1]))[1] + 16;
#else
        const int *ti = (const int *)rec->ExceptionInformation[2];
        const int *cta = (const int *)ti[3];
        const char *name = (const char *)((const int *)cta[1])[1] + 8;
#endif
        printf("thrown %s", name);
        fflush(stdout);
        ExitProcess(0x42);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char **argv)
{
    void *(__cdecl *xfsopen)(const WCHAR *, const WCHAR *, int);
    size_t (__cdecl *p_fwrite)(const void *, size_t, size_t, void *), (__cdecl *p_fread)(void *, size_t, size_t, void *);
    int (__cdecl *p_fclose)(void *);
    HMODULE mod, crt;
    WCHAR path[MAX_PATH];
    int is90;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    is90 = !strcmp(argv[1], "msvcp90");
    if (argc > 2 && !strcmp(argv[2], "child"))
    {
        void (__cdecl *xinvalid)(const char *) = (void *)GetProcAddress(mod, N("?_Xinvalid@tr1@std@@YAXPEBD@Z", "?_Xinvalid@tr1@std@@YAXPBD@Z"));
        AddVectoredExceptionHandler(1, handler);
        xinvalid("bad argument");
        printf("returned");
        return 0;
    }
    crt = LoadLibraryA(is90 ? "msvcr90.dll" : "msvcr80.dll");
    xfsopen = (void *)GetProcAddress(mod, N("?_Xfsopen@std@@YAPEAU_iobuf@@PEB_W0H@Z", "?_Xfsopen@std@@YAPAU_iobuf@@PB_W0H@Z"));
    p_fwrite = (void *)GetProcAddress(crt, "fwrite");
    p_fread = (void *)GetProcAddress(crt, "fread");
    p_fclose = (void *)GetProcAddress(crt, "fclose");
    check(xfsopen && p_fwrite && p_fread && p_fclose, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"xfsopen.txt");
    {
        static const struct { const WCHAR *mode; const char *write, *want; } steps[] =
        {
            {L"w", "first", NULL}, {L"a", " second", NULL}, {L"r", NULL, "first second"}, {L"w", "third", NULL}, {L"rb", NULL, "third"},
        };
        unsigned int i;

        for (i = 0; i < ARRAYSIZE(steps); i++)
        {
            void *f = xfsopen(path, steps[i].mode, 0x40 /* _SH_DENYNO */);
            char buf[64] = {0};

            check(f != NULL, "_Xfsopen(%ls)", steps[i].mode);
            if (!f) continue;
            if (steps[i].write) p_fwrite(steps[i].write, 1, strlen(steps[i].write), f);
            if (steps[i].want)
            {
                p_fread(buf, 1, sizeof(buf) - 1, f);
                check(!strcmp(buf, steps[i].want), "read \"%s\"", buf);
            }
            p_fclose(f);
        }
        DeleteFileW(path);
        check(!xfsopen(L"C:\\no\\such\\dir\\file", L"r", 0x40), "_Xfsopen of a missing file");
    }

    if (is90)
    {
        char cmd[MAX_PATH + 32], self[MAX_PATH], out[256] = {0};
        STARTUPINFOA si = {sizeof(si)};
        PROCESS_INFORMATION pi;
        SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
        HANDLE rd, wr;
        DWORD code = 0, got = 0;

        CreatePipe(&rd, &wr, &sa, 0);
        SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = si.hStdError = wr;
        GetModuleFileNameA(NULL, self, sizeof(self));
        sprintf(cmd, "\"%s\" %s child", self, argv[1]);
        if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
        {
            CloseHandle(wr);
            WaitForSingleObject(pi.hProcess, 30000);
            GetExitCodeProcess(pi.hProcess, &code);
            ReadFile(rd, out, sizeof(out) - 1, &got, NULL);
            check(code == 0x42 && strstr(out, ".?AVinvalid_argument@std@@"), "tr1::_Xinvalid throws: exit %#lx \"%s\"", code, out);
        }
        else check(0, "CreateProcess %lu", GetLastError());
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
