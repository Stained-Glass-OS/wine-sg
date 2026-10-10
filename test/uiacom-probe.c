/* uiautomationcore: the cached properties of IUIAutomationElement and the getters that were
 * stubs (patches/sg/2631): every Cached getter returns what the cache request fetched (and
 * E_INVALIDARG for what it did not), the element-valued and array-valued properties, SetFocus,
 * GetRuntimeId, GetClickablePoint, GetCachedPropertyValue. A button of a window is the element. */
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
    IUIAutomationElement *cur = NULL, *cached = NULL, *elem = NULL;
    IUIAutomationElement9 *cur9 = NULL, *cached9 = NULL;
    IUIAutomationCacheRequest *req = NULL;
    IUIAutomationElementArray *arr = NULL;
    HWND top, button;
    HRESULT hr;
    BSTR bstr = NULL, bstr2 = NULL;
    BOOL b, b2;
    int i, i2;
    SAFEARRAY *sa = NULL;
    POINT pt;
    RECT rect;
    VARIANT v;
    static const PROPERTYID props[] =
    {
        UIA_ProcessIdPropertyId, UIA_ControlTypePropertyId, UIA_LocalizedControlTypePropertyId, UIA_NamePropertyId,
        UIA_AcceleratorKeyPropertyId, UIA_AccessKeyPropertyId, UIA_HasKeyboardFocusPropertyId,
        UIA_IsKeyboardFocusablePropertyId, UIA_IsEnabledPropertyId, UIA_AutomationIdPropertyId,
        UIA_ClassNamePropertyId, UIA_HelpTextPropertyId, UIA_CulturePropertyId, UIA_IsControlElementPropertyId,
        UIA_IsContentElementPropertyId, UIA_IsPasswordPropertyId, UIA_NativeWindowHandlePropertyId,
        UIA_ItemTypePropertyId, UIA_IsOffscreenPropertyId, UIA_OrientationPropertyId, UIA_FrameworkIdPropertyId,
        UIA_IsRequiredForFormPropertyId, UIA_ItemStatusPropertyId, UIA_BoundingRectanglePropertyId,
        UIA_LabeledByPropertyId, UIA_ProviderDescriptionPropertyId, UIA_ControllerForPropertyId,
        UIA_DescribedByPropertyId, UIA_FlowsToPropertyId, UIA_FlowsFromPropertyId,
        UIA_PositionInSetPropertyId, UIA_SizeOfSetPropertyId, UIA_LevelPropertyId, UIA_IsPeripheralPropertyId,
        UIA_LiveSettingPropertyId, UIA_AriaRolePropertyId, UIA_AriaPropertiesPropertyId,
        UIA_IsDataValidForFormPropertyId, UIA_OptimizeForVisualContentPropertyId, UIA_FullDescriptionPropertyId,
        UIA_LandmarkTypePropertyId, UIA_LocalizedLandmarkTypePropertyId, UIA_HeadingLevelPropertyId,
        UIA_IsDialogPropertyId, UIA_AnnotationTypesPropertyId, UIA_AnnotationObjectsPropertyId,
    };

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    top = CreateWindowA("static", "top", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 300, 200, 0, 0, 0, 0);
    button = CreateWindowA("button", "Press me", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 20, 20, 120, 40, top, (HMENU)7, 0, 0);
    check(top && button, "windows");

    hr = CoCreateInstance(&CLSID_CUIAutomation8, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAutomation, (void **)&uia);
    check(hr == S_OK && uia, "IUIAutomation (%#lx)", hr);
    if (!uia) goto done;
    hr = IUIAutomation_ElementFromHandle(uia, (UIA_HWND)button, &cur);
    check(hr == S_OK && cur, "the button (%#lx)", hr);
    if (!cur) goto done;

    hr = IUIAutomation_CreateCacheRequest(uia, &req);
    for (i = 0; i < (int)(sizeof(props) / sizeof(props[0])); ++i)
        IUIAutomationCacheRequest_AddProperty(req, props[i]);
    hr = IUIAutomationElement_BuildUpdatedCache(cur, req, &cached);
    check(hr == S_OK && cached, "cached element (%#lx)", hr);
    if (!cached) goto done;

    IUIAutomationElement_QueryInterface(cur, &IID_IUIAutomationElement9, (void **)&cur9);
    IUIAutomationElement_QueryInterface(cached, &IID_IUIAutomationElement9, (void **)&cached9);
    check(cur9 && cached9, "IUIAutomationElement9");
    if (!cur9 || !cached9) goto done;

    /* the cached values are the current ones */
