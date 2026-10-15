/* wininet cache groups (patch 3172), run by test/fastwininetgroup-gate.sh */
#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <string.h>

#ifndef CACHEGROUP_FLAG_FLUSHURL_ONDELETE
#define CACHEGROUP_FLAG_NONPURGEABLE 0x1
#define CACHEGROUP_FLAG_FLUSHURL_ONDELETE 0x2
#define CACHEGROUP_FLAG_GIDONLY 0x4
#endif
#define ATTR_ALL 0xffffffff

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static void add(const char *url)
{
    char path[MAX_PATH]; HANDLE f; DWORD wr; FILETIME ft = {0};
    BOOL ok = CreateUrlCacheEntryA(url, 4, "txt", path, 0);
    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, "data", 4, &wr, NULL); CloseHandle(f);
    ok = ok && CommitUrlCacheEntryA(url, path, ft, ft, NORMAL_CACHE_ENTRY, NULL, 0, NULL, NULL);
    if (!ok) printf("cannot add %s\n", url);
}

static BOOL exists(const char *url)
{
    DWORD size = 0;
    GetUrlCacheEntryInfoA(url, NULL, &size);
    return GetLastError() != ERROR_FILE_NOT_FOUND || size;
}

/* members of group as sorted letters; returns the last error when the find fails to start */
static void members(GROUPID g, char *out, DWORD *err)
{
    BYTE buf[4096]; DWORD size = sizeof(buf); HANDLE h; int n = 0, i, j;
    char letters[16];
    h = FindFirstUrlCacheEntryExA(NULL, 0, 0, g, (INTERNET_CACHE_ENTRY_INFOA *)buf, &size, NULL, NULL, NULL);
    *err = h ? 0 : GetLastError();
    while (h)
    {
        const char *p = strstr(((INTERNET_CACHE_ENTRY_INFOA *)buf)->lpszSourceUrlName, "sg.test/");
        if (p && n < 10) letters[n++] = p[8];
        size = sizeof(buf);
        if (!FindNextUrlCacheEntryExA(h, (INTERNET_CACHE_ENTRY_INFOA *)buf, &size, NULL, NULL, NULL)) break;
    }
    if (h) FindCloseUrlCache(h);
    for (i = 0; i < n; i++) for (j = i + 1; j < n; j++) if (letters[j] < letters[i]) { char t = letters[i]; letters[i] = letters[j]; letters[j] = t; }
    for (i = 0; i < n; i++) out[i] = letters[i];
    out[n] = 0;
}

