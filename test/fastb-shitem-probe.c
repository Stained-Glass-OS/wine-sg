/* IShellItem2 property getters and property stores (patch 3600). */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propvarutil.h>
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define SI_KEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) \
    static const PROPERTYKEY name = { { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }, pid }
SI_KEY(SI_ItemNameDisplay, 0xb725f130,0x47ef,0x101a,0xa5,0xf1,0x02,0x60,0x8c,0x9e,0xeb,0xac,10);
SI_KEY(SI_ItemNameDisplayWithoutExtension, 0xb725f130,0x47ef,0x101a,0xa5,0xf1,0x02,0x60,0x8c,0x9e,0xeb,0xac,24);
SI_KEY(SI_ItemPathDisplay, 0xe3e0584c,0xb788,0x4a5a,0xbb,0x20,0x7f,0x5a,0x44,0xc9,0xac,0xdd,7);
SI_KEY(SI_ItemFolderPathDisplay, 0xe3e0584c,0xb788,0x4a5a,0xbb,0x20,0x7f,0x5a,0x44,0xc9,0xac,0xdd,6);
SI_KEY(SI_FileName, 0x41cf5ae0,0xf75a,0x4806,0xbd,0x87,0x59,0xc7,0xd9,0x24,0x8e,0xb9,100);
SI_KEY(SI_FileExtension, 0xe4f10a3c,0x49e6,0x405d,0x82,0x88,0xa2,0x3b,0xd4,0xee,0xaa,0x6c,100);
SI_KEY(SI_ItemType, 0x28636aa6,0x953d,0x11d2,0xb5,0xd6,0x00,0xc0,0x4f,0xd9,0x18,0xd0,11);
SI_KEY(SI_FileAttributes, 0xb725f130,0x47ef,0x101a,0xa5,0xf1,0x02,0x60,0x8c,0x9e,0xeb,0xac,13);
SI_KEY(SI_Size, 0xb725f130,0x47ef,0x101a,0xa5,0xf1,0x02,0x60,0x8c,0x9e,0xeb,0xac,12);
SI_KEY(SI_DateModified, 0xb725f130,0x47ef,0x101a,0xa5,0xf1,0x02,0x60,0x8c,0x9e,0xeb,0xac,14);
SI_KEY(SI_DateCreated, 0xb725f130,0x47ef,0x101a,0xa5,0xf1,0x02,0x60,0x8c,0x9e,0xeb,0xac,15);
SI_KEY(SI_DateAccessed, 0xb725f130,0x47ef,0x101a,0xa5,0xf1,0x02,0x60,0x8c,0x9e,0xeb,0xac,16);

DEFINE_GUID(UNKNOWN_FMT, 0x12345678, 0x1111, 0x2222, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa);

static IShellItem2 *item_for(const WCHAR *path)
{
    IShellItem2 *it = NULL;
    HRESULT hr = SHCreateItemFromParsingName(path, NULL, &IID_IShellItem2, (void **)&it);
    CHECK(hr == S_OK, "SHCreateItemFromParsingName %ls: %lx", path, hr);
    return it;
}

