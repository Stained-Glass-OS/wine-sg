/* shell32 long tail (patches/sg/2001): IApplicationAssociationRegistration's
 * SetAppAsDefault and SetAppAsDefaultAll, run by test/appdefault-gate.sh.
 * These were E_NOTIMPL stubs; now they look the calling app up under
 * HKCU/HKLM\Software\RegisteredApplications, read its Capabilities'
 * FileAssociations/URLAssociations, and record the ProgId they name as the
 * user's UserChoice for the extension/protocol, exactly as "Always use this
 * app" in the Open With dialog (dlls/shell32/openwith.c) already does by
 * hand for one extension at a time.
 *
 *   appdefault-probe.exe */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <shobjidl.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static void registry_setup(void)
{
    HKEY key;

    /* A fake registered application, SGProbeApp, with one file extension
     * and one URL protocol capability -- as a real installer would under
     * RegisteredApplications + Capabilities. */
    RegCreateKeyExW(HKEY_LOCAL_MACHINE,
            L"Software\\Clients\\SGProbeApp\\Capabilities\\FileAssociations",
            0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
    RegSetValueExW(key, L".sgprobe", 0, REG_SZ, (const BYTE *)L"SGProbeApp.sgprobe",
            (lstrlenW(L"SGProbeApp.sgprobe") + 1) * sizeof(WCHAR));
    RegCloseKey(key);

    RegCreateKeyExW(HKEY_LOCAL_MACHINE,
            L"Software\\Clients\\SGProbeApp\\Capabilities\\URLAssociations",
            0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
    RegSetValueExW(key, L"sgprobe-proto", 0, REG_SZ, (const BYTE *)L"SGProbeApp.proto",
            (lstrlenW(L"SGProbeApp.proto") + 1) * sizeof(WCHAR));
    RegCloseKey(key);

    RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\RegisteredApplications",
            0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
    RegSetValueExW(key, L"SGProbeApp", 0, REG_SZ,
            (const BYTE *)L"Software\\Clients\\SGProbeApp\\Capabilities",
            (lstrlenW(L"Software\\Clients\\SGProbeApp\\Capabilities") + 1) * sizeof(WCHAR));
    RegCloseKey(key);

    /* The ProgIds themselves must exist under HKCR for a lookup to treat
     * them as real classes (as SHELL_GetUserChoice checks). */
    RegCreateKeyExW(HKEY_CLASSES_ROOT, L"SGProbeApp.sgprobe", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
    RegCloseKey(key);
    RegCreateKeyExW(HKEY_CLASSES_ROOT, L"SGProbeApp.proto", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
    RegCloseKey(key);
}

static BOOL user_choice_is(const WCHAR *path, const WCHAR *expect)
{
    WCHAR value[MAX_PATH];
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, path, L"ProgId", RRF_RT_REG_SZ, NULL, value, &size))
        return FALSE;
    return !lstrcmpW(value, expect);
}

int main(void)
{
    IApplicationAssociationRegistration *reg;
    HRESULT hr;

    CoInitialize(NULL);
    registry_setup();

    hr = CoCreateInstance(&CLSID_ApplicationAssociationRegistration, NULL, CLSCTX_INPROC_SERVER,
            &IID_IApplicationAssociationRegistration, (void **)&reg);
    check(hr == S_OK, "IApplicationAssociationRegistration created");
    if (hr != S_OK) goto done;

    hr = IApplicationAssociationRegistration_SetAppAsDefault(reg, L"SGProbeApp", L".sgprobe", AT_FILEEXTENSION);
    check(hr == S_OK, "SetAppAsDefault(.sgprobe) succeeds");
    check(user_choice_is(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.sgprobe\\UserChoice",
            L"SGProbeApp.sgprobe"), "it recorded the app's ProgId as the UserChoice");

    hr = IApplicationAssociationRegistration_SetAppAsDefault(reg, L"SGProbeApp", L"sgprobe-proto", AT_URLPROTOCOL);
    check(hr == S_OK, "SetAppAsDefault(sgprobe-proto) succeeds");
    check(user_choice_is(L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\sgprobe-proto\\UserChoice",
            L"SGProbeApp.proto"), "it recorded the protocol's ProgId as the UserChoice");

    hr = IApplicationAssociationRegistration_SetAppAsDefault(reg, L"SGProbeApp", L".notdeclared", AT_FILEEXTENSION);
    check(FAILED(hr), "an association the app never declared fails");

    hr = IApplicationAssociationRegistration_SetAppAsDefault(reg, L"NoSuchApp", L".sgprobe", AT_FILEEXTENSION);
    check(FAILED(hr), "an unregistered app name fails");

    /* Reset, then SetAppAsDefaultAll should pick up both associations. */
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.sgprobe\\UserChoice");
    hr = IApplicationAssociationRegistration_SetAppAsDefaultAll(reg, L"SGProbeApp");
    check(hr == S_OK, "SetAppAsDefaultAll succeeds");
    check(user_choice_is(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.sgprobe\\UserChoice",
            L"SGProbeApp.sgprobe"), "SetAppAsDefaultAll set the file extension too");
    check(user_choice_is(L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\sgprobe-proto\\UserChoice",
            L"SGProbeApp.proto"), "SetAppAsDefaultAll set the protocol too");

    IApplicationAssociationRegistration_Release(reg);
done:
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
