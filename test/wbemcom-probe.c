/* Probe for patches/sg/2459: wbemprox COM semantics (IClientSecurity sharing the object's count, IWbemContext
 * enumeration, class derivation, qualifier sets, class object methods, NextAsync, WQL string escapes). */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <wbemcli.h>
#include <stdio.h>
#include <string.h>

static int fails;
static void check(const char *name, int ok) { printf("      %s  %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }
static void checkhr(const char *name, HRESULT got, HRESULT want)
{
    printf("      %s  %s (%#lx, want %#lx)\n", got == want ? "PASS" : "FAIL", name, (unsigned long)got, (unsigned long)want);
    if (got != want) fails++;
}

static IWbemServices *connect_ns(const WCHAR *ns)
{
    IWbemLocator *loc;
    IWbemServices *svc = NULL;
    BSTR path = SysAllocString(ns);
    if (CoCreateInstance(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (void **)&loc) != S_OK) return NULL;
    IWbemLocator_ConnectServer(loc, path, NULL, NULL, NULL, 0, NULL, NULL, &svc);
    SysFreeString(path);
    IWbemLocator_Release(loc);
    return svc;
}

static IEnumWbemClassObject *query(IWbemServices *svc, const WCHAR *q, HRESULT *hr)
{
    IEnumWbemClassObject *e = NULL;
    BSTR wql = SysAllocString(L"WQL"), str = SysAllocString(q);
    *hr = IWbemServices_ExecQuery(svc, wql, str, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &e);
    SysFreeString(wql);
    SysFreeString(str);
    return e;
}

static IWbemClassObject *first(IWbemServices *svc, const WCHAR *q)
{
    HRESULT hr;
    IEnumWbemClassObject *e = query(svc, q, &hr);
    IWbemClassObject *obj = NULL;
    ULONG n = 0;
    if (e)
    {
        IEnumWbemClassObject_Next(e, WBEM_INFINITE, 1, &obj, &n);
        IEnumWbemClassObject_Release(e);
    }
    return n ? obj : NULL;
}

static int names_have(SAFEARRAY *sa, const WCHAR *name, LONG *count)
{
    LONG lo, hi, i;
    int found = 0;
    SafeArrayGetLBound(sa, 1, &lo);
    SafeArrayGetUBound(sa, 1, &hi);
    if (count) *count = hi - lo + 1;
    for (i = lo; i <= hi; i++)
    {
        BSTR s;
        SafeArrayGetElement(sa, &i, &s);
        if (!wcsicmp(s, name)) found = 1;
        SysFreeString(s);
    }
    return found;
}

static int str_equals_at(SAFEARRAY *sa, LONG i, const WCHAR *want)
{
    BSTR s = NULL;
    int ok;
    if (SafeArrayGetElement(sa, &i, &s) != S_OK) return 0;
    ok = s && !wcscmp(s, want);
    SysFreeString(s);
    return ok;
}

/* a sink that counts what it is given */
struct sink { IWbemObjectSink iface; LONG refs; LONG indicated; HRESULT status; HANDLE done; };
static HRESULT WINAPI sink_QI(IWbemObjectSink *i, REFIID r, void **o)
{
    if (IsEqualGUID(r, &IID_IUnknown) || IsEqualGUID(r, &IID_IWbemObjectSink)) { *o = i; IWbemObjectSink_AddRef(i); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI sink_AddRef(IWbemObjectSink *i) { return InterlockedIncrement(&((struct sink *)i)->refs); }
static ULONG WINAPI sink_Release(IWbemObjectSink *i) { return InterlockedDecrement(&((struct sink *)i)->refs); }
static HRESULT WINAPI sink_Indicate(IWbemObjectSink *i, LONG n, IWbemClassObject **o) { InterlockedAdd(&((struct sink *)i)->indicated, n); return S_OK; }
static HRESULT WINAPI sink_SetStatus(IWbemObjectSink *i, LONG f, HRESULT hr, BSTR s, IWbemClassObject *o)
{
    struct sink *k = (struct sink *)i;
    if (f == WBEM_STATUS_COMPLETE) { k->status = hr; SetEvent(k->done); }
    return S_OK;
}
static IWbemObjectSinkVtbl sink_vtbl = { sink_QI, sink_AddRef, sink_Release, sink_Indicate, sink_SetStatus };

static void test_client_security(void)
{
    IWbemServices *svc = connect_ns(L"ROOT\\CIMV2");
    IClientSecurity *cs;
    IEnumWbemClassObject *e;
    IWbemClassObject *obj;
    DWORD authn, authz, level, imp, caps;
    OLECHAR *princ;
    HRESULT hr;
    ULONG refs;

    puts("   IClientSecurity");
    check("connected", svc != NULL);
    if (!svc) return;
    hr = IWbemServices_QueryInterface(svc, &IID_IClientSecurity, (void **)&cs);
    checkhr("QueryInterface(IClientSecurity)", hr, S_OK);
    refs = IWbemServices_Release(svc);
    check("the services object is still held by the interface", refs == 1);
    hr = IClientSecurity_SetBlanket(cs, (IUnknown *)svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL,
                                    RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
    checkhr("SetBlanket", hr, S_OK);
    hr = IClientSecurity_QueryBlanket(cs, (IUnknown *)svc, &authn, &authz, &princ, &level, &imp, NULL, &caps);
    checkhr("QueryBlanket", hr, S_OK);
    check("the blanket that was set comes back", authn == RPC_C_AUTHN_WINNT && level == RPC_C_AUTHN_LEVEL_CALL &&
          imp == RPC_C_IMP_LEVEL_IMPERSONATE && princ == NULL);
    refs = IClientSecurity_Release(cs);
    check("releasing the interface releases the object", refs == 0);

    svc = connect_ns(L"ROOT\\CIMV2");
    e = query(svc, L"SELECT * FROM Win32_BIOS", &hr);
    hr = IEnumWbemClassObject_QueryInterface(e, &IID_IClientSecurity, (void **)&cs);
    checkhr("enumerator: IClientSecurity", hr, S_OK);
    refs = IEnumWbemClassObject_Release(e);
    check("enumerator: shares the count", refs == 1);
    IClientSecurity_Release(cs);
    e = query(svc, L"SELECT * FROM Win32_BIOS", &hr);
    IEnumWbemClassObject_Next(e, WBEM_INFINITE, 1, &obj, &refs);
    hr = IWbemClassObject_QueryInterface(obj, &IID_IClientSecurity, (void **)&cs);
    checkhr("object: IClientSecurity", hr, S_OK);
    refs = IWbemClassObject_Release(obj);
    check("object: shares the count", refs == 1);
    IClientSecurity_Release(cs);
    IEnumWbemClassObject_Release(e);
    IWbemServices_Release(svc);
}

static void test_context(void)
{
    IWbemContext *ctx;
    VARIANT v;
    SAFEARRAY *sa = NULL;
    BSTR name;
    HRESULT hr;
    LONG count;

    puts("   IWbemContext");
    CoCreateInstance(&CLSID_WbemContext, NULL, CLSCTX_INPROC_SERVER, &IID_IWbemContext, (void **)&ctx);
    V_VT(&v) = VT_I4;
    V_I4(&v) = 1; IWbemContext_SetValue(ctx, L"first", 0, &v);
    V_I4(&v) = 2; IWbemContext_SetValue(ctx, L"second", 0, &v);
    V_I4(&v) = 3; IWbemContext_SetValue(ctx, L"third", 0, &v);
    hr = IWbemContext_GetNames(ctx, 0, &sa);
    checkhr("GetNames", hr, S_OK);
    check("three names, in the order they were set", sa && names_have(sa, L"first", &count) && count == 3 &&
          str_equals_at(sa, 0, L"first") && str_equals_at(sa, 2, L"third"));
    if (sa) SafeArrayDestroy(sa);
    checkhr("GetNames with flags", IWbemContext_GetNames(ctx, 1, &sa), WBEM_E_INVALID_PARAMETER);
    checkhr("Next without BeginEnumeration", IWbemContext_Next(ctx, 0, &name, &v), WBEM_E_UNEXPECTED);
    checkhr("BeginEnumeration", IWbemContext_BeginEnumeration(ctx, 0), S_OK);
    count = 0;
    while ((hr = IWbemContext_Next(ctx, 0, &name, &v)) == S_OK)
    {
        count++;
        if (count == 2) check("the second one is 'second' = 2", !wcscmp(name, L"second") && V_I4(&v) == 2);
        SysFreeString(name);
    }
    check("three values enumerated", count == 3);
    checkhr("the end", hr, WBEM_S_NO_MORE_DATA);
    checkhr("EndEnumeration", IWbemContext_EndEnumeration(ctx), S_OK);
    checkhr("Next after EndEnumeration", IWbemContext_Next(ctx, 0, &name, &v), WBEM_E_UNEXPECTED);
    checkhr("DeleteValue", IWbemContext_DeleteValue(ctx, L"SECOND", 0), S_OK);
    checkhr("DeleteValue of what is gone", IWbemContext_DeleteValue(ctx, L"second", 0), WBEM_E_NOT_FOUND);
    checkhr("GetValue of what is gone", IWbemContext_GetValue(ctx, L"second", 0, &v), WBEM_E_NOT_FOUND);
    IWbemContext_GetNames(ctx, 0, &sa);
    check("two names left", sa && names_have(sa, L"third", &count) && count == 2);
    if (sa) SafeArrayDestroy(sa);
    checkhr("DeleteAll", IWbemContext_DeleteAll(ctx), S_OK);
    IWbemContext_GetNames(ctx, 0, &sa);
    check("nothing left", sa && !names_have(sa, L"first", &count) && count == 0);
    if (sa) SafeArrayDestroy(sa);
    IWbemContext_Release(ctx);
}

static void test_classes(void)
{
    IWbemServices *svc = connect_ns(L"ROOT\\CIMV2");
    IWbemClassObject *proc, *svcobj, *obj2;
    VARIANT v;
    SAFEARRAY *sa;
    CIMTYPE type;
    BSTR origin;
    HRESULT hr;
    LONG count;

    puts("   class derivation");
    proc = first(svc, L"SELECT * FROM Win32_Process");
    svcobj = first(svc, L"SELECT * FROM Win32_Service");
    check("Win32_Process instance", proc != NULL);
    if (!proc) return;
    VariantInit(&v);
    hr = IWbemClassObject_Get(proc, L"__DERIVATION", 0, &v, &type, NULL);
    checkhr("__DERIVATION", hr, S_OK);
    check("CIM_Process, CIM_LogicalElement, CIM_ManagedSystemElement", V_VT(&v) == (VT_BSTR | VT_ARRAY) &&
          str_equals_at(V_ARRAY(&v), 0, L"CIM_Process") && str_equals_at(V_ARRAY(&v), 1, L"CIM_LogicalElement") &&
          str_equals_at(V_ARRAY(&v), 2, L"CIM_ManagedSystemElement"));
    VariantClear(&v);
    IWbemClassObject_Get(proc, L"__SUPERCLASS", 0, &v, &type, NULL);
    check("__SUPERCLASS is CIM_Process", V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), L"CIM_Process") && type == CIM_STRING);
    VariantClear(&v);
    IWbemClassObject_Get(proc, L"__DYNASTY", 0, &v, &type, NULL);
    check("__DYNASTY is CIM_ManagedSystemElement", V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), L"CIM_ManagedSystemElement"));
    VariantClear(&v);
    if (svcobj)
    {
        IWbemClassObject_Get(svcobj, L"__DERIVATION", 0, &v, &type, NULL);
        check("Win32_Service: Win32_BaseService, CIM_Service, ...", V_VT(&v) == (VT_BSTR | VT_ARRAY) &&
              str_equals_at(V_ARRAY(&v), 0, L"Win32_BaseService") && str_equals_at(V_ARRAY(&v), 1, L"CIM_Service") &&
              str_equals_at(V_ARRAY(&v), 3, L"CIM_ManagedSystemElement"));
        VariantClear(&v);
    }
    checkhr("InheritsFrom(CIM_LogicalElement)", IWbemClassObject_InheritsFrom(proc, L"CIM_LogicalElement"), S_OK);
    checkhr("InheritsFrom(cim_process), any case", IWbemClassObject_InheritsFrom(proc, L"cim_process"), S_OK);
    checkhr("InheritsFrom(Win32_BIOS)", IWbemClassObject_InheritsFrom(proc, L"Win32_BIOS"), WBEM_S_FALSE);
    checkhr("InheritsFrom(NULL)", IWbemClassObject_InheritsFrom(proc, NULL), WBEM_E_INVALID_PARAMETER);

    puts("   names, origins");
    hr = IWbemClassObject_GetNames(proc, NULL, 0, NULL, &sa);
    check("GetNames lists __SUPERCLASS and __DYNASTY", hr == S_OK && names_have(sa, L"__SUPERCLASS", &count) && names_have(sa, L"__DYNASTY", NULL));
    SafeArrayDestroy(sa);
    hr = IWbemClassObject_GetNames(proc, NULL, WBEM_FLAG_SYSTEM_ONLY, NULL, &sa);
    check("system only: ten names, no Handle", hr == S_OK && !names_have(sa, L"Handle", &count) && count == 10);
    SafeArrayDestroy(sa);
    hr = IWbemClassObject_GetNames(proc, NULL, WBEM_FLAG_NONSYSTEM_ONLY, NULL, &sa);
    check("non-system only: Handle, no __CLASS", hr == S_OK && names_have(sa, L"Handle", NULL) && !names_have(sa, L"__CLASS", NULL));
    SafeArrayDestroy(sa);
    hr = IWbemClassObject_GetNames(proc, NULL, WBEM_FLAG_LOCAL_ONLY, NULL, &sa);
    check("local only keeps the properties", hr == S_OK && names_have(sa, L"Handle", NULL) && !names_have(sa, L"__CLASS", NULL));
    SafeArrayDestroy(sa);
    hr = IWbemClassObject_GetNames(proc, NULL, WBEM_FLAG_PROPAGATED_ONLY, NULL, &sa);
    check("propagated only: none of ours", hr == S_OK && (names_have(sa, L"Handle", &count), count == 0));
    SafeArrayDestroy(sa);
    hr = IWbemClassObject_GetNames(proc, NULL, WBEM_FLAG_KEYS_ONLY, NULL, &sa);
    check("keys only: Handle alone", hr == S_OK && names_have(sa, L"Handle", &count) && count == 1);
    SafeArrayDestroy(sa);
    V_VT(&v) = VT_BOOL; V_BOOL(&v) = VARIANT_TRUE;
    hr = IWbemClassObject_GetNames(proc, L"key", WBEM_FLAG_ONLY_IF_TRUE | WBEM_FLAG_NONSYSTEM_ONLY, &v, &sa);
    check("the properties with [key] true: Handle alone", hr == S_OK && names_have(sa, L"Handle", &count) && count == 1);
    SafeArrayDestroy(sa);
    hr = IWbemClassObject_GetNames(proc, L"key", WBEM_FLAG_ONLY_IF_FALSE | WBEM_FLAG_NONSYSTEM_ONLY, &v, &sa);
    check("the properties with [key] false: none", hr == S_OK && (names_have(sa, L"Handle", &count), count == 0));
    SafeArrayDestroy(sa);
    hr = IWbemClassObject_GetNames(proc, NULL, 0x8000, NULL, &sa);
    checkhr("a flag that means nothing", hr, WBEM_E_INVALID_PARAMETER);

    hr = IWbemClassObject_GetPropertyOrigin(proc, L"__CLASS", &origin);
    check("origin of __CLASS", hr == S_OK && !wcscmp(origin, L"___SYSTEM"));
    SysFreeString(origin);
    hr = IWbemClassObject_GetPropertyOrigin(proc, L"Handle", &origin);
    check("origin of Handle", hr == S_OK && !wcscmp(origin, L"Win32_Process"));
    SysFreeString(origin);
    checkhr("origin of nothing", IWbemClassObject_GetPropertyOrigin(proc, L"Bogus", &origin), WBEM_E_NOT_FOUND);
    hr = IWbemClassObject_GetMethodOrigin(proc, L"Create", &origin);
    check("origin of Create", hr == S_OK && !wcscmp(origin, L"Win32_Process"));
    SysFreeString(origin);
    checkhr("method origin of a property", IWbemClassObject_GetMethodOrigin(proc, L"Handle", &origin), WBEM_E_NOT_FOUND);

    puts("   clone, compare, delete");
    hr = IWbemClassObject_Clone(proc, &obj2);
    checkhr("Clone", hr, S_OK);
    if (hr == S_OK)
    {
        checkhr("a clone is the same", IWbemClassObject_CompareTo(proc, 0, obj2), WBEM_S_SAME);
        IWbemClassObject_Release(obj2);
    }
    if (svcobj)
    {
        checkhr("another class is different", IWbemClassObject_CompareTo(proc, 0, svcobj), WBEM_S_DIFFERENT);
        checkhr("CompareTo(NULL)", IWbemClassObject_CompareTo(proc, 0, NULL), WBEM_E_INVALID_PARAMETER);
    }
    checkhr("Delete of a system property", IWbemClassObject_Delete(proc, L"__CLASS"), WBEM_E_INVALID_OPERATION);
    checkhr("Delete of a property", IWbemClassObject_Delete(proc, L"Handle"), WBEM_E_INVALID_OPERATION);
    checkhr("Delete of nothing", IWbemClassObject_Delete(proc, L"Bogus"), WBEM_E_NOT_FOUND);
    checkhr("DeleteMethod of nothing", IWbemClassObject_DeleteMethod(proc, L"Bogus"), WBEM_E_NOT_FOUND);
    checkhr("DeleteMethod of Create", IWbemClassObject_DeleteMethod(proc, L"Create"), WBEM_E_INVALID_OPERATION);
    IWbemClassObject_Release(proc);
    if (svcobj) IWbemClassObject_Release(svcobj);
    IWbemServices_Release(svc);
}

static void test_qualifiers(void)
{
    IWbemServices *svc = connect_ns(L"ROOT\\CIMV2");
    IWbemClassObject *proc = first(svc, L"SELECT * FROM Win32_Process");
    IWbemQualifierSet *q;
    VARIANT v;
    SAFEARRAY *sa;
    BSTR name;
    LONG flavor, count;
    HRESULT hr;

    puts("   qualifier sets");
    if (!proc) { check("a process", 0); return; }
    IWbemClassObject_GetPropertyQualifierSet(proc, L"Handle", &q);
    hr = IWbemQualifierSet_Get(q, L"key", 0, &v, &flavor);
    check("Handle is a [key]", hr == S_OK && V_VT(&v) == VT_BOOL && V_BOOL(&v));
    VariantClear(&v);
    hr = IWbemQualifierSet_Get(q, L"CIMTYPE", 0, &v, NULL);
    check("Handle's CIMTYPE is string", hr == S_OK && V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), L"string"));
    VariantClear(&v);
    checkhr("a qualifier that is not there", IWbemQualifierSet_Get(q, L"nothing", 0, &v, NULL), WBEM_E_NOT_FOUND);
    checkhr("Get with flags", IWbemQualifierSet_Get(q, L"key", 1, &v, NULL), WBEM_E_INVALID_PARAMETER);
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"hello");
    checkhr("Put a new one", IWbemQualifierSet_Put(q, L"Description", &v, 0), S_OK);
    VariantClear(&v);
    hr = IWbemQualifierSet_Get(q, L"description", 0, &v, &flavor);
    check("it reads back", hr == S_OK && V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), L"hello"));
    VariantClear(&v);
    V_VT(&v) = VT_BOOL; V_BOOL(&v) = VARIANT_FALSE;
    checkhr("a [key] cannot be overridden", IWbemQualifierSet_Put(q, L"key", &v, 0), WBEM_E_OVERRIDE_NOT_ALLOWED);
    checkhr("Put with the origin flavor", IWbemQualifierSet_Put(q, L"x", &v, WBEM_FLAVOR_ORIGIN_SYSTEM), WBEM_E_INVALID_PARAMETER);
    hr = IWbemQualifierSet_GetNames(q, 0, &sa);
    check("GetNames: key, CIMTYPE, Description", hr == S_OK && names_have(sa, L"key", &count) && names_have(sa, L"Description", NULL) && count == 3);
    SafeArrayDestroy(sa);
    checkhr("Next without BeginEnumeration", IWbemQualifierSet_Next(q, 0, &name, &v, &flavor), WBEM_E_UNEXPECTED);
    checkhr("BeginEnumeration", IWbemQualifierSet_BeginEnumeration(q, 0), S_OK);
    count = 0;
    while ((hr = IWbemQualifierSet_Next(q, 0, &name, &v, &flavor)) == S_OK) { count++; SysFreeString(name); VariantClear(&v); }
    check("three enumerated", count == 3);
    checkhr("then the end", hr, WBEM_S_NO_MORE_DATA);
    IWbemQualifierSet_EndEnumeration(q);
    checkhr("Delete", IWbemQualifierSet_Delete(q, L"Description"), S_OK);
    checkhr("Delete again", IWbemQualifierSet_Delete(q, L"Description"), WBEM_E_NOT_FOUND);
    IWbemQualifierSet_Release(q);
    IWbemClassObject_GetPropertyQualifierSet(proc, L"Handle", &q);
    hr = IWbemQualifierSet_Get(q, L"Description", 0, &v, NULL);
    check("a change stays with its set", hr == WBEM_E_NOT_FOUND);
    IWbemQualifierSet_Release(q);
    IWbemClassObject_GetMethodQualifierSet(proc, L"Create", &q);
    check("Create is [implemented]", IWbemQualifierSet_Get(q, L"implemented", 0, &v, NULL) == S_OK);
    IWbemQualifierSet_Release(q);
    IWbemClassObject_GetQualifierSet(proc, &q);
    hr = IWbemQualifierSet_Get(q, L"dynamic", 0, &v, NULL);
    check("the class is [dynamic]", hr == S_OK && V_VT(&v) == VT_BOOL && V_BOOL(&v));
    IWbemQualifierSet_Release(q);
    IWbemClassObject_Release(proc);
    IWbemServices_Release(svc);
}

