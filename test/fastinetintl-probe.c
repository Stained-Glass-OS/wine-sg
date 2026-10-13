/* inetcomm RFC 2047 headers (patch 3170), run by test/fastinetintl-gate.sh.
 * Calls IMimeInternational through a hand-written vtable (mingw has no mimeole.h). */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <propidl.h>
#include <stdio.h>
#include <string.h>

typedef struct { BOOL fRfc1522Allowed, fRfc1522Used, fAllow8bit; HANDLE hRfc1522Cset; } RI;
typedef struct { char szName[128]; UINT cpiWindows, cpiInternet; HANDLE hCharset; DWORD res; } CSI;
typedef struct { DWORD mask; DWORD cp; BOOL valid; ULONG maxchar; BOOL inet; DWORD fam; char name[128], body[128], header[128], web[128], fixed[128], var[128]; int news, mail; DWORD res; } CPI;

typedef struct IMI IMI;
typedef struct {
    void *qi, *addref;
    ULONG (WINAPI *Release)(IMI *);
    void *setdef;
    HRESULT (WINAPI *GetDefaultCharset)(IMI *, HANDLE *);
    void *getcp;
    HRESULT (WINAPI *FindCharset)(IMI *, const char *, HANDLE *);
    void *getcsinfo;
    HRESULT (WINAPI *GetCodePageInfo)(IMI *, DWORD, CPI *);
    void *canconv;
    HRESULT (WINAPI *DecodeHeader)(IMI *, HANDLE, const char *, PROPVARIANT *, RI *);
    HRESULT (WINAPI *EncodeHeader)(IMI *, HANDLE, PROPVARIANT *, char **, RI *);
    void *convbuf, *convstr;
    HRESULT (WINAPI *InetReset)(IMI *);
    HRESULT (WINAPI *InetString)(IMI *, DWORD, DWORD, const char *, int *, char *, int *);
    HRESULT (WINAPI *Rfc1522Decode)(IMI *, const char *, char *, ULONG, char **);
    HRESULT (WINAPI *Rfc1522Encode)(IMI *, const char *, HANDLE, char **);
} VT;
struct IMI { const VT *vt; };

HRESULT WINAPI MimeOleGetInternat(IMI **);
HRESULT WINAPI MimeOleDecodeHeader(HANDLE, const char *, PROPVARIANT *, RI *);
HRESULT WINAPI MimeOleEncodeHeader(HANDLE, PROPVARIANT *, char **, RI *);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

