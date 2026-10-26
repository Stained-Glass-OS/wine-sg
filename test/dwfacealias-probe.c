/* dwfacealias-probe: a face of a font that a Replacements / FontSubstitutes name stands for
 * gives that name among its family names, first (patches/sg/7006) -- also when the face comes
 * from a collection made of the font's file alone, as Chromium makes them.
 *   dwfacealias-probe.exe FONTFILE OWNNAME ALIAS OTHERFILE
 * Prints key=1|0 lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <dwrite_3.h>
#include <stdio.h>

static IDWriteFactory *factory;
static const WCHAR *current_path;

struct fenum { IDWriteFontFileEnumerator iface; LONG ref; int pos; const WCHAR *path; };
static HRESULT WINAPI fe_QI(IDWriteFontFileEnumerator *i, REFIID r, void **o)
{ if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDWriteFontFileEnumerator)) { *o = i; IDWriteFontFileEnumerator_AddRef(i); return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI fe_AddRef(IDWriteFontFileEnumerator *i) { return InterlockedIncrement(&((struct fenum *)i)->ref); }
static ULONG WINAPI fe_Release(IDWriteFontFileEnumerator *i) { LONG r = InterlockedDecrement(&((struct fenum *)i)->ref); if (!r) free(i); return r; }
static HRESULT WINAPI fe_MoveNext(IDWriteFontFileEnumerator *i, BOOL *cur) { *cur = ((struct fenum *)i)->pos++ == 0; return S_OK; }
static HRESULT WINAPI fe_Get(IDWriteFontFileEnumerator *i, IDWriteFontFile **f) { return IDWriteFactory_CreateFontFileReference(factory, ((struct fenum *)i)->path, NULL, f); }
static IDWriteFontFileEnumeratorVtbl fevtbl = { fe_QI, fe_AddRef, fe_Release, fe_MoveNext, fe_Get };
static HRESULT WINAPI cl_QI(IDWriteFontCollectionLoader *i, REFIID r, void **o)
{ if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDWriteFontCollectionLoader)) { *o = i; return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI cl_AddRef(IDWriteFontCollectionLoader *i) { return 2; }
static ULONG WINAPI cl_Release(IDWriteFontCollectionLoader *i) { return 1; }
static HRESULT WINAPI cl_Create(IDWriteFontCollectionLoader *i, IDWriteFactory *f, const void *key, UINT32 size, IDWriteFontFileEnumerator **e)
{ struct fenum *fe = calloc(1, sizeof(*fe)); fe->iface.lpVtbl = &fevtbl; fe->ref = 1; fe->path = current_path; *e = &fe->iface; return S_OK; }
static IDWriteFontCollectionLoaderVtbl clvtbl = { cl_QI, cl_AddRef, cl_Release, cl_Create };
static IDWriteFontCollectionLoader loader = { &clvtbl };

/* the face of the first font of family 0 of a collection made of PATH alone */
static IDWriteFontFace3 *face_from_file(const WCHAR *path)
{
    IDWriteFontCollection *col; IDWriteFontFamily *fam; IDWriteFont *font; IDWriteFontFace *face; IDWriteFontFace3 *f3 = NULL;
    current_path = path;
    if (FAILED(IDWriteFactory_CreateCustomFontCollection(factory, &loader, NULL, 0, &col))) return NULL;
    if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(col, 0, &fam)) &&
        SUCCEEDED(IDWriteFontFamily_GetFont(fam, 0, &font)) &&
        SUCCEEDED(IDWriteFont_CreateFontFace(font, &face)))
        IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3);
    return f3;
}

/* the first en-us name, and whether NAME is among them */
static BOOL names_of(IDWriteFontFace3 *f3, const WCHAR *name, WCHAR *first)
{
    IDWriteLocalizedStrings *n; UINT32 i, c, idx; BOOL exists = FALSE, has = FALSE; WCHAR s[128];
    first[0] = 0;
    if (!f3 || FAILED(IDWriteFontFace3_GetFamilyNames(f3, &n))) return FALSE;
    if (SUCCEEDED(IDWriteLocalizedStrings_FindLocaleName(n, L"en-us", &idx, &exists)) && exists)
        IDWriteLocalizedStrings_GetString(n, idx, first, 128);
    c = IDWriteLocalizedStrings_GetCount(n);
    for (i = 0; i < c; i++)
    {
        UINT32 len = 0;
        s[0] = 0;
        IDWriteLocalizedStrings_GetStringLength(n, i, &len);
        if (len < 128) IDWriteLocalizedStrings_GetString(n, i, s, 128);
        if (getenv("DWALIAS_LIST")) printf("  name[%u]=%ls\n", i, s);
        if (!lstrcmpiW(s, name)) has = TRUE;
    }
    IDWriteLocalizedStrings_Release(n);
    return has;
}

int main(int argc, char **argv)
{
    WCHAR file[MAX_PATH], own[64], alias[64], other[MAX_PATH], first[128];
    IDWriteFontFace3 *f3;
    BOOL has_alias, has_own;

    if (argc < 5) return 1;
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, file, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, argv[2], -1, own, 64);
    MultiByteToWideChar(CP_ACP, 0, argv[3], -1, alias, 64);
    MultiByteToWideChar(CP_ACP, 0, argv[4], -1, other, MAX_PATH);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory, (IUnknown **)&factory);
    IDWriteFactory_RegisterFontCollectionLoader(factory, &loader);

    f3 = face_from_file(file);
    printf("face=%d\n", f3 != NULL);
    has_alias = names_of(f3, alias, first);
    printf("first_name=%ls\n", first);
    printf("alias_is_family_name=%d\n", has_alias && !lstrcmpiW(first, alias));

    /* the font's family keeps its own name */
    {
        IDWriteFontCollection *sys; IDWriteFontFamily *fam; IDWriteLocalizedStrings *n; UINT32 idx; BOOL exists = FALSE;
        WCHAR famname[128] = L"";
        IDWriteFactory_GetSystemFontCollection(factory, &sys, FALSE);
        IDWriteFontCollection_FindFamilyName(sys, own, &idx, &exists);
        if (exists && SUCCEEDED(IDWriteFontCollection_GetFontFamily(sys, idx, &fam)) &&
            SUCCEEDED(IDWriteFontFamily_GetFamilyNames(fam, &n)))
            IDWriteLocalizedStrings_GetString(n, 0, famname, 128);
        printf("family_name=%ls\n", famname);
        printf("family_keeps_own_name=%d\n", !lstrcmpiW(famname, own));
    }
    has_own = FALSE;

    /* a font no name stands for keeps its own names */
    f3 = face_from_file(other);
    names_of(f3, L"", first);
    printf("other_first=%ls\n", first);
    printf("other_unchanged=%d\n", first[0] && lstrcmpiW(first, alias));
    printf("done=1\n");
    return 0;
}
