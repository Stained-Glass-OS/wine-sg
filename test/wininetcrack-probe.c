/* Probe for patches/sg/2440: InternetCrackUrl buffer rules, empty http host, empty request headers, raw header index. */
#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <string.h>

static int fails;
static void check(const char *name, int ok) { printf("      %s  %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }

static void zero_lens(URL_COMPONENTSA *c) { c->dwSchemeLength = c->dwHostNameLength = c->dwUserNameLength = c->dwPasswordLength = c->dwUrlPathLength = c->dwExtraInfoLength = 0; }

int main(void)
{
    URL_COMPONENTSA uc;
    char scheme[32], host[256], user[256], pass[256], path[256], extra[256];
    BOOL ret;
    DWORD gle, idx, len;
    HINTERNET hi, hc, hr;
    char buf[512];

    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszScheme = scheme; uc.lpszHostName = host; uc.lpszUserName = user;
    uc.lpszPassword = pass; uc.lpszUrlPath = path; uc.lpszExtraInfo = extra;

    /* a buffer of length 0 cannot take a part, not even an empty one */
    zero_lens(&uc);
    uc.dwSchemeLength = 32; uc.dwHostNameLength = 256; uc.dwUserNameLength = 256; uc.dwPasswordLength = 256;
    uc.dwUrlPathLength = 0; uc.dwExtraInfoLength = 256;
    SetLastError(0xdeadbeef);
    ret = InternetCrackUrlA("http://example.org/a/b?x=1", 0, 0, &uc);
    gle = GetLastError();
    check("path buffer of length 0: fails", !ret);
    check("path buffer of length 0: insufficient buffer", gle == ERROR_INSUFFICIENT_BUFFER);
    uc.dwUrlPathLength = 256; uc.dwExtraInfoLength = 0;
    SetLastError(0xdeadbeef);
    ret = InternetCrackUrlA("http://example.org/a/b?x=1", 0, 0, &uc);
    check("extra info buffer of length 0: fails", !ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER);
    uc.dwExtraInfoLength = 256; uc.dwHostNameLength = 0;
    SetLastError(0xdeadbeef);
    ret = InternetCrackUrlA("file:///C:/x.txt", 0, 0, &uc);
    check("host buffer of length 0 (no host in the URL): fails", !ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER);
    zero_lens(&uc);
    SetLastError(0xdeadbeef);
    ret = InternetCrackUrlA("http://example.org/a", 0, 0, &uc);
    check("all lengths 0 with buffers: invalid parameter", !ret && GetLastError() == ERROR_INVALID_PARAMETER);

    /* the ordinary case still works */
    uc.dwSchemeLength = 32; uc.dwHostNameLength = 256; uc.dwUserNameLength = 256; uc.dwPasswordLength = 256;
    uc.dwUrlPathLength = 256; uc.dwExtraInfoLength = 256;
    ret = InternetCrackUrlA("http://u:p@example.org:81/a/b?x=1", 0, 0, &uc);
    check("ordinary crack", ret && !strcmp(host, "example.org") && !strcmp(path, "/a/b") && !strcmp(extra, "?x=1") && uc.nPort == 81);

    /* http without a host */
    uc.dwSchemeLength = 32; uc.dwHostNameLength = 256; uc.dwUserNameLength = 256; uc.dwPasswordLength = 256;
    uc.dwUrlPathLength = 256; uc.dwExtraInfoLength = 256;
    SetLastError(0xdeadbeef);
    ret = InternetCrackUrlA("http://", 0, 0, &uc);
    check("http:// has no host: fails", !ret);
    uc.dwSchemeLength = 32; uc.dwHostNameLength = 256; uc.dwUserNameLength = 256; uc.dwPasswordLength = 256;
    uc.dwUrlPathLength = 256; uc.dwExtraInfoLength = 256;
    ret = InternetCrackUrlA("https:///x", 0, 0, &uc);
    check("https:///x has no host: fails", !ret);
    uc.dwSchemeLength = 32; uc.dwHostNameLength = 256; uc.dwUserNameLength = 256; uc.dwPasswordLength = 256;
    uc.dwUrlPathLength = 256; uc.dwExtraInfoLength = 256;
    ret = InternetCrackUrlA("file:///C:/x.txt", 0, 0, &uc);
    check("file:/// is fine", ret);

    /* ICU_DECODE does not add the slash of an empty path */
    uc.dwSchemeLength = 32; uc.dwHostNameLength = 256; uc.dwUserNameLength = 256; uc.dwPasswordLength = 256;
    uc.dwUrlPathLength = 256; uc.dwExtraInfoLength = 256;
    path[0] = 'x';
    ret = InternetCrackUrlA("http://example.org", 0, ICU_DECODE, &uc);
    check("decode: path stays empty", ret && path[0] == 0 && uc.dwUrlPathLength == 0);

    /* request headers */
    hi = InternetOpenA("sgprobe", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    hc = InternetConnectA(hi, "localhost", 80, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    hr = HttpOpenRequestA(hc, "GET", "/", NULL, NULL, NULL, 0, 0);
    if (!hr) { printf("      FAIL  no request handle %lu\n", GetLastError()); puts("RESULT: FAIL"); return 1; }
    SetLastError(0xdeadbeef);
    ret = HttpAddRequestHeadersA(hr, "EmptyOne:", -1, HTTP_ADDREQ_FLAG_ADD);
    check("header with no value: refused", !ret && GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    ret = HttpAddRequestHeadersA(hr, "EmptyTwo:\r\n", -1, HTTP_ADDREQ_FLAG_ADD);
    check("header with no value and CRLF: refused", !ret && GetLastError() == ERROR_INVALID_PARAMETER);
    len = sizeof(buf); strcpy(buf, "EmptyOne");
    SetLastError(0xdeadbeef);
    ret = HttpQueryInfoA(hr, HTTP_QUERY_CUSTOM | HTTP_QUERY_FLAG_REQUEST_HEADERS, buf, &len, NULL);
    check("and it is not there", !ret && GetLastError() == ERROR_HTTP_HEADER_NOT_FOUND);
    ret = HttpAddRequestHeadersA(hr, "Real: yes\r\n", -1, HTTP_ADDREQ_FLAG_ADD);
    check("a header with a value is added", ret);
    ret = HttpAddRequestHeadersA(hr, "Real:\r\n", -1, HTTP_ADDREQ_FLAG_REPLACE);
    check("replacing with no value removes it", ret);
    len = sizeof(buf); strcpy(buf, "Real"); SetLastError(0xdeadbeef);
    ret = HttpQueryInfoA(hr, HTTP_QUERY_CUSTOM | HTTP_QUERY_FLAG_REQUEST_HEADERS, buf, &len, NULL);
    check("removed", !ret && GetLastError() == ERROR_HTTP_HEADER_NOT_FOUND);

    /* raw headers: one block, so only the start index 0 finds it */
    idx = 0; len = sizeof(buf);
    ret = HttpQueryInfoA(hr, HTTP_QUERY_RAW_HEADERS | HTTP_QUERY_FLAG_REQUEST_HEADERS, buf, &len, &idx);
    check("raw request headers, index 0", ret && idx == 0);
    idx = 0xdeadbeef; len = sizeof(buf); SetLastError(0xdeadbeef);
    ret = HttpQueryInfoA(hr, HTTP_QUERY_RAW_HEADERS | HTTP_QUERY_FLAG_REQUEST_HEADERS, buf, &len, &idx);
    check("raw request headers, bad index: not found", !ret && GetLastError() == ERROR_HTTP_HEADER_NOT_FOUND);
    idx = 5; len = sizeof(buf); SetLastError(0xdeadbeef);
    ret = HttpQueryInfoA(hr, HTTP_QUERY_RAW_HEADERS_CRLF | HTTP_QUERY_FLAG_REQUEST_HEADERS, buf, &len, &idx);
    check("raw CRLF request headers, index 5: not found", !ret && GetLastError() == ERROR_HTTP_HEADER_NOT_FOUND);

    InternetCloseHandle(hr);
    InternetCloseHandle(hc);
    InternetCloseHandle(hi);
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
