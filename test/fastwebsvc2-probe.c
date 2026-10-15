#include <windows.h>
#include <webservices.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef WS_E_INVALID_FORMAT
#define WS_E_INVALID_FORMAT ((HRESULT)0x803d0000)
#define WS_E_NUMERIC_OVERFLOW ((HRESULT)0x803d0002)
#define WS_E_INVALID_OPERATION ((HRESULT)0x803d0003)
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif
static int bad, total;
#define EXPECT(c) do { total++; if (!(c)) { bad++; printf("MISMATCH line %d: %s\n", __LINE__, #c); } } while (0)
#define EXPECT_HR(want, expr) do { HRESULT hr_ = (expr); total++; if (hr_ != (want)) { bad++; \
    printf("MISMATCH line %d: %s = %#lx, want %#lx\n", __LINE__, #expr, (unsigned long)hr_, (unsigned long)(want)); } } while (0)

#define XS(s) { sizeof(s) - 1, (BYTE *)(s) }
static WS_XML_STRING empty_ns = XS("");
static WS_XML_STRING el_t = XS("t");
static WS_XML_STRING soap11 = XS("http://schemas.xmlsoap.org/soap/envelope/");
static WS_XML_STRING soap12 = XS("http://www.w3.org/2003/05/soap-envelope");
static WS_XML_STRING pfx_s = XS("s");

static WS_HEAP *heap;
static WS_XML_WRITER *w;
static WS_XML_READER *r;
static WS_XML_BUFFER *wbuf;

static void w_start(int binary)
{
    WS_XML_WRITER_TEXT_ENCODING te;
    WS_XML_WRITER_BINARY_ENCODING be;
    WS_XML_WRITER_BUFFER_OUTPUT out;
    if (w) WsFreeWriter(w);
    WsCreateWriter(NULL, 0, &w, NULL);
    memset(&out, 0, sizeof(out)); out.output.outputType = WS_XML_WRITER_OUTPUT_TYPE_BUFFER;
    if (binary) { memset(&be, 0, sizeof(be)); be.encoding.encodingType = WS_XML_WRITER_ENCODING_TYPE_BINARY;
        WsSetOutput(w, &be.encoding, &out.output, NULL, 0, NULL); }
    else { memset(&te, 0, sizeof(te)); te.encoding.encodingType = WS_XML_WRITER_ENCODING_TYPE_TEXT; te.charSet = WS_CHARSET_UTF8;
        WsSetOutput(w, &te.encoding, &out.output, NULL, 0, NULL); }
}
static void w_get(void **p, ULONG *len)
{
    WS_BYTES b;
    WsFlushWriter(w, 0, NULL, NULL);
    WsGetWriterProperty(w, WS_XML_WRITER_PROPERTY_BYTES, &b, sizeof(b), NULL);
    *p = b.bytes; *len = b.length;
}
static void r_start(int binary, void *data, ULONG len)
{
    WS_XML_READER_TEXT_ENCODING te;
    WS_XML_READER_BINARY_ENCODING be;
    WS_XML_READER_BUFFER_INPUT in;
    if (r) WsFreeReader(r);
    WsCreateReader(NULL, 0, &r, NULL);
    memset(&in, 0, sizeof(in)); in.input.inputType = WS_XML_READER_INPUT_TYPE_BUFFER;
    in.encodedData = data; in.encodedDataSize = len;
    if (binary) { memset(&be, 0, sizeof(be)); be.encoding.encodingType = WS_XML_READER_ENCODING_TYPE_BINARY;
        WsSetInput(r, &be.encoding, &in.input, NULL, 0, NULL); }
    else { memset(&te, 0, sizeof(te)); te.encoding.encodingType = WS_XML_READER_ENCODING_TYPE_TEXT; te.charSet = WS_CHARSET_UTF8;
        WsSetInput(r, &te.encoding, &in.input, NULL, 0, NULL); }
}

/* write <t>value</t> with a type, binary or text */
static void put(int binary, WS_TYPE type, const void *desc, const void *val, ULONG size)
{
    w_start(binary);
    WsWriteStartElement(w, NULL, &el_t, &empty_ns, NULL);
    EXPECT_HR(S_OK, WsWriteType(w, WS_ELEMENT_CONTENT_TYPE_MAPPING, type, desc, WS_WRITE_REQUIRED_VALUE, val, size, NULL));
    WsWriteEndElement(w, NULL);
}
static HRESULT get(int binary, WS_TYPE type, const void *desc, void *out, ULONG size)
{
    void *p; ULONG len; HRESULT hr;
    w_get(&p, &len);
    r_start(binary, p, len);
    hr = WsReadToStartElement(r, &el_t, &empty_ns, NULL, NULL);
    if (hr == S_OK) hr = WsReadStartElement(r, NULL);
    if (hr == S_OK) hr = WsReadType(r, WS_ELEMENT_CONTENT_TYPE_MAPPING, type, desc, WS_READ_REQUIRED_VALUE, heap, out, size, NULL);
    return hr;
}
static char *w_text(void)
{
    static char buf[2048]; void *p; ULONG len;
    w_get(&p, &len);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, p, len); buf[len] = 0;
    return buf;
}

