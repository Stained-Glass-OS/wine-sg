#define COBJMACROS
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <inspectable.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const GUID IID_AF = {0x00000035,0,0,{0xc0,0,0,0,0,0,0,0x46}};

int main(void)
{
    static const WCHAR name[] = L"Windows.ApplicationModel.DataTransfer.DragDrop.Core.CoreDragDropManager";
    HSTRING cls, got = NULL;
    IInspectable *insp;
    ULONG count = 0;
    IID *iids = NULL;
    TrustLevel trust = 99;
    HRESULT hr;

    hr = RoInitialize(RO_INIT_MULTITHREADED);
    CHECK(SUCCEEDED(hr), "init");
    WindowsCreateString(name, (sizeof(name)/sizeof(WCHAR) - 1), &cls);
    hr = RoGetActivationFactory(cls, &IID_AF, (void **)&insp);
    WindowsDeleteString(cls);
    CHECK(hr == S_OK, "factory %08lx", hr);
    if (hr == S_OK)
    {
        hr = IInspectable_GetIids(insp, &count, &iids);
        CHECK(hr == S_OK && count == 3 && iids, "iids %08lx %lu", hr, count);
        CoTaskMemFree(iids);
        hr = IInspectable_GetRuntimeClassName(insp, &got);
        CHECK(hr == S_OK && got && !wcscmp(WindowsGetStringRawBuffer(got, NULL), name), "class name %08lx", hr);
        WindowsDeleteString(got);
        hr = IInspectable_GetTrustLevel(insp, &trust);
        CHECK(hr == S_OK && trust == BaseTrust, "trust %08lx", hr);
        IInspectable_Release(insp);
    }
    RoUninitialize();
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
