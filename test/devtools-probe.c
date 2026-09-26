/* Probes for what developer tools need (patches/sg/0320 on); see
 * test/devtools-gate.sh. Each subcommand prints "key value" lines. */
#define _WIN32_WINNT 0x0a00
#define NTDDI_VERSION 0x0A000006
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <wincred.h>
#include <msi.h>
#include <msiquery.h>

static int conscp(void)
{
    BOOL ok;
    DWORD err;

    printf( "initial-output-cp %u\n", GetConsoleOutputCP() );
    SetLastError( 0xdeadbeef );
    ok = SetConsoleOutputCP( 65001 );
    printf( "set-output-cp %d\n", ok );
    printf( "output-cp %u\n", GetConsoleOutputCP() );
    ok = SetConsoleCP( 1252 );
    printf( "set-input-cp %d\n", ok );
    printf( "input-cp %u\n", GetConsoleCP() );
    SetLastError( 0xdeadbeef );
    ok = SetConsoleOutputCP( 12345 );
    err = GetLastError();
    printf( "set-bogus-cp %d error %lu\n", ok, err );
    printf( "output-cp-after-bogus %u\n", GetConsoleOutputCP() );
    return 0;
}

static UINT sql( MSIHANDLE db, const char *query )
{
    MSIHANDLE view;
    UINT r = MsiDatabaseOpenViewA( db, query, &view );
    if (r) { printf( "sql-error %u %s\n", r, query ); return r; }
    r = MsiViewExecute( view, 0 );
    if (r) printf( "sql-error %u %s\n", r, query );
    MsiViewClose( view );
    MsiCloseHandle( view );
    return r;
}

/* An MSI whose Environment table sets a per-user variable that refers to
 * another (%USERPROFILE%\go, as Go's installer does) and appends to PATH. */
static int msienv( const char *path )
{
    static const char *tables[] =
    {
        "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) NOT NULL PRIMARY KEY `Property`)",
        "CREATE TABLE `Directory` (`Directory` CHAR(72) NOT NULL, `Directory_Parent` CHAR(72), `DefaultDir` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Directory`)",
        "CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64), `Description` CHAR(255), `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)",
        "CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)",
        "CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)",
        "CREATE TABLE `Environment` (`Environment` CHAR(72) NOT NULL, `Name` CHAR(255) NOT NULL LOCALIZABLE, `Value` CHAR(255) LOCALIZABLE, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Environment`)",
        "CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)",
        "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductCode', '{5C1D9C4E-2F5B-4D41-9E7A-3B0C4E5D6F70}')",
        "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductName', 'SG env test')",
        "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductVersion', '1.0.0')",
        "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductLanguage', '1033')",
        "INSERT INTO `Property` (`Property`, `Value`) VALUES ('Manufacturer', 'Stained Glass')",
        "INSERT INTO `Directory` (`Directory`, `Directory_Parent`, `DefaultDir`) VALUES ('TARGETDIR', '', 'SourceDir')",
        "INSERT INTO `Feature` (`Feature`, `Feature_Parent`, `Title`, `Description`, `Display`, `Level`, `Directory_`, `Attributes`) VALUES ('F', '', '', '', 0, 1, 'TARGETDIR', 0)",
        "INSERT INTO `Component` (`Component`, `ComponentId`, `Directory_`, `Attributes`, `Condition`, `KeyPath`) VALUES ('C', '{8E3F1A2B-6C4D-4E5F-8A9B-0C1D2E3F4A5B}', 'TARGETDIR', 4, '', '')",
        "INSERT INTO `FeatureComponents` (`Feature_`, `Component_`) VALUES ('F', 'C')",
        "INSERT INTO `Environment` (`Environment`, `Name`, `Value`, `Component_`) VALUES ('E1', '=-SGTESTHOME', '%USERPROFILE%\\sgtest', 'C')",
        "INSERT INTO `Environment` (`Environment`, `Name`, `Value`, `Component_`) VALUES ('E2', '=-SGTESTPLAIN', 'C:\\plain', 'C')",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('CostInitialize', '', 800)",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('FileCost', '', 900)",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('CostFinalize', '', 1000)",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('InstallValidate', '', 1400)",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('InstallInitialize', '', 1500)",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('WriteEnvironmentStrings', '', 5200)",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('RegisterProduct', '', 6100)",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('PublishFeatures', '', 6300)",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('PublishProduct', '', 6400)",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('InstallFinalize', '', 6600)",
    };
    MSIHANDLE db, si;
    unsigned int i;
    UINT r;
    HKEY key;
    DWORD type, size;
    char data[512];

    DeleteFileA( path );
    if ((r = MsiOpenDatabaseA( path, MSIDBOPEN_CREATE, &db ))) { printf( "create %u\n", r ); return 1; }
    for (i = 0; i < ARRAYSIZE(tables); i++) if (sql( db, tables[i] )) return 1;
    MsiGetSummaryInformationA( db, NULL, 5, &si );
    MsiSummaryInfoSetPropertyA( si, PIDSI_TEMPLATE, VT_LPSTR, 0, NULL, "x64;1033" );
    MsiSummaryInfoSetPropertyA( si, PIDSI_REVNUMBER, VT_LPSTR, 0, NULL, "{2B7C9D1E-3F4A-4B5C-9D6E-7F8A9B0C1D2E}" );
    MsiSummaryInfoSetPropertyA( si, PIDSI_PAGECOUNT, VT_I4, 200, NULL, NULL );
    MsiSummaryInfoSetPropertyA( si, PIDSI_WORDCOUNT, VT_I4, 0, NULL, NULL );
    MsiSummaryInfoPersist( si );
    MsiCloseHandle( si );
    MsiDatabaseCommit( db );
    MsiCloseHandle( db );

    MsiSetInternalUI( INSTALLUILEVEL_NONE, NULL );
    r = MsiInstallProductA( path, "ALLUSERS=\"\"" );
    printf( "install %u\n", r );

    RegOpenKeyExA( HKEY_CURRENT_USER, "Environment", 0, KEY_READ, &key );
    size = sizeof(data);
    if (!RegQueryValueExA( key, "SGTESTHOME", NULL, &type, (BYTE *)data, &size ))
    {
        printf( "home-type %s\n", type == REG_EXPAND_SZ ? "REG_EXPAND_SZ" : type == REG_SZ ? "REG_SZ" : "other" );
    }
    else printf( "home-type missing\n" );
    size = sizeof(data);
    if (!RegQueryValueExA( key, "SGTESTPLAIN", NULL, &type, (BYTE *)data, &size ))
        printf( "plain-type %s\n", type == REG_EXPAND_SZ ? "REG_EXPAND_SZ" : type == REG_SZ ? "REG_SZ" : "other" );
    RegCloseKey( key );
    return 0;
}

