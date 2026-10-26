/* menuverbs-probe PATH [--background] [--extended] [--invoke TEXT] -- the File Explorer menu of
 * PATH (with --background: of the folder PATH's background, CreateViewObject) as text
 * ("item TEXT", "sub TEXT" for a submenu's items, "sep" / "subsep" for separators), and the item
 * whose text is TEXT run by its command. A copy named ...args.exe instead appends its command
 * line to C:\menuverbs-args.txt (the program the verbs start). test/menuverbs-gate.sh */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

static UINT found;
static const WCHAR *want;
static IContextMenu2 *cm2;

static void dump(HMENU menu, const char *prefix, const char *sepname)
{
    int i, n = GetMenuItemCount(menu);
    for (i = 0; i < n; i++)
    {
        WCHAR text[256] = L"";
        MENUITEMINFOW mi = { sizeof(mi) };
        mi.fMask = MIIM_ID | MIIM_FTYPE | MIIM_SUBMENU | MIIM_STRING;
        mi.dwTypeData = text; mi.cch = 256;
        GetMenuItemInfoW(menu, i, TRUE, &mi);
        if (mi.fType & MFT_SEPARATOR) { printf("%s\n", sepname); continue; }
        if (mi.hSubMenu && cm2) IContextMenu2_HandleMenuMsg(cm2, WM_INITMENUPOPUP, (WPARAM)mi.hSubMenu, i);
        printf("%s %ls%s\n", prefix, text, mi.hSubMenu ? " >" : "");
        if (want && !mi.hSubMenu && !wcscmp(text, want)) found = mi.wID;
        if (mi.hSubMenu) dump(mi.hSubMenu, "sub", "subsep");
    }
}

int wmain(int argc, WCHAR **argv)
{
    IShellFolder *parent;
    LPCITEMIDLIST child;
    ITEMIDLIST *pidl;
    IContextMenu *cm;
    HMENU menu;
    UINT flags = CMF_NORMAL | CMF_EXPLORE;
    BOOL bg = FALSE;
    WCHAR self[MAX_PATH];
    size_t n = GetModuleFileNameW(NULL, self, MAX_PATH);
    int i;

    if (n >= 8 && !_wcsicmp(self + n - 8, L"args.exe"))
    {
        FILE *f = _wfopen(L"C:\\menuverbs-args.txt", L"a");
        const WCHAR *cl = GetCommandLineW();
        if (!f) return 1;
        if (*cl == '"') { cl = wcschr(cl + 1, '"'); cl = cl ? cl + 1 : L""; }
        else while (*cl && *cl != ' ') cl++;
        while (*cl == ' ') cl++;
        fwprintf(f, L"args[%ls]\n", cl);
        fclose(f);
        return 0;
    }
    if (argc < 2) return 2;
    for (i = 2; i < argc; i++)
    {
        if (!wcscmp(argv[i], L"--extended")) flags |= CMF_EXTENDEDVERBS;
        else if (!wcscmp(argv[i], L"--background")) bg = TRUE;
        else if (!wcscmp(argv[i], L"--invoke") && i + 1 < argc) want = argv[++i];
    }
    CoInitialize(NULL);
    if (bg)
    {
        IShellFolder *desktop, *folder;
        SHGetDesktopFolder(&desktop);
        if (FAILED(SHParseDisplayName(argv[1], NULL, &pidl, 0, NULL)) ||
            FAILED(IShellFolder_BindToObject(desktop, pidl, NULL, &IID_IShellFolder, (void **)&folder)) ||
            FAILED(IShellFolder_CreateViewObject(folder, NULL, &IID_IContextMenu, (void **)&cm)))
        { printf("no menu\n"); return 1; }
    }
    else if (FAILED(SHParseDisplayName(argv[1], NULL, &pidl, 0, NULL)) ||
        FAILED(SHBindToParent(pidl, &IID_IShellFolder, (void **)&parent, &child)) ||
        FAILED(IShellFolder_GetUIObjectOf(parent, NULL, 1, &child, &IID_IContextMenu, NULL, (void **)&cm)))
    { printf("no menu\n"); return 1; }
    IContextMenu_QueryInterface(cm, &IID_IContextMenu2, (void **)&cm2);
    menu = CreatePopupMenu();
    IContextMenu_QueryContextMenu(cm, menu, 0, 1, 0x7fff, flags);
    dump(menu, "item", "sep");
    if (want)
    {
        CMINVOKECOMMANDINFO ici = { sizeof(ici) };
        WCHAR verb[128] = L"";
        if (!found) { printf("no item\n"); return 1; }
        IContextMenu_GetCommandString(cm, found - 1, GCS_VERBW, NULL, (char *)verb, 128);
        printf("verb %ls\n", verb);
        ici.lpVerb = MAKEINTRESOURCEA(found - 1);
        ici.nShow = SW_SHOWNORMAL;
        printf("invoke %08lx\n", IContextMenu_InvokeCommand(cm, &ici));
        Sleep(1500);
    }
    return 0;
}
