/* uiautomationcore + oleacc (patch 3583): the Dynamic Annotation provider. A value set through IAccPropServices
 * for a window's client object shows up in UI Automation ahead of the MSAA proxy's own value; an element whose
 * IAccessible has no IAccIdentity gets no annotation provider. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <oleacc.h>
#include <uiautomation.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>

static int failures;

static void check(int ok, const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s  %s\n", ok ? "PASS" : "FAIL", buf);
    if (!ok) failures++;
}

static const GUID acc_name = { 0x608d3df8, 0x8128, 0x4aa7, { 0xa4, 0x28, 0xf5, 0x5e, 0x49, 0x26, 0x72, 0x91 } };
static const GUID acc_help = { 0xc831e11f, 0x44db, 0x4a99, { 0x97, 0x68, 0xcb, 0x8f, 0x97, 0x8b, 0x72, 0x31 } };
static const GUID acc_role = { 0xcb905ff2, 0x7bd1, 0x4c05, { 0xb3, 0xc8, 0xe6, 0xc2, 0x41, 0x36, 0x4d, 0x70 } };

/* a property server answering the help text for every identity */
static LONG server_calls;
static HRESULT WINAPI srv_QI(IAccPropServer *iface, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IAccPropServer)) { *ppv = iface; return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI srv_AddRef(IAccPropServer *iface) { return 2; }
static ULONG WINAPI srv_Release(IAccPropServer *iface) { return 1; }
static HRESULT WINAPI srv_GetPropValue(IAccPropServer *iface, const BYTE *id, DWORD len, MSAAPROPID prop,
        VARIANT *value, BOOL *has)
{
    server_calls++;
    VariantInit(value);
    *has = FALSE;
    if (IsEqualGUID(&prop, &acc_help))
    {
        V_VT(value) = VT_BSTR;
        V_BSTR(value) = SysAllocString(L"served help");
        *has = TRUE;
    }
    return S_OK;
}
static IAccPropServerVtbl srv_vtbl = { srv_QI, srv_AddRef, srv_Release, srv_GetPropValue };
static IAccPropServer server = { &srv_vtbl };

static BSTR node_string(HUIANODE node, PROPERTYID id)
{
    VARIANT v;
    BSTR ret = NULL;

    VariantInit(&v);
    if (SUCCEEDED(UiaGetPropertyValue(node, id, &v)) && V_VT(&v) == VT_BSTR)
    {
        ret = V_BSTR(&v);
        V_BSTR(&v) = NULL;
    }
    VariantClear(&v);
    return ret;
}

static BOOL has_provider(HUIANODE node, const WCHAR *type)
{
    BSTR desc = node_string(node, UIA_ProviderDescriptionPropertyId);
    BOOL ret = desc && wcsstr(desc, type);

    SysFreeString(desc);
    return ret;
}

static int node_int(HUIANODE node, PROPERTYID id)
{
    VARIANT v;
    int ret = -1;

    VariantInit(&v);
    if (SUCCEEDED(UiaGetPropertyValue(node, id, &v)) && V_VT(&v) == VT_I4)
        ret = V_I4(&v);
    VariantClear(&v);
    return ret;
}

int main(void)
{
    IAccPropServices *svc;
    HUIANODE node;
    HWND hwnd;
    VARIANT var;
    BSTR str;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    hr = CoCreateInstance(&CLSID_AccPropServices, NULL, CLSCTX_INPROC_SERVER, &IID_IAccPropServices, (void **)&svc);
    check(hr == S_OK, "CAccPropServices created (%#lx)", hr);
    if (FAILED(hr)) goto done;

    hwnd = CreateWindowA("BUTTON", "original", WS_VISIBLE | WS_POPUP | BS_PUSHBUTTON, 0, 0, 100, 40,
            NULL, NULL, NULL, NULL);
    check(!!hwnd, "window created");

    /* nothing annotated: the proxy answers, and an annotation provider is there for the oleacc client object */
    hr = UiaNodeFromHandle(hwnd, &node);
    check(hr == S_OK, "UiaNodeFromHandle (%#lx)", hr);
    check(has_provider(node, L"Annotation:"), "annotation provider present for the window's client object");
    str = node_string(node, UIA_NamePropertyId);
    check(str && !wcscmp(str, L"original"), "name before annotation: %ls", str ? str : L"(none)");
    SysFreeString(str);
    UiaNodeRelease(node);

    /* a fixed value for the name, annotated on the window's client object */
    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = SysAllocString(L"annotated name");
    hr = IAccPropServices_SetHwndProp(svc, hwnd, OBJID_CLIENT, CHILDID_SELF, acc_name, var);
    check(hr == S_OK, "SetHwndProp (%#lx)", hr);
    VariantClear(&var);

    hr = UiaNodeFromHandle(hwnd, &node);
    check(hr == S_OK, "UiaNodeFromHandle after annotation (%#lx)", hr);
    str = node_string(node, UIA_NamePropertyId);
    check(str && !wcscmp(str, L"annotated name"), "annotated name wins: %ls", str ? str : L"(none)");
    SysFreeString(str);

    /* a server answers the help text; properties it does not have fall through */
    {
        MSAAPROPID props[1] = { acc_help };
        hr = IAccPropServices_SetHwndPropServer(svc, hwnd, OBJID_CLIENT, CHILDID_SELF, props, 1, &server,
                ANNO_THIS);
        check(hr == S_OK, "SetHwndPropServer (%#lx)", hr);
    }
    str = node_string(node, UIA_HelpTextPropertyId);
    check(str && !wcscmp(str, L"served help") && server_calls > 0, "server value: %ls (%ld calls)",
            str ? str : L"(none)", server_calls);
    SysFreeString(str);
    str = node_string(node, UIA_FullDescriptionPropertyId);
    check(!str || !wcscmp(str, L""), "unserved property falls through");
    SysFreeString(str);

    /* an annotated MSAA role also decides the control type */
    V_VT(&var) = VT_I4;
    V_I4(&var) = ROLE_SYSTEM_DOCUMENT;
    hr = IAccPropServices_SetHwndProp(svc, hwnd, OBJID_CLIENT, CHILDID_SELF, acc_role, var);
    check(hr == S_OK, "SetHwndProp role (%#lx)", hr);
    check(node_int(node, UIA_ControlTypePropertyId) == UIA_DocumentControlTypeId, "control type %d from annotated role",
            node_int(node, UIA_ControlTypePropertyId));
    UiaNodeRelease(node);

    /* cleared: back to the proxy's own values */
    {
        MSAAPROPID props[3] = { acc_name, acc_help, acc_role };
        hr = IAccPropServices_ClearHwndProps(svc, hwnd, OBJID_CLIENT, CHILDID_SELF, props, 3);
        check(hr == S_OK, "ClearHwndProps (%#lx)", hr);
    }
    hr = UiaNodeFromHandle(hwnd, &node);
    str = node_string(node, UIA_NamePropertyId);
    check(str && !wcscmp(str, L"original"), "name after clearing: %ls", str ? str : L"(none)");
    SysFreeString(str);
    check(node_int(node, UIA_ControlTypePropertyId) == UIA_ButtonControlTypeId, "control type %d after clearing",
            node_int(node, UIA_ControlTypePropertyId));
    UiaNodeRelease(node);

    IAccPropServices_Release(svc);
    DestroyWindow(hwnd);

done:
    CoUninitialize();
    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
