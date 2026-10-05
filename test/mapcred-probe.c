/* Map Network Drive on a share that refuses the signed-in user
 * (patches/sg/0839): the dialog must ask for a name and password ("Enter
 * network credentials"), ask again while they are refused, and map the drive
 * with the right ones. A thread opens WNetConnectionDialog; the probe gives
 * it ARGV[1] (\\server\share) and answers each prompt -- "wrong" first, then
 * ARGV[2], with "Remember my credentials" ticked ("Reconnect at sign-in" is
 * ticked already). Prints: prompts=N errors=N result=N mapped=0|1, then
 * what was remembered for the next sign-in: remembered=<RemotePath of
 * HKCU\Network\<letter>> saved=<user name saved for the server>.
 *   mapcred-probe saved SERVER   prints only the saved user name */
#include <windows.h>
#include <winnetwk.h>
#include <wincred.h>
#include <stdio.h>

#define IDC_USERNAME 101
#define IDC_PASSWORD 102
#define IDC_SAVE     104

static DWORD WINAPI map_thread(void *arg) { return WNetConnectionDialog(NULL, RESOURCETYPE_DISK); }

static HWND found;
static BOOL CALLBACK find_dialog(HWND h, LPARAM want_prompt)
{
    WCHAR cls[64], title[128];
    if (!IsWindowVisible(h)) return TRUE;
    GetClassNameW(h, cls, ARRAYSIZE(cls));
    GetWindowTextW(h, title, ARRAYSIZE(title));
    if (wcscmp(cls, L"#32770") || wcscmp(title, L"Map Network Drive")) return TRUE;
    if (!!GetDlgItem(h, IDC_PASSWORD) != !!want_prompt) return TRUE;
    found = h;
    return FALSE;
}

static void print_saved(const WCHAR *server)
{
    CREDENTIALW *cred;
    if (CredReadW(server, CRED_TYPE_DOMAIN_PASSWORD, 0, &cred))
    {
        printf("saved=%ls\n", cred->UserName);
        CredFree(cred);
    }
    else printf("saved=none\n");
}

int wmain(int argc, WCHAR **argv)
{
    HANDLE t;
    HWND dlg = NULL, combo, edit;
    WCHAR letter[8] = L"";
    DWORD code = 0, start;
    int prompts = 0, errors = 0, i;
    WCHAR key[32], remembered[MAX_PATH] = L"none", server[MAX_PATH], *end;
    DWORD size = sizeof(remembered);

    if (argc > 2 && !wcscmp(argv[1], L"saved"))
    {
        print_saved(argv[2]);
        return 0;
    }
    t = CreateThread(NULL, 0, map_thread, NULL, 0, NULL);

    for (i = 0; i < 100 && !(dlg = FindWindowW(L"SGMapNetworkDrive", L"Map Network Drive")); i++) Sleep(100);
    if (!dlg) { printf("no dialog\n"); return 1; }
    Sleep(300);
    if ((combo = FindWindowExW(dlg, NULL, L"ComboBox", NULL))) GetWindowTextW(combo, letter, ARRAYSIZE(letter));
    if ((edit = FindWindowExW(dlg, NULL, L"Edit", NULL))) SendMessageW(edit, WM_SETTEXT, 0, (LPARAM)argv[1]);
    PostMessageW(dlg, WM_COMMAND, IDOK, 0);

    start = GetTickCount();
    while (GetTickCount() - start < 30000 && WaitForSingleObject(t, 100) == WAIT_TIMEOUT)
    {
        found = NULL;
        EnumWindows(find_dialog, TRUE);
        if (found)
        {
            HWND prompt = found;
            prompts++;
            Sleep(200);
            SetDlgItemTextW(prompt, IDC_USERNAME, L"tester");
            SetDlgItemTextW(prompt, IDC_PASSWORD, prompts == 1 ? L"wrong" : argv[2]);
            CheckDlgButton(prompt, IDC_SAVE, BST_CHECKED);
            PostMessageW(prompt, WM_COMMAND, IDOK, 0);
            for (i = 0; i < 50 && IsWindow(prompt) && IsWindowVisible(prompt); i++) Sleep(100);
            continue;
        }
        EnumWindows(find_dialog, FALSE);
        if (found)
        {
            /* an error box: the dialog gave up */
            errors++;
            PostMessageW(found, WM_COMMAND, IDOK, 0);
            Sleep(500);
            PostMessageW(dlg, WM_COMMAND, IDCANCEL, 0);
        }
        if (prompts > 4) PostMessageW(dlg, WM_COMMAND, IDCANCEL, 0);
    }
    WaitForSingleObject(t, 5000);
    GetExitCodeThread(t, &code);
    Sleep(1500);   /* the user's drive letters are read again at most once a second */
    printf("letter=%ls prompts=%d errors=%d result=%lu mapped=%d\n", letter, prompts, errors, code,
           letter[0] >= 'A' && letter[0] <= 'Z' && (GetLogicalDrives() & (1u << (letter[0] - 'A'))) ? 1 : 0);
    swprintf(key, ARRAYSIZE(key), L"Network\\%lc", letter[0]);
    RegGetValueW(HKEY_CURRENT_USER, key, L"RemotePath", RRF_RT_REG_SZ, NULL, remembered, &size);
    printf("remembered=%ls\n", remembered);
    lstrcpynW(server, argv[1] + 2, ARRAYSIZE(server));
    if ((end = wcschr(server, '\\'))) *end = 0;
    print_saved(server);
    return 0;
}
