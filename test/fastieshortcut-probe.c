/* ieframe: InternetShortcut SetURL flags and syntax, icon properties set after Save reach the .url file. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ole2.h>
#include <shlobj.h>
#include <isguids.h>
#include <intshcut.h>
#include <stdio.h>
#include <string.h>

#define PID_IS_ICONINDEX 8
#define PID_IS_ICONFILE 9

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const GUID fmt_intshcut = {0x000214a0, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};

static char *get_url(IUniformResourceLocatorA *u, HRESULT *hr)
{
    char *url = (char *)0xdeadbeef;
    *hr = u->lpVtbl->GetURL(u, &url);
    return url;
}

int main(void)
{
    IUniformResourceLocatorA *u;
    IPersistFile *pf;
    IPropertySetStorage *pss;
    IPropertyStorage *ps;
    PROPSPEC spec[2];
    PROPVARIANT pv[2], rd[2];
    char path[MAX_PATH], buf[400], *url;
    WCHAR wpath[MAX_PATH];
    HANDLE f;
    DWORD n;
    HRESULT hr;

    CoInitialize(NULL);
    CoCreateInstance(&CLSID_InternetShortcut, NULL, CLSCTX_ALL, &IID_IUniformResourceLocatorA, (void **)&u);

    u->lpVtbl->SetURL(u, "somerandomstring", 0);
    url = get_url(u, &hr);
    check(hr == S_FALSE && !url, "plain word is not a URL");
    u->lpVtbl->SetURL(u, "www.winehq.org", 0);
    url = get_url(u, &hr);
    check(hr == S_FALSE && !url, "host without scheme is not a URL without flags");
    u->lpVtbl->SetURL(u, "www.winehq.org", IURL_SETURL_FL_GUESS_PROTOCOL);
    url = get_url(u, &hr);
    check(hr == S_OK && !strcmp(url, "http://www.winehq.org/"), "guessed http");
    u->lpVtbl->SetURL(u, "ftp.winehq.org", IURL_SETURL_FL_GUESS_PROTOCOL);
    url = get_url(u, &hr);
    check(hr == S_OK && !strcmp(url, "ftp://ftp.winehq.org/"), "guessed ftp");
    u->lpVtbl->SetURL(u, "http://x.test/a b", 0);
    url = get_url(u, &hr);
    check(hr == S_OK && !strcmp(url, "http://x.test/a b"), "real URL kept as given");

    /* icon properties set after Save reach the file */
    GetTempPathA(MAX_PATH, path);
    strcat(path, "fastieshortcut.url");
    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAX_PATH);
    u->lpVtbl->SetURL(u, "http://some/bogus/url.html", 0);
    IUnknown_QueryInterface((IUnknown *)u, &IID_IPersistFile, (void **)&pf);
    check(IPersistFile_Save(pf, wpath, TRUE) == S_OK, "save");
    IPersistFile_Release(pf);
    IUnknown_QueryInterface((IUnknown *)u, &IID_IPropertySetStorage, (void **)&pss);
    hr = IPropertySetStorage_Open(pss, &fmt_intshcut, STGM_READWRITE | STGM_SHARE_EXCLUSIVE, &ps);
    check(hr == S_OK, "open property storage");
    spec[0].ulKind = PRSPEC_PROPID; spec[0].propid = PID_IS_ICONFILE;
    spec[1].ulKind = PRSPEC_PROPID; spec[1].propid = PID_IS_ICONINDEX;
    pv[0].vt = VT_LPWSTR; pv[0].pwszVal = (WCHAR *)L"file:///C:/icon/path";
    pv[1].vt = VT_I4; pv[1].lVal = 7;
    check(IPropertyStorage_WriteMultiple(ps, 2, spec, pv, 0) == S_OK, "write icon properties");
    IPropertyStorage_Release(ps);
    IPropertySetStorage_Release(pss);
    u->lpVtbl->Release(u);

    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    n = 0;
    if (f != INVALID_HANDLE_VALUE) { ReadFile(f, buf, sizeof(buf) - 1, &n, NULL); CloseHandle(f); }
    buf[n] = 0;
    check(strstr(buf, "IconFile=file:///C:/icon/path") != NULL, "file has IconFile");
    check(strstr(buf, "IconIndex=7") != NULL, "file has IconIndex");

    /* and a new shortcut loading it reads them back */
    CoCreateInstance(&CLSID_InternetShortcut, NULL, CLSCTX_ALL, &IID_IPersistFile, (void **)&pf);
    check(IPersistFile_Load(pf, wpath, 0) == S_OK, "load");
    IUnknown_QueryInterface((IUnknown *)pf, &IID_IPropertySetStorage, (void **)&pss);
    hr = IPropertySetStorage_Open(pss, &fmt_intshcut, STGM_READ | STGM_SHARE_EXCLUSIVE, &ps);
    memset(rd, 0, sizeof(rd));
    hr = IPropertyStorage_ReadMultiple(ps, 2, spec, rd);
    check(hr == S_OK && rd[1].vt == VT_I4 && rd[1].lVal == 7 && rd[0].vt == VT_LPWSTR
            && !wcscmp(rd[0].pwszVal, L"file:///C:/icon/path"), "icon properties read back");
    IPropertyStorage_Release(ps);
    IPropertySetStorage_Release(pss);
    IPersistFile_Release(pf);
    DeleteFileA(path);

    printf("%s\nDONE\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
