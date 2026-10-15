/* fastwebtypes-probe: webservices WS_DURATION_TYPE (xsd:duration) and WS_CUSTOM_TYPE (read/write callbacks) */
#include <windows.h>
#include <webservices.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

static int checks, fails;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define BADFMT   ((HRESULT)0x803d0000)
#define OVERFLOW_ ((HRESULT)0x803d0002)
#define WS_E_INVALID_FORMAT_ BADFMT
#define WS_E_INVALID_OPERATION ((HRESULT)0x803d0001)

static WS_HEAP *heap;
static WS_XML_READER *reader;
static WS_XML_WRITER *writer;

static HRESULT start_read(const char *xml)
{
    WS_XML_READER_TEXT_ENCODING enc = {{WS_XML_READER_ENCODING_TYPE_TEXT}, WS_CHARSET_UTF8};
    WS_XML_READER_BUFFER_INPUT in = {{WS_XML_READER_INPUT_TYPE_BUFFER}, (void *)xml, (ULONG)strlen(xml)};
    HRESULT hr = WsSetInput(reader, &enc.encoding, &in.input, NULL, 0, NULL);
    if (hr != S_OK) return hr;
    return WsReadToStartElement(reader, NULL, NULL, NULL, NULL);
}

/* read the content of <t>xml</t> */
static HRESULT rd(const char *xml, WS_TYPE type, const void *desc, WS_READ_OPTION opt, void *val, ULONG size)
{
    HRESULT hr = start_read(xml);
    if (hr != S_OK) return hr;
    hr = WsReadStartElement(reader, NULL);
    if (hr != S_OK) return hr;
    return WsReadType(reader, WS_ELEMENT_CONTENT_TYPE_MAPPING, type, desc, opt, heap, val, size, NULL);
}

static HRESULT begin_write(void)
{
    WS_XML_WRITER_TEXT_ENCODING enc = {{WS_XML_WRITER_ENCODING_TYPE_TEXT}, WS_CHARSET_UTF8};
    WS_XML_WRITER_BUFFER_OUTPUT o = {{WS_XML_WRITER_OUTPUT_TYPE_BUFFER}};
    return WsSetOutput(writer, &enc.encoding, &o.output, NULL, 0, NULL);
}

static void get_out(char *out, ULONG outsize)
{
    WS_BYTES bytes = {0};
    WsGetWriterProperty(writer, WS_XML_WRITER_PROPERTY_BYTES, &bytes, sizeof(bytes), NULL);
    if (bytes.length >= outsize) bytes.length = outsize - 1;
    memcpy(out, bytes.bytes, bytes.length);
    out[bytes.length] = 0;
}

/* write <t>value</t> */
static HRESULT wr(WS_TYPE type, const void *desc, WS_WRITE_OPTION opt, const void *val, ULONG size, char *out, ULONG outsize)
{
    WS_XML_STRING name = {1, (BYTE *)"t"}, ns = {0, NULL};
    HRESULT hr = begin_write();
    if (hr != S_OK) return hr;
    hr = WsWriteStartElement(writer, NULL, &name, &ns, NULL);
    if (hr != S_OK) return hr;
    hr = WsWriteType(writer, WS_ELEMENT_CONTENT_TYPE_MAPPING, type, desc, opt, val, size, NULL);
    if (hr != S_OK) return hr;
    hr = WsWriteEndElement(writer, NULL);
    if (hr != S_OK) return hr;
    get_out(out, outsize);
    return S_OK;
}

static int dur_eq(const WS_DURATION *a, const WS_DURATION *b)
{
    return a->negative == b->negative && a->years == b->years && a->months == b->months && a->days == b->days &&
           a->hours == b->hours && a->minutes == b->minutes && a->seconds == b->seconds &&
           a->milliseconds == b->milliseconds && a->ticks == b->ticks;
}

static void check_write(const WS_DURATION *d, const char *expect)
{
    char out[128], want[160];
    HRESULT hr = wr(WS_DURATION_TYPE, NULL, WS_WRITE_REQUIRED_VALUE, d, sizeof(*d), out, sizeof(out));
    sprintf(want, "<t>%s</t>", expect);
    CHECK(hr == S_OK && !strcmp(out, want), "write %s: %08lx %s", expect, hr, hr == S_OK ? out : "");
}

