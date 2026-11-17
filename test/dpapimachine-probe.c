/* dpapimachine-probe protect FILE [machine] | unprotect FILE
 * CryptProtectData (CRYPTPROTECT_LOCAL_MACHINE with "machine") of a fixed secret into FILE, or
 * CryptUnprotectData of FILE: prints "unprotect=OK <text>" or "unprotect=FAIL <error>" (wine-sg 7844). */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv)
{
    static char buf[4096];
    DATA_BLOB in, out;
    FILE *f;
    if (argc >= 3 && !strcmp(argv[1], "protect"))
    {
        in.pbData = (BYTE *)"sg-dpapi-secret"; in.cbData = 16;
        if (!CryptProtectData(&in, L"probe", NULL, NULL, NULL,
                              CRYPTPROTECT_UI_FORBIDDEN | (argc > 3 ? CRYPTPROTECT_LOCAL_MACHINE : 0), &out))
        { printf("protect=FAIL %lu\n", GetLastError()); return 1; }
        if (!(f = fopen(argv[2], "wb"))) return 2;
        fwrite(out.pbData, 1, out.cbData, f); fclose(f);
        printf("protect=OK\n");
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "unprotect"))
    {
        if (!(f = fopen(argv[2], "rb"))) { printf("unprotect=NOFILE\n"); return 2; }
        in.cbData = fread(buf, 1, sizeof(buf), f); fclose(f); in.pbData = (BYTE *)buf;
        if (!CryptUnprotectData(&in, NULL, NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &out))
        { printf("unprotect=FAIL %lu\n", GetLastError()); return 1; }
        printf("unprotect=OK %.*s\n", (int)out.cbData, out.pbData);
        return 0;
    }
    return 3;
}
