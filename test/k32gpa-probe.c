/* kernel32's GetProcAddress is its own export (patches/sg/6544). Windows'
 * kernel32 exports GetProcAddress, LoadLibraryA/W, GetModuleHandleA/W and
 * FreeLibrary as code, not as forwarders to kernelbase; Wine forwarded
 * GetProcAddress ("kernelbase.GetProcAddress"). PotPlayer's packer walks
 * kernel32's export table to find GetProcAddress, cannot follow a forwarder
 * without it, called a null pointer and overflowed its stack in its own
 * exception handler. The probe reads kernel32's export table the way such a
 * loader does: each of those names is code, and calling it works. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static const char *names[] = { "GetProcAddress", "LoadLibraryA", "LoadLibraryW", "LoadLibraryExA",
                               "GetModuleHandleA", "GetModuleHandleW", "FreeLibrary" };

static DWORD export_rva( const BYTE *base, const char *name, const IMAGE_DATA_DIRECTORY **dir_out )
{
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(base + ((const IMAGE_DOS_HEADER *)base)->e_lfanew);
    const IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    const IMAGE_EXPORT_DIRECTORY *exp = (const IMAGE_EXPORT_DIRECTORY *)(base + dir->VirtualAddress);
    const DWORD *funcs = (const DWORD *)(base + exp->AddressOfFunctions);
    const DWORD *names_rva = (const DWORD *)(base + exp->AddressOfNames);
    const WORD *ords = (const WORD *)(base + exp->AddressOfNameOrdinals);
    DWORD i;

    *dir_out = dir;
    for (i = 0; i < exp->NumberOfNames; i++)
        if (!strcmp( (const char *)base + names_rva[i], name )) return funcs[ords[i]];
    return 0;
}

int main( void )
{
    const BYTE *k32 = (const BYTE *)GetModuleHandleA( "kernel32.dll" );
    const IMAGE_DATA_DIRECTORY *dir;
    FARPROC (WINAPI *gpa)( HMODULE, LPCSTR );
    unsigned int i, code = 0;
    DWORD rva;

    for (i = 0; i < ARRAYSIZE(names); i++)
    {
        BOOL is_code;
        rva = export_rva( k32, names[i], &dir );
        is_code = rva && (rva < dir->VirtualAddress || rva >= dir->VirtualAddress + dir->Size);
        printf( "%s: %s\n", names[i], !rva ? "missing" : is_code ? "code" : (const char *)k32 + rva );
        if (is_code) code++;
    }
    printf( "all_code=%d\n", code == ARRAYSIZE(names) );

    rva = export_rva( k32, "GetProcAddress", &dir );
    gpa = (void *)(k32 + rva);
    if (rva && (rva < dir->VirtualAddress || rva >= dir->VirtualAddress + dir->Size))
    {
        FARPROC sleep_fn = gpa( (HMODULE)k32, "Sleep" ), none = gpa( (HMODULE)k32, "NoSuchExport" );
        DWORD err = GetLastError();
        printf( "gpa_works=%d gpa_missing_null=%d (err %lu)\n", sleep_fn && sleep_fn == GetProcAddress( (HMODULE)k32, "Sleep" ),
                !none && err == ERROR_PROC_NOT_FOUND, err );
    }
    else printf( "gpa_works=0 gpa_missing_null=0\n" );
    printf( "done=1\n" );
    return 0;
}
