/* inetcomm batch: streams, options, address table, schema, security, message tree, offset tables,
 * range lists, IMAP (against a scripted server), SMTP/POP3 additions, mhtml protocol. */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ole2.h>
#include <initguid.h>
#include <urlmon.h>
#include <wincrypt.h>
#include <imnxport.h>
#include <mimeole.h>

#define COMMIT_ONLYIFDIRTY 1

/* mingw has no import library for inetcomm */
static HRESULT (WINAPI *pMimeOleCreateMessage)(IUnknown *, IMimeMessage **);
static HRESULT (WINAPI *pMimeOleGetAllocator)(IMimeAllocator **);
static HRESULT (WINAPI *pMimeGetAddressFormatW)(REFIID, void *, DWORD, ADDRESSFORMAT, WCHAR **);
static HRESULT (WINAPI *pMimeOleGetPropertySchema)(IMimePropertySchema **);
static HRESULT (WINAPI *pMimeOleCreateSecurity)(IMimeSecurity **);
static HRESULT (WINAPI *pMimeOleFindCharset)(LPCSTR, LPHCHARSET);
static HRESULT (WINAPI *pCreateRangeList)(IRangeList **);
static HRESULT (WINAPI *pCreateIMAPTransport)(IIMAPTransport **);
static HRESULT (WINAPI *pCreateSMTPTransport)(ISMTPTransport **);
static HRESULT (WINAPI *pCreatePOP3Transport)(IPOP3Transport **);
#define MimeOleCreateMessage pMimeOleCreateMessage
#define MimeOleGetAllocator pMimeOleGetAllocator
#define MimeGetAddressFormatW pMimeGetAddressFormatW
#define MimeOleGetPropertySchema pMimeOleGetPropertySchema
#define MimeOleCreateSecurity pMimeOleCreateSecurity
#define MimeOleFindCharset pMimeOleFindCharset
#define CreateRangeList pCreateRangeList
#define CreateIMAPTransport pCreateIMAPTransport
#define CreateSMTPTransport pCreateSMTPTransport
#define CreatePOP3Transport pCreatePOP3Transport

static int checks, bad;
#define CHECK(c, ...) do { checks++; if (!(c)) { bad++; printf("FAIL  line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define HR(h) ((h) & 0xffffffffu)

static IStream *mem_stream(const char *text)
{
    IStream *s;
    CreateStreamOnHGlobal(NULL, TRUE, &s);
    IStream_Write(s, text, strlen(text), NULL);
    IStream_Seek(s, (LARGE_INTEGER){{0}}, STREAM_SEEK_SET, NULL);
    return s;
}

static char *stream_text(IStream *s)
{
    static char buf[16384];
    ULONG n = 0;
    IStream_Seek(s, (LARGE_INTEGER){{0}}, STREAM_SEEK_SET, NULL);
    IStream_Read(s, buf, sizeof(buf) - 1, &n);
    buf[n] = 0;
    return buf;
}

static char *message_text(IMimeMessage *msg)
{
    IStream *s;
    char *t;
    static char out[16384];
    if (FAILED(IMimeMessage_GetMessageSource(msg, &s, 0))) return "(no source)";
    t = stream_text(s);
    strcpy(out, t);
    IStream_Release(s);
    return out;
}

static const char multipart_msg[] =
    "From: Alice Example <alice@example.com>\r\n"
    "To: \"Bob, the Builder\" <bob@example.com>, carol@example.com (Carol C)\r\n"
    "Cc: group: dan@example.com, eve@example.com;\r\n"
    "Subject: test\r\n"
    "MIME-Version: 1.0\r\n"
    "Content-Type: multipart/mixed; boundary=\"XX\"\r\n"
    "\r\n"
    "--XX\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "hello world\r\n"
    "--XX\r\n"
    "Content-Type: text/html\r\n"
    "Content-Location: http://example.com/page.html\r\n"
    "\r\n"
    "<b>hi</b>\r\n"
    "--XX--\r\n";

static void test_streams_and_bodies(void)
{
    IMimeMessage *msg;
    IMimeBody *body;
    IStream *data, *clone;
    HBODY h;
    FINDBODY fb = {0};
    HRESULT hr;
    ULONG n;
    char buf[8];
    STATSTG st;
    PROPVARIANT pv;
    TRANSMITINFO ti;

    MimeOleCreateMessage(NULL, &msg);
    {
        IStream *src = mem_stream(multipart_msg);
        IPersistStreamInit *psi;
        IMimeMessage_QueryInterface(msg, &IID_IPersistStreamInit, (void **)&psi);
        hr = IPersistStreamInit_Load(psi, src);
        CHECK(hr == S_OK, "load %08lx", HR(hr));
        IPersistStreamInit_Release(psi);
        IStream_Release(src);
    }
    hr = IMimeMessage_FindFirst(msg, &fb, &h);
    CHECK(hr == S_OK, "FindFirst %08lx", HR(hr));
    hr = IMimeMessage_FindNext(msg, &fb, &h);
    hr = IMimeMessage_FindNext(msg, &fb, &h);
    CHECK(hr == S_OK, "FindNext %08lx", HR(hr));
    hr = IMimeMessage_BindToObject(msg, h, &IID_IMimeBody, (void **)&body);
    CHECK(hr == S_OK, "bind %08lx", HR(hr));
    hr = IMimeBody_GetData(body, IET_BINARY, &data);
    CHECK(hr == S_OK, "GetData %08lx", HR(hr));

    /* the data of a loaded body is a read-only window on the message */
    hr = IStream_Write(data, "x", 1, &n);
    CHECK(hr == STG_E_ACCESSDENIED, "sub Write %08lx", HR(hr));
    hr = IStream_SetSize(data, (ULARGE_INTEGER){{5}});
    CHECK(hr == STG_E_ACCESSDENIED, "sub SetSize %08lx", HR(hr));
    CHECK(IStream_Commit(data, 0) == S_OK, "sub Commit");
    CHECK(IStream_Revert(data) == S_OK, "sub Revert");
    hr = IStream_LockRegion(data, (ULARGE_INTEGER){{0}}, (ULARGE_INTEGER){{1}}, LOCK_WRITE);
    CHECK(hr == STG_E_INVALIDFUNCTION, "sub Lock %08lx", HR(hr));
    hr = IStream_UnlockRegion(data, (ULARGE_INTEGER){{0}}, (ULARGE_INTEGER){{1}}, LOCK_WRITE);
    CHECK(hr == STG_E_INVALIDFUNCTION, "sub Unlock %08lx", HR(hr));
    hr = IStream_Stat(data, &st, STATFLAG_NONAME);
    CHECK(hr == S_OK && st.type == STGTY_STREAM && st.grfMode == STGM_READ && st.cbSize.LowPart > 0,
          "sub Stat %08lx type %lu mode %lx", HR(hr), st.type, st.grfMode);
    memset(buf, 0, sizeof(buf));
    IStream_Read(data, buf, 3, &n);
    CHECK(n == 3 && !memcmp(buf, "<b>", 3), "sub read '%s'", buf);
    hr = IStream_Clone(data, &clone);
    CHECK(hr == S_OK && clone, "sub Clone %08lx", HR(hr));
    if (clone)
    {
        memset(buf, 0, sizeof(buf));
        IStream_Read(clone, buf, 2, &n);
        CHECK(n == 2 && !memcmp(buf, "hi", 2), "clone continues at the position: '%s'", buf);
        IStream_Read(data, buf, 2, &n);
        CHECK(n == 2 && !memcmp(buf, "hi", 2), "original keeps its own position: '%s'", buf);
        IStream_Release(clone);
    }
    IStream_Release(data);

    /* IsType, transmit info */
    CHECK(IMimeBody_IsType(body, IBT_EMPTY) == S_FALSE, "IBT_EMPTY");
    hr = IMimeBody_IsType(body, 42);
    CHECK(HR(hr) == 0x800cce2e, "IsType(42) %08lx", HR(hr));
    hr = IMimeBody_GetTransmitInfo(body, NULL);
    CHECK(hr == E_INVALIDARG, "xmit NULL %08lx", HR(hr));
    memset(&ti, 0xaa, sizeof(ti));
    hr = IMimeBody_GetTransmitInfo(body, &ti);
    CHECK(hr == S_OK && ti.ietXmitMime == IET_7BIT && ti.cbSize == 9 && ti.cLines == 1 && ti.cExtended == 0 &&
          ti.cbLongestLine == 9, "xmit %08lx mime %d size %lu lines %lu ext %lu longest %lu", HR(hr),
          ti.ietXmitMime, ti.cbSize, ti.cLines, ti.cExtended, ti.cbLongestLine);

    /* options of a body */
    PropVariantInit(&pv);
    pv.vt = VT_BOOL; pv.boolVal = VARIANT_TRUE;
    hr = IMimeBody_SetOption(body, OID_HIDE_TNEF_ATTACHMENTS, &pv);
    CHECK(hr == S_OK, "body SetOption %08lx", HR(hr));
    PropVariantInit(&pv);
    hr = IMimeBody_GetOption(body, OID_HIDE_TNEF_ATTACHMENTS, &pv);
    CHECK(hr == S_OK && pv.vt == VT_BOOL && pv.boolVal, "body GetOption %08lx vt %d", HR(hr), pv.vt);
    pv.vt = VT_UI4; pv.ulVal = 5;
    hr = IMimeBody_SetOption(body, OID_CBMAX_HEADER_LINE, &pv);
    PropVariantInit(&pv);
    hr = IMimeBody_GetOption(body, OID_CBMAX_HEADER_LINE, &pv);
    CHECK(hr == S_OK && pv.ulVal == 5, "body ulong option %08lx %lu", HR(hr), pv.ulVal);
    pv.vt = VT_BOOL; pv.boolVal = 1;
    hr = IMimeBody_SetOption(body, 0xff00000a, &pv);
    CHECK(HR(hr) == 0x800cce20, "body SetOption bad id %08lx", HR(hr));
    hr = IMimeBody_GetOption(body, 0xff00000a, &pv);
    CHECK(HR(hr) == 0x800cce20, "body GetOption bad id %08lx", HR(hr));
    PropVariantInit(&pv);
    hr = IMimeBody_GetOption(body, OID_SECURITY_2KEY_CERT_BAG, &pv);
    CHECK(hr == S_OK && pv.vt == (VT_VECTOR | VT_UI4) && pv.caul.cElems == 0, "empty cert bag %08lx vt %x", HR(hr), pv.vt);
    {
        ULONG vals[3] = {7, 8, 9};
        PROPVARIANT set;
        PropVariantInit(&set);
        set.vt = VT_VECTOR | VT_UI4;
        set.caul.cElems = 3;
        set.caul.pElems = vals;
        hr = IMimeBody_SetOption(body, OID_SECURITY_2KEY_CERT_BAG, &set);
        CHECK(hr == S_OK, "set bag %08lx", HR(hr));
        PropVariantInit(&pv);
        hr = IMimeBody_GetOption(body, OID_SECURITY_2KEY_CERT_BAG, &pv);
        CHECK(hr == S_OK && pv.caul.cElems == 3 && pv.caul.pElems[2] == 9, "bag back %08lx", HR(hr));
        PropVariantClear(&pv);
        hr = IMimeMessage_SetOption(msg, OID_SECURITY_2KEY_CERT_BAG, &set);
        CHECK(hr == S_OK, "message set bag %08lx", HR(hr));
        PropVariantInit(&pv);
        hr = IMimeMessage_GetOption(msg, OID_SECURITY_2KEY_CERT_BAG, &pv);
        CHECK(hr == S_OK && pv.caul.cElems == 3 && pv.caul.pElems[0] == 7, "message bag back %08lx", HR(hr));
        PropVariantClear(&pv);
    }
    /* BindToObject of the property set */
    {
        IMimePropertySet *props = NULL;
        hr = IMimeBody_BindToObject(body, &IID_IMimePropertySet, (void **)&props);
        CHECK(hr == S_OK && props, "BindToObject %08lx", HR(hr));
        if (props) IMimePropertySet_Release(props);
        hr = IMimeBody_BindToObject(body, &IID_IMimeSecurity, (void **)&clone);
        CHECK(hr == E_NOINTERFACE, "BindToObject other %08lx", HR(hr));
    }
    IMimeBody_Release(body);

    /* a body that is multipart has no data to transmit */
    hr = IMimeMessage_BindToObject(msg, HBODY_ROOT, &IID_IMimeBody, (void **)&body);
    hr = IMimeBody_GetTransmitInfo(body, &ti);
    CHECK(hr != S_OK, "multipart transmit info %08lx", HR(hr));
    IMimeBody_Release(body);
    IMimeMessage_Release(msg);

    {
        IMimeMessage *agg;
        hr = MimeOleCreateMessage((IUnknown *)msg, &agg);
        CHECK(HR(hr) == 0x80040110 || hr == S_OK, "aggregation %08lx", HR(hr));
    }
}

