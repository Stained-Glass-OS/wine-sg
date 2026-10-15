#define COBJMACROS
#include <windows.h>
#include <dxfile.h>
#include <stdio.h>

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static char file[] =
    "xof 0303txt 0032\n"
    "template Foo { <11111111-2222-3333-4444-555555555555> DWORD a; }\n"
    "Foo MyObj { <aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee> 7; }\n"
    "Foo Other { 9; }\n";

int main(void)
{
    HMODULE m = LoadLibraryA("d3dxof.dll");
    HRESULT (WINAPI *create)(LPDIRECTXFILE *) = (void *)GetProcAddress(m, "DirectXFileCreate");
    LPDIRECTXFILE dx = NULL;
    LPDIRECTXFILEENUMOBJECT en = NULL;
    LPDIRECTXFILEDATA d = NULL;
    DXFILELOADMEMORY mem = { file, sizeof(file) - 1 };
    GUID id = {0xaaaaaaaa, 0xbbbb, 0xcccc, {0xdd, 0xdd, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee}};
    GUID bad = {1, 2, 3, {4}};
    char name[64];
    DWORD len;
    HRESULT hr;

    CHECK(create && create(&dx) == DXFILE_OK);
    hr = IDirectXFile_CreateEnumObject(dx, &mem, DXFILELOAD_FROMMEMORY, &en);
    CHECK(hr == DXFILE_OK);

    hr = IDirectXFileEnumObject_GetDataObjectByName(en, "Other", &d);
    CHECK(hr == DXFILE_OK && d);
    if (d)
    {
        len = sizeof(name);
        CHECK(IDirectXFileData_GetName(d, name, &len) == DXFILE_OK && !strcmp(name, "Other"));
        IDirectXFileData_Release(d);
    }
    d = NULL;
    hr = IDirectXFileEnumObject_GetDataObjectById(en, &id, &d);
    CHECK(hr == DXFILE_OK && d);
    if (d)
    {
        len = sizeof(name);
        CHECK(IDirectXFileData_GetName(d, name, &len) == DXFILE_OK && !strcmp(name, "MyObj"));
        IDirectXFileData_Release(d);
    }
    d = (void *)1;
    CHECK(IDirectXFileEnumObject_GetDataObjectById(en, &bad, &d) == DXFILEERR_NOTFOUND && d == NULL);
    CHECK(IDirectXFileEnumObject_GetDataObjectByName(en, "Nope", &d) == DXFILEERR_NOTFOUND);
    IDirectXFileEnumObject_Release(en);
    IDirectXFile_Release(dx);
    printf("%d checks, %d failures\nRESULT: %s\n", checks, fails, fails ? "FAIL" : "PASS");
    return fails != 0;
}
