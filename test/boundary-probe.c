/* Boundary descriptors and private namespaces (patches/sg/2204): all of
 * CreateBoundaryDescriptor*, AddSIDToBoundaryDescriptor,
 * AddIntegrityLabelToBoundaryDescriptor, DeleteBoundaryDescriptor,
 * Create/Open/ClosePrivateNamespace* were stubs (the descriptor NULL, the
 * namespace functions not even exported), so a program that keeps its
 * cross-process objects in a private namespace -- the way sandboxed
 * browsers and Office's add-in hosts do -- could never create them. */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static HANDLE (WINAPI *pCreateBoundaryDescriptorA)(const char *, ULONG);
static HANDLE (WINAPI *pCreateBoundaryDescriptorW)(const WCHAR *, ULONG);
static BOOL (WINAPI *pAddSIDToBoundaryDescriptor)(HANDLE *, PSID);
static BOOL (WINAPI *pAddIntegrityLabelToBoundaryDescriptor)(HANDLE *, PSID);
static void (WINAPI *pDeleteBoundaryDescriptor)(HANDLE);
static HANDLE (WINAPI *pCreatePrivateNamespaceA)(SECURITY_ATTRIBUTES *, void *, const char *);
static HANDLE (WINAPI *pCreatePrivateNamespaceW)(SECURITY_ATTRIBUTES *, void *, const WCHAR *);
static HANDLE (WINAPI *pOpenPrivateNamespaceA)(void *, const char *);
static HANDLE (WINAPI *pOpenPrivateNamespaceW)(void *, const WCHAR *);
static BOOLEAN (WINAPI *pClosePrivateNamespace)(HANDLE, ULONG);

static PSID current_user_sid(void)
{
    HANDLE token;
    TOKEN_USER *user;
    DWORD len = 0;
    PSID copy;

    OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &token );
    GetTokenInformation( token, TokenUser, NULL, 0, &len );
    user = HeapAlloc( GetProcessHeap(), 0, len );
    GetTokenInformation( token, TokenUser, user, len, &len );
    len = GetLengthSid( user->User.Sid );
    copy = HeapAlloc( GetProcessHeap(), 0, len );
    CopySid( len, copy, user->User.Sid );
    CloseHandle( token );
    HeapFree( GetProcessHeap(), 0, user );
    return copy;
}

