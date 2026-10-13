/* windows.storage: FileIO and PathIO through the raw vtables */
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

#define G(name, a, b, c, d0,d1,d2,d3,d4,d5,d6,d7) static const GUID name = {a, b, c, {d0,d1,d2,d3,d4,d5,d6,d7}}
G(IID_IPathIOStatics_, 0x0f2f3758, 0x8ec7, 0x4381, 0x92,0x2b,0x8f,0x6c,0x07,0xd2,0x88,0xf3);
G(IID_IFileIOStatics_, 0x887411eb, 0x7f54, 0x4732, 0xa5,0xf0,0x5e,0x43,0xe3,0xb8,0xc2,0xf5);
G(IID_IStorageItem_, 0x4207a996, 0xca2f, 0x42f7, 0xbd,0xe8,0x8b,0x10,0x45,0x7a,0x7f,0x30);
G(IID_IStorageFileStatics_, 0x5984c710, 0xdaf2, 0x43c8, 0x8b,0xb4,0xa4,0xd3,0xea,0xcf,0xd0,0x3f);
G(IID_IIterable_HSTRING_, 0xe2fcc7c1, 0x3bfc, 0x5a0b, 0xb2,0xb0,0x72,0xe7,0x69,0xd1,0xcb,0x7e);
G(IID_IStreamedFileDataRequest_, 0x1673fcce, 0xdabd, 0x4d50, 0xbe,0xee,0x18,0x0b,0x8a,0x81,0x91,0xb6);
G(IID_IOutputStream_, 0x905a0fe6, 0xbc53, 0x11df, 0x8c,0x49,0x00,0x1e,0x4f,0xc6,0x86,0xda);
G(IID_IClosable_, 0x30d5a829, 0x7fa4, 0x4026, 0x83,0xbb,0xd7,0x5b,0xae,0x4e,0xa9,0x9e);
G(IID_IBufferFactory_, 0x71af914d, 0xc10f, 0x484b, 0xbc,0x50,0x14,0xbc,0x62,0x3b,0x3a,0x27);
G(IID_IBufferByteAccess_, 0x905a0fef, 0xbc53, 0x11df, 0x8c,0x49,0x00,0x1e,0x4f,0xc6,0x86,0xda);

static HS mk(const WCHAR *s) { HS h = NULL; create(s, wcslen(s), &h); return h; }
static const WCHAR *str(HS h) { return h ? raw(h, NULL) : L""; }

/* the result of a completed async object (GetResults is slot 8), releasing it */
static HRESULT done(void *op, void **out)
{
    HRESULT hr = ((HRESULT (WINAPI *)(void *, void **))VT(op)[8])(op, out);
    CALL(op, 2, ULONG);
    return hr;
}
static HRESULT done_action(void *op)
{
    HRESULT hr = ((HRESULT (WINAPI *)(void *))VT(op)[8])(op);
    CALL(op, 2, ULONG);
    return hr;
}
static HRESULT done_str(void *op, HS *out)
{
    HRESULT hr = ((HRESULT (WINAPI *)(void *, HS *))VT(op)[8])(op, out);
    CALL(op, 2, ULONG);
    return hr;
}

/* a StreamedFileDataRequestedHandler: mode 0 writes and closes, 1 writes and returns without closing,
 * 2 fails the request as unavailable, 3 writes from another thread and closes later */
struct handler { void *vtbl; LONG ref; int mode; RGAF get_factory; };
static HRESULT WINAPI h_qi(void *This, const GUID *iid, void **out) { *out = This; return S_OK; }
static ULONG WINAPI h_addref(void *This) { return InterlockedIncrement(&((struct handler *)This)->ref); }
static ULONG WINAPI h_release(void *This) { return InterlockedDecrement(&((struct handler *)This)->ref); }

static void write_text(void *request, const char *text)
{
    void *factory = NULL, *buffer = NULL, *access = NULL, *out = NULL, *op = NULL;
    BYTE *p = NULL;
    UINT32 len = strlen(text);

    CALL(request, 0, HRESULT, &IID_IOutputStream_, &out);
    if (!out) return;
    ((RGAF)GetProcAddress(GetModuleHandleA("combase.dll"), "RoGetActivationFactory"))(mk(L"Windows.Storage.Streams.Buffer"),
            &IID_IBufferFactory_, &factory);
    CALL(factory, 6, HRESULT, len, &buffer);
    CALL(buffer, 0, HRESULT, &IID_IBufferByteAccess_, &access);
    CALL(access, 3, HRESULT, &p);
    memcpy(p, text, len);
    CALL(buffer, 8, HRESULT, len);                      /* put_Length */
    CALL(out, 6, HRESULT, buffer, &op);                  /* WriteAsync */
    CALL(out, 2, ULONG);
    CALL(access, 2, ULONG);
}

