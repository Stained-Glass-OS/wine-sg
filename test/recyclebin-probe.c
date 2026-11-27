/* The Recycle Bin as Windows 10 has it (patches/sg/9065): its menu (Empty
 * Recycle Bin, greyed while empty), its Properties (locations, maximum size,
 * "Don't move files to the Recycle Bin", "Display delete confirmation
 * dialog"), SHEmptyRecycleBin and SHQueryRecycleBin, its icon empty or full,
 * the shell items' Properties (This PC, the user's folder) and the folder a
 * program typed in File Explorer's address bar starts in (#20).
 *
 *   recyclebin-probe.exe menu | bgmenu | icon | query [root] | empty
 *   recyclebin-probe.exe trash PATH         (Delete, no questions)
 *   recyclebin-probe.exe trashask PATH yes|no (Delete; the question answered)
 *   recyclebin-probe.exe emptyask yes|no [root]
 *   recyclebin-probe.exe verb VERB PARSENAME [yes|no]
 *   recyclebin-probe.exe props-bin [MAXMB NUKE CONFIRM]
 *   recyclebin-probe.exe props-item PARSENAME
 *   recyclebin-probe.exe cwd-report FILE
 *   recyclebin-probe.exe addr TEXT          (typed into File Explorer's address bar) */
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>

#define BIN L"::{645FF040-5081-101B-9F08-00AA002F954E}"
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const WCHAR *answer;            /* what a question is answered */
static WCHAR seen[1024];               /* the question asked */
static volatile LONG done;

static BOOL CALLBACK collect_text(HWND hwnd, LPARAM lp)
{
    WCHAR cls[32], text[512];

    GetClassNameW(hwnd, cls, ARRAY_SIZE(cls));
    if (!lstrcmpiW(cls, L"Static") && GetWindowTextW(hwnd, text, ARRAY_SIZE(text)) && text[0])
    {
        if (seen[0]) lstrcatW(seen, L" | ");
        if (lstrlenW(seen) + lstrlenW(text) < ARRAY_SIZE(seen) - 4) lstrcatW(seen, text);
    }
    return TRUE;
}

struct find { DWORD pid; const WCHAR *cls; const WCHAR *title; HWND found; };

static BOOL CALLBACK find_window(HWND hwnd, LPARAM lp)
{
    struct find *f = (struct find *)lp;
    WCHAR cls[64], title[256];
    DWORD pid;

    GetWindowThreadProcessId(hwnd, &pid);
    if (f->pid && pid != f->pid) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;
    GetClassNameW(hwnd, cls, ARRAY_SIZE(cls));
    GetWindowTextW(hwnd, title, ARRAY_SIZE(title));
    if (f->cls && lstrcmpiW(cls, f->cls)) return TRUE;
    if (f->title && !wcsstr(title, f->title)) return TRUE;
    f->found = hwnd;
    return FALSE;
}

static HWND wait_window(DWORD pid, const WCHAR *cls, const WCHAR *title, int ms)
{
    struct find f = { pid, cls, title, NULL };
    int i;

    for (i = 0; i < ms / 50; i++)
    {
        f.found = NULL;
        EnumWindows(find_window, (LPARAM)&f);
        if (f.found) return f.found;
        Sleep(50);
    }
    return NULL;
}

/* answers the first question of this process (a message box) */
static DWORD WINAPI answer_thread(void *arg)
{
    HWND dlg = wait_window(GetCurrentProcessId(), L"#32770", NULL, 8000);
    WCHAR title[256] = L"";

    if (dlg)
    {
        const WCHAR *confirm;

        GetWindowTextW(dlg, title, ARRAY_SIZE(title));
        swprintf(seen, ARRAY_SIZE(seen), L"%ls: ", title);
        EnumChildWindows(dlg, collect_text, 0);
        /* shell32's own question draws its text, kept in a property */
        if ((confirm = GetPropW(dlg, L"WINE_CONFIRM")) && lstrlenW(seen) + lstrlenW(confirm) < ARRAY_SIZE(seen) - 4)
            lstrcatW(seen, confirm);
        PostMessageW(dlg, WM_COMMAND, !lstrcmpiW(answer, L"yes") ? IDYES : IDNO, 0);
    }
    else lstrcpyW(seen, L"(none)");
    InterlockedExchange(&done, 1);
    return 0;
}

static void start_answer(const WCHAR *what)
{
    answer = what;
    seen[0] = 0;
    done = 0;
    CloseHandle(CreateThread(NULL, 0, answer_thread, NULL, 0, NULL));
}

static void end_answer(void)
{
    int i;
    for (i = 0; i < 200 && !done; i++) Sleep(50);
    wprintf(L"asked: %ls\n", seen);
}

