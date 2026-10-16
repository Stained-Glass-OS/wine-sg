/* Probe for patches/sg/4545: _SetImageBase/_GetImageBase and the throw
 * forms (per thread), and __p__mbcasemap (the case map of the current
 * multibyte code page), in the DLL named on the command line as it exports
 * them. Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures, checked;
#define check(c, ...) do { checked++; if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static ULONG_PTR (__cdecl *p_get)(void), (__cdecl *p_get_throw)(void);
static void (__cdecl *p_set)(ULONG_PTR), (__cdecl *p_set_throw)(ULONG_PTR);

static DWORD WINAPI other(void *arg)
{
    ULONG_PTR v = p_get();
    p_set(0x5000);
    return v == 0 && p_get() == 0x5000;
}

int main(int argc, char **argv)
{
    HMODULE mod;
    unsigned char *(__cdecl *casemap)(void);

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    p_get = (void *)GetProcAddress(mod, "_GetImageBase");
    p_set = (void *)GetProcAddress(mod, "_SetImageBase");
    p_get_throw = (void *)GetProcAddress(mod, "_GetThrowImageBase");
    p_set_throw = (void *)GetProcAddress(mod, "_SetThrowImageBase");
    if (p_get && p_set && p_get_throw && p_set_throw)
    {
        HANDLE th;
        DWORD code = 0;

        p_set(0x1234);
        p_set_throw(0x9876);
        check(p_get() == 0x1234 && p_get_throw() == 0x9876, "set/get image bases");
        th = CreateThread(NULL, 0, other, NULL, 0, NULL);
        WaitForSingleObject(th, 5000);
        GetExitCodeThread(th, &code);
        CloseHandle(th);
        check(code == 1 && p_get() == 0x1234, "per thread");
    }
    if ((casemap = (void *)GetProcAddress(mod, "__p__mbcasemap")))
    {
        int (__cdecl *setmbcp)(int) = (void *)GetProcAddress(mod, "_setmbcp");
        unsigned char *m;

        setmbcp(1252);
        m = casemap();
        check(m && m['a'] == 'A' && m['Z'] == 'z' && m['5'] == 0, "__p__mbcasemap in 1252: %#x %#x", m ? m['a'] : 0, m ? m['Z'] : 0);
    }
    check(checked > 0, "%d checks", checked);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
