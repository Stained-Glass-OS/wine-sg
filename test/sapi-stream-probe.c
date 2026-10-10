/* SAPI streams, audio formats and the resource manager (patch 2840), run by
 * test/sapi-stream-gate.sh on Xvfb. A native client: ISpStream/IStream and
 * ISpResourceManager through their vtables, ISpeechFileStream and the
 * ISpeechAudioFormat/ISpeechWaveFormatEx objects late-bound through IDispatch
 * (as a script would), file contents checked on disk.
 *
 *   sapi-stream-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <initguid.h>
#include <mmreg.h>
#include <sapi.h>
#include <stdio.h>
#include <wchar.h>

DEFINE_GUID(PR_SPDFID_Text, 0x7ceef9f9, 0x3d13, 0x11d2, 0x9e, 0xe7, 0x00, 0xc0, 0x4f, 0x79, 0x73, 0x96);
DEFINE_GUID(PR_SPDFID_WaveFormatEx, 0xc31adbae, 0x527f, 0x4ff5, 0xa2, 0x30, 0xf6, 0x2b, 0xb6, 0x1f, 0xf7, 0x0c);
DEFINE_GUID(IID_Bogus, 0x12345678, 0x1234, 0x1234, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34);
DEFINE_GUID(PR_SVC_A, 0x11111111, 0x1111, 0x1111, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11);
DEFINE_GUID(PR_SVC_B, 0x22222222, 0x2222, 0x2222, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22);

#define SPERR_UNINITIALIZED         ((HRESULT)0x80045001)
#define SPERR_ALREADY_INITIALIZED   ((HRESULT)0x80045002)
#define SPERR_UNSUPPORTED_FORMAT    ((HRESULT)0x80045003)
#define SPERR_FORMAT_NOT_SPECIFIED  ((HRESULT)0x8004500e)
#define SPERR_NOT_FOUND             ((HRESULT)0x8004503a)

/* ISpResourceManager (not in the MinGW headers' C form): its vtable by hand. */
typedef struct RM RM;
typedef struct RMVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(RM *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(RM *);
    ULONG (STDMETHODCALLTYPE *Release)(RM *);
    HRESULT (STDMETHODCALLTYPE *QueryService)(RM *, REFGUID, REFIID, void **);
    HRESULT (STDMETHODCALLTYPE *SetObject)(RM *, REFGUID, IUnknown *);
    HRESULT (STDMETHODCALLTYPE *GetObject)(RM *, REFGUID, REFCLSID, REFIID, BOOL, void **);
} RMVtbl;
struct RM { const RMVtbl *lpVtbl; };

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static WCHAR dir[MAX_PATH];
static const WCHAR *path(const WCHAR *name)
{
    static WCHAR bufs[4][MAX_PATH];
    static int n;
    WCHAR *b = bufs[n++ & 3];
    swprintf(b, MAX_PATH, L"%ls\\%ls", dir, name);
    return b;
}

static ULONG refs(IUnknown *u) { IUnknown_AddRef(u); return IUnknown_Release(u); }