static void test_address_table(void)
{
    IMimeMessage *msg;
    IMimeAddressTable *table, *clone;
    IMimeEnumAddressTypes *en, *en2;
    ADDRESSPROPS props = {0};
    ADDRESSLIST list;
    HADDRESS h;
    ULONG count, fetched;
    char *text;
    WCHAR *wtext;
    HRESULT hr;
    PROPVARIANT pv;
    IMimeBody *body;
    IStream *src = mem_stream(multipart_msg);
    IPersistStreamInit *psi;

    MimeOleCreateMessage(NULL, &msg);
    IMimeMessage_QueryInterface(msg, &IID_IPersistStreamInit, (void **)&psi);
    IPersistStreamInit_Load(psi, src);
    IPersistStreamInit_Release(psi);
    IStream_Release(src);

    hr = IMimeMessage_GetAddressTable(msg, &table);
    CHECK(hr == S_OK && table, "GetAddressTable %08lx", HR(hr));
    if (!table) { IMimeMessage_Release(msg); return; }

    hr = IMimeAddressTable_CountTypes(table, IAT_TO, &count);
    CHECK(hr == S_OK && count == 2, "To count %08lx %lu", HR(hr), count);
    hr = IMimeAddressTable_CountTypes(table, IAT_CC, &count);
    CHECK(hr == S_OK && count == 2, "Cc count (group members) %08lx %lu", HR(hr), count);
    hr = IMimeAddressTable_CountTypes(table, IAT_ALL, &count);
    CHECK(count == 5, "all count %lu", count);
    hr = IMimeAddressTable_CountTypes(table, IAT_TO, NULL);
    CHECK(hr == E_INVALIDARG, "count NULL %08lx", HR(hr));

    props.dwProps = IAP_FRIENDLY | IAP_EMAIL | IAP_ADRTYPE | IAP_HANDLE;
    hr = IMimeAddressTable_GetSender(table, &props);
    CHECK(hr == S_OK && !strcmp(props.pszEmail, "alice@example.com") && !strcmp(props.pszFriendly, "Alice Example") &&
          props.dwAdrType == IAT_FROM, "sender %08lx", HR(hr));
    {
        IMimeAllocator *alloc;
        MimeOleGetAllocator(&alloc);
        IMimeAllocator_FreeAddressProps(alloc, &props);
        /* the Sender header only counts when there is no From */
        IMimeAddressTable_Append(table, IAT_SENDER, IET_DECODED, "Sam", "sam@example.com", NULL);
        memset(&props, 0, sizeof(props));
        props.dwProps = IAP_EMAIL;
        hr = IMimeAddressTable_GetSender(table, &props);
        CHECK(hr == S_OK && !strcmp(props.pszEmail, "alice@example.com"), "From wins over Sender: %s", hr == S_OK ? props.pszEmail : "");
        IMimeAllocator_FreeAddressProps(alloc, &props);
        IMimeAddressTable_DeleteTypes(table, IAT_SENDER);
        IMimeAllocator_Release(alloc);
    }

    memset(&list, 0, sizeof(list));
    hr = IMimeAddressTable_GetTypes(table, IAT_TO, IAP_FRIENDLY | IAP_EMAIL, &list);
    CHECK(hr == S_OK && list.cAdrs == 2 && !strcmp(list.prgAdr[0].pszFriendly, "Bob, the Builder") &&
          !strcmp(list.prgAdr[0].pszEmail, "bob@example.com") && !strcmp(list.prgAdr[1].pszFriendly, "Carol C") &&
          !strcmp(list.prgAdr[1].pszEmail, "carol@example.com"), "GetTypes To %08lx n=%lu", HR(hr), list.cAdrs);
    h = list.cAdrs ? NULL : NULL;
    {
        IMimeAllocator *alloc;
        MimeOleGetAllocator(&alloc);
        IMimeAllocator_FreeAddressList(alloc, &list);
        IMimeAllocator_Release(alloc);
    }

    hr = IMimeAddressTable_GetFormat(table, IAT_TO, AFT_DISPLAY_FRIENDLY, &text);
    CHECK(hr == S_OK && !strcmp(text, "Bob, the Builder; Carol C"), "friendly '%s'", hr == S_OK ? text : "");
    if (hr == S_OK) CoTaskMemFree(text);
    hr = IMimeAddressTable_GetFormat(table, IAT_TO, AFT_DISPLAY_EMAIL, &text);
    CHECK(hr == S_OK && !strcmp(text, "bob@example.com; carol@example.com"), "email '%s'", hr == S_OK ? text : "");
    if (hr == S_OK) CoTaskMemFree(text);
    hr = IMimeAddressTable_GetFormat(table, IAT_TO, AFT_DISPLAY_BOTH, &text);
    CHECK(hr == S_OK && !strcmp(text, "Bob, the Builder <bob@example.com>; Carol C <carol@example.com>"), "both '%s'",
          hr == S_OK ? text : "");
    if (hr == S_OK) CoTaskMemFree(text);
    hr = IMimeAddressTable_GetFormat(table, IAT_TO, AFT_RFC822_DECODED, &text);
    CHECK(hr == S_OK && !strcmp(text, "\"Bob, the Builder\" <bob@example.com>, Carol C <carol@example.com>"), "rfc822 '%s'",
          hr == S_OK ? text : "");
    if (hr == S_OK) CoTaskMemFree(text);
    hr = IMimeAddressTable_GetFormat(table, IAT_BCC, AFT_DISPLAY_EMAIL, &text);
    CHECK(HR(hr) == 0x800cce05, "no Bcc %08lx", HR(hr));
    hr = IMimeAddressTable_GetFormat(table, IAT_TO, AFT_DISPLAY_EMAIL, NULL);
    CHECK(hr == E_INVALIDARG, "format NULL %08lx", HR(hr));

    /* changes reach the headers */
    hr = IMimeAddressTable_Append(table, IAT_BCC, IET_DECODED, "Frank F", "frank@example.com", &h);
    CHECK(hr == S_OK && h, "Append %08lx", HR(hr));
    hr = IMimeAddressTable_Append(table, IAT_TO | IAT_CC, IET_DECODED, "x", "x@example.com", NULL);
    CHECK(HR(hr) == 0x800cce05, "Append two types %08lx", HR(hr));
    hr = IMimeAddressTable_Append(table, IAT_BCC, IET_DECODED, "x", "", NULL);
    CHECK(hr == E_INVALIDARG, "Append empty email %08lx", HR(hr));
    IMimeMessage_BindToObject(msg, HBODY_ROOT, &IID_IMimeBody, (void **)&body);
    PropVariantInit(&pv);
    pv.vt = VT_LPSTR;
    hr = IMimeBody_GetProp(body, "Bcc", 0, &pv);
    CHECK(hr == S_OK && strstr(pv.pszVal, "frank@example.com"), "Bcc header '%s'", hr == S_OK ? pv.pszVal : "");
    PropVariantClear(&pv);

    props.dwProps = IAP_EMAIL;
    hr = IMimeAddressTable_GetProps(table, h, &props);
    CHECK(hr == S_OK && !strcmp(props.pszEmail, "frank@example.com"), "GetProps");
    CoTaskMemFree(props.pszEmail);
    hr = IMimeAddressTable_GetProps(table, (HADDRESS)0x7777, &props);
    CHECK(HR(hr) == 0x800cce09, "GetProps bad handle %08lx", HR(hr));
    memset(&props, 0, sizeof(props));
    props.dwProps = IAP_EMAIL;
    props.pszEmail = "frank2@example.com";
    hr = IMimeAddressTable_SetProps(table, h, &props);
    CHECK(hr == S_OK, "SetProps %08lx", HR(hr));
    PropVariantInit(&pv);
    pv.vt = VT_LPSTR;
    IMimeBody_GetProp(body, "Bcc", 0, &pv);
    CHECK(pv.vt == VT_LPSTR && strstr(pv.pszVal, "frank2@example.com"), "SetProps reached header");
    PropVariantClear(&pv);

    hr = IMimeAddressTable_Delete(table, h);
    CHECK(hr == S_OK, "Delete %08lx", HR(hr));
    PropVariantInit(&pv);
    pv.vt = VT_LPSTR;
    hr = IMimeBody_GetProp(body, "Bcc", 0, &pv);
    CHECK(hr != S_OK, "Bcc header removed with its last address %08lx", HR(hr));
    PropVariantClear(&pv);
    hr = IMimeAddressTable_Delete(table, h);
    CHECK(HR(hr) == 0x800cce09, "Delete twice %08lx", HR(hr));

    /* a header changed behind the table's back is seen */
    PropVariantInit(&pv);
    pv.vt = VT_LPSTR;
    pv.pszVal = (char *)"zed@example.com";
    IMimeBody_SetProp(body, "Reply-To", 0, &pv);
    hr = IMimeAddressTable_CountTypes(table, IAT_REPLYTO, &count);
    CHECK(count == 1, "Reply-To seen %lu", count);

    /* enumeration */
    hr = IMimeAddressTable_EnumTypes(table, IAT_RECIPS, IAP_EMAIL, &en);
    CHECK(hr == S_OK && en, "EnumTypes %08lx", HR(hr));
    if (en)
    {
        ADDRESSPROPS two[2];
        memset(two, 0, sizeof(two));
        hr = IMimeEnumAddressTypes_Count(en, &count);
        CHECK(hr == S_OK && count == 4, "enum count %lu", count);
        hr = IMimeEnumAddressTypes_Next(en, 2, two, &fetched);
        CHECK(hr == S_OK && fetched == 2 && !strcmp(two[0].pszEmail, "bob@example.com"), "Next %08lx %lu", HR(hr), fetched);
        CoTaskMemFree(two[0].pszEmail); CoTaskMemFree(two[1].pszEmail);
        hr = IMimeEnumAddressTypes_Clone(en, &en2);
        CHECK(hr == S_OK, "enum Clone");
        hr = IMimeEnumAddressTypes_Skip(en, 1);
        CHECK(hr == S_OK, "Skip");
        memset(two, 0, sizeof(two));
        hr = IMimeEnumAddressTypes_Next(en, 2, two, &fetched);
        CHECK(hr == S_FALSE && fetched == 1 && !strcmp(two[0].pszEmail, "eve@example.com"), "Next at end %08lx %lu", HR(hr), fetched);
        CoTaskMemFree(two[0].pszEmail);
        IMimeEnumAddressTypes_Reset(en);
        hr = IMimeEnumAddressTypes_Next(en, 1, two, &fetched);
        CHECK(fetched == 1, "after Reset");
        CoTaskMemFree(two[0].pszEmail);
        if (en2)
        {
            memset(two, 0, sizeof(two));
            IMimeEnumAddressTypes_Next(en2, 1, two, &fetched);
            CHECK(fetched == 1 && !strcmp(two[0].pszEmail, "dan@example.com"), "clone keeps its position: %s", fetched ? two[0].pszEmail : "");
            if (fetched) CoTaskMemFree(two[0].pszEmail);
            IMimeEnumAddressTypes_Release(en2);
        }
        IMimeEnumAddressTypes_Release(en);
    }

    /* parsing a string, appending one, cloning */
    memset(&list, 0, sizeof(list));
    hr = IMimeAddressTable_ParseRfc822(table, IAT_TO, IET_DECODED, "\"Q, R\" <q@x.org>, plain@x.org", &list);
    CHECK(hr == S_OK && list.cAdrs == 2 && !strcmp(list.prgAdr[1].pszEmail, "plain@x.org"), "ParseRfc822 %08lx n=%lu", HR(hr), list.cAdrs);
    {
        IMimeAllocator *alloc;
        MimeOleGetAllocator(&alloc);
        IMimeAllocator_FreeAddressList(alloc, &list);
        IMimeAllocator_Release(alloc);
    }
    hr = IMimeAddressTable_AppendRfc822(table, IAT_CC, IET_DECODED, "g@x.org, h@x.org");
    CHECK(hr == S_OK, "AppendRfc822");
    IMimeAddressTable_CountTypes(table, IAT_CC, &count);
    CHECK(count == 4, "Cc after AppendRfc822 %lu", count);
    hr = IMimeAddressTable_Clone(table, &clone);
    CHECK(hr == S_OK && clone, "table Clone");
    if (clone)
    {
        IMimeAddressTable_CountTypes(clone, IAT_CC, &count);
        CHECK(count == 4, "clone has the addresses");
        IMimeAddressTable_DeleteTypes(clone, IAT_CC);
        IMimeAddressTable_CountTypes(table, IAT_CC, &count);
        CHECK(count == 4, "clone is independent");
        IMimeAddressTable_Release(clone);
    }
    hr = IMimeAddressTable_DeleteTypes(table, IAT_CC);
    CHECK(hr == S_OK, "DeleteTypes");
    hr = IMimeAddressTable_DeleteTypes(table, IAT_CC);
    CHECK(HR(hr) == 0x800cce05, "DeleteTypes none %08lx", HR(hr));

    /* the message level calls */
    memset(&list, 0, sizeof(list));
    hr = IMimeMessage_GetAddressTypes(msg, IAT_TO, IAP_EMAIL, &list);
    CHECK(hr == S_OK && list.cAdrs == 2, "message GetAddressTypes %08lx", HR(hr));
    {
        IMimeAllocator *alloc;
        MimeOleGetAllocator(&alloc);
        IMimeAllocator_FreeAddressList(alloc, &list);
        IMimeAllocator_Release(alloc);
    }
    hr = IMimeMessage_GetAddressFormat(msg, IAT_FROM, AFT_DISPLAY_BOTH, &text);
    CHECK(hr == S_OK && !strcmp(text, "Alice Example <alice@example.com>"), "message format");
    if (hr == S_OK) CoTaskMemFree(text);
    hr = IMimeMessage_EnumAddressTypes(msg, IAT_TO, IAP_EMAIL, &en);
    CHECK(hr == S_OK && en, "message EnumAddressTypes");
    if (en) IMimeEnumAddressTypes_Release(en);
    memset(&props, 0, sizeof(props));
    props.dwProps = IAP_EMAIL;
    hr = IMimeMessage_GetSender(msg, &props);
    CHECK(hr == S_OK && !strcmp(props.pszEmail, "alice@example.com"), "message GetSender");
    CoTaskMemFree(props.pszEmail);

    /* MimeGetAddressFormatW */
    hr = MimeGetAddressFormatW(&IID_IMimeMessage, msg, IAT_TO, AFT_DISPLAY_EMAIL, &wtext);
    CHECK(hr == S_OK && !wcscmp(wtext, L"bob@example.com; carol@example.com"), "MimeGetAddressFormatW %08lx", HR(hr));
    if (hr == S_OK) CoTaskMemFree(wtext);
    hr = MimeGetAddressFormatW(&IID_IMimeAddressTable, table, IAT_FROM, AFT_DISPLAY_FRIENDLY, &wtext);
    CHECK(hr == S_OK && !wcscmp(wtext, L"Alice Example"), "table form");
    if (hr == S_OK) CoTaskMemFree(wtext);
    hr = MimeGetAddressFormatW(&IID_IUnknown, msg, IAT_FROM, AFT_DISPLAY_FRIENDLY, &wtext);
    CHECK(hr == E_INVALIDARG, "bad riid %08lx", HR(hr));

    IMimeBody_Release(body);
    IMimeAddressTable_Release(table);
    IMimeMessage_Release(msg);
}

