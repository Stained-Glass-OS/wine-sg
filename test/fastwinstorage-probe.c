/* windows.storage: StorageFolder / StorageFile / KnownFolders through the raw vtables */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>

typedef void *HS;
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define VT(obj) (*(void ***)(obj))
#define CALL(obj, idx, ret, ...) (((ret (WINAPI *)(void *, ...))VT(obj)[idx])(obj, ##__VA_ARGS__))

typedef HRESULT (WINAPI *WCS)(const WCHAR *, UINT32, HS *);
typedef HRESULT (WINAPI *RGAF)(HS, const GUID *, void **);
typedef const WCHAR *(WINAPI *WGSRB)(HS, UINT32 *);
static WCS create;
static WGSRB raw;

static const GUID IID_IStorageFolderStatics_ = {0x08f327ff, 0x85d5, 0x48b9, {0xae, 0xe9, 0x28, 0x51, 0x1e, 0x33, 0x9f, 0x9f}};
static const GUID IID_IStorageFileStatics_ = {0x5984c710, 0xdaf2, 0x43c8, {0x8b, 0xb4, 0xa4, 0xd3, 0xea, 0xcf, 0xd0, 0x3f}};
static const GUID IID_IKnownFoldersStatics_ = {0x5a2a7520, 0x4802, 0x452d, {0x9a, 0xd9, 0x43, 0x51, 0xad, 0xa7, 0xec, 0x35}};
static const GUID IID_IStorageItem_ = {0x4207a996, 0xca2f, 0x42f7, {0xbd, 0xe8, 0x8b, 0x10, 0x45, 0x7a, 0x7f, 0x30}};
static const GUID IID_IInputStream_ = {0x905a0fe2, 0xbc53, 0x11df, {0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda}};
static const GUID IID_IClosable_ = {0x30d5a829, 0x7fa4, 0x4026, {0x83, 0xbb, 0xd7, 0x5b, 0xae, 0x4e, 0xa9, 0x9e}};
static const GUID IID_IBufferFactory_ = {0x71af914d, 0xc10f, 0x484b, {0xbc, 0x50, 0x14, 0xbc, 0x62, 0x3b, 0x3a, 0x27}};
static const GUID IID_IBufferByteAccess_ = {0x905a0fef, 0xbc53, 0x11df, {0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda}};

static HS mk(const WCHAR *s) { HS h = NULL; create(s, wcslen(s), &h); return h; }

/* the result of a completed IAsyncOperation / IAsyncAction (GetResults is slot 8), releasing it */
static HRESULT done(void *op, void **out)
{
    HRESULT hr = ((HRESULT (WINAPI *)(void *, void **))VT(op)[8])(op, out);
    CALL(op, 2, ULONG);
    return hr;
}

static void *folder_at(void *statics, const WCHAR *path, HRESULT *hr)
{
    void *op = NULL, *folder = NULL;
    HS h = mk(path);
    CALL(statics, 6, HRESULT, h, &op);
    *hr = done(op, &folder);
    return folder;
}

