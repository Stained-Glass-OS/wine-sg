/* tcpipreg-probe: what Windows programs see of the TCP/IP registry keys (patches/sg/9250).
 *   tcpipreg-probe dump          the adapters (GetAdaptersAddresses, GetAdaptersInfo) and their keys:
 *                                ADAPTER/INFO/VAL/PNP/CLASS/PARAM lines
 *   tcpipreg-probe wait MS       NOTIFY when Tcpip\Parameters\Interfaces changes within MS ms, else QUIET
 *   tcpipreg-probe write         READ ok|denied, WRITE ok|denied, CREATE ok|denied (as the caller)
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <string.h>

static const char interfaces[] = "System\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces";

static void dump_values( const char *tag, const char *path )
{
    char name[256];
    BYTE data[4096];
    DWORD i, nlen, dlen, type;
    HKEY key;

    if (RegOpenKeyExA( HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &key )) return;
    for (i = 0; nlen = sizeof(name), dlen = sizeof(data) - 2,
         !RegEnumValueA( key, i, name, &nlen, NULL, &type, data, &dlen ); i++)
    {
        data[dlen] = data[dlen + 1] = 0;
        if (type == REG_DWORD) printf( "%s %s dword %lu\n", tag, name, *(DWORD *)data );
        else if (type == REG_SZ) printf( "%s %s sz [%s]\n", tag, name, (char *)data );
        else if (type == REG_MULTI_SZ)
        {
            char *p;
            printf( "%s %s multi [", tag, name );
            for (p = (char *)data; *p; p += strlen( p ) + 1) printf( "%s%s", p == (char *)data ? "" : "|", p );
            printf( "]\n" );
        }
        else printf( "%s %s type%lu\n", tag, name, type );
    }
    RegCloseKey( key );
}

static BOOL get_sz( const char *path, const char *value, char *buf, DWORD size )
{
    DWORD type;
    HKEY key;
    BOOL ret;

    if (RegOpenKeyExA( HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &key )) return FALSE;
    ret = !RegQueryValueExA( key, value, NULL, &type, (BYTE *)buf, &size ) && type == REG_SZ;
    RegCloseKey( key );
    return ret;
}

static void dump(void)
{
    IP_ADAPTER_ADDRESSES *aa, *a;
    IP_ADAPTER_INFO *info, *i;
    ULONG size = 0;
    char path[512], tag[128], buf[512], name[64];
    DWORD n, len;
    HKEY key;

    GetAdaptersAddresses( AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, NULL, NULL, &size );
    aa = malloc( size );
    if (GetAdaptersAddresses( AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, NULL, aa, &size )) { printf( "GAA failed\n" ); return; }
    for (a = aa; a; a = a->Next)
    {
        char friendly[128];
        WideCharToMultiByte( CP_UTF8, 0, a->FriendlyName, -1, friendly, sizeof(friendly), NULL, NULL );
        printf( "ADAPTER %s type %lu name %s dhcpflag %d\n", a->AdapterName, a->IfType, friendly,
                !!(a->Flags & IP_ADAPTER_DHCP_ENABLED) );
    }
    size = 0;
    GetAdaptersInfo( NULL, &size );
    info = malloc( size );
    if (!GetAdaptersInfo( info, &size ))
        for (i = info; i; i = i->Next)
            printf( "INFO %s dhcp %u server [%s] obtained %lld expires %lld\n", i->AdapterName, i->DhcpEnabled,
                    i->DhcpServer.IpAddress.String, (long long)i->LeaseObtained, (long long)i->LeaseExpires );

    if (!RegOpenKeyExA( HKEY_LOCAL_MACHINE, interfaces, 0, KEY_READ, &key ))
    {
        for (n = 0; len = sizeof(name), !RegEnumKeyExA( key, n, name, &len, NULL, NULL, NULL, NULL ); n++)
        {
            char pnp[256], flags_path[512];
            DWORD flags, type, fsize = sizeof(flags);
            HKEY dev;

            printf( "KEY %s\n", name );
            snprintf( path, sizeof(path), "%s\\%s", interfaces, name );
            snprintf( tag, sizeof(tag), "VAL %s", name );
            dump_values( tag, path );
            /* as dnspython checks an adapter */
            snprintf( path, sizeof(path), "System\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\%s\\Connection", name );
            if (!get_sz( path, "PnpInstanceID", pnp, sizeof(pnp) )) { printf( "PNP %s none\n", name ); continue; }
            if (!get_sz( path, "Name", buf, sizeof(buf) )) strcpy( buf, "?" );
            snprintf( flags_path, sizeof(flags_path), "System\\CurrentControlSet\\Enum\\%s", pnp );
            if (RegOpenKeyExA( HKEY_LOCAL_MACHINE, flags_path, 0, KEY_READ, &dev )) { printf( "PNP %s %s noenum\n", name, pnp ); continue; }
            if (RegQueryValueExA( dev, "ConfigFlags", NULL, &type, (BYTE *)&flags, &fsize ) || type != REG_DWORD) flags = ~0u;
            if (!get_sz( flags_path, "Driver", tag, sizeof(tag) )) strcpy( tag, "?" );
            printf( "PNP %s %s flags %lu driver %s conn [%s]\n", name, pnp, flags, tag, buf );
            RegCloseKey( dev );
        }
        RegCloseKey( key );
    }
    if (!RegOpenKeyExA( HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}", 0, KEY_READ, &key ))
    {
        for (n = 0; len = sizeof(name), !RegEnumKeyExA( key, n, name, &len, NULL, NULL, NULL, NULL ); n++)
        {
            char id[64] = "", inst[256] = "";
            snprintf( path, sizeof(path), "System\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\%s", name );
            get_sz( path, "NetCfgInstanceId", id, sizeof(id) );
            get_sz( path, "DeviceInstanceID", inst, sizeof(inst) );
            printf( "CLASS %s %s %s\n", name, id, inst );
        }
        RegCloseKey( key );
    }
    dump_values( "PARAM", "System\\CurrentControlSet\\Services\\Tcpip\\Parameters" );
}

