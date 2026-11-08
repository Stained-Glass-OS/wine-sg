/* test/shlwapictr-gate.sh's probe: SHLWAPI's ordinal-only exports keep Windows' numbers (7703).
 *
 *   ordinals       no named export sits on Windows' ordinals 552-645; 633 is not PathIsUNCServerShareW
 *   inc|get|dec N  SHGlobalCounterIncrement/GetValue/Decrement (ordinals 633/632/634) of counter N
 *   seq N          inc, inc, get, dec, dec, dec of counter N in this process
 *   hold N SECS    inc counter N, print it, then stay SECS seconds (the counter is shared meanwhile)
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

typedef LONG (WINAPI *counter_func)( int );

int main( int argc, char **argv )
{
    HMODULE shlwapi = LoadLibraryA( "shlwapi.dll" );
    BYTE *base = (BYTE *)shlwapi;

    if (!shlwapi || argc < 2) return 2;
    if (!strcmp( argv[1], "ordinals" ))
    {
        IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
        IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        IMAGE_EXPORT_DIRECTORY *exp = (IMAGE_EXPORT_DIRECTORY *)(base + dir->VirtualAddress);
        WORD *ords = (WORD *)(base + exp->AddressOfNameOrdinals);
        DWORD *names = (DWORD *)(base + exp->AddressOfNames), i, bad = 0;

        for (i = 0; i < exp->NumberOfNames; i++)
        {
            DWORD ordinal = ords[i] + exp->Base;
            /* SHCreateThreadWithHandle is 615 by name on Windows too */
            if (ordinal >= 552 && ordinal <= 645 && strcmp( (char *)(base + names[i]), "SHCreateThreadWithHandle" ))
            {
                if (!bad) printf( "First=%s.%lu\n", (char *)(base + names[i]), ordinal );
                bad++;
            }
        }
        printf( "NamedOnWindowsOrdinals=%lu\n", bad );
        printf( "Ordinal633IsPathIsUNCServerShareW=%d\n",
                GetProcAddress( shlwapi, (LPCSTR)633 ) == GetProcAddress( shlwapi, "PathIsUNCServerShareW" ) );
        printf( "Ordinal633=%s\n", GetProcAddress( shlwapi, (LPCSTR)633 ) ? "present" : "missing" );
    }
    else if (!strcmp( argv[1], "seq" ) && argc >= 3)
    {
        counter_func inc = (counter_func)GetProcAddress( shlwapi, (LPCSTR)633 );
        counter_func get = (counter_func)GetProcAddress( shlwapi, (LPCSTR)632 );
        counter_func dec = (counter_func)GetProcAddress( shlwapi, (LPCSTR)634 );
        int n = atoi( argv[2] );
        long a = inc( n ), b = inc( n ), c = get( n ), d = dec( n ), e = dec( n ), f = dec( n );
        printf( "Seq=%ld %ld %ld %ld %ld %ld\n", a, b, c, d, e, f );
    }
    else if (!strcmp( argv[1], "hold" ) && argc >= 4)
    {
        counter_func inc = (counter_func)GetProcAddress( shlwapi, (LPCSTR)633 );
        printf( "Value=%ld\n", inc( atoi( argv[2] ) ) );
        fflush( stdout );
        Sleep( atoi( argv[3] ) * 1000 );
    }
    else if (argc >= 3)
    {
        int ordinal = !strcmp( argv[1], "inc" ) ? 633 : !strcmp( argv[1], "get" ) ? 632 : 634;
        counter_func func = (counter_func)GetProcAddress( shlwapi, (LPCSTR)(ULONG_PTR)ordinal );
        if (!func) { printf( "Value=(missing)\n" ); return 1; }
        printf( "Value=%ld\n", func( atoi( argv[2] ) ) );
    }
    return 0;
}
