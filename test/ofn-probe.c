/* GetOpenFileName with no hook shows the item dialog, whose address box names
 * the folder (patches/sg/0582).
 * A helper thread finds the dialog, reports whether it is the item dialog (its
 * address box, id 204), types a name and presses Open. Prints
 * "ofn=<ret> itemdlg=<yes|no> file=<path> offset=<n> ext=<n> filter=<n>". */
#include <windows.h>
#include <commdlg.h>
#include <dlgs.h>
#include <stdio.h>

static volatile int g_item = -1;
static WCHAR g_addr[MAX_PATH];
static const WCHAR *g_title = L"SgOfnProbe", *g_name = L"ofn-test.txt";

static DWORD WINAPI driver(void *arg)
{
    HWND dlg = NULL;
    int i;
    for (i = 0; i < 100 && !(dlg = FindWindowW(NULL, g_title)); i++) Sleep(100);
    if (!dlg) return 0;
    Sleep(1000);
    g_item = GetDlgItem(dlg, 204) != NULL;
    g_addr[0] = 0;
    GetWindowTextW(GetDlgItem(dlg, 204), g_addr, MAX_PATH);
    {
        HWND name = GetDlgItem(dlg, edt1);
        if (!name) name = GetDlgItem(dlg, cmb13);
        SendMessageW(name, WM_SETTEXT, 0, (LPARAM)g_name);
    }
    PostMessageW(dlg, WM_COMMAND, IDOK, 0);
    return 0;
}

int main(void)
{
    WCHAR file[MAX_PATH] = L"";
    OPENFILENAMEW ofn = { sizeof(ofn) };
    HANDLE h = CreateFileW(L"C:\\ofn-test.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    BOOL ret;
    CloseHandle(h);
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = L"C:\\";
    ofn.lpstrTitle = L"SgOfnProbe";
    ofn.lpstrFilter = L"Text\0*.txt\0All\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    CloseHandle(CreateThread(NULL, 0, driver, NULL, 0, NULL));
    ret = GetOpenFileNameW(&ofn);
    printf("ofn=%d itemdlg=%s address=%ls file=%ls offset=%u ext=%u filter=%lu\n", ret, g_item == 1 ? "yes" : "no",
           g_addr, file, ofn.nFileOffset, ofn.nFileExtension, ofn.nFilterIndex);

    /* the ANSI save dialog: a default extension is added */
    {
        char afile[MAX_PATH] = "";
        OPENFILENAMEA sfn = { sizeof(sfn) };
        g_item = -1; g_title = L"SgSfnProbe"; g_name = L"saved";
        sfn.lpstrFile = afile;
        sfn.nMaxFile = MAX_PATH;
        sfn.lpstrInitialDir = "C:\\";
        sfn.lpstrTitle = "SgSfnProbe";
        sfn.lpstrFilter = "Text\0*.txt\0";
        sfn.lpstrDefExt = "txt";
        sfn.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR;
        CloseHandle(CreateThread(NULL, 0, driver, NULL, 0, NULL));
        ret = GetSaveFileNameA(&sfn);
        printf("sfn=%d itemdlg=%s file=%s offset=%u ext=%u\n", ret, g_item == 1 ? "yes" : "no",
               afile, sfn.nFileOffset, sfn.nFileExtension);
    }
    return 0;
}
