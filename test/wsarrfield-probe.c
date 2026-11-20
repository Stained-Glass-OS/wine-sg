/* wsarrfield-probe: webservices WS_CHAR_ARRAY_TYPE, WS_UTF8_ARRAY_TYPE and WS_BYTE_ARRAY_TYPE struct fields (a
 * pointer field and a count field serialized as one value) written by WsWriteElement and read by WsReadElement
 * (Stained Glass OS wine-sg 8229; test/wsarrfield-gate.sh). They were "type not supported" (E_NOTIMPL).
 * Types declared here: the mingw-w64 headers have no webservices.h.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

typedef struct { ULONG length; BYTE *bytes; void *dictionary; ULONG id; } WS_XML_STRING;
typedef struct { ULONG length; BYTE *bytes; } WS_BYTES;
typedef struct { int encodingType; } WS_XML_READER_ENCODING;
typedef struct { WS_XML_READER_ENCODING encoding; int charSet; } WS_XML_READER_TEXT_ENCODING;
typedef struct { int inputType; } WS_XML_READER_INPUT;
typedef struct { WS_XML_READER_INPUT input; void *encodedData; ULONG encodedDataSize; } WS_XML_READER_BUFFER_INPUT;
typedef struct { int encodingType; } WS_XML_WRITER_ENCODING;
typedef struct { WS_XML_WRITER_ENCODING encoding; int charSet; } WS_XML_WRITER_TEXT_ENCODING;
typedef struct { int outputType; } WS_XML_WRITER_OUTPUT;
typedef struct { ULONG minCount, maxCount; } MINMAX;   /* WS_CHAR_ARRAY_DESCRIPTION and the others */
typedef struct {
    int mapping; WS_XML_STRING *localName, *ns; int type; void *typeDescription; ULONG offset, options;
    void *defaultValue; ULONG countOffset; WS_XML_STRING *itemLocalName, *itemNs; void *itemRange;
} WS_FIELD_DESCRIPTION;
typedef struct {
    ULONG size, alignment; WS_FIELD_DESCRIPTION **fields; ULONG fieldCount; WS_XML_STRING *typeLocalName, *typeNs;
    void *parentType; void **subTypes; ULONG subTypeCount, structOptions;
} WS_STRUCT_DESCRIPTION;
typedef struct { WS_XML_STRING *elementLocalName, *elementNs; int type; void *typeDescription; } WS_ELEMENT_DESCRIPTION;
typedef void WS_XML_READER, WS_XML_WRITER, WS_HEAP;
#define WS_ATTRIBUTE_FIELD_MAPPING 1
#define WS_ELEMENT_FIELD_MAPPING 2
#define WS_CHAR_ARRAY_TYPE 22
#define WS_UTF8_ARRAY_TYPE 23
#define WS_BYTE_ARRAY_TYPE 24
#define WS_STRUCT_TYPE 26
#define WS_XML_WRITER_PROPERTY_BYTES 9

struct rec { ULONG name_count; WCHAR *name; BYTE *id; ULONG data_count; ULONG id_count; BYTE *data; };

static HRESULT (WINAPI *pWsCreateHeap)(SIZE_T, SIZE_T, const void *, ULONG, WS_HEAP **, void *);
static HRESULT (WINAPI *pWsCreateReader)(void *, ULONG, WS_XML_READER **, void *);
static HRESULT (WINAPI *pWsSetInput)(WS_XML_READER *, const void *, const void *, void *, ULONG, void *);
static HRESULT (WINAPI *pWsReadElement)(WS_XML_READER *, const WS_ELEMENT_DESCRIPTION *, int, WS_HEAP *, void *, ULONG, void *);
static HRESULT (WINAPI *pWsCreateWriter)(void *, ULONG, WS_XML_WRITER **, void *);
static HRESULT (WINAPI *pWsSetOutput)(WS_XML_WRITER *, const void *, const void *, void *, ULONG, void *);
static HRESULT (WINAPI *pWsWriteElement)(WS_XML_WRITER *, const WS_ELEMENT_DESCRIPTION *, int, const void *, ULONG, void *);
static HRESULT (WINAPI *pWsGetWriterProperty)(WS_XML_WRITER *, int, void *, ULONG, void *);

static int failures;
static void check( int ok, const char *what, long got )
{
    printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}
#define XS(s) { sizeof(s) - 1, (BYTE *)s, NULL, 0 }

static HRESULT read_rec( const char *xml, WS_ELEMENT_DESCRIPTION *elem, WS_HEAP *heap, struct rec *out )
{
    WS_XML_READER_TEXT_ENCODING enc = { { 1 }, 0 };
    WS_XML_READER_BUFFER_INPUT in = { { 1 }, (void *)xml, strlen( xml ) };
    WS_XML_READER *reader = NULL;
    HRESULT hr = pWsCreateReader( NULL, 0, &reader, NULL );
    if (hr == S_OK) hr = pWsSetInput( reader, &enc, &in, NULL, 0, NULL );
    memset( out, 0, sizeof(*out) );
    if (hr == S_OK) hr = pWsReadElement( reader, elem, 1 /* WS_READ_REQUIRED_VALUE */, heap, out, sizeof(*out), NULL );
    return hr;
}

