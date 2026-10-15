/* sxs.dll: IAssemblyName properties/IsEqual/Clone, IAssemblyCacheItem (install by streams), install references (patch 3718)
 * Calls by vtable slot (the mingw headers have no winsxs.h). Installs into C:\windows\winsxs of the (scratch) prefix. */
#include <windows.h>
#include <objidl.h>
#include <stdio.h>
#include <wchar.h>

typedef struct { void **vt; } OBJ;
#define SLOT(o, n) ((OBJ *)(o))->vt[n]
static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define REL(o) ((ULONG (WINAPI *)(void *))SLOT(o, 2))(o)

typedef struct { DWORD cbSize, dwFlags; GUID guidScheme; const WCHAR *szIdentifier, *szNonCannonicalData; } REF;
typedef struct { ULONG cbAssemblyInfo; DWORD dwAssemblyFlags; ULARGE_INTEGER size; WCHAR *path; ULONG cch; } INFO;

#define E_INVALID_NAME ((HRESULT)0x80131047)
enum { NAME_PKT = 1, NAME_NAME = 3, NAME_MAJOR, NAME_MINOR, NAME_BUILD, NAME_REV, NAME_CULTURE, NAME_NULLKEY = 15, NAME_NULLPKT, NAME_CUSTOM, NAME_NULLCUSTOM, NAME_MAX = 20 };

static HRESULT (WINAPI *pCreateName)(void **, const WCHAR *, DWORD, void *);
static HRESULT (WINAPI *pCreateCache)(void **, DWORD);

static HRESULT setp(void *n, DWORD id, const void *p, DWORD sz) { return ((HRESULT (WINAPI *)(void *, DWORD, const void *, DWORD))SLOT(n, 3))(n, id, p, sz); }
static HRESULT getp(void *n, DWORD id, void *b, DWORD *sz) { return ((HRESULT (WINAPI *)(void *, DWORD, void *, DWORD *))SLOT(n, 4))(n, id, b, sz); }
static HRESULT dispname(void *n, WCHAR *b, DWORD *sz) { return ((HRESULT (WINAPI *)(void *, WCHAR *, DWORD *, DWORD))SLOT(n, 6))(n, b, sz, 0); }

static void *mkname(const WCHAR *s)
{
    void *n = NULL;
    HRESULT hr = pCreateName(&n, s, 1, NULL);
    CHECK(hr == S_OK && n, "CreateAssemblyNameObject(%ls) %#lx", s, hr);
    return n;
}

