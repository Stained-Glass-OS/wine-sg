/*
 * eventlog-probe: drives the event log API for test/eventlog-gate.sh.
 *
 *   selftest            the functional checks, one PASS/FAIL line each
 *   report LOGORSOURCE TYPE ID [STRING...]   ReportEvent; "OK <n>" or "ERR <code>"
 *   open LOG            OpenEventLog; "OK" or "ERR <code>"
 *   register SOURCE     RegisterEventSource; "OK" or "ERR <code>"
 *   dump LOG            every record: "REC <n> <type> <id> <source> | <strings joined by |>"
 *   count LOG           "COUNT <n> OLDEST <n>"
 *   clear LOG [BACKUP]  ClearEventLog; "OK" or "ERR <code>"
 *   backup LOG FILE     BackupEventLog; "OK" or "ERR <code>"
 *   readbackup FILE     the backup's records, as dump
 *   notify LOG          waits (10 s) for a change: "NOTIFIED" or "TIMEOUT"
 *   flood SOURCE N SIZE FIRSTID   N events of SIZE characters, IDs from FIRSTID
 *
 * Copyright 2026 Stained Glass OS contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <sddl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails;

static void check( int ok, const char *what )
{
    printf( "%s %s\n", ok ? "PASS" : "FAIL", what );
    if (!ok) fails++;
}

static EVENTLOGRECORD *read_one( HANDLE h, DWORD flags, DWORD offset, BOOL ansi, DWORD *err )
{
    DWORD size = sizeof(EVENTLOGRECORD), read = 0, needed = 0;
    void *buf = malloc( size );
    BOOL ok = ansi ? ReadEventLogA( h, flags, offset, buf, size, &read, &needed )
                   : ReadEventLogW( h, flags, offset, buf, size, &read, &needed );
    if (!ok && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
        buf = realloc( buf, needed );
        ok = ansi ? ReadEventLogA( h, flags, offset, buf, needed, &read, &needed )
                  : ReadEventLogW( h, flags, offset, buf, needed, &read, &needed );
    }
    *err = ok ? 0 : GetLastError();
    if (!ok) { free( buf ); return NULL; }
    return buf;
}

static void print_record( const EVENTLOGRECORD *r )
{
    const WCHAR *src = (const WCHAR *)(r + 1), *s = (const WCHAR *)((const BYTE *)r + r->StringOffset);
    DWORD i;
    printf( "REC %lu %u %lu %ls |", r->RecordNumber, r->EventType, r->EventID & 0xffff, src );
    for (i = 0; i < r->NumStrings; i++) { printf( " %ls |", s ); s += wcslen( s ) + 1; }
    if (r->UserSidLength)
    {
        WCHAR *str;
        if (ConvertSidToStringSidW( (PSID)((const BYTE *)r + r->UserSidOffset), &str )) { printf( " SID %ls", str ); LocalFree( str ); }
    }
    printf( " DATA %lu\n", r->DataLength );
}

static void dump( HANDLE h )
{
    EVENTLOGRECORD *r;
    DWORD err;
    while ((r = read_one( h, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_FORWARDS_READ, 0, FALSE, &err )))
    {
        print_record( r );
        free( r );
    }
    printf( "END %lu\n", err );
}

static const WCHAR *record_string( const EVENTLOGRECORD *r, DWORD n )
{
    const WCHAR *s = (const WCHAR *)((const BYTE *)r + r->StringOffset);
    while (n--) s += wcslen( s ) + 1;
    return s;
}

static void selftest( void )
{
    static const WCHAR *strings[] = { L"first \x00e9 string", L"second string" };
    static const BYTE data[5] = { 1, 2, 3, 4, 5 };
    BYTE sidbuf[SECURITY_MAX_SID_SIZE];
    DWORD sidlen = sizeof(sidbuf), count, oldest, err, read, needed, n, first, last, i;
    WCHAR backup[MAX_PATH], backup2[MAX_PATH], tmp[MAX_PATH];
    EVENTLOGRECORD *r;
    HANDLE h, src, b;
    char *buf;

    GetTempPathW( MAX_PATH, tmp );
    swprintf( backup, MAX_PATH, L"%lsevlprobe-%lu.bak", tmp, GetCurrentProcessId() );
    swprintf( backup2, MAX_PATH, L"%lsevlprobe-%lu-2.bak", tmp, GetCurrentProcessId() );
    DeleteFileW( backup ); DeleteFileW( backup2 );
    CreateWellKnownSid( WinInteractiveSid, NULL, sidbuf, &sidlen );

    /* a registered source (the gate registers SgProbeSrc under SgProbe) */
    src = RegisterEventSourceW( NULL, L"SgProbeSrc" );
    check( src != NULL, "RegisterEventSource(SgProbeSrc)" );
    h = OpenEventLogW( NULL, L"SgProbe" );
    check( h != NULL, "OpenEventLog(SgProbe)" );
    check( ClearEventLogW( h, NULL ), "ClearEventLog(SgProbe) as an administrator" );
    GetNumberOfEventLogRecords( h, &count );
    check( count == 0, "cleared log is empty" );

    check( ReportEventW( src, EVENTLOG_WARNING_TYPE, 7, 1001, sidbuf, 2, sizeof(data), strings, (void *)data ),
           "ReportEvent with strings, SID and data" );
    for (i = 0; i < 4; i++)
    {
        WCHAR text[32]; const WCHAR *p = text;
        swprintf( text, 32, L"event %lu", i );
        ReportEventW( src, EVENTLOG_INFORMATION_TYPE, 0, 2000 + i, NULL, 1, 0, &p, NULL );
    }
    count = oldest = 0xdeadbeef;
    check( GetNumberOfEventLogRecords( h, &count ) && count == 5, "five records counted" );
    check( GetOldestEventLogRecord( h, &oldest ) && oldest >= 1 && oldest != 0xdeadbeef, "oldest record number" );
    first = oldest; last = oldest + 4;

    /* the first record, round trip */
    r = read_one( h, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_FORWARDS_READ, 0, FALSE, &err );
    check( r != NULL, "ReadEventLogW forwards" );
    if (r)
    {
        check( r->RecordNumber == first && r->EventID == 1001 && r->EventType == EVENTLOG_WARNING_TYPE &&
               r->EventCategory == 7 && r->NumStrings == 2, "record header fields" );
        check( !wcscmp( (WCHAR *)(r + 1), L"SgProbeSrc" ), "source name" );
        check( !wcscmp( record_string( r, 0 ), strings[0] ) && !wcscmp( record_string( r, 1 ), strings[1] ),
               "insertion strings (non-ASCII too)" );
        check( r->UserSidLength == sidlen && EqualSid( (BYTE *)r + r->UserSidOffset, sidbuf ), "user SID" );
        check( r->DataLength == sizeof(data) && !memcmp( (BYTE *)r + r->DataOffset, data, sizeof(data) ), "raw data" );
        check( *(DWORD *)((BYTE *)r + r->Length - 4) == r->Length && r->Reserved == 0x654c664c, "record framing" );
        check( r->TimeWritten >= r->TimeGenerated && r->TimeGenerated > 1700000000, "time written" );
        free( r );
    }
    /* sequential continues; buffer too small */
    buf = malloc( 65536 );
    read = needed = 0xdeadbeef;
    check( !ReadEventLogW( h, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_FORWARDS_READ, 0, buf, 8, &read, &needed ) &&
           GetLastError() == ERROR_INSUFFICIENT_BUFFER && read == 0 && needed > sizeof(EVENTLOGRECORD),
           "too small a buffer: ERROR_INSUFFICIENT_BUFFER and the size needed" );
    memset( buf, 0, 65536 );
    read = 0;
    check( ReadEventLogW( h, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_FORWARDS_READ, 0, buf, 65536, &read, &needed ) &&
           ((EVENTLOGRECORD *)buf)->RecordNumber == first + 1, "sequential read goes on from the last" );
    for (n = 0, i = 0; read <= 65536 && i + sizeof(EVENTLOGRECORD) <= read &&
                       ((EVENTLOGRECORD *)(buf + i))->Length; i += ((EVENTLOGRECORD *)(buf + i))->Length) n++;
    check( n == 4, "a large buffer takes every remaining record" );
    check( !ReadEventLogW( h, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_FORWARDS_READ, 0, buf, 65536, &read, &needed ) &&
           GetLastError() == ERROR_HANDLE_EOF, "then ERROR_HANDLE_EOF" );
    CloseEventLog( h );

    h = OpenEventLogW( NULL, L"SgProbe" );
    r = read_one( h, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_BACKWARDS_READ, 0, FALSE, &err );
    check( r && r->RecordNumber == last && r->EventID == 2003, "backwards read starts at the newest" );
    free( r );
    r = read_one( h, EVENTLOG_SEEK_READ | EVENTLOG_FORWARDS_READ, first + 2, FALSE, &err );
    check( r && r->RecordNumber == first + 2, "seek read" );
    free( r );
    r = read_one( h, EVENTLOG_SEEK_READ | EVENTLOG_FORWARDS_READ, last + 1, FALSE, &err );
    check( !r && err == ERROR_INVALID_PARAMETER, "seek past the end: ERROR_INVALID_PARAMETER" );
    r = read_one( h, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_FORWARDS_READ, 0, TRUE, &err );
    check( r && !strcmp( (char *)(r + 1), "SgProbeSrc" ), "ReadEventLogA converts the names" );
    free( r );
    r = read_one( h, EVENTLOG_SEEK_READ | EVENTLOG_FORWARDS_READ, first, TRUE, &err );
    check( r && !strcmp( (char *)r + r->StringOffset, "first \xe9 string" ) &&
           !memcmp( (BYTE *)r + r->DataOffset, data, sizeof(data) ) && r->UserSidLength == sidlen,
           "ReadEventLogA converts the strings and keeps SID and data" );
    free( r );

    /* an unregistered source lands in Application */
    {
        HANDLE u = RegisterEventSourceW( NULL, L"SgProbeUnregistered" ), app;
        const WCHAR *p = L"unregistered source text";
        DWORD c1 = 0, c2 = 0;
        app = OpenEventLogW( NULL, L"Application" );
        GetNumberOfEventLogRecords( app, &c1 );
        check( u && ReportEventW( u, EVENTLOG_ERROR_TYPE, 0, 42, NULL, 1, 0, &p, NULL ), "report from an unregistered source" );
        GetNumberOfEventLogRecords( app, &c2 );
        check( c2 == c1 + 1, "...is written to Application" );
        r = read_one( app, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_BACKWARDS_READ, 0, FALSE, &err );
        check( r && !wcscmp( (WCHAR *)(r + 1), L"SgProbeUnregistered" ) && !wcscmp( record_string( r, 0 ), p ),
               "...with its own source name and text" );
        free( r );
        DeregisterEventSource( u );
        CloseEventLog( app );
    }

    /* backups */
    check( BackupEventLogW( h, backup ), "BackupEventLog" );
    check( !BackupEventLogW( h, backup ) && GetLastError() == ERROR_ALREADY_EXISTS, "no overwriting a backup" );
    b = OpenBackupEventLogW( NULL, backup );
    check( b != NULL, "OpenBackupEventLog" );
    count = 0;
    check( GetNumberOfEventLogRecords( b, &count ) && count == 5, "the backup has the five records" );
    r = read_one( b, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_FORWARDS_READ, 0, FALSE, &err );
    check( r && r->EventID == 1001 && !wcscmp( record_string( r, 1 ), strings[1] ), "a backup's record reads back" );
    free( r );
    check( !ClearEventLogW( b, NULL ) && GetLastError() == ERROR_INVALID_HANDLE, "a backup cannot be cleared" );
    CloseEventLog( b );

    check( ClearEventLogW( h, backup2 ), "ClearEventLog with a backup" );
    count = 1;
    GetNumberOfEventLogRecords( h, &count );
    check( count == 0, "cleared" );
    b = OpenBackupEventLogW( NULL, backup2 );
    count = 0;
    check( b && GetNumberOfEventLogRecords( b, &count ) && count == 5, "the clearing's backup has the records" );
    CloseEventLog( b );
    {
        const WCHAR *p = L"after clear";
        ReportEventW( src, EVENTLOG_INFORMATION_TYPE, 0, 3000, NULL, 1, 0, &p, NULL );
        GetOldestEventLogRecord( h, &oldest );
        check( oldest == last + 1, "numbering goes on after a clear" );
    }
    check( !CloseEventLog( (HANDLE)0x1234 ) && GetLastError() == ERROR_INVALID_HANDLE, "a bogus handle" );
    CloseEventLog( h );
    check( !CloseEventLog( h ) && GetLastError() == ERROR_INVALID_HANDLE, "closing twice" );
    DeregisterEventSource( src );
    DeleteFileW( backup ); DeleteFileW( backup2 );
    free( buf );
    printf( "SELFTEST %s %d\n", fails ? "FAILED" : "OK", fails );
}

