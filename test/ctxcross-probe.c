/* combase IContextCallback::ContextCallback from another apartment (patches/sg/6180): an MTA thread
 * entering an STA thread's context runs the function on that thread (from its message loop, as
 * C++/WinRT's co_await of an apartment_context does: Windows Terminal's UI updates); an STA thread
 * entering the MTA's context runs it on an MTA thread; the caller gets the function's result. */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <ctxtcall.h>
#include <stdio.h>

static const IID IID_IContextCallback_ = {0x000001da, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const IID IID_ICallbackNoReentrancy_ = {0x0a299774, 0x3e4e, 0xfc42, {0x1d, 0x9d, 0x72, 0xce, 0xe1, 0x05, 0xca, 0x57}};

static DWORD ran_on;
static APTTYPE ran_apt;
static HRESULT CALLBACK cb(ComCallData *data)
{
    APTTYPEQUALIFIER q;
    ran_on = GetCurrentThreadId();
    CoGetApartmentType(&ran_apt, &q);
    data->dwReserved = data->dwDispid * 2;
    return S_FALSE;
}

static IContextCallback *sta_ctx, *mta_ctx;
static HRESULT worker_hr;
static DWORD worker_doubled;
static HANDLE got_mta_ctx, release_mta;

static DWORD WINAPI enter_sta(void *arg)
{
    ComCallData data = {21, 0, NULL};
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    worker_hr = IContextCallback_ContextCallback(sta_ctx, cb, &data, &IID_ICallbackNoReentrancy_, 5, NULL);
    worker_doubled = data.dwReserved;
    CoUninitialize();
    return 0;
}

static DWORD WINAPI mta_owner(void *arg)
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    CoGetObjectContext(&IID_IContextCallback_, (void **)&mta_ctx);
    SetEvent(got_mta_ctx);
    WaitForSingleObject(release_mta, 30000);
    CoUninitialize();
    return 0;
}

int main(void)
{
    ComCallData data = {50, 0, NULL};
    HANDLE thread;
    DWORD main_tid = GetCurrentThreadId();
    HRESULT hr;
    MSG msg;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoGetObjectContext(&IID_IContextCallback_, (void **)&sta_ctx);
    printf("context=%08lx\n", hr);
    if (FAILED(hr)) return 1;

    /* an MTA thread enters this STA thread's context: run here, while this thread dispatches messages */
    thread = CreateThread(NULL, 0, enter_sta, NULL, 0, NULL);
    while (MsgWaitForMultipleObjects(1, &thread, FALSE, 10000, QS_ALLINPUT) == WAIT_OBJECT_0 + 1)
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    printf("sta=%08lx,%lu,%d,%d\n", worker_hr, worker_doubled, ran_on == main_tid, ran_apt == APTTYPE_STA || ran_apt == APTTYPE_MAINSTA);

    /* this STA thread enters the MTA's context: run on an MTA thread */
    got_mta_ctx = CreateEventW(NULL, TRUE, FALSE, NULL);
    release_mta = CreateEventW(NULL, TRUE, FALSE, NULL);
    thread = CreateThread(NULL, 0, mta_owner, NULL, 0, NULL);
    WaitForSingleObject(got_mta_ctx, 10000);
    ran_on = 0;
    hr = mta_ctx ? IContextCallback_ContextCallback(mta_ctx, cb, &data, &IID_ICallbackNoReentrancy_, 5, NULL) : E_FAIL;
    printf("mta=%08lx,%lu,%d,%d\n", hr, data.dwReserved, ran_on && ran_on != main_tid, ran_apt == APTTYPE_MTA);
    SetEvent(release_mta);
    WaitForSingleObject(thread, 10000);
    return 0;
}
