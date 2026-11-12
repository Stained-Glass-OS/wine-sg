/* Probe for patches/sg/7739: shcore CreateStreamOverRandomAccessStream (Plex: its media controls'
 * cover art; the stub crashed it). A COM IStream over a WinRT stream:
 *   1. over an InMemoryRandomAccessStream: write, seek, read back, Stat, SetSize, Clone
 *   2. over shcore's own wrapper of a COM stream: that very stream back */
#define COBJMACROS
#include <windows.h>
#include <objidl.h>
#include <shlwapi.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>
#include <string.h>

static int fails;
static void check(const char *name, int ok) { printf("      %s  %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }
static const IID IID_IRandomAccessStream_ = { 0x905a0fe1, 0xbc53, 0x11df, { 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda } };

int main(void)
{
    HRESULT (WINAPI *over_ras)(IUnknown *, REFIID, void **);
    HRESULT (WINAPI *over_stream)(IStream *, DWORD, REFIID, void **);
    HMODULE shcore = LoadLibraryA("shcore.dll");
    static const WCHAR cls[] = L"Windows.Storage.Streams.InMemoryRandomAccessStream";
    IInspectable *mem = NULL;
    IStream *s = NULL, *c = NULL, *com, *back = NULL;
    IUnknown *ras = NULL;
    HSTRING name;
    char buf[32] = {0};
    ULONG n = 0;
    LARGE_INTEGER zero = {{0}};
    ULARGE_INTEGER pos, size5;
    STATSTG st;
    HRESULT hr;

    over_ras = (void *)GetProcAddress(shcore, "CreateStreamOverRandomAccessStream");
    over_stream = (void *)GetProcAddress(shcore, "CreateRandomAccessStreamOverStream");
    RoInitialize(RO_INIT_MULTITHREADED);
    WindowsCreateString(cls, (UINT32)wcslen(cls), &name);
    hr = RoActivateInstance(name, &mem);
    check("an InMemoryRandomAccessStream", SUCCEEDED(hr) && mem);
    if (!mem || !over_ras) { puts("RESULT: FAIL"); return 1; }

    hr = over_ras((IUnknown *)mem, &IID_IStream, (void **)&s);
    check("CreateStreamOverRandomAccessStream gives an IStream", hr == S_OK && s);
    if (!s) { puts("RESULT: FAIL"); return 1; }
    hr = IStream_Write(s, "cover art bytes", 15, &n);
    check("Write: 15 bytes", hr == S_OK && n == 15);
    hr = IStream_Seek(s, zero, STREAM_SEEK_CUR, &pos);
    check("the position follows (15)", hr == S_OK && pos.QuadPart == 15);
    IStream_Seek(s, zero, STREAM_SEEK_SET, NULL);
    hr = IStream_Read(s, buf, sizeof(buf), &n);
    check("Read from the start: the bytes, S_FALSE at the end", hr == S_FALSE && n == 15 && !memcmp(buf, "cover art bytes", 15));
    hr = IStream_Stat(s, &st, STATFLAG_NONAME);
    check("Stat: 15 bytes, a stream", hr == S_OK && st.cbSize.QuadPart == 15 && st.type == STGTY_STREAM);
    size5.QuadPart = 5;
    hr = IStream_SetSize(s, size5);
    IStream_Stat(s, &st, STATFLAG_NONAME);
    check("SetSize 5", hr == S_OK && st.cbSize.QuadPart == 5);
    IStream_Seek(s, zero, STREAM_SEEK_SET, NULL);
    hr = IStream_Clone(s, &c);
    memset(buf, 0, sizeof(buf));
    if (c) IStream_Read(c, buf, sizeof(buf), &n);
    check("Clone reads the same bytes", hr == S_OK && c && n == 5 && !memcmp(buf, "cover", 5));
    if (c) IStream_Release(c);
    IStream_Release(s);

    com = SHCreateMemStream(NULL, 0);
    hr = over_stream(com, 0, &IID_IRandomAccessStream_, (void **)&ras);
    if (SUCCEEDED(hr)) hr = over_ras(ras, &IID_IStream, (void **)&back);
    check("over shcore's own wrapper of a COM stream: that stream", hr == S_OK && back == com);
    if (back) IStream_Release(back);
    if (ras) IUnknown_Release(ras);
    IStream_Release(com);
    IInspectable_Release(mem);
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
