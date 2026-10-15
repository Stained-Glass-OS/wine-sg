/* shell32 DAD_SetDragImage / DAD_ShowDragImage (patch 3697) */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef BOOL (WINAPI *set_fn)(HIMAGELIST, POINT *);
typedef BOOL (WINAPI *show_fn)(BOOL);

int main(void)
{
    HMODULE shell32 = LoadLibraryA("shell32.dll");
    set_fn set = (set_fn)GetProcAddress(shell32, "DAD_SetDragImage");
    show_fn show = (show_fn)GetProcAddress(shell32, "DAD_ShowDragImage");
    HIMAGELIST him;
    POINT pt = { 4, 6 }, hot = { -1, -1 };
    HICON icon = LoadIconA(NULL, IDI_APPLICATION);
    HWND wnd;
    HIMAGELIST got;

    InitCommonControls();
    CHECK(set && show, "exports %p %p", set, show);
    if (!set || !show) return 1;
    wnd = CreateWindowExW(0, L"Static", L"dad", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, NULL, NULL, NULL, NULL);
    him = ImageList_Create(32, 32, ILC_COLOR32 | ILC_MASK, 1, 1);
    ImageList_AddIcon(him, icon);

    CHECK(!set(NULL, &pt), "no list");
    CHECK(!set(him, NULL), "no hot spot");
    CHECK(set(him, &pt), "set the drag image");
    got = ImageList_GetDragImage(NULL, &hot);
    CHECK(got != NULL, "the drag image is set");
    CHECK(hot.x == 4 && hot.y == 6, "hot spot %ld,%ld", hot.x, hot.y);
    ImageList_DragEnter(wnd, 10, 10);
    CHECK(show(FALSE), "hide the image");
    CHECK(show(TRUE), "show the image");
    ImageList_DragLeave(wnd);
    CHECK(set((HIMAGELIST)-1, NULL), "clear");
    got = ImageList_GetDragImage(NULL, NULL);
    CHECK(got == NULL, "the drag image is gone: %p", got);

    printf(failed ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failed);
    return failed != 0;
}
