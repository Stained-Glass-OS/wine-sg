/* webservices' reader behaviours that Wine's own tests record from real Windows
 * (patches/sg/2426), run by test/wsreader-gate.sh: which text starts a
 * document, the XML declaration's shape, an empty document, and the stream
 * properties a buffer input does not have. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

/* webservices.h is not in the MinGW headers: what is used here, declared by hand */
typedef struct { int encodingType; } WS_XML_READER_ENCODING;
typedef struct { WS_XML_READER_ENCODING encoding; int charSet; } WS_XML_READER_TEXT_ENCODING;
typedef struct { int inputType; } WS_XML_READER_INPUT;
typedef struct { WS_XML_READER_INPUT input; void *encodedData; ULONG encodedDataSize; } WS_XML_READER_BUFFER_INPUT;
typedef struct { int nodeType; } WS_XML_NODE;
typedef struct { ULONG length; BYTE *bytes; void *dictionary; ULONG id; } WS_XML_STRING;
typedef void WS_XML_READER, WS_HEAP;
#define WS_XML_READER_ENCODING_TYPE_TEXT 1
#define WS_XML_READER_INPUT_TYPE_BUFFER 1
#define WS_XML_READER_PROPERTY_UTF8_TRIM_SIZE 7
#define WS_XML_READER_PROPERTY_STREAM_BUFFER_SIZE 8
#define WS_XML_READER_PROPERTY_CHARSET 4
#define WS_E_NUMERIC_OVERFLOW ((HRESULT)0x803d0002)
#define WS_E_INVALID_OPERATION ((HRESULT)0x803d0003)
#define WS_ELEMENT_CONTENT_TYPE_MAPPING 3
#define WS_READ_REQUIRED_VALUE 1
#define WS_MOVE_TO_EOF 10
#define WS_E_INVALID_FORMAT ((HRESULT)0x803d0000)

static HRESULT (WINAPI *WsCreateReader)(void *, ULONG, WS_XML_READER **, void *);
static void (WINAPI *WsFreeReader)(WS_XML_READER *);
static HRESULT (WINAPI *WsSetInput)(WS_XML_READER *, const WS_XML_READER_ENCODING *, const WS_XML_READER_INPUT *, void *, ULONG, void *);
static HRESULT (WINAPI *WsReadNode)(WS_XML_READER *, void *);
static HRESULT (WINAPI *WsReadToStartElement)(WS_XML_READER *, const WS_XML_STRING *, const WS_XML_STRING *, BOOL *, void *);
static HRESULT (WINAPI *WsReadStartElement)(WS_XML_READER *, void *);
static HRESULT (WINAPI *WsReadEndElement)(WS_XML_READER *, void *);
static HRESULT (WINAPI *WsReadType)(WS_XML_READER *, int, int, const void *, int, WS_HEAP *, void *, ULONG, void *);
static HRESULT (WINAPI *WsGetNamespaceFromPrefix)(WS_XML_READER *, const WS_XML_STRING *, BOOL, const WS_XML_STRING **, void *);
static HRESULT (WINAPI *WsMoveReader)(WS_XML_READER *, int, BOOL *, void *);
static HRESULT (WINAPI *WsReadXmlBuffer)(WS_XML_READER *, WS_HEAP *, void **, void *);
static HRESULT (WINAPI *WsCreateHeap)(SIZE_T, SIZE_T, void *, ULONG, WS_HEAP **, void *);
static HRESULT (WINAPI *WsGetReaderProperty)(WS_XML_READER *, int, void *, ULONG, void *);

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  " __VA_ARGS__); printf("\n"); } } while (0)

static HRESULT open_reader(WS_XML_READER **r, const char *text, int size)
{
    WS_XML_READER_TEXT_ENCODING enc = { { WS_XML_READER_ENCODING_TYPE_TEXT }, 0 };
    WS_XML_READER_BUFFER_INPUT in = { { WS_XML_READER_INPUT_TYPE_BUFFER }, (void *)text, size };
    HRESULT hr = WsCreateReader(NULL, 0, r, NULL);
    if (hr != S_OK) return hr;
    return WsSetInput(*r, &enc.encoding, &in.input, NULL, 0, NULL);
}

