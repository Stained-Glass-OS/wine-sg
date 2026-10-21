/* What System.Management does with wminet_utils.dll (patches/sg/5943): load it from
 * the .NET Framework folder by its full path, find its functions by name, connect
 * to ROOT\CIMV2, run a WQL query and read a property of what it returns. */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <wbemcli.h>
#include <stdio.h>

typedef HRESULT (WINAPI *connect_fn)(const WCHAR *, const WCHAR *, const WCHAR *, const WCHAR *, LONG, const WCHAR *,
                                      IWbemContext *, IWbemServices **, DWORD, DWORD);
typedef HRESULT (WINAPI *query_fn)(const WCHAR *, const WCHAR *, LONG, IWbemContext *, IEnumWbemClassObject **, DWORD,
                                    DWORD, IWbemServices *, const WCHAR *, const WCHAR *, const WCHAR *);
typedef HRESULT (WINAPI *get_fn)(int, IWbemClassObject *, const WCHAR *, LONG, VARIANT *, CIMTYPE *, LONG *);
typedef HRESULT (WINAPI *begin_fn)(int, IWbemClassObject *, LONG);
typedef HRESULT (WINAPI *next_fn)(int, IWbemClassObject *, LONG, BSTR *, VARIANT *, CIMTYPE *, LONG *);
typedef HRESULT (WINAPI *stub_fn)(IUnknown *, BOOL, IUnknown **);

