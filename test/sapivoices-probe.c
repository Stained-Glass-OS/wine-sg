/* sapivoices-probe: a voice per language, each speaking its own (patches/sg/9418).
 *
 * There was one voice, English: a program choosing its voice by language (SAPI's
 * "Language=407", Windows.Media.SpeechSynthesis' AllVoices) found none for German, and German
 * text through it came out in English sounds. Now each language eSpeak NG speaks has its voice
 * token (Language, Name, Gender attributes), the engine speaks the token's language for text
 * that names none, and with no choice of the user's the default voice is the display language's.
 *
 *   sapivoices-probe            the voices and what they say
 *   sapivoices-probe default    the default voice (the gate runs it under two display languages)
 *
 * Prints name=value lines; see test/sapivoices-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <sapi.h>
#include <shlwapi.h>
#include <stdio.h>

DEFINE_GUID(p_SPDFID_WaveFormatEx, 0xc31adbae, 0x527f, 0x4ff5, 0xa2, 0x30, 0xf6, 0x2b, 0xb6, 0x1f, 0xf7, 0x0c);

static void say(const char *name, int ok, const char *fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s=%d %s\n", name, ok ? 1 : 0, buf);
    fflush(stdout);
}

static ISpObjectTokenCategory *voices(void)
{
    ISpObjectTokenCategory *cat;
    if (FAILED(CoCreateInstance(&CLSID_SpObjectTokenCategory, NULL, CLSCTX_INPROC_SERVER,
            &IID_ISpObjectTokenCategory, (void **)&cat))) return NULL;
    if (FAILED(ISpObjectTokenCategory_SetId(cat, SPCAT_VOICES, FALSE))) { ISpObjectTokenCategory_Release(cat); return NULL; }
    return cat;
}

static ISpObjectToken *voice_for(const WCHAR *attrs, ULONG *count)
{
    ISpObjectTokenCategory *cat = voices();
    IEnumSpObjectTokens *e;
    ISpObjectToken *token = NULL;
    *count = 0;
    if (!cat) return NULL;
    if (SUCCEEDED(ISpObjectTokenCategory_EnumTokens(cat, attrs, NULL, &e)))
    {
        IEnumSpObjectTokens_GetCount(e, count);
        if (*count) IEnumSpObjectTokens_Item(e, 0, &token);
        IEnumSpObjectTokens_Release(e);
    }
    ISpObjectTokenCategory_Release(cat);
    return token;
}

static void attr(ISpObjectToken *token, const WCHAR *name, WCHAR *out, int cch)
{
    ISpDataKey *key;
    WCHAR *v;
    out[0] = 0;
    if (FAILED(ISpObjectToken_OpenKey(token, L"Attributes", &key))) return;
    if (SUCCEEDED(ISpDataKey_GetStringValue(key, name, &v))) { lstrcpynW(out, v, cch); CoTaskMemFree(v); }
    ISpDataKey_Release(key);
}

/* what VOICE says for TEXT, as 16 kHz 16-bit mono PCM: its byte count and an FNV hash */
static HRESULT speak(ISpObjectToken *token, const WCHAR *text, DWORD flags, ULONG *bytes, DWORD *hash)
{
    WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, 16000, 32000, 2, 16, 0 };
    ISpVoice *voice;
    ISpStream *spstream;
    IStream *mem;
    STATSTG st;
    LARGE_INTEGER zero = {{0}};
    HRESULT hr;
    BYTE *data;
    ULONG i, got;

    *bytes = 0; *hash = 0;
    if (FAILED(hr = CoCreateInstance(&CLSID_SpVoice, NULL, CLSCTX_INPROC_SERVER, &IID_ISpVoice, (void **)&voice))) return hr;
    CoCreateInstance(&CLSID_SpStream, NULL, CLSCTX_INPROC_SERVER, &IID_ISpStream, (void **)&spstream);
    mem = SHCreateMemStream(NULL, 0);
    ISpStream_SetBaseStream(spstream, mem, &p_SPDFID_WaveFormatEx, &wfx);
    if (token) ISpVoice_SetVoice(voice, token);
    ISpVoice_SetOutput(voice, (IUnknown *)spstream, TRUE);
    hr = ISpVoice_Speak(voice, text, flags, NULL);
    ISpVoice_WaitUntilDone(voice, 30000);
    IStream_Stat(mem, &st, STATFLAG_NONAME);
    *bytes = st.cbSize.LowPart;
    if (*bytes && (data = malloc(*bytes)))
    {
        IStream_Seek(mem, zero, STREAM_SEEK_SET, NULL);
        IStream_Read(mem, data, *bytes, &got);
        *hash = 2166136261u;
        for (i = 0; i < got; i++) *hash = (*hash ^ data[i]) * 16777619u;
        free(data);
    }
    ISpVoice_Release(voice);
    ISpStream_Release(spstream);
    IStream_Release(mem);
    return hr;
}

