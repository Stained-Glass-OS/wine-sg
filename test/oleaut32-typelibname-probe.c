/* Probe for patches/sg/5036: ITypeLib::IsName and FindName match names
 * whatever their case and write the library's spelling into the caller's
 * buffer (oleaut32 typelib tests on Windows: FindName("guid") finds GUID and
 * the buffer reads "GUID"); unknown names leave the buffer alone.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    static const struct { const WCHAR *ask, *want; BOOL found; } names[] =
    {
        {L"guid", L"GUID", TRUE}, {L"iunknown", L"IUnknown", TRUE}, {L"IUnknown", L"IUnknown", TRUE},
        {L"BOLD", L"Bold", TRUE}, {L"queryinterface", L"QueryInterface", TRUE}, {L"sgnosuchname", L"sgnosuchname", FALSE},
    };
    ITypeLib *tl;
    unsigned int i;
    HRESULT hr;

    CoInitialize(NULL);
    hr = LoadTypeLib(L"stdole2.tlb", &tl);
    check(hr == S_OK, "stdole2.tlb loads: %#lx", hr);
    if (hr) { printf("RESULT: FAIL\n"); return 1; }
    for (i = 0; i < ARRAYSIZE(names); i++)
    {
        WCHAR buf[64];
        BOOL found = 2;
        ITypeInfo *ti[4] = {0};
        MEMBERID ids[4];
        USHORT count = 4, j;

        lstrcpyW(buf, names[i].ask);
        hr = ITypeLib_IsName(tl, buf, 0, &found);
        check(hr == S_OK && found == names[i].found && !lstrcmpW(buf, names[i].want), "IsName(%ls): %#lx %d \"%ls\"",
              names[i].ask, hr, found, buf);
        lstrcpyW(buf, names[i].ask);
        hr = ITypeLib_FindName(tl, buf, 0, ti, ids, &count);
        check(hr == S_OK && (names[i].found ? count >= 1 : count == 0) && !lstrcmpW(buf, names[i].want),
              "FindName(%ls): %#lx %u \"%ls\"", names[i].ask, hr, count, buf);
        for (j = 0; j < count; j++) ITypeInfo_Release(ti[j]);
    }
    ITypeLib_Release(tl);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