static void check_name(void *item_or_obj, const WCHAR *expect, int line)
{
    void *item = NULL;
    HS name = NULL;
    HRESULT hr = CALL(item_or_obj, 0, HRESULT, &IID_IStorageItem_, &item);
    if (hr == S_OK) hr = CALL(item, 11, HRESULT, &name);
    if (!(hr == S_OK && !wcscmp(raw(name, NULL), expect)))
    {
        fails++;
        printf("FAIL line %d: name %ls, expected %ls (hr %lx)\n", line, name ? raw(name, NULL) : L"?", expect, hr);
    }
    if (item) CALL(item, 2, ULONG);
}

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    RGAF get_factory = (RGAF)GetProcAddress(combase, "RoGetActivationFactory");
    HRESULT (WINAPI *ro_init)(int) = (void *)GetProcAddress(combase, "RoInitialize");
    void *folder_statics = NULL, *file_statics = NULL, *known = NULL, *root, *op = NULL, *file = NULL, *vec = NULL;
    void *item = NULL, *action = NULL, *stream = NULL, *input = NULL, *buffer = NULL, *factory = NULL, *access = NULL;
    WCHAR dir[MAX_PATH], path[MAX_PATH], temp[MAX_PATH];
    HRESULT hr;
    UINT32 count = 0, got = 0;
    UINT64 size = 0;
    BYTE *bytes = NULL;
    HANDLE h;
    DWORD written;
    void *rop = NULL;

    create = (WCS)GetProcAddress(combase, "WindowsCreateString");
    raw = (WGSRB)GetProcAddress(combase, "WindowsGetStringRawBuffer");
    ro_init(1);

    hr = get_factory(mk(L"Windows.Storage.StorageFolder"), &IID_IStorageFolderStatics_, &folder_statics);
    CHECK(hr == S_OK && folder_statics, "StorageFolder factory %lx", hr);
    hr = get_factory(mk(L"Windows.Storage.StorageFile"), &IID_IStorageFileStatics_, &file_statics);
    CHECK(hr == S_OK && file_statics, "StorageFile factory %lx", hr);
    hr = get_factory(mk(L"Windows.Storage.KnownFolders"), &IID_IKnownFoldersStatics_, &known);
    CHECK(hr == S_OK && known, "KnownFolders factory %lx", hr);
    if (!folder_statics || !file_statics) return 1;

    GetTempPathW(MAX_PATH, temp);
    swprintf(dir, MAX_PATH, L"%lsfastws_%lu", temp, GetCurrentProcessId());
    CreateDirectoryW(dir, NULL);

    root = folder_at(folder_statics, dir, &hr);
    CHECK(hr == S_OK && root, "GetFolderFromPathAsync %lx", hr);
    check_name(root, wcsrchr(dir, '\\') + 1, __LINE__);
    folder_at(folder_statics, L"relative", &hr);
    CHECK(hr == E_INVALIDARG, "relative path %lx", hr);
    swprintf(path, MAX_PATH, L"%ls\\absent", dir);
    folder_at(folder_statics, path, &hr);
    CHECK(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "absent folder %lx", hr);

    /* CreateFileAsync(name, options): slot 7 */
    CALL(root, 7, HRESULT, mk(L"g.txt"), 2, &op);
    hr = done(op, &file);
    CHECK(hr == S_OK && file, "CreateFileAsync %lx", hr);
    check_name(file, L"g.txt", __LINE__);
    CALL(root, 7, HRESULT, mk(L"g.txt"), 2, &op);
    hr = done(op, &item);
    CHECK(hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS), "FailIfExists %lx", hr);
    CALL(root, 7, HRESULT, mk(L"g.txt"), 0, &op);
    hr = done(op, &item);
    CHECK(hr == S_OK && item, "GenerateUniqueName %lx", hr);
    check_name(item, L"g (1).txt", __LINE__);

    swprintf(path, MAX_PATH, L"%ls\\g.txt", dir);
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    WriteFile(h, "hello", 5, &written, NULL);
    CloseHandle(h);

    /* IStorageFile::OpenAsync(mode): slot 8, then read it back */
    hr = CALL(file, 8, HRESULT, 0, &op);
    CHECK(hr == S_OK, "OpenAsync %lx", hr);
    hr = done(op, &stream);
    CHECK(hr == S_OK && stream, "stream %lx", hr);
    hr = CALL(stream, 6, HRESULT, &size);
    CHECK(hr == S_OK && size == 5, "size %lx %I64u", hr, size);
    hr = CALL(stream, 0, HRESULT, &IID_IInputStream_, &input);
    CHECK(hr == S_OK && input, "QI IInputStream %lx", hr);
    hr = get_factory(mk(L"Windows.Storage.Streams.Buffer"), &IID_IBufferFactory_, &factory);
    CHECK(hr == S_OK, "Buffer factory %lx", hr);
    hr = CALL(factory, 6, HRESULT, 16, &buffer);
    CHECK(hr == S_OK && buffer, "Buffer.Create %lx", hr);
    hr = CALL(input, 6, HRESULT, buffer, 16, 0, &rop);   /* ReadAsync */
    CHECK(hr == S_OK, "ReadAsync %lx", hr);
    hr = ((HRESULT (WINAPI *)(void *, void **))VT(rop)[10])(rop, &item);   /* GetResults of WithProgress */
    CHECK(hr == S_OK, "read results %lx", hr);
    CALL(buffer, 0, HRESULT, &IID_IBufferByteAccess_, &access);
    CALL(access, 3, HRESULT, &bytes);
    CALL(buffer, 7, HRESULT, &got);                       /* Length */
    CHECK(got == 5 && bytes && !memcmp(bytes, "hello", 5), "read back %u", got);

    {   /* closing the stream lets go of the file */
        void *closable = NULL;
        hr = CALL(stream, 0, HRESULT, &IID_IClosable_, &closable);
        CHECK(hr == S_OK && closable, "QI IClosable %lx", hr);
        hr = CALL(closable, 6, HRESULT);
        CHECK(hr == S_OK, "Close %lx", hr);
        CALL(closable, 2, ULONG);
        CALL(input, 2, ULONG);
        CALL(stream, 2, ULONG);
    }

    /* GetFilesAsync: slot 13 */
    CALL(root, 13, HRESULT, &op);
    hr = done(op, &vec);
    CHECK(hr == S_OK && vec, "GetFilesAsync %lx", hr);
    hr = CALL(vec, 7, HRESULT, &count);
    CHECK(hr == S_OK && count == 2, "file count %lx %u", hr, count);
    {   /* the files come sorted by name */
        void *first = NULL;
        hr = CALL(vec, 6, HRESULT, 0, &first);
        CHECK(hr == S_OK && first, "GetAt %lx", hr);
        if (first) check_name(first, L"g (1).txt", __LINE__);
        hr = CALL(vec, 6, HRESULT, 2, &first);
        CHECK(hr == (HRESULT)0x8000000b, "GetAt out of range %lx", hr);
    }

    /* RenameAsync(name): IStorageItem slot 6, then DeleteAsync() slot 8 */
    hr = CALL(file, 0, HRESULT, &IID_IStorageItem_, &item);
    CHECK(hr == S_OK && item, "QI IStorageItem %lx", hr);
    hr = CALL(item, 6, HRESULT, mk(L"moved.dat"), &action);
    CHECK(hr == S_OK, "RenameAsync %lx", hr);
    hr = done(action, NULL);
    CHECK(hr == S_OK, "rename result %lx", hr);
    swprintf(path, MAX_PATH, L"%ls\\moved.dat", dir);
    CHECK(GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES, "renamed file is missing");
    check_name(file, L"moved.dat", __LINE__);
    CALL(item, 8, HRESULT, &action);
    hr = done(action, NULL);
    CHECK(hr == S_OK && GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES, "delete %lx", hr);

    /* a known folder is a real directory */
    if (known)
    {
        void *docs = NULL, *name = NULL;
        HS p = NULL;
        hr = CALL(known, 9, HRESULT, &docs);                /* DocumentsLibrary */
        CHECK(hr == S_OK && docs, "DocumentsLibrary %lx", hr);
        if (docs)
        {
            CALL(docs, 0, HRESULT, &IID_IStorageItem_, &name);
            CALL(name, 12, HRESULT, &p);                     /* Path */
            CHECK(p && (GetFileAttributesW(raw(p, NULL)) & FILE_ATTRIBUTE_DIRECTORY), "Documents path");
        }
    }

    {   /* delete the folder through the API */
        void *root_item = NULL;
        CALL(root, 0, HRESULT, &IID_IStorageItem_, &root_item);
        CALL(root_item, 8, HRESULT, &action);
        hr = done(action, NULL);
        CHECK(hr == S_OK && GetFileAttributesW(dir) == INVALID_FILE_ATTRIBUTES, "delete folder %lx", hr);
    }
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
