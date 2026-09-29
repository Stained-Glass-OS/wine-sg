/* savedesk-probe: a Save As dialog (GetSaveFileName, as Notepad's), its
 * places bar's Desktop pressed; prints the items the list then shows, one
 * per line, and cancels. A file "gate-desktop.txt" is put on the desktop
 * first. (patches/sg/0480) */
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <stdio.h>

static DWORD WINAPI driver(void *arg)
{
    HWND dlg = NULL, view, list;
    int i, n, tries;
    (void)arg;
    for (tries = 0; tries < 100 && !(dlg = FindWindowW(L"#32770", L"Save As")); tries++) Sleep(100);
    if (!dlg) { printf("no dialog\n"); return 0; }
    Sleep(1500);
    SendMessageW(dlg, WM_COMMAND, MAKEWPARAM(0xa064, BN_CLICKED), 0);   /* the first place: Desktop */
    Sleep(3000);
    {
        WCHAR folder[MAX_PATH] = L"";
        SendMessageW(dlg, CDM_GETFOLDERPATH, MAX_PATH, (LPARAM)folder);
        printf("folder %ls\n", folder);
    }
    view = FindWindowExW(dlg, NULL, L"SHELLDLL_DefView", NULL);
    list = view ? FindWindowExW(view, NULL, L"SysListView32", NULL) : NULL;
    n = list ? (int)SendMessageW(list, LVM_GETITEMCOUNT, 0, 0) : -1;
    printf("items %d\n", n);
    for (i = 0; i < n; i++)
    {
        WCHAR text[260] = L"";
        LVITEMW item = { 0 };
        item.iSubItem = 0; item.pszText = text; item.cchTextMax = 260;
        SendMessageW(list, LVM_GETITEMTEXTW, i, (LPARAM)&item);
        printf("item %ls\n", text);
    }
    fflush(stdout);
    PostMessageW(dlg, WM_COMMAND, IDCANCEL, 0);
    return 0;
}

int main(void)
{
    WCHAR desk[MAX_PATH], path[MAX_PATH], file[MAX_PATH] = L"";
    OPENFILENAMEW ofn = { sizeof(ofn) };
    HANDLE f;

    SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY | CSIDL_FLAG_CREATE, NULL, 0, desk);
    swprintf(path, MAX_PATH, L"%ls\\gate-desktop.txt", desk);
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    printf("made %d %ls\n", f != INVALID_HANDLE_VALUE, path);
    CloseHandle(f);
    CloseHandle(CreateThread(NULL, 0, driver, NULL, 0, NULL));
    ofn.lpstrFilter = L"Text Documents (*.txt)\0*.txt\0All Files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT;
    GetSaveFileNameW(&ofn);
    return 0;
}
