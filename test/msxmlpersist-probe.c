/* msxml3 batch (patches/sg/2022), run by test/msxmlpersist-gate.sh. Families
 * (from Wine's todo_wine blocks, which record Windows): the old XMLDocument's
 * IPersistStreamInit (IsDirty, Save, InitNew, Load round trips) and the
 * reason in the IXMLDOMParseError a schema validation hands back.
 *
 *   msxmlpersist-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ocidl.h>
#include <msxml6.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
static void checkhr(HRESULT hr, HRESULT want, const char *what)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "%s (hr %08lx, want %08lx)", what, (unsigned long)hr, (unsigned long)want);
    check(hr == want, buf);
}

static const char *find(const char *hay, size_t n, const char *needle)
{
    size_t k = strlen(needle), i;
    for (i = 0; i + k <= n; i++) if (!memcmp(hay + i, needle, k)) return hay + i;
    return NULL;
}

static void *create_progid(const WCHAR *progid, REFIID riid)
{
    CLSID clsid;
    void *obj = NULL;
    if (FAILED(CLSIDFromProgID(progid, &clsid))) return NULL;
    CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, riid, &obj);
    return obj;
}

static const GUID SG_CLSID_XMLDocument = { 0xcfc399af, 0xd876, 0x11d0, { 0x9c, 0x10, 0x00, 0xc0, 0x4f, 0xc9, 0x9c, 0x8e } };

static IStream *stream_of(const char *text)
{
    IStream *s = NULL;
    ULONG n;
    CreateStreamOnHGlobal(NULL, TRUE, &s);
    IStream_Write(s, text, strlen(text), &n);
    {
        LARGE_INTEGER zero = { 0 };
        IStream_Seek(s, zero, STREAM_SEEK_SET, NULL);
    }
    return s;
}

static ULONGLONG stream_size(IStream *s)
{
    STATSTG st;
    memset(&st, 0, sizeof(st));
    IStream_Stat(s, &st, STATFLAG_NONAME);
    return st.cbSize.QuadPart;
}

static void test_persist(void)
{
    IPersistStreamInit *psi = NULL;
    IStream *in, *out;
    ULARGE_INTEGER size;
    CLSID id;
    HRESULT hr;
    char *mem;
    HGLOBAL hg;

    hr = CoCreateInstance(&SG_CLSID_XMLDocument, NULL, CLSCTX_INPROC_SERVER, &IID_IPersistStreamInit, (void **)&psi);
    checkhr(hr, S_OK, "create the XMLDocument and ask for IPersistStreamInit");
    if (!psi) return;

    checkhr(IPersistStreamInit_IsDirty(psi), S_FALSE, "a new document is not dirty");
    checkhr(IPersistStreamInit_Save(psi, NULL, FALSE), E_INVALIDARG, "Save without a stream");
    checkhr(IPersistStreamInit_Load(psi, NULL), E_INVALIDARG, "Load without a stream");
    size.QuadPart = 5;
    checkhr(IPersistStreamInit_GetSizeMax(psi, &size), E_NOTIMPL, "GetSizeMax is not implemented");
    check(size.QuadPart == 5, "and leaves the size alone");

    in = stream_of("<?xml version=\"1.0\"?><bank><account id=\"7\"><name>Ann</name></account></bank>");
    hr = IPersistStreamInit_Load(psi, in);
    IStream_Release(in);
    checkhr(hr, S_OK, "Load");
    checkhr(IPersistStreamInit_IsDirty(psi), S_FALSE, "a loaded document is not dirty");

    out = NULL;
    CreateStreamOnHGlobal(NULL, TRUE, &out);
    hr = IPersistStreamInit_Save(psi, out, FALSE);
    checkhr(hr, S_OK, "Save");
    check(stream_size(out) > 40, "what was saved has content");
    GetHGlobalFromStream(out, &hg);
    mem = GlobalLock(hg);
    check(mem && find(mem, (size_t)stream_size(out), "<bank>") && find(mem, (size_t)stream_size(out), "Ann"),
          "and it is the document");
    GlobalUnlock(hg);
    IStream_Release(out);

    /* saved text loads again */
    out = NULL;
    CreateStreamOnHGlobal(NULL, TRUE, &out);
    IPersistStreamInit_Save(psi, out, TRUE);
    {
        LARGE_INTEGER zero = { 0 };
        IPersistStreamInit *psi2 = NULL;
        IStream_Seek(out, zero, STREAM_SEEK_SET, NULL);
        CoCreateInstance(&SG_CLSID_XMLDocument, NULL, CLSCTX_INPROC_SERVER, &IID_IPersistStreamInit, (void **)&psi2);
        checkhr(IPersistStreamInit_Load(psi2, out), S_OK, "a saved document loads into another");
        out = out;
        IPersistStreamInit_Release(psi2);
    }
    IStream_Release(out);

    checkhr(IPersistStreamInit_InitNew(psi), S_OK, "InitNew");
    checkhr(IPersistStreamInit_IsDirty(psi), S_FALSE, "after InitNew: not dirty");
    out = NULL;
    CreateStreamOnHGlobal(NULL, TRUE, &out);
    hr = IPersistStreamInit_Save(psi, out, FALSE);
    checkhr(hr, S_OK, "Save of an empty document");
    check(stream_size(out) > 0, "still writes something");
    IStream_Release(out);

    memset(&id, 0, sizeof(id));
    hr = IPersistStreamInit_GetClassID(psi, &id);
    check(hr == S_OK && IsEqualCLSID(&id, &SG_CLSID_XMLDocument), "GetClassID");
    checkhr(IPersistStreamInit_GetClassID(psi, NULL), E_POINTER, "GetClassID(NULL)");
    IPersistStreamInit_Release(psi);
}

