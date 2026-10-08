/* Preferred UI languages are kept and used (patches/sg/1644).
 *
 * Set{Process,Thread}PreferredUILanguages kept nothing and every Get* call
 * returned the UI language ("returning a dummy value"); GetThreadUILanguage
 * and SetThreadUILanguage were stubs. Now the thread's and process's lists
 * are kept, the merge flags build the list the resource loader uses, the
 * user's list comes from the MUI override, the language list or the Linux
 * LANGUAGE list, and resources are loaded in the thread's or process's
 * preferred language (LoadString, FindResource).
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

/* a double-null list as "a,b,c" */
static void flat(const WCHAR *list, char *out)
{
    out[0] = 0;
    for (; *list; list += wcslen(list) + 1)
    {
        if (out[0]) strcat(out, ",");
        WideCharToMultiByte(CP_ACP, 0, list, -1, out + strlen(out), 64, NULL, NULL);
    }
}

typedef BOOL (WINAPI *get_t)(DWORD, ULONG *, WCHAR *, ULONG *);
typedef BOOL (WINAPI *set_t)(DWORD, const WCHAR *, ULONG *);

static get_t pGetThread, pGetProcess, pGetUser;
static set_t pSetThread, pSetProcess;

static const char *get(get_t f, DWORD flags, ULONG *count)
{
    static char out[512];
    WCHAR buf[256];
    ULONG size = 256;
    *count = 0xdead;
    if (!f(flags, count, buf, &size)) { sprintf(out, "error %lu", GetLastError()); return out; }
    flat(buf, out);
    return out;
}

static const char *string1(void)
{
    static char out[64];
    WCHAR buf[64];
    if (!LoadStringW(GetModuleHandleW(NULL), 1, buf, 64)) return "(none)";
    WideCharToMultiByte(CP_ACP, 0, buf, -1, out, 64, NULL, NULL);
    return out;
}

static DWORD WINAPI other_thread(void *arg)
{
    ULONG count;
    strcpy(arg, get(pGetThread, MUI_LANGUAGE_NAME, &count));
    return 0;
}

