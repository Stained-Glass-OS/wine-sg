/* flatcontract-probe: a UniversalApiContract version reported by the flat wintypes exports
 * (RoIsApiContractMajorVersionPresent / RoIsApiContractPresent) must come with its classes.
 * Microsoft Office probes versions 15 down to 7 and on a yes draws through
 * Windows.UI.Composition with its interaction classes (UniversalApiContract 3); when
 * Windows.UI.Composition.Interactions.VisualInteractionSource cannot be activated, Word stops
 * itself at its splash. */
#include <windows.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef void *HSTRING_;
static const GUID IID_IActivationFactory_ = {0x00000035, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

int main(void)
{
    static const WCHAR universal[] = L"Windows.Foundation.UniversalApiContract";
    static const WCHAR interactions[] = L"Windows.UI.Composition.Interactions.VisualInteractionSource";
    HMODULE combase = LoadLibraryA("combase.dll"), wintypes = LoadLibraryA("wintypes.dll");
    HRESULT (WINAPI *create)(const WCHAR *, UINT32, HSTRING_ *) = (void *)GetProcAddress(combase, "WindowsCreateString");
    HRESULT (WINAPI *del)(HSTRING_) = (void *)GetProcAddress(combase, "WindowsDeleteString");
    HRESULT (WINAPI *get_factory)(HSTRING_, const GUID *, void **) = (void *)GetProcAddress(combase, "RoGetActivationFactory");
    HRESULT (WINAPI *ro_init)(int) = (void *)GetProcAddress(combase, "RoInitialize");
    HRESULT (WINAPI *major_present)(const WCHAR *, UINT16, BOOL *) = (void *)GetProcAddress(wintypes, "RoIsApiContractMajorVersionPresent");
    HRESULT (WINAPI *present)(const WCHAR *, UINT16, UINT16, BOOL *) = (void *)GetProcAddress(wintypes, "RoIsApiContractPresent");
    HSTRING_ name = NULL;
    void *factory = NULL;
    BOOL yes, activatable;
    HRESULT hr;
    int v, highest = 0;

    if (!create || !get_factory || !ro_init || !major_present || !present)
    {
        printf("FAIL exports missing\nRESULT: FAIL\n");
        return 1;
    }
    ro_init(1);

    for (v = 1; v <= 15; v++)
    {
        yes = FALSE;
        hr = major_present(universal, v, &yes);
        CHECK(hr == S_OK, "major %d: hr %#lx", v, hr);
        if (yes) highest = v;
    }
    printf("flat UniversalApiContract: highest major reported %d\n", highest);
    /* the exports are not a blanket "no": version 1 is there */
    yes = FALSE;
    hr = major_present(universal, 1, &yes);
    CHECK(hr == S_OK && yes, "major 1: hr %#lx present %d", hr, yes);
    yes = FALSE;
    hr = present(universal, 1, 0, &yes);
    CHECK(hr == S_OK && yes, "1.0: hr %#lx present %d", hr, yes);

    create(interactions, lstrlenW(interactions), &name);
    hr = get_factory(name, &IID_IActivationFactory_, &factory);
    activatable = SUCCEEDED(hr) && factory;
    printf("%ls: hr %#lx (%s)\n", interactions, hr, activatable ? "activatable" : "not activatable");
    if (factory) ((ULONG (WINAPI *)(void *))(*(void ***)factory)[2])(factory);
    del(name);

    /* what Office decides by: any of 7..15 means the full Windows.UI.Composition */
    if (!activatable)
    {
        CHECK(highest < 3, "contract %d reported, but its Windows.UI.Composition.Interactions classes are not activatable", highest);
        yes = TRUE;
        hr = present(universal, 3, 0, &yes);
        CHECK(hr == S_OK && !yes, "3.0: hr %#lx present %d", hr, yes);
    }

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
