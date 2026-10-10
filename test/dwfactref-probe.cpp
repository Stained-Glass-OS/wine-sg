/* dwrite (patches/sg/2665): the factory's reference count. The system collection takes a reference of it, a
 * custom collection does not, and neither does a font fallback beyond its system collection (the values the
 * Wine tests record from native). C++: mingw's dwrite.h is. */
#include <windows.h>
#include <dwrite_2.h>
#include <stdio.h>
#include <stdarg.h>

static int failures;

static void check(int ok, const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s  %s\n", ok ? "PASS" : "FAIL", buf);
    if (!ok) failures++;
}

static ULONG refs(IUnknown *o) { o->AddRef(); return o->Release(); }

struct Enumerator : IDWriteFontFileEnumerator
{
    LONG ref;
    Enumerator() : ref(1) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out) override
    {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteFontFileEnumerator)) { *out = this; AddRef(); return S_OK; }
        *out = NULL; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r = InterlockedDecrement(&ref); if (!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE MoveNext(BOOL *has) override { *has = FALSE; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetCurrentFontFile(IDWriteFontFile **file) override { *file = NULL; return E_FAIL; }
};

struct Loader : IDWriteFontCollectionLoader
{
    LONG ref;
    Loader() : ref(1) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out) override
    {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteFontCollectionLoader)) { *out = this; AddRef(); return S_OK; }
        *out = NULL; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r = InterlockedDecrement(&ref); if (!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE CreateEnumeratorFromKey(IDWriteFactory *, const void *, UINT32, IDWriteFontFileEnumerator **e) override
    {
        *e = new Enumerator;
        return S_OK;
    }
};

int main()
{
    IDWriteFactory2 *factory = NULL;
    IDWriteFontFallbackBuilder *builder = NULL;
    IDWriteFontFallback *fallback = NULL;
    IDWriteFontCollection *collection = NULL, *custom = NULL;
    Loader *loader = new Loader;
    HRESULT hr;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory2), (IUnknown **)&factory);
    check(hr == S_OK && factory, "factory (%#lx)", hr);
    if (!factory) return 1;
    check(refs(factory) == 1, "one reference at first (%lu)", refs(factory));

    factory->GetSystemFontCollection(&collection, FALSE);
    check(refs(factory) == 2, "the system collection takes a reference (%lu)", refs(factory));
    collection->Release();
    check(refs(factory) == 1, "and gives it back (%lu)", refs(factory));

    factory->RegisterFontCollectionLoader(loader);
    hr = factory->CreateCustomFontCollection(loader, "key", 4, &custom);
    check(hr == S_OK && custom, "custom collection (%#lx)", hr);
    check(refs(factory) == 1, "a custom collection takes none (%lu)", refs(factory));
    if (custom) custom->Release();
    factory->UnregisterFontCollectionLoader(loader);
    loader->Release();

    factory->CreateFontFallbackBuilder(&builder);
    check(refs(factory) == 2, "the fallback builder takes one (%lu)", refs(factory));
    hr = builder->CreateFontFallback(&fallback);
    check(hr == S_OK && fallback, "fallback (%#lx)", hr);
    check(refs(factory) == 3, "the fallback adds only its system collection's (%lu)", refs(factory));
    fallback->Release();
    check(refs(factory) == 2, "released with it (%lu)", refs(factory));
    builder->Release();
    check(refs(factory) == 1, "builder released (%lu)", refs(factory));
    factory->Release();

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
