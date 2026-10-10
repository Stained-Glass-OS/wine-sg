/* Performance keys, RegQueryInfoKey and application hives (patches/sg/2226).
 * RegLoadAppKey was a stub returning the handle 0xdeadbeef; it now loads a
 * private hive from the file (made if missing), writes it back when the
 * root handle is closed and unloads it, all without privileges. */
#include <windows.h>
#include <winternl.h>
#include <winperf.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static NTSTATUS (WINAPI *pNtOpenKey)(HANDLE *, ACCESS_MASK, OBJECT_ATTRIBUTES *);
static NTSTATUS (WINAPI *pNtEnumerateKey)(HANDLE, ULONG, int, void *, ULONG, ULONG *);
#ifndef STATUS_INVALID_PARAMETER_7
#define STATUS_INVALID_PARAMETER_7 ((NTSTATUS)0xC00000F5)
#endif
static NTSTATUS (WINAPI *pNtLoadKeyEx)(OBJECT_ATTRIBUTES *, OBJECT_ATTRIBUTES *, ULONG, HANDLE, HANDLE, ACCESS_MASK, HANDLE *, IO_STATUS_BLOCK *);
static NTSTATUS (WINAPI *pRtlDosPathNameToNtPathName_U_WithStatus)(const WCHAR *, UNICODE_STRING *, WCHAR **, void *);

/* subkeys of \Registry\A */
static int count_app_hives(void)
{
    UNICODE_STRING name;
    OBJECT_ATTRIBUTES attr;
    HANDLE key;
    char buf[512];
    ULONG len, i = 0;
    RtlInitUnicodeString( &name, L"\\Registry\\A" );
    InitializeObjectAttributes( &attr, &name, OBJ_CASE_INSENSITIVE, NULL, NULL );
    if (pNtOpenKey( &key, KEY_ENUMERATE_SUB_KEYS, &attr )) return -1;
    while (!pNtEnumerateKey( key, i, 0 /* KeyBasicInformation */, buf, sizeof(buf), &len )) i++;
    CloseHandle( key );
    return i;
}

static void test_perf_keys(void)
{
    static const HKEY keys[] = { HKEY_PERFORMANCE_DATA, HKEY_PERFORMANCE_TEXT, HKEY_PERFORMANCE_NLSTEXT };
    char name[64], *buf = HeapAlloc( GetProcessHeap(), 0, 65536 );
    DWORD size, ret, subkeys, values;
    PERF_DATA_BLOCK *data;
    HKEY key;
    int i;

    for (i = 0; i < 3; i++)
    {
        char ctx[64];
        sprintf( ctx, "[%p] ", keys[i] );
        key = (HKEY)0x1234;
        ret = RegOpenKeyA( keys[i], NULL, &key );
        sprintf( name, "%sRegOpenKey(NULL) is ERROR_INVALID_HANDLE", ctx );
        check( ret == ERROR_INVALID_HANDLE, name );

        subkeys = 77; values = 0;
        ret = RegQueryInfoKeyA( keys[i], NULL, NULL, NULL, &subkeys, NULL, NULL, &values, NULL, NULL, NULL, NULL );
        sprintf( name, "%sRegQueryInfoKey: success, no subkeys, two values", ctx );
        check( !ret && !subkeys && values == 2, name );

        size = 1024;
        ret = RegEnumValueA( keys[i], 0, buf, &size, NULL, NULL, NULL, NULL );
        sprintf( name, "%sRegEnumValue(0) is ERROR_MORE_DATA, size untouched", ctx );
        check( ret == ERROR_MORE_DATA && size == 1024, name );

        ret = RegSetValueExA( keys[i], "Global", 0, REG_SZ, (const BYTE *)"dummy", 5 );
        sprintf( name, "%sRegSetValueEx", ctx );
        check( ret == (i == 0 ? ERROR_INVALID_HANDLE : ERROR_BADKEY), name );

        ret = RegEnumKeyA( keys[i], 0, buf, 64 );
        sprintf( name, "%sRegEnumKey", ctx );
        check( ret == (i == 0 ? ERROR_INVALID_HANDLE : ERROR_NO_MORE_ITEMS), name );
    }

    /* a too small buffer keeps its size, the system object is the default */
    size = 10;
    ret = RegQueryValueExA( HKEY_PERFORMANCE_DATA, "Global", NULL, NULL, (BYTE *)buf, &size );
    check( ret == ERROR_MORE_DATA && size == 10, "RegQueryValueEx with 10 bytes: MORE_DATA and size stays 10" );
    size = 65536;
    ret = RegQueryValueExA( HKEY_PERFORMANCE_DATA, "Global", NULL, NULL, (BYTE *)buf, &size );
    data = (PERF_DATA_BLOCK *)buf;
    check( !ret && data->DefaultObject == 238, "perf data: the default object is the System object (238)" );
    HeapFree( GetProcessHeap(), 0, buf );
}

