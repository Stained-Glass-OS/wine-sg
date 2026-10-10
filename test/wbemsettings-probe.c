/* Probe for patches/sg/2462: a class with no instance is still a class (GetObject); IWbemClassObject::Next lists
 * the system properties and honours the flags. */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <wbemcli.h>
#include <stdio.h>
#include <string.h>

static int fails;
static void check(const char *name, int ok) { printf("      %s  %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }

static int count_props(IWbemClassObject *obj, LONG flags, int *first_system)
{
    BSTR name;
    int n = 0;

    *first_system = 0;
    if (IWbemClassObject_BeginEnumeration(obj, flags) != S_OK) return -1;
    while (IWbemClassObject_Next(obj, 0, &name, NULL, NULL, NULL) == S_OK)
    {
        if (!n && name[0] == '_' && name[1] == '_') *first_system = 1;
        n++;
        SysFreeString(name);
    }
    IWbemClassObject_EndEnumeration(obj);
    return n;
}

int main(void)
{
    IWbemLocator *loc;
    IWbemServices *ns = NULL;
    IWbemClassObject *cls = NULL, *proc = NULL;
    BSTR path, cn;
    VARIANT v;
    CIMTYPE type;
    HRESULT hr;
    int n, sys;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    CoCreateInstance(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (void **)&loc);
    path = SysAllocString(L"ROOT\\CIMV2");
    IWbemLocator_ConnectServer(loc, path, NULL, NULL, NULL, 0, NULL, NULL, &ns);
    SysFreeString(path);

    puts("   a class without instances");
    cn = SysAllocString(L"Win32_StartupCommand");
    hr = IWbemServices_GetObject(ns, cn, 0, NULL, &cls, NULL);
    SysFreeString(cn);
    check("GetObject finds the class", hr == S_OK && cls);
    if (cls)
    {
        VariantInit(&v);
        hr = IWbemClassObject_Get(cls, L"__CLASS", 0, &v, &type, NULL);
        check("__CLASS", hr == S_OK && V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), L"Win32_StartupCommand"));
        VariantClear(&v);
        hr = IWbemClassObject_Get(cls, L"__GENUS", 0, &v, &type, NULL);
        check("__GENUS is class (1)", hr == S_OK && V_I4(&v) == 1);
        hr = IWbemClassObject_Get(cls, L"__RELPATH", 0, &v, &type, NULL);
        check("__RELPATH is the class name", hr == S_OK && V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), L"Win32_StartupCommand"));
        VariantClear(&v);
        hr = IWbemClassObject_Get(cls, L"__PATH", 0, &v, &type, NULL);
        check("__PATH ends with :Win32_StartupCommand", hr == S_OK && V_VT(&v) == VT_BSTR && wcsstr(V_BSTR(&v), L":Win32_StartupCommand") != NULL);
        VariantClear(&v);
        hr = IWbemClassObject_Get(cls, L"Command", 0, &v, &type, NULL);
        check("Command has no value, a string type", hr == S_OK && V_VT(&v) == VT_NULL && type == CIM_STRING);
        n = count_props(cls, WBEM_FLAG_NONSYSTEM_ONLY, &sys);
        check("seven properties of its own", n == 7);
        IWbemClassObject_Release(cls);
    }

    puts("   property enumeration of an instance");
    {
        IEnumWbemClassObject *e = NULL;
        BSTR wql = SysAllocString(L"WQL"), q = SysAllocString(L"SELECT * FROM Win32_TimeZone");
        ULONG count = 0;

        IWbemServices_ExecQuery(ns, wql, q, WBEM_FLAG_FORWARD_ONLY, NULL, &e);
        if (e) IEnumWbemClassObject_Next(e, WBEM_INFINITE, 1, &proc, &count);
        check("a time zone instance", count == 1);
        SysFreeString(wql); SysFreeString(q);
        if (e) IEnumWbemClassObject_Release(e);
    }
    if (proc)
    {
        int all = count_props(proc, 0, &sys), user, sysonly;

        check("with no flags, the system properties come first", sys == 1);
        user = count_props(proc, WBEM_FLAG_NONSYSTEM_ONLY, &sys);
        check("non-system only: 24 properties, no system one first", user == 24 && !sys);
        sysonly = count_props(proc, WBEM_FLAG_SYSTEM_ONLY, &sys);
        check("system only: ten", sysonly == 10 && sys);
        check("together they are all", all == user + sysonly);
        check("flags that mean nothing", count_props(proc, 0x1000, &sys) == -1);
        IWbemClassObject_Release(proc);
    }
    IWbemServices_Release(ns);
    IWbemLocator_Release(loc);
    CoUninitialize();
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
