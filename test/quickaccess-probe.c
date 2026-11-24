/* quickaccess-probe (patches/sg/9753, test/quickaccess-gate.sh): Quick access as a shell folder,
 * ::{679F85CB-0220-4080-B29B-5540CC05AAB6}, as Blender's file browser reads it. Prints:
 *   PARSE <hr>                      SHCreateItemFromParsingName("shell:::{679f85cb-...}")
 *   NAME <text>                     the folder's display name
 *   ITEM <path> <folder 0|1> <fs 0|1> <name>   each item: SIGDN_FILESYSPATH, SFGAO_FOLDER, SFGAO_FILESYSTEM,
 *                                   SIGDN_NORMALDISPLAY (first pass: the default pins)
 *   PINNED ...                      the same after pinning C:\qa-pinned and counting C:\qa-freq as frequent
 *   INSIDE <name>                   the pinned folder's contents through its Quick access item
 *   KNOWN <path>                    Documents, for comparison */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <knownfolders.h>
#include <stdio.h>

static const CLSID qa = { 0x679f85cb, 0x0220, 0x4080, { 0xb2, 0x9b, 0x55, 0x40, 0xcc, 0x05, 0xaa, 0xb6 } };

static void list(const char *tag, IShellItem *folder)
{
    IEnumShellItems *e;
    IShellItem *item;
    HRESULT hr = IShellItem_BindToHandler(folder, NULL, &BHID_EnumItems, &IID_IEnumShellItems, (void **)&e);

    if (FAILED(hr)) { printf("%s none %08lx\n", tag, hr); return; }
    while (IEnumShellItems_Next(e, 1, &item, NULL) == S_OK)
    {
        WCHAR *path = NULL, *name = NULL;
        SFGAOF a = 0;
        IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &path);
        IShellItem_GetDisplayName(item, SIGDN_NORMALDISPLAY, &name);
        IShellItem_GetAttributes(item, SFGAO_FOLDER | SFGAO_FILESYSTEM, &a);
        printf("%s %ls %d %d %ls\n", tag, path ? path : L"(none)", !!(a & SFGAO_FOLDER), !!(a & SFGAO_FILESYSTEM),
               name ? name : L"(none)");
        if (path && !lstrcmpiW(path, L"C:\\qa-pinned"))
        {
            IEnumShellItems *e2;
            IShellItem *child;
            if (SUCCEEDED(IShellItem_BindToHandler(item, NULL, &BHID_EnumItems, &IID_IEnumShellItems, (void **)&e2)))
            {
                while (IEnumShellItems_Next(e2, 1, &child, NULL) == S_OK)
                {
                    WCHAR *n = NULL;
                    IShellItem_GetDisplayName(child, SIGDN_PARENTRELATIVEPARSING, &n);
                    printf("INSIDE %ls\n", n ? n : L"(none)");
                    CoTaskMemFree(n);
                    IShellItem_Release(child);
                }
                IEnumShellItems_Release(e2);
            }
        }
        CoTaskMemFree(path);
        CoTaskMemFree(name);
        IShellItem_Release(item);
    }
    IEnumShellItems_Release(e);
}

int main(void)
{
    IShellItem *folder;
    WCHAR *s = NULL, pins[512], *p;
    struct { DWORD count; FILETIME last; } freq = { 3 };
    HRESULT hr;

    CoInitialize(NULL);
    hr = SHCreateItemFromParsingName(L"shell:::{679f85cb-0220-4080-b29b-5540cc05aab6}", NULL, &IID_IShellItem, (void **)&folder);
    printf("PARSE %08lx\n", hr);
    if (FAILED(hr)) return 0;
    IShellItem_GetDisplayName(folder, SIGDN_NORMALDISPLAY, &s);
    printf("NAME %ls\n", s ? s : L"(none)");
    CoTaskMemFree(s);
    list("ITEM", folder);
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Documents, 0, NULL, &s))) { printf("KNOWN %ls\n", s); CoTaskMemFree(s); }

    CreateDirectoryW(L"C:\\qa-pinned", NULL);
    CreateDirectoryW(L"C:\\qa-freq", NULL);
    CloseHandle(CreateFileW(L"C:\\qa-pinned\\inside.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
    SHGetKnownFolderPath(&FOLDERID_Documents, 0, NULL, &s);
    lstrcpyW(pins, s);
    p = pins + lstrlenW(pins) + 1;
    lstrcpyW(p, L"C:\\qa-pinned");
    p += lstrlenW(p) + 1;
    *p++ = 0;
    RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Stained Glass\\Explorer\\QuickAccess", L"Pinned", REG_MULTI_SZ,
                    pins, (DWORD)((p - pins) * sizeof(WCHAR)));
    GetSystemTimeAsFileTime(&freq.last);
    RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Stained Glass\\Explorer\\QuickAccess\\Frequent", L"C:\\qa-freq",
                    REG_BINARY, &freq, sizeof(freq));
    list("PINNED", folder);
    IShellItem_Release(folder);
    (void)qa;
    return 0;
}