static void test_schema_and_security(void)
{
    IMimePropertySchema *schema;
    IMimeSecurity *sec;
    IMimeMessage *msg;
    IMimeBody *body;
    DWORD id, id2, type, t2;
    char *name;
    HRESULT hr;
    PROPVARIANT pv;

    hr = MimeOleGetPropertySchema(&schema);
    CHECK(hr == S_OK, "schema %08lx", HR(hr));
    hr = IMimePropertySchema_GetPropertyId(schema, "Subject", &id);
    CHECK(hr == S_OK && id == PID_HDR_SUBJECT, "builtin id %08lx %lu", HR(hr), id);
    hr = IMimePropertySchema_GetPropertyName(schema, PID_HDR_FROM, &name);
    CHECK(hr == S_OK && !strcmp(name, "From"), "builtin name");
    if (hr == S_OK) CoTaskMemFree(name);
    hr = IMimePropertySchema_GetPropertyId(schema, "X-Fast-Unknown", &id);
    CHECK(HR(hr) == 0x800cce05, "unknown id %08lx", HR(hr));
    hr = IMimePropertySchema_GetPropertyName(schema, 0x7fff, &name);
    CHECK(HR(hr) == 0x800cce05, "unknown name %08lx", HR(hr));
    hr = IMimePropertySchema_RegisterProperty(schema, "X-Fast-Prop", 0, 0, VT_LPSTR, &id);
    CHECK(hr == S_OK && id, "register %08lx", HR(hr));
    hr = IMimePropertySchema_RegisterProperty(schema, "x-fast-prop", 0, 0, VT_LPSTR, &id2);
    CHECK(hr == S_OK && id2 == id, "register twice gives the same id");
    hr = IMimePropertySchema_GetPropertyId(schema, "X-FAST-PROP", &id2);
    CHECK(hr == S_OK && id2 == id, "registered id");
    hr = IMimePropertySchema_GetPropertyName(schema, id, &name);
    CHECK(hr == S_OK && !strcmp(name, "X-Fast-Prop"), "registered name");
    if (hr == S_OK) CoTaskMemFree(name);
    hr = IMimePropertySchema_RegisterProperty(schema, NULL, 0, 0, VT_LPSTR, &id2);
    CHECK(hr == E_INVALIDARG, "register NULL");
    hr = IMimePropertySchema_RegisterProperty(schema, "bad name:", 0, 0, VT_LPSTR, &id2);
    CHECK(HR(hr) == 0x800cce21, "register bad name %08lx", HR(hr));
    hr = IMimePropertySchema_ModifyProperty(schema, "X-Fast-Prop", MPF_ADDRESS, 1, VT_LPSTR);
    CHECK(hr == S_OK, "modify");
    hr = IMimePropertySchema_ModifyProperty(schema, "X-Fast-Nope", 0, 1, VT_LPSTR);
    CHECK(HR(hr) == 0x800cce05, "modify unknown %08lx", HR(hr));
    hr = IMimePropertySchema_RegisterAddressType(schema, "X-Fast-Addr", &type);
    CHECK(hr == S_OK && type >= 0x800 && !(type & (type - 1)), "address type %08lx %lx", HR(hr), type);
    hr = IMimePropertySchema_RegisterAddressType(schema, "x-fast-addr", &t2);
    CHECK(hr == S_OK && t2 == type, "address type twice");
    hr = IMimePropertySchema_RegisterAddressType(schema, NULL, &t2);
    CHECK(hr == E_INVALIDARG, "address type NULL");

    /* a registered property is found by its id on a body */
    MimeOleCreateMessage(NULL, &msg);
    IMimeMessage_BindToObject(msg, HBODY_ROOT, &IID_IMimeBody, (void **)&body);
    PropVariantInit(&pv);
    pv.vt = VT_LPSTR; pv.pszVal = (char *)"value";
    hr = IMimeBody_SetProp(body, "X-Fast-Prop", 0, &pv);
    CHECK(hr == S_OK, "set registered");
    {
        char pid[16];
        PROPVARIANT out;
        sprintf(pid, "%c%lu", '\x01', id); /* PIDTOSTR form */
        pid[0] = (char)0xff;
        PropVariantInit(&out);
        out.vt = VT_LPSTR;
        hr = IMimeBody_GetProp(body, "X-Fast-Prop", 0, &out);
        CHECK(hr == S_OK && !strcmp(out.pszVal, "value"), "get registered");
        PropVariantClear(&out);
    }
    /* and the new address type is a header of the table */
    {
        IMimeAddressTable *table;
        ULONG count;
        IMimeBody_QueryInterface(body, &IID_IMimeAddressTable, (void **)&table);
        hr = IMimeAddressTable_Append(table, type, IET_DECODED, NULL, "t@x.org", NULL);
        CHECK(hr == S_OK, "append new type %08lx", HR(hr));
        PropVariantInit(&pv);
        pv.vt = VT_LPSTR;
        hr = IMimeBody_GetProp(body, "X-Fast-Addr", 0, &pv);
        CHECK(hr == S_OK && strstr(pv.pszVal, "t@x.org"), "new type header");
        PropVariantClear(&pv);
        IMimeAddressTable_CountTypes(table, type, &count);
        CHECK(count == 1, "new type count");
        IMimeAddressTable_Release(table);
    }
    IMimeBody_Release(body);

    /* security */
    hr = MimeOleCreateSecurity(&sec);
    CHECK(hr == S_OK, "security");
    hr = IMimeSecurity_CheckInit(sec);
    CHECK(HR(hr) == 0x800cce0b, "CheckInit before InitNew %08lx", HR(hr));
    hr = IMimeSecurity_EncodeMessage(sec, (IMimeMessageTree *)msg, 0);
    CHECK(HR(hr) == 0x800cce0b, "Encode before InitNew %08lx", HR(hr));
    IMimeSecurity_InitNew(sec);
    hr = IMimeSecurity_CheckInit(sec);
    CHECK(hr == S_OK, "CheckInit after InitNew");
    hr = IMimeSecurity_EncodeMessage(sec, (IMimeMessageTree *)msg, 0);
    CHECK(HR(hr) == 0x000cceb1, "EncodeMessage nothing to do %08lx", HR(hr));
    hr = IMimeSecurity_EncodeMessage(sec, NULL, 0);
    CHECK(hr == E_INVALIDARG, "EncodeMessage NULL");
    hr = IMimeSecurity_DecodeMessage(sec, (IMimeMessageTree *)msg, 0);
    CHECK(HR(hr) == 0x000cceb1, "DecodeMessage nothing to do %08lx", HR(hr));
    IMimeMessage_Release(msg);
    {
        IMimeBody *b;
        DWORD sectype = 99;
        IStream *src = mem_stream("Content-Type: multipart/signed; boundary=\"b\"\r\n\r\n--b\r\n\r\nx\r\n--b--\r\n");
        IMimeMessage *m;
        IPersistStreamInit *psi;
        MimeOleCreateMessage(NULL, &m);
        IMimeMessage_QueryInterface(m, &IID_IPersistStreamInit, (void **)&psi);
        IPersistStreamInit_Load(psi, src);
        IPersistStreamInit_Release(psi);
        IStream_Release(src);
        IMimeMessage_BindToObject(m, HBODY_ROOT, &IID_IMimeBody, (void **)&b);
        hr = IMimeSecurity_GetMessageType(sec, NULL, b, &sectype);
        CHECK(hr == S_OK && sectype == (MST_THIS_SIGN | 4), "multipart/signed type %08lx %lx", HR(hr), sectype);
        IMimeBody_Release(b);
        IMimeMessage_Release(m);
        src = mem_stream("Content-Type: application/pkcs7-mime; smime-type=signed-data\r\n\r\nx");
        MimeOleCreateMessage(NULL, &m);
        IMimeMessage_QueryInterface(m, &IID_IPersistStreamInit, (void **)&psi);
        IPersistStreamInit_Load(psi, src);
        IPersistStreamInit_Release(psi);
        IStream_Release(src);
        IMimeMessage_BindToObject(m, HBODY_ROOT, &IID_IMimeBody, (void **)&b);
        hr = IMimeSecurity_GetMessageType(sec, NULL, b, &sectype);
        CHECK(hr == S_OK && sectype == MST_THIS_SIGN, "pkcs7 signed type %lx", sectype);
        IMimeBody_Release(b);
        IMimeMessage_Release(m);
        src = mem_stream("Content-Type: application/pkcs7-mime\r\n\r\nx");
        MimeOleCreateMessage(NULL, &m);
        IMimeMessage_QueryInterface(m, &IID_IPersistStreamInit, (void **)&psi);
        IPersistStreamInit_Load(psi, src);
        IPersistStreamInit_Release(psi);
        IStream_Release(src);
        IMimeMessage_BindToObject(m, HBODY_ROOT, &IID_IMimeBody, (void **)&b);
        hr = IMimeSecurity_GetMessageType(sec, NULL, b, &sectype);
        CHECK(hr == S_OK && sectype == MST_THIS_ENCRYPT, "pkcs7 enveloped type %lx", sectype);
        IMimeSecurity_DecodeMessage(sec, (IMimeMessageTree *)m, 0);
        IMimeBody_Release(b);
        IMimeMessage_Release(m);
    }
    {
        HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, NULL);
        PCCERT_CONTEXT cert = NULL;
        char *nm;
        hr = IMimeSecurity_EnumCertificates(sec, store, 0, NULL, (const void **)&cert);
        CHECK(HR(hr) == 0x800cce05 && !cert, "empty store %08lx", HR(hr));
        hr = IMimeSecurity_EnumCertificates(sec, NULL, 0, NULL, (const void **)&cert);
        CHECK(hr == E_INVALIDARG, "no store");
        hr = IMimeSecurity_GetCertificateName(sec, NULL, SIMPLE, &nm);
        CHECK(hr == E_INVALIDARG, "name of no cert");
        pv.vt = 0;
        hr = IMimeSecurity_GetCertData(sec, NULL, CDID_EMAIL, &pv);
        CHECK(hr == E_INVALIDARG, "data of no cert");
        CertCloseStore(store, 0);
    }
    IMimeSecurity_Release(sec);
    IMimePropertySchema_Release(schema);
}

