#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <initguid.h>
#include <msxml6.h>
#include <stdio.h>
#include <wchar.h>

static const CLSID CLSID_W60 = {0x88d96a0f,0xf192,0x11d4,{0xa6,0x5f,0x00,0x40,0x96,0x32,0x51,0xe5}};
static const CLSID CLSID_W30 = {0x3d813dfe,0x6c91,0x4a4e,{0x8f,0x41,0x04,0x34,0x6a,0x84,0x1d,0x9c}};
static const CLSID CLSID_A60 = {0x88d96a0e,0xf192,0x11d4,{0xa6,0x5f,0x00,0x40,0x96,0x32,0x51,0xe5}};
static int fails;

#define CHECK(c, ...) do { if (c) printf("ok   %s\n", #c); else { printf("FAIL %s: ", #c); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static void out_is(IMXWriter *w, const WCHAR *expect, const char *what)
{
    VARIANT v; HRESULT hr;
    VariantInit(&v);
    hr = IMXWriter_get_output(w, &v);
    if (hr == S_OK && V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), expect)) printf("ok   output %s\n", what);
    else { printf("FAIL output %s: got %ls\n", what, hr == S_OK && V_VT(&v) == VT_BSTR ? V_BSTR(&v) : L"(none)"); fails++; }
    VariantClear(&v);
}