static void test_error(void)
{
    WS_ERROR *err; WS_HEAP *h; WS_XML_BUFFER *b; ULONG n = 0, code = 0; WS_STRING s;
    WS_XML_BUFFER_PROPERTY prop = { 1, NULL, 0 };
    WsCreateError(NULL, 0, &err);
    WsCreateHeap(1 << 16, 0, NULL, 0, &h, NULL);
    EXPECT_HR(E_INVALIDARG, WsCreateXmlBuffer(h, &prop, 1, &b, err));
    WsGetErrorProperty(err, WS_ERROR_PROPERTY_STRING_COUNT, &n, sizeof(n));
    EXPECT(n == 1);
    WsGetErrorProperty(err, WS_ERROR_PROPERTY_ORIGINAL_ERROR_CODE, &code, sizeof(code));
    EXPECT(code == (ULONG)E_INVALIDARG);
    EXPECT_HR(S_OK, WsGetErrorString(err, 0, &s));
    EXPECT(s.length > 0);
    /* success leaves the error object alone */
    EXPECT_HR(S_OK, WsCreateXmlBuffer(h, NULL, 0, &b, err));
    WsGetErrorProperty(err, WS_ERROR_PROPERTY_STRING_COUNT, &n, sizeof(n));
    EXPECT(n == 1);
    /* another API: WsMoveReader with a bad value fills the error too */
    EXPECT_HR(E_INVALIDARG, WsCreateXmlBuffer(h, &prop, 1, &b, err));
    WsGetErrorProperty(err, WS_ERROR_PROPERTY_STRING_COUNT, &n, sizeof(n));
    EXPECT(n >= 2);
    WsFreeError(err); WsFreeHeap(h);
}

static void test_conv(void)
{
    INT32 v = 100; INT8 i8 = 0; UINT16 u16 = 0; INT64 i64 = 0; double d = 0; float f = 0; WS_STRING str;
    put(1, WS_INT32_TYPE, NULL, &v, sizeof(v));
    EXPECT_HR(S_OK, get(1, WS_INT8_TYPE, NULL, &i8, sizeof(i8))); EXPECT(i8 == 100);
    put(1, WS_INT32_TYPE, NULL, &v, sizeof(v));
    EXPECT_HR(S_OK, get(1, WS_UINT16_TYPE, NULL, &u16, sizeof(u16))); EXPECT(u16 == 100);
    put(1, WS_INT32_TYPE, NULL, &v, sizeof(v));
    EXPECT_HR(S_OK, get(1, WS_INT64_TYPE, NULL, &i64, sizeof(i64))); EXPECT(i64 == 100);
    put(1, WS_INT32_TYPE, NULL, &v, sizeof(v));
    EXPECT_HR(S_OK, get(1, WS_DOUBLE_TYPE, NULL, &d, sizeof(d))); EXPECT(d == 100.0);
    put(1, WS_INT32_TYPE, NULL, &v, sizeof(v));
    EXPECT_HR(S_OK, get(1, WS_FLOAT_TYPE, NULL, &f, sizeof(f))); EXPECT(f == 100.0f);
    put(1, WS_INT32_TYPE, NULL, &v, sizeof(v));
    memset(&str, 0, sizeof(str));
    EXPECT_HR(S_OK, get(1, WS_STRING_TYPE, NULL, &str, sizeof(str)));
    EXPECT(str.length == 3 && str.chars && !memcmp(str.chars, L"100", 6));
    v = 300;
    put(1, WS_INT32_TYPE, NULL, &v, sizeof(v));
    EXPECT_HR(WS_E_NUMERIC_OVERFLOW, get(1, WS_INT8_TYPE, NULL, &i8, sizeof(i8)));
    /* a double record read as an integer */
    d = 7.0;
    put(1, WS_DOUBLE_TYPE, NULL, &d, sizeof(d));
    i64 = 0;
    EXPECT_HR(S_OK, get(1, WS_INT64_TYPE, NULL, &i64, sizeof(i64))); EXPECT(i64 == 7);
}

