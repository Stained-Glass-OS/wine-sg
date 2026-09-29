/* The folder view's right-click Delete (patches/sg/0514): the item context
 * menu's "delete" verb, invoked with no UI, as File Explorer invokes it. Prints
 * "gone <0|1>" for the test file afterwards. */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    const WCHAR *path = argc > 1 ? argv[1] : L"C:\\users\\Public\\rectest.txt";
    PIDLIST_ABSOLUTE pidl;
    PCUITEMID_CHILD child;
    IShellFolder *parent;
    IContextMenu *cm;
    CMINVOKECOMMANDINFO ici = { sizeof(ici) };
    HMENU menu;
    HANDLE h;

    CoInitialize(NULL);
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { printf("create failed\n"); return 1; }
    CloseHandle(h);
    if (FAILED(SHParseDisplayName(path, NULL, &pidl, 0, NULL))
        || FAILED(SHBindToParent(pidl, &IID_IShellFolder, (void **)&parent, &child))
        || FAILED(IShellFolder_GetUIObjectOf(parent, NULL, 1, &child, &IID_IContextMenu, NULL, (void **)&cm)))
    { printf("no context menu\n"); return 1; }
    menu = CreatePopupMenu();
    IContextMenu_QueryContextMenu(cm, menu, 0, 1, 0x7fff, CMF_NORMAL);
    ici.lpVerb = "delete";
    ici.fMask = CMIC_MASK_FLAG_NO_UI;
    ici.nShow = SW_SHOWNORMAL;
    IContextMenu_InvokeCommand(cm, &ici);
    printf("gone %d\n", GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES);
    fflush(stdout);
    return 0;
}
