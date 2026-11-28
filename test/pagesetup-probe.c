/* pagesetup-probe: the Page Setup dialog in Windows' words (wine-sg 9820): its caption is
 * "Page Setup" (it was "Setup Page") and its margins are "Margins (inches)" or
 * "Margins (millimeters)" (they were "Margins [inches]", "Margins [mm]"). */
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <wchar.h>

static int failures;
#define CHECK(c, ...) do { int ok_ = !!(c); printf(ok_ ? "PASS  " : "FAIL  "); printf(__VA_ARGS__); printf("\n"); \
                           failures += !ok_; fflush(stdout); } while (0)

static WCHAR caption[128], margins[128];

static BOOL CALLBACK find_margins(HWND child, LPARAM lparam)
{
    WCHAR text[128];

    GetWindowTextW(child, text, 128);
    if (!wcsncmp(text, L"Margins", 7)) { lstrcpynW(margins, text, 128); return FALSE; }
    return TRUE;
}

static DWORD WINAPI watcher(void *arg)
{
    HWND dlg = NULL, w = NULL;
    DWORD pid, owner = GetCurrentProcessId();
    int i;

    for (i = 0; i < 200 && !dlg; i++)
    {
        while ((w = FindWindowExW(NULL, w, L"#32770", NULL)))
        {
            GetWindowThreadProcessId(w, &pid);
            if (pid == owner && IsWindowVisible(w)) { dlg = w; break; }
        }
        if (!dlg) Sleep(50);
    }
    if (!dlg) return 1;
    Sleep(300);
    GetWindowTextW(dlg, caption, 128);
    EnumChildWindows(dlg, find_margins, 0);
    PostMessageW(dlg, WM_COMMAND, IDCANCEL, 0);
    return 0;
}

int main(int argc, char **argv)
{
    PAGESETUPDLGW ps;
    HANDLE thread;

    memset(&ps, 0, sizeof(ps));
    ps.lStructSize = sizeof(ps);
    ps.Flags = argc > 1 ? PSD_INTHOUSANDTHSOFINCHES : PSD_INHUNDREDTHSOFMILLIMETERS;
    thread = CreateThread(NULL, 0, watcher, NULL, 0, NULL);
    PageSetupDlgW(&ps);
    WaitForSingleObject(thread, 5000);
    CHECK(!wcscmp(caption, L"Page Setup"), "the dialog is called Page Setup (%ls)", caption);
    CHECK(!wcscmp(margins, argc > 1 ? L"Margins (inches)" : L"Margins (millimeters)"), "its margins are %s (%ls)",
          argc > 1 ? "Margins (inches)" : "Margins (millimeters)", margins);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures ? 1 : 0;
}
