/* wine-sg 8415: a TLS 1.3 client through wininet and winhttp. Usage: probe inet|http PORT.
 * Connects to https://localhost:PORT/ (the gate's openssl s_server, a self-signed certificate:
 * certificate errors are ignored, as only the protocol is under test) and prints
 * "ok proto=0xNNN" (schannel's SP_PROT_* of the connection) or "fail N". */
#include <windows.h>
#include <wininet.h>
#include <schannel.h>
#include <stdio.h>
#include <stdlib.h>

/* wininet's 66 (66); its leading fields */
struct sec_conn_info { DWORD dwSize; BOOL fSecure; SecPkgContext_ConnectionInfo connectionInfo; BYTE rest[512]; };

static int inet(int port)
{
    HINTERNET s = InternetOpenA("tls13-probe", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0), c, r;
    DWORD flags, len, err;
    struct sec_conn_info info;
    char buf[256];

    c = InternetConnectA(s, "localhost", port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    r = HttpOpenRequestA(c, "GET", "/", NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD |
                         INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID, 0);
    len = sizeof(flags);
    InternetQueryOptionA(r, INTERNET_OPTION_SECURITY_FLAGS, &flags, &len);
    flags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_REVOCATION | SECURITY_FLAG_IGNORE_WRONG_USAGE;
    InternetSetOptionA(r, INTERNET_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
    if (!HttpSendRequestA(r, NULL, 0, NULL, 0)) { err = GetLastError(); printf("fail %lu\n", err); return 1; }
    InternetReadFile(r, buf, sizeof(buf), &len);
    memset(&info, 0, sizeof(info)); info.dwSize = sizeof(info); len = sizeof(info);
    if (!InternetQueryOptionA(r, 66, &info, &len)) { printf("ok proto=?\n"); return 0; }  /* Wine has no such option */
    printf("ok proto=0x%lx\n", info.connectionInfo.dwProtocol);
    return 0;
}

int http(int port);   /* tls13-http.c: winhttp's header cannot be included with wininet's */

int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    return !strcmp(argv[1], "inet") ? inet(atoi(argv[2])) : http(atoi(argv[2]));
}
