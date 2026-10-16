/* uiautomationcore: the *WithOptions finders with a traversal order (patch 3568). A panel with three buttons and a
 * sub-panel (two more buttons) is searched; the orders are compared with a walk of the windows made here. */
#define COBJMACROS
#include <windows.h>
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

#define MAXN 32
typedef struct { WCHAR names[MAXN][16]; int n; } list_t;

/* the expected orders, from the windows: children are taken in z-order, as the tree gives them */
static void walk(HWND hwnd, int pre, int reverse, list_t *out)
{
    HWND child[MAXN];
    int count = 0, i;
    HWND c;

    for (c = GetWindow(hwnd, GW_CHILD); c && count < MAXN; c = GetWindow(c, GW_HWNDNEXT)) child[count++] = c;
    if (pre && hwnd) { GetWindowTextW(hwnd, out->names[out->n], 16); out->n++; }
    for (i = 0; i < count; i++)
        walk(child[reverse ? count - 1 - i : i], pre, reverse, out);
    if (!pre && hwnd) { GetWindowTextW(hwnd, out->names[out->n], 16); out->n++; }
}

static void expected(HWND panel, int post, int last_to_first, list_t *out)
{
    HWND child[MAXN];
    int count = 0, i;
    HWND c;

    out->n = 0;
    for (c = GetWindow(panel, GW_CHILD); c && count < MAXN; c = GetWindow(c, GW_HWNDNEXT)) child[count++] = c;
    for (i = 0; i < count; i++)
        walk(child[last_to_first ? count - 1 - i : i], !post, last_to_first, out);
}

static void collect(IUIAutomationElementArray *array, list_t *out)
{
    int len = 0, i;

    out->n = 0;
    IUIAutomationElementArray_get_Length(array, &len);
    for (i = 0; i < len && i < MAXN; i++)
    {
        IUIAutomationElement *e = NULL;
        BSTR name = NULL;

        IUIAutomationElementArray_GetElement(array, i, &e);
        if (e) IUIAutomationElement_get_CurrentName(e, &name);
        wcsncpy(out->names[out->n], name ? name : L"?", 15);
        out->names[out->n][15] = 0;
        out->n++;
        SysFreeString(name);
        if (e) IUIAutomationElement_Release(e);
    }
}

static void show(const char *what, list_t *l)
{
    int i;
    printf("      %s:", what);
    for (i = 0; i < l->n; i++) printf(" %ls", l->names[i]);
    printf("\n");
}

static int same(list_t *a, list_t *b)
{
    int i;
    if (a->n != b->n) return 0;
    for (i = 0; i < a->n; i++) if (wcscmp(a->names[i], b->names[i])) return 0;
    return 1;
}

