/* pkgname-probe: the app-model package name functions (kernel32/kernelbase)
 * against known answers, for test/appx-gate.sh. Resolved at run time so the
 * probe needs no appmodel.h.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>

typedef struct
{
    UINT32 reserved, processorArchitecture;
    UINT64 version;           /* PACKAGE_VERSION: revision, build, minor, major (low to high) */
    WCHAR *name, *publisher, *resourceId, *publisherId;
} PACKAGE_ID_;

static void printw( const char *key, const WCHAR *w )
{
    char buf[512];
    WideCharToMultiByte( CP_UTF8, 0, w, -1, buf, sizeof(buf), NULL, NULL );
    printf( "%s=%s\n", key, buf );
}

int main( void )
{
    HMODULE k32 = GetModuleHandleA( "kernel32.dll" ), kb = LoadLibraryA( "kernelbase.dll" );
    LONG (WINAPI *family_from_full)( const WCHAR *, UINT32 *, WCHAR * ) = (void *)GetProcAddress( k32, "PackageFamilyNameFromFullName" );
    LONG (WINAPI *full_from_id)( const PACKAGE_ID_ *, UINT32 *, WCHAR * ) = (void *)GetProcAddress( k32, "PackageFullNameFromId" );
    LONG (WINAPI *name_and_pub)( const WCHAR *, UINT32 *, WCHAR *, UINT32 *, WCHAR * ) =
        (void *)GetProcAddress( k32, "PackageNameAndPublisherIdFromFamilyName" );
    LONG (WINAPI *verify_full)( const WCHAR * ) = (void *)GetProcAddress( kb, "VerifyPackageFullName" );
    WCHAR buf[256], buf2[64];
    UINT32 len, len2;
    PACKAGE_ID_ id = {0};
    LONG r;

    if (!family_from_full || !full_from_id || !name_and_pub || !verify_full) { printf( "missing=exports\n" ); return 1; }

    len = ARRAYSIZE(buf);
    r = family_from_full( L"Microsoft.Winget.Source_2026.924.610.8_neutral__8wekyb3d8bbwe", &len, buf );
    if (r) printf( "FamilyFromFull=ERROR %ld\n", r ); else printw( "FamilyFromFull", buf );

    id.processorArchitecture = 11;    /* neutral */
    id.version = (2026ull << 48) | (924ull << 32) | (610ull << 16) | 8;
    id.name = (WCHAR *)L"Microsoft.Winget.Source";
    id.publisher = (WCHAR *)L"CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US";
    len = ARRAYSIZE(buf);
    r = full_from_id( &id, &len, buf );
    if (r) printf( "FullFromId=ERROR %ld\n", r ); else printw( "FullFromId", buf );

    len = ARRAYSIZE(buf);
    len2 = ARRAYSIZE(buf2);
    r = name_and_pub( L"Microsoft.Winget.Source_8wekyb3d8bbwe", &len, buf, &len2, buf2 );
    if (r) printf( "NameAndPublisherId=ERROR %ld\n", r );
    else
    {
        char a[256], b[64];
        WideCharToMultiByte( CP_UTF8, 0, buf, -1, a, sizeof(a), NULL, NULL );
        WideCharToMultiByte( CP_UTF8, 0, buf2, -1, b, sizeof(b), NULL, NULL );
        printf( "NameAndPublisherId=%s|%s\n", a, b );
    }
    printf( "VerifyBad=%ld\n", verify_full( L"not_a_full_name" ) );
    fflush( stdout );
    return 0;
}
