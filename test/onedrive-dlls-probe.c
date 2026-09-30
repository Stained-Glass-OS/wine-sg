/* Probe for cldapi.dll (Cloud Files API) and cryptxml.dll (XML DigSig API),
 * the two modules OneDrive's sync client imports that Wine did not provide
 * (patches/sg/0518).  Drives them the way OneDrive's loader and FileSyncFALWB
 * do: every export must resolve, CfGetPlatformInfo must report the running
 * Windows build, and the not-yet-implemented calls must fail cleanly rather
 * than claim success.  Output is one "key value" line per check. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

struct platform_info { DWORD build, revision, integration; };

static const char *cld_exports[] = {
    "CfCloseHandle","CfConnectSyncRoot","CfConvertToPlaceholder","CfCreatePlaceholders",
    "CfDisconnectSyncRoot","CfExecute","CfGetPlaceholderInfo","CfGetPlaceholderRangeInfo",
    "CfGetPlaceholderStateFromAttributeTag","CfGetPlatformInfo","CfGetTransferKey",
    "CfGetWin32HandleFromProtectedHandle","CfHydratePlaceholder","CfOpenFileWithOplock",
    "CfReferenceProtectedHandle","CfRegisterSyncRoot","CfReleaseProtectedHandle",
    "CfReportProviderProgress","CfRevertPlaceholder","CfSetInSyncState","CfSetPinState",
    "CfUnregisterSyncRoot","CfUpdatePlaceholder", NULL };
static const char *xml_exports[] = {
    "CryptXmlClose","CryptXmlGetDocContext","CryptXmlGetReference","CryptXmlGetSignature",
    "CryptXmlGetStatus","CryptXmlOpenToDecode","CryptXmlVerifySignature", NULL };

static int resolved(HMODULE mod, const char **names)
{
    int n = 0;
    for (; *names; names++) if (GetProcAddress(mod, *names)) n++; else printf("missing %s\n", *names);
    return n;
}

int main(void)
{
    HMODULE cld = LoadLibraryA("cldapi.dll");
    HMODULE xml = LoadLibraryA("cryptxml.dll");
    printf("load %d %d\n", cld != NULL, xml != NULL);
    if (!cld || !xml) return 0;

    printf("cldexports %d 23\n", resolved(cld, cld_exports));
    printf("xmlexports %d 7\n", resolved(xml, xml_exports));

    /* CfGetPlatformInfo must succeed and report the running build. */
    HRESULT (WINAPI *pCfGetPlatformInfo)(struct platform_info *) =
        (void *)GetProcAddress(cld, "CfGetPlatformInfo");
    struct platform_info pi = { 0, 0, 0 };
    HRESULT hr = pCfGetPlatformInfo(&pi);
    RTL_OSVERSIONINFOEXW ver = { .dwOSVersionInfoSize = sizeof(ver) };
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    NTSTATUS (WINAPI *pRtlGetVersion)(RTL_OSVERSIONINFOEXW *) =
        (void *)GetProcAddress(nt, "RtlGetVersion");
    pRtlGetVersion(&ver);
    printf("platform %08lx %lu %lu\n", (unsigned long)hr,
           (unsigned long)pi.build, (unsigned long)ver.dwBuildNumber);

    /* Close of nothing is a no-op success; an unimplemented op fails cleanly. */
    HRESULT (WINAPI *pCryptXmlClose)(void *) = (void *)GetProcAddress(xml, "CryptXmlClose");
    printf("xmlclose %08lx\n", (unsigned long)pCryptXmlClose(NULL));
    HRESULT (WINAPI *pCfRegisterSyncRoot)(const WCHAR *, const void *, const void *, DWORD) =
        (void *)GetProcAddress(cld, "CfRegisterSyncRoot");
    printf("register %08lx\n", (unsigned long)pCfRegisterSyncRoot(L"C:\\od", NULL, NULL, 0));
    return 0;
}
