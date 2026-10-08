/* SHGetStockIconInfo's SHGSI_SYSICONINDEX, SHGSI_LINKOVERLAY and
 * SHGSI_SELECTED (patches/sg/1614): they were ignored with a FIXME. */
#define COBJMACROS
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

int main(void)
{
    SHSTOCKICONINFO plain = { sizeof(plain) }, link = { sizeof(link) }, sel = { sizeof(sel) };
    HRESULT hr;

    CoInitialize(NULL);
    hr = SHGetStockIconInfo(SIID_FOLDER, SHGSI_SYSICONINDEX, &plain);
    printf("folder: hr %#lx index %d\n", hr, plain.iSysImageIndex);
    check(hr == S_OK && plain.iSysImageIndex >= 0, "SHGSI_SYSICONINDEX gives the system image list index");
    hr = SHGetStockIconInfo(SIID_FOLDER, SHGSI_SYSICONINDEX | SHGSI_LINKOVERLAY, &link);
    printf("folder shortcut: hr %#lx index %d\n", hr, link.iSysImageIndex);
    check(hr == S_OK && link.iSysImageIndex >= 0 && link.iSysImageIndex != plain.iSysImageIndex,
          "SHGSI_LINKOVERLAY gives the shortcut-overlaid icon's index");
    hr = SHGetStockIconInfo(SIID_FOLDER, SHGSI_ICON | SHGSI_SELECTED | SHGSI_SMALLICON, &sel);
    check(hr == S_OK && sel.hIcon && sel.iSysImageIndex == -1, "SHGSI_SELECTED gives an icon (no index unasked)");
    if (sel.hIcon) DestroyIcon(sel.hIcon);
    hr = SHGetStockIconInfo(SIID_DOCNOASSOC, SHGSI_ICON | SHGSI_LARGEICON, &plain);
    check(hr == S_OK && plain.hIcon && plain.iSysImageIndex == -1, "a plain icon as before");
    if (plain.hIcon) DestroyIcon(plain.hIcon);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
