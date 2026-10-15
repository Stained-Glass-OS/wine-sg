/* EnumResource{Types,Names,Languages}Ex{A,W} with the MUI flags (kernelbase). The probe copies two resource-only
 * files next to itself as <dir>\<locale>\<exe>.mui and checks what each flag combination enumerates. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static char items[64][40];
static int nitems, stop_after;

static void add_str(const char *s)
{
    if (nitems < 64) { lstrcpynA(items[nitems], s, 40); nitems++; }
}
static void add_id(const void *p, const char *pre)
{
    char b[40];
    if (IS_INTRESOURCE(p)) sprintf(b, "%s#%u", pre, (unsigned)(ULONG_PTR)p); else sprintf(b, "%s%s", pre, (const char *)p);
    add_str(b);
}
static int cmp(const void *a, const void *b) { return strcmp(a, b); }
/* the sorted list, comma separated */
static const char *result(void)
{
    static char out[1024];
    int i;
    qsort(items, nitems, 40, cmp);
    out[0] = 0;
    for (i = 0; i < nitems; i++) { if (i) strcat(out, ","); strcat(out, items[i]); }
    return out;
}
static BOOL CALLBACK typeA(HMODULE m, LPSTR t, LONG_PTR p) { add_id(t, ""); return !(stop_after && nitems >= stop_after); }
static BOOL CALLBACK typeW(HMODULE m, LPWSTR t, LONG_PTR p)
{
    char b[40];
    if (IS_INTRESOURCE(t)) add_id(t, ""); else { WideCharToMultiByte(CP_ACP, 0, t, -1, b, sizeof(b), 0, 0); add_str(b); }
    return !(stop_after && nitems >= stop_after);
}
static BOOL CALLBACK nameA(HMODULE m, LPCSTR t, LPSTR n, LONG_PTR p) { add_id(n, ""); return !(stop_after && nitems >= stop_after); }
static BOOL CALLBACK nameW(HMODULE m, LPCWSTR t, LPWSTR n, LONG_PTR p)
{
    char b[40];
    if (IS_INTRESOURCE(n)) add_id(n, ""); else { WideCharToMultiByte(CP_ACP, 0, n, -1, b, sizeof(b), 0, 0); add_str(b); }
    return !(stop_after && nitems >= stop_after);
}
static BOOL CALLBACK langA(HMODULE m, LPCSTR t, LPCSTR n, WORD l, LONG_PTR p) { char b[16]; sprintf(b, "%04x", l); add_str(b); return !(stop_after && nitems >= stop_after); }
static BOOL CALLBACK langW(HMODULE m, LPCWSTR t, LPCWSTR n, WORD l, LONG_PTR p) { char b[16]; sprintf(b, "%04x", l); add_str(b); return !(stop_after && nitems >= stop_after); }

#define LN RESOURCE_ENUM_LN
#define MUI RESOURCE_ENUM_MUI
#define SYS RESOURCE_ENUM_MUI_SYSTEM
#define VAL RESOURCE_ENUM_VALIDATE

static const struct
{
    int kind;  /* 0 types, 1 names, 2 languages */
    DWORD flags;
    WORD lang;
    const char *expect;   /* NULL: the call must fail */
    DWORD error;
    const char *what;
} tests[] =
{
    { 0, 0,        0, "#10,LNTYPE,MUI,MUITYPE", 0, "default flags are LN|MUI" },
    { 0, LN,       0, "#10,LNTYPE,MUI", 0, "types, LN only" },
    { 0, MUI,      0, "#10,MUI,MUITYPE", 0, "types, MUI only" },
    { 0, LN|MUI,   0, "#10,LNTYPE,MUI,MUITYPE", 0, "types, LN+MUI: no duplicates" },
    { 0, LN|SYS,   0, "#10,LNTYPE,MUI,MUITYPE", 0, "types, LN+MUI_SYSTEM" },
    { 0, VAL,      0, NULL, 0, "validate alone enumerates nothing" },
    { 0, LN|MUI|VAL, 0, "#10,LNTYPE,MUI,MUITYPE", 0, "both files carry a MUI resource: validated" },
    { 0, LN|MUI,   0x40c, "#10,LNTYPE,MUI", 0, "types with the fr-FR file (no MUITYPE there)" },
    { 0, MUI,      0x411, NULL, ERROR_MUI_FILE_NOT_FOUND, "no file for ja-JP: MUI only fails" },
    { 0, LN|MUI,   0x411, "#10,LNTYPE,MUI", 0, "no file for ja-JP: LN still enumerated" },
    { 1, LN,       0, "#1,#2,LNNAME", 0, "names, LN only" },
    { 1, MUI,      0, "#2,#3,LNNAME,MUINAME", 0, "names, MUI only" },
    { 1, LN|MUI,   0, "#1,#2,#3,LNNAME,MUINAME", 0, "names, LN+MUI: #2 and LNNAME once" },
    { 1, LN|MUI,   0x40c, "#1,#2,#4,LNNAME", 0, "names with the fr-FR file (lang argument)" },
    { 1, MUI,      0x40c, "#1,#4", 0, "names, fr-FR only" },
    { 1, LN|MUI|VAL, 0x40c, "#1,#2,LNNAME", 0, "fr-FR has no MUI resource: rejected by validate" },
    { 1, MUI,      0x407, "#2,#3,LNNAME,MUINAME", 0, "de-DE has no file: the neutral de directory is used" },
    { 1, MUI|VAL,  0x40c, NULL, ERROR_MUI_FILE_NOT_FOUND, "fr-FR only, rejected: fails" },
    { 2, LN,       0, "0409", 0, "languages, LN only" },
    { 2, MUI,      0, "0407", 0, "languages, MUI only" },
    { 2, LN|MUI,   0, "0407,0409", 0, "languages, LN+MUI" },
    { 2, SYS,      0, "0407", 0, "languages, MUI_SYSTEM only" },
};

