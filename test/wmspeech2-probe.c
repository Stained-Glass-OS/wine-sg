/* windows.media.speech, part 2 (patch 2811), run by test/wmspeech2-gate.sh on
 * Xvfb: the recognizer's languages, timeouts and UI options, TrySetSystemSpeechLanguageAsync,
 * RecognizeAsync / RecognizeWithUIAsync (no engine: a TimeoutExceeded result),
 * StopRecognitionAsync, Close, the StateChanged event, and the synthesizer's
 * Options, Voice, DefaultVoice and Close. Interfaces are called through their
 * vtable slots.
 *
 *   wmspeech2-probe.exe */
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
DEFINE_GUID(IID_ISpeechRecognizerTimeouts, 0x2ef76fca, 0x6a3c, 0x4dca, 0xa1,0x53, 0xdf,0x1b,0xc8,0x8a,0x79,0xaf);
DEFINE_GUID(IID_ISpeechRecognizerUIOptions, 0x7888d641, 0xb92b, 0x44ba, 0xa2,0x5f, 0xd1,0x86,0x46,0x30,0x64,0x1f);
DEFINE_GUID(IID_ISpeechRecognizerStateChangedEventArgs, 0x563d4f09, 0xba03, 0x4bad, 0xad,0x81, 0xdd,0xc6,0xc4,0xda,0xb0,0xc3);
DEFINE_GUID(IID_ISpeechRecognitionResult, 0x4e303157, 0x034e, 0x4652, 0x85,0x7e, 0xd0,0x45,0x4c,0xc4,0xbe,0xec);
DEFINE_GUID(IID_ISpeechRecognitionResult2, 0xaf7ed1ba, 0x451b, 0x4166, 0xa0,0xc1, 0x1f,0xfe,0x84,0x03,0x2d,0x03);
DEFINE_GUID(IID_ISpeechSynthesizerOptions, 0xa0e23871, 0xcc3d, 0x43c9, 0x91,0xb1, 0xee,0x18,0x53,0x24,0xd8,0x3d);
DEFINE_GUID(IID_ISpeechSynthesizerOptions2, 0x1cbef60e, 0x119c, 0x4bed, 0xb1,0x18, 0xd2,0x50,0xc3,0xa2,0x57,0x93);
DEFINE_GUID(IID_ISpeechSynthesizerOptions3, 0x401ed877, 0x902c, 0x4814, 0xa5,0x82, 0xa5,0xd0,0xc0,0x76,0x9f,0xa8);
DEFINE_GUID(IID_IAsyncOperation_boolean, 0xcdb5efb3, 0x5788, 0x509d, 0x9b,0xe1, 0x71,0xcc,0xb8,0xa3,0x36,0x2a);
DEFINE_GUID(IID_IAsyncOperation_SpeechRecognitionResult, 0xba3eebe8, 0x8d7c, 0x51f2, 0x9e,0xd4, 0xeb,0xaf,0xe3,0x67,0x4d,0xb4);
DEFINE_GUID(IID_IVectorView_Language, 0x144b0f3d, 0x2d59, 0x5dd2, 0xb0,0x12, 0x90,0x8e,0xc3,0xe0,0x64,0x35);
DEFINE_GUID(IID_IVectorView_SpeechRecognitionResult, 0x0e37810f, 0x1de6, 0x5199, 0x83,0x3f, 0x5a,0x6b,0x0b,0xd9,0x1e,0x23);
DEFINE_GUID(IID_IVectorView_HSTRING, 0x2f13c006, 0xa03a, 0x5f69, 0xb0,0x90, 0x75,0xa4,0x3e,0x33,0x42,0x3e);
DEFINE_GUID(IID_ILanguage, 0xea79a752, 0xf7c2, 0x4265, 0xb1,0xbd, 0xc4,0xde,0xc4,0xe4,0xf0,0x80);
DEFINE_GUID(IID_IVectorView_VoiceInformation, 0xee8d63ce, 0x51ac, 0x5984, 0x89,0x1b, 0xd2,0x32,0xfa,0x7f,0x64,0x53);
DEFINE_GUID(IID_ISpeechRecognizerStatics2, 0x1d1b0d95, 0x7565, 0x4ef9, 0xa2,0xf3, 0xba,0x15,0x16,0x2a,0x96,0xcf);
DEFINE_GUID(IID_ISpeechRecognizerStatics, 0x87a35eac, 0xa7dc, 0x4b0b, 0xbc,0xc9, 0x24,0xf4,0x7c,0x0b,0x7e,0xbf);
DEFINE_GUID(IID_IIterable_Language, 0x48409a10, 0x61b6, 0x5db1, 0xa6,0x9d, 0x8a,0xbc,0x46,0xac,0x60,0x8a);
DEFINE_GUID(IID_Bogus, 0x12345678, 0x1234, 0x1234, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34);
DEFINE_GUID(IID_ISpeechRecognizer, 0x0bc3c9cb, 0xc26a, 0x40f2, 0xae,0xb5, 0x80,0x96,0xb2,0xe4,0x80,0x73);
DEFINE_GUID(IID_ISpeechRecognizer2, 0x63c9baf1, 0x91e3, 0x4ea4, 0x86,0xa1, 0x7c,0x38,0x67,0xd0,0x84,0xa6);
DEFINE_GUID(IID_ISpeechRecognizerFactory, 0x60c488dd, 0x7fb8, 0x4033, 0xac,0x70, 0xd0,0x46,0xf6,0x48,0x18,0xe1);
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
DEFINE_GUID(IID_IVector_ISpeechRecognitionConstraint, 0x2691d763, 0x561e, 0x5060, 0xbb,0xc9, 0x7b,0x07,0x36,0x1a,0xcc,0x95);
DEFINE_GUID(IID_IVectorView_ISpeechRecognitionConstraint, 0x341dee1d, 0x6ac2, 0x5d06, 0x90,0x26, 0xb3,0x0a,0xda,0x20,0x56,0x65);
DEFINE_GUID(IID_IIterable_ISpeechRecognitionConstraint, 0x88e6436c, 0x3253, 0x520b, 0x9e,0xd8, 0xa6,0x3b,0x17,0x8c,0x44,0xa2);
DEFINE_GUID(IID_IIterator_ISpeechRecognitionConstraint, 0x738f00b1, 0xe18c, 0x5140, 0xa5,0x3a, 0xf1,0x78,0x8d,0x10,0xc9,0x3d);
DEFINE_GUID(IID_IIterable_HSTRING, 0xe2fcc7c1, 0x3bfc, 0x5a0b, 0xb2,0xb0, 0x72,0xe7,0x69,0xd1,0xcb,0x7e);
DEFINE_GUID(IID_IIterator_HSTRING, 0x8c304ebb, 0x6615, 0x50a4, 0x88,0x29, 0x87,0x9e,0xcd,0x44,0x32,0x36);
DEFINE_GUID(IID_IVector_HSTRING, 0x98b9acc1, 0x4b56, 0x532e, 0xac,0x73, 0x03,0xd5,0x29,0x1c,0xca,0x90);
DEFINE_GUID(IID_IAsyncAction, 0x5a648006, 0x843a, 0x4da9, 0x86,0x5b, 0x9d,0x26,0xe5,0xdf,0xad,0x7b);
DEFINE_GUID(IID_IClosable, 0x30d5a829, 0x7fa4, 0x4026, 0x83,0xbb, 0xd7,0x5b,0xae,0x4e,0xa9,0x9e);
DEFINE_GUID(IID_IAsyncOperationCompletedHandler_SpeechSynthesisStream, 0xc972b996, 0x6165, 0x50d4, 0xaf,0x60, 0xa8,0xc3,0xdf,0x51,0xd0,0x92);
DEFINE_GUID(IID_IAsyncOperationCompletedHandler_SpeechRecognitionCompilationResult, 0x78c859bd, 0x14d4, 0x5c40, 0xab,0xff, 0x49,0x06,0x16,0xd5,0xe9,0x2d);
DEFINE_GUID(IID_IAsyncActionCompletedHandler, 0xa4ed5c81, 0x76c9, 0x40bd, 0x8b,0xe6, 0xb1,0xd9,0x0f,0xb2,0x0a,0xe7);