static void check_read(const char *text, const WS_DURATION *expect)
{
    char xml[160];
    WS_DURATION d;
    HRESULT hr;
    memset(&d, 0xcc, sizeof(d));
    sprintf(xml, "<t>%s</t>", text);
    hr = rd(xml, WS_DURATION_TYPE, NULL, WS_READ_REQUIRED_VALUE, &d, sizeof(d));
    CHECK(hr == S_OK && dur_eq(&d, expect), "read %s: %08lx", text, hr);
}

static HRESULT read_bad(const char *text)
{
    char xml[160];
    WS_DURATION d;
    sprintf(xml, "<t>%s</t>", text);
    return rd(xml, WS_DURATION_TYPE, NULL, WS_READ_REQUIRED_VALUE, &d, sizeof(d));
}

static int comparer_calls;
static HRESULT CALLBACK my_comparer(const WS_DURATION *a, const WS_DURATION *b, int *result, WS_ERROR *error)
{
    comparer_calls++;
    *result = (int)a->days - (int)b->days;
    return S_OK;
}

static void test_duration(void)
{
    WS_DURATION full = {0, 1, 2, 3, 4, 5, 6, 789, 0};
    WS_DURATION zero = {0};
    WS_DURATION neg = {1, 0, 0, 1, 0, 0, 0, 0, 0};
    WS_DURATION tick = {0, 0, 0, 0, 0, 0, 0, 0, 1};
    WS_DURATION frac = {0, 0, 0, 0, 0, 0, 0, 999, 9999};
    WS_DURATION hour = {0, 0, 0, 0, 1, 0, 0, 0, 0};
    WS_DURATION day5 = {0, 0, 0, 5, 0, 0, 0, 0, 0};
    WS_DURATION minute = {0, 0, 0, 0, 0, 2, 0, 0, 0};
    WS_DURATION sec3 = {0, 0, 0, 0, 0, 0, 3, 0, 0};
    WS_DURATION month = {0, 0, 1, 0, 0, 0, 0, 0, 0};
    WS_DURATION badms = {0, 0, 0, 0, 0, 0, 0, 1000, 0};
    WS_DURATION badtick = {0, 0, 0, 0, 0, 0, 0, 0, 10000};
    WS_DURATION d, ptr_val, *pd;
    WS_DURATION_DESCRIPTION desc;
    char out[256];
    HRESULT hr;

    check_write(&full, "P1Y2M3DT4H5M6.789S");
    check_write(&zero, "PT0S");
    check_write(&neg, "-P1D");
    check_write(&tick, "PT0.0000001S");
    check_write(&frac, "PT0.9999999S");
    check_write(&hour, "PT1H");
    check_write(&day5, "P5D");
    check_write(&minute, "PT2M");
    check_write(&sec3, "PT3S");
    check_write(&month, "P1M");
    hr = wr(WS_DURATION_TYPE, NULL, WS_WRITE_REQUIRED_VALUE, &badms, sizeof(badms), out, sizeof(out));
    CHECK(hr == BADFMT, "milliseconds above 999: %08lx", hr);
    hr = wr(WS_DURATION_TYPE, NULL, WS_WRITE_REQUIRED_VALUE, &badtick, sizeof(badtick), out, sizeof(out));
    CHECK(hr == BADFMT, "ticks above 9999: %08lx", hr);
    hr = wr(WS_DURATION_TYPE, NULL, WS_WRITE_REQUIRED_VALUE, &full, sizeof(full) - 1, out, sizeof(out));
    CHECK(hr == E_INVALIDARG, "wrong size: %08lx", hr);
    pd = &full;
    hr = wr(WS_DURATION_TYPE, NULL, WS_WRITE_REQUIRED_POINTER, &pd, sizeof(pd), out, sizeof(out));
    CHECK(hr == S_OK && !strcmp(out, "<t>P1Y2M3DT4H5M6.789S</t>"), "pointer: %08lx", hr);
    pd = NULL;
    hr = wr(WS_DURATION_TYPE, NULL, WS_WRITE_NILLABLE_POINTER, &pd, sizeof(pd), out, sizeof(out));
    CHECK(hr == S_OK && strstr(out, "nil=\"true\""), "nil pointer: %08lx %s", hr, out);

    check_read("P1Y2M3DT4H5M6.789S", &full);
    check_read("PT0S", &zero);
    check_read("-P1D", &neg);
    check_read("PT0.0000001S", &tick);
    check_read("PT0.9999999S", &frac);
    check_read("PT1H", &hour);
    check_read("P5D", &day5);
    check_read("PT2M", &minute);
    check_read("PT3S", &sec3);
    check_read("P1M", &month);
    check_read("  P5D  ", &day5);
    check_read("P0001Y2M3DT4H5M6.7890000S", &full);
    { WS_DURATION t = {0, 0, 0, 0, 0, 0, 0, 0, 1}; check_read("PT0.00000019S", &t); }   /* digits past 7 are dropped */

    CHECK(read_bad("") == BADFMT, "empty");
    CHECK(read_bad("P") == BADFMT, "P alone");
    CHECK(read_bad("PT") == BADFMT, "PT alone");
    CHECK(read_bad("P1") == BADFMT, "no designator");
    CHECK(read_bad("1D") == BADFMT, "no P");
    CHECK(read_bad("P1H") == BADFMT, "hours without T");
    CHECK(read_bad("PT1D") == BADFMT, "days after T");
    CHECK(read_bad("P1M1Y") == BADFMT, "order");
    CHECK(read_bad("PT1S1M") == BADFMT, "time order");
    CHECK(read_bad("P1D1D") == BADFMT, "repeated");
    CHECK(read_bad("P1.5D") == BADFMT, "fraction on days");
    CHECK(read_bad("PT1.S") == BADFMT, "empty fraction");
    CHECK(read_bad("PT1.5M") == BADFMT, "fraction on minutes");
    CHECK(read_bad("P1DT") == BADFMT, "T with nothing after");
    CHECK(read_bad("P1Dx") == BADFMT, "trailing text");
    CHECK(read_bad("P99999999999D") == OVERFLOW_, "overflow: %08lx", read_bad("P99999999999D"));

    /* pointers */
    memset(&d, 0, sizeof(d));
    pd = NULL;
    hr = rd("<t>P2D</t>", WS_DURATION_TYPE, NULL, WS_READ_REQUIRED_POINTER, &pd, sizeof(pd));
    CHECK(hr == S_OK && pd && pd->days == 2, "required pointer %08lx", hr);
    hr = rd("<t>P2D</t>", WS_DURATION_TYPE, NULL, WS_READ_REQUIRED_VALUE, &d, sizeof(d) - 4);
    CHECK(hr == E_INVALIDARG, "wrong size %08lx", hr);

    /* ranges: years and months compare, a mix does not */
    memset(&desc, 0, sizeof(desc));
    desc.minValue.months = 1;
    desc.maxValue.years = 2;
    hr = rd("<t>P1Y</t>", WS_DURATION_TYPE, &desc, WS_READ_REQUIRED_VALUE, &d, sizeof(d));
    CHECK(hr == S_OK && d.years == 1, "in range %08lx", hr);
    hr = rd("<t>P3Y</t>", WS_DURATION_TYPE, &desc, WS_READ_REQUIRED_VALUE, &d, sizeof(d));
    CHECK(hr == BADFMT, "above the maximum %08lx", hr);
    hr = rd("<t>P0M</t>", WS_DURATION_TYPE, &desc, WS_READ_REQUIRED_VALUE, &d, sizeof(d));
    CHECK(hr == BADFMT, "below the minimum %08lx", hr);
    hr = rd("<t>P1D</t>", WS_DURATION_TYPE, &desc, WS_READ_REQUIRED_VALUE, &d, sizeof(d));
    CHECK(hr == BADFMT, "months against days cannot be compared %08lx", hr);
    hr = wr(WS_DURATION_TYPE, &desc, WS_WRITE_REQUIRED_VALUE, &month, sizeof(month), out, sizeof(out));
    CHECK(hr == S_OK, "write in range %08lx", hr);
    { WS_DURATION three = {0, 3, 0}; hr = wr(WS_DURATION_TYPE, &desc, WS_WRITE_REQUIRED_VALUE, &three, sizeof(three), out, sizeof(out));
      CHECK(hr == BADFMT, "write above the maximum %08lx", hr); }
    memset(&desc, 0, sizeof(desc));
    desc.maxValue.days = 10;
    desc.minValue.negative = 1;
    desc.minValue.days = 20;
    hr = rd("<t>PT36H</t>", WS_DURATION_TYPE, &desc, WS_READ_REQUIRED_VALUE, &d, sizeof(d));
    CHECK(hr == S_OK && d.hours == 36, "hours compare as time %08lx", hr);
    hr = rd("<t>P11D</t>", WS_DURATION_TYPE, &desc, WS_READ_REQUIRED_VALUE, &d, sizeof(d));
    CHECK(hr == BADFMT, "11 days above 10 %08lx", hr);
    hr = rd("<t>-P11D</t>", WS_DURATION_TYPE, &desc, WS_READ_REQUIRED_VALUE, &d, sizeof(d));
    CHECK(hr == S_OK && d.negative, "a negative duration is below the maximum %08lx", hr);
    /* a comparer of the description replaces the default one */
    desc.comparer = my_comparer;
    memset(&desc.minValue, 0, sizeof(desc.minValue));
    desc.maxValue.days = 3;
    comparer_calls = 0;
    hr = rd("<t>P1Y</t>", WS_DURATION_TYPE, &desc, WS_READ_REQUIRED_VALUE, &d, sizeof(d));
    CHECK(hr == S_OK && comparer_calls == 2, "comparer used (%d calls) %08lx", comparer_calls, hr);

    /* a struct with a duration field */
    {
        struct S { WS_DURATION d; } s = {{0, 0, 0, 7, 0, 0, 0, 0, 0}}, s2 = {{0}};
        WS_XML_STRING ln = {1, (BYTE *)"d"}, ns = {0, NULL}, sn = {1, (BYTE *)"s"};
        WS_FIELD_DESCRIPTION fd = {WS_ELEMENT_FIELD_MAPPING, &ln, &ns, WS_DURATION_TYPE, NULL, 0, 0, 0};
        WS_FIELD_DESCRIPTION *fields[1] = {&fd};
        WS_STRUCT_DESCRIPTION sd = {sizeof(s), TYPE_ALIGNMENT(struct S), fields, 1, &sn, &ns, 0};
        WS_XML_STRING root = {1, (BYTE *)"s"};
        fd.offset = 0;
        hr = wr(WS_STRUCT_TYPE, &sd, WS_WRITE_REQUIRED_POINTER, &(const void *){&s}, sizeof(void *), out, sizeof(out));
        CHECK(hr == S_OK && strstr(out, "<d>P7D</d>"), "struct field written: %08lx %s", hr, out);
        hr = start_read("<s><d>P9DT1H</d></s>");
        if (hr == S_OK) hr = WsReadType(reader, WS_ELEMENT_TYPE_MAPPING, WS_STRUCT_TYPE, &sd, WS_READ_REQUIRED_VALUE, heap, &s2, sizeof(s2), NULL);
        CHECK(hr == S_OK && s2.d.days == 9 && s2.d.hours == 1, "struct field read: %08lx", hr);
        (void)root;
    }

    /* arrays of the value type */
    {
        WS_DURATION arr[2] = {{0, 0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 0, 2, 0, 0, 0, 0, 0}}, back[2] = {{0}};
        WS_XML_STRING n = {1, (BYTE *)"i"}, ns = {0, NULL}, root = {1, (BYTE *)"r"};
        ULONG got = 0;
        hr = begin_write();
        if (hr == S_OK) hr = WsWriteStartElement(writer, NULL, &root, &ns, NULL);
        if (hr == S_OK) hr = WsWriteArray(writer, &n, &ns, WS_DURATION_VALUE_TYPE, arr, sizeof(arr), 0, 2, NULL);
        if (hr == S_OK) hr = WsWriteEndElement(writer, NULL);
        get_out(out, sizeof(out));
        CHECK(hr == S_OK && strstr(out, "<i>P1D</i><i>P2D</i>"), "array written: %08lx %s", hr, out);
        hr = start_read("<r><i>P1D</i><i>P2D</i></r>");
        if (hr == S_OK) hr = WsReadStartElement(reader, NULL);
        if (hr == S_OK) hr = WsReadArray(reader, &n, &ns, WS_DURATION_VALUE_TYPE, back, sizeof(back), 0, 2, &got, NULL);
        CHECK(hr == S_OK && got == 2 && back[0].days == 1 && back[1].days == 2, "array read: %08lx %lu", hr, got);
    }
}

