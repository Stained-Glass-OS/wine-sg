/* menuext-probe PATH -- shell32's context menu for PATH, as File Explorer asks
 * for it: each item ("item ID TEXT"), the test handler's verb and help, and
 * the item invoked by its command and by its verb (test/menuext-gate.sh). */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    IShellFolder *parent;
    LPCITEMIDLIST child;
    ITEMIDLIST *pidl;
    IContextMenu *cm;
    HMENU menu;
    int i, n, test = -1;
    WCHAR text[256];
    CMINVOKECOMMANDINFO ici = { sizeof(ici) };

    if (argc < 2) return 2;
    CoInitialize(NULL);
    if (FAILED(SHParseDisplayName(argv[1], NULL, &pidl, 0, NULL)) ||
        FAILED(SHBindToParent(pidl, &IID_IShellFolder, (void **)&parent, &child)) ||
        FAILED(IShellFolder_GetUIObjectOf(parent, NULL, 1, &child, &IID_IContextMenu, NULL, (void **)&cm)))
    { printf("no menu\n"); return 1; }
    menu = CreatePopupMenu();
    IContextMenu_QueryContextMenu(cm, menu, 0, 1, 0x7fff, CMF_NORMAL | CMF_EXPLORE);
    n = GetMenuItemCount(menu);
    for (i = 0; i < n; i++)
    {
        UINT id = GetMenuItemID(menu, i);
        text[0] = 0;
        GetMenuStringW(menu, i, text, 256, MF_BYPOSITION);
        wprintf(L"item %d %ls\n", (int)id, text);
        if (!wcscmp(text, L"SG Test Action")) test = id;
    }
    if (test > 0)
    {
        WCHAR verb[64] = L"", help[64] = L"";
        IContextMenu_GetCommandString(cm, test - 1, GCS_VERBW, NULL, (char *)verb, 64);
        IContextMenu_GetCommandString(cm, test - 1, GCS_HELPTEXTW, NULL, (char *)help, 64);
        wprintf(L"verb %ls\nhelp %ls\n", verb, help);
        ici.lpVerb = MAKEINTRESOURCEA(test - 1);
        wprintf(L"invoke-id %08lx\n", IContextMenu_InvokeCommand(cm, &ici));
        ici.lpVerb = "sgtest";
        wprintf(L"invoke-verb %08lx\n", IContextMenu_InvokeCommand(cm, &ici));
    }
    IContextMenu_Release(cm);
    return 0;
}