/* a sink as System.Management makes them: IWmiEventSource (Indicate one object, SetStatus) */
typedef struct src { const void *vtbl; LONG refs, indicated, status; } src;
static const GUID IID_IWmiEventSource = {0x87a5ad68,0xa38a,0x43ef,{0xac,0xa9,0xef,0xe9,0x10,0xe5,0xd2,0x4c}};
static HRESULT WINAPI src_qi(src *s, REFIID riid, void **out)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IWmiEventSource)) { *out = s; s->refs++; return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI src_addref(src *s) { return ++s->refs; }
static ULONG WINAPI src_release(src *s) { return --s->refs; }
static HRESULT WINAPI src_indicate(src *s, IWbemClassObject *obj) { if (obj) s->indicated++; return S_OK; }
static HRESULT WINAPI src_setstatus(src *s, LONG flags, HRESULT hr, BSTR p, IWbemClassObject *o) { s->status = hr; return S_OK; }
static const void *src_vtbl[] = { src_qi, src_addref, src_release, src_indicate, src_setstatus };

static const char *names[] = {
    "BeginEnumeration", "BeginMethodEnumeration", "BlessIWbemServices", "BlessIWbemServicesObject", "Clone",
    "CloneEnumWbemClassObject", "CompareTo", "ConnectServerWmi", "CreateClassEnumWmi", "CreateInstanceEnumWmi",
    "Delete", "DeleteMethod", "EndEnumeration", "EndMethodEnumeration", "ExecNotificationQueryWmi", "ExecQueryWmi",
    "Get", "GetCurrentApartmentType", "GetDemultiplexedStub", "GetErrorInfo", "GetMethod", "GetMethodOrigin",
    "GetMethodQualifierSet", "GetNames", "GetObjectText", "GetPropertyHandle", "GetPropertyOrigin",
    "GetPropertyQualifierSet", "GetQualifierSet", "InheritsFrom", "Initialize", "Next", "NextMethod", "Put",
    "PutClassWmi", "PutInstanceWmi", "PutMethod", "QualifierSet_BeginEnumeration", "QualifierSet_Delete",
    "QualifierSet_EndEnumeration", "QualifierSet_Get", "QualifierSet_GetNames", "QualifierSet_Next",
    "QualifierSet_Put", "ResetSecurity", "SetSecurity", "SpawnDerivedClass", "SpawnInstance", "VerifyClientKey",
    "WritePropertyValue",
};

int main(void)
{
    WCHAR path[MAX_PATH];
    HMODULE mod;
    IWbemServices *svc = NULL;
    IEnumWbemClassObject *enm = NULL;
    IWbemClassObject *obj = NULL;
    IUnknown *stub = NULL;
    ULONG got = 0;
    VARIANT v;
    CIMTYPE type;
    BSTR name;
    HRESULT hr;
    int i, missing = 0, props = 0, ok = 1;

    GetWindowsDirectoryW(path, MAX_PATH);
    lstrcatW(path, sizeof(void *) == 8 ? L"\\Microsoft.NET\\Framework64\\v4.0.30319\\wminet_utils.dll"
                                       : L"\\Microsoft.NET\\Framework\\v4.0.30319\\wminet_utils.dll");
    if (!(mod = LoadLibraryW(path))) { printf("LOAD failed %lu\n", GetLastError()); return 1; }
    printf("LOAD ok\n");
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (!GetProcAddress(mod, names[i])) { printf("MISSING %s\n", names[i]); missing++; }
    printf("EXPORTS %d of %d\n", (int)(sizeof(names) / sizeof(names[0])) - missing, (int)(sizeof(names) / sizeof(names[0])));
    if (missing) ok = 0;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = ((connect_fn)GetProcAddress(mod, "ConnectServerWmi"))(L"ROOT\\CIMV2", NULL, NULL, NULL, 0, NULL, NULL, &svc, 3, 2);
    printf("CONNECT %#lx\n", hr);
    if (FAILED(hr)) return 1;
    hr = ((query_fn)GetProcAddress(mod, "ExecQueryWmi"))(L"WQL", L"SELECT * FROM Win32_OperatingSystem",
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &enm, 3, 2, svc, NULL, NULL, NULL);
    printf("QUERY %#lx\n", hr);
    if (FAILED(hr) || !enm) return 1;
    hr = IEnumWbemClassObject_Next(enm, WBEM_INFINITE, 1, &obj, &got);
    printf("NEXT %#lx %lu\n", hr, got);
    if (!got) return 1;
    VariantInit(&v);
    hr = ((get_fn)GetProcAddress(mod, "Get"))(4, obj, L"Caption", 0, &v, &type, NULL);
    printf("CAPTION %#lx %d %ls\n", hr, V_VT(&v), V_VT(&v) == VT_BSTR ? V_BSTR(&v) : L"");
    if (FAILED(hr) || V_VT(&v) != VT_BSTR || !V_BSTR(&v)[0]) ok = 0;
    VariantClear(&v);
    ((begin_fn)GetProcAddress(mod, "BeginEnumeration"))(8, obj, 0);
    while (((next_fn)GetProcAddress(mod, "Next"))(9, obj, 0, &name, &v, &type, NULL) == WBEM_S_NO_ERROR)
    {
        props++; SysFreeString(name); VariantClear(&v);
    }
    printf("PROPERTIES %d\n", props);
    if (props < 5) ok = 0;
    {
        /* what ManagementEventWatcher.Start() does: a stub for its sink, cast to IWbemObjectSink */
        src source = { src_vtbl, 1, 0, 0 };
        IWbemObjectSink *sink = NULL;
        hr = ((stub_fn)GetProcAddress(mod, "GetDemultiplexedStub"))((IUnknown *)&source, TRUE, &stub);
        if (hr == S_OK && stub) hr = IUnknown_QueryInterface(stub, &IID_IWbemObjectSink, (void **)&sink);
        if (sink)
        {
            IWbemClassObject *two[2] = { obj, obj };
            IWbemObjectSink_Indicate(sink, 2, two);
            IWbemObjectSink_SetStatus(sink, WBEM_STATUS_COMPLETE, 0x80041032, NULL, NULL);
            IWbemObjectSink_Release(sink);
        }
        if (stub) IUnknown_Release(stub);
        printf("STUB %#lx sink %d indicated %ld status %#lx refs %ld\n", hr, sink != NULL, source.indicated, source.status, source.refs);
        if (!sink || source.indicated != 2 || source.status != 0x80041032 || source.refs != 1) ok = 0;
    }
    if (ok) printf("ALL-OK\n");
    return 0;
}
