/* Probe for patches/sg/4547: the i386 port I/O functions _inp, _inpw,
 * _inpd, _outp, _outpw and _outpd (a privileged instruction exception in
 * user mode, as on Windows: a handler checks the instruction, the port
 * and the data, emulates it and goes on), __p__fileinfo with _fileinfo,
 * and __p__mbcasemap, in the DLL named on the command line as it exports
 * them. Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures, checked;
#define check(c, ...) do { checked++; if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#ifdef __i386__
static DWORD exc_code;
static unsigned int exc_prefix, exc_opcode, exc_port, exc_data, faults;
#define IN_VALUE 0x89abcdefu

static LONG CALLBACK handler(EXCEPTION_POINTERS *ep)
{
    CONTEXT *ctx = ep->ContextRecord;
    const unsigned char *ip = (const unsigned char *)ctx->Eip;
    unsigned int len = 1, mask;

    faults++;
    exc_code = ep->ExceptionRecord->ExceptionCode;
    exc_prefix = 0;
    if (*ip == 0x66) { exc_prefix = 1; ip++; len++; }
    exc_opcode = *ip;
    if (exc_opcode < 0xec || exc_opcode > 0xef) return EXCEPTION_CONTINUE_SEARCH;
    mask = !(exc_opcode & 1) ? 0xff : exc_prefix ? 0xffff : 0xffffffff;
    exc_port = ctx->Edx & 0xffff;
    if (exc_opcode <= 0xed) ctx->Eax = (ctx->Eax & ~mask) | (IN_VALUE & mask);  /* in */
    else exc_data = ctx->Eax & mask;                                           /* out */
    ctx->Eip += len;
    return EXCEPTION_CONTINUE_EXECUTION;
}
#endif

int main(int argc, char **argv)
{
    static const char *const portio[] = {"_inp", "_inpw", "_inpd", "_outp", "_outpw", "_outpd"};
    unsigned int i;
    HMODULE mod;
    int old;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    old = !strcmp(argv[1], "msvcrt") || !strcmp(argv[1], "msvcrtd") || !strcmp(argv[1], "msvcr70") || !strcmp(argv[1], "msvcr71");

#ifdef __i386__
    {
        /* opcode, operand size prefix, port, data written, value read */
        static const struct { unsigned int opcode, prefix; unsigned short port; unsigned int data, ret; } tests[] =
        {
            {0xec, 0, 0x60, 0, 0xef},
            {0xed, 1, 0x3f8, 0, 0xcdef},
            {0xed, 0, 0xcf8, 0, 0x89abcdef},
            {0xee, 0, 0x80, 0x5a, 0x5a},
            {0xef, 1, 0x1f0, 0xbeef, 0xbeef},
            {0xef, 0, 0xcfc, 0xfeedf00d, 0xfeedf00d},
        };
        void *h = AddVectoredExceptionHandler(1, handler);

        for (i = 0; i < ARRAYSIZE(tests); i++)
        {
            void *f = GetProcAddress(mod, portio[i]);
            unsigned int r;

            check(f != NULL, "%s exported", portio[i]);
            if (!f) continue;
            faults = 0; exc_code = 0; exc_opcode = exc_prefix = exc_port = 0; exc_data = 0xdead;
            if (i < 3) r = ((unsigned int (__cdecl *)(unsigned short))f)(tests[i].port);
            else r = ((unsigned int (__cdecl *)(unsigned short, unsigned int))f)(tests[i].port, tests[i].data);
            if (i == 1) r &= 0xffff;  /* unsigned short */
            check(faults == 1 && exc_code == EXCEPTION_PRIV_INSTRUCTION, "%s: privileged instruction (%u, %#lx)",
                  portio[i], faults, exc_code);
            check(exc_opcode == tests[i].opcode && exc_prefix == tests[i].prefix && exc_port == tests[i].port,
                  "%s: opcode %#x prefix %u port %#x", portio[i], exc_opcode, exc_prefix, exc_port);
            if (i >= 3) check(exc_data == tests[i].data, "%s: data %#x", portio[i], exc_data);
            check(r == tests[i].ret, "%s: returns %#x", portio[i], r);
        }
        RemoveVectoredExceptionHandler(h);
    }
#else
    for (i = 0; i < ARRAYSIZE(portio); i++)
        check(!GetProcAddress(mod, portio[i]), "%s: i386 only", portio[i]);
#endif

    if (old)
    {
        int *(__cdecl *p_fileinfo)(void) = (void *)GetProcAddress(mod, "__p__fileinfo");
        int *data = (void *)GetProcAddress(mod, "_fileinfo");

        if (sizeof(void *) == 4 || strcmp(argv[1], "msvcrt"))
        {
            check(p_fileinfo && data && p_fileinfo() == data && *data == 0, "__p__fileinfo, _fileinfo: %p %p",
                  p_fileinfo ? p_fileinfo() : NULL, data);
            if (p_fileinfo && data) { *p_fileinfo() = 1; check(*data == 1, "_fileinfo set"); *data = 0; }
        }
        else check(!p_fileinfo && data && *data == 0, "x64 msvcrt: _fileinfo only");
    }

    {
        unsigned char *(__cdecl *casemap)(void) = (void *)GetProcAddress(mod, "__p__mbcasemap");
        int (__cdecl *setmbcp)(int) = (void *)GetProcAddress(mod, "_setmbcp");
        unsigned char *m;

        if (sizeof(void *) == 4 || strcmp(argv[1], "msvcrt")) check(casemap != NULL, "__p__mbcasemap exported");
        if (casemap)
        {
            setmbcp(1252);
            m = casemap();
            check(m && m['a'] == 'A' && m['Z'] == 'z' && m['5'] == 0, "__p__mbcasemap in 1252: %#x %#x",
                  m ? m['a'] : 0, m ? m['Z'] : 0);
        }
    }

    check(checked > 0, "%d checks", checked);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
