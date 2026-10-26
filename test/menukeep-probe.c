/* menukeep-probe PATH... -- shell32's context menu for one or more items of one folder, as File
 * Explorer asks for it: "count N" (how many "SG Keep Action" items it has) and "invoke HR" (that
 * item run by its command, after the menu was built; test/menukeep-gate.sh). */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    IShellFolder *parent;
    LPCITEMIDLIST kids[8];
    ITEMIDLIST *pidl[8];
    IContextMenu *cm;
    HMENU menu;
    int i, n, count = 0, id = -1;
    WCHAR text[256];
    CMINVOKECOMMANDINFO ici = { sizeof(ici) };

    if (argc < 2 || argc > 9) return 2;
    CoInitialize(NULL);
    for (i = 1; i < argc; i++)
        if (FAILED(SHParseDisplayName(argv[i], NULL, &pidl[i - 1], 0, NULL))) { printf("no item\n"); return 1; }
    if (FAILED(SHBindToParent(pidl[0], &IID_IShellFolder, (void **)&parent, &kids[0]))) { printf("no parent\n"); return 1; }
    for (i = 1; i < argc - 1; i++) kids[i] = ILFindLastID(pidl[i]);
    if (FAILED(IShellFolder_GetUIObjectOf(parent, NULL, argc - 1, kids, &IID_IContextMenu, NULL, (void **)&cm)))
    { printf("no menu\n"); return 1; }
    menu = CreatePopupMenu();
    IContextMenu_QueryContextMenu(cm, menu, 0, 1, 0x7fff, CMF_NORMAL | CMF_EXPLORE);
    n = GetMenuItemCount(menu);
    for (i = 0; i < n; i++)
    {
        text[0] = 0;
        GetMenuStringW(menu, i, text, 256, MF_BYPOSITION);
        if (!wcscmp(text, L"SG Keep Action")) { count++; if (id < 0) id = GetMenuItemID(menu, i); }
    }
    printf("count %d\n", count);
    /* File Explorer runs the command after the menu is up and the user picked it */
    IShellFolder_Release(parent);
    if (id > 0)
    {
        ici.lpVerb = MAKEINTRESOURCEA(id - 1);
        printf("invoke %08lx\n", IContextMenu_InvokeCommand(cm, &ici));
    }
    IContextMenu_Release(cm);
    DestroyMenu(menu);
    return 0;
}
