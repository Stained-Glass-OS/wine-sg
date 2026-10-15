/* UI Automation recheck (patch 3261): IEnumVARIANT::Clone of the bridge's selection, the runtime id of an MSAA
 * proxy provider, run by test/fastuiarc-gate.sh on Xvfb. Built on the probe of test/uiabridge-gate.sh.
 *
 * This process owns a window whose WM_GETOBJECT answers every object id with
 * UiaReturnRawElementProvider, its provider a fragment root with two
 * children: a button (Invoke) and a check box (Toggle). A copy of this
 * program ("client") asks for the window's OBJID_CLIENT IAccessible, as an
 * MSAA client does, and finds the elements through it: names, roles,
 * states, children, navigation, locations, hit testing, focus, the default
 * actions, and UIA_PFIA_UNWRAP_BRIDGE. UiaReturnRawElementProvider answered
 * OBJID_CLIENT with nothing (a FIXME). */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <oleacc.h>
#include <uiautomation.h>
#include <stdio.h>
#include <stddef.h>

#ifndef UIA_PFIA_UNWRAP_BRIDGE
#define UIA_PFIA_UNWRAP_BRIDGE 1
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

/* ---------------- the provider: root, button, check box ---------------- */

static HWND main_hwnd;

struct elem
{
    IRawElementProviderSimple simple;
    IRawElementProviderFragment frag;
    IRawElementProviderFragmentRoot root;
    IInvokeProvider invoke;
    IToggleProvider toggle;
    ISelectionProvider selection;
    IRawElementProviderSimple2 simple2;
    int id;              /* 0 root, 1 button, 2 check box */
    RECT rect;           /* client coordinates */
};

static struct elem elems[3];
static int presses, focused = 1, menus;
static enum ToggleState toggle_state = ToggleState_Off;

#define FROM(iface, field) ((struct elem *)((char *)(iface) - offsetof(struct elem, field)))

static HRESULT elem_qi(struct elem *e, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IRawElementProviderSimple)) *ppv = &e->simple;
    else if (IsEqualIID(riid, &IID_IRawElementProviderFragment)) *ppv = &e->frag;
    else if (IsEqualIID(riid, &IID_IRawElementProviderSimple2) && e->id == 0) *ppv = &e->simple2;
    else if (IsEqualIID(riid, &IID_IRawElementProviderFragmentRoot) && e->id == 0) *ppv = &e->root;
    else { *ppv = NULL; return E_NOINTERFACE; }
    return S_OK;
}

