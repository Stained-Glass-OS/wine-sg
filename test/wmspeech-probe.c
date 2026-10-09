/* windows.media.speech's IInspectable stubs, async identity, list constraint and
 * recognition session state (patches/sg/2810), run by test/wmspeech-gate.sh on
 * Xvfb. Every factory and object the DLL hands out is asked for GetIids (the
 * exact set, each answering QueryInterface), GetTrustLevel (BaseTrust),
 * GetRuntimeClassName and an unknown IID; then the members that were FIXME
 * stubs: IAsyncInfo::Id (counted per owner from 1), IAsync*::Completed (the
 * handler is kept after it ran), Cancel on a closed operation, the list
 * constraint's Tag/Type/Probability/IsEnabled, the continuous session's
 * AutoStopSilenceTimeout, StartWithModeAsync/CancelAsync and the recognizer's
 * event registrations. Interfaces are called through their vtable slots.
 *
 *   wmspeech-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <inspectable.h>
#include <activation.h>
#include <stdio.h>
#include <wchar.h>

DEFINE_GUID(IID_IAsyncInfo, 0x00000036, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_IVectorView_IMediaMarker, 0xb543562c, 0x02b1, 0x5824, 0x80,0xa8, 0x98,0x54,0x13,0x0c,0xda,0xdd);
DEFINE_GUID(IID_IIterable_IMediaMarker, 0xa1c0a397, 0x0364, 0x5e4c, 0x9d,0xca, 0x7c,0xd7,0x01,0x1b,0xd1,0x14);
DEFINE_GUID(IID_Bogus, 0x12345678, 0x1234, 0x1234, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34);
DEFINE_GUID(IID_ISpeechRecognizer, 0x0bc3c9cb, 0xc26a, 0x40f2, 0xae,0xb5, 0x80,0x96,0xb2,0xe4,0x80,0x73);
DEFINE_GUID(IID_ISpeechRecognizer2, 0x63c9baf1, 0x91e3, 0x4ea4, 0x86,0xa1, 0x7c,0x38,0x67,0xd0,0x84,0xa6);
DEFINE_GUID(IID_ISpeechRecognizerFactory, 0x60c488dd, 0x7fb8, 0x4033, 0xac,0x70, 0xd0,0x46,0xf6,0x48,0x18,0xe1);
DEFINE_GUID(IID_ISpeechRecognizerStatics, 0x87a35eac, 0xa7dc, 0x4b0b, 0xbc,0xc9, 0x24,0xf4,0x7c,0x0b,0x7e,0xbf);
DEFINE_GUID(IID_ISpeechRecognizerStatics2, 0x1d1b0d95, 0x7565, 0x4ef9, 0xa2,0xf3, 0xba,0x15,0x16,0x2a,0x96,0xcf);
DEFINE_GUID(IID_ISpeechContinuousRecognitionSession, 0x6a213c04, 0x6614, 0x49f8, 0x99,0xa2, 0xb5,0xe9,0xb3,0xa0,0x85,0xc8);
DEFINE_GUID(IID_ISpeechRecognitionCompilationResult, 0x407e6c5d, 0x6ac7, 0x4da4, 0x9c,0xc1, 0x2f,0xce,0x32,0xcf,0x74,0x89);
DEFINE_GUID(IID_ISpeechRecognitionListConstraint, 0x09c487e9, 0xe4ad, 0x4526, 0x81,0xf2, 0x49,0x46,0xfb,0x48,0x1d,0x98);
DEFINE_GUID(IID_ISpeechRecognitionListConstraintFactory, 0x40f3cdc7, 0x562a, 0x426a, 0x9f,0x3b, 0x3b,0x4e,0x28,0x2b,0xe1,0xd5);
DEFINE_GUID(IID_ISpeechRecognitionConstraint, 0x79ac1628, 0x4d68, 0x43c4, 0x89,0x11, 0x40,0xdc,0x41,0x01,0xb5,0x5b);
DEFINE_GUID(IID_ISpeechSynthesizer, 0xce9f7c76, 0x97f4, 0x4ced, 0xad,0x68, 0xd5,0x1c,0x45,0x8e,0x45,0xc6);
DEFINE_GUID(IID_ISpeechSynthesizer2, 0xa7c5ecb2, 0x4339, 0x4d6a, 0xbb,0xf8, 0xc7,0xa4,0xf1,0x54,0x4c,0x2e);
DEFINE_GUID(IID_IInstalledVoicesStatic, 0x7d526ecc, 0x7533, 0x4c3f, 0x85,0xbe, 0x88,0x8c,0x2b,0xae,0xeb,0xdc);
DEFINE_GUID(IID_ISpeechSynthesisStream, 0x83e46e93, 0x244c, 0x4622, 0xba,0x0b, 0x62,0x29,0xc4,0xd0,0xd6,0x5d);
DEFINE_GUID(IID_IAsyncOperation_SpeechSynthesisStream, 0xdf9d48ad, 0x9cea, 0x560c, 0x9e,0xdc, 0xcb,0x88,0x52,0xcb,0x55,0xe3);
DEFINE_GUID(IID_IAsyncOperation_SpeechRecognitionCompilationResult, 0xa392249a, 0xe28a, 0x564a, 0x9e,0x73, 0x1d,0xda,0x63,0xca,0x64,0x3c);
DEFINE_GUID(IID_IVectorView_VoiceInformation, 0xee8d63ce, 0x51ac, 0x5984, 0x89,0x1b, 0xd2,0x32,0xfa,0x7f,0x64,0x53);
DEFINE_GUID(IID_IVector_ISpeechRecognitionConstraint, 0x2691d763, 0x561e, 0x5060, 0xbb,0xc9, 0x7b,0x07,0x36,0x1a,0xcc,0x95);
DEFINE_GUID(IID_IVectorView_ISpeechRecognitionConstraint, 0x341dee1d, 0x6ac2, 0x5d06, 0x90,0x26, 0xb3,0x0a,0xda,0x20,0x56,0x65);
DEFINE_GUID(IID_IIterable_ISpeechRecognitionConstraint, 0x88e6436c, 0x3253, 0x520b, 0x9e,0xd8, 0xa6,0x3b,0x17,0x8c,0x44,0xa2);
DEFINE_GUID(IID_IIterator_ISpeechRecognitionConstraint, 0x738f00b1, 0xe18c, 0x5140, 0xa5,0x3a, 0xf1,0x78,0x8d,0x10,0xc9,0x3d);
DEFINE_GUID(IID_IIterable_HSTRING, 0xe2fcc7c1, 0x3bfc, 0x5a0b, 0xb2,0xb0, 0x72,0xe7,0x69,0xd1,0xcb,0x7e);
DEFINE_GUID(IID_IIterator_HSTRING, 0x8c304ebb, 0x6615, 0x50a4, 0x88,0x29, 0x87,0x9e,0xcd,0x44,0x32,0x36);
DEFINE_GUID(IID_IVector_HSTRING, 0x98b9acc1, 0x4b56, 0x532e, 0xac,0x73, 0x03,0xd5,0x29,0x1c,0xca,0x90);
DEFINE_GUID(IID_IVectorView_HSTRING, 0x2f13c006, 0xa03a, 0x5f69, 0xb0,0x90, 0x75,0xa4,0x3e,0x33,0x42,0x3e);
DEFINE_GUID(IID_IAsyncAction, 0x5a648006, 0x843a, 0x4da9, 0x86,0x5b, 0x9d,0x26,0xe5,0xdf,0xad,0x7b);
DEFINE_GUID(IID_IClosable, 0x30d5a829, 0x7fa4, 0x4026, 0x83,0xbb, 0xd7,0x5b,0xae,0x4e,0xa9,0x9e);
DEFINE_GUID(IID_ISpeechSynthesizerOptions, 0xa0e23871, 0xcc3d, 0x43c9, 0x91,0xb1, 0xee,0x18,0x53,0x24,0xd8,0x3d);
DEFINE_GUID(IID_IAsyncOperationCompletedHandler_SpeechSynthesisStream, 0xc972b996, 0x6165, 0x50d4, 0xaf,0x60, 0xa8,0xc3,0xdf,0x51,0xd0,0x92);
DEFINE_GUID(IID_IAsyncOperationCompletedHandler_SpeechRecognitionCompilationResult, 0x78c859bd, 0x14d4, 0x5c40, 0xab,0xff, 0x49,0x06,0x16,0xd5,0xe9,0x2d);
DEFINE_GUID(IID_IAsyncActionCompletedHandler, 0xa4ed5c81, 0x76c9, 0x40bd, 0x8b,0xe6, 0xb1,0xd9,0x0f,0xb2,0x0a,0xe7);

#define UREL(p) ((IUnknown *)(p))->lpVtbl->Release((IUnknown *)(p))
#undef IInspectable_QueryInterface
#define IInspectable_QueryInterface(p, ...) ((IInspectable *)(p))->lpVtbl->QueryInterface((IInspectable *)(p), __VA_ARGS__)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

typedef HRESULT (WINAPI *vtbl_fn)();
#define SLOT(obj, n) ((vtbl_fn)(*(void ***)(obj))[n])
#define VC(obj, n, ...) SLOT(obj, n)(obj, ##__VA_ARGS__)

#define MAXIIDS 6
struct inspectable_case
{
    const char *name;
    const WCHAR *class_name;
    const GUID *iids[MAXIIDS];
};

static int in_list(const GUID *g, const GUID *const *list)
{
    for (; *list; list++) if (IsEqualGUID(g, *list)) return 1;
    return 0;
}

static void check_inspectable(IInspectable *obj, const struct inspectable_case *c)
{
    ULONG count = 0, i, expected = 0;
    IID *iids = NULL;
    TrustLevel trust = 0x5555;
    HSTRING name = NULL;
    HRESULT hr;
    void *out;
    int all_known = 1, all_qi = 1;

    while (c->iids[expected]) expected++;

    hr = IInspectable_GetIids(obj, &count, &iids);
    CHECKF(hr == S_OK, "%s: GetIids returns S_OK (%#lx)", c->name, hr);
    CHECKF(hr == S_OK && count == expected, "%s: GetIids reports %lu interfaces (want %lu)", c->name, count, expected);
    if (hr == S_OK)
    {
        for (i = 0; i < count; i++)
        {
            IUnknown *unk = NULL;
            if (!in_list(&iids[i], c->iids))
            {
                all_known = 0;
                printf("note  %s: unexpected IID {%08lx-%04x-%04x-...}\n", c->name, iids[i].Data1, iids[i].Data2, iids[i].Data3);
            }
            if (FAILED(IInspectable_QueryInterface(obj, &iids[i], (void **)&unk)) || !unk) all_qi = 0;
            else UREL(unk);
        }
        CHECKF(all_known, "%s: every reported IID is an expected one", c->name);
        CHECKF(all_qi, "%s: every reported IID answers QueryInterface", c->name);
        CoTaskMemFree(iids);
    }

    hr = IInspectable_GetTrustLevel(obj, &trust);
    CHECKF(hr == S_OK && trust == BaseTrust, "%s: GetTrustLevel is BaseTrust (hr %#lx, trust %d)", c->name, hr, trust);

    hr = IInspectable_GetRuntimeClassName(obj, &name);
    CHECKF(hr == S_OK && name && !wcscmp(WindowsGetStringRawBuffer(name, NULL), c->class_name),
           "%s: GetRuntimeClassName is %ls (hr %#lx, got %ls)", c->name, c->class_name, hr,
           name ? WindowsGetStringRawBuffer(name, NULL) : L"(null)");
    WindowsDeleteString(name);

    out = (void *)0x1234;
    hr = IInspectable_QueryInterface(obj, &IID_Bogus, &out);
    CHECKF(hr == E_NOINTERFACE && !out, "%s: unknown IID gives E_NOINTERFACE and NULL (%#lx %p)", c->name, hr, out);
}

static IActivationFactory *get_factory(const WCHAR *class_name)
{
    IActivationFactory *factory = NULL;
    HSTRING str;
    HRESULT hr;

    WindowsCreateString(class_name, wcslen(class_name), &str);
    hr = RoGetActivationFactory(str, &IID_IActivationFactory, (void **)&factory);
    WindowsDeleteString(str);
    if (FAILED(hr)) printf("note  RoGetActivationFactory(%ls) = %#lx\n", class_name, hr);
    return factory;
}

/* a completion handler for any IAsync*CompletedHandler: Invoke is slot 3 */
struct handler { void *vtbl; LONG ref; LONG calls; };
/* the operations keep their handler until they are freed: handlers must outlive the test function */
static struct handler handler_pool[32];
static int handler_next;
static HRESULT WINAPI h_qi(void *iface, REFIID iid, void **out) { *out = iface; InterlockedIncrement(&((struct handler *)iface)->ref); return S_OK; }
static ULONG WINAPI h_addref(void *iface) { return InterlockedIncrement(&((struct handler *)iface)->ref); }
static ULONG WINAPI h_release(void *iface) { return InterlockedDecrement(&((struct handler *)iface)->ref); }
static HRESULT WINAPI h_invoke(void *iface, void *op, int status) { InterlockedIncrement(&((struct handler *)iface)->calls); return S_OK; }
static void *handler_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

