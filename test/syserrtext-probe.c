/* syserrtext-probe (patches/sg/9750, test/syserrtext-gate.sh): the system message table's text for
 * every Windows Sockets error and the common COM HRESULTs. Prints one line per code:
 *   <hex code> <ok|NONE gle>   FormatMessageW(FROM_SYSTEM | IGNORE_INSERTS) found text, or not
 * then "SAMPLE <text>" for WSAENETUNREACH (10051), the error Lapce showed as
 * "OS Error 10051 (FormatMessageW() returned error 317)". */
#include <windows.h>
#include <stdio.h>

static const DWORD codes[] = {
    /* Windows Sockets: every code winerror.h defines from WSAEINTR to WSA_QOS_RESERVED_PETYPE */
    10004, 10009, 10013, 10014, 10022, 10024, 10035, 10036, 10037, 10038, 10039, 10040, 10041, 10042,
    10043, 10044, 10045, 10046, 10047, 10048, 10049, 10050, 10051, 10052, 10053, 10054, 10055, 10056,
    10057, 10058, 10059, 10060, 10061, 10062, 10063, 10064, 10065, 10066, 10067, 10068, 10069, 10070,
    10071, 10091, 10092, 10093, 10101, 10102, 10103, 10104, 10105, 10106, 10107, 10108, 10109, 10110,
    10111, 10112, 11001, 11002, 11003, 11004, 11005, 11006, 11007, 11008, 11009, 11010, 11011, 11012,
    11013, 11014, 11015, 11016, 11017, 11018, 11019, 11020, 11021, 11022, 11023, 11024, 11025, 11026,
    11027, 11028, 11029, 11030, 11031,
    /* COM */
    0x80004001, 0x80004002, 0x80004003, 0x80004004, 0x80004005, 0x8000FFFF, 0x8000000A, 0x8000000B,
    0x8000000C, 0x8000000D, 0x8000000E, 0x80000013, 0x80000018, 0x80040110, 0x80040111, 0x80040154,
    0x800401F0, 0x800401F1, 0x80010001, 0x80010002, 0x80010105, 0x80010106, 0x80010108, 0x8001010A,
    0x8001010D, 0x8001010E, 0x80020001, 0x80020003, 0x80020004, 0x80020005, 0x80020006, 0x80020009,
    0x8002000A, 0x8002000B, 0x8002000E,
    /* HRESULT_FROM_WIN32 of a Windows Sockets error folds to it */
    0x80072743,
};

int main(void)
{
    WCHAR buf[512];
    unsigned i;

    for (i = 0; i < ARRAYSIZE(codes); i++)
    {
        DWORD n = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, codes[i],
                                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, ARRAYSIZE(buf), NULL);
        if (n) printf("%08lx ok\n", codes[i]);
        else printf("%08lx NONE %lu\n", codes[i], GetLastError());
    }
    if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, 10051, 0, buf,
                       ARRAYSIZE(buf), NULL))
        printf("SAMPLE %ls", buf);
    else
        printf("SAMPLE none\n");
    return 0;
}
