/* shlwapi stub batch (patches/sg/2005), run by test/shlwapistub-gate.sh.
 * Families: AssocGetPerceivedType, the SHCreateStreamWrapper stream methods
 * (CopyTo, Commit, Revert, Lock/Unlock, Clone), MLLoadLibrary/MLFreeLibrary/
 * MLIsMLHInstance, SHSetDefaultDialogFont, SHGetInverseCMAP,
 * SHRegisterValidateTemplate, SHCreatePropertyBagOnRegKey, SHAutoComplete
 * and SHWinHelpOnDemand.
 *
 *   shlwapistub-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

static HMODULE shlwapi;
static FARPROC ord(int n) { return GetProcAddress(shlwapi, MAKEINTRESOURCEA(n)); }

/* ---- AssocGetPerceivedType ------------------------------------------ */

static void set_class_value(const WCHAR *ext, const WCHAR *name, const WCHAR *value)
{
    HKEY key;
    if (!RegCreateKeyExW(HKEY_CLASSES_ROOT, ext, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL))
    {
        RegSetValueExW(key, name, 0, REG_SZ, (const BYTE *)value, (lstrlenW(value) + 1) * sizeof(WCHAR));
        RegCloseKey(key);
    }
}

static const struct
{
    const WCHAR *ext;
    PERCEIVED type;
    int flags;
    const WCHAR *name;
    const char *label;
} perceived_cases[] =
{
    { L".txt",     PERCEIVED_TYPE_TEXT,        PERCEIVEDFLAG_HARDCODED, L"text",       "txt is text" },
    { L".JPG",     PERCEIVED_TYPE_IMAGE,       PERCEIVEDFLAG_HARDCODED, L"image",      "JPG is image (case blind)" },
    { L".wav",     PERCEIVED_TYPE_AUDIO,       PERCEIVEDFLAG_HARDCODED, L"audio",      "wav is audio" },
    { L".avi",     PERCEIVED_TYPE_VIDEO,       PERCEIVEDFLAG_HARDCODED, L"video",      "avi is video" },
    { L".zip",     PERCEIVED_TYPE_COMPRESSED,  PERCEIVEDFLAG_HARDCODED, L"compressed", "zip is compressed" },
    { L".exe",     PERCEIVED_TYPE_APPLICATION, PERCEIVEDFLAG_HARDCODED, L"application","exe is application" },
    { L".sgaudio", PERCEIVED_TYPE_AUDIO,       PERCEIVEDFLAG_SOFTCODED, L"audio",      "registry PerceivedType audio" },
    { L".sgdoc",   PERCEIVED_TYPE_DOCUMENT,    PERCEIVEDFLAG_SOFTCODED, L"document",   "registry PerceivedType document" },
    { L".sgcust",  PERCEIVED_TYPE_CUSTOM,      PERCEIVEDFLAG_SOFTCODED, L"widget",     "registry PerceivedType custom text" },
    { L".sgmime",  PERCEIVED_TYPE_VIDEO,       PERCEIVEDFLAG_SOFTCODED, L"video",      "Content Type video/x-foo" },
    { L".sgapp",   PERCEIVED_TYPE_UNSPECIFIED, PERCEIVEDFLAG_UNDEFINED, NULL,          "Content Type application/x-foo" },
    { L".sgnone",  PERCEIVED_TYPE_UNSPECIFIED, PERCEIVEDFLAG_UNDEFINED, NULL,          "unknown extension" },
    { L"txt",      PERCEIVED_TYPE_UNSPECIFIED, PERCEIVEDFLAG_UNDEFINED, NULL,          "extension without dot" },
};

