/* Probe for patches/sg/5019: CoGetDefaultContext. Outside any apartment it
 * fails CO_E_NOTINITIALIZED; APTTYPE_STA is refused with E_INVALIDARG ("returns
 * E_INVALIDARG if APTTYPE_STA is specified"); APTTYPE_CURRENT and the type of
 * the caller's own apartment give the caller's (default) context, the object
 * CoGetObjectContext gives.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <ctxtcall.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const IID IID_IContextCallback_ = {0x000001da, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const IID IID_IComThreadingInfo_ = {0x000001ce, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

static void run(const char *where, APTTYPE own)
{
    static const struct { APTTYPE type; int expect; } tests[] = /* 0 S_OK, 1 E_INVALIDARG, 2 S_OK when own */
    {
        {APTTYPE_CURRENT, 0}, {APTTYPE_STA, 1}, {APTTYPE_MTA, 2}, {APTTYPE_MAINSTA, 2},
    };
    unsigned int i;

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        IUnknown *ctx = (void *)0xdeadbeef, *obj = NULL;
        HRESULT hr;

        if (tests[i].expect == 2 && tests[i].type != own) continue;
        hr = CoGetDefaultContext(tests[i].type, &IID_IContextCallback_, (void **)&ctx);
        if (tests[i].expect == 1)
        {
            check(hr == E_INVALIDARG && !ctx, "%s, type %d: %#lx", where, tests[i].type, hr);
            continue;
        }
        CoGetObjectContext(&IID_IContextCallback_, (void **)&obj);
        check(hr == S_OK && ctx == obj, "%s, type %d: %#lx, the object context %d", where, tests[i].type, hr, ctx == obj);
        if (hr == S_OK)
        {
            IComThreadingInfo *info;
            APTTYPE t = -1;
            if (SUCCEEDED(IUnknown_QueryInterface(ctx, &IID_IComThreadingInfo_, (void **)&info)))
            {
                IComThreadingInfo_GetCurrentApartmentType(info, &t);
                IComThreadingInfo_Release(info);
            }
            check(t == own, "%s, type %d: the context's apartment %d", where, tests[i].type, t);
            IUnknown_Release(ctx);
        }
        if (obj) IUnknown_Release(obj);
    }
}

static DWORD WINAPI sta_thread(void *arg)
{
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    run("STA thread", APTTYPE_STA);
    CoUninitialize();
    return 0;
}

int main(void)
{
    IUnknown *ctx = (void *)0xdeadbeef;
    HANDLE thread;
    HRESULT hr;

    hr = CoGetDefaultContext(APTTYPE_CURRENT, &IID_IContextCallback_, (void **)&ctx);
    check(hr == CO_E_NOTINITIALIZED && !ctx, "no apartment, current: %#lx", hr);
    hr = CoGetDefaultContext(APTTYPE_MTA, &IID_IContextCallback_, (void **)&ctx);
    check(hr == CO_E_NOTINITIALIZED, "no apartment, MTA: %#lx", hr);

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    run("main STA", APTTYPE_MAINSTA);
    thread = CreateThread(NULL, 0, sta_thread, NULL, 0, NULL);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    CoUninitialize();

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    run("MTA", APTTYPE_MTA);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
