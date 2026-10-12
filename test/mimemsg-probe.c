/* inetcomm batch (patches/sg/2047), run by test/mimemsg-gate.sh. Families:
 * saving a message (the loaded text while clean, rebuilt from the bodies when
 * not), size, source, flags, body saving and offsets, message and body
 * properties, InitNew, and the IMimeAllocator frees. Interfaces are called
 * through their vtable slots.
 *
 *   mimemsg-probe.exe */
#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define MIME_E_NOT_FOUND ((HRESULT)0x800cce05)
enum { IMF_ATTACHMENTS = 1, IMF_MULTIPART = 2, IMF_SUBMULTIPART = 4, IMF_MIME = 8, IMF_HTML = 0x10, IMF_PLAIN = 0x20,
       IMF_MHTML = 0x400, IMF_TEXT = 0x1000, IMF_CSETTAGGED = 0x2000 };
typedef HANDLE HBODY;
typedef struct { DWORD cbBoundaryStart, cbHeaderStart, cbBodyStart, cbBodyEnd; } BODYOFFSETS;

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

#define SLOT(obj, n) ((*(void ***)(obj))[n])
#define MRelease(o) ((ULONG (WINAPI *)(void *))SLOT(o, 2))(o)
#define GetClassID(o, c) ((HRESULT (WINAPI *)(void *, CLSID *))SLOT(o, 3))(o, c)
#define IsDirty(o) ((HRESULT (WINAPI *)(void *))SLOT(o, 4))(o)
#define Load(o, s) ((HRESULT (WINAPI *)(void *, IStream *))SLOT(o, 5))(o, s)
#define Save(o, s, c) ((HRESULT (WINAPI *)(void *, IStream *, BOOL))SLOT(o, 6))(o, s, c)
#define GetSizeMax(o, p) ((HRESULT (WINAPI *)(void *, ULARGE_INTEGER *))SLOT(o, 7))(o, p)
#define InitNew(o) ((HRESULT (WINAPI *)(void *))SLOT(o, 8))(o)
#define GetMessageSource(o, p, f) ((HRESULT (WINAPI *)(void *, IStream **, DWORD))SLOT(o, 9))(o, p, f)
#define GetMessageSize(o, p, f) ((HRESULT (WINAPI *)(void *, ULONG *, DWORD))SLOT(o, 10))(o, p, f)
#define GetFlags(o, p) ((HRESULT (WINAPI *)(void *, DWORD *))SLOT(o, 13))(o, p)
#define SaveBody(o, h, f, s) ((HRESULT (WINAPI *)(void *, HBODY, DWORD, IStream *))SLOT(o, 17))(o, h, f, s)
#define GetBody(o, l, h, p) ((HRESULT (WINAPI *)(void *, int, HBODY, HBODY *))SLOT(o, 19))(o, l, h, p)
#define CountBodies(o, h, r, p) ((HRESULT (WINAPI *)(void *, HBODY, boolean, ULONG *))SLOT(o, 22))(o, h, r, p)
#define GetBodyOffsets(o, h, p) ((HRESULT (WINAPI *)(void *, HBODY, BODYOFFSETS *))SLOT(o, 27))(o, h, p)
#define QueryBodyProp(o, h, n, c, s, cs) ((HRESULT (WINAPI *)(void *, HBODY, const char *, const char *, boolean, boolean))SLOT(o, 32))(o, h, n, c, s, cs)
#define GetBodyProp(o, h, n, f, v) ((HRESULT (WINAPI *)(void *, HBODY, const char *, DWORD, PROPVARIANT *))SLOT(o, 33))(o, h, n, f, v)
#define SetBodyProp(o, h, n, f, v) ((HRESULT (WINAPI *)(void *, HBODY, const char *, DWORD, const PROPVARIANT *))SLOT(o, 34))(o, h, n, f, v)
#define DeleteBodyProp(o, h, n) ((HRESULT (WINAPI *)(void *, HBODY, const char *))SLOT(o, 35))(o, h, n)
#define MGetProp(o, n, f, v) ((HRESULT (WINAPI *)(void *, const char *, DWORD, PROPVARIANT *))SLOT(o, 39))(o, n, f, v)
#define MSetProp(o, n, f, v) ((HRESULT (WINAPI *)(void *, const char *, DWORD, const PROPVARIANT *))SLOT(o, 40))(o, n, f, v)
#define MDeleteProp(o, n) ((HRESULT (WINAPI *)(void *, const char *))SLOT(o, 41))(o, n)
#define MQueryProp(o, n, c, s, cs) ((HRESULT (WINAPI *)(void *, const char *, const char *, boolean, boolean))SLOT(o, 42))(o, n, c, s, cs)
/* IMimeAllocator */
#define AAlloc(o, n) ((void *(WINAPI *)(void *, SIZE_T))SLOT(o, 3))(o, n)
#define AGetSize(o, p) ((SIZE_T (WINAPI *)(void *, void *))SLOT(o, 6))(o, p)
#define ADidAlloc(o, p) ((int (WINAPI *)(void *, void *))SLOT(o, 7))(o, p)
#define AFreeAddressList(o, p) ((HRESULT (WINAPI *)(void *, void *))SLOT(o, 10))(o, p)
#define AFreeAddressProps(o, p) ((HRESULT (WINAPI *)(void *, void *))SLOT(o, 11))(o, p)
#define AReleaseObjects(o, n, p, f) ((HRESULT (WINAPI *)(void *, ULONG, IUnknown **, boolean))SLOT(o, 12))(o, n, p, f)
#define AFreeEnumHeaderRowArray(o, n, p, f) ((HRESULT (WINAPI *)(void *, ULONG, void *, boolean))SLOT(o, 13))(o, n, p, f)
#define AFreeEnumPropertyArray(o, n, p, f) ((HRESULT (WINAPI *)(void *, ULONG, void *, boolean))SLOT(o, 14))(o, n, p, f)
#define AFreeThumbprint(o, p) ((HRESULT (WINAPI *)(void *, void *))SLOT(o, 15))(o, p)
#define APropVariantClear(o, p) ((HRESULT (WINAPI *)(void *, PROPVARIANT *))SLOT(o, 16))(o, p)