static void test_message_tree(void)
{
    IMimeMessage *msg, *msg2;
    IMimeBody *body;
    HBODY root, h1, h2, h3, hm;
    HRESULT hr;
    IStream *s, *copy;
    char *text, *cid = NULL;
    ULONG count;
    HCHARSET cs;
    PROPVARIANT pv;
    DWORD flags;
    IPersistStreamInit *psi;
    IStream *src;

    /* ToMultipart / MoveBody */
    MimeOleCreateMessage(NULL, &msg);
    src = mem_stream("Subject: s\r\nContent-Type: text/plain\r\n\r\nplain body\r\n");
    IMimeMessage_QueryInterface(msg, &IID_IPersistStreamInit, (void **)&psi);
    IPersistStreamInit_Load(psi, src);
    IPersistStreamInit_Release(psi);
    IStream_Release(src);
    hr = IMimeMessage_ToMultipart(msg, HBODY_ROOT, "mixed", &h1);
    CHECK(hr == S_OK && h1, "ToMultipart %08lx", HR(hr));
    CHECK(IMimeMessage_IsContentType(msg, HBODY_ROOT, "multipart", "mixed") == S_OK, "root is multipart/mixed");
    CHECK(IMimeMessage_IsContentType(msg, h1, "text", "plain") == S_OK, "content moved to the child");
    hr = IMimeMessage_CountBodies(msg, HBODY_ROOT, TRUE, &count);
    CHECK(count == 2, "two bodies %lu", count);
    hr = IMimeMessage_ToMultipart(msg, HBODY_ROOT, NULL, &h2);
    CHECK(hr == E_INVALIDARG, "ToMultipart NULL subtype %08lx", HR(hr));
    hr = IMimeMessage_ToMultipart(msg, (HBODY)0x555, "mixed", &h2);
    CHECK(HR(hr) == 0x800cce09, "ToMultipart bad handle %08lx", HR(hr));
    IMimeMessage_GetBody(msg, IBL_ROOT, NULL, &root);
    hr = IMimeMessage_GetBody(msg, IBL_FIRST, root, &h1);

    /* attachments */
    src = mem_stream("file contents");
    hr = IMimeMessage_AttachFile(msg, "C:\\dir\\notes.txt", src, &h2);
    CHECK(hr == S_OK && h2, "AttachFile %08lx", HR(hr));
    IStream_Release(src);
    hr = IMimeMessage_AttachFile(msg, NULL, NULL, &h3);
    CHECK(hr == E_INVALIDARG, "AttachFile NULL %08lx", HR(hr));
    CHECK(IMimeMessage_IsBodyType(msg, h2, IBT_ATTACHMENT) == S_OK, "attachment type");
    IMimeMessage_BindToObject(msg, h2, &IID_IMimeBody, (void **)&body);
    PropVariantInit(&pv);
    pv.vt = VT_LPSTR;
    IMimeBody_GetProp(body, "att:filename", 0, &pv);
    CHECK(pv.vt == VT_LPSTR && !strcmp(pv.pszVal, "notes.txt"), "att:filename '%s'", pv.vt == VT_LPSTR ? pv.pszVal : "");
    PropVariantClear(&pv);
    IMimeBody_Release(body);

    src = mem_stream("PNGDATA");
    hr = IMimeMessage_AttachURL(msg, NULL, NULL, 2 /* GENERATE_CID */, src, &cid, &h3);
    CHECK(hr == S_OK && cid && !strncmp(cid, "cid:", 4), "AttachURL cid %08lx '%s'", HR(hr), cid ? cid : "");
    IStream_Release(src);
    if (cid)
    {
        HBODY found = NULL;
        hr = IMimeMessage_ResolveURL(msg, NULL, NULL, cid, 0, &found);
        CHECK(hr == S_OK && found == h3, "ResolveURL cid %08lx", HR(hr));
        hr = IMimeMessage_ResolveURL(msg, NULL, NULL, "cid:nothing@here", 0, &found);
        CHECK(HR(hr) == 0x800cce05, "ResolveURL unknown %08lx", HR(hr));
        {
            char longer[128];
            sprintf(longer, "%sx", cid);
            hr = IMimeMessage_ResolveURL(msg, NULL, NULL, longer, 0, &found);
            CHECK(HR(hr) == 0x800cce05, "ResolveURL of a longer cid %08lx", HR(hr));
            sprintf(longer, "cid:%.1s", cid + 4);
            hr = IMimeMessage_ResolveURL(msg, NULL, NULL, longer, 0, &found);
            CHECK(HR(hr) == 0x800cce05, "ResolveURL of a prefix %08lx", HR(hr));
        }
        CoTaskMemFree(cid);
    }
    src = mem_stream("IMG");
    hr = IMimeMessage_AttachURL(msg, "http://example.com/dir/", "pic.gif", 4 /* SET_CNTTYPE */, src, NULL, &hm);
    CHECK(hr == S_OK, "AttachURL location %08lx", HR(hr));
    IStream_Release(src);
    {
        HBODY found = NULL;
        hr = IMimeMessage_ResolveURL(msg, NULL, "http://example.com/dir/", "pic.gif", 0, &found);
        CHECK(hr == S_OK && found == hm, "ResolveURL by location %08lx", HR(hr));
        hr = IMimeMessage_ResolveURL(msg, NULL, NULL, NULL, 0, &found);
        CHECK(hr == E_INVALIDARG, "ResolveURL NULL");
    }
    hr = IMimeMessage_GetAttachments(msg, &count, (HBODY **)&cid);
    CoTaskMemFree(cid);

    /* object attach */
    MimeOleCreateMessage(NULL, &msg2);
    hr = IMimeMessage_AttachObject(msg, &IID_IMimeMessage, msg2, &h3);
    CHECK(hr == S_OK, "AttachObject message %08lx", HR(hr));
    hr = IMimeMessage_AttachObject(msg, &IID_IUnknown, msg2, &h3);
    CHECK(HR(hr) == 0x800cce28, "AttachObject bad iid %08lx", HR(hr));
    IMimeMessage_Release(msg2);

    /* MoveBody */
    IMimeMessage_GetBody(msg, IBL_FIRST, root, &h1);
    hr = IMimeMessage_MoveBody(msg, h1, IBL_LAST);
    CHECK(hr == S_OK, "MoveBody last %08lx", HR(hr));
    hr = IMimeMessage_GetBody(msg, IBL_LAST, root, &h3);
    CHECK(hr == S_OK && h3 == h1, "moved body is last");
    hr = IMimeMessage_MoveBody(msg, h1, IBL_NEXT);
    CHECK(hr != S_OK, "last body has no next %08lx", HR(hr));
    hr = IMimeMessage_MoveBody(msg, h1, IBL_PREVIOUS);
    CHECK(hr == S_OK, "MoveBody previous");
    IMimeMessage_GetBody(msg, IBL_LAST, root, &h3);
    CHECK(h3 != h1, "the body moved before the last one");
    hr = IMimeMessage_MoveBody(msg, h1, IBL_FIRST);
    CHECK(hr == S_OK, "MoveBody first");
    hr = IMimeMessage_GetBody(msg, IBL_FIRST, root, &h3);
    CHECK(h3 == h1, "first again");
    hr = IMimeMessage_MoveBody(msg, root, IBL_FIRST);
    CHECK(hr != S_OK, "root cannot move");
    hr = IMimeMessage_MoveBody(msg, h1, IBL_ROOT);
    CHECK(hr == E_INVALIDARG, "MoveBody to root %08lx", HR(hr));
    hr = IMimeMessage_MoveBody(msg, NULL, IBL_FIRST);
    CHECK(hr == E_INVALIDARG, "MoveBody NULL");
    hr = IMimeMessage_MoveBody(msg, (HBODY)0x777, IBL_FIRST);
    CHECK(HR(hr) == 0x800cce09, "MoveBody bad handle %08lx", HR(hr));

    /* the whole message reloads with the same shape */
    text = message_text(msg);
    CHECK(strstr(text, "filename=notes.txt") || strstr(text, "filename=\"notes.txt\""), "attachment in text");
    CHECK(strstr(text, "Content-Transfer-Encoding: base64"), "attachment is base64");
    CHECK(strstr(text, "Content-Location: http://example.com/dir/pic.gif") || strstr(text, "Content-Location: pic.gif"), "location header");
    CHECK(strstr(text, "Content-Base: http://example.com/dir/"), "base header");
    MimeOleCreateMessage(NULL, &msg2);
    src = mem_stream(text);
    IMimeMessage_QueryInterface(msg2, &IID_IPersistStreamInit, (void **)&psi);
    IPersistStreamInit_Load(psi, src);
    IPersistStreamInit_Release(psi);
    IStream_Release(src);
    IMimeMessage_CountBodies(msg2, HBODY_ROOT, TRUE, &count);
    CHECK(count == 7, "reloaded bodies %lu", count);

    /* offset table: save from the loaded message, load into a new one, same bodies */
    CreateStreamOnHGlobal(NULL, TRUE, &s);
    hr = IMimeMessage_SaveOffsetTable(msg2, s, 0);
    CHECK(hr == S_OK, "SaveOffsetTable %08lx", HR(hr));
    {
        STATSTG st;
        IStream_Stat(s, &st, STATFLAG_NONAME);
        CHECK(st.cbSize.LowPart > 12, "table has entries: %lu bytes", st.cbSize.LowPart);
    }
    IStream_Seek(s, (LARGE_INTEGER){{0}}, STREAM_SEEK_SET, NULL);
    {
        IMimeMessage *msg3;
        IStream *text_stream = mem_stream(text);
        MimeOleCreateMessage(NULL, &msg3);
        hr = IMimeMessage_LoadOffsetTable(msg3, s);
        CHECK(hr == S_OK, "LoadOffsetTable %08lx", HR(hr));
        IMimeMessage_QueryInterface(msg3, &IID_IPersistStreamInit, (void **)&psi);
        IPersistStreamInit_Load(psi, text_stream);
        IPersistStreamInit_Release(psi);
        IMimeMessage_CountBodies(msg3, HBODY_ROOT, TRUE, &count);
        CHECK(count == 7, "bodies from the offset table %lu", count);
        IStream_Release(text_stream);
        IMimeMessage_Release(msg3);
    }
    hr = IMimeMessage_LoadOffsetTable(msg2, NULL);
    CHECK(hr == E_INVALIDARG, "LoadOffsetTable NULL");
    {
        IStream *junk = mem_stream("this is not an offset table at all");
        hr = IMimeMessage_LoadOffsetTable(msg2, junk);
        CHECK(hr != S_OK, "junk table refused %08lx", HR(hr));
        IStream_Release(junk);
    }
    IStream_Release(s);

    /* HandsOffStorage: the message no longer needs the stream it was loaded from */
    {
        IMimeMessage *msg4;
        IStream *loaded = mem_stream(text);
        IMimeBody *b4;
        HBODY first;
        char big[64] = {0};
        ULONG n;

        MimeOleCreateMessage(NULL, &msg4);
        IMimeMessage_QueryInterface(msg4, &IID_IPersistStreamInit, (void **)&psi);
        IPersistStreamInit_Load(psi, loaded);
        IPersistStreamInit_Release(psi);
        hr = IMimeMessage_HandsOffStorage(msg4);
        CHECK(hr == S_OK, "HandsOffStorage %08lx", HR(hr));
        {
            ULONG refs = IStream_AddRef(loaded);
            CHECK(refs == 2, "nothing of the message holds the loaded stream any more: %lu references", refs);
            IStream_Release(loaded);
        }
        IStream_Release(loaded);   /* the last reference but the message's own, if it still held it */
        {
            IStream *d;
            hr = IMimeMessage_GetTextBody(msg4, TXT_PLAIN, IET_DECODED, &d, &first);
            if (hr == S_OK)
            {
                IStream_Read(d, big, 10, &n);
                CHECK(n == 10 && !strncmp(big, "plain body", 10), "data after HandsOffStorage '%s'", big);
                IStream_Release(d);
            }
            else CHECK(0, "GetTextBody after HandsOff %08lx", HR(hr));
        }
        (void)b4;
        CHECK(strstr(message_text(msg4), "plain body"), "source after HandsOffStorage");
        IMimeMessage_Release(msg4);
    }
    IMimeMessage_Release(msg2);

    /* charset */
    hr = IMimeMessage_GetCharset(msg, &cs);
    CHECK(hr == S_OK && cs, "GetCharset %08lx", HR(hr));
    hr = IMimeMessage_GetCharset(msg, NULL);
    CHECK(hr == E_INVALIDARG, "GetCharset NULL");
    hr = IMimeMessage_SetCharset(msg, NULL, CSET_APPLY_ALL);
    CHECK(hr == E_INVALIDARG, "SetCharset NULL");
    hr = MimeOleFindCharset("iso-8859-2", &cs);
    hr = IMimeMessage_SetCharset(msg, cs, CSET_APPLY_ALL);
    CHECK(hr == S_OK, "SetCharset %08lx", HR(hr));
    CHECK(strstr(message_text(msg), "charset=iso-8859-2") != NULL, "charset tagged on the text part");
    IMimeMessage_Release(msg);

    /* Commit */
    MimeOleCreateMessage(NULL, &msg);
    hr = IMimeMessage_Commit(msg, 0);
    CHECK(hr == S_OK || HR(hr) == 0x800cce05, "Commit empty %08lx", HR(hr));
    src = mem_stream("<p>html</p>");
    hr = IMimeMessage_SetTextBody(msg, TXT_HTML, IET_DECODED, NULL, src, &h1);
    CHECK(hr == S_OK && h1, "SetTextBody html %08lx", HR(hr));
    IStream_Release(src);
    src = mem_stream("plain text");
    hr = IMimeMessage_SetTextBody(msg, TXT_PLAIN, IET_DECODED, h1, src, &h2);
    CHECK(hr == S_OK && h2, "SetTextBody alt %08lx", HR(hr));
    IStream_Release(src);
    text = message_text(msg);
    CHECK(strstr(text, "multipart/alternative") && strstr(text, "text/html") && strstr(text, "text/plain"), "alternative:\n%s", text);
    hr = IMimeMessage_SetTextBody(msg, 77, IET_DECODED, NULL, src, &h3);
    CHECK(HR(hr) == 0x800cce38 || hr == E_INVALIDARG, "SetTextBody bad type %08lx", HR(hr));
    hr = IMimeMessage_SetTextBody(msg, TXT_HTML, IET_DECODED, NULL, NULL, &h3);
    CHECK(hr == E_INVALIDARG, "SetTextBody NULL stream");
    CHECK(IMimeMessage_IsDirty(msg) == S_OK, "dirty before Commit");
    hr = IMimeMessage_Commit(msg, COMMIT_ONLYIFDIRTY);
    CHECK(hr == S_OK, "Commit %08lx", HR(hr));
    CHECK(IMimeMessage_IsDirty(msg) == S_FALSE, "clean after Commit");
    hr = IMimeMessage_GetTextBody(msg, TXT_PLAIN, IET_DECODED, &s, &h3);
    CHECK(hr == S_OK && s, "GetTextBody after SetTextBody %08lx", HR(hr));
    if (s) { CHECK(!strncmp(stream_text(s), "plain text", 10), "plain text back"); IStream_Release(s); }
    /* unicode text is kept as UTF-8 */
    src = NULL;
    {
        static const WCHAR uni[] = L"caf\xe9";
        IStream *u;
        CreateStreamOnHGlobal(NULL, TRUE, &u);
        IStream_Write(u, uni, 8, NULL);
        IMimeMessage_Release(msg);
        MimeOleCreateMessage(NULL, &msg);
        hr = IMimeMessage_SetTextBody(msg, TXT_PLAIN, IET_UNICODE, NULL, u, &h1);
        CHECK(hr == S_OK, "SetTextBody unicode %08lx", HR(hr));
        text = message_text(msg);
        CHECK(strstr(text, "charset=utf-8") && strstr(text, "base64"), "unicode text as base64 utf-8:\n%s", text);
        IStream_Release(u);
    }
    IMimeMessage_Release(msg);
    (void)copy; (void)flags;
}

