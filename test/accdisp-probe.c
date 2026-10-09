/* oleacc: the IAccessible objects of a window (client area, window, title
 * bar) are dual interfaces: GetTypeInfoCount/GetTypeInfo/GetIDsOfNames/Invoke
 * used to answer E_NOTIMPL.  Also LresultFromObject with a non-zero wParam
 * (the WM_GETOBJECT wParam) logged an "unsupported" FIXME; it must give a
 * result that ObjectFromLresult turns back into a working object. */
#define COBJMACROS
#include <windows.h>
#include <oleacc.h>
#include <stdio.h>

/* mingw has no usable IID_IAccessible in libuuid; it resolves to junk. */
static const GUID my_IID_IAccessible = {0x618736e0, 0x3c3d, 0x11cf, {0x81, 0x0c, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static void checkv(int ok, const char *what, unsigned long got)
{
    printf("%s  %s (got %#lx)\n", ok ? "PASS" : "FAIL", what, got);
    if (!ok) failures++;
}

static const struct { const WCHAR *name; DISPID id; } names[] =
{
    {L"accParent", -5000}, {L"accChildCount", -5001}, {L"accChild", -5002}, {L"accName", -5003},
    {L"accValue", -5004}, {L"accDescription", -5005}, {L"accRole", -5006}, {L"accState", -5007},
    {L"accHelp", -5008}, {L"accHelpTopic", -5009}, {L"accKeyboardShortcut", -5010},
    {L"accFocus", -5011}, {L"accSelection", -5012}, {L"accDefaultAction", -5013},
};

static void test_object(HWND hwnd, LONG objid, const char *label)
{
    IDispatch *disp = NULL;
    IAccessible *acc = NULL;
    ITypeInfo *ti = NULL;
    DISPPARAMS params = {0}, noargs = {0};
    VARIANT arg, res, direct;
    UINT count = 77;
    HRESULT hr;
    BSTR docname = NULL;
    char what[160];
    unsigned int i;
    DISPID id;
    LONG childcount = -1;
    WCHAR *bad = (WCHAR *)L"noSuchAccessibleMember";

    hr = AccessibleObjectFromWindow(hwnd, objid, &IID_IDispatch, (void **)&disp);
    sprintf(what, "%s: IDispatch obtained", label);
    checkv(hr == S_OK && disp, what, hr);
    if (!disp) return;
    IDispatch_QueryInterface(disp, &my_IID_IAccessible, (void **)&acc);

    hr = IDispatch_GetTypeInfoCount(disp, &count);
    sprintf(what, "%s: GetTypeInfoCount is 1", label);
    checkv(hr == S_OK && count == 1, what, count);
    hr = IDispatch_GetTypeInfoCount(disp, NULL);
    sprintf(what, "%s: GetTypeInfoCount(NULL) fails", label);
    checkv(FAILED(hr), what, hr);

    hr = IDispatch_GetTypeInfo(disp, 0, 0, &ti);
    sprintf(what, "%s: GetTypeInfo(0) gives a type info", label);
    checkv(hr == S_OK && ti, what, hr);
    if (ti)
    {
        hr = ITypeInfo_GetDocumentation(ti, MEMBERID_NIL, &docname, NULL, NULL, NULL);
        sprintf(what, "%s: the type info is IAccessible", label);
        check(hr == S_OK && docname && !wcscmp(docname, L"IAccessible"), what);
        SysFreeString(docname);
        ITypeInfo_Release(ti);
    }
    ti = (ITypeInfo *)(UINT_PTR)1;
    hr = IDispatch_GetTypeInfo(disp, 1, 0, &ti);
    sprintf(what, "%s: GetTypeInfo(1) is DISP_E_BADINDEX", label);
    check(hr == DISP_E_BADINDEX && !ti, what);

    for (i = 0; i < sizeof(names) / sizeof(*names); ++i)
    {
        WCHAR *n = (WCHAR *)names[i].name;
        id = 0;
        hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, &n, 1, 0, &id);
        sprintf(what, "%s: GetIDsOfNames(%ls) = %ld", label, names[i].name, (long)names[i].id);
        check(hr == S_OK && id == names[i].id, what);
    }
    hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, &bad, 1, 0, &id);
    sprintf(what, "%s: unknown member name is DISP_E_UNKNOWNNAME", label);
    checkv(hr == DISP_E_UNKNOWNNAME, what, hr);
    {
        WCHAR *n = (WCHAR *)L"accName";
        hr = IDispatch_GetIDsOfNames(disp, &IID_IUnknown, &n, 1, 0, &id);
        sprintf(what, "%s: GetIDsOfNames with a non-NULL riid is DISP_E_UNKNOWNINTERFACE", label);
        checkv(hr == DISP_E_UNKNOWNINTERFACE, what, hr);
    }

    /* accChildCount through Invoke equals the direct call */
    if (acc) IAccessible_get_accChildCount(acc, &childcount);
    VariantInit(&res);
    hr = IDispatch_Invoke(disp, -5001, &IID_NULL, 0, DISPATCH_PROPERTYGET, &noargs, &res, NULL, NULL);
    sprintf(what, "%s: Invoke(accChildCount) matches the direct call (%ld)", label, (long)childcount);
    check(hr == S_OK && V_VT(&res) == VT_I4 && V_I4(&res) == childcount, what);
    VariantClear(&res);

    /* accRole(CHILDID_SELF) */
    V_VT(&arg) = VT_I4; V_I4(&arg) = CHILDID_SELF;
    params.rgvarg = &arg; params.cArgs = 1;
    VariantInit(&res); VariantInit(&direct);
    if (acc) IAccessible_get_accRole(acc, arg, &direct);
    hr = IDispatch_Invoke(disp, -5006, &IID_NULL, 0, DISPATCH_PROPERTYGET, &params, &res, NULL, NULL);
    sprintf(what, "%s: Invoke(accRole) matches the direct call (vt %d, %ld)", label, V_VT(&direct),
            V_VT(&direct) == VT_I4 ? (long)V_I4(&direct) : -1L);
    check(hr == S_OK && V_VT(&res) == V_VT(&direct) && (V_VT(&res) != VT_I4 || V_I4(&res) == V_I4(&direct)), what);
    VariantClear(&res); VariantClear(&direct);

    /* accName(CHILDID_SELF) */
    VariantInit(&res); VariantInit(&direct);
    {
        BSTR dn = NULL;
        HRESULT dhr = acc ? IAccessible_get_accName(acc, arg, &dn) : E_FAIL;
        hr = IDispatch_Invoke(disp, -5003, &IID_NULL, 0, DISPATCH_PROPERTYGET, &params, &res, NULL, NULL);
        sprintf(what, "%s: Invoke(accName) agrees with the direct call (hr %#lx)", label, dhr);
        check(hr == dhr && (FAILED(dhr) || (V_VT(&res) == VT_BSTR && ((!dn && !V_BSTR(&res))
                || (dn && V_BSTR(&res) && !wcscmp(dn, V_BSTR(&res)))))), what);
        SysFreeString(dn);
    }
    VariantClear(&res);

    hr = IDispatch_Invoke(disp, -5001, &IID_IUnknown, 0, DISPATCH_PROPERTYGET, &noargs, &res, NULL, NULL);
    sprintf(what, "%s: Invoke with a non-NULL riid is DISP_E_UNKNOWNINTERFACE", label);
    checkv(hr == DISP_E_UNKNOWNINTERFACE, what, hr);
    hr = IDispatch_Invoke(disp, 0x7777, &IID_NULL, 0, DISPATCH_PROPERTYGET, &noargs, &res, NULL, NULL);
    sprintf(what, "%s: Invoke of an unknown dispid is DISP_E_MEMBERNOTFOUND", label);
    checkv(hr == DISP_E_MEMBERNOTFOUND, what, hr);

    if (acc) IAccessible_Release(acc);
    IDispatch_Release(disp);
}

