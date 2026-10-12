/* inetcomm batch (patches/sg/2046), run by test/mimeprops-gate.sh. Families:
 * property set operations of an IMimeBody (SetPropInfo, AppendProp, CopyProps,
 * MoveProps, DeleteExcept, QueryProp, Clone), EnumProps and its enumerator,
 * the display name, IsType and CopyTo. IMimeBody is called through its
 * vtable slots.
 *
 *   mimeprops-probe.exe */
#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define MIME_E_NOT_FOUND ((HRESULT)0x800cce05)
enum { IET_BINARY = 0, IET_BASE64 = 1, IET_UUENCODE = 2, IET_QP = 3, IET_7BIT = 4, IET_8BIT = 5 };
enum { IBT_SECURE, IBT_ATTACHMENT, IBT_EMPTY, IBT_CSETTAGGED, IBT_AUTOATTACH };
enum { PIM_CHARSET = 1, PIM_ENCODINGTYPE = 2, PIM_ROWNUMBER = 4, PIM_FLAGS = 8, PIM_PROPID = 0x10, PIM_VALUES = 0x20 };

typedef struct { DWORD dwMask; void *hCharset; int ietEncoding; DWORD dwRowNumber, dwFlags, dwPropId, cValues; VARTYPE vtDefault, vtCurrent; } PROPINFO;
typedef struct { LPSTR pszName; void *hRow; DWORD dwPropId; } ENUMPROP;

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

#define SLOT(obj, n) ((*(void ***)(obj))[n])
#define BRelease(o) ((ULONG (WINAPI *)(void *))SLOT(o, 2))(o)
#define IsDirty(o) ((HRESULT (WINAPI *)(void *))SLOT(o, 4))(o)
#define Load(o, s) ((HRESULT (WINAPI *)(void *, IStream *))SLOT(o, 5))(o, s)
#define Save(o, s, c) ((HRESULT (WINAPI *)(void *, IStream *, BOOL))SLOT(o, 6))(o, s, c)
#define GetPropInfo(o, n, i) ((HRESULT (WINAPI *)(void *, const char *, PROPINFO *))SLOT(o, 9))(o, n, i)
#define SetPropInfo(o, n, i) ((HRESULT (WINAPI *)(void *, const char *, const PROPINFO *))SLOT(o, 10))(o, n, i)
#define BGetProp(o, n, f, v) ((HRESULT (WINAPI *)(void *, const char *, DWORD, PROPVARIANT *))SLOT(o, 11))(o, n, f, v)
#define BSetProp(o, n, f, v) ((HRESULT (WINAPI *)(void *, const char *, DWORD, const PROPVARIANT *))SLOT(o, 12))(o, n, f, v)
#define AppendProp(o, n, f, v) ((HRESULT (WINAPI *)(void *, const char *, DWORD, PROPVARIANT *))SLOT(o, 13))(o, n, f, v)
#define DeleteProp(o, n) ((HRESULT (WINAPI *)(void *, const char *))SLOT(o, 14))(o, n)
#define CopyProps(o, c, n, d) ((HRESULT (WINAPI *)(void *, ULONG, const char **, void *))SLOT(o, 15))(o, c, n, d)
#define MoveProps(o, c, n, d) ((HRESULT (WINAPI *)(void *, ULONG, const char **, void *))SLOT(o, 16))(o, c, n, d)
#define DeleteExcept(o, c, n) ((HRESULT (WINAPI *)(void *, ULONG, const char **))SLOT(o, 17))(o, c, n)
#define QueryProp(o, n, c, s, cs) ((HRESULT (WINAPI *)(void *, const char *, const char *, boolean, boolean))SLOT(o, 18))(o, n, c, s, cs)
#define Clone(o, p) ((HRESULT (WINAPI *)(void *, void **))SLOT(o, 24))(o, p)
#define BEnumProps(o, f, p) ((HRESULT (WINAPI *)(void *, DWORD, void **))SLOT(o, 27))(o, f, p)
#define IsType(o, t) ((HRESULT (WINAPI *)(void *, int))SLOT(o, 28))(o, t)
#define SetDisplayName(o, n) ((HRESULT (WINAPI *)(void *, const char *))SLOT(o, 29))(o, n)
#define GetDisplayName(o, p) ((HRESULT (WINAPI *)(void *, char **))SLOT(o, 30))(o, p)
#define GetData(o, e, s) ((HRESULT (WINAPI *)(void *, int, IStream **))SLOT(o, 36))(o, e, s)
#define SetData(o, e, a, b, iid, p) ((HRESULT (WINAPI *)(void *, int, const char *, const char *, const GUID *, void *))SLOT(o, 37))(o, e, a, b, iid, p)
#define CopyTo(o, d) ((HRESULT (WINAPI *)(void *, void *))SLOT(o, 39))(o, d)
/* IMimeEnumProperties */
#define ENext(o, n, p, f) ((HRESULT (WINAPI *)(void *, ULONG, ENUMPROP *, ULONG *))SLOT(o, 3))(o, n, p, f)
#define ESkip(o, n) ((HRESULT (WINAPI *)(void *, ULONG))SLOT(o, 4))(o, n)
#define EReset(o) ((HRESULT (WINAPI *)(void *))SLOT(o, 5))(o)
#define EClone(o, p) ((HRESULT (WINAPI *)(void *, void **))SLOT(o, 6))(o, p)
#define ECount(o, p) ((HRESULT (WINAPI *)(void *, ULONG *))SLOT(o, 7))(o, p)