static int msiremove( const char *path )
{
    MsiSetInternalUI( INSTALLUILEVEL_NONE, NULL );
    printf( "remove %u\n", MsiInstallProductA( path, "REMOVE=ALL" ) );
    return 0;
}

/* a new process sees the variable expanded (the environment built from HKCU\Environment) */
static int showenv( const char *name )
{
    char buf[512];
    DWORD n = GetEnvironmentVariableA( name, buf, sizeof(buf) );
    printf( "env %s\n", n ? buf : "(unset)" );
    return 0;
}

/* The C99 complex functions of ucrtbase (0322), called as a program built
 * with MSVC's <complex.h> calls them: 16-byte _Dcomplex returned through a
 * hidden pointer, 8-byte _Fcomplex in registers. */
typedef struct { double v[2]; } dcx;
typedef struct { float v[2]; } fcx;

static int close_to( double a, double b ) { return fabs( a - b ) < 1e-5; }

static int complex_( void )
{
    HMODULE crt = LoadLibraryA( "ucrtbase.dll" );
    dcx (__cdecl *pcsqrt)(dcx) = (void *)GetProcAddress( crt, "csqrt" );
    dcx (__cdecl *pcexp)(dcx) = (void *)GetProcAddress( crt, "cexp" );
    dcx (__cdecl *pcasin)(dcx) = (void *)GetProcAddress( crt, "casin" );
    dcx (__cdecl *pclog)(dcx) = (void *)GetProcAddress( crt, "clog" );
    dcx (__cdecl *pcpow)(dcx, dcx) = (void *)GetProcAddress( crt, "cpow" );
    dcx (__cdecl *pctan)(dcx) = (void *)GetProcAddress( crt, "ctan" );
    double (__cdecl *pcabs)(dcx) = (void *)GetProcAddress( crt, "cabs" );
    double (__cdecl *pcimag)(dcx) = (void *)GetProcAddress( crt, "cimag" );
    float (__cdecl *pcrealf)(fcx) = (void *)GetProcAddress( crt, "crealf" );
    float (__cdecl *pcimagf)(fcx) = (void *)GetProcAddress( crt, "cimagf" );
    fcx (__cdecl *pFCbuild)(float, float) = (void *)GetProcAddress( crt, "_FCbuild" );
    fcx (__cdecl *pcpowf)(fcx, fcx) = (void *)GetProcAddress( crt, "cpowf" );
    fcx (__cdecl *pconjf)(fcx) = (void *)GetProcAddress( crt, "conjf" );
    dcx z, r;
    fcx f, g;

    if (!pcsqrt || !pcexp || !pcasin || !pclog || !pcpow || !pctan || !pcabs || !pcimag ||
        !pcrealf || !pcimagf || !pFCbuild || !pcpowf || !pconjf)
    {
        printf( "complex missing\n" );
        return 1;
    }
    /* a stub aborts the process: each line is printed as it is reached */
    setvbuf( stdout, NULL, _IONBF, 0 );
    z.v[0] = 3; z.v[1] = 4;
    printf( "cabs %d\n", close_to( pcabs( z ), 5 ) );
    printf( "cimag %d\n", close_to( pcimag( z ), 4 ) );
    z.v[0] = -4; z.v[1] = 0; r = pcsqrt( z );
    printf( "csqrt %d\n", close_to( r.v[0], 0 ) && close_to( r.v[1], 2 ) );
    z.v[0] = 0; z.v[1] = M_PI; r = pcexp( z );
    printf( "cexp %d\n", close_to( r.v[0], -1 ) && close_to( r.v[1], 0 ) );
    z.v[0] = -1; z.v[1] = 0; r = pclog( z );
    printf( "clog %d\n", close_to( r.v[0], 0 ) && close_to( r.v[1], M_PI ) );
    z.v[0] = 0.5; z.v[1] = 0; r = pcasin( z );
    printf( "casin %d\n", close_to( r.v[0], asin( 0.5 ) ) && close_to( r.v[1], 0 ) );
    z.v[0] = 1; z.v[1] = 1; r = pctan( z );   /* tan(1+i) = 0.271753 + 1.083923i */
    printf( "ctan %d\n", close_to( r.v[0], 0.2717525853 ) && close_to( r.v[1], 1.0839233273 ) );
    {
        dcx i = { { 0, 1 } }, two = { { 2, 0 } };
        r = pcpow( i, two );
        printf( "cpow %d\n", close_to( r.v[0], -1 ) && close_to( r.v[1], 0 ) );
    }
    f = pFCbuild( 1.5f, -2.5f );
    printf( "fcbuild %d\n", f.v[0] == 1.5f && f.v[1] == -2.5f );
    printf( "crealf %d\n", pcrealf( f ) == 1.5f && pcimagf( f ) == -2.5f );
    g = pconjf( f );
    printf( "conjf %d\n", g.v[0] == 1.5f && g.v[1] == 2.5f );
    f.v[0] = 0; f.v[1] = 1; g.v[0] = 2; g.v[1] = 0; g = pcpowf( f, g );
    printf( "cpowf %d\n", close_to( g.v[0], -1 ) && close_to( g.v[1], 0 ) );
    return 0;
}