static const CLSID CLSID_IMimeMessage_ = { 0xfd853ce3, 0x7f86, 0x11d0, { 0x82, 0x52, 0x00, 0xc0, 0x4f, 0xd8, 0x5a, 0xb4 } };

static const char source[] =
    "MIME-Version: 1.0\r\n"
    "Content-Type: multipart/mixed;\r\n"
    " boundary=\"------------1.5.0.6\"\r\n"
    "From: Huw Davies <huw@codeweavers.com>\r\n"
    "To: wine-patches <wine-patches@winehq.org>\r\n"
    "\r\n"
    "This is a multi-part message in MIME format.\r\n"
    "--------------1.5.0.6\r\n"
    "Content-Type: text/plain; format=fixed; charset=UTF-8\r\n"
    "Content-Transfer-Encoding: 8bit\r\n"
    "\r\n"
    "Stuff\r\n"
    "--------------1.5.0.6\r\n"
    "Content-Type: text/plain; charset=\"us-ascii\"\r\n"
    "Content-Transfer-Encoding: 7bit\r\n"
    "\r\n"
    "More stuff\r\n"
    "--------------1.5.0.6--\r\n";

static const char mhtml[] =
    "MIME-Version: 1.0\r\n"
    "Content-Type: multipart/related; type:=\"text/html\"; boundary=\"----=_NextPart_000_00\"\r\n"
    "\r\n"
    "------=_NextPart_000_00\r\n"
    "Content-Type: text/html; charset=\"Windows-1252\"\r\n"
    "Content-Transfer-Encoding: quoted-printable\r\n"
    "\r\n"
    "<HTML></HTML>\r\n"
    "------=_NextPart_000_00\r\n"
    "Content-Type: Image/Jpeg\r\n"
    "Content-Transfer-Encoding: base64\r\n"
    "\r\n"
    "VGVzdA==\r\n"
    "------=_NextPart_000_00--";

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

static char *read_all(IStream *s)
{
    static char bufs[4][4096];
    static int which;
    char *buf = bufs[which++ & 3];
    ULONG n = 0;
    LARGE_INTEGER zero = { 0 };
    s->lpVtbl->Seek(s, zero, STREAM_SEEK_SET, NULL);
    s->lpVtbl->Read(s, buf, 4095, &n);
    buf[n] = 0;
    return buf;
}

static char *saved(void *msg, BOOL clear, HRESULT *hr)
{
    IStream *s = stream_of("");
    char *text;
    *hr = Save(msg, s, clear);
    text = read_all(s);
    s->lpVtbl->Release(s);
    return text;
}

static HRESULT (WINAPI *create_message)(IUnknown *, void **);

static void *message_from(const char *text)
{
    void *msg = NULL;
    IStream *in = stream_of(text);
    if (FAILED(create_message(NULL, &msg)) || FAILED(Load(msg, in))) { printf("FAIL  could not make a message\n"); failures++; }
    in->lpVtbl->Release(in);
    return msg;
}

