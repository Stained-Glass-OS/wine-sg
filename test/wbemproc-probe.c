/* Probe for patches/sg/2460: Win32_Process and Win32_Service properties and methods. */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <wbemcli.h>
#include <sddl.h>
#include <stdio.h>
#include <string.h>

static int fails;
static void check(const char *name, int ok) { printf("      %s  %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }
static void checku(const char *name, unsigned got, unsigned want)
{
    printf("      %s  %s (%u, want %u)\n", got == want ? "PASS" : "FAIL", name, got, want);
    if (got != want) fails++;
}

static IWbemServices *ns;

static IWbemClassObject *query_one(const WCHAR *q)
{
    IEnumWbemClassObject *e = NULL;
    IWbemClassObject *obj = NULL;
    ULONG n = 0;
    BSTR wql = SysAllocString(L"WQL"), str = SysAllocString(q);
    if (IWbemServices_ExecQuery(ns, wql, str, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &e) == S_OK)
    {
        IEnumWbemClassObject_Next(e, WBEM_INFINITE, 1, &obj, &n);
        IEnumWbemClassObject_Release(e);
    }
    SysFreeString(wql);
    SysFreeString(str);
    return n ? obj : NULL;
}

/* a property as text ("" when it is NULL or missing) */
static void prop_text(IWbemClassObject *obj, const WCHAR *name, WCHAR *out, int len, VARTYPE *vt)
{
    VARIANT v;
    VariantInit(&v);
    out[0] = 0;
    if (IWbemClassObject_Get(obj, name, 0, &v, NULL, NULL) != S_OK) { if (vt) *vt = 0xffff; return; }
    if (vt) *vt = V_VT(&v);
    if (V_VT(&v) == VT_BSTR) wcsncpy(out, V_BSTR(&v), len - 1), out[len - 1] = 0;
    else if (V_VT(&v) == VT_I4) swprintf(out, len, L"%ld", V_I4(&v));
    else if (V_VT(&v) == VT_BOOL) wcscpy(out, V_BOOL(&v) ? L"TRUE" : L"FALSE");
    VariantClear(&v);
}

/* run a method: the in parameters are pairs name, variant; returns ReturnValue (or -1 when the call itself failed) */
static IWbemClassObject *last_out;
static int call(const WCHAR *path, const WCHAR *class, const WCHAR *method, int nparams, const WCHAR **names, VARIANT *vals)
{
    IWbemClassObject *cls = NULL, *sig = NULL, *in = NULL, *out = NULL;
    BSTR cp = SysAllocString(class), pp = SysAllocString(path), mn = SysAllocString(method);
    VARIANT v;
    HRESULT hr;
    int i, ret = -1;

    if (last_out) { IWbemClassObject_Release(last_out); last_out = NULL; }
    IWbemServices_GetObject(ns, cp, 0, NULL, &cls, NULL);
    IWbemClassObject_GetMethod(cls, method, 0, &sig, NULL);
    if (sig && nparams)
    {
        IWbemClassObject_SpawnInstance(sig, 0, &in);
        for (i = 0; i < nparams; i++) IWbemClassObject_Put(in, names[i], 0, &vals[i], 0);
    }
    hr = IWbemServices_ExecMethod(ns, pp, mn, 0, NULL, in, &out, NULL);
    if (hr == S_OK && out)
    {
        VariantInit(&v);
        if (IWbemClassObject_Get(out, L"ReturnValue", 0, &v, NULL, NULL) == S_OK && V_VT(&v) == VT_I4) ret = V_I4(&v);
        last_out = out;
    }
    else printf("         (ExecMethod %ls: %#lx)\n", method, (unsigned long)hr);
    if (in) IWbemClassObject_Release(in);
    if (sig) IWbemClassObject_Release(sig);
    if (cls) IWbemClassObject_Release(cls);
    SysFreeString(cp); SysFreeString(pp); SysFreeString(mn);
    return ret;
}

static VARIANT vi4(LONG x) { VARIANT v; V_VT(&v) = VT_I4; V_I4(&v) = x; return v; }
static VARIANT vbstr(const WCHAR *s) { VARIANT v; V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(s); return v; }
static VARIANT vbool(int b) { VARIANT v; V_VT(&v) = VT_BOOL; V_BOOL(&v) = b ? VARIANT_TRUE : VARIANT_FALSE; return v; }

static void test_process(const WCHAR *self)
{
    WCHAR q[200], path[100], text[600], computer[64], cmd[MAX_PATH + 40];
    IWbemClassObject *obj;
    PROCESS_INFORMATION pi;
    STARTUPINFOW si = { sizeof(si) };
    DWORD size = ARRAYSIZE(computer), code;
    VARTYPE vt;
    const WCHAR *n[2];
    VARIANT v[2];
    int r;

    puts("   Win32_Process");
    swprintf(q, 200, L"SELECT * FROM Win32_Process WHERE ProcessId=%lu", GetCurrentProcessId());
    obj = query_one(q);
    check("own process found", obj != NULL);
    if (obj)
    {
        GetComputerNameW(computer, &size);
        prop_text(obj, L"CommandLine", text, 600, NULL);
        check("CommandLine", wcsstr(text, L"probe-") != NULL);
        prop_text(obj, L"CreationDate", text, 600, &vt);
        check("CreationDate is a CIM date and time", wcslen(text) == 25 && text[14] == '.' && (text[21] == '+' || text[21] == '-') &&
              text[0] == '2');
        prop_text(obj, L"HandleCount", text, 600, NULL);
        check("HandleCount", _wtoi(text) > 3);
        prop_text(obj, L"Priority", text, 600, NULL);
        checku("Priority of a normal process", _wtoi(text), 8);
        prop_text(obj, L"SessionId", text, 600, &vt);
        check("SessionId", vt == VT_I4);
        prop_text(obj, L"WorkingSetSize", text, 600, &vt);
        check("WorkingSetSize is a string, in bytes", vt == VT_BSTR && _wtoi64(text) > 100000);
        prop_text(obj, L"VirtualSize", text, 600, &vt);
        check("VirtualSize", vt == VT_BSTR && _wtoi64(text) > 100000);
        prop_text(obj, L"UserModeTime", text, 600, &vt);
        check("UserModeTime is a string", vt == VT_BSTR);
        prop_text(obj, L"CSName", text, 600, NULL);
        check("CSName", !wcsicmp(text, computer));
        prop_text(obj, L"CreationClassName", text, 600, NULL);
        check("CreationClassName", !wcscmp(text, L"Win32_Process"));
        prop_text(obj, L"WindowsVersion", text, 600, NULL);
        check("WindowsVersion", wcsncmp(text, L"10.0.", 5) == 0 || wcschr(text, '.') != NULL);
        prop_text(obj, L"ProcessId", text, 600, NULL);
        checku("ProcessId", _wtoi(text), GetCurrentProcessId());
        prop_text(obj, L"ParentProcessId", text, 600, NULL);
        prop_text(obj, L"ParentProcessId", text, 600, &vt);
        check("ParentProcessId", vt == VT_I4);
        prop_text(obj, L"Status", text, 600, &vt);
        check("Status is NULL", vt == VT_NULL);
        IWbemClassObject_Release(obj);
    }

    /* another process, with its own command line */
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\" sleep", self);
    check("child started", CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi));
    if (!pi.hProcess) printf("         (CreateProcess %ls: %lu)\n", cmd, GetLastError());
    Sleep(500);
    swprintf(q, 200, L"SELECT * FROM Win32_Process WHERE ProcessId=%lu", pi.dwProcessId);
    obj = query_one(q);
    check("child found", obj != NULL);
    if (obj)
    {
        prop_text(obj, L"CommandLine", text, 600, NULL);
        check("the command line of another process", wcsstr(text, L"probe-") && wcsstr(text, L"sleep"));
        IWbemClassObject_Release(obj);
    }
    swprintf(path, 100, L"Win32_Process.Handle=\"%lu\"", pi.dwProcessId);
    r = call(path, L"Win32_Process", L"GetOwner", 0, NULL, NULL);
    checku("GetOwner", r, 0);
    if (last_out)
    {
        WCHAR user[100], me[100];
        DWORD len = 100;
        prop_text(last_out, L"User", user, 100, NULL);
        GetUserNameW(me, &len);
        check("the owner is the user that started it", !wcsicmp(user, me));
    }
    r = call(path, L"Win32_Process", L"GetOwnerSid", 0, NULL, NULL);
    checku("GetOwnerSid", r, 0);
    if (last_out)
    {
        WCHAR sid[200];
        HANDLE token;
        TOKEN_USER *tu;
        WCHAR *mine = NULL;
        DWORD len = 0;
        prop_text(last_out, L"Sid", sid, 200, NULL);
        OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
        GetTokenInformation(token, TokenUser, NULL, 0, &len);
        tu = malloc(len);
        GetTokenInformation(token, TokenUser, tu, len, &len);
        ConvertSidToStringSidW(tu->User.Sid, &mine);
        check("its SID", mine && !wcscmp(sid, mine) && sid[0] == 'S');
        LocalFree(mine); free(tu); CloseHandle(token);
    }
    n[0] = L"Priority";
    v[0] = vi4(64);
    checku("SetPriority(idle)", call(path, L"Win32_Process", L"SetPriority", 1, n, v), 0);
    checku("... it is idle", GetPriorityClass(pi.hProcess), IDLE_PRIORITY_CLASS);
    v[0] = vi4(7);
    checku("SetPriority(7) is not a priority", call(path, L"Win32_Process", L"SetPriority", 1, n, v), 21);
    swprintf(q, 200, L"SELECT * FROM Win32_Process WHERE ProcessId=%lu", pi.dwProcessId);
    obj = query_one(q);
    if (obj)
    {
        prop_text(obj, L"Priority", text, 600, NULL);
        checku("Priority of an idle process", _wtoi(text), 4);
        IWbemClassObject_Release(obj);
    }
    n[0] = L"Reason";
    v[0] = vi4(77);
    checku("Terminate", call(path, L"Win32_Process", L"Terminate", 1, n, v), 0);
    check("the child ended", WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
    GetExitCodeProcess(pi.hProcess, &code);
    checku("with the reason as its exit code", code, 77);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}

static void test_service(void)
{
    const WCHAR *n[12];
    VARIANT v[12];
    IWbemClassObject *obj;
    WCHAR text[300];
    VARTYPE vt;
    int r;

    puts("   Win32_Service");
    n[0] = L"Name"; v[0] = vbstr(L"SgProbeSvc");
    n[1] = L"DisplayName"; v[1] = vbstr(L"Stained Glass probe service");
    n[2] = L"PathName"; v[2] = vbstr(L"C:\\windows\\system32\\sgprobe.exe -x");
    n[3] = L"ServiceType"; v[3] = vi4(16);
    n[4] = L"ErrorControl"; v[4] = vi4(1);
    n[5] = L"StartMode"; v[5] = vbstr(L"Manual");
    n[6] = L"DesktopInteract"; v[6] = vbool(0);
    checku("Create", call(L"Win32_Service", L"Win32_Service", L"Create", 7, n, v), 0);
    checku("Create again", call(L"Win32_Service", L"Win32_Service", L"Create", 7, n, v), 23);
    v[3] = vi4(5);
    n[0] = L"Name"; v[0] = vbstr(L"SgProbeSvc2");
    checku("Create with a service type that is none", call(L"Win32_Service", L"Win32_Service", L"Create", 7, n, v), 21);

    obj = query_one(L"SELECT * FROM Win32_Service WHERE Name='SgProbeSvc'");
    check("it is there", obj != NULL);
    if (obj)
    {
        prop_text(obj, L"PathName", text, 300, NULL);
        check("PathName", !wcscmp(text, L"C:\\windows\\system32\\sgprobe.exe -x"));
        prop_text(obj, L"StartMode", text, 300, NULL);
        check("StartMode", !wcscmp(text, L"Manual"));
        prop_text(obj, L"ErrorControl", text, 300, NULL);
        check("ErrorControl", !wcscmp(text, L"Normal"));
        prop_text(obj, L"ServiceType", text, 300, NULL);
        check("ServiceType", !wcscmp(text, L"Own Process"));
        prop_text(obj, L"Caption", text, 300, NULL);
        check("Caption is the display name", !wcscmp(text, L"Stained Glass probe service"));
        prop_text(obj, L"Started", text, 300, &vt);
        check("Started is FALSE", vt == VT_BOOL && !wcscmp(text, L"FALSE"));
        prop_text(obj, L"State", text, 300, NULL);
        check("State", !wcscmp(text, L"Stopped"));
        prop_text(obj, L"Status", text, 300, NULL);
        check("Status", !wcscmp(text, L"OK"));
        prop_text(obj, L"DesktopInteract", text, 300, &vt);
        check("DesktopInteract is FALSE", vt == VT_BOOL && !wcscmp(text, L"FALSE"));
        prop_text(obj, L"Description", text, 300, &vt);
        check("Description is NULL", vt == VT_NULL);
        prop_text(obj, L"CreationClassName", text, 300, NULL);
        check("CreationClassName", !wcscmp(text, L"Win32_Service"));
        IWbemClassObject_Release(obj);
    }

    n[0] = L"StartMode"; v[0] = vbstr(L"Disabled");
    checku("ChangeStartMode(Disabled)", call(L"Win32_Service.Name=\"SgProbeSvc\"", L"Win32_Service", L"ChangeStartMode", 1, n, v), 0);
    v[0] = vbstr(L"Sometimes");
    checku("ChangeStartMode(Sometimes)", call(L"Win32_Service.Name=\"SgProbeSvc\"", L"Win32_Service", L"ChangeStartMode", 1, n, v), 21);
    obj = query_one(L"SELECT * FROM Win32_Service WHERE Name='SgProbeSvc'");
    if (obj)
    {
        prop_text(obj, L"StartMode", text, 300, NULL);
        check("it is Disabled", !wcscmp(text, L"Disabled"));
        IWbemClassObject_Release(obj);
    }
    n[0] = L"DisplayName"; v[0] = vbstr(L"Renamed probe service");
    n[1] = L"StartMode"; v[1] = vbstr(L"Automatic");
    checku("Change", call(L"Win32_Service.Name=\"SgProbeSvc\"", L"Win32_Service", L"Change", 2, n, v), 0);
    obj = query_one(L"SELECT * FROM Win32_Service WHERE Name='SgProbeSvc'");
    if (obj)
    {
        prop_text(obj, L"DisplayName", text, 300, NULL);
        check("the display name changed", !wcscmp(text, L"Renamed probe service"));
        prop_text(obj, L"StartMode", text, 300, NULL);
        check("Automatic reads back as Auto", !wcscmp(text, L"Auto"));
        prop_text(obj, L"PathName", text, 300, NULL);
        check("the path did not change", !wcscmp(text, L"C:\\windows\\system32\\sgprobe.exe -x"));
        IWbemClassObject_Release(obj);
    }
    checku("InterrogateService of a stopped service", call(L"Win32_Service.Name=\"SgProbeSvc\"", L"Win32_Service", L"InterrogateService", 0, NULL, NULL), 6);
    n[0] = L"ControlCode"; v[0] = vi4(100);
    checku("UserControlService(100)", call(L"Win32_Service.Name=\"SgProbeSvc\"", L"Win32_Service", L"UserControlService", 1, n, v), 21);
    v[0] = vi4(200);
    checku("UserControlService(200) of a stopped service", call(L"Win32_Service.Name=\"SgProbeSvc\"", L"Win32_Service", L"UserControlService", 1, n, v), 6);
    checku("StopService of a stopped service", call(L"Win32_Service.Name=\"SgProbeSvc\"", L"Win32_Service", L"StopService", 0, NULL, NULL), 6);
    checku("Delete", call(L"Win32_Service.Name=\"SgProbeSvc\"", L"Win32_Service", L"Delete", 0, NULL, NULL), 0);
    obj = query_one(L"SELECT * FROM Win32_Service WHERE Name='SgProbeSvc'");
    check("it is gone", obj == NULL);
    if (obj) IWbemClassObject_Release(obj);
}

int main(int argc, char **argv)
{
    WCHAR self[MAX_PATH];
    IWbemLocator *loc;
    BSTR path;

    if (argc > 1 && !strcmp(argv[1], "sleep")) { Sleep(60000); return 0; }
    GetModuleFileNameW(NULL, self, MAX_PATH);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    CoCreateInstance(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (void **)&loc);
    path = SysAllocString(L"ROOT\\CIMV2");
    IWbemLocator_ConnectServer(loc, path, NULL, NULL, NULL, 0, NULL, NULL, &ns);
    SysFreeString(path);
    check("connected", ns != NULL);
    if (ns)
    {
        test_process(self);
        test_service();
        IWbemServices_Release(ns);
    }
    IWbemLocator_Release(loc);
    CoUninitialize();
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