static void test_async_and_wql(void)
{
    IWbemServices *svc = connect_ns(L"ROOT\\CIMV2");
    IEnumWbemClassObject *e;
    struct sink sink = { { &sink_vtbl }, 1, 0, 0xdead, NULL };
    IWbemClassObject *obj;
    VARIANT v;
    HRESULT hr;
    ULONG n;

    puts("   NextAsync, WQL escapes");
    sink.done = CreateEventW(NULL, TRUE, FALSE, NULL);
    e = query(svc, L"SELECT * FROM Win32_Process", &hr);
    checkhr("NextAsync(NULL)", IEnumWbemClassObject_NextAsync(e, 5, NULL), WBEM_E_INVALID_PARAMETER);
    hr = IEnumWbemClassObject_NextAsync(e, 1, &sink.iface);
    checkhr("NextAsync", hr, S_OK);
    check("the sink is told it is complete", WaitForSingleObject(sink.done, 20000) == WAIT_OBJECT_0 && sink.status == S_OK);
    check("one object was indicated", sink.indicated == 1);
    IEnumWbemClassObject_Release(e);
    CloseHandle(sink.done);

    e = query(svc, L"SELECT * FROM Win32_DiskDrive WHERE DeviceID='\\\\\\\\.\\\\PHYSICALDRIVE0'", &hr);
    checkhr("an escaped backslash query", hr, S_OK);
    obj = NULL; n = 0;
    if (e) IEnumWbemClassObject_Next(e, WBEM_INFINITE, 1, &obj, &n);
    check("finds the first disk", n == 1);
    if (n)
    {
        IWbemClassObject_Get(obj, L"DeviceID", 0, &v, NULL, NULL);
        check("DeviceID is \\\\.\\PHYSICALDRIVE0 (single backslashes)", V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), L"\\\\.\\PHYSICALDRIVE0"));
        VariantClear(&v);
        IWbemClassObject_Get(obj, L"__RELPATH", 0, &v, NULL, NULL);
        check("__RELPATH escapes them", V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), L"Win32_DiskDrive.DeviceID=\"\\\\\\\\.\\\\PHYSICALDRIVE0\""));
        if (V_VT(&v) == VT_BSTR) printf("         (got %ls)\n", V_BSTR(&v));
        VariantClear(&v);
        IWbemClassObject_Release(obj);
    }
    if (e) IEnumWbemClassObject_Release(e);
    e = query(svc, L"SELECT * FROM Win32_DiskDrive WHERE DeviceID LIKE 'PHYS\\%'", &hr);
    checkhr("a backslash before % is not an escape", hr, WBEM_E_INVALID_QUERY);
    e = query(svc, L"SELECT * FROM Win32_Directory WHERE Name='c:\\\\windows'", &hr);
    obj = NULL; n = 0;
    if (e) IEnumWbemClassObject_Next(e, WBEM_INFINITE, 1, &obj, &n);
    check("a directory by its escaped name", n == 1);
    if (n)
    {
        IWbemClassObject_Get(obj, L"Name", 0, &v, NULL, NULL);
        check("its Name has single backslashes", V_VT(&v) == VT_BSTR && !wcsicmp(V_BSTR(&v), L"c:\\windows"));
        VariantClear(&v);
        IWbemClassObject_Release(obj);
    }
    if (e) IEnumWbemClassObject_Release(e);
    IWbemServices_Release(svc);
}

