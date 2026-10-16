/* IEnumGUID::Skip of the component categories manager (ICatInformation::EnumClassesOfCategories and
 * EnumImplCategoriesOfClass): Skip(n) steps over n entries and says S_FALSE when fewer were left.
 * It used to step the index blindly and always say S_OK. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <comcat.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const GUID cat1 = {0x5ee0e9a1, 0x4001, 0x4c01, {1, 2, 3, 4, 5, 6, 7, 1}};
static const GUID cat2 = {0x5ee0e9a1, 0x4001, 0x4c01, {1, 2, 3, 4, 5, 6, 7, 2}};
static const GUID cat3 = {0x5ee0e9a1, 0x4001, 0x4c01, {1, 2, 3, 4, 5, 6, 7, 3}};
static const GUID cls[3] =
{
    {0x5ee0e9a2, 0x4002, 0x4c02, {9, 9, 9, 9, 9, 9, 9, 1}},
    {0x5ee0e9a2, 0x4002, 0x4c02, {9, 9, 9, 9, 9, 9, 9, 2}},
    {0x5ee0e9a2, 0x4002, 0x4c02, {9, 9, 9, 9, 9, 9, 9, 3}},
};

/* the table: how many to skip, the result, and what Next(1) then gives (an index in the first pass, or -1 for nothing) */
static const struct { ULONG skip; HRESULT hr; int next; const char *name; } steps[] =
{
    {0, S_OK, 0, "skip nothing, the first is next"},
    {1, S_OK, 1, "skip one, the second is next"},
    {2, S_OK, 2, "skip two, the third is next"},
    {3, S_OK, -1, "skip all three, none is left"},
    {4, S_FALSE, -1, "skip four of three"},
    {100, S_FALSE, -1, "skip a hundred of three"},
};

static void run_table(IEnumGUID *en, const GUID *first_pass, const char *kind)
{
    unsigned int i;
    char text[160];

    for (i = 0; i < sizeof(steps) / sizeof(steps[0]); i++)
    {
        GUID g;
        ULONG got = 99;
        HRESULT hr;

        IEnumGUID_Reset(en);
        hr = IEnumGUID_Skip(en, steps[i].skip);
        sprintf(text, "%s: %s (got %#lx)", kind, steps[i].name, hr);
        check(hr == steps[i].hr, text);
        memset(&g, 0, sizeof(g));
        hr = IEnumGUID_Next(en, 1, &g, &got);
        if (steps[i].next >= 0)
        {
            sprintf(text, "%s: after it Next gives entry %d", kind, steps[i].next);
            check(hr == S_OK && got == 1 && IsEqualGUID(&g, &first_pass[steps[i].next]), text);
        }
        else
        {
            sprintf(text, "%s: after it Next gives nothing", kind);
            check(hr == S_FALSE && !got, text);
        }
    }
    /* skip in two steps */
    IEnumGUID_Reset(en);
    check(IEnumGUID_Skip(en, 1) == S_OK && IEnumGUID_Skip(en, 1) == S_OK, "two skips of one");
    {
        GUID g;
        ULONG got = 0;
        check(IEnumGUID_Next(en, 1, &g, &got) == S_OK && IsEqualGUID(&g, &first_pass[2]), "then the third is next");
        check(IEnumGUID_Skip(en, 1) == S_FALSE, "skipping past the end after the last");
    }
}

int main(void)
{
    ICatRegister *reg;
    ICatInformation *info;
    IEnumGUID *en;
    CATEGORYINFO ci;
    HRESULT hr;
    GUID first[3], catids[3];
    ULONG got;
    unsigned int i;
    CATID all[3];

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_StdComponentCategoriesMgr, NULL, CLSCTX_INPROC_SERVER, &IID_ICatRegister, (void **)&reg);
    check(hr == S_OK, "the categories manager (register)");
    hr = CoCreateInstance(&CLSID_StdComponentCategoriesMgr, NULL, CLSCTX_INPROC_SERVER, &IID_ICatInformation, (void **)&info);
    check(hr == S_OK, "the categories manager (information)");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    all[0] = cat1; all[1] = cat2; all[2] = cat3;
    for (i = 0; i < 3; i++)
    {
        ci.catid = all[i];
        ci.lcid = 0x409;
        wsprintfW(ci.szDescription, L"sg test category %u", i);
        check(ICatRegister_RegisterCategories(reg, 1, &ci) == S_OK, "register a category");
    }
    for (i = 0; i < 3; i++)
        check(ICatRegister_RegisterClassImplCategories(reg, &cls[i], 1, (CATID *)&cat1) == S_OK, "register a class in the first category");
    check(ICatRegister_RegisterClassImplCategories(reg, &cls[0], 3, all) == S_OK, "the first class implements all three");

    /* classes of a category */
    hr = ICatInformation_EnumClassesOfCategories(info, 1, (CATID *)&cat1, 0, NULL, &en);
    check(hr == S_OK, "EnumClassesOfCategories");
    got = 0;
    hr = IEnumGUID_Next(en, 3, first, &got);
    check(hr == S_OK && got == 3, "the three classes come out");
    run_table(en, first, "classes");
    IEnumGUID_Release(en);

    /* categories of a class */
    hr = ICatInformation_EnumImplCategoriesOfClass(info, &cls[0], &en);
    check(hr == S_OK, "EnumImplCategoriesOfClass");
    got = 0;
    hr = IEnumGUID_Next(en, 3, catids, &got);
    check(hr == S_OK && got == 3, "the three categories come out");
    run_table(en, catids, "categories");
    IEnumGUID_Release(en);

    /* clean up what was registered */
    for (i = 0; i < 3; i++) ICatRegister_UnRegisterClassImplCategories(reg, &cls[i], 1, (CATID *)&cat1);
    ICatRegister_UnRegisterClassImplCategories(reg, &cls[0], 3, all);
    ICatRegister_UnRegisterCategories(reg, 3, all);
    ICatRegister_Release(reg);
    ICatInformation_Release(info);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
