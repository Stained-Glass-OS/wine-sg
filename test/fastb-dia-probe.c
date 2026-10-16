/* diasymreader: SymWriter writes a PDB that dbghelp and an independent MSF reader accept */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <dbghelp.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef void *OBJ;
#define SLOT(o, n) ((*(void ***)(o))[n])
#define CALL(o, n, ret, ...) ((ret (WINAPI *)())SLOT(o, n))(o, ##__VA_ARGS__)

static const GUID CLSID_Writer = {0x0ae2deb0,0xf901,0x478b,{0xbb,0x9f,0x88,0x1e,0xe8,0x06,0x67,0x88}};
static const GUID IID_Writer5 = {0xdcf7780d,0xbde9,0x45df,{0xac,0xfe,0x21,0x73,0x1a,0x32,0x00,0x0c}};
static const GUID IID_PdbWriter = {0x98ecee1e,0x752d,0x11d3,{0x8d,0x56,0x00,0xc0,0x4f,0x68,0x0b,0x2b}};
static const GUID lang_cs = {0x3f5162f8,0x07c6,0x11d3,{0x90,0x53,0x00,0xc0,0x4f,0xa3,0x02,0xa1}};
static const GUID sha1_alg = {0xff1816ec,0xaa5e,0x4d10,{0x87,0xf7,0x6f,0x49,0x63,0x83,0x34,0x60}};

enum { W_DefineDocument = 3, W_SetUserEntryPoint, W_OpenMethod, W_CloseMethod, W_OpenScope, W_CloseScope,
       W_SetScopeRange, W_DefineLocalVariable, W_DefineParameter, W_DefineField, W_DefineGlobalVariable,
       W_Close, W_SetSymAttributes, W_OpenNamespace, W_CloseNamespace, W_UsingNamespace,
       W_SetMethodSourceRange, W_Initialize, W_GetDebugInfo, W_DefineSequencePoints, W_RemapToken,
       W_Initialize2, W_DefineConstant, W_Abort, W_DefineLocalVariable2, W_DefineGlobalVariable2,
       W_DefineConstant2, W_OpenMethod2, W_Commit };

static void *find_mem(const BYTE *h, DWORD hl, const char *n, DWORD nl)
{
    DWORD i;
    for (i = 0; i + nl <= hl; i++) if (!memcmp(h + i, n, nl)) return (void *)(h + i);
    return NULL;
}

static BYTE *rd_file(const WCHAR *path, DWORD *size)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    BYTE *p;
    DWORD got;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    *size = GetFileSize(h, NULL);
    p = malloc(*size);
    ReadFile(h, p, *size, &got, NULL);
    CloseHandle(h);
    return p;
}

struct msf { const BYTE *data; DWORD size, bs, nstreams; DWORD *sizes; DWORD **blocks; };

static BOOL msf_open(struct msf *m, const BYTE *d, DWORD size)
{
    DWORD dir_bytes, nb, i, j, *dirblocks, dirpos = 0, mapblk;
    BYTE *dir;

    memset(m, 0, sizeof(*m));
    if (size < 64 || memcmp(d, "Microsoft C/C++ MSF 7.00\r\n\x1a" "DS\0\0\0", 32)) return FALSE;
    m->data = d; m->size = size;
    m->bs = ((DWORD *)(d + 32))[0];
    dir_bytes = ((DWORD *)(d + 32))[3];
    mapblk = ((DWORD *)(d + 32))[5];
    if (m->bs != 4096 || ((DWORD *)(d + 32))[2] * m->bs != size) return FALSE;
    nb = (dir_bytes + m->bs - 1) / m->bs;
    dirblocks = (DWORD *)(d + mapblk * m->bs);
    dir = malloc(nb * m->bs);
    for (i = 0; i < nb; i++) memcpy(dir + i * m->bs, d + dirblocks[i] * m->bs, m->bs);
    m->nstreams = ((DWORD *)dir)[0];
    m->sizes = (DWORD *)dir + 1;
    dirpos = 1 + m->nstreams;
    m->blocks = calloc(m->nstreams, sizeof(DWORD *));
    for (i = 0; i < m->nstreams; i++)
    {
        DWORD n = (m->sizes[i] + m->bs - 1) / m->bs;
        m->blocks[i] = (DWORD *)dir + dirpos;
        dirpos += n;
        for (j = 0; j < n; j++) if (m->blocks[i][j] >= size / m->bs) return FALSE;
    }
    return TRUE;
}

