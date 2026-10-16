/* rohidden gate (5923): the hidden and system attributes of a read-only file are kept */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what, DWORD attrs)
{
    printf("%s  %s (attributes %#lx)\n", ok ? "PASS" : "FAIL", what, attrs);
    if (!ok) failures++;
}

int main(void)
{
    const char *a = "c:\\rohidden-a.txt", *b = "c:\\rohidden-b.txt";
    HANDLE f;
    DWORD attrs;

    f = CreateFileA(a, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(f);
    SetFileAttributesA(a, FILE_ATTRIBUTE_READONLY);
    SetFileAttributesA(a, FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN);
    attrs = GetFileAttributesA(a);
    check(attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_HIDDEN) && (attrs & FILE_ATTRIBUTE_READONLY),
          "hidden added to a read-only file", attrs);
    SetFileAttributesA(a, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY);
    attrs = GetFileAttributesA(a);
    check((attrs & FILE_ATTRIBUTE_SYSTEM) && !(attrs & FILE_ATTRIBUTE_HIDDEN) && (attrs & FILE_ATTRIBUTE_READONLY),
          "read-only file: hidden taken off, system put on", attrs);
    SetFileAttributesA(a, FILE_ATTRIBUTE_HIDDEN);
    attrs = GetFileAttributesA(a);
    check((attrs & FILE_ATTRIBUTE_HIDDEN) && !(attrs & FILE_ATTRIBUTE_READONLY), "read-only off, hidden on at once", attrs);
    SetFileAttributesA(a, FILE_ATTRIBUTE_NORMAL);
    check(DeleteFileA(a), "deleted", 0);

    f = CreateFileA(b, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN, NULL);
    CloseHandle(f);
    attrs = GetFileAttributesA(b);
    check(attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_HIDDEN) && (attrs & FILE_ATTRIBUTE_READONLY),
          "created read-only and hidden", attrs);
    f = CreateFileA(b, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    check(f == INVALID_HANDLE_VALUE, "still read-only: not writable", attrs);
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    SetFileAttributesA(b, FILE_ATTRIBUTE_NORMAL);
    DeleteFileA(b);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
