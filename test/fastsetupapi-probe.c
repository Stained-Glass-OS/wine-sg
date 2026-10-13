#include <windows.h>
#include <setupapi.h>
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int exists(const char *p) { return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES; }

static void write_file(const char *path, const char *data)
{
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD n;
    if (h == INVALID_HANDLE_VALUE) return;
    WriteFile(h, data, strlen(data), &n, NULL);
    CloseHandle(h);
}

static int starts;
static UINT CALLBACK count_cb(void *ctx, UINT msg, UINT_PTR p1, UINT_PTR p2)
{
    if (msg == SPFILENOTIFY_STARTCOPY) { starts++; return FILEOP_DOIT; }
    return 1;
}

int main(void)
{
    char dir[MAX_PATH], log[MAX_PATH], buf[128], src[MAX_PATH], dst[MAX_PATH], inf[MAX_PATH];
    HSPFILELOG h, h2;
    DWORD need;
    BOOL ret;

    GetTempPathA(sizeof(dir), dir);
    strcat(dir, "fastsetupapi");
    CreateDirectoryA(dir, NULL);
    sprintf(log, "%s\\file.log", dir);
    DeleteFileA(log);

    /* file log */
    SetLastError(0xdeadbeef);
    h = SetupInitializeFileLogA(NULL, 0);
    CHECK(h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER, "NULL name: %p %lu", h, GetLastError());
    SetLastError(0xdeadbeef);
    h = SetupInitializeFileLogA(log, SPFILELOG_QUERYONLY);
    CHECK(h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND, "query-only of a missing log: %p %lu", h, GetLastError());
    h = SetupInitializeFileLogA(log, 0);
    CHECK(h != INVALID_HANDLE_VALUE && exists(log), "create: %p %lu", h, GetLastError());

    ret = SetupLogFileA(h, "Files.Test", "C:\\src\\a.txt", "C:\\dst\\a.txt", 1234, "tag,file", "Disk 100%", "other", 0);
    CHECK(ret, "log a: %lu", GetLastError());
    ret = SetupLogFileA(h, NULL, "C:\\src\\b.txt", "C:\\dst\\b.txt", 5, NULL, NULL, NULL, SPFILELOG_OEMFILE);
    CHECK(ret, "log b: %lu", GetLastError());
    SetLastError(0xdeadbeef);
    ret = SetupLogFileA(h, NULL, "x", NULL, 0, NULL, NULL, NULL, 0);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "no target: %d %lu", ret, GetLastError());

    ret = SetupQueryFileLogA(h, "Files.Test", "C:\\dst\\a.txt", SetupFileLogSourceFilename, buf, sizeof(buf), &need);
    CHECK(ret && !strcmp(buf, "C:\\src\\a.txt") && need == strlen(buf) + 1, "source: %d '%s' %lu", ret, buf, need);
    ret = SetupQueryFileLogA(h, "Files.Test", "C:\\dst\\a.txt", SetupFileLogChecksum, buf, sizeof(buf), &need);
    CHECK(ret && !strcmp(buf, "1234"), "checksum: '%s'", buf);
    ret = SetupQueryFileLogA(h, "Files.Test", "C:\\dst\\a.txt", SetupFileLogDiskTagfile, buf, sizeof(buf), &need);
    CHECK(ret && !strcmp(buf, "tag,file"), "tag with a comma: '%s'", buf);
    ret = SetupQueryFileLogA(h, "Files.Test", "C:\\dst\\a.txt", SetupFileLogDiskDescription, buf, sizeof(buf), &need);
    CHECK(ret && !strcmp(buf, "Disk 100%"), "description with a percent: '%s'", buf);
    ret = SetupQueryFileLogA(h, "Files.Test", "C:\\dst\\a.txt", SetupFileLogOtherInfo, buf, sizeof(buf), &need);
    CHECK(ret && !strcmp(buf, "other"), "other: '%s'", buf);
    ret = SetupQueryFileLogA(h, NULL, "C:\\dst\\b.txt", SetupFileLogSourceFilename, buf, sizeof(buf), &need);
    CHECK(ret && !strcmp(buf, "C:\\src\\b.txt"), "default section: '%s'", buf);
    SetLastError(0xdeadbeef);
    ret = SetupQueryFileLogA(h, "Files.Test", "C:\\dst\\a.txt", SetupFileLogSourceFilename, buf, 3, &need);
    CHECK(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER && need == 13, "small buffer: %d %lu need=%lu", ret, GetLastError(), need);
    SetLastError(0xdeadbeef);
    ret = SetupQueryFileLogA(h, "Files.Test", "C:\\dst\\none.txt", SetupFileLogSourceFilename, buf, sizeof(buf), &need);
    CHECK(!ret && GetLastError() == ERROR_NOT_FOUND, "missing entry: %d %lu", ret, GetLastError());

    /* a second handle, query only, sees the same log and cannot write */
    h2 = SetupInitializeFileLogA(log, SPFILELOG_QUERYONLY);
    CHECK(h2 != INVALID_HANDLE_VALUE, "query-only open: %lu", GetLastError());
    SetLastError(0xdeadbeef);
    ret = SetupLogFileA(h2, NULL, "s", "t", 0, NULL, NULL, NULL, 0);
    CHECK(!ret && GetLastError() == ERROR_ACCESS_DENIED, "write on a query-only log: %d %lu", ret, GetLastError());
    ret = SetupQueryFileLogA(h2, "Files.Test", "C:\\dst\\a.txt", SetupFileLogChecksum, buf, sizeof(buf), &need);
    CHECK(ret && !strcmp(buf, "1234"), "query via second handle");
    CHECK(SetupTerminateFileLog(h2), "terminate 2");

    ret = SetupRemoveFileLogEntryA(h, "Files.Test", "C:\\dst\\a.txt");
    CHECK(ret, "remove entry");
    SetLastError(0xdeadbeef);
    ret = SetupQueryFileLogA(h, "Files.Test", "C:\\dst\\a.txt", SetupFileLogSourceFilename, buf, sizeof(buf), &need);
    CHECK(!ret && GetLastError() == ERROR_NOT_FOUND, "entry gone: %d", ret);
    CHECK(SetupTerminateFileLog(h), "terminate");
    SetLastError(0xdeadbeef);
    CHECK(!SetupTerminateFileLog(INVALID_HANDLE_VALUE) && GetLastError() == ERROR_INVALID_HANDLE, "terminate of a bad handle");
    h = SetupInitializeFileLogA(log, SPFILELOG_FORCENEW);
    CHECK(h != INVALID_HANDLE_VALUE, "force new");
    ret = SetupQueryFileLogA(h, "Files.WinNt", "C:\\dst\\b.txt", SetupFileLogSourceFilename, buf, sizeof(buf), &need);
    CHECK(!ret, "the new log is empty");
    SetupTerminateFileLog(h);
    DeleteFileA(log);

    /* SetupInstallFile takes the subdirectory of a SourceDisksFiles line */
    sprintf(inf, "%s\\inst.inf", dir);
    write_file(inf, "[Version]\nSignature=\"$Chicago$\"\n[section1]\none.txt\ntwo.txt\n"
                    "[SourceDisksNames]\n1=heis\n[SourceDisksFiles]\none.txt=1\ntwo.txt=1,beta\n"
                    "[DestinationDirs]\nDefaultDestDir=-1,");
    {
        char text[2048];
        sprintf(text, "[Version]\nSignature=\"$Chicago$\"\n[section1]\none.txt\ntwo.txt\n"
                      "[SourceDisksNames]\n1=heis\n[SourceDisksFiles]\none.txt=1\ntwo.txt=1,beta\n"
                      "[DestinationDirs]\nDefaultDestDir=-1,%s\\dst\n", dir);
        write_file(inf, text);
    }
    {
        char p[MAX_PATH];
        sprintf(p, "%s\\src", dir); CreateDirectoryA(p, NULL);
        sprintf(p, "%s\\src\\beta", dir); CreateDirectoryA(p, NULL);
        sprintf(p, "%s\\dst", dir); CreateDirectoryA(p, NULL);
        sprintf(p, "%s\\src\\two.txt", dir); write_file(p, "wrong");
        sprintf(p, "%s\\src\\beta\\two.txt", dir); write_file(p, "right");
    }
    {
        HINF hinf = SetupOpenInfFileA(inf, NULL, INF_STYLE_WIN4, NULL);
        INFCONTEXT ctx;
        char srcdir[MAX_PATH];
        CHECK(hinf != INVALID_HANDLE_VALUE, "open inf %lu", GetLastError());
        sprintf(srcdir, "%s\\src", dir);
        ret = SetupFindFirstLineA(hinf, "section1", "two.txt", &ctx);
        CHECK(ret, "find two.txt");
        ret = SetupInstallFileA(hinf, &ctx, "two.txt", srcdir, "two.txt", 0, NULL, NULL);
        CHECK(ret, "SetupInstallFile of a file in a subdirectory: %lu", GetLastError());
        sprintf(dst, "%s\\dst\\two.txt", dir);
        {
            char got[16] = ""; DWORD n = 0;
            HANDLE f = CreateFileA(dst, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (f != INVALID_HANDLE_VALUE) { ReadFile(f, got, 15, &n, NULL); got[n] = 0; CloseHandle(f); }
            CHECK(!strcmp(got, "right"), "the file of the subdirectory was copied: '%s'", got);
        }
        SetupCloseInfFile(hinf);
    }

    /* the same copy queued twice runs once */
    {
        HSPFILEQ q = SetupOpenFileQueue();
        char sdir[MAX_PATH], ddir[MAX_PATH];
        sprintf(sdir, "%s\\src", dir); sprintf(ddir, "%s\\dst", dir);
        sprintf(src, "%s\\src\\x.txt", dir); write_file(src, "x");
        SetupQueueCopyA(q, sdir, NULL, "x.txt", NULL, NULL, ddir, NULL, 0);
        SetupQueueCopyA(q, sdir, NULL, "x.txt", NULL, NULL, ddir, NULL, 0);
        SetupQueueCopyA(q, sdir, NULL, "x.txt", NULL, NULL, ddir, "y.txt", 0);
        starts = 0;
        ret = SetupCommitFileQueueA(NULL, q, count_cb, NULL);
        CHECK(ret, "commit: %lu", GetLastError());
        CHECK(starts == 2, "copy starts: %d", starts);
        SetupCloseFileQueue(q);
    }

    /* custom directory ids belong to the INF they were set for */
    {
        char p1[MAX_PATH], p2[MAX_PATH], text[256];
        HINF a, b;
        DWORD n;
        sprintf(p1, "%s\\a.inf", dir); sprintf(p2, "%s\\b.inf", dir);
        write_file(p1, "[Version]\nSignature=\"$Chicago$\"\n[S]\nKey=%41000%\\app\nG=%41001%\\g\n");
        write_file(p2, "[Version]\nSignature=\"$Chicago$\"\n[S]\nKey=%41000%\\app\nG=%41001%\\g\n");
        a = SetupOpenInfFileA(p1, NULL, INF_STYLE_WIN4, NULL);
        b = SetupOpenInfFileA(p2, NULL, INF_STYLE_WIN4, NULL);
        CHECK(a != INVALID_HANDLE_VALUE && b != INVALID_HANDLE_VALUE, "open infs");
        CHECK(SetupSetDirectoryIdA(a, 41000, "C:\\OnlyA"), "set for a");
        CHECK(SetupSetDirectoryIdA(NULL, 41001, "C:\\Everyone"), "set for all");
        text[0] = 0;
        SetupGetLineTextA(NULL, a, "S", "Key", text, sizeof(text), &n);
        CHECK(!strcmp(text, "C:\\OnlyA\\app"), "a sees its id: '%s'", text);
        text[0] = 0;
        SetupGetLineTextA(NULL, b, "S", "Key", text, sizeof(text), &n);
        CHECK(!strcmp(text, "%41000%\\app"), "b does not: '%s'", text);
        text[0] = 0;
        SetupGetLineTextA(NULL, b, "S", "G", text, sizeof(text), &n);
        CHECK(!strcmp(text, "C:\\Everyone\\g"), "everybody's id: '%s'", text);
        SetupCloseInfFile(a);
        a = SetupOpenInfFileA(p1, NULL, INF_STYLE_WIN4, NULL);
        text[0] = 0;
        SetupGetLineTextA(NULL, a, "S", "Key", text, sizeof(text), &n);
        CHECK(!strcmp(text, "%41000%\\app"), "a reopened has none: '%s'", text);
        SetupCloseInfFile(a);
        SetupCloseInfFile(b);
        SetupSetDirectoryIdA(NULL, 0, NULL);
    }

    printf("checks %d fails %d\n", checks, fails);
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