static HRESULT WINAPI s_QueryInterface(IRawElementProviderSimple *i, REFIID r, void **p) { return elem_qi(FROM(i, simple), r, p); }
static ULONG WINAPI s_AddRef(IRawElementProviderSimple *i) { return 2; }
static ULONG WINAPI s_Release(IRawElementProviderSimple *i) { return 1; }
static HRESULT WINAPI s_get_ProviderOptions(IRawElementProviderSimple *i, enum ProviderOptions *ret)
{
    *ret = ProviderOptions_ServerSideProvider;
    return S_OK;
}
static HRESULT WINAPI s_GetPatternProvider(IRawElementProviderSimple *i, PATTERNID id, IUnknown **ret)
{
    struct elem *e = FROM(i, simple);
    *ret = NULL;
    if (e->id == 1 && id == UIA_InvokePatternId) *ret = (IUnknown *)&e->invoke;
    if (e->id == 2 && id == UIA_TogglePatternId) *ret = (IUnknown *)&e->toggle;
    if (e->id == 0 && id == UIA_SelectionPatternId) *ret = (IUnknown *)&e->selection;
    return S_OK;
}
static HRESULT WINAPI s_GetPropertyValue(IRawElementProviderSimple *i, PROPERTYID id, VARIANT *ret)
{
    static const WCHAR *names[] = { L"SG bridge pane", L"Save", L"Remember me" };
    struct elem *e = FROM(i, simple);
    WCHAR buf[64];

    VariantInit(ret);
    switch (id)
    {
    case UIA_NamePropertyId:
        if (e->id == 1 && presses) swprintf(buf, 64, L"Saved %d", presses);
        else if (e->id == 0 && menus) swprintf(buf, 64, L"Menu %d", menus);
        else lstrcpyW(buf, names[e->id]);
        V_VT(ret) = VT_BSTR;
        V_BSTR(ret) = SysAllocString(buf);
        break;
    case UIA_ControlTypePropertyId:
        V_VT(ret) = VT_I4;
        V_I4(ret) = e->id == 0 ? UIA_PaneControlTypeId : e->id == 1 ? UIA_ButtonControlTypeId : UIA_CheckBoxControlTypeId;
        break;
    case UIA_HelpTextPropertyId:
        if (e->id == 1) { V_VT(ret) = VT_BSTR; V_BSTR(ret) = SysAllocString(L"Saves the file"); }
        break;
    case UIA_AccessKeyPropertyId:
        if (e->id == 1) { V_VT(ret) = VT_BSTR; V_BSTR(ret) = SysAllocString(L"Alt+S"); }
        break;
    case UIA_IsKeyboardFocusablePropertyId:
        V_VT(ret) = VT_BOOL;
        V_BOOL(ret) = e->id ? VARIANT_TRUE : VARIANT_FALSE;
        break;
    case UIA_HasKeyboardFocusPropertyId:
        V_VT(ret) = VT_BOOL;
        V_BOOL(ret) = e->id && e->id == focused ? VARIANT_TRUE : VARIANT_FALSE;
        break;
    case UIA_IsEnabledPropertyId:
        V_VT(ret) = VT_BOOL;
        V_BOOL(ret) = VARIANT_TRUE;
        break;
    }
    return S_OK;
}
static HRESULT WINAPI s_get_HostRawElementProvider(IRawElementProviderSimple *i, IRawElementProviderSimple **ret)
{
    if (FROM(i, simple)->id == 0) return UiaHostProviderFromHwnd(main_hwnd, ret);
    *ret = NULL;
    return S_OK;
}
static IRawElementProviderSimpleVtbl s_vtbl = { s_QueryInterface, s_AddRef, s_Release, s_get_ProviderOptions,
    s_GetPatternProvider, s_GetPropertyValue, s_get_HostRawElementProvider };

static HRESULT WINAPI f_QueryInterface(IRawElementProviderFragment *i, REFIID r, void **p) { return elem_qi(FROM(i, frag), r, p); }
static ULONG WINAPI f_AddRef(IRawElementProviderFragment *i) { return 2; }
static ULONG WINAPI f_Release(IRawElementProviderFragment *i) { return 1; }
static HRESULT WINAPI f_Navigate(IRawElementProviderFragment *i, enum NavigateDirection dir, IRawElementProviderFragment **ret)
{
    struct elem *e = FROM(i, frag);
    int to = -1;

    *ret = NULL;
    switch (dir)
    {
    case NavigateDirection_Parent: if (e->id) to = 0; break;
    case NavigateDirection_FirstChild: if (!e->id) to = 1; break;
    case NavigateDirection_LastChild: if (!e->id) to = 2; break;
    case NavigateDirection_NextSibling: if (e->id == 1) to = 2; break;
    case NavigateDirection_PreviousSibling: if (e->id == 2) to = 1; break;
    }
    if (to >= 0) *ret = &elems[to].frag;
    return S_OK;
}
static HRESULT WINAPI f_GetRuntimeId(IRawElementProviderFragment *i, SAFEARRAY **ret)
{
    struct elem *e = FROM(i, frag);
    LONG idx;
    int v;

    *ret = NULL;
    if (!e->id) return S_OK;
    *ret = SafeArrayCreateVector(VT_I4, 0, 2);
    idx = 0; v = UiaAppendRuntimeId; SafeArrayPutElement(*ret, &idx, &v);
    idx = 1; v = e->id; SafeArrayPutElement(*ret, &idx, &v);
    return S_OK;
}
static HRESULT WINAPI f_get_BoundingRectangle(IRawElementProviderFragment *i, struct UiaRect *ret)
{
    struct elem *e = FROM(i, frag);
    POINT pt = { e->rect.left, e->rect.top };

    ClientToScreen(main_hwnd, &pt);
    ret->left = pt.x;
    ret->top = pt.y;
    ret->width = e->rect.right - e->rect.left;
    ret->height = e->rect.bottom - e->rect.top;
    return S_OK;
}
static HRESULT WINAPI f_GetEmbeddedFragmentRoots(IRawElementProviderFragment *i, SAFEARRAY **ret) { *ret = NULL; return S_OK; }
static HRESULT WINAPI f_SetFocus(IRawElementProviderFragment *i)
{
    focused = FROM(i, frag)->id;
    return S_OK;
}
static HRESULT WINAPI f_get_FragmentRoot(IRawElementProviderFragment *i, IRawElementProviderFragmentRoot **ret)
{
    *ret = &elems[0].root;
    return S_OK;
}
static IRawElementProviderFragmentVtbl f_vtbl = { f_QueryInterface, f_AddRef, f_Release, f_Navigate, f_GetRuntimeId,
    f_get_BoundingRectangle, f_GetEmbeddedFragmentRoots, f_SetFocus, f_get_FragmentRoot };

