/* webservices WsReadArray and the WsWriteArray offset (patches/sg/2447), run by test/wsarray-gate.sh. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef struct { int encodingType; } WS_XML_READER_ENCODING;
typedef struct { WS_XML_READER_ENCODING encoding; int charSet; } WS_XML_READER_TEXT_ENCODING;
typedef struct { int inputType; } WS_XML_READER_INPUT;
typedef struct { WS_XML_READER_INPUT input; void *encodedData; ULONG encodedDataSize; } WS_XML_READER_BUFFER_INPUT;
typedef struct { int encodingType; } WS_XML_WRITER_ENCODING;
typedef struct { WS_XML_WRITER_ENCODING encoding; int charSet; } WS_XML_WRITER_TEXT_ENCODING;
typedef struct { int outputType; } WS_XML_WRITER_OUTPUT;
typedef struct { WS_XML_WRITER_OUTPUT output; } WS_XML_WRITER_BUFFER_OUTPUT;
typedef struct { ULONG length; BYTE *bytes; void *dictionary; ULONG id; } WS_XML_STRING;
typedef struct { ULONG length; BYTE *bytes; } WS_BYTES;
typedef void WS_XML_READER, WS_XML_WRITER, WS_HEAP;
#define WS_XML_READER_ENCODING_TYPE_TEXT 1
#define WS_XML_READER_INPUT_TYPE_BUFFER 1
#define WS_XML_WRITER_ENCODING_TYPE_TEXT 1
#define WS_XML_WRITER_OUTPUT_TYPE_BUFFER 1
#define WS_XML_WRITER_PROPERTY_BYTES 9
#define WS_INT32_VALUE_TYPE 3
#define WS_BOOL_VALUE_TYPE 0
#define WS_E_INVALID_OPERATION ((HRESULT)0x803d0003)

static HRESULT (WINAPI *WsCreateReader)(void *, ULONG, WS_XML_READER **, void *);
static HRESULT (WINAPI *WsSetInput)(WS_XML_READER *, const WS_XML_READER_ENCODING *, const WS_XML_READER_INPUT *, void *, ULONG, void *);
static HRESULT (WINAPI *WsReadToStartElement)(WS_XML_READER *, const WS_XML_STRING *, const WS_XML_STRING *, BOOL *, void *);
static HRESULT (WINAPI *WsReadStartElement)(WS_XML_READER *, void *);
static HRESULT (WINAPI *WsReadEndElement)(WS_XML_READER *, void *);
static HRESULT (WINAPI *WsReadArray)(WS_XML_READER *, const WS_XML_STRING *, const WS_XML_STRING *, int, void *, ULONG, ULONG, ULONG, ULONG *, void *);
static HRESULT (WINAPI *WsReadValue)(WS_XML_READER *, int, void *, ULONG, void *);
static HRESULT (WINAPI *WsFreeReader)(WS_XML_READER *);
static HRESULT (WINAPI *WsCreateWriter)(void *, ULONG, WS_XML_WRITER **, void *);
static HRESULT (WINAPI *WsSetOutput)(WS_XML_WRITER *, const WS_XML_WRITER_ENCODING *, const WS_XML_WRITER_OUTPUT *, void *, ULONG, void *);
static HRESULT (WINAPI *WsWriteStartElement)(WS_XML_WRITER *, const void *, const WS_XML_STRING *, const WS_XML_STRING *, void *);
static HRESULT (WINAPI *WsWriteEndElement)(WS_XML_WRITER *, void *);
static HRESULT (WINAPI *WsWriteArray)(WS_XML_WRITER *, const WS_XML_STRING *, const WS_XML_STRING *, int, const void *, ULONG, ULONG, ULONG, void *);
static HRESULT (WINAPI *WsGetWriterProperty)(WS_XML_WRITER *, int, void *, ULONG, void *);
static HRESULT (WINAPI *WsFreeWriter)(WS_XML_WRITER *);

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  " __VA_ARGS__); printf("\n"); } else printf("PASS  " __VA_ARGS__), printf("\n"); } while (0)

#define XS(name) { sizeof(name) - 1, (BYTE *)name, NULL, 0 }

static HRESULT open_reader(WS_XML_READER **r, const char *text)
{
    WS_XML_READER_TEXT_ENCODING enc = { { WS_XML_READER_ENCODING_TYPE_TEXT }, 0 };
    WS_XML_READER_BUFFER_INPUT in = { { WS_XML_READER_INPUT_TYPE_BUFFER }, (void *)text, strlen(text) };
    HRESULT hr = WsCreateReader(NULL, 0, r, NULL);
    if (hr != S_OK) return hr;
    return WsSetInput(*r, &enc.encoding, &in.input, NULL, 0, NULL);
}

int main(void)
{
    HMODULE ws = LoadLibraryA("webservices.dll");
    WS_XML_STRING root = XS("a"), b = XS("b"), c = XS("c"), ns = XS(""), ns2 = XS("urn:other");
    WS_XML_READER *reader;
    BOOL found;
    HRESULT hr;

#define LOAD(f) do { *(void **)&f = (void *)GetProcAddress(ws, #f); if (!f) { printf("FAIL  no %s\n", #f); puts("RESULT: FAIL"); return 1; } } while (0)
    LOAD(WsCreateReader); LOAD(WsSetInput); LOAD(WsReadToStartElement); LOAD(WsReadStartElement); LOAD(WsReadEndElement);
    LOAD(WsFreeReader);

    hr = open_reader(&reader, "<a><b>1</b>\n  <b>2</b>\n</a>");
    CHECK(hr == S_OK, "reader on a buffer");
    found = -1;
    hr = WsReadToStartElement(reader, &c, &ns, &found, NULL);
    CHECK(hr == S_OK && found == FALSE, "the document element is not <c>: found FALSE (hr %#lx, found %d)", hr, found);
    found = -1;
    hr = WsReadToStartElement(reader, &root, &ns2, &found, NULL);
    CHECK(hr == S_OK && found == FALSE, "nor <a> in another namespace: found FALSE (found %d)", found);
    found = -1;
    hr = WsReadToStartElement(reader, &root, &ns, &found, NULL);
    CHECK(hr == S_OK && found == TRUE, "but it is <a>: found TRUE (hr %#lx, found %d)", hr, found);
    found = -1;
    hr = WsReadToStartElement(reader, NULL, NULL, &found, NULL);
    CHECK(hr == S_OK && found == TRUE, "no name asked for: found TRUE");
    hr = WsReadStartElement(reader, NULL);
    CHECK(hr == S_OK, "into <a>");
    found = -1;
    hr = WsReadToStartElement(reader, &b, &ns, &found, NULL);
    CHECK(hr == S_OK && found == TRUE, "<b> next");
    hr = WsReadStartElement(reader, NULL);
    hr = WsReadEndElement(reader, NULL);
    CHECK(hr == S_OK, "<b> read through");
    found = -1;
    hr = WsReadToStartElement(reader, &c, &ns, &found, NULL);
    CHECK(hr == S_OK && found == FALSE, "the second <b> is not <c> (found %d)", found);
    found = -1;
    hr = WsReadToStartElement(reader, &b, &ns, &found, NULL);
    CHECK(hr == S_OK && found == TRUE, "but is <b> (found %d)", found);
    hr = WsReadStartElement(reader, NULL);
    hr = WsReadEndElement(reader, NULL);
    found = -1;
    hr = WsReadToStartElement(reader, &b, &ns, &found, NULL);
    CHECK(hr == S_OK && found == FALSE, "at the end tag of <a>: found FALSE, no error (hr %#lx, found %d)", hr, found);
    hr = WsReadEndElement(reader, NULL);
    CHECK(hr == S_OK, "and </a> can still be read");
    WsFreeReader(reader);

    puts(failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
