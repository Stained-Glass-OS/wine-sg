#include <windows.h>
#include <wininet.h>
#include <stdio.h>
static int bad, total;
#define EXPECT(c) do { total++; if (!(c)) { bad++; printf("MISMATCH line %d: %s (err %lu)\n", __LINE__, #c, GetLastError()); } } while (0)

typedef struct { DWORD dwCacheVersion; WCHAR *lpszName, *lpszCachePrefix, *lpszVolumeLabel, *lpszVolumeTitle; } info_w;

static BOOL (WINAPI *pCreateW)(const WCHAR *, const WCHAR *, const WCHAR *, DWORD, DWORD, DWORD, void *, DWORD *);
static BOOL (WINAPI *pDeleteW)(const WCHAR *, DWORD);
static BOOL (WINAPI *pCreateA)(const char *, const char *, const char *, DWORD, DWORD, DWORD, void *, DWORD *);
static BOOL (WINAPI *pDeleteA)(const char *, DWORD);
static HANDLE (WINAPI *pFirst)(DWORD *, void *, DWORD *, DWORD);
static BOOL (WINAPI *pNext)(HANDLE, void *, DWORD *);

static BOOL listed(const WCHAR *name, WCHAR *prefix_out)
{
    BYTE buf[2048];
    DWORD size = sizeof(buf), modified;
    HANDLE h = pFirst(&modified, buf, &size, 0);
    BOOL found = FALSE;

    if (!h) return FALSE;
    do
    {
        info_w *info = (info_w *)buf;
        if (!lstrcmpiW(info->lpszName, name)) { found = TRUE; if (prefix_out) lstrcpyW(prefix_out, info->lpszCachePrefix); }
        size = sizeof(buf);
    } while (pNext(h, buf, &size));
    FindCloseUrlCache(h);
    return found;
}

int main(void)
{
    HMODULE wininet = LoadLibraryA("wininet.dll");
    WCHAR dir[MAX_PATH], prefix[MAX_PATH];
    BYTE entry_buf[2048];
    DWORD size;
    char path_a[MAX_PATH];
    INTERNET_CACHE_ENTRY_INFOA *e = (void *)entry_buf;
    HANDLE h;

    setvbuf(stdout, NULL, _IONBF, 0);
    pCreateW = (void *)GetProcAddress(wininet, "CreateUrlCacheContainerW");
    pDeleteW = (void *)GetProcAddress(wininet, "DeleteUrlCacheContainerW");
    pCreateA = (void *)GetProcAddress(wininet, "CreateUrlCacheContainerA");
    pDeleteA = (void *)GetProcAddress(wininet, "DeleteUrlCacheContainerA");
    pFirst = (void *)GetProcAddress(wininet, "FindFirstUrlCacheContainerW");
    pNext = (void *)GetProcAddress(wininet, "FindNextUrlCacheContainerW");
    EXPECT(pCreateW && pDeleteW && pCreateA && pDeleteA && pFirst && pNext);

    GetTempPathW(MAX_PATH, dir);
    lstrcatW(dir, L"fastwininetrc");

    EXPECT(!listed(L"SGTest", NULL));
    EXPECT(pCreateW(L"SGTest", L"sgtest:", dir, 100, 0, 0, NULL, NULL));
    EXPECT(listed(L"SGTest", prefix) && !lstrcmpW(prefix, L"sgtest:"));
    EXPECT(GetFileAttributesW(dir) != INVALID_FILE_ATTRIBUTES);

    /* entries with the prefix are kept in the new container's directory */
    size = sizeof(entry_buf);
    EXPECT(CreateUrlCacheEntryA("sgtest:http://example.org/a", 0, "txt", path_a, 0));
    {
        char dirA[MAX_PATH];
        WideCharToMultiByte(CP_ACP, 0, dir, -1, dirA, MAX_PATH, NULL, NULL);
        EXPECT(!_strnicmp(path_a, dirA, strlen(dirA)));
        if (_strnicmp(path_a, dirA, strlen(dirA))) printf("  path %s dir %s\n", path_a, dirA);
    }
    DeleteFileA(path_a);
    (void)e; (void)h;

    SetLastError(0xdeadbeef);
    EXPECT(!pCreateW(L"SGTest", L"sgtest2:", dir, 100, 0, 0, NULL, NULL) && GetLastError() == ERROR_ALREADY_EXISTS);
    SetLastError(0xdeadbeef);
    EXPECT(!pCreateW(NULL, L"p:", dir, 0, 0, 0, NULL, NULL) && GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    EXPECT(!pCreateW(L"SGTest3", L"p3:", dir, 0, 0, 1, NULL, NULL) && GetLastError() == ERROR_INVALID_PARAMETER);

    SetLastError(0xdeadbeef);
    EXPECT(!pDeleteW(L"SGTest", 1) && GetLastError() == ERROR_INVALID_PARAMETER);
    EXPECT(pDeleteW(L"SGTest", 0));
    EXPECT(!listed(L"SGTest", NULL));
    SetLastError(0xdeadbeef);
    EXPECT(!pDeleteW(L"SGTest", 0) && GetLastError() == ERROR_FILE_NOT_FOUND);
    SetLastError(0xdeadbeef);
    EXPECT(!pDeleteW(L"Content", 0) && GetLastError() == ERROR_FILE_NOT_FOUND);

    /* the ANSI forms */
    EXPECT(pCreateA("SGTestA", "sgtesta:", NULL, 0, 0, 0, NULL, NULL));
    EXPECT(listed(L"SGTestA", NULL));
    EXPECT(pDeleteA("SGTestA", 0));
    EXPECT(!listed(L"SGTestA", NULL));

    /* the default containers are still listed */
    EXPECT(listed(L"Content", NULL) && listed(L"Cookies", NULL) && listed(L"History", NULL));

    RemoveDirectoryW(dir);
    printf("checks %d, mismatches %d\nRESULT: %s\n", total, bad, bad ? "FAIL" : "PASS");
    return bad != 0;
}
