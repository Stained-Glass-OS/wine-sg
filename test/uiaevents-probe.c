/* UI Automation events with arguments and the MSAA bridge (patches/sg/1683),
 * run by test/uiaevents-gate.sh on Xvfb.
 *
 * A child copy of this program ("server") owns a window whose UI Automation
 * provider raises, when told, a property change (HelpText, then Name), a
 * structure change, a notification, two text edit changes (composition,
 * then autocorrect) and a changes event. This process listens with the COM
 * client's handlers, so the arguments cross between processes, and checks
 * what arrives: only the property it asked for, only the text edit change
 * type it asked for, every argument intact.
 *
 * Then, in this process, UiaProviderFromIAccessible over an IAccessible of
 * our own: ILegacyIAccessibleProvider's name, value, state, default action,
 * SetValue, Select; SetFocus; ElementProviderFromPoint; and the MSAA
 * events as UI Automation events. These were stubs (E_NOTIMPL). */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <oleacc.h>
#include <uiautomation.h>
#include <stdio.h>

#ifndef UIA_SummaryChangeId
#define UIA_SummaryChangeId 90000
#endif
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

static HRESULT (WINAPI *pUiaRaiseNotificationEvent)(IRawElementProviderSimple *, int, int, BSTR, BSTR);
static HRESULT (WINAPI *pUiaRaiseTextEditTextChangedEvent)(IRawElementProviderSimple *, int, SAFEARRAY *);
static HRESULT (WINAPI *pUiaRaiseChangesEvent)(IRawElementProviderSimple *, int, struct UiaChangeInfo *);

/* ---------------- the server: a provider that raises events ---------------- */

static HWND server_hwnd;

static HRESULT WINAPI prov_QueryInterface(IRawElementProviderSimple *iface, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IRawElementProviderSimple))
    {
        *ppv = iface;
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI prov_AddRef(IRawElementProviderSimple *iface) { return 2; }
static ULONG WINAPI prov_Release(IRawElementProviderSimple *iface) { return 1; }
static HRESULT WINAPI prov_get_ProviderOptions(IRawElementProviderSimple *iface, enum ProviderOptions *ret)
{
    *ret = ProviderOptions_ServerSideProvider;
    return S_OK;
}
static HRESULT WINAPI prov_GetPatternProvider(IRawElementProviderSimple *iface, PATTERNID id, IUnknown **ret)
{
    *ret = NULL;
    return S_OK;
}
static HRESULT WINAPI prov_GetPropertyValue(IRawElementProviderSimple *iface, PROPERTYID id, VARIANT *ret)
{
    VariantInit(ret);
    if (id == UIA_NamePropertyId)
    {
        V_VT(ret) = VT_BSTR;
        V_BSTR(ret) = SysAllocString(L"SG events element");
    }
    return S_OK;
}
static HRESULT WINAPI prov_get_HostRawElementProvider(IRawElementProviderSimple *iface, IRawElementProviderSimple **ret)
{
    return UiaHostProviderFromHwnd(server_hwnd, ret);
}
static IRawElementProviderSimpleVtbl prov_vtbl = {
    prov_QueryInterface, prov_AddRef, prov_Release, prov_get_ProviderOptions,
    prov_GetPatternProvider, prov_GetPropertyValue, prov_get_HostRawElementProvider,
};
static IRawElementProviderSimple prov = { &prov_vtbl };

static LRESULT CALLBACK server_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_GETOBJECT && (LONG)lp == UiaRootObjectId)
        return UiaReturnRawElementProvider(hwnd, wp, lp, &prov);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void raise_all(void)
{
    VARIANT old, new;
    int rt[2] = { 42, 7 };
    SAFEARRAY *sa;
    LONG idx = 0;
    BSTR str;
    struct UiaChangeInfo change;

    VariantInit(&old);
    V_VT(&new) = VT_BSTR;
    V_BSTR(&new) = SysAllocString(L"some help");
    UiaRaiseAutomationPropertyChangedEvent(&prov, UIA_HelpTextPropertyId, old, new);
    VariantClear(&new);
    V_VT(&old) = VT_BSTR;
    V_BSTR(&old) = SysAllocString(L"old name");
    V_VT(&new) = VT_BSTR;
    V_BSTR(&new) = SysAllocString(L"new name");
    UiaRaiseAutomationPropertyChangedEvent(&prov, UIA_NamePropertyId, old, new);
    VariantClear(&old);
    VariantClear(&new);

    UiaRaiseStructureChangedEvent(&prov, StructureChangeType_ChildAdded, rt, 2);

    pUiaRaiseNotificationEvent(&prov, NotificationKind_ActionCompleted, NotificationProcessing_MostRecent,
                               SysAllocString(L"Saved"), SysAllocString(L"sg.save"));

    sa = SafeArrayCreateVector(VT_BSTR, 0, 1);
    str = SysAllocString(L"composing");
    SafeArrayPutElement(sa, &idx, str);
    SysFreeString(str);
    pUiaRaiseTextEditTextChangedEvent(&prov, TextEditChangeType_Composition, sa);
    SafeArrayDestroy(sa);
    sa = SafeArrayCreateVector(VT_BSTR, 0, 1);
    str = SysAllocString(L"fixed");
    SafeArrayPutElement(sa, &idx, str);
    SysFreeString(str);
    pUiaRaiseTextEditTextChangedEvent(&prov, TextEditChangeType_AutoCorrect, sa);
    SafeArrayDestroy(sa);

    change.uiaId = UIA_SummaryChangeId;
    V_VT(&change.payload) = VT_I4;
    V_I4(&change.payload) = 1234;
    V_VT(&change.extraInfo) = VT_BSTR;
    V_BSTR(&change.extraInfo) = SysAllocString(L"extra");
    pUiaRaiseChangesEvent(&prov, 1, &change);
    VariantClear(&change.extraInfo);
}

