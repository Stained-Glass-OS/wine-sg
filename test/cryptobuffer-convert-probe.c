/* Probe for patches/sg/5058: CryptographicBuffer.ConvertStringToBinary and
 * ConvertBinaryToString (BinaryStringEncoding Utf8 0, Utf16LE 1, Utf16BE 2).
 * - each encoding gives the documented bytes for a string with ASCII, Latin-1 and
 *   a character beyond Latin-1, and turns them back into the same string;
 * - an empty string gives an empty buffer and back;
 * - an unknown encoding is E_INVALIDARG both ways.
 * ICryptographicBufferStatics: 9 CreateFromByteArray, 10 CopyToByteArray,
 * 15 ConvertStringToBinary, 16 ConvertBinaryToString.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include "sg-inspectable.h"

DEFINE_GUID(SG_IID_ICryptographicBufferStatics, 0x320b7e22, 0x3cb0, 0x4cdf, 0x86, 0x63, 0x1d, 0x28, 0x91, 0x00, 0x65, 0xeb);

typedef HRESULT (WINAPI *from_bytes_t)(void *, UINT32, BYTE *, void **);
typedef HRESULT (WINAPI *to_bytes_t)(void *, void *, UINT32 *, BYTE **);
typedef HRESULT (WINAPI *to_binary_t)(void *, HSTRING, int, void **);
typedef HRESULT (WINAPI *to_string_t)(void *, int, void *, HSTRING *);

int main(void)
{
    static const struct { int encoding; const WCHAR *text; BYTE bytes[16]; UINT32 len; } tests[] =
    {
        {0, L"A\x00e9\x20ac", {0x41, 0xc3, 0xa9, 0xe2, 0x82, 0xac}, 6},
        {1, L"A\x00e9\x20ac", {0x41, 0x00, 0xe9, 0x00, 0xac, 0x20}, 6},
        {2, L"A\x00e9\x20ac", {0x00, 0x41, 0x00, 0xe9, 0x20, 0xac}, 6},
        {0, L"", {0}, 0},
        {1, L"", {0}, 0},
        {2, L"", {0}, 0},
    };
    void *statics, *buffer;
    unsigned int i;
    HRESULT hr;

    RoInitialize(RO_INIT_MULTITHREADED);
    if (!(statics = sg_factory(L"Windows.Security.Cryptography.CryptographicBuffer", &SG_IID_ICryptographicBufferStatics)))
        goto done;
    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        HSTRING s = NULL, back = NULL;
        UINT32 len = 0xdeadbeef;
        BYTE *bytes = NULL, copy[16];
        const WCHAR *buf;

        WindowsCreateString(tests[i].text, lstrlenW(tests[i].text), &s);
        buffer = NULL;
        hr = CALL(statics, 15, to_binary_t, s, tests[i].encoding, &buffer);
        check(hr == S_OK && buffer, "%u: ConvertStringToBinary %#lx", i, hr);
        if (buffer)
        {
            hr = CALL(statics, 10, to_bytes_t, buffer, &len, &bytes);
            check(hr == S_OK && len == tests[i].len && (!len || !memcmp(bytes, tests[i].bytes, len)),
                  "%u: the bytes (%lu of them)", i, (unsigned long)len);
            CoTaskMemFree(bytes);
            CALL(buffer, 2, sg_rel_t);
        }
        WindowsDeleteString(s);

        /* back from a buffer of the documented bytes */
        memcpy(copy, tests[i].bytes, sizeof(copy));
        buffer = NULL;
        hr = CALL(statics, 9, from_bytes_t, tests[i].len, copy, &buffer);
        hr = buffer ? CALL(statics, 16, to_string_t, tests[i].encoding, buffer, &back) : hr;
        buf = hr == S_OK ? WindowsGetStringRawBuffer(back, &len) : NULL;
        check(hr == S_OK && buf && !lstrcmpW(buf, tests[i].text) && len == lstrlenW(tests[i].text),
              "%u: ConvertBinaryToString %#lx \"%ls\"", i, hr, buf ? buf : L"");
        WindowsDeleteString(back);
        if (buffer) CALL(buffer, 2, sg_rel_t);
    }

    /* an encoding that is not a BinaryStringEncoding */
    {
        HSTRING s, back = NULL;
        BYTE b[2] = {0x41, 0};
        WindowsCreateString(L"A", 1, &s);
        buffer = (void *)0xdeadbeef;
        hr = CALL(statics, 15, to_binary_t, s, 3, &buffer);
        check(hr == E_INVALIDARG, "ConvertStringToBinary encoding 3: %#lx", hr);
        WindowsDeleteString(s);
        buffer = NULL;
        CALL(statics, 9, from_bytes_t, 2, b, &buffer);
        hr = buffer ? CALL(statics, 16, to_string_t, 3, buffer, &back) : E_FAIL;
        check(hr == E_INVALIDARG, "ConvertBinaryToString encoding 3: %#lx", hr);
        if (buffer) CALL(buffer, 2, sg_rel_t);
    }
    CALL(statics, 2, sg_rel_t);
done:
    RoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