static IXMLDOMDocument2 *newdoc(const WCHAR *xml)
{
    IXMLDOMDocument2 *doc = NULL;
    VARIANT_BOOL ok = VARIANT_FALSE;
    BSTR b = SysAllocString(xml);
    doc = create_progid(L"Msxml2.DOMDocument.3.0", &IID_IXMLDOMDocument2);
    if (doc) IXMLDOMDocument2_loadXML(doc, b, &ok);
    SysFreeString(b);
    return ok == VARIANT_TRUE ? doc : NULL;
}

static void validate_case(IXMLDOMSchemaCollection *cache, const WCHAR *xml, int valid, const char *what)
{
    IXMLDOMDocument2 *doc = newdoc(xml);
    IXMLDOMParseError *err = NULL;
    VARIANT v;
    BSTR reason = NULL;
    LONG code = 0;
    HRESULT hr, hr_reason = E_FAIL;
    char msg[200];

    snprintf(msg, sizeof(msg), "%s: document loads", what);
    check(doc != NULL, msg);
    if (!doc) return;
    VariantInit(&v);
    V_VT(&v) = VT_DISPATCH;
    IXMLDOMSchemaCollection_QueryInterface(cache, &IID_IDispatch, (void **)&V_DISPATCH(&v));
    IXMLDOMDocument2_putref_schemas(doc, v);
    VariantClear(&v);
    hr = IXMLDOMDocument2_validate(doc, &err);
    snprintf(msg, sizeof(msg), "%s: validate (hr %08lx)", what, (unsigned long)hr);
    check(hr == (valid ? S_OK : S_FALSE) && err, msg);
    if (err)
    {
        IXMLDOMParseError_get_errorCode(err, &code);
        hr_reason = IXMLDOMParseError_get_reason(err, &reason);
        if (valid)
        {
            snprintf(msg, sizeof(msg), "%s: no reason", what);
            check(hr_reason == S_FALSE || !reason || !*reason, msg);
        }
        else
        {
            snprintf(msg, sizeof(msg), "%s: a reason is given (hr %08lx)", what, (unsigned long)hr_reason);
            check(hr_reason == S_OK && reason && *reason, msg);
            snprintf(msg, sizeof(msg), "%s: and it has no trailing line break", what);
            check(reason && *reason && reason[wcslen(reason) - 1] != '\n', msg);
            snprintf(msg, sizeof(msg), "%s: an error code", what);
            check(code != 0, msg);
        }
        SysFreeString(reason);
        IXMLDOMParseError_Release(err);
    }
    IXMLDOMDocument2_Release(doc);
}

static void test_validate_reason(void)
{
    static const WCHAR schema_xml[] =
        L"<?xml version=\"1.0\"?><schema xmlns=\"http://www.w3.org/2001/XMLSchema\" targetNamespace=\"urn:sg\" elementFormDefault=\"qualified\">"
        L"<element name=\"root\"><complexType><sequence><element name=\"a\" type=\"string\"/></sequence></complexType></element>"
        L"</schema>";
    IXMLDOMDocument2 *schema = newdoc(schema_xml);
    IXMLDOMSchemaCollection *cache = NULL;
    VARIANT v;
    BSTR ns;
    HRESULT hr;

    check(schema != NULL, "the schema loads");
    cache = create_progid(L"Msxml2.XMLSchemaCache.3.0", &IID_IXMLDOMSchemaCollection);
    check(cache != NULL, "schema cache");
    if (!schema || !cache) return;
    VariantInit(&v);
    V_VT(&v) = VT_DISPATCH;
    V_DISPATCH(&v) = (IDispatch *)schema;
    ns = SysAllocString(L"urn:sg");
    hr = IXMLDOMSchemaCollection_add(cache, ns, v);
    checkhr(hr, S_OK, "add the schema");
    SysFreeString(ns);

    validate_case(cache, L"<root xmlns=\"urn:sg\"><a>fine</a></root>", 1, "valid");
    validate_case(cache, L"<root xmlns=\"urn:sg\"><b/></root>", 0, "wrong element");
    validate_case(cache, L"<root xmlns=\"urn:sg\"><a>x</a><a>y</a></root>", 0, "extra element");
    validate_case(cache, L"<root xmlns=\"urn:sg\"/>", 0, "missing element");

    IXMLDOMSchemaCollection_Release(cache);
    IXMLDOMDocument2_Release(schema);
}

int main(void)
{
    CoInitialize(NULL);
    test_persist();
    test_validate_reason();
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    CoUninitialize();
    return failures != 0;
}
