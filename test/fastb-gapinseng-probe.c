/* inseng engine probe (patch 3650) */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

static int failures, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { failures++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const GUID CLSID_IE = {0x6e449686,0xc509,0x11cf,{0xaa,0xfa,0x00,0xaa,0x00,0xb6,0x01,0x5c}};
static const GUID IID_IE2 = {0x6e449689,0xc509,0x11cf,{0xaa,0xfa,0x00,0xaa,0x00,0xb6,0x01,0x5c}};
static const GUID IID_IECB = {0x6e449685,0xc509,0x11cf,{0xaa,0xfa,0x00,0xaa,0x00,0xb6,0x01,0x5c}};

typedef struct { DWORD cbSize, dwInstallSize, dwWinDriveSize, dwDownloadSize, dwDependancySize,
    dwInstallDriveReq, dwWinDriveReq, dwDownloadDriveReq; char chWinDrive, chInstallDrive, chDownloadDrive;
    DWORD dwTotalDownloadSize; } SIZES;

typedef struct IE2 IE2;
typedef struct IECB IECB;
struct IE2Vtbl {
    HRESULT (WINAPI *QueryInterface)(IE2*, REFIID, void**);
    ULONG (WINAPI *AddRef)(IE2*);
    ULONG (WINAPI *Release)(IE2*);
    HRESULT (WINAPI *GetEngineStatus)(IE2*, DWORD*);
    HRESULT (WINAPI *SetCifFile)(IE2*, const char*, const char*);
    HRESULT (WINAPI *DownloadComponents)(IE2*, DWORD);
    HRESULT (WINAPI *InstallComponents)(IE2*, DWORD);
    HRESULT (WINAPI *EnumInstallIDs)(IE2*, UINT, char**);
    HRESULT (WINAPI *EnumDownloadIDs)(IE2*, UINT, char**);
    HRESULT (WINAPI *IsComponentInstalled)(IE2*, const char*, DWORD*);
    HRESULT (WINAPI *RegisterCB)(IE2*, IECB*);
    HRESULT (WINAPI *UnregisterCB)(IE2*);
    HRESULT (WINAPI *SetAction)(IE2*, const char*, DWORD, DWORD);
    HRESULT (WINAPI *GetSizes)(IE2*, const char*, SIZES*);
    HRESULT (WINAPI *LaunchExtraCommand)(IE2*, const char*, const char*);
    HRESULT (WINAPI *GetDisplayName)(IE2*, const char*, const char*);
    HRESULT (WINAPI *SetBaseUrl)(IE2*, const char*);
    HRESULT (WINAPI *SetDownloadDir)(IE2*, const char*);
    HRESULT (WINAPI *SetInstallDrive)(IE2*, char);
    HRESULT (WINAPI *SetInstallOptions)(IE2*, DWORD);
    HRESULT (WINAPI *SetHWND)(IE2*, HWND);
    HRESULT (WINAPI *SetIStream)(IE2*, IStream*);
    HRESULT (WINAPI *Abort)(IE2*, DWORD);
    HRESULT (WINAPI *Suspend)(IE2*);
    HRESULT (WINAPI *Resume)(IE2*);
    HRESULT (WINAPI *SetLocalCif)(IE2*, const char*);
    HRESULT (WINAPI *GetICifFile)(IE2*, void**);
};
struct IE2 { const struct IE2Vtbl *v; };

static struct { int status, start_install, start_comp, stop_comp, stop_install; HRESULT last_err; } cb;
struct IECBVtbl {
    HRESULT (WINAPI *QueryInterface)(IECB*, REFIID, void**);
    ULONG (WINAPI *AddRef)(IECB*);
    ULONG (WINAPI *Release)(IECB*);
    HRESULT (WINAPI *OnEngineStatusChange)(IECB*, DWORD, DWORD);
    HRESULT (WINAPI *OnStartInstall)(IECB*, DWORD, DWORD);
    HRESULT (WINAPI *OnStartComponent)(IECB*, const char*, DWORD, DWORD, const char*);
    HRESULT (WINAPI *OnComponentProgress)(IECB*, const char*, DWORD, const char*, const char*, ULONG, ULONG);
    HRESULT (WINAPI *OnStopComponent)(IECB*, const char*, HRESULT, DWORD, const char*, DWORD);
    HRESULT (WINAPI *OnStopInstall)(IECB*, HRESULT, const char*, DWORD);
    HRESULT (WINAPI *OnEngineProblem)(IECB*, DWORD, LPDWORD);
};
struct IECB { const struct IECBVtbl *v; };
static LONG cbref = 1;
static HRESULT WINAPI cb_qi(IECB *i, REFIID r, void **p) { if (IsEqualGUID(r,&IID_IECB)||IsEqualGUID(r,&IID_IUnknown)) { *p=i; return S_OK;} *p=NULL; return E_NOINTERFACE; }
static ULONG WINAPI cb_addref(IECB *i) { return InterlockedIncrement(&cbref); }
static ULONG WINAPI cb_release(IECB *i) { return InterlockedDecrement(&cbref); }
static HRESULT WINAPI cb_status(IECB *i, DWORD s, DWORD ss) { cb.status++; return S_OK; }
static HRESULT WINAPI cb_si(IECB *i, DWORD a, DWORD b) { cb.start_install++; return S_OK; }
static IE2 *abort_engine;
static HRESULT WINAPI cb_sc(IECB *i, const char *id, DWORD a, DWORD b, const char *s) { cb.start_comp++; if (abort_engine) abort_engine->v->Abort(abort_engine, 0); return S_OK; }
static HRESULT WINAPI cb_prog(IECB *i, const char *id, DWORD a, const char *s, const char *m, ULONG p, ULONG mx) { return S_OK; }
static HRESULT WINAPI cb_stc(IECB *i, const char *id, HRESULT e, DWORD p, const char *s, DWORD st) { cb.stop_comp++; cb.last_err = e; return S_OK; }
static HRESULT WINAPI cb_sti(IECB *i, HRESULT e, const char *s, DWORD st) { cb.stop_install++; return S_OK; }
static HRESULT WINAPI cb_prob(IECB *i, DWORD p, LPDWORD a) { return S_OK; }
static const struct IECBVtbl cbvtbl = { cb_qi, cb_addref, cb_release, cb_status, cb_si, cb_sc, cb_prog, cb_stc, cb_sti, cb_prob };
static IECB cbobj = { &cbvtbl };

static void write_file(const char *path, const char *text)
{
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD n;
    WriteFile(h, text, strlen(text), &n, NULL);
    CloseHandle(h);
}

int main(void)
{
    IE2 *e;
    HRESULT hr;
    DWORD st;
    char *id;
    SIZES sz;
    HKEY key;
    char buf[64];

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_IE, NULL, CLSCTX_INPROC_SERVER, &IID_IE2, (void**)&e);
    CHECK(hr == S_OK, "create hr=%lx", hr);
    if (FAILED(hr)) goto done;

    CreateDirectoryA("C:\\inseng", NULL);
    CreateDirectoryA("C:\\inseng\\dl", NULL);
    write_file("C:\\inseng\\payload.txt", "payload");
    write_file("C:\\inseng\\a.cif",
        "[Version]\nSignature=$Chicago$\n"
        "[CompA]\nGUID={11111111-2222-3333-4444-555555555555}\nVersion=5,2,0,0\nSize=2000,5000\n"
        "URL1=payload.txt,0\nCommand1=cmd /c echo ran> C:\\inseng\\ran.txt\n"
        "[CompB]\nGUID={66666666-2222-3333-4444-555555555555}\nVersion=1,0,0,0\nSize=10,20\n"
        "URL1=missing.txt,0\n");
    write_file("C:\\inseng\\x.inf", "[Version]\nSignature=\"$CHICAGO$\"\n[DefaultInstall]\nAddReg=R\n[R]\nHKCU,Software\\InsengProbe,Ran,0x10001,7\n");

    st = 99;
    hr = e->v->GetEngineStatus(e, &st);
    CHECK(hr == S_OK && st == 0, "initial status hr=%lx st=%lu", hr, st);
    CHECK(e->v->GetEngineStatus(e, NULL) == E_INVALIDARG, "null status");

    CHECK(e->v->SetCifFile(e, NULL, NULL) == E_INVALIDARG, "null cif");
    hr = e->v->SetCifFile(e, NULL, "C:\\inseng\\nonexistent.cif");
    CHECK(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "missing cif hr=%lx", hr);
    hr = e->v->RegisterCB(e, &cbobj);
    CHECK(hr == S_OK && cbref == 2, "register hr=%lx ref=%ld", hr, cbref);
    hr = e->v->SetCifFile(e, NULL, "C:\\inseng\\a.cif");
    CHECK(hr == S_OK, "setcif hr=%lx", hr);
    e->v->GetEngineStatus(e, &st);
    CHECK(st == 1 && cb.status == 1, "ready st=%lu cbstatus=%d", st, cb.status);

    hr = e->v->EnumInstallIDs(e, 0, &id);
    CHECK(hr == HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS), "no actions yet hr=%lx", hr);
    CHECK(e->v->SetAction(e, "nope", 1, 0) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND), "unknown component");
    CHECK(e->v->SetAction(e, "compa", 1, 5) == S_OK, "setaction (case-insensitive)");
    hr = e->v->EnumInstallIDs(e, 0, &id);
    CHECK(hr == S_OK && id && !strcmp(id, "CompA"), "enum install hr=%lx id=%s", hr, id ? id : "(null)");
    if (hr == S_OK) CoTaskMemFree(id);
    CHECK(e->v->EnumInstallIDs(e, 1, &id) == HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS), "enum past end");
    hr = e->v->EnumDownloadIDs(e, 0, &id);
    CHECK(hr == S_OK && !strcmp(id, "CompA"), "enum download");
    if (hr == S_OK) CoTaskMemFree(id);

    memset(&sz, 0, sizeof(sz)); sz.cbSize = sizeof(sz);
    hr = e->v->GetSizes(e, "CompA", &sz);
    CHECK(hr == S_OK && sz.dwDownloadSize == 2000 && sz.dwInstallSize == 5000 && sz.dwTotalDownloadSize == 2000
          && sz.chWinDrive == 'C', "sizes hr=%lx dl=%lu inst=%lu win=%c", hr, sz.dwDownloadSize, sz.dwInstallSize, sz.chWinDrive);
    sz.cbSize = 4;
    CHECK(e->v->GetSizes(e, "CompA", &sz) == E_INVALIDARG, "bad cbSize");
    sz.cbSize = sizeof(sz);
    CHECK(e->v->GetSizes(e, "zzz", &sz) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND), "sizes unknown");
    CHECK(e->v->SetInstallDrive(e, '1') == E_INVALIDARG, "bad drive");
    CHECK(e->v->SetInstallDrive(e, 'D') == S_OK, "drive");
    e->v->GetSizes(e, "CompA", &sz);
    CHECK(sz.chInstallDrive == 'D', "install drive %c", sz.chInstallDrive);

    /* installed-state from the registry */
    hr = e->v->IsComponentInstalled(e, "CompA", &st);
    CHECK(hr == S_OK && st == 0, "not installed hr=%lx st=%lu", hr, st);
    RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Active Setup\\Installed Components\\{11111111-2222-3333-4444-555555555555}", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
    RegSetValueExA(key, "Version", 0, REG_SZ, (BYTE*)"5,1,0,0", 8);
    hr = e->v->IsComponentInstalled(e, "CompA", &st);
    CHECK(hr == S_OK && st == 2, "older hr=%lx st=%lu", hr, st);
    RegSetValueExA(key, "Version", 0, REG_SZ, (BYTE*)"5,2,0,0", 8);
    e->v->IsComponentInstalled(e, "CompA", &st);
    CHECK(st == 1, "equal version installed st=%lu", st);
    RegSetValueExA(key, "Version", 0, REG_SZ, (BYTE*)"6,0,0,0", 8);
    e->v->IsComponentInstalled(e, "CompA", &st);
    CHECK(st == 1, "newer version installed st=%lu", st);
    RegCloseKey(key);
    RegDeleteKeyA(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Active Setup\\Installed Components\\{11111111-2222-3333-4444-555555555555}");
    CHECK(e->v->IsComponentInstalled(e, "zzz", &st) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND), "installed unknown");

    /* download + install */
    CHECK(e->v->SetDownloadDir(e, "C:\\inseng\\dl") == S_OK, "dl dir");
    hr = e->v->DownloadComponents(e, 0);
    CHECK(hr == S_OK, "download hr=%lx", hr);
    CHECK(GetFileAttributesA("C:\\inseng\\dl\\payload.txt") != INVALID_FILE_ATTRIBUTES, "payload downloaded");
    CHECK(cb.start_install == 1 && cb.start_comp == 1 && cb.stop_comp == 1 && cb.stop_install == 1,
          "callbacks %d %d %d %d", cb.start_install, cb.start_comp, cb.stop_comp, cb.stop_install);
    CHECK(e->v->EnumDownloadIDs(e, 0, &id) == HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS), "nothing left to download");
    hr = e->v->InstallComponents(e, 0);
    CHECK(hr == S_OK, "install hr=%lx", hr);
    CHECK(GetFileAttributesA("C:\\inseng\\ran.txt") != INVALID_FILE_ATTRIBUTES, "install command ran");
    e->v->GetEngineStatus(e, &st);
    CHECK(st == 5, "done status %lu", st);

    /* a component whose URL is missing fails and reports through the callbacks */
    e->v->SetAction(e, "CompA", 0, 0);
    e->v->SetAction(e, "CompB", 1, 0);
    memset(&cb, 0, sizeof(cb));
    hr = e->v->DownloadComponents(e, 0);
    CHECK(FAILED(hr), "missing download must fail hr=%lx", hr);
    CHECK(cb.stop_comp == 1 && FAILED(cb.last_err) && cb.stop_install == 1, "failure callbacks");

    /* Abort from a callback stops the run before the next component */
    write_file("C:\\inseng\\c.cif", "[Version]\nSignature=x\n[One]\nURL1=payload.txt\n[Two]\nURL1=payload.txt\n");
    e->v->SetLocalCif(e, "C:\\inseng\\c.cif");
    e->v->SetAction(e, "One", 1, 0);
    e->v->SetAction(e, "Two", 1, 0);
    memset(&cb, 0, sizeof(cb));
    e->v->RegisterCB(e, &cbobj);
    abort_engine = e;
    hr = e->v->DownloadComponents(e, 0);
    abort_engine = NULL;
    CHECK(hr == E_ABORT && cb.start_comp == 1 && cb.stop_install == 1, "abort hr=%lx comps=%d", hr, cb.start_comp);
    CHECK(e->v->Suspend(e) == S_OK && e->v->Resume(e) == S_OK, "suspend/resume");
    e->v->UnregisterCB(e);

    /* LaunchExtraCommand runs an INF section */
    hr = e->v->LaunchExtraCommand(e, "C:\\inseng\\x.inf", "DefaultInstall");
    CHECK(hr == S_OK, "launch hr=%lx", hr);
    CHECK(RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\InsengProbe", 0, KEY_READ, &key) == 0, "inf section ran");
    if (key) RegCloseKey(key);
    CHECK(e->v->LaunchExtraCommand(e, "C:\\inseng\\nofile.inf", "x") == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "launch missing inf");

    /* SetLocalCif replaces the component list */
    write_file("C:\\inseng\\b.cif", "[Version]\nSignature=x\n[Only]\nSize=1,2\n");
    CHECK(e->v->SetLocalCif(e, "C:\\inseng\\b.cif") == S_OK, "local cif");
    CHECK(e->v->SetAction(e, "CompA", 1, 0) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND), "old component gone");
    CHECK(e->v->SetAction(e, "Only", 1, 0) == S_OK, "new component");

    CHECK(e->v->UnregisterCB(e) == S_OK && cbref == 1, "unregister ref=%ld", cbref);
    CHECK(e->v->SetBaseUrl(e, "http://example.invalid/") == S_OK, "base url");
    CHECK(e->v->SetInstallOptions(e, 3) == S_OK, "options");
    CHECK(e->v->SetHWND(e, NULL) == S_OK, "hwnd");
    e->v->Release(e);
done:
    CoUninitialize();
    printf("%d checks, %d failures\n", checks, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
