/* cmlogconf-probe: cfgmgr32 logical configurations and hardware profiles, which were spec stubs (Stained Glass
 * OS wine-sg 8246; test/cmlogconf-gate.sh): a device has no resource lists here (CR_NO_MORE_LOG_CONF, not a
 * crash), there is one hardware profile, the current one, not dockable.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

typedef DWORD CONFIGRET;
typedef DWORD_PTR LOG_CONF;
typedef struct { ULONG prof; WCHAR name[80]; DWORD flags; } HWPW;
typedef struct { ULONG prof; CHAR name[80]; DWORD flags; } HWPA;

static int failures;
static void check( int ok, const char *what, long got )
{
    printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

int main( void )
{
    HMODULE cm = LoadLibraryA( "cfgmgr32.dll" );
    CONFIGRET (WINAPI *pLocate)(DWORD *, WCHAR *, ULONG) = (void *)GetProcAddress( cm, "CM_Locate_DevNodeW" );
    CONFIGRET (WINAPI *pFirst)(LOG_CONF *, DWORD, ULONG) = (void *)GetProcAddress( cm, "CM_Get_First_Log_Conf" );
    CONFIGRET (WINAPI *pNext)(LOG_CONF *, LOG_CONF, ULONG) = (void *)GetProcAddress( cm, "CM_Get_Next_Log_Conf" );
    CONFIGRET (WINAPI *pFree)(LOG_CONF) = (void *)GetProcAddress( cm, "CM_Free_Log_Conf_Handle" );
    CONFIGRET (WINAPI *pProfW)(ULONG, HWPW *, ULONG) = (void *)GetProcAddress( cm, "CM_Get_Hardware_Profile_InfoW" );
    CONFIGRET (WINAPI *pProfA)(ULONG, HWPA *, ULONG) = (void *)GetProcAddress( cm, "CM_Get_Hardware_Profile_InfoA" );
    CONFIGRET (WINAPI *pFlagsW)(WCHAR *, ULONG, ULONG *, ULONG) = (void *)GetProcAddress( cm, "CM_Get_HW_Prof_FlagsW" );
    DWORD root = 0;
    LOG_CONF conf = 123;
    HWPW pw;
    HWPA pa;
    ULONG value = 99;
    CONFIGRET cr;

    if (!pFirst || !pNext || !pFree || !pProfW || !pProfA || !pFlagsW)
    {
        printf( "FAIL  the functions are exported\nRESULT: FAIL\n" );
        return 1;
    }
    cr = pLocate( &root, NULL, 0 );
    check( !cr, "CM_Locate_DevNode of the root", cr );
    cr = pFirst( &conf, root, 2 /* ALLOC_LOG_CONF */ );
    check( cr == 0x0e && !conf, "CM_Get_First_Log_Conf: CR_NO_MORE_LOG_CONF, handle cleared", cr );
    cr = pFirst( NULL, root, 3 /* BOOT_LOG_CONF */ );
    check( cr == 0x0e, "  with no handle pointer (existence test)", cr );
    cr = pFirst( &conf, root, 8 );
    check( cr == 0x04, "  unknown flags: CR_INVALID_FLAG", cr );
    cr = pFirst( &conf, 0xdeadbeef, 0 );
    check( cr == 0x05, "  bad devnode: CR_INVALID_DEVNODE", cr );
    check( pNext( &conf, 1, 0 ) == 0x07, "CM_Get_Next_Log_Conf: CR_INVALID_LOG_CONF", 0 );
    check( pFree( 1 ) == 0x07, "CM_Free_Log_Conf_Handle: CR_INVALID_LOG_CONF", 0 );

    memset( &pw, 0xcc, sizeof(pw) );
    cr = pProfW( 0xffffffff, &pw, 0 );
    printf( "      profile %lu \"%ls\" flags %lu\n", pw.prof, pw.name, pw.flags );
    check( !cr && pw.prof == 1 && !lstrcmpW( pw.name, L"Profile 1" ) && pw.flags == 0, "CM_Get_Hardware_Profile_InfoW: current profile", cr );
    cr = pProfA( 0, &pa, 0 );
    check( !cr && pa.prof == 1 && !strcmp( pa.name, "Profile 1" ), "  index 0 (ANSI)", cr );
    check( pProfW( 1, &pw, 0 ) == 0x23, "  index 1: CR_NO_MORE_HW_PROFILES", 0 );
    check( pProfW( 0, NULL, 0 ) == 0x03, "  no buffer: CR_INVALID_POINTER", 0 );
    cr = pFlagsW( NULL, 0, &value, 0 );
    check( !cr && value == 0, "CM_Get_HW_Prof_FlagsW of the root: no flags", cr );
    cr = pFlagsW( (WCHAR *)L"ROOT\\NO_SUCH\\0000", 0, &value, 0 );
    check( cr == 0x0d, "  a device that does not exist: CR_NO_SUCH_DEVINST", cr );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
