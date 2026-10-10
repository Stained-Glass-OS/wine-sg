/* uiautomationcore: the COM pattern objects of IUIAutomationElement::GetCurrentPattern(As)
 * (patches/sg/2632), against a provider that logs what its pattern interfaces are called with
 * and gives the pattern properties. The element is that of a window that answers WM_GETOBJECT
 * with the provider. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ole2.h>
#include <oleacc.h>
#include <uiautomation.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

static char logbuf[1024];
static int failures;
static char logbuf[1024];

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

static void logf_(const char *fmt, ...)
{
    size_t len = strlen(logbuf);
    va_list args;

    va_start(args, fmt);
    vsnprintf(logbuf + len, sizeof(logbuf) - len, fmt, args);
    va_end(args);
}

/* ---- one object that is the element and every pattern provider ---- */

struct mock
{
    IRawElementProviderSimple simple;
    IRawElementProviderFragment frag;
    IInvokeProvider invoke;
    IExpandCollapseProvider expcol;
    IToggleProvider toggle;
    IValueProvider value;
    IRangeValueProvider range;
    ISelectionItemProvider selitem;
    IScrollItemProvider scrollitem;
    IScrollProvider scroll;
    IDockProvider dock;
    ITransformProvider transform;
    IWindowProvider window;
    IVirtualizedItemProvider virt;
    IMultipleViewProvider multiview;
    ISynchronizedInputProvider sync;
    ILegacyIAccessibleProvider legacy;
    LONG ref;
    unsigned supported;       /* bit n: pattern 10000+n is available */
    BOOL has_fragment;
    HRESULT fail;             /* result of the next pattern method */
    IAccessible *acc;
};

#define PAT(id) (1u << ((id) - 10000))

static struct mock g_mock;

#define FROM(type, member) static inline struct mock *from_##member(type *iface) \
    { return CONTAINING_RECORD(iface, struct mock, member); }
#define CONTAINING_RECORD2(addr, type, field) ((type *)((char *)(addr) - offsetof(type, field)))
#undef FROM
#define FROM(type, member) static inline struct mock *from_##member(type *iface) \
    { return CONTAINING_RECORD2(iface, struct mock, member); }
FROM(IRawElementProviderSimple, simple)
FROM(IRawElementProviderFragment, frag)
FROM(IInvokeProvider, invoke)
FROM(IExpandCollapseProvider, expcol)
FROM(IToggleProvider, toggle)
FROM(IValueProvider, value)
FROM(IRangeValueProvider, range)
FROM(ISelectionItemProvider, selitem)
FROM(IScrollItemProvider, scrollitem)
FROM(IScrollProvider, scroll)
FROM(IDockProvider, dock)
FROM(ITransformProvider, transform)
FROM(IWindowProvider, window)
FROM(IVirtualizedItemProvider, virt)
FROM(IMultipleViewProvider, multiview)
FROM(ISynchronizedInputProvider, sync)
FROM(ILegacyIAccessibleProvider, legacy)

static HRESULT mock_qi(struct mock *m, REFIID riid, void **ppv)
{
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IRawElementProviderSimple)) *ppv = &m->simple;
    else if (IsEqualIID(riid, &IID_IRawElementProviderFragment) && m->has_fragment) *ppv = &m->frag;
    else if (IsEqualIID(riid, &IID_IInvokeProvider)) *ppv = &m->invoke;
    else if (IsEqualIID(riid, &IID_IExpandCollapseProvider)) *ppv = &m->expcol;
    else if (IsEqualIID(riid, &IID_IToggleProvider)) *ppv = &m->toggle;
    else if (IsEqualIID(riid, &IID_IValueProvider)) *ppv = &m->value;
    else if (IsEqualIID(riid, &IID_IRangeValueProvider)) *ppv = &m->range;
    else if (IsEqualIID(riid, &IID_ISelectionItemProvider)) *ppv = &m->selitem;
    else if (IsEqualIID(riid, &IID_IScrollItemProvider)) *ppv = &m->scrollitem;
    else if (IsEqualIID(riid, &IID_IScrollProvider)) *ppv = &m->scroll;
    else if (IsEqualIID(riid, &IID_IDockProvider)) *ppv = &m->dock;
    else if (IsEqualIID(riid, &IID_ITransformProvider)) *ppv = &m->transform;
    else if (IsEqualIID(riid, &IID_IWindowProvider)) *ppv = &m->window;
    else if (IsEqualIID(riid, &IID_IVirtualizedItemProvider)) *ppv = &m->virt;
    else if (IsEqualIID(riid, &IID_IMultipleViewProvider)) *ppv = &m->multiview;
    else if (IsEqualIID(riid, &IID_ISynchronizedInputProvider)) *ppv = &m->sync;
    else if (IsEqualIID(riid, &IID_ILegacyIAccessibleProvider)) *ppv = &m->legacy;
    else return E_NOINTERFACE;
    InterlockedIncrement(&m->ref);
    return S_OK;
}

#define IUNK(name, type) \
    static HRESULT WINAPI name##_QI(type *iface, REFIID riid, void **ppv) { return mock_qi(from_##name(iface), riid, ppv); } \
    static ULONG WINAPI name##_AddRef(type *iface) { return InterlockedIncrement(&from_##name(iface)->ref); } \
    static ULONG WINAPI name##_Release(type *iface) { return InterlockedDecrement(&from_##name(iface)->ref); }

IUNK(simple, IRawElementProviderSimple)
IUNK(frag, IRawElementProviderFragment)
IUNK(invoke, IInvokeProvider)
IUNK(expcol, IExpandCollapseProvider)
IUNK(toggle, IToggleProvider)
IUNK(value, IValueProvider)
IUNK(range, IRangeValueProvider)
IUNK(selitem, ISelectionItemProvider)
IUNK(scrollitem, IScrollItemProvider)
IUNK(scroll, IScrollProvider)
IUNK(dock, IDockProvider)
IUNK(transform, ITransformProvider)
IUNK(window, IWindowProvider)
IUNK(virt, IVirtualizedItemProvider)
IUNK(multiview, IMultipleViewProvider)
IUNK(sync, ISynchronizedInputProvider)
IUNK(legacy, ILegacyIAccessibleProvider)

static HRESULT result(struct mock *m)
{
    HRESULT hr = m->fail;
    m->fail = S_OK;
    return hr;
}

static HRESULT WINAPI simple_get_ProviderOptions(IRawElementProviderSimple *iface, enum ProviderOptions *ret)
{
    *ret = ProviderOptions_ServerSideProvider;
    return S_OK;
}

static HRESULT WINAPI simple_GetPatternProvider(IRawElementProviderSimple *iface, PATTERNID id, IUnknown **ret)
{
    struct mock *m = from_simple(iface);

    *ret = NULL;
    if (id >= 10000 && id < 10032 && (m->supported & PAT(id)))
    {
        *ret = (IUnknown *)&m->simple;
        InterlockedIncrement(&m->ref);
    }
    return S_OK;
}

