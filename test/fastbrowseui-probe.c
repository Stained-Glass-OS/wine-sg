/* fastbrowseui-probe: browseui CompCatCacheDaemon (IRunnableTask state machine), the shell autocomplete source
 * (IEnumString + IACList::Expand) and IProgressDialog::Timer. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int checks, fails;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const GUID CLSID_Daemon = {0x8c7461ef, 0x2b13, 0x11d2, {0xbe, 0x35, 0x30, 0x78, 0x30, 0x2c, 0x20, 0x30}};
static const GUID CLSID_ACL = {0x03c036f1, 0xa186, 0x11d0, {0x82, 0x4a, 0x00, 0xaa, 0x00, 0x5b, 0x43, 0x83}};
static const GUID CLSID_ProgDlg = {0xf8383852, 0xfcd3, 0x11d1, {0xa6, 0xb9, 0x00, 0x60, 0x97, 0xdf, 0x5b, 0xd4}};
static const GUID IID_IRunnableTask_ = {0x85788d00, 0x6807, 0x11d0, {0xb8, 0x10, 0x00, 0xc0, 0x4f, 0xd7, 0x06, 0xec}};
static const GUID IID_IACList_ = {0x77a130b0, 0x94fd, 0x11d0, {0xa5, 0x44, 0x00, 0xc0, 0x4f, 0xd7, 0xd0, 0x62}};
static const GUID IID_IACList2_ = {0x470141a0, 0x5186, 0x11d2, {0xbb, 0xb6, 0x00, 0x60, 0x97, 0x7b, 0x46, 0x4c}};
static const GUID CAT_X = {0x5a3c0000, 0x1111, 0x4222, {0x83, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66}};

/* IRunnableTask, declared by hand */
typedef struct IRT IRT;
typedef struct IRTVtbl {
    HRESULT (WINAPI *QueryInterface)(IRT *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IRT *);
    ULONG (WINAPI *Release)(IRT *);
    HRESULT (WINAPI *Run)(IRT *);
    HRESULT (WINAPI *Kill)(IRT *, BOOL);
    HRESULT (WINAPI *Suspend)(IRT *);
    HRESULT (WINAPI *Resume)(IRT *);
    ULONG (WINAPI *IsRunning)(IRT *);
} IRTVtbl;
struct IRT { const IRTVtbl *v; };

typedef struct IACL2 IACL2;
typedef struct IACL2Vtbl {
    HRESULT (WINAPI *QueryInterface)(IACL2 *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IACL2 *);
    ULONG (WINAPI *Release)(IACL2 *);
    HRESULT (WINAPI *Expand)(IACL2 *, LPCWSTR);
    HRESULT (WINAPI *SetOptions)(IACL2 *, DWORD);
    HRESULT (WINAPI *GetOptions)(IACL2 *, DWORD *);
} IACL2Vtbl;
struct IACL2 { const IACL2Vtbl *v; };

#define ST_NOT 0
#define ST_RUN 1
#define ST_SUSP 2
#define ST_FIN 4

static DWORD WINAPI run_thread(void *p)
{
    IRT *t = p;
    return (DWORD)t->v->Run(t);
}

