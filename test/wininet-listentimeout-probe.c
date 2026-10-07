/* wininet-listentimeout-gate.sh's probe (1473): INTERNET_OPTION_LISTEN_TIMEOUT
 * on an HTTP connect handle, as Office's HTTP client (OneAuth) sets it. */
#include <windows.h>
#include <wininet.h>
#include <stdio.h>

#ifndef INTERNET_OPTION_LISTEN_TIMEOUT
#define INTERNET_OPTION_LISTEN_TIMEOUT 11
#endif

static void set(HINTERNET h, const char *name, DWORD opt, DWORD value)
{
    DWORD err;
    SetLastError(0xdeadbeef);
    if (InternetSetOptionW(h, opt, &value, sizeof(value))) printf("%s 1 0\n", name);
    else { err = GetLastError(); printf("%s 0 %lu\n", name, err); }
}

int main(void)
{
    HINTERNET inet = InternetOpenW(L"probe", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    HINTERNET conn;

    if (!inet) { printf("open 0 %lu\n", GetLastError()); return 0; }
    conn = InternetConnectW(inet, L"localhost", 80, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!conn) { printf("connect 0 %lu\n", GetLastError()); return 0; }
    printf("connect 1 0\n");
    set(conn, "connect_timeout", INTERNET_OPTION_CONNECT_TIMEOUT, 5000);
    set(conn, "send_timeout", INTERNET_OPTION_SEND_TIMEOUT, 5000);
    set(conn, "receive_timeout", INTERNET_OPTION_RECEIVE_TIMEOUT, 5000);
    set(conn, "listen_timeout", INTERNET_OPTION_LISTEN_TIMEOUT, 5000);
    InternetCloseHandle(conn);
    InternetCloseHandle(inet);
    return 0;
}
