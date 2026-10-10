/* inetcomm batch (patches/sg/2019), run by test/mimeopt-gate.sh. Families
 * (from Wine's todo_wine blocks, which record Windows): the scalar options of
 * IMimeMessage (SetOption / GetOption: stored per message, wrong type ignored,
 * bad ids refused), and CombineUrl of the mhtml protocol with a relative URL
 * that is itself an mhtml URL.
 *
 *   mimeopt-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <urlmon.h>
#include <wininet.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifndef ICU_BROWSER_MODE
#define ICU_BROWSER_MODE 0x02000000
#endif
HRESULT WINAPI MimeOleCreateMessage(IUnknown *, void **);

#define MIME_E_INVALID_OPTION_ID ((HRESULT)0x800cce20)
static const GUID SG_CLSID_MimeHtmlProtocol = { 0x5300401, 0xbcbc, 0x11d0, { 0x85, 0xe3, 0x0, 0xc0, 0x4f, 0xd8, 0x5a, 0xb4 } };

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef HRESULT (WINAPI *setopt_t)(void *, DWORD, const PROPVARIANT *);
typedef HRESULT (WINAPI *getopt_t)(void *, DWORD, PROPVARIANT *);

static HRESULT set_option(void *msg, DWORD oid, const PROPVARIANT *pv) { return ((setopt_t *)*(void **)msg)[36](msg, oid, pv); }
static HRESULT get_option(void *msg, DWORD oid, PROPVARIANT *pv) { return ((getopt_t *)*(void **)msg)[37](msg, oid, pv); }
static ULONG release(void *msg) { return ((ULONG (WINAPI **)(void *))*(void **)msg)[2](msg); }

#define OID(id, vt) (((id) << 16) | (vt))

static const struct { const char *name; DWORD id; VARTYPE vt; } options[] =
{
    { "ALLOW_8BIT_HEADER", 0x01, VT_BOOL }, { "CBMAX_HEADER_LINE", 0x02, VT_UI4 }, { "SAVE_FORMAT", 0x03, VT_UI4 },
    { "WRAP_BODY_TEXT", 0x04, VT_BOOL }, { "CBMAX_BODY_LINE", 0x05, VT_UI4 }, { "TRANSMIT_BODY_ENCODING", 0x06, VT_UI4 },
    { "TRANSMIT_TEXT_ENCODING", 0x07, VT_UI4 }, { "GENERATE_MESSAGE_ID", 0x08, VT_BOOL }, { "HIDE_TNEF_ATTACHMENTS", 0x0e, VT_BOOL },
    { "CLEANUP_TREE_ON_SAVE", 0x0f, VT_BOOL }, { "BODY_REMOVE_NBSP", 0x14, VT_BOOL }, { "DEFAULT_BODY_CHARSET", 0x15, VT_UI4 },
    { "DEFAULT_HEADER_CHARSET", 0x16, VT_UI4 }, { "DBCS_ESCAPE_IS_8BIT", 0x17, VT_BOOL }, { "SECURITY_TYPE", 0x18, VT_UI4 },
    { "SECURITY_HWND_OWNER", 0x32, VT_UI4 }, { "HEADER_RELOAD_TYPE", 0x37, VT_UI4 }, { "CAN_INLINE_TEXT_BODIES", 0x38, VT_BOOL },
    { "SHOW_MACBINARY", 0x39, VT_BOOL }, { "SAVEBODY_KEEPBOUNDARY", 0x40, VT_BOOL },
};

static void test_options(void)
{
    void *msg = NULL, *msg2 = NULL;
    PROPVARIANT pv;
    HRESULT hr;
    char buf[200];
    unsigned i;

    MimeOleCreateMessage(NULL, &msg);
    MimeOleCreateMessage(NULL, &msg2);
    check(msg && msg2, "two messages");
    if (!msg || !msg2) return;

    for (i = 0; i < sizeof(options) / sizeof(options[0]); i++)
    {
        DWORD oid = OID(options[i].id, options[i].vt);
        DWORD value = options[i].vt == VT_BOOL ? 1 : 0x1234 + i;

        memset(&pv, 0xcc, sizeof(pv));
        hr = get_option(msg, oid, &pv);
        snprintf(buf, sizeof(buf), "%s unset: right type, zero (hr %08lx vt %x)", options[i].name, (unsigned long)hr, pv.vt);
        check(hr == S_OK && pv.vt == options[i].vt && (options[i].vt == VT_BOOL ? pv.boolVal == 0 : pv.ulVal == 0), buf);

        memset(&pv, 0, sizeof(pv));
        pv.vt = options[i].vt;
        if (pv.vt == VT_BOOL) pv.boolVal = value; else pv.ulVal = value;
        hr = set_option(msg, oid, &pv);
        snprintf(buf, sizeof(buf), "%s set (hr %08lx)", options[i].name, (unsigned long)hr);
        check(hr == S_OK, buf);

        memset(&pv, 0xcc, sizeof(pv));
        hr = get_option(msg, oid, &pv);
        snprintf(buf, sizeof(buf), "%s reads back", options[i].name);
        check(hr == S_OK && pv.vt == options[i].vt && (options[i].vt == VT_BOOL ? pv.boolVal == (VARIANT_BOOL)value : pv.ulVal == value), buf);

        memset(&pv, 0xcc, sizeof(pv));
        get_option(msg2, oid, &pv);
        snprintf(buf, sizeof(buf), "%s of the other message is unchanged", options[i].name);
        check(pv.vt == options[i].vt && (options[i].vt == VT_BOOL ? pv.boolVal == 0 : pv.ulVal == 0), buf);
    }

    /* a value of the wrong type is accepted and ignored */
    memset(&pv, 0, sizeof(pv));
    pv.vt = VT_LPSTR;
    pv.pszVal = (char *)"XXXXX";
    hr = set_option(msg, OID(0x0e, VT_BOOL), &pv);
    check(hr == S_OK, "a string for a BOOL option: S_OK");
    memset(&pv, 0, sizeof(pv));
    get_option(msg, OID(0x0e, VT_BOOL), &pv);
    check(pv.vt == VT_BOOL && pv.boolVal == 1, "and the value stays");
    memset(&pv, 0, sizeof(pv));
    pv.vt = VT_BOOL;
    pv.boolVal = 0;
    set_option(msg, OID(0x0e, VT_BOOL), &pv);
    memset(&pv, 0xcc, sizeof(pv));
    get_option(msg, OID(0x0e, VT_BOOL), &pv);
    check(pv.vt == VT_BOOL && pv.boolVal == 0, "a BOOL can be cleared again");

    /* ids that are not options */
    memset(&pv, 0, sizeof(pv));
    pv.vt = VT_BOOL;
    pv.boolVal = 1;
    hr = set_option(msg, 0xff00000a, &pv);
    check(hr == MIME_E_INVALID_OPTION_ID, "SetOption of an id out of range");
    pv.vt = VT_I4;
    hr = set_option(msg, 0xff00000a, &pv);
    check(hr == MIME_E_INVALID_OPTION_ID, "out of range is checked before the type");
    hr = get_option(msg, 0xff00000a, &pv);
    check(hr == MIME_E_INVALID_OPTION_ID, "GetOption of an id out of range");
    hr = get_option(msg, OID(0x00, VT_BOOL), &pv);
    check(hr == MIME_E_INVALID_OPTION_ID, "GetOption of id 0");
    hr = get_option(msg, OID(0x4d, VT_BOOL), &pv);
    check(hr == MIME_E_INVALID_OPTION_ID, "GetOption of an id that is not defined");
    pv.vt = VT_BOOL;
    hr = set_option(msg, OID(0x4d, VT_BOOL), &pv);
    check(hr == MIME_E_INVALID_OPTION_ID, "SetOption of an id that is not defined");

    release(msg);
    release(msg2);
}