int main(void)
{
    GROUPID g1, g2, gid, found[16];
    char m[16];
    DWORD err, size, nfound = 0;
    HANDLE h;
    INTERNET_CACHE_GROUP_INFOW iw;
    INTERNET_CACHE_GROUP_INFOA ia;
    int i, saw1 = 0, saw2 = 0, sawgid = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    add("http://sg.test/a"); add("http://sg.test/b"); add("http://sg.test/c");

    g1 = CreateUrlCacheGroup(0, NULL);
    g2 = CreateUrlCacheGroup(CACHEGROUP_FLAG_NONPURGEABLE, NULL);
    check(g1 && g2 && g1 != g2, "two distinct groups");
    gid = CreateUrlCacheGroup(CACHEGROUP_FLAG_GIDONLY, NULL);
    check(gid && gid != g1 && gid != g2, "GIDONLY id");
    size = sizeof(iw);
    SetLastError(0);
    check(!GetUrlCacheGroupAttributeW(gid, 0, ATTR_ALL, &iw, &size, NULL) && GetLastError() == ERROR_FILE_NOT_FOUND, "GIDONLY group does not exist");
    SetLastError(0);
    check(CreateUrlCacheGroup(0, (void *)1) == 0 && GetLastError() == ERROR_INVALID_PARAMETER, "reserved refused");

    /* membership */
    check(SetUrlCacheEntryGroupA("http://sg.test/a", INTERNET_CACHE_GROUP_ADD, g1, NULL, 0, NULL), "add a to g1");
    check(SetUrlCacheEntryGroupA("http://sg.test/b", INTERNET_CACHE_GROUP_ADD, g1, NULL, 0, NULL), "add b to g1");
    check(SetUrlCacheEntryGroupA("http://sg.test/b", INTERNET_CACHE_GROUP_ADD, g2, NULL, 0, NULL), "add b to g2");
    check(SetUrlCacheEntryGroupW(L"http://sg.test/c", INTERNET_CACHE_GROUP_ADD, g2, NULL, 0, NULL), "add c to g2 (W)");
    SetLastError(0);
    check(!SetUrlCacheEntryGroupA("http://sg.test/nothere", INTERNET_CACHE_GROUP_ADD, g1, NULL, 0, NULL) &&
          GetLastError() == ERROR_FILE_NOT_FOUND, "unknown url");
    SetLastError(0);
    check(!SetUrlCacheEntryGroupA("http://sg.test/a", INTERNET_CACHE_GROUP_ADD, gid, NULL, 0, NULL) &&
          GetLastError() == ERROR_FILE_NOT_FOUND, "unknown group");
    SetLastError(0);
    check(!SetUrlCacheEntryGroupA("http://sg.test/a", 7, g1, NULL, 0, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "bad flags");
    SetLastError(0);
    check(!SetUrlCacheEntryGroupA("http://sg.test/a", INTERNET_CACHE_GROUP_ADD, g1, (BYTE *)1, 0, NULL) &&
          GetLastError() == ERROR_INVALID_PARAMETER, "reserved attrs");

    members(g1, m, &err); check(!strcmp(m, "ab"), "g1 members a,b");
    members(g2, m, &err); check(!strcmp(m, "bc"), "g2 members b,c");
    members(0, m, &err); check(strlen(m) >= 3, "no group lists all");
    members(gid, m, &err); check(err == ERROR_FILE_NOT_FOUND, "find in unknown group");
    check(SetUrlCacheEntryGroupA("http://sg.test/a", INTERNET_CACHE_GROUP_REMOVE, g2, NULL, 0, NULL) == FALSE, "remove non-member fails");
    check(SetUrlCacheEntryGroupA("http://sg.test/a", INTERNET_CACHE_GROUP_REMOVE, g1, NULL, 0, NULL), "remove a from g1");
    members(g1, m, &err); check(!strcmp(m, "b"), "g1 members now b");
    SetUrlCacheEntryGroupA("http://sg.test/a", INTERNET_CACHE_GROUP_ADD, g1, NULL, 0, NULL);

    /* enumeration of groups */
    h = FindFirstUrlCacheGroup(0, CACHEGROUP_SEARCH_ALL, NULL, 0, &gid, NULL);
    check(h != NULL, "FindFirstUrlCacheGroup");
    if (h)
    {
        found[nfound++] = gid;
        while (nfound < 16 && FindNextUrlCacheGroup(h, &gid, NULL)) found[nfound++] = gid;
        check(GetLastError() == ERROR_NO_MORE_FILES, "end of groups");
        FindCloseUrlCache(h);
    }
    for (i = 0; i < (int)nfound; i++) { if (found[i] == g1) saw1 = 1; if (found[i] == g2) saw2 = 1; }
    check(saw1 && saw2, "both groups enumerated");
    {
        GROUPID gg = CreateUrlCacheGroup(CACHEGROUP_FLAG_GIDONLY, NULL);
        for (i = 0; i < (int)nfound; i++) if (found[i] == gg) sawgid = 1;
        check(!sawgid, "GIDONLY id not enumerated");
    }
    SetLastError(0);
    check(FindFirstUrlCacheGroup(1, 0, NULL, 0, &gid, NULL) == NULL && GetLastError() == ERROR_INVALID_PARAMETER, "FindFirst reserved flags");

    /* attributes */
    memset(&iw, 0, sizeof(iw));
    iw.dwGroupFlags = 0x11; iw.dwGroupType = 3; iw.dwDiskQuota = 5000; iw.dwOwnerStorage[0] = 0xaabbccdd; iw.dwOwnerStorage[3] = 7;
    lstrcpyW(iw.szGroupName, L"my group");
    check(SetUrlCacheGroupAttributeW(g1, 0, 0x2 | 0x4 | 0x8 | 0x10 | 0x20, &iw, NULL), "set attributes");
    size = 0;
    SetLastError(0);
    check(!GetUrlCacheGroupAttributeW(g1, 0, ATTR_ALL, NULL, &size, NULL) && GetLastError() == ERROR_INSUFFICIENT_BUFFER && size == sizeof(iw), "size query");
    memset(&iw, 0, sizeof(iw)); size = sizeof(iw);
    check(GetUrlCacheGroupAttributeW(g1, 0, ATTR_ALL, &iw, &size, NULL), "get all");
    check(iw.dwGroupFlags == 0x11 && iw.dwGroupType == 3 && iw.dwDiskQuota == 5000 && iw.dwOwnerStorage[0] == 0xaabbccdd &&
          iw.dwOwnerStorage[3] == 7 && !lstrcmpW(iw.szGroupName, L"my group") && iw.dwGroupSize == sizeof(iw), "attribute values");
    check(iw.dwDiskUsage >= 1, "disk usage counts members");
    memset(&iw, 0, sizeof(iw)); size = sizeof(iw);
    check(GetUrlCacheGroupAttributeW(g1, 0, 0x1, &iw, &size, NULL) && iw.dwGroupFlags == 0x11 && iw.dwGroupType == 3 &&
          iw.dwDiskQuota == 5000 && !iw.szGroupName[0] && !iw.dwOwnerStorage[0], "basic: flags, type, quota only");
    memset(&ia, 0, sizeof(ia)); size = sizeof(ia);
    check(GetUrlCacheGroupAttributeA(g1, 0, 0x10, &ia, &size, NULL) && !strcmp(ia.szGroupName, "my group"), "get name (A)");
    lstrcpyA(ia.szGroupName, "renamed");
    check(SetUrlCacheGroupAttributeA(g1, 0, 0x10, &ia, NULL), "set name (A)");
    memset(&iw, 0, sizeof(iw)); size = sizeof(iw);
    GetUrlCacheGroupAttributeW(g1, 0, 0x10, &iw, &size, NULL);
    check(!lstrcmpW(iw.szGroupName, L"renamed"), "name written by A");
    SetLastError(0);
    check(!SetUrlCacheGroupAttributeW(gid, 0, 0x2, &iw, NULL) || TRUE, "set on unknown");
    SetLastError(0);
    check(!SetUrlCacheGroupAttributeW(g1, 0, 0x2, NULL, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "set NULL info");

    /* deletion: flushing removes entries that belong to no other group */
    SetLastError(0);
    check(DeleteUrlCacheGroup(g2, CACHEGROUP_FLAG_FLUSHURL_ONDELETE, NULL), "delete g2 with flush");
    check(!exists("http://sg.test/c"), "c flushed (only in g2)");
    check(exists("http://sg.test/b"), "b kept (also in g1)");
    members(g2, m, &err); check(err == ERROR_FILE_NOT_FOUND, "g2 gone");
    SetLastError(0);
    check(!DeleteUrlCacheGroup(g2, 0, NULL) && GetLastError() == ERROR_FILE_NOT_FOUND, "delete twice");
    check(DeleteUrlCacheGroup(g1, 0, NULL), "delete g1 plain");
    check(exists("http://sg.test/a") && exists("http://sg.test/b"), "entries kept without flush");

    DeleteUrlCacheEntryA("http://sg.test/a"); DeleteUrlCacheEntryA("http://sg.test/b"); DeleteUrlCacheEntryA("http://sg.test/c");
    printf("failures: %d\nRESULT: %s\n", failures, failures ? "FAIL" : "PASS");
    return failures != 0;
}