static void test_name(void)
{
    static const WCHAR full[] = L"sg.test.one,processorArchitecture=\"x86\",publicKeyToken=\"0123456789abcdef\",type=\"win32\",version=\"1.2.3.4\"";
    void *n, *c, *o;
    WORD w; DWORD sz; HRESULT hr; BYTE tok[16]; WCHAR buf[300], small[2];
    static const BYTE want[8] = { 1, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef };

    n = mkname(full);
    if (!n) return;
    sz = sizeof(buf); hr = getp(n, NAME_NAME, buf, &sz);
    CHECK(hr == S_OK && sz == 24 && !wcscmp(buf, L"sg.test.one"), "name %#lx %lu %ls", hr, sz, buf);
    sz = 2; hr = getp(n, NAME_NAME, small, &sz);
    CHECK(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) && sz == 24, "small buffer %#lx %lu", hr, sz);
    sz = 2; hr = getp(n, NAME_MAJOR, &w, &sz); CHECK(hr == S_OK && sz == 2 && w == 1, "major %#lx %lu %u", hr, sz, w);
    sz = 2; hr = getp(n, NAME_MINOR, &w, &sz); CHECK(hr == S_OK && sz == 2 && w == 2, "minor %u", w);
    sz = 2; hr = getp(n, NAME_BUILD, &w, &sz); CHECK(hr == S_OK && sz == 2 && w == 3, "build %u", w);
    sz = 2; hr = getp(n, NAME_REV, &w, &sz); CHECK(hr == S_OK && sz == 2 && w == 4, "rev %u", w);
    sz = 1; hr = getp(n, NAME_MAJOR, &w, &sz); CHECK(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) && sz == 2, "word too small %#lx %lu", hr, sz);
    sz = 16; memset(tok, 0, 16); hr = getp(n, NAME_PKT, tok, &sz);
    CHECK(hr == S_OK && sz == 8 && !memcmp(tok, want, 8), "token %#lx %lu", hr, sz);
    sz = 16; hr = getp(n, NAME_CULTURE, buf, &sz); CHECK(hr == S_OK && sz == 0, "unset culture %#lx %lu", hr, sz);
    sz = 16; hr = getp(n, NAME_CUSTOM, buf, &sz); CHECK(hr == S_OK && sz == 0, "unset custom");
    hr = getp(n, NAME_NAME, buf, NULL); CHECK(hr == E_INVALIDARG, "null size %#lx", hr);
    sz = 16; hr = getp(n, NAME_MAX, buf, &sz); CHECK(hr == E_INVALIDARG, "id out of range %#lx", hr);
    sz = 0; hr = getp(n, NAME_NULLPKT, NULL, &sz); CHECK(hr == S_OK, "null pkt with token %#lx", hr);
    sz = 0; hr = getp(n, NAME_NULLCUSTOM, NULL, &sz); CHECK(hr == S_OK, "null custom %#lx", hr);

    /* set: version component, culture, custom blob, token */
    w = 7; hr = setp(n, NAME_MINOR, &w, 2); CHECK(hr == S_OK, "set minor %#lx", hr);
    sz = 2; getp(n, NAME_MINOR, &w, &sz); CHECK(w == 7, "minor now %u", w);
    sz = 300; hr = dispname(n, buf, &sz);
    CHECK(hr == S_OK && wcsstr(buf, L"version=\"1.7.3.4\""), "display after set: %ls", buf);
    hr = setp(n, NAME_MINOR, &w, 3); CHECK(hr == E_INVALIDARG, "set minor with size 3 %#lx", hr);
    hr = setp(n, NAME_CULTURE, L"de", 6); CHECK(hr == S_OK, "set culture");
    sz = 16; hr = getp(n, NAME_CULTURE, buf, &sz); CHECK(hr == S_OK && sz == 6 && !wcscmp(buf, L"de"), "culture %#lx %lu", hr, sz);
    hr = setp(n, NAME_CUSTOM, "abc", 4); CHECK(hr == S_OK, "set custom");
    sz = 8; memset(tok, 0, 16); hr = getp(n, NAME_CUSTOM, tok, &sz); CHECK(hr == S_OK && sz == 4 && !memcmp(tok, "abc", 4), "custom %#lx %lu", hr, sz);
    hr = setp(n, NAME_CUSTOM, NULL, 0); CHECK(hr == S_OK, "remove custom");
    sz = 8; hr = getp(n, NAME_CUSTOM, tok, &sz); CHECK(hr == S_OK && sz == 0, "custom removed");
    hr = setp(n, NAME_PKT, want, 3); CHECK(hr == E_INVALIDARG, "short token %#lx", hr);
    { BYTE t2[8] = { 0xde, 0xad, 0xbe, 0xef, 0, 1, 2, 3 };
      hr = setp(n, NAME_PKT, t2, 8); CHECK(hr == S_OK, "set token");
      sz = 300; dispname(n, buf, &sz); CHECK(wcsstr(buf, L"publicKeyToken=\"deadbeef00010203\""), "display token %ls", buf); }
    hr = setp(n, NAME_NAME, NULL, 4); CHECK(hr == E_INVALIDARG, "null value %#lx", hr);
    hr = setp(n, NAME_MAX, "x", 2); CHECK(hr == E_INVALIDARG, "set id out of range %#lx", hr);
    hr = setp(n, NAME_MAJOR, NULL, 0); CHECK(hr == S_OK, "remove major");
    sz = 2; hr = getp(n, NAME_MAJOR, &w, &sz); CHECK(hr == S_OK && sz == 0, "major removed (all components go) %lu", sz);

    /* clone is independent and modifiable */
    o = mkname(full);
    hr = ((HRESULT (WINAPI *)(void *))SLOT(o, 5))(o);   /* Finalize */
    CHECK(hr == S_OK, "Finalize %#lx", hr);
    w = 9; hr = setp(o, NAME_MAJOR, &w, 2); CHECK(hr == E_UNEXPECTED, "set after Finalize %#lx", hr);
    hr = setp(o, NAME_NAME, NULL, 0); CHECK(hr == E_UNEXPECTED, "remove after Finalize %#lx", hr);
    c = NULL; hr = ((HRESULT (WINAPI *)(void *, void **))SLOT(o, 11))(o, &c);
    CHECK(hr == S_OK && c && c != o, "Clone %#lx", hr);
    if (c)
    {
        sz = 300; hr = dispname(c, buf, &sz);
        CHECK(hr == S_OK && !wcscmp(buf, full), "clone display %ls", buf);
        w = 9; hr = setp(c, NAME_MAJOR, &w, 2); CHECK(hr == S_OK, "set on clone of finalized %#lx", hr);
        sz = 2; getp(o, NAME_MAJOR, &w, &sz); CHECK(w == 1, "original unchanged %u", w);
        REL(c);
    }
    hr = ((HRESULT (WINAPI *)(void *, void **))SLOT(o, 11))(o, NULL); CHECK(hr == E_INVALIDARG, "Clone null %#lx", hr);

    /* IsEqual */
    c = mkname(L"SG.TEST.ONE,processorArchitecture=\"x86\",publicKeyToken=\"0123456789abcdef\",type=\"win32\",version=\"1.2.9.9\"");
    {
        HRESULT (WINAPI *eq)(void *, void *, DWORD) = SLOT(o, 10);
        CHECK(eq(o, c, 0x1) == S_OK, "name equal (case-insens.)");
        CHECK(eq(o, c, 0x2 | 0x4) == S_OK, "major/minor equal");
        CHECK(eq(o, c, 0x8) == S_FALSE, "build differs");
        CHECK(eq(o, c, 0x10) == S_FALSE, "revision differs");
        CHECK(eq(o, c, 0x20) == S_OK, "token equal");
        CHECK(eq(o, c, 0x1 | 0x2 | 0x4 | 0x20 | 0x40) == S_OK, "ignoring build/rev");
        CHECK(eq(o, c, 0xff) == S_FALSE, "ALL differs");
        CHECK(eq(o, c, 0x100) == S_FALSE, "DEFAULT with a token compares the version");
        CHECK(eq(o, o, 0xff) == S_OK, "self");
        CHECK(eq(o, NULL, 0x1) == S_FALSE, "null");
        REL(c);
        c = mkname(L"other.name,processorArchitecture=\"x86\",publicKeyToken=\"0123456789abcdef\",type=\"win32\",version=\"1.2.3.4\"");
        CHECK(eq(o, c, 0x1) == S_FALSE, "name differs");
        CHECK(eq(o, c, 0x2) == S_OK, "version same");
        REL(c);
        c = mkname(L"sg.test.one,processorArchitecture=\"x86\",type=\"win32\",version=\"5.2.3.4\"");
        CHECK(eq(o, c, 0x20) == S_FALSE, "token vs none");
        { void *d = mkname(L"sg.test.one,processorArchitecture=\"x86\",type=\"win32\",version=\"1.1.1.1\"");
          CHECK(eq(c, d, 0x100) == S_OK, "DEFAULT of a simple name ignores the version");
          CHECK(eq(c, d, 0xff) == S_FALSE, "ALL does not");
          REL(d); }
        REL(c);
    }
    REL(o);
    REL(n);
    /* name removal, GetName */
    n = mkname(L"sg.nm,type=\"win32\"");
    hr = setp(n, NAME_NAME, NULL, 0); CHECK(hr == S_OK, "remove name");
    sz = 300; hr = dispname(n, buf, &sz); CHECK(hr == E_INVALID_NAME, "display without a name %#lx", hr);
    { DWORD len = 100; hr = ((HRESULT (WINAPI *)(void *, DWORD *, WCHAR *))SLOT(n, 8))(n, &len, buf); CHECK(hr == S_OK && len == 0, "GetName without a name %#lx %lu", hr, len); }
    REL(n);
}

