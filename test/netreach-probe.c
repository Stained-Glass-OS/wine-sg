/* netreach-probe: network places that cannot be reached, in Windows' words (patches/sg/9658),
 * and the Network folder's logon with saved and remembered credentials. Against the stand-in
 * test/netreach-netmountd.py (one file server, SRV). Prints PASS/FAIL lines.
 *   netreach-probe PASSWORD */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <winnetwk.h>
#include <wincred.h>
#include <stdio.h>

#define IDC_USERNAME 101
#define IDC_PASSWORD 102
#define IDC_SAVE     104

static int rc;
static void check(BOOL ok, const char *what, DWORD got)
{
    printf("%s  %s (got %lu / %#lx)\n", ok ? "PASS" : "FAIL", what, got, got);
    if (!ok) rc = 1;
}

/* a watcher answers "Enter network credentials": counts prompts, notes the save box */
static volatile LONG prompts, save_box;
static const WCHAR *answer_pw;
static volatile BOOL tick_save, stop_watch;
static HWND found;
static BOOL CALLBACK find_prompt(HWND h, LPARAM l)
{
    WCHAR title[128];
    if (!IsWindowVisible(h)) return TRUE;
    GetWindowTextW(h, title, ARRAYSIZE(title));
    if (wcscmp(title, L"Enter network credentials") || !GetDlgItem(h, IDC_PASSWORD)) return TRUE;
    found = h;
    return FALSE;
}
static DWORD WINAPI watcher(void *arg)
{
    while (!stop_watch)
    {
        found = NULL;
        EnumWindows(find_prompt, 0);
        if (found)
        {
            HWND p = found, save = GetDlgItem(p, IDC_SAVE);
            InterlockedIncrement(&prompts);
            if (save && IsWindowVisible(save)) InterlockedIncrement(&save_box);
            Sleep(200);
            if (answer_pw)
            {
                SetDlgItemTextW(p, IDC_USERNAME, L"tester");
                SetDlgItemTextW(p, IDC_PASSWORD, answer_pw);
                if (tick_save) CheckDlgButton(p, IDC_SAVE, BST_CHECKED);
                PostMessageW(p, WM_COMMAND, IDOK, 0);
            }
            else PostMessageW(p, WM_COMMAND, IDCANCEL, 0);
            while (IsWindow(p) && IsWindowVisible(p)) Sleep(50);
        }
        Sleep(50);
    }
    return 0;
}

static HRESULT parse(const WCHAR *path)
{
    ITEMIDLIST *pidl = NULL;
    HRESULT hr = SHParseDisplayName(path, NULL, &pidl, 0, NULL);
    if (pidl) CoTaskMemFree(pidl);
    return hr;
}

/* explorer.exe PATH: its "Network Error" box and the text in it */
static HWND neterr;
static WCHAR neterr_text[2048];
static BOOL CALLBACK collect_text(HWND h, LPARAM l)
{
    WCHAR t[512];
    if (GetWindowTextW(h, t, ARRAYSIZE(t)) && wcslen(neterr_text) + wcslen(t) + 2 < ARRAYSIZE(neterr_text))
    {
        wcscat(neterr_text, t);
        wcscat(neterr_text, L"|");
    }
    return TRUE;
}
static BOOL CALLBACK find_neterr(HWND h, LPARAM l)
{
    WCHAR title[128];
    GetWindowTextW(h, title, ARRAYSIZE(title));
    if (IsWindowVisible(h) && !wcscmp(title, L"Network Error")) { neterr = h; return FALSE; }
    return TRUE;
}
static BOOL explorer_error(const WCHAR *path, const WCHAR *want)
{
    WCHAR cmd[MAX_PATH];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    int i;
    BOOL ok;

    swprintf(cmd, ARRAYSIZE(cmd), L"explorer.exe %ls", path);
    neterr = NULL; neterr_text[0] = 0;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return FALSE;
    for (i = 0; i < 300 && !neterr; i++) { Sleep(100); EnumWindows(find_neterr, 0); }
    if (neterr) { Sleep(300); EnumChildWindows(neterr, collect_text, 0); }
    ok = neterr && wcsstr(neterr_text, want);
    printf("      explorer %ls: %ls\n", path, neterr ? neterr_text : L"(no Network Error box)");
    if (neterr) PostMessageW(neterr, WM_COMMAND, IDCANCEL, 0);
    if (WaitForSingleObject(pi.hProcess, 10000)) TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return ok;
}

static HWND map_dlg;
static BOOL CALLBACK find_map(HWND h, LPARAM l)
{
    WCHAR title[128];
    GetWindowTextW(h, title, ARRAYSIZE(title));
    if (IsWindowVisible(h) && !wcscmp(title, L"Map Network Drive")) { map_dlg = h; return FALSE; }
    return TRUE;
}
static DWORD WINAPI shortcut_thread(void *arg)
{
    void (WINAPI *run)(HWND, HINSTANCE, WCHAR *, int) =
        (void *)GetProcAddress(LoadLibraryW(L"shell32.dll"), "SHHelpShortcuts_RunDLLW");
    if (run) run(NULL, NULL, (WCHAR *)L"Connect", SW_SHOWNORMAL);
    return 0;
}

