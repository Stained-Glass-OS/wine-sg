/* ICLRStrongName and the strong name image functions (wine-sg 8202), for test/clrsn-gate.sh. The fixtures
 * (test/net: sgmeta-signed.dll, signed with sgtest.snk by Wine Mono's mcs) are in the cwd. Prints KEY=VALUE.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef struct { void **vtbl; } Obj;
#define CALL(o, i, ret, ...) ((ret (__stdcall *)(Obj*, ##__VA_ARGS__))((o)->vtbl[i]))
#define HR(x) ((unsigned)(x))

static const GUID CLSID_CLRMetaHost_ = {0x9280188d, 0x0e8e, 0x4867, {0xb3, 0x0c, 0x7f, 0xa8, 0x38, 0x84, 0xe8, 0xde}};
static const GUID IID_ICLRMetaHost_ = {0xd332db9e, 0xb9b3, 0x4125, {0x82, 0x07, 0xa1, 0x48, 0x84, 0xf5, 0x32, 0x16}};
static const GUID IID_ICLRRuntimeInfo_ = {0xbd39d1d2, 0xba2f, 0x486a, {0x89, 0xb0, 0xb4, 0xb0, 0xcb, 0x46, 0x68, 0x91}};
static const GUID CLSID_CLRStrongName_ = {0xb79b0acd, 0xf5cd, 0x409b, {0xb5, 0xa5, 0xa1, 0x62, 0x44, 0x61, 0x0b, 0x92}};
static const GUID IID_IUnknown_ = {0x00000000, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const GUID IID_ICLRStrongName_ = {0x9fd93ccf, 0x3280, 0x4391, {0xb3, 0xa9, 0x96, 0xe1, 0xcd, 0xe7, 0x7c, 0x8d}};

/* ICLRStrongName slots */
enum { SN_COMPARE = 9, SN_FREE = 10, SN_GETBLOB = 11, SN_GETBLOBIMAGE = 12, SN_HASHSIZE = 14, SN_VERIFYEX = 23,
       SN_VERIFYIMAGE = 24, SN_TOKEN = 25 };

