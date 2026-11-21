/* concrtguid-probe: concrt140 exports the Concurrency Runtime ETW GUIDs as data (Stained Glass OS wine-sg 8259;
 * test/concrtguid-gate.sh); Bambu Studio, Plex and nomacs import PPLParallelForEventGuid, and a missing data
 * import left them reading through an invalid pointer.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static const struct { const char *name; GUID guid; } want[] =
{
    { "?PPLParallelForEventGuid@Concurrency@@3U_GUID@@B", { 0x31c8da6b, 0x6165, 0x4042, { 0x8b, 0x92, 0x94, 0x9e, 0x31, 0x5f, 0x4d, 0x84 } } },
    { "?ConcRT_ProviderGuid@Concurrency@@3U_GUID@@B", { 0xf7b697a3, 0x4db5, 0x4d3b, { 0xbe, 0x71, 0xc4, 0xd2, 0x84, 0xe6, 0x59, 0x2f } } },
    { "?AgentEventGuid@Concurrency@@3U_GUID@@B", { 0xb9b5b78c, 0x0713, 0x4898, { 0xa2, 0x1a, 0xc6, 0x79, 0x49, 0xdc, 0xed, 0x07 } } },
    { "?VirtualProcessorEventGuid@Concurrency@@3U_GUID@@B", { 0x2f27805f, 0x1676, 0x4ecc, { 0x96, 0xfa, 0x7e, 0xb0, 0x9d, 0x44, 0x30, 0x2f } } },
};

int main( void )
{
    HMODULE concrt = LoadLibraryA( "concrt140.dll" );
    int i, fails = 0;

    for (i = 0; i < ARRAYSIZE(want); i++)
    {
        const GUID *got = (const GUID *)GetProcAddress( concrt, want[i].name );
        BOOL ok = got && !memcmp( got, &want[i].guid, sizeof(GUID) );
        printf( "%s  %s\n", ok ? "PASS" : "FAIL", want[i].name );
        if (!ok) fails++;
    }
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
