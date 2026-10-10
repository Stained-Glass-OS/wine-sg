/* UI Automation cached children and parent (patch 2676), run by
 * test/uiacachetree-gate.sh on Xvfb. A window with two child windows, the
 * first with a child of its own, is read through cache requests of
 * different scopes: TreeScope_Children / Descendants cache the children
 * (and the grandchildren) with the element, GetCachedChildren and
 * GetCachedParent answer from the cache, and an element whose request did
 * not cover them answers E_INVALIDARG. The cached tree outlives the element
 * it was requested for. These were E_NOTIMPL, and put_TreeScope refused the
 * scopes. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <uiautomation.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static int cached_hwnd(IUIAutomationElement *e, HWND *h)
{
    VARIANT v;
    HRESULT hr = IUIAutomationElement_GetCachedPropertyValue(e, UIA_NativeWindowHandlePropertyId, &v);
    *h = NULL;
    if (hr != S_OK || V_VT(&v) != VT_I4) return 0;
    *h = (HWND)(LONG_PTR)V_I4(&v);
    return 1;
}

static IUIAutomationCacheRequest *make_request(IUIAutomation *uia, enum TreeScope scope)
{
    IUIAutomationCacheRequest *req = NULL;
    IUIAutomationCondition *cond = NULL;

    IUIAutomation_CreateCacheRequest(uia, &req);
    IUIAutomation_CreateTrueCondition(uia, &cond);
    IUIAutomationCacheRequest_put_TreeFilter(req, cond);
    IUIAutomationCondition_Release(cond);
    IUIAutomationCacheRequest_AddProperty(req, UIA_NativeWindowHandlePropertyId);
    IUIAutomationCacheRequest_put_TreeScope(req, scope);
    return req;
}

static HWND top_hwnd, c1, c2, g1;
static HANDLE ready, stop_thread;

/* the windows live in a thread of their own that keeps pumping messages:
 * the providers send WM_GETOBJECT to them */
static DWORD WINAPI window_thread(void *arg)
{
    WNDCLASSW wc = { 0 };
    MSG msg;

    wc.lpfnWndProc = DefWindowProcW;
    wc.lpszClassName = L"SgCacheCont";
    RegisterClassW(&wc);
    top_hwnd = CreateWindowW(L"SgCacheCont", L"cache top", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    c1 = CreateWindowW(L"SgCacheCont", L"c1", WS_CHILD | WS_VISIBLE, 0, 0, 100, 100, top_hwnd, NULL, NULL, NULL);
    c2 = CreateWindowW(L"SgCacheCont", L"c2", WS_CHILD | WS_VISIBLE, 100, 0, 100, 100, top_hwnd, NULL, NULL, NULL);
    g1 = CreateWindowW(L"SgCacheCont", L"g1", WS_CHILD | WS_VISIBLE, 0, 0, 50, 50, c1, NULL, NULL, NULL);
    SetEvent(ready);
    while (WaitForSingleObject(stop_thread, 0) != WAIT_OBJECT_0)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        MsgWaitForMultipleObjects(1, &stop_thread, FALSE, 50, QS_ALLINPUT);
    }
    DestroyWindow(top_hwnd);
    return 0;
}

