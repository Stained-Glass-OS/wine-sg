/* newtext-probe DIR: a folder's background context menu (right-click on its
 * empty space): print its New submenu, then invoke "Text Document" twice
 * (patches/sg/0450). For test/newtext-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    IShellFolder *desk, *folder;
    IContextMenu *cm;
    LPITEMIDLIST pidl;
    HMENU menu = CreatePopupMenu(), sub = NULL;
    WCHAR text[128];
    UINT i, n, text_id = 0;

    CoInitialize(NULL);
    if (argc < 2 || FAILED(SHGetDesktopFolder(&desk)) ||
        FAILED(IShellFolder_ParseDisplayName(desk, NULL, NULL, argv[1], NULL, &pidl, NULL)) ||
        FAILED(IShellFolder_BindToObject(desk, pidl, NULL, &IID_IShellFolder, (void **)&folder)) ||
        FAILED(IShellFolder_CreateViewObject(folder, NULL, &IID_IContextMenu, (void **)&cm)))
    { printf("no menu\n"); return 1; }
    IContextMenu_QueryContextMenu(cm, menu, 0, FCIDM_SHVIEWFIRST, FCIDM_SHVIEWLAST, CMF_NORMAL);
    n = GetMenuItemCount(menu);
    for (i = 0; i < n; i++)
    {
        GetMenuStringW(menu, i, text, ARRAYSIZE(text), MF_BYPOSITION);
        if (!lstrcmpW(text, L"New")) sub = GetSubMenu(menu, i);
    }
    if (!sub) { printf("no New submenu\n"); return 1; }
    n = GetMenuItemCount(sub);
    for (i = 0; i < n; i++)
    {
        GetMenuStringW(sub, i, text, ARRAYSIZE(text), MF_BYPOSITION);
        if (!text[0]) continue;
        printf("new: %ls\n", text);
        if (!lstrcmpW(text, L"&Text Document")) text_id = GetMenuItemID(sub, i);
    }
    if (!text_id) { printf("no Text Document\n"); return 1; }
    for (i = 0; i < 2; i++)
    {
        CMINVOKECOMMANDINFO ici = { sizeof(ici) };
        ici.lpVerb = MAKEINTRESOURCEA(text_id);
        ici.nShow = SW_SHOWNORMAL;
        printf("invoke %#lx\n", IContextMenu_InvokeCommand(cm, &ici));
    }
    return 0;
}