/* waits for an async object to leave Started */
static int wait_async(IInspectable *op, int *status)
{
    void *info = NULL;
    int i, s = 0;
    if (FAILED(IInspectable_QueryInterface(op, &IID_IAsyncInfo, &info))) return 0;
    for (i = 0; i < 500; i++)
    {
        s = 0;
        VC(info, 7, &s);
        if (s != 0) break;
        Sleep(10);
    }
    UREL((IUnknown *)info);
    *status = s;
    return s == 1;
}

static UINT32 async_id(IInspectable *op, HRESULT *hr)
{
    void *info = NULL;
    UINT32 id = 0xdeadbeef;
    if (FAILED(IInspectable_QueryInterface(op, &IID_IAsyncInfo, &info))) { *hr = E_FAIL; return 0; }
    *hr = (HRESULT)VC(info, 6, &id);
    UREL((IUnknown *)info);
    return id;
}

/* Completed handler / Id / Cancel / Close contract of one async object; slot of put_Completed is 6 */
static void check_async(IInspectable *op, const char *name, UINT32 want_id)
{
    struct handler *hp = &handler_pool[handler_next++];
    hp->vtbl = handler_vtbl; hp->ref = 1; hp->calls = 0;
    void *got = NULL, *info = NULL;
    int status = 0, i;
    HRESULT hr;
    UINT32 id;
    LONG before;

    hr = (HRESULT)VC(op, 6, hp);
    CHECKF(hr == S_OK, "%s: put_Completed (%#lx)", name, hr);
    for (i = 0; i < 500 && !hp->calls; i++) Sleep(10);
    CHECKF(hp->calls == 1, "%s: the handler was invoked once (%ld)", name, hp->calls);
    wait_async(op, &status);
    CHECKF(status == 1, "%s: status is Completed (%d)", name, status);
    Sleep(50);
    CHECKF(hp->ref == 2, "%s: the operation holds the handler after it ran (ref %ld, want 2)", name, hp->ref);
    before = hp->ref;
    hr = (HRESULT)VC(op, 7, &got);
    CHECKF(hr == S_OK && got == hp, "%s: get_Completed returns the handler after it ran (%#lx %p)", name, hr, got);
    CHECKF(hp->ref == before + 1, "%s: get_Completed AddRefs the handler (%ld -> %ld)", name, before, hp->ref);
    if (got) h_release(got);
    hr = (HRESULT)VC(op, 6, hp);
    CHECKF(hr == 0x80000018, "%s: a second put_Completed is E_ILLEGAL_DELEGATE_ASSIGNMENT (%#lx)", name, hr);

    hr = S_OK;
    id = async_id(op, &hr);
    CHECKF(hr == S_OK && id == want_id, "%s: IAsyncInfo::Id is %u (hr %#lx, got %u)", name, want_id, hr, id);

    IInspectable_QueryInterface(op, &IID_IAsyncInfo, &info);
    hr = (HRESULT)VC(info, 9);
    CHECKF(hr == S_OK, "%s: Cancel of a completed operation is S_OK (%#lx)", name, hr);
    hr = (HRESULT)VC(info, 10);
    CHECKF(hr == S_OK, "%s: Close (%#lx)", name, hr);
    id = 0xdeadbeef;
    hr = (HRESULT)VC(info, 6, &id);
    CHECKF(hr == 0x8000000e && id == want_id, "%s: Id on a closed operation: E_ILLEGAL_METHOD_CALL with the id (%#lx, %u)", name, hr, id);
    hr = (HRESULT)VC(info, 9);
    CHECKF(hr == S_OK, "%s: Cancel of a closed operation is S_OK (%#lx)", name, hr);
    hr = (HRESULT)VC(info, 9);
    UREL((IUnknown *)info);
    hr = (HRESULT)VC(op, 7, &got);
    CHECKF(hr == 0x8000000e, "%s: get_Completed on a closed operation is E_ILLEGAL_METHOD_CALL (%#lx)", name, hr);
}

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, wcslen(s), &h); return h; }

