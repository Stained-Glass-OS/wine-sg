/* RtlDecompressFragment and ProcessAccessToken (patches/sg/2227).
 *
 * RtlDecompressFragment refuses a short uncompressed chunk (RtlDecompressBuffer
 * takes it) and gives zeroes up to the end of a chunk that ends before the
 * offset.  ProcessAccessToken refuses a token that is a process's token
 * (STATUS_TOKEN_ALREADY_IN_USE) and an impersonation token below the
 * impersonation level, and makes a primary token of one at or above it. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef struct { HANDLE Token; HANDLE Thread; } ACCESS_TOKEN_INFO;

static NTSTATUS (WINAPI *pRtlDecompressFragment)(USHORT, PUCHAR, ULONG, PUCHAR, ULONG, ULONG, PULONG, PVOID);
static NTSTATUS (WINAPI *pRtlDecompressBuffer)(USHORT, PUCHAR, ULONG, PUCHAR, ULONG, PULONG);
static NTSTATUS (WINAPI *pNtSetInformationProcess)(HANDLE, ULONG, void *, ULONG);

#define STATUS_BAD_COMPRESSION_BUFFER ((NTSTATUS)0xC0000242)
#define STATUS_TOKEN_ALREADY_IN_USE   ((NTSTATUS)0xC000012B)
#define STATUS_BAD_IMPERSONATION_LEVEL ((NTSTATUS)0xC00000A5)

static void test_lznt1(void)
{
    static const UCHAR short_uncompressed[] = { 0x03, 0x30, 'W', 'i', 'n', 'e' };
    static const UCHAR compressed_word[] = { 0x08, 0xB0, 0x00, 'W', 'i', 'n', 'e', 'W', 'i', 'n', 'e' };
    static const UCHAR empty_chunk[] = { 0x00, 0xB0, 0x02, 0x01 };
    UCHAR buf[8192], workspace[0x10000];
    NTSTATUS status;
    ULONG size;

    memset( buf, 0x11, sizeof(buf) );
    size = 0xdeadbeef;
    status = pRtlDecompressBuffer( COMPRESSION_FORMAT_LZNT1, buf, sizeof(buf), (UCHAR *)short_uncompressed, sizeof(short_uncompressed), &size );
    check( !status && size == 4 && !memcmp( buf, "Wine", 4 ), "RtlDecompressBuffer takes a short uncompressed chunk" );

    size = 0xdeadbeef;
    status = pRtlDecompressFragment( COMPRESSION_FORMAT_LZNT1, buf, sizeof(buf), (UCHAR *)short_uncompressed, sizeof(short_uncompressed), 0, &size, workspace );
    check( status == STATUS_BAD_COMPRESSION_BUFFER, "RtlDecompressFragment refuses it at offset 0" );
    status = pRtlDecompressFragment( COMPRESSION_FORMAT_LZNT1, buf, sizeof(buf), (UCHAR *)short_uncompressed, sizeof(short_uncompressed), 2, &size, workspace );
    check( status == STATUS_BAD_COMPRESSION_BUFFER, "and at offset 2" );

    /* a chunk skipped by the offset is not looked at */
    status = pRtlDecompressFragment( COMPRESSION_FORMAT_LZNT1, buf, sizeof(buf), (UCHAR *)short_uncompressed, sizeof(short_uncompressed), 4096, &size, workspace );
    check( !status && size == 0, "a short chunk before the offset is skipped" );

    /* compressed data: the rest of the chunk, or zeroes when it ends before the offset */
    memset( buf, 0x11, sizeof(buf) );
    status = pRtlDecompressFragment( COMPRESSION_FORMAT_LZNT1, buf, sizeof(buf), (UCHAR *)compressed_word, sizeof(compressed_word), 3, &size, workspace );
    check( !status && size == 5 && !memcmp( buf, "eWine", 5 ) && buf[5] == 0x11, "offset 3 inside the chunk: the remaining 5 bytes, no padding" );

    memset( buf, 0x11, sizeof(buf) );
    status = pRtlDecompressFragment( COMPRESSION_FORMAT_LZNT1, buf, sizeof(buf), (UCHAR *)compressed_word, sizeof(compressed_word), 4095, &size, workspace );
    check( !status && size == 1 && buf[0] == 0 && buf[1] == 0x11, "offset 4095 past the chunk's data: one zero byte" );

    memset( buf, 0x11, sizeof(buf) );
    status = pRtlDecompressFragment( COMPRESSION_FORMAT_LZNT1, buf, sizeof(buf), (UCHAR *)empty_chunk, sizeof(empty_chunk), 1, &size, workspace );
    check( !status && size == 4095 && buf[0] == 0 && buf[4094] == 0 && buf[4095] == 0x11, "empty chunk at offset 1: 4095 zero bytes" );
}

