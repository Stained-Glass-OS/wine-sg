/* uiautomationcore: client pattern objects (UiaGetPatternProvider, the
 * Xxx_Method exports, UiaPatternRelease) and UiaSetFocus, against a provider
 * that logs what its pattern interfaces are called with (patches/sg/2610). */
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

typedef HUIAPATTERNOBJECT PATOBJ;

static HRESULT (WINAPI *pUiaGetPatternProvider)(HUIANODE, PATTERNID, PATOBJ *);
static BOOL (WINAPI *pUiaPatternRelease)(PATOBJ);
static HRESULT (WINAPI *pUiaSetFocus)(HUIANODE);
static HRESULT (WINAPI *pInvoke)(PATOBJ);
static HRESULT (WINAPI *pExpand)(PATOBJ);
static HRESULT (WINAPI *pCollapse)(PATOBJ);
static HRESULT (WINAPI *pToggle)(PATOBJ);
static HRESULT (WINAPI *pValueSet)(PATOBJ, LPCWSTR);
static HRESULT (WINAPI *pRangeSet)(PATOBJ, double);
static HRESULT (WINAPI *pSelSelect)(PATOBJ);
static HRESULT (WINAPI *pSelAdd)(PATOBJ);
static HRESULT (WINAPI *pSelRemove)(PATOBJ);
static HRESULT (WINAPI *pScrollIntoView)(PATOBJ);
static HRESULT (WINAPI *pScroll)(PATOBJ, int, int);
static HRESULT (WINAPI *pScrollPercent)(PATOBJ, double, double);
static HRESULT (WINAPI *pDock)(PATOBJ, int);
static HRESULT (WINAPI *pMove)(PATOBJ, double, double);
static HRESULT (WINAPI *pResize)(PATOBJ, double, double);
static HRESULT (WINAPI *pRotate)(PATOBJ, double);
static HRESULT (WINAPI *pClose)(PATOBJ);
static HRESULT (WINAPI *pVisualState)(PATOBJ, int);
static HRESULT (WINAPI *pWaitIdle)(PATOBJ, int, BOOL *);
static HRESULT (WINAPI *pRealize)(PATOBJ);
static HRESULT (WINAPI *pViewName)(PATOBJ, int, BSTR *);
static HRESULT (WINAPI *pSetView)(PATOBJ, int);
static HRESULT (WINAPI *pSyncStart)(PATOBJ, int);
static HRESULT (WINAPI *pSyncCancel)(PATOBJ);
static HRESULT (WINAPI *pLegSelect)(PATOBJ, LONG);
static HRESULT (WINAPI *pLegAction)(PATOBJ);
static HRESULT (WINAPI *pLegSetValue)(PATOBJ, LPCWSTR);
static HRESULT (WINAPI *pLegGetAcc)(PATOBJ, IAccessible **);

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
    *ret = ProviderOptions_ClientSideProvider;
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

static IInvokeProviderVtbl invoke_vtbl = { invoke_QI, invoke_AddRef, invoke_Release, invoke_Invoke };
static IExpandCollapseProviderVtbl expcol_vtbl = { .QueryInterface = expcol_QI, .AddRef = expcol_AddRef, .Release = expcol_Release,
    .Expand = expcol_Expand, .Collapse = expcol_Collapse };
static IToggleProviderVtbl toggle_vtbl = { .QueryInterface = toggle_QI, .AddRef = toggle_AddRef, .Release = toggle_Release,
    .Toggle = toggle_Toggle };

static HRESULT WINAPI value_SetValue(IValueProvider *iface, LPCWSTR val)
{
    logf_("SetValue(%ls);", val);
    return result(from_value(iface));
}
static IValueProviderVtbl value_vtbl = { .QueryInterface = value_QI, .AddRef = value_AddRef, .Release = value_Release,
    .SetValue = value_SetValue };

static HRESULT WINAPI range_SetValue(IRangeValueProvider *iface, double val)
{
    logf_("RangeSet(%g);", val);
    return result(from_range(iface));
}
static IRangeValueProviderVtbl range_vtbl = { .QueryInterface = range_QI, .AddRef = range_AddRef, .Release = range_Release,
    .SetValue = range_SetValue };

static ISelectionItemProviderVtbl selitem_vtbl = { .QueryInterface = selitem_QI, .AddRef = selitem_AddRef,
    .Release = selitem_Release, .Select = selitem_Select, .AddToSelection = selitem_Add, .RemoveFromSelection = selitem_Remove };
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
    .Scroll = scroll_Scroll, .SetScrollPercent = scroll_SetScrollPercent };