static unsigned char *slurp(const WCHAR *name, DWORD *size)
{
    HANDLE h = CreateFileW(path(name), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    unsigned char *buf;
    *size = 0;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    *size = GetFileSize(h, NULL);
    buf = calloc(1, *size + 1);
    ReadFile(h, buf, *size, size, NULL);
    CloseHandle(h);
    return buf;
}
#define LE32(p, o) (*(DWORD *)((p) + (o)))
#define LE16(p, o) (*(WORD *)((p) + (o)))

static void check_wav(const WCHAR *name, const char *what, WORD tag, WORD ch, DWORD rate, WORD bits, DWORD data_size)
{
    DWORD size, fmt_len;
    unsigned char *p = slurp(name, &size);
    int ok;

    ok = p && size >= 44 && !memcmp(p, "RIFF", 4) && !memcmp(p + 8, "WAVEfmt ", 8);
    CHECKF(ok, "%s: file is RIFF/WAVE (size %lu)", what, size);
    if (!ok) { free(p); return; }
    fmt_len = LE32(p, 16);
    CHECKF(LE16(p, 20) == tag && LE16(p, 22) == ch && LE32(p, 24) == rate && LE16(p, 34) == bits,
           "%s: header format tag %u ch %u rate %lu bits %u", what, LE16(p, 20), LE16(p, 22), LE32(p, 24), LE16(p, 34));
    CHECKF(LE32(p, 28) == rate * ch * bits / 8 && LE16(p, 32) == ch * bits / 8, "%s: header byte rate and alignment", what);
    CHECKF(!memcmp(p + 20 + fmt_len, "data", 4), "%s: data chunk follows the fmt chunk", what);
    CHECKF(LE32(p, 24 + fmt_len) == data_size, "%s: data chunk size %lu (want %lu)", what, LE32(p, 24 + fmt_len), data_size);
    CHECKF(LE32(p, 4) == size - 8, "%s: RIFF size %lu (file is %lu)", what, LE32(p, 4), size);
    CHECKF(size == 28 + fmt_len + (fmt_len & 1) + data_size, "%s: file length %lu", what, size);
    free(p);
}

/* ---- ISpStream over a memory stream ------------------------------------ */
static void test_spstream_memory(void)
{
    static const WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0 };
    ISpStream *sp, *sp2;
    IStream *mem, *base, *clone, *dest;
    IUnknown *unk;
    ULARGE_INTEGER pos, size, rd, wr;
    LARGE_INTEGER mv;
    STATSTG st;
    WAVEFORMATEX *got;
    GUID fmt;
    char buf[32];
    ULONG n;
    HRESULT hr;
    void *out;
    int i;
    static const IID *ifaces[] = { &IID_IUnknown, &IID_ISequentialStream, &IID_IStream, &IID_ISpStreamFormat, &IID_ISpStream };

    hr = CoCreateInstance(&CLSID_SpStream, NULL, CLSCTX_INPROC_SERVER, &IID_ISpStream, (void **)&sp);
    CHECKF(hr == S_OK, "SpStream: CoCreateInstance (%#lx)", hr);
    if (FAILED(hr)) return;

    for (i = 0; i < 5; i++)
    {
        unk = NULL;
        hr = ISpStream_QueryInterface(sp, ifaces[i], (void **)&unk);
        CHECKF(hr == S_OK && unk, "SpStream: QueryInterface interface #%d (%#lx)", i, hr);
        if (unk) IUnknown_Release(unk);
    }
    out = (void *)1;
    hr = ISpStream_QueryInterface(sp, &IID_IDispatch, &out);
    CHECKF(hr == E_NOINTERFACE && !out, "SpStream: IDispatch is E_NOINTERFACE with a NULL pointer");
    out = (void *)1;
    hr = ISpStream_QueryInterface(sp, &IID_Bogus, &out);
    CHECKF(hr == E_NOINTERFACE && !out, "SpStream: unknown IID is E_NOINTERFACE with a NULL pointer");

    /* Before SetBaseStream everything says uninitialized. */
    mv.QuadPart = 0; size.QuadPart = 0;
    hr = ISpStream_Read(sp, buf, 4, &n);
    CHECKF(hr == SPERR_UNINITIALIZED, "SpStream uninit: Read (%#lx)", hr);
    hr = ISpStream_Write(sp, buf, 4, &n);
    CHECKF(hr == SPERR_UNINITIALIZED, "SpStream uninit: Write (%#lx)", hr);
    hr = ISpStream_Seek(sp, mv, STREAM_SEEK_SET, &pos);
    CHECKF(hr == SPERR_UNINITIALIZED, "SpStream uninit: Seek (%#lx)", hr);
    hr = ISpStream_SetSize(sp, size);
    CHECKF(hr == SPERR_UNINITIALIZED, "SpStream uninit: SetSize (%#lx)", hr);
    hr = ISpStream_Commit(sp, 0);
    CHECKF(hr == SPERR_UNINITIALIZED, "SpStream uninit: Commit (%#lx)", hr);
    hr = ISpStream_Stat(sp, &st, 0);
    CHECKF(hr == SPERR_UNINITIALIZED, "SpStream uninit: Stat (%#lx)", hr);
    hr = ISpStream_Clone(sp, &clone);
    CHECKF(hr == SPERR_UNINITIALIZED, "SpStream uninit: Clone (%#lx)", hr);
    hr = ISpStream_GetFormat(sp, &fmt, &got);
    CHECKF(hr == SPERR_UNINITIALIZED, "SpStream uninit: GetFormat (%#lx)", hr);
    hr = ISpStream_GetBaseStream(sp, &base);
    CHECKF(hr == SPERR_UNINITIALIZED, "SpStream uninit: GetBaseStream (%#lx)", hr);
    hr = ISpStream_Close(sp);
    CHECKF(hr == SPERR_UNINITIALIZED, "SpStream uninit: Close (%#lx)", hr);

    CreateStreamOnHGlobal(NULL, TRUE, &mem);
    hr = ISpStream_SetBaseStream(sp, NULL, &PR_SPDFID_WaveFormatEx, &wfx);
    CHECKF(hr == E_INVALIDARG, "SetBaseStream: NULL stream is E_INVALIDARG (%#lx)", hr);
    hr = ISpStream_SetBaseStream(sp, mem, &PR_SPDFID_WaveFormatEx, NULL);
    CHECKF(hr == E_INVALIDARG, "SetBaseStream: wave format without a WAVEFORMATEX is E_INVALIDARG (%#lx)", hr);
    hr = ISpStream_SetBaseStream(sp, mem, &IID_Bogus, &wfx);
    CHECKF(hr == SPERR_UNSUPPORTED_FORMAT, "SetBaseStream: unknown format GUID (%#lx)", hr);
    hr = ISpStream_SetBaseStream(sp, mem, &PR_SPDFID_WaveFormatEx, &wfx);
    CHECKF(hr == S_OK, "SetBaseStream: wave format (%#lx)", hr);
    CHECKF(refs((IUnknown *)mem) == 2, "SetBaseStream: holds a reference on the stream (%lu)", refs((IUnknown *)mem));
    hr = ISpStream_SetBaseStream(sp, mem, &PR_SPDFID_WaveFormatEx, &wfx);
    CHECKF(hr == SPERR_ALREADY_INITIALIZED, "SetBaseStream: second call is already-initialized (%#lx)", hr);

    memset(&fmt, 0, sizeof(fmt)); got = NULL;
    hr = ISpStream_GetFormat(sp, &fmt, &got);
    CHECKF(hr == S_OK && IsEqualGUID(&fmt, &PR_SPDFID_WaveFormatEx) && got && got->nChannels == 2 &&
           got->nSamplesPerSec == 44100 && got->wBitsPerSample == 16 && got->nBlockAlign == 4,
           "GetFormat: wave format returned (%#lx)", hr);
    CoTaskMemFree(got);
    hr = ISpStream_GetFormat(sp, NULL, &got);
    CHECKF(hr == E_POINTER, "GetFormat: NULL GUID pointer is E_POINTER (%#lx)", hr);

    hr = ISpStream_GetBaseStream(sp, &base);
    CHECKF(hr == S_OK && base == mem, "GetBaseStream returns the stream given");
    if (hr == S_OK) IStream_Release(base);

    for (i = 0; i < 10; i++) buf[i] = 'A' + i;
    n = 0;
    hr = ISpStream_Write(sp, buf, 10, &n);
    CHECKF(hr == S_OK && n == 10, "Write: 10 bytes (%#lx, %lu)", hr, n);
    mv.QuadPart = 0;
    hr = ISpStream_Seek(sp, mv, STREAM_SEEK_SET, &pos);
    CHECKF(hr == S_OK && pos.QuadPart == 0, "Seek: to the start");
    memset(buf, 0, sizeof(buf)); n = 0;
    hr = ISpStream_Read(sp, buf, 4, &n);
    CHECKF(hr == S_OK && n == 4 && !memcmp(buf, "ABCD", 4), "Read: 4 bytes back");
    mv.QuadPart = 2;
    hr = ISpStream_Seek(sp, mv, STREAM_SEEK_CUR, &pos);
    CHECKF(hr == S_OK && pos.QuadPart == 6, "Seek: relative to the current position gives 6 (%llu)", (unsigned long long)pos.QuadPart);
    mv.QuadPart = -3;
    hr = ISpStream_Seek(sp, mv, STREAM_SEEK_END, &pos);
    CHECKF(hr == S_OK && pos.QuadPart == 7, "Seek: relative to the end gives 7");
    hr = ISpStream_Stat(sp, &st, STATFLAG_NONAME);
    CHECKF(hr == S_OK && st.cbSize.QuadPart == 10 && st.type == STGTY_STREAM, "Stat: size 10, stream type");

    hr = ISpStream_Clone(sp, &clone);
    CHECKF(hr == S_OK && clone, "Clone (%#lx)", hr);
    if (hr == S_OK)
    {
        hr = IStream_QueryInterface(clone, &IID_ISpStream, (void **)&sp2);
        CHECKF(hr == S_OK, "Clone: is an ISpStream with the same format");
        if (hr == S_OK)
        {
            got = NULL;
            hr = ISpStream_GetFormat(sp2, &fmt, &got);
            CHECKF(hr == S_OK && IsEqualGUID(&fmt, &PR_SPDFID_WaveFormatEx) && got && got->nSamplesPerSec == 44100,
                   "Clone: format copied (%#lx)", hr);
            CoTaskMemFree(got);
            ISpStream_Release(sp2);
        }
        IStream_Release(clone);
    }

    CreateStreamOnHGlobal(NULL, TRUE, &dest);
    mv.QuadPart = 0;
    ISpStream_Seek(sp, mv, STREAM_SEEK_SET, NULL);
    size.QuadPart = 6;
    hr = ISpStream_CopyTo(sp, dest, size, &rd, &wr);
    CHECKF(hr == S_OK && rd.QuadPart == 6 && wr.QuadPart == 6, "CopyTo: 6 bytes (%#lx)", hr);
    mv.QuadPart = 0;
    IStream_Seek(dest, mv, STREAM_SEEK_SET, NULL);
    memset(buf, 0, sizeof(buf));
    IStream_Read(dest, buf, 6, &n);
    CHECKF(n == 6 && !memcmp(buf, "ABCDEF", 6), "CopyTo: data arrived");
    IStream_Release(dest);

    size.QuadPart = 4;
    hr = ISpStream_SetSize(sp, size);
    CHECKF(hr == S_OK, "SetSize (%#lx)", hr);
    hr = ISpStream_Stat(sp, &st, STATFLAG_NONAME);
    CHECKF(hr == S_OK && st.cbSize.QuadPart == 4, "SetSize: Stat shows 4");
    hr = ISpStream_Commit(sp, STGC_DEFAULT);
    CHECKF(hr == S_OK, "Commit (%#lx)", hr);

    hr = ISpStream_Close(sp);
    CHECKF(hr == S_OK, "Close (%#lx)", hr);
    CHECKF(refs((IUnknown *)mem) == 1, "Close: drops the reference on the base stream (%lu)", refs((IUnknown *)mem));
    hr = ISpStream_Read(sp, buf, 1, &n);
    CHECKF(hr == SPERR_UNINITIALIZED, "after Close: Read is uninitialized (%#lx)", hr);
    hr = ISpStream_Close(sp);
    CHECKF(hr == SPERR_UNINITIALIZED, "after Close: Close again is uninitialized (%#lx)", hr);

    hr = ISpStream_SetBaseStream(sp, mem, &PR_SPDFID_Text, NULL);
    CHECKF(hr == S_OK, "SetBaseStream: text format after Close (%#lx)", hr);
    memset(&fmt, 0, sizeof(fmt)); got = (WAVEFORMATEX *)1;
    hr = ISpStream_GetFormat(sp, &fmt, &got);
    CHECKF(hr == S_OK && IsEqualGUID(&fmt, &PR_SPDFID_Text) && !got, "GetFormat: text format has no WAVEFORMATEX (%#lx)", hr);
    ISpStream_Release(sp);
    CHECKF(refs((IUnknown *)mem) == 1, "release of the stream object drops the base reference");
    IStream_Release(mem);
}