int main(int argc, char **argv)
{
    static const WCHAR german[] = L"Guten Tag. Heute ist ein sch\x00f6ner Tag, und wir sprechen Deutsch.";
    static const WCHAR german_xml[] = L"<lang langid=\"407\">Guten Tag. Heute ist ein sch\x00f6ner Tag, und wir sprechen Deutsch.</lang>";
    ISpObjectToken *de, *en, *fr;
    WCHAR lang[64], name[256];
    ULONG count, b1, b2, b3;
    DWORD h1, h2, h3;
    HRESULT hr;

    CoInitialize(NULL);
    if (argc > 1 && !strcmp(argv[1], "default"))
    {
        ISpObjectTokenCategory *cat = voices();
        WCHAR *id = NULL;
        hr = cat ? ISpObjectTokenCategory_GetDefaultTokenId(cat, &id) : E_FAIL;
        printf("default_id=%ls (%#lx, display language %04x)\n", id ? id : L"", hr, GetUserDefaultUILanguage());
        fflush(stdout);
        TerminateProcess(GetCurrentProcess(), 0);
    }

    de = voice_for(L"Language=407", &count);
    if (de) { attr(de, L"Language", lang, 64); attr(de, L"Name", name, 256); }
    say("german_voice", de && !wcsicmp(lang, L"407") && wcsstr(name, L"German"), "%lu voice(s) for Language=407: %ls (%ls)",
            count, de ? name : L"none", de ? lang : L"");
    fr = voice_for(L"Language=40C", &count);
    say("french_voice", fr != NULL, "%lu voice(s) for Language=40C", count);
    en = voice_for(L"Language=409", &count);
    say("english_voice", en != NULL, "%lu voice(s) for Language=409", count);
    {
        IEnumSpObjectTokens *e;
        ISpObjectTokenCategory *cat = voices();
        ULONG all = 0;
        if (cat && SUCCEEDED(ISpObjectTokenCategory_EnumTokens(cat, NULL, NULL, &e)))
        {
            IEnumSpObjectTokens_GetCount(e, &all);
            IEnumSpObjectTokens_Release(e);
        }
        say("voice_count", all >= 20, "%lu voices installed", all);
    }
    if (!de || !en) goto done;

    hr = speak(de, german, SPF_DEFAULT, &b1, &h1);
    speak(en, german_xml, SPF_IS_XML, &b2, &h2);
    speak(en, german, SPF_DEFAULT, &b3, &h3);
    say("speaks", SUCCEEDED(hr) && b1 > 16000, "the German voice: %#lx, %lu bytes", hr, b1);
    /* the same voice of the synthesizer both ways: within 1% (the XML's fragments pause a little
     * differently); the English voice's reading of German is several percent longer */
    say("own_language", b1 && labs((long)b1 - (long)b2) * 100 < (long)b1,
            "plain text through the German voice ~ <lang langid=407> through the English one (%lu vs %lu bytes)", b1, b2);
    say("not_english", h1 != h3 && labs((long)b1 - (long)b3) * 100 > 3 * (long)b1,
            "and not the English voice's sounds (%lu vs %lu bytes)", b1, b3);
done:
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), 0);
    return 0;
}