static BYTE *msf_stream(struct msf *m, DWORD idx)
{
    DWORD n = (m->sizes[idx] + m->bs - 1) / m->bs, i;
    BYTE *p = calloc(n + 1, m->bs);
    for (i = 0; i < n; i++) memcpy(p + i * m->bs, m->data + m->blocks[idx][i] * m->bs, m->bs);
    return p;
}

/* find a named stream in the PDB info stream */
static int named_stream(const BYTE *info, DWORD size, const char *name)
{
    const BYTE *p = info + 28;
    DWORD cb = *(DWORD *)p, count, cap, words, i;
    const char *strs = (const char *)p + 4;
    const DWORD *q;
    const DWORD *present;

    p += 4 + cb;
    count = ((DWORD *)p)[0]; cap = ((DWORD *)p)[1];
    words = ((DWORD *)p)[2];
    present = (DWORD *)p + 3;
    q = present + words;
    q += 1 + *q;            /* deleted vector */
    (void)count;
    for (i = 0; i < cap; i++)
    {
        if (!(present[i / 32] & (1u << (i % 32)))) continue;
        if (!strcmp(strs + q[0], name)) return q[1];
        q += 2;
    }
    return -1;
}

static void check_pdb(const WCHAR *path, const GUID *guid, DWORD age, DWORD stamp)
{
    DWORD size, nd = 0;
    BYTE *file = rd_file(path, &size), *info, *dbi, *mod, *names, *src;
    struct msf m;
    int idx;
    DWORD modinfo_size, sym, c13, sym_off, found_lines = 0, found_proc = 0, found_chk = 0;

    CHECK(file != NULL, "pdb file exists");
    if (!file) return;
    CHECK(msf_open(&m, file, size), "valid MSF container");
    if (!m.nstreams) return;
    CHECK(m.nstreams >= 10, "streams %lu", m.nstreams);

    info = msf_stream(&m, 1);
    CHECK(((DWORD *)info)[0] == 20000404, "info version %lu", ((DWORD *)info)[0]);
    CHECK(((DWORD *)info)[1] == stamp, "info stamp");
    CHECK(((DWORD *)info)[2] == age, "info age");
    CHECK(!memcmp(info + 12, guid, 16), "info guid");
    idx = named_stream(info, m.sizes[1], "/names");
    CHECK(idx > 0 && idx < (int)m.nstreams, "/names stream index %d", idx);
    names = msf_stream(&m, idx);
    CHECK(((DWORD *)names)[0] == 0xeffeeffe && ((DWORD *)names)[1] == 1, "names header");
    CHECK(find_mem(names, m.sizes[idx], "a.cs", 5) != NULL, "names has a.cs");
    idx = named_stream(info, m.sizes[1], "/src/files/a.cs");
    CHECK(idx > 0 && idx < (int)m.nstreams, "source stream %d", idx);
    if (idx > 0) { src = msf_stream(&m, idx); CHECK(m.sizes[idx] == 10 && !memcmp(src, "class A{}\n", 10), "source content"); }
    CHECK(named_stream(info, m.sizes[1], "/nope") == -1, "missing named stream");

    dbi = msf_stream(&m, 3);
    CHECK(((int *)dbi)[0] == -1 && ((DWORD *)dbi)[1] == 19990903, "dbi header");
    CHECK(((DWORD *)dbi)[2] == age, "dbi age");
    modinfo_size = *(DWORD *)(dbi + 24);
    CHECK(modinfo_size >= 64, "module info size %lu", modinfo_size);
    sym = *(DWORD *)(dbi + 64 + 4 + 28 + 4);
    c13 = *(DWORD *)(dbi + 64 + 4 + 28 + 12);
    idx = *(WORD *)(dbi + 64 + 4 + 28 + 2);
    CHECK(idx > 0 && idx < (int)m.nstreams && m.sizes[idx] > sym, "module stream %d", idx);
    mod = msf_stream(&m, idx);
    CHECK(*(DWORD *)mod == 4, "module signature");
    CHECK(m.sizes[idx] == sym + c13 + 4, "module size %lu vs %lu+%lu", m.sizes[idx], sym, c13);

    /* symbols: two methods, each with an end record */
    for (sym_off = 4; sym_off < sym; )
    {
        WORD len = *(WORD *)(mod + sym_off), kind = *(WORD *)(mod + sym_off + 2);
        if (kind == 0x112a)
        {
            DWORD token = *(DWORD *)(mod + sym_off + 4 + 24);
            CHECK(token == 0x06000001 || token == 0x06000042, "method token %08lx", token);
            found_proc++;
            CHECK(*(WORD *)(mod + *(DWORD *)(mod + sym_off + 8) + 2) == 6, "proc end points to S_END");
        }
        if (kind == 0x1120) nd++;
        sym_off += len + 2;
    }
    CHECK(sym_off == sym, "symbols tile exactly");
    CHECK(found_proc == 2, "procs %lu", found_proc);
    CHECK(nd == 2, "slots %lu", nd);
    for (sym_off = sym; sym_off < sym + c13; )
    {
        DWORD kind = *(DWORD *)(mod + sym_off), len = *(DWORD *)(mod + sym_off + 4);
        if (kind == 0xf2)
        {
            DWORD nl = *(DWORD *)(mod + sym_off + 8 + 12 + 4);
            CHECK(nl == 3, "lines %lu", nl);
            CHECK(*(DWORD *)(mod + sym_off + 8 + 12 + 12) == 0 && (*(DWORD *)(mod + sym_off + 8 + 12 + 16) & 0xffffff) == 10, "first line");
            found_lines++;
        }
        if (kind == 0xf4)
        {
            CHECK(mod[sym_off + 8 + 4] == 20 && mod[sym_off + 8 + 5] == 2, "checksum size/kind");
            CHECK(mod[sym_off + 8 + 6] == 0xab, "checksum bytes");
            found_chk++;
        }
        sym_off += 8 + len;
    }
    CHECK(found_lines == 1 && found_chk == 1, "subsections %lu %lu", found_lines, found_chk);
    free(file);
}

