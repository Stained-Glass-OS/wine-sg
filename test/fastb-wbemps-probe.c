/* wbemprox (patch 3680): Win32_ProcessStartup and the ProcessStartupInformation parameter of Win32_Process.Create. */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <wbemcli.h>
#include <stdio.h>
#include <string.h>

static int fails;
static void check(const char *name, int ok) { printf("%s  %s\n", ok ? "PASS" : "FAIL", name); fflush(stdout); if (!ok) fails++; }

static IWbemServices *ns;

static IWbemClassObject *get_class(const WCHAR *name)
{
    IWbemClassObject *obj = NULL;
    BSTR n = SysAllocString(name);
    if (IWbemServices_GetObject(ns, n, 0, NULL, &obj, NULL) != S_OK) obj = NULL;
    SysFreeString(n);
    return obj;
}

static HRESULT put_u32(IWbemClassObject *o, const WCHAR *name, DWORD v)
{
    VARIANT var;
    V_VT(&var) = VT_I4;
    V_I4(&var) = v;
    return IWbemClassObject_Put(o, name, 0, &var, 0);
}

static HRESULT put_str(IWbemClassObject *o, const WCHAR *name, const WCHAR *v)
{
    VARIANT var;
    HRESULT hr;
    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = SysAllocString(v);
    hr = IWbemClassObject_Put(o, name, 0, &var, 0);
    VariantClear(&var);
    return hr;
}

static int get_str(IWbemClassObject *o, const WCHAR *name, WCHAR *out, int len)
{
    VARIANT v;
    int ok = 0;
    VariantInit(&v);
    out[0] = 0;
    if (IWbemClassObject_Get(o, name, 0, &v, NULL, NULL) == S_OK && V_VT(&v) == VT_BSTR) { wcsncpy(out, V_BSTR(&v), len - 1); out[len - 1] = 0; ok = 1; }
    VariantClear(&v);
    return ok;
}

static int child(const char *path)
{
    STARTUPINFOW si = {sizeof(si)};
    FILE *f = fopen(path, "w");
    char env[64] = "";
    UINT old;

    GetStartupInfoW(&si);
    GetEnvironmentVariableA("SGFOO", env, sizeof(env));
    fprintf(f, "title=%ls\n", si.lpTitle ? si.lpTitle : L"");
    fprintf(f, "flags=%lu show=%u x=%lu y=%lu xs=%lu ys=%lu xc=%lu yc=%lu fill=%lu\n", (unsigned long)si.dwFlags, si.wShowWindow,
            si.dwX, si.dwY, si.dwXSize, si.dwYSize, si.dwXCountChars, si.dwYCountChars, si.dwFillAttribute);
    fprintf(f, "env=%s\n", env);
    fprintf(f, "prio=%lu\n", (unsigned long)GetPriorityClass(GetCurrentProcess()));
    old = SetErrorMode(0);
    fprintf(f, "errmode=%u\n", old);
    fprintf(f, "done\n");
    fclose(f);
    return 0;
}

static int read_child(const char *path, char *buf, int len)
{
    int i;
    for (i = 0; i < 200; ++i)
    {
        FILE *f = fopen(path, "r");
        if (f)
        {
            size_t n = fread(buf, 1, len - 1, f);
            fclose(f);
            buf[n] = 0;
            if (strstr(buf, "done")) return 1;
        }
        Sleep(100);
    }
    return 0;
}

