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


    /* Documented: the default only changes when the new program's publisher matches the
     * current default's, else E_ACCESSDENIED. Two programs whose open commands run the
     * same signed/versioned executable share a publisher; a program with no version
     * resource has none to match. */
    {
        HKEY key;
        WCHAR sys[MAX_PATH], cmd[MAX_PATH + 8];

        GetSystemDirectoryW(sys, MAX_PATH);
        wsprintfW(cmd, L"\"%s\\winver.exe\" %%1", sys);
        RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Clients\\SGProbeB\\Capabilities\\FileAssociations", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
        RegSetValueExW(key, L".sgprobe", 0, REG_SZ, (const BYTE *)L"SGProbeB.sgprobe", 34);
        RegSetValueExW(key, L".sgprobe2", 0, REG_SZ, (const BYTE *)L"SGProbeB.sgprobe2", 36);
        RegCloseKey(key);
        RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Clients\\SGProbeB\\Capabilities\\MimeAssociations", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
        RegSetValueExW(key, L"application/x-sgprobe", 0, REG_SZ, (const BYTE *)L"SGProbeB.sgprobe", 34);
        RegCloseKey(key);
        RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\RegisteredApplications", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
        RegSetValueExW(key, L"SGProbeB", 0, REG_SZ, (const BYTE *)L"Software\\Clients\\SGProbeB\\Capabilities", 80);
        RegCloseKey(key);
        RegCreateKeyExW(HKEY_CLASSES_ROOT, L"SGProbeB.sgprobe\\shell\\open\\command", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
        RegSetValueExW(key, NULL, 0, REG_SZ, (const BYTE *)cmd, (lstrlenW(cmd) + 1) * sizeof(WCHAR));
        RegCloseKey(key);
        RegCreateKeyExW(HKEY_CLASSES_ROOT, L"SGProbeB.sgprobe2", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
        RegCloseKey(key);
        /* SGProbeApp (command-less, no publisher) is the current default of .sgprobe */
        hr = IApplicationAssociationRegistration_SetAppAsDefault(reg, L"SGProbeB", L".sgprobe", AT_FILEEXTENSION);
        check(hr == E_ACCESSDENIED, "a different program without a matching publisher is refused");
        check(user_choice_is(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.sgprobe\\UserChoice",
                L"SGProbeApp.sgprobe"), "the default did not change");
        /* no current default for .sgprobe2: allowed */
        hr = IApplicationAssociationRegistration_SetAppAsDefault(reg, L"SGProbeB", L".sgprobe2", AT_FILEEXTENSION);
        check(hr == S_OK, "no current default: set");
        /* same publisher: make SGProbeApp's progid run the same notepad */
        RegCreateKeyExW(HKEY_CLASSES_ROOT, L"SGProbeApp.sgprobe\\shell\\open\\command", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
        RegSetValueExW(key, NULL, 0, REG_SZ, (const BYTE *)cmd, (lstrlenW(cmd) + 1) * sizeof(WCHAR));
        RegCloseKey(key);
        hr = IApplicationAssociationRegistration_SetAppAsDefault(reg, L"SGProbeB", L".sgprobe", AT_FILEEXTENSION);
        check(hr == S_OK, "same publisher: set");
        check(user_choice_is(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.sgprobe\\UserChoice",
                L"SGProbeB.sgprobe"), "the default changed to the other program");

        hr = IApplicationAssociationRegistration_SetAppAsDefault(reg, L"SGProbeB", L"application/x-sgprobe", AT_MIMETYPE);
        check(hr == S_OK, "MIME type default set");
        {
            WCHAR value[MAX_PATH]; DWORD size = sizeof(value);
            check(!RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\Shell\\Associations\\MIMEAssociations\\application/x-sgprobe\\UserChoice",
                    L"Progid", RRF_RT_REG_SZ, NULL, value, &size) && !lstrcmpW(value, L"SGProbeB.sgprobe"), "MIME UserChoice recorded");
        }
        RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Clients\\SGKind\\SGProbeB", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
        RegCloseKey(key);
        hr = IApplicationAssociationRegistration_SetAppAsDefault(reg, L"SGProbeB", L"SGKind", AT_STARTMENUCLIENT);
        check(hr == S_OK, "start menu client default set");
        hr = IApplicationAssociationRegistration_SetAppAsDefault(reg, L"NoSuchClient", L"SGKind", AT_STARTMENUCLIENT);
        check(FAILED(hr), "an unknown client is refused");
        RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"Software\\Clients\\SGKind");
        RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Clients\\SGKind");
    }

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
