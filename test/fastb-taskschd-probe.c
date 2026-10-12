/* taskschd, fast mode (patch 3504): monthly, monthly-by-weekday, event and
 * session-state-change triggers; COM handler, e-mail and message actions;
 * their task XML, written and read back. Late-bound through IDispatch, so it
 * builds with any header set. */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <oleauto.h>
#include <stdio.h>
#include <wchar.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

DEFINE_GUID(CLSID_TaskScheduler_, 0x0f87369f, 0xa4e5, 0x4cfc, 0xbd, 0x3e, 0x73, 0xe6, 0x15, 0x45, 0x72, 0xdd);

static const GUID nullguid;

static HRESULT invoke(IDispatch *obj, const WCHAR *name, WORD flags, VARIANT *args, int nargs, VARIANT *result)
{
    DISPID id, named = DISPID_PROPERTYPUT;
    DISPPARAMS params = { args, NULL, nargs, 0 };
    LPOLESTR n = (LPOLESTR)name;
    HRESULT hr;

    if (FAILED(hr = IDispatch_GetIDsOfNames(obj, &nullguid, &n, 1, 0, &id))) return hr;
    if (flags & (DISPATCH_PROPERTYPUT)) { params.rgdispidNamedArgs = &named; params.cNamedArgs = 1; }
    if (result) VariantInit(result);
    return IDispatch_Invoke(obj, id, &nullguid, 0, flags, &params, result, NULL, NULL);
}

static HRESULT getp(IDispatch *obj, const WCHAR *name, VARIANT *out)
{
    return invoke(obj, name, DISPATCH_PROPERTYGET, NULL, 0, out);
}

static HRESULT put_i(IDispatch *obj, const WCHAR *name, long v)
{
    VARIANT a;
    V_VT(&a) = VT_I4; V_I4(&a) = v;
    return invoke(obj, name, DISPATCH_PROPERTYPUT, &a, 1, NULL);
}

static HRESULT put_b(IDispatch *obj, const WCHAR *name, int v)
{
    VARIANT a;
    V_VT(&a) = VT_BOOL; V_BOOL(&a) = v ? VARIANT_TRUE : VARIANT_FALSE;
    return invoke(obj, name, DISPATCH_PROPERTYPUT, &a, 1, NULL);
}

static HRESULT put_s(IDispatch *obj, const WCHAR *name, const WCHAR *v)
{
    VARIANT a;
    HRESULT hr;
    V_VT(&a) = VT_BSTR; V_BSTR(&a) = SysAllocString(v);
    hr = invoke(obj, name, DISPATCH_PROPERTYPUT, &a, 1, NULL);
    SysFreeString(V_BSTR(&a));
    return hr;
}

static long get_i(IDispatch *obj, const WCHAR *name)
{
    VARIANT v;
    long r = -12345;
    if (SUCCEEDED(getp(obj, name, &v))) { VariantChangeType(&v, &v, 0, VT_I4); r = V_I4(&v); }
    return r;
}

static int get_b(IDispatch *obj, const WCHAR *name)
{
    VARIANT v;
    if (FAILED(getp(obj, name, &v))) return -1;
    return V_BOOL(&v) != 0;
}

/* a new object from Collection.Create(type) */
static IDispatch *create(IDispatch *collection, long type)
{
    VARIANT a, r;
    V_VT(&a) = VT_I4; V_I4(&a) = type;
    if (FAILED(invoke(collection, L"Create", DISPATCH_METHOD, &a, 1, &r))) return NULL;
    return V_DISPATCH(&r);
}

static IDispatch *get_obj(IDispatch *obj, const WCHAR *name)
{
    VARIANT v;
    if (FAILED(getp(obj, name, &v)) || V_VT(&v) != VT_DISPATCH) return NULL;
    return V_DISPATCH(&v);
}

static IDispatch *item(IDispatch *collection, long index)
{
    VARIANT a, r;
    V_VT(&a) = VT_I4; V_I4(&a) = index;
    if (FAILED(invoke(collection, L"Item", DISPATCH_PROPERTYGET | DISPATCH_METHOD, &a, 1, &r))) return NULL;
    return V_DISPATCH(&r);
}

