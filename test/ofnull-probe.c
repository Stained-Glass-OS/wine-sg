/* comdlg32 batch (patches/sg/2021), run by test/ofnull-gate.sh. GetOpenFileName
 * with no buffer (lpstrFile NULL) is accepted and still reports the file
 * offsets; with a buffer it fills it, and a buffer that is too small gives
 * FNERR_BUFFERTOOSMALL with the size needed in the first WORD. The dialog is
 * closed by a hook that presses OK after a moment.
 *
 *   ofnull-probe.exe */
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static int type_name;

static BOOL CALLBACK find_edit(HWND hwnd, LPARAM lParam)
{
    char cls[20];
    if (GetClassNameA(hwnd, cls, sizeof(cls)) > 0 && !strcmp("Edit", cls))
    {
        SetWindowTextA(hwnd, "testfile");
        return FALSE;
    }
    return TRUE;
}

static UINT_PTR CALLBACK hook(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HWND parent = GetParent(dlg);
    if (msg == WM_NOTIFY)
    {
        SetTimer(dlg, 0, 100, 0);
        SetTimer(dlg, 1, 1500, 0);
        if (type_name) EnumChildWindows(parent, find_edit, 0);
    }
    if (msg == WM_TIMER)
    {
        if (!wParam) PostMessageA(parent, WM_COMMAND, IDOK, 0);
        else
        {
            KillTimer(dlg, 0);
            PostMessageA(parent, WM_COMMAND, IDCANCEL, 0);
        }
    }
    return FALSE;
}

static void setup(OPENFILENAMEA *ofn, char *file, DWORD max)
{
    memset(ofn, 0, sizeof(*ofn));
    ofn->lStructSize = OPENFILENAME_SIZE_VERSION_400A;
    ofn->lpstrFile = file;
    ofn->nMaxFile = max;
    ofn->nFileOffset = 0xdead;
    ofn->nFileExtension = 0xbeef;
    ofn->lpfnHook = hook;
    ofn->Flags = OFN_ENABLEHOOK | OFN_EXPLORER;
    ofn->hInstance = GetModuleHandleA(NULL);
    ofn->lpstrFilter = "text\0*.txt\0All\0*\0\0";
}

int main(void)
{
    OPENFILENAMEA ofn;
    char file[MAX_PATH], dir[MAX_PATH];
    DWORD ret, err;

    GetTempPathA(MAX_PATH, dir);
    if (dir[0] && dir[strlen(dir) - 1] == '\\') dir[strlen(dir) - 1] = 0;

    type_name = 1;
    setup(&ofn, NULL, 0);
    ret = GetOpenFileNameA(&ofn);
    err = CommDlgExtendedError();
    check(ret == 1, "no buffer: the dialog is accepted");
    check(err == 0, "no buffer: no error");
    check(ofn.nFileOffset != 0xdead, "no buffer: the file offset is set");
    check(ofn.nFileExtension != 0xbeef, "no buffer: the extension offset is set");

    setup(&ofn, NULL, 1024);
    ret = GetOpenFileNameA(&ofn);
    err = CommDlgExtendedError();
    check(ret == 1 && err == 0 && ofn.nFileOffset != 0xdead, "a size but no buffer");

    type_name = 0;
    snprintf(file, sizeof(file), "%s\\sgnote.txt", dir);
    setup(&ofn, file, MAX_PATH);
    ret = GetOpenFileNameA(&ofn);
    err = CommDlgExtendedError();
    check(ret == 1 && err == 0, "a buffer with a file name: accepted");
    check(!_stricmp(file + strlen(file) - 10, "sgnote.txt"), "the buffer has the name");
    check(ofn.nFileOffset == strlen(dir) + 1, "and the file offset is after the folder");
    check(ofn.nFileExtension == ofn.nFileOffset + 7, "and the extension offset after the dot");

    type_name = 1;
    file[0] = 0;
    setup(&ofn, file, 4);
    ret = GetOpenFileNameA(&ofn);
    err = CommDlgExtendedError();
    check(ret == 0 && err == FNERR_BUFFERTOOSMALL, "a buffer too small: refused with FNERR_BUFFERTOOSMALL");
    check(*(WORD *)file >= 9, "and the size needed is in its first WORD");

    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
