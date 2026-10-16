/* Smoke probe for patches/sg/4562 (fast mode), in the DLL named on the
 * command line: msvcr70/msvcr71's __set_buffer_overrun_handler (the same
 * handler as _set_security_error_handler) and __buffer_overrun (the
 * handler gets _SECERR_BUFFER_OVERRUN, then the process ends with 3; run
 * in a child), and msvcr80's fastcall @_calloc_crt@8/@_realloc_crt@8
 * (i386). Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef void (__cdecl *handler_t)(int, void *);
static void __cdecl handler(int code, void *data) { printf("handler %d %p\n", code, data); fflush(stdout); }
static void __cdecl other(int code, void *data) {}

int main(int argc, char **argv)
{
    HMODULE mod;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }

    if (!strcmp(argv[1], "msvcr70") || !strcmp(argv[1], "msvcr71"))
    {
        handler_t (__cdecl *set_overrun)(handler_t) = (void *)GetProcAddress(mod, "__set_buffer_overrun_handler");
        handler_t (__cdecl *set_security)(handler_t) = (void *)GetProcAddress(mod, "_set_security_error_handler");
        void (__cdecl *overrun)(void) = (void *)GetProcAddress(mod, "__buffer_overrun");

        if (argc > 2)  /* the child */
        {
            set_overrun(handler);
            overrun();
            printf("returned\n");
            return 0;
        }
        check(set_overrun && set_security && overrun, "exports");
        if (failures) { printf("RESULT: FAIL\n"); return 1; }
        check(set_overrun(other) == NULL, "no handler at first");
        check(set_security(NULL) == other, "one handler for both names");
        {
            char cmd[MAX_PATH + 64], self[MAX_PATH], out[256] = {0};
            STARTUPINFOA si = {sizeof(si)};
            PROCESS_INFORMATION pi;
            SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
            HANDLE rd, wr;
            DWORD code = 0, got = 0;

            CreatePipe(&rd, &wr, &sa, 0);
            SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = si.hStdError = wr;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            GetModuleFileNameA(NULL, self, sizeof(self));
            sprintf(cmd, "\"%s\" %s child", self, argv[1]);
            if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
            {
                CloseHandle(wr);
                WaitForSingleObject(pi.hProcess, 30000);
                GetExitCodeProcess(pi.hProcess, &code);
                ReadFile(rd, out, sizeof(out) - 1, &got, NULL);
                check(code == 3 && strstr(out, "handler 1 00000000") && !strstr(out, "returned"),
                      "__buffer_overrun: exit %lu, \"%s\"", code, out);
            }
            else check(0, "CreateProcess %lu", GetLastError());
        }
    }
    else
    {
#ifdef __i386__
        void *(__fastcall *p_calloc)(size_t, size_t) = (void *)GetProcAddress(mod, "@_calloc_crt@8");
        void *(__fastcall *p_realloc)(void *, size_t) = (void *)GetProcAddress(mod, "@_realloc_crt@8");
        unsigned char *p;
        unsigned int i, zero = 1;

        check(p_calloc && p_realloc, "fastcall exports");
        if (p_calloc && p_realloc)
        {
            p = p_calloc(16, 8);
            for (i = 0; p && i < 128; i++) if (p[i]) zero = 0;
            check(p && zero, "@_calloc_crt@8(16, 8): zeroed %p", p);
            memset(p, 0x77, 128);
            p = p_realloc(p, 4096);
            check(p && p[0] == 0x77 && p[127] == 0x77, "@_realloc_crt@8 keeps the data");
            free(NULL);
        }
#else
        check(!GetProcAddress(mod, "@_calloc_crt@8"), "i386 only");
#endif
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