static const GUID IID_IStream_ = { 0x0000000c, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };

static IStream *stream_of(const char *text)
{
    IStream *s = NULL;
    ULONG n;
    LARGE_INTEGER zero = { 0 };
    CreateStreamOnHGlobal(NULL, TRUE, &s);
    s->lpVtbl->Write(s, text, strlen(text), &n);
    s->lpVtbl->Seek(s, zero, STREAM_SEEK_SET, NULL);
    return s;
}

static HRESULT (WINAPI *create_body)(void **);

static void *body_from(const char *headers)
{
    void *body = NULL;
    IStream *in = stream_of(headers);
    if (FAILED(create_body(&body)) || FAILED(Load(body, in))) { printf("FAIL  could not make a body\n"); failures++; }
    in->lpVtbl->Release(in);
    return body;
}

static int has(void *body, const char *name, const char *value)
{
    PROPVARIANT pv;
    int ok;
    PropVariantInit(&pv);
    pv.vt = VT_LPSTR;
    ok = BGetProp(body, name, 0, &pv) == S_OK && pv.vt == VT_LPSTR && !strcmp(pv.pszVal, value);
    PropVariantClear(&pv);
    return ok;
}

static int missing(void *body, const char *name)
{
    PROPVARIANT pv;
    PropVariantInit(&pv);
    pv.vt = VT_LPSTR;
    return BGetProp(body, name, 0, &pv) == MIME_E_NOT_FOUND;
}

static int saved_has(void *body, const char *text)
{
    static char buf[2048];
    IStream *s = stream_of("");
    ULONG n = 0;
    LARGE_INTEGER zero = { 0 };
    Save(body, s, FALSE);
    s->lpVtbl->Seek(s, zero, STREAM_SEEK_SET, NULL);
    s->lpVtbl->Read(s, buf, sizeof(buf) - 1, &n);
    buf[n] = 0;
    s->lpVtbl->Release(s);
    return strstr(buf, text) != NULL;
}