int main(void)
{
    IMI *mi;
    HANDLE latin1, utf8;
    HRESULT hr;
    PROPVARIANT pv;
    RI ri;
    char *s, cs[32];
    WCHAR w;
    CPI cpi;

    CoInitialize(NULL);
    hr = MimeOleGetInternat(&mi);
    check(hr == S_OK, "MimeOleGetInternat");
    check(mi->vt->FindCharset(mi, "iso-8859-1", &latin1) == S_OK, "find iso-8859-1");
    check(mi->vt->FindCharset(mi, "utf-8", &utf8) == S_OK, "find utf-8");

    /* Rfc1522Decode: raw bytes, charset reported, whitespace between words dropped */
    s = NULL;
    hr = mi->vt->Rfc1522Decode(mi, "=?iso-8859-1?Q?caf=E9_au?= =?iso-8859-1?Q?_lait?=", cs, sizeof(cs), &s);
    check(hr == S_OK && s && !strcmp(s, "caf\xe9 au lait"), "Rfc1522Decode Q joined words");
    check(!strcmp(cs, "iso-8859-1"), "Rfc1522Decode charset out");
    CoTaskMemFree(s);
    s = NULL;
    hr = mi->vt->Rfc1522Decode(mi, "Re: =?UTF-8?B?w6k=?= ok", cs, sizeof(cs), &s);
    check(hr == S_OK && s && !strcmp(s, "Re: \xc3\xa9 ok") && !strcmp(cs, "UTF-8"), "Rfc1522Decode B with literals");
    CoTaskMemFree(s);
    s = NULL;
    hr = mi->vt->Rfc1522Decode(mi, "plain", cs, sizeof(cs), &s);
    check(FAILED(hr), "Rfc1522Decode plain text fails");
    check(mi->vt->Rfc1522Decode(mi, NULL, cs, sizeof(cs), &s) == E_INVALIDARG, "Rfc1522Decode NULL");

    /* DecodeHeader to Unicode */
    memset(&pv, 0, sizeof(pv)); pv.vt = VT_LPWSTR;
    memset(&ri, 0xff, sizeof(ri));
    hr = mi->vt->DecodeHeader(mi, NULL, "=?utf-8?Q?=C3=A9t=C3=A9?= done", &pv, &ri);
    check(hr == S_OK && pv.pwszVal && pv.pwszVal[0] == 0xe9 && pv.pwszVal[1] == 't' && pv.pwszVal[2] == 0xe9 &&
          !lstrcmpW(pv.pwszVal + 3, L" done"), "DecodeHeader utf-8 to LPWSTR");
    check(ri.fRfc1522Used == TRUE && ri.hRfc1522Cset == utf8, "DecodeHeader info used + charset");
    CoTaskMemFree(pv.pwszVal);

    memset(&pv, 0, sizeof(pv)); pv.vt = VT_LPWSTR;
    hr = mi->vt->DecodeHeader(mi, latin1, "just ascii", &pv, &ri);
    check(hr == S_OK && !lstrcmpW(pv.pwszVal, L"just ascii") && ri.fRfc1522Used == FALSE && ri.hRfc1522Cset == NULL,
          "DecodeHeader plain: not used");
    CoTaskMemFree(pv.pwszVal);

    /* hCharset overrides the charset in the word */
    memset(&pv, 0, sizeof(pv)); pv.vt = VT_LPWSTR;
    hr = mi->vt->DecodeHeader(mi, latin1, "=?utf-8?Q?=C3=A9?=", &pv, NULL);
    check(hr == S_OK && pv.pwszVal[0] == 0xc3 && pv.pwszVal[1] == 0xa9, "hCharset overrides word charset");
    CoTaskMemFree(pv.pwszVal);

    /* LPSTR result in the Windows code page of the charset (1252) */
    memset(&pv, 0, sizeof(pv)); pv.vt = VT_LPSTR;
    hr = mi->vt->DecodeHeader(mi, latin1, "=?iso-8859-1?B?6Q==?=", &pv, &ri);
    check(hr == S_OK && pv.pszVal && !strcmp(pv.pszVal, "\xe9"), "DecodeHeader B to LPSTR");
    CoTaskMemFree(pv.pszVal);

    memset(&pv, 0, sizeof(pv)); pv.vt = VT_I4;
    check(mi->vt->DecodeHeader(mi, latin1, "x", &pv, &ri) == E_INVALIDARG, "DecodeHeader bad vt");
    check(mi->vt->DecodeHeader(mi, (HANDLE)0x7777, "x", &pv, &ri) == E_INVALIDARG, "DecodeHeader bad vt first");
    pv.vt = VT_LPSTR;
    check(mi->vt->DecodeHeader(mi, (HANDLE)0x7777, "x", &pv, &ri) == (HRESULT)0x800cce09, "DecodeHeader bad handle");

    /* EncodeHeader */
    pv.vt = VT_LPSTR; pv.pszVal = "plain ascii";
    ri.fRfc1522Allowed = TRUE; ri.fAllow8bit = FALSE; ri.fRfc1522Used = TRUE;
    hr = mi->vt->EncodeHeader(mi, latin1, &pv, &s, &ri);
    check(hr == S_OK && !strcmp(s, "plain ascii") && ri.fRfc1522Used == FALSE, "EncodeHeader ascii unchanged");
    CoTaskMemFree(s);

    pv.pszVal = "caf\xe9";
    hr = mi->vt->EncodeHeader(mi, latin1, &pv, &s, &ri);
    check(hr == S_OK && !strcmp(s, "=?iso-8859-1?Q?caf=E9?=") && ri.fRfc1522Used == TRUE, "EncodeHeader Q");
    printf("  -> %s\n", s);
    CoTaskMemFree(s);

    ri.fAllow8bit = TRUE;
    hr = mi->vt->EncodeHeader(mi, latin1, &pv, &s, &ri);
    check(hr == S_OK && !strcmp(s, "caf\xe9") && ri.fRfc1522Used == FALSE, "EncodeHeader allow 8bit: raw");
    CoTaskMemFree(s);
    ri.fAllow8bit = FALSE; ri.fRfc1522Allowed = FALSE;
    hr = mi->vt->EncodeHeader(mi, latin1, &pv, &s, &ri);
    check(hr == S_OK && !strcmp(s, "caf\xe9") && ri.fRfc1522Used == FALSE, "EncodeHeader not allowed: raw");
    CoTaskMemFree(s);
    ri.fRfc1522Allowed = TRUE;

    /* UTF-8 always encoded, even for ASCII, even when 8bit allowed */
    ri.fAllow8bit = TRUE;
    pv.pszVal = "ascii";
    hr = mi->vt->EncodeHeader(mi, utf8, &pv, &s, &ri);
    check(hr == S_OK && !strncmp(s, "=?utf-8?B?", 10) && ri.fRfc1522Used == TRUE, "EncodeHeader utf-8 always encoded");
    printf("  -> %s\n", s);
    CoTaskMemFree(s);
    ri.fAllow8bit = FALSE;

    /* NULL info: allowed TRUE, 8bit FALSE; LPWSTR input */
    pv.vt = VT_LPWSTR; pv.pwszVal = L"\x00e9t\x00e9";
    hr = mi->vt->EncodeHeader(mi, latin1, &pv, &s, NULL);
    printf("  -> hr %lx %s\n", hr, s);
    check(hr == S_OK && !strcmp(s, "=?iso-8859-1?B?6XTp?="), "EncodeHeader LPWSTR, NULL info");
    CoTaskMemFree(s);

    /* long text is folded into words of at most 75 characters */
    {
        char longs[400];
        char *p, *line;
        int ok = 1;
        memset(longs, 'a', sizeof(longs)); longs[399] = 0; longs[10] = '\xe9';
        pv.vt = VT_LPSTR; pv.pszVal = longs;
        hr = mi->vt->EncodeHeader(mi, latin1, &pv, &s, NULL);
        check(hr == S_OK, "EncodeHeader long");
        for (line = s; line && *line; )
        {
            p = strstr(line, "\r\n ");
            if ((p ? p - line : strlen(line)) > 75) ok = 0;
            line = p ? p + 3 : NULL;
        }
        check(ok && strstr(s, "\r\n "), "EncodeHeader long folded, words <= 75");
        /* round trip */
        memset(&pv, 0, sizeof(pv)); pv.vt = VT_LPSTR;
        hr = mi->vt->DecodeHeader(mi, latin1, s, &pv, NULL);
        check(hr == S_OK && !strcmp(pv.pszVal, longs), "long text round trip");
        CoTaskMemFree(pv.pszVal);
        CoTaskMemFree(s);
    }

    /* Rfc1522Encode: only when 8-bit present */
    s = NULL;
    check(mi->vt->Rfc1522Encode(mi, "ascii", latin1, &s) == E_FAIL, "Rfc1522Encode ascii -> E_FAIL");
    hr = mi->vt->Rfc1522Encode(mi, "\xe9""t\xe9", latin1, &s);
    printf("  -> hr %lx %s\n", hr, s);
    check(hr == S_OK && !strcmp(s, "=?iso-8859-1?B?6XTp?="), "Rfc1522Encode");
    CoTaskMemFree(s);
    check(mi->vt->Rfc1522Encode(mi, NULL, latin1, &s) == E_INVALIDARG, "Rfc1522Encode NULL value");
    check(mi->vt->Rfc1522Encode(mi, "x", NULL, &s) == E_INVALIDARG, "Rfc1522Encode NULL charset");
    check(mi->vt->Rfc1522Encode(mi, "x", (HANDLE)0x7777, &s) == (HRESULT)0x800cce09, "Rfc1522Encode bad handle");

    /* exported wrappers */
    memset(&pv, 0, sizeof(pv)); pv.vt = VT_LPWSTR;
    hr = MimeOleDecodeHeader(NULL, "=?iso-8859-1?Q?=E9?=", &pv, NULL);
    check(hr == S_OK && pv.pwszVal[0] == 0xe9, "MimeOleDecodeHeader");
    CoTaskMemFree(pv.pwszVal);
    pv.vt = VT_LPSTR; pv.pszVal = "\xe9";
    hr = MimeOleEncodeHeader(latin1, &pv, &s, NULL);
    check(hr == S_OK && !strcmp(s, "=?iso-8859-1?B?6Q==?="), "MimeOleEncodeHeader");
    CoTaskMemFree(s);

    /* GetCodePageInfo */
    memset(&cpi, 0, sizeof(cpi));
    hr = mi->vt->GetCodePageInfo(mi, 1252, &cpi);
    check(hr == S_OK && cpi.cp == 1252 && cpi.valid && cpi.body[0] && cpi.name[0], "GetCodePageInfo 1252");
    printf("  -> name=%s body=%s header=%s web=%s\n", cpi.name, cpi.body, cpi.header, cpi.web);
    check(mi->vt->GetCodePageInfo(mi, 1252, NULL) == E_INVALIDARG, "GetCodePageInfo NULL");
    hr = mi->vt->GetCodePageInfo(mi, 99999, &cpi); printf("  -> invalid hr %lx\n", hr);
    check(hr == (HRESULT)0x800cce05, "GetCodePageInfo invalid");

    /* MLANG_ConvertInetString: latin1 -> utf-8 */
    {
        int in = 1, out = 8;
        char buf[8] = {0};
        check(mi->vt->InetReset(mi) == S_OK, "ConvertInetReset");
        hr = mi->vt->InetString(mi, 28591, 65001, "\xe9", &in, buf, &out);
        check(hr == S_OK && out == 2 && !memcmp(buf, "\xc3\xa9", 2), "ConvertInetString latin1->utf8");
    }

    mi->vt->Release(mi);
    printf("failures: %d\nRESULT: %s\n", failures, failures ? "FAIL" : "PASS");
    return failures != 0;
}
