#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <string.h>
#define P(...) printf(__VA_ARGS__)

static void add(const char *url, DWORD type)
{
    char path[MAX_PATH]; HANDLE f; DWORD wr; FILETIME ft = {0};
    BOOL ok = CreateUrlCacheEntryA(url, 4, "txt", path, 0);
    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, "data", 4, &wr, NULL); CloseHandle(f);
    ok = ok && CommitUrlCacheEntryA(url, path, ft, ft, type, NULL, 0, NULL, NULL);
    P("ADD %s %d\n", url, ok);
}

/* the entries of the test host found with a filter, sorted by name as a string like "a,b" */
static void list(const char *tag, BOOL wide, DWORD filter, const char *pattern)
{
    char letters[16] = "", out[64] = ""; int i, j; BYTE buf[4096]; DWORD size; HANDLE h; int n = 0;
    size = sizeof(buf);
    if (wide) {
        WCHAR wp[64]; MultiByteToWideChar(CP_ACP, 0, pattern ? pattern : "", -1, wp, 64);
        h = FindFirstUrlCacheEntryExW(pattern ? wp : NULL, 0, filter, 0, (INTERNET_CACHE_ENTRY_INFOW *)buf, &size, NULL, NULL, NULL);
    } else
        h = FindFirstUrlCacheEntryExA(pattern, 0, filter, 0, (INTERNET_CACHE_ENTRY_INFOA *)buf, &size, NULL, NULL, NULL);
    while (h) {
        char name[512];
        if (wide) WideCharToMultiByte(CP_ACP, 0, ((INTERNET_CACHE_ENTRY_INFOW *)buf)->lpszSourceUrlName, -1, name, sizeof(name), NULL, NULL);
        else lstrcpyA(name, ((INTERNET_CACHE_ENTRY_INFOA *)buf)->lpszSourceUrlName);
        if (strstr(name, "sg.test/") && n < 10) letters[n++] = strstr(name, "sg.test/")[8];
        size = sizeof(buf);
        if (wide ? !FindNextUrlCacheEntryExW(h, (INTERNET_CACHE_ENTRY_INFOW *)buf, &size, NULL, NULL, NULL)
                 : !FindNextUrlCacheEntryExA(h, (INTERNET_CACHE_ENTRY_INFOA *)buf, &size, NULL, NULL, NULL)) break;
    }
    if (h) FindCloseUrlCache(h);
    for (i = 0; i < n; i++) for (j = i + 1; j < n; j++) if (letters[j] < letters[i]) { char t = letters[i]; letters[i] = letters[j]; letters[j] = t; }
    for (i = 0, j = 0; i < n; i++) j += sprintf(out + j, "%s%c", i ? "," : "", letters[i]);
    P("%s %s\n", tag, out);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    add("http://sg.test/a", NORMAL_CACHE_ENTRY);
    add("http://sg.test/b", STICKY_CACHE_ENTRY);
    add("http://sg.test/c", EDITED_CACHE_ENTRY);
    list("ALL_A", FALSE, 0, NULL);
    list("NORMAL_A", FALSE, NORMAL_CACHE_ENTRY, NULL);
    list("STICKY_A", FALSE, STICKY_CACHE_ENTRY, NULL);
    list("NORMAL_W", TRUE, NORMAL_CACHE_ENTRY, NULL);
    list("STICKY_W", TRUE, STICKY_CACHE_ENTRY, NULL);
    list("EDITED_A", FALSE, EDITED_CACHE_ENTRY, NULL);
    list("COOKIE_A", FALSE, 0, "cookie:");
    { BYTE buf[256]; DWORD size = sizeof(buf); HANDLE h = FindFirstUrlCacheEntryExA(NULL, 0, 0, 1, (INTERNET_CACHE_ENTRY_INFOA *)buf, &size, NULL, NULL, NULL);
      P("GROUP %d %lu\n", h != NULL, GetLastError()); }
    DeleteUrlCacheEntryA("http://sg.test/a"); DeleteUrlCacheEntryA("http://sg.test/b"); DeleteUrlCacheEntryA("http://sg.test/c");
    P("DONE\n");
    return 0;
}