/* ---- custom type: a pair of numbers written as "a,b" ---- */
struct pair { int a, b; };
static int last_mapping = -1, last_size, write_calls, read_calls;
static const void *last_data;
static int fail_write;

static HRESULT CALLBACK pair_write(WS_XML_WRITER *w, WS_TYPE_MAPPING mapping, const void *data, const void *value,
                                   ULONG size, WS_ERROR *error)
{
    const struct pair *p = value;
    WCHAR text[64];
    WS_XML_UTF16_TEXT t;
    write_calls++;
    last_mapping = mapping;
    last_data = data;
    last_size = size;
    if (fail_write) return WS_E_INVALID_OPERATION;
    swprintf(text, 64, L"%d,%d", p->a, p->b);
    t.text.textType = WS_XML_TEXT_TYPE_UTF16;
    t.bytes = (BYTE *)text;
    t.byteCount = wcslen(text) * sizeof(WCHAR);
    return WsWriteText(w, &t.text, error);
}

static HRESULT CALLBACK pair_read(WS_XML_READER *r, WS_TYPE_MAPPING mapping, const void *data, WS_HEAP *h, void *value,
                                  ULONG size, WS_ERROR *error)
{
    struct pair *p = value;
    WCHAR text[64];
    ULONG n = 0;
    HRESULT hr;
    read_calls++;
    last_mapping = mapping;
    last_data = data;
    last_size = size;
    if (mapping == WS_ELEMENT_TYPE_MAPPING && (hr = WsReadStartElement(r, error)) != S_OK) return hr;
    if ((hr = WsReadChars(r, text, 63, &n, error)) != S_OK) return hr;
    text[n] = 0;
    if (swscanf(text, L"%d,%d", &p->a, &p->b) != 2) return WS_E_INVALID_FORMAT_;
    if (mapping == WS_ELEMENT_TYPE_MAPPING) return WsReadEndElement(r, error);
    return S_OK;
}

