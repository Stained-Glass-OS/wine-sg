#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <string.h>
#define P(...) printf(__VA_ARGS__)

typedef struct { DWORD dwAccessType; const WCHAR *lpszProxy, *lpszProxyBypass; } INTERNET_PROXY_INFOW_;
typedef struct { DWORD dwAccessType; const char *lpszProxy, *lpszProxyBypass; } INTERNET_PROXY_INFOA_;
typedef struct { DWORD ver; char *name, *prefix, *label, *title; } cinfoA;
typedef struct { DWORD ver; WCHAR *name, *prefix, *label, *title; } cinfoW;
static HANDLE (WINAPI *pFindFirstA)(DWORD *, void *, DWORD *, DWORD);
static HANDLE (WINAPI *pFindFirstW)(DWORD *, void *, DWORD *, DWORD);
static BOOL (WINAPI *pFindNextA)(HANDLE, void *, DWORD *);
static BOOL (WINAPI *pFindNextW)(HANDLE, void *, DWORD *);

static void proxy_query(const char *tag, HINTERNET h)
{
    BYTE buf[1024]; DWORD size = sizeof(buf); INTERNET_PROXY_INFOA_ *pi = (INTERNET_PROXY_INFOA_ *)buf;
    BOOL ok = InternetQueryOptionA(h, INTERNET_OPTION_PROXY, buf, &size);
    P("%s=%d %lu %s %s\n", tag, ok, ok ? pi->dwAccessType : 0, ok && pi->lpszProxy ? pi->lpszProxy : "-", ok && pi->lpszProxyBypass ? pi->lpszProxyBypass : "-");
}

int main(void)
{
    HINTERNET h; INTERNET_PROXY_INFOW_ piw; INTERNET_PROXY_INFOA_ pia; BOOL ok; HMODULE m = GetModuleHandleA("wininet.dll");
    BYTE buf[1024]; DWORD size, n; HANDLE f; char names[128] = "";
    setvbuf(stdout, NULL, _IONBF, 0);
    pFindFirstA = (void *)GetProcAddress(m, "FindFirstUrlCacheContainerA");
    pFindFirstW = (void *)GetProcAddress(m, "FindFirstUrlCacheContainerW");
    pFindNextA = (void *)GetProcAddress(m, "FindNextUrlCacheContainerA");
    pFindNextW = (void *)GetProcAddress(m, "FindNextUrlCacheContainerW");

    h = InternetOpenA("probe", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    proxy_query("P0", h);
    piw.dwAccessType = INTERNET_OPEN_TYPE_PROXY; piw.lpszProxy = L"proxy.example:8080"; piw.lpszProxyBypass = L"*.local";
    ok = InternetSetOptionW(h, INTERNET_OPTION_PROXY, &piw, sizeof(piw));
    P("P1set=%d %lu\n", ok, GetLastError());
    proxy_query("P1", h);
    pia.dwAccessType = INTERNET_OPEN_TYPE_PROXY; pia.lpszProxy = "other.example:3128"; pia.lpszProxyBypass = "<local>";
    ok = InternetSetOptionA(h, INTERNET_OPTION_PROXY, &pia, sizeof(pia));
    P("P2set=%d\n", ok);
    proxy_query("P2", h);
    piw.dwAccessType = INTERNET_OPEN_TYPE_DIRECT; piw.lpszProxy = NULL; piw.lpszProxyBypass = NULL;
    ok = InternetSetOptionW(h, INTERNET_OPTION_PROXY, &piw, sizeof(piw));
    P("P3set=%d\n", ok);
    proxy_query("P3", h);
    SetLastError(0);
    ok = InternetSetOptionW(h, INTERNET_OPTION_PROXY, &piw, 4);
    P("P4=%d %lu\n", ok, GetLastError());
    piw.dwAccessType = INTERNET_OPEN_TYPE_PROXY; piw.lpszProxy = NULL;
    SetLastError(0);
    ok = InternetSetOptionW(h, INTERNET_OPTION_PROXY, &piw, sizeof(piw));
    P("P5=%d %lu\n", ok, GetLastError());
    piw.dwAccessType = 77; piw.lpszProxy = L"x";
    SetLastError(0);
    ok = InternetSetOptionW(h, INTERNET_OPTION_PROXY, &piw, sizeof(piw));
    P("P6=%d %lu\n", ok, GetLastError());
    InternetCloseHandle(h);

    /* containers */
    size = 1; SetLastError(0);
    f = pFindFirstW(NULL, buf, &size, 0);
    P("C0=%d %lu %d\n", f != NULL, GetLastError(), size > sizeof(cinfoW));
    size = sizeof(buf);
    f = pFindFirstW(NULL, buf, &size, 0);
    n = 0;
    while (f) {
        cinfoW *c = (cinfoW *)buf; char a[64], b[64];
        WideCharToMultiByte(CP_ACP, 0, c->name, -1, a, sizeof(a), NULL, NULL);
        WideCharToMultiByte(CP_ACP, 0, c->prefix, -1, b, sizeof(b), NULL, NULL);
        sprintf(names + strlen(names), "%s[%s]%d,", a, b, c->label[0] == 0 && c->title[0] == 0);
        n++;
        size = sizeof(buf);
        if (!pFindNextW(f, buf, &size)) break;
    }
    P("C1=%lu %lu %s\n", n, GetLastError(), names);
    P("C1b=%d\n", f ? FindCloseUrlCache(f) : 0);
    size = sizeof(buf); names[0] = 0; n = 0;
    f = pFindFirstA(NULL, buf, &size, 0);
    while (f) {
        cinfoA *c = (cinfoA *)buf;
        sprintf(names + strlen(names), "%s[%s],", c->name, c->prefix);
        n++; size = sizeof(buf);
        if (!pFindNextA(f, buf, &size)) break;
    }
    P("C2=%lu %lu %s\n", n, GetLastError(), names);
    if (f) {
        SetLastError(0); size = sizeof(buf);
        ok = FindNextUrlCacheEntryA(f, (INTERNET_CACHE_ENTRY_INFOA *)buf, &size);
        P("C3=%d %lu\n", ok, GetLastError());
        FindCloseUrlCache(f);
    }
    P("DONE\n");
    return 0;
}
