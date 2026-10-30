/* datareader-probe: Windows.Storage.Streams.DataReader and file: URLs for StorageFile (patches/sg/6205).
 *
 * React Native for Windows (OneDrive) opens its script bundle with
 * StorageFile.GetFileFromPathAsync("file:///C:\...") and reads it with DataReader.FromBuffer: the
 * URL was refused (E_INVALIDARG) and DataReader did not exist. Checks both URL forms, a reader made
 * from a buffer (byte order, UTF-8 strings, bounds) and one loading from a stream.
 * Interface identity per Wine's windows.storage IDL and windows-rs 0.30.0 (MIT).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>

DEFINE_GUID(IID_StorageFileStatics, 0x5984c710, 0xdaf2, 0x43c8, 0x8b, 0xb4, 0xa4, 0xd3, 0xea, 0xcf, 0xd0, 0x3f);
DEFINE_GUID(IID_FileIOStatics, 0x887411eb, 0x7f54, 0x4732, 0xa5, 0xf0, 0x5e, 0x43, 0xe3, 0xb8, 0xc2, 0xf5);
DEFINE_GUID(IID_StorageFile, 0xfa3f6186, 0x4214, 0x428c, 0xa6, 0x4c, 0x14, 0xc9, 0xac, 0x73, 0x15, 0xea);
DEFINE_GUID(IID_DataReaderStatics, 0x11fcbfc8, 0xf93a, 0x471b, 0xb1, 0x21, 0xf3, 0x79, 0xe3, 0x49, 0x31, 0x3c);
DEFINE_GUID(IID_DataReaderFactory, 0xd7527847, 0x57da, 0x4e15, 0x91, 0x4c, 0x06, 0x80, 0x66, 0x99, 0xa0, 0x98);
DEFINE_GUID(IID_InputStream, 0x905a0fe2, 0xbc53, 0x11df, 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda);
DEFINE_GUID(IID_AsyncInfo, 0x00000036, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
#define CALL(o, n, type) ((type)SLOT(o, n))
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef ULONG (WINAPI *rel_fn)(obj *);

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }
static void release(obj *o) { if (o) ((rel_fn)SLOT(o, 2))(o); }

/* the result of an async operation once complete (GetResults at slot), or NULL */
static obj *await(obj *op, int slot)
{
    obj *info = as(op, &IID_AsyncInfo), *result = NULL;
    int status = 0, i;

    if (!info) return NULL;
    for (i = 0; i < 500; i++)
    {
        CALL(info, 7, HRESULT (WINAPI *)(obj *, int *))(info, &status);   /* IAsyncInfo.Status */
        if (status) break;
        Sleep(10);
    }
    release(info);
    if (status == 1) CALL(op, slot, HRESULT (WINAPI *)(obj *, obj **))(op, &result);
    return result;
}

static obj *file_from(obj *statics, const WCHAR *path, HRESULT *hr)
{
    obj *op = NULL, *file;
    *hr = CALL(statics, 6, HRESULT (WINAPI *)(obj *, HSTRING, obj **))(statics, hs(path), &op);
    if (!op) return NULL;
    file = await(op, 8);
    release(op);
    return file;
}

