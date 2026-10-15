/* userenv: LoadUserProfile/UnloadUserProfile, GetAllUsersProfileDirectory, CreateLinkFile (ordinal 138),
 * AppContainer profiles. "probe.exe child-nopriv|child-noadmin" is the restricted child of the privilege checks. */
#include <windows.h>
#include <userenv.h>
#include <sddl.h>
#include <shlobj.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef HRESULT (WINAPI *create_ac_t)(PCWSTR, PCWSTR, PCWSTR, SID_AND_ATTRIBUTES *, DWORD, PSID *);
typedef HRESULT (WINAPI *delete_ac_t)(PCWSTR);
typedef HRESULT (WINAPI *folder_ac_t)(PCWSTR, PWSTR *);
typedef HRESULT (WINAPI *reg_ac_t)(REGSAM, PHKEY);
typedef BOOL (WINAPI *link_t)(int, LPCSTR, LPCSTR, LPCSTR, LPCSTR, DWORD, LPCSTR, WORD, DWORD, LPCSTR, LPCSTR, DWORD);

static HANDLE own_token(void)
{
    HANDLE t;
    OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &t);
    return t;
}

static int child(const char *mode)
{
    PROFILEINFOW pi = { sizeof(pi) };
    HANDLE token = own_token();
    BOOL ret;
    DWORD err;

    pi.lpUserName = (WCHAR *)L"child";
    ret = LoadUserProfileW(token, &pi);
    err = GetLastError();
    if (ret) { UnloadUserProfile(token, pi.hProfile); return 2; }
    if (!strcmp(mode, "child-nopriv")) return err == ERROR_PRIVILEGE_NOT_HELD ? 0 : 10 + (err & 0xff);
    return err == ERROR_ACCESS_DENIED ? 0 : 10 + (err & 0xff);
}

static DWORD run_child(const char *mode)
{
    HANDLE token, restricted = NULL;
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    WCHAR cmd[MAX_PATH + 40], path[MAX_PATH];
    DWORD code = 99, len;
    BYTE sidbuf[SECURITY_MAX_SID_SIZE];
    SID_AND_ATTRIBUTES disable;
    BOOL ok;

    token = own_token();
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wsprintfW(cmd, L"\"%s\" %S", path, mode);
    if (!strcmp(mode, "child-nopriv"))
        ok = CreateRestrictedToken(token, DISABLE_MAX_PRIVILEGE, 0, NULL, 0, NULL, 0, NULL, &restricted);
    else
    {
        len = sizeof(sidbuf);
        CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, sidbuf, &len);
        disable.Sid = sidbuf;
        disable.Attributes = 0;
        ok = CreateRestrictedToken(token, 0, 1, &disable, 0, NULL, 0, NULL, &restricted);
    }
    if (!ok) { printf("restricted token failed %lu\n", GetLastError()); return 98; }
    if (!CreateProcessAsUserW(restricted, NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        /* a restricted-token child may be refused by the host; fall back to the thread impersonation-free check */
        printf("CreateProcessAsUser failed %lu\n", GetLastError());
        return 97;
    }
    WaitForSingleObject(pi.hProcess, 60000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code;
}

static void test_profile(void)
{
    PROFILEINFOW pi;
    PROFILEINFOA pa;
    HANDLE token = own_token();
    HKEY hk;
    DWORD val = 0, size = sizeof(val), type, code;
    BOOL ret;
    LONG r;

    memset(&pi, 0, sizeof(pi));
    pi.dwSize = sizeof(pi);
    SetLastError(0xdeadbeef);
    ret = LoadUserProfileW(token, &pi);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "no user name: %d %lu", ret, GetLastError());
    pi.lpUserName = (WCHAR *)L"sgtest";
    pi.dwSize = sizeof(pi) - 4;
    SetLastError(0xdeadbeef);
    ret = LoadUserProfileW(token, &pi);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "bad size: %d %lu", ret, GetLastError());
    pi.dwSize = sizeof(pi);
    SetLastError(0xdeadbeef);
    ret = LoadUserProfileW(NULL, &pi);
    CHECK(!ret && GetLastError() == ERROR_INVALID_HANDLE, "null token: %d %lu", ret, GetLastError());

    ret = LoadUserProfileW(token, &pi);
    CHECK(ret, "LoadUserProfileW failed %lu", GetLastError());
    CHECK(pi.hProfile && pi.hProfile != HKEY_CURRENT_USER, "hProfile %p", pi.hProfile);
    if (ret)
    {
        /* the handle is the root of the user's hive: what is written there shows in HKCU */
        val = 0x5a5a;
        r = RegSetValueExA(pi.hProfile, "SgUserenvProbe", 0, REG_DWORD, (BYTE *)&val, sizeof(val));
        CHECK(!r, "write via hProfile: %ld", r);
        val = 0;
        RegOpenKeyExA(HKEY_CURRENT_USER, NULL, 0, KEY_READ, &hk);
        r = RegQueryValueExA(hk, "SgUserenvProbe", NULL, &type, (BYTE *)&val, &size);
        CHECK(!r && val == 0x5a5a, "HKCU sees the value: %ld %#lx", r, val);
        RegDeleteValueA(hk, "SgUserenvProbe");
        RegCloseKey(hk);
        SetLastError(0xdeadbeef);
        ret = UnloadUserProfile(NULL, pi.hProfile);
        CHECK(!ret && GetLastError() == ERROR_INVALID_HANDLE, "unload null token: %d %lu", ret, GetLastError());
        ret = UnloadUserProfile(token, pi.hProfile);
        CHECK(ret, "UnloadUserProfile failed %lu", GetLastError());
        r = RegSetValueExA(pi.hProfile, "x", 0, REG_DWORD, (BYTE *)&val, sizeof(val));
        CHECK(r == ERROR_INVALID_HANDLE, "handle closed by unload: %ld", r);
        SetLastError(0xdeadbeef);
        ret = UnloadUserProfile(token, pi.hProfile);
        CHECK(!ret, "unload twice succeeded");
    }
    SetLastError(0xdeadbeef);
    ret = UnloadUserProfile(token, NULL);
    CHECK(!ret && GetLastError() == ERROR_INVALID_HANDLE, "unload null profile: %d %lu", ret, GetLastError());

    memset(&pa, 0, sizeof(pa));
    pa.dwSize = sizeof(pa);
    SetLastError(0xdeadbeef);
    ret = LoadUserProfileA(token, &pa);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "A no user name: %d %lu", ret, GetLastError());
    pa.lpUserName = (char *)"sgtest";
    ret = LoadUserProfileA(token, &pa);
    CHECK(ret && pa.hProfile, "LoadUserProfileA %d %lu", ret, GetLastError());
    if (ret) CHECK(UnloadUserProfile(token, pa.hProfile), "unload A %lu", GetLastError());

    code = run_child("child-nopriv");
    CHECK(code == 0, "token without privileges: child exit %lu (2 = the load was granted)", code);
    code = run_child("child-noadmin");
    CHECK(code == 0, "token with deny-only administrators: child exit %lu (2 = the load was granted)", code);
    CloseHandle(token);
}

