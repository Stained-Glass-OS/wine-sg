/* Probe for patches/sg/2456: MsiGetFileVersion looks for the file where the path says, not in the system directories. */
#include <windows.h>
#include <msi.h>
#include <stdio.h>
#include <string.h>

static int fails;
static void checku(const char *name, UINT got, UINT want)
{
    printf("      %s  %s (%u, want %u)\n", got == want ? "PASS" : "FAIL", name, got, want);
    if (got != want) fails++;
}
static void checks(const char *name, int ok) { printf("      %s  %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }

int main(void)
{
    char version[MAX_PATH], lang[MAX_PATH], path[MAX_PATH];
    DWORD versz, langsz;
    UINT r;

    SetCurrentDirectoryA("C:\\");
    versz = langsz = MAX_PATH;
    strcpy(version, "version"); strcpy(lang, "lang");
    r = MsiGetFileVersionA("kernel32.dll", version, &versz, lang, &langsz);
    checku("a relative name is not looked for in the system directory", r, ERROR_FILE_NOT_FOUND);
    checks("the buffers are as they were", !strcmp(version, "version") && !strcmp(lang, "lang") && versz == MAX_PATH && langsz == MAX_PATH);

    GetSystemDirectoryA(path, MAX_PATH);
    strcat(path, "\\kernel32.dll");
    versz = langsz = MAX_PATH;
    r = MsiGetFileVersionA(path, version, &versz, lang, &langsz);
    checku("the full path gives its version", r, 0);
    checks("  a dotted version", versz >= 5 && strchr(version, '.') != NULL);

    r = MsiGetFileVersionA("C:\\no\\such\\directory\\file.dll", version, &versz, lang, &langsz);
    checku("a missing file in a missing directory", r, ERROR_FILE_NOT_FOUND);
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