static void test_float_decimal(void)
{
    float f = 0.1f, fo = 0; DECIMAL dec, dout; double d;
    put(0, WS_FLOAT_TYPE, NULL, &f, sizeof(f));
    EXPECT(strstr(w_text(), ">0.1</t>") != NULL);
    f = 1.5f; put(0, WS_FLOAT_TYPE, NULL, &f, sizeof(f));
    EXPECT(strstr(w_text(), ">1.5</t>") != NULL);
    EXPECT_HR(S_OK, get(0, WS_FLOAT_TYPE, NULL, &fo, sizeof(fo))); EXPECT(fo == 1.5f);
    f = 3.25f; put(1, WS_FLOAT_TYPE, NULL, &f, sizeof(f));
    EXPECT_HR(S_OK, get(1, WS_FLOAT_TYPE, NULL, &fo, sizeof(fo))); EXPECT(fo == 3.25f);
    d = 0; put(1, WS_FLOAT_TYPE, NULL, &f, sizeof(f));
    EXPECT_HR(S_OK, get(1, WS_DOUBLE_TYPE, NULL, &d, sizeof(d))); EXPECT(d == 3.25);

    memset(&dec, 0, sizeof(dec)); dec.Lo32 = 12345; dec.scale = 2;
    put(0, WS_DECIMAL_TYPE, NULL, &dec, sizeof(dec));
    EXPECT(strstr(w_text(), ">123.45</t>") != NULL);
    memset(&dout, 0xcc, sizeof(dout));
    EXPECT_HR(S_OK, get(0, WS_DECIMAL_TYPE, NULL, &dout, sizeof(dout)));
    EXPECT(dout.Lo32 == 12345 && dout.scale == 2 && dout.Hi32 == 0 && dout.Mid32 == 0 && !(dout.sign & 0x80));
    dec.sign = DECIMAL_NEG;
    put(1, WS_DECIMAL_TYPE, NULL, &dec, sizeof(dec));
    memset(&dout, 0, sizeof(dout));
    EXPECT_HR(S_OK, get(1, WS_DECIMAL_TYPE, NULL, &dout, sizeof(dout)));
    EXPECT(dout.Lo32 == 12345 && dout.scale == 2 && (dout.sign & 0x80));
    {   /* the range of the description */
        WS_DECIMAL_DESCRIPTION dd;
        memset(&dd, 0, sizeof(dd)); dd.minValue.Lo32 = 1; dd.maxValue.Lo32 = 100;
        memset(&dec, 0, sizeof(dec)); dec.Lo32 = 12345; dec.scale = 2; /* 123.45 */
        put(0, WS_DECIMAL_TYPE, NULL, &dec, sizeof(dec));
        EXPECT_HR(WS_E_INVALID_FORMAT, get(0, WS_DECIMAL_TYPE, &dd, &dout, sizeof(dout)));
        dec.Lo32 = 5000; /* 50.00 */
        put(0, WS_DECIMAL_TYPE, NULL, &dec, sizeof(dec));
        EXPECT_HR(S_OK, get(0, WS_DECIMAL_TYPE, &dd, &dout, sizeof(dout)));
    }
    {   /* 2^96 does not fit */
        const char xml[] = "<t>79228162514264337593543950336</t>";
        r_start(0, (void *)xml, sizeof(xml) - 1);
        WsReadToStartElement(r, &el_t, &empty_ns, NULL, NULL); WsReadStartElement(r, NULL);
        EXPECT_HR(WS_E_NUMERIC_OVERFLOW, WsReadType(r, WS_ELEMENT_CONTENT_TYPE_MAPPING, WS_DECIMAL_TYPE, NULL, WS_READ_REQUIRED_VALUE, heap, &dout, sizeof(dout), NULL));
    }
}