static void test_allusers(void)
{
    WCHAR expect[MAX_PATH], buf[MAX_PATH];
    char bufa[MAX_PATH];
    DWORD len;
    BOOL ret;

    SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, expect);
    len = 0;
    SetLastError(0xdeadbeef);
    ret = GetAllUsersProfileDirectoryW(NULL, &len);
    CHECK(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER && len == lstrlenW(expect) + 1, "size query %d %lu len %lu", ret, GetLastError(), len);
    len = MAX_PATH;
    ret = GetAllUsersProfileDirectoryW(buf, &len);
    CHECK(ret && !lstrcmpiW(buf, expect) && len == lstrlenW(expect) + 1, "W: %d %ls len %lu", ret, buf, len);
    len = 3;
    SetLastError(0xdeadbeef);
    ret = GetAllUsersProfileDirectoryW(buf, &len);
    CHECK(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "short buffer %d %lu", ret, GetLastError());
    len = MAX_PATH;
    ret = GetAllUsersProfileDirectoryA(bufa, &len);
    CHECK(ret && len == strlen(bufa) + 1 && !strcmp(bufa + strlen(bufa) - 11, "ProgramData") , "A: %d %s len %lu", ret, bufa, len);
    SetLastError(0xdeadbeef);
    ret = GetAllUsersProfileDirectoryW(buf, NULL);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "null size %d %lu", ret, GetLastError());
}

