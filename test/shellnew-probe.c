/* shellnew-probe DIR [NAME]: the folder background menu's New submenu for
 * DIR ("item <text>" lines); with NAME, invokes that item. */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

static HMENU new_submenu(HMENU menu)
{
    int i, n = GetMenuItemCount(menu);
    WCHAR text[128];
    for (i = 0; i < n; i++)
    {
        HMENU sub = GetSubMenu(menu, i);
        if (!sub) continue;
        GetMenuStringW(menu, i, text, ARRAYSIZE(text), MF_BYPOSITION);
        if (wcsstr(text, L"New") || wcsstr(text, L"new")) return sub;
    }
    return NULL;
}

int wmain(int argc, WCHAR **argv)
{
    IShellFolder *desktop, *folder;
    IContextMenu *cm;
    ITEMIDLIST *pidl;
    HMENU menu, sub;
    WCHAR text[128];
    int i, n, invoke = -1;

    if (argc < 2) return 2;
    CoInitialize(NULL);
    SHGetDesktopFolder(&desktop);
    if (FAILED(IShellFolder_ParseDisplayName(desktop, NULL, NULL, argv[1], NULL, &pidl, NULL))) return 3;
    if (FAILED(IShellFolder_BindToObject(desktop, pidl, NULL, &IID_IShellFolder, (void **)&folder))) return 4;
    if (FAILED(IShellFolder_CreateViewObject(folder, NULL, &IID_IContextMenu, (void **)&cm))) return 5;
    menu = CreatePopupMenu();
    IContextMenu_QueryContextMenu(cm, menu, 0, 1, 0x7fff, CMF_NORMAL);
    if (!(sub = new_submenu(menu))) { printf("no New submenu\n"); return 6; }
    n = GetMenuItemCount(sub);
    for (i = 0; i < n; i++)
    {
        GetMenuStringW(sub, i, text, ARRAYSIZE(text), MF_BYPOSITION);
        printf("item %ls\n", text);
        if (argc > 2 && !wcscmp(text, argv[2])) invoke = GetMenuItemID(sub, i);
    }
    if (invoke > 0)
    {
        CMINVOKECOMMANDINFO ci = { sizeof(ci) };
        ci.lpVerb = MAKEINTRESOURCEA(invoke - 1);
        printf("invoke %#lx\n", IContextMenu_InvokeCommand(cm, &ci));
    }
    return 0;
}
