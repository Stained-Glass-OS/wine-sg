/* tlbbase-gate.sh's probe (patches/sg/1301): marshals ISgEdge (see
 * tlbbase.idl) from this STA to an MTA thread with only ISgEdge registered
 * (ProxyStubClsid32 = the type library marshaler) and calls the methods it
 * inherits through the proxy. Prints "NAME VALUE" lines. argv[1]: the .tlb. */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <stdio.h>

static const GUID LIBID_SgTlbBase = {0x5f2a8a0e,0x5a7e,0x4c4c,{0x9a,0x9a,0x5a,0xb0,0x00,0x00,0x00,0x01}};
static const GUID IID_ISgGrand = {0x5f2a8a0e,0x5a7e,0x4c4c,{0x9a,0x9a,0x5a,0xb0,0x00,0x00,0x00,0x10}};
static const GUID IID_ISgBase = {0x5f2a8a0e,0x5a7e,0x4c4c,{0x9a,0x9a,0x5a,0xb0,0x00,0x00,0x00,0x11}};
static const GUID IID_ISgEdge = {0x5f2a8a0e,0x5a7e,0x4c4c,{0x9a,0x9a,0x5a,0xb0,0x00,0x00,0x00,0x12}};

typedef struct obj obj;
struct vtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(obj *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(obj *);
    ULONG (STDMETHODCALLTYPE *Release)(obj *);
    HRESULT (STDMETHODCALLTYPE *Add)(obj *, int, int, int *);
    HRESULT (STDMETHODCALLTYPE *Mul)(obj *, int, int, int *);
    HRESULT (STDMETHODCALLTYPE *Neg)(obj *, int, int *);
};
struct obj { const struct vtbl *lpVtbl; LONG ref; };

static HRESULT STDMETHODCALLTYPE o_qi(obj *o, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_ISgGrand) ||
        IsEqualGUID(iid, &IID_ISgBase) || IsEqualGUID(iid, &IID_ISgEdge))
    { *out = o; InterlockedIncrement(&o->ref); return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE o_addref(obj *o) { return InterlockedIncrement(&o->ref); }
static ULONG STDMETHODCALLTYPE o_release(obj *o) { return InterlockedDecrement(&o->ref); }
static HRESULT STDMETHODCALLTYPE o_add(obj *o, int a, int b, int *r) { *r = a + b; return S_OK; }
static HRESULT STDMETHODCALLTYPE o_mul(obj *o, int a, int b, int *r) { *r = a * b; return S_OK; }
static HRESULT STDMETHODCALLTYPE o_neg(obj *o, int a, int *r) { *r = -a; return S_OK; }
static const struct vtbl o_vtbl = { o_qi, o_addref, o_release, o_add, o_mul, o_neg };
static obj the_obj = { &o_vtbl, 1 };

static IStream *stream;
static DWORD main_tid;

static DWORD WINAPI client(void *arg)
{
    obj *p = NULL;
    int r;
    HRESULT hr;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoGetInterfaceAndReleaseStream(stream, &IID_ISgEdge, (void **)&p);
    printf("unmarshal %08lx\n", hr);
    if (SUCCEEDED(hr))
    {
        printf("proxy %d\n", p != &the_obj);
        r = 0; hr = p->lpVtbl->Add(p, 2, 3, &r); printf("add %08lx %d\n", hr, r);
        r = 0; hr = p->lpVtbl->Mul(p, 4, 5, &r); printf("mul %08lx %d\n", hr, r);
        r = 0; hr = p->lpVtbl->Neg(p, 7, &r); printf("neg %08lx %d\n", hr, r);
        p->lpVtbl->Release(p);
    }
    fflush(stdout);
    CoUninitialize();
    PostThreadMessageW(main_tid, WM_QUIT, 0, 0);
    return 0;
}

static void set(HKEY root, const char *key, const char *name, const char *value)
{
    HKEY k;
    RegCreateKeyExA(root, key, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    RegSetValueExA(k, name, 0, REG_SZ, (const BYTE *)value, strlen(value) + 1);
    RegCloseKey(k);
}

int main(int argc, char **argv)
{
    char key[200], path[MAX_PATH];
    HANDLE thread;
    MSG msg;
    HRESULT hr;

    if (argc < 2) return 2;
    GetFullPathNameA(argv[1], MAX_PATH, path, NULL);
    /* only ISgEdge is registered, as Edge registers only IElevatorEdge */
    strcpy(key, "Software\\Classes\\Interface\\{5F2A8A0E-5A7E-4C4C-9A9A-5AB000000012}");
    set(HKEY_LOCAL_MACHINE, key, NULL, "ISgEdge");
    strcat(key, "\\ProxyStubClsid32");
    set(HKEY_LOCAL_MACHINE, key, NULL, "{00020424-0000-0000-C000-000000000046}");
    strcpy(key, "Software\\Classes\\Interface\\{5F2A8A0E-5A7E-4C4C-9A9A-5AB000000012}\\TypeLib");
    set(HKEY_LOCAL_MACHINE, key, NULL, "{5F2A8A0E-5A7E-4C4C-9A9A-5AB000000001}");
    set(HKEY_LOCAL_MACHINE, key, "Version", "1.0");
    set(HKEY_LOCAL_MACHINE, "Software\\Classes\\TypeLib\\{5F2A8A0E-5A7E-4C4C-9A9A-5AB000000001}\\1.0\\0\\win64", NULL, path);
    set(HKEY_LOCAL_MACHINE, "Software\\Classes\\TypeLib\\{5F2A8A0E-5A7E-4C4C-9A9A-5AB000000001}\\1.0\\0\\win32", NULL, path);

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    main_tid = GetCurrentThreadId();
    {
        CLSID ps;
        printf("base-registered %d\n", SUCCEEDED(CoGetPSClsid(&IID_ISgBase, &ps)));
    }
    hr = CoMarshalInterThreadInterfaceInStream(&IID_ISgEdge, (IUnknown *)&the_obj, &stream);
    printf("marshal %08lx\n", hr);
    fflush(stdout);
    if (FAILED(hr)) return 1;
    thread = CreateThread(NULL, 0, client, NULL, 0, NULL);
    while (GetMessageW(&msg, 0, 0, 0)) DispatchMessageW(&msg);
    WaitForSingleObject(thread, 10000);
    CoUninitialize();
    return 0;
}
