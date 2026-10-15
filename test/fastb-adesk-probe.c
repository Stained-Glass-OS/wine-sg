/* shell32 (3704): IActiveDesktop desktop items */
#define COBJMACROS
#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <stdio.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#ifndef DTI_ADDUI_POSITIONITEM
#define DTI_ADDUI_POSITIONITEM  2
#endif
#ifndef ADDURL_SILENT
#define ADDURL_SILENT 1
#endif

static IActiveDesktop *ad;

static void mk(COMPONENT *c, int type, const WCHAR *src, const WCHAR *name)
{
    memset(c, 0, sizeof(*c));
    c->dwSize = sizeof(*c);
    c->iComponentType = type;
    c->fChecked = TRUE;
    lstrcpyW(c->wszSource, src);
    lstrcpyW(c->wszFriendlyName, name);
    c->cpPos.dwSize = sizeof(c->cpPos);
    c->cpPos.iLeft = 10; c->cpPos.iTop = 20; c->cpPos.dwWidth = 300; c->cpPos.dwHeight = 200;
}

static char *readfile(const WCHAR *path)
{
    static char buf[8192];
    DWORD n = 0;
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    buf[0] = 0;
    if (h != INVALID_HANDLE_VALUE) { ReadFile(h, buf, sizeof(buf) - 1, &n, NULL); buf[n] = 0; CloseHandle(h); }
    return buf;
}

static int child(void)
{
    /* a second process: the items come from the registry */
    IActiveDesktop *a = NULL;
    COMPONENT c;
    COMPONENTSOPT o = { sizeof(o) };
    int n = -1;
    CoInitialize(NULL);
    if (CoCreateInstance(&CLSID_ActiveDesktop, NULL, CLSCTX_INPROC_SERVER, &IID_IActiveDesktop, (void **)&a)) return 100;
    a->lpVtbl->GetDesktopItemCount(a, &n, 0);
    if (n >= 1)
    {
        memset(&c, 0, sizeof(c)); c.dwSize = sizeof(c);
        if (a->lpVtbl->GetDesktopItem(a, 0, &c, 0) != S_OK) return 101;
        if (lstrcmpW(c.wszSource, L"http://example.org/b.htm") || lstrcmpW(c.wszFriendlyName, L"All") || c.fDirty
                || c.cpPos.iLeft != 1 || c.iComponentType != 2) return 102;
    }
    a->lpVtbl->GetDesktopItemOptions(a, &o, 0);
    if (n == 2 && (!o.fEnableComponents || !o.fActiveDesktop)) return 103;
    return n;
}