static HRESULT exec_security(IWbemServices *ns, const WCHAR *method, SAFEARRAY *sd, IWbemClassObject **out)
{
    IWbemClassObject *cls = NULL, *sig = NULL, *in = NULL;
    BSTR cn = SysAllocString(L"__SystemSecurity"), mn = SysAllocString(method);
    HRESULT hr;

    IWbemServices_GetObject(ns, cn, 0, NULL, &cls, NULL);
    IWbemClassObject_GetMethod(cls, method, 0, &sig, NULL);
    if (sd)
    {
        IWbemClassObject_SpawnInstance(sig, 0, &in);
        VARIANT v;
        V_VT(&v) = VT_UI1 | VT_ARRAY;
        V_ARRAY(&v) = sd;
        IWbemClassObject_Put(in, L"SD", 0, &v, CIM_UINT8 | CIM_FLAG_ARRAY);
    }
    hr = IWbemServices_ExecMethod(ns, cn, mn, 0, NULL, in, out, NULL);
    if (in) IWbemClassObject_Release(in);
    if (sig) IWbemClassObject_Release(sig);
    IWbemClassObject_Release(cls);
    SysFreeString(cn);
    SysFreeString(mn);
    return hr;
}

static void test_security(void)
{
    IWbemServices *ns = connect_ns(L"ROOT\\CIMV2");
    IWbemClassObject *out = NULL;
    VARIANT v1, v2;
    SAFEARRAY *bad;
    HRESULT hr;
    void *d1, *d2;
    LONG n1 = 0, n2 = 0;

    puts("   __SystemSecurity SetSD / GetSD");
    hr = exec_security(ns, L"GetSD", NULL, &out);
    checkhr("GetSD", hr, S_OK);
    VariantInit(&v1);
    IWbemClassObject_Get(out, L"SD", 0, &v1, NULL, NULL);
    IWbemClassObject_Release(out);
    check("the default descriptor", V_VT(&v1) == (VT_UI1 | VT_ARRAY));
    hr = exec_security(ns, L"SetSD", V_ARRAY(&v1), &out);
    checkhr("SetSD with it", hr, S_OK);
    if (hr == S_OK) IWbemClassObject_Release(out);
    VariantInit(&v2);
    out = NULL;
    exec_security(ns, L"GetSD", NULL, &out);
    IWbemClassObject_Get(out, L"SD", 0, &v2, NULL, NULL);
    IWbemClassObject_Release(out);
    SafeArrayGetUBound(V_ARRAY(&v1), 1, &n1);
    SafeArrayGetUBound(V_ARRAY(&v2), 1, &n2);
    SafeArrayAccessData(V_ARRAY(&v1), &d1);
    SafeArrayAccessData(V_ARRAY(&v2), &d2);
    check("GetSD returns what SetSD stored", n1 == n2 && !memcmp(d1, d2, n1 + 1));
    SafeArrayUnaccessData(V_ARRAY(&v1));
    SafeArrayUnaccessData(V_ARRAY(&v2));
    VariantClear(&v2);
    bad = SafeArrayCreateVector(VT_UI1, 0, 40);
    out = NULL;
    checkhr("SetSD with rubbish", exec_security(ns, L"SetSD", bad, &out), WBEM_E_INVALID_PARAMETER);
    VariantClear(&v1);
    IWbemServices_Release(ns);
}

