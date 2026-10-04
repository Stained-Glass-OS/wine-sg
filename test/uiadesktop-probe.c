/* progman-gate.sh's UI Automation probe: the desktop's Progman and its icon
 * list found as Opera's installer finds them -- the root with a cache
 * request, an And of class name and control type, FindFirstBuildCache */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <ole2.h>
#include <uiautomation.h>
#include <stdio.h>

int wmain(void)
{
    IUIAutomation *uia = NULL; IUIAutomationCacheRequest *cache = NULL; IUIAutomationElement *root = NULL, *pm = NULL, *list = NULL;
    IUIAutomationCondition *byclass = NULL, *bytype = NULL, *both = NULL, *listcond = NULL;
    VARIANT v; HRESULT hr;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoCreateInstance(&CLSID_CUIAutomation8, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAutomation, (void **)&uia);
    if (FAILED(hr)) { printf("NOUIA %08lx\n", hr); return 1; }
    IUIAutomation_CreateCacheRequest(uia, &cache);
    IUIAutomationCacheRequest_AddProperty(cache, UIA_ClassNamePropertyId);
    hr = IUIAutomation_GetRootElementBuildCache(uia, cache, &root);
    printf("ROOT %08lx %s\n", hr, root ? "yes" : "no");
    VariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"Progman");
    IUIAutomation_CreatePropertyCondition(uia, UIA_ClassNamePropertyId, v, &byclass);
    VariantClear(&v); V_VT(&v) = VT_I4; V_I4(&v) = UIA_PaneControlTypeId;
    IUIAutomation_CreatePropertyCondition(uia, UIA_ControlTypePropertyId, v, &bytype);
    hr = IUIAutomation_CreateAndCondition(uia, byclass, bytype, &both);
    printf("AND %08lx %s\n", hr, both ? "yes" : "no");
    if (root && both) hr = IUIAutomationElement_FindFirstBuildCache(root, TreeScope_Children, both, cache, &pm);
    printf("PROGMAN %08lx %s\n", hr, pm ? "found" : "none");
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"SysListView32");
    IUIAutomation_CreatePropertyCondition(uia, UIA_ClassNamePropertyId, v, &listcond);
    if (pm && listcond) hr = IUIAutomationElement_FindFirstBuildCache(pm, TreeScope_Descendants, listcond, cache, &list);
    printf("LIST %08lx %s\n", hr, list ? "found" : "none");
    return 0;
}
