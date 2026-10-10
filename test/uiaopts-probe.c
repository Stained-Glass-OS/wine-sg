/* uiautomationcore (patches/sg/2668): the children of an Or condition, AddPattern and Clone of a cache request,
 * the connection recovery and event coalescing options, FindFirstWithOptions with the default traversal. */
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

int main(void)
{
    IUIAutomation *uia = NULL;
    IUIAutomation6 *uia6 = NULL;
    IUIAutomationElement *top_elem = NULL, *found = NULL;
    IUIAutomationElement9 *top9 = NULL;
    IUIAutomationCondition *c1 = NULL, *c2 = NULL, *or_cond = NULL;
    IUIAutomationOrCondition *or_iface = NULL;
    IUIAutomationCacheRequest *req = NULL, *clone = NULL;
    HWND top, button;
    HRESULT hr;
    VARIANT v;
    SAFEARRAY *sa = NULL;
    LONG lb, ub;
    int count;
    enum ConnectionRecoveryBehaviorOptions rec;
    enum CoalesceEventsOptions coal;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    top = CreateWindowA("static", "top", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 300, 200, 0, 0, 0, 0);
    button = CreateWindowA("button", "Press me", WS_CHILD | WS_VISIBLE, 20, 20, 120, 40, top, (HMENU)7, 0, 0);
    check(top && button, "windows");
    hr = CoCreateInstance(&CLSID_CUIAutomation8, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAutomation, (void **)&uia);
    check(hr == S_OK && uia, "IUIAutomation (%#lx)", hr);
    if (!uia) goto done;
    IUIAutomation_ElementFromHandle(uia, (UIA_HWND)top, &top_elem);

    /* Or condition children */
    VariantInit(&v);
    V_VT(&v) = VT_I4; V_I4(&v) = UIA_ButtonControlTypeId;
    IUIAutomation_CreatePropertyCondition(uia, UIA_ControlTypePropertyId, v, &c1);
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"Press me");
    IUIAutomation_CreatePropertyCondition(uia, UIA_NamePropertyId, v, &c2);
    VariantClear(&v);
    hr = IUIAutomation_CreateOrCondition(uia, c1, c2, &or_cond);
    check(hr == S_OK && or_cond, "Or condition (%#lx)", hr);
    if (or_cond)
    {
        IUIAutomationCondition_QueryInterface(or_cond, &IID_IUIAutomationOrCondition, (void **)&or_iface);
        hr = IUIAutomationOrCondition_GetChildren(or_iface, &sa);
        check(hr == S_OK && sa, "GetChildren (%#lx)", hr);
        if (sa)
        {
            SafeArrayGetLBound(sa, 1, &lb);
            SafeArrayGetUBound(sa, 1, &ub);
            check(ub - lb + 1 == 2 && SafeArrayGetElemsize(sa) == sizeof(void *), "two entries");
            lb = 1;
            {
                IUnknown *unk = NULL;
                SafeArrayGetElement(sa, &lb, &unk);
                check(unk == (IUnknown *)c2, "the second entry is the second condition");
                if (unk) IUnknown_Release(unk);
            }
            SafeArrayDestroy(sa);
        }
        hr = IUIAutomationOrCondition_GetChildren(or_iface, NULL);
        check(hr == E_POINTER, "GetChildren(NULL) is E_POINTER (%#lx)", hr);
        count = 0;
        IUIAutomationOrCondition_get_ChildCount(or_iface, &count);
        check(count == 2, "child count");
        IUIAutomationOrCondition_Release(or_iface);
    }

    /* cache request */
    IUIAutomation_CreateCacheRequest(uia, &req);
    check(IUIAutomationCacheRequest_AddPattern(req, UIA_InvokePatternId) == S_OK, "AddPattern");
    check(IUIAutomationCacheRequest_AddPattern(req, UIA_InvokePatternId) == S_OK, "AddPattern twice");
    check(IUIAutomationCacheRequest_AddPattern(req, 99999) == E_INVALIDARG, "an unknown pattern is E_INVALIDARG");
    IUIAutomationCacheRequest_AddProperty(req, UIA_NamePropertyId);
    IUIAutomationCacheRequest_put_TreeFilter(req, c1);
    hr = IUIAutomationCacheRequest_Clone(req, &clone);
    check(hr == S_OK && clone && clone != req, "Clone (%#lx)", hr);
    if (clone)
    {
        IUIAutomationCondition *filter = NULL;

        IUIAutomationCacheRequest_get_TreeFilter(clone, &filter);
        check(filter == c1, "the clone has the filter (%p, %p)", filter, c1);
        if (filter) IUIAutomationCondition_Release(filter);
        IUIAutomationCacheRequest_put_TreeFilter(clone, c2);
        IUIAutomationCacheRequest_get_TreeFilter(req, &filter);
        check(filter == c1, "changing the clone leaves the original (%p, %p)", filter, c1);
        if (filter) IUIAutomationCondition_Release(filter);
        IUIAutomationCacheRequest_Release(clone);
    }
    check(IUIAutomationCacheRequest_Clone(req, NULL) == E_POINTER, "Clone(NULL) is E_POINTER");

    /* options of the automation object */
    IUIAutomation_QueryInterface(uia, &IID_IUIAutomation6, (void **)&uia6);
    check(uia6 != NULL, "IUIAutomation6");
    if (uia6)
    {
        rec = 7; coal = 7;
        hr = IUIAutomation6_get_ConnectionRecoveryBehavior(uia6, &rec);
        check(hr == S_OK && rec == ConnectionRecoveryBehaviorOptions_Enabled, "recovery is enabled by default (%#lx, %d)", hr, rec);
        check(IUIAutomation6_put_ConnectionRecoveryBehavior(uia6, ConnectionRecoveryBehaviorOptions_Disabled) == S_OK, "put recovery");
        IUIAutomation6_get_ConnectionRecoveryBehavior(uia6, &rec);
        check(rec == ConnectionRecoveryBehaviorOptions_Disabled, "recovery disabled");
        check(IUIAutomation6_put_ConnectionRecoveryBehavior(uia6, 7) == E_INVALIDARG, "an unknown recovery value is E_INVALIDARG");
        hr = IUIAutomation6_get_CoalesceEvents(uia6, &coal);
        check(hr == S_OK && coal == CoalesceEventsOptions_Enabled, "coalescing is enabled by default (%#lx, %d)", hr, coal);
        IUIAutomation6_put_CoalesceEvents(uia6, CoalesceEventsOptions_Disabled);
        IUIAutomation6_get_CoalesceEvents(uia6, &coal);
        check(coal == CoalesceEventsOptions_Disabled, "coalescing disabled");
        check(IUIAutomation6_put_CoalesceEvents(uia6, 9) == E_INVALIDARG, "an unknown coalesce value is E_INVALIDARG");
        check(IUIAutomation6_get_CoalesceEvents(uia6, NULL) == E_POINTER, "get(NULL) is E_POINTER");
        IUIAutomation6_Release(uia6);
    }

    /* FindFirstWithOptions */
    IUIAutomationElement_QueryInterface(top_elem, &IID_IUIAutomationElement9, (void **)&top9);
    if (top9 && c2)
    {
        hr = IUIAutomationElement9_FindFirstWithOptions(top9, TreeScope_Descendants, c2, TreeTraversalOptions_Default, NULL, &found);
        check(hr == S_OK && found, "FindFirstWithOptions with the default order finds the button (%#lx)", hr);
        if (found) IUIAutomationElement_Release(found);
        found = NULL;
        hr = IUIAutomationElement9_FindFirstWithOptions(top9, TreeScope_Descendants, c2, TreeTraversalOptions_PostOrder, NULL, &found);
        check(hr == E_NOTIMPL, "post order is not available (%#lx)", hr);
        if (found) IUIAutomationElement_Release(found);
    }
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
