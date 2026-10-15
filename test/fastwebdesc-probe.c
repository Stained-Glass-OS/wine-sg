#include <windows.h>
#include <webservices.h>
#include <stdio.h>
#include <string.h>
#define P(...) printf(__VA_ARGS__)
static int bad;
#define EXPECT(c) do { if (!(c)) { bad++; printf("MISMATCH line %d\n", __LINE__); } } while (0)
#define BADFMT ((HRESULT)0x803d0000)

static WS_HEAP *heap;
static WS_XML_READER *reader;
static WS_XML_WRITER *writer;

static HRESULT rd(const char *xml, WS_TYPE type, const void *desc, WS_READ_OPTION opt, void *val, ULONG size)
{
    WS_XML_READER_TEXT_ENCODING enc = {{WS_XML_READER_ENCODING_TYPE_TEXT}, WS_CHARSET_UTF8};
    WS_XML_READER_BUFFER_INPUT in = {{WS_XML_READER_INPUT_TYPE_BUFFER}, (void *)xml, (ULONG)strlen(xml)};
    HRESULT hr = WsSetInput(reader, &enc.encoding, &in.input, NULL, 0, NULL);
    if (hr != S_OK) return hr;
    hr = WsReadToStartElement(reader, NULL, NULL, NULL, NULL);
    if (hr != S_OK) return hr;
    hr = WsReadStartElement(reader, NULL);
    if (hr != S_OK) return hr;
    return WsReadType(reader, WS_ELEMENT_CONTENT_TYPE_MAPPING, type, desc, opt, heap, val, size, NULL);
}

static HRESULT wr(WS_TYPE type, const void *desc, WS_WRITE_OPTION opt, const void *val, ULONG size, char *out, ULONG outsize)
{
    WS_XML_WRITER_TEXT_ENCODING enc = {{WS_XML_WRITER_ENCODING_TYPE_TEXT}, WS_CHARSET_UTF8};
    WS_XML_WRITER_BUFFER_OUTPUT o = {{WS_XML_WRITER_OUTPUT_TYPE_BUFFER}};
    WS_XML_STRING name = {1, (BYTE *)"t"}, ns = {0, NULL};
    WS_BYTES bytes;
    HRESULT hr = WsSetOutput(writer, &enc.encoding, &o.output, NULL, 0, NULL);
    if (hr != S_OK) return hr;
    hr = WsWriteStartElement(writer, NULL, &name, &ns, NULL);
    if (hr != S_OK) return hr;
    hr = WsWriteType(writer, WS_ELEMENT_CONTENT_TYPE_MAPPING, type, desc, opt, val, size, NULL);
    if (hr != S_OK) return hr;
    hr = WsWriteEndElement(writer, NULL);
    if (hr != S_OK) return hr;
    hr = WsGetWriterProperty(writer, WS_XML_WRITER_PROPERTY_BYTES, &bytes, sizeof(bytes), NULL);
    if (hr != S_OK) return hr;
    if (bytes.length >= outsize) bytes.length = outsize - 1;
    memcpy(out, bytes.bytes, bytes.length);
    out[bytes.length] = 0;
    return S_OK;
}