static HRESULT WINAPI r_QueryInterface(IRawElementProviderFragmentRoot *i, REFIID r, void **p) { return elem_qi(FROM(i, root), r, p); }
static ULONG WINAPI r_AddRef(IRawElementProviderFragmentRoot *i) { return 2; }
static ULONG WINAPI r_Release(IRawElementProviderFragmentRoot *i) { return 1; }
static HRESULT WINAPI r_ElementProviderFromPoint(IRawElementProviderFragmentRoot *i, double x, double y,
        IRawElementProviderFragment **ret)
{
    POINT pt = { (LONG)x, (LONG)y };
    int n;

    ScreenToClient(main_hwnd, &pt);
    *ret = &elems[0].frag;
    for (n = 1; n < 3; n++) if (PtInRect(&elems[n].rect, pt)) *ret = &elems[n].frag;
    return S_OK;
}
static HRESULT WINAPI r_GetFocus(IRawElementProviderFragmentRoot *i, IRawElementProviderFragment **ret)
{
    *ret = focused ? &elems[focused].frag : NULL;
    return S_OK;
}
static IRawElementProviderFragmentRootVtbl r_vtbl = { r_QueryInterface, r_AddRef, r_Release,
    r_ElementProviderFromPoint, r_GetFocus };

static HRESULT WINAPI inv_QueryInterface(IInvokeProvider *i, REFIID r, void **p)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IInvokeProvider)) { *p = i; return S_OK; }
    *p = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI inv_AddRef(IInvokeProvider *i) { return 2; }
static ULONG WINAPI inv_Release(IInvokeProvider *i) { return 1; }
static HRESULT WINAPI inv_Invoke(IInvokeProvider *i) { presses++; return S_OK; }
static IInvokeProviderVtbl inv_vtbl = { inv_QueryInterface, inv_AddRef, inv_Release, inv_Invoke };

static HRESULT WINAPI tog_QueryInterface(IToggleProvider *i, REFIID r, void **p)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IToggleProvider)) { *p = i; return S_OK; }
    *p = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI tog_AddRef(IToggleProvider *i) { return 2; }
static ULONG WINAPI tog_Release(IToggleProvider *i) { return 1; }
static HRESULT WINAPI tog_Toggle(IToggleProvider *i)
{
    toggle_state = toggle_state == ToggleState_On ? ToggleState_Off : ToggleState_On;
    return S_OK;
}
static HRESULT WINAPI tog_get_ToggleState(IToggleProvider *i, enum ToggleState *ret) { *ret = toggle_state; return S_OK; }
static IToggleProviderVtbl tog_vtbl = { tog_QueryInterface, tog_AddRef, tog_Release, tog_Toggle, tog_get_ToggleState };

static HRESULT WINAPI s2_QueryInterface(IRawElementProviderSimple2 *i, REFIID r, void **p) { return elem_qi(FROM(i, simple2), r, p); }
static ULONG WINAPI s2_AddRef(IRawElementProviderSimple2 *i) { return 2; }
static ULONG WINAPI s2_Release(IRawElementProviderSimple2 *i) { return 1; }
static HRESULT WINAPI s2_get_ProviderOptions(IRawElementProviderSimple2 *i, enum ProviderOptions *ret)
{
    *ret = ProviderOptions_ServerSideProvider;
    return S_OK;
}
static HRESULT WINAPI s2_GetPatternProvider(IRawElementProviderSimple2 *i, PATTERNID id, IUnknown **ret)
{
    return s_GetPatternProvider(&FROM(i, simple2)->simple, id, ret);
}
static HRESULT WINAPI s2_GetPropertyValue(IRawElementProviderSimple2 *i, PROPERTYID id, VARIANT *ret)
{
    return s_GetPropertyValue(&FROM(i, simple2)->simple, id, ret);
}
static HRESULT WINAPI s2_get_HostRawElementProvider(IRawElementProviderSimple2 *i, IRawElementProviderSimple **ret)
{
    return s_get_HostRawElementProvider(&FROM(i, simple2)->simple, ret);
}
static HRESULT WINAPI s2_ShowContextMenu(IRawElementProviderSimple2 *i) { menus++; return S_OK; }
static IRawElementProviderSimple2Vtbl s2_vtbl = { s2_QueryInterface, s2_AddRef, s2_Release, s2_get_ProviderOptions,
    s2_GetPatternProvider, s2_GetPropertyValue, s2_get_HostRawElementProvider, s2_ShowContextMenu };

