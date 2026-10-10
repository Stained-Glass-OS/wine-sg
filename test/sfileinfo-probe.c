/* shell32 batch (patches/sg/2023), run by test/sfileinfo-gate.sh. Families
 * (from Wine's todo_wine blocks, which record Windows): the fields
 * SHGetFileInfo clears and the ones it leaves alone, SHGFI_EXETYPE with other
 * flags, and SHFileOperation moves of several things to one destination (made
 * a folder), a folder moved into itself.
 *
 *   sfileinfo-probe.exe */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static void test_fileinfo(void)
{
    SHFILEINFOA a;
    SHFILEINFOW w;
    DWORD_PTR rc;

    memset(&a, 0xcf, sizeof(a));
    rc = SHGetFileInfoA("", 0, &a, sizeof(a), 0);
    check(rc == 1, "empty name, no flags: returns 1");
    check(a.hIcon == 0, "hIcon is cleared");
    check(a.szDisplayName[0] == 0, "the display name is cleared");
    check(a.szTypeName[0] == 0, "the type name is cleared");
    check(a.iIcon == 0xcfcfcfcf, "the icon index is left alone");
    check(a.dwAttributes == 0xcfcfcfcf, "the attributes are left alone");

    memset(&w, 0xcf, sizeof(w));
    rc = SHGetFileInfoW(NULL, 0, &w, sizeof(w), 0);
    check(!rc && w.szDisplayName[0] == 0xcfcf && w.iIcon == 0xcfcfcfcf, "no name: fails and touches nothing");

    memset(&a, 0xcf, sizeof(a));
    rc = SHGetFileInfoA("c:\\nonexistent", FILE_ATTRIBUTE_DIRECTORY, &a, sizeof(a), SHGFI_ATTRIBUTES | SHGFI_USEFILEATTRIBUTES);
    check(rc == 1, "attributes of a made-up folder");
    check(a.dwAttributes != 0xcfcfcfcf, "and they are set");
    check(a.hIcon == 0 && a.szDisplayName[0] == 0 && a.szTypeName[0] == 0, "the rest is cleared");
    check(a.iIcon == 0xcfcfcfcf, "the icon index stays");

    memset(&w, 0xcf, sizeof(w));
    rc = SHGetFileInfoW(L"c:\\nonexistent", FILE_ATTRIBUTE_DIRECTORY, &w, sizeof(w), SHGFI_ATTRIBUTES | SHGFI_USEFILEATTRIBUTES);
    check(rc == 1 && w.hIcon == 0 && w.szTypeName[0] == 0 && w.iIcon == 0xcfcfcfcf, "the same through the wide function");

    rc = SHGetFileInfoA("c:\\nonexistent", FILE_ATTRIBUTE_DIRECTORY, &a, sizeof(a), SHGFI_EXETYPE | SHGFI_USEFILEATTRIBUTES);
    check(rc == 1, "EXETYPE with USEFILEATTRIBUTES returns 1");

    /* an icon asked for sets the attributes to nothing; a plain index leaves them */
    memset(&w, 0xcf, sizeof(w));
    rc = SHGetFileInfoW(L"c:\\windows", FILE_ATTRIBUTE_DIRECTORY, &w, sizeof(w), SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON);
    check(rc != 0 && w.hIcon != 0, "SHGFI_ICON gives an icon");
    check(w.dwAttributes == 0, "and attributes of 0");
    if (w.hIcon) DestroyIcon(w.hIcon);
    memset(&w, 0xcf, sizeof(w));
    rc = SHGetFileInfoW(L"c:\\windows", FILE_ATTRIBUTE_DIRECTORY, &w, sizeof(w), SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON);
    check(rc != 0 && w.iIcon != 0xcfcfcfcf && w.dwAttributes == 0xcfcfcfcf, "SHGFI_SYSICONINDEX sets the index and leaves the attributes");
    memset(&w, 0xcf, sizeof(w));
    rc = SHGetFileInfoW(L"c:\\windows", FILE_ATTRIBUTE_DIRECTORY, &w, sizeof(w), SHGFI_OPENICON | SHGFI_USEFILEATTRIBUTES);
    check(rc != 0 && w.iIcon == 0xcfcfcfcf, "OPENICON alone leaves the index");
    memset(&w, 0xcf, sizeof(w));
    rc = SHGetFileInfoW(L"c:\\windows", FILE_ATTRIBUTE_DIRECTORY, &w, sizeof(w), SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON | SHGFI_EXETYPE);
    check(rc != 0 && w.iIcon != 0xcfcfcfcf, "EXETYPE beside other flags is ignored");
}

static WCHAR base[MAX_PATH];

static void touch(const WCHAR *rel)
{
    WCHAR p[MAX_PATH];
    HANDLE h;
    swprintf(p, MAX_PATH, L"%ls\\%ls", base, rel);
    h = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(h);
}

static int exists(const WCHAR *rel)
{
    WCHAR p[MAX_PATH];
    swprintf(p, MAX_PATH, L"%ls\\%ls", base, rel);
    return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
}

