/* SHGetImageList(-1): a shell image list a program may resize to its own icon
 * size (IImageList2::Resize); icons the shell adds afterwards come at that size,
 * and the documented lists are not touched. TreeSize (VCL) takes it at every
 * start and crashed on a NULL list. patches/sg/9301. Prints RESULT: PASS / FAIL. */
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <commoncontrols.h>
#include <shellapi.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const GUID iid_imagelist2 = { 0x192b9d83, 0x50fc, 0x457b, { 0x90, 0xa0, 0x2b, 0x82, 0xa8, 0xb5, 0xda, 0xe1 } };
static const GUID iid_imagelist = { 0x46eb5926, 0x582e, 0x4017, { 0x9f, 0xdf, 0xe8, 0x99, 0x8d, 0xaa, 0x09, 0x50 } };

typedef HRESULT (WINAPI *get_list_fn)(int, REFIID, void **);

int main(void)
{
    HMODULE shell32 = LoadLibraryA("shell32.dll");
    get_list_fn get_list = (get_list_fn)GetProcAddress(shell32, MAKEINTRESOURCEA(727));
    IImageList2 *any = NULL, *any2 = NULL;
    IImageList *small = NULL;
    SHFILEINFOW info;
    WCHAR sysdir[MAX_PATH], file[MAX_PATH];
    int cx = 0, cy = 0, count = 0, before = 0, small_cx = 0, small_cy = 0;
    HICON icon = NULL;
    ICONINFO ii;
    BITMAP bm;
    HRESULT hr;

    CoInitialize(NULL);
    CHECK(get_list != NULL, "SHGetImageList (ordinal 727) found");
    if (!get_list) { printf("RESULT: FAIL\n"); return 1; }

    hr = get_list(-1, &iid_imagelist2, (void **)&any);
    CHECK(hr == S_OK && any, "SHGetImageList(-1, IImageList2) gives a list (hr %#lx)", hr);
    if (!any) { printf("RESULT: FAIL\n"); return 1; }
    hr = IImageList2_GetIconSize(any, &cx, &cy);
    CHECK(hr == S_OK && cx > 0 && cy > 0, "its icon size %dx%d", cx, cy);
    IImageList2_GetImageCount(any, &before);
    CHECK(before > 0, "it holds the shell's icons (%d)", before);

    hr = get_list(-1, &iid_imagelist2, (void **)&any2);
    CHECK(hr == S_OK && any2 == any, "the same list again (%p %p)", any, any2);
    if (any2) IImageList2_Release(any2);

    hr = get_list(SHIL_SMALL, &iid_imagelist, (void **)&small);
    CHECK(hr == S_OK && small, "SHIL_SMALL list (hr %#lx)", hr);
    if (small) IImageList_GetIconSize(small, &small_cx, &small_cy);
    CHECK(small && (void *)small != (void *)any, "SHIL_SMALL is another list");

    /* TreeSize: resize it to a size of its own */
    hr = IImageList2_Resize(any, 20, 20);
    CHECK(hr == S_OK, "Resize(20, 20) (hr %#lx)", hr);
    IImageList2_GetIconSize(any, &cx, &cy);
    CHECK(cx == 20 && cy == 20, "now 20x20 (%dx%d)", cx, cy);
    if (small)
    {
        int sx = 0, sy = 0;
        IImageList_GetIconSize(small, &sx, &sy);
        CHECK(sx == small_cx && sy == small_cy, "SHIL_SMALL keeps its size (%dx%d)", sx, sy);
    }

    /* an icon the shell adds later is in it, at 20x20 */
    GetSystemDirectoryW(sysdir, MAX_PATH);
    swprintf(file, MAX_PATH, L"%ls\\notepad.exe", sysdir);
    memset(&info, 0, sizeof(info));
    SHGetFileInfoW(file, 0, &info, sizeof(info), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
    IImageList2_GetImageCount(any, &count);
    CHECK(info.iIcon >= 0 && info.iIcon < count, "the file's icon index %d is in it (%d images)", info.iIcon, count);
    if (small)
    {
        int small_count = 0;
        IImageList_GetImageCount(small, &small_count);
        CHECK(small_count == count, "in step with SHIL_SMALL (%d, %d)", small_count, count);
    }
    hr = IImageList2_GetIcon(any, info.iIcon, ILD_TRANSPARENT, &icon);
    memset(&bm, 0, sizeof(bm));
    if (icon && GetIconInfo(icon, &ii)) GetObjectW(ii.hbmColor ? ii.hbmColor : ii.hbmMask, sizeof(bm), &bm);
    CHECK(hr == S_OK && bm.bmWidth == 20, "its icon comes out 20 wide (%ld)", bm.bmWidth);

    hr = get_list(SHIL_LAST + 1, &iid_imagelist2, (void **)&any2);
    CHECK(hr != S_OK, "an index past SHIL_LAST is still refused (hr %#lx)", hr);

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