static IContextMenu *item_menu(const WCHAR *name, HMENU *menu)
{
    IShellFolder *desktop, *parent;
    LPITEMIDLIST pidl;
    PCUITEMID_CHILD child;
    IContextMenu *cm = NULL;

    SHGetDesktopFolder(&desktop);
    if (FAILED(SHParseDisplayName(name, NULL, &pidl, 0, NULL))) { wprintf(L"no item %ls\n", name); return NULL; }
    if (SUCCEEDED(SHBindToParent(pidl, &IID_IShellFolder, (void **)&parent, &child)))
    {
        IShellFolder_GetUIObjectOf(parent, NULL, 1, &child, &IID_IContextMenu, NULL, (void **)&cm);
        IShellFolder_Release(parent);
    }
    if (cm)
    {
        *menu = CreatePopupMenu();
        IContextMenu_QueryContextMenu(cm, *menu, 0, 1, 0x7fff, CMF_NORMAL);
    }
    return cm;
}

/* the state of the menu item named TEXT: enabled, grey or absent */
static const WCHAR *item_state(HMENU menu, const WCHAR *text)
{
    int i, n = GetMenuItemCount(menu);
    WCHAR buf[128];

    for (i = 0; i < n; i++)
    {
        MENUITEMINFOW mi = { sizeof(mi), MIIM_STRING | MIIM_STATE };
        mi.dwTypeData = buf;
        mi.cch = ARRAY_SIZE(buf);
        if (!GetMenuItemInfoW(menu, i, TRUE, &mi)) continue;
        if (wcscmp(buf, text)) continue;
        return (mi.fState & MFS_DISABLED) ? L"grey" : L"enabled";
    }
    return L"absent";
}

static void invoke(IContextMenu *cm, const char *verb)
{
    CMINVOKECOMMANDINFO ici = { sizeof(ici) };
    ici.lpVerb = verb;
    ici.nShow = SW_SHOWNORMAL;
    IContextMenu_InvokeCommand(cm, &ici);
}

static void print_query(const WCHAR *root)
{
    SHQUERYRBINFO info = { sizeof(info) };
    HRESULT hr = SHQueryRecycleBinW(root, &info);
    wprintf(L"query hr=%#lx items=%I64d size=%I64d\n", hr, info.i64NumItems, info.i64Size);
}

static int do_delete(const WCHAR *path, BOOL ask)
{
    SHFILEOPSTRUCTW op = { 0 };
    WCHAR from[MAX_PATH + 2] = { 0 };
    int ret;

    lstrcpynW(from, path, MAX_PATH);
    op.wFunc = FO_DELETE;
    op.pFrom = from;
    op.fFlags = FOF_ALLOWUNDO | FOF_SILENT | FOF_NOERRORUI | (ask ? 0 : FOF_NOCONFIRMATION);
    ret = SHFileOperationW(&op);
    wprintf(L"delete ret=%d aborted=%d gone=%d\n", ret, op.fAnyOperationsAborted,
            GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES);
    return ret;
}

struct empty_args { const WCHAR *root; DWORD flags; };

static DWORD WINAPI empty_thread(void *arg)
{
    struct empty_args *a = arg;
    HRESULT hr = SHEmptyRecycleBinW(NULL, a->root, a->flags);
    wprintf(L"empty hr=%#lx\n", hr);
    return 0;
}

static void dump_list(HWND list)
{
    int i, n = SendMessageW(list, LVM_GETITEMCOUNT, 0, 0);
    WCHAR a[128], b[64];

    wprintf(L"locations=%d\n", n);
    for (i = 0; i < n; i++)
    {
        LVITEMW item = { 0 };
        item.cchTextMax = ARRAY_SIZE(a);
        item.pszText = a;
        SendMessageW(list, LVM_GETITEMTEXTW, i, (LPARAM)&item);
        item.iSubItem = 1;
        item.cchTextMax = ARRAY_SIZE(b);
        item.pszText = b;
        SendMessageW(list, LVM_GETITEMTEXTW, i, (LPARAM)&item);
        wprintf(L"location %ls | %ls\n", a, b);
    }
}

static HWND find_page_control(HWND sheet, int id)
{
    HWND page = (HWND)SendMessageW(sheet, PSM_GETCURRENTPAGEHWND, 0, 0);
    return page ? GetDlgItem(page, id) : NULL;
}

