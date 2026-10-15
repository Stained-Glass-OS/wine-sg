/* query.dll LoadIFilter and the persistent handler registration (patch 3302), run by test/fastiq-gate.sh */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ole2.h>
#include <filter.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

DEFINE_GUID(CLSID_Handler, 0x6c5a0a01, 0x1111, 0x4222, 0x83, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66);
DEFINE_GUID(CLSID_NoAddin, 0x6c5a0a02, 0x1111, 0x4222, 0x83, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66);
DEFINE_GUID(CLSID_Filter, 0x6c5a0a03, 0x1111, 0x4222, 0x83, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66);

typedef HRESULT (WINAPI *LOADIFILTER)(const WCHAR *, IUnknown *, void **);

static WCHAR loaded_path[MAX_PATH];
static DWORD loaded_mode;
static int loads, filters_alive, creates, last_outer_set;
static HRESULT load_result = S_OK;

struct filter
{
    IFilter IFilter_iface;
    IPersistFile IPersistFile_iface;
    LONG ref;
};

static struct filter *impl_f(IFilter *i) { return CONTAINING_RECORD(i, struct filter, IFilter_iface); }
static struct filter *impl_p(IPersistFile *i) { return CONTAINING_RECORD(i, struct filter, IPersistFile_iface); }

static HRESULT WINAPI f_QI(IFilter *iface, REFIID riid, void **out)
{
    struct filter *f = impl_f(iface);
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IFilter)) *out = &f->IFilter_iface;
    else if (IsEqualIID(riid, &IID_IPersistFile)) *out = &f->IPersistFile_iface;
    else { *out = NULL; return E_NOINTERFACE; }
    InterlockedIncrement(&f->ref);
    return S_OK;
}
static ULONG WINAPI f_AddRef(IFilter *iface) { return InterlockedIncrement(&impl_f(iface)->ref); }
static ULONG WINAPI f_Release(IFilter *iface)
{
    struct filter *f = impl_f(iface);
    LONG ref = InterlockedDecrement(&f->ref);
    if (!ref) { filters_alive--; HeapFree(GetProcessHeap(), 0, f); }
    return ref;
}
static SCODE WINAPI f_Init(IFilter *i, ULONG a, ULONG b, const FULLPROPSPEC *c, ULONG *d) { return S_OK; }
static SCODE WINAPI f_GetChunk(IFilter *i, STAT_CHUNK *c) { return ((SCODE)0x80041700); }
static SCODE WINAPI f_GetText(IFilter *i, ULONG *a, WCHAR *b) { return ((SCODE)0x80041701); }
static SCODE WINAPI f_GetValue(IFilter *i, PROPVARIANT **v) { return ((SCODE)0x80041702); }
static SCODE WINAPI f_BindRegion(IFilter *i, FILTERREGION r, REFIID riid, void **o) { return E_NOTIMPL; }
static const IFilterVtbl filter_vtbl = { f_QI, f_AddRef, f_Release, f_Init, f_GetChunk, f_GetText, f_GetValue, f_BindRegion };

static HRESULT WINAPI p_QI(IPersistFile *i, REFIID r, void **o) { return f_QI(&impl_p(i)->IFilter_iface, r, o); }
static ULONG WINAPI p_AddRef(IPersistFile *i) { return f_AddRef(&impl_p(i)->IFilter_iface); }
static ULONG WINAPI p_Release(IPersistFile *i) { return f_Release(&impl_p(i)->IFilter_iface); }
static HRESULT WINAPI p_GetClassID(IPersistFile *i, CLSID *c) { *c = CLSID_Filter; return S_OK; }
static HRESULT WINAPI p_IsDirty(IPersistFile *i) { return S_FALSE; }
static HRESULT WINAPI p_Load(IPersistFile *i, LPCOLESTR path, DWORD mode)
{
    loads++;
    lstrcpynW(loaded_path, path, MAX_PATH);
    loaded_mode = mode;
    return load_result;
}
static HRESULT WINAPI p_Save(IPersistFile *i, LPCOLESTR p, BOOL r) { return E_NOTIMPL; }
static HRESULT WINAPI p_SaveCompleted(IPersistFile *i, LPCOLESTR p) { return E_NOTIMPL; }
static HRESULT WINAPI p_GetCurFile(IPersistFile *i, LPOLESTR *p) { return E_NOTIMPL; }
static const IPersistFileVtbl persist_vtbl = { p_QI, p_AddRef, p_Release, p_GetClassID, p_IsDirty, p_Load, p_Save, p_SaveCompleted, p_GetCurFile };