static HRESULT WINAPI dock_SetDockPosition(IDockProvider *iface, enum DockPosition pos)
{
    logf_("Dock(%d);", pos);
    return result(from_dock(iface));
}
static IDockProviderVtbl dock_vtbl = { .QueryInterface = dock_QI, .AddRef = dock_AddRef, .Release = dock_Release,
    .SetDockPosition = dock_SetDockPosition };

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
    .Release = transform_Release, .Move = transform_Move, .Resize = transform_Resize, .Rotate = transform_Rotate };

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
    .SetVisualState = window_SetVisualState, .WaitForInputIdle = window_WaitForInputIdle, .Close = window_Close };
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
    .Release = multiview_Release, .GetViewName = multiview_GetViewName, .SetCurrentView = multiview_SetCurrentView };

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
    .SetValue = legacy_SetValue, .GetIAccessible = legacy_GetIAccessible };

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

/* a provider whose pattern object is a bare IUnknown: the pattern interface is missing */
static struct { IUnknown unk; LONG ref; } bare_obj;
static HRESULT WINAPI bare_QI(IUnknown *iface, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown)) { *ppv = iface; return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI bare_AddRef(IUnknown *iface) { return 2; }
static ULONG WINAPI bare_Release(IUnknown *iface) { return 1; }
static IUnknownVtbl bare_vtbl = { bare_QI, bare_AddRef, bare_Release };

static IRawElementProviderSimple bare_simple;
static HRESULT WINAPI bare_GetPatternProvider(IRawElementProviderSimple *iface, PATTERNID id, IUnknown **ret)
{
    *ret = &bare_obj.unk;
    return S_OK;
}
static HRESULT WINAPI bare_simple_QI(IRawElementProviderSimple *iface, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IRawElementProviderSimple))
    {
        *ppv = iface;
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI bare_simple_AddRef(IRawElementProviderSimple *iface) { return 2; }
static ULONG WINAPI bare_simple_Release(IRawElementProviderSimple *iface) { return 1; }
static IRawElementProviderSimpleVtbl bare_simple_vtbl = {
    bare_simple_QI, bare_simple_AddRef, bare_simple_Release, simple_get_ProviderOptions, bare_GetPatternProvider,
    simple_GetPropertyValue, simple_get_HostRawElementProvider,
};

/* ---- the table ---- */

struct call
{
    const char *name;
    int pattern;
    HRESULT (*run)(PATOBJ obj);
    const char *log;
};

static HRESULT c_invoke(PATOBJ o) { return pInvoke(o); }
static HRESULT c_expand(PATOBJ o) { return pExpand(o); }
static HRESULT c_collapse(PATOBJ o) { return pCollapse(o); }
static HRESULT c_toggle(PATOBJ o) { return pToggle(o); }
static HRESULT c_value(PATOBJ o) { return pValueSet(o, L"hello world"); }
static HRESULT c_range(PATOBJ o) { return pRangeSet(o, 12.25); }
static HRESULT c_selsel(PATOBJ o) { return pSelSelect(o); }
static HRESULT c_seladd(PATOBJ o) { return pSelAdd(o); }
static HRESULT c_selrem(PATOBJ o) { return pSelRemove(o); }
static HRESULT c_siv(PATOBJ o) { return pScrollIntoView(o); }
static HRESULT c_scroll(PATOBJ o) { return pScroll(o, 2, 3); }
static HRESULT c_scrollpct(PATOBJ o) { return pScrollPercent(o, 12.5, 87.25); }
static HRESULT c_dock(PATOBJ o) { return pDock(o, 4); }
static HRESULT c_move(PATOBJ o) { return pMove(o, 10.5, -20); }
static HRESULT c_resize(PATOBJ o) { return pResize(o, 300, 150.5); }
static HRESULT c_rotate(PATOBJ o) { return pRotate(o, 45.5); }
static HRESULT c_close(PATOBJ o) { return pClose(o); }
static HRESULT c_visual(PATOBJ o) { return pVisualState(o, 2); }
static HRESULT c_realize(PATOBJ o) { return pRealize(o); }
static HRESULT c_setview(PATOBJ o) { return pSetView(o, 7); }
static HRESULT c_syncstart(PATOBJ o) { return pSyncStart(o, 3); }
static HRESULT c_synccancel(PATOBJ o) { return pSyncCancel(o); }
static HRESULT c_legselect(PATOBJ o) { return pLegSelect(o, 9); }
static HRESULT c_legaction(PATOBJ o) { return pLegAction(o); }
static HRESULT c_legvalue(PATOBJ o) { return pLegSetValue(o, L"legacy text"); }

static const struct call calls[] = {
    {"Invoke", UIA_InvokePatternId, c_invoke, "Invoke;"},
    {"Expand", UIA_ExpandCollapsePatternId, c_expand, "Expand;"},
    {"Collapse", UIA_ExpandCollapsePatternId, c_collapse, "Collapse;"},
    {"Toggle", UIA_TogglePatternId, c_toggle, "Toggle;"},
    {"Value.SetValue", UIA_ValuePatternId, c_value, "SetValue(hello world);"},
    {"RangeValue.SetValue", UIA_RangeValuePatternId, c_range, "RangeSet(12.25);"},
    {"SelectionItem.Select", UIA_SelectionItemPatternId, c_selsel, "Select;"},
    {"SelectionItem.AddToSelection", UIA_SelectionItemPatternId, c_seladd, "AddToSelection;"},
    {"SelectionItem.RemoveFromSelection", UIA_SelectionItemPatternId, c_selrem, "RemoveFromSelection;"},
    {"ScrollItem.ScrollIntoView", UIA_ScrollItemPatternId, c_siv, "ScrollIntoView;"},
    {"Scroll.Scroll", UIA_ScrollPatternId, c_scroll, "Scroll(2,3);"},
    {"Scroll.SetScrollPercent", UIA_ScrollPatternId, c_scrollpct, "ScrollPercent(12.5,87.25);"},
    {"Dock.SetDockPosition", UIA_DockPatternId, c_dock, "Dock(4);"},
    {"Transform.Move", UIA_TransformPatternId, c_move, "Move(10.5,-20);"},
    {"Transform.Resize", UIA_TransformPatternId, c_resize, "Resize(300,150.5);"},
    {"Transform.Rotate", UIA_TransformPatternId, c_rotate, "Rotate(45.5);"},
    {"Window.Close", UIA_WindowPatternId, c_close, "Close;"},
    {"Window.SetWindowVisualState", UIA_WindowPatternId, c_visual, "VisualState(2);"},
    {"VirtualizedItem.Realize", UIA_VirtualizedItemPatternId, c_realize, "Realize;"},
    {"MultipleView.SetCurrentView", UIA_MultipleViewPatternId, c_setview, "SetView(7);"},
    {"SynchronizedInput.StartListening", UIA_SynchronizedInputPatternId, c_syncstart, "SyncStart(3);"},
    {"SynchronizedInput.Cancel", UIA_SynchronizedInputPatternId, c_synccancel, "SyncCancel;"},
    {"LegacyIAccessible.Select", UIA_LegacyIAccessiblePatternId, c_legselect, "LegacySelect(9);"},
    {"LegacyIAccessible.DoDefaultAction", UIA_LegacyIAccessiblePatternId, c_legaction, "DoDefaultAction;"},
    {"LegacyIAccessible.SetValue", UIA_LegacyIAccessiblePatternId, c_legvalue, "LegacySetValue(legacy text);"},
};

static void test_table(HUIANODE node)
{
    unsigned i;

    for (i = 0; i < sizeof(calls) / sizeof(calls[0]); i++)
    {
        const struct call *c = &calls[i];
        PATOBJ obj = NULL;
        HRESULT hr;

        hr = pUiaGetPatternProvider(node, c->pattern, &obj);
        check(hr == S_OK && obj, "%s: pattern object obtained (%#lx)", c->name, hr);
        if (!obj) continue;

        logbuf[0] = 0;
        hr = c->run(obj);
        check(hr == S_OK, "%s: call returns %#lx", c->name, hr);
        check(!strcmp(logbuf, c->log), "%s: provider saw \"%s\" (wanted \"%s\")", c->name, logbuf, c->log);

        g_mock.fail = (HRESULT)0x80040200;
        logbuf[0] = 0;
        hr = c->run(obj);
        check(hr == (HRESULT)0x80040200, "%s: the provider's error comes back (%#lx)", c->name, hr);

        check(pUiaPatternRelease(obj) == TRUE, "%s: pattern object released", c->name);
    }
}

static void test_values(HUIANODE node)
{
    PATOBJ obj = NULL;
    BOOL idle = 5;
    BSTR name = NULL;
    HRESULT hr;

    pUiaGetPatternProvider(node, UIA_WindowPatternId, &obj);
    logbuf[0] = 0;
    hr = pWaitIdle(obj, 100, &idle);
    check(hr == S_OK && idle == TRUE && !strcmp(logbuf, "WaitIdle(100);"), "WaitForInputIdle(100) = TRUE (%#lx, %d, %s)", hr, idle, logbuf);
    hr = pWaitIdle(obj, 50, &idle);
    check(hr == S_OK && idle == FALSE, "WaitForInputIdle(50) = FALSE (%#lx, %d)", hr, idle);
    hr = pWaitIdle(obj, 50, NULL);
    check(hr == E_INVALIDARG, "WaitForInputIdle with no result pointer = %#lx", hr);
    pUiaPatternRelease(obj);

    pUiaGetPatternProvider(node, UIA_MultipleViewPatternId, &obj);
    logbuf[0] = 0;
    hr = pViewName(obj, 3, &name);
    check(hr == S_OK && name && !wcscmp(name, L"View 3") && !strcmp(logbuf, "ViewName(3);"), "GetViewName(3) = \"View 3\" (%#lx, %ls)", hr, name ? name : L"(null)");
    SysFreeString(name);
    hr = pViewName(obj, 3, NULL);
    check(hr == E_INVALIDARG, "GetViewName with no result pointer = %#lx", hr);
    pUiaPatternRelease(obj);
}

static void test_errors(HUIANODE node)
{
    PATOBJ obj = (PATOBJ)0x1234, invoke_obj = NULL, toggle_obj = NULL;
    HRESULT hr;

    hr = pUiaGetPatternProvider(node, UIA_InvokePatternId, NULL);
    check(hr == E_INVALIDARG, "UiaGetPatternProvider with no result pointer = %#lx", hr);
    hr = pUiaGetPatternProvider(NULL, UIA_InvokePatternId, &obj);
    check(hr == E_INVALIDARG && !obj, "UiaGetPatternProvider(NULL node) = %#lx, obj %p", hr, obj);

    g_mock.supported &= ~PAT(UIA_InvokePatternId);
    obj = (PATOBJ)0x1234;
    hr = pUiaGetPatternProvider(node, UIA_InvokePatternId, &obj);
    check(hr == S_OK && !obj, "an unsupported pattern gives S_OK and no object (%#lx, %p)", hr, obj);
    g_mock.supported |= PAT(UIA_InvokePatternId);

    check(pUiaPatternRelease(NULL) == FALSE, "UiaPatternRelease(NULL) = FALSE");

    hr = pInvoke(NULL);
    check(hr == E_INVALIDARG, "InvokePattern_Invoke(NULL) = %#lx", hr);
    hr = pValueSet(NULL, L"x");
    check(hr == E_INVALIDARG, "ValuePattern_SetValue(NULL) = %#lx", hr);

    pUiaGetPatternProvider(node, UIA_InvokePatternId, &invoke_obj);
    pUiaGetPatternProvider(node, UIA_TogglePatternId, &toggle_obj);
    hr = pToggle(invoke_obj);
    check(hr == E_INVALIDARG, "a pattern call on an object of another pattern = %#lx", hr);
    hr = pInvoke(toggle_obj);
    check(hr == E_INVALIDARG, "the same the other way round = %#lx", hr);
    hr = pValueSet(toggle_obj, NULL);
    check(hr == E_INVALIDARG, "ValuePattern_SetValue with a NULL string = %#lx", hr);
    pUiaPatternRelease(invoke_obj);
    pUiaPatternRelease(toggle_obj);
}

static void test_missing_interface(void)
{
    HUIANODE node = NULL;
    PATOBJ obj = NULL;
    HRESULT hr;

    bare_obj.unk.lpVtbl = &bare_vtbl;
    bare_simple.lpVtbl = &bare_simple_vtbl;

    hr = UiaNodeFromProvider(&bare_simple, &node);
    check(hr == S_OK && node, "node for the bare provider (%#lx)", hr);
    if (!node) return;

    hr = pUiaGetPatternProvider(node, UIA_InvokePatternId, &obj);
    check(hr == S_OK && obj, "the bare provider claims the pattern (%#lx)", hr);
    hr = pInvoke(obj);
    check(hr == (HRESULT)0x80040204, "its pattern object has no IInvokeProvider: UIA_E_NOTSUPPORTED (%#lx)", hr);
    pUiaPatternRelease(obj);
    UiaNodeRelease(node);
}

static void test_set_focus(void)
{
    HUIANODE node = NULL;
    HRESULT hr;

    mock_init(&g_mock);
    g_mock.has_fragment = TRUE;
    hr = UiaNodeFromProvider(&g_mock.simple, &node);
    check(hr == S_OK && node, "node for the fragment provider (%#lx)", hr);
    if (!node) return;

    logbuf[0] = 0;
    hr = pUiaSetFocus(node);
    check(hr == S_OK && !strcmp(logbuf, "SetFocus;"), "UiaSetFocus reaches IRawElementProviderFragment::SetFocus (%#lx, %s)", hr, logbuf);
    g_mock.fail = (HRESULT)0x80040201;
    hr = pUiaSetFocus(node);
    check(hr == (HRESULT)0x80040201, "UiaSetFocus returns the provider's error (%#lx)", hr);
    hr = pUiaSetFocus(NULL);
    check(hr == E_INVALIDARG, "UiaSetFocus(NULL) = %#lx", hr);
    UiaNodeRelease(node);

    mock_init(&g_mock);
    g_mock.has_fragment = FALSE;
    hr = UiaNodeFromProvider(&g_mock.simple, &node);
    hr = pUiaSetFocus(node);
    check(hr == (HRESULT)0x80040204, "UiaSetFocus on a provider without a fragment interface = UIA_E_NOTSUPPORTED (%#lx)", hr);
    UiaNodeRelease(node);
}

static void test_legacy_iaccessible(void)
{
    HUIANODE node = NULL;
    PATOBJ obj = NULL;
    IAccessible *acc = NULL, *got = NULL;
    HWND hwnd;
    HRESULT hr;
    BSTR name = NULL;
    VARIANT cid;

    hwnd = CreateWindowW(L"BUTTON", L"Probe button", WS_POPUP | WS_VISIBLE, 0, 0, 100, 40, NULL, NULL, NULL, NULL);
    if (!hwnd) { check(0, "test window created"); return; }
    hr = AccessibleObjectFromWindow(hwnd, OBJID_CLIENT, &IID_IAccessible, (void **)&acc);
    check(hr == S_OK && acc, "IAccessible of the test window (%#lx)", hr);
    if (!acc) { DestroyWindow(hwnd); return; }

    mock_init(&g_mock);
    g_mock.supported = PAT(UIA_LegacyIAccessiblePatternId);
    g_mock.acc = acc;
    UiaNodeFromProvider(&g_mock.simple, &node);
    pUiaGetPatternProvider(node, UIA_LegacyIAccessiblePatternId, &obj);

    hr = pLegGetAcc(obj, &got);
    check(hr == S_OK && got, "LegacyIAccessiblePattern_GetIAccessible hands back the accessible (%#lx)", hr);
    if (got)
    {
        VariantInit(&cid);
        V_VT(&cid) = VT_I4;
        V_I4(&cid) = CHILDID_SELF;
        hr = IAccessible_get_accName(got, cid, &name);
        check(hr == S_OK && name && !wcscmp(name, L"Probe button"), "and it is the button (%#lx, %ls)", hr, name ? name : L"(null)");
        SysFreeString(name);
        IAccessible_Release(got);
    }
    hr = pLegGetAcc(obj, NULL);
    check(hr == E_INVALIDARG, "GetIAccessible with no result pointer = %#lx", hr);

    pUiaPatternRelease(obj);
    UiaNodeRelease(node);
    IAccessible_Release(acc);
    DestroyWindow(hwnd);
}


/* ---- properties of the proxy over an IAccessible ---- */

struct macc
{
    IAccessible acc;
    IOleWindow ow;
    HWND hwnd;
    LONG ref;
};

static struct macc g_macc;

static inline struct macc *macc_from_acc(IAccessible *iface) { return CONTAINING_RECORD2(iface, struct macc, acc); }
static inline struct macc *macc_from_ow(IOleWindow *iface) { return CONTAINING_RECORD2(iface, struct macc, ow); }

static HRESULT WINAPI macc_QI(IAccessible *iface, REFIID riid, void **ppv)
{
    struct macc *m = macc_from_acc(iface);

    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDispatch) || IsEqualIID(riid, &IID_IAccessible))
        *ppv = &m->acc;
    else if (IsEqualIID(riid, &IID_IOleWindow))
        *ppv = &m->ow;
    else
        return E_NOINTERFACE;
    return S_OK;
}
static ULONG WINAPI macc_AddRef(IAccessible *iface) { return 2; }
static ULONG WINAPI macc_Release(IAccessible *iface) { return 1; }
static HRESULT WINAPI macc_ow_QI(IOleWindow *iface, REFIID riid, void **ppv) { return macc_QI(&macc_from_ow(iface)->acc, riid, ppv); }
static ULONG WINAPI macc_ow_AddRef(IOleWindow *iface) { return 2; }
static ULONG WINAPI macc_ow_Release(IOleWindow *iface) { return 1; }
static HRESULT WINAPI macc_ow_GetWindow(IOleWindow *iface, HWND *hwnd)
{
    *hwnd = macc_from_ow(iface)->hwnd;
    return S_OK;
}
static HRESULT WINAPI macc_ow_Help(IOleWindow *iface, BOOL mode) { return E_NOTIMPL; }
static IOleWindowVtbl macc_ow_vtbl = { macc_ow_QI, macc_ow_AddRef, macc_ow_Release, macc_ow_GetWindow, macc_ow_Help };