#define SAME_BSTR(name) do { bstr = bstr2 = NULL; \
        hr = IUIAutomationElement9_get_Cached##name(cached9, &bstr); \
        check(hr == S_OK && bstr, "Cached" #name " (%#lx, %ls)", hr, bstr ? bstr : L"(null)"); \
        IUIAutomationElement9_get_Current##name(cur9, &bstr2); \
        check(bstr && bstr2 && !wcscmp(bstr, bstr2), "Cached" #name " is Current" #name); \
        SysFreeString(bstr); SysFreeString(bstr2); } while (0)
#define SAME_BOOL(name) do { b = b2 = 7; \
        hr = IUIAutomationElement9_get_Cached##name(cached9, &b); \
        IUIAutomationElement9_get_Current##name(cur9, &b2); \
        check(hr == S_OK && b == b2 && (b == TRUE || b == FALSE), "Cached" #name " is Current" #name " (%d, %d, %#lx)", b, b2, hr); } while (0)
#define SAME_INT(name, type) do { type x = (type)77, y = (type)88; \
        hr = IUIAutomationElement9_get_Cached##name(cached9, &x); \
        IUIAutomationElement9_get_Current##name(cur9, &y); \
        check(hr == S_OK && x == y, "Cached" #name " is Current" #name " (%d, %d, %#lx)", (int)x, (int)y, hr); } while (0)

    SAME_INT(ProcessId, int);
    check(1, "-");
    SAME_BSTR(LocalizedControlType);
    SAME_BSTR(AcceleratorKey);
    SAME_BSTR(AccessKey);
    SAME_BOOL(IsEnabled);
    SAME_BSTR(AutomationId);
    SAME_BSTR(ClassName);
    SAME_BSTR(HelpText);
    SAME_INT(Culture, int);
    SAME_BOOL(IsControlElement);
    SAME_BOOL(IsContentElement);
    SAME_BOOL(IsPassword);
    SAME_BSTR(ItemType);
    SAME_BOOL(IsOffscreen);
    SAME_INT(Orientation, enum OrientationType);
    SAME_BSTR(FrameworkId);
    SAME_BOOL(IsRequiredForForm);
    SAME_BSTR(ItemStatus);
    SAME_BSTR(AriaRole);
    SAME_BSTR(AriaProperties);
    SAME_BOOL(IsDataValidForForm);
    SAME_BSTR(ProviderDescription);
    SAME_BOOL(IsPeripheral);
    SAME_INT(PositionInSet, int);
    SAME_INT(SizeOfSet, int);
    SAME_INT(Level, int);
    SAME_INT(LiveSetting, enum LiveSetting);
    SAME_BOOL(OptimizeForVisualContent);
    SAME_BSTR(FullDescription);
    SAME_INT(LandmarkType, LANDMARKTYPEID);
    SAME_BSTR(LocalizedLandmarkType);
    SAME_BOOL(IsDialog);
    {
        UIA_HWND h1 = 0, h2 = 0;

        IUIAutomationElement_get_CachedNativeWindowHandle(cached, &h1);
        IUIAutomationElement_get_CurrentNativeWindowHandle(cur, &h2);
        check(h1 == (UIA_HWND)button && h1 == h2, "the native window handle is the button's (%p)", (void *)h1);
    }
    check(!wcscmp(L"Press me", (bstr = NULL, IUIAutomationElement_get_CachedName(cached, &bstr), bstr ? bstr : L"")), "the name");
    SysFreeString(bstr);

    /* elements and arrays */
    elem = (void *)0xdeadbeef;
    hr = IUIAutomationElement_get_CachedLabeledBy(cached, &elem);
    check(hr == S_OK && !elem, "cached LabeledBy: nothing (%#lx, %p)", hr, elem);
    elem = (void *)0xdeadbeef;
    hr = IUIAutomationElement_get_CurrentLabeledBy(cur, &elem);
    check(hr == S_OK && !elem, "current LabeledBy: nothing (%#lx, %p)", hr, elem);
    arr = NULL;
    hr = IUIAutomationElement_get_CachedControllerFor(cached, &arr);
    i = -1;
    if (arr) IUIAutomationElementArray_get_Length(arr, &i);
    check(hr == S_OK && arr && i == 0, "cached ControllerFor: an empty array (%#lx, %d)", hr, i);
    if (arr) IUIAutomationElementArray_Release(arr);
    arr = NULL;
    hr = IUIAutomationElement_get_CurrentFlowsTo(cur, &arr);
    i = -1;
    if (arr) IUIAutomationElementArray_get_Length(arr, &i);
    check(hr == S_OK && arr && i == 0, "current FlowsTo: an empty array (%#lx, %d)", hr, i);
    if (arr) IUIAutomationElementArray_Release(arr);

    /* what was not fetched */
    hr = IUIAutomationElement_get_CachedName(cur, &bstr);
    check(hr == E_INVALIDARG || hr == S_OK, "the current element is no cache (%#lx)", hr);
    {
        IUIAutomationCacheRequest *req2 = NULL;
        IUIAutomationElement *cached2 = NULL;

        IUIAutomation_CreateCacheRequest(uia, &req2);
        IUIAutomationCacheRequest_AddProperty(req2, UIA_NamePropertyId);
        IUIAutomationElement_BuildUpdatedCache(cur, req2, &cached2);
        b = 7;
        hr = IUIAutomationElement_get_CachedIsEnabled(cached2, &b);
        check(hr == E_INVALIDARG, "a property the request did not name = %#lx", hr);
        hr = IUIAutomationElement_get_CachedAutomationId(cached2, &bstr);
        check(hr == E_INVALIDARG, "also a string = %#lx", hr);
        hr = IUIAutomationElement_get_CachedIsEnabled(cached2, NULL);
        check(hr == E_POINTER, "no pointer = %#lx", hr);
        IUIAutomationElement_Release(cached2);
        IUIAutomationCacheRequest_Release(req2);
    }

    /* GetCachedPropertyValue is the Ex with the default */
    VariantInit(&v);
    hr = IUIAutomationElement_GetCachedPropertyValue(cached, UIA_NamePropertyId, &v);
    check(hr == S_OK && V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), L"Press me"), "GetCachedPropertyValue (%#lx)", hr);
    VariantClear(&v);
    hr = IUIAutomationElement_GetCachedPropertyValue(cached, UIA_ValueValuePropertyId, &v);
    check(hr == E_INVALIDARG, "an unfetched one = %#lx", hr);

    /* runtime id, focus, clickable point */
    hr = IUIAutomationElement_GetRuntimeId(cur, &sa);
    i = 0;
    if (sa) { LONG lb = 0, ub = -1; SafeArrayGetLBound(sa, 1, &lb); SafeArrayGetUBound(sa, 1, &ub); i = ub - lb + 1; SafeArrayDestroy(sa); }
    check(hr == S_OK && i > 0, "runtime id of %d numbers (%#lx)", i, hr);
    hr = IUIAutomationElement_GetRuntimeId(cur, NULL);
    check(hr == E_POINTER, "no array = %#lx", hr);

    hr = IUIAutomationElement_SetFocus(cur);
    check(hr == S_OK, "SetFocus = %#lx", hr);
    check(GetFocus() == button, "the button has the focus");
    b = FALSE;
    IUIAutomationElement_get_CurrentHasKeyboardFocus(cur, &b);
    check(b, "and says so");

    IUIAutomationElement_get_CurrentBoundingRectangle(cur, &rect);
    b = FALSE; pt.x = pt.y = -1;
    hr = IUIAutomationElement_GetClickablePoint(cur, &pt, &b);
    check(hr == S_OK && b && pt.x >= rect.left && pt.x < rect.right && pt.y >= rect.top && pt.y < rect.bottom,
            "clickable point (%ld,%ld) in (%ld,%ld)-(%ld,%ld) (%#lx)", pt.x, pt.y, rect.left, rect.top, rect.right, rect.bottom, hr);
    hr = IUIAutomationElement_GetClickablePoint(cur, NULL, &b);
    check(hr == E_POINTER, "no point = %#lx", hr);

    IUIAutomationElement_Release(cached);
    IUIAutomationCacheRequest_Release(req);
    IUIAutomationElement_Release(cur);
    IUIAutomation_Release(uia);
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
