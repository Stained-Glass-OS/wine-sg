/* hvsimanagementapi, windows.perception.stub, graphicscapture: consistent "not supported" answers */
#define COBJMACROS
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef void *OBJ;
#define SLOT(o, n) ((*(void ***)(o))[n])
#define CALL(o, n, ret, ...) ((ret (WINAPI *)())SLOT(o, n))(o, ##__VA_ARGS__)

static const GUID IID_AF = {0x00000035,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
static const GUID IID_AsyncInfo = {0x00000036,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
static const GUID IID_HostStatics = {0x2c0e22c7,0x05a0,0x517a,{0xb8,0x1c,0x6e,0xe8,0x79,0x0c,0x38,0x1f}};
static const GUID IID_HoloStatics2 = {0x0e777088,0x75fc,0x48af,{0x87,0x58,0x06,0x52,0xf6,0xf0,0x7c,0x59}};
static const GUID IID_ObserverStatics = {0x165951ed,0x2108,0x4168,{0x91,0x75,0x87,0xe0,0x27,0xbc,0x92,0x85}};
#define E_BOUNDS_ ((HRESULT)0x8000000B)

static OBJ factory(const WCHAR *cls, const GUID *iid)
{
    HSTRING s;
    OBJ f = NULL;
    HRESULT hr;

    WindowsCreateString(cls, wcslen(cls), &s);
    hr = RoGetActivationFactory(s, iid, (void **)&f);
    WindowsDeleteString(s);
    CHECK(hr == S_OK, "factory %ls %08lx", cls, hr);
    return f;
}

/* the inspectable methods of an activation factory */
static void check_factory(const WCHAR *cls)
{
    OBJ f = factory(cls, &IID_AF);
    ULONG n = 0;
    IID *iids = NULL;
    HSTRING name = NULL;
    TrustLevel trust = 99;
    OBJ inst = (OBJ)1;
    HRESULT hr;

    if (!f) return;
    hr = CALL(f, 3, HRESULT, &n, &iids);
    CHECK(hr == S_OK && n >= 1 && iids && IsEqualGUID(&iids[0], &IID_AF), "%ls GetIids %08lx", cls, hr);
    CoTaskMemFree(iids);
    hr = CALL(f, 4, HRESULT, &name);
    CHECK(hr == S_OK && !wcscmp(WindowsGetStringRawBuffer(name, NULL), cls), "%ls class name", cls);
    WindowsDeleteString(name);
    hr = CALL(f, 5, HRESULT, &trust);
    CHECK(hr == S_OK && trust == BaseTrust, "%ls trust", cls);
    hr = CALL(f, 6, HRESULT, &inst);
    CHECK(hr == E_NOTIMPL && !inst, "%ls ActivateInstance %08lx", cls, hr);
    CALL(f, 2, ULONG);
}

int main(void)
{
    OBJ st, view, iterable, it, op, info;
    UINT32 size = 99, index = 99, copied = 0;
    int value = -1, status = -1, i;
    boolean b = 7, found = 7;
    ULONG n = 0;
    IID *iids = NULL;
    HRESULT hr;
    LONGLONG tok1 = 0, tok2 = 0;

    RoInitialize(RO_INIT_MULTITHREADED);

    check_factory(L"Windows.Security.Isolation.IsolatedWindowsEnvironmentHost");
    check_factory(L"Windows.Graphics.Holographic.HolographicSpace");
    check_factory(L"Windows.Perception.Spatial.Surfaces.SpatialSurfaceObserver");
    check_factory(L"Windows.Graphics.Capture.GraphicsCaptureSession");

    /* the isolated host is not ready, and says why */
    st = factory(L"Windows.Security.Isolation.IsolatedWindowsEnvironmentHost", &IID_HostStatics);
    if (st)
    {
        hr = CALL(st, 6, HRESULT, &b);
        CHECK(hr == S_OK && !b, "IsReady");
        view = NULL;
        hr = CALL(st, 7, HRESULT, &view);
        CHECK(hr == S_OK && view, "HostErrors %08lx", hr);
        if (view)
        {
            hr = CALL(view, 7, HRESULT, &size);
            CHECK(hr == S_OK && size == 1, "Size %lu", (unsigned long)size);
            hr = CALL(view, 6, HRESULT, 0, &value);
            CHECK(hr == S_OK && value == 0, "GetAt(0) %d", value);
            hr = CALL(view, 6, HRESULT, 1, &value);
            CHECK(hr == E_BOUNDS_, "GetAt(1) %08lx", hr);
            hr = CALL(view, 8, HRESULT, 0, &index, &found);
            CHECK(hr == S_OK && found && index == 0, "IndexOf");
            hr = CALL(view, 8, HRESULT, 3, &index, &found);
            CHECK(hr == S_OK && !found, "IndexOf missing");
            hr = CALL(view, 9, HRESULT, 0, 4, &value, &copied);
            CHECK(hr == S_OK && copied == 1, "GetMany %lu", (unsigned long)copied);

            /* iterate through IIterable, whose IID comes from the object */
            hr = CALL(view, 3, HRESULT, &n, &iids);
            CHECK(hr == S_OK && n == 2, "view IIDs");
            if (hr == S_OK && n == 2)
            {
                iterable = NULL;
                hr = CALL(view, 0, HRESULT, &iids[1], &iterable);
                CHECK(hr == S_OK && iterable, "QI IIterable %08lx", hr);
                if (iterable)
                {
                    it = NULL;
                    hr = CALL(iterable, 6, HRESULT, &it);
                    CHECK(hr == S_OK && it, "First");
                    if (it)
                    {
                        hr = CALL(it, 7, HRESULT, &b);
                        CHECK(hr == S_OK && b, "HasCurrent");
                        hr = CALL(it, 6, HRESULT, &value);
                        CHECK(hr == S_OK && value == 0, "Current");
                        hr = CALL(it, 8, HRESULT, &b);
                        CHECK(hr == S_OK && !b, "MoveNext ends");
                        hr = CALL(it, 6, HRESULT, &value);
                        CHECK(hr == E_BOUNDS_, "Current past the end %08lx", hr);
                        CALL(it, 2, ULONG);
                    }
                    CALL(iterable, 2, ULONG);
                }
            }
            CoTaskMemFree(iids);
            CALL(view, 2, ULONG);
        }
        CALL(st, 2, ULONG);
    }

    /* holographic space: never available, so nothing to notify about */
    st = factory(L"Windows.Graphics.Holographic.HolographicSpace", &IID_HoloStatics2);
    if (st)
    {
        hr = CALL(st, 6, HRESULT, &b);
        CHECK(hr == S_OK && !b, "IsSupported");
        hr = CALL(st, 7, HRESULT, &b);
        CHECK(hr == S_OK && !b, "IsAvailable");
        hr = CALL(st, 8, HRESULT, NULL, &tok1);
        CHECK(hr == E_POINTER, "add_IsAvailableChanged(NULL) %08lx", hr);
        hr = CALL(st, 8, HRESULT, (void *)0x1000, &tok1);
        CHECK(hr == S_OK && tok1, "add_IsAvailableChanged %08lx", hr);
        hr = CALL(st, 8, HRESULT, (void *)0x1000, &tok2);
        CHECK(hr == S_OK && tok2 && tok2 != tok1, "second token");
        hr = CALL(st, 9, HRESULT, tok1);
        CHECK(hr == S_OK, "remove_IsAvailableChanged %08lx", hr);
        CALL(st, 2, ULONG);
    }

    /* spatial surface access is denied by the system */
    st = factory(L"Windows.Perception.Spatial.Surfaces.SpatialSurfaceObserver", &IID_ObserverStatics);
    if (st)
    {
        op = NULL;
        hr = CALL(st, 6, HRESULT, NULL);
        CHECK(hr == E_POINTER, "RequestAccessAsync(NULL) %08lx", hr);
        hr = CALL(st, 6, HRESULT, &op);
        CHECK(hr == S_OK && op, "RequestAccessAsync %08lx", hr);
        if (op)
        {
            info = NULL;
            hr = CALL(op, 0, HRESULT, &IID_AsyncInfo, &info);
            CHECK(hr == S_OK && info, "IAsyncInfo");
            if (info)
            {
                for (i = 0; i < 50 && status != 1; i++) { CALL(info, 7, HRESULT, &status); }
                CHECK(status == 1, "completed immediately: %d", status);
                CALL(info, 2, ULONG);
            }
            value = -1;
            hr = CALL(op, 8, HRESULT, &value);
            CHECK(hr == S_OK && value == 3, "DeniedBySystem: %08lx %d", hr, value);
            CALL(op, 2, ULONG);
        }
        CALL(st, 2, ULONG);
    }
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
