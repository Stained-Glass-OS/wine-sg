/* kernel32 private-profile edge behaviour that Wine's conformance tests
 * (kernel32/tests/profile.c) mark todo_wine (patches/sg/2211): flush
 * returns TRUE, GetPrivateProfileStruct error codes, double-null ending of
 * cut-short name lists (ANSI), and delete-from-missing-file creating it. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
static void checkerr(BOOL ret, DWORD err, DWORD want, const char *what)
{
    char b[200];
    snprintf(b, sizeof(b), "%s (ret %d, error %lu)", what, ret, (unsigned long)err);
    check(!ret && err == want, b);
}

static int file_is(const char *path, const char *data, DWORD size)
{
    char buf[256];
    DWORD got = 0;
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (h == INVALID_HANDLE_VALUE) return 0;
    ReadFile(h, buf, sizeof(buf), &got, NULL);
    CloseHandle(h);
    return got == size && !memcmp(buf, data, size);
}

int main(void)
{
    const char *ini = ".\\sgprofile_probe.ini", *ini2 = ".\\sgprofile_probe2.ini";
    char buf[64], big[64];
    BOOL ret;
    DWORD n;

    DeleteFileA(ini);
    DeleteFileA(ini2);

    /* --- flush --- */
    SetLastError(0xdeadbeef);
    ret = WritePrivateProfileStringA(NULL, NULL, NULL, ".\\sgprofile_nonexistent.ini");
    check(ret == TRUE, "WritePrivateProfileString(NULL, NULL, NULL, missing file) flushes and returns TRUE");
    check(GetFileAttributesA(".\\sgprofile_nonexistent.ini") == INVALID_FILE_ATTRIBUTES, "...and does not create the file");
    WritePrivateProfileStringA("s", "k", "v", ini);
    check(WritePrivateProfileStringA(NULL, NULL, NULL, ini) == TRUE, "...nor does flushing an existing file fail");

    /* --- struct --- */
    DeleteFileA(ini);
    SetLastError(0xdeadbeef);
    ret = GetPrivateProfileStructA("s", "key", buf, 7, ini);
    checkerr(ret, GetLastError(), ERROR_BAD_LENGTH, "Struct: a missing key is ERROR_BAD_LENGTH");
    check(WritePrivateProfileStructA("s", "key", (void *)"abacus", 7, ini), "Struct: write");
    memset(buf, 0xcc, sizeof(buf));
    check(GetPrivateProfileStructA("s", "key", buf, 7, ini) && !strcmp(buf, "abacus"), "Struct: reads back");
    SetLastError(0xdeadbeef);
    ret = GetPrivateProfileStructA("s", "key", buf, 6, ini);
    checkerr(ret, GetLastError(), ERROR_BAD_LENGTH, "Struct: asking for fewer bytes than stored is ERROR_BAD_LENGTH");
    SetLastError(0xdeadbeef);
    ret = GetPrivateProfileStructA("s", "key", buf, 8, ini);
    checkerr(ret, GetLastError(), ERROR_BAD_LENGTH, "Struct: asking for more bytes is ERROR_BAD_LENGTH");
    WritePrivateProfileStringA("s", "key", "636163747573006F", ini);
    SetLastError(0xdeadbeef);
    ret = GetPrivateProfileStructA("s", "key", buf, 7, ini);
    checkerr(ret, GetLastError(), ERROR_INVALID_DATA, "Struct: a wrong checksum is ERROR_INVALID_DATA");
    WritePrivateProfileStringA("s", "key", "636163747573008Q", ini);
    SetLastError(0xdeadbeef);
    ret = GetPrivateProfileStructA("s", "key", buf, 7, ini);
    checkerr(ret, GetLastError(), ERROR_INVALID_DATA, "Struct: a bad hex digit is ERROR_INVALID_DATA");
    WritePrivateProfileStringA("s", "key", "16361637475730083", ini);
    SetLastError(0xdeadbeef);
    ret = GetPrivateProfileStructA("s", "key", buf, 7, ini);
    checkerr(ret, GetLastError(), ERROR_BAD_LENGTH, "Struct: a value one digit too long is ERROR_BAD_LENGTH");
    WritePrivateProfileStringA("s", "key", "6361637475730083", ini);
    memset(buf, 0xcc, sizeof(buf));
    check(GetPrivateProfileStructA("s", "key", buf, 7, ini) && !strcmp(buf, "cactus"), "Struct: a good value with a good checksum");

    /* --- double-null ending of name lists --- */
    DeleteFileA(ini);
    WritePrivateProfileStringA("section1", "name1", "v1", ini);
    WritePrivateProfileStringA("section1", "name2", "v2", ini);
    WritePrivateProfileStringA("section2", "name1", "v1", ini);

    memset(buf, 0xc, sizeof(buf));
    n = GetPrivateProfileStringA(NULL, "name1", "default", buf, 16, ini);
    check(n == 14 && !memcmp(buf, "section1\0secti\0", 16) , "section names cut short end with two nulls");
    memset(buf, 0xc, sizeof(buf));
    n = GetPrivateProfileStringA("section1", NULL, "default", buf, 14, ini);
    check(n == 12 && !memcmp(buf, "name1\0name2\0", 12) && buf[12] == 0 && buf[13] == 0, "key names that fit end with two nulls");
    memset(buf, 0xc, sizeof(buf));
    n = GetPrivateProfileStringA("section1", NULL, "default", buf, 9, ini);
    check(n == 7 && !memcmp(buf, "name1\0n", 7) && buf[7] == 0 && buf[8] == 0, "key names cut short end with two nulls");

    memset(big, 0xc, sizeof(big));
    n = GetPrivateProfileSectionNamesA(big, 28, ini);
    check(n == 18 && !memcmp(big, "section1\0section2\0", 18) && big[18] == 0 && big[19] == 0, "section names that fit end with two nulls");
    memset(big, 0xc, sizeof(big));
    n = GetPrivateProfileSectionNamesA(big, 12, ini);
    check(n == 10 && big[10] == 0 && big[11] == 0, "section names cut short end with two nulls");

    /* --- deleting from a file that is not there --- */
    DeleteFileA(ini2);
    ret = WritePrivateProfileStringA("App", NULL, "string", ini2);
    check(ret && file_is(ini2, "", 0), "deleting a section of a missing file creates it, empty");
    DeleteFileA(ini2);
    ret = WritePrivateProfileStringA("App", "key", NULL, ini2);
    check(ret && file_is(ini2, "", 0), "deleting a key of a missing file creates it, empty");
    DeleteFileA(ini2);
    WritePrivateProfileStringA("App", "key", "value", ini2);
    ret = WritePrivateProfileStringA("App", "other", NULL, ini2);
    check(ret && file_is(ini2, "[App]\r\nkey=value\r\n", 18), "deleting a key that is not there leaves an existing file as it was");

    DeleteFileA(ini);
    DeleteFileA(ini2);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
