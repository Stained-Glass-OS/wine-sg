/* d3dxof: save side. Write templates and objects, read them back through the parser. */
#define COBJMACROS
#include <windows.h>
#include <dxfile.h>
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static const GUID g_vec   = {0x11111111,0,0,{0,0,0,0,0,0,0,1}};
static const GUID g_thing = {0x11111111,0,0,{0,0,0,0,0,0,0,2}};
static const GUID g_mark  = {0x11111111,0,0,{0,0,0,0,0,0,0,3}};
static const GUID g_none  = {0x22222222,0,0,{0,0,0,0,0,0,0,9}};
static const GUID id_thing = {0xaaaaaaaa,0xbbbb,0xcccc,{0xdd,0xdd,0xee,0xee,0xee,0xee,0xee,0xee}};

static const char templates[] =
    "xof 0303txt 0032\n"
    "template Vec3 { <11111111-0000-0000-0000-000000000001> FLOAT x; FLOAT y; FLOAT z; }\n"
    "template Thing { <11111111-0000-0000-0000-000000000002> STRING label; DWORD n; array Vec3 pts[n]; WORD w; [...] }\n"
    "template Mark { <11111111-0000-0000-0000-000000000003> DWORD id; }\n";

#pragma pack(push, 1)
struct thing { char *label; DWORD n; float pts[6]; WORD w; };
#pragma pack(pop)

static int contains(const char *hay, const char *needle) { return strstr(hay, needle) != NULL; }

static char *slurp(const char *path)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD size, got;
    char *p;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    size = GetFileSize(h, NULL);
    p = calloc(size + 1, 1);
    ReadFile(h, p, size, &got, NULL);
    CloseHandle(h);
    return p;
}

static HRESULT (WINAPI *create)(LPDIRECTXFILE *);

