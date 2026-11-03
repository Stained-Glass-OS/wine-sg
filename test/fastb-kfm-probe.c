/* IKnownFolderManager::FindFolderFromPath / FindFolderFromIDList and IKnownFolder::GetFolderType (patch 3601);
 * no match is a failure, IKnownFolder::GetRedirectionCapabilities (6239). */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <knownfolders.h>
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void trim(WCHAR *s)
{
    size_t n = wcslen(s);
    while (n > 3 && s[n - 1] == '\\') s[--n] = 0;
}

int main(void)
{
    IKnownFolderManager *mgr = NULL;
    IKnownFolder *kf;
    WCHAR *path, sub[MAX_PATH], got[MAX_PATH];
    PIDLIST_ABSOLUTE pidl;
    FOLDERTYPEID ftid;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_KnownFolderManager, NULL, CLSCTX_INPROC_SERVER, &IID_IKnownFolderManager, (void **)&mgr);
    CHECK(hr == S_OK, "KnownFolderManager %lx", hr);
    if (!mgr) { printf("RESULT: FAIL\n"); return 1; }

    hr = SHGetKnownFolderPath(&FOLDERID_Windows, KF_FLAG_DONT_VERIFY, NULL, &path);
    CHECK(hr == S_OK, "SHGetKnownFolderPath %lx", hr);
    if (hr != S_OK) return 1;

    /* exact */
    kf = (void *)1;
    hr = IKnownFolderManager_FindFolderFromPath(mgr, path, FFFP_EXACTMATCH, &kf);
    CHECK(hr == S_OK && kf && kf != (void *)1, "exact match %lx", hr);
    if (hr == S_OK && kf)
    {
        WCHAR *p2;
        hr = IKnownFolder_GetPath(kf, KF_FLAG_DONT_VERIFY, &p2);
        wcscpy(got, p2); trim(got);
        CoTaskMemFree(p2);
        wcscpy(sub, path); trim(sub);
        CHECK(hr == S_OK && !wcsicmp(got, sub), "found folder has path %ls, want %ls", got, sub);
        IKnownFolder_Release(kf);
    }

    /* a path below it */
    swprintf(sub, MAX_PATH, L"%ls\\some\\child", path);
    kf = (void *)1;
    hr = IKnownFolderManager_FindFolderFromPath(mgr, sub, FFFP_EXACTMATCH, &kf);
    CHECK(hr == E_FAIL && !kf, "no exact match for a child: %lx %p", hr, kf);
    kf = NULL;
    hr = IKnownFolderManager_FindFolderFromPath(mgr, sub, FFFP_NEARESTPARENTMATCH, &kf);
    CHECK(hr == S_OK && kf, "nearest parent %lx", hr);
    if (kf)
    {
        WCHAR *p2;
        hr = IKnownFolder_GetPath(kf, KF_FLAG_DONT_VERIFY, &p2);
        wcscpy(got, p2); trim(got);
        CoTaskMemFree(p2);
        CHECK(hr == S_OK && wcslen(got) <= wcslen(path) + 1 && !wcsnicmp(got, sub, wcslen(got)), "parent path %ls", got);
        IKnownFolder_Release(kf);
    }

    /* a path that matches nothing */
    kf = (void *)1;
    hr = IKnownFolderManager_FindFolderFromPath(mgr, L"Q:\\nowhere\\at all", FFFP_NEARESTPARENTMATCH, &kf);
    CHECK(hr == E_FAIL && !kf, "no match: %lx %p", hr, kf);

    /* a prefix that is not on a component boundary is not a parent */
    swprintf(sub, MAX_PATH, L"%lsXYZ", path);
    hr = IKnownFolderManager_FindFolderFromPath(mgr, sub, FFFP_EXACTMATCH, &kf);
    CHECK(hr == E_FAIL && !kf, "sibling name: %lx", hr);

    swprintf(sub, MAX_PATH, L"%lsXYZ\\child", path);
    kf = NULL;
    hr = IKnownFolderManager_FindFolderFromPath(mgr, sub, FFFP_NEARESTPARENTMATCH, &kf);
    if (hr == S_OK && kf)
    {
        WCHAR *p2;
        IKnownFolder_GetPath(kf, KF_FLAG_DONT_VERIFY, &p2);
        wcscpy(got, p2); trim(got);
        CoTaskMemFree(p2);
        wcscpy(sub, path); trim(sub);
        CHECK(wcsicmp(got, sub) != 0, "a name sharing only a prefix matched %ls", got);
        IKnownFolder_Release(kf);
    }
    CHECK(IKnownFolderManager_FindFolderFromPath(mgr, NULL, FFFP_EXACTMATCH, &kf) == E_INVALIDARG, "NULL path");
    CHECK(IKnownFolderManager_FindFolderFromPath(mgr, path, FFFP_EXACTMATCH, NULL) == E_INVALIDARG, "NULL out");

    /* id list */
    hr = SHGetKnownFolderIDList(&FOLDERID_Windows, KF_FLAG_DONT_VERIFY, NULL, &pidl);
    CHECK(hr == S_OK, "SHGetKnownFolderIDList %lx", hr);
    if (hr == S_OK)
    {
        kf = NULL;
        hr = IKnownFolderManager_FindFolderFromIDList(mgr, pidl, &kf);
        CHECK(hr == S_OK && kf, "FindFolderFromIDList %lx", hr);
        if (kf) IKnownFolder_Release(kf);
        ILFree(pidl);
    }
    CHECK(IKnownFolderManager_FindFolderFromIDList(mgr, NULL, &kf) == E_INVALIDARG, "NULL pidl");
    /* an id list of a folder that is no known folder */
    swprintf(sub, MAX_PATH, L"%ls\\system32\\drivers", path);
    if (SUCCEEDED(SHParseDisplayName(sub, NULL, &pidl, 0, NULL)))
    {
        kf = (void *)1;
        hr = IKnownFolderManager_FindFolderFromIDList(mgr, pidl, &kf);
        CHECK(FAILED(hr) && !kf, "no known folder for an id list: %lx %p", hr, kf);
        ILFree(pidl);
    }

    /* redirection: per-user folders can move, unless policy says no; virtual ones cannot */
    {
        static const WCHAR pol[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer";
        KF_REDIRECTION_CAPABILITIES caps;
        DWORD one = 1;
        HKEY key;
        BOOL hklm_ok;

        caps = 0xdead;
        CHECK(IKnownFolderManager_GetFolder(mgr, &FOLDERID_Documents, &kf) == S_OK, "Documents");
        CHECK(IKnownFolder_GetRedirectionCapabilities(kf, &caps) == S_OK && caps == KF_REDIRECTION_CAPABILITIES_ALLOW_ALL,
              "Documents redirectable: %#x", caps);
        RegCreateKeyExW(HKEY_CURRENT_USER, pol, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
        RegSetValueExW(key, L"DisablePersonalDirChange", 0, REG_DWORD, (BYTE *)&one, sizeof(one));
        caps = 0xdead;
        CHECK(IKnownFolder_GetRedirectionCapabilities(kf, &caps) == S_OK && caps == KF_REDIRECTION_CAPABILITIES_DENY_POLICY,
              "policy denies: %#x", caps);
        RegDeleteValueW(key, L"DisablePersonalDirChange");
        RegCloseKey(key);
        IKnownFolder_Release(kf);

        caps = 0xdead;
        CHECK(IKnownFolderManager_GetFolder(mgr, &FOLDERID_ComputerFolder, &kf) == S_OK, "Computer");
        CHECK(IKnownFolder_GetRedirectionCapabilities(kf, &caps) == S_OK && caps == KF_REDIRECTION_CAPABILITIES_DENY_ALL,
              "virtual folder not redirectable: %#x", caps);
        IKnownFolder_Release(kf);

        hklm_ok = !RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders",
                                 0, KEY_SET_VALUE, &key);
        if (hklm_ok) RegCloseKey(key);
        caps = 0xdead;
        CHECK(IKnownFolderManager_GetFolder(mgr, &FOLDERID_PublicDocuments, &kf) == S_OK, "Public Documents");
        CHECK(IKnownFolder_GetRedirectionCapabilities(kf, &caps) == S_OK &&
              caps == (hklm_ok ? KF_REDIRECTION_CAPABILITIES_ALLOW_ALL : KF_REDIRECTION_CAPABILITIES_DENY_PERMISSIONS),
              "common folder by HKLM access (%d): %#x", hklm_ok, caps);
        IKnownFolder_Release(kf);
    }

    /* folder type */
    hr = IKnownFolderManager_GetFolder(mgr, &FOLDERID_Documents, &kf);
    CHECK(hr == S_OK, "GetFolder Documents %lx", hr);
    if (kf)
    {
        KNOWNFOLDER_DEFINITION def;
        memset(&ftid, 0xcc, sizeof(ftid));
        hr = IKnownFolder_GetFolderType(kf, &ftid);
        CHECK(hr == S_OK, "GetFolderType %lx", hr);
        if (SUCCEEDED(IKnownFolder_GetFolderDefinition(kf, &def)))
        {
            CHECK(IsEqualGUID(&ftid, &def.ftidType), "type matches the definition");
            FreeKnownFolderDefinitionFields(&def);
        }
        CHECK(IKnownFolder_GetFolderType(kf, NULL) == E_INVALIDARG, "GetFolderType NULL");
        IKnownFolder_Release(kf);
    }

    CoTaskMemFree(path);
    IKnownFolderManager_Release(mgr);
    printf("%d checks, %d failures\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