static void test_text_nodes(void)
{
    WS_XML_UTF8_TEXT a, b; WS_XML_STRING el = el_t; WS_STRING str;
    /* two text pieces into one binary text node */
    a.text.textType = WS_XML_TEXT_TYPE_UTF8; a.value.bytes = (BYTE *)"ab"; a.value.length = 2;
    b.text.textType = WS_XML_TEXT_TYPE_UTF8; b.value.bytes = (BYTE *)"cd"; b.value.length = 2;
    w_start(1);
    WsWriteStartElement(w, NULL, &el, &empty_ns, NULL);
    EXPECT_HR(S_OK, WsWriteText(w, &a.text, NULL));
    EXPECT_HR(S_OK, WsWriteText(w, &b.text, NULL));
    WsWriteEndElement(w, NULL);
    memset(&str, 0, sizeof(str));
    EXPECT_HR(S_OK, get(1, WS_STRING_TYPE, NULL, &str, sizeof(str)));
    EXPECT(str.length == 4 && !memcmp(str.chars, L"abcd", 8));
    {   /* a list of numbers */
        WS_XML_INT32_TEXT i1 = { { WS_XML_TEXT_TYPE_INT32 }, 1 }, i2 = { { WS_XML_TEXT_TYPE_INT32 }, 2 }, i3 = { { WS_XML_TEXT_TYPE_INT32 }, 3 };
        WS_XML_TEXT *items[3] = { &i1.text, &i2.text, &i3.text };
        WS_XML_LIST_TEXT list; list.text.textType = WS_XML_TEXT_TYPE_LIST; list.itemCount = 3; list.items = items;
        w_start(0);
        WsWriteStartElement(w, NULL, &el, &empty_ns, NULL);
        EXPECT_HR(S_OK, WsWriteText(w, &list.text, NULL));
        WsWriteEndElement(w, NULL);
        EXPECT(strstr(w_text(), ">1 2 3</t>") != NULL);
    }
    {   /* a float text and a decimal text in an attribute, binary and text */
        WS_XML_FLOAT_TEXT ft = { { WS_XML_TEXT_TYPE_FLOAT }, 2.5f };
        WS_XML_STRING an = XS("a");
        int k;
        for (k = 0; k < 2; k++)
        {
            w_start(k);
            WsWriteStartElement(w, NULL, &el, &empty_ns, NULL);
            WsWriteStartAttribute(w, NULL, &an, &empty_ns, FALSE, NULL);
            EXPECT_HR(S_OK, WsWriteText(w, &ft.text, NULL));
            WsWriteEndAttribute(w, NULL);
            WsWriteEndElement(w, NULL);
            if (!k) EXPECT(strstr(w_text(), "a=\"2.5\"") != NULL);
        }
    }
}

static void test_invalid(void)
{
    BOOL found; INT32 v = 5, out = 0;
    put(0, WS_INT32_TYPE, NULL, &v, sizeof(v));
    {   void *p; ULONG len; w_get(&p, &len); r_start(0, p, len); }
    EXPECT_HR(S_OK, WsReadToStartElement(r, &el_t, &empty_ns, NULL, NULL));
    EXPECT_HR(S_OK, WsReadStartElement(r, NULL));
    EXPECT_HR(E_INVALIDARG, WsReadType(r, WS_ELEMENT_CONTENT_TYPE_MAPPING, WS_INT32_TYPE, NULL, (WS_READ_OPTION)77, heap, &out, sizeof(out), NULL));
    EXPECT_HR(E_INVALIDARG, WsReadType(r, (WS_TYPE_MAPPING)77, WS_INT32_TYPE, NULL, WS_READ_REQUIRED_VALUE, heap, &out, sizeof(out), NULL));
    {   /* moves need a buffer */
        WS_XML_BUFFER *mb; WS_XML_WRITER *mw; WS_XML_READER *mr;
        WsCreateXmlBuffer(heap, NULL, 0, &mb, NULL);
        WsCreateWriter(NULL, 0, &mw, NULL); WsSetOutputToBuffer(mw, mb, NULL, 0, NULL);
        WsWriteStartElement(mw, NULL, &el_t, &empty_ns, NULL); WsWriteEndElement(mw, NULL);
        EXPECT_HR(E_INVALIDARG, WsMoveWriter(mw, (WS_MOVE_TO)99, &found, NULL));
        WsCreateReader(NULL, 0, &mr, NULL); WsSetInputToBuffer(mr, mb, NULL, 0, NULL);
        EXPECT_HR(E_INVALIDARG, WsMoveReader(mr, (WS_MOVE_TO)99, &found, NULL));
        WsFreeReader(mr); WsFreeWriter(mw);
    }
    w_start(0);
    WsWriteStartElement(w, NULL, &el_t, &empty_ns, NULL);
    EXPECT_HR(E_INVALIDARG, WsWriteType(w, (WS_TYPE_MAPPING)77, WS_INT32_TYPE, NULL, WS_WRITE_REQUIRED_VALUE, &v, sizeof(v), NULL));
    {   /* required value in the description of a bool and a guid */
        WS_BOOL_DESCRIPTION bd; BOOL b = 0; GUID g1 = { 1, 2, 3, { 4, 5, 6, 7, 8, 9, 10, 11 } }, g2, gd;
        WS_GUID_DESCRIPTION gdsc;
        const char xml[] = "<t>true</t>";
        bd.value = FALSE;
        r_start(0, (void *)xml, sizeof(xml) - 1);
        WsReadToStartElement(r, &el_t, &empty_ns, NULL, NULL); WsReadStartElement(r, NULL);
        EXPECT_HR(WS_E_INVALID_FORMAT, WsReadType(r, WS_ELEMENT_CONTENT_TYPE_MAPPING, WS_BOOL_TYPE, &bd, WS_READ_REQUIRED_VALUE, heap, &b, sizeof(b), NULL));
        bd.value = TRUE;
        r_start(0, (void *)xml, sizeof(xml) - 1);
        WsReadToStartElement(r, &el_t, &empty_ns, NULL, NULL); WsReadStartElement(r, NULL);
        EXPECT_HR(S_OK, WsReadType(r, WS_ELEMENT_CONTENT_TYPE_MAPPING, WS_BOOL_TYPE, &bd, WS_READ_REQUIRED_VALUE, heap, &b, sizeof(b), NULL));
        put(0, WS_GUID_TYPE, NULL, &g1, sizeof(g1));
        gd = g1; gd.Data1 = 2; gdsc.value = gd;
        EXPECT_HR(WS_E_INVALID_FORMAT, get(0, WS_GUID_TYPE, &gdsc, &g2, sizeof(g2)));
        put(0, WS_GUID_TYPE, NULL, &g1, sizeof(g1));
        gdsc.value = g1;
        EXPECT_HR(S_OK, get(0, WS_GUID_TYPE, &gdsc, &g2, sizeof(g2)));
        EXPECT(!memcmp(&g1, &g2, sizeof(g1)));
    }
}