static void test_range_list(void)
{
    IRangeList *rl, *rl2;
    ULONG v;
    char *str;
    DWORD len;
    HRESULT hr;
    byte *saved;
    ULONG size;

    hr = CreateRangeList(&rl);
    CHECK(hr == S_OK, "CreateRangeList");
    CHECK(IRangeList_IsInRange(rl, 1) == S_FALSE, "empty");
    hr = IRangeList_Min(rl, &v);
    CHECK(hr != S_OK && v == RL_RANGE_ERROR, "Min of empty");
    IRangeList_AddRange(rl, 5, 9);
    IRangeList_AddSingleValue(rl, 3);
    IRangeList_AddRange(rl, 10, 12);   /* touches 9 */
    IRangeList_AddSingleValue(rl, 20);
    IRangeList_RangeToIMAPString(rl, &str, &len);
    CHECK(!strcmp(str, "3,5:12,20") && len == 9, "string '%s' %lu", str, len);
    CoTaskMemFree(str);
    CHECK(IRangeList_IsInRange(rl, 7) == S_OK && IRangeList_IsInRange(rl, 4) == S_FALSE, "membership");
    IRangeList_Min(rl, &v); CHECK(v == 3, "Min");
    IRangeList_Max(rl, &v); CHECK(v == 20, "Max");
    IRangeList_Cardinality(rl, &v); CHECK(v == 10, "Cardinality %lu", v);
    IRangeList_CardinalityFrom(rl, 8, &v); CHECK(v == 6, "CardinalityFrom %lu", v);
    IRangeList_MinOfRange(rl, 8, &v); CHECK(v == 5, "MinOfRange");
    IRangeList_MaxOfRange(rl, 8, &v); CHECK(v == 12, "MaxOfRange");
    IRangeList_Next(rl, 12, &v); CHECK(v == 20, "Next across a gap");
    IRangeList_Next(rl, 6, &v); CHECK(v == 7, "Next inside");
    IRangeList_Prev(rl, 5, &v); CHECK(v == 3, "Prev across a gap");
    hr = IRangeList_Next(rl, 20, &v);
    CHECK(hr != S_OK && v == RL_RANGE_ERROR, "Next at the end");
    IRangeList_DeleteRange(rl, 6, 7);
    IRangeList_RangeToIMAPString(rl, &str, &len);
    CHECK(!strcmp(str, "3,5,8:12,20"), "after delete '%s'", str);
    CoTaskMemFree(str);
    IRangeList_Save(rl, &saved, &size);
    CreateRangeList(&rl2);
    hr = IRangeList_Load(rl2, saved, size);
    CHECK(hr == S_OK, "Load");
    IRangeList_RangeToIMAPString(rl2, &str, &len);
    CHECK(!strcmp(str, "3,5,8:12,20"), "saved and loaded '%s'", str);
    CoTaskMemFree(str);
    CoTaskMemFree(saved);
    hr = IRangeList_Load(rl2, (byte *)"abc", 3);
    CHECK(hr == E_INVALIDARG, "Load bad size");
    IRangeList_DeleteRangeList(rl, rl2);
    IRangeList_Cardinality(rl, &v);
    CHECK(v == 0, "DeleteRangeList");
    IRangeList_AddRangeList(rl, rl2);
    IRangeList_Cardinality(rl, &v);
    CHECK(v == 8, "AddRangeList %lu", v);
    IRangeList_AddRange(rl, 100, RL_LAST_MESSAGE);
    IRangeList_RangeToIMAPString(rl, &str, &len);
    CHECK(strstr(str, "100:*") != NULL, "last message '%s'", str);
    CoTaskMemFree(str);
    IRangeList_Clear(rl);
    CHECK(IRangeList_IsInRange(rl, 3) == S_FALSE, "Clear");
    IRangeList_Release(rl2);
    IRangeList_Release(rl);
}