int main(void)
{
    static const BYTE data[] = { 0x01, 0x02, 0x03, 0x04, 'h', 0xc3, 0xa9, 0x78, 0x56, 0x34, 0x12 };
    obj *files, *fileio, *readers, *factory, *file, *op = NULL, *buffer, *reader = NULL, *stream, *input;
    UINT32 left = 0, u = 0, loaded = 0;
    HSTRING str = NULL;
    HANDLE h;
    DWORD written;
    BYTE byte;
    HRESULT hr;

    RoInitialize(RO_INIT_MULTITHREADED);
    CreateDirectoryW(L"C:\\dr", NULL);
    h = CreateFileW(L"C:\\dr\\data.bin", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, data, sizeof(data), &written, NULL);
    CloseHandle(h);

    RoGetActivationFactory(hs(L"Windows.Storage.StorageFile"), &IID_StorageFileStatics, (void **)&files);
    RoGetActivationFactory(hs(L"Windows.Storage.FileIO"), &IID_FileIOStatics, (void **)&fileio);
    hr = RoGetActivationFactory(hs(L"Windows.Storage.Streams.DataReader"), &IID_DataReaderStatics, (void **)&readers);
    printf("reader_statics=%08lx\n", hr);
    if (!files || !fileio || !readers) return 1;

    /* file: URLs name their path */
    file = file_from(files, L"file:///C:\\dr\\data.bin", &hr);
    printf("url_backslash=%08lx,%d\n", hr, file != NULL);
    release(file);
    file = file_from(files, L"file:///C:/dr/data.bin", &hr);
    printf("url_slash=%08lx,%d\n", hr, file != NULL);
    if (!file) return 1;

    /* a reader over the file's bytes */
    CALL(fileio, 18, HRESULT (WINAPI *)(obj *, obj *, obj **))(fileio, as(file, &IID_StorageFile), &op);   /* ReadBufferAsync */
    buffer = op ? await(op, 8) : NULL;
    hr = buffer ? CALL(readers, 6, HRESULT (WINAPI *)(obj *, obj *, obj **))(readers, buffer, &reader) : E_FAIL;
    printf("from_buffer=%08lx\n", hr);
    if (!reader) return 1;
    CALL(reader, 6, HRESULT (WINAPI *)(obj *, UINT32 *))(reader, &left);           /* UnconsumedBufferLength */
    CALL(reader, 22, HRESULT (WINAPI *)(obj *, UINT32 *))(reader, &u);             /* ReadUInt32 */
    printf("left=%u big_endian=%08x\n", left, u);
    CALL(reader, 26, HRESULT (WINAPI *)(obj *, UINT32, HSTRING *))(reader, 3, &str);   /* ReadString: 3 UTF-8 units */
    printf("string=%d\n", str && !lstrcmpW(WindowsGetStringRawBuffer(str, NULL), L"h\x00e9"));
    CALL(reader, 10, HRESULT (WINAPI *)(obj *, int))(reader, 0);                   /* ByteOrder = LittleEndian (0) */
    CALL(reader, 22, HRESULT (WINAPI *)(obj *, UINT32 *))(reader, &u);
    CALL(reader, 6, HRESULT (WINAPI *)(obj *, UINT32 *))(reader, &left);
    printf("little_endian=%08x left=%u\n", u, left);
    hr = CALL(reader, 13, HRESULT (WINAPI *)(obj *, BYTE *))(reader, &byte);       /* ReadByte past the end */
    printf("past_end=%08lx\n", hr);
    release(reader);

    /* a reader loading from the file's stream */
    op = NULL;
    CALL(file, 8, HRESULT (WINAPI *)(obj *, int, obj **))(file, 0, &op);          /* OpenAsync(Read) */
    stream = op ? await(op, 8) : NULL;
    input = as(stream, &IID_InputStream);
    RoGetActivationFactory(hs(L"Windows.Storage.Streams.DataReader"), &IID_DataReaderFactory, (void **)&factory);
    reader = NULL;
    if (factory && input) CALL(factory, 6, HRESULT (WINAPI *)(obj *, obj *, obj **))(factory, input, &reader);
    printf("from_stream=%d\n", reader != NULL);
    if (!reader) return 1;
    op = NULL;
    CALL(reader, 29, HRESULT (WINAPI *)(obj *, UINT32, obj **))(reader, 64, &op);  /* LoadAsync(64): the stream ends at 11 */
    if (op)
    {
        obj *info = as(op, &IID_AsyncInfo);
        int status = 0, i;
        for (i = 0; info && i < 500 && !status; i++) { CALL(info, 7, HRESULT (WINAPI *)(obj *, int *))(info, &status); if (!status) Sleep(10); }
        CALL(op, 8, HRESULT (WINAPI *)(obj *, UINT32 *))(op, &loaded);
        release(info);
    }
    CALL(reader, 22, HRESULT (WINAPI *)(obj *, UINT32 *))(reader, &u);
    printf("loaded=%u first=%08x\n", loaded, u);
    printf("done=1\n");
    return 0;
}