/* Git Credential Manager: CredEnumerate(NULL, CRED_ENUMERATE_ALL_CREDENTIALS) (0323) */
static int credenum( void )
{
    CREDENTIALW cred = { 0 };
    CREDENTIALW **creds;
    DWORD count = 0, i;
    BOOL ok, found = FALSE;

    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = (WCHAR *)L"git:https://sg-devtools.example";
    cred.UserName = (WCHAR *)L"someone";
    cred.CredentialBlob = (BYTE *)"secret";
    cred.CredentialBlobSize = 6;
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    printf( "write %d\n", CredWriteW( &cred, 0 ) );

    SetLastError( 0xdeadbeef );
    ok = CredEnumerateW( NULL, 1 /* CRED_ENUMERATE_ALL_CREDENTIALS */, &count, &creds );
    printf( "enum-all %d error %lu\n", ok, ok ? 0 : GetLastError() );
    if (ok)
    {
        for (i = 0; i < count; i++)
            if (!wcscmp( creds[i]->TargetName, L"git:https://sg-devtools.example" )) found = TRUE;
        CredFree( creds );
    }
    printf( "enum-all-found %d\n", found );
    SetLastError( 0xdeadbeef );
    ok = CredEnumerateW( L"git:*", 1, &count, &creds );
    printf( "enum-all-filter %d error %lu\n", ok, ok ? 0 : GetLastError() );
    SetLastError( 0xdeadbeef );
    ok = CredEnumerateW( NULL, 2, &count, &creds );
    printf( "enum-bad-flag %d error %lu\n", ok, ok ? 0 : GetLastError() );
    ok = CredEnumerateW( L"git:*", 0, &count, &creds );
    printf( "enum-filter %d count %lu\n", ok, ok ? count : 0 );
    if (ok) CredFree( creds );
    CredDeleteW( L"git:https://sg-devtools.example", CRED_TYPE_GENERIC, 0 );
    return 0;
}

/* A pseudo console made on named pipes before anyone has connected to them,
 * as node-pty does it (VS Code's terminal, 0324): CreatePseudoConsole first,
 * then the client ends are opened and the server ends connected, then the
 * shell started. What is written to the input must reach the shell. */
