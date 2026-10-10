/* webservices' writer behaviours that Wine's own tests record from real Windows
 * (patches/sg/2425), run by test/wswriter-gate.sh: how a double is written,
 * conflicting namespace declarations, the output the writer must have, and
 * mixing text types in an attribute. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* webservices.h is not in the MinGW headers: what is used here, declared by hand */
typedef struct { ULONG length; BYTE *bytes; void *dictionary; ULONG id; } WS_XML_STRING;
typedef struct { int textType; } WS_XML_TEXT;
typedef struct { WS_XML_TEXT text; WS_XML_STRING value; } WS_XML_UTF8_TEXT;
typedef struct { WS_XML_TEXT text; BYTE *bytes; ULONG byteCount; } WS_XML_UTF16_TEXT;
typedef struct { WS_XML_TEXT text; double value; } WS_XML_DOUBLE_TEXT;
typedef struct { ULONG length; BYTE *bytes; } WS_BYTES;
typedef struct { void *buffer; void *node; } WS_XML_NODE_POSITION;
typedef struct { int encodingType; } WS_XML_WRITER_ENCODING;
typedef struct { WS_XML_WRITER_ENCODING encoding; int charSet; } WS_XML_WRITER_TEXT_ENCODING;
typedef struct { int outputType; } WS_XML_WRITER_OUTPUT;
typedef struct { WS_XML_WRITER_OUTPUT output; } WS_XML_WRITER_BUFFER_OUTPUT;
typedef void WS_XML_WRITER, WS_HEAP, WS_XML_BUFFER;
#define WS_XML_TEXT_TYPE_UTF8 1
#define WS_XML_TEXT_TYPE_UTF16 2
#define WS_XML_TEXT_TYPE_DOUBLE 9
#define WS_XML_WRITER_PROPERTY_BYTES 9
#define WS_MOVE_TO_EOF 10
#define WS_E_INVALID_OPERATION ((HRESULT)0x803d0003)
#define WS_E_INVALID_FORMAT ((HRESULT)0x803d0000)

static HRESULT (WINAPI *WsCreateWriter)(void *, ULONG, WS_XML_WRITER **, void *);
static void (WINAPI *WsFreeWriter)(WS_XML_WRITER *);
static HRESULT (WINAPI *WsSetOutput)(WS_XML_WRITER *, WS_XML_WRITER_ENCODING *, WS_XML_WRITER_OUTPUT *, void *, ULONG, void *);
static HRESULT (WINAPI *WsSetOutputToBuffer)(WS_XML_WRITER *, WS_XML_BUFFER *, void *, ULONG, void *);
static HRESULT (WINAPI *WsGetWriterProperty)(WS_XML_WRITER *, int, void *, ULONG, void *);
static HRESULT (WINAPI *WsWriteStartElement)(WS_XML_WRITER *, const WS_XML_STRING *, const WS_XML_STRING *, const WS_XML_STRING *, void *);
static HRESULT (WINAPI *WsWriteEndElement)(WS_XML_WRITER *, void *);
static HRESULT (WINAPI *WsWriteStartAttribute)(WS_XML_WRITER *, const WS_XML_STRING *, const WS_XML_STRING *, const WS_XML_STRING *, BOOL, void *);
static HRESULT (WINAPI *WsWriteEndAttribute)(WS_XML_WRITER *, void *);
static HRESULT (WINAPI *WsWriteText)(WS_XML_WRITER *, const WS_XML_TEXT *, void *);
static HRESULT (WINAPI *WsWriteXmlnsAttribute)(WS_XML_WRITER *, const WS_XML_STRING *, const WS_XML_STRING *, BOOL, void *);
static HRESULT (WINAPI *WsWriteNode)(WS_XML_WRITER *, const void *, void *);
static HRESULT (WINAPI *WsMoveWriter)(WS_XML_WRITER *, int, BOOL *, void *);
static HRESULT (WINAPI *WsGetWriterPosition)(WS_XML_WRITER *, WS_XML_NODE_POSITION *, void *);
static HRESULT (WINAPI *WsCreateHeap)(SIZE_T, SIZE_T, void *, ULONG, WS_HEAP **, void *);
static void (WINAPI *WsFreeHeap)(WS_HEAP *);
static HRESULT (WINAPI *WsCreateXmlBuffer)(WS_HEAP *, void *, ULONG, WS_XML_BUFFER **, void *);

