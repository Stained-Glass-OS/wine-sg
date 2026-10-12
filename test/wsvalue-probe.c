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
#define WS_ELEMENT_CONTENT_TYPE_MAPPING 3
#define WS_READ_REQUIRED_VALUE 1
#define WS_READ_REQUIRED_POINTER 2
#define WS_STRING_TYPE 16
#define WS_BYTES_TYPE 18
#define WS_XML_STRING_TYPE 19
#define WS_E_INVALID_OPERATION ((HRESULT)0x803d0003)

static HRESULT (WINAPI *WsCreateReader)(void *, ULONG, WS_XML_READER **, void *);
static HRESULT (WINAPI *WsSetInput)(WS_XML_READER *, const WS_XML_READER_ENCODING *, const WS_XML_READER_INPUT *, void *, ULONG, void *);
static HRESULT (WINAPI *WsReadToStartElement)(WS_XML_READER *, const WS_XML_STRING *, const WS_XML_STRING *, BOOL *, void *);
static HRESULT (WINAPI *WsReadStartElement)(WS_XML_READER *, void *);
static HRESULT (WINAPI *WsReadEndElement)(WS_XML_READER *, void *);
static HRESULT (WINAPI *WsReadArray)(WS_XML_READER *, const WS_XML_STRING *, const WS_XML_STRING *, int, void *, ULONG, ULONG, ULONG, ULONG *, void *);
static HRESULT (WINAPI *WsReadValue)(WS_XML_READER *, int, void *, ULONG, void *);
static HRESULT (WINAPI *WsFreeReader)(WS_XML_READER *);
static HRESULT (WINAPI *WsGetXmlAttribute)(WS_XML_READER *, const WS_XML_STRING *, WS_HEAP *, WCHAR **, ULONG *, void *);
static HRESULT (WINAPI *WsCreateHeap)(SIZE_T, SIZE_T, void *, ULONG, WS_HEAP **, void *);
static HRESULT (WINAPI *WsFreeHeap)(WS_HEAP *);
static HRESULT (WINAPI *WsReadType)(WS_XML_READER *, int, int, const void *, int, WS_HEAP *, void *, ULONG, void *);
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
    WS_XML_STRING root = XS("a"), attr_x = XS("x"), attr_none = XS("none"), attr_y = XS("y");
    WS_XML_READER *reader;
    WS_HEAP *heap;
    BOOL found;
    HRESULT hr;
    WCHAR *chars;
    ULONG count;
    struct { ULONG length; WCHAR *chars; } str;
    struct { ULONG length; BYTE *bytes; } bytes;
    WS_XML_STRING xs;

#define LOAD(f) do { *(void **)&f = (void *)GetProcAddress(ws, #f); if (!f) { printf("FAIL  no %s\n", #f); puts("RESULT: FAIL"); return 1; } } while (0)
    LOAD(WsCreateReader); LOAD(WsSetInput); LOAD(WsReadToStartElement); LOAD(WsReadStartElement); LOAD(WsReadEndElement);
    LOAD(WsFreeReader); LOAD(WsGetXmlAttribute); LOAD(WsCreateHeap); LOAD(WsFreeHeap); LOAD(WsReadType);
    WsCreateHeap(1 << 16, 0, NULL, 0, &heap, NULL);

    /* attributes of the current element */
    open_reader(&reader, "<a p:x='2' x='value' y='' xmlns:p='urn:p'/>");
    WsReadToStartElement(reader, &root, NULL, &found, NULL);
    chars = (WCHAR *)1; count = 77;
    hr = WsGetXmlAttribute(reader, &attr_x, heap, &chars, &count, NULL);
    CHECK(hr == S_OK && chars && count == 5 && !memcmp(chars, L"value", 5 * sizeof(WCHAR)),
          "the x attribute in no namespace: \"value\" (hr %#lx, count %lu)", hr, count);
    chars = (WCHAR *)1; count = 77;
    hr = WsGetXmlAttribute(reader, &attr_none, heap, &chars, &count, NULL);
    CHECK(hr == S_FALSE && chars == NULL && count == 0, "no such attribute: S_FALSE, nothing (hr %#lx)", hr);
    chars = NULL; count = 77;
    hr = WsGetXmlAttribute(reader, &attr_y, heap, &chars, &count, NULL);
    CHECK(hr == S_OK && count == 0, "an empty attribute: S_OK, no characters (hr %#lx, count %lu)", hr, count);
    hr = WsGetXmlAttribute(reader, NULL, heap, &chars, &count, NULL);
    CHECK(hr == E_INVALIDARG, "no name: E_INVALIDARG");
    hr = WsGetXmlAttribute(reader, &attr_x, NULL, &chars, &count, NULL);
    CHECK(hr == E_INVALIDARG, "no heap: E_INVALIDARG");
    hr = WsGetXmlAttribute(NULL, &attr_x, heap, &chars, &count, NULL);
    CHECK(hr == E_INVALIDARG, "no reader: E_INVALIDARG");
    WsFreeReader(reader);

    /* empty content reads as empty values that still point somewhere */
    open_reader(&reader, "<t></t>");
    WsReadToStartElement(reader, NULL, NULL, &found, NULL);
    memset(&str, 0xcc, sizeof(str));
    hr = WsReadType(reader, WS_ELEMENT_CONTENT_TYPE_MAPPING, WS_STRING_TYPE, NULL, WS_READ_REQUIRED_VALUE, heap, &str, sizeof(str), NULL);
    CHECK(hr == S_OK && str.length == 0 && str.chars != NULL, "an empty WS_STRING: length 0, chars set (hr %#lx)", hr);
    WsFreeReader(reader);
    open_reader(&reader, "<t></t>");
    WsReadToStartElement(reader, NULL, NULL, &found, NULL);
    memset(&bytes, 0xcc, sizeof(bytes));
    hr = WsReadType(reader, WS_ELEMENT_CONTENT_TYPE_MAPPING, WS_BYTES_TYPE, NULL, WS_READ_REQUIRED_VALUE, heap, &bytes, sizeof(bytes), NULL);
    CHECK(hr == S_OK && bytes.length == 0 && bytes.bytes != NULL, "empty WS_BYTES: length 0, bytes set (hr %#lx)", hr);
    WsFreeReader(reader);
    open_reader(&reader, "<t></t>");
    WsReadToStartElement(reader, NULL, NULL, &found, NULL);
    memset(&xs, 0xcc, sizeof(xs));
    hr = WsReadType(reader, WS_ELEMENT_CONTENT_TYPE_MAPPING, WS_XML_STRING_TYPE, NULL, WS_READ_REQUIRED_VALUE, heap, &xs, sizeof(xs), NULL);
    CHECK(hr == S_OK && xs.length == 0 && xs.bytes != NULL, "an empty WS_XML_STRING: length 0, bytes set (hr %#lx)", hr);
    WsFreeReader(reader);
    WsFreeHeap(heap);

    puts(failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