/* ---- ISpStream over files ---------------------------------------------- */
static void test_bindtofile(void)
{
    static const WAVEFORMATEX pcm16 = { WAVE_FORMAT_PCM, 1, 16000, 32000, 2, 16, 0 };
    static const WAVEFORMATEX mulaw = { WAVE_FORMAT_MULAW, 1, 8000, 8000, 1, 8, 0 };
    ISpStream *sp;
    IStream *base, *clone;
    unsigned char data[120], buf[200];
    WAVEFORMATEX *got;
    LARGE_INTEGER mv;
    ULARGE_INTEGER pos;
    STATSTG st;
    GUID fmt;
    ULONG n;
    HRESULT hr;
    HANDLE h;
    DWORD i, written;

    for (i = 0; i < sizeof(data); i++) data[i] = (unsigned char)(i * 7 + 1);

    CoCreateInstance(&CLSID_SpStream, NULL, CLSCTX_INPROC_SERVER, &IID_ISpStream, (void **)&sp);

    hr = ISpStream_BindToFile(sp, NULL, SPFM_CREATE, &PR_SPDFID_WaveFormatEx, &pcm16, 0);
    CHECKF(hr == E_INVALIDARG, "BindToFile: NULL file name is E_INVALIDARG (%#lx)", hr);
    hr = ISpStream_BindToFile(sp, path(L"x.wav"), (SPFILEMODE)99, &PR_SPDFID_WaveFormatEx, &pcm16, 0);
    CHECKF(hr == E_INVALIDARG, "BindToFile: bad mode is E_INVALIDARG (%#lx)", hr);
    hr = ISpStream_BindToFile(sp, path(L"x.wav"), SPFM_CREATE, NULL, NULL, 0);
    CHECKF(hr == SPERR_FORMAT_NOT_SPECIFIED, "BindToFile: creating without a format (%#lx)", hr);
    hr = ISpStream_BindToFile(sp, path(L"x.wav"), SPFM_CREATE, &PR_SPDFID_WaveFormatEx, NULL, 0);
    CHECKF(hr == E_INVALIDARG, "BindToFile: wave format without WAVEFORMATEX (%#lx)", hr);
    CHECKF(GetFileAttributesW(path(L"x.wav")) == INVALID_FILE_ATTRIBUTES, "BindToFile: failed calls create no file");
    hr = ISpStream_BindToFile(sp, path(L"missing.wav"), SPFM_OPEN_READONLY, NULL, NULL, 0);
    CHECKF(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "BindToFile: open of a missing file (%#lx)", hr);

    hr = ISpStream_BindToFile(sp, path(L"a.wav"), SPFM_CREATE, &PR_SPDFID_WaveFormatEx, &pcm16, 0);
    CHECKF(hr == S_OK, "BindToFile: create a wave file (%#lx)", hr);
    hr = ISpStream_BindToFile(sp, path(L"b.wav"), SPFM_CREATE, &PR_SPDFID_WaveFormatEx, &pcm16, 0);
    CHECKF(hr == SPERR_ALREADY_INITIALIZED, "BindToFile: second bind is already-initialized (%#lx)", hr);
    hr = ISpStream_Write(sp, data, 100, &n);
    CHECKF(hr == S_OK && n == 100, "wave file: write 100 bytes (%#lx)", hr);
    mv.QuadPart = 0;
    hr = ISpStream_Seek(sp, mv, STREAM_SEEK_CUR, &pos);
    CHECKF(hr == S_OK && pos.QuadPart == 100, "wave file: position counts data bytes only, not the header");
    hr = ISpStream_Stat(sp, &st, 0);
    CHECKF(hr == S_OK && st.cbSize.QuadPart == 100 && st.pwcsName && wcsstr(st.pwcsName, L"a.wav"), "wave file: Stat size 100 and name");
    if (hr == S_OK) CoTaskMemFree(st.pwcsName);
    hr = ISpStream_Close(sp);
    CHECKF(hr == S_OK, "wave file: Close (%#lx)", hr);
    check_wav(L"a.wav", "created wave file", WAVE_FORMAT_PCM, 1, 16000, 16, 100);

    hr = ISpStream_BindToFile(sp, path(L"a.wav"), SPFM_CREATE, &PR_SPDFID_WaveFormatEx, &pcm16, 0);
    CHECKF(hr == HRESULT_FROM_WIN32(ERROR_FILE_EXISTS), "BindToFile: SPFM_CREATE on an existing file (%#lx)", hr);

    /* Read-only open takes the format from the file. */
    hr = ISpStream_BindToFile(sp, path(L"a.wav"), SPFM_OPEN_READONLY, NULL, NULL, 0);
    CHECKF(hr == S_OK, "BindToFile: open read-only (%#lx)", hr);
    got = NULL;
    hr = ISpStream_GetFormat(sp, &fmt, &got);
    CHECKF(hr == S_OK && IsEqualGUID(&fmt, &PR_SPDFID_WaveFormatEx) && got && got->nSamplesPerSec == 16000 &&
           got->nChannels == 1 && got->wBitsPerSample == 16 && got->nAvgBytesPerSec == 32000,
           "open: format read from the file (%#lx)", hr);
    CoTaskMemFree(got);
    hr = ISpStream_Stat(sp, &st, STATFLAG_NONAME);
    CHECKF(hr == S_OK && st.cbSize.QuadPart == 100, "open: size is the data size, 100");
    memset(buf, 0, sizeof(buf));
    hr = ISpStream_Read(sp, buf, 150, &n);
    CHECKF(hr == S_OK && n == 100 && !memcmp(buf, data, 100), "open: Read returns exactly the data (%lu bytes)", n);
    hr = ISpStream_Read(sp, buf, 10, &n);
    CHECKF(hr == S_OK && n == 0, "open: Read at the end returns 0 bytes");
    hr = ISpStream_Write(sp, data, 4, &n);
    CHECKF(hr == STG_E_ACCESSDENIED, "open read-only: Write is denied (%#lx)", hr);
    mv.QuadPart = -10;
    hr = ISpStream_Seek(sp, mv, STREAM_SEEK_END, &pos);
    CHECKF(hr == S_OK && pos.QuadPart == 90, "open: Seek from the end");
    hr = ISpStream_Read(sp, buf, 10, &n);
    CHECKF(hr == S_OK && n == 10 && !memcmp(buf, data + 90, 10), "open: data at offset 90");
    mv.QuadPart = -1;
    hr = ISpStream_Seek(sp, mv, STREAM_SEEK_SET, &pos);
    CHECKF(hr == STG_E_INVALIDFUNCTION, "open: Seek before the start fails (%#lx)", hr);
    hr = ISpStream_GetBaseStream(sp, &base);
    CHECKF(hr == S_OK, "open: GetBaseStream");
    if (hr == S_OK)
    {
        mv.QuadPart = 5;
        IStream_Seek(base, mv, STREAM_SEEK_SET, NULL);
        hr = IStream_Clone(base, &clone);
        CHECKF(hr == S_OK, "open: file stream Clone");
        if (hr == S_OK)
        {
            memset(buf, 0, sizeof(buf));
            IStream_Read(clone, buf, 3, &n);
            CHECKF(n == 3 && !memcmp(buf, data + 5, 3), "open: Clone starts at the same position");
            IStream_Release(clone);
        }
        IStream_Release(base);
    }
    ISpStream_Close(sp);

    /* Read-write: append, header sizes follow. */
    hr = ISpStream_BindToFile(sp, path(L"a.wav"), SPFM_OPEN_READWRITE, NULL, NULL, 0);
    CHECKF(hr == S_OK, "BindToFile: open read-write (%#lx)", hr);
    mv.QuadPart = 0;
    ISpStream_Seek(sp, mv, STREAM_SEEK_END, &pos);
    hr = ISpStream_Write(sp, data + 100, 20, &n);
    CHECKF(hr == S_OK && n == 20, "read-write: append 20 bytes");
    ISpStream_Commit(sp, 0);
    check_wav(L"a.wav", "after Commit", WAVE_FORMAT_PCM, 1, 16000, 16, 120);
    mv.QuadPart = 0;
    ISpStream_Seek(sp, mv, STREAM_SEEK_SET, NULL);
    ISpStream_Read(sp, buf, 120, &n);
    CHECKF(n == 120 && !memcmp(buf, data, 120), "read-write: all 120 bytes read back");
    {
        ULARGE_INTEGER sz; sz.QuadPart = 30;
        hr = ISpStream_SetSize(sp, sz);
        CHECKF(hr == S_OK, "read-write: SetSize 30 (%#lx)", hr);
    }
    ISpStream_Close(sp);
    check_wav(L"a.wav", "after SetSize and Close", WAVE_FORMAT_PCM, 1, 16000, 16, 30);

    /* CREATE_ALWAYS truncates; a non-PCM format keeps its 18-byte fmt chunk. */
    hr = ISpStream_BindToFile(sp, path(L"a.wav"), SPFM_CREATE_ALWAYS, &PR_SPDFID_WaveFormatEx, &mulaw, 0);
    CHECKF(hr == S_OK, "BindToFile: SPFM_CREATE_ALWAYS over an existing file (%#lx)", hr);
    ISpStream_Write(sp, data, 7, &n);
    ISpStream_Close(sp);
    check_wav(L"a.wav", "mu-law file", WAVE_FORMAT_MULAW, 1, 8000, 8, 7);

    /* Raw text file: no header, the whole file is the data. */
    h = CreateFileW(path(L"t.txt"), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, "hello world", 11, &written, NULL);
    CloseHandle(h);
    hr = ISpStream_BindToFile(sp, path(L"t.txt"), SPFM_OPEN_READONLY, NULL, NULL, 0);
    CHECKF(hr == S_OK, "BindToFile: open a plain file (%#lx)", hr);
    got = (WAVEFORMATEX *)1;
    hr = ISpStream_GetFormat(sp, &fmt, &got);
    CHECKF(hr == S_OK && IsEqualGUID(&fmt, &PR_SPDFID_Text) && !got, "plain file: text format");
    memset(buf, 0, sizeof(buf));
    ISpStream_Read(sp, buf, 50, &n);
    CHECKF(n == 11 && !memcmp(buf, "hello world", 11), "plain file: whole file is the data");
    ISpStream_Close(sp);

    hr = ISpStream_BindToFile(sp, path(L"n.txt"), SPFM_CREATE, &PR_SPDFID_Text, NULL, 0);
    CHECKF(hr == S_OK, "BindToFile: create a text stream (%#lx)", hr);
    ISpStream_Write(sp, "abc", 3, &n);
    ISpStream_Close(sp);
    {
        DWORD size; unsigned char *p = slurp(L"n.txt", &size);
        CHECKF(size == 3 && p && !memcmp(p, "abc", 3), "text file is written without a wave header (%lu bytes)", size);
        free(p);
    }
    ISpStream_Release(sp);
}

