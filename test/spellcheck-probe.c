/* spellcheck-probe.c: the Spell Checking API (CLSID_SpellCheckerFactory, patches/sg/7010) over the
 * system's hunspell en_US dictionary. Prints key=value lines. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <spellcheck.h>
#include <stdio.h>

struct handler { ISpellCheckerChangedEventHandler iface; LONG calls; };
static HRESULT WINAPI h_qi(ISpellCheckerChangedEventHandler *i, REFIID iid, void **out)
{ *out = i; return S_OK; }
static ULONG WINAPI h_addref(ISpellCheckerChangedEventHandler *i) { return 2; }
static ULONG WINAPI h_release(ISpellCheckerChangedEventHandler *i) { return 1; }
static HRESULT WINAPI h_invoke(ISpellCheckerChangedEventHandler *i, ISpellChecker *sender)
{ ((struct handler *)i)->calls++; return S_OK; }
static ISpellCheckerChangedEventHandlerVtbl h_vtbl = { h_qi, h_addref, h_release, h_invoke };

/* errors as "start:len:action," */
static void errors(ISpellChecker *sc, const WCHAR *text, char *out, size_t size)
{
    IEnumSpellingError *e;
    ISpellingError *err;
    ULONG start, len;
    CORRECTIVE_ACTION act;
    *out = 0;
    if (FAILED(ISpellChecker_Check(sc, text, &e))) { snprintf(out, size, "failed"); return; }
    while (IEnumSpellingError_Next(e, &err) == S_OK)
    {
        ISpellingError_get_StartIndex(err, &start);
        ISpellingError_get_Length(err, &len);
        ISpellingError_get_CorrectiveAction(err, &act);
        snprintf(out + strlen(out), size - strlen(out), "%lu:%lu:%d,", start, len, act);
        ISpellingError_Release(err);
    }
    IEnumSpellingError_Release(e);
    if (!*out) snprintf(out, size, "none");
}

static int has_string(IEnumString *en, const WCHAR *want)
{
    LPOLESTR s; int found = 0;
    while (IEnumString_Next(en, 1, &s, NULL) == S_OK) { if (!lstrcmpW(s, want)) found = 1; CoTaskMemFree(s); }
    return found;
}