static HRESULT WINAPI sel_QueryInterface(ISelectionProvider *i, REFIID r, void **p)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_ISelectionProvider)) { *p = i; return S_OK; }
    *p = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI sel_AddRef(ISelectionProvider *i) { return 2; }
static ULONG WINAPI sel_Release(ISelectionProvider *i) { return 1; }
static HRESULT WINAPI sel_GetSelection(ISelectionProvider *i, SAFEARRAY **ret)
{
    SAFEARRAY *sa = SafeArrayCreateVector(VT_UNKNOWN, 0, 2);
    LONG idx;

    for (idx = 0; idx < 2; idx++) SafeArrayPutElement(sa, &idx, (IUnknown *)&elems[idx + 1].simple);
    *ret = sa;
    return S_OK;
}
static HRESULT WINAPI sel_get_CanSelectMultiple(ISelectionProvider *i, BOOL *ret) { *ret = TRUE; return S_OK; }
static HRESULT WINAPI sel_get_IsSelectionRequired(ISelectionProvider *i, BOOL *ret) { *ret = FALSE; return S_OK; }
static ISelectionProviderVtbl sel_vtbl = { sel_QueryInterface, sel_AddRef, sel_Release, sel_GetSelection,
    sel_get_CanSelectMultiple, sel_get_IsSelectionRequired };

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_GETOBJECT)
        return UiaReturnRawElementProvider(hwnd, wp, lp, &elems[0].simple);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ---------------- the client: an MSAA view of it ---------------- */

static BSTR name_of(IAccessible *acc)
{
    VARIANT self;
    BSTR s = NULL;
    V_VT(&self) = VT_I4;
    V_I4(&self) = CHILDID_SELF;
    IAccessible_get_accName(acc, self, &s);
    return s;
}

static int is_name(IAccessible *acc, const WCHAR *want)
{
    BSTR s = name_of(acc);
    int ok = s && !lstrcmpW(s, want);
    if (!ok) printf("      name: %ls\n", s ? s : L"(none)");
    SysFreeString(s);
    return ok;
}

static IAccessible *acc_from_variant(VARIANT *v)
{
    IAccessible *acc = NULL;
    if (V_VT(v) == VT_DISPATCH && V_DISPATCH(v))
        IDispatch_QueryInterface(V_DISPATCH(v), &IID_IAccessible, (void **)&acc);
    VariantClear(v);
    return acc;
}

