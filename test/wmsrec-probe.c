/* wmsrec-probe: Windows.Media.SpeechRecognition's SpeechRecognizer hears speech through the system's engine
 * (wine-sg 8206), for test/wmsrec-gate.sh. The gate hands sg-dictate a recording of "open the door" as its
 * microphone (SG_DICTATE_AUDIO_FILE). Prints KEY=VALUE.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
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
DEFINE_GUID(IID_ISpeechRecognizer, 0x0bc3c9cb, 0xc26a, 0x40f2, 0xae,0xb5, 0x80,0x96,0xb2,0xe4,0x80,0x73);
DEFINE_GUID(IID_ISpeechRecognitionListConstraintFactory, 0x40f3cdc7, 0x562a, 0x426a, 0x9f,0x3b, 0x3b,0x4e,0x28,0x2b,0xe1,0xd5);
DEFINE_GUID(IID_ISpeechRecognitionConstraint, 0x79ac1628, 0x4d68, 0x43c4, 0x89,0x11, 0x40,0xdc,0x41,0x01,0xb5,0x5b);

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


static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, wcslen(s), &h); return h; }

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


static int wait_async(void *op)
{
    void *info = NULL;
    int i, s = 0;
    if (FAILED(IInspectable_QueryInterface(op, &IID_IAsyncInfo, &info))) return 0;
    for (i = 0; i < 1200; i++)   /* the engine loads its model first: up to two minutes */
    {
        s = 0;
        VC(info, 7, &s);
        if (s != 0) break;
        Sleep(100);
    }
    UREL(info);
    return s;
}

/* RecognizeAsync: "status confidence text constraint" */
static void recognize(const char *tag, void *rec)
{
    void *op = NULL, *res = NULL, *con = NULL;
    HSTRING str = NULL;
    int st = -1, conf = -1, async;
    char text[256] = "";
    HRESULT hr = (HRESULT)VC(rec, 11, &op);

    if (FAILED(hr) || !op) { printf("%s=fail %#lx\n", tag, hr); return; }
    async = wait_async(op);
    hr = (HRESULT)VC(op, 8, &res);
    if (FAILED(hr) || !res) { printf("%s=noresult %d %#lx\n", tag, async, hr); UREL(op); return; }
    VC(res, 6, &st);
    VC(res, 7, &str);
    VC(res, 8, &conf);
    VC(res, 11, &con);
    WideCharToMultiByte(CP_UTF8, 0, WindowsGetStringRawBuffer(str, NULL), -1, text, sizeof(text), NULL, NULL);
    printf("%s=%d %d [%s] %d\n", tag, st, conf, text, con != NULL);
    WindowsDeleteString(str);
    if (con) UREL(con);
    UREL(res);
    UREL(op);
}

static void *list_constraint(HSTRING *items, UINT32 count)
{
    IActivationFactory *f = get_factory(L"Windows.Media.SpeechRecognition.SpeechRecognitionListConstraint");
    struct strings src = { iterable_vtbl, 1, items, count, 0 };
    void *cf = NULL, *list = NULL, *con = NULL;

    if (!f || FAILED(IActivationFactory_QueryInterface(f, &IID_ISpeechRecognitionListConstraintFactory, &cf))) return NULL;
    VC(cf, 6, &src, &list);   /* Create */
    if (list) IInspectable_QueryInterface(list, &IID_ISpeechRecognitionConstraint, &con);
    if (list) UREL(list);
    UREL(cf);
    IActivationFactory_Release(f);
    return con;
}

int main(void)
{
    IActivationFactory *f;
    IInspectable *obj = NULL;
    void *rec = NULL, *constraints = NULL, *con;
    HSTRING match[2] = { hs(L"Close the window"), hs(L"open the door") }, other[1] = { hs(L"close the window") };
    HRESULT hr;

    RoInitialize(RO_INIT_MULTITHREADED);
    if (!(f = get_factory(L"Windows.Media.SpeechRecognition.SpeechRecognizer"))) { printf("factory=0\n"); return 1; }
    hr = IActivationFactory_ActivateInstance(f, &obj);
    printf("recognizer=%#lx\n", hr);
    if (FAILED(hr)) return 1;
    IInspectable_QueryInterface(obj, &IID_ISpeechRecognizer, &rec);

    /* dictation: what was said */
    recognize("dictation", rec);

    /* a list of phrases, one of them said: that phrase, its constraint */
    VC(rec, 7, &constraints);   /* get_Constraints: IVector<ISpeechRecognitionConstraint> */
    con = list_constraint(match, 2);
    VC(constraints, 13, con);   /* Append */
    recognize("list", rec);
    VC(constraints, 12, 0);     /* RemoveAt */
    UREL(con);

    /* a list without what was said: rejected */
    con = list_constraint(other, 1);
    VC(constraints, 13, con);
    recognize("rejected", rec);
    UREL(con);
    UREL(constraints);
    UREL(rec);
    printf("done=1\n");
    return 0;
}
