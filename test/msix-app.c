/* msix-app: the application in test/msix-gate.sh's packages. It prints the
 * package identity it runs with, calls into the framework package it depends
 * on (a DLL that is only in the framework's folder, so it loads only through
 * the package graph), echoes its arguments and exits with 42.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <appmodel.h>
#include <stdio.h>
#include <stdlib.h>

__declspec(dllimport) int sg_framework_answer(void);

int main( int argc, char **argv )
{
    WCHAR name[256];
    UINT32 len = ARRAYSIZE(name);
    int i;

    if (GetCurrentPackageFullName( &len, name )) lstrcpyW( name, L"(none)" );
    printf( "package=%ls\n", name );
    printf( "framework=%d\n", sg_framework_answer() );
    {
        UINT32 size = 0, count = 0, j;
        BYTE *buffer;
        if (GetCurrentPackageInfo( PACKAGE_FILTER_HEAD | PACKAGE_FILTER_DIRECT, &size, NULL, &count ) == ERROR_INSUFFICIENT_BUFFER &&
            (buffer = malloc( size )) && !GetCurrentPackageInfo( PACKAGE_FILTER_HEAD | PACKAGE_FILTER_DIRECT, &size, buffer, &count ))
        {
            PACKAGE_INFO *info = (PACKAGE_INFO *)buffer;
            printf( "graph=%u\n", count );
            for (j = 0; j < count; j++)
                printf( "graph%u=%ls %s%s\n", j, info[j].packageFullName,
                        info[j].flags & PACKAGE_PROPERTY_FRAMEWORK ? "framework" : "main",
                        GetFileAttributesW( info[j].path ) & FILE_ATTRIBUTE_DIRECTORY ? "" : " (no folder)" );
        }
        else printf( "graph=error\n" );
    }
    for (i = 1; i < argc; i++) printf( "arg%d=%s\n", i, argv[i] );
    return 42;
}