static void test_lresult(HWND hwnd)
{
    static const WPARAM wparams[] = {0, 1, 0x1234};
    unsigned int i;

    for (i = 0; i < sizeof(wparams) / sizeof(*wparams); ++i)
    {
        IAccessible *acc = NULL, *out = NULL;
        LRESULT lres;
        HRESULT hr;
        char what[100];
        LONG count = -1, count2 = -2;

        hr = AccessibleObjectFromWindow(hwnd, OBJID_CLIENT, &my_IID_IAccessible, (void **)&acc);
        if (hr != S_OK) { check(0, "AccessibleObjectFromWindow(OBJID_CLIENT)"); return; }

        lres = LresultFromObject(&my_IID_IAccessible, wparams[i], (IUnknown *)acc);
        sprintf(what, "LresultFromObject(wParam %#Ix) gives a positive result", wparams[i]);
        check(lres > 0, what);
        if (lres > 0)
        {
            hr = ObjectFromLresult(lres, &my_IID_IAccessible, wparams[i], (void **)&out);
            sprintf(what, "ObjectFromLresult(wParam %#Ix) returns the object", wparams[i]);
            checkv(hr == S_OK && out, what, hr);
            if (out)
            {
                IAccessible_get_accChildCount(acc, &count);
                hr = IAccessible_get_accChildCount(out, &count2);
                sprintf(what, "object from wParam %#Ix works (childcount %ld)", wparams[i], (long)count);
                check(hr == S_OK && count == count2, what);
                IAccessible_Release(out);
            }
        }
        IAccessible_Release(acc);
    }
    check(LresultFromObject(&my_IID_IAccessible, 0, NULL) == E_INVALIDARG, "LresultFromObject(NULL) is E_INVALIDARG");
}

int main(void)
{
    HWND hwnd;

    CoInitialize(NULL);
    hwnd = CreateWindowExA(0, "STATIC", "accdisp", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 10, 10, 200, 120,
            NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!hwnd)
    {
        printf("FAIL  no window\nRESULT: FAIL\n");
        return 1;
    }

    test_object(hwnd, OBJID_CLIENT, "client");
    test_object(hwnd, OBJID_WINDOW, "window");
    test_object(hwnd, OBJID_TITLEBAR, "titlebar");
    test_lresult(hwnd);

    DestroyWindow(hwnd);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
