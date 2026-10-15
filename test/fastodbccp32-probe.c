#include <windows.h>
#include <odbcinst.h>
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    char buf[256], app[256];
    WORD len = 0;
    WCHAR wbuf[256];
    BOOL ret;
    DWORD code = 0;
    char msg[64];
    WORD mlen;
    HANDLE f;
    char inf[MAX_PATH];
    WCHAR winf[MAX_PATH];

    DeleteFileA("t.dsn");
    ret = SQLWriteFileDSN("t.dsn", "ODBC", "DRIVER", "Test Driver");
    CHECK(ret, "write");
    ret = SQLWriteFileDSN("t.dsn", "ODBC", "DBQ", "x.mdb");
    CHECK(ret, "write2");
    ret = SQLWriteFileDSN("t.dsn", "Other", "A", "1");
    CHECK(ret, "write3");

    memset(buf, 0xcc, sizeof(buf));
    ret = SQLReadFileDSN("t.dsn", "ODBC", "DRIVER", buf, sizeof(buf), &len);
    CHECK(ret && !strcmp(buf, "Test Driver") && len == 11, "read %d '%s' %u", ret, buf, len);

    memset(buf, 0, sizeof(buf));
    ret = SQLReadFileDSN("t.dsn", "ODBC", NULL, buf, sizeof(buf), &len);
    CHECK(ret && !strcmp(buf, "DRIVER") && !strcmp(buf + 7, "DBQ"), "keys %d '%s' '%s'", ret, buf, buf + 7);
    memset(app, 0, sizeof(app));
    ret = SQLReadFileDSN("t.dsn", NULL, NULL, app, sizeof(app), &len);
    CHECK(ret && !strcmp(app, "ODBC") && !strcmp(app + 5, "Other"), "sections %d", ret);

    ret = SQLReadFileDSN("t.dsn", "ODBC", "NOPE", buf, sizeof(buf), &len);
    CHECK(ret && buf[0] == 0 && len == 0, "missing key");

    ret = SQLWriteFileDSN("t.dsn", "ODBC", "DBQ", NULL);
    CHECK(ret, "delete key");
    ret = SQLReadFileDSN("t.dsn", "ODBC", "DBQ", buf, sizeof(buf), &len);
    CHECK(ret && buf[0] == 0, "deleted");
    ret = SQLWriteFileDSN("t.dsn", "Other", NULL, NULL);
    CHECK(ret, "delete section");
    ret = SQLReadFileDSN("t.dsn", NULL, NULL, app, sizeof(app), &len);
    CHECK(ret && !strcmp(app, "ODBC") && app[5] == 0, "one section left");

    ret = SQLReadFileDSN("", "ODBC", "DRIVER", buf, sizeof(buf), &len);
    CHECK(!ret, "empty name rejected");
    ret = SQLReadFileDSN("t.dsn", "ODBC", "DRIVER", buf, 0, &len);
    CHECK(!ret, "zero buffer rejected");
    DeleteFileA("t.dsn");

    /* available drivers from an INF-style file */
    f = CreateFileA("d.inf", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f != INVALID_HANDLE_VALUE)
    {
        const char *text = "[ODBC Drivers]\r\nDriver One=Installed\r\nDriver Two=Installed\r\n[Driver One]\r\nDriver=x.dll\r\n";
        DWORD w;
        WriteFile(f, text, strlen(text), &w, NULL);
        CloseHandle(f);
    }
    GetFullPathNameA("d.inf", sizeof(inf), inf, NULL);
    MultiByteToWideChar(CP_ACP, 0, inf, -1, winf, MAX_PATH);
    memset(buf, 0, sizeof(buf));
    ret = SQLGetAvailableDrivers(inf, buf, sizeof(buf), &len);
    CHECK(ret && !strcmp(buf, "Driver One") && !strcmp(buf + 11, "Driver Two") && len == 11 + 11 + 1,
          "available %d '%s' '%s' %u", ret, buf, buf + 11, len);
    ret = SQLGetAvailableDriversW(winf, wbuf, (sizeof(wbuf)/sizeof(wbuf[0])), &len);
    CHECK(ret && !wcscmp(wbuf, L"Driver One"), "available W");
    ret = SQLGetAvailableDrivers("missing.inf", buf, sizeof(buf), &len);
    CHECK(!ret, "missing inf");
    DeleteFileA("d.inf");

    /* errors posted by an installer component are visible to SQLInstallerError */
    { UWORD mode; SQLGetConfigMode(&mode); } /* clears the error stack */
    ret = SQLPostInstallerError(ODBC_ERROR_GENERAL_ERR, "posted message");
    CHECK(ret == SQL_SUCCESS, "post %d", ret);
    ret = SQLInstallerError(1, &code, msg, sizeof(msg), &mlen);
    CHECK(ret == SQL_SUCCESS && code == ODBC_ERROR_GENERAL_ERR && !strcmp(msg, "posted message"), "installer error %d %lu '%s'", ret, code, msg);
    ret = SQLInstallerError(2, &code, msg, sizeof(msg), &mlen);
    CHECK(ret == SQL_NO_DATA, "no second error");

    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