static void test_perceived(void)
{
    unsigned int i;
    HRESULT hr;
    PERCEIVED type;
    PERCEIVEDFLAG flags;
    LPWSTR name;

    set_class_value(L".sgaudio", L"PerceivedType", L"audio");
    set_class_value(L".sgdoc", L"PerceivedType", L"Document");
    set_class_value(L".sgcust", L"PerceivedType", L"widget");
    set_class_value(L".sgmime", L"Content Type", L"video/x-foo");
    set_class_value(L".sgapp", L"Content Type", L"application/x-foo");

    for (i = 0; i < ARRAY_SIZE(perceived_cases); i++)
    {
        char what[160];
        BOOL ok;

        type = 0x77; flags = 0x77; name = (LPWSTR)0x77;
        hr = AssocGetPerceivedType(perceived_cases[i].ext, &type, &flags, &name);
        ok = hr == S_OK && type == perceived_cases[i].type && (int)flags == perceived_cases[i].flags;
        if (perceived_cases[i].name)
            ok = ok && name && !lstrcmpW(name, perceived_cases[i].name);
        else
            ok = ok && !name;
        snprintf(what, sizeof(what), "%s (hr %lx type %d flags %d)", perceived_cases[i].label, hr, (int)type, (int)flags);
        check(ok, what);
        if (hr == S_OK && name && name != (LPWSTR)0x77) CoTaskMemFree(name);
    }

    type = 0x77; flags = 0x77;
    hr = AssocGetPerceivedType(L".txt", &type, &flags, NULL);
    check(hr == S_OK && type == PERCEIVED_TYPE_TEXT, "optional type text may be NULL");
    hr = AssocGetPerceivedType(NULL, &type, &flags, NULL);
    check(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "NULL extension fails");
}

/* ---- SHCreateStreamWrapper methods ----------------------------------- */

static HRESULT (WINAPI *pSHCreateStreamWrapper)(BYTE *, DWORD, DWORD, IStream **);
#define SHCreateStreamWrapper pSHCreateStreamWrapper