static int client(HWND hwnd)
{
    IAccessible *root = NULL, *button = NULL, *box = NULL, *acc;
    IDispatch *disp = NULL;
    IRawElementProviderSimple *elprov = NULL;
    IServiceProvider *sp;
    VARIANT self, v, child;
    LONG count = 0, l, t, w, h;
    BSTR s = NULL;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    V_VT(&self) = VT_I4;
    V_I4(&self) = CHILDID_SELF;

    hr = AccessibleObjectFromWindow(hwnd, OBJID_CLIENT, &IID_IAccessible, (void **)&root);
    check(hr == S_OK && root, "AccessibleObjectFromWindow(OBJID_CLIENT): the bridge's IAccessible");
    if (!root) return 1;
    check(is_name(root, L"SG bridge pane"), "its name is the provider's Name");
    VariantInit(&v);
    check(IAccessible_get_accRole(root, self, &v) == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == ROLE_SYSTEM_PANE,
          "a pane's role: ROLE_SYSTEM_PANE");
    check(IAccessible_get_accChildCount(root, &count) == S_OK && count == 2, "two children");

    V_VT(&child) = VT_I4;
    V_I4(&child) = 1;
    hr = IAccessible_get_accChild(root, child, &disp);
    if (disp) IDispatch_QueryInterface(disp, &IID_IAccessible, (void **)&button);
    if (disp) IDispatch_Release(disp);
    check(button && is_name(button, L"Save"), "child 1: the button");
    if (!button) return 1;
    VariantInit(&v);
    IAccessible_get_accRole(button, self, &v);
    check(V_VT(&v) == VT_I4 && V_I4(&v) == ROLE_SYSTEM_PUSHBUTTON, "a button's role: ROLE_SYSTEM_PUSHBUTTON");
    VariantInit(&v);
    IAccessible_get_accState(button, self, &v);
    check(V_VT(&v) == VT_I4 && (V_I4(&v) & STATE_SYSTEM_FOCUSABLE) && (V_I4(&v) & STATE_SYSTEM_FOCUSED),
          "its state: focusable, focused");
    IAccessible_get_accHelp(button, self, &s);
    check(s && !lstrcmpW(s, L"Saves the file"), "get_accHelp: HelpText");
    SysFreeString(s); s = NULL;
    IAccessible_get_accKeyboardShortcut(button, self, &s);
    check(s && !lstrcmpW(s, L"Alt+S"), "get_accKeyboardShortcut: AccessKey");
    SysFreeString(s); s = NULL;
    IAccessible_get_accDefaultAction(button, self, &s);
    check(s && !lstrcmpW(s, L"Press"), "get_accDefaultAction: Press (Invoke)");
    SysFreeString(s); s = NULL;
    check(IAccessible_accDoDefaultAction(button, self) == S_OK && is_name(button, L"Saved 1"),
          "accDoDefaultAction invokes it");

    VariantInit(&v);
    hr = IAccessible_accNavigate(button, NAVDIR_NEXT, self, &v);
    box = acc_from_variant(&v);
    check(box && is_name(box, L"Remember me"), "accNavigate(NAVDIR_NEXT): the check box");
    if (!box) return 1;
    VariantInit(&v);
    IAccessible_get_accRole(box, self, &v);
    check(V_VT(&v) == VT_I4 && V_I4(&v) == ROLE_SYSTEM_CHECKBUTTON, "its role: ROLE_SYSTEM_CHECKBUTTON");
    check(IAccessible_accDoDefaultAction(box, self) == S_OK, "accDoDefaultAction on it");
    VariantInit(&v);
    IAccessible_get_accState(box, self, &v);
    check(V_VT(&v) == VT_I4 && (V_I4(&v) & STATE_SYSTEM_CHECKED), "toggled: STATE_SYSTEM_CHECKED");

    disp = NULL;
    IAccessible_get_accParent(box, &disp);
    acc = NULL;
    if (disp) IDispatch_QueryInterface(disp, &IID_IAccessible, (void **)&acc);
    if (disp) IDispatch_Release(disp);
    check(acc && is_name(acc, L"SG bridge pane"), "its parent: the pane");
    if (acc) IAccessible_Release(acc);

    check(IAccessible_accLocation(box, &l, &t, &w, &h, self) == S_OK && w == 100 && h == 20,
          "accLocation: the BoundingRectangle");
    VariantInit(&v);
    hr = IAccessible_accHitTest(root, l + 5, t + 5, &v);
    acc = acc_from_variant(&v);
    check(hr == S_OK && acc && is_name(acc, L"Remember me"), "accHitTest: the check box at its place");
    if (acc) IAccessible_Release(acc);

    check(IAccessible_accSelect(box, SELFLAG_TAKEFOCUS, self) == S_OK, "accSelect(SELFLAG_TAKEFOCUS)");
    VariantInit(&v);
    IAccessible_get_accFocus(root, &v);
    acc = acc_from_variant(&v);
    check(acc && is_name(acc, L"Remember me"), "get_accFocus: the check box now");
    if (acc) IAccessible_Release(acc);

    check(SUCCEEDED(IAccessible_QueryInterface(root, &IID_IServiceProvider, (void **)&sp)) &&
          SUCCEEDED(IServiceProvider_QueryService(sp, &IID_IRawElementProviderSimple, &IID_IRawElementProviderSimple,
                                                   (void **)&elprov)) && elprov,
          "IServiceProvider: the provider behind the bridge");
    if (elprov) IRawElementProviderSimple_Release(elprov);
    elprov = NULL;
    hr = UiaProviderFromIAccessible(root, CHILDID_SELF, UIA_PFIA_UNWRAP_BRIDGE, &elprov);
    if (elprov)
    {
        VARIANT n;
        VariantInit(&n);
        IRawElementProviderSimple_GetPropertyValue(elprov, UIA_NamePropertyId, &n);
        check(V_VT(&n) == VT_BSTR && !lstrcmpW(V_BSTR(&n), L"SG bridge pane"),
              "UIA_PFIA_UNWRAP_BRIDGE: the window's own provider");
        VariantClear(&n);
        IRawElementProviderSimple_Release(elprov);
    }
    else check(0, "UIA_PFIA_UNWRAP_BRIDGE: the window's own provider");


    /* the selection of the pane is an enumeration; its clone has the same state and goes on by itself */
    {
        VARIANT sel, item, item2;
        IEnumVARIANT *en = NULL, *clone = NULL;
        ULONG got = 0;

        VariantInit(&sel);
        hr = IAccessible_get_accSelection(root, &sel);
        check(hr == S_OK && V_VT(&sel) == VT_UNKNOWN && V_UNKNOWN(&sel) &&
              SUCCEEDED(IUnknown_QueryInterface(V_UNKNOWN(&sel), &IID_IEnumVARIANT, (void **)&en)),
              "get_accSelection: an enumeration");
        VariantClear(&sel);
        if (en)
        {
            VariantInit(&item);
            check(IEnumVARIANT_Next(en, 1, &item, &got) == S_OK && got == 1, "Next: the first selected element");
            acc = acc_from_variant(&item);
            check(acc && is_name(acc, L"Save") || acc && is_name(acc, L"Saved 1"), "it is the button");
            if (acc) IAccessible_Release(acc);

            hr = IEnumVARIANT_Clone(en, &clone);
            check(hr == S_OK && clone && clone != en, "Clone: a second enumeration");
            if (clone)
            {
                VariantInit(&item2);
                got = 0;
                check(IEnumVARIANT_Next(clone, 1, &item2, &got) == S_OK && got == 1, "the clone goes on at the same place");
                acc = acc_from_variant(&item2);
                check(acc && is_name(acc, L"Remember me"), "it is the check box");
                if (acc) IAccessible_Release(acc);
                got = 0;
                check(IEnumVARIANT_Next(clone, 1, &item2, &got) == S_FALSE && got == 0, "the clone is then at the end");
                /* the original did not move with it */
                VariantInit(&item2);
                got = 0;
                check(IEnumVARIANT_Next(en, 1, &item2, &got) == S_OK && got == 1, "the original is still at the second element");
                VariantClear(&item2);
                check(IEnumVARIANT_Reset(clone) == S_OK, "Reset the clone");
                got = 0;
                VariantInit(&item2);
                check(IEnumVARIANT_Next(clone, 1, &item2, &got) == S_OK && got == 1, "and it starts again");
                VariantClear(&item2);
                IEnumVARIANT_Release(clone);
            }
            IEnumVARIANT_Release(en);
        }
    }

    /* the runtime id of an element behind an MSAA proxy provider */
    {
        IRawElementProviderSimple *prov = NULL;
        IRawElementProviderFragment *frag = NULL;
        SAFEARRAY *rid = (SAFEARRAY *)1;

        hr = UiaProviderFromIAccessible(root, CHILDID_SELF, 0, &prov);
        check(hr == S_OK && prov && SUCCEEDED(IRawElementProviderSimple_QueryInterface(prov, &IID_IRawElementProviderFragment, (void **)&frag)),
              "UiaProviderFromIAccessible: the proxy of the window's root");
        if (frag)
        {
            hr = IRawElementProviderFragment_GetRuntimeId(frag, &rid);
            check(hr == S_OK && rid == NULL, "the root of the window has no runtime id of its own (NULL)");
            IRawElementProviderFragment_Release(frag);
        }
        if (prov) IRawElementProviderSimple_Release(prov);

        prov = NULL; frag = NULL; rid = NULL;
        hr = UiaProviderFromIAccessible(button, CHILDID_SELF, 0, &prov);
        check(hr == S_OK && prov && SUCCEEDED(IRawElementProviderSimple_QueryInterface(prov, &IID_IRawElementProviderFragment, (void **)&frag)),
              "UiaProviderFromIAccessible: the proxy of the button");
        if (frag)
        {
            LONG lb = 0, ub = 0, first = 0, second = 0, idx;

            hr = IRawElementProviderFragment_GetRuntimeId(frag, &rid);
            check(hr == S_OK && rid != NULL, "a child has a runtime id");
            if (rid)
            {
                SafeArrayGetLBound(rid, 1, &lb);
                SafeArrayGetUBound(rid, 1, &ub);
                check(ub - lb + 1 == 2, "of two values");
                idx = lb; SafeArrayGetElement(rid, &idx, &first);
                idx = lb + 1; SafeArrayGetElement(rid, &idx, &second);
                check(first == UiaAppendRuntimeId && second != 0, "UiaAppendRuntimeId and a value");
                SafeArrayDestroy(rid);
            }
            IRawElementProviderFragment_Release(frag);
        }
        if (prov) IRawElementProviderSimple_Release(prov);
    }

    /* IUIAutomationElement3::ShowContextMenu reaches IRawElementProviderSimple2 of the provider */
    {
        IUIAutomation *uia = NULL;
        IUIAutomationElement *el = NULL;
        IUIAutomationElement3 *el3 = NULL;
        BSTR nm = NULL;

        hr = CoCreateInstance(&CLSID_CUIAutomation8, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAutomation, (void **)&uia);
        check(hr == S_OK && uia, "CUIAutomation");
        if (uia) hr = IUIAutomation_ElementFromHandle(uia, hwnd, &el);
        if (el) hr = IUIAutomationElement_QueryInterface(el, &IID_IUIAutomationElement3, (void **)&el3);
        check(el3 != NULL, "an IUIAutomationElement3 for the window");
        if (el3)
        {
            hr = el3->lpVtbl->ShowContextMenu(el3);
            check(hr == S_OK, "ShowContextMenu succeeds");
            IUIAutomationElement_get_CurrentName(el, &nm);
            check(nm && !lstrcmpW(nm, L"Menu 1"), "the provider's ShowContextMenu ran once");
            SysFreeString(nm);
            IUIAutomationElement3_Release(el3);
        }
        if (el) IUIAutomationElement_Release(el);
        if (uia) IUIAutomation_Release(uia);
    }

    IAccessible_Release(box);
    IAccessible_Release(button);
    IAccessible_Release(root);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    return failures != 0;
}