/* ---- IMAP ---- */
typedef struct
{
    IIMAPCallback IIMAPCallback_iface;
    LONG ref;
    int completions, statuses;
    int exists, recent, unseen, deleted, flags_resp, permflags, uidvalidity, rw, trycreate, searches, listings;
    int status_resp, fetch_msgs, fetch_bodies, appends, alerts;
    DWORD last_exists, last_uidv, search_card, flagsmask, permmask, status_messages, status_uidnext;
    char listed[4][32];
    DWORD fetch_uid, fetch_size, fetch_flags;
    FILETIME fetch_date;
    char body[64];
    HRESULT last_hr[32];
    WPARAM last_wparam[32];
    LPARAM last_lparam[32];
    BOOL logon_prompt;
} imap_cb;

static imap_cb *cb_from(IIMAPCallback *iface) { return (imap_cb *)iface; }
static HRESULT WINAPI cb_QI(IIMAPCallback *i, REFIID r, void **p)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_ITransportCallback) || IsEqualIID(r, &IID_IIMAPCallback))
    { *p = i; IIMAPCallback_AddRef(i); return S_OK; }
    *p = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cb_AddRef(IIMAPCallback *i) { return InterlockedIncrement(&cb_from(i)->ref); }
static ULONG WINAPI cb_Release(IIMAPCallback *i) { return InterlockedDecrement(&cb_from(i)->ref); }
static HRESULT WINAPI cb_OnTimeout(IIMAPCallback *i, DWORD *t, IInternetTransport *x) { return S_OK; }
static HRESULT WINAPI cb_OnLogonPrompt(IIMAPCallback *i, LPINETSERVER s, IInternetTransport *x)
{
    cb_from(i)->logon_prompt = TRUE;
    strcpy(s->szUserName, "fast");
    strcpy(s->szPassword, "secret");
    return S_OK;
}
static INT WINAPI cb_OnPrompt(IIMAPCallback *i, HRESULT h, LPCSTR t, LPCSTR c, UINT u, IInternetTransport *x) { return 0; }
static HRESULT WINAPI cb_OnStatus(IIMAPCallback *i, IXPSTATUS s, IInternetTransport *x) { cb_from(i)->statuses++; return S_OK; }
static HRESULT WINAPI cb_OnError(IIMAPCallback *i, IXPSTATUS s, LPIXPRESULT r, IInternetTransport *x) { return S_OK; }
static HRESULT WINAPI cb_OnCommand(IIMAPCallback *i, CMDTYPE c, LPSTR l, HRESULT h, IInternetTransport *x) { return S_OK; }
static HRESULT WINAPI cb_OnResponse(IIMAPCallback *i, const IMAP_RESPONSE *r)
{
    imap_cb *cb = cb_from(i);
    switch (r->irtResponseType)
    {
    case irtCOMMAND_COMPLETION:
        if (cb->completions < 32) { cb->last_hr[cb->completions] = r->hrResult; cb->last_wparam[cb->completions] = r->wParam;
                                    cb->last_lparam[cb->completions] = r->lParam; }
        cb->completions++;
        break;
    case irtMAILBOX_UPDATE:
        if (r->irdResponseData.pmcMsgCount->bGotExistsResponse) { cb->exists++; cb->last_exists = r->irdResponseData.pmcMsgCount->dwExists; }
        if (r->irdResponseData.pmcMsgCount->bGotRecentResponse) cb->recent++;
        if (r->irdResponseData.pmcMsgCount->bGotUnseenResponse) { cb->unseen++; cb->status_messages = r->irdResponseData.pmcMsgCount->dwUnseen; }
        break;
    case irtDELETED_MSG: cb->deleted++; break;
    case irtAPPLICABLE_FLAGS: cb->flags_resp++; cb->flagsmask = r->irdResponseData.imfImapMessageFlags; break;
    case irtPERMANENT_FLAGS: cb->permflags++; cb->permmask = r->irdResponseData.imfImapMessageFlags; break;
    case irtUIDVALIDITY: cb->uidvalidity++; cb->last_uidv = r->irdResponseData.dwUIDValidity; break;
    case irtREADWRITE_STATUS: cb->rw += r->irdResponseData.bReadWrite ? 1 : 100; break;
    case irtTRYCREATE: cb->trycreate++; break;
    case irtSEARCH: cb->searches++; IRangeList_Cardinality(r->irdResponseData.prlSearchResults, &cb->search_card); break;
    case irtMAILBOX_LISTING:
        if (cb->listings < 4) { strcpy(cb->listed[cb->listings], r->irdResponseData.illrdMailboxListing.pszMailboxName);
                                cb->listed[cb->listings][30] = (char)(r->irdResponseData.illrdMailboxListing.imfMboxFlags | 0x40); }
        cb->listings++;
        break;
    case irtMAILBOX_STATUS:
        cb->status_resp++;
        cb->status_messages = r->irdResponseData.pisrStatusResponse->dwMessages;
        cb->status_uidnext = r->irdResponseData.pisrStatusResponse->dwUIDNext;
        break;
    case irtUPDATE_MSG:
        cb->fetch_msgs++;
        cb->fetch_uid = r->irdResponseData.pFetchResults->dwUID;
        cb->fetch_size = r->irdResponseData.pFetchResults->dwRFC822Size;
        cb->fetch_flags = r->irdResponseData.pFetchResults->mfMsgFlags;
        cb->fetch_date = r->irdResponseData.pFetchResults->ftInternalDate;
        break;
    case irtFETCH_BODY:
        cb->fetch_bodies++;
        strncpy(cb->body, r->irdResponseData.pFetchBodyPart->pszData, 63);
        break;
    case irtAPPEND_PROGRESS: cb->appends++; break;
    case irtSERVER_ALERT: cb->alerts++; break;
    default: break;
    }
    return S_OK;
}

