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
    WS_XML_STRING root = XS("a"), item = XS("i"), other = XS("j"), ns = XS(""), wrong = XS("w");
    WS_XML_READER *reader;
    WS_XML_WRITER *writer;
    INT32 values[6];
    ULONG actual;
    BOOL found;
    HRESULT hr;
    int v;

#define LOAD(f) do { *(void **)&f = (void *)GetProcAddress(ws, #f); if (!f) { printf("FAIL  no %s\n", #f); puts("RESULT: FAIL"); return 1; } } while (0)
    LOAD(WsCreateReader); LOAD(WsSetInput); LOAD(WsReadToStartElement); LOAD(WsReadStartElement); LOAD(WsReadEndElement);
    LOAD(WsReadArray); LOAD(WsReadValue); LOAD(WsFreeReader); LOAD(WsCreateWriter); LOAD(WsSetOutput);
    LOAD(WsWriteStartElement); LOAD(WsWriteEndElement); LOAD(WsWriteArray); LOAD(WsGetWriterProperty); LOAD(WsFreeWriter);

    /* reading */
    hr = open_reader(&reader, "<a><i>1</i><i>2</i><i>-3</i><j>9</j></a>");
    CHECK(hr == S_OK, "reader on a buffer");
    hr = WsReadToStartElement(reader, &root, &ns, &found, NULL);
    CHECK(hr == S_OK && found, "to <a>");
    hr = WsReadStartElement(reader, NULL);
    CHECK(hr == S_OK, "into <a>");
    memset(values, 0xcc, sizeof(values));
    actual = 0xdeadbeef;
    hr = WsReadArray(reader, &item, &ns, WS_INT32_VALUE_TYPE, values, sizeof(values), 1, 5, &actual, NULL);
    CHECK(hr == S_OK && actual == 3, "reads the 3 <i> elements into items 1.. of 6 (actual %lu, hr %#lx)", actual, hr);
    CHECK(values[1] == 1 && values[2] == 2 && values[3] == -3, "the values: %d %d %d", values[1], values[2], values[3]);
    CHECK(values[0] == 0xcccccccc && values[4] == 0xcccccccc, "the other items are left alone");
    v = 0;
    hr = WsReadToStartElement(reader, &other, &ns, &found, NULL);
    CHECK(hr == S_OK && found, "<j> is next (hr %#lx found %d)", hr, found);
    hr = WsReadStartElement(reader, NULL);
    hr = WsReadValue(reader, WS_INT32_VALUE_TYPE, &v, sizeof(v), NULL);
    CHECK(hr == S_OK && v == 9, "and holds 9");
    WsFreeReader(reader);

    /* an array that stops short, a wrong name, and the checks */
    open_reader(&reader, "<a><i>5</i><i>6</i></a>");
    WsReadToStartElement(reader, &root, &ns, &found, NULL);
    WsReadStartElement(reader, NULL);
    memset(values, 0, sizeof(values));
    actual = 0xdeadbeef;
    hr = WsReadArray(reader, &wrong, &ns, WS_INT32_VALUE_TYPE, values, sizeof(values), 0, 3, &actual, NULL);
    CHECK(hr == S_OK && actual == 0, "no element of that name: none read, no error (actual %lu, hr %#lx)", actual, hr);
    hr = WsReadArray(reader, &item, &ns, WS_INT32_VALUE_TYPE, values, sizeof(values), 0, 0, &actual, NULL);
    CHECK(hr == S_OK && actual == 0, "count 0");
    hr = WsReadArray(reader, &item, &ns, WS_INT32_VALUE_TYPE, values, sizeof(values), 4, 3, &actual, NULL);
    CHECK(hr == E_INVALIDARG, "offset + count beyond the array: E_INVALIDARG (%#lx)", hr);
    hr = WsReadArray(reader, &item, &ns, WS_INT32_VALUE_TYPE, values, 7, 0, 1, &actual, NULL);
    CHECK(hr == E_INVALIDARG, "size not a multiple of the item size: E_INVALIDARG (%#lx)", hr);
    hr = WsReadArray(reader, &item, &ns, WS_INT32_VALUE_TYPE, NULL, 0, 0, 1, &actual, NULL);
    CHECK(hr == E_INVALIDARG, "no array for a positive count: E_INVALIDARG (%#lx)", hr);
    hr = WsReadArray(reader, NULL, &ns, WS_INT32_VALUE_TYPE, values, sizeof(values), 0, 1, &actual, NULL);
    CHECK(hr == E_INVALIDARG, "no name: E_INVALIDARG");
    hr = WsReadArray(reader, &item, &ns, 99, values, sizeof(values), 0, 1, &actual, NULL);
    CHECK(hr == E_INVALIDARG, "bad value type: E_INVALIDARG");
    hr = WsReadArray(reader, &item, &ns, WS_INT32_VALUE_TYPE, values, sizeof(values), 0, 1, NULL, NULL);
    CHECK(hr == E_INVALIDARG, "no actual count: E_INVALIDARG");
    hr = WsReadArray(NULL, &item, &ns, WS_INT32_VALUE_TYPE, values, sizeof(values), 0, 1, &actual, NULL);
    CHECK(hr == E_INVALIDARG, "no reader: E_INVALIDARG");
    actual = 0xdeadbeef;
    hr = WsReadArray(reader, &item, &ns, WS_INT32_VALUE_TYPE, values, sizeof(values), 0, 6, &actual, NULL);
    CHECK(hr == S_OK && actual == 2 && values[0] == 5 && values[1] == 6, "asking for more than there are: 2 read (hr %#lx actual %lu: %d %d)", hr, actual, values[0], values[1]);
    WsFreeReader(reader);
    hr = WsCreateReader(NULL, 0, &reader, NULL);
    hr = WsReadArray(reader, &item, &ns, WS_INT32_VALUE_TYPE, values, sizeof(values), 0, 1, &actual, NULL);
    CHECK(hr == WS_E_INVALID_OPERATION, "a reader with no input: WS_E_INVALID_OPERATION (%#lx)", hr);
    WsFreeReader(reader);

    /* writing from an offset */
    {
        WS_XML_WRITER_TEXT_ENCODING enc = { { WS_XML_WRITER_ENCODING_TYPE_TEXT }, 1 };
        WS_XML_WRITER_BUFFER_OUTPUT out = { { WS_XML_WRITER_OUTPUT_TYPE_BUFFER } };
        WS_BYTES bytes;
        INT32 src[4] = { 10, 20, 30, 40 };
        char text[256];

        hr = WsCreateWriter(NULL, 0, &writer, NULL);
        hr = WsSetOutput(writer, &enc.encoding, &out.output, NULL, 0, NULL);
        CHECK(hr == S_OK, "writer to a buffer (%#lx)", hr);
        hr = WsWriteStartElement(writer, NULL, &root, &ns, NULL);
        hr = WsWriteArray(writer, &item, &ns, WS_INT32_VALUE_TYPE, src, sizeof(src), 1, 2, NULL);
        CHECK(hr == S_OK, "WsWriteArray from offset 1, 2 items");
        hr = WsWriteEndElement(writer, NULL);
        memset(&bytes, 0, sizeof(bytes));
        hr = WsGetWriterProperty(writer, WS_XML_WRITER_PROPERTY_BYTES, &bytes, sizeof(bytes), NULL);
        memset(text, 0, sizeof(text));
        if (hr == S_OK && bytes.length < sizeof(text)) memcpy(text, bytes.bytes, bytes.length);
        CHECK(!strcmp(text, "<a><i>20</i><i>30</i></a>"), "written: %s", text);
        WsFreeWriter(writer);
    }

    puts(failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