static void test_custom(void)
{
    static int marker;
    WS_CUSTOM_TYPE_DESCRIPTION desc = {sizeof(struct pair), TYPE_ALIGNMENT(struct pair), pair_read, pair_write, &marker, NULL};
    struct pair p = {3, 4}, q = {0, 0};
    const struct pair *pp = &p;
    char out[256];
    HRESULT hr;

    write_calls = read_calls = 0;
    hr = wr(WS_CUSTOM_TYPE, &desc, WS_WRITE_REQUIRED_VALUE, &p, sizeof(p), out, sizeof(out));
    CHECK(hr == S_OK && !strcmp(out, "<t>3,4</t>"), "write: %08lx %s", hr, out);
    CHECK(write_calls == 1 && last_mapping == WS_ELEMENT_CONTENT_TYPE_MAPPING && last_data == &marker &&
          last_size == sizeof(p), "callback arguments: %d %d %p %d", write_calls, last_mapping, last_data, last_size);
    hr = wr(WS_CUSTOM_TYPE, &desc, WS_WRITE_REQUIRED_VALUE, &p, sizeof(p) - 1, out, sizeof(out));
    CHECK(hr == E_INVALIDARG && write_calls == 1, "wrong size: %08lx", hr);
    hr = wr(WS_CUSTOM_TYPE, &desc, WS_WRITE_REQUIRED_POINTER, &pp, sizeof(pp), out, sizeof(out));
    CHECK(hr == S_OK && !strcmp(out, "<t>3,4</t>"), "pointer: %08lx %s", hr, out);
    pp = NULL;
    hr = wr(WS_CUSTOM_TYPE, &desc, WS_WRITE_NILLABLE_POINTER, &pp, sizeof(pp), out, sizeof(out));
    CHECK(hr == S_OK && strstr(out, "nil=\"true\"") && write_calls == 2, "nil pointer: %08lx %s calls %d", hr, out, write_calls);
    hr = wr(WS_CUSTOM_TYPE, NULL, WS_WRITE_REQUIRED_VALUE, &p, sizeof(p), out, sizeof(out));
    CHECK(hr == E_INVALIDARG, "no description: %08lx", hr);
    fail_write = 1;
    hr = wr(WS_CUSTOM_TYPE, &desc, WS_WRITE_REQUIRED_VALUE, &p, sizeof(p), out, sizeof(out));
    CHECK(hr == WS_E_INVALID_OPERATION, "callback error is returned: %08lx", hr);
    fail_write = 0;

    /* reading: with the element mapping the callback reads the element itself */
    hr = start_read("<t>5,6</t>");
    if (hr == S_OK) hr = WsReadType(reader, WS_ELEMENT_TYPE_MAPPING, WS_CUSTOM_TYPE, &desc, WS_READ_REQUIRED_VALUE, heap, &q, sizeof(q), NULL);
    CHECK(hr == S_OK && q.a == 5 && q.b == 6, "read element: %08lx %d,%d", hr, q.a, q.b);
    CHECK(read_calls == 1 && last_mapping == WS_ELEMENT_TYPE_MAPPING && last_data == &marker && last_size == sizeof(q),
          "read callback arguments: %d %d %p %d", read_calls, last_mapping, last_data, last_size);
    /* and the reader is after the element */
    hr = WsReadToStartElement(reader, NULL, NULL, NULL, NULL);
    {
        WS_XML_NODE_TYPE type = 0;
        const WS_XML_NODE *node = NULL;
        WsGetReaderNode(reader, &node, NULL);
        type = node ? node->nodeType : 0;
        CHECK(type == WS_XML_NODE_TYPE_EOF, "after the element: node type %d", type);
    }
    hr = rd("<t>7,8</t>", WS_CUSTOM_TYPE, &desc, WS_READ_REQUIRED_VALUE, &q, sizeof(q) + 1);
    CHECK(hr == E_INVALIDARG, "read wrong size: %08lx", hr);
    hr = rd("<t>x</t>", WS_CUSTOM_TYPE, &desc, WS_READ_REQUIRED_VALUE, &q, sizeof(q));
    CHECK(hr == WS_E_INVALID_FORMAT_, "callback error is returned: %08lx", hr);
    hr = rd("<t>1,2</t>", WS_CUSTOM_TYPE, NULL, WS_READ_REQUIRED_VALUE, &q, sizeof(q));
    CHECK(hr == E_INVALIDARG, "read without description: %08lx", hr);

    /* a field of a struct */
    {
        struct S { struct pair c; int after; struct pair *ptr; } s = {{11, 12}, 5, &p}, s2 = {{0}, 0, NULL};
        WS_XML_STRING ln = {1, (BYTE *)"c"}, ns = {0, NULL}, sn = {1, (BYTE *)"s"}, ln2 = {1, (BYTE *)"n"};
        WS_FIELD_DESCRIPTION fd = {WS_ELEMENT_FIELD_MAPPING, &ln, &ns, WS_CUSTOM_TYPE, &desc, 0, 0, 0};
        WS_FIELD_DESCRIPTION fd2 = {WS_ELEMENT_FIELD_MAPPING, &ln2, &ns, WS_INT32_TYPE, NULL, 0, 0, 0};
        WS_XML_STRING ln3 = {1, (BYTE *)"p"};
        WS_FIELD_DESCRIPTION fd3 = {WS_ELEMENT_FIELD_MAPPING, &ln3, &ns, WS_CUSTOM_TYPE, &desc, 0, WS_FIELD_POINTER, 0};
        WS_FIELD_DESCRIPTION *fields[3] = {&fd, &fd2, &fd3};
        WS_STRUCT_DESCRIPTION sd = {sizeof(s), TYPE_ALIGNMENT(struct S), fields, 3, &sn, &ns, 0};
        const void *ps = &s;
        fd.offset = 0;
        fd2.offset = offsetof(struct S, after);
        fd3.offset = offsetof(struct S, ptr);
        hr = wr(WS_STRUCT_TYPE, &sd, WS_WRITE_REQUIRED_POINTER, &ps, sizeof(ps), out, sizeof(out));
        CHECK(hr == S_OK && strstr(out, "<c>11,12</c><n>5</n><p>3,4</p>"), "struct written: %08lx %s", hr, out);
        hr = start_read("<s><c>21,22</c><n>6</n><p>31,32</p></s>");
        if (hr == S_OK) hr = WsReadType(reader, WS_ELEMENT_TYPE_MAPPING, WS_STRUCT_TYPE, &sd, WS_READ_REQUIRED_VALUE, heap, &s2, sizeof(s2), NULL);
        CHECK(hr == S_OK && s2.c.a == 21 && s2.c.b == 22 && s2.after == 6 && s2.ptr && s2.ptr->a == 31 && s2.ptr->b == 32, "struct read: %08lx %d,%d %d", hr, s2.c.a, s2.c.b, s2.after);
    }
}