static void test_link(void)
{
    link_t create = (link_t)GetProcAddress(GetModuleHandleA("userenv.dll"), (LPCSTR)138);
    IShellLinkA *sl;
    IPersistFile *pf;
    WCHAR dirw[MAX_PATH], pathw[MAX_PATH];
    char buf[MAX_PATH], dir[MAX_PATH];
    int idx, show;
    WORD hk;
    BOOL ret;
    HRESULT hr;

    CHECK(create != NULL, "ordinal 138 not exported");
    if (!create) return;
    SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, SHGFP_TYPE_CURRENT, dirw);
    wsprintfW(pathw, L"%s\\SgProbeDir\\SgProbeLink.lnk", dirw);
    DeleteFileW(pathw);
    SetLastError(0xdeadbeef);
    ret = create(CSIDL_PROGRAMS, "SgProbeDir", NULL, "C:\\windows\\notepad.exe", NULL, 0, NULL, 0, 0, NULL, NULL, 0);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "no file name %d %lu", ret, GetLastError());

    ret = create(CSIDL_PROGRAMS, "SgProbeDir", "SgProbeLink", "C:\\windows\\notepad.exe", "C:\\windows\\system32\\shell32.dll", 3,
                 "C:\\windows", 0x0141, SW_SHOWMAXIMIZED, "probe comment", NULL, 0);
    CHECK(ret, "CreateLinkFile failed %lu", GetLastError());
    CHECK(GetFileAttributesW(pathw) != INVALID_FILE_ATTRIBUTES, "link file missing");
    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkA, (void **)&sl);
    if (SUCCEEDED(hr))
    {
        sl->lpVtbl->QueryInterface(sl, &IID_IPersistFile, (void **)&pf);
        hr = pf->lpVtbl->Load(pf, pathw, STGM_READ);
        CHECK(hr == S_OK, "load %#lx", hr);
        buf[0] = 0;
        sl->lpVtbl->GetPath(sl, buf, MAX_PATH, NULL, 0);
        CHECK(!lstrcmpiA(buf, "C:\\windows\\notepad.exe"), "target %s", buf);
        sl->lpVtbl->GetWorkingDirectory(sl, dir, MAX_PATH);
        CHECK(!lstrcmpiA(dir, "C:\\windows"), "dir %s", dir);
        sl->lpVtbl->GetDescription(sl, buf, MAX_PATH);
        CHECK(!strcmp(buf, "probe comment"), "comment %s", buf);
        sl->lpVtbl->GetIconLocation(sl, buf, MAX_PATH, &idx);
        CHECK(!lstrcmpiA(buf, "C:\\windows\\system32\\shell32.dll") && idx == 3, "icon %s %d", buf, idx);
        sl->lpVtbl->GetShowCmd(sl, &show);
        CHECK(show == SW_SHOWMAXIMIZED, "show %d", show);
        sl->lpVtbl->GetHotkey(sl, &hk);
        CHECK(hk == 0x0141, "hotkey %#x", hk);
        pf->lpVtbl->Release(pf);
        sl->lpVtbl->Release(sl);
    }
    /* any other window state is the normal one */
    ret = create(CSIDL_PROGRAMS, "SgProbeDir", "SgProbeLink", "C:\\windows\\notepad.exe", NULL, 0, NULL, 0, SW_HIDE, NULL, NULL, 0);
    CHECK(ret, "second create %lu", GetLastError());
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkA, (void **)&sl);
    if (SUCCEEDED(hr))
    {
        sl->lpVtbl->QueryInterface(sl, &IID_IPersistFile, (void **)&pf);
        pf->lpVtbl->Load(pf, pathw, STGM_READ);
        sl->lpVtbl->GetShowCmd(sl, &show);
        CHECK(show == SW_SHOWNORMAL, "show other %d", show);
        pf->lpVtbl->Release(pf);
        sl->lpVtbl->Release(sl);
    }
    {
        /* the stored ShowCommand (header offset 0x3c) is 1 for any state but maximised and minimised-no-activate */
        HANDLE h = CreateFileW(pathw, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        BYTE hdr[0x40];
        DWORD n = 0;
        ReadFile(h, hdr, sizeof(hdr), &n, NULL);
        CloseHandle(h);
        CHECK(n == sizeof(hdr) && *(DWORD *)(hdr + 0x3c) == 1, "stored ShowCommand %lu", n == sizeof(hdr) ? *(DWORD *)(hdr + 0x3c) : 999);
    }
    DeleteFileW(pathw);
    wsprintfW(pathw, L"%s\\SgProbeDir", dirw);
    RemoveDirectoryW(pathw);
}

