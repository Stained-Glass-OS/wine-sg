/* Probe for patches/sg/5049: GetColorAdjustment/SetColorAdjustment keep a
 * COLORADJUSTMENT per DC: a new DC gives the no-change values (unit gammas,
 * black 0, white 10000), a set one comes back, another DC keeps its own, and
 * values outside the documented member ranges are refused. Both always
 * returned FALSE. Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    static const struct { const char *what; int field; int value; BOOL ok; } sets[] =
    {
        {"contrast 50", 0, 50, TRUE}, {"contrast 101", 0, 101, FALSE}, {"brightness -100", 1, -100, TRUE},
        {"white 5999", 2, 5999, FALSE}, {"red gamma 65000", 3, 65000, TRUE}, {"illuminant 9", 4, 9, FALSE},
        {"flags CA_NEGATIVE", 5, CA_NEGATIVE, TRUE}, {"flags 0x10", 5, 0x10, FALSE},
    };
    HDC dc = CreateCompatibleDC(NULL), other = CreateCompatibleDC(NULL);
    COLORADJUSTMENT ca, got;
    unsigned int i;
    BOOL ret;

    memset(&got, 0x55, sizeof(got));
    ret = GetColorAdjustment(dc, &got);
    check(ret && got.caSize == sizeof(got) && got.caRedGamma == 10000 && got.caGreenGamma == 10000 && got.caBlueGamma == 10000
          && got.caReferenceBlack == 0 && got.caReferenceWhite == 10000 && !got.caContrast && !got.caBrightness && !got.caFlags,
          "a new DC: %d size %u gamma %u white %u", ret, got.caSize, got.caRedGamma, got.caReferenceWhite);
    for (i = 0; i < ARRAYSIZE(sets); i++)
    {
        GetColorAdjustment(dc, &ca);
        switch (sets[i].field)
        {
        case 0: ca.caContrast = sets[i].value; break;
        case 1: ca.caBrightness = sets[i].value; break;
        case 2: ca.caReferenceWhite = sets[i].value; break;
        case 3: ca.caRedGamma = sets[i].value; break;
        case 4: ca.caIlluminantIndex = sets[i].value; break;
        case 5: ca.caFlags = sets[i].value; break;
        }
        ret = SetColorAdjustment(dc, &ca);
        memset(&got, 0, sizeof(got));
        GetColorAdjustment(dc, &got);
        if (sets[i].ok) check(ret && !memcmp(&got, &ca, sizeof(ca)), "%s: set and read back (%d)", sets[i].what, ret);
        else check(!ret && memcmp(&got, &ca, sizeof(ca)), "%s: refused (%d)", sets[i].what, ret);
    }
    GetColorAdjustment(other, &got);
    check(got.caContrast == 0 && got.caBrightness == 0, "another DC keeps its own (%d %d)", got.caContrast, got.caBrightness);
    ca.caSize = 0;
    check(!SetColorAdjustment(dc, &ca), "a wrong size is refused");
    check(!GetColorAdjustment((HDC)0xdead, &got), "a bad DC fails");
    DeleteDC(dc); DeleteDC(other);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
