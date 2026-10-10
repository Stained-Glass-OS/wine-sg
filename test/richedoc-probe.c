/* riched20 ITextDocument batch (patches/sg/2020), run by test/richedoc-gate.sh.
 * Families (from Wine's todo_wine blocks, which record Windows): Open (open
 * dispositions, sharing, code pages, RTF, paste), GetName, GetSaved / SetSaved,
 * New and Save (text in several code pages, RTF, the current file).
 *
 *   richedoc-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static const GUID SG_IID_ITextDocument = { 0x8cc497c0, 0xa1df, 0x11ce, { 0x80, 0x98, 0x00, 0xaa, 0x00, 0x47, 0xbe, 0x5d } };

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
static void checkhr(HRESULT hr, HRESULT want, const char *what)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "%s (hr %08lx, want %08lx)", what, (unsigned long)hr, (unsigned long)want);
    check(hr == want, buf);
}

static WCHAR dir[MAX_PATH];
static HWND edit;
static ITextDocument *doc;
static IUnknown *ole;

static void path_for(const WCHAR *name, WCHAR *out) { swprintf(out, MAX_PATH, L"%lssg-doc-%ls", dir, name); }

static void write_file(const WCHAR *path, const void *data, DWORD size)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD n;
    if (size) WriteFile(h, data, size, &n, NULL);
    CloseHandle(h);
}

static DWORD read_file(const WCHAR *path, BYTE *buf, DWORD max)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    DWORD n = 0;
    if (h == INVALID_HANDLE_VALUE) return 0xffffffff;
    ReadFile(h, buf, max, &n, NULL);
    CloseHandle(h);
    return n;
}

static int exists_exclusive(const WCHAR *path)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    CloseHandle(h);
    return 1;
}

static void new_window(void)
{
    if (doc) ITextDocument_Release(doc);
    if (ole) IUnknown_Release(ole);
    if (edit) DestroyWindow(edit);
    edit = CreateWindowExW(0, L"RichEdit20W", L"", WS_POPUP | ES_MULTILINE, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    SendMessageW(edit, EM_GETOLEINTERFACE, 0, (LPARAM)&ole);
    IUnknown_QueryInterface(ole, &SG_IID_ITextDocument, (void **)&doc);
}

static VARIANT name_var(const WCHAR *path)
{
    VARIANT v;
    VariantInit(&v);
    V_VT(&v) = VT_BSTR;
    V_BSTR(&v) = SysAllocString(path);
    return v;
}

static int text_is(const WCHAR *want)
{
    WCHAR buf[512];
    buf[0] = 0;
    SendMessageW(edit, WM_GETTEXT, ARRAYSIZE(buf), (LPARAM)buf);
    return !wcscmp(buf, want);
}

static void test_dispositions(void)
{
    WCHAR path[MAX_PATH];
    VARIANT v;
    HRESULT hr;
    char buf[200];
    static const struct { const char *name; LONG flags; } ok_flags[] =
    {
        { "ReadOnly", tomReadOnly }, { "ShareDenyRead", tomShareDenyRead }, { "ShareDenyWrite", tomShareDenyWrite },
        { "CreateAlways", tomCreateAlways }, { "OpenExisting", tomOpenExisting }, { "OpenAlways", tomOpenAlways },
        { "TruncateExisting", tomTruncateExisting }, { "RTF", tomRTF }, { "Text", tomText },
        { "ReadOnly|Paste", tomReadOnly | tomPasteFile }, { "DenyRead|DenyWrite", tomShareDenyRead | tomShareDenyWrite },
    };
    unsigned i;

    path_for(L"a.txt", path);
    v = name_var(path);

    for (i = 0; i < ARRAYSIZE(ok_flags); i++)
    {
        write_file(path, "x", 0);
        new_window();
        hr = ITextDocument_Open(doc, &v, ok_flags[i].flags, CP_ACP);
        snprintf(buf, sizeof(buf), "Open of an existing empty file with %s (hr %08lx)", ok_flags[i].name, (unsigned long)hr);
        check(hr == S_OK, buf);
        new_window();
        DeleteFileW(path);
    }

    new_window();
    DeleteFileW(path);
    hr = ITextDocument_Open(doc, &v, tomCreateAlways, CP_ACP);
    check(hr == S_OK && exists_exclusive(path), "CreateAlways creates the file");
    new_window();
    DeleteFileW(path);
    hr = ITextDocument_Open(doc, &v, tomOpenAlways, CP_UTF8);
    check(hr == S_OK && exists_exclusive(path), "OpenAlways creates the file");
    new_window();
    DeleteFileW(path);
    hr = ITextDocument_Open(doc, &v, tomCreateNew, CP_ACP);
    check(hr == S_OK && exists_exclusive(path), "CreateNew creates the file");
    new_window();
    hr = ITextDocument_Open(doc, &v, tomCreateNew, CP_ACP);
    checkhr(hr, HRESULT_FROM_WIN32(ERROR_FILE_EXISTS), "CreateNew of an existing file");
    DeleteFileW(path);
    hr = ITextDocument_Open(doc, &v, tomOpenExisting, CP_ACP);
    checkhr(hr, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "OpenExisting of a missing file");
    new_window();
    hr = ITextDocument_Open(doc, &v, tomText, CP_ACP);
    check(hr == S_OK && exists_exclusive(path), "no disposition: the file is created, and not left open");
    DeleteFileW(path);

    /* sharing */
    write_file(path, "x", 0);
    new_window();
    hr = ITextDocument_Open(doc, &v, tomShareDenyRead, CP_ACP);
    check(hr == S_OK, "Open with ShareDenyRead");
    {
        HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        check(h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_SHARING_VIOLATION, "others cannot read it");
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    new_window();
    check(exists_exclusive(path), "and it is free again once the control is gone");
    hr = ITextDocument_Open(doc, &v, tomShareDenyWrite, CP_ACP);
    check(hr == S_OK, "Open with ShareDenyWrite");
    {
        HANDLE h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        check(h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_SHARING_VIOLATION, "others cannot write it");
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
        h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        check(h != INVALID_HANDLE_VALUE, "but may read it");
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    new_window();
    DeleteFileW(path);

    /* what is not a file name */
    new_window();
    VariantClear(&v);
    V_VT(&v) = VT_I4; V_I4(&v) = 3;
    checkhr(ITextDocument_Open(doc, &v, tomText, CP_ACP), E_INVALIDARG, "Open with a number");
    checkhr(ITextDocument_Open(doc, NULL, tomText, CP_ACP), E_INVALIDARG, "Open without a variant");
    VariantInit(&v);
    checkhr(ITextDocument_Open(doc, &v, tomText, CP_ACP), E_INVALIDARG, "Open with nothing");
}

static void test_content(void)
{
    static const char ansi[] = "TestSomeText";
    static const char utf8[] = "\xef\xbb\xbfTextWithUTF8BOM";
    static const WCHAR utf16[] = { 0xfeff, 'T', 'e', 's', 't', 'S', 'o', 'm', 'e', 'T', 'e', 'x', 't', 0 };
    static const WCHAR utf16be[] = { 0xfffe, 0x5400, 0x6500, 0x7300, 0x7400, 0 };
    static const char rtf[] = "{\\rtf1\\ansi RtfBody}";
    WCHAR path[MAX_PATH];
    VARIANT v;
    HRESULT hr;
    LONG saved;
    BSTR name;

    path_for(L"c.txt", path);
    v = name_var(path);

    write_file(path, ansi, sizeof(ansi) - 1);
    new_window();
    hr = ITextDocument_Open(doc, &v, tomReadOnly, CP_ACP);
    check(hr == S_OK && text_is(L"TestSomeText"), "ANSI text");
    write_file(path, utf8, sizeof(utf8) - 1);
    new_window();
    hr = ITextDocument_Open(doc, &v, tomReadOnly, CP_UTF8);
    check(hr == S_OK && text_is(L"TextWithUTF8BOM"), "UTF-8 with a byte order mark");
    write_file(path, "plain utf8 \xc3\xa9", 13);
    new_window();
    hr = ITextDocument_Open(doc, &v, tomReadOnly, CP_UTF8);
    check(hr == S_OK && text_is(L"plain utf8 \xe9"), "UTF-8 without one");
    write_file(path, utf16, sizeof(utf16) - sizeof(WCHAR));
    new_window();
    hr = ITextDocument_Open(doc, &v, tomReadOnly, 1200);
    check(hr == S_OK && text_is(L"TestSomeText"), "UTF-16 with a byte order mark");
    write_file(path, utf16be, sizeof(utf16be) - sizeof(WCHAR));
    new_window();
    hr = ITextDocument_Open(doc, &v, tomReadOnly, 1201);
    check(hr == S_OK && text_is(L"Test"), "UTF-16 big endian");
    write_file(path, rtf, sizeof(rtf) - 1);
    new_window();
    hr = ITextDocument_Open(doc, &v, tomRTF, CP_ACP);
    check(hr == S_OK && text_is(L"RtfBody"), "RTF by flag");
    new_window();
    hr = ITextDocument_Open(doc, &v, tomReadOnly, CP_ACP);
    check(hr == S_OK && text_is(L"RtfBody"), "RTF by its first bytes");

    /* pasting */
    write_file(path, "INS", 3);
    new_window();
    SendMessageW(edit, WM_SETTEXT, 0, (LPARAM)L"abcdef");
    SendMessageW(edit, EM_SETSEL, 3, 3);
    hr = ITextDocument_Open(doc, &v, tomReadOnly | tomPasteFile, CP_ACP);
    check(hr == S_OK && text_is(L"abcINSdef"), "a pasted file lands at the selection");

    /* name and saved state */
    hr = ITextDocument_GetName(doc, &name);
    check(hr == S_OK && name && !wcscmp(name, path), "GetName is the file name");
    SysFreeString(name);
    checkhr(ITextDocument_GetName(doc, NULL), E_INVALIDARG, "GetName without a pointer");
    saved = 99;
    hr = ITextDocument_GetSaved(doc, &saved);
    check(hr == S_OK && saved == tomTrue, "a document just opened is saved, pasted or not");
    new_window();
    ITextDocument_Open(doc, &v, tomReadOnly, CP_ACP);
    saved = 99;
    hr = ITextDocument_GetSaved(doc, &saved);
    check(hr == S_OK && saved == tomTrue, "just opened: saved");
    SendMessageW(edit, EM_REPLACESEL, FALSE, (LPARAM)L"z");
    ITextDocument_GetSaved(doc, &saved);
    check(saved == tomFalse, "an edit makes it not saved");
    checkhr(ITextDocument_SetSaved(doc, tomTrue), S_OK, "SetSaved(true)");
    ITextDocument_GetSaved(doc, &saved);
    check(saved == tomTrue, "saved again");
    checkhr(ITextDocument_SetSaved(doc, tomFalse), S_OK, "SetSaved(false)");
    ITextDocument_GetSaved(doc, &saved);
    check(saved == tomFalse, "not saved again");
    checkhr(ITextDocument_SetSaved(doc, 7), E_INVALIDARG, "SetSaved of a bad value");
    checkhr(ITextDocument_GetSaved(doc, NULL), E_INVALIDARG, "GetSaved without a pointer");

    /* New */
    checkhr(ITextDocument_New(doc), S_OK, "New");
    check(text_is(L""), "New empties the text");
    hr = ITextDocument_GetName(doc, &name);
    check(hr == S_OK && name && !*name, "and forgets the name");
    SysFreeString(name);
    ITextDocument_GetSaved(doc, &saved);
    check(saved == tomTrue, "and the new document is saved");
    DeleteFileW(path);
}

static void test_save(void)
{
    WCHAR path[MAX_PATH], path2[MAX_PATH];
    BYTE buf[600];
    VARIANT v, v2, empty;
    HRESULT hr;
    DWORD n;
    LONG saved;
    BSTR name;

    path_for(L"s1.txt", path);
    path_for(L"s2.txt", path2);
    DeleteFileW(path); DeleteFileW(path2);
    VariantInit(&empty);

    new_window();
    SendMessageW(edit, WM_SETTEXT, 0, (LPARAM)L"Caf\xe9 text");
    SendMessageW(edit, EM_REPLACESEL, FALSE, (LPARAM)L"!");
    checkhr(ITextDocument_Save(doc, &empty, 0, CP_ACP), E_FAIL, "Save of a document with no file");

    v = name_var(path);
    hr = ITextDocument_Save(doc, &v, tomCreateAlways, CP_UTF8);
    checkhr(hr, S_OK, "Save as UTF-8");
    n = read_file(path, buf, sizeof(buf));
    check(n == 11 && !memcmp(buf, "!Caf\xc3\xa9 text", 11), "UTF-8 text without a mark");
    ITextDocument_GetSaved(doc, &saved);
    check(saved == tomTrue, "saving makes the document saved");
    hr = ITextDocument_GetName(doc, &name);
    check(hr == S_OK && name && !wcscmp(name, path), "and names it");
    SysFreeString(name);

    /* the current file */
    SendMessageW(edit, WM_SETTEXT, 0, (LPARAM)L"second");
    checkhr(ITextDocument_Save(doc, &empty, 0, CP_ACP), S_OK, "Save to the current file");
    n = read_file(path, buf, sizeof(buf));
    check(n == 6 && !memcmp(buf, "second", 6), "the file was rewritten, shorter");

    v2 = name_var(path2);
    checkhr(ITextDocument_Save(doc, &v2, 0, 1200), S_OK, "Save as UTF-16");
    n = read_file(path2, buf, sizeof(buf));
    check(n == 14 && buf[0] == 0xff && buf[1] == 0xfe && !memcmp(buf + 2, L"second", 12), "UTF-16 with its mark");
    hr = ITextDocument_GetName(doc, &name);
    check(hr == S_OK && name && !wcscmp(name, path2), "the new name is the document's");
    SysFreeString(name);

    checkhr(ITextDocument_Save(doc, &v2, tomCreateNew, CP_ACP), HRESULT_FROM_WIN32(ERROR_FILE_EXISTS), "Save with CreateNew over a file");
    checkhr(ITextDocument_Save(doc, &v2, tomRTF, CP_ACP), S_OK, "Save as RTF");
    n = read_file(path2, buf, sizeof(buf) - 1);
    buf[n] = 0;
    check(n > 20 && !strncmp((char *)buf, "{\\rtf", 5) && strstr((char *)buf, "second"), "RTF with the text");

    /* a document held open can be saved */
    ITextDocument_New(doc);
    write_file(path, "held", 4);
    VariantClear(&v);
    v = name_var(path);
    checkhr(ITextDocument_Open(doc, &v, tomShareDenyRead | tomShareDenyWrite, CP_ACP), S_OK, "Open held");
    SendMessageW(edit, WM_SETTEXT, 0, (LPARAM)L"rewritten by save");
    checkhr(ITextDocument_Save(doc, &empty, 0, CP_ACP), S_OK, "Save through the held handle");
    new_window();
    n = read_file(path, buf, sizeof(buf));
    check(n == 17 && !memcmp(buf, "rewritten by save", 17), "the held file has the new text");

    checkhr(ITextDocument_Save(doc, &v, 0, CP_ACP), S_OK, "Save by name in a new window");
    VariantClear(&v);
    VariantClear(&v2);
    DeleteFileW(path); DeleteFileW(path2);
}

int main(void)
{
    HMODULE lib = LoadLibraryW(L"riched20.dll");

    CoInitialize(NULL);
    GetTempPathW(MAX_PATH, dir);
    if (!lib) { printf("FAIL  riched20 not loaded\nRESULT: FAIL\n"); return 1; }
    test_dispositions();
    test_content();
    test_save();
    if (doc) ITextDocument_Release(doc);
    if (ole) IUnknown_Release(ole);
    if (edit) DestroyWindow(edit);
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
