#include <windows.h>
#include <setupapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* {4d36e967-e325-11ce-bfc1-08002be10318} "Disk drives" is registered by the prefix's wine.inf */
static const GUID disk = {0x4d36e967, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};
static const GUID bogus = {0x12345678, 0x1111, 0x2222, {1, 2, 3, 4, 5, 6, 7, 8}};
static const GUID nullguid;

static void test_images(void)
{
    SP_CLASSIMAGELIST_DATA d;
    int idx, idx2, idx3, count;
    HICON icon = NULL;
    BOOL ret;

    memset(&d, 0, sizeof(d));
    SetLastError(0xdeadbeef);
    ret = SetupDiGetClassImageList(&d);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "zero cbSize: %d %lu", ret, GetLastError());

    d.cbSize = sizeof(d);
    ret = SetupDiGetClassImageList(&d);
    CHECK(ret && d.ImageList, "get list %d err %lu", ret, GetLastError());
    if (!ret) return;
    count = ImageList_GetImageCount(d.ImageList);
    CHECK(count >= 2, "image count %d", count);

    ret = SetupDiGetClassImageIndex(&d, &disk, &idx);
    CHECK(ret && idx >= 0 && idx < count, "disk index %d ret %d", idx, ret);
    ret = SetupDiGetClassImageIndex(&d, &nullguid, &idx2);
    CHECK(ret && idx2 >= 0 && idx2 < count, "unknown index %d", idx2);
    CHECK(idx != idx2, "disk and unknown share index %d", idx);
    SetLastError(0xdeadbeef);
    ret = SetupDiGetClassImageIndex(&d, &bogus, &idx3);
    CHECK(!ret && GetLastError() == ERROR_INVALID_CLASS, "bogus class: %d %#lx", ret, GetLastError());
    ret = SetupDiGetClassImageIndex(&d, &disk, NULL);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "null index");

    ret = SetupDiLoadClassIcon(&disk, &icon, &idx3);
    CHECK(ret && icon, "load class icon %d %#lx", ret, GetLastError());
    if (icon) DestroyIcon(icon);
    SetLastError(0xdeadbeef);
    ret = SetupDiLoadClassIcon(&bogus, &icon, NULL);
    CHECK(!ret && GetLastError() == ERROR_INVALID_CLASS, "load bogus: %d %#lx", ret, GetLastError());

    ret = SetupDiGetClassBitmapIndex(&disk, &idx3);
    CHECK(ret && idx3 >= 0, "bitmap index");
    ret = SetupDiGetClassBitmapIndex(&bogus, &idx2);
    CHECK(ret, "bitmap index of an unknown class");
    CHECK(idx3 != idx2, "bogus class shares the disk bitmap index");

    {
        HDC dc = GetDC(NULL);
        RECT rc = {0, 0, 16, 16};
        INT sz = SetupDiDrawMiniIcon(dc, rc, idx3, 0);
        CHECK(LOWORD(sz) == GetSystemMetrics(SM_CXSMICON) && HIWORD(sz) == GetSystemMetrics(SM_CYSMICON), "draw size %#x", sz);
        ReleaseDC(NULL, dc);
    }

    ret = SetupDiDestroyClassImageList(&d);
    CHECK(ret && !d.ImageList, "destroy %d", ret);
    ret = SetupDiDestroyClassImageList(&d);
    CHECK(!ret, "double destroy");
}

static void test_reboot(void)
{
    char path[MAX_PATH], src[MAX_PATH], dst[MAX_PATH], dir[MAX_PATH];
    HSPFILEQ q;
    HANDLE h;
    INT r;
    BOOL ok;

    SetLastError(0);
    r = SetupPromptReboot(NULL, NULL, TRUE);
    CHECK(r == -1 && GetLastError() == ERROR_INVALID_PARAMETER, "scan without queue: %d %lu", r, GetLastError());

    q = SetupOpenFileQueue();
    r = SetupPromptReboot(q, NULL, TRUE);
    CHECK(r == 0, "fresh queue wants reboot %d", r);
    /* not scan-only but nothing to restart for: no prompt, returns 0 */
    r = SetupPromptReboot(q, NULL, FALSE);
    CHECK(r == 0, "fresh queue prompted %d", r);

    GetTempPathA(sizeof(dir), dir);
    sprintf(src, "%sfastsetup_src.txt", dir);
    sprintf(dst, "%sfastsetup_dst.txt", dir);
    h = CreateFileA(src, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(h);
    h = CreateFileA(dst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); /* held open: in use */
    sprintf(path, "%s", dir);
    SetupQueueCopyA(q, dir, "", "fastsetup_src.txt", NULL, NULL, dir, "fastsetup_dst.txt", SP_COPY_IN_USE_NEEDS_REBOOT);
    ok = SetupCommitFileQueueA(NULL, q, SetupDefaultQueueCallbackA, SetupInitDefaultQueueCallback(NULL));
    CHECK(ok, "commit failed %lu", GetLastError());
    r = SetupPromptReboot(q, NULL, TRUE);
    CHECK(r == (SPFILEQ_FILE_IN_USE | SPFILEQ_REBOOT_RECOMMENDED), "in-use queue flags %d", r);
    CloseHandle(h);
    SetupCloseFileQueue(q);
    DeleteFileA(src);
    DeleteFileA(dst);
}

int main(void)
{
    test_images();
    test_reboot();
    printf("checks %d fails %d\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