struct S1 { int a; int b; };
static WS_XML_STRING n_s = XS("S"), n_a = XS("a"), n_b = XS("b");

static void make_struct_desc(WS_STRUCT_DESCRIPTION *sd, WS_FIELD_DESCRIPTION *fa, WS_FIELD_DESCRIPTION **fl, ULONG opts)
{
    memset(sd, 0, sizeof(*sd)); memset(fa, 0, sizeof(*fa));
    fa->mapping = WS_ATTRIBUTE_FIELD_MAPPING; fa->localName = &n_a; fa->ns = &empty_ns; fa->type = WS_INT32_TYPE;
    fa->offset = 0;
    fl[0] = fa;
    sd->size = sizeof(struct S1); sd->alignment = 4; sd->fields = fl; sd->fieldCount = 1; sd->structOptions = opts;
}

static void test_struct(void)
{
    WS_STRUCT_DESCRIPTION sd; WS_FIELD_DESCRIPTION fa, fb, *fl[3]; struct S1 out;
    const char xml[] = "<S a=\"1\" b=\"2\"/>";
    const char xml1[] = "<S a=\"1\"/>";
    make_struct_desc(&sd, &fa, fl, 0);
    r_start(0, (void *)xml, sizeof(xml) - 1);
    WsReadToStartElement(r, &n_s, &empty_ns, NULL, NULL);
    EXPECT_HR(WS_E_INVALID_FORMAT, WsReadType(r, WS_ELEMENT_TYPE_MAPPING, WS_STRUCT_TYPE, &sd, WS_READ_REQUIRED_VALUE, heap, &out, sizeof(out), NULL));
    sd.structOptions = WS_STRUCT_IGNORE_UNHANDLED_ATTRIBUTES;
    r_start(0, (void *)xml, sizeof(xml) - 1);
    WsReadToStartElement(r, &n_s, &empty_ns, NULL, NULL);
    memset(&out, 0, sizeof(out));
    EXPECT_HR(S_OK, WsReadType(r, WS_ELEMENT_TYPE_MAPPING, WS_STRUCT_TYPE, &sd, WS_READ_REQUIRED_VALUE, heap, &out, sizeof(out), NULL));
    EXPECT(out.a == 1);
    /* an attribute that is mapped is fine */
    make_struct_desc(&sd, &fa, fl, 0);
    r_start(0, (void *)xml1, sizeof(xml1) - 1);
    WsReadToStartElement(r, &n_s, &empty_ns, NULL, NULL);
    EXPECT_HR(S_OK, WsReadType(r, WS_ELEMENT_TYPE_MAPPING, WS_STRUCT_TYPE, &sd, WS_READ_REQUIRED_VALUE, heap, &out, sizeof(out), NULL));
    /* b is a field without mapping: it takes the default value */
    memset(&fb, 0, sizeof(fb)); fb.mapping = WS_NO_FIELD_MAPPING; fb.type = WS_INT32_TYPE; fb.offset = 4;
    { int dv = 42; WS_DEFAULT_VALUE def; def.value = &dv; def.valueSize = sizeof(dv); fb.defaultValue = &def;
      fl[1] = &fb; sd.fieldCount = 2;
      r_start(0, (void *)xml1, sizeof(xml1) - 1);
      WsReadToStartElement(r, &n_s, &empty_ns, NULL, NULL);
      out.b = 7;
      EXPECT_HR(S_OK, WsReadType(r, WS_ELEMENT_TYPE_MAPPING, WS_STRUCT_TYPE, &sd, WS_READ_REQUIRED_VALUE, heap, &out, sizeof(out), NULL));
      EXPECT(out.a == 1 && out.b == 42);
      /* writing leaves it out */
      out.a = 3;
      w_start(0);
      EXPECT_HR(S_OK, WsWriteType(w, WS_ELEMENT_TYPE_MAPPING, WS_STRUCT_TYPE, &sd, WS_WRITE_REQUIRED_VALUE, &out, sizeof(out), NULL));
    }
    {   /* a reserved xml attribute (xml:lang) */
        WS_STRING lang; struct { WS_STRING l; } o, in; WS_STRUCT_DESCRIPTION sd2; WS_FIELD_DESCRIPTION f, *fl2[1];
        WS_XML_STRING ln = XS("lang");
        const char xl[] = "<S xml:lang=\"en-US\"/>";
        memset(&f, 0, sizeof(f)); memset(&sd2, 0, sizeof(sd2));
        f.mapping = WS_XML_ATTRIBUTE_FIELD_MAPPING; f.localName = &ln; f.ns = &empty_ns; f.type = WS_STRING_TYPE;
        fl2[0] = &f; sd2.size = sizeof(o); sd2.alignment = sizeof(void *); sd2.fields = fl2; sd2.fieldCount = 1;
        r_start(0, (void *)xl, sizeof(xl) - 1);
        WsReadToStartElement(r, &n_s, &empty_ns, NULL, NULL);
        memset(&o, 0, sizeof(o));
        EXPECT_HR(S_OK, WsReadType(r, WS_ELEMENT_TYPE_MAPPING, WS_STRUCT_TYPE, &sd2, WS_READ_REQUIRED_VALUE, heap, &o, sizeof(o), NULL));
        EXPECT(o.l.length == 5 && !memcmp(o.l.chars, L"en-US", 10));
        lang.chars = (WCHAR *)L"fr"; lang.length = 2; in.l = lang;
        w_start(0);
        WsWriteStartElement(w, NULL, &n_s, &empty_ns, NULL);
        EXPECT_HR(S_OK, WsWriteType(w, WS_ELEMENT_TYPE_MAPPING, WS_STRUCT_TYPE, &sd2, WS_WRITE_REQUIRED_VALUE, &in, sizeof(in), NULL));
        WsWriteEndElement(w, NULL);
        EXPECT(strstr(w_text(), "xml:lang=\"fr\"") != NULL);
    }
}

