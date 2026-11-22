/* wine-sg 8415 gate, winhttp half of tls13-probe.c (separate file: winhttp.h and wininet.h
 * do not go together). */
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>

int http(int port)
{
    HINTERNET s = WinHttpOpen(L"tls13-probe", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0), c, r;
    DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                  SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;

    c = WinHttpConnect(s, L"localhost", port, 0);
    r = WinHttpOpenRequest(c, L"GET", L"/", NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
    WinHttpSetOption(r, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
    if (!WinHttpSendRequest(r, NULL, 0, NULL, 0, 0, 0) || !WinHttpReceiveResponse(r, NULL))
    { printf("fail %lu\n", GetLastError()); return 1; }
    /* no winhttp option gives schannel's protocol: against a TLS 1.3-only server, success is
     * the proof */
    printf("ok proto=?\n");
    return 0;
}
