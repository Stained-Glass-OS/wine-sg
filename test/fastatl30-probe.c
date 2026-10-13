/* atl.dll (ATL 3.0): unregistering a server, revoking class objects, subclassed window classes */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>
#include <stddef.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct
{
    UINT cbSize;
    HINSTANCE m_hInst, m_hInstResource, m_hInstTypeLib;
    void *m_pObjMap;
    LONG m_nLockCnt;
    HANDLE m_hHeap;
    CRITICAL_SECTION cs[3];
} module_v1;

typedef struct
{
    const CLSID *pclsid;
    HRESULT (WINAPI *pfnUpdateRegistry)(BOOL);
    HRESULT (WINAPI *pfnGetClassObject)(void *, REFIID, void **);
    HRESULT (WINAPI *pfnCreateInstance)(void *, REFIID, void **);
    IUnknown *pCF;
    DWORD dwRegister;
    void *pfnGetObjectDescription;
} entry_v1;

typedef struct
{
    WNDCLASSEXW m_wc;
    LPCWSTR m_lpszOrigName;
    WNDPROC pWndProc;
    LPCWSTR m_lpszCursorID;
    BOOL m_bSystemCursor;
    ATOM m_atom;
    WCHAR m_szAutoName[sizeof("ATL:") + sizeof(void *) * 2];
} wndclassinfo;

static const CLSID clsid_a = {0x6a000001, 0x1111, 0x2222, {1, 2, 3, 4, 5, 6, 7, 8}};
static const CLSID clsid_b = {0x6a000002, 0x1111, 0x2222, {1, 2, 3, 4, 5, 6, 7, 8}};
static int a_unreg, a_reg, b_unreg, b_reg;

static HRESULT WINAPI update_a(BOOL reg) { if (reg) a_reg++; else a_unreg++; return S_OK; }
static HRESULT WINAPI update_b(BOOL reg) { if (reg) b_reg++; else b_unreg++; return S_OK; }

/* a class factory */
static HRESULT WINAPI cf_qi(IClassFactory *iface, REFIID riid, void **out)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IClassFactory)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cf_addref(IClassFactory *iface) { return 2; }
static ULONG WINAPI cf_release(IClassFactory *iface) { return 1; }
static HRESULT WINAPI cf_create(IClassFactory *iface, IUnknown *outer, REFIID riid, void **out) { *out = NULL; return E_NOTIMPL; }
static HRESULT WINAPI cf_lock(IClassFactory *iface, BOOL lock) { return S_OK; }
static const IClassFactoryVtbl cf_vtbl = { cf_qi, cf_addref, cf_release, cf_create, cf_lock };
static IClassFactory factory = { &cf_vtbl };

static HRESULT WINAPI get_class_object(void *create, REFIID riid, void **out)
{
    return IClassFactory_QueryInterface(&factory, riid, out);
}

static LRESULT CALLBACK own_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) { return DefWindowProcW(hwnd, msg, wp, lp); }

