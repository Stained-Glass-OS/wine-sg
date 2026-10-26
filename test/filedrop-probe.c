/* filedrop-probe TARGET SOURCE... -- drops SOURCE files (one folder) on TARGET the way File
 * Explorer and the desktop do ("desktop:NAME" is an item of the desktop folder itself): the target item's IDropTarget from its folder's GetUIObjectOf,
 * the sources' IDataObject from theirs, DragEnter, DragOver, Drop with copy/move/link allowed.
 * Prints "target-is-folder 0|1" (the folder's own drop target came back, not the item's),
 * "enter HR EFFECT" and "drop HR EFFECT". args.exe: a copy named ...args.exe appends its command
 * line to C:\filedrop-args.txt (the program a drop starts). "--mklink LNK TARGET [ARGS]" makes a
 * shortcut. test/filedrop-gate.sh */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    IShellFolder *tfolder, *sfolder, *parent;
    LPCITEMIDLIST tchild, kids[8];
    ITEMIDLIST *tpidl, *spidl[8];
    IDropTarget *dt, *folder_dt = NULL;
    IDataObject *data;
    POINTL pt = { 10, 10 };
    DWORD effect;
    HRESULT hr;
    int i;

    {
        /* started as ...args.exe: the program a drop starts; write what it was given */
        WCHAR self[MAX_PATH];
        size_t n = GetModuleFileNameW(NULL, self, MAX_PATH);
        if (n >= 8 && !_wcsicmp(self + n - 8, L"args.exe"))
        {
            FILE *f = _wfopen(L"C:\\filedrop-args.txt", L"a");
            const WCHAR *cl = GetCommandLineW();
            if (!f) return 1;
            /* skip the program's own name */
            if (*cl == '"') { cl = wcschr(cl + 1, '"'); cl = cl ? cl + 1 : L""; }
            else while (*cl && *cl != ' ') cl++;
            while (*cl == ' ') cl++;
            fwprintf(f, L"args[%ls]\n", cl);
            fclose(f);
            return 0;
        }
    }
    CoInitialize(NULL);
    if (argc >= 4 && !wcscmp(argv[1], L"--mklink"))
    {
        IShellLinkW *link;
        IPersistFile *pf;
        if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&link))) return 1;
        IShellLinkW_SetPath(link, argv[3]);
        if (argc > 4) IShellLinkW_SetArguments(link, argv[4]);
        IShellLinkW_QueryInterface(link, &IID_IPersistFile, (void **)&pf);
        hr = IPersistFile_Save(pf, argv[2], TRUE);
        printf("mklink %08lx\n", hr);
        return FAILED(hr);
    }
    if (argc < 3 || argc > 10) return 2;
    if (!wcsncmp(argv[1], L"desktop:", 8))
    {
        /* an item of the desktop itself, as the desktop's own view has it */
        IEnumIDList *items;
        ITEMIDLIST *item;
        STRRET name;
        WCHAR text[MAX_PATH];
        SHGetDesktopFolder(&tfolder);
        tchild = NULL;
        if (SUCCEEDED(IShellFolder_EnumObjects(tfolder, NULL, SHCONTF_NONFOLDERS | SHCONTF_FOLDERS, &items)))
        {
            while (!tchild && IEnumIDList_Next(items, 1, &item, NULL) == S_OK)
                if (SUCCEEDED(IShellFolder_GetDisplayNameOf(tfolder, item, SHGDN_INFOLDER | SHGDN_FORPARSING, &name)) &&
                    SUCCEEDED(StrRetToBufW(&name, item, text, MAX_PATH)) && !_wcsicmp(text, argv[1] + 8))
                    tchild = item;
        }
        if (!tchild) { printf("no desktop item\n"); return 1; }
    }
    else if (FAILED(SHParseDisplayName(argv[1], NULL, &tpidl, 0, NULL)) ||
             FAILED(SHBindToParent(tpidl, &IID_IShellFolder, (void **)&tfolder, &tchild)))
    { printf("no target\n"); return 1; }
    for (i = 2; i < argc; i++)
        if (FAILED(SHParseDisplayName(argv[i], NULL, &spidl[i - 2], 0, NULL))) { printf("no source\n"); return 1; }
    if (FAILED(SHBindToParent(spidl[0], &IID_IShellFolder, (void **)&sfolder, &kids[0]))) return 1;
    for (i = 1; i < argc - 2; i++) kids[i] = ILFindLastID(spidl[i]);
    if (FAILED(IShellFolder_GetUIObjectOf(sfolder, NULL, argc - 2, kids, &IID_IDataObject, NULL, (void **)&data)))
    { printf("no data\n"); return 1; }
    hr = IShellFolder_GetUIObjectOf(tfolder, NULL, 1, &tchild, &IID_IDropTarget, NULL, (void **)&dt);
    printf("target %08lx\n", hr);
    if (FAILED(hr)) return 1;
    /* is it just the folder's own drop target? (what a file with no handler gives) */
    if (SUCCEEDED(IShellFolder_QueryInterface(tfolder, &IID_IDropTarget, (void **)&folder_dt)))
    {
        IUnknown *a, *b;
        IDropTarget_QueryInterface(dt, &IID_IUnknown, (void **)&a);
        IDropTarget_QueryInterface(folder_dt, &IID_IUnknown, (void **)&b);
        printf("target-is-folder %d\n", a == b);
        IUnknown_Release(a); IUnknown_Release(b);
        IDropTarget_Release(folder_dt);
    }
    effect = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
    hr = IDropTarget_DragEnter(dt, data, MK_LBUTTON, pt, &effect);
    printf("enter %08lx %lu\n", hr, effect);
    effect = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
    IDropTarget_DragOver(dt, MK_LBUTTON, pt, &effect);
    if (!effect) { IDropTarget_DragLeave(dt); printf("drop none\n"); return 0; }
    effect = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
    hr = IDropTarget_Drop(dt, data, 0, pt, &effect);
    printf("drop %08lx %lu\n", hr, effect);
    IDropTarget_Release(dt);
    IDataObject_Release(data);
    (void)parent;
    Sleep(1500);  /* a program the drop started writes its arguments */
    return 0;
}
