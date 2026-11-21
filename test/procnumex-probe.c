/* procnumex-probe: ntdll RtlGetCurrentProcessorNumberEx / kernel32 GetCurrentProcessorNumberEx (Stained Glass
 * OS wine-sg 8262; test/procnumex-gate.sh): group 0 and the current processor, without a FIXME line on every
 * call (the gate counts them on stderr). Anki, Audacity, Blender, LibreOffice, MuseScore, Telegram and other
 * Qt programs import it.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

int main( void )
{
    void (WINAPI *pRtl)(PROCESSOR_NUMBER *) = (void *)GetProcAddress( GetModuleHandleA( "ntdll.dll" ), "RtlGetCurrentProcessorNumberEx" );
    PROCESSOR_NUMBER pn;
    DWORD count = GetActiveProcessorCount( ALL_PROCESSOR_GROUPS );
    int i, ok = 1;

    for (i = 0; i < 1000; i++)
    {
        memset( &pn, 0xcc, sizeof(pn) );
        pRtl( &pn );
        if (pn.Group != 0 || pn.Number >= count || pn.Reserved != 0) ok = 0;
    }
    GetCurrentProcessorNumberEx( &pn );
    printf( "%s  group 0, a processor below %lu, 1000 calls (last %u/%u)\n", ok ? "PASS" : "FAIL", count, pn.Group, pn.Number );
    printf( "RESULT: %s\n", ok ? "PASS" : "FAIL" );
    return !ok;
}
