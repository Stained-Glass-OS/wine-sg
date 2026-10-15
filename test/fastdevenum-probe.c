/* windows.devices.enumeration: DeviceInformation, its collection, the watcher, access information */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>

typedef void *HS;
#define E_BOUNDS ((HRESULT)0x8000000BL)
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define VT(obj) (*(void ***)(obj))
#define CALL(obj, idx, ret, ...) (((ret (WINAPI *)(void *, ...))VT(obj)[idx])(obj, ##__VA_ARGS__))

typedef HRESULT (WINAPI *WCS)(const WCHAR *, UINT32, HS *);
typedef HRESULT (WINAPI *RGAF)(HS, const GUID *, void **);
typedef const WCHAR *(WINAPI *WGSRB)(HS, UINT32 *);
static WCS create;
static WGSRB raw;
static RGAF get_factory;

#define G(name, a, b, c, d0,d1,d2,d3,d4,d5,d6,d7) static const GUID name = {a, b, c, {d0,d1,d2,d3,d4,d5,d6,d7}}
G(IID_IDeviceInformationStatics_, 0xc17f100e, 0x3a46, 0x4a78, 0x80,0x13,0x76,0x9d,0xc9,0xb9,0x73,0x90);
G(IID_IDeviceInformationStatics2_, 0x493b4f34, 0xa84f, 0x45fd, 0x91,0x67,0x15,0xd1,0xcb,0x1b,0xd1,0xf9);
G(IID_IDeviceAccessInformationStatics_, 0x574bd3d3, 0x5f30, 0x45cd, 0x8a,0x94,0x72,0x4f,0xe5,0x97,0x30,0x84);

static HS mk(const WCHAR *s) { HS h = NULL; create(s, wcslen(s), &h); return h; }
static const WCHAR *str(HS h) { return h ? raw(h, NULL) : L"(null)"; }

/* a typed event handler: Invoke(sender, args) counts and sets the event */
struct handler { void *vtbl; LONG ref; LONG count; HANDLE event; };
static HRESULT WINAPI h_qi(void *This, const GUID *iid, void **out) { *out = This; return S_OK; }
static ULONG WINAPI h_addref(void *This) { return InterlockedIncrement(&((struct handler *)This)->ref); }
static ULONG WINAPI h_release(void *This) { return InterlockedDecrement(&((struct handler *)This)->ref); }
static HRESULT WINAPI h_invoke(void *This, void *sender, void *args)
{
    struct handler *h = This;
    InterlockedIncrement(&h->count);
    if (h->event) SetEvent(h->event);
    return S_OK;
}
static void *handler_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

