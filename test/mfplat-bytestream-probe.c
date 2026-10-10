/* mfplat's byte stream and property store stubs (patches/sg/2831), run by
 * test/mfplat-bytestream-gate.sh: a file byte stream's SetLength, Flush,
 * Close and IMFGetService, MFCreateMFByteStreamOnStreamEx and the property
 * store's Commit (left E_NOTIMPL: the conformance test says so).
 *
 *   mfplat-bytestream-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <propsys.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static DWORD disk_size(const WCHAR *path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &data)) return 0xffffffff;
    return data.nFileSizeLow;
}

static void test_file(void)
{
    WCHAR path[MAX_PATH];
    IMFByteStream *stream;
    IMFGetService *gs;
    ULONG written, read;
    QWORD length;
    BYTE buf[32];
    HRESULT hr;

    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"sg-mfplat-bytestream.bin");
    hr = MFCreateFile(MF_ACCESSMODE_READWRITE, MF_OPENMODE_DELETE_IF_EXIST, MF_FILEFLAGS_NONE, path, &stream);
    CHECKF(hr == S_OK, "MFCreateFile (%#lx)", hr);
    if (hr != S_OK) return;

    hr = IMFByteStream_Write(stream, (const BYTE *)"0123456789abcdefghij", 20, &written);
    CHECKF(hr == S_OK && written == 20, "Write 20 bytes (%#lx)", hr);
    hr = IMFByteStream_Flush(stream);
    CHECKF(hr == S_OK, "Flush on a writable stream (%#lx)", hr);
    CHECKF(disk_size(path) == 20, "the flushed bytes are in the file (%lu)", disk_size(path));

    hr = IMFByteStream_SetLength(stream, 5);
    CHECKF(hr == S_OK, "SetLength(5) (%#lx)", hr);
    hr = IMFByteStream_GetLength(stream, &length);
    CHECKF(hr == S_OK && length == 5, "GetLength is 5 (%#lx, %I64u)", hr, length);
    CHECKF(disk_size(path) == 5, "the file on disk was cut to 5 (%lu)", disk_size(path));
    IMFByteStream_SetCurrentPosition(stream, 0);
    memset(buf, 0, sizeof(buf));
    hr = IMFByteStream_Read(stream, buf, sizeof(buf), &read);
    CHECKF(hr == S_OK && read == 5 && !memcmp(buf, "01234", 5), "reading gives the first 5 bytes (%lu)", read);

    hr = IMFByteStream_SetLength(stream, 100);
    CHECKF(hr == S_OK, "SetLength(100) extends (%#lx)", hr);
    IMFByteStream_GetLength(stream, &length);
    CHECKF(length == 100 && disk_size(path) == 100, "length and file are 100 (%I64u, %lu)", length, disk_size(path));
    IMFByteStream_SetCurrentPosition(stream, 5);
    IMFByteStream_Read(stream, buf, 4, &read);
    CHECKF(read == 4 && !buf[0] && !buf[3], "the extension reads as zeros");

    hr = IMFByteStream_QueryInterface(stream, &IID_IMFGetService, (void **)&gs);
    if (hr == S_OK)
    {
        void *obj = (void *)1;
        hr = IMFGetService_GetService(gs, &IID_IUnknown, &IID_IUnknown, &obj);
        CHECKF(hr == MF_E_UNSUPPORTED_SERVICE, "GetService of an unknown service is MF_E_UNSUPPORTED_SERVICE (%#lx)", hr);
        IMFGetService_Release(gs);
    }
    else check(1, "(no IMFGetService on this stream: skipped)");

    hr = IMFByteStream_Close(stream);
    CHECKF(hr == S_OK, "Close (%#lx)", hr);
    hr = IMFByteStream_Write(stream, (const BYTE *)"x", 1, &written);
    CHECKF(FAILED(hr), "Write after Close fails (%#lx)", hr);
    hr = IMFByteStream_GetLength(stream, &length);
    CHECKF(FAILED(hr), "GetLength after Close fails (%#lx)", hr);
    hr = IMFByteStream_Close(stream);
    CHECKF(hr == S_OK, "a second Close is harmless (%#lx)", hr);
    /* the handle is really released: the file can be deleted while the stream object lives */
    CHECKF(DeleteFileW(path), "the file can be deleted once the stream is closed (%lu)", GetLastError());
    IMFByteStream_Release(stream);

    /* a read-only stream cannot be resized, and has nothing to flush */
    {
        HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        DWORD n;
        WriteFile(h, "abcdef", 6, &n, NULL);
        CloseHandle(h);
    }
    hr = MFCreateFile(MF_ACCESSMODE_READ, MF_OPENMODE_FAIL_IF_NOT_EXIST, MF_FILEFLAGS_NONE, path, &stream);
    CHECKF(hr == S_OK, "MFCreateFile read-only (%#lx)", hr);
    if (hr == S_OK)
    {
        hr = IMFByteStream_SetLength(stream, 2);
        CHECKF(FAILED(hr), "SetLength on a read-only stream fails (%#lx)", hr);
        CHECKF(disk_size(path) == 6, "the read-only file is untouched (%lu)", disk_size(path));
        hr = IMFByteStream_Flush(stream);
        CHECKF(hr == S_OK, "Flush on a read-only stream is S_OK (%#lx)", hr);
        IMFByteStream_Release(stream);
    }
    DeleteFileW(path);
}