int wmain( int argc, WCHAR **argv )
{
    HANDLE h;
    DWORD err;

    setvbuf( stdout, NULL, _IONBF, 0 );
    if (argc < 2) return 2;
    if (!wcscmp( argv[1], L"selftest" )) { selftest(); return fails ? 1 : 0; }
    if (!wcscmp( argv[1], L"report" ) && argc >= 5)
    {
        const WCHAR **strs = (const WCHAR **)argv + 5;
        h = RegisterEventSourceW( NULL, argv[2] );
        if (!h) { printf( "ERR %lu\n", GetLastError() ); return 1; }
        if (!ReportEventW( h, (WORD)wcstoul( argv[3], NULL, 0 ), 0, wcstoul( argv[4], NULL, 0 ), NULL,
                           argc - 5, 0, strs, NULL ))
        { printf( "ERR %lu\n", GetLastError() ); return 1; }
        printf( "OK\n" );
        DeregisterEventSource( h );
        return 0;
    }
    if ((!wcscmp( argv[1], L"open" ) || !wcscmp( argv[1], L"register" )) && argc >= 3)
    {
        h = !wcscmp( argv[1], L"open" ) ? OpenEventLogW( NULL, argv[2] ) : RegisterEventSourceW( NULL, argv[2] );
        if (!h) { printf( "ERR %lu\n", GetLastError() ); return 1; }
        printf( "OK\n" );
        CloseEventLog( h );
        return 0;
    }
    if ((!wcscmp( argv[1], L"dump" ) || !wcscmp( argv[1], L"readbackup" ) || !wcscmp( argv[1], L"count" )) && argc >= 3)
    {
        DWORD count = 0, oldest = 0;
        h = !wcscmp( argv[1], L"readbackup" ) ? OpenBackupEventLogW( NULL, argv[2] ) : OpenEventLogW( NULL, argv[2] );
        if (!h) { printf( "ERR %lu\n", GetLastError() ); return 1; }
        if (!wcscmp( argv[1], L"count" ))
        {
            GetNumberOfEventLogRecords( h, &count );
            GetOldestEventLogRecord( h, &oldest );
            printf( "COUNT %lu OLDEST %lu\n", count, oldest );
        }
        else dump( h );
        CloseEventLog( h );
        return 0;
    }
    if ((!wcscmp( argv[1], L"clear" ) || !wcscmp( argv[1], L"backup" )) && argc >= 3)
    {
        BOOL ok;
        h = OpenEventLogW( NULL, argv[2] );
        if (!h) { printf( "ERR %lu\n", GetLastError() ); return 1; }
        ok = !wcscmp( argv[1], L"clear" ) ? ClearEventLogW( h, argc > 3 ? argv[3] : NULL )
                                          : BackupEventLogW( h, argc > 3 ? argv[3] : NULL );
        err = GetLastError();
        CloseEventLog( h );
        if (!ok) { printf( "ERR %lu\n", err ); return 1; }
        printf( "OK\n" );
        return 0;
    }
    if (!wcscmp( argv[1], L"flood" ) && argc >= 6)
    {
        DWORD i, n = wcstoul( argv[3], NULL, 0 ), len = wcstoul( argv[4], NULL, 0 ), id = wcstoul( argv[5], NULL, 0 );
        WCHAR *text = calloc( len + 1, sizeof(WCHAR) );
        const WCHAR *p = text;
        for (i = 0; i < len; i++) text[i] = 'x';
        h = RegisterEventSourceW( NULL, argv[2] );
        for (i = 0; h && i < n; i++)
            if (!ReportEventW( h, EVENTLOG_INFORMATION_TYPE, 0, id + i, NULL, 1, 0, &p, NULL )) break;
        printf( "%s %lu\n", i == n ? "OK" : "ERR", GetLastError() );
        return i == n ? 0 : 1;
    }
    if (!wcscmp( argv[1], L"notify" ) && argc >= 3)
    {
        HANDLE ev = CreateEventW( NULL, TRUE, FALSE, NULL );
        h = OpenEventLogW( NULL, argv[2] );
        if (!h || !NotifyChangeEventLog( h, ev )) { printf( "ERR %lu\n", GetLastError() ); return 1; }
        printf( "WAITING\n" );
        printf( "%s\n", WaitForSingleObject( ev, 10000 ) ? "TIMEOUT" : "NOTIFIED" );
        CloseEventLog( h );
        return 0;
    }
    return 2;
}
