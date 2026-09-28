/* drivemenu-probe X: the items of drive X:'s context menu in File Explorer */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    WCHAR root[4] = L"D:\\", text[128];
    IShellFolder *parent;
    ITEMIDLIST *pidl;
    const ITEMIDLIST *child;
    IContextMenu *menu;
    HMENU hmenu;
    int i, n;

    if (argc > 1) root[0] = argv[1][0];
    CoInitialize(NULL);
    if (SHParseDisplayName(root, NULL, &pidl, 0, NULL) || SHBindToParent(pidl, &IID_IShellFolder, (void **)&parent, &child))
    {
        printf("no drive\n");
        return 1;
    }
    if (parent->lpVtbl->GetUIObjectOf(parent, NULL, 1, &child, &IID_IContextMenu, NULL, (void **)&menu))
    {
        printf("no menu\n");
        return 1;
    }
    hmenu = CreatePopupMenu();
    menu->lpVtbl->QueryContextMenu(menu, hmenu, 0, 1, 0x7fff, CMF_NORMAL | CMF_CANRENAME);
    n = GetMenuItemCount(hmenu);
    for (i = 0; i < n; i++)
    {
        MENUITEMINFOW mi = { sizeof(mi), MIIM_STRING | MIIM_FTYPE | MIIM_STATE };
        mi.dwTypeData = text; mi.cch = ARRAYSIZE(text);
        if (!GetMenuItemInfoW(hmenu, i, TRUE, &mi) || (mi.fType & MFT_SEPARATOR)) continue;
        printf("%s%ls\n", (mi.fState & MFS_DEFAULT) ? "*" : "", text);
    }
    return 0;
}