static void test_fault(void)
{
    WS_FAULT fault, out; WS_FAULT_CODE code, sub; WS_FAULT_REASON reasons[2]; WS_FAULT_DESCRIPTION fd;
    WS_XML_STRING fault_el = XS("Fault"), body_el = XS("Body"); WS_XML_STRING client = XS("Client"), deep = XS("Deep");
    int v;
    memset(&fault, 0, sizeof(fault));
    for (v = 0; v < 2; v++)
    {
        WS_XML_STRING *ns = v ? &soap12 : &soap11;
        memset(&code, 0, sizeof(code)); memset(&sub, 0, sizeof(sub));
        code.value.localName = client; code.value.ns = *ns;
        if (v) { sub.value.localName = deep; sub.value.ns = *ns; code.subCode = &sub; }
        reasons[0].text.chars = (WCHAR *)L"Boom"; reasons[0].text.length = 4;
        reasons[0].lang.chars = (WCHAR *)L"en"; reasons[0].lang.length = 2;
        reasons[1].text.chars = (WCHAR *)L"Knall"; reasons[1].text.length = 5;
        reasons[1].lang.chars = (WCHAR *)L"de"; reasons[1].lang.length = 2;
        fault.code = &code; fault.reasons = reasons; fault.reasonCount = v ? 2 : 1;
        fault.actor.chars = (WCHAR *)L"http://actor"; fault.actor.length = 12;
        fault.node.chars = v ? (WCHAR *)L"http://node" : NULL; fault.node.length = v ? 11 : 0;
        fd.envelopeVersion = v ? WS_ENVELOPE_VERSION_SOAP_1_2 : WS_ENVELOPE_VERSION_SOAP_1_1;
        w_start(0);
        WsWriteStartElement(w, &pfx_s, &body_el, ns, NULL);
        WsWriteStartElement(w, &pfx_s, &fault_el, ns, NULL);
        EXPECT_HR(S_OK, WsWriteType(w, WS_ELEMENT_CONTENT_TYPE_MAPPING, WS_FAULT_TYPE, &fd, WS_WRITE_REQUIRED_VALUE, &fault, sizeof(fault), NULL));
        WsWriteEndElement(w, NULL);
        WsWriteEndElement(w, NULL);
        if (v) EXPECT(strstr(w_text(), "<s:Subcode><s:Value>") != NULL && strstr(w_text(), "xml:lang=\"de\"") != NULL);
        else EXPECT(strstr(w_text(), "<faultcode>s:Client</faultcode>") != NULL && strstr(w_text(), "xml:lang=\"en\"") != NULL);
        {   void *p; ULONG len; w_get(&p, &len); r_start(0, p, len); }
        memset(&out, 0, sizeof(out));
        EXPECT_HR(S_OK, WsReadToStartElement(r, NULL, NULL, NULL, NULL));
        EXPECT_HR(S_OK, WsReadStartElement(r, NULL));
        EXPECT_HR(S_OK, WsReadToStartElement(r, NULL, NULL, NULL, NULL));
        EXPECT_HR(S_OK, WsReadType(r, WS_ELEMENT_TYPE_MAPPING, WS_FAULT_TYPE, &fd, WS_READ_REQUIRED_VALUE, heap, &out, sizeof(out), NULL));
        if (!out.code || !out.reasons) continue;
        EXPECT(out.code && out.code->value.localName.length == 6 && !memcmp(out.code->value.localName.bytes, "Client", 6));
        EXPECT(out.reasonCount == fault.reasonCount && out.reasons[0].text.length == 4 && !memcmp(out.reasons[0].text.chars, L"Boom", 8));
        EXPECT(out.reasons[0].lang.length == 2 && !memcmp(out.reasons[0].lang.chars, L"en", 4));
        EXPECT(out.actor.length == 12);
        if (v)
        {
            EXPECT(out.code->subCode && out.code->subCode->value.localName.length == 4 && !memcmp(out.code->subCode->value.localName.bytes, "Deep", 4));
            EXPECT(out.reasons[1].text.length == 5 && out.reasons[1].lang.length == 2 && !memcmp(out.reasons[1].lang.chars, L"de", 4));
            EXPECT(out.node.length == 11);
        }
    }
}