int main(int argc, char **argv)
{
    IWbemLocator *loc;
    IWbemClassObject *startup, *startup2, *proc_class, *in_sig, *in, *out, *back;
    VARIANT v;
    CIMTYPE type;
    HRESULT hr;
    WCHAR text[256], cmd[MAX_PATH * 2], exe[MAX_PATH];
    char buf[1024], outpath[MAX_PATH];
    SAFEARRAY *sa;
    BSTR str;
    LONG idx;
    IWbemClassObject *pnames = NULL;

    if (argc > 2 && !strcmp(argv[1], "child")) return child(argv[2]);

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (CoCreateInstance(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (void **)&loc) != S_OK)
    { check("locator", 0); return 1; }
    str = SysAllocString(L"ROOT\\CIMV2");
    hr = IWbemLocator_ConnectServer(loc, str, NULL, NULL, NULL, 0, NULL, NULL, &ns);
    SysFreeString(str);
    if (hr != S_OK) { check("connect", 0); return 1; }

    startup = get_class(L"Win32_ProcessStartup");
    check("Win32_ProcessStartup class exists", startup != NULL);
    if (!startup) return 1;
    check("class name", get_str(startup, L"__CLASS", text, 256) && !wcscmp(text, L"Win32_ProcessStartup"));
    VariantInit(&v);
    hr = IWbemClassObject_Get(startup, L"ShowWindow", 0, &v, &type, NULL);
    check("ShowWindow is a uint16 property", hr == S_OK && type == CIM_UINT16);
    hr = IWbemClassObject_Get(startup, L"EnvironmentVariables", 0, &v, &type, NULL);
    check("EnvironmentVariables is a string array", hr == S_OK && type == (CIM_STRING | CIM_FLAG_ARRAY));
    hr = IWbemClassObject_Get(startup, L"Title", 0, &v, &type, NULL);
    check("Title is a string", hr == S_OK && type == CIM_STRING);
    hr = IWbemClassObject_GetNames(startup, NULL, WBEM_FLAG_NONSYSTEM_ONLY, NULL, &sa);
    check("14 properties", hr == S_OK && sa && SafeArrayGetUBound(sa, 1, &idx) == S_OK && idx == 13);
    if (sa) SafeArrayDestroy(sa);

    hr = IWbemClassObject_SpawnInstance(startup, 0, &startup2);
    check("SpawnInstance", hr == S_OK && startup2);
    if (!startup2) return 1;
    check("put ShowWindow", put_u32(startup2, L"ShowWindow", 3) == S_OK);
    check("put Title ok", put_str(startup2, L"Title", L"SG probe title") == S_OK);
    check("put X, Y, XSize, YSize", put_u32(startup2, L"X", 11) == S_OK && put_u32(startup2, L"Y", 22) == S_OK
            && put_u32(startup2, L"XSize", 333) == S_OK && put_u32(startup2, L"YSize", 444) == S_OK);
    check("put PriorityClass idle", put_u32(startup2, L"PriorityClass", IDLE_PRIORITY_CLASS) == S_OK);
    check("a string into a number is refused", put_str(startup2, L"X", L"abc") != S_OK);
    {
        SAFEARRAYBOUND b = {2, 0};
        BSTR e0 = SysAllocString(L"SGFOO=bar baz"), e1 = SysAllocString(L"SGOTHER=2");

        sa = SafeArrayCreate(VT_BSTR, 1, &b);
        idx = 0; SafeArrayPutElement(sa, &idx, e0);
        idx = 1; SafeArrayPutElement(sa, &idx, e1);
        V_VT(&v) = VT_BSTR | VT_ARRAY;
        V_ARRAY(&v) = sa;
        check("put EnvironmentVariables", IWbemClassObject_Put(startup2, L"EnvironmentVariables", 0, &v, 0) == S_OK);
        VariantClear(&v);
        SysFreeString(e0); SysFreeString(e1);
    }

    /* the in-parameter signature carries the object */
    proc_class = get_class(L"Win32_Process");
    check("Win32_Process class", proc_class != NULL);
    in_sig = NULL;
    hr = IWbemClassObject_GetMethod(proc_class, L"Create", 0, &in_sig, NULL);
    check("Create has an in signature", hr == S_OK && in_sig);
    VariantInit(&v);
    hr = IWbemClassObject_Get(in_sig, L"ProcessStartupInformation", 0, &v, &type, NULL);
    check("ProcessStartupInformation is an object parameter", hr == S_OK && type == CIM_OBJECT);
    VariantClear(&v);
    in = NULL;
    hr = IWbemClassObject_SpawnInstance(in_sig, 0, &in);
    check("SpawnInstance of the in signature", hr == S_OK && in);
    VariantInit(&v);
    hr = IWbemClassObject_Get(in, L"ProcessStartupInformation", 0, &v, NULL, NULL);
    check("an unset object parameter is NULL", hr == S_OK && V_VT(&v) == VT_NULL);

    V_VT(&v) = VT_BSTR;
    V_BSTR(&v) = SysAllocString(L"nonsense");
    check("a string is not an object", IWbemClassObject_Put(in, L"ProcessStartupInformation", 0, &v, 0) == WBEM_E_TYPE_MISMATCH);
    VariantClear(&v);
    V_VT(&v) = VT_UNKNOWN;
    V_UNKNOWN(&v) = (IUnknown *)startup2;
    check("put the startup object", IWbemClassObject_Put(in, L"ProcessStartupInformation", 0, &v, 0) == S_OK);
    /* the in parameters hold a copy */
    put_str(startup2, L"Title", L"changed afterwards");
    VariantInit(&v);
    hr = IWbemClassObject_Get(in, L"ProcessStartupInformation", 0, &v, &type, NULL);
    check("get returns an object", hr == S_OK && V_VT(&v) == VT_UNKNOWN && V_UNKNOWN(&v));
    back = NULL;
    if (V_VT(&v) == VT_UNKNOWN && V_UNKNOWN(&v)) IUnknown_QueryInterface(V_UNKNOWN(&v), &IID_IWbemClassObject, (void **)&back);
    check("which is a class object", back != NULL);
    check("the stored object is a copy", back && get_str(back, L"Title", text, 256) && !wcscmp(text, L"SG probe title"));
    if (back) IWbemClassObject_Release(back);
    VariantClear(&v);

    /* run the child with the startup information */
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    snprintf(outpath, sizeof(outpath), "C:\\sg-wbemps-%lu.txt", (unsigned long)GetCurrentProcessId());
    DeleteFileA(outpath);
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\" child %hs", exe, outpath);
    check("put CommandLine", put_str(in, L"CommandLine", cmd) == S_OK);
    {
        BSTR path = SysAllocString(L"Win32_Process"), method = SysAllocString(L"Create");
        out = NULL;
        hr = IWbemServices_ExecMethod(ns, path, method, 0, NULL, in, &out, NULL);
        SysFreeString(path); SysFreeString(method);
    }
    check("ExecMethod Create", hr == S_OK && out);
    VariantInit(&v);
    check("ReturnValue 0", out && IWbemClassObject_Get(out, L"ReturnValue", 0, &v, NULL, NULL) == S_OK && V_I4(&v) == 0);
    VariantInit(&v);
    check("ProcessId", out && IWbemClassObject_Get(out, L"ProcessId", 0, &v, NULL, NULL) == S_OK && V_I4(&v) != 0);
    check("the child ran", read_child(outpath, buf, sizeof(buf)));
    printf("%s", buf);
    check("child title", strstr(buf, "title=SG probe title\n") != NULL);
    check("child window position and size", strstr(buf, "x=11 y=22 xs=333 ys=444") != NULL);
    check("child show window", strstr(buf, "show=3") != NULL);
    check("child flags (USESHOWWINDOW|USEPOSITION|USESIZE = 0x1|0x4|0x2)", strstr(buf, "flags=7 ") != NULL);
    check("child environment", strstr(buf, "env=bar baz\n") != NULL);
    DeleteFileA(outpath);

    /* without startup information nothing of that is set */
    if (out) IWbemClassObject_Release(out);
    IWbemClassObject_Release(in);
    in = NULL;
    IWbemClassObject_SpawnInstance(in_sig, 0, &in);
    snprintf(outpath, sizeof(outpath), "C:\\sg-wbemps2-%lu.txt", (unsigned long)GetCurrentProcessId());
    DeleteFileA(outpath);
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\" child %hs", exe, outpath);
    put_str(in, L"CommandLine", cmd);
    {
        BSTR path = SysAllocString(L"Win32_Process"), method = SysAllocString(L"Create");
        out = NULL;
        hr = IWbemServices_ExecMethod(ns, path, method, 0, NULL, in, &out, NULL);
        SysFreeString(path); SysFreeString(method);
    }
    check("Create without startup information", hr == S_OK && read_child(outpath, buf, sizeof(buf)));
    check("child default priority and no title override", strstr(buf, "prio=32\n") != NULL && strstr(buf, "env=\n") != NULL && strstr(buf, "flags=0 ") != NULL);
    DeleteFileA(outpath);

    /* CreateFlags reach CreateProcess: a suspended child never runs */
    {
        IWbemClassObject *st3 = NULL, *in3 = NULL, *out3 = NULL;
        BSTR path = SysAllocString(L"Win32_Process"), method = SysAllocString(L"Create");
        DWORD pid = 0;
        HANDLE h;

        IWbemClassObject_SpawnInstance(startup, 0, &st3);
        put_u32(st3, L"CreateFlags", CREATE_SUSPENDED);
        IWbemClassObject_SpawnInstance(in_sig, 0, &in3);
        snprintf(outpath, sizeof(outpath), "C:\\sg-wbemps3-%lu.txt", (unsigned long)GetCurrentProcessId());
        DeleteFileA(outpath);
        swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\" child %hs", exe, outpath);
        put_str(in3, L"CommandLine", cmd);
        VariantInit(&v);
        V_VT(&v) = VT_UNKNOWN;
        V_UNKNOWN(&v) = (IUnknown *)st3;
        IWbemClassObject_Put(in3, L"ProcessStartupInformation", 0, &v, 0);
        hr = IWbemServices_ExecMethod(ns, path, method, 0, NULL, in3, &out3, NULL);
        VariantInit(&v);
        if (out3 && IWbemClassObject_Get(out3, L"ProcessId", 0, &v, NULL, NULL) == S_OK) pid = V_I4(&v);
        check("Create with CREATE_SUSPENDED", hr == S_OK && pid);
        Sleep(1500);
        check("the suspended child has not run", GetFileAttributesA(outpath) == INVALID_FILE_ATTRIBUTES);
        if (pid && (h = OpenProcess(PROCESS_TERMINATE, FALSE, pid))) { TerminateProcess(h, 0); CloseHandle(h); }
        SysFreeString(path); SysFreeString(method);
        if (out3) IWbemClassObject_Release(out3);
        if (in3) IWbemClassObject_Release(in3);
        if (st3) IWbemClassObject_Release(st3);
    }

    /* a bad object is refused */
    {
        VARIANT bad;
        IWbemClassObject_Release(in);
        in = NULL;
        IWbemClassObject_SpawnInstance(in_sig, 0, &in);
        V_VT(&bad) = VT_NULL;
        check("NULL clears the parameter", IWbemClassObject_Put(in, L"ProcessStartupInformation", 0, &bad, 0) == S_OK);
    }
    (void)pnames;
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
