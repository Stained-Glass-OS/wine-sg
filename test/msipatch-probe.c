/* Probe for patches/sg/2452: the errors of MsiApplyMultiplePatches for lists that name nothing or a missing patch. */
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <stdio.h>
#include <string.h>

#ifndef ERROR_PATCH_PACKAGE_OPEN_FAILED
#define ERROR_PATCH_PACKAGE_OPEN_FAILED 1635
#endif

static int fails;
static void check(const char *name, UINT got, UINT want)
{
    printf("      %s  %s (%u, want %u)\n", got == want ? "PASS" : "FAIL", name, got, want);
    if (got != want) fails++;
}

int main(void)
{
    BOOL fixed = GetDriveTypeW(NULL) == DRIVE_FIXED;
    UINT blank_path = fixed ? ERROR_PATH_NOT_FOUND : ERROR_INVALID_NAME;
    UINT blank_open = fixed ? ERROR_PATCH_PACKAGE_OPEN_FAILED : ERROR_INVALID_NAME;

    check("NULL list", MsiApplyMultiplePatchesA(NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
    check("empty list", MsiApplyMultiplePatchesA("", NULL, NULL), ERROR_INVALID_PARAMETER);
    check("\";\"", MsiApplyMultiplePatchesA(";", NULL, NULL), blank_path);
    check("\"  ;\" (blanks)", MsiApplyMultiplePatchesA("  ;", NULL, NULL), blank_open);
    check("\";;\"", MsiApplyMultiplePatchesA(";;", NULL, NULL), blank_path);
    check("a missing patch", MsiApplyMultiplePatchesA("nosuchpatchpackage;", NULL, NULL), ERROR_FILE_NOT_FOUND);
    check("nothing, then a missing patch", MsiApplyMultiplePatchesA(";nosuchpatchpackage", NULL, NULL), blank_path);
    check("two missing patches", MsiApplyMultiplePatchesA("nosuchpatchpackage;nosuchpatchpackage", NULL, NULL), ERROR_FILE_NOT_FOUND);
    check("with blanks around", MsiApplyMultiplePatchesA("  nosuchpatchpackage  ;  nosuchpatchpackage  ", NULL, NULL), ERROR_FILE_NOT_FOUND);
    check("a missing directory", MsiApplyMultiplePatchesA("Z:\\no\\such\\dir\\x.msp", NULL, NULL), ERROR_PATH_NOT_FOUND);
    check("MsiApplyPatch of a missing patch", MsiApplyPatchA("nosuchpatchpackage", NULL, INSTALLTYPE_DEFAULT, NULL), ERROR_FILE_NOT_FOUND);
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
