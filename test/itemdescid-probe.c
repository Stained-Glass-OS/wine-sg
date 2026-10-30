/* itemdescid-probe: a shell item's PKEY_DescriptionID (patches/sg/6218).
 *
 * IShellItem2::GetProperty(PKEY_DescriptionID) gave S_OK with no value; OneDrive's sync service
 * (FileSyncFSNtfs) asks it of C:\ and read the buffer it expects through a null pointer during
 * setup. It is the item's SHDESCRIPTIONID as a byte vector: a drive's type, a folder, a file.
 *
 * Prints name=value lines; see test/itemdescid-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <stdio.h>

/* {28636AA6-953D-11D2-B5D6-00C04FD918D0}, 2 */
static const PROPERTYKEY key_description_id =
    { { 0x28636aa6, 0x953d, 0x11d2, { 0xb5, 0xd6, 0x00, 0xc0, 0x4f, 0xd9, 0x18, 0xd0 } }, 2 };

static void show(const char *name, const WCHAR *path)
{
    IShellItem2 *item = NULL;
    PROPVARIANT var;
    HRESULT hr;

    PropVariantInit(&var);
    hr = SHCreateItemFromParsingName(path, NULL, &IID_IShellItem2, (void **)&item);
    if (FAILED(hr)) { printf("%s=create:%08lx\n", name, hr); return; }
    hr = IShellItem2_GetProperty(item, &key_description_id, &var);
    if (SUCCEEDED(hr) && var.vt == (VT_VECTOR | VT_UI1) && var.caub.cElems == sizeof(SHDESCRIPTIONID) && var.caub.pElems)
    {
        SHDESCRIPTIONID desc;
        memcpy(&desc, var.caub.pElems, sizeof(desc));
        printf("%s=%08lx,%#lx\n", name, hr, desc.dwDescriptionId);
    }
    else printf("%s=%08lx,vt%u\n", name, hr, var.vt);
    PropVariantClear(&var);
    IShellItem2_Release(item);
}

int main(void)
{
    HANDLE f;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    f = CreateFileW(L"C:\\descid.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(f);
    show("drive", L"C:\\");
    show("folder", L"C:\\windows");
    show("file", L"C:\\descid.txt");
    printf("done=1\n");
    return 0;
}