/* ---- WS_XML_BUFFER_TYPE ---- */
static WS_XML_BUFFER *make_buffer(void)
{
    WS_XML_BUFFER *buf = NULL;
    WS_XML_WRITER *w = NULL;
    WS_XML_STRING src = {3, (BYTE *)"Src"}, attr = {4, (BYTE *)"attr"}, b = {1, (BYTE *)"b"}, ns = {0, NULL};
    WS_XML_STRING pfx = {1, (BYTE *)"p"}, pns = {5, (BYTE *)"urn:p"};
    WS_XML_UTF8_TEXT one = {{WS_XML_TEXT_TYPE_UTF8}, {1, (BYTE *)"1"}}, text = {{WS_XML_TEXT_TYPE_UTF8}, {4, (BYTE *)"text"}};
    WS_XML_UTF8_TEXT hi = {{WS_XML_TEXT_TYPE_UTF8}, {2, (BYTE *)"hi"}};

    WsCreateXmlBuffer(heap, NULL, 0, &buf, NULL);
    WsCreateWriter(NULL, 0, &w, NULL);
    WsSetOutputToBuffer(w, buf, NULL, 0, NULL);
    WsWriteStartElement(w, NULL, &src, &ns, NULL);
    WsWriteXmlnsAttribute(w, &pfx, &pns, FALSE, NULL);
    WsWriteStartAttribute(w, NULL, &attr, &ns, FALSE, NULL);
    WsWriteText(w, &one.text, NULL);
    WsWriteEndAttribute(w, NULL);
    WsWriteText(w, &text.text, NULL);
    WsWriteStartElement(w, NULL, &b, &ns, NULL);
    WsWriteText(w, &hi.text, NULL);
    WsWriteEndElement(w, NULL);
    WsWriteEndElement(w, NULL);
    WsFreeWriter(w);
    return buf;
}

