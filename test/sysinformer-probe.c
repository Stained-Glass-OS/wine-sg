/* What System Informer's setup needs from ntdll (patches/sg/7900, 7901):
 * RtlGetFullPathName_UEx, and NtCreatePrivateNamespace / NtOpenPrivateNamespace /
 * NtDeletePrivateNamespace with a boundary descriptor as the namespace's identity.
 * Prints key=1 for each check that holds; "child" mode opens the namespace from
 * another process. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#ifndef DIRECTORY_ALL_ACCESS
#define DIRECTORY_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED | 0xf)
#endif
#define STATUS_OBJECT_NAME_INVALID   ((NTSTATUS)0xC0000033)
#define STATUS_OBJECT_NAME_NOT_FOUND ((NTSTATUS)0xC0000034)
#define STATUS_OBJECT_NAME_COLLISION ((NTSTATUS)0xC0000035)
#define STATUS_ACCESS_DENIED_NT      ((NTSTATUS)0xC0000022)
#ifndef MUTANT_ALL_ACCESS
#define MUTANT_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 1)
#endif

typedef NTSTATUS (WINAPI *full_uex_fn)(const WCHAR *, ULONG, WCHAR *, WCHAR **, ULONG *);
typedef NTSTATUS (WINAPI *ns_fn)(HANDLE *, ACCESS_MASK, OBJECT_ATTRIBUTES *, void *);
typedef NTSTATUS (WINAPI *ns_del_fn)(HANDLE);
typedef NTSTATUS (WINAPI *mutant_fn)(HANDLE *, ACCESS_MASK, OBJECT_ATTRIBUTES *, BOOLEAN);
typedef NTSTATUS (WINAPI *open_mutant_fn)(HANDLE *, ACCESS_MASK, OBJECT_ATTRIBUTES *);
typedef void *(WINAPI *create_bd_fn)(const UNICODE_STRING *, ULONG);
typedef NTSTATUS (WINAPI *add_sid_fn)(void **, PSID);

static full_uex_fn pFullUEx;
static ns_fn pCreateNs, pOpenNs;
static ns_del_fn pDeleteNs;
static mutant_fn pCreateMutant;
static open_mutant_fn pOpenMutant;
static create_bd_fn pCreateBd;
static add_sid_fn pAddSid;

static void *boundary( const WCHAR *name, const char *sid_str )
{
    UNICODE_STRING str;
    void *bd;
    PSID sid = NULL;
    SID_IDENTIFIER_AUTHORITY world = { SECURITY_WORLD_SID_AUTHORITY }, nt = { SECURITY_NT_AUTHORITY };

    RtlInitUnicodeString( &str, name );
    bd = pCreateBd( &str, 0 );
    if (!strcmp( sid_str, "everyone" )) AllocateAndInitializeSid( &world, 1, 0, 0,0,0,0,0,0,0, &sid );
    else AllocateAndInitializeSid( &nt, 4, 21, 1, 2, 3, 0,0,0,0, &sid );   /* nobody's */
    pAddSid( &bd, sid );
    FreeSid( sid );
    return bd;
}

static NTSTATUS mutant( HANDLE root, const WCHAR *name, BOOL create, HANDLE *h )
{
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING str;
    RtlInitUnicodeString( &str, name );
    InitializeObjectAttributes( &attr, &str, 0, root, NULL );
    if (create) return pCreateMutant( h, MUTANT_ALL_ACCESS, &attr, FALSE );
    return pOpenMutant( h, MUTANT_ALL_ACCESS, &attr );
}