int main(void)
{
    HMODULE dll;
    HRESULT (WINAPI *get_class)(REFCLSID, REFIID, void **);
    void *factory = NULL, *w = NULL, *pdbw = NULL, *doc = NULL, *doc2 = NULL;
    HRESULT hr;
    WCHAR path[MAX_PATH], path2[MAX_PATH], buf[MAX_PATH];
    BYTE chk[20], dbg[512];
    BYTE src[] = "class A{}\n";
    IMAGE_DEBUG_DIRECTORY idd;
    DWORD cdata = 0, ts, age, id = 0;
    ULONG32 off[3] = {0, 5, 12}, line[3] = {10, 11, 12}, col[3] = {1, 1, 1}, eline[3] = {10, 11, 12}, ecol[3] = {9, 9, 9};
    VARIANT v;
    SYMSRV_INDEX_INFOW info;
    GUID rsds_guid;
    IStream *stream;
    LARGE_INTEGER zero = {{0}};
    STATSTG stat;
    int i;

    CoInitialize(NULL);
    GetCurrentDirectoryW(MAX_PATH, path);
    wcscat(path, L"\\out.pdb");
    DeleteFileW(path);

    dll = LoadLibraryA("diasymreader.dll");
    CHECK(dll != NULL, "load diasymreader err=%lu", GetLastError());
    if (!dll) goto end;
    get_class = (void *)GetProcAddress(dll, "DllGetClassObject");
    hr = get_class(&CLSID_Writer, &IID_IClassFactory, &factory);
    CHECK(hr == S_OK, "class factory %08lx", hr);
    hr = CALL(factory, 3, HRESULT, NULL, &IID_Writer5, &w);
    CHECK(hr == S_OK, "create writer %08lx", hr);
    if (hr != S_OK) goto end;
    hr = CALL(w, 0, HRESULT, &IID_PdbWriter, &pdbw);
    CHECK(hr == S_OK && pdbw, "IPdbWriter");

    /* state errors */
    hr = CALL(w, W_CloseMethod, HRESULT);
    CHECK(hr == E_UNEXPECTED, "CloseMethod without method %08lx", hr);
    hr = CALL(w, W_OpenScope, HRESULT, 0, &id);
    CHECK(hr == E_UNEXPECTED, "OpenScope without method %08lx", hr);
    hr = CALL(w, W_CloseNamespace, HRESULT);
    CHECK(hr == E_UNEXPECTED, "CloseNamespace without namespace %08lx", hr);
    hr = CALL(w, W_Initialize, HRESULT, NULL, NULL, NULL, TRUE);
    CHECK(hr == E_INVALIDARG, "Initialize without target %08lx", hr);

    hr = CALL(w, W_Initialize, HRESULT, NULL, path, NULL, TRUE);
    CHECK(hr == S_OK, "Initialize %08lx", hr);

    hr = CALL(pdbw, 6, HRESULT, MAX_PATH, &cdata, buf);
    CHECK(hr == S_OK && !wcscmp(buf, path) && cdata == wcslen(path) + 1, "GetPath %08lx", hr);
    hr = CALL(pdbw, 6, HRESULT, 2, &cdata, buf);
    CHECK(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), "GetPath small %08lx", hr);

    hr = CALL(w, W_DefineDocument, HRESULT, L"a.cs", &lang_cs, &lang_cs, &lang_cs, &doc);
    CHECK(hr == S_OK && doc, "DefineDocument %08lx", hr);
    hr = CALL(w, W_DefineDocument, HRESULT, L"A.CS", &lang_cs, &lang_cs, &lang_cs, &doc2);
    CHECK(hr == S_OK && doc2 == doc, "same document returned");
    hr = CALL(w, W_DefineDocument, HRESULT, NULL, &lang_cs, &lang_cs, &lang_cs, &doc2);
    CHECK(hr == E_INVALIDARG, "DefineDocument(NULL) %08lx", hr);
    memset(chk, 0xab, sizeof(chk));
    hr = CALL(doc, 4, HRESULT, sha1_alg, 20, chk);
    CHECK(hr == S_OK, "SetChecksum %08lx", hr);
    hr = CALL(doc, 3, HRESULT, 10, src);
    CHECK(hr == S_OK, "SetSource %08lx", hr);

    hr = CALL(w, W_OpenMethod, HRESULT, 0x06000001);
    CHECK(hr == S_OK, "OpenMethod %08lx", hr);
    hr = CALL(w, W_OpenMethod, HRESULT, 0x06000002);
    CHECK(hr == E_UNEXPECTED, "OpenMethod nested %08lx", hr);
    hr = CALL(w, W_OpenScope, HRESULT, 0, &id);
    CHECK(hr == S_OK && id == 0, "OpenScope %08lx %lu", hr, id);
    hr = CALL(w, W_OpenScope, HRESULT, 4, &id);
    CHECK(hr == S_OK && id == 1, "OpenScope 2 %08lx %lu", hr, id);
    hr = CALL(w, W_DefineLocalVariable, HRESULT, L"x", 0, 0, (void *)0, 1, 0, 0, 0, 0, 0);
    CHECK(hr == S_OK, "DefineLocalVariable %08lx", hr);
    hr = CALL(w, W_DefineLocalVariable, HRESULT, NULL, 0, 0, (void *)0, 1, 0, 0, 0, 0, 0);
    CHECK(hr == E_INVALIDARG, "DefineLocalVariable(NULL name) %08lx", hr);
    hr = CALL(w, W_CloseScope, HRESULT, 8);
    CHECK(hr == S_OK, "CloseScope %08lx", hr);
    hr = CALL(w, W_CloseScope, HRESULT, 20);
    CHECK(hr == S_OK, "CloseScope 2 %08lx", hr);
    hr = CALL(w, W_CloseScope, HRESULT, 20);
    CHECK(hr == E_UNEXPECTED, "CloseScope without scope %08lx", hr);
    hr = CALL(w, W_DefineSequencePoints, HRESULT, doc, 3, off, line, col, eline, ecol);
    CHECK(hr == S_OK, "DefineSequencePoints %08lx", hr);
    hr = CALL(w, W_DefineSequencePoints, HRESULT, NULL, 3, off, line, col, eline, ecol);
    CHECK(hr == E_INVALIDARG, "DefineSequencePoints(NULL doc) %08lx", hr);
    hr = CALL(w, W_CloseMethod, HRESULT);
    CHECK(hr == S_OK, "CloseMethod %08lx", hr);
    hr = CALL(w, W_DefineSequencePoints, HRESULT, doc, 3, off, line, col, eline, ecol);
    CHECK(hr == E_UNEXPECTED, "DefineSequencePoints outside method %08lx", hr);

    hr = CALL(w, W_OpenMethod2, HRESULT, 0x06000099, 1, 0x100);
    CHECK(hr == S_OK, "OpenMethod2 %08lx", hr);
    V_VT(&v) = VT_I4; V_I4(&v) = -5;
    hr = CALL(w, W_DefineConstant, HRESULT, L"c", v, 0, NULL);
    CHECK(hr == S_OK, "DefineConstant %08lx", hr);
    V_VT(&v) = VT_DISPATCH; V_DISPATCH(&v) = NULL;
    hr = CALL(w, W_DefineConstant, HRESULT, L"c2", v, 0, NULL);
    CHECK(hr == E_INVALIDARG, "DefineConstant bad variant %08lx", hr);
    hr = CALL(w, W_UsingNamespace, HRESULT, L"System");
    CHECK(hr == S_OK, "UsingNamespace %08lx", hr);
    hr = CALL(w, W_DefineParameter, HRESULT, L"p", 0, 1, 1, 0, 0, 0);
    CHECK(hr == S_OK, "DefineParameter %08lx", hr);
    hr = CALL(w, W_CloseMethod, HRESULT);
    CHECK(hr == S_OK, "CloseMethod 2 %08lx", hr);
    hr = CALL(w, W_RemapToken, HRESULT, 0x06000099, 0x06000042);
    CHECK(hr == S_OK, "RemapToken %08lx", hr);
    hr = CALL(w, W_SetSymAttributes, HRESULT, 0x06000001, L"MD2", 3, (void *)"abc");
    CHECK(hr == S_OK, "SetSymAttributes %08lx", hr);

    hr = CALL(w, W_GetDebugInfo, HRESULT, &idd, sizeof(dbg), &cdata, dbg);
    CHECK(hr == S_OK && cdata > 24 && !memcmp(dbg, "RSDS", 4), "GetDebugInfo %08lx", hr);
    CHECK(idd.Type == IMAGE_DEBUG_TYPE_CODEVIEW && idd.SizeOfData == cdata, "debug directory");
    memcpy(&rsds_guid, dbg + 4, 16);
    hr = CALL(pdbw, 7, HRESULT, &ts, &age);
    CHECK(hr == S_OK && age == 1 && ts == idd.TimeDateStamp, "signature/age %08lx", hr);
    CHECK(!strcmp((char *)dbg + 24, "") == 0, "debug dir has the pdb path");

    hr = CALL(w, W_Close, HRESULT);
    CHECK(hr == S_OK, "Close %08lx", hr);
    check_pdb(path, &rsds_guid, age, ts);

    /* dbghelp reads the same identity back */
    memset(&info, 0, sizeof(info));
    info.sizeofstruct = sizeof(info);
    if (SymSrvGetFileIndexInfoW(path, &info, 0))
    {
        CHECK(!memcmp(&info.guid, &rsds_guid, 16), "dbghelp guid");
        CHECK(info.age == age, "dbghelp age %lu", info.age);
    }
    else CHECK(0, "SymSrvGetFileIndexInfoW failed %lu", GetLastError());

    for (i = 0; w && i < 2; i++) { /* release */ }
    CALL(pdbw, 2, ULONG);
    CALL(w, 2, ULONG);

    /* stream target, Commit, and Abort */
    hr = get_class ? CALL(factory, 3, HRESULT, NULL, &IID_Writer5, &w) : E_FAIL;
    CHECK(hr == S_OK, "second writer");
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    hr = CALL(w, W_Initialize2, HRESULT, NULL, L"t.pdb", stream, TRUE, L"c:\\final.pdb");
    CHECK(hr == S_OK, "Initialize2 %08lx", hr);
    hr = CALL(w, W_OpenMethod, HRESULT, 0x06000007);
    hr = CALL(w, W_CloseMethod, HRESULT);
    hr = CALL(w, W_Commit, HRESULT);
    CHECK(hr == S_OK, "Commit %08lx", hr);
    hr = stream->lpVtbl->Stat(stream, &stat, STATFLAG_NONAME);
    CHECK(hr == S_OK && stat.cbSize.QuadPart >= 4096 * 6 && stat.cbSize.QuadPart % 4096 == 0, "stream size %llu", (unsigned long long)stat.cbSize.QuadPart);
    hr = CALL(w, W_GetDebugInfo, HRESULT, NULL, sizeof(dbg), &cdata, dbg);
    CHECK(hr == S_OK && strstr((char *)dbg + 24, "c:\\final.pdb") != NULL, "debug info uses the final path");
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    {
        BYTE magic[32];
        ULONG n;
        IStream_Read(stream, magic, 32, &n);
        CHECK(!memcmp(magic, "Microsoft C/C++ MSF 7.00\r\n", 26), "stream holds an MSF file");
    }
    CALL(w, 2, ULONG);
    IStream_Release(stream);

    wcscpy(path2, path); wcscat(path2, L".abort");
    DeleteFileW(path2);
    hr = CALL(factory, 3, HRESULT, NULL, &IID_Writer5, &w);
    hr = CALL(w, W_Initialize, HRESULT, NULL, path2, NULL, TRUE);
    hr = CALL(w, W_Abort, HRESULT);
    hr = CALL(w, W_Close, HRESULT);
    CHECK(GetFileAttributesW(path2) == INVALID_FILE_ATTRIBUTES, "aborted writer leaves no file");
    CALL(w, 2, ULONG);

end:
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
