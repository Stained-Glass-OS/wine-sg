/* StgCreateDocfile / StgCreateStorageEx with STGM_CONVERT: the old contents of an existing file become the
 * stream "CONTENTS" of the new compound file, STG_S_CONVERTED says so; a file that is not there is just
 * created.  Without it (STGM_FAILIFTHERE) an existing file is refused.  The flag used to be treated as
 * "create new" with a FIXME. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static void write_file(const WCHAR *path, const void *data, DWORD size)
{
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    DWORD written;
    WriteFile(f, data, size, &written, NULL);
    CloseHandle(f);
}

static int stream_has(IStorage *stg, const WCHAR *name, const void *data, DWORD size)
{
    IStream *stream;
    BYTE *buf = calloc(1, size + 16);
    ULONG got = 0;
    HRESULT hr = IStorage_OpenStream(stg, name, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stream);
    int ok = 0;

    if (hr == S_OK)
    {
        STATSTG st;
        IStream_Read(stream, buf, size + 16, &got);
        IStream_Stat(stream, &st, STATFLAG_NONAME);
        ok = got == size && st.cbSize.QuadPart == size && !memcmp(buf, data, size);
        IStream_Release(stream);
    }
    free(buf);
    return ok;
}

int main(void)
{
    WCHAR dir[MAX_PATH], path[MAX_PATH], path2[MAX_PATH];
    static const char legacy[] = "legacy file bytes, not a compound file\r\n\x01\x02\x03";
    static BYTE big[100000];
    IStorage *stg, *old;
    IStream *stream;
    HRESULT hr;
    DWORD i;
    STATSTG st;

    for (i = 0; i < sizeof(big); i++) big[i] = (BYTE)(i * 7 + i / 251);
    GetTempPathW(MAX_PATH, dir);
    swprintf(path, MAX_PATH, L"%ssg_convert_%lu.bin", dir, GetCurrentProcessId());
    swprintf(path2, MAX_PATH, L"%ssg_convert2_%lu.bin", dir, GetCurrentProcessId());
    DeleteFileW(path);
    DeleteFileW(path2);

    /* a file that is not there is created */
    hr = StgCreateDocfile(path, STGM_CONVERT | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg);
    check(hr == S_OK, "new file: S_OK");
    if (hr == S_OK)
    {
        hr = IStorage_OpenStream(stg, L"CONTENTS", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stream);
        check(hr == STG_E_FILENOTFOUND, "new file: no CONTENTS stream");
        IStorage_Release(stg);
    }
    DeleteFileW(path);

    /* an existing plain file */
    write_file(path, legacy, sizeof(legacy) - 1);
    hr = StgCreateDocfile(path, STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg);
    check(hr == STG_E_FILEALREADYEXISTS, "plain file without STGM_CONVERT is STG_E_FILEALREADYEXISTS");
    hr = StgCreateDocfile(path, STGM_CONVERT | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg);
    check(hr == STG_S_CONVERTED, "plain file: STG_S_CONVERTED");
    if (SUCCEEDED(hr))
    {
        check(stream_has(stg, L"CONTENTS", legacy, sizeof(legacy) - 1), "plain file: CONTENTS holds the old bytes");
        IStorage_Release(stg);
        check(StgIsStorageFile(path) == S_OK, "the file is a compound file now");
        hr = StgOpenStorage(path, NULL, STGM_READ | STGM_SHARE_DENY_WRITE, NULL, 0, &old);
        check(hr == S_OK && stream_has(old, L"CONTENTS", legacy, sizeof(legacy) - 1), "reopened: CONTENTS is there");
        if (hr == S_OK) IStorage_Release(old);
    }
    DeleteFileW(path);

    /* a large file, through StgCreateStorageEx */
    write_file(path, big, sizeof(big));
    hr = StgCreateStorageEx(path, STGM_CONVERT | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, STGFMT_DOCFILE, 0, NULL, NULL,
                            &IID_IStorage, (void **)&stg);
    check(hr == STG_S_CONVERTED, "large file through StgCreateStorageEx: STG_S_CONVERTED");
    if (SUCCEEDED(hr))
    {
        check(stream_has(stg, L"CONTENTS", big, sizeof(big)), "large file: CONTENTS holds all 100000 bytes");
        IStorage_Release(stg);
    }
    DeleteFileW(path);

    /* an empty file */
    write_file(path, "", 0);
    hr = StgCreateDocfile(path, STGM_CONVERT | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg);
    check(hr == STG_S_CONVERTED, "empty file: STG_S_CONVERTED");
    if (SUCCEEDED(hr))
    {
        hr = IStorage_OpenStream(stg, L"CONTENTS", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stream);
        check(hr == S_OK, "empty file: an empty CONTENTS stream exists");
        if (hr == S_OK)
        {
            IStream_Stat(stream, &st, STATFLAG_NONAME);
            check(st.cbSize.QuadPart == 0, "empty file: it is empty");
            IStream_Release(stream);
        }
        IStorage_Release(stg);
    }
    DeleteFileW(path);

    /* an existing compound file is converted as well: it ends up inside CONTENTS, its own streams gone */
    hr = StgCreateDocfile(path, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &old);
    check(hr == S_OK, "an old compound file");
    if (hr == S_OK)
    {
        IStorage_CreateStream(old, L"OldStream", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stream);
        IStream_Write(stream, "old data", 8, NULL);
        IStream_Release(stream);
        IStorage_Release(old);
    }
    {
        BYTE *raw = malloc(1 << 20);
        DWORD rawsize = 0;
        HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
        ReadFile(f, raw, 1 << 20, &rawsize, NULL);
        CloseHandle(f);
        hr = StgCreateDocfile(path, STGM_CONVERT | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg);
        check(hr == STG_S_CONVERTED, "compound file: STG_S_CONVERTED");
        if (SUCCEEDED(hr))
        {
            check(stream_has(stg, L"CONTENTS", raw, rawsize), "compound file: CONTENTS holds the old file's bytes");
            hr = IStorage_OpenStream(stg, L"OldStream", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stream);
            check(hr == STG_E_FILENOTFOUND, "compound file: the old streams are not in the new storage");
            IStorage_Release(stg);
        }
        free(raw);
    }
    DeleteFileW(path);

    /* STGM_CONVERT with STGM_DELETEONRELEASE is invalid */
    hr = StgCreateDocfile(path2, STGM_CONVERT | STGM_DELETEONRELEASE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg);
    check(FAILED(hr), "STGM_CONVERT | STGM_DELETEONRELEASE fails");
    if (SUCCEEDED(hr)) IStorage_Release(stg);
    DeleteFileW(path2);

    /* the two creation flags together are invalid */
    hr = StgCreateDocfile(path2, STGM_CONVERT | STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg);
    check(FAILED(hr), "STGM_CONVERT | STGM_CREATE fails");
    if (SUCCEEDED(hr)) IStorage_Release(stg);
    DeleteFileW(path2);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
