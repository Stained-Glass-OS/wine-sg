/* WinRT objects answer IAgileObject (patch 3288). Every class is activated through RoGetActivationFactory; the
 * factory and, where the class has a default constructor, an instance are asked for IAgileObject. The classes
 * listed are Windows Runtime classes that are agile. Not listed on purpose, because Wine's tests (real Windows)
 * say they are NOT agile: EasClientDeviceInformation, DeviceInformation and DeviceAccessInformation. */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>

static const IID IID_IAgile = {0x94ea2b94, 0xe9cc, 0x49e0, {0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90}};
static const IID IID_IActFactory = {0x00000035, 0x0000, 0x0000, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
static const IID IID_IInsp = {0xaf86e2e0, 0xb12d, 0x4c6a, {0x9c, 0x5a, 0xd7, 0xaa, 0x65, 0x10, 0x1e, 0x90}};
static int fails, checked;

typedef HRESULT (WINAPI *PWindowsCreateString)(const WCHAR *, UINT32, void **);
typedef HRESULT (WINAPI *PWindowsDeleteString)(void *);
typedef HRESULT (WINAPI *PRoGetActivationFactory)(void *, REFIID, void **);
typedef HRESULT (WINAPI *PRoActivateInstance)(void *, IUnknown **);

static const WCHAR *agile[] = {
    L"Windows.Web.Http.HttpClient", L"Windows.Web.Http.HttpRequestMessage", L"Windows.Web.Http.HttpResponseMessage",
    L"Windows.Web.Http.HttpStringContent", L"Windows.Web.Http.HttpMethod", L"Windows.Web.Http.HttpCookie",
    L"Windows.Web.Http.HttpCookieManager", L"Windows.Web.Http.Filters.HttpBaseProtocolFilter",
    L"Windows.Data.Json.JsonObject", L"Windows.Data.Json.JsonArray", L"Windows.Data.Json.JsonValue",
    L"Windows.Storage.StorageFile", L"Windows.Storage.StorageFolder", L"Windows.Storage.FileIO",
    L"Windows.Storage.PathIO", L"Windows.Storage.KnownFolders", L"Windows.Storage.Streams.Buffer",
    L"Windows.Storage.Streams.DataWriter", L"Windows.Storage.Streams.InMemoryRandomAccessStream",
    L"Windows.Storage.Streams.RandomAccessStreamReference", L"Windows.Storage.ApplicationData",
    L"Windows.Foundation.Collections.PropertySet", L"Windows.Foundation.PropertyValue",
    L"Windows.Management.Deployment.PackageManager", L"Windows.ApplicationModel.Package",
    L"Windows.ApplicationModel.PackageCatalog", L"Windows.Globalization.GeographicRegion",
    L"Windows.System.Threading.ThreadPool",
    L"Windows.Data.Xml.Dom.XmlDocument",
    L"Windows.System.Profile.AnalyticsInfo", NULL };
static void run(const WCHAR *name, int want_agile, PWindowsCreateString create, PWindowsDeleteString del,
                PRoGetActivationFactory getf, PRoActivateInstance activate)
{
    void *hs = NULL;
    IUnknown *factory = NULL, *obj = NULL, *tmp = NULL;
    HRESULT hr;

    if (FAILED(create(name, lstrlenW(name), &hs))) return;
    hr = getf(hs, &IID_IActFactory, (void **)&factory);
    if (FAILED(hr) || !factory) { printf("note: no factory for %ls (%08lx)\n", name, hr); del(hs); return; }
    checked++;
    hr = IUnknown_QueryInterface(factory, &IID_IAgile, (void **)&tmp);
    if (want_agile ? hr != S_OK : hr == S_OK) { fails++; printf("FAIL factory of %ls: IAgileObject %08lx (want %s)\n", name, hr, want_agile ? "yes" : "no"); }
    if (tmp) IUnknown_Release(tmp);
    tmp = NULL;
    hr = activate(hs, &obj);
    if (SUCCEEDED(hr) && obj)
    {
        checked++;
        hr = IUnknown_QueryInterface(obj, &IID_IAgile, (void **)&tmp);
        if (want_agile ? hr != S_OK : hr == S_OK) { fails++; printf("FAIL instance of %ls: IAgileObject %08lx (want %s)\n", name, hr, want_agile ? "yes" : "no"); }
        if (tmp) IUnknown_Release(tmp);
        /* the identity: asking again through IInspectable gives the same object */
        tmp = NULL;
        if (SUCCEEDED(IUnknown_QueryInterface(obj, &IID_IInsp, (void **)&tmp)) && tmp)
        {
            IUnknown *a = NULL, *b = NULL;
            IUnknown_QueryInterface(obj, &IID_IUnknown, (void **)&a);
            IUnknown_QueryInterface(tmp, &IID_IUnknown, (void **)&b);
            if (a != b) { fails++; printf("FAIL identity of %ls\n", name); }
            if (a) IUnknown_Release(a);
            if (b) IUnknown_Release(b);
            IUnknown_Release(tmp);
        }
        IUnknown_Release(obj);
    }
    IUnknown_Release(factory);
    del(hs);
}

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    PWindowsCreateString create = (void *)GetProcAddress(combase, "WindowsCreateString");
    PWindowsDeleteString del = (void *)GetProcAddress(combase, "WindowsDeleteString");
    PRoGetActivationFactory getf = (void *)GetProcAddress(combase, "RoGetActivationFactory");
    PRoActivateInstance activate = (void *)GetProcAddress(combase, "RoActivateInstance");
    typedef HRESULT (WINAPI *PRoInitialize)(int);
    PRoInitialize init = (void *)GetProcAddress(combase, "RoInitialize");
    int i;

    setvbuf(stdout, NULL, _IONBF, 0);
    init(1);   /* RO_INIT_MULTITHREADED */
    for (i = 0; agile[i]; i++) run(agile[i], 1, create, del, getf, activate);
    printf("checked %d, failures %d\n", checked, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
