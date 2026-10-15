/* Probe for patches/sg/5077: IPropertyStorage::SetTimes on a (simple) property set of a
 * compound file succeeds, and the set keeps no times (ole32 stg_prop tests, run on Windows).
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ole2.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

DEFINE_GUID(FMTID_SummaryInformation_, 0xf29f85e0, 0x4ff9, 0x1068, 0xab, 0x91, 0x08, 0x00, 0x2b, 0x27, 0xb3, 0xd9);

int main(void)
{
    static const struct { int c, a, m; } rows[] = { {0, 0, 1}, {1, 1, 1}, {1, 0, 0}, {0, 0, 0} };
    IPropertySetStorage *pss = NULL;
    IPropertyStorage *ps = NULL;
    STATPROPSETSTG stat;
    FILETIME t = {1, 0};
    IStorage *stg = NULL;
    unsigned int i;
    HRESULT hr;

    CoInitialize(NULL);
    hr = StgCreateDocfile(NULL, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_DELETEONRELEASE, 0, &stg);
    check(hr == S_OK, "StgCreateDocfile %#lx", hr);
    if (stg) IStorage_QueryInterface(stg, &IID_IPropertySetStorage, (void **)&pss);
    if (pss) hr = IPropertySetStorage_Create(pss, &FMTID_SummaryInformation_, NULL, PROPSETFLAG_DEFAULT,
                                             STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, &ps);
    check(ps != NULL, "a property set %#lx", hr);
    if (!ps) goto done;
    for (i = 0; i < ARRAYSIZE(rows); i++)
    {
        hr = IPropertyStorage_SetTimes(ps, rows[i].c ? &t : NULL, rows[i].a ? &t : NULL, rows[i].m ? &t : NULL);
        check(hr == S_OK, "SetTimes %u: %#lx", i, hr);
    }
    memset(&stat, 0xcc, sizeof(stat));
    hr = IPropertyStorage_Stat(ps, &stat);
    check(hr == S_OK && IsEqualGUID(&stat.fmtid, &FMTID_SummaryInformation_) && stat.mtime.dwLowDateTime != 1,
          "Stat %#lx: the time given is not kept (mtime %#lx)", hr, stat.mtime.dwLowDateTime);
    IPropertyStorage_Release(ps);
done:
    if (pss) IPropertySetStorage_Release(pss);
    if (stg) IStorage_Release(stg);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