int main(int argc, char **argv)
{
    PROCESS_INFORMATION pi;
    STARTUPINFOA si = { sizeof(si) };
    WNDCLASSW wc = { 0 };
    char cmd[MAX_PATH + 32];
    DWORD code = 1;
    MSG msg;
    int n;

    if (argc > 2 && !strcmp(argv[1], "client")) return client((HWND)(ULONG_PTR)_strtoui64(argv[2], NULL, 10));

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    for (n = 0; n < 3; n++)
    {
        elems[n].simple.lpVtbl = &s_vtbl;
        elems[n].frag.lpVtbl = &f_vtbl;
        elems[n].root.lpVtbl = &r_vtbl;
        elems[n].invoke.lpVtbl = &inv_vtbl;
        elems[n].toggle.lpVtbl = &tog_vtbl;
        elems[n].selection.lpVtbl = &sel_vtbl;
        elems[n].simple2.lpVtbl = &s2_vtbl;
        elems[n].id = n;
    }
    SetRect(&elems[0].rect, 0, 0, 300, 200);
    SetRect(&elems[1].rect, 10, 10, 90, 40);
    SetRect(&elems[2].rect, 10, 60, 110, 80);

    wc.lpfnWndProc = wnd_proc;
    wc.lpszClassName = L"SgUiaBridge";
    RegisterClassW(&wc);
    main_hwnd = CreateWindowW(L"SgUiaBridge", L"SG bridge", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 320, 240,
                              NULL, NULL, NULL, NULL);
    cmd[0] = '"';
    GetModuleFileNameA(NULL, cmd + 1, MAX_PATH);
    sprintf(cmd + strlen(cmd), "\" client %Iu", (ULONG_PTR)main_hwnd);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        printf("FAIL  the client process starts\nRESULT: FAIL\n");
        return 1;
    }
    while (MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, 60000, QS_ALLINPUT) == WAIT_OBJECT_0 + 1)
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    GetExitCodeProcess(pi.hProcess, &code);
    return code;
}
