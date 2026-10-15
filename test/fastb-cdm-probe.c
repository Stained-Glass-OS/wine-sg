/* CDM_SETDEFEXT in the common file dialog (patch 3601). The hook sets the default extension and a
 * name without extension, then presses OK: the result carries the new extension. */
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)


static UINT_PTR CALLBACK hook(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp) { return 0; } /* a hook selects the classic dialog */

static DWORD main_tid;
static int use_ansi;

static BOOL CALLBACK find_dlg(HWND hwnd, LPARAM lp)
{
    WCHAR cls[32];

    if (GetClassNameW(hwnd, cls, 32) && !wcscmp(cls, L"#32770") && IsWindowVisible(hwnd))
    {
        *(HWND *)lp = hwnd;
        return FALSE;
    }
    return TRUE;
}

/* The dialog runs its own modal loop on the main thread: another thread finds it and drives it. */
static DWORD WINAPI driver(void *arg)
{
    HWND dlg = NULL;
    int i;

    for (i = 0; i < 150 && !dlg; i++)
    {
        Sleep(100);
        EnumThreadWindows(main_tid, find_dlg, (LPARAM)&dlg);
    }
    if (!dlg) return 1;
    Sleep(500);
    if (use_ansi)
    {
        LRESULT r = SendMessageA(dlg, CDM_SETDEFEXT, 0, (LPARAM)"sgx");
        CHECK(r == TRUE, "CDM_SETDEFEXT (A) result %Id", (INT_PTR)r);
        SetDlgItemTextA(dlg, 0x480, "newname");
        SetDlgItemTextA(dlg, 0x47c, "newname");
    }
    else
    {
        LRESULT r = SendMessageW(dlg, CDM_SETDEFEXT, 0, (LPARAM)L"sgx");
        CHECK(r == TRUE, "CDM_SETDEFEXT result %Id", (INT_PTR)r);
        SendMessageW(dlg, CDM_SETDEFEXT, 0, (LPARAM)L"sgy"); /* replaced again */
        SetDlgItemTextW(dlg, 0x480, L"newname");
        SetDlgItemTextW(dlg, 0x47c, L"newname");
    }
    Sleep(300);
    PostMessageW(dlg, WM_COMMAND, IDOK, 0);
    return 0;
}

int main(void)
{
    WCHAR file[MAX_PATH] = L"", dir[MAX_PATH];
    char fileA[MAX_PATH] = "";
    OPENFILENAMEW ofn = {0};
    OPENFILENAMEA ofnA = {0};
    BOOL ret;
    size_t len;

    GetTempPathW(MAX_PATH, dir);
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = dir;
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_EXPLORER | OFN_ENABLEHOOK | OFN_NOTESTFILECREATE | OFN_OVERWRITEPROMPT;
    ofn.lpfnHook = (LPOFNHOOKPROC)hook;
    ofnA.lpfnHook = (LPOFNHOOKPROC)hook;
    main_tid = GetCurrentThreadId();
    CloseHandle(CreateThread(NULL, 0, driver, NULL, 0, NULL));
    ret = GetSaveFileNameW(&ofn);
    CHECK(ret, "GetSaveFileNameW failed, error %lx", CommDlgExtendedError());
    len = wcslen(file);
    CHECK(ret && len > 11 && !wcsicmp(file + len - 11, L"newname.sgy"), "W result %ls", file);

    ofnA.lStructSize = sizeof(ofnA);
    ofnA.lpstrFile = fileA;
    ofnA.nMaxFile = MAX_PATH;
    ofnA.lpstrDefExt = "txt";
    ofnA.Flags = OFN_EXPLORER | OFN_ENABLEHOOK | OFN_NOTESTFILECREATE | OFN_OVERWRITEPROMPT;
    use_ansi = 1;
    CloseHandle(CreateThread(NULL, 0, driver, NULL, 0, NULL));
    ret = GetSaveFileNameA(&ofnA);
    CHECK(ret, "GetSaveFileNameA failed, error %lx", CommDlgExtendedError());
    len = strlen(fileA);
    CHECK(ret && len > 11 && !stricmp(fileA + len - 11, "newname.sgx"), "A result %s", fileA);

    printf("%d checks, %d failures\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
