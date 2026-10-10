/* CreateService: a display name may match neither another service's name
 * nor another display name -- ERROR_DUPLICATE_SERVICE_NAME (patches/sg/2233). */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static SC_HANDLE make(SC_HANDLE scm, const char *name, const char *display)
{
    return CreateServiceA( scm, name, display, DELETE, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
                           SERVICE_ERROR_NORMAL, "C:\\windows\\system32\\sg_dup_probe.exe", NULL, NULL, NULL, NULL, NULL );
}

static void drop(SC_HANDLE h) { if (h) { DeleteService( h ); CloseServiceHandle( h ); } }

int main(void)
{
    SC_HANDLE scm = OpenSCManagerA( NULL, NULL, SC_MANAGER_ALL_ACCESS ), a, b;

    if (!scm) { printf( "FAIL  OpenSCManager %lu\n", GetLastError() ); return 1; }
    a = make( scm, "sgdup_a", "sgdup_a_display" );
    check( a != NULL, "create the first service" );

    SetLastError( 0 );
    b = make( scm, "sgdup_b", "sgdup_a" );
    check( !b && GetLastError() == ERROR_DUPLICATE_SERVICE_NAME, "a display name equal to another service's name" );
    drop( b );
    SetLastError( 0 );
    b = make( scm, "sgdup_c", "sgdup_a_display" );
    check( !b && GetLastError() == ERROR_DUPLICATE_SERVICE_NAME, "a display name equal to another display name" );
    drop( b );
    SetLastError( 0 );
    b = make( scm, "sgdup_a", NULL );
    check( !b && GetLastError() == ERROR_SERVICE_EXISTS, "the same service name" );
    drop( b );
    b = make( scm, "sgdup_d", "sgdup_d_display" );
    check( b != NULL, "unrelated names are fine" );
    drop( b );
    drop( a );
    CloseServiceHandle( scm );
    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures != 0;
}