static void test_stream_ex(void)
{
    HRESULT (WINAPI *pEx)(IUnknown *, IMFByteStream **) = (void *)GetProcAddress(GetModuleHandleW(L"mfplat.dll"), "MFCreateMFByteStreamOnStreamEx");
    IMFByteStream *bs;
    IMFAttributes *attrs;
    IStream *stream;
    ULONG written, read;
    BYTE buf[8] = {0};
    QWORD length;
    HRESULT hr;

    check(pEx != NULL, "MFCreateMFByteStreamOnStreamEx is exported");
    if (!pEx) return;
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    hr = pEx((IUnknown *)stream, &bs);
    CHECKF(hr == S_OK, "MFCreateMFByteStreamOnStreamEx(IStream) (%#lx)", hr);
    if (hr == S_OK)
    {
        hr = IMFByteStream_Write(bs, (const BYTE *)"wxyz", 4, &written);
        CHECKF(hr == S_OK && written == 4, "Write through the wrapper (%#lx)", hr);
        IMFByteStream_SetCurrentPosition(bs, 0);
        IMFByteStream_Read(bs, buf, 4, &read);
        CHECKF(read == 4 && !memcmp(buf, "wxyz", 4), "Read back the bytes");
        hr = IMFByteStream_GetLength(bs, &length);
        CHECKF(hr == S_OK && length == 4, "GetLength is 4 (%I64u)", length);
        IMFByteStream_Release(bs);
    }
    IStream_Release(stream);

    MFCreateAttributes(&attrs, 1);
    bs = (IMFByteStream *)1;
    hr = pEx((IUnknown *)attrs, &bs);
    CHECKF(hr == E_INVALIDARG, "an object that is not a stream is E_INVALIDARG (%#lx)", hr);
    IMFAttributes_Release(attrs);
    hr = pEx(NULL, &bs);
    CHECKF(hr == E_INVALIDARG, "NULL stream is E_INVALIDARG (%#lx)", hr);
}

static void test_property_store(void)
{
    HRESULT (WINAPI *pCreate)(IPropertyStore **) = (void *)GetProcAddress(GetModuleHandleW(L"mfplat.dll"), "CreatePropertyStore");
    IPropertyStore *store;
    PROPERTYKEY key = {{0x12345678, 0x1234, 0x1234, {1, 2, 3, 4, 5, 6, 7, 8}}, 5};
    PROPVARIANT value, got;
    HRESULT hr;

    check(pCreate != NULL, "CreatePropertyStore is exported");
    if (!pCreate) return;
    hr = pCreate(&store);
    CHECKF(hr == S_OK, "CreatePropertyStore (%#lx)", hr);
    if (hr != S_OK) return;
    value.vt = VT_I4;
    value.lVal = 42;
    hr = IPropertyStore_SetValue(store, &key, &value);
    CHECKF(hr == S_OK, "SetValue (%#lx)", hr);
    hr = IPropertyStore_Commit(store);
    CHECKF(hr == E_NOTIMPL, "Commit is E_NOTIMPL, as in dlls/mfplat/tests (%#lx)", hr);
    PropVariantInit(&got);
    hr = IPropertyStore_GetValue(store, &key, &got);
    CHECKF(hr == S_OK && got.vt == VT_I4 && got.lVal == 42, "the value is still there after Commit (%#lx)", hr);
    IPropertyStore_Release(store);
}

int main(void)
{
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    check(hr == S_OK, "MFStartup");
    test_file();
    test_stream_ex();
    test_property_store();
    MFShutdown();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