/* ---- ISpResourceManager ------------------------------------------------ */
static void test_resource_manager(void)
{
    RM *rm, *rm2;
    IUnknown *unk, *obj, *obj2;
    IServiceProvider *sp;
    ISpStream *stream;
    HRESULT hr;
    void *out;

    hr = CoCreateInstance(&CLSID_SpResourceManager, NULL, CLSCTX_INPROC_SERVER, &IID_ISpResourceManager, (void **)&rm);
    CHECKF(hr == S_OK, "ResourceManager: create (%#lx)", hr);
    if (FAILED(hr)) return;
    hr = CoCreateInstance(&CLSID_SpResourceManager, NULL, CLSCTX_INPROC_SERVER, &IID_ISpResourceManager, (void **)&rm2);
    CHECKF(hr == S_OK && rm2 == rm, "ResourceManager: one manager per process (%p %p)", rm, rm2);
    rm2->lpVtbl->Release(rm2);
    hr = CoCreateInstance(&CLSID_SpResourceManager, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&unk);
    CHECKF(hr == S_OK && unk == (IUnknown *)rm, "ResourceManager: IUnknown is the same object");
    IUnknown_Release(unk);

    hr = rm->lpVtbl->QueryInterface(rm, &IID_IServiceProvider, (void **)&sp);
    CHECKF(hr == S_OK && sp == (IServiceProvider *)rm, "ResourceManager: QueryInterface(IServiceProvider) (%#lx)", hr);
    if (hr == S_OK) IServiceProvider_Release(sp);
    out = (void *)1;
    hr = rm->lpVtbl->QueryInterface(rm, &IID_IDispatch, &out);
    CHECKF(hr == E_NOINTERFACE && !out, "ResourceManager: IDispatch refused");

    out = (void *)1;
    hr = rm->lpVtbl->QueryService(rm, &PR_SVC_A, &IID_IUnknown, &out);
    CHECKF(hr == SPERR_NOT_FOUND && !out, "QueryService: unknown service is SPERR_NOT_FOUND (%#lx)", hr);
    hr = rm->lpVtbl->QueryService(rm, &PR_SVC_A, &IID_IUnknown, NULL);
    CHECKF(hr == E_POINTER, "QueryService: NULL output is E_POINTER (%#lx)", hr);

    CoCreateInstance(&CLSID_SpStream, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&obj);
    CHECKF(refs(obj) == 1, "test object starts with one reference");
    hr = rm->lpVtbl->SetObject(rm, &PR_SVC_A, obj);
    CHECKF(hr == S_OK, "SetObject (%#lx)", hr);
    CHECKF(refs(obj) == 2, "SetObject: manager holds a reference (%lu)", refs(obj));
    out = NULL;
    hr = rm->lpVtbl->QueryService(rm, &PR_SVC_A, &IID_ISpStream, &out);
    CHECKF(hr == S_OK && out, "QueryService: finds the object as ISpStream (%#lx)", hr);
    if (out) { CHECKF(refs(obj) == 3, "QueryService: adds a reference"); IUnknown_Release((IUnknown *)out); }
    out = (void *)1;
    hr = rm->lpVtbl->QueryService(rm, &PR_SVC_A, &IID_IDispatch, &out);
    CHECKF(hr == E_NOINTERFACE && !out, "QueryService: interface the object lacks (%#lx)", hr);
    hr = rm->lpVtbl->QueryService(rm, &PR_SVC_B, &IID_IUnknown, &out);
    CHECKF(hr == SPERR_NOT_FOUND, "QueryService: other service still not found");

    out = NULL;
    hr = rm->lpVtbl->GetObject(rm, &PR_SVC_A, &CLSID_SpFileStream, &IID_IUnknown, FALSE, &out);
    CHECKF(hr == S_OK && out == (void *)obj, "GetObject: existing service wins over the CLSID (%#lx)", hr);
    if (out) IUnknown_Release((IUnknown *)out);

    /* Replacing releases the old object. */
    CoCreateInstance(&CLSID_SpStream, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&obj2);
    rm->lpVtbl->SetObject(rm, &PR_SVC_A, obj2);
    CHECKF(refs(obj) == 1 && refs(obj2) == 2, "SetObject: replacing releases the old object (%lu, %lu)", refs(obj), refs(obj2));
    hr = rm->lpVtbl->SetObject(rm, &PR_SVC_A, NULL);
    CHECKF(hr == S_OK && refs(obj2) == 1, "SetObject(NULL): removes the service (%#lx)", hr);
    hr = rm->lpVtbl->QueryService(rm, &PR_SVC_A, &IID_IUnknown, &out);
    CHECKF(hr == SPERR_NOT_FOUND, "QueryService: removed service is gone");
    hr = rm->lpVtbl->SetObject(rm, &PR_SVC_B, NULL);
    CHECKF(hr == S_OK, "SetObject(NULL): removing an absent service is fine (%#lx)", hr);
    IUnknown_Release(obj);
    IUnknown_Release(obj2);

    /* GetObject creates what is missing and remembers it. */
    out = NULL;
    hr = rm->lpVtbl->GetObject(rm, &PR_SVC_B, &CLSID_SpStream, &IID_ISpStream, FALSE, &out);
    CHECKF(hr == S_OK && out, "GetObject: creates the object from its CLSID (%#lx)", hr);
    stream = out;
    out = NULL;
    hr = rm->lpVtbl->GetObject(rm, &PR_SVC_B, &CLSID_SpFileStream, &IID_ISpStream, FALSE, &out);
    CHECKF(hr == S_OK && out == (void *)stream, "GetObject: the second call returns the same object (%#lx)", hr);
    if (out) ISpStream_Release((ISpStream *)out);
    out = NULL;
    hr = rm->lpVtbl->QueryService(rm, &PR_SVC_B, &IID_ISpStream, &out);
    CHECKF(hr == S_OK && out == (void *)stream, "QueryService: sees the object GetObject created");
    if (out) ISpStream_Release((ISpStream *)out);
    ISpStream_Release(stream);

    out = (void *)1;
    hr = rm->lpVtbl->GetObject(rm, &PR_SVC_A, &IID_Bogus, &IID_IUnknown, FALSE, &out);
    CHECKF(hr == REGDB_E_CLASSNOTREG && !out, "GetObject: unregistered CLSID (%#lx)", hr);
    out = NULL;
    hr = rm->lpVtbl->QueryService(rm, &PR_SVC_A, &IID_IUnknown, &out);
    CHECKF(hr == SPERR_NOT_FOUND, "GetObject: a failed creation registers nothing");
    hr = rm->lpVtbl->GetObject(rm, &PR_SVC_A, &CLSID_SpStream, &IID_IUnknown, FALSE, NULL);
    CHECKF(hr == E_POINTER, "GetObject: NULL output is E_POINTER (%#lx)", hr);

    rm->lpVtbl->Release(rm);
}

