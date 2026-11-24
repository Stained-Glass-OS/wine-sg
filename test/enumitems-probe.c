/* enumitems-probe (patches/sg/9753, test/enumitems-gate.sh): IShellItem::BindToHandler(BHID_EnumItems),
 * a folder's items as IEnumShellItems. Prints:
 *   FOLDER <hr> <names...>    C:\enumtest (a.txt, b.txt, sub), SIGDN_PARENTRELATIVEPARSING of each, sorted
 *   FULL <path>               the first item's SIGDN_FILESYSPATH
 *   EMPTY <hr> <next hr>      an empty folder: the enumerator, then its first Next
 *   THISPC <hr> <n>           This PC's items (drives)
 *   FILE <failed 0|1>         a file has no items */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cmp(const void *a, const void *b) { return lstrcmpiW(*(WCHAR **)a, *(WCHAR **)b); }

int main(void)
{
    IShellItem *folder, *item;
    IEnumShellItems *e;
    WCHAR *names[16], *full = NULL;
    ULONG n = 0, i, fetched;
    HRESULT hr;

    CoInitialize(NULL);
    CreateDirectoryW(L"C:\\enumtest", NULL);
    CreateDirectoryW(L"C:\\enumtest\\sub", NULL);
    CreateDirectoryW(L"C:\\enumempty", NULL);
    CloseHandle(CreateFileW(L"C:\\enumtest\\a.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
    CloseHandle(CreateFileW(L"C:\\enumtest\\b.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));

    SHCreateItemFromParsingName(L"C:\\enumtest", NULL, &IID_IShellItem, (void **)&folder);
    hr = IShellItem_BindToHandler(folder, NULL, &BHID_EnumItems, &IID_IEnumShellItems, (void **)&e);
    printf("FOLDER %08lx", hr);
    if (SUCCEEDED(hr))
    {
        while (n < 16 && IEnumShellItems_Next(e, 1, &item, &fetched) == S_OK)
        {
            IShellItem_GetDisplayName(item, SIGDN_PARENTRELATIVEPARSING, &names[n]);
            if (!full) IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &full);
            n++;
            IShellItem_Release(item);
        }
        IEnumShellItems_Release(e);
        qsort(names, n, sizeof(*names), cmp);
        for (i = 0; i < n; i++) printf(" %ls", names[i]);
    }
    printf("\nFULL %ls\n", full ? full : L"(none)");
    IShellItem_Release(folder);

    SHCreateItemFromParsingName(L"C:\\enumempty", NULL, &IID_IShellItem, (void **)&folder);
    hr = IShellItem_BindToHandler(folder, NULL, &BHID_EnumItems, &IID_IEnumShellItems, (void **)&e);
    printf("EMPTY %08lx", hr);
    if (SUCCEEDED(hr)) { printf(" %08lx", IEnumShellItems_Next(e, 1, &item, &fetched)); IEnumShellItems_Release(e); }
    printf("\n");
    IShellItem_Release(folder);

    n = 0;
    hr = SHCreateItemFromParsingName(L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}", NULL, &IID_IShellItem, (void **)&folder);
    if (SUCCEEDED(hr)) hr = IShellItem_BindToHandler(folder, NULL, &BHID_EnumItems, &IID_IEnumShellItems, (void **)&e);
    if (SUCCEEDED(hr))
    {
        while (IEnumShellItems_Next(e, 1, &item, &fetched) == S_OK) { n++; IShellItem_Release(item); }
        IEnumShellItems_Release(e);
    }
    printf("THISPC %08lx %lu\n", hr, n);

    SHCreateItemFromParsingName(L"C:\\enumtest\\a.txt", NULL, &IID_IShellItem, (void **)&folder);
    hr = IShellItem_BindToHandler(folder, NULL, &BHID_EnumItems, &IID_IEnumShellItems, (void **)&e);
    printf("FILE %d\n", FAILED(hr));
    return 0;
}