static BYTE *load(const WCHAR *name, DWORD *size)
{
    HANDLE f = CreateFileW(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    BYTE *data;
    DWORD got;
    if (f == INVALID_HANDLE_VALUE) return NULL;
    *size = GetFileSize(f, NULL);
    data = malloc(*size);
    ReadFile(f, data, *size, &got, NULL);
    CloseHandle(f);
    return data;
}

/* the file as the loader maps an image (SEC_IMAGE) */
static BYTE *map_image(const WCHAR *name, DWORD *size)
{
    HANDLE f = CreateFileW(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL), m;
    MEMORY_BASIC_INFORMATION mbi;
    BYTE *base;
    if (f == INVALID_HANDLE_VALUE) return NULL;
    m = CreateFileMappingW(f, NULL, PAGE_READONLY | SEC_IMAGE, 0, 0, NULL);
    CloseHandle(f);
    if (!m) return NULL;
    base = MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(m);
    if (!base) return NULL;
    VirtualQuery(base, &mbi, sizeof(mbi));
    *size = ((IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew))->OptionalHeader.SizeOfImage;
    return base;
}

int main(void)
{
    typedef HRESULT (WINAPI *create_t)(REFCLSID, REFIID, void **);
    BOOLEAN (WINAPI *pGetBlob)(const WCHAR *, BYTE *, DWORD *);
    BOOLEAN (WINAPI *pVerifyImage)(BYTE *, DWORD, DWORD, DWORD *);
    HMODULE mscoree = LoadLibraryW(L"mscoree.dll");
    create_t create = (create_t)GetProcAddress(mscoree, "CLRCreateInstance");
    Obj *host = NULL, *info = NULL, *sn = NULL, *sn2 = NULL;
    BYTE *file, *buf, *image, *tampered, *token = NULL;
    DWORD size, len, isize, outflags, result, hsize;
    ULONG token_len = 0;
    BOOLEAN verified = FALSE;
    HRESULT hr;

    pGetBlob = (void *)GetProcAddress(mscoree, "StrongNameGetBlob");
    pVerifyImage = (void *)GetProcAddress(mscoree, "StrongNameSignatureVerificationFromImage");
    printf("Exports=%d %d\n", pGetBlob != NULL, pVerifyImage != NULL);
    if (!create || create(&CLSID_CLRMetaHost_, &IID_ICLRMetaHost_, (void **)&host) != S_OK) { printf("Host=0\n"); return 1; }
    hr = CALL(host, 3, HRESULT, LPCWSTR, const GUID *, void **)(host, L"v4.0.30319", &IID_ICLRRuntimeInfo_, (void **)&info);
    if (FAILED(hr)) hr = CALL(host, 3, HRESULT, LPCWSTR, const GUID *, void **)(host, L"v2.0.50727", &IID_ICLRRuntimeInfo_, (void **)&info);
    printf("Runtime=%#x\n", HR(hr));
    if (!info) return 1;
    /* ICLRRuntimeInfo::GetInterface (slot 9) */
    hr = CALL(info, 9, HRESULT, const GUID *, const GUID *, void **)(info, &CLSID_CLRStrongName_, &IID_ICLRStrongName_, (void **)&sn);
    printf("Interface=%#x %d\n", HR(hr), sn != NULL);
    if (!sn) return 1;
    hr = CALL(sn, 0, HRESULT, const GUID *, void **)(sn, &IID_IUnknown_, (void **)&sn2);
    if (sn2) CALL(sn2, 2, ULONG)(sn2);
    printf("QueryInterface=%#x\n", HR(hr));

    hsize = 0;
    hr = CALL(sn, SN_HASHSIZE, HRESULT, ULONG, DWORD *)(sn, 0x8004 /* CALG_SHA1 */, &hsize);
    printf("HashSize=%#x %lu\n", HR(hr), hsize);
    hr = CALL(sn, SN_VERIFYEX, HRESULT, LPCWSTR, BOOLEAN, BOOLEAN *)(sn, L"sgmeta-signed.dll", TRUE, &verified);
    printf("Verify=%#x %d\n", HR(hr), verified);
    hr = CALL(sn, SN_VERIFYEX, HRESULT, LPCWSTR, BOOLEAN, BOOLEAN *)(sn, L"nothere.dll", TRUE, &verified);
    printf("VerifyMissing=%d\n", hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    hr = CALL(sn, SN_TOKEN, HRESULT, LPCWSTR, BYTE **, ULONG *)(sn, L"sgmeta-signed.dll", &token, &token_len);
    printf("Token=%#x %lu\n", HR(hr), token_len);
    if (token) CALL(sn, SN_FREE, HRESULT, BYTE *)(sn, token);
    hr = CALL(sn, SN_COMPARE, HRESULT, LPCWSTR, LPCWSTR, DWORD *)(sn, L"sgmeta-signed.dll", L"sgmeta-signed.dll", &result);
    printf("Compare=%#x %lu\n", HR(hr), result);

    /* the file's bytes */
    file = load(L"sgmeta-signed.dll", &size);
    buf = malloc(size + 16);
    len = 16;
    hr = CALL(sn, SN_GETBLOB, HRESULT, LPCWSTR, BYTE *, DWORD *)(sn, L"sgmeta-signed.dll", buf, &len);
    printf("GetBlob.Small=%d %d\n", FAILED(hr), len == size);
    len = size + 16;
    hr = CALL(sn, SN_GETBLOB, HRESULT, LPCWSTR, BYTE *, DWORD *)(sn, L"sgmeta-signed.dll", buf, &len);
    printf("GetBlob=%#x %d %d\n", HR(hr), len == size, file && !memcmp(buf, file, size));

    /* a mapped image */
    image = map_image(L"sgmeta-signed.dll", &isize);
    printf("Mapped=%d\n", image != NULL);
    if (!image) return 1;
    len = size + 16;
    hr = CALL(sn, SN_GETBLOBIMAGE, HRESULT, BYTE *, DWORD, BYTE *, DWORD *)(sn, image, 64, buf, &len);
    printf("GetBlobFromImage=%#x %lu %d\n", HR(hr), len, !memcmp(buf, image, 64));
    outflags = 0;
    hr = CALL(sn, SN_VERIFYIMAGE, HRESULT, BYTE *, DWORD, DWORD, DWORD *)(sn, image, isize, 1 /* SN_INFLAG_FORCE_VER */, &outflags);
    printf("VerifyImage=%#x %lu\n", HR(hr), outflags);
    outflags = 0;
    verified = pVerifyImage && pVerifyImage(image, isize, 1, &outflags);
    printf("VerifyImage.Export=%d %lu\n", verified, outflags);
    /* the same image, one byte of its code changed */
    tampered = malloc(isize);
    memcpy(tampered, image, isize);
    {
        IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(tampered + ((IMAGE_DOS_HEADER *)tampered)->e_lfanew);
        IMAGE_SECTION_HEADER *sect = IMAGE_FIRST_SECTION(nt);
        tampered[sect->VirtualAddress + 8] ^= 0x5a;
    }
    hr = CALL(sn, SN_VERIFYIMAGE, HRESULT, BYTE *, DWORD, DWORD, DWORD *)(sn, tampered, isize, 1, &outflags);
    printf("VerifyTampered=%d\n", hr == (HRESULT)0x8013141a /* CORSEC_E_INVALID_STRONGNAME */);
    hr = CALL(sn, SN_VERIFYIMAGE, HRESULT, BYTE *, DWORD, DWORD, DWORD *)(sn, file, 10, 1, &outflags);
    printf("VerifyGarbage=%d\n", FAILED(hr));
    printf("done=1\n");
    return 0;
}
