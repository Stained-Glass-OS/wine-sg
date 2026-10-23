/* ofnpreview-probe: the file dialogs' preview pane (patches/sg/6512).
 *
 * GetOpenFileNamePreview and GetSaveFileNamePreview were the plain dialogs. Opens
 * GetOpenFileNamePreviewW with a hook of the program's own on a folder with a small AVI and a
 * text file; a watcher thread selects each in the dialog's list and looks at the preview's MCI
 * window and its text, then presses OK.
 *
 * Prints name=value lines; see test/ofnpreview-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <vfw.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define W 32
#define H 16

static OPENFILENAMEW ofn;
static WCHAR file[MAX_PATH];
static BOOL hook_init_ok;
static int hook_selchanges;
static DWORD main_thread;

static UINT_PTR CALLBACK app_hook(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_INITDIALOG)
        hook_init_ok = (OPENFILENAMEW *)lparam == &ofn && ((OPENFILENAMEW *)lparam)->lCustData == 0x1234;
    if (msg == WM_NOTIFY && ((NMHDR *)lparam)->code == CDN_SELCHANGE) hook_selchanges++;
    return 0;
}

static void write_avi(const WCHAR *path)
{
    AVISTREAMINFOW info = { 0 };
    BITMAPINFOHEADER bih = { sizeof(bih), W, H, 1, 24, BI_RGB, W * H * 3 };
    static BYTE bits[W * H * 3];
    PAVIFILE avi;
    PAVISTREAM stream;
    int i;

    memset(bits, 0x80, sizeof(bits));
    AVIFileInit();
    if (FAILED(AVIFileOpenW(&avi, path, OF_CREATE | OF_WRITE, NULL))) return;
    info.fccType = streamtypeVIDEO;
    info.fccHandler = mmioFOURCC('D','I','B',' ');
    info.dwScale = 1;
    info.dwRate = 10;
    SetRect(&info.rcFrame, 0, 0, W, H);
    if (SUCCEEDED(AVIFileCreateStreamW(avi, &stream, &info)))
    {
        AVIStreamSetFormat(stream, 0, &bih, sizeof(bih));
        for (i = 0; i < 20; i++) AVIStreamWrite(stream, i, 1, bits, sizeof(bits), AVIIF_KEYFRAME, NULL, NULL);
        AVIStreamRelease(stream);
    }
    AVIFileRelease(avi);
}

struct found { HWND hwnd; const WCHAR *cls; };

static BOOL CALLBACK find_class(HWND hwnd, LPARAM lparam)
{
    struct found *f = (struct found *)lparam;
    WCHAR cls[64];

    GetClassNameW(hwnd, cls, ARRAY_SIZE(cls));
    if (!wcscmp(cls, f->cls)) { f->hwnd = hwnd; return FALSE; }
    return TRUE;
}

static HWND find_child(HWND parent, const WCHAR *cls)
{
    struct found f = { NULL, cls };
    EnumChildWindows(parent, find_class, (LPARAM)&f);
    return f.hwnd;
}

static BOOL CALLBACK find_dialog(HWND hwnd, LPARAM lparam)
{
    WCHAR cls[16];
    GetClassNameW(hwnd, cls, ARRAY_SIZE(cls));
    if (!wcscmp(cls, L"#32770") && IsWindowVisible(hwnd)) { *(HWND *)lparam = hwnd; return FALSE; }
    return TRUE;
}

static void select_item(HWND list, const WCHAR *name)
{
    LVFINDINFOW find = { LVFI_STRING, name };
    LVITEMW item = { 0 };
    int index = SendMessageW(list, LVM_FINDITEMW, -1, (LPARAM)&find);

    if (index < 0)
    {
        /* the list may show names without extensions */
        WCHAR base[MAX_PATH], *dot;
        lstrcpyW(base, name);
        if ((dot = wcsrchr(base, '.'))) *dot = 0;
        find.psz = base;
        index = SendMessageW(list, LVM_FINDITEMW, -1, (LPARAM)&find);
    }
    item.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    SendMessageW(list, LVM_SETITEMSTATE, -1, (LPARAM)&item);
    item.state = LVIS_SELECTED | LVIS_FOCUSED;
    SendMessageW(list, LVM_SETITEMSTATE, index, (LPARAM)&item);
    printf("select %ls index=%d\n", name, index);
}