static void test_daemon(void)
{
    IRT *t = NULL, *u;
    HRESULT hr;
    HANDLE th;
    DWORD code, i;
    HKEY key;
    WCHAR path[200];

    hr = CoCreateInstance(&CLSID_Daemon, NULL, CLSCTX_INPROC_SERVER, &IID_IRunnableTask_, (void **)&t);
    CHECK(hr == S_OK, "create daemon %08lx", hr);
    if (!t) return;

    CHECK(t->v->IsRunning(t) == ST_NOT, "idle state %lu", t->v->IsRunning(t));
    CHECK(t->v->Suspend(t) == E_FAIL, "suspend when idle");
    CHECK(t->v->Resume(t) == E_FAIL, "resume when idle");
    CHECK(t->v->Kill(t, TRUE) == S_OK, "kill when idle");
    CHECK(t->v->IsRunning(t) == ST_NOT, "still idle");

    hr = t->v->Run(t);
    CHECK(hr == S_OK, "run %08lx", hr);
    CHECK(t->v->IsRunning(t) == ST_FIN, "finished state %lu", t->v->IsRunning(t));
    CHECK(t->v->Suspend(t) == E_FAIL, "suspend when finished");
    hr = t->v->Run(t);
    CHECK(hr == S_OK, "run again after finishing %08lx", hr);
    CHECK(t->v->IsRunning(t) == ST_FIN, "finished again");

    /* many classes in one category so that a run takes a while */
    if (!RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Classes\\Component Categories\\{5A3C0000-1111-4222-8333-444455556666}",
                         0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL))
        RegCloseKey(key);
    for (i = 0; i < 2500; i++)
    {
        wsprintfW(path, L"Software\\Classes\\CLSID\\{5A3C%04lX-0000-4000-8000-000000000001}\\Implemented Categories\\{5A3C0000-1111-4222-8333-444455556666}", i + 1);
        if (!RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL))
            RegCloseKey(key);
    }

    {
        DWORD t0 = GetTickCount();
        hr = t->v->Run(t);
        printf("note: a run with 2500 classes takes %lu ms (%08lx)\n", GetTickCount() - t0, hr);
    }
    /* suspend */
    th = CreateThread(NULL, 0, run_thread, t, 0, NULL);
    for (i = 0; i < 2000 && t->v->IsRunning(t) != ST_RUN; i++) Sleep(1);
    CHECK(t->v->IsRunning(t) == ST_RUN, "running state %lu", t->v->IsRunning(t));
    hr = t->v->Run(t);
    CHECK(hr == E_PENDING, "run while running %08lx", hr);
    hr = t->v->Suspend(t);
    CHECK(hr == S_OK, "suspend %08lx", hr);
    CHECK(t->v->IsRunning(t) == ST_SUSP, "suspended state %lu", t->v->IsRunning(t));
    CHECK(t->v->Suspend(t) == E_FAIL, "suspend twice");
    Sleep(1500);
    CHECK(WaitForSingleObject(th, 0) == WAIT_TIMEOUT, "the suspended task made no progress to the end");
    CHECK(t->v->IsRunning(t) == ST_SUSP, "still suspended");
    hr = t->v->Run(t);
    CHECK(hr == E_PENDING, "run while suspended %08lx", hr);
    CHECK(t->v->Resume(t) == S_OK, "resume");
    CHECK(WaitForSingleObject(th, 60000) == WAIT_OBJECT_0, "resumed task ends");
    GetExitCodeThread(th, &code);
    CloseHandle(th);
    CHECK(code == S_OK, "run result after resume %08lx", code);
    CHECK(t->v->IsRunning(t) == ST_FIN, "finished after resume %lu", t->v->IsRunning(t));

    /* kill */
    th = CreateThread(NULL, 0, run_thread, t, 0, NULL);
    for (i = 0; i < 2000 && t->v->IsRunning(t) != ST_RUN; i++) Sleep(1);
    CHECK(t->v->Suspend(t) == S_OK, "suspend before kill");
    hr = t->v->Kill(t, TRUE);
    CHECK(hr == S_OK, "kill %08lx", hr);
    CHECK(WaitForSingleObject(th, 60000) == WAIT_OBJECT_0, "killed task ends");
    GetExitCodeThread(th, &code);
    CloseHandle(th);
    CHECK(code == (DWORD)E_ABORT, "run result after kill %08lx", code);
    CHECK(t->v->IsRunning(t) == ST_NOT, "not running after kill %lu", t->v->IsRunning(t));
    hr = t->v->Run(t);
    CHECK(hr == S_OK && t->v->IsRunning(t) == ST_FIN, "run after kill %08lx", hr);

    hr = t->v->QueryInterface(t, &IID_IUnknown, (void **)&u);
    CHECK(hr == S_OK, "QI");
    if (hr == S_OK) ((IUnknown *)u)->lpVtbl->Release((IUnknown *)u);
    t->v->Release(t);

    RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"Software\\Classes\\Component Categories\\{5A3C0000-1111-4222-8333-444455556666}");
    for (i = 0; i < 2500; i++)
    {
        wsprintfW(path, L"Software\\Classes\\CLSID\\{5A3C%04lX-0000-4000-8000-000000000001}", i + 1);
        RegDeleteTreeW(HKEY_LOCAL_MACHINE, path);
    }
}

/* ---- autocomplete source ---- */
static int collect(IEnumString *e, WCHAR out[][MAX_PATH], int max)
{
    int n = 0;
    for (;;)
    {
        LPOLESTR s = NULL;
        ULONG got = 0;
        HRESULT hr = IEnumString_Next(e, 1, &s, &got);
        if (hr != S_OK) { CHECK(hr == S_FALSE && got == 0, "end of enumeration %08lx", hr); break; }
        if (n < max) wcscpy(out[n], s);
        n++;
        CoTaskMemFree(s);
        if (n > 1000) break;
    }
    return n;
}