static void test_misc(void)
{
    IWbemLocator *loc;
    IWbemServices *svc = NULL;
    IWbemServices *ns;
    HRESULT hr;
    BSTR path;
    HMODULE mod = LoadLibraryW(L"wbemprox.dll");
    HRESULT (WINAPI *can_unload)(void) = (void *)GetProcAddress(mod, "DllCanUnloadNow");
    HRESULT (WINAPI *get_class)(REFCLSID, REFIID, void **) = (void *)GetProcAddress(mod, "DllGetClassObject");
    IClassFactory *cf;

    puts("   server lifetime, locator");
    check("wbemprox exports DllCanUnloadNow", can_unload != NULL);
    if (can_unload)
    {
        check("DllCanUnloadNow while an object lives", (ns = connect_ns(L"ROOT\\CIMV2"), can_unload() == S_FALSE));
        IWbemServices_Release(ns);
        get_class(&CLSID_WbemLocator, &IID_IClassFactory, (void **)&cf);
        IClassFactory_LockServer(cf, TRUE);
        checkhr("DllCanUnloadNow with a server lock", can_unload(), S_FALSE);
        IClassFactory_LockServer(cf, FALSE);
        IClassFactory_Release(cf);
    }
    CoCreateInstance(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (void **)&loc);
    path = SysAllocString(L"\\\\ROOT");
    hr = IWbemLocator_ConnectServer(loc, path, NULL, NULL, NULL, 0, NULL, NULL, &svc);
    checkhr("a machine called ROOT is not there", hr, HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE));
    SysFreeString(path);
    path = SysAllocString(L"\\\\no-such-machine\\ROOT\\CIMV2");
    hr = IWbemLocator_ConnectServer(loc, path, NULL, NULL, NULL, 0, NULL, NULL, &svc);
    checkhr("nor is another machine", hr, HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE));
    SysFreeString(path);
    IWbemLocator_Release(loc);

    ns = connect_ns(L"ROOT\\CIMV2");
    {
        IWbemClassObject *cls = NULL, *in = NULL, *out = NULL;
        BSTR cn = SysAllocString(L"Win32_Process"), mn = SysAllocString(L"Create");
        VARIANT v;
        CIMTYPE type = 0;

        IWbemServices_GetObject(ns, cn, 0, NULL, &cls, NULL);
        IWbemClassObject_GetMethod(cls, L"Create", 0, &in, NULL);
        V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"no-such-program-here.exe");
        IWbemClassObject_Put(in, L"CommandLine", 0, &v, 0);
        VariantClear(&v);
        hr = IWbemServices_ExecMethod(ns, cn, mn, 0, NULL, in, &out, NULL);
        VariantInit(&v);
        V_VT(&v) = VT_I4;
        IWbemClassObject_Get(out, L"ProcessId", 0, &v, &type, NULL);
        check("ProcessId of a failed Create is NULL", V_VT(&v) == VT_NULL && type == CIM_UINT32);
        IWbemClassObject_Release(out);
        IWbemClassObject_Release(in);
        IWbemClassObject_Release(cls);
        SysFreeString(cn);
        SysFreeString(mn);
    }
    IWbemServices_Release(ns);
    FreeLibrary(mod);
}

int main(void)
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    test_client_security();
    test_context();
    test_classes();
    test_qualifiers();
    test_async_and_wql();
    test_security();
    test_misc();
    CoUninitialize();
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