static const char *manifest_text(const char *ver, const char *file, char *out)
{
    sprintf(out, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
        "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">\r\n"
        "<assemblyIdentity type=\"win32\" name=\"sg.item.test\" version=\"%s\" processorArchitecture=\"x86\" publicKeyToken=\"0123456789abcdef\"/>\r\n"
        "<file name=\"%s\"/>\r\n</assembly>\r\n", ver, file);
    return out;
}

static HRESULT put_stream(void *item, const WCHAR *name, DWORD fmt, const void *data, DWORD len)
{
    IStream *s = NULL; ULONG w;
    HRESULT hr = ((HRESULT (WINAPI *)(void *, DWORD, const WCHAR *, DWORD, DWORD, IStream **, void *))SLOT(item, 3))(item, 0, name, fmt, 0, &s, NULL);
    if (hr == S_OK) { hr = s->lpVtbl->Write(s, data, len, &w); s->lpVtbl->Release(s); }
    return hr;
}

static HRESULT cache_item(void *cache, const WCHAR *name, void **item) { return ((HRESULT (WINAPI *)(void *, DWORD, void *, void **, const WCHAR *))SLOT(cache, 5))(cache, 0, NULL, item, name); }
static HRESULT commit(void *item, DWORD flags, ULONG *disp) { return ((HRESULT (WINAPI *)(void *, DWORD, ULONG *))SLOT(item, 4))(item, flags, disp); }
static HRESULT query(void *cache, const WCHAR *name, WCHAR *path, ULONG cch)
{
    INFO info = { sizeof(info), 0, {{0}}, path, cch };
    HRESULT hr;
    path[0] = 0;
    hr = ((HRESULT (WINAPI *)(void *, DWORD, const WCHAR *, INFO *))SLOT(cache, 4))(cache, 0, name, &info);
    return hr == S_OK && !(info.dwAssemblyFlags & 1) ? S_FALSE : hr;   /* S_FALSE: known name, not installed */
}
static HRESULT uninstall(void *cache, const WCHAR *name, const REF *ref, ULONG *disp)
{ return ((HRESULT (WINAPI *)(void *, DWORD, const WCHAR *, const REF *, ULONG *))SLOT(cache, 3))(cache, 0, name, ref, disp); }