static void test_factories(void)
{
    static const struct { struct inspectable_case c; HRESULT activate; } cases[] =
    {
        { { "SpeechRecognizer factory", L"Windows.Media.SpeechRecognition.SpeechRecognizer",
            { &IID_IActivationFactory, &IID_ISpeechRecognizerFactory, &IID_ISpeechRecognizerStatics, &IID_ISpeechRecognizerStatics2 } }, S_OK },
        { { "ListConstraint factory", L"Windows.Media.SpeechRecognition.SpeechRecognitionListConstraint",
            { &IID_IActivationFactory, &IID_ISpeechRecognitionListConstraintFactory } }, E_NOTIMPL },
        { { "SpeechSynthesizer factory", L"Windows.Media.SpeechSynthesis.SpeechSynthesizer",
            { &IID_IActivationFactory, &IID_IInstalledVoicesStatic } }, S_OK },
    };
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(cases); i++)
    {
        IActivationFactory *f = get_factory(cases[i].c.class_name);
        IInspectable *obj = NULL;
        HRESULT hr;
        if (!f) { check(0, cases[i].c.name); continue; }
        check_inspectable((IInspectable *)f, &cases[i].c);
        hr = IActivationFactory_ActivateInstance(f, &obj);
        if (cases[i].activate == S_OK && FAILED(hr)) printf("note  %s: ActivateInstance = %#lx (no audio device?)\n", cases[i].c.name, hr);
        else CHECKF(hr == cases[i].activate, "%s: ActivateInstance is %#lx (got %#lx)", cases[i].c.name, cases[i].activate, hr);
        if (obj) IInspectable_Release(obj);
        IActivationFactory_Release(f);
    }
}