int main(int argc, char **argv)
{
    ISpellCheckerFactory *f;
    IUserDictionariesRegistrar *reg;
    ISpellChecker *sc, *sc2;
    IEnumString *en;
    struct handler h = { { &h_vtbl }, 0 };
    char buf[512];
    WCHAR path[MAX_PATH];
    LPWSTR tag;
    DWORD cookie;
    BOOL ok = FALSE;
    HRESULT hr;
    HANDLE file;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_SpellCheckerFactory, NULL, CLSCTX_INPROC_SERVER, &IID_ISpellCheckerFactory, (void **)&f);
    printf("created=%d\n", hr == S_OK);
    if (hr != S_OK) return 1;
    ISpellCheckerFactory_IsSupported(f, L"en-US", &ok);
    printf("supported=%d\n", ok);
    ok = TRUE;
    ISpellCheckerFactory_IsSupported(f, L"xx-XX", &ok);
    printf("unsupported=%d\n", !ok);
    if (SUCCEEDED(ISpellCheckerFactory_get_SupportedLanguages(f, &en)))
    { printf("languages=%d\n", has_string(en, L"en-US")); IEnumString_Release(en); }
    ok = FALSE;
    ISpellCheckerFactory_IsSupported(f, L"en", &ok);
    hr = ok ? ISpellCheckerFactory_CreateSpellChecker(f, L"en", &sc) : E_FAIL;
    if (hr == S_OK) { ISpellChecker_get_LanguageTag(sc, &tag); ok = !lstrcmpW(tag, L"en-US"); CoTaskMemFree(tag); ISpellChecker_Release(sc); }
    printf("bare_language=%d\n", hr == S_OK && ok);
    hr = ISpellCheckerFactory_CreateSpellChecker(f, L"xx-XX", &sc);
    printf("create_unsupported=%d\n", hr == E_INVALIDARG);
    hr = ISpellCheckerFactory_CreateSpellChecker(f, L"en-us", &sc);
    printf("checker=%d\n", hr == S_OK);
    if (hr != S_OK) return 1;
    ISpellChecker_get_LanguageTag(sc, &tag);
    printf("tag=%d\n", !lstrcmpW(tag, L"en-US"));
    CoTaskMemFree(tag);

    /* 0123456789012345678901234567890123456789 */
    errors(sc, L"Ths is a tset of the the spelling, yes?", buf, sizeof(buf));
    printf("check=%d check_errors=%s\n", !strcmp(buf, "0:3:1,9:4:1,21:3:3,"), buf);
    errors(sc, L"Mail me at someone@example.org or see http://exampl.test/pth, 42abc", buf, sizeof(buf));
    printf("names_skipped=%d names_errors=%s\n", !strcmp(buf, "none"), buf);
    errors(sc, L"It\x2019s fine, isn't it", buf, sizeof(buf));
    printf("apostrophes=%d apostrophe_errors=%s\n", !strcmp(buf, "none"), buf);

    hr = ISpellChecker_Suggest(sc, L"speling", &en);
    printf("suggest=%d\n", hr == S_OK && has_string(en, L"spelling"));
    if (SUCCEEDED(hr)) IEnumString_Release(en);
    hr = ISpellChecker_Suggest(sc, L"hello", &en);
    printf("suggest_right=%d\n", hr == S_FALSE && has_string(en, L"hello"));
    if (SUCCEEDED(hr)) IEnumString_Release(en);

    ISpellChecker_add_SpellCheckerChanged(sc, &h.iface, &cookie);
    ISpellChecker_Ignore(sc, L"Ths");
    errors(sc, L"Ths", buf, sizeof(buf));
    printf("ignored=%d\n", !strcmp(buf, "none"));
    ISpellChecker_Add(sc, L"Zorblaxian");
    errors(sc, L"Zorblaxian", buf, sizeof(buf));
    printf("added=%d events=%d\n", !strcmp(buf, "none"), h.calls == 2);
    ISpellChecker_remove_SpellCheckerChanged(sc, cookie);
    ISpellChecker_AutoCorrect(sc, L"teh", L"the");
    errors(sc, L"teh", buf, sizeof(buf));
    printf("autocorrect=%d\n", !strcmp(buf, "0:3:2,"));

    /* a new checker reads the user's word list */
    ISpellCheckerFactory_CreateSpellChecker(f, L"en-US", &sc2);
    errors(sc2, L"Zorblaxian Ths", buf, sizeof(buf));
    printf("persisted=%d persisted_errors=%s\n", !strcmp(buf, "11:3:1,"), buf);
    ISpellChecker_Release(sc2);
    ISpellChecker2_Remove((ISpellChecker2 *)sc, L"Zorblaxian");
    errors(sc, L"Zorblaxian", buf, sizeof(buf));
    printf("removed=%d\n", !strcmp(buf, "0:10:1,"));

    /* a registered exclusion list */
    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"sgprobe.exc");
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(file, "\xff\xfeh\0o\0u\0s\0e\0\r\0\n\0", 16, &cookie, NULL);
    CloseHandle(file);
    ISpellCheckerFactory_QueryInterface(f, &IID_IUserDictionariesRegistrar, (void **)&reg);
    printf("registered=%d\n", IUserDictionariesRegistrar_RegisterUserDictionary(reg, path, L"en-US") == S_OK);
    ISpellCheckerFactory_CreateSpellChecker(f, L"en-US", &sc2);
    errors(sc2, L"house boat", buf, sizeof(buf));
    printf("excluded=%d\n", !strcmp(buf, "0:5:1,"));
    ISpellChecker_Release(sc2);
    IUserDictionariesRegistrar_Release(reg);

    ISpellChecker_Release(sc);
    ISpellCheckerFactory_Release(f);
    printf("done=1\n");
    return 0;
}
