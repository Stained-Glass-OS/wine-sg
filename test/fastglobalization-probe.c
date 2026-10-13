/* windows.globalization: GeographicRegion, Language names, preferences, IInspectable answers */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>

typedef void *HS;
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define VT(obj) (*(void ***)(obj))
#define CALL(obj, idx, ret, ...) (((ret (WINAPI *)(void *, ...))VT(obj)[idx])(obj, ##__VA_ARGS__))

typedef HRESULT (WINAPI *WCS)(const WCHAR *, UINT32, HS *);
typedef HRESULT (WINAPI *RGAF)(HS, const GUID *, void **);
typedef const WCHAR *(WINAPI *WGSRB)(HS, UINT32 *);
static WCS create;
static WGSRB raw;
static RGAF get_factory;

#define G(name, a, b, c, d0,d1,d2,d3,d4,d5,d6,d7) static const GUID name = {a, b, c, {d0,d1,d2,d3,d4,d5,d6,d7}}
G(IID_IGeographicRegionFactory_, 0x53425270, 0x77b4, 0x426b, 0x85,0x9f,0x81,0xe1,0x9d,0x51,0x25,0x46);
G(IID_IGeographicRegion_, 0x01e9a621, 0x4a64, 0x4ed9, 0x95,0x4f,0x9e,0xde,0xb0,0x7b,0xd9,0x03);
G(IID_ILanguageFactory_, 0x9b0252ac, 0x0c27, 0x44f8, 0xb7,0x92,0x97,0x93,0xfb,0x66,0xc6,0x3e);
G(IID_IGlobalizationPreferencesStatics_, 0x01bf4326, 0xed37, 0x4e96, 0xb0,0xe9,0xc1,0x34,0x0d,0x1e,0xa1,0x58);

static HS mk(const WCHAR *s) { HS h = NULL; create(s, wcslen(s), &h); return h; }
static const WCHAR *str(HS h) { return h ? raw(h, NULL) : L"(null)"; }