static void test_appcontainer(void)
{
    HMODULE mod = GetModuleHandleA("userenv.dll");
    create_ac_t create = (create_ac_t)GetProcAddress(mod, "CreateAppContainerProfile");
    delete_ac_t del = (delete_ac_t)GetProcAddress(mod, "DeleteAppContainerProfile");
    folder_ac_t folder = (folder_ac_t)GetProcAddress(mod, "GetAppContainerFolderPath");
    reg_ac_t regloc = (reg_ac_t)GetProcAddress(mod, "GetAppContainerRegistryLocation");
    HRESULT (WINAPI *derive)(PCWSTR, PSID *) = (void *)GetProcAddress(mod, "DeriveAppContainerSidFromAppContainerName");
    SID_AND_ATTRIBUTES cap;
    WCHAR longname[80], *sidstr, *path = NULL, tail[MAX_PATH];
    PSID sid = NULL, sid2 = NULL, capsid = NULL;
    HRESULT hr;
    HKEY key;
    DWORD attrs;
    int i;

    CHECK(create && del && folder && regloc, "exports missing %p %p %p %p", create, del, folder, regloc);
    if (!create || !del || !folder || !regloc) return;
    del(L"sg.probe.container");

    hr = create(NULL, L"d", L"d", NULL, 0, &sid);
    CHECK(hr == E_INVALIDARG, "null name %#lx", hr);
    hr = create(L"", L"d", L"d", NULL, 0, &sid);
    CHECK(hr == E_INVALIDARG, "empty name %#lx", hr);
    for (i = 0; i < 65; i++) longname[i] = 'a';
    longname[65] = 0;
    hr = create(longname, L"d", L"d", NULL, 0, &sid);
    CHECK(hr == E_INVALIDARG, "65 chars %#lx", hr);
    longname[64] = 0;
    hr = create(L"bad/name", L"d", L"d", NULL, 0, &sid);
    CHECK(hr == E_INVALIDARG, "bad char %#lx", hr);
    hr = create(L"sg.probe.container", L"d", L"d", NULL, 1, &sid);
    CHECK(hr == E_INVALIDARG, "count without capabilities %#lx", hr);
    hr = create(L"sg.probe.container", L"d", L"d", NULL, 0, NULL);
    CHECK(hr == E_INVALIDARG, "null sid out %#lx", hr);
    {
        static WCHAR big[600];
        for (i = 0; i < 599; i++) big[i] = 'x';
        hr = create(L"sg.probe.container", big, L"d", NULL, 0, &sid);
        CHECK(hr == E_INVALIDARG, "display name 599 chars still within limit? %#lx", hr);
        if (hr == S_OK) { FreeSid(sid); sid = NULL; del(L"sg.probe.container"); }
    }

    ConvertStringSidToSidW(L"S-1-15-3-1024-1065365936-1281604716-3511738428-1654721687-432734479-3232135806-4053264122-3456934681", &capsid);
    cap.Sid = capsid;
    cap.Attributes = SE_GROUP_ENABLED;
    hr = create(L"sg.probe.container", L"Probe container", L"made by the probe", &cap, 1, &sid);
    CHECK(hr == S_OK, "create %#lx", hr);
    if (hr == S_OK)
    {
        derive(L"sg.probe.container", &sid2);
        CHECK(sid2 && EqualSid(sid, sid2), "profile sid is the derived sid");
        FreeSid(sid2);
        ConvertSidToStringSidW(sid, &sidstr);
        hr = folder(sidstr, &path);
        CHECK(hr == S_OK && path, "folder path %#lx", hr);
        if (path)
        {
            SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, tail);
            CHECK(!wcsncmp(path, tail, lstrlenW(tail)) && wcsstr(path, L"Packages\\sg.probe.container"), "path %ls", path);
            attrs = GetFileAttributesW(path);
            CHECK(attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY), "folder missing");
            wsprintfW(tail, L"%s\\AC\\Temp", path);
            attrs = GetFileAttributesW(tail);
            CHECK(attrs != INVALID_FILE_ATTRIBUTES, "AC\\Temp missing");
            /* a file the delete must remove too */
            wsprintfW(tail, L"%s\\AC\\Temp\\f.txt", path);
            CloseHandle(CreateFileW(tail, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
        }
        hr = create(L"sg.probe.container", L"Probe container", L"made by the probe", &cap, 1, &sid2);
        CHECK(hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS), "create twice %#lx", hr);
        hr = folder(NULL, &path);
        CHECK(hr == E_INVALIDARG, "folder null sid %#lx", hr);
        hr = regloc(KEY_READ, &key);
        CHECK(hr == E_INVALIDARG, "registry location outside a container %#lx", hr);
        hr = del(NULL);
        CHECK(hr == E_INVALIDARG, "delete null %#lx", hr);
        hr = del(L"sg.probe.container");
        CHECK(hr == S_OK, "delete %#lx", hr);
        if (path)
        {
            CHECK(GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES, "folder still there after delete");
            CoTaskMemFree(path);
        }
        path = NULL;
        hr = folder(sidstr, &path);
        CHECK(FAILED(hr) && !path, "folder of a deleted profile %#lx", hr);
        LocalFree(sidstr);
        FreeSid(sid);
    }
    hr = del(L"sg.probe.container");
    CHECK(hr == S_OK, "delete of a missing profile %#lx", hr);
    hr = create(L"sg.probe.container", NULL, NULL, NULL, 0, &sid);
    CHECK(hr == S_OK, "create again after delete %#lx", hr);
    if (hr == S_OK) FreeSid(sid);
    del(L"sg.probe.container");
    LocalFree(capsid);
}

int main(int argc, char **argv)
{
    if (argc > 1) return child(argv[1]);
    test_profile();
    test_allusers();
    test_link();
    test_appcontainer();
    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