static HRESULT WINAPI cf_QI(IClassFactory *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IClassFactory)) { *o = i; return S_OK; }
    *o = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cf_AddRef(IClassFactory *i) { return 2; }
static ULONG WINAPI cf_Release(IClassFactory *i) { return 1; }
static HRESULT WINAPI cf_Create(IClassFactory *i, IUnknown *outer, REFIID riid, void **out)
{
    struct filter *f;
    HRESULT hr;
    creates++;
    last_outer_set = outer != NULL;
    if (outer) return CLASS_E_NOAGGREGATION;
    f = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*f));
    f->IFilter_iface.lpVtbl = &filter_vtbl;
    f->IPersistFile_iface.lpVtbl = &persist_vtbl;
    f->ref = 1;
    filters_alive++;
    hr = IFilter_QueryInterface(&f->IFilter_iface, riid, out);
    IFilter_Release(&f->IFilter_iface);
    return hr;
}
static HRESULT WINAPI cf_Lock(IClassFactory *i, BOOL l) { return S_OK; }
static const IClassFactoryVtbl cf_vtbl = { cf_QI, cf_AddRef, cf_Release, cf_Create, cf_Lock };
static IClassFactory factory = { &cf_vtbl };

static void set_value(const WCHAR *key, const WCHAR *value)
{
    HKEY hkey;
    WCHAR path[300];
    swprintf(path, 300, L"Software\\Classes\\%ls", key);
    RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, NULL, 0, KEY_WRITE, NULL, &hkey, NULL);
    RegSetValueExW(hkey, NULL, 0, REG_SZ, (const BYTE *)value, (lstrlenW(value) + 1) * sizeof(WCHAR));
    RegCloseKey(hkey);
}