static HRESULT str_out(const WCHAR *text, BSTR *out)
{
    if (!text) return DISP_E_MEMBERNOTFOUND;
    *out = SysAllocString(text);
    return S_OK;
}
static HRESULT WINAPI macc_get_accName(IAccessible *iface, VARIANT cid, BSTR *out) { return str_out(L"Open file", out); }
static HRESULT WINAPI macc_get_accValue(IAccessible *iface, VARIANT cid, BSTR *out) { return str_out(L"the value", out); }
static HRESULT WINAPI macc_get_accDescription(IAccessible *iface, VARIANT cid, BSTR *out) { return str_out(L"the description", out); }
static HRESULT WINAPI macc_get_accHelp(IAccessible *iface, VARIANT cid, BSTR *out) { return str_out(L"Opens a file", out); }
static HRESULT WINAPI macc_get_accKeyboardShortcut(IAccessible *iface, VARIANT cid, BSTR *out) { return str_out(L"Alt+O", out); }
static HRESULT WINAPI macc_get_accDefaultAction(IAccessible *iface, VARIANT cid, BSTR *out) { return str_out(L"Press", out); }
static HRESULT WINAPI macc_get_accRole(IAccessible *iface, VARIANT cid, VARIANT *out)
{
    V_VT(out) = VT_I4;
    V_I4(out) = ROLE_SYSTEM_PUSHBUTTON;
    return S_OK;
}
static HRESULT WINAPI macc_get_accState(IAccessible *iface, VARIANT cid, VARIANT *out)
{
    V_VT(out) = VT_I4;
    V_I4(out) = STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_FOCUSED;
    return S_OK;
}
static HRESULT WINAPI macc_accLocation(IAccessible *iface, LONG *left, LONG *top, LONG *width, LONG *height, VARIANT cid)
{
    *left = 40; *top = 60; *width = 120; *height = 30;
    return S_OK;
}
static HRESULT WINAPI macc_get_accChildCount(IAccessible *iface, LONG *count)
{
    *count = 0;
    return S_OK;
}
static IAccessibleVtbl macc_vtbl = { .QueryInterface = macc_QI, .AddRef = macc_AddRef, .Release = macc_Release,
    .get_accName = macc_get_accName, .get_accValue = macc_get_accValue, .get_accDescription = macc_get_accDescription,
    .get_accHelp = macc_get_accHelp, .get_accKeyboardShortcut = macc_get_accKeyboardShortcut,
    .get_accDefaultAction = macc_get_accDefaultAction, .get_accRole = macc_get_accRole,
    .get_accState = macc_get_accState, .accLocation = macc_accLocation, .get_accChildCount = macc_get_accChildCount };