static void run(DXFILEFORMAT fmt, const char *fname)
{
    LPDIRECTXFILE dx = NULL, dx2 = NULL;
    LPDIRECTXFILESAVEOBJECT save = NULL, save2 = NULL;
    LPDIRECTXFILEDATA other = NULL, thing = NULL, mark = NULL, d = NULL;
    LPDIRECTXFILEENUMOBJECT en = NULL;
    LPDIRECTXFILEOBJECT child = NULL;
    const GUID *tpl_list[3] = { &g_vec, &g_thing, &g_mark };
    const GUID *bad_list[1] = { &g_none };
    char *label = strdup("hello");
    struct thing t = { NULL, 2, { 1, 2, 3, 4.5f, 5, 6 }, 7 };
    struct thing *got;
    DWORD mark_id = 42, other_id = 5, size, len;
    char name[64], path[MAX_PATH], *text;
    const GUID *type;
    GUID id;
    HRESULT hr;
    IDirectXFileData *dd;

    GetCurrentDirectoryA(MAX_PATH, path);
    strcat(path, "\\");
    strcat(path, fname);
    DeleteFileA(path);

    CHECK(create && create(&dx) == DXFILE_OK);
    CHECK(IDirectXFile_RegisterTemplates(dx, (void *)templates, sizeof(templates) - 1) == DXFILE_OK);

    hr = IDirectXFile_CreateSaveObject(dx, path, 4, &save2);
    CHECK(hr == DXFILEERR_BADVALUE);
    hr = IDirectXFile_CreateSaveObject(dx, "Z:\\nonexistent-dir\\x.x", DXFILEFORMAT_TEXT, &save2);
    CHECK(FAILED(hr));
    hr = IDirectXFile_CreateSaveObject(dx, path, fmt, &save);
    CHECK(hr == DXFILE_OK && save);
    if (!save) goto end;

    hr = IDirectXFileSaveObject_SaveTemplates(save, 1, bad_list);
    CHECK(hr == DXFILEERR_NOTEMPLATE);
    hr = IDirectXFileSaveObject_SaveTemplates(save, 3, tpl_list);
    CHECK(hr == DXFILE_OK);

    /* errors */
    hr = IDirectXFileSaveObject_CreateDataObject(save, &g_none, "x", NULL, 4, &mark_id, &d);
    CHECK(hr == DXFILEERR_NOTEMPLATE && !d);
    hr = IDirectXFileSaveObject_CreateDataObject(save, &g_mark, "x", NULL, 2, &mark_id, &d);
    CHECK(hr == DXFILEERR_BADVALUE);
    t.label = "x";
    hr = IDirectXFileSaveObject_CreateDataObject(save, &g_thing, "x", NULL, sizeof(t) - 8, &t, &d);
    CHECK(hr == DXFILEERR_BADVALUE);

    hr = IDirectXFileSaveObject_CreateDataObject(save, &g_mark, "other", NULL, 4, &other_id, &other);
    CHECK(hr == DXFILE_OK && other);
    hr = IDirectXFileSaveObject_SaveData(save, other);
    CHECK(hr == DXFILE_OK);

    t.label = label;
    hr = IDirectXFileSaveObject_CreateDataObject(save, &g_thing, "t1", &id_thing, sizeof(t), &t, &thing);
    CHECK(hr == DXFILE_OK && thing);
    strcpy(label, "XXXXX");               /* the object keeps its own copy of the string */
    free(label);
    hr = IDirectXFileSaveObject_CreateDataObject(save, &g_mark, NULL, NULL, 4, &mark_id, &mark);
    CHECK(hr == DXFILE_OK && mark);
    CHECK(IDirectXFileData_AddDataObject(thing, mark) == DXFILE_OK);
    CHECK(IDirectXFileData_AddDataObject(thing, NULL) == DXFILEERR_BADVALUE);
    CHECK(IDirectXFileData_AddDataObject(thing, thing) == DXFILEERR_BADVALUE);
    CHECK(IDirectXFileData_AddDataReference(thing, "other", NULL) == DXFILE_OK);
    CHECK(IDirectXFileData_AddDataReference(thing, NULL, NULL) == DXFILEERR_BADVALUE);
    IDirectXFileData_Release(mark);

    /* the object reports what it was made from */
    len = sizeof(name);
    CHECK(IDirectXFileData_GetName(thing, name, &len) == DXFILE_OK && !strcmp(name, "t1"));
    CHECK(IDirectXFileData_GetId(thing, &id) == DXFILE_OK && IsEqualGUID(&id, &id_thing));
    CHECK(IDirectXFileData_GetType(thing, &type) == DXFILE_OK && IsEqualGUID(type, &g_thing));
    CHECK(IDirectXFileData_GetData(thing, "n", &size, (void **)&got) == DXFILE_OK && size == 4 && *(DWORD *)got == 2);

    hr = IDirectXFileSaveObject_SaveData(save, thing);
    CHECK(hr == DXFILE_OK);
    IDirectXFileData_Release(thing);
    IDirectXFileData_Release(other);
    IDirectXFileSaveObject_Release(save);

    text = slurp(path);
    CHECK(text != NULL);
    if (text && fmt == DXFILEFORMAT_BINARY)
    {
        CHECK(!strncmp(text, "xof 0303bin 0032", 16));
        CHECK(text[16] == 31 && text[17] == 0);             /* TOKEN_TEMPLATE first */
        CHECK(!memcmp(text + 18, "\1\0\4\0\0\0Vec3", 10));  /* then its name token */
        CHECK(!contains(text + 16, "template"));
    }
    else if (text && fmt == (DXFILEFORMAT_BINARY | DXFILEFORMAT_COMPRESSED))
    {
        CHECK(!strncmp(text, "xof 0303bzip0032", 16));
        CHECK(text[24] == 'C' && text[25] == 'K');           /* the MSZIP signature of the first chunk */
        CHECK(*(WORD *)(text + 20) > 16 && *(DWORD *)(text + 16) >= 16 + *(WORD *)(text + 20));
    }
    else if (text && fmt == (DXFILEFORMAT_TEXT | DXFILEFORMAT_COMPRESSED))
    {
        CHECK(!strncmp(text, "xof 0303tzip0032", 16));
        CHECK(text[24] == 'C' && text[25] == 'K');
    }
    else if (text)
    {
        CHECK(!strncmp(text, "xof 0303txt 0032", 16));
        CHECK(contains(text, "template Thing {"));
        CHECK(contains(text, "STRING label;"));
        CHECK(contains(text, "array Vec3 pts[n];"));
        CHECK(contains(text, "[...]"));
        CHECK(contains(text, "Thing t1 {"));
        CHECK(contains(text, "\"hello\";"));
        CHECK(contains(text, "1.000000;2.000000;3.000000;,"));
        CHECK(contains(text, "4.500000;5.000000;6.000000;;"));
        CHECK(!contains(text, "XXXXX"));
        CHECK(contains(text, "{ other }"));
    }

    /* read it back through the parser */
    CHECK(create(&dx2) == DXFILE_OK);
    {
        DXFILELOADOPTIONS opt = DXFILELOAD_FROMFILE;
        hr = IDirectXFile_CreateEnumObject(dx2, path, opt, &en);
    }
    CHECK(hr == DXFILE_OK && en);
    if (!en) goto end;
    hr = IDirectXFileEnumObject_GetNextDataObject(en, &d);        /* other */
    CHECK(hr == DXFILE_OK);
    len = sizeof(name);
    if (d) { CHECK(IDirectXFileData_GetName(d, name, &len) == DXFILE_OK && !strcmp(name, "other"));
             CHECK(IDirectXFileData_GetData(d, NULL, &size, (void **)&got) == DXFILE_OK && size == 4 && *(DWORD *)got == 5);
             IDirectXFileData_Release(d); }
    d = NULL;
    hr = IDirectXFileEnumObject_GetNextDataObject(en, &d);        /* t1 */
    CHECK(hr == DXFILE_OK && d);
    if (d)
    {
        len = sizeof(name);
        CHECK(IDirectXFileData_GetName(d, name, &len) == DXFILE_OK && !strcmp(name, "t1"));
        CHECK(IDirectXFileData_GetId(d, &id) == DXFILE_OK && IsEqualGUID(&id, &id_thing));
        hr = IDirectXFileData_GetData(d, NULL, &size, (void **)&got);
        CHECK(hr == DXFILE_OK && size == sizeof(t));
        if (hr == DXFILE_OK && size == sizeof(t))
        {
            CHECK(!strcmp(got->label, "hello"));
            CHECK(got->n == 2 && got->pts[0] == 1 && got->pts[3] == 4.5f && got->pts[5] == 6 && got->w == 7);
        }
        hr = IDirectXFileData_GetNextObject(d, &child);
        CHECK(hr == DXFILE_OK && child);
        if (child)
        {
            hr = IDirectXFileObject_QueryInterface(child, &IID_IDirectXFileData, (void **)&dd);
            CHECK(hr == DXFILE_OK);
            if (hr == DXFILE_OK)
            {
                CHECK(IDirectXFileData_GetData(dd, NULL, &size, (void **)&got) == DXFILE_OK && *(DWORD *)got == 42);
                IDirectXFileData_Release(dd);
            }
            IDirectXFileObject_Release(child);
        }
        child = NULL;
        hr = IDirectXFileData_GetNextObject(d, &child);
        CHECK(hr == DXFILE_OK && child);
        if (child)
        {
            LPDIRECTXFILEDATAREFERENCE ref;
            hr = IDirectXFileObject_QueryInterface(child, &IID_IDirectXFileDataReference, (void **)&ref);
            CHECK(hr == DXFILE_OK);
            if (hr == DXFILE_OK)
            {
                len = sizeof(name);
                CHECK(IDirectXFileDataReference_GetName(ref, name, &len) == DXFILE_OK && !strcmp(name, "other"));
                IDirectXFileDataReference_Release(ref);
            }
            IDirectXFileObject_Release(child);
        }
        IDirectXFileData_Release(d);
    }
    IDirectXFileEnumObject_Release(en);
end:
    if (dx2) IDirectXFile_Release(dx2);
    IDirectXFile_Release(dx);
}

