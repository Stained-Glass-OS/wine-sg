/* trashmount-probe: "probe <path>" deletes <path> to the Recycle Bin
 * (SHFileOperation, FOF_ALLOWUNDO); "probe /restore <name>" restores the
 * Recycle Bin item named <name> with its "undelete" verb, as File Explorer
 * does. The gate looks at where the file went. */
#define COBJMACROS
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>

static int restore(const WCHAR *name)
{
    IShellFolder *desktop, *bin;
    IEnumIDList *items;
    LPITEMIDLIST bin_pidl, item;
    STRRET sr;
    WCHAR shown[MAX_PATH];
    int found = 0;

    CoInitialize(NULL);
    if (FAILED(SHGetDesktopFolder(&desktop))) return 3;
    if (FAILED(SHGetSpecialFolderLocation(NULL, CSIDL_BITBUCKET, &bin_pidl))) return 3;
    if (FAILED(IShellFolder_BindToObject(desktop, bin_pidl, NULL, &IID_IShellFolder, (void **)&bin))) return 3;
    if (IShellFolder_EnumObjects(bin, NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &items) != S_OK) return 3;
    while (IEnumIDList_Next(items, 1, &item, NULL) == S_OK)
    {
        IContextMenu *menu;
        CMINVOKECOMMANDINFO ci = { sizeof(ci) };
        HRESULT hr;

        if (FAILED(IShellFolder_GetDisplayNameOf(bin, item, SHGDN_INFOLDER, &sr))) continue;
        StrRetToBufW(&sr, item, shown, MAX_PATH);
        if (!wcsstr(shown, name)) continue;
        found = 1;
        hr = IShellFolder_GetUIObjectOf(bin, NULL, 1, (LPCITEMIDLIST *)&item, &IID_IContextMenu, NULL, (void **)&menu);
        if (FAILED(hr)) { printf("no menu %#lx\n", hr); return 4; }
        ci.lpVerb = "undelete";   /* the Recycle Bin's verb (GetCommandString) */
        hr = IContextMenu_InvokeCommand(menu, &ci);
        printf("restore %#lx\n", hr);
        return FAILED(hr) ? 1 : 0;
    }
    printf("not in the Recycle Bin\n");
    return found ? 0 : 5;
}

int wmain(int argc, WCHAR **argv)
{
    SHFILEOPSTRUCTW op = {0};
    WCHAR from[MAX_PATH + 2] = {0};
    int ret;

    if (argc == 3 && !lstrcmpW(argv[1], L"/restore")) return restore(argv[2]);
    if (argc < 2) return 2;
    lstrcpynW(from, argv[1], MAX_PATH);
    op.wFunc = FO_DELETE;
    op.pFrom = from;   /* double-NUL terminated: the array is zeroed */
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    ret = SHFileOperationW(&op);
    printf("ret %d aborted %d\n", ret, op.fAnyOperationsAborted);
    return ret ? 1 : 0;
}