static void load(void)
{
    HMODULE m = LoadLibraryA("webservices.dll");
#define L(f) f = (void *)GetProcAddress(m, #f)
    L(WsCreateWriter); L(WsFreeWriter); L(WsSetOutput); L(WsSetOutputToBuffer); L(WsGetWriterProperty);
    L(WsWriteStartElement); L(WsWriteEndElement); L(WsWriteStartAttribute); L(WsWriteEndAttribute); L(WsWriteText);
    L(WsWriteXmlnsAttribute); L(WsWriteNode); L(WsMoveWriter); L(WsGetWriterPosition); L(WsCreateHeap); L(WsFreeHeap);
    L(WsCreateXmlBuffer);
#undef L
}

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static HRESULT set_output(WS_XML_WRITER *w)
{
    WS_XML_WRITER_TEXT_ENCODING enc = {{1}, 1};
    WS_XML_WRITER_BUFFER_OUTPUT out = {{1}};
    return WsSetOutput(w, &enc.encoding, &out.output, NULL, 0, NULL);
}

static char *output(WS_XML_WRITER *w, ULONG *len)
{
    WS_BYTES bytes;
    memset(&bytes, 0, sizeof(bytes));
    if (WsGetWriterProperty(w, WS_XML_WRITER_PROPERTY_BYTES, &bytes, sizeof(bytes), NULL)) return NULL;
    *len = bytes.length;
    return (char *)bytes.bytes;
}

static int output_is(WS_XML_WRITER *w, const char *expect)
{
    ULONG len;
    char *out = output(w, &len);
    return out && len == strlen(expect) && !memcmp(out, expect, len);
}

