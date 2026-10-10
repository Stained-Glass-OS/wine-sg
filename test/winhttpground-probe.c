/* winhttp behaviours that Wine's own tests record from real Windows
 * (patches/sg/2424), run by test/winhttpground-gate.sh. Nothing is sent: the
 * handles are only made. An option number that is no option is
 * ERROR_WINHTTP_INVALID_OPTION to set and ERROR_INVALID_PARAMETER to query; one
 * that exists but not for this kind of handle is ERROR_WINHTTP_INCORRECT_HANDLE_TYPE
 * (on a connection, to query or set; to set on a request or a session);
 * WINHTTP_OPTION_USERNAME and _PASSWORD belong to the request;
 * WinHttpQueryDataAvailable before the response is ERROR_WINHTTP_INCORRECT_HANDLE_STATE. */
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <string.h>

#ifndef WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE
#define WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE 122
#define WINHTTP_OPTION_WEB_SOCKET_SEND_BUFFER_SIZE 123
#define WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL 133
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define ERR_IS(e) (GetLastError() == (e))

int main(void)
{
    HINTERNET ses = WinHttpOpen(L"sgtest", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0), con, req, req2;
    BYTE buffer[64];
    DWORD size = sizeof(buffer), value = 5;
    WCHAR name[64];
    BOOL ret;

    check(ses != NULL, "a session");
    con = WinHttpConnect(ses, L"localhost", 80, 0);
    check(con != NULL, "a connection");
    req = WinHttpOpenRequest(con, NULL, L"/x", NULL, NULL, NULL, 0);
    check(req != NULL, "a request");

    /* a number that is no option */
    SetLastError(0xdeadbeef);
    check(!WinHttpSetOption(ses, 0, buffer, 4) && ERR_IS(ERROR_WINHTTP_INVALID_OPTION), "session: setting option 0: ERROR_WINHTTP_INVALID_OPTION");
    SetLastError(0xdeadbeef);
    check(!WinHttpQueryOption(ses, 0, buffer, &size) && ERR_IS(ERROR_INVALID_PARAMETER), "session: querying option 0: ERROR_INVALID_PARAMETER");
    SetLastError(0xdeadbeef);
    check(!WinHttpSetOption(con, 0, buffer, 4) && ERR_IS(ERROR_WINHTTP_INVALID_OPTION), "connection: setting option 0: ERROR_WINHTTP_INVALID_OPTION");
    SetLastError(0xdeadbeef);
    size = sizeof(buffer);
    check(!WinHttpQueryOption(con, 0, buffer, &size) && ERR_IS(ERROR_INVALID_PARAMETER), "connection: querying option 0: ERROR_INVALID_PARAMETER");
    SetLastError(0xdeadbeef);
    check(!WinHttpSetOption(req, 0, buffer, 4) && ERR_IS(ERROR_WINHTTP_INVALID_OPTION), "request: setting option 0: ERROR_WINHTTP_INVALID_OPTION");
    SetLastError(0xdeadbeef);
    size = sizeof(buffer);
    check(!WinHttpQueryOption(req, 0, buffer, &size) && ERR_IS(ERROR_INVALID_PARAMETER), "request: querying option 0: ERROR_INVALID_PARAMETER");

    /* options that exist, for another handle */
    SetLastError(0xdeadbeef);
    size = sizeof(value);
    check(!WinHttpQueryOption(con, WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE, &value, &size) && ERR_IS(ERROR_WINHTTP_INCORRECT_HANDLE_TYPE),
          "a connection has no websocket buffer size to query: ERROR_WINHTTP_INCORRECT_HANDLE_TYPE");
    SetLastError(0xdeadbeef);
    check(!WinHttpSetOption(con, WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE, &value, sizeof(value)) && ERR_IS(ERROR_WINHTTP_INCORRECT_HANDLE_TYPE),
          "nor to set");
    value = 20000;
    SetLastError(0xdeadbeef);
    check(!WinHttpSetOption(req, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, &value, sizeof(value)) && ERR_IS(ERROR_WINHTTP_INCORRECT_HANDLE_TYPE),
          "a request has no keep-alive interval to set (that is a websocket's): ERROR_WINHTTP_INCORRECT_HANDLE_TYPE");
    SetLastError(0xdeadbeef);
    size = sizeof(value);
    check(!WinHttpQueryOption(req, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, &value, &size) && ERR_IS(ERROR_INVALID_PARAMETER),
          "and querying it is ERROR_INVALID_PARAMETER");
    SetLastError(0xdeadbeef);
    size = sizeof(value);
    check(!WinHttpQueryOption(ses, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, &value, &size) && ERR_IS(ERROR_INVALID_PARAMETER),
          "on a session too");

    /* credentials belong to the request */
    check(WinHttpSetOption(req, WINHTTP_OPTION_USERNAME, (void *)L"alice", 5), "a user name is set on a request");
    check(WinHttpSetOption(req, WINHTTP_OPTION_PASSWORD, (void *)L"secret", 6), "and a password");
    size = sizeof(name);
    memset(name, 0, sizeof(name));
    ret = WinHttpQueryOption(req, WINHTTP_OPTION_USERNAME, name, &size);
    check(ret && !wcscmp(name, L"alice") && size == 5 * sizeof(WCHAR), "which it gives back");
    req2 = WinHttpOpenRequest(con, NULL, L"/y", NULL, NULL, NULL, 0);
    size = sizeof(name);
    memset(name, 0xff, sizeof(name));
    ret = WinHttpQueryOption(req2, WINHTTP_OPTION_USERNAME, name, &size);
    check(ret && !name[0] && size == 0, "another request on the same connection has none");
    size = sizeof(name);
    memset(name, 0xff, sizeof(name));
    ret = WinHttpQueryOption(req2, WINHTTP_OPTION_PASSWORD, name, &size);
    check(ret && !name[0] && size == 0, "nor a password");
    check(WinHttpSetCredentials(req2, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_BASIC, L"bob", L"pw", NULL), "credentials set with WinHttpSetCredentials");
    size = sizeof(name);
    memset(name, 0xff, sizeof(name));
    ret = WinHttpQueryOption(req2, WINHTTP_OPTION_USERNAME, name, &size);
    check(ret && !name[0] && size == 0, "are not the request's user name");

    /* data before there is a response */
    value = 12345;
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryDataAvailable(req, &value);
    check(!ret && ERR_IS(ERROR_WINHTTP_INCORRECT_HANDLE_STATE) && value == 12345, "WinHttpQueryDataAvailable before the response: ERROR_WINHTTP_INCORRECT_HANDLE_STATE, the count untouched");
    SetLastError(0xdeadbeef);
    ret = WinHttpQueryDataAvailable(con, &value);
    check(!ret && ERR_IS(ERROR_WINHTTP_INCORRECT_HANDLE_TYPE), "on a connection: ERROR_WINHTTP_INCORRECT_HANDLE_TYPE");

    WinHttpCloseHandle(req2);
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
