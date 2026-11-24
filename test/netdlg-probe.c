/* wine-sg 9921: SHStartNetConnectionDialogW/A, run by test/netdlg-gate.sh. Imported by name (Shell32.lib
 * names the W one, ordinal 12 is the A one; Wine had neither, so this probe did not start). The Map Network Drive dialog opens with
 * the remote name given, read-only; the probe cancels it; the result is S_OK as documented. */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

typedef HRESULT (WINAPI *netdlg_a_t)(HWND, LPCSTR, DWORD);
static netdlg_a_t pSHStartNetConnectionDialogA;
static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)

static WCHAR seen_folder[MAX_PATH];
static int seen_dialog, seen_readonly;
static DWORD main_tid;

static BOOL CALLBACK find_edit(HWND child, LPARAM lp)
{
    WCHAR cls[32];
    GetClassNameW(child, cls, 32);
    if (!lstrcmpiW(cls, L"Edit"))
    {
        GetWindowTextW(child, seen_folder, MAX_PATH);
        seen_readonly = (GetWindowLongW(child, GWL_STYLE) & ES_READONLY) || !IsWindowEnabled(child);
        return FALSE;
    }
    return TRUE;
}

static BOOL CALLBACK find_dialog(HWND hwnd, LPARAM lp)
{
    WCHAR title[100];
    if (!IsWindowVisible(hwnd)) return TRUE;
    GetWindowTextW(hwnd, title, 100);
    if (wcscmp(title, L"Map Network Drive")) return TRUE;
    *(HWND *)lp = hwnd;
    return FALSE;
}

/* waits for the dialog on the main thread, records it, presses Cancel */
static DWORD WINAPI closer(void *arg)
{
    int i;
    for (i = 0; i < 200; i++)
    {
        HWND dlg = NULL;
        EnumThreadWindows(main_tid, find_dialog, (LPARAM)&dlg);
        if (dlg)
        {
            seen_dialog++;
            seen_folder[0] = 0;
            EnumChildWindows(dlg, find_edit, 0);
            PostMessageW(dlg, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
            return 0;
        }
        Sleep(50);
    }
    return 1;
}

static HRESULT run(BOOL ansi, const char *remote)
{
    HANDLE t;
    HRESULT hr;
    WCHAR wremote[MAX_PATH];
    seen_dialog = 0; seen_readonly = 0; seen_folder[0] = 0;
    t = CreateThread(NULL, 0, closer, NULL, 0, NULL);
    if (remote) MultiByteToWideChar(CP_ACP, 0, remote, -1, wremote, MAX_PATH);
    if (ansi) hr = pSHStartNetConnectionDialogA(NULL, remote, RESOURCETYPE_DISK);
    else hr = SHStartNetConnectionDialogW(NULL, remote ? wremote : NULL, RESOURCETYPE_DISK);
    WaitForSingleObject(t, 15000);
    CloseHandle(t);
    return hr;
}

int main(void)
{
    HRESULT hr;
    HMODULE shell32 = GetModuleHandleA("shell32.dll");

    main_tid = GetCurrentThreadId();
    CHECK(GetProcAddress(shell32, (LPCSTR)14) == (FARPROC)SHStartNetConnectionDialogW, "ordinal 14 is SHStartNetConnectionDialogW");
    pSHStartNetConnectionDialogA = (netdlg_a_t)GetProcAddress(shell32, (LPCSTR)12);
    CHECK(pSHStartNetConnectionDialogA != NULL, "ordinal 12 (SHStartNetConnectionDialogA) is exported");
    if (!pSHStartNetConnectionDialogA) goto end;

    hr = run(FALSE, "\\\\sgserver\\share");
    CHECK(seen_dialog == 1, "W: the Map Network Drive dialog opened");
    CHECK(!wcscmp(seen_folder, L"\\\\sgserver\\share"), "W: with the remote name in the folder box (\"%ls\")", seen_folder);
    CHECK(seen_readonly, "W: read-only (CONNDLG_RO_PATH)");
    CHECK(hr == S_OK, "W: cancelled, the result is S_OK (%08lx)", hr);

    hr = run(TRUE, "\\\\sgserver\\other");
    CHECK(seen_dialog == 1 && !wcscmp(seen_folder, L"\\\\sgserver\\other"), "A: the dialog opened with \"%ls\"", seen_folder);
    CHECK(hr == S_OK, "A: S_OK (%08lx)", hr);

    hr = run(FALSE, NULL);
    CHECK(seen_dialog == 1 && !seen_folder[0], "no remote name: an empty folder box (\"%ls\")", seen_folder);
    CHECK(!seen_readonly, "no remote name: the folder box can be typed in");
    CHECK(hr == S_OK, "no remote name: S_OK (%08lx)", hr);

end:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
