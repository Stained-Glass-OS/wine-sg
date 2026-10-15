/* Probe for patches/sg/5203: logical colour spaces and ICM state on a DC.
 *   - every DC starts with a default sRGB colour space (GetColorSpace, GetCurrentObject),
 *   - CreateColorSpaceA/W make an OBJ_COLORSPACE object GetLogColorSpaceA/W read back,
 *   - SetColorSpace / SelectObject select it and return the one replaced, SaveDC/RestoreDC keep it,
 *   - DeleteColorSpace only deletes colour spaces,
 *   - SetICMMode keeps ICM_ON/ICM_OFF per DC, SetICMProfile is what GetICMProfile then returns.
 * Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#undef LCS_SIGNATURE
#define LCS_SIGNATURE 0x50534f43
#ifndef ICM_DONE_OUTSIDEDC
#define ICM_DONE_OUTSIDEDC 4
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const struct { int mode; int is_valid; } icm_modes[] =
{
    { ICM_ON, 1 }, { ICM_OFF, 1 }, { ICM_DONE_OUTSIDEDC, 1 }, { ICM_OFF, 1 }, { 0, 0 }, { -1, 0 }, { 5, 0 }, { 100, 0 },
};

static void test_dc(const char *name, HDC dc)
{
    LOGCOLORSPACEW lw, def, mine;
    LOGCOLORSPACEA la;
    HCOLORSPACE defcs, cs, cs2, old;
    HGDIOBJ pen;
    unsigned int i;
    WCHAR buf[MAX_PATH];
    char bufa[MAX_PATH];
    DWORD size, err;
    INT ret, cur;
    BOOL ok;

    defcs = GetColorSpace(dc);
    check(defcs != NULL, "%s: default colour space exists", name);
    check(GetObjectType(defcs) == OBJ_COLORSPACE, "%s: default is OBJ_COLORSPACE (%lu)", name, GetObjectType(defcs));
    check(GetCurrentObject(dc, OBJ_COLORSPACE) == defcs, "%s: GetCurrentObject agrees", name);
    memset(&def, 0xcc, sizeof(def));
    ok = GetLogColorSpaceW(defcs, &def, sizeof(def));
    check(ok, "%s: GetLogColorSpaceW(default) %lu", name, GetLastError());
    check(def.lcsSignature == LCS_SIGNATURE && def.lcsVersion == 0x400 && def.lcsSize == sizeof(def),
          "%s: signature %08lx version %08lx size %lu", name, def.lcsSignature, def.lcsVersion, def.lcsSize);
    check(def.lcsCSType == 0x73524742 && def.lcsIntent == LCS_GM_IMAGES, "%s: sRGB, images intent (%08lx, %ld)",
          name, def.lcsCSType, def.lcsIntent);
    memset(&la, 0xcc, sizeof(la));
    ok = GetLogColorSpaceA(defcs, &la, sizeof(la));
    check(ok && la.lcsSize == sizeof(la) && la.lcsCSType == def.lcsCSType, "%s: GetLogColorSpaceA(default)", name);
    SetLastError(0xdeadbeef);
    ok = GetLogColorSpaceW(defcs, &lw, sizeof(lw) - 1);
    err = GetLastError();
    check(!ok && err == ERROR_INVALID_PARAMETER, "%s: small buffer: %d %lu", name, ok, err);
    pen = GetStockObject(BLACK_PEN);
    SetLastError(0xdeadbeef);
    ok = GetLogColorSpaceW(pen, &lw, sizeof(lw));
    err = GetLastError();
    check(!ok && err == ERROR_INVALID_HANDLE, "%s: pen is not a colour space: %d %lu", name, ok, err);

    /* create */
    memset(&mine, 0, sizeof(mine));
    mine.lcsSignature = LCS_SIGNATURE; mine.lcsVersion = 0x400; mine.lcsSize = sizeof(mine);
    mine.lcsCSType = LCS_CALIBRATED_RGB; mine.lcsIntent = LCS_GM_GRAPHICS;
    mine.lcsEndpoints.ciexyzRed.ciexyzX = 1; mine.lcsEndpoints.ciexyzGreen.ciexyzY = 2; mine.lcsEndpoints.ciexyzBlue.ciexyzZ = 3;
    mine.lcsGammaRed = 0x10000; mine.lcsGammaGreen = 0x20000; mine.lcsGammaBlue = 0x30000;
    lstrcpyW(mine.lcsFilename, L"mine.icm");
    cs = CreateColorSpaceW(&mine);
    check(cs != NULL && cs != defcs, "%s: CreateColorSpaceW %lu", name, GetLastError());
    check(GetObjectType(cs) == OBJ_COLORSPACE, "%s: created object is OBJ_COLORSPACE", name);
    memset(&lw, 0xcc, sizeof(lw));
    ok = GetLogColorSpaceW(cs, &lw, sizeof(lw));
    check(ok && !memcmp(&lw, &mine, sizeof(lw)), "%s: GetLogColorSpaceW reads back what was created", name);

    memset(&la, 0, sizeof(la));
    la.lcsSignature = LCS_SIGNATURE; la.lcsVersion = 0x400; la.lcsSize = sizeof(la);
    la.lcsCSType = LCS_CALIBRATED_RGB; la.lcsIntent = LCS_GM_BUSINESS; la.lcsGammaRed = 7;
    lstrcpyA(la.lcsFilename, "other.icm");
    cs2 = CreateColorSpaceA(&la);
    check(cs2 != NULL && cs2 != cs, "%s: CreateColorSpaceA", name);
    ok = GetLogColorSpaceW(cs2, &lw, sizeof(lw));
    check(ok && lw.lcsSize == sizeof(lw) && lw.lcsIntent == LCS_GM_BUSINESS && lw.lcsGammaRed == 7 && !lstrcmpW(lw.lcsFilename, L"other.icm"),
          "%s: A structure converted (%ls)", name, lw.lcsFilename);
    memset(&la, 0xcc, sizeof(la));
    ok = GetLogColorSpaceA(cs2, &la, sizeof(la));
    check(ok && la.lcsSize == sizeof(la) && !lstrcmpA(la.lcsFilename, "other.icm"), "%s: GetLogColorSpaceA(%s)", name, la.lcsFilename);
    SetLastError(0xdeadbeef);
    check(CreateColorSpaceW(NULL) == NULL && GetLastError() == ERROR_INVALID_PARAMETER, "%s: CreateColorSpaceW(NULL)", name);
    SetLastError(0xdeadbeef);
    check(CreateColorSpaceA(NULL) == NULL && GetLastError() == ERROR_INVALID_PARAMETER, "%s: CreateColorSpaceA(NULL)", name);

    /* select */
    old = SetColorSpace(dc, cs);
    check(old == defcs, "%s: SetColorSpace returns the replaced one %p/%p", name, old, defcs);
    check(GetColorSpace(dc) == cs && GetCurrentObject(dc, OBJ_COLORSPACE) == cs, "%s: selected", name);
    old = SelectObject(dc, cs2);
    check(old == cs, "%s: SelectObject returns the replaced one %p/%p", name, old, cs);
    check(GetColorSpace(dc) == cs2, "%s: SelectObject selected it", name);
    SaveDC(dc);
    SetColorSpace(dc, cs);
    check(GetColorSpace(dc) == cs, "%s: changed after SaveDC", name);
    RestoreDC(dc, -1);
    check(GetColorSpace(dc) == cs2, "%s: RestoreDC brings the saved colour space back", name);
    SetLastError(0xdeadbeef);
    check(SetColorSpace(dc, NULL) == NULL && GetLastError() == ERROR_INVALID_HANDLE, "%s: SetColorSpace(NULL) fails (%lu)", name, GetLastError());
    SetLastError(0xdeadbeef);
    check(SetColorSpace(dc, pen) == NULL, "%s: a pen is no colour space", name);
    check(SetColorSpace(NULL, cs) == NULL, "%s: no DC", name);
    check(GetColorSpace(dc) == cs2, "%s: failed calls changed nothing", name);

    /* delete */
    SetColorSpace(dc, defcs);
    SetLastError(0xdeadbeef);
    ok = DeleteColorSpace(pen);
    err = GetLastError();
    check(!ok && err == ERROR_INVALID_HANDLE, "%s: DeleteColorSpace(pen): %d %lu", name, ok, err);
    check(DeleteColorSpace(cs), "%s: DeleteColorSpace", name);
    check(DeleteColorSpace(cs2), "%s: DeleteColorSpace (A)", name);
    SetLastError(0xdeadbeef);
    check(GetObjectType(cs) == 0, "%s: deleted handle is gone", name);
    DeleteColorSpace(defcs);
    check(GetObjectType(defcs) == OBJ_COLORSPACE && GetColorSpace(dc) == defcs, "%s: the default colour space stays", name);

    /* ICM mode */
    cur = SetICMMode(dc, ICM_QUERY);
    check(cur == ICM_OFF, "%s: a new DC has ICM off (%d)", name, cur);
    for (i = 0; i < ARRAYSIZE(icm_modes); i++)
    {
        SetLastError(0xdeadbeef);
        ret = SetICMMode(dc, icm_modes[i].mode);
        if (icm_modes[i].is_valid)
        {
            check(ret != 0, "%s: SetICMMode(%d) = %d", name, icm_modes[i].mode, ret);
            check(SetICMMode(dc, ICM_QUERY) == icm_modes[i].mode, "%s: query after %d", name, icm_modes[i].mode);
        }
        else
        {
            check(ret == 0, "%s: SetICMMode(%d) = %d is refused", name, icm_modes[i].mode, ret);
        }
    }
    check(SetICMMode(NULL, ICM_ON) == 0, "%s: SetICMMode without a DC", name);

    /* ICM profile */
    size = ARRAYSIZE(buf);
    ok = GetICMProfileW(dc, &size, buf);
    SetICMMode(dc, ICM_OFF);
    SetLastError(0xdeadbeef);
    check(!SetICMProfileW(dc, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "%s: SetICMProfileW(NULL)", name);
    SetLastError(0xdeadbeef);
    check(!SetICMProfileW(NULL, (WCHAR *)L"x.icm") && GetLastError() == ERROR_INVALID_HANDLE, "%s: SetICMProfileW(no DC)", name);
    SetLastError(0xdeadbeef);
    check(!SetICMProfileW((HDC)0xdeadbeef, (WCHAR *)L"x.icm") && GetLastError() == ERROR_INVALID_HANDLE, "%s: SetICMProfileW(bad DC)", name);
    check(SetICMProfileW(dc, (WCHAR *)L"C:\\profiles\\my.icm"), "%s: SetICMProfileW", name);
    size = ARRAYSIZE(buf);
    memset(buf, 0, sizeof(buf));
    ok = GetICMProfileW(dc, &size, buf);
    check(ok && !lstrcmpW(buf, L"C:\\profiles\\my.icm") && size == lstrlenW(L"C:\\profiles\\my.icm") + 1,
          "%s: GetICMProfileW returns it: %ls (%lu)", name, buf, size);
    size = 5;
    SetLastError(0xdeadbeef);
    ok = GetICMProfileW(dc, &size, buf);
    err = GetLastError();
    check(!ok && err == ERROR_INSUFFICIENT_BUFFER && size == lstrlenW(L"C:\\profiles\\my.icm") + 1,
          "%s: short buffer: %d %lu size %lu", name, ok, err, size);
    check(SetICMProfileA(dc, (char *)"D:\\a\\other.icm"), "%s: SetICMProfileA", name);
    size = ARRAYSIZE(bufa);
    ok = GetICMProfileA(dc, &size, bufa);
    check(ok && !lstrcmpA(bufa, "D:\\a\\other.icm"), "%s: GetICMProfileA returns it: %s", name, bufa);
    SetLastError(0xdeadbeef);
    check(!SetICMProfileA(dc, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "%s: SetICMProfileA(NULL)", name);
}

/* records played one by one with PlayEnhMetaFileRecord */
static void test_records(const char *name, HDC dc)
{
    struct { EMR emr; DWORD ihCS; LOGCOLORSPACEW lcs; DWORD dwFlags; DWORD cbData; BYTE Data[1]; } create;
    struct { EMR emr; DWORD ihCS; } set;
    struct { EMR emr; DWORD dwFlags; DWORD cbName; DWORD cbData; BYTE Data[64]; } icm;
    HANDLETABLE *ht = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*ht) + 3 * sizeof(HGDIOBJ));
    HCOLORSPACE old = GetColorSpace(dc);
    LOGCOLORSPACEW lw;
    WCHAR bufw[MAX_PATH];
    char bufa[MAX_PATH];
    DWORD size;
    BOOL ok;

    memset(&create, 0, sizeof(create));
    create.emr.iType = EMR_CREATECOLORSPACEW;
    create.emr.nSize = sizeof(create);
    create.ihCS = 1;
    create.lcs.lcsSignature = LCS_SIGNATURE; create.lcs.lcsVersion = 0x400; create.lcs.lcsSize = sizeof(create.lcs);
    create.lcs.lcsCSType = LCS_CALIBRATED_RGB; create.lcs.lcsIntent = LCS_GM_ABS_COLORIMETRIC;
    lstrcpyW(create.lcs.lcsFilename, L"rec.icm");
    ok = PlayEnhMetaFileRecord(dc, ht, (const ENHMETARECORD *)&create.emr, 3);
    check(ok && ht->objectHandle[1] && GetObjectType(ht->objectHandle[1]) == OBJ_COLORSPACE, "%s: EMR_CREATECOLORSPACEW makes a colour space", name);
    ok = GetLogColorSpaceW(ht->objectHandle[1], &lw, sizeof(lw));
    check(ok && lw.lcsIntent == LCS_GM_ABS_COLORIMETRIC && !lstrcmpW(lw.lcsFilename, L"rec.icm"), "%s: with the record's data", name);
    set.emr.iType = EMR_SETCOLORSPACE; set.emr.nSize = sizeof(set); set.ihCS = 1;
    PlayEnhMetaFileRecord(dc, ht, (const ENHMETARECORD *)&set.emr, 3);
    check(GetColorSpace(dc) == ht->objectHandle[1], "%s: EMR_SETCOLORSPACE selects it", name);
    SetColorSpace(dc, old);
    create.ihCS = 7;
    ok = PlayEnhMetaFileRecord(dc, ht, (const ENHMETARECORD *)&create.emr, 3);
    check(ok && !ht->objectHandle[0], "%s: a handle index beyond the table is ignored", name);
    DeleteColorSpace(ht->objectHandle[1]);

    memset(&icm, 0, sizeof(icm));
    icm.emr.iType = EMR_SETICMPROFILEW;
    icm.cbName = sizeof(L"C:\\emf\\w.icm");
    memcpy(icm.Data, L"C:\\emf\\w.icm", icm.cbName);
    icm.emr.nSize = (FIELD_OFFSET(__typeof__(icm), Data) + icm.cbName + 3) & ~3;
    ok = PlayEnhMetaFileRecord(dc, ht, (const ENHMETARECORD *)&icm.emr, 3);
    size = ARRAYSIZE(bufw);
    check(ok && GetICMProfileW(dc, &size, bufw) && !lstrcmpW(bufw, L"C:\\emf\\w.icm"), "%s: EMR_SETICMPROFILEW sets the profile (%ls)", name, bufw);
    memset(&icm, 0, sizeof(icm));
    icm.emr.iType = EMR_SETICMPROFILEA;
    icm.cbName = sizeof("D:\\emf\\a.icm");
    memcpy(icm.Data, "D:\\emf\\a.icm", icm.cbName);
    icm.emr.nSize = (FIELD_OFFSET(__typeof__(icm), Data) + icm.cbName + 3) & ~3;
    ok = PlayEnhMetaFileRecord(dc, ht, (const ENHMETARECORD *)&icm.emr, 3);
    size = ARRAYSIZE(bufa);
    check(ok && GetICMProfileA(dc, &size, bufa) && !lstrcmpA(bufa, "D:\\emf\\a.icm"), "%s: EMR_SETICMPROFILEA sets the profile (%s)", name, bufa);
    HeapFree(GetProcessHeap(), 0, ht);
}

int main(void)
{
    HDC screen = GetDC(0), mem = CreateCompatibleDC(screen), dc2;
    HBITMAP bmp = CreateCompatibleBitmap(screen, 4, 4);

    SelectObject(mem, bmp);
    test_dc("screen", screen);
    test_dc("memory", mem);
    test_records("memory", mem);
    /* another DC does not see the first one's profile or colour space */
    dc2 = CreateCompatibleDC(screen);
    check(GetColorSpace(dc2) == GetColorSpace(screen), "new DC starts with the default colour space");
    check(SetICMMode(dc2, ICM_QUERY) == ICM_OFF, "new DC has ICM off");
    DeleteDC(dc2); DeleteDC(mem); DeleteObject(bmp); ReleaseDC(0, screen);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
