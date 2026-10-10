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
typedef void WS_LISTENER;
typedef struct { int id; void *value; ULONG valueSize; } WS_LISTENER_PROPERTY;
static HRESULT (WINAPI *WsCreateListener)(int, int, const WS_LISTENER_PROPERTY *, ULONG, void *, WS_LISTENER **, void *);
static HRESULT (WINAPI *WsSetListenerProperty)(WS_LISTENER *, int, const void *, ULONG, void *);
static HRESULT (WINAPI *WsGetListenerProperty)(WS_LISTENER *, int, void *, ULONG, void *);
static void (WINAPI *WsFreeListener)(WS_LISTENER *);
static HRESULT (WINAPI *WsAlloc)(WS_HEAP *, SIZE_T, void **, void *);
static HRESULT (WINAPI *WsResetHeap)(WS_HEAP *, void *);
static HRESULT (WINAPI *WsFreeHeap2)(WS_HEAP *);
static HRESULT (WINAPI *WsGetHeapProperty)(WS_HEAP *, int, void *, ULONG, void *);
static HRESULT (WINAPI *WsCreateHeap)(SIZE_T, SIZE_T, void *, ULONG, WS_HEAP **, void *);
#define WS_HEAP_PROPERTY_REQUESTED_SIZE 2
#define WS_HEAP_PROPERTY_ACTUAL_SIZE 3
#define WS_CHANNEL_TYPE_DUPLEX_SESSION 7
#define WS_TCP_CHANNEL_BINDING 1
#define WS_LISTENER_PROPERTY_LISTEN_BACKLOG 0
#define WS_LISTENER_PROPERTY_IP_VERSION 1
#define WS_LISTENER_PROPERTY_STATE 2
#define WS_IP_VERSION_4 1
#define WS_IP_VERSION_6 2
#define WS_IP_VERSION_AUTO 3
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

static SIZE_T prop(WS_HEAP *h, int id)
{
    SIZE_T v = 0xdeadbeef;
    WsGetHeapProperty(h, id, &v, sizeof(v), NULL);
    return v;
}

int main(void)
{
    HMODULE ws = LoadLibraryA("webservices.dll");
    WS_HEAP *heap;
    void *p;
    HRESULT hr;

#define LOAD(f) do { *(void **)&f = (void *)GetProcAddress(ws, #f); if (!f) { printf("FAIL  no %s\n", #f); puts("RESULT: FAIL"); return 1; } } while (0)
    LOAD(WsAlloc); LOAD(WsResetHeap); LOAD(WsGetHeapProperty); LOAD(WsCreateHeap);
    WsFreeHeap2 = (void *)GetProcAddress(ws, "WsFreeHeap");

    hr = WsCreateHeap(1 << 16, 0, NULL, 0, &heap, NULL);
    CHECK(hr == S_OK, "a heap");
    CHECK(prop(heap, WS_HEAP_PROPERTY_REQUESTED_SIZE) == 0 && prop(heap, WS_HEAP_PROPERTY_ACTUAL_SIZE) == 0, "nothing requested, nothing actual");
    WsAlloc(heap, 16, &p, NULL);
    CHECK(prop(heap, WS_HEAP_PROPERTY_REQUESTED_SIZE) == 16, "16 bytes requested");
    CHECK(prop(heap, WS_HEAP_PROPERTY_ACTUAL_SIZE) == 128, "the first chunk is 128 bytes (%Iu)", prop(heap, WS_HEAP_PROPERTY_ACTUAL_SIZE));
    WsAlloc(heap, 112, &p, NULL);
    CHECK(prop(heap, WS_HEAP_PROPERTY_ACTUAL_SIZE) == 128, "128 bytes still fit it (%Iu)", prop(heap, WS_HEAP_PROPERTY_ACTUAL_SIZE));
    WsAlloc(heap, 1, &p, NULL);
    CHECK(prop(heap, WS_HEAP_PROPERTY_REQUESTED_SIZE) == 129, "129 requested");
    CHECK(prop(heap, WS_HEAP_PROPERTY_ACTUAL_SIZE) == 384, "the byte over needs a chunk of 256: 384 (%Iu)", prop(heap, WS_HEAP_PROPERTY_ACTUAL_SIZE));
    hr = WsResetHeap(heap, NULL);
    CHECK(hr == S_OK, "reset");
    CHECK(prop(heap, WS_HEAP_PROPERTY_REQUESTED_SIZE) == 0, "nothing requested after it");
    CHECK(prop(heap, WS_HEAP_PROPERTY_ACTUAL_SIZE) == 128, "the first chunk stays: 128 (%Iu)", prop(heap, WS_HEAP_PROPERTY_ACTUAL_SIZE));
    WsFreeHeap2(heap);

    puts(failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