static void test_msg_error_header(void)
{
    WS_ERROR *err; WS_XML_BUFFER *b = NULL, *got = NULL, *copy;
    WsCreateError(NULL, 0, &err);
    WsCreateXmlBuffer(heap, NULL, 0, &b, NULL);
    w_start(0);
    WsSetOutputToBuffer(w, b, NULL, 0, NULL);
    WsWriteStartElement(w, NULL, &el_t, &empty_ns, NULL); WsWriteEndElement(w, NULL);
    EXPECT_HR(S_OK, WsGetFaultErrorProperty(err, WS_FAULT_ERROR_PROPERTY_HEADER, &got, sizeof(got)));
    EXPECT(got == NULL);
    copy = b;
    EXPECT_HR(S_OK, WsSetFaultErrorProperty(err, WS_FAULT_ERROR_PROPERTY_HEADER, &copy, sizeof(copy)));
    EXPECT_HR(S_OK, WsGetFaultErrorProperty(err, WS_FAULT_ERROR_PROPERTY_HEADER, &got, sizeof(got)));
    EXPECT(got != NULL && got != b);
    {   /* it holds the same bytes */
        WS_BYTES b1, b2; ULONG n1, n2;
        WsSetInputToBuffer(r ? r : NULL, got, NULL, 0, NULL);
        (void)b1; (void)b2; (void)n1; (void)n2;
    }
    WsResetError(err);
    EXPECT_HR(S_OK, WsGetFaultErrorProperty(err, WS_FAULT_ERROR_PROPERTY_HEADER, &got, sizeof(got)));
    EXPECT(got == NULL);
    WsFreeError(err);
}