static void test_stream(void)
{
    IStream *s = NULL, *dst = NULL, *clone = NULL, *clone2 = NULL;
    ULARGE_INTEGER cb, rd, wr;
    LARGE_INTEGER zero = {{0}};
    ULARGE_INTEGER pos;
    char buf[32];
    ULONG n;
    STATSTG st;
    HRESULT hr;

    pSHCreateStreamWrapper = (void *)GetProcAddress(shlwapi, "SHCreateStreamWrapper");
    if (!pSHCreateStreamWrapper) { check(0, "SHCreateStreamWrapper present"); return; }
    hr = SHCreateStreamWrapper(NULL, 0, 0, &s);
    CHECK(hr == S_OK && s);
    hr = SHCreateStreamWrapper(NULL, 0, 0, &dst);
    CHECK(hr == S_OK && dst);
    if (!s || !dst) return;

    hr = IStream_Write(s, "hello world", 11, &n);
    CHECK(hr == S_OK && n == 11);
    IStream_Seek(s, zero, STREAM_SEEK_SET, NULL);

    cb.QuadPart = 5;
    rd.QuadPart = wr.QuadPart = 99;
    hr = IStream_CopyTo(s, dst, cb, &rd, &wr);
    check(hr == S_OK && rd.QuadPart == 5 && wr.QuadPart == 5, "CopyTo copies the requested count");
    IStream_Seek(s, zero, STREAM_SEEK_CUR, &pos);
    check(pos.QuadPart == 5, "CopyTo advances the source position");
    cb.QuadPart = 1000;
    hr = IStream_CopyTo(s, dst, cb, &rd, &wr);
    check(hr == S_OK && rd.QuadPart == 6 && wr.QuadPart == 6, "CopyTo stops at the end of the data");
    hr = IStream_CopyTo(s, dst, cb, &rd, &wr);
    check(hr == S_OK && rd.QuadPart == 0 && wr.QuadPart == 0, "CopyTo at the end copies nothing");
    IStream_Seek(dst, zero, STREAM_SEEK_SET, NULL);
    memset(buf, 0, sizeof(buf));
    hr = IStream_Read(dst, buf, sizeof(buf), &n);
    check(hr == S_OK && n == 11 && !memcmp(buf, "hello world", 11), "destination holds the whole text");
    hr = IStream_CopyTo(s, NULL, cb, NULL, NULL);
    check(hr == STG_E_INVALIDPOINTER, "CopyTo without destination fails");

    CHECK(IStream_Commit(s, 0) == S_OK);
    CHECK(IStream_Revert(s) == S_OK);
    cb.QuadPart = 4;
    pos.QuadPart = 0;
    CHECK(IStream_LockRegion(s, pos, cb, LOCK_WRITE) == STG_E_INVALIDFUNCTION);
    CHECK(IStream_UnlockRegion(s, pos, cb, LOCK_WRITE) == STG_E_INVALIDFUNCTION);

    IStream_Seek(s, zero, STREAM_SEEK_SET, NULL);
    IStream_Seek(s, zero, STREAM_SEEK_CUR, &pos);
    hr = IStream_Clone(s, &clone);
    CHECK(hr == S_OK && clone && clone != s);
    if (clone)
    {
        LARGE_INTEGER four;
        four.QuadPart = 6;
        IStream_Seek(s, four, STREAM_SEEK_SET, NULL);
        IStream_Seek(clone, zero, STREAM_SEEK_CUR, &pos);
        check(pos.QuadPart == 0, "clone starts at the position it was cloned at");
        hr = IStream_Write(clone, "HELLO", 5, &n);
        CHECK(hr == S_OK && n == 5);
        memset(buf, 0, sizeof(buf));
        hr = IStream_Read(s, buf, 5, &n);
        check(hr == S_OK && n == 5 && !memcmp(buf, "world", 5), "clone has its own position");
        IStream_Seek(s, zero, STREAM_SEEK_SET, NULL);
        memset(buf, 0, sizeof(buf));
        hr = IStream_Read(s, buf, 11, &n);
        check(n == 11 && !memcmp(buf, "HELLO world", 11), "clone writes are visible through the original");
        /* growing through the clone grows the shared data */
        IStream_Seek(clone, zero, STREAM_SEEK_END, NULL);
        IStream_Write(clone, "!!", 2, NULL);
        memset(&st, 0, sizeof(st));
        hr = IStream_Stat(s, &st, STATFLAG_NONAME);
        check(hr == S_OK && st.cbSize.QuadPart == 13, "size is shared with the clone");
        hr = IStream_Clone(clone, &clone2);
        check(hr == S_OK && clone2, "clone of a clone");
        IStream_Release(s);
        s = NULL;
        IStream_Seek(clone, zero, STREAM_SEEK_SET, NULL);
        memset(buf, 0, sizeof(buf));
        hr = IStream_Read(clone, buf, 13, &n);
        check(hr == S_OK && n == 13 && !memcmp(buf, "HELLO world!!", 13), "clone outlives the original");
        if (clone2)
        {
            IStream_Seek(clone2, zero, STREAM_SEEK_SET, NULL);
            n = 0;
            IStream_Read(clone2, buf, 5, &n);
            check(n == 5, "clone of clone outlives the original");
            IStream_Release(clone2);
        }
        IStream_Release(clone);
    }
    if (s) IStream_Release(s);
    IStream_Release(dst);
}

/* ---- ML library list --------------------------------------------------- */

static void test_ml(void)
{
    BOOL (WINAPI *is_mlh)(HMODULE) = (void *)ord(429);
    HMODULE (WINAPI *loadw)(LPCWSTR, HMODULE, DWORD) = (void *)GetProcAddress(shlwapi, "MLLoadLibraryW");
    HMODULE (WINAPI *loada)(LPCSTR, HMODULE, DWORD) = (void *)GetProcAddress(shlwapi, "MLLoadLibraryA");
    BOOL (WINAPI *freelib)(HMODULE) = (void *)GetProcAddress(shlwapi, "MLFreeLibrary");
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    HMODULE a, b, c;

    if (!is_mlh || !loadw || !loada || !freelib) { check(0, "ML exports present"); return; }

    CHECK(!is_mlh(k32));
    a = loadw(L"version.dll", k32, 0);
    CHECK(a != NULL);
    CHECK(is_mlh(a));
    b = loadw(L"version.dll", k32, 0);
    CHECK(b == a);
    CHECK(freelib(a));
    check(is_mlh(a), "still listed after the first of two loads is freed");
    CHECK(freelib(b));
    check(!is_mlh(a), "no longer listed after the last free");

    c = loada("version.dll", k32, 0);
    CHECK(c != NULL && is_mlh(c));
    CHECK(freelib(c));
    CHECK(!is_mlh(c));

    SetLastError(0);
    CHECK(loadw(L"sg-no-such-module.dll", k32, 0) == NULL);
    CHECK(!is_mlh(NULL));
}

