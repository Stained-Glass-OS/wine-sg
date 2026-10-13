/* twinapi.appcore and windows.applicationmodel through the raw vtables */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>

#ifndef E_BOUNDS
#define E_BOUNDS ((HRESULT)0x8000000BL)
#endif
typedef void *HS;
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
G(IID_IPropertySet_, 0x8a43ed9f, 0xf4e6, 0x4421, 0xac,0xf9,0x1d,0xab,0x29,0x86,0x82,0x0c);
G(IID_IMap_, 0x1b0d3570, 0x0877, 0x5ec2, 0x8a,0x2c,0x3b,0x95,0x39,0x50,0x6a,0xca);
G(IID_IObservableMap_, 0x236aac9d, 0xfb12, 0x5c4d, 0xa4,0x1c,0x9e,0x44,0x5f,0xb4,0xd7,0xec);
G(IID_IMapView_, 0xbb78502a, 0xf79d, 0x54fa, 0x92,0xc9,0x90,0xc5,0x03,0x9f,0xdf,0x7e);
G(IID_IIterable_, 0xfe2f3d47, 0x5d47, 0x5499, 0x83,0x74,0x43,0x0c,0x7c,0xda,0x02,0x04);
G(IID_IMapChangedHandler_, 0x24f981e5, 0xddca, 0x538d, 0xaa,0xda,0xa5,0x99,0x06,0x08,0x4c,0xf1);
G(IID_IPackageStatics_, 0x4e534bdf, 0x2960, 0x4878, 0x97,0xa4,0x96,0x24,0xde,0xb7,0x2f,0x2d);
G(IID_IPackage_, 0x163c792f, 0xbd75, 0x413c, 0xbf,0x23,0xb1,0xfe,0x7b,0x95,0xd8,0x25);
G(IID_ICoreApplication_, 0x0aacf7a4, 0x5e1d, 0x49df, 0x80,0x34,0xfb,0x6a,0x68,0xbc,0x5e,0xd1);
G(IID_IAnalyticsInfoStatics_, 0x1d5ee066, 0x188d, 0x5ba9, 0x43,0x87,0xac,0xae,0xb0,0xe7,0xe3,0x05);
G(IID_IAdvertisingManagerStatics_, 0xadd3468c, 0xa273, 0x48cb, 0xb3,0x46,0x35,0x44,0x52,0x2d,0x55,0x81);
G(IID_IStorageItem_, 0x4207a996, 0xca2f, 0x42f7, 0xbd,0xe8,0x8b,0x10,0x45,0x7a,0x7f,0x30);
G(IID_IActivationFactory_, 0x00000035, 0x0000, 0x0000, 0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46);
G(IID_IEasClientDeviceInformation_, 0x54dfd981, 0x1968, 0x4ca3, 0xb9,0x58,0xe5,0x95,0xd1,0x65,0x05,0xeb);

static HS mk(const WCHAR *s) { HS h = NULL; create(s, wcslen(s), &h); return h; }
static const WCHAR *str(HS h) { return h ? raw(h, NULL) : L""; }

