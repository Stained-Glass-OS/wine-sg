/* Probe for patches/sg/5217: ActivateKeyboardLayout with an aliased layout (0xf000 | id in the
 * high word: other layouts of a language) works like a plain one: the layout becomes the
 * thread's, WM_INPUTLANGCHANGE carries the layout and the input language's character set, and
 * no "not yet implemented" fixme is logged.  Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static int changes;
static WPARAM last_wp;
static LPARAM last_lp;

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INPUTLANGCHANGE) { changes++; last_wp = wp; last_lp = lp; }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int main(void)
{
    WNDCLASSA wc = {0};
    HWND hwnd;
    LANGID lang = LANGIDFROMLCID(GetUserDefaultLCID());
    CHARSETINFO ci = {0};
    DWORD acp = GetACP();
    HKL base = (HKL)(ULONG_PTR)MAKELONG(lang, lang), plain_other, alias, ret, cur;
    static const WORD ids[] = { 0xf001, 0xf002, 0xf123, 0xe001 };
    unsigned int i;

    setvbuf(stdout, NULL, _IONBF, 0);
    TranslateCharsetInfo((DWORD *)(ULONG_PTR)acp, &ci, TCI_SRCCODEPAGE);
    wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "SgHkl";
    RegisterClassA(&wc);
    hwnd = CreateWindowA("SgHkl", "t", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);
    SetFocus(hwnd);
    plain_other = (HKL)(ULONG_PTR)MAKELONG(lang, 0x0001);
    (void)plain_other;

    ret = ActivateKeyboardLayout(base, 0);
    check(ret != NULL, "the language's own layout activates (%p)", ret);
    changes = 0;

    for (i = 0; i < sizeof(ids) / sizeof(*ids); i++)
    {
        alias = (HKL)(ULONG_PTR)MAKELONG(lang, ids[i]);
        changes = 0; last_wp = 0xdead; last_lp = 0;
        ret = ActivateKeyboardLayout(alias, 0);
        cur = GetKeyboardLayout(0);
        check(ret != NULL, "alias %#x: activated (%p)", ids[i], ret);
        check(cur == alias, "alias %#x: it is the thread's layout (%p)", ids[i], cur);
        check(changes == 1 && last_lp == (LPARAM)alias, "alias %#x: WM_INPUTLANGCHANGE x%d lp %Ix", ids[i], changes, last_lp);
        check(last_wp == ci.ciCharset, "alias %#x: charset %Iu (want %lu)", ids[i], last_wp, (DWORD)ci.ciCharset);
        ActivateKeyboardLayout(base, 0);
    }

    DestroyWindow(hwnd);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