#define UREL(p) ((IUnknown *)(p))->lpVtbl->Release((IUnknown *)(p))
#undef IInspectable_QueryInterface
#define IInspectable_QueryInterface(p, ...) ((IInspectable *)(p))->lpVtbl->QueryInterface((IInspectable *)(p), __VA_ARGS__)
#ifndef RO_E_CLOSED
#define RO_E_CLOSED ((HRESULT)0x80000013)
#endif
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


typedef HRESULT (WINAPI *dbl_fn)(void *, double);
#define DBL_PUT(obj, n, v) ((dbl_fn)(*(void ***)(obj))[n])(obj, v)

/* StateChanged handler: Invoke(sender, args) records args->State */
struct state_handler { void *vtbl; LONG ref; LONG count; int states[16]; };
static HRESULT WINAPI sh_invoke(void *iface, void *sender, void *args)
{
    struct state_handler *h = iface;
    int state = -1;
    VC(args, 6, &state);
    if (h->count < 16) h->states[h->count] = state;
    InterlockedIncrement(&h->count);
    return S_OK;
}
static void *state_handler_vtbl[] = { h_qi, h_addref, h_release, sh_invoke };
static struct state_handler state_handler_obj = { state_handler_vtbl, 1 };

static const WCHAR *str_of(HSTRING s) { return s ? WindowsGetStringRawBuffer(s, NULL) : L""; }

