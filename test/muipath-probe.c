/* GetFileMUIPath finds a file's MUI resource file (patches/sg/1613); it
 * failed with ERROR_CALL_NOT_IMPLEMENTED. */
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static void touch(const WCHAR *path)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(h);
}

int main(void)
{
    WCHAR base[MAX_PATH], file[MAX_PATH], p[MAX_PATH], lang[LOCALE_NAME_MAX_LENGTH], path[MAX_PATH];
    ULONG langlen, pathlen;
    ULONGLONG en = 0;
    BOOL ok;

    GetTempPathW(MAX_PATH, base);
    wcscat(base, L"muiprobe");
    CreateDirectoryW(base, NULL);
    swprintf(file, MAX_PATH, L"%ls\\app.dll", base);
    touch(file);
    swprintf(p, MAX_PATH, L"%ls\\en-US", base); CreateDirectoryW(p, NULL);
    swprintf(p, MAX_PATH, L"%ls\\en-US\\app.dll.mui", base); touch(p);
    swprintf(p, MAX_PATH, L"%ls\\de-DE", base); CreateDirectoryW(p, NULL);
    swprintf(p, MAX_PATH, L"%ls\\de-DE\\app.dll.mui", base); touch(p);

    lang[0] = 0; langlen = ARRAYSIZE(lang); pathlen = ARRAYSIZE(path);
    SetLastError(0xdeadbeef);
    ok = GetFileMUIPath(MUI_LANGUAGE_NAME, file, lang, &langlen, path, &pathlen, NULL);
    printf("preferred: ok %d err %lu lang %ls path %ls\n", ok, ok ? 0 : GetLastError(), ok ? lang : L"", ok ? path : L"");
    check(ok && !wcscmp(lang, L"en-US") && wcsstr(path, L"\\en-US\\app.dll.mui"), "the preferred UI language's MUI file");
    check(ok && langlen == 6 && pathlen == wcslen(path) + 1, "... with the lengths, terminators included");

    wcscpy(lang, L"de-DE"); langlen = ARRAYSIZE(lang); pathlen = ARRAYSIZE(path);
    ok = GetFileMUIPath(MUI_LANGUAGE_NAME, file, lang, &langlen, path, &pathlen, NULL);
    check(ok && wcsstr(path, L"\\de-DE\\app.dll.mui"), "the language asked for");

    wcscpy(lang, L"0407"); langlen = ARRAYSIZE(lang); pathlen = ARRAYSIZE(path);
    ok = GetFileMUIPath(MUI_LANGUAGE_ID, file, lang, &langlen, path, &pathlen, NULL);
    printf("by id: ok %d lang %ls\n", ok, ok ? lang : L"");
    check(ok && wcsstr(path, L"\\de-DE\\app.dll.mui") && !wcscmp(lang, L"0407"), "... by identifier");

    wcscpy(lang, L"fr-FR"); langlen = ARRAYSIZE(lang); pathlen = ARRAYSIZE(path);
    SetLastError(0xdeadbeef);
    ok = GetFileMUIPath(MUI_LANGUAGE_NAME, file, lang, &langlen, path, &pathlen, NULL);
    check(!ok && GetLastError() == ERROR_FILE_NOT_FOUND, "a language without its file: ERROR_FILE_NOT_FOUND");

    lang[0] = 0; langlen = 2; pathlen = 2;
    SetLastError(0xdeadbeef);
    ok = GetFileMUIPath(MUI_LANGUAGE_NAME, file, lang, &langlen, path, &pathlen, NULL);
    check(!ok && GetLastError() == ERROR_INSUFFICIENT_BUFFER && langlen == 6 && pathlen > 20,
          "small buffers: ERROR_INSUFFICIENT_BUFFER and the lengths needed");

    /* every language, one per call */
    lang[0] = 0; langlen = ARRAYSIZE(lang); pathlen = ARRAYSIZE(path);
    ok = GetFileMUIPath(MUI_LANGUAGE_NAME | MUI_USE_SEARCH_ALL_LANGUAGES, file, lang, &langlen, path, &pathlen, &en);
    printf("all 1: ok %d lang %ls\n", ok, ok ? lang : L"");
    check(ok, "MUI_USE_SEARCH_ALL_LANGUAGES: the first language");
    lang[0] = 0; langlen = ARRAYSIZE(lang); pathlen = ARRAYSIZE(path);
    ok = GetFileMUIPath(MUI_LANGUAGE_NAME | MUI_USE_SEARCH_ALL_LANGUAGES, file, lang, &langlen, path, &pathlen, &en);
    printf("all 2: ok %d lang %ls\n", ok, ok ? lang : L"");
    check(ok, "... the second");
    lang[0] = 0; langlen = ARRAYSIZE(lang); pathlen = ARRAYSIZE(path);
    SetLastError(0xdeadbeef);
    ok = GetFileMUIPath(MUI_LANGUAGE_NAME | MUI_USE_SEARCH_ALL_LANGUAGES, file, lang, &langlen, path, &pathlen, &en);
    check(!ok && GetLastError() == ERROR_NO_MORE_FILES, "... then ERROR_NO_MORE_FILES");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
