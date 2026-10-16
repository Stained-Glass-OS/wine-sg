/* Probe for patches/sg/5062: IComThreadingInfo::SetCurrentLogicalThreadId.
 * - in an apartment (STA and MTA) it sets the GUID that GetCurrentLogicalThreadId and
 *   CoGetCurrentLogicalThreadId then report;
 * - outside any apartment it is E_FAIL (documented).
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <objidl.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

DEFINE_GUID(IID_IComThreadingInfo_, 0x000001ce, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

int main(void)
{
    static const struct { DWORD init; const char *name; GUID id; } tests[] =
    {
        {COINIT_APARTMENTTHREADED, "STA", {0x11111111, 0x2222, 0x3333, {4, 4, 5, 5, 6, 6, 7, 7}}},
        {COINIT_MULTITHREADED, "MTA", {0x99999999, 0x8888, 0x7777, {1, 2, 3, 4, 5, 6, 7, 8}}},
    };
    IComThreadingInfo *info;
    GUID got, got2;
    unsigned int i;
    HRESULT hr;

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        CoInitializeEx(NULL, tests[i].init);
        info = NULL;
        hr = CoGetObjectContext(&IID_IComThreadingInfo_, (void **)&info);
        check(hr == S_OK && info, "%s: CoGetObjectContext %#lx", tests[i].name, hr);
        if (!info) { CoUninitialize(); continue; }
        hr = IComThreadingInfo_SetCurrentLogicalThreadId(info, &tests[i].id);
        check(hr == S_OK, "%s: SetCurrentLogicalThreadId %#lx", tests[i].name, hr);
        memset(&got, 0, sizeof(got));
        hr = IComThreadingInfo_GetCurrentLogicalThreadId(info, &got);
        check(hr == S_OK && IsEqualGUID(&got, &tests[i].id), "%s: GetCurrentLogicalThreadId %#lx", tests[i].name, hr);
        memset(&got2, 0, sizeof(got2));
        hr = CoGetCurrentLogicalThreadId(&got2);
        check(hr == S_OK && IsEqualGUID(&got2, &tests[i].id), "%s: CoGetCurrentLogicalThreadId %#lx", tests[i].name, hr);
        CoUninitialize();

        /* the thread has left its apartment */
        hr = IComThreadingInfo_SetCurrentLogicalThreadId(info, &tests[1 - i].id);
        check(hr == E_FAIL, "%s: outside an apartment SetCurrentLogicalThreadId %#lx", tests[i].name, hr);
        IComThreadingInfo_Release(info);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