static const struct
{
    const char *name;
    const char *text;
    HRESULT hr;
    int type;   /* node type when it reads */
}
docs[] =
{
    { "plain element",           "<t/>",                                   S_OK, 2 },
    { "declaration only",        "<?xml version=\"1.0\"?>",                S_OK, 10 },
    { "declaration, encoding",   "<?xml version=\"1.0\" encoding=\"utf-8\"?><t/>", S_OK, 2 },
    { "declaration, standalone", "<?xml version='1.0' standalone='yes'?><t/>",     S_OK, 2 },
    { "declaration without version", "<?xml ?>",                           WS_E_INVALID_FORMAT, 0 },
    { "declaration unterminated", "<?xml version=\"1.0\"",                 WS_E_INVALID_FORMAT, 0 },
    { "declaration bad quote",   "<?xml version=1.0?><t/>",                WS_E_INVALID_FORMAT, 0 },
    { "empty document",          "",                                       WS_E_INVALID_FORMAT, 0 },
    { "text first",              "text",                                   S_OK, 4 },
    { "stray angle",             "<",                                      WS_E_INVALID_FORMAT, 0 },
    { "stray decl start",        "<?x",                                    WS_E_INVALID_FORMAT, 0 },
};

/* the charset is worked out from the first bytes, and only a plausible declaration lets it be */
static const struct { const char *name; const char *data; int size; HRESULT hr; } charsets[] =
{
    { "utf-8 declaration, no version", "<?xml encoding=\"utf-8\"?>", 24, WS_E_INVALID_FORMAT },
    { "truncated declaration",         "<?xml", 5, WS_E_INVALID_FORMAT },
    { "not the declaration",           "<?yml", 5, WS_E_INVALID_FORMAT },
    { "utf-16 declaration start",      "<\0?\0", 4, WS_E_INVALID_FORMAT },
    { "utf-16 truncated declaration",  "<\0?\0x\0m\0l\0", 10, WS_E_INVALID_FORMAT },
    { "utf-8 element",                 "<a/>", 4, S_OK },
    { "utf-8 comment start",           "<!--", 4, S_OK },
    { "utf-16 element",                "<\0a\0/\0>\0", 8, S_OK },
};

/* numbers: 8 and 16 bit overflow, 32 and 64 bit are malformed, a minus on an unsigned is below the range */
static const struct { const char *text; int type; HRESULT hr; } numbers[] =
{
    { "<t>-129</t>",                  1, WS_E_NUMERIC_OVERFLOW },
    { "<t>-32769</t>",                2, WS_E_NUMERIC_OVERFLOW },
    { "<t>-2147483648</t>",           3, S_OK },
    { "<t>-2147483649</t>",           3, WS_E_INVALID_FORMAT },
    { "<t>-9223372036854775809</t>",  4, WS_E_INVALID_FORMAT },
    { "<t>-255</t>",                  5, WS_E_NUMERIC_OVERFLOW },
    { "<t>+255</t>",                  5, WS_E_INVALID_FORMAT },
    { "<t>256</t>",                   5, WS_E_NUMERIC_OVERFLOW },
    { "<t>4294967296</t>",            7, WS_E_NUMERIC_OVERFLOW },
    { "<t>18446744073709551615</t>",  8, S_OK },
    { "<t>18446744073709551616</t>",  8, WS_E_INVALID_FORMAT },
};