static void check_language_view(void *view, const char *name)
{
    static const struct inspectable_case c = { "language view", L"Windows.Foundation.Collections.IVectorView`1<Windows.Globalization.Language>",
        { &IID_IVectorView_Language, &IID_IIterable_Language } };
    UINT32 size = 7;
    HRESULT hr;
    struct inspectable_case cc = c;
    cc.name = name;
    check_inspectable(view, &cc);
    hr = (HRESULT)VC(view, 7, &size);
    CHECKF(hr == S_OK && size == 0, "%s: no installed languages, so an empty view (%#lx, %u)", name, hr, size);
}

static void test_recognizer(void)
{
    static const struct inspectable_case timeouts_case = { "Timeouts", L"Windows.Media.SpeechRecognition.SpeechRecognizerTimeouts",
        { &IID_ISpeechRecognizerTimeouts } };
    static const struct inspectable_case ui_case = { "UIOptions", L"Windows.Media.SpeechRecognition.SpeechRecognizerUIOptions",
        { &IID_ISpeechRecognizerUIOptions } };
    static const struct inspectable_case bool_op_case = { "boolean operation", L"Windows.Foundation.IAsyncOperation`1<Boolean>",
        { &IID_IAsyncOperation_boolean, &IID_IAsyncInfo } };
    static const struct inspectable_case rec_op_case = { "recognize operation",
        L"Windows.Foundation.IAsyncOperation`1<Windows.Media.SpeechRecognition.SpeechRecognitionResult>",
        { &IID_IAsyncOperation_SpeechRecognitionResult, &IID_IAsyncInfo } };
    static const struct inspectable_case result_case = { "recognition result", L"Windows.Media.SpeechRecognition.SpeechRecognitionResult",
        { &IID_ISpeechRecognitionResult, &IID_ISpeechRecognitionResult2 } };
    IActivationFactory *f = get_factory(L"Windows.Media.SpeechRecognition.SpeechRecognizer");
    void *statics = NULL, *statics2 = NULL, *rfactory = NULL, *lang = NULL, *lang2 = NULL, *view = NULL;
    void *rec = NULL, *rec2 = NULL, *closable = NULL, *session = NULL, *obj = NULL;
    void *timeouts = NULL, *timeouts_b = NULL, *ui = NULL, *op = NULL, *res = NULL, *res2 = NULL, *act = NULL, *alts = NULL, *path = NULL;
    HSTRING tag1 = NULL, tag2 = NULL, str = NULL;
    LONGLONG t = 0;
    BOOLEAN b = 2, bv;
    int i, status, st = -1;
    UINT32 size = 7;
    double d = -1;
    HRESULT hr;

    if (!f) { check(0, "recognizer factory"); return; }
    IActivationFactory_QueryInterface(f, &IID_ISpeechRecognizerStatics, &statics);
    IActivationFactory_QueryInterface(f, &IID_ISpeechRecognizerStatics2, &statics2);
    IActivationFactory_QueryInterface(f, &IID_ISpeechRecognizerFactory, &rfactory);

    hr = (HRESULT)VC(statics, 6, &lang);
    CHECKF(hr == S_OK && lang, "SystemSpeechLanguage (%#lx)", hr);
    if (lang) { VC(lang, 6, &tag1); CHECKF(WindowsGetStringLen(tag1) > 0, "the system speech language has a tag (%ls)", str_of(tag1)); }
    hr = (HRESULT)VC(statics, 7, &view);
    CHECKF(hr == S_OK && view, "SupportedTopicLanguages (%#lx)", hr);
    if (view) { check_language_view(view, "SupportedTopicLanguages"); UREL(view); }
    view = NULL;
    hr = (HRESULT)VC(statics, 8, &view);
    CHECKF(hr == S_OK && view, "SupportedGrammarLanguages (%#lx)", hr);
    if (view) { check_language_view(view, "SupportedGrammarLanguages"); UREL(view); }

    /* TrySetSystemSpeechLanguageAsync: no language can be installed, so nothing changes */
    op = (void *)0x1;
    hr = (HRESULT)VC(statics2, 6, NULL, &op);
    CHECKF(hr == E_INVALIDARG && !op, "TrySetSystemSpeechLanguageAsync(NULL) is E_INVALIDARG (%#lx %p)", hr, op);
    for (i = 1; i <= 2; i++)
    {
        op = NULL;
        hr = (HRESULT)VC(statics2, 6, lang, &op);
        CHECKF(hr == S_OK && op, "TrySetSystemSpeechLanguageAsync #%d (%#lx)", i, hr);
        if (!op) continue;
        if (i == 1) check_inspectable(op, &bool_op_case);
        b = 2;
        hr = (HRESULT)VC(op, 8, &b);
        CHECKF(hr == S_OK && b == FALSE, "TrySetSystemSpeechLanguageAsync reports FALSE (%#lx, %d)", hr, b);
        check_async(op, "boolean operation", i);
        UREL(op);
    }

    /* a recognizer made for a language reports it */
    hr = (HRESULT)VC(rfactory, 6, lang, &rec);
    if (FAILED(hr)) { printf("SKIP  recognizer: Create = %#lx\n", hr); goto done; }
    hr = (HRESULT)VC(rec, 6, &lang2);
    CHECKF(hr == S_OK && lang2 == lang, "CurrentLanguage is the language the recognizer was made for (%#lx %p %p)", hr, lang2, lang);
    if (lang2) UREL(lang2);
    UREL(rec); rec = NULL;

    /* the default one */
    {
        IInspectable *inst = NULL;
        hr = IActivationFactory_ActivateInstance(f, &inst);
        if (FAILED(hr)) { printf("SKIP  recognizer: ActivateInstance = %#lx\n", hr); goto done; }
        IInspectable_QueryInterface(inst, &IID_ISpeechRecognizer, &rec);
        IInspectable_QueryInterface(inst, &IID_ISpeechRecognizer2, &rec2);
        IInspectable_QueryInterface(inst, &IID_IClosable, &closable);
        IInspectable_Release(inst);
    }
    lang2 = NULL;
    hr = (HRESULT)VC(rec, 6, &lang2);
    CHECKF(hr == S_OK && lang2, "CurrentLanguage of a default recognizer (%#lx)", hr);
    if (lang2)
    {
        VC(lang2, 6, &tag2);
        CHECKF(!wcscmp(str_of(tag1), str_of(tag2)), "it is the system speech language (%ls, %ls)", str_of(tag1), str_of(tag2));
        UREL(lang2);
    }

    /* Timeouts */
    hr = (HRESULT)VC(rec, 8, &timeouts);
    CHECKF(hr == S_OK && timeouts, "Timeouts (%#lx)", hr);
    if (timeouts)
    {
        check_inspectable(timeouts, &timeouts_case);
        VC(rec, 8, &timeouts_b);
        CHECKF(timeouts_b == timeouts, "Timeouts is the same object every time (%p %p)", timeouts, timeouts_b);
        UREL(timeouts_b);
        t = -1; VC(timeouts, 6, &t);
        CHECKF(t == 50000000, "InitialSilenceTimeout starts at 5 s (%lld)", t);
        t = -1; VC(timeouts, 8, &t);
        CHECKF(t == 1500000, "EndSilenceTimeout starts at 150 ms (%lld)", t);
        t = -1; VC(timeouts, 10, &t);
        CHECKF(t == 0, "BabbleTimeout starts off (%lld)", t);
        hr = (HRESULT)VC(timeouts, 7, (LONGLONG)100000000);
        t = -1; VC(timeouts, 6, &t);
        CHECKF(hr == S_OK && t == 100000000, "InitialSilenceTimeout is read back (%#lx, %lld)", hr, t);
        hr = (HRESULT)VC(timeouts, 9, (LONGLONG)-5);
        t = -1; VC(timeouts, 8, &t);
        CHECKF(hr == E_INVALIDARG && t == 1500000, "a negative EndSilenceTimeout is E_INVALIDARG and ignored (%#lx, %lld)", hr, t);
        hr = (HRESULT)VC(timeouts, 11, (LONGLONG)20000000);
        t = -1; VC(timeouts, 10, &t);
        CHECKF(hr == S_OK && t == 20000000, "BabbleTimeout is read back (%#lx, %lld)", hr, t);
        UREL(timeouts);
    }

    /* UIOptions */
    hr = (HRESULT)VC(rec, 9, &ui);
    CHECKF(hr == S_OK && ui, "UIOptions (%#lx)", hr);
    if (ui)
    {
        check_inspectable(ui, &ui_case);
        VC(ui, 6, &str);
        CHECKF(!WindowsGetStringLen(str), "ExampleText starts empty");
        WindowsDeleteString(str); str = NULL;
        VC(ui, 8, &str);
        CHECKF(!WindowsGetStringLen(str), "AudiblePrompt starts empty");
        WindowsDeleteString(str); str = NULL;
        b = 2; VC(ui, 10, &b);
        bv = 2; VC(ui, 12, &bv);
        CHECKF(b == TRUE && bv == TRUE, "IsReadBackEnabled and ShowConfirmation start TRUE (%d %d)", b, bv);
        str = hs(L"say hello");
        hr = (HRESULT)VC(ui, 7, str);
        WindowsDeleteString(str); str = NULL;
        VC(ui, 6, &str);
        CHECKF(hr == S_OK && !wcscmp(str_of(str), L"say hello"), "ExampleText is read back (%#lx %ls)", hr, str_of(str));
        WindowsDeleteString(str); str = NULL;
        VC(ui, 11, FALSE); VC(ui, 13, FALSE);
        b = 2; VC(ui, 10, &b);
        bv = 2; VC(ui, 12, &bv);
        CHECKF(b == FALSE && bv == FALSE, "IsReadBackEnabled and ShowConfirmation are read back (%d %d)", b, bv);
        UREL(ui);
    }

    /* RecognizeAsync / RecognizeWithUIAsync: no engine, so nothing is heard */
    for (i = 1; i <= 2; i++)
    {
        op = NULL;
        hr = (HRESULT)VC(rec, i == 1 ? 11 : 12, &op);
        CHECKF(hr == S_OK && op, "%s (%#lx)", i == 1 ? "RecognizeAsync" : "RecognizeWithUIAsync", hr);
        if (!op) continue;
        if (i == 1) check_inspectable(op, &rec_op_case);
        wait_async(op, &status);
        res = NULL;
        hr = (HRESULT)VC(op, 8, &res);
        CHECKF(hr == S_OK && res, "recognize GetResults (%#lx)", hr);
        if (res)
        {
            if (i == 1) check_inspectable(res, &result_case);
            st = -1; VC(res, 6, &st);
            CHECKF(st == 7, "the result is TimeoutExceeded (%d)", st);
            str = NULL; VC(res, 7, &str);
            CHECKF(!WindowsGetStringLen(str), "the result has no text");
            WindowsDeleteString(str); str = NULL;
            st = -1; VC(res, 8, &st);
            CHECKF(st == 3, "the confidence is Rejected (%d)", st);
            alts = NULL; hr = (HRESULT)VC(res, 10, 5, &alts);
            size = 7; if (alts) VC(alts, 7, &size);
            CHECKF(hr == S_OK && alts && size == 0, "no alternates (%#lx %u)", hr, size);
            if (alts) UREL(alts);
            path = NULL; hr = (HRESULT)VC(res, 12, &path);
            size = 7; if (path) VC(path, 7, &size);
            CHECKF(hr == S_OK && path && size == 0, "an empty rule path (%#lx %u)", hr, size);
            if (path) UREL(path);
            d = -1; VC(res, 13, &d);
            CHECKF(d == 0.0, "raw confidence is 0");
            if (SUCCEEDED(IInspectable_QueryInterface(res, &IID_ISpeechRecognitionResult2, &res2))) UREL(res2);
            UREL(res);
        }
        check_async(op, "recognize operation", i);
        UREL(op);
    }

    /* StateChanged follows the session */
    {
        INT64 token = 0;
        void *a1 = NULL, *a2 = NULL, *a3 = NULL;

        hr = (HRESULT)VC(rec, 15, &state_handler_obj, &token);
        CHECKF(hr == S_OK && token, "add_StateChanged (%#lx)", hr);
        hr = (HRESULT)VC(rec2, 6, &session);
        CHECKF(hr == S_OK && session, "session");
        if (session)
        {
            VC(session, 8, &a1);
            wait_async(a1, &status);
            VC(session, 12, &a2);       /* PauseAsync */
            VC(session, 12, &a3);       /* again: no change */
            VC(session, 13);            /* Resume */
            VC(session, 13);            /* again: no change */
            if (a1) UREL(a1);
            a1 = NULL;
            VC(session, 10, &a1);       /* StopAsync */
            if (a1) UREL(a1);
            if (a2) UREL(a2);
            if (a3) UREL(a3);
            CHECKF(state_handler_obj.count == 4, "StateChanged fired four times (%ld)", state_handler_obj.count);
            CHECKF(state_handler_obj.states[0] == 1 && state_handler_obj.states[1] == 6 && state_handler_obj.states[2] == 1 &&
                   state_handler_obj.states[3] == 0,
                   "the states were Capturing, Paused, Capturing, Idle (%d %d %d %d)", state_handler_obj.states[0],
                   state_handler_obj.states[1], state_handler_obj.states[2], state_handler_obj.states[3]);
            UREL(session);
        }
        VC(rec, 16, token);
    }

    /* StopRecognitionAsync with nothing running, then Close */
    act = NULL;
    hr = (HRESULT)VC(rec2, 8, &act);
    CHECKF(hr == S_OK && act, "StopRecognitionAsync when idle (%#lx)", hr);
    if (act) UREL(act);
    hr = (HRESULT)VC(closable, 6);
    CHECKF(hr == S_OK, "Close (%#lx)", hr);
    hr = (HRESULT)VC(closable, 6);
    CHECKF(hr == S_OK, "Close twice (%#lx)", hr);
    op = (void *)0x1;
    hr = (HRESULT)VC(rec, 10, &op);
    CHECKF(hr == RO_E_CLOSED && !op, "CompileConstraintsAsync after Close is RO_E_CLOSED (%#lx %p)", hr, op);
    op = (void *)0x1;
    hr = (HRESULT)VC(rec, 11, &op);
    CHECKF(hr == RO_E_CLOSED && !op, "RecognizeAsync after Close is RO_E_CLOSED (%#lx %p)", hr, op);
    act = (void *)0x1;
    hr = (HRESULT)VC(rec2, 8, &act);
    CHECKF(hr == RO_E_CLOSED && !act, "StopRecognitionAsync after Close is RO_E_CLOSED (%#lx %p)", hr, act);
    VC(rec2, 6, &session);
    act = (void *)0x1;
    hr = (HRESULT)VC(session, 8, &act);
    CHECKF(hr == RO_E_CLOSED && !act, "StartAsync after Close is RO_E_CLOSED (%#lx %p)", hr, act);
    UREL(session);

done:
    (void)obj;
    if (rec) UREL(rec);
    if (rec2) UREL(rec2);
    if (closable) UREL(closable);
    if (lang) UREL(lang);
    WindowsDeleteString(tag1); WindowsDeleteString(tag2);
    if (statics) UREL(statics);
    if (statics2) UREL(statics2);
    if (rfactory) UREL(rfactory);
    IActivationFactory_Release(f);
}