/* hand-built IIterable<HSTRING> over a fixed array, to feed the constraint factory */
struct strings { void *vtbl; LONG ref; HSTRING *items; UINT32 count, index; };
static HRESULT WINAPI s_qi(void *iface, REFIID iid, void **out) { *out = iface; InterlockedIncrement(&((struct strings *)iface)->ref); return S_OK; }
static ULONG WINAPI s_addref(void *iface) { return InterlockedIncrement(&((struct strings *)iface)->ref); }
static ULONG WINAPI s_release(void *iface) { return InterlockedDecrement(&((struct strings *)iface)->ref); }
static HRESULT WINAPI s_notimpl() { return E_NOTIMPL; }
static HRESULT WINAPI s_current(void *iface, HSTRING *out)
{
    struct strings *s = iface;
    return WindowsDuplicateString(s->items[s->index], out);
}
static HRESULT WINAPI s_has_current(void *iface, BOOLEAN *out) { struct strings *s = iface; *out = s->index < s->count; return S_OK; }
static HRESULT WINAPI s_move_next(void *iface, BOOLEAN *out) { struct strings *s = iface; if (s->index < s->count) s->index++; *out = s->index < s->count; return S_OK; }
static void *iterator_vtbl[] = { s_qi, s_addref, s_release, s_notimpl, s_notimpl, s_notimpl, s_current, s_has_current, s_move_next, s_notimpl };
static struct strings iterator_obj = { iterator_vtbl, 1 };
static HRESULT WINAPI s_first(void *iface, void **out)
{
    struct strings *s = iface;
    iterator_obj.items = s->items; iterator_obj.count = s->count; iterator_obj.index = 0;
    *out = &iterator_obj;
    return S_OK;
}
static void *iterable_vtbl[] = { s_qi, s_addref, s_release, s_notimpl, s_notimpl, s_notimpl, s_first };