static int wait_change( DWORD ms )
{
    HANDLE event = CreateEventA( NULL, FALSE, FALSE, NULL );
    HKEY key;

    if (RegOpenKeyExA( HKEY_LOCAL_MACHINE, interfaces, 0, KEY_NOTIFY, &key )) { printf( "NOKEY\n" ); return 1; }
    RegNotifyChangeKeyValue( key, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, event, TRUE );
    printf( "ARMED\n" );
    fflush( stdout );
    printf( WaitForSingleObject( event, ms ) ? "QUIET\n" : "NOTIFY\n" );
    return 0;
}

static void try_write(void)
{
    char name[64], path[512];
    DWORD len = sizeof(name), err;
    HKEY key, sub;

    err = RegOpenKeyExA( HKEY_LOCAL_MACHINE, interfaces, 0, KEY_READ, &key );
    printf( "READ %s\n", err ? "denied" : "ok" );
    if (err) return;
    if (RegEnumKeyExA( key, 0, name, &len, NULL, NULL, NULL, NULL )) { printf( "NOADAPTER\n" ); return; }
    RegCloseKey( key );
    snprintf( path, sizeof(path), "%s\\%s", interfaces, name );
    err = RegOpenKeyExA( HKEY_LOCAL_MACHINE, path, 0, KEY_SET_VALUE, &key );
    if (!err) err = RegSetValueExA( key, "NameServer", 0, REG_SZ, (const BYTE *)"6.6.6.6", 8 );
    printf( "WRITE %s %lu\n", err ? "denied" : "ok", err );
    err = RegCreateKeyExA( HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\{sg-probe}",
                           0, NULL, 0, KEY_ALL_ACCESS, NULL, &sub, NULL );
    printf( "CREATE %s %lu\n", err ? "denied" : "ok", err );
    if (!err) { RegCloseKey( sub ); RegDeleteKeyA( HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\{sg-probe}" ); }
}

int main( int argc, char **argv )
{
    setvbuf( stdout, NULL, _IONBF, 0 );
    if (argc > 1 && !strcmp( argv[1], "dump" )) dump();
    else if (argc > 2 && !strcmp( argv[1], "wait" )) return wait_change( atoi( argv[2] ) );
    else if (argc > 1 && !strcmp( argv[1], "write" )) try_write();
    else { printf( "usage\n" ); return 2; }
    return 0;
}
