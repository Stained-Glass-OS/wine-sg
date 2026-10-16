#include <windows.h>
#include <imm.h>
#include <stdio.h>
#include <string.h>
static int failed;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); failed++; } } while (0)
int main(void)
{
    HWND hwnd = CreateWindowA("static", "imm", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    HIMC himc = ImmGetContext(hwnd);
    char buf[16];
    WCHAR wbuf[16];
    DWORD ret;
    HIMCC imcc;
    BOOL u;
    HIMC (WINAPI *create)(void);
    typedef HIMCC (WINAPI *create_imcc_t)(DWORD);
    typedef HIMCC (WINAPI *destroy_imcc_t)(HIMCC);
    typedef BOOL (WINAPI *unlock_imcc_t)(HIMCC);
    HMODULE imm32 = GetModuleHandleA("imm32.dll");
    create_imcc_t create_imcc = (void *)GetProcAddress(imm32, "ImmCreateIMCC");
    destroy_imcc_t destroy_imcc = (void *)GetProcAddress(imm32, "ImmDestroyIMCC");
    unlock_imcc_t unlock_imcc = (void *)GetProcAddress(imm32, "ImmUnlockIMCC");
    (void)create;

    CHECK(hwnd && himc, "window %p context %p", hwnd, himc);
    /* a default context has no guide line: level GL_LEVEL_NOGUIDELINE, empty string */
    ret = ImmGetGuideLineA(himc, GGL_LEVEL, NULL, 0);
    CHECK(ret == GL_LEVEL_NOGUIDELINE, "level %lu", ret);
    ret = ImmGetGuideLineA(himc, GGL_INDEX, NULL, 0);
    CHECK(ret == 0, "index %lu", ret);
    ret = ImmGetGuideLineA(himc, GGL_STRING, buf, sizeof(buf));
    CHECK(ret == 0, "string %lu", ret);
    ret = ImmGetGuideLineW(himc, GGL_STRING, wbuf, sizeof(wbuf));
    CHECK(ret == 0, "wide string %lu", ret);
    ret = ImmGetGuideLineW(himc, GGL_PRIVATE, NULL, 0);
    CHECK(ret == 0, "private %lu", ret);
    ret = ImmGetGuideLineW(himc, 77, NULL, 0);
    CHECK(ret == 0, "unknown index %lu", ret);
    ret = ImmGetGuideLineW((HIMC)0x1234, GGL_LEVEL, NULL, 0);
    CHECK(ret == 0, "bad context %lu", ret);

    /* a destroyed IMCC cannot be unlocked */
    imcc = create_imcc(64);
    CHECK(imcc != NULL, "create");
    destroy_imcc(imcc);
    u = unlock_imcc(imcc);
    CHECK(u == FALSE, "unlock destroyed returned %d", u);
    ImmReleaseContext(hwnd, himc);
    DestroyWindow(hwnd);
    printf(failed ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failed;
}