static void test_list_constraint(void)
{
    static const struct inspectable_case list_case = { "ListConstraint", L"Windows.Media.SpeechRecognition.SpeechRecognitionListConstraint",
        { &IID_ISpeechRecognitionListConstraint, &IID_ISpeechRecognitionConstraint } };
    static const struct inspectable_case con_case = { "Constraint interface", L"Windows.Media.SpeechRecognition.SpeechRecognitionListConstraint",
        { &IID_ISpeechRecognitionListConstraint, &IID_ISpeechRecognitionConstraint } };
    static const struct inspectable_case vec_case = { "Commands vector", L"Windows.Foundation.Collections.IVector`1<String>",
        { &IID_IVector_HSTRING, &IID_IIterable_HSTRING } };
    static const struct inspectable_case view_case = { "Commands view", L"Windows.Foundation.Collections.IVectorView`1<String>",
        { &IID_IVectorView_HSTRING, &IID_IIterable_HSTRING } };
    static const struct inspectable_case iter_case = { "Commands iterator", L"Windows.Foundation.Collections.IIterator`1<String>",
        { &IID_IIterator_HSTRING } };
    IActivationFactory *f = get_factory(L"Windows.Media.SpeechRecognition.SpeechRecognitionListConstraint");
    void *cf = NULL, *list = NULL, *con = NULL, *cmds = NULL, *view = NULL, *iterable = NULL, *iter = NULL, *plain = NULL;
    HSTRING items[2] = { hs(L"open"), hs(L"close") }, tag = hs(L"mytag"), str = NULL;
    struct strings src = { iterable_vtbl, 1, items, 2, 0 };
    BOOLEAN enabled = 2;
    int type = -1, prob = -1;
    UINT32 size = 0;
    HRESULT hr;

    if (!f) { check(0, "list constraint factory"); return; }
    hr = IActivationFactory_QueryInterface(f, &IID_ISpeechRecognitionListConstraintFactory, &cf);
    CHECKF(hr == S_OK, "list constraint factory interface (%#lx)", hr);
    if (hr != S_OK) return;

    hr = (HRESULT)VC(cf, 7, &src, tag, &list);
    CHECKF(hr == S_OK && list, "CreateWithTag (%#lx)", hr);
    if (hr != S_OK) return;
    check_inspectable(list, &list_case);
    IInspectable_QueryInterface(list, &IID_ISpeechRecognitionConstraint, &con);
    check_inspectable(con, &con_case);

    hr = (HRESULT)VC(con, 6, &enabled);
    CHECKF(hr == S_OK && enabled == TRUE, "IsEnabled starts TRUE (%#lx, %d)", hr, enabled);
    VC(con, 7, FALSE);
    enabled = 2;
    VC(con, 6, &enabled);
    CHECKF(enabled == FALSE, "IsEnabled keeps FALSE (%d)", enabled);

    hr = (HRESULT)VC(con, 8, &str);
    CHECKF(hr == S_OK && str && !wcscmp(WindowsGetStringRawBuffer(str, NULL), L"mytag"), "Tag is the one given to CreateWithTag (%#lx %ls)", hr,
           str ? WindowsGetStringRawBuffer(str, NULL) : L"(null)");
    WindowsDeleteString(str);
    str = hs(L"another");
    hr = (HRESULT)VC(con, 9, str);
    WindowsDeleteString(str);
    str = NULL;
    VC(con, 8, &str);
    CHECKF(hr == S_OK && str && !wcscmp(WindowsGetStringRawBuffer(str, NULL), L"another"), "put_Tag is read back (%#lx)", hr);
    WindowsDeleteString(str);

    hr = (HRESULT)VC(con, 10, &type);
    CHECKF(hr == S_OK && type == 1, "Type is List (%#lx, %d)", hr, type);
    hr = (HRESULT)VC(con, 11, &prob);
    CHECKF(hr == S_OK && prob == 0, "Probability starts Default (%#lx, %d)", hr, prob);
    hr = (HRESULT)VC(con, 12, 2);
    prob = -1;
    VC(con, 11, &prob);
    CHECKF(hr == S_OK && prob == 2, "put_Probability(Max) is read back (%#lx, %d)", hr, prob);
    hr = (HRESULT)VC(con, 12, 3);
    prob = -1;
    VC(con, 11, &prob);
    CHECKF(hr == E_INVALIDARG && prob == 2, "put_Probability(3) is E_INVALIDARG and keeps the old value (%#lx, %d)", hr, prob);

    hr = (HRESULT)VC(list, 6, &cmds);
    CHECKF(hr == S_OK && cmds, "Commands (%#lx)", hr);
    if (cmds)
    {
        check_inspectable(cmds, &vec_case);
        hr = (HRESULT)VC(cmds, 7, &size);
        CHECKF(hr == S_OK && size == 2, "Commands has the two commands (%#lx, %u)", hr, size);
        hr = (HRESULT)VC(cmds, 8, &view);
        CHECKF(hr == S_OK && view, "Commands GetView (%#lx)", hr);
        if (view)
        {
            check_inspectable(view, &view_case);
            if (SUCCEEDED(IInspectable_QueryInterface(view, &IID_IIterable_HSTRING, &iterable)))
            {
                hr = (HRESULT)VC(iterable, 6, &iter);
                CHECKF(hr == S_OK && iter, "view First (%#lx)", hr);
                if (iter) { check_inspectable(iter, &iter_case); UREL(iter); }
                UREL(iterable);
            }
            UREL(view);
        }
        UREL(cmds);
    }
    UREL(con);
    UREL(list);

    /* no tag: an empty one */
    hr = (HRESULT)VC(cf, 6, &src, &plain);
    CHECKF(hr == S_OK && plain, "Create without a tag (%#lx)", hr);
    if (plain)
    {
        con = NULL; str = (HSTRING)0x1;
        IInspectable_QueryInterface(plain, &IID_ISpeechRecognitionConstraint, &con);
        hr = (HRESULT)VC(con, 8, &str);
        CHECKF(hr == S_OK && !WindowsGetStringLen(str), "Tag of a constraint made without one is empty (%#lx)", hr);
        enabled = 2;
        VC(con, 6, &enabled);
        CHECKF(enabled == TRUE, "IsEnabled starts TRUE without a tag too (%d)", enabled);
        WindowsDeleteString(str);
        UREL(con);
        UREL(plain);
    }
    UREL(cf);
    IActivationFactory_Release(f);
    WindowsDeleteString(items[0]); WindowsDeleteString(items[1]); WindowsDeleteString(tag);
}