/* ---- SHSetDefaultDialogFont ---------------------------------------------- */

static void test_dialog_font(void)
{
    HRESULT (WINAPI *setfont)(HWND, INT) = (void *)ord(220);
    HANDLE (WINAPI *rmfont)(HWND) = (void *)ord(221);
    HWND parent, child;
    HFONT font;

    if (!setfont || !rmfont) { check(0, "dialog font exports present"); return; }
    parent = CreateWindowW(L"STATIC", L"dlg", WS_POPUP, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    child = CreateWindowW(L"EDIT", L"", WS_CHILD, 0, 0, 50, 20, parent, (HMENU)100, NULL, NULL);
    CHECK(parent && child);
    if (!parent || !child) return;

    CHECK(GetPropA(parent, "PropDlgFont") == NULL);
    CHECK(setfont(parent, 100) == S_OK);
    font = GetPropA(parent, "PropDlgFont");
    check(font != NULL, "PropDlgFont property is set on the parent");
    check(font && (HFONT)SendMessageW(child, WM_GETFONT, 0, 0) == font, "the child uses the dialog font");
    CHECK(setfont(parent, 100) == S_OK);
    check(GetPropA(parent, "PropDlgFont") == font, "second call keeps the same font");
    CHECK(setfont(parent, 999) == S_OK);
    check(rmfont(parent) != NULL, "SHRemoveDefaultDialogFont returns the font");
    check(GetPropA(parent, "PropDlgFont") == NULL, "property is removed");
    DestroyWindow(parent);
}

/* ---- SHGetInverseCMAP / SHRegisterValidateTemplate ----------------------- */

static void test_misc(void)
{
    HRESULT (WINAPI *inverse)(LPDWORD, DWORD) = (void *)GetProcAddress(shlwapi, "SHGetInverseCMAP");
    HRESULT (WINAPI *validate)(LPCWSTR, BOOL) = (void *)GetProcAddress(shlwapi, "SHRegisterValidateTemplate");
    static BYTE table[8192];
    DWORD ptrval = 0;
    WCHAR path[MAX_PATH];
    HANDLE file;
    DWORD n;

    if (!inverse || !validate) { check(0, "SHGetInverseCMAP, SHRegisterValidateTemplate present"); return; }
    CHECK(inverse(NULL, 4) == E_POINTER);
    CHECK(inverse((LPDWORD)table, 3) == E_INVALIDARG);
    CHECK(inverse((LPDWORD)table, 8191) == E_INVALIDARG);
    CHECK(inverse((LPDWORD)table, 8192) == S_OK);
    CHECK(inverse(&ptrval, 4) == S_OK);
    check(ptrval != 0 && ptrval != 0xabba1249, "pointer form returns a real table address");

    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"sg-template.tmp");
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(file, "template data", 13, &n, NULL);
    CloseHandle(file);
    CHECK(validate(path, 0) == S_OK);
    DeleteFileW(path);
    CHECK(validate(path, 0) == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    CHECK(validate(NULL, 0) == E_INVALIDARG);
}

/* ---- SHCreatePropertyBagOnRegKey ----------------------------------------- */

static void test_propbag(void)
{
    HRESULT (WINAPI *create)(HKEY, LPCWSTR, DWORD, REFIID, void **) = (void *)ord(471);
    static const WCHAR keyname[] = L"Software\\SGProbeBag";
    IPropertyBag *bag = NULL, *rw = NULL;
    IUnknown *unk = NULL;
    void *junk = NULL;
    VARIANT v;
    DWORD dw = 42, type, size;
    HKEY key;
    HRESULT hr;
    WCHAR str[64];

    if (!create) { check(0, "SHCreatePropertyBagOnRegKey present"); return; }
    RegDeleteTreeW(HKEY_CURRENT_USER, keyname);
    RegCreateKeyExW(HKEY_CURRENT_USER, keyname, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    RegSetValueExW(key, L"Name", 0, REG_SZ, (const BYTE *)L"hello", 6 * sizeof(WCHAR));
    RegSetValueExW(key, L"Num", 0, REG_DWORD, (const BYTE *)&dw, sizeof(dw));
    RegSetValueExW(key, L"Bin", 0, REG_BINARY, (const BYTE *)"abc", 3);
    RegCloseKey(key);

    hr = create(HKEY_CURRENT_USER, keyname, STGM_READ, &IID_IPropertyBag, (void **)&bag);
    check(hr == S_OK && bag, "create a read-only property bag");
    if (!bag) return;

    VariantInit(&v);
    hr = IPropertyBag_Read(bag, L"Name", &v, NULL);
    check(hr == S_OK && V_VT(&v) == VT_BSTR && !lstrcmpW(V_BSTR(&v), L"hello"), "REG_SZ reads as a BSTR");
    VariantClear(&v);
    VariantInit(&v);
    hr = IPropertyBag_Read(bag, L"Num", &v, NULL);
    check(hr == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == 42, "REG_DWORD reads as a VT_I4");
    VariantClear(&v);
    VariantInit(&v);
    V_VT(&v) = VT_BSTR;
    hr = IPropertyBag_Read(bag, L"Num", &v, NULL);
    check(hr == S_OK && V_VT(&v) == VT_BSTR && !lstrcmpW(V_BSTR(&v), L"42"), "DWORD converted to the requested BSTR");
    VariantClear(&v);
    VariantInit(&v);
    V_VT(&v) = VT_I4;
    hr = IPropertyBag_Read(bag, L"Name", &v, NULL);
    check(hr == DISP_E_TYPEMISMATCH, "text that is no number fails the requested conversion");
    VariantInit(&v);
    hr = IPropertyBag_Read(bag, L"Missing", &v, NULL);
    check(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "missing value");
    VariantInit(&v);
    hr = IPropertyBag_Read(bag, L"Bin", &v, NULL);
    check(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), "binary value is unsupported");

    VariantInit(&v);
    V_VT(&v) = VT_BSTR;
    V_BSTR(&v) = SysAllocString(L"x");
    hr = IPropertyBag_Write(bag, L"Name", &v);
    check(hr == STG_E_ACCESSDENIED, "read-only bag refuses writes");
    VariantClear(&v);
    IPropertyBag_Release(bag);

    hr = create(HKEY_CURRENT_USER, keyname, STGM_READWRITE, &IID_IPropertyBag, (void **)&rw);
    check(hr == S_OK && rw, "create a read-write property bag");
    if (rw)
    {
        VariantInit(&v);
        V_VT(&v) = VT_BSTR;
        V_BSTR(&v) = SysAllocString(L"newval");
        hr = IPropertyBag_Write(rw, L"Name", &v);
        VariantClear(&v);
        CHECK(hr == S_OK);
        V_VT(&v) = VT_I4; V_I4(&v) = 7;
        CHECK(IPropertyBag_Write(rw, L"Num", &v) == S_OK);
        V_VT(&v) = VT_BOOL; V_BOOL(&v) = VARIANT_TRUE;
        CHECK(IPropertyBag_Write(rw, L"Flag", &v) == S_OK);
        IPropertyBag_Release(rw);

        RegOpenKeyExW(HKEY_CURRENT_USER, keyname, 0, KEY_READ, &key);
        size = sizeof(str);
        hr = RegQueryValueExW(key, L"Name", NULL, &type, (BYTE *)str, &size);
        check(!hr && type == REG_SZ && !lstrcmpW(str, L"newval"), "BSTR written as REG_SZ");
        size = sizeof(dw);
        hr = RegQueryValueExW(key, L"Num", NULL, &type, (BYTE *)&dw, &size);
        check(!hr && type == REG_DWORD && dw == 7, "VT_I4 written as REG_DWORD");
        size = sizeof(dw);
        hr = RegQueryValueExW(key, L"Flag", NULL, &type, (BYTE *)&dw, &size);
        check(!hr && type == REG_DWORD && dw == 1, "VT_BOOL true written as DWORD 1");
        RegCloseKey(key);
    }

    hr = create(HKEY_CURRENT_USER, L"Software\\SGProbeBag\\absent", STGM_READ, &IID_IPropertyBag, (void **)&bag);
    check(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) && !bag, "absent subkey fails without STGM_CREATE");
    hr = create(HKEY_CURRENT_USER, L"Software\\SGProbeBag\\made", STGM_CREATE | STGM_READWRITE,
                &IID_IPropertyBag, (void **)&bag);
    check(hr == S_OK && bag, "STGM_CREATE makes the subkey");
    if (bag) IPropertyBag_Release(bag);
    check(!RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\SGProbeBag\\made", 0, KEY_READ, &key), "subkey exists");
    RegCloseKey(key);

    hr = create(HKEY_CURRENT_USER, keyname, STGM_READ, &IID_IUnknown, (void **)&unk);
    check(hr == S_OK && unk, "IUnknown is accepted");
    if (unk) IUnknown_Release(unk);
    hr = create(HKEY_CURRENT_USER, keyname, STGM_READ, &IID_IStream, &junk);
    check(hr == E_NOINTERFACE && !junk, "unsupported interface");
    RegDeleteTreeW(HKEY_CURRENT_USER, keyname);
}

/* ---- SHAutoComplete ---------------------------------------------------------- */

static void test_autocomplete(void)
{
    HWND edit;
    LONG_PTR before, after;
    HRESULT hr;

    edit = CreateWindowW(L"EDIT", L"", WS_POPUP | WS_VISIBLE, 0, 0, 200, 20, NULL, NULL, NULL, NULL);
    CHECK(edit != NULL);
    if (!edit) return;
    before = GetWindowLongPtrW(edit, GWLP_WNDPROC);
    hr = SHAutoComplete(edit, SHACF_FILESYSTEM);
    check(hr == S_OK, "SHAutoComplete(file system) succeeds");
    after = GetWindowLongPtrW(edit, GWLP_WNDPROC);
    check(after != before, "the edit control is subclassed by the auto-complete object");
    hr = SHAutoComplete(edit, SHACF_DEFAULT | SHACF_USETAB);
    check(hr == S_OK, "default flags succeed on an already completing edit");
    DestroyWindow(edit);
    hr = SHAutoComplete(edit, SHACF_FILESYSTEM);
    check(hr == E_INVALIDARG, "destroyed window is refused");
}

/* ---- SHWinHelpOnDemand ------------------------------------------------------------ */

static void test_winhelp(void)
{
    DWORD (WINAPI *helpw)(HWND, LPCWSTR, DWORD, void *, DWORD) = (void *)ord(416);
    DWORD (WINAPI *helpa)(HWND, LPCSTR, DWORD, void *, DWORD) = (void *)ord(417);
    DWORD direct, viaw, viaa;

    if (!helpw || !helpa) { check(0, "SHWinHelpOnDemand present"); return; }
    /* HELP_QUIT with no help window open: the result is whatever WinHelp
     * gives, which the stub did not forward to. */
    direct = WinHelpW(NULL, L"C:\\sg-none.hlp", HELP_QUIT, 0);
    viaw = helpw(NULL, L"C:\\sg-none.hlp", HELP_QUIT, NULL, 0);
    viaa = helpa(NULL, "C:\\sg-none.hlp", HELP_QUIT, NULL, 0);
    printf("INFO  WinHelp quit direct %lu, W %lu, A %lu\n", direct, viaw, viaa);
    check(viaw == direct && viaa == direct, "SHWinHelpOnDemand returns WinHelp's result");
    check(viaw != 0, "HELP_QUIT with no window succeeds");
}

int main(void)
{
    CoInitialize(NULL);
    shlwapi = LoadLibraryW(L"shlwapi.dll");
    CHECK(shlwapi != NULL);
    if (!shlwapi) return 1;
    test_perceived();
    test_stream();
    test_ml();
    test_dialog_font();
    test_misc();
    test_propbag();
    test_autocomplete();
    test_winhelp();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
