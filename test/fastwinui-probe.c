/* windows.ui: XmlDocument namespaces / processing instructions / doctype, the toast schedule */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>

typedef void *HS;
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define VT(obj) (*(void ***)(obj))
#define CALL(obj, idx, ret, ...) (((ret (WINAPI *)(void *, ...))VT(obj)[idx])(obj, ##__VA_ARGS__))

typedef HRESULT (WINAPI *WCS)(const WCHAR *, UINT32, HS *);
typedef HRESULT (WINAPI *RGAF)(HS, const GUID *, void **);
typedef HRESULT (WINAPI *RAI)(HS, void **);
typedef const WCHAR *(WINAPI *WGSRB)(HS, UINT32 *);
static WCS create;
static WGSRB raw;
static RGAF get_factory;

#ifndef E_BOUNDS
#define E_BOUNDS ((HRESULT)0x8000000BL)
#endif
#define G(name, a, b, c, d0,d1,d2,d3,d4,d5,d6,d7) static const GUID name = {a, b, c, {d0,d1,d2,d3,d4,d5,d6,d7}}
G(IID_IXmlDocument_, 0xf7f3a506, 0x1e87, 0x42d6, 0xbc,0xfb,0xb8,0xc8,0x09,0xfa,0x54,0x94);
G(IID_IXmlDocumentIO_, 0x6cd0e74e, 0xee65, 0x4489, 0x9e,0xbf,0xca,0x43,0xe8,0x7b,0xa6,0x37);
G(IID_IXmlNodeSelector_, 0x63dbba8b, 0xd0db, 0x4fe1, 0xb7,0x45,0xf9,0x43,0x3a,0xfd,0xc2,0x5b);
G(IID_IXmlProcessingInstruction_, 0x2707fd1e, 0x1e92, 0x4ece, 0xb6,0xf4,0x26,0xf0,0x69,0x07,0x8d,0xdc);
G(IID_IXmlDocumentType_, 0xf7342425, 0x9781, 0x4964, 0x8e,0x94,0x9b,0x1c,0x6d,0xfc,0x9b,0xc7);
G(IID_IXmlNodeList_, 0x8c60ad77, 0x83a4, 0x4ec1, 0x9c,0x54,0x7b,0xa4,0x29,0xe1,0x3d,0xa6);
G(IID_IPropertyValueStatics_, 0x629bdbc8, 0xd932, 0x4ff4, 0x96,0xb9,0x8d,0x96,0xc5,0xc1,0xe8,0x58);
G(IID_IToastNotificationManagerStatics_, 0x50ac103f, 0xd235, 0x4598, 0xbb,0xef,0x98,0xfe,0x4d,0x1a,0x3a,0xd4);
G(IID_IScheduledToastNotificationFactory_, 0xe7bed191, 0x0bb9, 0x4189, 0x83,0x94,0x31,0x76,0x1b,0x47,0x6f,0xd7);