/* the result of an operation that has finished */
static HRESULT result(void *op, void **out)
{
    HRESULT hr = CALL(op, 8, HRESULT, out);
    CALL(op, 2, ULONG);
    return hr;
}

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    HRESULT (WINAPI *ro_init)(int) = (void *)GetProcAddress(combase, "RoInitialize");
    void *factory = NULL, *statics = NULL, *statics2 = NULL, *op = NULL, *coll = NULL, *info = NULL, *watcher = NULL;
    void *access_statics = NULL, *access = NULL, *props = NULL;
    HS s = NULL;
    HRESULT hr;
    UINT32 size = 99, i, status;
    struct { INT64 value; } token = {0};
    struct handler added = { handler_vtbl, 1, 0, NULL }, completed = { handler_vtbl, 1, 0, NULL }, stopped = { handler_vtbl, 1, 0, NULL };
    int reg;

    setvbuf(stdout, NULL, _IONBF, 0);
    create = (WCS)GetProcAddress(combase, "WindowsCreateString");
    raw = (WGSRB)GetProcAddress(combase, "WindowsGetStringRawBuffer");
    get_factory = (RGAF)GetProcAddress(combase, "RoGetActivationFactory");
    ro_init(1);
    completed.event = CreateEventW(NULL, FALSE, FALSE, NULL);
    stopped.event = CreateEventW(NULL, FALSE, FALSE, NULL);

    hr = get_factory(mk(L"Windows.Devices.Enumeration.DeviceInformation"), &IID_IDeviceInformationStatics_, &statics);
    CHECK(hr == S_OK && statics, "statics %lx", hr);
    hr = get_factory(mk(L"Windows.Devices.Enumeration.DeviceInformation"), &IID_IDeviceInformationStatics2_, &statics2);
    CHECK(hr == S_OK && statics2, "statics2 %lx", hr);
    if (!statics || !statics2) return 1;

    /* the AQS filters */
    hr = CALL(statics2, 6, HRESULT, 2 /* AudioRender */, &s);
    CHECK(hr == S_OK && wcsstr(str(s), L"{e6327cad-dcec-4949-ae8a-991e976a79d2}") &&
          wcsstr(str(s), L"System.Devices.InterfaceEnabled"), "AqsFilter(AudioRender): %lx %ls", hr, str(s));
    s = NULL;
    hr = CALL(statics2, 6, HRESULT, 0, &s);
    CHECK(hr == S_OK && !s, "AqsFilter(All): %lx %ls", hr, str(s));
    hr = CALL(statics2, 6, HRESULT, 77, &s);
    CHECK(hr == E_INVALIDARG, "AqsFilter(77): %lx", hr);

    /* a class with no devices here, and an invalid class */
    hr = CALL(statics, 9, HRESULT, 5 /* ImageScanner */, &op);
    CHECK(hr == S_OK && op, "FindAllAsync(ImageScanner) %lx", hr);
    coll = NULL;
    hr = op ? result(op, &coll) : E_FAIL;
    CHECK(hr == S_OK && coll, "ImageScanner result %lx", hr);
    size = 99;
    if (coll) { CALL(coll, 7, HRESULT, &size); CALL(coll, 2, ULONG); }
    CHECK(size == 0, "ImageScanner count %u", size);
    hr = CALL(statics, 9, HRESULT, 77, &op);
    CHECK(hr == E_INVALIDARG, "FindAllAsync(77) %lx", hr);

    /* the render devices: whatever there are, they hold together */
    hr = CALL(statics, 9, HRESULT, 2, &op);
    CHECK(hr == S_OK && op, "FindAllAsync(AudioRender) %lx", hr);
    coll = NULL;
    hr = op ? result(op, &coll) : E_FAIL;
    CHECK(hr == S_OK && coll, "AudioRender result %lx", hr);
    size = 0;
    if (coll)
    {
        CALL(coll, 7, HRESULT, &size);
        for (i = 0; i < size; i++)
        {
            info = NULL;
            hr = CALL(coll, 6, HRESULT, i, &info);
            CHECK(hr == S_OK && info, "GetAt(%u) %lx", i, hr);
            if (!info) continue;
            s = NULL; CALL(info, 6, HRESULT, &s);
            CHECK(wcsstr(str(s), L"\\\\?\\SWD#MMDEVAPI#") == str(s) && wcsstr(str(s), L"#{e6327cad-dcec-4949-ae8a-991e976a79d2}"), "id %ls", str(s));
            s = NULL; CALL(info, 7, HRESULT, &s);
            CHECK(*str(s), "empty name");
            props = NULL;
            hr = CALL(info, 11, HRESULT, &props);
            CHECK(hr == S_OK && props, "Properties %lx", hr);
            if (props) { UINT32 n = 9; CALL(props, 7, HRESULT, &n); CHECK(n == 0, "properties size %u", n); CALL(props, 2, ULONG); }
            CALL(info, 2, ULONG);
        }
        info = (void *)1;
        hr = CALL(coll, 6, HRESULT, size, &info);
        CHECK(hr == E_BOUNDS && !info, "GetAt(size) %lx", hr);
        CALL(coll, 2, ULONG);
    }

    /* an identifier nothing has */
    hr = CALL(statics, 6, HRESULT, mk(L"\\\\?\\SWD#NOTHING#nothing"), &op);
    CHECK(hr == S_OK && op, "CreateFromIdAsync %lx", hr);
    info = NULL;
    hr = op ? result(op, &info) : E_FAIL;
    CHECK(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND) && !info, "CreateFromIdAsync(unknown) %lx", hr);
    hr = CALL(statics, 6, HRESULT, NULL, &op);
    CHECK(hr == E_INVALIDARG, "CreateFromIdAsync(null) %lx", hr);

    /* the watcher reports the same devices, completes, and stops */
    hr = CALL(statics, 13, HRESULT, 2, &watcher);
    CHECK(hr == S_OK && watcher, "CreateWatcher(AudioRender) %lx", hr);
    if (watcher)
    {
        CALL(watcher, 6, HRESULT, &added, &token);
        CALL(watcher, 12, HRESULT, &completed, &token);
        CALL(watcher, 14, HRESULT, &stopped, &token);
        status = 99; CALL(watcher, 16, HRESULT, &status);
        CHECK(status == 0, "status before start %u", status);
        hr = CALL(watcher, 18, HRESULT);
        CHECK(hr == HRESULT_FROM_WIN32(ERROR_INVALID_STATE), "Stop before Start %lx", hr);
        hr = CALL(watcher, 17, HRESULT);
        CHECK(hr == S_OK, "Start %lx", hr);
        hr = CALL(watcher, 17, HRESULT);
        CHECK(hr == HRESULT_FROM_WIN32(ERROR_INVALID_STATE), "second Start %lx", hr);
        reg = WaitForSingleObject(completed.event, 5000);
        CHECK(reg == WAIT_OBJECT_0, "EnumerationCompleted not raised");
        CHECK(added.count == (LONG)size, "Added %ld times, %u devices", added.count, size);
        status = 99; CALL(watcher, 16, HRESULT, &status);
        CHECK(status == 2, "status after enumeration %u", status);
        hr = CALL(watcher, 18, HRESULT);
        CHECK(hr == S_OK, "Stop %lx", hr);
        CHECK(WaitForSingleObject(stopped.event, 5000) == WAIT_OBJECT_0, "Stopped not raised");
        status = 99; CALL(watcher, 16, HRESULT, &status);
        CHECK(status == 4, "status after stop %u", status);
        hr = CALL(watcher, 18, HRESULT);
        CHECK(hr == HRESULT_FROM_WIN32(ERROR_INVALID_STATE), "second Stop %lx", hr);
        CALL(watcher, 2, ULONG);
    }

    /* a watcher released while it is reporting must not crash */
    hr = CALL(statics, 14, HRESULT, mk(L"System.Devices.InterfaceClassGuid:=\"{2eef81be-33fa-4800-9670-1cd474972c3f}\""), &watcher);
    if (watcher)
    {
        CALL(watcher, 17, HRESULT);
        CALL(watcher, 2, ULONG);
        Sleep(200);
    }

    /* access information */
    hr = get_factory(mk(L"Windows.Devices.Enumeration.DeviceAccessInformation"), &IID_IDeviceAccessInformationStatics_, &access_statics);
    CHECK(hr == S_OK && access_statics, "access statics %lx", hr);
    if (access_statics)
    {
        hr = CALL(access_statics, 8, HRESULT, 2, &access);
        CHECK(hr == S_OK && access, "CreateFromDeviceClass %lx", hr);
        status = 99;
        if (access) { hr = CALL(access, 8, HRESULT, &status); CHECK(hr == S_OK && status == 1, "CurrentStatus %lx %u", hr, status); CALL(access, 2, ULONG); }
        hr = CALL(access_statics, 8, HRESULT, 77, &access);
        CHECK(hr == E_INVALIDARG, "CreateFromDeviceClass(77) %lx", hr);
    }
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails;
}
