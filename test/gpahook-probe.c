/* kernel32's GetProcAddress goes through kernelbase (patches/sg/0644).
 *
 * The TWAIN data source manager (TWAINDSM.dll) takes over the old
 * TWAIN_32.DLL's DSM_Entry the way it does on Windows: it replaces
 * ntdll!LdrGetProcedureAddress in kernelbase's import table, so a
 * GetProcAddress( TWAIN_32, "DSM_Entry" ) from a TWAIN 1.x data source returns
 * its own entry point. This probe does the same and asks through kernel32
 * (as a program does): "redirected=1" when the hook answered. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <string.h>

typedef NTSTATUS (WINAPI *ldr_gpa_fn)( HMODULE, const ANSI_STRING *, ULONG, void ** );
static ldr_gpa_fn orig;
static int calls;

static int WINAPI replacement( void ) { return 42; }

static NTSTATUS WINAPI hook( HMODULE module, const ANSI_STRING *name, ULONG ord, void **proc )
{
    calls++;
    if (name && name->Length == 13 && !memcmp( name->Buffer, "SgProbeExport", 13 ))
    {
        *proc = (void *)replacement;
        return 0;
    }
    return orig( module, name, ord, proc );
}

static void **find_slot( HMODULE module, const char *dll, const char *func )
{
    BYTE *base = (BYTE *)module;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *imp;

    if (!dir->VirtualAddress) return NULL;
    for (imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir->VirtualAddress); imp->Name; imp++)
    {
        IMAGE_THUNK_DATA *names, *slots;
        if (_stricmp( (char *)base + imp->Name, dll )) continue;
        names = (IMAGE_THUNK_DATA *)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        slots = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
        for (; names->u1.AddressOfData; names++, slots++)
        {
            IMAGE_IMPORT_BY_NAME *by_name;
            if (IMAGE_SNAP_BY_ORDINAL( names->u1.Ordinal )) continue;
            by_name = (IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData);
            if (!strcmp( (char *)by_name->Name, func )) return (void **)&slots->u1.Function;
        }
    }
    return NULL;
}

int main( void )
{
    HMODULE kernelbase = GetModuleHandleA( "kernelbase.dll" ), kernel32 = GetModuleHandleA( "kernel32.dll" );
    void **slot = kernelbase ? find_slot( kernelbase, "ntdll.dll", "LdrGetProcedureAddress" ) : NULL;
    DWORD old;
    FARPROC p;

    printf( "slot=%d\n", slot != NULL );
    if (!slot) return 1;
    VirtualProtect( slot, sizeof(*slot), PAGE_EXECUTE_READWRITE, &old );
    orig = (ldr_gpa_fn)*slot;
    *slot = (void *)hook;
    VirtualProtect( slot, sizeof(*slot), old, &old );

    p = GetProcAddress( kernel32, "SgProbeExport" );
    printf( "redirected=%d\n", p == (FARPROC)replacement );
    p = GetProcAddress( kernel32, "Sleep" );
    printf( "others=%d\n", p != NULL && p == (FARPROC)Sleep );
    printf( "calls=%d\n", calls );
    return 0;
}
