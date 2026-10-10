/* UI Automation event handler groups (patch 2675), run by test/uiagroup-gate.sh
 * on Xvfb. A child copy of this program owns a window whose provider raises
 * property, structure, notification, text edit and changes events. This
 * process puts one handler of each kind into an IUIAutomationEventHandlerGroup,
 * registers the group on the element with AddEventHandlerGroup, checks every
 * handler is called with the arguments of its event, then takes the group off
 * with RemoveEventHandlerGroup and checks no handler is called any more. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
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
        V_BSTR(ret) = SysAllocString(L"SG group element");
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
    HANDLE go = CreateEventW(NULL, TRUE, FALSE, L"SgUiaGrpGo");
    HANDLE quit = CreateEventW(NULL, TRUE, FALSE, L"SgUiaGrpQuit");
    MSG msg;
    DWORD r;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    wc.lpfnWndProc = server_proc;
    wc.lpszClassName = L"SgUiaGrpServer";
    RegisterClassW(&wc);
    server_hwnd = CreateWindowW(L"SgUiaGrpServer", L"SG group", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
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
    IUIAutomationEventHandlerGroup *group = NULL, *group2 = NULL, *empty = NULL;
    PROCESS_INFORMATION pi;
    STARTUPINFOA si = { sizeof(si) };
    char cmd[MAX_PATH + 16];
    HANDLE go, quit;
    HWND hwnd = NULL;
    PROPERTYID props[2] = { UIA_NamePropertyId, UIA_HelpTextPropertyId };
    HRESULT hr;
    int i;

    pUiaRaiseNotificationEvent = (void *)GetProcAddress(mod, "UiaRaiseNotificationEvent");
    pUiaRaiseTextEditTextChangedEvent = (void *)GetProcAddress(mod, "UiaRaiseTextEditTextChangedEvent");
    pUiaRaiseChangesEvent = (void *)GetProcAddress(mod, "UiaRaiseChangesEvent");
    if (argc > 1 && !strcmp(argv[1], "server")) return server();

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    go = CreateEventW(NULL, TRUE, FALSE, L"SgUiaGrpGo");
    quit = CreateEventW(NULL, TRUE, FALSE, L"SgUiaGrpQuit");
    GetModuleFileNameA(NULL, cmd, MAX_PATH);
    lstrcatA(cmd, " server");
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        check(0, "the server process starts");
        goto done;
    }
    for (i = 0; i < 100 && !(hwnd = FindWindowW(L"SgUiaGrpServer", NULL)); i++) Sleep(100);
    check(hwnd != NULL, "the server's window");

    hr = CoCreateInstance(&CLSID_CUIAutomation8, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAutomation6, (void **)&uia6);
    check(hr == S_OK, "CUIAutomation8");
    if (FAILED(hr) || !hwnd) goto done;
    uia = (IUIAutomation *)uia6;
    hr = IUIAutomation_ElementFromHandle(uia, hwnd, &elem);
    check(hr == S_OK && elem, "ElementFromHandle");
    if (!elem) goto done;

    init_handler(&prop_h, prop_vtbl);
    init_handler(&struct_h, struct_vtbl);
    init_handler(&notif_h, notif_vtbl);
    init_handler(&text_h, text_vtbl);
    init_handler(&changes_h, changes_vtbl);

    hr = IUIAutomation6_CreateEventHandlerGroup(uia6, NULL);
    check(hr == E_POINTER, "CreateEventHandlerGroup(NULL): E_POINTER");
    hr = IUIAutomation6_CreateEventHandlerGroup(uia6, &group);
    check(hr == S_OK && group, "CreateEventHandlerGroup");
    hr = IUIAutomation6_CreateEventHandlerGroup(uia6, &empty);
    check(hr == S_OK && empty && empty != group, "a second group is a separate object");
    if (!group || !empty) goto done;

    hr = IUIAutomationEventHandlerGroup_AddNotificationEventHandler(group, TreeScope_Element, NULL, NULL);
    check(hr == E_POINTER, "a NULL handler: E_POINTER");
    hr = IUIAutomationEventHandlerGroup_AddNotificationEventHandler(group, 0, NULL,
            (IUIAutomationNotificationEventHandler *)&notif_h.iface);
    check(hr == E_INVALIDARG, "no scope: E_INVALIDARG");
    hr = IUIAutomationEventHandlerGroup_AddPropertyChangedEventHandler(group, TreeScope_Element, NULL,
            (IUIAutomationPropertyChangedEventHandler *)&prop_h.iface, props, 0);
    check(hr == E_INVALIDARG, "a property handler for no properties: E_INVALIDARG");

    hr = IUIAutomationEventHandlerGroup_AddPropertyChangedEventHandler(group, TreeScope_Element, NULL,
            (IUIAutomationPropertyChangedEventHandler *)&prop_h.iface, props, 1);
    check(hr == S_OK, "group: AddPropertyChangedEventHandler (Name only)");
    hr = IUIAutomationEventHandlerGroup_AddStructureChangedEventHandler(group, TreeScope_Element, NULL,
            (IUIAutomationStructureChangedEventHandler *)&struct_h.iface);
    check(hr == S_OK, "group: AddStructureChangedEventHandler");
    hr = IUIAutomationEventHandlerGroup_AddNotificationEventHandler(group, TreeScope_Element, NULL,
            (IUIAutomationNotificationEventHandler *)&notif_h.iface);
    check(hr == S_OK, "group: AddNotificationEventHandler");
    hr = IUIAutomationEventHandlerGroup_AddTextEditTextChangedEventHandler(group, TreeScope_Element,
            TextEditChangeType_AutoCorrect, NULL, (IUIAutomationTextEditTextChangedEventHandler *)&text_h.iface);
    check(hr == S_OK, "group: AddTextEditTextChangedEventHandler (autocorrect only)");
    hr = IUIAutomationEventHandlerGroup_AddChangesEventHandler(group, TreeScope_Element, NULL, 0, NULL,
            (IUIAutomationChangesEventHandler *)&changes_h.iface);
    check(hr == E_POINTER || hr == E_INVALIDARG, "group: a changes handler needs change types");
    {
        int types[1] = { UIA_SummaryChangeId };
        hr = IUIAutomationEventHandlerGroup_AddChangesEventHandler(group, TreeScope_Element, types, 1, NULL,
                (IUIAutomationChangesEventHandler *)&changes_h.iface);
        check(hr == S_OK, "group: AddChangesEventHandler");
    }

    hr = IUIAutomation6_AddEventHandlerGroup(uia6, NULL, group);
    check(hr == E_POINTER, "AddEventHandlerGroup(NULL element): E_POINTER");
    hr = IUIAutomation6_AddEventHandlerGroup(uia6, elem, NULL);
    check(hr == E_POINTER, "AddEventHandlerGroup(NULL group): E_POINTER");
    hr = IUIAutomation6_AddEventHandlerGroup(uia6, elem, empty);
    check(hr == S_OK, "an empty group adds nothing and succeeds");
    hr = IUIAutomation6_RemoveEventHandlerGroup(uia6, elem, empty);
    check(hr == S_OK, "and removes nothing");

    /* nothing is registered until the group goes onto an element */
    SetEvent(go);
    Sleep(1500);
    check(!prop_h.calls && !struct_h.calls && !notif_h.calls && !text_h.calls && !changes_h.calls,
          "before AddEventHandlerGroup no handler is called");

    hr = IUIAutomation6_AddEventHandlerGroup(uia6, elem, group);
    check(hr == S_OK, "AddEventHandlerGroup");
    SetEvent(go);
    if (wait_for(&prop_h, "the group's property handler is called"))
    {
        Sleep(300);
        check(prop_h.calls == 1 && prop_h.prop == UIA_NamePropertyId && !lstrcmpW(prop_h.str, L"new name"),
              "only the Name change, with its new value");
    }
    if (wait_for(&struct_h, "the group's structure handler is called"))
        check(struct_h.kind == StructureChangeType_ChildAdded && struct_h.count == 2 && struct_h.rt[0] == 42 &&
              struct_h.rt[1] == 7, "ChildAdded, with the runtime id");
    if (wait_for(&notif_h, "the group's notification handler is called"))
        check(notif_h.kind == NotificationKind_ActionCompleted && !lstrcmpW(notif_h.str, L"Saved") &&
              !lstrcmpW(notif_h.str2, L"sg.save"), "the notification's kind, text and activity");
    if (wait_for(&text_h, "the group's text edit handler is called"))
    {
        Sleep(300);
        check(text_h.calls == 1 && text_h.kind == TextEditChangeType_AutoCorrect && !lstrcmpW(text_h.str, L"fixed"),
              "only the autocorrect change, with its text");
    }
    if (wait_for(&changes_h, "the group's changes handler is called"))
        check(changes_h.count == 1 && changes_h.kind == UIA_SummaryChangeId && changes_h.payload == 1234,
              "the change and its payload");

    hr = IUIAutomation6_RemoveEventHandlerGroup(uia6, NULL, group);
    check(hr == E_POINTER, "RemoveEventHandlerGroup(NULL element): E_POINTER");
    hr = IUIAutomation6_RemoveEventHandlerGroup(uia6, elem, group);
    check(hr == S_OK, "RemoveEventHandlerGroup");
    prop_h.calls = struct_h.calls = notif_h.calls = text_h.calls = changes_h.calls = 0;
    SetEvent(go);
    Sleep(2000);
    check(!prop_h.calls && !struct_h.calls && !notif_h.calls && !text_h.calls && !changes_h.calls,
          "removed: no handler of the group is called any more");

    /* a group can be added again, and is a snapshot of nothing but its own list */
    hr = IUIAutomation6_AddEventHandlerGroup(uia6, elem, group);
    check(hr == S_OK, "the group can be added again");
    SetEvent(go);
    check(WaitForSingleObject(notif_h.done, 10000) == WAIT_OBJECT_0, "and works again");
    IUIAutomation6_RemoveEventHandlerGroup(uia6, elem, group);

    /* a group made by something else is refused */
    group2 = (IUIAutomationEventHandlerGroup *)&notif_h.iface;
    hr = IUIAutomation6_AddEventHandlerGroup(uia6, elem, group2);
    check(hr == E_INVALIDARG, "AddEventHandlerGroup with a foreign object: E_INVALIDARG");

    IUIAutomationEventHandlerGroup_Release(empty);
    IUIAutomationEventHandlerGroup_Release(group);
    IUIAutomation_RemoveAllEventHandlers(uia);
    IUIAutomationElement_Release(elem);
    IUIAutomation_Release(uia);
    SetEvent(quit);

done:
    SetEvent(quit);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    return failures != 0;
}