int main( void )
{
    HMODULE ws = LoadLibraryA( "webservices.dll" );
    WS_XML_STRING root = XS("rec"), ns = XS(""), name = XS("name"), id = XS("id"), data = XS("data");
    MINMAX limits = { 0, 3 };
    WS_FIELD_DESCRIPTION f_name = { WS_ELEMENT_FIELD_MAPPING, &name, &ns, WS_CHAR_ARRAY_TYPE, NULL,
                                    offsetof(struct rec, name), 0, NULL, offsetof(struct rec, name_count) };
    WS_FIELD_DESCRIPTION f_id = { WS_ATTRIBUTE_FIELD_MAPPING, &id, &ns, WS_UTF8_ARRAY_TYPE, NULL,
                                  offsetof(struct rec, id), 0, NULL, offsetof(struct rec, id_count) };
    WS_FIELD_DESCRIPTION f_data = { WS_ELEMENT_FIELD_MAPPING, &data, &ns, WS_BYTE_ARRAY_TYPE, NULL,
                                    offsetof(struct rec, data), 0, NULL, offsetof(struct rec, data_count) };
    WS_FIELD_DESCRIPTION *fields[] = { &f_id, &f_name, &f_data };
    WS_STRUCT_DESCRIPTION sd = { sizeof(struct rec), 8, fields, 3, NULL, NULL, NULL, NULL, 0, 0 };
    WS_ELEMENT_DESCRIPTION elem = { &root, &ns, WS_STRUCT_TYPE, &sd };
    WS_XML_WRITER_TEXT_ENCODING wenc = { { 1 }, 1 /* WS_CHARSET_UTF8 */ };
    WS_XML_WRITER_OUTPUT wout = { 1 };
    WCHAR hello[] = { 'H', 'e', 'l', 'l', 'o', '!', '!' };   /* only the count's 5 are the value */
    BYTE bin[] = { 1, 2, 3 };
    struct rec r = { 5, hello, (BYTE *)"x7", 3, 2, bin }, back;
    const char *want = "<rec id=\"x7\"><name>Hello</name><data>AQID</data></rec>";
    WS_XML_WRITER *writer = NULL;
    WS_HEAP *heap = NULL;
    WS_BYTES out = { 0 };
    HRESULT hr;

#define LOAD(f) p##f = (void *)GetProcAddress( ws, #f )
    LOAD(WsCreateHeap); LOAD(WsCreateReader); LOAD(WsSetInput); LOAD(WsReadElement);
    LOAD(WsCreateWriter); LOAD(WsSetOutput); LOAD(WsWriteElement); LOAD(WsGetWriterProperty);
    pWsCreateHeap( 1 << 16, 0, NULL, 0, &heap, NULL );

    hr = pWsCreateWriter( NULL, 0, &writer, NULL );
    if (hr == S_OK) hr = pWsSetOutput( writer, &wenc, &wout, NULL, 0, NULL );
    if (hr == S_OK) hr = pWsWriteElement( writer, &elem, 1 /* WS_WRITE_REQUIRED_VALUE */, &r, sizeof(r), NULL );
    check( hr == S_OK, "WsWriteElement of a struct with char, UTF-8 and byte array fields", hr );
    pWsGetWriterProperty( writer, WS_XML_WRITER_PROPERTY_BYTES, &out, sizeof(out), NULL );
    printf( "      %.*s\n", (int)out.length, out.bytes );
    check( out.length == strlen( want ) && !memcmp( out.bytes, want, out.length ),
           "the count field gives the length: char array as text, UTF-8 array as text, byte array as base64", out.length );

    hr = read_rec( "<rec id=\"ab\"><name>Hello</name><data>AQID</data></rec>", &elem, heap, &back );
    check( hr == S_OK, "WsReadElement of the struct", hr );
    check( back.name_count == 5 && back.name && !memcmp( back.name, L"Hello", 10 ), "the char array and its count", back.name_count );
    check( back.id_count == 2 && back.id && !memcmp( back.id, "ab", 2 ), "the UTF-8 array and its count", back.id_count );
    check( back.data_count == 3 && back.data && !memcmp( back.data, bin, 3 ), "the byte array and its count", back.data_count );

    f_name.typeDescription = &limits;   /* a WS_CHAR_ARRAY_DESCRIPTION: at most 3 characters */
    hr = read_rec( "<rec id=\"ab\"><name>Hello</name><data>AQID</data></rec>", &elem, heap, &back );
    check( hr != S_OK, "a WS_CHAR_ARRAY_DESCRIPTION's maximum is kept", hr );
    hr = read_rec( "<rec id=\"ab\"><name>Hi</name><data>AQID</data></rec>", &elem, heap, &back );
    check( hr == S_OK && back.name_count == 2, "within it the value is read", hr );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