static HRESULT WINAPI simple_GetPropertyValue(IRawElementProviderSimple *iface, PROPERTYID id, VARIANT *ret)
{
    VariantInit(ret);
#define I4(p, x) case p: V_VT(ret) = VT_I4; V_I4(ret) = (x); break;
#define R8(p, x) case p: V_VT(ret) = VT_R8; V_R8(ret) = (x); break;
#define BL(p, x) case p: V_VT(ret) = VT_BOOL; V_BOOL(ret) = (x) ? VARIANT_TRUE : VARIANT_FALSE; break;
#define BS(p, x) case p: V_VT(ret) = VT_BSTR; V_BSTR(ret) = SysAllocString(x); break;
    switch (id)
    {
    I4(UIA_ExpandCollapseExpandCollapseStatePropertyId, 2)
    I4(UIA_ToggleToggleStatePropertyId, 1)
    BS(UIA_ValueValuePropertyId, L"the value")
    BL(UIA_ValueIsReadOnlyPropertyId, TRUE)
    R8(UIA_RangeValueValuePropertyId, 5.5)
    BL(UIA_RangeValueIsReadOnlyPropertyId, FALSE)
    R8(UIA_RangeValueLargeChangePropertyId, 10.0)
    R8(UIA_RangeValueSmallChangePropertyId, 1.0)
    R8(UIA_RangeValueMaximumPropertyId, 100.0)
    R8(UIA_RangeValueMinimumPropertyId, -100.0)
    BL(UIA_SelectionItemIsSelectedPropertyId, TRUE)
    R8(UIA_ScrollHorizontalScrollPercentPropertyId, 12.5)
    R8(UIA_ScrollVerticalScrollPercentPropertyId, 25.0)
    R8(UIA_ScrollHorizontalViewSizePropertyId, 50.0)
    R8(UIA_ScrollVerticalViewSizePropertyId, 75.0)
    BL(UIA_ScrollHorizontallyScrollablePropertyId, TRUE)
    BL(UIA_ScrollVerticallyScrollablePropertyId, FALSE)
    I4(UIA_DockDockPositionPropertyId, 3)
    BL(UIA_TransformCanMovePropertyId, TRUE)
    BL(UIA_TransformCanResizePropertyId, FALSE)
    BL(UIA_TransformCanRotatePropertyId, TRUE)
    BL(UIA_WindowCanMaximizePropertyId, TRUE)
    BL(UIA_WindowCanMinimizePropertyId, FALSE)
    BL(UIA_WindowIsModalPropertyId, TRUE)
    BL(UIA_WindowIsTopmostPropertyId, FALSE)
    I4(UIA_WindowWindowVisualStatePropertyId, 1)
    I4(UIA_WindowWindowInteractionStatePropertyId, 2)
    I4(UIA_MultipleViewCurrentViewPropertyId, 4)
    case UIA_MultipleViewSupportedViewsPropertyId:
    {
        int vals[3] = {1, 2, 3};
        LONG i;

        V_VT(ret) = VT_I4 | VT_ARRAY;
        V_ARRAY(ret) = SafeArrayCreateVector(VT_I4, 0, 3);
        for (i = 0; i < 3; ++i) SafeArrayPutElement(V_ARRAY(ret), &i, &vals[i]);
        break;
    }
    I4(UIA_LegacyIAccessibleChildIdPropertyId, 7)
    BS(UIA_LegacyIAccessibleNamePropertyId, L"legacy name")
    BS(UIA_LegacyIAccessibleValuePropertyId, L"legacy value")
    BS(UIA_LegacyIAccessibleDescriptionPropertyId, L"legacy description")
    I4(UIA_LegacyIAccessibleRolePropertyId, 42)
    I4(UIA_LegacyIAccessibleStatePropertyId, 0x20)
    BS(UIA_LegacyIAccessibleHelpPropertyId, L"legacy help")
    BS(UIA_LegacyIAccessibleKeyboardShortcutPropertyId, L"Ctrl+L")
    BS(UIA_LegacyIAccessibleDefaultActionPropertyId, L"legacy click")
    }
    return S_OK;
}

static HRESULT WINAPI simple_get_HostRawElementProvider(IRawElementProviderSimple *iface, IRawElementProviderSimple **ret)
{
    *ret = NULL;
    return S_OK;
}

static IRawElementProviderSimpleVtbl simple_vtbl = {
    simple_QI, simple_AddRef, simple_Release, simple_get_ProviderOptions, simple_GetPatternProvider,
    simple_GetPropertyValue, simple_get_HostRawElementProvider,
};

static HRESULT WINAPI frag_Navigate(IRawElementProviderFragment *iface, enum NavigateDirection dir, IRawElementProviderFragment **ret)
{
    *ret = NULL;
    return S_OK;
}

static HRESULT WINAPI frag_GetRuntimeId(IRawElementProviderFragment *iface, SAFEARRAY **ret)
{
    int vals[2] = { UiaAppendRuntimeId, 77 };
    LONG i;

    *ret = SafeArrayCreateVector(VT_I4, 0, 2);
    for (i = 0; i < 2; i++) SafeArrayPutElement(*ret, &i, &vals[i]);
    return S_OK;
}

static HRESULT WINAPI frag_get_BoundingRectangle(IRawElementProviderFragment *iface, struct UiaRect *ret)
{
    memset(ret, 0, sizeof(*ret));
    return S_OK;
}

static HRESULT WINAPI frag_GetEmbeddedFragmentRoots(IRawElementProviderFragment *iface, SAFEARRAY **ret)
{
    *ret = NULL;
    return S_OK;
}

static HRESULT WINAPI frag_SetFocus(IRawElementProviderFragment *iface)
{
    logf_("SetFocus;");
    return result(from_frag(iface));
}

static HRESULT WINAPI frag_get_FragmentRoot(IRawElementProviderFragment *iface, IRawElementProviderFragmentRoot **ret)
{
    *ret = NULL;
    return S_OK;
}

static IRawElementProviderFragmentVtbl frag_vtbl = {
    frag_QI, frag_AddRef, frag_Release, frag_Navigate, frag_GetRuntimeId, frag_get_BoundingRectangle,
    frag_GetEmbeddedFragmentRoots, frag_SetFocus, frag_get_FragmentRoot,
};