static const char *kinds[] = { "types", "names", "languages" };

static void run(HMODULE m, int i, int wide)
{
    BOOL ret;
    const char *r;
    nitems = 0;
    SetLastError(0xdeadbeef);
    switch (tests[i].kind)
    {
    case 0:
        ret = wide ? EnumResourceTypesExW(m, typeW, 0, tests[i].flags, tests[i].lang)
                   : EnumResourceTypesExA(m, typeA, 0, tests[i].flags, tests[i].lang);
        break;
    case 1:
        ret = wide ? EnumResourceNamesExW(m, MAKEINTRESOURCEW(10), nameW, 0, tests[i].flags, tests[i].lang)
                   : EnumResourceNamesExA(m, RT_RCDATA, nameA, 0, tests[i].flags, tests[i].lang);
        break;
    default:
        ret = wide ? EnumResourceLanguagesExW(m, MAKEINTRESOURCEW(10), MAKEINTRESOURCEW(2), langW, 0, tests[i].flags, tests[i].lang)
                   : EnumResourceLanguagesExA(m, RT_RCDATA, MAKEINTRESOURCEA(2), langA, 0, tests[i].flags, tests[i].lang);
        break;
    }
    r = result();
    if (!tests[i].expect)
    {
        CHECK(!ret && !nitems, "%s%s (%s): ret %d, got [%s]", kinds[tests[i].kind], wide ? "W" : "A", tests[i].what, ret, r);
        if (tests[i].error) CHECK(GetLastError() == tests[i].error, "%s (%s): error %lu", kinds[tests[i].kind], tests[i].what, GetLastError());
        return;
    }
    CHECK(!strcmp(r, tests[i].expect), "%s%s (%s): got [%s] expected [%s] (error %lu)", kinds[tests[i].kind], wide ? "W" : "A",
          tests[i].what, r, tests[i].expect, GetLastError());
}

static void copy_mui(const char *dir, const char *src, const char *exe, WORD lang, const char *locale)
{
    char d[MAX_PATH], to[MAX_PATH];
    sprintf(d, "%s\\%s", dir, locale);
    CreateDirectoryA(d, NULL);
    sprintf(to, "%s\\%s.mui", d, exe);
    if (!CopyFileA(src, to, FALSE)) printf("FAIL copy %s -> %s: %lu\n", src, to, GetLastError()), f++;
}

int main(void)
{
    char path[MAX_PATH], dir[MAX_PATH], src[MAX_PATH], *p;
    WCHAR loc[LOCALE_NAME_MAX_LENGTH];
    char locA[LOCALE_NAME_MAX_LENGTH];
    HMODULE m;
    int i;

    GetModuleFileNameA(NULL, path, MAX_PATH);
    p = strrchr(path, '\\'); *p = 0; strcpy(dir, path);
    for (i = 0; i < 2; i++)  /* the user's and the system's UI language */
    {
        LANGID l = i ? GetSystemDefaultUILanguage() : GetUserDefaultUILanguage();
        LCIDToLocaleName(MAKELCID(l, SORT_DEFAULT), loc, ARRAYSIZE(loc), 0);
        WideCharToMultiByte(CP_ACP, 0, loc, -1, locA, sizeof(locA), 0, 0);
        sprintf(src, "%s\\mui-a.bin", dir);
        copy_mui(dir, src, "fastb-enummui-probe.exe", l, locA);
    }
    sprintf(src, "%s\\mui-a.bin", dir);
    copy_mui(dir, src, "fastb-enummui-probe.exe", 0x407, "de");
    sprintf(src, "%s\\mui-f.bin", dir);
    copy_mui(dir, src, "fastb-enummui-probe.exe", 0x40c, "fr-FR");

    m = GetModuleHandleA(NULL);
    for (i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); i++) { run(m, i, 0); run(m, i, 1); }

    /* a callback that returns FALSE stops the enumeration at once */
    stop_after = 1; nitems = 0;
    CHECK(!EnumResourceNamesExW(m, MAKEINTRESOURCEW(10), nameW, 0, LN|MUI, 0) && nitems == 1, "stop: %d items", nitems);
    stop_after = 0;
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