int main(void)
{
    IDispatch *service, *def, *def2, *triggers, *actions, *triggers2, *actions2, *t, *queries, *a;
    VARIANT v, args[2], r;
    BSTR xml;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    {
        HRESULT (WINAPI *get_version)(void *) = (void *)GetProcAddress(LoadLibraryA("taskschd.dll"), "DllGetVersion");
        DWORD info[5] = { 20, 0, 0, 0, 0 };

        check(get_version != NULL, "DllGetVersion is exported");
        if (get_version)
        {
            check(get_version(info) == S_OK && info[1] == 10 && info[3] != 0, "DllGetVersion fills the structure");
            info[0] = 7;
            check(get_version(info) == E_INVALIDARG, "DllGetVersion size check");
        }
    }
    hr = CoCreateInstance(&CLSID_TaskScheduler_, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&service);
    if (FAILED(hr)) { printf("FAIL  no task service %lx\n", hr); printf("RESULT: FAIL\n"); return 1; }
    V_VT(&args[0]) = VT_I4; V_I4(&args[0]) = 0;
    hr = invoke(service, L"NewTask", DISPATCH_METHOD, args, 1, &r);
    check(hr == S_OK && V_VT(&r) == VT_DISPATCH, "NewTask");
    def = V_DISPATCH(&r);
    triggers = get_obj(def, L"Triggers");
    actions = get_obj(def, L"Actions");

    t = create(triggers, 4 /* monthly */);
    check(t != NULL, "monthly trigger");
    put_i(t, L"DaysOfMonth", 5);
    put_i(t, L"MonthsOfYear", 1 | 4 | 2048);
    put_b(t, L"RunOnLastDayOfMonth", 1);
    check(get_i(t, L"DaysOfMonth") == 5 && get_i(t, L"MonthsOfYear") == (1 | 4 | 2048) &&
          get_b(t, L"RunOnLastDayOfMonth") == 1, "monthly properties");
    IDispatch_Release(t);

    t = create(triggers, 5 /* monthly day of week */);
    check(t != NULL, "monthly day-of-week trigger");
    put_i(t, L"DaysOfWeek", 2 | 32);
    put_i(t, L"WeeksOfMonth", 1 | 8);
    put_i(t, L"MonthsOfYear", 64);
    put_b(t, L"RunOnLastWeekOfMonth", 1);
    check(get_i(t, L"DaysOfWeek") == (2 | 32) && get_i(t, L"WeeksOfMonth") == (1 | 8), "monthly day-of-week properties");
    IDispatch_Release(t);

    t = create(triggers, 0 /* event */);
    check(t != NULL, "event trigger");
    put_s(t, L"Subscription", L"<QueryList><Query Id=\"0\"><Select Path=\"System\">*</Select></Query></QueryList>");
    put_s(t, L"Delay", L"PT30S");
    queries = get_obj(t, L"ValueQueries");
    check(queries != NULL, "value queries");
    if (queries)
    {
        V_VT(&args[1]) = VT_BSTR; V_BSTR(&args[1]) = SysAllocString(L"who");
        V_VT(&args[0]) = VT_BSTR; V_BSTR(&args[0]) = SysAllocString(L"Event/System/Provider/@Name");
        hr = invoke(queries, L"Create", DISPATCH_METHOD, args, 2, &r);
        check(hr == S_OK, "add a value query");
        check(get_i(queries, L"Count") == 1, "one value query");
        IDispatch_Release(queries);
    }
    IDispatch_Release(t);

    t = create(triggers, 11 /* session state change */);
    check(t != NULL, "session state change trigger");
    hr = put_i(t, L"StateChange", 7);
    check(hr == S_OK, "put StateChange");
    hr = put_i(t, L"StateChange", 99);
    check(FAILED(hr), "StateChange value check");
    IDispatch_Release(t);

    a = create(actions, 5 /* COM handler */);
    check(a != NULL, "COM handler action");
    put_s(a, L"ClassId", L"{00000000-1111-2222-3333-444444444444}");
    put_s(a, L"Data", L"some data");
    IDispatch_Release(a);
    a = create(actions, 7 /* message */);
    check(a != NULL, "show message action");
    put_s(a, L"Title", L"Hello");
    put_s(a, L"MessageBody", L"It is time");
    IDispatch_Release(a);
    a = create(actions, 6 /* e-mail */);
    check(a != NULL, "e-mail action");
    put_s(a, L"Server", L"smtp.example.org");
    put_s(a, L"To", L"you@example.org");
    IDispatch_Release(a);

    check(get_i(actions, L"Count") == 3, "three actions");
    check(get_i(triggers, L"Count") == 4, "four triggers");

    hr = getp(def, L"XmlText", &v);
    check(hr == S_OK && V_VT(&v) == VT_BSTR, "XmlText");
    xml = hr == S_OK ? V_BSTR(&v) : NULL;
    if (xml)
    {
        check(wcsstr(xml, L"<ScheduleByMonth>") != NULL, "xml: ScheduleByMonth");
        check(wcsstr(xml, L"<January") != NULL, "xml: January");
        check(wcsstr(xml, L"<Day>Last</Day>") != NULL, "xml: last day");
        check(wcsstr(xml, L"<ScheduleByMonthDayOfWeek>") != NULL, "xml: ScheduleByMonthDayOfWeek");
        check(wcsstr(xml, L"<EventTrigger>") != NULL && wcsstr(xml, L"<Subscription>") != NULL, "xml: EventTrigger");
        check(wcsstr(xml, L"name=\"who\"") != NULL, "xml: value query");
        check(wcsstr(xml, L"<StateChange>SessionLock</StateChange>") != NULL, "xml: StateChange");
        check(wcsstr(xml, L"<ComHandler>") != NULL && wcsstr(xml, L"<ClassId>") != NULL, "xml: ComHandler");
        check(wcsstr(xml, L"<ShowMessage>") != NULL && wcsstr(xml, L"<Title>Hello</Title>") != NULL, "xml: ShowMessage");
        check(wcsstr(xml, L"<SendEmail>") != NULL && wcsstr(xml, L"<Server>smtp.example.org</Server>") != NULL, "xml: SendEmail");

        V_VT(&args[0]) = VT_I4; V_I4(&args[0]) = 0;
        invoke(service, L"NewTask", DISPATCH_METHOD, args, 1, &r);
        def2 = V_DISPATCH(&r);
        hr = put_s(def2, L"XmlText", xml);
        check(hr == S_OK, "put_XmlText of what was written");
        triggers2 = get_obj(def2, L"Triggers");
        actions2 = get_obj(def2, L"Actions");
        check(get_i(triggers2, L"Count") == 4, "four triggers read back");
        t = item(triggers2, 1);
        check(t && get_i(t, L"Type") == 4, "first trigger is monthly");
        if (t)
        {
            check(get_i(t, L"DaysOfMonth") == 5 && get_i(t, L"MonthsOfYear") == (1 | 4 | 2048) &&
                  get_b(t, L"RunOnLastDayOfMonth") == 1, "monthly read back");
            IDispatch_Release(t);
        }
        t = item(triggers2, 2);
        if (t)
        {
            check(get_i(t, L"DaysOfWeek") == (2 | 32) && get_i(t, L"WeeksOfMonth") == (1 | 8) &&
                  get_b(t, L"RunOnLastWeekOfMonth") == 1, "monthly day-of-week read back");
            IDispatch_Release(t);
        }
        t = item(triggers2, 3);
        if (t)
        {
            queries = get_obj(t, L"ValueQueries");
            check(queries && get_i(queries, L"Count") == 1, "event value query read back");
            if (queries) IDispatch_Release(queries);
            IDispatch_Release(t);
        }
        t = item(triggers2, 4);
        check(t && get_i(t, L"StateChange") == 7, "state change read back");
        if (t) IDispatch_Release(t);
        check(get_i(actions2, L"Count") == 3, "three actions read back");
        a = item(actions2, 3);
        if (a)
        {
            VARIANT to;
            check(SUCCEEDED(getp(a, L"To", &to)) && V_VT(&to) == VT_BSTR && !wcscmp(V_BSTR(&to), L"you@example.org"),
                  "e-mail To read back");
            IDispatch_Release(a);
        }
        IDispatch_Release(triggers2);
        IDispatch_Release(actions2);
        IDispatch_Release(def2);
        SysFreeString(xml);
    }
    IDispatch_Release(triggers);
    IDispatch_Release(actions);
    IDispatch_Release(def);
    IDispatch_Release(service);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