static void test_msg_headers(void)
{
    WS_MESSAGE *msg; WS_XML_BUFFER *hb; WS_ENDPOINT_ADDRESS addr; WS_XML_STRING act = XS("urn:act"), ref = XS("Ref");
    WS_STRING url = { 14, (WCHAR *)L"http://a.b/c/d" };
    WS_XML_WRITER *mw; WS_XML_BUFFER *mb; WS_XML_WRITER_TEXT_ENCODING te; WS_XML_WRITER_BUFFER_OUTPUT out;
    WS_BYTES bytes; char text[4096]; WS_XML_STRING refns = XS("urn:refns");
    WsCreateXmlBuffer(heap, NULL, 0, &hb, NULL);
    w_start(0);
    WsSetOutputToBuffer(w, hb, NULL, 0, NULL);
    WsWriteStartElement(w, NULL, &ref, &refns, NULL); WsWriteEndElement(w, NULL);
    memset(&addr, 0, sizeof(addr));
    addr.url = url; addr.headers = hb;
    EXPECT_HR(S_OK, WsCreateMessage(WS_ENVELOPE_VERSION_SOAP_1_2, WS_ADDRESSING_VERSION_1_0, NULL, 0, &msg, NULL));
    EXPECT_HR(S_OK, WsInitializeMessage(msg, WS_REQUEST_MESSAGE, NULL, NULL));
    EXPECT_HR(S_OK, WsSetHeader(msg, WS_ACTION_HEADER, WS_XML_STRING_TYPE, WS_WRITE_REQUIRED_VALUE, &act, sizeof(act), NULL));
    EXPECT_HR(S_OK, WsAddressMessage(msg, &addr, NULL));
    WsCreateWriter(NULL, 0, &mw, NULL);
    memset(&te, 0, sizeof(te)); te.encoding.encodingType = WS_XML_WRITER_ENCODING_TYPE_TEXT; te.charSet = WS_CHARSET_UTF8;
    memset(&out, 0, sizeof(out)); out.output.outputType = WS_XML_WRITER_OUTPUT_TYPE_BUFFER;
    WsSetOutput(mw, &te.encoding, &out.output, NULL, 0, NULL);
    EXPECT_HR(S_OK, WsWriteEnvelopeStart(msg, mw, NULL, NULL, NULL));
    WsFlushWriter(mw, 0, NULL, NULL);
    WsGetWriterProperty(mw, WS_XML_WRITER_PROPERTY_BYTES, &bytes, sizeof(bytes), NULL);
    memcpy(text, bytes.bytes, min(bytes.length, sizeof(text) - 1)); text[min(bytes.length, sizeof(text) - 1)] = 0;
    EXPECT(strstr(text, "<Ref") != NULL && strstr(text, "urn:refns") != NULL);
    EXPECT(strstr(text, "http://a.b/c/d") != NULL);
    (void)mb;
    WsFreeWriter(mw); WsFreeMessage(msg);
}

static void test_channel_types(void)
{
    WS_CHANNEL *ch; WS_LISTENER *l;
    static const WS_CHANNEL_TYPE okt[] = { WS_CHANNEL_TYPE_INPUT, WS_CHANNEL_TYPE_OUTPUT, WS_CHANNEL_TYPE_INPUT_SESSION,
        WS_CHANNEL_TYPE_OUTPUT_SESSION, WS_CHANNEL_TYPE_DUPLEX, WS_CHANNEL_TYPE_DUPLEX_SESSION, WS_CHANNEL_TYPE_REQUEST,
        WS_CHANNEL_TYPE_REPLY };
    ULONG i, got;
    for (i = 0; i < ARRAY_SIZE(okt); i++)
    {
        ch = NULL; got = 0;
        EXPECT_HR(S_OK, WsCreateChannel(okt[i], WS_TCP_CHANNEL_BINDING, NULL, 0, NULL, &ch, NULL));
        if (ch)
        {
            WsGetChannelProperty(ch, WS_CHANNEL_PROPERTY_CHANNEL_TYPE, &got, sizeof(got), NULL);
            EXPECT(got == (ULONG)okt[i]);
            WsFreeChannel(ch);
        }
    }
    EXPECT_HR(E_INVALIDARG, WsCreateChannel((WS_CHANNEL_TYPE)0x100, WS_TCP_CHANNEL_BINDING, NULL, 0, NULL, &ch, NULL));
    EXPECT_HR(S_OK, WsCreateListener(WS_CHANNEL_TYPE_INPUT_SESSION, WS_TCP_CHANNEL_BINDING, NULL, 0, NULL, &l, NULL));
    WsFreeListener(l);
    EXPECT_HR(S_OK, WsCreateListener(WS_CHANNEL_TYPE_INPUT, WS_UDP_CHANNEL_BINDING, NULL, 0, NULL, &l, NULL));
    WsFreeListener(l);
    EXPECT_HR(E_INVALIDARG, WsCreateListener(WS_CHANNEL_TYPE_OUTPUT, WS_TCP_CHANNEL_BINDING, NULL, 0, NULL, &l, NULL));
    {   /* a TCP binding template */
        WS_TCP_BINDING_TEMPLATE tmpl; WS_SERVICE_PROXY *proxy = NULL; char desc[8];
        memset(&tmpl, 0, sizeof(tmpl));
        EXPECT_HR(S_OK, WsCreateServiceProxyFromTemplate(WS_CHANNEL_TYPE_DUPLEX_SESSION, NULL, 0, WS_TCP_BINDING_TEMPLATE_TYPE,
                                                         &tmpl, sizeof(tmpl), desc, sizeof(desc), &proxy, NULL));
        if (proxy) WsFreeServiceProxy(proxy);
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    WsCreateHeap(1 << 16, 0, NULL, 0, &heap, NULL);
    test_error();
    test_conv();
    test_float_decimal();
    test_text_nodes();
    test_invalid();
    test_struct();
    test_fault();
    test_msg_error_header();
    test_msg_headers();
    test_channel_types();
    printf("checks: %d, mismatches: %d\n", total, bad);
    printf("RESULT: %s\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
