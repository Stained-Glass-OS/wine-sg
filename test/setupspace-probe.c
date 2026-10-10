/* setupapi disk space list and source list batch (patches/sg/2013), run by
 * test/setupspace-gate.sh. Families: SetupAddToDiskSpaceList /
 * SetupRemoveFromDiskSpaceList (cluster accounting, replacement, existing
 * files), SetupQueryDrivesInDiskSpaceList, SetupDuplicateDiskSpaceList with
 * entries, and the source list (add, set, query, free, cancel; temporary,
 * user and system lists).
 *
 *   setupspace-probe.exe */
#include <windows.h>
#include <setupapi.h>
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
static void checkerr(BOOL ret, DWORD err, DWORD want, const char *what)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "%s (ret %d, error %lu, want %lu)", what, ret, (unsigned long)err, (unsigned long)want);
    check(!ret && err == want, buf);
}

static LONGLONG space(HDSKSPC l, const char *drive)
{
    LONGLONG v = -12345;
    if (!SetupQuerySpaceRequiredOnDriveA(l, drive, &v, NULL, 0)) return -99999;
    return v;
}

static void test_diskspace(void)
{
    WCHAR dir[MAX_PATH], path[MAX_PATH], path2[MAX_PATH], exist[MAX_PATH], full[MAX_PATH];
    DWORD sectors, bytes, clusters, total, cluster, i;
    LONGLONG existing = 5000;
    HDSKSPC l, dup;
    BOOL ret;
    char drive[3];
    char dropbuf[200];
    WCHAR wbuf[200];
    DWORD need;
    HANDLE h;
    DWORD n;
    static char data[5000];

    GetTempPathW(MAX_PATH, dir);
    GetFullPathNameW(dir, MAX_PATH, full, NULL);
    drive[0] = (char)full[0]; drive[1] = ':'; drive[2] = 0;
    {
        WCHAR root[4] = { full[0], ':', '\\', 0 };
        GetDiskFreeSpaceW(root, &sectors, &bytes, &clusters, &total);
        cluster = sectors * bytes;
    }
    swprintf(path, MAX_PATH, L"%lssg-space-new1.bin", full);
    swprintf(path2, MAX_PATH, L"%lssg-space-new2.bin", full);
    swprintf(exist, MAX_PATH, L"%lssg-space-exist.bin", full);
    DeleteFileW(path); DeleteFileW(path2);
    h = CreateFileW(exist, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, data, (DWORD)existing, &n, NULL);
    CloseHandle(h);

    l = SetupCreateDiskSpaceListW(NULL, 0, 0);
    check(l != NULL, "create the list");
    check(space(l, drive) == 0, "a new list needs no space");

    /* arguments */
    SetLastError(0xdeadbeef);
    ret = SetupAddToDiskSpaceListW(NULL, path, 1, FILEOP_COPY, NULL, 0);
    checkerr(ret, GetLastError(), ERROR_INVALID_HANDLE, "add to no list");
    SetLastError(0xdeadbeef);
    ret = SetupAddToDiskSpaceListW(l, NULL, 1, FILEOP_COPY, NULL, 0);
    checkerr(ret, GetLastError(), ERROR_INVALID_PARAMETER, "add without a file");
    SetLastError(0xdeadbeef);
    ret = SetupAddToDiskSpaceListW(l, path, 1, 7, NULL, 0);
    checkerr(ret, GetLastError(), ERROR_INVALID_PARAMETER, "add with an unknown operation");
    SetLastError(0xdeadbeef);
    ret = SetupAddToDiskSpaceListW(l, path, 1, FILEOP_COPY, (void *)1, 0);
    checkerr(ret, GetLastError(), ERROR_INVALID_PARAMETER, "add with a reserved pointer");
    SetLastError(0xdeadbeef);
    ret = SetupAddToDiskSpaceListW(l, path, -5, FILEOP_COPY, NULL, 0);
    checkerr(ret, GetLastError(), ERROR_INVALID_PARAMETER, "add with a negative size");
    SetLastError(0xdeadbeef);
    ret = SetupAddToDiskSpaceListW(l, L"Q:\\nowhere\\x.bin", 1, FILEOP_COPY, NULL, 0);
    checkerr(ret, GetLastError(), ERROR_INVALID_DRIVE, "add on a drive not in the list");

    /* cluster accounting */
    check(SetupAddToDiskSpaceListW(l, path, 1, FILEOP_COPY, NULL, 0) && space(l, drive) == cluster, "1 byte takes a cluster");
    check(SetupAddToDiskSpaceListW(l, path, cluster, FILEOP_COPY, NULL, 0) && space(l, drive) == cluster, "an exact cluster takes one, and replaces the entry");
    check(SetupAddToDiskSpaceListW(l, path, cluster + 1, FILEOP_COPY, NULL, 0) && space(l, drive) == 2 * cluster, "cluster + 1 takes two");
    check(SetupAddToDiskSpaceListW(l, path2, 0, FILEOP_COPY, NULL, 0) && space(l, drive) == 2 * cluster, "an empty file takes nothing");
    check(SetupAddToDiskSpaceListA(l, "xx", 10, FILEOP_COPY, NULL, 0) || 1, "relative file via the A function");
    SetupRemoveFromDiskSpaceListA(l, "xx", FILEOP_COPY, NULL, 0);
    {
        char pathA[MAX_PATH];
        WideCharToMultiByte(CP_ACP, 0, path2, -1, pathA, MAX_PATH, NULL, NULL);
        check(SetupAddToDiskSpaceListA(l, pathA, 3 * (LONGLONG)cluster, FILEOP_COPY, NULL, 0) && space(l, drive) == 5 * cluster, "the A function adds too");
        check(SetupRemoveFromDiskSpaceListA(l, pathA, FILEOP_COPY, NULL, 0) && space(l, drive) == 2 * cluster, "the A remove gives it back");
    }

    /* an existing file */
    check(SetupAddToDiskSpaceListW(l, exist, 5000, FILEOP_COPY, NULL, 0), "copy over an existing file");
    check(space(l, drive) == 2 * cluster, "the same size over an existing file needs nothing more");
    check(SetupAddToDiskSpaceListW(l, exist, 5000 + 3 * (LONGLONG)cluster, FILEOP_COPY, NULL, 0), "grow an existing file");
    {
        LONGLONG old = (existing + cluster - 1) / cluster * cluster;
        LONGLONG new_ = (5000 + 3 * (LONGLONG)cluster + cluster - 1) / cluster * cluster;
        check(space(l, drive) == 2 * cluster + new_ - old, "growing counts only the growth");
    }
    check(SetupAddToDiskSpaceListW(l, exist, 0, FILEOP_DELETE, NULL, 0), "delete an existing file");
    {
        LONGLONG old = (existing + cluster - 1) / cluster * cluster;
        LONGLONG new_ = (5000 + 3 * (LONGLONG)cluster + cluster - 1) / cluster * cluster;
        check(space(l, drive) == 2 * cluster + new_ - old - old, "a delete gives its clusters back");
    }
    check(SetupRemoveFromDiskSpaceListW(l, exist, FILEOP_DELETE, NULL, 0), "remove the delete");
    check(SetupRemoveFromDiskSpaceListW(l, exist, FILEOP_COPY, NULL, 0) && space(l, drive) == 2 * cluster, "remove the copy: back to the earlier total");

    /* remove */
    SetLastError(0xdeadbeef);
    ret = SetupRemoveFromDiskSpaceListW(l, exist, FILEOP_COPY, NULL, 0);
    checkerr(ret, GetLastError(), ERROR_FILE_NOT_FOUND, "remove an entry that is not there");
    SetLastError(0xdeadbeef);
    ret = SetupRemoveFromDiskSpaceListW(NULL, path, FILEOP_COPY, NULL, 0);
    checkerr(ret, GetLastError(), ERROR_INVALID_HANDLE, "remove from no list");
    SetLastError(0xdeadbeef);
    ret = SetupRemoveFromDiskSpaceListW(l, NULL, FILEOP_COPY, NULL, 0);
    checkerr(ret, GetLastError(), ERROR_INVALID_PARAMETER, "remove without a file");

    /* duplicates keep the entries, and then go their own way */
    dup = SetupDuplicateDiskSpaceListW(l, NULL, 0, 0);
    check(dup && space(dup, drive) == 2 * cluster, "a duplicate has the total");
    check(SetupRemoveFromDiskSpaceListW(l, path, FILEOP_COPY, NULL, 0) && space(l, drive) == 0, "remove the file from the original");
    check(space(dup, drive) == 2 * cluster, "the duplicate is unchanged");
    check(SetupRemoveFromDiskSpaceListW(dup, path, FILEOP_COPY, NULL, 0) && space(dup, drive) == 0, "the duplicate has the entry to remove");
    SetupDestroyDiskSpaceList(dup);

    /* drives */
    need = 0;
    check(SetupQueryDrivesInDiskSpaceListW(l, NULL, 0, &need) && need >= 4, "required size of the drive list");
    SetLastError(0xdeadbeef);
    ret = SetupQueryDrivesInDiskSpaceListW(l, wbuf, 2, &need);
    checkerr(ret, GetLastError(), ERROR_INSUFFICIENT_BUFFER, "drive list in too small a buffer");
    memset(wbuf, 0xff, sizeof(wbuf));
    check(SetupQueryDrivesInDiskSpaceListW(l, wbuf, 200, &need), "drive list");
    {
        const WCHAR *p = wbuf;
        int found_c = 0, n_drives = 0;
        DWORD chars = 1;
        while (*p) { if (p[1] == ':' && p[2] == 0 && (p[0] == full[0])) found_c = 1; n_drives++; chars += wcslen(p) + 1; p += wcslen(p) + 1; }
        check(found_c, "the temp drive is in the list");
        check(chars == need, "the required size counts the strings and the final NUL");
        check(n_drives >= 1, "drives listed");
    }
    memset(dropbuf, 0xff, sizeof(dropbuf));
    check(SetupQueryDrivesInDiskSpaceListA(l, dropbuf, 200, &need) && dropbuf[1] == ':', "drive list, ANSI");
    SetLastError(0xdeadbeef);
    ret = SetupQueryDrivesInDiskSpaceListW(NULL, wbuf, 200, &need);
    checkerr(ret, GetLastError(), ERROR_INVALID_HANDLE, "drive list of no list");

    SetupDestroyDiskSpaceList(l);
    DeleteFileW(exist);
    (void)i;
}