int main(int argc, char **argv)
{
    COMPONENT c, c2, c3;
    COMPONENTSOPT opt;
    HRESULT hr;
    WCHAR path[MAX_PATH], exe[MAX_PATH], cmd[MAX_PATH + 16];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD code = 0;
    BOOL ok;
    int n, sx = GetSystemMetrics(SM_CXSCREEN);
    char *html;

    if (argc > 1) return child();
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Internet Explorer\\Desktop\\Components");
    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_ActiveDesktop, NULL, CLSCTX_INPROC_SERVER, &IID_IActiveDesktop, (void **)&ad);
    CHECK(hr == S_OK, "create %#lx", hr);
    if (!ad) return 1;

    n = -1; hr = ad->lpVtbl->GetDesktopItemCount(ad, &n, 0);
    CHECK(hr == S_OK && n == 0, "initial count %#lx %d", hr, n);

    /* argument checks */
    hr = ad->lpVtbl->AddDesktopItem(ad, NULL, 0);
    CHECK(hr == E_INVALIDARG, "NULL %#lx", hr);
    mk(&c, 2, L"http://example.org/a.htm", L"Ay");
    c.dwSize = 4;
    hr = ad->lpVtbl->AddDesktopItem(ad, &c, 0);
    CHECK(hr == E_INVALIDARG, "small dwSize %#lx", hr);
    mk(&c, 2, L"", L"Empty");
    hr = ad->lpVtbl->AddDesktopItem(ad, &c, 0);
    CHECK(hr == E_INVALIDARG, "empty source %#lx", hr);
    mk(&c, 9, L"http://example.org/a.htm", L"Ay");
    hr = ad->lpVtbl->AddDesktopItem(ad, &c, 0);
    CHECK(hr == E_INVALIDARG, "bad type %#lx", hr);
    hr = ad->lpVtbl->GetDesktopItemCount(ad, &n, 0);
    CHECK(n == 0, "failed adds left items: %d", n);

    /* add, get, duplicates */
    mk(&c, 2, L"http://example.org/a.htm", L"Ay");
    c.cpPos.izIndex = 3;
    hr = ad->lpVtbl->AddDesktopItem(ad, &c, 0);
    CHECK(hr == S_OK, "add %#lx", hr);
    hr = ad->lpVtbl->AddDesktopItem(ad, &c, 0);
    CHECK(hr == E_FAIL, "duplicate %#lx", hr);
    c2 = c; lstrcpyW(c2.wszSource, L"HTTP://EXAMPLE.ORG/A.HTM");
    hr = ad->lpVtbl->AddDesktopItem(ad, &c2, 0);
    CHECK(hr == E_FAIL, "duplicate (case) %#lx", hr);
    hr = ad->lpVtbl->GetDesktopItemCount(ad, &n, 0);
    CHECK(hr == S_OK && n == 1, "count 1: %d", n);
    memset(&c3, 0, sizeof(c3)); c3.dwSize = sizeof(c3);
    hr = ad->lpVtbl->GetDesktopItem(ad, 0, &c3, 0);
    CHECK(hr == S_OK && c3.dwSize == sizeof(c3) && !lstrcmpW(c3.wszSource, c.wszSource) && !lstrcmpW(c3.wszFriendlyName, L"Ay")
          && c3.iComponentType == 2 && c3.fChecked && c3.cpPos.iLeft == 10 && c3.cpPos.iTop == 20 && c3.cpPos.dwWidth == 300
          && c3.cpPos.izIndex == 3 && c3.dwID != 0 && c3.fDirty && c3.dwCurItemState == 1
          && c3.csiOriginal.iLeft == 10 && c3.csiOriginal.dwHeight == 200, "GetDesktopItem %#lx id %lu", hr, c3.dwID);
    hr = ad->lpVtbl->GetDesktopItem(ad, 1, &c3, 0);
    CHECK(hr == E_INVALIDARG, "index 1 %#lx", hr);
    hr = ad->lpVtbl->GetDesktopItem(ad, -1, &c3, 0);
    CHECK(hr == E_INVALIDARG, "index -1 %#lx", hr);
    c3.dwSize = 8;
    hr = ad->lpVtbl->GetDesktopItem(ad, 0, &c3, 0);
    CHECK(hr == E_INVALIDARG, "small out %#lx", hr);
    memset(&c2, 0, sizeof(c2)); c2.dwSize = sizeof(c2);
    c3.dwSize = sizeof(c3);
    hr = ad->lpVtbl->GetDesktopItem(ad, 0, &c3, 0);
    hr = ad->lpVtbl->GetDesktopItemByID(ad, c3.dwID, &c2, 0);
    CHECK(hr == S_OK && !lstrcmpW(c2.wszSource, c.wszSource), "ByID %#lx", hr);
    hr = ad->lpVtbl->GetDesktopItemByID(ad, c3.dwID + 1000, &c2, 0);
    CHECK(hr == E_FAIL, "ByID unknown %#lx", hr);
    memset(&c2, 0, sizeof(c2)); c2.dwSize = sizeof(c2);
    hr = ad->lpVtbl->GetDesktopItemBySource(ad, L"http://example.org/a.htm", &c2, 0);
    CHECK(hr == S_OK && !lstrcmpW(c2.wszFriendlyName, L"Ay") && c2.dwID == c3.dwID, "BySource %#lx", hr);
    hr = ad->lpVtbl->GetDesktopItemBySource(ad, L"http://nowhere/", &c2, 0);
    CHECK(hr == E_FAIL, "BySource unknown %#lx", hr);
    hr = ad->lpVtbl->GetDesktopItemBySource(ad, NULL, &c2, 0);
    CHECK(hr == E_INVALIDARG, "BySource NULL %#lx", hr);

    /* modify: only the selected members change */
    mk(&c2, 1, L"http://example.org/a.htm", L"Renamed");
    c2.cpPos.iLeft = 99; c2.fNoScroll = TRUE; c2.fChecked = FALSE;
    hr = ad->lpVtbl->ModifyDesktopItem(ad, &c2, COMP_ELEM_FRIENDLYNAME);
    CHECK(hr == S_OK, "modify name %#lx", hr);
    memset(&c3, 0, sizeof(c3)); c3.dwSize = sizeof(c3);
    ad->lpVtbl->GetDesktopItem(ad, 0, &c3, 0);
    CHECK(!lstrcmpW(c3.wszFriendlyName, L"Renamed") && c3.iComponentType == 2 && c3.cpPos.iLeft == 10 && !c3.fNoScroll && c3.fChecked,
          "only the name changed: %ls %d %d", c3.wszFriendlyName, c3.iComponentType, c3.cpPos.iLeft);
    hr = ad->lpVtbl->ModifyDesktopItem(ad, &c2, COMP_ELEM_POS_LEFT | COMP_ELEM_NOSCROLL | COMP_ELEM_CHECKED | COMP_ELEM_TYPE);
    CHECK(hr == S_OK, "modify pos %#lx", hr);
    ad->lpVtbl->GetDesktopItem(ad, 0, &c3, 0);
    CHECK(c3.cpPos.iLeft == 99 && c3.cpPos.iTop == 20 && c3.fNoScroll && !c3.fChecked && c3.iComponentType == 1 && !lstrcmpW(c3.wszFriendlyName, L"Renamed"),
          "pos/noscroll/checked/type: %d %d %d %d", c3.cpPos.iLeft, c3.fNoScroll, c3.fChecked, c3.iComponentType);
    hr = ad->lpVtbl->ModifyDesktopItem(ad, &c2, COMP_ELEM_SIZE_WIDTH | COMP_ELEM_SIZE_HEIGHT | COMP_ELEM_POS_TOP);
    ad->lpVtbl->GetDesktopItem(ad, 0, &c3, 0);
    CHECK(c3.cpPos.dwWidth == 300 && c3.cpPos.dwHeight == 200 && c3.cpPos.iTop == 20, "size flags");
    c2.cpPos.dwWidth = 640; c2.cpPos.iTop = 5;
    ad->lpVtbl->ModifyDesktopItem(ad, &c2, COMP_ELEM_SIZE_WIDTH | COMP_ELEM_POS_TOP);
    ad->lpVtbl->GetDesktopItem(ad, 0, &c3, 0);
    CHECK(c3.cpPos.dwWidth == 640 && c3.cpPos.iTop == 5 && c3.cpPos.dwHeight == 200, "size/top applied");
    hr = ad->lpVtbl->ModifyDesktopItem(ad, &c2, 0x10000);
    CHECK(hr == E_INVALIDARG, "bad flags %#lx", hr);
    mk(&c2, 2, L"http://unknown.example/", L"Nope");
    hr = ad->lpVtbl->ModifyDesktopItem(ad, &c2, COMP_ELEM_ALL);
    CHECK(hr == E_FAIL, "modify unknown %#lx", hr);
    mk(&c2, 2, L"http://example.org/a.htm", L"All");
    c2.cpPos.iLeft = 1; c2.cpPos.iTop = 2; c2.cpPos.dwWidth = 3; c2.cpPos.dwHeight = 4;
    hr = ad->lpVtbl->ModifyDesktopItem(ad, &c2, COMP_ELEM_ALL);
    ad->lpVtbl->GetDesktopItem(ad, 0, &c3, 0);
    CHECK(hr == S_OK && !lstrcmpW(c3.wszFriendlyName, L"All") && c3.cpPos.iLeft == 1 && c3.cpPos.dwHeight == 4 && c3.iComponentType == 2,
          "COMP_ELEM_ALL");
    /* replace the source: the ID names the item */
    c2 = c3; lstrcpyW(c2.wszSource, L"http://example.org/b.htm");
    hr = ad->lpVtbl->ModifyDesktopItem(ad, &c2, COMP_ELEM_SOURCE);
    ad->lpVtbl->GetDesktopItem(ad, 0, &c3, 0);
    CHECK(hr == S_OK && !lstrcmpW(c3.wszSource, L"http://example.org/b.htm"), "source replaced %#lx %ls", hr, c3.wszSource);

    /* with UI + position clamp */
    mk(&c, 0, L"C:\\page.htm", L"Local");
    c.cpPos.iLeft = 5000; c.cpPos.iTop = -40; c.cpPos.dwWidth = 9000;
    hr = ad->lpVtbl->AddDesktopItemWithUI(ad, NULL, &c, DTI_ADDUI_POSITIONITEM);
    CHECK(hr == S_OK && c.dwID != 0, "WithUI %#lx id %lu", hr, c.dwID);
    memset(&c3, 0, sizeof(c3)); c3.dwSize = sizeof(c3);
    hr = ad->lpVtbl->GetDesktopItemBySource(ad, L"C:\\page.htm", &c3, 0);
    CHECK(hr == S_OK && c3.dwID == c.dwID && c3.cpPos.iLeft == sx - 1 && c3.cpPos.iTop == 0 && c3.cpPos.dwWidth == (DWORD)sx,
          "clamped: %d %d %lu (id %lu/%lu)", c3.cpPos.iLeft, c3.cpPos.iTop, c3.cpPos.dwWidth, c3.dwID, c.dwID);
    mk(&c, 0, L"C:\\page2.htm", L"Local2");
    c.cpPos.iLeft = 5000;
    ad->lpVtbl->AddDesktopItemWithUI(ad, NULL, &c, 0);
    ad->lpVtbl->GetDesktopItemBySource(ad, L"C:\\page2.htm", &c3, 0);
    CHECK(c3.cpPos.iLeft == 5000, "no clamp without the flag: %d", c3.cpPos.iLeft);
    hr = ad->lpVtbl->AddDesktopItemWithUI(ad, NULL, NULL, 0);
    CHECK(hr == E_INVALIDARG, "WithUI NULL %#lx", hr);

    /* remove */
    ad->lpVtbl->GetDesktopItemCount(ad, &n, 0);
    CHECK(n == 3, "count 3: %d", n);
    mk(&c, 0, L"c:\\PAGE2.htm", L"x");
    hr = ad->lpVtbl->RemoveDesktopItem(ad, &c, 0);
    CHECK(hr == S_OK, "remove %#lx", hr);
    hr = ad->lpVtbl->RemoveDesktopItem(ad, &c, 0);
    CHECK(hr == E_FAIL, "remove again %#lx", hr);
    hr = ad->lpVtbl->RemoveDesktopItem(ad, NULL, 0);
    CHECK(hr == E_INVALIDARG, "remove NULL %#lx", hr);
    ad->lpVtbl->GetDesktopItemCount(ad, &n, 0);
    CHECK(n == 2, "count 2: %d", n);
    ad->lpVtbl->GetDesktopItem(ad, 1, &c3, 0);
    CHECK(!lstrcmpW(c3.wszSource, L"C:\\page.htm"), "order kept: %ls", c3.wszSource);

    /* AddUrl */
    memset(&c, 0, sizeof(c)); c.dwSize = sizeof(c);
    hr = ad->lpVtbl->AddUrl(ad, NULL, L"http://example.org/pic.JPG", &c, ADDURL_SILENT);
    CHECK(hr == S_OK && c.iComponentType == 1 && !lstrcmpW(c.wszSource, L"http://example.org/pic.JPG") && c.dwID && c.fChecked
          && !lstrcmpW(c.wszFriendlyName, c.wszSource), "AddUrl picture %#lx type %d", hr, c.iComponentType);
    memset(&c2, 0, sizeof(c2)); c2.dwSize = sizeof(c2);
    hr = ad->lpVtbl->AddUrl(ad, NULL, L"http://example.org/index.html", &c2, ADDURL_SILENT);
    CHECK(hr == S_OK && c2.iComponentType == 2, "AddUrl web page %#lx type %d", hr, c2.iComponentType);
    memset(&c2, 0, sizeof(c2)); c2.dwSize = sizeof(c2);
    hr = ad->lpVtbl->AddUrl(ad, NULL, L"C:\\docs\\start.mht", &c2, ADDURL_SILENT);
    CHECK(hr == S_OK && c2.iComponentType == 0, "AddUrl local document %#lx type %d", hr, c2.iComponentType);
    memset(&c2, 0, sizeof(c2)); c2.dwSize = sizeof(c2);
    hr = ad->lpVtbl->AddUrl(ad, NULL, L"http://example.org/pic.JPG", &c2, ADDURL_SILENT);
    CHECK(hr == S_OK && c2.dwID == c.dwID, "AddUrl existing, silent: %#lx", hr);
    hr = ad->lpVtbl->AddUrl(ad, NULL, L"http://example.org/pic.JPG", &c2, 0);
    CHECK(hr == E_FAIL, "AddUrl existing: %#lx", hr);
    hr = ad->lpVtbl->AddUrl(ad, NULL, NULL, &c2, 0);
    CHECK(hr == E_INVALIDARG, "AddUrl NULL: %#lx", hr);
    hr = ad->lpVtbl->AddUrl(ad, NULL, L"", &c2, 0);
    CHECK(hr == E_INVALIDARG, "AddUrl empty: %#lx", hr);
    ad->lpVtbl->GetDesktopItemCount(ad, &n, 0);
    CHECK(n == 5, "count 5: %d", n);

    /* HTML generation */
    GetTempPathW(MAX_PATH, path); lstrcatW(path, L"sgadesk.htm");
    mk(&c, 1, L"C:\\pics\\a&b.jpg", L"Pic <1>");
    hr = ad->lpVtbl->GenerateDesktopItemHtml(ad, path, &c, 0);
    html = readfile(path);
    CHECK(hr == S_OK && strstr(html, "<img") && strstr(html, "a&amp;b.jpg") && strstr(html, "Pic &lt;1&gt;") && !strstr(html, "<iframe"),
          "picture html %#lx: %s", hr, html);
    mk(&c, 2, L"http://example.org/", L"Site");
    c.fNoScroll = TRUE;
    hr = ad->lpVtbl->GenerateDesktopItemHtml(ad, path, &c, 0);
    html = readfile(path);
    CHECK(hr == S_OK && strstr(html, "<iframe") && strstr(html, "http://example.org/") && strstr(html, "scrolling=\"no\"") && !strstr(html, "<img"),
          "site html %#lx: %s", hr, html);
    c.fNoScroll = FALSE;
    ad->lpVtbl->GenerateDesktopItemHtml(ad, path, &c, 0);
    CHECK(strstr(readfile(path), "scrolling=\"auto\""), "scroll auto");
    mk(&c, 3, L"clsid:1234", L"Ctl");
    hr = ad->lpVtbl->GenerateDesktopItemHtml(ad, path, &c, 0);
    CHECK(hr == S_OK && strstr(readfile(path), "<object"), "control html");
    DeleteFileW(path);
    hr = ad->lpVtbl->GenerateDesktopItemHtml(ad, L"Z:\\no\\such\\dir\\x.htm", &c, 0);
    CHECK(hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND) || hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || hr == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED),
          "bad path %#lx", hr);
    hr = ad->lpVtbl->GenerateDesktopItemHtml(ad, NULL, &c, 0);
    CHECK(hr == E_INVALIDARG, "NULL name %#lx", hr);

    /* options */
    opt.dwSize = sizeof(opt); opt.fEnableComponents = TRUE; opt.fActiveDesktop = TRUE;
    hr = ad->lpVtbl->SetDesktopItemOptions(ad, &opt, 0);
    CHECK(hr == S_OK, "SetDesktopItemOptions %#lx", hr);
    memset(&opt, 0, sizeof(opt)); opt.dwSize = sizeof(opt);
    hr = ad->lpVtbl->GetDesktopItemOptions(ad, &opt, 0);
    CHECK(hr == S_OK && opt.fEnableComponents == TRUE && opt.fActiveDesktop == TRUE, "options %#lx", hr);
    opt.dwSize = 4;
    hr = ad->lpVtbl->SetDesktopItemOptions(ad, &opt, 0);
    CHECK(hr == E_INVALIDARG, "options small %#lx", hr);
    hr = ad->lpVtbl->SetDesktopItemOptions(ad, NULL, 0);
    CHECK(hr == E_INVALIDARG, "options NULL %#lx", hr);

    /* ApplyChanges saves: drop to two items and look from another process */
    mk(&c, 0, L"x", L"x");
    lstrcpyW(c.wszSource, L"http://example.org/pic.JPG"); ad->lpVtbl->RemoveDesktopItem(ad, &c, 0);
    lstrcpyW(c.wszSource, L"http://example.org/index.html"); ad->lpVtbl->RemoveDesktopItem(ad, &c, 0);
    lstrcpyW(c.wszSource, L"C:\\docs\\start.mht"); ad->lpVtbl->RemoveDesktopItem(ad, &c, 0);
    ad->lpVtbl->GetDesktopItemCount(ad, &n, 0);
    CHECK(n == 2, "count before apply: %d", n);
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\" child", exe);
    ok = CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    CHECK(ok, "CreateProcess %lu %ls", GetLastError(), cmd);
    code = 999;
    WaitForSingleObject(pi.hProcess, 30000); GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    CHECK(code == 0, "before apply the other process sees none (exit %lu)", code);
    hr = ad->lpVtbl->ApplyChanges(ad, AD_APPLY_ALL);
    CHECK(hr == S_OK, "apply %#lx", hr);
    ad->lpVtbl->GetDesktopItem(ad, 0, &c3, 0);
    CHECK(!c3.fDirty, "saved items are clean");
    ok = CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    CHECK(ok, "CreateProcess %lu %ls", GetLastError(), cmd);
    code = 999;
    WaitForSingleObject(pi.hProcess, 30000); GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    CHECK(code == 2, "the other process sees 2 items and the options (exit %lu)", code);

    ad->lpVtbl->Release(ad);
    printf("failures: %d\nRESULT: %s\n", failed, failed ? "FAIL" : "PASS");
    return failed ? 1 : 0;
}
