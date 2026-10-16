#include <windows.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef void *HSTRING_;
typedef HRESULT (WINAPI *WCS)(const WCHAR *, UINT32, HSTRING_ *);
typedef HRESULT (WINAPI *RGAF)(HSTRING_, const GUID *, void **);
#define VT(obj) (*(void ***)(obj))
#define CALL(obj, idx, ret, ...) (((ret (WINAPI *)(void *, ...))VT(obj)[idx])(obj, ##__VA_ARGS__))

static const GUID IID_IThreadPoolStatics_ = {0xb6bf67dd, 0x84bd, 0x44f8, {0xac, 0x1c, 0x93, 0xeb, 0xcb, 0x9d, 0xba, 0x91}};
static const GUID IID_IAsyncInfo_ = {0x00000036, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

static HANDLE item_event, done_event;
static volatile LONG completed_status = -1;

static HRESULT WINAPI qi(void *iface, const GUID *iid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI addref(void *iface) { return 2; }
static ULONG WINAPI release(void *iface) { return 1; }
static HRESULT WINAPI invoke_item(void *iface, void *action) { SetEvent(item_event); return S_OK; }
static HRESULT WINAPI invoke_done(void *iface, void *action, int status) { completed_status = status; SetEvent(done_event); return S_OK; }
static void *item_vtbl[] = { qi, addref, release, invoke_item };
static void *done_vtbl[] = { qi, addref, release, invoke_done };

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    WCS create = (WCS)GetProcAddress(combase, "WindowsCreateString");
    RGAF get_factory = (RGAF)GetProcAddress(combase, "RoGetActivationFactory");
    HRESULT (WINAPI *ro_init)(int) = (void *)GetProcAddress(combase, "RoInitialize");
    void *item = item_vtbl, *done = done_vtbl;
    void *statics = NULL, *action = NULL, *info = NULL;
    HSTRING_ name = NULL;
    HRESULT hr, error = E_FAIL;
    UINT32 id = 0;
    int status = -1;

    item_event = CreateEventA(NULL, FALSE, FALSE, NULL);
    done_event = CreateEventA(NULL, FALSE, FALSE, NULL);
    ro_init(1);
    create(L"Windows.System.Threading.ThreadPool", 35, &name);
    hr = get_factory(name, &IID_IThreadPoolStatics_, &statics);
    CHECK(hr == S_OK, "factory %lx", hr);
    if (hr != S_OK) return 1;

    hr = CALL(statics, 6, HRESULT, &item, &action);
    CHECK(hr == S_OK && action, "RunAsync %lx", hr);
    CHECK(WaitForSingleObject(item_event, 5000) == WAIT_OBJECT_0, "work item did not run");
    hr = CALL(action, 0, HRESULT, &IID_IAsyncInfo_, &info); CHECK(hr == S_OK, "QI info");

    /* the completion handler is called with Completed, also when it is set late */
    hr = CALL(action, 6, HRESULT, &done); CHECK(hr == S_OK, "put_Completed %lx", hr);
    CHECK(WaitForSingleObject(done_event, 5000) == WAIT_OBJECT_0 && completed_status == 1, "handler status %ld", completed_status);
    hr = CALL(action, 6, HRESULT, &done); CHECK(hr == (HRESULT)0x80000018, "second handler %lx", hr);
    hr = CALL(action, 8, HRESULT); CHECK(hr == S_OK, "GetResults %lx", hr);
    hr = CALL(info, 6, HRESULT, &id); CHECK(hr == S_OK && id != 0, "Id %lx", hr);
    hr = CALL(info, 7, HRESULT, &status); CHECK(hr == S_OK && status == 1, "Status %lx %d", hr, status);
    hr = CALL(info, 8, HRESULT, &error); CHECK(hr == S_OK && error == S_OK, "ErrorCode %lx", hr);
    hr = CALL(info, 10, HRESULT); CHECK(hr == S_OK, "Close %lx", hr);
    hr = CALL(info, 7, HRESULT, &status); CHECK(hr == (HRESULT)0x8000000e, "Status after Close %lx", hr);

    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
