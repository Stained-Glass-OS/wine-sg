/* windows.ui: scheduled toasts kept across processes: add | list | remove */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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


int main(int argc, char **argv)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    HRESULT (WINAPI *ro_init)(int) = (void *)GetProcAddress(combase, "RoInitialize");
    RAI activate = (RAI)GetProcAddress(combase, "RoActivateInstance");
    void *manager = NULL, *notifier = NULL, *sfactory = NULL, *vec = NULL, *x = NULL, *item = NULL;
    const char *mode = argc > 1 ? argv[1] : "list";
    HRESULT hr;
    UINT32 n = 0;
    FILETIME ft;
    INT64 now;

    setvbuf(stdout, NULL, _IONBF, 0);
    create = (WCS)GetProcAddress(combase, "WindowsCreateString");
    raw = (WGSRB)GetProcAddress(combase, "WindowsGetStringRawBuffer");
    get_factory = (RGAF)GetProcAddress(combase, "RoGetActivationFactory");
    ro_init(1);

    hr = get_factory(mk(L"Windows.UI.Notifications.ToastNotificationManager"), &IID_IToastNotificationManagerStatics_, &manager);
    CHECK(hr == S_OK && manager, "ToastNotificationManager %lx", hr);
    hr = CALL(manager, 7, HRESULT, mk(L"windows.ui.persist"), &notifier);
    CHECK(hr == S_OK && notifier, "notifier %lx", hr);
    if (!notifier) return 1;

    if (!strcmp(mode, "add"))
    {
        void *doc = NULL, *io = NULL;
        int secs = argc > 2 ? atoi(argv[2]) : 4;
        WCHAR xml[256];

        hr = get_factory(mk(L"Windows.UI.Notifications.ScheduledToastNotification"), &IID_IScheduledToastNotificationFactory_, &sfactory);
        CHECK(hr == S_OK && sfactory, "ScheduledToastNotification factory %lx", hr);
        activate(mk(L"Windows.Data.Xml.Dom.XmlDocument"), &x);
        CALL(x, 0, HRESULT, &IID_IXmlDocument_, &doc);
        CALL(x, 0, HRESULT, &IID_IXmlDocumentIO_, &io);
        swprintf(xml, 256, L"<toast><visual><binding template=\"ToastGeneric\"><text>persisted toast %d</text></binding></visual></toast>", secs);
        CALL(io, 6, HRESULT, mk(xml));
        GetSystemTimeAsFileTime(&ft);
        now = ((INT64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
        hr = CALL(sfactory, 6, HRESULT, doc, now + secs * 10000000LL, &item);
        CHECK(hr == S_OK && item, "Create %lx", hr);
        hr = CALL(notifier, 9, HRESULT, item);
        CHECK(hr == S_OK, "AddToSchedule %lx", hr);
        printf("added %d\n", secs);
        ExitProcess(fails ? 1 : 0);        /* the program ends with the toast still scheduled */
    }

    hr = CALL(notifier, 11, HRESULT, &vec);
    CHECK(hr == S_OK && vec, "GetScheduledToastNotifications %lx", hr);
    if (vec) CALL(vec, 7, HRESULT, &n);
    printf("COUNT %u\n", n);
    if (!strcmp(mode, "remove") && n)
    {
        hr = CALL(vec, 6, HRESULT, 0, &item);
        CHECK(hr == S_OK && item, "GetAt %lx", hr);
        hr = CALL(notifier, 10, HRESULT, item);
        CHECK(hr == S_OK, "RemoveFromSchedule %lx", hr);
        vec = NULL;
        CALL(notifier, 11, HRESULT, &vec);
        n = 99;
        if (vec) CALL(vec, 7, HRESULT, &n);
        CHECK(n == 0, "count after removal %u", n);
        printf("COUNT %u\n", n);
    }
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