static const IIMAPCallbackVtbl cb_vtbl =
{
    cb_QI, cb_AddRef, cb_Release, cb_OnTimeout, cb_OnLogonPrompt, cb_OnPrompt, cb_OnStatus, cb_OnError, cb_OnCommand,
    cb_OnResponse
};

static void pump_until(int *counter, int target, DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (*counter < target && GetTickCount() < end)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(5);
    }
}

static void test_imap(int port)
{
    imap_cb cb = {{(IIMAPCallbackVtbl *)&cb_vtbl}, 1};
    IIMAPTransport *imap = NULL;
    IRangeList *range;
    INETSERVER server;
    HRESULT hr;
    DWORD caps;
    IStream *mail;
    FILETIME zero = {0};
    DWORD uid;

    hr = CreateIMAPTransport(&imap);
    CHECK(hr == S_OK, "CreateIMAPTransport");
    hr = IIMAPTransport_IsState(imap, IXP_IS_CONNECTED);
    CHECK(hr == S_FALSE, "not connected yet");
    hr = IIMAPTransport_Select(imap, 1, 1, NULL, "INBOX");
    CHECK(hr == IXP_E_NOT_CONNECTED || HR(hr) == 0x800ccc05, "Select before Connect %08lx", HR(hr));
    hr = IIMAPTransport_Capability(imap, &caps);
    CHECK(HR(hr) == 0x800ccc05, "Capability before Connect %08lx", HR(hr));
    hr = IIMAPTransport_InitNew(imap, NULL, &cb.IIMAPCallback_iface);
    CHECK(hr == S_OK, "InitNew");
    hr = IIMAPTransport_InitNew(imap, NULL, NULL);
    CHECK(hr == E_INVALIDARG, "InitNew NULL");

    memset(&server, 0, sizeof(server));
    strcpy(server.szServerName, "127.0.0.1");
    server.dwPort = port;
    server.dwTimeout = 10;
    hr = IIMAPTransport_Connect(imap, &server, TRUE, FALSE);
    CHECK(hr == S_OK, "Connect %08lx", HR(hr));
    CHECK(cb.logon_prompt, "the callback was asked for the account");
    CHECK(IIMAPTransport_IsState(imap, IXP_IS_CONNECTED) == S_OK, "connected");
    CHECK(IIMAPTransport_IsState(imap, IXP_IS_AUTHENTICATED) == S_OK, "authenticated");
    CHECK(IIMAPTransport_IsState(imap, IXP_IS_READY) == S_OK, "ready");
    hr = IIMAPTransport_Capability(imap, &caps);
    CHECK(hr == S_OK && caps == (IMAP_CAPABILITY_IMAP4rev1 | IMAP_CAPABILITY_IDLE), "capabilities %08lx %lx", HR(hr), caps);

    IIMAPTransport_Select(imap, 11, 22, NULL, "INBOX");
    CHECK(IIMAPTransport_IsState(imap, IXP_IS_BUSY) == S_OK, "busy with a command");
    pump_until(&cb.completions, 1, 5000);
    CHECK(cb.completions == 1 && cb.last_hr[0] == S_OK && cb.last_wparam[0] == 11 && cb.last_lparam[0] == 22,
          "Select completion %d wparam %Id lparam %Id", cb.completions, cb.last_wparam[0], cb.last_lparam[0]);
    CHECK(cb.exists == 1 && cb.last_exists == 3 && cb.recent == 1 && cb.unseen == 1 && cb.status_messages == 2,
          "mailbox counts exists %d/%lu recent %d unseen %d/%lu", cb.exists, cb.last_exists, cb.recent, cb.unseen, cb.status_messages);
    CHECK(cb.flags_resp == 1 && cb.flagsmask == IMAP_MSG_ALLFLAGS, "flags %d %lx", cb.flags_resp, cb.flagsmask);
    CHECK(cb.permflags == 1 && cb.permmask == (IMAP_MSG_SEEN | IMAP_MSG_DELETED), "permanent flags %lx", cb.permmask);
    CHECK(cb.uidvalidity == 1 && cb.last_uidv == 777, "uidvalidity %lu", cb.last_uidv);
    CHECK(cb.rw >= 1, "read-write");
    CHECK(IIMAPTransport_IsState(imap, IXP_IS_READY) == S_OK, "ready again");

    IIMAPTransport_List(imap, 1, 2, NULL, "", "*");
    pump_until(&cb.completions, 2, 5000);
    CHECK(cb.listings == 2 && !strcmp(cb.listed[0], "Top") && (cb.listed[0][30] & 0xf) == IMAP_MBOX_NOSELECT &&
          !strcmp(cb.listed[1], "INBOX"), "listing %d '%s' '%s'", cb.listings, cb.listed[0], cb.listed[1]);

    IIMAPTransport_NewIRangeList(imap, &range);
    IRangeList_AddRange(range, 1, 2);
    IIMAPTransport_Search(imap, 3, 4, NULL, "UNSEEN", FALSE, NULL, FALSE);
    pump_until(&cb.completions, 3, 5000);
    CHECK(cb.searches == 1 && cb.search_card == 3, "search results %lu", cb.search_card);

    IIMAPTransport_Fetch(imap, 5, 6, NULL, range, FALSE, "(FLAGS UID RFC822.SIZE INTERNALDATE)");
    pump_until(&cb.completions, 4, 5000);
    {
        SYSTEMTIME st;
        FileTimeToSystemTime(&cb.fetch_date, &st);
        CHECK(cb.fetch_msgs == 2 && cb.fetch_uid == 101 && cb.fetch_size == 66 && cb.fetch_flags == (IMAP_MSG_SEEN | IMAP_MSG_ANSWERED) &&
              st.wYear == 1996 && st.wMonth == 7 && st.wDay == 17 && st.wHour == 9 && st.wMinute == 44,
              "fetch %d uid %lu size %lu flags %lx date %04d-%02d-%02d %02d:%02d", cb.fetch_msgs, cb.fetch_uid, cb.fetch_size,
              cb.fetch_flags, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    }
    IIMAPTransport_MsgSeqNumToUID(imap, 2, &uid);
    CHECK(uid == 101, "sequence number table learned the uid: %lu", uid);
    hr = IIMAPTransport_MsgSeqNumToUID(imap, 99, &uid);
    CHECK(hr == E_INVALIDARG, "sequence number out of range %08lx", HR(hr));

    IIMAPTransport_Fetch(imap, 7, 8, NULL, range, TRUE, "(BODY[])");
    pump_until(&cb.completions, 5, 5000);
    CHECK(cb.fetch_bodies >= 1 && !strcmp(cb.body, "hello world"), "fetched body part '%s'", cb.body);

    IIMAPTransport_Store(imap, 9, 9, NULL, range, FALSE, "+FLAGS (\\Deleted)");
    pump_until(&cb.completions, 6, 5000);
    CHECK(cb.last_hr[5] == S_OK && cb.fetch_flags == (IMAP_MSG_SEEN | IMAP_MSG_DELETED), "store flags %lx", cb.fetch_flags);
    IIMAPTransport_Copy(imap, 10, 10, NULL, range, FALSE, "Archive");
    pump_until(&cb.completions, 7, 5000);
    CHECK(cb.last_hr[6] == S_OK, "copy");
    IIMAPTransport_Status(imap, 11, 11, NULL, "INBOX", "(MESSAGES UIDNEXT)");
    pump_until(&cb.completions, 8, 5000);
    CHECK(cb.status_resp == 1 && cb.status_messages == 3 && cb.status_uidnext == 103, "status %lu %lu", cb.status_messages, cb.status_uidnext);

    mail = mem_stream("Subject: appended\r\n\r\nbody of the appended mail");
    hr = IIMAPTransport_Append(imap, 12, 12, NULL, "Drafts", "\\Seen", zero, mail);
    CHECK(hr == S_OK, "Append %08lx", HR(hr));
    pump_until(&cb.completions, 9, 5000);
    CHECK(cb.last_hr[8] == S_OK && cb.appends == 1, "append completed, progress %d", cb.appends);
    IStream_Release(mail);

    IIMAPTransport_Noop(imap, 13, 13, NULL);
    pump_until(&cb.completions, 10, 5000);
    CHECK(cb.last_hr[9] == S_OK, "noop");
    IIMAPTransport_Expunge(imap, 14, 14, NULL);
    pump_until(&cb.completions, 11, 5000);
    CHECK(cb.deleted == 1 && cb.last_hr[10] == S_OK, "expunge: %d deleted", cb.deleted);
    IIMAPTransport_Create(imap, 15, 15, NULL, "New");
    IIMAPTransport_Rename(imap, 16, 16, NULL, "New", "Newer");
    IIMAPTransport_Subscribe(imap, 17, 17, NULL, "Newer");
    IIMAPTransport_Unsubscribe(imap, 18, 18, NULL, "Newer");
    IIMAPTransport_Lsub(imap, 19, 19, NULL, "", "*");
    IIMAPTransport_Examine(imap, 20, 20, NULL, "Archive");
    IIMAPTransport_Delete(imap, 21, 21, NULL, "Gone");
    IIMAPTransport_Close(imap, 22, 22, NULL);
    pump_until(&cb.completions, 19, 8000);
    CHECK(cb.completions == 19, "eight more commands completed: %d", cb.completions);
    CHECK(cb.last_hr[11] == S_OK && cb.last_hr[12] == S_OK && cb.last_hr[13] == S_OK && cb.last_hr[14] == S_OK, "mailbox commands ok");
    CHECK(cb.last_hr[17] != S_OK && cb.last_wparam[17] == 21, "Delete of a missing mailbox fails: %08lx", HR(cb.last_hr[17]));
    CHECK(cb.last_hr[18] == S_OK, "close");

    IIMAPTransport_ResizeMsgSeqNumTable(imap, 5);
    IIMAPTransport_UpdateSeqNumToUID(imap, 3, 333);
    IIMAPTransport_MsgSeqNumToUID(imap, 3, &uid);
    CHECK(uid == 333, "table update");
    hr = IIMAPTransport_UpdateSeqNumToUID(imap, 9, 1);
    CHECK(hr == E_INVALIDARG, "table update out of range");
    IIMAPTransport_RemoveSequenceNum(imap, 1);
    IIMAPTransport_GetHighestMsgSeqNum(imap, &uid);
    CHECK(uid == 4, "highest after removal %lu", uid);
    IIMAPTransport_MsgSeqNumToUID(imap, 2, &uid);
    CHECK(uid == 333, "numbers shift down");
    {
        DWORD *arr, n;
        hr = IIMAPTransport_GetMsgSeqNumToUIDArray(imap, &arr, &n);
        CHECK(hr == S_OK && n == 4 && arr[1] == 333, "array copy");
        CoTaskMemFree(arr);
    }
    IIMAPTransport_ResetMsgSeqNumToUID(imap);
    IIMAPTransport_GetHighestMsgSeqNum(imap, &uid);
    CHECK(uid == 0, "reset");
    hr = IIMAPTransport_SetDefaultCBHandler(imap, NULL);
    CHECK(hr == E_INVALIDARG, "default handler NULL");

    hr = IIMAPTransport_Disconnect(imap);
    CHECK(hr == S_OK, "Disconnect %08lx", HR(hr));
    CHECK(IIMAPTransport_IsState(imap, IXP_IS_CONNECTED) == S_FALSE, "disconnected");
    IRangeList_Release(range);
    IIMAPTransport_Release(imap);
}

static HRESULT WINAPI smtp_OnResponse(ISMTPCallback *i, LPSMTPRESPONSE r) { return S_OK; }
static const ISMTPCallbackVtbl smtp_cb_vtbl =
{
    (void *)cb_QI, (void *)cb_AddRef, (void *)cb_Release, (void *)cb_OnTimeout, (void *)cb_OnLogonPrompt,
    (void *)cb_OnPrompt, (void *)cb_OnStatus, (void *)cb_OnError, (void *)cb_OnCommand, smtp_OnResponse
};

static void test_smtp_pop3(int port)
{
    struct { ISMTPCallback iface; LONG ref; } smtp_cb = {{(ISMTPCallbackVtbl *)&smtp_cb_vtbl}, 1};
    ISMTPTransport2 *smtp;
    ISMTPTransport *smtp1;
    IPOP3Transport *pop3;
    HRESULT hr;
    SMTPMESSAGE2 m2;

    hr = CreateSMTPTransport(&smtp1);
    CHECK(hr == S_OK, "CreateSMTPTransport");
    ISMTPTransport_QueryInterface(smtp1, &IID_ISMTPTransport2, (void **)&smtp);
    hr = ISMTPTransport2_InitNew(smtp, NULL, &smtp_cb.iface);
    CHECK(hr == S_OK, "smtp InitNew %08lx", HR(hr));
    CHECK(ISMTPTransport2_IsState(smtp, IXP_IS_CONNECTED) == S_FALSE, "smtp not connected");
    CHECK(ISMTPTransport2_IsState(smtp, IXP_IS_BUSY) == S_FALSE, "smtp not busy");
    CHECK(ISMTPTransport2_IsState(smtp, 17) == E_INVALIDARG, "smtp bad state");
    hr = ISMTPTransport2_SetWindow(smtp);
    CHECK(hr == S_OK, "SetWindow %08lx", HR(hr));
    hr = ISMTPTransport2_SetWindow(smtp);
    CHECK(hr == S_OK, "SetWindow twice");
    hr = ISMTPTransport2_ResetWindow(smtp);
    CHECK(hr == S_OK, "ResetWindow");
    hr = ISMTPTransport2_ResetWindow(smtp);
    CHECK(hr == S_OK, "ResetWindow twice");
    hr = ISMTPTransport2_CommandRCPT2(smtp, NULL, ADDR_DSN_FAILURE);
    CHECK(hr == E_INVALIDARG, "RCPT2 NULL");
    hr = ISMTPTransport2_CommandRCPT2(smtp, "a@b.org", ADDR_DSN_FAILURE);
    CHECK(HR(hr) == 0x800ccc05, "RCPT2 not connected %08lx", HR(hr));
    hr = ISMTPTransport2_SendDataStream(smtp, NULL, 3);
    CHECK(hr == E_INVALIDARG, "SendDataStream NULL");
    memset(&m2, 0, sizeof(m2));
    hr = ISMTPTransport2_SendMessage2(smtp, NULL);
    CHECK(hr == E_INVALIDARG, "SendMessage2 NULL");

    /* the script server hands back the lines it got in port+1's capture */
    {
        INETSERVER server;
        IStream *data;
        struct { ISMTPCallback c; } dummy;
        (void)dummy;
        memset(&server, 0, sizeof(server));
        strcpy(server.szServerName, "127.0.0.1");
        server.dwPort = port;
        hr = ISMTPTransport2_Connect(smtp, &server, FALSE, FALSE);
        CHECK(hr == S_OK, "smtp Connect %08lx", HR(hr));
        {
            MSG msg;
            DWORD end = GetTickCount() + 1500;
            while (GetTickCount() < end)
            {
                while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
                Sleep(5);
            }
        }
        CHECK(ISMTPTransport2_IsState(smtp, IXP_IS_CONNECTED) == S_OK, "smtp connected");
        hr = ISMTPTransport2_CommandRCPT2(smtp, "rcpt@example.org", ADDR_DSN_SUCCESS | ADDR_DSN_FAILURE);
        CHECK(hr == S_OK, "RCPT2 %08lx", HR(hr));
        {
            MSG msg;
            DWORD end = GetTickCount() + 800;
            while (GetTickCount() < end)
            {
                while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
                Sleep(5);
            }
        }
        data = mem_stream("line one\r\n.dot line\r\n..two\r\nlast");
        hr = ISMTPTransport2_SendDataStream(smtp, data, 33);
        CHECK(hr == S_OK, "SendDataStream %08lx", HR(hr));
        IStream_Release(data);
        hr = ISMTPTransport2_CommandDOT(smtp);
        CHECK(hr == S_OK, "CommandDOT %08lx", HR(hr));
        {
            MSG msg;
            DWORD end = GetTickCount() + 800;
            while (GetTickCount() < end)
            {
                while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
                Sleep(5);
            }
        }
        ISMTPTransport2_DropConnection(smtp);
    }
    ISMTPTransport2_Release(smtp);
    ISMTPTransport_Release(smtp1);

    hr = CreatePOP3Transport(&pop3);
    CHECK(hr == S_OK, "CreatePOP3Transport");
    CHECK(IPOP3Transport_IsState(pop3, IXP_IS_CONNECTED) == S_FALSE, "pop3 not connected");
    CHECK(IPOP3Transport_IsState(pop3, IXP_IS_AUTHENTICATED) == S_FALSE, "pop3 not authenticated");
    hr = IPOP3Transport_MarkItem(pop3, POP3_MARK_FOR_DELE, 3, TRUE);
    CHECK(hr == S_OK, "MarkItem %08lx", HR(hr));
    hr = IPOP3Transport_MarkItem(pop3, POP3_MARK_FOR_DELE, 3, FALSE);
    CHECK(hr == S_OK, "unmark");
    hr = IPOP3Transport_MarkItem(pop3, POP3_MARK_FOR_DELE, 0, TRUE);
    CHECK(hr == E_INVALIDARG, "MarkItem id 0 %08lx", HR(hr));
    hr = IPOP3Transport_CommandAUTH(pop3, NULL);
    CHECK(hr == E_INVALIDARG, "AUTH NULL");
    hr = IPOP3Transport_CommandAUTH(pop3, "PLAIN");
    CHECK(HR(hr) == 0x800ccc05, "AUTH not connected %08lx", HR(hr));
    IPOP3Transport_Release(pop3);
}

static void test_mhtml_protocol(void)
{
    IUnknown *unk;
    IInternetProtocol *proto;
    LARGE_INTEGER zero = {{0}};
    ULARGE_INTEGER pos;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_IMimeHtmlProtocol, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&unk);
    CHECK(hr == S_OK, "create protocol %08lx", HR(hr));
    if (hr != S_OK) return;
    IUnknown_QueryInterface(unk, &IID_IInternetProtocol, (void **)&proto);
    hr = IInternetProtocol_Seek(proto, zero, STREAM_SEEK_SET, &pos);
    CHECK(hr == E_FAIL, "Seek before Start %08lx", HR(hr));
    hr = IInternetProtocol_LockRequest(proto, 0);
    CHECK(hr == S_OK, "LockRequest");
    hr = IInternetProtocol_UnlockRequest(proto);
    CHECK(hr == S_OK, "UnlockRequest");
    hr = IInternetProtocol_Suspend(proto);
    CHECK(hr == E_NOTIMPL, "Suspend %08lx", HR(hr));
    hr = IInternetProtocol_Resume(proto);
    CHECK(hr == E_NOTIMPL, "Resume %08lx", HR(hr));
    hr = IInternetProtocol_Abort(proto, E_ABORT, 0);
    CHECK(hr == S_OK, "Abort without a sink %08lx", HR(hr));
    IInternetProtocol_Release(proto);
    IUnknown_Release(unk);
}

