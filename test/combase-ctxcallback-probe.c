/* Probe for patches/sg/5039: IContextCallback::ContextCallback on the object
 * context "enters the object context, executes the specified function, and
 * returns": a caller already in that context - its own STA thread, or any
 * MTA thread for the MTA's context - runs the function at once and gets its
 * result. It returned E_NOTIMPL.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <ctxtcall.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const IID IID_IContextCallback_ = {0x000001da, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const IID IID_ICallbackNoReentrancy_ = {0x0a299774, 0x3e4e, 0xfc42, {0x1d, 0x9d, 0x72, 0xce, 0xe1, 0x05, 0xca, 0x57}};

static DWORD ran_on;
static HRESULT CALLBACK cb(ComCallData *data)
{
    ran_on = GetCurrentThreadId();
    data->dwReserved = data->dwDispid * 2;
    return data->pUserDefined ? S_FALSE : S_OK;
}

static HRESULT call(IContextCallback *ctx, DWORD value, BOOL want_false, DWORD *doubled)
{
    ComCallData data = {value, 0, want_false ? (void *)1 : NULL};
    HRESULT hr = IContextCallback_ContextCallback(ctx, cb, &data, &IID_ICallbackNoReentrancy_, 5, NULL);
    *doubled = data.dwReserved;
    return hr;
}

static IContextCallback *mta_ctx;

static DWORD WINAPI mta_thread(void *arg)
{
    DWORD doubled = 0;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    ran_on = 0;
    hr = call(mta_ctx, 21, FALSE, &doubled);
    check(hr == S_OK && doubled == 42 && ran_on == GetCurrentThreadId(),
          "the MTA context from another MTA thread: %#lx %lu, ran here %d", hr, doubled, ran_on == GetCurrentThreadId());
    CoUninitialize();
    return 0;
}

int main(void)
{
    IContextCallback *ctx = NULL;
    DWORD doubled = 0;
    HANDLE thread;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoGetObjectContext(&IID_IContextCallback_, (void **)&ctx);
    check(hr == S_OK && ctx, "STA object context: %#lx", hr);
    if (ctx)
    {
        ran_on = 0;
        hr = call(ctx, 5, FALSE, &doubled);
        check(hr == S_OK && doubled == 10 && ran_on == GetCurrentThreadId(), "STA, own thread: %#lx %lu", hr, doubled);
        hr = call(ctx, 7, TRUE, &doubled);
        check(hr == S_FALSE && doubled == 14, "the function's result comes back: %#lx %lu", hr, doubled);
        IContextCallback_Release(ctx);
    }
    CoUninitialize();

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoGetObjectContext(&IID_IContextCallback_, (void **)&mta_ctx);
    check(hr == S_OK && mta_ctx, "MTA object context: %#lx", hr);
    if (mta_ctx)
    {
        hr = call(mta_ctx, 1, FALSE, &doubled);
        check(hr == S_OK && doubled == 2, "MTA, own thread: %#lx %lu", hr, doubled);
        thread = CreateThread(NULL, 0, mta_thread, NULL, 0, NULL);
        WaitForSingleObject(thread, 10000);
        CloseHandle(thread);
        IContextCallback_Release(mta_ctx);
    }
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
