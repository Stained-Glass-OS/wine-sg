#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <stdio.h>

typedef void *HSTRING_;
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef HRESULT (WINAPI *WCS)(const WCHAR *, UINT32, HSTRING_ *);
typedef HRESULT (WINAPI *RGAF)(HSTRING_, const GUID *, void **);
#define VT(obj) (*(void ***)(obj))
#define CALL(obj, idx, ret, ...) (((ret (WINAPI *)(void *, ...))VT(obj)[idx])(obj, ##__VA_ARGS__))

static const GUID IID_IApplicationDataStatics_ = {0x5612147b, 0xe843, 0x45e3, {0x94, 0xd8, 0x06, 0x16, 0x9e, 0x3c, 0x8e, 0x17}};
static const GUID IID_IPropertyValueStatics_ = {0x629bdbc8, 0xd932, 0x4ff4, {0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58}};
/* IMap<HSTRING, IInspectable> */
static const GUID IID_IMap_ = {0x1b0d3570, 0x0877, 0x5ec2, {0x8a, 0x2c, 0x3b, 0x95, 0x39, 0x50, 0x6a, 0xca}};

static HSTRING_ mk(WCS create, const WCHAR *s) { HSTRING_ h = NULL; create(s, wcslen(s), &h); return h; }

static int child(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    WCS create = (WCS)GetProcAddress(combase, "WindowsCreateString");
    RGAF get_factory = (RGAF)GetProcAddress(combase, "RoGetActivationFactory");
    HRESULT (WINAPI *ro_init)(int) = (void *)GetProcAddress(combase, "RoInitialize");
    void *statics = NULL, *data = NULL, *settings = NULL, *values = NULL, *map = NULL, *vs = NULL, *boxed = NULL, *got = NULL;
    HSTRING_ key;
    HRESULT hr;
    UINT32 size = 0;
    unsigned char replaced = 0, found = 0;

    ro_init(1);
    hr = get_factory(mk(create, L"Windows.Storage.ApplicationData"), &IID_IApplicationDataStatics_, &statics);
    CHECK(hr == S_OK, "factory %lx", hr);
    hr = CALL(statics, 6, HRESULT, &data); CHECK(hr == S_OK && data, "Current %lx", hr);
    hr = CALL(data, 10, HRESULT, &settings); CHECK(hr == S_OK && settings, "LocalSettings %lx", hr);
    hr = CALL(settings, 8, HRESULT, &values); CHECK(hr == S_OK && values, "Values %lx", hr);
    hr = CALL(values, 0, HRESULT, &IID_IMap_, &map); CHECK(hr == S_OK && map, "QI IMap %lx", hr);
    if (!map) return 1;

    hr = get_factory(mk(create, L"Windows.Foundation.PropertyValue"), &IID_IPropertyValueStatics_, &vs);
    CHECK(hr == S_OK, "propertyvalue %lx", hr);
    key = mk(create, L"answer");
    CALL(map, 12, HRESULT);                                      /* Clear */
    hr = CALL(vs, 10, HRESULT, 42, &boxed); CHECK(hr == S_OK, "CreateInt32 %lx", hr);
    hr = CALL(map, 10, HRESULT, key, boxed, &replaced); CHECK(hr == S_OK && !replaced, "Insert %lx", hr);
    hr = CALL(map, 7, HRESULT, &size); CHECK(hr == S_OK && size == 1, "Size %lx %u", hr, size);
    hr = CALL(map, 8, HRESULT, key, &found); CHECK(hr == S_OK && found, "HasKey %lx", hr);
    hr = CALL(map, 6, HRESULT, key, &got); CHECK(hr == S_OK && got, "Lookup %lx", hr);
    hr = CALL(map, 11, HRESULT, key); CHECK(hr == S_OK, "Remove %lx", hr);
    hr = CALL(map, 11, HRESULT, key); CHECK(hr == (HRESULT)0x8000000b, "Remove again %lx", hr);
    return fails;
}

int main(int argc, char **argv)
{
    HMODULE combase;
    WCS create;
    RGAF get_factory;
    HRESULT (WINAPI *ro_init)(int);
    void *statics = NULL, *data = NULL;
    HRESULT hr;
    char path[MAX_PATH], cmd[MAX_PATH + 16], dir[MAX_PATH], *p;
    WCHAR wdir[MAX_PATH];
    HKEY key;
    PROCESS_INFORMATION pi;
    STARTUPINFOA si = {sizeof(si)};
    DWORD code = 1;
    static const WCHAR *pkey = L"Software\\Wine\\AppModel\\Packages\\Fast.AppData_1.0.0.0_x64__8wekyb3d8bbwe";

    if (argc > 1) return child();

    combase = LoadLibraryA("combase.dll");
    create = (WCS)GetProcAddress(combase, "WindowsCreateString");
    get_factory = (RGAF)GetProcAddress(combase, "RoGetActivationFactory");
    ro_init = (void *)GetProcAddress(combase, "RoInitialize");
    ro_init(1);

    /* a process without a package has no application data */
    hr = get_factory(mk(create, L"Windows.Storage.ApplicationData"), &IID_IApplicationDataStatics_, &statics);
    CHECK(hr == S_OK, "factory %lx", hr);
    hr = CALL(statics, 6, HRESULT, &data);
    CHECK(hr == (HRESULT)0x80073d54 && !data, "unpackaged Current %lx", hr);

    /* the same program registered as a package has */
    GetModuleFileNameA(NULL, path, sizeof(path));
    strcpy(dir, path);
    if ((p = strrchr(dir, '\\'))) *p = 0;
    MultiByteToWideChar(CP_ACP, 0, dir, -1, wdir, MAX_PATH);
    if (RegCreateKeyExW(HKEY_CURRENT_USER, pkey, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL) == 0)
    {
        RegSetValueExW(key, L"InstallLocation", 0, REG_SZ, (BYTE *)wdir, (wcslen(wdir) + 1) * sizeof(WCHAR));
        RegCloseKey(key);
        sprintf(cmd, "\"%s\" child", path);
        CHECK(CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi), "child");
        WaitForSingleObject(pi.hProcess, 30000);
        GetExitCodeProcess(pi.hProcess, &code);
        CHECK(code == 0, "packaged child failed (%lu)", code);
        RegDeleteTreeW(HKEY_CURRENT_USER, pkey);
        RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Wine\\ApplicationData\\Fast.AppData_8wekyb3d8bbwe");
    }
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
