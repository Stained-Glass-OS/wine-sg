/* uiautomationcore (patches/sg/2669): IUIAutomationTreeWalker::NormalizeElement returns the element when the
 * walker's view has it, otherwise its nearest ancestor that is in the view. */
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

static BOOL same(IUIAutomation *uia, IUIAutomationElement *a, IUIAutomationElement *b)
{
    BOOL m = FALSE;
    IUIAutomation_CompareElements(uia, a, b, &m);
    return m;
}

int main(void)
{
    IUIAutomation *uia = NULL;
    IUIAutomationElement *top_elem = NULL, *btn = NULL, *out = NULL;
    IUIAutomationCondition *cond = NULL, *true_cond = NULL;
    IUIAutomationTreeWalker *walker = NULL, *all = NULL;
    HWND top, button;
    HRESULT hr;
    VARIANT v;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    top = CreateWindowA("static", "top", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 300, 200, 0, 0, 0, 0);
    button = CreateWindowA("button", "Press me", WS_CHILD | WS_VISIBLE, 20, 20, 120, 40, top, (HMENU)7, 0, 0);
    check(top && button, "windows");
    hr = CoCreateInstance(&CLSID_CUIAutomation8, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAutomation, (void **)&uia);
    check(hr == S_OK && uia, "IUIAutomation (%#lx)", hr);
    if (!uia) goto done;
    IUIAutomation_ElementFromHandle(uia, (UIA_HWND)top, &top_elem);
    IUIAutomation_ElementFromHandle(uia, (UIA_HWND)button, &btn);
    check(top_elem && btn, "elements");
    if (!top_elem || !btn) goto done;

    VariantInit(&v);
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"top");
    IUIAutomation_CreatePropertyCondition(uia, UIA_NamePropertyId, v, &cond);
    VariantClear(&v);
    IUIAutomation_CreateTrueCondition(uia, &true_cond);
    IUIAutomation_CreateTreeWalker(uia, cond, &walker);
    IUIAutomation_CreateTreeWalker(uia, true_cond, &all);
    check(walker && all, "tree walkers");
    if (!walker || !all) goto done;

    hr = IUIAutomationTreeWalker_NormalizeElement(all, btn, &out);
    check(hr == S_OK && out && same(uia, out, btn), "an element in the view is itself (%#lx)", hr);
    if (out) IUIAutomationElement_Release(out);
    out = NULL;

    hr = IUIAutomationTreeWalker_NormalizeElement(walker, btn, &out);
    check(hr == S_OK && out && same(uia, out, top_elem), "an element out of the view gives its nearest ancestor in it (%#lx)", hr);
    if (out) IUIAutomationElement_Release(out);
    out = NULL;

    hr = IUIAutomationTreeWalker_NormalizeElementBuildCache(walker, btn, NULL, &out);
    check(hr != S_OK || (out && same(uia, out, top_elem)), "with a cache request too (%#lx)", hr);
    if (out) IUIAutomationElement_Release(out);
    out = NULL;

    hr = IUIAutomationTreeWalker_NormalizeElement(walker, NULL, &out);
    check(hr == E_POINTER, "no element is E_POINTER (%#lx)", hr);
    hr = IUIAutomationTreeWalker_NormalizeElement(walker, btn, NULL);
    check(hr == E_POINTER, "no result is E_POINTER (%#lx)", hr);

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
