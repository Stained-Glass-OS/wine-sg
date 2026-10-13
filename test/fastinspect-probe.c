#include <windows.h>
#include <roapi.h>
#define COBJMACROS
#include <winstring.h>
#include <inspectable.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d ", #c, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static const GUID IID_IInspectable_ = { 0xaf86e2e0, 0xb12d, 0x4c6a, { 0x9c, 0x5a, 0xd7, 0xaa, 0x65, 0x10, 0x1e, 0x90 } };
static const GUID IID_IActivationFactory_ = { 0x00000035, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };

static void check_class(const WCHAR *name, int min_iids)
{
    HSTRING hs;
    IInspectable *insp;
    ULONG count = 0, i;
    IID *iids = NULL;
    HSTRING cls = NULL;
    TrustLevel trust = FullTrust;
    const WCHAR *buf;
    HRESULT hr;
    int have_af = 0;

    hr = WindowsCreateString(name, lstrlenW(name), &hs);
    CHECK(hr == S_OK, "create %lx", hr);
    hr = RoGetActivationFactory(hs, &IID_IInspectable_, (void **)&insp);
    if (hr != S_OK) { printf("FAIL factory %ls hr %lx\n", name, hr); fails++; WindowsDeleteString(hs); return; }
    hr = insp->lpVtbl->GetIids(insp, &count, &iids);
    CHECK(hr == S_OK, "%ls GetIids %lx", name, hr);
    CHECK(count >= (ULONG)min_iids, "%ls count %lu", name, count);
    for (i = 0; hr == S_OK && i < count; i++) if (IsEqualGUID(&iids[i], &IID_IActivationFactory_)) have_af = 1;
    CHECK(have_af, "%ls IActivationFactory in list", name);
    CoTaskMemFree(iids);
    hr = insp->lpVtbl->GetRuntimeClassName(insp, &cls);
    CHECK(hr == S_OK, "%ls GetRuntimeClassName %lx", name, hr);
    if (hr == S_OK)
    {
        buf = WindowsGetStringRawBuffer(cls, NULL);
        CHECK(!lstrcmpW(buf, name), "%ls class name %ls", name, buf);
        WindowsDeleteString(cls);
    }
    hr = insp->lpVtbl->GetTrustLevel(insp, &trust);
    CHECK(hr == S_OK && trust == BaseTrust, "%ls trust %lx %d", name, hr, trust);
    hr = insp->lpVtbl->GetIids(insp, NULL, &iids);
    CHECK(hr == E_POINTER, "%ls null count %lx", name, hr);
    insp->lpVtbl->Release(insp);
    WindowsDeleteString(hs);
}

int main(void)
{
    RoInitialize(RO_INIT_MULTITHREADED);
    check_class(L"Windows.Data.Json.JsonValue", 2);
    check_class(L"Windows.Data.Json.JsonObject", 1);
    check_class(L"Windows.Data.Json.JsonArray", 1);
    check_class(L"Windows.Storage.Streams.Buffer", 2);
    check_class(L"Windows.Storage.Streams.DataWriter", 2);
    check_class(L"Windows.Storage.Streams.InMemoryRandomAccessStream", 1);
    check_class(L"Windows.Management.Deployment.PackageManager", 1);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
