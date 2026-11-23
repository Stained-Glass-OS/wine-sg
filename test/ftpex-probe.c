/* wine-sg 9707: FtpPutFileEx / FtpGetFileEx (wininet): the local file name in Unicode (a name no ANSI code
 * page holds), the remote name in ANSI. Talks to test/ftpex-server.py on 127.0.0.1 (port in argv[1]). */
#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <stdlib.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)
typedef BOOL (WINAPI *get_t)(HINTERNET, LPCSTR, LPCWSTR, BOOL, DWORD, DWORD, DWORD_PTR);
typedef BOOL (WINAPI *put_t)(HINTERNET, LPCWSTR, LPCSTR, DWORD, DWORD_PTR);

int main(int argc, char **argv)
{
    HMODULE wininet = LoadLibraryA("wininet.dll");
    get_t get = (get_t)GetProcAddress(wininet, "FtpGetFileEx");
    put_t put = (put_t)GetProcAddress(wininet, "FtpPutFileEx");
    static const WCHAR local[] = L"sg-ftpex-\x4e2d\x6587-up.txt", back[] = L"sg-ftpex-\x4e2d\x6587-back.txt";
    static const char data[] = "Stained Glass FtpPutFileEx/FtpGetFileEx gate data\n";
    HINTERNET inet, ftp;
    HANDLE f;
    DWORD n;
    char buf[256];
    BOOL ret;

    CHECK(get && put, "wininet exports FtpGetFileEx and FtpPutFileEx");
    if (!get || !put || argc < 2) { printf("RESULT: FAIL\n"); return 1; }
    f = CreateFileW(local, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, data, sizeof(data) - 1, &n, NULL);
    CloseHandle(f);
    inet = InternetOpenA("sg-ftpex", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    ftp = InternetConnectA(inet, "127.0.0.1", (INTERNET_PORT)atoi(argv[1]), "gate", "gate", INTERNET_SERVICE_FTP,
                           INTERNET_FLAG_PASSIVE, 0);
    CHECK(ftp != NULL, "connected to the gate's FTP server (error %lu)", GetLastError());
    ret = put(ftp, local, "remote.txt", FTP_TRANSFER_TYPE_BINARY, 0);
    CHECK(ret, "FtpPutFileEx from a Unicode-only local name (error %lu)", GetLastError());
    DeleteFileW(back);
    ret = get(ftp, "remote.txt", back, TRUE, FILE_ATTRIBUTE_NORMAL, FTP_TRANSFER_TYPE_BINARY, 0);
    CHECK(ret, "FtpGetFileEx to a Unicode-only local name (error %lu)", GetLastError());
    memset(buf, 0, sizeof(buf));
    f = CreateFileW(back, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    ReadFile(f, buf, sizeof(buf) - 1, &n, NULL);
    CloseHandle(f);
    CHECK(!strcmp(buf, data), "the file came back whole (%lu bytes)", n);
    ret = get(ftp, "remote.txt", back, TRUE, FILE_ATTRIBUTE_NORMAL, FTP_TRANSFER_TYPE_BINARY, 0);
    CHECK(!ret && GetLastError() == ERROR_FILE_EXISTS, "fFailIfExists with the file there: ERROR_FILE_EXISTS (%lu)", GetLastError());
    ret = get(NULL, "remote.txt", back, FALSE, 0, 0, 0);
    CHECK(!ret && GetLastError() == ERROR_INVALID_HANDLE, "no session: ERROR_INVALID_HANDLE (%lu)", GetLastError());
    InternetCloseHandle(ftp);
    InternetCloseHandle(inet);
    DeleteFileW(local); DeleteFileW(back);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