int main(void)
{
    HMODULE atl = LoadLibraryA("atl.dll");
    HRESULT (WINAPI *init)(module_v1 *, entry_v1 *, HINSTANCE);
    HRESULT (WINAPI *unregister)(module_v1 *, const CLSID *);
    HRESULT (WINAPI *unregister_ex)(module_v1 *, BOOL, const CLSID *);
    HRESULT (WINAPI *register_objects)(module_v1 *, DWORD, DWORD);
    HRESULT (WINAPI *revoke_objects)(module_v1 *);
    ATOM (WINAPI *register_class)(module_v1 *, wndclassinfo *, WNDPROC *);
    module_v1 module = { offsetof(module_v1, cs[0]) - 0 };
    entry_v1 entries[3] = {{0}};
    wndclassinfo info = {0};
    WNDCLASSEXW got = { sizeof(got) }, button = { sizeof(button) };
    IUnknown *obj = NULL;
    WNDPROC proc = NULL;
    HRESULT hr;
    ATOM atom;

    CoInitialize(NULL);
    CHECK(atl != NULL, "atl.dll not loaded");
    if (!atl) return 1;
    init = (void *)GetProcAddress(atl, "AtlModuleInit");
    unregister = (void *)GetProcAddress(atl, "AtlModuleUnregisterServer");
    unregister_ex = (void *)GetProcAddress(atl, "AtlModuleUnregisterServerEx");
    register_objects = (void *)GetProcAddress(atl, "AtlModuleRegisterClassObjects");
    revoke_objects = (void *)GetProcAddress(atl, "AtlModuleRevokeClassObjects");
    register_class = (void *)GetProcAddress(atl, "AtlModuleRegisterWndClassInfoW");
    CHECK(init && unregister && unregister_ex && register_objects && revoke_objects && register_class, "exports");

    /* the first version of the module: it ends before the build version */
    module.cbSize = sizeof(module_v1) - sizeof(module.cs) + 0;
    module.cbSize = offsetof(module_v1, cs) + 3 * sizeof(CRITICAL_SECTION);
    entries[0].pclsid = &clsid_a; entries[0].pfnUpdateRegistry = update_a; entries[0].pfnGetClassObject = get_class_object;
    entries[1].pclsid = &clsid_b; entries[1].pfnUpdateRegistry = update_b;
    hr = init(&module, entries, GetModuleHandleW(NULL));
    CHECK(hr == S_OK, "AtlModuleInit %lx", hr);

    hr = unregister(&module, NULL);
    CHECK(hr == S_OK && a_unreg == 1 && b_unreg == 1, "AtlModuleUnregisterServer %lx %d %d", hr, a_unreg, b_unreg);
    hr = unregister_ex(&module, FALSE, &clsid_b);
    CHECK(hr == S_OK && a_unreg == 1 && b_unreg == 2, "UnregisterServerEx(B) %lx %d %d", hr, a_unreg, b_unreg);
    hr = unregister_ex(&module, FALSE, NULL);
    CHECK(hr == S_OK && a_unreg == 2 && b_unreg == 3, "UnregisterServerEx(all) %lx %d %d", hr, a_unreg, b_unreg);
    hr = unregister_ex(NULL, FALSE, NULL);
    CHECK(hr == E_INVALIDARG, "UnregisterServerEx(NULL) %lx", hr);

    /* class objects are registered, then revoked */
    hr = register_objects(&module, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE);
    CHECK(hr == S_OK && entries[0].dwRegister, "AtlModuleRegisterClassObjects %lx", hr);
    hr = CoGetClassObject(&clsid_a, CLSCTX_INPROC_SERVER, NULL, &IID_IUnknown, (void **)&obj);
    CHECK(hr == S_OK && obj, "class object registered: %lx", hr);
    if (obj) IUnknown_Release(obj);
    hr = revoke_objects(&module);
    CHECK(hr == S_OK && !entries[0].dwRegister, "AtlModuleRevokeClassObjects %lx", hr);
    obj = NULL;
    hr = CoGetClassObject(&clsid_a, CLSCTX_INPROC_SERVER, NULL, &IID_IUnknown, (void **)&obj);
    CHECK(FAILED(hr) && !obj, "class object still registered: %lx", hr);
    hr = revoke_objects(NULL);
    CHECK(hr == E_INVALIDARG, "revoke(NULL) %lx", hr);

    /* a window class made on an existing one */
    GetClassInfoExW(NULL, L"BUTTON", &button);
    info.m_wc.cbSize = sizeof(info.m_wc);
    info.m_wc.lpszClassName = L"SgAtlSubclass";
    info.m_wc.lpfnWndProc = own_proc;
    info.m_lpszOrigName = L"BUTTON";
    info.m_bSystemCursor = TRUE;
    atom = register_class(&module, &info, &proc);
    CHECK(atom != 0, "AtlModuleRegisterWndClassInfoW failed %lu", GetLastError());
    CHECK(proc == button.lpfnWndProc, "original procedure %p vs %p", proc, button.lpfnWndProc);
    CHECK(GetClassInfoExW(module.m_hInst, L"SgAtlSubclass", &got) != 0, "class not registered");
    CHECK(got.lpfnWndProc == own_proc, "own procedure");
    CHECK(got.style == (button.style & ~CS_GLOBALCLASS), "style %#x vs %#x", got.style, button.style);
    CHECK(got.hCursor == button.hCursor, "cursor of the original");
    info.m_atom = 0;
    info.m_lpszOrigName = L"NoSuchClass";
    info.m_wc.lpszClassName = L"SgAtlSubclass2";
    atom = register_class(&module, &info, &proc);
    CHECK(atom == 0, "unknown class registered");

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails;
}