static void test_xml_buffer(void)
{
    WS_XML_BUFFER *buf = make_buffer(), *nilbuf = NULL, *back = NULL;
    WS_XML_STRING name = {1, (BYTE *)"t"}, ns = {0, NULL};
    char out[512];
    HRESULT hr;

    CHECK(buf != NULL, "buffer");
    if (!buf) return;

    /* element mapping: the name of the root is replaced, its attributes and content are written */
    hr = begin_write();
    if (hr == S_OK) hr = WsWriteStartElement(writer, NULL, &name, &ns, NULL);
    if (hr == S_OK) hr = WsWriteType(writer, WS_ELEMENT_TYPE_MAPPING, WS_XML_BUFFER_TYPE, NULL, WS_WRITE_REQUIRED_VALUE, &buf, sizeof(buf), NULL);
    if (hr == S_OK) hr = WsWriteEndElement(writer, NULL);
    get_out(out, sizeof(out));
    CHECK(hr == S_OK && strstr(out, "<t ") && strstr(out, "attr=\"1\"") && strstr(out, "xmlns:p=\"urn:p\"") &&
          strstr(out, ">text<b>hi</b></t>") && !strstr(out, "Src"), "element mapping: %08lx %s", hr, out);

    /* any element mapping: the whole element */
    hr = begin_write();
    if (hr == S_OK) hr = WsWriteType(writer, WS_ANY_ELEMENT_TYPE_MAPPING, WS_XML_BUFFER_TYPE, NULL, WS_WRITE_REQUIRED_VALUE, &buf, sizeof(buf), NULL);
    get_out(out, sizeof(out));
    CHECK(hr == S_OK && strstr(out, "<Src ") && strstr(out, "attr=\"1\"") && strstr(out, ">text<b>hi</b></Src>"), "any element mapping: %08lx %s", hr, out);

    /* nil and bad arguments */
    hr = begin_write();
    if (hr == S_OK) hr = WsWriteStartElement(writer, NULL, &name, &ns, NULL);
    if (hr == S_OK) hr = WsWriteType(writer, WS_ELEMENT_TYPE_MAPPING, WS_XML_BUFFER_TYPE, NULL, WS_WRITE_NILLABLE_VALUE, &nilbuf, sizeof(nilbuf), NULL);
    if (hr == S_OK) hr = WsWriteEndElement(writer, NULL);
    get_out(out, sizeof(out));
    CHECK(hr == S_OK && strstr(out, "nil=\"true\""), "nil buffer: %08lx %s", hr, out);
    hr = begin_write();
    if (hr == S_OK) hr = WsWriteStartElement(writer, NULL, &name, &ns, NULL);
    if (hr == S_OK) hr = WsWriteType(writer, WS_ELEMENT_TYPE_MAPPING, WS_XML_BUFFER_TYPE, NULL, WS_WRITE_REQUIRED_VALUE, &nilbuf, sizeof(nilbuf), NULL);
    CHECK(hr == E_INVALIDARG, "required buffer is NULL: %08lx", hr);
    hr = begin_write();
    if (hr == S_OK) hr = WsWriteStartElement(writer, NULL, &name, &ns, NULL);
    if (hr == S_OK) hr = WsWriteType(writer, WS_ELEMENT_TYPE_MAPPING, WS_XML_BUFFER_TYPE, NULL, WS_WRITE_REQUIRED_VALUE, &buf, sizeof(buf) + 1, NULL);
    CHECK(hr == E_INVALIDARG, "wrong size: %08lx", hr);
    hr = begin_write();
    if (hr == S_OK) hr = WsWriteStartElement(writer, NULL, &name, &ns, NULL);
    if (hr == S_OK) hr = WsWriteType(writer, WS_ATTRIBUTE_TYPE_MAPPING, WS_XML_BUFFER_TYPE, NULL, WS_WRITE_REQUIRED_VALUE, &buf, sizeof(buf), NULL);
    CHECK(FAILED(hr), "attribute mapping: %08lx", hr);

    /* as a field of a struct, the element is read into a buffer which keeps its name */
    {
        struct S { WS_XML_BUFFER *b; int after; } s2 = {NULL, 0};
        WS_XML_STRING ln = {1, (BYTE *)"e"}, sn = {1, (BYTE *)"s"}, ln2 = {1, (BYTE *)"n"};
        WS_FIELD_DESCRIPTION fd = {WS_ELEMENT_FIELD_MAPPING, &ln, &ns, WS_XML_BUFFER_TYPE, NULL, 0, 0, 0};
        WS_FIELD_DESCRIPTION fd2 = {WS_ELEMENT_FIELD_MAPPING, &ln2, &ns, WS_INT32_TYPE, NULL, 0, 0, 0};
        WS_FIELD_DESCRIPTION *fields[2] = {&fd, &fd2};
        WS_STRUCT_DESCRIPTION sd = {sizeof(s2), TYPE_ALIGNMENT(struct S), fields, 2, &sn, &ns, 0};
        fd.offset = 0;
        fd2.offset = offsetof(struct S, after);
        hr = start_read("<s><e a=\"2\">x<c/></e><n>8</n></s>");
        if (hr == S_OK) hr = WsReadType(reader, WS_ELEMENT_TYPE_MAPPING, WS_STRUCT_TYPE, &sd, WS_READ_REQUIRED_VALUE, heap, &s2, sizeof(s2), NULL);
        CHECK(hr == S_OK && s2.b && s2.after == 8, "struct read: %08lx %p %d", hr, s2.b, s2.after);
        back = s2.b;
        if (back)
        {
            hr = begin_write();
            if (hr == S_OK) hr = WsWriteXmlBuffer(writer, back, NULL);
            get_out(out, sizeof(out));
            CHECK(hr == S_OK && strstr(out, "<e ") && strstr(out, "a=\"2\"") && strstr(out, ">x<c") && strstr(out, "</e>") == strstr(out, "</e>"),
                  "buffer content: %08lx %s", hr, out);
        }
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    WsCreateHeap(1 << 16, 0, NULL, 0, &heap, NULL);
    WsCreateReader(NULL, 0, &reader, NULL);
    WsCreateWriter(NULL, 0, &writer, NULL);
    test_duration();
    test_custom();
    test_xml_buffer();
    printf("checks %d fails %d\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