static int conpty_named( void )
{
    WCHAR in_name[64], out_name[64];
    HANDLE in_srv, out_srv, in_cli, out_cli;
    HPCON pc;
    COORD size = { 80, 25 };
    STARTUPINFOEXW si = { { sizeof(si) } };
    PROCESS_INFORMATION pi;
    SIZE_T attr_size = 0;
    WCHAR cmdline[] = L"cmd.exe";
    char buf[8192], all[65536] = "";
    DWORD n, avail, start;
    OVERLAPPED ov = { 0 };
    HRESULT hr;
    BOOL got = FALSE;

    swprintf( in_name, 64, L"\\\\.\\pipe\\sg-conpty-%lu-in", GetCurrentProcessId() );
    swprintf( out_name, 64, L"\\\\.\\pipe\\sg-conpty-%lu-out", GetCurrentProcessId() );
    in_srv = CreateNamedPipeW( in_name, PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
                               0, 1, 0, 0, 30000, NULL );
    out_srv = CreateNamedPipeW( out_name, PIPE_ACCESS_OUTBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
                                0, 1, 0, 0, 30000, NULL );
    hr = CreatePseudoConsole( size, in_srv, out_srv, 0, &pc );
    printf( "create %#lx\n", hr );
    if (FAILED(hr)) return 1;
    Sleep( 1000 );   /* the console host is up and reading before anyone connects */

    in_cli = CreateFileW( in_name, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL );
    out_cli = CreateFileW( out_name, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL );
    printf( "clients %d %d\n", in_cli != INVALID_HANDLE_VALUE, out_cli != INVALID_HANDLE_VALUE );
    ov.hEvent = CreateEventW( NULL, TRUE, FALSE, NULL );
    ConnectNamedPipe( in_srv, &ov );
    ConnectNamedPipe( out_srv, &ov );
    printf( "connected\n" );

    InitializeProcThreadAttributeList( NULL, 1, 0, &attr_size );
    si.lpAttributeList = HeapAlloc( GetProcessHeap(), 0, attr_size );
    InitializeProcThreadAttributeList( si.lpAttributeList, 1, 0, &attr_size );
    UpdateProcThreadAttribute( si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, pc, sizeof(pc), NULL, NULL );
    if (!CreateProcessW( NULL, cmdline, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
                         &si.StartupInfo, &pi ))
    {
        printf( "spawn failed %lu\n", GetLastError() );
        return 1;
    }
    printf( "spawned\n" );
    Sleep( 2000 );
    {
        /* on the old build nobody ever reads the input: do not wait forever */
        OVERLAPPED wov = { 0 };
        wov.hEvent = CreateEventW( NULL, TRUE, FALSE, NULL );
        n = 0;
        if (!WriteFile( in_cli, "echo SG-PTY-%OS%\r", 17, NULL, &wov ) && GetLastError() == ERROR_IO_PENDING &&
            WaitForSingleObject( wov.hEvent, 5000 )) CancelIo( in_cli );
        GetOverlappedResult( in_cli, &wov, &n, FALSE );
        printf( "wrote %lu\n", n );
    }
    for (start = GetTickCount(); GetTickCount() - start < 10000 && !got; )
    {
        if (PeekNamedPipe( out_cli, NULL, 0, NULL, &avail, NULL ) && avail)
        {
            ReadFile( out_cli, buf, min( avail, sizeof(buf) - 1 ), &n, NULL );
            buf[n] = 0;
            if (strlen( all ) + n < sizeof(all) - 1) strcat( all, buf );
            if (strstr( all, "SG-PTY-Windows_NT" )) got = TRUE;
        }
        else Sleep( 100 );
    }
    printf( "echoed %d\n", got );
    TerminateProcess( pi.hProcess, 0 );
    ClosePseudoConsole( pc );
    return 0;
}

int main( int argc, char **argv )
{
    setvbuf( stdout, NULL, _IONBF, 0 );
    if (argc >= 2 && !strcmp( argv[1], "conscp" )) return conscp();
    if (argc >= 2 && !strcmp( argv[1], "complex" )) return complex_();
    if (argc >= 2 && !strcmp( argv[1], "credenum" )) return credenum();
    if (argc >= 2 && !strcmp( argv[1], "conpty" )) return conpty_named();
    if (argc >= 3 && !strcmp( argv[1], "msienv" )) return msienv( argv[2] );
    if (argc >= 3 && !strcmp( argv[1], "msiremove" )) return msiremove( argv[2] );
    if (argc >= 3 && !strcmp( argv[1], "showenv" )) return showenv( argv[2] );
    fprintf( stderr, "usage: devtools-probe conscp | msienv FILE.msi | msiremove FILE.msi | showenv NAME | complex | credenum | conpty\n" );
    return 2;
}