int wmain(int argc, WCHAR **argv)
{
    CREDENTIALW cred = { 0 }, *saved;
    DWORD err, i;
    HRESULT hr;
    HANDLE t;

    if (argc < 2) return 2;
    CoInitialize(NULL);
    CreateThread(NULL, 0, watcher, NULL, 0, NULL);

    /* the file system's errors: no computer, no such share, refused */
    SetLastError(0); GetFileAttributesW(L"\\\\nosuchpc\\files"); err = GetLastError();
    check(err == ERROR_BAD_NETPATH, "a computer that is not there: ERROR_BAD_NETPATH (53)", err);
    SetLastError(0); GetFileAttributesW(L"\\\\srv\\nosuch"); err = GetLastError();
    check(err == ERROR_BAD_NET_NAME, "a share the computer does not have: ERROR_BAD_NET_NAME (67)", err);
    SetLastError(0); GetFileAttributesW(L"\\\\srv\\locked"); err = GetLastError();
    check(err == ERROR_ACCESS_DENIED, "a share that refuses a guest: ERROR_ACCESS_DENIED (5)", err);
    check(GetFileAttributesW(L"\\\\srv\\open\\hello.txt") != INVALID_FILE_ATTRIBUTES, "an open share is reached", GetLastError());

    /* File Explorer's parse: the same, and no password asked for them */
    hr = parse(L"\\\\nosuchpc");
    check(hr == HRESULT_FROM_WIN32(ERROR_BAD_NETPATH), "\\\\nosuchpc does not open: 0x80070035", hr);
    hr = parse(L"\\\\srv\\nosuch");
    check(hr == HRESULT_FROM_WIN32(ERROR_BAD_NET_NAME), "\\\\srv\\nosuch: 0x80070043", hr);
    check(prompts == 0, "and neither asked for a name and password", prompts);
    check(parse(L"\\\\srv") == S_OK, "\\\\srv, which is there, opens", 0);

    /* the name and password Credential Manager keeps for SRV: used, no prompt */
    cred.Type = CRED_TYPE_DOMAIN_PASSWORD;
    cred.TargetName = (WCHAR *)L"srv";
    cred.UserName = (WCHAR *)L"srv\\saveduser";
    cred.CredentialBlob = (BYTE *)argv[1];
    cred.CredentialBlobSize = lstrlenW(argv[1]) * sizeof(WCHAR);
    cred.Persist = CRED_PERSIST_ENTERPRISE;
    CredWriteW(&cred, 0);
    hr = parse(L"\\\\srv\\locked");
    check(hr == S_OK && prompts == 0, "a refusing share opens with the saved name and password, no prompt", hr);
    CredDeleteW(L"srv", CRED_TYPE_DOMAIN_PASSWORD, 0);

    /* none saved: "Enter network credentials", with "Remember my credentials" */
    answer_pw = argv[1]; tick_save = TRUE;
    hr = parse(L"\\\\srv\\team");
    check(hr == S_OK && prompts == 1, "another refusing share asks once and opens", hr);
    check(save_box == 1, "the prompt offers \"Remember my credentials\"", save_box);
    if (CredReadW(L"srv", CRED_TYPE_DOMAIN_PASSWORD, 0, &saved))
    {
        check(!wcsicmp(saved->UserName, L"srv\\tester"), "ticked, the name and password are saved for the computer", 0);
        CredFree(saved);
    }
    else check(FALSE, "ticked, the name and password are saved for the computer", GetLastError());

    /* File Explorer says why, in a Network Error box */
    check(explorer_error(L"\\\\nosuchpc", L"File Explorer cannot access \\\\nosuchpc"),
          "explorer \\\\nosuchpc: Network Error, cannot access", 0);
    check(explorer_error(L"\\\\srv\\nosuch", L"File Explorer cannot access \\\\srv\\nosuch") && prompts == 1,
          "explorer \\\\srv\\nosuch: Network Error, no password asked", prompts);

    /* rundll32 shell32.dll,SHHelpShortcuts_RunDLL Connect: Map Network Drive */
    t = CreateThread(NULL, 0, shortcut_thread, NULL, 0, NULL);
    for (i = 0; i < 100 && !map_dlg; i++) { Sleep(100); EnumWindows(find_map, 0); }
    check(map_dlg != NULL, "SHHelpShortcuts_RunDLL Connect opens Map Network Drive", 0);
    if (map_dlg) PostMessageW(map_dlg, WM_COMMAND, IDCANCEL, 0);
    WaitForSingleObject(t, 5000);

    stop_watch = TRUE;
    printf("RESULT: %s\n", rc ? "FAIL" : "PASS");
    return rc;
}