static void look(HWND dlg, HWND mci, const char *name, BOOL want_movie)
{
    WCHAR info[128] = L"";
    HWND child = NULL;
    UINT device;
    int i;

    for (i = 0; i < 30; i++)
    {
        Sleep(100);
        device = SendMessageW(mci, MCIWNDM_GETDEVICEID, 0, 0);
        if (!!device == want_movie) break;
    }
    device = SendMessageW(mci, MCIWNDM_GETDEVICEID, 0, 0);
    /* the preview's text is in the dialog's template child */
    child = GetParent(mci);
    GetDlgItemTextW(child, 0x931, info, ARRAY_SIZE(info));
    if (want_movie)
    {
        printf("%s=%d device=%u info=%ls\n", name, device != 0 && wcsstr(info, L"32 x 16") != NULL, device, info);
        printf("%s_length=%d\n", name, wcsstr(info, L", 2.0 s") != NULL);
    }
    else
        printf("%s=%d device=%u info=%ls\n", name, device == 0 && !info[0], device, info);
}

static DWORD WINAPI watcher(void *arg)
{
    HWND dlg = NULL, mci = NULL, list = NULL;
    int i;

    for (i = 0; i < 200 && !dlg; i++)
    {
        Sleep(100);
        EnumThreadWindows(main_thread, find_dialog, (LPARAM)&dlg);
    }
    printf("dialog=%d\n", dlg != NULL);
    if (!dlg) return 0;
    for (i = 0; i < 50 && !(list = find_child(dlg, L"SysListView32")); i++) Sleep(100);
    mci = find_child(dlg, L"MCIWndClass");
    printf("preview_window=%d\n", mci != NULL);
    if (mci && list)
    {
        select_item(list, L"clip.avi");
        look(dlg, mci, "previews_avi", TRUE);
        {
            RECT dr, mr;
            GetWindowRect(dlg, &dr);
            GetWindowRect(mci, &mr);
            printf("rects dlg=%ld,%ld-%ld,%ld mci=%ld,%ld-%ld,%ld\n", dr.left, dr.top, dr.right, dr.bottom, mr.left, mr.top, mr.right, mr.bottom);
            printf("preview_placed=%d\n", mr.left > dr.left + (dr.right - dr.left) / 2 && mr.right <= dr.right &&
                   mr.right - mr.left > 40 && mr.bottom - mr.top > 40);
        }
        select_item(list, L"note.txt");
        look(dlg, mci, "no_preview_text", FALSE);
        select_item(list, L"clip.avi");
        look(dlg, mci, "previews_again", TRUE);
    }
    Sleep(300);
    PostMessageW(dlg, WM_COMMAND, IDOK, 0);
    return 0;
}

int main(void)
{
    WCHAR dir[MAX_PATH], path[MAX_PATH];
    HANDLE thread, h;
    BOOL ret;
    DWORD written;

    setvbuf(stdout, NULL, _IONBF, 0);
    InitCommonControls();
    GetTempPathW(MAX_PATH, dir);
    wcscat(dir, L"pv");
    CreateDirectoryW(dir, NULL);
    swprintf(path, MAX_PATH, L"%ls\\clip.avi", dir);
    write_avi(path);
    swprintf(path, MAX_PATH, L"%ls\\note.txt", dir);
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, "hello\r\n", 7, &written, NULL);
    CloseHandle(h);

    main_thread = GetCurrentThreadId();
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"All files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = ARRAY_SIZE(file);
    ofn.lpstrInitialDir = dir;
    ofn.Flags = OFN_EXPLORER | OFN_ENABLEHOOK | OFN_FILEMUSTEXIST;
    ofn.lpfnHook = app_hook;
    ofn.lCustData = 0x1234;
    thread = CreateThread(NULL, 0, watcher, NULL, 0, NULL);
    ret = GetOpenFileNamePreviewW(&ofn);
    WaitForSingleObject(thread, 30000);
    printf("chosen=%d file=%ls\n", ret && wcsstr(file, L"clip.avi") && !wcscmp(file + ofn.nFileOffset, L"clip.avi"), file);
    printf("app_hook=%d selchanges=%d\n", hook_init_ok && hook_selchanges >= 3, hook_selchanges);
    printf("flags_kept=%d flags=%#lx\n", (ofn.Flags & (OFN_EXPLORER | OFN_ENABLEHOOK | OFN_ENABLETEMPLATE)) ==
           (OFN_EXPLORER | OFN_ENABLEHOOK) && ofn.lpfnHook == app_hook && ofn.lCustData == 0x1234, ofn.Flags);
    printf("done=1\n");
    return 0;
}
