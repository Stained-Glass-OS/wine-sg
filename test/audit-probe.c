/*
 * audit-probe: reads the Security log as Event Viewer shows it, for
 * test/audit-gate.sh (patches/sg/0187).
 *
 *   audit-probe dump   every record: "REC <id> <type> <category> <time> <source>"
 *                      then "MSG <the formatted message, line ends as \n>"
 *                      and "CAT <the category's name>"; "ERR <code>" if the
 *                      log cannot be opened
 *
 * Messages are formatted as a viewer does: the source's EventMessageFile and
 * CategoryMessageFile under EventLog\Security, FormatMessage with the
 * record's strings.
 *
 * Copyright 2026 Stained Glass OS contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static HMODULE message_module( const WCHAR *source, const WCHAR *value )
{
    WCHAR key[256], file[MAX_PATH], path[MAX_PATH];
    DWORD size = sizeof(file);
    swprintf( key, 256, L"System\\CurrentControlSet\\Services\\EventLog\\Security\\%ls", source );
    if (RegGetValueW( HKEY_LOCAL_MACHINE, key, value, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND,
                      NULL, file, &size ))
        return NULL;
    ExpandEnvironmentStringsW( file, path, MAX_PATH );
    return LoadLibraryExW( path, NULL, LOAD_LIBRARY_AS_DATAFILE );
}

static void print_flat( const char *tag, const WCHAR *text )
{
    printf( "%s ", tag );
    for (; text && *text; text++)
    {
        if (*text == '\r') continue;
        if (*text == '\n') printf( "\\n" );
        else if (*text == '\t') printf( " " );
        else printf( "%lc", *text );
    }
    printf( "\n" );
}

int wmain( int argc, WCHAR **argv )
{
    HANDLE h;
    BYTE *buf = malloc( 65536 );
    DWORD read, needed;

    if (argc < 2 || wcscmp( argv[1], L"dump" )) return 2;
    if (!(h = OpenEventLogW( NULL, L"Security" )))
    {
        printf( "ERR %lu\n", GetLastError() );
        return 0;
    }
    while (ReadEventLogW( h, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_FORWARDS_READ, 0, buf, 65536, &read, &needed ))
    {
        BYTE *p = buf;
        while (p < buf + read)
        {
            EVENTLOGRECORD *r = (EVENTLOGRECORD *)p;
            const WCHAR *source = (const WCHAR *)(r + 1);
            DWORD_PTR args[100] = { 0 };
            WCHAR *s = (WCHAR *)(p + r->StringOffset), *msg = NULL;
            HMODULE mod;
            int i;

            for (i = 0; i < r->NumStrings && i < 100; i++) { args[i] = (DWORD_PTR)s; s += wcslen( s ) + 1; }
            for (; i < 100; i++) args[i] = (DWORD_PTR)L"";
            printf( "REC %lu %u %u %lu %ls\n", r->EventID, r->EventType, r->EventCategory, r->TimeGenerated, source );
            if ((mod = message_module( source, L"EventMessageFile" )))
            {
                FormatMessageW( FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ARGUMENT_ARRAY |
                                FORMAT_MESSAGE_ALLOCATE_BUFFER, mod, r->EventID, 0, (WCHAR *)&msg, 0,
                                (va_list *)args );
                FreeLibrary( mod );
            }
            print_flat( "MSG", msg );
            LocalFree( msg );
            msg = NULL;
            if (r->EventCategory && (mod = message_module( source, L"CategoryMessageFile" )))
            {
                FormatMessageW( FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS |
                                FORMAT_MESSAGE_ALLOCATE_BUFFER, mod, r->EventCategory, 0, (WCHAR *)&msg, 0, NULL );
                FreeLibrary( mod );
            }
            print_flat( "CAT", msg );
            LocalFree( msg );
            p += r->Length;
        }
    }
    CloseEventLog( h );
    return 0;
}