int main(void)
{
    WCHAR dir[MAX_PATH], file[MAX_PATH], tmp[MAX_PATH];
    IShellItem2 *fi, *di;
    PROPVARIANT var;
    WCHAR *str;
    ULONGLONG u64;
    ULONG u32;
    FILETIME ft;
    HRESULT hr;
    HANDLE h;
    DWORD n;
    PROPERTYKEY unknown = { UNKNOWN_FMT, 7 };
    IPropertyStore *store;
    DWORD count;

    CoInitialize(NULL);
    GetTempPathW(MAX_PATH, tmp);
    swprintf(dir, MAX_PATH, L"%lssgshitem%lu", tmp, GetCurrentProcessId());
    CreateDirectoryW(dir, NULL);
    swprintf(file, MAX_PATH, L"%ls\\report.TXT", dir);
    h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, "hello world", 11, &n, NULL);
    CloseHandle(h);

    fi = item_for(file);
    di = item_for(dir);
    if (!fi || !di) { printf("RESULT: FAIL\n"); return 1; }

    hr = IShellItem2_GetString(fi, &SI_FileName, &str);
    CHECK(hr == S_OK && !wcscmp(str, L"report.TXT"), "FileName %lx", hr);
    if (hr == S_OK) CoTaskMemFree(str);
    hr = IShellItem2_GetString(fi, &SI_FileExtension, &str);
    CHECK(hr == S_OK && !wcscmp(str, L".TXT"), "FileExtension %lx", hr);
    if (hr == S_OK) CoTaskMemFree(str);
    hr = IShellItem2_GetString(fi, &SI_ItemNameDisplayWithoutExtension, &str);
    CHECK(hr == S_OK && !wcscmp(str, L"report"), "WithoutExtension %lx", hr);
    if (hr == S_OK) CoTaskMemFree(str);
    hr = IShellItem2_GetString(fi, &SI_ItemNameDisplay, &str);
    CHECK(hr == S_OK && !wcsncmp(str, L"report", 6), "ItemNameDisplay %lx", hr);
    if (hr == S_OK) CoTaskMemFree(str);
    hr = IShellItem2_GetString(fi, &SI_ItemPathDisplay, &str);
    CHECK(hr == S_OK && wcslen(str) > 10 && !wcscmp(str + wcslen(str) - 10, L"report.TXT"), "ItemPathDisplay %lx %ls", hr, hr == S_OK ? str : L"");
    if (hr == S_OK) CoTaskMemFree(str);
    hr = IShellItem2_GetString(fi, &SI_ItemFolderPathDisplay, &str);
    CHECK(hr == S_OK && !wcsicmp(str, dir), "ItemFolderPathDisplay %lx %ls", hr, hr == S_OK ? str : L"");
    if (hr == S_OK) CoTaskMemFree(str);
    hr = IShellItem2_GetString(fi, &SI_ItemType, &str);
    CHECK(hr == S_OK && !wcscmp(str, L".TXT"), "ItemType %lx", hr);
    if (hr == S_OK) CoTaskMemFree(str);

    u64 = 0;
    hr = IShellItem2_GetUInt64(fi, &SI_Size, &u64);
    CHECK(hr == S_OK && u64 == 11, "Size %lx %I64u", hr, u64);
    u32 = 0;
    hr = IShellItem2_GetUInt32(fi, &SI_FileAttributes, &u32);
    CHECK(hr == S_OK && (u32 & FILE_ATTRIBUTE_ARCHIVE), "FileAttributes %lx %lx", hr, u32);
    memset(&ft, 0, sizeof(ft));
    hr = IShellItem2_GetFileTime(fi, &SI_DateModified, &ft);
    CHECK(hr == S_OK && (ft.dwHighDateTime || ft.dwLowDateTime), "DateModified %lx", hr);
    hr = IShellItem2_GetFileTime(fi, &SI_Size, &ft);
    CHECK(hr == TYPE_E_TYPEMISMATCH, "GetFileTime on a size %lx", hr);

    hr = IShellItem2_GetProperty(fi, &SI_Size, &var);
    CHECK(hr == S_OK && var.vt == VT_UI8 && var.uhVal.QuadPart == 11, "GetProperty Size %lx vt=%d", hr, var.vt);
    PropVariantClear(&var);
    hr = IShellItem2_GetProperty(fi, &unknown, &var);
    CHECK(hr == S_OK && var.vt == VT_EMPTY, "GetProperty unknown %lx vt=%d", hr, var.vt);
    PropVariantClear(&var);

    /* a folder */
    hr = IShellItem2_GetString(di, &SI_ItemType, &str);
    CHECK(hr == S_OK && !wcscmp(str, L"Directory"), "folder ItemType %lx", hr);
    if (hr == S_OK) CoTaskMemFree(str);
    hr = IShellItem2_GetProperty(di, &SI_Size, &var);
    CHECK(hr == S_OK && var.vt == VT_EMPTY, "folder has no size %lx vt=%d", hr, var.vt);
    PropVariantClear(&var);
    hr = IShellItem2_GetUInt32(di, &SI_FileAttributes, &u32);
    CHECK(hr == S_OK && (u32 & FILE_ATTRIBUTE_DIRECTORY), "folder attributes %lx", hr);

    /* property stores */
    store = NULL;
    hr = IShellItem2_GetPropertyStore(fi, GPS_DEFAULT, &IID_IPropertyStore, (void **)&store);
    CHECK(hr == S_OK && store, "GetPropertyStore %lx", hr);
    if (store)
    {
        hr = IPropertyStore_GetValue(store, &SI_FileName, &var);
        CHECK(hr == S_OK && var.vt == VT_LPWSTR && !wcscmp(var.pwszVal, L"report.TXT"), "store FileName %lx", hr);
        PropVariantClear(&var);
        hr = IPropertyStore_GetValue(store, &SI_Size, &var);
        CHECK(hr == S_OK && var.vt == VT_UI8 && var.uhVal.QuadPart == 11, "store Size");
        PropVariantClear(&var);
        IPropertyStore_Release(store);
    }
    store = NULL;
    hr = IShellItem2_GetPropertyStoreForKeys(fi, &SI_Size, 1, GPS_DEFAULT, &IID_IPropertyStore, (void **)&store);
    CHECK(hr == S_OK && store, "GetPropertyStoreForKeys %lx", hr);
    if (store)
    {
        count = 99;
        hr = IPropertyStore_GetCount(store, &count);
        CHECK(hr == S_OK && count == 1, "store for one key has %lu", count);
        IPropertyStore_Release(store);
    }
    store = (void *)1;
    hr = IShellItem2_GetPropertyStore(fi, GPS_READWRITE, &IID_IPropertyStore, (void **)&store);
    CHECK(hr == STG_E_ACCESSDENIED && !store, "read-write store of a file %lx", hr);
    hr = IShellItem2_Update(fi, NULL);
    CHECK(hr == S_OK, "Update %lx", hr);

    IShellItem2_Release(fi);
    IShellItem2_Release(di);
    DeleteFileW(file);
    RemoveDirectoryW(dir);
    printf("%d checks, %d failures\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
