/*
 * wv2test -- does a WebView2 app work on wine-sg?
 *
 * Our own code (Stained Glass OS); the gate is test/webview2-gate.sh. Built
 * against the public WebView2 SDK header WebView2.h (Microsoft.Web.WebView2 NuGet package, BSD-3); the app
 * loads WebView2Loader.dll from its own directory, as real apps ship it.
 * The runtime is whatever the user installed (Evergreen), found by the
 * loader through the registry.
 *
 * Copyright (C) 2026 Stained Glass OS contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * It creates a window, an environment, a controller, navigates to a local
 * page whose script sets document.title, and reports:
 *
 *   Loader=<1|0>            WebView2Loader.dll + export found
 *   Version=<hr>,<ver>      GetAvailableCoreWebView2BrowserVersionString
 *   Environment=<hr>        CreateCoreWebView2EnvironmentWithOptions callback
 *   Controller=<hr>         CreateCoreWebView2Controller callback
 *   ProcessFailed=<kind>    (if a browser/renderer process dies)
 *   NavigationCompleted=<success>,<webErrorStatus>
 *   Title=<document title>
 *   Script=<ExecuteScript result JSON>
 *   RESULT=PASS|FAIL|TIMEOUT
 *
 * Lines go to stdout and to wv2test.txt next to the exe.
 *
 *   wv2test.exe [url]      (default: file:// page written next to the exe)
 * Env: WV2TEST_HOLD=<seconds> to keep the window up after the result (default 5).
 *      WV2TEST_RESIZE=1 grows the host window to 1000x700 3 s after the result.
 *      WV2TEST_CLIPCHILDREN=1 gives the host window WS_CLIPCHILDREN.
 *      WV2TEST_SCRIPT=<js> replaces the diagnostic script (default: userAgent,
 *      devicePixelRatio, typeof gc -- shows whether extra browser arguments arrived).
 *
 * Build:
 *   x86_64-w64-mingw32-gcc -O2 -w -Ishim -I<sdk>/build/native/include \
 *       -o wv2test.exe wv2test.c -lole32 -luuid -luser32 -lgdi32
 * (shim/EventToken.h is "#include <eventtoken.h>": mingw's name is lower-case.)
 */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include "WebView2.h"

typedef HRESULT (STDAPICALLTYPE *create_env_fn)(PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions *,
                                                ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *);
typedef HRESULT (STDAPICALLTYPE *get_version_fn)(PCWSTR, LPWSTR *);

static HWND main_hwnd;
static ICoreWebView2Controller *controller;
static ICoreWebView2 *webview;
static FILE *out_file;
static int finished;
static const WCHAR want_title[] = L"SGWV2-42";

static void report(const char *fmt, ...)
{
    char buf[65536];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    printf("%s\n", buf);
    fflush(stdout);
    if (out_file) { fprintf(out_file, "%s\n", buf); fflush(out_file); }
}

static char *utf8(const WCHAR *w)
{
    static char buf[65536];
    if (!w) return "(null)";
    WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, sizeof(buf), NULL, NULL);
    return buf;
}

static void finish(const char *result)
{
    const char *hold;
    int secs = 5;
    if (finished) return;
    finished = 1;
    report("RESULT=%s", result);
    if ((hold = getenv("WV2TEST_HOLD"))) secs = atoi(hold);
    /* WV2TEST_RESIZE=1: grow the host window, so the WebView must resize and repaint. */
    if (getenv("WV2TEST_RESIZE")) SetTimer(main_hwnd, 3, 3000, NULL);
    SetTimer(main_hwnd, 2, secs * 1000, NULL);
}

/* One minimal COM object shape for every callback: vtable pointer + refcount.
 * QueryInterface hands out itself for IUnknown and the handler's own IID. */
typedef struct handler { void *vtbl; LONG ref; const IID *iid; } handler;

