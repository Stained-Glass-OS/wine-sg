/* shell32 batch (3686): unread mail accounts, drag images (IDragSourceHelper / IDropTargetHelper) */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <stdio.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void test_mail(void)
{
    HRESULT (WINAPI *pSet)(LPCWSTR, DWORD, LPCWSTR) = (void *)GetProcAddress(GetModuleHandleA("shell32.dll"), "SHSetUnreadMailCountW");
    HRESULT (WINAPI *pEnum)(HKEY, DWORD, LPWSTR, INT) = (void *)GetProcAddress(GetModuleHandleA("shell32.dll"), "SHEnumerateUnreadMailAccountsW");
    WCHAR name[64];
    DWORD count, size, type;
    FILETIME ft;
    HRESULT hr;
    int found = 0, i;
    WCHAR cmd[64];

    hr = pSet(L"sgtest@example.com", 7, L"sgmail.exe /inbox");
    CHECK(hr == S_OK, "SHSetUnreadMailCountW %#lx", hr);
    size = sizeof(count);
    CHECK(!RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\UnreadMail\\sgtest@example.com", L"MessageCount",
            RRF_RT_REG_DWORD, &type, &count, &size) && count == 7, "MessageCount %lu", count);
    size = sizeof(ft);
    CHECK(!RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\UnreadMail\\sgtest@example.com", L"TimeStamp",
            RRF_RT_REG_BINARY, &type, &ft, &size) && size == 8 && (ft.dwHighDateTime || ft.dwLowDateTime), "TimeStamp");
    size = sizeof(cmd);
    CHECK(!RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\UnreadMail\\sgtest@example.com", L"Application",
            RRF_RT_REG_SZ, NULL, cmd, &size) && !lstrcmpW(cmd, L"sgmail.exe /inbox"), "Application %ls", cmd);
    hr = pSet(L"sgtest@example.com", 3, NULL);
    CHECK(hr == S_OK, "update %#lx", hr);
    size = sizeof(count);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\UnreadMail\\sgtest@example.com", L"MessageCount",
            RRF_RT_REG_DWORD, NULL, &count, &size);
    CHECK(count == 3, "updated count %lu", count);
    CHECK(pSet(NULL, 1, NULL) == E_INVALIDARG, "NULL address");
    CHECK(pSet(L"a\\b", 1, NULL) == E_INVALIDARG, "backslash in the address");

    for (i = 0; i < 20; i++)
    {
        hr = pEnum(HKEY_CURRENT_USER, i, name, 64);
        if (hr != S_OK) break;
        if (!lstrcmpiW(name, L"sgtest@example.com")) found++;
    }
    CHECK(found == 1, "account enumerated: %d", found);
    CHECK(hr == HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS), "end of the list %#lx", hr);
    hr = pEnum(HKEY_CURRENT_USER, 0, name, 4);
    CHECK(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), "short buffer %#lx", hr);
    SHDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\UnreadMail\\sgtest@example.com");
}

static HBITMAP make_bitmap(int w, int h)
{
    BITMAPINFO info = {{ sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB }};
    DWORD *bits;
    HBITMAP bmp = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    int i;
    for (i = 0; i < w * h; i++) bits[i] = 0xff3060c0;
    return bmp;
}

