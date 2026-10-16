#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <initguid.h>
#include <xmllite.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static IStream *mkstream(const char *data)
{
    IStream *s; LARGE_INTEGER z = {{0}}; ULONG n;
    CreateStreamOnHGlobal(NULL, TRUE, &s);
    IStream_Write(s, data, strlen(data), &n);
    IStream_Seek(s, z, STREAM_SEEK_SET, NULL);
    return s;
}

int main(void)
{
    static const char *xml = "<!DOCTYPE doc [\n<!ENTITY e1 \"hello\">\n<!ENTITY e2 \"[&e1;]\">\n]>\n<doc a=\"&e1;!\">&e1; &e2;</doc>";
    IXmlReader *r; IXmlWriter *w; IStream *s, *out; XmlNodeType t; const WCHAR *str; UINT len; HRESULT hr; LONG_PTR v;
    HGLOBAL hg; char *p;

    CoInitialize(NULL);
    CHECK(SUCCEEDED(CreateXmlReader(&IID_IXmlReader, (void **)&r, NULL)), "reader");
    IXmlReader_SetProperty(r, XmlReaderProperty_DtdProcessing, DtdProcessing_Parse);
    s = mkstream(xml);
    IXmlReader_SetInput(r, (IUnknown *)s);
    hr = IXmlReader_Read(r, &t); CHECK(hr == S_OK && t == XmlNodeType_DocumentType, "dtd %lx %d", hr, t);
    hr = IXmlReader_Read(r, &t); CHECK(hr == S_OK && t == XmlNodeType_Whitespace, "ws %lx %d", hr, t);
    hr = IXmlReader_Read(r, &t); CHECK(hr == S_OK && t == XmlNodeType_Element, "elem %lx %d", hr, t);
    hr = IXmlReader_MoveToFirstAttribute(r); CHECK(hr == S_OK, "attr");
    IXmlReader_GetValue(r, &str, &len); CHECK(!wcscmp(str, L"hello!"), "attr value %ls", str);
    hr = IXmlReader_Read(r, &t); CHECK(hr == S_OK && t == XmlNodeType_Text, "text %lx %d", hr, t);
    IXmlReader_GetValue(r, &str, &len); CHECK(!wcscmp(str, L"hello [hello]"), "text value %ls", str);
    hr = IXmlReader_GetBaseUri(r, &str, &len); CHECK(hr == S_OK && !str && !len, "base uri %lx", hr);
    hr = IXmlReader_GetProperty(r, XmlReaderProperty_MaxEntityExpansion, &v); CHECK(hr == S_OK && v > 0, "maxexp");
    IStream_Release(s); IXmlReader_Release(r);

    CHECK(SUCCEEDED(CreateXmlWriter(&IID_IXmlWriter, (void **)&w, NULL)), "writer");
    CreateStreamOnHGlobal(NULL, TRUE, &out);
    IXmlWriter_SetProperty(w, XmlWriterProperty_OmitXmlDeclaration, TRUE);
    IXmlWriter_SetOutput(w, (IUnknown *)out);
    IXmlWriter_WriteStartElement(w, NULL, L"e", NULL);
    hr = IXmlWriter_WriteAttributeString(w, NULL, L"a", L"urn:x", L"b"); CHECK(hr == S_OK, "autoprefix %lx", hr);
    hr = IXmlWriter_WriteAttributeString(w, NULL, L"a", L"urn:y", L"c"); CHECK(hr == S_OK, "autoprefix2 %lx", hr);
    hr = IXmlWriter_WriteAttributeString(w, L"p1", L"a", NULL, L"c"); CHECK(hr == WR_E_DUPLICATEATTRIBUTE, "dup %lx", hr);
    hr = IXmlWriter_WriteEntityRef(w, L"amp"); CHECK(hr == S_OK, "entityref %lx", hr);
    hr = IXmlWriter_WriteName(w, L"nm"); CHECK(hr == S_OK, "name %lx", hr);
    hr = IXmlWriter_WriteNmToken(w, L"1x"); CHECK(hr == S_OK, "nmtoken %lx", hr);
    hr = IXmlWriter_WriteQualifiedName(w, L"q", L"urn:y"); CHECK(hr == S_OK, "qname %lx", hr);
    hr = IXmlWriter_WriteEndElement(w); CHECK(hr == S_OK, "end");
    IXmlWriter_Flush(w);
    GetHGlobalFromStream(out, &hg); p = GlobalLock(hg);
    CHECK(strstr(p, "p1:a=\"b\"") && strstr(p, "p2:a=\"c\"") && strstr(p, "&amp;nm1xp2:q"), "output %s", p);
    GlobalUnlock(hg);
    IXmlWriter_Release(w); IStream_Release(out);

    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