static HRESULT STDMETHODCALLTYPE h_qi(void *This, REFIID riid, void **out)
{
    handler *h = This;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, h->iid))
    {
        *out = This;
        InterlockedIncrement(&h->ref);
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE h_addref(void *This) { return InterlockedIncrement(&((handler *)This)->ref); }
static ULONG STDMETHODCALLTYPE h_release(void *This) { return InterlockedDecrement(&((handler *)This)->ref); } /* static objects */

/* ExecuteScript completed */
static HRESULT STDMETHODCALLTYPE script_invoke(ICoreWebView2ExecuteScriptCompletedHandler *This, HRESULT hr, LPCWSTR json)
{
    report("ScriptHr=0x%08lx", (unsigned long)hr);
    report("Script=%s", utf8(json));
    finish(json && !wcscmp(json, L"\"SGWV2-42|42\"") ? "PASS" : "FAIL");
    return S_OK;
}
static ICoreWebView2ExecuteScriptCompletedHandlerVtbl script_vtbl = { (void *)h_qi, (void *)h_addref, (void *)h_release, script_invoke };
static handler script_handler = { &script_vtbl, 1, &IID_ICoreWebView2ExecuteScriptCompletedHandler };

/* ExecuteScript completed: navigator.userAgent (shows whether extra browser arguments arrived) */
static HRESULT STDMETHODCALLTYPE ua_invoke(ICoreWebView2ExecuteScriptCompletedHandler *This, HRESULT hr, LPCWSTR json)
{
    report("UserAgent=%s", utf8(json));
    return S_OK;
}
static ICoreWebView2ExecuteScriptCompletedHandlerVtbl ua_vtbl = { (void *)h_qi, (void *)h_addref, (void *)h_release, ua_invoke };
static handler ua_handler = { &ua_vtbl, 1, &IID_ICoreWebView2ExecuteScriptCompletedHandler };

static WCHAR extra_script[4096];

/* NavigationCompleted */
static HRESULT STDMETHODCALLTYPE nav_invoke(ICoreWebView2NavigationCompletedEventHandler *This, ICoreWebView2 *sender,
                                            ICoreWebView2NavigationCompletedEventArgs *args)
{
    BOOL ok = FALSE;
    COREWEBVIEW2_WEB_ERROR_STATUS status = 0;
    LPWSTR title = NULL;
    ICoreWebView2NavigationCompletedEventArgs_get_IsSuccess(args, &ok);
    ICoreWebView2NavigationCompletedEventArgs_get_WebErrorStatus(args, &status);
    report("NavigationCompleted=%d,%d", ok, status);
    ICoreWebView2_get_DocumentTitle(sender, &title);
    report("Title=%s", utf8(title));
    if (title) CoTaskMemFree(title);
    if (GetEnvironmentVariableW(L"WV2TEST_SCRIPT", extra_script, 4096))
        ICoreWebView2_ExecuteScript(sender, extra_script, (ICoreWebView2ExecuteScriptCompletedHandler *)&ua_handler);
    else ICoreWebView2_ExecuteScript(sender, L"navigator.userAgent + '|dpr=' + devicePixelRatio + '|gc=' + typeof gc", (ICoreWebView2ExecuteScriptCompletedHandler *)&ua_handler);
    ICoreWebView2_ExecuteScript(sender, L"document.title + '|' + (6*7)",
                                (ICoreWebView2ExecuteScriptCompletedHandler *)&script_handler);
    return S_OK;
}
static ICoreWebView2NavigationCompletedEventHandlerVtbl nav_vtbl = { (void *)h_qi, (void *)h_addref, (void *)h_release, nav_invoke };
static handler nav_handler = { &nav_vtbl, 1, &IID_ICoreWebView2NavigationCompletedEventHandler };

/* ProcessFailed */
static HRESULT STDMETHODCALLTYPE pf_invoke(ICoreWebView2ProcessFailedEventHandler *This, ICoreWebView2 *sender,
                                           ICoreWebView2ProcessFailedEventArgs *args)
{
    COREWEBVIEW2_PROCESS_FAILED_KIND kind = -1;
    ICoreWebView2ProcessFailedEventArgs_get_ProcessFailedKind(args, &kind);
    report("ProcessFailed=%d", kind);
    return S_OK;
}
static ICoreWebView2ProcessFailedEventHandlerVtbl pf_vtbl = { (void *)h_qi, (void *)h_addref, (void *)h_release, pf_invoke };
static handler pf_handler = { &pf_vtbl, 1, &IID_ICoreWebView2ProcessFailedEventHandler };

static WCHAR target_url[2048];

/* Controller created */
static HRESULT STDMETHODCALLTYPE ctl_invoke(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This, HRESULT hr,
                                            ICoreWebView2Controller *ctl)
{
    EventRegistrationToken tok;
    RECT rc;
    report("Controller=0x%08lx", (unsigned long)hr);
    if (FAILED(hr) || !ctl) { finish("FAIL"); return S_OK; }
    controller = ctl;
    ICoreWebView2Controller_AddRef(ctl);
    ICoreWebView2Controller_get_CoreWebView2(ctl, &webview);
    GetClientRect(main_hwnd, &rc);
    ICoreWebView2Controller_put_Bounds(ctl, rc);
    ICoreWebView2Controller_put_IsVisible(ctl, TRUE);
    ICoreWebView2_add_ProcessFailed(webview, (ICoreWebView2ProcessFailedEventHandler *)&pf_handler, &tok);
    ICoreWebView2_add_NavigationCompleted(webview, (ICoreWebView2NavigationCompletedEventHandler *)&nav_handler, &tok);
    hr = ICoreWebView2_Navigate(webview, target_url);
    report("Navigate=0x%08lx", (unsigned long)hr);
    return S_OK;
}
static ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl ctl_vtbl = { (void *)h_qi, (void *)h_addref, (void *)h_release, ctl_invoke };
static handler ctl_handler = { &ctl_vtbl, 1, &IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler };

/* Environment created */
static HRESULT STDMETHODCALLTYPE env_invoke(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This, HRESULT hr,
                                            ICoreWebView2Environment *env)
{
    LPWSTR ver = NULL;
    report("Environment=0x%08lx", (unsigned long)hr);
    if (FAILED(hr) || !env) { finish("FAIL"); return S_OK; }
    ICoreWebView2Environment_get_BrowserVersionString(env, &ver);
    report("EnvironmentVersion=%s", utf8(ver));
    if (ver) CoTaskMemFree(ver);
    hr = ICoreWebView2Environment_CreateCoreWebView2Controller(env, main_hwnd,
            (ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *)&ctl_handler);
    report("CreateController=0x%08lx", (unsigned long)hr);
    if (FAILED(hr)) finish("FAIL");
    return S_OK;
}
static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl env_vtbl = { (void *)h_qi, (void *)h_addref, (void *)h_release, env_invoke };
static handler env_handler = { &env_vtbl, 1, &IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler };

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_SIZE:
        if (controller)
        {
            RECT rc;
            GetClientRect(hwnd, &rc);
            ICoreWebView2Controller_put_Bounds(controller, rc);
        }
        return 0;
    case WM_TIMER:
        if (wp == 1) { report("Timeout"); finish("TIMEOUT"); }
        else if (wp == 3)
        {
            KillTimer(hwnd, 3);
            SetWindowPos(hwnd, NULL, 0, 0, 1000, 700, SWP_NOMOVE | SWP_NOZORDER);
            report("Resized=1000x700");
        }
        else DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (controller) ICoreWebView2Controller_Close(controller);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int main(int argc, char **argv)
{
    WCHAR dir[MAX_PATH], path[MAX_PATH], udf[MAX_PATH], *p;
    HMODULE loader;
    create_env_fn create_env;
    get_version_fn get_version;
    LPWSTR ver = NULL;
    WNDCLASSW wc = {0};
    MSG msg;
    HRESULT hr;
    FILE *page;

    GetModuleFileNameW(NULL, dir, MAX_PATH);
    if ((p = wcsrchr(dir, '\\'))) *p = 0;
    swprintf(path, MAX_PATH, L"%ls\\wv2test.txt", dir);
    out_file = _wfopen(path, L"w");

    if (argc > 1) MultiByteToWideChar(CP_ACP, 0, argv[1], -1, target_url, 2048);
    else
    {
        swprintf(path, MAX_PATH, L"%ls\\wv2test.html", dir);
        if ((page = _wfopen(path, L"w")))
        {
            fputs("<html><body style=\"margin:0;background:#129A3C\"><h1 style=\"color:white\">Stained Glass WebView2</h1>"
                  "<script>document.title='SGWV2-'+(40+2)</script></body></html>", page);
            fclose(page);
        }
        swprintf(target_url, 2048, L"file:///%ls", path);
        for (p = target_url; *p; p++) if (*p == '\\') *p = '/';
    }
    report("Url=%s", utf8(target_url));

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    swprintf(path, MAX_PATH, L"%ls\\WebView2Loader.dll", dir);
    loader = LoadLibraryW(path);
    create_env = loader ? (create_env_fn)GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions") : NULL;
    get_version = loader ? (get_version_fn)GetProcAddress(loader, "GetAvailableCoreWebView2BrowserVersionString") : NULL;
    report("Loader=%d", create_env != NULL);
    if (!create_env) { report("LoaderError=%lu", GetLastError()); report("RESULT=FAIL"); return 1; }
    if (get_version)
    {
        hr = get_version(NULL, &ver);
        report("Version=0x%08lx,%s", (unsigned long)hr, utf8(ver));
    }

    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"wv2test";
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    main_hwnd = CreateWindowW(L"wv2test", L"wv2test",
                              WS_OVERLAPPEDWINDOW | WS_VISIBLE | (getenv("WV2TEST_CLIPCHILDREN") ? WS_CLIPCHILDREN : 0),
                              40, 40, 800, 600, NULL, NULL, wc.hInstance, NULL);
    SetTimer(main_hwnd, 1, 120000, NULL);

    GetEnvironmentVariableW(L"LOCALAPPDATA", udf, MAX_PATH);
    wcscat(udf, L"\\wv2test");
    hr = create_env(NULL, udf, NULL, (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *)&env_handler);
    report("CreateEnvironment=0x%08lx", (unsigned long)hr);
    if (FAILED(hr)) finish("FAIL");

    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (out_file) fclose(out_file);
    return 0;
}