int main(void)
{
    LOADIFILTER load_ifilter;
    HMODULE query = LoadLibraryA("query.dll");
    WCHAR path[MAX_PATH], path2[MAX_PATH];
    void *unk;
    IFilter *filter;
    DWORD cookie;
    HANDLE file;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    load_ifilter = (LOADIFILTER)GetProcAddress(query, "LoadIFilter");
    if (!load_ifilter) { check(0, "query exports LoadIFilter"); return 1; }

    CoRegisterClassObject(&CLSID_Filter, (IUnknown *)&factory, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &cookie);

    /* registrations: .sgx has a persistent handler with a filter; .sgn a handler without one;
     * .sgp is of the class sgfile whose persistent handler is the same */
    set_value(L".sgx\\PersistentHandler", L"{6c5a0a01-1111-4222-8333-444455556666}");
    set_value(L"CLSID\\{6c5a0a01-1111-4222-8333-444455556666}\\PersistentAddinsRegistered\\{89BCB740-6119-101A-BCB7-00DD010655AF}", L"{6c5a0a03-1111-4222-8333-444455556666}");
    set_value(L".sgn\\PersistentHandler", L"{6c5a0a02-1111-4222-8333-444455556666}");
    set_value(L".sgp", L"sgfile");
    set_value(L"sgfile\\PersistentHandler", L"{6c5a0a01-1111-4222-8333-444455556666}");

    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"fastiq-test.sgx");
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(file);

    unk = (void *)1;
    hr = load_ifilter(NULL, NULL, &unk);
    CHECKF(hr == E_INVALIDARG && unk == NULL, "NULL path is E_INVALIDARG (%#lx)", hr);
    hr = load_ifilter(path, NULL, NULL);
    CHECKF(hr == E_INVALIDARG, "NULL output is E_INVALIDARG (%#lx)", hr);

    loads = creates = 0;
    unk = NULL;
    hr = load_ifilter(path, NULL, &unk);
    CHECKF(hr == S_OK && unk, "the filter of a .sgx file (%#lx)", hr);
    CHECKF(creates == 1 && loads == 1 && !lstrcmpiW(loaded_path, path) && (loaded_mode & 3) == STGM_READ,
            "it is made once and loads the file (%d, %d, mode %#lx)", creates, loads, loaded_mode);
    hr = IUnknown_QueryInterface((IUnknown *)unk, &IID_IFilter, (void **)&filter);
    CHECKF(hr == S_OK, "it is an IFilter (%#lx)", hr);
    if (hr == S_OK) IFilter_Release(filter);
    IUnknown_Release((IUnknown *)unk);
    CHECKF(filters_alive == 0, "released, it is gone (%d)", filters_alive);

    /* just an extension: no file to load */
    loads = creates = 0;
    unk = NULL;
    hr = load_ifilter(L".sgx", NULL, &unk);
    CHECKF(hr == S_OK && unk && creates == 1 && loads == 0, "an extension alone gives the filter without loading (%#lx, %d loads)", hr, loads);
    if (unk) IUnknown_Release((IUnknown *)unk);

    /* a name that is not there is fine: the type decides */
    lstrcpyW(path2, L"C:\\no\\such\\dir\\name.SGX");
    loads = 0; unk = NULL;
    hr = load_ifilter(path2, NULL, &unk);
    CHECKF(hr == S_OK && unk && loads == 1 && !lstrcmpW(loaded_path, path2), "the extension is found in any case (%#lx)", hr);
    if (unk) IUnknown_Release((IUnknown *)unk);

    /* through the class of the type */
    unk = NULL;
    hr = load_ifilter(L".sgp", NULL, &unk);
    CHECKF(hr == S_OK && unk, "the persistent handler of the file type's class (%#lx)", hr);
    if (unk) IUnknown_Release((IUnknown *)unk);

    /* not found */
    unk = (void *)1;
    hr = load_ifilter(L".sgz", NULL, &unk);
    CHECKF(hr == E_FAIL && unk == NULL, "no handler: E_FAIL (%#lx)", hr);
    unk = (void *)1;
    hr = load_ifilter(L".sgn", NULL, &unk);
    CHECKF(hr == E_FAIL && unk == NULL, "a handler without a filter: E_FAIL (%#lx)", hr);
    unk = (void *)1;
    hr = load_ifilter(L"C:\\dir.d\\noext", NULL, &unk);
    CHECKF(hr == E_FAIL && unk == NULL, "no extension (a dot in a directory does not count): E_FAIL (%#lx)", hr);

    /* the filter cannot read the file */
    load_result = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    unk = (void *)1;
    hr = load_ifilter(path, NULL, &unk);
    CHECKF(hr == E_FAIL && unk == NULL && filters_alive == 0, "a filter that fails to load the file is released: E_FAIL (%#lx, %d alive)", hr, filters_alive);
    load_result = E_ACCESSDENIED;
    hr = load_ifilter(path, NULL, &unk);
    CHECKF(hr == E_ACCESSDENIED && unk == NULL, "access denied is kept (%#lx)", hr);
    load_result = S_OK;

    /* aggregation: the factory is asked for it */
    unk = (void *)1;
    last_outer_set = 0;
    hr = load_ifilter(L".sgx", (IUnknown *)&factory, &unk);
    CHECKF(last_outer_set && hr != S_OK && unk == NULL, "the outer unknown reaches the filter's factory (%#lx)", hr);

    CoRevokeClassObject(cookie);
    DeleteFileW(path);
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"Software\\Classes\\.sgx");
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"Software\\Classes\\.sgn");
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"Software\\Classes\\.sgp");
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"Software\\Classes\\sgfile");
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"Software\\Classes\\CLSID\\{6c5a0a01-1111-4222-8333-444455556666}");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
