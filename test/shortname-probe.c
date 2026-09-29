/* shortname-gate.sh's probe (0494): SetFileShortName. "NAME RESULT ERROR" */
#include <windows.h>
#include <stdio.h>

BOOL WINAPI SetFileShortNameW(HANDLE, LPCWSTR);
BOOL WINAPI SetFileShortNameA(HANDLE, LPCSTR);

static void try(const char *what, HANDLE h, const WCHAR *name)
{
    BOOL ok;
    SetLastError(0xdeadbeef);
    ok = SetFileShortNameW(h, name);
    printf("%s %d %lu\n", what, ok, ok ? 0 : GetLastError());
}

int main(void)
{
    WCHAR path[MAX_PATH];
    HANDLE h;
    BOOL ok;

    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"a long file name for the short name.txt");
    h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    try("valid", h, L"ALONGF~1.TXT");
    try("noext", h, L"ABCDEFGH");
    try("toolong", h, L"ABCDEFGHI.TXT");
    try("longext", h, L"ABC.TEXT");
    try("star", h, L"A*B.TXT");
    try("twodots", h, L"A.B.C");
    try("dotfirst", h, L".TXT");
    try("empty", h, L"");
    try("badhandle", INVALID_HANDLE_VALUE, L"ABC.TXT");
    SetLastError(0xdeadbeef);
    ok = SetFileShortNameA(h, "ABC.TXT");
    printf("ansi %d %lu\n", ok, ok ? 0 : GetLastError());
    CloseHandle(h);
    return 0;
}
