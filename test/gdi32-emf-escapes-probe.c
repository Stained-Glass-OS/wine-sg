/* Probe for patches/sg/5222: PlayEnhMetaFileRecord plays EMR_EXTESCAPE, EMR_DRAWESCAPE and EMR_NAMEDESCAPE
 * (the gate checks the trace of the escape that reaches the DC), and accepts EMR_DRAWESCAPE, EMR_NAMEDESCAPE (a
 * damaged one is refused quietly), EMR_STARTDOC, EMR_FORCEUFIMAPPING and EMR_SETLINKEDUFI without
 * the "is unimplemented" fixme (the gate looks for it).  Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

/* record types of [MS-EMF] 2.1.1 (EMR_STARTDOC, 0x6B, is Wine's own name) */
#define EMR_DRAWESCAPE 0x69
#define EMR_EXTESCAPE 0x6a
#define EMR_STARTDOC 0x6b
#define EMR_FORCEUFIMAPPING 0x6d
#define EMR_NAMEDESCAPE 0x6e
#define EMR_SETLINKEDUFI 0x77

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static BYTE rec_buf[256];

static ENHMETARECORD *make_rec(DWORD type, const DWORD *params, unsigned int nparams, const void *extra, unsigned int extra_size)
{
    ENHMETARECORD *r = (ENHMETARECORD *)rec_buf;
    unsigned int size = 8 + nparams * 4 + ((extra_size + 3) & ~3);
    memset(rec_buf, 0, sizeof(rec_buf));
    r->iType = type;
    r->nSize = size;
    memcpy(r->dParm, params, nparams * 4);
    if (extra_size) memcpy((BYTE *)r->dParm + nparams * 4, extra, extra_size);
    return r;
}

int main(void)
{
    HDC mf = CreateCompatibleDC(NULL);
    HANDLETABLE ht[4] = {{0}};
    ENHMETARECORD *r;
    DWORD p[4];
    BOOL ret;
    static const char data[] = "escape-data";
    static const WCHAR driver[] = L"WINEDRV";
    BYTE buf[64];

    setvbuf(stdout, NULL, _IONBF, 0);
    check(mf != NULL, "memory DC");

    /* EXTESCAPE: iEscape, cbEscData, data */
    p[0] = 0x1234; p[1] = sizeof(data);
    r = make_rec(EMR_EXTESCAPE, p, 2, data, sizeof(data));
    ret = PlayEnhMetaFileRecord(mf, ht, r, 4);
    check(ret, "EMR_EXTESCAPE played");

    /* a damaged one (cbEscData larger than the record) is not passed on */
    p[0] = 0x4321; p[1] = 4000;
    r = make_rec(EMR_EXTESCAPE, p, 2, data, sizeof(data));
    ret = PlayEnhMetaFileRecord(mf, ht, r, 4);
    check(ret, "EMR_EXTESCAPE with a wrong size does not fail the call");

    /* the others */
    p[0] = 0x55; p[1] = sizeof(data);
    r = make_rec(EMR_DRAWESCAPE, p, 2, data, sizeof(data));
    check(PlayEnhMetaFileRecord(mf, ht, r, 4), "EMR_DRAWESCAPE");
    p[0] = 0x66; p[1] = sizeof(driver); p[2] = sizeof(data);
    memcpy(buf, driver, sizeof(driver)); memcpy(buf + sizeof(driver), data, sizeof(data));
    r = make_rec(EMR_NAMEDESCAPE, p, 3, buf, sizeof(driver) + sizeof(data));
    check(PlayEnhMetaFileRecord(mf, ht, r, 4), "EMR_NAMEDESCAPE");
    p[1] = 5; /* odd driver name size: refused quietly */
    r = make_rec(EMR_NAMEDESCAPE, p, 3, buf, sizeof(driver) + sizeof(data));
    check(PlayEnhMetaFileRecord(mf, ht, r, 4), "EMR_NAMEDESCAPE, damaged");
    p[0] = 1;
    r = make_rec(EMR_STARTDOC, p, 1, NULL, 0);
    check(PlayEnhMetaFileRecord(mf, ht, r, 4), "EMR_STARTDOC");
    p[0] = p[1] = 0x1234;
    r = make_rec(EMR_FORCEUFIMAPPING, p, 2, NULL, 0);
    check(PlayEnhMetaFileRecord(mf, ht, r, 4), "EMR_FORCEUFIMAPPING");
    p[0] = 0;
    r = make_rec(EMR_SETLINKEDUFI, p, 1, NULL, 0);
    check(PlayEnhMetaFileRecord(mf, ht, r, 4), "EMR_SETLINKEDUFI");

    DeleteDC(mf);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