static BOOL has(WCHAR list[][MAX_PATH], int n, const WCHAR *s)
{
    int i;
    for (i = 0; i < n; i++) if (!wcsicmp(list[i], s)) return TRUE;
    return FALSE;
}

static void test_acl(void)
{
    IEnumString *e = NULL, *c = NULL;
    IACL2 *acl = NULL;
    WCHAR dir[MAX_PATH], p[MAX_PATH], q[MAX_PATH], list[64][MAX_PATH], saved[MAX_PATH];
    LPOLESTR strs[8];
    ULONG got;
    DWORD opts;
    HRESULT hr;
    int n;

    hr = CoCreateInstance(&CLSID_ACL, NULL, CLSCTX_INPROC_SERVER, &IID_IEnumString, (void **)&e);
    CHECK(hr == S_OK, "create acl %08lx", hr);
    if (!e) return;
    hr = IEnumString_QueryInterface(e, &IID_IACList2_, (void **)&acl);
    CHECK(hr == S_OK, "IACList2 %08lx", hr);
    if (!acl) return;

    GetTempPathW(MAX_PATH, dir);
    wcscat(dir, L"sgacl");
    CreateDirectoryW(dir, NULL);
    {
        const WCHAR *names[] = { L"a1.txt", L"a2.txt", L"b.txt" };
        int i;
        for (i = 0; i < 3; i++)
        {
            HANDLE h;
            wsprintfW(p, L"%s\\%s", dir, names[i]);
            h = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
            CloseHandle(h);
        }
        wsprintfW(p, L"%s\\sd", dir);
        CreateDirectoryW(p, NULL);
    }

    /* nothing expanded: empty */
    hr = IEnumString_Next(e, 1, strs, &got);
    CHECK(hr == S_FALSE && got == 0, "empty source %08lx %lu", hr, got);
    CHECK(acl->v->Expand(acl, NULL) == E_INVALIDARG, "expand NULL");

    wsprintfW(p, L"%s\\", dir);
    hr = acl->v->Expand(acl, p);
    CHECK(hr == S_OK, "expand %08lx", hr);
    n = collect(e, list, 64);
    CHECK(n == 4, "four entries %d", n);
    wsprintfW(q, L"%s\\a1.txt", dir);
    CHECK(has(list, n, q), "file path complete");
    wsprintfW(q, L"%s\\b.txt", dir);
    CHECK(has(list, n, q), "second file");
    wsprintfW(q, L"%s\\sd\\", dir);
    CHECK(has(list, n, q), "directory with a trailing backslash (GUESS)");

    /* Reset, Skip, Next with a count */
    CHECK(IEnumString_Reset(e) == S_OK, "reset");
    hr = IEnumString_Next(e, 8, strs, &got);
    CHECK(hr == S_FALSE && got == 4, "next 8 of 4: %08lx %lu", hr, got);
    while (got) CoTaskMemFree(strs[--got]);
    IEnumString_Reset(e);
    CHECK(IEnumString_Skip(e, 3) == S_OK, "skip 3");
    hr = IEnumString_Next(e, 1, strs, NULL);
    CHECK(hr == S_OK, "next after skip with no count %08lx", hr);
    if (hr == S_OK) CoTaskMemFree(strs[0]);
    hr = IEnumString_Next(e, 1, strs, &got);
    CHECK(hr == S_FALSE && got == 0, "exhausted %08lx", hr);
    CHECK(IEnumString_Skip(e, 1) == S_FALSE, "skip past the end");
    IEnumString_Reset(e);
    CHECK(IEnumString_Skip(e, 5) == S_FALSE, "skip too many");

    /* Clone keeps the position */
    IEnumString_Reset(e);
    IEnumString_Skip(e, 2);
    hr = IEnumString_Clone(e, &c);
    CHECK(hr == S_OK && c, "clone %08lx", hr);
    if (c)
    {
        WCHAR l2[64][MAX_PATH];
        int n2 = collect(c, l2, 64);
        CHECK(n2 == 2, "clone starts at the same place: %d", n2);
        n2 = collect(e, l2, 64);
        CHECK(n2 == 2, "the original is independent: %d", n2);
        IEnumString_Reset(c);
        n2 = collect(c, l2, 64);
        CHECK(n2 == 4, "clone resets alone: %d", n2);
        IEnumString_Release(c);
    }
    hr = IEnumString_Clone(e, NULL);
    CHECK(hr == E_POINTER, "clone NULL %08lx", hr);

    /* prefix */
    wsprintfW(p, L"%s\\a", dir);
    hr = acl->v->Expand(acl, p);
    n = collect(e, list, 64);
    CHECK(n == 2, "prefix a: %d", n);
    wsprintfW(q, L"%s\\a2.txt", dir);
    CHECK(has(list, n, q), "a2");
    wsprintfW(p, L"%s\\A", dir);
    acl->v->Expand(acl, p);
    n = collect(e, list, 64);
    CHECK(n == 2, "prefix is case insensitive: %d", n);
    wsprintfW(p, L"%s\\zzz", dir);
    acl->v->Expand(acl, p);
    n = collect(e, list, 64);
    CHECK(n == 0, "no match: %d", n);

    /* options */
    acl->v->SetOptions(acl, 0x20);
    CHECK(acl->v->GetOptions(acl, &opts) == S_OK && opts == 0x20, "options %lx", opts);
    wsprintfW(p, L"%s\\", dir);
    acl->v->Expand(acl, p);
    n = collect(e, list, 64);
    CHECK(n == 1, "directories only: %d", n);
    wsprintfW(q, L"%s\\sd\\", dir);
    CHECK(has(list, n, q), "the directory");

    /* expanding again replaces the list */
    acl->v->SetOptions(acl, 0);
    wsprintfW(p, L"%s\\sd\\", dir);
    acl->v->Expand(acl, p);
    n = collect(e, list, 64);
    CHECK(n == 0, "empty directory: %d", n);

    /* current directory and my computer */
    GetCurrentDirectoryW(MAX_PATH, saved);
    SetCurrentDirectoryW(dir);
    acl->v->SetOptions(acl, 1);
    acl->v->Expand(acl, L"");
    n = collect(e, list, 64);
    CHECK(n == 4 && has(list, n, L"a1.txt") && has(list, n, L"sd\\"), "current directory: %d", n);
    acl->v->Expand(acl, L"b");
    n = collect(e, list, 64);
    CHECK(n == 1 && has(list, n, L"b.txt"), "current directory with a prefix: %d", n);
    SetCurrentDirectoryW(saved);
    acl->v->SetOptions(acl, 2);
    acl->v->Expand(acl, L"");
    n = collect(e, list, 64);
    CHECK(n >= 1 && has(list, n, L"C:\\") , "drives: %d", n);
    acl->v->SetOptions(acl, 0);
    acl->v->Expand(acl, L"");
    n = collect(e, list, 64);
    CHECK(n == 0, "no option, nothing: %d", n);


    {
        static const int csidls[2] = { CSIDL_DESKTOPDIRECTORY, CSIDL_FAVORITES };
        static const DWORD flags[2] = { 4, 8 };
        int k;
        for (k = 0; k < 2; k++)
        {
            HANDLE h;
            if (FAILED(SHGetFolderPathW(NULL, csidls[k] | CSIDL_FLAG_CREATE, NULL, 0, p))) { CHECK(0, "known folder %d", k); continue; }
            wcscat(p, L"\\sgacl_known.txt");
            h = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
            CloseHandle(h);
            acl->v->SetOptions(acl, flags[k]);
            acl->v->Expand(acl, L"sgacl_kn");
            n = collect(e, list, 64);
            CHECK(n == 1 && has(list, n, p), "known folder %d lists %ls: n=%d", k, p, n);
            DeleteFileW(p);
        }
    }

    acl->v->Release(acl);
    IEnumString_Release(e);

    wsprintfW(p, L"%s\\sd", dir); RemoveDirectoryW(p);
    wsprintfW(p, L"%s\\a1.txt", dir); DeleteFileW(p);
    wsprintfW(p, L"%s\\a2.txt", dir); DeleteFileW(p);
    wsprintfW(p, L"%s\\b.txt", dir); DeleteFileW(p);
    RemoveDirectoryW(dir);
}