static void test_drag(void)
{
    IDropTargetHelper *target = NULL;
    IDragSourceHelper2 *source = NULL;
    IDataObject *object = NULL;
    ITEMIDLIST *full = NULL, *folder, *item;
    WCHAR path[MAX_PATH];
    SHDRAGIMAGE image;
    POINT pt = { 300, 200 };
    RECT rect;
    HWND win;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DragDropHelper, NULL, CLSCTX_INPROC_SERVER, &IID_IDropTargetHelper, (void **)&target);
    CHECK(hr == S_OK && target, "DragDropHelper %#lx", hr);
    if (!target) return;
    hr = IDropTargetHelper_QueryInterface(target, &IID_IDragSourceHelper2, (void **)&source);
    CHECK(hr == S_OK && source, "IDragSourceHelper2 %#lx", hr);

    GetWindowsDirectoryW(path, MAX_PATH);
    SHParseDisplayName(path, NULL, &full, 0, NULL);
    folder = ILClone(full); ILRemoveLastID(folder); item = ILFindLastID(full);
    hr = SHCreateDataObject(folder, 1, (PCUITEMID_CHILD_ARRAY)&item, NULL, &IID_IDataObject, (void **)&object);
    CHECK(hr == S_OK && object, "data object %#lx", hr);
    if (!object || !source) return;

    /* no image yet: nothing is shown */
    hr = IDropTargetHelper_DragEnter(target, NULL, object, &pt, DROPEFFECT_COPY);
    CHECK(hr == S_OK, "DragEnter without an image %#lx", hr);
    CHECK(!FindWindowW(L"SGDragImage", NULL), "no image window yet");
    IDropTargetHelper_DragLeave(target);

    image.sizeDragImage.cx = 40; image.sizeDragImage.cy = 30;
    image.ptOffset.x = 5; image.ptOffset.y = 7;
    image.hbmpDragImage = make_bitmap(40, 30);
    image.crColorKey = 0;
    hr = IDragSourceHelper2_InitializeFromBitmap(source, &image, object);
    CHECK(hr == S_OK, "InitializeFromBitmap %#lx", hr);
    hr = IDragSourceHelper2_InitializeFromBitmap(source, NULL, object);
    CHECK(hr == E_INVALIDARG, "NULL image %#lx", hr);
    hr = IDragSourceHelper2_SetFlags(source, 1);
    CHECK(hr == S_OK, "SetFlags %#lx", hr);

    hr = IDropTargetHelper_DragEnter(target, NULL, object, &pt, DROPEFFECT_COPY);
    CHECK(hr == S_OK, "DragEnter %#lx", hr);
    win = FindWindowW(L"SGDragImage", NULL);
    CHECK(win != NULL, "image window");
    if (win)
    {
        GetWindowRect(win, &rect);
        CHECK(rect.left == 295 && rect.top == 193 && rect.right - rect.left == 40 && rect.bottom - rect.top == 30,
              "window at %ld,%ld size %ldx%ld", rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
        CHECK(IsWindowVisible(win), "visible");
        pt.x = 410; pt.y = 120;
        hr = IDropTargetHelper_DragOver(target, &pt, DROPEFFECT_COPY);
        CHECK(hr == S_OK, "DragOver %#lx", hr);
        GetWindowRect(win, &rect);
        CHECK(rect.left == 405 && rect.top == 113, "moved to %ld,%ld", rect.left, rect.top);
        hr = IDropTargetHelper_Show(target, FALSE);
        CHECK(hr == S_OK && !IsWindowVisible(win), "hidden");
        IDropTargetHelper_Show(target, TRUE);
        CHECK(IsWindowVisible(win), "shown again");
    }
    hr = IDropTargetHelper_DragLeave(target);
    CHECK(hr == S_OK && !FindWindowW(L"SGDragImage", NULL), "DragLeave removes the window");
    hr = IDropTargetHelper_DragEnter(target, NULL, object, &pt, DROPEFFECT_COPY);
    CHECK(FindWindowW(L"SGDragImage", NULL) != NULL, "second drag");
    hr = IDropTargetHelper_Drop(target, object, &pt, DROPEFFECT_COPY);
    CHECK(hr == S_OK && !FindWindowW(L"SGDragImage", NULL), "Drop removes the window");
    hr = IDropTargetHelper_DragEnter(target, NULL, object, NULL, DROPEFFECT_COPY);
    CHECK(hr == E_INVALIDARG, "DragEnter without a point %#lx", hr);

    IDataObject_Release(object);
    IDragSourceHelper2_Release(source);
    IDropTargetHelper_Release(target);
    ILFree(folder); ILFree(full);
}

int main(void)
{
    CoInitialize(NULL);
    test_mail();
    test_drag();
    printf(failed ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failed);
    return failed != 0;
}
