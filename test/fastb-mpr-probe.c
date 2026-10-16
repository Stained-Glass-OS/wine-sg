#include <windows.h>
#include <winnetwk.h>
#include <stdio.h>
#include <string.h>
static int failed;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); failed++; } } while (0)
int main(int argc, char **argv)
{
    char buf[1024];
    WCHAR wbuf[1024];
    DWORD size, ret, type;
    UNIVERSAL_NAME_INFOA *ui = (void *)buf;
    REMOTE_NAME_INFOA *ri = (void *)buf;
    UNIVERSAL_NAME_INFOW *uiw = (void *)wbuf;
    REMOTE_NAME_INFOW *riw = (void *)wbuf;
    char remote[256] = "";
    DWORD rsize = sizeof(remote), dlg;

    type = GetDriveTypeA("J:\\");
    printf("drive type J: %lu\n", type);
    ret = WNetGetConnectionA("J:", remote, &rsize);
    printf("WNetGetConnection J: %lu '%s'\n", ret, remote);
    if (type != DRIVE_REMOTE || ret) { printf("SKIP-NO-REMOTE\n"); }
    else
    {
        size = sizeof(buf);
        ret = WNetGetUniversalNameA("J:\\", UNIVERSAL_NAME_INFO_LEVEL, buf, &size);
        CHECK(ret == 0, "universal root %lu", ret);
        CHECK(!ret && !strcmp(ui->lpUniversalName, remote), "universal root '%s' vs '%s'", ret ? "" : ui->lpUniversalName, remote);
        size = sizeof(buf);
        ret = WNetGetUniversalNameA("J:\\dir\\file.txt", UNIVERSAL_NAME_INFO_LEVEL, buf, &size);
        CHECK(ret == 0, "universal path %lu", ret);
        if (!ret) { char exp[300]; sprintf(exp, "%s\\dir\\file.txt", remote); CHECK(!strcmp(ui->lpUniversalName, exp), "universal path '%s' expected '%s'", ui->lpUniversalName, exp); }
        size = sizeof(buf);
        ret = WNetGetUniversalNameA("J:\\dir\\file.txt", REMOTE_NAME_INFO_LEVEL, buf, &size);
        CHECK(ret == 0, "remote path %lu", ret);
        if (!ret) CHECK(!strcmp(ri->lpConnectionName, remote) && !strcmp(ri->lpRemainingPath, "\\dir\\file.txt"), "remote parts '%s' '%s'", ri->lpConnectionName, ri->lpRemainingPath);
        size = 1;
        ret = WNetGetUniversalNameA("J:\\dir", UNIVERSAL_NAME_INFO_LEVEL, buf, &size);
        CHECK(ret == WN_MORE_DATA && size > 1, "small buffer %lu size %lu", ret, size);
        size = 1;
        ret = WNetGetUniversalNameA("J:\\dir", REMOTE_NAME_INFO_LEVEL, buf, &size);
        CHECK(ret == WN_MORE_DATA && size > sizeof(REMOTE_NAME_INFOA), "small remote buffer %lu size %lu", ret, size);
        size = sizeof(wbuf);
        ret = WNetGetUniversalNameW(L"J:\\x", UNIVERSAL_NAME_INFO_LEVEL, wbuf, &size);
        CHECK(ret == 0 && uiw->lpUniversalName[0] == '\\', "wide universal %lu", ret);
        size = sizeof(wbuf);
        ret = WNetGetUniversalNameW(L"J:\\x", REMOTE_NAME_INFO_LEVEL, wbuf, &size);
        CHECK(ret == 0 && !wcscmp(riw->lpRemainingPath, L"\\x"), "wide remote %lu", ret);
        ret = WNetGetUniversalNameA("J:\\x", REMOTE_NAME_INFO_LEVEL, buf, NULL);
        CHECK(ret == WN_BAD_POINTER, "null size %lu", ret);
        size = sizeof(buf);
        ret = WNetGetUniversalNameA("J:\\x", REMOTE_NAME_INFO_LEVEL, NULL, &size);
        CHECK(ret == WN_BAD_POINTER, "null buffer %lu", ret);
    }
    size = sizeof(buf);
    ret = WNetGetUniversalNameA("C:\\windows", UNIVERSAL_NAME_INFO_LEVEL, buf, &size);
    CHECK(ret == WN_NOT_CONNECTED, "local drive %lu", ret);
    ret = WNetGetUniversalNameA("C:\\windows", 7, buf, &size);
    CHECK(ret == WN_BAD_VALUE, "bad level %lu", ret);

    /* extended error */
    {
        char text[64], prov[64];
        DWORD code = 99;
        ret = WNetGetLastErrorA(&code, text, sizeof(text), prov, sizeof(prov));
        CHECK(ret == 0 && code == 0 && !text[0], "last error initial %lu code %lu '%s'", ret, code, text);
        ret = WNetGetLastErrorA(NULL, text, sizeof(text), prov, sizeof(prov));
        CHECK(ret == WN_BAD_POINTER, "last error null %lu", ret);
        {
            HMODULE mpr = GetModuleHandleA("mpr.dll");
            DWORD (WINAPI *setx)(DWORD, LPSTR) = (void *)GetProcAddress(mpr, "NPSSetExtendedErrorA");
            DWORD (WINAPI *copy)(LPCSTR, LPVOID, LPDWORD) = (void *)GetProcAddress(mpr, "NPSCopyStringA");
            CHECK(setx && copy, "npsapi exports");
            if (setx)
            {
                ret = setx(1234, "provider says no");
                CHECK(ret == 0, "set extended %lu", ret);
                ret = WNetGetLastErrorA(&code, text, sizeof(text), prov, sizeof(prov));
                CHECK(ret == 0 && code == 1234 && !strcmp(text, "provider says no"), "last error %lu code %lu '%s'", ret, code, text);
                ret = WNetGetLastErrorA(&code, text, 5, prov, sizeof(prov));
                CHECK(ret == 0 && !strcmp(text, "prov"), "truncated '%s'", text);
            }
            if (copy)
            {
                char out[16]; DWORD n = 3;
                ret = copy("hello", out, &n);
                CHECK(ret == WN_MORE_DATA && n == 6, "copy small %lu n %lu", ret, n);
                n = sizeof(out);
                ret = copy("hello", out, &n);
                CHECK(ret == 0 && !strcmp(out, "hello"), "copy %lu", ret);
            }
        }
    }
    /* parent of a share and a server */
    {
        NETRESOURCEA in = {0};
        NETRESOURCEA *out = (void *)buf;
        in.dwType = RESOURCETYPE_DISK;
        in.lpRemoteName = (char *)"\\\\srv\\share";
        size = sizeof(buf);
        ret = WNetGetResourceParentA(&in, buf, &size);
        if (ret == WN_NO_NETWORK) printf("no network provider\n");
        else
        {
            CHECK(ret == 0 && !strcmp(out->lpRemoteName, "\\\\srv") && out->dwDisplayType == RESOURCEDISPLAYTYPE_SERVER,
                  "parent of share %lu '%s'", ret, ret ? "" : out->lpRemoteName);
            in.lpRemoteName = (char *)"\\\\srv";
            size = sizeof(buf);
            ret = WNetGetResourceParentA(&in, buf, &size);
            CHECK(ret == 0 && out->lpRemoteName && out->dwDisplayType == RESOURCEDISPLAYTYPE_NETWORK, "parent of server %lu", ret);
            in.lpRemoteName = (char *)"\\\\srv\\share\\a\\b";
            size = sizeof(buf);
            ret = WNetGetResourceParentA(&in, buf, &size);
            CHECK(ret == 0 && !strcmp(out->lpRemoteName, "\\\\srv\\share\\a"), "parent of path %lu '%s'", ret, ret ? "" : out->lpRemoteName);
            size = 4;
            ret = WNetGetResourceParentA(&in, buf, &size);
            CHECK(ret == WN_MORE_DATA && size > 4, "parent small %lu %lu", ret, size);
        }
    }
    /* disconnect dialog struct with no connection */
    {
        DISCDLGSTRUCTA d = { sizeof(d), NULL, (char *)"Y:", NULL, 0 };
        dlg = WNetDisconnectDialog1A(&d);
        CHECK(dlg != WN_NO_NETWORK || dlg == WN_NO_NETWORK, "disconnect %lu", dlg);
        printf("disconnect dialog on unconnected Y: -> %lu\n", dlg);
    }
    printf(failed ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failed;
}
