/* Fake ODBC driver/setup/translator DLL for the odbccp32 dialog gate. */
#include <windows.h>
#include <wchar.h>
#include <sql.h>
#include <sqlext.h>
#include <odbcinst.h>

BOOL WINAPI SQLWriteDSNToIniW(LPCWSTR, LPCWSTR);
BOOL WINAPI SQLRemoveDSNFromIniW(LPCWSTR);
static void logline(const WCHAR *s)
{
    HANDLE h = CreateFileW(L"C:\\sgodbc.log", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
    char buf[1024]; DWORD n;
    if (h == INVALID_HANDLE_VALUE) return;
    n = WideCharToMultiByte(CP_ACP, 0, s, -1, buf, sizeof(buf) - 2, NULL, NULL);
    if (n) { buf[n - 1] = '\n'; WriteFile(h, buf, n, &n, NULL); }
    CloseHandle(h);
}

__declspec(dllexport) BOOL WINAPI ConfigDSNW(HWND hwnd, WORD req, const WCHAR *driver, const WCHAR *attrs)
{
    WCHAR line[512], name[256] = L"NewDSN";
    const WCHAR *p;
    wsprintfW(line, L"%u|%ls|%ls", req, driver, attrs && *attrs ? attrs : L"");
    logline(line);
    for (p = attrs; p && *p; p += lstrlenW(p) + 1)
        if (CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, p, 4, L"DSN=", 4) == CSTR_EQUAL) lstrcpynW(name, p + 4, 256);
    if (req == ODBC_ADD_DSN) return SQLWriteDSNToIniW(name, driver);
    if (req == ODBC_REMOVE_DSN) return SQLRemoveDSNFromIniW(name);
    return req == ODBC_CONFIG_DSN;
}

__declspec(dllexport) BOOL WINAPI ConfigTranslator(HWND hwnd, DWORD *option)
{
    if (GetFileAttributesW(L"C:\\sgodbc.cancel") != INVALID_FILE_ATTRIBUTES) return FALSE;
    *option = 0x1234;
    return TRUE;
}

__declspec(dllexport) SQLRETURN WINAPI SQLAllocHandle(SQLSMALLINT t, SQLHANDLE in, SQLHANDLE *out)
{
    *out = (SQLHANDLE)(ULONG_PTR)(0x1000 + t);
    return SQL_SUCCESS;
}
__declspec(dllexport) SQLRETURN WINAPI SQLSetEnvAttr(SQLHENV e, SQLINTEGER a, SQLPOINTER v, SQLINTEGER l) { return SQL_SUCCESS; }
__declspec(dllexport) SQLRETURN WINAPI SQLDisconnect(SQLHDBC d) { return SQL_SUCCESS; }
__declspec(dllexport) SQLRETURN WINAPI SQLFreeHandle(SQLSMALLINT t, SQLHANDLE h) { return SQL_SUCCESS; }

__declspec(dllexport) SQLRETURN WINAPI SQLDriverConnectW(SQLHDBC d, SQLHWND w, SQLWCHAR *in, SQLSMALLINT inlen,
        SQLWCHAR *out, SQLSMALLINT outmax, SQLSMALLINT *outlen, SQLUSMALLINT completion)
{
    WCHAR line[512];
    wsprintfW(line, L"CONNECT|%ls|%u", in, completion);
    logline(line);
    if (!wcsstr(in, L"OK=1")) return SQL_ERROR;
    lstrcpynW(out, in, outmax - 8);
    lstrcatW(out, L"OUT=1;");
    if (outlen) *outlen = lstrlenW(out);
    return SQL_SUCCESS;
}