int main(void)
{
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    SID_IDENTIFIER_AUTHORITY nt = {SECURITY_NT_AUTHORITY}, label = {SECURITY_MANDATORY_LABEL_AUTHORITY};
    PSID user = current_user_sid(), system_sid = NULL, low = NULL, high = NULL;
    HANDLE desc, desc2, ns, ns2, ns3, mutex, mutex2;
    BOOL ret;

#define LOAD(n) if (!(p##n = (void *)GetProcAddress( k32, #n ))) { printf("FAIL  %s is not exported\n", #n); return 1; }
    LOAD(CreateBoundaryDescriptorA) LOAD(CreateBoundaryDescriptorW) LOAD(AddSIDToBoundaryDescriptor)
    LOAD(AddIntegrityLabelToBoundaryDescriptor) LOAD(DeleteBoundaryDescriptor)
    LOAD(CreatePrivateNamespaceA) LOAD(CreatePrivateNamespaceW) LOAD(OpenPrivateNamespaceA)
    LOAD(OpenPrivateNamespaceW) LOAD(ClosePrivateNamespace)

    AllocateAndInitializeSid( &nt, 1, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &system_sid );
    AllocateAndInitializeSid( &label, 1, SECURITY_MANDATORY_LOW_RID, 0, 0, 0, 0, 0, 0, 0, &low );
    AllocateAndInitializeSid( &label, 1, SECURITY_MANDATORY_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &high );

    /* the descriptor itself */
    desc = pCreateBoundaryDescriptorW( L"SgBoundary", 0 );
    check( desc != NULL, "CreateBoundaryDescriptorW returns a descriptor (was NULL)" );
    SetLastError( 0xdeadbeef );
    check( !pCreateBoundaryDescriptorW( L"x", 0x80 ) && GetLastError() == ERROR_INVALID_PARAMETER,
           "unknown flags are ERROR_INVALID_PARAMETER" );
    SetLastError( 0xdeadbeef );
    check( !pCreateBoundaryDescriptorW( NULL, 0 ) && GetLastError() == ERROR_INVALID_PARAMETER,
           "a NULL name is ERROR_INVALID_PARAMETER" );
    desc2 = pCreateBoundaryDescriptorA( "SgBoundaryA", 0 );
    check( desc2 != NULL, "CreateBoundaryDescriptorA works" );
    pDeleteBoundaryDescriptor( desc2 );

    SetLastError( 0xdeadbeef );
    ret = pAddSIDToBoundaryDescriptor( &desc, (PSID)"junk" );
    check( !ret, "a malformed SID is rejected" );
    ret = pAddSIDToBoundaryDescriptor( &desc, user );
    check( ret, "AddSIDToBoundaryDescriptor adds the user's SID" );
    ret = pAddIntegrityLabelToBoundaryDescriptor( &desc, user );
    check( !ret, "a non-label SID is not an integrity label" );
    ret = pAddIntegrityLabelToBoundaryDescriptor( &desc, low );
    check( ret, "a mandatory label SID is accepted" );
    ret = pAddIntegrityLabelToBoundaryDescriptor( &desc, low );
    check( !ret, "only one integrity label per descriptor" );

    /* creating the namespace, and using it by its alias */
    SetLastError( 0xdeadbeef );
    ns = pCreatePrivateNamespaceW( NULL, desc, L"SgAlias" );
    check( ns != NULL, "CreatePrivateNamespaceW creates the namespace (function was not even exported)" );
    mutex = CreateMutexW( NULL, FALSE, L"SgAlias\\Mtx" );
    check( mutex != NULL, "an object named alias\\name can be created in it" );
    SetLastError( 0xdeadbeef );
    mutex2 = OpenMutexW( SYNCHRONIZE, FALSE, L"SgAlias\\Mtx" );
    check( mutex2 != NULL, "...and opened again by that name" );
    if (mutex2) CloseHandle( mutex2 );
    SetLastError( 0xdeadbeef );
    mutex2 = OpenMutexW( SYNCHRONIZE, FALSE, L"Mtx" );
    check( mutex2 == NULL, "the object is not visible outside the namespace" );

    SetLastError( 0xdeadbeef );
    ns2 = pCreatePrivateNamespaceW( NULL, desc, L"SgAlias" );
    check( ns2 == NULL && GetLastError() == ERROR_ALREADY_EXISTS,
           "creating an existing namespace again is ERROR_ALREADY_EXISTS" );
    ns2 = pOpenPrivateNamespaceW( desc, L"SgAlias" );
    check( ns2 != NULL, "OpenPrivateNamespaceW opens it" );
    ns3 = pOpenPrivateNamespaceA( desc, "SgAlias" );
    check( ns3 != NULL, "OpenPrivateNamespaceA opens it" );
    SetLastError( 0xdeadbeef );
    check( !pOpenPrivateNamespaceW( desc, L"SgNoSuchAlias" ) && GetLastError() != ERROR_SUCCESS,
           "opening a namespace that does not exist fails" );

    /* the boundary keeps outsiders out */
    {
        HANDLE foreign = pCreateBoundaryDescriptorW( L"SgForeign", 0 );
        pAddSIDToBoundaryDescriptor( &foreign, system_sid );
        SetLastError( 0xdeadbeef );
        check( !pCreatePrivateNamespaceW( NULL, foreign, L"SgForeignAlias" ) && GetLastError() == ERROR_ACCESS_DENIED,
               "a boundary naming a SID the caller lacks is ERROR_ACCESS_DENIED to create" );
        SetLastError( 0xdeadbeef );
        check( !pOpenPrivateNamespaceW( foreign, L"SgAlias" ) && GetLastError() == ERROR_ACCESS_DENIED,
               "...and to open" );
        pDeleteBoundaryDescriptor( foreign );

        foreign = pCreateBoundaryDescriptorW( L"SgHigh", 0 );
        pAddIntegrityLabelToBoundaryDescriptor( &foreign, high );
        SetLastError( 0xdeadbeef );
        check( !pCreatePrivateNamespaceW( NULL, foreign, L"SgHighAlias" ) && GetLastError() == ERROR_ACCESS_DENIED,
               "a boundary with a higher integrity label than the caller's is ERROR_ACCESS_DENIED" );
        pDeleteBoundaryDescriptor( foreign );
    }
    SetLastError( 0xdeadbeef );
    check( !pCreatePrivateNamespaceW( NULL, NULL, L"SgNullBoundary" ) && GetLastError() == ERROR_INVALID_PARAMETER,
           "a NULL boundary descriptor is ERROR_INVALID_PARAMETER" );

    /* closing */
    SetLastError( 0xdeadbeef );
    check( !pClosePrivateNamespace( ns2, 0x10 ) && GetLastError() == ERROR_INVALID_PARAMETER,
           "unknown close flags are ERROR_INVALID_PARAMETER" );
    check( pClosePrivateNamespace( ns2, 0 ), "ClosePrivateNamespace closes a handle" );
    check( pClosePrivateNamespace( ns3, 1 ), "...with the destroy flag too" );
    CloseHandle( mutex );
    check( pClosePrivateNamespace( ns, 1 ), "the creator's handle closes" );
    SetLastError( 0xdeadbeef );
    mutex2 = OpenMutexW( SYNCHRONIZE, FALSE, L"SgAlias\\Mtx" );
    check( mutex2 == NULL, "with every handle closed the namespace and its objects are gone" );
    SetLastError( 0xdeadbeef );
    check( !pOpenPrivateNamespaceW( desc, L"SgAlias" ), "...and it cannot be opened any more" );

    pDeleteBoundaryDescriptor( desc );
    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures ? 1 : 0;
}