static void test_query_info(void)
{
    HKEY key;
    DWORD sdlen, classlen, ret;
    char cls[16];

    ret = RegCreateKeyExA( HKEY_CURRENT_USER, "Software\\Wine\\SgRegProbe", 0, (char *)"MyClass", 0, KEY_ALL_ACCESS, NULL, &key, NULL );
    check( !ret, "make a key with a class" );
    sdlen = 0;
    ret = RegQueryInfoKeyA( key, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &sdlen, NULL );
    check( !ret && sdlen >= 20, "RegQueryInfoKey reports the security descriptor's size" );
    sdlen = 0;
    ret = RegQueryInfoKeyW( key, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &sdlen, NULL );
    check( !ret && sdlen >= 20, "RegQueryInfoKeyW too" );

    memset( cls, 0x55, sizeof(cls) );
    classlen = 0;
    ret = RegQueryInfoKeyA( key, cls, &classlen, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL );
    check( !ret && classlen == 0 && (unsigned char)cls[0] == 0x55, "a class buffer of size 0: success, size 0, buffer untouched" );
    classlen = 0;
    ret = RegQueryInfoKeyA( key, NULL, &classlen, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL );
    check( !ret && classlen == 7, "no class buffer: the class length is returned" );
    RegCloseKey( key );
    RegDeleteKeyA( HKEY_CURRENT_USER, "Software\\Wine\\SgRegProbe" );
}