static void test_item(void)
{
    static const WCHAR nm[] = L"sg.item.test,processorArchitecture=\"x86\",publicKeyToken=\"0123456789abcdef\",type=\"win32\",version=\"1.0.0.0\"";
    static const WCHAR nm2[] = L"sg.item.test,processorArchitecture=\"x86\",publicKeyToken=\"0123456789abcdef\",type=\"win32\",version=\"2.0.0.0\"";
    void *cache = NULL, *item = NULL;
    char man[1024]; ULONG disp; HRESULT hr; WCHAR path[MAX_PATH];
    HANDLE h; DWORD n;

    hr = pCreateCache(&cache, 0);
    CHECK(hr == S_OK && cache, "CreateAssemblyCache %#lx", hr);
    if (!cache) return;
    uninstall(cache, nm, NULL, &disp);   /* leftovers from an earlier run */
    uninstall(cache, nm2, NULL, &disp);

    hr = cache_item(cache, L"sg.item.test", &item); CHECK(hr == E_INVALID_NAME && !item, "partial name %#lx", hr);
    hr = cache_item(cache, L"sg.item.test,version=\"1.0.0.0\"", &item); CHECK(hr == E_INVALID_NAME, "partial name 2 %#lx", hr);
    hr = cache_item(cache, nm, &item); CHECK(hr == S_OK && item, "CreateAssemblyCacheItem %#lx", hr);
    if (item)
    {
        IStream *s = NULL;
        manifest_text("1.0.0.0", "sgitem.dll", man);
        hr = put_stream(item, L"sg.item.test.manifest", 4, man, strlen(man)); CHECK(hr == S_OK, "manifest stream %#lx", hr);
        hr = commit(item, 0, &disp); CHECK(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "commit without the module %#lx", hr);
        hr = put_stream(item, L"sg.item.test.manifest", 4, man, 4); CHECK(hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS), "second manifest %#lx", hr);
        hr = ((HRESULT (WINAPI *)(void *, DWORD, const WCHAR *, DWORD, DWORD, IStream **, void *))SLOT(item, 3))(item, 0, L"a\\b", 2, 0, &s, NULL);
        CHECK(hr == E_INVALIDARG, "path in stream name %#lx", hr);
        hr = ((HRESULT (WINAPI *)(void *, DWORD, const WCHAR *, DWORD, DWORD, IStream **, void *))SLOT(item, 3))(item, 0, L"x", 99, 0, &s, NULL);
        CHECK(hr == E_INVALIDARG, "bad format %#lx", hr);
        hr = put_stream(item, L"sgitem.dll", 2, "MODULE-ONE", 10); CHECK(hr == S_OK, "module stream %#lx", hr);
        hr = commit(item, 0x40, &disp); CHECK(hr == E_INVALIDARG, "bad commit flags %#lx", hr);
        hr = commit(item, 0, &disp); CHECK(hr == S_OK && disp == 1, "commit %#lx disp %lu", hr, disp);
        hr = commit(item, 0, &disp); CHECK(hr == E_UNEXPECTED, "second commit %#lx", hr);
        REL(item);
    }
    path[0] = 0;
    hr = query(cache, nm, path, MAX_PATH); CHECK(hr == S_OK && path[0], "assembly is installed %#lx", hr);
    lstrcatW(path, L"sgitem.dll");
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE, "module file %ls exists", path);
    if (h != INVALID_HANDLE_VALUE) { char b[32] = {0}; ReadFile(h, b, 31, &n, NULL); CHECK(n == 10 && !memcmp(b, "MODULE-ONE", 10), "module content"); CloseHandle(h); }

    /* the same content again: already installed.  Different content: refreshed */
    hr = cache_item(cache, nm, &item);
    if (hr == S_OK)
    {
        put_stream(item, L"m.manifest", 4, man, strlen(man)); put_stream(item, L"sgitem.dll", 2, "MODULE-ONE", 10);
        hr = commit(item, 0, &disp); CHECK(hr == S_OK && disp == 3, "same content %#lx disp %lu", hr, disp);
        REL(item);
    }
    hr = cache_item(cache, nm, &item);
    if (hr == S_OK)
    {
        put_stream(item, L"m.manifest", 4, man, strlen(man)); put_stream(item, L"sgitem.dll", 2, "MODULE-TWO!", 11);
        hr = commit(item, 0, &disp); CHECK(hr == S_OK && disp == 2, "new module content %#lx disp %lu", hr, disp);
        REL(item);
    }
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { char b[32] = {0}; ReadFile(h, b, 31, &n, NULL); CHECK(n == 11 && !memcmp(b, "MODULE-TWO!", 11), "module replaced (%lu)", n); CloseHandle(h); }
    hr = cache_item(cache, nm, &item);
    if (hr == S_OK)
    {
        put_stream(item, L"m.manifest", 4, man, strlen(man)); put_stream(item, L"sgitem.dll", 2, "MODULE-TWO!", 11);
        hr = commit(item, 2, &disp); CHECK(hr == S_OK && disp == 2, "force refresh %#lx disp %lu", hr, disp);
        REL(item);
    }

    /* a name that does not match the manifest, and abort */
    hr = cache_item(cache, nm2, &item);
    if (hr == S_OK)
    {
        put_stream(item, L"m.manifest", 4, man, strlen(man)); put_stream(item, L"sgitem.dll", 2, "X", 1);
        hr = commit(item, 0, &disp); CHECK(hr == E_INVALID_NAME, "manifest does not match the name %#lx", hr);
        hr = ((HRESULT (WINAPI *)(void *))SLOT(item, 5))(item); CHECK(hr == S_OK, "AbortItem %#lx", hr);
        hr = commit(item, 0, &disp); CHECK(hr == E_UNEXPECTED, "commit after abort %#lx", hr);
        hr = ((HRESULT (WINAPI *)(void *))SLOT(item, 5))(item); CHECK(hr == E_UNEXPECTED, "second abort %#lx", hr);
        REL(item);
    }
    hr = query(cache, nm2, path, MAX_PATH); CHECK(hr != S_OK, "nothing of the failed item installed (%#lx)", hr);
    hr = cache_item(cache, NULL, &item);   /* no name: not verified */
    if (hr == S_OK)
    {
        put_stream(item, L"m.manifest", 4, man, strlen(man)); put_stream(item, L"sgitem.dll", 2, "MODULE-TWO!", 11);
        hr = commit(item, 0, &disp); CHECK(hr == S_OK && disp == 3, "unnamed item %#lx disp %lu", hr, disp);
        REL(item);
    } else CHECK(0, "item without name %#lx", hr);
    hr = ((HRESULT (WINAPI *)(void *, DWORD, void *, void **, const WCHAR *))SLOT(cache, 5))(cache, 0, NULL, NULL, NULL);
    CHECK(hr == E_INVALIDARG, "null out %#lx", hr);

    /* uninstall: no reference so it goes */
    hr = uninstall(cache, nm, NULL, &disp); CHECK(hr == S_OK && disp == 1, "uninstall %#lx disp %lu", hr, disp);
    hr = uninstall(cache, nm, NULL, &disp); CHECK(hr == S_FALSE && disp == 3, "uninstall again %#lx disp %lu", hr, disp);
    CHECK(GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES, "module gone");
    REL(cache);
}

