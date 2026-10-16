/* Smoke probe for patches/sg/4570 (fast mode), msvcp100:
 * tr1::_Xfunc throws std::bad_function_call (a child process reads the
 * thrown type's name from the C++ exception's throw information),
 * _Mutex(_Uninitialized) leaves the object as it is, and
 * details::_GetCurrentThreadId. The unsigned short reader is run by
 * msvcp-ushort-read-older-gate.sh's probe through this gate too.
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN64
#define THISCALL
#define N(a, b) a
#else
#define THISCALL __attribute__((thiscall))
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
        const int *ct = (const int *)(base + cta[1]);
        const char *name = base + ct[1] + 16;
#else
        const int *ti = (const int *)rec->ExceptionInformation[2];
        const int *cta = (const int *)ti[3];
        const int *ct = (const int *)cta[1];
        const char *name = (const char *)ct[1] + 8;
#endif
        printf("thrown %s", name);
        fflush(stdout);
        ExitProcess(0x42);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char **argv)
{
    HMODULE mod = LoadLibraryA("msvcp100.dll");
    void (__cdecl *xfunc)(void);

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    xfunc = (void *)GetProcAddress(mod, "?_Xfunc@tr1@std@@YAXXZ");
    if (argc > 1 && !strcmp(argv[argc - 1], "child"))
    {
        AddVectoredExceptionHandler(1, handler);
        xfunc();
        printf("returned\n");
        return 0;
    }

    check(xfunc != NULL, "tr1::_Xfunc exported");
    if (xfunc)
    {
        char cmd[MAX_PATH + 16], self[MAX_PATH], out[256] = {0};
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
        sprintf(cmd, "\"%s\" child", self);
        if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
        {
            CloseHandle(wr);
            WaitForSingleObject(pi.hProcess, 30000);
            GetExitCodeProcess(pi.hProcess, &code);
            ReadFile(rd, out, sizeof(out) - 1, &got, NULL);
            check(code == 0x42 && strstr(out, ".?AVbad_function_call@std@@"), "_Xfunc throws: exit %#lx \"%s\"", code, out);
        }
        else check(0, "CreateProcess %lu", GetLastError());
    }
    {
        void *(THISCALL *ctor)(void *, int) = (void *)GetProcAddress(mod, N("??0_Mutex@std@@QEAA@W4_Uninitialized@1@@Z", "??0_Mutex@std@@QAE@W4_Uninitialized@1@@Z"));
        DWORD (__cdecl *tid)(void) = (void *)GetProcAddress(mod, "?_GetCurrentThreadId@details@Concurrency@@YAKXZ");
        unsigned char m[32];
        unsigned int i, same = 1;

        check(ctor && tid, "_Mutex(_Uninitialized), _GetCurrentThreadId exported");
        if (ctor)
        {
            memset(m, 0x5a, sizeof(m));
            check(ctor(m, 0) == m, "_Mutex(_Uninitialized) returns this");
            for (i = 0; i < sizeof(m); i++) if (m[i] != 0x5a) same = 0;
            check(same, "and leaves the object as it is");
        }
        if (tid) check(tid() == GetCurrentThreadId(), "_GetCurrentThreadId");
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