static int server(void)
{
    WNDCLASSW wc = { 0 };
    HANDLE go = CreateEventW(NULL, TRUE, FALSE, L"SgUiaEvGo");
    HANDLE quit = CreateEventW(NULL, TRUE, FALSE, L"SgUiaEvQuit");
    MSG msg;
    DWORD r;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    wc.lpfnWndProc = server_proc;
    wc.lpszClassName = L"SgUiaEvServer";
    RegisterClassW(&wc);
    server_hwnd = CreateWindowW(L"SgUiaEvServer", L"SG events", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                10, 10, 200, 100, NULL, NULL, NULL, NULL);
    for (;;)
    {
        HANDLE h[2] = { go, quit };
        r = MsgWaitForMultipleObjects(2, h, FALSE, 30000, QS_ALLINPUT);
        if (r == WAIT_OBJECT_0)
        {
            ResetEvent(go);
            raise_all();
        }
        else if (r == WAIT_OBJECT_0 + 1 || r == WAIT_TIMEOUT) break;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return 0;
}

/* ---------------- the client's handlers ---------------- */

struct handler
{
    IUnknown iface; /* any of the handler interfaces: same first three slots */
    HANDLE done;
    int calls;
    PROPERTYID prop;
    WCHAR str[64];
    WCHAR str2[64];
    int kind, processing, count;
    int rt[4];
    LONG payload;
};

#define HANDLER(iface) ((struct handler *)(iface))

static HRESULT WINAPI h_QueryInterface(IUnknown *iface, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IUIAutomationPropertyChangedEventHandler) ||
        IsEqualIID(riid, &IID_IUIAutomationStructureChangedEventHandler) ||
        IsEqualIID(riid, &IID_IUIAutomationNotificationEventHandler) ||
        IsEqualIID(riid, &IID_IUIAutomationTextEditTextChangedEventHandler) ||
        IsEqualIID(riid, &IID_IUIAutomationChangesEventHandler))
    {
        *ppv = iface;
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI h_AddRef(IUnknown *iface) { return 2; }
static ULONG WINAPI h_Release(IUnknown *iface) { return 1; }

static HRESULT WINAPI prop_handle(IUIAutomationPropertyChangedEventHandler *iface, IUIAutomationElement *elem,
        PROPERTYID id, VARIANT v)
{
    struct handler *h = HANDLER(iface);
    h->calls++;
    h->prop = id;
    if (V_VT(&v) == VT_BSTR) lstrcpynW(h->str, V_BSTR(&v), 64);
    SetEvent(h->done);
    return S_OK;
}
static HRESULT WINAPI struct_handle(IUIAutomationStructureChangedEventHandler *iface, IUIAutomationElement *elem,
        enum StructureChangeType type, SAFEARRAY *rt)
{
    struct handler *h = HANDLER(iface);
    LONG i, ub = -1;
    h->calls++;
    h->kind = type;
    if (rt && SUCCEEDED(SafeArrayGetUBound(rt, 1, &ub)))
        for (i = 0; i <= ub && i < 4; i++) SafeArrayGetElement(rt, &i, &h->rt[i]);
    h->count = ub + 1;
    SetEvent(h->done);
    return S_OK;
}
static HRESULT WINAPI notif_handle(IUIAutomationNotificationEventHandler *iface, IUIAutomationElement *elem,
        enum NotificationKind kind, enum NotificationProcessing processing, BSTR display, BSTR activity)
{
    struct handler *h = HANDLER(iface);
    h->calls++;
    h->kind = kind;
    h->processing = processing;
    if (display) lstrcpynW(h->str, display, 64);
    if (activity) lstrcpynW(h->str2, activity, 64);
    SetEvent(h->done);
    return S_OK;
}
static HRESULT WINAPI text_handle(IUIAutomationTextEditTextChangedEventHandler *iface, IUIAutomationElement *elem,
        enum TextEditChangeType type, SAFEARRAY *data)
{
    struct handler *h = HANDLER(iface);
    LONG i = 0;
    BSTR s = NULL;
    h->calls++;
    h->kind = type;
    if (data && SUCCEEDED(SafeArrayGetElement(data, &i, &s)) && s) lstrcpynW(h->str, s, 64);
    SysFreeString(s);
    SetEvent(h->done);
    return S_OK;
}
static HRESULT WINAPI changes_handle(IUIAutomationChangesEventHandler *iface, IUIAutomationElement *elem,
        struct UiaChangeInfo *changes, int count)
{
    struct handler *h = HANDLER(iface);
    h->calls++;
    h->count = count;
    if (count > 0)
    {
        h->kind = changes[0].uiaId;
        if (V_VT(&changes[0].payload) == VT_I4) h->payload = V_I4(&changes[0].payload);
        if (V_VT(&changes[0].extraInfo) == VT_BSTR) lstrcpynW(h->str, V_BSTR(&changes[0].extraInfo), 64);
    }
    SetEvent(h->done);
    return S_OK;
}

static void *prop_vtbl[] = { h_QueryInterface, h_AddRef, h_Release, prop_handle };
static void *struct_vtbl[] = { h_QueryInterface, h_AddRef, h_Release, struct_handle };
static void *notif_vtbl[] = { h_QueryInterface, h_AddRef, h_Release, notif_handle };
static void *text_vtbl[] = { h_QueryInterface, h_AddRef, h_Release, text_handle };
static void *changes_vtbl[] = { h_QueryInterface, h_AddRef, h_Release, changes_handle };

static struct handler prop_h, struct_h, notif_h, text_h, changes_h;

static void init_handler(struct handler *h, void **vtbl)
{
    h->iface.lpVtbl = (void *)vtbl;
    h->done = CreateEventW(NULL, FALSE, FALSE, NULL);
}

/* ---------------- an IAccessible of our own ---------------- */

static HWND acc_hwnd;
static WCHAR acc_value[64] = L"first value";
static int acc_default_actions, acc_select_flags, acc_hits;

static HRESULT WINAPI acc_QueryInterface(IAccessible *iface, REFIID riid, void **ppv);
static ULONG WINAPI acc_AddRef(IAccessible *iface) { return 2; }
static ULONG WINAPI acc_Release(IAccessible *iface) { return 1; }
static HRESULT WINAPI acc_GetTypeInfoCount(IAccessible *iface, UINT *n) { return E_NOTIMPL; }
static HRESULT WINAPI acc_GetTypeInfo(IAccessible *iface, UINT i, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI acc_GetIDsOfNames(IAccessible *iface, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d) { return E_NOTIMPL; }
static HRESULT WINAPI acc_Invoke(IAccessible *iface, DISPID d, REFIID r, LCID l, WORD f, DISPPARAMS *p, VARIANT *v,
        EXCEPINFO *e, UINT *a) { return E_NOTIMPL; }
static HRESULT WINAPI acc_get_accParent(IAccessible *iface, IDispatch **out) { *out = NULL; return S_FALSE; }
static HRESULT WINAPI acc_get_accChildCount(IAccessible *iface, LONG *out) { *out = 2; return S_OK; }
static HRESULT WINAPI acc_get_accChild(IAccessible *iface, VARIANT cid, IDispatch **out) { *out = NULL; return S_FALSE; }
static HRESULT WINAPI acc_get_accName(IAccessible *iface, VARIANT cid, BSTR *out)
{
    *out = SysAllocString(V_I4(&cid) == 2 ? L"second child" : L"SG accessible");
    return S_OK;
}
static HRESULT WINAPI acc_get_accValue(IAccessible *iface, VARIANT cid, BSTR *out)
{
    *out = SysAllocString(acc_value);
    return S_OK;
}
static HRESULT WINAPI acc_get_accDescription(IAccessible *iface, VARIANT cid, BSTR *out)
{
    *out = SysAllocString(L"a description");
    return S_OK;
}
static HRESULT WINAPI acc_get_accRole(IAccessible *iface, VARIANT cid, VARIANT *out)
{
    V_VT(out) = VT_I4;
    V_I4(out) = ROLE_SYSTEM_LIST;
    return S_OK;
}
static HRESULT WINAPI acc_get_accState(IAccessible *iface, VARIANT cid, VARIANT *out)
{
    V_VT(out) = VT_I4;
    V_I4(out) = STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_SELECTABLE;
    return S_OK;
}
static HRESULT WINAPI acc_get_accHelp(IAccessible *iface, VARIANT cid, BSTR *out)
{
    *out = SysAllocString(L"some help");
    return S_OK;
}
static HRESULT WINAPI acc_get_accHelpTopic(IAccessible *iface, BSTR *f, VARIANT cid, LONG *t) { return E_NOTIMPL; }
static HRESULT WINAPI acc_get_accKeyboardShortcut(IAccessible *iface, VARIANT cid, BSTR *out)
{
    *out = SysAllocString(L"Alt+S");
    return S_OK;
}
static HRESULT WINAPI acc_get_accFocus(IAccessible *iface, VARIANT *out) { VariantInit(out); return S_FALSE; }
static HRESULT WINAPI acc_get_accSelection(IAccessible *iface, VARIANT *out)
{
    V_VT(out) = VT_I4;
    V_I4(out) = 2;
    return S_OK;
}
static HRESULT WINAPI acc_get_accDefaultAction(IAccessible *iface, VARIANT cid, BSTR *out)
{
    *out = SysAllocString(L"Press");
    return S_OK;
}
static HRESULT WINAPI acc_accSelect(IAccessible *iface, LONG flags, VARIANT cid)
{
    acc_select_flags |= flags;
    return S_OK;
}
static HRESULT WINAPI acc_accLocation(IAccessible *iface, LONG *l, LONG *t, LONG *w, LONG *h, VARIANT cid)
{
    *l = 0; *t = 0; *w = 100; *h = 100;
    return S_OK;
}
static HRESULT WINAPI acc_accNavigate(IAccessible *iface, LONG d, VARIANT s, VARIANT *out) { return E_NOTIMPL; }
static HRESULT WINAPI acc_accHitTest(IAccessible *iface, LONG x, LONG y, VARIANT *out)
{
    acc_hits++;
    V_VT(out) = VT_I4;
    V_I4(out) = x > 50 ? 2 : CHILDID_SELF;
    return S_OK;
}
static HRESULT WINAPI acc_accDoDefaultAction(IAccessible *iface, VARIANT cid)
{
    acc_default_actions++;
    return S_OK;
}
static HRESULT WINAPI acc_put_accName(IAccessible *iface, VARIANT cid, BSTR v) { return E_NOTIMPL; }
static HRESULT WINAPI acc_put_accValue(IAccessible *iface, VARIANT cid, BSTR v)
{
    lstrcpynW(acc_value, v, 64);
    return S_OK;
}

static IAccessibleVtbl acc_vtbl = {
    acc_QueryInterface, acc_AddRef, acc_Release, acc_GetTypeInfoCount, acc_GetTypeInfo, acc_GetIDsOfNames,
    acc_Invoke, acc_get_accParent, acc_get_accChildCount, acc_get_accChild, acc_get_accName, acc_get_accValue,
    acc_get_accDescription, acc_get_accRole, acc_get_accState, acc_get_accHelp, acc_get_accHelpTopic,
    acc_get_accKeyboardShortcut, acc_get_accFocus, acc_get_accSelection, acc_get_accDefaultAction,
    acc_accSelect, acc_accLocation, acc_accNavigate, acc_accHitTest, acc_accDoDefaultAction,
    acc_put_accName, acc_put_accValue,
};
static IAccessible acc = { &acc_vtbl };

static HRESULT WINAPI ow_QueryInterface(IOleWindow *iface, REFIID riid, void **ppv)
{
    return acc_QueryInterface(&acc, riid, ppv);
}
static ULONG WINAPI ow_AddRef(IOleWindow *iface) { return 2; }
static ULONG WINAPI ow_Release(IOleWindow *iface) { return 1; }
static HRESULT WINAPI ow_GetWindow(IOleWindow *iface, HWND *hwnd) { *hwnd = acc_hwnd; return S_OK; }
static HRESULT WINAPI ow_ContextSensitiveHelp(IOleWindow *iface, BOOL b) { return E_NOTIMPL; }
static IOleWindowVtbl ow_vtbl = { ow_QueryInterface, ow_AddRef, ow_Release, ow_GetWindow, ow_ContextSensitiveHelp };
static IOleWindow ow = { &ow_vtbl };

static HRESULT WINAPI acc_QueryInterface(IAccessible *iface, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDispatch) || IsEqualIID(riid, &IID_IAccessible))
    {
        *ppv = &acc;
        return S_OK;
    }
    if (IsEqualIID(riid, &IID_IOleWindow))
    {
        *ppv = &ow;
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

/* a WinEvent sink that records what the bridge adds */
static int sink_prop_calls, sink_struct_calls, sink_event_calls;
static PROPERTYID sink_prop;
static WCHAR sink_value[64];
static EVENTID sink_event;
static int sink_struct_type;

static HRESULT WINAPI sink_QueryInterface(IProxyProviderWinEventSink *iface, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IProxyProviderWinEventSink))
    {
        *ppv = iface;
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI sink_AddRef(IProxyProviderWinEventSink *iface) { return 2; }
static ULONG WINAPI sink_Release(IProxyProviderWinEventSink *iface) { return 1; }
static HRESULT WINAPI sink_AddAutomationPropertyChangedEvent(IProxyProviderWinEventSink *iface,
        IRawElementProviderSimple *p, PROPERTYID id, VARIANT v)
{
    sink_prop_calls++;
    sink_prop = id;
    if (V_VT(&v) == VT_BSTR) lstrcpynW(sink_value, V_BSTR(&v), 64);
    return S_OK;
}
static HRESULT WINAPI sink_AddAutomationEvent(IProxyProviderWinEventSink *iface, IRawElementProviderSimple *p, EVENTID id)
{
    sink_event_calls++;
    sink_event = id;
    return S_OK;
}
static HRESULT WINAPI sink_AddStructureChangedEvent(IProxyProviderWinEventSink *iface, IRawElementProviderSimple *p,
        enum StructureChangeType type, SAFEARRAY *rt)
{
    sink_struct_calls++;
    sink_struct_type = type;
    return S_OK;
}
static IProxyProviderWinEventSinkVtbl sink_vtbl = {
    sink_QueryInterface, sink_AddRef, sink_Release, sink_AddAutomationPropertyChangedEvent,
    sink_AddAutomationEvent, sink_AddStructureChangedEvent,
};
static IProxyProviderWinEventSink sink = { &sink_vtbl };

static LRESULT CALLBACK acc_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_GETOBJECT && (LONG)lp == OBJID_CLIENT)
        return LresultFromObject(&IID_IAccessible, wp, (IUnknown *)&acc);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void test_bridge(void)
{
    IRawElementProviderSimple *elprov = NULL, *child;
    IRawElementProviderFragmentRoot *root;
    IRawElementProviderFragment *frag;
    ILegacyIAccessibleProvider *legacy;
    IProxyProviderWinEventHandler *weh;
    WNDCLASSW wc = { 0 };
    SAFEARRAY *sel = NULL;
    DWORD state = 0;
    BSTR s = NULL;
    HRESULT hr;

    wc.lpfnWndProc = acc_proc;
    wc.lpszClassName = L"SgUiaAcc";
    RegisterClassW(&wc);
    acc_hwnd = CreateWindowW(L"SgUiaAcc", L"acc", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL);

    hr = UiaProviderFromIAccessible(&acc, CHILDID_SELF, UIA_PFIA_DEFAULT, &elprov);
    check(hr == S_OK && elprov, "UiaProviderFromIAccessible");
    if (!elprov) return;
    hr = IRawElementProviderSimple_QueryInterface(elprov, &IID_ILegacyIAccessibleProvider, (void **)&legacy);
    check(hr == S_OK, "ILegacyIAccessibleProvider");
    if (hr == S_OK)
    {
        hr = ILegacyIAccessibleProvider_get_Name(legacy, &s);
        check(hr == S_OK && s && !lstrcmpW(s, L"SG accessible"), "get_Name: the IAccessible's name");
        SysFreeString(s); s = NULL;
        hr = ILegacyIAccessibleProvider_get_Description(legacy, &s);
        check(hr == S_OK && s && !lstrcmpW(s, L"a description"), "get_Description");
        SysFreeString(s); s = NULL;
        hr = ILegacyIAccessibleProvider_get_KeyboardShortcut(legacy, &s);
        check(hr == S_OK && s && !lstrcmpW(s, L"Alt+S"), "get_KeyboardShortcut");
        SysFreeString(s); s = NULL;
        hr = ILegacyIAccessibleProvider_get_DefaultAction(legacy, &s);
        check(hr == S_OK && s && !lstrcmpW(s, L"Press"), "get_DefaultAction");
        SysFreeString(s); s = NULL;
        hr = ILegacyIAccessibleProvider_get_Help(legacy, &s);
        check(hr == S_OK && s && !lstrcmpW(s, L"some help"), "get_Help");
        SysFreeString(s); s = NULL;
        hr = ILegacyIAccessibleProvider_get_State(legacy, &state);
        check(hr == S_OK && state == (STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_SELECTABLE), "get_State");
        hr = ILegacyIAccessibleProvider_SetValue(legacy, L"second value");
        check(hr == S_OK && !lstrcmpW(acc_value, L"second value"), "SetValue: put_accValue");
        hr = ILegacyIAccessibleProvider_get_Value(legacy, &s);
        check(hr == S_OK && s && !lstrcmpW(s, L"second value"), "get_Value");
        SysFreeString(s); s = NULL;
        hr = ILegacyIAccessibleProvider_DoDefaultAction(legacy);
        check(hr == S_OK && acc_default_actions == 1, "DoDefaultAction: accDoDefaultAction");
        hr = ILegacyIAccessibleProvider_Select(legacy, SELFLAG_TAKESELECTION);
        check(hr == S_OK && (acc_select_flags & SELFLAG_TAKESELECTION), "Select: accSelect");
        hr = ILegacyIAccessibleProvider_GetSelection(legacy, &sel);
        check(hr == S_OK && sel && sel->rgsabound[0].cElements == 1, "GetSelection: the selected child");
        if (sel)
        {
            IUnknown *unk = NULL;
            LONG i = 0;
            ILegacyIAccessibleProvider *l2;
            SafeArrayGetElement(sel, &i, &unk);
            if (unk && SUCCEEDED(IUnknown_QueryInterface(unk, &IID_ILegacyIAccessibleProvider, (void **)&l2)))
            {
                ILegacyIAccessibleProvider_get_Name(l2, &s);
                check(s && !lstrcmpW(s, L"second child"), "and it is child 2");
                SysFreeString(s); s = NULL;
                ILegacyIAccessibleProvider_Release(l2);
            }
            else check(0, "and it is child 2");
            if (unk) IUnknown_Release(unk);
            SafeArrayDestroy(sel);
        }
        ILegacyIAccessibleProvider_Release(legacy);
    }

    hr = IRawElementProviderSimple_QueryInterface(elprov, &IID_IRawElementProviderFragment, (void **)&frag);
    if (hr == S_OK)
    {
        acc_select_flags = 0;
        hr = IRawElementProviderFragment_SetFocus(frag);
        check(hr == S_OK && (acc_select_flags & SELFLAG_TAKEFOCUS), "SetFocus: accSelect(SELFLAG_TAKEFOCUS)");
        IRawElementProviderFragment_Release(frag);
    }
    hr = IRawElementProviderSimple_QueryInterface(elprov, &IID_IRawElementProviderFragmentRoot, (void **)&root);
    if (hr == S_OK)
    {
        frag = NULL;
        hr = IRawElementProviderFragmentRoot_ElementProviderFromPoint(root, 80, 10, &frag);
        check(hr == S_OK && frag && acc_hits == 1, "ElementProviderFromPoint: the child hit");
        if (frag) IRawElementProviderFragment_Release(frag);
        frag = (void *)1;
        hr = IRawElementProviderFragmentRoot_ElementProviderFromPoint(root, 10, 10, &frag);
        check(hr == S_OK && !frag, "on the element itself: none below it");
        IRawElementProviderFragmentRoot_Release(root);
    }

    hr = IRawElementProviderSimple_QueryInterface(elprov, &IID_IProxyProviderWinEventHandler, (void **)&weh);
    if (hr == S_OK)
    {
        hr = IProxyProviderWinEventHandler_RespondToWinEvent(weh, EVENT_OBJECT_VALUECHANGE, acc_hwnd, OBJID_CLIENT,
                CHILDID_SELF, &sink);
        check(hr == S_OK && sink_prop_calls == 1 && sink_prop == UIA_ValueValuePropertyId &&
              !lstrcmpW(sink_value, L"second value"), "EVENT_OBJECT_VALUECHANGE: a Value property change");
        hr = IProxyProviderWinEventHandler_RespondToWinEvent(weh, EVENT_OBJECT_NAMECHANGE, acc_hwnd, OBJID_CLIENT,
                CHILDID_SELF, &sink);
        check(hr == S_OK && sink_prop_calls == 2 && sink_prop == UIA_NamePropertyId &&
              !lstrcmpW(sink_value, L"SG accessible"), "EVENT_OBJECT_NAMECHANGE: a Name property change");
        hr = IProxyProviderWinEventHandler_RespondToWinEvent(weh, EVENT_OBJECT_REORDER, acc_hwnd, OBJID_CLIENT,
                CHILDID_SELF, &sink);
        check(hr == S_OK && sink_struct_calls == 1 && sink_struct_type == StructureChangeType_ChildrenInvalidated,
              "EVENT_OBJECT_REORDER: children invalidated");
        hr = IProxyProviderWinEventHandler_RespondToWinEvent(weh, EVENT_OBJECT_INVOKED, acc_hwnd, OBJID_CLIENT,
                CHILDID_SELF, &sink);
        check(hr == S_OK && sink_event_calls == 1 && sink_event == UIA_Invoke_InvokedEventId,
              "EVENT_OBJECT_INVOKED: the Invoked event");
        IProxyProviderWinEventHandler_Release(weh);
    }

    hr = UiaProviderFromIAccessible(&acc, CHILDID_SELF, UIA_PFIA_UNWRAP_BRIDGE, &child);
    check(hr == S_OK && child, "UIA_PFIA_UNWRAP_BRIDGE: not a bridge, a provider still");
    if (child) IRawElementProviderSimple_Release(child);
    IRawElementProviderSimple_Release(elprov);
    DestroyWindow(acc_hwnd);
}

static int wait_for(struct handler *h, const char *what)
{
    int ok = WaitForSingleObject(h->done, 10000) == WAIT_OBJECT_0;
    if (!ok) check(0, what);
    return ok;
}

int main(int argc, char **argv)
{
    HMODULE mod = LoadLibraryW(L"uiautomationcore.dll");
    IUIAutomation6 *uia6 = NULL;
    IUIAutomation *uia;
    IUIAutomationElement *elem = NULL;
    PROCESS_INFORMATION pi;
    STARTUPINFOA si = { sizeof(si) };
    char cmd[MAX_PATH + 16];
    HANDLE go, quit;
    HWND hwnd = NULL;
    PROPERTYID name_id = UIA_NamePropertyId;
    HRESULT hr;
    int i;

    pUiaRaiseNotificationEvent = (void *)GetProcAddress(mod, "UiaRaiseNotificationEvent");
    pUiaRaiseTextEditTextChangedEvent = (void *)GetProcAddress(mod, "UiaRaiseTextEditTextChangedEvent");
    pUiaRaiseChangesEvent = (void *)GetProcAddress(mod, "UiaRaiseChangesEvent");
    if (argc > 1 && !strcmp(argv[1], "server")) return server();

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    check(pUiaRaiseNotificationEvent(NULL, NotificationKind_Other, NotificationProcessing_All, NULL, NULL) == E_INVALIDARG,
          "UiaRaiseNotificationEvent(NULL): E_INVALIDARG");
    check(UiaRaiseStructureChangedEvent(NULL, StructureChangeType_ChildAdded, NULL, 0) == E_INVALIDARG,
          "UiaRaiseStructureChangedEvent(NULL): E_INVALIDARG");

    go = CreateEventW(NULL, TRUE, FALSE, L"SgUiaEvGo");
    quit = CreateEventW(NULL, TRUE, FALSE, L"SgUiaEvQuit");
    GetModuleFileNameA(NULL, cmd, MAX_PATH);
    lstrcatA(cmd, " server");
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        check(0, "the server process starts");
        goto bridge;
    }
    for (i = 0; i < 100 && !(hwnd = FindWindowW(L"SgUiaEvServer", NULL)); i++) Sleep(100);
    check(hwnd != NULL, "the server's window");

    hr = CoCreateInstance(&CLSID_CUIAutomation8, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAutomation6, (void **)&uia6);
    check(hr == S_OK, "CUIAutomation8");
    if (FAILED(hr) || !hwnd) goto bridge;
    uia = (IUIAutomation *)uia6;
    hr = IUIAutomation_ElementFromHandle(uia, hwnd, &elem);
    check(hr == S_OK && elem, "ElementFromHandle");
    if (!elem) goto bridge;

    init_handler(&prop_h, prop_vtbl);
    init_handler(&struct_h, struct_vtbl);
    init_handler(&notif_h, notif_vtbl);
    init_handler(&text_h, text_vtbl);
    init_handler(&changes_h, changes_vtbl);

    check(IUIAutomation_AddPropertyChangedEventHandlerNativeArray(uia, elem, TreeScope_Element, NULL,
          (IUIAutomationPropertyChangedEventHandler *)&prop_h.iface, &name_id, 0) == E_INVALIDARG,
          "a property handler for no properties: E_INVALIDARG");
    hr = IUIAutomation_AddPropertyChangedEventHandlerNativeArray(uia, elem, TreeScope_Element, NULL,
            (IUIAutomationPropertyChangedEventHandler *)&prop_h.iface, &name_id, 1);
    check(hr == S_OK, "AddPropertyChangedEventHandlerNativeArray");
    hr = IUIAutomation_AddStructureChangedEventHandler(uia, elem, TreeScope_Element, NULL,
            (IUIAutomationStructureChangedEventHandler *)&struct_h.iface);
    check(hr == S_OK, "AddStructureChangedEventHandler");
    hr = IUIAutomation5_AddNotificationEventHandler((IUIAutomation5 *)uia6, elem, TreeScope_Element, NULL,
            (IUIAutomationNotificationEventHandler *)&notif_h.iface);
    check(hr == S_OK, "AddNotificationEventHandler");
    hr = IUIAutomation3_AddTextEditTextChangedEventHandler((IUIAutomation3 *)uia6, elem, TreeScope_Element,
            TextEditChangeType_AutoCorrect, NULL, (IUIAutomationTextEditTextChangedEventHandler *)&text_h.iface);
    check(hr == S_OK, "AddTextEditTextChangedEventHandler");
    hr = IUIAutomation4_AddChangesEventHandler((IUIAutomation4 *)uia6, elem, TreeScope_Element, NULL, 0, NULL,
            (IUIAutomationChangesEventHandler *)&changes_h.iface);
    check(hr == S_OK, "AddChangesEventHandler");

    SetEvent(go);
    if (wait_for(&prop_h, "the property changed handler is called"))
    {
        Sleep(300);
        check(prop_h.calls == 1 && prop_h.prop == UIA_NamePropertyId && !lstrcmpW(prop_h.str, L"new name"),
              "only the Name change, with its new value");
    }
    if (wait_for(&struct_h, "the structure changed handler is called"))
        check(struct_h.kind == StructureChangeType_ChildAdded && struct_h.count == 2 && struct_h.rt[0] == 42 &&
              struct_h.rt[1] == 7, "ChildAdded, with the runtime id");
    if (wait_for(&notif_h, "the notification handler is called"))
        check(notif_h.kind == NotificationKind_ActionCompleted && notif_h.processing == NotificationProcessing_MostRecent &&
              !lstrcmpW(notif_h.str, L"Saved") && !lstrcmpW(notif_h.str2, L"sg.save"),
              "the notification's kind, processing, text and activity");
    if (wait_for(&text_h, "the text edit handler is called"))
    {
        Sleep(300);
        check(text_h.calls == 1 && text_h.kind == TextEditChangeType_AutoCorrect && !lstrcmpW(text_h.str, L"fixed"),
              "only the autocorrect change, with its text");
    }
    if (wait_for(&changes_h, "the changes handler is called"))
        check(changes_h.count == 1 && changes_h.kind == UIA_SummaryChangeId && changes_h.payload == 1234 &&
              !lstrcmpW(changes_h.str, L"extra"), "the change, its payload and extra information");

    hr = IUIAutomation_RemovePropertyChangedEventHandler(uia, elem, (IUIAutomationPropertyChangedEventHandler *)&prop_h.iface);
    check(hr == S_OK, "RemovePropertyChangedEventHandler");
    prop_h.calls = 0;
    SetEvent(go);
    WaitForSingleObject(struct_h.done, 10000);
    Sleep(500);
    check(prop_h.calls == 0, "removed: no more property changes");
    IUIAutomation_RemoveAllEventHandlers(uia);
    IUIAutomationElement_Release(elem);
    IUIAutomation_Release(uia);

bridge:
    SetEvent(quit);
    test_bridge();

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    return failures != 0;
}