static void test_refs(void)
{
    static const WCHAR nm[] = L"sg.item.test,processorArchitecture=\"x86\",publicKeyToken=\"0123456789abcdef\",type=\"win32\",version=\"3.0.0.0\"";
    static const GUID g1 = { 0x2ec93463, 0xb0c3, 0x45e1, { 0x83, 0x64, 0x32, 0x7e, 0x96, 0xae, 0xa8, 0x56 } };
    REF a = { sizeof(REF), 0, g1, L"app-A", L"Wine" }, b = { sizeof(REF), 0, g1, L"app-B", NULL }, bad = { 12, 0 };
    void *cache = NULL; char man[1024]; WCHAR dir[MAX_PATH], mpath[MAX_PATH], tmp[MAX_PATH], mf[MAX_PATH];
    ULONG disp; HRESULT hr; HANDLE h; DWORD n;

    pCreateCache(&cache, 0);
    if (!cache) { CHECK(0, "no cache"); return; }
    GetTempPathW(MAX_PATH, tmp);
    swprintf(dir, MAX_PATH, L"%ssgsxsref%lx\\", tmp, GetCurrentProcessId());
    CreateDirectoryW(dir, NULL);
    swprintf(mf, MAX_PATH, L"%sm.manifest", dir);
    swprintf(mpath, MAX_PATH, L"%sref.dll", dir);
    manifest_text("3.0.0.0", "ref.dll", man);
    h = CreateFileW(mf, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); WriteFile(h, man, strlen(man), &n, NULL); CloseHandle(h);
    h = CreateFileW(mpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); WriteFile(h, "REF", 3, &n, NULL); CloseHandle(h);
    uninstall(cache, nm, NULL, &disp);

    hr = ((HRESULT (WINAPI *)(void *, DWORD, const WCHAR *, const REF *))SLOT(cache, 7))(cache, 0, mf, &bad);
    CHECK(hr == E_INVALIDARG, "install with a bad reference %#lx", hr);
    hr = ((HRESULT (WINAPI *)(void *, DWORD, const WCHAR *, const REF *))SLOT(cache, 7))(cache, 8, mf, &a);
    CHECK(hr == E_INVALIDARG, "install with bad flags %#lx", hr);
    hr = query(cache, nm, tmp, MAX_PATH); CHECK(hr != S_OK, "nothing installed by the failed calls (%#lx)", hr);
    hr = ((HRESULT (WINAPI *)(void *, DWORD, const WCHAR *, const REF *))SLOT(cache, 7))(cache, 0, mf, &a);
    CHECK(hr == S_OK, "install with ref A %#lx", hr);
    hr = ((HRESULT (WINAPI *)(void *, DWORD, const WCHAR *, const REF *))SLOT(cache, 7))(cache, 0, mf, &b);
    CHECK(hr == S_OK, "install with ref B %#lx", hr);

    hr = uninstall(cache, nm, NULL, &disp); CHECK(hr == S_FALSE && disp == 5, "uninstall without a ref while refs exist %#lx disp %lu", hr, disp);
    hr = ((HRESULT (WINAPI *)(void *, DWORD, const WCHAR *, const REF *, ULONG *))SLOT(cache, 3))(cache, 1, nm, &a, &disp);
    CHECK(hr == E_INVALIDARG, "uninstall with flags %#lx", hr);
    hr = uninstall(cache, nm, &bad, &disp); CHECK(hr == E_INVALIDARG, "uninstall with a bad ref %#lx", hr);
    hr = uninstall(cache, nm, &a, &disp); CHECK(hr == S_FALSE && disp == 5, "remove A, B left %#lx disp %lu", hr, disp);
    hr = uninstall(cache, nm, &a, &disp); CHECK(hr == S_FALSE && disp == 6, "A again: not found %#lx disp %lu", hr, disp);
    hr = query(cache, nm, tmp, MAX_PATH); CHECK(hr == S_OK && tmp[0], "still installed");
    hr = uninstall(cache, nm, &b, &disp); CHECK(hr == S_OK && disp == 1, "remove B: uninstalled %#lx disp %lu", hr, disp);
    hr = uninstall(cache, nm, &b, &disp); CHECK(hr == S_FALSE && disp == 3, "already gone %#lx disp %lu", hr, disp);
    hr = ((HRESULT (WINAPI *)(void *, DWORD, const WCHAR *, const REF *))SLOT(cache, 7))(cache, 0, mf, NULL);
    CHECK(hr == S_OK, "install without a reference");
    hr = uninstall(cache, nm, NULL, &disp); CHECK(hr == S_OK && disp == 1, "no refs: uninstalled %#lx disp %lu", hr, disp);
    DeleteFileW(mf); DeleteFileW(mpath); RemoveDirectoryW(dir);
    REL(cache);
}

int main(void)
{
    HMODULE m = LoadLibraryA("sxs.dll");
    if (!m) { printf("FAIL no sxs.dll\nRESULT: FAIL\n"); return 1; }
    pCreateName = (void *)GetProcAddress(m, "CreateAssemblyNameObject");
    pCreateCache = (void *)GetProcAddress(m, "CreateAssemblyCache");
    CoInitialize(NULL);
    test_name();
    test_item();
    test_refs();
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
