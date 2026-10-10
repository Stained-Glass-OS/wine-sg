/* msxml3 batch (patches/sg/2030), run by test/dtdcode-gate.sh: the error code
 * IXMLDOMDocument::validate reports for each kind of DTD violation (from
 * Wine's domdoc tests, which record the codes of the real library). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ocidl.h>
#include <msxml6.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static const GUID SG_CLSID_DOMDocument30 = { 0xf5078f32, 0xc551, 0x11d3, { 0x89, 0xb9, 0x00, 0x00, 0xf8, 0x1f, 0xe2, 0x21 } };
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

#define DTD L"<!DOCTYPE email [" \
 L"<!ELEMENT email (recipients,from,subject,body,attachment*)>" \
 L"<!ATTLIST email attachments IDREFS #REQUIRED>" \
 L"<!ELEMENT recipients (to+,cc*)>" \
 L"<!ELEMENT to (#PCDATA)>" \
 L"<!ELEMENT cc (#PCDATA)>" \
 L"<!ELEMENT from (#PCDATA)>" \
 L"<!ATTLIST from name CDATA #IMPLIED>" \
 L"<!ELEMENT subject ANY>" \
 L"<!ELEMENT body ANY>" \
 L"<!ATTLIST body enc CDATA #FIXED \"UTF-8\">" \
 L"<!ELEMENT attachment (#PCDATA)>" \
 L"<!ATTLIST attachment id ID #REQUIRED>" \
 L"]>"

#define OKHEAD L"<email attachments=\"a1\"><recipients><to>x</to></recipients><from name=\"n\">u</from><subject>s</subject>"
#define OKTAIL L"<attachment id=\"a1\">f</attachment></email>"

struct tcase { const char *what; const WCHAR *xml; LONG code; };

static const struct tcase cases[] = {
    { "valid document", L"<?xml version=\"1.0\"?>" DTD OKHEAD L"<body>b</body>" OKTAIL, 0 },
    { "undeclared element", L"<?xml version=\"1.0\"?>" DTD OKHEAD L"<body><undecl/></body>" OKTAIL, 0xC00CE00D },
    { "idref without id", L"<?xml version=\"1.0\"?>" DTD
      L"<email attachments=\"nope\"><recipients><to>x</to></recipients><from>u</from><subject>s</subject><body>b</body>"
      L"<attachment id=\"a1\">f</attachment></email>", 0xC00CE00E },
    { "empty where content is required", L"<?xml version=\"1.0\"?>" DTD
      L"<email attachments=\"a1\"><recipients></recipients><from>u</from><subject>s</subject><body>b</body>"
      L"<attachment id=\"a1\">f</attachment></email>", 0xC00CE011 },
    { "root name", L"<?xml version=\"1.0\"?>" DTD L"<emails attachments=\"a1\"/>", 0xC00CE013 },
    { "invalid content", L"<?xml version=\"1.0\"?>" DTD
      L"<email attachments=\"a1\"><from>u</from><recipients><to>x</to></recipients><subject>s</subject><body>b</body>"
      L"<attachment id=\"a1\">f</attachment></email>", 0xC00CE014 },
    { "undefined attribute", L"<?xml version=\"1.0\"?>" DTD
      L"<email attachments=\"a1\"><recipients><to>x</to></recipients><from nme=\"n\">u</from><subject>s</subject><body>b</body>"
      L"<attachment id=\"a1\">f</attachment></email>", 0xC00CE015 },
    { "fixed attribute value", L"<?xml version=\"1.0\"?>" DTD OKHEAD L"<body enc=\"latin1\">b</body>" OKTAIL, 0xC00CE016 },
    { "text in element content", L"<?xml version=\"1.0\"?>" DTD
      L"<email attachments=\"a1\">oops<recipients><to>x</to></recipients><from>u</from><subject>s</subject><body>b</body>"
      L"<attachment id=\"a1\">f</attachment></email>", 0xC00CE018 },
    { "missing required attribute", L"<?xml version=\"1.0\"?>" DTD OKHEAD L"<body>b</body><attachment>f</attachment></email>", 0xC00CE020 },
};

int main(void)
{
    unsigned i;
    CoInitialize(NULL);
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        IXMLDOMDocument2 *doc = NULL;
        IXMLDOMParseError *err = NULL;
        VARIANT_BOOL ok = VARIANT_FALSE;
        BSTR xml = SysAllocString(cases[i].xml);
        LONG code = -1;
        HRESULT hr;
        char buf[200];

        CoCreateInstance(&SG_CLSID_DOMDocument30, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument2, (void **)&doc);
        IXMLDOMDocument2_put_validateOnParse(doc, VARIANT_FALSE);
        IXMLDOMDocument2_loadXML(doc, xml, &ok);
        hr = IXMLDOMDocument2_validate(doc, &err);
        if (err) IXMLDOMParseError_get_errorCode(err, &code);
        snprintf(buf, sizeof(buf), "%s", cases[i].what);
        if (code != cases[i].code) printf("   hr %08lx code %08lx\n", (unsigned long)hr, (unsigned long)code);
        check(ok == VARIANT_TRUE && code == cases[i].code && (cases[i].code ? hr == S_FALSE : hr == S_OK), buf);
        if (err) IXMLDOMParseError_Release(err);
        IXMLDOMDocument2_Release(doc);
        SysFreeString(xml);
    }
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    CoUninitialize();
    return failures != 0;
}