static int props_bin(int argc, WCHAR **argv)
{
    HMENU menu;
    IContextMenu *cm = item_menu(BIN, &menu);
    HWND sheet, list, edit, custom, nuke, confirm;
    WCHAR text[64];

    if (!cm) return 1;
    invoke(cm, "properties");
    if (!(sheet = wait_window(GetCurrentProcessId(), NULL, L"Recycle Bin Properties", 8000)))
    {
        wprintf(L"no sheet\n");
        return 1;
    }
    wprintf(L"sheet\n");
    list = find_page_control(sheet, 19065);
    edit = find_page_control(sheet, 19068);
    custom = find_page_control(sheet, 19066);
    nuke = find_page_control(sheet, 19069);
    confirm = find_page_control(sheet, 19070);
    if (!list || !edit || !custom || !nuke || !confirm) { wprintf(L"controls missing\n"); return 1; }
    dump_list(list);
    GetWindowTextW(edit, text, ARRAY_SIZE(text));
    wprintf(L"maxsize=%ls custom=%d nuke=%d confirm=%d editenabled=%d\n", text,
            (int)SendMessageW(custom, BM_GETCHECK, 0, 0), (int)SendMessageW(nuke, BM_GETCHECK, 0, 0),
            (int)SendMessageW(confirm, BM_GETCHECK, 0, 0), IsWindowEnabled(edit));
    if (argc >= 5)
    {
        int want_nuke = _wtoi(argv[3]), want_confirm = _wtoi(argv[4]);

        SendMessageW(edit, WM_SETTEXT, 0, (LPARAM)argv[2]);
        SendMessageW(want_nuke ? nuke : custom, BM_CLICK, 0, 0);
        if ((int)SendMessageW(confirm, BM_GETCHECK, 0, 0) != want_confirm) SendMessageW(confirm, BM_CLICK, 0, 0);
        SendMessageW(sheet, PSM_PRESSBUTTON, PSBTN_OK, 0);
        Sleep(500);
        wprintf(L"applied\n");
    }
    else SendMessageW(sheet, PSM_PRESSBUTTON, PSBTN_CANCEL, 0);
    Sleep(300);
    return 0;
}