int main(void)
{
    HANDLE thread;
    IUIAutomation *uia;
    IUIAutomationElement *root = NULL, *kid, *kid2, *grand, *par, *top;
    IUIAutomationElementArray *arr = NULL, *arr2;
    IUIAutomationCacheRequest *req, *clone;
    HWND h, first, second;
    enum TreeScope scope;
    int len, i;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAutomation, (void **)&uia);
    check(hr == S_OK, "CUIAutomation");
    if (FAILED(hr)) goto done;

    ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    stop_thread = CreateEventW(NULL, TRUE, FALSE, NULL);
    thread = CreateThread(NULL, 0, window_thread, NULL, 0, NULL);
    WaitForSingleObject(ready, 10000);
    first = GetWindow(top_hwnd, GW_CHILD);
    second = GetWindow(first, GW_HWNDNEXT);

    /* the scope is stored as given */
    req = make_request(uia, TreeScope_Element);
    hr = IUIAutomationCacheRequest_put_TreeScope(req, TreeScope_Children);
    check(hr == S_OK, "put_TreeScope(Children)");
    scope = 0;
    hr = IUIAutomationCacheRequest_get_TreeScope(req, &scope);
    check(hr == S_OK && scope == TreeScope_Children, "get_TreeScope: Children, the element not added");
    hr = IUIAutomationCacheRequest_put_TreeScope(req, TreeScope_Subtree);
    check(hr == S_OK, "put_TreeScope(Subtree)");
    hr = IUIAutomationCacheRequest_Clone(req, &clone);
    scope = 0;
    check(hr == S_OK && SUCCEEDED(IUIAutomationCacheRequest_get_TreeScope(clone, &scope)) && scope == TreeScope_Subtree,
          "Clone keeps the scope");
    IUIAutomationCacheRequest_Release(clone);
    IUIAutomationCacheRequest_Release(req);

    /* an ordinary element: no tree is cached */
    hr = IUIAutomation_ElementFromHandle(uia, top_hwnd, &top);
    check(hr == S_OK && top, "ElementFromHandle");
    if (!top) goto done;
    req = make_request(uia, TreeScope_Element);
    hr = IUIAutomationElement_BuildUpdatedCache(top, req, &root);
    check(hr == S_OK && root, "BuildUpdatedCache(Element)");
    IUIAutomationCacheRequest_Release(req);
    arr = (void *)1;
    hr = IUIAutomationElement_GetCachedChildren(root, &arr);
    check(hr == E_INVALIDARG && !arr, "Element scope: GetCachedChildren is E_INVALIDARG");
    par = (void *)1;
    hr = IUIAutomationElement_GetCachedParent(root, &par);
    check(hr == E_INVALIDARG && !par, "Element scope: GetCachedParent is E_INVALIDARG");
    hr = IUIAutomationElement_GetCachedChildren(root, NULL);
    check(hr == E_POINTER, "GetCachedChildren(NULL): E_POINTER");
    hr = IUIAutomationElement_GetCachedParent(root, NULL);
    check(hr == E_POINTER, "GetCachedParent(NULL): E_POINTER");
    IUIAutomationElement_Release(root);

    /* Element | Children */
    req = make_request(uia, TreeScope_Element | TreeScope_Children);
    root = NULL;
    hr = IUIAutomationElement_BuildUpdatedCache(top, req, &root);
    check(hr == S_OK && root, "BuildUpdatedCache(Element | Children)");
    IUIAutomationCacheRequest_Release(req);
    arr = NULL;
    hr = IUIAutomationElement_GetCachedChildren(root, &arr);
    check(hr == S_OK && arr, "GetCachedChildren");
    len = -1;
    if (arr) IUIAutomationElementArray_get_Length(arr, &len);
    check(len == 2, "two children are cached");
    kid = kid2 = NULL;
    if (len == 2)
    {
        IUIAutomationElementArray_GetElement(arr, 0, &kid);
        IUIAutomationElementArray_GetElement(arr, 1, &kid2);
        check(kid && cached_hwnd(kid, &h) && h == first, "the first child, with its cached properties");
        check(kid2 && cached_hwnd(kid2, &h) && h == second, "the second child, in order");
        arr2 = (void *)1;
        hr = IUIAutomationElement_GetCachedChildren(kid, &arr2);
        check(hr == E_INVALIDARG && !arr2, "Children scope: a child's own children are not cached");
        par = NULL;
        hr = IUIAutomationElement_GetCachedParent(kid2, &par);
        check(hr == S_OK && par && cached_hwnd(par, &h) && h == top_hwnd, "GetCachedParent: the root, cached");
        if (par) IUIAutomationElement_Release(par);
    }
    if (arr) IUIAutomationElementArray_Release(arr);
    par = (void *)1;
    hr = IUIAutomationElement_GetCachedParent(root, &par);
    check(hr == E_INVALIDARG && !par, "the root of the tree has no cached parent");
    /* the tree outlives the element it was requested for */
    IUIAutomationElement_Release(root);
    if (kid)
    {
        par = NULL;
        hr = IUIAutomationElement_GetCachedParent(kid, &par);
        check(hr == S_OK && par && cached_hwnd(par, &h) && h == top_hwnd, "the parent is still there after the root is released");
        if (par) IUIAutomationElement_Release(par);
        IUIAutomationElement_Release(kid);
    }
    if (kid2) IUIAutomationElement_Release(kid2);

    /* Children alone: the element is only where the children come from */
    req = make_request(uia, TreeScope_Children);
    root = NULL;
    hr = IUIAutomationElement_BuildUpdatedCache(top, req, &root);
    check(hr == S_OK && root, "BuildUpdatedCache(Children)");
    IUIAutomationCacheRequest_Release(req);
    arr = NULL;
    len = -1;
    hr = IUIAutomationElement_GetCachedChildren(root, &arr);
    if (arr) IUIAutomationElementArray_get_Length(arr, &len);
    check(hr == S_OK && len == 2, "Children: two cached children");
    if (arr) IUIAutomationElementArray_Release(arr);
    IUIAutomationElement_Release(root);

    /* the whole subtree */
    req = make_request(uia, TreeScope_Subtree);
    root = NULL;
    hr = IUIAutomationElement_BuildUpdatedCache(top, req, &root);
    check(hr == S_OK && root, "BuildUpdatedCache(Subtree)");
    IUIAutomationCacheRequest_Release(req);
    arr = NULL;
    len = -1;
    hr = IUIAutomationElement_GetCachedChildren(root, &arr);
    if (arr) IUIAutomationElementArray_get_Length(arr, &len);
    check(hr == S_OK && len == 2, "Subtree: two cached children");
    for (i = 0; i < len; i++)
    {
        int glen = -1;

        kid = NULL;
        IUIAutomationElementArray_GetElement(arr, i, &kid);
        arr2 = NULL;
        hr = IUIAutomationElement_GetCachedChildren(kid, &arr2);
        if (arr2) IUIAutomationElementArray_get_Length(arr2, &glen);
        if (i == 0)
        {
            check(hr == S_OK && glen == 1, "the first child has a cached grandchild");
            grand = NULL;
            if (arr2) IUIAutomationElementArray_GetElement(arr2, 0, &grand);
            check(grand && cached_hwnd(grand, &h) && h == g1, "the grandchild, with its cached properties");
            par = NULL;
            if (grand) hr = IUIAutomationElement_GetCachedParent(grand, &par);
            check(par && cached_hwnd(par, &h) && h == first, "the grandchild's cached parent is the first child");
            if (par) IUIAutomationElement_Release(par);
            if (grand) IUIAutomationElement_Release(grand);
        }
        else
            check(hr == S_OK && glen == 0, "the second child has no children: an empty array");
        if (arr2) IUIAutomationElementArray_Release(arr2);
        IUIAutomationElement_Release(kid);
    }
    if (arr) IUIAutomationElementArray_Release(arr);
    IUIAutomationElement_Release(root);

    /* Find: every element found carries its subtree */
    {
        IUIAutomationCondition *cond;
        IUIAutomationElementArray *found = NULL;

        IUIAutomation_CreateTrueCondition(uia, &cond);
        req = make_request(uia, TreeScope_Element | TreeScope_Children);
        hr = IUIAutomationElement_FindAllBuildCache(top, TreeScope_Children, cond, req, &found);
        len = -1;
        if (found) IUIAutomationElementArray_get_Length(found, &len);
        check(hr == S_OK && len == 2, "FindAllBuildCache: the two children");
        if (len == 2)
        {
            kid = NULL;
            IUIAutomationElementArray_GetElement(found, 0, &kid);
            arr2 = NULL;
            hr = IUIAutomationElement_GetCachedChildren(kid, &arr2);
            len = -1;
            if (arr2) IUIAutomationElementArray_get_Length(arr2, &len);
            check(hr == S_OK && len == 1, "a found element has its own children cached");
            if (arr2) IUIAutomationElementArray_Release(arr2);
            IUIAutomationElement_Release(kid);
        }
        if (found) IUIAutomationElementArray_Release(found);
        IUIAutomationCacheRequest_Release(req);
        IUIAutomationCondition_Release(cond);
    }

    IUIAutomationElement_Release(top);
    IUIAutomation_Release(uia);
    SetEvent(stop_thread);
    WaitForSingleObject(thread, 5000);
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    return failures != 0;
}