int main(int argc, char **argv)
{
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    char other[512];
    const char *s;
    ULONG count, size;
    WCHAR buf[64];
    HANDLE thread;
    HKEY key;
    BOOL ok;

    pGetThread  = (void *)GetProcAddress(k32, "GetThreadPreferredUILanguages");
    pGetProcess = (void *)GetProcAddress(k32, "GetProcessPreferredUILanguages");
    pGetUser    = (void *)GetProcAddress(k32, "GetUserPreferredUILanguages");
    pSetThread  = (void *)GetProcAddress(k32, "SetThreadPreferredUILanguages");
    pSetProcess = (void *)GetProcAddress(k32, "SetProcessPreferredUILanguages");

    if (argc > 1 && !strcmp(argv[1], "user"))
    {
        /* the user's list, as the gate set it up */
        s = get(pGetUser, MUI_LANGUAGE_NAME, &count);
        printf("user: %s\n", s);
        check(argc > 2 && !strcmp(s, argv[2]), "GetUserPreferredUILanguages: the expected list");
        printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
        return failures != 0;
    }

    s = get(pGetUser, MUI_LANGUAGE_NAME, &count);
    printf("user: %s\n", s);
    check(!strcmp(s, "en-US"), "the user's list is the UI language (en-US here)");
    check(!strcmp(string1(), "Hello"), "a string loads in the user's language");

    size = 0;
    ok = pGetProcess(MUI_LANGUAGE_NAME, &count, NULL, &size);
    check(ok && count == 0 && size == 2, "no process languages at first: an empty list (count 0, size 2)");

    ok = pSetProcess(MUI_LANGUAGE_NAME, L"de-DE\0fr-FR\0", &count);
    check(ok && count == 2, "SetProcessPreferredUILanguages keeps two");
    s = get(pGetProcess, MUI_LANGUAGE_NAME, &count);
    check(!strcmp(s, "de-DE,fr-FR") && count == 2, "GetProcessPreferredUILanguages reads them back");
    s = get(pGetProcess, MUI_LANGUAGE_ID, &count);
    check(!strcmp(s, "0407,040C"), "... also as ids");
    s = get(pGetThread, MUI_LANGUAGE_NAME, &count);
    check(!strcmp(s, "de-DE,fr-FR"), "a thread without its own sees the process's");
    check(!strcmp(string1(), "Hallo"), "a string loads in the process's preferred language");

    ok = pSetThread(MUI_LANGUAGE_ID, L"0411\0", &count);
    check(ok && count == 1, "SetThreadPreferredUILanguages(ja-JP)");
    s = get(pGetThread, MUI_LANGUAGE_NAME | MUI_THREAD_LANGUAGES, &count);
    check(!strcmp(s, "ja-JP"), "MUI_THREAD_LANGUAGES: the thread's alone");
    s = get(pGetThread, MUI_LANGUAGE_NAME, &count);
    check(!strcmp(s, "ja-JP"), "no merge flags: the first list set (the thread's)");
    s = get(pGetThread, MUI_LANGUAGE_NAME | MUI_MERGE_USER_FALLBACK, &count);
    printf("merged: %s\n", s);
    check(!strncmp(s, "ja-JP,de-DE,fr-FR,en-US", 23), "MUI_MERGE_USER_FALLBACK: thread, process, user, system");
    s = get(pGetThread, MUI_LANGUAGE_NAME | MUI_MERGE_USER_FALLBACK | MUI_MERGE_SYSTEM_FALLBACK, &count);
    printf("with fallbacks: %s\n", s);
    check(!strncmp(s, "ja-JP,ja,de-DE,de,fr-FR,fr,en-US,en", 35), "MUI_UI_FALLBACK adds the neutral languages");
    check(!strcmp(string1(), "Konnichiwa"), "a string loads in the thread's preferred language");
    check(GetThreadUILanguage() == 0x0411, "GetThreadUILanguage: the thread's first");

    thread = CreateThread(NULL, 0, other_thread, other, 0, NULL);
    WaitForSingleObject(thread, 5000);
    check(!strcmp(other, "de-DE,fr-FR"), "another thread does not see this thread's list");

    check(SetThreadUILanguage(0x040c) == 0x040c && GetThreadUILanguage() == 0x040c, "SetThreadUILanguage sets the thread's");
    s = get(pGetThread, MUI_LANGUAGE_NAME | MUI_THREAD_LANGUAGES, &count);
    check(!strcmp(s, "fr-FR"), "... as its preferred list");
    check(!strcmp(string1(), "Hallo"), "fr-FR has no strings: the process's de-DE next");

    ok = pSetThread(0, NULL, &count);
    check(ok && count == 0, "a NULL list clears the thread's");
    ok = pSetProcess(MUI_LANGUAGE_NAME, NULL, &count);
    check(ok && count == 0, "... and the process's");
    s = get(pGetThread, MUI_LANGUAGE_NAME, &count);
    check(!strcmp(s, "en-US"), "then the user's list again");
    check(!strcmp(string1(), "Hello"), "and the user's strings");

    SetLastError(0xdeadbeef);
    ok = pSetProcess(MUI_LANGUAGE_NAME, L"en-US\0de-DE\0fr-FR\0it-IT\0es-ES\0ja-JP\0", &count);
    check(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "six languages are too many");
    SetLastError(0xdeadbeef);
    ok = pSetProcess(MUI_LANGUAGE_NAME, L"xx-NOPE\0", &count);
    check(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "an unknown language is refused");
    SetLastError(0xdeadbeef);
    ok = pSetProcess(0, L"de-DE\0", &count);
    check(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "the format must be given");

    /* the MUI override in the registry */
    RegCreateKeyA(HKEY_CURRENT_USER, "Control Panel\\Desktop", &key);
    RegSetValueExW(key, L"PreferredUILanguages", 0, REG_MULTI_SZ, (const BYTE *)L"ja-JP\0", 14);
    s = get(pGetUser, MUI_LANGUAGE_NAME, &count);
    check(!strcmp(s, "ja-JP"), "the PreferredUILanguages override is the user's list");
    check(!strcmp(string1(), "Hello") || !strcmp(string1(), "Konnichiwa"), "(strings: the UI language or the override)");
    RegDeleteValueW(key, L"PreferredUILanguages");
    RegCloseKey(key);

    memset(buf, 0, sizeof(buf));
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