int main( int argc, char **argv )
{
    HMODULE ntdll = GetModuleHandleA( "ntdll.dll" );
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING empty = { 0 };
    HANDLE ns = 0, ns2 = 0, mx = 0, mx2 = 0;
    WCHAR buf[MAX_PATH], small[4], cwd[MAX_PATH], *part = NULL;
    ULONG bytes = 0;
    NTSTATUS st;
    void *bd;

    pFullUEx = (void *)GetProcAddress( ntdll, "RtlGetFullPathName_UEx" );
    pCreateNs = (void *)GetProcAddress( ntdll, "NtCreatePrivateNamespace" );
    pOpenNs = (void *)GetProcAddress( ntdll, "NtOpenPrivateNamespace" );
    pDeleteNs = (void *)GetProcAddress( ntdll, "NtDeletePrivateNamespace" );
    pCreateMutant = (void *)GetProcAddress( ntdll, "NtCreateMutant" );
    pOpenMutant = (void *)GetProcAddress( ntdll, "NtOpenMutant" );
    pCreateBd = (void *)GetProcAddress( ntdll, "RtlCreateBoundaryDescriptor" );
    pAddSid = (void *)GetProcAddress( ntdll, "RtlAddSIDToBoundaryDescriptor" );
    InitializeObjectAttributes( &attr, &empty, OBJ_CASE_INSENSITIVE, NULL, NULL );

    if (argc > 1 && !strcmp( argv[1], "child" ))
    {
        if (!pOpenNs) return 2;
        bd = boundary( L"SgProbe Boundary", "everyone" );
        if ((st = pOpenNs( &ns, DIRECTORY_ALL_ACCESS, &attr, bd ))) return 3;
        if ((st = mutant( ns, L"ProbeMutant", FALSE, &mx ))) return 4;
        return 0;
    }

    printf( "exports=%d\n", pFullUEx && pCreateNs && pOpenNs && pDeleteNs );
    if (!pFullUEx || !pCreateNs || !pOpenNs || !pDeleteNs) { printf( "done=1\n" ); return 0; }

    /* RtlGetFullPathName_UEx */
    GetCurrentDirectoryW( MAX_PATH, cwd );
    st = pFullUEx( L"probe-file.txt", sizeof(buf), buf, &part, &bytes );
    printf( "fullpath=%d\n", !st && bytes == wcslen( buf ) * sizeof(WCHAR) && part && !wcscmp( part, L"probe-file.txt" ) &&
            !wcsncmp( buf, cwd, wcslen( cwd ) ) );
    bytes = 0;
    st = pFullUEx( L"probe-file.txt", sizeof(small), small, NULL, &bytes );
    printf( "shortbuf=%d\n", !st && bytes == (wcslen( buf ) + 1) * sizeof(WCHAR) );
    st = pFullUEx( L"", sizeof(buf), buf, NULL, &bytes );
    printf( "invalid=%d\n", st == STATUS_OBJECT_NAME_INVALID && !bytes );

    /* private namespaces */
    bd = boundary( L"SgProbe Boundary", "everyone" );
    st = pOpenNs( &ns, DIRECTORY_ALL_ACCESS, &attr, bd );
    printf( "absent=%d\n", st == STATUS_OBJECT_NAME_NOT_FOUND );
    st = pCreateNs( &ns, DIRECTORY_ALL_ACCESS, &attr, bd );
    printf( "create=%d\n", !st && ns );
    st = pCreateNs( &ns2, DIRECTORY_ALL_ACCESS, &attr, bd );
    printf( "collision=%d\n", st == STATUS_OBJECT_NAME_COLLISION );
    st = mutant( ns, L"ProbeMutant", TRUE, &mx );
    st = st ? st : pOpenNs( &ns2, DIRECTORY_ALL_ACCESS, &attr, bd );
    st = st ? st : mutant( ns2, L"ProbeMutant", FALSE, &mx2 );
    printf( "sameobject=%d\n", !st && mx2 );
    {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        char cmd[MAX_PATH + 16];
        DWORD code = 99;
        GetModuleFileNameA( NULL, cmd, MAX_PATH );
        strcat( cmd, " child" );
        if (CreateProcessA( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
        {
            WaitForSingleObject( pi.hProcess, 30000 );
            GetExitCodeProcess( pi.hProcess, &code );
            CloseHandle( pi.hProcess ); CloseHandle( pi.hThread );
        }
        printf( "otherprocess=%d\n", code == 0 );
    }
    {
        void *other = boundary( L"SgProbe Boundary", "nobody" ), *named = boundary( L"Other Boundary", "everyone" );
        HANDLE h = 0;
        st = pCreateNs( &h, DIRECTORY_ALL_ACCESS, &attr, other );
        printf( "denied=%d\n", st == STATUS_ACCESS_DENIED_NT && !h );
        st = pOpenNs( &h, DIRECTORY_ALL_ACCESS, &attr, named );
        printf( "byname=%d\n", st == STATUS_OBJECT_NAME_NOT_FOUND );
    }
    printf( "delete=%d\n", !pDeleteNs( ns ) && pDeleteNs( (HANDLE)0xdead ) );
    CloseHandle( mx ); CloseHandle( mx2 ); CloseHandle( ns ); CloseHandle( ns2 );
    st = pOpenNs( &ns, DIRECTORY_ALL_ACCESS, &attr, bd );
    printf( "gone=%d\n", st == STATUS_OBJECT_NAME_NOT_FOUND );
    printf( "done=1\n" );
    return 0;
}
