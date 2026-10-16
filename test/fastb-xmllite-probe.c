/* xmllite gate: the built-in ISO 8859 encodings, for reading and for writing */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <initguid.h>
#include <xmllite.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { failures++; printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static IStream *mem_stream(const void *data, UINT len)
{
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, len ? len : 1);
    IStream *s = NULL;
    void *p = GlobalLock(h);
    if (len) memcpy(p, data, len);
    GlobalUnlock(h);
    CreateStreamOnHGlobal(h, TRUE, &s);
    return s;
}

static HRESULT read_text(const char *xml, UINT len, const WCHAR *enc, WCHAR *out)
{
    IStream *stream = mem_stream(xml, len);
    IXmlReaderInput *input = NULL;
    IXmlReader *reader = NULL;
    XmlNodeType type;
    HRESULT hr;

    out[0] = 0;
    CreateXmlReader(&IID_IXmlReader, (void **)&reader, NULL);
    hr = CreateXmlReaderInputWithEncodingName((IUnknown *)stream, NULL, enc, TRUE, NULL, &input);
    if (hr == S_OK) hr = IXmlReader_SetInput(reader, (IUnknown *)input);
    while (hr == S_OK && (hr = IXmlReader_Read(reader, &type)) == S_OK)
    {
        if (type == XmlNodeType_Text)
        {
            const WCHAR *v = NULL;
            IXmlReader_GetValue(reader, &v, NULL);
            wcscpy(out, v);
            break;
        }
    }
    if (input) IUnknown_Release(input);
    IXmlReader_Release(reader);
    IStream_Release(stream);
    return hr;
}

static HRESULT write_text(const WCHAR *enc, const WCHAR *text, BYTE *out, ULONG *len)
{
    IStream *stream = mem_stream(NULL, 0);
    IXmlWriterOutput *output = NULL;
    IXmlWriter *writer = NULL;
    HRESULT hr;
    HGLOBAL h;

    CreateXmlWriter(&IID_IXmlWriter, (void **)&writer, NULL);
    hr = CreateXmlWriterOutputWithEncodingName((IUnknown *)stream, NULL, enc, &output);
    if (hr == S_OK) hr = IXmlWriter_SetOutput(writer, (IUnknown *)output);
    if (hr == S_OK)
    {
        IXmlWriter_SetProperty(writer, XmlWriterProperty_OmitXmlDeclaration, TRUE);
        IXmlWriter_WriteStartElement(writer, NULL, L"a", NULL);
        IXmlWriter_WriteString(writer, text);
        IXmlWriter_WriteEndElement(writer);
        IXmlWriter_Flush(writer);
        GetHGlobalFromStream(stream, &h);
        *len = (ULONG)GlobalSize(h);
        {
            STATSTG st;
            IStream_Stat(stream, &st, STATFLAG_NONAME);
            *len = st.cbSize.LowPart;
            memcpy(out, GlobalLock(h), *len);
            GlobalUnlock(h);
        }
    }
    if (output) IUnknown_Release(output);
    IXmlWriter_Release(writer);
    IStream_Release(stream);
    return hr;
}

int main(void)
{
    WCHAR text[64];
    HRESULT hr;
    BYTE out[256];
    ULONG len;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);

    hr = read_text("<a>caf\xe9</a>", 9, L"ISO-8859-1", text);
    CHECK(hr == S_OK && !wcscmp(text, L"café"), "ISO-8859-1 read: %#lx %ls", hr, text);
    hr = read_text("<a>\xe3\xe4</a>", 9, L"iso-8859-7", text);
    CHECK(hr == S_OK && !wcscmp(text, L"γδ"), "ISO-8859-7 read (greek): %#lx %ls", hr, text);
    hr = read_text("<a>\xb1</a>", 8, L"ISO-8859-2", text);
    CHECK(hr == S_OK && !wcscmp(text, L"ą"), "ISO-8859-2 read: %#lx %ls", hr, text);
    hr = read_text("<a>\xd0</a>", 8, L"ISO-8859-9", text);
    CHECK(hr == S_OK && !wcscmp(text, L"Ğ"), "ISO-8859-9 read: %#lx %ls", hr, text);
    hr = read_text("<a>x</a>", 8, L"ISO-8859-15", text);
    CHECK(hr == S_OK && !wcscmp(text, L"x"), "hint TRUE with a name that is not built in falls back to detection: %#lx", hr);
    hr = read_text("<a>x</a>", 8, L"KOI8-X", text);
    CHECK(hr == S_OK && !wcscmp(text, L"x"), "unknown name, hint TRUE: %#lx", hr);

    hr = read_text("\xef\xbb\xbf<a>\xc3\xa9</a>", 12, L"ISO-8859-1", text);
    CHECK(hr == S_OK && !wcscmp(text, L"\u00e9"), "a BOM wins over the named encoding: %#lx %ls", hr, text);

    hr = write_text(L"ISO-8859-1", L"café", out, &len);
    CHECK(hr == S_OK && len == 11 && out[6] == 0xe9, "ISO-8859-1 write: %#lx len %lu", hr, len);
    hr = write_text(L"ISO-8859-7", L"γ", out, &len);
    CHECK(hr == S_OK && len == 8 && out[3] == 0xe3, "ISO-8859-7 write: %#lx len %lu byte %02x", hr, len, out[3]);

    CoUninitialize();
    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