static DWORD WINAPI late_close(void *request)
{
    void *closable = NULL;
    Sleep(300);
    write_text(request, "late data");
    CALL(request, 0, HRESULT, &IID_IClosable_, &closable);
    CALL(closable, 6, HRESULT);                          /* Close */
    CALL(closable, 2, ULONG);
    CALL(request, 2, ULONG);
    return 0;
}

static HRESULT WINAPI h_invoke(void *This, void *request)
{
    struct handler *h = This;
    void *closable = NULL;

    if (h->mode == 2)
        return CALL(request, 6, HRESULT, 1 /* CurrentlyUnavailable */);
    if (h->mode == 3)
    {
        CALL(request, 1, ULONG);
        CloseHandle(CreateThread(NULL, 0, late_close, request, 0, NULL));
        return S_OK;
    }
    write_text(request, "streamed data");
    if (h->mode == 0)
    {
        CALL(request, 0, HRESULT, &IID_IClosable_, &closable);
        CALL(closable, 6, HRESULT);
        CALL(closable, 2, ULONG);
    }
    return S_OK;
}
static void *handler_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

static BYTE *slurp(const WCHAR *path, DWORD *size)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    BYTE *data = NULL;
    *size = 0;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    *size = GetFileSize(h, NULL);
    data = malloc(*size + 1);
    ReadFile(h, data, *size, size, NULL);
    CloseHandle(h);
    return data;
}

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    RGAF get_factory = (RGAF)GetProcAddress(combase, "RoGetActivationFactory");
    HRESULT (WINAPI *ro_init)(int) = (void *)GetProcAddress(combase, "RoInitialize");
    void *pio = NULL, *fio = NULL, *file_statics = NULL, *op = NULL, *lines = NULL, *iter = NULL, *buf = NULL, *file = NULL;
    WCHAR dir[MAX_PATH], path[MAX_PATH], temp[MAX_PATH];
    HS s = NULL;
    HRESULT hr;
    UINT32 n = 0;
    DWORD size;
    BYTE *data;

    setvbuf(stdout, NULL, _IONBF, 0);
    create = (WCS)GetProcAddress(combase, "WindowsCreateString");
    raw = (WGSRB)GetProcAddress(combase, "WindowsGetStringRawBuffer");
    ro_init(1);

    hr = get_factory(mk(L"Windows.Storage.PathIO"), &IID_IPathIOStatics_, &pio);
    CHECK(hr == S_OK && pio, "PathIO factory %lx", hr);
    hr = get_factory(mk(L"Windows.Storage.FileIO"), &IID_IFileIOStatics_, &fio);
    CHECK(hr == S_OK && fio, "FileIO factory %lx", hr);
    hr = get_factory(mk(L"Windows.Storage.StorageFile"), &IID_IStorageFileStatics_, &file_statics);
    if (!pio || !fio || !file_statics) return 1;

    GetTempPathW(MAX_PATH, temp);
    swprintf(dir, MAX_PATH, L"%lsfastfio_%lu", temp, GetCurrentProcessId());
    CreateDirectoryW(dir, NULL);
    swprintf(path, MAX_PATH, L"%ls\\t.txt", dir);

    /* text: written as UTF-8 without a mark, read back, appended */
    hr = CALL(pio, 8, HRESULT, mk(path), mk(L"héllo"), &op);
    CHECK(hr == S_OK && done_action(op) == S_OK, "WriteText %lx", hr);
    data = slurp(path, &size);
    CHECK(size == 6 && data[0] == 'h' && data[1] == 0xc3 && data[2] == 0xa9, "UTF-8 bytes (size %lu)", size);
    free(data);
    hr = CALL(pio, 6, HRESULT, mk(path), &op);
    s = NULL;
    CHECK(hr == S_OK && done_str(op, &s) == S_OK && !wcscmp(str(s), L"héllo"), "ReadText %ls", str(s));
    hr = CALL(pio, 10, HRESULT, mk(path), mk(L"!"), &op);
    CHECK(hr == S_OK && done_action(op) == S_OK, "AppendText");
    hr = CALL(pio, 6, HRESULT, mk(path), &op);
    s = NULL; done_str(op, &s);
    CHECK(!wcscmp(str(s), L"héllo!"), "appended %ls", str(s));

    /* UTF-16 with its byte order mark is detected; an explicit encoding writes no mark */
    hr = CALL(pio, 9, HRESULT, mk(path), mk(L"ab"), 1 /* Utf16LE */, &op);
    CHECK(hr == S_OK && done_action(op) == S_OK, "WriteTextWithEncoding");
    data = slurp(path, &size);
    CHECK(size == 4 && data[0] == 'a' && data[1] == 0 && data[2] == 'b', "UTF-16LE bytes (size %lu)", size);
    free(data);
    {
        HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        BYTE bom[] = {0xff, 0xfe, 'x', 0, 'y', 0};
        DWORD w;
        WriteFile(h, bom, sizeof(bom), &w, NULL);
        CloseHandle(h);
    }
    hr = CALL(pio, 6, HRESULT, mk(path), &op);
    s = NULL; done_str(op, &s);
    CHECK(!wcscmp(str(s), L"xy"), "BOM detection %ls", str(s));

    /* lines */
    hr = CALL(pio, 8, HRESULT, mk(path), mk(L"one\r\ntwo\nthree\r"), &op);
    done_action(op);
    hr = CALL(pio, 12, HRESULT, mk(path), &op);
    CHECK(hr == S_OK, "ReadLines %lx", hr);
    hr = done(op, &lines);
    CHECK(hr == S_OK && lines, "ReadLines result %lx", hr);
    if (lines)
    {
        CALL(lines, 7, HRESULT, &n);
        CHECK(n == 3, "line count %u", n);
        s = NULL; CALL(lines, 6, HRESULT, 1, &s);
        CHECK(!wcscmp(str(s), L"two"), "line 1 %ls", str(s));
        hr = CALL(lines, 0, HRESULT, &IID_IIterable_HSTRING_, &iter);
        CHECK(hr == S_OK && iter, "IIterable<String> %lx", hr);
        swprintf(path, MAX_PATH, L"%ls\\lines.txt", dir);
        hr = CALL(pio, 14, HRESULT, mk(path), iter, &op);
        CHECK(hr == S_OK && done_action(op) == S_OK, "WriteLines %lx", hr);
        hr = CALL(pio, 16, HRESULT, mk(path), iter, &op);
        CHECK(hr == S_OK && done_action(op) == S_OK, "AppendLines");
        data = slurp(path, &size);
        CHECK(size == 34 && !memcmp(data, "one\r\ntwo\r\nthree\r\none\r\n", 22), "lines file (size %lu)", size);
        free(data);
    }

    /* bytes and buffer */
    {
        BYTE bytes[5] = {1, 2, 3, 4, 5};
        BYTE *p = NULL;
        IUnknown *access = NULL;
        swprintf(path, MAX_PATH, L"%ls\\b.bin", dir);
        hr = CALL(pio, 20, HRESULT, mk(path), 5, bytes, &op);
        CHECK(hr == S_OK && done_action(op) == S_OK, "WriteBytes");
        hr = CALL(pio, 18, HRESULT, mk(path), &op);
        CHECK(hr == S_OK, "ReadBuffer");
        hr = done(op, &buf);
        CHECK(hr == S_OK && buf, "ReadBuffer result %lx", hr);
        if (buf)
        {
            UINT32 len = 0;
            CALL(buf, 7, HRESULT, &len);        /* IBuffer::get_Length */
            CHECK(len == 5, "buffer length %u", len);
            hr = CALL(buf, 0, HRESULT, &IID_IBufferByteAccess_, &access);
            if (hr == S_OK) { CALL(access, 3, HRESULT, &p); CHECK(p && !memcmp(p, bytes, 5), "buffer bytes"); }
            swprintf(path, MAX_PATH, L"%ls\\b2.bin", dir);
            hr = CALL(pio, 19, HRESULT, mk(path), buf, &op);
            CHECK(hr == S_OK && done_action(op) == S_OK, "WriteBuffer");
            data = slurp(path, &size);
            CHECK(size == 5 && !memcmp(data, bytes, 5), "copied buffer");
            free(data);
        }
    }

    /* errors: a relative path is refused at once, a missing file fails in the operation */
    hr = CALL(pio, 6, HRESULT, mk(L"relative.txt"), &op);
    CHECK(hr == E_INVALIDARG, "relative path %lx", hr);
    swprintf(path, MAX_PATH, L"%ls\\absent.txt", dir);
    hr = CALL(pio, 6, HRESULT, mk(path), &op);
    CHECK(hr == S_OK, "absent file call %lx", hr);
    if (hr == S_OK) { s = NULL; hr = done_str(op, &s); CHECK(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "absent file result %lx", hr); }

    /* FileIO takes a StorageFile */
    swprintf(path, MAX_PATH, L"%ls\\t.txt", dir);
    hr = CALL(file_statics, 6, HRESULT, mk(path), &op);
    hr = done(op, &file);
    CHECK(hr == S_OK && file, "GetFileFromPathAsync %lx", hr);
    if (file)
    {
        hr = CALL(fio, 8, HRESULT, file, mk(L"via file"), &op);
        CHECK(hr == S_OK && done_action(op) == S_OK, "FileIO WriteText");
        hr = CALL(fio, 6, HRESULT, file, &op);
        s = NULL; CHECK(hr == S_OK && done_str(op, &s) == S_OK && !wcscmp(str(s), L"via file"), "FileIO ReadText %ls", str(s));
        hr = CALL(fio, 6, HRESULT, NULL, &op);
        CHECK(hr == E_INVALIDARG, "FileIO null file %lx", hr);
    }

    /* streamed files: StorageFile statics slots 8 (create) and 9 (replace) */
    {
        struct handler h = { handler_vtbl, 1, 0 };
        void *f2 = NULL, *item = NULL;
        HS name = NULL;
        int mode;

        for (mode = 0; mode < 4; mode++)
        {
            const GUID *iid_item = &IID_IStorageItem_;
            h.mode = mode;
            hr = CALL(file_statics, 8, HRESULT, mk(L"streamed.txt"), &h, NULL, &op);
            CHECK(hr == S_OK, "CreateStreamedFileAsync mode %d: %lx", mode, hr);
            f2 = NULL;
            hr = done(op, &f2);
            if (mode == 2)
            {
                CHECK(hr == HRESULT_FROM_WIN32(ERROR_NOT_READY), "unavailable file result %lx", hr);
                continue;
            }
            CHECK(hr == S_OK && f2, "streamed file mode %d result %lx", mode, hr);
            if (!f2) continue;
            CALL(f2, 0, HRESULT, iid_item, &item);
            name = NULL; CALL(item, 11, HRESULT, &name);
            CHECK(!wcscmp(str(name), L"streamed.txt"), "streamed name %ls", str(name));
            name = NULL; CALL(item, 12, HRESULT, &name);
            data = slurp(str(name), &size);
            CHECK(data && size == strlen(mode == 3 ? "late data" : "streamed data") &&
                  !memcmp(data, mode == 3 ? "late data" : "streamed data", size), "streamed data mode %d (size %lu)", mode, size);
            free(data);
        }
        hr = CALL(file_statics, 8, HRESULT, mk(L"bad/name.txt"), &h, NULL, &op);
        CHECK(hr == E_INVALIDARG, "invalid streamed name %lx", hr);
        /* replace: the old data stays when the handler fails, changes when it succeeds */
        swprintf(path, MAX_PATH, L"%ls\\t.txt", dir);
        h.mode = 2;
        hr = CALL(file_statics, 9, HRESULT, file, &h, NULL, &op);
        CHECK(hr == S_OK, "ReplaceWithStreamedFileAsync call %lx", hr);
        hr = done(op, &f2);
        CHECK(hr == HRESULT_FROM_WIN32(ERROR_NOT_READY), "failed replace %lx", hr);
        data = slurp(path, &size);
        CHECK(size == 8 && !memcmp(data, "via file", 8), "old data kept (size %lu, err %lu)", size, GetLastError());
        free(data);
        h.mode = 0;
        hr = CALL(file_statics, 9, HRESULT, file, &h, NULL, &op);
        hr = done(op, &f2);
        CHECK(hr == S_OK, "replace %lx", hr);
        data = slurp(path, &size);
        CHECK(size == 13 && !memcmp(data, "streamed data", 13), "replaced data (size %lu)", size);
        free(data);
    }

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