static BOOL bstr_is(const VARIANT *v, const WCHAR *want)
{
    return V_VT(v) == VT_BSTR && V_BSTR(v) && !wcscmp(V_BSTR(v), want);
}

static void test_msaa_props(void)
{
    IRawElementProviderSimple *prov = NULL;
    HWND hwnd;
    VARIANT v;
    HRESULT hr;
    double *rect;

    hwnd = CreateWindowW(L"BUTTON", L"host", WS_POPUP | WS_VISIBLE, 0, 0, 50, 50, NULL, NULL, NULL, NULL);
    if (!hwnd) { check(0, "msaa: test window created"); return; }
    g_macc.acc.lpVtbl = &macc_vtbl;
    g_macc.ow.lpVtbl = &macc_ow_vtbl;
    g_macc.hwnd = hwnd;

    hr = UiaProviderFromIAccessible(&g_macc.acc, CHILDID_SELF, 0, &prov);
    check(hr == S_OK && prov, "msaa: provider from IAccessible (%#lx)", hr);
    if (!prov) goto done;

    hr = IRawElementProviderSimple_GetPropertyValue(prov, UIA_ProcessIdPropertyId, &v);
    check(hr == S_OK && V_VT(&v) == VT_I4 && (DWORD)V_I4(&v) == GetCurrentProcessId(), "msaa: ProcessId is ours (%#lx, vt %d)", hr, V_VT(&v));
    VariantClear(&v);

    hr = IRawElementProviderSimple_GetPropertyValue(prov, UIA_ClassNamePropertyId, &v);
    check(hr == S_OK && bstr_is(&v, L"Button"), "msaa: ClassName is Button (%#lx, vt %d)", hr, V_VT(&v));
    VariantClear(&v);

    hr = IRawElementProviderSimple_GetPropertyValue(prov, UIA_BoundingRectanglePropertyId, &v);
    check(hr == S_OK && V_VT(&v) == (VT_R8 | VT_ARRAY) && V_ARRAY(&v), "msaa: BoundingRectangle is an array of doubles (%#lx, vt %#x)", hr, V_VT(&v));
    if (V_VT(&v) == (VT_R8 | VT_ARRAY) && V_ARRAY(&v) && SUCCEEDED(SafeArrayAccessData(V_ARRAY(&v), (void **)&rect)))
    {
        LONG n = 0;
        SafeArrayGetUBound(V_ARRAY(&v), 1, &n);
        check(n == 3, "msaa: BoundingRectangle has four elements (%ld)", n + 1);
        check(rect[0] == 40 && rect[1] == 60 && rect[2] == 120 && rect[3] == 30,
              "msaa: BoundingRectangle is left, top, width, height (%g %g %g %g)", rect[0], rect[1], rect[2], rect[3]);
        SafeArrayUnaccessData(V_ARRAY(&v));
    }
    VariantClear(&v);

    hr = IRawElementProviderSimple_GetPropertyValue(prov, UIA_AccessKeyPropertyId, &v);
    check(hr == S_OK && bstr_is(&v, L"Alt+O"), "msaa: AccessKey is the keyboard shortcut (vt %d)", V_VT(&v));
    VariantClear(&v);
    hr = IRawElementProviderSimple_GetPropertyValue(prov, UIA_HelpTextPropertyId, &v);
    check(hr == S_OK && bstr_is(&v, L"Opens a file"), "msaa: HelpText is accHelp (vt %d)", V_VT(&v));
    VariantClear(&v);

    hr = IRawElementProviderSimple_GetPropertyValue(prov, UIA_LegacyIAccessibleChildIdPropertyId, &v);
    check(hr == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == CHILDID_SELF, "msaa: Legacy ChildId is 0 (vt %d)", V_VT(&v));
    hr = IRawElementProviderSimple_GetPropertyValue(prov, UIA_LegacyIAccessibleRolePropertyId, &v);
    check(hr == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == ROLE_SYSTEM_PUSHBUTTON, "msaa: Legacy Role is the push button role (vt %d, %ld)", V_VT(&v), V_I4(&v));
    hr = IRawElementProviderSimple_GetPropertyValue(prov, UIA_LegacyIAccessibleStatePropertyId, &v);
    check(hr == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == (STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_FOCUSED), "msaa: Legacy State (vt %d, %#lx)", V_VT(&v), V_I4(&v));
    hr = IRawElementProviderSimple_GetPropertyValue(prov, UIA_LegacyIAccessibleNamePropertyId, &v);
    check(hr == S_OK && bstr_is(&v, L"Open file"), "msaa: Legacy Name (vt %d)", V_VT(&v));
    VariantClear(&v);
    hr = IRawElementProviderSimple_GetPropertyValue(prov, UIA_LegacyIAccessibleValuePropertyId, &v);
    check(hr == S_OK && bstr_is(&v, L"the value"), "msaa: Legacy Value (vt %d)", V_VT(&v));
    VariantClear(&v);
    hr = IRawElementProviderSimple_GetPropertyValue(prov, UIA_LegacyIAccessibleDescriptionPropertyId, &v);
    check(hr == S_OK && bstr_is(&v, L"the description"), "msaa: Legacy Description (vt %d)", V_VT(&v));
    VariantClear(&v);
    hr = IRawElementProviderSimple_GetPropertyValue(prov, UIA_LegacyIAccessibleHelpPropertyId, &v);
    check(hr == S_OK && bstr_is(&v, L"Opens a file"), "msaa: Legacy Help (vt %d)", V_VT(&v));
    VariantClear(&v);
    hr = IRawElementProviderSimple_GetPropertyValue(prov, UIA_LegacyIAccessibleKeyboardShortcutPropertyId, &v);
    check(hr == S_OK && bstr_is(&v, L"Alt+O"), "msaa: Legacy KeyboardShortcut (vt %d)", V_VT(&v));
    VariantClear(&v);
    hr = IRawElementProviderSimple_GetPropertyValue(prov, UIA_LegacyIAccessibleDefaultActionPropertyId, &v);
    check(hr == S_OK && bstr_is(&v, L"Press"), "msaa: Legacy DefaultAction (vt %d)", V_VT(&v));
    VariantClear(&v);

    IRawElementProviderSimple_Release(prov);
    prov = NULL;

    /* a child id other than SELF has no window class of its own */
    hr = UiaProviderFromIAccessible(&g_macc.acc, 1, 0, &prov);
    check(hr == S_OK && prov, "msaa: provider for child 1 (%#lx)", hr);
    if (prov)
    {
        hr = IRawElementProviderSimple_GetPropertyValue(prov, UIA_ClassNamePropertyId, &v);
        check(hr == S_OK && V_VT(&v) == VT_EMPTY, "msaa: no ClassName for a child id (vt %d)", V_VT(&v));
        VariantClear(&v);
        hr = IRawElementProviderSimple_GetPropertyValue(prov, UIA_LegacyIAccessibleChildIdPropertyId, &v);
        check(hr == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == 1, "msaa: ChildId of a child is 1 (vt %d)", V_VT(&v));
    }

done:
    if (prov) IRawElementProviderSimple_Release(prov);
    DestroyWindow(hwnd);
}