/* a stand-in IVoiceInformation: only its identity and reference count matter */
static struct handler fake_voice = { handler_vtbl, 1, 0 };

static void test_synthesizer(void)
{
    static const struct inspectable_case options_case = { "SpeechSynthesizerOptions",
        L"Windows.Media.SpeechSynthesis.SpeechSynthesizerOptions",
        { &IID_ISpeechSynthesizerOptions, &IID_ISpeechSynthesizerOptions2, &IID_ISpeechSynthesizerOptions3 } };
    IActivationFactory *f = get_factory(L"Windows.Media.SpeechSynthesis.SpeechSynthesizer");
    IInspectable *obj = NULL;
    void *synth = NULL, *synth2 = NULL, *closable = NULL, *opt = NULL, *opt_b = NULL, *opt2 = NULL, *opt3 = NULL, *statics = NULL;
    void *voice = (void *)0x1, *op = NULL;
    HSTRING text = hs(L"hi");
    BOOLEAN b = 2;
    int e = -1;
    double d;
    HRESULT hr;

    if (!f) { check(0, "synthesizer factory"); return; }
    hr = IActivationFactory_ActivateInstance(f, &obj);
    if (FAILED(hr)) { check(0, "SpeechSynthesizer activates"); return; }
    IInspectable_QueryInterface(obj, &IID_ISpeechSynthesizer, &synth);
    IInspectable_QueryInterface(obj, &IID_ISpeechSynthesizer2, &synth2);
    IInspectable_QueryInterface(obj, &IID_IClosable, &closable);
    IInspectable_Release(obj);

    hr = (HRESULT)VC(synth2, 6, &opt);
    CHECKF(hr == S_OK && opt, "Options (%#lx)", hr);
    VC(synth2, 6, &opt_b);
    CHECKF(opt == opt_b, "Options is the same object every time");
    if (opt_b) UREL(opt_b);
    if (opt)
    {
        check_inspectable(opt, &options_case);
        IInspectable_QueryInterface(opt, &IID_ISpeechSynthesizerOptions2, &opt2);
        IInspectable_QueryInterface(opt, &IID_ISpeechSynthesizerOptions3, &opt3);
        CHECKF(opt2 && opt3, "Options answers Options2 and Options3");
        b = 2; VC(opt, 6, &b);
        CHECKF(b == FALSE, "IncludeWordBoundaryMetadata starts FALSE");
        b = 2; VC(opt, 8, &b);
        CHECKF(b == FALSE, "IncludeSentenceBoundaryMetadata starts FALSE");
        VC(opt, 7, TRUE); VC(opt, 9, TRUE);
        b = 2; VC(opt, 6, &b);
        CHECKF(b == TRUE, "IncludeWordBoundaryMetadata is read back");
        b = 2; VC(opt, 8, &b);
        CHECKF(b == TRUE, "IncludeSentenceBoundaryMetadata is read back");
        if (opt2)
        {
            d = -1; VC(opt2, 6, &d);
            CHECKF(d == 1.0, "AudioVolume starts at 1 (%f)", d);
            d = -1; VC(opt2, 8, &d);
            CHECKF(d == 1.0, "SpeakingRate starts at 1 (%f)", d);
            d = -1; VC(opt2, 10, &d);
            CHECKF(d == 1.0, "AudioPitch starts at 1 (%f)", d);
            hr = DBL_PUT(opt2, 7, 0.5);
            d = -1; VC(opt2, 6, &d);
            CHECKF(hr == S_OK && d == 0.5, "AudioVolume is read back (%#lx %f)", hr, d);
            hr = DBL_PUT(opt2, 7, 1.5);
            d = -1; VC(opt2, 6, &d);
            CHECKF(hr == E_INVALIDARG && d == 0.5, "AudioVolume 1.5 is E_INVALIDARG and ignored (%#lx %f)", hr, d);
            hr = DBL_PUT(opt2, 9, 6.0);
            d = -1; VC(opt2, 8, &d);
            CHECKF(hr == S_OK && d == 6.0, "SpeakingRate 6 is accepted (%#lx %f)", hr, d);
            hr = DBL_PUT(opt2, 9, 6.5);
            CHECKF(hr == E_INVALIDARG, "SpeakingRate 6.5 is E_INVALIDARG (%#lx)", hr);
            hr = DBL_PUT(opt2, 9, 0.25);
            CHECKF(hr == E_INVALIDARG, "SpeakingRate 0.25 is E_INVALIDARG (%#lx)", hr);
            hr = DBL_PUT(opt2, 11, 2.0);
            CHECKF(hr == S_OK, "AudioPitch 2 is accepted (%#lx)", hr);
            hr = DBL_PUT(opt2, 11, -0.5);
            CHECKF(hr == E_INVALIDARG, "AudioPitch -0.5 is E_INVALIDARG (%#lx)", hr);
            UREL(opt2);
        }
        if (opt3)
        {
            e = -1; VC(opt3, 6, &e);
            CHECKF(e == 0, "AppendedSilence starts Default (%d)", e);
            e = -1; VC(opt3, 8, &e);
            CHECKF(e == 0, "PunctuationSilence starts Default (%d)", e);
            hr = (HRESULT)VC(opt3, 7, 1);
            e = -1; VC(opt3, 6, &e);
            CHECKF(hr == S_OK && e == 1, "AppendedSilence Min is read back (%#lx %d)", hr, e);
            hr = (HRESULT)VC(opt3, 9, 2);
            CHECKF(hr == E_INVALIDARG, "PunctuationSilence 2 is E_INVALIDARG (%#lx)", hr);
            UREL(opt3);
        }
        UREL(opt);
    }

    /* voices: there is none installed */
    voice = (void *)0x1;
    hr = (HRESULT)VC(synth, 9, &voice);
    CHECKF(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND) && !voice, "Voice with no voice chosen or installed (%#lx %p)", hr, voice);
    hr = (HRESULT)VC(synth, 8, NULL);
    CHECKF(hr == E_INVALIDARG, "put_Voice(NULL) is E_INVALIDARG (%#lx)", hr);
    hr = (HRESULT)VC(synth, 8, &fake_voice);
    CHECKF(hr == S_OK && fake_voice.ref == 2, "put_Voice keeps a reference (%#lx, ref %ld)", hr, fake_voice.ref);
    voice = NULL;
    hr = (HRESULT)VC(synth, 9, &voice);
    CHECKF(hr == S_OK && voice == &fake_voice && fake_voice.ref == 3, "get_Voice returns the voice that was set (%#lx %p ref %ld)", hr, voice, fake_voice.ref);
    if (voice) h_release(voice);
    IActivationFactory_QueryInterface(f, &IID_IInstalledVoicesStatic, &statics);
    voice = (void *)0x1;
    hr = (HRESULT)VC(statics, 7, &voice);
    CHECKF(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND) && !voice, "DefaultVoice with no voice installed (%#lx %p)", hr, voice);
    UREL(statics);

    /* Close */
    hr = (HRESULT)VC(closable, 6);
    CHECKF(hr == S_OK, "Close (%#lx)", hr);
    hr = (HRESULT)VC(closable, 6);
    CHECKF(hr == S_OK, "Close twice (%#lx)", hr);
    op = (void *)0x1;
    hr = (HRESULT)VC(synth, 6, text, &op);
    CHECKF(hr == RO_E_CLOSED && !op, "SynthesizeTextToStreamAsync after Close is RO_E_CLOSED (%#lx %p)", hr, op);
    opt = (void *)0x1;
    hr = (HRESULT)VC(synth2, 6, &opt);
    CHECKF(hr == RO_E_CLOSED && !opt, "Options after Close is RO_E_CLOSED (%#lx %p)", hr, opt);
    CHECKF(fake_voice.ref == 2, "the chosen voice is still held until the synthesizer is freed (ref %ld)", fake_voice.ref);

    UREL(synth); UREL(synth2); UREL(closable);
    CHECKF(fake_voice.ref == 1, "freeing the synthesizer releases the chosen voice (ref %ld)", fake_voice.ref);
    IActivationFactory_Release(f);
    WindowsDeleteString(text);
}

int main(void)
{
    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(hr)) { printf("FAIL  RoInitialize %#lx\n", hr); return 1; }
    test_recognizer();
    test_synthesizer();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
