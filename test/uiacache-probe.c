/* uiautomationcore: UiaGetUpdatedCache with the children, descendants and subtree
 * scopes (patches/sg/2626): the nodes in tree order in the array, the tree description
 * ("P" and ")" for an element, its children in parentheses), cached properties of each
 * node. The tree is a few windows; the result is compared with a walk by UiaNavigate. */
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
    char buf[512];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s  %s\n", ok ? "PASS" : "FAIL", buf);
    if (!ok) failures++;
}

static struct UiaCondition true_cond = { ConditionType_True };

static void set_req(struct UiaCacheRequest *req, int scope, int *props, int nprops)
{
    memset(req, 0, sizeof(*req));
    req->pViewCondition = &true_cond;
    req->Scope = scope;
    req->pProperties = props;
    req->cProperties = nprops;
    req->automationElementMode = AutomationElementMode_Full;
}

static HUIANODE node_from_array(SAFEARRAY *sa, LONG row)
{
    LONG idx[2] = { row, 0 };
    VARIANT v;

    VariantInit(&v);
    SafeArrayGetElement(sa, idx, &v);
    return (HUIANODE)(UINT_PTR)V_I8(&v);
}

/* the children of a node by walking: first child, next siblings */
static HUIANODE navigate(HUIANODE node, enum NavigateDirection dir)
{
    struct UiaCacheRequest req;
    SAFEARRAY *sa = NULL;
    BSTR tree = NULL;
    HUIANODE ret = NULL;
    HRESULT hr;

    set_req(&req, TreeScope_Element, NULL, 0);
    hr = UiaNavigate(node, dir, &true_cond, &req, &sa, &tree);
    if (hr == S_OK && sa)
        ret = node_from_array(sa, 0);
    if (sa) SafeArrayDestroy(sa);
    SysFreeString(tree);
    return ret;
}

static int count_children(HUIANODE node, int recurse)
{
    HUIANODE child = navigate(node, NavigateDirection_FirstChild);
    int n = 0;

    while (child)
    {
        HUIANODE next;

        ++n;
        if (recurse) n += count_children(child, 1);
        next = navigate(child, NavigateDirection_NextSibling);
        UiaNodeRelease(child);
        child = next;
    }
    return n;
}

static void release_array_nodes(SAFEARRAY *sa)
{
    LONG lb, ub, i;

    SafeArrayGetLBound(sa, 1, &lb);
    SafeArrayGetUBound(sa, 1, &ub);
    for (i = lb; i <= ub; ++i)
        UiaNodeRelease(node_from_array(sa, i));
}

static void rows_cols(SAFEARRAY *sa, int *rows, int *cols)
{
    LONG lb, ub;

    SafeArrayGetUBound(sa, 1, &ub); SafeArrayGetLBound(sa, 1, &lb); *rows = ub - lb + 1;
    SafeArrayGetUBound(sa, 2, &ub); SafeArrayGetLBound(sa, 2, &lb); *cols = ub - lb + 1;
}

