/* OLE 1.0 client/server libraries (patch 3576): registries, handle checks, vtable dispatch. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures, total;
#define CHECK(c, ...) do { total++; if (!(c)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

enum { OLE_OK, OLE_WAIT_FOR_RELEASE, OLE_BUSY, OLE_ERROR_PROTECT_ONLY, OLE_ERROR_MEMORY, OLE_ERROR_STREAM,
       OLE_ERROR_STATIC, OLE_ERROR_BLANK, OLE_ERROR_DRAW, OLE_ERROR_METAFILE, OLE_ERROR_ABORT, OLE_ERROR_CLIPBOARD,
       OLE_ERROR_FORMAT, OLE_ERROR_OBJECT, OLE_ERROR_OPTION, OLE_ERROR_PROTOCOL, OLE_ERROR_ADDRESS,
       OLE_ERROR_NOT_EQUAL, OLE_ERROR_HANDLE, OLE_ERROR_GENERIC, OLE_ERROR_CLASS, OLE_ERROR_SYNTAX,
       OLE_ERROR_DATATYPE, OLE_ERROR_PALETTE, OLE_ERROR_NOT_LINK, OLE_ERROR_NOT_EMPTY, OLE_ERROR_SIZE,
       OLE_ERROR_DRIVE, OLE_ERROR_NETWORK, OLE_ERROR_NAME, OLE_ERROR_TEMPLATE, OLE_ERROR_NEW, OLE_ERROR_EDIT,
       OLE_ERROR_OPEN, OLE_ERROR_NOT_OPEN, OLE_ERROR_LAUNCH, OLE_ERROR_COMM, OLE_ERROR_TERMINATE, OLE_ERROR_COMMAND, OLE_ERROR_SHOW, OLE_ERROR_DOVERB, OLE_ERROR_ADVISE_NATIVE, OLE_ERROR_ADVISE_PICT, OLE_ERROR_ADVISE_RENAME, OLE_ERROR_POKE_NATIVE, OLE_ERROR_REQUEST_NATIVE, OLE_ERROR_REQUEST_PICT, OLE_ERROR_SERVER_BLOCKED, OLE_ERROR_REGISTRATION, OLE_ERROR_ALREADY_REGISTERED };

typedef int (WINAPI *PF)();

/* server side */
struct svr_vtbl { void *fn[7]; };
static int released;
static int CALLBACK svr_release(void *s) { released++; return 0; }
struct svr { const struct svr_vtbl *vtbl; };

/* client object vtable with the entries the tests call (index = vtable slot) */
static int calls[64];
static int CALLBACK o_delete(void *o) { calls[10]++; return 0; }
static int CALLBACK o_equal(void *a, void *b) { calls[15]++; return OLE_ERROR_NOT_EQUAL; }
static int CALLBACK o_update(void *o) { calls[21]++; return OLE_OK; }
static int CALLBACK o_activate(void *o, UINT verb, BOOL show, BOOL focus, HWND w, const RECT *r)
{ calls[18]++; return verb == 3 && show && !focus && r ? OLE_OK : OLE_ERROR_GENERIC; }
static int CALLBACK o_qtype(void *o, LONG *t) { *t = 2; return OLE_OK; }
static int CALLBACK o_qbounds(void *o, RECT *r) { r->left = 1; r->top = 2; r->right = 3; r->bottom = 4; return OLE_OK; }
static UINT CALLBACK o_enum(void *o, UINT f) { return f == 0 ? CF_TEXT : 0; }
static void *CALLBACK o_proto(void *o, const char *p) { return strcmp(p, "StdFileEditing") ? NULL : o; }

