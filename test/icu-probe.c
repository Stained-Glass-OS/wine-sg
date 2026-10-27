/* icu.dll, Windows' combined ICU library (patches/sg/6127): character properties and strings,
 * converters forwarded to icuuc; and the api set ext-ms-win-shell32-shellfolders-l1-1-0 (shell32).
 * Windows Terminal links to u_charType, u_countChar32, u_foldCase and u_strToUTF32 in icu.dll and to
 * that api set, and would not load. Prints KEY=VALUE lines. */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

typedef int8_t (__cdecl *chartype_fn)(int32_t);
typedef int32_t (__cdecl *fold_fn)(int32_t, uint32_t);
typedef int32_t (__cdecl *count_fn)(const WCHAR *, int32_t);
typedef int32_t *(__cdecl *toutf32_fn)(int32_t *, int32_t, int32_t *, const WCHAR *, int32_t, int *);
typedef void *(__cdecl *open_fn)(const char *, int *);
typedef void (__cdecl *close_fn)(void *);

int main(void)
{
    HMODULE icu = LoadLibraryA("icu.dll");
    chartype_fn type = icu ? (void *)GetProcAddress(icu, "u_charType") : NULL;
    fold_fn fold = icu ? (void *)GetProcAddress(icu, "u_foldCase") : NULL;
    count_fn count = icu ? (void *)GetProcAddress(icu, "u_countChar32") : NULL;
    toutf32_fn to32 = icu ? (void *)GetProcAddress(icu, "u_strToUTF32") : NULL;
    open_fn open = icu ? (void *)GetProcAddress(icu, "ucnv_open") : NULL;
    close_fn close = icu ? (void *)GetProcAddress(icu, "ucnv_close") : NULL;
    static const WCHAR text[] = { 'a', 0xd83d, 0xde00, 'b', 0 };
    static const WCHAR bad[] = { 'a', 0xdc00, 0 };
    int32_t out[8], len = -1;
    int err;
    void *cnv;

    printf("load=%d\n", icu && type && fold && count && to32);
    if (!icu || !type || !fold || !count || !to32) return 1;
    printf("type=%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", type('A'), type('a'), type('5'), type(' '),
           type(0x300), type(0x20ac), type(0x1f600), type(0xe000), type(0xd800), type(0x378), type('('),
           type(')'), type('-'), type('_'), type('+'));
    printf("fold=%x %x %x %x %x %x\n", fold('A', 0), fold(0x130, 0), fold(0x130, 1), fold('I', 1),
           fold(0x1e9e, 0), fold(0x10400, 0));
    printf("count=%d %d %d\n", count(text, -1), count(text, 2), count(text, 4));
    err = 0;
    to32(out, 8, &len, text, -1, &err);
    printf("utf32=%d %d %x %x %x\n", err, len, out[0], out[1], out[2]);
    err = 0; len = -1;
    to32(out, 2, &len, text, -1, &err);
    printf("utf32_small=%d %d\n", err, len);
    err = 0;
    to32(out, 8, &len, bad, -1, &err);
    printf("utf32_bad=%d\n", err);
    err = 0;
    cnv = open ? open("UTF-8", &err) : NULL;
    printf("ucnv=%d\n", cnv != NULL && err <= 0);
    if (cnv && close) close(cnv);
    {
        /* the api set Windows Terminal also links to: shell32's known folders */
        HMODULE set = LoadLibraryA("ext-ms-win-shell32-shellfolders-l1-1-0.dll");
        void *fn = set ? (void *)GetProcAddress(set, "SHGetKnownFolderPath") : NULL;
        printf("apiset=%d\n", fn != NULL && fn == (void *)GetProcAddress(LoadLibraryA("shell32.dll"), "SHGetKnownFolderPath"));
    }
    return 0;
}
