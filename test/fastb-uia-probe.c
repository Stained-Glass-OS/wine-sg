/* uiautomationcore, fast mode (patch 3503): UiaNodeFromPoint; IUIAutomation6 CoalesceEvents and
 * ConnectionRecoveryBehavior, IUIAutomationCacheRequest::Clone,
 * IUIAutomationTreeWalker::NormalizeElement and the *WithOptions finders. */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <uiautomation.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

/* the parts of the public structures UiaNodeFromPoint takes */
struct cond { int type; };
struct cache_request
{
    struct cond *view_condition;
    int scope;
    int *properties;
    int property_count;
    int *patterns;
    int pattern_count;
    int element_mode;
};

static void test_node_from_point(void)
{
    HRESULT (WINAPI *node_from_point)(double, double, struct cache_request *, SAFEARRAY **, BSTR *);
    struct cond cond = { 0 /* ConditionType_True */ };
    struct cache_request req = { &cond, TreeScope_Element, NULL, 0, NULL, 0, AutomationElementMode_Full };
    SAFEARRAY *data = (SAFEARRAY *)1;
    BSTR tree = (BSTR)1;
    HRESULT hr;

    node_from_point = (void *)GetProcAddress(LoadLibraryA("uiautomationcore.dll"), "UiaNodeFromPoint");
    check(node_from_point != NULL, "UiaNodeFromPoint is exported");
    if (!node_from_point) return;
    hr = node_from_point(0.0, 0.0, &req, &data, &tree);
    check(hr == S_OK && data && tree, "UiaNodeFromPoint at the screen's corner");
    if (hr == S_OK) { SafeArrayDestroy(data); SysFreeString(tree); }
    hr = node_from_point(0.0, 0.0, NULL, &data, &tree);
    check(hr == E_INVALIDARG, "UiaNodeFromPoint needs a request");
}

int main(void)
{
    IUIAutomation6 *uia = NULL;
    IUIAutomationCacheRequest *req = NULL, *clone = NULL;
    IUIAutomationTreeWalker *walker = NULL;
    IUIAutomationElement *root = NULL, *norm = NULL, *found = NULL;
    IUIAutomationElement9 *root9 = NULL;
    IUIAutomationCondition *cond = NULL;
    enum CoalesceEventsOptions coalesce = 0;
    enum ConnectionRecoveryBehaviorOptions recovery = 0;
    enum TreeScope scope = 0;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    test_node_from_point();
    hr = CoCreateInstance(&CLSID_CUIAutomation8, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAutomation6, (void **)&uia);
    if (FAILED(hr)) { printf("FAIL  no IUIAutomation6 %lx\n", hr); printf("RESULT: FAIL\n"); return 1; }

    hr = IUIAutomation6_get_CoalesceEvents(uia, &coalesce);
    check(hr == S_OK && coalesce == CoalesceEventsOptions_Enabled, "coalescing is on by default");
    hr = IUIAutomation6_put_CoalesceEvents(uia, CoalesceEventsOptions_Disabled);
    check(hr == S_OK, "put_CoalesceEvents");
    IUIAutomation6_get_CoalesceEvents(uia, &coalesce);
    check(coalesce == CoalesceEventsOptions_Disabled, "coalescing off");
    hr = IUIAutomation6_put_CoalesceEvents(uia, 4);
    check(hr == E_INVALIDARG, "coalesce option check");
    hr = IUIAutomation6_get_CoalesceEvents(uia, NULL);
    check(hr == E_POINTER, "coalesce null pointer");

    hr = IUIAutomation6_get_ConnectionRecoveryBehavior(uia, &recovery);
    check(hr == S_OK && recovery == ConnectionRecoveryBehaviorOptions_Enabled, "recovery is on by default");
    hr = IUIAutomation6_put_ConnectionRecoveryBehavior(uia, ConnectionRecoveryBehaviorOptions_Disabled);
    IUIAutomation6_get_ConnectionRecoveryBehavior(uia, &recovery);
    check(hr == S_OK && recovery == ConnectionRecoveryBehaviorOptions_Disabled, "recovery off");

    hr = IUIAutomation6_CreateCacheRequest(uia, &req);
    check(hr == S_OK, "CreateCacheRequest");
    IUIAutomationCacheRequest_AddProperty(req, UIA_NamePropertyId);
    IUIAutomationCacheRequest_put_TreeScope(req, TreeScope_Element);
    hr = IUIAutomationCacheRequest_Clone(req, &clone);
    check(hr == S_OK && clone && clone != req, "cache request Clone");
    if (clone)
    {
        hr = IUIAutomationCacheRequest_get_TreeScope(clone, &scope);
        check(hr == S_OK && scope == TreeScope_Element, "clone keeps the scope");
        IUIAutomationCacheRequest_AddProperty(clone, UIA_ControlTypePropertyId);
        IUIAutomationCacheRequest_Release(clone);
    }
    hr = IUIAutomationCacheRequest_Clone(req, NULL);
    check(hr == E_POINTER, "Clone null pointer");

    hr = IUIAutomation6_GetRootElement(uia, &root);
    check(hr == S_OK && root, "root element");
    hr = IUIAutomation6_get_ControlViewWalker(uia, &walker);
    check(hr == S_OK && walker, "control view walker");
    if (root && walker)
    {
        hr = IUIAutomationTreeWalker_NormalizeElement(walker, root, &norm);
        check(SUCCEEDED(hr), "NormalizeElement");
        if (norm) IUIAutomationElement_Release(norm);
        hr = IUIAutomationTreeWalker_NormalizeElement(walker, NULL, &norm);
        check(hr == E_POINTER, "NormalizeElement null element");
    }

    IUIAutomation6_CreateTrueCondition(uia, &cond);
    if (root && cond)
    {
        hr = IUIAutomationElement_QueryInterface(root, &IID_IUIAutomationElement9, (void **)&root9);
        check(hr == S_OK, "IUIAutomationElement9");
        if (root9)
        {
            hr = IUIAutomationElement9_FindFirstWithOptions(root9, TreeScope_Children, cond,
                    TreeTraversalOptions_Default, NULL, &found);
            check(SUCCEEDED(hr), "FindFirstWithOptions");
            if (found) IUIAutomationElement_Release(found);
            hr = IUIAutomationElement9_FindFirstWithOptions(root9, TreeScope_Children, cond,
                    TreeTraversalOptions_PostOrder, NULL, &found);
            check(hr == E_NOTIMPL, "post-order traversal is reported as unsupported");
            IUIAutomationElement9_Release(root9);
        }
    }

    if (cond) IUIAutomationCondition_Release(cond);
    if (walker) IUIAutomationTreeWalker_Release(walker);
    if (root) IUIAutomationElement_Release(root);
    if (req) IUIAutomationCacheRequest_Release(req);
    IUIAutomation6_Release(uia);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