static void test_synthesizer(void)
{
    static const struct inspectable_case synth_case = { "SpeechSynthesizer", L"Windows.Media.SpeechSynthesis.SpeechSynthesizer",
        { &IID_ISpeechSynthesizer, &IID_ISpeechSynthesizer2, &IID_IClosable } };
    static const struct inspectable_case op_case = { "synthesis operation",
        L"Windows.Foundation.IAsyncOperation`1<Windows.Media.SpeechSynthesis.SpeechSynthesisStream>",
        { &IID_IAsyncOperation_SpeechSynthesisStream, &IID_IAsyncInfo } };
    static const struct inspectable_case stream_case = { "SpeechSynthesisStream", L"Windows.Media.SpeechSynthesis.SpeechSynthesisStream",
        { &IID_ISpeechSynthesisStream } };
    static const struct inspectable_case markers_case = { "markers view", L"Windows.Foundation.Collections.IVectorView`1<Windows.Media.IMediaMarker>",
        { &IID_IVectorView_IMediaMarker, &IID_IIterable_IMediaMarker } };
    static const struct inspectable_case voices_case = { "AllVoices view",
        L"Windows.Foundation.Collections.IVectorView`1<Windows.Media.SpeechSynthesis.VoiceInformation>",
        { &IID_IVectorView_VoiceInformation } };
    IActivationFactory *f = get_factory(L"Windows.Media.SpeechSynthesis.SpeechSynthesizer");
    IInspectable *obj = NULL, *s2 = NULL, *closable = NULL;
    void *ops[3] = { 0 }, *stream = NULL, *markers = NULL, *statics = NULL, *voices = NULL;
    HSTRING text = hs(L"hello");
    HRESULT hr;
    int i, status;

    if (!f) { check(0, "synthesizer factory"); return; }
    hr = IActivationFactory_ActivateInstance(f, &obj);
    CHECKF(hr == S_OK, "SpeechSynthesizer activates (%#lx)", hr);
    if (hr != S_OK) return;
    check_inspectable(obj, &synth_case);
    if (SUCCEEDED(IInspectable_QueryInterface(obj, &IID_ISpeechSynthesizer2, (void **)&s2))) { check_inspectable(s2, &synth_case); IInspectable_Release(s2); }
    if (SUCCEEDED(IInspectable_QueryInterface(obj, &IID_IClosable, (void **)&closable))) { check_inspectable(closable, &synth_case); IInspectable_Release(closable); }

    for (i = 0; i < 3; i++)
    {
        hr = (HRESULT)VC(obj, 6, text, &ops[i]);
        CHECKF(hr == S_OK && ops[i], "SynthesizeTextToStreamAsync #%d (%#lx)", i + 1, hr);
    }
    check_inspectable(ops[0], &op_case);
    wait_async(ops[2], &status);
    hr = (HRESULT)VC(ops[2], 8, &stream);
    CHECKF(hr == S_OK && stream, "GetResults (%#lx)", hr);
    if (stream)
    {
        check_inspectable(stream, &stream_case);
        hr = (HRESULT)VC(stream, 6, &markers);
        CHECKF(hr == S_OK && markers, "get_Markers (%#lx)", hr);
        if (markers) { check_inspectable(markers, &markers_case); UREL(markers); }
        UREL(stream);
    }
    check_async(ops[0], "synthesis operation 1", 1);
    check_async(ops[1], "synthesis operation 2", 2);
    check_async(ops[2], "synthesis operation 3", 3);
    for (i = 0; i < 3; i++) UREL(ops[i]);

    IActivationFactory_QueryInterface(f, &IID_IInstalledVoicesStatic, &statics);
    hr = (HRESULT)VC(statics, 6, &voices);
    CHECKF(hr == S_OK && voices, "get_AllVoices (%#lx)", hr);
    if (voices) { check_inspectable(voices, &voices_case); UREL(voices); }
    UREL(statics);
    IInspectable_Release(obj);
    IActivationFactory_Release(f);
    WindowsDeleteString(text);
}