static const struct { const WCHAR *base, *relative, *expected; } combine[] =
{
    { L"mhtml:file:///c:/dir/test.mht", L"http://test.org", L"mhtml:file:///c:/dir/test.mht!x-usc:http://test.org" },
    { L"mhtml:file:///c:/dir/test.mht", L"123abc", L"mhtml:file:///c:/dir/test.mht!x-usc:123abc" },
    { L"mhtml:file:///c:/dir/test.mht!x-usc:http://test.org", L"123abc", L"mhtml:file:///c:/dir/test.mht!x-usc:123abc" },
    { L"mhtml:file:///c:/dir/test.mht!x-usc:http://test.org", L"", L"mhtml:file:///c:/dir/test.mht" },
    { L"mhtml:file:///c:/dir/test.mht!x-usc:http://test.org", L"mhtml:file:///d:/file.html", L"file:///d:/file.html" },
    { L"mhtml:file:///c:/dir/test.mht!x-usc:http://test.org", L"mhtml:file:///c:/dir2/test.mht!x-usc:http://test.org",
      L"mhtml:file:///c:/dir2/test.mht!x-usc:http://test.org" },
    { L"mhtml:file:///c:/dir/test.mht", L"MHTML:file:///d:/x.txt", L"file:///d:/x.txt" },
    { L"mhtml:file:///c:/dir/test.mht!http://test.org", L"123abc", L"mhtml:file:///c:/dir/test.mht!x-usc:123abc" },
    { L"mhtml:file:///c:/dir/test.mht!http://test.org", L"", L"mhtml:file:///c:/dir/test.mht" },
};

static void test_combine(void)
{
    IInternetProtocolInfo *info = NULL;
    WCHAR out[300];
    DWORD len;
    HRESULT hr;
    char buf[300];
    unsigned i;

    hr = CoCreateInstance(&SG_CLSID_MimeHtmlProtocol, NULL, CLSCTX_INPROC_SERVER, &IID_IInternetProtocolInfo, (void **)&info);
    check(hr == S_OK && info, "create the mhtml protocol");
    if (!info) return;

    for (i = 0; i < sizeof(combine) / sizeof(combine[0]); i++)
    {
        len = 0xdeadbeef;
        wcscpy(out, L"unset");
        hr = IInternetProtocolInfo_CombineUrl(info, combine[i].base, combine[i].relative, ICU_BROWSER_MODE, out, ARRAYSIZE(out), &len, 0);
        snprintf(buf, sizeof(buf), "CombineUrl %u: %ls + %ls (hr %08lx, got %ls)", i, combine[i].base, combine[i].relative, (unsigned long)hr, out);
        check(hr == S_OK && !wcscmp(out, combine[i].expected) && len == wcslen(combine[i].expected), buf);
    }

    /* a result that does not fit */
    len = 0xdeadbeef;
    hr = IInternetProtocolInfo_CombineUrl(info, combine[4].base, combine[4].relative, ICU_BROWSER_MODE, out, 5, &len, 0);
    check(FAILED(hr) && len == 0, "a result that does not fit");

    IInternetProtocolInfo_Release(info);
}

int main(void)
{
    CoInitialize(NULL);
    test_options();
    test_combine();
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    CoUninitialize();
    return failures != 0;
}