int main(void)
{
    HMODULE lib;
    void *body, *dest, *clone, *en, *en2;
    PROPINFO info;
    PROPVARIANT pv;
    ENUMPROP props[8];
    ULONG got, count;
    char *name;
    const char *names1[] = { "Subject" }, *names2[] = { "X-Custom", "Content-Type" }, *keep[] = { "Subject", "Received" };
    IStream *data;

    CoInitialize(NULL);
    lib = LoadLibraryA("inetcomm.dll");
    create_body = (void *)GetProcAddress(lib, "MimeOleCreateBody");
    if (!create_body) { check(0, "MimeOleCreateBody"); printf("RESULT: FAIL\n"); return 1; }

    body = body_from("Content-Type: text/plain; charset=us-ascii; name=\"f.txt\"\r\nSubject: one\r\nReceived: r1\r\nX-Custom: c1\r\n\r\n");

    /* SetPropInfo / GetPropInfo */
    memset(&info, 0, sizeof(info));
    info.dwMask = PIM_FLAGS | PIM_ROWNUMBER | PIM_ENCODINGTYPE;
    info.dwFlags = 0x1234; info.dwRowNumber = 7; info.ietEncoding = IET_QP;
    CHECK(SetPropInfo(body, "Subject", &info) == S_OK);
    memset(&info, 0xee, sizeof(info));
    info.dwMask = PIM_FLAGS | PIM_ROWNUMBER | PIM_ENCODINGTYPE;
    CHECK(GetPropInfo(body, "Subject", &info) == S_OK && info.dwFlags == 0x1234 && info.dwRowNumber == 7 && info.ietEncoding == IET_QP);
    memset(&info, 0, sizeof(info));
    info.dwMask = PIM_FLAGS;
    GetPropInfo(body, "X-Custom", &info);
    CHECK(info.dwFlags == 0);
    CHECK(SetPropInfo(body, "No-Such", &info) == MIME_E_NOT_FOUND);
    CHECK(SetPropInfo(body, NULL, &info) == E_INVALIDARG);
    CHECK(SetPropInfo(body, "Subject", NULL) == E_INVALIDARG);

    /* AppendProp: a second Received, and a new property */
    PropVariantInit(&pv);
    pv.vt = VT_LPSTR; pv.pszVal = (char *)"r2";
    CHECK(AppendProp(body, "Received", 0, &pv) == S_OK);
    memset(&info, 0, sizeof(info));
    info.dwMask = PIM_VALUES;
    CHECK(GetPropInfo(body, "Received", &info) == S_OK && info.cValues == 2);
    CHECK(has(body, "Received", "r1"));
    CHECK(saved_has(body, "Received: r1\r\nX-Custom: c1\r\nReceived: r2\r\n"));
    pv.pszVal = (char *)"n1";
    CHECK(AppendProp(body, "X-New", 0, &pv) == S_OK && has(body, "X-New", "n1"));
    CHECK(AppendProp(body, NULL, 0, &pv) == E_INVALIDARG);

    /* QueryProp */
    CHECK(QueryProp(body, "Subject", "one", FALSE, TRUE) == S_OK);
    CHECK(QueryProp(body, "Subject", "ONE", FALSE, TRUE) == S_FALSE);
    CHECK(QueryProp(body, "Subject", "ONE", FALSE, FALSE) == S_OK);
    CHECK(QueryProp(body, "Subject", "n", TRUE, TRUE) == S_OK);
    CHECK(QueryProp(body, "Subject", "x", TRUE, TRUE) == S_FALSE);
    CHECK(QueryProp(body, "Subject", "ON", TRUE, FALSE) == S_OK);
    CHECK(QueryProp(body, "Received", "r2", FALSE, TRUE) == S_OK);   /* any instance */
    CHECK(QueryProp(body, "No-Such", "x", TRUE, TRUE) == MIME_E_NOT_FOUND);

    /* CopyProps to another body */
    dest = body_from("Subject: old\r\nX-Custom: oldc\r\nX-Other: keepme\r\n\r\n");
    CHECK(CopyProps(body, 1, names1, dest) == S_OK);
    CHECK(has(dest, "Subject", "one") && has(dest, "X-Custom", "oldc") && has(dest, "Received", "r1") == 0);
    CHECK(has(body, "Subject", "one"));
    CHECK(CopyProps(body, 2, names2, dest) == S_OK);
    CHECK(has(dest, "X-Custom", "c1") && has(dest, "Content-Type", "text/plain") && has(dest, "X-Other", "keepme"));
    CHECK(CopyProps(body, 1, names1, NULL) == E_INVALIDARG);
    CHECK(CopyProps(body, 0, names1, dest) == S_OK);
    CHECK(CopyProps(body, 1, NULL, dest) == E_INVALIDARG);
    CHECK(CopyProps(body, 0, NULL, dest) == S_OK && has(dest, "Received", "r1") && has(dest, "X-New", "n1"));
    CHECK(saved_has(dest, "Received: r1\r\n") && saved_has(dest, "Received: r2\r\n"));
    CHECK(saved_has(dest, "name=f.txt"));
    BRelease(dest);

    /* MoveProps */
    dest = body_from("X-Other: keepme\r\n\r\n");
    CHECK(MoveProps(body, 1, names1, dest) == S_OK);
    CHECK(has(dest, "Subject", "one") && missing(body, "Subject") && has(body, "X-Custom", "c1"));
    BRelease(dest);

    /* DeleteExcept */
    CHECK(DeleteExcept(body, 1, keep + 1) == S_OK);
    CHECK(has(body, "Received", "r1") && missing(body, "X-Custom") && missing(body, "Content-Type") && missing(body, "X-New"));
    CHECK(DeleteExcept(body, 1, NULL) == E_INVALIDARG);
    CHECK(DeleteExcept(body, 0, NULL) == S_OK && missing(body, "Received"));
    BRelease(body);

    /* Clone and EnumProps */
    body = body_from("Content-Type: text/html; charset=utf-8\r\nSubject: s\r\nX-A: 1\r\nX-A: 2\r\n\r\n");
    CHECK(Clone(body, &clone) == S_OK && clone && clone != body);
    CHECK(has(clone, "Subject", "s") && has(clone, "X-A", "1"));
    CHECK(saved_has(clone, "Content-Type: text/html; charset=utf-8\r\n"));
    DeleteProp(clone, "Subject");
    CHECK(has(body, "Subject", "s"));    /* the clone is its own */
    BRelease(clone);
    CHECK(Clone(body, NULL) == E_INVALIDARG);

    CHECK(BEnumProps(body, 0, &en) == S_OK && en);
    count = 0;
    CHECK(ECount(en, &count) == S_OK && count == 4);
    memset(props, 0, sizeof(props));
    got = 99;
    CHECK(ENext(en, 2, props, &got) == S_OK && got == 2);
    CHECK(props[0].pszName && !strcasecmp(props[0].pszName, "Content-Type") && props[1].pszName && !strcasecmp(props[1].pszName, "Subject"));
    CHECK(props[0].dwPropId != props[1].dwPropId && props[0].hRow != props[1].hRow);
    CoTaskMemFree(props[0].pszName); CoTaskMemFree(props[1].pszName);
    CHECK(ESkip(en, 1) == S_OK);
    got = 99;
    CHECK(ENext(en, 4, props, &got) == S_FALSE && got == 1 && !strcasecmp(props[0].pszName, "X-A"));
    CoTaskMemFree(props[0].pszName);
    got = 99;
    CHECK(ENext(en, 1, props, &got) == S_FALSE && got == 0);
    CHECK(ESkip(en, 1) == S_FALSE);
    CHECK(EReset(en) == S_OK);
    CHECK(EClone(en, &en2) == S_OK && en2 != en);
    got = 0;
    CHECK(ENext(en2, 1, props, &got) == S_OK && got == 1 && !strcasecmp(props[0].pszName, "Content-Type"));
    CoTaskMemFree(props[0].pszName);
    BRelease(en2);
    ESkip(en, 3);
    CHECK(EClone(en, &en2) == S_OK);                      /* a clone keeps the position */
    got = 0;
    CHECK(ENext(en2, 1, props, &got) == S_OK && got == 1 && !strcasecmp(props[0].pszName, "X-A"));
    CoTaskMemFree(props[0].pszName);
    BRelease(en2);
    BRelease(en);
    CHECK(BEnumProps(body, 1, &en) == S_OK);               /* EPF_NONAME */
    got = 0;
    CHECK(ENext(en, 1, props, &got) == S_OK && got == 1 && props[0].pszName == NULL && props[0].dwPropId != 0);
    BRelease(en);
    CHECK(BEnumProps(body, 0, NULL) == E_INVALIDARG);

    /* display name */
    CHECK(GetDisplayName(body, &name) == MIME_E_NOT_FOUND);
    BRelease(body);
    body = body_from("Content-Type: application/pdf; name=\"typename.pdf\"\r\nContent-Disposition: attachment; filename=\"disp.pdf\"\r\n\r\n");
    CHECK(GetDisplayName(body, &name) == S_OK && !strcmp(name, "disp.pdf"));
    CoTaskMemFree(name);
    CHECK(SetDisplayName(body, "chosen.pdf") == S_OK);
    CHECK(GetDisplayName(body, &name) == S_OK && !strcmp(name, "chosen.pdf"));
    CoTaskMemFree(name);
    CHECK(SetDisplayName(body, NULL) == E_INVALIDARG);
    DeleteProp(body, "Content-Disposition");
    BRelease(body);
    body = body_from("Content-Type: application/pdf; name=\"typename.pdf\"\r\n\r\n");
    CHECK(GetDisplayName(body, &name) == S_OK && !strcmp(name, "typename.pdf"));
    CoTaskMemFree(name);

    /* IsType of loose bodies */
    CHECK(IsType(body, IBT_ATTACHMENT) == S_OK);
    CHECK(IsType(body, IBT_EMPTY) == S_OK);
    CHECK(IsType(body, IBT_CSETTAGGED) == S_FALSE);
    CHECK(IsType(body, IBT_SECURE) == S_FALSE);
    BRelease(body);
    body = body_from("Content-Type: text/plain; charset=us-ascii\r\n\r\n");
    CHECK(IsType(body, IBT_ATTACHMENT) == S_FALSE);
    CHECK(IsType(body, IBT_CSETTAGGED) == S_OK);
    BRelease(body);
    body = body_from("Content-Type: text/plain\r\nContent-Disposition: attachment; filename=\"n.txt\"\r\n\r\n");
    CHECK(IsType(body, IBT_ATTACHMENT) == S_OK);
    BRelease(body);
    body = body_from("Content-Type: multipart/signed; protocol=\"application/pkcs7-signature\"\r\n\r\n");
    CHECK(IsType(body, IBT_SECURE) == S_OK);
    CHECK(IsType(body, IBT_ATTACHMENT) == S_FALSE);
    BRelease(body);

    /* CopyTo: properties, data and the encoding */
    body = body_from("Content-Type: image/png; name=\"p.png\"\r\nContent-Transfer-Encoding: base64\r\nX-Tag: t\r\n\r\n");
    data = stream_of("aGVsbG8=");
    SetData(body, IET_BASE64, "image", "png", &IID_IStream_, data);
    data->lpVtbl->Release(data);
    dest = body_from("Subject: gone\r\nX-Stay: stay\r\n\r\n");
    CHECK(CopyTo(body, dest) == S_OK);
    CHECK(has(dest, "X-Tag", "t") && has(dest, "Content-Type", "image/png") && has(dest, "X-Stay", "stay") && missing(dest, "Subject") == 0);
    CHECK(GetData(dest, IET_BASE64, &data) == S_OK);
    {
        char buf[64]; ULONG n = 0;
        data->lpVtbl->Read(data, buf, sizeof(buf) - 1, &n);
        buf[n] = 0;
        CHECK(!strcmp(buf, "aGVsbG8="));
        data->lpVtbl->Release(data);
    }
    CHECK(saved_has(dest, "name=p.png"));
    CHECK(CopyTo(body, NULL) == E_INVALIDARG);
    CHECK(CopyTo(body, body) == S_OK);
    BRelease(dest);
    BRelease(body);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