int main(void)
{
    HRESULT hr;
    char out[128];
    WS_FLOAT_DESCRIPTION fd = {-1.5f, 2.5f};
    WS_DOUBLE_DESCRIPTION dd = {0.5, 100.0};
    WS_WSZ_DESCRIPTION wd = {2, 4};
    WS_STRING_DESCRIPTION sd = {2, 4};
    WS_BYTES_DESCRIPTION bd = {2, 4};
    WS_XML_STRING_DESCRIPTION xd = {2, 4};
    WS_INT32_DESCRIPTION id = {-10, 10};
    WS_DATETIME_DESCRIPTION td = {{1000, WS_DATETIME_FORMAT_UTC}, {2000, WS_DATETIME_FORMAT_UTC}};
    float f = 0; double d = 0; WCHAR *w = NULL; WS_STRING s = {0}; WS_BYTES b = {0}; WS_XML_STRING xs = {0};
    int i32;
    WS_STRING ws3 = {3, (WCHAR *)L"abc"}, ws1 = {1, (WCHAR *)L"a"}, ws5 = {5, (WCHAR *)L"abcde"};
    WS_BYTES by3 = {3, (BYTE *)"xyz"}, by1 = {1, (BYTE *)"x"};
    WS_XML_STRING x3 = {3, (BYTE *)"abc"}, x5 = {5, (BYTE *)"abcde"};
    WS_DATETIME dt;
    setvbuf(stdout, NULL, _IONBF, 0);
    WsCreateHeap(1 << 16, 0, NULL, 0, &heap, NULL);
    WsCreateReader(NULL, 0, &reader, NULL);
    WsCreateWriter(NULL, 0, &writer, NULL);

    hr = rd("<t>1.5</t>", WS_FLOAT_TYPE, &fd, WS_READ_REQUIRED_VALUE, &f, sizeof(f)); P("RF_IN %08lx %g\n", hr, f); EXPECT(hr == S_OK && f == 1.5f);
    hr = rd("<t>3</t>", WS_FLOAT_TYPE, &fd, WS_READ_REQUIRED_VALUE, &f, sizeof(f)); P("RF_HI %08lx\n", hr); EXPECT(hr == BADFMT);
    hr = rd("<t>-2</t>", WS_FLOAT_TYPE, &fd, WS_READ_REQUIRED_VALUE, &f, sizeof(f)); P("RF_LO %08lx\n", hr); EXPECT(hr == BADFMT);
    hr = rd("<t>2.5</t>", WS_DOUBLE_TYPE, &dd, WS_READ_REQUIRED_VALUE, &d, sizeof(d)); P("RD_IN %08lx %g\n", hr, d); EXPECT(hr == S_OK && d == 2.5);
    hr = rd("<t>0.25</t>", WS_DOUBLE_TYPE, &dd, WS_READ_REQUIRED_VALUE, &d, sizeof(d)); P("RD_LO %08lx\n", hr); EXPECT(hr == BADFMT);
    hr = rd("<t>abc</t>", WS_WSZ_TYPE, &wd, WS_READ_REQUIRED_POINTER, &w, sizeof(w)); P("RW_IN %08lx %ls\n", hr, w ? w : L"?"); EXPECT(hr == S_OK && w && !wcscmp(w, L"abc"));
    hr = rd("<t>a</t>", WS_WSZ_TYPE, &wd, WS_READ_REQUIRED_POINTER, &w, sizeof(w)); P("RW_LO %08lx\n", hr); EXPECT(hr == BADFMT);
    hr = rd("<t>abcde</t>", WS_WSZ_TYPE, &wd, WS_READ_REQUIRED_POINTER, &w, sizeof(w)); P("RW_HI %08lx\n", hr); EXPECT(hr == BADFMT);
    hr = rd("<t>abcd</t>", WS_STRING_TYPE, &sd, WS_READ_REQUIRED_VALUE, &s, sizeof(s)); P("RS_EDGE %08lx %lu\n", hr, s.length); EXPECT(hr == S_OK && s.length == 4);
    hr = rd("<t>abcde</t>", WS_STRING_TYPE, &sd, WS_READ_REQUIRED_VALUE, &s, sizeof(s)); P("RS_HI %08lx\n", hr); EXPECT(hr == BADFMT);
    hr = rd("<t>YWJj</t>", WS_BYTES_TYPE, &bd, WS_READ_REQUIRED_VALUE, &b, sizeof(b)); P("RB_IN %08lx %lu\n", hr, b.length); EXPECT(hr == S_OK && b.length == 3);
    hr = rd("<t>YQ==</t>", WS_BYTES_TYPE, &bd, WS_READ_REQUIRED_VALUE, &b, sizeof(b)); P("RB_LO %08lx\n", hr); EXPECT(hr == BADFMT);
    hr = rd("<t>abc</t>", WS_XML_STRING_TYPE, &xd, WS_READ_REQUIRED_VALUE, &xs, sizeof(xs)); P("RX_IN %08lx %lu\n", hr, xs.length); EXPECT(hr == S_OK && xs.length == 3);
    hr = rd("<t>abcde</t>", WS_XML_STRING_TYPE, &xd, WS_READ_REQUIRED_VALUE, &xs, sizeof(xs)); P("RX_HI %08lx\n", hr); EXPECT(hr == BADFMT);
    hr = rd("<t>abcde</t>", WS_XML_STRING_TYPE, NULL, WS_READ_REQUIRED_VALUE, &xs, sizeof(xs)); P("RX_NODESC %08lx\n", hr); EXPECT(hr == S_OK && xs.length == 5);

    i32 = 5; hr = wr(WS_INT32_TYPE, &id, WS_WRITE_REQUIRED_VALUE, &i32, sizeof(i32), out, sizeof(out)); P("WI_IN %08lx %s\n", hr, out); EXPECT(hr == S_OK && !strcmp(out, "<t>5</t>"));
    i32 = 11; hr = wr(WS_INT32_TYPE, &id, WS_WRITE_REQUIRED_VALUE, &i32, sizeof(i32), out, sizeof(out)); P("WI_HI %08lx\n", hr); EXPECT(hr == BADFMT);
    i32 = -11; hr = wr(WS_INT32_TYPE, &id, WS_WRITE_REQUIRED_VALUE, &i32, sizeof(i32), out, sizeof(out)); P("WI_LO %08lx\n", hr); EXPECT(hr == BADFMT);
    i32 = 99; hr = wr(WS_INT32_TYPE, NULL, WS_WRITE_REQUIRED_VALUE, &i32, sizeof(i32), out, sizeof(out)); P("WI_NODESC %08lx %s\n", hr, out); EXPECT(hr == S_OK && !strcmp(out, "<t>99</t>"));
    d = 0.25; hr = wr(WS_DOUBLE_TYPE, &dd, WS_WRITE_REQUIRED_VALUE, &d, sizeof(d), out, sizeof(out)); P("WD_LO %08lx\n", hr); EXPECT(hr == BADFMT);
    d = 50; hr = wr(WS_DOUBLE_TYPE, &dd, WS_WRITE_REQUIRED_VALUE, &d, sizeof(d), out, sizeof(out)); P("WD_IN %08lx %s\n", hr, out); EXPECT(hr == S_OK && !strcmp(out, "<t>50</t>"));
    hr = wr(WS_STRING_TYPE, &sd, WS_WRITE_REQUIRED_VALUE, &ws3, sizeof(ws3), out, sizeof(out)); P("WS_IN %08lx %s\n", hr, out); EXPECT(hr == S_OK && !strcmp(out, "<t>abc</t>"));
    hr = wr(WS_STRING_TYPE, &sd, WS_WRITE_REQUIRED_VALUE, &ws1, sizeof(ws1), out, sizeof(out)); P("WS_LO %08lx\n", hr); EXPECT(hr == BADFMT);
    hr = wr(WS_STRING_TYPE, &sd, WS_WRITE_REQUIRED_VALUE, &ws5, sizeof(ws5), out, sizeof(out)); P("WS_HI %08lx\n", hr); EXPECT(hr == BADFMT);
    { const WCHAR *w3 = L"abc", *w5 = L"abcde";
      hr = wr(WS_WSZ_TYPE, &wd, WS_WRITE_REQUIRED_POINTER, &w3, sizeof(w3), out, sizeof(out)); P("WW_IN %08lx %s\n", hr, out); EXPECT(hr == S_OK && !strcmp(out, "<t>abc</t>"));
      hr = wr(WS_WSZ_TYPE, &wd, WS_WRITE_REQUIRED_POINTER, &w5, sizeof(w5), out, sizeof(out)); P("WW_HI %08lx\n", hr); EXPECT(hr == BADFMT); }
    hr = wr(WS_BYTES_TYPE, &bd, WS_WRITE_REQUIRED_VALUE, &by3, sizeof(by3), out, sizeof(out)); P("WB_IN %08lx %s\n", hr, out); EXPECT(hr == S_OK && !strcmp(out, "<t>eHl6</t>"));
    hr = wr(WS_BYTES_TYPE, &bd, WS_WRITE_REQUIRED_VALUE, &by1, sizeof(by1), out, sizeof(out)); P("WB_LO %08lx\n", hr); EXPECT(hr == BADFMT);
    hr = wr(WS_XML_STRING_TYPE, &xd, WS_WRITE_REQUIRED_VALUE, &x3, sizeof(x3), out, sizeof(out)); P("WX_IN %08lx %s\n", hr, out); EXPECT(hr == S_OK && !strcmp(out, "<t>abc</t>"));
    hr = wr(WS_XML_STRING_TYPE, &xd, WS_WRITE_REQUIRED_VALUE, &x5, sizeof(x5), out, sizeof(out)); P("WX_HI %08lx\n", hr); EXPECT(hr == BADFMT);
    dt.ticks = 1500; dt.format = WS_DATETIME_FORMAT_UTC;
    hr = wr(WS_DATETIME_TYPE, &td, WS_WRITE_REQUIRED_VALUE, &dt, sizeof(dt), out, sizeof(out)); P("WT_IN %08lx\n", hr); EXPECT(hr == S_OK);
    dt.ticks = 3000;
    hr = wr(WS_DATETIME_TYPE, &td, WS_WRITE_REQUIRED_VALUE, &dt, sizeof(dt), out, sizeof(out)); P("WT_HI %08lx\n", hr); EXPECT(hr == BADFMT);
    WsFreeReader(reader); WsFreeWriter(writer); WsFreeHeap(heap);
    P("%s\n", bad ? "RESULT: FAIL" : "RESULT: PASS");
    P("DONE\n");
    return 0;
}