static void test_recognizer(void)
{
    static const struct inspectable_case rec_case = { "SpeechRecognizer", L"Windows.Media.SpeechRecognition.SpeechRecognizer",
        { &IID_ISpeechRecognizer, &IID_IClosable, &IID_ISpeechRecognizer2 } };
    static const struct inspectable_case session_case = { "continuous session",
        L"Windows.Media.SpeechRecognition.SpeechContinuousRecognitionSession", { &IID_ISpeechContinuousRecognitionSession } };
    static const struct inspectable_case op_case = { "compile operation",
        L"Windows.Foundation.IAsyncOperation`1<Windows.Media.SpeechRecognition.SpeechRecognitionCompilationResult>",
        { &IID_IAsyncOperation_SpeechRecognitionCompilationResult, &IID_IAsyncInfo } };
    static const struct inspectable_case result_case = { "compilation result",
        L"Windows.Media.SpeechRecognition.SpeechRecognitionCompilationResult", { &IID_ISpeechRecognitionCompilationResult } };
    static const struct inspectable_case action_case = { "session action", L"Windows.Foundation.IAsyncAction",
        { &IID_IAsyncAction, &IID_IAsyncInfo } };
    static const struct { const char *name; int slot, rec2; } events[] =
    {
        { "RecognitionQualityDegrading", 13, 0 }, { "StateChanged", 15, 0 }, { "HypothesisGenerated", 9, 1 },
    };
    IActivationFactory *f = get_factory(L"Windows.Media.SpeechRecognition.SpeechRecognizer");
    IInspectable *obj = NULL, *other = NULL;
    void *rec = NULL, *rec2 = NULL, *session = NULL, *op = NULL, *result = NULL, *act1 = NULL, *act2 = NULL, *act3 = NULL, *act4 = NULL;
    LONGLONG timeout = 0;
    HRESULT hr;
    unsigned int i;
    int status;

    if (!f) { check(0, "recognizer factory"); return; }
    hr = IActivationFactory_ActivateInstance(f, &obj);
    if (FAILED(hr)) { printf("SKIP  recognizer: ActivateInstance = %#lx\n", hr); IActivationFactory_Release(f); return; }
    check_inspectable(obj, &rec_case);
    IInspectable_QueryInterface(obj, &IID_ISpeechRecognizer, &rec);
    IInspectable_QueryInterface(obj, &IID_ISpeechRecognizer2, &rec2);
    if (SUCCEEDED(IInspectable_QueryInterface(obj, &IID_IClosable, (void **)&other))) { check_inspectable(other, &rec_case); IInspectable_Release(other); }
    check_inspectable(rec2, &rec_case);

    for (i = 0; i < ARRAY_SIZE(events); i++)
    {
        struct handler h = { handler_vtbl, 1, 0 };
        void *target = events[i].rec2 ? rec2 : rec;
        INT64 token = 0;
        char what[100];

        hr = (HRESULT)VC(target, events[i].slot, &h, &token);
        snprintf(what, sizeof(what), "%s: add_ gives a token and holds the handler (%#lx, ref %ld)", events[i].name, hr, h.ref);
        check(hr == S_OK && token && h.ref == 2, what);
        hr = (HRESULT)VC(target, events[i].slot, NULL, &token);
        CHECKF(hr == E_INVALIDARG, "%s: add_ with a NULL handler is E_INVALIDARG (%#lx)", events[i].name, hr);
        hr = (HRESULT)VC(target, events[i].slot + 1, token);
        CHECKF(hr == S_OK && h.ref == 1, "%s: remove_ releases the handler (%#lx, ref %ld)", events[i].name, hr, h.ref);
    }

    hr = (HRESULT)VC(rec, 10, &op);
    CHECKF(hr == S_OK && op, "CompileConstraintsAsync (%#lx)", hr);
    if (op)
    {
        check_inspectable(op, &op_case);
        wait_async(op, &status);
        hr = (HRESULT)VC(op, 8, &result);
        if (hr == S_OK && result) { check_inspectable(result, &result_case); UREL(result); }
        else check(0, "compile GetResults");
        check_async(op, "compile operation", 1);
        UREL(op);
    }

    hr = (HRESULT)VC(rec2, 6, &session);
    CHECKF(hr == S_OK && session, "ContinuousRecognitionSession (%#lx)", hr);
    if (session)
    {
        check_inspectable(session, &session_case);
        hr = (HRESULT)VC(session, 6, &timeout);
        CHECKF(hr == S_OK && timeout == 200000000, "AutoStopSilenceTimeout starts at 20 s (%#lx, %lld)", hr, timeout);
        hr = (HRESULT)VC(session, 7, (LONGLONG)50000000);
        timeout = 0;
        VC(session, 6, &timeout);
        CHECKF(hr == S_OK && timeout == 50000000, "AutoStopSilenceTimeout set to 5 s is read back (%#lx, %lld)", hr, timeout);
        hr = (HRESULT)VC(session, 7, (LONGLONG)-1);
        timeout = 0;
        VC(session, 6, &timeout);
        CHECKF(hr == E_INVALIDARG && timeout == 50000000, "a negative AutoStopSilenceTimeout is E_INVALIDARG and ignored (%#lx, %lld)", hr, timeout);

        act1 = (void *)0x1;
        hr = (HRESULT)VC(session, 9, 7, &act1);
        CHECKF(hr == E_INVALIDARG && !act1, "StartWithModeAsync(7) is E_INVALIDARG with no action (%#lx %p)", hr, act1);
        act1 = NULL;
        hr = (HRESULT)VC(session, 9, 1, &act1);
        CHECKF(hr == S_OK && act1, "StartWithModeAsync(PauseOnRecognition) (%#lx)", hr);
        act2 = (void *)0x1;
        hr = (HRESULT)VC(session, 8, &act2);
        CHECKF(hr == 0x80131509 && !act2, "StartAsync while started is COR_E_INVALIDOPERATION (%#lx %p)", hr, act2);
        hr = (HRESULT)VC(session, 11, &act3);
        CHECKF(hr == S_OK && act3, "CancelAsync stops the session (%#lx)", hr);
        if (act3) check_inspectable(act3, &action_case);
        act2 = (void *)0x1;
        hr = (HRESULT)VC(session, 11, &act2);
        CHECKF(hr == 0x80131509 && !act2, "CancelAsync while idle is COR_E_INVALIDOPERATION (%#lx %p)", hr, act2);
        hr = (HRESULT)VC(session, 8, &act4);
        CHECKF(hr == S_OK && act4, "StartAsync after the cancel (%#lx)", hr);
        if (act1) check_async(act1, "StartWithModeAsync action", 1);
        if (act3) check_async(act3, "CancelAsync action", 3);
        if (act4)
        {
            HRESULT idhr;
            UINT32 id = async_id(act4, &idhr);
            CHECKF(idhr == S_OK && id == 5, "ids count failed starts too: the next StartAsync is 5 (%u)", id);
            hr = (HRESULT)VC(session, 10, &act2);
            CHECKF(hr == S_OK, "StopAsync (%#lx)", hr);
            if (act2) UREL(act2);
            UREL(act4);
        }
        if (act1) UREL(act1);
        if (act3) UREL(act3);
        UREL(session);
    }
    UREL(rec);
    UREL(rec2);
    IInspectable_Release(obj);
    IActivationFactory_Release(f);
}

static void test_dllgetclassobject(void)
{
    HRESULT (WINAPI *get_class_object)(REFCLSID, REFIID, void **);
    HMODULE mod = LoadLibraryW(L"windows.media.speech.dll");
    void *out = (void *)0x1234;
    HRESULT hr;

    get_class_object = mod ? (void *)GetProcAddress(mod, "DllGetClassObject") : NULL;
    if (!get_class_object) { check(0, "DllGetClassObject is exported"); return; }
    hr = get_class_object(&IID_Bogus, &IID_IUnknown, &out);
    CHECKF(hr == CLASS_E_CLASSNOTAVAILABLE && !out, "DllGetClassObject of an unknown CLSID: CLASS_E_CLASSNOTAVAILABLE and NULL (%#lx %p)", hr, out);
}

int main(void)
{
    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(hr)) { printf("FAIL  RoInitialize %#lx\n", hr); return 1; }
    test_factories();
    test_list_constraint();
    test_synthesizer();
    test_recognizer();
    test_dllgetclassobject();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
