/* printersd-probe (wine-sg 8005): GetSecurityInfo/SetSecurityInfo and GetNamedSecurityInfo/
 * SetNamedSecurityInfo of SE_PRINTER objects are the spooler's printer descriptor (PRINTER_INFO_3).
 * GetSecurityInfo failed (ERROR_CALL_NOT_IMPLEMENTED), the named Get returned success with nothing, and
 * both Set functions reported success without setting anything. */
#include <windows.h>
#include <winspool.h>
#include <aclapi.h>
#include <sddl.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static BOOL dacl_text(PACL dacl, char *out, DWORD size)
{
    SECURITY_DESCRIPTOR sd;
    char *s;
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, dacl, FALSE);
    if (!ConvertSecurityDescriptorToStringSecurityDescriptorA(&sd, SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &s, NULL))
        return FALSE;
    lstrcpynA(out, s, size);
    LocalFree(s);
    return TRUE;
}

int main(void)
{
    static WCHAR name[] = L"SgSdPrinter";
    PSECURITY_DESCRIPTOR sd = NULL, newsd;
    PSID owner = NULL, admins = NULL;
    PACL dacl = NULL, newdacl;
    BOOL present, defaulted;
    char text[512], text2[512];
    HANDLE h = NULL;
    DWORD err, needed;
    BYTE buf[4096];
    HKEY key;

    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Control\\Print\\Printers\\SgSdPrinter", 0, NULL, 0,
                        KEY_ALL_ACCESS, NULL, &key, NULL))
    { printf("FAIL the printer key\nRESULT: FAIL\n"); return 1; }
    RegSetValueExA(key, "Name", 0, REG_SZ, (BYTE *)"SgSdPrinter", 12);
    RegSetValueExA(key, "Port", 0, REG_SZ, (BYTE *)"FILE:", 6);
    RegSetValueExA(key, "Print Processor", 0, REG_SZ, (BYTE *)"winprint", 9);
    RegSetValueExA(key, "Datatype", 0, REG_SZ, (BYTE *)"RAW", 4);
    RegCloseKey(key);
    check(OpenPrinterW(name, &h, NULL), "OpenPrinter (%lu)", h ? 0 : GetLastError());
    if (!h) { printf("RESULT: FAIL\n"); return 1; }
    ConvertStringSidToSidA("BA", &admins);

    err = GetSecurityInfo(h, SE_PRINTER, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner, NULL, &dacl, NULL, &sd);
    check(!err && owner && dacl && sd, "GetSecurityInfo(SE_PRINTER): %lu", err);
    if (!err)
    {
        check(EqualSid(owner, admins), "the owner is Administrators");
        check(dacl->AceCount >= 2 && dacl_text(dacl, text, sizeof(text)), "the DACL: %s", text);
        LocalFree(sd);
    }

    sd = NULL; dacl = NULL;
    err = GetNamedSecurityInfoW(name, SE_PRINTER, DACL_SECURITY_INFORMATION, NULL, NULL, &dacl, NULL, &sd);
    check(!err && dacl && sd, "GetNamedSecurityInfo(SE_PRINTER) by name: %lu", err);
    if (sd) LocalFree(sd);

    /* a new DACL by name; the owner stays */
    ConvertStringSecurityDescriptorToSecurityDescriptorA("D:(A;;GA;;;BA)(A;;SWRC;;;BU)", SDDL_REVISION_1, &newsd, NULL);
    GetSecurityDescriptorDacl(newsd, &present, &newdacl, &defaulted);
    dacl_text(newdacl, text, sizeof(text));
    err = SetNamedSecurityInfoW(name, SE_PRINTER, DACL_SECURITY_INFORMATION, NULL, NULL, newdacl, NULL);
    check(!err, "SetNamedSecurityInfo(SE_PRINTER, DACL): %lu", err);
    sd = NULL; dacl = NULL; owner = NULL;
    err = GetSecurityInfo(h, SE_PRINTER, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner, NULL, &dacl, NULL, &sd);
    text2[0] = 0;
    if (!err) dacl_text(dacl, text2, sizeof(text2));
    check(!err && !strcmp(text, text2), "the printer's DACL is the new one: %s", text2);
    check(!err && owner && EqualSid(owner, admins), "its owner is kept");
    if (sd) LocalFree(sd);

    /* the spooler has it too */
    check(GetPrinterW(h, 3, buf, sizeof(buf), &needed) && ((PRINTER_INFO_3 *)buf)->pSecurityDescriptor &&
          GetSecurityDescriptorDacl(((PRINTER_INFO_3 *)buf)->pSecurityDescriptor, &present, &dacl, &defaulted) &&
          dacl_text(dacl, text2, sizeof(text2)) && !strcmp(text, text2), "GetPrinter level 3 gives the same DACL");

    /* the owner alone through the handle; the DACL stays */
    err = SetSecurityInfo(h, SE_PRINTER, OWNER_SECURITY_INFORMATION, admins, NULL, NULL, NULL);
    check(!err, "SetSecurityInfo(SE_PRINTER, owner): %lu", err);
    sd = NULL; dacl = NULL;
    err = GetSecurityInfo(h, SE_PRINTER, DACL_SECURITY_INFORMATION, NULL, NULL, &dacl, NULL, &sd);
    text2[0] = 0;
    if (!err) dacl_text(dacl, text2, sizeof(text2));
    check(!err && !strcmp(text, text2), "the DACL is kept when only the owner is set");
    if (sd) LocalFree(sd);

    ClosePrinter(h);
    RegDeleteKeyA(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Control\\Print\\Printers\\SgSdPrinter");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