static void test_access_token(const char *self)
{
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    ACCESS_TOKEN_INFO info;
    HANDLE token, imp, prim;
    char cmd[MAX_PATH + 16];
    NTSTATUS status;

    sprintf( cmd, "\"%s\" idle", self );
    if (!CreateProcessA( NULL, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi ))
    {
        check( 0, "start a suspended process" );
        return;
    }
    if (!OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY | READ_CONTROL | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY |
                           TOKEN_ADJUST_PRIVILEGES | TOKEN_ADJUST_DEFAULT, &token ))
    {
        check( 0, "open our token" );
        TerminateProcess( pi.hProcess, 0 );
        return;
    }
    info.Thread = NULL;

    info.Token = token;
    status = pNtSetInformationProcess( pi.hProcess, 9 /* ProcessAccessToken */, &info, sizeof(info) );
    check( status == STATUS_TOKEN_ALREADY_IN_USE, "our own process token is STATUS_TOKEN_ALREADY_IN_USE" );

    DuplicateTokenEx( token, TOKEN_ALL_ACCESS, NULL, SecurityAnonymous, TokenImpersonation, &imp );
    info.Token = imp;
    status = pNtSetInformationProcess( pi.hProcess, 9, &info, sizeof(info) );
    check( status == STATUS_BAD_IMPERSONATION_LEVEL, "an anonymous-level impersonation token is STATUS_BAD_IMPERSONATION_LEVEL" );
    CloseHandle( imp );

    DuplicateTokenEx( token, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &imp );
    info.Token = imp;
    status = pNtSetInformationProcess( pi.hProcess, 9, &info, sizeof(info) );
    check( !status, "an impersonation-level impersonation token is taken" );
    {
        HANDLE ptoken;
        TOKEN_TYPE type = 0;
        DWORD len;
        BOOL ok = OpenProcessToken( pi.hProcess, TOKEN_QUERY, &ptoken );
        ok = ok && GetTokenInformation( ptoken, TokenType, &type, sizeof(type), &len );
        check( ok && type == TokenPrimary, "the process's token is a primary token" );
        if (ok) CloseHandle( ptoken );
    }
    CloseHandle( imp );

    DuplicateTokenEx( token, TOKEN_ALL_ACCESS, NULL, SecurityAnonymous, TokenPrimary, &prim );
    info.Token = prim;
    status = pNtSetInformationProcess( pi.hProcess, 9, &info, sizeof(info) );
    check( !status, "a fresh primary token is taken" );
    status = pNtSetInformationProcess( pi.hProcess, 9, &info, sizeof(info) );
    check( status == STATUS_TOKEN_ALREADY_IN_USE, "assigning that token again is STATUS_TOKEN_ALREADY_IN_USE" );
    CloseHandle( prim );

    CloseHandle( token );
    TerminateProcess( pi.hProcess, 0 );
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
}

int main(int argc, char **argv)
{
    HMODULE ntdll = GetModuleHandleA( "ntdll.dll" );

    if (argc > 1 && !strcmp( argv[1], "idle" )) return 0;

    pRtlDecompressFragment = (void *)GetProcAddress( ntdll, "RtlDecompressFragment" );
    pRtlDecompressBuffer = (void *)GetProcAddress( ntdll, "RtlDecompressBuffer" );
    pNtSetInformationProcess = (void *)GetProcAddress( ntdll, "NtSetInformationProcess" );

    test_lznt1();
    test_access_token( argv[0] );

    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures != 0;
}