/* ---- late-bound automation helpers ------------------------------------- */
static HRESULT dget(IDispatch *d, const WCHAR *name, VARIANT *res, VARIANT *args, int nargs, WORD flags)
{
    DISPID id, named = DISPID_PROPERTYPUT;
    DISPPARAMS dp;
    VARIANT rev[6];
    HRESULT hr;
    LPOLESTR n = (LPOLESTR)name;
    int i;

    hr = IDispatch_GetIDsOfNames(d, &IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id);
    if (FAILED(hr)) return hr;
    for (i = 0; i < nargs; i++) rev[i] = args[nargs - 1 - i];
    dp.rgvarg = rev;
    dp.cArgs = nargs;
    dp.rgdispidNamedArgs = (flags & (DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF)) ? &named : NULL;
    dp.cNamedArgs = dp.rgdispidNamedArgs ? 1 : 0;
    EXCEPINFO ex;
    UINT argerr = 0;
    if (res) VariantInit(res);
    memset(&ex, 0, sizeof(ex));
    hr = IDispatch_Invoke(d, id, &IID_NULL, LOCALE_USER_DEFAULT, flags, &dp, res, &ex, &argerr);
    if (hr == DISP_E_EXCEPTION) hr = ex.scode;
    return hr;
}
static HRESULT dcall(IDispatch *d, const WCHAR *name, VARIANT *res, VARIANT *args, int nargs)
{ return dget(d, name, res, args, nargs, DISPATCH_METHOD | DISPATCH_PROPERTYGET); }
static HRESULT dput(IDispatch *d, const WCHAR *name, VARIANT v)
{ return dget(d, name, NULL, &v, 1, DISPATCH_PROPERTYPUT); }
static HRESULT dputref(IDispatch *d, const WCHAR *name, IDispatch *obj)
{
    VARIANT v;
    V_VT(&v) = VT_DISPATCH; V_DISPATCH(&v) = obj;
    return dget(d, name, NULL, &v, 1, DISPATCH_PROPERTYPUTREF);
}
static VARIANT vi4(LONG l) { VARIANT v; V_VT(&v) = VT_I4; V_I4(&v) = l; return v; }
static VARIANT vbool(BOOL b) { VARIANT v; V_VT(&v) = VT_BOOL; V_BOOL(&v) = b ? VARIANT_TRUE : VARIANT_FALSE; return v; }
static VARIANT vbstr(const WCHAR *s) { VARIANT v; V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(s); return v; }
static VARIANT vbytes(const unsigned char *data, int n)
{
    VARIANT v;
    SAFEARRAY *a = SafeArrayCreateVector(VT_UI1, 0, n);
    void *p;
    SafeArrayAccessData(a, &p); memcpy(p, data, n); SafeArrayUnaccessData(a);
    V_VT(&v) = VT_ARRAY | VT_UI1; V_ARRAY(&v) = a;
    return v;
}
static LONG dlong(IDispatch *d, const WCHAR *name)
{
    VARIANT r;
    HRESULT hr = dcall(d, name, &r, NULL, 0);
    LONG l = -12345;
    if (hr == S_OK) { VariantChangeType(&r, &r, 0, VT_I4); l = V_I4(&r); }
    VariantClear(&r);
    return l;
}

static HRESULT dread(IDispatch *d, VARIANT *res, LONG n)
{
    VARIANT buf, args[2];
    HRESULT hr;
    VariantInit(&buf);
    V_VT(&args[0]) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&args[0]) = &buf;
    args[1] = vi4(n);
    hr = dcall(d, L"Read", res, args, 2);
    VariantClear(&buf);
    return hr;
}

static IDispatch *get_format(IDispatch *stream)
{
    VARIANT r;
    IDispatch *f = NULL;
    if (dcall(stream, L"Format", &r, NULL, 0) == S_OK && V_VT(&r) == VT_DISPATCH) { f = V_DISPATCH(&r); }
    return f;
}

