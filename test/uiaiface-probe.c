/* uiautomationcore (patches/sg/2657): IUIAutomation::CompareElements, CompareRuntimeIds, RectToVariant,
 * VariantToRect, SafeArrayToRectNativeArray, the timeouts and AutoSetFocus, ElementFromIAccessible's checks. */
#define COBJMACROS
#include <windows.h>
#include <uiautomation.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

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

static SAFEARRAY *make_ids(int n, const int *vals)
{
    SAFEARRAY *sa = SafeArrayCreateVector(VT_I4, 0, n);
    int i;

    for (i = 0; i < n; i++)
    {
        LONG idx = i;
        SafeArrayPutElement(sa, &idx, (void *)&vals[i]);
    }
    return sa;
}

int main(void)
{
    IUIAutomation *uia = NULL;
    IUIAutomation2 *uia2 = NULL;
    IUIAutomationElement *btn = NULL, *btn2 = NULL, *top_elem = NULL;
    HWND top, button;
    HRESULT hr;
    BOOL match;
    VARIANT v;
    RECT rc, out, *arr;
    SAFEARRAY *a, *b, *sa;
    int count;
    DWORD t;
    static const int ids1[3] = { 42, 1, 2 }, ids2[3] = { 42, 1, 3 }, ids3[2] = { 42, 1 };

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    top = CreateWindowA("static", "top", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 300, 200, 0, 0, 0, 0);
    button = CreateWindowA("button", "Press me", WS_CHILD | WS_VISIBLE, 20, 20, 120, 40, top, (HMENU)7, 0, 0);
    check(top && button, "windows");
    hr = CoCreateInstance(&CLSID_CUIAutomation8, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAutomation, (void **)&uia);
    check(hr == S_OK && uia, "IUIAutomation (%#lx)", hr);
    if (!uia) goto done;
    IUIAutomation_ElementFromHandle(uia, (UIA_HWND)button, &btn);
    IUIAutomation_ElementFromHandle(uia, (UIA_HWND)button, &btn2);
    IUIAutomation_ElementFromHandle(uia, (UIA_HWND)top, &top_elem);
    check(btn && btn2 && top_elem, "elements");
    if (!btn || !btn2 || !top_elem) goto done;

    match = 7;
    hr = IUIAutomation_CompareElements(uia, btn, btn2, &match);
    check(hr == S_OK && match == TRUE, "two elements of one window are the same (%#lx, %d)", hr, match);
    hr = IUIAutomation_CompareElements(uia, btn, top_elem, &match);
    check(hr == S_OK && match == FALSE, "a button and its parent differ (%#lx, %d)", hr, match);
    hr = IUIAutomation_CompareElements(uia, NULL, btn, &match);
    check(hr == E_POINTER, "no element is E_POINTER (%#lx)", hr);
    hr = IUIAutomation_CompareElements(uia, btn, btn2, NULL);
    check(hr == E_POINTER, "no result is E_POINTER (%#lx)", hr);

    a = make_ids(3, ids1);
    b = make_ids(3, ids1);
    match = 7;
    hr = IUIAutomation_CompareRuntimeIds(uia, a, b, &match);
    check(hr == S_OK && match == TRUE, "equal runtime ids (%#lx, %d)", hr, match);
    SafeArrayDestroy(b);
    b = make_ids(3, ids2);
    hr = IUIAutomation_CompareRuntimeIds(uia, a, b, &match);
    check(hr == S_OK && match == FALSE, "runtime ids differing in the last number (%#lx, %d)", hr, match);
    SafeArrayDestroy(b);
    b = make_ids(2, ids3);
    hr = IUIAutomation_CompareRuntimeIds(uia, a, b, &match);
    check(hr == S_OK && match == FALSE, "runtime ids of different lengths (%#lx, %d)", hr, match);
    hr = IUIAutomation_CompareRuntimeIds(uia, a, NULL, &match);
    check(hr == E_INVALIDARG, "a missing array is E_INVALIDARG (%#lx)", hr);
    SafeArrayDestroy(a);
    SafeArrayDestroy(b);

    rc.left = 10; rc.top = 20; rc.right = 110; rc.bottom = 70;
    VariantInit(&v);
    hr = IUIAutomation_RectToVariant(uia, rc, &v);
    check(hr == S_OK && V_VT(&v) == (VT_ARRAY | VT_R8), "RectToVariant (%#lx, vt %#x)", hr, V_VT(&v));
    memset(&out, 0, sizeof(out));
    hr = IUIAutomation_VariantToRect(uia, v, &out);
    check(hr == S_OK && out.left == 10 && out.top == 20 && out.right == 110 && out.bottom == 70,
          "VariantToRect round trip (%#lx, %ld %ld %ld %ld)", hr, out.left, out.top, out.right, out.bottom);
    VariantClear(&v);
    V_VT(&v) = VT_I4;
    V_I4(&v) = 5;
    hr = IUIAutomation_VariantToRect(uia, v, &out);
    check(hr == E_INVALIDARG, "VariantToRect of an integer is E_INVALIDARG (%#lx)", hr);
    hr = IUIAutomation_RectToVariant(uia, rc, NULL);
    check(hr == E_POINTER, "RectToVariant(NULL) is E_POINTER (%#lx)", hr);

    {
        double vals[8] = { 1, 2, 10, 20, 100, 200, 5, 6 };
        void *data;

        sa = SafeArrayCreateVector(VT_R8, 0, 8);
        SafeArrayAccessData(sa, &data);
        memcpy(data, vals, sizeof(vals));
        SafeArrayUnaccessData(sa);
        arr = NULL; count = 0;
        hr = IUIAutomation_SafeArrayToRectNativeArray(uia, sa, &arr, &count);
        check(hr == S_OK && count == 2 && arr && arr[0].right == 11 && arr[0].bottom == 22 && arr[1].left == 100
              && arr[1].right == 105 && arr[1].bottom == 206, "SafeArrayToRectNativeArray (%#lx, %d)", hr, count);
        CoTaskMemFree(arr);
        SafeArrayDestroy(sa);
        sa = SafeArrayCreateVector(VT_R8, 0, 6);
        hr = IUIAutomation_SafeArrayToRectNativeArray(uia, sa, &arr, &count);
        check(hr == E_INVALIDARG && !arr && !count, "six doubles are E_INVALIDARG (%#lx)", hr);
        SafeArrayDestroy(sa);
    }

    hr = IUIAutomation_QueryInterface(uia, &IID_IUIAutomation2, (void **)&uia2);
    check(hr == S_OK && uia2, "IUIAutomation2 (%#lx)", hr);
    if (uia2)
    {
        match = 7;
        hr = IUIAutomation2_get_AutoSetFocus(uia2, &match);
        check(hr == S_OK && match == TRUE, "AutoSetFocus is on by default (%#lx, %d)", hr, match);
        hr = IUIAutomation2_put_AutoSetFocus(uia2, FALSE);
        IUIAutomation2_get_AutoSetFocus(uia2, &match);
        check(hr == S_OK && match == FALSE, "AutoSetFocus off");
        t = 0;
        hr = IUIAutomation2_get_ConnectionTimeout(uia2, &t);
        check(hr == S_OK && t == 2000, "ConnectionTimeout is 2000 by default (%#lx, %lu)", hr, t);
        IUIAutomation2_put_ConnectionTimeout(uia2, 500);
        IUIAutomation2_get_ConnectionTimeout(uia2, &t);
        check(t == 500, "ConnectionTimeout 500 (%lu)", t);
        hr = IUIAutomation2_get_TransactionTimeout(uia2, &t);
        check(hr == S_OK && t == 20000, "TransactionTimeout is 20000 by default (%#lx, %lu)", hr, t);
        IUIAutomation2_put_TransactionTimeout(uia2, 1234);
        IUIAutomation2_get_TransactionTimeout(uia2, &t);
        check(t == 1234, "TransactionTimeout 1234 (%lu)", t);
        hr = IUIAutomation2_get_AutoSetFocus(uia2, NULL);
        check(hr == E_POINTER, "get_AutoSetFocus(NULL) is E_POINTER (%#lx)", hr);
        IUIAutomation2_Release(uia2);
    }

    {
        IUIAutomationCondition *cond = NULL;
        IUIAutomationElement *found = NULL;
        VARIANT name;

        V_VT(&name) = VT_BSTR;
        V_BSTR(&name) = SysAllocString(L"PRESS ME");
        hr = IUIAutomation_CreatePropertyConditionEx(uia, UIA_NamePropertyId, name, PropertyConditionFlags_IgnoreCase, &cond);
        check(hr == S_OK && cond, "CreatePropertyConditionEx(IgnoreCase) (%#lx)", hr);
        if (cond)
        {
            hr = IUIAutomationElement_FindFirst(top_elem, TreeScope_Descendants, cond, &found);
            check(hr == S_OK && found, "the case-insensitive name finds the button (%#lx)", hr);
            if (found) IUIAutomationElement_Release(found);
            IUIAutomationCondition_Release(cond);
        }
        found = NULL;
        hr = IUIAutomation_CreatePropertyConditionEx(uia, UIA_NamePropertyId, name, PropertyConditionFlags_None, &cond);
        if (hr == S_OK)
        {
            hr = IUIAutomationElement_FindFirst(top_elem, TreeScope_Descendants, cond, &found);
            check(hr == S_OK && !found, "without IgnoreCase it does not (%#lx)", hr);
            if (found) IUIAutomationElement_Release(found);
            IUIAutomationCondition_Release(cond);
        }
        cond = NULL;
        hr = IUIAutomation_CreatePropertyConditionEx(uia, UIA_NamePropertyId, name, 0x40, &cond);
        check(hr == E_INVALIDARG && !cond, "unknown flags are E_INVALIDARG (%#lx)", hr);
        VariantClear(&name);
    }

    {
        POINT pt = { 60, 40 };
        IUIAutomationElement *at = NULL;
        VARIANT hv;

        ClientToScreen(top, &pt);
        Sleep(200);
        hr = IUIAutomation_ElementFromPoint(uia, pt, &at);
        check(hr == S_OK && at, "ElementFromPoint over the button (%#lx)", hr);
        if (at)
        {
            VariantInit(&hv);
            IUIAutomationElement_GetCurrentPropertyValue(at, UIA_NativeWindowHandlePropertyId, &hv);
            check(V_VT(&hv) == VT_I4 && (HWND)(LONG_PTR)V_I4(&hv) == button, "it is the button (%ld)", V_VT(&hv) == VT_I4 ? V_I4(&hv) : -1);
            IUIAutomationElement_Release(at);
        }
        at = NULL;
        pt.x = pt.y = -30000;
        hr = IUIAutomation_ElementFromPoint(uia, pt, &at);
        check(hr == 0x80040201 && !at, "no window there is UIA_E_ELEMENTNOTAVAILABLE (%#lx)", hr);
    }

    btn2 = NULL;
    hr = IUIAutomation_ElementFromIAccessible(uia, NULL, 0, &btn2);
    check(hr == E_INVALIDARG && !btn2, "ElementFromIAccessible(NULL) is E_INVALIDARG (%#lx)", hr);
    hr = IUIAutomation_ElementFromIAccessible(uia, (IAccessible *)1, 0, NULL);
    check(hr == E_POINTER, "ElementFromIAccessible with no result is E_POINTER (%#lx)", hr);

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