/* ---- progress dialog timer ---- */
static int line3_number(HWND hwnd)
{
    WCHAR text[200];
    const WCHAR *s;
    if (!GetDlgItemTextW(hwnd, 105, text, 200)) return -1;
    for (s = text; *s && (*s < '0' || *s > '9'); s++) ;
    return *s ? _wtoi(s) : -1;
}

typedef struct PD PD;
typedef struct PDVtbl {
    HRESULT (WINAPI *QueryInterface)(PD *, REFIID, void **);
    ULONG (WINAPI *AddRef)(PD *);
    ULONG (WINAPI *Release)(PD *);
    HRESULT (WINAPI *StartProgressDialog)(PD *, HWND, IUnknown *, DWORD, LPCVOID);
    HRESULT (WINAPI *StopProgressDialog)(PD *);
    HRESULT (WINAPI *SetTitle)(PD *, LPCWSTR);
    HRESULT (WINAPI *SetAnimation)(PD *, HINSTANCE, UINT);
    BOOL (WINAPI *HasUserCancelled)(PD *);
    HRESULT (WINAPI *SetProgress)(PD *, DWORD, DWORD);
    HRESULT (WINAPI *SetProgress64)(PD *, ULONGLONG, ULONGLONG);
    HRESULT (WINAPI *SetLine)(PD *, DWORD, LPCWSTR, BOOL, LPCVOID);
    HRESULT (WINAPI *SetCancelMsg)(PD *, LPCWSTR, LPCVOID);
    HRESULT (WINAPI *Timer)(PD *, DWORD, LPCVOID);
} PDVtbl;
struct PD { const PDVtbl *lpVtbl; };
static const GUID IID_PD = {0xebbc7c04, 0x315e, 0x11d2, {0xb6, 0x2f, 0x00, 0x60, 0x97, 0xdf, 0x5b, 0xd4}};