static HS mk(const WCHAR *s) { HS h = NULL; create(s, wcslen(s), &h); return h; }
static const WCHAR *str(HS h) { return h ? raw(h, NULL) : L""; }
static void *prop_statics;
static void *box(const WCHAR *s) { void *v = NULL; CALL(prop_statics, 18, HRESULT, mk(s), &v); return v; }

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    HRESULT (WINAPI *ro_init)(int) = (void *)GetProcAddress(combase, "RoInitialize");
    RAI activate = (RAI)GetProcAddress(combase, "RoActivateInstance");
    void *inspectable = NULL, *doc = NULL, *io = NULL, *sel = NULL, *list = NULL, *root = NULL, *x;
    void *ns, *doctype = NULL, *pi = NULL, *impl = NULL, *manager = NULL, *notifier = NULL;
    void *sfactory = NULL, *sched = NULL, *vec = NULL;
    HS s = NULL;
    HRESULT hr;
    UINT32 n;
    IUnknown *u;
    FILETIME ft;
    INT64 now, when[2];
    int i;

    setvbuf(stdout, NULL, _IONBF, 0);
    create = (WCS)GetProcAddress(combase, "WindowsCreateString");
    raw = (WGSRB)GetProcAddress(combase, "WindowsGetStringRawBuffer");
    get_factory = (RGAF)GetProcAddress(combase, "RoGetActivationFactory");
    ro_init(1);

    hr = get_factory(mk(L"Windows.Foundation.PropertyValue"), &IID_IPropertyValueStatics_, &prop_statics);
    CHECK(hr == S_OK && prop_statics, "PropertyValue %lx", hr);
    hr = activate(mk(L"Windows.Data.Xml.Dom.XmlDocument"), &inspectable);
    CHECK(hr == S_OK && inspectable, "XmlDocument %lx", hr);
    if (!inspectable || !prop_statics) return 1;
    CALL(inspectable, 0, HRESULT, &IID_IXmlDocument_, &doc);
    CALL(inspectable, 0, HRESULT, &IID_IXmlDocumentIO_, &io);
    CALL(inspectable, 0, HRESULT, &IID_IXmlNodeSelector_, &sel);
    CHECK(doc && io && sel, "document interfaces");

    /* settings-less LoadXml and namespace selection */
    hr = CALL(io, 6, HRESULT, mk(L"<r xmlns:a=\"urn:a\"><a:x/><a:x/><y/></r>"));
    CHECK(hr == S_OK, "LoadXml %lx", hr);
    ns = box(L"xmlns:p=\"urn:a\"");
    hr = CALL(sel, 9, HRESULT, mk(L"//p:x"), ns, &list);
    CHECK(hr == S_OK && list, "SelectNodesNS %lx", hr);
    n = 0;
    if (list) CALL(list, 6, HRESULT, &n);
    CHECK(n == 2, "NS selection found %u", n);

    /* processing instruction, entity reference, implementation, doctype (none here) */
    hr = CALL(doc, 13, HRESULT, mk(L"app"), mk(L"x=\"1\""), &pi);
    CHECK(hr == S_OK && pi, "CreateProcessingInstruction %lx", hr);
    if (pi)
    {
        void *pii = NULL;
        CALL(pi, 0, HRESULT, &IID_IXmlProcessingInstruction_, &pii);
        CHECK(pii != NULL, "PI interface");
        if (pii)
        {
            s = NULL; CALL(pii, 6, HRESULT, &s);
            CHECK(!wcscmp(str(s), L"app"), "Target %ls", str(s));
            s = NULL; CALL(pii, 7, HRESULT, &s);
            CHECK(!wcscmp(str(s), L"x=\"1\""), "Data %ls", str(s));
        }
    }
    hr = CALL(doc, 7, HRESULT, &impl);
    CHECK(hr == S_OK && impl, "Implementation %lx", hr);
    if (impl)
    {
        BYTE has = 0;
        hr = CALL(impl, 6, HRESULT, mk(L"XML"), box(L"1.0"), &has);
        CHECK(hr == S_OK && has, "HasFeature %lx", hr);
    }
    hr = CALL(doc, 21, HRESULT, mk(L"id1"), &x);
    CHECK(hr == S_OK && !x, "GetElementById %lx", hr);

    /* a scheduled toast: refused in the past, listed and removed in the future */
    hr = get_factory(mk(L"Windows.UI.Notifications.ToastNotificationManager"), &IID_IToastNotificationManagerStatics_, &manager);
    CHECK(hr == S_OK && manager, "ToastNotificationManager %lx", hr);
    hr = CALL(manager, 7, HRESULT, mk(L"windows.ui.gate"), &notifier);
    CHECK(hr == S_OK && notifier, "notifier %lx", hr);
    hr = get_factory(mk(L"Windows.UI.Notifications.ScheduledToastNotification"), &IID_IScheduledToastNotificationFactory_, &sfactory);
    CHECK(hr == S_OK && sfactory, "ScheduledToastNotification factory %lx", hr);
    if (notifier && sfactory)
    {
        void *toast_doc = NULL, *toast_io = NULL, *items[2] = {0}, *past = NULL;
        activate(mk(L"Windows.Data.Xml.Dom.XmlDocument"), &x);
        CALL(x, 0, HRESULT, &IID_IXmlDocument_, &toast_doc);
        CALL(x, 0, HRESULT, &IID_IXmlDocumentIO_, &toast_io);
        CALL(toast_io, 6, HRESULT, mk(L"<toast><visual><binding template=\"ToastGeneric\"><text>t</text></binding></visual></toast>"));
        GetSystemTimeAsFileTime(&ft);
        now = ((INT64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
        when[0] = now + 3600 * 10000000LL;
        when[1] = now + 7200 * 10000000LL;
        for (i = 0; i < 2; i++)
        {
            hr = CALL(sfactory, 6, HRESULT, toast_doc, when[i], &items[i]);
            CHECK(hr == S_OK && items[i], "CreateScheduledToastNotification %lx", hr);
        }
        hr = CALL(sfactory, 6, HRESULT, toast_doc, now - 10000000LL, &past);
        hr = CALL(notifier, 9, HRESULT, past);
        CHECK(hr == E_INVALIDARG, "AddToSchedule in the past %lx", hr);
        for (i = 0; i < 2; i++)
        {
            hr = CALL(notifier, 9, HRESULT, items[i]);
            CHECK(hr == S_OK, "AddToSchedule %lx", hr);
        }
        hr = CALL(notifier, 11, HRESULT, &vec);
        CHECK(hr == S_OK && vec, "GetScheduledToastNotifications %lx", hr);
        n = 0;
        if (vec) CALL(vec, 7, HRESULT, &n);
        CHECK(n == 2, "scheduled count %u", n);
        hr = CALL(notifier, 10, HRESULT, items[0]);
        CHECK(hr == S_OK, "RemoveFromSchedule %lx", hr);
        vec = NULL;
        CALL(notifier, 11, HRESULT, &vec);
        n = 0;
        if (vec) CALL(vec, 7, HRESULT, &n);
        CHECK(n == 1, "scheduled count after removal %u", n);
        CALL(notifier, 10, HRESULT, items[1]);
    }

    u = inspectable;
    (void)u;
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