static int prop_is(void *msg, HBODY body, const char *name, const char *want)
{
    PROPVARIANT pv;
    int ok;
    PropVariantInit(&pv);
    pv.vt = VT_LPSTR;
    ok = (body ? GetBodyProp(msg, body, name, 0, &pv) : MGetProp(msg, name, 0, &pv)) == S_OK && pv.vt == VT_LPSTR && !strcmp(pv.pszVal, want);
    PropVariantClear(&pv);
    return ok;
}

typedef struct { IUnknownVtbl *lpVtbl; LONG refs; } Mock;
static HRESULT WINAPI mock_qi(IUnknown *i, REFIID r, void **p) { *p = NULL; return E_NOINTERFACE; }
static ULONG WINAPI mock_addref(IUnknown *i) { return 2; }
static ULONG WINAPI mock_release(IUnknown *i) { return ++((Mock *)i)->refs; }
static IUnknownVtbl mock_vtbl = { mock_qi, mock_addref, mock_release };

int main(void)
{
    HMODULE lib;
    HRESULT (WINAPI *get_alloc)(void **);
    void *msg, *msg2, *alloc;
    IStream *stream;
    ULARGE_INTEGER max;
    ULONG size, count;
    DWORD flags;
    CLSID clsid;
    HBODY part1, part2, root;
    BODYOFFSETS off;
    PROPVARIANT pv;
    HRESULT hr;
    char *text, saved_text[4096], boundary_line[64];

    CoInitialize(NULL);
    lib = LoadLibraryA("inetcomm.dll");
    create_message = (void *)GetProcAddress(lib, "MimeOleCreateMessage");
    get_alloc = (void *)GetProcAddress(lib, "MimeOleGetAllocator");
    if (!create_message || !get_alloc) { check(0, "exports"); printf("RESULT: FAIL\n"); return 1; }

    msg = message_from(source);
    CHECK(GetClassID(msg, &clsid) == S_OK && IsEqualGUID(&clsid, &CLSID_IMimeMessage_));
    CHECK(GetClassID(msg, NULL) == E_INVALIDARG);
    CHECK(IsDirty(msg) == S_FALSE);

    /* a clean message saves as it was loaded */
    text = saved(msg, FALSE, &hr);
    CHECK(hr == S_OK && !strcmp(text, source));
    max.QuadPart = 0;
    CHECK(GetSizeMax(msg, &max) == S_OK && max.QuadPart == strlen(source));
    CHECK(GetSizeMax(msg, NULL) == E_INVALIDARG);
    size = 0;
    CHECK(GetMessageSize(msg, &size, 0) == S_OK && size == strlen(source));
    CHECK(GetMessageSource(msg, &stream, 0) == S_OK && !strcmp(read_all(stream), source));
    stream->lpVtbl->Release(stream);
    CHECK(Save(msg, NULL, FALSE) == E_INVALIDARG);
    CHECK(GetMessageSource(msg, NULL, 0) == E_INVALIDARG);

    flags = 0;
    CHECK(GetFlags(msg, &flags) == S_OK);
    CHECK(flags == (IMF_MIME | IMF_MULTIPART | IMF_PLAIN | IMF_TEXT | IMF_ATTACHMENTS | IMF_CSETTAGGED));
    if (flags != (IMF_MIME | IMF_MULTIPART | IMF_PLAIN | IMF_TEXT | IMF_ATTACHMENTS | IMF_CSETTAGGED)) printf("   flags %lx\n", flags);
    CHECK(GetFlags(msg, NULL) == E_INVALIDARG);

    /* the parts */
    root = NULL;
    GetBody(msg, 0 /* IBL_ROOT */, NULL, &root);
    CHECK(GetBody(msg, 2 /* IBL_FIRST */, root, &part1) == S_OK);
    CHECK(GetBody(msg, 4 /* IBL_NEXT */, part1, &part2) == S_OK);
    CHECK(SaveBody(msg, NULL, 0, NULL) == E_INVALIDARG);
    stream = stream_of("");
    CHECK(SaveBody(msg, part1, 0, stream) == S_OK);
    CHECK(!strcmp(read_all(stream), "Content-Type: text/plain; format=fixed; charset=UTF-8\r\nContent-Transfer-Encoding: 8bit\r\n\r\nStuff"));
    stream->lpVtbl->Release(stream);
    memset(&off, 0, sizeof(off));
    CHECK(GetBodyOffsets(msg, part1, &off) == S_OK && off.cbHeaderStart < off.cbBodyStart && off.cbBodyStart < off.cbBodyEnd);
    CHECK(GetBodyOffsets(msg, part1, NULL) == E_INVALIDARG);

    /* properties of the message and of a body */
    CHECK(prop_is(msg, 0, "From", "Huw Davies <huw@codeweavers.com>"));
    PropVariantInit(&pv);
    pv.vt = VT_LPSTR; pv.pszVal = (char *)"changed";
    CHECK(MSetProp(msg, "Subject", 0, &pv) == S_OK && prop_is(msg, 0, "Subject", "changed"));
    CHECK(IsDirty(msg) == S_OK);
    CHECK(MQueryProp(msg, "Subject", "CHANG", TRUE, FALSE) == S_OK);
    CHECK(MQueryProp(msg, "Subject", "CHANG", TRUE, TRUE) == S_FALSE);
    CHECK(MQueryProp(msg, "X-Nothing", "x", TRUE, TRUE) == MIME_E_NOT_FOUND);
    pv.pszVal = (char *)"p";
    CHECK(SetBodyProp(msg, part2, "X-Part", 0, &pv) == S_OK && prop_is(msg, part2, "X-Part", "p"));
    CHECK(QueryBodyProp(msg, part2, "X-Part", "P", FALSE, FALSE) == S_OK);
    CHECK(QueryBodyProp(msg, part2, "X-Part", "P", FALSE, TRUE) == S_FALSE);
    CHECK(prop_is(msg, part1, "X-Part", "p") == 0);

    /* a dirty message is built from its bodies, and loads back the same */
    text = saved(msg, FALSE, &hr);
    CHECK(hr == S_OK && strstr(text, "Subject: changed\r\n") && strstr(text, "X-Part: p\r\n"));
    CHECK(strstr(text, "--------------1.5.0.6\r\nContent-Type: text/plain; format=fixed; charset=UTF-8\r\n") != NULL);
    CHECK(!strcmp(text + strlen(text) - strlen("--------------1.5.0.6--\r\n"), "--------------1.5.0.6--\r\n"));
    strcpy(saved_text, text);
    msg2 = message_from(saved_text);
    count = 0;
    CHECK(CountBodies(msg2, (HBODY)-1, TRUE, &count) == S_OK && count == 3);
    root = NULL;
    GetBody(msg2, 0, NULL, &root);
    GetBody(msg2, 2, root, &part1);
    GetBody(msg2, 4, part1, &part2);
    CHECK(prop_is(msg2, 0, "Subject", "changed") && prop_is(msg2, part2, "X-Part", "p"));
    CHECK(prop_is(msg2, part1, "Content-Transfer-Encoding", "8bit"));
    stream = stream_of("");
    SaveBody(msg2, part2, 0, stream);
    CHECK(strstr(read_all(stream), "\r\n\r\nMore stuff") != NULL);
    stream->lpVtbl->Release(stream);
    CHECK(IsDirty(msg2) == S_FALSE);
    MRelease(msg2);

    CHECK(GetMessageSource(msg, &stream, 0) == S_OK && strstr(read_all(stream), "Subject: changed") != NULL);
    stream->lpVtbl->Release(stream);
    size = 0;
    GetMessageSize(msg, &size, 0);
    CHECK(size == strlen(saved_text));
    CHECK(MDeleteProp(msg, "Subject") == S_OK && prop_is(msg, 0, "Subject", "changed") == 0);
    CHECK(MDeleteProp(msg, "Subject") == MIME_E_NOT_FOUND);
    CHECK(DeleteBodyProp(msg, part2, "X-Part") == S_OK);
    text = saved(msg, TRUE, &hr);
    CHECK(hr == S_OK && strstr(text, "Subject") == NULL);
    CHECK(IsDirty(msg) == S_FALSE);                     /* Save with clear */
    MRelease(msg);

    /* an HTML message with an attachment */
    msg = message_from(mhtml);
    flags = 0;
    CHECK(GetFlags(msg, &flags) == S_OK);
    CHECK(flags == (IMF_MIME | IMF_MULTIPART | IMF_MHTML | IMF_HTML | IMF_TEXT | IMF_ATTACHMENTS | IMF_CSETTAGGED));
    if (flags != (IMF_MIME | IMF_MULTIPART | IMF_MHTML | IMF_HTML | IMF_TEXT | IMF_ATTACHMENTS | IMF_CSETTAGGED)) printf("   flags %lx\n", flags);
    MRelease(msg);
    msg = message_from("Subject: plain\r\n\r\nJust text\r\n");
    flags = 0;
    CHECK(GetFlags(msg, &flags) == S_OK && flags == (IMF_PLAIN | IMF_TEXT));
    MRelease(msg);

    /* an empty message */
    msg = message_from(source);
    CHECK(InitNew(msg) == S_OK);
    count = 0;
    CHECK(CountBodies(msg, (HBODY)-1, TRUE, &count) == S_OK && count == 1);
    CHECK(IsDirty(msg) == S_FALSE);
    pv.vt = VT_LPSTR; pv.pszVal = (char *)"x";
    MSetProp(msg, "Subject", 0, &pv);
    text = saved(msg, FALSE, &hr);
    CHECK(hr == S_OK && !strcmp(text, "Subject: x\r\n\r\n"));
    size = 0;
    CHECK(GetMessageSize(msg, &size, 0) == S_OK && size == 14);
    CHECK(GetMessageSource(msg, &stream, 0) == S_OK && !strcmp(read_all(stream), "Subject: x\r\n\r\n"));
    stream->lpVtbl->Release(stream);
    MRelease(msg);

    /* the allocator */
    CHECK(get_alloc(&alloc) == S_OK);
    {
        void *p = AAlloc(alloc, 40);
        Mock mocks[3] = { { &mock_vtbl, 0 }, { &mock_vtbl, 0 }, { &mock_vtbl, 0 } };
        IUnknown **list = AAlloc(alloc, 4 * sizeof(*list));
        struct { DWORD dwProps; void *hAddress; int ietFriendly; void *hCharset; DWORD dwAdrType; char *pszFriendly; WCHAR *pwszReserved; char *pszEmail;
                 int certstate; BLOB tbSigning; BLOB tbEncryption; DWORD dwCookie, r1, r2; } addr = { 0 };
        struct { ULONG cAdrs; void *prgAdr; } alist;
        BLOB thumb;
        PROPVARIANT clr;

        CHECK(AGetSize(alloc, p) == 40);
        CHECK(ADidAlloc(alloc, p) == 1);
        CoTaskMemFree(p);

        list[0] = (IUnknown *)&mocks[0]; list[1] = NULL; list[2] = (IUnknown *)&mocks[1]; list[3] = (IUnknown *)&mocks[2];
        CHECK(AReleaseObjects(alloc, 4, list, TRUE) == S_OK);
        CHECK(mocks[0].refs == 1 && mocks[1].refs == 1 && mocks[2].refs == 1);

        addr.dwProps = 0x8 | 0x20;                     /* IAP_FRIENDLY | IAP_EMAIL */
        addr.pszFriendly = AAlloc(alloc, 8); strcpy(addr.pszFriendly, "name");
        addr.pszEmail = AAlloc(alloc, 8); strcpy(addr.pszEmail, "a@b");
        CHECK(AFreeAddressProps(alloc, &addr) == S_OK);
        CHECK(addr.pszFriendly == NULL && addr.pszEmail == NULL && addr.dwProps == 0);
        alist.cAdrs = 1;
        alist.prgAdr = AAlloc(alloc, sizeof(addr));
        memset(alist.prgAdr, 0, sizeof(addr));
        ((typeof(addr) *)alist.prgAdr)->dwProps = 0x20;
        ((typeof(addr) *)alist.prgAdr)->pszEmail = AAlloc(alloc, 8);
        CHECK(AFreeAddressList(alloc, &alist) == S_OK && alist.cAdrs == 0 && alist.prgAdr == NULL);

        thumb.cbSize = 4; thumb.pBlobData = AAlloc(alloc, 4);
        CHECK(AFreeThumbprint(alloc, &thumb) == S_OK && thumb.pBlobData == NULL && thumb.cbSize == 0);
        CHECK(AFreeThumbprint(alloc, NULL) == E_INVALIDARG);

        {
            struct { char *name; void *row; DWORD id; } *props = AAlloc(alloc, 2 * 24);
            struct { void *row; char *hdr; char *data; ULONG cch; DWORD_PTR r; } *rows = AAlloc(alloc, 2 * sizeof(*rows));
            props[0].name = AAlloc(alloc, 4); props[1].name = AAlloc(alloc, 4);
            rows[0].hdr = AAlloc(alloc, 4); rows[0].data = AAlloc(alloc, 4); rows[1].hdr = AAlloc(alloc, 4); rows[1].data = NULL;
            CHECK(AFreeEnumPropertyArray(alloc, 2, props, TRUE) == S_OK);
            CHECK(AFreeEnumHeaderRowArray(alloc, 2, rows, TRUE) == S_OK);
        }

        clr.vt = VT_LPSTR; clr.pszVal = CoTaskMemAlloc(4);
        CHECK(APropVariantClear(alloc, &clr) == S_OK && clr.vt == VT_EMPTY);
        CHECK(APropVariantClear(alloc, NULL) == E_INVALIDARG);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
