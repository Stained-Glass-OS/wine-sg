/* rtworkq: RtwqSetDeadline, RtwqSetDeadline2, RtwqCancelDeadline (patch 3738) */
#include <windows.h>
#include <stdio.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define E_INVALID_WORKQUEUE ((HRESULT)0xc00d36ff)  /* MF_E_INVALID_WORKQUEUE (RTWQ_E_ERROR(14079)) */

int main(void)
{
    HMODULE m = LoadLibraryA("rtworkq.dll");
    HRESULT (WINAPI *pStartup)(void) = (void *)GetProcAddress(m, "RtwqStartup");
    HRESULT (WINAPI *pShutdown)(void) = (void *)GetProcAddress(m, "RtwqShutdown");
    HRESULT (WINAPI *pSet)(DWORD, LONGLONG, HANDLE *) = (void *)GetProcAddress(m, "RtwqSetDeadline");
    HRESULT (WINAPI *pSet2)(DWORD, LONGLONG, LONGLONG, HANDLE *) = (void *)GetProcAddress(m, "RtwqSetDeadline2");
    HRESULT (WINAPI *pCancel)(HANDLE) = (void *)GetProcAddress(m, "RtwqCancelDeadline");
    HRESULT (WINAPI *pAlloc)(int, DWORD *) = (void *)GetProcAddress(m, "RtwqAllocateWorkQueue");
    HRESULT (WINAPI *pUnlock)(DWORD) = (void *)GetProcAddress(m, "RtwqUnlockWorkQueue");
    HANDLE h = NULL, h2 = NULL;
    HRESULT hr;
    DWORD queue = 0, flags;

    if (!pSet) { printf("FAIL no RtwqSetDeadline\nRESULT: FAIL\n"); return 1; }
    hr = pStartup();
    CHECK(hr == S_OK, "startup %#lx", hr);

    hr = pSet(1 /* standard queue */, 10000000, &h);
    CHECK(hr == S_OK && h, "SetDeadline on the standard queue: %#lx %p", hr, h);
    CHECK(h && GetHandleInformation(h, &flags), "the request is a handle");
    hr = pSet2(1, 20000000, 5000000, &h2);
    CHECK(hr == S_OK && h2 && h2 != h, "SetDeadline2: %#lx %p", hr, h2);
    hr = pCancel(h);
    CHECK(hr == S_OK, "cancel %#lx", hr);
    CHECK(!GetHandleInformation(h, &flags), "the request handle is released");
    hr = pCancel(h);
    CHECK(hr == E_INVALIDARG, "cancel twice: %#lx", hr);
    hr = pCancel(h2);
    CHECK(hr == S_OK, "cancel the second %#lx", hr);
    hr = pCancel(NULL);
    CHECK(hr == E_INVALIDARG, "cancel NULL: %#lx", hr);
    hr = pCancel(GetCurrentProcess());
    CHECK(hr == E_INVALIDARG, "cancel a handle that is not a request: %#lx", hr);

    h = (HANDLE)0x1234;
    hr = pSet(1, 0, &h);
    CHECK(hr == E_INVALIDARG && !h, "zero deadline: %#lx %p", hr, h);
    hr = pSet(1, -5, &h);
    CHECK(hr == E_INVALIDARG, "negative deadline: %#lx", hr);
    hr = pSet2(1, 100, 200, &h);
    CHECK(hr == E_INVALIDARG, "predeadline after the deadline: %#lx", hr);
    hr = pSet(1, 100, NULL);
    CHECK(hr == E_POINTER, "NULL request: %#lx", hr);
    h = NULL;
    hr = pSet(0xdead0000, 100, &h);
    CHECK(hr == E_INVALID_WORKQUEUE && !h, "unknown queue: %#lx", hr);

    /* a user queue */
    hr = pAlloc(0, &queue);
    CHECK(hr == S_OK && queue, "alloc queue %#lx", hr);
    if (hr == S_OK)
    {
        hr = pSet(queue, 100, &h);
        CHECK(hr == S_OK && h, "user queue: %#lx", hr);
        pCancel(h);
        hr = pUnlock(queue);
        CHECK(hr == S_OK, "unlock %#lx", hr);
        h = NULL;
        hr = pSet(queue, 100, &h);
        CHECK(hr == E_INVALID_WORKQUEUE, "released queue: %#lx", hr);
    }
    pShutdown();
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
