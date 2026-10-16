/* Probe for patches/sg/4531: <random>'s multiprecision helpers _MP_Mul,
 * _MP_Add, _MP_Rem and _MP_Get ((a*b + c) mod m, as linear_congruential
 * uses them with a 64-bit modulus) in std (msvcp110 and later) or tr1
 * (msvcp90, msvcp100), against a table computed with Python's integers, and
 * _Rng_abort (message on stderr, abort) in a child process.
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const struct { ULONGLONG a, b, c, m, want; } table[] =
{
    {0x0ull, 0x0ull, 0x0ull, 0x1ull, 0x0ull},
    {0x1ull, 0x1ull, 0x0ull, 0x2ull, 0x1ull},
    {0x5851f42d4c957f2dull, 0x1ull, 0x14057b7ef767814full, 0xffffffffffffffffull, 0x6c576fac43fd007cull},
    {0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffc5ull, 0xd5eull},
    {0x8000000000000000ull, 0x8000000000000000ull, 0x0ull, 0x8000000000000001ull, 0x1ull},
    {0xbc8full, 0x75bcd15ull, 0x0ull, 0x7fffffffull, 0x6e30592ull},
    {0xffffffffffffffffull, 0x2ull, 0x0ull, 0xffffffffffffffffull, 0x0ull},
    {0x3039ull, 0x10932ull, 0xbull, 0x100000000ull, 0x31f46c2dull},
    {0x820b356bab6994ull, 0xdc875c347943cdc8ull, 0x879b56950e513933ull, 0xffffffffffffffb4ull, 0xb61d9a3f02a783ffull},
    {0xb32f70497212e2e1ull, 0xb10a22db8e7d03cbull, 0x1d7ef213191cb58bull, 0xffffffffffffff64ull, 0x5cc8789de3d2c9eaull},
    {0x5dfcbcfb0b3b1b3aull, 0xcff1a719b58f6376ull, 0x8d7ccf726344fd5cull, 0x1cecaa595ull, 0xd396f131ull},
    {0x9b7edb57aa888edaull, 0x8ec56825dd29c46dull, 0xf0432f1c6f2d4a9ull, 0x713685caa7ad793bull, 0x162970f35b321747ull},
    {0x4079bc28b76b57b0ull, 0x9214baf83d847126ull, 0x941c21a8d12244a0ull, 0x9778d6946c59797dull, 0x1a42fcabce34ae7bull},
    {0x6ac3ce6e0ab319e5ull, 0x1187697b0b4b2de6ull, 0xc2acadeee4922999ull, 0xffffffffffffff09ull, 0xd2b25a7a77f6eddaull},
    {0x2156c249538fb5ccull, 0xb13740142a886dd6ull, 0xc90a6054353cc1e0ull, 0xffffffffffffff92ull, 0x51211503680d7768ull},
    {0x7895784128101114ull, 0x55678633b0ad22e2ull, 0xab4df1ad2b2453b7ull, 0xd8277f513b244c11ull, 0xd7e563e2cb63750ull},
    {0xee05ba29d19961e8ull, 0xd90f3ec1d8c959b2ull, 0x35b522264052c2e6ull, 0x12e57cee6d64b5full, 0x12245f487a76472ull},
    {0x752594b85d63eb95ull, 0x861925954f0353beull, 0x5f3a12214c989098ull, 0x5892cf65cc8a4701ull, 0x1bc71cd135e83fa3ull},
    {0x82b4403c3f4f8f67ull, 0x887715aff1a20041ull, 0xd099c1988c0d4d29ull, 0x5db38b89b488b521ull, 0x2ca6067610ce148aull},
    {0x12fc95ef0d1ec294ull, 0x3128d440397ba99aull, 0xade04be97ff1c461ull, 0xf3ecf29c351fecf7ull, 0xeb3239721c827e07ull},
    {0x11826ffb4e635dc7ull, 0x25e2c0e0964bdb3bull, 0x1157957fe07f80a5ull, 0xc3ae9714eee35f79ull, 0x4dc6429d1b3b6611ull},
    {0x544917d469ccb2a1ull, 0xe1c17e89c7683784ull, 0x34a90c6fffd4a025ull, 0xffffffffffffffd6ull, 0xe79ebaba066dafc1ull},
    {0x957818f69a8f0b78ull, 0x7108c2157182436dull, 0x62bfce19fcb61d67ull, 0x6519f6632939491bull, 0x30f407def1206753ull},
    {0xfaad28fdd38f8571ull, 0x3aec6f869516f7afull, 0x34c878afe9b256a6ull, 0xac34055b993fa34bull, 0x1fb710c378b5c987ull},
    {0x167b57f0caa4daaaull, 0x40e07342d7c07a2dull, 0x9515eb2ba4f0bd9bull, 0x51de48ddcb4ed027ull, 0x2cc7ba31d211a68ull},
    {0x1849952db6adbc23ull, 0xdc1d471dec423cdcull, 0x6afdf6d2eeb51c2cull, 0x9ace8ce8d57d358dull, 0x6943c903c54f30bull},
    {0x8e205ab516944666ull, 0x9c8b3b9880b2ab1cull, 0xf2203b9c69cdbf80ull, 0x566938ed842bf111ull, 0xf8f34374fbf7f90ull},
    {0x3a3404a958ecd447ull, 0x16995e5d8aaa4cc4ull, 0xee9ac84369136cbaull, 0xffffffffffffff84ull, 0xbbda4e3b52dbaaaull},
    {0x32e437e012e68b24ull, 0x92c64c0cc0b86fbbull, 0x757e2449a54402aull, 0x2e5f40edull, 0x11153b22ull},
    {0x5c82d21532ac605eull, 0xea2c43b88bd76e52ull, 0xfdf7b2a1b7710646ull, 0xeea34c89a28c0bb3ull, 0x130b77abb4d0efdfull},
    {0x392858fa2ad2de20ull, 0x37df54018c88e099ull, 0x6e26dbf32724e223ull, 0x1fa0aed83ull, 0xe8aa3b8aull},
    {0x8bae311b30393539ull, 0xadb4b5b2e43fc510ull, 0x8bb919c5e28f3d80ull, 0x16bc93fbadb98807ull, 0x20f563574487015ull},
};

int main(int argc, char **argv)
{
    char name[64];
    const char *ns;
    void (__cdecl *mul)(ULONGLONG *, ULONGLONG, ULONGLONG), (__cdecl *add)(ULONGLONG *, ULONGLONG), (__cdecl *rem)(ULONGLONG *, ULONGLONG);
    ULONGLONG (__cdecl *get)(ULONGLONG *);
    void (__cdecl *rng_abort)(const char *);
    HMODULE mod;
    unsigned int i;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    ns = !strcmp(argv[1], "msvcp90") || !strcmp(argv[1], "msvcp100") ? "tr1@std" : "std";
#ifdef _WIN64
#define Q "QEA"
#define P "PEB"
#else
#define Q "QA"
#define P "PB"
#endif
    sprintf(name, "?_MP_Mul@%s@@YAX" Q "_K_K1@Z", ns); mul = (void *)GetProcAddress(mod, name);
    sprintf(name, "?_MP_Add@%s@@YAX" Q "_K_K@Z", ns); add = (void *)GetProcAddress(mod, name);
    sprintf(name, "?_MP_Rem@%s@@YAX" Q "_K_K@Z", ns); rem = (void *)GetProcAddress(mod, name);
    sprintf(name, "?_MP_Get@%s@@YA_K" Q "_K@Z", ns); get = (void *)GetProcAddress(mod, name);
    sprintf(name, "?_Rng_abort@%s@@YAX" P "D@Z", ns); rng_abort = (void *)GetProcAddress(mod, name);
    check(mul && add && rem && get && rng_abort, "exports (%s)", ns);
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    if (argc > 2)  /* child */
    {
        rng_abort("invalid argument for engine");
        return 0x1001;
    }

    for (i = 0; i < ARRAYSIZE(table); i++)
    {
        ULONGLONG w[5];
        memset(w, 0xcc, sizeof(w));
        mul(w, table[i].a, table[i].b);
        add(w, table[i].c);
        rem(w, table[i].m);
        check(get(w) == table[i].want, "(%I64x * %I64x + %I64x) mod %I64x = %I64x (expected %I64x)",
              table[i].a, table[i].b, table[i].c, table[i].m, get(w), table[i].want);
    }
    {
        ULONGLONG w[5];
        mul(w, 0x123456789abcdefull, 1);
        check(get(w) == 0x123456789abcdefull, "_MP_Get of a product below 2^64");
    }
    {
        STARTUPINFOA si = {sizeof(si)};
        PROCESS_INFORMATION pi;
        SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
        char cmd[MAX_PATH + 64], self[MAX_PATH], path[MAX_PATH], text[256] = {0};
        HANDLE err;
        DWORD code = 0, got = 0;

        GetModuleFileNameA(NULL, self, sizeof(self));
        GetTempPathA(sizeof(path), path);
        strcat(path, "rng-abort.txt");
        err = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &sa,
                          CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdError = err;
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        sprintf(cmd, "\"%s\" %s child", self, argv[1]);
        if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 30000);
            GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
        SetFilePointer(err, 0, NULL, FILE_BEGIN);
        ReadFile(err, text, sizeof(text) - 1, &got, NULL);
        CloseHandle(err);
        check(code == 3 && strstr(text, "invalid argument for engine"), "_Rng_abort: exit %lu, stderr \"%s\"", code, text);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