#define LOAD(var, name) do { var = (void *)GetProcAddress(mod, name); if (!var) { printf("FAIL  no %s\nRESULT: FAIL\n", name); return 1; } } while (0)

int main(void)
{
    HMODULE mod = LoadLibraryA("uiautomationcore.dll");
    HUIANODE node = NULL;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (!mod) { printf("FAIL  no uiautomationcore\nRESULT: FAIL\n"); return 1; }
    LOAD(pUiaGetPatternProvider, "UiaGetPatternProvider");
    LOAD(pUiaPatternRelease, "UiaPatternRelease");
    LOAD(pUiaSetFocus, "UiaSetFocus");
    LOAD(pInvoke, "InvokePattern_Invoke");
    LOAD(pExpand, "ExpandCollapsePattern_Expand");
    LOAD(pCollapse, "ExpandCollapsePattern_Collapse");
    LOAD(pToggle, "TogglePattern_Toggle");
    LOAD(pValueSet, "ValuePattern_SetValue");
    LOAD(pRangeSet, "RangeValuePattern_SetValue");
    LOAD(pSelSelect, "SelectionItemPattern_Select");
    LOAD(pSelAdd, "SelectionItemPattern_AddToSelection");
    LOAD(pSelRemove, "SelectionItemPattern_RemoveFromSelection");
    LOAD(pScrollIntoView, "ScrollItemPattern_ScrollIntoView");
    LOAD(pScroll, "ScrollPattern_Scroll");
    LOAD(pScrollPercent, "ScrollPattern_SetScrollPercent");
    LOAD(pDock, "DockPattern_SetDockPosition");
    LOAD(pMove, "TransformPattern_Move");
    LOAD(pResize, "TransformPattern_Resize");
    LOAD(pRotate, "TransformPattern_Rotate");
    LOAD(pClose, "WindowPattern_Close");
    LOAD(pVisualState, "WindowPattern_SetWindowVisualState");
    LOAD(pWaitIdle, "WindowPattern_WaitForInputIdle");
    LOAD(pRealize, "VirtualizedItemPattern_Realize");
    LOAD(pViewName, "MultipleViewPattern_GetViewName");
    LOAD(pSetView, "MultipleViewPattern_SetCurrentView");
    LOAD(pSyncStart, "SynchronizedInputPattern_StartListening");
    LOAD(pSyncCancel, "SynchronizedInputPattern_Cancel");
    LOAD(pLegSelect, "LegacyIAccessiblePattern_Select");
    LOAD(pLegAction, "LegacyIAccessiblePattern_DoDefaultAction");
    LOAD(pLegSetValue, "LegacyIAccessiblePattern_SetValue");
    LOAD(pLegGetAcc, "LegacyIAccessiblePattern_GetIAccessible");

    mock_init(&g_mock);
    g_mock.supported = 0xffffffffu;
    hr = UiaNodeFromProvider(&g_mock.simple, &node);
    if (FAILED(hr) || !node) { printf("FAIL  UiaNodeFromProvider %#lx\nRESULT: FAIL\n", hr); return 1; }

    test_table(node);
    test_values(node);
    test_errors(node);
    UiaNodeRelease(node);
    test_missing_interface();
    test_set_focus();
    test_legacy_iaccessible();
    test_msaa_props();

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