int main(void)
{
    WS_XML_STRING t = {1, (BYTE *)"t"}, ns = {0, NULL}, attrname = {4, (BYTE *)"attr"};
    WS_XML_STRING prefix = {6, (BYTE *)"prefix"}, prefix2 = {7, (BYTE *)"prefix2"}, nsa = {2, (BYTE *)"ns"}, nsb = {3, (BYTE *)"ns2"};
    WS_XML_STRING xmlns6 = {6, (BYTE *)"xmlns"};  /* with its terminating zero, as Wine's test has it */
    WS_XML_WRITER *w;
    WS_XML_DOUBLE_TEXT dt;
    WS_XML_UTF8_TEXT u8;
    WS_XML_UTF16_TEXT u16;
    WS_HEAP *heap;
    WS_XML_BUFFER *buffer;
    HRESULT hr;
    unsigned i;
    char msg[160];
    static const WCHAR wtext[] = L"test";
    static const struct { double v; const char *s; } doubles[] =
    {
        { 0.0, "<t>0</t>" }, { 1.0, "<t>1</t>" }, { -1.0, "<t>-1</t>" }, { 1.0000000000000001, "<t>1</t>" },
        { 1.0000000000000002, "<t>1.0000000000000002</t>" }, { 1.0000000000000003, "<t>1.0000000000000002</t>" },
        { 1.0000000000000004, "<t>1.0000000000000004</t>" }, { 100000000000000, "<t>100000000000000</t>" },
        { 1000000000000000, "<t>1E+15</t>" }, { 0.1, "<t>0.1</t>" }, { 0.01, "<t>1E-2</t>" }, { -0.1, "<t>-0.1</t>" },
        { -0.01, "<t>-1E-2</t>" }, { 1.7976931348623158e308, "<t>1.7976931348623157E+308</t>" },
        { -1.7976931348623158e308, "<t>-1.7976931348623157E+308</t>" },
        { 0.15, "<t>0.15</t>" }, { 123.456, "<t>123.456</t>" }, { 1234567.5, "<t>1234567.5</t>" }, { 2.5e-5, "<t>2.5E-5</t>" },
        { 1.5e20, "<t>1.5E+20</t>" }, { 0.5, "<t>0.5</t>" }, { 12.0, "<t>12</t>" },
    };

    load();
    check(!WsCreateWriter(NULL, 0, &w, NULL), "a writer");

    /* doubles */
    dt.text.textType = WS_XML_TEXT_TYPE_DOUBLE;
    for (i = 0; i < sizeof(doubles) / sizeof(doubles[0]); i++)
    {
        set_output(w);
        WsWriteStartElement(w, NULL, &t, &ns, NULL);
        dt.value = doubles[i].v;
        WsWriteText(w, &dt.text, NULL);
        WsWriteEndElement(w, NULL);
        sprintf(msg, "%.17g is written as %s", doubles[i].v, doubles[i].s);
        check(output_is(w, doubles[i].s), msg);
    }
    set_output(w);
    WsWriteStartElement(w, NULL, &t, &ns, NULL);
    dt.value = NAN; WsWriteText(w, &dt.text, NULL); WsWriteEndElement(w, NULL);
    check(output_is(w, "<t>NaN</t>"), "NaN is NaN");
    set_output(w);
    WsWriteStartElement(w, NULL, &t, &ns, NULL);
    dt.value = INFINITY; WsWriteText(w, &dt.text, NULL); WsWriteEndElement(w, NULL);
    check(output_is(w, "<t>INF</t>"), "infinity is INF");

    /* output the writer must have */
    WsFreeWriter(w);
    WsCreateWriter(NULL, 0, &w, NULL);
    check(WsWriteNode(w, NULL, NULL) == WS_E_INVALID_OPERATION, "WsWriteNode with no output: WS_E_INVALID_OPERATION, before the node is looked at");
    set_output(w);
    WsCreateHeap(1 << 16, 0, NULL, 0, &heap, NULL);
    check(WsMoveWriter(w, WS_MOVE_TO_EOF, NULL, NULL) == WS_E_INVALID_OPERATION, "WsMoveWriter on a writer that does not write to an XML buffer: WS_E_INVALID_OPERATION");
    {
        WS_XML_NODE_POSITION pos;
        check(WsGetWriterPosition(w, &pos, NULL) == WS_E_INVALID_OPERATION, "WsGetWriterPosition too");
    }
    WsCreateXmlBuffer(heap, NULL, 0, &buffer, NULL);
    WsSetOutputToBuffer(w, buffer, NULL, 0, NULL);
    {
        WS_XML_NODE_POSITION pos;
        WS_BYTES bytes;
        check(WsGetWriterPosition(w, &pos, NULL) == S_OK, "with one it works");
        check(WsGetWriterProperty(w, WS_XML_WRITER_PROPERTY_BYTES, &bytes, sizeof(bytes), NULL) == E_INVALIDARG, "but the BYTES property is not there for a buffer the writer was given: E_INVALIDARG");
    }

    /* namespace declarations */
    #define XMLNS_CASE(label, body, want) do { \
        hr = WsCreateXmlBuffer(heap, NULL, 0, &buffer, NULL); WsSetOutputToBuffer(w, buffer, NULL, 0, NULL); \
        WsWriteStartElement(w, &prefix, &t, &nsa, NULL); body; hr = WsWriteEndElement(w, NULL); \
        check(hr == (want), label); } while (0)
    XMLNS_CASE("the same prefix declared twice for one namespace is fine",
               { WsWriteXmlnsAttribute(w, &prefix2, &nsa, FALSE, NULL); WsWriteXmlnsAttribute(w, &prefix2, &nsa, FALSE, NULL); }, S_OK);
    XMLNS_CASE("one prefix declared for two namespaces: WS_E_INVALID_FORMAT",
               { WsWriteXmlnsAttribute(w, &prefix2, &nsa, FALSE, NULL); WsWriteXmlnsAttribute(w, &prefix2, &nsb, FALSE, NULL); }, WS_E_INVALID_FORMAT);
    XMLNS_CASE("the element's own prefix declared for another namespace: WS_E_INVALID_FORMAT",
               { WsWriteXmlnsAttribute(w, &prefix, &nsb, TRUE, NULL); }, WS_E_INVALID_FORMAT);
    XMLNS_CASE("the element's own prefix declared for its namespace is fine",
               { WsWriteXmlnsAttribute(w, &prefix, &nsa, TRUE, NULL); }, S_OK);
    XMLNS_CASE("another prefix for another namespace is fine",
               { WsWriteXmlnsAttribute(w, &prefix2, &nsb, TRUE, NULL); }, S_OK);
    XMLNS_CASE("an attribute written with the xmlns prefix: WS_E_INVALID_FORMAT",
               { WsWriteStartAttribute(w, &xmlns6, &prefix2, &nsb, TRUE, NULL); WsWriteEndAttribute(w, NULL); }, WS_E_INVALID_FORMAT);

    /* text types in an attribute */
    WsFreeWriter(w);
    WsCreateWriter(NULL, 0, &w, NULL);
    u8.text.textType = WS_XML_TEXT_TYPE_UTF8; u8.value.bytes = (BYTE *)"test"; u8.value.length = 4;
    u16.text.textType = WS_XML_TEXT_TYPE_UTF16; u16.bytes = (BYTE *)wtext; u16.byteCount = 8;
    set_output(w);
    WsWriteStartElement(w, NULL, &t, &ns, NULL);
    WsWriteStartAttribute(w, NULL, &attrname, &ns, FALSE, NULL);
    check(WsWriteText(w, &u8.text, NULL) == S_OK, "text written to an attribute");
    check(WsWriteText(w, &u8.text, NULL) == S_OK, "more of the same type adds to it");
    check(WsWriteText(w, &u16.text, NULL) == WS_E_INVALID_OPERATION, "text of another type: WS_E_INVALID_OPERATION");
    WsWriteEndAttribute(w, NULL);
    WsWriteStartAttribute(w, NULL, &attrname, &ns, FALSE, NULL);
    check(WsWriteText(w, &u16.text, NULL) == S_OK, "a new attribute takes the other type again");
    WsWriteEndAttribute(w, NULL);
    WsWriteEndElement(w, NULL);
    check(output_is(w, "<t attr=\"testtest\" attr=\"test\"/>"), "and the output is both");

    WsFreeWriter(w);
    WsFreeHeap(heap);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