int main(void)
{
    static const struct { const char *name; int opts; int post; int ltf; } cases[] =
    {
        { "default", TreeTraversalOptions_Default, 0, 0 },
        { "last to first", TreeTraversalOptions_LastToFirstOrder, 0, 1 },
        { "post order", TreeTraversalOptions_PostOrder, 1, 0 },
        { "post order, last to first", TreeTraversalOptions_PostOrder | TreeTraversalOptions_LastToFirstOrder, 1, 1 },
    };
    IUIAutomation *uia = NULL;
    IUIAutomationElement *panel_elem = NULL, *first = NULL;
    IUIAutomationElement9 *panel9 = NULL;
    IUIAutomationElementArray *array = NULL;
    IUIAutomationCondition *cond = NULL;
    HWND top, panel, sub;
    list_t got, want;
    HRESULT hr;
    int i;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    top = CreateWindowA("static", "top", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 400, 300, 0, 0, 0, 0);
    panel = CreateWindowA("static", "Panel", WS_CHILD | WS_VISIBLE, 0, 0, 380, 250, top, 0, 0, 0);
    CreateWindowA("button", "One", WS_CHILD | WS_VISIBLE, 5, 5, 60, 20, panel, (HMENU)1, 0, 0);
    CreateWindowA("button", "Two", WS_CHILD | WS_VISIBLE, 5, 30, 60, 20, panel, (HMENU)2, 0, 0);
    sub = CreateWindowA("static", "Sub", WS_CHILD | WS_VISIBLE, 5, 60, 200, 100, panel, 0, 0, 0);
    CreateWindowA("button", "SubA", WS_CHILD | WS_VISIBLE, 5, 5, 60, 20, sub, (HMENU)3, 0, 0);
    CreateWindowA("button", "SubB", WS_CHILD | WS_VISIBLE, 5, 30, 60, 20, sub, (HMENU)4, 0, 0);
    CreateWindowA("button", "Three", WS_CHILD | WS_VISIBLE, 5, 170, 60, 20, panel, (HMENU)5, 0, 0);

    hr = CoCreateInstance(&CLSID_CUIAutomation8, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAutomation, (void **)&uia);
    check(hr == S_OK && uia, "IUIAutomation (%#lx)", hr);
    if (!uia) goto done;
    IUIAutomation_ElementFromHandle(uia, (UIA_HWND)panel, &panel_elem);
    IUIAutomation_CreateTrueCondition(uia, &cond);
    check(panel_elem && cond, "panel element");
    if (!panel_elem || !cond) goto done;
    IUIAutomationElement_QueryInterface(panel_elem, &IID_IUIAutomationElement9, (void **)&panel9);
    check(panel9 != NULL, "IUIAutomationElement9");
    if (!panel9) goto done;

    for (i = 0; i < 4; i++)
    {
        expected(panel, cases[i].post, cases[i].ltf, &want);
        array = NULL;
        hr = IUIAutomationElement9_FindAllWithOptions(panel9, TreeScope_Descendants, cond, cases[i].opts, NULL, &array);
        check(hr == S_OK && array, "FindAllWithOptions, %s (%#lx)", cases[i].name, hr);
        if (!array) continue;
        collect(array, &got);
        check(got.n == 6 && same(&got, &want), "%s: %d elements in the order of the windows", cases[i].name, got.n);
        if (!same(&got, &want)) { show("got", &got); show("expected", &want); }
        IUIAutomationElementArray_Release(array);

        first = NULL;
        hr = IUIAutomationElement9_FindFirstWithOptions(panel9, TreeScope_Descendants, cond, cases[i].opts, NULL, &first);
        check(hr == S_OK && first, "FindFirstWithOptions, %s (%#lx)", cases[i].name, hr);
        if (first)
        {
            BSTR name = NULL;

            IUIAutomationElement_get_CurrentName(first, &name);
            check(name && !wcscmp(name, want.names[0]), "%s: the first is %ls (expected %ls)", cases[i].name, name, want.names[0]);
            SysFreeString(name);
            IUIAutomationElement_Release(first);
        }
    }

    /* children only: no sub-panel children; the build-cache forms; bad options */
    expected(panel, 0, 1, &want);
    array = NULL;
    hr = IUIAutomationElement9_FindAllWithOptions(panel9, TreeScope_Children, cond, TreeTraversalOptions_LastToFirstOrder, NULL, &array);
    check(hr == S_OK && array, "children, last to first (%#lx)", hr);
    if (array)
    {
        collect(array, &got);
        check(got.n == 4 && !wcscmp(got.names[0], want.names[0]), "children: %d elements, first %ls", got.n, got.names[0]);
        IUIAutomationElementArray_Release(array);
    }
    hr = IUIAutomationElement9_FindAllWithOptions(panel9, TreeScope_Children, cond, 8, NULL, &array);
    check(hr == E_INVALIDARG, "an unknown option is E_INVALIDARG (%#lx)", hr);
    hr = IUIAutomationElement9_FindFirstWithOptions(panel9, TreeScope_Children, cond, 0, NULL, NULL);
    check(hr == E_POINTER, "FindFirstWithOptions(NULL) is E_POINTER (%#lx)", hr);
    {
        IUIAutomationCacheRequest *req = NULL;

        IUIAutomation_CreateCacheRequest(uia, &req);
        array = NULL;
        hr = IUIAutomationElement9_FindAllWithOptionsBuildCache(panel9, TreeScope_Descendants, cond, req,
                TreeTraversalOptions_PostOrder, NULL, &array);
        check(hr == S_OK && array, "FindAllWithOptionsBuildCache, post order (%#lx)", hr);
        if (array) { collect(&array ? array : NULL, &got); expected(panel, 1, 0, &want); check(same(&got, &want), "build cache: same order"); IUIAutomationElementArray_Release(array); }
        first = NULL;
        hr = IUIAutomationElement9_FindFirstWithOptionsBuildCache(panel9, TreeScope_Descendants, cond, req,
                TreeTraversalOptions_PostOrder, NULL, &first);
        check(hr == S_OK && first, "FindFirstWithOptionsBuildCache, post order (%#lx)", hr);
        if (first) IUIAutomationElement_Release(first);
        if (req) IUIAutomationCacheRequest_Release(req);
    }

    /* a captioned window: its non-client parts (title bar...) are children after its child
     * windows; walking last to first goes through the parts and on to the child windows
     * (5917: a part's parent is the non-client provider), and no part is the window's client */
    {
        HWND cap = CreateWindowA("static", "Cap", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 300, 200, 0, 0, 0, 0);
        IUIAutomationElement *cap_elem = NULL;
        IUIAutomationElement9 *cap9 = NULL;
        int k, has_btn = 0;

        CreateWindowA("button", "CapBtn", WS_CHILD | WS_VISIBLE, 5, 5, 60, 20, cap, (HMENU)9, 0, 0);
        IUIAutomation_ElementFromHandle(uia, (UIA_HWND)cap, &cap_elem);
        if (cap_elem) IUIAutomationElement_QueryInterface(cap_elem, &IID_IUIAutomationElement9, (void **)&cap9);
        array = NULL;
        if (cap9) IUIAutomationElement9_FindAllWithOptions(cap9, TreeScope_Children, cond, TreeTraversalOptions_LastToFirstOrder, NULL, &array);
        check(array != NULL, "captioned window: children last to first");
        if (array)
        {
            collect(array, &got);
            show("got", &got);
            for (k = 0; k < got.n; k++)
            {
                if (!wcscmp(got.names[k], L"CapBtn")) has_btn++;
            }
            check(got.n > 1 && has_btn == 1 && !wcscmp(got.names[got.n - 1], L"CapBtn"),
                  "captioned window: the child window comes last, after the non-client parts");
            IUIAutomationElementArray_Release(array);
        }
        if (cap9) IUIAutomationElement9_Release(cap9);
        if (cap_elem) IUIAutomationElement_Release(cap_elem);
        DestroyWindow(cap);
    }

    {
        BOOL (WINAPI *get_desc)(BSTR *) = (void *)GetProcAddress(GetModuleHandleA("uiautomationcore.dll"), "UiaGetErrorDescription");
        ICreateErrorInfo *create = NULL;
        IErrorInfo *info = NULL;
        BSTR text = NULL;

        check(get_desc != NULL, "UiaGetErrorDescription is exported");
        if (get_desc)
        {
            check(!get_desc(NULL), "UiaGetErrorDescription(NULL) is FALSE");
            SetErrorInfo(0, NULL);
            check(!get_desc(&text) && !text, "no error object: FALSE");
            CreateErrorInfo(&create);
            ICreateErrorInfo_SetDescription(create, (LPOLESTR)L"boom");
            ICreateErrorInfo_QueryInterface(create, &IID_IErrorInfo, (void **)&info);
            SetErrorInfo(0, info);
            IErrorInfo_Release(info);
            ICreateErrorInfo_Release(create);
            check(get_desc(&text) && text && !wcscmp(text, L"boom"), "the description of the error object is returned");
            SysFreeString(text);
            text = NULL;
            check(!get_desc(&text), "the error object is taken: FALSE the second time");
        }
    }

done:
    if (panel9) IUIAutomationElement9_Release(panel9);
    if (cond) IUIAutomationCondition_Release(cond);
    if (panel_elem) IUIAutomationElement_Release(panel_elem);
    if (uia) IUIAutomation_Release(uia);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