int main(void)
{
    HWND top, c1, g1, c2;
    HUIANODE node = NULL;
    struct UiaCacheRequest req;
    SAFEARRAY *sa;
    BSTR tree;
    HRESULT hr;
    int children, descendants, rows, cols;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    top = CreateWindowA("static", "top", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 300, 200, 0, 0, 0, 0);
    c1 = CreateWindowA("static", "c1", WS_CHILD | WS_VISIBLE, 0, 0, 100, 100, top, 0, 0, 0);
    g1 = CreateWindowA("static", "g1", WS_CHILD | WS_VISIBLE, 0, 0, 20, 20, c1, 0, 0, 0);
    c2 = CreateWindowA("static", "c2", WS_CHILD | WS_VISIBLE, 100, 0, 100, 100, top, 0, 0, 0);
    check(top && c1 && g1 && c2, "windows");
    (void)g1; (void)c2;

    hr = UiaNodeFromHandle(top, &node);
    check(hr == S_OK && node, "node of the window (%#lx)", hr);
    if (!node) goto done;

    children = count_children(node, 0);
    descendants = count_children(node, 1);
    check(children >= 2 && descendants > children, "the walk finds %d children, %d descendants", children, descendants);

    /* the element alone */
    set_req(&req, TreeScope_Element, NULL, 0);
    sa = NULL; tree = NULL;
    hr = UiaGetUpdatedCache(node, &req, NormalizeState_None, NULL, &sa, &tree);
    check(hr == S_OK && sa && tree && !wcscmp(tree, L"P)"), "element: %ls (%#lx)", tree ? tree : L"(null)", hr);
    if (sa) { release_array_nodes(sa); SafeArrayDestroy(sa); }
    SysFreeString(tree);

    /* the children */
    set_req(&req, TreeScope_Children, NULL, 0);
    sa = NULL; tree = NULL;
    hr = UiaGetUpdatedCache(node, &req, NormalizeState_None, NULL, &sa, &tree);
    check(hr == S_OK && sa && tree, "children (%#lx)", hr);
    if (sa)
    {
        WCHAR expected[64] = L"(";
        int i;

        rows_cols(sa, &rows, &cols);
        check(rows == children && cols == 1, "children: %d rows and %d columns", rows, cols);
        for (i = 0; i < children; ++i) wcscat(expected, L"P)");
        wcscat(expected, L")");
        check(!wcscmp(tree, expected), "children: tree %ls, expected %ls", tree, expected);
        release_array_nodes(sa);
        SafeArrayDestroy(sa);
    }
    SysFreeString(tree);

    /* the descendants: the children and theirs */
    set_req(&req, TreeScope_Descendants, NULL, 0);
    sa = NULL; tree = NULL;
    hr = UiaGetUpdatedCache(node, &req, NormalizeState_None, NULL, &sa, &tree);
    check(hr == S_OK && sa && tree, "descendants (%#lx)", hr);
    if (sa)
    {
        rows_cols(sa, &rows, &cols);
        check(rows == descendants, "descendants: %d rows, the walk found %d", rows, descendants);
        check(tree[0] == '(' && tree[wcslen(tree) - 1] == ')', "descendants: tree %ls", tree);
        check(wcschr(tree, 'P') && wcsstr(tree, L"(P") != tree + 0 || wcsstr(tree, L"P("), "descendants: a child has children (%ls)", tree);
        release_array_nodes(sa);
        SafeArrayDestroy(sa);
    }
    SysFreeString(tree);

    /* the subtree, with a property of every node */
    {
        int props[1] = { UIA_ControlTypePropertyId };

        set_req(&req, TreeScope_Subtree, props, 1);
        sa = NULL; tree = NULL;
        hr = UiaGetUpdatedCache(node, &req, NormalizeState_None, NULL, &sa, &tree);
        check(hr == S_OK && sa && tree, "subtree (%#lx)", hr);
        if (sa)
        {
            rows_cols(sa, &rows, &cols);
            check(rows == descendants + 1 && cols == 2, "subtree: %d rows, %d columns", rows, cols);
            check(tree[0] == 'P' && tree[1] == '(' && tree[wcslen(tree) - 1] == ')', "subtree: tree %ls", tree);
            {
                int opens = 0, closes = 0;
                const WCHAR *p;

                for (p = tree; *p; ++p) { if (*p == '(') ++opens; if (*p == ')') ++closes; }
                check(closes == opens + rows, "subtree: the parentheses balance (%d opens, %d closes, %d nodes)", opens, closes, rows);
            }
            release_array_nodes(sa);
            SafeArrayDestroy(sa);
        }
        SysFreeString(tree);
    }

    /* a node without children has none */
    {
        HUIANODE leaf = navigate(navigate(node, NavigateDirection_FirstChild), NavigateDirection_FirstChild);

        (void)leaf;
    }

    /* scopes not supported */
    set_req(&req, TreeScope_Parent, NULL, 0);
    sa = NULL; tree = NULL;
    hr = UiaGetUpdatedCache(node, &req, NormalizeState_None, NULL, &sa, &tree);
    check(hr == E_NOTIMPL, "the parent scope = %#lx", hr);
    SysFreeString(tree);

    UiaNodeRelease(node);
done:
    DestroyWindow(top);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