static void test_app_hive(void)
{
    char path[MAX_PATH], buf[64], text[4096];
    HANDLE file;
    HKEY key = NULL, sub;
    DWORD ret, size, read, before, now;
    UNICODE_STRING nt_file, name;
    OBJECT_ATTRIBUTES attr, fattr;
    NTSTATUS status;
    HANDLE root = (HANDLE)0x1234;

    GetTempPathA( MAX_PATH, path );
    strcat( path, "sg_apphive_probe.dat" );
    DeleteFileA( path );
    before = count_app_hives();
    check( before >= 0, "\\Registry\\A exists" );

    SetLastError( 0 );
    ret = RegLoadAppKeyA( NULL, &key, KEY_READ, 0, 0 );
    check( ret == ERROR_INVALID_PARAMETER, "NULL file is ERROR_INVALID_PARAMETER" );
    ret = RegLoadAppKeyA( path, &key, KEY_READ, 0, 1 );
    check( ret == ERROR_INVALID_PARAMETER, "reserved != 0 is ERROR_INVALID_PARAMETER" );
    ret = RegLoadAppKeyA( path, &key, KEY_READ, 0x100, 0 );
    check( ret == ERROR_INVALID_PARAMETER, "unknown option bits are ERROR_INVALID_PARAMETER" );
    check( GetFileAttributesA( path ) == INVALID_FILE_ATTRIBUTES, "failed calls did not make the file" );

    /* a file that is not there is made */
    key = NULL;
    ret = RegLoadAppKeyA( path, &key, KEY_ALL_ACCESS, 0, 0 );
    check( !ret && key && key != (HKEY)0xdeadbeef, "load of a new file succeeds with a real handle" );
    check( GetFileAttributesA( path ) != INVALID_FILE_ATTRIBUTES, "the hive file was made" );
    check( count_app_hives() == before + 1, "the hive is loaded under \\Registry\\A" );

    ret = RegSetValueExA( key, "name", 0, REG_SZ, (const BYTE *)"value one", 10 );
    check( !ret, "write a value" );
    ret = RegCreateKeyExA( key, "child\\grandchild", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &sub, NULL );
    check( !ret, "make subkeys" );
    if (!ret)
    {
        RegSetValueExA( sub, "deep", 0, REG_DWORD, (const BYTE *)&(DWORD){ 0x1234 }, 4 );
        RegCloseKey( sub );
    }
    else printf( "      (RegCreateKeyEx error %lu)\n", ret );
    ret = RegCloseKey( key );
    check( !ret, "close the root handle" );
    check( count_app_hives() == before, "the hive is unloaded on close" );

    file = CreateFileA( path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL );
    check( file != INVALID_HANDLE_VALUE, "the file is closed again (exclusive open works)" );
    if (file != INVALID_HANDLE_VALUE)
    {
        memset( text, 0, sizeof(text) );
        ReadFile( file, text, sizeof(text) - 1, &read, NULL );
        CloseHandle( file );
        check( strstr( text, "value one" ) != NULL, "the value was written to the file" );
    }

    /* read back, read only: the file does not change */
    ret = RegLoadAppKeyA( path, &key, KEY_READ, 0, 0 );
    check( !ret, "load it again" );
    size = sizeof(buf);
    ret = RegGetValueA( key, NULL, "name", RRF_RT_REG_SZ, NULL, buf, &size );
    check( !ret && !strcmp( buf, "value one" ), "the value reads back" );
    size = sizeof(buf);
    ret = RegGetValueA( key, "child\\grandchild", "deep", RRF_RT_REG_DWORD, NULL, buf, &size );
    check( !ret && *(DWORD *)buf == 0x1234, "the nested value reads back" );
    ret = RegSetValueExA( key, "nope", 0, REG_SZ, (const BYTE *)"x", 2 );
    check( ret == ERROR_ACCESS_DENIED, "a read-only handle cannot write" );
    RegCloseKey( key );

    /* loaded without privileges: NtLoadKeyEx root handle rules */
    pRtlDosPathNameToNtPathName_U_WithStatus( (WCHAR[]){ 0 }, &nt_file, NULL, NULL );
    {
        WCHAR wpath[MAX_PATH];
        MultiByteToWideChar( CP_ACP, 0, path, -1, wpath, MAX_PATH );
        pRtlDosPathNameToNtPathName_U_WithStatus( wpath, &nt_file, NULL, NULL );
    }
    InitializeObjectAttributes( &fattr, &nt_file, OBJ_CASE_INSENSITIVE, NULL, NULL );
    RtlInitUnicodeString( &name, L"\\REGISTRY\\A\\ProbeKey" );
    InitializeObjectAttributes( &attr, &name, OBJ_CASE_INSENSITIVE, NULL, NULL );
    status = pNtLoadKeyEx( &attr, &fattr, 0, NULL, NULL, KEY_READ, &root, NULL );
    check( status == STATUS_INVALID_PARAMETER_7 && root == (HANDLE)0x1234, "NtLoadKeyEx: a root handle without REG_APP_HIVE is STATUS_INVALID_PARAMETER_7" );
    RtlInitUnicodeString( &name, L"\\REGISTRY\\MACHINE\\ProbeKey" );
    status = pNtLoadKeyEx( &attr, &fattr, 0x10, NULL, NULL, KEY_READ, &root, NULL );
    check( status == STATUS_INVALID_PARAMETER, "NtLoadKeyEx: an app hive outside \\Registry\\A is refused" );
    RtlFreeUnicodeString( &nt_file );

    now = count_app_hives();
    check( now == before, "no hive is left loaded" );
    DeleteFileA( path );
}

int main(void)
{
    HMODULE ntdll = GetModuleHandleA( "ntdll.dll" );
    pNtOpenKey = (void *)GetProcAddress( ntdll, "NtOpenKey" );
    pNtEnumerateKey = (void *)GetProcAddress( ntdll, "NtEnumerateKey" );
    pNtLoadKeyEx = (void *)GetProcAddress( ntdll, "NtLoadKeyEx" );
    pRtlDosPathNameToNtPathName_U_WithStatus = (void *)GetProcAddress( ntdll, "RtlDosPathNameToNtPathName_U_WithStatus" );

    test_perf_keys();
    test_query_info();
    test_app_hive();

    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures != 0;
}
