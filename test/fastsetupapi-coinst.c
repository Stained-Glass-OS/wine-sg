/* test co-installer for fastsetupapi-probe: asks for post-processing and records what it sees in
 * HKLM\Software\SgCoinst. The value PostReturn (DWORD) is what post-processing returns. */
#include <windows.h>
#include <setupapi.h>

static void bump(const char *name, DWORD set)
{
    HKEY key;
    DWORD value = 0, size = sizeof(value), type;

    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\SgCoinst", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL)) return;
    if (!set)
    {
        RegQueryValueExA(key, name, NULL, &type, (BYTE *)&value, &size);
        value++;
    }
    else value = set;
    RegSetValueExA(key, name, 0, REG_DWORD, (BYTE *)&value, sizeof(value));
    RegCloseKey(key);
}

DWORD CALLBACK CoDeviceInstall(DI_FUNCTION function, HDEVINFO devinfo, PSP_DEVINFO_DATA data, PCOINSTALLER_CONTEXT_DATA ctx)
{
    HKEY key;
    DWORD ret = 0, size = sizeof(ret), type;

    if (!ctx->PostProcessing)
    {
        bump("Pre", 0);
        ctx->PrivateData = (PVOID)(ULONG_PTR)0x1234;
        return ERROR_DI_POSTPROCESSING_REQUIRED;
    }
    bump("Post", 0);
    bump("PostInstallResult", ctx->InstallResult + 1000);
    if (ctx->PrivateData == (PVOID)(ULONG_PTR)0x1234) bump("PrivateOK", 0);
    if (!RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\SgCoinst", 0, KEY_QUERY_VALUE, &key))
    {
        if (RegQueryValueExA(key, "PostReturn", NULL, &type, (BYTE *)&ret, &size)) ret = 0;
        RegCloseKey(key);
    }
    return ret;
}