int main(void)
{
    IMXWriter *w; ISAXContentHandler *ch; ISAXLexicalHandler *lex; ISAXDeclHandler *decl; ISAXDTDHandler *dtd;
    IVBSAXLexicalHandler *vblex; IMXAttributes *mxa; ISAXAttributes *sa; IStream *stream;
    HRESULT hr; VARIANT v; BSTR b; const WCHAR *p; int len; ULONG ref;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);

    /* --- version 6 writer --- */
    hr = CoCreateInstance(&CLSID_W60, NULL, CLSCTX_INPROC_SERVER, &IID_IMXWriter, (void **)&w);
    CHECK(hr == S_OK, "hr %lx", hr);
    IMXWriter_QueryInterface(w, &IID_ISAXContentHandler, (void **)&ch);
    IMXWriter_QueryInterface(w, &IID_ISAXLexicalHandler, (void **)&lex);
    IMXWriter_QueryInterface(w, &IID_ISAXDeclHandler, (void **)&decl);
    IMXWriter_QueryInterface(w, &IID_ISAXDTDHandler, (void **)&dtd);
    IMXWriter_QueryInterface(w, &IID_IVBSAXLexicalHandler, (void **)&vblex);
    IMXWriter_put_omitXMLDeclaration(w, VARIANT_TRUE);
    ISAXContentHandler_startDocument(ch);

    hr = ISAXContentHandler_characters(ch, NULL, 0);
    CHECK(hr == S_OK, "characters hr %lx", hr);
    hr = ISAXContentHandler_ignorableWhitespace(ch, NULL, 0);
    CHECK(hr == S_OK, "ignorableWhitespace hr %lx", hr);
    hr = ISAXLexicalHandler_comment(lex, NULL, 0);
    CHECK(hr == S_OK, "comment hr %lx", hr);
    hr = IVBSAXLexicalHandler_comment(vblex, NULL);
    CHECK(hr == E_INVALIDARG, "vb comment hr %lx", hr);
    hr = ISAXContentHandler_processingInstruction(ch, NULL, 0, NULL, 0);
    CHECK(hr == S_OK, "pi hr %lx", hr);
    out_is(w, L"<!---->\r\n<?\?>\r\n", "comment and pi");

    VariantInit(&v); IMXWriter_put_output(w, v);
    hr = ISAXLexicalHandler_startDTD(lex, NULL, 0, NULL, 0, NULL, 0);
    CHECK(hr == S_OK, "startDTD hr %lx", hr);
    hr = ISAXLexicalHandler_startDTD(lex, NULL, 0, L"pub", 3, NULL, 0);
    CHECK(hr == E_INVALIDARG, "startDTD pub hr %lx", hr);
    out_is(w, L"<!DOCTYPE  [\r\n<!DOCTYPE  PUBLIC \"pub\"", "startDTD");

    IMXWriter_put_output(w, v);
    hr = ISAXDeclHandler_elementDecl(decl, NULL, 0, NULL, 0);
    CHECK(hr == S_OK, "elementDecl hr %lx", hr);
    hr = ISAXDeclHandler_elementDecl(decl, L"name", 4, L"content", 7);
    CHECK(hr == S_OK, "elementDecl2 hr %lx", hr);
    hr = ISAXDeclHandler_attributeDecl(decl, L"e", 1, L"a", 1, L"CDATA", 5, L"#REQUIRED", 9, L"v", 1);
    CHECK(hr == S_OK, "attributeDecl hr %lx", hr);
    hr = ISAXDeclHandler_attributeDecl(decl, L"e", 1, L"a", 1, L"CDATA", 5, L"#FIXED", 6, L"v", 1);
    CHECK(hr == S_OK, "attributeDecl fixed hr %lx", hr);
    hr = ISAXDeclHandler_internalEntityDecl(decl, NULL, 0, NULL, 0);
    CHECK(hr == S_OK, "internalEntityDecl hr %lx", hr);
    hr = ISAXDeclHandler_externalEntityDecl(decl, L"name", -1, NULL, 0, NULL, 0);
    CHECK(hr == E_INVALIDARG, "externalEntityDecl -1 hr %lx", hr);
    out_is(w, L"<!ELEMENT  >\r\n<!ELEMENT name content>\r\n<!ATTLIST e a CDATA #REQUIRED>\r\n"
               "<!ATTLIST e a CDATA #FIXED \"v\">\r\n<!ENTITY  \"\">\r\n", "decls");

    IMXWriter_put_output(w, v);
    hr = ISAXDTDHandler_notationDecl(dtd, NULL, 0, NULL, 0, NULL, 0);
    CHECK(hr == E_INVALIDARG, "notationDecl hr %lx", hr);
    out_is(w, L"<!NOTATION ", "notation partial");
    ISAXContentHandler_Release(ch); ISAXLexicalHandler_Release(lex); ISAXDeclHandler_Release(decl);
    ISAXDTDHandler_Release(dtd); IVBSAXLexicalHandler_Release(vblex);
    IMXWriter_Release(w);

    /* encoding and version */
    CoCreateInstance(&CLSID_W60, NULL, CLSCTX_INPROC_SERVER, &IID_IMXWriter, (void **)&w);
    b = SysAllocString(L"");
    hr = IMXWriter_put_encoding(w, b);
    CHECK(hr == S_OK, "put_encoding empty hr %lx", hr);
    SysFreeString(b);
    hr = IMXWriter_put_version(w, NULL);
    CHECK(hr == S_OK, "put_version NULL hr %lx", hr);
    b = SysAllocString(L"2.0"); IMXWriter_put_version(w, b); SysFreeString(b);
    b = SysAllocString(L""); IMXWriter_put_version(w, b); SysFreeString(b);
    IMXWriter_get_version(w, &b);
    CHECK(!wcscmp(b, L"1.0"), "version %ls", b); SysFreeString(b);
    b = SysAllocString(L"UTF-8"); IMXWriter_put_encoding(w, b); SysFreeString(b);
    IMXWriter_QueryInterface(w, &IID_ISAXContentHandler, (void **)&ch);
    ISAXContentHandler_startDocument(ch);
    out_is(w, L"<?xml version=\"1.0\" standalone=\"no\"?>\r\n", "no encoding when chosen");
    ISAXContentHandler_Release(ch);

    /* stream is referenced twice by the writer */
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    VariantInit(&v); V_VT(&v) = VT_UNKNOWN; V_UNKNOWN(&v) = (IUnknown *)stream;
    hr = IMXWriter_put_output(w, v);
    IStream_AddRef(stream); ref = IStream_Release(stream);
    CHECK(ref == 3, "stream refs %lu", ref);
    VariantInit(&v); IMXWriter_put_output(w, v);
    IStream_AddRef(stream); ref = IStream_Release(stream);
    CHECK(ref == 1, "stream refs after detach %lu", ref);
    IStream_Release(stream);
    IMXWriter_Release(w);

    /* version 3 keeps its rules */
    hr = CoCreateInstance(&CLSID_W30, NULL, CLSCTX_INPROC_SERVER, &IID_IMXWriter, (void **)&w);
    CHECK(hr == S_OK, "create v3 hr %lx", hr);
    if (hr != S_OK) return 1;
    IMXWriter_QueryInterface(w, &IID_ISAXContentHandler, (void **)&ch);
    hr = ISAXContentHandler_characters(ch, NULL, 0);
    CHECK(hr == E_INVALIDARG, "v3 characters hr %lx", hr);
    ISAXContentHandler_Release(ch);
    IMXWriter_Release(w);

    /* attributes */
    CoCreateInstance(&CLSID_A60, NULL, CLSCTX_INPROC_SERVER, &IID_IMXAttributes, (void **)&mxa);
    IMXAttributes_QueryInterface(mxa, &IID_ISAXAttributes, (void **)&sa);
    hr = ISAXAttributes_getQName(sa, 0, NULL, NULL);
    CHECK(hr == E_POINTER, "getQName null hr %lx", hr);
    p = (void *)0xdeadbeef; len = -1;
    hr = ISAXAttributes_getQName(sa, 0, &p, &len);
    CHECK(hr == E_INVALIDARG && !p && !len, "getQName bad index hr %lx p %p len %d", hr, p, len);
    IMXAttributes_Release(mxa); ISAXAttributes_Release(sa);

    printf("%s\nDONE\n", fails ? "RESULT: FAIL" : "RESULT: PASS");
    return 0;
}
