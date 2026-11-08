/* test/winord-gate.sh's probe: does a Windows ordinal of SHELL32, COMCTL32 or SHLWAPI lead to a
 * function Wine exports under another name? (7703, 7705). Prints each such ordinal as
 * "Mismatch=DLL.ORDINAL:WINDOWSNAMES:WINENAME", then "Mismatches=N".
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "winordinals.h"

static int check( const char *dll, const struct win_ordinal *table, unsigned int count )
{
    HMODULE module = LoadLibraryA( dll );
    BYTE *base = (BYTE *)module;
    IMAGE_NT_HEADERS *nt;
    IMAGE_EXPORT_DIRECTORY *exp;
    DWORD *names, *funcs, i, j;
    WORD *ords;
    int bad = 0;

    if (!module) return 0;
    nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    exp = (IMAGE_EXPORT_DIRECTORY *)(base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
    names = (DWORD *)(base + exp->AddressOfNames);
    ords = (WORD *)(base + exp->AddressOfNameOrdinals);
    funcs = (DWORD *)(base + exp->AddressOfFunctions);
    for (i = 0; i < count; i++)
    {
        DWORD index = table[i].ordinal - exp->Base;
        if (index >= exp->NumberOfFunctions || !funcs[index]) continue;
        /* a name exported for exactly this ordinal */
        for (j = 0; j < exp->NumberOfNames; j++)
        {
            const char *name = (const char *)(base + names[j]), *p;
            size_t len = strlen( name );
            BOOL known = FALSE;

            if (ords[j] != index) continue;
            for (p = table[i].names; p && *p; p = strchr( p, ' ' ) ? strchr( p, ' ' ) + 1 : NULL)
                if (!strncmp( p, name, len ) && (p[len] == ' ' || !p[len])) known = TRUE;
            /* Wine's own names for these Windows functions (same ordinal, same function) */
            if ((table[i].ordinal == 212 && !strcmp( name, "Printers_AddPrinterPropPages" )) ||
                (table[i].ordinal == 709 && !strcmp( name, "SHGetSetFolderCustomSettings" )) ||
                (table[i].ordinal == 53 && !strcmp( name, "DoEnvironmentSubst" )))
                known = TRUE;
            if (!known)
            {
                printf( "Mismatch=%s.%u:%s:%s\n", dll, table[i].ordinal, table[i].names, name );
                bad++;
            }
        }
    }
    return bad;
}

int main( void )
{
    int bad = check( "shell32.dll", shell32_ordinals, ARRAYSIZE(shell32_ordinals) );
    bad += check( "comctl32.dll", comctl32_ordinals, ARRAYSIZE(comctl32_ordinals) );
    bad += check( "shlwapi.dll", shlwapi_ordinals, ARRAYSIZE(shlwapi_ordinals) );
    printf( "Mismatches=%d\n", bad );
    return 0;
}