#define METHOD0(fn, member, type, text) \
    static HRESULT WINAPI fn(type *iface) { logf_(text ";"); return result(from_##member(iface)); }

METHOD0(invoke_Invoke, invoke, IInvokeProvider, "Invoke")
METHOD0(expcol_Expand, expcol, IExpandCollapseProvider, "Expand")
METHOD0(expcol_Collapse, expcol, IExpandCollapseProvider, "Collapse")
METHOD0(toggle_Toggle, toggle, IToggleProvider, "Toggle")
METHOD0(selitem_Select, selitem, ISelectionItemProvider, "Select")
METHOD0(selitem_Add, selitem, ISelectionItemProvider, "AddToSelection")
METHOD0(selitem_Remove, selitem, ISelectionItemProvider, "RemoveFromSelection")
METHOD0(scrollitem_ScrollIntoView, scrollitem, IScrollItemProvider, "ScrollIntoView")
METHOD0(window_Close, window, IWindowProvider, "Close")
METHOD0(virt_Realize, virt, IVirtualizedItemProvider, "Realize")
METHOD0(sync_Cancel, sync, ISynchronizedInputProvider, "SyncCancel")
METHOD0(legacy_DoDefaultAction, legacy, ILegacyIAccessibleProvider, "DoDefaultAction")

static HRESULT mock_prop(PROPERTYID id, VARIANT *v)
{
    VariantInit(v);
    return simple_GetPropertyValue(NULL, id, v);
}
static HRESULT WINAPI expcol_get_ExpandCollapseState(IExpandCollapseProvider *iface, enum ExpandCollapseState *ret)
{
    VARIANT v;

    mock_prop(UIA_ExpandCollapseExpandCollapseStatePropertyId, &v);
    *ret = V_I4(&v);
    return S_OK;
}
static HRESULT WINAPI toggle_get_ToggleState(IToggleProvider *iface, enum ToggleState *ret)
{
    VARIANT v;

    mock_prop(UIA_ToggleToggleStatePropertyId, &v);
    *ret = V_I4(&v);
    return S_OK;
}
static HRESULT WINAPI value_get_Value(IValueProvider *iface, BSTR *ret)
{
    VARIANT v;

    mock_prop(UIA_ValueValuePropertyId, &v);
    *ret = V_BSTR(&v);
    return S_OK;
}
static HRESULT WINAPI value_get_IsReadOnly(IValueProvider *iface, BOOL *ret)
{
    VARIANT v;

    mock_prop(UIA_ValueIsReadOnlyPropertyId, &v);
    *ret = V_BOOL(&v) != VARIANT_FALSE;
    return S_OK;
}
static HRESULT WINAPI selitem_get_IsSelected(ISelectionItemProvider *iface, BOOL *ret)
{
    VARIANT v;

    mock_prop(UIA_SelectionItemIsSelectedPropertyId, &v);
    *ret = V_BOOL(&v) != VARIANT_FALSE;
    return S_OK;
}
static HRESULT WINAPI dock_get_DockPosition(IDockProvider *iface, enum DockPosition *ret)
{
    VARIANT v;

    mock_prop(UIA_DockDockPositionPropertyId, &v);
    *ret = V_I4(&v);
    return S_OK;
}
static HRESULT WINAPI multiview_get_CurrentView(IMultipleViewProvider *iface, int *ret)
{
    VARIANT v;

    mock_prop(UIA_MultipleViewCurrentViewPropertyId, &v);
    *ret = V_I4(&v);
    return S_OK;
}
static HRESULT WINAPI range_get_Value(IRangeValueProvider *iface, double *ret)
{
    VARIANT v;

    mock_prop(UIA_RangeValueValuePropertyId, &v);
    *ret = V_R8(&v);
    return S_OK;
}
static HRESULT WINAPI range_get_IsReadOnly(IRangeValueProvider *iface, BOOL *ret)
{
    VARIANT v;

    mock_prop(UIA_RangeValueIsReadOnlyPropertyId, &v);
    *ret = V_BOOL(&v) != VARIANT_FALSE;
    return S_OK;
}
static HRESULT WINAPI range_get_Maximum(IRangeValueProvider *iface, double *ret)
{
    VARIANT v;

    mock_prop(UIA_RangeValueMaximumPropertyId, &v);
    *ret = V_R8(&v);
    return S_OK;
}
static HRESULT WINAPI range_get_Minimum(IRangeValueProvider *iface, double *ret)
{
    VARIANT v;

    mock_prop(UIA_RangeValueMinimumPropertyId, &v);
    *ret = V_R8(&v);
    return S_OK;
}
static HRESULT WINAPI range_get_LargeChange(IRangeValueProvider *iface, double *ret)
{
    VARIANT v;

    mock_prop(UIA_RangeValueLargeChangePropertyId, &v);
    *ret = V_R8(&v);
    return S_OK;
}
static HRESULT WINAPI range_get_SmallChange(IRangeValueProvider *iface, double *ret)
{
    VARIANT v;

    mock_prop(UIA_RangeValueSmallChangePropertyId, &v);
    *ret = V_R8(&v);
    return S_OK;
}
static HRESULT WINAPI scroll_get_HorizontalScrollPercent(IScrollProvider *iface, double *ret)
{
    VARIANT v;

    mock_prop(UIA_ScrollHorizontalScrollPercentPropertyId, &v);
    *ret = V_R8(&v);
    return S_OK;
}
static HRESULT WINAPI scroll_get_VerticalScrollPercent(IScrollProvider *iface, double *ret)
{
    VARIANT v;

    mock_prop(UIA_ScrollVerticalScrollPercentPropertyId, &v);
    *ret = V_R8(&v);
    return S_OK;
}
static HRESULT WINAPI scroll_get_HorizontalViewSize(IScrollProvider *iface, double *ret)
{
    VARIANT v;

    mock_prop(UIA_ScrollHorizontalViewSizePropertyId, &v);
    *ret = V_R8(&v);
    return S_OK;
}
static HRESULT WINAPI scroll_get_VerticalViewSize(IScrollProvider *iface, double *ret)
{
    VARIANT v;

    mock_prop(UIA_ScrollVerticalViewSizePropertyId, &v);
    *ret = V_R8(&v);
    return S_OK;
}
static HRESULT WINAPI scroll_get_HorizontallyScrollable(IScrollProvider *iface, BOOL *ret)
{
    VARIANT v;

    mock_prop(UIA_ScrollHorizontallyScrollablePropertyId, &v);
    *ret = V_BOOL(&v) != VARIANT_FALSE;
    return S_OK;
}
static HRESULT WINAPI scroll_get_VerticallyScrollable(IScrollProvider *iface, BOOL *ret)
{
    VARIANT v;

    mock_prop(UIA_ScrollVerticallyScrollablePropertyId, &v);
    *ret = V_BOOL(&v) != VARIANT_FALSE;
    return S_OK;
}
static HRESULT WINAPI transform_get_CanMove(ITransformProvider *iface, BOOL *ret)
{
    VARIANT v;

    mock_prop(UIA_TransformCanMovePropertyId, &v);
    *ret = V_BOOL(&v) != VARIANT_FALSE;
    return S_OK;
}
static HRESULT WINAPI transform_get_CanResize(ITransformProvider *iface, BOOL *ret)
{
    VARIANT v;

    mock_prop(UIA_TransformCanResizePropertyId, &v);
    *ret = V_BOOL(&v) != VARIANT_FALSE;
    return S_OK;
}
static HRESULT WINAPI transform_get_CanRotate(ITransformProvider *iface, BOOL *ret)
{
    VARIANT v;

    mock_prop(UIA_TransformCanRotatePropertyId, &v);
    *ret = V_BOOL(&v) != VARIANT_FALSE;
    return S_OK;
}
static HRESULT WINAPI window_get_CanMaximize(IWindowProvider *iface, BOOL *ret)
{
    VARIANT v;

    mock_prop(UIA_WindowCanMaximizePropertyId, &v);
    *ret = V_BOOL(&v) != VARIANT_FALSE;
    return S_OK;
}
static HRESULT WINAPI window_get_CanMinimize(IWindowProvider *iface, BOOL *ret)
{
    VARIANT v;

    mock_prop(UIA_WindowCanMinimizePropertyId, &v);
    *ret = V_BOOL(&v) != VARIANT_FALSE;
    return S_OK;
}
static HRESULT WINAPI window_get_IsModal(IWindowProvider *iface, BOOL *ret)
{
    VARIANT v;

    mock_prop(UIA_WindowIsModalPropertyId, &v);
    *ret = V_BOOL(&v) != VARIANT_FALSE;
    return S_OK;
}
static HRESULT WINAPI window_get_IsTopmost(IWindowProvider *iface, BOOL *ret)
{
    VARIANT v;

    mock_prop(UIA_WindowIsTopmostPropertyId, &v);
    *ret = V_BOOL(&v) != VARIANT_FALSE;
    return S_OK;
}
static HRESULT WINAPI window_get_WindowVisualState(IWindowProvider *iface, enum WindowVisualState *ret)
{
    VARIANT v;

    mock_prop(UIA_WindowWindowVisualStatePropertyId, &v);
    *ret = V_I4(&v);
    return S_OK;
}
static HRESULT WINAPI window_get_WindowInteractionState(IWindowProvider *iface, enum WindowInteractionState *ret)
{
    VARIANT v;

    mock_prop(UIA_WindowWindowInteractionStatePropertyId, &v);
    *ret = V_I4(&v);
    return S_OK;
}
static HRESULT WINAPI legacy_get_ChildId(ILegacyIAccessibleProvider *iface, int *ret)
{
    VARIANT v;

    mock_prop(UIA_LegacyIAccessibleChildIdPropertyId, &v);
    *ret = V_I4(&v);
    return S_OK;
}
static HRESULT WINAPI legacy_get_Name(ILegacyIAccessibleProvider *iface, BSTR *ret)
{
    VARIANT v;

    mock_prop(UIA_LegacyIAccessibleNamePropertyId, &v);
    *ret = V_BSTR(&v);
    return S_OK;
}
static HRESULT WINAPI legacy_get_Value(ILegacyIAccessibleProvider *iface, BSTR *ret)
{
    VARIANT v;

    mock_prop(UIA_LegacyIAccessibleValuePropertyId, &v);
    *ret = V_BSTR(&v);
    return S_OK;
}
static HRESULT WINAPI legacy_get_Description(ILegacyIAccessibleProvider *iface, BSTR *ret)
{
    VARIANT v;

    mock_prop(UIA_LegacyIAccessibleDescriptionPropertyId, &v);
    *ret = V_BSTR(&v);
    return S_OK;
}
static HRESULT WINAPI legacy_get_Help(ILegacyIAccessibleProvider *iface, BSTR *ret)
{
    VARIANT v;

    mock_prop(UIA_LegacyIAccessibleHelpPropertyId, &v);
    *ret = V_BSTR(&v);
    return S_OK;
}
static HRESULT WINAPI legacy_get_KeyboardShortcut(ILegacyIAccessibleProvider *iface, BSTR *ret)
{
    VARIANT v;

    mock_prop(UIA_LegacyIAccessibleKeyboardShortcutPropertyId, &v);
    *ret = V_BSTR(&v);
    return S_OK;
}
static HRESULT WINAPI legacy_get_DefaultAction(ILegacyIAccessibleProvider *iface, BSTR *ret)
{
    VARIANT v;

    mock_prop(UIA_LegacyIAccessibleDefaultActionPropertyId, &v);
    *ret = V_BSTR(&v);
    return S_OK;
}
static HRESULT WINAPI legacy_get_Role(ILegacyIAccessibleProvider *iface, DWORD *ret)
{
    VARIANT v;

    mock_prop(UIA_LegacyIAccessibleRolePropertyId, &v);
    *ret = V_I4(&v);
    return S_OK;
}
static HRESULT WINAPI legacy_get_State(ILegacyIAccessibleProvider *iface, DWORD *ret)
{
    VARIANT v;

    mock_prop(UIA_LegacyIAccessibleStatePropertyId, &v);
    *ret = V_I4(&v);
    return S_OK;
}
static HRESULT WINAPI selitem_get_SelectionContainer(ISelectionItemProvider *iface, IRawElementProviderSimple **ret)
{
    *ret = NULL;
    return S_OK;
}
static HRESULT WINAPI multiview_GetSupportedViews(IMultipleViewProvider *iface, SAFEARRAY **ret)
{
    VARIANT v;

    mock_prop(UIA_MultipleViewSupportedViewsPropertyId, &v);
    *ret = V_ARRAY(&v);
    return S_OK;
}
static IInvokeProviderVtbl invoke_vtbl = { invoke_QI, invoke_AddRef, invoke_Release, invoke_Invoke };
static IExpandCollapseProviderVtbl expcol_vtbl = { .QueryInterface = expcol_QI, .AddRef = expcol_AddRef, .Release = expcol_Release,
    .Expand = expcol_Expand, .Collapse = expcol_Collapse,
    .get_ExpandCollapseState = expcol_get_ExpandCollapseState };
static IToggleProviderVtbl toggle_vtbl = { .QueryInterface = toggle_QI, .AddRef = toggle_AddRef, .Release = toggle_Release,
    .Toggle = toggle_Toggle,
    .get_ToggleState = toggle_get_ToggleState };

static HRESULT WINAPI value_SetValue(IValueProvider *iface, LPCWSTR val)
{
    logf_("SetValue(%ls);", val);
    return result(from_value(iface));
}
static IValueProviderVtbl value_vtbl = { .QueryInterface = value_QI, .AddRef = value_AddRef, .Release = value_Release,
    .SetValue = value_SetValue,
    .get_Value = value_get_Value,
    .get_IsReadOnly = value_get_IsReadOnly };

static HRESULT WINAPI range_SetValue(IRangeValueProvider *iface, double val)
{
    logf_("RangeSet(%g);", val);
    return result(from_range(iface));
}
static IRangeValueProviderVtbl range_vtbl = { .QueryInterface = range_QI, .AddRef = range_AddRef, .Release = range_Release,
    .SetValue = range_SetValue,
    .get_Value = range_get_Value,
    .get_IsReadOnly = range_get_IsReadOnly,
    .get_Maximum = range_get_Maximum,
    .get_Minimum = range_get_Minimum,
    .get_LargeChange = range_get_LargeChange,
    .get_SmallChange = range_get_SmallChange };

static ISelectionItemProviderVtbl selitem_vtbl = { .QueryInterface = selitem_QI, .AddRef = selitem_AddRef,
    .Release = selitem_Release, .Select = selitem_Select, .AddToSelection = selitem_Add, .RemoveFromSelection = selitem_Remove,
    .get_IsSelected = selitem_get_IsSelected,
    .get_SelectionContainer = selitem_get_SelectionContainer };
static IScrollItemProviderVtbl scrollitem_vtbl = { .QueryInterface = scrollitem_QI, .AddRef = scrollitem_AddRef,
    .Release = scrollitem_Release, .ScrollIntoView = scrollitem_ScrollIntoView };

static HRESULT WINAPI scroll_Scroll(IScrollProvider *iface, enum ScrollAmount h, enum ScrollAmount v)
{
    logf_("Scroll(%d,%d);", h, v);
    return result(from_scroll(iface));
}
static HRESULT WINAPI scroll_SetScrollPercent(IScrollProvider *iface, double h, double v)
{
    logf_("ScrollPercent(%g,%g);", h, v);
    return result(from_scroll(iface));
}
static IScrollProviderVtbl scroll_vtbl = { .QueryInterface = scroll_QI, .AddRef = scroll_AddRef, .Release = scroll_Release,
    .Scroll = scroll_Scroll, .SetScrollPercent = scroll_SetScrollPercent,
    .get_HorizontalScrollPercent = scroll_get_HorizontalScrollPercent,
    .get_VerticalScrollPercent = scroll_get_VerticalScrollPercent,
    .get_HorizontalViewSize = scroll_get_HorizontalViewSize,
    .get_VerticalViewSize = scroll_get_VerticalViewSize,
    .get_HorizontallyScrollable = scroll_get_HorizontallyScrollable,
    .get_VerticallyScrollable = scroll_get_VerticallyScrollable };

static HRESULT WINAPI dock_SetDockPosition(IDockProvider *iface, enum DockPosition pos)
{
    logf_("Dock(%d);", pos);
    return result(from_dock(iface));
}
static IDockProviderVtbl dock_vtbl = { .QueryInterface = dock_QI, .AddRef = dock_AddRef, .Release = dock_Release,
    .SetDockPosition = dock_SetDockPosition,
    .get_DockPosition = dock_get_DockPosition };

static HRESULT WINAPI transform_Move(ITransformProvider *iface, double x, double y)
{
    logf_("Move(%g,%g);", x, y);
    return result(from_transform(iface));
}
static HRESULT WINAPI transform_Resize(ITransformProvider *iface, double w, double h)
{
    logf_("Resize(%g,%g);", w, h);
    return result(from_transform(iface));
}
static HRESULT WINAPI transform_Rotate(ITransformProvider *iface, double deg)
{
    logf_("Rotate(%g);", deg);
    return result(from_transform(iface));
}
static ITransformProviderVtbl transform_vtbl = { .QueryInterface = transform_QI, .AddRef = transform_AddRef,
    .Release = transform_Release, .Move = transform_Move, .Resize = transform_Resize, .Rotate = transform_Rotate,
    .get_CanMove = transform_get_CanMove,
    .get_CanResize = transform_get_CanResize,
    .get_CanRotate = transform_get_CanRotate };

static HRESULT WINAPI window_SetVisualState(IWindowProvider *iface, enum WindowVisualState state)
{
    logf_("VisualState(%d);", state);
    return result(from_window(iface));
}
static HRESULT WINAPI window_WaitForInputIdle(IWindowProvider *iface, int ms, BOOL *ret)
{
    logf_("WaitIdle(%d);", ms);
    *ret = (ms == 100);
    return result(from_window(iface));
}
static IWindowProviderVtbl window_vtbl = { .QueryInterface = window_QI, .AddRef = window_AddRef, .Release = window_Release,
    .SetVisualState = window_SetVisualState, .WaitForInputIdle = window_WaitForInputIdle, .Close = window_Close,
    .get_CanMaximize = window_get_CanMaximize,
    .get_CanMinimize = window_get_CanMinimize,
    .get_IsModal = window_get_IsModal,
    .get_IsTopmost = window_get_IsTopmost,
    .get_WindowVisualState = window_get_WindowVisualState,
    .get_WindowInteractionState = window_get_WindowInteractionState };
static IVirtualizedItemProviderVtbl virt_vtbl = { .QueryInterface = virt_QI, .AddRef = virt_AddRef, .Release = virt_Release,
    .Realize = virt_Realize };

static HRESULT WINAPI multiview_GetViewName(IMultipleViewProvider *iface, int view, BSTR *name)
{
    WCHAR buf[32];

    logf_("ViewName(%d);", view);
    swprintf(buf, 32, L"View %d", view);
    *name = SysAllocString(buf);
    return result(from_multiview(iface));
}
static HRESULT WINAPI multiview_SetCurrentView(IMultipleViewProvider *iface, int view)
{
    logf_("SetView(%d);", view);
    return result(from_multiview(iface));
}
static IMultipleViewProviderVtbl multiview_vtbl = { .QueryInterface = multiview_QI, .AddRef = multiview_AddRef,
    .Release = multiview_Release, .GetViewName = multiview_GetViewName, .SetCurrentView = multiview_SetCurrentView,
    .get_CurrentView = multiview_get_CurrentView,
    .GetSupportedViews = multiview_GetSupportedViews };

static HRESULT WINAPI sync_StartListening(ISynchronizedInputProvider *iface, enum SynchronizedInputType type)
{
    logf_("SyncStart(%d);", type);
    return result(from_sync(iface));
}
static ISynchronizedInputProviderVtbl sync_vtbl = { .QueryInterface = sync_QI, .AddRef = sync_AddRef,
    .Release = sync_Release, .StartListening = sync_StartListening, .Cancel = sync_Cancel };

static HRESULT WINAPI legacy_Select(ILegacyIAccessibleProvider *iface, LONG flags)
{
    logf_("LegacySelect(%ld);", flags);
    return result(from_legacy(iface));
}
static HRESULT WINAPI legacy_SetValue(ILegacyIAccessibleProvider *iface, LPCWSTR val)
{
    logf_("LegacySetValue(%ls);", val);
    return result(from_legacy(iface));
}
static HRESULT WINAPI legacy_GetIAccessible(ILegacyIAccessibleProvider *iface, IAccessible **acc)
{
    struct mock *m = from_legacy(iface);

    *acc = m->acc;
    if (*acc) IAccessible_AddRef(*acc);
    return result(m);
}
static ILegacyIAccessibleProviderVtbl legacy_vtbl = { .QueryInterface = legacy_QI, .AddRef = legacy_AddRef,
    .Release = legacy_Release, .Select = legacy_Select, .DoDefaultAction = legacy_DoDefaultAction,
    .SetValue = legacy_SetValue, .GetIAccessible = legacy_GetIAccessible,
    .get_ChildId = legacy_get_ChildId,
    .get_Name = legacy_get_Name,
    .get_Value = legacy_get_Value,
    .get_Description = legacy_get_Description,
    .get_Help = legacy_get_Help,
    .get_KeyboardShortcut = legacy_get_KeyboardShortcut,
    .get_DefaultAction = legacy_get_DefaultAction,
    .get_Role = legacy_get_Role,
    .get_State = legacy_get_State };

static void mock_init(struct mock *m)
{
    memset(m, 0, sizeof(*m));
    m->simple.lpVtbl = &simple_vtbl;
    m->frag.lpVtbl = &frag_vtbl;
    m->invoke.lpVtbl = &invoke_vtbl;
    m->expcol.lpVtbl = &expcol_vtbl;
    m->toggle.lpVtbl = &toggle_vtbl;
    m->value.lpVtbl = &value_vtbl;
    m->range.lpVtbl = &range_vtbl;
    m->selitem.lpVtbl = &selitem_vtbl;
    m->scrollitem.lpVtbl = &scrollitem_vtbl;
    m->scroll.lpVtbl = &scroll_vtbl;
    m->dock.lpVtbl = &dock_vtbl;
    m->transform.lpVtbl = &transform_vtbl;
    m->window.lpVtbl = &window_vtbl;
    m->virt.lpVtbl = &virt_vtbl;
    m->multiview.lpVtbl = &multiview_vtbl;
    m->sync.lpVtbl = &sync_vtbl;
    m->legacy.lpVtbl = &legacy_vtbl;
    m->ref = 1;
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_GETOBJECT && lparam == UiaRootObjectId)
        return UiaReturnRawElementProvider(hwnd, wparam, lparam, &g_mock.simple);
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

#define LOGGED(expected_text, call) do { logbuf[0] = 0; hr = (call); \
        check(hr == S_OK && !strcmp(logbuf, expected_text), "%s -> %s (%#lx)", #call, logbuf, hr); } while (0)

static IUIAutomationElement *element;

static IUnknown *pattern(PATTERNID id, HRESULT *hr)
{
    IUnknown *unk = (void *)0xdeadbeef;

    *hr = IUIAutomationElement_GetCurrentPattern(element, id, &unk);
    return unk;
}

int main(void)
{
    WNDCLASSW cls = {0};
    IUIAutomation *uia = NULL;
    IUnknown *unk;
    HRESULT hr;
    HWND hwnd;
    BSTR bstr;
    BOOL b;
    int i;
    double d;
    DWORD dw;
    BOOL ok;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    mock_init(&g_mock);
    g_mock.has_fragment = FALSE;
    g_mock.supported = PAT(UIA_InvokePatternId) | PAT(UIA_ExpandCollapsePatternId) | PAT(UIA_TogglePatternId)
            | PAT(UIA_ValuePatternId) | PAT(UIA_RangeValuePatternId) | PAT(UIA_SelectionItemPatternId)
            | PAT(UIA_ScrollItemPatternId) | PAT(UIA_ScrollPatternId) | PAT(UIA_DockPatternId) | PAT(UIA_TransformPatternId)
            | PAT(UIA_WindowPatternId) | PAT(UIA_VirtualizedItemPatternId) | PAT(UIA_MultipleViewPatternId)
            | PAT(UIA_SynchronizedInputPatternId) | PAT(UIA_LegacyIAccessiblePatternId);

    cls.lpfnWndProc = wndproc;
    cls.hInstance = GetModuleHandleW(NULL);
    cls.lpszClassName = L"uiacpat";
    RegisterClassW(&cls);
    hwnd = CreateWindowW(L"uiacpat", L"pattern window", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 100, 0, 0, cls.hInstance, 0);
    check(hwnd != NULL, "window");

    hr = CoCreateInstance(&CLSID_CUIAutomation8, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAutomation, (void **)&uia);
    check(hr == S_OK && uia, "IUIAutomation (%#lx)", hr);
    if (!uia) goto done;
    hr = IUIAutomation_ElementFromHandle(uia, (UIA_HWND)hwnd, &element);
    check(hr == S_OK && element, "element (%#lx)", hr);
    if (!element) goto done;

    /* what is not supported, what has no object yet, bad arguments */
    g_mock.supported = 0;
    unk = pattern(UIA_InvokePatternId, &hr);
    check(hr == S_OK && !unk, "an unsupported pattern is no object (%#lx, %p)", hr, unk);
    g_mock.supported = ~0u;
    hr = IUIAutomationElement_GetCurrentPattern(element, UIA_InvokePatternId, NULL);
    check(hr == E_POINTER, "no pointer = %#lx", hr);
    unk = pattern(UIA_SelectionPatternId, &hr);
    check(hr == E_NOTIMPL, "Selection has no client object yet = %#lx", hr);
    g_mock.supported = PAT(UIA_InvokePatternId) | PAT(UIA_ExpandCollapsePatternId) | PAT(UIA_TogglePatternId)
            | PAT(UIA_ValuePatternId) | PAT(UIA_RangeValuePatternId) | PAT(UIA_SelectionItemPatternId)
            | PAT(UIA_ScrollItemPatternId) | PAT(UIA_ScrollPatternId) | PAT(UIA_DockPatternId) | PAT(UIA_TransformPatternId)
            | PAT(UIA_WindowPatternId) | PAT(UIA_VirtualizedItemPatternId) | PAT(UIA_MultipleViewPatternId)
            | PAT(UIA_SynchronizedInputPatternId) | PAT(UIA_LegacyIAccessiblePatternId);

    /* Invoke */
    {
        IUIAutomationInvokePattern *inv = NULL;

        unk = pattern(UIA_InvokePatternId, &hr);
        check(hr == S_OK && unk, "Invoke pattern (%#lx)", hr);
        hr = unk ? IUnknown_QueryInterface(unk, &IID_IUIAutomationInvokePattern, (void **)&inv) : E_FAIL;
        check(hr == S_OK && inv, "as IUIAutomationInvokePattern");
        if (inv)
        {
            LOGGED("Invoke;", IUIAutomationInvokePattern_Invoke(inv));
            g_mock.fail = E_ACCESSDENIED;
            logbuf[0] = 0;
            hr = IUIAutomationInvokePattern_Invoke(inv);
            check(hr == E_ACCESSDENIED, "an error of the provider comes back (%#lx)", hr);
            IUIAutomationInvokePattern_Release(inv);
        }
        if (unk)
        {
            void *other = (void *)0xdeadbeef;

            hr = IUnknown_QueryInterface(unk, &IID_IUIAutomationTogglePattern, &other);
            check(hr == E_NOINTERFACE && !other, "and not as another pattern (%#lx)", hr);
            IUnknown_Release(unk);
        }
        {
            IUIAutomationInvokePattern *as = NULL;

            hr = IUIAutomationElement_GetCurrentPatternAs(element, UIA_InvokePatternId, &IID_IUIAutomationInvokePattern, (void **)&as);
            check(hr == S_OK && as, "GetCurrentPatternAs (%#lx)", hr);
            if (as) IUIAutomationInvokePattern_Release(as);
            as = (void *)0xdeadbeef;
            hr = IUIAutomationElement_GetCurrentPatternAs(element, UIA_InvokePatternId, &IID_IUIAutomationTogglePattern, (void **)&as);
            check(hr == E_NOINTERFACE && !as, "GetCurrentPatternAs of the wrong interface (%#lx)", hr);
        }
    }

    /* ExpandCollapse */
    {
        IUIAutomationExpandCollapsePattern *p = NULL;
        enum ExpandCollapseState state = 99;

        IUIAutomationElement_GetCurrentPatternAs(element, UIA_ExpandCollapsePatternId, &IID_IUIAutomationExpandCollapsePattern, (void **)&p);
        check(p != NULL, "ExpandCollapse");
        if (p)
        {
            LOGGED("Expand;", IUIAutomationExpandCollapsePattern_Expand(p));
            LOGGED("Collapse;", IUIAutomationExpandCollapsePattern_Collapse(p));
            hr = IUIAutomationExpandCollapsePattern_get_CurrentExpandCollapseState(p, &state);
            check(hr == S_OK && state == 2, "state %d", state);
            hr = IUIAutomationExpandCollapsePattern_get_CachedExpandCollapseState(p, &state);
            check(hr == E_INVALIDARG, "cached state without a cache = %#lx", hr);
            hr = IUIAutomationExpandCollapsePattern_get_CurrentExpandCollapseState(p, NULL);
            check(hr == E_POINTER, "no result pointer = %#lx", hr);
            IUIAutomationExpandCollapsePattern_Release(p);
        }
    }

    /* Toggle */
    {
        IUIAutomationTogglePattern *p = NULL;
        enum ToggleState state = 99;

        IUIAutomationElement_GetCurrentPatternAs(element, UIA_TogglePatternId, &IID_IUIAutomationTogglePattern, (void **)&p);
        if (p)
        {
            LOGGED("Toggle;", IUIAutomationTogglePattern_Toggle(p));
            hr = IUIAutomationTogglePattern_get_CurrentToggleState(p, &state);
            check(hr == S_OK && state == 1, "toggle state %d", state);
            IUIAutomationTogglePattern_Release(p);
        }
        else check(0, "Toggle");
    }

    /* Value */
    {
        IUIAutomationValuePattern *p = NULL;

        IUIAutomationElement_GetCurrentPatternAs(element, UIA_ValuePatternId, &IID_IUIAutomationValuePattern, (void **)&p);
        if (p)
        {
            bstr = SysAllocString(L"hello world");
            LOGGED("SetValue(hello world);", IUIAutomationValuePattern_SetValue(p, bstr));
            SysFreeString(bstr);
            bstr = NULL;
            hr = IUIAutomationValuePattern_get_CurrentValue(p, &bstr);
            check(hr == S_OK && bstr && !wcscmp(bstr, L"the value"), "value %ls", bstr ? bstr : L"(null)");
            SysFreeString(bstr);
            b = FALSE;
            hr = IUIAutomationValuePattern_get_CurrentIsReadOnly(p, &b);
            check(hr == S_OK && b, "read only");
            IUIAutomationValuePattern_Release(p);
        }
        else check(0, "Value");
    }

    /* RangeValue */
    {
        IUIAutomationRangeValuePattern *p = NULL;

        IUIAutomationElement_GetCurrentPatternAs(element, UIA_RangeValuePatternId, &IID_IUIAutomationRangeValuePattern, (void **)&p);
        if (p)
        {
            LOGGED("RangeSet(12.25);", IUIAutomationRangeValuePattern_SetValue(p, 12.25));
            d = 0; hr = IUIAutomationRangeValuePattern_get_CurrentValue(p, &d);
            check(hr == S_OK && d == 5.5, "range value %g", d);
            d = 0; IUIAutomationRangeValuePattern_get_CurrentMaximum(p, &d);
            ok = d == 100.0; d = 0; IUIAutomationRangeValuePattern_get_CurrentMinimum(p, &d);
            check(ok && d == -100.0, "maximum and minimum");
            d = 0; IUIAutomationRangeValuePattern_get_CurrentLargeChange(p, &d);
            ok = d == 10.0; d = 0; IUIAutomationRangeValuePattern_get_CurrentSmallChange(p, &d);
            check(ok && d == 1.0, "large and small change");
            b = TRUE; IUIAutomationRangeValuePattern_get_CurrentIsReadOnly(p, &b);
            check(!b, "not read only");
            IUIAutomationRangeValuePattern_Release(p);
        }
        else check(0, "RangeValue");
    }

    /* SelectionItem */
    {
        IUIAutomationSelectionItemPattern *p = NULL;
        IUIAutomationElement *container = (void *)0xdeadbeef;

        IUIAutomationElement_GetCurrentPatternAs(element, UIA_SelectionItemPatternId, &IID_IUIAutomationSelectionItemPattern, (void **)&p);
        if (p)
        {
            LOGGED("Select;", IUIAutomationSelectionItemPattern_Select(p));
            LOGGED("AddToSelection;", IUIAutomationSelectionItemPattern_AddToSelection(p));
            LOGGED("RemoveFromSelection;", IUIAutomationSelectionItemPattern_RemoveFromSelection(p));
            b = FALSE; IUIAutomationSelectionItemPattern_get_CurrentIsSelected(p, &b);
            check(b, "selected");
            hr = IUIAutomationSelectionItemPattern_get_CurrentSelectionContainer(p, &container);
            check(hr == S_OK && !container, "no container (%#lx, %p)", hr, container);
            IUIAutomationSelectionItemPattern_Release(p);
        }
        else check(0, "SelectionItem");
    }

    /* ScrollItem, Scroll */
    {
        IUIAutomationScrollItemPattern *si = NULL;
        IUIAutomationScrollPattern *p = NULL;

        IUIAutomationElement_GetCurrentPatternAs(element, UIA_ScrollItemPatternId, &IID_IUIAutomationScrollItemPattern, (void **)&si);
        if (si) { LOGGED("ScrollIntoView;", IUIAutomationScrollItemPattern_ScrollIntoView(si)); IUIAutomationScrollItemPattern_Release(si); }
        else check(0, "ScrollItem");
        IUIAutomationElement_GetCurrentPatternAs(element, UIA_ScrollPatternId, &IID_IUIAutomationScrollPattern, (void **)&p);
        if (p)
        {
            LOGGED("Scroll(2,3);", IUIAutomationScrollPattern_Scroll(p, 2, 3));
            LOGGED("ScrollPercent(12.5,87.25);", IUIAutomationScrollPattern_SetScrollPercent(p, 12.5, 87.25));
            d = 0; IUIAutomationScrollPattern_get_CurrentHorizontalScrollPercent(p, &d); ok = d == 12.5;
            d = 0; IUIAutomationScrollPattern_get_CurrentVerticalScrollPercent(p, &d);
            check(ok && d == 25.0, "scroll percents");
            d = 0; IUIAutomationScrollPattern_get_CurrentHorizontalViewSize(p, &d); ok = d == 50.0;
            d = 0; IUIAutomationScrollPattern_get_CurrentVerticalViewSize(p, &d);
            check(ok && d == 75.0, "view sizes");
            b = FALSE; IUIAutomationScrollPattern_get_CurrentHorizontallyScrollable(p, &b); ok = b;
            b = TRUE; IUIAutomationScrollPattern_get_CurrentVerticallyScrollable(p, &b);
            check(ok && !b, "scrollable");
            IUIAutomationScrollPattern_Release(p);
        }
        else check(0, "Scroll");
    }

    /* Dock, Transform */
    {
        IUIAutomationDockPattern *dock = NULL;
        IUIAutomationTransformPattern *tr = NULL;
        enum DockPosition pos = 99;

        IUIAutomationElement_GetCurrentPatternAs(element, UIA_DockPatternId, &IID_IUIAutomationDockPattern, (void **)&dock);
        if (dock)
        {
            LOGGED("Dock(4);", IUIAutomationDockPattern_SetDockPosition(dock, 4));
            IUIAutomationDockPattern_get_CurrentDockPosition(dock, &pos);
            check(pos == 3, "dock position %d", pos);
            IUIAutomationDockPattern_Release(dock);
        }
        else check(0, "Dock");
        IUIAutomationElement_GetCurrentPatternAs(element, UIA_TransformPatternId, &IID_IUIAutomationTransformPattern, (void **)&tr);
        if (tr)
        {
            LOGGED("Move(10.5,-20);", IUIAutomationTransformPattern_Move(tr, 10.5, -20));
            LOGGED("Resize(300,150.5);", IUIAutomationTransformPattern_Resize(tr, 300, 150.5));
            LOGGED("Rotate(45.5);", IUIAutomationTransformPattern_Rotate(tr, 45.5));
            b = FALSE; IUIAutomationTransformPattern_get_CurrentCanMove(tr, &b); ok = b;
            b = TRUE; IUIAutomationTransformPattern_get_CurrentCanResize(tr, &b);
            check(ok && !b, "can move, cannot resize");
            IUIAutomationTransformPattern_Release(tr);
        }
        else check(0, "Transform");
    }

    /* Window */
    {
        IUIAutomationWindowPattern *p = NULL;
        enum WindowVisualState vs = 99;
        enum WindowInteractionState is = 99;

        IUIAutomationElement_GetCurrentPatternAs(element, UIA_WindowPatternId, &IID_IUIAutomationWindowPattern, (void **)&p);
        if (p)
        {
            LOGGED("Close;", IUIAutomationWindowPattern_Close(p));
            LOGGED("VisualState(2);", IUIAutomationWindowPattern_SetWindowVisualState(p, 2));
            b = FALSE;
            logbuf[0] = 0;
            hr = IUIAutomationWindowPattern_WaitForInputIdle(p, 100, &b);
            check(hr == S_OK && b && !strcmp(logbuf, "WaitIdle(100);"), "WaitForInputIdle (%#lx, %d, %s)", hr, b, logbuf);
            IUIAutomationWindowPattern_get_CurrentWindowVisualState(p, &vs);
            IUIAutomationWindowPattern_get_CurrentWindowInteractionState(p, &is);
            check(vs == 1 && is == 2, "visual state %d, interaction state %d", vs, is);
            b = FALSE; IUIAutomationWindowPattern_get_CurrentCanMaximize(p, &b); ok = b;
            b = TRUE; IUIAutomationWindowPattern_get_CurrentCanMinimize(p, &b);
            check(ok && !b, "can maximize, cannot minimize");
            b = FALSE; IUIAutomationWindowPattern_get_CurrentIsModal(p, &b); ok = b;
            b = TRUE; IUIAutomationWindowPattern_get_CurrentIsTopmost(p, &b);
            check(ok && !b, "modal, not topmost");
            IUIAutomationWindowPattern_Release(p);
        }
        else check(0, "Window");
    }

    /* VirtualizedItem, MultipleView, SynchronizedInput */
    {
        IUIAutomationVirtualizedItemPattern *v = NULL;
        IUIAutomationMultipleViewPattern *mv = NULL;
        IUIAutomationSynchronizedInputPattern *sy = NULL;

        IUIAutomationElement_GetCurrentPatternAs(element, UIA_VirtualizedItemPatternId, &IID_IUIAutomationVirtualizedItemPattern, (void **)&v);
        if (v) { LOGGED("Realize;", IUIAutomationVirtualizedItemPattern_Realize(v)); IUIAutomationVirtualizedItemPattern_Release(v); }
        else check(0, "VirtualizedItem");
        IUIAutomationElement_GetCurrentPatternAs(element, UIA_MultipleViewPatternId, &IID_IUIAutomationMultipleViewPattern, (void **)&mv);
        if (mv)
        {
            SAFEARRAY *views = NULL;
            LONG lb = 0, ub = -1;

            bstr = NULL;
            logbuf[0] = 0;
            hr = IUIAutomationMultipleViewPattern_GetViewName(mv, 5, &bstr);
            check(hr == S_OK && bstr && !wcscmp(bstr, L"View 5") && !strcmp(logbuf, "ViewName(5);"), "view name %ls", bstr ? bstr : L"(null)");
            SysFreeString(bstr);
            LOGGED("SetView(7);", IUIAutomationMultipleViewPattern_SetCurrentView(mv, 7));
            i = 0; IUIAutomationMultipleViewPattern_get_CurrentCurrentView(mv, &i);
            check(i == 4, "current view %d", i);
            hr = IUIAutomationMultipleViewPattern_GetCurrentSupportedViews(mv, &views);
            if (views) { SafeArrayGetLBound(views, 1, &lb); SafeArrayGetUBound(views, 1, &ub); SafeArrayDestroy(views); }
            check(hr == S_OK && ub - lb + 1 == 3, "three supported views (%#lx)", hr);
            IUIAutomationMultipleViewPattern_Release(mv);
        }
        else check(0, "MultipleView");
        IUIAutomationElement_GetCurrentPatternAs(element, UIA_SynchronizedInputPatternId, &IID_IUIAutomationSynchronizedInputPattern, (void **)&sy);
        if (sy)
        {
            LOGGED("SyncStart(3);", IUIAutomationSynchronizedInputPattern_StartListening(sy, 3));
            LOGGED("SyncCancel;", IUIAutomationSynchronizedInputPattern_Cancel(sy));
            IUIAutomationSynchronizedInputPattern_Release(sy);
        }
        else check(0, "SynchronizedInput");
    }

    /* LegacyIAccessible */
    {
        IUIAutomationLegacyIAccessiblePattern *p = NULL;

        IUIAutomationElement_GetCurrentPatternAs(element, UIA_LegacyIAccessiblePatternId, &IID_IUIAutomationLegacyIAccessiblePattern, (void **)&p);
        if (p)
        {
            LOGGED("LegacySelect(9);", IUIAutomationLegacyIAccessiblePattern_Select(p, 9));
            LOGGED("DoDefaultAction;", IUIAutomationLegacyIAccessiblePattern_DoDefaultAction(p));
            LOGGED("LegacySetValue(legacy text);", IUIAutomationLegacyIAccessiblePattern_SetValue(p, L"legacy text"));
            i = 0; IUIAutomationLegacyIAccessiblePattern_get_CurrentChildId(p, &i);
            check(i == 7, "child id %d", i);
            bstr = NULL; IUIAutomationLegacyIAccessiblePattern_get_CurrentName(p, &bstr);
            check(bstr && !wcscmp(bstr, L"legacy name"), "name %ls", bstr ? bstr : L"(null)"); SysFreeString(bstr);
            bstr = NULL; IUIAutomationLegacyIAccessiblePattern_get_CurrentDefaultAction(p, &bstr);
            check(bstr && !wcscmp(bstr, L"legacy click"), "default action %ls", bstr ? bstr : L"(null)"); SysFreeString(bstr);
            dw = 0; IUIAutomationLegacyIAccessiblePattern_get_CurrentRole(p, &dw); ok = dw == 42;
            dw = 0; IUIAutomationLegacyIAccessiblePattern_get_CurrentState(p, &dw);
            check(ok && dw == 0x20, "role and state");
            bstr = NULL; IUIAutomationLegacyIAccessiblePattern_get_CurrentKeyboardShortcut(p, &bstr);
            check(bstr && !wcscmp(bstr, L"Ctrl+L"), "shortcut %ls", bstr ? bstr : L"(null)"); SysFreeString(bstr);
            IUIAutomationLegacyIAccessiblePattern_Release(p);
        }
        else check(0, "LegacyIAccessible");
    }

    IUIAutomationElement_Release(element);
    IUIAutomation_Release(uia);
done:
    DestroyWindow(hwnd);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