int main(void)
{
    HMODULE m = LoadLibraryA("webservices.dll");
    WS_XML_READER *r;
    HRESULT hr;
    ULONG v, i;

#define L(f) f = (void *)GetProcAddress(m, #f)
    L(WsCreateReader); L(WsFreeReader); L(WsSetInput); L(WsReadNode); L(WsGetReaderProperty);
    L(WsReadToStartElement); L(WsReadStartElement); L(WsReadEndElement); L(WsReadType);
    L(WsGetNamespaceFromPrefix); L(WsMoveReader); L(WsReadXmlBuffer); L(WsCreateHeap);
#undef L

    for (i = 0; i < sizeof(docs) / sizeof(docs[0]); i++)
    {
        WS_XML_NODE *node = NULL;
        hr = open_reader(&r, docs[i].text, strlen(docs[i].text));
        CHECK(hr == S_OK, "%s: set input %#lx", docs[i].name, hr);
        hr = WsReadNode(r, NULL);
        CHECK(hr == docs[i].hr, "%s: ReadNode %#lx, expected %#lx", docs[i].name, hr, docs[i].hr);
        (void)node;
        WsFreeReader(r);
    }

    for (i = 0; i < sizeof(charsets) / sizeof(charsets[0]); i++)
    {
        int cs = -1;
        hr = open_reader(&r, charsets[i].data, charsets[i].size);
        CHECK(hr == S_OK, "%s: set input %#lx", charsets[i].name, hr);
        hr = WsGetReaderProperty(r, WS_XML_READER_PROPERTY_CHARSET, &cs, sizeof(cs), NULL);
        CHECK(hr == charsets[i].hr, "%s: charset %#lx, expected %#lx", charsets[i].name, hr, charsets[i].hr);
        WsFreeReader(r);
    }

    {
        WS_HEAP *heap;
        const WS_XML_STRING *ns;
        WS_XML_STRING prefix = { 6, (BYTE *)"prefix" };
        void *xmlbuf = NULL;
        const char *dup = "<t xmlns:p='ns' xmlns:p='ns2'></t>", *nsdoc = "<t xmlns:prefix='ns'></t>";

        hr = WsCreateHeap(1 << 16, 0, NULL, 0, &heap, NULL);
        CHECK(hr == S_OK, "heap %#lx", hr);
        for (i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++)
        {
            unsigned char val[8];
            hr = open_reader(&r, numbers[i].text, strlen(numbers[i].text));
            if (hr == S_OK) hr = WsReadToStartElement(r, NULL, NULL, NULL, NULL);
            if (hr == S_OK) hr = WsReadStartElement(r, NULL);
            CHECK(hr == S_OK, "%s: prepare %#lx", numbers[i].text, hr);
            hr = WsReadType(r, WS_ELEMENT_CONTENT_TYPE_MAPPING, numbers[i].type, NULL, WS_READ_REQUIRED_VALUE,
                            heap, val, numbers[i].type == 4 || numbers[i].type == 8 ? 8 : 4, NULL);
            CHECK(hr == numbers[i].hr, "%s: read %#lx, expected %#lx", numbers[i].text, hr, numbers[i].hr);
            WsFreeReader(r);
        }

        /* one prefix declared twice on an element */
        hr = open_reader(&r, dup, strlen(dup));
        hr = WsReadToStartElement(r, NULL, NULL, NULL, NULL);
        CHECK(hr == WS_E_INVALID_FORMAT, "duplicate xmlns prefix: %#lx", hr);
        WsFreeReader(r);

        /* the declaration of a closed element is out of scope, with or without input there is a difference */
        hr = open_reader(&r, nsdoc, strlen(nsdoc));
        hr = WsReadToStartElement(r, NULL, NULL, NULL, NULL);
        hr = WsReadStartElement(r, NULL);
        CHECK(hr == S_OK, "start element %#lx", hr);
        hr = WsReadEndElement(r, NULL);
        CHECK(hr == S_OK, "end element %#lx", hr);
        hr = WsGetNamespaceFromPrefix(r, &prefix, TRUE, &ns, NULL);
        CHECK(hr == WS_E_INVALID_FORMAT, "prefix of a closed element: %#lx", hr);
        WsFreeReader(r);
        WsCreateReader(NULL, 0, &r, NULL);
        hr = WsGetNamespaceFromPrefix(r, &prefix, TRUE, &ns, NULL);
        CHECK(hr == WS_E_INVALID_OPERATION, "prefix without input: %#lx", hr);

        /* no input at all: nothing to read a buffer from */
        hr = WsReadXmlBuffer(r, heap, &xmlbuf, NULL);
        CHECK(hr == E_FAIL, "xml buffer without input: %#lx", hr);
        WsFreeReader(r);

        /* a byte buffer is not an xml buffer, WsMoveReader needs the latter */
        hr = open_reader(&r, "<t/>", 4);
        hr = WsMoveReader(r, WS_MOVE_TO_EOF, NULL, NULL);
        CHECK(hr == WS_E_INVALID_OPERATION, "move on a byte buffer: %#lx", hr);
        WsFreeReader(r);
    }

    /* a buffer has no stream: the stream properties do not exist for it */
    hr = open_reader(&r, "<t/>", 4);
    CHECK(hr == S_OK, "set input %#lx", hr);
    v = 0;
    hr = WsGetReaderProperty(r, WS_XML_READER_PROPERTY_UTF8_TRIM_SIZE, &v, sizeof(v), NULL);
    CHECK(hr == E_INVALIDARG, "trim size on a buffer: %#lx", hr);
    hr = WsGetReaderProperty(r, WS_XML_READER_PROPERTY_STREAM_BUFFER_SIZE, &v, sizeof(v), NULL);
    CHECK(hr == E_INVALIDARG, "stream buffer size on a buffer: %#lx", hr);
    WsFreeReader(r);

    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