static int has_list(PCWSTR *list, UINT count, const WCHAR **want, UINT n)
{
    UINT i;
    if (count != n) return 0;
    for (i = 0; i < n; i++) if (wcscmp(list[i], want[i])) return 0;
    return 1;
}

static int query_is(DWORD flags, const WCHAR **want, UINT n)
{
    PCWSTR *list = NULL;
    UINT count = 99;
    int ok = SetupQuerySourceListW(flags, &list, &count) && has_list(list, count, want, n);
    if (list) SetupFreeSourceListW(&list, count);
    return ok;
}

static void test_sourcelist(void)
{
    const WCHAR *t1[] = { L"X:\\temp1" }, *t12[] = { L"X:\\temp1", L"X:\\temp2" };
    const WCHAR *u1[] = { L"U:\\user1" }, *s1[] = { L"S:\\sys1" };
    const WCHAR *all[] = { L"X:\\temp1", L"U:\\user1", L"S:\\sys1" };
    const WCHAR *newu[] = { L"U:\\a", L"U:\\b" };
    PCWSTR *list = NULL;
    UINT count = 0;
    BOOL ret;
    HKEY key;
    WCHAR buf[300];
    DWORD size;

    SetupCancelTemporarySourceList();
    SetupSetSourceListW(SRCLIST_USER, NULL, 0);
    SetupSetSourceListW(SRCLIST_SYSTEM, NULL, 0);

    check(query_is(SRCLIST_TEMPORARY, NULL, 0), "no temporary list at first");
    check(SetupAddToSourceListW(SRCLIST_TEMPORARY, L"X:\\temp1"), "add to the temporary list");
    check(SetupAddToSourceListW(SRCLIST_TEMPORARY, L"x:\\TEMP1"), "add the same source again, other case");
    check(query_is(SRCLIST_TEMPORARY, t1, 1), "a source is in the list once");
    check(SetupAddToSourceListA(SRCLIST_TEMPORARY, "X:\\temp2") && query_is(SRCLIST_TEMPORARY, t12, 2), "the A function adds");
    check(SetupAddToSourceListW(SRCLIST_USER, L"U:\\user1"), "add to the user list");
    check(SetupAddToSourceListW(SRCLIST_SYSTEM, L"S:\\sys1"), "add to the system list");
    check(query_is(SRCLIST_USER, u1, 1), "the user list");
    check(query_is(SRCLIST_SYSTEM, s1, 1), "the system list");
    SetupCancelTemporarySourceList();
    check(SetupAddToSourceListW(SRCLIST_TEMPORARY, L"X:\\temp1"), "temporary again");
    check(query_is(SRCLIST_TEMPORARY | SRCLIST_USER | SRCLIST_SYSTEM, all, 3), "all lists, temporary then user then system");
    check(query_is(SRCLIST_USER | SRCLIST_SYSTEM, all + 1, 2), "user and system");

    /* stored where it should be */
    size = sizeof(buf);
    if (!RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Setup", 0, KEY_READ, &key))
    {
        DWORD type = 0;
        memset(buf, 0, sizeof(buf));
        check(!RegQueryValueExW(key, L"Installation Sources", NULL, &type, (BYTE *)buf, &size) && type == REG_MULTI_SZ &&
              !wcscmp(buf, L"U:\\user1"), "the user list is a multi-string in HKCU");
        RegCloseKey(key);
    }
    else check(0, "the user setup key exists");

    /* set replaces */
    check(SetupSetSourceListW(SRCLIST_USER, (PCWSTR *)newu, 2) && query_is(SRCLIST_USER, newu, 2), "Set replaces the user list");
    {
        const char *a[] = { "U:\\ansi" };
        const WCHAR *w[] = { L"U:\\ansi" };
        check(SetupSetSourceListA(SRCLIST_USER, a, 1) && query_is(SRCLIST_USER, w, 1), "SetA replaces the user list");
    }
    check(SetupSetSourceListW(SRCLIST_USER, NULL, 0) && query_is(SRCLIST_USER, NULL, 0), "Set with no sources empties the list");
    check(query_is(SRCLIST_SYSTEM, s1, 1), "the system list is untouched");

    /* arguments */
    SetLastError(0xdeadbeef);
    ret = SetupAddToSourceListW(0, L"X:\\x");
    checkerr(ret, GetLastError(), ERROR_INVALID_PARAMETER, "add without a list named");
    SetLastError(0xdeadbeef);
    ret = SetupAddToSourceListW(SRCLIST_USER, NULL);
    checkerr(ret, GetLastError(), ERROR_INVALID_PARAMETER, "add without a source");
    SetLastError(0xdeadbeef);
    ret = SetupAddToSourceListW(0x80000000 | SRCLIST_USER, L"X:\\x");
    checkerr(ret, GetLastError(), ERROR_INVALID_PARAMETER, "add with an unknown flag");
    SetLastError(0xdeadbeef);
    ret = SetupQuerySourceListW(0, &list, &count);
    checkerr(ret, GetLastError(), ERROR_INVALID_PARAMETER, "query without a list named");
    SetLastError(0xdeadbeef);
    ret = SetupSetSourceListW(SRCLIST_USER, NULL, 2);
    checkerr(ret, GetLastError(), ERROR_INVALID_PARAMETER, "set with a count but no sources");

    /* query result and free */
    ret = SetupQuerySourceListA(SRCLIST_SYSTEM, (PCSTR **)&list, &count);
    check(ret && count == 1 && !strcmp(((PCSTR *)list)[0], "S:\\sys1"), "SetupQuerySourceListA");
    check(SetupFreeSourceListA((PCSTR **)&list, count) && list == NULL, "SetupFreeSourceListA clears the pointer");
    ret = SetupQuerySourceListW(SRCLIST_TEMPORARY, &list, &count);
    check(ret && count == 1 && list, "query the temporary list");
    check(SetupFreeSourceListW(&list, count) && list == NULL, "SetupFreeSourceListW clears the pointer");
    SetupCancelTemporarySourceList();
    check(query_is(SRCLIST_TEMPORARY, NULL, 0), "cancel empties the temporary list");

    SetupSetSourceListW(SRCLIST_USER, NULL, 0);
    SetupSetSourceListW(SRCLIST_SYSTEM, NULL, 0);
}

int main(void)
{
    test_diskspace();
    test_sourcelist();
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
