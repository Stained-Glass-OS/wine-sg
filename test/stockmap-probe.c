/* stockmap-probe: SHGetStockIconInfo's icons are shell32's own drawings of them (wine-sg 9819) --
 * the Print dialog's printers (SIID_PRINTER), folders, drives, the recycle bin -- not all the plain file. */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <wchar.h>

static int failures;
#define CHECK(c, ...) do { int ok_ = !!(c); printf(ok_ ? "PASS  " : "FAIL  "); printf(__VA_ARGS__); printf("\n"); \
                           failures += !ok_; fflush(stdout); } while (0)

static int location(int id)
{
    SHSTOCKICONINFO sii;

    memset(&sii, 0, sizeof(sii));
    sii.cbSize = sizeof(sii);
    if (FAILED(SHGetStockIconInfo(id, SHGSI_ICONLOCATION, &sii)) || !wcsstr(sii.szPath, L"shell32.dll")) return 1;
    return sii.iIcon;
}

int main(void)
{
    static const struct { int id; int res; const char *what; } icons[] =
    {
        { 16 /* SIID_PRINTER */,     -17, "the printer" },
        { 3  /* SIID_FOLDER */,      -4,  "the folder" },
        { 8  /* SIID_DRIVEFIXED */,  -9,  "the fixed drive" },
        { 11 /* SIID_DRIVECD */,     -12, "the CD drive" },
        { 31 /* SIID_RECYCLER */,    -32, "the empty recycle bin" },
        { 32 /* SIID_RECYCLERFULL */,-33, "the full recycle bin" },
        { 29 /* SIID_LINK */,        -30, "the shortcut overlay" },
        { 0  /* SIID_DOCNOASSOC */,  -1,  "the plain file" },
    };
    unsigned int i;

    for (i = 0; i < ARRAYSIZE(icons); i++)
    {
        int res = location(icons[i].id);
        CHECK(res == icons[i].res, "stock icon %d is %s (shell32,%d; got %d)", icons[i].id, icons[i].what,
              icons[i].res, res);
    }
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures ? 1 : 0;
}