/* a body larger than one 32 KB MSZIP chunk, and a template with the other integer types */
static void run_big(DXFILEFORMAT fmt, const char *fname)
{
    LPDIRECTXFILE dx = NULL, dx2 = NULL;
    LPDIRECTXFILESAVEOBJECT save = NULL;
    LPDIRECTXFILEDATA obj = NULL, d = NULL;
    LPDIRECTXFILEENUMOBJECT en = NULL;
    const GUID *tpl_list[2] = { &g_vec, &g_thing };
    DWORD n = 8000, i, size, bytes = 4 + 4 + n * 12 + 2 + 4;
    BYTE *buf = calloc(1, bytes), *p;
    struct thing *got;
    char path[MAX_PATH];
    HRESULT hr;

    GetCurrentDirectoryA(MAX_PATH, path);
    strcat(path, "\\");
    strcat(path, fname);
    DeleteFileA(path);

    /* Thing = { STRING label; DWORD n; array Vec3 pts[n]; WORD w } with the string pointer first */
    p = buf;
    *(char **)p = "big"; p += sizeof(char *);
    *(DWORD *)p = n; p += 4;
    for (i = 0; i < n * 3; i++) { *(float *)p = (float)i; p += 4; }
    *(WORD *)p = 9; p += 2;

    CHECK(create(&dx) == DXFILE_OK);
    CHECK(IDirectXFile_RegisterTemplates(dx, (void *)templates, sizeof(templates) - 1) == DXFILE_OK);
    CHECK(IDirectXFile_CreateSaveObject(dx, path, fmt, &save) == DXFILE_OK);
    CHECK(IDirectXFileSaveObject_SaveTemplates(save, 2, tpl_list) == DXFILE_OK);
    hr = IDirectXFileSaveObject_CreateDataObject(save, &g_thing, "bigone", NULL, p - buf, buf, &obj);
    CHECK(hr == DXFILE_OK && obj);
    if (obj) CHECK(IDirectXFileSaveObject_SaveData(save, obj) == DXFILE_OK);
    if (obj) IDirectXFileData_Release(obj);
    IDirectXFileSaveObject_Release(save);
    IDirectXFile_Release(dx);

    CHECK(create(&dx2) == DXFILE_OK);
    hr = IDirectXFile_CreateEnumObject(dx2, path, DXFILELOAD_FROMFILE, &en);
    CHECK(hr == DXFILE_OK && en);
    if (en)
    {
        hr = IDirectXFileEnumObject_GetNextDataObject(en, &d);
        CHECK(hr == DXFILE_OK && d);
        if (d)
        {
            hr = IDirectXFileData_GetData(d, NULL, &size, (void **)&got);
            CHECK(hr == DXFILE_OK && size == p - buf);
            if (hr == DXFILE_OK && size == p - buf)
            {
                CHECK(!strcmp(got->label, "big") && got->n == n);
                CHECK(got->pts[0] == 0 && got->pts[5] == 5 && *(float *)((BYTE *)got->pts + (n * 3 - 1) * 4) == n * 3 - 1);
            }
            IDirectXFileData_Release(d);
        }
        IDirectXFileEnumObject_Release(en);
    }
    IDirectXFile_Release(dx2);
    free(buf);
}

int main(void)
{
    HMODULE m = LoadLibraryA("d3dxof.dll");

    create = (void *)GetProcAddress(m, "DirectXFileCreate");
    run(DXFILEFORMAT_TEXT, "out-txt.x");
    run(DXFILEFORMAT_BINARY, "out-bin.x");
    run(DXFILEFORMAT_TEXT | DXFILEFORMAT_COMPRESSED, "out-tzip.x");
    run(DXFILEFORMAT_BINARY | DXFILEFORMAT_COMPRESSED, "out-bzip.x");
    run_big(DXFILEFORMAT_BINARY, "big-bin.x");
    run_big(DXFILEFORMAT_BINARY | DXFILEFORMAT_COMPRESSED, "big-bzip.x");
    run_big(DXFILEFORMAT_TEXT | DXFILEFORMAT_COMPRESSED, "big-tzip.x");
    printf("%d checks, %d failures\nRESULT: %s\n", checks, fails, fails ? "FAIL" : "PASS");
    return fails != 0;
}