static int is_dir(const WCHAR *rel)
{
    WCHAR p[MAX_PATH];
    DWORD a;
    swprintf(p, MAX_PATH, L"%ls\\%ls", base, rel);
    a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

/* a list of names in base, double-NUL terminated */
static WCHAR *list(WCHAR *buf, ...)
{
    va_list ap;
    const WCHAR *n;
    WCHAR *p = buf;
    va_start(ap, buf);
    while ((n = va_arg(ap, const WCHAR *)))
    {
        p += swprintf(p, MAX_PATH, L"%ls\\%ls", base, n) + 1;
    }
    va_end(ap);
    *p = 0;
    return buf;
}

static int move(const WCHAR *from, const WCHAR *to, FILEOP_FLAGS flags)
{
    SHFILEOPSTRUCTW op;
    memset(&op, 0, sizeof(op));
    op.wFunc = FO_MOVE;
    op.pFrom = from;
    op.pTo = to;
    op.fFlags = flags | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR;
    return SHFileOperationW(&op);
}

static void reset(void)
{
    WCHAR b[MAX_PATH * 4];
    SHFILEOPSTRUCTW op;
    memset(&op, 0, sizeof(op));
    op.wFunc = FO_DELETE;
    op.pFrom = list(b, L"*", NULL);
    op.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    SHFileOperationW(&op);
    touch(L"f1.txt"); touch(L"f2.txt"); touch(L"f3.txt");
    CreateDirectoryW(L"dummy", NULL);
}

static void test_moves(void)
{
    WCHAR from[MAX_PATH * 6], to[MAX_PATH * 6], dir[MAX_PATH];
    int rc;

    GetTempPathW(MAX_PATH, dir);
    swprintf(base, MAX_PATH, L"%lssg-fileop-%lu", dir, GetCurrentProcessId());
    CreateDirectoryW(base, NULL);
    SetCurrentDirectoryW(base);
    reset();

    /* several files to one destination that does not exist: it is made a folder */
    rc = move(list(from, L"f2.txt", L"f3.txt", NULL), list(to, L"dest1", NULL), 0);
    check(rc == 0, "two files to one new destination: success");
    check(is_dir(L"dest1") && exists(L"dest1\\f2.txt") && exists(L"dest1\\f3.txt"), "they are in the new folder");
    check(!exists(L"f2.txt") && !exists(L"f3.txt"), "and gone from where they were");

    /* several destinations, without FOF_MULTIDESTFILES: the first is the folder */
    reset();
    rc = move(list(from, L"f2.txt", L"f3.txt", NULL), list(to, L"d.txt", L"e.txt", NULL), 0);
    check(rc == 0, "two files to two destinations: success");
    check(exists(L"d.txt\\f2.txt") && exists(L"d.txt\\f3.txt"), "all go into the first");
    check(!exists(L"e.txt"), "the second destination is not made");

    /* a folder and files, three destinations */
    reset();
    CreateDirectoryW(L"sub", NULL);
    touch(L"sub\\inner.txt");
    rc = move(list(from, L"f1.txt", L"sub", NULL), list(to, L"x1", L"x2", L"x3", NULL), 0);
    check(rc == 0 && exists(L"x1\\f1.txt") && exists(L"x1\\sub\\inner.txt") && !exists(L"x2") && !exists(L"x3"),
          "a file and a folder go into the first destination");

    /* with FOF_MULTIDESTFILES each to its own */
    reset();
    rc = move(list(from, L"f1.txt", L"f2.txt", NULL), list(to, L"g1.txt", L"g2.txt", NULL), FOF_MULTIDESTFILES);
    check(rc == 0 && exists(L"g1.txt") && exists(L"g2.txt") && !exists(L"f1.txt") && !exists(L"f2.txt"), "FOF_MULTIDESTFILES: one to one");

    /* a folder cannot go into itself */
    reset();
    CreateDirectoryW(L"loop", NULL);
    rc = move(list(from, L"loop", NULL), list(to, L"loop\\inside", NULL), 0);
    check(rc == 0x76, "a folder into itself: DE_DESTSUBTREE");
    check(!exists(L"loop\\inside") && is_dir(L"loop"), "nothing was made");

    /* one file onto an existing folder still goes into it */
    reset();
    CreateDirectoryW(L"there", NULL);
    rc = move(list(from, L"f1.txt", NULL), list(to, L"there", NULL), 0);
    check(rc == 0 && exists(L"there\\f1.txt"), "one file into an existing folder");
    rc = move(list(from, L"f2.txt", L"f3.txt", NULL), list(to, L"there", NULL), 0);
    check(rc == 0 && exists(L"there\\f2.txt") && exists(L"there\\f3.txt"), "two files into an existing folder");

    reset();
    {
        WCHAR b[MAX_PATH * 4];
        SHFILEOPSTRUCTW op;
        memset(&op, 0, sizeof(op));
        op.wFunc = FO_DELETE;
        SetCurrentDirectoryW(dir);
        op.pFrom = list(b, L"", NULL);
        op.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
        SHFileOperationW(&op);
    }
}

int main(void)
{
    CoInitialize(NULL);
    test_fileinfo();
    test_moves();
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    CoUninitialize();
    return failures != 0;
}