/* a MapChanged handler that records what it was told */
struct handler { void *vtbl; LONG ref; int calls; int change; WCHAR key[32]; void *sender; };
static HRESULT WINAPI h_qi(void *This, const GUID *iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IMapChangedHandler_) || IsEqualGUID(iid, &IID_IUnknown)) { *out = This; return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI h_addref(void *This) { return InterlockedIncrement(&((struct handler *)This)->ref); }
static ULONG WINAPI h_release(void *This) { return InterlockedDecrement(&((struct handler *)This)->ref); }
static HRESULT WINAPI h_invoke(void *This, void *sender, void *args)
{
    struct handler *h = This;
    HS key = NULL;
    int change = -1;
    CALL(args, 6, HRESULT, &change);
    CALL(args, 7, HRESULT, &key);
    h->calls++;
    h->change = change;
    h->sender = sender;
    lstrcpynW(h->key, str(key), 32);
    return S_OK;
}
static void *handler_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

int main(int argc, char **argv)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    HRESULT (WINAPI *ro_init)(int) = (void *)GetProcAddress(combase, "RoInitialize");
    void *factory = NULL, *obj = NULL, *tmp = NULL;
    HS s = NULL;
    HRESULT hr;
    ULONG n;
    IID *iids;
    void *reg = NULL;
    struct handler h = { handler_vtbl, 1 };

    create = (WCS)GetProcAddress(combase, "WindowsCreateString");
    raw = (WGSRB)GetProcAddress(combase, "WindowsGetStringRawBuffer");
    get_factory = (RGAF)GetProcAddress(combase, "RoGetActivationFactory");
    ro_init(1);

    /* ---- twinapi.appcore ---- */
    {
        HMODULE m = LoadLibraryA("twinapi.appcore.dll");
        ULONG (WINAPI *reg_fn)(void *, void *, void **) = (void *)GetProcAddress(m, "RegisterAppStateChangeNotification");
        void (WINAPI *unreg_fn)(void *) = (void *)GetProcAddress(m, "UnregisterAppStateChangeNotification");
        ULONG (WINAPI *creg_fn)(void *, void *, void **) = (void *)GetProcAddress(m, "RegisterAppConstrainedChangeNotification");
        void (WINAPI *cunreg_fn)(void *) = (void *)GetProcAddress(m, "UnregisterAppConstrainedChangeNotification");
        WCHAR cname[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD len = (sizeof(cname)/sizeof(WCHAR));

        CHECK(reg_fn && unreg_fn && creg_fn && cunreg_fn, "exports");
        CHECK(reg_fn(NULL, NULL, &reg) == ERROR_INVALID_PARAMETER, "null routine");
        CHECK(reg_fn((void *)main, NULL, &reg) == ERROR_SUCCESS && reg, "register");
        unreg_fn(reg);
        reg = NULL;
        CHECK(creg_fn((void *)main, NULL, &reg) == ERROR_SUCCESS && reg, "register constrained");
        cunreg_fn(reg);

        hr = get_factory(mk(L"Windows.Security.ExchangeActiveSyncProvisioning.EasClientDeviceInformation"),
                         &IID_IActivationFactory_, &factory);
        CHECK(hr == S_OK && factory, "Eas factory %lx", hr);
        n = 0; iids = NULL;
        hr = CALL(factory, 3, HRESULT, &n, &iids);
        CHECK(hr == S_OK && n == 1 && iids && IsEqualGUID(&iids[0], &IID_IActivationFactory_), "factory GetIids %lx n=%lu", hr, n);
        CoTaskMemFree(iids);
        hr = CALL(factory, 4, HRESULT, &s);
        CHECK(hr == S_OK && !wcscmp(str(s), L"Windows.Security.ExchangeActiveSyncProvisioning.EasClientDeviceInformation"),
              "factory class name %ls", str(s));
        hr = CALL(factory, 6, HRESULT, &obj);
        CHECK(hr == S_OK && obj, "activate %lx", hr);
        n = 0; iids = NULL;
        hr = CALL(obj, 3, HRESULT, &n, &iids);
        CHECK(hr == S_OK && n == 1 && IsEqualGUID(&iids[0], &IID_IEasClientDeviceInformation_), "object GetIids %lx", hr);
        CoTaskMemFree(iids);
        { int level = -1; hr = CALL(obj, 5, HRESULT, &level); CHECK(hr == S_OK && level == 0, "trust %lx %d", hr, level); }
        s = NULL; CALL(obj, 7, HRESULT, &s);
        CHECK(!wcscmp(str(s), L"Windows"), "OperatingSystem %ls", str(s));
        s = NULL; CALL(obj, 8, HRESULT, &s);
        GetComputerNameW(cname, &len);
        CHECK(!wcscmp(str(s), cname), "FriendlyName %ls vs %ls", str(s), cname);
        { GUID id; memset(&id, 0, sizeof(id)); hr = CALL(obj, 6, HRESULT, &id); CHECK(hr == S_OK, "Id %lx", hr); }
        CALL(obj, 2, ULONG);
        CALL(factory, 2, ULONG);

        factory = NULL;
        hr = get_factory(mk(L"Windows.System.Profile.AnalyticsInfo"), &IID_IAnalyticsInfoStatics_, &factory);
        CHECK(hr == S_OK && factory, "AnalyticsInfo %lx", hr);
        s = NULL; hr = CALL(factory, 7, HRESULT, &s);
        CHECK(hr == S_OK && !wcscmp(str(s), L"Desktop"), "DeviceForm %lx %ls", hr, str(s));
        CALL(factory, 2, ULONG);

        factory = NULL;
        hr = get_factory(mk(L"Windows.System.UserProfile.AdvertisingManager"), &IID_IAdvertisingManagerStatics_, &factory);
        CHECK(hr == S_OK && factory, "AdvertisingManager %lx", hr);
        s = NULL; hr = CALL(factory, 6, HRESULT, &s);
        CHECK(hr == S_OK && !*str(s), "AdvertisingId %lx", hr);
        CALL(factory, 2, ULONG);
    }

    /* ---- windows.applicationmodel ---- */
    {
        void *app = NULL, *props = NULL, *map = NULL, *omap = NULL, *view = NULL, *iterable = NULL, *iter = NULL;
        void *pair = NULL, *value = NULL, *package = NULL, *pid = NULL, *folder = NULL, *item = NULL, *deps = NULL, *cview = NULL;
        UINT32 count = 99, e;
        BOOLEAN b = 0;
        INT64 token = 0;
        int seen = 0;

        hr = get_factory(mk(L"Windows.ApplicationModel.Core.CoreApplication"), &IID_ICoreApplication_, &app);
        CHECK(hr == S_OK && app, "CoreApplication %lx", hr);
        hr = CALL(app, 11, HRESULT, &props);
        CHECK(hr == S_OK && props, "Properties %lx", hr);
        CALL(props, 0, HRESULT, &IID_IMap_, &map);
        CALL(props, 0, HRESULT, &IID_IObservableMap_, &omap);
        CHECK(map && omap, "map interfaces");
        n = 0; iids = NULL;
        hr = CALL(props, 3, HRESULT, &n, &iids);
        CHECK(hr == S_OK && n == 4, "PropertySet GetIids %lx n=%lu", hr, n);
        CoTaskMemFree(iids);

        hr = CALL(omap, 6, HRESULT, &h, &token);
        CHECK(hr == S_OK && token, "add_MapChanged %lx", hr);
        hr = CALL(map, 10, HRESULT, mk(L"one"), app, &b);   /* any inspectable as a value */
        CHECK(hr == S_OK && !b, "Insert %lx", hr);
        CHECK(h.calls == 1 && h.change == 1 && !wcscmp(h.key, L"one") && h.sender == omap, "inserted event calls=%d change=%d key=%ls", h.calls, h.change, h.key);
        hr = CALL(map, 10, HRESULT, mk(L"one"), props, &b);
        CHECK(hr == S_OK && b && h.calls == 2 && h.change == 3, "replace event calls=%d change=%d", h.calls, h.change);
        CALL(map, 10, HRESULT, mk(L"two"), app, &b);
        CHECK(h.calls == 3, "second insert");

        hr = CALL(map, 9, HRESULT, &view);
        CHECK(hr == S_OK && view, "GetView %lx", hr);
        CALL(view, 7, HRESULT, &count);
        CHECK(count == 2, "view size %u", count);
        CALL(view, 8, HRESULT, mk(L"two"), &b);
        CHECK(b, "view HasKey");
        value = NULL;
        hr = CALL(view, 6, HRESULT, mk(L"one"), &value);
        CHECK(hr == S_OK && value, "view Lookup %lx", hr);
        if (value) CALL(value, 2, ULONG);
        hr = CALL(view, 6, HRESULT, mk(L"absent"), &value);
        CHECK(hr == E_BOUNDS, "view Lookup absent %lx", hr);
        CALL(map, 11, HRESULT, mk(L"two"));
        CHECK(h.calls == 4 && h.change == 2 && !wcscmp(h.key, L"two"), "removed event");
        CALL(view, 7, HRESULT, &count);
        CHECK(count == 2, "view is a snapshot (%u)", count);

        hr = CALL(props, 0, HRESULT, &IID_IIterable_, &iterable);
        CHECK(hr == S_OK && iterable, "iterable %lx", hr);
        hr = CALL(iterable, 6, HRESULT, &iter);
        CHECK(hr == S_OK && iter, "First %lx", hr);
        CALL(iter, 7, HRESULT, &b);
        while (b && seen < 5)
        {
            pair = NULL; s = NULL;
            CALL(iter, 6, HRESULT, &pair);
            CALL(pair, 6, HRESULT, &s);
            CHECK(!wcscmp(str(s), L"one"), "pair key %ls", str(s));
            CALL(pair, 2, ULONG);
            seen++;
            CALL(iter, 8, HRESULT, &b);
        }
        CHECK(seen == 1, "iterated %d", seen);
        CALL(iter, 2, ULONG);
        CALL(iterable, 2, ULONG);

        hr = CALL(omap, 7, HRESULT, token);
        CALL(map, 12, HRESULT);
        CHECK(h.calls == 4, "no event after remove_MapChanged (%d)", h.calls);
        CALL(view, 2, ULONG);

        /* Package */
        factory = NULL;
        hr = get_factory(mk(L"Windows.ApplicationModel.Package"), &IID_IPackageStatics_, &factory);
        CHECK(hr == S_OK && factory, "Package statics %lx", hr);
        hr = CALL(factory, 6, HRESULT, &package);
        if (argc < 2)
        {
            /* no package identity outside an installed package */
            CHECK(hr == HRESULT_FROM_WIN32(15700) && !package, "Package.Current unpackaged %lx", hr);
            goto no_package;
        }
        CHECK(hr == S_OK && package, "Package.Current %lx", hr);
        hr = CALL(package, 6, HRESULT, &pid);
        CHECK(hr == S_OK && pid, "Package.Id %lx", hr);
        {
            s = NULL; CALL(pid, 6, HRESULT, &s);
            CHECK(!wcscmp(str(s), L"Fabrikam.Test"), "Id.Name %ls", str(s));
            s = NULL; CALL(pid, 13, HRESULT, &s);
            CHECK(!wcscmp(str(s), L"Fabrikam.Test_8wekyb3d8bbwe"), "FamilyName %ls", str(s));
            s = NULL; CALL(pid, 12, HRESULT, &s);
            CHECK(!wcscmp(str(s), L"Fabrikam.Test_1.2.3.4_x64__8wekyb3d8bbwe"), "FullName %ls", str(s));
            s = NULL; CALL(pid, 11, HRESULT, &s);
            CHECK(!wcscmp(str(s), L"8wekyb3d8bbwe"), "PublisherId %ls", str(s));
            s = NULL; CALL(pid, 9, HRESULT, &s);
            CHECK(!wcscmp(str(s), L""), "ResourceId %ls", str(s));
        }
        { int arch = -1; CALL(pid, 8, HRESULT, &arch); CHECK(arch == 9, "Architecture %d", arch); }
        { UINT16 ver[4] = {9, 9, 9, 9}; CALL(pid, 7, HRESULT, ver); CHECK(ver[0] == 1 && ver[1] == 2 && ver[2] == 3 && ver[3] == 4, "Version %d.%d.%d.%d", ver[0], ver[1], ver[2], ver[3]); }
        CALL(pid, 2, ULONG);

        b = TRUE;
        CALL(package, 8, HRESULT, &b);
        CHECK(!b, "IsFramework");
        hr = CALL(package, 9, HRESULT, &deps);
        CHECK(hr == S_OK && deps, "Dependencies %lx", hr);
        count = 99; CALL(deps, 7, HRESULT, &count);
        CHECK(count == 0, "no dependencies");
        hr = CALL(deps, 6, HRESULT, 0, &value);
        CHECK(hr == E_BOUNDS, "GetAt(0) %lx", hr);
        CALL(deps, 2, ULONG);

        hr = CALL(package, 7, HRESULT, &folder);
        CHECK(hr == S_OK && folder, "InstalledLocation %lx", hr);
        if (folder)
        {
            CALL(folder, 0, HRESULT, &IID_IStorageItem_, &item);
            CHECK(item != NULL, "folder is an IStorageItem");
            if (item)
            {
                WCHAR exe[MAX_PATH], *sep;
                GetModuleFileNameW(NULL, exe, MAX_PATH);
                sep = wcsrchr(exe, 0x5c); *sep = 0;
                s = NULL; CALL(item, 12, HRESULT, &s);
                CHECK(!wcsicmp(str(s), exe), "Path %ls vs %ls", str(s), exe);
                CALL(item, 2, ULONG);
            }
            CALL(folder, 2, ULONG);
        }
        CALL(package, 2, ULONG);
no_package:

        /* CoreApplication.GetCurrentView */
        hr = CALL(app, 12, HRESULT, &cview);
        CHECK(hr == S_OK && cview, "GetCurrentView %lx", hr);
        b = FALSE; CALL(cview, 9, HRESULT, &b);
        CHECK(b, "IsMain");
        b = TRUE; CALL(cview, 10, HRESULT, &b);
        CHECK(!b, "IsHosted");
        hr = CALL(app, 13, HRESULT, (void *)1);
        CHECK(hr == HRESULT_FROM_WIN32(15700), "Run without a package %lx", hr);
        (void)e; (void)tmp;
    }

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