/* ---- ISpeechFileStream, ISpeechAudioFormat, ISpeechWaveFormatEx -------- */
static void test_file_stream(void)
{
    static const unsigned char payload[8] = { 9, 8, 7, 6, 5, 4, 3, 2 };
    IDispatch *fs, *fs2, *fmt, *wave;
    ISpStream *sp;
    IUnknown *u1, *u2;
    ITypeInfo *ti;
    VARIANT r, args[3];
    UINT count;
    HRESULT hr;
    DISPID id;
    LPOLESTR name;
    int i;
    DWORD size;
    unsigned char *p;

    hr = CoCreateInstance(&CLSID_SpFileStream, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&fs);
    CHECKF(hr == S_OK, "SpFileStream: CoCreateInstance as IDispatch (%#lx)", hr);
    if (FAILED(hr)) return;

    count = 77;
    hr = IDispatch_GetTypeInfoCount(fs, &count);
    CHECKF(hr == S_OK && count == 1, "FileStream: GetTypeInfoCount is 1 (%#lx, %u)", hr, count);
    hr = IDispatch_GetTypeInfo(fs, 0, LOCALE_USER_DEFAULT, &ti);
    CHECKF(hr == S_OK && ti, "FileStream: GetTypeInfo(0) (%#lx)", hr);
    if (hr == S_OK)
    {
        TYPEATTR *ta;
        ITypeInfo_GetTypeAttr(ti, &ta);
        CHECKF(IsEqualGUID(&ta->guid, &IID_ISpeechFileStream), "FileStream: type info is ISpeechFileStream");
        ITypeInfo_ReleaseTypeAttr(ti, ta);
        ITypeInfo_Release(ti);
    }
    ti = (ITypeInfo *)1;
    hr = IDispatch_GetTypeInfo(fs, 1, LOCALE_USER_DEFAULT, &ti);
    CHECKF(hr == DISP_E_BADINDEX, "FileStream: GetTypeInfo(1) is DISP_E_BADINDEX (%#lx)", hr);
    {
        static const struct { const WCHAR *name; DISPID id; } ids[] =
            { { L"Format", 1 }, { L"Read", 2 }, { L"Write", 3 }, { L"Seek", 4 }, { L"Open", 100 }, { L"Close", 101 } };
        for (i = 0; i < (int)ARRAY_SIZE(ids); i++)
        {
            name = (LPOLESTR)ids[i].name; id = 0;
            hr = IDispatch_GetIDsOfNames(fs, &IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &id);
            CHECKF(hr == S_OK && id == ids[i].id, "FileStream: GetIDsOfNames(%ls) is %ld (%#lx, %ld)", ids[i].name, ids[i].id, hr, id);
        }
        name = (LPOLESTR)L"NoSuchMember";
        hr = IDispatch_GetIDsOfNames(fs, &IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &id);
        CHECKF(hr == DISP_E_UNKNOWNNAME, "FileStream: unknown member name (%#lx)", hr);
    }

    /* Interfaces, one identity. */
    IDispatch_QueryInterface(fs, &IID_IUnknown, (void **)&u1);
    hr = IDispatch_QueryInterface(fs, &IID_ISpStream, (void **)&sp);
    CHECKF(hr == S_OK, "FileStream: QueryInterface(ISpStream) (%#lx)", hr);
    if (hr == S_OK)
    {
        ISpStream_QueryInterface(sp, &IID_IUnknown, (void **)&u2);
        CHECKF(u1 == u2, "FileStream: both views share one IUnknown");
        IUnknown_Release(u2);
    }
    IUnknown_Release(u1);
    {
        void *out = (void *)1;
        hr = IDispatch_QueryInterface(fs, &IID_Bogus, &out);
        CHECKF(hr == E_NOINTERFACE && !out, "FileStream: unknown IID refused");
        hr = IDispatch_QueryInterface(fs, &IID_ISpeechBaseStream, &out);
        CHECKF(hr == S_OK, "FileStream: ISpeechBaseStream (%#lx)", hr);
        if (hr == S_OK) IUnknown_Release((IUnknown *)out);
        hr = IDispatch_QueryInterface(fs, &IID_IStream, &out);
        CHECKF(hr == S_OK, "FileStream: IStream (%#lx)", hr);
        if (hr == S_OK) IUnknown_Release((IUnknown *)out);
    }

    /* Not opened yet. */
    hr = dread(fs, &r, 4);
    CHECKF(hr == (HRESULT)SPERR_UNINITIALIZED, "FileStream before Open: Read is uninitialized (%#lx)", hr);
    hr = dcall(fs, L"Close", &r, NULL, 0);
    CHECKF(hr == (HRESULT)SPERR_UNINITIALIZED, "FileStream before Open: Close is uninitialized (%#lx)", hr);
    hr = dcall(fs, L"Format", &r, NULL, 0);
    CHECKF(hr == (HRESULT)SPERR_UNINITIALIZED, "FileStream before Open: Format is uninitialized (%#lx)", hr);
    args[0] = vbstr(path(L"nope.wav")); args[1] = vi4(0); args[2] = vbool(FALSE);
    hr = dcall(fs, L"Open", &r, args, 3);
    CHECKF(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "FileStream: Open of a missing file (%#lx)", hr);
    SysFreeString(V_BSTR(&args[0]));

    /* Create with the default format. */
    args[0] = vbstr(path(L"f1.wav")); args[1] = vi4(2); args[2] = vbool(FALSE);
    hr = dcall(fs, L"Open", &r, args, 3);
    CHECKF(hr == S_OK, "FileStream: Open(create) (%#lx)", hr);
    SysFreeString(V_BSTR(&args[0]));
    fmt = get_format(fs);
    CHECKF(fmt != NULL, "FileStream: Format returns an ISpeechAudioFormat");
    if (fmt)
    {
        CHECKF(dlong(fmt, L"Type") == 22, "AudioFormat: default Type is SAFT22kHz16BitMono (%ld)", dlong(fmt, L"Type"));
        hr = dcall(fmt, L"Guid", &r, NULL, 0);
        CHECKF(hr == S_OK && V_VT(&r) == VT_BSTR && !_wcsicmp(V_BSTR(&r), L"{C31ADBAE-527F-4FF5-A230-F62BB61FF70C}"),
               "AudioFormat: Guid is the wave format id (%#lx %ls)", hr, V_VT(&r) == VT_BSTR ? V_BSTR(&r) : L"?");
        VariantClear(&r);
        hr = dcall(fmt, L"GetWaveFormatEx", &r, NULL, 0);
        CHECKF(hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r), "AudioFormat: GetWaveFormatEx returns an object (%#lx)", hr);
        if (hr == S_OK && V_VT(&r) == VT_DISPATCH)
        {
            wave = V_DISPATCH(&r);
            CHECKF(dlong(wave, L"FormatTag") == 1 && dlong(wave, L"Channels") == 1 && dlong(wave, L"SamplesPerSec") == 22050 &&
                   dlong(wave, L"AvgBytesPerSec") == 44100 && dlong(wave, L"BlockAlign") == 2 && dlong(wave, L"BitsPerSample") == 16,
                   "WaveFormatEx: 22050 Hz 16 bit mono PCM");
            hr = dcall(wave, L"ExtraData", &r, NULL, 0);
            CHECKF(hr == S_OK && V_VT(&r) == (VT_ARRAY | VT_UI1) && SafeArrayGetDim(V_ARRAY(&r)) == 1, "WaveFormatEx: ExtraData is an empty byte array");
            VariantClear(&r);
            {
                HRESULT hr2;
                hr = dput(wave, L"SamplesPerSec", vi4(11025));
                hr2 = dput(wave, L"BlockAlign", vi4(2));
                CHECKF(hr == S_OK && hr2 == S_OK && dlong(wave, L"SamplesPerSec") == 11025, "WaveFormatEx: properties can be set");
            }
            IDispatch_Release(wave);
        }
        IDispatch_Release(fmt);
    }

    hr = dcall(fs, L"Write", &r, (args[0] = vbytes(payload, 8), args), 1);
    CHECKF(hr == S_OK && V_VT(&r) == VT_I4 && V_I4(&r) == 8, "FileStream: Write(byte array) reports 8 bytes (%#lx)", hr);
    VariantClear(&args[0]);
    hr = dcall(fs, L"Write", &r, (args[0] = vbstr(L"text"), args), 1);
    CHECKF(hr == DISP_E_TYPEMISMATCH, "FileStream: Write(string) is a type mismatch (%#lx)", hr);
    SysFreeString(V_BSTR(&args[0]));
    args[0] = vi4(0); args[1] = vi4(0);
    hr = dcall(fs, L"Seek", &r, args, 2);
    CHECKF(hr == S_OK && V_VT(&r) == VT_I4 && V_I4(&r) == 0, "FileStream: Seek(0, start) (%#lx)", hr);
    args[0] = vi4(-3); args[1] = vi4(2);
    hr = dcall(fs, L"Seek", &r, args, 2);
    CHECKF(hr == S_OK && V_I4(&r) == 5, "FileStream: Seek(-3, end) is 5 (%#lx, %ld)", hr, V_I4(&r));
    args[0] = vi4(2); args[1] = vi4(1);
    hr = dcall(fs, L"Seek", &r, args, 2);
    CHECKF(hr == S_OK && V_I4(&r) == 7, "FileStream: Seek(2, current) is 7 (%#lx, %ld)", hr, V_I4(&r));
    {
        VARIANT dbl; V_VT(&dbl) = VT_R8; V_R8(&dbl) = 1.0;
        args[0] = dbl; args[1] = vi4(0);
        hr = dcall(fs, L"Seek", &r, args, 2);
        CHECKF(hr == S_OK && V_I4(&r) == 1, "FileStream: Seek accepts a double position (%#lx)", hr);
    }
    args[0] = vi4(0); args[1] = vi4(5);
    hr = dcall(fs, L"Seek", &r, args, 2);
    CHECKF(hr == E_INVALIDARG, "FileStream: Seek with a bad origin is E_INVALIDARG (%#lx)", hr);
    hr = dread(fs, &r, 100);
    CHECKF(hr == S_OK && V_VT(&r) == VT_I4 && V_I4(&r) == 7, "FileStream: Read(100) from position 1 returns the 7 left (%#lx, %ld)", hr, V_I4(&r));

    /* The reading of Read(): array comes back through the out VARIANT; use the vtable. */
    {
        ISpeechBaseStream *bs;
        LONG got = -1;
        VARIANT buf;
        IDispatch_QueryInterface(fs, &IID_ISpeechBaseStream, (void **)&bs);
        args[0] = vi4(0); args[1] = vi4(0);
        dcall(fs, L"Seek", &r, args, 2);
        VariantInit(&buf);
        hr = ISpeechBaseStream_Read(bs, &buf, 100, &got);
        CHECKF(hr == S_OK && got == 8 && V_VT(&buf) == (VT_ARRAY | VT_UI1), "BaseStream::Read: 8 bytes in a byte array (%#lx, %ld)", hr, got);
        if (hr == S_OK && V_VT(&buf) == (VT_ARRAY | VT_UI1))
        {
            LONG ub = -1; void *data;
            SafeArrayGetUBound(V_ARRAY(&buf), 1, &ub);
            SafeArrayAccessData(V_ARRAY(&buf), &data);
            CHECKF(ub == 7 && !memcmp(data, payload, 8), "BaseStream::Read: array holds exactly the data written (ubound %ld)", ub);
            SafeArrayUnaccessData(V_ARRAY(&buf));
        }
        VariantClear(&buf);
        hr = ISpeechBaseStream_Read(bs, &buf, -1, &got);
        CHECKF(hr == E_INVALIDARG, "BaseStream::Read: negative count is E_INVALIDARG (%#lx)", hr);
        hr = ISpeechBaseStream_Read(bs, &buf, 1, NULL);
        CHECKF(hr == E_POINTER, "BaseStream::Read: NULL count pointer is E_POINTER (%#lx)", hr);
        ISpeechBaseStream_Release(bs);
    }
    hr = dcall(fs, L"Close", &r, NULL, 0);
    CHECKF(hr == S_OK, "FileStream: Close (%#lx)", hr);
    hr = dread(fs, &r, 4);
    CHECKF(hr == (HRESULT)SPERR_UNINITIALIZED, "FileStream after Close: Read is uninitialized (%#lx)", hr);
    check_wav(L"f1.wav", "FileStream default file", WAVE_FORMAT_PCM, 1, 22050, 16, 8);
    p = slurp(L"f1.wav", &size);
    CHECKF(p && size >= 52 && !memcmp(p + size - 8, payload, 8), "FileStream: payload is at the end of the file");
    free(p);

    /* Format set before Open (by reference) is used for a new file. */
    hr = CoCreateInstance(&CLSID_SpFileStream, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&fs2);
    args[0] = vbstr(path(L"f1.wav")); args[1] = vi4(0); args[2] = vbool(FALSE);
    hr = dcall(fs2, L"Open", &r, args, 3);
    CHECKF(hr == S_OK, "FileStream: Open(read) of the file just written (%#lx)", hr);
    SysFreeString(V_BSTR(&args[0]));
    fmt = get_format(fs2);
    CHECKF(fmt && dlong(fmt, L"Type") == 22, "FileStream: Format of an opened file reflects the header");
    if (fmt)
    {
        VARIANT t;
        hr = dread(fs2, &r, 3);
        CHECKF(hr == S_OK, "FileStream: Read from a file opened for reading (%#lx)", hr);
        hr = dcall(fs2, L"Write", &r, (args[0] = vbytes(payload, 2), args), 1);
        CHECKF(hr == STG_E_ACCESSDENIED, "FileStream: Write to a read-only file is STG_E_ACCESSDENIED (%#lx)", hr);
        VariantClear(&args[0]);
        hr = dputref(fs2, L"Format", fmt);
        CHECKF(hr == (HRESULT)SPERR_ALREADY_INITIALIZED, "FileStream: setting Format on an open stream (%#lx)", hr);

        /* Format.Type controls the format: 8 kHz 8 bit mono. */
        V_VT(&t) = VT_I4; V_I4(&t) = 4;
        hr = dput(fmt, L"Type", t);
        CHECKF(hr == S_OK && dlong(fmt, L"Type") == 4, "AudioFormat: Type = SAFT8kHz8BitMono (%#lx)", hr);
        hr = dcall(fmt, L"GetWaveFormatEx", &r, NULL, 0);
        if (hr == S_OK && V_VT(&r) == VT_DISPATCH)
        {
            wave = V_DISPATCH(&r);
            CHECKF(dlong(wave, L"SamplesPerSec") == 8000 && dlong(wave, L"BitsPerSample") == 8 && dlong(wave, L"Channels") == 1 &&
                   dlong(wave, L"AvgBytesPerSec") == 8000 && dlong(wave, L"BlockAlign") == 1, "AudioFormat: Type 4 is 8000 Hz 8 bit mono");
            IDispatch_Release(wave);
        }
        IDispatch_Release(fmt);
    }
    IDispatch_Release(fs2);

    /* Table: Type <-> wave format. */
    {
        static const struct { int type; WORD tag, ch, bits; DWORD rate; } types[] =
        {
            { 4, 1, 1, 8, 8000 }, { 7, 1, 2, 16, 8000 }, { 13, 1, 2, 8, 12000 }, { 19, 1, 2, 16, 16000 },
            { 21, 1, 2, 8, 22050 }, { 27, 1, 2, 16, 24000 }, { 30, 1, 1, 16, 32000 }, { 34, 1, 1, 16, 44100 },
            { 39, 1, 2, 16, 48000 }, { 41, 6, 1, 8, 8000 }, { 48, 6, 2, 8, 44100 }, { 49, 7, 1, 8, 8000 }, { 56, 7, 2, 8, 44100 },
        };
        IDispatch *f1, *f2;
        VARIANT rr;

        for (i = 0; i < (int)ARRAY_SIZE(types); i++)
        {
            /* get a fresh format object from a new stream bound to a wave file */
            CoCreateInstance(&CLSID_SpFileStream, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&fs2);
            args[0] = vbstr(path(L"f1.wav")); args[1] = vi4(0); args[2] = vbool(FALSE);
            dcall(fs2, L"Open", &rr, args, 3);
            SysFreeString(V_BSTR(&args[0]));
            f1 = get_format(fs2);
            if (!f1) { check(0, "no format object for type table"); IDispatch_Release(fs2); continue; }
            V_VT(&rr) = VT_I4; V_I4(&rr) = types[i].type;
            hr = dput(f1, L"Type", rr);
            hr = dcall(f1, L"GetWaveFormatEx", &rr, NULL, 0);
            wave = (hr == S_OK && V_VT(&rr) == VT_DISPATCH) ? V_DISPATCH(&rr) : NULL;
            CHECKF(wave && dlong(wave, L"FormatTag") == types[i].tag && dlong(wave, L"Channels") == types[i].ch &&
                   dlong(wave, L"BitsPerSample") == types[i].bits && dlong(wave, L"SamplesPerSec") == (LONG)types[i].rate &&
                   dlong(wave, L"BlockAlign") == types[i].ch * types[i].bits / 8 &&
                   dlong(wave, L"AvgBytesPerSec") == (LONG)(types[i].rate * types[i].ch * types[i].bits / 8),
                   "Type %d sets tag %u, %u ch, %u bit, %lu Hz", types[i].type, types[i].tag, types[i].ch, types[i].bits, types[i].rate);
            /* and back: SetWaveFormatEx on a second object, Type reads the number */
            f2 = get_format(fs2);
            if (wave && f2)
            {
                V_VT(&rr) = VT_I4; V_I4(&rr) = 0;
                dput(f2, L"Type", rr); /* no assigned format */
                hr = dget(f2, L"SetWaveFormatEx", NULL, (V_VT(&args[0]) = VT_DISPATCH, V_DISPATCH(&args[0]) = wave, args), 1, DISPATCH_METHOD);
                CHECKF(hr == S_OK && dlong(f2, L"Type") == types[i].type, "SetWaveFormatEx then Type reads back %d (%#lx, %ld)", types[i].type, hr, dlong(f2, L"Type"));
            }
            if (f2) IDispatch_Release(f2);
            if (wave) IDispatch_Release(wave);
            IDispatch_Release(f1);
            IDispatch_Release(fs2);
        }
    }

    /* Special Type values. */
    {
        IDispatch *f1;
        VARIANT t;
        CoCreateInstance(&CLSID_SpFileStream, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&fs2);
        args[0] = vbstr(path(L"f1.wav")); args[1] = vi4(0); args[2] = vbool(FALSE);
        dcall(fs2, L"Open", &r, args, 3);
        SysFreeString(V_BSTR(&args[0]));
        f1 = get_format(fs2);
        if (f1)
        {
            V_VT(&t) = VT_I4; V_I4(&t) = 1;
            hr = dput(f1, L"Type", t);
            CHECKF(hr == S_OK && dlong(f1, L"Type") == 1, "AudioFormat: Type = SAFTText (%#lx)", hr);
            hr = dcall(f1, L"Guid", &r, NULL, 0);
            CHECKF(hr == S_OK && V_VT(&r) == VT_BSTR && !_wcsicmp(V_BSTR(&r), L"{7CEEF9F9-3D13-11D2-9EE7-00C04F797396}"), "AudioFormat: Guid is the text format id");
            VariantClear(&r);
            hr = dcall(f1, L"GetWaveFormatEx", &r, NULL, 0);
            CHECKF(hr == S_OK && V_VT(&r) == VT_DISPATCH && !V_DISPATCH(&r), "AudioFormat: text format has no WaveFormatEx object (%#lx)", hr);
            V_VT(&t) = VT_I4; V_I4(&t) = 58;
            hr = dput(f1, L"Type", t);
            CHECKF(hr == (HRESULT)SPERR_UNSUPPORTED_FORMAT, "AudioFormat: Type = ADPCM is unsupported (%#lx)", hr);
            V_VT(&t) = VT_I4; V_I4(&t) = 3;
            hr = dput(f1, L"Type", t);
            CHECKF(hr == E_INVALIDARG, "AudioFormat: Type = SAFTNonStandardFormat is E_INVALIDARG (%#lx)", hr);
            hr = dput(f1, L"Guid", vbstr(L"not a guid"));
            CHECKF(hr == E_INVALIDARG, "AudioFormat: Guid with junk is E_INVALIDARG (%#lx)", hr);
            IDispatch_Release(f1);
        }
        IDispatch_Release(fs2);
    }

    /* Format set before Open decides the new file's format. */
    {
        IDispatch *f1, *f2;
        VARIANT t;
        CoCreateInstance(&CLSID_SpFileStream, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&fs2);
        args[0] = vbstr(path(L"f1.wav")); args[1] = vi4(0); args[2] = vbool(FALSE);
        dcall(fs2, L"Open", &r, args, 3);
        SysFreeString(V_BSTR(&args[0]));
        f1 = get_format(fs2);
        V_VT(&t) = VT_I4; V_I4(&t) = 17; /* 16 kHz 8 bit stereo */
        dput(f1, L"Type", t);
        dcall(fs2, L"Close", &r, NULL, 0);
        IDispatch_Release(fs2);

        CoCreateInstance(&CLSID_SpFileStream, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&fs2);
        hr = dputref(fs2, L"Format", f1);
        CHECKF(hr == S_OK, "FileStream: putref Format before Open (%#lx)", hr);
        f2 = get_format(fs2);
        CHECKF(f2 && dlong(f2, L"Type") == 17, "FileStream: Format reads back before Open (type %ld)", f2 ? dlong(f2, L"Type") : -1);
        if (f2) IDispatch_Release(f2);
        args[0] = vbstr(path(L"f2.wav")); args[1] = vi4(3); args[2] = vbool(FALSE);
        hr = dcall(fs2, L"Open", &r, args, 3);
        CHECKF(hr == S_OK, "FileStream: Open(create always) (%#lx)", hr);
        SysFreeString(V_BSTR(&args[0]));
        dcall(fs2, L"Write", &r, (args[0] = vbytes(payload, 6), args), 1);
        VariantClear(&args[0]);
        dcall(fs2, L"Close", &r, NULL, 0);
        check_wav(L"f2.wav", "FileStream with Format set first", WAVE_FORMAT_PCM, 2, 16000, 8, 6);
        hr = dputref(fs2, L"Format", NULL);
        CHECKF(hr == E_INVALIDARG, "FileStream: putref Format(NULL) is E_INVALIDARG (%#lx)", hr);
        IDispatch_Release(fs2);
        IDispatch_Release(f1);
    }
    if (sp) ISpStream_Release(sp);
    IDispatch_Release(fs);
}

int main(void)
{
    WCHAR tmp[MAX_PATH];

    CoInitialize(NULL);
    GetTempPathW(MAX_PATH, tmp);
    swprintf(dir, MAX_PATH, L"%lsssprobe%lu", tmp, GetCurrentProcessId());
    CreateDirectoryW(dir, NULL);

    test_spstream_memory();
    test_bindtofile();
    test_resource_manager();
    test_file_stream();

    CoUninitialize();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures ? 1 : 0;
}
