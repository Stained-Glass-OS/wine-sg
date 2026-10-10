/* mscoree hash entry points (patches/sg/2429), run by test/mscoree-hash-gate.sh:
 * digests of "abc" against the published test vectors, the default algorithm,
 * buffer sizes and the file, handle and ANSI/Unicode variants. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef HRESULT (WINAPI *blob_fn)(BYTE *, DWORD, DWORD *, BYTE *, DWORD, DWORD *);
typedef HRESULT (WINAPI *handle_fn)(HANDLE, DWORD *, BYTE *, DWORD, DWORD *);
typedef HRESULT (WINAPI *filew_fn)(const WCHAR *, DWORD *, BYTE *, DWORD, DWORD *);
typedef HRESULT (WINAPI *filea_fn)(const char *, DWORD *, BYTE *, DWORD, DWORD *);

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  " __VA_ARGS__); printf("\n"); } } while (0)

static const struct { DWORD alg; const char *name; const char *hex; } vectors[] =
{
    { 0,      "default", "a9993e364706816aba3e25717850c26c9cd0d89d" },
    { 0x8004, "sha1",    "a9993e364706816aba3e25717850c26c9cd0d89d" },
    { 0x8003, "md5",     "900150983cd24fb0d6963f7d28e17f72" },
    { 0x800c, "sha256",  "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
};

static void hex(const BYTE *b, DWORD n, char *out)
{
    DWORD i;
    for (i = 0; i < n; i++) sprintf(out + 2 * i, "%02x", b[i]);
}

int main(void)
{
    HMODULE m = LoadLibraryA("mscoree.dll");
    blob_fn GetHashFromBlob = (void *)GetProcAddress(m, "GetHashFromBlob");
    handle_fn GetHashFromHandle = (void *)GetProcAddress(m, "GetHashFromHandle");
    filew_fn GetHashFromFileW = (void *)GetProcAddress(m, "GetHashFromFileW"),
             GetHashFromAssemblyFileW = (void *)GetProcAddress(m, "GetHashFromAssemblyFileW");
    filea_fn GetHashFromFile = (void *)GetProcAddress(m, "GetHashFromFile"),
             GetHashFromAssemblyFile = (void *)GetProcAddress(m, "GetHashFromAssemblyFile");
    char path[MAX_PATH], text[100];
    WCHAR wpath[MAX_PATH];
    BYTE out[64];
    DWORD alg, len, written;
    HANDLE file;
    HRESULT hr;
    int i;

    if (!GetHashFromBlob || !GetHashFromAssemblyFile) { printf("FAIL  exports missing\n"); return 1; }

    GetTempPathA(sizeof(path), path);
    strcat(path, "sg-hash-probe.bin");
    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAX_PATH);
    file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(file, "abc", 3, &written, NULL);

    for (i = 0; i < 4; i++)
    {
        alg = vectors[i].alg; len = 0; memset(out, 0, sizeof(out));
        hr = GetHashFromBlob((BYTE *)"abc", 3, &alg, out, sizeof(out), &len);
        hex(out, len, text);
        CHECK(hr == S_OK && !strcmp(text, vectors[i].hex), "%s blob: %#lx %s", vectors[i].name, hr, text);
        CHECK(alg == (vectors[i].alg ? vectors[i].alg : 0x8004), "%s: algorithm reported %#lx", vectors[i].name, alg);

        alg = vectors[i].alg; len = 0; memset(out, 0, sizeof(out));
        hr = GetHashFromHandle(file, &alg, out, sizeof(out), &len);
        hex(out, len, text);
        CHECK(hr == S_OK && !strcmp(text, vectors[i].hex), "%s handle: %#lx %s", vectors[i].name, hr, text);

        alg = vectors[i].alg; len = 0; memset(out, 0, sizeof(out));
        hr = GetHashFromFileW(wpath, &alg, out, sizeof(out), &len);
        hex(out, len, text);
        CHECK(hr == S_OK && !strcmp(text, vectors[i].hex), "%s file W: %#lx %s", vectors[i].name, hr, text);

        alg = vectors[i].alg; len = 0; memset(out, 0, sizeof(out));
        hr = GetHashFromFile(path, &alg, out, sizeof(out), &len);
        hex(out, len, text);
        CHECK(hr == S_OK && !strcmp(text, vectors[i].hex), "%s file A: %#lx %s", vectors[i].name, hr, text);

        alg = vectors[i].alg; len = 0; memset(out, 0, sizeof(out));
        hr = GetHashFromAssemblyFileW(wpath, &alg, out, sizeof(out), &len);
        hex(out, len, text);
        CHECK(hr == S_OK && !strcmp(text, vectors[i].hex), "%s assembly W: %#lx %s", vectors[i].name, hr, text);

        alg = vectors[i].alg; len = 0; memset(out, 0, sizeof(out));
        hr = GetHashFromAssemblyFile(path, &alg, out, sizeof(out), &len);
        hex(out, len, text);
        CHECK(hr == S_OK && !strcmp(text, vectors[i].hex), "%s assembly A: %#lx %s", vectors[i].name, hr, text);
    }

    /* a buffer that is too small: the size needed comes back */
    alg = 0; len = 0;
    hr = GetHashFromBlob((BYTE *)"abc", 3, &alg, out, 10, &len);
    CHECK(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) && len == 20, "small buffer: %#lx, %lu", hr, len);

    /* nothing, a bad algorithm, a missing file */
    alg = 0; len = 0;
    CHECK(GetHashFromBlob((BYTE *)"", 0, &alg, out, sizeof(out), &len) == S_OK && len == 20, "empty blob");
    alg = 0xdead;
    CHECK(FAILED(GetHashFromBlob((BYTE *)"abc", 3, &alg, out, sizeof(out), &len)), "bad algorithm accepted");
    CHECK(GetHashFromBlob(NULL, 3, &alg, out, sizeof(out), &len) == E_POINTER, "NULL blob");
    alg = 0;
    hr = GetHashFromFileW(L"Z:\\no\\such\\file", &alg, out, sizeof(out), &len);
    CHECK(hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND) || hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "missing file: %#lx", hr);

    CloseHandle(file);
    DeleteFileA(path);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