int wmain(int argc, WCHAR **argv)
{
    const WCHAR *cmd = argc > 1 ? argv[1] : L"";
    HMENU menu;
    IContextMenu *cm;

    CoInitialize(NULL);
    setvbuf(stdout, NULL, _IONBF, 0);

    if (!wcscmp(cmd, L"menu"))
    {
        if (!(cm = item_menu(BIN, &menu))) return 1;
        wprintf(L"menu open=%ls default=%d empty=%ls cut=%ls copy=%ls delete=%ls properties=%ls\n", item_state(menu, L"&Open"),
                (int)GetMenuDefaultItem(menu, TRUE, 0), item_state(menu, L"Empty Recycle &Bin"),
                item_state(menu, L"C&ut"), item_state(menu, L"&Copy"), item_state(menu, L"&Delete"),
                item_state(menu, L"&Properties"));
        return 0;
    }
    if (!wcscmp(cmd, L"bgmenu"))
    {
        IShellFolder *desktop, *bin;
        IShellView *view;
        LPITEMIDLIST pidl;

        SHGetDesktopFolder(&desktop);
        SHParseDisplayName(BIN, NULL, &pidl, 0, NULL);
        if (FAILED(IShellFolder_BindToObject(desktop, pidl, NULL, &IID_IShellFolder, (void **)&bin)) ||
            FAILED(IShellFolder_CreateViewObject(bin, NULL, &IID_IShellView, (void **)&view)) ||
            FAILED(IShellView_GetItemObject(view, SVGIO_BACKGROUND, &IID_IContextMenu, (void **)&cm)))
        {
            wprintf(L"no background menu\n");
            return 1;
        }
        menu = CreatePopupMenu();
        IContextMenu_QueryContextMenu(cm, menu, 0, 0, 0x7fff, CMF_NORMAL);
        wprintf(L"bgmenu empty=%ls new=%ls paste=%ls properties=%ls\n", item_state(menu, L"Empty Recycle &Bin"),
                item_state(menu, L"New"), item_state(menu, L"Paste"), item_state(menu, L"Properties"));
        return 0;
    }
    if (!wcscmp(cmd, L"icon"))
    {
        /* where the shell takes the bin's icon from (its file and index) */
        SHFILEINFOW info = { 0 };
        LPITEMIDLIST pidl;

        SHParseDisplayName(BIN, NULL, &pidl, 0, NULL);
        if (!SHGetFileInfoW((const WCHAR *)pidl, 0, &info, sizeof(info), SHGFI_PIDL | SHGFI_ICONLOCATION))
        {
            wprintf(L"no icon\n");
            return 1;
        }
        {
            /* and the icon is there, in the file (shell32 had no -32: a blank page showed) */
            HICON large = NULL;
            UINT got = ExtractIconExW(info.szDisplayName, info.iIcon, &large, NULL, 1);
            wprintf(L"icon %d res=%d\n", info.iIcon, got == 1 && large);
        }
        return 0;
    }
    if (!wcscmp(cmd, L"query")) { print_query(argc > 2 ? argv[2] : NULL); return 0; }
    if (!wcscmp(cmd, L"empty"))
    {
        HRESULT hr = SHEmptyRecycleBinW(NULL, argc > 2 ? argv[2] : NULL, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
        wprintf(L"empty hr=%#lx\n", hr);
        print_query(NULL);
        return 0;
    }
    if (!wcscmp(cmd, L"trash") && argc > 2) return do_delete(argv[2], FALSE);
    if (!wcscmp(cmd, L"trashask") && argc > 3)
    {
        start_answer(argv[3]);
        do_delete(argv[2], TRUE);
        end_answer();
        return 0;
    }
    if (!wcscmp(cmd, L"emptyask") && argc > 2)
    {
        struct empty_args a = { argc > 3 ? argv[3] : NULL, SHERB_NOSOUND };
        HANDLE t;

        start_answer(argv[2]);
        t = CreateThread(NULL, 0, empty_thread, &a, 0, NULL);
        WaitForSingleObject(t, 20000);
        end_answer();
        print_query(NULL);
        return 0;
    }
    if (!wcscmp(cmd, L"verb") && argc > 3)
    {
        char verb[64];

        WideCharToMultiByte(CP_ACP, 0, argv[2], -1, verb, sizeof(verb), NULL, NULL);
        if (!(cm = item_menu(argv[3], &menu))) return 1;
        if (argc > 4) start_answer(argv[4]);
        invoke(cm, verb);
        if (argc > 4) end_answer();
        print_query(NULL);
        return 0;
    }
    if (!wcscmp(cmd, L"props-bin")) return props_bin(argc, argv);
    if (!wcscmp(cmd, L"props-item") && argc > 2)
    {
        HWND sheet;
        WCHAR title[256];

        if (!(cm = item_menu(argv[2], &menu))) return 1;
        invoke(cm, "properties");
        if ((sheet = wait_window(GetCurrentProcessId(), L"#32770", L"Properties", 6000)))
        {
            HWND page = (HWND)SendMessageW(sheet, PSM_GETCURRENTPAGEHWND, 0, 0);
            WCHAR path[MAX_PATH] = L"";

            GetWindowTextW(sheet, title, ARRAY_SIZE(title));
            if (page) GetDlgItemTextW(page, 13001 /* IDC_FPROP_PATH */, path, ARRAY_SIZE(path));
            wprintf(L"sheet %ls | %ls\n", title, path);
            SendMessageW(sheet, PSM_PRESSBUTTON, PSBTN_CANCEL, 0);
        }
        else wprintf(L"no sheet\n");
        Sleep(1500);   /* a program it started has written its mark */
        return 0;
    }
    if (!wcscmp(cmd, L"cwd-report") && argc > 2)
    {
        WCHAR dir[MAX_PATH];
        FILE *f = _wfopen(argv[2], L"w");

        GetCurrentDirectoryW(ARRAY_SIZE(dir), dir);
        if (f) { fwprintf(f, L"%ls\n", dir); fclose(f); }
        return 0;
    }
    if (!wcscmp(cmd, L"addr") && argc > 2)
    {
        HWND frame = wait_window(0, L"ExplorerWClass", NULL, 20000), bar, edit;

        if (!frame) { wprintf(L"no File Explorer\n"); return 1; }
        Sleep(1500);
        if (!(bar = FindWindowExW(frame, NULL, L"SGExplorerAddressBar", NULL)) ||
            !(edit = FindWindowExW(bar, NULL, L"Edit", NULL)))
        {
            wprintf(L"no address bar\n");
            return 1;
        }
        SendMessageW(edit, WM_SETTEXT, 0, (LPARAM)argv[2]);
        PostMessageW(edit, WM_KEYDOWN, VK_RETURN, 0);
        wprintf(L"typed\n");
        return 0;
    }
    if (!wcscmp(cmd, L"title"))
    {
        /* File Explorer's window title, once it has settled on its folder */
        HWND frame = wait_window(0, L"ExplorerWClass", NULL, 20000);
        WCHAR title[256] = L"";

        if (!frame) { wprintf(L"no File Explorer\n"); return 1; }
        Sleep(3000);
        GetWindowTextW(frame, title, ARRAY_SIZE(title));
        wprintf(L"title %ls\n", title);
        return 0;
    }
    if (!wcscmp(cmd, L"idle") && argc > 2)
    {
        /* a window on the desktop keeps the session going */
        HWND hwnd = CreateWindowExW(0, L"Static", L"Away", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 600, 380, 200, 120,
                                    NULL, NULL, NULL, NULL);
        DWORD end = GetTickCount() + _wtoi(argv[2]) * 1000;
        MSG msg;

        while (GetTickCount() < end)
        {
            while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
            Sleep(50);
        }
        DestroyWindow(hwnd);
        return 0;
    }
    wprintf(L"usage\n");
    return 2;
}