int main(void)
{
    HMODULE svr = LoadLibraryA("olesvr32.dll"), cli = LoadLibraryA("olecli32.dll");
    PF RegSvr, RevSvr, Block, Unblock, RegDoc, RevDoc, RenDoc, Revert, Saved, RevObj, SvrVer;
    PF CRegDoc, CRevDoc, CRenDoc, CSaved, CRevert, EnumObj, Delete, Equal, Update, Activate, QType, QBounds, EnumFmt, QProto;
    PF QCreate, QLink, Create, CreateFile, CVer, LockSvr, Unlock;
    void *vt[40];
    struct { void *vtbl; } obj;
    LONG h, h2, hd, hd2, ch, ch2;
    BOOL blk;
    struct svr s1, s2;
    struct svr_vtbl sv = { { 0 } };
    int r;

    if (!svr || !cli) { printf("FAIL  cannot load the libraries\n"); return 1; }
    RegSvr = (PF)GetProcAddress(svr, "OleRegisterServer"); RevSvr = (PF)GetProcAddress(svr, "OleRevokeServer");
    Block = (PF)GetProcAddress(svr, "OleBlockServer"); Unblock = (PF)GetProcAddress(svr, "OleUnblockServer");
    RegDoc = (PF)GetProcAddress(svr, "OleRegisterServerDoc"); RevDoc = (PF)GetProcAddress(svr, "OleRevokeServerDoc");
    RenDoc = (PF)GetProcAddress(svr, "OleRenameServerDoc"); Revert = (PF)GetProcAddress(svr, "OleRevertServerDoc");
    Saved = (PF)GetProcAddress(svr, "OleSavedServerDoc"); RevObj = (PF)GetProcAddress(svr, "OleRevokeObject");
    SvrVer = (PF)GetProcAddress(svr, "OleQueryServerVersion");
    CRegDoc = (PF)GetProcAddress(cli, "OleRegisterClientDoc"); CRevDoc = (PF)GetProcAddress(cli, "OleRevokeClientDoc");
    CRenDoc = (PF)GetProcAddress(cli, "OleRenameClientDoc"); CSaved = (PF)GetProcAddress(cli, "OleSavedClientDoc");
    CRevert = (PF)GetProcAddress(cli, "OleRevertClientDoc"); EnumObj = (PF)GetProcAddress(cli, "OleEnumObjects");
    Delete = (PF)GetProcAddress(cli, "OleDelete"); Equal = (PF)GetProcAddress(cli, "OleEqual");
    Update = (PF)GetProcAddress(cli, "OleUpdate"); Activate = (PF)GetProcAddress(cli, "OleActivate");
    QType = (PF)GetProcAddress(cli, "OleQueryType"); QBounds = (PF)GetProcAddress(cli, "OleQueryBounds");
    EnumFmt = (PF)GetProcAddress(cli, "OleEnumFormats"); QProto = (PF)GetProcAddress(cli, "OleQueryProtocol");
    QCreate = (PF)GetProcAddress(cli, "OleQueryCreateFromClip"); QLink = (PF)GetProcAddress(cli, "OleQueryLinkFromClip");
    Create = (PF)GetProcAddress(cli, "OleCreate"); CreateFile = (PF)GetProcAddress(cli, "OleCreateFromFile");
    CVer = (PF)GetProcAddress(cli, "OleQueryClientVersion"); LockSvr = (PF)GetProcAddress(cli, "OleLockServer");
    Unlock = (PF)GetProcAddress(cli, "OleUnlockServer");
    if (!RegSvr || !EnumObj || !QProto) { printf("FAIL  missing exports\n"); return 1; }

    /* ---- olesvr32 ---- */
    sv.fn[5] = svr_release;
    s1.vtbl = &sv; s2.vtbl = &sv;
    CHECK(RegSvr(NULL, &s1, &h, NULL, 0) == OLE_ERROR_NAME, "null name");
    CHECK(RegSvr("", &s1, &h, NULL, 0) == OLE_ERROR_NAME, "empty name");
    CHECK(RegSvr("T1", NULL, &h, NULL, 0) == OLE_ERROR_ADDRESS, "null server");
    CHECK(RegSvr("T1", &s1, NULL, NULL, 0) == OLE_ERROR_ADDRESS, "null handle");
    CHECK(RegSvr("T1", &s1, &h, NULL, 7) == OLE_ERROR_OPTION, "bad use");
    CHECK(RegSvr("T1", &s1, &h, NULL, 0) == OLE_OK && h, "register");
    CHECK(RegSvr("t1", &s2, &h2, NULL, 0) == OLE_ERROR_ALREADY_REGISTERED, "duplicate name");
    CHECK(RegSvr("T2", &s1, &h2, NULL, 0) == OLE_ERROR_ALREADY_REGISTERED, "duplicate server");
    CHECK(RegSvr("T2", &s2, &h2, NULL, 1) == OLE_OK && h2 && h2 != h, "second server");
    CHECK(Block(0) == OLE_ERROR_HANDLE, "block 0");
    CHECK(Block(h) == OLE_OK, "block");
    blk = TRUE;
    CHECK(Unblock(h, (BOOL *)&blk) == OLE_OK && !blk, "unblock %d", blk);
    CHECK(Unblock(12345, (BOOL *)&blk) == OLE_ERROR_HANDLE, "unblock bad");
    CHECK(RegDoc(h, "doc1", (void *)&s1, &hd) == OLE_OK && hd, "register doc");
    CHECK(RegDoc(12345, "doc1", (void *)&s1, &hd2) == OLE_ERROR_HANDLE, "doc on bad server");
    CHECK(RegDoc(h, "", (void *)&s1, &hd2) == OLE_ERROR_NAME, "doc empty name");
    CHECK(RenDoc(hd, "doc2") == OLE_OK, "rename doc");
    CHECK(RenDoc(hd, NULL) == OLE_ERROR_NAME, "rename null");
    CHECK(RenDoc(999, "x") == OLE_ERROR_HANDLE, "rename bad");
    CHECK(Revert(hd) == OLE_OK && Saved(hd) == OLE_OK, "revert/saved");
    CHECK(Revert(999) == OLE_ERROR_HANDLE && Saved(0) == OLE_ERROR_HANDLE, "revert/saved bad");
    CHECK(RevObj(NULL) == OLE_ERROR_ADDRESS, "revoke object null");
    released = 0;
    CHECK(RevSvr(h) == OLE_WAIT_FOR_RELEASE, "revoke with open doc");
    CHECK(released == 0, "released too early");
    CHECK(RegDoc(h, "late", (void *)&s1, &hd2) != OLE_OK, "doc on revoking server");
    CHECK(RevDoc(hd) == OLE_OK, "revoke doc");
    CHECK(released == 1, "release callback after the last doc (%d)", released);
    CHECK(RevDoc(hd) == OLE_ERROR_HANDLE, "revoke doc twice");
    CHECK(RevSvr(h) == OLE_ERROR_HANDLE, "revoke server twice");
    CHECK(RevSvr(h2) == OLE_OK && released == 2, "revoke empty server");
    CHECK(RegSvr("T1", &s1, &h, NULL, 0) == OLE_OK, "name free again");
    RevSvr(h);
    r = SvrVer(); CHECK(r != 0, "server version");

    /* ---- olecli32 ---- */
    CHECK(CRegDoc("cls", NULL, 0, &ch) == OLE_ERROR_NAME, "client doc null name");
    CHECK(CRegDoc("cls", "d", 0, NULL) == OLE_ERROR_ADDRESS, "client doc null handle");
    CHECK(CRegDoc("cls", "d", 0, &ch) == OLE_OK && ch, "client doc");
    CHECK(CRegDoc("cls", "d2", 0, &ch2) == OLE_OK && ch2 != ch, "client doc 2");
    CHECK(CRenDoc(ch, "e") == OLE_OK && CSaved(ch) == OLE_OK && CRevert(ch) == OLE_OK, "client doc ops");
    CHECK(CRenDoc(77, "e") == OLE_ERROR_HANDLE && CSaved(77) == OLE_ERROR_HANDLE, "client doc bad handle");
    { void *o = (void *)1; CHECK(EnumObj(ch, &o) == OLE_OK && !o, "enum objects"); }
    CHECK(CRevDoc(ch) == OLE_OK && CRevDoc(ch) == OLE_ERROR_HANDLE, "revoke client doc");
    CHECK(CRevDoc(ch2) == OLE_OK, "revoke client doc 2");

    memset(vt, 0, sizeof(vt));
    vt[0] = o_proto; vt[8] = o_enum; vt[10] = o_delete; vt[15] = o_equal; vt[18] = o_activate; vt[21] = o_update;
    vt[28] = o_qtype; vt[29] = o_qbounds;
    obj.vtbl = vt;
    {
        RECT rc = { 0 }, out;
        LONG t = 0;
        CHECK(Delete(NULL) == OLE_ERROR_OBJECT, "delete null");
        CHECK(Delete(&obj) == OLE_OK && calls[10] == 1, "delete dispatch");
        CHECK(Equal(&obj, &obj) == OLE_ERROR_NOT_EQUAL && calls[15] == 1, "equal dispatch");
        CHECK(Equal(&obj, NULL) == OLE_ERROR_OBJECT, "equal null");
        CHECK(Update(&obj) == OLE_OK, "update");
        CHECK(Activate(&obj, 3, TRUE, FALSE, NULL, &rc) == OLE_OK, "activate args");
        CHECK(QType(&obj, &t) == OLE_OK && t == 2, "query type %ld", t);
        CHECK(QType(&obj, NULL) == OLE_ERROR_ADDRESS, "query type null out");
        CHECK(QBounds(&obj, &out) == OLE_OK && out.right == 3 && out.bottom == 4, "query bounds");
        CHECK(((UINT (WINAPI *)(void *, UINT))EnumFmt)(&obj, 0) == CF_TEXT, "enum formats");
        CHECK(((UINT (WINAPI *)(void *, UINT))EnumFmt)(NULL, 0) == 0, "enum formats null");
        CHECK(((void *(WINAPI *)(void *, const char *))QProto)(&obj, "StdFileEditing") == &obj, "protocol yes");
        CHECK(((void *(WINAPI *)(void *, const char *))QProto)(&obj, "Static") == NULL, "protocol no");
        CHECK(Update(&(struct { void *v; }){ NULL }) == OLE_ERROR_OBJECT, "no vtable");
    }
    CHECK(QCreate("StdFileEditing", 0, 0) == OLE_ERROR_FORMAT, "empty clipboard create");
    CHECK(QLink("StdFileEditing", 0, 0) == OLE_ERROR_FORMAT, "empty clipboard link");
    CHECK(QCreate(NULL, 0, 0) == OLE_ERROR_PROTOCOL, "null protocol");
    CHECK(QCreate("StdFileEditing", 9, 0) == OLE_ERROR_OPTION, "bad render");
    {
        void *o = (void *)1;
        char cl[] = "Wine.NoSuchOle1Class";
        CHECK(((int (WINAPI *)(const char *, void *, const char *, LONG, const char *, void **, int, UINT))Create)
              ("StdFileEditing", &obj, cl, 0, "obj1", &o, 0, 0) == OLE_ERROR_CLASS && !o, "unknown class");
        CHECK(((int (WINAPI *)(const char *, void *, const char *, LONG, const char *, void **, int, UINT))Create)
              ("Bogus", &obj, cl, 0, "obj1", &o, 0, 0) == OLE_ERROR_PROTOCOL, "bad protocol");
        CHECK(((int (WINAPI *)(const char *, void *, const char *, const char *, LONG, const char *, void **, int, UINT))CreateFile)
              ("StdFileEditing", &obj, NULL, "C:\\no\\such\\file", 0, "obj1", &o, 0, 0) == OLE_ERROR_OPEN, "missing file");
    }
    CHECK(CVer() != 0, "client version");
    CHECK(Unlock(5) == OLE_ERROR_HANDLE, "unlock bad");
    (void)LockSvr; (void)Revert;

    printf("%d checks, %d failures\n", total, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
