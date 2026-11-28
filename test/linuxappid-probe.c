/* linuxappid-gate.sh's helper (patches/sg/9511): a shortcut as sg-shell's
 * sg-linuxapp writes a Linux app's in Start -- a target, its arguments and
 * the app's System.AppUserModel.ID in the shortcut's property store.
 *
 *   linuxappid-probe link LNK TARGET ARGS APPID
 *   linuxappid-probe appid LNK          the shortcut's ID, printed
 */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <propsys.h>
#include <stdio.h>

static const PROPERTYKEY key_aumid = {{0x9f4c2855, 0x9f79, 0x4b39, {0xa8, 0xd0, 0xe1, 0xd4, 0x2d, 0xe1, 0xd5, 0xf3}}, 5};
static const IID iid_store = {0x886d8eeb, 0x8cf2, 0x4446, {0x8d, 0x02, 0xcd, 0xba, 0x1d, 0xbd, 0xcf, 0x99}};

int wmain(int argc, WCHAR **argv)
{
    IShellLinkW *link;
    IPersistFile *file;
    IPropertyStore *store;
    PROPVARIANT v;
    HRESULT hr;

    CoInitialize(NULL);
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&link))) return 2;
    IShellLinkW_QueryInterface(link, &IID_IPersistFile, (void **)&file);
    if (argc == 6 && !lstrcmpW(argv[1], L"link"))
    {
        IShellLinkW_SetPath(link, argv[3]);
        IShellLinkW_SetArguments(link, argv[4]);
        if (FAILED(IShellLinkW_QueryInterface(link, &iid_store, (void **)&store))) return 3;
        PropVariantInit(&v);
        v.vt = VT_LPWSTR;
        v.pwszVal = argv[5];
        hr = IPropertyStore_SetValue(store, &key_aumid, &v);
        if (SUCCEEDED(hr)) hr = IPropertyStore_Commit(store);
        IPropertyStore_Release(store);
        if (FAILED(hr)) return 4;
        return FAILED(IPersistFile_Save(file, argv[2], TRUE)) ? 5 : 0;
    }
    if (argc == 3 && !lstrcmpW(argv[1], L"appid"))
    {
        if (FAILED(IPersistFile_Load(file, argv[2], STGM_READ))) return 5;
        if (FAILED(IShellLinkW_QueryInterface(link, &iid_store, (void **)&store))) return 3;
        PropVariantInit(&v);
        if (SUCCEEDED(IPropertyStore_GetValue(store, &key_aumid, &v)) && v.vt == VT_LPWSTR) printf("%ls\n", v.pwszVal);
        return 0;
    }
    fprintf(stderr, "usage: linuxappid-probe link LNK TARGET ARGS APPID | appid LNK\n");
    return 1;
}