static void check_name(void *obj, int slot, const WCHAR *want, const char *what)
{
    HS s = NULL;
    HRESULT hr = CALL(obj, slot, HRESULT, &s);
    CHECK(hr == S_OK && !wcscmp(str(s), want), "%s: %lx %ls (want %ls)", what, hr, str(s), want);
}

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    HRESULT (WINAPI *ro_init)(int) = (void *)GetProcAddress(combase, "RoInitialize");
    void *factory = NULL, *region = NULL, *lang_factory = NULL, *language = NULL, *prefs = NULL, *vec = NULL;
    HS s = NULL;
    HRESULT hr;
    UINT32 n, count;
    ULONG iid_count = 0;
    GUID *iids = NULL;

    setvbuf(stdout, NULL, _IONBF, 0);
    create = (WCS)GetProcAddress(combase, "WindowsCreateString");
    raw = (WGSRB)GetProcAddress(combase, "WindowsGetStringRawBuffer");
    get_factory = (RGAF)GetProcAddress(combase, "RoGetActivationFactory");
    ro_init(1);

    /* GeographicRegion */
    hr = get_factory(mk(L"Windows.Globalization.GeographicRegion"), &IID_IGeographicRegionFactory_, &factory);
    CHECK(hr == S_OK && factory, "region factory %lx", hr);
    if (factory)
    {
        hr = CALL(factory, 6, HRESULT, mk(L"us"), &region);
        CHECK(hr == S_OK && region, "CreateGeographicRegion(us) %lx", hr);
        if (region)
        {
            HS name = NULL;
            check_name(region, 6, L"US", "Code");
            check_name(region, 7, L"US", "CodeTwoLetter");
            check_name(region, 8, L"USA", "CodeThreeLetter");
            check_name(region, 9, L"840", "CodeThreeDigit");
            check_name(region, 10, L"United States", "DisplayName");
            hr = CALL(region, 12, HRESULT, &vec);
            CHECK(hr == S_OK && vec, "CurrenciesInUse %lx", hr);
            if (vec)
            {
                n = 0; CALL(vec, 7, HRESULT, &n);
                s = NULL; CALL(vec, 6, HRESULT, 0, &s);
                CHECK(n == 1 && !wcscmp(str(s), L"USD"), "currencies %u %ls", n, str(s));
                CALL(vec, 2, ULONG);
            }
            hr = CALL(region, 3, HRESULT, &iid_count, &iids);
            CHECK(hr == S_OK && iid_count == 1 && IsEqualGUID(&iids[0], &IID_IGeographicRegion_), "GetIids %lx %lu", hr, iid_count);
            CoTaskMemFree(iids);
            hr = CALL(region, 4, HRESULT, &name);
            CHECK(hr == S_OK && !wcscmp(str(name), L"Windows.Globalization.GeographicRegion"), "class name %ls", str(name));
            CALL(region, 2, ULONG);
        }
        region = NULL;
        hr = CALL(factory, 6, HRESULT, mk(L"DEU"), &region);
        CHECK(hr == S_OK && region, "CreateGeographicRegion(DEU) %lx", hr);
        if (region)
        {
            check_name(region, 7, L"DE", "DEU CodeTwoLetter");
            check_name(region, 9, L"276", "DEU CodeThreeDigit");
            check_name(region, 11, L"Deutschland", "NativeName");
            CALL(region, 2, ULONG);
        }
        region = NULL;
        hr = CALL(factory, 6, HRESULT, mk(L"276"), &region);
        CHECK(hr == S_OK && region, "CreateGeographicRegion(276) %lx", hr);
        if (region) { check_name(region, 7, L"DE", "276 CodeTwoLetter"); CALL(region, 2, ULONG); }
        region = NULL;
        hr = CALL(factory, 6, HRESULT, mk(L"QQ"), &region);
        CHECK(hr == E_INVALIDARG && !region, "invalid region %lx", hr);
        hr = CALL(factory, 6, HRESULT, mk(L"Z"), &region);
        CHECK(hr == E_INVALIDARG, "invalid short region %lx", hr);
    }

    /* Language */
    hr = get_factory(mk(L"Windows.Globalization.Language"), &IID_ILanguageFactory_, &lang_factory);
    CHECK(hr == S_OK && lang_factory, "language factory %lx", hr);
    if (lang_factory)
    {
        hr = CALL(lang_factory, 6, HRESULT, mk(L"de-DE"), &language);
        CHECK(hr == S_OK && language, "CreateLanguage %lx", hr);
        if (language)
        {
            check_name(language, 6, L"de-DE", "LanguageTag");
            check_name(language, 8, L"Deutsch (Deutschland)", "NativeName");
            check_name(language, 9, L"Latn", "Script");
            s = NULL;
            hr = CALL(language, 7, HRESULT, &s);
            CHECK(hr == S_OK && wcsstr(str(s), L"German"), "DisplayName %lx %ls", hr, str(s));
            CALL(language, 2, ULONG);
        }
        language = NULL;
        hr = CALL(lang_factory, 6, HRESULT, mk(L"zh-Hans-CN"), &language);
        if (language) { check_name(language, 9, L"Hans", "zh Script"); CALL(language, 2, ULONG); }
    }

    /* preferences */
    hr = get_factory(mk(L"Windows.System.UserProfile.GlobalizationPreferences"), &IID_IGlobalizationPreferencesStatics_, &prefs);
    CHECK(hr == S_OK && prefs, "preferences %lx", hr);
    if (prefs)
    {
        int slot;
        const WCHAR *want[3] = { L"GregorianCalendar", L"12HourClock", L"USD" };
        UINT32 day = 99;

        for (slot = 6; slot <= 8; slot++)
        {
            vec = NULL;
            hr = CALL(prefs, slot, HRESULT, &vec);
            CHECK(hr == S_OK && vec, "preferences slot %d %lx", slot, hr);
            if (!vec) continue;
            n = 0; CALL(vec, 7, HRESULT, &n);
            s = NULL; CALL(vec, 6, HRESULT, 0, &s);
            CHECK(n == 1 && !wcscmp(str(s), want[slot - 6]), "slot %d: %u %ls", slot, n, str(s));
            hr = CALL(vec, 3, HRESULT, &iid_count, &iids);
            CHECK(hr == S_OK && iid_count == 2, "vector GetIids %lx %lu", hr, iid_count);
            CoTaskMemFree(iids);
            CALL(vec, 2, ULONG);
        }
        hr = CALL(prefs, 11, HRESULT, &day);
        CHECK(hr == S_OK && day == 0, "WeekStartsOn %lx %u", hr, day);
        count = 0;
        hr = CALL(prefs, 3, HRESULT, &iid_count, &iids);
        CHECK(hr == S_OK && iid_count == 1 && IsEqualGUID(&iids[0], &IID_IGlobalizationPreferencesStatics_), "statics GetIids %lx", hr);
        CoTaskMemFree(iids);
    }
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails;
}