int main(int argc, char **argv)
{
    int imap_port = 0, smtp_port = 0;
    FILE *f;

    HMODULE inetcomm = LoadLibraryA("inetcomm.dll");
#define LOAD(n) p##n = (void *)GetProcAddress(inetcomm, #n)
    LOAD(MimeOleCreateMessage); LOAD(MimeOleGetAllocator); LOAD(MimeGetAddressFormatW);
    LOAD(MimeOleGetPropertySchema); LOAD(MimeOleCreateSecurity); LOAD(MimeOleFindCharset); LOAD(CreateRangeList);
    LOAD(CreateIMAPTransport); LOAD(CreateSMTPTransport); LOAD(CreatePOP3Transport);
    if (!pCreateRangeList || !pMimeGetAddressFormatW) { printf("FAIL  exports missing\nRESULT: FAIL\n"); return 1; }
    CoInitialize(NULL);
    if ((f = fopen("fastinetcomm-ports.txt", "r")))
    {
        fscanf(f, "%d %d", &imap_port, &smtp_port);
        fclose(f);
    }
    test_streams_and_bodies();
    test_address_table();
    test_schema_and_security();
    test_message_tree();
    test_range_list();
    if (imap_port) test_imap(imap_port);
    else CHECK(0, "no server ports");
    if (smtp_port) test_smtp_pop3(smtp_port);
    test_mhtml_protocol();

    printf("%d checks, %d failed\n", checks, bad);
    printf(bad ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return bad != 0;
}