static void test_timer(void)
{
    PD *pd = NULL;
    IOleWindow *ow = NULL;
    HWND hwnd = NULL;
    HRESULT hr;
    int v;

    hr = CoCreateInstance(&CLSID_ProgDlg, NULL, CLSCTX_INPROC_SERVER, &IID_PD, (void **)&pd);
    CHECK(hr == S_OK, "create progress dialog %08lx", hr);
    if (!pd) return;
    hr = pd->lpVtbl->StartProgressDialog(pd, NULL, NULL, PROGDLG_AUTOTIME | PROGDLG_NOMINIMIZE, NULL);
    CHECK(hr == S_OK, "start %08lx", hr);
    ((IUnknown *)pd)->lpVtbl->QueryInterface((IUnknown *)pd, &IID_IOleWindow, (void **)&ow);
    if (ow) { IOleWindow_GetWindow(ow, &hwnd); IOleWindow_Release(ow); }
    CHECK(hwnd != NULL, "dialog window");

    Sleep(3000);
    pd->lpVtbl->SetProgress64(pd, 1, 10);
    v = line3_number(hwnd);
    CHECK(v >= 20 && v <= 40, "estimate from the start of the dialog: %d", v);

    CHECK(pd->lpVtbl->Timer(pd, 1, NULL) == S_OK, "reset");
    pd->lpVtbl->SetProgress64(pd, 1, 10);
    v = line3_number(hwnd);
    CHECK(v >= 0 && v <= 1, "estimate after a reset: %d", v);

    /* pause: time spent paused does not count */
    pd->lpVtbl->Timer(pd, 1, NULL);
    pd->lpVtbl->Timer(pd, 2, NULL);
    Sleep(3000);
    pd->lpVtbl->SetProgress64(pd, 1, 10);
    v = line3_number(hwnd);
    CHECK(v >= 0 && v <= 1, "estimate while paused: %d", v);
    CHECK(pd->lpVtbl->Timer(pd, 3, NULL) == S_OK, "resume");
    pd->lpVtbl->SetProgress64(pd, 1, 10);
    v = line3_number(hwnd);
    CHECK(v >= 0 && v <= 1, "estimate after resume: %d", v);
    Sleep(2100);
    pd->lpVtbl->SetProgress64(pd, 1, 10);
    v = line3_number(hwnd);
    CHECK(v >= 15 && v <= 25, "time runs again after resume: %d", v);
    CHECK(pd->lpVtbl->Timer(pd, 3, NULL) == S_OK, "resume when running");

    pd->lpVtbl->StopProgressDialog(pd);
    ((IUnknown *)pd)->lpVtbl->Release((IUnknown *)pd);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    test_daemon();
    test_acl();
    test_timer();
    printf("checks %d fails %d\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    CoUninitialize();
    return fails != 0;
}
